#!/usr/bin/env python3
"""往原生的四个 `pl0X.sht` 追加「新卡子机」的 shooterset，输出到 native/build/sht/，并生成 native/sht_ids.h。

    python3 append_shooterset.py                # 构建 + 自检 + 写 sht_ids.h
    python3 append_shooterset.py --verify-only  # 只自检已有的 build/sht/*.sht

## 为什么会重排文件（★ 2026-09-14 扩容到 96 格，`AUDIT.md` §CQ）

零售文件的原样是：头 `[0,0xe0)` + **40 项**偏移数组 `[0xe0,0x180)` + 数据区 `0x180` 起
（`0x180` 是解析器里硬编码的重定位基址 `th18:0x45a830` `lea eax,[edx+0x180]`）。
原生只用了 23 项（`0x00`–`0x09` 主炮、`0x0a`–`0x16` 装备卡子机），尾部 17 项是 0 —— 本 mod 到
2026-09-13 把 `0x17`–`0x27` 也占满（豌豆 2 + 116 季节 5 + 117 兽灵 10）⇒ **40/40，余量 0**。

扩容的写法是「**数组区原地长到预留格数、数据区跟着后移**」：

    [0,0xe0)     头（不动）
    [0xe0,0x4e0) 数组区 = **预留 256 格**（宣布可用 96 格 = 头 `+0x02`；原 40 项的值一个不改）
    [0x4e0,…)    数据区（每一条记录的字节原样，只是整体后移 0x360）

★ **为什么预留到 256 格**：数据区起点是 exe 里的硬编码立即数（`0x45a830`）⇒ 必须一次让够。
预留之后**以后再加格子只需改文件**（抬 `N_SLOTS` + 写槽位 + 接行数据），**exe 补丁与 DLL 常量都不用动**。
⚠ 用到 `N_SLOTS`（= 头 `+0x02`）之外的预留槽时，**必须同时把 `N_SLOTS` 抬到那个槽之上**：
计数外的槽不会被重定位（值仍是 0），谁读它谁拿到空指针。

- **数据区的偏移项是相对的**（`stored = 文件内偏移 − 数据区起点`），记录与基址**同时后移**
  ⇒ 40 个零售偏移项的值**一个字节都不用改**，记录的字节也一个都不用改。
- exe 侧只有**一处**要改：`th18:0x45a830` 的 `lea eax,[edx+0x180]` → `[edx+0x4e0]`
  （6 字节等长 binhack，见 `native/sites.py` 的 `SHT_DATA_BASE`）。
- ★ **13 个读者的位移 `[sht + 0xe0 + set*4]` 一个字节都不用动** —— 数组起点还是 `0xe0`。
  这是本次扩容与 `NEXT.md` §1w 最早那版计划（把数组搬文件尾、改 13 个读者）的**关键差别**：
  搬数组要改 14 处、漏一处就是静默错位（`CAPACITY.md` §4 的普查就是为那条路做的）；
  原地长大只需证明「数据区基址常量在全 exe 只出现一次」（已证：`.text` 里 `0x180` 的
  memory disp 只有 `0x45a830` 一处与 pl0X 有关，见 `AUDIT.md` §CQ 的取证）。
- 结构恒等式（`assert` 在模块级）：`DATA_BASE == OFF_ARRAY + N_SLOTS_RESERVED*4`
  —— 它同时保证了「第 `i` 项在 `0xe0 + i*4 < DATA_BASE`（对任何 `i < N_SLOTS_RESERVED`）」
  ⇒ 读者永远读在数组区里、踩不到数据区。
- 文件变大是**安全**的：影像内存是 `malloc(GetFileSize())`（`th18:0x491a34` ← `0x4ad088`
  GetFileSize ← `0x402140` 一带），缓冲区随文件大小；不是定长结构体内嵌数组。

一手：`engine/sht/th18/01-file-layout-and-shooterset-index.md`。

## 三条路：**追加**新索引（`APPEND`）/ **覆盖**原生空槽（`REPLACE`）/ **孤儿段**（`TRANSIENT`）

- 扩容后可用的是 `0x28`–`0x5f`（**56 格**，见 `sht_ids.h` 的分配表）；`0x00`–`0x27` 已满。
  再要一格只能 **复用原生里没有卡映射的那一格**（`REPLACE`，现在只有 108 的妖侧 set 用 0x16）。
  覆盖的写法是「新记录接在文件尾 + 把该槽的偏移指过去」，原生原来的 96 字节原地留着（死数据）。
- `APPEND` 分配 `0x17` 起；`REPLACE` 不新增索引、也**不挪动任何既有索引**。
- ★ 2026-09-13（卡 113 的灵界组）：**原生 0x0a–0x15 十二格全是活的原生装备卡子机组、0x16 已被 108 占**
  ⇒ 「抢一格」会永久毁掉某张原生卡、「加一格」要改解析器的硬编码基址 ⇒ 走第三条路：
  **行数据落在文件尾的孤儿段（没有任何偏移项指向它），运行时由 `cards/trance.c` 在灵界窗口内
  把借用格 `CE_SHT_SET_TRANCE`（0x0e）临时指向它，退出时还原原生指针**。
  普查与理由的全文在下面 `TRANSIENT_SLOT` 段的注释里（唯一出处）。

## 不变式（脚本每次都回读校验）

1. 产物头部 `+0x02` = **`N_SLOTS`（96）**（= 解析循环的上界）；**零售文件**仍是 40，两边分别核。
   数组区本身是 `N_SLOTS_RESERVED`（256）格。
2. **shooterset 0 的四个 func 字段仍全是 0**。剩下的空位仍指向 set 0，会被重复解析；
   靠 `0 → 表[0] = NULL → 0` 幂等才不炸。改坏它 = 开局崩。
   （扩容后计数内的空位多了 ⇒ set 0 被重复解析的次数从 17 涨到 73，更依赖这条。）
   ⚠ 计数**外**的预留槽不会被重定位（值仍是 0）：谁要用它，必须同时把文件头 `+0x02` 抬上去。
3. **除 `REPLACE` 点名的槽外**，前 23 组 shooterset 的字节逐字节不变，偏移项的值也不变
   （记录整体后移 0x360，偏移是相对的 ⇒ 值不变；比较时用 `DATA_BASE_RETAIL` / `DATA_BASE` 两侧基址）。
4. 被 `REPLACE` 覆盖的槽：原生原记录**原地原样**、新记录落在**追加区**（文件尾），
   且除该槽的偏移项外偏移数组一个字节不动。
5. **孤儿段（`TRANSIENT`）四条**：① 四个文件的孤儿段起点**同一个偏移**（短文件补 0 对齐）；
   ② **没有任何偏移项**（预留 256 个全查）指进孤儿段 —— 否则别的卡会拿到灵界行；
   ③ 孤儿段之前的补零段确实全是 0；④ 每文件的行数与登记的 `TRANCE_ROWS` 一致
   （咲夜 = 空组，只有终止符）。
6. **数组区与数据区不重叠**（★ 扩容新增）：`OFF_ARRAY + N_SLOTS_RESERVED*4 == DATA_BASE` —— 这是
   「读者位移 `0xe0` 不用改」的充要条件；数组区少算一格 ⇒ 数据区头 4 字节被当成数组最后一格，
   多算一格 ⇒ 数据区第 1 项被压在数组下面。两种都是静默错位。
7. **没分配的槽 + 全部预留槽（`N_RETAIL+len(sets)` 到 `N_SLOTS_RESERVED-1`）必须是 0**
   （脚本按分配表自动核）。计数内的空槽重定位后别名 set 0（幂等），计数外的保持 0。
8. **数据区起点与数组区长大严格同步**：
   `DATA_BASE == DATA_BASE_RETAIL + (N_SLOTS_RESERVED - N_SLOTS_RETAIL)*4`，
   且 exe 侧那一处 binhack 写的就是 `DATA_BASE`（`sites.py` ↔ 本文件 ↔ `capcheck.py` 三方对账）。

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

OFF_ARRAY = 0xE0        # 偏移数组在影像内的起点（**14 个读者指令里的位移就是这个值，扩容不动它**）
N_SLOTS_RETAIL = 40     # 零售文件的格数（= 零售头 `+0x02`）
# ── 两个「格数」：**宣布可用的**（= 影像头 `+0x02` = 加载器解析循环上界）与**预留的数组区**──
N_SLOTS = 96            # ★ 宣布可用：用户 2026-09-13 拍板的目标（本 mod 现在用到 0x5f）
N_SLOTS_RESERVED = 256  # ★ 数组区一次留够：以后用到 0x60+ 只需**改文件**（抬 N_SLOTS + 写槽位
                        #   + 接行数据），exe 补丁与 DLL 常量都不用动（数据区起点已让开）
DATA_BASE_RETAIL = 0x180  # 零售的数据区起点（= `th18:0x45a830` 的原始立即数）
# ★ 数据区起点必须**正好接在预留数组区的末尾**：[0xe0, 0xe0+N_SLOTS_RESERVED*4)，
#   于是「第 i 项恒在 0xe0+i*4 < DATA_BASE」对**任何** i < N_SLOTS_RESERVED 都成立
#   ⇒ 读者（位移 0xe0）永远踩不到数据区。两条等价写法给出同一个数（0x4e0），互为对账：
DATA_BASE = OFF_ARRAY + N_SLOTS_RESERVED * 4              # = 0x4e0
assert DATA_BASE == DATA_BASE_RETAIL + (N_SLOTS_RESERVED - N_SLOTS_RETAIL) * 4, \
    "数据区起点必须 = 零售基址 + 数组区预留字节数（否则会与数组重叠 / 留缝）"
assert N_SLOTS_RETAIL <= N_SLOTS <= N_SLOTS_RESERVED, "宣布可用格数必须落在 [零售格数, 预留格数] 内"
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
                            # ★★ 2026-09-16m：`+0x22` 从 116 的「季节点」弹（`SEASON_BULLET_DOYOU`）
                     #   换成**魔理沙自己的弹**（脚本 282 = TH18 原生 `pl01.anm` script5 的移植，
                     #   贴图 = `pl01.anm` entry0 sprite24/25/26/27 的 1:1 切图）。
                     #   一手：装备卡子机的弹贴图来自 `ability.anm`（`th18:0x40aa03` 换 `player+0x10`）
                     #   ⇒ 号段必须是 ability.anm 的；用户实跑「**弹的贴图**」不对 = 这条指错了。
                     #   生成器 `assets/ability/make_marisa_bullet_art.py`（entry 55 / sprite 436..439）。
                     anm="CE_ANM_ABILITY_SCRIPT_MARISA_BULLET", sfx=-1, func_on_init=0)]),  # 向后 24/帧

]


# ---- 覆盖原生空槽：妖侧 shooterset（卡 108「永夜抄」）-----------------------------
#
# ## 为什么是「覆盖 0x16」而不是「追加一个新索引」
#
# 一手（本仓字节扫描 + 逐文件解析，2026-09-12）：
#   · 偏移数组 40 格**全满**：四份原生 `pl0X.sht` 的 `+0x02` 都是 40，`0x00`–`0x16` 逐项非 0；
#     本 mod 的 `APPEND` 又占掉 `0x17`–`0x27`（见 `native/sht_ids.h`）⇒ `build_one()` 的
#     `N_RETAIL + len(sets) > N_SLOTS` 会直接拦住「再加一格」。
#     ★ 2026-09-14 起 `N_SLOTS` = 96（AUDIT §CQ 扩容）⇒ 又空出 `0x28`–`0x5f`，本覆盖写法**继续保留**
#       （108 已经发出去、槽号写进存档与卡定义，不动它），但**新卡请从 `0x28` 起拿号**。
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
def _yk_row(rate, damage, dx, angle, sfx, speed=12.0, dy=0.0, opt_slot=0):
    """一行 = 一个 TH18 shooter 记录 dict（字段名见 pack_shooter）。

    ★ 2026-09-22：加了 `speed` / `dy` 两个可选参数 —— 原版 TH08 的 `plyNNas.sht` **每行有自己的
    弹速**（`+0x18`：紫 12/10、アリス 24/10、レミリア 20/12），第一版把它们全写死成 12.0
    只是「当时那一档恰好全是 12」的巧合。`dy` 同理（本批 4 个文件的所有行都是 0.0，
    参数留着是为了「照抄」而不是「假设」）。既有 `REPLACE` 的调用点不传 ⇒ 行为逐字节不变。"""
    return dict(fire_rate=rate, start_delay=0, damage=damage,
                spawn_dx=dx, spawn_dy=dy, hit_w=16.0, hit_h=16.0,
                angle=angle, speed=speed, opt_slot=opt_slot, mode=0,
                anm=21, sfx=sfx, hit_fx=0, fire_rate2=0, start_delay2=0,
                func_on_init=0, func_on_tick=0, func_on_draw=0, func_on_hit=0)


# (名字, 槽号, [shooter, …]) —— 名字会生成 `#define CE_SHT_SET_<名字>`，
# 同时被 `check_card_constants()` 拿去核对 `cards/youkai_core.h` 里的 `YK_SHOT_SET_<名字>`。
REPLACE = [
    ("YOUKAI", 0x16, [
        # ply03as.sht tier5(thr=128) 的 9 行，逐行照抄（TH08 anm 值写在行末注释里）
        # ⚠ 行末的 `opt=` = TH08 `rec[+0x20]` 的原点（一手见上面 YOUKAI_TIER_ORIGINS 那一段的解析法）
        _yk_row(rate=10, damage=24, dx=-12.0, angle=-1.780236, sfx=-1, opt_slot=0),   # row0 anm4 外侧左
        _yk_row(rate=5,  damage=26, dx=-6.0,  angle=-1.675516, sfx=-1, opt_slot=0),   # row1 anm2
        _yk_row(rate=3,  damage=30, dx=0.0,   angle=-1.570796, sfx=0,  opt_slot=0),   # row2 anm2 中央（出膛音）
        _yk_row(rate=5,  damage=26, dx=6.0,   angle=-1.466077, sfx=-1, opt_slot=0),   # row3 anm2
        _yk_row(rate=10, damage=24, dx=12.0,  angle=-1.361357, sfx=-1, opt_slot=0),   # row4 anm4 外侧右
        _yk_row(rate=6,  damage=22, dx=0.0,   angle=-1.850049, sfx=-1, opt_slot=1),   # row5 anm6 子机1 最外左
        _yk_row(rate=5,  damage=24, dx=0.0,   angle=-1.710423, sfx=-1, opt_slot=1),   # row6 anm4 子机1
        _yk_row(rate=5,  damage=24, dx=0.0,   angle=-1.431170, sfx=-1, opt_slot=2),   # row7 anm4 子机2
        _yk_row(rate=6,  damage=22, dx=0.0,   angle=-1.291544, sfx=-1, opt_slot=2),   # row8 anm6 子机2 最外右
    ]),
]


# ============================================================================
# ★★ 2026-09-22（P4 · th08 #5「妖侧子机发射的弹幕都相同」）：**逐角色 × 逐火力档**的妖侧
#    shooterset，照抄 TH08 的 `plyNNas.sht`。
#
# ## 一手（怎么得到这张表的）
#
# 1. **TH08 的 `.sht` 是「组 = 火力档」**，一手落点（全在 `th08.exe`）：
#    `th08:0x450f60`（唯一生成器，`Player::UpdateChargeShotTimer` `th08:0x451595` 每帧调）
#    起手 `th08:0x450f6f movzx ecx,BYTE PTR [eax+0x3]` = **`player+0x3` = `isYoukaiMode`**
#    ⇒ 0 取 `player+0xe2a74`（= `plyNNa.sht`，人侧）、非 0 取 `player+0xe2a78`（= `plyNNas.sht`，**妖侧**）；
#    `th08:0x450f80/0x450f91 add eax,0x38` = 跳过 0x38 的头，落在**槽数组**首项；
#    档位循环在 `th08:0x451005`–`0x451020`：
#        ```
#        451005  mov ecx,0x160f508 ; GameManager
#        45100a  call 0x422480     ; = GameManager::get_current_power
#        451012  cmp eax,[ecx+0x4] ; ★ 槽的第 2 个 dword = 该档的**火力阈值**
#        451015  jl  0x451022      ; 未达阈值 ⇒ 停在这一档
#        45101a  add edx,0x8       ; 达阈值 ⇒ 换下一档（槽 8 字节一项）
#        ```
#    ⇒ **槽号 = 火力档**，阈值存在槽的 `+0x4`：实测三个文件都是 `8 / 24 / 48 / 80 / 128 / 999`。
#    （`999` 那一档在 TH08 里**够不到** —— 本作 P 上限 128 ⇒ slot 5 + 之后是死亡炸弹/别的用途，
#      本批**不用**。）
# 2. **行的字段**（`+0x38` 一项）：本脚本读的是 `+0x00 rate / +0x04 dx / +0x08 dy / +0x14 angle /
#    +0x18 speed / +0x1c damage / +0x26 sfx`（字段图与读点见
#    `engine/player/th08/01-youkai-familiars-first-hand.md` §4.2，**代码坐实**那一版）。
#    本批是**离线一次性提取**后把数字抄进来的（照 `HYPER_TEMPLATE_*` 的先例：产物进仓、
#    `local/` 的样本不进仓 ⇒ 别人 clone 下来也能 `make sht`）。
#    提取命令与逐行读数见 `engine/sht/th08/01-youkai-shootersets.md`。
# 3. **映射到 TH18 的火力档**：两作的 P 显示单位都是 x.xx、上限 4.00
#    （TH08 `P/32`、TH18 `P/100`）⇒ TH18 的档 `k`（P = 100k）折算成 TH08 的 P = 32k，
#    查上面的阈值 ⇒ **正好落在 slot k**（0→slot0 <8、100→32≥24 slot1、200→64≥48 slot2、
#    300→96≥80 slot3、400→128≥128 slot4）⇒ **`TH18 档 k ↔ TH08 slot k`，一一对应**。
#    ⇒ 本表的下标 = TH18 的 `CURRENT_POWER / 100`（= 卡侧的 `yk_shot_tier()`）。
#
# ## 与既有 `REPLACE(YOUKAI, 0x16)` 的关系
#
# `0x16` 那一格**保留**（pl03 = 早苗 的回落档：她的妖侧是 TH16 的射命丸文，
# 本批**没有** TH16 的一手行数据 ⇒ 见报告的 🔴 待办）。三个 TH08 角色走下面的新槽。
# 生成序列里**必须排最后**（= `N_RETAIL + 序号`，见 `sets_for_file` 的注）。
#
# ## 已知偏差（❓ 记账，不改数字硬凑）
#
# - **膛口**：✅ **2026-09-22（P7）已照抄** —— 每行的 `opt_slot` 现在 = TH08 `rec[+0x20]`
#   （见上面 `YOUKAI_TIER_ORIGINS`：0 = 自机本体、n = 第 n 架子机）。**前提**是这些组由**引擎的行循环**
#   发（卡侧认领 set 之后就是这样）；走**卡路径**（`Player__tick_shooters_for_ability_card`）时
#   `+0x20` 会被忽略（膛口恒取传进来的那颗子机）⇒ 认领 + 不压行循环是一对，缺一不可。
#   行数 / 角度 / 速度 / 伤害 / 节奏 / 原点全部照抄。
# - **贴图**：`+0x22` 填 21（= 原生 `ability.anm` 的合法弹脚本，与既有 `YOUKAI` 那一格同源）；
#   TH08 用的是它自己 `pl0X.anm` 的 2/4/6/12/14…，数字空间不同（`0x40aa03` 一手：
#   卡路径把 `player+0x10` 换成 `ability.anm`）⇒ 要把 TH08 的弹贴图切进 `ability.anm` 才谈得上，见报告 ❓。
# - **`+0x0c/+0x10`（判定盒）**：TH08 有数据（18×48 / 24×16）但全 exe 只有写点、没有读点
#   ⇒ 照原生同形组填 `(16, 16)`（与既有 `YOUKAI` 那一格同口径）。
# ## ★★★ 2026-09-22（P7 / owner #13「妖形态主机也发弹」）：**每行的 `+0x20`（生成原点）照抄** ----
#
# 一手：TH08 `rec[+0x20]`（i16）= **生成原点**：`0` = 自机本体、`n>=1` = `options[n-1]`
#   （`engine/sht/th08/01-youkai-shootersets.md` §3，读点 `th08:0x44fb7f`–`0x44fbd7`）。
# TH18 的**同一格语义相同**：原生行循环 `th18:0x45ec41 mov al,[edi+0x20]` → `0` ⇒ 膛口 = 自机自身
#   `player+0x620`；非 0 ⇒ `player+0x580 + (n & 0xf) * 0xf0`，而那一族正是 **slot 1..4 的 4 架子机**
#   （`engine.h` 一手：`CE_NATIVE_OPT_PLAYER_MUZ0 0x670` = slot 1，`CE_NATIVE_OPT_COUNT 4`）
#   ⇒ **两作的编号法一致（0 = 本体、1 起 = 第 n 架子机）**，照抄不需要 ±1。
# 来源（本机 `local/th08.v1.00d/dat/ply0{0,1,2,3}as.sht` 直读；**烘成常量**是为了让 `make sht`
#   在没有 TH08 样本的机器上也能跑 —— 与上面三张行表同一条纪律）：
# ```
#   nsets = u16 @ +0x02;  槽数组从 +0x38 起、每槽 8 字节 = (u32 首行文件内偏移, i32 火力阈值)
#   行 stride 0x38，`i16 @ +0x00 < 0` = 终止符；`i16 @ +0x20` = 生成原点
#   ⇒ 逐档行数实测（括号里是阈值）：ply00as 2,2,3,4,5(,7) @ 8/24/48/80/128/999
#     ply01as 2,2,2,2,2(,2)；ply02as 3,3,5,5,5(,6)；ply03as 3,3,5,7,9(,11)  —— 与下面三张行表的
#     行数**逐一相等**（生成时 assert，两边不可能静默漂移）。
# ```
YOUKAI_TIER_ORIGINS = {
    # 霊夢・紫：低档 1 主 + 1 子机 → 满档 3 主 + 2 子机（本体行的原点恒 0）
    "R": [[0, 1], [0, 1], [0, 0, 1], [0, 0, 1, 1], [0, 0, 0, 1, 1]],
    # 魔理沙・アリス：两行恒为「1 子机 + 1 本体」（注意顺序 = 子机行在前）
    "M": [[1, 0], [1, 0], [1, 0], [1, 0], [1, 0]],
    # 咲夜・レミリア：三档起就是 1 本体 + 4 架子机各一路
    "S": [[0, 2, 3], [0, 2, 3], [0, 1, 2, 3, 4], [0, 1, 2, 3, 4], [0, 1, 2, 3, 4]],
}
# 既有 `REPLACE(YOUKAI, 0x16)` = ply03as slot4（9 行）的原点同样照抄（早苗的回落档也走引擎行循环）
YOUKAI_0X16_ORIGINS = [0, 0, 0, 0, 0, 1, 1, 2, 2]

# ---- 霊夢 / 八雲紫（TH08 `ply00as.sht` 的 slot 0..4 = 火力档 0..4）----
YOUKAI_TIERS_R = [
    [   # tier 0（TH08 阈值 8）
        _yk_row(rate=5, damage=48, dx=0.0, angle=-1.570796, sfx=0, speed=12.0, dy=0.0),
        _yk_row(rate=5, damage=24, dx=0.0, angle=-1.570796, sfx=-1, speed=10.0, dy=0.0),
    ],
    [   # tier 1（TH08 阈值 24）
        _yk_row(rate=5, damage=48, dx=0.0, angle=-1.570796, sfx=0, speed=12.0, dy=0.0),
        _yk_row(rate=5, damage=24, dx=0.0, angle=-1.570796, sfx=-1, speed=10.0, dy=0.0),
    ],
    [   # tier 2（TH08 阈值 48）
        _yk_row(rate=5, damage=30, dx=-8.0, angle=-1.570796, sfx=0, speed=12.0, dy=0.0),
        _yk_row(rate=5, damage=30, dx=8.0, angle=-1.570796, sfx=-1, speed=12.0, dy=0.0),
        _yk_row(rate=5, damage=24, dx=0.0, angle=-1.570796, sfx=-1, speed=10.0, dy=0.0),
    ],
    [   # tier 3（TH08 阈值 80）
        _yk_row(rate=5, damage=30, dx=-8.0, angle=-1.570796, sfx=0, speed=12.0, dy=0.0),
        _yk_row(rate=5, damage=30, dx=8.0, angle=-1.570796, sfx=-1, speed=12.0, dy=0.0),
        _yk_row(rate=5, damage=19, dx=0.0, angle=-1.53589, sfx=-1, speed=10.0, dy=0.0),
        _yk_row(rate=5, damage=19, dx=0.0, angle=-1.605703, sfx=-1, speed=10.0, dy=0.0),
    ],
    [   # tier 4（TH08 阈值 128）
        _yk_row(rate=5, damage=22, dx=-16.0, angle=-1.570796, sfx=0, speed=12.0, dy=0.0),
        _yk_row(rate=5, damage=24, dx=0.0, angle=-1.570796, sfx=-1, speed=12.0, dy=0.0),
        _yk_row(rate=5, damage=22, dx=16.0, angle=-1.570796, sfx=-1, speed=12.0, dy=0.0),
        _yk_row(rate=5, damage=19, dx=0.0, angle=-1.53589, sfx=-1, speed=10.0, dy=0.0),
        _yk_row(rate=5, damage=19, dx=0.0, angle=-1.605703, sfx=-1, speed=10.0, dy=0.0),
    ],
]
# ---- 魔理沙 / アリス（TH08 `ply01as.sht` 的 slot 0..4 = 火力档 0..4）----
YOUKAI_TIERS_M = [
    [   # tier 0（TH08 阈值 8）
        _yk_row(rate=2, damage=7, dx=0.0, angle=-1.570796, sfx=-1, speed=10.0, dy=0.0),
        _yk_row(rate=5, damage=10, dx=0.0, angle=-1.570796, sfx=0, speed=24.0, dy=0.0),
    ],
    [   # tier 1（TH08 阈值 24）
        _yk_row(rate=4, damage=7, dx=0.0, angle=-1.570796, sfx=-1, speed=10.0, dy=0.0),
        _yk_row(rate=5, damage=10, dx=0.0, angle=-1.570796, sfx=0, speed=24.0, dy=0.0),
    ],
    [   # tier 2（TH08 阈值 48）
        _yk_row(rate=6, damage=7, dx=0.0, angle=-1.570796, sfx=-1, speed=10.0, dy=0.0),
        _yk_row(rate=5, damage=10, dx=0.0, angle=-1.570796, sfx=0, speed=24.0, dy=0.0),
    ],
    [   # tier 3（TH08 阈值 80）
        _yk_row(rate=8, damage=7, dx=0.0, angle=-1.570796, sfx=-1, speed=10.0, dy=0.0),
        _yk_row(rate=5, damage=10, dx=0.0, angle=-1.570796, sfx=0, speed=24.0, dy=0.0),
    ],
    [   # tier 4（TH08 阈值 128）
        _yk_row(rate=12, damage=7, dx=0.0, angle=-1.570796, sfx=-1, speed=10.0, dy=0.0),
        _yk_row(rate=5, damage=10, dx=0.0, angle=-1.570796, sfx=0, speed=24.0, dy=0.0),
    ],
]
# ---- 咲夜 / レミリア（TH08 `ply02as.sht` 的 slot 0..4 = 火力档 0..4）----
YOUKAI_TIERS_S = [
    [   # tier 0（TH08 阈值 8）
        _yk_row(rate=5, damage=36, dx=0.0, angle=-1.570796, sfx=0, speed=20.0, dy=0.0),
        _yk_row(rate=10, damage=26, dx=0.0, angle=-1.570796, sfx=-1, speed=12.0, dy=0.0),
        _yk_row(rate=10, damage=26, dx=0.0, angle=-1.570796, sfx=-1, speed=12.0, dy=0.0),
    ],
    [   # tier 1（TH08 阈值 24）
        _yk_row(rate=5, damage=36, dx=0.0, angle=-1.570796, sfx=0, speed=20.0, dy=0.0),
        _yk_row(rate=5, damage=26, dx=0.0, angle=-1.570796, sfx=-1, speed=12.0, dy=0.0),
        _yk_row(rate=5, damage=26, dx=0.0, angle=-1.570796, sfx=-1, speed=12.0, dy=0.0),
    ],
    [   # tier 2（TH08 阈值 48）
        _yk_row(rate=3, damage=25, dx=0.0, angle=-1.570796, sfx=0, speed=20.0, dy=0.0),
        _yk_row(rate=30, damage=30, dx=0.0, angle=-1.570796, sfx=-1, speed=12.0, dy=0.0),
        _yk_row(rate=5, damage=20, dx=0.0, angle=-1.570796, sfx=-1, speed=12.0, dy=0.0),
        _yk_row(rate=5, damage=20, dx=0.0, angle=-1.570796, sfx=-1, speed=12.0, dy=0.0),
        _yk_row(rate=30, damage=30, dx=0.0, angle=-1.570796, sfx=-1, speed=12.0, dy=0.0),
    ],
    [   # tier 3（TH08 阈值 80）
        _yk_row(rate=3, damage=25, dx=0.0, angle=-1.570796, sfx=0, speed=20.0, dy=0.0),
        _yk_row(rate=15, damage=23, dx=0.0, angle=-1.570796, sfx=-1, speed=12.0, dy=0.0),
        _yk_row(rate=5, damage=20, dx=0.0, angle=-1.570796, sfx=-1, speed=12.0, dy=0.0),
        _yk_row(rate=5, damage=20, dx=0.0, angle=-1.570796, sfx=-1, speed=12.0, dy=0.0),
        _yk_row(rate=15, damage=23, dx=0.0, angle=-1.570796, sfx=-1, speed=12.0, dy=0.0),
    ],
    [   # tier 4（TH08 阈值 128）
        _yk_row(rate=3, damage=25, dx=0.0, angle=-1.570796, sfx=0, speed=20.0, dy=0.0),
        _yk_row(rate=10, damage=22, dx=0.0, angle=-1.570796, sfx=-1, speed=12.0, dy=0.0),
        _yk_row(rate=5, damage=21, dx=0.0, angle=-1.570796, sfx=-1, speed=12.0, dy=0.0),
        _yk_row(rate=5, damage=21, dx=0.0, angle=-1.570796, sfx=-1, speed=12.0, dy=0.0),
        _yk_row(rate=10, damage=22, dx=0.0, angle=-1.570796, sfx=-1, speed=12.0, dy=0.0),
    ],
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


# =====================================================================
# ★ 2026-09-13（卡 113「灵界」）：TH13 灵界主炮组（`pl0Xa.sht` set **10**）的 TH18 落点
#
# ## 一、为什么是「孤儿段 + 运行时借用一格」而不是又一个 `APPEND` / `REPLACE`
#
# 一手普查（本会话在本机 `th18.exe` 上重跑，不是抄文档）：
#   · 偏移数组 **40 格全满**：原生 `0x00`–`0x16`（23 格）+ 本 mod 的 `APPEND` `0x17`–`0x27`
#     （17 格）= 40 = 头部 `+0x02`。`build_one()` 的 `N_RETAIL + len(sets) > N_SLOTS` 直接拦死，
#     **再没有 `APPEND` 的余地**。
#     ★ 2026-09-14：容量已扩到 96 格（AUDIT §CQ）⇒ 现在**有** `0x28` 起 56 格空位，
#       本卡的正式收尾（换成永久专属槽 `0x28`、删掉借格/还原/账本）是 `NEXT.md` §1w 第 5 步，
#       排在 113 这轮实跑验收之后。**在那之前本节的三条路推理与实现都照旧生效**。
#   · 原生那 23 格**一格都不空**：扫全 exe 的 `E8 rel32`，`Player__tick_shooters_for_ability_card`
#     `0x40a9c0` 共 **11 个调用点**，压栈的 set 立即数逐点读出来是
#     `0x40ab00→0xa` / `0x40ac10→0x12` / `0x40ad20→0xb` / `0x40ae30→0x13` /
#     `0x40af60→0xc+(聚焦?1:0)`（= `0xc`/`0xd` 两格）/ `0x40b080→0x14` / `0x40b190→0xe` /
#     `0x40b2a0→0x15` / `0x40b3ce→0x11` / `0x40b5f2 push 0xf` / `0x40bb30→0x10`
#     ⇒ **`0x0a`–`0x15` 十二格全部是活的原生装备卡子机组**（11 张卡，咲夜那张占两格，
#     与 `engine/card/th18/08-catalog.md` §D 的表逐行对得上）。
#     `0x16` 早被 108 的妖侧 set 用掉了（`REPLACE`）⇒ **REPLACE 也没有干净的槽可抢**：
#     抢任何一格 = 那张原生装备卡（任选角色都能装）在**非灵界时**也打灵界弹。
#   · 主炮 `0x00`–`0x09` 同理：平时每帧都在用，不能动。
#   ⇒ 结论：**「抢一格」和「加一格」都做不到零代价**，于是本块采取第三条路 ——
#     **数据落在文件尾的「孤儿段」（没有任何偏移项指向它），运行时由 `cards/trance.c`
#     只在灵界窗口内把借用格临时指向它，退出/删卡时还原原生指针。**
#
# ## 二、借用的那一格为什么在窗口内是安全的（同一份普查）
#
#   借用格 = `CE_SHT_SET_TRANCE` = **`0x0e`**（原生 = `SANAE_OP` 装备卡的子机组）。
#   灵界窗口内它的**唯一**读者是「装备卡子机路径」`0x40a9c0`，而那条路被本卡自己的
#   `on_option_shot_gate` 掐掉（TH13 灵界组**一条子机 row 都没有** ⇒ 复刻要求它停火），
#   主炮路径用的又是 `on_shot_set_index` 返回的**本卡**索引 ⇒ 窗口内 `0x0e` **无人读**。
#   窗口外本卡把该格**还原成解析器算出来的原生指针**（保存的是进入窗口那一刻读到的值，
#   ⇒ 不需要在 C 侧硬编码原生偏移，也就不会随数据改动而失配）。
#   ★ 失败方向：如果还原逻辑漏了，最坏结果是「装 SANAE_OP 的玩家打出灵界弹」——
#     看得见、不崩、日志里能查到；**不会**像"文件里直接覆盖"那样留下永久回归。
#
# ## 三、行数据：TH13 一手逐字节（`thdat -x 13` 的 `pl0Xa.sht` set 10）→ TH18 字段
#
#   一手读法：`local/th13.v1.00c/dat/pl0Xa.sht`，头部 `+0x02 = 11`、数组 `+0xc8`、
#   重定位基址 `+0xf4`、row stride `0x38`（见 `engine/spirit/th13/05-trance-attack.md` §1.2）。
#   实测三行（本会话重跑的字节解析，与那篇 §2 的表**多出一个字段 `dy`**，见下）：
#     灵梦  set10 = (rate2 dmg22 dx∓16 dy−8 wh64/24 spd24 anm4 sfx21) ×2
#     魔理沙 set10 = (rate2 dmg22 dx∓16) (rate2 dmg22 dx∓16) (rate2 dmg11 dx∓30) ×2, wh48/18, anm1
#     早苗  set10 = (rate2 dmg22 dx∓16 dy−8 wh48/18 spd24 anm1 sfx21) ×2
#     妖梦  set10 = rate30 dmg25 dy−48/−60（**TH18 没有妖梦，丢弃**，见 `REPORT-TRANCE-ATTACK-FEASIBILITY.md` §4.3）
#   全部：`delay=0`、`ang=−π/2`、`slot=0`、`mode=0`、四个 func 全 0（⇒ **没有诱导**、**没有子机 row**）、
#   `+0x1c = 0`。
#
#   逐字段映射（左 = TH13 `pl0Xa.sht`，右 = TH18 `pl0X.sht`；字段图见 `engine/sht/th18/02` §1）：
#     `+0x00` rate → `+0x00` fire_rate（两侧判据同形：`timer % rate == delay`）✅
#     `+0x01` delay → `+0x01` start_delay ✅（两作灵界组都是 0）
#     `+0x02` dmg → `+0x02` damage ✅（**TH13 没有伤害倍率**，增伤就在这两个数里）
#     `+0x04/+0x08` → `+0x04/+0x08` 出膛偏移 x/y ✅（**`dy = −8.0` 是本次逐字节新读到的**：
#         TH13 set10 三条自机都有，上一版文档表只列了 `dx`）
#     `+0x0c/+0x10` → `+0x0c/+0x10` 🟡 **语义不同名**：TH13 这两个 float 进 `bullet+0x88/+0x8c`
#         （那篇标 🟡「尺寸」），TH18 进 `bullet+0xa0/+0xa4`（判定框宽/高）。
#         取值**照抄 TH13 的数**（灵梦 64/24、魔理沙与早苗 48/18）—— 它们是同一量级、
#         与弹贴图（78×30 / 62×30）成比例，是对得上的一手数字；**不自己发明**。
#     `+0x14` ang → `+0x14` angle ✅（−π/2 = 正上）
#     `+0x18` spd → `+0x18` speed ✅
#     `+0x1c` → TH18 `+0x1c..+0x1f`（原生 268 条全 0）✅ 填 0
#     `+0x20` slot → `+0x20` opt_slot ✅（0 = 自机本体）
#     `+0x21` mode → `+0x21` mode ✅（0）
#     `+0x22` anm → `+0x22` anm ⚠ **数字空间不同**：TH13 实际读 `pl0X.anm script(row->anm + 10)`
#         （`th13:0x445e0c`/`0x445e10`），TH18 直接读 `script(row->anm)`。
#         ⇒ 这里填的是**我们往 TH18 `pl0X.anm` 追加的脚本号**（见 `build_player_anm.py` 的
#         `PL00_TRANCE=41` / `PL01_TRANCE=47` / `PL03_TRANCE=32`，`check_pl_anm_ids()` 会交叉核对）。
#     `+0x24` sfx → `+0x24` sfx ✅（首行 21、其余 −1；TH18 的 `0x15 = se_tan00` 是自机弹音 ⇒ 同族）
#     `+0x28/+0x2c/+0x30/+0x34` func → `+0x2c..+0x38` func ✅ 全 0（平弹；TH13 灵界组也没有诱导）
#     TH18 有、TH13 没有的：`+0x26 hit_fx` / `+0x2a fire_rate2` / `+0x2b start_delay2` / `+0x4c tilt`
#         ⇒ 全 0（只有激光/倾斜路径读它们，本组是平弹）
#
# ## 四、咲夜（TH18 `pl02`）为什么是空组
#
#   TH13 没有咲夜（四机体是 灵梦/魔理沙/早苗/妖梦）⇒ 用户 2026-09-13 定案「咲夜不变」。
#   这里仍然**产出**一份空组（只有终止符）把四个文件的孤儿段对齐：即使哪天有人误把
#   `CE_SHT_SET_TRANCE` 返回给咲夜，引擎的 row 循环见终止符就直接不发弹（安全回退）。
TRANSIENT_SLOT = 0x0E              # 借用的原生格（= SANAE_OP 的装备子机组），见上面 §二
_TRANCE_ANGLE = -1.5707963705062866  # = −π/2（float32 的 −π/2），TH13 三条都是这个值


def _tr_row(rate, dmg, dx, hit_w, hit_h, anm, sfx):
    """TH13 `pl0Xa.sht` set10 的一行 → TH18 shooter dict（映射表见上面注释 §三）。"""
    return dict(fire_rate=rate, start_delay=0, damage=dmg,
                spawn_dx=dx, spawn_dy=-8.0,          # −8.0 = TH13 逐字节读到的 dy
                hit_w=hit_w, hit_h=hit_h,
                angle=_TRANCE_ANGLE, speed=24.0,
                opt_slot=0, mode=0,
                anm=anm, sfx=sfx,
                func_on_init=0, func_on_tick=0, func_on_draw=0, func_on_hit=0)


# 追加脚本号必须与 `assets/build_player_anm.py` 的计划一致（`check_pl_anm_ids()` 交叉核对）。
# 数字空间：TH13 的 `pl00.anm script14` / `pl01.anm script11` / `pl02.anm script11`（= anm+10）
# 在 TH18 这边对应**新追加**的脚本（TH18 的 pl00/pl01 没有对应脚本，pl03 的原生 script11 是别的东西）。
PL00_SCRIPT_TRANCE = 41
PL01_SCRIPT_TRANCE = 47
PL03_SCRIPT_TRANCE = 32

TRANCE_ROWS = {
    # 灵梦（TH13 `pl00a.sht`，CHARACTER 0）：2 row，dmg 22，dx ±16，rate 2，速 24
    "pl00.sht": [_tr_row(2, 22, -16.0, 64.0, 24.0, PL00_SCRIPT_TRANCE, 21),
                 _tr_row(2, 22,  16.0, 64.0, 24.0, PL00_SCRIPT_TRANCE, -1)],
    # 魔理沙（TH13 `pl01a.sht`，CHARACTER 1）：4 row/轮 —— dx ±16 是 22、dx ±30 是 11，全 rate 2
    "pl01.sht": [_tr_row(2, 22, -16.0, 48.0, 18.0, PL01_SCRIPT_TRANCE, 21),
                 _tr_row(2, 22,  16.0, 48.0, 18.0, PL01_SCRIPT_TRANCE, -1),
                 _tr_row(2, 11, -30.0, 48.0, 18.0, PL01_SCRIPT_TRANCE, -1),
                 _tr_row(2, 11,  30.0, 48.0, 18.0, PL01_SCRIPT_TRANCE, -1)],
    # 咲夜（TH18 pl02）：TH13 没有这个自机 ⇒ **用户 2026-09-16 新定案**：
    #   数值**照抄灵梦那一份**（2 row、rate 2、dmg 22、dx ±16、dy −8、判定 64×24、速 24 ⇒ DPS 22），
    #   **贴图用她自己的刀弹** —— row 的 `anm` 填 **7** = TH18 `pl02.anm` 的刀弹脚本
    #   （一手：零售 `pl02.sht` 主炮行的 `anm` 字段就是 7/9；切图见 anm_compare/咲夜_刀弹_script7.png）。
    #   ⚠ 覆盖了 2026-09-13 的「咲夜不变」旧口径（那份已随本条作废）。
    "pl02.sht": [_tr_row(2, 22, -16.0, 64.0, 24.0, 7, 21),
                 _tr_row(2, 22,  16.0, 64.0, 24.0, 7, -1)],
    # 早苗（TH13 `pl02a.sht`，CHARACTER 2 → TH18 pl03）：2 row，dmg 22，dx ±16
    # ⚠ TH13 的「4 way」是**视觉**（一张 `pl02c.png` 里两条横条），数据上只有 2 row —— 照抄 2 row
    "pl03.sht": [_tr_row(2, 22, -16.0, 48.0, 18.0, PL03_SCRIPT_TRANCE, 21),
                 _tr_row(2, 22,  16.0, 48.0, 18.0, PL03_SCRIPT_TRANCE, -1)],
}


def trance_blob(target_file: str, anm_ids: dict) -> bytes:
    """该文件的灵界组字节（含终止符）。空列表 = 只有终止符的空组。"""
    return b"".join(pack_shooter(r, anm_ids) for r in TRANCE_ROWS[target_file]) + TERMINATOR


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


# ---- 111 地灵殿：魔理沙×帕秋莉「子机配置切换」五档（**本轮新占**）-----------------------
#
# ★★ **必须接在 `_hyper_sets()` 之后**（= `APPEND` + WOLF/EAGLE 两组之后），不能插进 `APPEND` 中间：
#   槽号 = `N_RETAIL + 在生成序列里的序号`，**插在中间会把 117 的 WOLF/EAGLE 十个槽全部顶掉一格**
#   （`animal_core.h` 的 `ANIMAL_SHOT_SET_*` 写死旧号 ⇒ 静默错弹）。这是本轮实际踩到并改回的一条。
#   现在这五组落在 **0x28–0x2c**（见 next_free_slot 的断言）。
#
# ## 一手：这件事在原版是「换一组子机的角/距 + 贴图」，**不换子弹**
#   `th11:0x4308fe`：`if (INPUT_B & 0x400) formation = (formation + 1) % 5`（`th11:0x43090a`–`0x430921`）；
#   随后 `th11:0x430929` 把 **ANM script = `0x22 + formation`**（`th11:0x430949`）写进每个子机的
#   `zPlayerOption+0xb0`，并把 `zPlayerOption+0x74` 的
#   `marisa_b_preferred_offsets_by_formation[formation]`（5 档 × zInt2）拷进目标角/距字段
#   （`th11:0x430970` 的 `[ebp+eax*8+0x75e4]`；`0x75e4 = 0x7570 + 0x74` ✓，步长 `0xe4`）。
#   ⇒ **原版五档的差别全在几何/贴图，弹种是同一条**（TH11 的 option 弹由 `pl01b.sht` 的 `+0x28`
#     专用发射函数驱动，那套函数在 TH18 没有对应物）。
#
# ## 那这五组为什么要建
#   ① 用户拍板的落点是「卡侧走 `ce_allocate_option()` + `ce_tick_shooters_for_card(opt, timer, 0, set)`」
#      —— 这条路径要**五格真实存在的 shooterset**；
#   ② TH18 侧的对应物取**原生「迷你八卦炉」那一行的族**：`mode=0`、`func_*` 全 0（平弹）、
#      `opt_slot=0`（用传进来的子机坐标）、出膛音 −1；
#   ③ 五档只让「方向 / 速度」微差，**五档的每帧伤害完全相等** ——
#      原版切换配置**不改变输出**，所以这里也**不拿它当强度来源**。
#
# ## ❓ 自定（登记 TUNING.md）
#   原版五档的**具体角/距表没能取到**（全 exe 对 `th11:0x75e4` 只有 `0x430970`/`0x43098a` 两处**读**、
#   没有任何写点，也没有 `75 e4 00 00` 的数据镜像）⇒ 阵型几何在 `native/cards/signal_core.c`
#   的 `s_formation[][]` 里自定，这里的五组**数值也是自定**（取了原生同形行的取值范围）。
#
# ## ★★ 2026-09-16：**每发伤害按「18/13 的 DPS 比例」重新定过（20，不再 144）**
#   用户口径（逐字）：「火力随 P 完全依赖原生关系，但是，得根据这一作的具体 dps 进行调整。
#   保证平均 dps 一致即可，比如可以相邻两次发射的弹幕攻击力不一样，以符合平均 dps 关系。
#   大概是 18魔理沙/13魔理沙 = 18灵梦/13灵梦」。
#   口径、一手表、算式、残留偏差的唯一出处 = `native/cards/signal_core.h` 的
#   「原生子机抑制 + 子机格数随 P + DPS 归一」那一段（**别在这里重复推理**）。落到这五组上只有一句：
#     · **格数**由卡侧按原生火力档声明（档 N ⇒ N 格，`signal_power_cells()`）⇒ 本组会被 tick **N 次**；
#     · **单发伤害** `D` 要满足「满档 4 格合计 = 本作原生子机层的每帧输出」：
#       `4 × D / fire_rate = V18_opt(4) = 6.667` ⇒ **D = 6.667 × 12 / 4 = 20**（`fire_rate` 保持 12）；
#     · 「平均 dps 一致」逐档核对：原生 `1.867/2.400/5.600/6.667` vs 我们 `1.667/3.333/5.000/6.667`
#       ⇒ 四档均值 `4.133` vs `4.167` = **+0.8%**（档 2 的 +39% 是原生自己非单调，见 core.h 的残留偏差一节）。
#   ⚠ **交替**（用户允许「相邻两发伤害不同」）在这一版**用不上**：D 是整数 ⇒ `(lo, hi) = (20, 20)`。
#     将来若要逐档拟合原生那条非单调曲线（`D(档) = 12 × V18_opt(档) / 档` = 22.4 / 14.4 / 22.4 / 20.0），
#     就把 `MARISA_FORMATION_ROW_DMG` 换成 `(lo, hi)` 对并按档拆组 —— 写法与代价见 TUNING.md 的 111 行。
#
# ## 不超模（每帧伤害横向对照；本仓既有口径 = Σdmg/rate）
#   满档（4 格）时每帧 `4 格 × 1 行 × 5 / 3 = 6.667` —— **逐值等于**原生魔理沙满档子机层的 6.667
#   （本卡把原生那 4 格子机整层关掉，所以这是**替换**、不是叠加）。对照：117 灵梦 WOLF 63.33（7 行）、
#   116 春/夏/秋 各 3.0/行、原生「迷你八卦炉」（set 0x0b，rate5/dmg13）2.6/行。
# ★★ 2026-09-16n **整套改成 TH11 的款式**（用户口径：「**你得用 th11 的魔理沙子机贴图和弹幕样式呀**」）。
#   一手（`local/th11.v1.00a/th11.exe`，逐字节实读）：
#     · TH11 的子机弹**永远走 `pl0X.anm`、脚本号 = `shooter[+0x1e] + 5`**（`th11:0x4341fb` / `0x4341ff`），
#       而 TH18 的卡子机走 **`ability.anm`**（`th18:0x40aa03` 换 `player+0x10`）⇒ 把那五格 1:1 搬过来（脚本 288..292）。
#     · **五档 = MarisaB（帕秋莉）的五元素**：`th11:0x433465 mov ecx,[ebx+0x8bac]` / `0x433470 add ecx,0x22`
#       ⇒ `pl01.anm` script34..38（五个纯色球）⇒ **子机贴图也按 formation 选**（卡侧 `signal_option_script_for()`）。
#   ★★ 2026-09-16zb **发弹数重核 + 改正一处旧误读**（用户口径：「**发弹数得大致对上，然后再考虑怎么调 dps**」）
#     · ❌ 旧注释写的「每格 4 行 stagger（0/2/4/6）、`fire_rate = 60`、判定盒 **16x32**」是**误读**：
#       那几条取自 **`pl01a.sht`**（TH11 魔理沙的**另一个**机体/配置，它的子机行才是 16–20 行、rate 60、
#       盒 16x32、speed 12、`anm=2`）。**元素弹在 `pl01b.sht`**（`anm` 6/4/0/8/10 → `pl0X.anm` 脚本 11/9/5/13/15
#       = 火/水/木/金/土），骨相完全不同 ⇒ 本轮按 **pl01b** 重写（旧版把 pl01b 的 speed 24 与 pl01a 的行数/盒混在了一起）。
#     · 一手（`local/th11.v1.00a` 的 `pl01b.sht`，逐字节实读；set 4/9/14/19/24 = 五档满档）：
#       `slot>=1`、`sfx=-1`、`dx=dy=0`、`speed = 24`、**`start_delay = 0`**、
#       判定盒 **16x48**、`fire_rate` 逐元素不同（火 3 / 水 6 / 木 3 / 金 3 / 土 3）：
#         | 元素 | 每格行数 | rate | dmg | 角度（一手） | 我们本轮 |
#         | 火(anm6) | 1 | 3 | 5 | -1.669 / -1.473 交替 | rate3 / dmg5 / 盒16x48 |
#         | 水(anm4) | 2 | 6 | 15 | 每格两个角 | 同上（1 行，密度等价） |
#         | 木(anm0) | 1 | 3 | 18 | -2.356 / -0.785 | 同上 |
#         | 金(anm8) | 2 | 3 | 12 | 0 与 pi（正右/正左） | 同上（**只有 TH11 的一半**） |
#         | 土(anm10)| 1 | 3 | 12..20 | +1.571（正下） | 同上 |
#       ⇒ 每格**发弹数**：火/木/土 = 1/3 发/帧、水 = 2/6 = 1/3、金 = 2/3（TH11 的金是两倍）。
#     · **本轮五档一律取「1 行 / 格、`rate = 3`」**（= 火/木/土的骨相，也是水档同密度的等价写法）：
#       满档 4 格 ⇒ **20 发 / 15 帧 = 1.3333 发/帧**，逐值等于 TH11 的主密度（火/木/土一手同值）。
#       ⚠ 与 TH11 的**已知差异**（留给「调 dps」那一轮，别当成复刻）：① 金档少一半弹（少了 `ang 0/pi` 那一行）；
#       ② 木/土的斜角、火档的角度交替没抄（本轮统一 -pi/2）；③ 水档的「两发齐射」变成均匀流（同密度、不同手感）。
#     · ★ 喂给行判定的**计时器也必须是一手的**（0..14、周期 15）：`signal.c` 的 `on_tick_shooters` 现在把
#       引擎给的那一个（`th18:0x45ecb8` 推的 `player+0x476d4`）原样转给 `ce_tick_shooters_for_card()` ——
#       与 TH11 的开火循环同源（`th11:0x4342f0` 拿 `[edi+0x934]`、`0x459270` increment、`0x4343f2` 用
#       `0x498300 = -14.0f` 回绕）⇒ `timer % 3 == 0` 在 15 帧里正好 5 次。
#     · **伤害**仍是反解出来的（5，不是 TH11 的原值）：TH11 五档自己的子机层输出 = 6.667 / 20.0 / 24.0 / 32.0 / 21.333
#       （差 5 倍），而用户 2026-09-16 拍板过「**保证平均 dps 一致**（满档 = 本作原生子机层 6.667/帧）」
#       ⇒ 火档连 `(rate, dmg) = (3, 5)` 都逐值等于 TH11，其余四档 =「TH11 的节奏 + 归一的伤害」：
#       `4 格 × 1 行 × 5 / 3 = 6.6667` 精确等于目标。「五档不等强（照抄 TH11 的 dmg）」是另一条路，
#       要改断言与那条用户口径 —— 没拍板前不动。
# ★★★ 2026-09-16zb：**整套改成 TH11 `pl01b.sht` 的逐字段复刻**（用户口径：「尽可能完全对齐」
#     +「一次性落，但是主炮别换弹幕了」）。旧版的「五档 = 每组 1 行、伤害归一成同一个数」**已作废**。
#
# ## 一、一手（本仓自己逐字节读 `local/th11.v1.00a` 解出的 `pl01b.sht`；`thdat -x 11`）
#   · **25 组 = 5 元素 × 5 档**（档 0 = 零子机），组索引 = `档 + (档数+1) × 元素`
#     （`th11:0x434333`–`0x43433b`：`index = level + ([shooter+0x20]+1) * [player+0x8bac]`；
#      pl01b 头 `+0x20 = 4`（档数）、`+0x24 = 20`（每档火力单位）⇒ 步长 5）。
#   · **每组同时含主炮行与子机行**（同一个开火循环 `th11:0x4342f0` 全发；
#     `Player__shoot_one_bullet` 侧 `slot ≥ 1` 的行若找不到活子机直接 `return 0` 不发）。
#   · **主炮行**：2 行、`rate 3`、`delay 0`、`dx = ∓10`、`dy = −8`、判定盒 **18×48**、
#     `angle −π/2`、`speed 24`、`slot 0`、anm 0、两个回调全 0 ⇒ 每 3 帧 2 发 = 0.667 发/帧。
#     每发伤害**逐元素逐档**不同（表 `MARISA_TH11_MAIN_DMG`，档 0 一律 16）。
#   · **子机行**：`rate` 逐元素（火/木/金/土 3、水 6）、`delay 0`、判定盒 **16×48**、`speed 24`、
#     角度**逐格**不同（火 ±0.098 交替、水每格一对、木 ±45°、金 {π, 0} 两发、土一律 +π/2）。
#
# ## 二、我们怎么复刻（**组的形状与 TH11 不同，但发出去的行逐字段相同**）
#   · TH11 是「一组里**平铺**列出该档每一格的行」；我们的机制是「**一组被 tick N 次**」
#     （卡侧按自己承认的子机格数逐格调 `ce_tick_shooters_for_card`，行用 `opt_slot = 0`
#       ⇒ 用传进来的那一格的坐标）。所以这里把 TH11 的「(元素,档,格) 行组」**去重成组**：
#     同一个行组（逐字段相同）只占一格 ⇒ **23 个子机组**。
#   · 主炮另立一组（TH11 的主炮行就长在同一组里，但我们的主炮是由引擎的原生行循环发的
#     ⇒ 由卡侧的 `on_shot_set_index` 覆盖整组）：**8 个主炮组**（= 8 个不同的单发伤害值）。
#   · 合计 **31 组**（0x29–0x47），占用仍落在 `write_ids_h` 算出来的空槽里，不用抬 `CE_SHT_SLOTS`。
#   · ⚠ **主炮行沿用本作原生的弹**（anm 5 / mode 7 / sfx 首行 0 次行 −1，与 TH18 `pl01.sht`
#     set 0x04 的主炮行同形）—— 用户 2026-09-16zb 口径「**主炮别换弹幕了**」⇒ 只抄 TH11 的数值。
#   · ⚠ 已知差异（明写）：TH11 的**阵型几何**（子机站位）我们取不到（`th11:0x75e4` 无写点）
#     ⇒ 站位仍是本仓自定（`signal_core.c` 的 `s_formation[][]`）；TH11 也**没有聚焦维度**
#     （25 组不含聚焦）⇒ 我们的覆盖对聚焦/非聚焦**给同一组**。
#
# ## 三、对账（`marisa_th11_check()` 是**硬断言**）
#   · 逐 (元素,档) 复算「主炮/帧 / 子机/帧」，必须**逐值等于** `MARISA_TH11_DPS`（从一手行算出来的表）；
#   · 改了任何一行数值而没重新对账 ⇒ `make sht` 直接停。
MARISA_ELEMS = ("FIRE", "WATER", "WOOD", "METAL", "EARTH")
MARISA_ELEM_CN = {"FIRE": "\u706b", "WATER": "\u6c34", "WOOD": "\u6728",
                  "METAL": "\u91d1", "EARTH": "\u571f"}
# 子机弹脚本（288..292）：TH11 用 `shooter[+0x1e] + 5` 去 `pl0X.anm` 取（火 6→11 / 水 4→9 /
# 木 0→5 / 金 8→13 / 土 10→15），五支正文只差 `sprite()`；我们把五格 1:1 切进 `ability.anm`。
MARISA_ELEM_ANM = {
    "FIRE":  "CE_ANM_ABILITY_SCRIPT_MARISA_ELEM_FIRE_BULLET",
    "WATER": "CE_ANM_ABILITY_SCRIPT_MARISA_ELEM_WATER_BULLET",
    "WOOD":  "CE_ANM_ABILITY_SCRIPT_MARISA_ELEM_WOOD_BULLET",
    "METAL": "CE_ANM_ABILITY_SCRIPT_MARISA_ELEM_METAL_BULLET",
    "EARTH": "CE_ANM_ABILITY_SCRIPT_MARISA_ELEM_EARTH_BULLET",
}
MARISA_MAIN_DMG = {                       # 一手：主炮每发伤害（[元素][档 0..4]）
    "FIRE":  [16, 14, 13, 12, 12],
    "WATER": [16, 14, 13, 12, 11],
    "WOOD":  [16, 16, 16, 16, 16],
    "METAL": [16, 10, 10,  8,  8],
    "EARTH": [16, 15, 15, 15, 15],
}
MARISA_OPT_ROWS = {                       # 一手：子机行（[元素][档 1..4][格] = [(rate, dmg, angle), …]）
    "FIRE": {
        1: [[(3, 9, -1.5707964)]],
        2: [[(3, 7, -1.6689711)], [(3, 7, -1.4726216)]],
        3: [[(3, 6, -1.5707964)], [(3, 5, -1.6689711)], [(3, 5, -1.4726216)]],
        4: [[(3, 5, -1.6689711)], [(3, 5, -1.4726216)],
            [(3, 5, -1.6689711)], [(3, 5, -1.4726216)]],
    },
    "WATER": {
        1: [[(6, 15, -1.7671459), (6, 15, -1.3744467)]],
        2: [[(6, 15, -1.7671459), (6, 15, -1.6689711)],
            [(6, 15, -1.3744467), (6, 15, -1.4726216)]],
        3: [[(6, 15, -1.7016960), (6, 15, -1.4398966)],
            [(6, 15, -1.8325957), (6, 15, -1.7278760)],
            [(6, 15, -1.3089969), (6, 15, -1.4137167)]],
        4: [[(6, 15, -1.8849556), (6, 15, -1.7802359)],
            [(6, 15, -1.2566371), (6, 15, -1.3613569)],
            [(6, 15, -1.7671459), (6, 15, -1.6689711)],
            [(6, 15, -1.3744467), (6, 15, -1.4726216)]],
    },
    "WOOD": {
        1: [[(5, 18, -2.3561945), (5, 18, -0.7853982)]],
        2: [[(3, 18, -2.3561945)], [(3, 18, -0.7853982)]],
        3: [[(5, 18, -2.3561945), (5, 18, -0.7853982)],
            [(3, 18, -2.3561945)], [(3, 18, -0.7853982)]],
        4: [[(3, 18, -2.3561945)], [(3, 18, -0.7853982)],
            [(3, 18, -2.3561945)], [(3, 18, -0.7853982)]],
    },
    "METAL": {
        1: [[(3, 12, 3.1415927), (3, 12, 0.0)]],
        2: [[(3, 12, 3.1415927), (3, 12, 0.0)], [(3, 12, 3.1415927), (3, 12, 0.0)]],
        3: [[(3, 12, 3.1415927), (3, 12, 0.0)], [(3, 12, 3.1415927), (3, 12, 0.0)],
            [(3, 12, 3.1415927), (3, 12, 0.0)]],
        4: [[(3, 12, 3.1415927), (3, 12, 0.0)], [(3, 12, 3.1415927), (3, 12, 0.0)],
            [(3, 12, 3.1415927), (3, 12, 0.0)], [(3, 12, 3.1415927), (3, 12, 0.0)]],
    },
    "EARTH": {
        1: [[(3, 28, 1.5707964)]],
        2: [[(3, 21, 1.5707964)], [(3, 21, 1.5707964)]],
        3: [[(3, 14, 1.5707964)], [(3, 21, 1.5707964)], [(3, 14, 1.5707964)]],
        4: [[(3, 12, 1.5707964)], [(3, 20, 1.5707964)],
            [(3, 20, 1.5707964)], [(3, 12, 1.5707964)]],
    },
}
# 一手算出来的期望值（[元素][档] = (主炮/帧, 子机/帧)）—— 断言用；别再手算。
MARISA_TH11_DPS = {
    "FIRE":  [(10.666667, 0.0), (9.333333, 3.0), (8.666667, 4.666667), (8.0, 5.333333), (8.0, 6.666667)],
    "WATER": [(10.666667, 0.0), (9.333333, 5.0), (8.666667, 10.0), (8.0, 15.0), (7.333333, 20.0)],
    "WOOD":  [(10.666667, 0.0), (10.666667, 7.2), (10.666667, 12.0), (10.666667, 19.2), (10.666667, 24.0)],
    "METAL": [(10.666667, 0.0), (6.666667, 8.0), (6.666667, 16.0), (5.333333, 24.0), (5.333333, 32.0)],
    "EARTH": [(10.666667, 0.0), (10.0, 9.333333), (10.0, 14.0), (10.0, 16.333333), (10.0, 21.333333)],
}
# 行形状常量（一手 pl01b.sht；主炮的弹/音效按用户口径**用本作原生**，见上）
MARISA_MAIN_RATE, MARISA_MAIN_DELAY = 3, 0
MARISA_MAIN_DX, MARISA_MAIN_DY = 10.0, -8.0
MARISA_MAIN_BOX = (48.0, 18.0)            # TH18 原生 `pl01.sht` set 0x04 主炮行的盒（本作口径）
MARISA_MAIN_ANGLE, MARISA_MAIN_SPEED = -1.5707964, 24.0
MARISA_MAIN_ANM, MARISA_MAIN_MODE = 5, 7  # anm 5 = `pl0X.anm` script5（原生主炮弹）；mode 7 = 原生主炮的档位
MARISA_OPT_DELAY, MARISA_OPT_SPEED = 0, 24.0
MARISA_OPT_BOX = (16.0, 48.0)
MARISA_FORMATION_CELLS = 4                # = 原生满档子机格数 / 卡侧 `SIGNAL_OPTION_MAX`
MARISA_LEVELS = 5                         # ★ TH11 的档数 = 5（档 0..4；`pl01b` 头 `+0x20 = 4` + 1）
# ★★ **`0x28` = 113 的永久专属槽**（NEXT.md §1w 第 5 步，2026-09-15 落地）。
#    此前那一格是「洞」（序列里占位、偏移项 0）+ 文件尾的**孤儿段**，靠卡侧临时把偏移项指过去；
#    现在它是一格**正常的追加组**（名字 `TRANCE`），行数据 = TH13 灵界 set10 的复刻行
#    ⇒ 卡侧只返回槽号、不写任何 sht 内存（删掉约 120 行借用/还原/账本），
#      而且失去"孤儿指针算到影像之外"这一整类失败模式（AUDIT §CV）。
#    111 的 31 组落在 **0x29 起**（第 6 步）：序列里那一格本来就占着位，改成有数据不会让谁移位。
RESERVED_113_SLOT = 0x28               # 断言用：113 的槽必须落在这一格
MARISA_FORMATION_SET_BASE = 0x29       # ★ 断言用：111 的第一组必须落在 0x29 起（NEXT.md §1w 第 6 步）


def _marisa_layout() -> tuple:
    """把 TH11 的 `(元素, 档, 格)` 行组**去重**成「组」，并算出卡侧要的映射。

    返回 `(opt_specs, main_specs, opt_map, main_map)`：
      · `opt_specs`  = [((元素, 行组), [row dict 的原料]), …]（顺序 = 组号顺序）
      · `main_specs` = [(伤害值, 元素), …]
      · `opt_map[元素][档][格]` / `main_map[元素][档]` = **绝对槽号**（`MARISA_FORMATION_SET_BASE + i`）
        或 `-1`（该档没有这一格）。"""
    opt_specs, opt_index = [], {}
    opt_map = [[[-1] * MARISA_FORMATION_CELLS for _ in range(MARISA_LEVELS)]
               for _ in MARISA_ELEMS]
    for e, elem in enumerate(MARISA_ELEMS):
        for lv in range(1, MARISA_LEVELS):
            cells = MARISA_OPT_ROWS[elem][lv]
            assert len(cells) == lv, f"{elem} 档 {lv} 的格数 {len(cells)} != {lv}（一手 pl01b 是逐档加一格）"
            for c, rows in enumerate(cells):
                key = (elem, tuple(rows))
                if key not in opt_index:
                    opt_index[key] = len(opt_specs)
                    opt_specs.append((key, rows))
                opt_map[e][lv][c] = MARISA_FORMATION_SET_BASE + opt_index[key]
    main_specs, main_index = [], {}
    main_map = [[-1] * MARISA_LEVELS for _ in MARISA_ELEMS]
    base_after_opt = MARISA_FORMATION_SET_BASE + len(opt_specs)
    for e, elem in enumerate(MARISA_ELEMS):
        for lv in range(MARISA_LEVELS):
            dmg = MARISA_MAIN_DMG[elem][lv]
            if dmg not in main_index:
                main_index[dmg] = len(main_specs)
                main_specs.append((dmg, elem))
            main_map[e][lv] = base_after_opt + main_index[dmg]
    return opt_specs, main_specs, opt_map, main_map


def _marisa_opt_row(elem: str, rate: int, dmg: int, angle: float) -> dict:
    return dict(fire_rate=rate, start_delay=MARISA_OPT_DELAY, damage=dmg,
                hit_w=MARISA_OPT_BOX[0], hit_h=MARISA_OPT_BOX[1],
                angle=angle, speed=MARISA_OPT_SPEED, opt_slot=0, mode=0,
                anm=MARISA_ELEM_ANM[elem], sfx=-1, func_on_init=0)


def _marisa_main_rows(dmg: int) -> list:
    """主炮 2 行（左右各一发）。弹/音效**用本作原生**（`anm 5` / `mode 7` / sfx 首 0 次 −1）。"""
    return [dict(fire_rate=MARISA_MAIN_RATE, start_delay=MARISA_MAIN_DELAY, damage=dmg,
                 spawn_dx=dx, spawn_dy=MARISA_MAIN_DY,
                 hit_w=MARISA_MAIN_BOX[0], hit_h=MARISA_MAIN_BOX[1],
                 angle=MARISA_MAIN_ANGLE, speed=MARISA_MAIN_SPEED,
                 opt_slot=0, mode=MARISA_MAIN_MODE, anm=MARISA_MAIN_ANM,
                 sfx=(0 if dx < 0 else -1), func_on_init=0)
            for dx in (-MARISA_MAIN_DX, MARISA_MAIN_DX)]


def marisa_dps_table() -> dict:
    """按**生成出来的行**复算 `[元素][档] = (主炮/帧, 子机/帧)` —— 对账用（与一手表比）。"""
    opt_specs, main_specs, opt_map, main_map = _marisa_layout()
    out = {}
    for elem in MARISA_ELEMS:
        out[elem] = []
        for lv in range(MARISA_LEVELS):
            main = sum(r["damage"] / r["fire_rate"] for r in _marisa_main_rows(MARISA_MAIN_DMG[elem][lv]))
            sub = 0.0
            for c in range(MARISA_FORMATION_CELLS):
                idx = opt_map[MARISA_ELEMS.index(elem)][lv][c]
                if idx < 0:
                    continue
                rows = opt_specs[idx - MARISA_FORMATION_SET_BASE][1]
                sub += sum(dmg / rate for rate, dmg, _ang in rows)
            out[elem].append((round(main, 6), round(sub, 6)))
    return out


def marisa_formation_dps(elem: str = "FIRE") -> float:
    """满档（档 4）子机层的每帧输出（一手 TH11 表；保留这个名字给打印/断言用）。"""
    return MARISA_TH11_DPS[elem][MARISA_LEVELS - 1][1]


def _marisa_formation_sets(anm_ids: dict) -> list:
    """→ 31 组：23 个子机组（`MARISA_OPT_*`）+ 8 个主炮组（`MARISA_MAIN_*`），顺序 = 槽号顺序。"""
    opt_specs, main_specs, _om, _mm = _marisa_layout()
    out = []
    for i, (_key, rows) in enumerate(opt_specs):
        rs = [_marisa_opt_row(_key[0], rate, dmg, ang) for rate, dmg, ang in rows]
        out.append((f"MARISA_OPT_{i:02d}",
                    b"".join(pack_shooter(r, anm_ids) for r in rs) + TERMINATOR))
    for i, (dmg, _elem) in enumerate(main_specs):
        rs = _marisa_main_rows(dmg)
        out.append((f"MARISA_MAIN_{i:02d}",
                    b"".join(pack_shooter(r, anm_ids) for r in rs) + TERMINATOR))
    return out


def marisa_layout_maps() -> tuple:
    """给 `write_ids_h` 用的 `(opt_map, main_map)`（绝对槽号 / −1）。"""
    _o, _m, opt_map, main_map = _marisa_layout()
    return opt_map, main_map


def _retail_set_blob(data: bytes, off: int) -> bytes:
    """按文件相对偏移 off 拷一段**零售** shooterset 原始字节（含终止符）⇒ 用零售基址。"""
    p = DATA_BASE_RETAIL + off
    while struct.unpack_from("<i", data, p)[0] != -1:
        p += STRIDE
        if p > len(data):
            raise SystemExit("原生 shooterset 没有终止符")
    return data[DATA_BASE_RETAIL + off:p + 4]


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
            offs = offsets(data, N_SLOTS_RETAIL)
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


def check_bullet_scripts_have_layer(anm_ids: dict) -> None:
    """★ 2026-09-16w **硬门**：凡是 shooter 行的 `+0x22`（anm 脚本号）指向的脚本，正文**必须写
    `layer(12..19)`**；不写就**直接停构建**。

    ## 为什么这条门是必须的（一手，出处 `native/engine.h` 的 `CE_ANM_VM_LAYER`）
    · 弹的 VM 进的是**世界列表**，绘制时按 **`vm+0x18`（层）**分桶：`AnmManager__render_layer`
      `0x488260` 只在「本趟桶号 == `vm+0x18`」时才画（`0x4882a1` / `0x4882b0`）。
    · 游玩区的桶 = **层 12–19**；**层 0–2 = 3D 背景那一趟**（相机 2，在游玩区之前）。
    · **没人替弹设层**：卡子机弹的 VM 走 `AnmManager__instantiate` `0x407420`（**两参、无层参数**，
      而且显式保留 `vm+0x18`），空闲池是 `memset` 0 ⇒ 不写 `layer()` 的弹**层 = 0**
      ⇒ **画在背景那一趟 = 弹幕区里一颗都看不见**（弹对象、伤害源都正常，只是不画）。
      对照：**子机本体**那条路由引擎强设层 14（`Player__allocate_option` `0x40a8bc`），
      所以子机脚本可以不写 —— 「子机不写、弹也不用写」这个推广是**错的**。
    · 写层的唯一入口 = 脚本的 `layer()`：opcode 304 → handler `0x47b67b` → `AnmManager__set_layer`
      `0x4066b0`（`mov [ecx+0x18],edx`）。
    · TH11 的引擎**自己传层**（`th11:0x4341e3 push 0x16` → `th11:0x455a00`）⇒ 从 TH11 移植弹脚本时
      这条信息会**静默丢掉** —— 本门就是为这一档踩过的坑立的（用户实跑：「魔理沙子机没有发弹」，
      根因 = 288..292 五支都没有 `layer()`）。

    ★ 2026-09-16zb：本门只查**卡侧 tick 的**弹（子机组）。111 的**主炮组**（`MARISA_MAIN_*`）豁免 ——
    它的 `+0x22` 写的是**本作原生 `pl0X.anm` 的脚本 5**（用户口径「主炮别换弹幕了」），
    而那一组由**原生行循环**发（`on_shot_set_index` 覆盖，`th18:0x45ebfb`），
    原生的自机弹一直是看得见的 ⇒ 不适用本门（本门是为"卡自己 tick 的弹"立的）。
    """
    scripts_dir = HERE.parent / "ability" / "scripts"
    bad = []
    seen = set()
    for name, blob in (*_common_sets(anm_ids), *_marisa_formation_sets(anm_ids)):
        if blob is None or name.startswith("MARISA_MAIN_"):
            continue                                    # 主炮组：原生 pl0X.anm 脚本，见 docstring
        for off in range(0, len(blob) - STRIDE + 1, STRIDE):
            rate = struct.unpack_from("<b", blob, off)[0]
            if rate < 0:
                break                                   # 终止符
            anm = struct.unpack_from("<h", blob, off + 0x22)[0]
            if anm in seen or anm <= 0:
                continue
            seen.add(anm)
            hit = sorted(scripts_dir.glob(f"{anm}_*.anm.txt"))
            if not hit:
                bad.append(f"脚本 {anm}：{scripts_dir} 下找不到 {anm}_*.anm.txt")
                continue
            body = hit[0].read_text(encoding="utf-8")
            m = re.search(r"^\s*layer\(\s*(\d+)\s*\)", body, re.M)
            if not m:
                bad.append(f"{hit[0].name}：正文没有 `layer(...)` ⇒ 弹的 VM 层 = 0 = 画在 3D 背景那一趟"
                           f"（弹幕区里一颗都看不见；见本函数注释）")
            elif not 12 <= int(m.group(1)) <= 19:
                bad.append(f"{hit[0].name}：`layer({m.group(1)})` 不在游玩区桶 12..19 里")
    if bad:
        raise SystemExit("★ 子弹脚本的层自检失败（shooter 行 +0x22 指向的脚本必须自带 layer(12..19)）：\n  "
                         + "\n  ".join(bad))


# ---- 生成：逐角色 × 逐火力档的妖侧组（P4 · th08 #5；数据表与一手的全文见上面那一大段注释）----
# 名字 = `YOUKAI_<角色后缀><档>`：后缀 R/M/S 分别对应 **TH08 的角色 0/1/2**
# （`ply00as`=霊夢・八雲紫 / `ply01as`=魔理沙・アリス / `ply02as`=咲夜・レミリア）。
# 生成序列里的**下标即槽号**（`N_RETAIL + 序号`）⇒ 四个文件必须产出**同长度同名字**的序列，
# 只有**行内容**按文件不同（照 `_hyper_sets` 的先例：117 的 WOLF/EAGLE 也是逐文件取内容）。
# 卡侧按 (角色, 火力档) 选名字（`cards/youkai.c` 的 `yk_shot_set_for()`），
# 顺序被 `_Static_assert(CE_SHT_SET_YOUKAI_M0 == CE_SHT_SET_YOUKAI_R0 + 5)` 钉死。
YOUKAI_TIER_FILES = {                    # TH18 文件 → TH08 的哪一份（None = 本批没有一手数据）
    "pl00.sht": "R",
    "pl01.sht": "M",
    "pl02.sht": "S",
    "pl03.sht": None,                    # 早苗的妖侧是 TH16 射命丸文 ⇒ 回落 `REPLACE(YOUKAI, 0x16)`
}
YOUKAI_TIER_SUFFIXES = ("R", "M", "S")
YOUKAI_TIER_TABLES = {"R": YOUKAI_TIERS_R, "M": YOUKAI_TIERS_M, "S": YOUKAI_TIERS_S}


def _youkai_tier_sets(target_file: str, anm_ids: dict) -> list:
    """该文件的 15 组（3 角色 × 5 档）。pl03 上全是**空组**（只有终止符）⇒
    卡侧 `yk_shotset_usable()` 读到 −1 ⇒ 回落到 `CE_SHT_SET_YOUKAI`（0x16）。"""
    suf = YOUKAI_TIER_FILES.get(target_file)
    out = []
    for k in YOUKAI_TIER_SUFFIXES:
        tiers = YOUKAI_TIER_TABLES[k] if k == suf else None
        orig = YOUKAI_TIER_ORIGINS[k] if k == suf else None
        for t in range(5):
            rows = (tiers[t] if tiers is not None else [])
            if orig is not None:
                # ★ 2026-09-22（P7）：逐行套上 TH08 的 `+0x20`。行数与烘进来的原点表**必须相等**
                #   —— 不等就说明行表或原点表被单独改过（那是静默错弹），当场停构建。
                if len(rows) != len(orig[t]):
                    raise SystemExit(f"YOUKAI_TIERS_{k} tier{t}: {len(rows)} 行 与烘焙的 origin 表 "
                                     f"{len(orig[t])} 项不等（TH08 ply{suf}as.sht 的实测行数）")
                rows = [dict(r, opt_slot=o) for r, o in zip(rows, orig[t])]
                for o in orig[t]:
                    if not 0 <= o <= 4:
                        raise SystemExit(f"YOUKAI_TIERS_{k} tier{t}: origin {o} 越界（TH08 只会有 0..4）")
            out.append((f"YOUKAI_{k}{t}",
                        b"".join(pack_shooter(r, anm_ids) for r in rows) + TERMINATOR))
    return out


def sets_for_file(target_file: str, anm_ids: dict) -> list:
    # ★ **顺序即槽号**（`N_RETAIL + 序号`）⇒ 111 的五组必须排在最后：插在中间会让
    #   117 的 WOLF/EAGLE 十个槽整体移位（`animal_core.h` 的槽号是写死的 ⇒ 静默错弹）。
    # ★ 2026-09-15（§1w 第 5 步落地）：`0x28` = **113 的灵界主炮组**，正常追加（带行数据）。
    #   名字取 `TRANCE` ⇒ `write_ids_h` 产出 `CE_SHT_SET_TRANCE`（卡侧只认这一个常量）。
    return (_common_sets(anm_ids) + _hyper_sets(target_file, anm_ids)
            + [("TRANCE", trance_blob(target_file, anm_ids))]
            + _marisa_formation_sets(anm_ids)
            # ★ 2026-09-22：妖侧的 15 组**排在最末**（下标 = 槽号 ⇒ 插在中间会把 111 的
            #   `MARISA_OPT_*` 与 117 的 WOLF/EAGLE 整体移位，那是静默错弹）。
            + _youkai_tier_sets(target_file, anm_ids))


def _alloc_names(sets: list) -> list:
    """生成序列里**真的分配了槽**的名字（洞不算）。"""
    return [nm for nm, blob in sets if blob is not None]


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
            "CE_PL_ANM_SCRIPT_PL03_SNAKE": PL03_SCRIPT_SNAKE,
            # ★ 卡 113 灵界弹（TH13 灵界组的三支主弹脚本）
            "CE_PL_ANM_SCRIPT_PL00_TRANCE": PL00_SCRIPT_TRANCE,
            "CE_PL_ANM_SCRIPT_PL01_TRANCE": PL01_SCRIPT_TRANCE,
            "CE_PL_ANM_SCRIPT_PL03_TRANCE": PL03_SCRIPT_TRANCE}
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


def offsets(data: bytes, n: int = N_SLOTS) -> list:
    return list(struct.unpack_from(f"<{n}I", data, OFF_ARRAY))


def walk_set(data: bytes, off: int, base: int = DATA_BASE) -> int:
    """→ 该 shooterset 的字节长度（含 4 字节终止符）。`base` 按文件类型给（零售 / 产物不同）。"""
    p = base + off
    while struct.unpack_from("<i", data, p)[0] != -1:
        p += STRIDE
        if p > len(data):
            raise SystemExit("shooterset 没有终止符，文件坏了")
    return p + 4 - (base + off)


def check_shape(name: str, data: bytes, n_slots: int, base: int):
    """头部计数 / 第 0 项 / set 0 的 func 字段。零售与产物分别调用（格数与基址都不同）。"""
    n = struct.unpack_from("<H", data, 0x02)[0]
    if n != n_slots:
        raise SystemExit(f"{name}: 头部 +0x02 = {n}，期望 {n_slots}")
    offs = offsets(data, n_slots)
    if offs[0] != 0:
        raise SystemExit(f"{name}: 第 0 项偏移不是 0")
    # set 0 的 func 字段必须全 0（空位重复解析靠它幂等）
    p = base
    while struct.unpack_from("<i", data, p)[0] != -1:
        if any(struct.unpack_from("<4i", data, p + 0x2C)):
            raise SystemExit(f"{name}: shooterset 0 的 func 字段不全为 0 —— 空位会被二次解析成野指针")
        p += STRIDE
    return offs


def build_one(name: str, retail: bytes, sets: list, repl: list) -> bytes:
    offs = check_shape(name, retail, N_SLOTS_RETAIL, DATA_BASE_RETAIL)
    used = sum(1 for i, o in enumerate(offs) if i == 0 or o != 0)
    if used != N_RETAIL:
        raise SystemExit(f"{name}: 原生用了 {used} 项，期望 {N_RETAIL}")
    if N_RETAIL + len(sets) > N_SLOTS:
        raise SystemExit(f"偏移数组只有 {N_SLOTS} 项，原生占 {N_RETAIL}，放不下 {len(sets)} 组")

    # ---- 重排（AUDIT §CQ）：头 + 96 格数组 + 数据区（数据整体后移 0xE0 = 新长的 56 格 × 4）----
    out = bytearray(retail[:OFF_ARRAY])                       # 头 [0,0xe0) 原样
    out += bytes(N_SLOTS_RESERVED * 4)                        # 数组区 [0xe0,0x4e0)：0x28 起先留 0
    struct.pack_into("<H", out, 0x02, N_SLOTS)                # 计数：零售 40 → 产物 96（预留的不计）
    struct.pack_into(f"<{N_SLOTS_RETAIL}I", out, OFF_ARRAY, *offs)   # 零售 40 项的**值一个不改**
    out += retail[DATA_BASE_RETAIL:]                          # 数据区：偏移是相对的 ⇒ 值不用改
    # ⚠ 预留空槽（0x28–0xff）的值必须保持 0：它们在计数内时会被重定位成「set 0」的别名（幂等），
    #   计数外时原样留着 0（谁要用它，必须同时把文件头 `+0x02` 抬到它之上 —— 只改文件）。

    for k, (_, blob) in enumerate(sets):
        if blob is None:                       # 洞：偏移项保持 0（未分配，读者拿到 set 0 别名）
            continue
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
    offs_r = offsets(retail, N_SLOTS_RETAIL)
    # 数组区按**预留**格数读（计数外的预留槽也要核「必须为 0」）
    offs_b = offsets(built, N_SLOTS_RESERVED)
    check_shape(name, built, N_SLOTS, DATA_BASE)
    # ★ 不变式 6/8（扩容新增）：数组必须**正好**顶到数据区起点
    if OFF_ARRAY + N_SLOTS_RESERVED * 4 != DATA_BASE:
        raise SystemExit(f"{name}: 预留数组末尾 {OFF_ARRAY + N_SLOTS_RESERVED * 4:#x} "
                         f"!= 数据区起点 {DATA_BASE:#x}")
    if DATA_BASE != DATA_BASE_RETAIL + (N_SLOTS_RESERVED - N_SLOTS_RETAIL) * 4:
        raise SystemExit(f"{name}: 数据区起点 {DATA_BASE:#x} 与数组区长大不同步")
    n_new = N_SLOTS_RESERVED - N_SLOTS_RETAIL
    # ★ 2026-09-15（111 五组落地）：原判据是「零售 40 格之后的**全部**新增格在追加前必须是 0」，
    #   它隐含了「本 mod 恰好用满 0x17–0x27」这个当时的事实（`APPEND` 17 组 ⇒
    #   `N_RETAIL + len(sets) - 1 == 0x27`）⇒ 一旦按 NEXT.md §1w 第 5/6 步把新卡放进
    #   `0x28` 起的空位，`0x17`–`0x27` 里就有**我们自己的**数据，那条断言必然假失败
    #   （111 的五组 0x28–0x2c 就是这样撞上的）。改成**按实际 allocated 数量**判：
    #   零售 40 格之后、到已分配的最后一项末尾为止都可以有数据，**之后的预留格必须全 0**。
    alloc_last = N_RETAIL + len(sets)          # 已分配的最后一格（开区间）
    if alloc_last > N_SLOTS_RESERVED:
        raise SystemExit(f"{name}: 分配的 {alloc_last} 格超过数组区容量 {N_SLOTS_RESERVED}")
    untouched = OFF_ARRAY + alloc_last * 4
    if built[untouched:OFF_ARRAY + N_SLOTS_RESERVED * 4] != bytes((n_new - (alloc_last - N_SLOTS_RETAIL)) * 4):
        raise SystemExit(f"{name}: 未分配的预留格（{alloc_last:#x}..{N_SLOTS_RESERVED - 1:#x}）不是 0")
    retail_data_end = len(retail) - DATA_BASE_RETAIL    # 追加区起点（相对基址，两侧同值）
    for i in range(N_RETAIL):
        n = walk_set(retail, offs_r[i], DATA_BASE_RETAIL)
        # 同一条记录的两侧位置：零售在旧基址下、产物在新基址下（记录后移 0xE0，偏移值不变）
        a, b = DATA_BASE_RETAIL + offs_r[i], DATA_BASE + offs_r[i]
        if i in repl_by_idx:
            # 被覆盖的槽：偏移被改指追加区，但**原生原记录必须原地原样**（死数据，不动）
            if retail[a:a + n] != built[b:b + n]:
                raise SystemExit(f"{name}: 覆盖槽 {i:#x} 把原生原记录的字节改了")
            continue
        if offs_b[i] != offs_r[i]:
            raise SystemExit(f"{name}: 第 {i:#x} 项偏移被动过（数据区整体后移 ⇒ 值应当不变）")
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
        if blob is None:                        # 洞（例：0x28 留给 113）：偏移项必须是 0
            if offs_b[idx] != 0:
                raise SystemExit(f"{name}: 【洞】{nm}（第 {idx:#x} 项）的偏移不是 0"
                                 f"（={offs_b[idx]:#x}）—— 洞必须**没分配**")
            continue
        o = offs_b[idx]
        if o == 0:
            raise SystemExit(f"{name}: 第 {idx:#x} 项没写偏移")
        if built[DATA_BASE + o:DATA_BASE + o + len(blob)] != blob:
            raise SystemExit(f"{name}: {nm} 的数据对不上")
        if walk_set(built, o) != len(blob):
            raise SystemExit(f"{name}: {nm} 的终止符位置不对")
    for i in range(N_RETAIL + len(sets), N_SLOTS_RESERVED):
        if offs_b[i] != 0:
            raise SystemExit(f"{name}: 第 {i:#x} 项本该留空（预留区里没分配的槽必须为 0）")

def _c_nested(obj) -> str:
    """把嵌套 list 打成 C 初始化器（`{ { 1, 2 }, … }`）—— `write_ids_h` 里给映射表用。"""
    if isinstance(obj, (list, tuple)):
        return "{ " + ", ".join(_c_nested(x) for x in obj) + " }"
    return str(obj)


def write_ids_h(sets: list, repl: list):
    # ★ 2026-09-15 修：`first_free` / `last_alloc` 原来在本函数里**根本没定义**
    #   （上一轮把它们从某处删掉后没补 ⇒ `write_ids_h` 一跑就 NameError，`make sht` 直接停）。
    #   现在**由生成序列算**（别再手写数字）：
    #     · `last_alloc` = 生成序列的最后一格（含洞；洞也算「序列里占了一格」）；
    #     · `first_free` = `last_alloc + 1`（下一个真正没人碰的槽）。
    #   ⚠ 洞（0x28）**不算**「本 mod 已占」⇒ 下面 CE_SHT_FREE_FIRST 的注释也改了口径。
    last_alloc = N_RETAIL + len(sets) - 1
    first_free = last_alloc + 1
    opt_map, main_map = marisa_layout_maps()      # ★ 111 的映射表（与组的顺序同源 ⇒ 不会错位）
    names_all = [nm for nm, _ in sets]
    form_first = N_RETAIL + names_all.index("MARISA_OPT_00")
    n_opt = len([n for n in names_all if n.startswith("MARISA_OPT_")])
    n_main = len([n for n in names_all if n.startswith("MARISA_MAIN_")])
    form_last = form_first + n_opt + n_main - 1
    assert form_first == MARISA_FORMATION_SET_BASE, "write_ids_h 里算出来的 111 起点与常量不符"
    lines = ["/* 由 assets/sht/append_shooterset.py 生成 —— 追加进 pl0X.sht 的 shooterset 索引。别手改。 */",
             "#pragma once", ""]
    for k, (nm, blob) in enumerate(sets):
        if blob is None:
            continue
        lines.append(f"#define CE_SHT_SET_{nm:<24} {N_RETAIL + k:#04x}   "
                     f"/* 追加，{len(blob) // STRIDE} shooter */")
    for idx, nm, blob in repl:
        lines.append(f"#define CE_SHT_SET_{nm:<24} {idx:#04x}   "
                     f"/* ★ 覆盖原生空槽，{len(blob) // STRIDE} shooter */")
    lines += [
        "",
        f"/* ---- 组数扩容：宣布可用 {N_SLOTS} 格 / 数组区预留 {N_SLOTS_RESERVED} 格"
        "（2026-09-14；AUDIT.md §CQ、NEXT.md §1w）----",
        f" * 零售影像 = 头 [0,{OFF_ARRAY:#x}) + **{N_SLOTS_RETAIL} 格**偏移数组 [{OFF_ARRAY:#x},{DATA_BASE_RETAIL:#x})"
        f" + 数据区 {DATA_BASE_RETAIL:#x} 起。",
        " * 零售的 `0x00`–`0x16` 原生、`0x17`–`0x27`（17 格）本 mod 用满 ⇒ 40/40、余量 0。",
        f" * **扩容写法**：数组区原地长到 **{N_SLOTS_RESERVED} 格（预留）**"
        f"（[{OFF_ARRAY:#x},{DATA_BASE:#x})），数据区整体后移",
        f" *   {DATA_BASE - DATA_BASE_RETAIL:#x} 字节（= 预留长大 {N_SLOTS_RESERVED - N_SLOTS_RETAIL} 格 × 4）"
        f"⇒ 数据区起点 {DATA_BASE_RETAIL:#x} → **{DATA_BASE:#x}**。",
        f" *   宣布可用 **{N_SLOTS} 格**（= 影像头 `+0x02`）；要用 `{N_SLOTS:#x}` 起的预留槽，"
        f"把 `CE_SHT_SLOTS` 抬上去即可",
        f" *   （**只改文件**：计数是纯数据，加载器每轮现读）⇒ 从 {N_SLOTS} 到 {N_SLOTS_RESERVED} 之间"
        f"再加格子**不用再发补丁**。",
        " *   · 偏移项是**相对**数据区的 ⇒ 零售 40 项的值与每条记录的字节**一个都不用改**；",
        f" *   · exe 侧唯一改动 = `th18:0x45a830` 的 `lea eax,[edx+{DATA_BASE_RETAIL:#x}]` → `[edx+{DATA_BASE:#x}]`"
        "（`native/sites.py` 的",
        " *     `sht_dbase_45a830`，6 字节等长，只换立即数）；",
        f" *   · **14 个读者的位移 `+{OFF_ARRAY:#x}` 一个字节都不用动**（数组起点没变）—— 这与 §1w 最早那版",
        " *     「把数组搬文件尾、改 14 处引用」是两条路，本实现走的是**改动面最小**的这条"
        "（折叠位移读者 `0x410609` 也不用改）；",
        " *   · 影像内存 = `malloc(GetFileSize())` ⇒ 文件长大安全（一手：`0x402140` 一带 `0x4ad088`/`0x491a34`）。",
        " * ★ 恒等式（生成器每次回读校验）：`"
        + f"{OFF_ARRAY:#x} + {N_SLOTS_RESERVED}×4 == {DATA_BASE:#x}" + "`",
        f" *   ⇒ 第 `i` 项永远落在 `[0xe0,{DATA_BASE:#x})` 内，读者（位移 0xe0）永远踩不到数据区。",
        " * ------------------------------------------------------------------------------------ */",
        f"#define CE_SHT_ARRAY_OFF           {OFF_ARRAY:#06x}   /* 偏移数组起点 = 14 个读者指令里的位移（未改）*/",
        f"#define CE_SHT_SLOTS               {N_SLOTS}          /* 宣布可用格数 = 影像头 +0x02 = 加载器解析循环上界 */",
        f"#define CE_SHT_SLOTS_RESERVED      {N_SLOTS_RESERVED}         /* 数组区容量（预留）：用 `{N_SLOTS:#x}`+ 的槽时抬 CE_SHT_SLOTS，**exe 不用动** */",
        f"#define CE_SHT_DATA_BASE           {DATA_BASE:#06x}   /* 数据区起点（原 {DATA_BASE_RETAIL:#06x}）= `th18:0x45a830` 的立即数 */",
        f"#define CE_SHT_FREE_FIRST          {first_free:#04x}   /* 第一个空槽（0x00–{last_alloc:#04x} 已分配）*/",
        f"#define CE_SHT_FREE_LAST           {N_SLOTS - 1:#04x}   /* 计数内最后一个空槽 */",
        f"#define CE_SHT_FREE_LAST_RESERVED  {N_SLOTS_RESERVED - 1:#04x}   /* 预留区最后一格（抬计数后可用，不用改 exe）*/",
        "/* 分配表（**由生成序列算出来，别再手写数字**）：",
        f" *   0x{RESERVED_113_SLOT:02x}            113 灵界主炮（1 格，四角色共用）= `CE_SHT_SET_TRANCE`（2026-09-15 §1w 第 5 步落地，正常追加组）",
        f" *   0x{form_first:02x}–0x{form_first + n_opt - 1:02x}      111 魔理沙B **子机组**（{n_opt} 格，"
        "TH11 `pl01b.sht` 逐字段复刻 ⇒ `MARISA_OPT_*` / 卡侧按「元素×档×格」选组）",
        f" *   0x{form_first + n_opt:02x}–0x{form_last:02x}      111 魔理沙B **主炮组**（{n_main} 格 = {n_main} 个不同的单发伤害；"
        "`MARISA_MAIN_*`，由卡侧 `on_shot_set_index` 整组覆盖）",
        f" *   0x{last_alloc + 1:02x}–…         108 使魔 / 106·107·109·110·112·114·115 各自子系统 / 58–80 那批的备用",
        " * ------------------------------------------------------------------------------------ */",
        "",
        "/* ---- 卡 113「灵界」的主炮弹组（TH13 灵界 set10 复刻）------------------------------------",
        f" * `CE_SHT_SET_TRANCE` = **{RESERVED_113_SLOT:#04x}** = 113 的**永久专属槽**（NEXT.md §1w 第 5 步，2026-09-15 落地）——",
        " * 它是一格**正常的追加组**：行数据 = TH13 `pl0Xa.sht` 的灵界 set10 行（每文件：灵梦 2 / 魔理沙 4 /",
        " * 咲夜 2 row（★ 2026-09-16 参与：数值照抄灵梦、贴图用她的刀弹 `anm=7`）/ 早苗 2 row），由偏移项指着，**引擎自己解析**。",
        " * ★ 历史：在这一格拿到之前，113 借的是原生槽 `0x0e`（SANAE_OP 的子机组）并把偏移项**临时**改指",
        " *   文件尾的孤儿段（卡侧还要记 4 格账本、退出时还原）。两个毛病都在收尾时消失了：",
        " *   ① 卡侧不再写任何 sht 内存（`on_shot_set_index` 只返回槽号）；",
        " *   ② 没有「孤儿指针算到影像之外」这一整类失败模式（老 sht + 新 DLL 下会越界读崩，",
        " *      见 AUDIT §CV；现在最坏只是读到未分配的 set 0 别名 ⇒ 主炮没变，不崩）。",
        f" * ★ 为什么之前不能直接占一格：零售数组只有 {N_SLOTS_RETAIL} 格（原生 23 + 本 mod 17，全满）⇒",
        f" *   扩容到 {N_SLOTS} 格（数组区预留 {N_SLOTS_RESERVED} 格）后 `0x28` 起才有空位（AUDIT §CQ）。",
        " * ------------------------------------------------------------------------------------ */",
        f"#define CE_SHT_TRANCE_DBASE        {DATA_BASE:#06x}   /* 解析器的重定位基址（th18:0x45a830 的 imm）*/",
        "",
        "/* ---- 111 魔理沙B：TH11 `pl01b.sht` 的**逐字段复刻映射**（生成物；卡侧 `cards/signal.c` 直接用）----",
        " * `CE_SHT_MARISA_OPT_INIT`  = `[元素 0..4][档 0..4][格 0..3]` ⇒ **子机组号**（`-1` = 该档没有这一格）",
        " * `CE_SHT_MARISA_MAIN_INIT` = `[元素 0..4][档 0..4]`      ⇒ **主炮组号**（恒 ≥ 0）",
        " * 元素顺序 = 火/水/木/金/土（= TH11 的 formation 0..4）；档 0..4（档 0 = 零子机）。",
        " * 一手、组的形状（为什么 23+8 而不是 25）、以及**主炮只抄数值不换弹**的用户口径：",
        " * `assets/sht/append_shooterset.py` 的 `MARISA_*` 那一段（唯一出处）。",
        " * ⚠ 主炮组由 `on_shot_set_index` **整组覆盖**（`th18:0x45ebfb`）⇒ 覆盖生效时引擎只发这 2 行主炮，",
        " *   原生子机行（`slot ≥ 1`）本来就没有活子机可挂（`Player__shoot_one_bullet` 找不到就 `return 0`）。",
        " * ------------------------------------------------------------------------------------ */",
        f"#define CE_SHT_MARISA_SET_BASE     {MARISA_FORMATION_SET_BASE:#04x}   /* 111 的第一格 */",
        f"#define CE_SHT_MARISA_LEVELS       {MARISA_LEVELS}          /* TH11 的档数（档 0..4）*/",
        f"#define CE_SHT_MARISA_CELLS        {MARISA_FORMATION_CELLS}          /* 满档子机格数 */",
        f"#define CE_SHT_MARISA_OPT_INIT     {_c_nested(opt_map)}",
        f"#define CE_SHT_MARISA_MAIN_INIT    {_c_nested(main_map)}",
    ]
    IDS_H.write_text("\n".join(lines) + "\n", encoding="utf-8")


def check_new_slot_placement(canonical: list):
    """★ 防漂移（2026-09-15，本轮踩过一次）：**已有的槽号一个都不许动**。

    槽号 = `N_RETAIL + 在生成序列里的序号`，所以「往 `APPEND` 中间插一项」会让它后面**所有**的
    追加组整体移位 —— 117 的 WOLF/EAGLE 十个槽就是这么被顶掉的（`animal_core.h` 的
    `ANIMAL_SHOT_SET_*` 写死旧号 ⇒ 弹照发、伤害表认错组 = 静默错弹）。
    这里把两个**已经发出去**的锚点钉住，并把 111 的新组钉在 NEXT.md §1w 第 6 步指定的 `0x29` 起
    （`0x28` = §1w 第 5 步留给 113 的永久槽 ⇒ 生成序列里用**洞**占位）。"""
    names = [nm for nm, _ in canonical]
    anchors = (("WOLF_0", 0x1e), ("WOLF_4", 0x22), ("EAGLE_0", 0x23), ("EAGLE_4", 0x27))
    for nm, want in anchors:
        if nm not in names:
            raise SystemExit(f"shooterset 生成序列里没有 {nm}（117 的锚点）—— 它被改名/删掉了")
        got = N_RETAIL + names.index(nm)
        if got != want:
            raise SystemExit(f"★ {nm} 的槽号从 {want:#04x} 变成 {got:#04x}："
                             f"有人在生成序列**中间**插了组（往 APPEND 里插会把后面的全部顶掉）。"
                             f"新组请接在 `sets_for_file()` 的最后。")
    # 113 的槽必须落在约定的一格、且**带数据**（§1w 第 5 步后它是一格正常追加组）
    if names[RESERVED_113_SLOT - N_RETAIL] != "TRANCE":
        raise SystemExit(f"★ 0x{RESERVED_113_SLOT:02x} 不是 113 的灵界组"
                         f"（生成序列第 {RESERVED_113_SLOT - N_RETAIL} 项 = "
                         f"{names[RESERVED_113_SLOT - N_RETAIL]}）—— §1w 第 5 步把它钉在这一格")
    if canonical[RESERVED_113_SLOT - N_RETAIL][1] is None:
        raise SystemExit(f"★ 0x{RESERVED_113_SLOT:02x} 的灵界组没有行数据")
    # ★ 2026-09-16zb：111 现在是 **23 子机组 + 8 主炮组 = 31 格**（TH11 pl01b 逐字段复刻）。
    #   起点仍钉在 §1w 第 6 步的 `0x29`；顺序也钉住（子机组在前、主炮组在后）——
    #   组的顺序 = 槽号，错位就会让卡侧映射表指错组（`sht_ids.h` 里那张表是同一函数算的 ⇒ 会一起错，
    #   所以这里再独立核一遍"第一格/最后一格"）。
    first = N_RETAIL + names.index("MARISA_OPT_00")
    if first != MARISA_FORMATION_SET_BASE:
        raise SystemExit(f"111 的第一组落在 {first:#04x}，约定是 {MARISA_FORMATION_SET_BASE:#04x}"
                         f"（NEXT.md §1w 第 6 步）")
    opt_specs, main_specs, opt_map, main_map = _marisa_layout()
    want = [f"MARISA_OPT_{i:02d}" for i in range(len(opt_specs))] + \
           [f"MARISA_MAIN_{i:02d}" for i in range(len(main_specs))]
    for k, nm in enumerate(want):
        if nm not in names or N_RETAIL + names.index(nm) != first + k:
            raise SystemExit(f"{nm} 不连续（期望 {first + k:#04x}）")
    last = first + len(want) - 1
    if last >= N_SLOTS:
        raise SystemExit(f"111 的 31 组用到 {last:#04x}，超出宣布可用的 {N_SLOTS:#x} 格"
                         f"（要更多格就把 CE_SHT_SLOTS 抬到预留区，见 sht_ids.h）")
    print(f"  槽位锚点：WOLF 0x1e–0x22 / EAGLE 0x23–0x27 未动；"
          f"0x{RESERVED_113_SLOT:02x} = 113 的灵界组（正常追加）；"
          f"111 = 子机 {first:#04x}–{first + len(opt_specs) - 1:#04x} + "
          f"主炮 {first + len(opt_specs):#04x}–{last:#04x}（共 {len(want)} 格）")


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
    check_new_slot_placement(canonical)
    check_bullet_scripts_have_layer(anm_ids)      # ★ 2026-09-16w：弹脚本必须自带 layer(12..19)

    if not canonical and not repl:                  # 没有要追加的：不产出（别发一份和原生一模一样的 .sht）
        if not a.verify_only:
            for name in FILES:
                (OUT / name).unlink(missing_ok=True)
            write_ids_h(canonical, repl, 0)
        print("  没有登记任何 shooterset：不产出 .sht（build/sht/ 已清空）")
        return 0

    OUT.mkdir(parents=True, exist_ok=True)
    for name in FILES:
        src = RETAIL / name
        if not src.is_file():
            raise SystemExit(f"缺原生样本 {src}（用户放进 local/，见 local/README.md）")
        retail = src.read_bytes()
        sets = sets_by_file[name]
        expect = build_one(name, retail, sets, repl)
        dst = OUT / name
        if a.verify_only:
            if not dst.is_file():
                raise SystemExit(f"缺 {dst}：先跑 make sht")
            built = dst.read_bytes()
        else:
            built = expect
            dst.write_bytes(built)
        verify(name, retail, built, sets, repl)
        print(f"  {name}: {len(retail)} → {len(built)} 字节，追加 {len(sets)} 组"
              f"（索引 {N_RETAIL:#04x}..{N_RETAIL + len(sets) - 1:#04x}）"
              f"，覆盖 {len(repl)} 个原生槽"
              f"，灵界组 {RESERVED_113_SLOT:#x}（{len(TRANCE_ROWS[name])} row）")

    if not a.verify_only:
        write_ids_h(canonical, repl)
        print(f"wrote {IDS_H.relative_to(MOD)}")
    for k, (nm, blob) in enumerate(canonical):
        if blob is None:
            print(f"  0x{N_RETAIL + k:02x}  预留洞（{nm}，偏移项 0 = 未分配）")
            continue
        print(f"  CE_SHT_SET_{nm} = {N_RETAIL + k:#04x}  ({len(blob) // STRIDE} shooter)")
    for idx, nm, blob in repl:
        print(f"  CE_SHT_SET_{nm} = {idx:#04x}  (★ 覆盖原生空槽，{len(blob) // STRIDE} shooter)")
    print(f"  CE_SHT_SET_TRANCE = {RESERVED_113_SLOT:#04x}  "
          f"(★ 永久专属槽；"
          f"灵梦 {len(TRANCE_ROWS['pl00.sht'])} / 魔理沙 {len(TRANCE_ROWS['pl01.sht'])} / "
          f"咲夜 {len(TRANCE_ROWS['pl02.sht'])} / 早苗 {len(TRANCE_ROWS['pl03.sht'])} row)")
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
           ("早苗 EAGLE(新)", _dps(_sanae_eagle_rows()), "大青蛙·紫（★AL65 rate12 dmg96）", SANAE_EAGLE_DPS_TARGET),
           # ★ 卡 113：TH13 灵界组的一手 DPS（Σdmg/rate，与 `engine/spirit/th13/05` §2 的简算同法）
           ("灵梦 灵界 113", _dps(TRANCE_ROWS["pl00.sht"]), "TH13 set10 一手（2×22/2）", 22.0),
           ("魔理沙 灵界 113", _dps(TRANCE_ROWS["pl01.sht"]), "TH13 set10 一手（(22+22+11+11)/2）", 33.0),
           ("早苗 灵界 113", _dps(TRANCE_ROWS["pl03.sht"]), "TH13 set10 一手（2×22/2）", 22.0),
           ("咲夜 灵界 113", _dps(TRANCE_ROWS["pl02.sht"]),
            "灵梦数值 + 咲夜刀弹（用户 2026-09-16 定案；anm=7，非 TH13 一手）", 22.0),
           # ★★ 2026-09-16zb（111 魔理沙B）：**逐字段照抄 TH11 `pl01b.sht`** —— 硬断言在下面，
           #   这里只打印满档的两个极端（火最弱 / 金最强，与一手一致）。
           ("111 火 满档 主炮+子机", MARISA_TH11_DPS["FIRE"][4][0] + MARISA_TH11_DPS["FIRE"][4][1],
            "TH11 pl01b 一手：主炮 8.00 + 子机 6.67（子机/主炮 = 0.83×）", None),
           ("111 金 满档 主炮+子机", MARISA_TH11_DPS["METAL"][4][0] + MARISA_TH11_DPS["METAL"][4][1],
            "TH11 pl01b 一手：主炮 5.33 + 子机 32.00（子机/主炮 = 6.00×）", None)]
    # ★★ 2026-09-16zb：111 现在是**逐字段复刻**（不再"归一"）⇒ 断言改成**逐 (元素, 档) 对账**：
    #   把生成出来的行复算一遍，必须逐值等于从 TH11 `pl01b.sht` 一手算出来的 `MARISA_TH11_DPS`。
    #   改了任何一行数值 / 率 / 角度 / 组映射而没重新对账 ⇒ 这里直接停构建（这是"照抄"的守卫）。
    _got = marisa_dps_table()
    for _e in MARISA_ELEMS:
        for _lv in range(MARISA_LEVELS):
            _wm, _ws = MARISA_TH11_DPS[_e][_lv]
            _gm, _gs = _got[_e][_lv]
            if abs(_gm - _wm) > 1e-4 or abs(_gs - _ws) > 1e-4:
                raise SystemExit(
                    f"★ 111 {MARISA_ELEM_CN[_e]} 档{_lv}：生成出来 主炮 {_gm:.6f} / 子机 {_gs:.6f}，"
                    f"与 TH11 `pl01b.sht` 一手值 主炮 {_wm:.6f} / 子机 {_ws:.6f} 不等"
                    f"（改了 `MARISA_*` 任一张表就必须重新对账；一手出处见那段注释）")
    print("  DPS 对照（每帧伤害；新卡参数在 append_shooterset.py / animal.c）:")
    for nm, v in refs:
        print(f"    {nm:<18} {v:7.2f}  (一手)")
    for nm, v, note, tgt in new:
        if tgt is None:                     # ★ 111 那两行：值就是 TH11 一手值本身（无"目标"可比）
            print(f"    {nm:<18} {v:7.2f}  ({note})")
        else:
            print(f"    {nm:<18} {v:7.2f}  ({note}；目标 {tgt:.2f}，偏差 {(v - tgt) / tgt * 100:+.1f}%)")
    print("    注（AL64c）：早苗 EAGLE 的命中路径 = hit0（默认 handler）⇒ 上表 40 是**真实**DPS；")
    print("      旧 hit4 会在命中点建一个 20 帧、每帧结算的爆炸源（+最多 24×20/发）⇒ 实际远超名义值。")
    print("    注（AL65）：早苗 WOLF 对齐**灵梦大鹫** 100/3=33.33（不是灵梦狼 63.33）；")
    print("      咲夜 EAGLE 总 30 拆成扫荡 20 + 正中正面 10。")
    print("    注（AL66）：咲夜 EAGLE 总 30→40（+1/3）——正面 10 不动，扫荡周期 6→4（20→30）。")
    print("    注（AL67）：咲夜 EAGLE 分布改成扫荡 20（周期 6、dmg20）+ 正面 20（dmg40 / 周期 2），总 40 不变。")


if __name__ == "__main__":
    sys.exit(main())
