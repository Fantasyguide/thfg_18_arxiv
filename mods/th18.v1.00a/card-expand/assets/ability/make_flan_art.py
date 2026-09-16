#!/usr/bin/env python3
"""地灵殿（id 111）二小姐「蓄力清屏」那一下的**爆闪**切图 —— TH18 原生卡 42（小槌）挥击的 4 个 sprite。

    python3 make_flan_art.py          # 出图 + 打印要 append 到 entries/ORDER.txt 的那几行
    python3 make_flan_art.py --dump   # 附带把每个 sprite 打成 ASCII（核证据用，只读源图）

=====================================================================================
一手考证：TH18 原生 `ability.anm` **entry0**（`player/option/op00.png`，256×512）的这 4 格
=====================================================================================

■ 结论（✅ 一手，逐条可复核）

  | 我们切的名字 | 源 sprite | 源矩形 | 一手 |
  | --- | --- | --- | --- |
  | `FLAN_MALLET_IDLE` | **sprite41** | (32, 80, 32, 16) | 卡 42 建 VM 时卡侧 `set_sprite(0x29)`（`th18:0x40f161` 的立即数）|
  | `FLAN_MALLET_SWING_0` | **sprite85** | (0, 160, 48, 47) | `ability.anm` **script28** 正文 `sprite(sprite85)` |
  | `FLAN_MALLET_SWING_1` | **sprite86** | (48, 160, 48, 47) | 同上 `sprite(sprite86)` |
  | `FLAN_MALLET_SWING_2` | **sprite87** | (96, 160, 48, 47) | 同上 `sprite(sprite87)` |

  - script28 = 卡 42（KOZUCHI 小槌）挥击那支 VM 的脚本（卡侧状态机 `th18:0x40eed0`：
    state 1 跟随+上升 20 帧 → state 2 挥击 → state 3 半径清弹 10 帧；层 `layer(13)`、存活 ≈29 帧）。
  - 全链一手过程（地址 / 帧数 / 半径 / 音效）见
    `engine/card/th18/13-mallet-clear-and-th17-charge.md` §1.4（脚本正文）与 §3.2（切图建议表）。
  - 矩形**逐项对过源文件的 sprite 表本体**（`local/th18.v1.00a/anm/ability/ability.anm.txt`
    第 57 / 101 / 102 / 103 行：`sprite41/sprite85/sprite86/sprite87`），不是从别处抄的。

■ 为什么抠图，而不是在脚本里直接写 `sprite(85)`

  两者**都能跑**（sprite 号是整份 anm 的**全局**表：entry0 = sprite0..99、entry1 = sprite100..103 …
  ⇒ 我们的追加脚本写 `sprite(85)` 就直接指到这一格）。选抠图的理由有两条：

  1. 本仓 `assets/ability/scripts/` 里**没有任何一条追加脚本引用原生 sprite 号**
     （全部走 `@NAME@` 或 `sprite(-1)`）⇒ 跟既有管线一致，并在 `anm_ids.h` 里留下
     `CE_ANM_ABILITY_SPRITE_FLAN_MALLET_*` 给 C 侧（要 `ce_anm_set_sprite` 换帧时有名字可用）。
  2. 顺带把「这张图是不是会随自机被换掉」这个疑问**永久钉死**（见下一条）——
     版权字节只在 `local/`，不入库（同 `ORDER.txt` 第 3 行的约定）。

■ ❓ 曾担心「entry0 的贴图会不会按机体被替换」—— 否（✅ 一手，两条独立证据）

  1. **整包 .dat 里只有一张 option 贴图**：`th18.dat` 解出来 `player/option/` 下只有
     `op00.png` / `enm5b.png` / `item09.png`（`find local/th18.v1.00a -name 'op*.png'` 只有一条），
     **没有** `op01/op02/op03.png` ⇒ 没有可换的源。
  2. **entry0 是「所有 option 图的总表」**：原生 8 支 option 脚本都在 entry0 里取图，
     卡侧经 `Player__allocate_option`（`th18:0x40a790`，第 5 个栈参 = ability.anm 脚本号）起它们：
     调用点 15 处的脚本号 = **2 / 6 / 7 / 14 / 16 / 18 / 20 / 22 / 23**（`th18:0x40aae1` 起逐处读），
     各自取 **sprite0 / sprite7 / sprite21..28+35..30 / sprite52 / sprite59..61 / sprite64..67 /
     sprite68..71 / sprite76..78** —— 全落在 entry0 的 sprite0..99 段内。
     ⇒ 一张贴图同时喂多个机体的 option，**不是**随自机替换的那一张。

  ⚠ 上述两条只影响「要不要抠图」这个取舍；**抠了以后与源贴图无关**，风险归零。

■ 与「蓄力光环」的关系

  光环走 `drawPoly`，**不消耗 sprite**：TH17 `pl02.anm` script30 与 TH18 原生 `ability.anm` script49
  都写 `sprite(-1)`（原文见 `engine/card/th18/13-mallet-clear-and-th17-charge.md` §2.3）
  ⇒ 本文件**只出爆闪这 4 张**，光环那一支一个 sprite 都不要。

■ 生成器自身的纪律（照 `make_signal_hud_art.py` 的既有管线）

  - 只用不带 mask 的 `crop` / `paste`：**不缩放、不重采样、不补边**（矩形 = 原图像素尺寸，
    这是 `build_ability.py` 图集硬门②/③要求的）；
  - 幂等（无随机、无时间戳）；缺源图时直接报错并给解包命令（`local/` 是 gitignored 的）。
"""
import sys
from pathlib import Path

