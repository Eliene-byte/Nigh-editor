#include "local_models.hpp"
#include "core.hpp"
#include "cut_weights.hpp"
#include "webrtc/common_audio/vad/include/webrtc_vad.h"
#include <cmath>
#include <cstdlib>
#include <cstring>

namespace nigh {
ShotSignature shotSignature(const uint8_t* bgra) {
    ShotSignature s;
    int total=0,i=0;
    const int dx=ProxyW/ShotW,dy=ProxyH/ShotH;
    for(int y=0;y<ShotH;++y) for(int x=0;x<ShotW;++x,++i) {
        int light=0,chroma=0;
        for(int row:{dy/4,dy*3/4}) for(int col:{dx/4,dx*3/4}) {
            const uint8_t* p=bgra+((y*dy+row)*ProxyW+x*dx+col)*4;
            light+=(54*p[2]+183*p[1]+19*p[0])>>8;
            chroma+=int(p[2])-p[0];
        }
        s.light[i]=int16_t(light/4);
        s.chroma[i]=int16_t(chroma/4);
        total+=s.light[i];
    }
    s.mean=total/(ShotW*ShotH);
    return s;
}
static void distance(const ShotSignature& a,const ShotSignature& b,int* d) {
    int raw=0,color=0, texture=255*(ShotW-2)*(ShotH-2)*2;
    for(int i=0;i<ShotW*ShotH;++i) {
        raw+=std::abs(a.light[i]-b.light[i]);
        color+=std::abs(a.chroma[i]-b.chroma[i]);
    }
    const int mean=a.mean-b.mean;
    for(int dy=-1;dy<=1;++dy) for(int dx=-1;dx<=1;++dx) {
        int e=0;
        for(int y=1;y<ShotH-1;++y) for(int x=1;x<ShotW-1;++x)
            e+=std::abs(a.light[y*ShotW+x]-b.light[(y+dy)*ShotW+x+dx]-mean);
        texture=std::min(texture,e);
    }
    d[0]=raw/(ShotW*ShotH); d[1]=color/(2*ShotW*ShotH); d[2]=texture/((ShotW-2)*(ShotH-2));
}
int cutScore(const ShotSignature& a,const ShotSignature& b,const ShotSignature& c) {
    int features[9];
    distance(a,b,features);
    if(features[0]+features[1]+features[2]<28) return -1;
    distance(a,c,features+3); distance(b,c,features+6);
    int score=CutBias;
    for(int i=0;i<9;++i) score+=features[i]*CutWeights[i];
    return score;
}

// Non-rotated Pico tree inference, derived from run_cascade by Nenad Markuš
// (MIT, models/LICENSE-Pico.txt). Decode the pinned little-endian format rather
// than aliasing the model's byte storage as int/float pointers.
static uint32_t u32(const uint8_t* b) {
    return uint32_t(b[0])|(uint32_t(b[1])<<8)|(uint32_t(b[2])<<16)|(uint32_t(b[3])<<24);
}
static bool readWeight(const uint8_t* b,float& v) {
    uint32_t raw=u32(b);
    std::memcpy(&v,&raw,4);
    return std::isfinite(v) && std::fabs(v)<=10000.f;
}
bool FaceCascade::load(const uint8_t* data,size_t bytes) {
    static_assert(sizeof(float)==4,"Pico weights are binary32");
    const size_t stride=63*4+64*4+4, count=468;
    trees_.clear();
    if(!data || bytes!=16+stride*count || u32(data)!=3 || u32(data+4)!=0x7f817f81u || u32(data+8)!=6 || u32(data+12)!=count) return false;
    trees_.resize(count);
    for(size_t i=0;i<count;++i) {
        Tree& t=trees_[i];
        const uint8_t* b=data+16+i*stride;
        std::memcpy(t.pixels,b,sizeof(t.pixels)); b+=sizeof(t.pixels);
        for(int k=0;k<64;++k) if(!readWeight(b+4*k,t.leaves[k])) { trees_.clear();return false; }
        if(!readWeight(b+256,t.threshold)) { trees_.clear();return false; }
    }
    return true;
}
float FaceCascade::classify(const uint8_t* gray,int stride,int x,int y,int scale) const {
    float value=0;
    const int row=y*256,col=x*256;
    for(const Tree& tree:trees_) {
        int node=1;
        for(int i=0;i<6;++i) {
            const int8_t* code=tree.pixels+4*(node-1);
            int a=((row+code[0]*scale)>>8)*stride+((col+code[1]*scale)>>8);
            int b=((row+code[2]*scale)>>8)*stride+((col+code[3]*scale)>>8);
            node=2*node+(gray[a]<=gray[b]);
        }
        value+=tree.leaves[node-64];
        if(value<=tree.threshold) return 0;
    }
    return value-trees_.back().threshold;
}
static bool overlaps(const FaceHit& a,const FaceHit& b) {
    int x=std::max(0,std::min(a.x+a.size/2,b.x+b.size/2)-std::max(a.x-a.size/2,b.x-b.size/2));
    int y=std::max(0,std::min(a.y+a.size/2,b.y+b.size/2)-std::max(a.y-a.size/2,b.y-b.size/2));
    return 4*x*y>=a.size*a.size+b.size*b.size-x*y;
}
FaceHit FaceCascade::find(const uint8_t* gray,int w,int h) const {
    if(!ready() || !gray || w<25 || h<25 || w>ProxyW || h>ProxyH) return FaceHit();
    std::vector<FaceHit> local;
    local.reserve(96);
    for(int d=24;d<=std::min(w,h)*9/10;d=d*5/4) {
        int margin=d/2+1, step=std::max(3,d/9);
        for(int y=margin;y<h-margin;y+=step) for(int x=margin;x<w-margin;x+=step) {
            float score=classify(gray,w,x,y,d);
            if(score<=0) continue;
            if(local.size()<96u) local.push_back(FaceHit(x,y,d,score));
            else {
                auto low=std::min_element(local.begin(),local.end(),[](const FaceHit& a,const FaceHit& b){return a.score<b.score;});
                if(score>low->score) *low=FaceHit(x,y,d,score);
            }
        }
    }
    std::sort(local.begin(),local.end(),[](const FaceHit& a,const FaceHit& b){return a.score>b.score;});
    FaceHit best;
    bool joined[96]={};
    for(size_t i=0;i<local.size();++i) if(!joined[i]) {
        const FaceHit& seed=local[i];
        float x=float(seed.x),y=float(seed.y),d=float(seed.size),score=seed.score;
        int samples=1;
        for(size_t j=i+1;j<local.size();++j) if(!joined[j] && overlaps(seed,local[j])) {
            joined[j]=true; x+=local[j].x; y+=local[j].y; d+=local[j].size; score+=local[j].score; ++samples;
        }
        if(samples>=2 && score>=5.f && score>best.score)
            best=FaceHit(int(x/samples+.5f),int(y/samples+.5f),int(d/samples+.5f),score);
    }
    return best;
}
VoiceModel::VoiceModel() : model_(0) {
    if(WebRtcVad_Create(&model_)!=0 || WebRtcVad_Init(model_)!=0 || WebRtcVad_set_mode(model_,2)!=0) {
        if(model_) WebRtcVad_Free(model_);
        model_=0;
    }
}
VoiceModel::~VoiceModel() { if(model_) WebRtcVad_Free(model_); }
int VoiceModel::classify(const int16_t* samples) { return model_?WebRtcVad_Process(model_,8000,samples,240):-1; }
}
