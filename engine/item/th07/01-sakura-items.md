# item 逆向 01：TH07 樱点道具 —— 类型表、掉落源、一次掉几个
> **版本**：TH07 v1.00b（`th07.exe`，imagebase `0x400000`）。本文所有地址都带 `th07:` 前缀，均为该版本。
>
> **方法**：一手平坦反汇编（`objdump -d -M intel` 产出 `local/_scratch/th07_full.asm`，172,741 行）
> + ExpHP `local/vendor/th-re-data/data/th07.v1.00b/{funcs,labels,statics,type-structs-own}.json` 命名/结构
> + ECL 操作码跳表直读（`th07:0x417838`）+ **自写 ECL 线性扫描器**（`local/_scratch/th07_ecl_scan.py`）
> + `local/vendor/thtk/build/thanm/thanm -l 7` 核对 etama.anm 矩形。日期 2026-09-12。
> **可信度**：§1–§4 的**类型号、地址、数量、调用者**全部 ✅（一手指令 / 一手扫描）；
> §2 的**中文名（点/P/大P/Bomb/1UP/满火力）**是「效果 + 一手调用者」推断，标 🟡；
> §3.3 的**两类 sprite 槽语义**标 🟡；`type 9` 无生成点见 §6 `❓ 未解 1`。

## 0. 一句话结论（**用户关心的判据**）

TH07 的樱点（`樱点`/`+C`）**没有专门的「樱点掉落指令」**。它由三条互不相干的路径送出来：

| 樱点档 | 类型号 | 一次几颗 | 谁给的（一手） |
| --- | --- | --- | --- |
| ★（樱点 & +C 各 `+20`） | **6** | **每只死敌 1 颗**；每颗被消掉的弹 1 颗 | `th07:0x423144`（敌死亡批处理）/ `th07:0x424c00`、`th07:0x424740`（消弹） |
| 小樱点（樱点 `+100` / +C `+30`） | **8** | 每颗被消掉的弹 1 颗 | `th07:0x4247e3`（`0x424740` 的 `mode ≥ 3` 分支，全 exe 唯一入口 `th07:0x420131` = `mode 0xa`） |
| 大樱点（樱点 & +C 各 `1000 + 100×符卡奖励次数`） | **7** | **火力满时**，每一颗 type 0 / type 2 道具都**变成** 1 颗它 | `th07:0x432730`（`spawn_item` 顶部的类型改写） |

⇒ **量化答案**：第八章（最终关）每个 boss/中 boss 死亡段落一次喷 `dropItems(15)` + `dropPointItems(20..40)`
（`th07` ECL 89 处配对，见 §4）；**这些在火力满时全部按 type 7（大樱点，1000+100n）结算**，
即「一次掉 15~40 颗大樱点量级的樱点」。反过来，**★ 与 小樱点 是"按颗"给的：一颗弹 / 一只敌 = 一颗**。

---

## 1. 道具系统（一手）

### 1.1 `ItemManager` 单例与池

| 项 | 值 | 证据 |
| --- | --- | --- |
| 全局对象地址 | `th07:0x575c70`（静态单例，所有调用点都是 `mov ecx,0x575c70`） | `th07:0x4154a1`、`th07:0x4326f0` 的 29 个调用点 |
| 池容量 | **`0x44c` = 1100** 条 | `th07:0x432749` `cmp DWORD PTR [ebp-0x4],0x44c` |
| 每条大小 | **`0x288` = 648 B** | `th07:0x432702` `imul ecx,ecx,0x288` |
| 环形游标 | `ITEM_MANAGER + 0xae2e8`（自增 / 满则回绕 0） | `th07:0x43275f`（+1）/ `th07:0x43278b`（置 0） |
| 池满时返回的替身 | `ITEM_MANAGER + 0xae060` | `th07:0x432972` `add edx,0xae060` |
| 活跃标志 | `item + 0x27d`（0 = 空槽） | `th07:0x43276e` `movsx eax,BYTE PTR [edx+0x27d]`、`th07:0x4327ca` 置 1 |
| 结构体 | ExpHP `zItemManager`（`items[0x44c]` @ `+0x0`，`dummy_item_for_failed_spawns` @ `+0xae060`，`next_index` @ `+0xae2e8`，`list_head_dummy_item` @ `+0xae2f0`） | `type-structs-own.json`，与上面逐条吻合 |

> 池里**没有链表**用于分配（是环形游标）；`zItemManager.list_head_dummy_item` @ `+0xae2f0` 是**绘制/更新**用的链表头。

### 1.2 `zItem` 字段（`0x288` B，ExpHP 结构 + 一手读写点）

