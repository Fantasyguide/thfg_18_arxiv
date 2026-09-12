#!/usr/bin/env python3
"""妖妖梦（TH07 系统卡 107）**森罗结界**的两张贴图：从 TH07 零售 etama.anm 里切。

一手来源（全部在 `local/`，gitignored，构建时取；不入库）
────────────────────────────────────────────────────────
* 零售 anm：`local/th07.v1.00b/dat/etama.anm`（anm **version 2**，4 个 entry）
    切好的贴图：`local/th07.v1.00b/anm/etama/data/etama/etama{2,4}.png`（各 256×256）

**为什么要这两张**（结界两半 = 结界展开的那个环 + 灵击崩结界时飞出去的那 32 颗）：
TH07 的「结界」不是一个 anm 脚本，而是 **EffectManager 的 effect type**；`th07:0x441aa4`（开结界）
与 `th07:0x441c19`（灵击崩结界）都 `push 0x1c; call 0x41c610`，`th07:0x441e01`（那 32 颗的循环）
`push 0x1d; call 0x41c1c0`。type → 脚本号的表在 `th07:0x49efc0`（每项 `{u16 id; u16 pad; void* update; void* draw}`）：

    type 0x1c → id 731   type 0x1d → id 690

**id 是怎么落到 etama.anm 的**（✅ 一手，两条独立链）
1. 脚本 id = **该文件累加的 base + 文件内 id**；`AnmManager::loadAnm` `th07:0x44df90` → 每 entry
   `0x44e070` 写 `anmbase[(base+id)*4 + 0x28ef0] = entry + offset`（`0x44e47f`），
   **base 每个 entry 前进「上一个 entry 的 sprite 数」**（`0x44e05b add ecx,[ebp-0x4]`；`[ebp-0x4]`
   是 `0x44e070` 的返回值 = sprite 数）。
   实证：`text.anm` base **1792**（`0x438c4e` 推 0x700）+ 36 sprites = **1828** = `capture.anm` 的 base
   （`0x401db3` 推 0x724）✓；`etama.anm` base **512**（`0x426fae` 推 0x200）+ entry0 的 **168** sprites
   = **680** = entry1（etama2.png）的 base；+50（entry1 的 sprites）= **730** = entry2（etama3.png）；
   +1 = **731** = entry3（etama4.png）✓；`eff01.anm` 的 base **732**（`0x41ce26` 推 0x2dc）= 731+1 ✓✓。
2. 同一条链上「道具脚本 = `0x2c4 + type`」也落在这里：`th07:0x4328ea add edx,0x2c4` → 708+type；
   base(entry1)=680 ⇒ 脚本号 708..717 = entry1 的文件内 id **28..37**，正是
   `make_sakura_art.py` 记的那十连发（script53..62 = `sprite(sprite4..12); static();`）✓
   ⇒ **两条链互相印证**（道具那半边已由 186_sakura_point.anm.txt 独立复核过）。

    ⇒ 731 = entry3（`data/etama/etama4.png`，**整张 256×256 一个 sprite**）的文件内脚本 0
      ⇒ `assets/ability/scripts/204_senro_field.anm.txt`（照抄 `thanm -l` 的 script64）
    ⇒ 690 = entry1（`data/etama/etama2.png`）的文件内 id **10** = `thanm -l` 里的 **script35**
      ⇒ `assets/ability/scripts/205_senro_ring.anm.txt`（照抄 `thanm -l` 的 script35）

复现（一手命令）
────────────────
    local/vendor/thtk/build/thanm/thanm -l 7 -m local/vendor/eclmap/anmmap/v2.anmm \
        local/th07.v1.00b/dat/etama.anm        # 看 entry3 / script35 原文
    python3 -c "import struct;d=open('local/th07.v1.00b/dat/etama.anm','rb').read();\
print([struct.unpack_from('<iI',d,64+168*4+i*8)[0] for i in range(25)])"   # entry0 的文件内 id

sprite 矩形（一手，`thanm -l`）
──────────────────────────────
    entry3 = `data/etama/etama4.png`（256×256）
      sprite0: { x: 0, y: 0, w: 256, h: 256 }   → FIELD.png = **结界展开的那个大环**（整张贴图）
    entry1 = `data/etama/etama2.png`（256×256）
      sprite28: { x: 128, y: 112, w: 64, h: 64 } → RING.png = **灵击时飞出去的那 32 颗**（一颗一份）

尺寸 / 缩放
──────────
* **原样切，不重采样、不补画**（纯零售切图；256×256 与 64×64 都是 2 的幂 ✅）。
* **不加 scale**：TH18 世界层 VM 的「坐标与尺寸都是 640 空间的 ECL 单位」、1 单位 = 2 表面 px
  （`engine/anm/th18/01-vm-instantiate.md` §3）⇒ 零售脚本里的 `scale(1.0f, 1.0f)` 与
  不带 scale 的 64×64 就**已经是原尺寸观感**，与 `186_sakura_point.anm.txt`（TH07 道具原样搬）同口径。

产物：`ability/senro/FIELD.png`（256×256）、`ability/senro/RING.png`（64×64）。
stdout 打印要追加进 `entries/ORDER.txt` 的行（也写在 `entries/ORDER.senro.txt`）。
幂等：可重复运行，输出逐字节一致。
"""
import subprocess
import tempfile
from pathlib import Path

