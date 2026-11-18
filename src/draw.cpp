#include "draw.hpp"
#include <cstring>

namespace nigh {
static int CALLBACK matchedFont(const LOGFONTW*,const TEXTMETRICW*,DWORD,LPARAM data) {
    *reinterpret_cast<bool*>(data)=true;
    return 0;
}
static const wchar_t* chooseFace(const wchar_t* preferred,const wchar_t* fallback) {
    HDC dc=GetDC(0);
    if(!dc) return fallback;
    LOGFONTW lf;
    std::memset(&lf,0,sizeof(lf));
    lf.lfCharSet=DEFAULT_CHARSET;
    lstrcpynW(lf.lfFaceName,preferred,LF_FACESIZE);
    bool found=false;
    EnumFontFamiliesExW(dc,&lf,matchedFont,reinterpret_cast<LPARAM>(&found),0);
    ReleaseDC(0,dc);
    return found?preferred:fallback;
}
Fonts::Fonts() {
    const wchar_t* ui=chooseFace(L"Segoe UI",L"Tahoma");
    const wchar_t* code=chooseFace(L"Consolas",L"Lucida Console");
    const int size[FaceCount]={11,11,13,13,17,24,11,16};
    const int weight[FaceCount]={FW_NORMAL,FW_BOLD,FW_NORMAL,FW_BOLD,FW_BOLD,FW_BOLD,FW_NORMAL,FW_NORMAL};
    for(int i=0;i<FaceCount;++i) f_[i]=CreateFontW(-size[i],0,0,0,weight[i],0,0,0,DEFAULT_CHARSET,
        OUT_DEFAULT_PRECIS,CLIP_DEFAULT_PRECIS,ANTIALIASED_QUALITY,DEFAULT_PITCH,i>=Mono?code:ui);
}
Fonts::~Fonts() { for(HFONT f:f_) if(f) DeleteObject(f); }
void Canvas::fill(const RECT& r,unsigned color) {
    if(r.right<=r.left || r.bottom<=r.top) return;
    SetDCBrushColor(dc_,rgb(color));
    FillRect(dc_,&r,static_cast<HBRUSH>(GetStockObject(DC_BRUSH)));
}
void Canvas::border(const RECT& r,unsigned color) {
    if(width(r)<2 || height(r)<2) return;
    fill(rect(r.left,r.top,width(r),1),color);
    fill(rect(r.left,r.bottom-1,width(r),1),color);
    fill(rect(r.left,r.top,1,height(r)),color);
    fill(rect(r.right-1,r.top,1,height(r)),color);
}
void Canvas::box(const RECT& r,unsigned color,unsigned stroke,int radius) {
    if(width(r)<3 || height(r)<3) { fill(r,color); return; }
    HGDIOBJ brush=SelectObject(dc_,GetStockObject(DC_BRUSH)), pen=SelectObject(dc_,GetStockObject(DC_PEN));
    SetDCBrushColor(dc_,rgb(color)); SetDCPenColor(dc_,rgb(stroke));
    RoundRect(dc_,r.left,r.top,r.right,r.bottom,radius,radius);
    SelectObject(dc_,brush); SelectObject(dc_,pen);
}
void Canvas::dot(int x,int y,int radius,unsigned color) {
    HGDIOBJ b=SelectObject(dc_,GetStockObject(DC_BRUSH)), p=SelectObject(dc_,GetStockObject(NULL_PEN));
    SetDCBrushColor(dc_,rgb(color));
    Ellipse(dc_,x-radius,y-radius,x+radius+1,y+radius+1);
    SelectObject(dc_,b); SelectObject(dc_,p);
}
void Canvas::line(int x1,int y1,int x2,int y2,unsigned color,int weight) {
    HPEN custom=weight>1?CreatePen(PS_SOLID,weight,rgb(color)):0;
    HGDIOBJ p=SelectObject(dc_,custom?custom:GetStockObject(DC_PEN));
    if(!custom) SetDCPenColor(dc_,rgb(color));
    MoveToEx(dc_,x1,y1,0); LineTo(dc_,x2,y2);
    SelectObject(dc_,p);
    if(custom) DeleteObject(custom);
}
void Canvas::text(const RECT& r,const std::wstring& s,Face f,unsigned color,UINT align) {
    if(s.empty() || width(r)<=0 || height(r)<=0) return;
    HGDIOBJ old=SelectObject(dc_,fonts_[f]);
    SetTextColor(dc_,rgb(color));
    RECT copy=r;
    DrawTextW(dc_,s.c_str(),int(s.size()),&copy,DT_SINGLELINE|DT_VCENTER|DT_END_ELLIPSIS|DT_NOPREFIX|align);
    SelectObject(dc_,old);
}
void Canvas::paragraph(const RECT& r,const std::wstring& s,Face f,unsigned color,UINT align) {
    if(s.empty() || width(r)<=0 || height(r)<=0) return;
    HGDIOBJ old=SelectObject(dc_,fonts_[f]);
    SetTextColor(dc_,rgb(color));
    RECT copy=r;
    DrawTextW(dc_,s.c_str(),int(s.size()),&copy,DT_WORDBREAK|DT_NOPREFIX|align);
    SelectObject(dc_,old);
}
int Canvas::measure(const std::wstring& s,Face f) {
    HGDIOBJ old=SelectObject(dc_,fonts_[f]);
    SIZE size={0,0};
    GetTextExtentPoint32W(dc_,s.data(),int(s.size()),&size);
    SelectObject(dc_,old);
    return int(size.cx);
}
void Canvas::icon(Glyph g,int x,int y,unsigned col,int size) {
    if(g==NoGlyph) return;
    const auto px=[x,size](int v){ return x+(v*size+8)/16; };
    const auto py=[y,size](int v){ return y+(v*size+8)/16; };
    const auto l=[&](int a,int b,int c,int d){ line(px(a),py(b),px(c),py(d),col,size>=24?2:1); };
    const auto r=[&](int a,int b,int c,int d){ border(rect(px(a),py(b),px(c)-px(a)+1,py(d)-py(b)+1),col); };
    const auto p=[&](std::initializer_list<POINT> pts) {
        POINT arr[12]; int i=0;
        for(POINT v:pts) { arr[i].x=px(int(v.x));arr[i].y=py(int(v.y));++i; }
        HGDIOBJ brush=SelectObject(dc_,GetStockObject(DC_BRUSH)), pen=SelectObject(dc_,GetStockObject(NULL_PEN));
        SetDCBrushColor(dc_,rgb(col)); Polygon(dc_,arr,i);
        SelectObject(dc_,pen); SelectObject(dc_,brush);
    };
    switch(g) {
    case Spark: p({{8,1},{10,6},{15,8},{10,10},{8,15},{6,10},{1,8},{6,6}});break;
    case Logo: p({{1,2},{4,2},{11,9},{11,3},{15,1},{15,14},{12,14},{5,7},{5,13},{1,15}}); break;
    case Folder: l(1,5,6,5);l(6,5,8,7);l(8,7,15,7);l(15,7,15,14);l(15,14,1,14);l(1,14,1,5);break;
    case Disk: r(2,2,14,14);r(5,2,11,6);r(5,9,11,14);break;
    case Plus: l(2,8,15,8);l(8,2,8,15);break;
    case Minus: l(2,8,15,8);break;
    case Export: l(3,12,12,3);l(6,3,13,3);l(12,3,12,10);l(2,15,15,15);break;
    case ArrowDown: l(4,6,8,10);l(8,10,12,6);break;
    case Film: r(1,2,15,14);l(4,2,4,15);l(12,2,12,15);l(1,6,4,6);l(1,10,4,10);l(12,6,16,6);l(12,10,16,10);break;
    case MusicNote: l(6,12,6,3);l(13,3,13,11);l(6,3,14,2);l(6,6,14,5);dot(px(4),py(12),std::max(1,size/8),col);dot(px(11),py(11),std::max(1,size/8),col);break;
    case Scissors: r(1,2,5,5);r(1,11,5,14);l(5,5,14,13);l(5,11,14,3);break;
    case TrimLeft: l(3,2,3,15);l(3,2,7,2);l(3,14,7,14);l(8,8,14,8);l(8,8,11,5);l(8,8,11,11);break;
    case TrimRight: l(13,2,13,15);l(9,2,14,2);l(9,14,14,14);l(2,8,8,8);l(8,8,5,5);l(8,8,5,11);break;
    case Play: p({{4,2},{14,8},{4,14}});break;
    case Pause: p({{4,3},{7,3},{7,13},{4,13}});p({{9,3},{12,3},{12,13},{9,13}});break;
    case First: l(3,3,3,14);p({{6,8},{13,3},{13,13}});break;
    case Last: l(13,3,13,14);p({{10,8},{3,3},{3,13}});break;
    case Prev: l(11,3,6,8);l(6,8,11,13);break;
    case Next: l(5,3,10,8);l(10,8,5,13);break;
    case Undo: l(7,3,3,7);l(3,7,7,11);l(3,7,11,7);l(11,7,14,10);l(14,10,14,14);break;
    case Redo: l(9,3,13,7);l(13,7,9,11);l(13,7,5,7);l(5,7,2,10);l(2,10,2,14);break;
    case Bin: l(2,4,15,4);l(6,2,11,2);l(4,5,5,14);l(5,14,12,14);l(12,14,13,5);l(8,7,8,12);break;
    case Speaker: case MutedSpeaker:
        p({{1,6},{4,6},{8,3},{8,13},{4,10},{1,10}});
        if(g==Speaker) { l(10,5,13,8);l(13,8,10,11);l(12,3,16,8);l(16,8,12,13); }
        else { l(10,5,15,11);l(15,5,10,11); }break;
    case Bolt: p({{9,0},{3,9},{7,9},{6,16},{14,6},{9,6}});break;
    case Fit: l(2,7,2,2);l(2,2,7,2);l(9,2,14,2);l(14,2,14,7);l(14,9,14,14);l(14,14,9,14);l(7,14,2,14);l(2,14,2,9);break;
    case Question: l(4,5,5,2);l(5,2,11,2);l(11,2,13,5);l(13,5,8,9);l(8,9,8,11);dot(px(8),py(14),1,col);break;
    case Cancel: l(3,3,13,13);l(13,3,3,13);break;
    case Cursor: p({{3,1},{13,10},{8,10},{7,15}});break;
    case Keyboard: r(1,4,15,12);l(4,7,5,7);l(7,7,8,7);l(10,7,11,7);l(5,10,12,10);break;
    case Check: l(2,8,6,12);l(6,12,14,4);break;
    case Target: r(6,6,10,10);l(1,8,5,8);l(11,8,15,8);l(8,1,8,5);l(8,11,8,15);break;
    case ListIcon: l(3,4,15,4);l(3,8,15,8);l(3,12,15,12);break;
    default: break;
    }
}
void Canvas::image(const RECT& r,const std::vector<uint8_t>& pixels,int w,int h) {
    if(pixels.empty() || width(r)<=0 || height(r)<=0 || !RectVisible(dc_,&r)) return;
    BITMAPINFO bi;
    std::memset(&bi,0,sizeof(bi));
    bi.bmiHeader.biSize=sizeof(BITMAPINFOHEADER);
    bi.bmiHeader.biWidth=w;
    bi.bmiHeader.biHeight=-h;
    bi.bmiHeader.biPlanes=1;
    bi.bmiHeader.biBitCount=32;
    bi.bmiHeader.biCompression=BI_RGB;
    SetStretchBltMode(dc_,COLORONCOLOR);
    StretchDIBits(dc_,r.left,r.top,width(r),height(r),0,0,w,h,pixels.data(),&bi,DIB_RGB_COLORS,SRCCOPY);
}
}
