# ra9fael.bosskey · 老板键

[English](README.en.md) | 中文

boss 来了？输入 `boss bsod`，授权后屏幕变成蓝屏。`boss wu` 是 Windows 更新，`boss gcc` 是一条正在编译的 gcc，`boss mr` 是黑客帝国代码雨，`boss bh` 是黑洞吸积盘。键入退出按键序列（默认 `deadbeef`）回来——或者等它自己消失。

WinShun 的结果列表里保持体面；全屏的部分不体面。

## 使用

| 输入 | 效果 |
|---|---|
| `boss ` | 列出五个场景行，每行都写着能触发它的词 |
| `boss bsod` / `lp` | 全屏蓝屏（Win10 风格，百分比缓慢收集错误信息） |
| `boss wu` / `gx` | 全屏 Windows 更新（spinner 卡在 87%） |
| `boss gcc` / `by` | 全屏 gcc 编译（`-Wall -O2`，真实格式的警告，长行换行，永远编译不到 100%） |
| `boss mr` / `dmy` | 全屏黑客帝国代码雨（片假名、数字与拉丁字母，绿色渐隐轨迹） |
| `boss bh` / `hd` | 全屏黑洞（不透明星空被引力透镜扭曲；吸积盘先铺开、黑洞随后长大，之后随机游走） |
| `boss aq` / `szg` | 全屏 ASCII 水族馆 |

回车启动伪装；设置里打开“请求 UAC 增强输入防护”后，每次先请求一次 UAC，允许则启用增强防护，关闭或拒绝则使用普通模式。默认触发词只保留英文缩写和拼音首字母（`boss bh` 等于 `boss hd`），搜索结果里每一行都列着这两个词；想增加其他词可在设置里改。顶层关键字支持中文别名 `摸鱼`（`摸鱼 bh` 同 `boss bh`）。

`boss blackhole` 绘制不透明的程序化星空，不采集桌面、不最小化其他窗口。引力透镜弯曲星空，黑洞阴影保持纯黑。

## 设置

| 设置 | 默认 | 说明 |
|---|---|---|
| 子触发词映射 | `bsod=bsod,lp=bsod,wu=update,gx=update,gcc=gcc,by=gcc,mr=matrix,dmy=matrix,bh=blackhole,hd=blackhole,aq=aquarium,szg=aquarium` | 默认只用英文缩写和拼音首字母；自定义设置可增加其他词 |
| 退出按键序列 | `deadbeef` | 伪装界面下直接键入即退出，不区分大小写；只接受字母和数字 |
| 自动消失秒数 | `0` | 0 = 一直伪装到手动退出 |
| gcc 场景项目名 | `winshun-core` | 编译输出显示的目标名 |
| 显示器覆盖 | `mouse` | `mouse` = 只盖鼠标所在的屏幕；`all` = 盖住所有显示器 |
| 请求 UAC 增强输入防护 | `false` | 开 = 每次启动伪装先请求一次 UAC，允许后失焦也能拦截退出序列；关 = 不弹 UAC，只在封面有焦点时拦截按键 |

顶层关键字 `boss` 来自 plugin.json（协议限制，设置改不了）；子触发词、退出序列、超时都在设置里改。

## 权限

申请 `ui` 和 `processes`：创建伪装窗口，由宿主启动包内固定的 `bosskey-guard.exe`。新增权限需要在 WinShun 中确认，插件主体仍为 `sandbox: strict`，不随守卫提权。

设置里打开“请求 UAC 增强输入防护”（默认关闭）后，每次启动伪装先请求一次 UAC，等到守卫就绪再显示封面；允许后独立管理员守卫抑制字母数字与约定快捷键，并在失焦时匹配退出序列。关闭该设置、拒绝授权或守卫启动失败都只使用普通模式，不重复请求。普通模式只在封面有焦点时拦截按键，不保证失焦后的跨权限抑制。

守卫不加载 QML、不接受任意程序或命令、不保存或转发逐键输入；通信只有就绪、退出、停止与错误状态。封面关闭时处理已吞按键的释放；插件退出、崩溃或管道断开后守卫退出，不常驻。增强防护不替代 Windows 锁屏，也不能拦截 Ctrl+Alt+Del 等安全注意序列。
封面跑在 D3D11 上（黑洞场景是逐像素积分测地线的 fragment shader，Qt 的软件光栅画不出
`ShaderEffect`），需要可用的 D3D11——Windows 10/11 均内置。

## 安装

需要 WinShun **0.22.0 或更新**（插件声明了宿主的共享 Qt 契约 `runtime.qt: "host"`，复用 WinShun 的 Qt 6.12，包内不含 Qt）。两种安装方式：

- 双击 `dist/ra9fael.bosskey-0.1.4.wsplugin`，WinShun 会引导安装；
- 或 WinShun 里「插件 → 从归档安装」。

## 从源码构建

Windows + MSVC + Ninja + Qt 6（与 WinShun 同源）：

```powershell
# VS 开发者命令行里
cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_PREFIX_PATH=<Qt 的 msvc 目录>
cmake --build build
ctest --test-dir build          # 纯逻辑单测
python tools/package.py         # dist/ra9fael.bosskey-0.1.4.wsplugin
```

产物：源码目录下的 `bosskey.exe`（无 Qt 启动器）、`BossKeyApp.dll`（QML 伪装窗口）和 `bosskey-guard.exe`（无 Qt、静态 CRT 的可选管理员输入守卫）。

场景可以脱离 WinShun 单看：

```powershell
.\bosskey.exe --cover bsod | update | gcc | matrix | blackhole [--all] [--windowed] [--autohide <秒>] --app-dir "<WinShun 安装目录>"
```

`tools/verify/` 下的 `shot.py`（截图，`--at 0.8,3,8` 可按时间点连拍同一场景）、`exit.py`（发送退出序列验证退出）和 `qr_check.py`（用 OpenCV 独立解码二维码矩阵）配合 `--autohide` 使用。

## 架构

```
bosskey.exe      无 Qt 启动器：从 WS_QT_DIR（共享 Qt 契约，0.22.0+）或 WS_APP_DIR 取 Qt 目录，
                 加进 DLL 搜索路径后加载 BossKeyApp.dll
BossKeyApp.dll   worker 线程跑 winshun.h SDK（JSON-RPC over stdio）
                 GUI 线程预创建每屏一个隐藏的 QML 全屏窗口，触发时只做 showFullScreen + raise
logic.h          纯逻辑：场景映射、退出序列匹配、百分比节奏、gcc 日志生成（可单测）
qrcode.h         真二维码编码器（Version 3 / 纠错 L）：蓝屏上那个能扫的码
qmlapp/qml/      BossCover.qml（全屏容器 + 按键吞掉）+ 五个场景
```

细节见 [CHANGELOG.md](CHANGELOG.md)。协议：MIT，作者 ra9fael。
