# stone 逆向 01：TH20 异变石表、四槽装配与石头接口（30 槽虚表）

> **版本**：TH20 v1.00a（`th20.exe`，imagebase `0x400000`）。本文裸地址默认属该版本；引用其他版本须写成 `th18:0x…`。

> 方法：`.data`/`.rdata` 静态字节直读（`tooling/pe_read.py`）+ **MSVC RTTI 复原**（类名 → COL → vftable）
> + 一手反编译（`local/th20.v1.00a/decomp/*.c`，由 `tooling/ghidra/th20_decomp_corpus.py` 落盘）。
> 日期 2026-09-28。可信度分级 ✅高 / 🟡中 / ❓未解。
>
> 本作**没有社区符号**（`local/vendor/th-re-data/th20.v1.00a` 不存在，见
> [`games/th20.v1.00a/INDEX.md`](../../../games/th20.v1.00a/INDEX.md) §2c），
> 类名来自 exe 里保留的 MSVC RTTI，函数名一律 `FUN_xxxxxxxx`（我们还没给它们起名）。
> 挖库工具与四条踩过的坑见 [`tooling/ghidra/README.md`](../../../tooling/ghidra/README.md) 的 TH20 一节。

## 0. 一句话结论

TH20 的「异变石」在代码里就是 **`StoneBaseInf` 家族**：一张 **`[角色][9]` 的工厂函数表**决定
「哪块石 ↔ 哪个类」，四个槽各持一个石头对象指针（`WeaponStoneInf+0x28/+0x2c/+0x30/+0x34`），
石头对象的虚表是 **30 槽**、其中只有少数几槽被各石覆写。石的**显示名/等级**另有
`0x5b09d8`（4 色名）与 `0x5b09e8`（4 色 × 5 级 = 20 条 `StoneAttackLevel_*`）两张字符串表，
等级参数表在 `0x573ff0`（4 色 × 5 级，四行相同 = `3200/3400/3700/3900/4000`）❓量纲未定。

**每角色 8 块石**（`stonetext.txt` 的 8 个键 + 通关实绩顺序一致）：

| # | 石（日文名） | vtable 颜色/序号 | 灵梦类 / 魔理沙类 |
| :-: | --- | --- | --- |
| 0 | スカーレットデビル | R | `StoneRedReimuInf` / `StoneRedMarisaInf` |
| 1 | クリーチャーレッド | R2 | `StoneRedReimu2Inf` / `StoneRedMarisa2Inf` |
| 2 | スノーブロッサム | B | `StoneBlueReimuInf` / `StoneBlueMarisaInf` |
| 3 | ブルーシーズン | B2 | `StoneBlueReimu2Inf` / `StoneBlueMarisa2Inf` |
| 4 | イエローサブタレイニアン | Y | `StoneYellowReimuInf` / `StoneYellowMarisaInf` |
| 5 | インペリシャブルムーン | Y2 | `StoneYellowReimu2Inf` / `StoneYellowMarisa2Inf` |
| 6 | ビーストハードネス | G | `StoneGreenReimuInf` / `StoneGreenMarisaInf` |
| 7 | シントイズムウィンド | G2 | `StoneGreenReimu2Inf` / `StoneGreenMarisa2Inf` |

## 1. 发现：石头工厂表（`stone_id` → 类）

**表地址 `0x5b0ab0`，`[角色 * 9 + stone_id]`，共 18 项**（`pe_read.py … 0x5b0ab0 --dwords 24` 直读）：

