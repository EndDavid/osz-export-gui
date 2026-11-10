# 第三方资源与致谢

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
