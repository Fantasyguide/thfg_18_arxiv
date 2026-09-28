# stone — 异变石系统（TH20《錦上京》特有：四槽石 → 异变攻击 → 异变敌人 → 异变值）

> **版本**：跨版本。本文出现的地址一律带版本前缀（如 `th20:0x5b0ab0`）。

TH20 的「异变石」（日文 **異変石**）是**一整套把「过往异变」当装备戴的系统**，一条链串起四件事：

```
装备四槽石（主/扩散/集中/支援）
   │ 石头自带 30 槽虚表：主石决定「异变攻击」样式，另外三槽决定高速/低速射击样式与支援效果
   ▼
异变石碎片（道具）──吸收──► 异变攻击槽（水平，画面左下）
                              │ 满 ⇒ 自动发动异变攻击（样式 = 主石；时长由公式现算）
                              ▼
                       异变攻击期间中弹 ⇒ 抵消本次 Miss（本仓救援阶梯的第三级）
                              
击破敌人 / 擦弹 ──► 异变敌人槽（竖直，画面左下）──满──► 召唤「异变敌人」（颜色由主石决定）
                        ▲                                      │ 击破
                        └── Miss 降级、同色再出升级 ◄───────────┘ 大量道具 + 碎片 + 异变值
异变值（画面右侧，上限 5000）──► 影响得分道具的得分倍率
```

一句话：**异变石 = 「戴着过往作品的弹幕」**——石既是**装备**（决定你这一局打什么），
又是**进度条**（碎片充攻击槽）、又是**敌人管理器**（异变敌人槽）与**倍率旋钮**（异变值）的入口。

放在 `engine/stone/` 而不是 `th20/` 目录下是刻意的（与 [`ufo/`](../ufo/OVERVIEW.md)、
[`season/`](../season/OVERVIEW.md)、[`card/`](../card/) 同规矩）：**特有机制也是子系统**。
矩阵加列即可，不必再开版本目录。

## 断言 × 版本矩阵

| 断言 | th06–th18.5 | th20 | 证据 |
| --- | :---: | :---: | --- |
| 存在「异变石」这套装备/进度系统 | ❌ | ✅ | 见下方 ❌ 的可复核依据 |
| 石是 30 槽虚表的对象，主石决定异变攻击 | — | ✅ | [th20/01](th20/01-stone-table-and-slots.md) §4 |
| 四槽 = 主/扩散/集中/支援，每槽一个石头对象 | — | ✅ | 同文 §2：`WeaponStoneInf+0x28/+0x2c/+0x30/+0x34` |
| 每角色 8 块石，`[角色][9]` 工厂表 | — | ✅ | 同文 §1：表 `th20:0x5b0ab0` |
| 普通魔石（COMMON）= 复制主石效果 | — | ✅ | 同文 §2：`stone_id == 8` ⇒ 换成主石 id |
| 异变攻击是「槽满自动发动」的三态状态机 | — | ✅ | [th20/06](th20/06-attack-gauge-and-miss.md) §1 |
| 时长由公式现算（`base × (1+a/100)`） | — | ✅ | 同文 §4：`th20:0x534260` |
| 异变攻击的弹幕写在 `default.ecl` 的 20 个子程序里 | — | 🟡 | [th20/02](th20/02-attack-assets.md) |
| 异变攻击抵消 Miss | — | ❓ | [th20/07](th20/07-miss-cancel-chain.md)（**引擎侧链路未坐实**）|
| 异变敌人 = 竖直槽满召唤、同色升级、Miss 降级 | — | 🟡 | [th20/03](th20/03-stone-enemy.md) |
| 异变值影响得分道具倍率 | — | 🟡 | [th20/04](th20/04-incident-value.md) |
| 碎片道具 ≈ 季节道具（TH16 那条轴） | ✅ th16 | 🟡 | [th20/05](th20/05-shards-and-items.md)、[`season/OVERVIEW.md`](../season/OVERVIEW.md) |

> **图例**：✅ 该版本一手验过 · 🟡 单源/推断 · ❌ 已知不同/不存在 · ❓ 存疑。

**❌ 那一列的依据（可复核、非推断）**：本机 `local/` 里从 TH06 到 TH18.5 **全部 17 个样本**
的 exe 都 `grep` 不到 `StoneBaseInf` / `WeaponStoneInf` / `StoneEnemyInf`
（这三条是 TH20 的 RTTI 类名串，见 [th20/01](th20/01-stone-table-and-slots.md)）。
⇒ 这套系统是 **TH20 专属**，后续作/前作都没有继承。
（TH20 太新，社区金矿 `th-re-data` 也没有它，见 [`games/th20.v1.00a/INDEX.md`](../../games/th20.v1.00a/INDEX.md) §2c。）

## 版本脉络

- **TH20**：完整实现（四槽石 + 20 个 ECL 攻击子程序 + 异变敌人 + 异变值 + 碎片道具）。
  与其他作的「第三条轴」对照：TH12 是飞碟令牌（[`ufo/`](../ufo/OVERVIEW.md)）、
  TH13 是灵界（[`spirit/`](../spirit/OVERVIEW.md)）、TH16 是季节（[`season/`](../season/OVERVIEW.md)）、
  TH17 是动物灵（[`animal/`](../animal/OVERVIEW.md)）、TH18 是卡牌（[`card/`](../card/)）。
  **TH20 的特色是「把前作当装备」**——石名就是前作副标题（スカーレットデビル＝紅魔郷、
  スノーブロッサム＝妖々夢、インペリシャブルムーン＝永夜抄…见
  [th20/01](th20/01-stone-table-and-slots.md) §6）。
- **TH06–TH18.5**：没有这套系统（见上表 ❌）。

## 相关一手文档

- 石表 / 四槽 / 30 槽接口：[`stone/th20/01-stone-table-and-slots.md`](th20/01-stone-table-and-slots.md)
- 异变攻击的 ECL/anm/SHT 资产侧：[`stone/th20/02-attack-assets.md`](th20/02-attack-assets.md)
- 异变敌人（竖直槽 / 升级 / 掉落）：[`stone/th20/03-stone-enemy.md`](th20/03-stone-enemy.md)
- 异变值与得分倍率：[`stone/th20/04-incident-value.md`](th20/04-incident-value.md)
- 碎片道具（对 116 季节道具）：[`stone/th20/05-shards-and-items.md`](th20/05-shards-and-items.md)
- 异变攻击状态机：[`stone/th20/06-attack-gauge-and-miss.md`](th20/06-attack-gauge-and-miss.md)
- 「抵消 Miss」链：[`stone/th20/07-miss-cancel-chain.md`](th20/07-miss-cancel-chain.md)
- 样本与工程登记：[`games/th20.v1.00a/INDEX.md`](../../games/th20.v1.00a/INDEX.md)
- 挖库手册（TH20 没有社区符号怎么办）：[`tooling/ghidra/README.md`](../../tooling/ghidra/README.md)
