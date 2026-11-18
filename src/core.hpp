#pragma once
#include <algorithm>
#include <memory>
#include <string>
#include <vector>
#include <stdint.h>

namespace nigh {
const int Fps = 24;
const int ProxyFps = 12;
const int ProxyW = 320;
const int ProxyH = 180;
const int PreviewRate = 24000;
const int RenderRate = 48000;
const int MaxFrames = Fps * 60 * 60 * 2;
const int MaxAssets = 128;
const int MaxClips = 256;

struct FrameRef {
    uint32_t offset, size;
    FrameRef(uint32_t o, uint32_t s) : offset(o), size(s) {}
};
struct Proxy {
    std::wstring avi, pcm;
    std::vector<FrameRef> frames;
    std::vector<uint8_t> peaks;
    std::vector<uint8_t> thumb; // 88x50, BGRX
};
struct Asset {
    std::wstring path, name;
    int frames, video, audio;
    std::shared_ptr<Proxy> proxy;
    Asset() : frames(0), video(-1), audio(-1) {}
};
struct Clip {
    int asset, in, len, pos, gain;
    Clip(int a=0, int i=0, int l=0, int p=0, int g=100) : asset(a), in(i), len(l), pos(p), gain(g) {}
    bool operator==(const Clip& c) const;
};
enum Lane { Video = 0, Music = 1 };
struct Selection {
    Lane lane;
    int index;
    Selection(Lane l=Video, int i=-1) : lane(l), index(i) {}
};
struct Sequence {
    std::vector<Clip> video, music;
    bool muteA1, muteA2;
    Sequence() : muteA1(false), muteA2(false) {}
    bool operator==(const Sequence& seq) const;
};
struct Project {
    std::wstring name, file;
    std::vector<Asset> assets;
    Sequence seq;
    Project() : name(L"Sem título") {}
};

int length(const Sequence& seq, Lane lane);
int length(const Sequence& seq);
int startOf(const Sequence& seq, Selection sel);
Selection at(const Sequence& seq, Lane lane, int frame);
std::vector<Clip>& laneOf(Sequence& seq, Lane lane);
const std::vector<Clip>& laneOf(const Sequence& seq, Lane lane);
Clip* get(Sequence& seq, Selection sel);
const Clip* get(const Sequence& seq, Selection sel);
int freePosition(const std::vector<Clip>& music, int skip, int desired, int duration);
Selection add(Project& p, int asset, int at, int beforeVideo=-1);
bool split(Sequence& seq, Selection sel, int frame);
bool erase(Sequence& seq, Selection sel);
int videoBoundary(const Sequence& seq, int frame);
Selection move(Sequence& seq, Selection sel, int target);
// Clip edge positions are source frames; volume and original asset are unchanged.
bool trim(Project& p, Selection sel, bool left, int sourceFrame);
bool valid(const Project& p, bool checkMedia);

std::wstring timecode(int frame);
std::wstring number(int value);
std::wstring seconds(int frame); // fixed decimal, not locale-dependent
}
