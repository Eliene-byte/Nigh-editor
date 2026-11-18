#include "app.hpp"
#include "dialogs.hpp"
#include <commdlg.h>
#include <windowsx.h>
#include <cwchar>
#include <cstring>
#include <cmath>

namespace nigh {
static const wchar_t Ready[]=L"Motor local • edição por proxies";
static const wchar_t ProjectFilter[]=L"Projeto Nigh (*.nigh)\0*.nigh\0\0";
static const wchar_t MediaFilter[]=L"Vídeo, áudio e imagem\0*.jpg;*.jpeg;*.png;*.bmp;*.mp4;*.m4v;*.mov;*.avi;*.mkv;*.webm;*.mpg;*.mpeg;*.ts;*.mts;*.wmv;*.flv;*.dv;*.wav;*.mp3;*.m4a;*.aac;*.flac;*.ogg;*.opus;*.wma\0Todos os arquivos\0*.*\0\0";
static const wchar_t ExportFilter[]=L"Vídeo MP4 (*.mp4)\0*.mp4\0\0";
static const wchar_t WavFilter[]=L"Áudio WAV (*.wav)\0*.wav\0\0";
static std::wstring recoveryPath(const std::wstring& file) {
    std::wstring path=file+L".recuperacao";
    return file.empty() || path.size()>=MAX_PATH?L"":path;
}
static bool knownRecovery(const std::wstring& path) {
    if(!exists(path)) return false;
    Project snapshot;
    std::wstring why;
    return readProject(path,snapshot,why);
}
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
App::App() : hwnd_(0),dir_(appDirectory()),cachedFrame_(-1),rawFrame_(-1),rawAsset_(-1),back_(0),bmp_(0),baseBmp_(0),bw_(0),bh_(0),copiedLane_(Video),reviewIndex_(-1),
    asset_(-1),source_(-1),sourceHead_(0),head_(0),zoom_(0),first_(0),assetScroll_(0),mouseX_(0),mouseY_(0),focus_(0),
    dirty_(false),demo_(false),mouseIn_(false),closeWhenDone_(false),changingCapture_(false),hasClipboard_(false),useRange_(false),snapshotPending_(false),recovered_(false),assistMode_(false),eco_(false),hasReport_(false),quality_(false),status_(Ready) {}
App::~App() {
    job_.reset();
    player_.stop(); preview_.reset(); rawPreview_.reset();
    if(back_) {
        if(bmp_) { SelectObject(back_,baseBmp_); DeleteObject(bmp_); }
        DeleteDC(back_);
    }
}
void App::redraw(bool all) {
    if(all) InvalidateRect(hwnd_,0,FALSE);
    else {
        InvalidateRect(hwnd_,source_>=0 && ui_.dual?&ui_.sourcePanel:&ui_.viewer,FALSE);
        InvalidateRect(hwnd_,&ui_.timeline,FALSE);
    }
}
void App::updateTitle() {
    std::wstring title=project_.name+(dirty_?L" *":L"")+L" — Nigh Editor";
    SetWindowTextW(hwnd_,title.c_str());
}
void App::invalidateFrame() {
    preview_.reset(); rawPreview_.reset();
    cachedFrame_=rawFrame_=-1; rawAsset_=-1;
    pixels_.clear(); rawPixels_.clear();
}
void App::explain(const std::wstring& text,UINT icon) {
    stop();
    MessageBoxW(hwnd_,text.c_str(),L"Nigh Editor",MB_OK|icon);
}
bool App::canDemo() const { return exists(join(dir_,L"demo\\Dunas.mp4")); }
bool App::canAssistDemo() const { return exists(join(dir_,L"demo\\Caderno.mp4")); }
void App::focusMonitor(bool source) {
    int selected=source?asset_:-1;
    if(source_!=selected) { stop(); source_=selected; redraw(); }
}
void App::seek(int t,bool show) {
    stop();
    if(source_>=0 && ui_.dual) InvalidateRect(hwnd_,&ui_.sourcePanel,FALSE);
    source_=-1;
    head_=limit(t,0,length(project_.seq));
    if(show && (head_<first_ || head_>first_+visibleFrames()-2)) first_=limit(head_-visibleFrames()/3,0,maxScroll());
    redraw(false);
}
void App::seekMonitor(int frame) {
    if(source_<0) { seek(frame); return; }
    stop();
    sourceHead_=limit(frame,0,monitorLength());
    InvalidateRect(hwnd_,ui_.dual?&ui_.sourcePanel:&ui_.viewer,FALSE);
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
        if(source_>=0) InvalidateRect(hwnd_,ui_.dual?&ui_.sourcePanel:&ui_.viewer,FALSE);
        else redraw(false);
    }
}
void App::clearHistory() { undo_.clear(); redo_.clear(); }
void App::commit(Sequence previous,const std::wstring& msg) {
    if(!(previous==project_.seq)) {
        cachedFrame_=-1; snapshotPending_=true;
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
    if(project_.seq.workIn<0 && project_.seq.workOut<0) useRange_=false;
    cachedFrame_=-1; snapshotPending_=true;
    selected_=Selection(); dirty_=true; updateTitle();
    first_=limit(first_,0,maxScroll()); seek(head_);
    status_=forward?L"Alteração refeita":L"Alteração desfeita";
    redraw();
}
void App::addAsset(int asset,int pos,int before,Lane lane,bool insert) {
    if(job_ || asset<0 || asset>=int(project_.assets.size())) return;
    stop();
    const Asset& a=project_.assets[asset];
    if(lane==LaneCount) lane=a.video>=0?Video:Music;
    const auto& r=sourceRanges_[asset];
    int in=std::max(0,r.first), out=r.last<0?a.frames:r.last;
    Sequence old=project_.seq;
    Selection s=insert?insertVideo(project_,asset,pos,in,out-in):place(project_,asset,lane,pos,in,out-in,before);
    if(s.index<0) { status_=L"A mídia não cabe nesta faixa ou não tem o tipo necessário."; redraw(); return; }
    if(asset_!=asset) sourceHead_=0;
    asset_=asset; selected_=s;
    seek(startOf(project_.seq,s));
    commit(std::move(old),L"Clipe em "+std::wstring(laneName(s.lane))+L" • "+a.name);
}
void App::resetSource() {
    for(SourceRange& r:sourceRanges_) r=SourceRange();
    hasClipboard_=false; useRange_=false;
    hasReport_=false; analyzed_=AssistTarget(); report_=AssistReport(); reviewIndex_=-1; sourceHead_=0;
}
void App::markSource(bool out) {
    const Asset& a=project_.assets[source_];
    SourceRange& r=sourceRanges_[source_];
    int f=limit(sourceHead_,0,a.frames-1);
    if(out) { r.last=f+1; if(r.first>=r.last) r.first=-1; }
    else { r.first=f; if(r.last<=f) r.last=-1; }
    status_=(out?L"Saída da fonte • ":L"Entrada da fonte • ")+timecode(f)+L". Use Inserir ou arraste para a faixa.";
    redraw();
}
void App::trackProperties(bool newTitle) {
    Clip* selected=get(project_.seq,selected_);
    if(!newTitle && !selected) return;
    stop();
    Clip edit=newTitle?Clip(-1,0,std::min(72,MaxFrames-head_)):*selected;
    if(newTitle || selected_.lane==Titles) {
        int cap=MaxFrames-head_;
        if(!newTitle) {
            const auto& titles=project_.seq.titles;
            int next=selected_.index+1;
            cap=(next<int(titles.size())?titles[next].pos:MaxFrames)-edit.pos;
        }
        if(cap<1 || !editTitle(hwnd_,!newTitle,cap,edit)) return;
    } else {
        bool sound=selected_.lane!=Overlay && project_.assets[edit.asset].audio>=0;
        if(!editEffects(hwnd_,selected_.lane,sound,edit)) return;
    }
    Sequence old=project_.seq;
    if(newTitle) {
        selected_=paste(project_.seq,Titles,edit,head_);
        if(selected_.index<0) { explain(L"Sem espaço na faixa T1 para este título.",MB_ICONWARNING); return; }
        seek(startOf(project_.seq,selected_));
    } else {
        *selected=edit;
        if(source_>=0 || head_<startOf(project_.seq,selected_) || head_>=startOf(project_.seq,selected_)+edit.len)
            seek(startOf(project_.seq,selected_));
    }
    commit(std::move(old),newTitle?L"Título adicionado à faixa T1":L"Propriedades atualizadas");
}
void App::marker() {
    stop();
    const auto& markers=project_.seq.markers;
    auto i=std::lower_bound(markers.begin(),markers.end(),head_,[](const Marker& m,int frame){return m.frame<frame;});
    std::wstring text=i!=markers.end() && i->frame==head_?i->text:L"Marcador "+number(int(markers.size())+1);
    if(!editMarker(hwnd_,head_,text)) return;
    Sequence old=project_.seq;
    if(!putMarker(project_.seq,head_,text) && !text.empty()) { explain(L"Limite de 128 marcadores.",MB_ICONWARNING); return; }
    commit(std::move(old),text.empty()?L"Marcador removido":L"Marcador • "+text);
}
void App::autoSave() {
    if(job_ || !dirty_ || !snapshotPending_ || drag_.type!=Idle || !IsWindowEnabled(hwnd_)) return;
    const std::wstring target=recoveryPath(project_.file);
    for(const Asset& a:project_.assets) if(equalPath(a.path,target)) return;
    if(!target.empty() && (!exists(target) || knownRecovery(target)) && saveProject(project_,target)) {
        snapshotPending_=false; recovered_=true;
    }
}
void App::removeRecovery() {
    if(!recovered_) return;
    std::wstring path=recoveryPath(project_.file);
    if(knownRecovery(path)) DeleteFileW(path.c_str());
    recovered_=false;
}
bool App::confirmDiscard() {
    if(!dirty_) return true;
    stop();
    int answer=MessageBoxW(hwnd_,L"Salvar as alterações antes de continuar?",L"Nigh Editor",MB_YESNOCANCEL|MB_ICONQUESTION);
    return answer==IDNO || (answer==IDYES && save(false));
}
void App::newProject() {
    if(!confirmDiscard()) return;
    stop(); invalidateFrame(); removeRecovery(); resetSource();
    project_=Project(); clearHistory();
    selected_=Selection(); source_=asset_=-1; head_=first_=zoom_=assetScroll_=0;
    dirty_=demo_=snapshotPending_=false; status_=L"Projeto em branco • importe suas mídias";
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
    removeRecovery();
    project_.file=target;
    // A normal save supersedes any older snapshot for this destination.
    std::wstring abandoned=recoveryPath(target);
    if(knownRecovery(abandoned)) DeleteFileW(abandoned.c_str());
    project_.name=baseName(target).substr(0,baseName(target).size()-5);
    dirty_=demo_=snapshotPending_=false;
    status_=L"Projeto salvo • "+baseName(target);
    updateTitle(); redraw();
    return true;
}
bool App::startJob(std::unique_ptr<Job> job) {
    if(job_) return false;
    stop(); cancelDrag();
    if(scratch_.path.empty()) { explain(L"Não foi possível acessar a pasta temporária deste computador.",MB_ICONWARNING); return false; }
    if(job->kind!=AssistJob && !codecsPresent(dir_)) {
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
    std::wstring snapshot=recoveryPath(target);
    bool recover=knownRecovery(snapshot) && MessageBoxW(hwnd_,L"Há uma cópia de recuperação. Abrir as edições recuperadas?\n\nO projeto original não será alterado até você salvar.",
        L"Recuperar projeto",MB_YESNO|MB_ICONQUESTION)==IDYES;
    Project draft;
    std::wstring why;
    if(!readProject(recover?snapshot:target,draft,why)) {
        if(!recover || MessageBoxW(hwnd_,L"A cópia de recuperação não pôde ser lida. Abrir o original?",L"Nigh Editor",
            MB_YESNO|MB_ICONWARNING)!=IDYES || !readProject(target,draft,why)) { explain(why,MB_ICONWARNING); return; }
        recover=false;
    }
    draft.file=target;
    draft.name=baseName(target).substr(0,baseName(target).size()-5);
    bool changed=recover;
    std::wstring lastFolder;
    for(Asset& a:draft.assets) if(!exists(a.path)) {
        std::wstring relocated=lastFolder.empty()?L"":join(lastFolder,baseName(a.path));
        if(relocated.empty() || !exists(relocated)) {
            std::wstring prompt=L"Arquivo original ausente:\n"+a.path+L"\n\nLocalizar a mídia agora?";
            if(MessageBoxW(hwnd_,prompt.c_str(),L"Religar mídia",MB_OKCANCEL|MB_ICONWARNING)!=IDOK) return;
            auto file=chooseOpen(hwnd_,L"Localizar o arquivo de mídia",MediaFilter,false);
            if(file.empty()) return;
            relocated=fullPath(file[0]);
            if(relocated.empty()) return;
            lastFolder=dirName(relocated);
        }
        a.path=relocated; a.name=baseName(relocated); changed=true;
    }
    std::unique_ptr<Job> job(new Job(OpenJob,dir_,scratch_.path));
    job->openedWithChanges=changed;
    job->target=recover?snapshot:L"";
    job->project=std::move(draft);
    startJob(std::move(job));
}
void App::showDemo(bool assistant) {
    if(!(assistant?canAssistDemo():canDemo()) || !confirmDiscard()) return;
    std::unique_ptr<Job> job(new Job(assistant?AssistDemoJob:DemoJob,dir_,scratch_.path));
    if(assistant) job->paths.push_back(join(dir_,L"demo\\Caderno.mp4"));
    else for(const wchar_t* p:DemoFiles) job->paths.push_back(join(join(dir_,L"demo"),p));
    startJob(std::move(job));
}
void App::workspace(bool assistant) {
    if(assistMode_!=assistant) {
        stop(); cancelDrag(); assistMode_=assistant; layout(ui_.w,ui_.h);
        status_=assistMode_?L"Assistente local • selecione a fonte ou um clipe e analise":L"Montagem • proxies locais";
    }
    redraw();
}
AssistTarget App::targetForAnalysis() const {
    AssistTarget target;
    if(source_<0 && selected_.lane!=Titles) if(const Clip* c=get(project_.seq,selected_)) {
        target.asset=c->asset; target.first=c->in; target.end=c->in+c->len;
        target.sequenceStart=startOf(project_.seq,selected_); target.clipLength=c->len; target.clip=selected_;
        return target;
    }
    if(asset_>=0 && asset_<int(project_.assets.size())) {
        target.asset=asset_;
        const SourceRange& sr=sourceRanges_[asset_];
        target.first=std::max(0,sr.first);
        target.end=sr.last<0?project_.assets[asset_].frames:sr.last;
    }
    return target;
}
void App::startAnalysis() {
    workspace(true);
    AssistTarget target=targetForAnalysis();
    if(target.asset<0 || target.end<=target.first) {
        status_=L"Selecione um clipe ou abra uma mídia para analisar"; redraw(); return;
    }
    std::unique_ptr<Job> job(new Job(AssistJob,dir_,scratch_.path));
    job->request=target;
    job->project.assets=project_.assets; // shares proxies; no video or audio is copied
    startJob(std::move(job));
}
void App::actOnReport(int id) {
    if(!hasReport_ || analyzed_.asset<0 || analyzed_.asset>=int(project_.assets.size())) return;
    stop();
    bool clip=sameTarget(project_,analyzed_);
    const auto toSequence=[&](int f) { return analyzed_.sequenceStart+f-analyzed_.first; };
    const auto go=[&](int f) {
        if(clip) { selected_=analyzed_.clip; seek(toSequence(f)); }
        else { source_=asset_=analyzed_.asset; seekMonitor(f); }
    };
    if(id==CmdAiNext || id==CmdAiPrevious) {
        int n=int(report_.cues.size());
        if(n==0) return;
        reviewIndex_=reviewIndex_<0?(id==CmdAiNext?0:n-1):(reviewIndex_+(id==CmdAiNext?1:n-1))%n;
        const AssistCue& cue=report_.cues[reviewIndex_];
        go(cue.frame);
        status_=(cue.voiceIndex>=0?L"Fala provável • ":L"Corte provável • ")+timecode(cue.frame)+
            L" na fonte • "+number(reviewIndex_+1)+L" / "+number(n);
        redraw(); return;
    }
    if(id==CmdAiVoice) {
        if(report_.voice.empty()) return;
        int voice=reviewIndex_<0?-1:report_.cues[reviewIndex_].voiceIndex;
        const auto span=voice>=0?report_.voice.begin()+voice:
            std::max_element(report_.voice.begin(),report_.voice.end(),[](const AssistSpan& a,const AssistSpan& b){return a.end-a.first<b.end-b.first;});
        source_=asset_=analyzed_.asset;
        sourceRanges_[asset_].first=span->first; sourceRanges_[asset_].last=span->end;
        seekMonitor(span->first);
        status_=L"Trecho falado marcado na fonte • "+timecode(span->first)+L"–"+timecode(span->end)+L". Arraste ou use Inserir.";
        redraw(); return;
    }
    if(!clip) { status_=L"O clipe mudou. Selecione-o na sequência e analise novamente."; redraw(); return; }
    Sequence previous=project_.seq;
    Clip* c=get(project_.seq,analyzed_.clip);
    std::wstring label;
    if(id==CmdAiMarkers) {
        int n=0;
        const auto mark=[&](int f,const wchar_t* text) {
            int t=toSequence(f);
            const auto& markers=project_.seq.markers;
            auto i=std::lower_bound(markers.begin(),markers.end(),t,[](const Marker& m,int k){return m.frame<k;});
            if((i==markers.end() || i->frame!=t) && putMarker(project_.seq,t,text)) ++n;
        };
        for(int f:report_.cuts) mark(f,L"Corte provável");
        for(const AssistSpan& s:report_.voice) { mark(s.first,L"Início de fala provável"); mark(s.end,L"Fim de fala provável"); }
        label=n?number(n)+L" marcadores adicionados sem substituir outros":L"Nenhum marcador novo neste trecho";
    } else if(id==CmdAiSplit) {
        auto cuts=splitCandidates(project_,analyzed_,report_);
        if(cuts.empty()) return;
        if(laneOf(project_.seq,analyzed_.clip.lane).size()+cuts.size()>size_t(MaxClips)) {
            status_=L"Sem espaço para todos os cortes nesta faixa"; redraw(); return;
        }
        std::wstring msg=L"Dividir somente o clipe analisado em "+number(int(cuts.size()))+
            L" ponto(s) sugeridos? Nenhum quadro será excluído. Você pode desfazer de uma vez.";
        if(MessageBoxW(hwnd_,msg.c_str(),L"Aplicar cortes sugeridos",MB_OKCANCEL|MB_ICONQUESTION)!=IDOK) return;
        for(auto it=cuts.rbegin();it!=cuts.rend();++it) if(!split(project_.seq,analyzed_.clip,*it)) {
            project_.seq=std::move(previous); redraw(); return;
        }
        selected_=at(project_.seq,analyzed_.clip.lane,cuts.front());
        seek(cuts.front());
        label=number(int(cuts.size()))+L" cortes aplicados ao clipe • Ctrl+Z desfaz o lote";
    } else if(id==CmdAiFace && report_.face.size && analyzed_.clip.lane==Video) {
        frameFace(c->fx,project_.seq.format,report_.face);
        go(report_.faceFrame);
        label=L"Reenquadramento estático proposto aplicado • revise no monitor de programa";
    } else if(id==CmdAiTune) {
        if(report_.videoFrames && (analyzed_.clip.lane==Video || analyzed_.clip.lane==Overlay) && report_.brightness) c->fx.brightness=report_.brightness;
        if(report_.gain>=0 && analyzed_.clip.lane!=Overlay) c->gain=report_.gain;
        label=L"Sugestão de brilho e/ou volume aplicada a este clipe";
    } else return;
    status_=label;
    commit(std::move(previous),label);
}
void App::useFormat(FrameFormat format) {
    stop();
    Sequence old=project_.seq;
    project_.seq.format=format;
    commit(std::move(old),L"Formato da sequência • "+std::wstring(formatName(format)));
}
void App::exportQuality(int quality) {
    FrameSize sz=outputSize(project_.seq.format,quality);
    RenderPreset preset(sz.w,sz.h); preset.detailed=quality_;
    exportTo(preset);
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
    int total=length(project_.seq);
    if(!total) return;
    stop();
    if(useRange_) {
        preset.first=std::max(0,project_.seq.workIn);
        preset.last=project_.seq.workOut<0?total:std::min(total,project_.seq.workOut);
        if(preset.first>=preset.last) { explain(L"O intervalo marcado ficou fora da sequência. Ajuste ou limpe In/Out.",MB_ICONWARNING); return; }
    }
    const wchar_t* ext=preset.audioOnly?L"wav":L"mp4";
    const std::wstring suffix=L"."+std::wstring(ext);
    const std::wstring target=chooseSave(hwnd_,preset.audioOnly?L"Exportar áudio WAV":L"Exportar vídeo MP4",
        preset.audioOnly?WavFilter:ExportFilter,ext,project_.name+suffix);
    if(target.empty()) return;
    if(target.size()<4 || !equalPath(target.substr(target.size()-4),suffix)) {
        explain(L"Use a extensão "+suffix+L" para este formato.",MB_ICONWARNING); return;
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
    } else if(job->kind==AssistJob) {
        analyzed_=job->request;
        report_=std::move(job->analysis); hasReport_=true; reviewIndex_=-1;
        status_=report_.limited?L"Primeiros 90 s analisados • marque In/Out para outro trecho":L"Sugestões prontas • nenhum corte foi aplicado";
    } else if(job->kind==ExportJob) {
        status_=L"Exportado • "+baseName(job->target);
        int see=MessageBoxW(hwnd_,(L"Arquivo criado em:\n\n"+job->target+L"\n\nAbrir a pasta da exportação?").c_str(),
            L"Exportação concluída",MB_YESNO|MB_ICONINFORMATION);
        if(see==IDYES) ShellExecuteW(hwnd_,L"open",dirName(job->target).c_str(),0,0,SW_SHOWNORMAL);
    } else if(job->kind==ImportJob) {
        int newIndex=int(project_.assets.size());
        int count=int(job->project.assets.size());
        for(Asset& a:job->project.assets) project_.assets.push_back(std::move(a));
        asset_=newIndex; sourceHead_=0;
        if(source_>=0 || project_.seq.video.empty()) source_=asset_;
        assetScroll_=limit(asset_-ui_.rows+1,0,std::max(0,int(project_.assets.size())-ui_.rows));
        dirty_=snapshotPending_=true; updateTitle();
        status_=number(count)+(count==1?L" mídia importada":L" mídias importadas")+L" • use + para adicionar à linha do tempo";
        if(job->rejected) explain(number(job->rejected)+L" arquivo(s) não importado(s).\n\n"+job->error,MB_ICONWARNING);
    } else {
        invalidateFrame(); resetSource();
        if(!equalPath(project_.file,job->project.file)) removeRecovery();
        project_=std::move(job->project); clearHistory();
        selected_=project_.seq.video.empty()?Selection():Selection(Video,0);
        source_=-1; asset_=project_.assets.empty()?-1:0;
        bool aiDemo=job->kind==AssistDemoJob;
        zoom_=first_=assetScroll_=0; demo_=job->kind==DemoJob || aiDemo;
        dirty_=snapshotPending_=job->openedWithChanges;
        recovered_=!job->target.empty();
        useRange_=project_.seq.workIn>=0 || project_.seq.workOut>=0;
        head_=demo_?std::min(aiDemo?30:144,length(project_.seq)):0;
        if(demo_ && !project_.seq.overlay.empty()) selected_=Selection(Overlay,0);
        status_=aiDemo?L"Caderno de campo • rosto, cortes e duas falas prováveis":
            demo_?L"Maré baixa • títulos, transição, sobreposição e mixagem":
            dirty_?L"Projeto recuperado ou mídia religada • salve as alterações":L"Projeto aberto • proxies recriados";
        updateTitle();
        if(aiDemo) startAnalysis();
    }
    redraw();
}
void App::menu(int id,int x,int y) {
    stop();
    HMENU m=CreatePopupMenu();
    if(!m) return;
    const auto addItem=[&](int cmd,const wchar_t* text,bool enabled=true,bool checked=false) {
        AppendMenuW(m,MF_STRING|(enabled?0:MF_GRAYED)|(checked?MF_CHECKED:0),cmd,text);
    };
    if(id==CmdFile) {
        addItem(CmdNew,L"&Novo projeto\tCtrl+N");
        addItem(CmdOpen,L"&Abrir projeto…\tCtrl+O");
        addItem(CmdSave,L"&Salvar\tCtrl+S");
        addItem(CmdSaveAs,L"Salvar &como…\tCtrl+Shift+S");
        AppendMenuW(m,MF_SEPARATOR,0,0);
        addItem(CmdImport,L"&Importar mídia…\tCtrl+I");
        if(canDemo()) addItem(CmdDemo,L"Abrir &demonstração");
        if(canAssistDemo()) addItem(CmdAssistDemo,L"Demonstração do assistente local");
        AppendMenuW(m,MF_SEPARATOR,0,0);
        addItem(CmdHelp,L"&Atalhos e ajuda\tF1");
        addItem(CmdExit,L"Sai&r");
    } else if(id==CmdExport) {
        const int cmds[]={CmdExport360,CmdExport480,CmdExport720};
        const wchar_t* labels[]={L"MP4 &leve",L"MP4 &SD",L"MP4 &HD (mais lento)"};
        for(int i=0;i<3;++i) {
            FrameSize sz=outputSize(project_.seq.format,i);
            std::wstring label=std::wstring(labels[i])+L"   "+number(sz.w)+L" × "+number(sz.h);
            addItem(cmds[i],label.c_str());
        }
        addItem(CmdExportWav,L"&WAV estéreo       Som da sequência");
        AppendMenuW(m,MF_SEPARATOR,0,0);
        addItem(CmdQuality,L"Qualidade detalhada (mais lenta)",true,quality_);
        addItem(CmdUseRange,L"Exportar somente intervalo In / Out",project_.seq.workIn>=0 || project_.seq.workOut>=0,useRange_);
    } else if(id==CmdFormat) {
        addItem(CmdFormatWide,L"Horizontal   16:9",true,project_.seq.format==FrameWide);
        addItem(CmdFormatSquare,L"Quadrado   1:1",true,project_.seq.format==FrameSquare);
        addItem(CmdFormatTall,L"Vertical   9:16",true,project_.seq.format==FrameTall);
    } else if(id==CmdAiReview) {
        const auto& r=report_;
        bool mapped=hasReport_ && sameTarget(project_,analyzed_);
        bool events=hasReport_ && !r.cues.empty();
        bool pic=analyzed_.clip.lane==Video || analyzed_.clip.lane==Overlay;
        addItem(CmdAiNext,L"&Próxima sugestão no monitor\tF6",events);
        addItem(CmdAiPrevious,L"&Anterior\tShift+F6",events);
        AppendMenuW(m,MF_SEPARATOR,0,0);
        addItem(CmdAiMarkers,L"&Marcar cortes e falas na sequência",mapped && events);
        addItem(CmdAiSplit,L"&Dividir clipe nos cortes sugeridos…",mapped && pic && !splitCandidates(project_,analyzed_,r).empty());
        bool reviewingVoice=reviewIndex_>=0 && r.cues[reviewIndex_].voiceIndex>=0;
        addItem(CmdAiVoice,reviewingVoice?L"Usar a fala em revisão como trecho de &fonte":L"Usar a fala mais longa como trecho de &fonte",hasReport_ && !r.voice.empty());
        addItem(CmdAiFace,L"&Reenquadrar V1 pelo rosto (estático)\tF7",mapped && analyzed_.clip.lane==Video && r.face.size>0);
        std::wstring tune=L"&Aplicar ajuste leve";
        if(r.brightness && pic) tune+=L" · brilho "+std::wstring(r.brightness>0?L"+":L"")+number(r.brightness);
        if(r.gain>=0 && analyzed_.clip.lane!=Overlay) tune+=L" · volume "+number(r.gain)+L"%";
        addItem(CmdAiTune,tune.c_str(),mapped && ((pic && r.brightness) || (r.gain>=0 && analyzed_.clip.lane!=Overlay)));
        AppendMenuW(m,MF_SEPARATOR,0,0);
        addItem(CmdAnalyze,L"Analisar seleção atual\tF4",targetForAnalysis().asset>=0);
        addItem(CmdAssistDemo,L"Abrir demo de análise",canAssistDemo());
    } else {
        const Clip* c=get(project_.seq,selected_);
        const Asset* a=asset_>=0?&project_.assets[asset_]:0;
        addItem(CmdProps,L"&Propriedades do clipe…\tF2",c!=0);
        addItem(CmdTitle,L"Novo &título ou legenda…\tT",head_<MaxFrames);
        addItem(CmdMarker,L"&Marcador no cursor…\tM");
        addItem(CmdFormatWide,L"Sequência horizontal   16:9",true,project_.seq.format==FrameWide);
        addItem(CmdFormatSquare,L"Sequência quadrada   1:1",true,project_.seq.format==FrameSquare);
        addItem(CmdFormatTall,L"Sequência vertical   9:16",true,project_.seq.format==FrameTall);
        AppendMenuW(m,MF_SEPARATOR,0,0);
        addItem(CmdInsert,L"&Inserir fonte em V1 no cursor\t,",a && a->video>=0);
        addItem(CmdAddOverlay,L"Enviar fonte para &V2",a && a->video>=0);
        addItem(CmdAddMusic,L"Enviar áudio da fonte para A&2",a && a->audio>=0);
        addItem(CmdAddVoice,L"Enviar áudio da fonte para A&3",a && a->audio>=0);
        AppendMenuW(m,MF_SEPARATOR,0,0);
        addItem(CmdCopy,L"Copiar clipe\tCtrl+C",c!=0);
        addItem(CmdPaste,L"Colar clipe no cursor\tCtrl+V",hasClipboard_);
        addItem(CmdDuplicate,L"Duplicar clipe\tCtrl+D",c!=0);
        addItem(CmdDelete,L"Remover clipe\tDelete",c!=0);
        AppendMenuW(m,MF_SEPARATOR,0,0);
        addItem(CmdWorkIn,L"Marcar entrada da exportação\tAlt+I",head_<length(project_.seq));
        addItem(CmdWorkOut,L"Marcar saída da exportação\tAlt+O",head_>0);
        addItem(CmdWorkClear,L"Limpar intervalo de exportação\tAlt+X",project_.seq.workIn>=0 || project_.seq.workOut>=0);
    }
    RECT r=commandArea(id); POINT p={x<0?r.left:x,y<0?r.bottom+3:y}; ClientToScreen(hwnd_,&p);
    int cmd=TrackPopupMenu(m,TPM_RETURNCMD|TPM_NONOTIFY|TPM_LEFTALIGN|TPM_TOPALIGN,p.x,p.y,0,hwnd_,0);
    DestroyMenu(m);
    if(cmd) execute(cmd);
}
void App::execute(int id) {
    if(job_ && id!=CmdCancel && id!=CmdHelp) return;
    if(id>=CmdAssetBase) { addAsset(id-CmdAssetBase,head_); return; }
    switch(id) {
    case CmdFile: case CmdExport: case CmdTools: case CmdFormat: case CmdAiReview:
        if(id!=CmdExport || length(project_.seq)) menu(id);
        return;
    case CmdNew:newProject();return;
    case CmdOpen:openProject();return;
    case CmdSave:save(false);return;
    case CmdSaveAs:save(true);return;
    case CmdDemo:showDemo();return;
    case CmdAssistDemo:showDemo(true);return;
    case CmdWorkspaceEdit:workspace(false);return;
    case CmdWorkspaceAssist:workspace(true);return;
    case CmdEco:
        stop(); cancelDrag(); eco_=!eco_; layout(ui_.w,ui_.h);
        status_=eco_?L"Modo Eco • um monitor, prévia de 6 qps durante a reprodução":L"Prévia normal • 12 qps";
        redraw();return;
    case CmdFormatWide:useFormat(FrameWide);return;
    case CmdFormatSquare:useFormat(FrameSquare);return;
    case CmdFormatTall:useFormat(FrameTall);return;
    case CmdAnalyze:startAnalysis();return;
    case CmdAiNext:case CmdAiPrevious:case CmdAiMarkers:case CmdAiSplit:case CmdAiFace:case CmdAiVoice:case CmdAiTune:
        actOnReport(id);return;
    case CmdExport360:exportQuality(0);return;
    case CmdExport480:exportQuality(1);return;
    case CmdExport720:exportQuality(2);return;
    case CmdExportWav: { RenderPreset wav; wav.audioOnly=true; exportTo(wav); return; }
    case CmdQuality:quality_=!quality_;
        status_=quality_?L"Exportação MP4 • qualidade detalhada, mais lenta":L"Exportação MP4 • codificação leve";
        redraw();return;
    case CmdUseRange:if(project_.seq.workIn>=0 || project_.seq.workOut>=0) {
        useRange_=!useRange_;
        status_=useRange_?L"Exportação: somente o intervalo In / Out":L"Exportação: sequência inteira";
        redraw();
    }return;
    case CmdImport: {
        stop(); auto p=chooseOpen(hwnd_,L"Importar vídeo, áudio ou imagem",MediaFilter,true);
        if(!p.empty()) importFiles(std::move(p));
        return;
    }
    case CmdHelp:
        explain(L"Fluxo rápido\nArraste da biblioteca para V1/V2 ou A2/A3. V1 fecha lacunas automaticamente; A1 acompanha V1. V2 tem apenas imagem.\n"
            L"Arraste o meio para mover, as bordas para aparar e use o botão direito para mais ferramentas.\n\n"
            L"Espaço / ← →      Reproduzir e navegar quadro a quadro\n"
            L"I / O      Marcar trecho no monitor de fonte ou aparar clipe na sequência\n"
            L", (vírgula)     Inserir fonte no cursor de V1; X limpa In / Out da fonte\n"
            L"C / Delete     Dividir / remover clipe; F2 abre Propriedades\n"
            L"T      Novo título; M cria marcador; Page Up / Down navega marcadores\n"
            L"Alt+I / O / X     Marcar entrada, saída ou limpar intervalo de exportação\n"
            L"Ctrl+C / V / D     Copiar / colar / duplicar clipe\n"
            L"Ctrl+Z / Y     Desfazer / refazer; Ctrl+roda para zoom; Shift ignora encaixe\n"
            L"F3 / F4 / F5     Assistente, analisar e menu de sugestões\n"
            L"F6 / F7 / F8     Próxima sugestão, reenquadrar V1 e modo Eco\n"
            L"Ctrl+I / S / O / E     Importar / salvar / abrir / exportar"); return;
    case CmdExit: PostMessageW(hwnd_,WM_CLOSE,0,0);return;
    case CmdCancel: if(job_) { job_->cancel(); progress_.label=L"Cancelando…"; redraw(); } return;
    case CmdPlay:play();return;
    case CmdPlaySource:if(asset_>=0) { focusMonitor(true); play(); }return;
    case CmdPlayProgram:focusMonitor(false);play();return;
    case CmdVideoMode:focusMonitor(false);return;
    case CmdSourceMode:if(asset_>=0) focusMonitor(true);return;
    case CmdFirst:seekMonitor(0);return;
    case CmdLast:seekMonitor(monitorLength());return;
    case CmdPrev:seekMonitor(monitorHead()-1);return;
    case CmdNext:seekMonitor(monitorHead()+1);return;
    case CmdZoomOut:zoomAt(-1,px(head_));return;
    case CmdZoomIn:zoomAt(1,px(head_));return;
    case CmdFit:zoom_=first_=0;redraw(false);return;
    case CmdUndo:restore(false);return;
    case CmdRedo:restore(true);return;
    case CmdTitle:trackProperties(true);return;
    case CmdProps:trackProperties();return;
    case CmdMarker:marker();return;
    case CmdInsert:addAsset(asset_,head_,-1,Video,true);return;
    case CmdAddOverlay:addAsset(asset_,head_,-1,Overlay);return;
    case CmdAddMusic:addAsset(asset_,head_,-1,Music);return;
    case CmdAddVoice:addAsset(asset_,head_,-1,Voice);return;
    case CmdSourceClear:if(source_>=0) { sourceRanges_[source_]=SourceRange(); status_=L"Intervalo da fonte limpo"; redraw(); }return;
    case CmdMarkPrev:case CmdMarkNext: {
        int total=length(project_.seq), next=id==CmdMarkNext?total:0;
        for(const Marker& m:project_.seq.markers) if(m.frame<=total) {
            if(id==CmdMarkNext && m.frame>head_) { next=m.frame; break; }
            if(id==CmdMarkPrev && m.frame<head_) next=m.frame;
        }
        seek(next);
        for(const Marker& m:project_.seq.markers) if(m.frame==next) { status_=m.text; break; }
        redraw();return;
    }
    case CmdCopy:if(const Clip* c=get(project_.seq,selected_)) {
        clipboard_=*c; copiedLane_=selected_.lane; hasClipboard_=true; status_=L"Clipe copiado • Ctrl+V para colar no cursor"; redraw();
    }return;
    default:break;
    }
    stop();
    if(source_>=0 && (id==CmdIn || id==CmdOut)) { markSource(id==CmdOut); return; }
    Sequence old=project_.seq;
    std::wstring label;
    if(id==CmdMute1 || id==CmdMute2 || id==CmdMute3 || id==CmdHideOverlay) {
        bool& val=id==CmdMute1?project_.seq.muteA1:id==CmdMute2?project_.seq.muteA2:id==CmdMute3?project_.seq.muteA3:project_.seq.hideV2;
        val=!val;
        label=std::wstring(id==CmdMute1?L"A1 ":id==CmdMute2?L"A2 ":id==CmdMute3?L"A3 ":L"V2 ")+
            (id==CmdHideOverlay?(val?L"oculta":L"visível"):(val?L"silenciada":L"reativada"));
    } else if(id==CmdWorkIn || id==CmdWorkOut || id==CmdWorkClear) {
        if(id==CmdWorkClear) {
            project_.seq.workIn=project_.seq.workOut=-1; useRange_=false; label=L"Intervalo de exportação limpo";
        } else if(setWorkEdge(project_.seq,id==CmdWorkOut,head_)) {
            useRange_=true; label=(id==CmdWorkOut?L"Saída da exportação • ":L"Entrada da exportação • ")+timecode(head_);
        }
    } else if(id==CmdPaste || id==CmdDuplicate) {
        const Clip* c=id==CmdPaste?(hasClipboard_?&clipboard_:0):get(project_.seq,selected_);
        if(!c) return;
        Lane lane=id==CmdPaste?copiedLane_:selected_.lane;
        int frame=id==CmdPaste?head_:startOf(project_.seq,selected_)+c->len;
        int before=lane==Video?(id==CmdPaste?videoBoundary(project_.seq,frame):selected_.index+1):-1;
        Selection pasted=paste(project_.seq,lane,*c,frame,before);
        if(pasted.index>=0) { selected_=pasted; seek(startOf(project_.seq,pasted)); label=L"Clipe colocado na mesma faixa"; }
        else { status_=L"Sem espaço para este clipe"; redraw(); return; }
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
            if(selected_.lane!=Titles && selected_.lane!=Overlay && project_.assets[clip->asset].audio>=0) {
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
        case 'C':execute(CmdCopy);return true;
        case 'V':execute(CmdPaste);return true;
        case 'D':execute(CmdDuplicate);return true;
        default:return false;
        }
    }
    if(alt()) {
        if(key==VK_UP || key==VK_DOWN) { execute(key==VK_UP?CmdGainUp:CmdGainDown);return true; }
        if(key=='I' || key=='O' || key=='X') { execute(key=='I'?CmdWorkIn:key=='O'?CmdWorkOut:CmdWorkClear);return true; }
        return false;
    }
    switch(key) {
    case VK_SPACE:execute(CmdPlay);return true;
    case VK_HOME:execute(CmdFirst);return true;
    case VK_END:execute(CmdLast);return true;
    case VK_LEFT:execute(CmdPrev);return true;
    case VK_RIGHT:execute(CmdNext);return true;
    case VK_F2:execute(CmdProps);return true;
    case VK_F3:workspace(!assistMode_);return true;
    case VK_F4:startAnalysis();return true;
    case VK_F5:if(assistMode_) execute(CmdAiReview);return true;
    case VK_F6:execute(shift()?CmdAiPrevious:CmdAiNext);return true;
    case VK_F7:execute(CmdAiFace);return true;
    case VK_F8:execute(CmdEco);return true;
    case VK_PRIOR:execute(CmdMarkPrev);return true;
    case VK_NEXT:execute(CmdMarkNext);return true;
    case VK_OEM_COMMA:execute(CmdInsert);return true;
    case 'T':execute(CmdTitle);return true;
    case 'M':execute(CmdMarker);return true;
    case 'X':execute(CmdSourceClear);return true;
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
    for(int l=1;l<LaneCount;++l) {
        const auto& clips=laneOf(project_.seq,Lane(l));
        for(int i=0;i<int(clips.size());++i) if(l!=lane || i!=skip) { check(clips[i].pos); check(clips[i].pos+clips[i].len); }
    }
    for(const Marker& m:project_.seq.markers) check(m.frame);
    if(project_.seq.workIn>=0) check(project_.seq.workIn);
    if(project_.seq.workOut>=0) check(project_.seq.workOut);
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
int App::rowAt(int x,int y) const {
    for(int row=0;row<RowCount;++row) if(inside(ui_.lanes[row],x,y)) return row;
    return -1;
}
Lane App::dropLane(int asset,int row) const {
    if(row<0 || row==RowTitle || asset<0 || asset>=int(project_.assets.size())) return LaneCount;
    Lane lane=rowLane(row);
    const Asset& a=project_.assets[asset];
    return (lane==Video || lane==Overlay?a.video:a.audio)>=0?lane:LaneCount;
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
    if(!dbl && inside(ui_.gain,x,y)) { stop(); track(AdjustGain,x,y); drag_.moved=true; mouseMove(x,y); return; }
    if(!dbl && inside(ui_.scroll,x,y) && maxScroll()>0) {
        RECT thumb=scrollThumb(); track(Scrollbar,x,y);
        drag_.off=inside(thumb,x,y)?x-thumb.left:width(thumb)/2;
        scrollToPointer(x,drag_.off); return;
    }
    if(!dbl && ui_.dual && inside(ui_.sourceSeek,x,y) && asset_>=0) {
        focusMonitor(true); track(MonitorSeek,x,y); seekMonitor(monitorFrame(x)); return;
    }
    if(!dbl && inside(ui_.transportSeek,x,y)) {
        if(ui_.dual) focusMonitor(false);
        track(MonitorSeek,x,y); seekMonitor(monitorFrame(x)); return;
    }
    if(!dbl && ui_.dual && inside(ui_.sourceDisplay,x,y) && asset_>=0) { focusMonitor(true); return; }
    if(!dbl && ui_.dual && inside(ui_.display,x,y)) { focusMonitor(false); return; }
    if(!dbl && inside(ui_.ruler,x,y)) { track(Scrub,x,y); seek(frameAt(x),false);return; }
    int row=rowAt(x,y);
    if(row>=0) {
        int f=frameAt(x);
        Lane lane=rowLane(row);
        selected_=at(project_.seq,lane,f);
        if(selected_.index<0 && x>ui_.ruler.left) selected_=at(project_.seq,lane,std::max(0,f-1));
        const Clip* c=get(project_.seq,selected_);
        if(dbl) {
            seek(f,false);
            if(c) trackProperties();
            else if(row==RowTitle) trackProperties(true);
            redraw();return;
        }
        Grab g=c?MoveClip:Scrub;
        int start=startOf(project_.seq,selected_);
        if(c && px(start+c->len)-px(start)>16 && row!=RowLinked) {
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
    drag_.targetX=-1; drag_.targetRow=-1;
    if(drag_.type==PoolItem || drag_.type==MoveClip) {
        int row=rowAt(x,y);
        bool fromPool=drag_.type==PoolItem;
        Lane lane=fromPool?dropLane(drag_.asset,row):row>=0 && rowLane(row)==drag_.sel.lane?drag_.sel.lane:LaneCount;
        if(lane!=LaneCount) {
            drag_.targetRow=laneRow(lane);
            if(lane==Video) {
                int i=videoBoundary(project_.seq,frameAt(x));
                int f=i<int(project_.seq.video.size())?startOf(project_.seq,Selection(Video,i)):length(project_.seq,Video);
                drag_.targetX=px(f);
            } else {
                const Clip* c=get(project_.seq,drag_.sel);
                int len=c?c->len:1, skip=fromPool?-1:drag_.sel.index;
                if(fromPool) {
                    const SourceRange& r=sourceRanges_[drag_.asset];
                    len=(r.last<0?project_.assets[drag_.asset].frames:r.last)-std::max(0,r.first);
                }
                int t=freePosition(laneOf(project_.seq,lane),skip,snap(frameAt(x)-drag_.off,skip,lane),len);
                if(t>=0) drag_.targetX=px(t);
            }
        }
    } else if(drag_.type==LeftEdge || drag_.type==RightEdge) {
        if(const Clip* c=get(project_.seq,drag_.sel)) {
            int a=startOf(project_.seq,drag_.sel);
            int origin=drag_.type==LeftEdge?a:a+c->len;
            drag_.targetRow=laneRow(drag_.sel.lane);
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
    int row=rowAt(x,y);
    if(drag.type==PoolItem && drag.moved && row>=0) {
        Lane lane=dropLane(drag.asset,row);
        if(lane!=LaneCount) addAsset(drag.asset,snap(frameAt(x)),lane==Video?videoBoundary(project_.seq,frameAt(x)):-1,lane);
        else { status_=L"Use V1/V2 para imagem e A2/A3 para áudio"; redraw(); }
    } else if(drag.type==MoveClip && drag.moved && row>=0 && rowLane(row)==drag.sel.lane) {
        Sequence old=project_.seq;
        int target=drag.sel.lane==Video?videoBoundary(project_.seq,frameAt(x)):snap(frameAt(x)-drag.off,drag.sel.index,drag.sel.lane);
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
        else if(arg==L"--assist-demo") showDemo(true);
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
    case WM_CREATE: DragAcceptFiles(hwnd_,TRUE); SetTimer(hwnd_,3,45000,0); PostMessageW(hwnd_,WM_APP+1,0,0); updateTitle(); return 0;
    case WM_APP+1:openArgs();return 0;
    case WM_ERASEBKGND:return 1;
    case WM_PAINT:paint();return 0;
    case WM_PRINTCLIENT: {
        RECT client;
        GetClientRect(hwnd_,&client);
        if(w && width(client)>0 && height(client)>0) paintContent(reinterpret_cast<HDC>(w),client,client);
        return 0;
    }
    case WM_GETMINMAXINFO: {
        MINMAXINFO* m=reinterpret_cast<MINMAXINFO*>(l);
        RECT min=rect(0,0,750,488); AdjustWindowRect(&min,WS_OVERLAPPEDWINDOW,FALSE);
        m->ptMinTrackSize.x=width(min);m->ptMinTrackSize.y=height(min);return 0;
    }
    case WM_SIZE:if(w!=SIZE_MINIMIZED) layout(LOWORD(l),HIWORD(l));redraw();return 0;
    case WM_CONTEXTMENU:
        if(job_) return 0;
        if(GET_X_LPARAM(l)==-1 && GET_Y_LPARAM(l)==-1) { menu(CmdTools); return 0; }
        {
            POINT p={GET_X_LPARAM(l),GET_Y_LPARAM(l)}; ScreenToClient(hwnd_,&p);
            int row=rowAt(p.x,p.y);
            if(row>=0 || inside(ui_.ruler,p.x,p.y)) {
                int t=frameAt(p.x);
                if(row>=0) selected_=at(project_.seq,rowLane(row),t);
                seek(t,false); redraw(); menu(CmdTools,p.x,p.y);
            }
        }
        return 0;
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
    case WM_TIMER:if(w==1 && player_.active()) playbackTick();else if(w==2) pollJob();else if(w==3) autoSave();return 0;
    case WM_CLOSE:
        if(job_) {
            if(MessageBoxW(hwnd_,L"Cancelar a operação em andamento e fechar?",L"Nigh Editor",MB_YESNO|MB_ICONQUESTION)==IDYES) {
                if(job_) { closeWhenDone_=true; execute(CmdCancel); }
                else PostMessageW(hwnd_,WM_CLOSE,0,0);
            }
        } else if(confirmDiscard()) { removeRecovery(); DestroyWindow(hwnd_); }
        return 0;
    case WM_DESTROY:stop(); PostQuitMessage(0);return 0;
    }
    return DefWindowProcW(hwnd_,msg,w,l);
}
}
