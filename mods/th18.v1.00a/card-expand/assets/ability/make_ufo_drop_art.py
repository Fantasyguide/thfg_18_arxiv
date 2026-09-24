#!/usr/bin/env python3
"""星莲船（系统卡 112）**掉落物实体**的贴图 + 动画脚本 —— TH12 v1.00b 原版 1:1 切图。

## 这一批是给谁用的（用户 2026-09-18 口径，`NEXT.md` §1x.8/§1x.9）

1. **击破时掉的「压缩包」**（用户原话：「击破还会掉落一个**被自动吸收的压缩包**（注意，**应该是有实体的**，
   只是被自动吸收过来）」）= 一手 TH12 的**赏金道具**（道具 type 16/17/18）。
2. **P6：绿 UFO 喂饱 ⇒ 整颗 Bomb**（一手 = 道具 **7**；用户：「有没有可能额外建一个掉落 Bomb 整卡的 VM
   （会小题大做吗？）」⇒ 答复：不小题大做，两者共用同一套「自建掉落物实体」）。

⇒ 两者在 TH12 里都是**道具**（在原生道具池里，带自己的 anm VM），而 TH18 里 **16/17/18 三个 type 全不可用**
（16 = 0x10 是 EXTEND2 卡道具、17/18 超出 type 上限 0x13）⇒ 我们**自建 VM**（= 用户说的那个「VM」）。

## 一手依据（全部可复核）

- 道具自身 anm 脚本号 = **type + `0xa5`(165)**：`th12:0x4275b2`（types10-15）/ `th12:0x427712`（types16-18）
  / `0x425c86`（state5→2 重建）/ `0x4264bc`（彩虹换色重建）—— 四处 `add ebx,0xa5`。
- 文件/entry：**`bullet.anm` 的 entry5，贴图 `bullet/etama8.png`（256×256）**。
  脚本号与 sprite 号都是**文件级全局编号**（`thanm` 源码注释明说 script 用 file 索引）。
- 脚本原文：`local/th12.v1.00b/anm/bullet/bullet.anm.txt`（**v4** anm；v8/v0 会把指令译错）。
- 精灵矩形：同一文件里 `name: "bullet/etama8.png"` 那个 entry 的 `sprites {}`。

### 贴图单元（一手矩形）

| TH12 sprite | 矩形 | 是什么 | 输出名 | 画布 |
| --- | --- | --- | --- | --- |
| **459** | (225,113,30,30) | 赏金**袋**（type 16/17/18 的主体）| `BOUNTY_BAG` | 32×32 |
| **426** | (0,96,16,16) | 袋子里冒出来的 **P 图标**（`script184`）| `DROP_ICON_P` | 16×16 |
| **427** | (16,96,16,16) | 袋子里冒出来的**点图标**（`script185`）| `DROP_ICON_POINT` | 16×16 |
| **432** | (96,96,16,16) | **整颗 Bomb**（道具 7）| `DROP_BOMB` | 16×16 |

### 脚本（逐条移植，只把 sprite 引用换成我们的名字）

| 输出 | 源 | 是什么 |
| --- | --- | --- |
| `296_ufo_bounty_bag` | script181 | 压缩包主体（袋 + 每 8 帧在 1.8/1.4 之间脉动 + 收走时 `interruptLabel(1)` 缩没）|
| `297_ufo_bounty_bag_p` | script181 | 上面那条，但**六个子图标全是 P**（= 一手 type 16「只有 cntA」那一支）|
| `298_ufo_bounty_bag_money` | script181 | 同上，六个子图标全是**点**（= 一手 type 17「只有 cntB」）|
| `299_ufo_drop_icon_p` | script184 | P 图标（`randMode(1)` + 每 10 帧随机飘 12 px、`layer(13)`）|
| `300_ufo_drop_icon_point` | script185 | 点图标（同形，只有 sprite 不同）|
| `301_ufo_bomb_item` | script172 | 整颗 Bomb 的道具外观（旋转 + 16 帧从 0.5 胀到 2.0、32 帧后停自转）|

⚠ `script181` 的 `layer(14)` / 子图标 `layer(13)` / `blendMode(1)` **逐条照抄**（一手值）。
⚠ 源脚本里的 `scriptNew(script184/185)` 换成我们的脚本名（本仓的写法：`@NAME@`，见 `build_ability.py`）。

## 输出

- `assets/ability/ufo_drop/{BOUNTY_BAG,DROP_ICON_P,DROP_ICON_POINT,DROP_BOMB}.png`
- ORDER 片段 `entries/ORDER.ufo_drop.txt`（**一个图集 entry、4 个成员**，照 `ORDER.txt` 的图集语法）
- 6 条脚本落 `assets/ability/scripts/`（已提交，别再改；改就改本文件再跑一遍）

跑法（幂等）：
    python3 assets/ability/make_ufo_drop_art.py          # 切图 + 打 ORDER 片段
    # 把 ORDER 片段拼进 entries/ORDER.txt 末尾（见本文件末尾的提示）
    make -C native anm                                   # 重编 ability.anm + anm_ids.h
"""
import re
import sys
from pathlib import Path

