# WinPuttier 0.6.1

WinPuttier visual-polish release for **Windows 10/11 x64**.

## Visual refinements

- White main window and a subtle **full-width pale-blue header** extending behind the IT/EN language selector.
- **Aligned bottom margins:** the final action buttons in all three panels now finish with the same 16px inset.
- **Clean rounded buttons:** the entire owner-drawn button surface is painted, eliminating rectangular grey backgrounds around pale buttons.
- **More compact lists:** session rows reduced from 47 to 36 logical pixels, theme rows from 55 to 45; icon and two-line palette labels adjusted accordingly.
- **Rounded text inputs:** native Windows edit controls are inset into custom rounded white frames. Standard keyboard shortcuts, text editing and clipboard behavior are preserved.
- **Cleaner collection selector:** rounded owner-drawn control with a native Windows popup for accessible mouse/keyboard navigation.
- The session names, theme palettes, backup logic, ANSI preview, and bilingual IT/EN switching are unchanged.

**Note:** list and ANSI preview scrollbars remain native for this release. The popup opened by the collection selector is also a native Windows menu. These are intentionally preserved pending a separate interaction-tested scrollbar redesign.

## Download

Download **WinPuttier.exe** from the assets below. Portable Windows x64 executable, unsigned; no installer, Python or .NET needed.

Verify with the attached **SHA256SUMS.txt**.

The original Puttier project and all upstream theme authors remain credited in [CREDITS.md](https://github.com/tommaso82/winputtier/blob/main/CREDITS.md).
