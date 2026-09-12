#!/usr/bin/env python3
"""妖妖梦（TH07 系统卡 107）的**樱点 / 小樱点**道具贴图：从 TH07 零售 etama.anm 里切。

一手来源（全部在 `local/`，gitignored，构建时取；不入库）
────────────────────────────────────────────────────────
* 零售 anm：`local/th07.v1.00b/dat/etama.anm`（anm **version 2**，4 个 entry）
    切好的贴图：`local/th07.v1.00b/anm/etama/data/etama/etama*.png`（256×256）
* **道具脚本 = entry1（贴图 `data/etama/etama2.png`）的 script53..script62**，
  即「一个 sprite + static()」那十连发（✅ 一手，见下）。
    script53 `sprite(sprite4);  static();`
    script54 `sprite(sprite5);  static();`
    script55 `sprite(sprite6);  static();`
    script56 `sprite(sprite7);  static();`
    script57 `sprite(sprite8);  static();`
    script58 `sprite(sprite9);  static();`
    script59 `sprite(sprite10); alpha(192); static();`
    script60 `sprite(sprite11); static();`          ← ★ TH07 **樱点**（item type 7）
    script61 `sprite(sprite12); …`                  ← ★ TH07 **小樱点**（item type 8）
    script62 `sprite(sprite11); color(0xC0C0C0); alpha(192); static();`（灰化樱点，type 9）
  复现：`local/vendor/thtk/build/thanm/thanm -l 7 -m local/vendor/eclmap/anmmap/v2.anmm \
            local/th07.v1.00b/dat/etama.anm`

**为什么是 script60 = type 7 = 樱点**（可信度 ✅，三条一手链交叉）
1. 脚本块的**结构 1:1 对上 TH08** 的 etama.anm script61..70（TH08 那十条是**反汇编直接证实**的
   道具脚本：`ItemManager::spawn_item` `th07`↔`th08:0x4403b9` `add edx,0x3d`（= 61+type）→
   `AnmLoaded::load_script_into_vm`）；两边同一位置的 script 连**指令都同形**：
   块内第 6 条两边都是 `+ alpha(192)`，块内第 9 条两边都是 `ins_9/color(0xC0C0C0) + alpha(192)`。
2. TH07 自己的 `spawn_item` `th07:0x4328ea` 用 `type + 0x2c4`（= 708+type）当运行时脚本号；
   而「把道具改成 type 7 再换脚本」的实证在 `th07:0x433bed`：
   `mov BYTE PTR [eax+0x27c],0x7` + `mov WORD PTR [edx+0x1d8],0x2cb` + `anmbase[0x28ef0+0x2cb*4]`
   ⇒ **0x2cb = 0x2c4 + 7**，即 type 7 的脚本号，且 0x2c4..0x2cd 是**连续十个**脚本
   ⇒ 与 etama.anm entry1 的 script53..62（连续十条）一一对应，type t ↔ script(53+t)。
3. `th07:0x4326f0`（`ItemManager::spawn_item`）里 `if (power >= 128 && (type==0||type==2)) type = 7`
   —— 满功率时 P / 大 P 变 type 7；`th07:0x433b20` 也是把其它 P 改成 **type 7** 并重载脚本 0x2cb。
   type 7 的收集处理 `th07:0x43375f` 读樱点计数对（`ds:0x62f888` 需求 / `ds:0x62f88c` 当前）并调
   樱点加法器 `th07:0x42f5a2`（写 `[GAME_MANAGER+0x961c]`，上限 `+0x9618`）⇒ **type 7 就是樱点道具**。
   小樱点 = type 8：`th07:0x433742` 的处理给它 `+30`（`0x42f5a2`）与 `+70`（`0x42f69f`），
   而 ECL 的批量掉落 `th07:0x4247a7..0x4247e3`（amount ≥ 3 时）**只掉 type 8** ⇒ 场上成片掉的那种。

sprite 矩形（一手，thanm -l 的 entry1 段）
────────────────────────────────────────
    entry1 = `data/etama/etama2.png`（256×256）
      sprite11: { x: 112, y: 64, w: 16, h: 16 }   → POINT.png  = **樱点**（type 7 / script60）
      sprite12: { x:   0, y: 80, w: 16, h: 16 }   → SMALL.png  = **小樱点**（type 8 / script61）

尺寸 / 缩放
──────────
* 原样 16×16（**不重采样、不补画**，纯零售切图；16×16 是 2 的幂 ✅）。
* TH07 是 640×480 场地、TH18 是 2×（1280×960）⇒ 脚本里 `scale(2.0f, 2.0f)` 才是原尺寸观感
  （同仓先例：`115_season_point.anm.txt` 对 16×16 的 `PT_*.png` 用 scale 2.0→1.0）。

产物：`ability/sakura/POINT.png`、`ability/sakura/SMALL.png`（各 16×16）。
stdout 打印要追加进 `entries/ORDER.txt` 的行（也写在 `entries/ORDER.sakura.txt`）。
幂等：可重复运行，输出逐字节一致。
"""
import subprocess
import sys
import tempfile
from pathlib import Path

