# enemy —— 敌人子系统

> 跨版本断言汇总页。一手过程在 `<版本>/` 下，本页只放结论并附链接。
> **本页不许出现没有出处的断言。**

## TH06NC 入口

`EnemyManager__SpawnEnemy` `th06nc:0x1400360d0` 是最好的抓手——找空闲槽的循环
把池的三个立即数直接给了出来。逐帧 tick 在 `EnemyManager__OnUpdate` `th06nc:0x1400373b0`。

| 断言 | TH06 | TH06NC | 出处 |
| --- | --- | --- | --- |
| `sizeof(Enemy)` | `0xec8` | **`0x10b0`** | [th06nc/01 §2](th06nc/01-pool-and-spawn.md) |
| `enemies[]` 数量 | 256 | **256（没变）** | 同上 |
| 池与模板的位置 | 池 `+0xed0`、模板 `+0x08` | **对调**：池 `+0x08`、模板 `+0x10c130` | 同上 |
| 槽占用位 | `enemy+0xe50` bit7 | `enemy+0xbc` bit7（同语义，异位置） | 同上 |
| `SpawnEnemy` 参数个数 | 5（`RET 0x14` 为证） | **6+1**：多一个父 `EclContext` 指针 | [th06nc/01 §3](th06nc/01-pool-and-spawn.md) |
| **`ENEMYCREATE` 子敌人继承父敌人的 ECL 变量** | ❌ 无此行为 | ✅ 14 个 dword 整块拷贝 | [th06nc/01 §3](th06nc/01-pool-and-spawn.md) 🟡 未实机 |
| `EnemyEclContext` 位置 | `enemy+0x990` | `enemy+0x40` | [th06nc/01 §4](th06nc/01-pool-and-spawn.md) |
| `maxLife` 写入次数 | 1 次 | **2 次**（多写一份到 `+0x23c`）❓ | [th06nc/01 §5](th06nc/01-pool-and-spawn.md) |

> **结构体两边都重排过**，偏移一律要在各自 exe 上重取；只有「有哪些字段」可以借。

## 相关但不在本页

- **由 ECL opcode 交叉比对得到的 15 个 `Enemy` 字段**在
  [`../ecl/th06nc/04`](../ecl/th06nc/04-enemy-struct.md)——那是它们的出处。
  合并成一张表是待办。
- **敌人受伤与死亡结算**在
  [`../player/th06nc/01`](../player/th06nc/01-main-loop-and-collision.md) §2.16
  （`Player__CalcDamageToEnemy` 与 `EnemyManager__OnUpdate` 的那一段）。
