#pragma once
#include <windows.h>
#include <string>
#include <vector>

namespace wp {
// Modal, light-weight rounded popup. Returns the selected index, or -1 if
// dismissed. Labels are already localized by the caller.
int showCollectionPopup(HWND owner,HWND anchor,
                        const std::vector<std::wstring>& labels,
                        int selected,int dpi);
}
