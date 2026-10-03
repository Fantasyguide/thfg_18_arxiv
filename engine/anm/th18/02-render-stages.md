# anm/th18 §2 — 渲染阶段 × 相机 × 层号（on_draw 优先级表）

> **版本**：TH18 v1.00a（`th18.exe`，imagebase `0x400000`）。本文裸地址默认属该版本；引用其他版本须写成 `th16:0x…`。
> 一手（2026-09-04）：`Supervisor__initialize` `0x453db0` 与 `AnmManager__initialize` `0x483820` 的 `register__on_draw` 调用 +
> 各桩的字节（`push 层号; call AnmManager__render_layer 0x488260`，大桩多一段切相机）。相机结构与矩阵见 §1、§3。

## 0. 结论

- 一帧的绘制由 UpdateFunc 注册表按**优先级**顺序跑；AnmManager 给每个层注册一个桩（`6A nn; E8 render_layer; C3`），
  Supervisor 在中间插切渲染目标 / 清屏 / 切相机的阶段函数。
- **相机归属**：层 0–2 → 相机 2；层 3–19 → 相机 3；层 20–23 → 相机 1；层 24 起（外围 UI，含 25–32 与 37–45）→ 相机 2（2D 偏移被清零）。
- **只有相机 2 的 2D 精灵偏移（`camera+0x104/+0x108` → `AnmManager+0xd8/+0xdc`）非零**（`Supervisor__sub_454f50`）。
  所以 §1 §3b 里「模式 8 放层 16 会少加区域原点而偏位」**不成立**——层 12–19 在相机 3 下，偏移本来就是 0。
  模式 8 与 2D 路径的偏移差异只影响相机 2 的层（0–2；24+ 时偏移已清零，也不影响）。
- 层 20–23（相机 1）的桩额外 `SetRenderState(ZWRITEENABLE, 0)` + `SetRenderState(ZFUNC, ALWAYS)`：UI 层不写不测深度。
- 所有相机都是 `D3DXMatrixLookAtLH` + `D3DXMatrixPerspectiveFovLH`（§3）；exe 无 Ortho。

## 1. 表

| 优先级 | 谁 | 做什么 | 相机 |
| --- | --- | --- | --- |
| 1 | Supervisor `0x4553b0` | 清后备缓冲；RT → surface 0；用相机 3 的视口清；设 `originMode(1)` 原点 `DAT_0056acbc/acc0`；`set_camera(2)` | 2 |
| 5 / 7 / 9 | Anm `0x487670/80/90` | 层 0 / 1 / 2（3D 背景） | 2 |
| 0xa | Anm `0x4876b0` | **切相机 3**（写 `current_camera`、`SetViewport`、复制 2D 偏移）；层 3 | 3 |
| 0xb / 0xd | Anm | 层 4 / 5 | 3 |
| 0xe | Supervisor `0x455610` | 一串渲染状态；RT → **surface 1**；用相机 3 视口清 | 3 |
| 0xf | Supervisor `0x455530` | `set_camera(3)`（`0x4555a8`）+ 画 `arcade_vm`（把 surface 0 的游戏区域拷过来） | 3 |
| 0x10 / 0x12 / 0x14 / 0x15 / 0x16 / 0x18 | Anm | 层 6 / 7 / 8 / 9 / 10 / 11（敌机等） | 3 |
| 0x19 | Supervisor `0x455a70` | RT → **surface 0**；用相机 3 视口清 | 3 |
| 0x1a | Supervisor `0x4559a0` | `set_camera(3)`（`0x455a18`）+ 拷回 | 3 |
| 0x1b / 0x1c / 0x1f / 0x20 / 0x23 / 0x25 / 0x28 / 0x2b | Anm | 层 12 / 13 / 14 / 15 / 16 / 17 / 18 / 19（道具、激光、子弹、自机穿插其间由各自管理器画） | 3 |
| 0x2c | Supervisor `0x455bc0` | RT → surface 1；用**相机 1** 视口清；设 `originMode(2)` 相关 `DAT_0056acb0/acac` | — |
| 0x2d | Supervisor `0x455b10` | 渲染状态 | — |
| 0x2e | Anm `0x487940` | **切相机 1**；`ZWRITEENABLE=0`、`ZFUNC=ALWAYS`；层 **20** | 1 |
| 0x2f / 0x36 | Anm | 层 21 / 22 | 1 |
| 0x38 | Anm `0x487a10` | 切相机 1；层 23 | 1 |
| 0x3a / 0x3b | Supervisor `0x455cf0` / `0x455c90` | RT → **后备缓冲** `th18:0x4ccfa4`（`th18:0x455d0b`）+ **Clear 整面成 `0xff000000`**（`th18:0x455d2e`）/ 画「屏幕四边形」`th18:0x4ccfb8` 把离屏面搬回来（`th18:0x455cb7`）| — |
| 0x3c | Anm `0x487ae0` | **切相机 2**，并把 `AnmManager+0xd0/+0xd4` 清零；**组 24 = 世界列表 layer 24**（`th18:0x487ba0 push 0x18`，**不是** UI layer 24）；RT = 后备缓冲（`0x3a` 已切回）| 2 |
| 0x3d / 0x3e / 0x40 / 0x41 / 0x42 / 0x43 | Anm | 层 25 / 38 / 26 / 39 / 27 / 40 | 2 |
| 0x44 / 0x45 / 0x46 / 0x47 | Anm `0x487db0/bc0/e40/c50` | 层 41 / 28 / 41 或 42（🟡 字节未逐条对）/ 29，各自随后再切相机 2 | 2 |
| 0x39 | Anm `0x487ce0` | 切相机 2；**组 37 = UI 列表 layer 24**（`th18:0x487d99 push 0x25`）；RT = **surface 1**（`0x2c` 切的，中间没人切回来）| 2 |
| 0x54 / 0x55 / 0x56 / 0x57 / 0x59 / 0x5a | Anm | 层 30 / 43 / 31 / 44 / 32 / 45 | 2 |
| 0x5c | Supervisor `0x455d40` | 收尾 | — |

