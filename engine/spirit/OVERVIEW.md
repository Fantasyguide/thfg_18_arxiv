# spirit — 神灵 / 灵界（Trance）系统（TH13 的第三条资源轴）

> **版本**：跨版本。本文出现的地址一律带版本前缀（如 `th13:0x4be808`）。

TH13《東方神霊廟》的「神灵（神霊）」是**第三条资源轴**：打敌人掉神灵 → 收进槽 → 槽满按灵界键进**灵界（Trance）**，
灵界期间**中弹不扣命**、收集物换外观且奖励翻倍，槽按帧流失到 0 就结束。

```
受伤 / 死亡 ──► SpiritItemManager::spawn_spirit ──► 神灵在场上飘（96px 内被吸）
                                                          │ 距离 ≤20px
                                       CURRENT_SPIRIT_GAUGE（0..600，每 200 一格）+= 2
                                                          │
                             按灵界键（输入位 0x800，须满槽）→ GAME_FLAGS |= 0x100
                                                          │
             灵界中：中弹无伤 / 死亡前自动免费灵界保命 / 槽 -1 帧 → 归零则退出
```

一句话：**灵界 = 用「收神灵充的槽」换的一段限时免伤 + 奖励翻倍状态**，与 TH17 的咆哮是**同构不同源**的两套实现。

## 断言 × 版本矩阵

| 断言 | th12 | th13 | th15 | th16 | th17 | th18 | 证据 |
| --- | :---: | :---: | :---: | :---: | :---: | :---: | --- |
| 有**独立的神灵/动物灵管理器对象** | ❓ | ✅ | ❓ | ❌ | ✅ | ❌ | `th13:0x4383a0`；`th17:0x40e420`；th16/th18 零符号 |
| 收集物是**独立对象池/链表**（不是 item） | ❓ | ✅ | ❓ | ❌ | ✅ | ❌ | th13 256 槽 stride `0x88`；th17 token 链表 |
| 有专用充能槽全局 | ❓ | ✅ | ❓ | ✅ | ✅ | ❌ | `th13:0x4be808`（0..600）；th16 季节槽；`th17` inner+`0x84` |
| 按**专用键**进入一个限时模式 | ❓ | ✅ | ❓ | ✅ | ✅ | ❌ | th13 输入位 `0x800`；th16 `0x800`（第二炸弹）|
| 模式期间**中弹不扣命** | ❓ | ✅ | ❓ | ❌ | ✅ | ❌ | `th13:0x444ca0`；th17 `do_hyper_cancel` |
| 被击中时用该资源**抵消死亡** | ❓ | ✅ | ❓ | ❌ | ✅ | ✅ | `th13:0x444a10`；th17 同上；th18 走卡钩子 |
| 模式期间收集物**换外观 / 奖励翻倍** | ❓ | ✅ | ❓ | ❓ | ❌ | ❌ | th13 anm 脚本 `type*6+4` + 碎片 ×2；th17 **无**此行为（咆哮只改自机射击：`hyper_type` 被 `th17:0x44a766` 读）→ [animal/th17](../animal/th17/01-animal-spirits.md) |
| 手动进入要求**槽满** | ❓ | ✅ | ❓ | ❓ | ✅ | ❌ | `th13:0x405815`（`>= 600`）；th17 满 5 格才置就绪位、之后才读键（`th17:0x40fc8a` / `th17:0x40f37c`）|

> **图例**：✅ 该版本一手验过 · 🟡 待验 · ❌ 已知不同/不存在 · ❓ 存疑/未看
>
> ❓ 的读法：`th12`/`th15` 的 Ghidra 符号表（`th12-funcs.json` 2156 条、`th15-funcs.json` 3068 条）里
> **零条** `spirit`/`trance`/`goast`/`gauge`/`hyper` 命名 —— 这是「没找到」，不是「证死」，
> 要变 ❌ 得按结构体形状扫全 exe（同 [`animal/th18/01`](../animal/th18/01-animal-residuals.md) §4 的待办口径）。

## 版本脉络

- **TH13**：完整实现。神灵对象池 + 600 格槽（3 段）+ 灵界（免伤 / 免费保命 / 奖励翻倍）——
  一手文档 [`spirit/th13/01`](th13/01-spirit-and-trance.md)。
- **TH17**：**另一套**「第三条轴」——动物灵 / 咆哮（`GoastManager` + `zToken` 链表 + `HYPER_FILL` + `do_hyper_cancel`）。
  机制同构（收集 → 充能 → 限时强化 → 中弹保命），**代码不同源**：类名、结构体、字段、地址全不一样。
  见 [`animal/OVERVIEW`](../animal/OVERVIEW.md)。
- **TH16**：第三条轴是**季节槽 + 季节解放**（第二个 `Bomb` 对象），有专用键 `0x800`，但**不是**「中弹免伤」型——
  被击中照样扣命。见 [`season/OVERVIEW`](../season/OVERVIEW.md)。
- **TH18**：没有这套东西。第三条轴是**装备卡**，死亡保护换成**卡侧 deathbomb 钩子 +
  `Player::cancel_impending_death`**（`th18:0x45d590`）。见 [`animal/th18/01`](../animal/th18/01-animal-residuals.md)。
- **TH12 / TH14 / TH15**：符号侧零证据；**未验** ❓。TH15 的第三条轴是「灵界」以外的
  trance-无关机制（本仓未系统反过）。

## 相关一手文档

- TH13 神灵 / 灵界**全链一手**（六条生成通道 / ECL `spec0-3` 分发表 / 运动半径与速度 / 槽数值 / 灵界进出 /
  「中弹不死」地址链 / HUD 子脚本）：[`spirit/th13/01`](th13/01-spirit-and-trance.md)
- TH13 侧 `dropItems` / `dropItemsSp` 的分发表订正：[`spirit/th13/01`](th13/01-spirit-and-trance.md) §3.3
- ★ TH13 灵界里的**自机攻击方式改变**（主炮 shooterset 组号硬换成常量 **10**；四机体逐字节表 + 「没有伤害倍率」的证伪式结论 + 退出 70 帧无敌的订正）：[`spirit/th13/05`](th13/05-trance-attack.md)
- TH07 森罗结界**持续期视觉**（全屏变暗 37.5 % + 自机红白闪；环只 33 帧、op1 ≡ delete）：
  [`spirit/th07/01`](th07/01-barrier-visual.md)
- TH17 动物灵 / 咆哮（同构对照）：[`animal/OVERVIEW`](../animal/OVERVIEW.md)
- TH16 季节系统（同款「道具 → 槽 → 按 C」+ 同款 HUD 淡出触发区）：[`season/OVERVIEW`](../season/OVERVIEW.md)、
  [`season/th16/01`](../season/th16/01-season-data-and-tables.md) §9
- TH18 残骸审计：[`animal/th18/01`](../animal/th18/01-animal-residuals.md)
- 跨版本台账：[`_shared/version-residuals.md`](../_shared/version-residuals.md)
