# 第三方资源与致谢

## 许可范围

本仓库的**源码**（C++/CMake/C#、构建脚本与文档）采用 [MIT License](LICENSE)。

以下内容**不在** MIT 授权范围内，仍归其原作者所有：

- `assets/source/lazer-256.png`、`assets/icon*.png`、`assets/icon.ico`：由 osu!lazer 图标派生；
- `screenshot.png` 中出现的 osu! 图标与标识。

osu! 名称与图标是 ppy Pty Ltd 的商标/美术资源。osu!lazer 的程序代码采用 MIT，
但其美术资源（[ppy/osu-resources](https://github.com/ppy/osu-resources)）通常按
CC BY-NC 4.0 提供，且商标不随代码授权。本工具仅在图标中表明用途关联，与 ppy 无隶属关系。

## osu!lazer 图标

`assets/source/lazer-256.png` 以及由它派生的 `assets/icon*.png`、`assets/icon.ico`
基于 osu!lazer 的游戏图标（ppy Pty Ltd，[ppy/osu](https://github.com/ppy/osu)）制作，
仅用于本工具的应用图标，以表明其与 osu!lazer 的用途关联。

本工具与 ppy Pty Ltd 无隶属关系，osu! 名称与图标归其各自权利人所有。

## EUI-NEO

界面框架：[sudoevolve/EUI-NEO](https://github.com/sudoevolve/EUI-NEO)，Apache License 2.0。
应用通过 `add_subdirectory` 引用本地源码构建。

## Font Awesome

模式徽章图标使用 EUI-NEO 随附的 Font Awesome 7 Free Solid 字体，
遵循 SIL Open Font License 1.1。

## Realm

通过 [Realm .NET](https://github.com/realm/realm-dotnet) 读取 osu!lazer 的 `client.realm`；
Realm.dll 与原生库来自用户本机的 osu!lazer 安装，不随仓库分发。
