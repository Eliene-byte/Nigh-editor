#pragma once
#include "core.hpp"
#include <windows.h>
#include <utility>

namespace nigh {
class File {
    HANDLE h_;
public:
    File() : h_(INVALID_HANDLE_VALUE) {}
    explicit File(const std::wstring& path, bool write=false) : File() { open(path,write); }
    ~File() { close(); }
    File(const File&) = delete;
    File& operator=(const File&) = delete;
    void close() { if(good()) CloseHandle(h_); h_=INVALID_HANDLE_VALUE; }
    bool open(const std::wstring& path,bool write=false);
    bool good() const { return h_!=INVALID_HANDLE_VALUE; }
    bool seek(uint64_t offset);
    uint64_t size() const;
    DWORD read(void* ptr,DWORD len);
    bool get(void* ptr,DWORD len) { return read(ptr,len)==len; }
    bool put(const void* ptr,DWORD len);
    bool flush() { return good() && FlushFileBuffers(h_); }
};
struct TempDir {
    std::wstring path;
    TempDir();
    explicit TempDir(const std::wstring& parent);
    ~TempDir();
    TempDir(const TempDir&) = delete;
    TempDir& operator=(const TempDir&) = delete;
};
std::wstring dirName(const std::wstring& path);
std::wstring baseName(const std::wstring& path);
std::wstring join(const std::wstring& parent,const std::wstring& leaf);
std::wstring appDirectory();
std::wstring fullPath(const std::wstring& path);
bool exists(const std::wstring& path);
bool equalPath(const std::wstring& lhs,const std::wstring& rhs);
std::wstring tempFile(const std::wstring& parent);
std::string utf8(const std::wstring& w);
std::wstring wide(const std::string& s);
std::string readText(const std::wstring& path, DWORD limit, bool tail=false);
bool putText(const std::wstring& path,const std::string& text);

bool saveProject(const Project& p,const std::wstring& target);
bool readProject(const std::wstring& path,Project& dest,std::wstring& error);
}
