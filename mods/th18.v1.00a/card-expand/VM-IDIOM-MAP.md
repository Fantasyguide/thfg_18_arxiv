# VM-IDIOM-MAP —— 「直写 VM 字段」的意图普查（P14 / W2）

> **版本**：TH18 v1.00a（mod 侧 `th18:0x400000`）；文中跨作地址一律带 `thNN:` 前缀 ✓。
> 工单 **P14**（见 [`WORK-PACKAGES.md`](WORK-PACKAGES.md) §0.1）。**只读普查，不改任何代码** ✓（R1/R4）。
> 目的：把"同一类表现、做法茫茫多"的 VM 操作**先看清楚**，再决定要不要加 `ce_anm_*` 词汇 ✓；
> **有等价证据才迁移，否则只加注释** ✓。

## 0. ★ 先更正一个前提：**「226 处」是"出现次数"，不是"直写处数"**

`CE_ANM_VM_*` 在卡侧共出现 **226 处**（`grep` 计数 ✓），但其中**大多数是读**（诊断日志、快照、算差值）与注释 ✗。
逐文件按「**对 VM 字段产生写入**」重数（一卡一个只读普查员，各自给行号 + 证据 ✓）后：

| 文件 | 宏出现 | **真·直写** | 备注 |
| --- | :-: | :-: | --- |
| `cards/ufo.c` | 60 | **13** | color_1 隐/还原 7、flags_lo RMW 4、entity_pos 2 |
| `cards/youkai.c` | 66 | **7** | color_1 保 alpha 4、flags_hi resolutionMode 1、POST_LOAD 清零 2 |
| `cards/senro.c` | 4 | **6** | 自机内嵌 VM 的 color_1+color_2 成对（3 个写点 × 2 条 store）|
| `cards/pofv.c` | 10 | **4** | color_1 染色/还原（含"保 alpha"版式）|
| `cards/trance.c` | 25 | **3** | SCALE 2（裁切三元组）+ entity_pos 1 |
| `cards/signal.c` | 18 | **3** | color_1「保 alpha 的 RGB 读改写」×3 |
| `cards/faith.c` | 10 | **3** | 换 VM 时 `memcpy` 搬 POS / POS_2 / SCALE |
| `cards/faith_hud.c` | 12 | **2** | **alpha 成对**：同一个 dword 写 color_1 与 color_2 |
| `cards/lolk.c` | 3 | **2** | color_1「保 alpha」染黄/还原 |
| `cards/bomb10.c` | 16 | **0** | 16 处**全是 `const` 只读**（诊断日志）|
| `cards/animal.c` | 1 | **0** | 唯一命中是读旋转 z |
| `cards/season.c` | 2 | **0** | 2 处都是**读**（读 alpha 再交给 `ce_anm_set_color`）|
| `cards/collect2x.c` | 0 | **0** | 连 `CE_ANM` 都不出现 ✓ |
| `cards/mof.c` | 0 | **0** | 同上 ✓ |
| **合计** | **226** | **43** | ⇒ **"直写"只有 43 处** ✓ |

**⇒ 这一条直接影响 P14 的范围** ✓：不是"226 处要收敛" ✗，而是**43 处**、且**已经高度集中**（color 类占绝大多数 ✓）。

## 1. 意图分类（43 处的全貌）

| # | 意图 | 处数 | 站点（文件:行）| SDK 现状 | 差距 |
| :-: | --- | :-: | --- | --- | --- |
| **I1** | **color_1 的"只改 RGB、保留 alpha"读改写**（染红/染黄/还原快照）| **16** | `youkai.c:2242,2244,6891,6904`、`pofv.c:1872,1928,1936,1969`、`signal.c:2044,2060,2142`、`lolk.c:446,457`、`ufo.c:4747,4749,4843,4845` | `ce_anm_set_color`（`sdk.h:273`）**整条 dword 覆盖** ✗ | ⚠ **语义不等价**：本类**必须**保留 alpha（alpha 归引擎插值器 ✓，见 `lolk.c:439-445`）⇒ 现封装**不能**等价替换 ✗ |
| **I2** | **color_1 + color_2 成对写同一个值**（两路顶点色同 A / 同色）| **8** | `faith_hud.c:621-622`（alpha 成对 ✓）、`senro.c:981/982,984/985,998/999` | `ce_anm_set_color` + `ce_anm_mirror_color`（两步组合 ✓）| 可用两步表达 ✓，但没有"一步成对"的封装（**计划里想加的 `ce_anm_set_alpha_pair` 实际对位的是这一类** ✓）| 
| **I3** | **flags_lo 的 RMW 位操作**（清 bit0/bit1 = 引擎最早那道绘制门槛）| **4** | `ufo.c:4753,4754,4848,4849` | 无 ✗ | 无封装（`engine.h` 的 `CE_ANM_VM_FLAGS_LO`）|
| **I4** | **entity_pos（`+0x5f0`）逐 VM 补偿**（HUD 槽居中；只在值变时写）| **3** | `ufo.c:5215,5216`、`trance.c:4005,4006` | `ce_anm_set_pos`（`sdk.h:265`）✓ | 差别只在"原地比较、值没变不写"与"只写 x/y 不写 z" ✓ ⇒ **可等价**（见 §3）|
| **I5** | **scale 的裁切三元组**（scale + uv_scale + flags_lo \|=0x18）| **2** | `trance.c:3974-3977` | **`ce_anm_set_clip_x`**（`sdk.h:358`）**逐行同形** ✓ | **完全等价 ⇒ 可直接迁移** ✓ |
| **I6** | **换 VM 时搬姿态**（POS / POS_2 / SCALE 三件套 memcpy）| **3** | `faith.c:2356,2357,2358` | 无（`set_pos`/`set_scale` 只管两件、且没有 `__pos_2`）✗ | 需新封装或保留直写 ✓ |
| **I7** | **flags_hi 的 resolutionMode**（清 bits22–24 后置档位）| **1** | `youkai.c:5274-5275` | 无 ✗ | 无封装 |
| **I8** | **POST_LOAD_A / POST_LOAD_B 装载后清零** | **2** | `youkai.c:7115,7116` | 无 ✗ | 无封装（照抄一手 `0x45a8dd`–`0x45a8fe` ✓）|

