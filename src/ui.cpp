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
    int lib=limit(w/5,178,248);
    ui_.side=w>=945;
    int insp=ui_.side?limit(w/6,181,226):0;
    ui_.media=rect(padding,top,lib,upper);
    ui_.mediaList=rect(padding+8,top+63,lib-16,std::max(0,upper-63-49));
    ui_.rows=std::max(1,height(ui_.mediaList)/RowH);
    int mx=padding+lib+gap, mw=w-padding-mx-(ui_.side?insp+gap:0);
    ui_.viewer=rect(mx,top,mw,upper);
    ui_.display=rect(mx+11,top+44,mw-22,std::max(8,upper-103));
    ui_.transportSeek=rect(mx+13,ui_.display.bottom+8,mw-26,14);
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
    return limit(int(double(x-ui_.transportSeek.left)*monitorLength()/std::max(1,width(ui_.transportSeek))+.5),0,monitorLength());
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
    if(style==1 && enabled) { fg=Ink; bg=hover?0xdbf8a7:Accent; edge=bg; }
    else if(style==2 && !hover) { bg=Panel; edge=Panel; fg=enabled?Muted:Quiet; }
    else if(style==3 && enabled) { bg=hover?Raised:AccentSoft; edge=hover?Soft:0x405338; fg=Accent; }
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
    c.fill(rect(0,0,ui_.w,61),Bar);
    c.fill(rect(0,60,ui_.w,1),Edge);
    c.box(rect(16,13,34,34),Accent,Accent,8);
    c.icon(Logo,23,20,Ink,20);
    c.text(rect(60,11,122,23),L"Nigh Editor",Brand,Text);
    c.text(rect(61,33,127,13),L"EDIÇÃO DE VÍDEO",Label,Dim);
    c.line(182,15,182,46,Edge);
    int ex=ui_.w-16-133, imp=ex-10-108, saveX=imp-8-86, fileX=saveX-8-93;
    c.text(rect(197,10,fileX-207,17),L"PROJETO ATUAL",Label,Dim);
    int pW=std::max(22,fileX-213);
    int nameW=std::min(c.measure(project_.name,Strong)+5,pW-(demo_ || dirty_?17:0));
    c.text(rect(197,25,nameW,23),project_.name,Strong,Text);
    if(dirty_) c.dot(199+nameW,37,3,Accent);
    else if(demo_ && nameW+42<pW) pill(c,rect(203+nameW,29,41,17),L"DEMO",Accent,AccentSoft);
    button(c,CmdFile,rect(fileX,14,93,32),L"Arquivo",Folder);
    button(c,CmdSave,rect(saveX,14,86,32),L"Salvar",Disk);
    button(c,CmdImport,rect(imp,14,108,32),L"Importar",Plus);
    button(c,CmdExport,rect(ex,14,133,32),L"Exportar",Export,length(project_.seq)>0,1);
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
    RECT r=ui_.viewer, canvas=ui_.display;
    section(c,r,L"02",source_>=0?L"MONITOR DE FONTE":L"MONITOR DE PROGRAMA");
    RECT pr=rect(r.right-113,r.top+10,100,20);
    if(source_>=0) button(c,CmdVideoMode,pr,L"Sequência",NoGlyph,true,2);
    else if(width(r)>289) {
        c.box(pr,AccentSoft,AccentSoft,5);
        c.dot(pr.left+10,centerY(pr),2,Accent);
        c.text(rect(pr.left+18,pr.top,77,20),L"PROXY • 12 QPS",Label,Accent);
    }
    c.fill(canvas,Screen); c.border(canvas,Edge);
    int end=monitorLength(), mark=monitorHead();
    int frame=limit(mark,0,std::max(0,end-1));
    const Asset* a=source_>=0?&project_.assets[source_]:0;
    if(RectVisible(c.dc(),&canvas)) {
        if(cachedSource_!=source_ || cachedFrame_!=frame) {
            cachedSource_=source_; cachedFrame_=frame;
            if(!preview_.frame(project_,frame,source_,pixels_)) pixels_.clear();
        }
        if(!pixels_.empty()) {
            int w=std::min(width(canvas)-2,(height(canvas)-2)*16/9), h=w*9/16;
            c.image(rect(centerX(canvas)-w/2,centerY(canvas)-h/2,w,h),pixels_,ProxyW,ProxyH);
        } else {
            int cx=centerX(canvas),cy=centerY(canvas);
            bool audio=a && a->audio>=0;
            bool demo=!audio && canDemo();
            if(height(canvas)>157) {
                c.box(rect(cx-22,cy-66,44,44),audio?MintBg:AccentSoft,audio?MintBg:AccentSoft,10);
                c.icon(audio?MusicNote:Film,cx-12,cy-56,audio?Mint:Accent,24);
            }
            int ty=cy-(height(canvas)>157?19:height(canvas)>99?40:31);
            if(audio || height(canvas)>62)
                c.text(rect(canvas.left+10,audio && height(canvas)<=99?cy-15:ty,width(canvas)-20,29),
                    audio?a->name:L"Sua próxima edição",height(canvas)>99?Title:Strong,Text,DT_CENTER);
            if(height(canvas)>113) c.text(rect(canvas.left+10,ty+29,width(canvas)-20,22),
                audio?L"Fonte de áudio • marque In / Out para selecionar um trecho.":L"Comece com seus arquivos ou explore a demonstração.",Body,Muted,DT_CENTER);
            if(!audio && height(canvas)>37) {
                int ww=demo?250:124;
                int bx=cx-ww/2, yy=std::min(int(canvas.bottom)-31,ty+(height(canvas)>113?55:31));
                button(c,CmdImport,rect(bx,yy,124,26),L"Importar mídia",Plus,true,1);
                if(demo) button(c,CmdDemo,rect(bx+133,yy,117,26),L"Abrir demo",Play);
            }
        }
    } else if(end==0 && source_<0 && height(canvas)>37) {
        // Keep the empty-state buttons in the keyboard/hit map on partial repaints.
        bool demo=canDemo();
        int cx=centerX(canvas),cy=centerY(canvas);
        int ty=cy-(height(canvas)>157?19:height(canvas)>99?40:31);
        int bx=cx-(demo?250:124)/2, yy=std::min(int(canvas.bottom)-31,ty+(height(canvas)>113?55:31));
        button(c,CmdImport,rect(bx,yy,124,26),L"Importar mídia",Plus,true,1);
        if(demo) button(c,CmdDemo,rect(bx+133,yy,117,26),L"Abrir demo",Play);
    }
    RECT seek=ui_.transportSeek;
    int y=centerY(seek);
    c.fill(rect(seek.left,y,width(seek),2),Soft);
    int x=seek.left+(end>0?int(double(width(seek)-1)*mark/end):0);
    c.fill(rect(seek.left,y,x-seek.left,2),Accent);
    if(a && end>0) {
        const SourceRange& span=sourceRanges_[source_];
        if(span.first>=0 || span.last>=0) {
            int left=seek.left+std::max(0,span.first)*(width(seek)-1)/end;
            int right=seek.left+(span.last<0?end:span.last)*(width(seek)-1)/end;
            c.fill(rect(left,y-2,std::max(1,right-left),5),TrackInk[RowTitle]);
        }
    }
    c.dot(x,y+1,3,end?Accent:Quiet);
    int toolbar=r.bottom-39;
    int cx=centerX(r);
    if(width(r)>=450) {
        int w=width(r)>530?105:88;
        c.text(rect(r.left+13,toolbar+1,w,30),timecode(mark),MonoLarge,Accent);
        if(width(r)>555) c.text(rect(r.left+119,toolbar+1,104,30),L"/ "+timecode(end),Mono,Dim);
    } else c.text(rect(r.left+11,toolbar+1,81,30),shortTime(mark),Mono,Accent);
    button(c,CmdFirst,rect(cx-84,toolbar+3,27,26),L"",First,end>0,2);
    button(c,CmdPrev,rect(cx-53,toolbar+3,27,26),L"",Prev,end>0,2);
    button(c,CmdPlay,rect(cx-20,toolbar,40,32),L"",player_.active()?Pause:Play,end>0,1);
    button(c,CmdNext,rect(cx+27,toolbar+3,27,26),L"",Next,end>0,2);
    button(c,CmdLast,rect(cx+58,toolbar+3,27,26),L"",Last,end>0,2);
    if(width(r)>370) {
        if(a) {
            const SourceRange& range=sourceRanges_[source_];
            std::wstring mode=range.first>=0 || range.last>=0?L"IN / OUT":L"FONTE";
            pill(c,rect(r.right-74,toolbar+7,61,18),mode,TrackInk[RowTitle],TrackFill[RowTitle]);
        } else pill(c,rect(r.right-74,toolbar+7,61,18),L"24 QPS",Dim,Surface);
    }
}
void App::drawInspector(Canvas& c) {
    if(!ui_.side) return;
    RECT r=ui_.inspector;
    section(c,r,L"03",L"INSPETOR");
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
    c.dot(18,cy,3,job_?Purple:Accent);
    c.text(rect(29,r.top,std::max(1,x-215),height(r)),job_?progress_.label:status_,Small,Muted);
    c.text(rect(x-213,r.top,203,height(r)),L"WIN32   /   SEQUÊNCIA 24 QPS",Mono,Dim,DT_RIGHT);
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
    c.text(rect(x+22,r.top+5,w-103,28),job_->kind==ExportJob?L"Exportando o vídeo":L"Preparando a mídia",Strong,Text);
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
