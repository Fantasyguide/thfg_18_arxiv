#!/usr/bin/env python3
"""风神录（id 110）· 地灵殿（id 111）共用的**绿点信仰实体**贴图 —— 从 TH10 原版图切。

    python3 make_mof_art.py

=====================================================================================
★ 2026-09-12 订正块（做 110 的 HUD 原生切图时顺手查清的；**只改注释与认知，产物一个字节没动**）
=====================================================================================

本文件下面「脚本原文」一节把 TH10 的 raw opcode 猜成了 `ins_63 = delete` / `ins_66/ins_67 没对上`
/ `ins_53 = rotate` —— **三条都不对**。理由：TH10 `front.anm`/`bullet.anm` 的 entry 是
`version: 4`，而 `local/vendor/thpages/static/mapfile/v4.anmm` 的抬头**自己写着**
「for version 4: StB, Mof / and version 7: SA, UFO, DS, GFW」—— 也就是说 **TH10 有官方助记符表**，
本文件当初是**不带 `-m` 直接 dump**（所以才看到 `ins_N`）。带上它重读即可对出真名：

    local/vendor/thtk/build/thanm/thanm -l 10 local/th10.v1.00a/dat/bullet.anm \
        -m local/vendor/thpages/static/mapfile/v4.anmm

订正后的对应（v4 号 → 助记符）：

| v4 opcode | 真名 | 本文件原来的说法 |
| --- | --- | --- |
| 51 | `alpha` | ✅ 对 |
| 50 | `scale` | ✅ 对 |
| 60 | `scaleTime` | ✅ 对 |
| **53** | **`angleVel`**（角速度）| ❌ 原写 `rotate`；`rotate` 是 49 —— 所以 faith dot 那条
  脚本的真实语义是「**ωz = π/4 /帧 的自转**」，不是「定角 45°」 |
| **63** | **`stop`** | ❌ 原写 `delete`（`delete` 是 1）—— 且原文末尾那条 `ins_63()` 在 `+32:` 段，
  是「停止脚本」而不是「删除 VM」 |
| **66 / 67** | **`blendMode(1)` / `type(1)`** | ❌ 原写「没对上、不搬」—— 两条都有名字，可搬 |
| 89 | `resampleMode` | ✅（本文件没用它）|
| 85 / 86 | mapfile 里**无名**（`# 85: unknown flag`）| 本文件与 `209/210` 都**没搬**（v4→v8 号不通用）|

⇒ **`assets/ability/scripts/185_faith_dot.anm.txt` 的注释与内容需要一次订正**（不在本轮的改动
范围内，作者/主 agent 处理）：注释里的「ins_63 = delete」「ins_53 = rotate」「ins_66/67 没对上」
换成上表，并把丢掉的 `blendMode(1)`、`type(1)` 补回去（那两条影响出生动画的混合模式与类型）。
本轮新写的 `209_mof_faith_icon.anm.txt` / `210_mof_faith_digit.anm.txt` 已经是**带 v4 mapfile
逐条读出来的原文**（见那两个文件头的「一手原文」段）。

======================================================================================
一手考证：TH10 的信仰点（绿点）贴图在哪
======================================================================================

■ 结论（✅ 定位链一手；🟡 只有「哪一格是绿点」这一步见 §候选）
  **`local/th10.v1.00a/anm/bullet/bullet/etama2.png`**（256×256，TH10 `bullet.anm` 的
  **entry2**，`format: 5` = RGBA）里，y=208 那一行是 **16 个 16×16 的「道具球」单元**，
  每个单元自带两帧：上半 10 行 = 盘/箭头主体，下半 4 行 = 内侧小星。
  绿色（信仰点）的那一格 = **`sprite351`，矩形 `(112, 208, 16, 16)`**；
  同像素还有一份别名 **`sprite359` `(240, 208, 16, 16)`**（🟡 见候选）。
  产物**不缩放**：16×16 本身就是 2 的幂，`build_ability.py` 收得下（小于声明尺寸会自动居中补边）。

■ 怎么找到的（可复核，别信我的话，跟着跑）

  1. 线索排除：`front.anm` 只有 1 个 16×16（`front00.png` sprite29 @(256,80)）—— 那是
     一个图标，不是道具；TH10 的道具贴图在 **`bullet.anm`**（4 张 etama 图）。
  2. **`ItemManager__spawn_item` `0x41bb00`**（th-re-data `local/vendor/th-re-data/data/th10.v1.00a/funcs.json`）
     里 **类型 → anm 脚本号** 的表就在这里（一手汇编，见下），**不是**在 `0x41beb0`：
     `0x41beb0` 是极坐标 → 速度的 `FSINCOS` 小函数（入参 `ecx` = 输出 float[2]、`[esp+8]` = 角度、
     `[esp+0xc]` = 速度，把 `cos*speed`/`sin*speed` 写进 `[0]`/`[4]`）—— **任务里说的
     「0x41beb0 是道具初始化」是误判**，附此更正。

        0x41bc47:  cmp   ebp, 0xa          ; ebp = 道具类型
        0x41bc4a:  mov   [esi+0x3e0], ebp   ; 存类型
        0x41bc56:  jne   0x41bc8c
        0x41bc6f:  mov   eax, 0x177         ; 类型 0xA → 脚本 375
        0x41bca1:  mov   eax, 0x17a         ; 类型 0xB → 脚本 378
        0x41bca8:  lea   eax, [ebp+0x176]   ; 其余    → 脚本 类型 + 374
        0x41bcb5:  call  0x404f30          ; AnmVm__run_script(VM@esi+0x3b8, 脚本号 eax, anm_id)

     ⚠️ **与任务给的地址有偏差，说明如下**（`0x41b8e0` 确实不在指令边界上 —— 那是**数据**：
     它是 `bullet.anm` 的 `id`/`nameoffset` 表，th-re-data 的地址在这一段可信但
     `on_draw__body` 的**入口**要从 `0x41b8e0` 前一条完整指令起读；我用 Capstone 从
     `0x41b8e0` 开始线性扫描得到的是 `push ebx; push ebp; push esi; push edi; mov edi,eax` ——
     即 **`0x41b8e0` 就是 `on_draw__body` 的入口**，能对上指令流，th-re-data 没错）。
     定位方法：`python3 -c "import capstone,…"` 线性扫 `.text`（**别用 `local/_scratch/th10_full.asm`**，
     那份平坦反汇编在 `0x41bc09`–`0x41bc10` 的 **byte 跳转表**处跑飞了，把数据当代码印，
     同一段在 `0x41bd00` 附近更是整个错位 —— 这就是「以能对上指令流的那份为准」的意思）。

  3. 拿脚本号去 `thanm -l 10` 的清单里读正文（下面「脚本原文」一节），**脚本正文里的
     `stop(spriteN)` 就是本地 sprite 号**，再用 `bullet.anm.txt` 的 sprite 矩形表换算成像素矩形。

  ⚠️ **TH10 的 mnemonic 与 TH13+ 不同**：`thanm -l 10` 打的是 `ins_50/ins_60/ins_53/...`
  （版本 10 没有 anmmap 别名），上面那份清单里是宏名。按**同一条指令在 TH13 `astral.anm` 里的
  已知语义**（`scripts/174..` 那批就是逐条搬的，见 `make_trance_art.py` 文件头）对应：
  `ins_51` = `alpha`、`ins_50` = `scale`、`ins_60` = `scaleTime`、`ins_53` = `rotate`、
  `ins_63` = `delete`、`ins_67`/`ins_66`/`ins_82`（🟡）= 混合/开关类，本次**不搬**（见报告）。

■ 候选：为什么是 `(112,208)` 这一格 🟡

  这一节**没有排他的一手证据**，把推理链和反证都摆出来：

  - `etama2.png` y=208 那一行 16 格，按平均色分是（一手像素统计，见 `--dump`）：
    `(0,R) (16,B) (32,G) (48,R) (64,B灰) (80,Y) (96,M) (112,G) (128,R) (144,B) (160,G) (176,R) (192,B灰) (208,Y) (224,M) (240,G)`
    ⇒ **绿色只有 3 格：x=32 / x=112 / x=160，其中 x=240 与 x=112 逐像素相同**（同图别名）。
  - 16×16 单元里 x=0..15 是 **8 格一组的「一组颜色」**（红/蓝/绿/紫 ×2），
    x=112 起是**第二组**（绿/红/蓝/绿…），`(112,208)` 与 `(240,208)` 是**同一张贴图数据的两个
    sprite 编号**（ZUN 用同一块 UV 给两个 sprite 号是常规手法，`thanm` 也照录）。
  - 30×30 那一族（`sprite336..343`，y=145/177）是**大件**（道中 Boss 掉的 P / 点道具本体），
    不是 8×8 的绿点；绿点在本作里是**小单元**，所以取 16×16 那一族。
  - ❓ **没有做到**：我没能把「道具类型 8（= 信仰）」钉到某一格。卡住的两步：
    ① `ItemManager::spawn_item` 里脚本号 = **类型 + 374**，而四种小件脚本（381..384）的
       `stop(spriteN)` 只差 `N`，**脚本号与 sprite 号正好错开 30**（381↔351、382↔352…），
       这个错位我没有找到一手解释（可能是 `sprite` 号在 TH10 里还有一层入口内偏移）；
    ② `0x41bbf7` 的 `cmp WORD PTR ds:0x474c48, 0x64`（火力满）之后那张 **byte 跳转表
       `0x41be28`** 会把类型 1..7 改写成 5/9，而 `0x474c48` 我读出来全 0
       （疑似 HMODULE/表基址，不是「当前火力」）⇒ 无法用它反推类型号。
    **以 `sprite` 号为准**取 `sprite351`（`make_spirit_art` 一族里唯一带 `ins_51(16)` 的那条
    就是 script382 的）；若卡侧实跑发现应当取 `sprite352`（x=128 那格，红色），
    只需把本文件里 `GREEN_MAIN` 改成 `(128, 208, 16, 16)` 重跑一次 —— **一行的事**。
    可信度：源图 / 矩形 / 行的位置 **✅ 一手**；「这一格就是信仰点」**🟡 推理**。

■ 脚本原文（一手，`thanm -l 10 local/th10.v1.00a/dat/bullet.anm`，脚本 382；宏名在注释里给出）

      script script382 {
          stop(sprite351);          // ← 本文件切的就是它（= 绿点，16×16 @(112,208)）
          ins_66(1);
          ins_67(1);
          ins_51(16);               // alpha(16)
          ins_53(0, 0, 0.7853982f); // rotate(0, 0, π/4)  ← **定角 45°**，不是动画
          ins_50(0.0f, 0.0f);       // scale(0, 0)      ← 从 0 起
          ins_60(16, 0, 1.0f, 1.0f);// scaleTime(16, 0, 1, 1)
      +32:
          ins_53(0, 0, 0.0f);       // rotate(0, 0, 0)  ← 32 帧后**瞬间**转正
          ins_67(0);
          ins_63();                 // delete()
      }

  ⇒ **8×8（16 的 scale 0.5 × 16px）的小方块，出场 16 帧从 0 放大到 1、
     入场时带 45° 定角、第 32 帧转正并立刻 delete**（所以它不是「一直在场」的道具，
     而是**出生一瞬的闪光单元**；TH10 里真正持续显示的是同一格的另一份 sprite 号）。
  ⚠️ 与本仓 `SPIRIT`/`season` 两处不同：**这里没有 `layer()` 也没有 `alpha` 的淡出**，
     照抄即可，不要替它加。
"""
import sys
from pathlib import Path

