# TH08 永夜抄 消弹 → 出不出实体道具

> **版本**：TH08 v1.00d（`local/th08.v1.00d/th08.exe`，840,704 B，imagebase `0x400000`，md5 `4608e69e470878a3603cc1352b2ea742`）。本文裸地址默认属该版本；引用别作一律写 `th18:0x…`。
> **一手工具**：`objdump -d -M intel …`（§4 给可复现命令；跨函数调用点穷举 = 全 `.text` 线性扫 `call rel32` + 常数真值用 python 直读 PE 节区）。
> **辅助（非一手，只借名字，不借结论）**：`local/vendor/th-re-data/data/th08.v1.00d/funcs.json`。
> **素材**：本仓研究稿 `local/_scratch/research-cancel-original-th06-th11.md` §TH08（707–896 行）+ §1.1（U2/U3/U6）+ §99（N15–N19）。本文对其中 **3 处**做了逐指令复核并**订正**（§2.3 / §1.6 / §1.7），**没有**把任何 🟡/❓ 升级成结论。
> **相关**：[`../th18/01-cancel-to-item.md`](../th18/01-cancel-to-item.md)（宿主引擎的 `mode` 表） · [`../../item/th08/01-time-orbs.md`](../../item/th08/01-time-orbs.md)（本作道具池 / state 语义）

## 1. 发现（一手事实）

### 1.1 消弹入口全集（四个函数，逐个点名 + 谁在调）

弹池 = `BulletManager` static `0xf54e90`；弹槽基址 `0xf6f710`、**`0x600`（1536）槽 × stride `0x10b8`**、槽 0 起即有效；
**弹 state（WORD）在 `+0xdb8`**、弹位置在 `+0xd44`、判定用的第二个向量在 `+0xd34`（`0x43083a`/`0x43084d`/`0x430856`/`0x43086f`）。
道具池 = `ItemManager` static `0x1653648`，`Item` stride `0x2e4`、**type 字节 `+0x2d4`、state 字节 `+0x2d7`**（`0x44022e`/`0x44023a`）。

| # | 入口 | 名字（vendor / 本文） | 签名 | 撒实体？ | 调用点（穷举） |
| --- | --- | --- | --- | --- | --- |
| ① | `0x430830` | `BulletManager::cancel_all` | `cdecl(arg)` | **分三支**，两撒一不撒（§1.2） | `0x415c6c` / `0x41e002` / `0x42bac2` |
| ② | `0x430d30` | `ecl_161__cancel_radius?` | `thiscall(mgr; pos*, float r)` | **撒**（每颗 `type 6`） | 只有 `0x41dff1` |
| ③ | `0x430aa0` | `BulletManager::sub_430aa0` | `thiscall(mgr; int maxBonus, int spawnItems)` | **撒（弹侧无条件）+ 冒数字** | `0x41620f` / `0x42d926` |
| ④ | `0x415c60` | `BulletManager::cancel_all_1` | `thiscall(mgr)` | **撒**（= ① 的 `arg=1`） | 6 处，见下表 |

- **① 的三个调用点**：`0x415c6c`（在 ④ 的包装体内）、`0x41e002`（**ECL op162**，`0x41dffb push 0x4`）、`0x42bac2`（`Enemy::check_time_threshold` 内，`0x42babb push 0x4`）。
- **④ 是"一行包装"**：`0x415c67 push 0x1` → `0x415c6c call 0x430830` → `ret`（函数体 `0x415c60`–`0x415c74`）。它的 6 个调用点 = `0x4154ae`（`Spellcard::ecl_spell`）、`0x41db01`（ECL VM）、`0x433d91`（`GuiImpl::sub_43396d`）、`0x440b01`（`ItemManager::on_tick`）、`0x440d83`（`Item::collect_power_item`）、`0x4411fc`（`Item::collect_big_power_item`）。
- **② 只被一个点调**：ECL 161/162 是**同一个分派块里的相邻分支**（`0x41dff1` 与 `0x41e002`，中间只隔一个 `jmp 0x41e7e4`）。
- **没有第五个入口**：`grep -nE 'call +0x(430830|430aa0|430d30|415c60)'` 的全部命中就是上表；`0x4400a0`（`spawn_item`）在消弹链上只从 ①②③ 内部进（§2.1）。

