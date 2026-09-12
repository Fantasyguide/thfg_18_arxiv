#!/usr/bin/env python3
"""星莲船（系统卡 112）**令牌**贴图 + 动画脚本 —— TH12 v1.00b 零售切图（资产缺口 A5 的贴图那一半）。

## 一手依据（全部可复核）

TH12 的「令牌」在引擎里是**道具**（不是敌机、也不在 ECL 里）：`engine/ufo/th12/01-ufo-system.md` §9
（一手地址 `th12:0x4273f0` 的 `ItemManager__spawn_item`）：

| 道具类型 | 颜色 | `bullet.anm` 脚本 | 谁掉 |
| --- | --- | --- | --- |
| 10 / 11 / 12 | 红 / 蓝 / 绿（固定色） | **175 / 176 / 177** | 敌人 `dropExtra(10..12, n)` |
| 13 / 14 / 15 | 红 / 蓝 / 绿（**彩虹令牌的三种状态**，每 ~150 帧换一种） | **178 / 179 / 180** | UFO 击破奖励 |

- 道具自身 anm 脚本 = **类型 + `0xa5`(165)**，读点 `th12:0x425c86`。
- 彩虹令牌换色 = `ItemManager__on_tick__body` `th12:0x4262f5`–`0x4264b6`：
  `type = ((type − 0xc) % 3) + 0xd`（13→14→15→13），玩家在 60 px 内时计时器暂停。
- 脚本原文：`local/th12.v1.00b/anm/bullet/bullet.anm.txt` 的 **script175..180**（用 `-m v4.anmm` 抽的，
  ⚠ TH12 必须用 **v4**，v8/v0 会把指令译错）。
- 精灵矩形：同一个文件里 `name: "bullet/etama8.png"`（256×256）那个 entry 的 `sprites {}`。

### 贴图单元（30×22，一手矩形）

| TH12 sprite | 矩形 | 是什么 | 输出名 |
| --- | --- | --- | --- |
| 444 / 447 | x=1/97, y=145 | 固定色令牌·红（两帧交替） | `TOKEN_RED_0/1` |
| 445 / 448 | x=33/129, y=145 | 固定色令牌·蓝 | `TOKEN_BLUE_0/1` |
| 446 / 449 | x=65/161, y=145 | 固定色令牌·绿 | `TOKEN_GREEN_0/1` |
| 453 / 456 | x=1/97, y=169 | **彩虹令牌**·红（亮一档，`alpha>32` 像素 250→342） | `TOKEN_RBRED_0/1` |
| 454 / 457 | x=33/129, y=169 | 彩虹令牌·蓝 | `TOKEN_RBBLUE_0/1` |
| 455 / 458 | x=65/161, y=169 | 彩虹令牌·绿 | `TOKEN_RBGREEN_0/1` |

两行同形异亮：`y=145` 那组可见像素 250、`y=169` 那组 342 ⇒ 后者带一圈更亮的光晕（= 彩虹令牌的外观）。
颜色用平均色自证：`y=145` 组 ≈(146,55,55)/(84,94,159)/(102,144,91)，`y=169` 组 ≈(143,73,73)/(99,106,153)/(112,143,104)。

## 输出

- `assets/ability/ufo/TOKEN_*.png`（12 张，各 **32×32** —— 源单元 30×22 居中放进 2 的幂画布）
- ORDER 片段 `entries/ORDER.ufo_token.txt`（12 行，`32x32`）
- 6 条脚本 `assets/ability/scripts/{168..173}_ufo_token_*.anm.txt`

## 与零售的差异（**逐条登记，别包装成「复刻」**）

1. **彩虹令牌的三段式被截断**：零售 script178/179/180 除了主循环，还有
   `interruptLabel(3)`（主循环）/ `interruptLabel(2)`（每 2 帧闪一次）/ `interruptLabel(7)`（收集爆环，
   用红固定色 sprite447）三段，由引擎在**别的时点**送中断切过去。本卡**目前没有**「给 anm VM 送中断」
   这条通路 ⇒ 只移植**主循环那一段**（到第一个 `jmp` 为止），`interruptLabel` 与被它标号的段**一律不抄**
   （抄了也永远进不去，反而是死代码）。这条差异标 ❓ 自定，登记在 `TUNING.md`。
2. **脚本号**：零售 175..180 → 本仓 `ability.anm` 的 168..173（本文件 `SCRIPTS` 表）。
3. `type(3)` / `scaleTime(8, 1, 1, 1)` / `rotateTime(32, 9, 0, 0, ±0.20943952)` / 每 4 帧切图 / `jmp` **逐条照抄**。

跑法：`python3 make_ufo_token_art.py`（幂等；可重复运行，产物逐字节稳定）。
"""

from __future__ import annotations

import re
import sys
from pathlib import Path

from PIL import Image

