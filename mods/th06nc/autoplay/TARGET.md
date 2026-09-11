# TARGET —— th06nc/autoplay 死绑登记

> **版本**：TH06NC（Steam 2026 重制版，`th06nc.exe`，imagebase `0x140000000`）。
> 本文裸地址默认属该版本；引用其他版本一律写成 `th06:0x…`。
>
> 这份文件回答一个问题：**换一个 exe build，我要重取哪些量？**

对账脚本：`native/check.py`。它把 `native/th06nc.h` 里的每个值与 `games/th06nc/symbols.json`
和 `tooling/ghidra/th06nc_{player,bullet,enemy,ecl}_spec.py` 逐条比，对不上就失败。

## 1. 目标二进制

| 项 | 值 |
| --- | --- |
| 游戏 / 版本 | 東方紅魔郷 New Classic（TH06NC） |
| exe md5 | `b0cc28ec904e9efce2439ad857c938b5` |
| 架构 | PE32+ x86-64 |
| imagebase | `0x140000000` |
| 版本校验 | 运行时比对 `Player__OnUpdate` 入口 13 字节，见 §2 |

## 2. 唯一的写入点 / hook 点

与 [`../mouse-control/TARGET.md`](../mouse-control/TARGET.md) §2 完全相同：
`Player__OnUpdate` `0x140068820` 入口 13 字节 `48 8b c4 57 41 54 48 81 ec d8 00 00 00`，
跳板与入口补丁逐字节相同（`check.py` C–E 节反汇编核对），只是回调换成 `agent_tick`。

写入的内存只有一处：`INPUT_HELD` `0x140a6ec60` 的低 7 位（`0xf7`）。

## 3. 代理入口

`VCRUNTIME140_1.dll`，转发 `__CxxFrameHandler4`。论证与 mouse-control 相同，不重复。

## 4. codecave 调用的引擎函数

**一个都没有。** 抽取器只读内存，策略是纯 C，注入只写一个 dword。

## 5. 读取点

全部只读，全部有一手出处；`check.py` §B 逐条对账。

### 全局

| 名字 | 地址 | 用途 | 依据 |
| --- | --- | --- | --- |
| `g_Player` | `0x1404ff3a0` | 玩家对象 | [`player/01`](../../../engine/player/th06nc/01-main-loop-and-collision.md) §2.5 |
| `playerMovementArea` | `0x1404ff0e0` | 可动区 4 个 f32（运行时才填，读到 0 退回 `(8,16)+(368,416)`） | 同上 §2.11 |
| `BulletManager.timer` | `0x1403ec2a0 + 4` | `ZunTimer.current` → 帧号 | [`bullet/01`](../../../engine/bullet/th06nc/01-pools-and-structs.md) §1 |
| `g_Bullets` | `0x1403ec2a8` | 640 × `0x620` | 同上 |
| `g_Enemies` | `0x140aa1e98` | 256 × `0x10b0`（`EnemyManager 0x140aa1e90 + 8`） | [`enemy/01`](../../../engine/enemy/th06nc/01-pool-and-spawn.md) §1 / §6.3 |
| `livesRemaining` / `bombsRemaining` | `0x1404ff0f0` / `0x1404ff0f1` | u8 | `player/01` §2.19 |
| `currentPower` | `0x1404f1e88` | u16 | [`sht/th06nc/README`](../../../engine/sht/th06nc/README.md) |
| `g_Score` / `g_GrazeInStage` | `0x1404f2798` / `0x1404ff0cc` | u64 / i32 | `player/01` §2.9 |
| `g_BombInUse` | `0x140509268` | u8 | 同上 |
| `spellcardInfo.isActive` | `0x140badf60` | i32（= `EnemyManager + 0x10c0d0`） | `player/01` §2.16、`enemy/01` §6.3 |

### `Player` 偏移

| 偏移 | 字段 | 依据 |
| --- | --- | --- |
| `+0x7730` / `+0x7734` | `positionCenter.x/y`（关卡区局部，原点左上角） | `player/01` §2.5 |
| `+0x774c` | `hitboxRadius`（1.25，被弹与擦弹共用） | §2.7 |
| `+0x785c` | `isFocused` | §2.10 |
| `+0x7860` / `+0x7864` | 正交速度 / 低速 | §2.10 |
| `+0x7898` | `playerState`：0 ALIVE / 1 SPAWNING / 2 DEAD / 3 INVULNERABLE | §2.9 |

### `Bullet` 偏移（步长 `0x620`）

