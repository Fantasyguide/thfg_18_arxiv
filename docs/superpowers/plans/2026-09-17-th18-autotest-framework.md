# TH18 自动测试框架（复用同僚的 stg-agent 契约 + autoplay）

> **版本**：TH18 v1.00a 为主；契约与代打范式来自同僚的 `feat/stg-agent-proto`（版本无关设计）。
> 目标（用户 2026-09-17）：**让"简单的事"不需要用户亲自实跑** —— 一键跑、自动判读、能区分
> 「未触发」与「失效」。取代原来"请用户打一局然后我读日志"的循环。

## 0. 现状：同僚已经有一整套（`upstream/feat/stg-agent-proto`，已 fetch 到本地）

| 资产 | 路径（该分支内）| 对我们有什么用 |
| --- | --- | --- |
| 计划书 | `docs/superpowers/plans/2026-09-11-stg-agent-proto.md` | 契约仓 + th06nc 接入的**逐任务**拆解（含 Global Constraints：帧格式/动作位/phase 位/字段类型）|
| 设计 spec | `docs/superpowers/specs/2026-09-10-ai-agent-bridge-design.md` | §4 契约 v1、§4.4 日志、§5.7 三层解耦、§6b th06nc、§8 测试 |
| 代打实现 | `mods/th06nc/autoplay/native/{hook,extract,policy,proxy_vcr}.c` + `check.py` + tests | ★ **就是"算法代打"**：观测提取 / 策略 / VCR / 自检 |
| 契约仓（外部）| `/data/sunyunbo/www/stg-agent-proto`（与 renkolab 同级，**不在本仓**）| `world.h` / `stgagent.h` / 编码器 / `.stglog` 写入器 / Python 读取器 |

**协议要点（从计划书一手抄录，够我们起步）**
- 记录：`u32 length`（小端，不含自身）+ `u8 type` + payload；type `1 HELLO / 2 OBS / 3 ACT / 4 CTRL / 5 BYE`。
- 动作位：`UP=0 DOWN=1 LEFT=2 RIGHT=3 SHOT=4 BOMB=5 SLOW=6 TIMESTOP=7 CARD_USE=8 CARD_SWITCH=9`
  ⇒ **我们要的"按 C 发动卡 118"= `CARD_USE`**；"蓄力（长按 Z）"= 按住 `SHOT`；"上线收点"= `UP` + `SLOW`。
- ACT = `u32 frame, u32 buttons, u32 flags`；`flags bit0 = PASSTHROUGH`（**人工直通**）、`bit1 = HUMAN`
  ⇒ ★ **`PASSTHROUGH` 正是"人机切换"的开关**：脚本跑简单动作，遇到要人看的再交回用户。
- phase 位：`IN_GAME PAUSED DIALOGUE SHOP BOMB_ACTIVE SPELL_ACTIVE PLAYER_CONTROLLABLE REPLAY_PLAYBACK`
  ⇒ 判"未触发 vs 失效"的关键：**动作只在 `PLAYER_CONTROLLABLE` 时才算真发出去**。
- 数值 `fx = i32 Q16.16`、`angle = u16 BAM`；坐标 x 中轴原点、y 顶边向下。

## 1. 目标形态（一条命令）

```
python3 <harness>/run_test.py --scenario z-intercept --deck all12 --seconds 60
   ① deploy（唯一发布路径）→ ② 启动游戏（带 --log 路径）→ ③ 代打按脚本动作
   ④ 收日志/dump（本次启动段）→ ⑤ 判定：必需判据 / 旁证 / 反例 / 未触发
   ⑥ 出报告（含"未触发"与"失效"的区分）+ 退出码
```

判读层**复用已有 `verify.py`**（八原语判据已就位），只补两件：**动作脚本**（什么时候按什么键）
与**"未触发"的判定**（动作发出去了但判据没中 ⇒ 失效；动作压根没发出去 ⇒ 未触发）。

## 2. 集成点（TH18 侧）

| 层 | 落点 | 备注 |
| --- | --- | --- |
| 观测 | 复用我们的断点/事件（`ce_item_frame` / `ce_enemy_death` / 站点回调）+ 已有日志 | TH18 已有 `ce_site_trail` / `integrity[stage N]` 现成观测 |
| 动作注入 | 需要"往输入位图里写"的能力 | ⚠ 现有卡代码**只读**输入（`0x4ca428` bit 0x001）；注入必须走**新的一条路**（每帧覆盖输入位）—— 这要算站点预算（当前 22/24，余量 2）|
| 策略 | `policy.c` 风格的脚本（JSON/纯 C 表）：时间轴 + phase 条件 | 借 th06nc 的 `policy.c` 结构，不重写 |
| 判读 | 现成 `native/tools/verify.py` | 加"未触发"分支 + 场景到判据的映射 |
| 发布 | 现成 `native/tools/deploy.py` | 数据文件同步已补（2026-09-17）|

