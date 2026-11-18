#pragma once
#include "win.hpp"
#include <initializer_list>

namespace nigh {
inline COLORREF rgb(unsigned c) { return RGB((c>>16)&255,(c>>8)&255,c&255); }
inline RECT rect(int x,int y,int w,int h) { RECT r={x,y,x+w,y+h}; return r; }
inline int width(const RECT& r) { return r.right-r.left; }
inline int height(const RECT& r) { return r.bottom-r.top; }
inline int centerX(const RECT& r) { return (r.right+r.left)/2; }
inline int centerY(const RECT& r) { return (r.bottom+r.top)/2; }
inline bool inside(const RECT& r,int x,int y) { return x>=r.left && x<r.right && y>=r.top && y<r.bottom; }
inline RECT inset(RECT r,int x,int y) { InflateRect(&r,-x,-y); return r; }
inline int limit(int v,int lo,int hi) { return std::max(lo,std::min(v,hi)); }

namespace palette {
const unsigned Base=0x0f151d, Bar=0x141d27, Panel=0x19232e, Surface=0x222e3b, Raised=0x2a3b4a;
const unsigned Edge=0x2d3b4b, Soft=0x3b4c5d, Text=0xeef4f9, Muted=0xa9b7c6, Dim=0x8598ac;
const unsigned Quiet=0x637589, Screen=0x090f17, Accent=0x7ae1f7, AccentSoft=0x203b49, Ink=0x102b3a;
const unsigned Purple=0xc8b9fc, PurpleBg=0x352d50, Blue=0x8bb8d5, BlueBg=0x253543;
const unsigned Mint=0x8fd3bd, MintBg=0x223c37, Danger=0xf0a7a1;
}
enum Face { Small, Label, Body, Strong, Brand, Title, Mono, MonoLarge, FaceCount };
enum Glyph { NoGlyph, Logo, Folder, Disk, Plus, Minus, Export, ArrowDown, Film, MusicNote, Scissors,
    TrimLeft, TrimRight, Play, Pause, First, Last, Prev, Next, Undo, Redo, Bin, Speaker, MutedSpeaker,
    Bolt, Fit, Question, Cancel, Cursor, Keyboard, Check, Target, ListIcon, Spark };

class Fonts {
    HFONT f_[FaceCount];
public:
    Fonts();
    ~Fonts();
    HFONT operator[](Face n) const { return f_[n]; }
};
class Canvas {
    HDC dc_;
    const Fonts& fonts_;
public:
    Canvas(HDC dc,const Fonts& fs) : dc_(dc),fonts_(fs) { SetBkMode(dc_,TRANSPARENT); }
    HDC dc() const { return dc_; }
    void fill(const RECT& r,unsigned color);
    void border(const RECT& r,unsigned color);
    void box(const RECT& r,unsigned color,unsigned stroke,int radius=6);
    void dot(int x,int y,int r,unsigned color);
    void line(int x1,int y1,int x2,int y2,unsigned color,int weight=1);
    void text(const RECT& r,const std::wstring& s,Face f,unsigned color,UINT align=DT_LEFT);
    void paragraph(const RECT& r,const std::wstring& s,Face f,unsigned color,UINT align=DT_LEFT);
    int measure(const std::wstring& s,Face f);
    void icon(Glyph g,int x,int y,unsigned color,int size=16);
    void image(const RECT& r,const std::vector<uint8_t>& pixels,int w,int h);
};
}
