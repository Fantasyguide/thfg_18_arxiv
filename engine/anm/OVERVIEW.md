# anm — ANM 精灵动画 VM

> **版本**：跨版本。本文出现的地址一律带版本前缀（如 `th18:0x405bf0`）。

⚠️ **本子系统尚未系统开工。**

东方里几乎所有「显示」都挂在 `.anm` 上：创建一个显示对象 = 选一段 anm 脚本/模板，
由 anm VM 每帧驱动它的精灵、变换、特效。它因此是**图形侧的总接缝**——
MSG 的文本绘制走它（[`../msg/OVERVIEW.md`](../msg/OVERVIEW.md)）、弹幕的外观也走它。

## 断言 × 版本矩阵

| 断言 | th16 | th18 | 证据 |
| --- | :---: | :---: | --- |
| 存在 anm 脚本 VM，每帧驱动显示对象 | 🟡 | 🟡 | [th16/README](th16/README.md)（做弹幕时顺带掀开的口子，多为 call-site/二手） |
| `AnmLoaded__instantiate_vm_to_world_list_back(anm; out_id, script, layer, out_vm)` thiscall ret 0x10，实体坐标 (0,0,0) | — | ✅ | [th18/01-vm-instantiate](th18/01-vm-instantiate.md) |
| `AnmLoaded__set_sprite(anm; vm, idx)` thiscall ret 8；按 id 删 `th18:0x488cf0` stdcall ret 4 | — | ✅ | 同上 |
| 脚本表在 `anm+0x10c`，模板步长 0x60c（th16 0x5fc） | 🟡 | ✅ | 同上 §2 |
| 渲染阶段 × 相机 × 层：层 0–2 相机 2、3–19 相机 3、20–23 相机 1、24+ 相机 2；四台相机全透视（LookAt + PerspectiveFov，无 Ortho） | — | ✅ | [th18/02-render-stages](th18/02-render-stages.md) |
| 渲染模式 8 = D3D WORLD 矩阵 + 单位四边形；2D 路径自算顶点并加相机 2D 偏移（只有相机 2 非零） | — | ✅ | [th18/01 §3b](th18/01-vm-instantiate.md) |
| `blendMode(n)` = VM `+0x18` bit 5..8 → D3D9 SRCBLEND/DESTBLEND/BLENDOP；**2 = 反向减算** `dst − src×alpha` | ✅ | 🟡 | [th15/01-blend-modes](th15/01-blend-modes.md) |
| ASCII 文字：字形集 = **页（`vm+0x20` → `preloaded[页]`）+ 字体 id（`+0x19244` → 精灵表字形块）** 两段 | ✅ | ✅ | [th18/03-ascii-text](th18/03-ascii-text.md) |
| 字符串记录步长 0x138，`create_string` 抄 10 个环境字段（字体在 `+0x120`），**页不抄** | ✅ | ✅ | 同上 §1 |

> **图例**：✅ 该版本一手验过（证据列给地址/出处） · 🟡 待验（从别的版本借来的假设，或单源） · ❌ 已知不同/不存在 · ❓ 存疑 · — 未看
>
> **本页不许出现没有出处的断言。** 从某作借到另一作的判断一律 🟡，在该版本 exe 上验过才能改 ✅（[`METHOD.md`](../../METHOD.md)）。

## 工具与样本（2026-09-04 就位）

- 标准 thtk（thanm / thdat，release 12）本机编好：[`tooling/thtk/`](../../tooling/thtk/README.md)；`unpack.py` 把 th18 的
  56 个 anm 全解成「一目录 = spec + 贴图」（`local/th18.v1.00a/anm/`，不入库）。`-l` → `-c` 往返字节一致。
- anmmap：thpages `v8.anmm`（th18 在用，150 条指令名）——这是 opcode 语义研究时的社区参照，运行时实现仍须在 exe 上自证。
- 第一个产物线：card-expand 的卡图追加（`mods/th18.v1.00a/card-expand/assets/`）。它只动格式层，没有引擎结论；
  引擎侧待答的第一问是 **运行时 sprite 数上限**（`AnmManager__preload_anm` → `AnmLoaded`）。

## 开工前必读

[th16/README.md](th16/README.md) 里的「起步锚点」全部是做 [`../bullet/`](../bullet/OVERVIEW.md)
时**顺带掀开的口子**，多为 🟡（call-site / 二手）。**动手时务必自己反编译复核，别当定论。**

ExpHP `th-re-data` 对 `zAnmVm` / `AnmManager` 已有大量命名（TH16 AnmVm 45 / AnmManager 38），
起步先套（[`../../tooling/ghidra/bootstrap.py`](../../tooling/ghidra/bootstrap.py)）。