### 1.2 ★ ① `cancel_all(arg)` 的三条支路（真值，逐指令）

```
43086f: movzx eax,WORD PTR [edx+0xdb8]   ; 弹 state
430878: je 0x430889                      ; state==0 ⇒ 跳过
430884: cmp edx,0x5 / 430887: jne 0x43088b ; state==5 ⇒ 跳过（去重）
43089e: mov ecx,0x17d5ef8                ; 判定对象（= PLAYER 基址）
4308a3: call 0x449ff0                    ; 判定 → eax
4308c8: cmp eax,0x2 / 4308cb: jne 0x4308f8
```

| 条件 | 结果 | 证据 |
| --- | --- | --- |
| 判定 == 2 | `spawn_item(pos, type = *(int32_t*)0x18b8988, state = 1)`，然后整槽 `rep stosd 0x42e` dword 清零 | `0x4308cd`–`0x4308f4` |
| 判定 ≠ 2 **且** `arg ≠ 4` | `spawn_item(pos, type = *(int32_t*)(mgr+0x6ba570), state = arg)`，然后整槽清零 | `0x4308f8`–`0x430929` |
| 判定 ≠ 2 **且** `arg == 4` | **只写 `state=5`，一颗都不出** | `0x4308fc je 0x43092d` / `0x430930 mov WORD PTR [ecx+0xdb8],0x5` |
| `state == 0` 或 `== 5` | 整颗跳过（不撒、也不改 state） | `0x430878` / `0x430887` |

⇒ **「boss 击破 / 超时：满屏弹幕消失但一颗点都不出」的同族就是 `arg == 4` 那一支**（ECL op162 与 `Enemy::check_time_threshold` 都传 4）。
⇒ 注意：`arg` 落在 **state** 位（不是 type）——`0x415c60` 传 1，所以那条路撒出的道具**出生即 `state=1`**。
⇒ `arg == 4` 只拦"判定 ≠ 2"的弹：**判定 == 2 的弹即使 `arg==4` 也照样撒**（判定在前，`0x4308c8` 早于 `0x4308f8`）。

### 1.3 ② 半径消弹（ECL op161）与 ③ `sub_430aa0`

- **②**：`0x600` 槽全扫（`0x430d3a` 基址、`0x430d67` stride `0x10b8`）；半径参数**先自乘**再比较：
  `0x430d49 fld [ebp+0xc]` / `0x430d4c fmul [ebp+0xc]` / `0x430d4f fstp [ebp+0xc]` ⇒ 比较用的是 **`r²`**（✅ 研究稿读法正确）。
  命中一颗 ⇒ `0x430dd7 push 0x1`（state）+ `0x430dd9 push 0x6`（**type 6**）+ `0x430de3 push pos` → `0x430de9 call 0x4400a0`，随后整槽清零。
- **③**：前半段扫弹池（`0x600` 槽），**只跳 `state == 0`，不跳 `state == 5`**（`0x430af3`/`0x430afc`；与 ① 不同）；
  后半段扫**激光池**（`this+0x660938`、`0x100` 槽 × stride `0x59c`、占用 `+0x584`、state 字节 `+0x598`，`0x430bcb`–`0x430c00`）并把激光 state 写成 `2`（`0x430c2a`）。
  - 弹侧撒道具**无开关**（两个分支都撒）；激光侧**有开关**：`0x430c53 cmp DWORD PTR [ebp+0xc],0x0` / `je` 跳过 ⇒ 第 2 个参数 = "激光侧撒不撒"。
  - 冒数字：`0x430b84 call 0x403200`（`AsciiManager::sub_403200_makes_array_1_popup`），值从 **`2000`（`0x430ab0 mov [ebp-0x4],0x7d0`）起、每颗 `+0x14`（20）**、上限 = 第 1 个参数（`0x430bac`/`0x430bb1`），累加值 `eax` 返回给调用方。
  - 两个调用点都传 **(maxBonus = `0x1f40` = 8000, spawnItems = 1)**（`0x416203 push 0x1` / `0x416205 push 0x1f40`；`0x42d91a`/`0x42d91c` 同），返回值再喂给 `0x42efb0`（`EnemyManager::sub_42efb0`）。

