#!/usr/bin/env python3
"""风神录（id 110）HUD —— **信仰读数**的原版切图（TH10 `front.anm` entry0 的图标 + 8×9 数字）。

    python3 make_mof_hud_art.py          # 出图
    python3 make_mof_hud_art.py --dump   # 附带把每个 sprite 的字形打成 ASCII（核证据用）

=====================================================================================
一手考证：TH10 的信仰读数（「信仰条」）到底是哪几个 VM / 哪几个 sprite
=====================================================================================

■ 结论（✅ 一手，逐条可复核）

  TH10 的信仰读数 = **`zGui` 里 `GUI+0x8094` 那一组共 7 个 VM**，脚本 = TH10 `front.anm`
  **script51..57**（一条 VM 一条脚本），贴图全部在 `front.anm` **entry0** = `front/front00.png`
  （512×512、`format: 5`）：

  | VM | 脚本 | sprite | 矩形 | `pos`（640×480 字面量空间）| anchor | layer | 角色 |
  | --- | --- | --- | --- | --- | --- | --- | --- |
  | 0 | 51 | sprite29 | (256, 80, 16, 16) | (32, 448) | (1,1)=左上 | 15 | **静态图标**（数字左侧）|
  | 1 | 52 | sprite30+d | (272+8d, 80, 8, 9) | (48, 450) | 同上 | 15 | 万的位 |
  | 2 | 53 | 同 | 同 | (55, 450) | 同上 | 15 | 千 |
  | 3 | 54 | 同 | 同 | (62, 450) | 同上 | 15 | 百 |
  | 4 | 55 | 同 | 同 | (69, 450) | 同上 | 15 | 十 |
  | 5 | 56 | 同 | 同 | (76, 450) | 同上 | 15 | 个 |
  | 6 | 57 | **sprite30（恒 '0'）** | (272, 80, 8, 9) | (83, 450) | 同上 | 15 | **静态末位 '0'** |

  `d` = 数字 0..9 ⇒ sprite30..39。**sprite40 是 ':' 之后的另一个字形、sprite41/42 不属本块**，
  本文件不切（`--dump` 只把它们打出来留证）。

■ 凭什么说这 7 个 VM 就是信仰读数（三条一手，互为独立）

  1. **值来源**：`Gui::on_tick` `th10:0x414a60` `mov ebp,[0x474c4c]` —— `0x474c4c` 就是
     信仰的**存储值**（`local/_scratch/research-th10-faith-and-graze.md` §1.1 的 16 处读写穷举里
     它是信仰本体；`set_faith` `0x418b80` 写得 `/10`，显示值 = 存储 ×10）。
     紧跟其后是 `edi+0x8094` 起的 5 次 `idiv 10000…1` 逐位取数 + `call 0x43e5a0`
     （= `AnmVm::set_sprite(vm, 数字+0x1e, anm)`，`0x43e5a0` 里写 `[vm+0x384]=dx` 并取
     `[anm+0x118]+sprite*0x44` 的 sprite 记录）—— 正好 5 个 VM（`mov ebx,5`）。
  2. **脚本装载**：`Gui::initialize` `th10:0x413d5e`–`0x413d7e` 对 `edi+0x8094` 起 **7 个**
     连续 VM（stride `0x3ac`）调 `0x43e710`（= `AnmLoaded::run_script`）。
  3. **末位恒 '0'**：第 7 个 VM（`GUI+0x969c`）**全 exe 没有任何一处写它的 sprite**
     （对 `+0x969c`/`+0x9a20` 的写入点搜索为空；组内其它 VM 由上面那条 `set_sprite` 更新）
     ⇒ 它永远显示 script57 里的 `sprite(sprite30)` = **'0'**。
     这一条与「显示值 = 存储值 ×10」**互相印证**：5 位动态 + 1 位恒 '0' = `存储 × 10`
     （初值存储 5000 → 画面 `05000` + `0` = **50000** ✅，正是 `0x417931 push 0xc350` 喂进去的数）。

  ⇒ 数字读数**不是** ASCII 数字，而是**原生 8×9 位图数字**（白字 + 1px 黑描边），
    这也是本文件要切图的原因。

■ `anchor(1,1)` = （左，顶）（✅ 一手，别按 TH18 的 3×3 直觉套）

  TH10 `front.anm` 的 HUD 边框条自己就是证明：script2 `sprite0 (0,0,32,480) pos(0,0) anchor(1,1)`、
  script3 `sprite1 (32,0,224,480) pos(416,0)`、script4 `sprite2 (0,480,384,16) pos(32,0)`、
  script5 `sprite3 (0,496,384,16) pos(32,464)` —— 四条**正好铺满 640×480 的四条边**
  （游玩区 x 32..416 / y 0..16 与 y 464..480），只有「pos = 矩形左上角」才成立。
  版本 v4/v7 的 `65 anchor` 取值表见 `engine/anm/OVERVIEW.md` §「锚点的跨版本演化」
  （0 中 / 1 左 / 2 右；0 中 / 1 顶 / 2 底），TH18（v8 `421 anchor`）同表。

■ 连击条（`0x4158b6`）**不是** anm 精灵，是引擎自己画的两个四边形

  `Gui::on_draw` `th10:0x4158b6`–`0x415937`：`fild [0x474c58]`（= `FAITH_TIMER.current`，
  上限 130）`fmul [0x470cb8]`（= **38/130 = 0.2923077f**）`fadd [0x470d2c]`（= 51.0f）
  ⇒ X = 51 + 槽 × 38/130；然后两次 `call 0x43bda0`（D3D 设备画实心四边形，`0x491c30` 是设备包装）
  画 (51, 461)–(X, 463) 与 (50, 460)–(X−1, 462)（颜色 = `[gui+0x8393]<<24 | 0xffffff` = 白）。
  ⇒ **原生贴图里没有这条**，我们的实现只能用 `drawRect`（脚本 104）拼出同样的两个四边形，
  坐标/宽度/颜色逐条照抄。**登记为「载体不同」（❓ 自定）**，见报告。

■ 产物尺寸（`build_ability.py` 的硬门：ORDER 里写死的尺寸必须是 2 的幂）

  | 产物 | 尺寸 | 为什么 |
  | --- | --- | --- |
  | `FAITH_ICON.png` | **16×16** | 源矩形本身就是 16×16（2 的幂）⇒ **原样切、不缩放、不重拼** |
  | `FAITH_DIGIT_0..9.png` | **8×16** | 源是 8×9：宽 8 已是 2 的幂 ⇒ **横向零补边**（7px 字距不被破坏）；高补到 16，**字形放在最上面 9 行**、下面 7 行透明 ⇒ `anchor(1,1)` 下字形左上角 = VM pos，与原生**逐像素同址** |

  ⚠️ 为什么不做「居中的 16×16」：那会把字形推到 (4,3)，卡侧每位数都要减这个偏移；
  8×16 让「VM pos = 原生 pos」这条式子直接成立（`front.anm` 里就是 8×9 的 sprite）。

■ 脚本侧的对应（`assets/ability/scripts/210_…` / `211_…`，逐条抄 script51 / script52）

  script51/52 原文（`thanm -l 10 local/th10.v1.00a/dat/front.anm -m v4.anmm`，v4 助记符）：

      script51 { sprite(sprite29); ins_85(1); randMode(1); pos(32,448,0); resampleMode(1);
                 anchor(1,1); layer(15); alpha(0);
               +20: alphaTime(20,0,255); stop();
                 interruptLabel(3): alphaTime(8,1,32); stop();
                 interruptLabel(2): alphaTime(8,1,255); stop(); }

  · `ins_85(1)`（v4 无名 flag）与 `randMode(1)`（v4 87 ↔ v8 307，v8 mapfile 注「无已知效果」）
    **没搬**：v4→v8 的 opcode 号不通用，按名字硬搬会静默指错 ⇒ 报告里登记为「未搬」。
  · `layer(15)` → **25**：15 属 TH18 的世界列表（相机视口 + 全分辨率），HUD 必须 24/25；
    25 = 组 38 = 桩 `th18:0x4878e0`（直接进后备缓冲）⇒ 会被暂停模糊罩（同组、后画）盖住，
    且进得了 P 键截图（依据 `engine/anm/th18/05-snapshot-and-surfaces.md` §5/§6/§7.1）。
  · 补 `resolutionMode(2)`（TH10 没有这条；TH18 的 25 层要显式写才按表面 px 定位）⇒
    脚本里的 `pos` 写 2×（(32,448) → (64,896)）。

■ 生成器自身的纪律

  · 切图只用**不带 mask** 的 `Image.paste` / `crop`（带 mask 时 PIL 会 alpha 合成、把
    「alpha 小、RGB 有值」的边缘像素预乘成暗色 —— 原生图 `colorKey: 0x00000000` 这种像素很多）。
  · 不缩放、不重采样、不重排：每个产物 = 源矩形内的**原字节** + （数字才有）透明补边。
  · 幂等：同一份源图连跑两次产物 md5 必须一致（内容全是常量决定，无随机）。
"""
import sys
from pathlib import Path

