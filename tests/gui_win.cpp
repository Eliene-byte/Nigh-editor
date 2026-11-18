#include "win.hpp"
#include <cstdio>
#include <cwchar>
#include <cstring>

using namespace nigh;
namespace {
struct Child {
    PROCESS_INFORMATION info;
    Child() : info() {}
    ~Child() {
        if(info.hProcess) {
            if(WaitForSingleObject(info.hProcess,500)!=WAIT_OBJECT_0) TerminateProcess(info.hProcess,1);
            CloseHandle(info.hProcess);
            CloseHandle(info.hThread);
        }
    }
    bool start(const std::wstring& exe,const std::wstring& arg) {
        STARTUPINFOW si;
        std::memset(&si,0,sizeof(si)); si.cb=sizeof(si);
        std::wstring cmd=L"\""+exe+L"\" "+arg;
        return CreateProcessW(exe.c_str(),&cmd[0],0,0,FALSE,0,0,0,&si,&info)!=FALSE;
    }
};
struct Query { DWORD pid; const wchar_t* klass; const wchar_t* text; HWND result; };
static BOOL CALLBACK match(HWND h,LPARAM param) {
    Query& q=*reinterpret_cast<Query*>(param);
    DWORD id=0;
    GetWindowThreadProcessId(h,&id);
    if(id!=q.pid || !IsWindowVisible(h)) return TRUE;
    wchar_t title[256],cls[80];
    GetClassNameW(h,cls,80);
    GetWindowTextW(h,title,256);
    if(std::wcscmp(cls,q.klass)==0 && std::wcsstr(title,q.text)) { q.result=h; return FALSE; }
    return TRUE;
}
static HWND waitFor(const Child& child,const wchar_t* klass,const wchar_t* title,DWORD ms=12000) {
    DWORD start=GetTickCount();
    do {
        Query q={child.info.dwProcessId,klass,title,0};
        EnumWindows(match,reinterpret_cast<LPARAM>(&q));
        if(q.result) return q.result;
    } while(WaitForSingleObject(child.info.hProcess,40)==WAIT_TIMEOUT && GetTickCount()-start<ms);
    return 0;
}
static bool change(HWND h,int id,const wchar_t* value) {
    HWND target=GetDlgItem(h,id);
    DWORD_PTR written=0;
    return target && SendMessageTimeoutW(target,WM_SETTEXT,0,reinterpret_cast<LPARAM>(value),SMTO_ABORTIFHUNG,2000,&written) && written;
}
static bool negativeInt(HWND h,int id) {
    HWND control=GetDlgItem(h,id);
    wchar_t value[32]={};
    DWORD_PTR got=0;
    return control && SendMessageTimeoutW(control,WM_GETTEXT,32,reinterpret_cast<LPARAM>(value),SMTO_ABORTIFHUNG,2000,&got) && got>1 &&
        std::wcstol(value,0,10)<0;
}
static bool snapshot(HWND h,const std::wstring& path) {
    RECT rect;
    if(!GetClientRect(h,&rect)) return false;
    int w=rect.right,hgt=rect.bottom;
    if(w<740 || hgt<470 || w>2000 || hgt>1500) return false;
    BITMAPINFO bi;
    std::memset(&bi,0,sizeof(bi));
    bi.bmiHeader.biSize=sizeof(bi.bmiHeader); bi.bmiHeader.biWidth=w; bi.bmiHeader.biHeight=-hgt;
    bi.bmiHeader.biPlanes=1; bi.bmiHeader.biBitCount=32; bi.bmiHeader.biSizeImage=w*hgt*4;
    void* pixels=0;
    HDC dc=CreateCompatibleDC(0);
    HBITMAP bmp=dc?CreateDIBSection(dc,&bi,DIB_RGB_COLORS,&pixels,0,0):0;
    bool ok=false;
    if(bmp && pixels) {
        HGDIOBJ old=SelectObject(dc,bmp);
        std::memset(pixels,0,bi.bmiHeader.biSizeImage);
        if(PrintWindow(h,dc,PW_CLIENTONLY)) {
            GdiFlush();
            uint8_t* raw=static_cast<uint8_t*>(pixels);
            // A real paint contains different header/timeline colors and light toolbar text.
            unsigned bright=0;
            for(DWORD i=0;i<bi.bmiHeader.biSizeImage;i+=4) if(raw[i]>140 && raw[i+1]>140 && raw[i+2]>140) ++bright;
            BITMAPFILEHEADER file={0x4d42,DWORD(sizeof(file)+sizeof(bi.bmiHeader)+bi.bmiHeader.biSizeImage),0,0,
                sizeof(file)+sizeof(bi.bmiHeader)};
            File out(path,true);
            ok=bright>90 && out.put(&file,sizeof(file)) && out.put(&bi.bmiHeader,sizeof(bi.bmiHeader)) && out.put(pixels,bi.bmiHeader.biSizeImage);
        }
        SelectObject(dc,old);
        DeleteObject(bmp);
    }
    if(dc) DeleteDC(dc);
    if(!ok) DeleteFileW(path.c_str());
    return ok;
}
static int fail(const char* step) { std::fprintf(stderr,"GUI smoke: %s\n",step); return 1; }
}
extern "C" int wmain(int argc,wchar_t** argv) {
    if(argc!=5) return fail("use: gui.exe editor.exe screenshot-assist.bmp project.nigh media.mp4");
    const std::wstring exe=fullPath(argv[1]), shot=fullPath(argv[2]), path=fullPath(argv[3]), media=fullPath(argv[4]);
    const wchar_t* mainClass=L"NighEditorNative";
    {
        Child demo;
        if(!demo.start(exe,L"--demo")) return fail("start exact packaged editor");
        HWND window=waitFor(demo,mainClass,L"Maré baixa",30000);
        if(!window) return fail("load packaged layered demo");
        SetWindowPos(window,0,0,0,1024,600,SWP_NOMOVE|SWP_NOZORDER|SWP_NOACTIVATE);
        PostMessageW(window,WM_KEYDOWN,VK_F2,0);
        HWND effects=waitFor(demo,L"#32770",L"Propriedades do clipe");
        if(!effects || !change(effects,103,L"75")) return fail("open overlay properties and set opacity");
        PostMessageW(effects,WM_COMMAND,IDOK,0);
        if(!waitFor(demo,mainClass,L"Maré baixa *")) return fail("apply properties and mark the project dirty");
        PostMessageW(window,WM_KEYDOWN,'T',0);
        HWND title=waitFor(demo,L"#32770",L"Novo título");
        if(!title || !change(title,100,L"Título do teste")) return fail("open and fill title dialog");
        PostMessageW(title,WM_COMMAND,IDOK,0);
        Sleep(150);
        if(IsWindow(title)) return fail("title dialog did not accept valid data");
        PostMessageW(window,WM_CLOSE,0,0);
        HWND confirm=waitFor(demo,L"#32770",L"Nigh Editor");
        if(!confirm) return fail("unsaved change prompt");
        PostMessageW(confirm,WM_COMMAND,IDNO,0);
        if(WaitForSingleObject(demo.info.hProcess,6000)!=WAIT_OBJECT_0) return fail("close demo cleanly");
    }
    {
        Child editor;
        if(!editor.start(exe,L"--assist-demo")) return fail("start bundled assistant demo");
        HWND window=waitFor(editor,mainClass,L"Caderno de campo",30000);
        if(!window) return fail("load Caderno de campo");
        SetWindowPos(window,0,0,0,1296,792,SWP_NOMOVE|SWP_NOZORDER|SWP_NOACTIVATE);
        // Analysis is asynchronous. F7 is ignored while the worker is active
        // and applies the valid proposal once available.
        DWORD begin=GetTickCount();
        bool applied=false;
        do {
            PostMessageW(window,WM_KEYDOWN,VK_F7,0);
            applied=waitFor(editor,mainClass,L"Caderno de campo *",80)!=0;
        } while(!applied && GetTickCount()-begin<15000);
        if(!applied) return fail("wait for local analysis and apply the face suggestion");
        PostMessageW(window,WM_KEYDOWN,VK_F6,0); // review the first proposed speech interval
        Sleep(140);
        if(!snapshot(window,shot)) std::puts("GUI: screenshot unavailable in this desktop session");
        SetWindowPos(window,0,0,0,1024,600,SWP_NOMOVE|SWP_NOZORDER|SWP_NOACTIVATE);
        PostMessageW(window,WM_KEYDOWN,VK_F8,0);
        PostMessageW(window,WM_KEYDOWN,VK_F2,0);
        HWND effects=waitFor(editor,L"#32770",L"Propriedades do clipe");
        if(!effects || !negativeInt(effects,101)) return fail("assistant edits normal X property in compact Eco layout");
        PostMessageW(effects,WM_COMMAND,IDCANCEL,0);
        PostMessageW(window,WM_CLOSE,0,0);
        HWND confirm=waitFor(editor,L"#32770",L"Nigh Editor");
        if(!confirm) return fail("local suggestion creates an unsaved edit");
        PostMessageW(confirm,WM_COMMAND,IDNO,0);
        if(WaitForSingleObject(editor.info.hProcess,6000)!=WAIT_OBJECT_0) return fail("close assistant demo cleanly");
    }
    {
        // Crash-style exit leaves an autosave; the next launch restores it without changing the saved file.
        Project original;
        Asset a; a.path=media;
        original.assets.push_back(a); original.seq.video.push_back(Clip(0,0,24));
        if(!saveProject(original,path)) return fail("create recovery fixture");
        Child editor;
        std::wstring quoted=L"\""+path+L"\"";
        if(!editor.start(exe,quoted)) return fail("relaunch with a saved project");
        HWND window=waitFor(editor,mainClass,baseName(path).substr(0,baseName(path).size()-5).c_str());
        if(!window) return fail("open fixture");
        PostMessageW(window,WM_KEYDOWN,'T',0);
        HWND title=waitFor(editor,L"#32770",L"Novo título");
        if(!title || !change(title,100,L"Texto recuperável")) return fail("prepare unsaved title");
        PostMessageW(title,WM_COMMAND,IDOK,0);
        if(!waitFor(editor,mainClass,L" *")) return fail("apply unsaved title");
        PostMessageW(window,WM_TIMER,3,0); // exercise the normal autosave timer immediately
        std::wstring backup=path+L".recuperacao", why;
        DWORD start=GetTickCount();
        while(!exists(backup) && GetTickCount()-start<6000) Sleep(30);
        Project recovered,clean;
        if(!readProject(backup,recovered,why) || recovered.seq.titles.size()!=1 ||
            recovered.seq.titles[0].title.text!=L"Texto recuperável" || !readProject(path,clean,why) || !clean.seq.titles.empty())
            return fail("autosave contains edits without modifying original file");
        TerminateProcess(editor.info.hProcess,0);
        WaitForSingleObject(editor.info.hProcess,2000);
        Child restart;
        if(!restart.start(exe,quoted)) return fail("restart editor to recover");
        HWND prompt=waitFor(restart,L"#32770",L"Recuperar projeto");
        if(!prompt) return fail("show recovery prompt");
        PostMessageW(prompt,WM_COMMAND,IDYES,0);
        if(!waitFor(restart,mainClass,L" *")) return fail("load recovered edits");
    }
    std::puts("GUI smoke: layered demo, local face suggestion, Eco layout, titles, autosave and recovery OK.");
    return 0;
}
