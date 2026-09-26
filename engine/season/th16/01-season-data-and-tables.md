# season 逆向 01:TH16 季节系统全链（档位 / 释放表 / 季节点掉落 / 子机）
> **版本**：TH16 v1.00a（`th16.exe`，imagebase `0x400000`）。本文裸地址默认属该版本；引用其他版本须写成 `th18:0x…`。

> 方法：`.rdata`/`.data` 静态字节直读 + **脱壳后 exe 的一手反汇编/反编译**（Ghidra 12 工程
> `local/th16.v1.00a/ghidra_projects/th16.exe`）+ `.dat` 侧 `thecl`/`thanm` 反汇编。日期 2026-09-09。
> 分级 ✅高 / 🟡中 / ❓未解。
>
> 已有的代码侧叙述（季节释放的双 Bomb 结构、消耗规则、清弹逐帧）在
> [`player/th16/02`](../../player/th16/02-season-release-and-bombs.md)，季节子机在
> [`player/th16/04`](../../player/th16/04-options-subshot-system.md)——本文补它们缺的**数值**与
> **掉落/充能链**，并订正其中一处函数语义（§2.3）。

## 0. 一句话结论

TH16 季节系统的数据面与掉落面已全部一手读通：**6 档阈值 100/230/390/590/840/1140**（档位 0..6）、
**5 副季节 × (半径/半径2/时长) 共 10 张释放表**、**季节点 = `bullet.anm` 脚本 129 的道具**、
**掉落 = 「按累计伤害掉 + 死亡时按计时衰减掉」两条通道**，其中衰减公式是
`count = min + (max − min) × 计时器余量 / max_time`（与 TH15 章节奖的同款线性衰减）。

## 1. 样本：Steam 包装版脱壳（本节是本文的证据前提）

本机唯一 TH16 exe 是 **Steam 包装版**（`local/th16.v1.00a/th16.exe.steamstub`，
md5 `701a603c2afc686776ffddacbed4ee4e`，812,400 B）：`.text` 熵 **8.000**（完全加密）、入口点 `0xe3310`
落在 `.bind` 段内、`.bind` 内无明文 SteamStub 头 → **SteamStub Variant 3.1**。