| 索引 | 值（工厂） | 类 | 索引 | 值（工厂） | 类 |
| :-: | --- | --- | :-: | --- | --- |
| 0 | `0x532460` | `StoneRedReimuInf` | 9 | `0x532700` | `StoneRedMarisaInf` |
| 1 | `0x532560` | `StoneRedReimu2Inf` | 10 | `0x532630` | `StoneRedMarisa2Inf` |
| 2 | `0x5305f0` | `StoneBlueReimuInf` | 11 | `0x531030` | `StoneBlueMarisaInf` |
| 3 | `0x530ae0` | `StoneBlueReimu2Inf` | 12 | `0x530c90` | `StoneBlueMarisa2Inf` |
| 4 | `0x5357f0` | `StoneYellowReimuInf` | 13 | `0x5389b0` | `StoneYellowMarisaInf` |
| 5 | `0x536d50` | `StoneYellowReimu2Inf` | 14 | `0x5381b0` | `StoneYellowMarisa2Inf` |
| 6 | `0x531580` | `StoneGreenReimuInf` | 15 | `0x532310` | `StoneGreenMarisaInf` |
| 7 | `0x531bf0` | `StoneGreenReimu2Inf` | 16 | `0x5321c0` | `StoneGreenMarisa2Inf` |
| 8 | `0x532460` | （= 0 号，见 §2 COMMON） | 17 | `0x532700` | （= 9 号，见 §2） |

- **发现**：`FUN_00534420(this, char, stone_id)` 里出现 `(*(code *)(&PTR_FUN_005b0ab0)[stone_id + char * 9])()`。
- **验证**：表值 = 各类的工厂（如 `0x532460` → `FUN_00532330` = `operator_new(0x38)` + `memset` + 构造函数，
  构造函数写 `*obj = StoneRedReimuInf::vftable`）；16 个 `weapongreen.cpp:59` 式 assert 串
  正好挂在其中 16 个工厂上（`local/th20.v1.00a/th20-anchors.json`）。
- **结论（✅）**：`stone_id` 0..7 = 上表 8 块石，顺序是 **R,R2,B,B2,Y,Y2,G,G2**；
  颜色块顺序与 `0x5b09d8` 的名字表（`StoneR/StoneB/StoneY/StoneG`）一致。
- 证据：`0x5b0ab0` 直读；`FUN_00534420`；`FUN_00532330`；`th20-anchors.json` 的 16 条 `weapon*_m?\.cpp` 锚点。

## 2. 四槽：setter、偏移与「普通魔石」

四个 setter 形状一致：`FUN_00534xxx(WeaponStoneInf *this, int char, int stone_id)`，
先删旧对象（`FUN_00532740`），再从工厂表造新对象写进固定偏移，最后调 `FUN_00412dc0(obj, N)`。

| setter | 写入偏移 | 第三个实参 N | 槽位（推断） | 石头 id 来源（§3） |
| --- | --- | :-: | --- | --- |
| `FUN_00534420` | `this+0x28` | 0 | **主（MAIN）** ✅ | `FUN_00421810(game)` |
| `FUN_005347d0` | `this+0x30` | 1 | 扩散（WIDE，高速）🟡 | `FUN_0041caf0(game)` |
| `FUN_005344d0` | `this+0x2c` | 2 | 集中（NARROW，低速）🟡 | `FUN_0041cad0(game)` |
| `FUN_00534600` | `this+0x34` | 3 | 支援（SUB）🟡 | `FUN_00412710(game)` |

- **主槽坐实（✅）**：攻击的全部虚调用都落在 `this+0x28` 上（`FUN_00532b20` 里
  `(**(code **)(**(int **)(this + 0x28) + 0x58))()` 等），而 wiki 明说「异变攻击样式由**主**异变石决定」。
- **其余三槽的命名（🟡）**：依据是 setter 第三个实参的顺序（0/1/2/3）与偏移的对应关系
  （`0x28→0`、`0x30→1`、`0x2c→2`、`0x34→3`），再按 wiki 的「主/扩散/集中/支援」次序套上；
  **没有**在代码里找到「槽 1 = 高速射击」的直接证据，故标 🟡。
- **普通魔石（COMMON）**：`stone_id == 8` 时，四个 setter 都会把它替换成
  `FUN_0050a280(game, 0)` 的返回值（= 主石 id）——
  与 `stonetext.txt` 的「コモン魔石…メイン異変石と同じ効果を発揮します」逐字对上（✅）。
