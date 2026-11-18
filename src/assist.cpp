#include "assist.hpp"
#include <cmath>
#include <cstdlib>

namespace nigh {
static int clamp(int n,int lo,int hi) { return std::min(hi,std::max(lo,n)); }
static bool loadFaces(const std::wstring& folder,FaceCascade& model) {
    File f(join(folder,L"models\\facefinder.bin"));
    const DWORD bytes=239632;
    if(!f.good() || f.size()!=bytes) return false;
    std::vector<uint8_t> data(bytes);
    return f.get(data.data(),bytes) && model.load(data.data(),data.size());
}
static void grayImage(const uint8_t* rgb,std::vector<uint8_t>& gray) {
    gray.resize(ProxyW*ProxyH);
    for(size_t i=0;i<gray.size();++i,rgb+=4) gray[i]=uint8_t((77*rgb[2]+150*rgb[1]+29*rgb[0])>>8);
}
struct Vision {
    AssistReport& out;
    FaceCascade faces;
    std::vector<uint8_t> gray;
    std::vector<std::pair<FaceHit,int> > candidates;
    ShotSignature older,last;
    int olderFrame,lastFrame,lastScore,lum,count;
    explicit Vision(AssistReport& o) : out(o),olderFrame(-1),lastFrame(-1),lastScore(0),lum(0),count(0) {}
    void sample(const std::vector<uint8_t>& frame,int pos,bool findFace,bool tone) {
        ShotSignature now=shotSignature(frame.data());
        if(olderFrame>=0 && lastFrame>out.first+1) {
            int score=cutScore(older,last,now);
            if(score>0) {
                if(out.cuts.empty() || lastFrame-out.cuts.back()>=Fps/2) { out.cuts.push_back(lastFrame); lastScore=score; }
                else if(score>lastScore) { out.cuts.back()=lastFrame; lastScore=score; }
            }
        }
        if(lastFrame>=0) { older=last; olderFrame=lastFrame; }
        last=now; lastFrame=pos;
        if(tone) for(short l:now.light) if(l>9 && l<251) { lum+=l; ++count; }
        if(findFace && faces.ready()) {
            grayImage(frame.data(),gray);
            FaceHit f=faces.find(gray.data(),ProxyW,ProxyH);
            if(f.size) candidates.push_back(std::make_pair(f,pos));
            ++out.faceChecks;
        }
        ++out.videoFrames;
    }
    void finish(bool still) {
        if(count) {
            int shift=clamp((112-lum/count)/4,-18,18);
            if(std::abs(shift)>=3) out.brightness=shift;
        }
        // Select the largest *consistent* cluster, not an identity. A moving
        // subject or mostly unrecognized angles should not pick a random crop.
        std::vector<int> xs,ys,ds,index;
        for(const auto& entry:candidates) {
            const FaceHit& f=entry.first;
            std::vector<int> x,y,d,ids;
            for(size_t i=0;i<candidates.size();++i) {
                const FaceHit& g=candidates[i].first;
                if(std::abs(f.x-g.x)<=ProxyW/9 && std::abs(f.y-g.y)<=ProxyH/5) {
                    x.push_back(g.x); y.push_back(g.y); d.push_back(g.size); ids.push_back(int(i));
                }
            }
            if(x.size()>xs.size()) { xs.swap(x); ys.swap(y); ds.swap(d); index.swap(ids); }
        }
        int n=int(xs.size());
        if(n>=1 && (still || out.faceChecks==1 || (n>=2 && n*2>out.faceChecks))) {
            std::sort(xs.begin(),xs.end()); std::sort(ys.begin(),ys.end()); std::sort(ds.begin(),ds.end());
            out.face=FaceHit(xs[n/2],ys[n/2],ds[n/2]); out.faceVotes=n;
            // Pick a representative real sample for inspection, never imply live tracking.
            int best=ProxyW+ProxyH;
            for(int i:index) {
                const auto& sample=candidates[i];
                int d=std::abs(sample.first.x-out.face.x)+std::abs(sample.first.y-out.face.y);
                if(d<best) { best=d; out.faceFrame=std::max(out.first,sample.second); }
            }
        }
    }
};
static bool scanVideo(const MediaContext& ctx,const Asset& a,AssistReport& out,std::wstring& error) {
    const Proxy& p=*a.proxy;
    if(a.video<0) return true;
    Vision v(out);
    out.faceModel=loadFaces(ctx.tools,v.faces);
    if(a.still && p.still.size()==size_t(ProxyW*ProxyH*4)) {
        v.sample(p.still,out.first,true,true);
    } else if(!a.still && !p.frames.empty()) {
        FrameReader reader;
        std::vector<uint8_t> frame;
        int step=Fps/ProxyFps, i0=out.first/step, i1=std::min(int(p.frames.size()),(out.end+step-1)/step);
        int gap=std::max(ProxyFps,(i1-i0+21)/22);
        for(int i=i0;i<i1;++i) {
            if(ctx.stopped()) return false;
            if((i-i0)%12==0) ctx.report(2+65*(i-i0)/std::max(1,i1-i0),L"Analisando cortes e rostos • "+timecode(i*step));
            if(!reader.frame(p,i,frame)) { error=L"Não foi possível ler os quadros da prévia. Reimporte a mídia."; return false; }
            v.sample(frame,i*step,(i-i0)%gap==0,(i-i0)%3==0);
        }
    }
    v.finish(a.still);
    return !ctx.stopped();
}
static bool scanAudio(const MediaContext& ctx,const Proxy& p,AssistReport& out,std::wstring& error) {
    if(p.pcm.empty()) return true;
    const int Count=PreviewRate*30/1000;
    const int per=PreviewRate/Fps;
    const uint64_t first=uint64_t(out.first)*per*4;
    File pcm(p.pcm);
    uint64_t bytes=pcm.size();
    if(bytes<=first || !pcm.seek(first)) { error=L"Não foi possível ler o áudio proxy neste trecho."; return false; }
    VoiceModel vad;
    if(!vad.ready()) { error=L"Não foi possível iniciar o detector de fala local."; return false; }
    int16_t buf[Count*2], sample[240];
    int total=int(std::min<uint64_t>((out.end-out.first)*per,(bytes-first)/4)), measured=0, peak=0;
    uint64_t power=0;
    out.hasVoice=true;
    int start=-1, last=-1;
    const auto finish=[&]() {
        if(start>=0 && last-start>=PreviewRate/3) {
            int a=std::max(out.first,out.first+(start-PreviewRate/14)/per);
            int b=std::min(out.end,out.first+(last+PreviewRate/14+per-1)/per);
            if(a<b) out.voice.push_back(AssistSpan(a,b));
        }
        start=last=-1;
    };
    for(int offset=0;offset<total;offset+=Count) {
        if(ctx.stopped()) return false;
        if(offset%(Count*30)==0) ctx.report(69+29*offset/total,L"Separando trechos com fala • "+timecode(out.first+offset/per));
        DWORD want=DWORD(std::min(Count,total-offset)*4);
        if(!pcm.get(buf,want)) { error=L"Erro ao ler o áudio proxy."; return false; }
        int n=int(want/4);
        for(int i=0;i<n*2;++i) { int val=buf[i]; peak=std::max(peak,std::abs(val)); power+=uint64_t(val*val); }
        measured+=n*2;
        for(int i=0;i<240;++i) {
            int value=0;
            for(int j=i*6;j<(i+1)*6 && j<n*2;++j) value+=buf[j];
            sample[i]=int16_t(value/6);
        }
        int result=vad.classify(sample);
        if(result<0) { error=L"Falha no detector de atividade de voz."; return false; }
        if(result==1) {
            if(start<0) start=offset;
            last=offset+n;
        } else if(start>=0 && offset-last>PreviewRate/4) finish();
    }
    finish();
    double rms=measured?std::sqrt(double(power)/measured):0;
    if(peak>0 && rms>=160) out.gain=clamp(int(std::min(4096.0*100/rms,29490.0*100/peak)+.5),50,150);
    return !ctx.stopped();
}
bool analyze(const MediaContext& ctx,const Asset& a,int first,int end,AssistReport& out,std::wstring& error) {
    if(ctx.stopped()) return false;
    AssistReport report;
    report.first=clamp(first,0,MaxFrames);
    int stop=std::min(a.still?MaxFrames:a.frames,end);
    report.end=std::min(stop,report.first+AssistFrameLimit);
    report.limited=report.end<stop;
    if(!a.proxy || report.end<=report.first) { error=L"Selecione uma mídia com prévia e um trecho válido."; return false; }
    if(!scanVideo(ctx,a,report,error) || !scanAudio(ctx,*a.proxy,report,error)) return false;
    if(!report.videoFrames && !report.hasVoice) { error=L"Este trecho não tem imagem nem áudio para analisar."; return false; }
    for(int f:report.cuts) report.cues.push_back(AssistCue(f,-1));
    for(size_t i=0;i<report.voice.size();++i) report.cues.push_back(AssistCue(report.voice[i].first,int(i)));
    std::sort(report.cues.begin(),report.cues.end(),[](const AssistCue& a,const AssistCue& b) {
        return a.frame==b.frame?a.voiceIndex<b.voiceIndex:a.frame<b.frame;
    });
    ctx.report(100,L"Sugestões prontas para revisão");
    if(ctx.stopped()) return false;
    out=std::move(report);
    return true;
}
bool sameTarget(const Project& p,const AssistTarget& a) {
    const Clip* c=get(p.seq,a.clip);
    return c && a.sequenceStart>=0 && c->asset==a.asset && c->in==a.first && c->len==a.clipLength &&
        startOf(p.seq,a.clip)==a.sequenceStart;
}
std::vector<int> splitCandidates(const Project& p,const AssistTarget& a,const AssistReport& r) {
    std::vector<int> marks;
    const Clip* c=get(p.seq,a.clip);
    if(!sameTarget(p,a) || (a.clip.lane!=Video && a.clip.lane!=Overlay)) return marks;
    int head=std::max(1,std::max(c->fx.fadeIn,dissolveLength(*c)));
    int tail=c->len-std::max(1,c->fx.fadeOut);
    for(int frame:r.cuts) if(frame-c->in>=head && frame-c->in<=tail) marks.push_back(a.sequenceStart+frame-c->in);
    return marks;
}
void frameFace(Effects& fx,FrameFormat format,const FaceHit& f) {
    if(!f.size) return;
    int w=previewWidth(format);
    int scale=format==FrameWide?125:100;
    // Preserve headroom in a horizontal shot. Avoid pushing a source beyond
    // its reference aperture on the side where no pixels are available.
    int want=(ProxyW/2-f.x)*scale*2/ProxyW;
    int bound=std::max(0,scale-(100*w+ProxyW-1)/ProxyW);
    fx.scale=scale; fx.x=clamp(want,-bound,bound); fx.y=0;
}
}