| 偏移 | 字段 | 依据 |
| --- | --- | --- |
| `+0x008` | `velocity`（x,y） | `bullet/01` §2 |
| `+0x030` | `pos`（x,y） | 同上 |
| `+0x044` | `state`（i16；0 空槽不入表） | 同上 |
| `+0x5f4` / `+0x5f8` | `grazeSize.x/y` → 致死半径 = `min × 0.5`（+ 自机 1.25） | `player/01` §2.7 完整公式 ✅ |
| `+0x618` | `isGrazed` | 同上 |

### `Enemy` 偏移（步长 `0x10b0`）

| 偏移 | 字段 | 档 | 依据 |
| --- | --- | --- | --- |
| `+0x0b0` | `position` | code | `enemy/01` §5 |
| `+0x0bc` | `flags`，bit7 = 槽占用 | code | `enemy/01` §1 |
| `+0x0bd` | 第二个 flags 字节，`& 3 == 3` 才做本体碰撞 | code | `player/01` §2.7（`EnemyManager__OnUpdate` 的调用门） |
| `+0x230` | `bossId`（≥ 0 视为 boss） | **weak** | [`ecl/04`](../../../engine/ecl/th06nc/04-enemy-struct.md) §3 |
| `+0x234` / `+0x238` | `life` / `maxLife` | code | `enemy/01` §5 |
| `+0x2c0` | `hitboxDimensions`（x,y）→ 致死盒半宽 = `/ 3` | code | `player/01` §2.7（× 2/3 再 × 0.5） |

### 逻辑按钮位（`INPUT_HELD`）

| 位 | 含义 |
| --- | --- |
| `0x01` / `0x02` / `0x04` | 射击 / 炸弹 / 低速 |
| `0x10 / 0x20 / 0x40 / 0x80` | 上 / 下 / 左 / 右 |

出处：[`engine/menu/th06nc/02`](../../../engine/menu/th06nc/02-classiclager-unlock-cheat.md) §8。

## 6. 判定公式（✅ 一手）与仍待实机的时序（🟡）

**判定式已按 `Player__CalcKillBoxCollision` `0x14006a980` 原样复刻**（[`player/01`](../../../engine/player/th06nc/01-main-loop-and-collision.md) §2.7，2026-09-11 MCP 直读）：

| 对象 | 引擎 | 本 mod |
| --- | --- | --- |
| 弹 | `R = min(grazeSize) × 0.5 + 1.25`，`dist² < R²` | `bullet.radius = min × 0.5`，策略算 `dist − (r + radius)` |
| 敌人本体 | 仅 `(enemy+0xbd & 3) == 3`；盒半宽 = `hitboxDimensions × 2/3 × 0.5`，圆心钳到盒内再比 1.25 | `collidable` 门 + `hit_w = hitbox / 3`，策略同式钳位 |

两个常量：`0x14030cd58` = 0.5f、`0x14030cd74` = 0.6667f，MCP `read_bytes` 直读。

仍是 🟡 的只有时序两条：

1. **hook 处读到的世界是「上一帧终态」还是「本帧改到一半」**：取决于 Player 在 calc 链里
   排在 Bullet / Enemy 之前还是之后（已知 Stage 6、EnemyManager 9，Player 未读出）。
   对内嵌策略影响很小（一帧的外推误差），对将来的协议语义有影响，spec §6b.4 已登记。
2. **炸弹按下沿**：`Player__OnUpdate` 自己比 `INPUT_HELD` 与 `INPUT_PREV`（`0x140a6ec64`）
   判上升沿。mouse-control 用中键 OR 上 `0x02` 实机放出过炸弹，说明在入口写入的位能形成沿。
   本 mod 只在那一帧写 `0x02`、下一帧清掉，与中键按下一帧等价。

弱锚只剩 `bossId`（只影响「对齐谁的 x」）。

## 7. 换版本时必须重取

- [ ] `Player__OnUpdate` 地址与入口 13 字节
- [ ] `INPUT_HELD` / `g_Player` / `playerMovementArea`
- [ ] 弹池 / 敌池基址、步长、数量，以及 §5 列出的每一个偏移（NC 相对 TH06 重排过，见 `ecl/04` §1）
- [ ] 命 / 炸弹 / 火力 / 分数 / 擦弹 / 符卡 / 放弹中 七个全局
- [ ] 按钮位定义
- [ ] 重新确认 `SetDefaultDllDirectories` 仍未被调用，否则代理路线失效

可以借的只有：方法论、字段语义、跳板设计、策略代码。
