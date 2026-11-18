#include "job.hpp"
#include <process.h>

namespace nigh {
namespace {
class CriticalLock {
    CRITICAL_SECTION* lock_;
public:
    explicit CriticalLock(CRITICAL_SECTION& lock) : lock_(&lock) { EnterCriticalSection(lock_); }
    ~CriticalLock() { LeaveCriticalSection(lock_); }
};
}
Job::Job(JobKind k,const std::wstring& tools,const std::wstring& dir) : thread_(0),
    cancel_(CreateEventW(0,TRUE,FALSE,0)),tools_(tools),scratch_(dir),kind(k),success(false),openedWithChanges(false),rejected(0) {
    InitializeCriticalSection(&lock_);
}
Job::~Job() {
    if(thread_) { cancel(); WaitForSingleObject(thread_,INFINITE); CloseHandle(thread_); }
    if(cancel_) CloseHandle(cancel_);
    DeleteCriticalSection(&lock_);
}
bool Job::start() {
    if(!cancel_ || thread_) return false;
    thread_=reinterpret_cast<HANDLE>(_beginthreadex(0,0,worker,this,0,0));
    return thread_!=0;
}
bool Job::done() const { return thread_ && WaitForSingleObject(thread_,0)==WAIT_OBJECT_0; }
JobProgress Job::progress() {
    CriticalLock guard(lock_);
    return progress_;
}
unsigned __stdcall Job::worker(void* data) {
    Job* job=static_cast<Job*>(data);
    if(job->kind==AssistJob) SetThreadPriority(GetCurrentThread(),THREAD_PRIORITY_BELOW_NORMAL);
    try { job->work(); }
    catch(...) { job->error=L"Não foi possível completar a operação. Verifique a memória e o espaço em disco."; job->success=false; }
    return 0;
}
void Job::work() {
    MediaContext ctx(tools_,scratch_,cancel_);
    ctx.progress=[this](int p,const std::wstring& label) {
        CriticalLock guard(lock_);
        progress_=JobProgress(p,label);
    };
    if(kind==ExportJob) { success=render(ctx,project,target,preset,error); return; }
    if(kind==AssistJob) {
        if(request.asset>=0 && request.asset<int(project.assets.size()))
            success=analyze(ctx,project.assets[request.asset],request.first,request.end,analysis,error);
        return;
    }
    if(kind==OpenJob) {
        for(const Asset& a:project.assets) paths.push_back(a.path);
        project.assets.clear();
    }
    for(size_t i=0;i<paths.size();++i) {
        if(ctx.stopped()) return;
        ctx.report(1+int(i*96/paths.size()),L"Criando prévia • "+baseName(paths[i]));
        Asset a;
        std::wstring why;
        if(importMedia(ctx,paths[i],a,why)) project.assets.push_back(std::move(a));
        else if(!ctx.stopped()) {
            if(error.empty()) error=baseName(paths[i])+L"\n\n"+why;
            ++rejected;
            if(kind!=ImportJob) return;
        }
    }
    if(kind==DemoJob) {
        project.name=L"Maré baixa";
        for(int i=0;i<3;++i) add(project,i,0);
        trim(project,Selection(Video,0),true,8);
        add(project,3,0);
        trim(project,Selection(Music,0),false,length(project.seq,Video));
        if(Clip* c=get(project.seq,Selection(Music,0))) { c->gain=82; c->fx.fadeIn=18; c->fx.fadeOut=32; }
        if(project.seq.video.size()>1) project.seq.video[1].fx.dissolve=16;
        Selection picture=place(project,2,Overlay,121,8,63);
        if(Clip* c=get(project.seq,picture)) { c->fx.contrast=111; c->fx.saturation=90; }
        Clip title(-1,0,66);
        title.title.text=L"MARÉ BAIXA"; title.title.color=1; title.title.size=8;
        paste(project.seq,Titles,title,6);
        title.title.text=L"A costa em camadas"; title.title.color=0; title.title.size=6; title.len=64;
        paste(project.seq,Titles,title,121);
        putMarker(project.seq,0,L"Abertura");
        putMarker(project.seq,104,L"Dissolução");
        putMarker(project.seq,121,L"Sobreposição");
    }
    if(kind==AssistDemoJob) {
        project.name=L"Caderno de campo";
        add(project,0,0);
        project.seq.format=FrameTall;
    }
    if(kind!=ImportJob && !valid(project,true)) {
        error=L"A duração ou o tipo de um arquivo original mudou. O projeto não foi alterado.";
        return;
    }
    success=!ctx.stopped() && (kind!=ImportJob || !project.assets.empty());
}
}
