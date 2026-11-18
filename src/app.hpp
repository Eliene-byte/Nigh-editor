#pragma once
#include "draw.hpp"
#include "job.hpp"
#include "comp.hpp"
#include "player.hpp"
#include <shellapi.h>

namespace nigh {
const wchar_t AppClass[]=L"NighEditorNative";
enum Command { CmdNone, CmdFile, CmdNew, CmdOpen, CmdSave, CmdSaveAs, CmdImport, CmdExport, CmdExport360, CmdExport480,
    CmdDemo, CmdHelp, CmdExit, CmdUndo, CmdRedo, CmdSplit, CmdIn, CmdOut, CmdDelete, CmdPlay, CmdFirst, CmdLast,
    CmdPrev, CmdNext, CmdZoomOut, CmdFit, CmdZoomIn, CmdMute1, CmdMute2, CmdGainDown, CmdGainUp, CmdCancel,
    CmdVideoMode, CmdTools, CmdProps, CmdTitle, CmdMute3, CmdHideOverlay, CmdInsert, CmdAddOverlay, CmdAddMusic, CmdAddVoice,
    CmdCopy, CmdPaste, CmdDuplicate, CmdMarker, CmdMarkPrev, CmdMarkNext, CmdWorkIn, CmdWorkOut, CmdWorkClear,
    CmdSourceClear, CmdExport720, CmdExportWav, CmdUseRange, CmdWorkspaceEdit, CmdWorkspaceAssist, CmdEco,
    CmdFormat, CmdFormatWide, CmdFormatSquare, CmdFormatTall, CmdSourceMode, CmdPlaySource, CmdPlayProgram,
    CmdAnalyze, CmdAssistDemo, CmdAiReview, CmdAiNext, CmdAiPrevious, CmdAiMarkers, CmdAiSplit, CmdAiFace,
    CmdAiVoice, CmdAiTune, CmdQuality, CmdAssetBase=1000 };
struct Hit {
    int command;
    RECT area;
    bool enabled;
    Hit(int c,RECT r,bool e) : command(c),area(r),enabled(e) {}
};
enum TrackRow { RowTitle, RowOverlay, RowVideo, RowLinked, RowMusic, RowVoice, RowCount };
inline Lane rowLane(int row) {
    return row==RowTitle?Titles:row==RowOverlay?Overlay:row==RowMusic?Music:row==RowVoice?Voice:Video;
}
inline int laneRow(Lane lane) {
    return lane==Titles?RowTitle:lane==Overlay?RowOverlay:lane==Music?RowMusic:lane==Voice?RowVoice:RowVideo;
}
inline const wchar_t* laneName(Lane lane) {
    return lane==Titles?L"T1":lane==Overlay?L"V2":lane==Music?L"A2":lane==Voice?L"A3":L"V1 / A1";
}
struct SourceRange {
    int first,last;
    SourceRange() : first(-1),last(-1) {}
};
struct Layout {
    int w,h,labelW, rows;
    bool side, dual;
    RECT media, mediaList, viewer, display, transportSeek, sourcePanel, sourceDisplay, sourceSeek, inspector, gain, timeline, ruler, lanes[RowCount], scroll, status;
    Layout() : w(0),h(0),labelW(0),rows(0),side(false),dual(false),media(),mediaList(),viewer(),display(),transportSeek(),sourcePanel(),sourceDisplay(),sourceSeek(),inspector(),gain(),timeline(),ruler(),lanes(),scroll(),status() {}
};
enum Grab { Idle, Scrub, MonitorSeek, PoolItem, MoveClip, LeftEdge, RightEdge, Scrollbar, AdjustGain };
struct Drag {
    Grab type;
    bool moved;
    int x,y,origHead,at,asset,off,targetX,targetRow;
    Selection sel;
    Drag() : type(Idle),moved(false),x(0),y(0),origHead(0),at(0),asset(-1),off(0),targetX(-1),targetRow(-1) {}
};
class App {
    HWND hwnd_;
    const std::wstring dir_;
    TempDir scratch_;
    Project project_;
    std::unique_ptr<Job> job_;
    JobProgress progress_;
    Player player_;
    Preview preview_,rawPreview_;
    int cachedFrame_,rawFrame_,rawAsset_;
    SourceRange sourceRanges_[MaxAssets];
    std::vector<uint8_t> pixels_,rawPixels_;
    Fonts fonts_;
    HDC back_;
    HBITMAP bmp_;
    HGDIOBJ baseBmp_;
    int bw_,bh_;
    Layout ui_;
    std::vector<Hit> hits_;
    std::vector<Sequence> undo_,redo_;
    Selection selected_;
    Clip clipboard_;
    Lane copiedLane_;
    Drag drag_;
    AssistTarget analyzed_;
    AssistReport report_;
    int reviewIndex_;
    int asset_,source_,sourceHead_,head_,zoom_,first_,assetScroll_,mouseX_,mouseY_,focus_;
    bool dirty_,demo_,mouseIn_,closeWhenDone_,changingCapture_,hasClipboard_,useRange_,snapshotPending_,recovered_,assistMode_,eco_,hasReport_,quality_;
    std::wstring status_;

