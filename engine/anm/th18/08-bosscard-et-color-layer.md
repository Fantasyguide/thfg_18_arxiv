# 08 — TH18 符卡「红/蓝晕光」= `et` 发射器 + anm 精灵（含 **blendMode / layer** 定案）

> **版本**：TH18 v1.00a（`th18.exe`，imagebase `0x400000`）。裸地址默认属该版本。
> **状态**：✅ 定案（2026-10-22）。一手链路：exe 反汇编 + `th18.dat` 解包（thtk）+ ECL 反编译（thecl）+ anm 反编译（thanm）。
> **与谁有关**：卡 109 C4「让自机看起来像 boss」的**颜色层**（扭曲层是另一件事，见 07-boss-aura-mesh）。

## 0. 三句话结论

1. **那层红/蓝不是 aura mesh，也不是一张"光环贴图"** —— 它是**符卡 ECL 里的 `et` 发射器子程序**
   （`BossCard6_et` / `BossCard6_et2`，在 **`st07bs.ecl`** 里有定义），发射器内部是 **16 台 anm VM** 的池，
   每台跑 `effect.anm` 里的一个 **sprite 脚本**。
2. **混合模式**：主要那台 sprite 脚本写 **`blendMode(4)`**（anm opcode **303**）⇒ RGB 走
   `SRCBLEND=INVDESTCOLOR(10)`、`DESTBLEND=INVSRCCOLOR(4)`、`BLENDOP=ADD(1)`（"反色/差值"混合）；
   **alpha 恒为 `Aa' = Sa`**（TH18 分离 alpha 混合，与 blendMode 无关 —— 见 [07](07-blendmode-rgb-only-alpha-is-sa.md)）。
3. **layer**：主要 sprite 写 **`layer(21)`**（anm opcode **304**）；另一个 sprite 写 **`layer(5)`**。

## 1. 一手链路（可复现）

```
thdat -x 18 th18.dat st07bs.ecl     # 取出第 7 关 boss 的 ECL
thecl -d 18 st07bs.ecl              # 反编译 ⇒ 找到 void BossCard6_et() / BossCard6_et2()
thdat -x 18 th18.dat effect.anm
thanm -l 18 effect.anm              # 反编译 ⇒ script33 / script18 …
```

**exe 侧**（谁在什么时候调这个子程序）：

| 一手 | 说明 |
| --- | --- |
| `0x4b651c` = `"BossCard6_et"`、`0x4b652c` = `"BossCard6_et2"` | 全 exe 只有这两个名字（`BossCard` 前缀）✓ |
| `0x439153` `push 0x4b651c` → `0x439168 call 0x42d7d0` | 按名字**创建/调用**；参数结构在 `[esp+0x4c]`（含 `0x2710`=10000 等） |
| `0x42d7d0` → `push 0x683c` / `call 0x48dc71`（分配）/ `call 0x42e100(obj,name)` | 建一个 **0x683c 字节**对象 |
| `0x42e1a0`–`0x42e1c3`：`ebx=0x10`，每次 `memset(edi,0,0x488)`，`edi += 0x488` | ⇒ 内部是 **16 台 anm VM（16×0x488）** |
| `0x48d920(this, name)` | 名字→脚本的**二分查找**（表在 `mgr+0x188`，`+8`=个数，`+0x8c`=指针数组，元素步长 8，逐字节比较） |

## 2. 画法（sprite / 几何）

`st07bs.ecl` 的 `BossCard6_et()`（opcode 名按 [`th16/06-settings`](../th16/06-settings.md) 的 anm 表）：

```
ins_502(303);                                   // ❓ 未定名（303 也是 blendMode 的 opcode 号，需注意别混）
ins_600(1);  ins_607(1,3);  ins_602(1,33,6);    // etNew / et* setter：槽 1，sprite=33，count=6（🟡 参数位序）
ins_606(1,1,1);  ins_604(1,0.0f,0.5235988f);    // 🟡 角度/相位类
ins_605(1,3.2f,1.0f);
ins_611(1,0,1048576,1,-9999994,…);              // 🟡 参数/插值
ins_611(1,0,4194304,10,1,1.0f,3.0f);            //   （10 帧、1.0→3.0）
ins_611(1,0,4194304,60,1,3.0f,0.3f);            //   （60 帧、3.0→0.3 = 扩散后收缩）
ins_611(1,0,1024,0,-9999994,…);
$A=8; loop { ins_604(1,[-9998],0); ins_605(1,([-9999]*0.5)+0.4,0); ins_601(1); }   // 8 帧渐显
… 再来一组 sprite=18/count=6（layer 见下）、sprite=1/count=2 …
```

对应的 **sprite 脚本**（`effect.anm`）：

| 脚本 | 关键指令 | 含义 |
| --- | --- | --- |
| **`script33`** | `ins_300(-1)` | ★ **不绑 sprite（不画 PNG）**；形状由 `ins_604(1.0f, 50)` = **`drawPoly(1, 50)`** 程序生成 ⇒ **实心圆**（opcode 名见 [`th16/09-drawing`](../th16/09-drawing.md)：604/605/611 = drawPoly/Border/Ring） |
| | **`ins_303(4)`** | ★ **`blendMode(4)`** |
| | **`ins_304(21)`** | ★ **`layer(21)`** |
| | `ins_404/405/406(…)` | 颜色/alpha（0、255、255,255,255 一类） |
| | `ins_412(50,2,600.0f,600.0f)` + `+30: ins_407(20,2,0,0,0)` | 50 帧的尺寸/插值 |
| **`script18`** | `ins_300(sprite6)` | 普通 sprite 6 |
| | **`ins_304(5)`** | ★ **`layer(5)`**（在 21 之下） |
| | `ins_402(16,192)` / `ins_400(128,384,0)` / `ins_415(0,0,0.02617994)` | 缩放 16→192、位置 (128,384)、每帧转 0.026 rad |

