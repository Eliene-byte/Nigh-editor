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
const int MaxMarkers = 128;
const int MaxTitle = 240;
const int MaxMarkerText = 80;
const int MaxFade = Fps*5;
const int MaxDissolve = Fps*3;

struct FrameRef {
    uint32_t offset, size;
    FrameRef(uint32_t o, uint32_t s) : offset(o), size(s) {}
};
struct Proxy {
    std::wstring avi, pcm;
    std::vector<FrameRef> frames;
    std::vector<uint8_t> peaks;
    std::vector<uint8_t> thumb; // 88x50, BGRX
    std::vector<uint8_t> still; // one 320x180 BGRA image, optional transparency
};
struct Asset {
    std::wstring path, name;
    int frames, video, audio;
    bool still;
    std::shared_ptr<Proxy> proxy;
    Asset() : frames(0), video(-1), audio(-1), still(false) {}
};
struct Effects {
    int scale, x, y, opacity, brightness, contrast, saturation, dissolve;
    int pan, fadeIn, fadeOut;
    Effects() : scale(100),x(0),y(0),opacity(100),brightness(0),contrast(100),saturation(100),dissolve(0),pan(0),fadeIn(0),fadeOut(0) {}
    bool operator==(const Effects& e) const;
    bool color() const { return brightness || contrast!=100 || saturation!=100; }
};
struct TitleStyle {
    std::wstring text;
    int position, size, color; // 0 bottom / 1 center / 2 top; percent of height; palette index
    bool box;
    TitleStyle() : position(0),size(6),color(0),box(true) {}
    bool operator==(const TitleStyle& o) const;
};
struct Clip {
    int asset, in, len, pos, gain;
    Effects fx;
    TitleStyle title;
    Clip(int a=0, int i=0, int l=0, int p=0, int g=100) : asset(a), in(i), len(l), pos(p), gain(g) {}
    bool operator==(const Clip& c) const;
};
enum Lane { Video = 0, Music, Overlay, Voice, Titles, LaneCount };
struct Selection {
    Lane lane;
    int index;
    Selection(Lane l=Video, int i=-1) : lane(l), index(i) {}
};
struct Marker {
    int frame;
    std::wstring text;
    Marker(int f,const std::wstring& s) : frame(f),text(s) {}
    bool operator==(const Marker& m) const { return frame==m.frame && text==m.text; }
};
enum FrameFormat { FrameWide=0, FrameSquare, FrameTall, FrameFormats };
struct FrameSize { int w,h; };
FrameSize outputSize(FrameFormat format,int quality); // 0 = SD, 1 = 480p, 2 = HD
int previewWidth(FrameFormat format);
const wchar_t* formatName(FrameFormat format);

struct Sequence {
    std::vector<Clip> video, music, overlay, voice, titles;
    std::vector<Marker> markers;
    bool muteA1, muteA2, muteA3, hideV2;
    FrameFormat format;
    int workIn, workOut; // half-open boundaries, -1 = unset
    Sequence() : muteA1(false),muteA2(false),muteA3(false),hideV2(false),format(FrameWide),workIn(-1),workOut(-1) {}
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
int freePosition(const std::vector<Clip>& lane, int skip, int desired, int duration);
Selection paste(Sequence& seq, Lane lane, Clip clip, int at, int beforeVideo=-1);
Selection place(Project& p, int asset, Lane lane, int at, int in, int duration, int beforeVideo=-1);
Selection add(Project& p, int asset, int at, int beforeVideo=-1);
Selection insertVideo(Project& p, int asset, int at, int in, int duration);
bool split(Sequence& seq, Selection sel, int frame);
bool erase(Sequence& seq, Selection sel);
int videoBoundary(const Sequence& seq, int frame);
Selection move(Sequence& seq, Selection sel, int target);
// Clip edge positions are source frames (virtual, unbounded source for titles and stills).
bool trim(Project& p, Selection sel, bool left, int sourceFrame);
bool putMarker(Sequence& seq, int frame, const std::wstring& text);
bool setWorkEdge(Sequence& seq, bool out, int frame);
bool valid(const Project& p, bool checkMedia);

int dissolveLength(const Clip& clip);
struct Scene {
    const Clip *main, *previous, *overlay, *title;
    int mainOffset, overlayOffset, dissolve;
    Scene() : main(0),previous(0),overlay(0),title(0),mainOffset(0),overlayOffset(0),dissolve(0) {}
};
Scene sceneAt(const Sequence& seq,int frame);
std::vector<int> sceneCuts(const Sequence& seq,int first,int end);
struct PictureBox { int x,y,w,h; };
PictureBox pictureBox(const Effects& fx,int width,int height);
PictureBox outputBox(const Effects& fx,int width,int height,FrameFormat format);

std::wstring timecode(int frame);
std::wstring number(int value);
std::wstring seconds(int frame); // fixed decimal, not locale-dependent
}
