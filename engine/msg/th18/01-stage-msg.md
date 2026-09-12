# 01 · TH18 关卡对话 MSG（`GuiMsgVm`）—— 一手验证
> **版本**：TH18 v1.00a（`th18.exe`，imagebase `0x400000`）。本文裸地址默认属该版本；引用其他版本须写成 `th16:0x…`。

> 证据等级：✅ 一手反编译（+ 真实文件实测 / ExpHP 交叉） · 🟡 单源或推断 · ❓ 待查。
> 对照物：TH16 的 [`../th16/01`](../th16/01-architecture-overview.md)、[`02`](../th16/02-msg-vm-opcodes.md)、
> [`03`](../th16/03-dialogue-lifecycle.md)。**凡与 TH16 一致处只写「同 TH16」+ th18 地址，不重复论证。**

## 0. 子系统地图

| 子系统 | th18 地址 | 说明 | 证据 |
| --- | --- | --- | --- |
| `Gui::operator_new` | `0x43b800` | 建 HUD；预载 `fronttr.anm`（op35 用） | ✅ |
| `Gui::initialize`（本次命名） | `0x43a730` | **按机体选 `.msg`**：`CUR_STAGE_DATA->msg_filenames[CHARACTER+SHOT]` → THA1 读 → `Gui+0x1b4` | ✅ |
| `Gui::start_dialogue`（本次命名） | `0x43e360` | ECL 518 的落点；负数 id 特殊分支（§2） | ✅ |
| `Gui::on_tick` | `0x43bb70` | 每帧驱动：`Gui+0x1b0 != 0` → `GuiMsgVm::run`；返回非 0 → 析构并置 0 | ✅ |
| `GuiMsgVm::constructor` | `0x43d720` | 0x1d8 字节；建 4 个文本 anm（`SUPERVISOR.text_anm` 脚本 0×2 行、1×2 注音，字号 0x15）；**清子弹/激光/杂兵** | ✅ |
| `GuiMsgVm::run` | `0x43e550` | 取指-派发，case 0..0x24（=36） | ✅ |
| `GuiMsgVm::destructor` | `0x43a3f0` | — | ExpHP 名 |
| ECL 派发器 | `0x430d30` | 518 = case 0x3f（`0x434dbf`），519 = case 0x40（`0x434e2b`） | ✅ |

`zGuiMsgVm`（th18，0x1d8）关键字段（ExpHP 名 + 一手印证）：

| off | 字段 | 一手语义 |
| --- | --- | --- |
| 0x18 | `time_in_script`（zTimer：+0x18 prev / **+0x1c cur** / +0x20 float） | 脚本计时；取指比较 `instr.time <= +0x1c` |
| 0x2c | `pause_timer` | op11 用 |
| 0x40 / 0x50 | `anm_id_player_face[4]` / `anm_id_enemy_face[4]` | 左侧 / 右侧立绘槽，`who` 索引 |
| 0x60 | 气泡底 anm | op7/8/9/32 给它发中断 2/3 |
| 0x64 / 0x68 | 文本行 1 / 2 | op15/16/17 写 |
| 0x6c / 0x70 | 注音行 1 / 2 | op17 的 `\|` 分支写 |
| 0x74 | intro（敌名牌）anm | op20 建，op5 中断 |
| 0x78 | 文本框 anm | op6/18 销毁 |
| 0x164 | `current_instr` | 指令指针 |
| 0x198 | ECL 放行脉冲 | op12 置 1，`run` 顶部每帧减 1，519 读 |
| 0x19c | `flags` | bit0 可跳过（op10）；bit1 字体位（op26）；bit2–5 气泡类型（op29）；bit6 玩家按过跳过键 |
| 0x1a0 / 0x1a4 | 下一行号 / 框内已有文本 | op17 翻页逻辑（§5） |
| 0x1a8 | 输入锁定倒计时 | >0 时不采样跳过键 |
| 0x1ac | `active_side` | 0 自机侧 / 1 敌方侧 / **2 无人**（op9） |
| 0x1b0 | `speaker_colors[4]` | 文本颜色按 side 取 |
| 0x1c0 | `callout_pos` | op28 写（值 ×2） |
| 0x1cc | 当前框最大文本宽 | op17 维护 |

## 1. 文件与入口表

