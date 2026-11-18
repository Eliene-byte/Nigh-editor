#pragma once
#include "media.hpp"

namespace nigh {
enum JobKind { ImportJob, OpenJob, DemoJob, ExportJob };
struct JobProgress {
    int percent;
    std::wstring label;
    JobProgress(int p=0,const std::wstring& s=L"Preparando…") : percent(p),label(s) {}
};
class Job {
    HANDLE thread_,cancel_;
    CRITICAL_SECTION lock_;
    JobProgress progress_;
    std::wstring tools_,scratch_;
    static unsigned __stdcall worker(void* data);
    void work();
public:
    JobKind kind;
    std::vector<std::wstring> paths;
    std::wstring target,error;
    Project project;
    RenderPreset preset;
    bool success;
    int rejected;
    Job(JobKind k,const std::wstring& tools,const std::wstring& scratch);
    ~Job();
    Job(const Job&) = delete;
    bool start();
    bool done() const;
    void cancel() { if(cancel_) SetEvent(cancel_); }
    bool canceled() const { return cancel_ && WaitForSingleObject(cancel_,0)==WAIT_OBJECT_0; }
    JobProgress progress();
};
}
