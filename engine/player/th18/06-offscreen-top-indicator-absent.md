# TH18 画面顶边的「上面还有东西」三角指示 —— ❌ 没有活的（同形代码在、触发字段无写点）；活的那份在 TH07

> **版本**：TH18 v1.00a（`th18.exe`，imagebase `0x400000`）。本文裸地址默认属该版本；引用其他版本须写成 `th16:0x…`。
> **方法**：一手反汇编 —— (a) 全 exe 扫道具池迭代点（stride `0xc94` / 上界 `0x1258` 的 disp32 字节模式）；
> (b) 用 `objdump --start-address` **重新对齐**反汇编道具 tick body（`0x445a80`–`0x44682f`）与 draw body（`0x446d60`–`0x446eb9`），
> 逐条列所有写 VM `pos`（`+0x30/+0x34/+0x38`）与写 `slot+0x40..0x48` 的指令；(c) 全 exe 列 208 处写 `[reg+0x44]` 的指令按函数归类；
> (d) 解析全部 TH18 `.anm.txt` 的 sprite 表 + PNG 逐像素（找「正三角」形态）；(e) TH16/TH17 同形对照；(f) TH07 活版逐条复核。
> **可信度**：TH18 侧 **✅**（字段、指令、判据、贴图号、类型分派表全是指令级/像素级；「没有活的」是穷举 + 两条独立否证）；
> TH16/TH17 **✅ 结构同形**（地址级）；TH07 活版 **✅**（地址 + 常量 + 贴图像素）。
> **归属**：引擎知识（道具子系统 × 版本）—— 结论回流到 `.md`：本文；改造侧落点见 [`WORK-PACKAGES.md` §11](../../../mods/th18.v1.00a/card-expand/WORK-PACKAGES.md)。
> **一句话给 owner**：**TH18 里这条机制是死的**（有三零件、没扳机），**活的是 `th07`**（妖妖梦，正好是卡 107 的原作）——
> 要复刻请照 §7.1 的 TH07 配方，不要指望 TH18 引擎替我们画。

---

## 0. 一句话结论（含五段链）

| 段 | 内容 |
| --- | --- |
| **发现** | TH18 引擎里**确实有一整套「道具出上沿 ⇒ 画三角」的零件**：每道具第二个 `AnmVm`、成对的三角贴图、阈值 `-8.0f`、固定 `pos.y = 8.0f`、alpha 淡出。宿主 = `ItemManager__on_draw__body` `0x446d60`。 |
| **推测** | 这就是 owner 说的那条机制（owner 2026-10-01 追加「**那个三角形是贴图**」⇒ 正是 `bullet/item.png` 里那几张正三角）。 |
| **验证** | 逐条读判据 ⇒ 判据字段是 **`VM_A.pos.y`**（`slot+0x44`）；全 exe **唯一**写它的地方是擦弹抖动（`0x445d34`，值域 `[-1,1)`）⇒ **`< -8.0f` 永远不成立**。第二条否证：即使成立，画出位置 = `8.0 + entity_pos.y`（= `8.0 + 道具自己的负 y`）⇒ 也在画面外。 |
| **结论** | ❌ **TH18 v1.00a 没有能出现的三角指示**（「形」在、「扳机」不在 ⇒ 死代码）。TH16 / TH17 / TH15 同形同死（§7.2/§7.3）。**活版在 TH07**（§7.1，`th07:0x433d35` 比的是**道具自己的 `pos.y`**）。 |
| **证据** | `0x446dcb`（`comiss` + `0x4b9438`）· `0x445d34`（唯一写点）· `0x446dec`/`0x446e17`（污染 y）· 脚本表 `0x4b4020` · `bullet.anm` script `134`–`141` · `th07:0x433d35`/`th07:0x433d46`/`th07:0x498c70`。 |

---

## 1. 问题 1：TH18 到底存不存在 —— ❌（两条独立否证；但零件全在）

### 1.1 「形」在：五个零件都在（都是一手）