- 证据：`FUN_00534420` / `0x5344d0` / `0x534600` / `0x5347d0` 全文；`0x534080`。

## 3. 装配入口：四个槽 id 从哪来

`FUN_00534080(this, char)` = **一次性把四槽装起来**（四个 setter 各调一次）：

| 调用 | 取的 id | 证据 |
| --- | --- | --- |
| `FUN_00534420`（+0x28） | `FUN_00421810(game)` | `FUN_00534080` 第 1 段 |
| `FUN_005347d0`（+0x30） | `FUN_0041caf0(game)` | 第 2 段 |
| `FUN_005344d0`（+0x2c） | `FUN_0041cad0(game)` | 第 3 段 |
| `FUN_00534600`（+0x34） | `FUN_00412710(game)` | 第 4 段 |

- **发现**：四个 `FUN_0041xxxx(game)` 是**游戏对象上四个字段的直读**
  （`FUN_00421810` = `+0x1c`、`FUN_0041cad0` = `+0x20`、`FUN_0041caf0` = `+0x24`、`FUN_00412710` = `+0x28`），
  这里的 `game` = `FUN_00464080(0)` 的返回值。
- **结论（✅）**：**四槽装备 = 游戏对象上的四个 dword `+0x1c/+0x20/+0x24/+0x28`**；
  同一个游戏对象还挂着异变石管理器指针（`+0x2c`）与两根槽的计数/阈值（`+0x4c/+0x50/+0x60`，见
  [`06`](06-attack-gauge-and-miss.md) §7）。
- **结论（🟡）**：装备/编成只在四个 id 上落地；`FUN_00534080` 的调用方 = 开游戏、**读 Replay**
  （`FUN_00508480`，replay.cpp 区域）、`WeaponStoneInf` 自身的 tick（`FUN_005332b0`）、
  石头菜单（`FUN_005167b0`）——与 wiki「主石决定 Replay 保存的机体类型」方向一致（✅ 方向，🟡 细节）。
- 证据：`FUN_00534080`；`FUN_00421810` / `FUN_0041cad0` / `FUN_0041caf0` / `FUN_00412710`（各自一行返回值）；
  `FUN_00464250`（`+0x2c` = 管理器）；`FUN_00534080` 的 7 个调用点
  （`0x4bb0a0`/`0x5089ce`/`0x53306c`/`0x533076`/`0x5332e0`/`0x517666`/`0x517aeb`）。

## 4. 石头接口：30 槽虚表（谁被覆写）

`StoneBaseInf` 的虚表在 `0x575810`，**30 槽**（MSVC，`vftable[-1]` = COL）。16 个具体石类各有一张
30 槽虚表（地址见 `local/th20.v1.00a/th20-rtti.json`）。已坐实的槽：

| 槽 | 基类实现 | 含义 | 证据 |
| :-: | --- | --- | --- |
| 15 | `FUN_00414b60` | 一类通用小函数（各石不同） | 基类虚表 `0x575810` |
| 16/17/18 | — | 另外三槽的同类 tick | `FUN_00532b20` 尾部依次调 15/16/17/18 |
| 20 | 各石不同（如 `0x52ffd0`） | **开始异变攻击** | `FUN_00531460`（G 组基实现） |
| 22 | `FUN_00532f20` | **结束异变攻击**（写 `stone+0x34 = 0`） | `FUN_00532f20`；`FUN_00532b20` 调它 |
| 25 | `FUN_00534180` | **等级/子程序索引**（`0x14` 步长的 4 色 × 5 级表） | `FUN_00534180` |
| 26 | `FUN_00530500` | **攻击是否进行中**（读 `stone+0x34`） | `FUN_00530500`；`FUN_00464270` 调它 |
| 27 | `FUN_005343b0` | 高速/低速**二选一**谓词（`FUN_004ff5e0` 取反） | `FUN_005343b0` |
| 28 | `FUN_00534320` | ↑ 的另一半 | `FUN_00534320` |
| 29 | `FUN_00530540` | 读 `stone+0x35`（各石可覆写，如 `0x532440`） | `FUN_00530540` |

