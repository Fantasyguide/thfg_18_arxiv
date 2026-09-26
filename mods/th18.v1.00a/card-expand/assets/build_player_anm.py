#!/usr/bin/env python3
"""玩家 anm 侧：卡片 117 的兽灵弹（pl00/pl01 原生拷贝；pl02/pl03 追加，AL59–AL62）
+ 卡片 113 的 **TH13 灵界主炮弹**（pl00/pl01/pl03 追加，2026-09-13）。

    python3 build_player_anm.py                # 重建 native/build/pl0X.anm + 硬校验
    python3 build_player_anm.py --verify-only  # 只校验已有的 native/build/pl0X.anm

## 现状
- **pl00/pl01**：TH18 原生 `pl0X.anm` 已逐字节含 TH17 兽灵脚本（TH17 `anm` = TH18 `anm`+5，
  `hit5` 硬编码 script8）⇒ 兽灵那一半**原样引用**，一个字节不改（`_assert_sources` 每次回读对账）；
  ★ 113 之后这两个文件**不再是整文件拷贝**，改为「原生部分逐块一致 + 追加灵界脚本」（见下）。
- **pl02（咲夜）/ pl03（早苗）**：没有 TH17 对应自机 ⇒ 追加：
  · pl02：script30=红刀（狼）、script31=紫刀（鹫，真弹视觉）；贴图 entry12（红/紫刀）。
  · pl03：script29=大青蛙、script30=大蛇（都 scale×2）；entry13..16（青蛙/蛇/爆炸/闪光）。
- **颜色（AL62）**：不再用 `color()` 乘法近似，改成**构建期按像素**转色：
  源图 = `assets/player_art_src/<key>.png`（用户可改）；缺图自动用「零售图 + 默认色」补出来
  （见 `player_art_lib.py`）。`make_player_art.py` 负责补默认 + 切到 `D:\\...\\18\\art-src\\`；
  回流用 `assets/sync_player_art_src.sh`。**缺图不会红、不会出错图**。
- **★ 113 灵界主炮弹（TH13 复刻）**：pl00/pl01/pl03 各追加「主弹 + 每帧闪光」两支脚本 +
  一张整贴图 entry。**脚本逐条抄 TH13 的 anm 正文**（出处见 `APPEND_PLAN` 注释），
  **贴图构建期直接从 `local/th13.v1.00c/anm/...` 切**（见 `TH13_TRANCE_ENTRIES` 的说明：
  为什么不走 117 那条 `player_art_src` 中间层），1:1、零缩放、零改色。
  ⚠ 两作层号不是一个空间 ⇒ `layer()` 必须过 `LAYER_MAP` 翻译（表里有实测依据）。
  ⚠ TH13 的 `ins_433`（v8 的 `posTime2D`，极坐标目标）TH18 **有**（分发表 case `th18:0x47a730`，
  极坐标 helper `th18:0x47d890` 是 `fld θ; fsincos; fmul r`）⇒ 原样保留，不算自定。
- **★★ 108 妖侧本体（2026-09-22，owner 裁决走「路 A」）**：把 TH08 `player0X.anm` 的**妖侧本体脚本
  5..9**（pl03 走 TH16 `pl03.anm` 的 0..4 = 射命丸文）**逐条搬进 pl0X.anm**，`sprite()` 号重编号到
  本文件新追加的 entry；贴图 = `assets/ability/youkai/YOUKAI_{YUKARI,ALICE,REMILIA}_S*.png`
  （+ `assets/ability/sanae_youkai/YOUKAI_AYA_S*.png`），**逐像素等于原版切图**（本文件有回读闸）。
  为什么必须进 pl0X.anm：本体 VM 的贴图按 **VM 自己那一页的 anm** 取（`th18:0x481117`–`0x481136`
  `[vm+0x20]` → AnmLoaded → `[anm+0x11c]` sprite 表），而本体 VM 的页就是 `player+0xc` = pl0X.anm。
  一手与设计见 `engine/player/th08/03-youkai-body-and-gauge.md` §1、`AUDIT.md` §BE 详录 #50。

## 硬校验（任一失败非 0 退出、不产出文件）
目标脚本存在；sprite 全在界内；`scriptNew` 目标存在；`pl00/pl01` 兽灵脚本与 TH17 逐字节一致；
追加脚本普通飞行路径无 `delete()`；**源脚本的 sprite/scriptNew 引用与计划表逐一对应**（TH13 来源
再加「每个 `layer()` 都在 `LAYER_MAP` 里」）；**追加的灵界贴图从编出来的 anm 里回读后必须逐像素
等于 TH13 原版切图**；原生 entry/script/贴图逐块不变；写 `native/pl_anm_ids.h`。
"""
import argparse
import re
import shutil
import subprocess
import sys
from pathlib import Path

import anmlib as L

sys.path.insert(0, str(L.HERE / "player_art_src"))
import player_art_lib as PA  # noqa: E402  —— AL62 源图（assets/player_art_src/）

HERE, MOD = L.HERE, L.MOD
REPO = L.REPO
TH17_DAT = REPO / "local" / "th17.v1.00b" / "dat"
TH18_DAT = REPO / "local" / "th18.v1.00a" / "dat"
TH13_DAT = REPO / "local" / "th13.v1.00c" / "dat"
TH13_ANM = REPO / "local" / "th13.v1.00c" / "anm"   # unpack.py 解出来的原版贴图（只读）
TH08_DAT = REPO / "local" / "th08.v1.00d" / "dat"   # ★ 108：TH08 的 player0X.anm（**裸 .dat 里的文件**，不用 unpack）
TH08_ANM = REPO / "local" / "th08.v1.00d" / "anm"   # 108 的 HUD / 刻符贴图源（`unpack.py th08.v1.00d` 解出来）
TH16_DAT = REPO / "local" / "th16.v1.00a" / "dat"
OUT = L.BUILD
PLAYERS = ("pl00", "pl01", "pl02", "pl03")

# ★ 跨版本源脚本（`game=` 键）：每作的 `thanm -l` 作品号与 **anm 头版本对应的 anmmap**。
#   ⚠ anmmap 按「anm 文件头 version」选，不是按作品号（`tooling/thtk/unpack.py` 的注释）：
#   TH08 的头是 3 ⇒ `v2.anmm`；TH13/TH16/TH18 的头是 8 ⇒ `v8.anmm`。选错不报错、只会把 mnemonic
#   译成垃圾（拿 v8.anmm 读 TH11 会把 `sprite` 译成 `stop`）。
GAME_VER = {"th08": "8", "th13": "13", "th16": "16"}
GAME_ANMMAP = {"th08": "v2.anmm", "th13": "v8.anmm", "th16": "v8.anmm"}
GAME_DAT = {"th08": TH08_DAT, "th13": TH13_DAT, "th16": TH16_DAT}
# 各作源脚本里允许出现的 `layer()` 值（跨版本层号不是同一个空间 ⇒ 必须逐游戏钉死，见 LAYER_MAP）：
#   · th13：走 LAYER_MAP 翻译；
#   · th16：与 TH18 同一个层空间，本体脚本的层就是 **14**（= TH18 原生本体脚本的 `layer(14)`）⇒ 恒等；
#   · th08：本体脚本 5..9 里**一个 layer() 都没有**（层由 VM 保留，见 `_inject_body_prologue`）。
GAME_LAYERS = {"th13": None, "th16": {14}, "th08": set()}
MAPDIR = None                                        # `main()` 里由 `L.find_tools()` 填

# TH17 shooter `anm` 字段 -> 该文件里的实际脚本号（= anm+5；slot7 是 hit5 硬编码 script8）。
# pl00/pl01 的原生文件里就有这些脚本；pl02/pl03 没有，走 FALLBACK。
TH17_ANM_TO_SCRIPT = {5: 10, 6: 11, 7: 8, 8: 13, 11: 16}
# pl00/pl01：TH17 `anm` -> 原版脚本号（就是同一支兽灵脚本）。
BEAST_SCRIPTS = {"pl00": {10, 11}, "pl01": {13, 16}}
HIT_FX_SCRIPT = 8
# 会飞的主弹脚本（只有它们要求「主飞行路径无 delete」；hit_fx=8 是一次性特效，本来就会自删）。
FLYING_SCRIPTS = {"pl00": (10, 11), "pl01": (13, 16)}
# pl02（咲夜）：没有 TH17 对应自机，原生拷贝 + identity 校验（当前 sht 用原生 script5 = 红刀）。
FALLBACK_ANM_TO_SCRIPT = {5: 5, 6: 6, 7: 7, 8: 8, 11: 11}
# ★ 2026-09-13（AL59/AL61/AL62）：pl02 / pl03 追加自己的兽灵弹脚本 + 贴图 entry。
#
# 为什么必须追加：主炮路径 `PlayerBullet__create` 用 `player+0x10 = pl0X.anm` 解析 shooter 的 anm 号
# （`th18:0x45e723`），所以**真子弹**的贴图脚本必须落在 pl0X.anm 里。
#
# ★ AL62：颜色不再用 `color()` 乘法近似，改成**构建期按像素转色**（见 `player_art_lib.py`）：
#   · 早苗：大青蛙→纯紫（mono H275）、蛇→纯红（hue H0）、爆炸→单独一份纯紫（mono H275）；
#   · 咲夜：红刀=零售原样、紫刀=蓝刀 hue H275；红/蓝/紫刀都切进 `assets/player_art_src/` 供用户改。
#   这些图作为**新 entry** 嵌进 pl02.anm / pl03.anm（脚本用新 sprite 号）。
#
# 每项 = dict(macro, src, sid, new_sid, smap, k, alpha255, scale_inject[, game, sref])：
#   · smap = {源 sprite 号: 新 sprite 号}；k = scale 倍数；alpha255 = `alpha(192)`→255；
#   · scale_inject = True 时在脚本头注入 `scale(k,k)`（原脚本没有 scale 的刀脚本用）；
#   · `game="th13"` = 源脚本来自 **TH13 原版**（不是 TH18 自己的 pl0X.anm），见 LAYER_MAP。
#
# 追加进去的脚本是「会飞的主弹」，普通飞行路径不许 delete()。
APPEND_FLYING = {"pl00": (41,), "pl01": (47,), "pl02": (30, 31), "pl03": (29, 30, 32)}

# ★★ 2026-09-13（卡 113 第三阶段）：TH13 灵界主炮弹的**层号翻译表**。
#
# 为什么需要翻译：TH13 与 TH18 的 `layer()` **不是同一个数字空间**（TH14/15/16 重排过层）。
# 这不是猜的 —— 拿 TH13 与 TH18 的 anm 逐脚本对拍（把 `layer(N)`/`sprite(N)`/`offsetN`/`// 注释`
# 归一化后找**唯一**同形对），统计出的对应关系：
#
#   TH13 层 → TH18 层 | 依据（同形对数量）
#   ---------------- | ---------------------------------------------------------------
#        4 → 5      | n=10；`effect.anm` **同号**脚本逐字节同形（s11→s11 / s12→s12 /
#                   | s13→s13 / s14→s14，唯一匹配）
#    6..10 → 不变    | n=9 / 24 / 135 / 52 / 48（enemy / bullet）
#       11 → 13     | n=52；含 `front.anm` 85–99（= TH13 的弹窗配方，CLAUDE.md 记过）
#       13 → 15     | pl00/pl01 主炮的同形对给出 **[11,13] → [13,15]**
#
# ★ **语义判据比数字更硬**（这条才是我们真正依赖的）：TH13 里「灵界主弹」与「普通主炮」
#   **是同一层**（pl00 s10 vs s14、pl01 s10 vs s11 都是 11/13）；TH18 里普通主炮是 **13/15**
#   （pl00 s5/s6/s7/s8/s9/s10、pl03 s6/s8 全部 `layer(13)` + 命中段 `layer(15)`）
#   ⇒ 灵界主弹也必须落在 13/15，否则会被自机的普通弹盖住（那是明确的画错，不是风格差异）。
# ⚠ 这张表**不是纯函数**：`effect.anm` 里另有一族 4→3 的同形对（n=9），说明 ZUN 是逐脚本挑的。
#   本次只用到 4 / 11 / 13 三个键；`_assert_spec_source` 会拦下任何**没登记**的 TH13 层号，
#   逼后来者先查清对应关系再动手，而不是照抄一个 TH13 的数字。
LAYER_MAP = {4: 5, 11: 13, 13: 15}

# ★ AL62 具名旋钮：咲夜刀弹视觉放大倍数（与早苗 k=2.0 同量级；命中框在 append_shooterset.py 对应调）。
KNIFE_SCALE = 2.0

# 追加 entry：每个 entry 一张整贴图，sprites = [(sprite_id, x, y, w, h, source_key), ...]
PLAYER_ART_ENTRIES = {
    "pl02": [
        dict(idx=12, name="player/pl02/pl02knife.png", size=(128, 16), sprites=[
            (75, 0, 0, 64, 16, "sakuya_knife_red_s24_f0"),
            (76, 64, 0, 64, 16, "sakuya_knife_blue_s25_f0"),   # 缺图时兜底 → 紫
        ]),
    ],
    "pl03": [
        dict(idx=13, name="player/pl03/pl03frog_c.png", size=(32, 32), sprites=[
            (57, 0, 0, 30, 30, "sanae_frog_s34_f0"),
        ]),
        dict(idx=14, name="player/pl03/pl03snake_c.png", size=(256, 16), sprites=[
            (58, 0, 0, 64, 16, "sanae_snake_s30_f0"),
            (59, 64, 0, 64, 16, "sanae_snake_s31_f1"),
            (60, 128, 0, 64, 16, "sanae_snake_s32_f2"),
            (61, 192, 0, 64, 16, "sanae_snake_s33_f3"),
        ]),
        dict(idx=15, name="player/pl03/pl03exp_c.png", size=(512, 128), sprites=[
            (62, 0, 0, 128, 128, "sanae_explosion_s40_f0"),
            (63, 128, 0, 128, 128, "sanae_explosion_s41_f1"),
            (64, 256, 0, 128, 128, "sanae_explosion_s42_f2"),
            (65, 384, 0, 128, 128, "sanae_explosion_s43_f3"),
        ]),
        dict(idx=16, name="player/pl03/pl03flash_c.png", size=(32, 16), sprites=[
            (66, 0, 0, 16, 16, "sanae_flash_s25_f0"),
            (67, 16, 0, 16, 16, "sanae_wolf_glint_s25_f0"),
        ]),
    ],
}

