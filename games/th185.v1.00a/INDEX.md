# th185.v1.00a —— 弹幕狂热者的黑市（100th Black Market）

> **版本**：TH18.5 v1.00a（Steam 版；`readme.txt` 标 2022/08/14）。

- **样本**（本机，gitignored）：`local/th185.v1.00a/th185.exe`
  （md5 `234b1e6eb1cc1b8b0c6e490487cf191c`；来自用户 2026-09-18 指的
  `D:\SteamLibrary\steamapps\common\th185`。同目录的 `th185.dat`（资源档）、`thbgm.dat`、
  `custom.exe`、`omake.txt`、`readme.txt` **留在原处未复制**）。
  ⚠ **这份 Steam 版是 SteamStub 包装过的**（EP 落在 `.bind`，`.text` 加密）：**反汇编前必须先用仓内
  `tooling/steamless` 脱壳**，产物是 `local/th185.v1.00a/th185.exe.unpacked.exe`（判据与对账过程见
  [`engine/card/th185/README.md`](../../engine/card/th185/README.md) §5）。
- **社区命名金矿**：`local/vendor/th-re-data/data/th185.v1.00a/`
  （funcs 58 / statics 41 / own 结构体 36 / ext 结构体 89）。
  ⚠ **比 th18 那份薄得多**：`zTableCardData` / `zCardBaseClass` / `zVTableCard` / `zAbilityMenu`
  这些**卡系统自己的类型名在 th185 里没有** —— 卡这一层在 th185 侧要靠反汇编自己认。
- **本仓已挖**：**卡牌 / 货币系统的第一手锚点清单**见
  [`engine/card/th185/README.md`](../../engine/card/th185/README.md)
  （`AbilityManager` / `BlackMarket` / `PermanentCardShop` 三个系统的地址与结构大小已列全）。
- **与本作的关系**：TH18.5 是 TH18 的**直接后继**，`AbilityManager` 一系同源（但结构大小不同，
  见上面那份清单），因此本仓的 [`engine/card/`](../../engine/card/OVERVIEW.md) 跨版本矩阵**加一列 th185**。
- **状态**：🟡 侦察。目标三问：① 它的**全部卡**能否搬进 TH18 卡牌 mod；② **货币（弹货）**能否复刻；
  ③ 卡表要扩到 **id > 128**，先查清 >128 到底坏在哪。