**脱壳** ✅（2026-09-09，Linux 上跑通）：上游 [Steamless](https://github.com/atom0s/Steamless) 是
.NET Framework 4.5.2 + WinForms，本机无 Mono/Wine；把它的 **跨平台源码原样 Compile Include** 进一个
.NET 8 控制台工程（[`tooling/steamless/`](../../../tooling/steamless/README.md)，未改上游任何文件）即可跑：

```
~/.dotnet/dotnet bin/Release/net8.0/steamless-linux.dll <包装版.exe>
→ th16.exe.unpacked.exe   683,520 B   md5 70d3429444e1e6c18a72e7200a4d7ce8
```

- 大小 **683,520 B = 原版版大小**（812,400 − 128,880 = `.bind` 原始大小，精确相等）；
- `.text` 熵 **8.000 → 6.504**、入口点回到 `.text`（`0x7537b`）、`.bind` 段消失；
- md5 与仓库记录的原版 md5 `cb9caf54ce5738f70086e783ec88fd2a` **不同** —— 因为 Steamless 重建了 PE
  容器（零化 DOS stub、重排段头、未重算 checksum），**代码段字节是原版的**；
- ✅ **证明**：`th-re-data` 的 v1.00a 函数地址在本文件里逐一对上序言 —— 例如
  `BombSubSpring__begin` `0x411460` = `55 8B EC 51 56 8B 35 F8 6E 4A 00`（读 `CACHED_PLAYER_SUBSEASON_SHT_FILE` 旁的 `0x4a6ef8`）、
  `item_collect_season` `0x43de50` = `8B 0D 08 58 4A 00`（读 `CURRENT_SEASON_POWER`）、
  `initiate_season_release_cooldown` `0x40e040` = `8B 81 D8 00 00 00`（读 `zBomb.season_level`）；
  另抽 40 个有名函数，**40/40** 首字节都是合法序言。
- 落盘：`local/th16.v1.00a/th16.exe`（Ghidra 工程 1863 函数 / 1334 有名）。

## 2. 季节槽：阈值、充能、档位

### 2.1 档位阈值 ✅

`Player__init_season_level_deltas(i, value)` `0x43deb0`：把 `SEASON_POWER_LEVEL_DELTAS[i]`（`0x4a5810`）
设为 value，然后把 `DELTAS[0..i]` 累加写进 `0x4a5838 + i*4`（= `SEASON_POWER_LEVEL_REQUIREMENTS[i-1]`，
数组本体 `0x4a583c`）。进关时 `player_shot_init` 用两张静态 seed 数组喂它：

| 符号 | 地址 | 字节值 |
| --- | --- | --- |
| seed 表 A | `0x494810` | `{0, 100, 130, 160}` |
| seed 表 B | `0x494790` | `{200, 250, 300, 0}` |

→ DELTAS = `{0,100,130,160,200,250,300,0}`，累计得 **阈值 `{100, 230, 390, 590, 840, 1140}`**，
`requirements[6]`（`0x4a5854` = `MAX_SEASON_POWER_COPY`）**= 1140 = 满槽**，同时充当数组末尾哨兵。
**档位 = 越过的阈值个数 = 0..6** ✅（与 §3 的 7 项释放表一一对应）。

### 2.2 充能 `item_collect_season` `0x43de50` ✅（一手反编译）

```c
bool item_collect_season(void) {
    if (MAX_SEASON_POWER <= CURRENT_SEASON_POWER) return false;   // 满槽：不加、返回 false
    old = 数阈值(CURRENT_SEASON_POWER);
    CURRENT_SEASON_POWER = min(CURRENT_SEASON_POWER + 1, MAX_SEASON_POWER);
    new = 数阈值(CURRENT_SEASON_POWER);
    return old != new;                                            // ★ 升档才返回 true（驱动升档音效/UI）
}
```

### 2.3 档位 vs 填充比 —— **订正既有文档** ✅

`get_season_gauge_fill_ratio` `0x43df20` **返回的是「当前档内的填充比 0..1」，不是档位**：

```c
float get_season_gauge_fill_ratio(void) {           // xmm0 返回（Ghidra 因无返回类型会丢掉，看汇编）
    L = 数阈值(CURRENT_SEASON_POWER);                // 0..6
    if (L >= 6) return 1.0f;                        // 常量 0x4944d8
    return (float)(CURRENT_SEASON_POWER - REQ[L-1]) / (float)DELTAS[L+1];
}
```

> ⚠️ [`player/th16/02`](../../player/th16/02-season-release-and-bombs.md) §2 原写「数越过几个阈值 = 当前档」
> —— 那是**档位**的算法（`Bomb__can_bomb` 等处各自内联计数），而**这个函数名对应的返回值是填充比**。
> 两者数值域不同（0..6 vs 0..1），本次以一手汇编订正。

## 3. 五个副季节的释放档位表（10 张，全部数值）✅

`BombSub*::begin` 按「档位 `level`」查本副季节的三张表（表全在 `.rdata` 明文，下标 = 档位 0..6，
`[0]=0` 是「没槽不能放」的占位）：

| 副季节 | 时长 `RELEASE_LEVEL_DURATION_*` | 半径 `RELEASE_LEVEL_RADIUS_*` | 半径2 `RELEASE_LEVEL_RADIUS_2_*` |
| --- | --- | --- | --- |
| 春 Spring | `0x491fb8` = `{0,10,13,16,20,20,20}` | `0x491fd4` = `{0,60,100,140,190,240,300}` | —（th-re-data 未命名）|
| 夏 Summer | `0x491f38` = `{0,30,30,30,30,30,30}` | `0x491f54` = `{0,40,42,44,46,48,50}` | — |
| 秋 Fall | `0x491ed4` = `{0,40,55,70,90,110,130}` | `0x491f14` = `{0,60,60,60,60,60,60}` | `0x491ef0` = `{0,40,40,40,40,40,40}` |
| 冬 Winter | `0x491f78` = `{0,30,70,110,150,170,190}` | `0x491f94` = `{0,30,35,40,45,50,60}` | — |
| 土用 Doyou | `0x491e70` = `{0,40,40,40,40,40,40}` | `0x491eb0` = `{0,50,52,54,56,58,60}` | `0x491e8c` = `{0,30,30,30,30,30,30}` |

读点 ✅（`BombSubDoyou__begin` `0x40e0f0` 实证）：`vm+0x4b0 = RADIUS[level]`、`vm+0x4b4 = RADIUS_2[level]`、
`vm+0x4ac = DURATION[level]`（`vm` = 释放特效的 anm VM）。

**设计意图** 🟡（数值 ✅）：**春 = 大而短**（半径到 300、时长仅 10→20 帧，且每帧刷 50 帧无敌）；
**冬 = 小而久**（半径 30→60、时长 30→190 帧，每帧伤害只有 9）；秋半径恒 60、时长 40→130、每帧伤害 30；
夏恒 30 帧、半径 40→50、每帧伤害 100；土用半径 50→60、恒 40 帧、每帧伤害 100（逐帧差异见
[`player/th16/02`](../../player/th16/02-season-release-and-bombs.md) §5e）。

> 🟡 **`RADIUS_2` 语义未定**：秋/土用的 `RADIUS_2`（40 / 30）**小于** `RADIUS`（60 / 50），所以它不是「更大的外圈」，
> 更像「第二层特效的基准尺寸 / 伤害内圈」。

## 4. 季节点道具：`bullet.anm` 脚本 129 ✅

`.rdata` 的 `ITEM_ANM_SCRIPT_IDS`（`0x4917f8`，`zInt2[0x11]` = 17 项，每项 `(脚本id, 第二脚本id)`）
把每种 `zItemType` 映射到 anm 脚本；**`ITEM_SEASON`(type 16) → 脚本 129**。这些脚本**不在** `front.anm`，
而在 **`bullet.anm`**（entry3 的贴图就叫 `bullet/item.png`，256×64）：`script129 = sprite519 = (224,0,16,16)`，
编号与表逐一对上（P 道具 112/134、分数 113/135、…、F 119/141）✅。

**出场动画**（`bullet.anm` `script129` 一手字节）✅：

```
-1:  sprite(519); blendMode(1);                     // 加算
     alpha(0); alphaTime(10, 0, 255);                // 10 帧淡入
     scale(2.0, 2.0); scaleTime(16, 0, 1.0, 1.0);    // ★ 16 帧从 2 倍缩到 1 倍
 0:  rotate(0,0,%RANDRAD); fsetMul(%F0,%RANDF2,0.08726646); angleVel(0,0,%F0);   // 随机自转 ±5°/帧
+16: scaleTime(300, 0, 0.7, 0.7); alphaTime(300, 0, 128);   // 之后 300 帧缩到 0.7、淡到 alpha128
+20: blendMode(0); stop();                            // 第 36 帧关加算
```

**生成** ✅（`ItemManager__spawn_item` `0x430960`，参数 `type=0x10`）：**位置 = 敌人 `abs_pos`**
（一手：两个调用方都把敌人位置指针传进来 —— 死亡掉落 `Enemy__die` `0x41d620` `LEA EBX,[EDI+0x1250]`、
按伤害掉落 `EnemyData__step_logic` `0x41c72f` 段传 `enemy+0x44`），角度 = `randf_minus_pi_to_pi()`，
**速度两条通道不一样**：

| 通道 | 速度 | 出处 |
| --- | --- | --- |
| **死亡掉落**（ECL `dropSeason`，`EnemyDrop__eject_extra_drops` 的 type 0x10 支） | `randf_0_to_1() × 1.9 + 0.2` ⇒ **0.2..2.1** | `0x41d880`–`0x41d8bb`（`FMUL [0x494548]` + `FADD [0x494458]`）|
| **按累计伤害掉落**（`seasonItemDamage`，`EnemyData__step_logic`） | `randf_0_to_1() + [0x494510]` ⇒ **1.2..2.2** | `0x41c769`（`[0x494510]` = 1.2f，字节 `9a 99 99 3f`）|

出生时 VM 色 = `0xffffffff`（**无染色**，`0x430960` 尾 `mov [item+0x530],0xffffffff`；贴图自带颜色）。
⚠️ **type 0x10 走的是「消弹道具池」**（`0x430960` 开头把 `0x10` 排除出普通池，落进
`cancel_items_freelist_head`），这一池子出生时 `item+0xc50 = 6`（**待发射**），
`item+0xc60 = total_items_created & 3`（+ 大批量时的 4/8/16 偏移）当**错帧延时**。

**状态机** ✅（`item+0xc50`，`ItemManager__on_tick_1d__body` `0x42f4e0` 分派）：

| state | 含义 | 运动 |
| --- | --- | --- |
| 6 | **待发射**（type 0x10 的出生态） | 不动、**VM 还没建**；`item+0xc60 -= 1`，减到 −1 时 `Item__init_anm_for_piv_or_season` `0x430c90` 建 VM（脚本 = `SUBSEASON + 0x81` = 129..133）并转 state 3 |
| 3 | **直线飞散**（type 0x10 飞散期） | `pos += vel × GAME_SPEED`、`item+0xc20 -= 0.03`、`vel = polar(item+0xc24, item+0xc20)`（`0x42f82c`–`0x42f83f`）——**角度全程不变、速度线性衰减** ⇒ 飞 `速度²/(2×0.03)` = **0.7..73 px** 后停；**无重力、不清 `vx`** |
| 1 | 飘落 | 见下表（重力 + `vx = 0`） |
| 4 | 飞向自机 | 吸引/回收线/放 B |

> ★ **「竖着一列同 x 坐标」的根源**：state 3 没有重力、也不清 `vx`，所以一撮点从敌人位置**沿各自角度
> 呈放射状散开**（0.2..2.1 速 ⇒ 落点分布在半径 0.7..73 px 的圆内）——这就是「击破掉落 x/y 在小怪附近
> 随机分布」的实际来源。若把「飘落」的 `vy ≥ 0 → vx = 0` 套到出生帧上，朝下的那半点会被钉在敌人的 x
> 上排成一列（我们 2026-09-10 的 bug）。

**运动与收集** ✅（`ItemManager__on_tick_1d__body` `0x42f4e0`，一手反编译 + 常量字节）：
下表**除注明外都是 state 1（飘落）/ state 4-5（飞向自机）**的规则，state 3 见上面的状态机。

| 项 | 值 | 出处 |
| --- | --- | --- |
| 重力（仅 state 1） | `vy += GAME_SPEED × 0.03` | `0x49440c` = 0.03f |
| 下落速度上限 | `vy > 2.0 → 2.0` | `0x494558` = 2.0f |
| 下落时水平速度（仅 state 1） | `vy ≥ 0` 时 `vx = 0` | `0x42fb5f` 起 |
| 出界删 | `y > 472` / `|x| ≥ 200` | `0x4946b8` / `0x494664` |
| 过回收线 | `player.y < 128`（角色 3 纱苗 = 224）→ state 4，速度 = `sht+8` | `0x42f857`–`0x42f874` |
| 放 B | 主炸弹 `bomb_is_in_use == 1 && timer < 0x3c`（60 帧）→ state 4 | `0x42f87a`–`0x42f8b3` |
| **季节解放期间** | **副季节炸弹** `bomb_is_in_use == 1 && timer < 10000` → 整个解放期都吸道具 | 同上（第二条 `SUBSEASON_BOMB_PTR`）|
| 对话中 | `gui->msg != 0` → 同样 → state 4 | `0x42f8b3` |
| 吸引半径 | 进入 `item_attract_box_focused/unfocused` 矩形 → state 5，速度 = `sht+8` **/ 3** | `0x42fd1d` 段 |
| 吸引加速 | 每帧 `+0.2`，上限 `12.0` | `0x4945d4` = 12.0f |
| 飞行方向 | `atan2(player − item)` + `cartesian_from_polar` | `0x42fc0e`–`0x42fc95`（同 TH18 `0x447430`）|
| 收集 | 碰自机 → `case 0x10`：`item_collect_season()`（升档时重建子机 + 弹字 + 音 `0x3f`）→ **`add_to_score(10)`** → 音 `0x25`（`se_item00`）→ 删 | `0x42fe59` 段 |

> ★ 与 TH18 的对照（[`season/th18/01`](../th18/01-season-residuals.md) §1c）：TH18 的原生道具是**同一套代码**
> （重力 0.03 `0x4b90a4`、上限 2.0、上限 12.0、+0.2、出界 472/200 全部一致），只是吸引半径由矩形换成
> 「磁速/3 的圆」并多一条 `player+0x47988..0x47994` 的参数组（ITEM_CATCH / ITEM_LINE 两张原版卡改的就是它）。
> **TH16 的 state 2（放 B / 消弹产生的上抛道具）到顶点时，type 0x10 会退回 state 1 而不是飞向自机**
> （`0x42f618`–`0x42f63e`）—— 这就是「季节点不会自动回收」那条攻略的实际含义；**敌人掉落（state 1）照常被吸引**。
> ⚠️ **别把 state 2 和季节点自己的 state 3 混了**：state 2 是「普通道具被上抛」的态，季节点（type 0x10）
> 出生走的是 **6 → 3**，`0x42f4e0` 里 `iVar3 == 3` 那一支才是它的飞散。

## 5. 掉落机制：两条通道 ✅

### 5.1 通道 A —— 按累计伤害掉（`seasonItemDamage` + 伤害管线）

`seasonItemDamage(D)` 的 ECL handler `0x422a61`：

```c
enemy->drop_season.damage_per_season_drop = D;              // zEnemyData+0x4058
enemy->drop_season.damage_accounted_for_season_drops = enemy->life.current;   // +0x405c ← +0x3f88
```

然后**每次受伤害**（伤害应用函数 `0x41c72f` 起）跑这段：

```c
if (drop_season.damage_per_season_drop > 0) {
    while (drop_season.damage_accounted_for_season_drops < life.current) {   // ★ life.current 是「累计伤害」
        drop_season.damage_accounted_for_season_drops += damage_per_season_drop;
        ItemManager__spawn_item(0x10 /* ITEM_SEASON */, &enemy->abs_pos, 随机角度, 随机速度);
    }
}
```

→ **每对敌人造成 `D` 点伤害掉一个季节点**，掉落点 = 敌人坐标，随机弹出。

### 5.2 通道 B —— 死亡掉落 + 计时衰减（`dropSeason` + `dropItems`）

`dropSeason(a0, a1, a2)` 的 ECL handler `0x422a26`：

```c
drop_season.max_time  (+0x4050) = a0;   Timer__set_value(&drop_season.bonus_timer /*+0x403c*/, a0);
drops.extra_counts[15] (+0x3fd0) = a1;  // extra_counts 按 item type 索引 → 15 = ITEM_SEASON
drop_season.min_count (+0x4054) = a2;
```

`dropItems`（opcode 509，handler `0x420585`）——**死亡/调用时结算，按计时器余量线性衰减**：

```c
if (((byte)DAT_004a5bec & 0x30) != 0x20) {          // 🟡 未命名全局标志位：命中时「不衰减」
    if (bonus_timer.current < 1)
        n = min_count;
    else
        n = (extra_counts[15] - min_count) * bonus_timer.current / max_time + min_count;
    extra_counts[15] = n;
    EnemyDrop__eject_all_drops(&drops, &enemy->abs_pos);   // 0x41a720
}
```

→ **叫 `dropSeason(T, N, M)` 后**：`T` 帧内击杀 → 掉 `N` 个季节点；计时走完 → 掉 `M` 个；中间线性插值。
与 TH15 的章节奖衰减**同款线性**。

### 5.3 ECL 调用普查（`.dat` 全 27 个 `st*.ecl`）✅

| 关卡 | 道中 `dropSeason` | BOSS `dropSeason` | `seasonItemDamage` |
| --- | --- | --- | --- |
| st01 | `(60,8,1)`×2、`(60,10,1)`×2、`(120,30,4)`、`(180,12,4)`×2、`(180,30,5)`、`(300,30,4)` | `(1200,200,10)`×4 | 200 |
| st02 | `(60,8,1)`、`(60,10,1)`、`(170,15,6)`、`(200,19,6)`、`(200,22,6)` | `(1200,200,10)`×5 | 200 ×2 |
| st03 | `(60,10,1)`×3、`(120,13,1)`×2、`(60,13,1)`、`(120,20,1)`、`(200,30,1)` | `(1200,200,10)`×7 | 200 ×2 |
| st04 | `(60,10,1)`×3、`(120,13,1)`×4、`(120,20,1)`、`(200,30,1)`×3 | `(1200,200,10)`×6 | 200 |
| st05 | `(60,10,1)`×5、`(200,30,1)`×2 | `(1200,200,10)`×8、`(2400,200,10)`×8 | 200 ×2 |
| st06 | `(60,10,1)`×2、`(600,100,1)` | — | 200 |
| st07 | `(60,10,1)`×7、`(60,40,1)` | `(2400,200,80)`×17、`(100,6,3)`、`(100,10,8)`、`(2400,400,400)` | 200 ×2 |
| 各 BOSS 收尾（`*bs`/`*mbs`）| — | `(900,200,10)` / `(1200,200,10)` / `(2400,200,80)` | 200 |

**读法** ✅：道中清一色 `(60, 10, 1)` = 60 帧内杀掉掉 10 个、拖完只掉 1 个；BOSS `(1200,200,10)`；
st07 终盘 `(2400,400,400)` = **不衰减**，恒 400 个。`seasonItemDamage(200)` 在 BOSS 脚本开头单独调一次。

### 5.5 ★ 季节子机打的弹：五副季节各一组（`plXXsub.sht`，2026-09-10 一手解析）✅

副 .sht 的 shooterset 偏移表在 **`+0x190`（10 项）**，项是**相对 `+0x1b8` 的偏移**；
shooter 记录 stride **`0x58`**，字段图见 [`sht/th16/05`](../../sht/th16/05-th16-flags-no-runtime-read.md) §2。
每个季节档 = 一个 shooterset，set 里的 shooter 数 = 该档的子机数（档 1 → 1 条、档 6 → 6 条），
`opt = 101..106` 指「第 1..6 颗子机」。**五副季节的弹完全不是同一种东西**：

| 副季节 | 文件 | fire_rate | 伤害 | 初速 | 角度 | tag | func（TH16） | 行为 |
| --- | --- | --- | --- | --- | --- | --- | --- | --- |
| 春 Spring | `pl00sub` | 5 | 8（档1）→ 3（档5-6） | **−0.2** | 扇开 −1.92..−1.22 | 1 | **init1 + tick1** | **寻的**（初速≈0，每帧朝最近敌人拧角 + 加速，59 帧后停） |
| 夏 Summer | `pl02sub` | 5 | 15 | **10.0** | 扇开 −1.92..−0.87 | 0 | — | 直飞 |
| 秋 Fall | `pl03sub` | 5 | 8~16 | **0.0** | −1.745 / −1.571 | 0 | **tick5** | **匀加速 +0.5/帧** |
| 冬 Winter | `pl01sub` | 15 | 5（档1）→ 15（档6） | 0.0 | −1.571 | 2 | **init2 + tick2 + hit2** | **激光**（anm 脚本 0/3/6/9/12/15 逐档变长） |
| 土用 Doyou | `pl04sub` | 3 | 13~15 | **24.0** | **+1.571（向后）** | 0 | — | 向后泼，最快最密（每 3 帧） |

→ 玩家的记忆是对的：**春是追踪的**；冬是激光、秋越飞越快、土用朝背后打。
TH16 的 func 表与 TH18 **init1/tick1（寻的）、init2/tick2/hit2（激光）同族**，秋的 tick5(+0.5)
在 TH18 是 tick3(+0.3)（[`sht/th18/03`](../../sht/th18/03-func-tables-timers-and-dead-fields.md) §6）——
所以复刻时可以直接映射，不必自己写 AI。

**TH18 卡（id 116）的落地**：`assets/sht/append_shooterset.py` 追加 **5 组** shooterset
（`SEASON_SPRING/SUMMER/FALL/WINTER/DOYOU` = 索引 0x19..0x1d），字段取上表**档 1** 的值，
func 用 TH18 的 1/1、0、3、2/2/2；`season.c` 按副季节选 set、每颗子机各发自己那发、
节奏用引擎的 short_timer（不再 `帧×档`）。
⚠️ 未复刻：TH16 每档的**伤害/角度微差**（春 8→3 反而变小、冬 5→15 变大）、
每条 shooter 自己的 opt 与角度扇（我们现在所有子机同角、伤害取档 1）。

### 5.6 ★ 这些弹的**贴图**从哪来（一手：`PlayerBullet__create` + `pl0Xsub.anm`）✅

`PlayerBullet__create` `th16:0x444e10` 里选 anm 的那一段（反编译原文）：

```c
if (uVar4 == 0) {                       /* uVar4 != 0 = 副/季节弹（发射 id 的 0x100 位）*/
    pvVar10 = pzVar3->anm_file;         /* 主弹：player+0xc = pl0X.anm */
    iVar7   = *(short *)(shooter + 0x22) + 5;   /* 脚本号 = sht+0x22 **+5** */
} else {
    pvVar10 = pzVar3->subseason_anm_file;       /* ★ 副弹：player+0x10 = pl0Xsub.anm */
    iVar7   = *(short *)(shooter + 0x22);       /* 脚本号 = sht+0x22（**不 +5**）*/
}
puVar8 = AnmLoaded__create_effect(pvVar10, &pos, iVar7, -1, NULL);
if ((vm+0x534 & 0x80) != 0) { vm+0x530 |= 4; vm+0x40 = shooter[0x14]; }  /* 该 VM 允许旋转 → 写角度 */
```

⇒ **副季节弹的贴图 = `pl0Xsub.anm` 里 `sht+0x22` 号脚本画的 sprite**（不是 `bullet.anm`、
也不是「随机 sprite」）。逐季节对应（`thanm spec` 的 sprite 表 + 脚本逐条）：

| 副季节 | anm 文件 | 脚本 | 主体 sprite（尺寸）| 命中（interrupt 1）|
| --- | --- | --- | --- | --- |
| 春（灵梦）| `pl00sub.anm` | `script0` | sprite9/10（96,32)/(128,32) **32×32**，每 2 帧循环，scale 0.6 / alpha 160 | sprite1..8 每 3 帧，layer 15 + 加算 |
| 夏（咲夜）| `pl02sub.anm` | `script0` | sprite0（0,0）**16×16**，alpha 220 | sprite0..3 每 5 帧 + `scaleTime(20,0,2.0,2.0)` |
| 秋（早苗）| `pl03sub.anm` | `script0` | sprite0（0,0）**64×64**，scale (1.0, 0.3)、color(224,192,64) 金黄 | sprite1..3 每 3 帧 → alpha 0 |
| 冬（魔理沙）| `pl01sub.anm` | `script0/3/6/9/12/15` | sprite0 = **(0,1,512,30) 长条**（贴图只有 256 宽）| `scriptNew(scriptN+1)` 子脚本 + `interrupt 1` 收起 |
| 土用（隐岐奈）| `pl04sub.anm` | `script0` | sprite0（1,1）**30×14**，alpha 128 | sprite1..3 每 10 帧 + `alphaTime(30,0,0)` |

