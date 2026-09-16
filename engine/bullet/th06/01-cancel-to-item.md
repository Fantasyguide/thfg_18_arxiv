# TH06 红魔乡 消弹 → 出不出实体道具

> **版本**：TH06 v1.02h（**本机无 exe** ⇒ 用源码重建 `/mnt/d/项目/幻想指南/18/th06`，仓外只读；该工程 README 要求原版 `東方紅魔郷.exe` v1.02h，
> `sha256 9f76483c46256804792399296619c1274363c31cd8f1775fafb55106fb852245`，**🟡 转引、本机无法自校**）。
> 本文地址一律带 `th06:` 前缀；引用别作写 `th07:0x…`。
> 一手工具：源码直读（`src/*.cpp` / `src/ItemManager.hpp`）+ 该工程的 `config/mapping.csv`（地址层）；复现命令见 §4。
> **证据分两层，全文严格区分**：**行为层 = 源码直读 ✅ 强**；**地址层 = 该工程 mapping.csv 🟡 单源**（本机无 exe 可独立复核）。
> 素材：`local/_scratch/research-cancel-original-th06-th11.md` §TH06（第 897–1167 行）。

## 1. 发现（一手事实）

### 1.1 消弹入口全集（三个真函数 + 炸弹 + 擦弹）

| # | 入口（源码名） | 地址（🟡 mapping.csv） | 行为 |
| --- | --- | --- | --- |
| ① | `BulletManager::RemoveAllBullets(ZunBool turnIntoItem)` | `th06:0x414160`（`0x1d2` B） | 按参数决定撒不撒（弹 + 激光） |
| ② | `BulletManager::TurnAllBulletsIntoPoints()` | `th06:0x414340`（`0x15` B） | 一行包装：`RemoveAllBullets(true)` |
| ③ | `BulletManager::DespawnBullets(i32 maxBonus, ZunBool awardPoints)` | `th06:0x414360`（`0x25f` B） | 撒（按参数）+ 冒数字 + `score += 总分` |
| ④ | 炸弹（Bomb） | **不调上面任何一个** | `Player::CheckGraze` / `CalcKillBoxCollision` 里的矩形裁剪（§1.4） |
| ⑤ | 擦弹（Graze） | `th06:0x426df0` / `th06:0x426c40` | **`return 1` = 只加分不消弹不撒** |
| ⑥ | `ItemManager::RemoveAllItems()` | `th06:0x420130` | 清空**道具**池（反方向，列出以免混淆） |
| ⑦ | `BulletManager::SpawnLaserPattern(EnemyLaserShooter*)` | `th06:0x414670` | **发射**激光 —— 与消弹无关（见 §2.3 的更正） |

池形状：`g_BulletManager.bullets[640]`（步长 `0x930`，`+0x5600`）+ `g_BulletManager.lasers[64]`
（步长 `0x260`，`+0xec000`）**在同一个 manager 里**（对照 TH10/TH11 是两个 manager）；`g_ItemManager.items[513]`。

### 1.2 被消掉的弹变成什么：✅ **每颗 1 颗 `ITEM_POINT_BULLET`（= 6）**

`ItemType` 枚举全文（`src/ItemManager.hpp:12-24`，消弹撒的是**最后一个**）：

```
enum ItemType {                  // 注释原文：This enum is 1 byte in size on Enemy
    ITEM_NO_ITEM = -2, ITEM_RANDOM_ITEM = -1,
    ITEM_POWER_SMALL,   // 0   小 P
    ITEM_POINT,         // 1   点（蓝点）
    ITEM_POWER_BIG,     // 2   大 P
    ITEM_BOMB,          // 3   Bomb
    ITEM_FULL_POWER,    // 4   满火力
    ITEM_LIFE,          // 5   1UP
    ITEM_POINT_BULLET,  // ★ 6  ← 消弹专用的点道具
};
```

