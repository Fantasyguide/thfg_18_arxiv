# 破损核心（card-expand id 71）—— 设计

> **版本**：TH18 v1.00a（`th18.exe`，imagebase `0x400000`）。本文裸地址默认属该版本；引用其他版本须写成 `th16:0x…`。
> 日期：2026-09-05。状态：用户已定（§0）；**第二版**——第一版让子机走 SHT 连射，弹要飞过去，与「瞬发单体」相悖，
> 同日改成定点伤害源。归属：`mods/th18.v1.00a/card-expand/`。
> 引擎一手：[`engine/card/th18/03-hooks.md`](../../../engine/card/th18/03-hooks.md) §5、
> [`engine/player/th18/02-damage-sources.md`](../../../engine/player/th18/02-damage-sources.md)、
> [`engine/sht/th18/`](../../../engine/sht/th18/README.md)（第一版的产物，独立成立）。

## 0. 一句话

**装备卡**：带着它就在自机旁边多一颗电球子机；电球每 1 秒朝**最近的一个敌人**瞬间劈一道电弧，
那个敌人吃 120 伤害（2026-09-06 平衡：原 80 / 2 s = 40 DPS，只有原生子机卡中位 120–135 的三分之一；改成 120 / 1 s 对齐中位）。纯正面效果，「破损」只体现在外观与文案上。

用户已定（2026-09-05）：**追踪最近敌人** / **无负面代价** / **瞬发、单体、电弧只是特效** /
美术：**卡图与电球用户原创**（青色裂核白底放大居中；黄绿球），**电弧程序生成**（黄白闪电链）。

## 1. 两件独立的事：子机怎么来、伤害怎么给

### 1.1 子机：走原生装备卡机制

原生装备卡 = `on_power_level_change` 里 `Player__allocate_option(card; card, off, card, off, ability_script)`
生成一个 zPlayerOption，引擎负责位置 / 聚焦位移 / 开店收起 / 火力档变时重建。我们照抄，两处不同：

| 原生 | 我们 | 原因 |
| --- | --- | --- |
| 子机指针存 `card+0x54` | 存 `ce_state()` | 新卡对象是基类 `0x54` 字节，`+0x54` 在对象外 |
| 每帧 `on_tick_shooters` 按 SHT 索引连射 | **不用** | 那个槽只在按住射击键时广播；自机弹也天生要飞过去 |

子机长相 = `ability.anm` script88（`allocate_option` 的最后一参就是 ability.anm 的脚本号）。

### 1.2 伤害：定点伤害源钉在目标上

`ce_damage_rect(center = 目标坐标, angle 0, life 2, dmg 30, w 32, h 32)` × 连续 4 帧（`0x45dfa0`，Remilia 脉冲 / 青眼光束同一个原语）。
拆帧的理由：每帧伤害上限 `player+0x47984` 是「本帧对该敌人的总量」，Sakuya 只有 60；单帧 120 会被钳掉一半以上，30 一帧四个自机都吃满。

- **单体**：判定只罩住目标中心 ±12 px。
- **定量**：`enm_compute_damage_sources` `0x45f0f0` 有 `src+0x84` 的 tag 守卫——一个伤害源对同一个敌人只结算一次，
  所以寿命 2 帧 = 正好一次 80（多的那帧只是给「AbilityManager tick 建、EnemyManager tick 结算」留余量）。
- **走正常管线**：每帧上限 `player+0x47984`（Sakuya 60 会钳）、计分、命中反馈都照旧。

### 1.3 为什么不是别的

| 方案 | 为什么不 |
| --- | --- |
| SHT 自机弹（第一版）| 弹要飞过去；要「瞬发」得把弹生在敌人身上 + 靠 VM 自删杀弹 + 靠 tag 守卫保证只算一次——三层把戏 |
| SHT 自机弹极速 | 一帧一判定，24 px 判定一帧走 100 px 会穿过小敌人 |
| 直接改 `enemy+0x6220` HP | 绕过无敌 / 符卡阶段 / 计分 / 命中反馈，没有任何原生先例 |
| 沿线矩形（长 = 距离）| 是穿透 AoE，用户要的是单体 |

## 2. 数值

| 项 | 值 | 理由 |
| --- | --- | --- |
| 卡 id / `internal_name` | `71` / `BROKEN_CORE` | |
| `category` / `price_tier` / `weight` | `1`（子机装备；2026-09-06 订正：原生 1 = 子机卡、2 = 被动）/ `8`（240）/ `2` | 与原生 `*_OP` 子机卡同价、同区段 |
| 子机偏移 | `0x18` | Alice `0x1c` 与 Marisa1 `0x10` 之间 |
| 周期 | 60 帧 | 蓄满没目标就攒着，敌人一进射程立刻劈 |
| 伤害 / 判定 / 寿命 | 4 帧 × 30 = 120 / 32×32 / 每源 2 帧 | §1.2 |
| 锁敌半径 | 512 | 抄 Alice 的搜索半径 `0x4b93b0` |
| 音效 | `0x46 se_noise` | 电流 |

## 3. 实现

- `native/cards/broken_core.c`：`on_power_level_change` 申请子机；`on_load` / `on_run_reset` 照原生清 bit1、删子机 VM、松指针；
  `on_tick_2` 每帧：认领子机槽（`option+0xd0 == card+0x08`）→ 计时 → 走敌人链表挑最近的（`EnemyManager+0x18c`、
  跳过 `+0x635c & 0xc000021`、坐标 `+0x1270/+0x1274`）→ `ce_damage_rect` → 起电弧 / 火花 VM → 音效。
- `native/cards/broken_core_core.c/.h`：计时状态机、最近目标归约、`bc_atan2f`（多项式）与 `bc_aim_dist`（`sqrtss`）——
  两个都是头里的 `static inline`：i386 ABI 返回 float 走 st0，会破坏 `make dllx87` 的零 x87 不变式；`-fno-math-errno` 让 sqrt 不掉进 libm。
- 特效：`ability.anm` entry `BROKEN_CORE_ORB`（128×128）/ `BROKEN_CORE_BOLT`（256×64，朝 +x 铺满、耐拉伸）；
  script88 子机常驻（照原生 script2 骨架：`interruptLabel(2)` 出场 / `(3)` 收起 / `(1)` 销毁）、
  script89 电弧（`anchor(1, 0)`，C 写 pos / rotation.z / scale.x = 距离 / 256，脚本不碰 scale / rotate）、
  script90 命中火花。电球与卡图是用户原创（`cards/_src/BROKEN_CORE.png`、`ability/broken_core/_src/LightningOrb.png`），
  电弧程序生成；都过 `assets/ability/make_broken_core_art.py`（固定种子）。
- 新 SDK 包装：`ce_allocate_option`（压栈序照 `0x40aae0` 复刻，AUDIT U1）、`ce_anm_set_rotation`（`vm+0x3c`）。

## 4. 风险 / 审计点

见 [`AUDIT.md`](../../../mods/th18.v1.00a/card-expand/AUDIT.md) §U（16 条）。核心三条：U1 压栈序、U3 槽认领、U9 tag 守卫。

## 5. 实跑标志

```
sdk: 71 bound (.on_power_level_change = …, .on_tick_2 = …, .on_load = …, .on_run_reset = …)
broken_core: option allocated (ptr …, anm id …)
broken_core: fire #1 at frame 120, orb (x, y) -> target (x, y) dist 143.2 angle -1.83
```

体感：自机右侧一颗电球；每 2 秒一道电弧瞬间连到最近的敌人、命中点火花、电流声、敌人掉血；
没有敌人时不发。过关不消失；进店收起、出店回来。
