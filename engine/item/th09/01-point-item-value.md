# item 逆向 01：TH09 没有「点道具（蓝点）」—— 收点分值的载体其实是每 side 一个分数对象
> **版本**：TH09 v1.50a（`local/th09.v1.50a/th09.exe`，685,056 B，imagebase `0x400000`；同目录只有 `th09.dat` 88,180,006 B）。
> **样本选择**：`ls local/th09.v1.50a/` 只有 `th09.exe` + `th09.dat` 两项，**没有第二个 exe 变体**，本文一律用 `th09.exe`。
> 本文所有地址都带 `th09:` 前缀，全部属该版本。
>
> **方法**：自写 PE 解节区（`.text` RVA `0x1000` / 文件 `0x400` / 576,512 B）→
> `objdump -D -b binary -m i386 -M intel --adjust-vma=0x401000 /tmp/_th09_text.bin > /tmp/th09_full.asm`（190,368 行）
> + `local/vendor/thtk/build/thdat/thdat -l 9 th09.dat` 清点资源 + 直读 `.rdata` 常量 +
> `local/vendor/th-re-data/data/th09.v1.50a/*.json` 命名（**只当线索**）。日期 2026-09-18。**没用 Ghidra**。
> **可信度**：§1 的**否定结论**= ✅（口径见 §1.6：对象名册 + 堆分配普查 + 资源清点 + 字符串 + 生成口全扫）；
> §2/§3 的**分数载体、加分函数、调用点**= ✅（一手地址 + 调用点穷举）；§4 的 **Lv 倍率**= ✅（算式与参数一手），
> **Lv 的中文名（「等级」）**与 `ds:0x4a7ea8` 的语义 🟡；§5 各行 bonus 的**中文含义**❓。

## 0. 一句话结论

**TH09 里不存在「点道具（蓝点）」这种东西** —— 没有道具实体、没有道具池、没有道具类型号、没有 item.anm、
没有任何「生成一个可收道具」的入口。所以「蓝点被收时的分值」在 TH09 **没有对应物**（不是「没查到」，是**没有**，口径见 §1.6）。

TH09 里叫「点」的是一个**数值**，不是实体：每方（side）一份 `zPlayer` 内嵌的得点管理器 `player+0x30410`，
它的 `+0x10` 是「点」数（HUD 上那串 6 位数），周期性 `score += 点数/5` 入账（`th09:0x41d7e0`）。

真正**会随时间增长**的是 **`ds:0x4a7e44`（本文读作 Lv，🟡）**：它按难度给初值、每 N 帧 +1、有上限，
并直接进入每次加分的算式 `加点 = base + base×Lv/5`（`th09:0x41d150`）。
⇒ 若要在 TH06–TH09 里找「PIV 的前身」，**TH09 的这个 Lv 倍率**比任何「点道具价值」都更接近，但它作用在**加分**上，不作用在道具上。

## 1. 否定结论：TH09 没有点道具 —— 穷举口径

### 1.1 对象名册（一手）

`GAME_MANAGER` `th09:0x4a7d90`（`zGameManager`）的全部字段：

| 偏移 | 内容 | 一手来源 |
| --- | --- | --- |
| `+0x0` / `+0x38` / `+0x70` | 三个 `zGameManagerSide`（第三份给 EffectSys 用） | 步长 `0x38` 见 `th09:0x415e8b` `imul edi,edi,0x38` |
| `+0xa8` | `gui*` | `th09:0x417bac` 等 |
| `+0xac` | `ex_attack_manager*` | `th09:0x41b2ee` 等 |

`zGameManagerSide`（`0x38` B）**七个对象槽 + 一个链表 next**，全部有主：

| 偏移 | 内容 | 谁填的（一手） |
| --- | --- | --- |
| `+0x0 stage` | `zStage` | `th09:0x403afb` `malloc 0x65c4` |
| `+0x4 player` | `zPlayer` | `th09:0x41f18d` `malloc 0x30f70` |
| `+0x8 bullet_manager` | `zBulletManager` | `th09:0x4150eb` `malloc 0x25e1c0` |
| `+0xc effect_sys` | `zEffectSys` | `th09:0x40d326` `malloc 0x2dc` |
| `+0x10 enemy_manager` | `zEnemyManager` | `th09:0x41225c` `malloc 0x2ac450` |
| `+0x14 spellcard` | `zSpellcard` | `th09:0x40461b` `malloc 0x1368` |
| `+0x18 gui_side` | `zGuiSide`（×3，每 side 一个） | `th09:0x41a5eb` `malloc 0xabe8` |
| `+0x1c` | **堆对象**（本文读作分数对象，§2） | `th09:0x41b082` `malloc 0xa0`；`[side+0x1c]` 作链表 next 用，终止哨兵 `0x4a7e1c`（`th09:0x41b093`） |

