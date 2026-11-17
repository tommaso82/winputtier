#define UNICODE
#define _UNICODE
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
    std::wcout<<L"All native theme parser tests passed\n";
    return 0;
}
