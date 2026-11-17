# WinPuttier 0.6.4

Small search-behaviour fix for the native Windows theme manager.

## Fixed

- **Theme search now searches the theme name only.** The collection/source name can no longer cause unrelated themes to appear in the results.
- Collection filtering remains independent, and searches remain case-insensitive.
- Added regression tests for matches in the theme name, matches only in the source, and empty/nonmatching searches.
- **No visual UI changes.** The offline-theme count continues to show the size of the local library rather than the number of matching results.

## Download

Download the portable, unsigned **WinPuttier.exe** from the release assets (Windows 10/11 x64). The **SHA256SUMS.txt** asset provides the SHA-256 checksum. No installer or extra runtime is required.