| 零件 | TH18 v1.00a 一手 |
| --- | --- |
| 每道具**第二个 VM**（`VM_B`） | `slot+0x61c`（槽 stride `0xc94`、池首 `mgr+0x14`、`0x1258` 槽）；`Item__constructor` `0x4456b0` 对 `slot+0x10` / `slot+0x61c` 各 `memset 0x60c` |
| 阈值 | `0x446dcb comiss xmm5, [esi-0x500]`，`xmm5 = [0x4b9438] = -8.0f`；`[esi-0x500]` = `slot+0x44` = `VM_A.pos.y`（`AnmVm.pos = +0x30`） |
| 固定 y（画面上沿那条） | `0x446e17 mov [esi+0x10c], 0x41000000` = 写 `VM_B.pos.y = 8.0f`（常量 `0x4b9268`） |
| alpha 淡 | `0x446e0b`–`0x446e36`：按 `VM_B` 上一帧 `pos.y` 算 `(prev+8.0)×0.03125×255`（`0x4b90a8`/`0x4b9350`），`≥32.0`（`0x4b92b4`）则 255；写 `VM_B.color1`（`+0x524`）的 alpha 字节 `slot+0xb43` |
| 三角贴图 | `0x4b4020` 每个 type 8 字节 `{VM_A 脚本, VM_B 脚本}`：type 1..8 的 `VM_B` = `134..141` = `bullet.anm` 的 `sprite501/503/505/507/509/511/513/515`（entry3 `bullet/item.png`） |

### 1.2 「扳机」不在（否证一：判据字段没有写点）

判据字段 = `VM_A.pos.y` = `slot+0x44`。穷举（口径见 §9）：

- 道具 tick body（`0x445a80`–`0x44682f`，重新对齐后逐条）里，**写 VM `pos` 的指令只有一处**：`0x445d34`（擦弹抖动）。
- 全 exe 208 处写 `[reg+0x44]` 的指令按函数归类，落在道具槽上的仍然只有 `0x445d34`（`0x4473c3`/`0x434c7f` 那类是**新建 VM**（FX VM）的清零，不是道具的 `VM_A`）。
- `0x445d34` 所在分支由 `0x445c8b comiss xmm5(1.0), [ecx+0xe6bb14](减速因子) / jbe 0x445d50` 把门 ⇒ **只有因子 < 1.0 才进**；而 TH18 自己从不写这个因子（见 `CE_ITEM_SLOWDOWN` 注释：全 exe 6 处引用，本作恒 1.0）⇒ **分支native永不执行**。
- 即便执行，值域也是 `[-1,1)`：`0x402810`（RNG）= `u32 × 2⁻³¹（0x4b906c）− 1.0`。`-8.0` 是 `VM_A.pos.y < -8.0` 的门，`[-1,1)` 永远跨不过去。
- 脚本侧补刀：道具 `VM_A` 的脚本（112..119）**没有 `pos()` / `posTime` / `posAdopt`**，而 `0x407420`（装载脚本）把脚本模板 `0x544` 字节拷进 VM 时 pos 就是模板里的 0 ⇒ `VM_A.pos.y ≡ 0`。

⇒ **`VM_A.pos.y < -8.0f` 恒假**。「每道具画三角」这条分支在 TH18 native 里取不到。

### 1.3 「扳机」不在（否证二：固定 y 被 `entity_pos` 污染）

`0x446dd2`–`0x446dfa` 在分支**之前**无条件把道具世界坐标（`slot+0xc2c`，float3）拷进**两个** VM 的 `entity_pos`
（`VM_A.entity_pos = slot+0x600`、`VM_B.entity_pos = slot+0xc0c`，两者差值都恰好 `+0x5f0`）。
而绘制位置（`0x48171f` 起）= `pos + entity_pos + pos_2` ⇒ 三角的实际 y = `8.0 + 道具自己的 y`。
道具在画面上沿外时 `y < -8` ⇒ 三角落在 y < 0，**照样在画面外**。
（TH07 的活版没有这一项：三角的 y 是**纯固定值** `8.0 + 全局`，见 §7.1 ⇒ 这一项是移植时带进来的 bug。）

---