### 1.4 出的那几条 / 不出的那几条

**出的**：① 的"判定 == 2"支与"`arg ≠ 4`"支、② 全部命中、③ 的弹侧全部命中（+ 激光侧当 `spawnItems ≠ 0`）、④（= ① `arg=1`）。
**不出的**：① 的 `arg == 4` 支（只 `state=5`）、① 跳过的 `state ∈ {0,5}` 槽、**场地门外**（§1.5）、③ 的激光侧当 `spawnItems == 0`（本轮两个调用点都不是）。

| 出口 | 每颗几颗 | type | state | 掉在哪 |
| --- | --- | --- | --- | --- |
| ① 判定==2 支 | 1 | `*(int32_t*)0x18b8988`（🟡 静态 0，§2.3） | 1 | 弹自己的位置 `bullet+0xd44` |
| ① 判定≠2 且 `arg≠4` | 1 | `*(int32_t*)(mgr+0x6ba570)`（🟡 字段语义未定） | `arg`（④ ⇒ 1） | 同上 |
| ② op161 | 1 | **`6`**（字面量 `push 0x6`） | 1 | 同上 |
| ③ 弹侧 / 激光侧 | 1 | 同 ① 的两个来源 | 1 | 弹：`+0xd44`；激光：`laser+0x548` |

### 1.5 判定形状与场地门（常量真值）

| 入口 | 形状 | 依据 |
| --- | --- | --- |
| ① | **无几何形状**（全屏逐槽），但每颗弹过一次"炸弹判定区"查询 `0x449ff0`（vendor 名 `Player::check_bomb_cancel_areas`） | `0x4308a3` / `0x4308c3` |
| ② | **圆**：`r²` vs 该弹到圆心的距离平方（圆心 = 调用方给的 `pos`） | `0x430d49`–`0x430d4f`、`0x430dc6 call 0x40b500`、`0x430dcb fcomp [ebp+0xc]` |
| ③ | 同 ①（弹侧）；激光侧无几何判定 | `0x430b14`–`0x430b19` |

**场地门在 `spawn_item` 开头（不在消弹入口里）**，两个常量真值：**`0x4b48dc = −64.0f`、`0x4b4940 = 448.0f`**（python 直读 PE）：

```
4400c0: mov eax,DWORD PTR [ebp+0x8]      ; pos
4400c3: fld DWORD PTR [eax]              ; ★ pos[0]
4400c5: fcomp DWORD PTR ds:0x4b48dc      ;   −64.0f
4400d0: jnp 0x4400e4                     ;   ⇒ 越界
4400d2: mov ecx,DWORD PTR [ebp+0x8]      ; pos（同一个指针）
4400d5: fld DWORD PTR [ecx]              ; ★ 又是 pos[0]（无 +4）
4400d7: fcomp DWORD PTR ds:0x4b4940      ;   448.0f
4400e4: mov eax,[ebp-0xc] / add eax,0x17aac0 / jmp 0x440464   ; ← 直接到函数尾
```