# ★★ 2026-09-13（卡 113 第三阶段）：灵界主炮的贴图 entry —— **构建期直接从 TH13 原版图切**。
#
# ■ 为什么走「现切 TH13」而不是 117 的 `player_art_src/` 中间层（这是一个刻意的选择）
#   117 的兽灵弹需要**改色**（早苗青蛙/蛇→紫/红、咲夜刀→紫），所以它必须有一层
#   「用户可编辑的源图 + 缺图兜底」（`player_art_lib.py`）。113 的灵界弹相反：TH13 的图
#   就是唯一正确的源 —— **1:1、零缩放、零改色、零补边**，中间层只会多出一份可能与原版
#   漂移的副本，并且那份副本还会被 `player_art_lib` 的默认色变换碰到（我们不需要任何变换）。
#   `assets/ability/make_trance_art.py`（113 的神灵/灵界槽贴图）用的就是这条「现切」路，
#   本文件与它保持一致。
#
# ■ 每项 = dict(idx, name, size, src_dir, src_png, sprites)
#   · src_dir / src_png = TH13 解包目录 `local/th13.v1.00c/anm/<src_dir>/<src_png>`
#     （= 该作 `dat/<src_dir>.anm` 里那条 entry 的 name）；
#   · sprites = [(新 sprite 号, 画布 x, 画布 y, w, h, 源图 x, 源图 y), …]
#     —— 矩形尺寸/坐标**逐条抄 TH13 的 spec**，不缩放不补边，画布只是把它们横向铺开。
#   · ⚠ 切图用不带 mask 的 `Image.paste`（同 `make_trance_art._paste` 的注释）：带 mask 会被
#     PIL 做 alpha 合成、把半透明像素预乘成暗色，那就不是「原样切图」了。
#   · ★★ 2026-09-16b：entry 还可声明 `src_game` / `transform`（**只给咲夜用**）——
#     TH13 没有咲夜这个自机 ⇒ 她没有"灵界成品图"可切；用户定案 = 拿**她自己的 TH18 那 24 帧**、
#     施加从 TH13 三对原版图量出来的同一个色调变换（`assets/trance_form_transform.py`，
#     ❓自定，逐条见 `AUDIT.md` §DG-7）。除这一条外，所有形态 entry 都是 TH13 原版现切、零处理。

TH13_TRANCE_ENTRIES = {
    # 灵梦：TH13 `pl00.anm` entry1 `player/pl00/pl00f.png`
    #   sprite67 = (1,145,78,30) 主弹本体（78×30 是最长的那个）
    #   sprite68 = (81,145,14,14) 每帧闪光
    "pl00": [
        dict(idx=13, name="player/pl00/pl00trance.png", size=(92, 30),
             src_dir="pl00", src_png="player/pl00/pl00f.png",
             sprites=[(60, 0, 0, 78, 30, 1, 145),
                      (61, 78, 0, 14, 14, 81, 145)]),
        # ★ 2026-09-16：**灵界形态 24 帧**（TH13 `pl00.anm` **entry1 `player/pl00/pl00f.png`** 的 sprite43..66 = script5..9 引用到的
        #     全部形态帧；见 AUDIT §DG）。⚠ 源图是 **pl00f.png**（灵界专用贴图，平均亮度 ~142），
        #     不是 `pl00.png`（普通形态，~80）——两者矩形网格相同，切错图会「看着像没变」。
        dict(idx=14, name="player/pl00/pl00form.png", size=(720, 46),
             src_dir="pl00", src_png="player/pl00/pl00f.png",
             sprites=[(62, 0, 0, 30, 46, 1, 1),
                      (63, 30, 0, 30, 46, 33, 1),
                      (64, 60, 0, 30, 46, 65, 1),
                      (65, 90, 0, 30, 46, 97, 1),
                      (66, 120, 0, 30, 46, 129, 1),
                      (67, 150, 0, 30, 46, 161, 1),
                      (68, 180, 0, 30, 46, 193, 1),
                      (69, 210, 0, 30, 46, 225, 1),
                      (70, 240, 0, 30, 46, 1, 49),
                      (71, 270, 0, 30, 46, 33, 49),
                      (72, 300, 0, 30, 46, 65, 49),
                      (73, 330, 0, 30, 46, 97, 49),
                      (74, 360, 0, 30, 46, 129, 49),
                      (75, 390, 0, 30, 46, 161, 49),
                      (76, 420, 0, 30, 46, 193, 49),
                      (77, 450, 0, 30, 46, 225, 49),
                      (78, 480, 0, 30, 46, 1, 97),
                      (79, 510, 0, 30, 46, 33, 97),
                      (80, 540, 0, 30, 46, 65, 97),
                      (81, 570, 0, 30, 46, 97, 97),
                      (82, 600, 0, 30, 46, 129, 97),
                      (83, 630, 0, 30, 46, 161, 97),
                      (84, 660, 0, 30, 46, 193, 97),
                      (85, 690, 0, 30, 46, 225, 97)]),
    ],
    # 魔理沙：TH13 `pl01.anm` entry1 `player/pl01/pl01d.png`
    #   sprite69..72 = (1/65/129/193,193,62,30) 大导弹的 4 帧
    #   sprite73     = (1,145,14,14) 闪光
    "pl01": [
        dict(idx=14, name="player/pl01/pl01trance.png", size=(262, 30),
             src_dir="pl01", src_png="player/pl01/pl01d.png",
             sprites=[(67, 0, 0, 62, 30, 1, 193),
                      (68, 62, 0, 62, 30, 65, 193),
                      (69, 124, 0, 62, 30, 129, 193),
                      (70, 186, 0, 62, 30, 193, 193),
                      (71, 248, 0, 14, 14, 1, 145)]),
        # ★ 2026-09-16：**灵界形态 24 帧**（TH13 `pl01.anm` **entry1 `player/pl01/pl01d.png`** 的 sprite45..68；⚠ 源图 pl01d.png
        #     （~117）而非 `pl01.png`（~46））
        dict(idx=15, name="player/pl01/pl01form.png", size=(720, 46),
             src_dir="pl01", src_png="player/pl01/pl01d.png",
             sprites=[(72, 0, 0, 30, 46, 1, 1),
                      (73, 30, 0, 30, 46, 33, 1),
                      (74, 60, 0, 30, 46, 65, 1),
                      (75, 90, 0, 30, 46, 97, 1),
                      (76, 120, 0, 30, 46, 129, 1),
                      (77, 150, 0, 30, 46, 161, 1),
                      (78, 180, 0, 30, 46, 193, 1),
                      (79, 210, 0, 30, 46, 225, 1),
                      (80, 240, 0, 30, 46, 1, 49),
                      (81, 270, 0, 30, 46, 33, 49),
                      (82, 300, 0, 30, 46, 65, 49),
                      (83, 330, 0, 30, 46, 97, 49),
                      (84, 360, 0, 30, 46, 129, 49),
                      (85, 390, 0, 30, 46, 161, 49),
                      (86, 420, 0, 30, 46, 193, 49),
                      (87, 450, 0, 30, 46, 225, 49),
                      (88, 480, 0, 30, 46, 1, 97),
                      (89, 510, 0, 30, 46, 33, 97),
                      (90, 540, 0, 30, 46, 65, 97),
                      (91, 570, 0, 30, 46, 97, 97),
                      (92, 600, 0, 30, 46, 129, 97),
                      (93, 630, 0, 30, 46, 161, 97),
                      (94, 660, 0, 30, 46, 193, 97),
                      (95, 690, 0, 30, 46, 225, 97)]),
    ],
    # 早苗：⚠ **TH13 的 `pl02` 是早苗**（`pl02a.sht` = CHARACTER 2），对应 **TH18 的 `pl03`**。
    #   TH18 的 `pl02` 是咲夜、TH13 没有她 ⇒ 她的**形态**改成"自己的图 + 量出来的变换"
    #   （见本 dict 的 `pl02` 段；❓自定，`AUDIT` §DG-7）。
    #   源 = TH13 `pl02.anm` entry2 `player/pl02/pl02c.png`：
    #   sprite58..61 = (1/65/129/193,145,62,30) 双横条弹的 4 帧；sprite62 = (1,177,14,14) 闪光。
    # ★★ 咲夜（TH18 `pl02`）：TH13 没有这个自机 ⇒ **无原版成品可切**。用户 2026-09-16 定案：
    #   拿**她自己的 TH18 那 24 帧**（`player/pl02/pl02.png`；与 TH13 同一套 30×46 帧、32 px 网格），
    #   施加从 TH13 三对原版图量出来的「普通 → 灵界」色调变换（`assets/trance_form_transform.py`；
    #   验证数字：把变换施加到 TH13 普通图后与真·灵界图的 MAE 69.7→20.5 / 88.4→25.5 / 82.5→26.5）。
    #   ⚠ **这是 ❓自定**（登记 §DG-7），性质与她的灵界弹相同（用户口径，非复刻）。
    "pl02": [
        dict(idx=13, name="player/pl02/pl02form.png", size=(720, 46),
             src_game="th18", src_dir="pl02", src_png="player/pl02/pl02.png",
             transform="trance_form",     # ← 默认档（没给覆盖图时用这个）
             # ★★ 用户 2026-09-16 亲手把这 24 帧的辉光画好了（1px、alpha≈99、满不透明像素 +40%），
             #   放在 local/（gitignored）⇒ 构建期**零处理**直接用他的图（`AUDIT` §DG-8）。
             src_override="local/th18.v1.00a/trance_form/pl02form.png",
             # ★ 零售自机贴图是 16 位（ARGB4444，每通道 4 位）⇒ **构建期按该格式量化**
             #   （`_quantize16`，规则 = `round(v/17)*17`）：这样① 交上去的图与游戏里显示的完全一致、
             #   ② 回读闸能过（TH13 原版图本身就是 4 位色阶，所以它一直是逐像素精确的）。
             #   见 `AUDIT` §DG-8 的实测表。
             snap16=True,
             sprites=[(77, 0, 0, 30, 46, 1, 1),
                      (78, 30, 0, 30, 46, 33, 1),
                      (79, 60, 0, 30, 46, 65, 1),
                      (80, 90, 0, 30, 46, 97, 1),
                      (81, 120, 0, 30, 46, 129, 1),
                      (82, 150, 0, 30, 46, 161, 1),
                      (83, 180, 0, 30, 46, 193, 1),
                      (84, 210, 0, 30, 46, 225, 1),
                      (85, 240, 0, 30, 46, 1, 49),
                      (86, 270, 0, 30, 46, 33, 49),
                      (87, 300, 0, 30, 46, 65, 49),
                      (88, 330, 0, 30, 46, 97, 49),
                      (89, 360, 0, 30, 46, 129, 49),
                      (90, 390, 0, 30, 46, 161, 49),
                      (91, 420, 0, 30, 46, 193, 49),
                      (92, 450, 0, 30, 46, 225, 49),
                      (93, 480, 0, 30, 46, 1, 97),
                      (94, 510, 0, 30, 46, 33, 97),
                      (95, 540, 0, 30, 46, 65, 97),
                      (96, 570, 0, 30, 46, 97, 97),
                      (97, 600, 0, 30, 46, 129, 97),
                      (98, 630, 0, 30, 46, 161, 97),
                      (99, 660, 0, 30, 46, 193, 97),
                      (100, 690, 0, 30, 46, 225, 97)]),
    ],
    "pl03": [
        dict(idx=17, name="player/pl03/pl03trance.png", size=(262, 30),
             src_dir="pl02", src_png="player/pl02/pl02c.png",
             sprites=[(68, 0, 0, 62, 30, 1, 145),
                      (69, 62, 0, 62, 30, 65, 145),
                      (70, 124, 0, 62, 30, 129, 145),
                      (71, 186, 0, 62, 30, 193, 145),
                      (72, 248, 0, 14, 14, 1, 177)]),
        # ★ 2026-09-16：**灵界形态 24 帧**（TH13 `pl02.anm` **entry2 `player/pl02/pl02c.png`** 的 sprite34..57；⚠ 源图 pl02c.png
        #     （~127）而非 `pl02.png`（~62））
        dict(idx=18, name="player/pl03/pl03form.png", size=(720, 46),
             src_dir="pl02", src_png="player/pl02/pl02c.png",
             sprites=[(73, 0, 0, 30, 46, 1, 1),
                      (74, 30, 0, 30, 46, 33, 1),
                      (75, 60, 0, 30, 46, 65, 1),
                      (76, 90, 0, 30, 46, 97, 1),
                      (77, 120, 0, 30, 46, 129, 1),
                      (78, 150, 0, 30, 46, 161, 1),
                      (79, 180, 0, 30, 46, 193, 1),
                      (80, 210, 0, 30, 46, 225, 1),
                      (81, 240, 0, 30, 46, 1, 49),
                      (82, 270, 0, 30, 46, 33, 49),
                      (83, 300, 0, 30, 46, 65, 49),
                      (84, 330, 0, 30, 46, 97, 49),
                      (85, 360, 0, 30, 46, 129, 49),
                      (86, 390, 0, 30, 46, 161, 49),
                      (87, 420, 0, 30, 46, 193, 49),
                      (88, 450, 0, 30, 46, 225, 49),
                      (89, 480, 0, 30, 46, 1, 97),
                      (90, 510, 0, 30, 46, 33, 97),
                      (91, 540, 0, 30, 46, 65, 97),
                      (92, 570, 0, 30, 46, 97, 97),
                      (93, 600, 0, 30, 46, 129, 97),
                      (94, 630, 0, 30, 46, 161, 97),
                      (95, 660, 0, 30, 46, 193, 97),
                      (96, 690, 0, 30, 46, 225, 97)]),

    ],
}