from PIL import Image

HERE = Path(__file__).resolve().parent
REPO = HERE.parents[4]
SRC = REPO / "local/th10.v1.00a/anm/front/front/front00.png"
OUT = HERE / "mof"

# 一手：TH10 `front.anm` entry0（front/front00.png，512×512、format 5）的 sprite 矩形
SHEET = (512, 512)
ICON = (256, 80, 16, 16)              # sprite29 —— 信仰读数左侧的静态图标（script51）
DIGIT_BASE = (272, 80)                # sprite30..39 —— 数字 0..9，每个 8×9、步进 8（x 方向）
DIGIT_SIZE = (8, 9)
DIGIT_CANVAS = (8, 16)                # 产物画布：宽 = 8（原样）、高 = 16（pow2），字形贴最上面 9 行


def _load():
    if not SRC.is_file():
        raise SystemExit(
            f"缺原版贴图 {SRC}\n（先 `python3 tooling/thtk/unpack.py th10.v1.00a`；"
            "本仓当前是手工解的，见报告「产物/证据」一节）"
        )
    im = Image.open(SRC).convert("RGBA")
    if im.size != SHEET:
        raise SystemExit(f"{SRC} 是 {im.size}，预期 {SHEET}")
    return im


def _crop(src, box):
    x, y, w, h = box
    return src.crop((x, y, x + w, y + h)).copy()


