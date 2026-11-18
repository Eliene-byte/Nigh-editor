#include "core.hpp"
#include <cstdlib>

namespace nigh {

bool Effects::operator==(const Effects& e) const {
    return scale==e.scale && x==e.x && y==e.y && opacity==e.opacity && brightness==e.brightness && contrast==e.contrast &&
        saturation==e.saturation && dissolve==e.dissolve && pan==e.pan && fadeIn==e.fadeIn && fadeOut==e.fadeOut;
}
bool TitleStyle::operator==(const TitleStyle& o) const { return text==o.text && position==o.position && size==o.size && color==o.color && box==o.box; }
bool Clip::operator==(const Clip& c) const {
    return asset==c.asset && in==c.in && len==c.len && pos==c.pos && gain==c.gain && fx==c.fx && title==c.title;
}
bool Sequence::operator==(const Sequence& s) const {
    return muteA1==s.muteA1 && muteA2==s.muteA2 && muteA3==s.muteA3 && hideV2==s.hideV2 && workIn==s.workIn && workOut==s.workOut &&
        video==s.video && music==s.music && overlay==s.overlay && voice==s.voice && titles==s.titles && markers==s.markers;
}
std::vector<Clip>& laneOf(Sequence& s,Lane l) {
    switch(l) {
    case Music:return s.music;
    case Overlay:return s.overlay;
    case Voice:return s.voice;
    case Titles:return s.titles;
    default:return s.video;
    }
}
const std::vector<Clip>& laneOf(const Sequence& s,Lane l) {
    switch(l) {
    case Music:return s.music;
    case Overlay:return s.overlay;
    case Voice:return s.voice;
    case Titles:return s.titles;
    default:return s.video;
    }
}
Clip* get(Sequence& s, Selection sel) {
    std::vector<Clip>& v=laneOf(s,sel.lane);
    return sel.index>=0 && sel.index<int(v.size()) ? &v[sel.index] : 0;
}
const Clip* get(const Sequence& s, Selection sel) {
    const std::vector<Clip>& v=laneOf(s,sel.lane);
    return sel.index>=0 && sel.index<int(v.size()) ? &v[sel.index] : 0;
}
int length(const Sequence& s,Lane l) {
    const auto& v=laneOf(s,l);
    if(l!=Video) return v.empty()?0:v.back().pos+v.back().len;
    int n=0;
    for(const Clip& c:v) n+=c.len;
    return n;
}
int length(const Sequence& s) {
    int n=0;
    for(int l=0;l<LaneCount;++l) n=std::max(n,length(s,Lane(l)));
    return n;
}
int startOf(const Sequence& s, Selection sel) {
    const Clip* clip=get(s,sel);
    if(!clip) return -1;
    if(sel.lane!=Video) return clip->pos;
    int t=0;
    for(int i=0;i<sel.index;++i) t+=s.video[i].len;
    return t;
}
Selection at(const Sequence& s,Lane l,int t) {
    const auto& v=laneOf(s,l);
    if(l!=Video) {
        auto i=std::upper_bound(v.begin(),v.end(),t,[](int frame,const Clip& c){return frame<c.pos;});
        if(i==v.begin() || t>=((i-1)->pos+(i-1)->len)) return Selection(l);
        return Selection(l,int(i-v.begin())-1);
    }
    if(t<0) return Selection(l);
    for(int i=0;i<int(v.size());++i) {
        if(t<v[i].len) return Selection(l,i);
        t-=v[i].len;
    }
    return Selection(l);
}
int freePosition(const std::vector<Clip>& v,int skip,int desired,int duration) {
    if(duration<=0 || duration>MaxFrames) return -1;
    desired=std::max(0,std::min(desired,MaxFrames-duration));
    int last=0,best=-1,distance=MaxFrames+1;
    for(int i=0;i<=int(v.size());++i) {
        if(i==skip) continue;
        int end=i==int(v.size())?MaxFrames:v[i].pos;
        if(end-last>=duration) {
            int p=std::max(last,std::min(desired,end-duration));
            if(std::abs(p-desired)<distance) { best=p; distance=std::abs(p-desired); }
        }
        if(i<int(v.size())) last=v[i].pos+v[i].len;
    }
    return best;
}
Selection paste(Sequence& s,Lane lane,Clip c,int t,int before) {
    auto& v=laneOf(s,lane);
    if(int(v.size())>=MaxClips || c.len<=0 || c.len>MaxFrames) return Selection(lane);
    int i;
    if(lane==Video) {
        if(c.len>MaxFrames-length(s,Video)) return Selection(lane);
        c.pos=0;
        i=before<0?int(v.size()):std::min(before,int(v.size()));
    } else {
        c.pos=freePosition(v,-1,t,c.len);
        if(c.pos<0) return Selection(lane);
        auto it=std::lower_bound(v.begin(),v.end(),c.pos,[](const Clip& e,int pos){return e.pos<pos;});
        i=int(it-v.begin());
    }
    v.insert(v.begin()+i,std::move(c));
    return Selection(lane,i);
}
Selection place(Project& p,int asset,Lane lane,int t,int in,int len,int before) {
    if(asset<0 || asset>=int(p.assets.size()) || lane==Titles) return Selection(lane);
    const Asset& a=p.assets[asset];
    bool picture=lane==Video || lane==Overlay;
    int cap=a.still?MaxFrames:a.frames;
    if((picture?a.video:a.audio)<0 || in<0 || len<=0 || len>cap || in>cap-len) return Selection(lane);
    Clip c(asset,in,len);
    if(lane==Overlay) { c.fx.scale=42; c.fx.x=51; c.fx.y=-50; }
    return paste(p.seq,lane,c,t,before);
}
Selection add(Project& p,int asset,int t,int before) {
    if(asset<0 || asset>=int(p.assets.size())) return Selection();
    const Asset& a=p.assets[asset];
    return place(p,asset,a.video>=0?Video:Music,t,0,a.frames,before);
}
Selection insertVideo(Project& p,int asset,int t,int in,int len) {
    if(asset<0 || asset>=int(p.assets.size()) || len<=0 || len>MaxFrames-length(p.seq,Video)) return Selection();
    int i=int(p.seq.video.size());
    if(i>=MaxClips) return Selection();
    const Asset& a=p.assets[asset];
    int cap=a.still?MaxFrames:a.frames;
    if(a.video<0 || in<0 || len>cap || in>cap-len) return Selection();
    Selection s=at(p.seq,Video,t);
    if(s.index>=0) {
        i=s.index;
        if(t>startOf(p.seq,s)) {
            if(int(p.seq.video.size())>=MaxClips-1 || !split(p.seq,s,t)) return Selection();
            ++i;
        }
    }
    return place(p,asset,Video,t,in,len,i);
}
bool split(Sequence& s, Selection sel, int t) {
    Clip* c=get(s,sel);
    if(!c) return false;
    auto& v=laneOf(s,sel.lane);
    int offset=t-startOf(s,sel);
    if(offset<=0 || offset>=c->len || int(v.size())>=MaxClips) return false;
    Clip tail=*c;
    if(sel.lane!=Titles) tail.in+=offset;
    tail.len-=offset;
    if(sel.lane!=Video) tail.pos+=offset;
    c->len=offset;
    c->fx.fadeOut=0;
    tail.fx.fadeIn=0;
    tail.fx.dissolve=0;
    v.insert(v.begin()+sel.index+1,std::move(tail));
    return true;
}
bool erase(Sequence& s, Selection sel) {
    if(!get(s,sel)) return false;
    auto& v=laneOf(s,sel.lane);
    v.erase(v.begin()+sel.index);
    return true;
}
int videoBoundary(const Sequence& s, int t) {
    int pos=0;
    for(int i=0;i<int(s.video.size());++i) {
        if(2*(t-pos)<s.video[i].len) return i;
        pos+=s.video[i].len;
    }
    return int(s.video.size());
}
Selection move(Sequence& s, Selection sel, int target) {
    const Clip* selected=get(s,sel);
    if(!selected) return sel;
    Clip c=*selected;
    auto& v=laneOf(s,sel.lane);
    if(sel.lane==Video) {
        int i=std::max(0,std::min(target,int(v.size())));
        if(i>sel.index) --i;
        if(i==sel.index) return sel;
        v.erase(v.begin()+sel.index);
        v.insert(v.begin()+i,std::move(c));
        return Selection(sel.lane,i);
    }
    c.pos=freePosition(v,sel.index,target,c.len);
    if(c.pos<0 || c.pos==selected->pos) return sel;
    v.erase(v.begin()+sel.index);
    auto it=std::lower_bound(v.begin(),v.end(),c.pos,[](const Clip& e,int pos){return e.pos<pos;});
    int i=int(it-v.begin());
    v.insert(it,std::move(c));
    return Selection(sel.lane,i);
}
bool trim(Project& p, Selection sel, bool left, int target) {
    Clip* c=get(p.seq,sel);
    if(!c) return false;
    bool title=sel.lane==Titles;
    if(!title && (c->asset<0 || c->asset>=int(p.assets.size()))) return false;
    bool virtualSource=title || p.assets[c->asset].still;
    int hi=c->in+c->len,lo=c->in;
    const auto& v=laneOf(p.seq,sel.lane);
    int prev=sel.index==0?0:v[sel.index-1].pos+v[sel.index-1].len;
    int next=sel.index+1==int(v.size())?MaxFrames:v[sel.index+1].pos;
    int budget=sel.lane==Video?MaxFrames-length(p.seq,Video):0;
    if(left) {
        int minIn=sel.lane==Video?lo-budget:lo+prev-c->pos;
        if(!virtualSource) minIn=std::max(0,minIn);
        int newIn=std::max(minIn,std::min(hi-1,target));
        if(newIn==lo) return false;
        if(sel.lane!=Video) c->pos+=newIn-lo;
        c->in=virtualSource?0:newIn;
        c->len=hi-newIn;
    } else {
        int maxOut=sel.lane==Video?hi+budget:lo+next-c->pos;
        int cap=virtualSource?MaxFrames:p.assets[c->asset].frames;
        int newOut=std::max(lo+1,std::min(std::min(maxOut,cap),target));
        if(newOut==hi) return false;
        c->len=newOut-lo;
    }
    return true;
}
bool putMarker(Sequence& s,int frame,const std::wstring& label) {
    if(frame<0 || frame>MaxFrames || label.size()>size_t(MaxMarkerText) || label.find(L'\0')!=std::wstring::npos) return false;
    auto i=std::lower_bound(s.markers.begin(),s.markers.end(),frame,[](const Marker& m,int t){return m.frame<t;});
    if(i!=s.markers.end() && i->frame==frame) {
        if(label.empty()) s.markers.erase(i);
        else i->text=label;
    } else {
        if(label.empty() || int(s.markers.size())>=MaxMarkers) return false;
        s.markers.insert(i,Marker(frame,label));
    }
    return true;
}
bool setWorkEdge(Sequence& s,bool out,int t) {
    if(t<0 || t>length(s) || (out?t==0:t==length(s))) return false;
    if(out) { s.workOut=t; if(s.workIn>=t) s.workIn=-1; }
    else { s.workIn=t; if(s.workOut<=t) s.workOut=-1; }
    return true;
}
static bool inRange(int n,int lo,int hi) { return n>=lo && n<=hi; }
static bool goodEffects(const Effects& e) {
    return inRange(e.scale,25,200) && inRange(e.x,-100,100) && inRange(e.y,-100,100) && inRange(e.opacity,0,100) &&
        inRange(e.brightness,-60,60) && inRange(e.contrast,50,150) && inRange(e.saturation,0,200) &&
        inRange(e.dissolve,0,MaxDissolve) && e.dissolve!=1 && inRange(e.pan,-100,100) &&
        inRange(e.fadeIn,0,MaxFade) && inRange(e.fadeOut,0,MaxFade);
}
bool valid(const Project& p,bool media) {
    const Sequence& s=p.seq;
    if(p.assets.size()>size_t(MaxAssets) || s.markers.size()>size_t(MaxMarkers) || !inRange(s.workIn,-1,MaxFrames-1) ||
        !inRange(s.workOut,-1,MaxFrames) || s.workOut==0 || (s.workOut>=0 && s.workIn>=s.workOut)) return false;
    int mark=-1;
    for(const Marker& m:s.markers) {
        if(m.frame<=mark || m.frame>MaxFrames || m.text.empty() || m.text.size()>size_t(MaxMarkerText) || m.text.find(L'\0')!=std::wstring::npos) return false;
        mark=m.frame;
    }
    for(int l=0;l<LaneCount;++l) {
        int last=0;
        const auto& v=laneOf(s,Lane(l));
        if(v.size()>size_t(MaxClips)) return false;
        for(const Clip& c:v) {
            if(c.in<0 || c.len<1 || c.len>MaxFrames || c.in>MaxFrames-c.len || !inRange(c.gain,0,200) || !goodEffects(c.fx)) return false;
            if(l==Video ? c.pos!=0 || last>MaxFrames-c.len : c.pos<last || c.pos>MaxFrames-c.len) return false;
            last=l==Video?last+c.len:c.pos+c.len;
            if(l==Titles) {
                const TitleStyle& t=c.title;
                if(c.asset!=-1 || c.in || t.text.empty() || t.text.size()>size_t(MaxTitle) || t.text.find(L'\0')!=std::wstring::npos ||
                    !inRange(t.position,0,2) || !inRange(t.color,0,3) || !inRange(t.size,3,13)) return false;
            } else {
                if(c.asset<0 || c.asset>=int(p.assets.size()) || !c.title.text.empty()) return false;
                const Asset& a=p.assets[c.asset];
                if(media && ((l==Video || l==Overlay?a.video:a.audio)<0 || (!a.still && c.in+c.len>a.frames))) return false;
            }
        }
    }
    return true;
}
int dissolveLength(const Clip& c) { return c.len>=2?std::min(c.len,c.fx.dissolve):0; }
Scene sceneAt(const Sequence& s,int frame) {
    Scene v;
    Selection sel=at(s,Video,frame);
    if((v.main=get(s,sel))) {
        v.mainOffset=frame-startOf(s,sel);
        v.dissolve=dissolveLength(*v.main);
        if(v.mainOffset>=v.dissolve) v.dissolve=0;
        else v.previous=get(s,Selection(Video,sel.index-1));
    }
    if(!s.hideV2 && (v.overlay=get(s,at(s,Overlay,frame)))) v.overlayOffset=frame-v.overlay->pos;
    v.title=get(s,at(s,Titles,frame));
    return v;
}
std::vector<int> sceneCuts(const Sequence& s,int first,int end) {
    std::vector<int> cuts={first,end};
    const auto mark=[&](int t){if(t>first && t<end) cuts.push_back(t);};
    int t=0;
    for(const Clip& c:s.video) { if(dissolveLength(c)) mark(t+dissolveLength(c)); t+=c.len; mark(t); }
    if(!s.hideV2) for(const Clip& c:s.overlay) { mark(c.pos); mark(c.pos+c.len); }
    for(const Clip& c:s.titles) { mark(c.pos); mark(c.pos+c.len); }
    std::sort(cuts.begin(),cuts.end());
    cuts.erase(std::unique(cuts.begin(),cuts.end()),cuts.end());
    return cuts;
}
PictureBox pictureBox(const Effects& e,int width,int height) {
    int w=std::max(2,(width*e.scale/100+1)&~1),h=std::max(2,(height*e.scale/100+1)&~1);
    PictureBox b={(width/2-w/2+width*e.x/200)&~1,(height/2-h/2+height*e.y/200)&~1,w,h};
    return b;
}
static std::wstring digits(unsigned value, unsigned width) {
    wchar_t buf[16];
    wchar_t* end=buf+16;
    wchar_t* p=end;
    do { *--p=wchar_t(L'0'+value%10); value/=10; } while(value);
    while(unsigned(end-p)<width) *--p=L'0';
    return std::wstring(p,end);
}
std::wstring number(int value) {
    return value<0 ? L"-"+digits(0u-unsigned(value),1) : digits(unsigned(value),1);
}
std::wstring timecode(int frame) {
    frame=std::max(0,frame);
    int s=frame/Fps;
    return digits(s/3600,2)+L":"+digits(s/60%60,2)+L":"+digits(s%60,2)+L":"+digits(frame%Fps,2);
}
std::wstring seconds(int frame) {
    frame=std::max(0,frame);
    return digits(frame/Fps,1)+L"."+digits(((frame%Fps)*1000000+Fps/2)/Fps,6);
}
}