from PIL import Image

HERE = Path(__file__).resolve().parent
REPO = HERE.parents[4]
SRC_PNG = REPO / "local/th12.v1.00b/anm/bullet/bullet/etama8.png"
# ★★ 2026-09-23（owner 实跑第 5 条）：**整颗 Bomb 的外观 = 本作（TH18）自己的「完整 B 卡面」**。
#   ★★ 2026-09-23 第二轮（owner「要**有黑边那版**」）：源从 `BOMB_min.png`（64×80，外沿是卡自己的浅绿描边）
#     换成 **`BOMB_max.png`（256×320，画布外圈 3 px 是**纯黑边**，逐像素量过：外圈均值 RGB(0,0,0)；
#     `_min` 同一位置均值 (171,205,124)）** ⇒ 缩小到 40×44 之后黑边还在（`_min` 那版的黑线只有 1 px，
#     缩下去就糊掉了 —— 这正是 owner 说「要有黑边那版」的原因）。
#   ⚠ 画布尺寸同时从 32×32 改成 **40×44 = 原生卡道具贴图 `abcard/ability/abitem.png` 的尺寸**
#     （一手：`abcard.anm` entry1 = sprite1 (0,0,40,44)；owner 另一句「卡图大了一倍」= 我们的
#      `scaleTime(…,2.0)` 让屏显变成 64×64，而原生整 B 道具是 32×32 ⇒ 本轮把脚本也换成一手的
#     `abcard.anm` script16 配方，见 `assets/ability/scripts/301_ufo_bomb_item.anm.txt` 文件头）。
# ★★★ 2026-09-24 第 22 轮（owner 终局口径）：「**我要和 B 碎一样大**」「**它得在各方面 == B 碎卡**」
#   ⇒ 满 B 的外观**不再是我们的二次加工**，而是**原生卡道具那一套逐条复刻**：
#     一手 `abcard.anm`：**场地里那张卡道具**的显示配方 = script14（父，摆动）→ script16（背光，
#     加算 + 呼吸）/ script15（**卡面**，`scale(0.3,0.3)` + `resampleMode(0)`）/ script17（暗卡框）。
#     · **卡面** `DROP_BOMB` = 原生**整 B 卡图** `abcard/ability/BOMB_min.png` 的 sprite9 (0,0,64,80)
#       **1:1 原样**（不缩放！脚本里写 `scale(0.3,0.3)` —— 与原生同一张 texel、同一个倍率、
#       同一个采样模式 ⇒ 屏显 **19.2×24**，与 B 碎卡（`BOMB2_min.png`，同 64×80 同倍率）**逐像素同尺寸**）。
#     · **背光** `CARD_GLOW` = 原生光晕 `abitem.png` sprite1 (0,0,40,44) **1:1 原样**（脚本 `scale(1,1)`
#       ⇒ 屏显 40×44，与原生 script16 同值）。
#   ⚠ 为什么从 `BOMB_max.png`（256×320）换回 `_min`：owner 第 5 轮要的「**有黑边那版**」是**在卡被画大**
#     （40×50 / 26×32）时才有意义 —— `_max` 外圈 3 texel 纯黑，缩到 **19.2 px 宽**只有 **0.22 px**，
#     与 `_min`（浅绿描边）**肉眼无差**；而「和 B 碎一样大」只能靠原生那对同尺寸的 `_min` 卡图做到。
SRC_TH18_BOMB_CARD = REPO / "local/th18.v1.00a/anm/abcard/ability/BOMB_min.png"
# ★★ 2026-09-24（owner 第三轮）：「**你画的满B卡是 smjb，不会用这个？**」+ 给了 modkit 里的
#   `cardart/native/ability/BOMB_max.png`（**与本机 `local/` 那份逐字节同一 md5** `ed6b9481…`）
#   + 「**发光也不是 tm 这么发的啊**」 ⇒ 本轮把「卡面」与「背光」拆成**两个图集成员**：
#     · `DROP_BOMB` = **原生卡图本身**（`BOMB_max.png` 256×320，**不再重画、不再压扁**）——
#       等比装进 **40×50** 画布（256:320 = 40:50 ✓ 比例零损失）；
#     · `CARD_GLOW` = **原生卡道具的发光卡形贴图** `abcard/ability/abitem.png`（40×44，自带一圈柔光晕；
#       一手 = `abcard.anm` entry1 sprite1，卡道具显示脚本 script16 画的就是它）。
#   ★★★ 2026-09-24 第三/四轮（owner）：「你要不然直接做一张合成图吧，发光难道还有动画效果吗？」
#     →「卡大了一倍，没有看见发光」→ **「别搞合成图了还是重新背光吧」** ⇒ 回到**两层**（合成图作废）：
#     · **卡面** `DROP_BOMB` = 原生卡图 `BOMB_max.png`（256×320）等比缩到 **26×32**（4:5）。
#       为什么 26×32：原生**整 B 道具图标** = `bullet/item.png` sprite512 = 32×32（`scale 0.5→1.0`
#       ⇒ 屏显 32×32）；owner 判上一版 40×50「大了一倍」（面积 2000 vs 1024 ≈ 两倍）⇒ 取高 32。
#     · **背光** `CARD_GLOW` = 原生**光晕** `abcard/ability/abitem.png`（一手 = `abcard.anm` entry1
#       sprite1；owner 2026-09-24 亲自确认「**这张图本身确实是光晕**」），缩到 **34×40**（比卡面大一圈）
#       ⇒ 加法叠在卡面后面 = 一圈柔光（上一版背光比卡面小 ⇒ 整圈被盖住 = "看不见发光"）。
#       ★ owner 澄清了那个「五角星」的来历：**不是这张光晕**，而是**引擎自己画的原生整 B 图标**
#         （`bullet/item.png` sprite512 —— 那颗星星是 ZUN 从早期作品层垒下来的**整 B 美术**，
#          owner 原话「其他作品确实是用那个作为一颗 B 的」）⇒ 本卡把**原生那颗图标藏起来**
#         （`ufo.c` 的 `ufo_drops_tick` 每帧写 `item+0x534` 的 alpha = 0），只留「卡面 + 光晕」。
#       （中间那版一度换成 `abframe.png` 卡框，是误以为"星"出自这张光晕 —— owner 已澄清，换回。）
#     · 发光 = **真加算**（脚本 331 的 `blendMode(1)` + 呼吸，= 一手 `abcard.anm` script16 的配方）。
SRC_ABITEM = REPO / "local/th18.v1.00a/anm/abcard/ability/abitem.png"     # ★ 原生**光晕**（背光层用它）
SRC_ABCARD_TXT = REPO / "local/th18.v1.00a/anm/abcard/abcard.anm.txt"     # ★ 原生矩形（现读，见下）
# 原生卡图（卡面层）：`BOMB_min.png` = **整颗 B 的卡面**（`BOMB2_min.png` = B 碎的卡面，同 64×80 同倍率）
#   ⇒ owner「和 B 碎一样大」靠的是**同一张画布的同一倍率**，不是我们手调的像素数。
ABCARD_CARD_ENTRY = "ability/BOMB_min.png"
ABCARD_GLOW_ENTRY = "ability/abitem.png"
SRC_TXT = REPO / "local/th12.v1.00b/anm/bullet/bullet.anm.txt"
OUT = HERE / "ufo_drop"
SCRIPTS = HERE / "scripts"
ORDER_FRAG = HERE / "entries" / "ORDER.ufo_drop.txt"
ENTRY_NAME = "bullet/etama8.png"