## 2. 问题 2：谁画它（Q2）—— 引擎的道具管理器自己画，不是 GUI VM、也不是道具脚本

| 层 | 一手 |
| --- | --- |
| 宿主函数 | `ItemManager__on_draw__body` `0x446d60`（`0x159` 字节；参数 = 一个「画哪一批」的标志 `[ebp+8]`） |
| 两个注册入口 | `ItemManager__on_draw_04` `0x446f00`（`push 1`，注册于 `0x4458bb`，优先级 `0x21`）；`ItemManager__on_draw_08` `0x446f20`（`push 0`，注册于 `0x4458d9`，优先级 `0x13`）|
| arg == 0 的语义 | `0x446dc3 test ebx,ebx / je` ⇒ **整池全跳过**（TH16 的「type 0x10 单独一趟」残骸；TH18 里那一趟是空转）|
| 实际绘制 | `AnmManager__draw_vm` `0x481210`（`ecx = [0x51f65c]` = AnmManager）|
| 池遍历 | `lea esi,[ecx+0x558]`（= `slot+0x544`）、`add esi,0xc94`、`edi = 0x1258` 计数 |

⇒ **不经过 `front.anm` / Gui 的 VM**（与 [`05-poc-height-and-indicator.md`](05-poc-height-and-indicator.md) 的收点线**不同源**：那条是 Gui 建 VM）。

---

## 3. 问题 3：什么时候画（Q3）—— 判据字段与阈值 + 四道闸

按槽（`esi = slot+0x544`）逐道具，四道闸全过才看阈值：

| 闸 | 指令 | 语义 |
| --- | --- | --- |
| 槽在用 | `0x446da0 cmp [esi+0x730],0` / `je` | `slot+0xc74`（`CE_ITEM_STATE`）≠ 0；`0` = 空/已删 |
| `VM_A` 可见 | `0x446dad test byte [esi],1` / `je` | `VM_A.flags_lo`（`+0x534`）bit0 = 绘制门槛（`slot+0x544`）|
| 已出 intro | `0x446db6 cmp [esi+0x740],0` / `jg` | `slot+0xc84`（intro 倒计时 `CE_ITEM_TIMER`）≤ 0 |
| 这一趟要画 | `0x446dc3 test ebx,ebx` / `je` | arg = 1（`on_draw_04`）|

**阈值**：`0x446dcb comiss [0x4b9438](-8.0f), VM_A.pos.y` + `jbe 0x446e5a` ⇒ `VM_A.pos.y ≥ -8.0f` 画 `VM_A`（正常道具图），
`< -8.0f` 才走三角支。**没有**「只在收点线上方」之类的附加条件 —— 阈值只有这一个数，且和收点线字段（`player+0x47998`）无关。

⚠ 但在 TH18 里 `VM_A.pos.y` 拿不到「道具 y」（§1.2/§1.3）⇒ **这条阈值在 TH18 里没有可用的输入**。TH07 的同一条阈值吃的是**道具自己的 `pos.y`**（§7.1）。

---

## 4. 问题 4：画成什么（Q4）—— anm / script / sprite / 尺寸 / 颜色

- **脚本**：`bullet.anm` script `134`–`141`（= 表 `0x4b4020` 的第二个 dword `0x85 + type`，type 1..8）。
  内容逐条（TH18 `local/th18.v1.00a/anm/bullet/bullet.anm.txt`）：
  `sprite(sprite501/503/…); originMode(1); type(0); static();` —— **静态无动画**（没有 alpha/scale/pos 指令）。
- **贴图**：`bullet.anm` 的 entry3，`name: "bullet/item.png"`（256×64）。`VM_B` 贴图号恒为 **`VM_A` 贴图号 +1**（偶/奇成对）：