from PIL import Image

HERE = Path(__file__).resolve().parent
REPO = HERE.parents[4]
SRC = REPO / "local/th10.v1.00a/anm/bullet/bullet/etama2.png"
OUT = HERE / "mof"

# 一手：TH10 `bullet.anm` entry2（bullet/etama2.png）的 sprite 矩形
SHEET = (256, 256)
GREEN_MAIN = (112, 208, 16, 16)   # sprite351 —— script382 的 stop(sprite351)
GREEN_ALIAS = (240, 208, 16, 16)  # sprite359 —— 与上一格逐像素相同（同图别名）


def _load():
    if not SRC.is_file():
        raise SystemExit(
            f"缺原版贴图 {SRC}\n（先 `python3 tooling/thtk/unpack.py th10.v1.00a`）"
        )
    im = Image.open(SRC).convert("RGBA")
    if im.size != SHEET:
        raise SystemExit(f"{SRC} 是 {im.size}，预期 {SHEET}")
    return im


def _crop(src, box):
    """按一手矩形切下来 —— **不缩放、不重拼、不补边**。

    `build_ability.py` 允许源图**小于** ORDER 里声明的尺寸（会自动居中补透明；
    见 `build_ability.py:106-110`），而 16×16 本身就是 2 的幂 ⇒ 这里保持原生原像素最好：
    放大到 32×32 会让脚本里的 `scale()` 语义跟着变（TH10 那条脚本没有 scale，靠原始 16px）。
    ⚠️ 用**不带 mask** 的 paste：带 mask 时 PIL 会做 alpha 合成，把「alpha 小、RGB 有值」的
    边缘像素预乘成暗色（实测原生图 (255,255,255,68) → (68,68,68,18)），就不是原样切图了。
    """
    x, y, w, h = box
    return src.crop((x, y, x + w, y + h)).copy()


