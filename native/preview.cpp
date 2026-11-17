#include "preview.h"
#include <algorithm>
#include <cmath>
#include <cwchar>
#include <string>
#pragma comment(lib,"user32.lib")
#pragma comment(lib,"gdi32.lib")

namespace wp {
namespace {
constexpr wchar_t kPreviewClass[]=L"WinPuttierExpandedPreviewWindow";
constexpr int kCanvasHeight=2440;
HWND window=nullptr;
Theme active{};
int activeIndex=2;
int scrollY=0;
HFONT titleFont=nullptr,normalFont=nullptr,monoFont=nullptr,boldMonoFont=nullptr;
const int kAnsi[16]={6,8,10,12,14,16,18,20,7,9,11,13,15,17,19,21};
const wchar_t* kAnsiLabels[16]={
    L"Nero",L"Rosso",L"Verde",L"Giallo",L"Blu",L"Magenta",L"Ciano",L"Bianco",
    L"Nero +",L"Rosso +",L"Verde +",L"Giallo +",L"Blu +",L"Magenta +",L"Ciano +",L"Bianco +"
};
const wchar_t* kRole[22]={
    L"Testo normale",L"Testo grassetto",L"Sfondo",L"Sfondo grassetto",
    L"Testo sotto cursore",L"Cursore",
    L"Nero",L"Nero brillante",L"Rosso",L"Rosso brillante",
    L"Verde",L"Verde brillante",L"Giallo",L"Giallo brillante",
    L"Blu",L"Blu brillante",L"Magenta",L"Magenta brillante",
    L"Ciano",L"Ciano brillante",L"Bianco",L"Bianco brillante"
};
COLORREF rgb(const Color& c){return RGB(c.r,c.g,c.b);}
COLORREF ink(){return RGB(36,42,57);}
std::wstring hex(const Color& c){
    wchar_t temp[12]{};
    swprintf_s(temp,L"#%02X%02X%02X",c.r,c.g,c.b);
    return temp;
}
void rect(HDC dc,int x,int y,int w,int h,COLORREF fill){
    RECT box{x,y,x+w,y+h};
    HBRUSH b=CreateSolidBrush(fill);
    FillRect(dc,&box,b);
    DeleteObject(b);
}
void stroke(HDC dc,int x,int y,int w,int h,COLORREF c,int size=2){
    HBRUSH brush=CreateSolidBrush(c);
    for(int i=0;i<size;++i){
        RECT r{x+i,y+i,x+w-i,y+h-i};
        FrameRect(dc,&r,brush);
    }
    DeleteObject(brush);
}
void line(HDC dc,int x,int y,int x2,COLORREF color){
    HPEN pen=CreatePen(PS_SOLID,1,color);
    auto prev=SelectObject(dc,pen);
    MoveToEx(dc,x,y,nullptr);LineTo(dc,x2,y);
    SelectObject(dc,prev);DeleteObject(pen);
}
void text(HDC dc,int x,int y,const std::wstring& str,COLORREF fg,HFONT font){
    auto old=SelectObject(dc,font);
    SetTextColor(dc,fg);SetBkMode(dc,TRANSPARENT);
    TextOutW(dc,x,y,str.c_str(),(int)str.size());
    SelectObject(dc,old);
}
double luminance(const Color& c){
    const double channels[3]={c.r/255.0,c.g/255.0,c.b/255.0};
    double weighted=0;
    const double weight[3]={0.2126,0.7152,0.0722};
    for(int i=0;i<3;++i){
        double x=channels[i];
        weighted+=(x<=0.04045?x/12.92:std::pow((x+0.055)/1.055,2.4))*weight[i];
    }
    return weighted;
}
std::wstring contrast(const Color& a,const Color& b){
    double x=luminance(a),y=luminance(b);
    const double value=(std::max(x,y)+0.05)/(std::min(x,y)+0.05);
    wchar_t buf[24]{};
    swprintf_s(buf,L"%.1f:1",value);
    return buf;
}
void title(HDC dc,int y,const wchar_t* name,const wchar_t* explanation,int width){
    text(dc,26,y,name,ink(),titleFont);
    if(explanation&&*explanation)
        text(dc,26,y+29,explanation,RGB(100,109,121),normalFont);
    line(dc,26,y+56,width-35,RGB(211,215,222));
}
void chip(HDC dc,int x,int y,int w,int h,int index,const wchar_t* name){
    const auto& p=active.colors;
    rect(dc,x,y,w,h,RGB(248,249,251));
    rect(dc,x+7,y+8,38,h-16,rgb(p[index]));
    stroke(dc,x+7,y+8,38,h-16,RGB(130,134,140),1);
    if(index==activeIndex)stroke(dc,x+1,y+1,w-2,h-2,RGB(37,117,212),3);
    text(dc,x+54,y+4,std::wstring(name),ink(),normalFont);
    text(dc,x+54,y+24,L"Colour"+std::to_wstring(index)+L"  "+hex(p[index]),RGB(78,87,102),normalFont);
}
void terminalLine(HDC dc,int x,int y,const wchar_t* first,int a,const wchar_t* second,int b,const wchar_t* third=nullptr,int c=0){
    const auto& colors=active.colors;
    int offset=x;
    if(first) {
        const int len=(int)wcslen(first);
        text(dc,offset,y,first,rgb(colors[a]),monoFont);offset+=len*10;
    }
    if(second){
        const int len=(int)wcslen(second);
        text(dc,offset,y,second,rgb(colors[b]),monoFont);offset+=len*10;
    }
    if(third)text(dc,offset,y,third,rgb(colors[c]),monoFont);
}
void paintContent(HDC dc,int width){
    const auto& p=active.colors;
    const int W=std::max(width,1110);
    text(dc,26,16,L"WinPuttier  /  ANTEPRIMA ANSI COMPLETA",ink(),titleFont);
    text(dc,26,48,L"Tema: "+active.name+L"    |    colore selezionato: Colour"+
        std::to_wstring(activeIndex)+L" - "+kRole[activeIndex]+L" "+hex(p[activeIndex]),
        RGB(55,102,159),normalFont);
    text(dc,26,73,L"Le cornici blu mostrano dove viene usato il colore selezionato.",RGB(91,97,110),normalFont);

    title(dc,116,L"1. SESSIONE TERMINALE REALISTICA",
          L"Testo normale, prompt, directory, programmi, errori, avvisi e grassetto",W);
    const int tx=27,ty=181,tw=W-65,th=336;
    rect(dc,tx,ty,tw,th,rgb(p[2]));
    stroke(dc,tx,ty,tw,th,RGB(123,128,135),1);
    int x=tx+18,y=ty+18;
    text(dc,x,y,L"Linux debian 6.12.0 x86_64 (tty1)",rgb(p[0]),monoFont);
    y+=29;
    terminalLine(dc,x,y,L"admin@debian",10,L":~/projects",14,L"$ ls -la --color=auto",0);
    y+=30;
    terminalLine(dc,x,y,L"drwxr-xr-x ",18,L"4 admin 4096 Nov 17 ",0,L"documents/",14);
    y+=29;
    terminalLine(dc,x,y,L"-rwxr-xr-x ",10,L"1 admin  538 Nov 17 ",0,L"deploy.sh",11);
    y+=29;
    terminalLine(dc,x,y,L"-rw-r--r-- ",0,L"1 admin 4096 Nov 17 ",0,L"config.yml",20);
    y+=29;
    terminalLine(dc,x,y,L"admin@debian",10,L":~/projects",14,L"$ git status",0);
    y+=29;
    terminalLine(dc,x,y,L"On branch ",0,L"main",15);
    y+=29;
    terminalLine(dc,x,y,L"modified: ",12,L"src/app.cpp",9);
    y+=29;
    terminalLine(dc,x,y,L"[INFO] ",19,L"Build completed successfully",11);
    y+=29;
    terminalLine(dc,x,y,L"[WARN] ",13,L"Configuration has changed",0);
    y+=29;
    terminalLine(dc,x,y,L"[ERROR] ",9,L"Connection refused",0);

    title(dc,545,L"2. SEI COLORI SPECIALI",
          L"PuTTY Colour0..Colour5: testo, grassetto, sfondi e cursore",W);
    const int card=(W-70-2*12)/3;
    for(int i=0;i<6;++i) {
        int cx=27+(i%3)*(card+12),cy=617+(i/3)*70;
        chip(dc,cx,cy,card,61,i,kRole[i]);
    }
    rect(dc,27,778,W-65,75,rgb(p[2]));
    text(dc,42,793,L"Testo normale",rgb(p[0]),monoFont);
    rect(dc,265,784,213,51,rgb(p[3]));
    text(dc,277,793,L"Testo GRASSETTO",rgb(p[1]),boldMonoFont);
    // Cursor block: the under-cursor glyph must use Colour4 on Colour5.
    rect(dc,510,790,18,24,rgb(p[5]));
    text(dc,513,792,L"A",rgb(p[4]),monoFont);
    text(dc,548,793,L"CURSORE (Colour4 su Colour5)",rgb(p[0]),normalFont);
    if(activeIndex<=5)stroke(dc,27,778,W-65,75,RGB(36,124,218),2);
    text(dc,27,863,L"Contrasto testo/sfondo: "+contrast(p[0],p[2])+
         L"    |    Grassetto/sfondo grassetto: "+contrast(p[1],p[3]),
         ink(),normalFont);

    title(dc,910,L"3. COLORI ANSI: TESTO SU SFONDO NORMALE",
         L"16 colori di testo x 8 sfondi normali. \"Aa 19\" usa i veri colori della palette.",W);
    const int matrixX=167,cellW=std::min(103,(W-matrixX-42)/8),cellH=29,firstY=994;
    for(int col=0;col<8;++col){
        int idx=kAnsi[col];
        text(dc,matrixX+col*cellW+3,firstY-26,kAnsiLabels[col],ink(),normalFont);
    }
    for(int row=0;row<16;++row){
        int fg=kAnsi[row],ry=firstY+row*cellH;
        text(dc,27,ry+5,std::wstring(kAnsiLabels[row])+L" ("+
             std::to_wstring(fg)+L")",ink(),normalFont);
        for(int col=0;col<8;++col){
            int bg=kAnsi[col],px=matrixX+col*cellW;
            rect(dc,px,ry,cellW-2,cellH-1,rgb(p[bg]));
            text(dc,px+10,ry+4,L"Aa 19",rgb(p[fg]),monoFont);
            if(fg==activeIndex||bg==activeIndex)
                stroke(dc,px,ry,cellW-2,cellH-1,RGB(38,130,229),2);
        }
    }
    title(dc,1493,L"4. COLORI ANSI: TESTO SU SFONDO BRILLANTE",
         L"Altre 128 combinazioni. Le caselle evidenziate usano il colore selezionato.",W);
    const int secondY=1578;
    for(int col=0;col<8;++col){
        int idx=kAnsi[col+8];
        text(dc,matrixX+col*cellW+3,secondY-26,kAnsiLabels[col+8],ink(),normalFont);
    }
    for(int row=0;row<16;++row){
        int fg=kAnsi[row],ry=secondY+row*cellH;
        text(dc,27,ry+5,std::wstring(kAnsiLabels[row])+L" ("+
             std::to_wstring(fg)+L")",ink(),normalFont);
        for(int col=0;col<8;++col){
            int bg=kAnsi[col+8],px=matrixX+col*cellW;
            rect(dc,px,ry,cellW-2,cellH-1,rgb(p[bg]));
            text(dc,px+10,ry+4,L"Aa 19",rgb(p[fg]),monoFont);
            if(fg==activeIndex||bg==activeIndex)
                stroke(dc,px,ry,cellW-2,cellH-1,RGB(38,130,229),2);
        }
    }
    title(dc,2072,L"5. LE 16 TINTE ANSI IN DETTAGLIO",
          L"Campione, indice PuTTY e valore HEX per ciascun colore normale e brillante",W);
    for(int i=0;i<16;++i){
        int col=i%4,row=i/4,idx=kAnsi[i];
        int cellX=27+col*((W-65)/4),cellY=2150+row*62;
        chip(dc,cellX,cellY,(W-85)/4,56,idx,kAnsiLabels[i]);
    }
    text(dc,28,2415,L"NOTA: L'uso effettivo dei colori grassetto dipende anche dalle opzioni PuTTY.",
         RGB(93,102,118),normalFont);
}
void setScroll(int value){
    if(!window)return;
    RECT r{};GetClientRect(window,&r);
    const int limit=std::max(0,kCanvasHeight-(r.bottom-r.top));
    scrollY=std::max(0,std::min(value,limit));
    SCROLLINFOW info{};info.cbSize=sizeof(info);info.fMask=SIF_RANGE|SIF_PAGE|SIF_POS;
    info.nMin=0;info.nMax=kCanvasHeight;info.nPage=(UINT)(r.bottom-r.top);
    info.nPos=scrollY;
    SetScrollInfo(window,SB_VERT,&info,TRUE);
    InvalidateRect(window,nullptr,TRUE);
}
LRESULT CALLBACK windowProc(HWND wnd,UINT msg,WPARAM w,LPARAM l){
    switch(msg){
    case WM_CREATE:
        window=wnd;
        titleFont=CreateFontW(-21,0,0,0,FW_SEMIBOLD,0,0,0,DEFAULT_CHARSET,0,0,
                             CLEARTYPE_QUALITY,DEFAULT_PITCH,L"Segoe UI");
        normalFont=CreateFontW(-16,0,0,0,FW_NORMAL,0,0,0,DEFAULT_CHARSET,0,0,
                              CLEARTYPE_QUALITY,DEFAULT_PITCH,L"Segoe UI");
        monoFont=CreateFontW(-16,0,0,0,FW_NORMAL,0,0,0,DEFAULT_CHARSET,0,0,
                            CLEARTYPE_QUALITY,FIXED_PITCH,L"Consolas");
        boldMonoFont=CreateFontW(-16,0,0,0,FW_BOLD,0,0,0,DEFAULT_CHARSET,0,0,
                                CLEARTYPE_QUALITY,FIXED_PITCH,L"Consolas");
        return 0;
    case WM_GETMINMAXINFO:
        ((MINMAXINFO*)l)->ptMinTrackSize={1050,560};
        return 0;
    case WM_SIZE:setScroll(scrollY);return 0;
    case WM_MOUSEWHEEL:
        setScroll(scrollY-GET_WHEEL_DELTA_WPARAM(w)/WHEEL_DELTA*92);
        return 0;
    case WM_VSCROLL:{
        SCROLLINFOW info{};info.cbSize=sizeof(info);info.fMask=SIF_ALL;
        GetScrollInfo(wnd,SB_VERT,&info);
        int next=scrollY;
        switch(LOWORD(w)){
            case SB_LINEUP:next-=38;break;
            case SB_LINEDOWN:next+=38;break;
            case SB_PAGEUP:next-=(int)info.nPage;break;
            case SB_PAGEDOWN:next+=(int)info.nPage;break;
            case SB_THUMBTRACK:next=info.nTrackPos;break;
            case SB_TOP:next=0;break;
            case SB_BOTTOM:next=kCanvasHeight;break;
        }
        setScroll(next);return 0;
    }
    case WM_KEYDOWN:
        if(w==VK_PRIOR)setScroll(scrollY-500);
        if(w==VK_NEXT)setScroll(scrollY+500);
        if(w==VK_HOME)setScroll(0);
        if(w==VK_END)setScroll(kCanvasHeight);
        return 0;
    case WM_ERASEBKGND:return 1;
    case WM_PAINT:{
        PAINTSTRUCT ps{};
        HDC dc=BeginPaint(wnd,&ps);
        RECT r{};GetClientRect(wnd,&r);
        rect(dc,0,0,r.right,r.bottom,RGB(249,250,252));
        SetViewportOrgEx(dc,0,-scrollY,nullptr);
        paintContent(dc,r.right);
        SetViewportOrgEx(dc,0,0,nullptr);
        EndPaint(wnd,&ps);
        return 0;
    }
    case WM_CLOSE:DestroyWindow(wnd);return 0;
    case WM_DESTROY:
        if(titleFont)DeleteObject(titleFont);
        if(normalFont)DeleteObject(normalFont);
        if(monoFont)DeleteObject(monoFont);
        if(boldMonoFont)DeleteObject(boldMonoFont);
        titleFont=normalFont=monoFont=boldMonoFont=nullptr;
        window=nullptr;
        return 0;
    }
    return DefWindowProcW(wnd,msg,w,l);
}
} // namespace

void updateExpandedPreview(const Theme& theme,int selectedColor){
    active=theme;
    activeIndex=std::max(0,std::min(selectedColor,21));
    if(window && IsWindow(window))InvalidateRect(window,nullptr,TRUE);
}
void showExpandedPreview(HWND owner,const Theme& theme,int selectedColor){
    updateExpandedPreview(theme,selectedColor);
    if(window && IsWindow(window)){
        ShowWindow(window,SW_SHOWNORMAL);
        SetForegroundWindow(window);
        return;
    }
    static bool registered=false;
    if(!registered) {
        WNDCLASSEXW wc{};wc.cbSize=sizeof(wc);
        wc.hInstance=GetModuleHandleW(nullptr);
        wc.lpfnWndProc=windowProc;wc.lpszClassName=kPreviewClass;
        wc.hCursor=LoadCursorW(nullptr,IDC_ARROW);
        wc.hbrBackground=(HBRUSH)(COLOR_WINDOW+1);
        if(!RegisterClassExW(&wc))return;
        registered=true;
    }
    RECT r{};GetWindowRect(owner,&r);
    int x=std::max(0,(int)r.left+45),y=std::max(0,(int)r.top+15);
    HWND created=CreateWindowExW(WS_EX_APPWINDOW,kPreviewClass,
       L"WinPuttier - Anteprima ANSI completa (rotellina per scorrere)",
       WS_OVERLAPPEDWINDOW|WS_VSCROLL,x,y,1200,850,owner,nullptr,
       GetModuleHandleW(nullptr),nullptr);
    if(!created)return;
    ShowWindow(created,SW_SHOW);UpdateWindow(created);
    SetForegroundWindow(created);
}
} // namespace wp
