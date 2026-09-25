# ufo — 飞碟系统（TH12《星莲船》特有：令牌 → UFO → 奖励）
> **版本**：跨版本。本文出现的地址一律带版本前缀（如 `th12:0x4b0c4c`）。

TH12 的「UFO」是**第三条资源轴**，与 Bomb / 火力平行：敌人掉彩色**令牌**，收满 3 个同色
（或三色各一）就召出一只 ECL 敌机 UFO；UFO 在上方飘、把场上的 P/点道具吸走充能，被击破时
按吸收量吐分数道具，并按颜色给残机碎片 / Bomb 碎片 / 彩虹令牌。

```
敌人死 ──dropExtra/dropMain──► 令牌道具(类型 10..12, 固定色)
                                   │ 自机吃到
                                   ▼
             Ufo 对象里的 3 槽状态机（GLOBALS.COLLECTED_UFOS[3]）
   三同色 ─► 该色 UFO      三色各一 ─► 彩虹 UFO      两同一异 ─► 保留最近 2 个
                                   │ 召唤（Ufo::on_tick 检测到计数 == 3）
                                   ▼
   ECL 敌机 UFO_Red/Blue/Green/Rainbow（default.ecl）
   在上方 280×128 框里飘；把类型 1/2 的道具吸过来充能；被打死 → 奖励
```

一句话：**UFO = 「令牌收集器 + 道具吸尘器 + 分数转换器」**，全部状态住在单个 `Ufo` 对象
（ZUN 源码里叫 `UfoInf`）里，敌机只是它的「身体」。

## 断言 × 版本矩阵

| 断言 | th12 | th13 | th16 | th17 | th18 | 证据 |
| --- | :---: | :---: | :---: | :---: | :---: | --- |
| 有 UFO 令牌道具（收 3 个同色召唤） | ✅ | ❌ | ❌ | ❌ | ❌ | [th12/01](th12/01-ufo-system.md) §3；其余：exe 内无 `UFO` 串、th-re-data 无 `Ufo` 符号 |
| 有全局 3 槽 `COLLECTED_UFOS[3]` + 计数 | ✅ | ❌ | ❌ | ❌ | ❌ | `th12:0x4b0c4c`、`th12:0x4b0c5c`、`th12:0x422e80` |
| 有独立 `Ufo`/`UfoInf` 对象（`malloc(0x6c)`） | ✅ | ❌ | ❌ | ❌ | ❌ | `th12:0x449fa0`、`th12:0x44a230`、`th12:0x44a2d0` |
| UFO 是 ECL 敌机（`default.ecl` 的 `UFO_*` 子程序） | ✅ | ❌ | ❌ | ❌ | ❌ | [th12/01](th12/01-ufo-system.md) §6 |
| UFO 会吸走场上 P/点道具并充能（`fill` 0..1） | ✅ | ❌ | ❌ | ❌ | ❌ | `th12:0x4271c0`、`th12:0x44aca0` |
| 击破 UFO 给残机碎片（红）/ Bomb 碎片（绿） | ✅ | ❌ | ❌ | ❌ | ❌ | `th12:0x44af00`、`th12:0x422d30`、`th12:0x422e10` |
| 彩虹令牌按计时换色（约 150 帧，靠近暂停） | ✅ | ❌ | ❌ | ❌ | ❌ | [th12/01](th12/01-ufo-system.md) §9；`th12:0x4263xx` 段 |
| 有「道具吸收充能 → 阈值出奖励」这一机制 | ✅ | ❌ | ❌ | ❌ | ❌ | `th12:0x44aca0` 的 `fill > 1.0` 分支；阈值表见 §7 |

> **图例**：✅ 该版本一手验过 · 🟡 待验 · ❌ 已知不同/不存在 · ❓ 存疑。
>
> ❌ 的依据（可复核、非推断）：`strings local/th13.v1.00c/th13.exe` 等 5 个样本
> （th13 / th15 / th16 / th17 / th18）**都没有** `UFO` / `UfoInf` 串；
> `local/vendor/th-re-data/data/<版本>/` 的 funcs / statics / type-structs-own 里
> **没有**任何 `ufo` 符号或 `zUfo` 结构体。⇒ 这套系统是 **TH12 专属**，后续作未继承。

## 版本脉络

- **TH12**：完整实现（令牌槽状态机 + 4 色 UFO + 吸收充能 + 击破奖励 + 彩虹令牌换色）。
  召唤瞬间的「缩小的紫圆 + 单像素蓝闪」是 **C 侧 `EffectManager` 的程序化特效**（`th12:0x44a947` spawn
  effect #2），**不在 anm 里**；同一次召唤另有四棵 anm VM（横幅 / 常驻光环 / 碟体 / 血槽）—— 见
  [`ufo/th12/01`](th12/01-ufo-system.md) §10c、§10d。
- **TH13 / TH15 / TH16 / TH17 / TH18**：没有这套系统（见上表 ❌ 的证据）。
  各作的「第三条轴」是别的东西：TH13 是灵界/trance，TH16 是季节（[`season/OVERVIEW.md`](../season/OVERVIEW.md)），
  TH18 是卡牌（[`card/`](../card/)）。
- TH12 里 UFO 的**身体**是普通 ECL 敌机，所以它的移动/消弹/离场全写在 `default.ecl` 里；
  引擎侧只负责「什么时候召唤」和「吸到什么程度给什么奖励」。这种「C++ 控制器 + ECL 身体」
  的分工在后续作里没再出现（后续作的同类系统都在 C++ 侧）。

## 相关一手文档

- TH12 UFO **全链一手**（令牌槽状态机 / `zUfo` 字段表 / 召唤 / 吸收阈值 / 击破奖励 / anm 脚本号）：
  [`ufo/th12/01`](th12/01-ufo-system.md)
- 召唤特效的**引擎侧**（紫圆 + 蓝闪 = `EffectManager` 的 effect #2/#1）：一手的颜色是 exe 里的立即数
  ——纯紫 RGB(255,0,255)、蓝 RGB(0,64,255)——anm 库里查不到；见同文 **§10d**
- 样本与工程登记：[`games/th12.v1.00b/INDEX.md`](../../games/th12.v1.00b/INDEX.md)
- 相邻子系统：ECL VM [`ecl/OVERVIEW.md`](../ecl/OVERVIEW.md)、
  道具/道具管理器（th12 侧尚未单独成文）、社区来源 [`_shared/community-sources.md`](../_shared/community-sources.md)
