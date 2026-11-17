# WinPuttier

Lightweight native C++17 / Win32 theme manager for PuTTY sessions. No Python, .NET or third-party runtime required.

**Development branch:** Native Windows port of [Puttier by anonimoanemico](https://github.com/anonimoanemico/puttier). The original Python implementation remains in the repository for reference. WinPuttier is currently an **experimental** build.

## Features

- Lists PuTTY saved sessions from the current user's Windows registry.
- Works with PuTTY's 22-color Colour0 to Colour21 palette format.
- Three-column UI: saved sessions, searchable theme catalog with a source filter, and an interactive live terminal preview.
- Select any of 22 color swatches, enter a HEX value or use the native RGB color picker. Reset unsaved edits at any time.
- Save edited palettes under a chosen name as personal themes. Saving a theme does not modify a PuTTY session.
- Save the currently configured colors of a PuTTY session into a personal theme.
- Import .reg palette files **as data only** (never execute/merge imported registry files).
- Export a selected theme as a PuTTY-compatible .reg targeted at the selected session.
- Apply a theme only to the selected PuTTY session, with a confirmation dialog.
- **Automatic original backup before the first change**; restore the **original** colors of the selected session, even after applying multiple different themes. The original snapshot is never overwritten by later changes. The backup distinguishes missing values from defined values.
- Fetch the catalog from configurable GitHub sources. Selecting an online theme downloads its 22-color palette asynchronously for preview and offline use.
- Use 'Gestisci fonti...' to edit the source configuration. Existing cached themes remain available offline.
- The original PuTTY default color palette is available without downloading any themes.

## Theme sources

WinPuttier uses these sources:

1. [mbadolato/iTerm2-Color-Schemes](https://github.com/mbadolato/iTerm2-Color-Schemes) — PuTTY .reg collection.
2. [jacek0x/pretty-putty](https://github.com/jacek0x/pretty-putty) — current location of the original Puttier source.
3. [teeli/dracula-putty](https://github.com/teeli/dracula-putty) — original Dracula theme.
4. [AlexAkulov/putty-color-themes](https://github.com/AlexAkulov/putty-color-themes) — 51 PuTTY themes, on-demand download.

The catalog is fetched via GitHub's public API using HTTPS on demand. This does not auto-apply themes or import .reg files into the registry. The AlexAkulov repository does not currently declare a redistribution license, so these themes are **not** vendored in this repository or EXE.

## Local storage

WinPuttier creates, on demand:

- %LOCALAPPDATA%\WinPuttier\sources.ini — editable source list, created on first save (four defaults).
- %LOCALAPPDATA%\WinPuttier\catalog.tsv — last downloaded list of themes.
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
- The source editor accepts GitHub API /contents directory endpoints or raw.githubusercontent.com .reg URLs over HTTPS.
- GitHub API availability and request limits may affect catalog updates; previously downloaded themes remain available locally.

## Attribution

Original project: [anonimoanemico/puttier](https://github.com/anonimoanemico/puttier), CC0-1.0. See [CREDITS_NATIVE.md](CREDITS_NATIVE.md) for theme sources.
