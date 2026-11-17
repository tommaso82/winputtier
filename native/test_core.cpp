#include "core.h"
#include <iostream>
#include <string>

int main() {
    wp::Theme theme;
    theme.name=L"Unit test";
    theme.colors=wp::defaultPalette();
    const std::wstring original=wp::exportReg(theme,L"Test%20Only");
    wp::Theme decoded;
    if(!wp::parseReg(original,decoded)) {
        std::cerr<<"Valid PuTTY theme rejected\n";return 1;
    }
    for(int i=0;i<22;++i) {
        const auto& a=theme.colors[i];const auto& b=decoded.colors[i];
        if(a.r!=b.r||a.g!=b.g||a.b!=b.b) {
            std::cerr<<"RGB mismatch at "<<i<<"\n";return 2;
        }
    }
    const auto end=original.find(L"\"Colour21\"");
    if(end==std::wstring::npos)return 3;
    if(wp::parseReg(original.substr(0,end),decoded)) {
        std::cerr<<"Missing color accepted\n";return 4;
    }
    auto duplicate=original+L"\"Colour5\"=\"1,2,3\"\r\n";
    if(wp::parseReg(duplicate,decoded)) {
        std::cerr<<"Duplicate color accepted\n";return 5;
    }
    auto invalid=original;
    const std::wstring before=L"\"Colour1\"=\"255,255,255\"";
    auto pos=invalid.find(before);
    if(pos==std::wstring::npos)return 6;
    invalid.replace(pos,before.size(),L"\"Colour1\"=\"256,255,255\"");
    if(wp::parseReg(invalid,decoded)) {
        std::cerr<<"Out-of-range color accepted\n";return 7;
    }
    // The first snapshot is immutable across repeated theme applications.
    wchar_t base[MAX_PATH+2]{};
    if(!GetTempPathW(MAX_PATH,base))return 8;
    wchar_t snapshot[MAX_PATH+2]{};
    if(!GetTempFileNameW(base,L"WPT",0,snapshot))return 9;
    DeleteFileW(snapshot);
    std::wstring error;
    const bool first=wp::preserveOriginalBackupFile(snapshot,L"original-A\n",error);
    const bool second=wp::preserveOriginalBackupFile(snapshot,L"intermediate-B\n",error);
    const bool third=wp::preserveOriginalBackupFile(snapshot,L"latest-C\n",error);
    HANDLE handle=CreateFileW(snapshot,GENERIC_READ,FILE_SHARE_READ,nullptr,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,nullptr);
    std::string bytes;
    if(handle!=INVALID_HANDLE_VALUE) {
        char content[256]{};
        DWORD got=0;
        if(ReadFile(handle,content,sizeof(content),&got,nullptr))bytes.assign(content,got);
        CloseHandle(handle);
    }
    DeleteFileW(snapshot);
    if(!first||!second||!third||bytes!="original-A\n") {
        std::cerr<<"Original snapshot replaced by subsequent theme changes\n";return 10;
    }

    // Catalog metadata does not imply that a palette has already been downloaded.
    const auto online=wp::parseCatalogText(
        L"AlexAkulov\tSolarized Dark\thttps://raw.githubusercontent.com/test/a.reg\n"
        L"Dracula PuTTY\tDracula\thttps://raw.githubusercontent.com/test/b.reg\n");
    if(online.size()!=2 || online[0].available || online[1].available ||
       online[0].source!=L"AlexAkulov" || online[1].name!=L"Dracula") {
        std::cerr<<"Online catalog theme falsely marked as local\n";return 11;
    }
    std::wstring sourceError;
    const std::wstring sourceGood=
        L"[Sources]\nExample = https://github.com/example/theme-repo\n"
        L"ThemeFile = https://github.com/example/theme-repo/blob/master/dracula.reg\n"
        L"Folder = https://github.com/example/theme-repo/tree/main/putty\n";
    if(!wp::validateSourceConfig(sourceGood,sourceError)) {
        std::wcerr<<L"Valid GitHub sources rejected: "<<sourceError<<L"\n";return 12;
    }
    const std::wstring sourceBad=L"[Sources]\nEvil = http://example.com/palette.reg\n";
    if(wp::validateSourceConfig(sourceBad,sourceError)) {
        std::cerr<<"Insecure source accepted\n";return 13;
    }
    // Revision metadata is optional: old catalog files remain loadable.
    const auto revisions=wp::parseCatalogText(
        L"Repo\tDark\thttps://raw.githubusercontent.com/a/x.reg\tabc123\n"
        L"Repo\tLight\thttps://raw.githubusercontent.com/a/y.reg\n");
    if(revisions.size()!=2 || revisions[0].available || revisions[0].remoteRevision!=L"abc123" ||
       !revisions[1].remoteRevision.empty() || revisions[1].available) {
        std::cerr<<"Revision catalog compatibility failure\n";return 14;
    }
    // Cache identity cannot depend on the filesystem-safe representation of
    // a theme's name. "A:B" and "A_B" must not use the same cache file.
    wp::Theme first;first.source=L"iTerm2";first.name=L"Neon Raincheck: After Hours";
    wp::Theme second=first;second.name=L"Neon Raincheck_ After Hours";
    if(wp::cacheFilePath(first)==wp::cacheFilePath(second) ||
       wp::themeId(first)==wp::themeId(second)){
        std::cerr<<"Theme identity collision after filename sanitization\n";return 15;
    }
    std::vector<std::wstring> saved{wp::themeId(first)};
    if(!wp::isFavorite(first,saved) || wp::isFavorite(second,saved)) {
        std::cerr<<"Favorites do not preserve exact theme identity\n";return 16;
    }
    wp::Session nonexistent{L"WinPuttier-DOES-NOT-EXIST",L"test"};
    auto settings=wp::sessionAppearance(nonexistent);
    if(settings.boldMode!=1 || settings.useSystemColours || !settings.ansiColours) {
        std::cerr<<"Incorrect default PuTTY color settings\n";return 17;
    }
    std::wcout<<L"Native theme identity, favorites, source and backup tests passed\n";
    return 0;
}
