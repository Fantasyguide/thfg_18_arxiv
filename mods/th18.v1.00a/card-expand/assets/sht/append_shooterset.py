#!/usr/bin/env python3
"""往零售的四个 `pl0X.sht` 追加「新卡子机」的 shooterset，输出到 native/build/sht/，并生成 native/sht_ids.h。

    python3 append_shooterset.py                # 构建 + 自检 + 写 sht_ids.h
    python3 append_shooterset.py --verify-only  # 只自检已有的 build/sht/*.sht

## 为什么能纯追加

`pl0X.sht` 的 `+0xe0` 是一张 **40 项**的 shooterset 偏移数组（数据区起点 `+0x180` 是解析器里的硬编码
常量），零售只用了 23 项（`0x00`–`0x09` 主炮、`0x0a`–`0x16` 装备卡子机），**尾部 17 项是 0**。
所以「填一项 + 文件尾接一段」不动前面任何一个字节。
一手：`engine/sht/th18/01-file-layout-and-shooterset-index.md`。

## 三条必须守住的不变式（脚本每次都回读校验）

1. 头部 `+0x02` 仍是 40 —— 它是解析循环的上界。
2. **shooterset 0 的四个 func 字段仍全是 0**。剩下的空位仍指向 set 0，会被重复解析；
   靠 `0 → 表[0] = NULL → 0` 幂等才不炸。改坏它 = 开局崩。
3. 前 23 组 shooterset 的字节逐字节不变。

另外 `fire_rate` 必须 1–127：`0` 在装备卡路径上会整数除零（`0x40a9c0` 没有零分支），
`< 0` 是 shooterset 的终止符。
"""
import argparse
import struct
import sys
from pathlib import Path

HERE = Path(__file__).resolve().parent          # …/assets/sht
MOD = HERE.parent.parent                        # …/card-expand
REPO = MOD.parent.parent.parent
RETAIL = REPO / "local" / "th18.v1.00a" / "dat"
OUT = MOD / "native" / "build" / "sht"
IDS_H = MOD / "native" / "sht_ids.h"
ANM_IDS_H = MOD / "native" / "anm_ids.h"
FILES = ("pl00.sht", "pl01.sht", "pl02.sht", "pl03.sht")

OFF_ARRAY = 0xE0        # 偏移数组
DATA_BASE = 0x180       # 解析器里的硬编码常量
N_SLOTS = 40            # 偏移数组长度（= 头部 +0x02）
N_RETAIL = 23           # 零售用掉的项（0x00–0x16）
STRIDE = 0x5C
TERMINATOR = b"\xff\xff\xff\xff"