**这张表里没有「item manager」这一格** —— 而 TH06/07/08 的道具池都是**独立单例**（TH06 是 `th06:0x69e268` 的静态池），
TH09 一个都没给。`zGameManagerSide` 是完整的 8 格，没有富余字段能藏一个道具管理器。

### 1.2 堆分配普查（全 exe 的 `call 0x47b24e` = malloc + 全部 `operator new`）

把每个 `malloc(size)` 调用点连同 size 列出（`.text` 全量扫描），
**除了库（msg/audio/jpeg）以外，TH09 运行期的对象只有这些**：

| 分配点 | size | 对象 |
| --- | --- | --- |
| `th09:0x403afb` | `0x65c4` | `zStage` |
| `th09:0x40461b` | `0x1368` | `zSpellcard` |
| `th09:0x40d326` | `0x2dc` | `zEffectSys` |
| `th09:0x40f099` | `0x2ab8` | `zEnding` |
| `th09:0x41225c` | `0x2ac450` | `zEnemyManager`（含敌机池） |
| `th09:0x4150eb` | `0x25e1c0` | `zBulletManager`（含弹池） |
| `th09:0x4157e9` | `0x4c80` | `zExAttackManager` |
| `th09:0x418859` | `0x1293c` | `zGui` |
| `th09:0x41a5eb` | `0xabe8` | `zGuiSide` ×3 |
| `th09:0x41b021` | `0xcc` | 单例小对象（`ds:0x4a7e78`；`+0xac` 有个字节计数） |
| `th09:0x41b082` | **`0xa0`** | **每 side 的分数对象**（§2） |
| `th09:0x41f18d` | `0x30f70` | `zPlayer` |
| `th09:0x421169` / `0x421681` / `0x420368` | `0x1ec` / `0x164` / `0x18` | ReplayManager 相关 |
| `th09:0x429533` / `0x429f4d` | `0x24cdc` | MainMenu |
| `th09:0x42ab26` | `0x1b388` | `zPofv1b388`（vendor 已命名） |

⇒ **没有一个「道具池」尺寸的分配**（TH06 = `512×0x144` 的静态池；TH09 最大的池是弹/敌机）。
⇒ 唯一按 side 复制的 `0xa0` 对象是分数对象，没有位置/速度/anm 字段（§2）。

### 1.3 资源清点（`thdat -l 9 th09.dat`）

| 项 | 结果 |
| --- | --- |
| `item.anm` | **不存在**（TH06/07/08 的点道具都走 item/etama 图） |
| `etama.anm` | **存在**，但引用点只有 3 处：`th09:0x40d1db`（EffectSys）、`th09:0x4125e7`、`th09:0x413e62`（BulletManager）⇒ **是弹/特效图，不是道具图** |
| 逐角色资源 | `pl00.anm`..`pl15.anm` + `pl*_match.msg` + `pl*.ecl`、`enemy.anm`/`enemy1.anm`/`enemy13.anm`、`world*.anm`、`front.anm`、`ascii.anm`、`text.anm` |

### 1.4 字符串清点（`.rdata` 全扫）

TH09 的 `.rdata`（VA `0x48e000`，文件 `0x8d000`）里
**没有任何 `item` / `point` / `PIV` / `BONUS` / `Point Item` 字样的字符串**；
分数相关只有：`'%7d'`（VA `0x48eeb0`）、`'%8d'`（`0x48eea8`）、`'%s %.8s %.9d%d'`（`0x48f618`，分数表）、
`'A%7d'`（`0x48eeaf`，前面 1 字节颜色控制）。对比 TH06 有 `'    * Point Item %3d'`。
⇒ **HUD 上根本没有「Point Item」这一行**：TH09 不给道具计数。