# (输出名, TH12 sprite 号, 单元尺寸, 画布)  —— 矩形从 anm 文本读出来做断言，不写死在切图里
#   ★ `sid is None` 的成员 = **TH18 原生图**（矩形从 `abcard.anm.txt` 读，1:1 原样，**不缩放**）
SPRITES = [
    ("BOUNTY_BAG",      459, (30, 30), (32, 32)),
    ("DROP_ICON_P",     426, (16, 16), (16, 16)),
    ("DROP_ICON_POINT", 427, (16, 16), (16, 16)),
    # ★ DROP_BOMB 的源不是 TH12 的 sprite432（见 `SRC_TH18_BOMB_CARD`）：owner 口径 = TH18 原生整 B 卡面
    ("DROP_BOMB",       None, None,    (64, 80)),   # ★ 原生卡图 `BOMB_min.png` sprite9 (0,0,64,80) **1:1**
                                                    #   （脚本 345 写 `scale(0.3,0.3)` ⇒ 屏显 19.2×24 =
                                                    #    与原生 B 碎卡同尺寸，见本文件头 2026-09-24 第 22 轮）
    ("CARD_GLOW",       None, None,    (40, 44)),   # ★ 原生光晕 `abitem.png` sprite1 (0,0,40,44) **1:1**
                                                    #   （脚本 331 写 `scale(1,1)` ⇒ 屏显 40×44 = 原生 script16 同值）
]

