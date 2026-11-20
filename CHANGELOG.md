# Changelog

All notable changes to this project are documented in this file.
The format follows [Keep a Changelog](https://keepachangelog.com/en/1.1.0/).

## [Unreleased]

### Added

- Add Matrix-style digital rain and black hole cover scenes, available as `boss m` and `boss h`.
- The Matrix rain mixes halfwidth katakana with digits, Latin letters and Chinese, so a column reads as code in either script; wide glyphs are centred in their cell to keep the column pitch even.

### Changed

- The black hole scene is a fragment shader: every pixel's null geodesic is integrated through the Schwarzschild metric (`a = -(3/2) h² x / r⁵`, 48 leapfrog steps), so the shadow, the photon ring, the accretion disk's upper and lower arcs and the starfield lensed into arcs around the hole are results of the integration rather than drawings. Ported from XboxNahida/ghostty-blackhole-main, after Eric Bruneton's black hole shader; the analysis and the porting notes are in docs/blackhole-shader.md.
- The covers render on Qt's D3D11 backend. Qt's software rasterizer draws no `ShaderEffect` at all — it skips it silently, which would leave `boss h` a black screen — so the software path and the `BOSSKEY_RENDERER` switch are gone. `runtime.memoryMB` rises from 128 to 1024 for the swapchains and texture atlases (91.8 MB private commit measured fullscreen at 1920×1080 on one display).
- The gcc scene's log never ends: it streams build pass after pass, and a pass hands over to the next through a CMake regeneration instead of linking, so the percent climbs to 96%, restarts, and the screen keeps moving for as long as the cover is up. Long lines wrap at the window edge the way a real terminal does.

## [0.1.1] - 2026-11-20

### Fixed

- Position the cover using each display's geometry so covers appear on every monitor.
- Add windowed, per-screen preview options for safe multi-monitor verification.

## [0.1.0] - 2026-11-06

### Added

- First release of the boss key cover. `boss b` / `boss u` / `boss t` (Enter) turn the screen into a fullscreen fake bluescreen, Windows Update, or a streaming gcc build; `boss ` with no argument lists the three scenes. The top-level keyword also answers to the Chinese alias `摸鱼`.
- The exit key sequence (default `deadbeef`, case-insensitive, letters and digits only) is typed directly on the cover; every other keystroke is swallowed and never shown.
- Settings: sub-trigger map (e.g. `b=bsod,u=update,t=gcc`), exit key sequence, auto-dismiss seconds (0 = manual), the gcc scene's project name, and display coverage (the mouse's screen or all displays).
- The cover windows are precreated hidden at startup, so showing one is showFullScreen + raise — up before the boss crosses the room.
- The covers render through Qt's software rasterizer (set `BOSSKEY_RENDERER=d3d11` for the GPU): they are solid colors and text, and the raster path keeps the process at ~33 MB commit with a fullscreen window per screen (measured with two 2560×1440 displays), against ~210 MB for D3D11 swapchains plus texture atlases. Zero GPU usage also makes the "the machine is busy" story more convincing.
- The bluescreen and the update screen climb with a piecewise pacing (fast, a stall at the classic 87%, a slow crawl); the gcc scene streams a deterministic, realistic `gcc -Wall -O2` log with real-format warnings.
- The blue screen's QR code is a real one: `qrcode.h` encodes `https://www.windows.com/stopcode` — the URL printed right below it — as a Version 3, level L QR code, so scanning the cover opens that page. `tools/verify/qr_check.py` decodes the matrix with OpenCV, and the rendered cover's screenshot decodes too.
- Pure-logic tests for the scene map, the sequence matcher, the percent pacing, the gcc log, and the QR module layout (`ctest`); scene previews with screenshots and an exit-sequence check under `tools/verify/`.
- The plugin declares WinShun's shared-Qt contract (`runtime.qt: "host"`, `qtVersion: 6.12.0`) and `minWinShun: 0.22.0`. The starter takes the Qt folder from `WS_QT_DIR` (the host's injection) and keeps `WS_APP_DIR` as the fallback for older hosts. The SDK header was refreshed to the 0.22.0 version, which also answers a throwing handler with an error instead of ending the process.
- Packaging: `tools/package.py` builds a deterministic `ra9fael.bosskey-0.1.0.wsplugin` (the Qt-less starter exe plus the QML cover DLL; no Qt bundled — the installed WinShun provides it).