**★ 一条重要否定结论** ✓：**没有任何一处**写 alpha 插值器三元组（`CE_ANM_VM_ALPHA1_I` / `ALPHA2_I` / `INTERP_INITIAL/GOAL/CURRENT`）✗ ——
计划里设想的「`ce_anm_set_alpha_pair`（必须同写 initial+goal+current）」**在现实里没有对位站点** ✗；
真正成对出现的是 **I2 的 color_1/color_2** ✓ 与 **I5 的裁切三元组**（已有 `ce_anm_set_clip_x` ✓）。

## 2. 候选新增词汇（**只列有真实站点的** ✓；本批不实现 ✗）

| 候选 | 对位 | 站点数 | 备注 |
| --- | :-: | :-: | --- |
| `ce_anm_set_color_rgb(id, rgb)` | I1 | **16** | **唯一够格的新封装** ✓：语义 = 读 `color_1`、`(c & 0xff000000u) \| (rgb & 0x00ffffffu)`、写回 ✓ —— 逐站点同形 ✓（`lolk.c:446/457`、`signal.c:2044/2060/2142`、`youkai.c:2242/2244/6891/6904`、`pofv.c` 4 处、`ufo.c` 6 处）|
| `ce_anm_set_color_pair(id, argb)` | I2 | 8 | 可等价拆成 `set_color` + `mirror_color` ✓ ⇒ **优先级低** ✓ |
| `ce_anm_set_flags_lo_mask(id, and_mask, or_mask)` | I3+I7 | 5 | 两组位操作形态不同（`&= ~3` ✓ / `(x & ~0x1c00000) \| 0x800000` ✓）⇒ 只能做**通用**位封装 ✓ |
| `ce_anm_set_pos2(...)` | I6 | 3 | 需要确认 `__pos_2` 的语义再定 ✓ |

⚠ **`ce_anm_set_clip_x` 已经存在**（`sdk.h:358`）⇒ I5 **不需要新词汇** ✓ 只需（在有等价证据时）把 `trance.c:3974-3977` 换成它 ✓。

## 3. 迁移判定（R1：**有等价证据才迁移** ✓）

| 类 | 可否现在迁移 | 判据 |
| :-: | --- | --- |
| **I5**（裁切三元组）| ✅ **可**（等价证据最硬 ✓）| `sdk.h:358` 的函数体与 `trance.c:3974-3977` **逐行同形** ✓（agent 已逐字段对照：scale[0]=f×art、scale[1]=art、`+0x6c`=f、`+0x534 \|= 0x18` ✓）|
| **I4**（entity_pos）| ⚠ 需先确认两点 | ① 现站点"值没变不写"是**有意**的（省一次写 ✓）⇒ 换成 `set_pos` 会变成"每帧都写"✗ 需确认无副作用；② 现站点**只写 x/y** ✓ 而 `set_pos` 写 x/y/z ✗ |
| **I1**（保 alpha 的 color）| ❌ **不能**（现封装语义不等价 ✗）| 必须先加 `ce_anm_set_color_rgb` ✓ 才谈迁移 |
| **I2/I3/I6/I7/I8** | ❌ 暂不 | 无等价封装 ✗（或需新封装先落地 ✓）|

## 4. 复现与范围

- 普查口径：`grep -n 'CE_ANM_VM_'` 取"宏出现"全集 ✓，再逐行判定"写 / 读 / 注释" ✓；
  **只统计对 VM 字段产生写入的语句** ✓（同一写点若跨两行取址+写，按**写**行计 ✓）。
- 方法：**一文件一个只读 agent** ✓（14 个文件、前台 workflow 扇出 ✓），每个 agent 必须给 `行号 + 归类证据 + 是否已有 SDK 封装` ✓；**不许给改进建议** ✓。
- 未决（留给后续批次）：① `ce_anm_set_color_rgb` 的语义边界（`sdk.h` 里现有 `set_color`/`set_color2`/`mirror_color` 的命名一致性 ✓）；
  ② I6 的 `__pos_2` 语义（`engine.h` 的 `CE_ANM_VM_POS_2 = 0x4e4` ✓）；③ 裸偏移直写（**不带宏**）的同类站点未被本次口径覆盖 ✗ ——
  至少已知 `ufo.c:4748/4750/4844/4846`（本地别名 `UFO_ITEM_VM_COLOR2 = 0x528` ✓）与 `youkai.c:5272`（`vm+0x18` 层号 ✗）两处 ⇒ **下次普查应把"裸偏移"也纳入** ✓。