## 3. 要用户/同僚定的事（阻塞项）

1. **契约仓怎么拿**：`/data/sunyunbo/www/stg-agent-proto` 是**同僚机器上的路径**，不在 renkolab 里。
   选项：① 请同僚把它 push 成独立仓/子模块（最干净）；② 我们**按 spec 本地实现最小子集**
   （只要 ACT 注入 + phase 判定 + 我们自己的日志，不需要 OBS 编码/训练侧）⇒ 不阻塞、当天能动工。
2. **动作注入要不要新站点**：TH18 侧写输入位需要一条每帧站点（预算余量只有 2）。
   替代：**走现有输入的物理层**（Windows `SendInput`/`keybd_event` 模拟按键）⇒ **0 站点**，
   且更"真"（走游戏自己的输入路径）。⚠ 代价：需要窗口焦点、时序靠 sleep；建议**先做 0 站点版**。
3. **人机切换**：确认用 `ACT.flags PASSTHROUGH` 那套语义（脚本跑，遇到要看画面的切回你）。

## 4. 建议的第一步（本轮之后的执行顺序）

1. **`mods/th18.v1.00a/card-expand/native/tools/autoplay.py`**（0 站点）：用 `SendInput` 或 `pyautogui`
   式的按键注入 + 时间轴脚本（YAML/JSON：`t=0..3s 按住 SHOT`、`t=5s 按 CARD_USE`…），
   启动器复用 `modkloader` 那条路，日志路径复用现有约定。
2. **判读合并**：`verify.py` 加 `--scenario`（把场景 → 判据 + 期望动作映射）与"未触发"分支。
3. **CI 化**：`run_test.py --scenario all` 串起八原语 + 全装（12 张）两条验收，出退出码。
4. 若要更强的（无窗口/进进程内/训练侧），再上契约仓那条路。

## 5. ★ WSL → Windows 桥（2026-09-17 实测通过）

**事实（一手）**
- 本环境是 WSL2（`/proc/version` 带 `microsoft-standard-WSL2`），**`WSLInterop` 已注册**（`interpreter /init`、enabled）；
  `cmd.exe` / `powershell.exe` **不在 PATH**，但**用绝对路径可用**：
  `/mnt/c/Windows/System32/cmd.exe`、`/mnt/c/Windows/System32/WindowsPowerShell/v1.0/powershell.exe`。
- **Windows 侧有 Python 3.11**：`C:\Users\Nipporita\AppData\Local\Programs\Python\Python311\python.exe`。

**踩过的两个坑（都已解）**
1. **引号过 `cmd /c` 会被吃**（`\"…\"` 变成字面量 ⇒ 命令名被当成整串）⇒ 改成**写一支 ASCII 的 `.bat`** 再调它，
   路径里无空格时不需要引号。
2. **中文路径过 cmd 有编码风险** ⇒ 参数**走 JSON 文件**（Python 侧按 UTF-8 读，实测 `launcher_exists`/`game_exe_exists` 都为 true）；
   脚本与 bat 放**纯 ASCII 的 Windows 目录**（`%TEMP%\renkolab\`）。

**已验证的调用链**（`--smoke` 不需要游戏）
```
/mnt/c/Windows/System32/cmd.exe /c C:\Users\Nipporita\AppData\Local\Temp\renkolab\run_autoplay.bat --smoke
  → {"ok": true, "platform": "win32", "python": "3.11.5",
     "launcher_exists": true, "game_exe_exists": true, "window": null}
```
⇒ **WSL 侧（我）能编排，Windows 侧（游戏/按键/日志/dump）真的在 Windows 上跑** —— 这正是用户要求的分工。

**分工定案**
- Windows 侧：`native/tools/autoplay_win.py`（启动游戏 / 找 `th18.exe` 窗口 / `keybd_event` 注入 / 收日志段与新 dump）。
- WSL 侧：`native/tools/autoplay.py`（场景与时间轴 → job JSON → 经上面的桥调 Windows runner → 本地 `verify.py` 判读）。
- 人只在"第一次真跑"时要看一眼（窗口标题/焦点/注入是否生效）——之后都能无人化。