from PIL import Image

HERE = Path(__file__).resolve().parent
REPO = HERE.parents[4]

# 一手源：anm（兜底解包用）与已切好的零售贴图
ANM = REPO / "local/th07.v1.00b/dat/etama.anm"
THANM = REPO / "local/vendor/thtk/build/thanm/thanm"
SRC_PNG = REPO / "local/th07.v1.00b/anm/etama/data/etama/etama2.png"
OUT = HERE / "sakura"

# entry1 的 sprite 矩形（一手，见文件头）
CROPS = {
    # 文件名          sprite 号  入口脚本            item type   含义     (x, y, w, h)
    "POINT.png": (11, "script60", 7, "樱点", (112, 64, 16, 16)),
    "SMALL.png": (12, "script61", 8, "小樱点", (0, 80, 16, 16)),
}
ORDER_LINES = [
    "SAKURA_POINT  ability/sakura/POINT.png  16x16",
    "SAKURA_SMALL  ability/sakura/SMALL.png  16x16",
]


def texture() -> Image.Image:
    """TH07 etama.anm entry1 的贴图（`data/etama/etama2.png`）。"""
    if SRC_PNG.is_file():
        return Image.open(SRC_PNG).convert("RGBA")
    if not (ANM.is_file() and THANM.is_file()):
        raise SystemExit(
            f"缺 {SRC_PNG}\n（兜底解包也要 {ANM} 与 {THANM}；先 python3 tooling/thtk/unpack.py th07.v1.00b）")
    with tempfile.TemporaryDirectory() as td:                      # 兜底：直接解 anm，不往 local/ 写
        subprocess.run([str(THANM), "-x", "7", str(ANM)], cwd=td, check=True,
                       stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
        p = Path(td) / "data/etama/etama2.png"
        if not p.is_file():
            raise SystemExit(f"缺 {SRC_PNG}，且 thanm 解包没产出 {p.name}")
        return Image.open(p).convert("RGBA").copy()


def main():
    src = texture()
    if src.size != (256, 256):
        raise SystemExit(f"{SRC_PNG} 是 {src.size}，预期 (256, 256)")
    OUT.mkdir(exist_ok=True)
    for name, (sidx, script, itype, label, rect) in CROPS.items():
        x, y, w, h = rect
        assert w & (w - 1) == 0 and h & (h - 1) == 0, "尺寸要是 2 的幂"
        tile = src.crop((x, y, x + w, y + h))
        if tile.getbbox() is None:
            raise SystemExit(f"{name}（sprite{sidx}）切出来全透明，矩形抄错了：{rect}")
        tile.save(OUT / name)
        print(f"# {name}: etama.anm entry1 sprite{sidx} ({x},{y},{w},{h}) "
              f"← {script} = item type {itype}（{label}）")
    print(f"sakura: {len(CROPS)} 张 16x16 -> {OUT.relative_to(HERE.parent)}")
    print("# ORDER 追加行（也见 entries/ORDER.sakura.txt）：")
    for line in ORDER_LINES:
        print(line)


if __name__ == "__main__":
    main()