**冬的激光**（`pl01sub.anm` script0 逐条，一手）：

```
scriptNew(script1);  layer(15);  sprite(sprite0);  type(1);  anchor(1, 0);
scale(1.0f, 0.0f);  scaleTime(4, 1, 1.0f, 0.3f);      # 束宽 4 帧长到 0.3（档越高越粗：0.3/0.4）
blendMode(1);  rotateAuto(1);  alpha(96);
fset(%F0, %RANDF);  scrollX(%F0);                     # 贴图沿束向随机滚动
+1:  scrollX(-0.04761905f);  stop();                  # 每帧 −1/21 → 流动感
interruptLabel(1) → scaleTime(8, 1, 1.0f, 0.0f) +8 → delete()
```

再叠加引擎的**蓄力**：`engine/sht/th16/03` §6.4 记的「写 `+0x68`（蓄力×1/512）/+0x70`（原值）
并置 vm flag `+0x530 |8|0x10` 驱动视觉伸长」。

> **TH18 卡的落地（2026-09-10 第二十六轮）**：`assets/ability/make_season_art.py` 把上表
> 五套 sprite 从 `local/th16.v1.00a/anm/pl0Xsub/` 切出来（土用 30×14 补到 32×16），
> `ORDER.txt` 追加 18 个 sprite，`ability.anm` 里新增脚本 **128..131**（春/夏/秋/土用，
> 逐条照抄对应 `script0`），`append_shooterset.py` 的 `anm` 字段改指它们。
> ⚠️ 冬仍是直飞快弹（激光要自己画束 + 伤害源，见 NEXT.md 第二十六轮的「未做」）。
>
> **★★ 切图必踩的坑：entry 的 `xOffset/yOffset`**。TH16 的 anm 是**同页多 texture**：
> 每个 entry 的 `width/height` 是逻辑尺寸，实际数据在页面里的位置由 **`xOffset/yOffset`** 给出，
> sprite 的 `(x,y)` 是**相对该 entry**的 ⇒ 切图要 `+ (xOffset, yOffset)`。各 pl0Xsub 的 entry0 偏移：
> `pl00sub (64,192)`、`pl01sub (0,176)`、`pl02sub (0,144)`、`pl03sub (0,176)`、`pl04sub (0,144)`。
> 不加偏移就会切到白色空白区 —— 实跑症状是**满屏「一系列长方形」**（2026-09-10 踩过）。
> 反例：`bullet/item.png`、`front/front00.png` 的偏移是 0，所以那两处的切图一直是对的。

### 5.7 ★★★ 文（TH16 `pl03`）**主炮** `pl03.sht` 的 10 组 = 5 火力档 × 高速/低速（2026-09-27 一手）

> 起因：卡 108（TH08 永夜抄）把**早苗的妖侧**当作「TH16 的文」复刻；owner 口径
> （2026-09-27）：「**th16 的文文机体，只取其低速部分复刻**（因为低速 = 妖机体）」＋
> 「**不是季节弹**：我看到的文文低速部分是主机**两列**弹幕，子机是**绘制上和秋季节子机一致的、
> 但是是红色的**弹幕」。⇒ 复刻口径 = **形态完全照抄**（行数/角度/发射口/两列都不能动），
> **只允许按 DPS 缩放伤害**。

**表结构**：偏移表在 `+0x190`（10 项，相对 `+0x1b8`），shooter stride **`0x58`**；
`+0x20` 那格（TH18 叫 `pwr_lvl_cnt`）**= 4**、`+0x2` 表长 **= 10** ⇒ 与 TH18 同一套约定
（`set = 档` 为高速、`set = 档 + 1 + 4` 即 **5..9** 为低速；TH18 一手 `th18:0x45ebf0`–`0x45ebf8`
就是 `edx = tier + 1 + [sht+0x20]`）——🟡 TH16 侧**未逐指令核实**，是从同族引擎 + 数据形状推的。

| set | 行数 | 本体行 | 子机行（`opt` / `anm` / `func`）|
| --- | --- | --- | --- |
| 0 / 5 | 2 / 2 | 2 列 `dx = ±10, dy = −8, angle = −π/2, speed 24`、dmg 16 | 无 / 无 |
| 1 / 6 | 4 / 3 | 同上（dmg 14）| 2 行（`anm 5` + tick5）/ 1 行（`anm 1` + init4,tick5）|
| 2 / 7 | 5 / 3 | 同上 | 3 行（`anm 5`）/ 1 行（`anm 2`）|
| 3 / 8 | 6 / 3 | 同上 | 4 行（`anm 5`）/ 1 行（`anm 3`）|
| 4 / 9 | 7 / 3 | 同上（dmg 14）| 5 行（`anm 5`）/ 1 行（`anm 4`）|

- 子机行统一 `opt = 0x400`（= **副/季节弹标志**，`PlayerBullet__create` 里 `uVar4 != 0`
  走 `player+0x10 = pl0Xsub.anm`、脚本号 **不 +5** ⇒ `anm` 就是 `pl03sub.anm` 的脚本号）、
  `dy = −20`、`speed 0`、`x`/`y` 判定盒 48×8（高两档 48×16 / 48×32 / 48×48）。
- `pl03sub.anm` 的脚本（一手 `thanm -l 16`）：**script0 = 秋的月牙弹**（sprite0..3、64×64、
  `color(224,192,64)` 金黄 —— 卡 108 现在用的就是它）；**script1/2 = sprite4..11 的另一种弹**
  （script2 带 `type(3)` + `blendMode(1)` ⇒ 加算，看起来偏亮/偏红）；script3/4 = `drawPoly` 的多边形弹。
  ⇒ owner 说的「和秋季节子机一致但**是红色**」**很可能**就是这一族（`anm 1..4` ⇒ `pl03sub.anm` script1..4），
  🟡 **还没坐实**（要一并查清「红色」是脚本里的 `color()` 还是引擎按机体染色）。

### 5.7b ★ 低速那一半（set 5..9）的**逐行数据**（2026-09-27 逐字节，卡 108 要照抄的就是这 11 行）

两组各 5 档，**本体全是两列**（与 owner 的实跑描述一致：「文文低速部分是主机两列弹幕」）：

| 本体行（set 5..9 都有，逐档只差伤害）| 值 |
| --- | --- |
| `+0x00 rate` / `+0x02 damage` | 3 / **16**（set5）/ **14**（set6..9）|
| `dx` / `dy` | **±10**（两列）/ **−8** |
| `angle` / `speed` | **−π/2** / **24.0** |
| 判定盒 `+0x0c/+0x10` | 48 × 16 |
| `opt` / `anm` | 0（本体）/ 0 ⇒ 主炮路径脚本 = `anm + 5` |
| 四个回调 `+0x28..` | `[0, 0, 0, 5]`（hit5）|

| 子机行（每档**只有 1 行**）| set6 | set7 | set8 | set9 |
| --- | --- | --- | --- | --- |
| `rate` | 5 | 5 | 4 | 4 |
| `damage` | 3 | 8 | 16 | 20 |
| `dy` / `angle` / `speed` | −20 / −π/2 / **0.0**（静止起步）| 同 | 同 | 同 |
| 判定盒 | 48×**8** | 48×**16** | 48×**32** | 48×**48** |
| `opt` | **0x400**（副/季节弹标志）| 同 | 同 | 同 |
| `anm` | **1** | **2** | **3** | **4**（⇒ `pl03sub.anm` 的同号脚本）|
| 四个回调 | `[4, 5, 0, 0]`（init4、tick5 = 匀加速）| 同 | 同 | 同 |
| （set5 = 档 0）| —— **没有子机行** —— | | | |

⇒ **两条独立判据都指向「低速 = set 5..9」**：① 同族引擎的 `set = 档`（高速）/ `档 + 1 + pwr_lvl_cnt`
（低速）；② owner 说低速的子机弹是「秋季节那一族的画法」而**只有 5..9 的子机行是季节标志 + `pl03sub.anm` 的脚本**。
⚠ **本体两列**在两组里都一样（±10）⇒ 两列本身**不能**用来判哪一半是低速（别拿它当判据）。

**卡 108 现在错在哪**：A0..A4 是从 **`pl03sub.sht`（文的季节弹）**搬的 —— 那是「本体 1 行 + 子机 1..4 行」，
而文自己的低速是「**本体 2 行** + 子机 **1** 行」⇒ 形状整个不对（owner：「文文子弹不对」）。
下一步 = 按上表重写 `YOUKAI_TIERS_A` / `YOUKAI_TIER_ORIGINS["A"]`（`[[0,0],[0,0,1],[0,0,1],[0,0,1],[0,0,1]]`），
子机行的贴图从 `pl03sub.anm` script1..4 取（`sprite4..11`；script3/4 是 `drawPoly` 多边形 ⇒ 要单独决定怎么复刻），
伤害**只按 DPS 缩放**。

**卡 108 的落地口径（owner 2026-09-27 拍）**：A0..A4 用 **set 5..9** 那半，**形态照抄**
（本体两列 dx ±10、子机行数与 `opt` 不变），**只有伤害按 DPS 缩放**（= 「除绝对 dps」那条老口径）。
现在的 A0..A4 还是从 **`pl03sub.sht`（= 文的季节弹）** 取的（§5.5 的表），**要整批换掉**。

## 6. 季节子机的静态表（复核 `player/th16/04` §4b）✅

| 符号 | 地址 | 值 | 用途 |
| --- | --- | --- | --- |
| 季节子机布局索引 | `0x4a5dac` | 扁平三角数 `0,0,1,3,6,10,15,21,28,0,1,3,…` | `idx = 表[SUBSEASON*8 + level] + i` |
| 本体子机布局索引 | `0x4a5e6c`（`0x4a5e4c`+0x20 起）| `0,1,3,6,10,11,13,16` | 按 `CHARACTER` |
| 季节子机 anm 脚本 | `0x492be0` | `{1, 1, 1, 0x12, 1}` | 春/夏/秋/冬/土用 |
| 本体子机 anm 脚本 | `0x492c00` | `{8, 7, 11, 11}` | 按 `CHARACTER` |

## 7. 结构体侧的季节字段（th-re-data + 一手读点）

| 结构体 | 字段 | 含义 |
| --- | --- | --- |
| `zBomb` | `+0xd4 is_season` / `+0xd8 season_level` | 主炸 vs 季节释放；释放时记下的档位 ✅ |
| `zPlayerInner` | `+0x3e0 subseason_options[8]` / `+0x1608c num_season_options` | 季节子机槽 ✅ |
| `zPlayerInner` | `+0x16078 speed_multiplier__used_by_fall`（= `PLAYER+0x16688`）| **秋**释放期间移速倍率：写 1.5（`0x40eed8`）、读于移动 `0x441f43`/`0x442ce7` ✅ |
| `zPlayerInner` | `+0x16010 winter__..._ge_3` | **冬**：激光在场且档 ≥3 时火力档提升 🟡 |
| `zItem` | `+0xc68`（位场，含 `force_autocollect__season_only`）| 道具状态位场；季节点自动收集位未逐位定位 🟡 |
| `zEnemyData` | `+0x403c drop_season` / `+0x3fd0 extra_counts[15]` | 见 §5 ✅ |

## 8. 社区口径交叉核对（THBWiki「东方天空璋/系统」）🟡→✅

用户 2026-09-09 贴来一份社区攻略口径，逐条与本仓一手对账：

| 攻略说法 | 本仓一手 | 判定 |
| --- | --- | --- |
| 1~6 级子机分别要 100 / 230 / 390 / 590 / 840 / 1140 个季节道具 | 阈值 `{100,230,390,590,840,1140}`（§2.1）| ✅ 完全一致 |
| 季节道具来自「迅速打倒敌人 / 擦弹 / 消弹」 | 三条通道全部反到：`EnemyData__step_logic` `0x41c72f`、`Player__do_graze` `0x444cf0`、`gen_items_from_cancel` `0x416a00`（§5）| ✅ 一致 |
| 解放后槽归零，**夏 / 土用只消耗 1 副机** | `Bomb__activate_bomb` `0x40db20` 的消耗分支（[`player/th16/02`](../../player/th16/02-season-release-and-bombs.md) §3）| ✅ 一致 |
| 春：大范围、仅一瞬、**消弹结束后 60 帧无敌**、中等伤害 | 半径 60→300、时长 10→20 帧、每帧伤害 100；`BombSubSpring__on_tick` 每帧把无敌刷到 50 | ✅ 一致（60 = 攻略体感，一手是「刷 50 帧/帧」）|
| 夏：小范围、比春长、**9 帧无敌**、伤害大 | 半径 40→50、恒 30 帧、每帧 100；`begin` 置 `+0x1663c = 10` | ✅ 基本一致（9 vs 10）|
| 秋：消弹区**随玩家移动**、先大后缩、**30 帧无敌**、玩家加速、每帧固定伤害 | 半径恒 60、时长 40→130、每帧 30、`speed_multiplier__used_by_fall` = 1.5 | ✅ 一致 |
| 冬：中范围、范围与时长随等级、**伤害 ×1.5**、30 帧无敌、每帧固定伤害 | 半径 30→60、时长 30→190、每帧 9、`+0x2c7cc = 1.5f`（→ `+0x1664c` bit5）| ✅ 一致 |
| 土用：小范围**向后移动并缩小**、只扣 1、大量伤害 | 半径 50→60、恒 40 帧、每帧 100、只扣一档 | ✅ 数值一致；**「向后移动」在 anm 脚本里**，本文未验 🟡 |
| 季节道具**不会自动回收**；靠到画面上方全屏回收 / 放符卡与解放时回收 | **一手订正（2026-09-10）**：只有 **state 2**（放 B / 消弹产生的上抛道具）在顶点退回 state 1、不飞向自机（`0x42f618`–`0x42f63e`）；**敌人掉落（state 1）照常吃吸引矩形与回收线**（§4 运动表）。攻略那句话是「被解放/消弹抛出来的那批不会自动追你」| ✅ 机制已定（`zItem+0xc68` 的消费点仍未追，不影响）|
| 消弹出现**绿分数道具**，提高蓝分数道具的得分；1C/2C 每绿点 +10 最大得点、3C +20 … 6C +50 | 未反；`ITEM_ANM_SCRIPT_IDS` 里 type 2（蓝点）的第二脚本 = 135，疑即绿色变体 | ❓ 待验 |
| 副季节「配置在角色身后作为子机」 | `zPlayerInner+0x3e0 subseason_options[8]` + `num_season_options` | ✅ 一致 |

> 结论：**攻略与一手逐条吻合**（含三处「等级 1~6 的累计点数」这种最容易被记错的数字），
> 只有两条（不自动回收 / 绿分数道具）本文还没反到 —— 它们不影响 TH18 卡的复刻（卡里季节点
> 直接撒在自机身上、TH18 也没有「最大得点」这套）。

## 9. 季节计量槽（UI）与「玩家靠近就淡出」✅

**外观**（`front.anm`）：根脚本 **script113**（`layer(20)`）用 `scriptNew` 拉起 5 个子 VM。
★ 玩家看到的**季节字**（春/夏/秋/冬/土用）是 `front.anm` entry0 贴图 `front/front00.png` 里的
**sprite82..86**（32×32，y=80 一行五个），由引擎按 `sprite = 82 + SUBSEASON` 换
（`th16:0x4276e1 add edx,0x52`）；**档位**体现为实心条（背景）的亮度（`pending_interrupt = 档 + 7`），
外框/填充条都是 `drawRect*` 画的矩形（102×10 外框 + 100×8 填充）。

| 子脚本 | 画什么 | 备注 |
| --- | --- | --- |
| `script114` | `drawRectBorder(102, 10)`、`color(80,80,80)` | 槽的**外框**（102×10 px）|
| `script115` | `drawRectGrad(100, 8)`、蓝 `color(64,64,255)` | **进度条**（宽度由引擎每帧写；档 0 → interrupt 3 = 蓝，档 ≥1 → interrupt 2 = 黄→白）|
| `script116` | `drawRect(100, 8)` | **背景条**（全长 100），档位 → `pending_interrupt = 档 + 7`（8..13，颜色逐档变亮）|
| `script117` | `sprite(82)` | 图标（`front.anm` sprite82）|
| `script118` | `sprite(87)` | 图标（sprite87）|

**★ 绘制顺序**：根脚本 script113 的 `scriptNew` 次序是 **114 → 116 → 115 → 117 → 118**
（`front.anm.txt` 一手），即**实心背景条先生成、渐变进度条后生成** ⇒ 同层内后生成者画在上面
= **进度条盖在背景条之上**（进度才看得见）。

**位置**：子 VM 的坐标是 `pos(-377, 888)` / `pos(-375, 888)` / `pos(-344, 884)`（`anchor(1,2)` = 右下锚）。
TH16 的 HUD 空间是 **640×480 窗口的 2 倍（1280×960）**，游玩区 x ∈ [-368, 368]、y ∈ [16, 864]
（2 × 玩家边界 x∈[-184,184]、y∈[8,432]）→ 计量槽正好落在**游玩区左边缘外侧、屏幕左下角**
（生命计数器在 x=1028 = 右侧，可作对照）。

**★ 哪根条是「会动的」= 渐变条（一手对账）**：`season_gauge_render_42c600` `0x42c600` 开头
用 `AnmVm__search_children(gauge_vm, 0x73, 0)` 取**子脚本 115（渐变条）**的 VM（`pauVar4`），
每帧把 `get_season_gauge_fill_ratio() × 100.0` 写进它的矩形宽度（`+0x70`），满槽时写常量
`0x42c80000`（= 100.0）；同一个 VM 上还发 `pending_interrupt = 2`（档 ≥1）/ `3`（档 0）。
实心条是 `search_children(gauge_vm, 0x74, 0)` 取到的 **script116**，只收 `pending_interrupt = 档 + 7`，
**宽度永不被写**（恒为 100）⇒ 它是背景。
⚠️ 档 0 时实心条连档位中断都不发（`*(uint*)(solid+0x530) &= ~2`；`+0x530` 是 anm VM 的脏标记字，
写 `+0x70` 后置 `|8`、改中断后置 `|2`，对照 `engine/sht/th16/03` §6.4 的 anm 束体）
⇒ 观感上「等级 0 时背景透明」。

**观感口径（用户 2026-09-10 逐条确认，与上面的一手对账一致）**：
- **会动的进度矩形** = `drawRectGrad` = **黄白相间**（档 ≥1 时 `interrupt 2` 的 (255,255,0)→(255,255,255)）；
- **不动的背景矩形** = `drawRect` = **蓝、随档位变白**（`interrupt 8..13` 的 (v,v,255)，v = 64/96/128/144/192/224）；
- **唯一例外 = 档 0**：进度填**蓝**（`interrupt 3`）、背景填**透明**（不发档位中断）。

**★ 档位数字是 ASCII 文字，不是贴图**（一手：`th16:0x428e70` `Gui::on_draw_2` 尾段）：
槽右端那个 0–6 的数字由 `FUN_004082b0`（= `create_string("%d")`）画，画之前把 AsciiManager 设成

| 字段 | 值 |
| --- | --- |
| `font_id`（TH16 在 `+0x19224`）| **2** |
| `group`（`+0x19228`）| **1** |
| `alignment_mode_h` / `_v`（`+0x19230`/`+0x19234`）| **0 / 2** |
| 颜色 | 7 档表 `local_60[]`：`0x60606060` / `0xa0b0b080` / `0xb0b8b880` / `0xc0c0c080` / `0xd0d0d080` / `0xe0e0e080` / `0xffffff30`（D3DCOLOR = 0xAARRGGBB）|
| 淡出 | 玩家贴近左下角（`gui+0x1ac` bit0）时把 alpha 字节写 **0x40**（= 64，与槽的 `alphaTime(…,64)` 同一档）|

TH18 的字体 2 与 TH16/TH15 的字形映射逐条相同（[`../../anm/th18/03-ascii-text.md`](../../anm/th18/03-ascii-text.md) §3）
⇒ 卡里照抄「字体 2 + group 1 + 对齐 (0,2) + 该档位色」即可（`season.c` 已这么画）。

**★「玩家靠近就淡出」** —— `Gui::on_tick_20` `0x427cf0` 里一个带闩锁的两分支（一手反汇编）：

```c
/* gui+0x1ac bit0 = 已隐藏闩锁；edi = gui，eax = PLAYER_PTR */
if (!(gui->flags_1ac & 1)) {                       /* 当前显示 */
    if (player && player->pos.y > 400.0f && player->pos.x < -64.0f) {
        AnmManager__interrupt_tree(gui->season_gauge_guid /*+0x10c*/, 5);   /* 淡出 */
        gui->flags_1ac |= 1;
    }
} else if (player && (player->pos.y < 384.0f || player->pos.x > -64.0f)) {
    AnmManager__interrupt_tree(gui->season_gauge_guid, 4);                  /* 淡回 */
    gui->flags_1ac &= ~1;
}
```

- 触发区 = **玩家坐标 x < −64 且 y > 400**（游玩区 x∈[−184,184]、y∈[8,432]）→ 即**左下角**
  约 120×32 px 的一块（正好是计量槽所在的角）。
- **淡出不是消失**：`script114/115/116/117` 的 `interruptLabel(5)` 是 `alphaTime(5, 0, 64)`
  —— 5 帧淡到 **alpha 64（约 25%）**；`interruptLabel(4)` 是 `alphaTime(5, 0, 255)` 淡回。
- 用闩锁（bit0）而不是每帧比大小，所以进出触发区各只发一次中断，动画不会被打断。
- ⚠️ 恢复条件是 `y < 384 || x > −64`（与触发区**不重合**，有 16 px 迟滞），别写成「取反」。

## 9b. 每个敌人掉多少季节点（ECL 全量普查）✅

`dropSeason(T, N, M)` 写在**每个敌人自己的 ECL 子程序**里（不是关卡脚本），所以「所有敌人都会掉」。
全 8 关普查（`thecl -d 16`，按子程序归并）：

| 敌人 | `dropSeason` | 含义 |
| --- | --- | --- |
| 小妖精 `Cir00` / `GirlA01` | `(60, 8, 1)` | **60 帧内杀掉给 8 个**，拖到计时走完只给 1 个 |
| `GirlA03` / `ECir00` / `Phantom01` | `(60, 10, 1)` | 10 → 1 |
| `GirlA01F` / `GirlA04F` | `(120, 13, 1)` | 13 → 1 |
| `GirlB01` | `(120, 20, 1)` / `(120, 30, 4)` | 20–30 → 1–4 |
| `GirlC01` / `GirlC02` | `(170~200, 12~19, 4~6)` | 12–19 → 4–6 |
| `GirlD01` / `GirlD02` | `(180~600, 22~100, 1~6)` | 大妖精：22–100 |
| BOSS | `(1200~2400, 200, 10)` / `(2400, 200, 80)` / `(2400, 400, 400)` | 200 个上限、10 个下限；st07 终盘恒 400 |
| BOSS 附加 | `seasonItemDamage(200)` | 每 **200 点伤害**再掉 1 个 |

→ **道中小怪一颗就是 8–10 个**（且只有快速击杀才拿满）；TH16 里**一个季节点 = 1 点季节点槽**
（`item_collect_season` `0x43de50` 每次 +1），所以「一串 8 个」= 8 点。
TH18 卡（id 116）**照这个数量撒**（小怪 8 颗、boss 200 颗，每颗 1 点）——
2026-09-10 之前曾把「一串折成一颗 = 8 点」，现已改回原义。

**出场过程** ✅（`zItem` + `bullet.anm` script129）：季节点出生时是**白色**且**不可收集**
（`zItem.intangibility_frames` 同义），过一会儿才褪白、可以收；**擦弹掉的那颗从被擦的弹上
沿「弹 − 自机」方向斜向上外抛**（`Player__do_graze` `th16:0x444e03` 用 atan2(弹−自机) 定角度）。
一手实参（2026-09-10 补）：`ItemManager__spawn_item(0x10, &bullet->pos, _, angle, 1.9f, _, 0)`
—— **出生点 = 弹的坐标、速度恒 1.9**（不是随机）、`force_autocollect` 参 = 0；
擦弹特效是 `effect_anm` script `0x18` 放在「弹与自机的中点」，另加一个 `0xffc0c0ff` 的小分弹窗与音 `0x2a`。

## 10. 还缺什么

1. 🟡 `RELEASE_LEVEL_RADIUS_2_*` 语义（§3 末尾）。
2. 🟡 季节点「自动收集」位在 `zItem+0xc68` 里的哪一位、由谁读。
3. 🟡 副季节常驻被动（秋移速 / 冬激光火力）的完整消费链。
4. 🟡 `DAT_004a5bec`（未命名全局标志字）在 `dropItems` 里的 `& 0x30 == 0x20` 门是什么意思。
5. ❓ 季节槽对伤害/符卡的反作用（[`player/th16/02`](../../player/th16/02-season-release-and-bombs.md) §7 遗留）。

## 11. 可信度与证据

- ✅ **一手**：§1 的脱壳与地址对账、§2 的 `init_season_level_deltas`/`item_collect_season`/`get_season_gauge_fill_ratio`
  反编译、§3 的 10 张表字节、§4 的道具脚本与 `spawn_item`、§5 的四个 ECL handler 反编译 + 伤害管线
  （`0x41c72f` 起的掉落循环）、§5.3 的 ECL 普查、§6 的 4 张表、§7 的读点。
- 🟡 **推断/未定**：§3 的 `RADIUS_2`、§5.2 的标志位门、§7 的冬/自动收集。
- 复核入口：Ghidra 工程 `local/th16.v1.00a/ghidra_projects/th16.exe`（程序名 `/th16.exe`），
  或 `python3 tooling/ghidra/scripts/decompile_funcs.py th16 0x422a26 0x422a61 0x420585`。
- 交叉：[`player/th16/02`](../../player/th16/02-season-release-and-bombs.md)（释放逻辑）、
  [`player/th16/04`](../../player/th16/04-options-subshot-system.md)（子机）、
  [`player/th16/06`](../../player/th16/06-resource-economy.md)（经济位置）、
  [`season/th18/01`](../th18/01-season-residuals.md)（TH18 侧残骸）。
