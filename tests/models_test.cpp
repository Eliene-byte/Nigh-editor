#include "core.hpp"
#include "local_models.hpp"
#include <cassert>
#include <fstream>
#include <iostream>
#include <iterator>
using namespace nigh;
static std::vector<uint8_t> load(const char* path) {
    std::ifstream in(path,std::ios::binary);
    return std::vector<uint8_t>(std::istreambuf_iterator<char>(in),std::istreambuf_iterator<char>());
}
static ShotSignature flat(int gray,int tint) {
    ShotSignature f;
    f.mean=gray;
    std::fill(f.light,f.light+ShotW*ShotH,int16_t(gray));
    std::fill(f.chroma,f.chroma+ShotW*ShotH,int16_t(tint));
    return f;
}
int main() {
    ShotSignature a=flat(82,76), b=flat(150,-100), c=flat(154,-104);
    assert(cutScore(a,b,c)>0);
    assert(cutScore(a,b,a)<=0); // single flash/exposure spike
    assert(cutScore(a,a,c)<=0);
    assert(cutScore(a,flat(100,76),flat(120,76))<=0);
    std::vector<uint8_t> model=load("models/facefinder.bin");
    FaceCascade pico;
    assert(model.size()==239632 && pico.load(model.data(),model.size()));
    std::vector<uint8_t> blank(ProxyW*ProxyH,0);
    assert(!pico.find(blank.data(),ProxyW,ProxyH).size);
    std::vector<uint8_t> image=load("tests/fixtures/face-320x180.gray");
    assert(image.size()==blank.size());
    FaceHit hit=pico.find(image.data(),ProxyW,ProxyH);
    assert(hit.score>=5 && hit.size>=24 && hit.x>ProxyW*3/5 && hit.x<ProxyW*4/5);
    model[268]=0; model[269]=0; model[270]=0xc0; model[271]=0x7f; // NaN in the first leaf
    assert(!pico.load(model.data(),model.size()) && !pico.ready());
    assert(!pico.load(model.data(),14));
    VoiceModel quiet,talk;
    assert(quiet.ready() && talk.ready());
    int16_t samples[240]={};
    assert(quiet.classify(samples)==0);
    std::vector<uint8_t> speech=load("tests/fixtures/speech-8k.raw");
    assert(speech.size()==14490);
    std::string result;
    for(size_t pos=0;pos+480<=speech.size();pos+=480) {
        for(size_t i=0;i<240;++i) samples[i]=int16_t(speech[pos+2*i]|(speech[pos+2*i+1]<<8));
        result+=char('0'+talk.classify(samples));
    }
    assert(result=="000000111111111111111111110000");
    std::cout<<"Modelos locais: cortes e flash; rosto frontal; voz e silêncio OK\n";
}
