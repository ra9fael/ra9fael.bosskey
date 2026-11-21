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
所以进程整跑 D3D11，`plugin.json` 的 `memoryMB` 从 128 提到 1024。已落地为 `boss blackhole`，
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
  即星空版不需要任何纹理输入；我们的封面也使用程序化星空，不采样桌面。

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
那个平面，用 `toward` 淡出，不采样垃圾。

**星空本身**（`stars()`）按出射方向的球面坐标取网格：`sph = (atan(d.x,-d.z), asin(d.y))`，
`sph*40` 分格，8% 的格子有星，每星有随机偏移、闪烁周期和冷暖色。因为采样用的是**弯折后**
的方向，星星在洞周围自动拉成弧线——这是免费得来的透镜感，不需要额外画什么。

当前实现保留了这颗程序化"天空"：星点与尘埃是引力透镜可见形变的背景结构。此前曾以桌面快照替代星野，相关历史实验记录在 validation/blackhole-desktop-lens。
远近两场都采样它，只是路径不同：

- **远场**（`b >= bmax`，解析偏折）：位移 `defl` 把取样点沿径向推离洞心，三通道各采一次
  得到轻微色差；`window` 只衰减位移不衰减颜色，透镜效应在几个盘直径外淡出时没有接缝。
- **近场**（逃逸光线）：沿出射方向投影回洞后方的桌面平面，再映射回快照；`window` 同样
  只掺在位移里，`toward` 挡掉弯过 90° 的光子。

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

即软件后端下 `boss blackhole` 会是一片纯黑——一个静默的坏状态，所以它被整个移除了，
而不是留成开关。

### 场景实测

`boss blackhole`（`--cover blackhole`）在本机 1920×1080 全屏下：

| 项目 | 数值 |
|---|---|
| 私有提交 | **106 MB**（工作集 86.1 MB），全屏 1920×1080 |
| CPU | 2.3% 单核 |
| 帧间隔 | 31 ms（≈32 fps）；480×270 窗口同为 31 ms，**与像素数无关，不是填充率瓶颈** |

帧间隔的上限来自本机的 GameViewer/Remote Display 虚拟显示管线，不是这个 shader 的成本；
帧数与像素数无关也印证了这一点。细节、截图与未覆盖项见
[validation/blackhole-shader-scene](validation/blackhole-shader-scene)。

之前那版 Canvas 黑洞之所以难看，根因是手绘椭圆环没有测地线：没有真正的阴影边界、
没有多次穿盘形成的上下双弧、没有被弯成弧线的背景。参考实现里这三样都是积分免费附带的。

### 暗色天幕：星尘就是背景

封面窗口仍使用透明窗口表面以适配场景容器，但黑洞场景从首帧起以近黑矩形兜底，并由 shader 输出全屏不透明画面。背景由程序化尘埃与两层星点组成；测地线将它们映射到弯折后的天空坐标，因此星点和尘埃会在阴影周围形成可见弧线。没有桌面截图、图像提供器或逐屏纹理。

阴影分支输出纯黑，外晕只在远场分支参与，避免重新照亮被捕获的中心像素。盘光通过 HDR 色调映射叠加到未被吞噬的天幕；较窄的环带外晕提供柔和过渡，暗角压低画面边缘。输出 alpha 恒为 1。

此前的桌面透镜实验及截图保留在 [validation/blackhole-desktop-lens](validation/blackhole-desktop-lens) 作为历史记录；当前实现不采集、不展示桌面内容，也不修改其他窗口状态。
### 入场：天幕先现，盘先铺开，黑洞随后长大

封面每次显示都演一遍"出生"：先是**近黑星空**，然后吸积盘先铺开、变亮，黑洞（阴影）在环内慢慢长大，落到稳态比例后开始随机游走。没有动物理，全是 QML 侧对既有 uniform 的时序：

