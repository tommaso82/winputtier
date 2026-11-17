#pragma once
#include <string>
namespace wp {
void initLanguage();
void setEnglish(bool enabled);
bool isEnglish();
std::wstring tr(const std::wstring& text);
std::wstring translate(const std::wstring& text,bool english);
}
