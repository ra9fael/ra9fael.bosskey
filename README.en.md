# ra9fael.bosskey · Boss Key

English | [中文](README.md)

Boss incoming? Type `boss b` and the screen instantly turns into a bluescreen. `boss u` is Windows Update, `boss t` is a gcc build in progress. Type the exit key sequence (default `deadbeef`) to come back — or let it dismiss itself.

The result list stays presentable; the fullscreen part does not.

## Usage

| Input | Effect |
|---|---|
| `boss ` | lists the three scene rows |
| `boss b` | fullscreen bluescreen (Win10 style, percent slowly collecting error info) |
| `boss u` | fullscreen Windows Update (spinner stalled at 87%) |
| `boss t` | fullscreen gcc build (`-Wall -O2`, real-format warnings, streaming) |

Press Enter to trigger the fullscreen cover; while covered, every keystroke is swallowed with nothing shown — type the exit key sequence to return. The top-level keyword also has the Chinese alias `摸鱼`.

## Settings

| Setting | Default | Meaning |
|---|---|---|
| Sub-trigger map | `b=bsod,u=update,t=gcc` | custom tokens, e.g. `a=bsod,s=update,d=gcc`; scenes are bsod / update / gcc only |
| Exit key sequence | `deadbeef` | type it on the cover to exit, case-insensitive; letters and digits only |
| Auto-dismiss seconds | `0` | 0 = keep the cover until dismissed manually |
| gcc scene project name | `winshun-core` | the target name shown in the build output |
| Display coverage | `mouse` | `mouse` = cover only the screen with the mouse; `all` = cover every display |

The top-level keyword `boss` comes from plugin.json (a protocol limit — settings cannot change it); the sub-tokens, exit sequence, and timeout are all in settings.

## Install

Requires an installed WinShun (the plugin reuses its Qt runtime; no Qt is bundled). Either way:

- double-click `dist/ra9fael.bosskey-0.1.0.wsplugin` and WinShun walks you through it;
- or in WinShun: Plugins → Install from archive.

## Building from source

Windows + MSVC + Ninja + Qt 6 (the same source WinShun uses):

```powershell
# in a VS developer shell
cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_PREFIX_PATH=<Qt's msvc dir>
cmake --build build
ctest --test-dir build          # pure-logic tests
python tools/package.py         # dist/ra9fael.bosskey-0.1.0.wsplugin
```

Outputs: `bosskey.exe` (a Qt-less starter) and `BossKeyApp.dll` (the QML cover; protocol and UI are separate, the same architecture as the official capture plugin).

The scenes can be checked without WinShun:

```powershell
.\bosskey.exe --cover bsod | update | gcc [--all] [--autohide <sec>] --app-dir "<WinShun install dir>"
```

`tools/verify/` has `shot.py` (screenshot) and `exit.py` (sends the exit sequence and verifies the exit) for use with `--autohide`.

## Architecture

```
bosskey.exe      Qt-less starter: adds WinShun's folder to the DLL search, loads BossKeyApp.dll
BossKeyApp.dll   worker thread runs the winshun.h SDK (JSON-RPC over stdio)
                 the GUI thread precreates one hidden fullscreen QML window per screen;
                 showing one is showFullScreen + raise
logic.h          pure logic: scene map, exit-sequence matcher, percent pacing, gcc log (testable)
qmlapp/qml/      BossCover.qml (fullscreen container + swallowed keys) + three scenes
```

See [CHANGELOG.md](CHANGELOG.md) for details. License: MIT, author ra9fael.