from PIL import Image

HERE = Path(__file__).resolve().parent
REPO = HERE.parents[4]

ANM = REPO / "local/th07.v1.00b/dat/etama.anm"
THANM = REPO / "local/vendor/thtk/build/thanm/thanm"
SRC2 = REPO / "local/th07.v1.00b/anm/etama/data/etama/etama2.png"   # entry1（ring 那颗）
SRC4 = REPO / "local/th07.v1.00b/anm/etama/data/etama/etama4.png"   # entry3（结界那个环）
OUT = HERE / "senro"

# 文件名         源 entry/贴图        sprite   入口脚本（一手）  effect type  含义          (x, y, w, h)
CROPS = {
    "FIELD.png": (4, "etama4.png", 0, "script64", 0x1C, "森罗结界展开的环", (0, 0, 256, 256)),
    "RING.png": (1, "etama2.png", 28, "script35", 0x1D, "灵击崩结界的 32 颗", (128, 112, 64, 64)),
}
ORDER_LINES = [
    "SENRO_FIELD  ability/senro/FIELD.png  256x256",
    "SENRO_RING   ability/senro/RING.png    64x64",
]


def texture(entry: int, name: str) -> Image.Image:
    """TH07 etama.anm 第 `entry` 个 entry 的贴图。"""
    src = {1: SRC2, 4: SRC4}[entry]
    if src.is_file():
        return Image.open(src).convert("RGBA")
    if not (ANM.is_file() and THANM.is_file()):
        raise SystemExit(
            f"缺 {src}\n（兜底解包也要 {ANM} 与 {THANM}；先 python3 tooling/thtk/unpack.py th07.v1.00b）")
    with tempfile.TemporaryDirectory() as td:                      # 兜底：直接解 anm，不往 local/ 写
        subprocess.run([str(THANM), "-x", "7", str(ANM)], cwd=td, check=True,
                       stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
        p = Path(td) / ("data/etama/" + name)
        if not p.is_file():
            raise SystemExit(f"缺 {src}，且 thanm 解包没产出 {name}")
        return Image.open(p).convert("RGBA").copy()


def main():
    OUT.mkdir(exist_ok=True)
    total_vis = 0
    for fname, (entry, tex, sidx, script, etype, label, rect) in CROPS.items():
        src = texture(entry, tex)
        if src.size != (256, 256):
            raise SystemExit(f"{tex} 是 {src.size}，预期 (256, 256)")
        x, y, w, h = rect
        assert w & (w - 1) == 0 and h & (h - 1) == 0, "尺寸要是 2 的幂"
        tile = src.crop((x, y, x + w, y + h))
        if tile.getbbox() is None:
            raise SystemExit(f"{fname}（sprite{sidx}）切出来全透明，矩形抄错了：{rect}")
        hist = tile.getchannel("A").histogram()          # alpha 直方图：第 0 桶 = 全透明
        vis = w * h - hist[0]
        total_vis += vis
        tile.save(OUT / fname)
        print(f"# {fname}: etama.anm entry{entry} sprite{sidx} ({x},{y},{w},{h}) "
              f"← {script} = effect type 0x{etype:02x}（{label}）；可见像素 {vis}/{w * h}")
    print(f"senro: {len(CROPS)} 张 -> {OUT.relative_to(HERE.parent)}（可见像素合计 {total_vis}）")
    print("# ORDER 追加行（也见 entries/ORDER.senro.txt）：")
    for line in ORDER_LINES:
        print(line)


if __name__ == "__main__":
    main()
