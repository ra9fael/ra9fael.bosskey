# Changelog

All notable changes to this project are documented in this file.
The format follows [Keep a Changelog](https://keepachangelog.com/en/1.1.0/).

## [Unreleased]

### Added

- Add Matrix-style digital rain and black hole cover scenes, available as `boss m` and `boss h`.

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