- 文件 = `u32 N` + N × `{u32 offset, u32 flags}` + 指令流。`start_dialogue` 只读 `*(Gui+0x1b4 + 4 + id*8)`（offset）；
  结局侧 `Ending::initialize` 只读 `buf[1]`（入口 0 的 offset）。**flags 字段（thmsg 打印的 `(256)`）在这两个读取点都不读**；
  全部 44 个真实文件里恒为 256。→ 引擎无用 / 编译器残留 🟡（未穷举全部 xref）。
- 指令 `{u16 time, u8 opcode, u8 size, args}`：`run` 里 `time` 按 **ushort** 读（`(uint)*puVar17`），不是 int16
  ——修正 TH16 文档写的 `int16`（数值上无差别，真实文件 time 都远小于 0x8000）。推进 `+= size + 4`。
- 真实文件入口数：st01/02/04 = 2，st03/05/07 = 3，**st06 = 5**（§2 的空白卡分支）。

## 2. 触发：ECL 518 / 519 与负数 id

```c
// 0x430d30 case 0x3f  (ECL ins 518 dialogRead)
id = arg0;
if (CURRENT_STAGE == 6 && id == 0 && AbilityManager__card_exists(0)) id = 4;   // ★ 持有空白卡 → 6 面开场对话换成入口 4
Gui__start_dialogue(GUI_PTR, id);
BulletManager__cancel_all(); LaserManager__cancel_all(0);
/* fallthrough */ case 0x46: FUN_00430710();      // 杀掉非 boss 敌机（按 flags 掩码筛）
```

`Gui::start_dialogue(id)`（`0x43e360`）：

| id | 行为 |
| --- | --- |
| ≥ 0 | 若已有 VM 先拆；`new GuiMsgVm(0x1d8)`，脚本起点 = 入口表 `[id].offset`；存 `Gui+0x1b0`，`vm[0]=id` |
| −1 | 播 **boss BGM**（`boss_bgm_id`，`FUN_00477a50` cmd 2 槽 1）+ 音乐室解锁位 + 用关卡 logo anm（`Gui+0x160`）起脚本 **2**（曲名弹出） |
| −3 | 同上但 **道中 BGM**（`stage_bgm_id`，槽 0）+ 脚本 **1** |
| −2 | 符卡 flag bit7 清 → `GameThread__end_stage()`；否则 `FUN_00458a30()` ❓ |

-1/-3 还有一个门：练习模式（`GLOBALS+8 & 0x30 == 0x20`）且非回放时，若目标曲 `<name>.wav` 已在播（与 `DAT_0056d104` 比）则跳过。

`519 dialogWait`（case 0x40）：`Gui+0x1b0 != 0 && Gui->msg+0x198 == 0` → 不推进 ECL（阻塞）。**同 TH16**（`th16:0x4216e0`）。

## 3. 每帧：`run` 顶部（时间门控 + 跳过机制）

```c
if (vm+0x198 > 0) vm+0x198--;                                   // ECL 放行脉冲自清
if (vm+0x1a8 < 1) { if (INPUT_PRESSED & 0x201) flags |= 0x40; } else vm+0x1a8--;   // 采样"玩家要跳过"
if ((flags & 0x41) == 0x41 &&                                    // op10 允许 && 玩家按过
    ((INPUT_HELD & 0x200 && hold[9] > 19) || (INPUT_HELD & 1 && hold[0] > 19)))   // 键长按 ≥20 帧
    time.cur = instr.time;                                       // ★ 快进 = 把脚本计时直接拨到当前指令的 time
while (instr.time <= time.cur) { switch(opcode) …; instr += 4 + size; }
Timer__increment(&time);
```

- `hold[n]` = `0x4ca3a4 + n*4`（`Input::compute_edges` `0x42abc0` 维护的逐位长按计数）。bit0 = 射击；**bit9（0x200）= 跳过键（Ctrl）** 🟡（按 ZUN 输入位惯例，未从按键映射一手核）。
- 快进不是「跳到 pause 结束」，而是每帧把计时拨到下一条指令的时间，于是所有等待瞬间过完；`op11` 里同一门再判一次（§4）。
- 返回值：op0 → `-1`（拆 VM）；op36 → 直接 `return 0`（跳过尾部）；其余走尾部：把气泡位置同步到 4 个文本 anm（按 side 做 x 偏移），`return 0`。
- `Gui::on_tick` 里 `DAT_004cf2a4 != 0`（❓ 暂停/菜单态）时不跑 `run`，只清 flags bit6 并把 `+0x1a8 = 2`（解除暂停后 2 帧内不采样跳过键）。

