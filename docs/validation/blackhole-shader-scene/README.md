# 黑洞 Shader 场景实测

`boss h`（`--cover blackhole`）是一个 `ShaderEffect` 场景：逐像素积分史瓦西测地线，
阴影、光子环、盘面上下双弧都是积分结果，不是画出来的。移植与坑位分析见
[../../blackhole-shader.md](../../blackhole-shader.md)。

该场景现在的形态是**桌面之上的透明透镜**——桌面从透明封面透出、被弯成弧线显示；
当前实测与截图见 [../blackhole-desktop-lens](../blackhole-desktop-lens)。本目录保留的是
shader 场景在星空背景时期的测量，其中"帧间隔与像素数无关"这条结论至今有效。

环境：Windows 11，NVIDIA RTX 3080，显示 2560×1440@60Hz（当前会话为 1920×1080 逻辑分辨率，
GameViewer/Remote Display 虚拟显示），Qt 6.12.0 MSVC 2022 x64。

| 项目 | 数值 | 条件 |
|---|---|---|
| 私有提交 | 91.8 MB（工作集 77.3 MB） | 全屏 1920×1080，D3D11，实时渲染 |
| CPU | ≈ 0% 单核 | 计算在 GPU，CPU 只提交命令 |
| 帧间隔 | 31 ms（≈32 fps） | 480×270 窗口同为 31 ms，**与像素数无关，不是填充率瓶颈** |

`blackhole-shader-scene.png` 是星空背景时期的全屏 1080p 画面。

已知未覆盖：

- 双屏/多屏未实测（本机当前只枚举到一块显示器）。`memoryMB` 取 1024 MB 已为此留出余量：
  参照本插件早前记录，D3D11 在双 2560×1440 下约 210 MB。
- 帧间隔上限来自本机虚拟显示管线，真实独显直连显示器上应能跑满 60 fps；30 fps 的
  `Timer` 上限因此在本机无法与显示管线的 31 ms 区分开。
