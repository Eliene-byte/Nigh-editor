#include "core.hpp"
#include <cassert>
#include <iostream>

using namespace nigh;
static Asset media(int frames,bool video,bool audio) {
    Asset a;
    a.frames=frames; a.video=video?0:-1; a.audio=audio?1:-1;
    return a;
}
int main() {
    Project p;
    p.assets={media(120,true,true),media(72,true,false),media(90,false,true),media(60,false,true)};
    assert(add(p,0,0).index==0);
    assert(add(p,1,0).index==1);
    assert(length(p.seq)==192);
    assert(at(p.seq,Video,119).index==0);
    assert(at(p.seq,Video,120).index==1);
    assert(at(p.seq,Video,192).index==-1);
    assert(split(p.seq,Selection(Video,0),48));
    assert(!split(p.seq,Selection(Video,0),0));
    assert(p.seq.video[0]==Clip(0,0,48));
    assert(p.seq.video[1]==Clip(0,48,72));
    assert(move(p.seq,Selection(Video,0),3).index==2);
    assert(p.seq.video[2]==Clip(0,0,48));
    assert(trim(p,Selection(Video,2),true,10));
    assert(trim(p,Selection(Video,2),false,1024));
    assert(p.seq.video[2]==Clip(0,10,110));
    assert(erase(p.seq,Selection(Video,1)));
    assert(length(p.seq,Video)==182);
    assert(add(p,2,0).lane==Music);
    assert(add(p,3,44).index==1);
    assert(p.seq.music[1].pos==90);
    assert(split(p.seq,Selection(Music,0),25));
    assert(p.seq.music[1]==Clip(2,25,65,25));
    assert(!trim(p,Selection(Music,1),true,12)); // previous audio occupies the gap
    assert(trim(p,Selection(Music,1),true,35));
    assert(p.seq.music[1].pos==35 && p.seq.music[1].len==55);
    assert(move(p.seq,Selection(Music,1),160).index==2);
    assert(p.seq.music[2].pos==160 && p.seq.music[2].in==35);
    assert(length(p.seq,Music)==215);
    assert(length(p.seq)==215);
    assert(at(p.seq,Music,27).index==-1);
    assert(valid(p,true));
    p.seq.music[2].pos=12;
    assert(!valid(p,false));
    p.seq.music[2].pos=160;
    p.seq.video[0].asset=99;
    assert(!valid(p,false));
    p.seq.video[0].asset=0;
    p.seq.video[0].len=121;
    assert(!valid(p,true));
    p.seq.video[0].len=0;
    assert(!valid(p,false));
    assert(seconds(1)==L"0.041667");
    assert(seconds(71)==L"2.958333");
    assert(timecode(86400+1440+47)==L"01:01:01:23");
    Project max;
    max.assets={media(MaxFrames,true,false),media(1,true,false),media(MaxFrames,false,true)};
    assert(add(max,0,0).index==0);
    assert(add(max,1,0).index==-1);
    assert(add(max,2,999999).index==0);
    assert(add(max,2,0).index==-1);
    assert(valid(max,true));
    std::cout<<"Core: split, trim, reorder, tracks, limits and timecodes OK\n";
}
