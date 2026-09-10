# th15 anm 的 blendMode 表（D3D9 混合状态）

> **版本**：TH15 v1.00b（`th15.exe`，imagebase `0x400000`）。本文裸地址默认属该版本；引用其他版本须写成 `th18:0x…`。
> **方法**：反编译 `AnmManager__setup_render_state_for_vm` `0x47d840`（一手），逐 case 读 `SetRenderState` 参数。
> **可信度**：✅ 全部一手。

## 0. 一句话

anm 的 `blendMode(n)` = VM 标志位 `+0x18` 的 bit 5..8（4 位）。引擎把它翻成三条 D3D9 render state：
`D3DRS_SRCBLEND(0x13)`、`D3DRS_DESTBLEND(0x14)`、`D3DRS_BLENDOP(0xab)`（`0x47d840` 末尾统一写 BLENDOP）。

| `blendMode` | SRCBLEND | DESTBLEND | BLENDOP | 结果 |
| --- | --- | --- | --- | --- |
| 0 | SRCALPHA(5) | INVSRCALPHA(6) | ADD(1) | 普通 alpha 混合 |
| 1 | SRCALPHA(5) | ONE(2) | ADD(1) | **加算**（发光） |
| **2** | SRCALPHA(5) | ONE(2) | **REVSUBTRACT(3)** | **反向减算**：`dst − src×alpha`（压暗） |
| 3 | ONE(2) | ZERO(1) | ADD(1) | 不透明覆盖 |
| 4 | INVDESTCOLOR(10) | INVSRCCOLOR(4) | ADD(1) | 反色混合 |
| 5 | DESTCOLOR(9) | ZERO(1) | ADD(1) | 乘算 |
| 6 | INVSRCCOLOR(4) | INVSRCALPHA(6) | ADD(1) | — |
| 7 | DESTALPHA(7) | INVDESTALPHA(8) | ADD(1) | — |
| 8 | SRCALPHA(5) | ONE(2) | MIN(4) | 取暗 |
| 9 | SRCALPHA(5) | ONE(2) | MAX(5) | 取亮 |

D3DBLEND / D3DBLENDOP 的数值取自 d3d9.h（1=ZERO、2=ONE、3=SRCCOLOR、4=INVSRCCOLOR、5=SRCALPHA、
6=INVSRCALPHA、7=DESTALPHA、8=INVDESTALPHA、9=DESTCOLOR、10=INVDESTCOLOR；OP 1=ADD、2=SUBTRACT、
3=REVSUBTRACT、4=MIN、5=MAX）。

## 1. 用得上的一条

TH15 的「擦弹/减速圈」（`effect.anm` script27，`eff_sloweffect.png` 右半）用的是 **`blendMode(2)`**：
它不发光，而是把**背后的画面压暗**——所以那个圈是**暗环**，不是亮环。
TH18 侧同一张表大概率一样（同一个函数族），但**未在 th18 上逐 case 复核** 🟡。

## 2. 证据

`th15:0x47d840`：`switch ((vm+0x18 >> 5) & 0xf)` → 每个 case 两次 `device->SetRenderState(0x13/0x14, …)`，
最后统一 `SetRenderState(0xab, uVar3)`（`uVar3` 就是 BLENDOP）。