| 偏移 | 含义 | 一手证据 |
| --- | --- | --- |
| `+0x0` | 内嵌 `zAnmVm vm`（0x24c B） | `zItem.vm`；`th07:0x432905` 写 `vm.anm_file_index` |
| `+0x24c` | `Float3 pos` | `th07:0x4327d7` 从 spawn 参数整块拷入 |
| `+0x258` | `Float3 velocity` | `th07:0x4327ef`/`th07:0x4327fc`/`th07:0x432809` |
| `+0x264` | `Float3`（第二组向量，只在 `mode == 2` 写） | `th07:0x432873`/`th07:0x432892`/`th07:0x43289b` |
| `+0x270` | `zTimer`（初值 `-999`） | `th07:0x43284e` |
| `+0x27c` | **道具类型**（u8） | `th07:0x432819` 写；`th07:0x432ebe` 读 |
| `+0x27d` | 活跃标志 | `th07:0x4327ca` |
| `+0x27e` | 「本次绘制要重设 sprite」闩锁 | `th07:0x433d88` 清 0 / `th07:0x433e32` 置 1 |
| `+0x27f` | mode（第 3 参数） | `th07:0x432825` 写 |
| `+0x284` | 绘制链 `next` | `th07:0x433e70` |

### 1.3 `ItemManager::spawn_item` `th07:0x4326f0`（**就是它**）

```
Item* ItemManager::spawn_item(Float3* pos /*[ebp+8]*/, int type /*[ebp+0xc]*/, int mode /*[ebp+0x10]*/)
        // cdecl，ecx = ITEM_MANAGER，ret 0xc  →  th07:0x432981
```

函数头三条硬事实：

| 地址 | 指令要点 | 语义 |
| --- | --- | --- |
| `th07:0x432724`–`0x432730` | `cmp [ebp+0xc],0x0` / `cmp [ebp+0xc],0x2` / `mov DWORD PTR [ebp+0xc],0x7` | **火力 ≥ 128 且 type ∈ {0, 2} ⇒ type = 7**（前置 `power >= 0x80` 在 `th07:0x43271d`） |
| `th07:0x4327fc` | `mov DWORD PTR [ecx+0x25c],0xc00ccccd` | 初速 `vy = -2.2f` |
| `th07:0x432905` | `mov WORD PTR [ecx+0x1d8],dx`，`dx = (u16)(type + 0x2c4)` | `vm.anm_file_index = type + 0x2c4` |

`mode`（第 3 参数）在池内被读作 `state`（`+0x27f`）：

| mode | 一手行为 |
| --- | --- |
| 2 | 「从上方撒入」：`+0x264 = rand×288.0f + 48.0f`、`+0x268 = rand×192.0f − 64.0f`、`+0x26c = 0`，并把 `pos` 抄进 `velocity`（`th07:0x432864`–`0x4328c6`） |
| 3 | 强制 `state = 1`（`th07:0x4328d1`） |
| 4 | 强制 `state = 0`（`th07:0x4328e3`） |
| 其他（含实测最常见的 0 / 1） | 直接采用传入值 |

### 1.4 脚本 / sprite 的选择链（**类型 → 图**）

| 步 | 地址 | 内容 |
| --- | --- | --- |
| 1 | `th07:0x432912` | `script = AnmManager->[0x28ef0 + 4*(type + 0x2c4)]` |
| 2 | `th07:0x432921` → `th07:0x44ea20` | `AnmManager::play_script(this, AnmVm* vm, AnmRawInstr* script)`：写 `vm+0x1dc` / `vm+0x1e0`（`beginning_of_function` / `current_instr`，`th07:0x44ea67`/`0x44ea79`） |
| 3 | `th07:0x433d73` / `th07:0x433e1a` | `ItemManager::on_draw__` 每帧按 `type` 选 **两个 sprite 槽之一**（见 §3.3） |
| 4 | `th07:0x44e8e0` | `AnmVm::set_sprite(AnmVm*, int sprite_index)`：`sprites[idx]` 步长 **`0x40`**、基址 `AnmManager + 0x60`（`th07:0x44e8ec` `shl eax,0x6` + `th07:0x44e8f2` `[ecx+eax*1+0x60]`） |

`AnmManager` 单例 = `ds:0x4b9e44`：在 `th07:0x434114` 以 `calloc(0x17e560)` 建（尺寸 = ExpHP `zAnmManager.__end` ✅），`th07:0x434147` 落盘。

---

## 2. 类型号 ↔ 名字（一手 dispatch 表）

`ItemManager::on_tick__` 的收集分派（`th07:0x432ebe`）：

```
type = (int8)item[+0x27c] ;  cmp type,0x9 ;  ja 0x43399d
jmp DWORD PTR [type*4 + 0x433a5a]        // 10 项，直接按 type 索引，无减法
```

表 `th07:0x433a5a` 的 10 个目标**全 exe 无其他跳入点**（逐个地址 grep 无 `jmp/je/call` 引用）
⇒ 类型号 ↔ 分支的对应关系是硬的。