# ---- 要追加的 shooterset ----------------------------------------------------
# 一项 = (NAME, [shooter, …])；索引按顺序从 N_RETAIL 起分配。字段图见
# engine/sht/th18/02-shooter-record.md §1。这里只列非 0 字段，其余补 0。
APPEND = [
    # 豌豆射手（id 77）。两组：平时一颗大豌豆，「开大」时一排 7 颗小豌豆。
    # 节奏**不**交给引擎的射击计时器 —— peashooter.c 自带计数器当 short_timer 传进
    # `Player__tick_shooters_for_ability_card`，所以这里的 fire_rate 就是「每 N 个我们的计数发一次」。
    # 字段图见 engine/sht/th18/02-shooter-record.md §1；发射判定是 `timer % fire_rate == start_delay`。
    # opt_slot 必须 0（用 tick_shooters 传进来的子机坐标）；angle −π/2 = 正上方；func_* 全 0 = 直飞不瞄准。

    # 平时：1 秒一颗、一颗 90 = 90 DPS（PVZ 原版射速是 1.425 s ≈ 85 帧，2026-09-08 用户改成整 1 秒；
    # DPS 与改之前的 85/130 基本相同，但节奏更密）。90 塞满 Reimu 的每帧上限、超出 Sakuya 的 60，
    # 所以卡在豌豆在途的帧把上限抬到 180。
    ("PEA_NORMAL", [dict(fire_rate=60, start_delay=0, damage=90, hit_w=16.0, hit_h=16.0,
                         angle=-1.5707964, speed=14.0, opt_slot=0, mode=0,
                         anm="CE_ANM_ABILITY_SCRIPT_PEA", sfx=0x20, func_on_init=0)]),

    # 开大（PVZ 的机枪射手）：每 5 帧一排，一排 7 颗横向铺开 ±48 px；持续 100 帧 = 20 排 = 140 颗。
    # 单颗只有 7（= PVZ 原版豌豆伤害的量级），一排 7 颗同帧也只有 49，稳在所有角色的每帧上限
    # （最低 Sakuya 60）之下 —— 一口气打出去的是**数量**，不是单发。
    ("PEA_GATLING", [dict(fire_rate=5, start_delay=0, damage=7, hit_w=12.0, hit_h=12.0,
                          spawn_dx=dx, angle=-1.5707964, speed=18.0, opt_slot=0, mode=0,
                          anm="CE_ANM_ABILITY_SCRIPT_PEA_SMALL", sfx=-1, func_on_init=0)
                     for dx in (-48.0, -32.0, -16.0, 0.0, 16.0, 32.0, 48.0)]),

    # ★ 季节（id 119）：**五副季节各一组**，数值逐条照 TH16 `pl00sub/pl02sub/pl03sub/pl01sub/pl04sub.sht`
    # 的 option 弹（2026-09-10 一手解析：sets 按季节档，每个 set 里每个 option 一条 shooter）。
    # TH16 的 func 表与 TH18 **init1/tick1（寻的）、init2/tick2/hit2（激光）同族**（`engine/sht/th18/03` §6），
    # 所以「春追踪 / 冬激光」能直接映射；秋的 tick5(+0.5/帧) 在 TH18 是 **tick3(+0.3/帧)**（同族不同常量）。
    # 每条都是「一颗弹从子机位置发出」，opt_slot=0 = 用 tick_shooters 传进来的那颗子机。
    # TH16 每档的伤害/角度略有差别（春 8→3、冬 5→15、秋 8~16、土用 13~15），这里取**档 1 的值**。
    ("SEASON_SPRING", [dict(fire_rate=5, start_delay=0, damage=8, hit_w=16.0, hit_h=16.0,
                            angle=-1.5707964, speed=-0.2, opt_slot=0, mode=0,
                            anm="CE_ANM_ABILITY_SCRIPT_SEASON_BULLET_SPRING", sfx=-1,
                            func_on_init=1, func_on_tick=1)]),        # 寻的（初速≈0，靠 tick1 转向加速）
    ("SEASON_SUMMER", [dict(fire_rate=5, start_delay=0, damage=15, hit_w=16.0, hit_h=16.0,
                            angle=-1.5707964, speed=10.0, opt_slot=0, mode=0,
                            anm="CE_ANM_ABILITY_SCRIPT_SEASON_BULLET_SUMMER", sfx=-1, func_on_init=0)]),
    ("SEASON_FALL",   [dict(fire_rate=5, start_delay=0, damage=16, hit_w=16.0, hit_h=16.0,
                            angle=-1.5707964, speed=0.0, opt_slot=0, mode=0,
                            anm="CE_ANM_ABILITY_SCRIPT_SEASON_BULLET_FALL", sfx=-1,
                            func_on_init=0, func_on_tick=3)]),        # 匀加速（TH18 tick3 = +0.3/帧）
    # ★★ 2026-09-10 第二十八轮：**照零售「ミニ八卦炉」重做激光**（一手：`ability.txt` @MARISA_OP
    # = 卡 10 = SHT set **0xb**，四个角色的 set 0xb 逐字段相同）：
    #   `rate=5 delay=0 dmg=13 ang=−π/2 **speed=0** mode=**2** anm=**3** sfx=−1 hit_fx(+0x26)=**5**
    #    func=init2/tick2/hit2`，而 `ability.anm` 的 **script3** 就是激光束（`anchor(1,0)` +
    #   `scale(1,0)→scaleTime(4,1,1,0.5)` + `blendMode(1)` + `scrollX(−1/21)` + `scriptNew(script4)` 光斑）。
    # ⇒ 上次「损坏内存」不是这条路的锅，是我们没照零售参数填（mode=0 / 无 hit_fx / 用了别的脚本）。
    # 我们照 TH16 `pl01sub` 的对应脚本自建 132（束）/133（光斑）/134（命中特效），贴图用 TH16 的激光条。
    ("SEASON_WINTER", [dict(fire_rate=15, start_delay=0, damage=5, hit_w=8.0, hit_h=16.0,
                            angle=-1.5707964, speed=0.0, opt_slot=0, mode=2,
                            anm="CE_ANM_ABILITY_SCRIPT_SEASON_LASER_BEAM", sfx=-1,
                            hit_fx="CE_ANM_ABILITY_SCRIPT_SEASON_LASER_HIT",
                            func_on_init=2, func_on_tick=2, func_on_hit=2)]),
    ("SEASON_DOYOU",  [dict(fire_rate=3, start_delay=0, damage=15, hit_w=48.0, hit_h=18.0,
                            angle=1.5707964, speed=24.0, opt_slot=0, mode=0,
                            anm="CE_ANM_ABILITY_SCRIPT_SEASON_BULLET_DOYOU", sfx=-1, func_on_init=0)]),  # 向后 24/帧
]


# ---- TH17「咆哮」shooterset 数据（来源：local/th17.v1.00b/dat/pl00a.sht / pl01a.sht）----
# 狼 = TH17 set 10..14、鹫 = set 15..19；本块存的是有实体记录的 11..14/16..19。
# set 10/15 只有 4 字节终止符，由 _hyper_sets 生成空组 WOLF_0 / EAGLE_0。
# 解码器证据：engine/sht/th17/01-file-layout-and-shooterset-index.md、02-hyper-shot-groups.md。
# 每个 dict 的 +0x00..+0x27 字段与 TH18 同构；func_* 是 TH17 索引；tail 是 TH17 +0x38..+0x57
# 的原始 0x20 字节，会按「TH18 +0x28..+0x2b 插入 4 字节」的映射搬到 TH18 +0x3c..+0x5b。

