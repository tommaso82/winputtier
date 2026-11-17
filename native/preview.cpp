#include "preview.h"
#include "i18n.h"
#include <algorithm>
#include <cmath>
#include <cwchar>
#include <string>
#pragma comment(lib,"gdi32.lib")
#pragma comment(lib,"user32.lib")

namespace wp {
namespace {
constexpr wchar_t klass[]=L"WinPuttierInlineAnsiCanvas";
const int ansi[16]={6,8,10,12,14,16,18,20,7,9,11,13,15,17,19,21};
const wchar_t* names[16]={
 L"Nero",L"Rosso",L"Verde",L"Giallo",L"Blu",L"Magenta",L"Ciano",L"Bianco",
 L"Nero+",L"Rosso+",L"Verde+",L"Giallo+",L"Blu+",L"Magenta+",L"Ciano+",L"Bianco+"
};
struct State {
 Theme theme{L"Default",L"",L"",defaultPalette(),true};
 SessionAppearance settings{};
 int selected=2,scroll=0,dpi=96;
 HFONT mono=nullptr,monoBold=nullptr,small=nullptr,heading=nullptr;
};
State* state(HWND h){return reinterpret_cast<State*>(GetWindowLongPtrW(h,GWLP_USERDATA));}
int dp(const State& s,int n){return MulDiv(n,s.dpi,96);}
COLORREF rgb(const Color& c){return RGB(c.r,c.g,c.b);}
COLORREF headingInk(){return RGB(45,56,76);}
void fonts(State& s){
 if(s.mono)DeleteObject(s.mono);
 if(s.monoBold)DeleteObject(s.monoBold);
 if(s.small)DeleteObject(s.small);
 if(s.heading)DeleteObject(s.heading);
 s.mono=CreateFontW(-dp(s,13),0,0,0,FW_NORMAL,0,0,0,DEFAULT_CHARSET,0,0,
                    CLEARTYPE_QUALITY,FIXED_PITCH,L"Consolas");
 s.monoBold=CreateFontW(-dp(s,13),0,0,0,FW_BOLD,0,0,0,DEFAULT_CHARSET,0,0,
                        CLEARTYPE_QUALITY,FIXED_PITCH,L"Consolas");
 s.small=CreateFontW(-dp(s,12),0,0,0,FW_NORMAL,0,0,0,DEFAULT_CHARSET,0,0,
                     CLEARTYPE_QUALITY,DEFAULT_PITCH,L"Segoe UI");
 s.heading=CreateFontW(-dp(s,14),0,0,0,FW_SEMIBOLD,0,0,0,DEFAULT_CHARSET,0,0,
                       CLEARTYPE_QUALITY,DEFAULT_PITCH,L"Segoe UI");
}
void fill(HDC dc,int x,int y,int w,int h,COLORREF c) {
 if(w<1||h<1)return;
 RECT r{x,y,x+w,y+h};HBRUSH b=CreateSolidBrush(c);FillRect(dc,&r,b);DeleteObject(b);
}
void frame(HDC dc,int x,int y,int w,int h,COLORREF c,int thickness=1) {
 HBRUSH b=CreateSolidBrush(c);
 for(int k=0;k<thickness;++k){
   RECT r{x+k,y+k,x+w-k,y+h-k};FrameRect(dc,&r,b);
 }
 DeleteObject(b);
}
int string(HDC dc,int x,int y,const wchar_t* value,COLORREF color,HFONT font){
 const auto translated=wp::tr(value);
 auto old=SelectObject(dc,font);SetBkMode(dc,TRANSPARENT);SetTextColor(dc,color);
 const int n=static_cast<int>(translated.size());
 TextOutW(dc,x,y,translated.c_str(),n);
 SIZE size{};GetTextExtentPoint32W(dc,translated.c_str(),n,&size);SelectObject(dc,old);
 return x+size.cx;
}
void title(HDC dc,const State& st,int x,int y,const wchar_t* name,int width){
 string(dc,x,y,name,headingInk(),st.heading);
 fill(dc,x,y+dp(st,24),width,dp(st,1),RGB(222,229,239));
}
int maxScroll(HWND h,State& st) {
 RECT r{};GetClientRect(h,&r);
 return std::max(0,dp(st,1870)-static_cast<int>(r.bottom-r.top));
}
void scrollTo(HWND h,State& st,int value){
 RECT r{};GetClientRect(h,&r);
 st.scroll=std::max(0,std::min(value,maxScroll(h,st)));
 // Position is held by the canvas; a slim sibling handles the scrollbar chrome.
 InvalidateRect(h,nullptr,FALSE);
}
void paintCanvas(HDC dc,HWND h,State& st){
 const auto& p=st.theme.colors;
 RECT bounds{};GetClientRect(h,&bounds);
 const int fullW=bounds.right-bounds.left;
 // Logical content scaled for the monitor DPI; no OS bitmap stretching.
 const int width=std::max(100,MulDiv(fullW,96,st.dpi));
 const int left=dp(st,14);
 const int cardW=std::max(200,fullW-dp(st,36));
 const auto bg=rgb(p[2]),fg=rgb(p[0]),bright=rgb(p[1]);
 const auto accent=RGB(61,120,206),muted=RGB(111,122,142);
 auto use=[&](int index,int x,int y,int w,int height){
   if(st.selected==index)frame(dc,x-dp(st,2),y-dp(st,2),w+dp(st,4),
                                height+dp(st,4),accent,dp(st,2));
 };
 // 1. Realistic terminal sample, using explicit PuTTY Colour indexes.
 title(dc,st,left,dp(st,12),L"01  TERMINALE / USCITA LINUX",cardW);
 const int terminalY=dp(st,47), terminalH=dp(st,268);
 // Rounded terminal surface; the ANSI output itself remains unchanged.
 HBRUSH terminalBrush=CreateSolidBrush(bg);
 HPEN terminalPen=CreatePen(PS_SOLID,std::max(1,dp(st,1)),RGB(42,56,76));
 HGDIOBJ previousBrush=SelectObject(dc,terminalBrush);
 HGDIOBJ previousPen=SelectObject(dc,terminalPen);
 RoundRect(dc,left,terminalY,left+cardW,terminalY+terminalH,dp(st,18),dp(st,18));
 SelectObject(dc,previousBrush);
 SelectObject(dc,previousPen);
 DeleteObject(terminalBrush);
 DeleteObject(terminalPen);
 const int x=left+dp(st,11);int y=terminalY+dp(st,13);
 const int row=dp(st,22);
 int terminalRight=left+cardW-dp(st,10);
 int saved=SaveDC(dc);IntersectClipRect(dc,x,y,terminalRight,terminalY+terminalH-dp(st,2));
 auto out=[&](const wchar_t* t,int index,bool bold=false){
     HFONT f=bold?st.monoBold:st.mono;
     int before=x;
     // The caller uses x explicitly for segments; this closure is only a fallback.
     (void)before;
     return string(dc,x,y,t,rgb(p[index]),f);
 };
 (void)out;
 auto seg=[&](int& where,const wchar_t* t,int index,bool b=false){
   where=string(dc,where,y,t,rgb(p[index]),b?st.monoBold:st.mono);
 };
 const bool byColor=st.settings.boldMode!=0;
 const bool byFont=st.settings.boldMode!=1;
 int pos=x;seg(pos,L"root@debian",10);seg(pos,L":",0);seg(pos,L"~/projects",14);
 seg(pos,L"$ ls --color=auto",0);y+=row;
 pos=x;seg(pos,L"drwxr-xr-x ",0);seg(pos,L"4  root 4096 ",0);
 seg(pos,L"documents/",14);y+=row;
 pos=x;seg(pos,L"-rwxr-xr-x ",0);seg(pos,L"1  root  728 ",0);
 seg(pos,L"build.sh",10);y+=row;
 pos=x;seg(pos,L"-rw-r--r-- ",0);seg(pos,L"1  root  128 ",0);
 seg(pos,L"config.yml",0);y+=row;
 pos=x;seg(pos,L"root@debian",10);seg(pos,L"$ git status",0);y+=row;
 pos=x;seg(pos,L"modified:",8);seg(pos,L" src/terminal.cpp",0);y+=row;
 pos=x;seg(pos,L"WARNING",12);seg(pos,L": 2 tasks pending",0);y+=row;
 pos=x;seg(pos,L"ERROR",8);seg(pos,L": connection refused",0);y+=row;
 pos=x;seg(pos,L"bold text:",byColor?1:0,byFont);seg(pos,L" example in this session",0);y+=row;
 pos=x;seg(pos,L"root@debian",10);seg(pos,L":~$ ",0);
 const int cursorX=pos;
 fill(dc,cursorX,y-dp(st,2),dp(st,10),dp(st,17),rgb(p[5]));
 string(dc,cursorX+dp(st,1),y-dp(st,1),L"A",rgb(p[4]),st.mono);
 RestoreDC(dc,saved);
 if(st.selected==2||st.selected==0)use(st.selected,left,terminalY,cardW,terminalH);
 // Real render annotations: ANSI color values shown exactly; shell colors are illustrative.
 string(dc,left,terminalY+terminalH+dp(st,7),
 L"Output dimostrativo con colori ANSI, non una sessione SSH reale.",muted,st.small);

 // 2. 22 PuTTY colors with 6 special roles.
 title(dc,st,left,dp(st,359),L"02  DEFAULT, GRASSETTO E CURSORE",cardW);
 const wchar_t* roles[6]={L"Testo",L"Grassetto",L"Sfondo",L"Sfondo bold",L"Testo cursore",L"Cursore"};
 const int chipY=dp(st,394),chipH=dp(st,40);
 const int gap=dp(st,6),chipW=(cardW-2*gap)/3;
 for(int i=0;i<6;++i){
   int xx=left+(i%3)*(chipW+gap),yy=chipY+(i/3)*dp(st,48);
   fill(dc,xx,yy,chipW,chipH,RGB(244,247,251));
   fill(dc,xx+dp(st,5),yy+dp(st,6),dp(st,23),dp(st,27),rgb(p[i]));
   frame(dc,xx+dp(st,5),yy+dp(st,6),dp(st,23),dp(st,27),RGB(135,145,158));
   string(dc,xx+dp(st,34),yy+dp(st,3),roles[i],headingInk(),st.small);
   wchar_t index[16];swprintf_s(index,L"C%02d",i);
   string(dc,xx+dp(st,34),yy+dp(st,20),index,muted,st.small);
   use(i,xx,yy,chipW,chipH);
 }
 string(dc,left,dp(st,501),L"Grassetto: ",headingInk(),st.small);
 const wchar_t* mode=st.settings.boldMode==0?L"solo font":
     st.settings.boldMode==1?L"colore intenso":L"font + colore intenso";
 string(dc,left+dp(st,75),dp(st,501),mode,accent,st.small);
 if(st.settings.useSystemColours){
   string(dc,left,dp(st,522),L"ATTENZIONE: PuTTY usa colori di sistema per i default/cursore.",
          RGB(182,80,35),st.small);
 }else{
   string(dc,left,dp(st,522),L"Colori di testo, sfondo e cursore letti dalla palette.",
          muted,st.small);
 }
 if(!st.settings.ansiColours)
   string(dc,left,dp(st,543),L"ANSI disattivato nella sessione: i blocchi sotto sono solo riferimenti.",
          RGB(176,70,35),st.small);

 // 3. Correct ANSI mapping: Colour6+2*n is normal, Colour7+2*n is bright.
 title(dc,st,left,dp(st,574),L"03  ANSI 16 COLORI / CODICI SGR",cardW);
 const int y0=dp(st,610),cellGap=dp(st,4),swatchW=std::max(dp(st,25),(cardW-7*cellGap)/8);
 for(int i=0;i<16;++i){
   int xx=left+(i%8)*(swatchW+cellGap),yy=y0+(i/8)*dp(st,61);
   int index=ansi[i];
   fill(dc,xx,yy,swatchW,dp(st,25),rgb(p[index]));
   frame(dc,xx,yy,swatchW,dp(st,25),RGB(130,144,166));
   use(index,xx,yy,swatchW,dp(st,25));
   const wchar_t* label=i<8?L"STD":L"HI";
   wchar_t desc[18];
   swprintf_s(desc,L"%02d %s",i,label);
   string(dc,xx,yy+dp(st,29),desc,muted,st.small);
 }
 // The matrices use *foreground* indexes for rows and *background* indexes
 // for columns; vivid/bright backgrounds are genuine ANSI 100..107.
 const int gridHead=dp(st,754),cellH=dp(st,24);
 const int rowNameW=dp(st,66),matrixGap=dp(st,4);
 const int cellWidth=std::max(dp(st,21),(cardW-rowNameW-7*matrixGap)/8);
 for(int half=0;half<2;++half){
   int beginY=gridHead+half*dp(st,455);
   title(dc,st,left,beginY,
         half==0?L"04  TESTO x SFONDI ANSI NORMALI":L"05  TESTO x SFONDI ANSI BRILLANTI",
         cardW);
   const int cellsY=beginY+dp(st,43);
   const int x0=left+rowNameW;
   for(int k=0;k<8;++k){
     int bgIndex=ansi[half*8+k];
     int xx=x0+k*(cellWidth+matrixGap);
     wchar_t number[10]{};
     swprintf_s(number,L"%d",half?100+k:40+k);
     string(dc,xx+dp(st,4),cellsY-dp(st,18),number,headingInk(),st.small);
     fill(dc,xx,cellsY,cellWidth,dp(st,13),rgb(p[bgIndex]));
   }
   for(int rowIdx=0;rowIdx<16;++rowIdx){
     const int fgIndex=ansi[rowIdx],yy=cellsY+dp(st,19)+rowIdx*cellH;
     wchar_t labelText[20]{};
     swprintf_s(labelText,L"%02d/%02d",rowIdx,
                rowIdx<8?30+rowIdx:90+rowIdx-8);
     string(dc,left+dp(st,1),yy+dp(st,2),labelText,headingInk(),st.small);
     for(int column=0;column<8;++column){
       const int bgIndex=ansi[half*8+column];
       const int xx=x0+column*(cellWidth+matrixGap);
       fill(dc,xx,yy,cellWidth,cellH-dp(st,2),rgb(p[bgIndex]));
       string(dc,xx+dp(st,3),yy+dp(st,3),L"Aa",rgb(p[fgIndex]),st.mono);
       if(st.selected==fgIndex||st.selected==bgIndex)
         frame(dc,xx,yy,cellWidth,cellH-dp(st,2),accent,dp(st,2));
     }
   }
 }
 title(dc,st,left,dp(st,1685),L"06  COME INTERPRETARE IL TEST",cardW);
 string(dc,left,dp(st,1721),L"Ogni cella mostra testo ANSI (riga) su sfondo ANSI (colonna).",
        headingInk(),st.small);
 string(dc,left,dp(st,1743),L"RGB esatti Colour0..Colour21: nessuna correzione automatica.",
        headingInk(),st.small);
 string(dc,left,dp(st,1765),L"Una combinazione poco leggibile qui lo sara' anche in PuTTY,",
        headingInk(),st.small);
 string(dc,left,dp(st,1787),L"salvo impostazioni come colori di sistema o grassetto.",
        headingInk(),st.small);
}
LRESULT CALLBACK canvasProc(HWND h,UINT msg,WPARAM w,LPARAM l) {
 auto* st=state(h);
 switch(msg){
 case WM_NCCREATE:{
   const auto* cs=reinterpret_cast<CREATESTRUCTW*>(l);
   st=new State{};
   SetWindowLongPtrW(h,GWLP_USERDATA,reinterpret_cast<LONG_PTR>(st));
   return TRUE;
 }
 case WM_CREATE:
   st->dpi=GetDpiForWindow(h);
   fonts(*st);
   return 0;
 case WM_SIZE:scrollTo(h,*st,st->scroll);return 0;
 case WM_MOUSEWHEEL:
   scrollTo(h,*st,st->scroll-GET_WHEEL_DELTA_WPARAM(w)/WHEEL_DELTA*dp(*st,64));return 0;
 case WM_VSCROLL:{
   RECT viewport{};GetClientRect(h,&viewport);
   int next=st->scroll;
   switch(LOWORD(w)){
     case SB_LINEUP:next-=dp(*st,42);break;
     case SB_LINEDOWN:next+=dp(*st,42);break;
     case SB_PAGEUP:next-=std::max(1,viewport.bottom-viewport.top);break;
     case SB_PAGEDOWN:next+=std::max(1,viewport.bottom-viewport.top);break;
     case SB_TOP:next=0;break;
     case SB_BOTTOM:next=dp(*st,1870);break;
   }
   scrollTo(h,*st,next);return 0;
 }
 case WM_ERASEBKGND:return 1;
 case WM_PAINT:{
   PAINTSTRUCT ps{};
   HDC dc=BeginPaint(h,&ps);
   RECT bounds{};GetClientRect(h,&bounds);
   const int w=bounds.right,hgt=bounds.bottom;
   HDC buffer=CreateCompatibleDC(dc);
   HBITMAP bitmap=CreateCompatibleBitmap(dc,std::max(1,w),std::max(1,hgt));
   HGDIOBJ previous=SelectObject(buffer,bitmap);
   fill(buffer,0,0,w,hgt,RGB(255,255,255));
   SetViewportOrgEx(buffer,0,-st->scroll,nullptr);
   paintCanvas(buffer,h,*st);
   SetViewportOrgEx(buffer,0,0,nullptr);
   BitBlt(dc,0,0,w,hgt,buffer,0,0,SRCCOPY);
   SelectObject(buffer,previous);
   DeleteObject(bitmap);DeleteDC(buffer);
   EndPaint(h,&ps);
   return 0;
 }
 case WM_DPICHANGED:
   st->dpi=HIWORD(w);fonts(*st);scrollTo(h,*st,st->scroll);return 0;
 case WM_NCDESTROY:
   if(st){
     if(st->mono)DeleteObject(st->mono);
     if(st->monoBold)DeleteObject(st->monoBold);
     if(st->small)DeleteObject(st->small);
     if(st->heading)DeleteObject(st->heading);
     SetWindowLongPtrW(h,GWLP_USERDATA,0);delete st;
   }
   return 0;
 }
 return DefWindowProcW(h,msg,w,l);
}
} // namespace

HWND createInlinePreview(HWND parent,int controlId){
 static bool registered=false;
 if(!registered){
   WNDCLASSEXW wc{};wc.cbSize=sizeof(wc);wc.hInstance=GetModuleHandleW(nullptr);
   wc.lpfnWndProc=canvasProc;wc.lpszClassName=klass;
   wc.hCursor=LoadCursorW(nullptr,IDC_ARROW);
   wc.hbrBackground=(HBRUSH)(COLOR_WINDOW+1);
   if(!RegisterClassExW(&wc))return nullptr;
   registered=true;
 }
 return CreateWindowExW(0,klass,L"",
    WS_CHILD|WS_VISIBLE|WS_TABSTOP,0,0,200,250,
    parent,(HMENU)(INT_PTR)controlId,GetModuleHandleW(nullptr),nullptr);
}
PreviewScrollMetrics inlinePreviewScrollMetrics(HWND preview){
  PreviewScrollMetrics result{};
  if(!preview||!IsWindow(preview))return result;
  auto* st=state(preview);
  if(!st)return result;
  RECT rect{};GetClientRect(preview,&rect);
  result.page=std::max(1,rect.bottom-rect.top);
  result.total=dp(*st,1870);
  result.position=st->scroll;
  return result;
}
void setInlinePreviewScroll(HWND preview,int position){
  if(!preview||!IsWindow(preview))return;
  auto* st=state(preview);
  if(st)scrollTo(preview,*st,position);
}
void updateInlinePreview(HWND preview,const Theme& theme,int selected,
                         const SessionAppearance& appearance){
 if(!preview||!IsWindow(preview))return;
 auto* s=state(preview);if(!s)return;
 s->theme=theme;s->selected=std::max(0,std::min(selected,21));
 s->settings=appearance;
 UINT dpi=GetDpiForWindow(preview);
 if(dpi!=static_cast<UINT>(s->dpi)){s->dpi=dpi;fonts(*s);scrollTo(preview,*s,s->scroll);}
 InvalidateRect(preview,nullptr,FALSE);
}
} // namespace wp
