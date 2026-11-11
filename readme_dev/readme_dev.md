[English](readme_dev_en.md) | 简体中文

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

详见 [ATTRIBUTION.md](ATTRIBUTION.md)。