| type | `VM_A` 脚本 / 贴图 | `VM_B` 脚本 / 贴图 | 三角像素形态（一手：逐像素行宽）|
| --- | --- | --- | --- |
| 1 | 112 / `sprite500`（16×16） | 134 / `sprite501` | 10 行，行宽 `2,2,4,6,6,8,8,10,10,10` = **66 px 正三角** |
| 2 | 113 / `sprite502` | 135 / `sprite503` | 同 1（同形，配色不同）|
| 3 | 114 / `sprite504`（32×32） | 136 / `sprite505` | 同形，放大到 32×32 单元格 |
| 4 | 115 / `sprite506` | 137 / `sprite507` | 同形 |
| 5 | 116 / `sprite508` | 138 / `sprite509` | 同形 |
| 6 | 117 / `sprite510` | 139 / `sprite511` | 同形 |
| 7 | 118 / `sprite512` | 140 / `sprite513` | 同形 |
| 8 | 119 / `sprite514` | 141 / `sprite515` | 同形 |

- **颜色**：由 `color1`（`+0x524`）决定；三角支**只改 alpha 字节**（`slot+0xb43`），RGB 不动（贴图自带颜色 + 脚本未写 `color()`）。
- **淡入淡出**：`alpha = clamp((上一帧 VM_B.pos.y + 8.0) × 0.03125 × 255, 上限 255)`。
  ⚠ 但引擎**每帧把 `VM_B.pos.y` 钉成 8.0**，而三角脚本又不改它 ⇒ 从第二帧起 alpha 恒 `(8+8)/32×255 = 127`（首帧 63）。
  ⇒ 「涨落/闪烁」在 TH18 里**不成立**（这条公式的输入被自己写死了）。TH07 的活版吃的是道具 y，是真的淡出（§7.1）。

---

## 5. 问题 5：画在哪（Q5）—— x 跟道具、y 固定值、每道具一个、无合并/上限

| 问 | 答（TH18 代码原义）|
| --- | --- |
| x | **跟道具**：`VM_B.entity_pos.x = 道具 x`（`0x446dec` 前的 `0x446dd8` 拷 `slot+0xc2c`）。**没有**夹边距（夹边距在 spawn：`0x446fd5`/`0x446fe6` 把道具 x 夹到 ±192，`0x4b946c`/`0x4b933c`）。 |
| y | **固定 `8.0f`**（写进 `VM_B.pos.y`）—— 在本作坐标系里就是游玩区上沿（`originMode(1)` 下 y 从顶部起算）。⚠ 实际还叠了 `entity_pos.y`（§1.3）。 |
| 多个道具 | **每个道具一个三角**（三角是**该道具自己的** `VM_B`，不是共享 VM）⇒ 同屏 N 个出上沿的道具 = N 个三角，各在自己的 x。 |
| 合并 / 上限 / 去重 | **都没有**（无共享槽、无计数、无去重）。唯一的「状态」是每个槽的 latch `slot+0xc7c`：画 `VM_A` 时写 0、画 `VM_B` 时写 1 —— **全 exe 无人读**（纯记录）。 |

---

## 6. 问题 6：生命周期（Q6）—— 每帧现判、没画就不画、删除路不额外收尾

| 时点 | 行为（一手）|
| --- | --- |
| 出现 | 每帧在 `0x446d60` 里**现判现画**，无「创建/销毁」动作（`VM_B` 是道具槽的内嵌 `AnmVm`，随槽存在）。 |
| 消失（掉回画面内） | 阈值不成立 ⇒ 那帧改画 `VM_A`；三角**下一帧就不再画**（**没有**反向淡出 —— alpha 斜坡只作用在三角支内部）。latch 翻回 0。 |
| 消失（被吸附/回收/删除） | 道具删除 = `state` 清 0（`0x445e38` 出界删 / `0x446595` 收点 commit）⇒ §3 第一道闸 `0x446da0` 直接跳过整个槽，三角当帧消失。`VM_A/VM_B` 是内嵌结构，**没有**单独释放；只有 type 0x10..0x13 那条外部 VM（id 记在 `slot+0xc28`）走 `0x488cf0`。 |
| intro 期 | `slot+0xc84 > 0` 时整个槽不画（闸 3）⇒ intro 里的三角也不画。 |
| 淡入 | 只在三角支里：首帧 alpha 63 → 之后 127（§4）⇒ 在这份代码里实际看不出淡入。 |

---

## 7. 问题 7：跨作 —— TH16 / TH17 / TH15 同形同死；**TH07 是活的**

