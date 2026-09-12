#!/usr/bin/env python3
"""地灵殿（id 111）HUD —— **信号槽**的零售切图（TH11 `front.anm` entry0 的槽底 / 增长条 / 图标）。

    python3 make_signal_hud_art.py          # 出图
    python3 make_signal_hud_art.py --dump   # 附带把每个 sprite 打成 ASCII（核证据用）

> ⚠ 任务书里把 111 写成「星莲船」，本仓的 111 = **地灵殿（TH11 SA）**、112 才是星莲船（TH12 UFO）
>   （`CARDS.md` 第 255 行：`| 111 | 地灵殿 | **地灵殿信号槽** …`）。信号槽（通信ゲージ）本来就是
>   **SA 的系统**、TH12 没有它 ⇒ 本文件按 **TH11 `front.anm`** 切。报告里已登记这条订正。

=====================================================================================
一手考证：TH11 的信号槽 = `front.anm` script32..35（4 个 VM），贴图在 entry0
=====================================================================================

■ 结论（✅ 一手，逐条可复核）

  `Gui::initialize` `th11:0x41a764`–`0x41a78e`：对 `gui+0x2e4c` 起 **4 个**连续 VM（stride `0x434`）
  循环 `call 0x44acd0`（= `AnmLoaded::run_script(vm, script, anm)`），脚本号 = `esi + 0x20` = **32..35**，
  anm = `[gui+0x444c]`（加载好的 HUD anm = `front.anm`）。
  `Gui::on_tick` `th11:0x41b55d`–`0x41b577` 每帧对同一组 4 个 VM 调 `0x44b4b0`（`AnmVm::run`）。

  | VM | 脚本 | sprite | 矩形 | `pos`（640×480）| anchor | layer | 角色 |
  | --- | --- | --- | --- | --- | --- | --- | --- |
  | 0 | 32 | **sprite57** | (416, 16, 48, 16) | (42, 456) | (1,2)=左下 | 19 | **槽底**（`color(32,32,64)` 压暗；`alpha(0)` + 20 帧淡入）|
  | 1 | 33 | **sprite58** | (416, 0, 48, 16) | (42, 456) | (1,2) | 18 | **增长条**：宽度 = 引擎每帧写（见下）；颜色按档位闪 |
  | 2 | 34 | **sprite59** | (416, 0, 48, 16) ← **与 58 同矩形** | (42, 456) | (1,2) | 18 | 第二层（同像素的另一份编号）|
  | 3 | 35 | **sprite26** | (256, 80, 16, 16) | (30, 468) | (1,2) | 18 | **静态图标**（槽左下方）|

  ⇒ 零售贴图里**没有**「数字读数」：SA 的信号就是用这排小竖条表示的。

■ `anchor(1,2)` = （左，底）（✅ 一手，同 TH10 那条的证法）

  TH11 `front.anm` 的 HUD 边框条：script1 `sprite0 (0,0,32,480) pos(0,0) anchor(1,1)`、
  script2 `sprite1 (32,0,224,480) pos(416,0)`、script3 `sprite2 (0,480,384,16) pos(32,0)`、
  script4 `sprite3 (0,496,384,16) pos(32,464)` —— 四条铺满屏幕四边 ⇒ `anchor(1,1)` = （左，顶）、
  `anchor(1,2)` = （左，底）。取值表见 `engine/anm/OVERVIEW.md`（0 中 / 1 左 / 2 右；0 中 / 1 顶 / 2 底）。

■ 增长条怎么长（✅ 一手：`Gui::on_tick` `th11:0x41b679`–`0x41b720`）

      sig  = [GLOBALS+0x14]（`0x4a56f4`，整数）
      pct  = min(sig, 10000) 时取 100，否则 sig*100/1000        ; 0..100 的百分数
      pct10= pct / 10                                            ; 0..10（十个档）
      [vm1+0x4c] = 9 + 4*pct10                                   ; ← 精灵的**横向尺寸**（px）
      [vm1+0x78] = [vm1+0x88] = [vm1+0x70] + (4*pct10 + 9)/512   ; 微调（同源，量级 0.02 px）
      [vm1+0x404] |= 8 ;  [vm2+0x404] |= 8                       ; 标脏（TH16 字段图里 bit3 = scale 脏位）
      [vm2+0x4c] = 0.0                                           ; ← VM[2] 每帧被写成**零宽**

  `AnmVm::write_sprite_corners__without_rot` `th11:0x44fe33` 里 `[vm+0x4c] × [vm+0x3c]` 就是
  「精灵宽 × scale.x」⇒ **+0x4c 是精灵的横向尺寸**：零售把 48 px 的条压到 **9 + 4×档** px
  （4 px = 条纹上一条竖线的间距 ⇒ **信号每 +10 点亮一根条**，起点 9 px 是首块的宽度）。
  ⇒ 我们的等价写法 = `ce_anm_set_scale(bar, (9 + 4*档)/48, 1.0)`（同 `anchor(1,2)` ⇒ 左端钉住）。

  ⚠️ **VM[2] 被零宽这件事照抄**：零售每帧 `fldz; fst [vm2+0x4c]` ⇒ 它正常游玩里不可见。
  我们的实装**不出这个 VM**（省一个不可见 VM），但贴图只出一份（58/59 同矩形）—— 报告里登记。

■ 档位闪色（✅ 一手：`Gui::on_tick` `th11:0x41b593`–`0x41b66e`，直接写 `[vm+0x37c]`）

  `[vm+0x37c]` = **待处理的中断号**（`AnmVm::run` `th11:0x44b4f9` `movzx ecx,word [edi+0x37c]`），
  而 script33/34 里的 `interruptLabel(N)` 正好有 7/8/9/10/11：

      pct < 100  → 中断 7   （script33: color(255,255,255) —— 常态白）
      pct < 110  → 中断 8   （5 帧 黄↔红 闪）
      pct < 120  → 中断 9   （3 帧 黄↔红 闪）
      pct >= 120 → 中断 10  （2 帧 黄↔红 闪）
      （中断 11 = 蓝↔品红 2 帧闪：**全 exe 只在 interruptLabel 里定义，没找到发它的地方** ❓）

  ⇒ 槽越满闪得越快；这条状态机是逐条照抄进 `signal.c` 的（只写一次，边沿触发，见那边注释）。

■ 淡出/淡入（✅ 一手：`Gui::on_tick` `th11:0x41b4a1`–`0x41b55a`）

      显示态（flag `gui+0x4420` bit0 = 0）：
          自机 y > 416.0 且 x < −64.0  ⇒  给 4 个 VM 写中断 **3**（script32/33/34/35 都是
                                         `alphaTime(10,0,64)` = 压暗到 alpha 64）并置 bit0
      压暗态（bit0 = 1）：
          自机 y < 400.0 **或** x > −64.0 ⇒  写中断 **2**（= `alphaTime(30,0,255)` 淡回）并清 bit0

  即「自机贴到左下角（HUD 那一块）时整块压暗」。**y 的门限 416/400、x 的门限 −64/−64 与
  SDK 的 `ce_hud_near_corner()`（TH16 栅格 400/384）不是同一组** ⇒ 111 自己按 TH11 的值实现
  （共享件不动，报告里登记这条差异）。

■ 产物尺寸（`build_ability.py` 的硬门：ORDER 里写死的尺寸必须是 2 的幂）

  | 产物 | 尺寸 | 为什么 |
  | --- | --- | --- |
  | `SIGNAL_GAUGE_BG.png` | **64×16** | 源是 48×16：宽 48 不是 2 的幂 ⇒ 补到 64；**内容靠左**（x 0..47）、右边 16 px 透明 |
  | `SIGNAL_GAUGE_BAR.png` | **64×16** | 同上（源 = sprite58）|
  | `SIGNAL_GAUGE_ICON.png` | **16×16** | 源矩形本身就是 16×16 ⇒ 原样切 |

  ⚠️ **靠左补边而不是居中补边**：这张图要按信号**横向缩放**（左端钉住）⇒ 居中的话scale 一变
  内容就会左右漂（`(64−48)/2 = 8 px × (scale−1)`）。靠左 ⇒ 内容左缘永远 = VM pos ✓
  （与 `make_trance_art.py` 的居中补边**故意不同**，那边不缩放）。

■ 生成器自身的纪律：只用不带 mask 的 `crop` / `paste`，不缩放、不重采样；幂等（无随机）。
"""
import sys
from pathlib import Path

