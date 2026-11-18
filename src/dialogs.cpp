#include "dialogs.hpp"
#include <cerrno>
#include <cwchar>

namespace nigh {
namespace {
enum FieldType { Numeric,Text,Multi,Choice,Toggle };
struct Field {
    FieldType type;
    std::wstring label,text;
    std::vector<std::wstring> options;
    int value,lo,hi,initial;
    HWND control;
    Field(FieldType k,const wchar_t* name) : type(k),label(name),value(0),lo(0),hi(0),initial(0),control(0) {}
};
struct Form {
    const wchar_t* caption;
    std::wstring help;
    std::vector<Field> fields[2];
    const wchar_t* section[2];
    bool restore;
    int columns,columnWidth;
    Form(const wchar_t* title,const std::wstring& info,int cols=1,int w=270) : caption(title),help(info),
        section{0,0},restore(false),columns(cols),columnWidth(w) {}
    void integer(int col,const wchar_t* label,int value,int low,int high,int initial) {
        Field f(Numeric,label); f.value=value; f.lo=low; f.hi=high; f.initial=initial; fields[col].push_back(f);
    }
    void string(const wchar_t* label,const std::wstring& s,int max,bool multi) {
        Field f(multi?Multi:Text,label); f.text=s; f.hi=max; fields[0].push_back(f);
    }
    void choice(const wchar_t* label,int selected,std::initializer_list<std::wstring> items) {
        Field f(Choice,label); f.value=selected; f.options=items; fields[0].push_back(f);
    }
    void check(const wchar_t* label,bool enabled) {
        Field f(Toggle,label); f.value=enabled?1:0; fields[0].push_back(f);
    }
    int bodyHeight(int col) const {
        int h=section[col]?17:0;
        for(const Field& f:fields[col]) h+=f.type==Multi?57:20;
        return h;
    }
    int w() const { return 20+columnWidth*columns+(columns-1)*10; }
    int h() const { return 59+std::max(bodyHeight(0),bodyHeight(1)); }
};
struct EmptyTemplate { DLGTEMPLATE header; WORD tail[3]; };
static HWND item(HWND dlg,const wchar_t* klass,const wchar_t* label,DWORD style,int x,int y,int w,int h,int id=0,DWORD extra=0) {
    RECT r={x,y,x+w,y+h};
    MapDialogRect(dlg,&r);
    HWND child=CreateWindowExW(extra,klass,label,WS_CHILD|WS_VISIBLE|style,r.left,r.top,r.right-r.left,r.bottom-r.top,dlg,
        reinterpret_cast<HMENU>(INT_PTR(id)),GetModuleHandleW(0),0);
    if(child) SendMessageW(child,WM_SETFONT,reinterpret_cast<WPARAM>(GetStockObject(DEFAULT_GUI_FONT)),TRUE);
    return child;
}
static void init(HWND dlg,Form& form) {
    SetWindowTextW(dlg,form.caption);
    item(dlg,L"STATIC",form.help.c_str(),SS_NOPREFIX,10,8,form.w()-20,24);
    int id=100;
    for(int col=0;col<form.columns;++col) {
        int x=10+col*(form.columnWidth+10), y=34, w=form.columnWidth;
        if(form.section[col]) {
            item(dlg,L"STATIC",form.section[col],SS_NOPREFIX,x,y,w,14);
            y+=17;
        }
        for(Field& f:form.fields[col]) {
            if(f.type==Multi) {
                item(dlg,L"STATIC",f.label.c_str(),SS_NOPREFIX,x,y,w,12);
                std::wstring nativeText;
                for(wchar_t ch:f.text) { if(ch==L'\n') nativeText+=L'\r'; nativeText+=ch; }
                f.control=item(dlg,L"EDIT",nativeText.c_str(),WS_TABSTOP|WS_VSCROLL|ES_MULTILINE|ES_AUTOVSCROLL|ES_WANTRETURN,
                    x,y+12,w,40,id++,WS_EX_CLIENTEDGE);
                SendMessageW(f.control,EM_LIMITTEXT,f.hi*2,0);
                y+=57; continue;
            }
            if(f.type==Toggle) {
                f.control=item(dlg,L"BUTTON",f.label.c_str(),WS_TABSTOP|BS_AUTOCHECKBOX,x,y,w,14,id++);
                SendMessageW(f.control,BM_SETCHECK,f.value?BST_CHECKED:BST_UNCHECKED,0);
            } else {
                int fw=f.type==Numeric?44:f.type==Choice?101:w-70;
                item(dlg,L"STATIC",f.label.c_str(),SS_NOPREFIX,x,y+2,w-fw-4,13);
                if(f.type==Choice) {
                    f.control=item(dlg,L"COMBOBOX",L"",WS_TABSTOP|WS_VSCROLL|CBS_DROPDOWNLIST,x+w-fw,y,fw,70,id++);
                    for(const auto& text:f.options) SendMessageW(f.control,CB_ADDSTRING,0,reinterpret_cast<LPARAM>(text.c_str()));
                    SendMessageW(f.control,CB_SETCURSEL,f.value,0);
                } else {
                    std::wstring initial=f.type==Numeric?number(f.value):f.text;
                    f.control=item(dlg,L"EDIT",initial.c_str(),WS_TABSTOP|ES_AUTOHSCROLL|(f.type==Numeric?ES_RIGHT:0),
                        x+w-fw,y,fw,14,id++,WS_EX_CLIENTEDGE);
                    SendMessageW(f.control,EM_LIMITTEXT,f.type==Numeric?8:f.hi,0);
                }
            }
            y+=20;
        }
    }
    int y=form.h()-21;
    if(form.restore) item(dlg,L"BUTTON",L"Restaurar padrões",WS_TABSTOP,10,y,80,14,901);
    item(dlg,L"BUTTON",L"&OK",WS_TABSTOP|BS_DEFPUSHBUTTON,form.w()-110,y,45,14,IDOK);
    item(dlg,L"BUTTON",L"Cancelar",WS_TABSTOP,form.w()-60,y,50,14,IDCANCEL);
    SendMessageW(dlg,DM_SETDEFID,IDOK,0);
}
static bool read(HWND dlg,Form& form) {
    for(int col=0;col<form.columns;++col) for(Field& f:form.fields[col]) {
        if(!f.control) return false;
        if(f.type==Choice) {
            int n=int(SendMessageW(f.control,CB_GETCURSEL,0,0));
            if(n<0 || n>=int(f.options.size())) return false;
            f.value=n;
        } else if(f.type==Toggle) f.value=SendMessageW(f.control,BM_GETCHECK,0,0)==BST_CHECKED;
        else {
            wchar_t text[512];
            GetWindowTextW(f.control,text,512);
            if(f.type==Numeric) {
                wchar_t* end=0;
                errno=0;
                long value=std::wcstol(text,&end,10);
                if(errno || end==text || *end || value<f.lo || value>f.hi || (f.label==L"Dissolver (qd.)" && value==1)) {
                    std::wstring bounds=f.label==L"Dissolver (qd.)"?L"0 ou 2–72":number(f.lo)+L" a "+number(f.hi);
                    MessageBoxW(dlg,(f.label+L": digite um valor de "+bounds+L".").c_str(),form.caption,MB_OK|MB_ICONWARNING);
                    SetFocus(f.control); SendMessageW(f.control,EM_SETSEL,0,LPARAM(-1));
                    return false;
                }
                f.value=int(value);
            } else {
                std::wstring trim=text;
                trim.erase(std::remove(trim.begin(),trim.end(),L'\r'),trim.end());
                size_t first=trim.find_first_not_of(L" \t\n"), end=trim.find_last_not_of(L" \t\n");
                f.text=first==std::wstring::npos?L"":trim.substr(first,end-first+1);
                if(f.type==Multi && (f.text.empty() || f.text.size()>size_t(f.hi))) {
                    MessageBoxW(dlg,L"Digite entre 1 e 240 caracteres para o título.",form.caption,MB_OK|MB_ICONWARNING);
                    SetFocus(f.control);
                    return false;
                }
            }
        }
    }
    return true;
}
static INT_PTR CALLBACK procs(HWND dlg,UINT msg,WPARAM wp,LPARAM lp) {
    if(msg==WM_INITDIALOG) {
        Form* f=reinterpret_cast<Form*>(lp);
        SetWindowLongPtrW(dlg,DWLP_USER,reinterpret_cast<LONG_PTR>(f));
        init(dlg,*f);
        if(!f->fields[0].empty()) {
            HWND focus=f->fields[0][0].control;
            SetFocus(focus); SendMessageW(focus,EM_SETSEL,0,LPARAM(-1));
            return FALSE;
        }
        return TRUE;
    }
    Form* f=reinterpret_cast<Form*>(GetWindowLongPtrW(dlg,DWLP_USER));
    if(!f) return FALSE;
    if(msg==WM_CLOSE || (msg==WM_COMMAND && LOWORD(wp)==IDCANCEL)) { EndDialog(dlg,IDCANCEL); return TRUE; }
    if(msg==WM_COMMAND && LOWORD(wp)==IDOK) {
        if(read(dlg,*f)) EndDialog(dlg,IDOK);
        return TRUE;
    }
    if(msg==WM_COMMAND && LOWORD(wp)==901 && f->restore) {
        for(auto& list:f->fields) for(const Field& field:list) if(field.type==Numeric) SetWindowTextW(field.control,number(field.initial).c_str());
        return TRUE;
    }
    return FALSE;
}
static bool show(HWND owner,Form& form) {
    EmptyTemplate dialog=EmptyTemplate();
    dialog.header.style=DS_MODALFRAME|DS_CENTER|WS_CAPTION|WS_SYSMENU|WS_POPUP;
    dialog.header.cx=short(form.w()); dialog.header.cy=short(form.h());
    return DialogBoxIndirectParamW(GetModuleHandleW(0),&dialog.header,owner,procs,reinterpret_cast<LPARAM>(&form))==IDOK;
}
}
bool editEffects(HWND owner,Lane lane,bool sound,Clip& c) {
    bool picture=lane==Video || lane==Overlay;
    const Effects& fx=c.fx;
    Form f(L"Propriedades do clipe",L"1 segundo = 24 quadros. X e Y deslocam o centro; valores negativos movem à esquerda ou para cima.",
        picture && sound?2:1,picture && sound?150:230);
    f.restore=true;
    if(picture) {
        f.section[0]=L"IMAGEM";
        f.integer(0,L"Escala (%)",fx.scale,25,200,100);
        f.integer(0,L"Posição X (%)",fx.x,-100,100,0);
        f.integer(0,L"Posição Y (%)",fx.y,-100,100,0);
        f.integer(0,L"Opacidade (%)",fx.opacity,0,100,100);
        f.integer(0,L"Brilho",fx.brightness,-60,60,0);
        f.integer(0,L"Contraste (%)",fx.contrast,50,150,100);
        f.integer(0,L"Saturação (%)",fx.saturation,0,200,100);
        if(lane==Video) f.integer(0,L"Dissolver (qd.)",fx.dissolve,0,MaxDissolve,0);
    } else f.help=L"1 segundo = 24 quadros. O pan vai de −100 (esquerda) a +100 (direita).";
    int col=picture?1:0;
    if(sound) {
        f.section[col]=L"ÁUDIO";
        f.integer(col,L"Volume (%)",c.gain,0,200,100);
        f.integer(col,L"Pan estéreo",fx.pan,-100,100,0);
        f.integer(col,L"Fade de entrada (qd.)",fx.fadeIn,0,MaxFade,0);
        f.integer(col,L"Fade de saída (qd.)",fx.fadeOut,0,MaxFade,0);
    }
    if(!show(owner,f)) return false;
    const auto& fields=f.fields[0];
    if(picture) {
        c.fx.scale=fields[0].value; c.fx.x=fields[1].value; c.fx.y=fields[2].value; c.fx.opacity=fields[3].value;
        c.fx.brightness=fields[4].value; c.fx.contrast=fields[5].value; c.fx.saturation=fields[6].value;
        if(lane==Video) c.fx.dissolve=fields[7].value;
    }
    if(sound) {
        const auto& audio=f.fields[col];
        c.gain=audio[0].value; c.fx.pan=audio[1].value; c.fx.fadeIn=audio[2].value; c.fx.fadeOut=audio[3].value;
    }
    return true;
}
bool editTitle(HWND owner,bool existing,int maxLength,Clip& c) {
    Form f(existing?L"Editar título":L"Novo título",L"Insira um título ou legenda gravados no vídeo. 1 segundo = 24 quadros.");
    f.string(L"Texto (até 240 caracteres)",c.title.text,MaxTitle,true);
    f.choice(L"Posição",c.title.position,{L"Inferior",L"Centro",L"Superior"});
    f.integer(0,L"Tamanho (% da altura)",c.title.size,3,13,6);
    f.choice(L"Cor",c.title.color,{L"Branco",L"Dourado",L"Azul",L"Verde"});
    f.check(L"Fundo escuro semitransparente",c.title.box);
    f.integer(0,L"Duração (quadros)",c.len,1,maxLength,72);
    if(!show(owner,f)) return false;
    const auto& a=f.fields[0];
    c.title.text=a[0].text; c.title.position=a[1].value; c.title.size=a[2].value; c.title.color=a[3].value;
    c.title.box=a[4].value!=0; c.len=a[5].value;
    return true;
}
bool editMarker(HWND owner,int frame,std::wstring& text) {
    Form f(L"Marcador de sequência",timecode(frame)+L" • Apague o texto para remover o marcador.",1,235);
    f.string(L"Nome",text,MaxMarkerText,false);
    if(!show(owner,f)) return false;
    text=f.fields[0][0].text;
    return true;
}
}