### 7.1 ★ 活版：TH07（`th07.exe`，v1.00b）—— 卡 107 的原作，配方在这里

| 项 | TH07 一手 |
| --- | --- |
| 谁画 | 引擎（不是 anm 脚本）：道具的每帧绘制函数 `th07:0x433cd0` 里，同一个 `AnmVm`（`item+0x1c8..0x1d0` 是它的 pos、`item+0x1b8` 是 color1）**换贴图** |
| 判据 | `th07:0x433d2c fld [item+0x250]`（**道具自己的 pos.y**）→ `th07:0x433d35 fcomp [th07:0x498c70]` → `fnstsw ax; test ah,5; jp` ⇒ 条件 = **`item.pos.y < -8.0f`**（常量 `th07:0x498c70` = `-8.0`，实测）|
| 固定 y | `th07:0x433d46 fld [th07:0x498a7c]`（= **8.0**）`+ [th07:0x62f868]`（全局，随游玩区偏移）→ `th07:0x433d55 fstp [item+0x1cc]` ⇒ **y 恒 = 8.0 + 全局**，只跟 x（`item.pos.x + [th07:0x62f864]`，`th07:0x433cf2`/`0x433d01`）|
| 贴图号 | 出上沿 ⇒ `set_sprite(type + 0x2b6)`（`th07:0x433d73`）；画面内 ⇒ `type + 0x2ac`（`th07:0x433e1a`）。两者都在 `etama.anm` entry1（`data/etama/etama2.png`）|
| 三角真身 | `etama2.png` 的 `(240,64,16,16)`（三档樱点 6/7/8 共用）：行宽 `2,3,4,6,6,8,8,10,10,10` = 正三角 ✅（与 TH18 的 `sprite501` 同形）|
| 淡出 | `th07:0x433daa`–`0x433dfa`：`alpha = clamp(255 − (8.0 − item.pos.y) × 255 / 128, 下限 0x40)`；常量 `th07:0x498af8` = 255、`th07:0x498b24` = 128 ⇒ 越往上越淡（y=8 → 255、y=-56 → 128、y≤-120 → 64 地板） |
| 去重 / latch | `item+0x27e` 一个字节的闩锁：只在**翻转**那一帧重设贴图（画面内写 1、出上沿写 0）；color1 在出上沿那帧先写 `0xffffffff` 再由 alpha 覆盖 |
| 只管哪些 type | 表里的 `type + 0x2b6` 槽：type 6/7/8 共用一张三角；type 9 另有一张 62×62 的（逐 type 全表见 [th07/01](../../item/th07/01-sakura-items.md) §3.3/§3.4）|

> **⇒ 这就是 owner 记的那条机制，而且它是「点在画面外、画面内的指示」这个语义的**唯一**一手活证。**
> 与 TH18 的差别只有两点：① 判据吃**道具自己的 y**（TH18 吃 VM 的 pos）；② y 是**纯固定值**（TH18 还加了 `entity_pos.y`）。
> 这两点正好解释了「为什么 TH18 引擎不替我们画」。

### 7.2 TH16 / TH17：与 TH18 **逐指令同形**（一样的三零件 + 一样的死扳机）

| 项 | TH16 v1.00a | TH17 v1.00b |
| --- | --- | --- |
| draw body | `ItemManager__on_draw__body` `th16:0x4307a0` | `th17:0x434570` |
| 阈值 | `th16:0x430832 comiss` vs `th16:0x494740` = `-8.0` | `th17:0x4345db comiss` vs `th17:0x4a3e5c` = `-8.0` |
| 固定 y 写点 | `th16:0x430860` = `8.0`（`th16:0x4945c0`）| `th17:0x434627` = `8.0`（`th17:0x4a3ca8`）|
| alpha | `th16:0x430890`；`32.0`/`0.03125`/`255.0` = `th16:0x4945fc`/`0x494410`/`0x49467c` | `th17:0x434646`；`th17:0x4a3ce8`/`0x4a3ac4`/`0x4a3d68` |
| 两 VM | `VM_A = esi-0x530`、`VM_B = esi+0xcc`，stride `0xc78` | `VM_A = esi-0x530`、`VM_B = esi+0xd0`，stride `0xc78` |
| 脚本表（type→两脚本）| `th16:0x4917f8`：type 1..8 ⇒ `112..119` / `134..141` | ❓ 表地址未定（代码同形）|
| 三角贴图 | TH16 `bullet/item.png` `sprite501/503` 逐像素 = 行宽 `2,2,4,6,6,8,8,10,10,10`（与 TH18 同）| ❓ 该作 `item.png` 未解包，未逐像素比 |
| TH16 独有 | draw body 里多一道 `type == 0x10` 分趟闸（`th16:0x4307e7` / `th16:0x4307fb`）| — |

