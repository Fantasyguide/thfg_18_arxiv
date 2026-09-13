#!/usr/bin/env python3
"""卡片 117 玩家兽灵弹 / 刀的**零售原图**切出（AL63）。

    python3 assets/player_art_src/make_player_art.py [--refresh-originals]

做三件事：
  1. 把**零售原图**（未调色、原始尺寸、无预乘）切到 Windows 侧
     `D:\\项目\\幻想指南\\18\\art-src\\<key>.png`（默认不覆盖已有文件；`--refresh-originals` 恢复原图）。
  2. 把构建**缺图兜底**会用的「已上色默认图」写到 `art-src/_auto_default/<key>.png`（每次重写，仅参考）。
  3. 写 `art-src/README.md`。

⚠ **不写** `assets/player_art_src/`（那是用户的规范输入目录；构建缺图时只在内存里用默认色兜底）。
用户流程：改 art-src 的原图 → `bash assets/sync_player_art_src.sh` → `cd native && make anm`。
"""
import argparse
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import player_art_lib as L  # noqa: E402

USAGE = [
    ("sanae_frog_s34_f0",        "早苗 青蛙本体",    "pl03.anm script29（=原 script8 副本）", "sprite34 (1,177,30,30)", "缺图兜底：mono→紫 H275"),
    ("sanae_snake_s30_f0",       "早苗 蛇 第0帧",    "pl03.anm script30（=原 script6 副本）", "sprite30 (0,160,64,16)", "缺图兜底：hue→红 H0"),
    ("sanae_snake_s31_f1",       "早苗 蛇 第1帧",    "同上",                                  "sprite31 (64,160,64,16)", "缺图兜底：hue→红 H0"),
    ("sanae_snake_s32_f2",       "早苗 蛇 第2帧",    "同上",                                  "sprite32 (128,160,64,16)", "缺图兜底：hue→红 H0"),
    ("sanae_snake_s33_f3",       "早苗 蛇 第3帧",    "同上",                                  "sprite33 (192,160,64,16)", "缺图兜底：hue→红 H0"),
    ("sanae_explosion_s40_f0",   "早苗 爆炸 第0帧",  "pl03.anm script29 的 interruptLabel(1)", "pl03c sprite40 (0,0,128,128)", "缺图兜底：mono→紫 H275"),
    ("sanae_explosion_s41_f1",   "早苗 爆炸 第1帧",  "同上",                                  "pl03c sprite41 (128,0,128,128)", "缺图兜底：mono→紫 H275"),
    ("sanae_explosion_s42_f2",   "早苗 爆炸 第2帧",  "同上",                                  "pl03c sprite42 (256,0,128,128)", "缺图兜底：mono→紫 H275"),
    ("sanae_explosion_s43_f3",   "早苗 爆炸 第3帧",  "同上",                                  "pl03c sprite43 (384,0,128,128)", "缺图兜底：mono→紫 H275"),
    ("sanae_flash_s25_f0",       "早苗 青蛙命中闪光", "script29 interruptLabel(1)",           "sprite25 (16,144,16,16)", "缺图兜底：mono→紫 H275"),
    ("sanae_wolf_glint_s25_f0",  "早苗 狼命中旋转小光斑", "pl03.anm script31（=原 script7 副本）", "sprite25 (16,144,16,16)", "缺图兜底：mono→浅红 H0/S0.65"),
    ("sakuya_knife_red_s24_f0",  "咲夜 红刀（狼）",  "pl02.anm script30（=原 script5 副本）", "sprite24 (0,192,64,16)", "缺图兜底：原样（红）"),
    ("sakuya_knife_blue_s25_f0", "咲夜 蓝刀",        "pl02.anm script31（=原 script9 副本；鹫默认紫）", "sprite25 (64,192,64,16)", "缺图兜底：hue→紫 H275"),
]


def readme_text():
    lines = [
        "# 卡片 117「鬼形兽」玩家兽灵弹 / 刀 —— 可编辑**零售原图**（AL63）",
        "",
        "本目录根下的 PNG = **零售原图**（原样裁切，**没有**做任何调色/缩放/预乘），",
        "就是游戏里那张图。你可以直接用画图 / PS 上色。",
        "",
        "> ⚠ `_auto_default/` 子目录里是**构建缺图时会用的默认色版本**（紫/红），",
        "> **不是原图**，只是给你对照/参考的，别拿它当源。",
        "",
        "## 怎么生效（三步）",
        "```bash",
        "# 1) 在本目录把原图上色（保持文件名、尺寸、PNG 格式）",
        "# 2) 拷回仓库规范输入目录：",
        "bash /home/nipporita/renkolab/mods/th18.v1.00a/card-expand/assets/sync_player_art_src.sh",
        "# 3) 重新生成 anm：",
        "cd /home/nipporita/renkolab/mods/th18.v1.00a/card-expand/native && make anm",
        "```",
        "> **构建不会再对你的图做任何自动调色**（有图=原样用；只有在文件缺失时才用 `_auto_default/` 那套兜底）。",
        "> 只改本目录不跑同步脚本，构建**看不到**你的改动。",
        "",
        "## 清单（文件名 ↔ 用在哪 ↔ 原始 sprite）",
        "",
        "| 文件 | 用途 | 用在哪 | 原始 sprite / 缺图兜底 |",
        "| --- | --- | --- | --- |",
    ]
    for key, use, where, sprite, fallback in USAGE:
        lines.append(f"| `{key}.png` | {use} | {where} | {sprite}；{fallback} |")
    lines += [
        "",
        "## 尺寸（**不要改画布尺寸**）",
        "",
        "| 文件 | 尺寸 |",
        "| --- | --- |",
        "| `sanae_frog_s34_f0.png` | 30×30 |",
        "| `sanae_snake_s3*_f*.png` | 64×16 |",
        "| `sanae_explosion_s4*_f*.png` | 128×128 |",
        "| `sanae_flash_s25_f0.png` | 16×16 |",
        "| `sanae_wolf_glint_s25_f0.png` | 16×16 |",
        "| `sakuya_knife_*_f0.png` | 64×16 |",
        "",
        "## 说明",
        "- 咲夜刀弹视觉已 ×2（`build_player_anm.py` 的 `KNIFE_SCALE`）；改贴图不会改这个倍数。",
        "- 早苗青蛙/蛇脚本也已 ×2（`APPEND_PLAN` 的 `k=2.0`），早苗狼出膛点在 `SANAE_WOLF_FORWARD_1/2`。",
        "- 缺图不会报错：构建用「零售原图 + 默认色」兜底（就是 `_auto_default/` 里的样子）。",
        "- 想把本目录恢复成零售原图：`python3 make_player_art.py --refresh-originals`（会覆盖）。",
    ]
    return "\n".join(lines) + "\n"


def main():
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    ap.add_argument("--refresh-originals", action="store_true",
                    help="用零售原图覆盖 art-src 根目录里已有的 PNG（默认不覆盖，保护用户编辑）")
    a = ap.parse_args()

    n, where = L.cut_originals(overwrite=a.refresh_originals)
    print(f"art-src 原图: 写入 {n} 张（未覆盖已存在的）-> {where}")
    m, where2 = L.write_auto_defaults()
    print(f"art-src 默认兜底: {m} 张 -> {where2}")
    if L.WIN_ART_SRC.parent.is_dir():
        L.WIN_ART_SRC.mkdir(parents=True, exist_ok=True)
        (L.WIN_ART_SRC / "README.md").write_text(readme_text(), encoding="utf-8")
        print("art-src/README.md 已写")
    return 0


if __name__ == "__main__":
    sys.exit(main())
