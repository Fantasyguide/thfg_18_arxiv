# boss-bg-warp —— TH18「boss 背景扭曲」**顶点效果验证 mod**（临时独立 DLL）

> **版本**：TH18 v1.00a（`th18.exe`，imagebase `0x400000`，md5 `9969cac756098c1da05a81de45437a70`）。本文裸地址默认属该版本；引用其他版本须写成 `th16:0x…`。
>
> **状态**：✅ 已编译、已过静态门禁（`make check` / `make verify`）；**未实跑**（要人跑一局带 boss 的非符）。
> **缘起**：`../card-expand/HANDOFF-109-BOSS-BG-WARP.md` §5 的 **WP-C「真·局域背景扭曲」**。工单要求「必须单独一人到底」，
> 而 card-expand 那支 DLL 背着 35 个站点 + 卡表搬迁 + 共享件 —— 顶点设施一失败就分不清是谁的锅。
> 所以**先把顶点这一层单独拆出来**：这支 DLL 零 patch、零站点、零共享件，装/卸就是放/拿一个文件。

## 0. 三句话

1. **原版那个扭曲确实是「一份网格的顶点」**：`stage+0x3410` 上有一份 17×17 的网格（ctor `0x42b1f0`），
   每帧由舞台 tick `0x41e760` 重算顶点；顶点经 `FUN_0042b540` 拷进 **16 台 zAnmVm 的 `+0x5c8`**
   （`special_vertex_buffer_data`），再由 `AnmManager__draw_vm` 推给 D3D（`0x481643`／`0x48166d`／`0x481697`）。
   ⇒ 工单 §5.4 最后一行那个「mesh 怎么上屏」**未核实项，本 DLL 落地时坐实了**（见 §2）。
2. ★ **静态读出一个反直觉的顺序**：同一帧里 `0x42b3e0`（建网格 **并上传**）先跑，
   `0x41e790` 起的「按半径/相位逐顶点重算」**后**跑 ⇒ 重算写进 `mesh+0x14` 时，
   它已经被拷进 VM 缓冲了。**这份重算到底有没有上屏，静态读不出，必须实跑看** —— 这正是本 DLL 的第 ① 项验收。
3. **能驱动**：hook ① 就挂在那个「上传」函数的入口 —— 在引擎拷贝**之前**改 `mesh+0x14`，
   引擎会照单全收。所以「我们自己造这个扭曲」不需要任何新渲染设施，只要有这个 hook 点。

## 1. 它验什么（对应工单 §5.5 的 M1/M2）

| # | 问题 | 本 DLL 怎么回答 | 判据 |
| --- | --- | --- | --- |
| Q1 | 那份网格**有没有真上屏** | 上传那一刻打顶点统计（alpha 直方图 + 步长是否规则） | 出现 `60/00` = 重算已上屏；全是 `ff` = 拿到的是刚建好的规则网格 |
| Q2 | 引擎的逐顶点重算**写在哪一档** | hook ② 在 `[mode==2]` 循环出口再打一次同样的统计 | 两条日志一对比，Q1 定案 |
| Q3 | **我们自己能不能驱动** | hook ① 里按一手公式注入（模式 2/3），引擎照单拷贝 | 画面上出现圆形边界的扭曲；模式 1（恒等）画面逐像素不变 |
| Q4 | 半径/中心/幅度的**真实取值** | hook ② 打出 `stage+0x3414/0x3418/0x3434/0x3438/0x343c` | 直接读到 112.0、当前半径、相位与模式 |

## 2. 一手链路（每条都本机核过字节；地址表见 [`TARGET.md`](TARGET.md)）