def main():
    dump = "--dump" in sys.argv
    src = _load()
    OUT.mkdir(exist_ok=True)

    # 只出一张 16×16：TH10 `bullet.anm` **script382 原文就只引用一格 sprite351**，
    # 多切一格当「动画」就是编出来的（本仓铁律不允许）⇒ sprite359（x=240，与 x=112 同像素）
    # 只作为 `--dump` 的像素统计留证，**不做成贴图**。
    _crop(src, GREEN_MAIN).save(OUT / "FAITH_DOT.png")

    print("mof: FAITH_DOT.png 16x16（原始像素，未缩放）"
          " = TH10 bullet.anm script382 的 stop(sprite351) @ bullet/etama2.png (112,208)")

    if dump:
        print("\n--- etama2.png y=208 一行 16 格的像素统计（核色用）---")
        for i in range(16):
            c = src.crop((i * 16, 208, i * 16 + 16, 224))
            px = [p for p in c.getdata() if p[3] > 40]
            if not px:
                print(f"  x={i * 16:3d} 空")
                continue
            n = len(px)
            avg = tuple(sum(p[k] for p in px) // n for k in range(3))
            print(f"  x={i * 16:3d} n={n:3d} avg={avg}")

    print("\n把下面一行追加到 entries/ORDER.txt（或合并 entries/ORDER.mof.txt）：")
    print("FAITH_DOT   ability/mof/FAITH_DOT.png  16x16")


if __name__ == "__main__":
    main()
