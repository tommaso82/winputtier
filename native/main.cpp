#include "core.h"
#include <windows.h>
#include <commctrl.h>
#include <commdlg.h>
#include <algorithm>
#include <cstdio>
#include <memory>
#include <thread>
#include <vector>
#pragma comment(lib, "comctl32.lib")
#pragma comment(lib, "comdlg32.lib")
#pragma comment(lib, "gdi32.lib")
#pragma comment(lib, "user32.lib")

namespace {
enum : int {
    ID_SESSIONS=101, ID_THEMES=102, ID_PREVIEW=103, ID_COLORS=104, ID_STATUS=105,
    ID_RELOAD=201, ID_IMPORT=202, ID_CATALOG=203, ID_DOWNLOAD=204,
    ID_EDIT=205, ID_SAVE=206, ID_EXPORT=207, ID_APPLY=208, ID_RESTORE=209
};
constexpr UINT WM_CATALOG_DONE=WM_APP+10;
HWND gWindow=nullptr,gSessions=nullptr,gThemes=nullptr,gPreview=nullptr,gColors=nullptr,gStatus=nullptr;
HWND gReload=nullptr,gImport=nullptr,gCatalog=nullptr,gDownload=nullptr,gEdit=nullptr,gSave=nullptr,gExport=nullptr,gApply=nullptr,gRestore=nullptr;
HWND gSessionLabel=nullptr,gThemeLabel=nullptr,gColorLabel=nullptr;
HFONT gFont=nullptr;
bool gRefreshing=false, gDirty=false, gHasTheme=false;
std::vector<wp::Session> gSessionList;
std::vector<wp::Theme> gThemeList;
wp::Theme gTheme;

const wchar_t* kColorNames[22]={
 L"Testo",L"Testo grassetto",L"Sfondo",L"Sfondo grassetto",L"Testo del cursore",L"Cursore",
 L"Nero",L"Nero brillante",L"Rosso",L"Rosso brillante",L"Verde",L"Verde brillante",
 L"Giallo",L"Giallo brillante",L"Blu",L"Blu brillante",L"Magenta",L"Magenta brillante",
 L"Ciano",L"Ciano brillante",L"Bianco",L"Bianco brillante"
};
COLORREF color(const wp::Color& c) { return RGB(c.r,c.g,c.b); }
void status(const std::wstring& value) { SetWindowTextW(gStatus,value.c_str()); }
HWND control(const wchar_t* cls,const wchar_t* text,DWORD style,int id) {
    HWND h=CreateWindowExW(0,cls,text,WS_CHILD|WS_VISIBLE|style,0,0,100,28,gWindow,
                         (HMENU)(INT_PTR)id,GetModuleHandleW(nullptr),nullptr);
    if(h&&gFont)SendMessageW(h,WM_SETFONT,(WPARAM)gFont,TRUE);
    return h;
}
HWND button(const wchar_t* label,int id){
    return control(L"BUTTON",label,WS_TABSTOP|BS_PUSHBUTTON,id);
}
int sessionIndex(){return (int)SendMessageW(gSessions,LB_GETCURSEL,0,0);}
int themeIndex(){return (int)SendMessageW(gThemes,LB_GETCURSEL,0,0);}
void resizeControls() {
    RECT rect{};GetClientRect(gWindow,&rect);
    int width=rect.right,height=rect.bottom;
    if(width<750||height<550)return;
    const int gap=12;
    int leftWidth=(width-3*gap)*35/100;
    int topHeight=std::max(170,height-337);
    MoveWindow(gSessionLabel,gap,8,leftWidth,22,TRUE);
    MoveWindow(gThemeLabel,leftWidth+gap*2,8,width-leftWidth-gap*3,22,TRUE);
    MoveWindow(gSessions,gap,31,leftWidth,topHeight,TRUE);
    MoveWindow(gThemes,leftWidth+gap*2,31,width-leftWidth-gap*3,topHeight,TRUE);

    HWND upper[]={gReload,gImport,gCatalog,gDownload,gEdit,gSave,gExport};
    int y=topHeight+40,space=6;
    int remaining=width-2*gap-space*6, each=remaining/7;
    for(int i=0;i<7;++i)MoveWindow(upper[i],gap+i*(each+space),y,(i==6?width-gap-(gap+i*(each+space)):each),29,TRUE);

    int previewY=y+39;
    MoveWindow(gPreview,gap,previewY,width-gap*2,std::max(80,height-previewY-103),TRUE);
    int bottom=height-94;
    MoveWindow(gColorLabel,gap,bottom+5,59,23,TRUE);
    MoveWindow(gColors,gap+64,bottom,210,300,TRUE);
    int x=gap+284;
    int spaceForButtons=width-x-gap;
    MoveWindow(gApply,x,bottom,spaceForButtons/2-5,31,TRUE);
    MoveWindow(gRestore,x+spaceForButtons/2+5,bottom,spaceForButtons/2-5,31,TRUE);
    MoveWindow(gStatus,gap,height-49,width-gap*2,29,TRUE);
}
void loadSessions() {
    int old=sessionIndex();
    gSessionList=wp::sessions();
    SendMessageW(gSessions,LB_RESETCONTENT,0,0);
    for(const auto& s:gSessionList)SendMessageW(gSessions,LB_ADDSTRING,0,(LPARAM)s.label.c_str());
    if(!gSessionList.empty())SendMessageW(gSessions,LB_SETCURSEL,std::min(std::max(0,old),(int)gSessionList.size()-1),0);
}
void chooseTheme() {
    int idx=themeIndex();
    gHasTheme=idx>=0&&idx<(int)gThemeList.size();
    gDirty=false;
    if(gHasTheme)gTheme=gThemeList[idx];
    EnableWindow(gEdit,gHasTheme&&gTheme.available);
    EnableWindow(gSave,gHasTheme&&gTheme.available);
    EnableWindow(gExport,gHasTheme&&gTheme.available);
    EnableWindow(gApply,gHasTheme&&gTheme.available);
    EnableWindow(gDownload,gHasTheme&&!gTheme.available);
    InvalidateRect(gPreview,nullptr,TRUE);
}
void loadThemes(const std::wstring& focusName=L"") {
    int old=themeIndex();
    gThemeList=wp::localThemes();
    auto online=wp::remoteCatalog();
    for(auto& t:online) {
        auto path=wp::cacheDir()+L"\\"+wp::safeFileName(t.source+L"__"+t.name)+L".reg";
        if(GetFileAttributesW(path.c_str())==INVALID_FILE_ATTRIBUTES)gThemeList.push_back(std::move(t));
    }
    if(gThemeList.size()>1) {
        std::sort(gThemeList.begin()+1,gThemeList.end(),[](const wp::Theme& a,const wp::Theme& b){
            return _wcsicmp(a.name.c_str(),b.name.c_str())<0;
        });
    }
    SendMessageW(gThemes,LB_RESETCONTENT,0,0);
    int focused=-1;
    for(size_t i=0;i<gThemeList.size();++i) {
        const auto& t=gThemeList[i];
        std::wstring label=(t.available?L"[Locale] ":L"[Online] ")+t.name+L" ("+t.source+L")";
        SendMessageW(gThemes,LB_ADDSTRING,0,(LPARAM)label.c_str());
        if(!focusName.empty()&&t.name==focusName)focused=(int)i;
    }
    if(!gThemeList.empty()) {
        int sel=focused>=0?focused:std::min(std::max(old,0),(int)gThemeList.size()-1);
        SendMessageW(gThemes,LB_SETCURSEL,sel,0);
    }
    chooseTheme();
}
bool dialogFile(bool saving,const wchar_t* filter,std::wstring& target,const std::wstring& suggestion=L"") {
    wchar_t buffer[32768]{};
    if(!suggestion.empty())wcsncpy_s(buffer,suggestion.c_str(),_TRUNCATE);
    OPENFILENAMEW info{};info.lStructSize=sizeof(info);info.hwndOwner=gWindow;
    info.lpstrFile=buffer;info.nMaxFile=_countof(buffer);info.lpstrFilter=filter;info.lpstrDefExt=L"reg";
    info.Flags=OFN_EXPLORER|OFN_PATHMUSTEXIST|(saving?OFN_OVERWRITEPROMPT:OFN_FILEMUSTEXIST);
    BOOL ok=saving?GetSaveFileNameW(&info):GetOpenFileNameW(&info);
    if(!ok)return false;
    target=buffer;return true;
}
bool saveTextFile(const std::wstring& path,const std::wstring& text) {
    FILE* f=nullptr;
    if(_wfopen_s(&f,path.c_str(),L"wb")!=0||!f)return false;
    const unsigned char bom[2]={0xff,0xfe};
    bool ok=fwrite(bom,1,2,f)==2 &&
        (text.empty()||fwrite(text.data(),sizeof(wchar_t),text.size(),f)==text.size());
    return fclose(f)==0&&ok;
}
void draw(DRAWITEMSTRUCT* info) {
    RECT rc=info->rcItem;
    auto& palette=(gHasTheme&&gTheme.available)?gTheme.colors:wp::defaultPalette();
    HBRUSH background=CreateSolidBrush(color(palette[2]));
    FillRect(info->hDC,&rc,background);
    DeleteObject(background);
    SetBkMode(info->hDC,TRANSPARENT);
    HFONT previewFont=CreateFontW(-17,0,0,0,FW_NORMAL,0,0,0,DEFAULT_CHARSET,OUT_DEFAULT_PRECIS,
                                  CLIP_DEFAULT_PRECIS,CLEARTYPE_QUALITY,FIXED_PITCH,L"Consolas");
    HGDIOBJ previous=SelectObject(info->hDC,previewFont);
    int x=rc.left+17,y=rc.top+10;
    auto line=[&](const wchar_t* value,int idx) {
        SetTextColor(info->hDC,color(palette[idx]));
        TextOutW(info->hDC,x,y,value,(int)wcslen(value));
        y+=26;
    };
    std::wstring title=gHasTheme?gTheme.name:L"Seleziona un tema";
    if(gDirty)title+=L"  (modificato)";
    line(title.c_str(),0);
    if(gHasTheme&&!gTheme.available) {
        line(L"Tema online: premi 'Scarica selezionato' per vedere l'anteprima.",0);
    } else {
        line(L"user@server:~$ ls -la --color=always",10);
        line(L"drwxr-xr-x  2 user users 4096 Nov 17 projects",18);
        line(L"-rw-r--r--  1 user users  248 Nov 17 notes.txt",12);
        int sw=std::max(12,std::min(44,static_cast<int>((rc.right-x-35)/16-5)));
        for(int i=0;i<16;++i) {
            RECT patch{x+i*(sw+5),y,x+i*(sw+5)+sw,y+22};
            HBRUSH b=CreateSolidBrush(color(palette[6+i]));
            FillRect(info->hDC,&patch,b);DeleteObject(b);
            FrameRect(info->hDC,&patch,(HBRUSH)GetStockObject(GRAY_BRUSH));
        }
    }
    SelectObject(info->hDC,previous);
    DeleteObject(previewFont);
}
void refreshCatalogAsync() {
    if(gRefreshing)return;
    gRefreshing=true;
    EnableWindow(gCatalog,FALSE);
    status(L"Aggiornamento catalogo temi in corso...");
    HWND destination=gWindow;
    std::thread([destination](){
        auto msg=new std::wstring;
        wp::refreshCatalog(*msg);
        if(!PostMessageW(destination,WM_CATALOG_DONE,0,(LPARAM)msg))delete msg;
    }).detach();
}
void importTheme() {
    std::wstring path,message;
    if(!dialogFile(false,L"Temi PuTTY (*.reg)\0*.reg\0Tutti i file\0*.*\0",path))return;
    if(wp::importTheme(path,message))loadThemes();
    status(message);
}
void downloadTheme() {
    int idx=themeIndex();
    if(idx<0||idx>=(int)gThemeList.size()||gThemeList[idx].available)return;
    status(L"Scaricamento del tema selezionato...");
    std::wstring error;
    std::wstring name=gThemeList[idx].name;
    if(wp::downloadTheme(gThemeList[idx],error)) {
        loadThemes(name);
        status(L"Tema scaricato: "+name);
    } else status(L"Download non riuscito: "+error);
}
void editColor() {
    if(!gHasTheme||!gTheme.available)return;
    int idx=(int)SendMessageW(gColors,CB_GETCURSEL,0,0);
    if(idx<0||idx>=22)return;
    static COLORREF customColors[16]{};
    CHOOSECOLORW cc{};cc.lStructSize=sizeof(cc);cc.hwndOwner=gWindow;
    cc.rgbResult=color(gTheme.colors[idx]);cc.lpCustColors=customColors;cc.Flags=CC_RGBINIT|CC_FULLOPEN;
    if(!ChooseColorW(&cc))return;
    gTheme.colors[idx]={(int)GetRValue(cc.rgbResult),(int)GetGValue(cc.rgbResult),(int)GetBValue(cc.rgbResult)};
    gDirty=true;InvalidateRect(gPreview,nullptr,TRUE);
    status(L"Colore modificato. Salva una copia personale per conservare le modifiche.");
}
void saveTheme(bool exportForSession) {
    if(!gHasTheme||!gTheme.available)return;
    int si=sessionIndex();
    if(exportForSession&&(si<0||si>=(int)gSessionList.size())) {
        status(L"Seleziona prima una sessione PuTTY per esportare il tema.");
        return;
    }
    std::wstring path,err;
    auto filename=wp::safeFileName(gTheme.name)+(exportForSession?L"-export.reg":L"-personal.reg");
    auto suggested=(exportForSession?wp::dataDir():wp::customDir())+L"\\"+filename;
    if(!dialogFile(true,L"Temi PuTTY (*.reg)\0*.reg\0",path,suggested))return;
    bool ok=exportForSession?
        saveTextFile(path,wp::exportReg(gTheme,gSessionList[si].key)):
        wp::saveCustomTheme(gTheme,path,err);
    if(ok) {
        if(!exportForSession)loadThemes();
        status(exportForSession?L"Tema esportato in .reg (non applicato).":L"Nuovo tema personale salvato.");
    } else status(L"Impossibile scrivere il file .reg.");
}
void apply() {
    int si=sessionIndex();
    if(si<0||si>=(int)gSessionList.size()||!gHasTheme||!gTheme.available) {
        status(L"Seleziona una sessione e un tema locale.");return;
    }
    std::wstring prompt=L"Applicare il tema '"+gTheme.name+L"' alla sola sessione '"+gSessionList[si].label+
        L"'?\nUn backup dei colori attuali verra' creato automaticamente.";
    if(MessageBoxW(gWindow,prompt.c_str(),L"WinPuttier - Conferma",MB_YESNO|MB_ICONQUESTION)!=IDYES)return;
    std::wstring message;
    wp::applyTheme(gSessionList[si],gTheme,message);
    status(message);
}
void restore() {
    int si=sessionIndex();
    if(si<0||si>=(int)gSessionList.size()){status(L"Seleziona una sessione.");return;}
    std::wstring prompt=L"Ripristinare l'ultimo backup dei colori della sessione '"+gSessionList[si].label+L"'?";
    if(MessageBoxW(gWindow,prompt.c_str(),L"WinPuttier - Ripristino",MB_YESNO|MB_ICONQUESTION)!=IDYES)return;
    std::wstring message;
    wp::restoreLastBackup(gSessionList[si],message);
    status(message);
}
LRESULT CALLBACK proc(HWND hwnd,UINT msg,WPARAM wpv,LPARAM lpv) {
    switch(msg) {
    case WM_CREATE:{
        gWindow=hwnd;
        gFont=CreateFontW(-15,0,0,0,FW_NORMAL,0,0,0,DEFAULT_CHARSET,OUT_DEFAULT_PRECIS,
                          CLIP_DEFAULT_PRECIS,CLEARTYPE_QUALITY,DEFAULT_PITCH,L"Segoe UI");
        gSessionLabel=control(L"STATIC",L"Sessioni PuTTY",0,400);
        gThemeLabel=control(L"STATIC",L"Temi disponibili (locale / online)",0,401);
        gSessions=control(L"LISTBOX",L"",WS_BORDER|WS_VSCROLL|WS_TABSTOP|LBS_NOTIFY,ID_SESSIONS);
        gThemes=control(L"LISTBOX",L"",WS_BORDER|WS_VSCROLL|WS_TABSTOP|LBS_NOTIFY,ID_THEMES);
        gReload=button(L"Rileggi sessioni",ID_RELOAD);
        gImport=button(L"Importa .reg",ID_IMPORT);
        gCatalog=button(L"Aggiorna catalogo",ID_CATALOG);
        gDownload=button(L"Scarica tema",ID_DOWNLOAD);
        gEdit=button(L"Modifica colore",ID_EDIT);
        gSave=button(L"Salva personale",ID_SAVE);
        gExport=button(L"Esporta .reg",ID_EXPORT);
        gPreview=control(L"STATIC",L"",SS_OWNERDRAW|WS_BORDER,ID_PREVIEW);
        gColorLabel=control(L"STATIC",L"Colore:",0,402);
        gColors=control(L"COMBOBOX",L"",CBS_DROPDOWNLIST|WS_VSCROLL|WS_TABSTOP,ID_COLORS);
        for(const auto* name:kColorNames)SendMessageW(gColors,CB_ADDSTRING,0,(LPARAM)name);
        SendMessageW(gColors,CB_SETCURSEL,2,0);
        gApply=button(L"Applica alla sessione selezionata",ID_APPLY);
        gRestore=button(L"Ripristina ultimo backup",ID_RESTORE);
        gStatus=control(L"STATIC",L"Pronto",0,ID_STATUS);
        wp::ensureDirs();
        loadSessions();
        loadThemes();
        status(L"Seleziona una sessione e un tema. Le altre sessioni non saranno modificate.");
        return 0;
    }
    case WM_SIZE:resizeControls();return 0;
    case WM_DRAWITEM:
        if(wpv==ID_PREVIEW){draw((DRAWITEMSTRUCT*)lpv);return TRUE;}break;
    case WM_COMMAND:{
        int id=LOWORD(wpv),event=HIWORD(wpv);
        if(id==ID_THEMES&&event==LBN_SELCHANGE){chooseTheme();return 0;}
        if(id==ID_RELOAD){loadSessions();status(L"Elenco sessioni aggiornato.");}
        else if(id==ID_IMPORT)importTheme();
        else if(id==ID_CATALOG)refreshCatalogAsync();
        else if(id==ID_DOWNLOAD)downloadTheme();
        else if(id==ID_EDIT)editColor();
        else if(id==ID_SAVE)saveTheme(false);
        else if(id==ID_EXPORT)saveTheme(true);
        else if(id==ID_APPLY)apply();
        else if(id==ID_RESTORE)restore();
        return 0;
    }
    case WM_CATALOG_DONE: {
        std::unique_ptr<std::wstring> message((std::wstring*)lpv);
        gRefreshing=false;
        EnableWindow(gCatalog,TRUE);
        loadThemes();
        status(*message);
        return 0;
    }
    case WM_CLOSE:DestroyWindow(hwnd);return 0;
    case WM_DESTROY:
        if(gFont)DeleteObject(gFont);
        gWindow=nullptr;
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcW(hwnd,msg,wpv,lpv);
}
} // namespace

int WINAPI wWinMain(HINSTANCE instance,HINSTANCE,LPWSTR,int show) {
    INITCOMMONCONTROLSEX cc{};cc.dwSize=sizeof(cc);cc.dwICC=ICC_WIN95_CLASSES;
    InitCommonControlsEx(&cc);
    WNDCLASSEXW wc{};wc.cbSize=sizeof(wc);wc.hInstance=instance;
    wc.lpfnWndProc=proc;wc.lpszClassName=L"WinPuttierNativeWindow";
    wc.hCursor=LoadCursorW(nullptr,IDC_ARROW);
    wc.hIcon=LoadIconW(nullptr,IDI_APPLICATION);
    wc.hbrBackground=(HBRUSH)(COLOR_BTNFACE+1);
    if(!RegisterClassExW(&wc))return 1;
    HWND h=CreateWindowExW(0,wc.lpszClassName,L"WinPuttier - PuTTY Theme Manager",
                          WS_OVERLAPPEDWINDOW|WS_CLIPCHILDREN,
                          CW_USEDEFAULT,CW_USEDEFAULT,1020,755,nullptr,nullptr,instance,nullptr);
    if(!h)return 1;
    ShowWindow(h,show);UpdateWindow(h);
    MSG m{};
    while(GetMessageW(&m,nullptr,0,0)>0) {
        if(!IsDialogMessageW(h,&m)){TranslateMessage(&m);DispatchMessageW(&m);}
    }
    return (int)m.wParam;
}