HERE = Path(__file__).resolve().parent
REPO = HERE.parents[4]                      # assets/ability → assets → card-expand → th18.v1.00a → mods → repo
SRC_PNG = REPO / "local/th12.v1.00b/anm/bullet/bullet/etama8.png"
SRC_TXT = REPO / "local/th12.v1.00b/anm/bullet/bullet.anm.txt"
OUT = HERE / "ufo"
SCRIPTS = HERE / "scripts"
ORDER_FRAG = HERE / "entries" / "ORDER.ufo_token.txt"

UNIT_W, UNIT_H = 30, 22                      # 零售单元尺寸
CANVAS = 32                                  # 输出画布（ORDER 要求 2 的幂）

# (输出名, TH12 sprite 号, 期望矩形) —— 矩形从 anm 文本读出来做断言，不写死在切图里
SPRITES = [
    ("TOKEN_RED_0", 444), ("TOKEN_RED_1", 447),
    ("TOKEN_BLUE_0", 445), ("TOKEN_BLUE_1", 448),
    ("TOKEN_GREEN_0", 446), ("TOKEN_GREEN_1", 449),
    ("TOKEN_RBRED_0", 453), ("TOKEN_RBRED_1", 456),
    ("TOKEN_RBBLUE_0", 454), ("TOKEN_RBBLUE_1", 457),
    ("TOKEN_RBGREEN_0", 455), ("TOKEN_RBGREEN_1", 458),
]

# (输出脚本号, 输出名, TH12 源脚本号, sprite 替换表)
#   ★ 彩虹那三条只取主循环：源脚本到第一个 `jmp(...)` 为止
SCRIPT_PORTS = [
    (168, "ufo_token_red", 175),
    (169, "ufo_token_blue", 176),
    (170, "ufo_token_green", 177),
    (171, "ufo_token_rbred", 178),
    (172, "ufo_token_rbblue", 179),
    (173, "ufo_token_rbgreen", 180),
]

ENTRY_NAME = "bullet/etama8.png"


def read_sprite_rects() -> dict[int, tuple[int, int]]:
    """从 `thanm -l` 的 spec 里读出 `ENTRY_NAME` 那个 entry 的 sprite 矩形。"""
    if not SRC_TXT.is_file():
        raise SystemExit(
            f"缺 TH12 bullet.anm 的文本 {SRC_TXT}（抽法：cd 到该目录，"
            f"local/vendor/thtk/build/thanm/thanm -l 12 -m local/vendor/thpages/static/mapfile/v4.anmm "
            f"../../dat/bullet.anm > bullet.anm.txt）"
        )
    txt = SRC_TXT.read_text(encoding="utf-8", errors="replace")
    m = re.search(
        r'name: "%s".*?sprites: \{(.*?)\n    \}' % re.escape(ENTRY_NAME), txt, re.S
    )
    if not m:
        raise SystemExit(f"{SRC_TXT}: 找不到 entry {ENTRY_NAME}")
    rects: dict[int, tuple[int, int]] = {}
    for sm in re.finditer(r"sprite(\d+): \{ x: (\d+), y: (\d+), w: (\d+), h: (\d+) \}", m.group(1)):
        rects[int(sm.group(1))] = (int(sm.group(2)), int(sm.group(3)))
    return rects


def scripts_of(text: str) -> dict[int, list[str]]:
    out: dict[int, list[str]] = {}
    cur = None
    for line in text.splitlines():
        m = re.match(r"script script(\d+) \{", line)
        if m:
            cur = int(m.group(1))
            out[cur] = []
            continue
        if cur is not None:
            out[cur].append(line)
            if line.strip() == "}":
                cur = None
    return out


