# WinPuttier technical documentation

The primary application is a C++17 / Win32 executable for Windows 10/11 x64. The [original Python implementation](puttier/) is kept for reference. See the [README](README.md) for downloads and usage.

## PuTTY sessions and backups

WinPuttier reads saved sessions from the current user's registry under `HKCU\Software\SimonTatham\PuTTY\Sessions`. Portable configurations stored in files are outside this scope. Applying a theme writes only the selected session's `Colour0`–`Colour21` values. The original snapshot records which colour values were absent. On a write failure, the program attempts to roll back.

WinPuttier preserves the first snapshot across applications and restorations. If an older build left timestamped snapshots and the current snapshot is absent, both applying and restoring use the oldest historical snapshot as the original.

WinPuttier reads PuTTY's `BoldAsColour`, `UseSystemColours` and `ANSIColour` settings for preview guidance. The preview shows sample output, the default/bold/cursor roles, sixteen ANSI colours and a matrix of 256 foreground/background combinations.

## Palette files and synchronization

Imported `.reg` files must contain all 22 valid RGB values. WinPuttier parses them as data. Export creates a `.reg` file targeted at the selected session.

### Default theme sources

- [iTerm2-Color-Schemes](https://github.com/mbadolato/iTerm2-Color-Schemes) PuTTY collection
- [pretty-putty](https://github.com/jacek0x/pretty-putty)
- [dracula-putty](https://github.com/teeli/dracula-putty)
- [AlexAkulov/putty-color-themes](https://github.com/AlexAkulov/putty-color-themes)

The source editor accepts HTTPS GitHub repository, folder and single-file links, as well as GitHub API Contents URLs and raw GitHub `.reg` URLs. Catalog sync uses up to ten parallel HTTPS downloads of individual palettes. It reuses valid cached files when their upstream revisions match. If a catalog request fails for a source, its previous entries keep their names and favourites. GitHub availability and API limits may affect synchronization.

Palettes are downloaded from their upstream repositories. See the [theme-source credits](CREDITS_NATIVE.md) for attribution and licensing information.

## Local storage

Data files are created as needed under `%LOCALAPPDATA%\WinPuttier\`.

| Path | Contents |
| --- | --- |
| `sources.ini` | Saved theme-source configuration |
| `catalog.tsv` | Metadata for the latest known catalog |
| `sync-index.tsv` | Revisions of successfully downloaded palettes |
| `favorites.tsv` | Favourite theme identities |
| `themes\` | Personal and imported palettes |
| `cache\` | Downloaded palettes |
| `backups\` | Original session colour snapshots (`.wpb`) |

The language preference is stored under `HKCU\Software\WinPuttier`.

## Build and release

Use the [README build commands](README.md#build). For Visual Studio 2019, substitute `-G "Visual Studio 16 2019"`. The tests cover palette parsing, catalog identity, search, translations and backup/restore of an isolated registry session.

Update the project version in `CMakeLists.txt` and the content of `RELEASE_NOTES.md` before tagging a release. Push a tag matching the executable's version, in the form `vMAJOR.MINOR.PATCH`. GitHub Actions verifies the tag, builds and tests on Windows, and publishes the unsigned executable and SHA-256 checksum from that run.
