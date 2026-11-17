#include "collection_popup.h"
#include <windowsx.h>
#include <algorithm>

namespace wp {
namespace {
constexpr wchar_t klass[]=L"WinPuttierCollectionPopup";
struct PopupState {
    const std::vector<std::wstring>* labels=nullptr;
    bool done=false;
    bool armed=false;
    int result=-1;
    int active=0;
    int selected=0;
    int top=0;
    int visible=0;
    int dpi=96;
    HFONT normalFont=nullptr;
    HFONT emphasisFont=nullptr;
};
PopupState* state(HWND h){
    return reinterpret_cast<PopupState*>(GetWindowLongPtrW(h,GWLP_USERDATA));
}
int px(const PopupState& s,int n){return MulDiv(n,s.dpi,96);}
int rowHeight(const PopupState& s){return px(s,31);}
int pad(const PopupState& s){return px(s,6);}
int count(const PopupState& s){return s.labels?static_cast<int>(s.labels->size()):0;}
void reveal(PopupState& s){
    if(s.active<s.top)s.top=s.active;
    if(s.active>=s.top+s.visible)s.top=s.active-s.visible+1;
    s.top=std::clamp(s.top,0,std::max(0,count(s)-s.visible));
}
void finish(HWND h,PopupState& s,int result){
    if(s.done)return;
    s.done=true;s.result=result;
    if(GetCapture()==h)ReleaseCapture();
    DestroyWindow(h);
}
void roundFill(HDC dc,const RECT& r,COLORREF fill,COLORREF edge,int radius){
    HBRUSH b=CreateSolidBrush(fill);
    HPEN p=CreatePen(PS_SOLID,1,edge);
    HGDIOBJ oldB=SelectObject(dc,b),oldP=SelectObject(dc,p);
    RoundRect(dc,r.left,r.top,r.right,r.bottom,radius,radius);
    SelectObject(dc,oldB);SelectObject(dc,oldP);
    DeleteObject(b);DeleteObject(p);
}
LRESULT CALLBACK proc(HWND h,UINT msg,WPARAM w,LPARAM l){
    auto* s=state(h);
    switch(msg){
    case WM_NCCREATE:{
        auto* cs=reinterpret_cast<CREATESTRUCTW*>(l);
        SetWindowLongPtrW(h,GWLP_USERDATA,reinterpret_cast<LONG_PTR>(cs->lpCreateParams));
        return TRUE;
    }
    case WM_CREATE:
        if(s){
            s->normalFont=CreateFontW(-px(*s,12),0,0,0,FW_NORMAL,0,0,0,
                DEFAULT_CHARSET,0,0,CLEARTYPE_QUALITY,DEFAULT_PITCH,L"Segoe UI");
            s->emphasisFont=CreateFontW(-px(*s,12),0,0,0,FW_SEMIBOLD,0,0,0,
                DEFAULT_CHARSET,0,0,CLEARTYPE_QUALITY,DEFAULT_PITCH,L"Segoe UI Semibold");
        }
        return 0;
    case WM_ERASEBKGND:return 1;
    case WM_PAINT:{
        PAINTSTRUCT ps{};HDC dc=BeginPaint(h,&ps);
        RECT rect{};GetClientRect(h,&rect);
        const int width=rect.right,height=rect.bottom;
        HDC memory=CreateCompatibleDC(dc);
        HBITMAP bitmap=CreateCompatibleBitmap(dc,std::max(1,width),std::max(1,height));
        HGDIOBJ saved=SelectObject(memory,bitmap);
        FillRect(memory,&rect,static_cast<HBRUSH>(GetStockObject(WHITE_BRUSH)));
        if(s){
            RECT shell{0,0,width,height};
            roundFill(memory,shell,RGB(255,255,255),RGB(207,222,243),px(*s,13));
            const int hRow=rowHeight(*s),padding=pad(*s);
            for(int i=0;i<s->visible;++i){
                const int index=s->top+i;
                if(index>=count(*s))break;
                RECT bounds{padding,padding+i*hRow,
                            width-padding,padding+(i+1)*hRow};
                if(index==s->active){
                    RECT active=bounds;
                    active.left+=px(*s,2);active.right-=px(*s,2);
                    active.top+=px(*s,1);active.bottom-=px(*s,1);
                    roundFill(memory,active,RGB(234,242,255),
                              RGB(234,242,255),px(*s,10));
                }
                const bool selected=index==s->selected;
                RECT label{bounds.left+px(*s,12),bounds.top,
                           bounds.right-px(*s,28),bounds.bottom};
                HGDIOBJ previous=SelectObject(memory,
                      index==s->active?s->emphasisFont:s->normalFont);
                SetBkMode(memory,TRANSPARENT);
                SetTextColor(memory,index==s->active?RGB(42,98,192):RGB(40,58,87));
                DrawTextW(memory,(*s->labels)[index].c_str(),-1,&label,
                        DT_VCENTER|DT_SINGLELINE|DT_END_ELLIPSIS|DT_NOPREFIX);
                SelectObject(memory,previous);
                if(selected){
                    RECT check{bounds.right-px(*s,27),bounds.top,
                               bounds.right-px(*s,8),bounds.bottom};
                    HGDIOBJ prior=SelectObject(memory,s->emphasisFont);
                    SetTextColor(memory,RGB(45,107,205));
                    DrawTextW(memory,L"✓",-1,&check,DT_CENTER|DT_VCENTER|DT_SINGLELINE);
                    SelectObject(memory,prior);
                }
            }
        }
        BitBlt(dc,0,0,width,height,memory,0,0,SRCCOPY);
        SelectObject(memory,saved);
        DeleteObject(bitmap);DeleteDC(memory);
        EndPaint(h,&ps);return 0;
    }
    case WM_MOUSEMOVE:
        if(s&&!s->done){
            RECT bounds{};GetClientRect(h,&bounds);
            const int x=GET_X_LPARAM(l),y=GET_Y_LPARAM(l);
            if(x>=0&&x<bounds.right&&y>=pad(*s)&&y<bounds.bottom-pad(*s)){
                const int index=s->top+(y-pad(*s))/rowHeight(*s);
                if(index<count(*s)&&index!=s->active){
                    s->active=index;InvalidateRect(h,nullptr,FALSE);
                }
            }
        }
        return 0;
    case WM_LBUTTONUP:
        if(s&&!s->done){
            RECT r{};GetClientRect(h,&r);
            const int x=GET_X_LPARAM(l),y=GET_Y_LPARAM(l);
            if(x>=0&&x<r.right&&y>=pad(*s)&&y<r.bottom-pad(*s)){
                const int index=s->top+(y-pad(*s))/rowHeight(*s);
                finish(h,*s,index>=0&&index<count(*s)?index:-1);
            }else finish(h,*s,-1);
        }
        return 0;
    case WM_MOUSEWHEEL:
        if(s&&!s->done){
            const int step=GET_WHEEL_DELTA_WPARAM(w)>0?-2:2;
            s->top=std::clamp(s->top+step,0,std::max(0,count(*s)-s->visible));
            s->active=std::clamp(s->active,s->top,
                                std::min(count(*s)-1,s->top+s->visible-1));
            InvalidateRect(h,nullptr,FALSE);
        }
        return 0;
    case WM_KEYDOWN:
        if(s&&!s->done){
            switch(w){
            case VK_ESCAPE:finish(h,*s,-1);return 0;
            case VK_RETURN:
            case VK_SPACE:finish(h,*s,s->active);return 0;
            case VK_UP:s->active=std::max(0,s->active-1);reveal(*s);break;
            case VK_DOWN:s->active=std::min(count(*s)-1,s->active+1);reveal(*s);break;
            case VK_HOME:s->active=0;reveal(*s);break;
            case VK_END:s->active=count(*s)-1;reveal(*s);break;
            case VK_PRIOR:s->active=std::max(0,s->active-s->visible);reveal(*s);break;
            case VK_NEXT:s->active=std::min(count(*s)-1,s->active+s->visible);reveal(*s);break;
            default:return 0;
            }
            InvalidateRect(h,nullptr,FALSE);
        }
        return 0;
    case WM_KILLFOCUS:
        if(s&&s->armed&&!s->done)finish(h,*s,-1);
        return 0;
    case WM_CAPTURECHANGED:
        // A different window taking the mouse closes the dropdown safely.
        if(s&&s->armed&&!s->done)finish(h,*s,-1);
        return 0;
    case WM_NCDESTROY:
        if(s){
            s->done=true;
            if(s->normalFont)DeleteObject(s->normalFont);
            if(s->emphasisFont)DeleteObject(s->emphasisFont);
            SetWindowLongPtrW(h,GWLP_USERDATA,0);
        }
        return 0;
    }
    return DefWindowProcW(h,msg,w,l);
}
} // namespace
int showCollectionPopup(HWND owner,HWND anchor,
                        const std::vector<std::wstring>& labels,
                        int selected,int dpi){
    if(labels.empty()||!owner||!anchor)return -1;
    static bool registered=false;
    if(!registered){
        WNDCLASSEXW wc{};wc.cbSize=sizeof(wc);
        wc.hInstance=GetModuleHandleW(nullptr);
        wc.lpfnWndProc=proc;
        wc.lpszClassName=klass;
        wc.hCursor=LoadCursorW(nullptr,IDC_ARROW);
        wc.hbrBackground=reinterpret_cast<HBRUSH>(COLOR_WINDOW+1);
        wc.style=CS_DROPSHADOW;
        if(!RegisterClassExW(&wc))return -1;
        registered=true;
    }
    PopupState s{};
    s.labels=&labels;s.dpi=std::max(96,dpi);
    s.active=std::clamp(selected,0,static_cast<int>(labels.size())-1);
    s.selected=s.active;
    s.visible=std::min(10,static_cast<int>(labels.size()));
    reveal(s);
    RECT anchorBounds{};GetWindowRect(anchor,&anchorBounds);
    MONITORINFO monitor{};monitor.cbSize=sizeof(monitor);
    GetMonitorInfoW(MonitorFromRect(&anchorBounds,MONITOR_DEFAULTTONEAREST),&monitor);
    const int width=std::max(anchorBounds.right-anchorBounds.left,MulDiv(190,s.dpi,96));
    const int height=s.visible*rowHeight(s)+2*pad(s);
    const RECT area=monitor.rcWork;
    const int x=std::clamp(anchorBounds.left,area.left,std::max(area.left,area.right-width));
    int y=anchorBounds.bottom+MulDiv(3,s.dpi,96);
    if(y+height>area.bottom)y=anchorBounds.top-height-MulDiv(3,s.dpi,96);
    y=std::clamp(y,area.top,std::max(area.top,area.bottom-height));
    HWND popup=CreateWindowExW(WS_EX_TOOLWINDOW|WS_EX_TOPMOST,klass,L"",
                   WS_POPUP,x,y,width,height,owner,nullptr,
                   GetModuleHandleW(nullptr),&s);
    if(!popup)return -1;
    HRGN region=CreateRoundRectRgn(0,0,width+1,height+1,MulDiv(13,s.dpi,96),MulDiv(13,s.dpi,96));
    if(!SetWindowRgn(popup,region,TRUE))DeleteObject(region);
    // When SetWindowRgn succeeds, Windows owns the region.
    ShowWindow(popup,SW_SHOW);
    SetForegroundWindow(popup);
    SetFocus(popup);
    SetCapture(popup);
    s.armed=true;
    MSG message{};
    while(!s.done){
        const BOOL status=GetMessageW(&message,nullptr,0,0);
        if(status<=0){
            if(status==0)PostQuitMessage(static_cast<int>(message.wParam));
            break;
        }
        TranslateMessage(&message);
        DispatchMessageW(&message);
    }
    s.armed=false;
    if(GetCapture()==popup)ReleaseCapture();
    if(IsWindow(popup))DestroyWindow(popup);
    if(IsWindow(owner))SetForegroundWindow(owner);
    if(IsWindow(anchor))SetFocus(anchor);
    return s.result;
}
} // namespace wp