HYPER_TEMPLATE_PL00A_WOLF = [
    dict(fire_rate=3, start_delay=0, damage=40, spawn_dx=0.0, spawn_dy=0.0, hit_w=112.0, hit_h=32.0, angle=-1.5707963705062866, speed=25.0, opt_slot=0, mode=0, anm=5, sfx=1, func_on_init=0, func_on_tick=0, func_on_draw=0, func_on_hit=1, tail=bytes.fromhex("0000000000000000000000000000000000000000000000000000000000000000")),
    dict(fire_rate=3, start_delay=0, damage=30, spawn_dx=20.0, spawn_dy=20.0, hit_w=112.0, hit_h=32.0, angle=-1.5707963705062866, speed=25.0, opt_slot=0, mode=0, anm=5, sfx=-1, func_on_init=0, func_on_tick=0, func_on_draw=0, func_on_hit=1, tail=bytes.fromhex("0000000000000000000000000000000000000000000000000000000000000000")),
    dict(fire_rate=3, start_delay=0, damage=30, spawn_dx=-20.0, spawn_dy=20.0, hit_w=112.0, hit_h=32.0, angle=-1.5707963705062866, speed=25.0, opt_slot=0, mode=0, anm=5, sfx=-1, func_on_init=0, func_on_tick=0, func_on_draw=0, func_on_hit=1, tail=bytes.fromhex("0000000000000000000000000000000000000000000000000000000000000000")),
    dict(fire_rate=3, start_delay=0, damage=25, spawn_dx=40.0, spawn_dy=30.0, hit_w=112.0, hit_h=32.0, angle=-1.5707963705062866, speed=25.0, opt_slot=0, mode=0, anm=5, sfx=-1, func_on_init=0, func_on_tick=0, func_on_draw=0, func_on_hit=1, tail=bytes.fromhex("0000000000000000000000000000000000000000000000000000000000000000")),
    dict(fire_rate=3, start_delay=0, damage=25, spawn_dx=-40.0, spawn_dy=30.0, hit_w=112.0, hit_h=32.0, angle=-1.5707963705062866, speed=25.0, opt_slot=0, mode=0, anm=5, sfx=-1, func_on_init=0, func_on_tick=0, func_on_draw=0, func_on_hit=1, tail=bytes.fromhex("0000000000000000000000000000000000000000000000000000000000000000")),
    dict(fire_rate=3, start_delay=0, damage=20, spawn_dx=60.0, spawn_dy=30.0, hit_w=112.0, hit_h=32.0, angle=-1.5707963705062866, speed=25.0, opt_slot=0, mode=0, anm=5, sfx=-1, func_on_init=0, func_on_tick=0, func_on_draw=0, func_on_hit=1, tail=bytes.fromhex("0000000000000000000000000000000000000000000000000000000000000000")),
    dict(fire_rate=3, start_delay=0, damage=20, spawn_dx=-60.0, spawn_dy=30.0, hit_w=112.0, hit_h=32.0, angle=-1.5707963705062866, speed=25.0, opt_slot=0, mode=0, anm=5, sfx=-1, func_on_init=0, func_on_tick=0, func_on_draw=0, func_on_hit=1, tail=bytes.fromhex("0000000000000000000000000000000000000000000000000000000000000000")),
]
HYPER_TEMPLATE_PL01A_WOLF = [
    dict(fire_rate=8, start_delay=0, damage=18, spawn_dx=0.0, spawn_dy=0.0, hit_w=48.0, hit_h=18.0, angle=-1.5707963705062866, speed=0.0, opt_slot=0, mode=0, anm=8, sfx=64, func_on_init=0, func_on_tick=3, func_on_draw=0, func_on_hit=6, tail=bytes.fromhex("320000000f000000000000000000000000000000000000000000000000000000")),
    dict(fire_rate=8, start_delay=3, damage=15, spawn_dx=20.0, spawn_dy=0.0, hit_w=48.0, hit_h=18.0, angle=-1.5707963705062866, speed=0.0, opt_slot=0, mode=0, anm=8, sfx=64, func_on_init=0, func_on_tick=3, func_on_draw=0, func_on_hit=6, tail=bytes.fromhex("320000000f000000000000000000000000000000000000000000000000000000")),
    dict(fire_rate=8, start_delay=3, damage=15, spawn_dx=-20.0, spawn_dy=0.0, hit_w=48.0, hit_h=18.0, angle=-1.5707963705062866, speed=0.0, opt_slot=0, mode=0, anm=8, sfx=64, func_on_init=0, func_on_tick=3, func_on_draw=0, func_on_hit=6, tail=bytes.fromhex("320000000f000000000000000000000000000000000000000000000000000000")),
    dict(fire_rate=8, start_delay=6, damage=8, spawn_dx=40.0, spawn_dy=0.0, hit_w=48.0, hit_h=18.0, angle=-1.5707963705062866, speed=0.0, opt_slot=0, mode=0, anm=8, sfx=64, func_on_init=0, func_on_tick=3, func_on_draw=0, func_on_hit=6, tail=bytes.fromhex("320000000f000000000000000000000000000000000000000000000000000000")),
    dict(fire_rate=8, start_delay=6, damage=8, spawn_dx=-40.0, spawn_dy=0.0, hit_w=48.0, hit_h=18.0, angle=-1.5707963705062866, speed=0.0, opt_slot=0, mode=0, anm=8, sfx=64, func_on_init=0, func_on_tick=3, func_on_draw=0, func_on_hit=6, tail=bytes.fromhex("320000000f000000000000000000000000000000000000000000000000000000")),
]
HYPER_TEMPLATE_PL00A_EAGLE = [
    dict(fire_rate=3, start_delay=0, damage=20, spawn_dx=0.0, spawn_dy=0.0, hit_w=16.0, hit_h=16.0, angle=-1.5707963705062866, speed=10.0, opt_slot=0, mode=1, anm=6, sfx=1, func_on_init=1, func_on_tick=1, func_on_draw=0, func_on_hit=0, tail=bytes.fromhex("0000000000000000000000000000000000000000000000000000000000000000")),
    dict(fire_rate=3, start_delay=0, damage=20, spawn_dx=0.0, spawn_dy=0.0, hit_w=16.0, hit_h=16.0, angle=-1.0471975803375244, speed=10.0, opt_slot=0, mode=1, anm=6, sfx=-1, func_on_init=1, func_on_tick=1, func_on_draw=0, func_on_hit=0, tail=bytes.fromhex("0000000000000000000000000000000000000000000000000000000000000000")),
    dict(fire_rate=3, start_delay=0, damage=20, spawn_dx=0.0, spawn_dy=0.0, hit_w=16.0, hit_h=16.0, angle=-2.094395160675049, speed=10.0, opt_slot=0, mode=1, anm=6, sfx=-1, func_on_init=1, func_on_tick=1, func_on_draw=0, func_on_hit=0, tail=bytes.fromhex("0000000000000000000000000000000000000000000000000000000000000000")),
    dict(fire_rate=3, start_delay=0, damage=20, spawn_dx=0.0, spawn_dy=0.0, hit_w=16.0, hit_h=16.0, angle=-0.5235987901687622, speed=10.0, opt_slot=0, mode=1, anm=6, sfx=-1, func_on_init=1, func_on_tick=1, func_on_draw=0, func_on_hit=0, tail=bytes.fromhex("0000000000000000000000000000000000000000000000000000000000000000")),
    dict(fire_rate=3, start_delay=0, damage=20, spawn_dx=0.0, spawn_dy=0.0, hit_w=16.0, hit_h=16.0, angle=-2.6179938316345215, speed=10.0, opt_slot=0, mode=1, anm=6, sfx=-1, func_on_init=1, func_on_tick=1, func_on_draw=0, func_on_hit=0, tail=bytes.fromhex("0000000000000000000000000000000000000000000000000000000000000000")),
]
HYPER_TEMPLATE_PL01A_EAGLE = [
    dict(fire_rate=15, start_delay=0, damage=80, spawn_dx=0.0, spawn_dy=0.0, hit_w=0.0, hit_h=34.0, angle=-1.5707963705062866, speed=0.0, opt_slot=0, mode=2, anm=11, sfx=-1, func_on_init=4, func_on_tick=5, func_on_draw=0, func_on_hit=5, tail=bytes.fromhex("0000000000000000000000000000000000000000000000000000000000000000")),
    dict(fire_rate=15, start_delay=0, damage=60, spawn_dx=0.0, spawn_dy=0.0, hit_w=0.0, hit_h=34.0, angle=-1.3089969158172607, speed=0.0, opt_slot=16, mode=2, anm=11, sfx=-1, func_on_init=4, func_on_tick=5, func_on_draw=0, func_on_hit=5, tail=bytes.fromhex("0000000000000000000000000000000000000000000000000000000000000000")),
    dict(fire_rate=15, start_delay=0, damage=60, spawn_dx=0.0, spawn_dy=0.0, hit_w=0.0, hit_h=34.0, angle=-1.832595705986023, speed=0.0, opt_slot=32, mode=2, anm=11, sfx=-1, func_on_init=4, func_on_tick=5, func_on_draw=0, func_on_hit=5, tail=bytes.fromhex("0000000000000000000000000000000000000000000000000000000000000000")),
    dict(fire_rate=15, start_delay=0, damage=50, spawn_dx=0.0, spawn_dy=0.0, hit_w=0.0, hit_h=34.0, angle=-1.0471975803375244, speed=0.0, opt_slot=48, mode=2, anm=11, sfx=-1, func_on_init=4, func_on_tick=5, func_on_draw=0, func_on_hit=5, tail=bytes.fromhex("0000000000000000000000000000000000000000000000000000000000000000")),
    dict(fire_rate=15, start_delay=0, damage=50, spawn_dx=0.0, spawn_dy=0.0, hit_w=0.0, hit_h=34.0, angle=-2.094395160675049, speed=0.0, opt_slot=64, mode=2, anm=11, sfx=-1, func_on_init=4, func_on_tick=5, func_on_draw=0, func_on_hit=5, tail=bytes.fromhex("0000000000000000000000000000000000000000000000000000000000000000")),
]