| type | 分支入口 | 一手效果（关键指令） | 名字 |
| --- | --- | --- | --- |
| 0 | `th07:0x432ee5` | 火力<128：按阈值表 `th07:0x49ed74`（8/16/32/48/64/80/96/128）加火力（`th07:0x432f98`）；火力≥128：分数 = `th07:0x49ecf8`[计数 ≤30]（10…300） | 小 P 🟡 |
| 1 | `th07:0x4330d2` | 分数 = 基准 `50000` 的蓝点公式（`th07:0x43310a` / `th07:0x4331c8`）；末尾 `th07:0x42d83a` 续关判定 | 点（蓝点）🟡 |
| 2 | `th07:0x4333ed` | `th07:0x4325e0` 传参 **8** = 火力 +8；`th07:0x424740(mode=1)` 消弹 | 大 P 🟡 |
| 3 | `th07:0x433574` | `if ([GM+0x68] < 8) th07:0x42d612(1)` = **炸弹 +1**；`th07:0x42f4aa(5)` | Bomb 🟡 |
| 4 | `th07:0x4335c4` | `[GM+0x7c] = 0x43000000` = **火力 = 128.0f**（`th07:0x433630`）；分数 +100 | 满火力（Full Power）🟡 |
| 5 | `th07:0x4335b5` | 只调 `th07:0x42d83a`（内部 `[GM+0x5c] < 8` 时 `th07:0x42d5cd(1)`） | 1UP 🟡 |
| 6 | `th07:0x433691` | 分数 = (命/40)×10+300（或 100）；**樱点 +20**（`th07:0x433712` → `th07:0x42f5a2`） | ★ |
| 7 | `th07:0x43375f` | 分数 = 50000 高度折扣；**樱点 & +C 各 +`1000 + 100×[GM+0x1c]`**（`th07:0x433886`–`0x4338d9`） | 大樱点 |
| 8 | `th07:0x433742` | **樱点 +30（`th07:0x433749`）再只加樱点 +70（`th07:0x433755`）= 100** | 小樱点 |
| 9 | `th07:0x4338e3` | 分数 = (命/40)×10+300；**樱点 +100**（`th07:0x433998`） | 樱点 |

三个加法器的语义（一手读点）：

| 函数 | 语义 | 证据 |
| --- | --- | --- |
| `th07:0x42f5a2` | `cherry(+0x961c) += n`，夹到 `cherry_max(+0x9618)`；**delta > 0 时同时给 +C（`+0x9620`）** | `th07:0x42f5bf`/`0x42f5d7`/`th07:0x42f605` |
| `th07:0x42f69f` | 同上但**只加樱点、不动 +C** | `th07:0x42f6b6`、`th07:0x42f6c2`（无 +0x9620 写点） |
| `th07:0x4325e0` | 加火力 | 被 type 0/2 调用（`th07:0x432fc0` 传 1 / `th07:0x43346d` 传 8） |

> 全局：`ds:0x62f888` = `最大樱点`（= `0x626270+0x9618`），`ds:0x62f88c` = `樱点`（`+0x961c`）。
> `th07:0x42f5a2` 内部就是拿 `[ecx+0x961c]` 与 `[ecx+0x9618]` 比较（`th07:0x42f5d7`），
> 所以 `cmp ds:0x62f88c, ds:0x62f888` 就是「樱点是否已满」。

### 2.1 ⚠ 与既有 spec 冲突（以本节为准）

`docs/superpowers/specs/2026-09-12-senro-107-design.md` §4.2 / §2.2 把两档类型号记成了：

| spec 原文 | 实测 | 结论 |
| --- | --- | --- |
| 「樱点（大）道具 **type 2** = `1000 + 100×[globals+0x1c]`，`th07:0x433886`–`0x4338d9`」 | `th07:0x433886`–`0x4338d9` **确实**是那条公式，但它在 **type 7** 的分支里（`th07:0x43375f`–`0x4338de`） | ❌ 类型号应为 **7** |
| 「★ 道具 **type 7** = 樱点 & +C 各 `+20`，`th07:0x43370b`→`0x433712`」 | `th07:0x433712` 在 **type 6** 的分支里（`th07:0x433691`–`0x43373d`） | ❌ 类型号应为 **6** |

⇒ **spec 的这两处类型号整体错位一格**；数值（`1000+100n` / `+20` / `+100`）本身是对的，**地址也对**，
只是「哪个 type 号」错了。卡侧 `senro.c` 用的是 TH18 自己的 item type，不受影响；
但**若将来要按「TH07 type N」对照，必须用本文的表**。

---

## 3. 掉落驱动路径

### 3.1 ECL 侧：三个掉落 opcode（一手 handler）

跳表 `th07:0x417838`（**161 项，index = opcode − 1**，`th07:0x41071b` `sub edx,1` /
`th07:0x410724` `cmp ...,0xa0` / `th07:0x41073a` `jmp [eax*4+0x417838]`）。

