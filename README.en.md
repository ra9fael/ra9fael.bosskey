# ra9fael.bosskey · Boss Key

English | [中文](README.md)

Boss incoming? Type `boss bsod` and the screen instantly turns into a bluescreen. `boss update` is Windows Update, `boss gcc` is a gcc build in progress, `boss matrix` is Matrix-style digital rain, and `boss blackhole` is a black hole. Type the exit key sequence (default `deadbeef`) to come back — or let it dismiss itself.

The result list stays presentable; the fullscreen part does not.

## Usage

| Input | Effect |
|---|---|
| `boss ` | lists the five scene rows, each showing the words that trigger it |
| `boss bsod` / `lanping` / `lp` | fullscreen bluescreen (Win10 style, percent slowly collecting error info) |
| `boss update` / `gengxin` / `gx` | fullscreen Windows Update (spinner stalled at 87%) |
| `boss gcc` / `bianyi` / `by` | fullscreen gcc build (`-Wall -O2`, real-format warnings, wrapped long lines, never reaching 100%) |
| `boss matrix` / `daimayu` / `dmy` | fullscreen Matrix-style digital rain (katakana, digits and Latin letters with fading trails) |
| `boss blackhole` / `heidong` / `hd` | fullscreen black hole (a transparent lens: the desktop shows through, bent into arcs; it clears the screen first, the disk spreads before the hole grows, then the hole wanders) |

Press Enter to trigger the fullscreen cover; while covered, every keystroke is swallowed with nothing shown — type the exit key sequence to return. Each scene answers to three words: its English name, the full Mandarin pinyin and the pinyin initials (`boss hd` = `boss heidong` = `boss blackhole`), and every search result row lists them; any other word can be bound in settings. The top-level keyword also has the Chinese alias `摸鱼`.

`boss blackhole` does not hide the desktop: the cover window is transparent, and the desktop shows through, bent into arcs around the hole. It first clears the screen the way Win+D does and puts the windows back on exit, so the lens bends the wallpaper and the desktop icons rather than whatever you were reading.

## Settings

| Setting | Default | Meaning |
|---|---|---|
| Sub-trigger map | `bsod=bsod,lanping=bsod,lp=bsod,update=update,gengxin=update,gx=update,gcc=gcc,bianyi=gcc,by=gcc,matrix=matrix,daimayu=matrix,dmy=matrix,blackhole=blackhole,heidong=blackhole,hd=blackhole` | one scene can answer to several words (name, full pinyin, initials); scenes are bsod / update / gcc / matrix / blackhole |
| Exit key sequence | `deadbeef` | type it on the cover to exit, case-insensitive; letters and digits only |
| Auto-dismiss seconds | `0` | 0 = keep the cover until dismissed manually |
| gcc scene project name | `winshun-core` | the target name shown in the build output |
| Display coverage | `mouse` | `mouse` = cover only the screen with the mouse; `all` = cover every display |

The top-level keyword `boss` comes from plugin.json (a protocol limit — settings cannot change it); the sub-tokens, exit sequence, and timeout are all in settings.

## Permissions

It asks for `ui` only: creating fullscreen windows and taking focus. No network, no file access, no subprocesses, no clipboard.

The covers render through D3D11 — the black hole scene is a fragment shader that integrates a geodesic per pixel, and Qt's software rasterizer draws no `ShaderEffect` — so a working D3D11 is required (built into every Windows 10/11).

## Install

Requires **WinShun 0.22.0 or later** (the plugin declares the host's shared-Qt contract, `runtime.qt: "host"`, and reuses WinShun's Qt 6.12; no Qt is bundled). Either way:

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
.\bosskey.exe --cover bsod | update | gcc | matrix | blackhole [--all] [--windowed] [--autohide <sec>] --app-dir "<WinShun install dir>"
```

`tools/verify/` has `shot.py` (screenshot; `--at 0.8,3,8` captures one scene at several wall-clock offsets), `exit.py` (sends the exit sequence and verifies the exit) and `qr_check.py` (decodes the QR matrix with OpenCV) for use with `--autohide`.

## Architecture

```
bosskey.exe      Qt-less starter: takes the Qt folder from WS_QT_DIR (shared-Qt contract, 0.22.0+)
                 or WS_APP_DIR, adds it to the DLL search, loads BossKeyApp.dll
BossKeyApp.dll   worker thread runs the winshun.h SDK (JSON-RPC over stdio)
                 the GUI thread precreates one hidden fullscreen QML window per screen;
                 showing one is showFullScreen + raise
logic.h          pure logic: scene map, exit-sequence matcher, percent pacing, gcc log (testable)
qrcode.h         real QR encoder (Version 3 / level L): the scannable code on the blue screen
qmlapp/qml/      BossCover.qml (fullscreen container + swallowed keys) + five scenes
```

See [CHANGELOG.md](CHANGELOG.md) for details. License: MIT, author ra9fael.