from PIL import Image

HERE = Path(__file__).resolve().parent
REPO = HERE.parents[4]
SRC = REPO / "local/th11.v1.00a/anm/front/front/front00.png"
OUT = HERE / "signal"

# 一手：TH11 `front.anm` entry0（front/front00.png，512×512）的 sprite 矩形
SHEET = (512, 512)
SP_GAUGE_BG = (416, 16, 48, 16)       # sprite57 —— 槽底（script32）
SP_GAUGE_BAR = (416, 0, 48, 16)       # sprite58 —— 增长条（script33）；sprite59 同矩形（script34）
SP_GAUGE_ICON = (256, 80, 16, 16)     # sprite26 —— 静态图标（script35）
CANVAS_W = 64                         # 48 → 64（2 的幂），内容靠左


def _load():
    if not SRC.is_file():
        raise SystemExit(
            f"缺零售贴图 {SRC}\n（TH11 的资产是本次手工解的："
            "`thdat -x 11 th11.dat front.anm -C dat` + `thanm -l/-x 11 dat/front.anm`）"
        )
    im = Image.open(SRC).convert("RGBA")
    if im.size != SHEET:
        raise SystemExit(f"{SRC} 是 {im.size}，预期 {SHEET}")
    return im


def _crop(src, box):
    x, y, w, h = box
    return src.crop((x, y, x + w, y + h)).copy()