| opcode | handler | 参数 | 一手行为 |
| --- | --- | --- | --- |
| **119** `dropItems(n)` | `th07:0x4153a9` | 1 个 int（`[insn+0xc]`） | 循环 n 次：`pos = enemy.pos + (rand×128.0f − 64.0f)`；**火力<128 → 第 1 颗 type 2、其余 type 0**；**火力≥128 → 全部 type 1**（细节见下） |
| **124** `dropItemId(id)` | `th07:0x41570d` | 1 个 int = **type** | 在敌人位置 spawn **1 颗**，type = 参数（`th07:0x41573f` 直接当 type 压栈） |
| **154** `dropPointItems(n)` | `th07:0x4154c9` | 1 个 int | 循环 n 次：同样抖动后 spawn **type 1**（`th07:0x41559a` 固定 push 1） |

op119 的细节（一手）：抖动常数 `th07:0x498b24` = 128.0f、`th07:0x498b08` = 64.0f，
写成 `rand()×128.0f − 64.0f` 分别加到 x / y（`th07:0x41542d`–`0x415476`）；
火力判定 `power >= 0x80` 在 `th07:0x415486`，两支的 push 分别在 `th07:0x415495`（type 2/0）与 `th07:0x4154af`（type 1）。

三个 handler 都用 `[insn+0xa] & 1` 判断「参数是不是变量」（`th07:0x4153ac`），
`th07:0x40e5b0` 求值 → 与 THTK `thecl06.c` 的 `param_mask & 1 ⇒ stack` 一致。

**指令编码（一手，用于写扫描器）**：

```
+0x0 int32 time ; +0x4 int16 opcode ; +0x6 int16 size ; +0x8 uint16 rank ; +0xa uint16 param_mask ; +0xc args[]
推进游标：th07:0x41677b  movsx eax,WORD PTR [edx+0x6] ; add ecx,eax
```

ECL 容器（对齐 `local/vendor/thtk/thecl/thecl06.c` 的 `th06_header_t`，version ≤ 7 无 magic）：
`u16 sub_count; u16 timeline_count; u32 offsets[];`，**sub s 从 `offsets[16+s]` 开始**，
结束哨兵 = `time == 0xffffffff` 或 `size == 0`。

### 3.2 引擎侧：`spawn_item` 的**全部 29 个调用者**

按「push 的 type」分类（type 为动态的地方标出来源）：

| 调用点 | 所在函数 | type / mode | 触发 |
| --- | --- | --- | --- |
| `th07:0x4154a6` | ECL op119 | 动态(2/0) / 0 | `dropItems` |
| `th07:0x4154ba` | ECL op119 | 1 / 0 | `dropItems`（火力满支） |
| `th07:0x4155a5` | ECL op154 | 1 / 0 | `dropPointItems` |
| `th07:0x415755` | ECL op124 | 动态(参数) / 0 | `dropItemId` |
| `th07:0x423144` | 敌死亡批处理 `th07:0x423090` | **6** / 1 | 死敌 1 颗 ★ |
| `th07:0x4231e3` | 同上 | **6** / 1 | 死敌自带 item 槽各 1 颗 ★ |
| `th07:0x4247c9` | `BulletManager::sub_424740` | `[BM+0x37a160]`(=6) / `[ebp+8]` | 消弹（mode<3） |
| `th07:0x4247e3` | 同上 | **8** / 1 | 消弹（**mode ≥ 3**）→ 小樱点 |
| `th07:0x424948` `th07:0x42495c` | 同上（`sub_424740` 第二处） | 同上 | 同上 |
| `th07:0x424a1a` `0x424b44` `0x424bbd` | `BulletManager::sub_4249a0` | `[BM+0x37a160]`(=6) / 1 | 矩形/圆域消弹 |
| `th07:0x424cf6` | `BulletManager::sub_424c00` | **6** / 1 | **半径消弹**（ECL op143） |
| `th07:0x425e6f` `0x425ee0` | `BulletManager::on_tick_0c` | `ds:0x4bfedc`(**=0**，BSS 无写点) / 1 | 特殊弹自毁 |
| `th07:0x421aad` | `EnemyManager::on_tick_0a` | `enemy[+0x2e10]` / 动态 | 敌人死亡掉「enmCreate 指定」的道具 |
| `th07:0x421b31` | 同上 | `[0x49efa0][计数]` / 动态 | 32 项轮转表（见 §3.4） |
| `th07:0x440df1` | `Player::sub_440cf0` | 2 / 2 | 死亡撒道具：1 颗大 P |
| `th07:0x440e09`–`0x440e67` | 同上 | 0 / 2 | 死亡撒道具：5 颗小 P |
| `th07:0x440f46`–`0x440fa5` | 同上 | 4 / 2 | 另一支：5 颗满火力 |

**★（type 6）的两大来源，逐指令**：