# ★★ 2026-09-22（108 · owner 裁决「路 A」）：**妖侧本体 55 张**接进 pl0X.anm。
#
# ■ 贴图源（**已经在仓里**，就是 `engine/player/th08/03-youkai-body-and-gauge.md` §1.3 切好的那 55 张）：
#   `assets/ability/youkai/YOUKAI_{YUKARI,ALICE,REMILIA}_S*.png`
#   `assets/ability/sanae_youkai/YOUKAI_AYA_S*.png`
#   ⇒ 本轮**逐张复核**：每张 PNG 里那段「有效画区」与原版切图**逐像素相等**（55/55，MAE = 0.000）。
#     PNG 是带透明补边的画布，**有效画区** = 原版 sprite 矩形：
#       pl00/pl01/pl03：32×64 画布、画区 (1,9,30,46)（= TH08 player00/01.anm、TH16 pl03.anm 的 sprite 矩形）
#       pl02（レミリア）：64×64 画布、画区 (9,9,46,46)（= TH08 player02.anm 的 sprite 矩形）
#     ⚠ **sprite 矩形必须写原版那个（30×46 / 46×46），不能写画布的 32×64 / 64×64**：引擎按 w/h 算锚点
#       （`AnmVm__set_sprite` `0x477b00` 把 w/256、h/256 写进 `vm+0x3d4/+0x3e8`，绘制时 ±128×w/256 = ±w/2
#       ⇒ 锚点 = 矩形中心，见 `engine/anm/th18/02-render-stages.md`）⇒ 拿画布尺寸接上去会整体偏 (1,9)。
#   ⇒ 源 PNG **原样**（含补边）贴进画布、矩形写原版值；两件事都不做变换 ⇒ 回读闸才能要求逐像素相等。
#     实测这 55 张**本来就是 4 位色阶**（每通道都是 17 的倍数，来自原版 16 位贴图）⇒ ARGB4444 往返无损。
#
# ■ 每项 = (源目录, 名字前缀, 帧 → 原版 sprite 号, 画布单格, 有效画区@单格内)
YOUKAI_BODY_SRC = MOD / "assets" / "ability"
YOUKAI_BODY = {
    "pl00": ("youkai", "YOUKAI_YUKARI", range(11, 22), (32, 64), (1, 9, 30, 46)),
    "pl01": ("youkai", "YOUKAI_ALICE", range(11, 22), (32, 64), (1, 9, 30, 46)),
    "pl02": ("youkai", "YOUKAI_REMILIA", range(11, 20), (64, 64), (9, 9, 46, 46)),
    "pl03": ("sanae_youkai", "YOUKAI_AYA", range(0, 24), (32, 64), (1, 9, 30, 46)),
}
# 各机体的 entry 号 / 首个 sprite 号 / 新贴图名（**接在已有追加之后**；`_append_spec` 会硬校验 entry 号）。
# sprite 号 = 该文件当前最大 sprite + 1 起连续（pl00 原生 0..59 + 113 追加 60..85 ⇒ 86 起，以此类推）。
YOUKAI_BODY_IDS = {
    "pl00": dict(idx=15, sprite0=86, name="player/pl00/pl00yk.png"),
    "pl01": dict(idx=16, sprite0=96, name="player/pl01/pl01yk.png"),
    "pl02": dict(idx=14, sprite0=101, name="player/pl02/pl02yk.png"),
    "pl03": dict(idx=19, sprite0=97, name="player/pl03/pl03yk.png"),
}


def _youkai_body_entries(player: str):
    """该机体的妖侧本体 entry（1 条；55 张按机体分成 11/11/9/24 帧）。"""
    if player not in YOUKAI_BODY:
        return []
    sub, prefix, ids, cell, (ax, ay, aw, ah) = YOUKAI_BODY[player]
    meta = YOUKAI_BODY_IDS[player]
    ids = list(ids)
    return [dict(idx=meta["idx"], name=meta["name"],
                 size=(cell[0] * len(ids), cell[1]), cell=cell,
                 cells=[(prefix, sub, sid) for sid in ids],
                 sprites=[(meta["sprite0"] + k, cell[0] * k + ax, ay, aw, ah)
                          for k in range(len(ids))])]


# ---- 五个「本体动画状态」的源脚本号 + 该脚本**实际引用到**的源 sprite 区间 ----------------------
#
# 状态名与号（0..4）**就是 TH18 引擎自己那套**（不是我们起的）：`th18:0x45b610`–`0x45b7a8` 的
# 移动动画状态机按「本帧输入速度 x（`player+0x47934`）vs 上一帧」四选一装载脚本 1/2/3/4，
# 其余时候保持不动（初始 = 脚本 0）。逐条读出来的对照：
#
#   dx<0 && px>=0 → 脚本 **1**   dx==0 && px<0 → 脚本 **2**
#   dx>0 && px<=0 → 脚本 **3**   dx==0 && px>0 → 脚本 **4**   （否则不变；开局 = 0）
#
# 与 TH08 的 `SET_MOVE_ANIM(a,b,c,d)`（`SET_MOVE_ANIM(1,2,3,4)` 人侧 / `(6,7,8,9)` 妖侧）**逐槽同序**：
# a=起步向左、b=从左侧停下、c=起步向右、d=从右侧停下 ⇒ **妖侧脚本号 = 人侧 + 5**（idle 0 → 5）。
# 结构证据（两条独立）：① TH18 原生 `pl00.anm` script1/2 的 lead-in+循环形状与 TH08 人侧 1/2 同形；
# ② TH18 的 script3/4 = 同形状但用 sprite 16..23，而 TH08 的 3/4 用 `flipX()` —— 实测
#    `mirror(sprite8..15)` 与 `sprite16..23` 的 MAE ≈ 8、不镜像 ≈ 27–54 ⇒ **16..23 就是 8..15 的镜像**，
#    即 TH18 把 TH08 的 `flipX` 预先烤成了贴图。一手全程见 `engine/player/th08/03-youkai-body-and-gauge.md`。
#
# 区间是**从原版 spec 里数出来的**（`sprite()` 引用集合），不是猜的顺序；
# `_assert_spec_source` 每次构建都回读对账（多一个少一个都报错）。
_YK_STATES = ("IDLE", "MOVE_L", "STOP_L", "MOVE_R", "STOP_R")
_YK_SRC_SPANS = {
    # TH08 `player00.anm`（八雲紫）与 `player01.anm`（アリス）：脚本 5..9，sprite 11..21
    "player00": ((5, ((11, 15),)), (6, ((15, 22),)), (7, ((11, 19),)),
                 (8, ((15, 22),)), (9, ((11, 19),))),
    "player01": ((5, ((11, 15),)), (6, ((15, 22),)), (7, ((11, 19),)),
                 (8, ((15, 22),)), (9, ((11, 19),))),
    # TH08 `player02.anm`（レミリア）：**只有 9 帧**（sprite 11..19），且每个脚本的引用集合更小
    "player02": ((5, ((11, 15),)), (6, ((15, 20),)), (7, ((11, 17),)),
                 (8, ((15, 20),)), (9, ((11, 17),))),
    # TH16 `pl03.anm`（射命丸文，24 帧）：脚本 **0..4**（TH16 的人/妖不分家，0..4 就是她的全部本体动画）
    "pl03": ((0, ((0, 8),)), (1, ((8, 16),)), (2, ((0, 12),)),
             (3, ((16, 24),)), (4, ((0, 8), (16, 20)))),
}
_YK_SRC_BASE = {"player00": 11, "player01": 11, "player02": 11, "pl03": 0}


def _youkai_plan(player: str, src: str, sid0: int, sprite0: int):
    """该机体的 5 支妖侧本体脚本（顺序 = `_YK_STATES` = 引擎的状态号 0..4）。

    `sid0` = 第一支的追加脚本号（必须**正好**接在该文件已有追加之后，`_append_spec` 会硬校验）；
    `sprite0` = 第一张帧贴图的 sprite 号（= 该文件当前最大 sprite + 1）。

    `prologue=True` 只给 TH08 源：TH08 的本体脚本 5..9 **没有** `type/layer/resampleMode` 前导
    （TH08 里这些字段由 VM 保留），而 TH18 的 `AnmVm__load_script` `0x407420` 会把整块脚本模板
    拷进 VM ⇒ 必须在头部补上与 **TH18 原生本体脚本逐字相同**的那三行（`-1:` / `type(0)` /
    `layer(14)` / `resampleMode(1)` / `0:`），否则装上去的是模板里的默认值。
    TH16 源的脚本自带同样的前导 ⇒ 不注入。

    ⚠ `flipX()` **原样保留**（TH18 的 v8 支持：原生 `enemy.anm` 等处共 50+ 次使用）。
    """
    game = "th16" if src == "pl03" else "th08"
    base = _YK_SRC_BASE[src]
    spans = _YK_SRC_SPANS[src]
    out = []
    for k, (name, (sid, rs)) in enumerate(zip(_YK_STATES, spans)):
        smap = {}
        for (lo, hi) in rs:
            for sp in range(lo, hi):
                smap[sp] = sprite0 + (sp - base)
        out.append(dict(macro=f"YKBODY_{player.upper()}_{name}", game=game, src=src,
                        sid=sid, new_sid=sid0 + k, smap=smap, k=1.0, alpha255=False,
                        scale_inject=False, prologue=(game == "th08")))
    return out



# ★★ 2026-09-22（owner：「那就先做人妖槽以及永点VM池」）：**108 的人妖槽 HUD + 刻符（时点）**。
#
# 两张贴图、5 支脚本，来源**全是 TH08 原版**（⇒ 走 `game="th08"` 那条现成路：裸 `dat/*.anm` + `v2.anmm`）：
#   · **刻符（时点）**：`etama.anm` entry1 `data/etama/etama2.png` 的 **sprite179 = (112,64,16,16)**
#     + 它自己的脚本 **script68**（`th08` 里脚本号 = `61 + item_type`，type 7 = 刻符，一手 `th08:0x4403b9`）。
#   · **人妖槽 HUD 四件**（`ascii.anm` entry0 `data/ascii/ascii.png`，一手 `AsciiManager::InitializeVms`）：
#     槽条 = **script5 / sprite155 (0,224,128,16)**、人间图标 = **script6 / sprite153 (160,208,16,16)**、
#     妖怪图标 = **script7 / sprite154 (176,208,16,16)**、游标 = **script8 / sprite152 (128,208,8,12)**。
#
# ⚠ **一处跨版本 opcode 翻译（必须做，否则编不过）**：TH08 的 `anchorTopLeft()`（v2 的 opcode 22）
#   在 v8 里**没有**了 —— v8 是带参的 `anchor(h, v)`（opcode 421），按
#   `engine/anm/OVERVIEW.md` 的跨版本锚点表：**0 = 中 / 1 = 左(顶) / 2 = 右(底)**
#   ⇒ `anchorTopLeft()` ≡ `anchor(1, 1)`（取值口径 🟡，但 107 的 HUD 标签也是这么用的）。
#   其余 opcode（`sprite/zWriteDisable/alpha/alphaTime/colorTime/pos/posTime/stop/stopHide/
#   interruptLabel/static`）v8 全有（`stopHide` 见 TH18 原生 `text.anm`，`static` 见 `bullet.anm`）。
#
# 每个机体一套（四个 `pl0X.anm` 各追加一份）：VM 的贴图按 **VM 自己那一页**的 anm 取
# （`th18:0x481117`–`0x481136`）⇒ 不追加进 pl0X.anm 就画不出来 —— 与妖侧本体同一条理。
YK_EXTRA_PLAN = {
    # 名字 : (源 anm 名, 源贴图相对路径, [(源 sprite 号, 源 x, 源 y, w, h), …], 画布宽高)
    "orb":   ("etama", "data/etama/etama2.png", [(179, 112, 64, 16, 16)], (16, 16)),
    "gauge": ("ascii", "data/ascii/ascii.png",
              [(152, 128, 208, 8, 12),      # 游标（放在画布最左，TH08 的号序）
               (153, 160, 208, 16, 16),     # 人间图标
               (154, 176, 208, 16, 16),     # 妖怪图标
               (155, 0, 224, 128, 16)],     # 槽条
              (168, 16)),
    # ★★ 2026-09-22 晚（owner：「人妖槽 hud 不对 …… 你得把 th08 的贴图搞过来」）：
    #   **百分比数字的字形**（TH08 `ascii.anm` entry0 的 sprite **0x88..0x94** = 136..148，
    #   各 8×12，y = 208、x = 0,8,…,96）。一手：`AsciiManager::DrawPercentage` `th08:0x405e10`
    #   的注释「0x88='0'..0x91='9'、0x92='%'、0x93='.'、0x94='-'」+ 逐条 `SetSprite` 调用；
    #   矩形是本仓 `thanm -l 8 -m v2.anmm dat/ascii.anm` 逐条读出来的（见报告的表）。
    #   ⚠ 这一批**不是**给脚本「画一句话」用的：TH08 那边引擎把**同一个 VM** 的 sprite 逐字改、
    #     逐字画（`DrawPercentage` 里 5~6 次 `SetSprite` + `DrawNoRotation`）⇒ TH18 侧要**多个 VM**
    #     （一个 VM 一帧只画一次），由 C 侧 `yk_gauge_text_*` 按同一套布局写每格的 sprite/位置。
    # ★ 2026-09-22 夜（owner #27）：**第 14 格 = '+'**（TH08 `ascii.anm` entry0 sprite **149** =
    #   `(104,208,8,12)`，逐像素确认是十字形），给「+1」弹字用；`0x8a` 那套里 150/151 未定名 ⇒ **不追加**。
    "gaugefont": ("ascii", "data/ascii/ascii.png",
                  [(136 + i, i * 8, 208, 8, 12) for i in range(14)],   # 136..149 = '0'..'9' '%' '.' '-' '+'
                  (112, 12)),
}
# 每项的**画布内摆放**（x 从左往右挨着放，间距 0；⚠ 槽条是 128 宽、游标 8 宽 ⇒ 画布 168 够）
_YK_EXTRA_LAYOUT = {"orb": [(0, 0)], "gauge": [(0, 0), (8, 0), (24, 0), (40, 0)],
                    "gaugefont": [(i * 8, 0) for i in range(14)]}
# entry 名（每机各一份，故名字里不带机体号也可以；贴图按 entry 名去 tex 目录找）
YK_EXTRA_NAME = {"orb": "player/yk_orb.png", "gauge": "player/yk_gauge.png",
                 "gaugefont": "player/yk_gaugefont.png"}
