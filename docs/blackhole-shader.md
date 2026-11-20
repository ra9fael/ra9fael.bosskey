# 黑洞 Shader 参考实现分析

分析对象：[XboxNahida/ghostty-blackhole-main](https://github.com/XboxNahida/ghostty-blackhole-main)
（v3.1.0，MIT License），一个 Windows 桌面黑洞屏保。它的黑洞不是贴图和动画曲线，
而是**逐像素数值积分史瓦西测地线**的 fragment shader，源头是 Eric Bruneton 的
[black_hole_shader](https://github.com/ebruneton/black_hole_shader)（BSD-3-Clause）。

本文的目的有三个：

1. 把它的 shader 实现拆到可以照着重写的细节；
2. 记录它和我们插件（原软件光栅 + Canvas）之间的架构差异；
3. 给出移植到本插件 QML `ShaderEffect` 的具体改法和代价。

结论先说：**物理部分可以几乎原样移植，卡点在渲染后端**——本插件原来的软件光栅路径
不渲染 `ShaderEffect`（已实测，见 [validation/shader-backend-probe](validation/shader-backend-probe)），
所以进程整跑 D3D11，`plugin.json` 的 `memoryMB` 从 128 提到 1024。已落地为 `boss h`，
实测数字见第 10 节。

---

## 1. 仓库里的 shader 文件地图

| 文件 | 使用者 | 作用 |
|---|---|---|
| `blackhole.glsl`（根目录） | Ghostty 终端自定义 shader | **核心实现**，单 fragment pass，注释最完整，本文主要拆它 |
| `shaders/blackhole.hlsl` | `blackhole.exe` D3D11 渲染路径 | 同一物理的 HLSL 直译，uniform 走 cbuffer |
| `shaders/blackhole_preview.glsl` | Qt 配置界面实时预览 | 同一物理 + GUI uniform 接口 |
| `shaders/frag_preview_header.glsl` | 上述预览 | uniform 清单：`uHoleRadius`、`uPreset*[64]`、`uPlayMode`、`uBornProgress` 等，"负值 = 用 shader 内默认值" |
| `shaders/frag_header.glsl` | 简化路径 | `sky()`：星云 + 三层闪烁星 + 银河带的程序化天空 |
| `shaders/frag_simple.glsl` | 旧版简化 shader | 纯径向近似（阴影圆 + 爱因斯坦环 + 色环），没有测地线 |
| `shaders/consumption*.glsl` | "持续吞噬"模式 | 物质被吸入的暖化/发光公式，共享 `consumptionBlackbody` |
| `shaders/desktop_transport_*.glsl` | 桌面输送/鼠标波纹 | 顶点位移 + 片元渐变，与黑洞物理无关 |
| `shaders/vert.glsl` / `fullscreen_vs.hlsl` | 两个渲染器 | 全屏四边形顶点着色器，就三行 |

> 协议提醒：参考仓库 MIT，Bruneton 原版 BSD-3。若移植其代码，需在源码注释和
> LICENSE 附近保留署名。

## 2. 渲染管线

```
blackhole.exe（托盘监控进程）
    └── blackhole.exe --render（GLFW + OpenGL 3.3 渲染进程）
            每帧：WGC/DXGI 捕获桌面 → staging texture → glTexSubImage2D
                  → 全屏四边形 → fragment shader（iChannel0 = 桌面纹理）
```

几个设计选择，和我们插件直接相关：

- **双进程**：渲染器被监控进程按需拉起/杀掉。README 的技术文档写明了原因：Win11 的
  DWM 会给全屏透明窗口加黄色强调边框，且 show/hide 会触发 DWM 重建缓存；opacity=0、
  缩成 1×1、WM hook 全部试过都失败，最后用进程级隔离（每次启动全新 GL 上下文和 DWM 状态）。
  窗口用 `WS_EX_NOACTIVATE | WS_EX_TOPMOST | WS_EX_TOOLWINDOW | WS_EX_TRANSPARENT | WS_EX_LAYERED`
  一次置顶，不再运行时轮询 Z 序。
- **单 fragment pass，无预计算纹理**：Bruneton 的做法是把史瓦西测地线预计算成查找纹理，
  运行时只采样；Ghostty 的自定义 shader 拿不到自定义纹理，所以改成每像素现算。
  这也正是我们能照抄的部分——它只依赖数学，不依赖任何桌面捕获。
- **Qt 预览不截图不采样桌面**：配置界面用 `QQuickFramebufferObject` 把同一个 shader 画进
  FBO（`frag_preview_header.glsl` 注释原话："无桌面纹理，用于 QQuickFramebufferObject"）。
  对我们是个好消息：**纯星空版不需要任何纹理输入**，桌面采样（`iChannel0`）整块可以删掉。

## 3. 物理模型与坐标系

单位：史瓦西半径 `r_s = 1`。三个尺度是物理常数不是美术参数：

| 量 | 值 | 含义 |
|---|---|---|
| 临界碰撞参数 `B_CRIT` | `3√3/2 ≈ 2.5980762` | 远方看到的阴影视半径；`b < B_CRIT` 的光子被俘获 |
| 光子球 | `r = 1.5` | 圆轨道存在的内界，盘内缘必须留在它外侧 |
| ISCO | `r = 3` | 最内稳定圆轨道，盘内缘的物理上限 |

屏幕到世界的映射（`mainImage` 开头）：

```glsl
vec2  p    = (uv - center) * vec2(aspect, 1.0);   // 以洞为中心、按高度归一的屏幕坐标
float W    = B_CRIT / max(rh, 1e-4);              // 1 屏幕单位 = W 个 r_s
vec2  pr   = rot(vec2(p.x, -p.y), L.roll) * W;    // 世界坐标（y 向上，带系统滚转）
float b    = length(pr);                          // 该像素对应的碰撞参数，单位 r_s
```

即：先把屏幕像素转成"这条光子的碰撞参数"，再决定它落入阴影、被透镜弯折、还是从盘面穿过。
阴影半径 `rh`（屏幕单位）由大小模式驱动；`DISK_ROLL` 让整个系统在屏幕平面内滚转。

## 4. 远场：解析弱偏折

积分很贵，所以只有洞附近的像素付这个钱。分界线：

```glsl
float bmax = rout + (rh < 0.085 ? 8.0 : 3.0);   // 小洞的盘面投影更靠近临界边缘，放宽求交范围
float Z0   = max(14.0, rout + 5.0);             // 相机距离
```

`b >= bmax` 的像素走解析式（作者用积分器拟合出来的有限相机修正，不是教科书那个
`α = 2r_s/b`——直接用会在交接圆上留下 ~20% 的位移跳变）：

```glsl
float u    = Z0 * inversesqrt(Z0*Z0 + b*b);      // = Z0/sqrt(Z0²+b²)
float defl = (2.0/(W*W)) / max(plen, 1e-4)
           * (1.29*u + 0.07) * max(LENS_DEPTH - 2.14*u + 0.75, 0.0)
           * window * shield;                    // world 单位的位移
```

配套细节：

- **色差**：三个通道各采样一次，`k = 1 + (i-1)*ab`，`ab = 0.035*smoothstep(1,2,b/bmax)`
  ——蓝光弯得比红光多一点，且只在远离交接圆处淡入。
- **镜像寻址** `mirrorUV(u) = 1 - abs(1 - mod(u, 2))`：弯折后的取样坐标可能出屏，
  镜像重复，不出边缘拖影。
- **两段衰减**：`window = exp(-(plen/(7*rh))²)` 让透镜效应在几个盘直径外淡出
  （否则洞一漂，全屏文字都跟着抖）；`shield` 保留下方工作区不畸变。
  **衰减的是位移不是颜色**，所以没有接缝。

## 5. 近场：逐像素测地线积分

平行光从相机（`+z`，距离 `Z0`）出发，洞在原点，`r_s = 1`。用的是 Binet 形式的光子
运动方程——`h = |x × v|` 守恒，所以每像素只算一次：

```glsl
vec3  x  = vec3(pr, Z0);        // 位置
vec3  v  = vec3(0, 0, -1);      // 方向（平行光）
float h2 = dot(pr, pr);         // h² = b²

// 每步：a = -(3/2) h² x / r⁵   （精确的史瓦西光子偏折）
```

积分器是 **leapfrog（kick-drift-kick）**，步长随半径自适应——光子球附近细、远处粗
（远处偏折按 1/r⁴ 衰减，粗一点无所谓，省下的步数留给强弯曲区）：

```glsl
float dt = clamp(0.16 * r, 0.03, 1.5);
vec3  a  = -1.5 * h2 * x / (r2*r2*r);
v += a * (0.5*dt);  x += v * dt;        // kick-drift
r2 = dot(x,x); r = sqrt(r2);
a  = -1.5 * h2 * x / (r2*r2*r);
v += a * (0.5*dt);                      // kick
```

终止条件（任一命中即停）：

| 条件 | 含义 |
|---|---|
| `r2 < 1.0` | 进入事件视界 → `captured`，像素纯黑 |
| `x.z < -Z0 && v.z < 0` | 从相机背后逃出 |
| `r2 > 4*Z0*Z0` | 被甩到侧方远处 |
| 步数预算耗尽 | `N_STEPS = 48`；若此时 `dot(x,x) < 4`（仍缠在光子球附近）按 captured 计 |

小黑洞（`rh < 0.085`）有两个专门修正：允许先完成本步盘面求交再终止于视界，否则内缘
盘带被提前截断；`diskRin` 从 1.60 平滑过渡到配置内缘，避免倾角变化时盘带断裂。

## 6. 吸积盘：光子在盘面上穿过时发生什么

盘的几何：倾角 `DISK_INCL` 绕屏幕 x 轴倾斜，法向 `n = (0, sin i, cos i)`，
面内另一轴 `e2 = (0, cos i, -sin i)`。

**穿越检测**：每步算 `s = dot(x, n)`，`s * sPrev < 0` 说明这一步跨过了盘面，用线性插值
还原精确交点：

```glsl
float tc = sPrev / (sPrev - s);
vec3  xc = mix(xPrev, x, tc);
float rc = length(xc);
if (rc > diskRin && rc < rout) { ... }
```

同一条光线可以穿过盘面多次——远侧盘弧因此会"翻到"阴影上方和下方各出现一次，
这就是 Interstellar 那种上下双弧，**不是画出来的，是积分出来的**。

交点处的发光计算，按顺序：

1. **径向包络** `band`：内缘 smoothstep 淡入 + 外缘 0.7rout 起淡出。
2. **角向条纹**（开普勒 + 引力时间膨胀）：
   ```glsl
   float kep    = pow(diskRin/rc, 1.5);              // 开普勒：内圈转得快
   float gloc   = sqrt(max(1.0 - 1.5/rc, 0.02));     // √(1-1.5/r)：内圈时间变慢
   float swirl  = rc*L.wind*0.12 - t*kep*spd*gloc*dil*sdir;   // dil 随洞变大整体减速
   float fine   = vnoiseWrapY(vec2(rc*2.8, turns*19.0 + swirl*3.0), 19.0);
   float broad  = vnoiseWrapY(vec2(rc*1.0, turns*9.0  + swirl*1.5 + 7.0), 9.0);
   ```
   `vnoiseWrapY` 是 y 方向按 `perY` 环绕的值噪声——`turns` 每转一圈刚好前进 `perY` 格，
   跨 `atan` 分支切割处条纹无缝。
3. **温度剖面**（Shakura–Sunyaev 型，峰值归一到 1）：
   ```glsl
   float xpr   = max(1.0 - sqrt(diskRin/rc), 0.0);
   float tprof = pow(rin/rc, 0.75) * pow(xpr, 0.25) / 0.488;
   ```
4. **相对论多普勒 + 引力红移**：圆轨道气体速度 `beta = 1/sqrt(2(rc-1))`，
   ```glsl
   vec3  gasdir = normalize(cross(n, xc)) * sdir;        // 气体运动方向
   float g = gloc / max(1.0 + beta*dot(gasdir, normalize(v)), 0.05);  // √(1-1.5/r)/(1-β·k̂)
   g = mix(1.0, g, L.dopp);                              // dopp=0 时关掉不对称
   ```
5. **颜色与束流**：黑体色 `blackbody(L.temp * tprof * g)`（Tanner Helland 拟合），
   观测强度 `boost = pow(g, L.beam)`——朝我们转来的一侧亮且偏蓝，是经典多普勒束流。
6. **半透明累积**（光线可能穿盘多次，必须按透射率往前传）：
   ```glsl
   emitc += trans * cbb * (L.gain * 2.2 * density * tprof*tprof * boost);
   trans *= 1.0 - clamp(L.opac * density, 0.0, 1.0);
   ```
   斜视时前盘与后盘投影重叠，作者把前盘不透明度降到 0.58 倍，避免后侧盘面和视界边缘
   被压成黑色缺口。

## 7. 背景：逃逸光线从哪来

```glsl
vec3 d = normalize(v);                       // 逃逸方向
bg += stars(d) * L.star * window * shield;   // 程序化星空
if (d.z < -0.05) {
    float tpl = (-LENS_DEPTH - x.z) / d.z;   // 沿逃逸光线投影到洞后方的"天空平面"
    vec3  hp  = x + d * tpl;
    vec2  q   = rot(hp.xy, -L.roll) / W;
    vec2  sp  = vec2(q.x, -q.y);
    vec2  suv = mirrorUV(center + (p + (sp - p)*window*shield) / vec2(aspect,1.0));
    bg += texture(iChannel0, suv).rgb * smoothstep(0.05, 0.35, -d.z);
}
```

即：把弯折后的出射光线反向投影回终端（桌面）平面取样。弯得超过 ~90° 的光子永远到不了
那个平面，用 `toward` 淡出到星空，不采样垃圾。

**星空本身**（`stars()`）按出射方向的球面坐标取网格：`sph = (atan(d.x,-d.z), asin(d.y))`，
`sph*40` 分格，8% 的格子有星，每星有随机偏移、闪烁周期和冷暖色。因为采样用的是**弯折后**
的方向，星星在洞周围自动拉成弧线——这是免费得来的透镜感，不需要额外画什么。

最后合成，盘光是 HDR，走 `1 - exp()` 色调映射叠在被保真的背景上：

```glsl
vec3 col = bg * trans + (vec3(1.0) - exp(-emitc * L.expo));
```

## 8. 参数与预设体系

14 个盘参数打包成一个 `DiskLook` struct（temp/incl/roll/inner/outer/opac/dopp/beam/
gain/contr/wind/speed/expo/star）。三个使用方：

- **Ghostty 版**：编译期常量，附一个 42 秒 demo 巡回（8 个预设，槽边界 `smoothstep` 交叉淡入）。
- **D3D11 渲染器**：cbuffer 里 14×64 的预设数组 + `playMode`（0 顺序 / 1 循环 / 2 随机），
  GUI 单项覆盖优先于预设。
- **Qt 预览**：`uPreset*[64]` uniform 数组，同样"负值 = 用 shader 默认"。

作者在技术文档里记了一个关键坑：**GLSL 的 `const` 不能用 uniform 赋值**，
`const float TOKEN_HOME_X = uHomeX;` 直接编译失败，必须去掉 `const`。

## 9. 性能特征

- 只有 `b < bmax` 的近场像素付 48 步积分；洞小的时候近场只占屏幕一小块。
- 成本量级：屏幕像素数 × N_STEPS 次积分步。Ghostty 版注释自己算过：5K 屏、高填充率时
  约 1500 万像素 × 48 步/帧，桌面独显没问题。
- 每一步约十几次浮点运算 + 一次 sqrt，GPU 上这是廉价工作；**软件光栅化跑这个不现实**
  （1920×1080 全屏近场 ≈ 200 万像素 × 48 步 ≈ 1 亿次积分步/帧）。

## 10. 我们插件的落地形态与实测

本插件（`ra9fael.bosskey`）的封面：`MatrixScene` 仍是 QML `Canvas` 手绘（每 100 ms 整幅重绘），
`BlackholeScene` 是本文移植出来的 `ShaderEffect`。整个进程跑 Qt 的 **D3D11 后端**
（`qmlmain.cpp`：`QQuickWindow::setGraphicsApi(QSGRendererInterface::Direct3D11)`），
`plugin.json` 的 `memoryMB` 相应从 128 提到 1024。

之所以不留软件路径，是实测出来的（证据与截图：
[validation/shader-backend-probe](validation/shader-backend-probe)）：

| 后端 | 结果 |
|---|---|
| 软件 | **完全不渲染** `ShaderEffect`，静默跳过，无报错 |
| D3D11 | 正常渲染，`qt_MultiTexCoord0` 逐像素坐标正确 |

即软件后端下 `boss h` 会是一片纯黑——一个静默的坏状态，所以它被整个移除了，
而不是留成开关。

### 场景实测

`boss h`（`--cover blackhole`）在本机 1920×1080 全屏下：

| 项目 | 数值 |
|---|---|
| 私有提交 | **91.8 MB**（工作集 77.3 MB） |
| CPU | ≈ 0% 单核 |
| 帧间隔 | 31 ms（≈32 fps）；480×270 窗口同为 31 ms，**与像素数无关，不是填充率瓶颈** |

帧间隔的上限来自本机的 GameViewer/Remote Display 虚拟显示管线，不是这个 shader 的成本；
帧数与像素数无关也印证了这一点。细节、截图与未覆盖项见
[validation/blackhole-shader-scene](validation/blackhole-shader-scene)。

之前那版 Canvas 黑洞之所以难看，根因是手绘椭圆环没有测地线：没有真正的阴影边界、
没有多次穿盘形成的上下双弧、没有透镜星空。参考实现里这三样都是积分免费附带的。

## 11. 移植到 QML `ShaderEffect` 的具体改法

Qt 6 的 `ShaderEffect` 只接受 `qsb` 烘焙过的 `.qsb`（`qsb --qt6`，产出 GLSL 100 es/120/150
+ HLSL 50 + MSL 12 变体）。源码是 GLSL ES 1.00 子集，需要这些改写：

| 参考实现里的东西 | 移植处理 |
|---|---|
| `struct DiskLook` + `DiskLook[]` 预设数组 | 拆成 14 个 `property real`，预设交叉淡入改成 QML 侧两组属性 + `mix` uniform |
| `iChannel0` 桌面纹理 | **整块删除**。我们不需要桌面取样；`mirrorUV`、色差、天空平面投影一起删 |
| `iTime` | QML 属性 `property real t` + 30 fps `Timer`（`running: cover.shown`，隐藏即停；按真实经过时间累加，帧率不影响节奏） |
| `iResolution` | `property real uAspect`，由 `width / height` 推出（shader 只需要宽高比） |
| token 解码 / pomodoro / demo 巡回 | 删除，与插件无关 |
| `#define N_STEPS 48`、`B_CRIT` | 原样保留（编译期常量） |
| 全部物理函数（hash/vnoise/blackbody/stars/测地线/盘面） | **原样保留**，纯函数无 API 依赖 |
| 小洞修正（`rh<0.085` 分支） | 保留；封面场景的洞通常就是小洞 |
| `uDiskRenderMode` 线条化/柔边 | 暂不需要，先只做常规模式 |

工程侧：CMake 里给 `qmlapp/qml/BlackholeScene.frag` 加一步 `qsb`，产物 `.qsb` 和现有 QML
一样登记进资源（`qt_add_shaders`，`PREFIX "/qt/qml/BossKey"` + `BASE`，QML 里用相对 URL
引用）。多屏每块屏幕独立动画沿用现在的 `screenIndex`：作为 uniform 传进 shader 做相位偏移
即可，不需要每屏一份 shader。

### 移植阶段实测到的坑（已解决）

| 现象 | 原因与改法 |
|---|---|
| `qt_add_shaders` 报 `FindFirstFileExA(.qsb/D:/…)` | 它把 `output_file` 拼在 `.qsb/` 后面，绝对路径会拼坏。`FILES` 用相对路径并给 `BASE`，产物名才正好是 `BlackholeScene.frag.qsb`，资源路径才对得上 |
| qsb 报 `ES shaders for SPIR-V require version 310 or higher` | 文件头要有 `#version 440`（qsb 会为每个目标改写版本号）；不写版本号时 glslang 按 100 es 编译 |
| qsb 报 `'non-opaque uniforms outside a block'` | qsb 生成 HLSL/MSL 时走 Vulkan 规范的 glslang，**所有 uniform 必须放进 `layout(std140, binding = 0) uniform buf { … };`** |
| 烘焙成功、零报错，但 ShaderEffect 什么都不画 | `buf` 块必须以 `mat4 qt_Matrix; float qt_Opacity;` 开头。缺这两个成员，Qt 绑不上自己的块，效果**静默不渲染**（日志一个字都没有）。最费时间的坑 |
| HLSL 侧 `mod` 语义不同 | GLSL `mod` 取地板、HLSL `fmod` 截断：自己写 `glmod(x,y) = x - y*floor(x/y)` |
| HLSL 不支持向量三元表达式 | 用 `if`/`else` 或 `mix`，不要写 `cond ? vec3(a) : vec3(b)` |
| `precision highp float;` | 不需要，qsb 会给 ES 目标注入精度限定 |

版权：源码注释里保留 Bruneton 与参考仓库的署名（BSD-3 / MIT）。

## 12. 落地形态与代价

结论：走 shader，进程整跑 D3D11，`memoryMB` 128 → 1024。

| 换来什么 | 付出什么 |
|---|---|
| 参考实现同款：真阴影边界、光子环、多普勒束流、多次穿盘形成的上下双弧、被弯成弧线的星空 | 进程不再零 GPU：所有场景都吃 D3D11 初始化；单屏 1080p 私有提交 91.8 MB（原软件路径单屏 ~27.5 MB） |
| 观感可调：色温、增益、倾角、转速都是 uniform，改一个数字就行 | `memoryMB` 是对已安装用户可见的 manifest 改动，1024 MB 已为多屏 swapchain 留足余量（双 2560×1440 参照早前记录约 210 MB） |

被否掉的两条路：留在软件光栅重画 Canvas 版（视觉上限明显低一档，且 `boss h` 在软件后端
根本不显示），以及参考仓库那种子进程渲染器（要复制窗口管理、Z 序、退出序列与进程生命周期，
复杂度最高，而常驻进程那点内存并不是瓶颈）。

剩下的工作在"调"而不在"造"：盘外缘的收边、以及帧率上限——现在动画由 30 fps 的 `Timer`
驱动，覆盖要挂几分钟，没必要满帧烧 GPU。