# 图集排布（★ 2026-09-24 第 22 轮：卡面 20×25 → 64×80、背光 28×35 → 40×44 ⇒ 画布 80×96 → 128×128）
ATLAS_NAME = "ATLAS_UFO_DROP"
ATLAS_W, ATLAS_H = 128, 128
ATLAS_POS = {                       # 输出名 → (x, y)
    "BOUNTY_BAG":      (0, 0),
    "DROP_ICON_P":     (32, 0),
    "DROP_ICON_POINT": (48, 0),
    "DROP_BOMB":       (0, 32),     # 0+64=64 ≤ 128、32+80=112 ≤ 128 ✓
    "CARD_GLOW":       (64, 32),    # 64+40=104 ≤ 128、32+44=76 ≤ 128 ✓（与 DROP_BOMB 不重叠）
}

# (输出脚本号, 输出名, TH12 源脚本号, 图标脚本)  —— 图标脚本为 None = 整条照抄（只换 sprite 名）
SCRIPT_PORTS = [
    (296, "ufo_bounty_bag",       181, "mixed"),
    (297, "ufo_bounty_bag_p",     181, "p"),
    (298, "ufo_bounty_bag_money", 181, "money"),
    (299, "ufo_drop_icon_p",      184, None),
    (300, "ufo_drop_icon_point",  185, None),
    # ⚠ 301（ufo_bomb_item）**不在本表里**：它 2026-09-23 第二轮换成了一手的 TH18 卡道具配方
    #   （`abcard.anm` script16），不再是从 TH12 script172 移植来的 —— 那一版是手写文件，
    #   本生成器**不重写**它（跑生成器不会把它冲掉）。
]
ICON_P, ICON_POINT = "ICON_P", "ICON_POINT"     # 占位符（生成时换成 @脚本名@）