1. **敌机死亡** `th07:0x423090`（`ecx = EnemyManager @ th07:0x9a9b00`，`[ebp+8]`/`[ebp+0xc]` 两个 int）：
   扫 480 个敌槽（步长 `0x4f48`，`th07:0x4230ce`）；对每只 `enemy[+0x2e28] >> 7 & 1`（死）
   且 `enemy[+0x2e29] >> 6 & 1 == 0`（未结算）的敌机：若 `enemy[+0x2e29] >> 5 & 1` 为真 ⇒
   **`spawn_item(enemy.pos, 6, 1)` × 1**（`th07:0x423131`–`0x423144`）；若 `enemy[+0x4f30] != 0`
   再按 `(int16)enemy[+0x4f32]` 为上限、`+0x1c` 步长的槽（`enemy+0x2f78 + i*0x1c`，循环步进 6）
   **各 1 颗 type 6**（`th07:0x4231c9`–`0x4231e3`）。调用者四处：
   `th07:0x421bd4`（`EnemyManager::on_tick_0a` 每帧扫）、`th07:0x4101f3`（符卡结束）、
   `th07:0x415b9d`（**ECL op94 `enmKillAll`**）、`th07:0x429b2d`（关卡收尾）。
2. **消弹**：type 取自 **`BulletManager(+0x37a160)`**，构造时写死 **6**（`th07:0x423314`，`BulletManager` = `th07:0x62f958`）。
   - 半径消弹 `th07:0x424c00`（= **ECL op143 `etStarClearRadius`**，`th07:0x41627a`）：每颗在半径内的弹 **1 颗 type 6**（`th07:0x424cf6`）。
   - 全屏消弹 `th07:0x424740(mode=1)`：每颗活弹 **1 颗 type 6**（`th07:0x4247c9`）。mode=1 的调用者含 ECL op79 `etCancel`（`th07:0x415c17`）、符卡宣言（`th07:0x40fcfd`）、收大 P / 收满火力（`th07:0x4334ad` / `th07:0x4335e0`）。
   - 域消弹 `th07:0x4249a0(mode=1)`：每颗弹 **1 颗 type 6**（`th07:0x424a1a`）。

**小樱点（type 8）的唯一来源**：`th07:0x424740` 的 `mode ≥ 3` 分支（`th07:0x4247a7` `cmp [ebp+8],3` → `jge`），
每颗弹 **1 颗 type 8**。全 exe 只有 **一个** mode ≥ 3 的调用：

```
th07:0x42012a:  push 0xa        ; mode = 10
th07:0x42012c:  mov  ecx,0x62f958
th07:0x420131:  call 0x424740
```

它位于 boss 死亡 / 符卡结束序列（前置门 `enemy[+0x2e2a] >> 6 & 1` 与状态机计数 `ds:0x12fe0c8`，`th07:0x4200ec`–`0x420124`）。

**大樱点（type 7）的两个来源**：

| 来源 | 地址 | 说明 |
| --- | --- | --- |
| `spawn_item` 顶部改写 | `th07:0x432724`–`0x432730` | **火力 ≥ 128 且 type ∈ {0,2} ⇒ 7**。于是「满火力时敌机掉的 P/大 P 类道具全部变成大樱点」 |
| 敌机轮转表 | `th07:0x421b31`，表 `th07:0x49efa0`，计数器 wrap 于 **0x20**（`th07:0x421b61` `cmp edx,0x20`） | 表前 32 字节 = `0,0,1,0,1,0,0,7,1,1,0,0,7,1,1,0,1,0,1,0,1,0,1,0,1,0,7,1,1,1,0,2`；值域只有 **{0,1,2,7}** |

### 3.3 三档樱点各有独立图（§③ 交叉印证）

anm 定位（一手）：`data/etama.anm` 由 **`BulletManager::on_registration`** 加载：
`th07:0x426f9c`–`0x426fae` `push 0x200; push "data/etama.anm"(th07:0x498524); push 0xb; ecx = AnmManager; call th07:0x44df90`。

`th07:0x44df90` → `th07:0x44e070` 的全局编号规则（一手）：

```
sprite 全局号 = sprite.id + base        ; th07:0x44e3ca (写 th07:0x44e780)
script 全局号 = script.id + base        ; th07:0x44e47f (写 AnmManager+0x28ef0+4*id)
下一个 entry 的 base = 本 entry base + max(sprite.id, script.id)   ; th07:0x44e05e / th07:0x44e46b
```

`thanm -l 7 etama.anm` 读出的 id 范围 ⇒ entry0（etama.png）base `0x200`（= 传入值），max id 167 ⇒
entry1（etama2.png）base **`0x2A7`**，max id 62 ⇒ entry2/3 base `0x2E5`。

