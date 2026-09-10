# 计划：ANM 播放器 层 B（渲染后端）+ 宿主层

> **版本**：跨版本。本文出现的地址一律带版本前缀（如 `th16:0x465c40`）。
>
> **上游**：设计文档 [`../specs/2026-09-09-runtime-anm-design.md`](../specs/2026-09-09-runtime-anm-design.md)
> （§10.1 的层 A / 层 B 切分、§10.3 的 D3D9→WebGL 映射、§10.6 的在线播放器）；
> 第一阶段计划 [`2026-09-09-runtime-anm-phase1.md`](2026-09-09-runtime-anm-phase1.md)。
>
> **写给谁**：可能跨多个会话执行。每条任务写「做什么 + 验收标准 + 已知的坑」，
> **不写抄代码级别的步骤**——执行者要么是我自己，要么是能读 `engine/anm/th16/` 的 subagent。

## 0. 现在站在哪

**层 A（纯状态机）已收口**：opcode 135/135 全实现，`runtime/anm/` 210 个测试全绿。
VM 能正确跑完一个脚本、维护所有字段与插值器，但**它不知道怎么把状态变成像素**。

**engine 侧为层 B 准备好的一手材料**（都已 ✅，可直接照抄，不需要再开 Ghidra）：

| 需要什么 | 在哪 | 状态 |
| --- | --- | :---: |
| sprite 四边形的顶点位置 / UV / 颜色（mode 0/1/2/3） | [`10-sprite-geometry.md`](../../../engine/anm/th16/10-sprite-geometry.md) | ✅ |
| 帧尾几何三族（mode 9 圆环、13/14 弧、24/25 3D） | [`09-drawing.md`](../../../engine/anm/th16/09-drawing.md) §3.1–§3.5 | ✅ |
| blend 10 模式 / alpha test / texture stage / FVF / stride | `09` §4 | ✅ |
| 贴图格式表（bpp × D3DFORMAT × 9 项） | [`01-format.md`](../../../engine/anm/th16/01-format.md) §6/§7 | ✅ |
| `zAnmVm` 字段图（顶点计算读的每个偏移） | [`02-vm-core.md`](../../../engine/anm/th16/02-vm-core.md) §3.3 | ✅ |

**仍缺的一手**（层 B 会撞上，按需再逆）：`draw_vm` 的 **mode 4 / 8 / 15**（走 `th16:0x467410`，
mode 8 真实用量 196 次）、**rect 族**（`th16:0x468722`）与 **poly 族**（`th16:0x4689b3`）的顶点计算。

## 1. 这个阶段做什么 / 不做什么

**做**：让 `runtime/anm/` 能把一个 `.anm` 文件在浏览器里**放出来**——解贴图、建 WebGL 后端、
补一层最小宿主（VM 列表 + layer 排序 + 帧循环）、一个能拖文件进去看的页面。

**不做**（明确排除，免得范围蔓延）：

- **不追求逐像素等同真机**。目标仍是设计文档 D1 的 A 保真度（「能跑看着像」）。
- **不做 ECL / 不做游戏本体**。播放器只放 ANM。
- **不进版权字节**：仓库里不放任何 ZUN 的 `.anm` / 贴图；页面靠用户自己上传
  （设计文档 §10.6 已定此路线）。测试夹具一律自造。
- **不做编辑器**（写 ANM 脚本、导出 `.anm`）——那是更后面的事。

## 2. 任务分解

依赖关系：**B1 → B2 → B3 → B4**，B5 可与 B3/B4 并行，B6 随时可插。

### B1 · THTX 贴图解码（format → RGBA8）

**做什么**：`runtime/anm/src/format/` 里加贴图解码，把 THTX 块解成 `RGBA8` 的 `Uint8Array`
（浏览器能直接 `texImage2D` 的布局）。`parseAnm` 现在只校验 THTX magic、不吐像素，需要扩契约。

**已知的坑**（都已一手查过，别再踩）：

1. **行距用的是 THTX 自己的 `format`**（`th16:0x46cccb`），**不是 `entry.format`**——
   两者在 54 个真实文件里有 1 处不同（`enemy.anm` 的 `enemy/enemy_aura.png`：
   THTX 源是 format 1、entry 目标是 format 6）。**用错了这一个 entry 会整块错位。**
2. **thanm 只实现了 4 个 format**（1/3/5/7），但**这不构成缺口**：真实 TH16 数据里
   `thtx.format` 只出现 1/3/5/7，format 6 仅作为**目标**格式出现那一次。
   ⇒ 抄 `local/vendor/thtk/thanm/image.c` 的 `format_to_rgba()` 即可覆盖 100% 真实数据。
   其余 format（0/2/4/6/8）**显式抛**，不要静默当某个已知格式处理。
3. **thanm 的位扩展要照抄**：`ARGB4444` 是 `0x3 -> 0x33`（复制半字节，不是 `<<4`）、
   `RGB565` 逐位复制高位补低位。直接 `<<4` / `<<3` 会在纯白处差 1，累积到 blend 上肉眼可见。
4. **`format 7` 的语义分歧要如实处理**：`01` §7 一手证明它是 `D3DFMT_A8`（**只有 alpha**），
   而 thanm 记作 `GRAY8` 并解成「亮度 + 不透明」。**两者不等价。**
   ⚠️ 但实测：39 个 format-7 entry **全部是同一个 `face/dummy.png` 占位图**，
   所以这条分歧在 TH16 **不影响任何真实画面**。实现上选一个并**在代码里写清楚选了哪个、为什么**。
