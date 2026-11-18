#include "comp.hpp"
#include <cmath>
#include <cstdlib>
#include <cstring>

#define STBI_ONLY_JPEG
#define STBI_ONLY_PNG
#define STBI_ONLY_BMP
#define STBI_NO_STDIO
#define STBI_NO_THREAD_LOCALS
#define STBI_NO_SIMD
#define STBI_NO_LINEAR
#define STBI_NO_FAILURE_STRINGS
#define STBI_MAX_DIMENSIONS 4096
#define STB_IMAGE_IMPLEMENTATION
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-value"
#pragma GCC diagnostic ignored "-Wunused-function"
#include "vendor/stb_image.h"
#pragma GCC diagnostic pop

namespace nigh {
using Args=std::vector<std::wstring>;
static std::wstring quote(const std::wstring& arg) {
    std::wstring out=L"\"";
    size_t slashes=0;
    for(wchar_t c:arg) {
        if(c==L'\\') { ++slashes; continue; }
        if(c==L'"') out.append(slashes*2+1,L'\\');
        else out.append(slashes,L'\\');
        slashes=0;
        out+=c;
    }
    out.append(slashes*2,L'\\');
    out+=L'"';
    return out;
}
bool codecsPresent(const std::wstring& folder) {
    return exists(join(folder,L"ffmpeg.exe")) && exists(join(folder,L"ffprobe.exe"));
}
static bool run(const MediaContext& ctx,const wchar_t* tool,const Args& args,const std::wstring& log,std::wstring& error,const std::wstring& cwd=L"") {
    if(ctx.stopped()) return false;
    std::wstring exe=join(ctx.tools,tool), line=quote(exe);
    for(const std::wstring& a:args) line+=L" "+quote(a);
    SECURITY_ATTRIBUTES sa={sizeof(sa),0,TRUE};
    STARTUPINFOW si;
    std::memset(&si,0,sizeof(si));
    si.cb=sizeof(si);
    si.dwFlags=STARTF_USESTDHANDLES;
    si.hStdInput=CreateFileW(L"NUL",GENERIC_READ,FILE_SHARE_READ|FILE_SHARE_WRITE,&sa,OPEN_EXISTING,0,0);
    si.hStdOutput=CreateFileW(log.c_str(),GENERIC_WRITE,FILE_SHARE_READ,&sa,CREATE_ALWAYS,FILE_ATTRIBUTE_NORMAL,0);
    si.hStdError=si.hStdOutput;
    PROCESS_INFORMATION pi;
    std::memset(&pi,0,sizeof(pi));
    BOOL started=si.hStdInput!=INVALID_HANDLE_VALUE && si.hStdOutput!=INVALID_HANDLE_VALUE &&
        CreateProcessW(exe.c_str(),&line[0],0,0,TRUE,CREATE_NO_WINDOW,0,cwd.empty()?0:cwd.c_str(),&si,&pi);
    DWORD winError=GetLastError();
    if(si.hStdInput!=INVALID_HANDLE_VALUE) CloseHandle(si.hStdInput);
    if(si.hStdOutput!=INVALID_HANDLE_VALUE) CloseHandle(si.hStdOutput);
    if(!started) {
        error=L"Não foi possível iniciar "+std::wstring(tool)+L" (erro "+number(int(winError))+L"). Mantenha os executáveis na mesma pasta.";
        return false;
    }
    CloseHandle(pi.hThread);
    DWORD wait=WAIT_TIMEOUT;
    while(!ctx.stopped() && wait==WAIT_TIMEOUT) wait=WaitForSingleObject(pi.hProcess,60);
    if(ctx.stopped()) {
        if(wait!=WAIT_OBJECT_0) TerminateProcess(pi.hProcess,1);
        WaitForSingleObject(pi.hProcess,INFINITE);
        CloseHandle(pi.hProcess);
        return false;
    }
    DWORD code=1;
    GetExitCodeProcess(pi.hProcess,&code);
    CloseHandle(pi.hProcess);
    if(wait==WAIT_OBJECT_0 && code==0) return true;
    error=wide(readText(log,1250,true));
    if(error.empty()) error=L"O motor de mídia não conseguiu concluir a operação (erro "+number(int(code))+L").";
    return false;
}
static Args ff() {
    return {L"-hide_banner",L"-v",L"error",L"-nostdin",L"-y",L"-max_alloc",L"67108864",L"-filter_threads",L"1",L"-filter_complex_threads",L"1"};
}
static void append(Args& args,std::initializer_list<std::wstring> more) { args.insert(args.end(),more.begin(),more.end()); }
static std::wstring stream(int index) { return L"0:"+number(index); }
static std::string probeField(const std::string& line,const std::string& key) {
    size_t i=line.find("|"+key+"=");
    if(i==std::string::npos) return "";
    i+=key.size()+2;
    size_t end=line.find('|',i);
    return line.substr(i,end==std::string::npos?end:end-i);
}
static double durationField(const std::string& s) {
    double n=std::strtod(s.c_str(),0);
    return std::isfinite(n) && n>0?n:0;
}
static bool inspect(const MediaContext& ctx,Asset& a,const std::wstring& log,std::wstring& error) {
    if(!run(ctx,L"ffprobe.exe",{L"-v",L"error",L"-max_alloc",L"67108864",L"-show_entries",
        L"stream=index,codec_type,duration:stream_disposition=attached_pic:format=duration",
        L"-of",L"compact=p=1:nk=0",L"-i",a.path},log,error)) return false;
    std::string text=readText(log,65536);
    double videoDuration=0,audioDuration=0,fileDuration=0;
    for(size_t i=0;i<text.size();) {
        size_t end=text.find('\n',i);
        if(end==std::string::npos) end=text.size();
        std::string line=text.substr(i,end-i);
        i=end+1;
        double d=durationField(probeField(line,"duration"));
        if(line.compare(0,7,"format|")==0) { fileDuration=d; continue; }
        if(line.compare(0,7,"stream|")!=0) continue;
        std::string kind=probeField(line,"codec_type"), index=probeField(line,"index");
        if(index.empty()) continue;
        if(kind=="video" && a.video<0 && line.find("attached_pic=1")==std::string::npos) {
            a.video=std::atoi(index.c_str()); videoDuration=d;
        } else if(kind=="audio" && a.audio<0) { a.audio=std::atoi(index.c_str()); audioDuration=d; }
    }
    double duration=a.video>=0?videoDuration:audioDuration;
    if(duration<=0) duration=fileDuration;
    if((a.video<0 && a.audio<0) || duration<=0) {
        error=L"O arquivo não contém vídeo/áudio utilizável com duração conhecida.";
        return false;
    }
    if(duration>double(MaxFrames)/Fps+.001) { error=L"Divida a mídia em arquivos com até 2 horas antes de importar."; return false; }
    a.frames=std::max(1,std::min(MaxFrames,int(std::floor(duration*Fps+.5))));
    return true;
}
static std::wstring fitFilter(int w,int h,int fps,bool alpha=false) {
    std::wstring x=number(w), y=number(h), r=x+L"/"+y;
    // Use display rather than pixel aspect ratio (e.g. DV). The bit-exact scaler
    // also avoids legacy x86 MMX range-conversion artifacts on YUV/JPEG previews.
    return L"fps="+number(fps)+L",scale=w='if(gte(dar,"+r+L"),"+x+L",max(2,trunc("+y+L"*dar/2)*2))'"
        L":h='if(gte(dar,"+r+L"),max(2,trunc("+x+L"/dar/2)*2),"+y+L")':flags=bilinear+accurate_rnd+bitexact,setsar=1,"+
        (alpha?L"format=rgba,":L"")+L"pad="+x+L":"+y+L":(ow-iw)/2:(oh-ih)/2"+(alpha?L":color=black@0":L"");
}
static uint32_t le32(const unsigned char* bytes) {
    return uint32_t(bytes[0])|(uint32_t(bytes[1])<<8)|(uint32_t(bytes[2])<<16)|(uint32_t(bytes[3])<<24);
}
static bool scanAvi(File& f,uint64_t start,uint64_t end,bool frames,int depth,std::vector<FrameRef>& out) {
    unsigned char header[12];
    for(uint64_t offset=start;offset+8<=end;) {
        if(!f.seek(offset) || !f.get(header,8)) return false;
        uint64_t body=offset+8, sz=le32(header+4), next=body+sz+(sz&1);
        if(body+sz>end) return false;
        if(std::memcmp(header,"LIST",4)==0 || std::memcmp(header,"RIFF",4)==0) {
            if(sz<4 || depth>=5 || !f.get(header+8,4)) return false;
            if(std::memcmp(header+8,"AVI ",4)==0 || std::memcmp(header+8,"AVIX",4)==0 ||
               std::memcmp(header+8,"movi",4)==0 || (frames && std::memcmp(header+8,"rec ",4)==0)) {
                if(!scanAvi(f,body+4,body+sz,frames || std::memcmp(header+8,"movi",4)==0,depth+1,out)) return false;
            }
        } else if(frames && (std::memcmp(header,"00dc",4)==0 || std::memcmp(header,"00db",4)==0) && sz>0) {
            if(sz>1024*1024 || body>0xffffffffu || out.size()>=size_t(MaxFrames/2+1)) return false;
            out.push_back(FrameRef(uint32_t(body),uint32_t(sz)));
        }
        offset=next;
    }
    return true;
}
bool FrameReader::frame(const Proxy& p,int i,std::vector<uint8_t>& pixels) {
    if(i<0 || i>=int(p.frames.size())) return false;
    if(path_!=p.avi || !file_.good()) { path_=p.avi; if(!file_.open(path_)) return false; }
    const FrameRef& r=p.frames[i];
    jpeg_.resize(r.size);
    if(!file_.seek(r.offset) || !file_.get(jpeg_.data(),r.size)) return false;
    int w,h,n;
    uint8_t* raw=stbi_load_from_memory(jpeg_.data(),int(jpeg_.size()),&w,&h,&n,4);
    bool ok=raw && w==ProxyW && h==ProxyH;
    if(ok) {
        pixels.assign(raw,raw+w*h*4);
        for(size_t j=0;j<pixels.size();j+=4) std::swap(pixels[j],pixels[j+2]);
    }
    stbi_image_free(raw);
    return ok;
}
static void thumb(Proxy& p) {
    std::vector<uint8_t> pixels;
    FrameReader reader;
    if(!p.still.empty()) pixels=p.still;
    else if(!reader.frame(p,0,pixels)) return;
    p.thumb.resize(88*50*4);
    for(int y=0;y<50;++y) for(int x=0;x<88;++x) {
        size_t src=(y*ProxyH/50*ProxyW+x*ProxyW/88)*4;
        uint8_t* to=&p.thumb[(y*88+x)*4];
        for(int k=0;k<3;++k) to[k]=uint8_t(int(pixels[src+k])*pixels[src+3]/255);
        to[3]=255;
    }
}
static bool peaks(const MediaContext& ctx,Proxy& p) {
    File pcm(p.pcm);
    const int bin=PreviewRate*2/4;
    std::vector<int16_t> samples(bin);
    while(!ctx.stopped() && p.peaks.size()<size_t(MaxFrames/(Fps/4)+1)) {
        DWORD n=pcm.read(samples.data(),DWORD(bin*2));
        if(!n) break;
        int peak=0;
        for(DWORD i=0;i<n/2;++i) peak=std::max(peak,std::abs(int(samples[i])));
        p.peaks.push_back(uint8_t(std::min(255,peak/128)));
    }
    return pcm.good() && !ctx.stopped();
}
static void disposeProxy(Proxy* p) {
    if(!p->avi.empty()) DeleteFileW(p->avi.c_str());
    if(!p->pcm.empty()) DeleteFileW(p->pcm.c_str());
    delete p;
}
static void pcmOutput(Args& a,int index,int frames,int rate,int channels,const std::wstring& dest) {
    append(a,{L"-map",stream(index),L"-vn",L"-af",L"aresample=async=1:first_pts=0,apad",L"-t",seconds(frames),
        L"-ac",number(channels),L"-ar",number(rate),L"-c:a",L"pcm_s16le",L"-f",L"s16le",dest});
}
static bool pictureFile(const std::wstring& path) {
    size_t dot=path.find_last_of(L'.');
    if(dot==std::wstring::npos) return false;
    std::wstring ext=path.substr(dot);
    return equalPath(ext,L".png") || equalPath(ext,L".jpg") || equalPath(ext,L".jpeg") || equalPath(ext,L".bmp");
}
struct StillInput {
    std::wstring path;
    int width,height;
    StillInput() : width(0),height(0) {}
};
static bool decodeStill(const std::wstring& path,StillInput& dst,std::wstring& error) {
    File file(path);
    uint64_t size=file.size();
    if(size==0 || size>32*1024*1024) { error=L"Imagem vazia ou acima de 32 MB."; return false; }
    std::vector<uint8_t> bytes(size_t(size),0);
    if(!file.get(bytes.data(),DWORD(size))) { error=L"Não foi possível ler a imagem."; return false; }
    int w=0,h=0,ch=0;
    std::unique_ptr<uint8_t,void(*)(void*)> raw(stbi_load_from_memory(bytes.data(),int(size),&w,&h,&ch,4),stbi_image_free);
    if(!raw || w<1 || h<1 || w>4096 || h>4096) { error=L"Imagem inválida ou acima de 4096 × 4096."; return false; }
    File out(dst.path,true);
    if(!out.put(raw.get(),DWORD(w*h*4))) { error=L"Não há espaço para preparar a imagem."; return false; }
    dst.width=w; dst.height=h;
    return true;
}
static void rawInput(Args& args,const std::wstring& path,const wchar_t* format,int w,int h,int fps) {
    append(args,{L"-f",L"rawvideo",L"-pixel_format",format,L"-video_size",number(w)+L"x"+number(h),
        L"-framerate",number(fps),L"-threads",L"1",L"-i",path});
}
static bool importStill(const MediaContext& ctx,Asset& a,const std::wstring& temp,const std::wstring& log,std::wstring& error) {
    StillInput source;
    source.path=join(temp,L"still.rgba");
    if(!decodeStill(a.path,source,error) || ctx.stopped()) return false;
    a.still=true; a.video=0; a.frames=5*Fps;
    a.proxy=std::make_shared<Proxy>();
    std::wstring path=join(temp,L"proxy.bgra");
    Args args=ff();
    rawInput(args,source.path,L"rgba",source.width,source.height,ProxyFps);
    append(args,{L"-an",L"-vf",fitFilter(ProxyW,ProxyH,ProxyFps,true),L"-frames:v",L"1",L"-c:v",L"rawvideo",
        L"-pix_fmt",L"bgra",L"-threads",L"1",L"-sws_flags",L"bilinear+accurate_rnd+bitexact",L"-f",L"rawvideo",path});
    if(!run(ctx,L"ffmpeg.exe",args,log,error)) return false;
    File proxy(path);
    auto& pixels=a.proxy->still;
    pixels.resize(ProxyW*ProxyH*4);
    if(!proxy.get(pixels.data(),DWORD(pixels.size()))) { error=L"Prévia da imagem incompleta."; return false; }
    thumb(*a.proxy);
    return !ctx.stopped();
}
bool importMedia(const MediaContext& ctx,const std::wstring& path,Asset& result,std::wstring& error) {
    if(!exists(path)) { error=L"Arquivo não encontrado:\n"+path; return false; }
    TempDir job(ctx.scratch);
    if(job.path.empty()) { error=L"Não foi possível preparar a pasta temporária."; return false; }
    const std::wstring log=join(job.path,L"media.log");
    Asset a;
    a.path=path;
    a.name=baseName(path);
    if(pictureFile(path)) {
        if(!importStill(ctx,a,job.path,log,error)) return false;
        result=std::move(a);
        return true;
    }
    if(!inspect(ctx,a,log,error)) return false;
    std::wstring seed=tempFile(ctx.scratch);
    if(seed.empty()) { error=L"Não há acesso à pasta dos proxies."; return false; }
    DeleteFileW(seed.c_str());
    a.proxy=std::shared_ptr<Proxy>(new Proxy,disposeProxy);
    Args args=ff();
    append(args,{L"-threads",L"1",L"-i",path});
    if(a.video>=0) {
        a.proxy->avi=seed+L".avi";
        append(args,{L"-map",stream(a.video),L"-an",L"-vf",L"tpad=stop_mode=clone:stop_duration=0.15,"+fitFilter(ProxyW,ProxyH,ProxyFps),
            L"-frames:v",number((a.frames+1)/2),L"-codec:v",L"mjpeg",L"-pix_fmt",L"yuvj420p",L"-q:v",L"8",
            L"-threads",L"1",L"-f",L"avi",a.proxy->avi});
    }
    if(a.audio>=0) pcmOutput(args,a.audio,a.frames,PreviewRate,2,a.proxy->pcm=seed+L".pcm");
    if(!run(ctx,L"ffmpeg.exe",args,log,error)) return false;
    if(a.video>=0) {
        File avi(a.proxy->avi);
        if(!scanAvi(avi,0,avi.size(),false,0,a.proxy->frames) || a.proxy->frames.empty()) {
            error=L"A prévia não contém quadros válidos.";
            return false;
        }
        a.frames=std::min(a.frames,int(a.proxy->frames.size())*2);
        thumb(*a.proxy);
        if(a.proxy->thumb.empty()) { error=L"Não foi possível decodificar a prévia do vídeo."; return false; }
    }
    if(a.audio>=0 && !peaks(ctx,*a.proxy)) { error=L"Não foi possível preparar o áudio da prévia."; return false; }
    result=std::move(a);
    return !ctx.stopped();
}
static bool muted(const Sequence& s,Lane l) { return l==Video?s.muteA1:l==Music?s.muteA2:s.muteA3; }
static const Lane AudioLanes[]={Video,Music,Voice};
std::vector<Sound> previewSounds(const Project& p) {
    std::vector<Sound> out;
    for(Lane l:AudioLanes) {
        if(muted(p.seq,l)) continue;
        int pos=0;
        for(Clip c:laneOf(p.seq,l)) {
            const Asset& a=p.assets[c.asset];
            if(l==Video) { c.pos=pos; pos+=c.len; }
            if(a.proxy && !a.proxy->pcm.empty() && c.gain) out.push_back(Sound(a.proxy->pcm,c));
        }
    }
    return out;
}
void Mixer::set(std::vector<Sound> s) {
    sounds_.swap(s);
    for(int i=0;i<3;++i) { paths_[i].clear(); files_[i].close(); }
    last_=0;
}
File& Mixer::audioFile(const std::wstring& path) {
    for(int i=0;i<3;++i) if(paths_[i]==path) return files_[i];
    last_=(last_+1)%3;
    paths_[last_]=path;
    files_[last_].open(path);
    return files_[last_];
}
bool Mixer::fill(int t,int count,int rate,int channels,int16_t* out) {
    const int perFrame=rate/Fps;
    sum_.assign(count*channels,0);
    bool ok=true;
    for(const Sound& s:sounds_) {
        const Clip& c=s.clip;
        int begin=c.pos*perFrame,lo=std::max(t,begin),hi=std::min(t+count,begin+c.len*perFrame);
        if(hi<=lo || !c.gain) continue;
        DWORD n=DWORD((hi-lo)*channels);
        input_.resize(n);
        File& file=audioFile(s.path);
        if(!file.seek(uint64_t(c.in*perFrame+lo-begin)*channels*2)) { ok=false; continue; }
        DWORD got=file.read(input_.data(),n*2)/2;
        int32_t* offset=&sum_[(lo-t)*channels];
        int in=std::min(c.fx.fadeIn,s.fadeLength)*perFrame, end=std::min(c.fx.fadeOut,s.fadeLength)*perFrame;
        int elapsed=lo-begin+s.fadeOffset*perFrame, total=s.fadeLength*perFrame;
        if(!in && !end && (channels==1 || !c.fx.pan)) {
            for(DWORD i=0;i<got;++i) offset[i]+=int(input_[i])*c.gain/100;
        } else {
            int pan[]={100-std::max(0,c.fx.pan),100+std::min(0,c.fx.pan)};
            for(DWORD i=0;i+DWORD(channels)<=got;i+=channels,++elapsed) {
                int gain=256;
                if(in>1 && elapsed<in) gain=elapsed*256/(in-1);
                int remain=total-1-elapsed;
                if(end>1 && remain<end) gain=std::min(gain,std::max(0,remain)*256/(end-1));
                for(int k=0;k<channels;++k) {
                    int v=int(input_[i+k])*c.gain*gain/(100*256);
                    offset[i+k]+=channels==2?v*pan[k]/100:v;
                }
            }
        }
    }
    for(size_t i=0;i<sum_.size();++i) out[i]=int16_t(std::max(-32768,std::min(32767,int(sum_[i]))));
    return ok;
}

struct StagedOutput {
    std::wstring path;
    explicit StagedOutput(const std::wstring& target) : path(tempFile(dirName(target))) {}
    ~StagedOutput() { if(!path.empty()) DeleteFileW(path.c_str()); }
};
// Keep only the last visual sources' decoded stills on disk. Full resolution
// pictures are never held in memory while the sidecar is encoding.
class StillCache {
    StillInput slots_[3];
    int keys_[3], next_;
    bool used_[3];
public:
    explicit StillCache(const std::wstring& dir) : keys_{-1,-1,-1},next_(0),used_{false,false,false} {
        for(int i=0;i<3;++i) slots_[i].path=join(dir,L"still-"+number(i)+L".rgba");
    }
    void newSpan() { std::fill(used_,used_+3,false); }
    const StillInput* get(const Project& p,int key,std::wstring& error) {
        for(int i=0;i<3;++i) if(keys_[i]==key) { used_[i]=true; return &slots_[i]; }
        int use=next_;
        for(int i=0;i<3 && used_[use];++i) use=(use+1)%3;
        if(used_[use]) { error=L"Não foi possível preparar as imagens deste segmento."; return 0; }
        used_[use]=true;
        next_=(use+1)%3;
        keys_[use]=-1;
        if(!decodeStill(p.assets[key].path,slots_[use],error)) return 0;
        keys_[use]=key;
        return &slots_[use];
    }
};
static std::wstring decimal(int value) {
    int n=std::abs(value);
    return (value<0?L"-":L"")+number(n/1000)+L"."+number(1000+n%1000).substr(1);
}
struct ExportGraph {
    const Project& project;
    int width,height,frames,index;
    StillCache& cache;
    Args args;
    std::wstring graph;
    ExportGraph(const Project& p,RenderPreset preset,int n,StillCache& c) :
        project(p),width(preset.width),height(preset.height),frames(n),index(0),cache(c),args(ff()),
        graph(L"sws_flags=bilinear+accurate_rnd+bitexact;") {}
    std::wstring period() const { return L"trim=end_frame="+number(frames); }
    void black(const std::wstring& tag) {
        graph+=L"color=c=black:s="+number(width)+L"x"+number(height)+L":r="+number(Fps)+L","+period()+L"["+tag+L"];";
    }
    void over(const std::wstring& base,const std::wstring& layer,const std::wstring& tag,int x,int y,bool title=false) {
        graph+=L"["+base+L"]["+layer+L"]overlay=x="+number(x)+L":y="+number(y)+
            (title?L":eof_action=repeat:format=yuv420":L":shortest=1:format=yuv420")+L"["+tag+L"];";
    }
    bool layer(const Clip* clip,int frame,const std::wstring& base,const std::wstring& tag,std::wstring& error) {
        if(!clip || clip->fx.opacity==0) { if(base.empty()) black(tag); return !clip || base.empty(); }
        const Asset& a=project.assets[clip->asset];
        const Effects& fx=clip->fx;
        PictureBox b=pictureBox(fx,width,height);
        bool full=b.x==0 && b.y==0 && b.w==width && b.h==height;
        bool composition=!base.empty() || !full || a.still || fx.opacity!=100;
        int input=index++, skip=0;
        if(a.still) {
            const StillInput* still=cache.get(project,clip->asset,error);
            if(!still) return false;
            rawInput(args,still->path,L"rgba",still->width,still->height,Fps);
        } else {
            frame=std::min(frame,a.frames-1);
            int seek=std::max(0,frame-3); // keep decode handles for CFR rounding near the source tail
            skip=frame-seek;
            append(args,{L"-ss",seconds(seek),L"-threads",L"1",L"-i",a.path});
        }
        std::wstring label=composition?L"layer"+number(input):tag;
        graph+=L"["+number(input)+L":"+number(a.video)+L"]setpts=PTS-STARTPTS,"+
            fitFilter(b.w,b.h,Fps,a.still)+L",tpad=stop_mode=clone:stop="+number(frames+skip)+
            L",trim=start_frame="+number(skip)+L":end_frame="+number(frames+skip)+L",setpts=PTS-STARTPTS";
        if(fx.color() || fx.opacity!=100) {
            graph+=L",format=rgba";
            if(fx.saturation!=100 || fx.opacity!=100) {
                graph+=L",colorchannelmixer=aa="+decimal(fx.opacity*10);
                if(fx.saturation!=100) {
                    const wchar_t channels[]=L"rgb";
                    for(int out=0;out<3;++out) for(int in=0;in<3;++in) {
                        graph+=L":"; graph+=channels[out]; graph+=channels[in]; graph+=L"=";
                        graph+=decimal(saturationWeight(in,out,fx.saturation));
                    }
                }
            }
            if(fx.brightness || fx.contrast!=100) {
                std::wstring lut=L"'clip((val-128)*"+decimal(fx.contrast*10)+L"+"+number(128+fx.brightness)+L",0,255)'";
                graph+=L",lutrgb=r="+lut+L":g="+lut+L":b="+lut;
            }
        }
        graph+=(composition?L",format=yuva420p[":L",format=yuv420p[")+label+L"];";
        if(composition) {
            std::wstring background=base;
            if(background.empty()) { background=L"black"+number(input); black(background); }
            over(background,label,tag,b.x,b.y);
        }
        return true;
    }
    void dissolve(const Scene& s,const std::wstring& tag) {
        std::wstring t=L"clip((T*24+"+number(s.mainOffset)+L")/"+number(s.dissolve-1)+L",0,1)";
        graph+=L"[previous][main]blend=all_expr='A*(1-"+t+L")+B*"+t+L"':shortest=1["+tag+L"];";
    }
    void title(const std::wstring& raw,const std::wstring& base) {
        std::wstring input=number(index++)+L":v:0";
        rawInput(args,raw,L"bgra",width,height,Fps);
        over(base,input,L"picture",0,0,true);
    }
    void output(const std::wstring& label,const std::wstring& dest) {
        graph.pop_back();
        append(args,{L"-filter_complex",graph,L"-map",L"["+label+L"]",L"-an",L"-frames:v",number(frames),
            L"-c:v",L"libx264",L"-preset",L"ultrafast",L"-profile:v",L"baseline",L"-crf",L"21",
            L"-g",L"48",L"-pix_fmt",L"yuv420p",L"-threads",L"1",L"-map_metadata",L"-1",L"-f",L"mp4",dest});
    }
};
static bool renderPicture(const MediaContext& ctx,const Project& p,int first,int end,RenderPreset preset,
                          const std::wstring& temp,const std::wstring& log,std::wstring& playlist,std::wstring& error) {
    auto cuts=sceneCuts(p.seq,first,end);
    StillCache stills(temp);
    const std::wstring titleFile=join(temp,L"title.bgra");
    TitleStyle lastTitle;
    std::vector<uint8_t> glyphs;
    std::string entries="ffconcat version 1.0\n";
    for(size_t i=0;i+1<cuts.size();++i) {
        if(ctx.stopped()) return false;
        int t=cuts[i], n=cuts[i+1]-t;
        Scene scene=sceneAt(p.seq,t);
        ctx.report(3+62*(t-first)/(end-first),L"Compondo • "+timecode(t));
        stills.newSpan();
        ExportGraph g(p,preset,n,stills);
        if(!g.layer(scene.main,scene.main?scene.main->in+scene.mainOffset:0,L"",L"main",error)) return false;
        std::wstring label=L"main";
        if(scene.dissolve) {
            auto prev=scene.previous;
            if(!g.layer(prev,prev?prev->in+prev->len+scene.mainOffset:0,L"",L"previous",error)) return false;
            label=L"transition"; g.dissolve(scene,label);
        }
        if(scene.overlay && scene.overlay->fx.opacity>0) {
            if(!g.layer(scene.overlay,scene.overlay->in+scene.overlayOffset,label,L"upper",error)) return false;
            label=L"upper";
        }
        if(scene.title) {
            const TitleStyle& text=scene.title->title;
            if(!(lastTitle==text)) {
                if(!rasterTitle(text,preset.width,preset.height,glyphs)) { error=L"Não foi possível rasterizar o título."; return false; }
                File textImage(titleFile,true);
                if(!textImage.put(glyphs.data(),DWORD(glyphs.size()))) { error=L"Não foi possível gravar o título temporário."; return false; }
                lastTitle=text;
            }
            g.title(titleFile,label); label=L"picture";
        }
        std::wstring name=L"segment-"+number(int(i))+L".mp4";
        g.output(label,join(temp,name));
        if(!run(ctx,L"ffmpeg.exe",g.args,log,error,temp)) return false;
        entries+="file "+utf8(name)+"\nduration "+utf8(seconds(n))+"\n";
    }
    playlist=join(temp,L"sequence.ffconcat");
    if(!putText(playlist,entries)) { error=L"Não foi possível gravar a lista de segmentos."; return false; }
    return true;
}
static bool prepareMix(const MediaContext& ctx,const Project& p,int first,int end,const std::wstring& temp,
                       const std::wstring& log,std::wstring& mix,std::wstring& error) {
    std::vector<Sound> sounds;
    for(Lane lane:AudioLanes) {
        if(muted(p.seq,lane)) continue;
        int pos=0;
        for(const Clip& c:laneOf(p.seq,lane)) {
            if(lane!=Video) pos=c.pos;
            int start=pos;
            pos+=c.len;
            int lo=std::max(first,start), hi=std::min(end,pos);
            const Asset& a=p.assets[c.asset];
            if(hi<=lo || !c.gain || a.audio<0) continue;
            ctx.report(68,L"Preparando áudio • "+a.name);
            std::wstring pcm=join(temp,L"audio-"+number(int(sounds.size()))+L".pcm");
            Args args=ff();
            append(args,{L"-ss",seconds(c.in+lo-start),L"-threads",L"1",L"-i",a.path});
            pcmOutput(args,a.audio,hi-lo,RenderRate,2,pcm);
            if(!run(ctx,L"ffmpeg.exe",args,log,error)) return false;
            Clip part=c;
            part.in=0; part.pos=lo-first; part.len=hi-lo;
            sounds.push_back(Sound(pcm,part,lo-start,c.len));
        }
    }
    ctx.report(83,L"Mixando A1, A2 e A3");
    mix=join(temp,L"mix.pcm");
    File pcm(mix,true);
    Mixer mixer;
    mixer.set(std::move(sounds));
    int samples=(end-first)*(RenderRate/Fps);
    std::vector<int16_t> buf(4096*2);
    for(int start=0;start<samples;start+=4096) {
        if(ctx.stopped()) return false;
        int count=std::min(4096,samples-start);
        if(!mixer.fill(start,count,RenderRate,2,buf.data()) || !pcm.put(buf.data(),count*4)) {
            error=L"Não foi possível gravar a mixagem. Verifique o espaço disponível.";
            return false;
        }
    }
    return true;
}
bool render(const MediaContext& ctx,const Project& p,const std::wstring& target,RenderPreset preset,std::wstring& error) {
    if(ctx.stopped()) return false;
    if(!valid(p,true)) { error=L"A sequência contém uma referência de mídia ou edição inválida."; return false; }
    int total=length(p.seq), first=preset.first, end=preset.last<0?total:preset.last;
    if(first<0 || end>total || first>=end) { error=L"Sequência vazia ou intervalo de exportação inválido."; return false; }
    if(!preset.audioOnly && !((preset.width==640 && preset.height==360) || (preset.width==854 && preset.height==480) ||
            (preset.width==1280 && preset.height==720))) { error=L"Resolução de exportação não suportada."; return false; }
    for(const Asset& a:p.assets) if(equalPath(a.path,target)) { error=L"Escolha um nome diferente do arquivo original."; return false; }
    if(!p.file.empty() && equalPath(p.file,target)) { error=L"A exportação não pode substituir o projeto."; return false; }
    TempDir temp(ctx.scratch);
    StagedOutput out(target);
    if(temp.path.empty() || out.path.empty()) { error=L"Não foi possível criar o arquivo temporário. Verifique o espaço e as permissões."; return false; }
    std::wstring log=join(temp.path,L"render.log"),list,mix;
    if(!preset.audioOnly && !renderPicture(ctx,p,first,end,preset,temp.path,log,list,error)) return false;
    if(!prepareMix(ctx,p,first,end,temp.path,log,mix,error)) return false;
    ctx.report(94,preset.audioOnly?L"Finalizando WAV estéreo":L"Finalizando MP4 • H.264 + AAC");
    Args args=ff();
    if(!preset.audioOnly) append(args,{L"-f",L"concat",L"-safe",L"1",L"-i",list});
    append(args,{L"-f",L"s16le",L"-ar",number(RenderRate),L"-ac",L"2",L"-i",mix});
    if(preset.audioOnly) append(args,{L"-c:a",L"copy",L"-f",L"wav",out.path});
    else append(args,{L"-map",L"0:v:0",L"-map",L"1:a:0",L"-c:v",L"copy",L"-c:a",L"aac",L"-b:a",L"128k",
        L"-movflags",L"+faststart",L"-f",L"mp4",out.path});
    if(!run(ctx,L"ffmpeg.exe",args,log,error) || ctx.stopped()) return false;
    if(!MoveFileExW(out.path.c_str(),target.c_str(),MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH)) {
        error=L"Não foi possível gravar a mídia no destino. Verifique as permissões.";
        return false;
    }
    return true;
}
}