- **发现**：`FUN_00532b20`（`WeaponStoneInf` 的每帧更新）末尾按 `+0x28 → +0x2c → +0x30 → +0x34`
  的顺序调四个石头的 **槽 15/16/17/18**，每个石头只被调一次。
- **结论（✅）**：`+0x28/+0x2c/+0x30/+0x34` 确实是**四个石头对象**，每帧各跑一次自己的 tick。
- 证据：`FUN_00532b20` 尾部；`FUN_00531460`；`FUN_00530500`；`FUN_00532f20`；`FUN_00534180`；
  `FUN_00534320` / `FUN_005343b0`。

### 4.1 石头对象的字段（已坐实的部分）

| 偏移 | 含义 | 证据 |
| --- | --- | --- |
| `+0x4` | 索引（喂给槽 25 的 5 级表） | `FUN_00534180` 读它 |
| `+0xc` | 模式（0/1/2 决定 4 色块里的哪一块） | `FUN_00534180` 读它 |
| `+0x14` | 计时器（发动时被设成攻击时长） | `FUN_00531460` |
| `+0x30` | **等级**（0..4；本颜色当前的异变等级） | `0x51325e` 写、`0x51327e` 读（`MOV EAX,[EDX+0x30]` ⇒ 表索引 `颜色*0x14 + 等级*4`）|
| `+0x34` | **异变攻击进行中标志**（发动置 1、结束清 0） | `FUN_00531460` / `FUN_00532f20` / `FUN_00530500` |
| `+0x35` | 另一个字节标志（槽 29 读它） | `FUN_00530540` |
| `+0x38` | 攻击实体（`.anm` 对象）的登记处 | `FUN_00531460` |

- ❓ **对象大小不一致**：`StoneRedReimuInf` 的工厂 `FUN_00532330` 是 `operator_new(0x38)`，
  而 `StoneGreenReimu2Inf` 的工厂 `FUN_005315c0` 是 `operator_new(0x98)`。同族石头的
  「小/大」两档是否对应「基础石 / 二型石（`…2Inf`）」，**尚未验证**，故只记事实。
- ❓ 槽 20/22 之外的那些被覆写的槽（各石不同，如 `0x531460` 只出现在 G 组两类的虚表里）
  与「异变攻击样式由主石决定」的对应关系，留给
  [`02-attack-assets.md`](02-attack-assets.md)（ECL 侧）回合。

## 5. 名字表与等级表

| 地址 | 内容 | 证据 |
| --- | --- | --- |
| `0x5b09d8` | 4 个颜色名串指针：`StoneR`/`StoneB`/`StoneY`/`StoneG` | HUD `FUN_005120d0` 引用于 `0x5124c3` |
| `0x5b09e8` | 20 个 `StoneAttackLevel_{R,B,Y,G}{1..5}` 串指针 | `FUN_00512fb0` 引用于 `0x51327e`：`表[颜色*0x14 + 等级*4]`，等级取 `stone+0x30` |
| `0x573ff0` | 20 个 dword：`0xc80/0xd48/0xe74/0xf3c/0xfa0`，**四行完全相同** | HUD `FUN_005120d0` 引用于 `0x51249c`：`&DAT + 等级*4 + 颜色*0x14` |
| `0x574040` 起 | §1 表上方还有 4 个颜色名串本体（`StoneR` 等） | `pe_read.py … 0x574040 --bytes 96` |
| `0x5b0ab0` | §1 的工厂函数表（18 项） | `FUN_00534420` 的 `PTR_FUN_005b0ab0[stone_id + char*9]` |

- **发现**：`0x5b09e8` 那条（`FUN_00512fb0`，`0x51327e`）把名字指针交给 `FUN_004a8920`
  —— 那是 **ECL 的「按名字取 sub id」入口**（`FUN_004a8920` → … → `FUN_00540340` 二分查找，
  见 [`02-attack-assets.md`](02-attack-assets.md) §2）。