`render_layer` `0x488260` 的层号折叠：world 列表 VM 层 37–45 → 减 13 后比对；UI 列表 VM 层 24–32 → 加 13，不在 37–45 内的一律当 39。
即 **UI 列表的 VM 实际都画在 37–45 那组桩里**，world 列表的按原层号。

> **★ 2026-09-xx 复核订正 + RT 实值**（一手过程与证据见 [05-snapshot-and-surfaces.md](05-snapshot-and-surfaces.md)；本表按优先级升序）：
> 1. 本表的「RT → surface 0 / surface 1」实体已查明：**surface 0 = `th18:0x4ccf9c`（supervisor+0x1ac）**、
>    **surface 1 = `th18:0x4ccfa0`（supervisor+0x1b0）**，两者都是 **`text.anm` 第 12 / 第 18 张贴图的 level-0 面**
>    （`th18:0x455ef6`–`th18:0x455f29` 的 `GetSurfaceLevel(0)` 写入），**都不是后备缓冲**；
>    后备缓冲是 `th18:0x4ccfa4`（supervisor+0x1b4，`th18:0x453f21` 的 `GetBackBuffer(0,0,…)` 写的）。
> 2. **`0x39` 与 `0x3c` 两行的「层 N」不是同一张列表的层号**：`0x39` 压 `37`（组 37 ⇒ **UI 列表 layer 24**）、
>    `0x3c` 压 `24`（组 24 ⇒ **世界列表 layer 24**）。即「UI layer 24」归 `0x39` 那一趟，
>    `0x3c` 那一趟只吃世界列表 layer 24（原文按「层 24」并排容易读错，`0x25`/`0x18` 都是压栈立即数）。
> 3. `0x3a` 不是「顺手回个 RT」：它先 `SetRenderTarget(0, 后备缓冲)` 再 **Clear 整面成 `0xff000000`**，
>    屏幕内容靠 `0x3b` 的合成四边形搬回来 ⇒ **`0x3a` 是「进不进得了 P 键截图」的分水岭**
>    （`0x39` 那一侧画进 surface 1，`0x3e` 起这一侧直接画进后备缓冲）。

