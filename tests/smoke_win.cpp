#include "media.hpp"
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
        int16_t samples[1024];
        mixer.set(previewSounds(p));
        if(!mixer.fill(24000,1024,PreviewRate,1,samples)) return fail(L"Mix de prévia");
        bool audible=false;
        for(int16_t sample:samples) audible=audible || sample!=0;
        if(!audible) return fail(L"Áudio inesperadamente silencioso");
        p.seq.muteA1=p.seq.muteA2=true;
        mixer.set(previewSounds(p));
        mixer.fill(24000,1024,PreviewRate,1,samples);
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
