#!/usr/bin/env python3
"""卡片 117 的玩家 anm 侧：pl00/pl01 原生拷贝；pl02/pl03 追加兽灵弹 entry/脚本（AL59–AL62）。

    python3 build_player_anm.py                # 重建 native/build/pl0X.anm + 硬校验
    python3 build_player_anm.py --verify-only  # 只校验已有的 native/build/pl0X.anm

## 现状
- **pl00/pl01**：TH18 原生 `pl0X.anm` 已逐字节含 TH17 兽灵脚本（TH17 `anm` = TH18 `anm`+5，
  `hit5` 硬编码 script8）⇒ **原样拷贝**，一个字节不改（`_assert_sources` 每次回读对账）。
- **pl02（咲夜）/ pl03（早苗）**：没有 TH17 对应自机 ⇒ 追加：
  · pl02：script30=红刀（狼）、script31=紫刀（鹫，真弹视觉）；贴图 entry12（红/紫刀）。
  · pl03：script29=大青蛙、script30=大蛇（都 scale×2）；entry13..16（青蛙/蛇/爆炸/闪光）。
- **颜色（AL62）**：不再用 `color()` 乘法近似，改成**构建期按像素**转色：
  源图 = `assets/player_art_src/<key>.png`（用户可改）；缺图自动用「零售图 + 默认色」补出来
  （见 `player_art_lib.py`）。`make_player_art.py` 负责补默认 + 切到 `D:\\...\\18\\art-src\\`；
  回流用 `assets/sync_player_art_src.sh`。**缺图不会红、不会出错图**。

## 硬校验（任一失败非 0 退出、不产出文件）
目标脚本存在；sprite 全在界内；`scriptNew` 目标存在；`pl00/pl01` 兽灵脚本与 TH17 逐字节一致；
追加脚本普通飞行路径无 `delete()`；原生 entry/script/贴图逐块不变；写 `native/pl_anm_ids.h`。
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
# 每项 = dict(macro, src, sid, new_sid, smap, k, alpha255, scale_inject)：
#   · smap = {源 sprite 号: 新 sprite 号}；k = scale 倍数；alpha255 = `alpha(192)`→255；
#   · scale_inject = True 时在脚本头注入 `scale(k,k)`（原脚本没有 scale 的刀脚本用）。
#
# 追加进去的脚本是「会飞的主弹」，普通飞行路径不许 delete()。
APPEND_FLYING = {"pl02": (30, 31), "pl03": (29, 30)}

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
APPEND_PLAN = {
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
    ],
}
PL_IDS_H = MOD / "native" / "pl_anm_ids.h"

_SPRITE_REF = re.compile(r"\bsprite\(sprite(\d+)\)")
_SPRITE_DEF = re.compile(r"\bsprite(\d+):")
_SCRIPT_NEW = re.compile(r"\bscriptNewPos?\(script(\d+)")
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
        body = re.sub(rf"\bscriptNew(Root|Pos)?\(script{old}\b", rf"scriptNew\1(script{new}", body)
    return body


def _inject_scale(body: str, k: float) -> str:
    """在脚本头后注入 `scale(k,k)`（原脚本没有 scale 的刀脚本用；k 由调用方保证 != 1）。"""
    m = re.match(r"(script script\d+ \{\n)", body)
    if not m:
        raise SystemExit("注入 scale 失败：脚本头没找到")
    return body[:m.end()] + f"    scale({_mul('1.0f', k)}, {_mul('1.0f', k)});\n" + body[m.end():]


def _entry_block(ent: dict) -> str:
    """追加 entry 的 spec 文本（format 5；整贴图一个 entry、里面若干 sprite）。"""
    w, h = ent["size"]
    lines = [
        f"entry entry{ent['idx']} {{",
        "    version: 8,",
        f'    name: "{ent["name"]}",',
        "    format: 5,",
        f"    width: {w},",
        f"    height: {h},",
        "    memoryPriority: 10,",
        "    lowResScale: 0,",
        "    hasData: 1,",
        f"    THTXSize: {w * h * 2},",
        "    THTXFormat: 5,",
        f"    THTXWidth: {w},",
        f"    THTXHeight: {h},",
        "    THTXZero: 0,",
        "    sprites: {",
    ]
    for k, (sid, x, y, sw, sh, _key) in enumerate(ent["sprites"]):
        comma = "," if k + 1 < len(ent["sprites"]) else ""
        lines.append(f"        sprite{sid}: {{ x: {x}, y: {y}, w: {sw}, h: {sh} }}{comma}")
    lines += ["    }", "}"]
    return "\n".join(lines)


def _append_spec(player: str, retail_text: str, tools, anmmap) -> str:
    """按 PLAYER_ART_ENTRIES / APPEND_PLAN 生成追加后的 spec 文本（纯函数，编译前先自检）。"""
    text = retail_text
    n_native_e = len(L.parse_entries(retail_text))
    for k, ent in enumerate(PLAYER_ART_ENTRIES.get(player, [])):
        want = n_native_e + k
        if ent["idx"] != want:
            raise SystemExit(f"{player} 原生 entry 数 = {n_native_e}，第 {k + 1} 个追加 entry 应编号 "
                             f"{want}，但计划写 {ent['idx']}（两处必须同步）")
        text = L.insert_entries(text, [_entry_block(ent)])
    n_scripts = len(L.parse_scripts(retail_text))
    blocks = []
    for spec in APPEND_PLAN[player]:
        macro, src_player, src_sid, new_sid = spec["macro"], spec["src"], spec["sid"], spec["new_sid"]
        if new_sid > n_scripts + len(blocks):
            raise SystemExit(f"{player}: {macro} 计划 script{new_sid}，但下一条应是 "
                             f"script{n_scripts + len(blocks)}（脚本号必须连续追加）")
        src_text = retail_text if src_player == player else L.retail(src_player)[1]
        src_block = _script_block(src_text, src_sid)
        if not src_block:
            raise SystemExit(f"{player}: 源 {src_player}.anm 没有 script{src_sid}")
        body = _scale_block(src_block, spec["k"])
        body = _replace_sprites(body, spec["smap"])
        if spec.get("sref"):
            body = _replace_script_new(body, spec["sref"])
        if spec["alpha255"]:
            body = body.replace("alpha(192)", "alpha(255)")
        if spec["scale_inject"]:
            body = _inject_scale(body, spec["k"])
        body = _rename_script(body, src_sid, new_sid)
        blocks.append(body)
    return L.append_scripts(text, blocks) if blocks else text


def _verify_appended(player: str, text: str):
    """回读核对：原生部分没变（`L.verify_rebuilt` 已做）+ 追加脚本的 sprite/scriptNew 全在界内
    + 会飞的主弹脚本普通路径无 delete()。-> (n_sprites, n_scripts, used)。"""
    sprites = _sprite_ids(text)
    n_sprites = (max(sprites) + 1) if sprites else 0
    scripts = _scripts(text)
    used = {}
    for spec in APPEND_PLAN[player]:
        macro, new_sid = spec["macro"], spec["new_sid"]
        body = scripts.get(new_sid)
        if not body:
            raise SystemExit(f"{player}.anm: 追加脚本 script{new_sid}（{macro}）不存在")
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


def write_pl_ids_h():
    lines = ["/* 由 assets/build_player_anm.py 生成 —— pl0X.anm 追加脚本号。别手改。 */",
             "#pragma once", ""]
    for player in sorted(APPEND_PLAN):
        for spec in APPEND_PLAN[player]:
            lines.append(f"#define CE_PL_ANM_SCRIPT_{spec['macro']:<16} {spec['new_sid']:>3}   "
                         f"/* {player}.anm 追加 */")
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


def _spec_of(path: Path, tools, anmmap):
    return L.thanm(["-l", L.VERSION, str(path), "-m", str(anmmap)], path.parent, tools)


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


def _do_appended(player: str, a, tools, anmmap):
    """pl02/pl03：用 anmlib 重建 = 原生部分 + 追加 entry/脚本。"""
    retail_dir, retail_text = L.retail(player)
    out_anm = OUT / f"{player}.anm"
    n_native_s = len(L.parse_scripts(retail_text))
    for spec in APPEND_PLAN[player]:
        if spec["new_sid"] < n_native_s:
            raise SystemExit(f"{player}: 追加脚本号 script{spec['new_sid']} 与原生脚本区间冲突")
    if not a.verify_only:
        from PIL import Image
        tex = L.BUILD / f"tex-player-{player}"
        L.fresh_dir(tex)
        L.link_retail_textures(tex, retail_dir, L.parse_entries(retail_text))
        for ent in PLAYER_ART_ENTRIES.get(player, []):
            canvas = Image.new("RGBA", ent["size"], (0, 0, 0, 0))
            for _sid, x, y, w, h, key in ent["sprites"]:
                src = PA.ensure(key)          # 缺图则用零售+默认色自动生成（不红）
                if src.size != (w, h):
                    raise SystemExit(f"{key}: 源图 {src.size} != 声明的 {w}x{h}（改图别改画布尺寸）")
                canvas.paste(src, (x, y))
            dst = tex / ent["name"]
            dst.parent.mkdir(parents=True, exist_ok=True)
            canvas.save(dst)
        L.compile_anm(_append_spec(player, retail_text, tools, anmmap), tex, out_anm, tools, anmmap)
    if not out_anm.is_file():
        raise SystemExit(f"缺 {out_anm}：先跑 python3 assets/build_player_anm.py")
    built_text, n_e, n_s = L.verify_rebuilt(out_anm, retail_text, retail_dir, tools, anmmap)
    n_sprites, n_scripts, used = _verify_appended(player, built_text)
    plan = " ".join(f"script{s['new_sid']}={s['macro']}({s['src']} script{s['sid']}"
                    + (f", scale×{s['k']}" if s["k"] != 1.0 else "")
                    + (", alpha255" if s["alpha255"] else "")
                    + (", scaleInj" if s["scale_inject"] else "") + ")"
                    for s in APPEND_PLAN[player])
    entries = " ".join(f"entry{e['idx']}({len(e['sprites'])} sprites, {e['size'][0]}x{e['size'][1]})"
                       for e in PLAYER_ART_ENTRIES.get(player, []))
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
    print(f"verify: pl00/pl01 = 原生零改动（TH17 兽灵脚本逐字节一致）；"
          f"pl02/pl03 = 原生部分逐块一致 + 追加脚本通过越界/scriptNew/主路径 delete 检查；"
          f"写 {PL_IDS_H.relative_to(MOD)}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
