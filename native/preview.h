#pragma once
#include "core.h"
#include <windows.h>

namespace wp {
struct PreviewScrollMetrics { int position=0,total=0,page=0; };
PreviewScrollMetrics inlinePreviewScrollMetrics(HWND preview);
void setInlinePreviewScroll(HWND preview,int position);
HWND createInlinePreview(HWND parent,int controlId);
void updateInlinePreview(HWND preview,const Theme& theme,int selected,
                         const SessionAppearance& appearance);
}