### 1.5 类型/结构层

`local/vendor/th-re-data/data/th09.v1.50a/type-structs-own.json` 里
**没有 `zItem` / `zItemManager`**（TH06 有这两个）；
TH09 里名字带 Item 的只有 `zExAttackManagerItem`（`0x4c` B，只有 `__unknown` 与
`timer`@`+0x10`，**没有位置、没有 anm、没有速度** ⇒ 是 EX 攻击的状态槽，不是可收实体）。

### 1.6 一句话穷举口径（写清搜了什么）

1. **对象名册**：`GAME_MANAGER` 三个 side × 8 格全部兑现（§1.1），无 item 槽；
2. **堆普查**：全 exe 的 `malloc` 调用点 + `operator new` 家族逐个看 size（§1.2），无道具池；
3. **资源**：`thdat -l 9` 全集（§1.3），无 item 图；
4. **字符串**：`.rdata` 全扫（§1.4），无 item/point 字样；
5. **生成口**：TH06 的道具生成口是 `ItemManager::spawn_item`（`th06:0x41f290`，`ret 0xc`，23 个调用点）。
   TH09 **不存在这个函数**（对象名册里没有它，堆普查里没有它）；能「在坐标处造实体」的入口只有三个：
   `BulletManager::shoot_one_bullet` `th09:0x412960`、`EffectManager::sub_40ccc0_create_something` `th09:0x40ccc0`、
   `ExAttackManager`（`th09:0x415340` 起的 tick）——**三者都不产出可收实体**；
6. **反向判据**：TH09 里「玩家吃分」的全部路径都直接改**分数对象**（§3 的 7 个调用点），
   **没有一条**先造实体再收。

## 2. 分数字在哪：每 side 一个 `0xa0` 堆对象

```
th09:0x41b069  mov  esi,0x4a7dac                 ; &GAME_MANAGER.side_1 + 0x1c（+0x38 递增到 0x4a7e1c 为止）
th09:0x41b07d  push 0xa0 / th09:0x41b082  call malloc
th09:0x41b08a  mov  [esi],edi                    ; ★ [side+0x1c] = 该对象
```

| 偏移 | 本文读法 | 一手证据 |
| --- | --- | --- |
| `+0x0` | float（🟡 与结算 bonus 有关；`th09:0x41b0c2` 写、`th09:0x415f5c` 读） | `th09:0x41b0c2` `fstp DWORD PTR [eax]` |
| `+0x4` | **显示用分数** | `th09:0x41a847` `mov [eax+0x4],esi`（从 `+0x8` 拷） |
| `+0x8` | ★ **真实分数（累加器）** | `th09:0x40f860` `add [ecx+0x8],eax` |
| `+0xc` | ❓（回放里与 `+0x4/+0x8` 一起被清零：`th09:0x435289`） | — |
| `+0x10` | **EXTEND 次数** | `th09:0x41b13a`：`score >= 1000000 + 2000000×n` ⇒ extend |
| `+0x14` | 字节计数（`inc BYTE PTR [eax+0x14]`，`th09:0x435265`） | — |
| `+0x8` 的钳位 | ★ **999,999,999**（`0x3b9ac9ff`；`0x3b9aca00` = 1e9 是判据） | `th09:0x41a82c`–`0x41a83b` |

EXTEND 判据（一手，两套模式）：

| 模式 | 判据 | 地址 |
| --- | --- | --- |
| `ds:0x4a7ea8 == 0` | `score ≥ 1000000 + 2000000×extend_count` | `th09:0x41b13a`–`th09:0x41b153` |
| `ds:0x4a7ea8 == 1` | `score ≥ (extend_count+1) × 500000` | `th09:0x41b161`–`th09:0x41b17a` |

## 3. 加分的唯一函数与全部调用点

```
th09:0x40f860  mov edx,[ebp+0x8]      ; amount
th09:0x40f866  mov ecx,[ecx+0x1c]     ; ecx = zGameManagerSide* ⇒ 取 side 的分数对象
th09:0x40f869  ... imul 0x66666667 ... ; /5（魔数除法）
th09:0x40f87a  add DWORD PTR [ecx+0x8],eax   ; ★ score += amount/5
```

