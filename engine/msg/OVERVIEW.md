# msg — MSG 对话/文本系统

`.msg` 是一个容器，但**有两套互不相干的消费者**：关卡内对话（`GuiMsgVm`）和结局/staff roll（`Ending`）。
两者**各有一套 opcode**，编号重叠但含义不同——这是读 MSG 最容易踩的坑。

关卡对话与 ECL 之间是**协程式握手**：ECL 用 ins 518/519 把控制权交给 MSG VM，等它跑完再收回。
文本的实际绘制走 ANM 精灵，不是 GDI——所以「换字体/排版」的接缝在 ANM 那边，不在 MSG。
MSG 也不画立绘：它只给立绘 anm 发中断（1 隐藏 / 2 高亮 / 3 变暗 / 7 抖动 / 17+n 表情）。

## 断言 × 版本矩阵

| 断言 | th16 | th18 | th19/th20 | 证据 |
| --- | :---: | :---: | :---: | --- |
| `.msg` 有两套独立指令集：关卡对话 vs 结局/staff | ✅ | ✅ | 🟡 | [th16/01 §1,§5](th16/01-architecture-overview.md)、[th18/README](th18/README.md) |
| 文件头 = `u32 N` + N×`{offset, flags}`；引擎只读 offset，flags 恒 256 且无人读 | 🟡 | ✅ | 🟡 | [th18/01 §1](th18/01-stage-msg.md)、[th16/04 §1.5](th16/04-ending-staff-msg-instruction-set.md) |
| 指令 `{u16 time, u8 op, u8 size, args}`，无对齐填充 | ✅ | ✅ | 🟡 | [th16/01 §3](th16/01-architecture-overview.md)、[th18/01 §1](th18/01-stage-msg.md) |
| 文本 Shift-JIS + 加速 XOR（0x77/0x07/0x10），TH12 起注音残留 bug | ✅ | ✅ | 🟡 | [th16/02 §1](th16/02-msg-vm-opcodes.md)、[th18/01 §7](th18/01-stage-msg.md) |
| 关卡对话由 `GuiMsgVm::run` 驱动，opcode 0..35（th16）/ 0..36（th18） | ✅ | ✅ | 🟡 | [th16/02 §2](th16/02-msg-vm-opcodes.md)、[th18/01 §4](th18/01-stage-msg.md) |
| ECL **518/519** 与 MSG ins 12 经 `vm` 一帧脉冲握手 | ✅ | ✅ | 🟡 | [th16/03 §2](th16/03-dialogue-lifecycle.md)、[th18/01 §2](th18/01-stage-msg.md) |
| 518 负数 id：−1 boss 曲 + 曲名、−3 道中曲 + 曲名、−2 结束关卡 | — | ✅ | 🟡 | [th18/01 §2](th18/01-stage-msg.md) |
| pause 期间脚本计时冻结；快进 = 把计时拨到当前指令时间 | 🟡 | ✅ | 🟡 | [th18/01 §3–§4](th18/01-stage-msg.md) |
| 文本每 2 行一页；注音语法 `\|x,字距,文本` 不占行 | 🟡 | ✅ | 🟡 | [th18/01 §5](th18/01-stage-msg.md) |
| 立绘由 ANM 中断驱动：1/2/3/7/17+n | 🟡 | ✅ | 🟡 | [th18/01 §6](th18/01-stage-msg.md) |
| 结局/staff 指令集 0..17，与关卡对话**编号重叠但语义不同** | ✅ | ✅ | 🟡 | [th16/04 §2](th16/04-ending-staff-msg-instruction-set.md)、[th18/02 §3](th18/02-ending-msg.md) |
| 结局 op13 淡入 / op14 淡出 | 🟡 | ✅ | 🟡 | [th18/02 §3](th18/02-ending-msg.md) |
| staff 按难度硬选 `staff1–4`，四文件只差末条 CG 脚本号 | ✅ | ✅ | 🟡 | [th16/04 §1.6](th16/04-ending-staff-msg-instruction-set.md)、[th18/02 §3](th18/02-ending-msg.md) |
| 结局 op4 / 0xd / 0xf–0x11 在 shipped 文件里未用 | ✅ | ✅ | — | [th16/04 §2.7](th16/04-ending-staff-msg-instruction-set.md)、[th18/02 §3](th18/02-ending-msg.md) |
| 指令集有 TH11 插位 + TH14 override 的继承链（跨版本对号易错） | ✅ | ✅ | 🟡 | [th16/02 §0,§3](th16/02-msg-vm-opcodes.md) |

> **图例**：✅ 该版本一手验过（证据列给地址/出处） · 🟡 待验（从别的版本借来的假设，或单源） · ❌ 已知不同/不存在 · ❓ 存疑 · — 未看
>
> **本页不许出现没有出处的断言。** 从某作借到另一作的判断一律 🟡，在该版本 exe 上验过才能改 ✅（[`METHOD.md`](../../METHOD.md)）。

## 与社区的关系

- **thmsg**（thtk）：格式权威；`-e` 切结局表 `th10_msg_ed_fmts`。TH19 关卡文件多 0x50 字节头、入口 flags 拆 4 字节。
- **ExpHP truth / thpages**：stage 表到 TH185；`end.ts`（2026-03 新增）有结局表但 13/14 仍 "Does. Something."、行数与淡出时长待研究——
  我们 th16/th18 两篇已补齐这些。
- **THBWiki**《脚本对照表/其他》§MSG：只有一张跨世代混写的扁平清单，无原理、无格式、无结局表。
  翻新稿见 [`WIKI-DRAFT.md`](WIKI-DRAFT.md)。

## 开放

- 输入位 `0x200` = Ctrl（跳过键）按 ZUN 惯例认定，未从按键映射一手核（[th18/01 §3](th18/01-stage-msg.md)）。
- op29 气泡类型 0–15 各对应哪个 anm 外观，只有 THBWiki 对 TH16 的描述（[th18/01 §5](th18/01-stage-msg.md)）。
- TH19/TH20 无 exe、无 th-re-data：矩阵整列 🟡，只能靠 thmsg 签名 + THBWiki 单源。
