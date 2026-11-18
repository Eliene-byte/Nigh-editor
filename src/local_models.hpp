#pragma once
#include <stdint.h>
#include <vector>
#include <cstddef>

struct WebRtcVadInst;
namespace nigh {
const int ShotW=16,ShotH=9;
struct ShotSignature {
    int16_t light[ShotW*ShotH],chroma[ShotW*ShotH];
    int mean;
};
ShotSignature shotSignature(const uint8_t* bgra); // 320x180 proxy, BGRA
int cutScore(const ShotSignature& a,const ShotSignature& b,const ShotSignature& c);

struct FaceHit {
    int x,y,size;
    float score;
    FaceHit(int cx=0,int cy=0,int d=0,float s=0) : x(cx),y(cy),size(d),score(s) {}
};
class FaceCascade {
    struct Tree {
        int8_t pixels[63*4];
        float leaves[64], threshold;
    };
    std::vector<Tree> trees_;
    float classify(const uint8_t* pixels,int stride,int x,int y,int scale) const;
public:
    bool load(const uint8_t* data,size_t bytes);
    bool ready() const { return !trees_.empty(); }
    FaceHit find(const uint8_t* gray,int width,int height) const;
};
class VoiceModel {
    WebRtcVadInst* model_;
public:
    VoiceModel();
    ~VoiceModel();
    VoiceModel(const VoiceModel&)=delete;
    VoiceModel& operator=(const VoiceModel&)=delete;
    bool ready() const { return model_!=0; }
    // 30 ms at 8000 Hz, 240 signed PCM samples; 1 voice, 0 other, -1 error.
    int classify(const int16_t* mono);
};
}
