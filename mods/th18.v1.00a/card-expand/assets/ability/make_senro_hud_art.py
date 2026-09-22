#!/usr/bin/env python3
"""妖妖梦（TH07 系统卡 107）**HUD 数字 + 固定标签 + 森罗结界指示**的贴图：从 TH07 原版 `ascii.anm` 里 1:1 切。

    python3 make_senro_hud_art.py

一手来源（全部在 `local/`，gitignored，构建时取；不入库）
────────────────────────────────────────────────────────
* 原版 anm：`local/th07.v1.00b/dat/ascii.anm`（anm **version 2**，2 个 entry）
    切好的贴图：`local/th07.v1.00b/anm/ascii/data/ascii/ascii.png`（256×256）
* anm 无内嵌贴图 → 用 `thanm -x 7` 兜底解到临时目录（不往 `local/` 写）。

**这三组贴图是谁**（一手 = `AsciiManager::sub_404690` = TH07 画「樱点计数器」的那个函数）：

| 贴图 | 原版 sprite | 用途（一手地址） |
| --- | --- | --- |
| `D0.png`..`D9.png` | `ascii.anm` entry0 **sprite132..141**（各 **8×12**） | HUD 的**数字字形**。画数字时 `sprite = digit + 0x84`（`th07:0x404a41 add edx,0x84` / `0x404d7d` 同一处，两行数字 + +C 行都用它） |
| `BORDER.png` | entry0 **sprite143**（**48×16**） | **森罗结界指示**：结界中才画的那块（VM4 = `ascii.anm` **script5**，`ins_36(30,4,1.2,1.2)` ↔ `(30,1,0.8,0.8)` 无限脉动）。一手判据 = `th07:0x404de1 movsx eax,BYTE PTR ds:0x4bfee5; cmp eax,1; jne` 才画 ⇒ 只在 `ds:0x4bfee5 == 1`（= **结界中**，同址也是 `th07:0x42f5f7` 的 +C 累加闸门）时出现。**内容**（2026-09-22 逐像素读出）= 两朵粉色樱花（sprite 内 `x6..22` 与 `x26..45`，`y1..15`），不是"板子"——它是**装饰性的花**，脉动盖在计数器左半上 |
| `LABEL.png` | entry0 **sprite142**（**96×16**） | **数字簇的锚 / 固定标签**：VM2（`AsciiManager +0x498`）在 `th07:0x401be3` 被装 **script4**，正文第一句 = `ins_3(sprite142)`；script4 再把它从 `(-96,449)` 用 `posTime(15,0,32,449)` 滑到 **(32,449)** 定住。**HUD 两行数字全部按 `[VM2.pos] + 常数` 摆**（`T.x+46/T.y+11` 樱点、`T.x+97` 最大樱点、`T.x+53/T.y+2` 的 +C）⇒ 这块就是计数器自己的底板 |

**为什么是 sprite132..141 而不是别的**（三条独立证据）：
1. `AsciiManager::sub_401ba0` `th07:0x401ba0` 给内嵌 VM3（`this+0x6e4`）装 **script3**（`th07:0x401c23`），
   而 `script3` 的正文只有 `ins_3(sprite132); ins_30(1); ins_2();` ⇒ VM3 就是「一个数字」的载体。
2. 画数字那两段循环里 `sprite = 数字 + 0x84`（`0x404a41`/`0x404d7d`），共 6+6+5 位 ⇒ 132..141。
3. 贴图侧：`sprite132..141` = `(0,208)..(72,208)` 十个 **8×12** 等距字形（本脚本从 `thanm -l` 读矩形断死）。

**sprite142 画的是什么**（2026-09-22 逐像素读出来，✅ 一手；本模型不能看图，判读手段 =
把 alpha/luminance 打成字符画 + `thanm -l 7` 的矩形 + 代码里的字段偏移三者对位）：

| 元素 | sprite 内矩形（左上角起）| 对上哪一段 |
| --- | --- | --- |
| 单词 **`Cherry`**（大写 C + 小写 herry，字高 7 px，`y` 带下延到第 13 行）| `x1..36, y5..13` | **樱点字段的左邻**：樱点字段首字框 = `T.x+42`（居中锚 ⇒ `±4`）⇒ 与标签不打架 |
| 粉色加号 **`+`**（5×5）| `x41..45, y1..5` | **+C 行的前缀**：+C 字段首字框 = `T.x+49` ⇒ 加号正好在它左边 4 px |
| 粉色斜杠 **`/`**（3×8，向左下斜）| `x86..90, y6..13` | **樱点 / 最大樱点之间的分隔符**：樱点字段末字右沿 `T.x+85`、最大字段首字左沿 `T.x+93` ⇒ 斜杠落在 8 px 空档正中（空档中心 89、斜杠中心 88.5）|
| 一层**软投影**（暗、半透明）| 约 `x1..90, y3..15` | 数字压在上面时仍读得清（贴图自带，不用另做）|

★ 这三条对位（加号 ↔ +C 字段、`Cherry` ↔ 樱点字段、斜杠 ↔ 两字段之间那 9 单位空档）**只有把数字按
「居中锚 + 代码里那些死偏移」摆才同时成立** ⇒ 它反过来**反证了下面这张锚点表读对了**。
交叉印证：THBWiki 对这块的描述「**Cherry** 右侧的分子 = 樱点、分母 = 樱点最大值」逐条对上（只作语义印证）。

**锚点（★ 2026-09-22 落到 opcode 级，✅ 一手）** —— 三台 VM 的锚点**不一样**，抄错就是半个字形：

| VM | 谁 | TH07 的锚点 | 一手判据 |
| --- | --- | --- | --- |
| **VM2** = 标签（script4 / sprite142）| 数字簇的锚 | **左上角** | script4 正文第 3 句 = `ins_22()`；opcode 22 的实现 = `th07:0x451b51`：`mov eax,[vm+0x1c0]; or eax,0xc00; mov [vm+0x1c0],eax` ⇒ 置 flags 位 10/11；`AnmVm::Draw` `th07:0x44f770` 用这两位选锚：位 10 置 ⇒ 左边 = `pos.x`（否则 `pos.x − w/2`，`0x44f80d`/`0x44f849`）、位 11 置 ⇒ 上边 = `pos.y`（`0x44f89b`/`0x44f8d9`）|
| **VM3** = 数字（script3 / sprite132..141）| 17 个数字位 | **居中（锚心）** | script3 正文 = `ins_3(sprite132); ins_30(1); ins_2();` —— **没有 `ins_22`**；而 `AnmLoaded::load_script_into_vm` `th07:0x44ea20` 装脚本时先 `flags &= 0xfffffcff` 再调 `AnmVm::reset` `th07:0x4010f0`（它把 `[vm+0x1c0]` 写成 **0x7**，位 10/11 = 0）⇒ 居中 |
| **VM4** = 脉动花（script5 / sprite143）| 结界指示 | **居中** | script5 正文同样没有 `ins_22` ⇒ 同上 |

⇒ **本卡脚本照抄这套锚点**：`306`（数字）与 `307`（花）不写 `anchor` 时 TH18 的**精灵默认锚 = 锚心**
（实跑证据见 `mods/th18.v1.00a/card-expand/AUDIT.md` §AL20：火焰/槽内图标「图心 = 槽心」）**正好等于
TH07 的这两台**，所以那两支脚本一行都不用改；新脚本 `308`（标签）是唯一那台**左上角锚**，
必须**显式写 `anchor(1, 1)`**（TH18 的锚点 = `flags_lo` 位 22–23 / 24–25，0 = 中 / 1 = 左·上 / 2 = 右·下）。

尺寸 / 缩放（为什么脚本里要 ×2）
─────────────────────────────────
* 原样切、不重采样（纯原生切图）——**8×12 / 48×16 / 96×16 就是原版矩形尺寸**。
* TH07 是 640×480 原生：1 texel = 1 个「640 单位」；TH18 的 UI 层 `resolutionMode(2)` 下
  1 texel = 1 **表面 px**（1280×960），而 640 单位 = 2 表面 px ⇒ 想还原原版观感要在脚本里
  `scale(2.0, 2.0)`（先例 = `scripts/302_ufo_hud_slot.anm.txt` 的 Δ4 同一条换算）。

产物：`ability/senro_hud/D0.png`..`D9.png`、`BORDER.png`、`LABEL.png`；
stdout 打印要追加进 `entries/ORDER.txt` 的两段（`ORDER.senro_hud.txt` 由本脚本**直接写出**，别手改）。
⚠ **标签单独一段 ATLAS**（`ATLAS_SENRO_HUD_LABEL`，96×16、一个成员）：`ORDER.txt` 只许**追加**，
既有那段的画布行不许改 ⇒ 新成员另起一段（多占 1 个 entry，48/256 仍宽裕）。
幂等：可重复运行，输出逐字节一致。
"""
import re
import subprocess
import sys
import tempfile
from pathlib import Path

