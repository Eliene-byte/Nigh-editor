#pragma once
#include "win.hpp"
#include <functional>

namespace nigh {
struct MediaContext {
    std::wstring tools, scratch;
    HANDLE cancel;
    std::function<void(int,const std::wstring&)> progress;
    MediaContext(const std::wstring& t,const std::wstring& s,HANDLE c=0) : tools(t),scratch(s),cancel(c) {}
    bool stopped() const { return cancel && WaitForSingleObject(cancel,0)==WAIT_OBJECT_0; }
    void report(int p,const std::wstring& text) const { if(progress) progress(p,text); }
};
struct RenderPreset {
    int width,height,first,last;
    bool audioOnly;
    RenderPreset(int w=640,int h=360) : width(w),height(h),first(0),last(-1),audioOnly(false) {}
};
bool codecsPresent(const std::wstring& tools);
bool importMedia(const MediaContext& ctx,const std::wstring& path,Asset& asset,std::wstring& error);
bool render(const MediaContext& ctx,const Project& p,const std::wstring& output,RenderPreset preset,std::wstring& error);

class FrameReader {
    std::wstring path_;
    File file_;
    std::vector<uint8_t> jpeg_;
public:
    void reset() { path_.clear(); file_.close(); }
    bool frame(const Proxy& proxy,int index,std::vector<uint8_t>& bgra);
};
struct Sound {
    std::wstring path;
    Clip clip;
    int fadeOffset, fadeLength;
    Sound(const std::wstring& p,const Clip& c,int offset=0,int full=0) :
        path(p),clip(c),fadeOffset(offset),fadeLength(full?full:c.len) {}
};
std::vector<Sound> previewSounds(const Project& p);
class Mixer {
    std::vector<Sound> sounds_;
    File files_[3];
    std::wstring paths_[3];
    int last_;
    std::vector<int32_t> sum_;
    std::vector<int16_t> input_;
    File& audioFile(const std::wstring& path);
public:
    Mixer() : last_(0) {}
    void set(std::vector<Sound> sounds);
    bool fill(int startSample,int count,int rate,int channels,int16_t* output);
};
}
