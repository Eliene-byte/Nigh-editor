#include "media.hpp"
#include <cmath>
#include <cstdlib>
#include <cstring>

#define STBI_ONLY_JPEG
#define STBI_NO_STDIO
#define STBI_NO_THREAD_LOCALS
#define STBI_NO_SIMD
#define STBI_NO_LINEAR
#define STBI_NO_FAILURE_STRINGS
#define STBI_MAX_DIMENSIONS 512
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
    return {L"-hide_banner",L"-v",L"error",L"-nostdin",L"-y",L"-max_alloc",L"67108864",L"-filter_threads",L"1"};
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
static std::wstring fitFilter(int w,int h,int fps) {
    std::wstring x=number(w), y=number(h), r=x+L"/"+y;
    // Use display rather than pixel aspect ratio (e.g. DV). The bit-exact scaler
    // also avoids legacy x86 MMX range-conversion artifacts on YUV/JPEG previews.
    return L"fps="+number(fps)+L",scale=w='if(gte(dar,"+r+L"),"+x+L",max(2,trunc("+y+L"*dar/2)*2))'"
        L":h='if(gte(dar,"+r+L"),max(2,trunc("+x+L"/dar/2)*2),"+y+L")':flags=bilinear+accurate_rnd+bitexact,setsar=1,pad="+x+L":"+y+L":(ow-iw)/2:(oh-ih)/2";
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
    if(!reader.frame(p,0,pixels)) return;
    p.thumb.resize(88*50*4);
    for(int y=0;y<50;++y) for(int x=0;x<88;++x) {
        size_t src=(y*ProxyH/50*ProxyW+x*ProxyW/88)*4;
        std::memcpy(&p.thumb[(y*88+x)*4],&pixels[src],4);
    }
}
static bool peaks(const MediaContext& ctx,Proxy& p) {
    File pcm(p.pcm);
    const int bin=PreviewRate/4;
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
bool importMedia(const MediaContext& ctx,const std::wstring& path,Asset& result,std::wstring& error) {
    if(!exists(path)) { error=L"Arquivo não encontrado:\n"+path; return false; }
    TempDir job(ctx.scratch);
    if(job.path.empty()) { error=L"Não foi possível preparar a pasta temporária."; return false; }
    const std::wstring log=join(job.path,L"media.log");
    Asset a;
    a.path=path;
    a.name=baseName(path);
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
    if(a.audio>=0) pcmOutput(args,a.audio,a.frames,PreviewRate,1,a.proxy->pcm=seed+L".pcm");
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
std::vector<Sound> previewSounds(const Project& p) {
    std::vector<Sound> out;
    int pos=0;
    for(Clip c:p.seq.video) {
        const Asset& a=p.assets[c.asset];
        if(a.proxy && !a.proxy->pcm.empty() && c.gain && !p.seq.muteA1) {
            c.pos=pos;
            out.push_back(Sound(a.proxy->pcm,c));
        }
        pos+=c.len;
    }
    if(!p.seq.muteA2) for(const Clip& c:p.seq.music) {
        const Asset& a=p.assets[c.asset];
        if(a.proxy && !a.proxy->pcm.empty() && c.gain) out.push_back(Sound(a.proxy->pcm,c));
    }
    return out;
}
void Mixer::set(std::vector<Sound> s) {
    sounds_.swap(s);
    for(int i=0;i<2;++i) { paths_[i].clear(); files_[i].close(); }
    last_=0;
}
File& Mixer::audioFile(const std::wstring& path) {
    for(int i=0;i<2;++i) if(paths_[i]==path) { last_=i; return files_[i]; }
    last_=1-last_;
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
        int in=c.pos*perFrame, lo=std::max(t,in), hi=std::min(t+count,in+c.len*perFrame);
        if(hi<=lo || !c.gain) continue;
        DWORD n=DWORD((hi-lo)*channels);
        input_.resize(n);
        File& file=audioFile(s.path);
        if(!file.seek(uint64_t(c.in*perFrame+lo-in)*channels*2)) { ok=false; continue; }
        DWORD got=file.read(input_.data(),n*2)/2;
        int32_t* offset=&sum_[(lo-t)*channels];
        for(DWORD i=0;i<got;++i) offset[i]+=int(input_[i])*c.gain/100;
    }
    for(size_t i=0;i<sum_.size();++i) out[i]=int16_t(std::max(-32768,std::min(32767,int(sum_[i]))));
    return ok;
}

struct StagedOutput {
    std::wstring path;
    explicit StagedOutput(const std::wstring& target) : path(tempFile(dirName(target))) {}
    ~StagedOutput() { if(!path.empty()) DeleteFileW(path.c_str()); }
};
static void videoOutput(Args& a,int index,int frames,RenderPreset p,const std::wstring& dest) {
    append(a,{L"-map",stream(index),L"-an",L"-vf",fitFilter(p.width,p.height,Fps)+L",tpad=stop_mode=clone:stop="+number(frames),
        L"-frames:v",number(frames),L"-c:v",L"libx264",L"-preset",L"ultrafast",L"-profile:v",L"baseline",L"-crf",L"21",
        L"-g",L"48",L"-pix_fmt",L"yuv420p",L"-threads",L"1",L"-map_metadata",L"-1",L"-f",L"mp4",dest});
}
bool render(const MediaContext& ctx,const Project& p,const std::wstring& target,RenderPreset preset,std::wstring& error) {
    if(!valid(p,true) || p.seq.video.empty()) { error=L"Adicione ao menos um clipe de vídeo a V1."; return false; }
    for(const Asset& a:p.assets) if(equalPath(a.path,target)) { error=L"Escolha um nome diferente do arquivo original."; return false; }
    TempDir temp(ctx.scratch);
    StagedOutput out(target);
    if(temp.path.empty() || out.path.empty()) { error=L"Não foi possível criar o arquivo temporário. Verifique o espaço e as permissões."; return false; }
    const std::wstring log=join(temp.path,L"render.log");
    std::string playlist="ffconcat version 1.0\n";
    std::vector<Sound> sounds;
    int total=length(p.seq), done=0, id=0;
    for(Clip c:p.seq.video) {
        const Asset& a=p.assets[c.asset];
        ctx.report(3+70*done/total,L"Codificando • "+a.name);
        std::wstring name=L"clip-"+number(id)+L".mp4";
        Args args=ff();
        // A single source decode per video clip; linked audio is emitted as a separate output.
        append(args,{L"-ss",seconds(c.in),L"-threads",L"1",L"-i",a.path});
        videoOutput(args,a.video,c.len,preset,join(temp.path,name));
        if(!p.seq.muteA1 && a.audio>=0 && c.gain) {
            std::wstring pcm=join(temp.path,L"v-"+number(id)+L".pcm");
            pcmOutput(args,a.audio,c.len,RenderRate,2,pcm);
            sounds.push_back(Sound(pcm,Clip(c.asset,0,c.len,done,c.gain)));
        }
        if(!run(ctx,L"ffmpeg.exe",args,log,error)) return false;
        playlist+="file "+utf8(name)+"\nduration "+utf8(seconds(c.len))+"\n";
        done+=c.len;
        ++id;
    }
    if(total>done) {
        ctx.report(3+70*done/total,L"Preenchendo o fim da sequência");
        std::wstring black=join(temp.path,L"black.rgb"), name=L"tail.mp4";
        if(!putText(black,std::string(16*16*3,'\0'))) { error=L"Não há espaço para os arquivos temporários."; return false; }
        Args args=ff();
        append(args,{L"-f",L"rawvideo",L"-pixel_format",L"rgb24",L"-video_size",L"16x16",L"-framerate",L"24",L"-i",black});
        videoOutput(args,0,total-done,preset,join(temp.path,name));
        if(!run(ctx,L"ffmpeg.exe",args,log,error)) return false;
        playlist+="file "+utf8(name)+"\nduration "+utf8(seconds(total-done))+"\n";
    }
    if(!p.seq.muteA2) for(const Clip& c:p.seq.music) {
        if(!c.gain) continue;
        const Asset& a=p.assets[c.asset];
        ctx.report(75,L"Preparando áudio • "+a.name);
        std::wstring pcm=join(temp.path,L"a-"+number(id++)+L".pcm");
        Args args=ff();
        append(args,{L"-ss",seconds(c.in),L"-threads",L"1",L"-i",a.path});
        pcmOutput(args,a.audio,c.len,RenderRate,2,pcm);
        if(!run(ctx,L"ffmpeg.exe",args,log,error)) return false;
        sounds.push_back(Sound(pcm,Clip(c.asset,0,c.len,c.pos,c.gain)));
    }
    const std::wstring mix=join(temp.path,L"mix.pcm"), list=join(temp.path,L"sequence.ffconcat");
    if(!putText(list,playlist)) { error=L"Não foi possível gravar a lista de segmentos."; return false; }
    ctx.report(87,L"Mixando as faixas A1 e A2");
    {
        File pcm(mix,true);
        Mixer mixer;
        mixer.set(std::move(sounds));
        int samples=total*(RenderRate/Fps);
        std::vector<int16_t> buf(4096*2);
        for(int start=0;start<samples;start+=4096) {
            if(ctx.stopped()) return false;
            int count=std::min(4096,samples-start);
            if(!mixer.fill(start,count,RenderRate,2,buf.data()) || !pcm.put(buf.data(),count*4)) {
                error=L"Não foi possível gravar a mixagem. Verifique o espaço disponível.";
                return false;
            }
        }
    }
    ctx.report(94,L"Finalizando MP4 • H.264 + AAC");
    Args args=ff();
    append(args,{L"-f",L"concat",L"-safe",L"1",L"-i",list,L"-f",L"s16le",L"-ar",number(RenderRate),L"-ac",L"2",L"-i",mix,
        L"-map",L"0:v:0",L"-map",L"1:a:0",L"-c:v",L"copy",L"-c:a",L"aac",L"-b:a",L"128k",L"-movflags",L"+faststart",L"-f",L"mp4",out.path});
    if(!run(ctx,L"ffmpeg.exe",args,log,error) || ctx.stopped()) return false;
    if(!MoveFileExW(out.path.c_str(),target.c_str(),MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH)) {
        error=L"O vídeo foi codificado, mas não foi possível salvar no destino. Verifique as permissões.";
        return false;
    }
    return true;
}
}
