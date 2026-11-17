#pragma once
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <array>
#include <string>
#include <vector>

namespace wp {
struct Color { int r=0, g=0, b=0; };
using Palette = std::array<Color, 22>;
struct Theme {
    std::wstring name, source, remoteUrl;
    Palette colors{};
    bool available = true;
};
struct Session { std::wstring key, label; };

const Palette& defaultPalette();
std::wstring dataDir();
std::wstring customDir();
std::wstring cacheDir();
std::wstring backupDir();
bool ensureDirs();
std::vector<Session> sessions();
std::vector<Theme> localThemes();
std::vector<Theme> remoteCatalog();
bool refreshCatalog(std::wstring& message);
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