```text
StageInner::run_std  0x41d260
  └─ 0x41dac2  call 0x42b1f0        ; 建网格（17 段 / 每段一台 VM / 贴图经 0x455d80 解析）
     └─ 0x42b275 FUN_00455d80(&out, 2) → sprite id，再按 id 取 zAnmVm
  └─ 0x41dace  stage+0x3410 = mesh

StageInner::on_tick_part_2  0x41e760        ; 每帧
  ├─ 0x41e7bd  test BYTE [0x4cf2c0]+0x78, 1  ; 符卡进行中？（bit0）
  ├─ 非符 ⇒ 0x41e7ab 按 stage+0x343c 分派
  │    ├─ ==1 → 0x41e7c7  建 17×17 平面网格（常数 -192/320/384/128）
  │    └─ ==2 → 0x41ead4  建「圆盘」网格（半径 r = stage+0x3418，中心 224/240）
  │    └─ 0x42b3e0 FUN_0042b3e0(mesh, xmm1, xmm2, xmm3, 栈参)  ; 建顶点 + 算 UV/颜色
  │         └─ 0x42b529  call 0x42b540                          ; ★ 立刻上传
  │              └─ 逐段：mesh+0x14 → [vm+0x5c8]（17 列 × 2 行 = 一条三角带）
  └─ ★ 上传之后才做「逐顶点重算」
       ├─ mode 1：0x41e870 起，内部点位移（sin 波，相位绕 2π 回卷，边界点不动）
       └─ mode 2：0x41eb4a 起，圆内 t=(R²-r²)/R² 拉 + sin 波；圆外 alpha=0（`0x41ed5a`）
```

**顶点形态**（一手 `0x42b480`–`0x42b4eb`）：`{x, y, z, w=1.0f, D3DCOLOR, u, v}` = 7 dword = **28 字节**。
`+0x13` 是 D3DCOLOR 的 alpha 字节，mode 2 写 `0x60`（圆内）/ `0x00`（圆外）—— 这是 Q1 的判据来源。

**每段一条三角带**（一手 `0x42b590`–`0x42b5b6`）：第 k 段 VM 拿第 k 行与第 k+1 行，共 17×2 = 34 个顶点；
17 行 ⇒ 16 段 ⇒ 网格就是那 16 台 VM。

**D3D 设备指针**（顺带坐实，别的工单用得上）：`[0x4ccdf8]` = `IDirect3DDevice9*`，
`DrawPrimitiveUP` = vtable `+0x14c`（全 exe 16 处调用点，例：渲染阶段 `0x455866`、`AnmManager__flush_sprites` `0x47e7d9`）。

## 3. 装法（没有 patch、没有 JSON、没有站点）

```text
<thcrap>/bin/th18_bgwarp.dll        ← 只有这一个文件
```

thcrap 会扫 `bin\*.dll` 逐个 `LoadLibrary` 找 `thcrap_plugin_init`（一手：thcrap `thcrap/src/plugin.cpp:126`），
所以**不用改 `enabled.json`、不用进 patch 栈**。modkit 的启动器只清 `bin\mod_*.dll`（前缀 `mod_`），
本文件无前缀 ⇒ 不会被它删。放好后照常用 thcrap 启动 TH18。

配置与日志都落在 **th18.exe 同目录**：

```ini
; th18_bgwarp.ini（不写就用默认值）
[warp]
mode = 0            ; 0=只看 1=恒等 2=复刻旋涡 3=放大旋涡
radius = 112.0      ; = 一手 stage+0x3414
cx = 224.0          ; = 一手 0x4b9348（网格自身空间的圆心）
cy = 240.0          ; = 一手 0x4b934c
amp = 1.0           ; ❓ 一手 [stage+0x3438] 的初值是运行期值，本 DLL 会打出来
amp_mul = 1.0       ; 模式 3 的放大倍数
hide_outside = 1     ; 一手：圆外 alpha=0
log_every = 64      ; 每 N 次上传记一行（0 = 只记第一行）
```

**热键**（`GetAsyncKeyState`，不进游戏输入）：`F5` 循环模式、`F6` 转储顶点、`F7` 切「圆外 alpha=0」。

## 4. 判读（这份只能人跑，然后照下表读日志）

先看 thcrap 日志有没有 `[Plugin] th18_bgwarp.dll: initialized and active`；
是 `not used for this game` ⇒ 签名没过（看 `th18_bgwarp.log` 里那句 `★ 签名不匹配`）。

