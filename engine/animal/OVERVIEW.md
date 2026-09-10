# animal — 动物灵 / 咆哮（HYPER）系统（TH17 的招牌机制）
> **版本**：跨版本。本文出现的地址一律带版本前缀（如 `th17:0x4b7684`）。

TH17《鬼形兽》的动物灵是**第三条资源轴**：打敌人 → 掉动物灵 → 收进槽 → 槽满进**咆哮（ロアリング）**，
咆哮期间火力变强、**中弹只是退出咆哮而不是死**。游戏内文本把这段叫**霊撃**（spirit strike）。

```
ECL dropMain(30..33) ──► item type 30..33 ──► GoastManager::spawn_token ──► zToken 在场上飘
        (boss / 中boss 死亡)                          th17:0x410380                │ 24 px 内收集
                                                                                    ▼
                                          TOKENS[5] 队列 / HYPER_FILL ──► 满 5 格 = 咆哮就绪
                                                                                    │
                                          按炸弹键（输入掩码 bit 1）→ do_hyper_cancel  th17:0x40f6d0
                                                                                    │
                                          中弹 → Player::die（state 4）→ 同上(by_hit=1)（不扣命）
```

> ⚠️ **不是「按 C」**：TH17 里咆哮与**炸弹同键**（输入掩码 `0x2`）；槽满时 `player_is_trying_to_bomb`
> `th17:0x411d40` 主动屏蔽炸弹，同一按键就变成霊撃 ✅（一手，见 [th17/01](th17/01-animal-spirits.md) §5.2）。

## 断言 × 版本矩阵

| 断言 | th16 | th17 | th18 | 证据 |
| --- | :---: | :---: | :---: | --- |
| 有动物灵管理器对象（`zGoastManager` + 全局指针） | ❌ | ✅ | ❌ | `th17:0x4b7684`；[th17/01](th17/01-animal-spirits.md) §2.1 |
| 动物灵是独立对象（`zToken` 链表，非 item） | ❌ | ✅ | ❌ | `th17:0x4111a0` 链表插入；但 item type 16..32 会**转成** token |
| 有 5 格动物灵**队列**（`TOKENS[5]`，存类型而非计数） | ❌ | ✅ | ❌ | `th17:0x40f9ab` `TOKENS[HYPER_FILL] = type` |
| 有咆哮进度（`HYPER_FILL` = 已收数 0..5） | ❌ | ✅ | ❌ | `th17` inner+0x84 |
| 有咆哮计时 / 标志（`HYPER_TIME` / `HYPER_FLAGS`） | ❌ | ✅ | ❌ | `th17` inner+0xc8 / +0xe8；位表见 [th17/01](th17/01-animal-spirits.md) §5.1 |
| 中弹只退出咆哮（不扣命） | ❌ | ✅ | ❌ | `th17:0x449420` → `th17:0x447bf8` → `th17:0x4495e0` |
| 「用资源换不死」的**通用**机制 | ❌ | ✅（霊撃） | ✅（deathbomb + 卡钩子） | `th18:0x45d590 Player::cancel_impending_death` |

> **图例**：✅ 一手验过 · 🟡 待验 · ❌ 已知不同/不存在 · ❓ 存疑

## 版本脉络

- **TH17**：完整实现（管理器 + token 链表 + 5 格队列 + 咆哮计时/标志 + 霊撃 + 中弹取消路径）。
  一手全链见 [th17/01](th17/01-animal-spirits.md)；对账见其 §9。
- **TH18**：**没有**这套东西。TH18 的「第三条轴」是**装备卡**（`zAbilityManager`），
  死亡保护也换成了**卡侧的 deathbomb 钩子 + `Player::cancel_impending_death`**。
- TH18 的 `zGlobalsInner` 里确实躺着一批「只被初始化、没有读者」的死槽
  （`th18:0x4ccda0` 起一串，见 [th18/01](th18/01-animal-residuals.md) §2b）——
  位置与 TH17 的 `HYPER_TIME` / `HYPER_FLAGS` 巧合，但**没有语义证据**把它们认成咆哮残骸 🟡。

## 相关一手文档

- TH17 动物灵 / 咆哮全链：[`animal/th17/01`](th17/01-animal-spirits.md)
- TH18 侧残骸审计：[`animal/th18/01`](th18/01-animal-residuals.md)
- TH18 玩家状态机 / 计时器：[`player/th18/01`](../player/th18/01-position-and-state-timers.md)
- TH18 伤害来源与死亡路径：[`player/th18/02`](../player/th18/02-damage-sources.md)
- 跨版本台账：[`_shared/version-residuals.md`](../_shared/version-residuals.md)
