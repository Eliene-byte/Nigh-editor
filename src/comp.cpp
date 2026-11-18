#include "comp.hpp"
#include <cstring>

namespace nigh {
static int clamp(int n,int lo,int hi) { return std::min(hi,std::max(lo,n)); }
int saturationWeight(int in,int out,int sat) {
    const int luma[]={213,715,72}; // R, G, B: neutral coefficients sum to one
    return (luma[in]*(100-sat))/100+(in==out?sat*10:0);
}

bool rasterTitle(const TitleStyle& t,int w,int h,std::vector<uint8_t>& out) {
    BITMAPINFO bi;
    std::memset(&bi,0,sizeof(bi));
    bi.bmiHeader.biSize=sizeof(BITMAPINFOHEADER);
    bi.bmiHeader.biWidth=w; bi.bmiHeader.biHeight=-h;
    bi.bmiHeader.biPlanes=1; bi.bmiHeader.biBitCount=32; bi.bmiHeader.biCompression=BI_RGB;
    out.resize(size_t(w)*h*4);
    void* data=0;
    HDC dc=CreateCompatibleDC(0);
    HBITMAP bmp=dc?CreateDIBSection(dc,&bi,DIB_RGB_COLORS,&data,0,0):0;
    if(!bmp || !data) {
        if(bmp) DeleteObject(bmp);
        if(dc) DeleteDC(dc);
        return false;
    }
    std::memset(data,0,out.size());
    HGDIOBJ prev=SelectObject(dc,bmp);
    int px=std::max(7,h*t.size/100);
    HFONT font=CreateFontW(-px,0,0,0,FW_BOLD,FALSE,FALSE,FALSE,DEFAULT_CHARSET,OUT_DEFAULT_PRECIS,
        CLIP_DEFAULT_PRECIS,ANTIALIASED_QUALITY,DEFAULT_PITCH|FF_SWISS,L"Arial");
    HGDIOBJ oldFont=font?SelectObject(dc,font):0;
    SetBkMode(dc,TRANSPARENT); SetTextColor(dc,RGB(255,255,255));
    UINT flags=DT_CENTER|DT_WORDBREAK|DT_NOPREFIX|DT_EDITCONTROL;
    int margin=w*8/100, inset=std::max(2,px/3);
    RECT measured={0,0,w-2*margin,h};
    DrawTextW(dc,t.text.c_str(),int(t.text.size()),&measured,flags|DT_CALCRECT);
    int body=clamp(measured.bottom,px,7*h/10);
    int y=t.position==1?(h-body)/2:t.position==2?h/12:h-h/12-body;
    RECT line={margin,y,w-margin,y+body};
    DrawTextW(dc,t.text.c_str(),int(t.text.size()),&line,flags);
    GdiFlush();
    const uint8_t* raw=static_cast<const uint8_t*>(data);
    int x0=w,x1=0,y0=h,y1=0;
    for(int row=y;row<y+body;++row) for(int x=margin;x<w-margin;++x) if(raw[(row*w+x)*4]) {
        x0=std::min(x0,x); x1=std::max(x1,x+1); y0=std::min(y0,row); y1=std::max(y1,row+1);
    }
    const unsigned colors[]={0xffffff,0xffdd75,0xbdeaff,0xc9faa6};
    unsigned rgb=colors[clamp(t.color,0,3)];
    bool panel=t.box && x0<x1;
    x0-=inset; x1+=inset; y0-=inset; y1+=inset;
    for(int row=0;row<h;++row) for(int x=0;x<w;++x) {
        int i=(row*w+x)*4, g=raw[i];
        int b=panel && x>=x0 && x<x1 && row>=y0 && row<y1?168:0;
        int a=g+(255-g)*b/255;
        for(int k=0;k<3;++k) out[i+k]=a?uint8_t(int((rgb>>(k*8))&255)*g/a):0;
        out[i+3]=uint8_t(a);
    }
    if(font) { SelectObject(dc,oldFont); DeleteObject(font); }
    SelectObject(dc,prev); DeleteObject(bmp); DeleteDC(dc);
    return true;
}
const std::vector<uint8_t>* Preview::Cached::read(const Asset& a,int t) {
    if(!a.proxy) return 0;
    const Proxy& p=*a.proxy;
    if(a.still) return p.still.empty()?0:&p.still;
    if(p.frames.empty()) return 0;
    int n=clamp(t/(Fps/ProxyFps),0,int(p.frames.size())-1);
    if(proxy!=&p || index!=n) {
        proxy=&p; index=n;
        if(!reader.frame(p,n,pixels)) pixels.clear();
    }
    return pixels.empty()?0:&pixels;
}
void Preview::reset() {
    base_.reset(); previous_.reset(); overlay_.reset(); title_.clear(); temp_.clear();
}
static void alphaOver(std::vector<uint8_t>& out,const std::vector<uint8_t>& src) {
    for(size_t i=0;i<out.size();i+=4) {
        int a=src[i+3];
        if(a) for(int k=0;k<3;++k) out[i+k]=uint8_t((out[i+k]*(255-a)+src[i+k]*a+127)/255);
    }
}
static void drawLayer(std::vector<uint8_t>& out,const std::vector<uint8_t>* src,bool still,const Effects& e,bool replace,FrameFormat format) {
    int width=previewWidth(format);
    if(format==FrameWide && replace && src && !still && e.scale==100 && !e.x && !e.y && e.opacity==100 && !e.color()) {
        out=*src;
        return;
    }
    if(replace) out.assign(width*ProxyH*4,0);
    if(!src || !e.opacity) return;
    PictureBox b=outputBox(e,width,ProxyH,format);
    int left=std::max(0,b.x),top=std::max(0,b.y),right=std::min(width,b.x+b.w),bottom=std::min(ProxyH,b.y+b.h);
    if(left>=right || top>=bottom) return;
    int columns[ProxyW], matrix[3][3];
    uint8_t lut[256];
    bool color=e.color(), sat=e.saturation!=100;
    for(int x=left;x<right;++x) columns[x]=(x-b.x)*ProxyW/b.w*4;
    if(color) {
        for(int j=0;j<3;++j) for(int i=0;i<3;++i) matrix[j][i]=saturationWeight(2-i,2-j,e.saturation);
        for(int i=0;i<256;++i) lut[i]=uint8_t(clamp(((i-128)*e.contrast+12800+e.brightness*100)/100,0,255));
    }
    for(int y=top;y<bottom;++y) {
        const uint8_t* row=src->data()+(y-b.y)*ProxyH/b.h*ProxyW*4;
        uint8_t* dest=out.data()+(y*width+left)*4;
        for(int x=left;x<right;++x,dest+=4) {
            const uint8_t* p=row+columns[x];
            int a=still?p[3]*e.opacity/100:255*e.opacity/100;
            if(a==0) continue;
            for(int k=0;k<3;++k) {
                int val=p[k];
                if(color) {
                    if(sat) val=clamp((matrix[k][0]*p[0]+matrix[k][1]*p[1]+matrix[k][2]*p[2]+500)/1000,0,255);
                    val=lut[val];
                }
                dest[k]=a==255?uint8_t(val):uint8_t((val*a+dest[k]*(255-a)+127)/255);
            }
        }
    }
}
bool Preview::frame(const Project& p,int t,int source,std::vector<uint8_t>& pixels) {
    if(source>=0) {
        const Asset& a=p.assets[source];
        auto src=base_.read(a,t);
        if(!src) return false;
        drawLayer(pixels,src,a.still,Effects(),true,FrameWide);
        return true;
    }
    if(length(p.seq)==0) return false;
    FrameFormat format=p.seq.format;
    int width=previewWidth(format);
    Scene scene=sceneAt(p.seq,t);
    const auto draw=[&](const Clip* c,int f,Cached& cache,std::vector<uint8_t>& out,bool base) {
        if(!c) { if(base) out.assign(width*ProxyH*4,0); return; }
        const Asset& a=p.assets[c->asset];
        drawLayer(out,cache.read(a,std::min(a.frames-1,f)),a.still,c->fx,base,format);
    };
    draw(scene.main,scene.main?scene.main->in+scene.mainOffset:0,base_,pixels,true);
    if(scene.dissolve && scene.mainOffset<scene.dissolve-1) {
        const Clip* a=scene.previous;
        draw(a,a?a->in+a->len+scene.mainOffset:0,previous_,temp_,true);
        int alpha=scene.mainOffset*256/(scene.dissolve-1);
        for(size_t i=0;i<pixels.size();i+=4) for(int k=0;k<3;++k)
            pixels[i+k]=uint8_t((pixels[i+k]*alpha+temp_[i+k]*(256-alpha)+128)>>8);
    }
    draw(scene.overlay,scene.overlay?scene.overlay->in+scene.overlayOffset:0,overlay_,pixels,false);
    if(scene.title) {
        if(title_.empty() || titleWidth_!=width || !(lastTitle_==scene.title->title)) {
            lastTitle_=scene.title->title;
            titleWidth_=width;
            if(!rasterTitle(lastTitle_,width,ProxyH,title_)) title_.clear();
        }
        if(!title_.empty()) alphaOver(pixels,title_);
    }
    return true;
}
}