# ★ 2026-09-11 七修 / 2026-09-11 八修（AUDIT §AL38）：魔理沙（pl01）EAGLE 的
# “从身体发射”的安全激光组。**九修后已不再使用**（AUDIT §AL39）；保留定义是为了让
# 后续实跑/对照能看到上一版安全组长什么样，真正入口见 HYPER_CUSTOM 的空列表。
# TH17 `pl01a.sht` EAGLE row0 是 `opt_slot=0 / mode=2`
# （自机体激光）；TH18 主炮路径会把这些指标压成非空的子机槽，但**没有真正分配到子机**时
# `tick2`/`hit2` 仍会去读空的 option 坐标，低火力/无 option 下看不到激光。
# 现在由 C 侧 `cards/animal.c` 的 `on_bullet_created` 给本卡 EAGLE set 的 `mode=2` 弹挂一个
# 伪 option（只填 +0x5c/+0x60 = 自机坐标 ×128）；`tick2` 0x45fcce/0x45fcd3 与
# `hit2` 0x460b9e/0x460ba3 都只读这两格，所以子弹既从身体出现、也随身体移动。
# 这里的 `opt_slot` 仍保留 TH18 零售激光形状：
#   · 五条 row 全保留 TH17 的 `damage / hit_h / angle`（80/60/60/50/50、34、-90°±）；
#   · 低半字节 = TH18 主炮路径的 option 槽位；高半字节 = `mode==2` 的
#     `player+0x476fc` 状态槽（`0x45e48d` / `0x46025c` 都取它）。
#     五行必须用不同高半字节（1..5），否则 `Player__shoot_one_bullet` `0x45e96e`
#     见槽非 0 会整行跳过；低半字节只影响伪 option 被覆盖前的槽位，不再决定是否空指针。
#   · TH18 字段取零售激光值：`mode=2`、`anm=6`（pl01.anm script6 = 激光束）、`hit_fx=8`、
#     `hit_w=0`、`hit_h=34`、func init/tick/hit = 2/2/2。
# 这是纯数据 + 一个已有 on_bullet_created 钩子的组合，不碰 `player+0xc` / `player+0x10`。
HYPER_TEMPLATE_PL01A_EAGLE_SAFE = [
    dict(fire_rate=15, start_delay=0, damage=80, spawn_dx=0.0, spawn_dy=0.0, hit_w=0.0, hit_h=34.0, angle=-1.5707963705062866, speed=0.0, opt_slot=0x11, mode=2, anm=6, sfx=-1, hit_fx=8, func_on_init=2, func_on_tick=2, func_on_draw=0, func_on_hit=2),
    dict(fire_rate=15, start_delay=0, damage=60, spawn_dx=0.0, spawn_dy=0.0, hit_w=0.0, hit_h=34.0, angle=-1.3089969158172607, speed=0.0, opt_slot=0x21, mode=2, anm=6, sfx=-1, hit_fx=8, func_on_init=2, func_on_tick=2, func_on_draw=0, func_on_hit=2),
    dict(fire_rate=15, start_delay=0, damage=60, spawn_dx=0.0, spawn_dy=0.0, hit_w=0.0, hit_h=34.0, angle=-1.832595705986023, speed=0.0, opt_slot=0x32, mode=2, anm=6, sfx=-1, hit_fx=8, func_on_init=2, func_on_tick=2, func_on_draw=0, func_on_hit=2),
    dict(fire_rate=15, start_delay=0, damage=50, spawn_dx=0.0, spawn_dy=0.0, hit_w=0.0, hit_h=34.0, angle=-1.0471975803375244, speed=0.0, opt_slot=0x43, mode=2, anm=6, sfx=-1, hit_fx=8, func_on_init=2, func_on_tick=2, func_on_draw=0, func_on_hit=2),
    dict(fire_rate=15, start_delay=0, damage=50, spawn_dx=0.0, spawn_dy=0.0, hit_w=0.0, hit_h=34.0, angle=-2.094395160675049, speed=0.0, opt_slot=0x54, mode=2, anm=6, sfx=-1, hit_fx=8, func_on_init=2, func_on_tick=2, func_on_draw=0, func_on_hit=2),
]


