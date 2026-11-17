#include "core.h"
#include "preview.h"
#include <windows.h>
#include <commctrl.h>
#include <commdlg.h>
#include <algorithm>
#include <cstdio>
#include <cwctype>
#include <memory>
#include <set>
#include <string>
#include <thread>
#include <vector>
#pragma comment(lib, "comctl32.lib")
#pragma comment(lib, "comdlg32.lib")
#pragma comment(lib, "gdi32.lib")
#pragma comment(lib, "user32.lib")

namespace {
enum : int {
    ID_SESSIONS=101, ID_THEMES=102, ID_PREVIEW=103, ID_STATUS=104,
    ID_SOURCE_FILTER=105, ID_SEARCH=106, ID_HEX=107, ID_PERSONAL_NAME=108,
    ID_COLOR_TITLE=109, ID_COLOR_RGB=110,
    ID_RELOAD=201, ID_SAVE_SESSION=202, ID_APPLY=203, ID_RESTORE=204,
    ID_REFRESH=205, ID_SOURCES=206, ID_IMPORT=207, ID_DOWNLOAD=208,
    ID_PICKER=209, ID_HEX_APPLY=210, ID_RESET=211, ID_SAVE_PERSONAL=212, ID_EXPORT=213,
    ID_SWATCH_FIRST=500, ID_FAVORITE=215,
    ID_MODAL_EDIT=901, ID_MODAL_SAVE=902, ID_MODAL_CANCEL=903
};
constexpr UINT WM_SYNC_DONE=WM_APP+10;
constexpr UINT WM_SYNC_PROGRESS=WM_APP+11;

const wchar_t* kColorNames[22]={
    L"Testo",L"Testo grassetto",L"Sfondo",L"Sfondo grassetto",L"Testo cursore",L"Cursore",
    L"Nero",L"Nero brillante",L"Rosso",L"Rosso brillante",L"Verde",L"Verde brillante",
    L"Giallo",L"Giallo brillante",L"Blu",L"Blu brillante",L"Magenta",L"Magenta brillante",
    L"Ciano",L"Ciano brillante",L"Bianco",L"Bianco brillante"
};
HWND gWindow=nullptr,gSessions=nullptr,gThemes=nullptr,gPreview=nullptr,gStatus=nullptr;
HWND gSessionTitle=nullptr,gThemesTitle=nullptr,gPreviewTitle=nullptr,gPaletteTitle=nullptr,gSearchLabel=nullptr,gLibraryStats=nullptr;
HWND gAppName=nullptr,gAppSub=nullptr,gFavoriteButton=nullptr;
HWND gSourceFilter=nullptr,gSearch=nullptr,gColorTitle=nullptr,gColorRgb=nullptr;
HWND gHex=nullptr,gPersonalName=nullptr,gSwatches[22]{};
HWND gReload=nullptr,gSaveSession=nullptr,gApply=nullptr,gRestore=nullptr;
HWND gRefresh=nullptr,gSources=nullptr,gImport=nullptr;
HWND gPicker=nullptr,gHexApply=nullptr,gReset=nullptr,gSavePersonal=nullptr,gExport=nullptr;
HFONT gFont=nullptr,gMono=nullptr,gTitleFont=nullptr,gSectionFont=nullptr,gSmallFont=nullptr;
HBRUSH gBackgroundBrush=nullptr,gCardBrush=nullptr;
int gDpi=96;
std::vector<std::wstring> gFavorites;
int pix(int n){return MulDiv(n,gDpi,96);}

std::vector<wp::Session> gSessionItems;
std::vector<wp::Theme> gAllThemes;
std::vector<size_t> gVisibleThemes;
std::wstring gThemeKey, gSelectedSource=L"Tutte";
wp::Theme gSelectedTheme;
wp::Palette gUneditedPalette{};
bool gHaveTheme=false,gDirty=false,gSyncing=false;
int gSelectedColor=2;
struct SyncResult { bool success=false; std::wstring message; };

COLORREF rgb(const wp::Color& c){return RGB(c.r,c.g,c.b);}
std::wstring textOf(HWND h) {
    int size=GetWindowTextLengthW(h);
    if(size<0||size>65535)return {};
    std::wstring out((size_t)size+1,L'\0');
    GetWindowTextW(h,&out[0],size+1);out.resize(size);
    return out;
}
std::wstring trim(std::wstring x){
    while(!x.empty()&&iswspace(x.back()))x.pop_back();
    size_t a=0;while(a<x.size()&&iswspace(x[a]))++a;
    return x.substr(a);
}
std::wstring toLower(std::wstring s){
    std::transform(s.begin(),s.end(),s.begin(),towlower);return s;
}
std::wstring keyOf(const wp::Theme& t){return t.source+L"\t"+t.name;}
int currentSession(){return (int)SendMessageW(gSessions,LB_GETCURSEL,0,0);}
const wp::Session* chosenSession() {
    int index=currentSession();
    return index>=0&&index<(int)gSessionItems.size()?&gSessionItems[index]:nullptr;
}
int selectedVisibleIndex(){return (int)SendMessageW(gThemes,LB_GETCURSEL,0,0);}
wp::Theme* chosenCatalogTheme(){
    int index=selectedVisibleIndex();
    if(index<0||index>=(int)gVisibleThemes.size())return nullptr;
    return &gAllThemes[gVisibleThemes[index]];
}
void message(const std::wstring& m){if(gStatus)SetWindowTextW(gStatus,m.c_str());}
HWND make(const wchar_t* klass,const wchar_t* label,DWORD style,int id,DWORD ex=0,HWND owner=nullptr){
    HWND h=CreateWindowExW(ex,klass,label,WS_CHILD|WS_VISIBLE|style,0,0,100,25,
                           owner?owner:gWindow,(HMENU)(INT_PTR)id,GetModuleHandleW(nullptr),nullptr);
    if(h&&gFont)SendMessageW(h,WM_SETFONT,(WPARAM)gFont,TRUE);
    return h;
}
HWND label(const wchar_t* label,int id){return make(L"STATIC",label,0,id);}
HWND button(const wchar_t* label,int id){return make(L"BUTTON",label,WS_TABSTOP|BS_OWNERDRAW,id);}
HWND edit(const wchar_t* text,int id){return make(L"EDIT",text,WS_TABSTOP|ES_AUTOHSCROLL,id,WS_EX_CLIENTEDGE);}
struct PendingMove { HWND window; int x,y,w,h; };
std::vector<PendingMove> gLayoutMoves;
bool gCollectingLayout=false;
void move(HWND h,int x,int y,int w,int height){
    if(!h)return;
    const PendingMove p{h,x,y,std::max(1,w),std::max(1,height)};
    if(gCollectingLayout)gLayoutMoves.push_back(p);
    else MoveWindow(h,p.x,p.y,p.w,p.h,TRUE);
}
void commitLayout(){
    gCollectingLayout=false;
    // Apply all child positions as one transaction, without intermediate
    // repainting. The old approach moved and painted dozens of children one
    // by one, leaving orphaned rounded borders while the window was resized.
    HDWP positions=BeginDeferWindowPos(static_cast<int>(gLayoutMoves.size()));
    if(positions){
        for(const auto& p:gLayoutMoves){
            positions=DeferWindowPos(positions,p.window,nullptr,
                p.x,p.y,p.w,p.h,
                SWP_NOZORDER|SWP_NOACTIVATE|SWP_NOREDRAW|SWP_NOCOPYBITS);
            if(!positions)break;
        }
    }
    if(positions)EndDeferWindowPos(positions);
    else for(const auto& p:gLayoutMoves)
        SetWindowPos(p.window,nullptr,p.x,p.y,p.w,p.h,
            SWP_NOZORDER|SWP_NOACTIVATE|SWP_NOREDRAW|SWP_NOCOPYBITS);
    gLayoutMoves.clear();
    // Clear everything (including exposed areas from the former positions),
    // then redraw the three rounded panels and every control consistently.
    RedrawWindow(gWindow,nullptr,nullptr,
        RDW_INVALIDATE|RDW_ERASE|RDW_ALLCHILDREN);
}
struct Columns {
 int left,mid,right,leftWidth,midWidth,rightWidth,top,bottom;
};
Columns columns(){
 RECT r{};GetClientRect(gWindow,&r);
 const int gap=pix(13),avail=r.right-4*gap;
 int a=avail*24/100, b=avail*33/100, c=avail-a-b;
 return {gap,2*gap+a,3*gap+a+b,a,b,c,pix(72),r.bottom-pix(47)};
}
void layout() {
 if(!gWindow||!gSessions)return;
 RECT r{};GetClientRect(gWindow,&r);
 const int W=r.right,H=r.bottom;
 if(W<pix(950)||H<pix(590))return;
 gLayoutMoves.clear();
 gCollectingLayout=true;
 const auto col=columns();
 const int pad=pix(14);
 const int x1=col.left+pad,x2=col.mid+pad,x3=col.right+pad;
 const int w1=col.leftWidth-2*pad,w2=col.midWidth-2*pad,w3=col.rightWidth-2*pad;
 const int top=col.top,bottom=col.bottom;
 move(gAppName,pix(35),pix(9),pix(340),pix(31));
 move(gAppSub,pix(35),pix(39),W-pix(53),pix(23));

 move(gSessionTitle,x1,top+pix(13),w1,pix(27));
 move(gSessions,x1,top+pix(48),w1,bottom-top-pix(230));
 int yy=bottom-pix(166);
 move(gReload,x1,yy,w1,pix(31));yy+=pix(38);
 move(gSaveSession,x1,yy,w1,pix(31));yy+=pix(38);
 move(gApply,x1,yy,w1,pix(35));yy+=pix(41);
 move(gRestore,x1,yy,w1,pix(31));

 move(gThemesTitle,x2,top+pix(13),w2,pix(26));
 move(gSourceFilter,x2,top+pix(46),w2,pix(240));
 move(gSearchLabel,x2,top+pix(82),w2,pix(18));
 move(gSearch,x2,top+pix(105),w2,pix(30));
 move(gThemes,x2,top+pix(144),w2,bottom-top-pix(288));
 move(gLibraryStats,x2,bottom-pix(131),w2,pix(20));
 const int half=(w2-pix(8))/2;
 move(gRefresh,x2,bottom-pix(105),half,pix(33));
 move(gSources,x2+half+pix(8),bottom-pix(105),half,pix(33));
 move(gImport,x2,bottom-pix(63),half,pix(33));
 move(gFavoriteButton,x2+half+pix(8),bottom-pix(63),half,pix(33));

 move(gPreviewTitle,x3,top+pix(13),w3,pix(26));
 const int previewY=top+pix(45);
 const int swY=bottom-pix(255);
 const int pvHeight=swY-previewY-pix(39);
 move(gPreview,x3,previewY,w3,std::max(pix(155),pvHeight));
 move(gPaletteTitle,x3,swY-pix(24),w3,pix(22));
 const int swGap=pix(5),swW=std::max(pix(18),(w3-10*swGap)/11);
 for(int i=0;i<22;++i)
    move(gSwatches[i],x3+(i%11)*(swW+swGap),
         swY+(i/11)*pix(32),swW,pix(27));
 move(gColorTitle,x3,bottom-pix(184),w3,pix(20));
 const int editorGap=pix(8);
 const int editorHalf=(w3-editorGap)/2;
 const int editorRight=x3+editorHalf+editorGap;
 // Same two-column grid for RGB, HEX and their paired actions.
 move(gColorRgb,x3,bottom-pix(161),editorHalf,pix(19));
 move(gHex,x3,bottom-pix(138),editorHalf,pix(29));
 move(gHexApply,editorRight,bottom-pix(138),editorHalf,pix(29));
 move(gPicker,x3,bottom-pix(102),editorHalf,pix(29));
 move(gReset,editorRight,bottom-pix(102),editorHalf,pix(29));
 move(gPersonalName,x3,bottom-pix(68),w3,pix(28));
 move(gSavePersonal,x3,bottom-pix(35),(w3-pix(8))/2,pix(29));
 move(gExport,x3+(w3+pix(8))/2,bottom-pix(35),(w3-pix(8))/2,pix(29));
 move(gStatus,pix(20),H-pix(36),W-pix(40),pix(25));
 commitLayout();
}
bool filePicker(bool save,std::wstring& filename,const std::wstring& suggested=L""){
    wchar_t value[32768]{};
    if(!suggested.empty())wcsncpy_s(value,suggested.c_str(),_TRUNCATE);
    OPENFILENAMEW opt{};opt.lStructSize=sizeof(opt);opt.hwndOwner=gWindow;
    opt.lpstrFile=value;opt.nMaxFile=_countof(value);
    opt.lpstrFilter=L"Temi PuTTY (*.reg)\0*.reg\0Tutti i file (*.*)\0*.*\0";
    opt.lpstrDefExt=L"reg";
    opt.Flags=OFN_EXPLORER|OFN_PATHMUSTEXIST|(save?OFN_OVERWRITEPROMPT:OFN_FILEMUSTEXIST);
    BOOL yes=save?GetSaveFileNameW(&opt):GetOpenFileNameW(&opt);
    if(!yes)return false;
    filename=value;return true;
}
bool writeExportFile(const std::wstring& path,const std::wstring& content){
    FILE* file=nullptr;
    if(_wfopen_s(&file,path.c_str(),L"wb")||!file)return false;
    const unsigned char bom[2]{0xff,0xfe};
    bool ok=fwrite(bom,1,2,file)==2 &&
       (content.empty()||fwrite(content.data(),sizeof(wchar_t),content.size(),file)==content.size());
    return fclose(file)==0&&ok;
}
void redrawPreview(){if(gPreview)InvalidateRect(gPreview,nullptr,TRUE);}
std::wstring hexOf(const wp::Color& c){
    wchar_t buf[12];swprintf_s(buf,L"#%02X%02X%02X",c.r,c.g,c.b);return buf;
}
void refreshColorEditor(){
    const bool usable=gHaveTheme&&gSelectedTheme.available;
    EnableWindow(gPicker,usable);
    EnableWindow(gHex,usable);
    EnableWindow(gHexApply,usable);
    EnableWindow(gReset,usable);
    EnableWindow(gSavePersonal,usable);
    EnableWindow(gExport,usable);
    EnableWindow(gPersonalName,usable);
    EnableWindow(gApply,usable&&chosenSession()!=nullptr);
    EnableWindow(gFavoriteButton,gHaveTheme);
    for(auto sw:gSwatches)EnableWindow(sw,usable);
    if(usable){
        const auto& c=gSelectedTheme.colors[gSelectedColor];
        std::wstring name=L"Colore "+std::to_wstring(gSelectedColor)+L" - "+kColorNames[gSelectedColor];
        if(gDirty)name+=L" (modificato)";
        SetWindowTextW(gColorTitle,name.c_str());
        SetWindowTextW(gHex,hexOf(c).c_str());
        auto rgbText=L"RGB: "+std::to_wstring(c.r)+L", "+std::to_wstring(c.g)+L", "+std::to_wstring(c.b);
        SetWindowTextW(gColorRgb,rgbText.c_str());
    } else{
        SetWindowTextW(gColorTitle,L"Seleziona un tema disponibile");
        SetWindowTextW(gColorRgb,L"I colori si modificano senza cambiare PuTTY.");
        SetWindowTextW(gHex,L"");
    }
    for(auto sw:gSwatches)InvalidateRect(sw,nullptr,TRUE);
    if(gPreview){
        wp::Theme shown=usable?gSelectedTheme:wp::Theme{L"Predefinito",L"Integrato",L"",wp::defaultPalette(),true};
        wp::SessionAppearance appearance{};
        if(const auto* session=chosenSession())appearance=wp::sessionAppearance(*session);
        wp::updateInlinePreview(gPreview,shown,gSelectedColor,appearance);
    }
    if(gFavoriteButton) {
        const bool fav=gHaveTheme&&wp::isFavorite(gSelectedTheme,gFavorites);
        SetWindowTextW(gFavoriteButton,fav?L"★ Rimuovi preferito":L"☆ Aggiungi preferito");
        InvalidateRect(gFavoriteButton,nullptr,TRUE);
    }
}
void selectColor(int index){
    if(index<0||index>21)return;
    gSelectedColor=index;refreshColorEditor();
}
void refreshSessions(){
    std::wstring previous;
    if(const auto* s=chosenSession())previous=s->key;
    gSessionItems=wp::sessions();
    SendMessageW(gSessions,LB_RESETCONTENT,0,0);
    int selected=-1;
    for(size_t i=0;i<gSessionItems.size();++i){
        SendMessageW(gSessions,LB_ADDSTRING,0,(LPARAM)gSessionItems[i].label.c_str());
        if(gSessionItems[i].key==previous)selected=(int)i;
    }
    if(!gSessionItems.empty())
        SendMessageW(gSessions,LB_SETCURSEL,selected>=0?selected:0,0);
    EnableWindow(gRestore,chosenSession()!=nullptr);
    EnableWindow(gSaveSession,chosenSession()!=nullptr);
    refreshColorEditor();
}
void populateSourceFilter(){
    auto was=gSelectedSource;
    std::set<std::wstring> names;
    names.insert(L"★ Preferiti");
    names.insert(L"Integrato");
    names.insert(L"Personali");
    for(auto& src:wp::configuredSources())names.insert(src.name);
    for(auto& t:gAllThemes)names.insert(t.source);
    SendMessageW(gSourceFilter,CB_RESETCONTENT,0,0);
    SendMessageW(gSourceFilter,CB_ADDSTRING,0,(LPARAM)L"Tutte");
    for(auto& n:names)SendMessageW(gSourceFilter,CB_ADDSTRING,0,(LPARAM)n.c_str());
    int index=(int)SendMessageW(gSourceFilter,CB_FINDSTRINGEXACT,(WPARAM)-1,(LPARAM)was.c_str());
    if(index==CB_ERR){index=0;was=L"Tutte";}
    gSelectedSource=was;
    SendMessageW(gSourceFilter,CB_SETCURSEL,index,0);
}
void applyThemeSelection(){
    auto theme=chosenCatalogTheme();
    if(!theme){
        gHaveTheme=false;gThemeKey.clear();
        refreshColorEditor();return;
    }
    auto key=keyOf(*theme);
    if(key==gThemeKey&&gHaveTheme){
        // A remote theme may have just finished downloading. Refresh its data
        // without discarding a palette the user is actively editing.
        if(!gDirty){
            gSelectedTheme=*theme;
            gUneditedPalette=theme->colors;
        }
        refreshColorEditor();
        return;
    }
    if(gDirty&&MessageBoxW(gWindow,L"Scartare le modifiche non salvate della palette?",
        L"WinPuttier",MB_YESNO|MB_ICONQUESTION)!=IDYES){
        for(size_t i=0;i<gVisibleThemes.size();++i)
            if(keyOf(gAllThemes[gVisibleThemes[i]])==gThemeKey){
                SendMessageW(gThemes,LB_SETCURSEL,i,0);return;
            }
        return;
    }
    gSelectedTheme=*theme;
    gThemeKey=key;
    gUneditedPalette=theme->colors;
    gHaveTheme=true;gDirty=false;gSelectedColor=2;
    const std::wstring suggestion=theme->name+L" Custom";
    SetWindowTextW(gPersonalName,suggestion.c_str());
    refreshColorEditor();
    if(!theme->available)
        message(L"Tema solo online. Usa Sincronizza libreria per ottenere tutti i file .reg.");
}
void filterThemes(const std::wstring& preferred=L""){
    std::wstring target=preferred.empty()?gThemeKey:preferred;
    auto query=toLower(trim(textOf(gSearch)));
    SendMessageW(gThemes,WM_SETREDRAW,FALSE,0);
    SendMessageW(gThemes,LB_RESETCONTENT,0,0);
    gVisibleThemes.clear();
    int focus=-1;
    for(size_t i=0;i<gAllThemes.size();++i){
        const auto& t=gAllThemes[i];
        if(gSelectedSource==L"★ Preferiti" && !wp::isFavorite(t,gFavorites))continue;
        if(gSelectedSource!=L"Tutte" && gSelectedSource!=L"★ Preferiti" &&
           gSelectedSource!=t.source)continue;
        const auto haystack=toLower(t.name+L" "+t.source);
        if(!query.empty()&&haystack.find(query)==std::wstring::npos)continue;
        std::wstring name=(wp::isFavorite(t,gFavorites)?L"★  ":L"")+t.name;
        if(gSelectedSource==L"Tutte")name+=L"  -  "+t.source;
        SendMessageW(gThemes,LB_ADDSTRING,0,(LPARAM)name.c_str());
        gVisibleThemes.push_back(i);
        if(keyOf(t)==target)focus=(int)gVisibleThemes.size()-1;
    }
    if(focus<0&&!gVisibleThemes.empty())focus=0;
    if(focus>=0)SendMessageW(gThemes,LB_SETCURSEL,focus,0);
    SendMessageW(gThemes,WM_SETREDRAW,TRUE,0);
    InvalidateRect(gThemes,nullptr,TRUE);
    applyThemeSelection();
}
void reloadCatalog(const std::wstring& focus=L"") {
    const auto target=focus.empty()?gThemeKey:focus;
    // Fully offline-first: online-only entries are not selectable. A successful
    // synchronization populates the cache; no broken previews or [Locale] tags.
    gAllThemes=wp::localThemes();
    if(gAllThemes.size()>1)
        std::sort(gAllThemes.begin()+1,gAllThemes.end(),[](const wp::Theme& a,const wp::Theme& b){
            const int n=_wcsicmp(a.name.c_str(),b.name.c_str());
            return n<0||(n==0&&_wcsicmp(a.source.c_str(),b.source.c_str())<0);
        });
    gFavorites=wp::favoriteIds();
    populateSourceFilter();
    filterThemes(target);
    SetWindowTextW(gLibraryStats,(std::to_wstring(gAllThemes.size())+
                    L" temi disponibili offline · cerca o filtra per raccolta").c_str());
}
void redrawSwatch(DRAWITEMSTRUCT* ds,int index){
    RECT bounds=ds->rcItem;
    const auto& p=gSelectedTheme.colors;
    COLORREF c=(gHaveTheme&&gSelectedTheme.available)?rgb(p[index]):GetSysColor(COLOR_BTNFACE);
    HBRUSH brush=CreateSolidBrush(c);FillRect(ds->hDC,&bounds,brush);DeleteObject(brush);
    HBRUSH border=CreateSolidBrush(index==gSelectedColor?RGB(45,126,220):RGB(114,114,114));
    RECT borderBounds=bounds;
    for(int k=0;k<(index==gSelectedColor?3:1);++k){
        FrameRect(ds->hDC,&borderBounds,border);
        InflateRect(&borderBounds,-1,-1);
    }
    DeleteObject(border);
}
void drawButton(DRAWITEMSTRUCT* ds) {
    RECT r=ds->rcItem;
    const bool disabled=(ds->itemState & ODS_DISABLED)!=0;
    const bool down=(ds->itemState & ODS_SELECTED)!=0;
    const bool primary=ds->CtlID==ID_APPLY||ds->CtlID==ID_REFRESH;
    const bool favorite=ds->CtlID==ID_FAVORITE;
    COLORREF back=disabled?RGB(236,240,246):
       primary?(down?RGB(37,81,151):RGB(53,103,190)):
       favorite?RGB(255,248,225):
       (down?RGB(230,235,245):RGB(247,249,253));
    COLORREF edge=disabled?RGB(222,226,233):
        primary?RGB(53,103,190):favorite?RGB(237,196,101):RGB(211,221,236);
    COLORREF foreground=disabled?RGB(151,162,177):
        primary?RGB(255,255,255):favorite?RGB(135,93,24):RGB(49,65,88);
    HBRUSH brush=CreateSolidBrush(back);
    HPEN pen=CreatePen(PS_SOLID,pix(1),edge);
    auto oldBrush=SelectObject(ds->hDC,brush);
    auto oldPen=SelectObject(ds->hDC,pen);
    RoundRect(ds->hDC,r.left+1,r.top+1,r.right-1,r.bottom-1,pix(9),pix(9));
    SelectObject(ds->hDC,oldBrush);SelectObject(ds->hDC,oldPen);
    DeleteObject(brush);DeleteObject(pen);
    wchar_t txt[256]{};GetWindowTextW(ds->hwndItem,txt,_countof(txt));
    SetTextColor(ds->hDC,foreground);SetBkMode(ds->hDC,TRANSPARENT);
    auto prior=SelectObject(ds->hDC,gFont);
    InflateRect(&r,-pix(7),0);
    DrawTextW(ds->hDC,txt,-1,&r,DT_CENTER|DT_VCENTER|DT_SINGLELINE|DT_END_ELLIPSIS);
    SelectObject(ds->hDC,prior);
}
void paintChrome(HWND wnd) {
    PAINTSTRUCT ps{};
    HDC dc=BeginPaint(wnd,&ps);
    RECT r{};GetClientRect(wnd,&r);
    FillRect(dc,&r,gBackgroundBrush);
    if(gSessions){
        auto col=columns();
        const int xs[3]={col.left,col.mid,col.right};
        const int ws[3]={col.leftWidth,col.midWidth,col.rightWidth};
        HBRUSH brush=CreateSolidBrush(RGB(255,255,255));
        HPEN pen=CreatePen(PS_SOLID,pix(1),RGB(222,230,241));
        HGDIOBJ prevBrush=SelectObject(dc,brush);
        HGDIOBJ prevPen=SelectObject(dc,pen);
        for(int i=0;i<3;++i){
            RoundRect(dc,xs[i],col.top,xs[i]+ws[i],col.bottom,pix(16),pix(16));
        }
        SelectObject(dc,prevBrush);SelectObject(dc,prevPen);
        DeleteObject(brush);DeleteObject(pen);
        // Accent line in the header keeps visual hierarchy without busy decoration.
        RECT bar{pix(18),pix(17),pix(23),pix(49)};
        HBRUSH blue=CreateSolidBrush(RGB(53,103,190));
        FillRect(dc,&bar,blue);DeleteObject(blue);
    }
    EndPaint(wnd,&ps);
}
bool decodeHex(std::wstring s,wp::Color& c){
    s=trim(s);
    if(!s.empty()&&s[0]==L'#')s.erase(0,1);
    if(s.size()!=6)return false;
    unsigned v=0;
    for(wchar_t x:s){
        v<<=4;
        if(x>=L'0'&&x<=L'9')v|=x-L'0';
        else if(x>=L'a'&&x<=L'f')v|=x-L'a'+10;
        else if(x>=L'A'&&x<=L'F')v|=x-L'A'+10;
        else return false;
    }
    c={(int)((v>>16)&255),(int)((v>>8)&255),(int)(v&255)};
    return true;
}
void setEditedColor(const wp::Color& value){
    if(!gHaveTheme||!gSelectedTheme.available)return;
    gSelectedTheme.colors[gSelectedColor]=value;
    gDirty=true;
    refreshColorEditor();
    message(L"Palette modificata solo nell'anteprima. Salva personale o applica alla sessione.");
}
void applyHex(){
    wp::Color value{};
    if(!decodeHex(textOf(gHex),value)){
        MessageBoxW(gWindow,L"Inserisci un valore HEX valido, ad esempio #7AA2F7.",L"Colore non valido",MB_OK|MB_ICONWARNING);
        return;
    }
    setEditedColor(value);
}
void colorPicker(){
    if(!gHaveTheme||!gSelectedTheme.available)return;
    static COLORREF custom[16]{};
    CHOOSECOLORW cc{};cc.lStructSize=sizeof(cc);cc.hwndOwner=gWindow;
    cc.rgbResult=rgb(gSelectedTheme.colors[gSelectedColor]);
    cc.lpCustColors=custom;cc.Flags=CC_RGBINIT|CC_FULLOPEN;
    if(ChooseColorW(&cc)){
        wp::Color value{(int)GetRValue(cc.rgbResult),(int)GetGValue(cc.rgbResult),(int)GetBValue(cc.rgbResult)};
        setEditedColor(value);
    }
}
void resetChanges(){
    if(!gHaveTheme||!gSelectedTheme.available)return;
    gSelectedTheme.colors=gUneditedPalette;
    gDirty=false;refreshColorEditor();
    message(L"Modifiche alla palette annullate. Nessuna sessione PuTTY modificata.");
}
void importReg(){
    std::wstring path,result;
    if(!filePicker(false,path))return;
    if(wp::importTheme(path,result)){
        gThemeKey.clear();gDirty=false;
        reloadCatalog();
    }
    message(result);
}
void savePersonalTheme(wp::Theme theme,bool afterSession=false){
    if(!afterSession&&!gHaveTheme)return;
    auto name=trim(textOf(gPersonalName));
    if(name.empty()){message(L"Inserisci prima il nome del tema personale.");return;}
    name=wp::safeFileName(name);
    const auto path=wp::customDir()+L"\\"+name+L".reg";
    if(GetFileAttributesW(path.c_str())!=INVALID_FILE_ATTRIBUTES){
        if(MessageBoxW(gWindow,L"Esiste gia' un tema personale con questo nome. Sovrascriverlo?",
                       L"WinPuttier",MB_YESNO|MB_ICONQUESTION)!=IDYES)return;
    }
    theme.name=name;theme.source=L"Personali";theme.remoteUrl=L"";theme.available=true;
    std::wstring error;
    if(!wp::saveCustomTheme(theme,path,error)){message(error);return;}
    gDirty=false;gThemeKey.clear();
    gSelectedSource=L"Personali";
    SetWindowTextW(gSearch,L"");
    reloadCatalog(keyOf(theme));
    message(L"Tema personale salvato: "+name+L". Nessuna modifica alle sessioni PuTTY.");
}
void saveSessionAsTheme(){
    const auto* session=chosenSession();
    if(!session)return;
    wp::Theme theme;std::wstring error;
    if(!wp::themeFromSession(*session,theme,error)){message(error);return;}
    const auto name=wp::safeFileName(theme.name);
    SetWindowTextW(gPersonalName,name.c_str());
    savePersonalTheme(theme,true);
}
void exportReg(){
    if(!gHaveTheme||!gSelectedTheme.available)return;
    const auto* session=chosenSession();
    if(!session){message(L"Seleziona prima la sessione a cui destinare il file .reg.");return;}
    std::wstring path;
    auto suggested=wp::dataDir()+L"\\"+wp::safeFileName(gSelectedTheme.name)+L".reg";
    if(!filePicker(true,path,suggested))return;
    if(writeExportFile(path,wp::exportReg(gSelectedTheme,session->key)))
        message(L"Tema esportato per "+session->label+L". NON applicato automaticamente.");
    else message(L"Impossibile scrivere il file .reg.");
}
void applyToSession(){
    const auto* session=chosenSession();
    if(!session||!gHaveTheme||!gSelectedTheme.available){
        message(L"Seleziona una sessione e un tema disponibile.");return;
    }
    auto question=L"Applicare il tema '"+gSelectedTheme.name+L"' alla sessione '"+session->label+
        L"'?\n\nIl backup dei colori originali viene preservato. Nessun'altra sessione sara' modificata.";
    if(MessageBoxW(gWindow,question.c_str(),L"Conferma applicazione tema",MB_YESNO|MB_ICONQUESTION)!=IDYES)return;
    std::wstring feedback;
    wp::applyTheme(*session,gSelectedTheme,feedback);
    message(feedback);
}
void restoreOriginal(){
    const auto* session=chosenSession();
    if(!session)return;
    if(MessageBoxW(gWindow,(L"Ripristinare i colori ORIGINALI di '"+session->label+
        L"'?\nIl backup originale rimarra' conservato.").c_str(),L"Ripristina colori originali",
        MB_YESNO|MB_ICONQUESTION)!=IDYES)return;
    std::wstring feedback;
    wp::restoreLastBackup(*session,feedback);
    message(feedback);
}
void syncAsync() {
    if(gSyncing)return;
    gSyncing=true;
    EnableWindow(gRefresh,FALSE);
    message(L"Sincronizzazione raccolte: scaricamento dei soli file .reg ...");
    const HWND target=gWindow;
    std::thread([target](){
        auto* result=new SyncResult{};
        result->success=wp::syncLibrary(result->message,[target](size_t done,size_t total){
            auto* progress=new std::pair<size_t,size_t>(done,total);
            if(!PostMessageW(target,WM_SYNC_PROGRESS,0,(LPARAM)progress))delete progress;
        });
        if(!PostMessageW(target,WM_SYNC_DONE,0,(LPARAM)result))delete result;
    }).detach();
}

struct SourceEditor {HWND window=nullptr,input=nullptr;bool done=false,saved=false;};
LRESULT CALLBACK sourceWndProc(HWND wnd,UINT msg,WPARAM w,LPARAM l){
    auto* editor=(SourceEditor*)GetWindowLongPtrW(wnd,GWLP_USERDATA);
    if(msg==WM_CREATE){
        editor=(SourceEditor*)((CREATESTRUCTW*)l)->lpCreateParams;
        SetWindowLongPtrW(wnd,GWLP_USERDATA,(LONG_PTR)editor);
        editor->window=wnd;
        auto labelControl=make(L"STATIC",
            L"Una fonte per riga: Nome = link repository, cartella o file .reg su GitHub",
            0,0,0,wnd);
        move(labelControl,14,12,733,24);
        editor->input=make(L"EDIT",L"",WS_TABSTOP|ES_MULTILINE|ES_AUTOVSCROLL|WS_VSCROLL|
                ES_WANTRETURN|ES_NOHIDESEL,ID_MODAL_EDIT,WS_EX_CLIENTEDGE,wnd);
        move(editor->input,14,43,740,351);
        SetWindowTextW(editor->input,wp::sourceConfigText().c_str());
        auto save=make(L"BUTTON",L"Salva fonti",WS_TABSTOP|BS_DEFPUSHBUTTON,ID_MODAL_SAVE,0,wnd);
        auto cancel=make(L"BUTTON",L"Annulla",WS_TABSTOP|BS_PUSHBUTTON,ID_MODAL_CANCEL,0,wnd);
        move(save,513,411,114,32);move(cancel,638,411,116,32);
        return 0;
    }
    if(msg==WM_COMMAND){
        if(LOWORD(w)==ID_MODAL_CANCEL){DestroyWindow(wnd);return 0;}
        if(LOWORD(w)==ID_MODAL_SAVE){
            std::wstring feedback;
            if(!wp::updateSourceConfig(textOf(editor->input),feedback)){
                MessageBoxW(wnd,feedback.c_str(),L"Configurazione non valida",MB_OK|MB_ICONERROR);
                return 0;
            }
            editor->saved=true;DestroyWindow(wnd);return 0;
        }
    }
    if(msg==WM_CLOSE){DestroyWindow(wnd);return 0;}
    if(msg==WM_DESTROY){if(editor)editor->done=true;return 0;}
    return DefWindowProcW(wnd,msg,w,l);
}
void manageSources(){
    static bool registered=false;
    if(!registered){
        WNDCLASSEXW wc{};wc.cbSize=sizeof(wc);wc.hInstance=GetModuleHandleW(nullptr);
        wc.lpfnWndProc=sourceWndProc;wc.lpszClassName=L"WinPuttierSourcesDialog";
        wc.hCursor=LoadCursorW(nullptr,IDC_ARROW);
        wc.hbrBackground=(HBRUSH)(COLOR_BTNFACE+1);
        if(!RegisterClassExW(&wc)){message(L"Impossibile aprire l'editor fonti.");return;}
        registered=true;
    }
    SourceEditor modal{};
    RECT owner{};GetWindowRect(gWindow,&owner);
    int x=owner.left+std::max(0,static_cast<int>(((owner.right-owner.left)-790)/2));
    int y=owner.top+std::max(0,static_cast<int>(((owner.bottom-owner.top)-500)/2));
    HWND dialog=CreateWindowExW(WS_EX_DLGMODALFRAME,L"WinPuttierSourcesDialog",
          L"WinPuttier - Gestisci fonti",WS_OVERLAPPED|WS_CAPTION|WS_SYSMENU,
          x,y,790,500,gWindow,nullptr,GetModuleHandleW(nullptr),&modal);
    if(!dialog)return;
    EnableWindow(gWindow,FALSE);
    ShowWindow(dialog,SW_SHOW);SetForegroundWindow(dialog);
    MSG m{};
    while(!modal.done&&GetMessageW(&m,nullptr,0,0)>0){
        if(!IsDialogMessageW(dialog,&m)){TranslateMessage(&m);DispatchMessageW(&m);}
    }
    EnableWindow(gWindow,TRUE);SetForegroundWindow(gWindow);
    if(modal.saved){reloadCatalog();message(L"Fonti aggiornate. Premi Sincronizza libreria per aggiornare i file .reg.");}
}
void rebuildFonts(){
    if(gFont)DeleteObject(gFont);
    if(gMono)DeleteObject(gMono);
    if(gTitleFont)DeleteObject(gTitleFont);
    if(gSectionFont)DeleteObject(gSectionFont);
    if(gSmallFont)DeleteObject(gSmallFont);
    gFont=CreateFontW(-pix(13),0,0,0,FW_MEDIUM,0,0,0,DEFAULT_CHARSET,
        0,0,CLEARTYPE_QUALITY,DEFAULT_PITCH,L"Segoe UI");
    gMono=CreateFontW(-pix(13),0,0,0,FW_NORMAL,0,0,0,DEFAULT_CHARSET,
        0,0,CLEARTYPE_QUALITY,FIXED_PITCH,L"Consolas");
    gTitleFont=CreateFontW(-pix(20),0,0,0,FW_BOLD,0,0,0,DEFAULT_CHARSET,
        0,0,CLEARTYPE_QUALITY,DEFAULT_PITCH,L"Segoe UI");
    gSectionFont=CreateFontW(-pix(15),0,0,0,FW_SEMIBOLD,0,0,0,DEFAULT_CHARSET,
        0,0,CLEARTYPE_QUALITY,DEFAULT_PITCH,L"Segoe UI");
    gSmallFont=CreateFontW(-pix(12),0,0,0,FW_MEDIUM,0,0,0,DEFAULT_CHARSET,
        0,0,CLEARTYPE_QUALITY,DEFAULT_PITCH,L"Segoe UI");
    if(gWindow){
        EnumChildWindows(gWindow,[](HWND child,LPARAM)->BOOL{
            HFONT font=gFont;
            if(child==gAppName)font=gTitleFont;
            else if(child==gAppSub ||child==gLibraryStats ||child==gSearchLabel ||
                    child==gStatus ||child==gColorRgb)font=gSmallFont;
            else if(child==gSessionTitle ||child==gThemesTitle ||
                    child==gPreviewTitle ||child==gPaletteTitle)font=gSectionFont;
            SendMessageW(child,WM_SETFONT,(WPARAM)font,TRUE);
            return TRUE;
        },0);
    }
}
void createControls(){
    if(!gBackgroundBrush)gBackgroundBrush=CreateSolidBrush(RGB(244,247,251));
    if(!gCardBrush)gCardBrush=CreateSolidBrush(RGB(255,255,255));
    rebuildFonts();
    gAppName=label(L"WinPuttier",8000);
    gAppSub=label(L"PuTTY Theme Manager  ·  Libreria offline  ·  Versione 0.4",8010);
    gSessionTitle=label(L"01   SESSIONI PuTTY",8001);
    gThemesTitle=label(L"02   LIBRERIA TEMI",8002);
    gPreviewTitle=label(L"03   ANTEPRIMA ANSI · SCORRI PER ESPLORARE",8003);
    gPaletteTitle=label(L"EDITOR PALETTE    ·    22 colori",8004);
    gSessions=make(L"LISTBOX",L"",WS_TABSTOP|WS_VSCROLL|LBS_NOTIFY|WS_BORDER,ID_SESSIONS);
    gThemes=make(L"LISTBOX",L"",WS_TABSTOP|WS_VSCROLL|LBS_NOTIFY|WS_BORDER,ID_THEMES);
    gSourceFilter=make(L"COMBOBOX",L"",WS_TABSTOP|WS_VSCROLL|CBS_DROPDOWNLIST,ID_SOURCE_FILTER);
    gSearchLabel=label(L"RICERCA TEMI",8007);
    gLibraryStats=label(L"Libreria locale",8008);
    gSearch=edit(L"",ID_SEARCH);
    SendMessageW(gSearch,EM_SETCUEBANNER,TRUE,(LPARAM)L"Digita il nome del tema...");
    gReload=button(L"Rileggi sessioni",ID_RELOAD);
    gSaveSession=button(L"Crea tema dalla sessione",ID_SAVE_SESSION);
    gApply=button(L"Applica tema",ID_APPLY);
    gRestore=button(L"Ripristina colori originali",ID_RESTORE);
    gRefresh=button(L"Sincronizza libreria",ID_REFRESH);
    gSources=button(L"Gestisci fonti",ID_SOURCES);
    gImport=button(L"Importa .reg",ID_IMPORT);
    gFavoriteButton=button(L"☆ Aggiungi preferito",ID_FAVORITE);
    gPreview=wp::createInlinePreview(gWindow,ID_PREVIEW);
    gColorTitle=label(L"Seleziona uno dei 22 colori",ID_COLOR_TITLE);
    gColorRgb=label(L"RGB",ID_COLOR_RGB);
    for(int i=0;i<22;++i)
        gSwatches[i]=make(L"BUTTON",kColorNames[i],WS_TABSTOP|BS_OWNERDRAW,ID_SWATCH_FIRST+i);
    gHex=edit(L"#24283B",ID_HEX);
    gHexApply=button(L"Conferma HEX",ID_HEX_APPLY);
    gPicker=button(L"Selettore RGB",ID_PICKER);
    gReset=button(L"Annulla",ID_RESET);
    gPersonalName=edit(L"",ID_PERSONAL_NAME);
    SendMessageW(gPersonalName,EM_SETCUEBANNER,TRUE,(LPARAM)L"Nome del nuovo tema...");
    gSavePersonal=button(L"Salva personale",ID_SAVE_PERSONAL);
    gExport=button(L"Esporta .reg",ID_EXPORT);
    gStatus=label(L"Pronto",ID_STATUS);
    rebuildFonts();
}
LRESULT CALLBACK mainProc(HWND wnd,UINT msg,WPARAM w,LPARAM l){
    switch(msg){
    case WM_CREATE:
        gWindow=wnd;
        gDpi=static_cast<int>(GetDpiForWindow(wnd));
        createControls();
        if(!wp::ensureDirs())message(L"Non riesco a creare la cartella dati di WinPuttier.");
        refreshSessions();reloadCatalog();
        message(L"Seleziona la sessione a sinistra e il tema al centro; l'anteprima non modifica PuTTY.");
        return 0;
    case WM_GETMINMAXINFO: {
        auto* info=(MINMAXINFO*)l;
        info->ptMinTrackSize.x=pix(985);info->ptMinTrackSize.y=pix(630);return 0;
    }
    case WM_SIZE:layout();return 0;
    case WM_DPICHANGED:{
        gDpi=HIWORD(w);
        const RECT* rect=reinterpret_cast<const RECT*>(l);
        SetWindowPos(wnd,nullptr,rect->left,rect->top,rect->right-rect->left,
            rect->bottom-rect->top,SWP_NOACTIVATE|SWP_NOZORDER);
        rebuildFonts();
        layout();
        refreshColorEditor();
        InvalidateRect(wnd,nullptr,TRUE);
        return 0;
    }
    case WM_ERASEBKGND:return 1;
    case WM_PAINT:paintChrome(wnd);return 0;
    case WM_CTLCOLORSTATIC:{
        HDC dc=(HDC)w;
        HWND child=(HWND)l;
        SetBkMode(dc,TRANSPARENT);
        SetTextColor(dc,(child==gStatus||child==gAppSub||child==gLibraryStats)?
                     RGB(102,114,132):RGB(35,53,80));
        return (LRESULT)((child==gAppName||child==gAppSub||child==gStatus)?
                         gBackgroundBrush:gCardBrush);
    }
    case WM_CTLCOLORLISTBOX:
    case WM_CTLCOLOREDIT:
        SetBkColor((HDC)w,RGB(255,255,255));
        SetTextColor((HDC)w,RGB(38,54,77));
        return (LRESULT)gCardBrush;
    case WM_DRAWITEM: {
        auto* ds=(DRAWITEMSTRUCT*)l;
        if(ds->CtlID>=ID_SWATCH_FIRST&&ds->CtlID<ID_SWATCH_FIRST+22){
            redrawSwatch(ds,(int)ds->CtlID-ID_SWATCH_FIRST);return TRUE;
        }
        if(ds->CtlID>=201 && ds->CtlID<500) {drawButton(ds);return TRUE;}
        break;
    }
    case WM_COMMAND:{
        int id=LOWORD(w),notification=HIWORD(w);
        if(id==ID_SESSIONS&&notification==LBN_SELCHANGE){
            refreshColorEditor();return 0;
        }
        if(id==ID_THEMES&&notification==LBN_SELCHANGE){
            applyThemeSelection();return 0;
        }
        if(id==ID_SOURCE_FILTER&&notification==CBN_SELCHANGE){
            int ix=(int)SendMessageW(gSourceFilter,CB_GETCURSEL,0,0);
            if(ix>=0){
                int n=(int)SendMessageW(gSourceFilter,CB_GETLBTEXTLEN,ix,0);
                std::wstring choice((size_t)n+1,L'\0');
                SendMessageW(gSourceFilter,CB_GETLBTEXT,ix,(LPARAM)&choice[0]);
                choice.resize(n);gSelectedSource=choice;
                gDirty=false;gThemeKey.clear();filterThemes();
            }
            return 0;
        }
        if(id==ID_SEARCH&&notification==EN_CHANGE){
            gDirty=false;gThemeKey.clear();filterThemes();return 0;
        }
        if(id>=ID_SWATCH_FIRST&&id<ID_SWATCH_FIRST+22&&notification==BN_CLICKED){
            selectColor(id-ID_SWATCH_FIRST);return 0;
        }
        switch(id){
        case ID_RELOAD:refreshSessions();message(L"Sessioni PuTTY ricaricate.");break;
        case ID_SAVE_SESSION:saveSessionAsTheme();break;
        case ID_APPLY:applyToSession();break;
        case ID_RESTORE:restoreOriginal();break;
        case ID_REFRESH:syncAsync();break;
        case ID_SOURCES:manageSources();break;
        case ID_IMPORT:importReg();break;
        case ID_FAVORITE:{
            if(!gHaveTheme)break;
            const bool marked=wp::isFavorite(gSelectedTheme,gFavorites);
            if(!wp::setFavorite(gSelectedTheme,!marked)){
                message(L"Impossibile salvare i preferiti nella cartella locale.");
                break;
            }
            gFavorites=wp::favoriteIds();
            filterThemes(gThemeKey);
            message(marked?L"Tema rimosso dai preferiti.":L"Tema aggiunto ai preferiti.");
            break;
        }
        case ID_PICKER:colorPicker();break;
        case ID_HEX_APPLY:applyHex();break;
        case ID_RESET:resetChanges();break;
        case ID_SAVE_PERSONAL:savePersonalTheme(gSelectedTheme);break;
        case ID_EXPORT:exportReg();break;
        }
        return 0;
    }
    case WM_SYNC_PROGRESS:{
        std::unique_ptr<std::pair<size_t,size_t>> p((std::pair<size_t,size_t>*)l);
        if(gSyncing)message(L"Sincronizzazione file .reg: "+std::to_wstring(p->first)+
                             L" / "+std::to_wstring(p->second));
        return 0;
    }
    case WM_SYNC_DONE:{
        std::unique_ptr<SyncResult> result((SyncResult*)l);
        gSyncing=false;
        EnableWindow(gRefresh,TRUE);
        reloadCatalog();
        message(result->message);
        return 0;
    }
    case WM_CLOSE:DestroyWindow(wnd);return 0;
    case WM_DESTROY:
        if(gMono)DeleteObject(gMono);
        if(gFont)DeleteObject(gFont);
        if(gTitleFont)DeleteObject(gTitleFont);
        if(gSectionFont)DeleteObject(gSectionFont);
        if(gSmallFont)DeleteObject(gSmallFont);
        if(gBackgroundBrush)DeleteObject(gBackgroundBrush);
        if(gCardBrush)DeleteObject(gCardBrush);
        gWindow=nullptr;
        PostQuitMessage(0);return 0;
    }
    return DefWindowProcW(wnd,msg,w,l);
}
} // namespace