**两者都是「形在、扳机不在」**：TH16 的 `VM_A.pos` 同样只有擦弹抖动一处写点（`th16:0x42fb2d`），TH17 同理（`th17:0x4336bf`/`0x4336c4`）。
（TH17 的 `th17:0x434c7f mov [esi+0x40],0` 是**新建 FX VM** 的清零，不是道具 `VM_A`；同 TH18 的 `0x4473c3`。）

### 7.3 TH15：零件一样，但本仓旧文档把它**叫错了名**

- `th15` 的 draw 闸同形：`th15:0x4407bd comiss -8.0(th15:0x4d0108), VM1.pos.y`；`pos.y ≥ -8` 画 `VM1`（`th15:0x44083c`），否则画 `VM2`（`th15:0x440824`）——见 [`../../item/th15/01-tick-shake-tint-and-graze-item.md`](../../item/th15/01-tick-shake-tint-and-graze-item.md) §3。
- ⚠ **订正（本文一手，2026-10-01）**：该文档把 `VM2` 记成「**压暗变体**」。实测 TH15 的 `VM2` 脚本（`126..133`）就是
  `sprite(sprite501/503/…/515); originMode(1); type(0); static();`（`local/th15.v1.00b/anm/bullet/bullet.anm.txt`），
  而 `local/th15.v1.00b/anm/bullet/bullet/item.png` 的 `(192,16)`/`(208,16)` 逐像素行宽 = `2,2,4,6,6,8,8,10,10,10`（= **同一个正三角**，
  不是同形的暗版）。原文「不透明像素 144→66」正是**方块→三角**的像素数差，不是压暗 ⇒「压暗变体」这个命名作废，`VM2` = 本条三角指示。
  ⇒ **TH15 与本条机制同源，同样死**（`VM1.pos` 也只有擦弹抖动一个写点，`th15:0x43fa44`）。

### 7.4 对改造的含义（非一手，做法建议）

1. **别指望 TH18 引擎**：`0x446d60` 那条支在 native 里取不到（§1.2/§1.3）；且它按 `-8` 判的是 VM 的 `pos`、y 还叠了 `entity_pos` ⇒ 就算强行把它点亮也不对位。
2. **要复刻就照 TH07 的配方**（§7.1 五项）：判据 `对象 y < -8.0f` · 贴图 = 对应「出上沿」槽 · pos = `(对象 x, 8.0)` · `alpha = clamp(255 − (8 − y)×255/128, ≥64)` · 翻转闩锁。
3. **贴图从原生切**：TH18 的三角贴图就在 `bullet/item.png`（`sprite501/503/505/…/515`），可直接用（`CE` 侧切图见
   [`../../anm/th18/04-anm-slot-selection.md`](../../anm/th18/04-anm-slot-selection.md) 的 slot 选择链）；**不要自己画三角**。
4. **长持续效果铁律**：三角 VM 必须在卡的 `on_delete`（虚表 `+0x50`）里收干净（见 [`METHOD.md`](../../../METHOD.md) 的对应铁律）。

---

## 8. 问题 8：三角只管「点」，还是所有道具类型（type 分派表）

**TH18（表 `0x4b4020`，每个 type 8 字节 = `{VM_A 脚本, VM_B 脚本}`，type 由 `slot+0xc78` 给；spawn `0x44706f`/`0x4470f2` 取两个 dword）：**

