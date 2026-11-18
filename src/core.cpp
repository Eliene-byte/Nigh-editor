#include "core.hpp"
#include <cstdio>
#include <cstdlib>
#include <cwchar>

namespace nigh {

bool Clip::operator==(const Clip& c) const {
    return asset==c.asset && in==c.in && len==c.len && pos==c.pos && gain==c.gain;
}
bool Sequence::operator==(const Sequence& s) const {
    return muteA1==s.muteA1 && muteA2==s.muteA2 && video==s.video && music==s.music;
}
std::vector<Clip>& laneOf(Sequence& s, Lane l) { return l==Video?s.video:s.music; }
const std::vector<Clip>& laneOf(const Sequence& s, Lane l) { return l==Video?s.video:s.music; }
Clip* get(Sequence& s, Selection sel) {
    std::vector<Clip>& v=laneOf(s,sel.lane);
    return sel.index>=0 && sel.index<int(v.size()) ? &v[sel.index] : 0;
}
const Clip* get(const Sequence& s, Selection sel) {
    const std::vector<Clip>& v=laneOf(s,sel.lane);
    return sel.index>=0 && sel.index<int(v.size()) ? &v[sel.index] : 0;
}
int length(const Sequence& s, Lane l) {
    if(l==Music) return s.music.empty()?0:s.music.back().pos+s.music.back().len;
    int n=0;
    for(const Clip& c:s.video) n+=c.len;
    return n;
}
int length(const Sequence& s) { return std::max(length(s,Video),length(s,Music)); }
int startOf(const Sequence& s, Selection sel) {
    const Clip* clip=get(s,sel);
    if(!clip) return -1;
    if(sel.lane==Music) return clip->pos;
    int t=0;
    for(int i=0;i<sel.index;++i) t+=s.video[i].len;
    return t;
}
Selection at(const Sequence& s, Lane l, int t) {
    int pos=0;
    const std::vector<Clip>& v=laneOf(s,l);
    for(int i=0;i<int(v.size());++i) {
        if(l==Music) pos=v[i].pos;
        if(t<pos) break;
        if(t<pos+v[i].len) return Selection(l,i);
        pos+=v[i].len;
    }
    return Selection(l);
}
int freePosition(const std::vector<Clip>& music, int skip, int desired, int duration) {
    if(duration<=0 || duration>MaxFrames) return -1;
    desired=std::max(0,std::min(desired, MaxFrames-duration));
    int last=0, best=-1, distance=MaxFrames+1;
    for(int i=0;i<=int(music.size());++i) {
        if(i==skip) continue;
        int end=i==int(music.size())?MaxFrames:music[i].pos;
        if(end-last>=duration) {
            int p=std::max(last,std::min(desired,end-duration));
            if(std::abs(p-desired)<distance) { best=p; distance=std::abs(p-desired); }
        }
        if(i<int(music.size())) last=music[i].pos+music[i].len;
    }
    return best;
}
Selection add(Project& p, int asset, int t, int before) {
    if(asset<0 || asset>=int(p.assets.size())) return Selection();
    const Asset& a=p.assets[asset];
    Lane lane=a.video>=0?Video:Music;
    if(a.frames<=0 || (a.video<0 && a.audio<0)) return Selection(lane);
    std::vector<Clip>& v=laneOf(p.seq,lane);
    if(int(v.size())>=MaxClips) return Selection(lane);
    Clip c(asset,0,a.frames);
    if(lane==Video) {
        if(a.frames>MaxFrames-length(p.seq,Video)) return Selection(lane);
        int i=before<0 ? int(v.size()) : std::min(before,int(v.size()));
        v.insert(v.begin()+i,c);
        return Selection(lane,i);
    }
    c.pos=freePosition(v,-1,t,c.len);
    if(c.pos<0) return Selection(lane);
    auto it=std::lower_bound(v.begin(),v.end(),c.pos,[](const Clip& e,int pos){return e.pos<pos;});
    int i=int(it-v.begin());
    v.insert(it,c);
    return Selection(lane,i);
}
bool split(Sequence& s, Selection sel, int t) {
    Clip* c=get(s,sel);
    if(!c) return false;
    std::vector<Clip>& v=laneOf(s,sel.lane);
    int offset=t-startOf(s,sel);
    if(offset<=0 || offset>=c->len || int(v.size())>=MaxClips) return false;
    Clip tail=*c;
    tail.in+=offset;
    tail.len-=offset;
    if(sel.lane==Music) tail.pos+=offset;
    c->len=offset;
    v.insert(v.begin()+sel.index+1,tail);
    return true;
}
bool erase(Sequence& s, Selection sel) {
    if(!get(s,sel)) return false;
    std::vector<Clip>& v=laneOf(s,sel.lane);
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
    std::vector<Clip>& v=laneOf(s,sel.lane);
    if(sel.lane==Video) {
        int i=std::max(0,std::min(target,int(v.size())));
        if(i>sel.index) --i;
        if(i==sel.index) return sel;
        v.erase(v.begin()+sel.index);
        v.insert(v.begin()+i,c);
        return Selection(sel.lane,i);
    }
    c.pos=freePosition(v,sel.index,target,c.len);
    if(c.pos<0 || c.pos==selected->pos) return sel;
    v.erase(v.begin()+sel.index);
    auto it=std::lower_bound(v.begin(),v.end(),c.pos,[](const Clip& e,int pos){return e.pos<pos;});
    int i=int(it-v.begin());
    v.insert(it,c);
    return Selection(sel.lane,i);
}
bool trim(Project& p, Selection sel, bool left, int target) {
    Clip* c=get(p.seq,sel);
    if(!c || c->asset<0 || c->asset>=int(p.assets.size())) return false;
    int hi=c->in+c->len, lo=c->in;
    const std::vector<Clip>& lane=laneOf(p.seq,sel.lane);
    int prev=sel.index==0?0:lane[sel.index-1].pos+lane[sel.index-1].len;
    int next=sel.index+1==int(lane.size())?MaxFrames:lane[sel.index+1].pos;
    int videoBudget=MaxFrames-length(p.seq,Video);
    if(left) {
        int minIn=sel.lane==Video ? lo-videoBudget : lo+prev-c->pos;
        int newIn=std::max(std::max(0,minIn),std::min(hi-1,target));
        if(newIn==lo) return false;
        if(sel.lane==Music) c->pos+=newIn-lo;
        c->in=newIn;
        c->len=hi-newIn;
    } else {
        int maxOut=sel.lane==Video ? hi+videoBudget : lo+next-c->pos;
        int newOut=std::max(lo+1,std::min(std::min(maxOut,p.assets[c->asset].frames),target));
        if(newOut==hi) return false;
        c->len=newOut-lo;
    }
    return true;
}
bool valid(const Project& p, bool media) {
    if(int(p.assets.size())>MaxAssets || int(p.seq.video.size())>MaxClips || int(p.seq.music.size())>MaxClips) return false;
    for(int l=0;l<2;++l) {
        int last=0;
        for(const Clip& c:laneOf(p.seq,Lane(l))) {
            if(c.asset<0 || c.asset>=int(p.assets.size()) || c.in<0 || c.len<1 ||
               c.len>MaxFrames || c.in>MaxFrames-c.len || c.gain<0 || c.gain>200) return false;
            if(l==Video && c.pos!=0) return false;
            if(l==Music && (c.pos<last || c.pos>MaxFrames-c.len)) return false;
            if(l==Video && last>MaxFrames-c.len) return false;
            last=l==Video ? last+c.len : c.pos+c.len;
            const Asset& a=p.assets[c.asset];
            if(media && ((l==Video?a.video:a.audio)<0 || c.in+c.len>a.frames)) return false;
        }
    }
    return true;
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
