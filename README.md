# osu!lazer 谱面导出器（EUI-NEO GUI）

一个本地小工具：读取 osu!lazer 的 `client.realm` 数据库，列出所有已导入谱面，勾选后重新打包为 `.osz`。

![界面](screenshot.png)

## 功能

- 列出全部已导入谱面，按标题 / 艺术家 / 导入时间排序，可切换升序、降序
- 每行显示：标题（大字）、艺术家（暗色小字）、谱师、文件数、大小、导入时间
- 谱面类型徽章：osu! / taiko / catch / mania 图标 + 对应难度数量（如 `● 3  🥁 1`）
- 搜索过滤（标题 / 艺术家 / 谱师）
- 整行点击或复选框选择；全选（当前筛选结果）/ 清空
- 导出选中谱面为标准 `.osz`（保留原始内部文件名、音频、背景、音效）
- 刷新：重新复制游戏数据库并读取（不会修改原数据库）
- 打开导出目录

## Windows 运行

需要已安装 **.NET 8 运行时**（`C:\Program Files\dotnet\dotnet.exe`）。

```powershell
.\build\osz_export_gui.exe
```

或双击 `build\run.bat`。导出目录：`%USERPROFILE%\Documents\osu-lazer-osz-export`

## Linux 构建

已在 Ubuntu 22.04（WSL2）上用 GCC 11 + Ninja 验证：配置、编译、链接及启动均通过。

```bash
sudo apt update
sudo apt install -y build-essential cmake ninja-build pkg-config \
  libgl1-mesa-dev libegl1-mesa-dev libx11-dev libxext-dev libxrandr-dev \
  libxinerama-dev libxcursor-dev libxi-dev libwayland-dev wayland-protocols \
  libxkbcommon-dev libglib2.0-dev libcurl4-openssl-dev libasound2-dev libpulse-dev

cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release \
  -DEUI_NEO_SOURCE=/path/to/EUI-NEO
cmake --build build -j"$(nproc)"
./build/osz_export_gui
```

### Linux 下的辅助程序（必须）

`helper/` 里的 `OszExporter.dll` 是跨平台的 .NET 程序集，但 Realm 的原生库与 `Realm.dll` 必须来自 **Linux 版 osu!lazer**：

```bash
# lazer AppImage 可执行 --appimage-extract 得到 squashfs-root；deb/安装版通常在 /opt/osu-lazer
LAZER_DIR=/path/to/lazer            # 含 osu!.dll / Realm.dll 的目录
cp "$LAZER_DIR/Realm.dll" helper/
cp "$LAZER_DIR/MongoDB.Bson.dll" helper/
cp "$LAZER_DIR/librealm-wrappers.so" helper/   # 文件名可用 find 确认
```

- 保持 `Realm.dll` 与 `librealm-wrappers.so` 来自**同一份** lazer 安装；
- Windows 版自带的 `realm-wrappers.dll` 在 Linux 下会被忽略，保留不影响；
- 需要 `dotnet --list-runtimes` 中有 .NET 8 运行时。

数据目录：`$XDG_DATA_HOME/osu`，默认 `~/.local/share/osu`（Flatpak 为 `~/.var/app/sh.ppy.osu/data/osu`）。导出目录：`~/osu-lazer-osz-export`。

## 构建（Windows）

依赖 CMake 3.14+、MinGW-w64 g++ 12+（或 MSVC）、以及本机 `E:\EUItest\EUI-NEO` 源码。

```powershell
$env:PATH = "D:\codename_EndDavid\MinGW64\bin;" + $env:PATH
cmake -S . -B build -G "MinGW Makefiles" -DCMAKE_BUILD_TYPE=Release `
  -DCMAKE_C_COMPILER="D:/codename_EndDavid/MinGW64/bin/gcc.exe" `
  -DCMAKE_CXX_COMPILER="D:/codename_EndDavid/MinGW64/bin/g++.exe"
cmake --build build --parallel
```

EUI-NEO 路径可用 `-DEUI_NEO_SOURCE=<path>` 覆盖；辅助程序目录可用 `-DOSZ_HELPER_DIR=<path>` 覆盖。

## 结构

```
main.cpp            EUI-NEO DSL 界面 + 后台任务 + 跨平台进程调用
CMakeLists.txt      链接 eui::neo，构建后部署 helper/ 与图标
assets/             应用图标（icon.png / icon.ico / 各尺寸 PNG）
tools/              图标生成脚本（make_icon.ps1 + pack_ico.py）
helper/             运行时辅助程序（Realm 托管/原生库，不提交二进制）
helper-src/         OszExporter 的 C# 源码
build/              osz_export_gui.exe、assets/、helper/
```

`OszExporter`（.NET 8）负责：

1. 用 Realm Dynamic API 打开 `client.realm` 副本；
2. 输出 TSV 清单（`--tsv`），含每个模式的难度数量；
3. 按 ID 列表把 `files/` 内容寻址存储重新打包成 `.osz`（`--ids-file`）。

GUI 启动时把 `client.realm` 复制到 `%TEMP%/osu-export-gui`（Linux 为 `/tmp/osu-export-gui`）再读取，避免锁库，也不会改动游戏数据。

## 图标

应用图标以 osu!lazer 的粉色游戏图标为底，右下角叠加“导出”（箭头 + 托盘）徽章：

- `assets/icon.ico`：Windows 可执行文件资源（16–256 多尺寸）
- `assets/icon.png`：运行时窗口/任务栏图标（256px）
- 重新生成：`powershell tools/make_icon.ps1`，然后 `python tools/pack_ico.py assets`

`CMakeLists.txt` 在调用 `eui_neo_configure_app()` 前覆盖全局属性
`EUI_NEO_APP_ICON_RESOURCE`，因此 exe 内嵌图标是本项目的 `assets/icon.ico`，
而不是 EUI-NEO 的默认图标。来源与授权说明见 [ATTRIBUTION.md](ATTRIBUTION.md)。

## 辅助程序源码

`helper-src/` 是 `OszExporter` 的 C# 源码（net8.0）。重新构建：

```bash
# 把对应平台的 Realm.dll 放到 helper-src/ 旁边
dotnet build helper-src/OszExporter.csproj -c Release
cp helper-src/bin/Release/net8.0/OszExporter.dll helper/
```

## 许可与致谢

osu!lazer 图标归 ppy Pty Ltd 所有，本项目与其无隶属关系；EUI-NEO 为 Apache-2.0。
详见 [ATTRIBUTION.md](ATTRIBUTION.md)。