⇒ **所有加分都是「某个 amount 的 1/5」**。调用点全集（全 exe 7 处，逐个回看压栈）：

| 调用点 | amount 来源 | 场景 |
| --- | --- | --- |
| `th09:0x411356` / `th09:0x411382` | `[enemy+0x2e54]`（敌机字段；`th09:0x40fb9d` 把它设成 `0x64`=100 ⇒ +20） | 敌机被击破/命中结算 |
| `th09:0x416071` | `[gui+0x11ea0]` = **关卡结算 bonus 总和**（§5） | 比赛/关卡结束结算 |
| `th09:0x41d1dc` | `0x1388`（5000 ⇒ +1000） | 「点」累加器到顶（999990）时的补偿 |
| `th09:0x41d7c7` / `th09:0x41d7f1` | `[sm+0x10]` = **「点」数**（§4.3） | 「点」入账（flush） |
| `th09:0x41e307` | `0x32`（50 ⇒ +10） | 玩家侧事件（`th09:0x41e2c5` 起） |

## 4. TH09 的「会增长的分值」：Lv 倍率 + 连击 + 「点」

### 4.1 ★ Lv `ds:0x4a7e44`：随时间增长、且直接乘进加分

```
th09:0x41ac67  inc DWORD PTR ds:0x4a7e40      ; 帧计数
th09:0x41ac6d  ecx = ds:0x4a7e54             ; 间隔
th09:0x41ac73  eax = [0x4a7e40]; edx = eax % ecx
th09:0x41ac7d  cmp edx,ecx-1 / jne → 跳过
th09:0x41ac86  inc DWORD PTR ds:0x4a7e44      ; ★ Lv++
th09:0x41ac8c  cmp [0x4a7e44],ds:0x4a7e58 / jle → 否则钳到上限
```

用在加分算式里（`th09:0x41d150`，得点管理器的方法）：

```
th09:0x41d186  ecx = ds:0x4a7e44      ; ★ Lv
th09:0x41d190  imul ecx,edi           ; Lv × base
th09:0x41d1a7  eax = (Lv×base)/5 + base     ; = base × (1 + Lv/5)
th09:0x41d1a9  ecx = [esi+0xc] + eax  ; ★ 「点」累加器 += 加点
```

难度参数表（开局设定，`th09:0x41b2ee` 起按 `ds:0x4a7eac` 五路分派）：

| difficulty | Lv 初值 | 每 +1 的间隔（帧） | Lv 上限 | 地址 |
| --- | --- | --- | --- | --- |
| 0 | 1 | 1800（`0x708`） | 16（`0x10`），或 `stage/2+6` | `th09:0x41b4f6` / `th09:0x41b528` |
| 1 | 1 | 1200（`0x4b0`）/ `ds:0x4a7ea8==2` 时 900 | 20（`0x14`），或 `stage+10` | `th09:0x41b481` / `0x41b4bb` |
| 2 | 6 | 900（`0x384`）/ 模式 2 时 720（`0x2d0`） | 22（`0x16`），或 `stage/2+15` | `th09:0x41b411` / `0x41b456` |
| 3 | 8 | 720（`0x2d0`）/ 模式 2 时 900（`0x384`） | 22，或 `stage/2+17` | `th09:0x41b3a3` / `0x41b3e3` |
| 4 | 18（`0x12`） | 600（`0x258`） | 22 | `th09:0x41b317` / `0x41b354` |

其它读到 Lv 的地方（⇒ 它是个「公开量」，不是内部临时值）：

| 地址 | 用途 |
| --- | --- |
| `th09:0x40590f` / `th09:0x406abd` | ECL 变量读取器（同一批 getter，`th09:0x406ab0` 读 `0x4a7eac`、`th09:0x406aca` 读 `[enemy+0x2e6c]`）⇒ **ECL 能读到 Lv** |
| `th09:0x40d876` / `th09:0x4109bd` | `fild [0x4a7e44]; fmul const; fadd const` ⇒ 按 Lv 定位某个显示/特效 |

### 4.2 连击与最大连击（HUD 显示，不进分值公式）

