# engine/msg/th18 — TH18 MSG（对话 / 结局）一手验证
> **版本**：TH18 v1.00a（`th18.exe`，imagebase `0x400000`）。本文裸地址默认属该版本；引用其他版本须写成 `th16:0x…`。

> 目的：给 [`../WIKI-DRAFT.md`](../WIKI-DRAFT.md)（THBWiki 翻新稿）提供第二个一手版本，
> 并把 TH16 四篇里标 🟡 的项在 th18 上对死。方法照 [`../th16/README.md`](../th16/README.md)：
> 一手反编译 × 真实文件实测 × 社区表（ExpHP thpages / truth、thmsg）三方交叉。

## 状态：✅ 两套指令集均已在 th18.exe 上一手验完（2026-09-06）

| 文件 | 内容 | 可信度 |
| --- | --- | --- |
| [`01-stage-msg.md`](01-stage-msg.md) | 关卡对话：加载 / ECL 518·519 握手 / 每帧驱动 / 跳过机制 / **opcode 0–36 逐 case 表** / 立绘 ANM 中断协议 / 气泡样式 / 注音语法 / 28 文件实测 | ✅ |
| [`02-ending-msg.md`](02-ending-msg.md) | 结局 + staff：结局编号公式 / 首次观看禁快进 / **opcode 0–17 表** / staff 按难度选文件 / 淡入淡出方向钉死 / 16 文件实测 | ✅ |

**一句话**：TH18 的 MSG 引擎与 TH16 **同构**——指令格式、文本编码、两套指令集、握手机制全部一致；
差异只有 ExpHP 已登记的那几条（4/7/13/23 加 `who` 参数、新增 36 `store`），外加几个常量。

## 从 TH16 借来、在 th18 上对死的 🟡

| TH16 遗留项（`../th16/README.md`） | th18 结论 |
| --- | --- |
| stage op27 是否读 float | **读**：`*(float*)(instr+4)` 作秒数，除以游戏速度后交给 BGM 淡出（`01` §4） |
| stage op17 `\|x,y,文本` 语法 | `\|x偏移,字距,注音文本`：两次 `atoi` + 两次找逗号；写入注音 anm，不占行（`01` §5） |
| ending op0xd 淡入方向 | **淡入**：mode 0 的 on_tick 让黑幕 alpha 从 255 降到 0；op0xe（mode 5）反之（`02` §4） |
| ending op4 / 0xf–0x11 无真实数据 | TH18 的 12 个结局 + 4 个 staff **同样一条都不用**；机制已一手（`02` §3） |
| 文件头 `@8 = 0x100` 字段 | = 入口表每条的第二个 dword（thmsg 打印的 `(256)`）；**引擎两个读取点都不读它**（`01` §1） |

## 环境

- ghidra-re MCP，database `th18`（已套 ExpHP 名 + 36 条 msg labels）。本次新增的命名（`Gui__start_dialogue`
  `0x43e360`、`Gui__initialize` `0x43a730`、`EndingChild__run_msg` `0x42be70`、两个 `ScreenEffect__on_tick_*`）
  已 `symbols.py export th18` 落进 [`../../../games/th18.v1.00a/symbols.json`](../../../games/th18.v1.00a/symbols.json)。
- 样本：`local/th18.v1.00a/dat/{st01a..st07d,e01..e12,staff1..4}.msg`（版权，gitignore）。
  统计脚本：`python3 engine/msg/tools/parse_th16_msg.py <file> --kind stage|ending`。