> **★ 2026-09-12 订正（做 107 结界特效时独立复核，详见 [`engine/anm/th07/01-anm-id-space.md`](../../anm/th07/01-anm-id-space.md)）**：
> 本节公式**漏了 `+1`**，而且「前进量 = sprite 数」**只是特例**。一手的精确形态是
> **`下一个 entry 的 base = 本 entry base + max(全部 sprite id ∪ 全部 script id, 下限 0) + 1`**
> （`+1` 在 `` `th07:0x44e4c6` ``–`` `th07:0x44e4c9` ``；两个取大点在 `` `th07:0x44e3b8` ``/`` `th07:0x44e459` ``；
> base 相加在 `` `th07:0x44e05e` `` —— 本节原先写的 `` `th07:0x44e05b` `` 是它上一条 `mov`）。
> 63 个零售 `.anm` / 101 个 entry 里 **25 个**「前进量 ≠ sprite 数」（最极端 `stg6enm.anm` entry0 前进 **147** 而只有 79 个 sprite）。
> ⇒ **本节的 base 与**派生的 raw / local 号**整体差 1**：
> `etama.anm` 的 base 真值是 **512 / 680（`0x2A8`）/ 730（`0x2DA`）/ 731（`0x2DB`）**（sprite 数 168 / 50 / 1 / 1），
> 两条独立佐证：① 道具那 10 支 = 全局 708..717 必须是 entry1 的 id **28**..37（按 679 计要 id 38）；
> ② [`engine/anm/th07/01-anm-id-space.md`](../../anm/th07/01-anm-id-space.md) §4.4 的 type `0x1d`→**690** = 680 + 10。
> 本节里**全局号**（`0x2c4 + type` 那一族）**全对**，只有「相对 entry 的 raw/local 号」需要 **−1**。

因此：

| 量 | 公式 | 落点 |
| --- | --- | --- |
| `vm.anm_file_index` | `type + 0x2c4` | `th07:0x432905` |
| 初始脚本 | `AnmManager[0x28ef0 + 4*(type+0x2c4)]` = **etama2.png 的 raw script `29 + type`** | `th07:0x432912` |
| 绘制 sprite（线下方） | `AnmVm::set_sprite(type + 0x2ac)` = etama2.png **local `5 + type`** | `th07:0x433e1a` |
| 绘制 sprite（线上方） | `AnmVm::set_sprite(type + 0x2b6)` = etama2.png **local `15 + type`** | `th07:0x433d73` |

⇒ **三档樱点确实各自占一个独立的类型槽 / 脚本槽 / sprite 槽** ✅，
但 ⚠ **anm 里有两处矩形是重复的**（etama2.png `local 11` 与 `local 13` 都是 `(112,64,16,16)`；
`local 21/22/23` 都是 `(240,64,16,16)`）⇒ **type 6 与 type 8 的「线下方」图指向同一块 16×16 像素**，
「线上方」三者亦同图。是否真的等价需要看图（本轮没能看图，见 §6 `❓ 未解 3`）。

| type | 名字 | 初始脚本 raw id | 脚本首帧 sprite（local / 矩形） | on_draw 线下方 (local/矩形) | on_draw 线上方 (local/矩形) |
| --- | --- | --- | --- | --- | --- |
| 6 | ★ | 35 | 28 → (128,112,64,64) | 11 → (112,64,16,16) | 21 → (240,64,16,16) |
| 7 | 大樱点 | 36 | 29 → (1,241,14,14) | 12 → (0,80,16,16) | 22 → (240,64,16,16) |
| 8 | 小樱点 | 37 | 33 → (65,241,14,14) | 13 → (112,64,16,16) | 23 → (240,64,16,16) |
| 9 | 樱点 | 38 | 37 → (129,241,14,14) | 14 → (128,64,16,16) | 24 → (193,177,62,62) |

> 图源：`thanm -l 7 local/th07.v1.00b/dat/etama.anm` 的 `entry1 (data/etama/etama2.png)` 段。
> `on_draw` 的两个槽由 `item[+0x27e]` 闩锁切换（`th07:0x433d67` / `th07:0x433e0e`），
> 判据是 `item.pos.y` 对 `th07:0x498c70` = **-8.0f** 的比较（`th07:0x433d35`）。🟡 两个槽的**语义**（大小/明暗）本文未定名。

### 3.4 ECL 常量与开关（顺手采到）

| 地址 | 值 | 用途（一手） |
| --- | --- | --- |
| `th07:0x49ed74` | 8,16,32,48,64,80,96,128 | type 0 的加火力阈值表（`th07:0x432f98`） |
| `th07:0x49ecf8` | 10,20,30,…,80 | type 0 满火力时的分数档（计数 `ds:0x62f644` ≤ 30，`th07:0x432f27`） |
| `th07:0x49efa0` | 32 字节，值域 {0,1,2,7} | 敌机轮转掉落表（`th07:0x421b1b`） |
| `th07:0x4d44f8` | 全 exe 只读、无写点 | type 6 的 +20 与 +10/+10-only 分支开关（`th07:0x433702`）🟡 身份未解 |
| `th07:0x4bfedc` | 在 .data 的 BSS 段（无初始化、无写点）⇒ **恒 0** | `BulletManager::on_tick_0c` 自毁弹掉的道具 type ⇒ 实际是 type 0 |

---

## 4. ECL 量化（一手扫描）