| 字段 | 含义 | 一手证据 |
| --- | --- | --- |
| `player+0x30414`（= 得点管理器 `+0x4`） | 连击数 | `th09:0x41d170`–`0x41d17e` 每次加分 +1；HUD 读 `th09:0x418acf`（钳 999） |
| `player+0x30418`（`+0x8`） | **最大连击** | `th09:0x41d17c`–`0x41d183` 取 max；结算里当 bonus 用（§5） |
| `player+0x3044c`（`+0x3c`） | 连击计时（`0x168`/`0x12c` 两档判据） | `th09:0x41d177` 清零；`th09:0x404a6c`、`th09:0x404cd9` 比较 |

⚠ 全 exe 里 `0x30414`/`0x30418` 只被**读**（HUD/结算），**没有被拿去做倍率** ⇒ **TH09 的连击不加分**（与「连击倍率」的印象相反，一手）。

### 4.3 「点」数（HUD 上那串 6 位数）与它的入账

| 字段 | 含义 | 一手证据 |
| --- | --- | --- |
| `player+0x3041c`（得点管理器 `+0xc`） | 「点」累加器：`+= base×(1+Lv/5)`，**向上取整到 10 的倍数**，钳 `999990` | `th09:0x41d1a9`–`0x41d1c0` |
| `player+0x30420`（`+0x10`） | ★ HUD 显示的「点」数（6 位） | HUD 读 `th09:0x418c2e` |
| `player+0x30448`（`+0x38`） | 命中计数，决定冒字数值（`≤100` 时 `×10`，超过给 1000） | `th09:0x41e2cb`–`0x41e2ed`；被 `th09:0x40f8b0` `inc [ecx+0x38]` |
| 入账 | `th09:0x41d7e0`：`score += [sm+0x10]/5`，然后 `[sm+0x10]=0`、`[sm+0xc]=0`、连击与计时清零 | `th09:0x41d7e8`–`0x41d802` |
| 入账触发 | `Gui::on_tick` 的结算分支（`th09:0x4178d6` 起）先给 30000 分（`th09:0x417968`），再对**两方**各调一次 `th09:0x41d7e0` | `th09:0x4179c1`–`0x4179de` |

⇒ TH09 的「收点」其实是**数值口径**：命中 → 「点」累加（带 Lv 倍率）→ 比赛/回合结算时
`score += 点数/5`。**没有实体、没有高度、没有难度×高度公式**。

## 5. 关卡/比赛结算（`Gui::sub_415e50_show_stage_results` `th09:0x415e50`）

| 行 | 算式 | 地址 |
| --- | --- | --- |
| 1 | `[player+0xa8] × 100000` | `th09:0x415e9b`–`0x415ea7` |
| 2 | `[player+0x30418] × 10000`（≥1000 时封顶 `0x989676`） | `th09:0x415eb3`–`0x415ed6` |
| 3 | `[player+0xb0] × 150000`（`>0x42` 时封顶） | `th09:0x415ee4`–`0x415efd` |
| 4 | `[player+0xb4] × 300000`（`>0x21` 时封顶） | `th09:0x415f09`–`0x415f22` |
| 5 | `[player+0xb8] × 300000`（`>0x21` 时封顶） | `th09:0x415f28`–`0x415f47` |
| 6 | `stage==8` 时 `int(*(float*)[side+0x1c]) × 10000000`，否则固定 `0x5f5e0f6`；`stage!=8` 时 0 | `th09:0x415f4d`–`0x415f96` |
| Total | 上面求和 → `[gui+0x11ea0]` | `th09:0x415f96`–`0x415fc9` |
| 入账 | `score += Total/5` | `th09:0x41605b`–`0x416071` |

⇒ 结算**不动任何「道具分值」**（没有道具），它是「每方一个 Total × 1/5」。

## 6. 量纲