def _wide(src, box, out_name):
    """48×16 的源 → 64×16 画布，内容靠左（见文件头的「靠左补边」）。"""
    tile = _crop(src, box)
    canvas = Image.new("RGBA", (CANVAS_W, box[3]), (0, 0, 0, 0))
    canvas.paste(tile, (0, 0))            # ★ 不带 mask
    canvas.save(OUT / out_name)


def _dump(src):
    def art(box, name):
        x, y, w, h = box
        print(f"\n--- {name} @{box} ---")
        for j in range(h):
            row = ""
            for i in range(w):
                r, g, b, a = src.getpixel((x + i, y + j))
                if a < 40:
                    row += "."
                elif r > 200 and g > 200 and b > 200:
                    row += "W"
                elif r < 80 and g < 80 and b < 80:
                    row += "k"
                else:
                    row += "?"
            print("  " + row)

    art(SP_GAUGE_BG, "sprite57 槽底")
    art(SP_GAUGE_BAR, "sprite58 增长条（= sprite59）")
    art(SP_GAUGE_ICON, "sprite26 图标")


def main():
    dump = "--dump" in sys.argv
    src = _load()
    OUT.mkdir(exist_ok=True)

    _wide(src, SP_GAUGE_BG, "SIGNAL_GAUGE_BG.png")
    _wide(src, SP_GAUGE_BAR, "SIGNAL_GAUGE_BAR.png")
    _crop(src, SP_GAUGE_ICON).save(OUT / "SIGNAL_GAUGE_ICON.png")

    print("signal-hud: SIGNAL_GAUGE_BG.png 64x16（内容靠左 48x16）"
          " = TH11 front.anm entry0 sprite57 (416,16,48,16)")
    print("signal-hud: SIGNAL_GAUGE_BAR.png 64x16（内容靠左 48x16）"
          " = sprite58 (416,0,48,16)（sprite59 同矩形 ⇒ 不再出一份）")
    print("signal-hud: SIGNAL_GAUGE_ICON.png 16x16 = sprite26 (256,80,16,16)")
    print("\n把下面几行追加到 entries/ORDER.txt（或合并 entries/ORDER.signal.txt）：")
    print("SIGNAL_GAUGE_BG    ability/signal/SIGNAL_GAUGE_BG.png    64x16")
    print("SIGNAL_GAUGE_BAR   ability/signal/SIGNAL_GAUGE_BAR.png   64x16")
    print("SIGNAL_GAUGE_ICON  ability/signal/SIGNAL_GAUGE_ICON.png  16x16")

    if dump:
        _dump(src)


if __name__ == "__main__":
    main()