# 每机的新 entry 号 / 首个 sprite 号 / 首个脚本号（接在「妖侧本体」之后，只追加）
YK_EXTRA_IDS = {
    "pl00": dict(entry0=16, sprite0=97,  script0=48),
    "pl01": dict(entry0=17, sprite0=107, script0=54),
    "pl02": dict(entry0=15, sprite0=110, script0=37),
    "pl03": dict(entry0=20, sprite0=121, script0=39),
}
# 脚本：键 → (源 anm, 源脚本号, 宏后缀, 源 sprite → 该 entry 内第几个)
YK_EXTRA_SCRIPTS = (
    ("orb",    "etama", 68, "ORB",        {179: 0}),
    ("gauge",  "ascii", 5,  "GAUGE_BAR",  {155: 3}),
    ("gauge",  "ascii", 6,  "GAUGE_HUMAN", {153: 1}),
    ("gauge",  "ascii", 7,  "GAUGE_YOUKAI", {154: 2}),
    ("gauge",  "ascii", 8,  "GAUGE_CURSOR", {152: 0}),
    # ★ 字体 VM 的**载体脚本**：TH08 `ascii.anm` **script4** 逐条照抄 —— 它本身只做
    #   `sprite(sprite136)`（= 字形 '0'）+ `zWriteDisable(1)` + `static()`：TH08 的引擎就是拿它当
    #   「一块可换字的画布」（`AsciiManager.cpp:202` 把 percentageText 装成 script4，随后逐字
    #   `SetSprite` 再 `DrawNoRotation`）。C 侧每帧用 `ce_anm_set_sprite` 换字、`ce_anm_set_scale`
    #   缩放小数位 ⇒ 与一手同构（一个字形 = 一次绘制）。
    ("gaugefont", "ascii", 4, "GAUGEFONT", {136: 0}),
)


def _yk_extra_order():
    return ("orb", "gauge", "gaugefont")


def _yk_extra_entries(player: str):
    """该机体的 2 条追加 entry（刻符 + 人妖槽 HUD）；编号/sprite 号接在妖侧本体之后。"""
    ids = YK_EXTRA_IDS[player]
    out, k = [], 0
    for name in _yk_extra_order():
        src_anm, src_png, sprites, size = YK_EXTRA_PLAN[name]
        lay = _YK_EXTRA_LAYOUT[name]
        ps = []
        for i, (_sid, sx, sy, w, h) in enumerate(sprites):
            cx, cy = lay[i]
            ps.append((ids["sprite0"] + k, cx, cy, w, h, src_anm, src_png, sx, sy))
            k += 1
        out.append(dict(idx=ids["entry0"] + len(out), name=YK_EXTRA_NAME[name],
                        size=size, sprites=ps))
    return out


def _yk_extra_plan(player: str):
    """该机体的 5 支追加脚本（刻符 1 + 人妖槽 HUD 4）。"""
    ids = YK_EXTRA_IDS[player]
    order = _yk_extra_order()
    base = {}
    k = 0
    for name in order:
        base[name] = ids["sprite0"] + k
        k += len(YK_EXTRA_PLAN[name][2])
    out = []
    for i, (name, src, sid, macro, smap_src) in enumerate(YK_EXTRA_SCRIPTS):
        smap = {s: base[name] + idx for s, idx in smap_src.items()}
        # ⚠ 宏名必须**带机体号**：四个 pl0X.anm 的追加脚本号互不相同（各文件从自己的原生脚本数
        #   往后接），不带就会在 `pl_anm_ids.h` 里互相覆盖（实测报 `macro redefined`）。
        out.append(dict(macro=f"YK{macro}_{player.upper()}", game="th08", src=src, sid=sid,
                        new_sid=ids["script0"] + i, smap=smap,
                        k=(2.0 if name == "orb" else 1.0), alpha255=False,
                        # ★★★ 2026-09-23 **订正**（owner 实跑：「**永点图案大了 tm 3 倍**」）——
                        #   上一版在这里注入 `scale(2.0)`，理由抄的是 **UI 层**那条规矩（UI 层 1 texel = 1 表面 px，
                        #   TH08 的 640 空间美术要 ×2）。但**刻符不是 UI 层**：它是**世界层** VM
                        #   （`YK_ORB_ANM_LAYER = 7`、`ce_anm_spawn` = 世界列表、引擎里层 3–19 归**相机 3**，
                        #   见 `engine/anm/th18/02-render-stages.md` §1）⇒ 世界层本来就吃 `×DAT = ×2`
                        #   （一手 `th18:0x4063d0` 的 mode 0/2 路径 + `0x4735ce` 把 DAT 写成 2.0）
                        #   ⇒ 再注入 2.0 = **双重放大**（16 texel 的球画成 64 表面 px）。
                        #   ⇒ **不注入**：VM 初值 1.0 ⇒ 16 texel × 2 = **32 表面 px** = TH18 自己那枚道具
                        #   的尺寸（`etama` 16×16 的球在 TH08 的 640 空间里也是 16 px）✓
                        #   ⚠ 要按眼睛微调就改这一个开关（改成 `True` 会把 `k` 当 scale 注入；`k` 见上行）。
                        #   验收：`youkai/orb.vm … scale …` 那行应打 **1.00**（注入 2.0 那一版打 2.00）。
                        scale_inject=False, th08_opcodes=True,
                        # ★ #30：人妖槽四支 + 字形要**归零脚本 pos**（否则与 C 侧锚点叠加 ⇒ y 翻倍画到屏外）
                        zero_pos=(name in ("gauge", "gaugefont")),
                        # ★ 2026-09-23（#th11-3 同源缺口）+ 2026-09-26（#39 真根因）：刻符（`orb`）
                        #   走**世界列表**且自带层（`YK_ORB_ANM_LAYER`，由 C 侧传）⇒ 不注入；
                        #   四支人妖槽 HUD + 字形走 `ce_anm_spawn_ui`（UI 列表）⇒ 必须自带
                        #   `layer(25)` **并且紧跟一行 `resolutionMode(2)`**（少了后半句 = 位置被
                        #   `set_layer` 派生的 resolutionMode 1 乘 2 ⇒ 整块画到屏外，见
                        #   `_inject_hud_layer` 的一手理由）。
                        hud_layer=(None if name == "orb" else 25)))
    return out

APPEND_PLAN = {
    # ★★ 2026-09-13（卡 113）：TH13 灵界**主炮**弹 —— 每机两支：主弹 + 每帧闪光子 VM。
    #
    # 一手出处（`thanm -l 13 -m v8.anmm local/th13.v1.00c/dat/pl0X.anm` 解出的正文，
    # 逐条读出来的，不是照抄社区）：
    #   · pl00（灵梦）：`script14` = 主弹（`sprite67` + `scriptNewRoot(script15)`）、
    #                   `script15` = 闪光（`sprite68`，`ins_433` 极坐标）。
    #   · pl01（魔理沙）：`script11` = 主弹（`sprite69`，每 10 帧换 70/71/72）、`script12` = 闪光（`sprite73`）。
    #   · TH13 `pl02`（早苗）→ TH18 `pl03`：`script11` = 主弹（`sprite58..61`，每 2 帧换一帧、
    #     同时 `scriptNewRoot(script12)`）、`script12` = 闪光（`sprite62`）。
    #   ⚠ TH13 的 pl02 是早苗、TH18 的 pl02 是咲夜 —— 这里**没有** TH18 pl02 的条目。
    #
    # 追加脚本号（硬约定，另一侧 `assets/sht/append_shooterset.py` 的
    # `PL00/PL01/PL03_SCRIPT_TRANCE` 与 `check_pl_anm_ids()` 会交叉核对，**不许改**）：
    #   pl00 41/42 · pl01 47/48 · pl03 32/33（= 各自「原生脚本数 + 已有追加」的下一个连续号）。
    "pl00": [
        dict(macro="PL00_TRANCE", game="th13", src="pl00", sid=14, new_sid=41,
             smap={67: 60}, sref={15: 42}, k=1.0, alpha255=False, scale_inject=False),
        dict(macro="PL00_TRANCE_GLINT", game="th13", src="pl00", sid=15, new_sid=42,
             smap={68: 61}, k=1.0, alpha255=False, scale_inject=False),
    ] + _youkai_plan(
        # ★★ 108 妖侧本体 = 八雲紫（TH08 `player00.anm` script5..9）
        # state 号 ↔ 宏后缀：0 idle / 1 move-L / 2 stop-L / 3 move-R / 4 stop-R（见 `_youkai_plan` 的长注释）
        "pl00", "player00", 43, 86) + _yk_extra_plan("pl00"),
    "pl01": [
        dict(macro="PL01_TRANCE", game="th13", src="pl01", sid=11, new_sid=47,
             smap={69: 67, 70: 68, 71: 69, 72: 70}, sref={12: 48},
             k=1.0, alpha255=False, scale_inject=False),
        dict(macro="PL01_TRANCE_GLINT", game="th13", src="pl01", sid=12, new_sid=48,
             smap={73: 71}, k=1.0, alpha255=False, scale_inject=False),
    ] + _youkai_plan(
        # ★★ 108 妖侧本体 = アリス（TH08 `player01.anm` script5..9）
        "pl01", "player01", 49, 96) + _yk_extra_plan("pl01"),
    "pl02": [
        dict(macro="PL02_WOLF",  src="pl02", sid=5, new_sid=30, smap={24: 75},
             k=KNIFE_SCALE, alpha255=True, scale_inject=True),
        dict(macro="PL02_EAGLE", src="pl02", sid=9, new_sid=31, smap={25: 76},
             k=KNIFE_SCALE, alpha255=True, scale_inject=True),
    ] + _youkai_plan(
        # ★★ 108 妖侧本体 = レミリア（TH08 `player02.anm` script5..9，9 帧、46×46）
        "pl02", "player02", 32, 101) + _yk_extra_plan("pl02"),
    "pl03": [
        dict(macro="PL03_FROG",  src="pl03", sid=8, new_sid=29,
             smap={34: 57, 40: 62, 41: 63, 42: 64, 43: 65, 25: 66},
             k=2.0, alpha255=False, scale_inject=False),
        dict(macro="PL03_SNAKE", src="pl03", sid=6, new_sid=30,
             smap={30: 58, 31: 59, 32: 60, 33: 61}, sref={7: 31},
             k=2.0, alpha255=False, scale_inject=False),
        # ★ AL64：早苗狼命中「旋转小光斑」= 原生 script7（单帧 sprite25）→ 用我们嵌的 sprite67
        #   （用户可在 art-src 里改 `sanae_wolf_glint_s25_f0.png`）；旋转由父蛇 VM 的 rotateAuto 继承。
        dict(macro="PL03_GLINT", src="pl03", sid=7, new_sid=31,
             smap={25: 67}, sref={},
             k=1.0, alpha255=False, scale_inject=False),
        # —— ★ 113：TH13 `pl02.anm`（= 早苗）的灵界主弹/闪光（出处见本 dict 开头那段）——
        dict(macro="PL03_TRANCE", game="th13", src="pl02", sid=11, new_sid=32,
             smap={58: 68, 59: 69, 60: 70, 61: 71}, sref={12: 33},
             k=1.0, alpha255=False, scale_inject=False),
        dict(macro="PL03_TRANCE_GLINT", game="th13", src="pl02", sid=12, new_sid=33,
             smap={62: 72}, k=1.0, alpha255=False, scale_inject=False),
    ] + _youkai_plan(
        # ★★ 108 妖侧本体 = 射命丸文（**TH16** `pl03.anm` script0..4，24 帧；她的 0..4 就是本体全套）
        "pl03", "pl03", 34, 97) + _yk_extra_plan("pl03"),
}
PL_IDS_H = MOD / "native" / "pl_anm_ids.h"

# ★★ 硬钉死的追加脚本号（2026-09-13）：`assets/sht/append_shooterset.py` 里
#   `PL00_SCRIPT_TRANCE / PL01_SCRIPT_TRANCE / PL03_SCRIPT_TRANCE` 与 `check_pl_anm_ids()`
#   把这三个数**写死**了，卡 113 的另一半（sht 行）就是照它发弹的。
#   这里再钉一遍**反方向**的闸：`write_pl_ids_h()` 是「计划 → 头」，本表是「外部约定 → 计划」。
#   谁要动号，必须**同时**改 `APPEND_PLAN` + 本表 + `append_shooterset.py`，否则这里先红，
#   而不是等实跑发现「灵界弹指向了别的脚本」（那是最难查的一类静默错）。
PINNED_NEW_SIDS = {
    # ★★ 108 妖侧本体（2026-09-22）：C 侧 `YK_BODY_SCRIPT[4][5]` 直接吃 `pl_anm_ids.h` 的宏，
    #   所以号本身不会静默漂移；钉这一遍是为了**任何重排顺序**都先在这里红，而不是等实跑发现
    #   「低速画出来的是另一支动画」。宏名后缀 = 状态号（`_YK_STATES`，0..4）。
    "pl00": {"PL00_TRANCE": 41, "PL00_TRANCE_GLINT": 42,
             "YKBODY_PL00_IDLE": 43, "YKBODY_PL00_MOVE_L": 44, "YKBODY_PL00_STOP_L": 45,
             "YKBODY_PL00_MOVE_R": 46, "YKBODY_PL00_STOP_R": 47},
    "pl01": {"PL01_TRANCE": 47, "PL01_TRANCE_GLINT": 48,
             "YKBODY_PL01_IDLE": 49, "YKBODY_PL01_MOVE_L": 50, "YKBODY_PL01_STOP_L": 51,
             "YKBODY_PL01_MOVE_R": 52, "YKBODY_PL01_STOP_R": 53},
    "pl02": {"PL02_WOLF": 30, "PL02_EAGLE": 31,
             "YKBODY_PL02_IDLE": 32, "YKBODY_PL02_MOVE_L": 33, "YKBODY_PL02_STOP_L": 34,
             "YKBODY_PL02_MOVE_R": 35, "YKBODY_PL02_STOP_R": 36},
    "pl03": {"PL03_FROG": 29, "PL03_SNAKE": 30, "PL03_GLINT": 31, "PL03_TRANCE": 32,
             "PL03_TRANCE_GLINT": 33,
             "YKBODY_PL03_IDLE": 34, "YKBODY_PL03_MOVE_L": 35, "YKBODY_PL03_STOP_L": 36,
             "YKBODY_PL03_MOVE_R": 37, "YKBODY_PL03_STOP_R": 38,
             "YKORB_PL03": 39, "YKGAUGE_BAR_PL03": 40, "YKGAUGE_HUMAN_PL03": 41,
             "YKGAUGE_YOUKAI_PL03": 42, "YKGAUGE_CURSOR_PL03": 43, "YKGAUGEFONT_PL03": 44},
}
# ★★ 108 的**人妖槽 HUD + 刻符**（2026-09-22，owner：「那就先做人妖槽以及永点VM池」）。
#   与 `YKBODY_*` 同一个理由钉号：C 侧 `YK_ORB_SCRIPT[4]` / `YK_GAUGE_SCRIPT[4][4]` 直接吃这些宏。
PINNED_NEW_SIDS["pl00"].update({"YKORB_PL00": 48, "YKGAUGE_BAR_PL00": 49,
                               "YKGAUGE_HUMAN_PL00": 50, "YKGAUGE_YOUKAI_PL00": 51,
                               "YKGAUGE_CURSOR_PL00": 52, "YKGAUGEFONT_PL00": 53})