def make_sprites() -> list[str]:
    if not SRC_PNG.is_file():
        raise SystemExit(f"缺 TH12 零售贴图 {SRC_PNG}")
    rects = read_sprite_rects()
    src = Image.open(SRC_PNG).convert("RGBA")
    if src.size != (256, 256):
        raise SystemExit(f"{SRC_PNG}: 期望 256×256，实际 {src.size}")

    OUT.mkdir(parents=True, exist_ok=True)
    order_lines = []
    for name, sid in SPRITES:
        if sid not in rects:
            raise SystemExit(f"{SRC_TXT}: entry {ENTRY_NAME} 里没有 sprite{sid}")
        x, y = rects[sid]
        unit = src.crop((x, y, x + UNIT_W, y + UNIT_H))
        vis = sum(1 for a in unit.getchannel("A").tobytes() if a > 32)
        if vis < 200:
            raise SystemExit(f"sprite{sid} @({x},{y}) 可见像素只有 {vis} —— 切错位置了")
        canvas = Image.new("RGBA", (CANVAS, CANVAS), (0, 0, 0, 0))
        canvas.paste(unit, ((CANVAS - UNIT_W) // 2, (CANVAS - UNIT_H) // 2))
        canvas.save(OUT / f"{name}.png")
        order_lines.append(f"{name}  ability/ufo/{name}.png  {CANVAS}x{CANVAS}")
    return order_lines


def make_scripts() -> list[tuple[int, str]]:
    if not SRC_TXT.is_file():
        raise SystemExit(f"缺 TH12 bullet.anm 的文本 {SRC_TXT}")
    src_scripts = scripts_of(SRC_TXT.read_text(encoding="utf-8", errors="replace"))
    # sprite 号 → 输出名
    by_sid = {sid: name for name, sid in SPRITES}
    # 源脚本 → 它用的 sprite 号（保证替换表自洽）
    SCRIPTS.mkdir(parents=True, exist_ok=True)
    written: list[tuple[int, str]] = []

    for out_no, out_name, src_no in SCRIPT_PORTS:
        if src_no not in src_scripts:
            raise SystemExit(f"{SRC_TXT}: 没有 script{src_no}")
        body = list(src_scripts[src_no])
        # `scripts_of` 收的是**花括号之间**的行（不含 `script scriptNN {` 那行，含收尾的 `}`）⇒ 先摘掉收尾 `}`
        if body and body[-1].strip() == "}":
            body = body[:-1]
        else:
            raise SystemExit(f"script{src_no}: 没找到收尾的 `}}` —— 抽取器坏了")
        # 彩虹族（178/179/180）有三段，靠引擎送中断切换；本卡没有这条通路 ⇒ 只留**主循环**那一段：
        #   ① 截到第一个 `jmp(...)` 之后为止（含那一行）；
        #   ② 把 `interruptLabel(N)` 行本身删掉（没有中断发给它 ⇒ 留着是死标记，且会让 reader 以为支持中断）。
        # 固定色那三条（175/176/177）没有 interruptLabel，原样整条移植。
        has_label = any("interruptLabel(" in ln for ln in body)
        if has_label:
            cut = None
            for i, ln in enumerate(body):
                if re.match(r"\s*jmp\(", ln):
                    cut = i + 1
                    break
            if cut is None:
                raise SystemExit(f"script{src_no}: 有 interruptLabel 却找不到第一个 jmp —— 手工核对")
            body = [ln for ln in body[:cut] if "interruptLabel(" not in ln]

        def sub(m):
            sid = int(m.group(1))
            if sid not in by_sid:
                raise SystemExit(f"script{src_no}: 用到未登记的 sprite{sid}")
            return f"sprite(@{by_sid[sid]})"

        new_body = [re.sub(r"sprite\(sprite(\d+)\)", sub, ln) for ln in body]
        # 断言：截断 + 删标记之后不允许再有 interruptLabel / 未替换的 sprite(
        for ln in new_body:
            if "interruptLabel(" in ln:
                raise SystemExit(f"script{src_no}: 截断后仍留 interruptLabel —— 检查截断点")
            if re.search(r"sprite\(sprite\d+\)", ln):
                raise SystemExit(f"script{src_no}: 还有没替换的 sprite 引用：{ln.strip()}")

        header = [
            f"script script{out_no} {{",
            f"    // 由 assets/ability/make_ufo_token_art.py 从 TH12 v1.00b `bullet.anm` script{src_no} 生成，别手改。",
            f"    // 一手：engine/ufo/th12/01-ufo-system.md §9（令牌 = 道具类型 10..15，道具自身 anm 脚本 = 类型 + 0xa5）。",
            f"    // 指令逐条照抄，只把 sprite 引用换成 ORDER 名字。",
        ]
        if has_label:
            header.append(
                "    // ⚠ 只移植**主循环**那一段：零售的 interruptLabel(3)/(2)/(7) 三段要靠引擎送中断切过去，"
                "本卡没有这条通路 ⇒ 不抄。登记见 make_ufo_token_art.py 文件头差异 1。"
            )
        # body 已经是「花括号之间」的正文（`script scriptNN {` 那行在 `scripts_of` 里就被丢掉了）
        path = SCRIPTS / f"{out_no}_{out_name}.anm.txt"
        path.write_text("\n".join(header + new_body + ["}"]) + "\n", encoding="utf-8")
        written.append((out_no, out_name))
    return written


def main() -> int:
    order_lines = make_sprites()
    names = make_scripts()
    ORDER_FRAG.write_text(
        "\n".join(
            [
                "# 星莲船（系统卡 112）**令牌**贴图 —— TH12 v1.00b `bullet.anm` script175..180 + `bullet/etama8.png` 切图。",
                "# 本片段由 assets/ability/make_ufo_token_art.py 生成（幂等，整段重写）；合并时把这些行追加进 ORDER.txt。",
                "# 一个 30×22 零售单元 → 一张 32×32 画布（居中）；不写尺寸会被当成 256×320 卡图。",
                "# 矩阵（一手矩形）：444/447 红·固定色 / 445/448 蓝 / 446/449 绿（y=145）；",
                "#                   453/456 红·彩虹 / 454/457 蓝 / 455/458 绿（y=169，亮一档）。",
            ]
            + order_lines
        )
        + "\n",
        encoding="utf-8",
    )
    print(f"ufo_token: {len(order_lines)} 张 32x32 PNG -> {OUT.relative_to(HERE.parent)}")
    print("  脚本：" + ", ".join(f"{n}_{s}" for n, s in names))
    print("\n# —— 追加到 assets/ability/entries/ORDER.txt（只追加）——")
    print("\n".join(order_lines))
    return 0


if __name__ == "__main__":
    sys.exit(main())
