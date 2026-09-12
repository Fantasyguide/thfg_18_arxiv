#!/usr/bin/env python3
"""永夜抄（TH08 系统卡 108）的**时点 / 刻符**道具贴图：从 TH08 零售 etama.anm 里切。

一手来源（全部在 `local/`，gitignored，构建时取；不入库）
────────────────────────────────────────────────────────
* 零售 anm：`local/th08.v1.00d/dat/etama.anm`（anm **version 3**，6 个 entry）
    ⚠️ TH08 的 anm 还没切成目录；本脚本优先读
       `local/th08.v1.00d/anm/etama/data/etama/etama2.png`，
       没有就地用 `local/vendor/thtk/build/thanm/thanm -x 8` 解到**临时目录**（不往 `local/` 写）。
* **道具脚本 = entry1（贴图 `data/etama/etama2.png`）的 script61..script70**，
  由 `ItemManager::spawn_item` `th08:0x4403b9` 的
      `mov edx,DWORD PTR [ebp+0xc] / add edx,0x3d / push edx`
      `mov ecx,DWORD PTR ds:0x160f404 / call 0x4069f0`（`AnmLoaded::load_script_into_vm`）
  直接证实：**脚本号 = 61 + item type**。于是
      script68 = type 7 = **时点** = `sprite(sprite179); +30: colorTime(20,0,128,128,128);
                                          alphaTime(20,0,192); +20: static();`
      script70 = type 9（灰化同款，`color(192,192,192)`；TH08 的跳表只到 type 8 ⇒ 未走）
* `ItemManager::on_draw__` `th08:0x4415a0` 是**旁证**：它按 `type+0xac`（=172+type）直接
  `AnmLoaded::set_sprite`，type 7 → sprite179，与脚本 68 的 sprite179 **同一个号**（✅ 两条独立链同值）；
  另一支 `type+0xb6`（=182+type）是「道具在画面上方时的贴边指示」，type 7 → sprite189，
  而 **sprite189 整块全透明**（本脚本会实测这一点）。
* 类型口径（一手，我复核过、与 `engine/item/th08/01-time-orbs.md` 一致）：
  `th08:0x440115 cmp [ebp+0xc],0x7 / mov [ebp+0x10],3`（type 7 → state 3）、
  `th08:0x440124 cmp [ebp+0xc],0xa / mov [ebp+0x10],5 / mov [ebp+0xc],7`（type 10 → type 7、state 5）、
  类型分派跳表 `th08:0x440cc5` 第 8 项 → `th08:0x440bf9` → `Item::collect_time_orb` `th08:0x4412b0`。

sprite 矩形（一手，thanm -l 的 entry1 段）
────────────────────────────────────────
    entry1 = `data/etama/etama2.png`（256×256）
      sprite179: { x: 112, y: 64, w: 16, h: 16 }  → TPOINT.png = **时点**（type 7 / script68）
      旁证 sprite189: { x: 240, y: 64, w: 16, h: 16 } → 全透明（脚本会断言它确实全透明）

尺寸 / 缩放
──────────
* 原样 16×16（**不重采样、不补画**，纯零售切图；16×16 是 2 的幂 ✅）。
* TH08 是 640×480 场地、TH18 是 2×（1280×960）⇒ 脚本里 `scale(2.0f, 2.0f)` 才是原尺寸观感
  （同仓先例：`115_season_point.anm.txt` 对 16×16 的 `PT_*.png` 用 scale 2.0→1.0）。

产物：`ability/tpoint/TPOINT.png`（16×16）。
stdout 打印要追加进 `entries/ORDER.txt` 的行（也写在 `entries/ORDER.tpoint.txt`）。
幂等：可重复运行，输出逐字节一致。
"""
import subprocess
import tempfile
from pathlib import Path

from PIL import Image

HERE = Path(__file__).resolve().parent
REPO = HERE.parents[4]

ANM = REPO / "local/th08.v1.00d/dat/etama.anm"
THANM = REPO / "local/vendor/thtk/build/thanm/thanm"
SRC_PNG = REPO / "local/th08.v1.00d/anm/etama/data/etama/etama2.png"
OUT = HERE / "tpoint"

RECT_TPOINT = (112, 64, 16, 16)      # sprite179，一手
RECT_BLANK = (240, 64, 16, 16)       # sprite189：on_draw 的 type+182 支路用，期望全透明
ORDER_LINES = ["TPOINT  ability/tpoint/TPOINT.png  16x16"]


def texture() -> Image.Image:
    """TH08 etama.anm entry1 的贴图（`data/etama/etama2.png`）。"""
    if SRC_PNG.is_file():
        return Image.open(SRC_PNG).convert("RGBA")
    if not (ANM.is_file() and THANM.is_file()):
        raise SystemExit(
            f"缺 {SRC_PNG}\n（兜底解包也要 {ANM} 与 {THANM}；先 python3 tooling/thtk/unpack.py th08.v1.00d）")
    with tempfile.TemporaryDirectory() as td:                      # 兜底：直接解 anm，不往 local/ 写
        subprocess.run([str(THANM), "-x", "8", str(ANM)], cwd=td, check=True,
                       stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
        p = Path(td) / "data/etama/etama2.png"
        if not p.is_file():
            raise SystemExit(f"缺 {SRC_PNG}，且 thanm 解包没产出 {p.name}")
        return Image.open(p).convert("RGBA").copy()


def main():
    src = texture()
    if src.size != (256, 256):
        raise SystemExit(f"{SRC_PNG} 是 {src.size}，预期 (256, 256)")

    x, y, w, h = RECT_TPOINT
    assert w & (w - 1) == 0 and h & (h - 1) == 0, "尺寸要是 2 的幂"
    tile = src.crop((x, y, x + w, y + h))
    if tile.getbbox() is None:
        raise SystemExit(f"时点（sprite179）切出来全透明，矩形抄错了：{RECT_TPOINT}")

    bx, by, bw, bh = RECT_BLANK
    blank = src.crop((bx, by, bx + bw, by + bh))
    if blank.getbbox() is not None:
        print(f"⚠️ 旁证失效：sprite189 {RECT_BLANK} 竟然不是全透明（on_draw 的贴边指示那支要重看）")

    OUT.mkdir(exist_ok=True)
    tile.save(OUT / "TPOINT.png")
    print(f"# TPOINT.png: etama.anm entry1 sprite179 ({x},{y},{w},{h}) "
          f"← script68 = item type 7（时点）")
    print(f"# 旁证：sprite189 {RECT_BLANK} 全透明 = {blank.getbbox() is None}"
          f"（对应 on_draw `type+0xb6` 那条支路，时点没有贴边指示）")
    print(f"tpoint: 1 张 16x16 -> {OUT.relative_to(HERE.parent)}")
    print("# ORDER 追加行（也见 entries/ORDER.tpoint.txt）：")
    for line in ORDER_LINES:
        print(line)


if __name__ == "__main__":
    main()
