#include "app.hpp"
#include <cmath>

namespace nigh {
using namespace palette;
static const int RowH=57;
static std::wstring shortTime(int t) { return timecode(t).substr(t>=3600*Fps?0:3); }
static std::wstring rulerTime(int t) { std::wstring s=shortTime(t); return s.substr(0,s.size()-3); }
static const unsigned TrackFill[]={0x44361f,0x243c55,PurpleBg,BlueBg,MintBg,0x253e3c};
static const unsigned TrackInk[]={0xf0cb85,0x99c7f1,Purple,Blue,Mint,0x9cd2cc};
static const unsigned TrackStroke[]={0x645235,0x3f627f,0x56476c,0x354f61,0x355d53,0x41625c};
static bool pictureRow(int row) { return row==RowVideo || row==RowOverlay; }

void App::layout(int w,int h) {
    ui_.w=w; ui_.h=h;
    const int padding=12, gap=10, top=69, footer=26;
    int tlH=limit(h*40/100,223,337), tlY=h-footer-tlH;
    int upper=std::max(0,tlY-top-gap);
    int lib=limit(w/6,166,217);
    ui_.side=assistMode_ || w>=945;
    ui_.dual=!eco_ && w>=1110;
    int insp=assistMode_?limit(w/5,202,244):ui_.side?limit(w/6,181,215):0;
    ui_.media=rect(padding,top,lib,upper);
    ui_.mediaList=rect(padding+8,top+63,lib-16,std::max(0,upper-63-49));
    ui_.rows=std::max(1,height(ui_.mediaList)/RowH);
    int mx=padding+lib+gap, mw=w-padding-mx-(ui_.side?insp+gap:0);
    int sw=ui_.dual?(mw-gap)*46/100:0;
    int pw=ui_.dual?mw-sw-gap:mw, px=ui_.dual?mx+sw+gap:mx;
    ui_.viewer=rect(px,top,pw,upper);
    ui_.display=rect(px+9,top+43,pw-18,std::max(8,upper-99));
    ui_.transportSeek=rect(px+11,ui_.display.bottom+6,pw-22,14);
    ui_.sourcePanel=ui_.dual?rect(mx,top,sw,upper):rect(0,0,0,0);
    ui_.sourceDisplay=ui_.dual?rect(mx+9,top+43,sw-18,height(ui_.display)):rect(0,0,0,0);
    ui_.sourceSeek=ui_.dual?rect(mx+11,ui_.sourceDisplay.bottom+6,sw-22,14):rect(0,0,0,0);
    ui_.inspector=rect(w-padding-insp,top,insp,upper);
    ui_.gain=rect(0,0,0,0);
    ui_.status=rect(0,h-footer,w,footer);
    ui_.timeline=rect(padding,tlY,w-padding*2,tlH-7);
    ui_.labelW=w<870?99:120;
    int rx=ui_.timeline.left+ui_.labelW, rw=ui_.timeline.right-rx-1;
    ui_.ruler=rect(rx,tlY+43,rw,28);
    int body=ui_.timeline.bottom-15-ui_.ruler.bottom;
    int y=ui_.ruler.bottom, hTotal=0;
    const int weights[]={17,19,22,14,14,14};
    for(int i=0;i<RowCount;++i) {
        hTotal+=weights[i];
        int end=ui_.ruler.bottom+body*hTotal/100;
        ui_.lanes[i]=rect(rx,y,rw,end-y);
        y=end;
    }
    ui_.scroll=rect(rx+7,y+4,rw-14,8);
    first_=limit(first_,0,maxScroll());
    assetScroll_=limit(assetScroll_,0,std::max(0,int(project_.assets.size())-ui_.rows));
}
double App::pixelsPerFrame() const {
    int extent=std::max(Fps*10,length(project_.seq));
    double scale=std::max(.002,double(width(ui_.ruler))*0.86/extent);
    return std::max(.002,std::min(20.0,scale*std::pow(1.5,zoom_)));
}
int App::visibleFrames() const { return std::max(1,int(width(ui_.ruler)/pixelsPerFrame())); }
int App::fullExtent() const { return std::max(visibleFrames(),length(project_.seq)+std::max(4,visibleFrames()/7)); }
int App::maxScroll() const { return fullExtent()-visibleFrames(); }
int App::px(int frame) const { return ui_.ruler.left+int(std::floor((frame-first_)*pixelsPerFrame()+.5)); }
int App::frameAt(int x) const { return limit(first_+int(std::floor((x-ui_.ruler.left)/pixelsPerFrame()+.5)),0,MaxFrames); }
int App::monitorLength() const {
    return source_>=0 && source_<int(project_.assets.size())?project_.assets[source_].frames:length(project_.seq);
}
int App::monitorFrame(int x) const {
    const RECT& r=ui_.dual && source_>=0?ui_.sourceSeek:ui_.transportSeek;
    return limit(int(double(x-r.left)*monitorLength()/std::max(1,width(r)-1)+.5),0,monitorLength());
}
RECT App::scrollThumb() const {
    int w=width(ui_.scroll);
    if(maxScroll()<=0 || w<=20) return ui_.scroll;
    int s=limit(int(double(w)*visibleFrames()/fullExtent()),20,w);
    return rect(ui_.scroll.left+int(double(w-s)*first_/maxScroll()),ui_.scroll.top,s,height(ui_.scroll));
}
void App::button(Canvas& c,int id,RECT r,const std::wstring& label,Glyph g,bool enabled,int style) {
    enabled=enabled && (!job_ || id==CmdCancel || id==CmdHelp);
    hits_.push_back(Hit(id,r,enabled));
    bool hover=enabled && mouseIn_ && inside(r,mouseX_,mouseY_);
    unsigned fg=enabled?Text:Quiet, bg=hover?Raised:Surface, edge=hover?Soft:Edge;
    if(style==1 && enabled) { fg=Ink; bg=hover?0xa3ebf8:Accent; edge=bg; }
    else if(style==2 && !hover) { bg=Panel; edge=Panel; fg=enabled?Muted:Quiet; }
    else if(style==3 && enabled) { bg=hover?Raised:AccentSoft; edge=hover?Soft:0x345468; fg=Accent; }
    else if(style==4 && enabled) { bg=hover?Raised:Bar; edge=Bar; fg=Muted; }
    if(focus_==id && enabled) edge=Accent;
    c.box(r,bg,edge,style==3?9:6);
    int iconW=g==NoGlyph?0:16;
    int labelW=c.measure(label,Strong);
    int x=r.left+std::max(5,(width(r)-(iconW+(iconW && labelW?7:0)+labelW))/2);
    if(g!=NoGlyph) c.icon(g,x,centerY(r)-8,fg);
    if(labelW) {
        x+=iconW+(iconW?7:0);
        c.text(rect(x,r.top,std::max(2,int(r.right)-x-3),height(r)),label,Strong,fg);
    }
}
void App::pill(Canvas& c,RECT r,const std::wstring& s,unsigned fg,unsigned bg) {
    c.box(r,bg,bg,5);
    c.text(inset(r,4,0),s,Label,fg,DT_CENTER);
}
void App::section(Canvas& c,RECT r,const wchar_t* n,const wchar_t* title) {
    c.fill(r,Panel); c.border(r,Edge);
    c.fill(rect(r.left+1,r.top+39,width(r)-2,1),Edge);
    pill(c,rect(r.left+12,r.top+11,24,18),n,Accent,AccentSoft);
    c.text(rect(r.left+44,r.top+2,width(r)-52,36),title,Label,Text);
}
void App::drawHeader(Canvas& c) {
    RECT bar=rect(0,0,ui_.w,61);
    c.fill(bar,Bar); c.fill(rect(0,60,ui_.w,1),Edge);
    c.box(rect(14,13,33,33),Accent,Accent,8);
    c.icon(Logo,21,20,Ink,19);
    c.text(rect(55,11,102,23),L"Nigh Editor",Brand,Text);
    c.text(rect(56,34,112,13),L"ESTÚDIO LOCAL · 0.3",Label,Dim);
    c.line(165,14,165,46,Edge);
    button(c,CmdWorkspaceEdit,rect(176,14,89,32),L"Montagem",NoGlyph,true,assistMode_?2:3);
    button(c,CmdWorkspaceAssist,rect(273,14,101,32),L"Assistente",Spark,true,assistMode_?3:2);
    int ex=ui_.w-14-108, imp=ex-7-94, saveX=imp-7-40, fileX=saveX-7-83;
    int nameWidth=fileX-392;
    if(nameWidth>82) {
        c.line(384,14,384,46,Edge);
        c.text(rect(395,10,nameWidth-4,16),L"PROJETO ATUAL",Label,Dim);
        c.text(rect(395,26,nameWidth-17,22),(dirty_?L"* ":L"")+project_.name,Strong,Text);
    }
    button(c,CmdFile,rect(fileX,14,83,32),L"Arquivo",Folder);
    button(c,CmdSave,rect(saveX,14,40,32),L"",Disk);
    button(c,CmdImport,rect(imp,14,94,32),L"Importar",Plus);
    button(c,CmdExport,rect(ex,14,108,32),L"Exportar",Export,length(project_.seq)>0,1);
}
void App::drawMedia(Canvas& c) {
    RECT r=ui_.media;
    section(c,r,L"01",L"PROJETO");
    pill(c,rect(r.right-41,r.top+10,28,19),number(int(project_.assets.size())),Muted,Surface);
    c.text(rect(r.left+13,r.top+42,width(r)-26,23),L"MÍDIAS DO PROJETO",Label,Dim);
    RECT list=ui_.mediaList;
    int saved=SaveDC(c.dc());
    IntersectClipRect(c.dc(),list.left,list.top,list.right,list.bottom);
    if(project_.assets.empty()) {
        c.box(rect(r.left+13,list.top+6,width(r)-26,75),Screen,Edge,8);
        c.icon(Film,r.left+25,list.top+20,Dim);
        c.text(rect(r.left+49,list.top+13,width(r)-66,26),L"Biblioteca vazia",Strong,Muted);
        c.paragraph(rect(r.left+25,list.top+43,width(r)-47,35),L"Os arquivos importados aparecem aqui.",Small,Dim);
    }
    for(int i=assetScroll_,y=list.top;i<int(project_.assets.size()) && y+24<list.bottom;++i,y+=RowH) {
        const Asset& a=project_.assets[i];
        RECT row=rect(list.left,y,width(list)-1,53);
        bool hover=mouseIn_ && inside(row,mouseX_,mouseY_);
        if(asset_==i || hover) c.box(row,asset_==i?Raised:Surface,asset_==i?Soft:Edge,6);
        if(asset_==i) c.fill(rect(row.left+1,row.top+7,2,39),Accent);
        RECT pic=rect(row.left+7,row.top+8,62,36);
        if(a.video>=0 && a.proxy && !a.proxy->thumb.empty()) c.image(pic,a.proxy->thumb,88,50);
        else {
            c.box(pic,MintBg,MintBg,6);
            c.icon(MusicNote,centerX(pic)-8,centerY(pic)-8,Mint);
        }
        int x=pic.right+8, w=std::max(8,int(row.right)-x-32);
        c.text(rect(x,row.top+7,w,20),a.name,Strong,Text);
        c.text(rect(x,row.top+27,w+5,18),shortTime(a.frames)+L"  ·  "+(a.still?L"IMAGEM":a.video>=0?L"VÍDEO":L"ÁUDIO"),Small,Dim);
        if(y+44<list.bottom) button(c,CmdAssetBase+i,rect(row.right-28,row.top+15,23,24),L"",Plus,true,2);
        c.fill(rect(row.left+4,y+54,width(row)-8,1),Edge);
    }
    RestoreDC(c.dc(),saved);
    if(int(project_.assets.size())>ui_.rows && height(list)>0) {
        int range=std::max(1,int(project_.assets.size())-ui_.rows);
        int thumb=limit(height(list)*ui_.rows/int(project_.assets.size()),7,height(list));
        c.fill(rect(r.right-4,list.top+(height(list)-thumb)*assetScroll_/range,2,thumb),Quiet);
    }
    button(c,CmdImport,rect(r.left+12,r.bottom-43,width(r)-24,30),L"Importar mídia",Plus);
}
void App::drawViewer(Canvas& c) {
    if(ui_.dual) drawMonitor(c,true,true);
    drawMonitor(c,!ui_.dual && source_>=0,ui_.dual);
}
void App::drawMonitor(Canvas& c,bool source,bool paired) {
    const bool left=source && paired, active=source==(source_>=0);
    const RECT r=left?ui_.sourcePanel:ui_.viewer, canvas=left?ui_.sourceDisplay:ui_.display;
    const RECT seek=left?ui_.sourceSeek:ui_.transportSeek;
    if(width(r)<1) return;
    int asset=source?asset_:-1;
    const Asset* a=asset>=0?&project_.assets[asset]:0;
    int end=source?(a?a->frames:0):length(project_.seq), mark=source?sourceHead_:head_;
    int frame=limit(mark,0,std::max(0,end-1));
    if(eco_ && active && player_.active()) frame=(frame/4)*4;
    int w=source?ProxyW:previewWidth(project_.seq.format);
    section(c,r,source?L"F":L"P",source?L"MONITOR DE FONTE":L"MONITOR DE PROGRAMA");
    if(active) c.fill(rect(r.left+1,r.top+1,width(r)-2,2),Accent);
    if(!paired) {
        int x=r.right-(source?98:147);
        if(source || asset_>=0)
            button(c,source?CmdVideoMode:CmdSourceMode,rect(x,r.top+9,source?85:74,23),source?L"Programa":L"Fonte",NoGlyph,true,2);
    }
    if(source && paired && a && width(r)>=292) c.text(rect(r.left+164,r.top+2,width(r)-174,36),a->name,Small,Dim,DT_RIGHT);
    if(!source) button(c,CmdFormat,rect(r.right-55,r.top+9,47,23),formatName(project_.seq.format),NoGlyph,true,3);
    c.fill(canvas,Screen); c.border(canvas,Edge);
    bool visible=RectVisible(c.dc(),&canvas)!=FALSE;
    auto& data=source?rawPixels_:pixels_;
    int& cached=source?rawFrame_:cachedFrame_;
    if(visible && (cached!=frame || (source && rawAsset_!=asset))) {
        cached=frame;
        if(source) {
            rawAsset_=asset;
            if(!a || !rawPreview_.frame(project_,frame,asset,data)) data.clear();
        } else if(!preview_.frame(project_,frame,-1,data)) data.clear();
    }
    if(visible && !data.empty()) {
        int imgW=std::min(width(canvas)-2,(height(canvas)-2)*w/ProxyH), imgH=imgW*ProxyH/w;
        RECT picture=rect(centerX(canvas)-imgW/2,centerY(canvas)-imgH/2,imgW,imgH);
        c.image(picture,data,w,ProxyH);
        if(source && assistMode_ && hasReport_ && analyzed_.asset==asset && report_.face.size && (a->still || std::abs(mark-report_.faceFrame)<Fps/ProxyFps)) {
            int x=picture.left+report_.face.x*imgW/ProxyW, y=picture.top+report_.face.y*imgH/ProxyH;
            int d=std::max(9,report_.face.size*imgW/ProxyW);
            c.border(rect(x-d/2,y-d/2,d,d),Accent);
            if(d>23) c.line(x-d/2,y-d/2,x-d/2+9,y-d/2,Accent,2);
        }
    } else if(visible || (!source && end==0)) {
        int cx=centerX(canvas), cy=centerY(canvas);
        const wchar_t* title=source?(a?a->name.c_str():L"Visualizar uma fonte"):L"Monte sua próxima história";
        if(source && height(canvas)>99) c.icon(a?MusicNote:Film,cx-8,cy-38,Dim);
        if(height(canvas)>42) c.text(rect(canvas.left+7,cy-(source?16:30),width(canvas)-14,22),title,Strong,Muted,DT_CENTER);
        if(!source && end==0 && height(canvas)>42) {
            bool show=canDemo();
            button(c,show?CmdDemo:CmdImport,rect(cx-63,cy-3,126,26),show?L"Abrir demo":L"Importar mídia",show?Play:Plus,true,3);
        } else if(source && !a && height(canvas)>109)
            c.text(rect(canvas.left+7,cy+9,width(canvas)-14,22),L"Selecione um arquivo na biblioteca",Small,Dim,DT_CENTER);
    }
    int y=centerY(seek), k=width(seek)-1;
    int head=seek.left+(end?int(int64_t(k)*std::min(mark,end)/end):0);
    c.fill(rect(seek.left,y,width(seek),2),Soft);
    c.fill(rect(seek.left,y,head-seek.left,2),Accent);
    if(a && end>0) {
        const SourceRange& s=sourceRanges_[asset];
        if(s.first>=0 || s.last>=0) {
            int x1=seek.left+int(int64_t(std::max(0,s.first))*k/end);
            int x2=seek.left+int(int64_t(s.last<0?end:s.last)*k/end);
            c.fill(rect(x1,y-2,std::max(1,x2-x1),5),TrackInk[RowTitle]);
        }
    }
    c.dot(head,y+1,3,end?Accent:Quiet);
    int top=r.bottom-34,cx=centerX(r);
    c.text(rect(r.left+10,top,89,28),timecode(mark),width(r)>=390?MonoLarge:Mono,active?Accent:Muted);
    int playCmd=paired?(source?CmdPlaySource:CmdPlayProgram):CmdPlay;
    if(paired) {
        button(c,playCmd,rect(cx-17,top,34,28),L"",active && player_.active()?Pause:Play,end>0,1);
        if(width(r)>345) c.text(rect(r.right-92,top,82,28),timecode(end),Mono,Dim,DT_RIGHT);
    } else {
        button(c,CmdFirst,rect(cx-83,top+1,25,26),L"",First,end>0,2);
        button(c,CmdPrev,rect(cx-54,top+1,25,26),L"",Prev,end>0,2);
        button(c,playCmd,rect(cx-19,top,38,28),L"",player_.active()?Pause:Play,end>0,1);
        button(c,CmdNext,rect(cx+29,top+1,25,26),L"",Next,end>0,2);
        button(c,CmdLast,rect(cx+58,top+1,25,26),L"",Last,end>0,2);
        if(width(r)>367) c.text(rect(r.right-92,top,82,28),timecode(end),Mono,Dim,DT_RIGHT);
    }
}
void App::drawAssistant(Canvas& c) {
    RECT r=ui_.inspector;
    section(c,r,L"IA",L"ASSISTENTE LOCAL");
    int x=r.left+11,w=width(r)-22, y=r.top+45;
    int bottom=r.bottom-35;
    if(hasReport_) {
        bool mapped=sameTarget(project_,analyzed_);
        c.text(rect(x,y,w,20),project_.assets[analyzed_.asset].name,Strong,Text);
        c.text(rect(x,y+20,w,16),timecode(report_.first)+L"  →  "+timecode(report_.end),Mono,Dim);
        if(y+78<=bottom) {
            int cell=(w-7)/2;
            c.box(rect(x,y+39,cell,39),Surface,Edge,6);
            c.box(rect(x+cell+7,y+39,w-cell-7,39),Surface,Edge,6);
            c.text(rect(x+8,y+41,cell-15,18),number(int(report_.cuts.size())),Brand,Accent);
            c.text(rect(x+cell+15,y+41,cell-15,18),number(int(report_.voice.size())),Brand,Mint);
            c.text(rect(x+8,y+58,cell-13,16),L"CORTES",Label,Muted);
            c.text(rect(x+cell+15,y+58,cell-13,16),L"FALAS",Label,Muted);
        } else c.text(rect(x,y+38,w,22),number(int(report_.cuts.size()))+L" cortes  /  "+number(int(report_.voice.size()))+L" falas",Strong,Accent);
        if(y+103<bottom) {
            if(mapped && analyzed_.clip.lane==Video && report_.face.size)
                button(c,CmdAiFace,rect(x,y+83,w,23),L"Reenquadrar pelo rosto",Target,true,3);
            else c.text(rect(x,y+83,w,20),report_.face.size?L"Rosto frontal detectado":report_.faceModel?L"Sem rosto estável":L"Rosto não analisado",Small,report_.face.size?Accent:Dim);
        }
        if(y+137<bottom && !report_.cues.empty()) {
            bool pick=reviewIndex_>=0;
            std::wstring label=L"Percorrer sugestões";
            if(pick) {
                const AssistCue& cue=report_.cues[reviewIndex_];
                label=(cue.voiceIndex>=0?L"Fala  ":L"Corte  ")+shortTime(cue.frame);
            }
            button(c,CmdAiPrevious,rect(x,y+115,25,22),L"",Prev,true,2);
            c.text(rect(x+30,y+114,w-60,22),label,pick?Mono:Small,Muted,DT_CENTER);
            button(c,CmdAiNext,rect(x+w-25,y+115,25,22),L"",Next,true,2);
            if(pick && y+160<bottom) {
                c.text(rect(x,y+139,w,17),L"Sugestão "+number(reviewIndex_+1)+L" de "+number(int(report_.cues.size()))+
                    (mapped?L" · na sequência":L" · na fonte"),Small,Dim,DT_CENTER);
            }
        }
        button(c,CmdAiReview,rect(x,bottom,w-41,25),L"Revisar sugestões",Spark,true,1);
        button(c,CmdAnalyze,rect(x+w-34,bottom,34,25),L"",Redo,targetForAnalysis().asset>=0);
    } else {
        AssistTarget t=targetForAnalysis();
        std::wstring title=t.asset<0?L"Análise sob demanda":project_.assets[t.asset].name;
        c.text(rect(x,y,w,21),title,Strong,Text);
        c.paragraph(rect(x,y+25,w,std::max(1,bottom-y-29)),L"Cortes, atividade de voz e posição do rosto. Até 90 s por análise, sem enviar arquivos.",Small,Muted);
        if(t.asset>=0 && bottom-y>101 && canAssistDemo()) button(c,CmdAssistDemo,rect(x,bottom-31,w,24),L"Abrir demo de IA",Play,true,2);
        if(t.asset>=0) button(c,CmdAnalyze,rect(x,bottom,w,25),L"Analisar trecho",Spark,true,1);
        else button(c,CmdAssistDemo,rect(x,bottom,w,25),L"Abrir demo de IA",Play,canAssistDemo(),1);
    }
}
void App::drawInspector(Canvas& c) {
    if(!ui_.side) return;
    if(assistMode_) { drawAssistant(c); return; }
    RECT r=ui_.inspector;
    section(c,r,L"03",L"PROPRIEDADES");
    const Clip* clip=get(project_.seq,selected_);
    int left=r.left+13, ww=width(r)-26;
    if(!clip) {
        c.icon(Target,left,r.top+55,Accent);
        c.text(rect(left+23,r.top+47,ww-24,31),L"Detalhes do clipe",Strong,Text);
        c.paragraph(rect(left,r.top+82,ww,62),L"Selecione um clipe para ajustar imagem, transição, áudio ou título. F2 abre as propriedades.",Body,Muted);
        return;
    }
    int row=laneRow(selected_.lane);
    const Asset* a=selected_.lane==Titles?0:&project_.assets[clip->asset];
    std::wstring name=a?a->name:clip->title.text.substr(0,clip->title.text.find_first_of(L"\r\n"));
    int top=r.top+47, bottom=r.bottom-40;
    unsigned fg=TrackInk[row];
    pill(c,rect(left,top,64,16),laneName(selected_.lane),fg,TrackFill[row]);
    if(selected_.lane==Overlay) c.text(rect(left+71,top,ww-71,16),L"SÓ IMAGEM",Label,Dim);
    c.text(rect(left,top+17,ww,22),name,Strong,Text);
    struct Data { const wchar_t* label; int frame; } data[]={{L"Posição",startOf(project_.seq,selected_)},
        {L"Duração",clip->len},{L"Entrada",clip->in},{L"Saída",clip->in+clip->len}};
    int y=top+43;
    int rows=bottom-y>=121 && a?4:2;
    for(int i=0;i<rows && y+20<bottom;++i,y+=20) {
        c.fill(rect(left,y,ww,1),Edge);
        c.text(rect(left,y+1,ww-82,18),data[i].label,Small,Muted);
        c.text(rect(left+ww-83,y+1,83,18),timecode(data[i].frame),Mono,Text,DT_RIGHT);
    }
    if(y+32<bottom) {
        if(row==RowVideo || row==RowOverlay) {
            std::wstring fx=L"Escala "+number(clip->fx.scale)+L"%";
            if(clip->fx.dissolve) fx+=L"  ·  Dissolver "+number(dissolveLength(*clip));
            else if(clip->fx.color()) fx+=L"  ·  Cor ajustada";
            c.text(rect(left,y+2,ww,18),fx,Small,fg);
        } else if(!a) c.text(rect(left,y+2,ww,18),L"Texto gravado na imagem",Small,fg);
    }
    if(y+50<bottom && a && selected_.lane!=Overlay && a->audio>=0) {
        y=bottom-44;
        c.text(rect(left,y,ww-49,17),L"VOLUME",Label,Dim);
        int gain=drag_.type==AdjustGain?limit((mouseX_-left-28)*200/std::max(1,ww-57),0,200):clip->gain;
        c.text(rect(left+ww-49,y,49,17),number(gain)+L"%",Mono,Accent,DT_RIGHT);
        button(c,CmdGainDown,rect(left,y+17,22,20),L"",Minus,true,2);
        button(c,CmdGainUp,rect(left+ww-22,y+17,22,20),L"",Plus,true,2);
        ui_.gain=job_?rect(0,0,0,0):rect(left+28,y+17,ww-56,20);
        RECT bar=rect(left+28,y+25,ww-56,3);
        c.fill(bar,Soft);
        int x=bar.left+(width(bar)-1)*gain/200;
        c.fill(rect(bar.left,bar.top,x-bar.left,3),fg);
        c.dot(x,bar.top+1,3,fg);
    }
    button(c,CmdProps,rect(left,r.bottom-34,ww,25),selected_.lane==Titles?L"Editar título":L"Propriedades",Target,true,3);
}
void App::wave(Canvas& c,RECT box,const Clip& clip,const Asset& a,unsigned color) {
    if(!a.proxy || a.proxy->peaks.empty() || width(box)<3 || height(box)<4 || !RectVisible(c.dc(),&box)) return;
    const auto& peaks=a.proxy->peaks;
    int lo=std::max(int(ui_.ruler.left),int(box.left)), hi=std::min(int(ui_.ruler.right),int(box.right));
    int mid=centerY(box), sw=width(box), half=height(box)/2;
    for(int x=lo;x<hi;x+=3) {
        int t=clip.in+int(int64_t(clip.len-1)*(x-box.left)/sw);
        int n=limit(t/(Fps/4),0,int(peaks.size())-1);
        int q=limit((int(peaks[n])*clip.gain)*half/(100*52),1,half);
        c.fill(rect(x,mid-q,1,2*q),color);
    }
}
void App::drawClip(Canvas& c,const Clip& clip,int row,int i,int start) {
    RECT slot=ui_.lanes[row];
    int x=px(start),x2=px(start+clip.len);
    if(x2<slot.left || x>=slot.right) return;
    Lane lane=rowLane(row);
    const Asset* a=lane==Titles?0:&project_.assets[clip.asset];
    bool sel=selected_.index==i && selected_.lane==lane;
    bool mute=(row==RowOverlay && project_.seq.hideV2)||(row==RowLinked && project_.seq.muteA1)||
        (row==RowMusic && project_.seq.muteA2)||(row==RowVoice && project_.seq.muteA3);
    unsigned fg=mute?Quiet:TrackInk[row];
    unsigned edge=sel?TrackInk[row]:mute?Edge:TrackStroke[row];
    RECT box=rect(x+1,slot.top+2,std::max(1,x2-x-2),height(slot)-4);
    if(!RectVisible(c.dc(),&box)) return;
    c.box(box,mute?Surface:TrackFill[row],edge,4);
    int w=width(box), h=height(box);
    if(w<6) return;
    c.fill(rect(box.left+2,box.top+3,2,h-6),fg);
    if(!a || pictureRow(row)) {
        int picW=pictureRow(row) && h>=30 && w>=119 && a->proxy && !a->proxy->thumb.empty()?std::min(58,(h-6)*16/9):0;
        if(picW) {
            int ph=picW*9/16;
            c.image(rect(box.left+7,centerY(box)-ph/2,picW,ph),a->proxy->thumb,88,50);
        }
        int tx=box.left+(picW?picW+14:7),tw=std::max(1,int(box.right)-tx-5),cy=centerY(box);
        std::wstring name=a?a->name:clip.title.text.substr(0,clip.title.text.find_first_of(L"\r\n"));
        c.text(rect(tx,cy-(h>35?16:h/2),tw,std::min(21,h)),name,h>23?Strong:Small,mute?Dim:Text);
        if(h>35) c.text(rect(tx,cy+3,tw,16),shortTime(clip.len)+L"  ·  "+laneName(lane),Small,fg);
        if(row==RowVideo && dissolveLength(clip)>0) {
            int band=limit(px(start+dissolveLength(clip))-x-2,3,std::max(3,w-2));
            c.fill(rect(box.left+1,box.top+1,band,2),Accent);
        }
    } else {
        if(a->audio>=0) {
            if(h>=26 && row!=RowLinked && w>55) {
                c.text(rect(box.left+7,box.top+1,w-14,12),a->name,Small,fg);
                wave(c,rect(box.left+7,box.top+13,w-14,h-15),clip,*a,fg);
            } else wave(c,inset(box,7,3),clip,*a,fg);
            int in=px(start+std::min(clip.fx.fadeIn,clip.len)), out=px(start+clip.len-std::min(clip.fx.fadeOut,clip.len));
            if(in>box.left+3) c.line(box.left+2,box.bottom-2,std::min(in,int(box.right)-2),box.top+2,fg);
            if(out<box.right-3) c.line(std::max(out,int(box.left)+2),box.top+2,box.right-2,box.bottom-2,fg);
        } else c.text(inset(box,7,0),L"Sem áudio",Small,Dim);
    }
    if(sel && w>18 && row!=RowLinked) {
        int cy=centerY(box);
        c.line(box.left+4,cy-3,box.left+4,cy+4,fg);
        c.line(box.right-4,cy-3,box.right-4,cy+4,fg);
    }
}
void App::drawTimeline(Canvas& c) {
    RECT r=ui_.timeline;
    c.fill(r,Panel); c.border(r,Edge);
    int left=r.left+12,top=r.top+7;
    pill(c,rect(left,top+5,24,18),L"04",Accent,AccentSoft);
    c.text(rect(left+30,top+1,101,28),L"LINHA DO TEMPO",Label,Text);
    int x=left+132;
    button(c,CmdUndo,rect(x,top,26,29),L"",Undo,!undo_.empty(),2);
    button(c,CmdRedo,rect(x+28,top,26,29),L"",Redo,!redo_.empty(),2);
    c.line(x+59,top+4,x+59,top+25,Edge);
    const Clip* clip=get(project_.seq,selected_);
    int start=startOf(project_.seq,selected_);
    bool edit=clip && head_>start && head_<start+clip->len;
    button(c,CmdSplit,rect(x+65,top,65,29),L"Dividir",Scissors,edit,2);
    button(c,CmdIn,rect(x+133,top,31,29),L"In",NoGlyph,edit || source_>=0,2);
    button(c,CmdOut,rect(x+165,top,34,29),L"Out",NoGlyph,edit || source_>=0,2);
    button(c,CmdTitle,rect(x+205,top,66,29),L"Título",Plus,head_<MaxFrames,3);
    button(c,CmdProps,rect(x+274,top,28,29),L"",Target,clip!=0,2);
    button(c,CmdTools,rect(x+306,top,92,29),L"Ferramentas",NoGlyph,true);
    int zx=r.right-132;
    if(zx>x+507) c.text(rect(zx-105,top,98,29),timecode(length(project_.seq)),Mono,Muted,DT_RIGHT);
    button(c,CmdZoomOut,rect(zx,top,26,29),L"",Minus,zoom_>-6,2);
    button(c,CmdFit,rect(zx+29,top,60,29),zoom_==0?L"Auto":number(int(100*std::pow(1.5,zoom_)))+L"%",NoGlyph,true,zoom_==0?3:0);
    button(c,CmdZoomIn,rect(zx+92,top,26,29),L"",Plus,zoom_<8,2);
    c.fill(rect(r.left+1,r.top+42,width(r)-2,1),Edge);
    RECT ruler=ui_.ruler;
    int bottom=ui_.lanes[RowCount-1].bottom;
    c.fill(ruler,Screen);
    c.fill(rect(r.left+1,ruler.top,ui_.labelW-1,height(ruler)),Bar);
    c.icon(Cursor,left+1,ruler.top+6,Dim);
    c.text(rect(left+23,ruler.top,ui_.labelW-32,height(ruler)),L"Edição",Small,Dim);
    static const wchar_t* names[]={L"Texto",L"Sobrepor",L"Vídeo",L"Original",L"Trilha",L"Voz"};
    static const wchar_t* lanes[]={L"T1",L"V2",L"V1",L"A1",L"A2",L"A3"};
    for(int i=0;i<RowCount;++i) {
        RECT lane=ui_.lanes[i];
        c.fill(lane,i%2?0x171b21:0x13161b);
        c.fill(rect(r.left+1,lane.top,ui_.labelW-1,height(lane)),i%2?Bar:Panel);
        c.fill(rect(r.left+1,lane.top,width(r)-2,1),Edge);
        int hh=std::min(17,height(lane)-2);
        pill(c,rect(left,centerY(lane)-hh/2,26,hh),lanes[i],TrackInk[i],TrackFill[i]);
        c.text(rect(left+32,lane.top,ui_.labelW-57,height(lane)),names[i],Small,Muted);
        int cmd=i==RowLinked?CmdMute1:i==RowMusic?CmdMute2:i==RowVoice?CmdMute3:i==RowOverlay?CmdHideOverlay:0;
        if(cmd) {
            bool mute=i==RowLinked?project_.seq.muteA1:i==RowMusic?project_.seq.muteA2:i==RowVoice?project_.seq.muteA3:project_.seq.hideV2;
            button(c,cmd,rect(lane.left-22,centerY(lane)-9,19,18),L"",i==RowOverlay?(mute?Minus:Check):mute?MutedSpeaker:Speaker,true,mute?3:2);
        }
    }
    c.fill(rect(ruler.left-1,ruler.top,1,bottom-ruler.top),Edge);
    int saved=SaveDC(c.dc());
    IntersectClipRect(c.dc(),ruler.left,ruler.top,ruler.right,bottom);
    int total=length(project_.seq);
    int lo=std::max(0,project_.seq.workIn), hi=project_.seq.workOut<0?total:std::min(total,project_.seq.workOut);
    if((project_.seq.workIn>=0 || project_.seq.workOut>=0) && lo<hi) {
        c.fill(rect(px(lo),ruler.top+21,std::max(1,px(hi)-px(lo)),5),useRange_?TrackInk[RowTitle]:TrackFill[RowTitle]);
    }
    const int grid[]={6,12,24,48,120,240,360,720,1440,2880,7200,14400,28800,57600};
    int step=grid[sizeof(grid)/sizeof(grid[0])-1];
    for(int cell:grid) if(cell*pixelsPerFrame()>=63) { step=cell; break; }
    int from=(first_/step)*step;
    for(int t=from;t<=MaxFrames+step && px(t)<ruler.right;t+=step) {
        int gx=px(t);
        if(gx>=ruler.left-64) {
            c.fill(rect(gx,ruler.top+18,1,bottom-ruler.top-18),0x292e36);
            c.text(rect(gx+6,ruler.top,76,18),step<Fps?shortTime(t):rulerTime(t),Mono,Dim);
        }
        if(step>=12) for(int n=1;n<4;++n) c.fill(rect(px(t+step*n/4),ruler.bottom-4,1,3),0x3a444e);
    }
    int t=0;
    for(int i=0;i<int(project_.seq.video.size());++i) {
        const Clip& v=project_.seq.video[i];
        drawClip(c,v,RowVideo,i,t); drawClip(c,v,RowLinked,i,t);
        t+=v.len;
    }
    for(int l=1;l<LaneCount;++l) {
        const auto& list=laneOf(project_.seq,Lane(l));
        for(int i=0;i<int(list.size());++i) drawClip(c,list[i],laneRow(Lane(l)),i,list[i].pos);
    }
    for(size_t i=0;i<project_.seq.markers.size();++i) {
        const Marker& m=project_.seq.markers[i];
        int mark=px(m.frame);
        if(mark<ruler.left-3 || mark>=ruler.right) continue;
        c.fill(rect(mark-2,ruler.top+15,5,5),TrackInk[RowTitle]);
        int next=i+1<project_.seq.markers.size()?px(project_.seq.markers[i+1].frame):ruler.right;
        int available=std::min(103,next-mark-6);
        if(available>37) {
            RECT tag=rect(mark+4,ruler.top+13,available,12);
            c.fill(tag,TrackFill[RowTitle]);
            c.text(inset(tag,2,0),m.text,Small,TrackInk[RowTitle]);
        }
    }
    if(!total) {
        c.icon(Plus,ruler.left+13,centerY(ui_.lanes[RowVideo])-8,Quiet);
        c.text(rect(ruler.left+36,ui_.lanes[RowVideo].top,width(ruler)-50,height(ui_.lanes[RowVideo])),
            L"Arraste uma mídia da biblioteca para começar",Body,Dim);
    }
    if(assistMode_ && hasReport_ && sameTarget(project_,analyzed_)) {
        const auto suggest=[&](int frame,unsigned color) {
            int x=px(analyzed_.sequenceStart+frame-analyzed_.first);
            if(x<ruler.left || x>=ruler.right) return;
            c.fill(rect(x-1,ruler.bottom-4,3,4),color);
            for(int y=ruler.bottom;y<bottom;y+=8) c.fill(rect(x,y,1,3),color);
        };
        for(int f:report_.cuts) suggest(f,0x73bbfa);
        int row=analyzed_.clip.lane==Video?RowLinked:laneRow(analyzed_.clip.lane);
        const RECT& band=ui_.lanes[row];
        for(const AssistSpan& seg:report_.voice) {
            int left=px(analyzed_.sequenceStart+seg.first-analyzed_.first);
            int right=px(analyzed_.sequenceStart+seg.end-analyzed_.first);
            c.fill(rect(left,band.bottom-4,std::max(1,right-left),2),Mint);
        }
    }
    int head=px(head_);
    c.fill(rect(head,ruler.top+5,1,bottom-ruler.top-5),Accent);
    c.box(rect(head-4,ruler.top+3,9,11),Accent,Accent,3);
    if(drag_.moved && drag_.targetX>=ruler.left && drag_.targetX<ruler.right && drag_.targetRow>=0) {
        RECT dest=ui_.lanes[drag_.targetRow];
        c.fill(rect(drag_.targetX-1,dest.top,2,height(dest)),Accent);
        c.dot(drag_.targetX,dest.top+4,3,Accent);
    }
    RestoreDC(c.dc(),saved);
    c.fill(rect(r.left+1,bottom,width(r)-2,1),Edge);
    c.box(ui_.scroll,Screen,Screen,5);
    c.box(inset(scrollThumb(),0,1),maxScroll()?Soft:0x252b33,maxScroll()?Soft:0x252b33,5);
}
void App::drawStatus(Canvas& c) {
    RECT r=ui_.status;
    c.fill(r,Bar);
    c.fill(rect(r.left,r.top,r.right,1),Edge);
    int cy=centerY(r), x=r.right-105;
    c.dot(17,cy,3,job_?Purple:Accent);
    c.text(rect(27,r.top,std::max(1,x-201),height(r)),job_?progress_.label:status_,Small,Muted);
    c.text(rect(x-196,r.top,130,height(r)),eco_?L"6 QPS / ECO":L"12 QPS / PROXY",Mono,Dim,DT_RIGHT);
    button(c,CmdEco,rect(x-56,r.top+2,47,22),L"Eco",NoGlyph,true,eco_?3:4);
    button(c,CmdHelp,rect(x,r.top+2,92,22),L"Atalhos",Keyboard,true,4);
}
void App::drawProgress(Canvas& c) {
    if(!job_) return;
    RECT v=ui_.display;
    int w=std::min(414,width(v)-10), h=105;
    RECT r=rect(centerX(v)-w/2,centerY(v)-h/2,w,h);
    int x=r.left+13;
    c.box(r,Bar,Soft,10);
    c.icon(Bolt,x,r.top+12,Accent);
    c.text(rect(x+22,r.top+5,w-103,28),job_->kind==AssistJob?L"Analisando localmente":job_->kind==ExportJob?L"Exportando a sequência":L"Preparando a mídia",Strong,Text);
    c.text(rect(x,r.top+33,w-26,22),progress_.label,Small,Muted);
    RECT b=rect(x,r.top+63,w-26,5);
    c.box(b,Soft,Soft,5);
    c.fill(rect(x,b.top,width(b)*limit(progress_.percent,0,100)/100,5),Accent);
    c.text(rect(x,b.bottom+5,84,20),number(progress_.percent)+L"%",Mono,Accent);
    button(c,CmdCancel,rect(r.right-97,b.bottom+7,84,24),L"Cancelar",Cancel,!job_->canceled());
}
void App::paint() {
    PAINTSTRUCT ps;
    HDC paintDC=BeginPaint(hwnd_,&ps);
    if(!paintDC) return;
    RECT client;
    GetClientRect(hwnd_,&client);
    int w=width(client),h=height(client);
    if(w<=0 || h<=0) { EndPaint(hwnd_,&ps); return; }
    if(!back_) back_=CreateCompatibleDC(paintDC);
    if(back_ && (bw_!=w || bh_!=h || !bmp_)) {
        if(bmp_) { SelectObject(back_,baseBmp_); DeleteObject(bmp_); }
        bmp_=CreateCompatibleBitmap(paintDC,w,h);
        if(bmp_) baseBmp_=SelectObject(back_,bmp_);
        bw_=w; bh_=h;
    }
    HDC dc=back_ && bmp_?back_:paintDC;
    paintContent(dc,client,ps.rcPaint);
    if(dc!=paintDC) BitBlt(paintDC,ps.rcPaint.left,ps.rcPaint.top,width(ps.rcPaint),height(ps.rcPaint),dc,ps.rcPaint.left,ps.rcPaint.top,SRCCOPY);
    EndPaint(hwnd_,&ps);
}
void App::paintContent(HDC dc,RECT client,RECT dirty) {
    if(ui_.w!=width(client) || ui_.h!=height(client)) layout(width(client),height(client));
    int saved=SaveDC(dc);
    IntersectClipRect(dc,dirty.left,dirty.top,dirty.right,dirty.bottom);
    nigh::Canvas c(dc,fonts_);
    c.fill(client,Base);
    hits_.clear(); ui_.gain=rect(0,0,0,0);
    drawHeader(c); drawMedia(c); drawViewer(c); drawInspector(c); drawTimeline(c); drawStatus(c); drawProgress(c);
    RestoreDC(dc,saved);
}
}
