#include "session_storage.h"

#include <filesystem>
#include <iostream>

namespace {
class Fixture {
public:
    HKEY root = nullptr;
    HKEY session = nullptr;
    std::filesystem::path backups;

    bool create() {
        wchar_t temp[MAX_PATH]{};
        wchar_t unique[MAX_PATH]{};
        const DWORD length = GetTempPathW(MAX_PATH, temp);
        if (length == 0 || length >= MAX_PATH || !GetTempFileNameW(temp, L"WPT", 0, unique))
            return false;
        if (!DeleteFileW(unique) || !CreateDirectoryW(unique, nullptr)) return false;
        directory = unique;
        backups = directory / L"backups";
        if (!CreateDirectoryW(backups.c_str(), nullptr)) return false;

        const auto hive = directory / L"session.hive";
        LONG status = RegLoadAppKeyW(hive.c_str(), &root, KEY_ALL_ACCESS, REG_PROCESS_APPKEY, 0);
        if (status != ERROR_SUCCESS) {
            std::cerr << "Loading the test hive returned Windows error " << status << '\n';
            return false;
        }

        status = RegCreateKeyExW(root, L"Software\\SimonTatham\\PuTTY\\Sessions\\Review%20session",
                                 0, nullptr, REG_OPTION_NON_VOLATILE, KEY_ALL_ACCESS, nullptr,
                                 &session, nullptr);
        if (status != ERROR_SUCCESS)
            std::cerr << "Creating the isolated session returned Windows error " << status << '\n';
        return status == ERROR_SUCCESS;
    }

    ~Fixture() {
        if (session) RegCloseKey(session);
        if (root) RegCloseKey(root);
        removeFilesAndDirectory(backups);
        removeFilesAndDirectory(directory);
    }

private:
    std::filesystem::path directory;

    static void removeFilesAndDirectory(const std::filesystem::path& path) {
        if (path.empty()) return;
        std::error_code error;
        for (auto iter = std::filesystem::directory_iterator(path, error);
             !error && iter != std::filesystem::directory_iterator{}; iter.increment(error))
            DeleteFileW(iter->path().c_str());
        RemoveDirectoryW(path.c_str());
    }
};

bool set(HKEY key, const wchar_t* name, const wchar_t* value) {
    const auto bytes = static_cast<DWORD>((wcslen(value) + 1) * sizeof(wchar_t));
    return RegSetValueExW(key, name, 0, REG_SZ, reinterpret_cast<const BYTE*>(value), bytes) ==
           ERROR_SUCCESS;
}

bool matches(HKEY key, const wchar_t* name, const std::wstring& expected) {
    wchar_t value[64]{};
    DWORD bytes = sizeof(value);
    return RegGetValueW(key, nullptr, name, RRF_RT_REG_SZ, nullptr, value, &bytes) ==
               ERROR_SUCCESS && expected == value;
}

bool missing(HKEY key, const wchar_t* name) {
    DWORD bytes = 0;
    return RegQueryValueExW(key, name, nullptr, nullptr, nullptr, &bytes) == ERROR_FILE_NOT_FOUND;
}
}

int main() {
    Fixture fixture;
    if (!fixture.create() || !set(fixture.session, L"Colour0", L"1,2,3") ||
        !set(fixture.session, L"HostName", L"test.invalid")) {
        std::cerr << "Could not create the isolated session\n";
        return 1;
    }

    const wp::Session session{L"Review%20session", L"Review session"};
    wp::Theme theme;
    theme.colors = wp::defaultPalette();
    theme.colors[0] = {4, 5, 6};
    theme.colors[21] = {7, 8, 9};
    std::wstring message;
    if (!wp::detail::applyTheme(fixture.root, fixture.backups.wstring(), session, theme, message) ||
        !matches(fixture.session, L"Colour0", L"4,5,6") ||
        !matches(fixture.session, L"Colour21", L"7,8,9")) {
        std::wcerr << L"First application failed: " << message << L'\n';
        return 2;
    }

    theme.colors[0] = {10, 11, 12};
    if (!wp::detail::applyTheme(fixture.root, fixture.backups.wstring(), session, theme, message) ||
        !matches(fixture.session, L"Colour0", L"10,11,12") ||
        !wp::detail::restoreLastBackup(fixture.root, fixture.backups.wstring(), session, message) ||
        !matches(fixture.session, L"Colour0", L"1,2,3") ||
        !missing(fixture.session, L"Colour21") ||
        !matches(fixture.session, L"HostName", L"test.invalid")) {
        std::wcerr << L"Round trip did not restore the original values: " << message << L'\n';
        return 3;
    }

    // Re-create the on-disk layout of an installation with timestamped backups.
    const auto first = std::filesystem::directory_iterator(fixture.backups);
    if (first == std::filesystem::directory_iterator{}) return 4;
    const auto original = first->path();
    const auto stem = original.stem().wstring();
    const std::wstring prefix = L"original-";
    if (stem.compare(0, prefix.size(), prefix) != 0) return 5;
    const auto legacyBase = L"backup-" + stem.substr(prefix.size());
    const auto legacy = fixture.backups / (legacyBase + L"-20260101.wpb");
    const auto newer = fixture.backups / (legacyBase + L"-20260102.wpb");
    if (!MoveFileW(original.c_str(), legacy.c_str()) ||
        !wp::preserveOriginalBackupFile(newer.wstring(), L"invalid newer snapshot\n", message) ||
        !set(fixture.session, L"Colour0", L"20,21,22") ||
        !set(fixture.session, L"Colour21", L"23,24,25")) {
        std::cerr << "Could not stage the historical backup fixture\n";
        return 6;
    }

    if (!wp::detail::restoreLastBackup(fixture.root, fixture.backups.wstring(), session, message) ||
        !matches(fixture.session, L"Colour0", L"1,2,3") ||
        !missing(fixture.session, L"Colour21") ||
        !matches(fixture.session, L"HostName", L"test.invalid") ||
        !std::filesystem::is_regular_file(original) ||
        !std::filesystem::is_regular_file(legacy)) {
        std::wcerr << L"Direct restore of the oldest historical backup failed: " << message << L'\n';
        return 7;
    }
}