工具：`local/_scratch/th07_ecl_scan.py`（自写；格式见 §3.1）。
覆盖 `local/th07.v1.00b/dat/ecldata1..8.ecl` 全部 8 关，
**共 19,919 条指令、0 条解析失败**，sub 数 58/77/60/153/78/74/126/136。

### 4.1 三个掉落 opcode 的出现次数

| opcode | 全 8 关次数 | 参数取值 |
| --- | --- | --- |
| 119 `dropItems(n)` | **80** | `5`（关卡 1–3 主流）、`7`、`6`、`8`、`10`、`15`（关卡 7/8 主流）、`4`；另有 5 处是变量 `$I3` |
| 154 `dropPointItems(n)` | **80** | `1`–`40`，关卡 8 最大到 `40` |
| 124 `dropItemId(id)` | **19** | 只有 **3**（Bomb）与 **5**（1UP），另 2 处是变量 `$I0` |

**op119 与 op154 成对出现**（同一 sub 内相邻两条），80 : 80 完全相等。
按关卡的第二条参数分布（`dropItems` → `dropPointItems`）：

| 关卡 | 典型对 | 备注 |
| --- | --- | --- |
| 1 | 5→5, 5→3 | sub0/sub27/sub39 |
| 2 | 5→7, 5→6, 5→8 | sub46/53/68/70/75 |
| 3 | 7→3, 7→8, 6→6, 6→8 | sub18/20/36/37/56 |
| 4 | 4→2（×8 处）, 8→5, 8→8, 4→2, 6→4… | 中 boss 密集 |
| 5/6 | 4→3, 4→1, 7→7, 10→5, 8→6, 10→10 | |
| 7 | **15→20/25/30**, 7→7, 10→7, 4→3 | 最终关 |
| 8 | **15→20/25/30/40**, 7→7, 10→7, 4→3 | 最终关 |

### 4.2 「哪些 ECL 掉樱点一族」

**没有任何 ECL opcode 直接掉 type 6 / 7 / 8 / 9** ✅（一手：三个 handler 的 push 立即数已逐一读出）。
樱点一族的产出**全部在引擎侧**（§3.2）。ECL 间接影响它的唯一接口是：

| 接口 | 一手 | 效果 |
| --- | --- | --- |
| `enmCreate` / `enmCreateA`（op 93 / 92）的**第 6 参数** | `th07:0x41f535`（helper `th07:0x41f430`，入参 `[ebp+0x14]`）→ `enemy[+0x2e10]`；`EnemyManager::on_tick_0a` 在 `th07:0x421aad` 用它当 type | 敌人死亡时掉 1 颗指定 type 的道具 |
| 火力（`power`） | `th07:0x432724` | 火力满 ⇒ **type 0/2 → 7（大樱点）**，op119 的 P 类掉落变成 type 1 |
| `etCancel`(op79) / `etClear`(op146) / `etStarClearRadius`(op143) / `enmKillAll`(op94) | `th07:0x415c17`(mode 1) / `th07:0x416349`(mode 0) / `th07:0x41627a` / `th07:0x415b9d` | 分别掉 ★ / 不掉 / ★ / 死敌 ★ |

`enmCreate` 第 6 参数的实测取值分布（全 8 关，359 次）：

| 值 | 次数 | 含义 |
| --- | --- | --- |
| −2 | 227 | 不指定（走 §3.4 的轮转表） |
| −1 | 24 | 不指定（同样走轮转表） |
| 0 | 22 | 小 P（火力满时变 type 7 = 大樱点） |
| 1 | 16 | 点（蓝点） |
| 2 | 28 | 大 P（火力满时变 type 7 = 大樱点） |
| `$I`(10070) | 42 | **变量**（`param_mask` bit5 = 1，`th07:0x4159f6` 同族判定）⇒ **运行期任意 type** ❓ 见 §6 `❓ 未解 2` |

⇒ **ECL 从不显式掉 ★/小樱点/大樱点**（三个掉落 opcode 的立即数已逐一读出）；
但 `enmCreate` 的掉落 type 在 **42 处是变量**，所以「ECL 能让敌人掉出 type 6/7/8/9」在理论上**没被排除**。
玩家实际拿到的樱点里，**★ 来自「打死敌机 + 消弹」、小樱点来自「boss 死亡时那颗 `0xa` 的消弹」、
大樱点来自「火力满之后所有 P 类掉落」** —— 这三条是有地址的一手路径。

---

## 5. 结论表（可信度 + 版本）

