#pragma once
#include "media.hpp"

namespace nigh {
// Straight-alpha BGRA. The same Win32 rasterizer is used for preview and final output.
bool rasterTitle(const TitleStyle& title,int width,int height,std::vector<uint8_t>& bgra);
int saturationWeight(int input,int output,int saturation); // coefficients in thousandths

class Preview {
    struct Cached {
        FrameReader reader;
        const Proxy* proxy;
        int index;
        std::vector<uint8_t> pixels;
        Cached() : proxy(0),index(-1) {}
        void reset() { reader.reset(); proxy=0; index=-1; pixels.clear(); }
        const std::vector<uint8_t>* read(const Asset& asset,int source);
    } base_,previous_,overlay_;
    TitleStyle lastTitle_;
    std::vector<uint8_t> title_,temp_;
public:
    void reset();
    bool frame(const Project& p,int frame,int source,std::vector<uint8_t>& pixels);
};
}
