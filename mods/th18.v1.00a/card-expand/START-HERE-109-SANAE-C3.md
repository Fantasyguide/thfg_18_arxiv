# START HERE —— 109 卡 · 早苗 C3（曲线激光）开工入口

> **版本**：TH19 v1.10c（效果来源）＋ TH18 v1.00a（卡侧载体）。
> **分支**：`feat/p109-c34-refactor`，工作目录 `/home/nipporita/rl-109-sakuya`。

一个人接手时的**唯一入口**。按下面顺序读，前三份读完就能动手。

## 1. 读什么（按顺序）

| # | 文件 | 读完你会知道 |
| ---: | --- | --- |
| 1 | [`HANDOFF-109-SANAE-C3-KINEMATICS.md`](HANDOFF-109-SANAE-C3-KINEMATICS.md) | ★ **运动学定稿**：五段 `a`/`b`/`r`/`s`、速度曲线、卡侧配方、作废清单、待拍板的 4 件事 |
| 2 | [`../../engine/bullet/th19/01-curve-laser-kinematics.md`](../../engine/bullet/th19/01-curve-laser-kinematics.md) | ★ 同上，engine 层版本（带一手证据与置信度标注） |
| 3 | [`../../engine/bullet/th18/01-laser-subsystem.md`](../../engine/bullet/th18/01-laser-subsystem.md) **§26** | ★ TH19 `609` 的 8 个参数在内存里各落哪里（段表 `+0x00`=r、`+0x04`=s） |
| 4 | [`../../mods/th19.v1.10c/runtime-probe/README.md`](../../mods/th19.v1.10c/runtime-probe/README.md) | ★ 运行时探针怎么用（只读，装进 modkit 的 `mods\` 即可） |

## 2. 三句话结论（先记住这个）

1. ★ `609` 的 **`r` = 航向速度增量/帧**（px/帧²），**`s` = 自转角速度**（rad/帧）；
2. ★ **转向是 `s`，不是 `r`**：段 4 用 `∓11.25°/帧 × 8 帧 = ∓90°`；段 3 的 `r = −0.4` 只是**减速到停**；
3. ★ 旧的 `1.15 px/帧`、`k=0.34` **全部作废**，不要再用。

## 3. 不许做 / 必须做

* ★ **不许**改 TH18 的 `native/`、`Makefile`、脚本资产 —— 除非 owner 明确发话；
* ★ **必须**先读 §1 的第 1、2 份再动手；
* ★ 每条结论带地址/偏移/置信度；🟡/❓ 不许写成一手；
* ★ 提交前跑 `python3 tooling/check-docs.py`（要 EXIT=0）。

## 4. 现状

★ **运动学已闭合**；★ 卡侧**一行代码都没动**（等 owner 拍板 §6 的四件事）。