## 2. 相机（`zSupervisor+0x25c` 起四台，各 0x164；`FUN_00454b20` `0x454b20` 初始化）

| 相机 | 视口（`+0xe0..0xec`，X,Y,W,H） | 2D 偏移 `+0x104/+0x108` | 用途 |
| --- | --- | --- | --- |
| 0 | `scale × (32, 16, 384, 448)` | 0 | 640×480 布局里的弹幕区（`FUN_00454b20` 先配它，再拷给 1/2/3 当底） |
| 1 | `scale × (128, 16, 384, 448)` | 0 | 层 20–23（游戏区域内全分辨率 UI；X = 128 而非 32，surface 1 的布局与窗口不同 🟡） |
| 2 | `(0, 0, DAT_0056ac80, DAT_0056ac84)`（整张表面） | `((DAT_0056ac98 − ac80)/2, (ac9c − ac84)/2)`，`Supervisor__sub_454f50` | 3D 背景层 0–2；外围 UI 层 24+（偏移被 `0x487ae0` 清零） |
| 3 | `((ac80 − 408)/2, (ac84 − 472)/2, 408, 472)` = 居中的 **408×472** | 0 | 游戏区域层 3–19 |

`scale` = `DAT_0056aca0`（分辨率倍率）。常量一手（`0x4b9088` 起的只读浮点表）：`0x4b9288` = 16、`0x4b92b4` = 32、`0x4b9304` = 100、
`0x4b9310` = 128、`0x4b9354` = 256、`0x4b9384` = 384、`0x4b9390` = 408、`0x4b939c` = 448、`0x4b93a4` = 472、`0x4b9138` = 0.5、`0x4b9178` = 1.0、`0x4b9088` = 1/256。

## 3. 矩阵（`FUN_00454950` `0x454950`，四台共用）

```
cx = X + W/2 ; cy = Y + H/2                      // 视口中心
eye = (cx, cy, -(H/2) / (fov/2)) ; at = (cx, cy, 0) ; up = (0, -1, 0)   // y 朝下
D3DXMatrixLookAtLH(view = camera+0x60, eye, at, up)
D3DXMatrixPerspectiveFovLH(proj = camera+0xa0, fov = camera+0x54, W/H, near 1.0, far 10000.0)
```

眼距按 `(H/2)/(fov/2)` 取（🟡 没看到 tan，`fov` 可能已是 tan 半角或近似），使 z = 0 平面 1:1 映到视口像素；
z > 0 远、z < 0 近——`type(8)` 绕 Y 转有近大远小，实跑 ✅。

## 4. 对 mod 的意义

- 场上特效放层 20（相机 1）：不写不测深度、坐标原点由 `layer(20)` 设默认；`pos(0, y, 0)` 里 y 从区域顶部算。
- 放层 12–19（相机 3）：与子弹同一台相机，理论上也行（本轮没验，第一版是 anm 拿错导致空白，不是层的问题）。
- 模式 8 的顶点缓冲（`FUN_00485110` `0x485110`，`CreateVertexBuffer(0x2d0, FVF 0x102)` = 36 顶点 = 9 块四边形，每顶点 0x14：x,y,z,u,v）：
  基础四边形四角 (−128,−128,0,0,0) (128,−128,0,1,0) (−128,128,0,0,1) (128,128,0,1,1)——**居中**；之后按 y +128 / −256、x 同样平移出 9 块
  （[−128,128] → [0,256] 左/上锚 → [−256,0] 右/下锚，与 2D 的 anchor 语义一致），`DrawPrimitive` 起始 = `(v*3 + h) * 4`，`anchor(0,0)` = 第 0 块 = 居中。
  `set_sprite` 把 `w/256`、`h/256` 写进 `vm+0x3d4/+0x3e8` 当矩阵缩放 → ±128 × w/256 = ±w/2 ✓。
  → 几何上模式 8 的锚点与 2D 一致；实跑却观察到**顶边对齐**（层 20、`pos(0,224)` 时卡顶在场地中点），原因未找到 🟡，
  现用 `pos` y 上移半张卡高补偿。可疑点：`layer(20)` 设的默认 originMode 2 原点 `DAT_0056acb8 = scale × 0x4b9288` 与相机 1 视口 Y 的关系。