- 越界 ⇒ 跳到函数尾 `0x440464`（`mov esp,ebp / pop ebp / 0x440467 ret 0xc`），返回 **`ItemManager+0x17aac0`**（不落池的替身）⇒ **既不撒、也不占槽**。
- ⚠ **两次 `fld` 读的都是 `[pos+0]`**（`0x4400c3` 与 `0x4400d5`，两次 `mov` 都是 `[ebp+0x8]`，都没有位移）⇒ 门判的是**同一个分量**。
  研究稿 §T8.3 把它读作"判 y、`y ∈ (−64,448)`"。**轴名我本轮没钉死**（`Float3` 的分量序按 `spawn_item` 写速度的 `+0x2b0/+0x2b4/+0x2b8` 推是 `{x,y,z}`，那门就是 `x ∈ (−64,448)`，与 TH08 的战场宽度对不上）⇒ **轴名标 🟡**，别把 `y` 当结论搬进别作。
- 门的方向（哪个比较结果算"越界"）我只做了单侧验证 ⇒ 🟡（同研究稿 §T8.3 的自我标注）。

### 1.6 RNG —— **消弹链不消耗随机数**（🔁 订正研究稿 §T8.7/T8.5）

- ① `0x430830`（`0x430830`–`0x43093e`）、② `0x430d30`、④ `0x415c60` **体内零 RNG 调用**。
- `spawn_item`（`0x4400a0`–`0x440467`）体内**以 `0x164d520` 为 this 的取值调用只有 3 组**，且各自被 state 门关住：
  `state==2`（`0x440260` / `0x44027e`，`push 0x43900000`=288.0f / `0x43400000`=192.0f）、
  `state==3`（`0x4402d5` / `0x4402f3`，`push 0x3e4ccccd`=0.2f / `0x3f19999a`=0.6f）、
  `state==5`（`0x440350` / `0x44036e`）。
- **消弹一律传 `state = 1`**（§1.2 / §1.3），落进 `0x440340 cmp [ebp+0x10],5 / jne 0x4403b9` 那条公共尾 ⇒ **不进任何一组取值支**。
- ⇒ 研究稿"✅ 有（间接推断：`spawn_item` 内部有带 float 范围参数的取值函数）"是**在通用路径上推断的**；加上 state 门之后结论反过来。
  🟡 边界：我只扫了本函数体内以 `0x164d520` 为 this 的调用，**别的 RNG 入口（不经该对象）没有排除**。

### 1.7 boss 击破 / 符卡结束 / 关卡切换属哪一档

| 档 | 一手 | 结论 |
| --- | --- | --- |
| **符卡结束** | `Spellcard::ecl_spell_end` `0x4161b0` 的 `0x41620f` 调 `0x430aa0(0x1f40, 1)` | ✅ **清弹 + 撒实体（state=1）+ 冒数字**（同 TH06 的"撒 + 冒分"档） |
| **boss 击破** | `EnemyManager::on_tick` 的 `0x42d926` 调同一发 `0x430aa0(0x1f40, 1)`；触发条件 `0x4178a0(0x4ea670) == 0` | 🟡 **引擎侧确实有"清弹 + 撒道具"这条路**（订正研究稿"引擎侧无对应函数"）；但**这一支是不是"boss 击破"我没钉死** |
| **关卡切换** | §2.1 的调用点穷举里没有换关路径；③ 与 `reset` 这条我本轮没读 | ❓ 未钉死（钉法：读 `Stage`/`Supervisor` 换关调用链，看它走 `0x430aa0` 还是整池 reset） |
| **擦弹（不属消弹）** | `0x43163c`–`0x43168f`（`BulletManager::on_tick`）连发 `type 7`、`state 1` 两颗；另一支把 `*(int32_t*)0x18b8988` 当 type（`0x431676 jl` 守门） | 🟡 **TH08 的擦弹是会撒道具的**（订正研究稿 §T8.1⑤"擦弹消去 ❌ 未找到"）；但这属擦弹侧，不是消弹入口 |

## 2. 验证（怎么排除掉别的路）

### 2.1 调用点穷举（不是抽样）

