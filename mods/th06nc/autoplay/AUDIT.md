# AUDIT —— th06nc/autoplay 对抗审计

> **版本**：TH06NC（`th06nc.exe`，imagebase `0x140000000`）。裸地址默认属该版本。
>
> 姿态：默认我们写错了，去二进制找证伪证据。逐条给 **CONFIRMED / REFUTED / OPEN**。
> 机器可复核的部分落在 `native/check.py`（A–G 七节），文字部分负责说清「为什么这么查」。

**总判定：静态审计通过，✅ 实机首跑通过（2026-09-11）。** 见 §E.1。

```sh
python3 mods/th06nc/autoplay/native/check.py     # 全过
```

---

## A. ABI / 栈平衡

**整节 N/A —— mod 不调用任何引擎函数。** 与 mouse-control 同一设计选择：写入面是一个 dword
加一条 AsciiManager 记录（引擎把 `AddString` 内联了，没有函数可调，只能照它写内存），
读取面全是只读内存。没有 cave 调引擎函数，就没有调用约定 / 栈参 / `RET imm` 这一整类问题。

- [x] 调用的每个引擎函数 —— **N/A**，零个。
- [x] FPU 栈平衡 —— **N/A**，x64 走 SSE。**但**本 mod 的回调比 mouse-control 多做了大量浮点运算
      （`policy.c`），见 §B.3。

## B. 手写机器码：跳板

与 mouse-control **逐字节相同**（`check.py` C–E 节把 `hook.c` 实际写入的字节交给 capstone 反汇编）。

### B.1 栈对齐 —— CONFIRMED
6 次 push 后 rsp ≡ 8，`sub rsp,0x28` 后 ≡ 0，`call` 前对齐；`0x28 ≥ 32` 字节 shadow space。

### B.2 寄存器 —— CONFIRMED
易失的 RCX/RDX/R8/R9/R10/R11 保存 / 逆序还原；RAX 不保存（原序言第一条 `mov rax,rsp` 重置它）；
非易失寄存器跳板不碰。

### B.3 SSE 寄存器 —— CONFIRMED（推理）
`agent_tick` 是普通 C 函数，编译器按 x64 ABI 处理：XMM6–15 若用到会在其序言里保存 / 尾声还原，
XMM0–5 是易失的。跳板到达时 `Player__OnUpdate` 尚未开始执行、它的入参只有一个指针（RCX），
所以 XMM0–5 里没有活值需要保护。这与 mouse-control 相同，只是本 mod 用得更多。

### B.4 入口补丁 —— CONFIRMED
`movabs rax,stub / jmp rax / nop` 正好 13 字节 = 被覆盖的 4 条完整指令，均位置无关。

### B.5 代理转发 —— CONFIRMED
`proxy_vcr.c` 从 mouse-control 逐字复制（仅改两个函数名）；`check.py` F 节核导出表、
6 字节 `jmp [rip+disp]`、`.data` 槽初值 0、exe 导入表要的正好是这一个。

## C. 偏移与内存安全

- [x] **每个偏移都能回指一手文档** —— CONFIRMED，`TARGET.md` §5 逐条给出处，`check.py` B 节与
      四个 spec 模块对账（27 条）。
- [x] **池的算式** —— CONFIRMED：弹 `0x1403ec2a8 + i × 0x620`（`i < 640`），
      敌 `0x140aa1e98 + i × 0x10b0`（`i < 256`），与 `bullet/01` §1.1、`enemy/01` §1 的三方自洽一致。
      读取范围上界：弹 `0x1403ec2a8 + 640×0x620 = 0x1404e12a8` = `g_BulletCount`，不越界；
      敌 `0x140aa1e98 + 256×0x10b0 = 0x140badea0` < 模板 `0x140bae0c0`，不越界。
- [x] **只读** —— CONFIRMED：`extract.c` 的 `base` 是 `const uint8_t *`；全文件无写入。
- [x] **AsciiManager 写入与引擎同式** —— CONFIRMED：`check.py` B 节把 `0x14007a3a0` 里的基址 lea、
      七个字段的 disp32、numStrings 的 rip 相对访存逐条从 exe 读出来比对；`ascii_add` 只在
      `numStrings < 0x100` 时写、文本截到 63 字节 + NUL、isGui / isSelected 拷管理器当前值（与引擎同）。
      写入时序：calc 链里晚于 AsciiManager 的清零（优先级 1）、早于 draw，与引擎画 FPS 同类。
- [x] **写入点时序** —— OPEN（与 mouse-control 同）：在 `Player__OnUpdate` 入口写，正是它马上要读的那份；
      炸弹的上升沿由它自己比 `INPUT_PREV`，mouse-control 中键实机放过炸弹 → 同机制。
