#pragma once
#include "core.h"
#include <windows.h>

namespace wp {
// Native, scrollable 22-color terminal preview with two complete ANSI 16x16
// foreground/background matrices and explicitly labeled special colors.
void showExpandedPreview(HWND owner, const Theme& theme, int selectedColor);
void updateExpandedPreview(const Theme& theme, int selectedColor);
}
