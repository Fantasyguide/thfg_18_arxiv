# menu — MainMenu 状态机
> **版本**：跨版本。本文出现的地址一律带版本前缀（如 `th16:0x442560`）。


标题画面/菜单是一个**显式状态机**：一个 tick 分派器按状态号 switch 到各 handler，
转移统一走一个 change 函数。它本身玩法价值不高，但**它是「玩家选了什么」流进游戏的入口**——
角色、副季节、难度都在这里写进全局，再由射击初始化消费。想改自机选择相关的东西，接缝在这儿。

## 断言 × 版本矩阵

| 断言 | th16 | th18 | 证据 |
| --- | :---: | :---: | --- |
| `MainMenu__on_tick` `th16:0x44af80` 按状态号 switch 分派 | ✅ | 🟡 | [th16/01 §1](th16/01-state-machine.md) |
| 状态转移统一走 `MainMenu__change_menu` `th16:0x44a560` | ✅ | 🟡 | [th16/01 §2](th16/01-state-machine.md) |
| 状态枚举 → handler 全表（来自 on_tick switch） | ✅ | 🟡 | [th16/01 §3](th16/01-state-machine.md) |
| 角色/副季节选择写入全局，由 `player_shot_init` `th16:0x440fb0` 消费 | ✅ | ❌ | [th16/03](th16/03-character-subseason-sht-chain.md)；th18 无副季节 |
| 符卡练习有独立数据表 | ✅ | 🟡 | [th16/02](th16/02-state-helpers.md) |

> **图例**：✅ 该版本一手验过（证据列给地址/出处） · 🟡 待验（从别的版本借来的假设，或单源） · ❌ 已知不同/不存在 · ❓ 存疑 · — 未看
>
> **本页不许出现没有出处的断言。** 从某作借到另一作的判断一律 🟡，在该版本 exe 上验过才能改 ✅（[`METHOD.md`](../../METHOD.md)）。

## TH06NC 入口

TH06NC 的菜单/结果页尚未纳入上面的跨版本断言矩阵。当前已定位结果页 chain 的状态 tick、初始化和 `score.dat` 写回；**结果页全开秘籍已一手反出**——口令 `classiclager` 与一条 XInput 手柄序列，挂在 Lunatic 成绩页（`th06nc:0x14006e190` / `th06nc:0x14006e020`），见 [th06nc/02-classiclager-unlock-cheat.md](th06nc/02-classiclager-unlock-cheat.md)；调查过程见 [th06nc/01](th06nc/01-result-screen-unlock-cheat-recon.md)。

## 备注

尚未做完的部分见 [th16/PLAN.md](th16/PLAN.md)。

[th16/04](th16/04-pr-to-th-re-data.md) 是给 ExpHP `th-re-data` 提 PR 的材料（16 个新函数 + 2 张静态表），
不是引擎结论——回流社区的记录放在这里以免丢失。