- [x] **静态缓冲** —— CONFIRMED：`ap_world_t`（≈23 KB）与 `ap_policy_t` 都是静态，回调内零堆分配。
- [x] **判定式与引擎同式** —— CONFIRMED（2026-09-11 MCP 直读 `0x14006a980` 与 `0x1400373b0`）：
      弹 `min(grazeSize)×0.5 + r` 圆对圆；敌人本体 `hitboxDimensions × 2/3 × 0.5` 半宽盒、圆心钳盒、
      且只对 `(enemy+0xbd & 3) == 3` 的敌人。两个常量 `0x14030cd58` = 0.5、`0x14030cd74` = 0.6667 直读。
      抽取器与策略按此改过（第一版把敌人盒取成了 `hitbox/2`，大 1.5 倍，且没看碰撞门）。
- [x] **激光判定与引擎同式** —— CONFIRMED（`0x14006aba0` + 唯一调用点 `0x140011883`，`bullet/01` §3.1）：
      旋转 −angle 进激光坐标系、盒 `[start,end] × ±width/4`、钳位比 1.25；只 state 1 判。
      `laser_margin()` 照此实现；预警态用 `+0x270 − timer` 当 `t_active`。伸长按 `end += speed`、
      `start = max(start, end − startLength)` 外推。转角未读（🟡：扫射激光按静止角度算）。
- [x] **弱锚字段的失效后果** —— 只剩 `bossId`（weak）：错 → 只影响次要目标（对齐谁的 x），不致死。

## D. 量纲 / 常识关

- [x] 自机速度 4/2（灵梦）5/2.5（魔理沙），24 帧外推 = 最多 96 px，关卡区 384 宽 —— 量级合理。
- [x] 弹半径 `min(grazeSize)/2`：TH06 弹的 `grazeSize` 常见 8–16 → 半径 4–8 px，与视觉一致；且与引擎判定式同源（§C）。
- [x] 判别式单测（`tests/test_policy.c`，13 条）：直落弹必须横躲、贴墙时不选撞墙方向、四面围死才用炸弹、
      没炸弹不按、冷却期不按、敌人本体是障碍、出生 / 死亡态全零；势场版另加：迎面弹 100 px 外已有
      横向推力而 400 px 外经时间折扣后不动、擦身弹（最近距离 > safe_r）无推力、
      弹流前 40 帧内横向方向最多换一次且不被打中；激光 5 条（水平 / 越界不算 / 竖直旋转 / 预警期 /
      伸长中）；实机反馈回归 3 条（无弹 120 帧不振荡且停在中下、底边斥力弱于侧边、横穿弹往下让不往上）。
      去抖 3 条（扇区边界 30 帧内换向 ≤ 3、掠过弹 100 帧内低速键翻转 ≤ 2、贴中轴时涡旋选边 10 帧一致）。
      共 24 条，几何取值都能区分对错。
- [x] 抽取器单测（`tests/test_extract.c`）：伪造 12 MB 镜像，每个字段放非默认值，验坐标平移、
      空槽不入表、可动区退回默认、状态 1 不可操作。

## E. 收尾

- [x] 汇编后比对反汇编 —— CONFIRMED（`check.py` C 节）。
- [x] 可一键关掉 —— CONFIRMED：C 键；或删 DLL。屏幕文字 `show_text = 0` 可关。
- [x] **游戏内实跑** —— CONFIRMED（首跑）。开发机是 Linux。验收表在 `README.md`「怎么确认成功」，
      五条按序看。跑完把结果（含崩溃与「不像在躲」的观察）回填到这里。

### E.1 实跑记录

- **2026-09-11 用户首跑**：DLL 被加载、hook 触发、自机自己跑、能躲弹。判定式修正（8ce5648）后的版本。
  未逐条核对 README 五条验收里的日志数值，未观察炸弹触发；死亡率 / 长时间稳定性未记录。
- **2026-09-11 第二次实机**（用户）：C 键开关、屏幕文字 ✅。势场版三条反馈：无弹时中轴附近振荡、
  底边斥力过强、往上冲到 boss 脸上——原因分别是惯性奖励高于起动阈值、底边与侧边同参数、涡旋选边参考点
  取了可动区中心（y=224）。三处已改并各加一条回归测试。
- **2026-09-11 第三次实机**（用户）：势场版能跑，但「移动太抽搐」。原因是连续合力落到八向：扇区边界、
  起停阈值、低速单阈值、涡旋选边在中轴翻转。加了指数平滑 + 换向滞回 / 最短保持 + 两处施密特触发 +
  选边闩锁（policy.c D10），各加回归测试；改后版本 + 激光支持**待跑**。

## F. UI / 动画复刻

N/A —— 本 mod 不画任何东西。