def read_sprite_rects() -> dict[int, tuple[int, int, int, int]]:
    if not SRC_TXT.is_file():
        raise SystemExit(f"缺 TH12 bullet.anm 的文本 {SRC_TXT}")
    txt = SRC_TXT.read_text(encoding="utf-8", errors="replace")
    m = re.search(r'name: "%s".*?sprites: \{(.*?)\n    \}' % re.escape(ENTRY_NAME), txt, re.S)
    if not m:
        raise SystemExit(f"{SRC_TXT}: 找不到 entry {ENTRY_NAME}")
    rects: dict[int, tuple[int, int, int, int]] = {}
    for sm in re.finditer(r"sprite(\d+): \{ x: (\d+), y: (\d+), w: (\d+), h: (\d+) \}", m.group(1)):
        rects[int(sm.group(1))] = (int(sm.group(2)), int(sm.group(3)),
                                   int(sm.group(4)), int(sm.group(5)))
    return rects


def read_abcard_rects() -> dict[str, tuple[int, int, int, int]]:
    """TH18 `abcard.anm` 的「entry 名 → 那一个 sprite 的矩形」—— 一手，不写死。

    ★ 2026-09-24 第 22 轮：卡面/背光的尺寸**必须**从原生 anm 文本现读（`sprite9: {0,0,64,80}` /
    `sprite1: {0,0,40,44}`）—— owner 的判据是「和 B 碎一样大」，而"一样大"= 原生卡面 64×80 乘
    脚本里的 0.3 ⇒ 一旦 ZUN 的矩形记错，这里当场红，不会悄悄画大一圈。"""
    if not SRC_ABCARD_TXT.is_file():
        raise SystemExit(f"缺 TH18 abcard.anm 的文本 {SRC_ABCARD_TXT}（先 tooling/thtk/unpack.py th18.v1.00a）")
    txt = SRC_ABCARD_TXT.read_text(encoding="utf-8", errors="replace")
    out: dict[str, tuple[int, int, int, int]] = {}
    for m in re.finditer(r'name: "([^"]+)"(.*?)\n\}', txt, re.S):
        name, body = m.group(1), m.group(2)
        sm = re.search(r"sprite\d+: \{ x: (\d+), y: (\d+), w: (\d+), h: (\d+) \}", body)
        if sm:
            out[name] = tuple(int(g) for g in sm.groups())     # type: ignore[assignment]
    return out


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


# ★ 2026-09-24 第 22 轮：卡面/背光**不再由我们缩**（`CARD_FIT`/`GLOW_FIT` 两个手调常数已删）——
#   取原生矩形 1:1，倍率写进脚本（0.3 / 1.0，逐值同原生）⇒ 与 B 碎卡同尺寸且同采样。


