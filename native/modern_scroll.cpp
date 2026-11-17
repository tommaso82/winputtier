#include "modern_scroll.h"
#include "preview.h"
#include <commctrl.h>
#include <windowsx.h>
#include <algorithm>
#include <cstdlib>

namespace wp {
namespace {
constexpr wchar_t klass[]=L"WinPuttierModernScrollbar";
constexpr UINT_PTR subclassId=0x57505342;
struct ScrollState {
    HWND target=nullptr;
    bool preview=false;
    bool dragging=false;
    int grab=0;
    int dpi=96;
    int wheelAccumulator=0;
};
struct Model {
    int position=0, page=1, total=1;
    int maxPosition()const{return std::max(0,total-page);}
};
ScrollState* state(HWND h){
    return reinterpret_cast<ScrollState*>(GetWindowLongPtrW(h,GWLP_USERDATA));
}
int dp(const ScrollState& s,int px){return MulDiv(px,s.dpi,96);}
Model model(const ScrollState& st){
    Model m{};
    if(!st.target||!IsWindow(st.target))return m;
    if(st.preview){
        const auto x=inlinePreviewScrollMetrics(st.target);
        m.position=x.position;
        m.page=std::max(1,x.page);
        m.total=std::max(m.page,x.total);
    }else{
        RECT rect{};GetClientRect(st.target,&rect);
        const int count=static_cast<int>(SendMessageW(st.target,LB_GETCOUNT,0,0));
        const int rowHeight=static_cast<int>(SendMessageW(st.target,LB_GETITEMHEIGHT,0,0));
        m.total=std::max(0,count);
        const int height=std::max(1,rect.bottom-rect.top);
        m.page=std::max(1,height/std::max(1,rowHeight));
        m.total=std::max(m.total,m.page);
        m.position=std::max(0,static_cast<int>(SendMessageW(st.target,LB_GETTOPINDEX,0,0)));
    }
    m.position=std::min(m.position,m.maxPosition());
    return m;
}
void scrollTo(ScrollState& st,int position){
    const auto m=model(st);
    const int clamped=std::clamp(position,0,m.maxPosition());
    if(st.preview)setInlinePreviewScroll(st.target,clamped);
    else SendMessageW(st.target,LB_SETTOPINDEX,clamped,0);
}
struct Geometry{
    int top=0, height=0, thumbTop=0, thumbHeight=0;
};
Geometry geometry(HWND h,const ScrollState& st,const Model& m){
    RECT bounds{};GetClientRect(h,&bounds);
    const int height=std::max(1,bounds.bottom-bounds.top);
    Geometry g{};
    g.top=dp(st,3);
    g.height=std::max(1,height-2*g.top);
    g.thumbHeight=std::clamp(static_cast<int>(
         static_cast<long long>(g.height)*m.page/std::max(1,m.total)),
         std::min(g.height,dp(st,24)),g.height);
    const int travel=std::max(0,g.height-g.thumbHeight);
    g.thumbTop=g.top+(m.maxPosition()?static_cast<int>(
         static_cast<long long>(travel)*m.position/m.maxPosition()):0);
    return g;
}
void setFromY(HWND h,ScrollState& st,int y){
    const Model m=model(st);
    if(m.maxPosition()<=0)return;
    const auto g=geometry(h,st,m);
    const int travel=std::max(1,g.height-g.thumbHeight);
    const int offset=std::clamp(y-st.grab-g.top,0,travel);
    const int value=static_cast<int>(
        (static_cast<long long>(offset)*m.maxPosition()+travel/2)/travel);
    scrollTo(st,value);
    InvalidateRect(h,nullptr,FALSE);
}
void roundFill(HDC dc,const RECT& r,COLORREF fill,int radius){
    HBRUSH brush=CreateSolidBrush(fill);
    HGDIOBJ oldBrush=SelectObject(dc,brush);
    HGDIOBJ oldPen=SelectObject(dc,GetStockObject(NULL_PEN));
    RoundRect(dc,r.left,r.top,r.right,r.bottom,radius,radius);
    SelectObject(dc,oldPen);
    SelectObject(dc,oldBrush);
    DeleteObject(brush);
}
LRESULT CALLBACK targetSubclass(HWND target,UINT msg,WPARAM w,LPARAM l,
                                UINT_PTR id,DWORD_PTR ref){
    HWND bar=reinterpret_cast<HWND>(ref);
    ScrollState* s=IsWindow(bar)?state(bar):nullptr;
    if(msg==WM_MOUSEWHEEL&&s&&!s->preview){
        // A hidden OS scrollbar must not disable the wheel. Respect Windows
        // wheel-lines settings while keeping per-control fractional deltas.
        UINT lines=3;
        SystemParametersInfoW(SPI_GETWHEELSCROLLLINES,0,&lines,0);
        s->wheelAccumulator+=GET_WHEEL_DELTA_WPARAM(w);
        while(std::abs(s->wheelAccumulator)>=WHEEL_DELTA){
            const int direction=s->wheelAccumulator>0?-1:1;
            s->wheelAccumulator+=direction*WHEEL_DELTA;
            const Model m=model(*s);
            const int units=lines==WHEEL_PAGESCROLL?m.page:
                     static_cast<int>(std::max(1u,lines));
            scrollTo(*s,m.position+direction*units);
        }
        InvalidateRect(bar,nullptr,FALSE);
        return 0;
    }
    const LRESULT value=DefSubclassProc(target,msg,w,l);
    if(s&&(msg==WM_MOUSEWHEEL||msg==WM_VSCROLL||msg==WM_KEYDOWN||
           msg==WM_SIZE||msg==WM_LBUTTONDOWN||msg==WM_LBUTTONUP||
           msg==LB_RESETCONTENT||msg==LB_SETCURSEL||msg==LB_SETTOPINDEX)){
        InvalidateRect(bar,nullptr,FALSE);
    }
    if(msg==WM_NCDESTROY)RemoveWindowSubclass(target,targetSubclass,id);
    return value;
}
LRESULT CALLBACK proc(HWND h,UINT msg,WPARAM w,LPARAM l){
    ScrollState* s=state(h);
    switch(msg){
    case WM_NCCREATE:{
        const auto* cs=reinterpret_cast<const CREATESTRUCTW*>(l);
        auto* initial=static_cast<ScrollState*>(cs->lpCreateParams);
        SetWindowLongPtrW(h,GWLP_USERDATA,reinterpret_cast<LONG_PTR>(initial));
        return TRUE;
    }
    case WM_ERASEBKGND:return 1;
    case WM_PAINT:{
        PAINTSTRUCT ps{};HDC dc=BeginPaint(h,&ps);
        RECT rect{};GetClientRect(h,&rect);
        FillRect(dc,&rect,static_cast<HBRUSH>(GetStockObject(WHITE_BRUSH)));
        if(s){
            const Model m=model(*s);
            if(m.maxPosition()>0){
                const auto g=geometry(h,*s,m);
                const int center=(rect.right+rect.left)/2;
                const int trackWidth=std::max(dp(*s,7),2);
                RECT track{center-trackWidth/2,g.top,
                           center+(trackWidth+1)/2,g.top+g.height};
                roundFill(dc,track,RGB(246,249,253),dp(*s,7));
                const int thumbWidth=std::max(dp(*s,6),2);
                RECT thumb{center-thumbWidth/2,g.thumbTop,
                           center+(thumbWidth+1)/2,g.thumbTop+g.thumbHeight};
                roundFill(dc,thumb,s->dragging?RGB(105,141,195):
                          RGB(171,192,220),dp(*s,6));
            }
        }
        EndPaint(h,&ps);
        return 0;
    }
    case WM_LBUTTONDOWN:
        if(s){
            const auto m=model(*s);
            if(m.maxPosition()>0){
                const auto g=geometry(h,*s,m);
                const int y=GET_Y_LPARAM(l);
                s->grab=(y>=g.thumbTop&&y<g.thumbTop+g.thumbHeight)
                          ?y-g.thumbTop:g.thumbHeight/2;
                s->dragging=true;
                SetCapture(h);
                setFromY(h,*s,y);
            }
        }
        return 0;
    case WM_MOUSEMOVE:
        if(s&&s->dragging){
            setFromY(h,*s,GET_Y_LPARAM(l));
        }
        return 0;
    case WM_LBUTTONUP:
        if(s&&s->dragging){
            setFromY(h,*s,GET_Y_LPARAM(l));
            s->dragging=false;
            if(GetCapture()==h)ReleaseCapture();
            InvalidateRect(h,nullptr,FALSE);
        }
        return 0;
    case WM_CAPTURECHANGED:
        if(s){s->dragging=false;InvalidateRect(h,nullptr,FALSE);}
        return 0;
    case WM_MOUSEWHEEL:
        if(s&&IsWindow(s->target))
            SendMessageW(s->target,WM_MOUSEWHEEL,w,l);
        return 0;
    case WM_NCDESTROY:
        if(s){
            if(IsWindow(s->target))
                RemoveWindowSubclass(s->target,targetSubclass,subclassId);
            SetWindowLongPtrW(h,GWLP_USERDATA,0);
            delete s;
        }
        return 0;
    }
    return DefWindowProcW(h,msg,w,l);
}
} // namespace
HWND createModernScrollbar(HWND parent,HWND target,bool preview){
    if(!parent||!target)return nullptr;
    static bool registered=false;
    if(!registered){
        WNDCLASSEXW wc{};wc.cbSize=sizeof(wc);
        wc.hInstance=GetModuleHandleW(nullptr);
        wc.lpfnWndProc=proc;
        wc.lpszClassName=klass;
        wc.hCursor=LoadCursorW(nullptr,IDC_ARROW);
        wc.hbrBackground=reinterpret_cast<HBRUSH>(COLOR_WINDOW+1);
        if(!RegisterClassExW(&wc))return nullptr;
        registered=true;
    }
    auto* s=new ScrollState{};
    s->target=target;s->preview=preview;
    s->dpi=std::max(96,static_cast<int>(GetDpiForWindow(parent)));
    HWND scrollbar=CreateWindowExW(0,klass,L"",WS_CHILD|WS_VISIBLE,
        0,0,12,100,parent,nullptr,GetModuleHandleW(nullptr),s);
    if(!scrollbar){delete s;return nullptr;}
    if(!SetWindowSubclass(target,targetSubclass,subclassId,
                          reinterpret_cast<DWORD_PTR>(scrollbar))){
        DestroyWindow(scrollbar);return nullptr;
    }
    return scrollbar;
}
void updateModernScrollbar(HWND scrollbar){
    if(scrollbar&&IsWindow(scrollbar))InvalidateRect(scrollbar,nullptr,FALSE);
}
void setModernScrollbarDpi(HWND scrollbar,int dpi){
    if(!scrollbar||!IsWindow(scrollbar))return;
    if(auto* s=state(scrollbar))s->dpi=std::max(96,dpi);
    updateModernScrollbar(scrollbar);
}
} // namespace wp
