# Third-Party Assets and Credits

## License Scope

The **source code** of this repository (C++/CMake/C#, build scripts, and documentation)
is licensed under the [MIT License](LICENSE).

The following are **not** covered by the MIT license and remain the property of their
respective owners:

- `assets/source/lazer-256.png`, `assets/icon*.png`, `assets/icon.ico` — derived from the
  osu!lazer icon;
- the osu! icon and marks appearing in `screenshot.png`.

The osu! name and logo are trademarks/artwork of ppy Pty Ltd. The osu!lazer program code
is MIT-licensed, but its artwork
([ppy/osu-resources](https://github.com/ppy/osu-resources)) is provided under
CC BY-NC 4.0.

## osu!lazer Icon

`assets/source/lazer-256.png` and the derived `assets/icon*.png` and `assets/icon.ico`
are based on the osu!lazer game icon (ppy Pty Ltd,
[ppy/osu](https://github.com/ppy/osu)) and are used solely as this tool's application
icon to indicate its association with osu!lazer.

## EUI-NEO

UI framework: [sudoevolve/EUI-NEO](https://github.com/sudoevolve/EUI-NEO),
Apache License 2.0. The application builds against a local checkout via
`add_subdirectory`.

## Font Awesome

The mode badges use the Font Awesome 7 Free Solid font bundled with EUI-NEO, under the
SIL Open Font License 1.1.

## Realm

Reads osu!lazer's `client.realm` through
[Realm .NET](https://github.com/realm/realm-dotnet); `Realm.dll` and the native
libraries come from the user's local osu!lazer installation and are not distributed
with this repository.
