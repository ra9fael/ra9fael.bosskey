# ra9fael.bosskey · Boss Key

English | [中文](README.md)

Boss incoming? Type `boss bsod` to open a bluescreen after the consent step. `boss wu` is Windows Update, `boss gcc` is a gcc build in progress, `boss mr` is Matrix-style digital rain, and `boss bh` is a black hole. Type the exit key sequence (default `deadbeef`) to come back — or let it dismiss itself.

The result list stays presentable; the fullscreen part does not.

## Usage

| Input | Effect |
|---|---|
| `boss ` | lists the five scene rows, each showing the words that trigger it |
| `boss bsod` / `lp` | fullscreen bluescreen (Win10 style, percent slowly collecting error info) |
| `boss wu` / `gx` | fullscreen Windows Update (spinner stalled at 87%) |
| `boss gcc` / `by` | fullscreen gcc build (`-Wall -O2`, real-format warnings, wrapped long lines, never reaching 100%) |
| `boss mr` / `dmy` | fullscreen Matrix-style digital rain (katakana, digits and Latin letters with fading trails) |
| `boss bh` / `hd` | fullscreen black hole (an opaque star field bent into arcs; the disk spreads before the hole grows, then the hole wanders) |
| `boss aq` / `szg` | fullscreen ASCII aquarium |

Press Enter to start the cover; with "Request UAC input protection" enabled it asks for consent once and accepting selects enhanced protection, while leaving it off or declining selects ordinary mode. Type the exit key sequence to return. Defaults use an English abbreviation and pinyin initials (`boss bh` = `boss hd`); any other word can be bound in settings. The top-level keyword also has the Chinese alias `摸鱼`.

`boss blackhole` paints an opaque procedural star field. It neither captures the desktop nor minimizes other windows. The lens bends the stars around a pure-black shadow.

## Settings

| Setting | Default | Meaning |
|---|---|---|
| Sub-trigger map | `bsod=bsod,lp=bsod,wu=update,gx=update,gcc=gcc,by=gcc,mr=matrix,dmy=matrix,bh=blackhole,hd=blackhole,aq=aquarium,szg=aquarium` | defaults use English abbreviations and pinyin initials; custom settings can add other words |
| Exit key sequence | `deadbeef` | type it on the cover to exit, case-insensitive; letters and digits only |
| Auto-dismiss seconds | `0` | 0 = keep the cover until dismissed manually |
| gcc scene project name | `winshun-core` | the target name shown in the build output |
| Display coverage | `mouse` | `mouse` = cover only the screen with the mouse; `all` = cover every display |
| Request UAC input protection | `false` | on = request UAC once per cover start, then match the exit sequence even without focus; off = no UAC prompt, keys are suppressed only while the cover has focus |

The top-level keyword `boss` comes from plugin.json (a protocol limit — settings cannot change it); the sub-tokens, exit sequence, and timeout are all in settings.

## Permissions

It requests `ui` and `processes`: it creates the cover windows and asks the host to start the fixed, bundled `bosskey-guard.exe`. WinShun requires confirmation of the added permission. The plugin itself remains in `sandbox: strict`; only the separate input guard requests elevation.

With "Request UAC input protection" enabled, each cover start asks for consent once and waits for the guard before showing the cover; accepting lets the elevated guard suppress letters, digits and the supported shell shortcuts and match the exit sequence without focus. It is off by default; declining or a guard startup failure also leaves ordinary mode without another prompt. Ordinary mode suppresses keys only while the cover has focus and does not guarantee suppression across integrity levels when focus is lost.

The guard loads no QML, accepts no arbitrary program or command, and neither stores nor forwards individual keystrokes. Its pipe carries only readiness, exit, stop and error states. Closing the cover drains swallowed-key releases; plugin exit, a crash or pipe disconnection ends the guard. This cover is not a Windows lock screen and cannot block secure attention sequences such as Ctrl+Alt+Del.

The covers render through D3D11 — the black hole scene is a fragment shader that integrates a geodesic per pixel, and Qt's software rasterizer draws no `ShaderEffect` — so a working D3D11 is required (built into every Windows 10/11).

## Install

Requires **WinShun 0.22.0 or later** (the plugin declares the host's shared-Qt contract, `runtime.qt: "host"`, and reuses WinShun's Qt 6.12; no Qt is bundled). Either way:

- double-click `dist/ra9fael.bosskey-0.1.4.wsplugin` and WinShun walks you through it;
- or in WinShun: Plugins → Install from archive.

## Building from source

Windows + MSVC + Ninja + Qt 6 (the same source WinShun uses):

```powershell
# in a VS developer shell
cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_PREFIX_PATH=<Qt's msvc dir>
cmake --build build
ctest --test-dir build          # pure-logic tests
python tools/package.py         # dist/ra9fael.bosskey-0.1.4.wsplugin
```

Outputs: `bosskey.exe` (a Qt-less starter), `BossKeyApp.dll` (the QML cover), and `bosskey-guard.exe` (the optional elevated input guard, without Qt and with a static CRT).

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