    void redraw(bool all=true);
    void updateTitle();
    void invalidateFrame();
    void layout(int w,int h);
    bool canDemo() const;
    bool canAssistDemo() const;
    void paint();
    void paintContent(HDC dc,RECT client,RECT dirty);
    void button(Canvas& c,int id,RECT r,const std::wstring& text,Glyph g,bool enabled=true,int style=0);
    void pill(Canvas& c,RECT r,const std::wstring& text,unsigned fg,unsigned bg);
    void section(Canvas& c,RECT r,const wchar_t* id,const wchar_t* title);
    void drawHeader(Canvas& c);
    void drawMedia(Canvas& c);
    void drawViewer(Canvas& c);
    void drawMonitor(Canvas& c,bool source,bool paired);
    void drawAssistant(Canvas& c);
    void drawInspector(Canvas& c);
    void drawTimeline(Canvas& c);
    void drawStatus(Canvas& c);
    void drawProgress(Canvas& c);
    void wave(Canvas& c,RECT box,const Clip& clip,const Asset& a,unsigned color);
    void drawClip(Canvas& c,const Clip& clip,int row,int index,int start);

    double pixelsPerFrame() const;
    int visibleFrames() const;
    int fullExtent() const;
    int maxScroll() const;
    int px(int frame) const;
    int frameAt(int x) const;
    int monitorFrame(int x) const;
    int monitorLength() const;
    int monitorHead() const { return source_>=0?sourceHead_:head_; }
    void seekMonitor(int frame);
    int snap(int frame,int skip=-1,Lane lane=Video) const;
    RECT scrollThumb() const;
    void scrollToPointer(int x,int offset);
    void zoomAt(int delta,int anchor);
    void seek(int t,bool reveal=true);
    void play();
    void stop();
    void playbackTick();
    void commit(Sequence previous,const std::wstring& message);
    void clearHistory();
    void restore(bool redo);
    void addAsset(int asset,int pos,int before=-1,Lane lane=LaneCount,bool insert=false);
    void resetSource();
    void trackProperties(bool newTitle=false);
    void marker();
    void markSource(bool out);
    void autoSave();
    void removeRecovery();
    void newProject();
    bool save(bool as);
    bool confirmDiscard();
    void openProject(const std::wstring& path=L"");
    void showDemo(bool assistant=false);
    void workspace(bool assistant);
    AssistTarget targetForAnalysis() const;
    void startAnalysis();
    void actOnReport(int id);
    void focusMonitor(bool source);
    void useFormat(FrameFormat format);
    void exportQuality(int quality);
    void importFiles(std::vector<std::wstring> paths);
    void exportTo(RenderPreset preset);
    bool startJob(std::unique_ptr<Job> job);
    void pollJob();
    void explain(const std::wstring& s,UINT icon=MB_ICONINFORMATION);
    void menu(int id,int x=-1,int y=-1);
    void execute(int id);
    const Hit* hit(int x,int y) const;
    RECT commandArea(int id) const;
    bool shortcut(WPARAM key,LPARAM flags);
    void mouseDown(int x,int y,bool dbl);
    int rowAt(int x,int y) const;
    Lane dropLane(int asset,int row) const;
    void mouseMove(int x,int y);
    void mouseUp(int x,int y);
    void mouseWheel(int x,int y,int delta,bool ctrl);
    void track(Grab grab,int x,int y);
    void cancelDrag();
    void doDrop(HDROP drop);
    void openArgs();
public:
    App();
    ~App();
    static LRESULT CALLBACK wndProc(HWND hwnd,UINT message,WPARAM w,LPARAM l);
    LRESULT event(UINT message,WPARAM w,LPARAM l);
};
}
