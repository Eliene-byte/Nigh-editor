#pragma once
#include "win.hpp"

namespace nigh {
bool editEffects(HWND owner,Lane lane,bool sound,Clip& clip);
bool editTitle(HWND owner,bool existing,int maxLength,Clip& clip);
bool editMarker(HWND owner,int frame,std::wstring& text);
}
