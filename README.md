# ra9fael.bosskey · 老板键

[English](README.en.md) | 中文

boss 来了？输入 `boss b`，屏幕瞬间变成蓝屏。`boss u` 是 Windows 更新，`boss t` 是一条正在编译的 gcc。键入退出按键序列（默认 `deadbeef`）回来——或者等它自己消失。

WinShun 的结果列表里保持体面；全屏的部分不体面。

## 使用

| 输入 | 效果 |
|---|---|
| `boss ` | 列出三个场景行 |
| `boss b` | 全屏蓝屏（Win10 风格，百分比缓慢收集错误信息） |
| `boss u` | 全屏 Windows 更新（spinner 卡在 87%） |
| `boss t` | 全屏 gcc 编译（`-Wall -O2`，真实格式的警告，持续滚动） |

回车触发全屏伪装；伪装期间所有按键都被吞掉、不产生任何显示，键入退出按键序列即返回。顶层关键字支持中文别名 `摸鱼`（`摸鱼 b` 同 `boss b`）。

## 设置

| 设置 | 默认 | 说明 |
|---|---|---|
| 子触发词映射 | `b=bsod,u=update,t=gcc` | 自定义子词，如 `a=bsod,s=update,d=gcc`；场景只能是 bsod / update / gcc |
| 退出按键序列 | `deadbeef` | 伪装界面下直接键入即退出，不区分大小写；只接受字母和数字 |
| 自动消失秒数 | `0` | 0 = 一直伪装到手动退出 |
| gcc 场景项目名 | `winshun-core` | 编译输出显示的目标名 |
| 显示器覆盖 | `mouse` | `mouse` = 只盖鼠标所在的屏幕；`all` = 盖住所有显示器 |

顶层关键字 `boss` 来自 plugin.json（协议限制，设置改不了）；子触发词、退出序列、超时都在设置里改。

## 安装

需要 WinShun **0.22.0 或更新**（插件声明了宿主的共享 Qt 契约 `runtime.qt: "host"`，复用 WinShun 的 Qt 6.12，包内不含 Qt）。两种安装方式：

- 双击 `dist/ra9fael.bosskey-0.1.0.wsplugin`，WinShun 会引导安装；
- 或 WinShun 里「插件 → 从归档安装」。

## 从源码构建

Windows + MSVC + Ninja + Qt 6（与 WinShun 同源）：

```powershell
# VS 开发者命令行里
cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_PREFIX_PATH=<Qt 的 msvc 目录>
cmake --build build
ctest --test-dir build          # 纯逻辑单测
python tools/package.py         # dist/ra9fael.bosskey-0.1.0.wsplugin
```

产物：源码目录下的 `bosskey.exe`（无 Qt 启动器）和 `BossKeyApp.dll`（QML 伪装窗口，协议层与界面层分离，架构同官方 capture 插件）。

场景可以脱离 WinShun 单看：

```powershell
.\bosskey.exe --cover bsod | update | gcc [--all] [--autohide <秒>] --app-dir "<WinShun 安装目录>"
```

`tools/verify/` 下的 `shot.py`（截图）、`exit.py`（发送退出序列验证退出）和 `qr_check.py`（用 OpenCV 独立解码二维码矩阵）配合 `--autohide` 使用。

## 架构

```
bosskey.exe      无 Qt 启动器：从 WS_QT_DIR（共享 Qt 契约，0.22.0+）或 WS_APP_DIR 取 Qt 目录，
                 加进 DLL 搜索路径后加载 BossKeyApp.dll
BossKeyApp.dll   worker 线程跑 winshun.h SDK（JSON-RPC over stdio）
                 GUI 线程预创建每屏一个隐藏的 QML 全屏窗口，触发时只做 showFullScreen + raise
logic.h          纯逻辑：场景映射、退出序列匹配、百分比节奏、gcc 日志生成（可单测）
qrcode.h         真二维码编码器（Version 3 / 纠错 L）：蓝屏上那个能扫的码
qmlapp/qml/      BossCover.qml（全屏容器 + 按键吞掉）+ 三个场景
```

细节见 [CHANGELOG.md](CHANGELOG.md)。协议：MIT，作者 ra9fael。