- **结论（✅）**：**异变攻击的弹幕脚本是按名字（`颜色 × 等级`）查 ECL sub 后起线程的**，
  名字表 `0x5b09e8` 就是「四色 × 五级 → ECL sub 名字」的映射；
  `0x573ff0` 是同一套索引下的数值表（量纲仍未定 ❓）。
- ❓ `stone+0x4`（槽 25 `FUN_00534180` 读它）与 `stone+0x30`（本文这条读的等级）
  是同一个等级还是两个不同字段，尚未判定。
- 证据：`0x5124c3` / `0x51249c` / `0x51325e` / `0x51327e` 四条指令引用（xref + 反汇编）；
  `FUN_005120d0`；`FUN_00512fb0`；`FUN_00534180`；`pe_read.py` 直读 `0x573ff0` 与 `0x574040`；
  `local/th20.v1.00a/extract/stonetext.txt`。

- **发现**：HUD `FUN_005120d0` 先判定「主石在哪个槽 / 哪个颜色」，再按
  `表[颜色 * 0x14 + 等级 * 4]` 取一个 dword（`0xc80..0xfa0`）。
- **结论（🟡）**：`0x573ff0` 是「颜色 × 等级」的一个参数表；
  **量纲未定**（帧数？格数？脚本号？），四行相同说明当前版本里它只随等级变。❓
  它只被 `FUN_005120d0` 读一次，且取出的值写进一个栈对象后**再无人读** —— 这条也记在
  [`02-attack-assets.md`](02-attack-assets.md) §6（资产侧独立复核过同一结论）。

## 6. 八块石 ↔ 过往作品的对应（**owner 裁决①最需要的一张表，目前只到这一步**）

owner 裁决①：「异变石 = 过往异变的能力，**拥有那一作的卡 = 拥有这个石头的支援效果**」。
⇒ 卡 120 需要「石 ↔ 卡号」的映射。目前的一手只有**石自身的名字与文案**：

| 石 | 名字直译 | 过往作品（卡号）| 可信度 |
| --- | --- | --- | --- |
| スカーレットデビル | Scarlet Devil | 紅魔郷（106） | ✅ 名字即副标题 |
| スノーブロッサム | Snow Blossom | 妖々夢（107） | ✅ 同上 |
| インペリシャブルムーン | Imperishable Moon | 永夜抄（108） | ✅ 同上 |
| シントイズムウィンド | Shintoism Wind | 風神録（110） | ✅ 同上 |
| イエローサブタレイニアン | Yellow Subterranean | 地霊殿（111） | ✅ 同上 |
| ブルーシーズン | Blue Season | 天空璋（116） | ✅ 同上 |
| クリーチャーレッド | Creature Red | 鬼形獣（117，「Weakest **Creature**」）| ✅ 名字 + EX 文案双证 |
| ビーストハードネス | Beast Hardness | **獣王園（119 = TH19）** | ✅ owner 2026-09-28 口径 + EX 文案坐实 |

- **发现**：`_EX` 文案里提到的角色是**决定性旁证**（不再是「不构成映射」）：
  - `RED2`（Creature Red）EX 提到「勁牙組組長」「袿姫」= **鬼形獣（117）** 的角色（勁牙組 / 埴安神袿姫）。
  - `GREEN`（Beast Hardness）EX 提到「**地獄の鬼、残無**」= **日白残無**，而她是
    **東方獣王園（TH19）的地獄を統治する鬼**；文案里「獣の持つ無常の力」「無限に広い地獄」也正对 119。
