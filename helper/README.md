# helper/

运行时辅助程序目录。构建产物不会被提交（见 `.gitignore`），需要按平台放入以下文件：

## 通用（.NET 8）

- `OszExporter.dll` —— 由 `helper-src/` 构建（`dotnet build -c Release`），或直接使用现成构建产物
- `OszExporter.runtimeconfig.json`
- `MongoDB.Bson.dll`

## Windows

- `Realm.dll`
- `realm-wrappers.dll`

## Linux

- `Realm.dll`
- `librealm-wrappers.so`

`Realm.dll` 与原生库必须来自**同一份** osu!lazer 安装：

- Windows：`%LOCALAPPDATA%\osulazer\current\`（`osu!.exe` 同目录）
- Linux：AppImage 执行 `--appimage-extract` 后的 `squashfs-root/`，或安装目录（通常 `/opt/osu-lazer`）

把文件放在本目录后重新构建，CMake 会把它复制到可执行文件旁的 `helper/`。