HYPER_FUNC_MAP = {
    "init": {0: 0, 1: 1, 4: 2},          # 3/5 无等价；TH18 init 表没有「发射时生成 4 个伤害源」
    "tick": {0: 0, 1: 1, 3: 3, 5: 2},    # 6 无等价；TH18 tick 表没有「每帧 +damage」
    "hit":  {0: 0, 1: 1, 5: 2, 6: 4},    # 4 无等价；6 → 4 见 02 的 🟡 表
}

# 目标文件 -> TH17 sht `anm` 字段值 -> 目标 pl0X.anm 脚本号。
#
# ★ 2026-09-11（四修，AUDIT §AL33）：不再追加任何 entry / 脚本。
# TH17 `PlayerBullet__fire`（`th17:0x44a180`）在 `0x44a535` 做
# `movsx eax, word [shooter+0x22]; add eax, 5`，实际读 **`pl0X.anm script(anm+5)`**；
# TH18 同字段直接读 `pl0X.anm script(anm)`。TH18 零售 `pl00.anm` / `pl01.anm` 里
# **已经逐字节含有**这几支 TH17 兽灵脚本（entry0 贴图/sprite 表也与 TH17 同名自机一致），
# 所以直接指过去即可：5→10 狼、6→11 鹫、8→13 狼、11→16 鹫、7→8 是 hit5 硬编码的束端。
# `build_player_anm.py` 现在只把零售 pl0X.anm 原样拷贝 + 硬校验，绝不再追加。
#
# pl02 / pl03 没有 TH17 对应自机，零售文件里没有兽灵脚本/贴图 ⇒ **安全回退**：
# identity 指回各自零售普通自机弹脚本（不会崩、不会出自机图），视觉目标暂未达成。
HYPER_ANM_MAP = {
    "pl00.sht": {5: 10, 6: 11, 7: 8, 8: 13, 11: 16},   # pl00.anm 零售兽灵脚本（TH17 逐字节同）
    # ★ 2026-09-11 九修（AUDIT §AL39）：pl01 WOLF 仍按 TH17 行直指零售脚本；
    #   EAGLE 改走 `HYPER_CUSTOM` 的空列表（主炮留空、卡侧矩形束），本 map 的 6/11
    #   两项仅保留给仍要翻译的模板，pl01 EAGLE 不再使用。
    "pl01.sht": {5: 10, 6: 11, 7: 8, 8: 13, 11: 16},
    "pl02.sht": {5: 5, 6: 6, 7: 7, 8: 8, 11: 11},      # 咲夜：安全回退 identity（普通自机弹）
    "pl03.sht": {5: 5, 6: 6, 7: 7, 8: 8, 11: 11},      # 早苗：安全回退 identity（普通自机弹）
}

