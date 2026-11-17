# WinPuttier 0.6.2

A further native Windows interface refinement for **Windows 10/11 x64**.

## UI refinements

- **Floating header stripe:** the pale-blue band now leaves white breathing room above and below it; all header content remains within the band.
- **More compact lists:** saved session rows are 31 logical pixels high and theme rows 39 pixels, with appropriately scaled icons and colour thumbnails.
- **Slim scrollbars:** session and theme lists, plus the ANSI preview, use matching narrow rounded scrollbar tracks/thumbs. Drag the thumb or click the track; wheel and normal keyboard navigation remain available. The underlying listboxes and preview canvas are still native Win32, not HTML.
- **Custom collection dropdown:** replaced the grey legacy Windows popup with a rounded white floating menu, soft-blue highlight, current-collection marker, scrolling when necessary, keyboard arrows, Enter/Escape, and click-away dismissal.
- Kept the 22-colour palette editor, original PuTTY backup and restore, IT/EN switching, and the standalone executable packaging.

## Download

Download **WinPuttier.exe** from this release's assets. Portable, **unsigned**, no installer, browser runtime, Python or .NET required.

Validate the executable with **SHA256SUMS.txt**.

The previous releases remain available for comparison. Credits and original theme authors are retained in [CREDITS.md](https://github.com/tommaso82/winputtier/blob/main/CREDITS.md).
