#!/usr/bin/env python3
"""往原生的四个 `pl0X.sht` 追加「新卡子机」的 shooterset，输出到 native/build/sht/，并生成 native/sht_ids.h。

    python3 append_shooterset.py                # 构建 + 自检 + 写 sht_ids.h
    python3 append_shooterset.py --verify-only  # 只自检已有的 build/sht/*.sht

## 为什么能纯追加

`pl0X.sht` 的 `+0xe0` 是一张 **40 项**的 shooterset 偏移数组（数据区起点 `+0x180` 是解析器里的硬编码
常量），原生只用了 23 项（`0x00`–`0x09` 主炮、`0x0a`–`0x16` 装备卡子机），**尾部 17 项是 0**。
所以「填一项 + 文件尾接一段」不动前面任何一个字节。
一手：`engine/sht/th18/01-file-layout-and-shooterset-index.md`。

## 两条路：**追加**新索引（`APPEND`）/ **覆盖**原生空槽（`REPLACE`）

- 偏移数组只有 40 格，`0x00`–`0x16` 原生、`0x17`–`0x27` 本 mod 已用满 ⇒ 再要一格只能
  **复用原生里没有卡映射的那一格**（`REPLACE`，现在只有 108 的妖侧 set 用 0x16）。
  覆盖的写法是「新记录接在文件尾 + 把该槽的偏移指过去」，原生原来的 96 字节原地留着（死数据）。
- `APPEND` 分配 `0x17` 起；`REPLACE` 不新增索引、也**不挪动任何既有索引**。

## 四条必须守住的不变式（脚本每次都回读校验）

1. 头部 `+0x02` 仍是 40 —— 它是解析循环的上界。
2. **shooterset 0 的四个 func 字段仍全是 0**。剩下的空位仍指向 set 0，会被重复解析；
   靠 `0 → 表[0] = NULL → 0` 幂等才不炸。改坏它 = 开局崩。
3. **除 `REPLACE` 点名的槽外**，前 23 组 shooterset 的字节逐字节不变，偏移也不变。
4. 被 `REPLACE` 覆盖的槽：原生原记录**原地原样**、新记录落在**追加区**（文件尾），
   且除该槽的偏移项外偏移数组一个字节不动。

另外 `fire_rate` 必须 1–127：`0` 在装备卡路径上会整数除零（`0x40a9c0` 没有零分支），
`< 0` 是 shooterset 的终止符。
"""
import argparse
import re
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
PL_IDS_H = MOD / "native" / "pl_anm_ids.h"       # 由 assets/build_player_anm.py 生成
YOUKAI_CORE_H = MOD / "native" / "cards" / "youkai_core.h"
FILES = ("pl00.sht", "pl01.sht", "pl02.sht", "pl03.sht")

OFF_ARRAY = 0xE0        # 偏移数组
DATA_BASE = 0x180       # 解析器里的硬编码常量
N_SLOTS = 40            # 偏移数组长度（= 头部 +0x02）
N_RETAIL = 23           # 原生用掉的项（0x00–0x16）
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
    # ★★ 2026-09-10 第二十八轮：**照原生「ミニ八卦炉」重做激光**（一手：`ability.txt` @MARISA_OP
    # = 卡 10 = SHT set **0xb**，四个角色的 set 0xb 逐字段相同）：
    #   `rate=5 delay=0 dmg=13 ang=−π/2 **speed=0** mode=**2** anm=**3** sfx=−1 hit_fx(+0x26)=**5**
    #    func=init2/tick2/hit2`，而 `ability.anm` 的 **script3** 就是激光束（`anchor(1,0)` +
    #   `scale(1,0)→scaleTime(4,1,1,0.5)` + `blendMode(1)` + `scrollX(−1/21)` + `scriptNew(script4)` 光斑）。
    # ⇒ 上次「损坏内存」不是这条路的锅，是我们没照原生参数填（mode=0 / 无 hit_fx / 用了别的脚本）。
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