5. **`entry.format` 的降位转换不建模**：游戏加载时会把源像素转成 entry 的目标格式
   （如 A8R8G8B8 → A4R4G4B4 会掉到 4 位/通道）。播放器直接用源精度 ⇒ 比真机**更准**。
   这是 A 保真度下的有意偏离，**在 OVERVIEW 里记一条**，不要假装没有。

**验收**：解出的 RGBA 能与 `thanm -x` 吐的 PNG 对上（若本地能跑 thanm）；否则至少
「同一张图用 format 1 与 format 5 两条路径解出的像素，在 4 位精度内一致」这类自洽检查 +
真实文件全量跑通不抛（423 个 entry）。

### B2 · WebGL 后端（`src/render/webgl.js` + `D3D9-MAPPING.md`）

**做什么**：把 `09` §4 的 render state 映射成 WebGL 状态 + 一对 shader，提供
「提交一批四边形」的接口。**只负责画，不认识 VM**。

**要点**：

- **blend 10 模式**照 `09` §4.2 的表建 `blendFunc`/`blendEquation`；
  ⚠️ **只有 mode 3（Replace）关 alpha test**，其余 9 种都开。
- **alpha test 不是 no-op**：`ALPHAREF=1`、`GREATEREQUAL` ⇒ fragment shader 里
  `if (a < 1.0/255.0) discard;`，且只在 blend mode ≠ 3 时。
- **texture stage 就是一次乘法**：`tex * (DIFFUSE 或 TFACTOR)`，`COLORARG1` 恒为纹理。
  别照「D3D9 固定管线默认」多写一层。
- **`ZFUNC = ALWAYS`**（`09` §4.1）——深度测试恒过；rect 族还会关 `ZWRITEENABLE`。
- **两种坐标空间**：sprite 族与 mode 9/13/14 提交的是**已经算好的屏幕/世界坐标**
  （FVF 带 `XYZRHW`），mode 24/25 提交的是**模型坐标**、要走 WORLD 矩阵（`09` §3.4）。
  后端接口必须能区分这两类，不能都当同一种。
- `D3D9-MAPPING.md` 与代码同写：每条 WebGL 状态指回 `09` §4 的哪一行。

**验收**：离屏渲染一个已知四边形，读回像素断言颜色/混合结果；10 种 blend 各一条。

### B3 · 几何生成（`src/render/geometry.js`）

**做什么**：把 `10` §0 的 sprite 四边形公式与 `09` §3.2–§3.4 的三族公式实现成
「vm 状态 → 顶点数组」的纯函数。**纯计算、不碰 WebGL**，因此可以纯 Node 测试。

**要点**：锚点两张表照抄；`uv_scale` 的收缩**恒以 TL 为锚**；`423 colorMode` 四种平涂/渐变；
角度每轮归一（34 步上限）；`th16:0x464dd0` 的有效 z 旋转直接复用 `core.js` 已有的实现。

**验收**：这是本阶段**最该上数值断言**的地方——锚点九宫格、旋转 90°/180° 的角点、
`uv_scale=0.5` 的 UV、四种 colorMode 的四角颜色，逐条断言；配合故意破坏验证。

### B4 · 最小宿主（`src/host/`）

**做什么**：补上骨架故意没做的 AnmManager——VM 列表、layer 排序、每帧 `stepFrame` +
`stepInterpolators` + 几何生成 + 提交、以及 `500–506` 建出来的子 VM 的注册与销毁回收。

**要点**：`08` §5 的子链 prepend 顺序、`th16:0x46e660` 的「子先于己」递归销毁与待销毁链、
`02` §3.5 的 world/UI 两张列表、`304 layer` 的层区间派生位。
**`group-child.js` 已经把 `vm._child` 的树结构与 `destroyTree` 准备好了**，宿主只需接线。

### B5 · viewer 页面（`web/`）

拖入 `.anm` → 选 entry/script → 播放/暂停/单步/时间轴 → 显示当前 VM 状态（变量、flags、pc）。
兼作调试器：设计文档 §10.6 已把它定位成「层 B 遮住层 A 的错」这个风险的对冲。

### B6 · 补逆 `draw_vm` 的剩余路径

`mode 4 / 8 / 15`（`th16:0x467410`，mode 8 用量 196 次）、rect 族（`th16:0x468722`）、
poly 族（`th16:0x4689b3`）。**按需插入**——B3 撞到哪个再逆哪个，不必一次逆完。

## 3. 执行状态

| 任务 | 状态 | commit |
| --- | :---: | --- |
| B1 THTX 解码 | ✅ 已完成 | 见下方 commit；真实数据 417 entry / 89.7M 像素全通 |
| B2 WebGL 后端 | ✅ 已完成 | 见 commit；含 D3D9-MAPPING.md |
| B3 几何生成 | ✅ 已完成 | 见 commit；顺带补 apply_velocities 与帧尾时序 |
| B4 最小宿主 | 未开始 | — |
| B5 viewer 页面 | 未开始 | — |
| B6 补逆 draw_vm | 未开始 | — |

## 关联

- 设计文档：[`../specs/2026-09-09-runtime-anm-design.md`](../specs/2026-09-09-runtime-anm-design.md)
- 第一阶段（层 A）计划与执行状态：[`2026-09-09-runtime-anm-phase1.md`](2026-09-09-runtime-anm-phase1.md)
- 六篇审阅发现与处置：[`2026-09-09-anm-review-findings.md`](2026-09-09-anm-review-findings.md)
- 实现状态矩阵：[`../../../runtime/anm/OVERVIEW.md`](../../../runtime/anm/OVERVIEW.md)
