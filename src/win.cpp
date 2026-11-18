#include "win.hpp"
#include <cwchar>
#include <cstring>

namespace nigh {
bool File::open(const std::wstring& path,bool write) {
    close();
    h_=CreateFileW(path.c_str(),write?GENERIC_WRITE:GENERIC_READ,FILE_SHARE_READ,
        0,write?CREATE_ALWAYS:OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,0);
    return good();
}
bool File::seek(uint64_t offset) {
    LARGE_INTEGER pos;
    pos.QuadPart=offset;
    return good() && SetFilePointerEx(h_,pos,0,FILE_BEGIN);
}
uint64_t File::size() const {
    LARGE_INTEGER n;
    return good() && GetFileSizeEx(h_,&n) ? uint64_t(n.QuadPart) : 0;
}
DWORD File::read(void* p,DWORD len) {
    DWORD n=0;
    return good() && ReadFile(h_,p,len,&n,0) ? n : 0;
}
bool File::put(const void* p,DWORD len) {
    DWORD n=0;
    return good() && WriteFile(h_,p,len,&n,0) && n==len;
}
std::wstring dirName(const std::wstring& p) {
    size_t i=p.find_last_of(L"\\/");
    return i==std::wstring::npos ? std::wstring() : p.substr(0,i+1);
}
std::wstring baseName(const std::wstring& p) {
    size_t i=p.find_last_of(L"\\/");
    return i==std::wstring::npos?p:p.substr(i+1);
}
std::wstring join(const std::wstring& p,const std::wstring& leaf) {
    return p.empty() || p.back()==L'\\' || p.back()==L'/' ? p+leaf : p+L"\\"+leaf;
}
std::wstring appDirectory() {
    wchar_t path[MAX_PATH];
    DWORD n=GetModuleFileNameW(0,path,MAX_PATH);
    return n && n<MAX_PATH ? dirName(std::wstring(path,n)) : L"";
}
std::wstring fullPath(const std::wstring& p) {
    wchar_t path[MAX_PATH];
    DWORD n=GetFullPathNameW(p.c_str(),MAX_PATH,path,0);
    return n && n<MAX_PATH ? std::wstring(path,n) : L"";
}
bool exists(const std::wstring& p) {
    DWORD attr=GetFileAttributesW(p.c_str());
    return attr!=INVALID_FILE_ATTRIBUTES && !(attr&FILE_ATTRIBUTE_DIRECTORY);
}
bool equalPath(const std::wstring& a,const std::wstring& b) {
    return CompareStringW(LOCALE_INVARIANT,NORM_IGNORECASE,a.c_str(),-1,b.c_str(),-1)==CSTR_EQUAL;
}
std::wstring tempFile(const std::wstring& parent) {
    wchar_t path[MAX_PATH];
    return GetTempFileNameW(parent.c_str(),L"ngh",0,path) ? std::wstring(path) : L"";
}
static std::wstring makeDir(const std::wstring& parent) {
    std::wstring p=tempFile(parent);
    if(p.empty() || !DeleteFileW(p.c_str()) || !CreateDirectoryW(p.c_str(),0)) return L"";
    return p;
}
TempDir::TempDir() {
    wchar_t parent[MAX_PATH];
    DWORD n=GetTempPathW(MAX_PATH,parent);
    if(n && n<MAX_PATH) path=makeDir(parent);
}
TempDir::TempDir(const std::wstring& parent) {
    if(!parent.empty()) path=makeDir(parent);
}
static void removeDir(const std::wstring& path) {
    WIN32_FIND_DATAW found;
    HANDLE list=FindFirstFileW(join(path,L"*").c_str(),&found);
    if(list!=INVALID_HANDLE_VALUE) {
        do {
            std::wstring name=found.cFileName;
            if(name==L"." || name==L"..") continue;
            std::wstring p=join(path,name);
            if(found.dwFileAttributes&FILE_ATTRIBUTE_DIRECTORY) {
                if(found.dwFileAttributes&FILE_ATTRIBUTE_REPARSE_POINT) RemoveDirectoryW(p.c_str());
                else removeDir(p);
            } else DeleteFileW(p.c_str());
        } while(FindNextFileW(list,&found));
        FindClose(list);
    }
    RemoveDirectoryW(path.c_str());
}
TempDir::~TempDir() { if(!path.empty()) removeDir(path); }
std::string utf8(const std::wstring& s) {
    if(s.empty()) return "";
    int n=WideCharToMultiByte(CP_UTF8,0,s.data(),int(s.size()),0,0,0,0);
    if(n<=0) return "";
    std::string result(n,'\0');
    WideCharToMultiByte(CP_UTF8,0,s.data(),int(s.size()),&result[0],n,0,0);
    return result;
}
std::wstring wide(const std::string& s) {
    if(s.empty()) return L"";
    int n=MultiByteToWideChar(CP_UTF8,0,s.data(),int(s.size()),0,0);
    if(n<=0) return L"";
    std::wstring result(n,L'\0');
    MultiByteToWideChar(CP_UTF8,0,s.data(),int(s.size()),&result[0],n);
    return result;
}
std::string readText(const std::wstring& path,DWORD limit,bool tail) {
    File f(path);
    uint64_t size=f.size();
    if(!tail && size>limit) return "";
    DWORD n=DWORD(std::min<uint64_t>(size,limit));
    if(n==0 || (tail && !f.seek(size-n))) return "";
    std::string result(n,'\0');
    if(!f.get(&result[0],n)) return "";
    return result;
}
bool putText(const std::wstring& path,const std::string& text) {
    File f(path,true);
    return f.put(text.data(),DWORD(text.size()));
}

// Little-endian project format. Only the edits and paths to original footage are stored.
static const char Legacy[8]={'N','I','G','H','E','D','1','\n'};
static const char Layered[8]={'N','I','G','H','E','D','2','\n'};
static const char Signature[8]={'N','I','G','H','E','D','3','\n'};
static bool putInt(File& f,int n) { uint32_t u=n; return f.put(&u,4); }
static bool getInt(File& f,int& n) {
    uint32_t u;
    if(!f.get(&u,4) || u>0x7fffffffu) return false;
    n=int(u);
    return true;
}
static bool putString(File& f,const std::wstring& s) {
    const std::string raw=utf8(s);
    return !raw.empty() && raw.size()<=MAX_PATH*4u && raw.find('\0')==std::string::npos &&
        putInt(f,int(raw.size())) && f.put(raw.data(),DWORD(raw.size()));
}
static bool getString(File& f,std::wstring& s) {
    int n;
    if(!getInt(f,n) || n<1 || n>MAX_PATH*4) return false;
    std::string text(n,'\0');
    if(!f.get(&text[0],n) || text.find('\0')!=std::string::npos) return false;
    s=wide(text);
    return !s.empty() && s.size()<MAX_PATH;
}
static bool putEffects(File& f,const Effects& e) {
    int fields[]={e.scale,e.x+100,e.y+100,e.opacity,e.brightness+60,e.contrast,e.saturation,e.dissolve,e.pan+100,e.fadeIn,e.fadeOut};
    for(int n:fields) if(!putInt(f,n)) return false;
    return true;
}
static bool getEffects(File& f,Effects& e) {
    int* fields[]={&e.scale,&e.x,&e.y,&e.opacity,&e.brightness,&e.contrast,&e.saturation,&e.dissolve,&e.pan,&e.fadeIn,&e.fadeOut};
    for(int* field:fields) if(!getInt(f,*field)) return false;
    e.x-=100; e.y-=100; e.brightness-=60; e.pan-=100;
    return true;
}
static bool putProject(File& f,const std::wstring& base,const Project& p) {
    if(!f.put(Signature,8) || !putInt(f,int(p.assets.size()))) return false;
    for(const Asset& a:p.assets) {
        bool rel=a.path.size()>base.size() && equalPath(a.path.substr(0,base.size()),base);
        if(!putInt(f,rel?1:0) || !putString(f,rel?a.path.substr(base.size()):a.path)) return false;
    }
    const Sequence& s=p.seq;
    if(!putInt(f,int(s.muteA1)+2*int(s.muteA2)+4*int(s.muteA3)+8*int(s.hideV2))) return false;
    for(int l=0;l<LaneCount;++l) {
        const auto& clips=laneOf(s,Lane(l));
        if(!putInt(f,int(clips.size()))) return false;
        for(const Clip& c:clips) {
            if(!putInt(f,l==Titles?0:c.asset) || !putInt(f,c.in) || !putInt(f,c.len) || !putInt(f,c.pos) ||
                !putInt(f,c.gain) || !putEffects(f,c.fx)) return false;
            if(l==Titles && (!putString(f,c.title.text) || !putInt(f,c.title.position) || !putInt(f,c.title.size) ||
                !putInt(f,c.title.color) || !putInt(f,c.title.box?1:0))) return false;
        }
    }
    if(!putInt(f,s.workIn+1) || !putInt(f,s.workOut+1) || !putInt(f,int(s.markers.size()))) return false;
    for(const Marker& m:s.markers) if(!putInt(f,m.frame) || !putString(f,m.text)) return false;
    return putInt(f,int(s.format)) && f.flush();
}
bool saveProject(const Project& p,const std::wstring& target) {
    if(!valid(p,false)) return false;
    std::wstring base=dirName(target), tmp=tempFile(base);
    if(tmp.empty()) return false;
    bool ok;
    { File f(tmp,true); ok=putProject(f,base,p); }
    if(ok && MoveFileExW(tmp.c_str(),target.c_str(),MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH)) return true;
    DeleteFileW(tmp.c_str());
    return false;
}
static bool getProject(File& f,const std::wstring& base,Project& p) {
    char magic[8];
    int n;
    if(f.size()>1024*1024 || !f.get(magic,8) || !getInt(f,n) || n>MaxAssets) return false;
    bool old=std::memcmp(magic,Legacy,8)==0;
    bool layered=std::memcmp(magic,Layered,8)==0;
    if(!old && !layered && std::memcmp(magic,Signature,8)) return false;
    for(int i=0;i<n;++i) {
        Asset a;
        int rel;
        if(!getInt(f,rel) || rel>1 || !getString(f,a.path)) return false;
        if(rel) a.path=join(base,a.path);
        a.path=fullPath(a.path);
        if(a.path.empty()) return false;
        a.name=baseName(a.path);
        p.assets.push_back(a);
    }
    if(!getInt(f,n) || n>(old?3:15)) return false;
    p.seq.muteA1=(n&1)!=0; p.seq.muteA2=(n&2)!=0; p.seq.muteA3=(n&4)!=0; p.seq.hideV2=(n&8)!=0;
    for(int l=0;l<(old?2:int(LaneCount));++l) {
        if(!getInt(f,n) || n>MaxClips) return false;
        auto& lane=laneOf(p.seq,Lane(l));
        for(int i=0;i<n;++i) {
            Clip c;
            if(!getInt(f,c.asset) || !getInt(f,c.in) || !getInt(f,c.len) || !getInt(f,c.pos) || !getInt(f,c.gain) ||
                (!old && !getEffects(f,c.fx))) return false;
            if(l==Titles) {
                int box;
                if(c.asset!=0 || !getString(f,c.title.text) || !getInt(f,c.title.position) || !getInt(f,c.title.size) ||
                    !getInt(f,c.title.color) || !getInt(f,box) || box>1) return false;
                c.asset=-1; c.title.box=box!=0;
            }
            lane.push_back(std::move(c));
        }
    }
    if(!old) {
        if(!getInt(f,p.seq.workIn) || !getInt(f,p.seq.workOut) || !getInt(f,n) || n>MaxMarkers) return false;
        --p.seq.workIn; --p.seq.workOut;
        for(int i=0;i<n;++i) {
            int t;
            std::wstring text;
            if(!getInt(f,t) || !getString(f,text)) return false;
            p.seq.markers.push_back(Marker(t,text));
        }
        if(!layered) {
            if(!getInt(f,n) || n>=FrameFormats) return false;
            p.seq.format=FrameFormat(n);
        }
        char end;
        if(f.read(&end,1)) return false;
    }
    return valid(p,false);
}
bool readProject(const std::wstring& path,Project& p,std::wstring& error) {
    File f(path);
    Project draft;
    if(!getProject(f,dirName(path),draft)) {
        error=L"Não foi possível ler o projeto. Verifique se é um arquivo .nigh desta versão.";
        return false;
    }
    draft.file=path;
    draft.name=baseName(path);
    size_t dot=draft.name.find_last_of(L'.');
    if(dot!=std::wstring::npos && dot>0) draft.name.resize(dot);
    p=std::move(draft);
    return true;
}
}