def make_sprites() -> list[str]:
    if not SRC_PNG.is_file():
        raise SystemExit(f"缺 TH12 原版贴图 {SRC_PNG}")
    rects = read_sprite_rects()
    abrects = read_abcard_rects()
    src = Image.open(SRC_PNG).convert("RGBA")
    if src.size != (256, 256):
        raise SystemExit(f"{SRC_PNG}: 期望 256×256，实际 {src.size}")
    # 原生矩形断言（一手 `abcard.anm`：卡面 64×80、光晕 40×44）
    for ent, want in ((ABCARD_CARD_ENTRY, (0, 0, 64, 80)), (ABCARD_GLOW_ENTRY, (0, 0, 40, 44))):
        got = abrects.get(ent)
        if got != want:
            raise SystemExit(f"abcard.anm 的 {ent} 矩形漂了：期望 {want}，实际 {got}")
    OUT.mkdir(parents=True, exist_ok=True)
    order_lines = []
    for name, sid, unit_wh, canvas_wh in SPRITES:
        if sid is None:                                # ★ DROP_BOMB（卡面）/ CARD_GLOW（背光）：原生图
            ent = ABCARD_GLOW_ENTRY if name == "CARD_GLOW" else ABCARD_CARD_ENTRY
            src_path = SRC_ABITEM if name == "CARD_GLOW" else SRC_TH18_BOMB_CARD
            if not src_path.is_file():
                raise SystemExit(f"缺 {src_path}（先 python3 tooling/thtk/unpack.py th18.v1.00a）")
            x, y, w, h = abrects[ent]
            if (w, h) != canvas_wh:
                raise SystemExit(f"{ent}: 原生矩形 {w}×{h} 与 SPRITES 里写的画布 {canvas_wh} 不一致")
            art = Image.open(src_path).convert("RGBA")
            unit = art.crop((x, y, x + w, y + h))      # ★ 1:1 原样（**不缩放、不重采样**）
            vis = sum(1 for a in unit.getchannel("A").tobytes() if a > 32)
            if vis < 60:
                raise SystemExit(f"{src_path}: 切出来的 {w}×{h} 只有 {vis} 个可见像素 —— 矩形错位")
            unit.save(OUT / f"{name}.png")
            order_lines.append(
                f"{name:<18} ability/ufo_drop/{name}.png   "
                f"@{ATLAS_POS[name][0]},{ATLAS_POS[name][1]},{canvas_wh[0]}x{canvas_wh[1]}")
            continue
        if sid not in rects:
            raise SystemExit(f"{SRC_TXT}: entry {ENTRY_NAME} 里没有 sprite{sid}")
        x, y, w, h = rects[sid]
        if (w, h) != unit_wh:
            raise SystemExit(f"sprite{sid} 的矩形是 {w}×{h}，期望 {unit_wh}")
        unit = src.crop((x, y, x + w, y + h))
        vis = sum(1 for a in unit.getchannel("A").tobytes() if a > 32)
        if vis < 60:
            raise SystemExit(f"sprite{sid} @({x},{y}) 可见像素只有 {vis} —— 切错位置了")
        canvas = Image.new("RGBA", canvas_wh, (0, 0, 0, 0))
        canvas.paste(unit, ((canvas_wh[0] - w) // 2, (canvas_wh[1] - h) // 2))
        canvas.save(OUT / f"{name}.png")
        # ORDER 行：图集成员 = `名字  源png  @x,y,WxH`（W×H = **画布**尺寸，图集本身就是画布拼的）
        order_lines.append(
            f"{name:<18} ability/ufo_drop/{name}.png   "
            f"@{ATLAS_POS[name][0]},{ATLAS_POS[name][1]},{canvas_wh[0]}x{canvas_wh[1]}")
    return order_lines


def port_script(lines: list[str], name_map: dict[int, str], icon: str | None,
                icon_scripts: dict[str, tuple[int, str]]) -> list[str]:
    """把一段 TH12 脚本正文逐条搬过来，只做两件事：
       ① `sprite(N)` → `sprite(@我们的名@)`（N 在表里才换）；
       ② `scriptNew(script184/185)` → 按 `icon` 选我们的图标脚本（p / money / mixed）。"""
    out: list[str] = []
    n_icon = 0
    for line in lines:
        if line.strip() == "}":
            break
        s = line
        m = re.search(r"sprite\(sprite(\d+)\)", s)
        if m:
            sid = int(m.group(1))
            if sid in name_map:
                s = s.replace(f"sprite(sprite{sid})", f"sprite(@{name_map[sid]}@)")
        m = re.search(r"scriptNew\(script(\d+)\)", s)
        if m:
            src = int(m.group(1))
            if src not in (184, 185):
                raise SystemExit(f"意外的 scriptNew(script{src})（这一批只预期 184/185）")
            if icon is None:
                which = "p" if src == 184 else "money"
            elif icon == "mixed":
                which = "p" if (n_icon % 2 == 0) else "money"
            else:
                which = icon
            n_icon += 1
            # ⚠ 占位名约定（照 `228_ufo_gauge_arc` / `231_ufo_gauge_bar` 的先例）：
            #   **脚本**引用 = `@SCRIPT_<名的大写>@`；**sprite** 引用 = `@<名的大写>@`（无前缀）。
            s = s.replace(f"scriptNew(script{src})",
                          f"scriptNew(@SCRIPT_{icon_scripts[which][1].upper()}@)")
        out.append(s)
    return out


def main() -> int:
    rects = read_sprite_rects()
    # 脚本里的 sprite 引用 → 我们的图集成员名。
    # ★★ 2026-09-23：`DROP_BOMB` 的**贴图来源**换成了 TH18 的完整 B 卡面（`SPRITES` 里它 sid=None），
    #   但**脚本 301 里引用的仍然是 TH12 的 `sprite432`**（一手脚本原文如此）⇒ 这里必须**单独**保留这条映射，
    #   否则 `port_script()` 找不到 432 ⇒ 生成出来的脚本会留下裸的 `sprite(sprite432)`
    #   （本仓的 ability.anm 里没有 432 号 sprite ⇒ 画出来是越界采样的杂图）。
    name_map = {sid: name for name, sid, _u, _c in SPRITES if sid is not None}
    name_map[432] = "DROP_BOMB"                       # ← 一手 script172 的 sprite 号（贴图换成 TH18 卡面）
    # sprite 号断言（把一手矩形钉死，改源文件会当场红）
    expect = {459: (225, 113, 30, 30), 426: (0, 96, 16, 16), 427: (16, 96, 16, 16)}
    for sid, want in expect.items():
        got = rects.get(sid)
        if got != want:
            raise SystemExit(f"sprite{sid} 矩形漂了：期望 {want}，实际 {got}")

    order = make_sprites()
    ORDER_FRAG.write_text(
        "# 卡 112 的掉落物实体贴图（TH12 bullet.anm entry5 = bullet/etama8.png 的 1:1 切图）\n"
        "#   生成器 = assets/ability/make_ufo_drop_art.py（矩形从 anm 文本读 + 断死）\n"
        f"ATLAS {ATLAS_NAME} {ATLAS_W}x{ATLAS_H}\n" + "\n".join(order) + "\n",
        encoding="utf-8")

    src_scripts = scripts_of(SRC_TXT.read_text(encoding="utf-8", errors="replace"))
    icon_scripts = {"p": (299, "ufo_drop_icon_p"), "money": (300, "ufo_drop_icon_point")}
    SCRIPTS.mkdir(parents=True, exist_ok=True)
    made = []
    for num, name, src_num, icon in SCRIPT_PORTS:
        if src_num not in src_scripts:
            raise SystemExit(f"源脚本 script{src_num} 不在 {SRC_TXT}")
        body = port_script(src_scripts[src_num], name_map, icon, icon_scripts)
        path = SCRIPTS / f"{num}_{name}.anm.txt"
        header = [
            f"script script{num} {{",
            f"    // 卡 112（星莲船）掉落物实体 —— TH12 v1.00b `bullet.anm` **script{src_num}** 的逐条移植",
            f"    // （`local/th12.v1.00b/anm/bullet/bullet.anm.txt`；生成器 assets/ability/make_ufo_drop_art.py）。",
            "    // 逐条照抄：只把 `sprite(N)` 换成我们的图集成员名、`scriptNew(N)` 换成我们的脚本名。",
            "    // 一手出处与用途见生成器文件头（压缩包 = 一手道具 16/17/18；整颗 Bomb = 一手道具 7）。",
        ]
        path.write_text("\n".join(header + body + ["}"]) + "\n", encoding="utf-8")
        made.append((num, name, src_num))
    print(f"切图 {len(SPRITES)} 张 → {OUT}")
    print(f"ORDER 片段 → {ORDER_FRAG}（图集一格 {ATLAS_W}x{ATLAS_H}）")
    for num, name, src_num in made:
        print(f"  script{num:<3} ← TH12 script{src_num}   {name}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