# ---- 覆盖原生空槽：妖侧 shooterset（卡 108「永夜抄」）-----------------------------
#
# ## 为什么是「覆盖 0x16」而不是「追加一个新索引」
#
# 一手（本仓字节扫描 + 逐文件解析，2026-09-12）：
#   · 偏移数组 40 格**全满**：四份原生 `pl0X.sht` 的 `+0x02` 都是 40，`0x00`–`0x16` 逐项非 0；
#     本 mod 的 `APPEND` 又占掉 `0x17`–`0x27`（见 `native/sht_ids.h`）⇒ `build_one()` 的
#     `N_RETAIL + len(sets) > N_SLOTS` 会直接拦住「再加一格」。
#   · 槽 `0x16` 是原生**唯一没有卡映射**的一格：扫全 exe 的 `E8 rel32`，调用
#     `Player__tick_shooters_for_ability_card` `0x40a9c0` 共 11 处，压栈的 set 立即数
#     = {0x0a,0x0b,0x0c/0x0d,0x0e,0x0f,0x10,0x11,0x12,0x13,0x14,0x15}，**没有 0x16**；
#     主炮路径 `0x45ea00` 的公式（`CURRENT_POWER/100`，聚焦 +5）只给 `0x00`–`0x09`。
#   · 它原本 `fire_rate = 0`（四个文件逐字节一致：rate 0 / dmg 16 / anm 21 / func_on_init 7），
#     而 `0x40a9c0` 的 `idiv ecx` 在 `0x40aa4f` **没有零分支** ⇒ 谁把它喂进装备卡路径谁除零。
#     改成合法记录（1–127）只会更安全。
#
# ## 写法
#
# 新记录**接在文件尾**，把偏移数组第 `0x16` 项指过去；原生那 96 字节原地留着当死数据。
# ⇒ 除 `offset[0x16]` 一个 dword 外，文件其余部分**逐字节不变**（`verify()` 会断言），
#   而且一个槽可以放任意多行（下面的 9 行就是 TH08 一档的完整形态）。
#
# ## 参数来源（每个值）
#
# TH08 v1.00d 的妖侧 sht = **`ply03as.sht`**（妖夢＆幽々子 的妖侧；人侧是 `ply03a.sht`）。
#   ⚠ 文件名不是 `ply0Xbs.sht`：一手扫 `th08.exe` 的字符串表只有 8 个名
#     （`ply00a/01a/02a/03a` + `ply00as/01as/02as/03as`），人/妖两张 12 项文件名表在
#     `th08:0x4c7ce0`（人）/ `th08:0x4c7d10`（妖）—— team 0..3 的妖侧 = `plyNN**as**.sht`。
# 取 **第 5 档（`thr = 128` = TH08 满 P；TH08 的档阈值是 8/24/48/80/128/999，
#   `th08:0x451005`–`0x451020` 的推进循环）**，全部 9 行照抄。选 ply03as 的理由：
#   它的第 5 档 **9 行的 `rec[+0x28]`（专用发射函数索引）全是 0** = 默认发射路径
#   （`th08:0x44fd80`：节奏判定 + `th08:0x44fb70` 发一发）⇒ 每个字段都能 1:1 翻译、无需猜函数。
#   （`ply00as` 的妖侧同档有 2 行 `rec[+0x28] = 1` → `th08:0x450240` = 「搭档同时开火」机制，
#     读 `player+0xe2abc` 的位置再补一发，TH18 **没有搭档** ⇒ 那两行不翻译，见报告。）
#
# 逐字段（TH08 `rec[+NN]` → TH18 `+NN`，字段图见 `engine/sht/th18/02-shooter-record.md` §1）：
#   `+0x00` 周期 → `+0x00` fire_rate（两侧判据同形：`timer % rate == phase`，
#           TH08 `th08:0x45024f` / TH18 `0x40aa4f`）；`+0x02` 相位 → `+0x01` start_delay（全 0）；
#   `+0x1c` 伤害 → `+0x02` damage；`+0x04/+0x08` 出膛偏移 → `+0x04/+0x08`
#           （TH18 侧一手：`0x45e6dd`/`0x45e6f1` 把 `shooter[+4]/[+8]` 加到弹坐标上）；
#   `+0x14` 角度 → `+0x14`（两侧都是 rad、−π/2 = 上）；`+0x18` 速度 → `+0x18`；
#   `+0x26` 音效 → `+0x24` sfx（两侧都是 `< 0` = 不响；TH08 只有中央那行是 0 = 出膛音，
#           与 TH18 原生多行组的「首行 0、其余 −1」是同一套约定）；
#   `+0x28` 专用发射函数 → `+0x2c..+0x38` 四个 func **全 0**（TH08 侧 = 0 就是「无专用函数」，
#           TH18 侧全 0 = 平弹；装载表 `0x4b4230[0] = 0` ⇒ `0 → 0` 幂等）。
#
# TH18 有、TH08 没有（或语义对不上）的字段 ⇒ **取 TH18 原生里语义最接近的值**：
#   `+0x0c/+0x10`（TH18 → `bullet+0xa0/+0xa4`，TH16 同构字段是 hitbox_x/y，原版同形组
#       0x0a/0x0f/0x16 都是 `(16.0, 16.0)`）：TH08 的 `rec[+0x0c]/[+0x10]` 有数据
#       （本档全是 `18.0`）但**全 exe 从不读**（只有 14 处直接位移引用、全是写）⇒ 语义对不上，
#       照原生同形组填 `(16.0, 16.0)`，不照抄一个没人读的数。
#   `+0x20` opt_slot：TH08 `rec[+0x20]` 是子机索引（0 = 本体 / 1 / 2），但 TH18 的
#       **卡路径恒用传进来的那颗子机坐标**（`0x40aa0c`/`0x40aa21` 一手：膛口只算一次）、
#       `opt_slot` 只决定 `mode==2` 的另一支 ⇒ 填 `0`（原生同形组也全是 0）。
#   `+0x21` mode = `0`（TH08 无此字段；原版 0x16 自己是 0，`mode==4/5/6` 会跳过那次
#       PosVel 修正 `0x45e6ce`，不是我们要的）。
#   `+0x22` anm = `21`：TH08 用 `2/4/6`（它自己的 `pl0X.anm`，且是三种不同大小），
#       TH18 卡路径把 `player+0x10` 换成 `ability.anm`（`0x40aa03` 一手）⇒ 数字空间不同。
#       取**这一格原生自己的 21**（原生 `ability.anm` 的合法脚本号，零耦合）——
#       TH08 的三种弹贴图要在 `ability.anm` 里新建脚本才谈得上，见报告 ❓。
#   `+0x26` hit_fx = 0（只有 hit 表 idx2 的激光读它，我们不是激光）；`+0x2a/+0x2b` = 0/0
#       （TH08 没有第二计时器；`0x40aa40` 的 `test al,al; jne` 一手：0 = 走 fire_rate 那支）；
#   `+0x4c` 横移倾斜 = 0（只有 `func_on_init == 3` 才读）；
#   `+0x1c..+0x1f` / `+0x3c..+0x4b` / `+0x50..+0x5b` = 0（TH18 原生 268 行全 0 的字段）。
#   ❓ TH08 的 `rec[+0x1e]`（`side`，ply00as 是 ±20..50、ply03as 全 0）与 `rec[+0x22]`（弹种，
#      本档全 0 = 普通命中）在 TH18 记录里**没有对应字段** ⇒ 不翻译。
def _yk_row(rate, damage, dx, angle, sfx):
    """一行 = 一个 TH18 shooter 记录 dict（字段名见 pack_shooter）。"""
    return dict(fire_rate=rate, start_delay=0, damage=damage,
                spawn_dx=dx, spawn_dy=0.0, hit_w=16.0, hit_h=16.0,
                angle=angle, speed=12.0, opt_slot=0, mode=0,
                anm=21, sfx=sfx, hit_fx=0, fire_rate2=0, start_delay2=0,
                func_on_init=0, func_on_tick=0, func_on_draw=0, func_on_hit=0)


