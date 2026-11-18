#include "comp.hpp"
#include <cstdio>
#include <cstring>

using namespace nigh;
static void print(const std::wstring& message) { std::puts(utf8(message).c_str()); std::fflush(stdout); }
static int fail(const std::wstring& message) { print(L"FALHA: "+message); return 1; }
static bool duneColors(const Asset& a) {
    if(!a.proxy || a.proxy->thumb.size()!=88u*50u*4u) return false;
    const auto& t=a.proxy->thumb;
    const uint8_t* sand=&t[(10*88+30)*4];
    const uint8_t* sea=&t[(21*88+70)*4];
    // Warm dunes and cool water catch resize/color regressions that still decode to valid JPEGs.
    return sand[2]>150 && sand[2]>sand[1]+10 && sand[1]>sand[0]+10 &&
           sea[1]>sea[2]+12 && sea[0]>sea[2]+10;
}
// Synthetic 32x18 RGBA stills make composition tests independent of photographic colors.
static const unsigned char Red[]={
    137,80,78,71,13,10,26,10,0,0,0,13,73,72,68,82,
    0,0,0,32,0,0,0,18,8,6,0,0,0,58,200,220,
    82,0,0,0,38,73,68,65,84,120,156,99,120,33,37,249,
    127,32,49,195,168,3,70,29,48,234,128,81,7,140,58,96,
    212,1,163,14,24,117,192,64,59,0,0,204,192,186,189,223,
    224,159,231,0,0,0,0,73,69,78,68,174,66,96,130,
};
static const unsigned char Blue[]={
    137,80,78,71,13,10,26,10,0,0,0,13,73,72,68,82,
    0,0,0,32,0,0,0,18,8,6,0,0,0,58,200,220,
    82,0,0,0,38,73,68,65,84,120,156,99,144,144,127,240,
    127,32,49,195,168,3,70,29,48,234,128,81,7,140,58,96,
    212,1,163,14,24,117,192,64,59,0,0,96,236,177,189,14,
    196,105,195,0,0,0,0,73,69,78,68,174,66,96,130,
};
static const unsigned char Logo[]={
    137,80,78,71,13,10,26,10,0,0,0,13,73,72,68,82,
    0,0,0,32,0,0,0,18,8,6,0,0,0,58,200,220,
    82,0,0,0,41,73,68,65,84,120,156,99,16,125,164,246,
    159,66,204,64,9,102,24,117,192,168,3,70,29,48,234,128,
    81,7,140,58,96,212,1,163,14,24,104,7,0,0,122,253,
    160,78,134,72,65,93,0,0,0,0,73,69,78,68,174,66,
    96,130,
};
static const uint8_t* pixel(const std::vector<uint8_t>& frame,int x,int y) { return &frame[(y*ProxyW+x)*4]; }
static bool red(const uint8_t* p) { return p[2]>178 && p[0]<73 && p[1]<74; }
static bool blue(const uint8_t* p) { return p[0]>165 && p[2]<100 && p[1]<103; }
static bool purple(const uint8_t* p) { return p[0]>86 && p[2]>87 && p[1]<90; }
static bool titleVisible(const std::vector<uint8_t>& frame) {
    int count=0;
    for(int y=ProxyH*7/10;y<ProxyH;++y) for(int x=0;x<ProxyW;++x) {
        const uint8_t* p=pixel(frame,x,y);
        if(p[0]>150 && p[1]>150 && p[2]>150) ++count;
    }
    return count>36;
}
static void append32(std::string& data,uint32_t n) {
    for(int b=0;b<4;++b) data+=char(n>>(8*b));
}
static bool oldProject(const std::wstring& out,const std::wstring& media) {
    std::string data="NIGHED1\n", path=utf8(baseName(media));
    append32(data,1); // one relative asset
    append32(data,1); append32(data,uint32_t(path.size())); data+=path;
    for(int n:{2,1,0,0,7,0,96,0}) append32(data,n); // A2 muted, one V1 clip, A2 empty
    std::wstring file=join(out,L"Projeto antigo.nigh"), why;
    Project read;
    return putText(file,data) && readProject(file,read,why) && equalPath(read.assets[0].path,media) &&
        read.seq.muteA2 && read.seq.video[0]==Clip(0,0,7,0,96) && read.seq.overlay.empty() && read.seq.workIn==-1;
}
static bool audioEnvelope(const std::wstring& out) {
    std::wstring path=join(out,L"envelope.pcm");
    int frames=2, per=PreviewRate/Fps;
    std::vector<int16_t> samples(frames*per*2,12000);
    File file(path,true);
    if(!file.put(samples.data(),DWORD(samples.size()*2))) return false;
    file.close();
    Clip c(0,0,frames);
    c.fx.pan=100; c.fx.fadeIn=c.fx.fadeOut=1; c.gain=50;
    Mixer mix;
    mix.set({Sound(path,c)});
    if(!mix.fill(0,frames*per,PreviewRate,2,samples.data())) return false;
    for(size_t i=0;i<samples.size();i+=2) if(samples[i]) return false;
    if(samples[1] || samples.back() || samples[per-1]<2850 || samples[per-1]>3150 || samples[2*per+1]!=6000) return false;
    c.len=1;
    mix.set({Sound(path,c,1,2)});  // range starts in the second half of the clip
    return mix.fill(0,per,PreviewRate,2,samples.data()) && samples[1]==6000 && samples[2*per-1]==0;
}
static bool composition(const MediaContext& ctx,const Asset& ambience,const std::wstring& out,std::wstring& why) {
    struct Fixture { const unsigned char* bytes; size_t size; } fixtures[]={{Red,sizeof(Red)},{Blue,sizeof(Blue)},{Logo,sizeof(Logo)}};
    Project p;
    int j=0;
    for(const Fixture& f:fixtures) {
        std::wstring path=join(out,L"Imagem "+number(j++)+L".png");
        if(!putText(path,std::string(reinterpret_cast<const char*>(f.bytes),f.size))) return false;
        Asset a;
        if(!importMedia(ctx,path,a,why) || !a.still || a.frames!=120 || a.proxy->still.size()!=size_t(ProxyW*ProxyH*4)) return false;
        p.assets.push_back(std::move(a));
    }
    p.assets.push_back(ambience);
    if(place(p,0,Video,0,0,24).index<0 || place(p,1,Video,0,0,24).index<0 ||
        place(p,2,Overlay,28,0,14).index<0 || place(p,3,Voice,10,0,34).index<0) return false;
    p.seq.video[0].fx.dissolve=7;
    p.seq.video[1].fx.dissolve=11;
    p.seq.video[1].fx.contrast=110;
    p.seq.video[1].fx.brightness=10;
    p.seq.video[1].fx.saturation=86;
    p.seq.overlay[0].fx.opacity=60;
    p.seq.voice[0].fx.pan=25;
    p.seq.voice[0].fx.fadeIn=18; p.seq.voice[0].fx.fadeOut=17;
    Clip t(-1,0,31);
    t.title.text=L"Edição • expressão";
    t.title.size=9;
    if(paste(p.seq,Titles,t,9).index<0 || !putMarker(p.seq,24,L"Dissolução") || !setWorkEdge(p.seq,false,20) || !setWorkEdge(p.seq,true,44))
        return false;
    Project q;
    std::wstring project=join(out,L"Composição.nigh");
    if(!saveProject(p,project) || !readProject(project,q,why) || !(p.seq==q.seq) || !equalPath(p.assets[1].path,q.assets[1].path)) {
        why=L"Persistência de título, V2, A3, FX, marcações ou intervalo"; return false;
    }
    Preview preview;
    std::vector<uint8_t> frame;
    if(!preview.frame(p,0,-1,frame) || pixel(frame,40,28)[2]>3 || !preview.frame(p,6,-1,frame) || !red(pixel(frame,40,28)) ||
        !preview.frame(p,24,-1,frame) || !red(pixel(frame,40,28)) || !preview.frame(p,30,-1,frame) || !purple(pixel(frame,40,28)) ||
        !preview.frame(p,38,-1,frame) || !titleVisible(frame) || !blue(pixel(frame,265,31)) || pixel(frame,205,31)[1]<120) {
        why=L"Prévia: fade inicial, dissolução, cor, transparência ou título"; return false;
    }
    std::wstring output=join(out,L"Composição HD.mp4");
    RenderPreset hd(1280,720);
    hd.first=p.seq.workIn; hd.last=p.seq.workOut;
    if(!render(ctx,p,output,hd,why)) return false;
    Asset exported;
    if(!importMedia(ctx,output,exported,why) || exported.frames!=24) { why=L"Duração da exportação de intervalo: "+why; return false; }
    FrameReader frames;
    if(!frames.frame(*exported.proxy,0,frame) || !red(pixel(frame,40,28)) ||
        !frames.frame(*exported.proxy,5,frame) || !purple(pixel(frame,40,28)) ||
        !frames.frame(*exported.proxy,9,frame) || !titleVisible(frame) || !blue(pixel(frame,265,31)) || pixel(frame,205,31)[1]<105 ||
        !frames.frame(*exported.proxy,11,frame) || !blue(pixel(frame,205,31))) {
        why=L"MP4: conferir pixels em diferentes instantes da composição"; return false;
    }
    RenderPreset wav;
    wav.audioOnly=true; wav.first=hd.first; wav.last=hd.last;
    std::wstring audio=join(out,L"Mix estéreo.wav");
    if(!render(ctx,p,audio,wav,why) || File(audio).size()<192000) return false;
    // A title-only project exercises the generated background with no video inputs.
    Project onlyTitle;
    t.len=7;
    if(paste(onlyTitle.seq,Titles,t,0).index!=0 || !render(ctx,onlyTitle,join(out,L"Cartela.mp4"),RenderPreset(),why)) return false;
    return true;
}
extern "C" int wmain(int argc,wchar_t** argv) {
    if(argc!=4) return fail(L"Uso: smoke.exe pasta_codecs pasta_demo pasta_saída");
    const std::wstring tools=fullPath(argv[1]), demo=fullPath(argv[2]), out=fullPath(argv[3]);
    TempDir cache;
    if(!codecsPresent(tools) || cache.path.empty()) return fail(L"Motor ou pasta de teste não disponível");
    MediaContext ctx(tools,cache.path);
    ctx.progress=[](int percent,const std::wstring& line){print(number(percent)+L"% "+line);};
    std::wstring unicode=join(out,L"mídia de teste — dúnas.mp4");
    if(!CopyFileW(join(demo,L"Dunas.mp4").c_str(),unicode.c_str(),FALSE)) return fail(L"Cópia do fixture ("+number(int(GetLastError()))+L"): "+join(demo,L"Dunas.mp4")+L" -> "+unicode);
    Project p;
    std::wstring why;
    for(const std::wstring& path:{unicode,join(demo,L"Espuma.mp4"),join(demo,L"Horizonte.mp4"),join(demo,L"Ambiencia.m4a")}) {
        Asset a;
        if(!importMedia(ctx,path,a,why)) return fail(path+L": "+why);
        print(L"Importado: "+a.name+L" / "+number(a.frames)+L" quadros");
        if(a.video>=0) {
            FrameReader reader;
            std::vector<uint8_t> frame;
            if(!reader.frame(*a.proxy,int(a.proxy->frames.size())-1,frame) || frame.size()!=size_t(ProxyW*ProxyH*4))
                return fail(L"Leitura do último quadro da prévia");
        }
        if(p.assets.empty() && !duneColors(a)) return fail(L"Cores da prévia após redimensionar");
        p.assets.push_back(std::move(a));
    }
    if(!oldProject(out,unicode)) return fail(L"Compatibilidade com projetos v0.1");
    for(int i=0;i<3;++i) if(add(p,i,0).index<0) return fail(L"Inserção na V1");
    if(!trim(p,Selection(Video,0),true,8) || !split(p.seq,Selection(Video,1),138) ||
        move(p.seq,Selection(Video,2),4).index!=3 || !erase(p.seq,Selection(Video,1))) return fail(L"Edição do fixture");
    if(add(p,3,12).index<0 || !trim(p,Selection(Music,0),false,length(p.seq,Video)-12+6)) return fail(L"Trilha A2");
    p.seq.music[0].gain=75;
    if(!valid(p,true) || length(p.seq)!=length(p.seq,Video)+6) return fail(L"Limites da sequência");
    std::wstring project=join(out,L"Sequência de teste.nigh");
    Project reopened;
    if(!saveProject(p,project) || !readProject(project,reopened,why) || !(p.seq==reopened.seq) ||
        p.assets.size()!=reopened.assets.size() || !equalPath(reopened.assets[0].path,p.assets[0].path)) return fail(L"Salvar e reabrir projeto");
    // Real stereo/mono data is read and muted according to the active tracks.
    {
        Mixer mixer;
        int16_t samples[1024*2];
        mixer.set(previewSounds(p));
        if(!mixer.fill(24000,1024,PreviewRate,2,samples)) return fail(L"Mix de prévia");
        bool audible=false;
        for(int16_t sample:samples) audible=audible || sample!=0;
        if(!audible) return fail(L"Áudio inesperadamente silencioso");
        p.seq.muteA1=p.seq.muteA2=true;
        mixer.set(previewSounds(p));
        mixer.fill(24000,1024,PreviewRate,2,samples);
        for(int16_t sample:samples) if(sample!=0) return fail(L"Controle de silêncio");
        p.seq.muteA1=p.seq.muteA2=false;
    }
    std::wstring output=join(out,L"Vídeo exportado.mp4");
    if(!render(ctx,p,output,RenderPreset(),why) || File(output).size()<32768) return fail(L"Exportar 360p: "+why);
    print(L"MP4 360p • "+number(length(p.seq))+L" quadros esperados");
    std::wstring output480=join(out,L"Vídeo 480p.mp4");
    p.seq.muteA1=true;
    if(!render(ctx,p,output480,RenderPreset(854,480),why) || File(output480).size()<32768) return fail(L"Exportar 480p: "+why);
    Asset rendered;
    if(!importMedia(ctx,output480,rendered,why) || !duneColors(rendered) || rendered.frames!=length(p.seq))
        return fail(L"Duração ou cores do MP4 redimensionado: "+why);
    if(!audioEnvelope(out)) return fail(L"Fade e pan estéreo com fonte PCM conhecida");
    if(!composition(ctx,p.assets[3],out,why)) return fail(L"Composição multifaixa: "+why);
    Asset corrupt;
    if(!putText(join(out,L"inválido.mp4"),"not a movie") || importMedia(ctx,join(out,L"inválido.mp4"),corrupt,why))
        return fail(L"Rejeição de mídia inválida");
    HANDLE cancel=CreateEventW(0,TRUE,TRUE,0);
    MediaContext canceled(tools,cache.path,cancel);
    std::wstring untouched=join(out,L"não sobrescrever.mp4");
    if(!putText(untouched,"original")) return fail(L"Fixture da saída");
    bool canceledRender=render(canceled,p,untouched,RenderPreset(),why);
    CloseHandle(cancel);
    if(canceledRender || readText(untouched,64)!="original") return fail(L"Cancelamento e proteção da saída");
    print(L"Smoke Win32 concluído.");
    return 0;
}