| 阶段 | 区间 | uniform |
|---|---|---|
| 暗色天幕 | 0 – 0.8 s | `uHoleRadius` 0.0008（亚像素），透镜位移全域≈0 |
| 种子 | 0.8 – 1.4 s | 阴影 0.0008 → 0.02，盘外缘 5 → 11 r_s |
| 盘铺开 | 0.8 – 4.3 s | `uDiskOuter` 5 → 18、`uDiskInner` 1.8 → 3.6、`uGain` 0.06 → 1.1、`uTemp` 2900 → 4200 K |
| 黑洞成形 | 4.3 – 10.8 s | 阴影 0.02 → 0.125，盘外缘 18 → 8、内缘 → 1.8（回稳态比例） |

**"盘先变大"只能靠 `rout`**。屏幕上只有一个尺度 `W = B_CRIT / rh`：阴影视半径恒等于 `rh`，
盘的外缘是 `rout × rh / B_CRIT`，两者被同一个 `rh` 绑死，比值固定为 `rout / 2.598`。所以要让
盘先出现，就得让 `rout`（r_s 单位）在早期更大、随后收回 8——盘/阴影的屏幕比例从约 6.9:1
收到稳态的 3.1:1，看起来就是"环先铺开，黑点后长大"。`rout = 18` 会把 `Z0 = max(14, rout+5)`
抬到 23，但远场偏折式里的 `u = Z0/√(Z0²+b²)` 在这个范围内饱和到 ≈1，位移几乎不变；近场是精确
积分，只是路径长一点。

**随机游走放在 QML 里**。走路需要记忆，shader 里没有；QML 每 33 ms 走一步（布朗加速度 + 阻尼
`exp(-0.8dt)` + 半径 0.045 外的软回拉，位置限 ±0.08、速度限 0.04/秒），把偏移当 `uCenterX`/
`uCenterY` 交给 shader，`center = 0.5 + uCenter`。每块屏各有自己的场景实例，所以多屏天然不同步，
不需要再拿 `uSeed` 做相位偏移。

实测四帧与限制见 [validation/blackhole-intro](validation/blackhole-intro)。


## 11. 移植到 QML `ShaderEffect` 的具体改法

Qt 6 的 `ShaderEffect` 只接受 `qsb` 烘焙过的 `.qsb`（`qsb --qt6`，产出 GLSL 100 es/120/150
+ HLSL 50 + MSL 12 变体）。源码是 GLSL ES 1.00 子集，需要这些改写：

| 参考实现里的东西 | 移植处理 |
|---|---|
| `struct DiskLook` + `DiskLook[]` 预设数组 | 拆成 14 个 `property real`，预设交叉淡入改成 QML 侧两组属性 + `mix` uniform |
| `iChannel0` 纹理 | 换成 shader 内程序化生成的暗色星空；测地线映射后的天空坐标采样尘埃和星点，不需要外部纹理 |
| `iTime` | QML 属性 `property real t` + 30 fps `Timer`（`running: cover.shown`，隐藏即停；按真实经过时间累加，帧率不影响节奏；`shown` 变真时归零，入场因此每次重演） |
| `iResolution` | `property real uAspect`，由 `width / height` 推出（shader 只需要宽高比） |
| token 解码 / pomodoro / demo 巡回 | 删除，与插件无关 |
| `#define N_STEPS 48`、`B_CRIT` | 原样保留（编译期常量） |
| 全部物理函数（hash/vnoise/blackbody/测地线/盘面） | **原样保留**，并用 `hash`/`vnoise` 生成程序化尘埃与星点；没有外部桌面纹理 |
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
| 当前场景不透明输出 | Shader 从近黑天幕输出 alpha=1，QML 背景矩形覆盖 shader 初始化前的首帧 |
| 天空纹理映射 | 近场沿逃逸方向投影回天空平面采样程序化尘埃与星点；远场偏折与近场在交界处连续 |
| 入场只演一次，第二次 `boss blackhole` 直接是稳态 | `t` 只增不减，而场景 item 在 Loader 里跨多次显示存活。在 QML 里听 `cover.shownChanged`，`shown` 变真时把 `t` 与 `lastTick` 归零 |
| 入场第一拍就露出一颗黑点 | `rh` 的旧下限 0.004 在小屏上约 4 px。下限降到 0.0008（亚像素），暗色天幕保持完整 |