## 4. opcode → 行为表（0..36，一手 `0x43e550`；子 agent 独立逐 case 读，不喂标签）

签名列 = ExpHP truth `MSG_10_185`（TH18 行）；`who` 索引左侧 `+0x40[]` 或右侧 `+0x50[]`。中断号见 §6。

| op | ExpHP 名 | 签名 | th18 一手行为 | 与 TH16 差异 |
| --- | --- | --- | --- | --- |
| 0 | end | — | `return -1` | 同 |
| 1 | show-player | `S` | `who=0`：anm = 自机 anm（`PLAYER+0xc`），脚本 = 表 `0x4b6620[CHARACTER]` = {40,46,29,28}；`who≠0`：同 op2 的 `boss_data[who]` 选择器；存 `+0x40[who]` | TH16 读 `_`（忽略） |
| 2 | show-enemy | `S` | `boss_data[who].msg_face_anim_index` 选 anm 源（四分支见表下「op2 anm 源选择器」），脚本 = `.msg_face_anm_script`；存 `+0x50[who]` | 同 |
| 3 | show-textbox | — | **无 case**（无 default，只推进） | 同 |
| 4 | hide-player | `S` | `interrupt(+0x40[who], 1)`，槽清零 | TH18 新增 who |
| 5 | hide-enemy | `S` | `interrupt(+0x50[who], 1)`，槽清零；并 `interrupt(+0x74 intro, 1)` | 同 |
| 6 | hide-textbox | — | 4 个文本 anm 中断 1；销毁文本框 `+0x78` | 同 |
| 7 | focus-player | `S` | 右侧 4 槽全发 3；左侧 `who` 发 2、其余 3；气泡底 `+0x60` 中断 2；`side=0`；清 flags bit1、行号、框态；4 文本 y 偏移归 0 | TH18 新增 who |
| 8 | focus-enemy | `S` | 镜像 op7；`side=1`；气泡底中断 3 | 同 |
| 9 | focus-none | — | 8 槽全发 3；**`side=2`**；气泡底中断 3 | TH16 文档写 side=0，th18 为 2 |
| 10 | skippable | `S` | `flags.bit0 = arg & 1`（读 1 字节） | 同 |
| 11 | pause | `S` | 见下 | 同（节流由 13 帧改为长按门） |
| 12 | ecl-resume | — | `+0x198 = 1` | 同 |
| 13 | face-player | `SS` | `interrupt(+0x40[arg2], arg1 + 17)` | TH18 新增 who |
| 14 | face-enemy | `SS` | `interrupt(+0x50[arg2], arg1 + 17)` | 同 |
| 15 / 16 | text-1 / text-2 | `m` | 解码串写入行 1 / 行 2 anm（`FUN_00487140`，颜色 `colors[side]`，模式 `4 + flags.bit1`），中断 2 | 同（真实文件未用） |
| 17 | text-add | `m` | §5 | 同 |
| 18 | text-clear | — | 销毁文本框 `+0x78`；4 文本 anm 中断 3 | 同 |
| 19 | music-boss | — | `FUN_004546a0(1, boss_bgm_id)`（播 + 音乐室解锁）；logo anm 脚本 2（曲名）；`TIME_IN_MUSIC = 0` | 同 |
| 20 | intro | `S` | anm = `FUN_00438d40(boss_data[who].msg_ename_anim_index)`，脚本 = `.msg_ename_anm_script` → `+0x74`；`FUN_004422c0(GUI)` | 同 |
| 21 | stage-end | — | `GameThread__end_stage()` | 同 |
| 22 | music-fade | — | 淡出秒数 = **8.0**（6 面 = **2.0**）÷ 游戏速度 → `FUN_00477a50(cmd 5)` | 常量 |
| 23 | shake-player | — | `interrupt(+0x40[0], 7)`、`interrupt(+0x40[1], 7)`（**固定槽 0/1，不读参数**） | truth 标 `S`；实际不读 |
| 24 | shake-enemy | — | `interrupt(+0x50[0], 7)`、`interrupt(+0x50[1], 7)` | 同 |
| 25 | y-offset | `S` | 4 个文本 anm 的 `pos_2.y = (float)arg` | 同 |
| 26 | modern-26 | — | `flags |= 2`（op15/16/17 把它当文本模式位传给 `FUN_00487140`） | 同 |
| 27 | music-fade-custom | `f` | **读 `*(float*)(instr+4)`** 秒数 ÷ 游戏速度 → cmd 5 | TH16 🟡 → th18 ✅ |
| 28 | callout-pos | `ff` | `+0x1c0 = 2x`，`+0x1c4 = 2y` | 同 |
| 29 | callout-type | `S` | `flags.bit2–5 = arg & 0xf` | 同 |
| 30 | route-select | — | **无 case** | 同 |
| 31 | show-enemy-1 | `S` | 同 op2 但 `who` 固定 1 → `+0x54` | 同 |
| 32 | modern-32 | `S` | `side = arg`；气泡底中断 3；清 bit1/行号/框态 | 同 |
| 33 | darken-portrait | `SS` | `interrupt(arg1==0 ? +0x40[arg2] : +0x50[arg2], 3)` | 同 |
| 34 | highlight-portrait | `SS` | 同上，中断 2 | 同 |
| 35 | lights-out | — | 用 `fronttr.anm`（`Gui` 预载）起脚本 0x30，id 存 `Gui+0x100` | 同 |
| 36 | **store** | — | `GAME_THREAD+0xb0 \|= 0x20000`；推进；**直接 `return 0`** | TH18 新增 |

