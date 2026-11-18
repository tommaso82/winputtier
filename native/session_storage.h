#pragma once
#include "core.h"

namespace wp::detail {
// The caller supplies the registry root and the directory for this user's backups.
bool applyTheme(HKEY registryRoot, const std::wstring& backupFolder,
                const Session& session, const Theme& theme, std::wstring& message);
bool restoreLastBackup(HKEY registryRoot, const std::wstring& backupFolder,
                       const Session& session, std::wstring& message);
}