## 6. 图元族的操作码与**空间渐变**（2026-10-03 补，实装卡 92 的「威光」时钉下）

`600..613` 是 TH16 时代就有的图元族（[`../th16/09-drawing.md`](../th16/09-drawing.md)），TH18 的
`ability.anm` 里还能看到**更高的两条**：**614 / 615**。反编译器（`thanm -d`，本仓
`native/build/tex-ability/spec.txt` 就是它的输出）**不认识这两条**，会原样印成 `ins_614(...)`。

| opcode | 反编译器名字 | 见过的原生用法 | 备注 |
| :-: | --- | --- | --- |
| 604 | `drawPoly` | `drawPoly(1.0f, 32)` / `(1.0f, 3)` | 实心多边形（半径 1.0、段数）；`scale` 就是半径 |
| 605 | `drawPolyBorder` | `drawPolyBorder(112.0f, 64)` | 同上，只画边 |
| 611 | `drawRing` | `drawRing(96.0f, 2.0f, 64)`（半径/厚度/段数）| **恒整圈**（见 `engine/card/th18/14-clear-range-barrier.md`）|
| 612 | `drawRectBorder` | — | TH18 一用就崩（实测 C0000005）|
| **614** | **（无名）** | **`ability.anm` script48 `ins_614(640.0f, 1.0f)`** | **扇形/光扇图元**（`CardMiko` 威光的每一片）。本仓工具链**能直接汇编** `ins_614(640.0f, 1.0f)`（2026-10-03 实测通过）|
| 615 | （无名）| `ability.anm` 里 1 处 | 未查 |

**⚠ `scale` 在两族里的语义不同**（2026-10-03 实装踩坑）：**sprite** 的 `scale` 乘的是**贴图矩形自己的
像素尺寸**（`assets/ability/entries/ORDER.txt` 的注：`[vm+0x74] × [vm+0x54]`）⇒ 128×128 的贴图写
`scale = 180` 就是 23000 px（屏幕上只剩渐变最中心那块**平的** ✗）；而**图元**（`drawPoly`/`drawRing`）
的 `scale` 是**几何半径**。⇒ 想让贴图铺满游玩区（≈600 px），scale 只要 **≈4.7**；图元才写 600。

**空间渐变 = 两路顶点色 + `colorMode`（缺一不可！）**：图元按顶点栅格化（§3 的 `DrawPrimitive`
起始 `(v*3 + h) * 4`），VM 有**两路**顶点色寄存器（`color()` → `color_1`、`color2()` → `color_2`；
`alpha()`/`alpha2()` 同理），但**默认模式只平涂 `color_1`、`color2()` 完全不参与** ——
必须显式设 **`colorMode`**（opcode 423 的 bits 17–18）才会出现渐变：

| `colorMode` | bits | 四角色 | 含义 |
| :-: | :-: | --- | --- |
| **0（默认）** | `00` | c1 c1 c1 c1 | 平涂 `color_1`（`color2()` **无效**）|
| 1 | `01` | c2 c2 c2 c2 | 平涂 `color_2` |
| **2** | `10` | c1 c2 c1 c2 | **水平渐变**（左 c1 → 右 c2）|
| **3** | `11` | c1 c1 c2 c2 | **垂直渐变**（上 c1 → 下 c2）|

一手：TH16 [`10-sprite-geometry.md`](../th16/10-sprite-geometry.md) §5（引擎 4 项跳表 `0x465a14` 逐字节直读）；
TH18 侧的写入点同族（`07-blendmode-rgb-only-alpha-is-sa.md` §2 的两路顶点色寄存器）。
⚠ 实战教训（2026-10-03，卡 92 的威光）：**忘了设 `colorMode` 就会「看起来完全没有渐变」** ✗ ——
`color()`/`color2()` 都写了也没用。
原生 script48 就是靠这个把一片扇画成「根部蓝紫 → 外缘品红」（`color(128,128,255)` +
`color2(255,32,255)`，再叠 `alpha2(16)` 让外缘淡出）。**想做任何渐变图元，都走这两路色，不必用贴图。**