**op2 anm 源选择器**（一手，2026-09-12 复核消 ❓）：`msg_face_anim_index` 四分支 —— `0` = `bullet.anm`（`th18:0x4cf2bc` = BulletManager，`+0x7a41ec`）；`1` = `effect.anm`（`th18:0x4cf2c8` = EffectManager，`+0xc`，载入点 `th18:0x42acf7`）；`7` = **当前关的 `st0Xwl.anm`**（`th18:0x4cf2b4` = Stage，`+0x3464`，文件名取自 `st0X.std` 偏移 `+0x10`，载入点 `th18:0x41b907`）；**其余** = `ENEMY_MANAGER+0x168+idx*4` = 本关 ECL 头 `anim{}` 的第 `idx-2` 条（`2` → `enemy.anm`、`3` → `st0Xenm.anm`；列表装载器 `th18:0x42da90`）。op1 的 `who≠0` 分支共用同一选择器。全推导见 [`../../anm/th18/04-anm-slot-selection.md`](../../anm/th18/04-anm-slot-selection.md)。

**op11 pause 细节**（一手）：首次进入把 `pause_timer` 装成 `arg`；每帧 `Timer__decrement`。
放行条件：`INPUT_PRESSED & 0x80001`（射击键或 bit19 ❓）、或计时归 0、或 §3 的长按快进门。
未放行则 `goto` 尾部——**跳过 `Timer__increment`，脚本计时冻结**，指令指针停在 pause 上。
放行时重置 pause_timer，并把 `+0x1a0 = +0x1a4 = 0`（下一条文本从新框开始）；手动放行还播 SE 0（`FUN_00476be0(0)`）。

## 5. op17 text-add：翻页与注音

```
+0x1a4 == 0（框里还没字）→ 4 个文本 anm 先写 "  " 清空并中断 3；+0x1a4 = 1；+0x1cc = 0
串首 != '|' →
    宽度 = ((strlen*8) & ~0xf − 28) × 2，与 +0x1cc 取最大（气泡宽度依据）
    FUN_004416d0(this, type*3 + side)          // 行 1；行 2 用 (type+8)*3 + side   ← 气泡样式 anm 脚本号
    写入 +0x64（行 1）或 +0x68（行 2），颜色 colors[side]；side==0 只摆行 1 位置，side 1/2 摆 4 个
    行 1：+0x1a0 = 1；行 2：+0x1a0 = 0，+0x1a4 = 0     ← 每 2 行一页；第 3 条 text-add 开新框
串首 == '|' → "|x,spacing,text"：atoi(x)、找 ','、atoi(spacing)、找 ','、余下为注音文本
    写入 +0x6c（行 1 的注音）或 +0x70（行 2 的注音），颜色 0xa0a0a0，模式 2；不占行号
```