PINNED_NEW_SIDS["pl01"].update({"YKORB_PL01": 54, "YKGAUGE_BAR_PL01": 55,
                               "YKGAUGE_HUMAN_PL01": 56, "YKGAUGE_YOUKAI_PL01": 57,
                               "YKGAUGE_CURSOR_PL01": 58, "YKGAUGEFONT_PL01": 59})
PINNED_NEW_SIDS["pl02"].update({"YKORB_PL02": 37, "YKGAUGE_BAR_PL02": 38,
                               "YKGAUGE_HUMAN_PL02": 39, "YKGAUGE_YOUKAI_PL02": 40,
                               "YKGAUGE_CURSOR_PL02": 41, "YKGAUGEFONT_PL02": 42})

_SPRITE_REF = re.compile(r"\bsprite\(sprite(\d+)\)")
_SPRITE_DEF = re.compile(r"\bsprite(\d+):")
# `thanm` 读 spec 时**已知无害**的 stderr 行（见 `_spec_of`）。
_THANM_BENIGN = ("is a keyword, ignoring",)
# `scriptNew` 家族的**全部**拼法（v8.anmm：500 scriptNew / 501 scriptNewUI / 502 scriptNewFront /
# 503 scriptNewUIFront / 504 scriptNewRoot / 505 scriptNewPos / 506 scriptNewRootPos）。
# ⚠ 2026-09-13 修：这里原先是 `scriptNewPos?` —— **漏了 `scriptNewRoot`**。而 TH13 灵界弹与
#   TH18 原生 pl00 script7/pl02 script5 用的正是 `scriptNewRoot` ⇒ 那道「scriptNew 目标存在」
#   的闸当时**根本没在跑**（静默放过）。现在补全，闸才真的成立。
_SCRIPT_NEW_KIND = r"UI|UIFront|Front|Root|RootPos|Pos"
_SCRIPT_NEW = re.compile(rf"\bscriptNew(?:{_SCRIPT_NEW_KIND})?\(script(\d+)")
_LAYER_CALL = re.compile(r"\blayer\((\d+)\)")
_SCALE_CALL = re.compile(r"\b(scale|scaleTime)\(([^)]*)\)")
_NUMF = re.compile(r"^(-?\d+(?:\.\d+)?)([fF]?)$")


def _mul(tok: str, k: float) -> str:
    m = _NUMF.match(tok.strip())
    if not m:
        raise SystemExit(f"不能缩放 scale 参数 {tok!r}")
    v = float(m.group(1)) * k
    s = f"{v:.6g}"
    if "." not in s and "e" not in s and "E" not in s:
        s += ".0"
    return s + "f"


def _scale_block(body: str, k: float) -> str:
    """把脚本里所有 `scale(x,y)` / `scaleTime(t,m,x,y)` 的 x/y 各乘 k（sprite 号不动）。"""
    if k == 1.0:
        return body

    def rep(m):
        name, args = m.group(1), [a.strip() for a in m.group(2).split(",")]
        if name == "scale":
            args[0], args[1] = _mul(args[0], k), _mul(args[1], k)
        else:
            args[2], args[3] = _mul(args[2], k), _mul(args[3], k)
        return f"{name}({', '.join(args)})"

    return _SCALE_CALL.sub(rep, body)


def _rename_script(block: str, old: int, new: int) -> str:
    out, n = re.subn(rf"^script script{old} \{{", f"script script{new} {{", block, count=1, flags=re.M)
    if n != 1:
        raise SystemExit(f"源脚本 script{old} 的头没找到，不能改名为 script{new}")
    return out


def _replace_sprites(body: str, mapping: dict) -> str:
    for old, new in mapping.items():
        body = re.sub(rf"\bsprite\(sprite{old}\)", f"sprite(sprite{new})", body)
    return body


def _replace_script_new(body: str, mapping: dict) -> str:
    """把 `scriptNew(scriptN)` / `scriptNewRoot(scriptN)` / `scriptNewPos(scriptN,…)` 的 N 换掉。"""
    for old, new in mapping.items():
        body = re.sub(rf"\bscriptNew({_SCRIPT_NEW_KIND})?\(script{old}\b",
                      lambda m: f"scriptNew{m.group(1) or ''}(script{new}", body)
    return body


def _remap_layer(body: str, mapping: dict) -> str:
    """把跨版本搬来的 `layer(N)` 翻成目标版本的层号（表见 `LAYER_MAP`）。"""
    def rep(m):
        v = int(m.group(1))
        return f"layer({mapping.get(v, v)})"
    return _LAYER_CALL.sub(rep, body)


def _inject_scale(body: str, k: float) -> str:
    """在脚本头后注入 `scale(k,k)`（原脚本没有 scale 的刀脚本用；k 由调用方保证 != 1）。"""
    m = re.match(r"(script script\d+ \{\n)", body)
    if not m:
        raise SystemExit("注入 scale 失败：脚本头没找到")
    return body[:m.end()] + f"    scale({_mul('1.0f', k)}, {_mul('1.0f', k)});\n" + body[m.end():]


# 追加 entry 的贴图格式（spec 的 `format:` / `THTXFormat:` 号）：
#   5 = **ARGB4444（16 位、每通道 4 位）** —— 零售自机贴图就是这个（实测：TH13/TH18 的自机贴图
#       每个通道取值 **100% 是 17 的倍数** ⇒ 原版本身就是 4 位色阶）；
#   1 = **BGRA8888（32 位）** —— 零售面图（`face/pl0X/face_*.png`，1024×1024）用的就是它 ⇒ 引擎支持。
# ★ 2026-09-16（`AUDIT` §DG-8）：用户手绘的辉光有更细的灰阶（238/221/…），走 16 位会被量化成
#   8 档（实测输入 0→136、221→255，见 §DG-8 的实测表）⇒ 他那一张改用 **format 1**，原样入库。
TRANCE_ENTRY_FORMAT_DEFAULT = 5
TRANCE_ENTRY_FORMAT_BPP = {1: 4, 7: 4}      # 32 位：BGRA8888 / RGBA8888；其余按 16 位算


def _entry_block(ent: dict) -> str:
    """追加 entry 的 spec 文本（整贴图一个 entry、里面若干 sprite）。"""
    w, h = ent["size"]
    fmt = ent.get("format", TRANCE_ENTRY_FORMAT_DEFAULT)
    bpp = TRANCE_ENTRY_FORMAT_BPP.get(fmt, 2)
    lines = [
        f"entry entry{ent['idx']} {{",
        "    version: 8,",
        f'    name: "{ent["name"]}",',
        f"    format: {fmt},",
        f"    width: {w},",
        f"    height: {h},",
        "    memoryPriority: 10,",
        "    lowResScale: 0,",
        "    hasData: 1,",
        f"    THTXSize: {w * h * bpp},",
        f"    THTXFormat: {fmt},",
        f"    THTXWidth: {w},",
        f"    THTXHeight: {h},",
        "    THTXZero: 0,",
        "    sprites: {",
    ]
    for k, sp in enumerate(ent["sprites"]):
        sid, x, y, sw, sh = sp[:5]          # 两类 entry 的前 5 项同形（后面才是来源信息）
        comma = "," if k + 1 < len(ent["sprites"]) else ""
        lines.append(f"        sprite{sid}: {{ x: {x}, y: {y}, w: {sw}, h: {sh} }}{comma}")
    lines += ["    }", "}"]
    return "\n".join(lines)


def _entries_of(player: str):
    """该文件**按追加顺序**排好的 entry 计划（117 的兽灵/刀 → 113 的灵界弹 → 108 的妖侧本体）。

    顺序就是 entry 号：`idx` 必须等于「原生 entry 数 + 前面几项」，`_append_spec` 会硬校验。
    """
    return (PLAYER_ART_ENTRIES.get(player, []) + TH13_TRANCE_ENTRIES.get(player, [])
            + _youkai_body_entries(player) + _yk_extra_entries(player))


def _anmmap_for(game):
    """`game=` 对应的 anmmap（按 **anm 头版本**选，见 `GAME_ANMMAP`）。"""
    p = MAPDIR / GAME_ANMMAP[game]
    if not p.is_file():
        raise SystemExit(f"缺 anmmap {p}（git clone https://github.com/ExpHP/thpages local/vendor/thpages）")
    return p


def _th13_texture(ent: dict):
    """形态 entry 的**源贴图**（只读），三级优先：
      ① `src_override` —— **用户在 `local/` 下给的成品图**（gitignored；用户画好辉光后接管，
         见 `AUDIT` §DG-8）。它已经过变换 + 手绘，所以构建期**不再施加任何处理**；
      ② `src_game="th18"` + `transform` —— TH18 自机图 + 从 TH13 量出的色调变换（咲夜默认档，§DG-7）；
      ③ 默认 —— TH13 原版现切（ground truth，不是可选项）。"""
    from PIL import Image
    ov = ent.get("src_override")
    if ov:
        p = L.REPO / ov
        if not p.is_file():
            print(f"  ⚠ {ent['name']}: 找不到用户覆盖图 {ov} ⇒ 退回默认档"
                  f"（{'变换' if ent.get('transform') else 'TH13 原版现切'}）")
            ov = None
        else:
            print(f"  ★ {ent['name']}: **用户覆盖图** {ov}（构建期零处理，直接用；"
                  f"源矩形按**条带布局**取 = 目标画布同一格）")
            return Image.open(p).convert("RGBA"), "sheet"
    th18 = ent.get("src_game") == "th18"
    root = L.RETAIL_ROOT if th18 else TH13_ANM
    p = root / ent["src_dir"] / ent["src_png"]
    if not p.is_file():
        raise SystemExit(f"缺 TH{'18.v1.00a' if th18 else '13.v1.00c'} 原版贴图 {p}\n"
                         f"（先 `python3 tooling/thtk/unpack.py {'th18.v1.00a' if th18 else 'th13.v1.00c'}`；"
                         f"已解过的话检查 local/ 下那份 anm/ 是否被清过）")
    return Image.open(p).convert("RGBA"), "atlas"