- `call 0x430830` 全 exe **3 处**：`0x415c6c` / `0x41e002` / `0x42bac2`。
- `call 0x430aa0` **2 处**：`0x41620f` / `0x42d926`。`call 0x430d30` **1 处**：`0x41dff1`。`call 0x415c60` **6 处**（§1.1）。
- 消弹入口没有"函数指针表/虚表"这条路可用：`0x430830`/`0x430aa0`/`0x430d30`/`0x415c60` 这四个地址作 dword 在文件里的出现全部落在 `.text` 的**指令编码**里（`call rel32` 的操作数字节），没有额外的数据引用。
- 弹池与激光池**是两个池**：弹 `0xf6f710`（`0x600`×`0x10b8`）、激光 `this+0x660938`（`0x100`×`0x59c`）⇒ ① ② 只碰弹池，只有 ③ 同时碰两边（§1.3）。

### 2.2 `spawn_item` 参数序（**钉死研究稿 N15**）

```
44022b: mov dl,BYTE PTR [ebp+0xc]  → 44022e: mov BYTE PTR [ecx+0x2d4],dl   ; [ebp+0xc] = type
440237: mov cl,BYTE PTR [ebp+0x10] → 44023a: mov BYTE PTR [eax+0x2d7],cl   ; [ebp+0x10] = state
```
⇒ 签名 = `spawn_item(Float3 *pos /*[ebp+8]*/, int type /*[ebp+0xc]*/, int state /*[ebp+0x10]*/)`，`0x440467 ret 0xc`（3 个栈参）✅
⇒ 所以 §1.2 里"`push 0x1` 在前"= **state=1**、"`push 0x6` 在后"= **type=6**（cdecl 从右往左压）。**研究稿 N15 本轮收口**。

### 2.3 "判定 == 2 ⇒ `type 6`" 这句话站不住（🔁 订正研究稿 §1 表 108 行）

- `push 0x6`（`6a 06`）**只出现在 `0x430d30`**（`0x430dd9`）；① 的 `0x430830` 里**没有**任何字面量 6，它推的是**全局变量** `*(int32_t*)0x18b8988`（`0x4308cf mov ecx,DWORD PTR ds:0x18b8988`）。
- 这个全局：**全 exe 17 处引用全是读**（`0x4308cf` / `0x430b25` / `0x431634` / `0x43166f` / `0x43167a` / `0x4316ee` / `0x431729` / `0x431734` / `0x43181b` / `0x431855` / `0x431860` / `0x43192c` / `0x431967` / `0x431972` / `0x431a3e` / `0x431a78` / `0x431a83`）；
  把它当 dword 在整个 exe 文件里搜（含 `mov ds:…,eax` 型写的编码）也**只命中这 17 条指令自身** ⇒ 没有写点、也没有指针表。
- 它落在 `.data`（`0x4c6000`、virtual size `0x1413840`、**raw size 只有 `0x6e00`**）的**零初始化尾巴**（`0x4c6000+0x6e00 = 0x4cce00` < `0x18b8988`）⇒ **静态初值 0**。
- ⇒ ① 判定==2 支撒出的 type **静态上是 0**（经 `0x44010e` 的"火力满 ⇒ 0/2 改成 8"改写后另算），**不是 6**。🟡 残余可能：用**算出来的基址**间接写这个全局（我这种字面地址扫描抓不到）。
- ⇒ **输出层要改口径**：`engine/bullet/OVERVIEW.md` 的 TH08 行也写着"判定 == 2 ⇒ `type 6`"，与本条冲突（那份文档另有人负责，我**没动**）。

### 2.4 未钉死项（照抄研究稿 §99，**不升级**）

