#include "app.hpp"
#include <commdlg.h>
#include <windowsx.h>
#include <cwchar>
#include <cstring>
#include <cmath>

namespace nigh {
static const wchar_t Ready[]=L"Motor local • edição por proxies";
static const wchar_t ProjectFilter[]=L"Projeto Nigh (*.nigh)\0*.nigh\0\0";
static const wchar_t MediaFilter[]=L"Vídeo e áudio\0*.mp4;*.m4v;*.mov;*.avi;*.mkv;*.webm;*.mpg;*.mpeg;*.ts;*.mts;*.wmv;*.flv;*.dv;*.wav;*.mp3;*.m4a;*.aac;*.flac;*.ogg;*.opus;*.wma\0Todos os arquivos\0*.*\0\0";
static const wchar_t ExportFilter[]=L"Vídeo MP4 (*.mp4)\0*.mp4\0\0";
static const wchar_t* DemoFiles[]={L"Dunas.mp4",L"Espuma.mp4",L"Horizonte.mp4",L"Ambiencia.m4a"};
static bool projectPath(const std::wstring& s) {
    return s.size()>=5 && equalPath(s.substr(s.size()-5),L".nigh");
}
static bool control() { return (GetKeyState(VK_CONTROL)&0x8000)!=0; }
static bool shift() { return (GetKeyState(VK_SHIFT)&0x8000)!=0; }
static bool alt() { return (GetKeyState(VK_MENU)&0x8000)!=0; }
static std::vector<std::wstring> chooseOpen(HWND hwnd,const wchar_t* title,const wchar_t* filter,bool many) {
    std::vector<wchar_t> buf(32768,0);
    OPENFILENAMEW ofn;
    std::memset(&ofn,0,sizeof(ofn));
    ofn.lStructSize=sizeof(ofn); ofn.hwndOwner=hwnd; ofn.lpstrFilter=filter;
    ofn.lpstrTitle=title; ofn.lpstrFile=buf.data(); ofn.nMaxFile=DWORD(buf.size());
    ofn.Flags=OFN_EXPLORER|OFN_FILEMUSTEXIST|OFN_PATHMUSTEXIST|OFN_HIDEREADONLY|OFN_NOCHANGEDIR|(many?OFN_ALLOWMULTISELECT:0);
    if(!GetOpenFileNameW(&ofn)) return {};
    std::wstring first=buf.data();
    const wchar_t* p=buf.data()+first.size()+1;
    if(!many || !*p) return {first};
    std::vector<std::wstring> result;
    while(*p && result.size()<size_t(MaxAssets)) {
        std::wstring name=p;
        result.push_back(join(first,name)); p+=name.size()+1;
    }
    return result;
}
static std::wstring chooseSave(HWND hwnd,const wchar_t* title,const wchar_t* filter,const wchar_t* ext,std::wstring suggest) {
    wchar_t buf[MAX_PATH];
    for(wchar_t& ch:suggest) if(std::wcschr(L"\\/:*?\"<>|",ch)) ch=L'-';
    lstrcpynW(buf,suggest.c_str(),MAX_PATH);
    OPENFILENAMEW ofn;
    std::memset(&ofn,0,sizeof(ofn));
    ofn.lStructSize=sizeof(ofn); ofn.hwndOwner=hwnd; ofn.lpstrFilter=filter;
    ofn.lpstrTitle=title; ofn.lpstrFile=buf; ofn.nMaxFile=MAX_PATH; ofn.lpstrDefExt=ext;
    ofn.Flags=OFN_EXPLORER|OFN_OVERWRITEPROMPT|OFN_PATHMUSTEXIST|OFN_NOCHANGEDIR;
    return GetSaveFileNameW(&ofn)?fullPath(buf):L"";
}
App::App() : hwnd_(0),dir_(appDirectory()),frameProxy_(0),cachedFrame_(-1),back_(0),bmp_(0),baseBmp_(0),bw_(0),bh_(0),
    asset_(-1),source_(-1),sourceHead_(0),head_(0),zoom_(0),first_(0),assetScroll_(0),mouseX_(0),mouseY_(0),focus_(0),
    dirty_(false),demo_(false),mouseIn_(false),closeWhenDone_(false),changingCapture_(false),status_(Ready) {}
App::~App() {
    job_.reset();
    player_.stop(); reader_.reset();
    if(back_) {
        if(bmp_) { SelectObject(back_,baseBmp_); DeleteObject(bmp_); }
        DeleteDC(back_);
    }
}
void App::redraw(bool all) {
    if(all) InvalidateRect(hwnd_,0,FALSE);
    else {
        InvalidateRect(hwnd_,&ui_.viewer,FALSE);
        InvalidateRect(hwnd_,&ui_.timeline,FALSE);
    }
}
void App::updateTitle() {
    std::wstring title=project_.name+(dirty_?L" *":L"")+L" — Nigh Editor";
    SetWindowTextW(hwnd_,title.c_str());
}
void App::invalidateFrame() {
    reader_.reset(); frameProxy_=0; cachedFrame_=-1; pixels_.clear();
}
void App::explain(const std::wstring& text,UINT icon) {
    stop();
    MessageBoxW(hwnd_,text.c_str(),L"Nigh Editor",MB_OK|icon);
}
bool App::canDemo() const { return exists(join(dir_,L"demo\\Dunas.mp4")); }
void App::seek(int t,bool show) {
    stop();
    source_=-1;
    head_=limit(t,0,length(project_.seq));
    if(show && (head_<first_ || head_>first_+visibleFrames()-2)) first_=limit(head_-visibleFrames()/3,0,maxScroll());
    redraw(false);
}
void App::seekMonitor(int frame) {
    if(source_<0) { seek(frame); return; }
    stop();
    sourceHead_=limit(frame,0,monitorLength());
    InvalidateRect(hwnd_,&ui_.viewer,FALSE);
}
void App::play() {
    if(job_) return;
    if(player_.active()) { playbackTick(); stop(); return; }
    std::vector<Sound> sounds;
    if(source_>=0) {
        const Asset& a=project_.assets[source_];
        if(a.proxy && !a.proxy->pcm.empty()) sounds.push_back(Sound(a.proxy->pcm,Clip(source_,0,a.frames)));
    } else sounds=previewSounds(project_);
    if(player_.start(std::move(sounds),monitorLength(),monitorHead())) {
        SetTimer(hwnd_,1,30,0);
        playbackTick();
    }
    redraw(false);
}
void App::stop() {
    if(player_.active()) { player_.stop(); KillTimer(hwnd_,1); redraw(false); }
}
void App::playbackTick() {
    int& mark=source_>=0?sourceHead_:head_;
    int old=mark;
    mark=player_.tick();
    if(!player_.active()) KillTimer(hwnd_,1);
    if(source_<0 && (head_<first_ || head_>=first_+visibleFrames()-2)) first_=limit(head_-visibleFrames()/5,0,maxScroll());
    if(old!=mark || !player_.active()) {
        if(source_>=0) InvalidateRect(hwnd_,&ui_.viewer,FALSE);
        else redraw(false);
    }
}
void App::clearHistory() { undo_.clear(); redo_.clear(); }
void App::commit(Sequence previous,const std::wstring& msg) {
    if(!(previous==project_.seq)) {
        if(undo_.size()>=48) undo_.erase(undo_.begin());
        undo_.push_back(std::move(previous)); redo_.clear();
        status_=msg;
        dirty_=true; updateTitle();
        head_=limit(head_,0,length(project_.seq)); first_=limit(first_,0,maxScroll());
    }
    redraw();
}
void App::restore(bool forward) {
    auto& from=forward?redo_:undo_;
    auto& to=forward?undo_:redo_;
    if(from.empty()) return;
    stop();
    to.push_back(std::move(project_.seq));
    project_.seq=std::move(from.back()); from.pop_back();
    selected_=Selection(); dirty_=true; updateTitle();
    first_=limit(first_,0,maxScroll()); seek(head_);
    status_=forward?L"Alteração refeita":L"Alteração desfeita";
    redraw();
}
void App::addAsset(int asset,int pos,int before) {
    if(job_ || asset<0 || asset>=int(project_.assets.size())) return;
    stop();
    Sequence old=project_.seq;
    Selection s=add(project_,asset,pos,before);
    if(s.index<0) { status_=L"Não há espaço para este clipe na trilha (limite: 2 horas / 256 clipes por trilha)."; redraw(); return; }
    asset_=asset; selected_=s;
    seek(startOf(project_.seq,s));
    commit(std::move(old),L"Clipe adicionado • "+project_.assets[asset].name);
}
bool App::confirmDiscard() {
    if(!dirty_) return true;
    stop();
    int answer=MessageBoxW(hwnd_,L"Salvar as alterações antes de continuar?",L"Nigh Editor",MB_YESNOCANCEL|MB_ICONQUESTION);
    return answer==IDNO || (answer==IDYES && save(false));
}
void App::newProject() {
    if(!confirmDiscard()) return;
    stop(); invalidateFrame();
    project_=Project(); clearHistory();
    selected_=Selection(); source_=asset_=-1; head_=first_=zoom_=assetScroll_=0;
    dirty_=demo_=false; status_=L"Projeto em branco • importe suas mídias";
    updateTitle(); redraw();
}
bool App::save(bool as) {
    stop();
    std::wstring target=project_.file;
    if(as || target.empty()) target=chooseSave(hwnd_,L"Salvar projeto Nigh",ProjectFilter,L"nigh",project_.name+L".nigh");
    if(target.empty()) return false;
    if(!projectPath(target)) {
        explain(L"Use a extensão .nigh para o projeto.",MB_ICONWARNING); return false;
    }
    for(const Asset& a:project_.assets) if(equalPath(a.path,target)) {
        explain(L"O projeto não pode substituir um arquivo de mídia.",MB_ICONWARNING); return false;
    }
    if(!saveProject(project_,target)) { explain(L"Não foi possível salvar o projeto. Verifique o espaço e as permissões.",MB_ICONWARNING); return false; }
    project_.file=target;
    project_.name=baseName(target).substr(0,baseName(target).size()-5);
    dirty_=demo_=false;
    status_=L"Projeto salvo • "+baseName(target);
    updateTitle(); redraw();
    return true;
}
bool App::startJob(std::unique_ptr<Job> job) {
    if(job_) return false;
    stop(); cancelDrag();
    if(scratch_.path.empty()) { explain(L"Não foi possível acessar a pasta temporária deste computador.",MB_ICONWARNING); return false; }
    if(!codecsPresent(dir_)) {
        explain(L"ffmpeg.exe e ffprobe.exe são necessários. Extraia o ZIP inteiro e mantenha os três executáveis juntos.",MB_ICONWARNING);
        return false;
    }
    if(!job->start()) { explain(L"Não foi possível iniciar a operação.",MB_ICONWARNING); return false; }
    progress_=JobProgress(); job_=std::move(job);
    focus_=0;
    SetTimer(hwnd_,2,75,0);
    redraw(); return true;
}
void App::openProject(const std::wstring& path) {
    stop();
    std::wstring target=path;
    if(target.empty()) {
        auto paths=chooseOpen(hwnd_,L"Abrir projeto Nigh",ProjectFilter,false);
        if(paths.empty()) return;
        target=fullPath(paths[0]);
    }
    if(target.empty() || !confirmDiscard()) return;
    std::unique_ptr<Job> job(new Job(OpenJob,dir_,scratch_.path));
    job->target=target;
    startJob(std::move(job));
}
void App::showDemo() {
    if(!canDemo() || !confirmDiscard()) return;
    std::unique_ptr<Job> job(new Job(DemoJob,dir_,scratch_.path));
    for(const wchar_t* p:DemoFiles) job->paths.push_back(join(join(dir_,L"demo"),p));
    startJob(std::move(job));
}
void App::importFiles(std::vector<std::wstring> paths) {
    if(job_) return;
    std::unique_ptr<Job> next(new Job(ImportJob,dir_,scratch_.path));
    for(const std::wstring& f:paths) {
        std::wstring path=fullPath(f);
        if(path.empty()) continue;
        bool duplicate=false;
        for(const Asset& a:project_.assets) if(equalPath(a.path,path)) { duplicate=true; break; }
        if(!duplicate) for(const std::wstring& p:next->paths) if(equalPath(p,path)) { duplicate=true; break; }
        if(!duplicate && project_.assets.size()+next->paths.size()<size_t(MaxAssets)) next->paths.push_back(path);
    }
    if(next->paths.empty()) { status_=L"Nenhuma mídia nova para importar • limite de 128 arquivos por projeto"; redraw(); return; }
    startJob(std::move(next));
}
void App::exportTo(RenderPreset preset) {
    if(project_.seq.video.empty()) return;
    stop();
    const std::wstring target=chooseSave(hwnd_,L"Exportar vídeo MP4",ExportFilter,L"mp4",project_.name+L".mp4");
    if(target.empty()) return;
    if(target.size()<4 || !equalPath(target.substr(target.size()-4),L".mp4")) {
        explain(L"Use a extensão .mp4 para o vídeo exportado.",MB_ICONWARNING); return;
    }
    for(const Asset& a:project_.assets) if(equalPath(a.path,target)) {
        explain(L"Escolha outro nome para não substituir a mídia original.",MB_ICONWARNING); return;
    }
    std::unique_ptr<Job> job(new Job(ExportJob,dir_,scratch_.path));
    job->target=target; job->project=project_; job->preset=preset;
    startJob(std::move(job));
}
void App::pollJob() {
    if(!job_) return;
    if(!job_->done()) {
        if(job_->canceled()) progress_.label=L"Cancelando…";
        else progress_=job_->progress();
        redraw();
        return;
    }
    KillTimer(hwnd_,2);
    std::unique_ptr<Job> job=std::move(job_);
    if(closeWhenDone_) { closeWhenDone_=false; PostMessageW(hwnd_,WM_CLOSE,0,0); return; }
    if(job->canceled()) status_=L"Operação cancelada";
    else if(!job->success) {
        status_=L"Não foi possível concluir a operação";
        explain(job->error.empty()?status_:job->error,MB_ICONWARNING);
    } else if(job->kind==ExportJob) {
        status_=L"Exportado • "+baseName(job->target);
        int see=MessageBoxW(hwnd_,(L"Vídeo MP4 criado em:\n\n"+job->target+L"\n\nAbrir a pasta do vídeo?").c_str(),
            L"Exportação concluída",MB_YESNO|MB_ICONINFORMATION);
        if(see==IDYES) ShellExecuteW(hwnd_,L"open",dirName(job->target).c_str(),0,0,SW_SHOWNORMAL);
    } else if(job->kind==ImportJob) {
        int newIndex=int(project_.assets.size());
        int count=int(job->project.assets.size());
        for(Asset& a:job->project.assets) project_.assets.push_back(std::move(a));
        asset_=newIndex;
        if(project_.seq.video.empty()) { source_=asset_; sourceHead_=0; }
        assetScroll_=limit(asset_-ui_.rows+1,0,std::max(0,int(project_.assets.size())-ui_.rows));
        dirty_=true; updateTitle();
        status_=number(count)+(count==1?L" mídia importada":L" mídias importadas")+L" • use + para adicionar à linha do tempo";
        if(job->rejected) explain(number(job->rejected)+L" arquivo(s) não importado(s).\n\n"+job->error,MB_ICONWARNING);
    } else {
        invalidateFrame();
        project_=std::move(job->project); clearHistory();
        selected_=project_.seq.video.empty()?Selection():Selection(Video,0);
        source_=-1; asset_=project_.assets.empty()?-1:0;
        zoom_=first_=assetScroll_=0; demo_=job->kind==DemoJob; dirty_=false;
        head_=demo_?std::min(Fps*2+8,length(project_.seq)):0;
        status_=demo_?L"Demonstração Maré baixa • pressione Espaço para reproduzir":L"Projeto aberto • proxies recriados";
        updateTitle();
    }
    redraw();
}
void App::menu(int id) {
    stop();
    HMENU m=CreatePopupMenu();
    if(!m) return;
    if(id==CmdFile) {
        AppendMenuW(m,MF_STRING,CmdNew,L"&Novo projeto\tCtrl+N");
        AppendMenuW(m,MF_STRING,CmdOpen,L"&Abrir projeto…\tCtrl+O");
        AppendMenuW(m,MF_STRING,CmdSave,L"&Salvar\tCtrl+S");
        AppendMenuW(m,MF_STRING,CmdSaveAs,L"Salvar &como…\tCtrl+Shift+S");
        AppendMenuW(m,MF_SEPARATOR,0,0);
        AppendMenuW(m,MF_STRING,CmdImport,L"&Importar mídia…\tCtrl+I");
        if(canDemo()) AppendMenuW(m,MF_STRING,CmdDemo,L"Abrir &demonstração");
        AppendMenuW(m,MF_SEPARATOR,0,0);
        AppendMenuW(m,MF_STRING,CmdHelp,L"&Atalhos e ajuda\tF1");
        AppendMenuW(m,MF_STRING,CmdExit,L"Sai&r");
    } else {
        AppendMenuW(m,MF_STRING,CmdExport360,L"&360p leve  ·  640 × 360");
        AppendMenuW(m,MF_STRING,CmdExport480,L"&480p  ·  854 × 480");
    }
    RECT r=commandArea(id); POINT p={r.left,r.bottom+3}; ClientToScreen(hwnd_,&p);
    int cmd=TrackPopupMenu(m,TPM_RETURNCMD|TPM_NONOTIFY|TPM_LEFTALIGN|TPM_TOPALIGN,p.x,p.y,0,hwnd_,0);
    DestroyMenu(m);
    if(cmd) execute(cmd);
}
void App::execute(int id) {
    if(job_ && id!=CmdCancel && id!=CmdHelp) return;
    if(id>=CmdAssetBase) { addAsset(id-CmdAssetBase,head_); return; }
    switch(id) {
    case CmdFile: case CmdExport:
        if(id!=CmdExport || !project_.seq.video.empty()) menu(id);
        return;
    case CmdNew:newProject();return;
    case CmdOpen:openProject();return;
    case CmdSave:save(false);return;
    case CmdSaveAs:save(true);return;
    case CmdDemo:showDemo();return;
    case CmdExport360:exportTo(RenderPreset());return;
    case CmdExport480:exportTo(RenderPreset(854,480));return;
    case CmdImport: {
        stop(); auto p=chooseOpen(hwnd_,L"Importar vídeo ou áudio",MediaFilter,true);
        if(!p.empty()) importFiles(std::move(p));
        return;
    }
    case CmdHelp:
        explain(L"Fluxo rápido\nImporte mídia e use +, duplo clique ou arraste para a linha do tempo.\n"
            L"Arraste as bordas para aparar; arraste o centro para mover. A1 acompanha V1 e A2 é independente.\n\n"
            L"Espaço      Reproduzir / pausar\n←  →          Quadro anterior / seguinte\nHome / End      Início / fim\n"
            L"C      Dividir no cursor\nI / O      Aparar a entrada / saída até o cursor\nDelete      Remover clipe\n"
            L"Alt + ↑ / ↓      Ajustar volume do clipe\nCtrl + Z / Y      Desfazer / refazer\n"
            L"Ctrl + roda      Zoom da linha do tempo\nCtrl + I / S / O / E      Importar / salvar / abrir / exportar\n"
            L"F1      Ajuda         Tab / Enter      Navegar pelos botões"); return;
    case CmdExit: PostMessageW(hwnd_,WM_CLOSE,0,0);return;
    case CmdCancel: if(job_) { job_->cancel(); progress_.label=L"Cancelando…"; redraw(); } return;
    case CmdPlay:play();return;
    case CmdVideoMode:stop();source_=-1;redraw(false);return;
    case CmdFirst:seekMonitor(0);return;
    case CmdLast:seekMonitor(monitorLength());return;
    case CmdPrev:seekMonitor(monitorHead()-1);return;
    case CmdNext:seekMonitor(monitorHead()+1);return;
    case CmdZoomOut:zoomAt(-1,px(head_));return;
    case CmdZoomIn:zoomAt(1,px(head_));return;
    case CmdFit:zoom_=first_=0;redraw(false);return;
    case CmdUndo:restore(false);return;
    case CmdRedo:restore(true);return;
    default:break;
    }
    stop();
    Sequence old=project_.seq;
    std::wstring label;
    if(id==CmdMute1 || id==CmdMute2) {
        bool& val=id==CmdMute1?project_.seq.muteA1:project_.seq.muteA2;
        val=!val; label=(id==CmdMute1?L"A1 ":L"A2 ") + std::wstring(val?L"silenciada":L"reativada");
    } else {
        Clip* clip=get(project_.seq,selected_);
        if(!clip) return;
        int start=startOf(project_.seq,selected_);
        switch(id) {
        case CmdSplit:
            if(split(project_.seq,selected_,head_)) { ++selected_.index; label=L"Clipe dividido no cursor"; }
            else label=L"Posicione o cursor dentro do clipe para dividir";
            break;
        case CmdIn: case CmdOut:
            if(head_>start && head_<start+clip->len) {
                bool left=id==CmdIn;
                if(trim(project_,selected_,left,clip->in+head_-start)) {
                    if(left) seek(startOf(project_.seq,selected_));
                    label=left?L"Entrada do clipe aparada":L"Saída do clipe aparada";
                }
            }
            break;
        case CmdDelete: erase(project_.seq,selected_);selected_=Selection();label=L"Clipe removido";break;
        case CmdGainUp: case CmdGainDown:
            if(project_.assets[clip->asset].audio>=0) {
                clip->gain=limit(clip->gain+(id==CmdGainUp?5:-5),0,200);
                label=L"Volume do clipe • "+number(clip->gain)+L"%";
            }
            break;
        default:return;
        }
    }
    if(!label.empty()) status_=label;
    commit(std::move(old),label);
}
const Hit* App::hit(int x,int y) const {
    for(auto i=hits_.rbegin();i!=hits_.rend();++i) if(inside(i->area,x,y)) return &*i;
    return 0;
}
RECT App::commandArea(int id) const {
    for(const Hit& h:hits_) if(h.command==id) return h.area;
    return rect(15,15,80,31);
}
bool App::shortcut(WPARAM key,LPARAM flags) {
    if(key==VK_F1) { execute(CmdHelp);return true; }
    if(key==VK_TAB) {
        std::vector<int> focusable;
        for(const Hit& h:hits_) if(h.enabled && std::find(focusable.begin(),focusable.end(),h.command)==focusable.end()) focusable.push_back(h.command);
        if(!focusable.empty()) {
            auto it=std::find(focusable.begin(),focusable.end(),focus_);
            int i=it==focusable.end()?(shift()?0:-1):int(it-focusable.begin());
            int n=int(focusable.size());
            focus_=focusable[(i+(shift()?-1:1)+n)%n]; redraw();
        }
        return true;
    }
    if(key==VK_ESCAPE) {
        if(job_) execute(CmdCancel); else if(drag_.type!=Idle) cancelDrag(); else if(player_.active()) stop(); else { focus_=0; source_=-1; redraw(); }
        return true;
    }
    if(key==VK_RETURN && focus_) {
        for(const Hit& h:hits_) if(h.command==focus_ && h.enabled) { int cmd=h.command;execute(cmd);break; }
        return true;
    }
    if(job_) return false;
    if(drag_.type!=Idle) return false;
    if((flags&(1L<<30)) && key!=VK_LEFT && key!=VK_RIGHT && key!=VK_UP && key!=VK_DOWN) return false;
    if(control()) {
        switch(key) {
        case 'N':execute(CmdNew);return true;
        case 'O':execute(CmdOpen);return true;
        case 'S':execute(shift()?CmdSaveAs:CmdSave);return true;
        case 'I':execute(CmdImport);return true;
        case 'E':execute(CmdExport);return true;
        case 'Z':execute(shift()?CmdRedo:CmdUndo);return true;
        case 'Y':execute(CmdRedo);return true;
        default:return false;
        }
    }
    if(alt()) {
        if(key==VK_UP || key==VK_DOWN) { execute(key==VK_UP?CmdGainUp:CmdGainDown);return true; }
        return false;
    }
    switch(key) {
    case VK_SPACE:execute(CmdPlay);return true;
    case VK_HOME:execute(CmdFirst);return true;
    case VK_END:execute(CmdLast);return true;
    case VK_LEFT:execute(CmdPrev);return true;
    case VK_RIGHT:execute(CmdNext);return true;
    case 'C':execute(CmdSplit);return true;
    case 'I':execute(CmdIn);return true;
    case 'O':execute(CmdOut);return true;
    case VK_DELETE:execute(CmdDelete);return true;
    case VK_UP: case VK_DOWN:
        if(project_.assets.empty()) return false;
        stop(); asset_=limit(asset_+(key==VK_UP?-1:1),0,int(project_.assets.size())-1);
        source_=asset_; sourceHead_=0;
        if(asset_<assetScroll_) assetScroll_=asset_;
        if(asset_>=assetScroll_+ui_.rows) assetScroll_=asset_-ui_.rows+1;
        redraw();return true;
    case VK_RETURN:if(asset_>=0) addAsset(asset_,head_);return true;
    default:return false;
    }
}
void App::zoomAt(int delta,int anchor) {
    const RECT r=ui_.ruler;
    if(anchor<r.left || anchor>=r.right) anchor=centerX(r);
    int frame=frameAt(anchor);
    zoom_=limit(zoom_+delta,-6,8);
    if(zoom_==0) first_=0;
    else first_=limit(frame-int(double(anchor-r.left)/pixelsPerFrame()+.5),0,maxScroll());
    redraw(false);
}
int App::snap(int t,int skip,Lane lane) const {
    if(shift()) return t;
    int best=t, dist=5;
    const auto check=[&](int f) {
        int d=std::abs(px(f)-px(t));
        if(d<dist) { dist=d;best=f; }
    };
    check(head_); check(0);
    int v=0;
    for(const Clip& c:project_.seq.video) { v+=c.len;check(v); }
    for(int i=0;i<int(project_.seq.music.size());++i) if(lane!=Music || i!=skip) {
        const Clip& c=project_.seq.music[i];check(c.pos);check(c.pos+c.len);
    }
    return best;
}
void App::scrollToPointer(int x,int off) {
    int track=width(ui_.scroll)-width(scrollThumb());
    if(track>0) first_=limit(int(double(x-off-ui_.scroll.left)*maxScroll()/track+.5),0,maxScroll());
    redraw(false);
}
void App::track(Grab grab,int x,int y) {
    drag_=Drag(); drag_.type=grab; drag_.x=x; drag_.y=y; drag_.origHead=head_; drag_.at=frameAt(x); drag_.sel=selected_;
    SetCapture(hwnd_);
}
void App::mouseDown(int x,int y,bool dbl) {
    mouseX_=x; mouseY_=y;
    SetFocus(hwnd_); focus_=0;
    if(const Hit* h=hit(x,y)) {
        if(h->enabled && !dbl) execute(h->command);
        redraw(); return;
    }
    if(job_) return;
    if(inside(ui_.mediaList,x,y)) {
        int idx=assetScroll_+(y-ui_.mediaList.top)/57;
        if(idx<int(project_.assets.size())) {
            stop(); asset_=source_=idx; sourceHead_=0;
            if(dbl) { cancelDrag(); addAsset(idx,head_); }
            else { track(PoolItem,x,y); drag_.asset=idx; redraw(); }
        }
        return;
    }
    if(dbl) return;
    if(inside(ui_.gain,x,y)) { stop(); track(AdjustGain,x,y); drag_.moved=true; mouseMove(x,y); return; }
    if(inside(ui_.scroll,x,y) && maxScroll()>0) {
        RECT thumb=scrollThumb(); track(Scrollbar,x,y);
        drag_.off=inside(thumb,x,y)?x-thumb.left:width(thumb)/2;
        scrollToPointer(x,drag_.off); return;
    }
    if(inside(ui_.transportSeek,x,y)) {
        track(MonitorSeek,x,y); seekMonitor(monitorFrame(x)); return;
    }
    if(inside(ui_.ruler,x,y)) { track(Scrub,x,y); seek(frameAt(x),false);return; }
    for(int row=0;row<3;++row) if(inside(ui_.lanes[row],x,y)) {
        int f=frameAt(x);
        selected_=at(project_.seq,row==2?Music:Video,f);
        // Mark the right handle from inside the clip, including the last rounded pixel.
        if(selected_.index<0 && x>ui_.ruler.left) selected_=at(project_.seq,row==2?Music:Video,std::max(0,f-1));
        const Clip* c=get(project_.seq,selected_);
        Grab g=c?MoveClip:Scrub;
        int start=startOf(project_.seq,selected_);
        if(c && px(start+c->len)-px(start)>16 && row!=1) {
            if(x-px(start)<=7 && x>=px(start)-2) g=LeftEdge;
            else if(px(start+c->len)-x<=7 && x<=px(start+c->len)+2) g=RightEdge;
        }
        track(g,x,y); drag_.off=c?f-start:0;
        seek(f,false); redraw();return;
    }
    redraw();
}
void App::mouseMove(int x,int y) {
    const Hit* prev=mouseIn_?hit(mouseX_,mouseY_):0;
    const Hit* next=hit(x,y);
    bool media=inside(ui_.mediaList,x,y) || inside(ui_.mediaList,mouseX_,mouseY_);
    mouseX_=x;mouseY_=y;
    if(!mouseIn_) {
        TRACKMOUSEEVENT leave={sizeof(leave),TME_LEAVE,hwnd_,0};
        TrackMouseEvent(&leave); mouseIn_=true;
    }
    if(drag_.type==Idle) {
        if(prev!=next) {
            if(prev) InvalidateRect(hwnd_,&prev->area,FALSE);
            if(next) InvalidateRect(hwnd_,&next->area,FALSE);
        }
        if(media) InvalidateRect(hwnd_,&ui_.mediaList,FALSE);
        return;
    }
    if(std::abs(x-drag_.x)>3 || std::abs(y-drag_.y)>3) drag_.moved=true;
    if(drag_.type==Scrub) { seek(frameAt(x),false);return; }
    if(drag_.type==MonitorSeek) { seekMonitor(monitorFrame(x));return; }
    if(drag_.type==Scrollbar) { scrollToPointer(x,drag_.off);return; }
    if(drag_.type==AdjustGain) {
        drag_.targetX=-1;
        status_=L"Volume do clipe • "+number(limit((x-ui_.gain.left)*200/std::max(1,width(ui_.gain)-1),0,200))+L"%";
        redraw(); return;
    }
    if(!drag_.moved) return;
    drag_.targetX=-1;
    if(drag_.type==PoolItem || drag_.type==MoveClip) {
        int lane=drag_.type==PoolItem?(project_.assets[drag_.asset].video>=0?0:2):(drag_.sel.lane==Video?0:2);
        if(x>=ui_.ruler.left && x<ui_.ruler.right && y>=ui_.ruler.bottom && y<ui_.lanes[2].bottom) {
            if(lane==0) {
                int i=videoBoundary(project_.seq,frameAt(x));
                int f=i<int(project_.seq.video.size())?startOf(project_.seq,Selection(Video,i)):length(project_.seq,Video);
                drag_.targetX=px(f);
            } else {
                const Clip* c=get(project_.seq,drag_.sel);
                int len=drag_.type==PoolItem?project_.assets[drag_.asset].frames:c?c->len:1;
                int skip=drag_.type==PoolItem?-1:drag_.sel.index;
                int t=freePosition(project_.seq.music,skip,snap(frameAt(x)-drag_.off,skip,Music),len);
                if(t>=0) drag_.targetX=px(t);
            }
        }
    } else if(drag_.type==LeftEdge || drag_.type==RightEdge) {
        if(const Clip* c=get(project_.seq,drag_.sel)) {
            int a=startOf(project_.seq,drag_.sel);
            int origin=drag_.type==LeftEdge?a:a+c->len;
            drag_.targetX=px(snap(origin+frameAt(x)-drag_.at,drag_.sel.index,drag_.sel.lane));
        }
    }
    redraw();
}
void App::cancelDrag() {
    if(drag_.type==Idle) return;
    drag_=Drag();
    if(GetCapture()==hwnd_) { changingCapture_=true; ReleaseCapture(); changingCapture_=false; }
    redraw();
}
void App::mouseUp(int x,int y) {
    if(drag_.type==Idle) return;
    Drag drag=drag_;
    drag.moved=drag.moved || std::abs(x-drag.x)>3 || std::abs(y-drag.y)>3;
    cancelDrag();
    bool timeline=x>=ui_.ruler.left && x<ui_.ruler.right && y>=ui_.ruler.bottom && y<ui_.lanes[2].bottom;
    if(drag.type==PoolItem && drag.moved && timeline) {
        bool video=project_.assets[drag.asset].video>=0;
        addAsset(drag.asset,snap(frameAt(x)),video?videoBoundary(project_.seq,frameAt(x)):-1);
    } else if(drag.type==MoveClip && drag.moved && timeline) {
        Sequence old=project_.seq;
        int target=drag.sel.lane==Video?videoBoundary(project_.seq,frameAt(x)):snap(frameAt(x)-drag.off,drag.sel.index,Music);
        selected_=move(project_.seq,drag.sel,target);
        if(!(old==project_.seq)) seek(startOf(project_.seq,selected_));
        commit(std::move(old),L"Clipe movido");
    } else if((drag.type==LeftEdge || drag.type==RightEdge) && drag.moved) {
        Clip* c=get(project_.seq,drag.sel);
        if(c) {
            Sequence old=project_.seq;
            bool left=drag.type==LeftEdge;
            int start=startOf(project_.seq,drag.sel), origin=left?start:start+c->len;
            int t=snap(origin+frameAt(x)-drag.at,drag.sel.index,drag.sel.lane);
            if(trim(project_,drag.sel,left,c->in+t-start)) seek(left?startOf(project_.seq,drag.sel):start+c->len-1);
            commit(std::move(old),left?L"Entrada do clipe ajustada":L"Saída do clipe ajustada");
        }
    } else if(drag.type==AdjustGain) {
        if(Clip* c=get(project_.seq,drag.sel)) {
            Sequence old=project_.seq;
            c->gain=limit((x-ui_.gain.left)*200/std::max(1,width(ui_.gain)-1),0,200);
            commit(std::move(old),L"Volume do clipe • "+number(c->gain)+L"%");
        }
    }
}
void App::mouseWheel(int x,int y,int delta,bool ctrl) {
    if(job_ || drag_.type!=Idle) return;
    int turns=delta/WHEEL_DELTA;
    if(!turns && delta) turns=delta>0?1:-1;
    if(inside(ui_.media,x,y)) {
        assetScroll_=limit(assetScroll_-turns,0,std::max(0,int(project_.assets.size())-ui_.rows)); redraw();
    } else if(inside(ui_.timeline,x,y)) {
        if(ctrl) zoomAt(turns,x);
        else { first_=limit(first_-turns*std::max(1,visibleFrames()/8),0,maxScroll());redraw(false); }
    }
}
void App::doDrop(HDROP drop) {
    std::vector<std::wstring> paths;
    UINT count=std::min(UINT(MaxAssets),DragQueryFileW(drop,0xffffffffu,0,0));
    for(UINT i=0;i<count;++i) {
        wchar_t path[MAX_PATH];
        UINT n=DragQueryFileW(drop,i,0,0);
        if(n>0 && n<MAX_PATH && DragQueryFileW(drop,i,path,MAX_PATH)) paths.push_back(path);
    }
    DragFinish(drop);
    if(!job_ && !paths.empty()) {
        if(paths.size()==1 && projectPath(paths[0])) openProject(paths[0]);
        else importFiles(std::move(paths));
    }
}
void App::openArgs() {
    int count=0;
    LPWSTR* args=CommandLineToArgvW(GetCommandLineW(),&count);
    if(!args) return;
    if(count>1) {
        std::wstring arg=args[1];
        if(arg==L"--demo") showDemo();
        else if(projectPath(arg)) openProject(fullPath(arg));
        else if(!arg.empty() && arg[0]!=L'-') {
            std::vector<std::wstring> paths;
            for(int i=1;i<count && i<=MaxAssets;++i) paths.push_back(args[i]);
            importFiles(std::move(paths));
        }
    }
    LocalFree(args);
}
LRESULT CALLBACK App::wndProc(HWND hwnd,UINT msg,WPARAM w,LPARAM l) {
    App* app=reinterpret_cast<App*>(GetWindowLongPtrW(hwnd,GWLP_USERDATA));
    if(msg==WM_NCCREATE) {
        app=static_cast<App*>(reinterpret_cast<CREATESTRUCTW*>(l)->lpCreateParams);
        if(!app) return FALSE;
        app->hwnd_=hwnd;
        SetWindowLongPtrW(hwnd,GWLP_USERDATA,reinterpret_cast<LONG_PTR>(app));
    }
    return app?app->event(msg,w,l):DefWindowProcW(hwnd,msg,w,l);
}
LRESULT App::event(UINT msg,WPARAM w,LPARAM l) {
    switch(msg) {
    case WM_CREATE: DragAcceptFiles(hwnd_,TRUE); PostMessageW(hwnd_,WM_APP+1,0,0); updateTitle(); return 0;
    case WM_APP+1:openArgs();return 0;
    case WM_ERASEBKGND:return 1;
    case WM_PAINT:paint();return 0;
    case WM_GETMINMAXINFO: {
        MINMAXINFO* m=reinterpret_cast<MINMAXINFO*>(l);
        RECT min=rect(0,0,750,488); AdjustWindowRect(&min,WS_OVERLAPPEDWINDOW,FALSE);
        m->ptMinTrackSize.x=width(min);m->ptMinTrackSize.y=height(min);return 0;
    }
    case WM_SIZE:if(w!=SIZE_MINIMIZED) layout(LOWORD(l),HIWORD(l));redraw();return 0;
    case WM_MOUSEMOVE:mouseMove(GET_X_LPARAM(l),GET_Y_LPARAM(l));return 0;
    case WM_MOUSELEAVE:mouseIn_=false;redraw();return 0;
    case WM_LBUTTONDOWN:case WM_LBUTTONDBLCLK:mouseDown(GET_X_LPARAM(l),GET_Y_LPARAM(l),msg==WM_LBUTTONDBLCLK);return 0;
    case WM_LBUTTONUP:mouseUp(GET_X_LPARAM(l),GET_Y_LPARAM(l));return 0;
    case WM_CAPTURECHANGED:if(!changingCapture_) cancelDrag();return 0;
    case WM_MOUSEWHEEL: {
        POINT p={GET_X_LPARAM(l),GET_Y_LPARAM(l)}; ScreenToClient(hwnd_,&p);
        mouseWheel(p.x,p.y,GET_WHEEL_DELTA_WPARAM(w),(GET_KEYSTATE_WPARAM(w)&MK_CONTROL)!=0);return 0;
    }
    case WM_SETCURSOR:if(LOWORD(l)==HTCLIENT && drag_.type!=Idle) {
        LPCWSTR cursor=drag_.type==LeftEdge || drag_.type==RightEdge || drag_.type==Scrollbar || drag_.type==AdjustGain?IDC_SIZEWE:
            drag_.type==MoveClip || drag_.type==PoolItem?IDC_SIZEALL:IDC_ARROW;
        SetCursor(LoadCursorW(0,cursor));return TRUE;
    }break;
    case WM_KEYDOWN:case WM_SYSKEYDOWN:if(shortcut(w,l)) return 0;break;
    case WM_DROPFILES:doDrop(reinterpret_cast<HDROP>(w));return 0;
    case WM_TIMER:if(w==1 && player_.active()) playbackTick();else if(w==2) pollJob();return 0;
    case WM_CLOSE:
        if(job_) {
            if(MessageBoxW(hwnd_,L"Cancelar a operação em andamento e fechar?",L"Nigh Editor",MB_YESNO|MB_ICONQUESTION)==IDYES) {
                if(job_) { closeWhenDone_=true; execute(CmdCancel); }
                else PostMessageW(hwnd_,WM_CLOSE,0,0);
            }
        } else if(confirmDiscard()) DestroyWindow(hwnd_);
        return 0;
    case WM_DESTROY:stop(); PostQuitMessage(0);return 0;
    }
    return DefWindowProcW(hwnd_,msg,w,l);
}
}
