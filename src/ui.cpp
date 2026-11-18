#include "app.hpp"
#include <cmath>

namespace nigh {
using namespace palette;
static const int RowH=57;
static std::wstring shortTime(int t) { return timecode(t).substr(t>=3600*Fps?0:3); }
static std::wstring rulerTime(int t) { std::wstring s=shortTime(t); return s.substr(0,s.size()-3); }
static unsigned quietClip(int row) { return row==0?PurpleBg:row==1?BlueBg:MintBg; }
static unsigned brightClip(int row) { return row==0?Purple:row==1?Blue:Mint; }

void App::layout(int w,int h) {
    ui_.w=w; ui_.h=h;
    const int padding=12, gap=10, top=72, footer=26;
    int tlH=limit(h*35/100,188,265), tlY=h-footer-tlH;
    int upper=std::max(0,tlY-top-gap);
    int lib=limit(w/5,178,248);
    ui_.side=w>=975;
    int insp=ui_.side?limit(w/6,194,226):0;
    ui_.media=rect(padding,top,lib,upper);
    ui_.mediaList=rect(padding+8,top+65,lib-16,std::max(0,upper-65-58));
    ui_.rows=std::max(1,height(ui_.mediaList)/RowH);
    int mx=padding+lib+gap, mw=w-padding-mx-(ui_.side?insp+gap:0);
    ui_.viewer=rect(mx,top,mw,upper);
    ui_.display=rect(mx+11,top+44,mw-22,std::max(8,upper-108));
    ui_.transportSeek=rect(mx+13,ui_.display.bottom+8,mw-26,14);
    ui_.inspector=rect(w-padding-insp,top,insp,upper);
    ui_.gain=rect(0,0,0,0);
    ui_.status=rect(0,h-footer,w,footer);
    ui_.timeline=rect(padding,tlY,w-padding*2,tlH-7);
    ui_.labelW=w<870?99:120;
    int rx=ui_.timeline.left+ui_.labelW, rw=ui_.timeline.right-rx-1;
    ui_.ruler=rect(rx,tlY+43,rw,32);
    int body=ui_.timeline.bottom-17-ui_.ruler.bottom;
    int v=body*42/100, a=(body-v)/2;
    ui_.lanes[0]=rect(rx,ui_.ruler.bottom,rw,v);
    ui_.lanes[1]=rect(rx,ui_.lanes[0].bottom,rw,a);
    ui_.lanes[2]=rect(rx,ui_.lanes[1].bottom,rw,body-v-a);
    ui_.scroll=rect(rx+7,ui_.lanes[2].bottom+4,rw-14,9);
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
    button(c,CmdExport,rect(ex,14,133,32),L"Exportar MP4",Export,!project_.seq.video.empty(),1);
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
        c.text(rect(x,row.top+27,w+5,18),shortTime(a.frames)+L"  ·  "+(a.video>=0?L"VÍDEO":L"ÁUDIO"),Small,Dim);
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
const Asset* App::currentAsset(int& f) const {
    if(source_>=0 && source_<int(project_.assets.size())) { f=limit(sourceHead_,0,project_.assets[source_].frames-1); return &project_.assets[source_]; }
    int pos=std::min(head_,std::max(0,length(project_.seq)-1));
    Selection sel=at(project_.seq,Video,pos);
    if(const Clip* clip=get(project_.seq,sel)) {
        f=clip->in+pos-startOf(project_.seq,sel);
        return &project_.assets[clip->asset];
    }
    return 0;
}
void App::drawViewer(Canvas& c) {
    RECT r=ui_.viewer, canvas=ui_.display;
    section(c,r,L"02",source_>=0?L"MONITOR DE FONTE":L"MONITOR DE PROGRAMA");
    if(width(r)>306) {
        RECT pr=rect(r.right-107,r.top+10,94,19);
        if(source_>=0) button(c,CmdVideoMode,pr,L"Sequência",Target,true,2);
        else {
            c.box(pr,AccentSoft,AccentSoft,5);
            c.dot(pr.left+10,centerY(pr),2,Accent);
            c.text(rect(pr.left+18,pr.top,74,19),L"PRÉVIA LEVE",Label,Accent);
        }
    }
    c.fill(canvas,Screen); c.border(canvas,Edge);
    int f=0;
    const Asset* a=currentAsset(f);
    if(a && a->proxy && a->video>=0 && !a->proxy->frames.empty()) {
        const Proxy& proxy=*a->proxy;
        int i=limit(f/2,0,int(proxy.frames.size())-1);
        if(frameProxy_!=&proxy || cachedFrame_!=i) {
            frameProxy_=&proxy; cachedFrame_=i;
            if(!reader_.frame(proxy,i,pixels_)) pixels_.clear();
        }
        int w=std::min(width(canvas)-2,(height(canvas)-2)*16/9), h=w*9/16;
        RECT pic=rect(centerX(canvas)-w/2,centerY(canvas)-h/2,w,h);
        c.image(pic,pixels_,ProxyW,ProxyH);
        if(width(pic)>240 && height(pic)>115) {
            RECT stamp=rect(pic.right-81,pic.bottom-25,72,17);
            c.box(stamp,0x171c1b,0x171c1b,5);
            c.text(stamp,L"320 × 180",Mono,0xe3e7de,DT_CENTER);
        }
    } else {
        int cx=centerX(canvas), cy=centerY(canvas);
        bool sound=(a && a->audio>=0) || length(project_.seq)>0;
        if(height(canvas)>158) {
            c.box(rect(cx-22,cy-66,44,44),sound?MintBg:AccentSoft,sound?MintBg:AccentSoft,10);
            c.icon(sound?MusicNote:Film,cx-12,cy-56,sound?Mint:Accent,24);
        }
        int ty=cy-(height(canvas)>158?20:height(canvas)>87?42:15);
        c.text(rect(canvas.left+10,ty,width(canvas)-20,29),sound?(a?a->name:L"Som da sequência"):L"Sua próxima edição",height(canvas)>80?Title:Strong,Text,DT_CENTER);
        if(height(canvas)>103) c.text(rect(canvas.left+10,ty+31,width(canvas)-20,22),
            sound?L"Nenhum quadro de vídeo nesta posição.":L"Comece com seus arquivos ou explore a demonstração.",Body,Muted,DT_CENTER);
        if(!sound && height(canvas)>87) {
            bool demo=canDemo();
            int ww=demo?250:123;
            int bx=cx-ww/2, yy=std::min(int(canvas.bottom)-34,ty+59);
            button(c,CmdImport,rect(bx,yy,124,28),L"Importar mídia",Plus,true,1);
            if(demo) button(c,CmdDemo,rect(bx+133,yy,117,28),L"Abrir demo",Play);
        }
    }
    RECT seek=ui_.transportSeek;
    int y=centerY(seek), end=monitorLength(), mark=monitorHead();
    c.fill(rect(seek.left,y,width(seek),2),Soft);
    int x=seek.left+(end>0?int(double(width(seek)-1)*mark/end):0);
    c.fill(rect(seek.left,y,x-seek.left,2),Accent);
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
    if(width(r)>380) pill(c,rect(r.right-72,toolbar+7,59,18),L"12 QPS",Dim,Surface);
}
void App::drawInspector(Canvas& c) {
    if(!ui_.side) return;
    RECT r=ui_.inspector;
    section(c,r,L"03",L"INSPETOR");
    const Clip* clip=get(project_.seq,selected_);
    int left=r.left+13, ww=width(r)-26;
    if(!clip) {
        c.icon(Target,left,r.top+56,Accent);
        c.text(rect(left+25,r.top+49,ww-26,31),L"Detalhes do clipe",Strong,Text);
        c.paragraph(rect(left,r.top+88,ww,62),L"Selecione um clipe na linha do tempo para ver seus pontos de corte e ajustar o volume.",Body,Muted);
        if(height(r)>225) {
            c.line(left,r.top+158,r.right-12,r.top+158,Edge);
            c.text(rect(left,r.top+173,ww,19),L"SEQUÊNCIA",Label,Dim);
            c.text(rect(left,r.top+194,ww,23),L"24 quadros por segundo",Body,Text);
        }
        return;
    }
    const Asset& a=project_.assets[clip->asset];
    unsigned bg=selected_.lane==Video?PurpleBg:MintBg, fg=selected_.lane==Video?Purple:Mint;
    int top=r.top+50;
    pill(c,rect(left,top,selected_.lane==Video?108:105,19),selected_.lane==Video?L"CLIPE DE VÍDEO":L"CLIPE DE ÁUDIO",fg,bg);
    c.text(rect(left,top+25,ww,25),a.name,Brand,Text);
    c.text(rect(left,top+53,ww,17),selected_.lane==Video?L"FAIXAS V1 / A1":L"FAIXA A2",Label,Dim);
    int y=top+78;
    struct Data { const wchar_t* label; int frame; } data[]={{L"Posição",startOf(project_.seq,selected_)},
        {L"Entrada",clip->in},{L"Saída",clip->in+clip->len},{L"Duração",clip->len}};
    for(const Data& d:data) {
        if(y+24>r.bottom-6) return;
        c.fill(rect(left,y,ww,1),Edge);
        c.text(rect(left,y+1,ww-90,22),d.label,Body,Muted);
        c.text(rect(left+ww-88,y+1,88,22),timecode(d.frame),Mono,Text,DT_RIGHT);
        y+=25;
    }
    if(y+65>=r.bottom) return;
    c.fill(rect(left,y,ww,1),Edge);
    bool has=a.audio>=0;
    c.text(rect(left,y+7,ww-49,19),L"VOLUME DO CLIPE",Label,Dim);
    int gain=drag_.type==AdjustGain?limit((mouseX_-left-30)*200/std::max(1,ww-61),0,200):clip->gain;
    c.text(rect(left+ww-52,y+7,52,19),has?number(gain)+L"%":L"—",Mono,has?Accent:Dim,DT_RIGHT);
    button(c,CmdGainDown,rect(left,y+31,24,24),L"",Minus,has,2);
    button(c,CmdGainUp,rect(left+ww-24,y+31,24,24),L"",Plus,has,2);
    ui_.gain=has && !job_?rect(left+30,y+32,ww-60,21):rect(0,0,0,0);
    RECT bar=rect(left+30,y+41,ww-60,3);
    c.box(bar,Soft,Soft,3);
    int x=bar.left+(width(bar)-1)*gain/200;
    c.fill(rect(bar.left,bar.top,x-bar.left,3),has?fg:Soft);
    c.dot(x,bar.top+1,4,has?fg:Quiet);
    if(y+89<r.bottom) c.text(rect(left,y+62,ww,17),has?L"100% mantém o volume original.":L"Este arquivo não tem áudio.",Small,Dim);
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
    const Asset& a=project_.assets[clip.asset];
    int x=px(start), x2=px(start+clip.len);
    if(x2<slot.left || x>=slot.right) return;
    bool sel=selected_.index==i && (selected_.lane==Video?row<2:row==2);
    unsigned bg=quietClip(row), fg=brightClip(row), edge=sel?fg:row==0?0x56476c:row==1?0x354f61:0x355d53;
    bool mute=(row==1 && project_.seq.muteA1)||(row==2 && project_.seq.muteA2);
    if(sel && row==0) bg=0x423661;
    if(mute) { bg=Surface; fg=Quiet; if(!sel) edge=Edge; }
    RECT box=rect(x+1,slot.top+4,std::max(1,x2-x-2),height(slot)-8);
    c.box(box,bg,edge,6);
    int w=width(box);
    if(w<6) return;
    c.fill(rect(box.left+2,box.top+4,2,height(box)-8),fg);
    if(row==0) {
        int picW=height(box)>=41?54:43;
        if(w>117 && a.proxy && !a.proxy->thumb.empty()) {
            int ph=picW*9/16;
            c.image(rect(box.left+8,centerY(box)-ph/2,picW,ph),a.proxy->thumb,88,50);
        } else picW=0;
        int tx=box.left+(picW?picW+15:8), tw=std::max(1,int(box.right)-tx-6);
        int cy=centerY(box);
        c.text(rect(tx,cy-(height(box)>39?17:11),tw,21),a.name,Strong,Text);
        if(height(box)>39) c.text(rect(tx,cy+3,tw,17),shortTime(clip.len)+L"  ·  V1",Small,fg);
    } else if(row==1) {
        if(a.audio>=0) wave(c,inset(box,8,5),clip,a,fg);
        else c.text(inset(box,8,0),L"Sem áudio",Small,Dim);
    } else {
        if(w>52) {
            RECT waves=rect(box.left+8,box.top+height(box)/2+2,width(box)-16,height(box)/2-3);
            wave(c,waves,clip,a,fg);
            c.text(rect(box.left+8,box.top,w-16,height(box)/2+2),a.name,Label,mute?Dim:Mint);
        } else wave(c,inset(box,8,5),clip,a,fg);
    }
    if(sel && w>18 && row!=1) {
        int cy=centerY(box);
        c.line(box.left+5,cy-4,box.left+5,cy+5,fg);
        c.line(box.right-5,cy-4,box.right-5,cy+5,fg);
    }
}
void App::drawTimeline(Canvas& c) {
    RECT r=ui_.timeline;
    c.fill(r,Panel); c.border(r,Edge);
    int left=r.left+12, top=r.top+7;
    pill(c,rect(left,top+5,24,18),L"04",Accent,AccentSoft);
    c.text(rect(left+31,top+1,110,28),L"LINHA DO TEMPO",Label,Text);
    int x=left+144;
    button(c,CmdUndo,rect(x,top,29,29),L"",Undo,!undo_.empty(),2);
    button(c,CmdRedo,rect(x+31,top,29,29),L"",Redo,!redo_.empty(),2);
    c.line(x+67,top+4,x+67,top+25,Edge);
    const Clip* clip=get(project_.seq,selected_);
    int start=startOf(project_.seq,selected_);
    bool edit=clip && head_>=start && head_<=start+clip->len;
    button(c,CmdSplit,rect(x+74,top,75,29),L"Dividir",Scissors,edit && head_>start && head_<start+(clip?clip->len:0));
    button(c,CmdIn,rect(x+154,top,50,29),L"In",TrimLeft,edit && head_>start && head_<start+(clip?clip->len:0),2);
    button(c,CmdOut,rect(x+207,top,53,29),L"Out",TrimRight,edit && head_>start && head_<start+(clip?clip->len:0),2);
    button(c,CmdDelete,rect(x+265,top,31,29),L"",Bin,clip!=0,2);
    int zx=r.right-142;
    if(zx>x+395) {
        c.text(rect(zx-176,top,166,29),L"DURAÇÃO  "+timecode(length(project_.seq)),Mono,Muted,DT_RIGHT);
    }
    button(c,CmdZoomOut,rect(zx,top,28,29),L"",Minus,zoom_>-6,2);
    button(c,CmdFit,rect(zx+31,top,63,29),zoom_==0?L"Auto":number(int(100*std::pow(1.5,zoom_)))+L"%",NoGlyph,true,zoom_==0?3:0);
    button(c,CmdZoomIn,rect(zx+98,top,28,29),L"",Plus,zoom_<8,2);
    c.fill(rect(r.left+1,r.top+42,width(r)-2,1),Edge);
    RECT ruler=ui_.ruler;
    c.fill(ruler,Screen);
    c.fill(rect(r.left+1,ruler.top,ui_.labelW-1,height(ruler)),Bar);
    c.icon(Cursor,left+1,ruler.top+8,Dim);
    c.text(rect(left+24,ruler.top,ui_.labelW-35,height(ruler)),L"Edição",Small,Dim);
    static const wchar_t* names[]={L"Vídeo",L"Original",L"Trilha"};
    static const wchar_t* lanes[]={L"V1",L"A1",L"A2"};
    for(int i=0;i<3;++i) {
        RECT lane=ui_.lanes[i];
        c.fill(lane,i%2?0x16191e:0x13161b);
        c.fill(rect(r.left+1,lane.top,ui_.labelW-1,height(lane)),i%2?Bar:Panel);
        c.fill(rect(r.left+1,lane.top,width(r)-2,1),Edge);
        pill(c,rect(left,centerY(lane)-10,27,20),lanes[i],brightClip(i),quietClip(i));
        c.text(rect(left+34,lane.top,ui_.labelW-53,height(lane)),names[i],Small,Muted);
        if(i) {
            bool mute=i==1?project_.seq.muteA1:project_.seq.muteA2;
            button(c,i==1?CmdMute1:CmdMute2,rect(lane.left-27,centerY(lane)-11,22,22),L"",mute?MutedSpeaker:Speaker,true,mute?3:2);
        }
    }
    c.fill(rect(ruler.left-1,ruler.top,1,ui_.lanes[2].bottom-ruler.top),Edge);
    int saved=SaveDC(c.dc());
    IntersectClipRect(c.dc(),ruler.left,ruler.top,ruler.right,ui_.lanes[2].bottom);
    const int grid[]={6,12,24,48,120,240,360,720,1440,2880,7200,14400,28800,57600};
    int step=grid[sizeof(grid)/sizeof(grid[0])-1];
    for(int cell:grid) if(cell*pixelsPerFrame()>=63) { step=cell; break; }
    int from=(first_/step)*step;
    for(int t=from;t<=MaxFrames+step && px(t)<ruler.right;t+=step) {
        int gx=px(t);
        if(gx>=ruler.left-64) {
            c.fill(rect(gx,ruler.top+20,1,ui_.lanes[2].bottom-ruler.top-20),0x292e36);
            c.text(rect(gx+6,ruler.top+1,75,20),step<Fps?shortTime(t):rulerTime(t),Mono,Dim);
        }
        if(step>=12) for(int n=1;n<4;++n) c.fill(rect(px(t+step*n/4),ruler.bottom-6,1,5),0x3a444e);
    }
    int t=0;
    for(int i=0;i<int(project_.seq.video.size());++i) {
        const Clip& v=project_.seq.video[i];
        drawClip(c,v,0,i,t); drawClip(c,v,1,i,t);
        t+=v.len;
    }
    for(int i=0;i<int(project_.seq.music.size());++i) {
        const Clip& a=project_.seq.music[i]; drawClip(c,a,2,i,a.pos);
    }
    if(project_.seq.video.empty() && project_.seq.music.empty()) {
        c.icon(Plus,ruler.left+16,centerY(ui_.lanes[0])-8,Quiet);
        c.text(rect(ruler.left+40,ui_.lanes[0].top,width(ruler)-55,height(ui_.lanes[0])),L"Arraste um arquivo da biblioteca para começar",Body,Dim);
    }
    int head=px(head_);
    c.fill(rect(head,ruler.top+7,1,ui_.lanes[2].bottom-ruler.top-7),Accent);
    c.box(rect(head-5,ruler.top+5,11,12),Accent,Accent,4);
    if(drag_.moved && drag_.targetX>=ruler.left && drag_.targetX<ruler.right) {
        c.fill(rect(drag_.targetX-1,ruler.bottom,2,ui_.lanes[2].bottom-ruler.bottom),Purple);
        c.dot(drag_.targetX,ruler.bottom+4,3,Purple);
    }
    RestoreDC(c.dc(),saved);
    c.fill(rect(r.left+1,ui_.lanes[2].bottom,width(r)-2,1),Edge);
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
    if(ui_.w!=w || ui_.h!=h) layout(w,h);
    if(!back_) back_=CreateCompatibleDC(paintDC);
    if(back_ && (bw_!=w || bh_!=h || !bmp_)) {
        if(bmp_) { SelectObject(back_,baseBmp_); DeleteObject(bmp_); }
        bmp_=CreateCompatibleBitmap(paintDC,w,h);
        if(bmp_) baseBmp_=SelectObject(back_,bmp_);
        bw_=w; bh_=h;
    }
    HDC dc=back_ && bmp_?back_:paintDC;
    int saved=SaveDC(dc);
    IntersectClipRect(dc,ps.rcPaint.left,ps.rcPaint.top,ps.rcPaint.right,ps.rcPaint.bottom);
    nigh::Canvas c(dc,fonts_);
    c.fill(client,Base);
    hits_.clear(); ui_.gain=rect(0,0,0,0);
    drawHeader(c); drawMedia(c); drawViewer(c); drawInspector(c); drawTimeline(c); drawStatus(c); drawProgress(c);
    RestoreDC(dc,saved);
    if(dc!=paintDC) BitBlt(paintDC,ps.rcPaint.left,ps.rcPaint.top,width(ps.rcPaint),height(ps.rcPaint),dc,ps.rcPaint.left,ps.rcPaint.top,SRCCOPY);
    EndPaint(hwnd_,&ps);
}
}
