#pragma once
#include "media.hpp"
#include <mmsystem.h>

namespace nigh {
class Player {
    enum { BufCount=4, BufSamples=2048 };
    struct Slot { WAVEHDR header; int16_t data[BufSamples*2]; } slots_[BufCount];
    Mixer mixer_;
    HWAVEOUT wave_;
    int start_,end_,sent_,last_;
    DWORD epoch_;
    bool playing_;
    bool feed(Slot& slot);
    void closeWave();
public:
    Player();
    ~Player() { stop(); }
    bool active() const { return playing_; }
    bool start(std::vector<Sound> sounds,int length,int frame);
    void stop();
    int tick();
};
}