# ★ 2026-09-11 七修（AUDIT §AL37）：**已翻译成 TH18 形状**的覆盖组。
# 与下面的 `HYPER_FALLBACK` 是两套不同的东西：
#   · HYPER_CUSTOM 的行已经是 TH18 字段/函数索引/零售 pl0X.anm 脚本号，直接 pack，不再过
#     `_map_hyper_row`；
#   · HYPER_FALLBACK 仍按「零售 set 索引」逐字节拷整组，用于还没翻译好、或实跑未坐实的兽灵组。
HYPER_CUSTOM = {
    # ★ 2026-09-11 九修（AUDIT §AL39）：pl01 EAGLE 不再走主炮 shooter 行。
    # 用户实跑确认 TH17 `mode=2` 主炮路径会变成「带激光贴图的移动弹」，且
    # 「松开 Z 进入咆哮再按 Z 无事」与「按住 Z 进入咆哮」两条路都不稳。按用户
    # 已批准的 end state，EAGLE 的 main set 留空：由 `animal.c` 的卡侧矩形束负责
    # 视觉 + 伤害 + 消弹。0x23..0x27 仍是合法空组（与 WOLF_0 同形，只有终止符），
    # `on_shot_set_index` 返回这些索引时引擎不会创建 bullet。
    ("pl01.sht", "EAGLE"): [],
}

# ★ 2026-09-11 七修（§AL37）：Task2 的反汇编把「主炮路径会拿 ability.anm」证伪后，
# pl01 不再需要任何整组零售回退。WOLF 回到 TH17 行 → `HYPER_ANM_MAP` 直指零售
# `pl00/pl01.anm` 里逐字节存在的 `script(anm+5)`；EAGLE 走 `HYPER_CUSTOM`。
# 保留空 dict 是为了让 `_hyper_sets` 的两条安全网（custom / fallback）都在代码里可查：
# 以后任何 group 只要实跑还没坐实，填一条 set 索引回退即可，不必改生成器。
HYPER_FALLBACK = {}

HYPER_SOURCE = {
    "pl00.sht": {"WOLF": "pl00a.sht", "EAGLE": "pl00a.sht"},
    "pl01.sht": {"WOLF": "pl01a.sht", "EAGLE": "pl01a.sht"},
    "pl02.sht": {"WOLF": "pl00a.sht", "EAGLE": "pl00a.sht"},
    "pl03.sht": {"WOLF": "pl01a.sht", "EAGLE": "pl01a.sht"},
}

HYPER_TEMPLATES = {
    ("pl00a.sht", "WOLF"):  HYPER_TEMPLATE_PL00A_WOLF,
    ("pl00a.sht", "EAGLE"): HYPER_TEMPLATE_PL00A_EAGLE,
    ("pl01a.sht", "WOLF"):  HYPER_TEMPLATE_PL01A_WOLF,
    ("pl01a.sht", "EAGLE"): HYPER_TEMPLATE_PL01A_EAGLE,
}


def pack_shooter(s: dict, anm_ids: dict) -> bytes:
    rate = s["fire_rate"]
    if not 1 <= rate <= 127:
        raise SystemExit(f"fire_rate={rate} 不在 1..127：0 会在装备卡路径上除零，负数是终止符")
    anm = s["anm"]
    anm = anm_ids[anm] if isinstance(anm, str) else anm
    b = bytearray(STRIDE)
    struct.pack_into("<bbh", b, 0x00, rate, s["start_delay"], s["damage"])
    struct.pack_into("<ff", b, 0x04, s.get("spawn_dx", 0.0), s.get("spawn_dy", 0.0))
    struct.pack_into("<ff", b, 0x0C, s["hit_w"], s["hit_h"])
    struct.pack_into("<ff", b, 0x14, s["angle"], s["speed"])
    struct.pack_into("<bb", b, 0x20, s["opt_slot"], s["mode"])
    struct.pack_into("<hh", b, 0x22, anm, s["sfx"])
    # +0x26 = 激光命中特效的 anm 脚本号（只有 hit2 `0x460320` 读；零售魔理沙「迷你八卦炉」= 5）
    hf = s.get("hit_fx", 0)
    hf = anm_ids[hf] if isinstance(hf, str) else hf
    struct.pack_into("<h", b, 0x26, hf)
    struct.pack_into("<bb", b, 0x2A, s.get("fire_rate2", 0), s.get("start_delay2", 0))
    struct.pack_into("<4i", b, 0x2C, s["func_on_init"], s.get("func_on_tick", 0),
                     s.get("func_on_draw", 0), s.get("func_on_hit", 0))
    # tail = TH17 +0x38..+0x57 原样搬到 TH18 +0x3c..+0x5b（TH18 在 +0x28 插了 4 字节）。
    tail = s.get("tail", b"")
    if isinstance(tail, str):
        tail = bytes.fromhex(tail)
    if len(tail) > 0x20:
        raise SystemExit(f"tail 长度 {len(tail)} > 0x20")
    b[0x3C:0x3C + len(tail)] = tail
    return bytes(b)


