# Optional UAC input protection

Validation session: 2026-11-21 UTC. Every interactive cover used a 480×270 window at (0, 0). Production fullscreen behavior is unchanged.

The plugin ran through the installed `C:\Program Files\WinShun\WinShunPluginHost.exe` with `strict: true`, a restricted Low Integrity token (4096), and no UIAccess. Its independent input guard ran at High Integrity (12288). The ordinary-application test controller and runner were Medium Integrity; the administrator-application tests used an isolated elevated controller/runner, with the cover still restricted and Low. The resident WinShun application was not elevated for these tests.

## Results

| Case | Result | Evidence |
|---|---|---|
| Native exit while a Medium Integrity external window owns focus | Exit matched; 0 received characters; supported shortcuts suppressed; Win+R restored; helper exited | `medium-runner.json`, `medium-input.json` |
| Native exit while a High Integrity external window owns focus | Exit matched; 0 received characters; supported shortcuts suppressed; Win+R restored; helper exited | `high-runner.json`, `high-input.json` |
| Focused QML exit, native hooks disabled | QML exit observed, no native exit observed; Win+R restored | `qml-runner.json`, `qml-input.json` |
| Simulated consent cancellation (1223) | One launch request; ordinary mode; focused exit works; Win+R restored | `simulated-decline-*.json` |
| Simulated helper startup failure (2) | One launch request; ordinary mode; focused exit works; Win+R restored | `simulated-startup-failure-*.json` |
| Plugin job terminated | Helper exited; Win+R restored | `crash-runner.json` |
| Elevated helper terminated while the cover remains | Ordinary fallback without another launch request; focused exit works; Win+R restored | `disconnect-*.json` |
| Timed cover closure | Helper exited; Win+R restored | `autohide-runner.json` |

Native no-focus exit and focused QML exit are separate acceptance cases. The no-focus tests send a non-sequence letter followed by `deadbeef` to the external target; only character counts, not typed-text contents, are recorded. The shortcut cases cover Ctrl+Shift+Esc, Shift+F10, Alt+Tab, Alt+Esc, Alt+F4, Win+D, Win+R and the Windows key.

`bosskey_logic` and `bosskey_input` pass under MSVC Release. The input-policy tests cover overlapping sequences, auto-repeat, modifier state, numpad digits, one exit notification per cover, and draining swallowed repeats/key-up events. The guard executable has no Qt dependency and uses the static CRT.

The two small screenshots show the brief mode label: `enhanced.png` and `ordinary.png`. Source evidence is in `build/uac-input/validation/`; only selected result files and these screenshots are retained here. The temporary runtime staging path is `C:\Users\renfei\AppData\Local\Temp\opencode\bosskey-uac-input`.

## Local installation

The 0.1.2 package passes `tools/ci_release.py` package verification and has been installed through `ws plugin install`. The installed starter, DLL and input helper are byte-for-byte equal to the packaged build. WinShun has suspended the plugin pending the user's confirmation of the new `processes` permission; that confirmation is not supplied by the test controller. The existing `v0.1.1` tag and its release assets are unchanged.

## Reproduction

Use a runtime deployment whose executables retain ordinary installed-file integrity labels; the workspace itself is Low-labelled and the real runner correctly refuses its images under `IMAGE_LOAD_NO_LOW_LABEL`.

```powershell
python tools/verify/strict_preview.py guard build/uac-input/validation/medium --exe <runtime>/bosskey.exe --enhanced
python tools/verify/strict_preview.py guard build/uac-input/validation/qml --exe <runtime>/bosskey.exe --focused-qml
python tools/verify/strict_preview.py guard build/uac-input/validation/cancel --exe <runtime>/bosskey.exe --simulate-guard-error 1223
```

For the High Integrity external target and lifecycle cases, run `tools/verify/uac_suite.ps1 -Runtime <runtime>` as administrator. It elevates only the isolated verification controller, not the user's resident WinShun. For an actual consent-rejection check, run the Medium Integrity controller with `--enhanced --expect-declined` and decline the displayed UAC request.

## Limitations

- Actual UAC rejection still requires manual sign-off. The recorded cancellation and startup-failure cases inject the guard's failure status; they are not evidence of a user rejecting the Windows consent dialog.
- Ordinary mode does not guarantee no-focus suppression across integrity levels. The enhanced tests establish suppression of exit-sequence letters/digits and the listed shortcuts, not every possible input method or secure-desktop interaction.
- Readiness and helper elevation alone are not acceptance. Only the separately recorded focus, exit-source, received-character and cleanup results establish the tested behavior.