def _quantize16(img):
    """把画布量化到 **16 位贴图（ARGB4444，每通道 4 位）** 的色阶 —— 也就是游戏真正会显示的取值。

    规则 `out = round(v / 17) * 17`（等价于 `(v + 8) // 17 * 17`，与 thtk 的 ARGB4444 编码
    「Use the extra precision for rounding」一致）。零售自机贴图**本来**就全是 17 的倍数
    （实测 TH13/TH18 的自机贴图 100% 是），所以对原版图调用它是**恒等**的；
    对手绘的 8 位图则是"如实告诉你会被量化成什么样"（`AUDIT` §DG-8）。"""
    from PIL import Image
    img = img.convert("RGBA")
    out = Image.new("RGBA", img.size, (0, 0, 0, 0))
    src, dst = img.load(), out.load()
    for y in range(img.size[1]):
        for x in range(img.size[0]):
            r, g, b, a = src[x, y]
            dst[x, y] = (((r + 8) // 17) * 17, ((g + 8) // 17) * 17,
                         ((b + 8) // 17) * 17, ((a + 8) // 17) * 17)
    return out


def _trance_canvas(ent: dict):
    """新 entry 的**期望画布** = TH13 源图按声明矩形现切、原尺寸零补边、**不带 mask** 贴。

    ⚠ 只能用 `canvas.paste(tile, xy)`（不带 mask）。带 mask 时 PIL 做 alpha 合成，会把
      「alpha 小、RGB 有值」的像素预乘成暗色 —— 那就不是原样切图（同
      `assets/ability/make_trance_art.py` 的 `_paste` 注释）。
    """
    from PIL import Image
    src, mode = _th13_texture(ent)
    if mode == "sheet" and src.size != tuple(ent["size"]):
        raise SystemExit(f"{ent['name']}: 用户覆盖图 {src.size} != 声明的 {tuple(ent['size'])}"
                         f"（画布尺寸不许改：24 帧横排、每帧 30×46、步进 30px）")
    canvas = Image.new("RGBA", ent["size"], (0, 0, 0, 0))
    for _sid, x, y, w, h, sx, sy in ent["sprites"]:
        # ★ 覆盖图模式：用户交的是**与本画布同布局**的条带（第 k 帧在 x=30k），
        #   所以源矩形 = 目标矩形；图集模式（TH13/TH18 原版贴图）才用声明里的 atlas 坐标。
        if mode == "sheet":
            sx, sy = x, y
        tile = src.crop((sx, sy, sx + w, sy + h))
        if ent.get("transform") == "trance_form":
            # ★ ❓自定（用户 2026-09-16 定案，`AUDIT` §DG-7）：TH13 没有的自机（咲夜）——
            #   拿她自己的图过一遍**从 TH13 三对原版图量出来的**色调变换（模块里有表与验证）。
            import trance_form_transform as TFT
            tile = TFT.apply(tile)
        canvas.paste(tile, (x, y))
    if ent.get("snap16"):
        canvas = _quantize16(canvas)
    return canvas


def _source_spec(spec: dict, player: str, retail_text: str, tools, anmmap):
    """该 spec 的源 spec 文本 —— TH18 本文件 / TH18 别的自机 / **跨版本原版**
    （`game="th13"` TH13 / `game="th08"` TH08 / `game="th16"` TH16）。

    ⚠ 跨版本的源**直接读 `local/<版本>/dat/<名>.anm`**（不用先 `unpack.py`）：`thanm -l` 能直接吃
      `.dat` 里的 anm 文件，`-m` 用该作头版本对应的 anmmap（`GAME_ANMMAP`）。
    """
    g = spec.get("game")
    if g in ("th08", "th13", "th16"):
        p = GAME_DAT[g] / f"{spec['src']}.anm"
        if not p.is_file():
            raise SystemExit(f"缺 {g} 样本 {p}（用户放进 local/，见 local/README.md）")
        return _spec_of(p, tools, _anmmap_for(g), ver=GAME_VER[g])
    return retail_text if spec["src"] == player else L.retail(spec["src"])[1]


def _assert_spec_source(spec: dict, src_block: str, src_text: str):
    """源头闸：**源脚本必须是计划里那一支**。

    · `smap` 里每个源 sprite 号都得真的被引用（引用别的 = 多半抄错了脚本号）；
    · `sref` 里每个源脚本号都得真的被 `scriptNew`；
    · **跨版本来源**（`game=` 有值）的还要**双向相等** + 「每个 `layer()` 在 `GAME_LAYERS` 里」——
      层号没登记就报错，逼后来者先查清目标版本对应哪一层，而不是照抄源版的数字。
    """
    macro = spec["macro"]
    refs = {int(m.group(1)) for m in _SPRITE_REF.finditer(src_block)}
    unused = sorted(set(spec["smap"]) - refs)
    if unused:
        raise SystemExit(f"{macro}: 源 {spec['src']}.anm script{spec['sid']} 没引用 sprite {unused}"
                         f"（计划 smap 里登记的）—— 源脚本号或 sprite 号抄错了")
    sn = {int(m.group(1)) for m in _SCRIPT_NEW.finditer(src_block)}
    unused_sn = sorted(set(spec.get("sref", {})) - sn)
    if unused_sn:
        raise SystemExit(f"{macro}: 源 script{spec['sid']} 没有 scriptNew 到 {unused_sn}"
                         f"（计划 sref 里登记的）")
    g = spec.get("game")
    if g not in ("th08", "th13", "th16"):
        return
    if refs != set(spec["smap"]) or sn != set(spec.get("sref", {})):
        raise SystemExit(f"{macro}: {g} 源 script{spec['sid']} 的引用 {sorted(refs)}/{sorted(sn)}"
                         f" 与计划 {sorted(spec['smap'])}/{sorted(spec.get('sref', {}))} 不是一一对应"
                         f"（{g} 来源要求精确对齐，否则贴图/子 VM 会静默错位）")
    layers = {int(m.group(1)) for m in _LAYER_CALL.finditer(src_block)}
    allowed = GAME_LAYERS[g]
    if allowed is None:                              # th13：走 LAYER_MAP 翻译
        unknown = sorted(layers - set(LAYER_MAP))
        if unknown:
            raise SystemExit(f"{macro}: TH13 源 script{spec['sid']} 用了未登记的 TH13 层号 {unknown}"
                             f"（LAYER_MAP 只有 {sorted(LAYER_MAP)}）—— 先在 TH18 里找出对应层，"
                             f"再往 LAYER_MAP 加一行，别照抄 TH13 的数字")
    elif layers - allowed:
        # th16：与 TH18 同一个层空间，本体脚本只允许 `layer(14)`（= TH18 原生本体脚本的层）；
        # th08：本体脚本 5..9 **一个 layer() 都没有**（层由 VM 保留）。这一支红了说明源脚本抄错了。
        raise SystemExit(f"{macro}: {g} 源 script{spec['sid']} 的 layer() = {sorted(layers)}，"
                         f"不在该来源允许的 {sorted(allowed)} 里 —— 层号要显式登记（`GAME_LAYERS`）")


def _inject_body_prologue(body: str) -> str:
    """给「从 TH08 搬来的本体脚本」补上与 **TH18 原生本体脚本逐字相同**的前导。

    一手：TH18 原生 `pl00/pl01/pl02/pl03.anm` 的**每一支**本体脚本（0..4）都以
    `-1: type(0); layer(14); resampleMode(1); 0:` 开头；而**装载新脚本会把整块模板拷进 VM**
    （`AnmVm__load_script` `0x407420`：`rep movs` 0x151 dword 从 `[anm+0x10c]+idx*0x60c` 到 `vm+0`，
    只把 `vm+0x18`(层) 与 `vm+0x548` 存下来再写回）⇒ 前导不跟着来就会留在模板的默认值上。
    TH08 的本体脚本 5..9 没有前导（TH08 的层/type 由 VM 保留）⇒ 这一段是**必需的搬运**，不是自定。
    `0:` 之后紧跟源脚本自己的第一行（TH08 script5 是 `offset0:` 标签 ⇒ 两个标签同刻，thanm 允许 ——
    TH18 原生 script0 就是 `0:` 紧跟 `offset36:`）。
    """
    m = re.match(r"(script script\d+ \{\n)", body)
    if not m:
        raise SystemExit("注入本体前导失败：脚本头没找到")
    pro = "-1:\n    type(0);\n    layer(14);\n    resampleMode(1);\n0:\n"
    return body[:m.end()] + pro + body[m.end():]


def _inject_hud_layer(body: str, layer: int) -> str:
    """给「追加进 `pl0X.anm` 的 **HUD** 脚本」补 `layer(N);` **+ `resolutionMode(2);`**（2026-09-23 立、
    2026-09-26 补后半句 —— 见下面「★ 陷阱」）。

    为什么必需（一手）：`AnmManager__render_layer` `0x4882b8` 的 UI 列表那一段把 layer 24..32
    折到绘制组 layer+13、**其余任何层一律兜底折到组 39**；而暂停模糊罩 = `text.anm` script59 的
    UI VM（层 38 ⇒ 组 38）⇒ **只有 24/25 会被罩住**（`HUD-LAYOUT.md` §4.1，硬规则）。
    TH08 的 `ascii.anm` 那几支 HUD 脚本**原文就没有 `layer()`**（TH08 的层由 VM 保留，
    与本体脚本同一情形）⇒ 直接搬进 TH18 会落到兜底组 39 ⇒ 暂停时**我们的 HUD 比原版 HUD 更清晰**
    （观感差异；同一条规则害过 99/104/105 三支，它们写的是 26 也照红过）。
    ⇒ TH18 侧一律 24 或 25（推荐 25，与 `ce_hud_vm_text_*` 一致）。

    ★★★ **陷阱（2026-09-26，owner #39「人妖槽我是完全看不到」的真根因，一手逐条）**：
    `layer()` **不是只写 `vm+0x18`** —— 它调 `set_layer` `th18:0x4066b0`，那个函数还会**派生
    `flags_hi(+0x538)` 的位**，而 `resolutionMode` 就住在同一批位里（掩码 `0x1c00000` = bits 22–24）：

    ```
    40667b MOVZX EAX,byte [EDI+8] / PUSH EAX / CALL 0x4066b0      ; 304 layer 的 handler
    4066b0 set_layer: [ecx+0x18] = layer
    4066b6..4066da  3..19   → flags_hi = (f & ~0x200000) | 0x100000      ; bit20（不在 resolutionMode 掩码里）
    4066dc..4066fd  20..23  → flags_hi = (f & ~0x100000) | 0x200000      ; bit21（同上，无害）
    4066fd          其余    → flags_hi &= ~0x300000
    406708..406726  layer ∈ 20..32 **或** 37..45 → flags_hi = (f & ~0x1800000) | 0x400000  ; ★ bit22！
    ```
    `0x400000` = `resolutionMode` 的**模式 1** ⇒ `th18:0x4063d0` 那一步把 VM 的位置**三个分量全体
    × `DAT_0056aca0`**（1280×960 下 = **×2**；尺寸那一支在 `th18:0x4817c6` 同样 ×2）
    ⇒ 我们 C 侧按**表面 px** 写的 `(248,864)` 会被画到 `(496,1728)` —— **屏只有 960 高 ⇒ 整个人妖槽
    一个像素都不显示**。这就是「脚本里加了 `layer(25)` 之后 HUD 又看不见」的全部原因
    （`0bc213c` 注入层 → 之后每一版 owner 都报看不见；`youkai/hud.vm` 的诊断行把 `pos+pos2+ent`
    当成了「表面坐标」，而那个和**还要再乘一次 DAT**，所以日志看着一直是对的）。

    ⇒ 修法：**层之后立刻写回 `resolutionMode(2)`**（= `(f & ~0x1c00000) | 0x800000`，`0x47b5de` 的
    handler）—— 模式 2 在 1280×960 下是 `DAT × 0.5 = ×1`，也就是「脚本没写 `resolutionMode` 时」的
    那套表面 px 口径 ⇒ **C 侧的数值一个都不用动**。112 的飞碟槽 / 113 的灵界槽本来就是
    `layer(25|24); resolutionMode(2);` 这个顺序（它们看得见正是因为这个）。⚠ 顺序不能反。
    一手全文与实跑合证见 `engine/anm/th18/01-vm-instantiate.md` §3c。

    插入点：脚本头之后的第一条**指令**之前。若头后面紧跟的是标签（`-1:` / `0:` 那种）就插到标签之后 ——
    thanm 的 `layer()` 是脚本级指令，放进 `-1:` 块里会改不到 VM 的层。
    """
    m = re.match(r"(script script\d+ \{\n)", body)
    if not m:
        raise SystemExit("注入 HUD 层失败：脚本头没找到")
    head, rest = body[:m.end()], body[m.end():]
    # ⚠ 注入的正文里**不许写注释**：`_verify_appended` 会拿计划正文与「编译后再反解回来」的正文
    #   逐字符比（注释在往返里必然丢）⇒ 加一行 `//` 就会假红。理由全写在本函数的 docstring 里。
    note = f"    layer({layer});\n    resolutionMode(2);\n"
    lm = re.match(r"[ \t]*(-?\d+:)[ \t]*\n", rest)
    if lm:                                            # 头后面是标签 ⇒ 插到标签之后
        return head + rest[:lm.end()] + note + rest[lm.end():]
    return head + note + rest



def _zero_script_pos(body: str) -> str:
    """★ 2026-09-23（#30「人妖槽又看不见」）：把脚本自带的 `pos(...)` **全部删掉**并改成 `pos(0,0,0)`。

    为什么：人妖槽那四支（+字形）是从 TH08 `ascii.anm` 搬来的，**自带 `pos(32,449)` 一类的站位**；
    而我们 C 侧每帧又用 `ce_anm_set_pos()` 写 `entity_pos(+0x5f0)` 摆位 ⇒ **绘制点 = 脚本 pos + 实体 pos**
    （一手 `th18:0x48171f`）⇒ **y 被加了两次**。实跑日志（`youkai/hud.vm`）给的是铁证：
      `bar: pos(0x30)(32,449) + ent(0x5f0)(257,860) = 表面 (289,1309)` —— 屏幕只有 960 高。
    ⇒ 与 111/112 的 HUD 同一口径：**脚本 pos 归零、位置全归 C 侧**（它们的脚本本来就写着 `pos(0,0,0)`）。
    """
    body = re.sub(r"^[ \t]*posTime\([^\n]*\);\n", "", body, flags=re.M)   # ★ 连 posTime 一起删
    body = re.sub(r"^[ \t]*pos\([^\n]*\);[ \t]*\n", "", body, flags=re.M)
    m = re.match(r"(script script\d+ \{\n)", body)
    return body if not m else body[:m.end()] + "    pos(0.0f, 0.0f, 0.0f);\n" + body[m.end():]

def _append_spec(player: str, retail_text: str, tools, anmmap) -> str:
    """按 PLAYER_ART_ENTRIES / TH13_TRANCE_ENTRIES / YOUKAI_BODY / APPEND_PLAN 生成追加后的 spec
    （纯函数，编译前先自检）。"""
    text = retail_text
    n_native_e = len(L.parse_entries(retail_text))
    for k, ent in enumerate(_entries_of(player)):
        want = n_native_e + k
        if ent["idx"] != want:
            raise SystemExit(f"{player} 原生 entry 数 = {n_native_e}，第 {k + 1} 个追加 entry 应编号 "
                             f"{want}，但计划写 {ent['idx']}（两处必须同步）")
        text = L.insert_entries(text, [_entry_block(ent)])
    n_scripts = len(L.parse_scripts(retail_text))
    blocks = []
    for spec in APPEND_PLAN[player]:
        macro, src_sid, new_sid = spec["macro"], spec["sid"], spec["new_sid"]
        # 追加脚本号 = 出现顺序 ⇒ 必须**正好**接在已排好的后面（不连续会让 pl_anm_ids.h 里的
        # 宏指向别的脚本，而 sht 正是照那个号发弹 —— 那是最难查的一类静默错）。
        if new_sid != n_scripts + len(blocks):
            raise SystemExit(f"{player}: {macro} 计划 script{new_sid}，但下一条必须是 "
                             f"script{n_scripts + len(blocks)}（脚本号必须**连续**追加）")
        src_text = _source_spec(spec, player, retail_text, tools, anmmap)
        src_block = _script_block(src_text, src_sid)
        if not src_block:
            raise SystemExit(f"{player}: 源 {spec.get('game', 'th18')}/{src_sid} 没有 script{src_sid}")
        _assert_spec_source(spec, src_block, src_text)
        body = _scale_block(src_block, spec["k"])
        body = _replace_sprites(body, spec["smap"])
        if spec.get("sref"):
            body = _replace_script_new(body, spec["sref"])
        if spec.get("game") == "th13":
            body = _remap_layer(body, LAYER_MAP)     # 跨版本层号翻译（表见 LAYER_MAP）
        if spec["alpha255"]:
            body = body.replace("alpha(192)", "alpha(255)")
        if spec.get("zero_pos"):
            body = _zero_script_pos(body)            # ★ #30：脚本 pos 归零（位置全归 C 侧；否则与锚点叠加 ⇒ y 翻倍画到屏外）
        if spec["scale_inject"]:
            body = _inject_scale(body, spec["k"])
        if spec.get("th08_opcodes"):
            body = _th08_opcodes(body)               # TH08(v2) → TH18(v8) 的 opcode 翻译
        if spec.get("prologue"):
            body = _inject_body_prologue(body)       # TH08 本体脚本的前导（见该函数注释）
        if spec.get("hud_layer"):
            body = _inject_hud_layer(body, spec["hud_layer"])   # HUD 脚本必须自带层，见该函数
        body = _rename_script(body, src_sid, new_sid)
        blocks.append(body)
    return L.append_scripts(text, blocks) if blocks else text


def _verify_appended(player: str, text: str, planned_text: str):
    """回读核对：原生部分没变（`L.verify_rebuilt` 已做）+ 追加脚本的 sprite/scriptNew 全在界内
    + 会飞的主弹脚本普通路径无 delete() + **编出来的追加脚本与计划逐字符一致**。
    -> (n_sprites, n_scripts, used)。

    ★ 最后那条是 113 加的：它把整条翻译链（源脚本 → sprite/scriptNew 重编号 → `layer()` 版本
    翻译 → 改名）**端到端**钉住。只查说"引用的 sprite/script 存在"是不够的 —— 层号翻错、
      或漏翻一处 `layer()`，引用一样全部合法，但画面是错的。逐字符比 `_script_block` 才管得住。
    """
    sprites = _sprite_ids(text)
    n_sprites = (max(sprites) + 1) if sprites else 0
    scripts = _scripts(text)
    used = {}
    for spec in APPEND_PLAN[player]:
        macro, new_sid = spec["macro"], spec["new_sid"]
        body = scripts.get(new_sid)
        if not body:
            raise SystemExit(f"{player}.anm: 追加脚本 script{new_sid}（{macro}）不存在")
        want_block = _script_block(planned_text, new_sid)
        got_block = _script_block(text, new_sid)
        if _norm_labels(want_block or "").rstrip() != _norm_labels(got_block or "").rstrip():
            raise SystemExit(f"{player}.anm: {macro} script{new_sid} 与计划不一致 —— "
                             f"文件里的内容不是 `_append_spec` 生成的那一份"
                             f"（层号翻译 / sprite 重编号 / 源脚本号有一处对不上）")
        refs = sorted({int(m.group(1)) for m in _SPRITE_REF.finditer(body)})
        bad = [s for s in refs if s not in sprites or s >= n_sprites]
        if bad:
            raise SystemExit(f"{player}.anm: {macro} script{new_sid} 引用越界 sprite {bad}")
        sn = sorted({int(m.group(1)) for m in _SCRIPT_NEW.finditer(body)})
        miss = [s for s in sn if s not in scripts]
        if miss:
            raise SystemExit(f"{player}.anm: {macro} script{new_sid} 的 scriptNew 指向不存在的 {miss}")
        if new_sid in APPEND_FLYING.get(player, ()):
            main = body.split("interruptLabel", 1)[0]
            if "delete(" in main:
                raise SystemExit(f"{player}.anm: {macro} script{new_sid} 普通飞行路径含 delete()")
        used[new_sid] = refs
    return n_sprites, len(scripts), used


def _assert_pinned_numbers():
    """`PINNED_NEW_SIDS` ↔ `APPEND_PLAN` 逐条对账（见该表的注释）。

    只比**登记在表里**的那几个宏 —— pl03 另有 117 留下的 FROG/SNAKE/GLINT，不归本表管。
    """
    for player, want in PINNED_NEW_SIDS.items():
        got = {s["macro"]: s["new_sid"] for s in APPEND_PLAN.get(player, [])}
        off = {m: (got.get(m), v) for m, v in want.items() if got.get(m) != v}
        if off:
            raise SystemExit(f"{player}: 追加脚本号与外部约定不一致 —— " +
                             "、".join(f"{m} 计划={a} 钉死={b}" for m, (a, b) in sorted(off.items())) +
                             "。要么改回计划，要么**同时**改 `assets/sht/append_shooterset.py` 的 "
                             "PLxx_SCRIPT_TRANCE 与 `check_pl_anm_ids()`（否则 sht 会指到别的脚本）。")


def write_pl_ids_h():
    _assert_pinned_numbers()
    lines = ["/* 由 assets/build_player_anm.py 生成 —— pl0X.anm 追加脚本号。别手改。 */",
             "#pragma once", ""]
    for player in sorted(APPEND_PLAN):
        for spec in APPEND_PLAN[player]:
            lines.append(f"#define CE_PL_ANM_SCRIPT_{spec['macro']:<16} {spec['new_sid']:>3}   "
                         f"/* {player}.anm 追加 */")
    # ★ 2026-09-16：**113 灵界形态（白剪影）的 sprite 偏移**。C 侧（`cards/trance.c` 的
    #   `trance_form_tick`）拿它把本体脚本写的帧号 0..23 换成我们追加的 24 张形态帧
    #   ⇒ 这里导出「追加 entry 的首个 sprite 号」= 加数，别让 C 侧手写常数（改图就会错位）。
    lines.append("")
    for player in sorted(TH13_TRANCE_ENTRIES):
        for ent in TH13_TRANCE_ENTRIES[player]:
            if "form" not in ent["name"]:
                continue
            first, last = ent["sprites"][0][0], ent["sprites"][-1][0]
            lines.append(f"#define CE_PL_ANM_FORM_BASE_{player.upper():<5} {first:>3}   "
                         f"/* {player}.anm 灵界形态 sprite {first}..{last}"
                         f"（{len(ent['sprites'])} 帧）= 本体帧号 0..{len(ent['sprites']) - 1} 的加数 */")
    # ★★ 2026-09-22（108 路 A）：**妖侧本体帧的 sprite 区间**。C 侧（`cards/youkai.c` 的
    #   `yk_body_sync`）拿它做一个**自愈闸**：我们装上去的那支脚本只可能写这个区间里的 sprite，
    #   一旦在低速下看到区间外的号，就说明**引擎把脚本换回去了**（例：死亡重生走
    #   `Player__initialize` 又装了人侧 script0）⇒ 立刻重装，而不是等下一次移动状态变化。
    #   （引擎自己那套本体脚本用的是 sprite 0..23 ⇒ 两个区间天然不重叠。）
    lines.append("")
    for player in sorted(YOUKAI_BODY_IDS):
        ent = _youkai_body_entries(player)[0]
        first, last = ent["sprites"][0][0], ent["sprites"][-1][0]
        lines.append(f"#define CE_PL_ANM_YKBODY_FIRST_{player.upper():<5} {first:>3}   "
                     f"/* {player}.anm 妖侧本体帧 sprite {first}..{last}"
                     f"（{len(ent['sprites'])} 帧）*/")
        lines.append(f"#define CE_PL_ANM_YKBODY_LAST_{player.upper():<6} {last:>3}   "
                     f"/* 区间外 = 引擎把本体脚本换回去了（自愈闸，见 youkai.c）*/")
    # ★★ 2026-09-22 晚（owner：「th08 的贴图搞过来」）：**人妖槽百分比字形的 sprite 基号**。
    #   C 侧（`youkai.c` 的 `yk_gauge_text_*`）按 TH08 `AsciiManager::DrawPercentage` 的布局逐格
    #   `ce_anm_set_sprite(pl0X.anm, id, BASE + 序号)` —— 序号顺序 = TH08 的 sprite 号序：
    #   0..9 = '0'..'9'、10 = '%'、11 = '.'、12 = '-'、13 = '+'（= 原生 0x88..0x94）。
    #   写死常数会在「重新生成 / 插一格」时静默错位（画出来是别的字），所以从计划表现算。
    lines.append("")
    for player in sorted(YK_EXTRA_IDS):
        ent = [e for e in _yk_extra_entries(player) if "gaugefont" in e["name"]][0]
        first = ent["sprites"][0][0]
        lines.append(f"#define CE_PL_ANM_YKGAUGEFONT_BASE_{player.upper():<5} {first:>3}   "
                     f"/* {player}.anm 人妖槽字形 sprite {first}..{first + 12}"
                     f"（0..9 = '0'..'9'、10 = '%'、11 = '.'、12 = '-'、13 = '+'）*/")
    PL_IDS_H.write_text("\n".join(lines) + "\n", encoding="utf-8")


def _scripts(text: str):
    return {int(m.group(1)): m.group(2)
            for m in re.finditer(r"^script script(\d+) \{\n(.*?)^\}",
                                 text, re.M | re.S)}


# `offsetNN` 里的 NN 是**汇编器按字节偏移重算**出来的：往脚本头注入前导（`_inject_body_prologue`）
# 之后，同一支脚本的标签号必然整体平移。比较「计划 vs 编出来的」时把它归一掉；**其余逐字符比**
# （opcode / 时间 / sprite 号一个都不放过）⇒ 注入前导不会让这道闸失效。
_LABEL = re.compile(r"\boffset\d+\b")


def _norm_labels(block: str) -> str:
    return _LABEL.sub("offset#", block)


def _script_block(text: str, sid: int):
    """整块（含 `script scriptNN {` 头）—— 追加时要连头一起改名。"""
    m = re.search(rf"^script script{sid} \{{\n.*?^\}}", text, re.M | re.S)
    return m.group(0) if m else None


def _sprite_ids(text: str):
    ids = set()
    for m in _SPRITE_DEF.finditer(text):
        ids.add(int(m.group(1)))
    return ids


def _verify_scripts(player: str, text: str, targets: dict, label: str):
    """硬校验：目标脚本存在 + sprite / scriptNew 引用全部在界内。-> (n_sprites, used)"""
    sprites = _sprite_ids(text)
    n_sprites = (max(sprites) + 1) if sprites else 0
    scripts = _scripts(text)
    used = {}
    for anm, sid in sorted(targets.items()):
        if sid not in scripts:
            raise SystemExit(f"{player}.anm: {label} anm={anm} -> script{sid} 不存在"
                             f"（文件只有 {len(scripts)} 支脚本：{sorted(scripts)[:5]}…）")
        body = scripts[sid]
        refs = sorted({int(m.group(1)) for m in _SPRITE_REF.finditer(body)})
        bad = [s for s in refs if s not in sprites or s >= n_sprites]
        if bad:
            raise SystemExit(
                f"{player}.anm: {label} script{sid} 引用不存在的 sprite {bad}"
                f"（该文件 sprite 总数 {n_sprites}，id 最大 {max(sprites) if sprites else -1}）")
        sn = sorted({int(m.group(1)) for m in _SCRIPT_NEW.finditer(body)})
        miss = [s for s in sn if s not in scripts]
        if miss:
            raise SystemExit(f"{player}.anm: {label} script{sid} 的 scriptNew "
                             f"指向不存在的 script {miss}")
        # 会飞的主弹脚本：普通飞行路径（第一个 interruptLabel 之前）绝不能有 delete()，
        # 否则 TH18 的「VM 没了 = 弹没了」会让射程变短（AUDIT §AL28/§AL33）。
        if sid in FLYING_SCRIPTS.get(player, ()):
            main = body.split("interruptLabel", 1)[0]
            if "delete(" in main:
                raise SystemExit(f"{player}.anm: {label} script{sid} 的普通飞行路径含 delete()")
        used[sid] = refs
    return n_sprites, len(scripts), used


def _spec_of(path: Path, tools, anmmap, ver: str = None):
    """解 spec。`ver` = thanm `-l` 的版本参数（默认本作 TH18；跨版本源要显式传 `GAME_VER`）。

    ⚠ anmmap 与 `-l` 是两件事：anmmap 按 **anm 文件头 version** 选（TH13/TH16/TH18 的头都是 8
      ⇒ 同一张 `v8.anmm`；**TH08 的头是 3 ⇒ `v2.anmm`**，见 `GAME_ANMMAP`）。

    ⚠ 这里**自己**跑 `thanm` 而不是复用 `L.thanm`：后者把「stderr 非空」当失败（防静默译错，
      对构建是对的），但 TH08 的 `v2.anmm` 每次都会打一行**无害**的 mapfile 解析警告
      （`'acos' is a keyword, ignoring` = mapfile 里那条注释）⇒ 只放行**这一条已知警告**，
      其余 stderr 照旧报错。
    """
    args = ["-l", ver or L.VERSION, str(path), "-m", str(anmmap)]
    r = subprocess.run([str(tools[0]), *args], cwd=path.parent, capture_output=True, text=True)
    err = "\n".join(l for l in r.stderr.splitlines()
                    if l.strip() and not any(b in l for b in _THANM_BENIGN))
    if r.returncode != 0 or err.strip():
        raise SystemExit(f"thanm {' '.join(map(str, args))} 失败：\n{err or r.stdout}")
    return r.stdout


def _assert_sources(player: str, text: str, tools, anmmap):
    """TH17 源与 TH18 原生的兽灵脚本必须逐字节一致（这是「直接用原生」的硬前提）。"""
    src = TH17_DAT / f"{player}.anm"
    if not src.is_file():
        raise SystemExit(f"缺 TH17 样本 {src}")
    src_text = _spec_of(src, tools, anmmap)
    a, b = _scripts(src_text), _scripts(text)
    for sid in sorted(BEAST_SCRIPTS[player] | {HIT_FX_SCRIPT}):
        if sid not in a:
            raise SystemExit(f"TH17 {player}.anm 没有 script{sid}")
        if sid not in b:
            raise SystemExit(f"TH18 {player}.anm 没有 script{sid}")
        if a[sid] != b[sid]:
            raise SystemExit(
                f"{player}.anm: script{sid} 在 TH17 与 TH18 原生之间不一致 —— "
                f"不能直接引用原生脚本，需要重新评估追加方案")


def _do_native(player: str, a, tools, anmmap):
    """pl00/pl01/pl02：原样拷贝 + 硬校验。pl00/pl01 还要与 TH17 兽灵脚本逐字节一致；
    pl02（咲夜）没有 TH17 对应自机 ⇒ 只做 identity 脚本存在性 + sprite 界内校验。"""
    retail_anm = TH18_DAT / f"{player}.anm"
    out_anm = OUT / f"{player}.anm"
    text = _spec_of(retail_anm, tools, anmmap)
    if player in BEAST_SCRIPTS:
        _assert_sources(player, text, tools, anmmap)
        targets = {anm: TH17_ANM_TO_SCRIPT[anm] for anm in (5, 6, 7, 8, 11)}
        label = "TH17 兽灵脚本"
    else:
        targets = dict(FALLBACK_ANM_TO_SCRIPT)
        label = "原生拷贝（identity 校验）"
    n_sprites, n_scripts, used = _verify_scripts(player, text, targets, label)
    flat_used = sorted({s for refs in used.values() for s in refs})
    if not a.verify_only:
        shutil.copyfile(retail_anm, out_anm)
    if not out_anm.is_file():
        raise SystemExit(f"缺 {out_anm}：先跑 python3 assets/build_player_anm.py")
    if out_anm.read_bytes() != retail_anm.read_bytes():
        raise SystemExit(f"{player}.anm: native/build 与原生不一致 —— 绝不允许追加/改动")
    print(f"  {player}: {n_sprites} sprites, {n_scripts} scripts, 不改动；"
          f"{label} " + " ".join(f"{k}->{v}" for k, v in sorted(targets.items())) +
          f"；用到 sprite {flat_used}")


def _body_canvas(ent: dict):
    """妖侧本体 entry 的**期望画布** = 55 张源 PNG **原样**（含透明补边）按帧序横排。

    ⚠ 与 `_trance_canvas` 同一条纪律：只 `canvas.paste(tile, xy)`（**不带 mask**）。
    零缩放、零改色、零补边 ⇒ 回读闸才能要求**逐像素相等**；sprite 矩形由 `ent["sprites"]` 给出
    （写的是**原版**的有效画区，不是画布单格 —— 见 `YOUKAI_BODY` 上方那段）。
    """
    from PIL import Image
    cw, ch = ent["cell"]
    canvas = Image.new("RGBA", ent["size"], (0, 0, 0, 0))
    for k, (prefix, sub, sid) in enumerate(ent["cells"]):
        p = YOUKAI_BODY_SRC / sub / f"{prefix}_S{sid}.png"
        if not p.is_file():
            raise SystemExit(f"缺妖侧本体源图 {p}（108 的 55 张，见 YOUKAI_BODY 的注释）")
        tile = Image.open(p).convert("RGBA")
        if tile.size != (cw, ch):
            raise SystemExit(f"{p.name}: 源图 {tile.size} != 声明的单格 {cw}x{ch}"
                             f"（改图别改画布尺寸）")
        canvas.paste(tile, (cw * k, 0))
    return canvas


def _yk_extra_canvas(ent: dict):
    """108 的 HUD / 刻符 entry 的**期望画布**：从 TH08 解包目录按声明的源矩形现切、1:1 贴。

    与 `_body_canvas` 同一条纪律：只 `paste`（不带 mask）、零缩放零改色 ⇒ 回读闸要求逐像素相等。
    ⚠ 源目录要 `python3 tooling/thtk/unpack.py th08.v1.00d`（本仓 TH13 那条路同款要求）。
    """
    from PIL import Image
    canvas = Image.new("RGBA", ent["size"], (0, 0, 0, 0))
    for (_sid, x, y, w, h, src_anm, src_png, sx, sy) in ent["sprites"]:
        p = TH08_ANM / src_anm / src_png
        if not p.is_file():
            raise SystemExit(f"缺 TH08 解包贴图 {p}\n"
                             f"（先 `python3 tooling/thtk/unpack.py th08.v1.00d`）")
        tile = Image.open(p).convert("RGBA").crop((sx, sy, sx + w, sy + h))
        if tile.size != (w, h):
            raise SystemExit(f"{p.name}: 源矩形 ({sx},{sy},{w},{h}) 切不出来")
        canvas.paste(tile, (x, y))
    return canvas


def _yk_extra_readback(player: str, td: Path):
    """回读闸：从编出来的 anm 里解出来的 HUD / 刻符 entry 贴图，必须逐像素等于 TH08 原版切图。"""
    from PIL import Image, ImageChops
    for ent in _yk_extra_entries(player):
        p = td / ent["name"]
        if not p.is_file():
            raise SystemExit(f"{player}.anm: 回读不到追加 entry 贴图 {ent['name']}")
        got = Image.open(p).convert("RGBA")
        want = _yk_extra_canvas(ent)
        if got.size != want.size:
            raise SystemExit(f"{player}.anm: entry{ent['idx']} 贴图 {got.size} != 期望 {want.size}")
        if ImageChops.difference(got, want).getbbox() is not None:
            raise SystemExit(f"{player}.anm: entry{ent['idx']}（108 HUD/刻符）的贴图与 TH08 原版"
                             f"切图不一致 —— 切图/编译链路改坏了像素")


def _th08_opcodes(body: str) -> str:
    """TH08（v2）→ TH18（v8）的 **opcode 翻译**（只翻名字真的变了的那些）。

    一手：`engine/anm/OVERVIEW.md` 的跨版本锚点表 —— v2 的 `22 anchorTopLeft`（无参）在 v8 里
    没有了，v8 是带参的 `421 anchor(h, v)`，取值 `0 = 中 / 1 = 左(顶) / 2 = 右(底)`
    ⇒ `anchorTopLeft()` ≡ `anchor(1, 1)`。**别的 opcode 一个都不动**（脚本里用到的其余指令
    `sprite/zWriteDisable/alpha/alphaTime/colorTime/pos/posTime/stop/stopHide/interruptLabel/static`
    v8 全有：`stopHide` 见 TH18 原生 `text.anm`、`static` 见 `bullet.anm`）。
    """
    return body.replace("anchorTopLeft();", "anchor(1, 1);")


def _body_readback(player: str, td: Path):
    """回读闸：从**编出来的** anm 里解出来的本体 entry 贴图，必须逐像素等于 55 张源图拼的画布。"""
    from PIL import Image, ImageChops
    for ent in _youkai_body_entries(player):
        p = td / ent["name"]
        if not p.is_file():
            raise SystemExit(f"{player}.anm: 回读不到追加 entry 贴图 {ent['name']}")
        got = Image.open(p).convert("RGBA")
        want = _body_canvas(ent)
        if got.size != want.size:
            raise SystemExit(f"{player}.anm: entry{ent['idx']} 贴图 {got.size} != 期望 {want.size}")
        if ImageChops.difference(got, want).getbbox() is not None:
            raise SystemExit(f"{player}.anm: entry{ent['idx']}（妖侧本体）的贴图与源图不一致 —— "
                             f"切图/编译链路改坏了像素（这一批要求 1:1 零处理）")


def _write_entry_textures(player: str, tex: Path):
    """把追加 entry 的贴图写进编译用的 tex 目录（三类来源，见文件头与三个 ENTRIES 表）。"""
    from PIL import Image
    for ent in PLAYER_ART_ENTRIES.get(player, []):
        canvas = Image.new("RGBA", ent["size"], (0, 0, 0, 0))
        for _sid, x, y, w, h, key in ent["sprites"]:
            src = PA.ensure(key)              # 缺图则用零售+默认色自动生成（不红）
            if src.size != (w, h):
                raise SystemExit(f"{key}: 源图 {src.size} != 声明的 {w}x{h}（改图别改画布尺寸）")
            canvas.paste(src, (x, y))
        dst = tex / ent["name"]
        dst.parent.mkdir(parents=True, exist_ok=True)
        canvas.save(dst)
    for ent in TH13_TRANCE_ENTRIES.get(player, []):
        dst = tex / ent["name"]
        dst.parent.mkdir(parents=True, exist_ok=True)
        _trance_canvas(ent).save(dst)         # TH13 原版图现切（1:1、零补边）
    for ent in _youkai_body_entries(player):
        dst = tex / ent["name"]
        dst.parent.mkdir(parents=True, exist_ok=True)
        _body_canvas(ent).save(dst)           # 108 妖侧本体（源图原样横排，1:1、零补边）
    for ent in _yk_extra_entries(player):
        dst = tex / ent["name"]
        dst.parent.mkdir(parents=True, exist_ok=True)
        _yk_extra_canvas(ent).save(dst)       # 108 HUD / 刻符（TH08 原版现切，1:1、零补边）


def _trance_readback(player: str, td: Path):
    """回读闸：从**编出来的** anm 里解出来的新 entry 贴图，必须逐像素等于 TH13 原版切图。

    这是端到端的：`thanm -c`（PNG→THTX）→ `thanm -x`（THTX→PNG）走的是引擎同一条格式链，
    所以「贴图真的进了文件、且一个像素没被改」是被证明过的，而不是靠「我们刚刚写过」。
    """
    from PIL import Image, ImageChops
    for ent in TH13_TRANCE_ENTRIES.get(player, []):
        p = td / ent["name"]
        if not p.is_file():
            raise SystemExit(f"{player}.anm: 回读不到追加 entry 贴图 {ent['name']}")
        got = Image.open(p).convert("RGBA")
        want = _trance_canvas(ent)
        if got.size != want.size:
            raise SystemExit(f"{player}.anm: entry{ent['idx']} 贴图 {got.size} != 期望 {want.size}")
        if ImageChops.difference(got, want).getbbox() is not None:
            raise SystemExit(f"{player}.anm: entry{ent['idx']} 的贴图与**期望画布**不一致"
                             f"（切图/编译链路改坏了像素，或在中间做了不该做的变换）"
                             f"；期望画布源 = "
                             f"{ent['src_override'] if ent.get('src_override') else (ent['transform'] if ent.get('transform') else 'TH13 原版现切')}"
                             f"（{ent['src_png']}）")


def _do_appended(player: str, a, tools, anmmap):
    """pl00/pl01/pl02/pl03：用 anmlib 重建 = 原生部分 + 追加 entry/脚本。"""
    retail_dir, retail_text = L.retail(player)
    out_anm = OUT / f"{player}.anm"
    n_native_s = len(L.parse_scripts(retail_text))
    for spec in APPEND_PLAN[player]:
        if spec["new_sid"] < n_native_s:
            raise SystemExit(f"{player}: 追加脚本号 script{spec['new_sid']} 与原生脚本区间冲突")
    # ★ 117 的兽灵侧一个字不改：pl00/pl01 的原生文件里就**逐字节**含 TH17 兽灵脚本，
    #   这两道闸原来在 `_do_native` 里跑；113 之后这两个文件也走「重建」路径，
    #   所以把闸搬到这里 —— 绝不能因为换了路径就把校验丢掉。
    if player in BEAST_SCRIPTS:
        _assert_sources(player, retail_text, tools, anmmap)
        _verify_scripts(player, retail_text,
                        {anm: TH17_ANM_TO_SCRIPT[anm] for anm in (5, 6, 7, 8, 11)},
                        "TH17 兽灵脚本")
    # ★ `_append_spec` 是「编译前的自检」，所以**两种模式都要跑**（不能只在构建时跑）：
    #   它管的是「源脚本 ↔ 计划表对齐」与「TH13 层号全都翻译过」—— 这两条恰恰是
    #   `make check` → `anm-verify` → `--verify-only` 那条路上唯一能拦住跨版本抄错的地方。
    spec_text = _append_spec(player, retail_text, tools, anmmap)
    if not a.verify_only:
        tex = L.BUILD / f"tex-player-{player}"
        L.fresh_dir(tex)
        L.link_retail_textures(tex, retail_dir, L.parse_entries(retail_text))
        _write_entry_textures(player, tex)
        L.compile_anm(spec_text, tex, out_anm, tools, anmmap)
    if not out_anm.is_file():
        raise SystemExit(f"缺 {out_anm}：先跑 python3 assets/build_player_anm.py")
    built_text, n_e, n_s = L.verify_rebuilt(out_anm, retail_text, retail_dir, tools, anmmap,
                                           on_extract=lambda td: (_trance_readback(player, td),
                                                                  _body_readback(player, td),
                                                                  _yk_extra_readback(player, td)))
    n_sprites, n_scripts, used = _verify_appended(player, built_text, spec_text)
    plan = " ".join(f"script{s['new_sid']}={s['macro']}("
                    + (f"{s['game']} {s['src']}" if s.get("game") else s["src"])
                    + f" script{s['sid']}"
                    + (f", scale×{s['k']}" if s["k"] != 1.0 else "")
                    + (", alpha255" if s["alpha255"] else "")
                    + (", scaleInj" if s["scale_inject"] else "")
                    + (", prologue" if s.get("prologue") else "") + ")"
                    for s in APPEND_PLAN[player])
    entries = " ".join(f"entry{e['idx']}({len(e['sprites'])} sprites, {e['size'][0]}x{e['size'][1]})"
                       for e in _entries_of(player))
    flat_used = sorted({s for refs in used.values() for s in refs})
    print(f"  {player}: {n_sprites} sprites, {n_scripts} scripts（原生 {len(L.parse_entries(retail_text))} entry / "
          f"{n_native_s} script + 追加 entry {entries} + 脚本 {plan}）；追加脚本用到 sprite {flat_used}")


def main():
    global MAPDIR
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    ap.add_argument("--verify-only", action="store_true")
    a = ap.parse_args()

    th, td, mapdir = L.find_tools()
    MAPDIR = mapdir
    tools, anmmap = (th, td), mapdir / "v8.anmm"

    OUT.mkdir(parents=True, exist_ok=True)
    for player in PLAYERS:
        retail_anm = TH18_DAT / f"{player}.anm"
        if not retail_anm.is_file():
            raise SystemExit(f"缺原生样本 {retail_anm}（用户放进 local/，见 local/README.md）")
        if player in APPEND_PLAN:
            _do_appended(player, a, tools, anmmap)
        else:
            _do_native(player, a, tools, anmmap)

    write_pl_ids_h()
    print(f"verify: 四个文件的原生部分 = 逐 entry / 逐 script / 逐贴图块一致（`verify_rebuilt`）；"
          f"pl00/pl01 的 TH17 兽灵脚本仍逐字节对账；追加脚本通过 越界 / scriptNew / 源脚本对齐 / "
          f"主路径 delete 检查；TH13 灵界贴图与 **108 妖侧本体 55 张** 从编出的 anm 回读逐像素等于"
          f"各自的源图；写 {PL_IDS_H.relative_to(MOD)}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