| # | 断言 | 可信度 | 版本 |
| --- | --- | --- | --- |
| 1 | `ItemManager` 单例 = `th07:0x575c70`，池 `0x44c`×`0x288` B，环形游标 `+0xae2e8` | ✅ 一手 | v1.00b |
| 2 | `spawn_item(Float3*, type, mode)` @ `th07:0x4326f0`（cdecl / `ret 0xc` / ecx = manager） | ✅ 一手 | v1.00b |
| 3 | 火力 ≥ 128 时 type 0 与 2 被改写成 **7** | ✅ 一手（`th07:0x432730`） | v1.00b |
| 4 | 类型分派表 `th07:0x433a5a`（10 项，按 type 直索引，无第二跳入点） | ✅ 一手 | v1.00b |
| 5 | type 6 = ★（+20）/ type 7 = 大樱点（1000+100n）/ type 8 = 小樱点（+100）/ type 9 = 樱点（+100） | ✅ 效果与类型号一手；**名字 🟡** | v1.00b |
| 6 | 道具图 = `data/etama.anm` entry1（etama2.png），sprite = `0x2ac+type` / `0x2b6+type`，脚本 raw id `29+type` | ✅ 一手（3 档独立槽）；sprite 语义 🟡 | v1.00b |
| 7 | ECL 掉落只有 op119 / op154 / op124，**三者的立即数里没有樱点一族**（19,919 条指令全扫；`enmCreate` 的 42 处变量参数未排除，见 §6 `❓ 未解 2`） | ✅ 一手 | v1.00b |
| 8 | ★ 来源 = 敌机死亡（每只 1 颗 + item 槽各 1 颗）与消弹（每颗弹 1 颗） | ✅ 一手 | v1.00b |
| 9 | 小樱点来源 = `th07:0x424740(mode ≥ 3)`，全 exe 唯一入口 `th07:0x420131`（mode `0xa`） | ✅ 一手 | v1.00b |
| 10 | 大樱点来源 = `spawn_item` 满火力改写 + 轮转表 `th07:0x49efa0` | ✅ 一手 | v1.00b |
| 11 | 一次掉几个：关卡 7/8 中 boss 段 `dropItems(15)` + `dropPointItems(20..40)`；关卡 1 只有 `5` | ✅ 一手扫描 | v1.00b |
| 12 | `enmCreate` 第 6 参数 = 敌人死亡掉落 type | ✅ 一手（`th07:0x41f535`） | v1.00b |

## 6. 未解 / 待办

- `❓ 未解 1`：**type 9（樱点 +100）全 exe 找不到生成点**。29 个 `spawn_item` 调用点 + 4 个动态 type 来源
  （`[BM+0x37a160]` 恒 6、`enemy[+0x2e10]` ∈ {−2,−1,0,1,2,$I}、`[0x49efa0]` ∈ {0,1,2,7}、`ds:0x4bfedc` 恒 0）
  全部枚举过。**推测是死代码**（ZUN 保留但未用），但未证。
- `❓ 未解 2`：`enmCreate` 第 6 参数出现 `10070` 共 42 次，且 `param_mask` bit5 = 1 ⇒ **它是变量引用**
  （ECL 变量编码 `10000 + 4n + t` ⇒ n = 17）。**运行期有可能取 6/7/8/9 任一值**。
  ⇒ 「ECL 能不能间接掉出樱点一族」本文**没有证否**，只证了「三个掉落 opcode 的立即数里没有」。
  要定案得跑样本读 `enemy[+0x2e10]`，或在 `th07:0x41f535` 下断点看 `[ebp+0x14]`。
  同理 `dropItems` / `dropPointItems` 各有 5 处参数是变量（`10003`，pmask = 1，全在每关 `sub0`）。
- `❓ 未解 3`：etama2.png 里 `local 11` 与 `local 13` 矩形完全相同（`local 21/22/23` 亦同），
  故「★ 与 小樱点 的图是否真的不同」**没能用像素确认**（本会话模型不吃图，无法看图；
  `thanm` 只给矩形）。要定案得看图或跑样本。
- `❓ 未解 4`：`th07:0x4d44f8`（type 6 的 +20 / +10 分支开关）与 `ds:0x12fe0c8`（boss 死亡状态机计数）
  的全 exe 写点未找到（前者在 `.text` 里只有读）。
- `❓ 未解 5`：`EnemyManager::on_tick_0a` 里轮转计数器的宿主对象 `[ebp-0x1e8]`（字段 `+0x9545b8` / `+0x9545ba`）
  未定名。

## 附录 A：复现命令

```bash
# 1) ECL 掉落扫描（自写；容器与指令格式见 §3.1）
python3 local/_scratch/th07_ecl_scan.py
#    → 输出每关 sub 数 / 指令数 / 掉落 opcode 计数，并落盘
#      local/_scratch/th07_ecl_drops.json（含全部 hit 明细）

# 2) 道具图核对（矩形 / script 首帧 sprite）
local/vendor/thtk/build/thanm/thanm -l 7 local/th07.v1.00b/dat/etama.anm | less
#    entry1 = data/etama/etama2.png；item sprite = local 5+type / 15+type
#    script = local raw id 29+type

# 3) 一手反汇编（已存在，勿重跑）
#    objdump -d -M intel local/th07.v1.00b/th07.exe > local/_scratch/th07_full.asm
```