| 入口 | 每颗几个 | 掉在哪 | 值 / 入账 |
| --- | --- | --- | --- |
| ① / ② / ③（`turnIntoItem`/`awardPoints` 为真时） | **弹：每颗 1 颗**；**激光：沿轴每 `32.0f` 1 颗**（不是一根一颗） | **弹/激光点自己的坐标** | `state = 1`（实体；由道具规则决定走位收） |
| ③ 额外 | 每颗**冒一个分数数字**（`CreatePopup1`） | 同上 | 2000 起、每颗 `+10`、上限 = 调用方参数（调用点 `12800`）；`score += 总和` |

① 弹支逐行（`src/BulletManager.cpp:381-439`）：`state == UNUSED || DESPAWNING` 跳过；
`turnIntoItem` 真 ⇒ `SpawnItem(&bullet->pos, ITEM_POINT_BULLET, 1)` + `memset(bullet,0,sizeof)`（**不进死亡动画**）；
假 ⇒ 只写 `state = DESPAWNING`。激光支：`laser->state < 2` 时置 `state = 2`，真则从 `startOffset` 到 `endOffset`
用 `fsincos_wrapper` 取轴方向，`itemPos = pos + dir*offset`，`offset += 32.0f`。

**③ 的两个数值与 TH07 不同（不能互抄）**：TH06 = 起点 **2000**、每颗 **+10**、上限 = 参数（**12800**）、入账 `score += 总和`；
TH07 = 起点 2000、每颗 **+20**、上限 **8000**、入账走 `0x423090(...)` 的返回值 `/10`。

### 1.3 擦弹 / 炸弹：同一个函数的两条 `return`（★ TH06 最有特色的一条）

`Player::CheckGraze`（`src/Player.cpp:1196-1248`）：先把**弹盒外扩 `20.0f`**，再拿 `bombProjectiles[16]`
逐格做 AABB 相交测试（`bombProjectile->sizeX == 0.0f` 表示未用）：

```cpp
bulletTopLeft.x = center->x - size->x/2.0f - 20.0f;   // ★ 弹盒外扩 20.0f
...
if (bombTopLeft.x > bulletBottomRight.x || ... ) { }   // 不相交
else return 2;                                          // ★★ 被炸弹裁掉
...
this->ScoreGraze(center);  return 1;                    // ★ 只擦弹：不消弹、不撒道具
```

* **`return 2` 的落点**（`src/BulletManager.cpp:924-929` / `:935-943`，逐字）：
  `curBullet->state = BULLET_STATE_DESPAWNING; g_ItemManager.SpawnItem(&curBullet->pos, ITEM_POINT_BULLET, 1);`
  ⇒ **炸弹消弹会撒点道具**（形状是**矩形**，不是圆）。
* 炸弹矩形由 `src/BombData.cpp` 每帧按机体/帧号填（例：`sizeX = sizeY = 256.0f`、其它格 `48.0f`）⇒
  判定 = 若干矩形（**最大 `256×256`**）。
* **擦弹不消弹**（上一版按 TH06NC 文档写的这条成立）。

### 1.4 场地门：❌ **没有**（`SpawnItem` 全文零边界比较）

`ItemManager::SpawnItem`（`src/ItemManager.cpp:21`）只有「池扫描 + 写字段」，**没有任何 x/y 边界比较**
⇒ TH06 **没有** TH16/TH18 的「屏外不撒」（`x∉(−192−32,192+32)` / `y∉(−32,480)`），
也**没有** TH11 的「把 `item.x` 钳到 ±192」。
🟡 **否定范围**：只读了 `SpawnItem` 全文与三个消弹函数全文，**没有**穷举「调用者是否先自己过滤位置」
（但三个函数都是 `SpawnItem(&bullet->pos, …)` 直传，看得到没有过滤）。

### 1.5 RNG：❌ 消弹链**不消耗**

