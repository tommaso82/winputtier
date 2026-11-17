# WinPuttier

Lightweight native C++17 / Win32 theme manager for PuTTY sessions. No Python, .NET or third-party runtime required.

**Development branch:** Native Windows port of [Puttier by anonimoanemico](https://github.com/anonimoanemico/puttier). The original Python implementation remains in the repository for reference. WinPuttier 0.4 is an **experimental** build. GUI and High DPI behavior should be verified on real Windows installations.

## Features

- Lists PuTTY saved sessions from the current user's Windows registry.
- Works with PuTTY's 22-color Colour0 to Colour21 palette format.
- Three-column native UI with lightweight card styling, DPI-aware layout, saved PuTTY sessions, local theme catalog, and an **integrated, scrollable ANSI preview**.
- The single built-in preview covers realistic sample terminal output, the six default/bold/cursor roles, the 16 ANSI swatches, and **256 foreground/background combinations**. There is **no second popup preview**. Clicking a color highlights its use in the ANSI matrices.
- Select any of 22 color swatches, enter a HEX value or use the native RGB color picker. Reset unsaved edits at any time.
- Save edited palettes under a chosen name as personal themes. Saving a theme does not modify a PuTTY session.
- "Crea tema dalla sessione" saves the currently configured colors of a PuTTY session into a personal theme.
- Import .reg palette files **as data only** (never execute/merge imported registry files).
- Export a selected theme as a PuTTY-compatible .reg targeted at the selected session.
- Apply a theme only to the selected PuTTY session, with a confirmation dialog.
- **Automatic original backup before the first change**; restore the **original** colors of the selected session, even after applying multiple different themes. The original snapshot is never overwritten by later changes. The backup distinguishes missing values from defined values.
- "Sincronizza libreria" downloads **only the .reg palette files**, in parallel (up to ten simultaneous HTTPS requests). It does not download ZIP archives or the complete upstream repositories.
- The application stores the themes in its local cache and uses source revision IDs when provided. Only **downloaded and valid themes** appear in the list, with no redundant [Locale]/[Online] prefixes. The original name and source survive filename sanitization; existing WinPuttier 0.3 caches migrate automatically. Selecting a theme never starts a download.
- A progress counter shows how many .reg files have been processed. Failed downloads are reported and can be retried by synchronizing again.
- Add/remove favorites using a star toggle, and choose "★ Preferiti" in the source filter to see only favorites. The favorites list is persistent and survives library sync.
- Use 'Gestisci fonti...' to edit the source configuration. Existing cached themes remain available offline.
- The chosen PuTTY session's "BoldAsColour", "UseSystemColours" and "ANSIColour" values are read for preview guidance. These settings are not modified. The preview warns when system colors or ANSI-disabling options can cause differences from a live PuTTY terminal.
- The original PuTTY default color palette is available without downloading any themes.

## Theme sources

WinPuttier uses these sources:

1. [mbadolato/iTerm2-Color-Schemes](https://github.com/mbadolato/iTerm2-Color-Schemes) — PuTTY .reg collection.
2. [jacek0x/pretty-putty](https://github.com/jacek0x/pretty-putty) — current location of the original Puttier source.
3. [teeli/dracula-putty](https://github.com/teeli/dracula-putty) — original Dracula theme.
4. [AlexAkulov/putty-color-themes](https://github.com/AlexAkulov/putty-color-themes) — 51 PuTTY themes, downloaded during full library synchronization.

The catalog index is refreshed from GitHub when you synchronize the library. Only .reg files are downloaded; no archive files are used. The source URL list is editable in the application without recompiling. This does not auto-apply themes or import .reg files into the registry. The AlexAkulov repository does not currently declare a redistribution license, so these themes are **not** vendored in this repository or EXE.

## Local storage

WinPuttier creates, on demand:

- %LOCALAPPDATA%\WinPuttier\sources.ini — editable source list, created on first save (four defaults).
- %LOCALAPPDATA%\WinPuttier\catalog.tsv — last synchronized theme list.
- %LOCALAPPDATA%\WinPuttier\sync-index.tsv — last successfully synchronized file revisions.
- %LOCALAPPDATA%\WinPuttier\favorites.tsv — the persistent favorites list.
- %LOCALAPPDATA%\WinPuttier\themes\ — personal/imported .reg themes.
- %LOCALAPPDATA%\WinPuttier\cache\ — downloaded .reg themes.
- %LOCALAPPDATA%\WinPuttier\backups\ — per-session backup snapshots (.wpb).

**No login credentials, host settings, port, font or default PuTTY settings are modified by applying a theme.** Only the 22 Colour* registry values of the chosen session are changed.

A single original backup is created before the first color change and is never overwritten. Restoring colors does not remove it. If previous builds left timestamped backups, the oldest one is migrated as the original. On a write failure the program attempts to roll back. Closing/reopening PuTTY may be necessary to see updated colors in already-open sessions.

## Build

With Visual Studio 2019 and the Desktop development with C++ workload installed:

1. Open a Developer PowerShell / Command Prompt.
2. Configure: cmake -S . -B build -G "Visual Studio 16 2019" -A x64
3. Build: cmake --build build --config Release
4. Run tests: ctest --test-dir build -C Release --output-on-failure
5. Use build\Release\WinPuttier.exe.

CMake generates a Visual Studio solution under the build directory if you prefer to open it in Visual Studio 2019.

GitHub Actions builds and tests with MSVC on windows-2022 and uploads the WinPuttier.exe artifact. This development binary has not been code-signed.

## Safety and boundaries

- Windows only; standard PuTTY registry-backed sessions only (portable configurations stored in files are not supported yet).
- The restore button always restores the **first/original** color snapshot for the selected session. No new history is generated. The original backup remains available after restore.
- Importing or downloading a theme does **not** immediately change a PuTTY session.
- Imported themes must contain all 22 valid RGB values (0–255).
- Personal and downloaded themes are not embedded in the EXE; the built-in default PuTTY palette is available offline.
- The source editor accepts ordinary HTTPS GitHub repository, folder (tree), or single .reg file (blob) links, as well as GitHub API /contents and raw.githubusercontent.com links.
- GitHub API availability and request limits can affect synchronization; previously downloaded themes remain available locally. Some sources may have no revision identifiers and will be fetched again during synchronization.

## Attribution

Original project: [anonimoanemico/puttier](https://github.com/anonimoanemico/puttier), CC0-1.0. See [CREDITS_NATIVE.md](CREDITS_NATIVE.md) for theme sources.