from PIL import Image

HERE = Path(__file__).resolve().parent
REPO = HERE.parents[4]

ANM = REPO / "local/th07.v1.00b/dat/ascii.anm"
THANM = REPO / "local/vendor/thtk/build/thanm/thanm"
SRC = REPO / "local/th07.v1.00b/anm/ascii/data/ascii/ascii.png"
OUT = HERE / "senro_hud"
SNIPPET = HERE / "entries/ORDER.senro_hud.txt"

# 段 1（已在 ORDER.txt 里）：数字字形 + 结界指示。成员名 → (原版 sprite 号, 期望尺寸, 含义)
CROPS = ([("D%d" % i, 132 + i, (8, 12), "HUD 数字字形 %d" % i) for i in range(10)]
         + [("BORDER", 143, (48, 16), "森罗结界指示（结界中才脉动的那两朵樱花）")])
# 段 2（2026-09-22 追加）：数字簇的固定标签 —— 单独一段 ATLAS（ORDER.txt 只许追加）
CROPS_LABEL = [("LABEL", 142, (96, 16), "数字簇的固定标签（VM2 / script4）= Cherry + 加号 + 斜杠 + 投影")]

SNIPPET_HEAD = """\
# 妖妖梦（TH07 系统卡 107）**HUD 数字字形 + 固定标签 + 森罗结界指示**的贴图 —— 由
# assets/ability/make_senro_hud_art.py 生成，别手改。
# 格式同 entries/ORDER.txt：`ATLAS <名> <画布WxH>` + 段内 `成员名 源图 @x,y,WxH`。
# ⚠️ 本文件**不参与构建**（`build_ability.py` 只读 entries/ORDER.txt）—— 合并时把下面两段**追加**进去。
#
# 一手（全部在 TH07 v1.00b 原版里，逐条见生成器文件头）：
#   D0..D9 = `ascii.anm` entry0 sprite132..141（8×12）—— TH07 画樱点计数器时的数字字形：
#            画数字的 `sprite = digit + 0x84`（`th07:0x404a41` / `th07:0x404d7d`），
#            而 VM3（`AsciiManager` +0x6e4）在 `th07:0x401c23` 被装上的 script3 正文 =
#            `ins_3(sprite132); ins_30(1); ins_2();` ⇒ 一个数字一台 VM、换 sprite 出数字。
#   BORDER = entry0 sprite143（48×16）—— **森罗结界指示**：`AsciiManager` 的 VM4（+0x930）在
#            `th07:0x404de1` 判 `ds:0x4bfee5 == 1`（= 结界中）才画，位置 = 数字锚 +(24,8)，
#            脚本 = `th07:0x401c62` 装上的 script5（`scaleTime(30,4,1.2,1.2)` ↔ `(30,1,0.8,0.8)` 无限脉动）。
#   LABEL  = entry0 sprite142（96×16）—— **数字簇的固定标签**：VM2（`AsciiManager` +0x498）在
#            `th07:0x401be3` 被装上 script4，正文 = sprite142 + `ins_22()`（**左上角锚**）+ 滑到 (32,449)。
#            内容（逐像素读出）= 单词 `Cherry` + 粉色加号 `+` + 粉色斜杠 `/` + 一层软投影：
#              加号 ↔ +C 字段前缀、`Cherry` ↔ 樱点字段、斜杠 ↔ 樱点/最大樱点之间那 9 单位空档。
#
# 尺寸：原样 1:1 切（8×12 / 48×16 / 96×16）。TH07 是 640×480 原生、TH18 UI 层 `resolutionMode(2)` 下
#   1 texel = 1 表面 px ⇒ 脚本里用 `scale(2.0, 2.0)` 还原原版观感（先例 = 302 的 Δ4 同款换算）。
"""