| # | 项 | 本轮状态 | 怎么钉 |
| --- | --- | --- | --- |
| N15 | `spawn_item` 参数序 | ✅ **本轮钉死**（§2.2） | — |
| N16 | 消弹撒出的那一档**每颗多少分 / 给不给时点** | ❓ 仍在（type 号已知：`6` / `*(0x18b8988)` / `*(mgr+0x6ba570)`；**收集侧没读**） | 读 `Item::collect_*` 一族（`0x440cf0` / `0x440e40` / `0x441020` / `0x441170` / `0x4412b0`）与 `ItemManager::on_tick`（`0x440500`）里按 `item+0x2d4` 的分派 —— **这是卡 108 最需要的一手** |
| N17 | 场地门 flag 方向 + **轴名** | 🟡 半结：两次比较都读 `pos[0]`（§1.5）；方向与轴名仍 🟡 | 单值代入（造一颗 `x` 越界 / `y` 越界各一次）或实跑 |
| N18 | `0x430aa0` 全文 / `0x40b500` 本体 | 🟡 ③ 本轮读了（弹池 + 激光池 + 冒分 + 返回值），**`0x40b500`（距离）仍未读本体** | 读 `0x40b500` |
| N19 | RNG | 🔁 本轮**订正为"消弹链不消耗"**（§1.6，带 state 门地址）；🟡 残余：别的 RNG 入口没排除 | 全 `.text` 扫 RNG 助手的 `mov ecx,0x164d520` 清单，与消弹链的所有被调函数对账 |
| 新 | `*(int32_t*)(mgr+0x6ba570)` 这个 **BulletManager 字段**是什么 | ❓ 未定（① 的"判定≠2"支与 ③ 都拿它当 type） | 扫它的全部读写点（本轮只读到读点） |
| 新 | `0x449ff0`（判定查询）的**形状** | ❓ 未读本体（① 的"判定 == 2"到底意味着什么，取决于它） | 读 `0x449ff0` |

## 3. 结论（可信度 + 适用范围）

**一句话**：TH08 的消弹由**四个函数**承载 —— `cancel_all(arg)`（①，全屏、按"炸弹判定区"查询分三支）、**ECL op161 的圆判定**（②）、`sub_430aa0(maxBonus, spawnItems)`（③，弹池 + 激光池 + 撒道具 + 冒数字 2000 起 / 每颗 +20 / 上限 8000）、以及 `cancel_all_1`（④，= ① 的 `arg=1`）；
**撒出的实体一律 `state = 1`、每颗 1 个**，type 分别是 `6`（②）、`*(int32_t*)0x18b8988`（① 判定==2 支，静态 0）、`*(int32_t*)(mgr+0x6ba570)`（① 判定≠2 支与 ③）；
**`arg == 4` 那一支只写 `state=5`、一颗都不出**（= "满屏消失但一颗点都不出"的同族）；
场地门在 `spawn_item` 里、常量 **`−64.0f` / `448.0f`**、判的是 `pos` 的**第 0 个分量**（轴名 🟡）；**消弹链不消耗 RNG**（§1.6）。

- ✅ **一手**：四个入口的函数体与调用点穷举、三条支路的 `cmp`/`push` 真值、`spawn_item` 参数序（`0x44022b`/`0x440237`）、门的两条 `fcomp` 与常量真值（python 直读 PE）、`spawn_item` 三组 RNG 的 state 门、③ 的两池结构与 AsciiManager 冒分。
- 🟡 **中**：① 的"判定"语义（`0x449ff0` 未读）；门的轴名与 flag 方向；`mgr+0x6ba570` 字段；`0x18b8988` 的"无写点"是**字面地址穷举 + 数据搜索**得来的否定（算出来的基址抓不到）；③ 的触发条件（`0x4178a0(0x4ea670)`）。
- ❓ **未解**：N16（每颗多少分）；`0x40b500`；关卡切换那一档；擦弹侧的完整条件。
- **适用范围**：只对本 exe 的这版 build 成立。**别跨作搬**：TH18 的 `mode` 表（`0` 纯删除 / `1` → `type 9` / `4` → `type 9`+`type 2` / `3` 只 inc）与这里的 `arg` **不是一个东西**；TH08 的场地门（`−64`/`448`）也与 `th16:0x…`/`th18:0x…` 的 `x ∉ (−224,224)` 不同形。

