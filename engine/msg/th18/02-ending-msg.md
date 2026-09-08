# 02 · TH18 结局 / Staff-roll MSG（`EndingChild`）—— 一手验证
> **版本**：TH18 v1.00a（`th18.exe`，imagebase `0x400000`）。本文裸地址默认属该版本；引用其他版本须写成 `th16:0x…`。

> 对照物：TH16 的 [`../th16/04`](../th16/04-ending-staff-msg-instruction-set.md)。指令集**逐 case 与 TH16 相同**
> （case 0/3/4/5/6/7/8/9/0xa/0xb/0xc/0xd/0xe/0xf/0x10/0x11，1/2 无 case），本篇只记 th18 地址、常量、
> 新钉死的 🟡 和真实文件实测。三方交叉：一手 × 16 个真实文件 × ExpHP thpages `end.ts`（2026-03 新增）/ thmsg `th10_msg_ed_fmts`。

## 1. 场景与加载

| 函数 | 地址 | 作用 |
| --- | --- | --- |
| `Ending::initialize` | `0x42b650` | 选结局文件、写存档解锁位、建子 VM |
| `Ending::on_tick` | `0x42bb30` | 每帧跑子 VM；返回 **6** 时本帧连跑 6 次（staff 快进） |
| `EndingChild::run_msg`（本次命名） | `0x42be70` | 取指-派发（`child+0x54` 指令指针，格式同 stage） |
| `EndingChild` 构造 | `0x42bc60` | `child = new(0xf0)`，起点 = `buf + buf[1]`（入口 0 offset） |
| staff 换载 | `0x42b5e0` | 按名读文件 |
| 文件名表 | `0x4b3fa0` | `e01.msg`…`e12.msg` 12 项；紧接 `staff1..4.msg`（`0x4b3fd0`） |

**结局编号**（`GameThread::end_stage` `0x444650` 写 `DAT_004ccde8`）：
```
未用 continue：idx = CHARACTER*2 + (持有 id 0「空白卡」? 0 : 1)     // e01..e08
用过 continue：idx = CHARACTER + 8                                  // e09..e12
```
（`CHARACTER = DAT_004cccf4`，`CONTINUES = DAT_004ccd04`，按用法命名 🟡；空白卡 = `engine/card/th18/08` 的 id 0。）
`Ending::initialize` 里 `ENDING_ID`（`0x4c5f88`）≥ 0 表示从菜单重看，此时 `flags |= 4`。

**`Ending+0x1c` 标志位**（一手）：

| bit | 置位条件 | 效果 |
| --- | --- | --- |
| 1 | 存档里该结局从未看过 | **wait 期间禁止快进**（op5/6 `return 0`） |
| 2 | 存档里从未通关（`+0x5f4da == 0`） | staff roll 禁止 6 倍速 |
| 4 | 从菜单重看 | op12 直接结束，**不接 staff roll** |

## 2. 每帧与等待

- 时间门控同 stage：`child+0x1c < instr.time` → 等，计时 +1（乘游戏速度）。
- **op5 wait / op6 wait-clear**：装载 `arg`（负数 → 999）；放行 = `MENU_INPUT & 0x80001`、或超时、
  或（非首次观看 且（bit9 键按住 或 射击键长按 ≥20 帧）且 `剩余 % 6 == 0`）→ 即快进时每 6 帧翻一次。
  手动放行播 SE 0。op6 额外把行号 `+0x78 = 0`、`DAT_00507648 = 0` ❓。
- **staff 6 倍速**：`Ending::on_tick` 在 child 处于 staff 态（`+0x74 & 2`）、非「从未通关」、bit9/射击长按时返回 6。

## 3. opcode 表（th18 一手；命名对齐 ExpHP `end.ts`）

| op | ExpHP end.ts 名 | 我们 TH16 名 | th18 一手 | 16 文件实测 |
| --- | --- | --- | --- | --- |
| 0 | delete | end | `return -1` | ✅ |
| 3 | text-add | text_line | 5 行滚动窗：行号 0 时先把 5 槽写 `" "` 并中断 3，再写；行号 ++，>4 归 0 | ✅ 402 次 |
| 4 | text-clear | text_clear | 5 槽中断 3 | 未用 |
| 5 | wait | wait | §2 | ✅ |
| 6 | wait-clear | wait_page | §2 | ✅ |
| 7 | anm-source-load | load_anm_present | `{idx, 文件名@+8}`：`+0x74 \|= 4`（加载中，VM 每帧 `return 0`），起线程 `0x42cb10` 载 anm 到源槽 | ✅ `{0,"e01.anm"}` |
| 8 | anm-set-slot | show_image | `{slot, src, script}`：卸旧 → 从源 `+0x80[src]` 建 anm 到 `+0x90[slot]` | ✅ |
| 9 | text-color | set_text_color | `+0x7c = arg` | ✅ |
| 10 | music | music_by_name | 载 `<name>.wav` 并播；名 == `"bgm/th18_15"` → 音乐室解锁位 `+0x5f4ed`，否则 `+0x5f4ee` | ✅ 结局 `th18_15` / staff `th18_16` |
| 11 | music-fade | music_fade | 淡出 **3.0 s**（`0x4b9228`）÷ 游戏速度；清 `+0x74 & 1` | ✅ |
| 12 | switch-to-staff | goto_staff_roll | 参数串被忽略；`flags & 4` → 结束；否则按 `DIFFICULTY` 1/2/3 → `staff2/3/4`，其余 `staff1`；memset 重置、`+0x74 \|= 2`、颜色 `0xffffff` | ✅ e01–e08 有，**e09–e12 无** |
| 13 | screen-effect-a | screen_effect_fadein | `ScreenEffect` mode **0**：on_tick `0x475480`，黑幕 alpha `255 − t·255/dur` → **淡入** | 未用 |
| 14 | screen-effect-b | screen_effect_fadeout | mode **5**：on_tick `0x475820`，alpha `t·255/dur`，到时后钉 255 → **淡出**；暂停时不推进 | ✅ 结尾 60 |
| 15/16/17 | anm-set-slot-normal/hard/lunatic | show_image_d1/2/3 | `DIFFICULTY == 1/2/3` 时跳进 op8 函数体 | 未用 |

- ExpHP `end.ts` 的 13/14 仍写 "Does. Something."：本篇补了方向（TH16 04 §2.7 的 🟡 至此升 ✅）。
- ExpHP 说 TH18 的四个 staff 文件相同：**不对**，`staff1–4.msg` 只差 `0x10d` 一个字节（末条 `show_image` 的脚本号
  16/17/18/19 → 难度专属结尾 CG），与 TH16 手法一样。
- 用过 continue 的结局（e09–e12，33 条指令）没有 op12 → 败北结局不接 staff roll。

## 证据指针

`Ending__initialize` `0x42b650`（文件表 `0x4b3fa0`、标志位）· `Ending__on_tick` `0x42bb30`（返回 6 的条件）·
`EndingChild__run_msg` `0x42be70`（全部 case）· `GameThread__end_stage` `0x444650`（`DAT_004ccde8` 公式 `0x444967`）·
`ScreenEffect__on_tick_mode0_fade_in` `0x475480` / `ScreenEffect__on_tick_mode5_fade_out` `0x475820`（本次建函数并命名）·
常量 `0x4b9228`=3.0、`0x4b6480`="bgm/th18_15" · 样本 `local/th18.v1.00a/dat/e01–e12, staff1–4`（`md5` 四个 staff 各异，`cmp` 仅 1 字节）。
