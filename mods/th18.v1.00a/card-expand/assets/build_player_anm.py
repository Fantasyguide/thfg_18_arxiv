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

## 硬校验（任一失败非 0 退出、不产出文件）
目标脚本存在；sprite 全在界内；`scriptNew` 目标存在；`pl00/pl01` 兽灵脚本与 TH17 逐字节一致；
追加脚本普通飞行路径无 `delete()`；**源脚本的 sprite/scriptNew 引用与计划表逐一对应**（TH13 来源
再加「每个 `layer()` 都在 `LAYER_MAP` 里」）；**追加的灵界贴图从编出来的 anm 里回读后必须逐像素
等于 TH13 原版切图**；原生 entry/script/贴图逐块不变；写 `native/pl_anm_ids.h`。
"""
import argparse
import re
import shutil
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
OUT = L.BUILD
PLAYERS = ("pl00", "pl01", "pl02", "pl03")

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
    ],
    "pl01": [
        dict(macro="PL01_TRANCE", game="th13", src="pl01", sid=11, new_sid=47,
             smap={69: 67, 70: 68, 71: 69, 72: 70}, sref={12: 48},
             k=1.0, alpha255=False, scale_inject=False),
        dict(macro="PL01_TRANCE_GLINT", game="th13", src="pl01", sid=12, new_sid=48,
             smap={73: 71}, k=1.0, alpha255=False, scale_inject=False),
    ],
    "pl02": [
        dict(macro="PL02_WOLF",  src="pl02", sid=5, new_sid=30, smap={24: 75},
             k=KNIFE_SCALE, alpha255=True, scale_inject=True),
        dict(macro="PL02_EAGLE", src="pl02", sid=9, new_sid=31, smap={25: 76},
             k=KNIFE_SCALE, alpha255=True, scale_inject=True),
    ],
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
    ],
}
PL_IDS_H = MOD / "native" / "pl_anm_ids.h"

# ★★ 硬钉死的追加脚本号（2026-09-13）：`assets/sht/append_shooterset.py` 里
#   `PL00_SCRIPT_TRANCE / PL01_SCRIPT_TRANCE / PL03_SCRIPT_TRANCE` 与 `check_pl_anm_ids()`
#   把这三个数**写死**了，卡 113 的另一半（sht 行）就是照它发弹的。
#   这里再钉一遍**反方向**的闸：`write_pl_ids_h()` 是「计划 → 头」，本表是「外部约定 → 计划」。
#   谁要动号，必须**同时**改 `APPEND_PLAN` + 本表 + `append_shooterset.py`，否则这里先红，
#   而不是等实跑发现「灵界弹指向了别的脚本」（那是最难查的一类静默错）。
PINNED_NEW_SIDS = {
    "pl00": {"PL00_TRANCE": 41, "PL00_TRANCE_GLINT": 42},
    "pl01": {"PL01_TRANCE": 47, "PL01_TRANCE_GLINT": 48},
    "pl03": {"PL03_TRANCE": 32, "PL03_TRANCE_GLINT": 33},
}

_SPRITE_REF = re.compile(r"\bsprite\(sprite(\d+)\)")
_SPRITE_DEF = re.compile(r"\bsprite(\d+):")
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
    """该文件**按追加顺序**排好的 entry 计划（117 的兽灵/刀在前，113 的灵界弹在后）。

    顺序就是 entry 号：`idx` 必须等于「原生 entry 数 + 前面几项」，`_append_spec` 会硬校验。
    """
    return PLAYER_ART_ENTRIES.get(player, []) + TH13_TRANCE_ENTRIES.get(player, [])


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
    """该 spec 的源 spec 文本 —— TH18 本文件 / TH18 别的自机 / **TH13 原版**（`game="th13"`）。"""
    if spec.get("game") == "th13":
        p = TH13_DAT / f"{spec['src']}.anm"
        if not p.is_file():
            raise SystemExit(f"缺 TH13 样本 {p}（用户放进 local/，见 local/README.md）")
        return _spec_of(p, tools, anmmap, ver="13")
    return retail_text if spec["src"] == player else L.retail(spec["src"])[1]


def _assert_spec_source(spec: dict, src_block: str, src_text: str):
    """源头闸：**源脚本必须是计划里那一支**。

    · `smap` 里每个源 sprite 号都得真的被引用（引用别的 = 多半抄错了脚本号）；
    · `sref` 里每个源脚本号都得真的被 `scriptNew`；
    · `game="th13"` 的还要**双向相等** + 「每个 `layer()` 都在 `LAYER_MAP` 里」——
      层号没登记就报错，逼后来者先查清 TH18 对应哪一层，而不是照抄一个 TH13 的数字。
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
    if spec.get("game") != "th13":
        return
    if refs != set(spec["smap"]) or sn != set(spec.get("sref", {})):
        raise SystemExit(f"{macro}: TH13 源 script{spec['sid']} 的引用 {sorted(refs)}/{sorted(sn)}"
                         f" 与计划 {sorted(spec['smap'])}/{sorted(spec.get('sref', {}))} 不是一一对应"
                         f"（TH13 来源要求精确对齐，否则贴图/子 VM 会静默错位）")
    unknown = sorted({int(m.group(1)) for m in _LAYER_CALL.finditer(src_block)} - set(LAYER_MAP))
    if unknown:
        raise SystemExit(f"{macro}: TH13 源 script{spec['sid']} 用了未登记的 TH13 层号 {unknown}"
                         f"（LAYER_MAP 只有 {sorted(LAYER_MAP)}）—— 先在 TH18 里找出对应层，"
                         f"再往 LAYER_MAP 加一行，别照抄 TH13 的数字")