| type | 0 | 1..8 | 9..0xe | 0xf | 0x10..0x13 | ≥0x14 |
| --- | --- | --- | --- | --- | --- | --- |
| `VM_A` | — | `112..119` | `120..125` | — | 表外（`0x447089` 支）| 直接返回 0 |
| `VM_B`（三角）| — | **`134..141` ✅** | `0xffffffff`（无）| — | 表外（外部 VM，`slot+0xc28`）| — |

⇒ **只有 type 1..8 有三角**。按本仓类型表（`CE_ITEM_TYPE` / [道具 OVERVIEW](../../item/OVERVIEW.md)）：1 = P、2 = 点（金钱）、4 = 命碎、6 = 符卡碎，3/5/7/8 = 它们的「大」版/满值版。
**9..0xe（分数转换点族）没有三角**；type 0xf 在 `0x447001` 被重映射成 6（借用 6 的三角）；0x10..0x13 走另一条「外部 VM」路（`VM_A/VM_B` 的闸位在 `0x447089`/`0x447093` 被清 0 ⇒ 那两个 VM 不画）。

**TH07（活版）：** 有独立「出上沿」贴图槽的是 type 6/7/8（★ / 大樱点 / 小樱点，共用同一张三角）+ type 9（樱点，另一张 62×62）⇒ **只管樱点族**（这也正是 owner 记忆的来源：卡 107 = 妖妖梦）。

---

## 9. 未解与穷举口径

### 9.1 穷举口径（做了什么、范围多大）

| # | 口径 | 结果 |
| --- | --- | --- |
| ① | 全 exe 扫道具池迭代点：`local/_scratch/th18.dis`（265,489 行）grep `0xc94`（stride）/`0x1258`（上界）/`0x4cf2ec`（池指针） | 池迭代只有 4 处：`0x42aa30`（destroy_all）、`0x445a80`（tick body）、`0x4467e9`（body 内）、`0x446d60`（draw body）⇒ 没有「第五个遍历道具池的地方」 |
| ② | 对齐反汇编道具 tick body（`0x445a80`–`0x44682f`）+ draw body（`0x446d60`–`0x446eb9`）后，列所有写 `[reg+0x30/0x34/0x38/0x40/0x44/0x48]` 的指令 | tick 里唯一：`0x445d34`（+`0x445d39`）；draw 里唯一：`0x446e17`（写 `VM_B.pos.y`）|
| ③ | 全 exe 列 208 处写 `[reg+0x44]` 的指令，按函数归类（`th18-funcs.json` 归属），逐类排除 | 落在道具槽 `VM_A.pos.y` 上的仍只有 `0x445d34`；其余是别的结构体/别的 VM |
| ④ | 全 exe grep 立即数 `0xc1000000`（`-8.0f`）| **0 处** ⇒ 全 exe 只有 `0x4b9438` 这一份 `-8.0` DAT，且只被 `0x446dcb` 读 ⇒ 没有第二个用同一阈值的判据 |
| ⑤ | 解析**全部** TH18 `.anm.txt` 的 sprite 表（`front.anm` 382 script 等）+ 对应 PNG 逐像素，筛「行宽单调增、尖顶、宽 ≤ 40」的正三角 | 命中：`bullet/item.png` `sprite501/503`（+ 同族的 505..515）、`front/front00.png` `sprite28/58`、`ascii*.png` 三个小三角、`st03logo` 一个 ⇒ **道具侧只有 `item.png` 那对偶号/奇号** |
| ⑥ | TH16 / TH17 / TH15 同形对照（`local/_scratch/th16.dis`、`local/_scratch/th17/th17.asm`、TH15/16/17 的 `bullet.anm.txt` 与 `item.png`）| 三作都是同一套（§7.2/§7.3）；TH15/TH16 的三角贴图逐像素复核过 |
| ⑦ | TH07 活版复核（`local/th07.v1.00b/th07.exe` 反汇编 + `etama2.png` 逐像素）| 判据/固定 y/贴图号/alpha/闩锁逐条落地（§7.1）|

### 9.2 未解（诚实清单）

