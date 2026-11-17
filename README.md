# WinPuttier

A lightweight, native Windows application for managing **PuTTY colour schemes**. Preview and edit a theme, then apply it to a specific saved PuTTY session. WinPuttier keeps a backup of that session's original colours so you can restore them later.

[![Windows build](https://github.com/tommaso82/winputtier/actions/workflows/windows-native.yml/badge.svg?branch=main)](https://github.com/tommaso82/winputtier/actions/workflows/windows-native.yml?query=branch%3Amain)
[![License: CC0-1.0](https://img.shields.io/badge/license-CC0--1.0-lightgrey)](LICENSE)

**Windows 10/11 · x64 · C++17 / Win32 · standalone executable**

![WinPuttier interface overview](docs/winputtier-interface.svg)

*Illustration of the interface with example session names; not a live screenshot.*

## Get started

1. Download **WinPuttier.exe** from the [latest GitHub Release](https://github.com/tommaso82/winputtier/releases/latest).
2. Run it on a Windows computer with at least one saved PuTTY session.
3. Click **Sincronizza libreria** to download the available themes, then select a session on the left and a theme in the centre.
4. Check the preview and click **Applica tema** to apply its 22 colours. Use **Ripristina colori originali** to restore the original session palette.

No installation, Python or .NET is needed. The executable is currently **unsigned**. For test builds, see [GitHub Actions](https://github.com/tommaso82/winputtier/actions/workflows/windows-native.yml).

## Features

- **Visual refinement (0.6.1):** white app background with a subtle pale-blue full-width header, aligned 16px card footer margins, denser PuTTY session/theme lists, clean rounded buttons and inset native text fields. Theme collections use a rounded selector with a native menu popup. System scrollbars remain native for reliable keyboard, wheel and drag behavior.
- **Refined native UI:** rounded selection rows in the PuTTY sessions and themes lists, lighter typography and buttons, a cached-theme count and a cleaner ANSI preview. Session icons are uniform; theme thumbnails show colours from each actual locally available 22-colour PuTTY palette, never a guessed category or synthetic palette.
- **English / Italiano:** detects the Windows display language on first launch, with a compact IT / EN segmented selector at the top right. Language changes take effect immediately; the choice is remembered. One executable includes both languages.
- **Session-specific changes:** writes only PuTTY's `Colour0`–`Colour21` values for the selected session, leaving connection settings and credentials untouched.
- **Integrated ANSI preview:** sample terminal output, PuTTY's special colours and 256 combinations of ANSI foreground and background colours.
- **Colour editor:** 22 swatches, HEX input and a Windows RGB picker; save your edits as a personal theme or export a PuTTY-compatible `.reg` file.
- **Offline library:** synchronize individual `.reg` palette files from configurable GitHub sources, then use cached themes without an Internet connection. Browse, search and save favorites.
- **Original-colour restoration:** on the first change to a session, WinPuttier saves the existing colours and keeps that backup when you apply other themes.
- **Safe import:** reads `.reg` colour files as data; it does not execute them or merge them into the Windows registry.

Theme and session names are never translated. Windows-provided dialogs (such as the RGB colour picker) use the Windows display language.

The preview is a **colour reference**, not a complete PuTTY terminal emulator. Terminal settings such as ANSI colours, bold text and system colours can affect the appearance of an actual session.

### Sources

The default catalog includes themes from [iTerm2-Color-Schemes](https://github.com/mbadolato/iTerm2-Color-Schemes), [pretty-putty](https://github.com/jacek0x/pretty-putty), [dracula-putty](https://github.com/teeli/dracula-putty) and [AlexAkulov/putty-color-themes](https://github.com/AlexAkulov/putty-color-themes). WinPuttier downloads themes from their respective repositories instead of bundling them in the executable. Availability and licensing remain subject to the upstream projects.

## Backups and data

Files are stored under `%LOCALAPPDATA%\WinPuttier\`: downloaded palettes in `cache\`, personal themes in `themes\`, preferences and catalog metadata alongside them, and the original session snapshots in `backups\`.

The original backup is retained after restoration. **Keep the `backups` folder** if you may need to undo colour changes. Reopen any PuTTY window that was already running to pick up changed saved-session colours.

WinPuttier currently supports conventional PuTTY sessions stored in the Windows registry. Alternative portable PuTTY configurations may not be detected. It changes colours only; it does not modify other session settings.

## Build

Requires CMake and Visual Studio 2019 or newer with the **Desktop development with C++** workload. On Windows with Visual Studio 2022:

```powershell
cmake -S . -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Release
ctest --test-dir build -C Release --output-on-failure
```

The executable is created at `build\Release\WinPuttier.exe`. The same build and tests run in [GitHub Actions](.github/workflows/windows-native.yml). See [additional technical details](README_NATIVE.md).

## Credits

**[Tommaso (@tommaso82)](https://github.com/tommaso82)** — Creator, developer and maintainer.  
**[OpenAI ChatGPT](https://chatgpt.com/)** — C++/Win32 development and documentation assistance.

WinPuttier is based on [Puttier by anonimoanemico](https://github.com/anonimoanemico/puttier), released under [CC0-1.0](LICENSE). The original Python sources are retained for reference.

The original theme-source acknowledgments and the individual theme-author credits are preserved in [CONTRIBUTORS.md](CONTRIBUTORS.md) and the complete original [CREDITS.md](CREDITS.md). Thank you to the maintainers and creators of the underlying themes.

*Independent community project; not affiliated with or endorsed by the PuTTY project.*