| # | 看什么 | 通过 | 不通过说明什么 |
| --- | --- | --- | --- |
| 1 | 第一条 `★ hook① 首次命中` 的 `来自舞台 build_grid` | 命中，且 `n=17 m=17 VMs=16(有顶点缓冲 16)` | 没命中 ⇒ 这一场没有这份网格（或还没进 boss 战） |
| 2 | `上传前` 那行的 alpha 直方图 | 全是 `ff` ⇒ Q1 = **重算没上屏**（静态推断成立） | 出现 `60/00` ⇒ 重算确实进了 VM 缓冲，工单 §9.24 的结论成立 |
| 3 | `★ hook② 首次命中` + `重算后` 那行 | 与第 2 条相反：这里应出现 `60/00` | hook② 不响 ⇒ 那一场走的是 `mode==1`（波浪），不是旋涡 |
| 4 | 按 `F5` 到模式 2 / 3 | 画面出现**圆形边界**的扭曲，圆外不变 | 圆内也没变 ⇒ 拷进 VM 的不是 `mesh+0x14`（我们的模型错了，回 §2 复核） |
| 5 | 按 `F5` 到模式 1（恒等） | 画面与基线**逐像素一致** | 有任何变化 ⇒ 我们动了不该动的东西 |

★ 第 2/3 两条是本 DLL 存在的主要理由：它把「工单说机制已钉死」变成**活进程上的两行日志**。

## 5. 与 109 / WP-C 的关系

* **能不能自己做**：能。要的不是「新渲染设施」，而是**一个 hook 点 + 一份网格 + 一段映射数学**。
  本 DLL 已经证明前两件都在引擎里现成（网格由 `run_std` 建、映射数学在 `0x41eb4a` 起）。
* **能不能照抄**：映射那一支的公式已在 §2 记成一手（含常量 `224/240`、`×8`、相位步长 `π/2` 与 `0.698`）。
  本 DLL 的模式 2 就是它的直译；**`amp` 的运行时初值**是唯一没静态定下来的量（DLL 会打印它）。
* **109 的差别**：原版这个效果只挂在**舞台**的网格上（boss 战非符期间），玩家用卡的场景没有舞台网格。
  ⇒ 要么另建一份网格（ctor `0x42b1f0` 可直接调），要么复用舞台那份（仅在有舞台的场合）。
  这是 M3 的设计题，不在本验证 DLL 的范围。
* **不要**走「敌人 ECL-629 那份 mesh」：那份在 TH18 里**建了不画**（一手：`enemy+0x558c` 全 exe 只有 3 处引用，
  全在 opcode 629 的 case 里）。本 DLL 的 `touch_enemy_mesh` 默认关。

## 6. 目录

```text
boss-bg-warp/
├── README.md                  # 你在这
├── TARGET.md                  # ★ 死绑登记：hook 站点 / 读点 / 换 build 要重取什么
├── AUDIT.md                   # 对抗审计记录
└── native/
    ├── th18_bgwarp.c          # 全部逻辑（单文件，只依赖 kernel32 + msvcrt + user32）
    ├── th18_bgwarp.def        # 导出无装饰的 thcrap_plugin_init
    ├── Makefile               # make / make check / make verify
    └── check_constants.py     # 死绑量 ↔ 真 exe 对账（含 hook 窗口的指令边界）
```

相关：工单 [`../card-expand/HANDOFF-109-BOSS-BG-WARP.md`](../card-expand/HANDOFF-109-BOSS-BG-WARP.md)、
一手全过程 [`../../../engine/enemy/th18/07-boss-aura-mesh.md`](../../../engine/enemy/th18/07-boss-aura-mesh.md)、
渲染阶段表 [`../../../engine/anm/th18/02-render-stages.md`](../../../engine/anm/th18/02-render-stages.md)、
同类独立探针 [`../runtime-probe/README.md`](../runtime-probe/README.md)、
手写机器码的审计清单 [`../../_template/AUDIT-checklist.md`](../../_template/AUDIT-checklist.md)。