`SpawnItem` 里 RNG **只出现在一个分支**：`if (state == 2)` ⇒
`targetPosition.x = g_Rng.GetRandomF32ZeroToOne() * 288.0f + 48.0f`（48.0→336.0）、
`targetPosition.y = … * 192.0f - 64.0f`（−64.0→128.0）。
而**三个消弹函数统统传 `state = 1`**（逐处核对）⇒ **消弹不取随机数**，全链确定性
（同 TH10/TH11；**与 TH07「每撒一颗取一次」相反**）。
🟡 边界：`ItemManager::OnUpdate`（下落/吸附/收集）里的随机没查（不在消弹链上）。
⚠ 顺带钉死字段语义：TH06 的 `Item.startPosition` 装的是**初速 `(0, -2.2, 0)`**（不是位置），`currentPosition` 才是位置。
（交叉印证：TH07 的 `state == 2` 分支用的随机器常数是 `288.0f`/`48.0f`、`192.0f`/`64.0f`，与这里**同值** —— 两个作品同一套「随机散布」。）

### 1.6 boss 击破 / 符卡结束 / 关卡切换属哪一档（★ 五档，全表）

`grep -rn 'RemoveAllBullets\|TurnAllBulletsIntoPoints\|DespawnBullets' src/` 的**全部 9 个调用点**（穷举）：

| 调用点（源码） | 场景 | 撒道具？ | 冒分？ |
| --- | --- | --- | --- |
| `EclManager.cpp:892` `ECL_OPCODE_BULLETCANCEL` | ECL 的消弹 opcode | **✅**（`TurnAllBulletsIntoPoints`） | ❌ |
| `EclManager.cpp:717` `ECL_OPCODE_SPELLCARDSTART` | **符卡宣言** | **✅** | ❌ |
| `EclManager.cpp:755` `ECL_OPCODE_SPELLCARDEND` | **符卡结束** | **✅**（`DespawnBullets(12800, 1)`） | **✅**（2000 起 +10/颗、上限 12800） |
| `ItemManager.cpp:186` / `:265` / `:305` | 满 P（小 P 到 128）/ 大 P ≥128 / 吃到满火力道具 | **✅** | ❌ |
| `EnemyManager.cpp:415` | **符卡被中途打断** | **❌**（`RemoveAllBullets(0)`） | ❌ |
| `EnemyManager.cpp:693` | **boss 死亡且非符卡中** | **❌**（`DespawnBullets(12800, false)`） | **✅** |
| `Player.cpp:296` | 复活无敌期（`bulletGracePeriod != 0`，逐帧调） | **❌**（`RemoveAllBullets(0)`） | ❌ |

* **boss 击破（不在符卡中）= 弹幕消失 + 冒数字加分，但一颗点道具都不出** ——
  与 TH18 那条「boss 击破满屏弹幕消失但一颗点都不出」**同档**（跨作一致的一手对照）。
* 符卡宣言 / 结束 = **出点**（结束那一下额外冒数字）；符卡被打断 = **不出**（TH06 独有的第三档）。
* **关卡切换不在这 9 处**：弹幕走 `BulletManager::InitializeToZero`（`th06:0x413430`）/`BulletManager`（`th06:0x413450`）**整池重建**。

## 2. 验证（怎么排除掉别的路）

### 2.1 计数与穷举可复现

`grep -rn` 三个函数名 ⇒ 声明 3 行（`BulletManager.hpp:115/:118/:120`）+ 定义 3 行 + `TurnAllBulletsIntoPoints`
体内 1 行 `RemoveAllBullets(true)`（`BulletManager.cpp:443`）+ **调用点 9 行**，逐行地址见 §1.6 表。

### 2.2 「擦弹不消弹」与「炸弹消弹」是同一函数的两条 return

`CheckGraze` 里炸弹判定**在**自机判定**之前**（顺序要紧：贴脸时炸弹先赢），
且 `return 2` 的**唯一落点**在调用侧写 `state = DESPAWNING` + `SpawnItem(..., ITEM_POINT_BULLET, 1)`
⇒ 「擦弹 = `return 1`」与「炸弹 = `return 2`」不可能互相冒充。

