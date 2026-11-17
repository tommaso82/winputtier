#pragma once
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <array>
#include <string>
#include <vector>
#include <functional>

namespace wp {
struct Color { int r=0, g=0, b=0; };
using Palette = std::array<Color, 22>;
struct Theme {
    std::wstring name, source, remoteUrl;
    Palette colors{};
    bool available = true;
    std::wstring remoteRevision;
};
struct Session { std::wstring key, label; };
struct ThemeSource { std::wstring name, url; };

const Palette& defaultPalette();
std::wstring dataDir();
std::wstring customDir();
std::wstring cacheDir();
std::wstring backupDir();
bool ensureDirs();
std::vector<Session> sessions();
std::vector<Theme> localThemes();
std::vector<Theme> remoteCatalog();
std::vector<Theme> parseCatalogText(const std::wstring& content);
bool validateSourceConfig(const std::wstring& input, std::wstring& error);
std::wstring sourceConfigText();
bool updateSourceConfig(const std::wstring& text, std::wstring& message);
std::vector<ThemeSource> configuredSources();
bool themeFromSession(const Session& session, Theme& theme, std::wstring& error);
bool refreshCatalog(std::wstring& message);
bool syncLibrary(std::wstring& message, const std::function<void(size_t,size_t)>& progress);
bool downloadTheme(Theme& item, std::wstring& error);
bool importTheme(const std::wstring& path, std::wstring& message);
bool saveCustomTheme(const Theme& theme, const std::wstring& path, std::wstring& error);
bool preserveOriginalBackupFile(const std::wstring& path, const std::wstring& snapshot, std::wstring& message);
bool applyTheme(const Session& session, const Theme& theme, std::wstring& message);
bool restoreLastBackup(const Session& session, std::wstring& message);
bool parseReg(const std::wstring& contents, Theme& theme);
bool readRegFile(const std::wstring& path, Theme& theme);
std::wstring exportReg(const Theme& theme, const std::wstring& sessionKey);
std::wstring safeFileName(std::wstring name);
}
