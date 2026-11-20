# Shader 后端探针验证

验证问题：Qt 6.12 的 **软件光栅后端**（当时 `bosskey.exe` 的默认渲染路径，`qmlmain.cpp` 里
`QQuickWindow::setGraphicsApi(QSGRendererInterface::Software)`）能否渲染 `ShaderEffect`。

方法：临时在 `BossCover.qml` 注册一个 probe 场景，内容为一个全屏 `ShaderEffect`
（`qsb --qt6` 烘焙的渐变 fragment shader，输出随 `qt_MultiTexCoord0` 逐像素变化）
加一个白色标记矩形，用 `bosskey.exe --cover probe --windowed` 分别在两种后端下截图
（`tools/verify/shot.py`）。当时两个后端由 `BOSSKEY_RENDERER` 环境变量切换（现已移除）。
探针场景已从源码和构建中移除，仓库恢复原状。

| 后端 | 环境 | 截图 | 结果 |
|---|---|---|---|
| 软件（默认） | 未设置 `BOSSKEY_RENDERER` | `plugin-software.png` | ShaderEffect **完全不渲染**：只剩背景矩形和白色标记，无任何着色器输出，也无报错（静默跳过） |
| D3D11 | `BOSSKEY_RENDERER=d3d11` | `plugin-d3d11-bands.png` | 正常渲染：8 条竖向红带 + 纵向渐变，逐像素纹理坐标正确 |

结论：本插件的默认软件路径跑不了 shader；上 shader 必须让进程跑在 D3D11 上。
这条结论直接决定了后续取舍——软件路径已随之移除，D3D11 现在是唯一后端
（`plugin.json` 的 `memoryMB` 提到 1024 MB）。

相关分析见 [../blackhole-shader.md](../blackhole-shader.md)。
