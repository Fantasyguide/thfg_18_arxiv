# TH06NC 勘探地图

这是手工维护的方向清单，不是“已实现功能”列表。地址默认属于 TH06NC；跨版本引用必须带版本名。

| 子系统 | 目前知道什么 | 下一步 |
| --- | --- | --- |
| ECL | 运行时与 TH06 结构高度相似；主分派在 `0x1400221b1`，敌机/时间线入口已命名 | 逐 case 对齐；确认 opcode 201 的弹型语义；补齐加载器边界与敌机结构 |
| ANM | ✅ VM `0x1400069b0` 已定位；opcode 0–35，新增 32/33/34/35 = 锚点枚举 | 收口「为什么加」那条 🟡（找精灵矩形写回点）；0–31 尚未逐条一手复核 |
| STD | ✅ 解释器 `0x140077bb0`、跳表 `0x14007826c`、opcode 0–5 全反；**NC 独有**：op3 的 `-1`/`-2` = 背景循环 | 拆静态几何（objects/faces/quad）；实机验证 Extra 背景真的循环 |
| SHT | ⚠️ 「没有入口」是**过期说法**：`g_CharData` `0x1403de3b0`、四张 `CharacterPowerData` 表、条目布局都在 `th06nc_player_spec.py` 里 | 把四张表 dump 成 `engine/sht/th06nc/01`（纯读，不用开 Ghidra） |
| 菜单/解锁 | ✅ 已结案：`classiclager` + XInput 序列，挂 Lunatic 成绩页（state 6） | 实机验证三条验收标准；再全局搜一遍还有没有第三条入口 |
| PKGL/DAT | `thdat-nc` 可列出/解包/创建；官方 seed 来源未知 | 记录样本 seed 与输出校验，不把猜测写入格式规范 |

## 已结案：解锁秘籍

TH06 的 `HOME×5 → Q×2 → S×3`（按住 `FOCUS` 或 `SKIP`）在 NC 里**不存在**——NC 的逻辑按钮位图
根本没有 `HOME` 位。NC 换成了两条挂在 **Lunatic** 成绩页的新序列：敲 `classiclager`，
或手柄 L3+R3 → Y → X → LB+RB。完整证据链见
[`engine/menu/th06nc/02-classiclager-unlock-cheat.md`](../../engine/menu/th06nc/02-classiclager-unlock-cheat.md)。

留下的尾巴：实机未跑；`0x140509694` 语义存疑；没有穷尽搜索过是否还有第三条入口。

## 接手时不要重做

1. 先核对上面的 exe 哈希和 `local/th06nc/state` 是否为同一份导出。
2. ECL 先读版本目录文档；不要把现代 TH16 的 300+ opcode 模型套到 TH06NC。
3. 菜单解锁已结案，别再从零重查；要动手先读 `engine/menu/th06nc/02-classiclager-unlock-cheat.md` 的 §7 未验清单。