from PIL import Image

HERE = Path(__file__).resolve().parent
REPO = HERE.parents[4]
SRC = REPO / "local/th18.v1.00a/anm/ability/player/option/op00.png"
OUT = HERE / "flan"

# 一手：TH18 `ability.anm` entry0 的贴图尺寸（`ability.anm.txt` entry0 的 width/height）
SHEET = (256, 512)

# 一手：`ability.anm` script28 正文 + 卡 42 建 VM 时的 `set_sprite(0x29)`（见文件头）
SP_IDLE = (32, 80, 32, 16)             # sprite41 —— 卡 42 建 VM 时那一格（32x16）
SP_SWING_0 = (0, 160, 48, 47)          # sprite85 —— script28 三帧循环第 0 张
SP_SWING_1 = (48, 160, 48, 47)         # sprite86
SP_SWING_2 = (96, 160, 48, 47)         # sprite87


def _load():
    if not SRC.is_file():
        raise SystemExit(
            f"缺原版贴图 {SRC}\n（TH18 的资产先解包：`python3 tooling/thtk/unpack.py th18.v1.00a`，"
            "或 `thdat -x 18 th18.dat ability.anm` + `thanm -l/-x`）"
        )
    im = Image.open(SRC).convert("RGBA")
    if im.size != SHEET:
        raise SystemExit(f"{SRC} 是 {im.size}，预期 {SHEET}（entry0 的 width/height）")
    return im


def _crop(src, box):
    x, y, w, h = box
    return src.crop((x, y, x + w, y + h)).copy()


def _dump(src):
    def art(box, name):
        x, y, w, h = box
        print(f"\n--- {name} @{box} ---")
        for j in range(h):
            row = ""
            for i in range(w):
                r, g, b, a = src.getpixel((x + i, y + j))
                row += "." if a < 40 else ("W" if r > 200 and g > 200 and b > 200
                                           else ("k" if r < 80 and g < 80 and b < 80 else "?"))
            print("  " + row)

    art(SP_IDLE, "sprite41 FLAN_MALLET_IDLE")
    art(SP_SWING_0, "sprite85 FLAN_MALLET_SWING_0")
    art(SP_SWING_1, "sprite86 FLAN_MALLET_SWING_1")
    art(SP_SWING_2, "sprite87 FLAN_MALLET_SWING_2")


def main():
    dump = "--dump" in sys.argv
    src = _load()
    OUT.mkdir(exist_ok=True)

    for box, name in ((SP_IDLE, "FLAN_MALLET_IDLE"), (SP_SWING_0, "FLAN_MALLET_SWING_0"),
                      (SP_SWING_1, "FLAN_MALLET_SWING_1"), (SP_SWING_2, "FLAN_MALLET_SWING_2")):
        tile = _crop(src, box)
        tile.save(OUT / f"{name}.png")
        print(f"flan: {name}.png {tile.width}x{tile.height} = TH18 ability.anm entry0 "
              f"({box[0]},{box[1]},{box[2]},{box[3]})")

    print("\n图集画布 144x64（4 个成员并列，无补边）；把下面这段 append 到 entries/ORDER.txt：")
    print("ATLAS ATLAS_FLAN 144x64")
    print("FLAN_MALLET_IDLE       ability/flan/FLAN_MALLET_IDLE.png              @0,0,32x16")
    print("FLAN_MALLET_SWING_0    ability/flan/FLAN_MALLET_SWING_0.png           @0,16,48x47")
    print("FLAN_MALLET_SWING_1    ability/flan/FLAN_MALLET_SWING_1.png           @48,16,48x47")
    print("FLAN_MALLET_SWING_2    ability/flan/FLAN_MALLET_SWING_2.png           @96,16,48x47")

    if dump:
        _dump(src)


if __name__ == "__main__":
    main()