int WINAPI wWinMain(HINSTANCE app,HINSTANCE,LPWSTR,int show){
    // Windows 10: monitor-aware native rendering, never bitmap-stretch controls.
    SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
    INITCOMMONCONTROLSEX common{};common.dwSize=sizeof(common);common.dwICC=ICC_WIN95_CLASSES;
    InitCommonControlsEx(&common);
    WNDCLASSEXW cls{};cls.cbSize=sizeof(cls);cls.hInstance=app;
    cls.lpfnWndProc=mainProc;cls.lpszClassName=L"WinPuttierNative";
    cls.hCursor=LoadCursorW(nullptr,IDC_ARROW);
    cls.hIcon=LoadIconW(app,MAKEINTRESOURCEW(101));
    if(!cls.hIcon)cls.hIcon=LoadIconW(nullptr,IDI_APPLICATION);
    cls.hIconSm=(HICON)LoadImageW(app,MAKEINTRESOURCEW(101),IMAGE_ICON,
        GetSystemMetrics(SM_CXSMICON),GetSystemMetrics(SM_CYSMICON),LR_DEFAULTCOLOR);
    cls.hbrBackground=(HBRUSH)(COLOR_WINDOW+1);
    if(!RegisterClassExW(&cls))return 1;
    HWND wnd=CreateWindowExW(0,cls.lpszClassName,L"WinPuttier - Theme Manager",
         WS_OVERLAPPEDWINDOW|WS_CLIPCHILDREN,CW_USEDEFAULT,CW_USEDEFAULT,
         1320,820,nullptr,nullptr,app,nullptr);
    if(!wnd)return 1;
    ShowWindow(wnd,show);UpdateWindow(wnd);
    MSG msg{};
    while(GetMessageW(&msg,nullptr,0,0)>0){
        if(!IsDialogMessageW(wnd,&msg)){TranslateMessage(&msg);DispatchMessageW(&msg);}
    }
    return (int)msg.wParam;
}