- `type*3 + side`：op29 的类型（0–15）× 3 个 side（自机 / 敌方 / 无人）→ 气泡样式脚本索引。THBWiki 记的
  「0 圆角矩形 / 1 尖刺 / 2 思考云 / 3 说话云 / 4–7 附身红框」是这套索引在 TH16 的外观，th18 未逐个看 anm 🟡。
- `|x,spacing,` 与 ExpHP `end.ts` 注（TH20 的 `|xoffset,spacing,`）一致；THBWiki 的「起始位置,字距」也对。

## 6. 立绘 ANM 中断协议（本篇最有用的一张表）

MSG 不画立绘，它只给立绘 anm 发 **interrupt**。协议（一手，op4/5/7/8/13/14/23/24/33/34 汇总）：

| 中断号 | 含义 | 发送者 |
| --- | --- | --- |
| 1 | 隐藏 | op4 / op5 / op6 |
| 2 | 高亮（正在说话） | op7 / op8（指定 who）、op34 |
| 3 | 变暗（未说话） | op7 / op8（其他人）、op9（全体）、op33 |
| 7 | 抖动 | op23 / op24 |
| 17 + n | 表情 n | op13 / op14 |

→ 「表情号 = anm 里第 n 个表情脚本」这一社区说法在机制上是：**表情 n = 该立绘 anm 的中断标签 17+n**。

## 7. 真实文件实测（28 个 `st0Xy.msg`）

- 用到的 opcode：0 1 2 4 5 6 7 8 9 10 11 12 13 14 17 18 19 20 21 22 28 29 36；
  **未用**：3 15 16 23 24 25 26 27 30 31 32 33 34 35。
- 参数长度全部与签名吻合（1/2/4/5/10/20/29 = 4，13/14/28 = 8，17 = 8..60，其余 0）。
- 36 `store` 出现 24 次 = 每个机体每关一次（1–6 面）。st06 五个入口中第 4 个是空白卡专用开场（§2）。
- 解码样例（st01a 入口 0）：`10;1 → 29;0 → 28;112,240 → 17;"よかった" → 17;"雨が止んで" → 11;600 → …`
  与 THBWiki 原页那段 dump 的形态一致。

## 8. 与社区表的出入

- ExpHP truth 给 TH18 op23 签名 `S`（继承自 TH14 之前的表？），**th18 代码不读参数**，且真实文件里 23 未出现。
- ExpHP thpages `store` 描述「脚本结束后开商店」：一手只见置位 `GAME_THREAD+0xb0 |= 0x20000`；谁消费这一位未跟（🟡，卡牌侧文档 `engine/card/th18/05` 可能已覆盖）。
- THBWiki 原页：`9`「增援立绘」错（= 无人说话，side=2）；`25`「增援表情」错（= 文本 y 偏移）；`18`「???」= 收起文本框；
  `30`「???」= TH128 残留、无 case；`1(int a) a=0 自机登场` 对，且 a≠0 时是左侧其他角色；`3` 现代作无效。

## 证据指针

`Gui__start_dialogue` `0x43e360` · `Gui__initialize` `0x43a730` · `Gui__on_tick` `0x43bb70`（`GuiMsgVm__run` 调用点与析构）·
`GuiMsgVm__constructor` `0x43d720` · `GuiMsgVm__run` `0x43e550`（子 agent 全文逐 case，2089 行反编译）·
ECL 派发 `0x430d30`（518 `0x434dbf` / 519 `0x434e2b`；`FUN_00407d60` 读 `Gui+0x1b0`，`FUN_0042d5b0` 读 `msg+0x198`）·
`Input__compute_edges` `0x42abc0`（长按计数表 `0x4ca3a4`）· 常量 `0x4b9200`=2.0、`0x4b9268`=8.0、`0x4b6620`={40,46,29,28} ·
结构 `zGuiMsgVm` / `zTableStageData` / `zTableStageBossData`（ExpHP th-re-data th18）。

> **本文 §4 的 op2「anm 源选择器」结论来源**：[`../../anm/th18/04-anm-slot-selection.md`](../../anm/th18/04-anm-slot-selection.md)
> （2026-09-12，消掉本条原来的两个 ❓）。该文同时给出 `anmSelect(N)`/`anmSetSprite(K,S)` 的完整语义，
> 以及 `th18:0x438d40` 这条选择器在 exe 里另外至少 13 处同构副本（含本文件 op2 用的那 3 处 `th18:0x43f857`/`th18:0x43f965`/`th18:0x43fa72`）。