- ★ **owner 2026-09-28 口径**：「**兽之冷硬是 119**」（后订正：「**注意，没有 119，它就是 109**」）
  ⇒ ① 映射定案：`ビーストハードネス`（TH19 獣王園）的效果**并入卡 109**（＝ 花映塚 × 獣王園 合成卡）；
  ② 追加口径：「119 和 109 **都是对战作**，并且从原始的 th09–th19 编号来看就**有纪念意义**，
  我打算实际上**合成一张卡**。不过我几乎不打对战作，具体怎么回事还得再看」
  ⇒ **109 = 花映塚 + 獣王園 合成卡（没有 119 这张卡）**；机制细节见
  [`games/th20.v1.00a/INDEX.md`](../../../games/th20.v1.00a/INDEX.md) §2b-4）。
- **旁注**：8 个石名**全部是英文词的片假名转写**（不是日文词），所以「直译」= 还原转写：
  名字里的 `Creature` / `Beast` 分别对应 **鬼形獣（117）**英文副标题 *Wily Beast and Weakest **Creature***
  与 **獣王園（119）**的「獣」——两块石原先都像 117，EX 文案与卡号编号（106↔TH06 ⇒ TH19↔119）把两块分开了。
- **结论（✅）**：8 块石 ↔ 8 张卡全部对齐：**106 / 107 / 108 / 110 / 111 / 116 / 117 / 119**。
  注意这套映射**不是**我们能从 exe 里读出来的（exe 里只有颜色名 `StoneR/B/Y/G` 与
  `StoneAttackLevel_*`，**没有**任何「原作编号」字段）：6 块靠名字、2 块靠 EX 文案 + owner 口径。
- 证据：`local/th20.v1.00a/extract/stonetext.txt`（8 个 `@…_MAIN_*` 键与其显示名）；
  `local/th20.v1.00a/extract/trophy.txt`（通关实绩按 0..7 顺序列 8 块石，与 §1 表序一致）。

## 7. 与卡 120 的关系（本阶段只记，不实现）

- owner 裁决①「不做装备界面」⇒ §3 的「四个 id 从游戏状态取」这套**照抄可用**，
  但 id 的来源要改成「按持有的卡算」；§2 的四个 setter 与 §4 的接口是现成骨架。
- owner 裁决③「复制粘贴骨架」⇒ §1 表 + §2 setter + §4 的 30 槽接口是**可整段搬**的部分；
  具体弹幕在 ECL（`default.ecl` 的 `StoneAttackLevel_*`），美术在 anm。

## 8. 未解与待补

| # | 未解 | 已知线索 |
| :-: | --- | --- |
| 1 | 石对象「小档 0x38 / 大档 0x98」的分界与含义 | `FUN_00532330` vs `FUN_005315c0` |
| 2 | `0x573ff0` 那 20 个数的量纲 | 四行相同，随等级变 |
| 3 | 槽 1/2/3（扩散/集中/支援）的命名只有顺序证据 | setter 实参 0/1/2/3 |
| 4 | 四个 `FUN_0041xxxx(game)` 各自读哪个字段 | 未挖 |
| 5 | ~~`Creature Red` / `Beast Hardness` 对应哪一作~~ ⇒ **已定**：117（鬼形獣）/ **109 合成卡里的獣王園那一半** | owner 2026-09-28 口径 + EX「残無」文案 |
| 6 | **109 合成卡的机制**（= 花映塚 + 獣王園 合成；**没有 119 这张卡**）| owner 已定「合成一张卡」；细节待调研 —— 样本都在本机（TH09 已在 `local/`，TH19 在 Steam 且带 SteamStub 壳），见 INDEX §2b-4 |

## 关联

- [`06-attack-gauge-and-miss.md`](06-attack-gauge-and-miss.md) —— 异变攻击槽、时长与「抵消 Miss」。
- [`02-attack-assets.md`](02-attack-assets.md) —— `default.ecl` 的 20 个 `StoneAttackLevel_*` 与 anm/SHT 侧。
- [`../OVERVIEW.md`](../OVERVIEW.md) —— 跨版本矩阵（TH20 列）。
- 卡侧工单与 owner 口径：`mods/th18.v1.00a/card-expand/SINGLE-CARD-TEST.md` 的 `§th20`。