def _map_hyper_row(row: dict, target_file: str) -> dict:
    out = dict(row)
    anm_map = HYPER_ANM_MAP[target_file]
    try:
        out["anm"] = anm_map[row["anm"]]
    except KeyError as e:
        raise SystemExit(f"{target_file}: 没有 TH17 anm={row['anm']} 的映射") from e
    for slot, key in (("init", "func_on_init"), ("tick", "func_on_tick"), ("hit", "func_on_hit")):
        src = row.get(key, 0)
        try:
            out[key] = HYPER_FUNC_MAP[slot][src]
        except KeyError as e:
            raise SystemExit(f"{target_file}: 没有 TH17 {slot} idx={src} 的映射") from e
    out["func_on_draw"] = 0
    if row.get("func_on_hit", 0) == 5:
        # TH17 hit5 内部硬编码 pl0X.anm script8 当束端特效（`th17:0x44c462` 同样取
        # PLAYER_PTR+0xc）；TH18 hit2 改读 shooter+0x26。我们对应该 slot7 的
        # TH18 原生一次性脚本（不是 slot8/11 的子弹本体脚本）。
        out["hit_fx"] = anm_map.get(7, 0)
    else:
        out["hit_fx"] = 0
    return out


def _common_sets(anm_ids: dict) -> list:
    return [(nm, b"".join(pack_shooter(s, anm_ids) for s in rows) + TERMINATOR)
            for nm, rows in APPEND]


def _retail_set_blob(data: bytes, off: int) -> bytes:
    """按文件相对偏移 off 拷一段零售 shooterset 原始字节（含终止符）。"""
    p = DATA_BASE + off
    while struct.unpack_from("<i", data, p)[0] != -1:
        p += STRIDE
        if p > len(data):
            raise SystemExit("零售 shooterset 没有终止符")
    return data[DATA_BASE + off:p + 4]


def _hyper_sets(target_file: str, anm_ids: dict) -> list:
    """按文件/家族生成 5 个追加组。三种来源，优先级：
       1. HYPER_CUSTOM —— 已经是 TH18 形状的行；pl01 EAGLE 现在登记为空列表（空组）；
       2. HYPER_FALLBACK —— 逐字节拷零售 shooterset（当前为空；留给实跑未坐实的组）；
       3. HYPER_TEMPLATES —— TH17 行经 `_map_hyper_row` 翻译（pl00、pl01 WOLF、pl02、pl03）。
       每个 family 仍生成 5 组、索引连续：*_0 = 空组（只有终止符），*_1..*_4 = 实际行。
    """
    out = []
    for family in ("WOLF", "EAGLE"):
        custom = HYPER_CUSTOM.get((target_file, family))
        if custom is not None:
            blob = b"".join(pack_shooter(r, anm_ids) for r in custom) + TERMINATOR
            for power in range(5):
                out.append((f"{family}_{power}", TERMINATOR if power == 0 else blob))
            print(f"  {target_file}: {family} = TH18 形状自定义组 "
                  f"({len(custom)} shooter/组，索引 0x1e+ 起)")
            continue

        fallback = HYPER_FALLBACK.get((target_file, family))
        if fallback is not None:
            data = (RETAIL / target_file).read_bytes()
            offs = offsets(data)
            for power, src_idx in enumerate(fallback):
                off = offs[src_idx]
                if off == 0:
                    raise SystemExit(f"{target_file}: 回退目标 set {src_idx:#x} 为空")
                out.append((f"{family}_{power}", _retail_set_blob(data, off)))
            print("  " + target_file + f": {family} 安全回退 -> 零售行 " +
                  ", ".join(f"{family}_{i}=set{si:#x}" for i, si in enumerate(fallback)))
            continue

        src = HYPER_SOURCE[target_file][family]
        rows = [_map_hyper_row(r, target_file) for r in HYPER_TEMPLATES[(src, family)]]
        blob = b"".join(pack_shooter(r, anm_ids) for r in rows) + TERMINATOR
        for power in range(5):
            out.append((f"{family}_{power}", TERMINATOR if power == 0 else blob))
    return out


def sets_for_file(target_file: str, anm_ids: dict) -> list:
    return _common_sets(anm_ids) + _hyper_sets(target_file, anm_ids)


def read_anm_ids() -> dict:
    if not ANM_IDS_H.is_file():
        raise SystemExit(f"缺 {ANM_IDS_H}：先跑 make anm")
    out = {}
    for line in ANM_IDS_H.read_text(encoding="utf-8").splitlines():
        parts = line.split()
        if len(parts) >= 3 and parts[0] == "#define" and parts[2].rstrip("/*").strip().isdigit():
            out[parts[1]] = int(parts[2])
    return out


def offsets(data: bytes) -> list:
    return list(struct.unpack_from(f"<{N_SLOTS}I", data, OFF_ARRAY))


def walk_set(data: bytes, off: int) -> int:
    """→ 该 shooterset 的字节长度（含 4 字节终止符）。"""
    p = DATA_BASE + off
    while struct.unpack_from("<i", data, p)[0] != -1:
        p += STRIDE
        if p > len(data):
            raise SystemExit("shooterset 没有终止符，文件坏了")
    return p + 4 - (DATA_BASE + off)


