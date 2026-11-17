#pragma once
#include "core.h"
#include <windows.h>

namespace wp {
HWND createInlinePreview(HWND parent,int controlId);
void updateInlinePreview(HWND preview,const Theme& theme,int selected,
                         const SessionAppearance& appearance);
}