⇒ **几何就是"若干张带颜色的四边形 sprite"**（每台 VM 一张），没有专门的 shader/后处理 ✓

## 3. 混合模式（RGB 三条状态 + alpha）

`blendMode(4)` 的 RGB（表沿用 TH15 的实测表，TH18 同一函数族 ⇒ 🟡）：

| blendMode | SRCBLEND | DESTBLEND | BLENDOP | 结果 |
| :-: | --- | --- | --- | --- |
| **4** | `INVDESTCOLOR(10)` | `INVSRCCOLOR(4)` | `ADD(1)` | **反色/差值类**（颜色随底色与 sprite 底色一起决定 ⇒ 观感是"染上一层蓝/红"） |
| （对照）0 | `SRCALPHA(5)` | `INVSRCALPHA(6)` | `ADD(1)` | 普通 alpha |
| （对照）1 | `SRCALPHA(5)` | `ONE(2)` | `ADD(1)` | 加算（发光） |

**alpha**：TH18 的 anm 绘制**恒开分离 alpha 混合**（`0xce←1`、`0xcf←ONE`、`0xd0←ZERO`、`0xd1←ADD`）
⇒ **`Aa' = Sa`**（源 alpha 直接覆盖，和 Da、和 blendMode 都无关）—— 见 [07](07-blendmode-rgb-only-alpha-is-sa.md) ✅

## 4. layer 的语义

anm opcode **304 `layer(n)`** = `set_layer(vm, n & 0xff)`（`th16/06-settings` 一手，case `0x462950`）
⇒ 之后由 anm 的层系统按层号画（见 [02-render-stages](02-render-stages.md) 的优先级表）。
**本例：主层 = 21，副层 = 5。**

## 5. 可信度与遗留

| 项 | 可信度 |
| --- | --- |
| ECL 子程序名/内容、`BossCard6_et` 定义在 `st07bs.ecl` | ✅（thecl 反编译 + 原始字节里找得到字符串） |
| sprite 脚本 → `blendMode(4)` / `layer(21)` / `layer(5)` | ✅（thanm 反编译，逐条） |
| `0x42d7d0`/`0x42e100` 的实例结构与 16 台 VM 池 | ✅（反汇编） |
| `blendMode 4` 的 D3D 常量映射 | 🟡（沿用 TH15 实测表；未在 TH18 上逐 case 复核） |
| ECL `et*` 各 setter 的**参数位序**（`ins_602/604/605/606/607/611`） | 🟡（按 TH09 的九参族类推；未逐条在 TH18 上验证） |
| `ins_502(303)` 的语义 | ❓ |
| `sprite(-1)` = **不绑 sprite**；形状由 **`drawPoly(1.0f, 50)`** 程序生成 = **50 边形实心圆** | ✅（`script33` 里的 `ins_604(1.0f, 50)` + [`th16/09-drawing`](../th16/09-drawing.md) opcode 604；同族佐证 `engine/player/th16/02-season-release-and-bombs.md` `sprite(-1)+drawPoly(1.0f,32)`=实心圆、`engine/card/th18/13-mallet-clear-and-th17-charge.md` `drawPoly` 不消耗 sprite） |


## 6. ❓ 唯一还缺的一手：`sprite(-1)` 时**程序怎么画那个形**

owner 口径：`sprite(-1)` = 本次绘制**不画 PNG**，形状由程序画（圆/长方形等）。
⇒ 若要"复刻那层的观感"，必须读**程序画形的代码**：它决定了圆的**细分方式、半径映射、边缘 alpha 曲线**。
这些在 `blendMode(4)`（`INVDESTCOLOR × INVSRCCOLOR + ADD`）下会直接影响观感 —— 用烤好的径向渐变去近似，
差别就出在这里（**不是**混合模式错，是**形状/边缘**不同）。


## 7. ✅ 定案补充（2026-10-22，来自 `engine/` 既有成果，不是新反汇编）

`effect.anm` 的 **`script33`** 全文（`thanm -l 18 effect.anm`）：

```c
ins_300(-1);                    // sprite(-1)      —— 不绑 sprite（不画 PNG 贴图）
ins_304(21);                    // layer(21)       —— 层 21
ins_604(1.0f, 50);              // drawPoly(1, 50) —— ★ 半径 1 的 50 边形 = 实心圆
ins_303(4);                     // blendMode(4)    —— 反色混合
ins_403(0); ins_405(255); ins_404(255,255,255); ins_406(255,255,255);   // color/alpha
ins_412(50, 2, 600.0f, 600.0f); // 50 帧的尺寸插值
+30: ins_407(20, 2, 0.0f, 0.0f, 0.0f);
ins_3();
```

⇒ **那层"圆"是程序画的 50 边形**（不是贴图、不是烤图）✓
⇒ 要复刻：`sprite(-1); layer(21); drawPoly(1.0f, 50); blendMode(4); color(...); alpha(...)`，
再用 `scale`/`alpha` 做时间序列 ✓ —— **烤好的径向渐变在 `blendMode(4)` 下必然观感不同**（形状与边缘都不一样）✗