版权：源码注释里保留 Bruneton 与参考仓库的署名（BSD-3 / MIT）。

## 12. 落地形态与代价

结论：走 shader，进程整跑 D3D11，`memoryMB` 128 → 1024。

| 换来什么 | 付出什么 |
|---|---|
| 参考实现同款：真阴影边界、光子环、多普勒束流、多次穿盘形成的上下双弧，以及被弯成弧线的星尘与星点 | 进程不再零 GPU：所有场景都吃 D3D11 初始化；单屏 1080p 私有提交 106 MB（原软件路径单屏 ~27.5 MB） |
| 观感可调：色温、增益、倾角、转速都是 uniform，改一个数字就行 | `memoryMB` 是对已安装用户可见的 manifest 改动，1024 MB 已为多屏 swapchain 留足余量（双 2560×1440 参照早前记录约 210 MB） |
| 不采集桌面内容；不透明天幕从首帧遮住桌面 | 背景结构由 shader 程序化生成，极近黑区域本身没有可见纹理 |

被否掉的两条路：留在软件光栅重画 Canvas 版（视觉上限明显低一档，且 `boss blackhole` 在软件后端
根本不显示），以及参考仓库那种子进程渲染器（要复制窗口管理、Z 序、退出序列与进程生命周期，
复杂度最高，而常驻进程那点内存并不是瓶颈）。

剩下的工作在"调"而不在"造"：盘外缘的收边、以及帧率上限——现在动画由 30 fps 的 `Timer`
驱动，覆盖要挂几分钟，没必要满帧烧 GPU。

## 13. 分层动态星空与窗口化验收

`backdrop()` 的三层星点使用同一个场景时钟，不创建逐粒子 QML 对象。
远景、中景、近景分别在天空坐标中以 `(0.0010,-0.0005)`、
`(0.0030,-0.0017)`、`(0.0070,-0.0038)` 每秒平移；换算到 1080p，
速度约为 1.2、3.7、8.6 px/s，480×270 验证窗口中的速度为其四分之一。
每个网格单元有独立的闪烁相位与频率，强度因子为 `0.88 + 0.12*sin(...)`。
星点仍在透镜映射后的 `backdrop()` 中采样，捕获光线的黑色阴影不叠加粒子。
暗色天幕、吸积盘参数、入场曲线和正式运行时的随机游走保持不变。

验证只使用左上角 480×270 小窗，不全屏、不录视频。生产场景与固定中心对照
都在入场结束后，对同一个 `(8,8)-(88,232)` 物理像素矩形截取两帧，间隔
2.000 秒，原尺寸保存为 80×224 PNG。对照复用原场景 QML 和同一个已烘焙 shader，
仅固定中心与游走速度，排除黑洞移动改变透镜采样的影响。对照中 39 个匹配星点的
质心移动至少 0.3 px；例如 `(40.760,65.089)` 移至 `(39.165,66.022)`，
位移 `(-1.595,+0.933)` px，确认不是只有亮度变化。自动匹配用于位移佐证，
不据此反推每颗星的层级或精确速度。

同一安装 DLL 通过真实 strict runner 运行小窗，预热 12 秒后测量：

| 指标 | 实测 |
|---|---:|
| `frameSwapped` 平均频率 | 30.31 FPS |
| 稳态呈现事件间隔，中位数 / P95 / 最大值 | 31.15 / 62.15 / 63.05 ms |
| 超过 66.7 ms 的间隔 | 0 / 807 |
| CPU，20 秒平均，100% 表示占满一个逻辑核 | 0.470% |
| 工作集平均 / 最大值 | 77.10 / 77.13 MiB |
| 私有提交平均值 | 83.31 MiB |

这些是小窗的呈现事件间隔和进程资源数据，不是 GPU 执行时长，也不外推为
1080p 全屏或多屏性能。截图、匹配坐标和复测命令见
`validation/cover-input-sky/windowed-20261121/`。
