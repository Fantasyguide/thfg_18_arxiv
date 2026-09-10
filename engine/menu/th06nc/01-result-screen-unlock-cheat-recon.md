# TH06NC 结果页“全开档”隐藏输入：调查方向

状态：**已结案，本文保留为调查过程记录**。结论见 [`02-classiclager-unlock-cheat.md`](02-classiclager-unlock-cheat.md)：秘籍存在，口令是 `classiclager`（另有一条 XInput 手柄序列），挂在 **Lunatic** 成绩页（state 6）。

> **本文的推进方向有一处判断失误**：它按 TH06 的先验只把 Extra 页（case 7）当第一候选，而 NC 把秘籍挪到了 case 6。下面「NC 静态观察」表里关于 case 7 的观察本身没错——
> case 7 确实直接落入共享成绩处理——错的是把它当成了整条线索的终点。

## TH06 社区基线（已找到）

`local/vendor/th06-re/src/th06_102h.exe/ResultScreen/OnUpdate.c` 给出完整逻辑：进入 `RESULT_SCREEN_STATE_BEST_SCORES_EXTRA` 后，只有 `FOCUS` 或 `SKIP` 按住时才推进序列；按下沿依次要求 `HOME` 5 次、`Q` 2 次、`S` 3 次。错误的其它按键会把 step 清零。完成后遍历 4 个角色/射击类型 × 5 个难度，把 `difficulty_cleared_with_retries` 和 `...without_retries` 都写成 `99`，清零 step，播放 `SOUND_1UP`。

相关社区定义：`Controller.hpp` 中 Q=`1<<9`、S=`1<<10`、HOME=`1<<11`；`TH_BUTTON_WRONG_CHEATCODE` 用于错误输入检测。这个“全开”首先改变内存中的 `clrd` 表，随后由成绩/存档流程写入 `score.dat`，所以不能只搜索音效引用。

## NC 静态观察

| 目标 | NC 位置 | 现状 |
| --- | --- | --- |
| 输入采集 | `FUN_140012be0`，经 `FUN_1400127d0` 生成当前逻辑位图 | 已确认每帧保存 current/previous 的输入全局；键位映射与 TH06 位号不应直接假定相同 |
| 结果页 tick | `FUN_14006e3a0` | 状态 switch 含成绩页 case 3–7；case 7（Extra）当前静态上直接落入共享成绩处理，未见 TH06 的十步 cheat 分支 |
| 结果页初始化 | `FUN_140073d50` | 加载 `result*.anm`、初始化成绩页对象 |
| 存档写入/清理 | `FUN_140074470` | 生成并写入 `score.dat`，包含 `HSCR/CLRD/CATK/PSCR` 记录 |
| 状态/调度 | `FUN_140079c70` | 创建并驱动结果页 chain；适合确认状态号和生命周期 |

当前证据更支持“原版 handler 没有按原形保留，或被重写到别处”，而不是“序列一定可用”。NC 输入位图中 Shift、Ctrl、Home、Q、S 的映射也不是 TH06 的固定 `0x400/0x800` 组合，必须以 `FUN_140012be0` 的实际按键映射为准。

## 不预设结果页：反向追踪策略

结果页只是 TH06 参照下的第一候选点，不是 NC 的前提。若秘籍被移动到标题画面、主菜单、Extra 选择页或独立状态机，应从“效果”向输入条件反查：

1. **效果锚点全局搜索**：枚举所有写入 `clrd`/通关标志、写入 `99` 或异常常量、播放 `SOUND_1UP`、改变 Extra/角色/Stage/符卡练习解锁状态的代码，再追其调用者和所属页面。
2. **输入模式全局搜索**：寻找小整数 step/counter 的递增、错误输入清零、连续按下沿比较、达到阈值后执行动作等模式；不要只搜索字符串 `unlock` 或只看结果页。
3. **动态写入监控**：在标题、主菜单、成绩页、Extra 选择和符卡练习等状态间，对解锁表和相关全局做 watchpoint，记录写入者、页面 state、current/previous input bitmap。
4. **存档差分确认**：备份 `score.dat`，比较触发前后 `CLRD/PSCR/CATK` 记录；存档分析用于确认效果和持久化，不单独用来猜按键序列。

若全局效果搜索只找到正常通关路径，且动态监控没有独立的输入状态机，这是“NC 可能移除了该秘籍”的有效否定证据；在此之前不能因为结果页没有原版代码就断言秘籍不存在。

## 建议的前置工作

1. **锁定样本**：记录 exe 哈希、Steam 构建日期/环境；运行前备份 `score.dat`。
2. **恢复输入符号**：在 Ghidra 中给 `FUN_140012be0` 的键盘状态缓存、current/previous 位图和按下沿比较命名；建立 VK/scan-code → NC 逻辑位表。
3. **恢复结果页状态枚举**：确认 `FUN_14006e3a0` 的 case 7 是否确为 Extra 成绩页，并标出 `clrd` 表基址、维度、写回调用者。
4. **全局搜索行为锚点**：搜索 `0x63`（99）、`SOUND_1UP` 对应音效索引、连续的 4×5 byte 写入、Extra/Stage/Spell Practice 解锁标志和 `score.dat` 写入调用；不要只搜字符串 “unlock”，也不要把结果页当成唯一候选。
5. **对齐 TH06 CFG**：待用户提供匹配的 TH06 1.02h exe 后，导入社区 XML/符号，比较 ResultScreen/OnUpdate 的 CFG 形状，而不是迁移地址。
6. **动态验证**：在隔离存档副本上逐帧记录当前/上一帧输入、结果页 state 和 `clrd` bytes；先测试单个按键错误是否 reset，再测试完整序列。成功标准是表变化、1UP 音效和重启后持久化三者同时成立。

## 参考与限制

- 社区源码：`local/vendor/th06-re`，commit `1e8fbba263ab384600af660e85331774a7087389`；TH06 反编译语义仅作参考。
- NC HTML：本地 `local/th06nc/state/f/14006e3a0.html`、`140012be0.html`、`140074470.html`；HTML 不入库，需按样本重新导出。
- 目前没有 NC 动态运行记录，因此“移除/改写”仍是 🟡，不能据此制作永久 patch 或声称存在 NC 对应秘籍。