### 2.3 更正：`th06:0x414670` 不是「消激光」

vendor 的旧名 `sub_414670_does_smn_to_lasers` 是**误名**；`config/mapping.csv` 与源码都给
`BulletManager::SpawnLaserPattern(EnemyLaserShooter*)` ⇒ 它是**发射**激光。
素材上一版写的「激光专用消弹入口（很可能就是把激光 state 置掉）」**错了**：激光的「消」没有独立入口，
就是 ① / ③ 两个函数的**后半段**（与弹同一个 `turnIntoItem` / `awardPoints` 参数）。

### 2.4 参数不是 `mode`，是 `ZunBool`（型号码也不可跨作搬）

TH06 用 `ZunBool turnIntoItem` / `awardPoints`，**没有 `mode`**（同 TH09/TH10/TH11 一档）。
所以 `ITEM_POINT_BULLET = 6` 是 **TH06 的号**，与 TH07 的 `type` 号、TH18 的 `type` 号**都不通用**。

## 3. 结论（可信度 + 适用范围）

**一句话**：TH06 的消弹有三个真函数 —— `RemoveAllBullets(turnIntoItem)`、它的一行包装
`TurnAllBulletsIntoPoints()`、以及 `DespawnBullets(maxBonus, awardPoints)`；开关为真时每颗弹撒 1 颗
`ITEM_POINT_BULLET`（= **6**）在弹自己的位置，**激光沿轴每 `32.0f` 撒一颗**；炸弹走
`Player::CheckGraze` 里 `bombProjectiles[16]` 的矩形裁剪（弹盒外扩 **`20.0f`**，命中 `return 2` ⇒ 消 + 撒），
擦弹（`return 1`）只加分；`SpawnItem` 内**没有场地门**，RNG 只在 `state == 2` 取而消弹一律传 `state = 1`
⇒ **全链确定性**；boss 击破（非符卡中）= 只冒分不出点，符卡宣言/结束 = 出点，符卡被打断 = 不出。

* ✅ **行为层（源码直读，强）**：① 三个消弹函数的全文语义与 `ITEM_POINT_BULLET` 的落点（逐行）；
  ② `ItemType` 枚举全表（`ITEM_POINT_BULLET = 6`）；③ 炸弹矩形裁剪的判据、`+20.0f` 外扩、
  `bombProjectiles[16]`、`sizeX == 0.0f` 判未用；④ `SpawnItem` 全文：无场地门、RNG 只在 `state == 2`、
  `startPosition = (0, -2.2, 0)`；⑤ **9 个调用点穷举**（含 ECL opcode 名 `BULLETCANCEL` /
  `SPELLCARDSTART` / `SPELLCARDEND`）；⑥ `lasers[64]` 与 `bullets[640]` 同 manager、
  `SpawnLaserPattern` 是**发射**而非消除。
* 🟡 **中（地址层，单源）**：**所有 `th06:0x…` 地址**来自该工程自己的 `config/mapping.csv`，
  **本机没有 TH06 exe ⇒ 无法独立复核**。**行为结论不依赖地址**；若要拿这些地址做字节级 patch
  （或写 hook），必须先取到 th06 v1.02h 的 exe 对一次。
* 🟡 其它：`ItemManager::OnUpdate` 的随机未查（不属消弹链）；`CreatePopup1` 的入账口径未细读；
  「没有场地门」的否定范围见 §1.4。
* ❓ **未钉死**：`ITEM_POINT_BULLET`（type 6）的**收集分值**没读到（`OnUpdate` 里的 type 6 分支）——
  **不许按 TH07 的 `type 6` handler 外推**（那是另一作的 code）。

**适用范围**：`th06 v1.02h` 的**行为**；地址仅对该重建工程的 mapping 负责。
跨作时**只借行为、不借号**：TH06 的 `6`、TH07 的 `type`、TH18 的 `mode` 是三套不同的表。