def sprite_rects() -> dict:
    """从 `thanm -l 7` 的 entry0 读全部 sprite 矩形（**不硬编码**，矩形错了就报错）。"""
    if not (ANM.is_file() and THANM.is_file()):
        raise SystemExit(f"缺 {ANM} 或 {THANM}（TH07 样本 + thtk 工具链）")
    txt = subprocess.run([str(THANM), "-l", "7", str(ANM)],
                         capture_output=True, text=True, check=True).stdout
    rects, entry = {}, None
    for line in txt.split("\n"):
        m = re.match(r"\s*entry entry(\d+)", line)
        if m:
            entry = int(m.group(1))
            continue
        m = re.match(r"\s*sprite(\d+): \{ x: (\d+), y: (\d+), w: (\d+), h: (\d+)", line)
        if m and entry == 0:
            rects[int(m.group(1))] = tuple(int(m.group(i)) for i in (2, 3, 4, 5))
    if len(rects) < 146:
        raise SystemExit(f"entry0 只读到 {len(rects)} 个 sprite，预期 ≥146（ascii.anm 变了？）")
    return rects


def texture() -> Image.Image:
    if SRC.is_file():
        return Image.open(SRC).convert("RGBA")
    with tempfile.TemporaryDirectory() as td:                     # 兜底：直接解 anm，不往 local/ 写
        subprocess.run([str(THANM), "-x", "7", str(ANM)], cwd=td, check=True,
                       stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
        p = Path(td) / "data/ascii/ascii.png"
        if not p.is_file():
            raise SystemExit(f"缺 {SRC}，且 thanm 解包没产出 ascii.png")
        return Image.open(p).convert("RGBA").copy()


def pack_rects(crops: list, width_limit: int = 256) -> tuple:
    """把成员摆进图集画布（显式矩形，行优先；画布 = 各行的最大宽度 × 行高之和）。

    段 1 = 十个 8×12 数字 + 一块 48×16（一行放完，画布 128×16）；
    段 2 = 一块 96×16（一行，画布 96×16）。
    产物 `([(name, sid, (x, y, w, h))], 画布)`。"""
    out, x, row_y, row_h = [], 0, 0, 0
    for name, sid, (w, h), _label in crops:
        if x + w > width_limit:                             # 画布宽上限（本段用不到，留着防手滑）
            row_y += row_h + 2
            x, row_h = 0, 0
        out.append((name, sid, (x, row_y, w, h)))
        x += w
        row_h = max(row_h, h)
    canvas = (max(x, 1), row_y + max(row_h, 1))
    return out, canvas


def emit(img, rects, crops, atlas: str, prefix: str) -> list:
    """切一组、存图、打印逐张信息，返回要追加进 ORDER.txt 的那几行。"""
    layout, canvas = pack_rects(crops)
    order_lines = []
    for name, sid, (dx, dy, w, h) in layout:
        x, y, sw, sh = rects[sid]
        if (sw, sh) != (w, h):
            raise SystemExit(f"{name}: sprite{sid} 矩形是 {(sw, sh)}，预期 {(w, h)}（表变了？）")
        tile = img.crop((x, y, x + sw, y + sh))
        if tile.getbbox() is None:
            raise SystemExit(f"{name}: sprite{sid} {(x, y, sw, sh)} 切出来全透明（矩形抄错了）")
        tile.save(OUT / f"{name}.png")
        ink = sw * sh - tile.getchannel("A").histogram()[0]
        label = [c[3] for c in crops if c[0] == name][0]
        print(f"# {name:<7} sprite{sid:<4} 源({x},{y},{sw},{sh}) → 画布 @{dx},{dy},{w}x{h}"
              f"  墨迹 {ink:>4}/{w * h:<4} ← {label}")
        # 列宽照既有 ORDER.txt 的写法（名字段 22 列 + 源图 + 补齐），别让新行的排版和旧行不一致
        order_lines.append(f"{prefix + name:<22} ability/senro_hud/{name}.png"
                           f"{' ' * max(1, 12 - len(name))}@{dx},{dy},{w}x{h}")
    print(f"{atlas}: {len(crops)} 张 -> {OUT.relative_to(HERE.parent)}（画布 {canvas[0]}x{canvas[1]}）")
    return [f"ATLAS {atlas} {canvas[0]}x{canvas[1]}"] + order_lines


def main():
    rects = sprite_rects()
    img = texture()
    if img.size != (256, 256):
        raise SystemExit(f"{'ascii.png'} 是 {img.size}，预期 (256, 256)")
    OUT.mkdir(exist_ok=True)

    sec1 = emit(img, rects, CROPS, "ATLAS_SENRO_HUD", "SENRO_HUD_")
    sec2 = emit(img, rects, CROPS_LABEL, "ATLAS_SENRO_HUD_LABEL", "SENRO_HUD_")

    print("# ORDER 追加行（**只许追加到 entries/ORDER.txt 末尾**；本脚本同时写出 entries/ORDER.senro_hud.txt）：")
    for line in sec1 + [""] + sec2:
        print(line)

    SNIPPET.write_text(SNIPPET_HEAD + "\n".join(sec1) + "\n\n" + "\n".join(sec2) + "\n",
                       encoding="utf-8", newline="\n")
    print(f"# 片段文件已写出：{SNIPPET.relative_to(REPO)}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
