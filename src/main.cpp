#include "app.hpp"
#include <cstring>

int WINAPI wWinMain(HINSTANCE instance,HINSTANCE,LPWSTR,int show) {
    using namespace nigh;
    WNDCLASSEXW wc;
    std::memset(&wc,0,sizeof(wc));
    wc.cbSize=sizeof(wc); wc.style=CS_DBLCLKS; wc.lpfnWndProc=App::wndProc;
    wc.hInstance=instance; wc.hCursor=LoadCursorW(0,IDC_ARROW);
    wc.hIcon=LoadIconW(instance,MAKEINTRESOURCEW(101));
    wc.hIconSm=static_cast<HICON>(LoadImageW(instance,MAKEINTRESOURCEW(101),IMAGE_ICON,16,16,LR_SHARED));
    wc.lpszClassName=AppClass;
    if(!RegisterClassExW(&wc)) return 1;
    RECT work=rect(0,0,GetSystemMetrics(SM_CXSCREEN),GetSystemMetrics(SM_CYSCREEN));
    SystemParametersInfoW(SPI_GETWORKAREA,0,&work,0);
    int w=std::max(100,std::min(1280,width(work)-24)),h=std::max(100,std::min(800,height(work)-24));
    App app;
    HWND window=CreateWindowExW(0,AppClass,L"Nigh Editor",WS_OVERLAPPEDWINDOW|WS_CLIPCHILDREN,
        centerX(work)-w/2,centerY(work)-h/2,w,h,0,0,instance,&app);
    if(!window) {
        MessageBoxW(0,L"Não foi possível criar a janela do editor.",L"Nigh Editor",MB_OK|MB_ICONERROR);
        return 1;
    }
    ShowWindow(window,show); UpdateWindow(window);
    MSG msg;
    int status;
    while((status=GetMessageW(&msg,0,0,0))>0) { TranslateMessage(&msg); DispatchMessageW(&msg); }
    return status==0?int(msg.wParam):1;
}