## 4. 证据（可复现命令 —— 仓外只读）

```bash
cd /mnt/d/项目/幻想指南/18/th06

# ① 地址 ↔ 符号（本节「地址层」全部来自这张表，🟡 单源）
grep -E 'BulletManager::|ItemManager::|Player::CheckGraze|Player::CalcKillBoxCollision' config/mapping.csv
# ② 三个消弹函数 + 激光（全文）
sed -n '380,525p' src/BulletManager.cpp
# ③ ItemType 枚举（ITEM_POINT_BULLET 的值）
sed -n '10,26p' src/ItemManager.hpp
# ④ SpawnItem 全文（场地门 / RNG / startPosition）
sed -n '21,70p' src/ItemManager.cpp   # SpawnItem 全文（到 70 行止）
# ⑤ 炸弹矩形裁剪（20.0f 外扩 + bombProjectiles[16] + return 2）
sed -n '1196,1260p' src/Player.cpp ; grep -n 'PlayerRect bombProjectiles' src/Player.hpp
# ⑥ 调用点穷举（9 处；含 ECL opcode 名）
grep -rn 'RemoveAllBullets\|TurnAllBulletsIntoPoints\|DespawnBullets' src/
sed -n '705,760p;885,896p' src/EclManager.cpp
sed -n '405,420p;685,698p' src/EnemyManager.cpp
sed -n '178,190p;258,270p;298,310p' src/ItemManager.cpp ; sed -n '288,300p' src/Player.cpp
# ⑦ 样本校验（本机无 exe ⇒ 只能转引工程 README 的要求）
sed -n '21p' README.md   # sha256 9f76483c46256804792399296619c1274363c31cd8f1775fafb55106fb852245
```

## 5. 对卡的直接影响（只读观察，不改代码）

`grep -n "ce_cancel\|cancel" mods/th18.v1.00a/card-expand/native/cards/th06rank.c` ⇒ **0 命中**（exit 1）。

| 项 | 我们卡现状 | 一手（TH06） | 判定 |
| --- | --- | --- | --- |
| 消弹 | 106（`th06rank.c`）是 **rank 卡**：只挂 `on_item_collect` / `on_item_frame` 观测道具，**不调任何消弹入口** | 三个消弹函数 + 炸弹矩形裁剪 | ✅ **一致（本卡不碰消弹，无冲突）** |
| 道具型号码 | 卡里 `T06_ITEM_POWER_SMALL = 1`、`T06_ITEM_POINT = 2`（`th06rank_core.h:119-120`，注释自带 TH18 语义「÷5 ⇒ +0.002」） | TH06 自己：`POWER_SMALL = 0`、`POINT = 1`、**`POINT_BULLET = 6`** | ⚠ **号差 1 档**（卡用 TH18 号，是卡侧选择）；将来 106 若要复刻消弹，**必须做号映射** |
| 擦弹 | 卡用 `0x4ccd0c` 差分读擦弹计数（不消弹） | 擦弹 = `return 1`，**不消弹不撒** | ✅ 一致 |
| 炸弹 | 卡用 `ce_bomb_spent`（`0x4203bc`）观测消耗 | 炸弹消弹会**撒点道具**（矩形裁剪） | ❓ 本卡不涉；若将来做「炸弹消弹撒点」，要按 §1.3 的 `return 2` 路径复刻，别用 TH18 的 mode 表 |

⚠ **最容易被误用的两条**：① TH06 的 `ITEM_POINT_BULLET = 6` 与 **TH18** 的 `type 6`（我们卡注释里 = B 碎，`th06rank.c:278`）**不是一回事**；
② TH06 **没有 `mode` 参数** —— 看到 `RemoveAllBullets(0)` 的 `0` 是 `ZunBool false`（不撒），
不是 TH18 那种「mode 0 = 纯删除」。**两处都不能按号套用。**
