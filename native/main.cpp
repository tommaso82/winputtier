#include "core.h"
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
    ID_SWATCH_FIRST=500, ID_TIMER_PREVIEW=800,
    ID_MODAL_EDIT=901, ID_MODAL_SAVE=902, ID_MODAL_CANCEL=903
};
constexpr UINT WM_CATALOG_DONE=WM_APP+10;
constexpr UINT WM_DOWNLOAD_DONE=WM_APP+11;

const wchar_t* kColorNames[22]={
    L"Testo",L"Testo grassetto",L"Sfondo",L"Sfondo grassetto",L"Testo cursore",L"Cursore",
    L"Nero",L"Nero brillante",L"Rosso",L"Rosso brillante",L"Verde",L"Verde brillante",
    L"Giallo",L"Giallo brillante",L"Blu",L"Blu brillante",L"Magenta",L"Magenta brillante",
    L"Ciano",L"Ciano brillante",L"Bianco",L"Bianco brillante"
};
HWND gWindow=nullptr,gSessions=nullptr,gThemes=nullptr,gPreview=nullptr,gStatus=nullptr;
HWND gSessionTitle=nullptr,gThemesTitle=nullptr,gPreviewTitle=nullptr,gPaletteTitle=nullptr;
HWND gSourceFilter=nullptr,gSearch=nullptr,gColorTitle=nullptr,gColorRgb=nullptr;
HWND gHex=nullptr,gPersonalName=nullptr,gSwatches[22]{};
HWND gReload=nullptr,gSaveSession=nullptr,gApply=nullptr,gRestore=nullptr;
HWND gRefresh=nullptr,gSources=nullptr,gImport=nullptr,gDownload=nullptr;
HWND gPicker=nullptr,gHexApply=nullptr,gReset=nullptr,gSavePersonal=nullptr,gExport=nullptr;
HFONT gFont=nullptr,gMono=nullptr;
std::vector<wp::Session> gSessionItems;
std::vector<wp::Theme> gAllThemes;
std::vector<size_t> gVisibleThemes;
std::wstring gThemeKey, gPreviewFailedKey, gActiveDownloadKey, gSelectedSource=L"Tutte";
wp::Theme gSelectedTheme;
wp::Palette gUneditedPalette{};
bool gHaveTheme=false,gDirty=false,gRefreshing=false,gDownloading=false;
int gSelectedColor=2;
struct DownloadResult { std::wstring key,error; bool ok=false; };

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
HWND button(const wchar_t* label,int id){return make(L"BUTTON",label,WS_TABSTOP|BS_PUSHBUTTON,id);}
HWND edit(const wchar_t* text,int id){return make(L"EDIT",text,WS_TABSTOP|ES_AUTOHSCROLL,id,WS_EX_CLIENTEDGE);}
void move(HWND h,int x,int y,int w,int height){
    if(h)MoveWindow(h,x,y,std::max(1,w),std::max(1,height),TRUE);
}
void layout() {
    RECT r{};GetClientRect(gWindow,&r);
    const int W=r.right,H=r.bottom,gap=13,top=10;
    if(W<1050||H<720)return;
    const int usable=W-4*gap;
    const int a=usable*27/100;
    const int b=usable*35/100;
    const int c=usable-a-b;
    const int xa=gap,xb=xa+a+gap,xc=xb+b+gap;
    const int bottom=H-46;
    move(gSessionTitle,xa,top,a,25);
    move(gThemesTitle,xb,top,b,25);
    move(gPreviewTitle,xc,top,c,25);
    // Left: sessions and session-specific controls.
    move(gSessions,xa,42,a,H-267);
    int sy=H-211;
    move(gReload,xa,sy,a,30);sy+=38;
    move(gSaveSession,xa,sy,a,30);sy+=43;
    move(gApply,xa,sy,a,39);sy+=46;
    move(gRestore,xa,sy,a,34);
    // Middle: one stable source filter and a searchable theme catalog.
    move(gSourceFilter,xb,42,b,300);
    move(gSearch,xb,81,b,28);
    move(gThemes,xb,119,b,H-330);
    int by=H-194;
    move(gRefresh,xb,by,b/2-5,32);
    move(gSources,xb+b/2+5,by,b/2-5,32);by+=40;
    move(gImport,xb,by,b/2-5,32);
    move(gDownload,xb+b/2+5,by,b/2-5,32);
    // Right: live preview and the complete RGB/HEX editor.
    move(gPreview,xc,42,c,204);
    move(gPaletteTitle,xc,257,c,25);
    const int swGap=5,swW=(c-10*swGap)/11;
    for(int i=0;i<22;++i){
        move(gSwatches[i],xc+(i%11)*(swW+swGap),288+(i/11)*35,swW,29);
    }
    move(gColorTitle,xc,373,c,25);
    move(gColorRgb,xc,399,c,23);
    move(gHex,xc,432,c*38/100,29);
    const int rx=xc+c*38/100+7, rem=c-c*38/100-7;
    move(gHexApply,rx,432,rem,29);
    int actionsY=470;
    move(gPicker,xc,actionsY,c/2-4,32);
    move(gReset,xc+c/2+4,actionsY,c/2-4,32);
    move(gPersonalName,xc,525,c,29);
    move(gSavePersonal,xc,564,c/2-4,33);
    move(gExport,xc+c/2+4,564,c/2-4,33);
    move(gStatus,gap,bottom,W-2*gap,30);
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
    EnableWindow(gDownload,gHaveTheme&&!usable);
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
    redrawPreview();
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
    if(!theme->available){
        message(L"Caricamento dell'anteprima dal catalogo online...");
        KillTimer(gWindow,ID_TIMER_PREVIEW);
        if(!gDownloading && gPreviewFailedKey!=gThemeKey)
            SetTimer(gWindow,ID_TIMER_PREVIEW,280,nullptr);
    }
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
        if(gSelectedSource!=L"Tutte"&&gSelectedSource!=t.source)continue;
        const auto haystack=toLower(t.name+L" "+t.source);
        if(!query.empty()&&haystack.find(query)==std::wstring::npos)continue;
        std::wstring name=(t.available?L"[Locale] ":L"[Online] ")+t.name;
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
void reloadCatalog(const std::wstring& focus=L""){
    const auto selected=focus.empty()?gThemeKey:focus;
    gAllThemes=wp::localThemes();
    auto remote=wp::remoteCatalog();
    for(auto& item:remote){
        bool found=false;
        for(const auto& cached:gAllThemes){
            if(cached.available&&cached.source==item.source&&cached.name==item.name){
                found=true;break;
            }
        }
        if(!found)gAllThemes.push_back(std::move(item));
    }
    if(gAllThemes.size()>1)
        std::sort(gAllThemes.begin()+1,gAllThemes.end(),[](const wp::Theme& a,const wp::Theme& b){
            int n=_wcsicmp(a.name.c_str(),b.name.c_str());
            return n<0||(n==0&&_wcsicmp(a.source.c_str(),b.source.c_str())<0);
        });
    populateSourceFilter();
    filterThemes(selected);
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
void drawPreview(DRAWITEMSTRUCT* ds){
    RECT r=ds->rcItem;
    const auto& palette=gHaveTheme&&gSelectedTheme.available?gSelectedTheme.colors:wp::defaultPalette();
    HBRUSH bg=CreateSolidBrush(rgb(palette[2]));FillRect(ds->hDC,&r,bg);DeleteObject(bg);
    SetBkMode(ds->hDC,TRANSPARENT);
    HFONT old=(HFONT)SelectObject(ds->hDC,gMono);
    int x=r.left+15,y=r.top+12;
    auto add=[&](int at,int col,const std::wstring& text){
        SetTextColor(ds->hDC,rgb(palette[col]));
        TextOutW(ds->hDC,at,y,text.c_str(),(int)text.size());
    };
    if(!gHaveTheme){
        add(x,0,L"Seleziona un tema per vedere l'anteprima.");
    }else if(!gSelectedTheme.available){
        add(x,0,L"Anteprima online");
        y+=30;
        add(x,0,L"Scaricamento della palette in corso...");
    }else{
        add(x,0,L"user@server:");
        add(x+111,14,L"~/projects");
        add(x+227,0,L"$ ls --color=auto");
        y+=32;
        add(x,14,L"documents/");add(x+137,10,L"backup.sh");add(x+265,0,L"notes.txt");
        y+=29;
        add(x,12,L"warning:");add(x+104,0,L" 2 changes pending");
        y+=29;
        add(x,8,L"error:");add(x+81,0,L" command not found");
        y+=28;
        int innerWidth=std::max(80,static_cast<int>(r.right-r.left-32)),square=(innerWidth-15*4)/16;
        square=std::min(square,40);
        for(int i=0;i<16;++i){
            RECT block{x+i*(square+4),y,x+i*(square+4)+square,y+20};
            HBRUSH b=CreateSolidBrush(rgb(palette[6+i]));
            FillRect(ds->hDC,&block,b);DeleteObject(b);
            FrameRect(ds->hDC,&block,(HBRUSH)GetStockObject(GRAY_BRUSH));
        }
    }
    SelectObject(ds->hDC,old);
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
void downloadAsync(const wp::Theme& item){
    if(gDownloading||item.available||item.remoteUrl.empty())return;
    gDownloading=true;
    gActiveDownloadKey=keyOf(item);
    EnableWindow(gDownload,FALSE);
    const HWND target=gWindow;
    const std::wstring key=gActiveDownloadKey;
    message(L"Scaricamento palette per anteprima: "+item.name+L"...");
    std::thread([item,target,key](){
        auto* result=new DownloadResult{};
        result->key=key;
        auto copy=item;
        result->ok=wp::downloadTheme(copy,result->error);
        if(!PostMessageW(target,WM_DOWNLOAD_DONE,0,(LPARAM)result))delete result;
    }).detach();
}
void refreshAsync(){
    if(gRefreshing)return;
    gRefreshing=true;EnableWindow(gRefresh,FALSE);
    message(L"Aggiornamento catalogo dalle fonti configurate...");
    const HWND target=gWindow;
    std::thread([target](){
        auto* result=new std::wstring;
        wp::refreshCatalog(*result);
        if(!PostMessageW(target,WM_CATALOG_DONE,0,(LPARAM)result))delete result;
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
            L"Una fonte per riga: Nome = URL HTTPS GitHub API /contents/... oppure raw GitHub .reg",
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
    if(modal.saved)refreshAsync();
}
void createControls(){
    gFont=CreateFontW(-16,0,0,0,FW_NORMAL,FALSE,FALSE,FALSE,DEFAULT_CHARSET,
        OUT_DEFAULT_PRECIS,CLIP_DEFAULT_PRECIS,CLEARTYPE_QUALITY,DEFAULT_PITCH,L"Segoe UI");
    gMono=CreateFontW(-17,0,0,0,FW_NORMAL,FALSE,FALSE,FALSE,DEFAULT_CHARSET,
        OUT_DEFAULT_PRECIS,CLIP_DEFAULT_PRECIS,CLEARTYPE_QUALITY,FIXED_PITCH,L"Consolas");
    gSessionTitle=label(L"1  SESSIONI PuTTY",8001);
    gThemesTitle=label(L"2  LIBRERIA TEMI",8002);
    gPreviewTitle=label(L"3  ANTEPRIMA",8003);
    gPaletteTitle=label(L"MODIFICA PALETTE  -  seleziona uno dei 22 colori",8004);
    gSessions=make(L"LISTBOX",L"",WS_TABSTOP|WS_VSCROLL|LBS_NOTIFY|WS_BORDER,ID_SESSIONS);
    gThemes=make(L"LISTBOX",L"",WS_TABSTOP|WS_VSCROLL|LBS_NOTIFY|WS_BORDER,ID_THEMES);
    gSourceFilter=make(L"COMBOBOX",L"",WS_TABSTOP|WS_VSCROLL|CBS_DROPDOWNLIST,ID_SOURCE_FILTER);
    gSearch=edit(L"",ID_SEARCH);
    SendMessageW(gSearch,EM_SETCUEBANNER,TRUE,(LPARAM)L"Cerca tema o fonte...");
    gReload=button(L"Rileggi sessioni",ID_RELOAD);
    gSaveSession=button(L"Salva colori sessione come tema",ID_SAVE_SESSION);
    gApply=button(L"Applica tema alla sessione",ID_APPLY);
    gRestore=button(L"Ripristina colori ORIGINALI",ID_RESTORE);
    gRefresh=button(L"Aggiorna catalogo",ID_REFRESH);
    gSources=button(L"Gestisci fonti...",ID_SOURCES);
    gImport=button(L"Importa .reg",ID_IMPORT);
    gDownload=button(L"Scarica selezionato",ID_DOWNLOAD);
    gPreview=make(L"STATIC",L"",WS_BORDER|SS_OWNERDRAW,ID_PREVIEW);
    gColorTitle=label(L"Seleziona un colore",ID_COLOR_TITLE);
    gColorRgb=label(L"RGB",ID_COLOR_RGB);
    for(int i=0;i<22;++i){
        gSwatches[i]=make(L"BUTTON",kColorNames[i],WS_TABSTOP|BS_OWNERDRAW,ID_SWATCH_FIRST+i);
    }
    gHex=edit(L"#24283B",ID_HEX);
    gHexApply=button(L"Aggiorna HEX",ID_HEX_APPLY);
    gPicker=button(L"Selettore RGB...",ID_PICKER);
    gReset=button(L"Annulla modifiche",ID_RESET);
    gPersonalName=edit(L"",ID_PERSONAL_NAME);
    gSavePersonal=button(L"Salva tema personale",ID_SAVE_PERSONAL);
    gExport=button(L"Esporta .reg",ID_EXPORT);
    gStatus=label(L"Pronto",ID_STATUS);
}
LRESULT CALLBACK mainProc(HWND wnd,UINT msg,WPARAM w,LPARAM l){
    switch(msg){
    case WM_CREATE:
        gWindow=wnd;
        createControls();
        if(!wp::ensureDirs())message(L"Non riesco a creare la cartella dati di WinPuttier.");
        refreshSessions();reloadCatalog();
        message(L"Seleziona la sessione a sinistra e il tema al centro; l'anteprima non modifica PuTTY.");
        return 0;
    case WM_GETMINMAXINFO: {
        auto* info=(MINMAXINFO*)l;
        info->ptMinTrackSize.x=1090;info->ptMinTrackSize.y=735;return 0;
    }
    case WM_SIZE:layout();return 0;
    case WM_DRAWITEM: {
        auto* ds=(DRAWITEMSTRUCT*)l;
        if(ds->CtlID==ID_PREVIEW){drawPreview(ds);return TRUE;}
        if(ds->CtlID>=ID_SWATCH_FIRST&&ds->CtlID<ID_SWATCH_FIRST+22){
            redrawSwatch(ds,(int)ds->CtlID-ID_SWATCH_FIRST);return TRUE;
        }
        break;
    }
    case WM_TIMER:{
        if(w==ID_TIMER_PREVIEW){
            KillTimer(wnd,ID_TIMER_PREVIEW);
            if(!gDownloading&&gHaveTheme&&!gSelectedTheme.available&&
               keyOf(gSelectedTheme)!=gPreviewFailedKey)
                downloadAsync(gSelectedTheme);
            return 0;
        }
        break;
    }
    case WM_COMMAND:{
        int id=LOWORD(w),notification=HIWORD(w);
        if(id==ID_SESSIONS&&notification==LBN_SELCHANGE){
            refreshColorEditor();return 0;
        }
        if(id==ID_THEMES&&notification==LBN_SELCHANGE){
            KillTimer(wnd,ID_TIMER_PREVIEW);
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
        case ID_REFRESH:refreshAsync();break;
        case ID_SOURCES:manageSources();break;
        case ID_IMPORT:importReg();break;
        case ID_DOWNLOAD:
            if(gHaveTheme&&!gSelectedTheme.available){gPreviewFailedKey.clear();downloadAsync(gSelectedTheme);}
            break;
        case ID_PICKER:colorPicker();break;
        case ID_HEX_APPLY:applyHex();break;
        case ID_RESET:resetChanges();break;
        case ID_SAVE_PERSONAL:savePersonalTheme(gSelectedTheme);break;
        case ID_EXPORT:exportReg();break;
        }
        return 0;
    }
    case WM_CATALOG_DONE:{
        std::unique_ptr<std::wstring> response((std::wstring*)l);
        gRefreshing=false;EnableWindow(gRefresh,TRUE);
        reloadCatalog();message(*response);
        return 0;
    }
    case WM_DOWNLOAD_DONE:{
        std::unique_ptr<DownloadResult> response((DownloadResult*)l);
        gDownloading=false;gActiveDownloadKey.clear();
        if(response->ok){
            auto selected=gThemeKey;
            // Re-load preserves selected theme, including its original source.
            reloadCatalog(selected);
            message(L"Palette scaricata e disponibile offline.");
        }else{
            gPreviewFailedKey=response->key;
            message(L"Impossibile ottenere la palette: "+response->error);
        }
        if(gHaveTheme&&!gSelectedTheme.available&&keyOf(gSelectedTheme)!=gPreviewFailedKey){
            SetTimer(wnd,ID_TIMER_PREVIEW,280,nullptr);
        }
        return 0;
    }
    case WM_CLOSE:DestroyWindow(wnd);return 0;
    case WM_DESTROY:
        KillTimer(wnd,ID_TIMER_PREVIEW);
        if(gMono)DeleteObject(gMono);
        if(gFont)DeleteObject(gFont);
        gWindow=nullptr;
        PostQuitMessage(0);return 0;
    }
    return DefWindowProcW(wnd,msg,w,l);
}
} // namespace

int WINAPI wWinMain(HINSTANCE app,HINSTANCE,LPWSTR,int show){
    INITCOMMONCONTROLSEX common{};common.dwSize=sizeof(common);common.dwICC=ICC_WIN95_CLASSES;
    InitCommonControlsEx(&common);
    WNDCLASSEXW cls{};cls.cbSize=sizeof(cls);cls.hInstance=app;
    cls.lpfnWndProc=mainProc;cls.lpszClassName=L"WinPuttierNative";
    cls.hCursor=LoadCursorW(nullptr,IDC_ARROW);
    cls.hIcon=LoadIconW(nullptr,IDI_APPLICATION);
    cls.hbrBackground=(HBRUSH)(COLOR_BTNFACE+1);
    if(!RegisterClassExW(&cls))return 1;
    HWND wnd=CreateWindowExW(0,cls.lpszClassName,L"WinPuttier - Theme Manager",
         WS_OVERLAPPEDWINDOW|WS_CLIPCHILDREN,CW_USEDEFAULT,CW_USEDEFAULT,
         1340,790,nullptr,nullptr,app,nullptr);
    if(!wnd)return 1;
    ShowWindow(wnd,show);UpdateWindow(wnd);
    MSG msg{};
    while(GetMessageW(&msg,nullptr,0,0)>0){
        if(!IsDialogMessageW(wnd,&msg)){TranslateMessage(&msg);DispatchMessageW(&msg);}
    }
    return (int)msg.wParam;
}
