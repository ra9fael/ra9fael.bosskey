# Changelog

All notable changes to this project are documented in this file.
The format follows [Keep a Changelog](https://keepachangelog.com/en/1.1.0/).

## [Unreleased]

## [0.1.3] - 2026-11-21

### Added

- Add an optional UAC input guard without Qt. The host starts the fixed helper through its existing open effect; the cover plugin remains strict and Low Integrity. When enabled, each cover start requests consent at most once and shows the cover only after guard readiness or ordinary-mode fallback.
- Drain swallowed key repeats and releases before removing the keyboard hook. The elevated guard exits when the cover closes, its owner exits or its pipe disconnects; it sends no individual key data and accepts no arbitrary commands.
- Add shared input-policy tests and separate windowed acceptance modes for native no-focus exit, focused QML exit, fallback and guard lifecycle.
- Add a cover-scoped keyboard hook on a separate message-loop thread for common shell and window-switching shortcuts. Exit matching stores only a sequence-prefix length; the hook is removed on hide and shutdown.
- Animate three sky layers at different drift speeds with independent low-amplitude twinkling inside the existing gravitational sky sampling.
- Add native/QML exit-source diagnostics and windowed strict-runner acceptance tools, including fixed-centre particle-motion captures and steady-state frame-event/resource measurements.
- Add Matrix-style digital rain and black hole cover scenes, available as `boss matrix` and `boss blackhole`.
- The black hole cover arrives in three stages: 0.8 s of near-black sky, then the accretion disk spreads and brightens while the shadow is still a point, then the shadow grows into it over 6.5 s and the disk returns to its steady 8 r_s. Afterwards the hole follows a bounded random walk — Brownian acceleration, drag and a soft spring back to the middle third — so it never repeats a path and never leaves the screen. The clock is the cover's own `t`, rewound on every show.
- The black hole scene paints an opaque procedural night sky of dust and stars behind the lens. The shadow swallows the sky to pure black; the accretion disk and a restrained outer glow stand clear against it.

### Changed

- The UAC input guard is opt-in through the new `uacGuard` setting and off by default: a consent prompt in front of the cover gives the disguise away, so ordinary mode is what `boss` does unless the setting is enabled. Switching the setting off, declining consent, a helper startup failure or guard disconnection all settle on ordinary mode without another prompt.
- Request `processes` in addition to `ui` so WinShun can launch the fixed helper. Updating requires confirmation of the added permission; it does not elevate the plugin or WinShun.
- Include `bosskey-guard.exe` as a required runtime asset in packaging and package validation.
- Sub-triggers are the scenes' own names plus the full pinyin and the pinyin initials — `boss blackhole`, `boss heidong`, `boss hd` all reach the same scene — and every search result row names all three (`boss blackhole / heidong / hd · Enter 全屏伪装 · 键入 deadbeef 退出`), with the command hint listing all five groups. One scene may be bound to any number of words, so a custom map still works; the listing still shows five rows, one per scene.
- The Matrix rain drops Chinese: halfwidth katakana, digits and Latin letters only, one glyph width, each glyph centred in its cell. The Chinese characters were what made the scene hitch on arrival — every distinct one pulls the CJK fallback font in and is rasterized the first time it lands, spread over the first second of falling. Measured in a 480×270 preview: with Chinese the fourth paint took 841 ms (the first one 360 ms); without it the fourth takes 0 ms and only the one-off MS Gothic load remains (≈140 ms). The evidence, including the per-stage breakdown of the first paint, is in docs/validation/cover-scenes.
- The black hole scene is an opaque dark sky with procedural dust and stars. The geodesic shader bends that sky into arcs around a pure-black shadow while the accretion disk lights the scene; no desktop image is captured or displayed.
- The black hole scene's fragment shader integrates every pixel's null geodesic through the Schwarzschild metric (`a = -(3/2) h² x / r⁵`, 48 leapfrog steps), so the shadow, the photon ring and the accretion disk's upper and lower arcs are results of the integration rather than drawings. Ported from XboxNahida/ghostty-blackhole-main, after Eric Bruneton's black hole shader; the analysis and the porting notes are in docs/blackhole-shader.md. The procedural sky supplies the structure for lensing, and the shader output is opaque.
- The covers render on Qt's D3D11 backend. Qt's software rasterizer draws no `ShaderEffect` at all — it skips it silently, which would leave `boss blackhole` a black screen — so the software path and the `BOSSKEY_RENDERER` switch are gone. `runtime.memoryMB` rises from 128 to 1024 for the swapchains and texture atlases (106 MB private commit measured fullscreen at 1920×1080 on one display).
- The gcc scene's log never ends: it streams build pass after pass, and a pass hands over to the next through a CMake regeneration instead of linking, so the percent climbs to 96%, restarts, and the screen keeps moving for as long as the cover is up. Long lines wrap at the window edge the way a real terminal does.
- The plugin runs in `sandbox: strict`; the black hole scene no longer injects Win+D or captures the desktop, so it needs no window-management access.

### Fixed

- `boss blackhole` uses a full-screen opaque sky from its first frame, so desktop windows never show through, including while the shader initializes.

### Limitations

- Ordinary strict mode does not guarantee focus-independent input suppression across integrity levels. Enhanced protection covers the matched letters/digits and supported shell shortcuts, not Windows secure attention sequences or the secure desktop.
- Enhanced mode accepts normalized exit sequences up to 64 characters; longer custom sequences use ordinary mode.

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
