# Changelog

All notable changes to this project are documented in this file.
The format follows [Keep a Changelog](https://keepachangelog.com/en/1.1.0/).

## [0.1.0] - 2026-11-06

### Added

- First release of the boss key cover. `boss b` / `boss u` / `boss t` (Enter) turn the screen into a fullscreen fake bluescreen, Windows Update, or a streaming gcc build; `boss ` with no argument lists the three scenes. The top-level keyword also answers to the Chinese alias `摸鱼`.
- The exit key sequence (default `deadbeef`, case-insensitive, letters and digits only) is typed directly on the cover; every other keystroke is swallowed and never shown.
- Settings: sub-trigger map (e.g. `b=bsod,u=update,t=gcc`), exit key sequence, auto-dismiss seconds (0 = manual), the gcc scene's project name, and display coverage (the mouse's screen or all displays).
- The cover windows are precreated hidden at startup, so showing one is showFullScreen + raise — up before the boss crosses the room.
- The bluescreen and the update screen climb with a piecewise pacing (fast, a stall at the classic 87%, a slow crawl); the gcc scene streams a deterministic, realistic `gcc -Wall -O2` log with real-format warnings.
- Pure-logic tests for the scene map, the sequence matcher, the percent pacing, and the gcc log (`ctest`); scene previews with screenshots and an exit-sequence check under `tools/verify/`.
- Packaging: `tools/package.py` builds a deterministic `ra9fael.bosskey-0.1.0.wsplugin` (the Qt-less starter exe plus the QML cover DLL; no Qt bundled — the installed WinShun provides it).