- ❓ **TH18 里 `VM_A.pos.y`（`slot+0x44`）到底有没有第三方写点**：我用「全 exe 写 `[reg+0x44]`」这一刀切过（208 处），
  理论上仍可能有「先把槽地址算进别的寄存器、再用 `[reg+neg]`」的写法绕过 disp 口径；本轮没有做到「按指针数据流」级的穷举。
  但即使有写点，§1.3 的第二条否证（y 叠 `entity_pos`）仍独立成立 ⇒ 「TH18 不对位」这个结论不依赖它。
- ❓ **ZUN 为什么留着这份死代码**：本仓无 TH13/TH14 的道具绘制一手可对（`engine/item/` 没有 th13/th14），
  所以「TH15 改成 VM 体系时把 `item.pos.y` 误写成 `VM1.pos.y`」只是**推测**（🟡），不是结论。
- ❓ **TH17 的脚本表地址与 `item.png`**：该作 `item.png` 未解包、表地址未定（TH17 代码同形已验）⇒ §7.2 那两格留 ❓。
- ❓ **TH18 三角的最终屏幕像素**：`pos.y = 8.0` 叠上 `entity_pos.y`、再叠 `resolutionMode` 缩放与 `originMode(1)` 的原点，
  本轮的数值未逐像素对过（🟡）。对「TH18 不画」这个结论无影响。
- ❓ **`VM_B.flags_lo`（`slot+0xb50`）bit0 由谁置上**：TH18 的 `0x446e02` 要它；我以为由脚本装载（`0x407420` 拷模板 `0x544` 字节）置上，
  但没有逐字节读 anm 模板验证（🟡；`bullet.anm` 二进制在 `local/th18.v1.00a/dat/bullet.anm`，可复核）。

### 9.3 复核命令

```bash
# ① 判据 + 固定 y + alpha（对齐反汇编，避免线性快照的 misalignment）
objdump -d --start-address=0x446d60 --stop-address=0x446eb9 -M intel local/th18.v1.00a/th18.exe
# ② 阈值/常量
python3 tooling/pe_read.py local/th18.v1.00a/th18.exe 0x4b9438 --dwords 1   # -8.0
python3 tooling/pe_read.py local/th18.v1.00a/th18.exe 0x4b9268 --dwords 1   # 8.0
# ③ 唯一写点（擦弹抖动）与其闸
objdump -d --start-address=0x445c80 --stop-address=0x445d60 -M intel local/th18.v1.00a/th18.exe
# ④ type→两脚本表
python3 tooling/pe_read.py local/th18.v1.00a/th18.exe 0x4b4020 --dwords 32
# ⑤ TH07 活版
objdump -d --start-address=0x433cd0 --stop-address=0x433e80 -M intel local/th07.v1.00b/th07.exe
python3 tooling/pe_read.py local/th07.v1.00b/th07.exe 0x498c70 --dwords 1   # -8.0
```

---

## 10. 交叉链接

- 收点线那条**另一条**指示（Gui 自建 VM，别混）：[`05-poc-height-and-indicator.md`](05-poc-height-and-indicator.md)。
- 道具子系统一手：[`../../item/OVERVIEW.md`](../../item/OVERVIEW.md)、TH07 的樱点/两槽贴图 [`../../item/th07/01-sakura-items.md`](../../item/th07/01-sakura-items.md) §3.3、
  TH15 的副 VM 与擦弹抖动 [`../../item/th15/01-tick-shake-tint-and-graze-item.md`](../../item/th15/01-tick-shake-tint-and-graze-item.md) §3（本文对该节命名有订正，见 §7.3）。
- 道具字段（本仓改造侧）：[`../../../mods/th18.v1.00a/card-expand/native/engine.h`](../../../mods/th18.v1.00a/card-expand/native/engine.h) 的 `CE_ITEM_*` 段；AnmVm 字段：`CE_ANM_VM_*`。
- 改造需求与落点：[`WORK-PACKAGES.md` §11](../../../mods/th18.v1.00a/card-expand/WORK-PACKAGES.md)、验收行 `SINGLE-CARD-TEST.md` §th07 #4。
