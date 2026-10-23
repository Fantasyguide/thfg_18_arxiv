# 08 — TH18 符卡「红/蓝晕光」= `et` 特效（`BossCard6_et` / `BossCard6_et2`）

> **版本**：TH18 v1.00a（`th18.exe`，imagebase `0x400000`）。裸地址默认属该版本。
> **状态**：🟡 **进行中**（2026-10-22，目标 goal-ba5c1b97）。已定案的写 ✅，仍缺的写 ❓。
> **与谁有关**：卡 109 C4「让自机看起来像 boss」的**颜色层**部分（扭曲层是另一件事）。

## 0. 已定案（一手）

| # | 结论 | 证据 |
| --- | --- | --- |
| ✅1 | 那层颜色是**独立的 `et` 特效**，**不是** aura mesh 的一部分 | exe 里只有两个字符串 `BossCard6_et`(`0x4b651c`)、`BossCard6_et2`(`0x4b652c`)；`th18.dat` 里**全部 55 个 .anm 解出来搜过 ⇒ 零命中** |
| ✅2 | 它由 **`0x42d7d0(this=效果管理器, name, params)`** 创建：`push 0x683c` 申请 **0x683c 字节**实例 → `0x42e100(obj, name)` 初始化；随后把 `params` 里的位置（两个 float）写 `+0x12b4`、`+0x12bc`、`+0x621c`、`+0x6220`、`+0x6224` | `0x42d7d6`/`0x42d7ed`/`0x42d7f7`–`0x42d82f` |
| ✅3 | **实例内部是一池 16 台 anm VM**：`0x42e1a0` 处 `ebx=0x10` 的循环，每次 `push 0x488 / push 0 / push edi / call 0x48f8f0`（memset）后 `edi += 0x488` ⇒ **16 × 0x488** 的 VM 槽 | `0x42e1a0`–`0x42e1c3` |
| ✅4 | 名字是**运行时按表二分查找**解析的：`0x48d920(this=效果, name)` 在 `[this+0x1218]` 指向的表里二分（`+8`=个数、`+0x8c`=指针数组、元素步长 8、字符串逐字节比较） | `0x48d920`–`0x48d9xx`；表来自效果管理器 `+0x188`（`0x42e431`） |
| ✅5 | **混合模式（机制层）**：TH18 的 anm 精灵绘制**永远**开分离 alpha 混合 —— `SEPARATEALPHABLENDENABLE=1`、`SRCBLENDALPHA=ONE`、`DESTBLENDALPHA=ZERO`、`BLENDOPALPHA=ADD` ⇒ **`Aa' = Sa`**；`blendMode(n)` **只改 RGB** 三个状态 | 见 [`07-blendmode-rgb-only-alpha-is-sa.md`](07-blendmode-rgb-only-alpha-is-sa.md)（`0x47d937`–`0x47d9a8`） |

⇒ 所以"红/蓝晕光"的画法 = **若干张带颜色的 anm sprite（顶点色/贴图 alpha）铺在场上**，
RGB 按 `blendMode` 的二选一预设混合、**alpha 恒等于源 alpha（直接覆盖）**。

## 1. 仍缺（❓ 待一手）

| 项 | 现状 | 下一步 |
| --- | --- | --- |
| ❓ `blendMode` 具体值 | 未读到 | 读该 et 脚本实例里的 blendMode 字段（或它注册时设的状态） |
| ❓ 所在 **layer / 优先级数字** | 未读到 | 16 台 VM 由 anm 渲染器按层画 ⇒ 找出这些 VM 的 layer 来源（脚本字段 or 注册时传参） |
| ❓ 用了几张 sprite、各自颜色 | 未读到 | 同上（脚本实例数组） |
| ❓ 脚本本体在哪 | 名字表是**运行时表**；`th18.dat` 里搜不到名字 ⇒ 脚本可能是**exe 内建**或按索引取自 `effect.anm` | 追 `0x48d920` 的返回值怎么被使用（脚本指针） |

## 2. 已知的工具/环境

- `local/vendor/thtk` **在且已构建**（`build/thdat/thdat`、`build/thanm/thanm` 可用）
- `thdat -l 18 <th18.dat>`：227 个条目；`.anm` **55 个**（含 `effect.anm`）
- `thanm -l 18 effect.anm`：可导出该 anm 的完整文本（条目名是**贴图名**，如 `effect/eff_switch.png`）