| 项 | 结论 | 证据 |
| --- | --- | --- |
| 分数存储 | 每 side `0xa0` 对象 `+0x8`，纯 `int32`，钳 999,999,999 | `th09:0x41a82c` |
| HUD 分数 | 9 位：`[[side+0x1c]+0x4]` 逐位 `idiv 10^k`（`10^8` 单独一段 + `10^7..10^0` 八位） | `th09:0x418cf2`（第 9 位）、`th09:0x418d41` 起（低 8 位） |
| HUD「点」数 | 6 位：`[player+0x30420]`，`idiv 100000/10000/1000/100/10` + 个位 | `th09:0x418c2e`–`th09:0x418d00` |
| 数字绘制 | `AnmLoaded::set_sprite(vm, digit+8)`（`th09:0x436ac0`），或 `AsciiManager::drawf` `%7d` | `th09:0x418c42`、`th09:0x417fa3` |
| 有没有定点/显示缩放 | **没有**：没有 `/10` 之类的显示换算；`/5` 是**游戏规则**（`score += amount/5`），不是显示比例 | `th09:0x40f869`、`th09:0x41d7f1` |
| HUD 其它数字槽 | 连击 3 位 `[player+0x30414]`；两个 2 位数 `[player+0xa0]` / `[player+0xa4]`（每 3600 帧 +1，钳 16）；计时 `mm:ss` | `th09:0x418acf`、`th09:0x41908a`/`0x4190c9`、`th09:0x41811c` |

## 7. 未解 / 待钉

1. ❓ `ds:0x4a7ea8` 的语义（`0/1/2`）：它决定 Lv 上限与间隔走哪一支（§4.1），
   `th09:0x41b0a6`/`0x41b12d` 与 `0` 比、`th09:0x41b158` 与 `1` 比、`th09:0x41b3db` 与 `2` 比。
   本文只写「模式量」，**没有**去追菜单里哪个选项写它（写点在 `th09:0x425695` 一带的 MainMenu 代码）。
2. ❓ Lv 的中文名：我按「随时间升、进倍率」读作**等级/Lv（🟡）**；`th09:0x417fa3` 起的 5 个 `%7d` 列是结算行，
   与 Lv 无关；**HUD 上是否有一格直接显示 Lv** 未钉死（`th09:0x40d876`/`0x4109bd` 用它算显示位置）。
3. ❓ 表格 §5 六行的中文含义（`player+0xa8/b0/b4/b8` 各自是什么计数）未追写点。
4. ❓ 「点数/5」与「base×(1+Lv/5)」两次 `/5` 的关系：一手只证明两处各自都做 `/5`，
   **没有**证明它们是同一个量纲换算（可能是「显示 = 内部/5」的两次巧合）。
5. ❓ `th09:0x415e50` 的 `esi+0x11e88..0x11e98`（5 个 dword）与 `+0x11ea0`（Total）在 `zGui` 的
   `__unknown` 区间里，字段名没有上游可对（vendor 结构到那里是 `__unknown`）。

## 8. 证据 / 可复现

```bash
cd local/th09.v1.50a
# 0) 样本：只有 th09.exe（无变体）
ls -la
# 1) 解 .text 并平坦反汇编
python3 - <<'PY'
d=open('th09.exe','rb').read()
open('/tmp/_th09_text.bin','wb').write(d[0x400:0x400+0x8cc00])
PY
objdump -D -b binary -m i386 -M intel --adjust-vma=0x401000 /tmp/_th09_text.bin > /tmp/th09_full.asm
# 2) 资源清点（无 item.anm；etama.anm 存在）
../../local/vendor/thtk/build/thdat/thdat -l 9 th09.dat | grep -E 'anm|ecl|msg'
# 3) 堆普查：列全部 malloc 调用点与其 size
grep -n -B3 "call   0x47b24e" /tmp/th09_full.asm | grep -E "push   0x|call"
# 4) 加分函数与调用点
grep -n "call   0x40f860" /tmp/th09_full.asm
# 5) Lv 的一手读写
grep -nE "0x4a7e(44|54|58)" /tmp/th09_full.asm
```

- 反汇编：`/tmp/th09_full.asm`（190,368 行，`.text` 576,512 B 全量）。
- 命名/结构（**线索**）：`local/vendor/th-re-data/data/th09.v1.50a/{funcs,statics,type-structs-own}.json`
  （`GAME_MANAGER` `0x4a7d90`、`zGameManagerSide`、`AnmLoaded::set_sprite` `0x436ac0`、
  `Gui::sub_415e50_show_stage_results` `0x415e50`）。⚠ 该来源**没有** `zItem`，与本文结论一致。
- ⚠ 假阳性登记：早期笔记（`local/_scratch/research-cancel-original-th06-th11.md` §T9）已给出
  「TH09 无实体道具」的结论；本文用**独立口径**（堆普查 + 对象名册 + 资源清点）复核成立。
