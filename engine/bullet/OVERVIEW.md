# bullet — 弹幕引擎
> **版本**：跨版本。本文出现的地址一律带版本前缀（如 `th16:0x442560`）。


TH16 的敌弹**不是**由一个中央弹幕系统统一驱动的。每颗弹自己带一小段**字节码 VM 上下文**，
运动由 VM 逐帧解释；VM 之外还有一层按位场派发的 **behavior handler**，两者在同一个弹对象里交织。
换句话说：**弹对象既是数据也是执行上下文**。这个设计决定了「改弹幕行为」的接缝在哪——
不在某个 if，而在 opcode 表和 handler 表。

激光是另一个类族（有独立 RTTI 与 vtable），显示、判定、Bomb 取消三件事分开走。

## TH06NC 入口

`g_BulletManager` `th06nc:0x1403ec2a0`：`bullets[640]`（步长 `0x620`，`+0x08`）与
`lasers[64]`（步长 `0x298`，`+0xf5018`）两个池，逐帧 tick 在
`BulletManager__OnUpdate` `th06nc:0x140010870`。

| 断言 | TH06 | TH06NC | 出处 |
| --- | --- | --- | --- |
| `sizeof(Bullet)` | `0x5c4` | **`0x620`** | [th06nc/01 §1.3](th06nc/01-pools-and-structs.md) |
| `sizeof(Laser)` | `0x270` | **`0x298`** | 同上 |
| 池容量（640 / 64） | — | **都没变** | 同上 |
| `BulletManager` 开头 | `bulletTypeTemplates[16]` | **`ZunTimer` 后直接是 `bullets`** | 同上 |
| 弹出界后不立刻回收（`offscreenFrames > 0xff`） | — | ✅ 一手 | [th06nc/01 §2](th06nc/01-pools-and-structs.md) |
| 擦弹逻辑内联在弹幕 tick 里 | ❌（独立函数） | ✅ | [player/th06nc/01 §2.8](../player/th06nc/01-main-loop-and-collision.md) |

> 结构体两边都重排过，**偏移一律要在各自 exe 上重取**；只有「有哪些字段」可以借。

## 断言 × 版本矩阵

| 断言 | th16 | th18 | 证据 |
| --- | :---: | :---: | --- |
| 弹运动由**弹对象体内**的字节码 VM 驱动（opcode 表 `FUN_00413860`） | ✅ | 🟡 | [th16/01 §3](th16/01-core-engine.md)、[th16/02 §2](th16/02-bullet-vm-model.md) |
| VM 上下文就是弹对象自身（无独立 VM 堆） | ✅ | 🟡 | [th16/02 §4](th16/02-bullet-vm-model.md) |
| behavior handler 按 `obj+0xc68` 位场每帧派发，与 VM 并行 | ✅ | 🟡 | [th16/01 §4](th16/01-core-engine.md)、[th16/02 §3](th16/02-bullet-vm-model.md) |
| 激光是独立类族（RTTI 名 + 专属 vtable），非普通弹 | ✅ | 🟡 | [th16/03 §1](th16/03-lasers.md) |
| 激光对自机的碰撞 = **旋转 OBB**（`th16:0x443af0`），不是线段距离 | ✅ | 🟡 | [th16/03 §4](th16/03-lasers.md) |
| 激光有两条创建路径 | ✅ | 🟡 | [th16/03 §3a](th16/03-lasers.md) |

> **图例**：✅ 该版本一手验过（证据列给地址/出处） · 🟡 待验（从别的版本借来的假设，或单源） · ❌ 已知不同/不存在 · ❓ 存疑 · — 未看
>
> **本页不许出现没有出处的断言。** 从某作借到另一作的判断一律 🟡，在该版本 exe 上验过才能改 ✅（[`METHOD.md`](../../METHOD.md)）。

## 开放

- handler 表的部分槽位仍是 🟡（agent 枚举，主控只验了碰撞槽）。
- 弹的 provenance（谁创建、从哪个池来）是双源互证，未逐条一手复核。
- 与 ECL 的接缝见 [`../ecl/OVERVIEW.md`](../ecl/OVERVIEW.md) 的「开火接缝」行。