## 4. 证据（地址 / 常量 / 复现命令）

```bash
cd local/th08.v1.00d
md5sum th08.exe        # 4608e69e470878a3603cc1352b2ea742
objdump -d -M intel --start-address=0x430830 --stop-address=0x430940 th08.exe  # ① 三条支路
objdump -d -M intel --start-address=0x430aa0 --stop-address=0x430d20 th08.exe  # ③ 弹池 + 激光池 + 冒分
objdump -d -M intel --start-address=0x430d30 --stop-address=0x430e00 th08.exe  # ② 半径 + push 0x6
objdump -d -M intel --start-address=0x415c60 --stop-address=0x415c78 th08.exe  # ④ push 0x1
objdump -d -M intel --start-address=0x4400a0 --stop-address=0x440470 th08.exe  # spawn_item：门 + type/state + 三组 RNG
# 调用点穷举
grep -nE 'call +(0x430830|0x430aa0|0x430d30|0x415c60)' local/_scratch/th08_full.asm
# 各分支归属（vendor 名，仅取名）
#   0x4161b0 Spellcard::ecl_spell_end / 0x42c660 EnemyManager::on_tick
#   0x4152a0 Spellcard::ecl_spell / 0x440cf0 Item::collect_power_item / 0x441170 Item::collect_big_power_item
# 常量真值（python 直读 PE 节区，VA→文件偏移）
#   0x4b48dc = −64.0f   0x4b4940 = 448.0f
# 0x18b8988 的"无写点"：17 处字面引用 + 该 dword 全文件搜索（全部命中 .text 的指令编码）
```

## 5. 对卡的直接影响（只读观察，不改代码）

只读 `mods/th18.v1.00a/card-expand/native/cards/youkai.c`（`grep -n "ce_cancel\|cancel"` ⇒ **0 命中**）与 `pofv.c`（属卡 109，TH09 篇详述）。**本节不改任何代码。**

- **卡 108（`youkai.c`）不碰消弹**：全文件零 `cancel` 命中 ⇒ 本卡的妖侧子机逻辑与"消弹 → 出道具"这条链**没有交点**，本作的一手结论对它是**纯知识**（将来若要加"消弹撒点"，必须按 §1.2 的三条支路自己选 `arg`，且知道 `arg=4` 是不出道具的那一档）。
- **一致性**：`youkai.c` 现状与 TH08 原版**无从对照**（它不调任何消弹入口）⇒ 不构成"不一致"，也不构成"一致"。
- **宿主引擎口径提醒（我们所有卡共用）**：我们的调用面是 TH18 引擎的 `ce_cancel_radius(pos, r, max, mode)`，它的 `mode` 表是 **TH18 自己的**（见 `engine/bullet/th18/01-cancel-to-item.md`），**与 TH08 的 `arg` 不可互推**：
  - TH08：`arg` 落 **state** 位（`0x430909`），且 `arg == 4` ⇒ 不出道具；TH18：`mode` 落"撒什么"位（0 纯删除 / 1 → `type 9` / 4 → `type 9`+`type 2` / 3 只 inc）。
  - 想要"TH08 的一颗都不出"那一下，TH18 侧对应的是 **`mode 0`**；想要"TH08 op161 每颗一颗 `type 6`"，TH18 侧**没有等价物**（没有 `type 6` 这条消弹掉落），只能自己记账 —— 这与 110 / 111 的现做法同族（见 `engine/bullet/th18/01-cancel-to-item.md` §6）。
- **场地门的量纲**：TH08 的 `−64.0f` / `448.0f` 判的是 `pos[0]`；TH18 的门是 `x ∉ (−224,224)` 或 `y ∉ (−32,480)` 才不撒 ⇒ 讨论"像不像原版"时**必须先说清是哪一种门**，别把 `448` 这个两边都有的常量当成同一个东西。
