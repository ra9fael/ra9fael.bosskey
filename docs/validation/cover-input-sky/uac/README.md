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
| `uacGuard` setting on, through the resident action path | The action answered with the guard's `open` effect; no cover appeared before the guard was ready; plugin exited 0 with no helper left | `settings-runner.json` |
| Cover after the mode label was removed | No on-screen mode text; ordinary fallback still exits and restores Win+R | `fallback-runner.json`, `fallback-input.json`, `cover.png` |
| Released 0.1.3 build, same two cases | Guard requested by the `uacGuard` setting; ordinary fallback exits and restores Win+R | `release-settings-runner.json`, `release-fallback-runner.json` |

Native no-focus exit and focused QML exit are separate acceptance cases. The no-focus tests send a non-sequence letter followed by `deadbeef` to the external target; only character counts, not typed-text contents, are recorded. The shortcut cases cover Ctrl+Shift+Esc, Shift+F10, Alt+Tab, Alt+Esc, Alt+F4, Win+D, Win+R and the Windows key.

`bosskey_logic` and `bosskey_input` pass under MSVC Release. The input-policy tests cover overlapping sequences, auto-repeat, modifier state, numpad digits, one exit notification per cover, and draining swallowed repeats/key-up events. The guard executable has no Qt dependency and uses the static CRT.

The cover draws no mode label: 0.1.3 removed the brief enhanced/ordinary text so nothing on screen betrays the disguise. `cover.png` is the 480×270 cover of the ordinary fallback run; the mode is recorded in the runner logs (`protection=enhanced|ordinary`) instead. Raw controller output is generated in `build/uac-input/validation/`; only selected result files and this screenshot are retained here. The isolated runtime deployment at `C:\Users\renfei\AppData\Local\Temp\opencode\bosskey-uac-input` was removed after installation verification.

## Release and local installation

`v0.1.3` is published from tag `v0.1.3` (`bedda0d`) by the repository's own workflow: verify → build on `windows-2022` → ctest → package `--tag v0.1.3` → `tools/ci_release.py` verification → publish. The release carries `ra9fael.bosskey-0.1.3.wsplugin` (sha256 `baf9ff377d74d404cf3181e72422d7b153eecb8a531db4115a390ae74c8a335d`) and its `.sha256` file; the checksum file matches the asset digest, and the tag commit is the source the workflow checked out.

That published package was downloaded and installed with `ws plugin install`, replacing the local build, and the installed `bosskey.exe`, `BossKeyApp.dll` and `bosskey-guard.exe` are byte-for-byte equal to the ones inside the release archive (`installed.json`). The permission set is unchanged (`ui`, `processes`), so no new consent was needed, and the plugin is running. The resident plugin is verified restricted/Low (4096), without UIAccess; no guard process remains while the cover is inactive. Both acceptance modes were re-run against this released build (`release-*.json`). The existing `v0.1.1` tag and its release assets are unchanged.

Two CI-only fixes were needed before the release could publish, neither of which changes runtime behavior: the Qt installation step now installs the `qtshadertools` module that `qt_add_shaders` needs, and `bosskey_input` checks a null process handle instead of `INVALID_HANDLE_VALUE`, which is the current process's pseudo handle and made the assertion depend on whether the machine is elevated.

## Reproduction

Use a runtime deployment whose executables retain ordinary installed-file integrity labels; the workspace itself is Low-labelled and the real runner correctly refuses its images under `IMAGE_LOAD_NO_LOW_LABEL`.

```powershell
python tools/verify/strict_preview.py guard build/uac-input/validation/medium --exe <runtime>/bosskey.exe --enhanced
python tools/verify/strict_preview.py guard build/uac-input/validation/qml --exe <runtime>/bosskey.exe --focused-qml
python tools/verify/strict_preview.py guard build/uac-input/validation/cancel --exe <runtime>/bosskey.exe --simulate-guard-error 1223
python tools/verify/strict_preview.py settings build/uac-input/validation/settings --exe <runtime>/bosskey.exe
```

For the High Integrity external target and lifecycle cases, run `tools/verify/uac_suite.ps1 -Runtime <runtime>` as administrator. It elevates only the isolated verification controller, not the user's resident WinShun. For an actual consent-rejection check, run the Medium Integrity controller with `--enhanced --expect-declined` and decline the displayed UAC request.

## Limitations

- Actual UAC rejection still requires manual sign-off. The recorded cancellation and startup-failure cases inject the guard's failure status; they are not evidence of a user rejecting the Windows consent dialog.
- Ordinary mode does not guarantee no-focus suppression across integrity levels. The enhanced tests establish suppression of exit-sequence letters/digits and the listed shortcuts, not every possible input method or secure-desktop interaction.
- Readiness and helper elevation alone are not acceptance. Only the separately recorded focus, exit-source, received-character and cleanup results establish the tested behavior.
