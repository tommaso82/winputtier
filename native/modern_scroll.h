#pragma once
#include <windows.h>

namespace wp {
// Native, lightweight scrollbar companion for a standard LISTBOX or ANSI preview.
// The target retains keyboard, wheel and selection semantics; only its scrollbar
// chrome is replaced.
HWND createModernScrollbar(HWND parent, HWND target, bool preview);
void updateModernScrollbar(HWND scrollbar);
void setModernScrollbarDpi(HWND scrollbar,int dpi);
}