def _dump_digits(src):
    """把 8×9 的字形打成 ASCII（核「sprite30..39 就是 0..9」这条结论）。

    颜色记号：`W` = 白(255,255,255) 实心、`k` = 黑(0,0,0) 不透明描边、
    `:` = 黑 153 半透明（贴图自带的外描边）、`.` = 全透明。"""
    print("\n--- front00.png sprite30..39（(272,80) 起，每个 8×9，步进 8）---")
    for d in range(10):
        x = DIGIT_BASE[0] + d * DIGIT_SIZE[0]
        print(f"  sprite{30 + d}  = 数字 {d}   @({x},80,8,9)")
        for j in range(DIGIT_SIZE[1]):
            row = ""
            for i in range(DIGIT_SIZE[0]):
                px = src.getpixel((x + i, DIGIT_BASE[1] + j))
                r, g, b, a = px
                if a == 0:
                    row += "."
                elif (r, g, b) == (255, 255, 255):
                    row += "W"
                elif a >= 200:
                    row += "k"
                else:
                    row += ":"
            print("      " + row)
    x = ICON[0]
    print(f"  sprite29 = 静态图标 @{ICON}（左上 6×6 的 RGB 抽样，看色相用）")
    for j in range(0, 8, 2):
        print("      " + " ".join(
            "%3d,%3d,%3d,%3d" % src.getpixel((x + i, ICON[1] + j)) for i in range(0, 6)))


def main():
    dump = "--dump" in sys.argv
    src = _load()
    OUT.mkdir(exist_ok=True)

    # ① 图标：16×16 原样切（2 的幂，零补边）
    _crop(src, ICON).save(OUT / "FAITH_ICON.png")

    # ② 数字 0..9：8×9 原样贴进 8×16 画布的最上面 9 行（下面 7 行透明）
    for d in range(10):
        box = (DIGIT_BASE[0] + d * DIGIT_SIZE[0], DIGIT_BASE[1], DIGIT_SIZE[0], DIGIT_SIZE[1])
        tile = _crop(src, box)
        canvas = Image.new("RGBA", DIGIT_CANVAS, (0, 0, 0, 0))
        canvas.paste(tile, (0, 0))          # ★ 不带 mask（见文件头「生成器自身的纪律」）
        canvas.save(OUT / f"FAITH_DIGIT_{d}.png")

    print("mof-hud: FAITH_ICON.png 16x16 = TH10 front.anm entry0 sprite29 (256,80,16,16)")
    for d in range(10):
        print(f"mof-hud: FAITH_DIGIT_{d}.png 8x16 = sprite{30 + d} "
              f"({DIGIT_BASE[0] + d * 8},80,8,9) 贴在画布最上面 9 行（下面 7 行透明）")
    print("\n把下面几行追加到 entries/ORDER.txt（或合并 entries/ORDER.mof.txt）：")
    print("MOF_FAITH_ICON     ability/mof/FAITH_ICON.png     16x16")
    for d in range(10):
        print(f"MOF_FAITH_DIGIT_{d}    ability/mof/FAITH_DIGIT_{d}.png    8x16")

    if dump:
        _dump_digits(src)


if __name__ == "__main__":
    main()