# (名字, 槽号, [shooter, …]) —— 名字会生成 `#define CE_SHT_SET_<名字>`，
# 同时被 `check_card_constants()` 拿去核对 `cards/youkai_core.h` 里的 `YK_SHOT_SET_<名字>`。
REPLACE = [
    ("YOUKAI", 0x16, [
        # ply03as.sht tier5(thr=128) 的 9 行，逐行照抄（TH08 anm 值写在行末注释里）
        _yk_row(rate=10, damage=24, dx=-12.0, angle=-1.780236, sfx=-1),   # row0 anm4 外侧左
        _yk_row(rate=5,  damage=26, dx=-6.0,  angle=-1.675516, sfx=-1),   # row1 anm2
        _yk_row(rate=3,  damage=30, dx=0.0,   angle=-1.570796, sfx=0),    # row2 anm2 中央（出膛音）
        _yk_row(rate=5,  damage=26, dx=6.0,   angle=-1.466077, sfx=-1),   # row3 anm2
        _yk_row(rate=10, damage=24, dx=12.0,  angle=-1.361357, sfx=-1),   # row4 anm4 外侧右
        _yk_row(rate=6,  damage=22, dx=0.0,   angle=-1.850049, sfx=-1),   # row5 anm6 子机1 最外左
        _yk_row(rate=5,  damage=24, dx=0.0,   angle=-1.710423, sfx=-1),   # row6 anm4 子机1
        _yk_row(rate=5,  damage=24, dx=0.0,   angle=-1.431170, sfx=-1),   # row7 anm4 子机2
        _yk_row(rate=6,  damage=22, dx=0.0,   angle=-1.291544, sfx=-1),   # row8 anm6 子机2 最外右
    ]),
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
# 这里的 `opt_slot` 仍保留 TH18 原生激光形状：
#   · 五条 row 全保留 TH17 的 `damage / hit_h / angle`（80/60/60/50/50、34、-90°±）；
#   · 低半字节 = TH18 主炮路径的 option 槽位；高半字节 = `mode==2` 的
#     `player+0x476fc` 状态槽（`0x45e48d` / `0x46025c` 都取它）。
#     五行必须用不同高半字节（1..5），否则 `Player__shoot_one_bullet` `0x45e96e`
#     见槽非 0 会整行跳过；低半字节只影响伪 option 被覆盖前的槽位，不再决定是否空指针。
#   · TH18 字段取原生激光值：`mode=2`、`anm=6`（pl01.anm script6 = 激光束）、`hit_fx=8`、
#     `hit_w=0`、`hit_h=34`、func init/tick/hit = 2/2/2。
# 这是纯数据 + 一个已有 on_bullet_created 钩子的组合，不碰 `player+0xc` / `player+0x10`。
HYPER_TEMPLATE_PL01A_EAGLE_SAFE = [
    dict(fire_rate=15, start_delay=0, damage=80, spawn_dx=0.0, spawn_dy=0.0, hit_w=0.0, hit_h=34.0, angle=-1.5707963705062866, speed=0.0, opt_slot=0x11, mode=2, anm=6, sfx=-1, hit_fx=8, func_on_init=2, func_on_tick=2, func_on_draw=0, func_on_hit=2),
    dict(fire_rate=15, start_delay=0, damage=60, spawn_dx=0.0, spawn_dy=0.0, hit_w=0.0, hit_h=34.0, angle=-1.3089969158172607, speed=0.0, opt_slot=0x21, mode=2, anm=6, sfx=-1, hit_fx=8, func_on_init=2, func_on_tick=2, func_on_draw=0, func_on_hit=2),
    dict(fire_rate=15, start_delay=0, damage=60, spawn_dx=0.0, spawn_dy=0.0, hit_w=0.0, hit_h=34.0, angle=-1.832595705986023, speed=0.0, opt_slot=0x32, mode=2, anm=6, sfx=-1, hit_fx=8, func_on_init=2, func_on_tick=2, func_on_draw=0, func_on_hit=2),
    dict(fire_rate=15, start_delay=0, damage=50, spawn_dx=0.0, spawn_dy=0.0, hit_w=0.0, hit_h=34.0, angle=-1.0471975803375244, speed=0.0, opt_slot=0x43, mode=2, anm=6, sfx=-1, hit_fx=8, func_on_init=2, func_on_tick=2, func_on_draw=0, func_on_hit=2),
    dict(fire_rate=15, start_delay=0, damage=50, spawn_dx=0.0, spawn_dy=0.0, hit_w=0.0, hit_h=34.0, angle=-2.094395160675049, speed=0.0, opt_slot=0x54, mode=2, anm=6, sfx=-1, hit_fx=8, func_on_init=2, func_on_tick=2, func_on_draw=0, func_on_hit=2),
]


# =====================================================================
# ★ 2026-09-13（AL59）：pl02（咲夜）/ pl03（早苗）的兽灵咆哮（替掉「安全回退」）。
#
# 一手依据（详见 TUNING.md §2.1b / AUDIT §AL59）：
#   · 早苗（pl03）的**零售主炮**里本来就有青蛙和蛇（`pl03.sht` set0x01..0x04 / 0x06..0x09）：
#       青蛙 anm=8 → `pl03.anm` script8（sprite34 30×30；`interruptLabel(1)` 播 sprite40..43
#              = 128×128 爆炸），func=0/0/0/4（TH18 hit4 = 伤害源 + `interrupt_tree(vm,1)`），
#              `+0x4c = 2.0`；
#       蛇   anm=6 → `pl03.anm` script6（sprite30..33 64×16 四帧、`anchor(2,0)`），
#              func=4/4/0/5（tick4 = 锁敌→水平冲刺 = 90° 折线）。
#   · TH17 **没有**咲夜/早苗的兽灵行（TH17 pl02 是妖梦）⇒ 这两套只按 DPS 近似一手量级，
#     数值全部是「❓ 自定 + 具名常量」。
#   · 咲夜 WOLF = 仿灵梦 7 行，但每行角度改成朝自机前方一个汇聚点（静态聚焦）。
#   · 咲夜 EAGLE = 卡侧（`cards/animal.c` 的 `sakuya_eagle_tick`），**主炮组留空**。
#
# DPS 目标（每帧伤害 = Σ每发伤害 / 射速；算式与对照见 TUNING.md §2.1b）：
SAKUYA_WOLF_DPS_TARGET  = 190.0 / 3.0      # ≈ 63.33，= 灵梦 WOLF 一手（100.0%）
# ★ AL65（用户订正）：早苗狼要对齐的是「**灵梦的大鹫**」（5 行 rate3 × 20 ⇒ 100/3 = 33.33），
#   不是灵梦狼（63.33）。⇒ 早苗狼直接采用灵梦鹫的逐行 dmg/rate（20 / 3），DPS 精确 33.33、偏差 0%；
#   5 行、视觉 ×2、`tick4` 90° 折线机制全部保留。
SANAE_WOLF_DPS_TARGET   = 5 * 20.0 / 3.0  # = 33.33（5 行 rate3 各 20）
# ★ AL65 #2：早苗鹫发弹频率再降 50%（rate 6→12）、每发伤害 ×2（48→96）⇒ DPS 仍 = 40。
SANAE_EAGLE_DPS_TARGET  = 5 * 96.0 / 12.0 # = 40.0（5 行 rate12 各 96，hit0）
# 咲夜 EAGLE 是卡侧（`animal.c` 的 `ANIMAL_SAKUYA_EAGLE_*`），这里只留目标做对照。
# ★ AL65 #4：总 DPS 30 = 扫荡 20（6 点周期 6、dmg 20）+ 正中正面 10（dmg 20 / 周期 2）。
# ★ AL66（用户「咲夜大鹫得加强，整体 dps 上升 1/3」）：总 30 → 40。
# ★ AL67（用户「换成 dps 分布扫荡 20+正面 20」）：总 40 不变，火力更多挪到正面。
#   扫荡周期 4→6（6×20/6 = 20）；正面选方案 (b)：dmg 20→40、周期仍 2（40/2 = 20，弹流视觉不变）。
#   ⇒ 扫荡 20 + 正面 20 = **40**。
SAKUYA_EAGLE_DPS_TARGET = 40.0
SAKUYA_EAGLE_SWEEP_DPS  = 20.0
SAKUYA_EAGLE_FRONT_DPS  = 20.0
SAKUYA_EAGLE_FRONT_DMG  = 40

# —— 咲夜 WOLF 的汇聚点 y 偏移（**自机本地坐标，负 = 正前方/上**）——❓ 自定
# ★ AL65 #3（用户口径「贴板底恰好聚焦到板头」）：
#   一手 y 值（`engine/player/th18/01-position-and-state-timers.md` §2 = `Player__sub_45b170` `0x45B170`）：
#     · 自机 y 钳位 [0x1000, 0xd800] 定点（单位 1/128 px）⇒ [32.0, 432.0] px；
#       板底可达最低位置 = **432.0**（y 越大越靠屏幕下方），板头（弹幕区顶端）= **0.0**
#       （弹幕区 384×448、场地正中 (0,224)，`engine/anm/th18/01-vm-instantiate.md` §2、
#        `engine/anm/th18/02-render-stages.md` 相机 0 = `(32,16,384,448)`）。
#     · 汇聚点相对自机 = 板头 − 板底 = 0 − 432 = **−432.0**（正前方）。
#     · 含义 = 自机贴板底 y=432 时，汇聚点恰好落在板头 y=0（旧值 320 见 TUNING §2.1b 注）。
#   注：聚焦/低速只改移动速度与子机偏移（`player+0x476cc` / `sht+0x90` 块），**不改本体 y 钳位**；
#       本卡 WOLF 组 `opt_slot=0`（body）⇒ 两档的弹道基准都是自机本体 y，**无像素差**。
SAKUYA_WOLF_FOCUS_DY = -432.0
# —— 咲夜 WOLF 的 7 行（与灵梦一手逐行同 dmg / spawn / speed；只有角度被换成朝汇聚点）——❓ 角度自定
SAKUYA_WOLF_DMG   = (40, 30, 30, 25, 25, 20, 20)
SAKUYA_WOLF_SPAWN = ((0.0, 0.0), (20.0, 20.0), (-20.0, 20.0),
                     (40.0, 30.0), (-40.0, 30.0), (60.0, 30.0), (-60.0, 30.0))

# —— 早苗 EAGLE（大青蛙）：散射角度（相对正上 -π/2 的偏移弧度）——❓ 自定
#    取 ±0.6 rad ≈ ±34°，贴近零售早苗青蛙在满火力档的扇面（set0x04: -120°..-60°）。
SANAE_EAGLE_SPREAD = (-0.6, -0.3, 0.0, 0.3, 0.6)
# ★ 2026-09-13 AL59-2（用户实机 #4）：发弹频率 ×0.5（rate 3→6）、每发伤害 ×2（24→48）⇒
#   单发 DPS 不变（5×48/6 = 40），但节奏更慢。
#   ★★ 命中路径由 **hit4 改成 hit0**（默认 handler）：一手反汇编 `0x45f6f0` = 向 bullet VM 发
#   `interrupt(1)`（播 script29 的爆炸）+ `bullet+0x8c=2` + 关掉 bullet 自己的伤害源 + 返回 dmg；
#   **不新建伤害源**。原 hit4 `0x460cb0` 会 `Player__create_damage_source_45de40(pos, lifetime=20,
#   dmg, xmm2=24.0, xmm3=shooter+0x4c)`：DS+0x60/+0x64=20 帧寿命、DS+0x80=1（每帧结算）、
#   DS+0x7c=9999999（不因上限停用）⇒ **每次命中都会在命中点留一个 20 帧、每帧多判的爆炸源**
#   ⇒ 实际 DPS 远超名义值（用户实机「远远超过灵魔理」就是这个）。改用 hit0 后爆炸只播视觉、不补伤。
# ★ AL65 #2（用户「再把发弹频率降 50%，dps 不变」）：rate 6→12、dmg 48→96 ⇒ DPS 仍 = 5×96/12 = 40；
#   命中路径仍是 **hit0**（不建伤害源），爆炸仍是她自己的纯紫四帧（`PL03_SCRIPT_FROG` → script29）。
SANAE_EAGLE_DMG     = 96
SANAE_EAGLE_RATE    = 12

# —— 早苗 WOLF（大蛇）：5 行；出膛偏移 = 灵梦狼的对称 5 个，但 **y 取负（前方）** ——❓ 自定
#    灵梦一手 7 行 = (0,0)/(±20,20)/(±40,30)/(±60,30)（`engine/sht/th17/02` §2.1）。
#    ★ AL61：用户「早苗狼的发弹点太靠后了」→ 从灵梦的 +y（下方=子机位）改成身前（−y）；
#    ★ AL62：用户「再往前调一点」→ 纵向偏移再 ×1.4（−20→−28、−30→−42）。
#    纵向偏移是具名旋钮（负=前方）；改这两行继续微调，x 不动。
#    ★ AL65 #1（用户订正：「早苗狼对齐**灵梦大鹫**」）：直接取灵梦鹫的逐行 dmg/rate（20 / 3）
#      ⇒ DPS = 5×20/3 = 33.33；5 行、视觉 ×2、`tick4` 90° 折线机制不变。
SANAE_WOLF_FORWARD_1 = -28.0   # 内圈（x=±20）纵向偏移：负=前方
SANAE_WOLF_FORWARD_2 = -42.0   # 中圈（x=±40）纵向偏移：负=前方
SANAE_WOLF_DMG   = 20
SANAE_WOLF_RATE  = 3
SANAE_WOLF_SPAWN = ((0.0, 0.0),
                    (20.0, SANAE_WOLF_FORWARD_1), (-20.0, SANAE_WOLF_FORWARD_1),
                    (40.0, SANAE_WOLF_FORWARD_2), (-40.0, SANAE_WOLF_FORWARD_2))

# 追加脚本号：必须与 `assets/build_player_anm.py` 的计划一致。
# `assets/check_player_anm_bounds.py` 会独立回读 pl0X.anm 核对「sht 指到的脚本存在」。
# ★ AL61：pl02 追加 script30（红刀，alpha 255）/ script31（紫刀，alpha 255）；
#          pl03 追加 script29（大青蛙，紫染）/ script30（大蛇，红染）。
PL02_SCRIPT_WOLF  = 30
PL02_SCRIPT_EAGLE = 31
PL03_SCRIPT_FROG  = 29
PL03_SCRIPT_SNAKE = 30

# ★ AL62：咲夜刀弹视觉 ×2（`build_player_anm.py` 的 `KNIFE_SCALE` 注入脚本 `scale(2,2)`）。
#   命中框跟着放大（看得见的弹 = 打得到的地方）：
#   · 狼：视觉 64×16→128×32，判定取 (128,32)（≈ Retail Reimu 狼弹 112×32 的量级）。
#   · 鹫：视觉 64×16→128×32，判定保持「0.75×弹长」的零售比例 ⇒ (96,16)。
SAKUYA_KNIFE_SCALE  = 2.0
SAKUYA_WOLF_HIT_W   = 128.0
SAKUYA_WOLF_HIT_H   = 32.0
SAKUYA_EAGLE_HIT_W  = 96.0
SAKUYA_EAGLE_HIT_H  = 16.0


def _sakuya_wolf_rows():
    """仿灵梦 7 行；每行角度 = atan2(汇聚点 − 出膛点)。
    汇聚点 = 自机本地 `(0, SAKUYA_WOLF_FOCUS_DY)`，`FOCUS_DY` **负 = 正前方/上**
    （★ AL65 #3：默认 −432 ⇒ 自机贴板底 y=432 时汇聚点正好在板头 y=0）。
    视觉 = **咲夜自己的刀 ×2**：`anm=30` = pl02.anm 追加副本（红刀 sprite75；alpha 255、scale ×2）。"""
    import math
    rows = []
    for (dx, dy), dmg in zip(SAKUYA_WOLF_SPAWN, SAKUYA_WOLF_DMG):
        # 屏幕坐标 y 向下为正；角度 = atan2(Δy, Δx)，−π/2 = 正上。
        ang = math.atan2(SAKUYA_WOLF_FOCUS_DY - dy, -dx)
        rows.append(dict(fire_rate=3, start_delay=0, damage=dmg, spawn_dx=dx, spawn_dy=dy,
                         hit_w=SAKUYA_WOLF_HIT_W, hit_h=SAKUYA_WOLF_HIT_H, angle=ang,
                         speed=25.0, opt_slot=0, mode=0,
                         anm=PL02_SCRIPT_WOLF, sfx=(1 if not rows else -1),
                         func_on_init=0, func_on_tick=0, func_on_draw=0, func_on_hit=1,
                         tail=bytes(0x20)))
    return rows


def _sakuya_eagle_rows():
    """★ AL61：卡侧用 `Player__shoot_one_bullet`（`th18:0x45e930`）真发弹 —— 这两行就是那两颗刀弹。
    · `func_on_init=5`（`0x4612d0`）= 发射瞬间把 `bullet+0x64` 设成 `player+0x479cc`；
      卡侧每发前把该点 θ 写进 `player+0x479cc` ⇒ 每颗弹按自己的摆动方向飞。
    · `opt_slot=0 / mode=0` = body 弹：`PlayerBullet__create` 对 body（低 4 位=0）直接用传入的 pos。
    · `anm=31` = pl02.anm 追加的紫刀（alpha 255、scale ×2，AL62）。`fire_rate` 只满足 pack 约束。
    · ★ AL67：**row 0 = 扫荡（dmg 20）/ row 1 = 正中正面（dmg 40）**，其余字段逐字相同。"""
    def row(dmg):
        return dict(fire_rate=1, start_delay=0, damage=dmg,
                    spawn_dx=0.0, spawn_dy=0.0, hit_w=SAKUYA_EAGLE_HIT_W, hit_h=SAKUYA_EAGLE_HIT_H,
                    angle=-1.5707963705062866, speed=18.0, opt_slot=0, mode=0,
                    anm=PL02_SCRIPT_EAGLE, sfx=-1,
                    func_on_init=5, func_on_tick=0, func_on_draw=0, func_on_hit=0)
    return [row(20), row(SAKUYA_EAGLE_FRONT_DMG)]


def _sanae_eagle_rows():
    """大青蛙：5 发扇形散射；命中走 **hit0（默认 handler：interrupt(1) 播爆炸、不建伤害源）**。"""
    return [dict(fire_rate=SANAE_EAGLE_RATE, start_delay=0, damage=SANAE_EAGLE_DMG,
                 spawn_dx=0.0, spawn_dy=0.0, hit_w=48.0, hit_h=48.0,
                 angle=-1.5707963705062866 + off, speed=7.0, opt_slot=0, mode=0,
                 anm=PL03_SCRIPT_FROG, sfx=-1,
                 func_on_init=0, func_on_tick=0, func_on_draw=0, func_on_hit=0)
            for off in SANAE_EAGLE_SPREAD]


def _sanae_wolf_rows():
    """大蛇：5 行；func 4/4/0/5 = TH18 init4/tick4/hit5（tick4 = 锁敌 90° 折线）。
    出膛偏移 = 灵梦狼的对称 5 个（AL59-2 #5）。"""
    return [dict(fire_rate=SANAE_WOLF_RATE, start_delay=0, damage=SANAE_WOLF_DMG,
                 spawn_dx=dx, spawn_dy=dy, hit_w=16.0, hit_h=72.0,
                 angle=-1.5707963705062866, speed=8.0, opt_slot=0, mode=0,
                 anm=PL03_SCRIPT_SNAKE, sfx=-1,
                 func_on_init=4, func_on_tick=4, func_on_draw=0, func_on_hit=5)
            for dx, dy in SANAE_WOLF_SPAWN]


def _dps(rows):
    """每帧伤害 = Σ damage_i / rate_i（同一组内按行求和；一手 DPS 口径）。"""
    return sum(r["damage"] / r["fire_rate"] for r in rows)


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
# TH18 同字段直接读 `pl0X.anm script(anm)`。TH18 原生 `pl00.anm` / `pl01.anm` 里
# **已经逐字节含有**这几支 TH17 兽灵脚本（entry0 贴图/sprite 表也与 TH17 同名自机一致），
# 所以直接指过去即可：5→10 狼、6→11 鹫、8→13 狼、11→16 鹫、7→8 是 hit5 硬编码的束端。
# `build_player_anm.py` 现在只把原生 pl0X.anm 原样拷贝 + 硬校验，绝不再追加。
#
# pl02 / pl03 没有 TH17 对应自机，原版文件里没有兽灵脚本/贴图 ⇒ **安全回退**：
# identity 指回各自原生普通自机弹脚本（不会崩、不会出自机图），视觉目标暂未达成。
HYPER_ANM_MAP = {
    "pl00.sht": {5: 10, 6: 11, 7: 8, 8: 13, 11: 16},   # pl00.anm 原版兽灵脚本（TH17 逐字节同）
    # ★ 2026-09-11 九修（AUDIT §AL39）：pl01 WOLF 仍按 TH17 行直指原版脚本；
    #   EAGLE 改走 `HYPER_CUSTOM` 的空列表（主炮留空、卡侧矩形束），本 map 的 6/11
    #   两项仅保留给仍要翻译的模板，pl01 EAGLE 不再使用。
    "pl01.sht": {5: 10, 6: 11, 7: 8, 8: 13, 11: 16},
    # ★ 2026-09-13（AL59）：pl02/pl03 的 WOLF/EAGLE 已全部走 `HYPER_CUSTOM`（TH18 形状的行，
    #   直接 pack，不过本 map），下面两行只作为「如果哪天退回模板路径」的兜底，保留 identity。
    "pl02.sht": {5: 5, 6: 6, 7: 7, 8: 8, 11: 11},      # 咲夜：兜底 identity（当前未使用）
    "pl03.sht": {5: 5, 6: 6, 7: 7, 8: 8, 11: 11},      # 早苗：兜底 identity（当前未使用）
}

# ★ 2026-09-11 七修（AUDIT §AL37）：**已翻译成 TH18 形状**的覆盖组。
# 与下面的 `HYPER_FALLBACK` 是两套不同的东西：
#   · HYPER_CUSTOM 的行已经是 TH18 字段/函数索引/原生 pl0X.anm 脚本号，直接 pack，不再过
#     `_map_hyper_row`；
#   · HYPER_FALLBACK 仍按「原生 set 索引」逐字节拷整组，用于还没翻译好、或实跑未坐实的兽灵组。
HYPER_CUSTOM = {
    # ★ 2026-09-11 九修（AUDIT §AL39）：pl01 EAGLE 不再走主炮 shooter 行。
    # 用户实跑确认 TH17 `mode=2` 主炮路径会变成「带激光贴图的移动弹」，且
    # 「松开 Z 进入咆哮再按 Z 无事」与「按住 Z 进入咆哮」两条路都不稳。按用户
    # 已批准的 end state，EAGLE 的 main set 留空：由 `animal.c` 的卡侧矩形束负责
    # 视觉 + 伤害 + 消弹。0x23..0x27 仍是合法空组（与 WOLF_0 同形，只有终止符），
    # `on_shot_set_index` 返回这些索引时引擎不会创建 bullet。
    ("pl01.sht", "EAGLE"): [],
    # ★ 2026-09-13（AL59/AL61）：咲夜（pl02）—— WOLF = 仿灵梦 7 行 + 朝汇聚点，视觉 = 红刀副本；
    #   EAGLE = **真子弹**（卡侧 `Player__shoot_one_bullet` + 紫刀副本），主炮路径被
    #   `on_shot_set_index` 指到空的 EAGLE_0 ⇒ 这组只由卡侧显式发射，主炮不会自动打。
    ("pl02.sht", "WOLF"):  _sakuya_wolf_rows(),
    ("pl02.sht", "EAGLE"): _sakuya_eagle_rows(),
    # ★ 2026-09-13（AL59）：早苗（pl03）—— WOLF = 大蛇（`tick4` 90° 折线）；
    #   EAGLE = 大青蛙 + 命中爆炸（script29 的 `interruptLabel(1)`，经 **hit0** 默认 handler）。
    ("pl03.sht", "WOLF"):  _sanae_wolf_rows(),
    ("pl03.sht", "EAGLE"): _sanae_eagle_rows(),
}

# ★ 2026-09-11 七修（§AL37）：Task2 的反汇编把「主炮路径会拿 ability.anm」证伪后，
# pl01 不再需要任何整组原版回退。WOLF 回到 TH17 行 → `HYPER_ANM_MAP` 直指原版
# `pl00/pl01.anm` 里逐字节存在的 `script(anm+5)`；EAGLE 走 `HYPER_CUSTOM`。
# 保留空 dict 是为了让 `_hyper_sets` 的两条安全网（custom / fallback）都在代码里可查：
# 以后任何 group 只要实跑还没坐实，填一条 set 索引回退即可，不必改生成器。
HYPER_FALLBACK = {}

HYPER_SOURCE = {
    "pl00.sht": {"WOLF": "pl00a.sht", "EAGLE": "pl00a.sht"},
    "pl01.sht": {"WOLF": "pl01a.sht", "EAGLE": "pl01a.sht"},
    "pl02.sht": {"WOLF": "pl00a.sht", "EAGLE": "pl00a.sht"},
    # ★ 2026-09-12 十二修：pl03（早苗）没有 TH17 对应自机；EAGLE 若照 pl01a 模板会把 TH17 的
    #   `mode=2` 激光行原样搬进来 —— 但那些行的 `opt_slot` 是 TH17 的 0/16/32/48/64，TH18 主炮
    #   路径只取 **低半字节** ⇒ 全变成 0（body），`mode=2` 的 tick2 会去读 `bullet+0xe4 == 0` 的
    #   `option+0x5c`（`th18:0x45fcc8` 无空指针保护）⇒ **空指针崩溃**。pl02（咲夜）已经映射到
    #   pl00a 的 5 颗 body 追尾弹；早苗选择同一条安全回退（贴图 identity 到 pl03 自己的普通弹）。
    "pl03.sht": {"WOLF": "pl01a.sht", "EAGLE": "pl00a.sht"},
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
    # +0x26 = 激光命中特效的 anm 脚本号（只有 hit2 `0x460320` 读；原生魔理沙「迷你八卦炉」= 5）
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
    # ★ +0x4c：命中函数 hit 表 idx4（`th18:0x460cb0` = TH17 hit6 → TH18 hit4 的映射）会读它，
    #   作为 `Player__create_damage_source_45de40` 的浮点参数。原生早苗青蛙行 = **2.0**
    #   （一手回读 `pl03.sht` set 0x01 row2/3：`+0x4c = 0x40000000`）。放在 tail 之后写，
    #   保证显式字段赢过任何 tail 字节。
    struct.pack_into("<f", b, 0x4C, s.get("tilt", 0.0))
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
    """按文件相对偏移 off 拷一段原生 shooterset 原始字节（含终止符）。"""
    p = DATA_BASE + off
    while struct.unpack_from("<i", data, p)[0] != -1:
        p += STRIDE
        if p > len(data):
            raise SystemExit("原生 shooterset 没有终止符")
    return data[DATA_BASE + off:p + 4]


def _hyper_sets(target_file: str, anm_ids: dict) -> list:
    """按文件/家族生成 5 个追加组。三种来源，优先级：
       1. HYPER_CUSTOM —— 已经是 TH18 形状的行；pl01 EAGLE 现在登记为空列表（空组）；
       2. HYPER_FALLBACK —— 逐字节拷原生 shooterset（当前为空；留给实跑未坐实的组）；
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
            print("  " + target_file + f": {family} 安全回退 -> 原生行 " +
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


def repl_sets(anm_ids: dict) -> list:
    """REPLACE → [(槽号, 名字, blob)]，blob 含终止符。四个文件各自独立 rebuild，内容相同。"""
    out = []
    for nm, idx, rows in REPLACE:
        if not 0 <= idx < N_RETAIL:
            raise SystemExit(f"REPLACE {nm}: 槽 {idx:#x} 不是原生槽（只能覆盖 0x00–{N_RETAIL - 1:#x}）")
        if any(o == idx for o, _, _ in out):
            raise SystemExit(f"REPLACE {nm}: 槽 {idx:#x} 被两条登记同时覆盖")
        blob = b"".join(pack_shooter(r, anm_ids) for r in rows) + TERMINATOR
        out.append((idx, nm, blob))
    return out


def check_card_constants(repl: list):
    """★ 防漂移：卡片头里的常量必须和这里分配的槽/行数一致。

    `cards/youkai_core.c` 的 `yk_bullet_is_youkai()` 靠 `(packed_id >> 8) == YK_SHOT_SET_YOUKAI`
    认弹、`cards/youkai.c` 靠 `YK_SHOT_SET_YOUKAI` 取 `sht + 0xe0 + idx*4` 那一行 ——
    任何一边改了数字而另一边没改，都是**静默失效**（弹打出来了但伤害表不认 / 取到别的组）。
    所以这里在生成时就把两边对上。"""
    if not repl or not YOUKAI_CORE_H.is_file():
        return
    txt = YOUKAI_CORE_H.read_text(encoding="utf-8")
    for idx, nm, blob in repl:
        want_rows = (len(blob) - len(TERMINATOR)) // STRIDE
        for macro, want in ((f"YK_SHOT_SET_{nm}", idx), (f"YK_SHOT_SET_{nm}_ROWS", want_rows)):
            m = re.search(rf"^#define\s+{macro}\s+(\S+)", txt, re.M)
            if not m:
                raise SystemExit(f"{YOUKAI_CORE_H.name}: 缺 `#define {macro}`"
                                 f"（REPLACE 里登记了 {nm} 槽 {idx:#x} / {want_rows} 行）")
            got = int(m.group(1), 0)
            if got != want:
                raise SystemExit(f"{YOUKAI_CORE_H.name}: {macro} = {got:#x}（或 {got}），"
                                 f"但 shooterset 侧是 {want:#x}（或 {want}）—— 两边必须一致")
    print(f"  {YOUKAI_CORE_H.name}: 卡片常量与 REPLACE 一致 " +
          ", ".join(f"{nm}=slot {idx:#x}/{(len(b) - 4) // STRIDE} 行" for idx, nm, b in repl))


def read_anm_ids() -> dict:
    if not ANM_IDS_H.is_file():
        raise SystemExit(f"缺 {ANM_IDS_H}：先跑 make anm")
    out = {}
    for line in ANM_IDS_H.read_text(encoding="utf-8").splitlines():
        parts = line.split()
        if len(parts) >= 3 and parts[0] == "#define" and parts[2].rstrip("/*").strip().isdigit():
            out[parts[1]] = int(parts[2])
    return out


def check_pl_anm_ids():
    """★ 防漂移：本文件里硬编码的 pl0X 追加脚本号必须与 build_player_anm.py 生成的头一致。

    `pl_anm_ids.h` 由 `assets/build_player_anm.py` 写（`make anm`）。**软检查**：
    头不存在（例如只跑 `make sht` 还没跑 `make anm`）时只提示、不拦；存在就必须对上，
    否则 sht 会指到别的脚本 = 静默画错弹。`check_player_anm_bounds.py` 是第二道独立闸。
    """
    want = {"CE_PL_ANM_SCRIPT_PL02_WOLF": PL02_SCRIPT_WOLF,
            "CE_PL_ANM_SCRIPT_PL02_EAGLE": PL02_SCRIPT_EAGLE,
            "CE_PL_ANM_SCRIPT_PL03_FROG": PL03_SCRIPT_FROG,
            "CE_PL_ANM_SCRIPT_PL03_SNAKE": PL03_SCRIPT_SNAKE}
    if not PL_IDS_H.is_file():
        print(f"  {PL_IDS_H.name}: 不存在（还没跑 make anm）—— 跳过 pl0X 脚本号交叉核对")
        return
    txt = PL_IDS_H.read_text(encoding="utf-8")
    for macro, v in want.items():
        m = re.search(rf"^#define\s+{macro}\s+(\S+)", txt, re.M)
        if not m:
            raise SystemExit(f"{PL_IDS_H.name}: 缺 {macro}（append_shooterset.py 需要它）")
        got = int(m.group(1), 0)
        if got != v:
            raise SystemExit(f"{PL_IDS_H.name}: {macro} = {got}，但 append_shooterset.py 写死 {v}"
                             f" —— pl0X.sht 会指到错的脚本，先让两边一致")
    print(f"  {PL_IDS_H.name}: pl0X 追加脚本号与 append_shooterset.py 一致")


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


def build_one(name: str, retail: bytes, sets: list, repl: list) -> bytes:
    offs = check_retail_shape(name, retail)
    used = sum(1 for i, o in enumerate(offs) if i == 0 or o != 0)
    if used != N_RETAIL:
        raise SystemExit(f"{name}: 原生用了 {used} 项，期望 {N_RETAIL}")
    if N_RETAIL + len(sets) > N_SLOTS:
        raise SystemExit(f"偏移数组只有 {N_SLOTS} 项，原生占 {N_RETAIL}，放不下 {len(sets)} 组")

    out = bytearray(retail)
    for k, (_, blob) in enumerate(sets):
        idx = N_RETAIL + k
        struct.pack_into("<I", out, OFF_ARRAY + idx * 4, len(out) - DATA_BASE)
        out += blob
    # REPLACE：新记录接在**文件尾**，把该槽的偏移指过去（原生原记录的字节原地不动）
    for idx, nm, blob in repl:
        if offs[idx] == 0:
            raise SystemExit(f"{name}: REPLACE {nm} 指向空槽 {idx:#x}，没有原生记录可覆盖")
        struct.pack_into("<I", out, OFF_ARRAY + idx * 4, len(out) - DATA_BASE)
        out += blob
    return bytes(out)


def verify(name: str, retail: bytes, built: bytes, sets: list, repl: list):
    repl_by_idx = {idx: (nm, blob) for idx, nm, blob in repl}
    offs_r, offs_b = offsets(retail), offsets(built)
    check_retail_shape(name, built)
    retail_data_end = len(retail) - DATA_BASE
    for i in range(N_RETAIL):
        n = walk_set(retail, offs_r[i])
        a, b = DATA_BASE + offs_r[i], DATA_BASE + offs_b[i]
        if i in repl_by_idx:
            # 被覆盖的槽：偏移被改指追加区，但**原生原记录必须原地原样**（死数据，不动）
            if retail[a:a + n] != built[a:a + n]:
                raise SystemExit(f"{name}: 覆盖槽 {i:#x} 把原生原记录的字节改了")
            continue
        if offs_b[i] != offs_r[i]:
            raise SystemExit(f"{name}: 第 {i:#x} 项偏移被动过")
        if retail[a:a + n] != built[b:b + n]:
            raise SystemExit(f"{name}: shooterset {i:#x} 的字节变了")
    for idx, (nm, blob) in repl_by_idx.items():
        o = offs_b[idx]
        if o == 0:
            raise SystemExit(f"{name}: 覆盖槽 {idx:#x} 没写偏移")
        if o < retail_data_end:
            raise SystemExit(f"{name}: 覆盖槽 {idx:#x}（{nm}）的新记录没落在追加区"
                             f"（off={o:#x} < 原生数据末尾 {retail_data_end:#x}）"
                             f"—— build/sht/*.sht 还是旧的：跑 `make sht` 重建")
        if built[DATA_BASE + o:DATA_BASE + o + len(blob)] != blob:
            raise SystemExit(f"{name}: 覆盖槽 {idx:#x}（{nm}）的数据对不上")
        if walk_set(built, o) != len(blob):
            raise SystemExit(f"{name}: 覆盖槽 {idx:#x}（{nm}）的终止符位置不对")
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


def write_ids_h(sets: list, repl: list):
    lines = ["/* 由 assets/sht/append_shooterset.py 生成 —— 追加进 pl0X.sht 的 shooterset 索引。别手改。 */",
             "#pragma once", ""]
    for k, (nm, blob) in enumerate(sets):
        lines.append(f"#define CE_SHT_SET_{nm:<24} {N_RETAIL + k:#04x}   "
                     f"/* 追加，{len(blob) // STRIDE} shooter */")
    for idx, nm, blob in repl:
        lines.append(f"#define CE_SHT_SET_{nm:<24} {idx:#04x}   "
                     f"/* ★ 覆盖原生空槽，{len(blob) // STRIDE} shooter */")
    IDS_H.write_text("\n".join(lines) + "\n", encoding="utf-8")


def main():
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    ap.add_argument("--verify-only", action="store_true")
    a = ap.parse_args()

    anm_ids = read_anm_ids()
    sets_by_file = {name: sets_for_file(name, anm_ids) for name in FILES}
    canonical = sets_by_file[FILES[0]]
    repl = repl_sets(anm_ids)
    check_card_constants(repl)
    check_pl_anm_ids()

    if not canonical and not repl:                  # 没有要追加的：不产出（别发一份和原生一模一样的 .sht）
        if not a.verify_only:
            for name in FILES:
                (OUT / name).unlink(missing_ok=True)
            write_ids_h(canonical, repl)
        print("  没有登记任何 shooterset：不产出 .sht（build/sht/ 已清空）")
        return 0

    OUT.mkdir(parents=True, exist_ok=True)
    for name in FILES:
        src = RETAIL / name
        if not src.is_file():
            raise SystemExit(f"缺原生样本 {src}（用户放进 local/，见 local/README.md）")
        retail = src.read_bytes()
        sets = sets_by_file[name]
        dst = OUT / name
        if a.verify_only:
            if not dst.is_file():
                raise SystemExit(f"缺 {dst}：先跑 make sht")
            built = dst.read_bytes()
        else:
            built = build_one(name, retail, sets, repl)
            dst.write_bytes(built)
        verify(name, retail, built, sets, repl)
        print(f"  {name}: {len(retail)} → {len(built)} 字节，追加 {len(sets)} 组"
              f"（索引 {N_RETAIL:#04x}..{N_RETAIL + len(sets) - 1:#04x}）"
              f"，覆盖 {len(repl)} 个原生槽")

    if not a.verify_only:
        write_ids_h(canonical, repl)
        print(f"wrote {IDS_H.relative_to(MOD)}")
    for k, (nm, blob) in enumerate(canonical):
        print(f"  CE_SHT_SET_{nm} = {N_RETAIL + k:#04x}  ({len(blob) // STRIDE} shooter)")
    for idx, nm, blob in repl:
        print(f"  CE_SHT_SET_{nm} = {idx:#04x}  (★ 覆盖原生空槽，{len(blob) // STRIDE} shooter)")
    print_dps_table()
    return 0


def print_dps_table():
    """★ 2026-09-13（AL59）：DPS 对照。每帧伤害 = Σ damage_i / fire_rate_i（一手口径）。

    参考值全部来自 `engine/sht/th17/02-hyper-shot-groups.md` §2.1–2.6（TH17 一手行）：
      灵梦 WOLF  7 行 rate3  40/30/30/25/25/20/20  ⇒ 190/3  = 63.33
      魔理沙 WOLF 5 行 rate8  18/15/15/8/8          ⇒  64/8  =  8.00
      妖梦 WOLF  8 行 rate100 80/80/70×5            ⇒ 580/100=  5.80
      灵梦 EAGLE 5 行 rate3  ×20                    ⇒ 100/3  = 33.33
      魔理沙 EAGLE 5 行 rate15 80/60/60/50/50       ⇒ 300/15 = 20.00
      妖梦 EAGLE 15 行 rate3 ×8                     ⇒ 120/3  = 40.00
    """
    refs = [("灵梦 WOLF", 190.0 / 3.0), ("魔理沙 WOLF", 64.0 / 8.0), ("妖梦 WOLF", 580.0 / 100.0),
            ("灵梦 EAGLE", 100.0 / 3.0), ("魔理沙 EAGLE", 300.0 / 15.0), ("妖梦 EAGLE", 120.0 / 3.0)]
    new = [("咲夜 WOLF (新)", _dps(_sakuya_wolf_rows()), "仿灵梦·红刀·汇聚贴板底", SAKUYA_WOLF_DPS_TARGET),
           ("早苗 WOLF (新)", _dps(_sanae_wolf_rows()), "大蛇·红（★AL65 对齐灵梦鹫）", SANAE_WOLF_DPS_TARGET),
           ("咲夜 EAGLE 扫荡", SAKUYA_EAGLE_SWEEP_DPS, "6 点 rate6 dmg20（卡侧真弹·紫刀）", SAKUYA_EAGLE_SWEEP_DPS),
           ("咲夜 EAGLE 正面", SAKUYA_EAGLE_FRONT_DPS, "正中 1 点 rate2 dmg40（θ=0 正前）", SAKUYA_EAGLE_FRONT_DPS),
           ("咲夜 EAGLE 合计", SAKUYA_EAGLE_DPS_TARGET, "扫荡 20 + 正面 20（★AL67 总 40 不变）", SAKUYA_EAGLE_DPS_TARGET),
           ("早苗 EAGLE(新)", _dps(_sanae_eagle_rows()), "大青蛙·紫（★AL65 rate12 dmg96）", SANAE_EAGLE_DPS_TARGET)]
    print("  DPS 对照（每帧伤害；新卡参数在 append_shooterset.py / animal.c）:")
    for nm, v in refs:
        print(f"    {nm:<18} {v:7.2f}  (一手)")
    for nm, v, note, tgt in new:
        print(f"    {nm:<18} {v:7.2f}  ({note}；目标 {tgt:.2f}，偏差 {(v - tgt) / tgt * 100:+.1f}%)")
    print("    注（AL64c）：早苗 EAGLE 的命中路径 = hit0（默认 handler）⇒ 上表 40 是**真实**DPS；")
    print("      旧 hit4 会在命中点建一个 20 帧、每帧结算的爆炸源（+最多 24×20/发）⇒ 实际远超名义值。")
    print("    注（AL65）：早苗 WOLF 对齐**灵梦大鹫** 100/3=33.33（不是灵梦狼 63.33）；")
    print("      咲夜 EAGLE 总 30 拆成扫荡 20 + 正中正面 10。")
    print("    注（AL66）：咲夜 EAGLE 总 30→40（+1/3）——正面 10 不动，扫荡周期 6→4（20→30）。")
    print("    注（AL67）：咲夜 EAGLE 分布改成扫荡 20（周期 6、dmg20）+ 正面 20（dmg40 / 周期 2），总 40 不变。")


if __name__ == "__main__":
    sys.exit(main())