def _append_spec(player: str, retail_text: str, tools, anmmap) -> str:
    """按 PLAYER_ART_ENTRIES / TH13_TRANCE_ENTRIES / APPEND_PLAN 生成追加后的 spec
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
        if spec["scale_inject"]:
            body = _inject_scale(body, spec["k"])
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
        if (want_block or "").rstrip() != (got_block or "").rstrip():
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
    PL_IDS_H.write_text("\n".join(lines) + "\n", encoding="utf-8")


def _scripts(text: str):
    return {int(m.group(1)): m.group(2)
            for m in re.finditer(r"^script script(\d+) \{\n(.*?)^\}",
                                 text, re.M | re.S)}


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
    """解 spec。`ver` = thanm `-l` 的版本参数（默认本作 TH18；TH13 的源要显式传 "13"）。

    ⚠ anmmap 与 `-l` 是两件事：anmmap 按 **anm 文件头 version** 选（TH13 与 TH18 的头都是 8
      ⇒ 同一张 `v8.anmm`，见 `tooling/thtk/unpack.py` 的 VERSIONS 表），`-l` 才是作品号。
    """
    return L.thanm(["-l", ver or L.VERSION, str(path), "-m", str(anmmap)], path.parent, tools)


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


def _write_entry_textures(player: str, tex: Path):
    """把追加 entry 的贴图写进编译用的 tex 目录（两类来源，见文件头与两个 ENTRIES 表）。"""
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
                                           on_extract=lambda td: _trance_readback(player, td))
    n_sprites, n_scripts, used = _verify_appended(player, built_text, spec_text)
    plan = " ".join(f"script{s['new_sid']}={s['macro']}("
                    + (f"TH13 {s['src']}" if s.get("game") == "th13" else s["src"])
                    + f" script{s['sid']}"
                    + (f", scale×{s['k']}" if s["k"] != 1.0 else "")
                    + (", alpha255" if s["alpha255"] else "")
                    + (", scaleInj" if s["scale_inject"] else "") + ")"
                    for s in APPEND_PLAN[player])
    entries = " ".join(f"entry{e['idx']}({len(e['sprites'])} sprites, {e['size'][0]}x{e['size'][1]})"
                       for e in _entries_of(player))
    flat_used = sorted({s for refs in used.values() for s in refs})
    print(f"  {player}: {n_sprites} sprites, {n_scripts} scripts（原生 {len(L.parse_entries(retail_text))} entry / "
          f"{n_native_s} script + 追加 entry {entries} + 脚本 {plan}）；追加脚本用到 sprite {flat_used}")


def main():
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    ap.add_argument("--verify-only", action="store_true")
    a = ap.parse_args()

    th, td, mapdir = L.find_tools()
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
          f"主路径 delete 检查；TH13 灵界贴图从编出的 anm 回读逐像素等于原版切图；"
          f"写 {PL_IDS_H.relative_to(MOD)}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
