# std —— 关卡背景脚本子系统

> **版本**：跨版本。地址一律带前缀（`th06nc:` / `th06:`）；表里的裸十六进制是
> **偏移或大小**，不是地址。
>
> 跨版本断言汇总页。一手过程在 `<版本>/` 下，本页只放结论并附链接。
> **本页不许出现没有出处的断言。**

STD 是关卡的**背景**脚本：相机位置、朝向、雾、以及一张静态的方块（quad）几何表。
它不管弹幕和敌人——那是 ECL 的活。

## TH06NC 入口

`Stage__OnUpdate` `th06nc:0x140077bb0`（calc chain 优先级 6）就是解释器；
六项跳表在 `th06nc:0x14007826c`。

| 断言 | TH06 | TH06NC | 出处 |
| --- | --- | --- | --- |
| 指令记录 | `{i32 frame; i16 opcode; i16 size; i32 args[3]}` = `0x14` | **完全相同** | [th06nc/01 §2](th06nc/01-stage-runtime-and-nc-loop.md) |
| opcode 集合 | 0–5（`StageOpcode` 六项） | **0–5，一条没加** | [th06nc/01 §3](th06nc/01-stage-runtime-and-nc-loop.md) |
| 跳表顺序 | 与 `StageOpcode` 枚举一致 | **逐项一致**（有测试比对） | 同上 |
| **opcode 3 的参数 `-1` / `-2`** | ❌ 无（`th06:0x403810` 一手证伪） | ✅ **记回跳点 / 跳回去**，构成背景循环 | [th06nc/01 §4](th06nc/01-stage-runtime-and-nc-loop.md) |
| 雾的下发 | `SetRenderState(D3DRS_FOG*)` ×3 | 两个**带脏检查的缓存写入器** | [th06nc/01 §5](th06nc/01-stage-runtime-and-nc-loop.md) |
| chain 优先级 | calc 6 / draw 3 / draw 4 | calc 6 / draw **5** / draw **6** | [th06nc/01 §6](th06nc/01-stage-runtime-and-nc-loop.md) |
| `Stage` 字段偏移 | 见 CC0 `Stage.hpp` | **重排过，不能照抄** | [th06nc/01 §7](th06nc/01-stage-runtime-and-nc-loop.md) |
| 关卡名/曲名 | 头里是日文字面量 | **本地化 key**（`ST_STAGE1_STAGE_NAME`） | [th06nc/01 §8](th06nc/01-stage-runtime-and-nc-loop.md) 🟡 |

## 一句话结论

**NC 的 STD 就是 TH06 那台机器原样重编译**，唯一的行为增量是 opcode 3 的两个魔数参数——
一对「记点 / 跳回」，让背景可以无限循环。样本里只有 Extra 关（`stage7.std`）在用。
