/* great_work_core.h —— 伟大之作（id 92）的纯逻辑：**「转换量 → 冷却帧数」与「血量 → 金钱枚数」**。
 * 不碰引擎 ⇒ 主机 `make test-host` 能直接测。
 *
 * ## 规则（owner 2026-10-03 逐字）
 * 「noita卡：伟大之作：把所有敌人+弹幕变成金钱道具（类P点的那个）。冷却时间按照转换的弹幕数量和
 *   消灭敌人的血量等比例计算。敌人也按照剩余血量可以转换出多个金钱。如果是 boss 期间就会直接击破一个阶段。」
 *
 * ⇒ 冷却 = `GW_BASE + 弹幕数·GW_PER_BULLET + Σ(敌人剩余血量)·GW_PER_HP`，钳 `[GW_MIN, GW_MAX]`；
 *   每个敌人换出的金钱枚数 = `血量 / GW_MONEY_PER_HP`（至少 1 枚、至多 `GW_MONEY_MAX` 枚）。
 *
 * ⚠ 与一手的关系：**这两条公式是 owner 的口径、不是原版的**（Noita 的「伟大之作」在那边是炼金终点，
 *   不是这张卡的效果）⇒ 全部算 ❓ 自定，登记在 `TUNING.md`。可复刻的只有"东西"本身：
 *   金钱道具 = **item type 2**（一手 `engine/faith/th18/01-point-item-value.md` §3：收下时 `MONEY += 1`
 *   + `SCORE += 身价/10`），弹幕 = 引擎的子弹池（`CE_BM_BULLETS` 起 2000 槽，`state ∈ {1,2}` = 活动）。 */
#pragma once
#include <stdint.h>

/* —— 转换出来的东西（❓ 全部自定） —— */
#define GW_MONEY_PER_HP     20      /* 每 20 点剩余血量 ⇒ 1 枚金钱道具 */
#define GW_MONEY_MAX        40      /* 单个敌人最多换这么多枚（防一个 boss 撒爆道具池）*/
#define GW_BULLET_MAX       512     /* 一次最多把多少颗弹幕变成金钱（引擎子弹池 2000 槽；道具池 600+4096）。
                                     * 超出的照删、只是不撒金钱（`GW_BULLET_MAX` 之外不记位置）*/
#define GW_ENEMY_MAX        64      /* 一次最多处理多少个敌人（同帧记位置的静态表大小）*/
#define GW_MONEY_ANGLE_SPREAD 1     /* 1 = 金钱撒落角度打散（不叠成一摞）*/

/* —— 冷却公式（❓ 全部自定） —— */
#define GW_BASE             360     /* ★ 6 s 底价（owner 2026-10-03「再减少冷却」：原 600 的 0.6 倍）*/
#define GW_PER_BULLET       6       /* ★ 2026-10-24 owner：弹的基数（2 → 6）*/
#define GW_PER_HP           1       /* 每点敌人剩余血量 +1 帧（不变）*/
#define GW_MIN              360     /* ★ 冷却下限 6 s（原 600）*/
#define GW_MAX              10800   /* ★ 2026-10-24 owner：上限 **3 分钟**（300 s → 180 s）。
                                     * 量级参照：200 颗弹 + 2000 血 ≈ 3560 帧（59 s）；
                                     * 一屏弹 + 一个大 boss 阶段（2 万血）会直接顶到 10800（180 s）*/

/* 一个敌人换出几枚金钱（`hp <= 0` 也给 1 枚：能转换出来的东西不该白转）。*/
int32_t gw_money_for_hp(int32_t hp);
/* 冷却帧数：`bullets` = 转换掉的弹幕数、`hp_sum` = 转换掉的敌人剩余血量之和。
 * 两个输入都当非负处理（调用方传负数 = 0），结果钳在 [GW_MIN, GW_MAX]。*/
int32_t gw_recharge(int32_t bullets, int64_t hp_sum);
/* 把 `GW_MAX` 帧摊成 0..1000 的进度（诊断 / 贴图用不到，写单测方便）。*/
int32_t gw_recharge_permille(int32_t frames);
