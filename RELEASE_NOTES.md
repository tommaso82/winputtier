# WinPuttier 0.6.0

A more polished **native Windows** interface for WinPuttier, the PuTTY theme manager. For **Windows 10/11 x64**.

## Refined interface

- **Sessions:** cleaner, taller list rows with a consistent terminal icon. Session names are left exactly as stored by PuTTY; WinPuttier does not guess whether a session is a VPS, router or NAS.
- **Theme library:** each available theme is represented by a small **real colour thumbnail** computed from its PuTTY palette (background, foreground, green and blue). Theme name and source are shown on separate lines, with the actual favourite state on the right.
- **Better visual hierarchy:** lighter body typography, softer blue selection highlights, more rounded buttons and panels, and rounded palette swatches.
- **Header:** a subtle badge displaying the actual number of locally available offline themes.
- **ANSI preview:** a rounded terminal surface; PuTTY colour semantics and all existing preview features remain unchanged.
- **Native and portable:** the entire visual treatment is rendered through Win32/GDI without browser engines, external graphics, icon packs or extra runtime dependencies.

## Unchanged safeguards

WinPuttier applies colours **only to the selected saved PuTTY session** and preserves the original session-colour backup. Theme import, export, sync, favourites, restore, and the Italian/English language selector are unchanged.

## Download

Download **WinPuttier.exe** from the release assets. It is a **portable, unsigned** application; no installer, Python or .NET required.

**SHA256SUMS.txt** provides the SHA-256 checksum of the published executable.

The original Puttier project and upstream theme authors are acknowledged in [CREDITS.md](https://github.com/tommaso82/winputtier/blob/main/CREDITS.md).
