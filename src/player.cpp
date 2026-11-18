#include "player.hpp"
#include <cstring>

namespace nigh {
static const int SamplesPerFrame=PreviewRate/Fps;
Player::Player() : wave_(0),start_(0),end_(0),sent_(0),last_(0),epoch_(0),playing_(false) {}
void Player::closeWave() {
    if(!wave_) return;
    waveOutReset(wave_);
    for(Slot& s:slots_) if(s.header.dwFlags&WHDR_PREPARED) waveOutUnprepareHeader(wave_,&s.header,sizeof(s.header));
    waveOutClose(wave_);
    wave_=0;
}
void Player::stop() {
    closeWave();
    mixer_.set(std::vector<Sound>());
    playing_=false;
}
bool Player::feed(Slot& s) {
    int count=std::min(BufSamples+0,end_-sent_);
    if(count<=0) return true;
    mixer_.fill(sent_,count,PreviewRate,2,s.data);
    s.header.dwBufferLength=count*4;
    s.header.dwFlags&=~WHDR_DONE;
    if(waveOutWrite(wave_,&s.header,sizeof(s.header))!=MMSYSERR_NOERROR) return false;
    sent_+=count;
    return true;
}
bool Player::start(std::vector<Sound> sounds,int length,int frame) {
    stop();
    end_=length*SamplesPerFrame;
    if(end_<=0) return false;
    if(frame*SamplesPerFrame>=end_) frame=0;
    start_=sent_=last_=std::max(0,frame)*SamplesPerFrame;
    epoch_=GetTickCount();
    playing_=true;
    if(sounds.empty()) return true;
    mixer_.set(std::move(sounds));
    WAVEFORMATEX fmt={WAVE_FORMAT_PCM,2,PreviewRate,PreviewRate*4,4,16,0};
    if(waveOutOpen(&wave_,WAVE_MAPPER,&fmt,0,0,CALLBACK_NULL)!=MMSYSERR_NOERROR) { wave_=0; return true; }
    std::memset(slots_,0,sizeof(slots_));
    waveOutPause(wave_);
    for(Slot& s:slots_) {
        s.header.lpData=reinterpret_cast<LPSTR>(s.data);
        s.header.dwBufferLength=sizeof(s.data);
        if(waveOutPrepareHeader(wave_,&s.header,sizeof(s.header))!=MMSYSERR_NOERROR || !feed(s)) { closeWave(); break; }
    }
    if(wave_ && waveOutRestart(wave_)!=MMSYSERR_NOERROR) closeWave();
    epoch_=GetTickCount();
    return true;
}
int Player::tick() {
    if(!playing_) return last_/SamplesPerFrame;
    int pos=last_;
    if(wave_) {
        MMTIME mm;
        std::memset(&mm,0,sizeof(mm));
        mm.wType=TIME_SAMPLES;
        if(waveOutGetPosition(wave_,&mm,sizeof(mm))==MMSYSERR_NOERROR) {
            if(mm.wType==TIME_SAMPLES) pos=start_+int(mm.u.sample);
            else if(mm.wType==TIME_BYTES) pos=start_+int(mm.u.cb/4);
            else if(mm.wType==TIME_MS) pos=start_+int(uint64_t(mm.u.ms)*PreviewRate/1000);
            else pos=-1;
            if(pos>=0) pos=std::min(sent_,pos);
        } else pos=-1;
        bool pending=false;
        for(Slot& s:slots_) {
            if((s.header.dwFlags&WHDR_DONE) && !feed(s)) { pos=-1; break; }
            if(s.header.dwFlags&WHDR_INQUEUE) pending=true;
        }
        if(!pending && sent_>=end_) pos=end_;
        if(pos<0) { closeWave(); start_=last_; epoch_=GetTickCount(); pos=last_; }
    } else pos=start_+int(uint64_t(GetTickCount()-epoch_)*PreviewRate/1000);
    last_=std::min(end_,std::max(last_,pos));
    if(last_>=end_) stop();
    return last_/SamplesPerFrame;
}
}
