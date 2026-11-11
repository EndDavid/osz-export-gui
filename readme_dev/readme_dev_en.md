English | [简体中文](readme_dev.md)

## Structure

```
main.cpp            EUI-NEO DSL UI + background tasks + cross-platform process invocation
CMakeLists.txt      links eui::neo; deploys helper/ and the icon after build
assets/             application icons (icon.png / icon.ico / size variants)
tools/              icon generation scripts (make_icon.ps1 + pack_ico.py)
helper/             runtime helper (Realm managed/native libraries; binaries not committed)
helper-src/         C# source for OszExporter
build/              osz_export_gui.exe, assets/, helper/
```

`OszExporter` (.NET 8) is responsible for:

1. Opening a copy of `client.realm` through the Realm Dynamic API;
2. Emitting a TSV listing (`--tsv`) that includes the difficulty count for each mode;
3. Repacking the `files/` content-addressed store into `.osz` files by ID list
   (`--ids-file`).

At startup the GUI copies `client.realm` to `%TEMP%/osu-export-gui`
(`/tmp/osu-export-gui` on Linux) before reading it, which avoids locking the database
and leaves the game data untouched.

## Icon

The application icon uses the osu!lazer pink game icon as its base, with an "export"
(arrow and tray) badge overlaid at the bottom right:

- `assets/icon.ico`: Windows executable resource (multi-size, 16–256)
- `assets/icon.png`: runtime window/taskbar icon (256px)
- Regenerate with: `powershell tools/make_icon.ps1`, then
  `python tools/pack_ico.py assets`

`CMakeLists.txt` overrides the global property `EUI_NEO_APP_ICON_RESOURCE` before calling
`eui_neo_configure_app()`, so the icon embedded in the exe is this project's
`assets/icon.ico` rather than EUI-NEO's default. See [ATTRIBUTION.md](ATTRIBUTION.md)
for provenance and licensing.

## Helper source

`helper-src/` contains the C# source for `OszExporter` (net8.0). To rebuild:

```bash
# Place the platform's Realm.dll next to helper-src/
dotnet build helper-src/OszExporter.csproj -c Release
cp helper-src/bin/Release/net8.0/OszExporter.dll helper/
```

## License and credits

See [ATTRIBUTION.md](ATTRIBUTION.md).