def check_retail_shape(name: str, data: bytes):
    n = struct.unpack_from("<H", data, 0x02)[0]
    if n != N_SLOTS:
        raise SystemExit(f"{name}: 头部 +0x02 = {n}，期望 {N_SLOTS}")
    offs = offsets(data)
    if offs[0] != 0:
        raise SystemExit(f"{name}: 第 0 项偏移不是 0")
    # set 0 的 func 字段必须全 0（空位重复解析靠它幂等）
    p = DATA_BASE
    while struct.unpack_from("<i", data, p)[0] != -1:
        if any(struct.unpack_from("<4i", data, p + 0x2C)):
            raise SystemExit(f"{name}: shooterset 0 的 func 字段不全为 0 —— 空位会被二次解析成野指针")
        p += STRIDE
    return offs


def build_one(name: str, retail: bytes, sets: list) -> bytes:
    offs = check_retail_shape(name, retail)
    used = sum(1 for i, o in enumerate(offs) if i == 0 or o != 0)
    if used != N_RETAIL:
        raise SystemExit(f"{name}: 零售用了 {used} 项，期望 {N_RETAIL}")
    if N_RETAIL + len(sets) > N_SLOTS:
        raise SystemExit(f"偏移数组只有 {N_SLOTS} 项，零售占 {N_RETAIL}，放不下 {len(sets)} 组")

    out = bytearray(retail)
    for k, (_, blob) in enumerate(sets):
        idx = N_RETAIL + k
        struct.pack_into("<I", out, OFF_ARRAY + idx * 4, len(out) - DATA_BASE)
        out += blob
    return bytes(out)


def verify(name: str, retail: bytes, built: bytes, sets: list):
    offs_r, offs_b = offsets(retail), offsets(built)
    check_retail_shape(name, built)
    if offs_b[:N_RETAIL] != offs_r[:N_RETAIL]:
        raise SystemExit(f"{name}: 前 {N_RETAIL} 项偏移被动过")
    for i in range(N_RETAIL):
        n = walk_set(retail, offs_r[i])
        a, b = DATA_BASE + offs_r[i], DATA_BASE + offs_b[i]
        if retail[a:a + n] != built[b:b + n]:
            raise SystemExit(f"{name}: shooterset {i:#x} 的字节变了")
    for k, (nm, blob) in enumerate(sets):
        idx = N_RETAIL + k
        o = offs_b[idx]
        if o == 0:
            raise SystemExit(f"{name}: 第 {idx:#x} 项没写偏移")
        if built[DATA_BASE + o:DATA_BASE + o + len(blob)] != blob:
            raise SystemExit(f"{name}: {nm} 的数据对不上")
        if walk_set(built, o) != len(blob):
            raise SystemExit(f"{name}: {nm} 的终止符位置不对")
    for i in range(N_RETAIL + len(sets), N_SLOTS):
        if offs_b[i] != 0:
            raise SystemExit(f"{name}: 第 {i:#x} 项本该留空")


def write_ids_h(sets: list):
    lines = ["/* 由 assets/sht/append_shooterset.py 生成 —— 追加进 pl0X.sht 的 shooterset 索引。别手改。 */",
             "#pragma once", ""]
    for k, (nm, blob) in enumerate(sets):
        lines.append(f"#define CE_SHT_SET_{nm:<24} {N_RETAIL + k:#04x}   "
                     f"/* {len(blob) // STRIDE} shooter */")
    IDS_H.write_text("\n".join(lines) + "\n", encoding="utf-8")


def main():
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    ap.add_argument("--verify-only", action="store_true")
    a = ap.parse_args()

    anm_ids = read_anm_ids()
    sets_by_file = {name: sets_for_file(name, anm_ids) for name in FILES}
    canonical = sets_by_file[FILES[0]]

    if not canonical:                               # 没有要追加的：不产出（别发一份和零售一模一样的 .sht）
        if not a.verify_only:
            for name in FILES:
                (OUT / name).unlink(missing_ok=True)
            write_ids_h(canonical)
        print("  没有登记任何 shooterset：不产出 .sht（build/sht/ 已清空）")
        return 0

    OUT.mkdir(parents=True, exist_ok=True)
    for name in FILES:
        src = RETAIL / name
        if not src.is_file():
            raise SystemExit(f"缺零售样本 {src}（用户放进 local/，见 local/README.md）")
        retail = src.read_bytes()
        sets = sets_by_file[name]
        dst = OUT / name
        if a.verify_only:
            if not dst.is_file():
                raise SystemExit(f"缺 {dst}：先跑 make sht")
            built = dst.read_bytes()
        else:
            built = build_one(name, retail, sets)
            dst.write_bytes(built)
        verify(name, retail, built, sets)
        print(f"  {name}: {len(retail)} → {len(built)} 字节，追加 {len(sets)} 组"
              f"（索引 {N_RETAIL:#04x}..{N_RETAIL + len(sets) - 1:#04x}）")

    if not a.verify_only:
        write_ids_h(canonical)
        print(f"wrote {IDS_H.relative_to(MOD)}")
    for k, (nm, blob) in enumerate(canonical):
        print(f"  CE_SHT_SET_{nm} = {N_RETAIL + k:#04x}  ({len(blob) // STRIDE} shooter)")
    return 0


if __name__ == "__main__":
    sys.exit(main())
