#pragma once
#include "media.hpp"
#include "local_models.hpp"

namespace nigh {
const int AssistFrameLimit=90*Fps;
struct AssistSpan {
    int first,end;
    AssistSpan(int a,int b) : first(a),end(b) {}
};
struct AssistCue {
    int frame, voiceIndex; // -1 is a shot cut, otherwise indexes the voice intervals
    AssistCue(int f,int v) : frame(f),voiceIndex(v) {}
};
struct AssistTarget {
    int asset, first, end, sequenceStart, clipLength; // source and sequence, in timeline frames
    Selection clip;
    AssistTarget() : asset(-1),first(0),end(0),sequenceStart(-1),clipLength(0) {}
};
struct AssistReport {
    int first,end,videoFrames,gain,brightness,faceChecks,faceVotes,faceFrame;
    bool limited,hasVoice,faceModel;
    FaceHit face;
    std::vector<int> cuts;
    std::vector<AssistSpan> voice;
    std::vector<AssistCue> cues;
    AssistReport() : first(0),end(0),videoFrames(0),gain(-1),brightness(0),faceChecks(0),faceVotes(0),faceFrame(-1),
        limited(false),hasVoice(false),faceModel(false) {}
};

bool analyze(const MediaContext& ctx,const Asset& asset,int first,int end,AssistReport& out,std::wstring& error);
bool sameTarget(const Project& project,const AssistTarget& target);
std::vector<int> splitCandidates(const Project& project,const AssistTarget& target,const AssistReport& report);
void frameFace(Effects& fx,FrameFormat format,const FaceHit& face);
}
