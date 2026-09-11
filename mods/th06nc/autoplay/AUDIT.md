# AUDIT —— th06nc/autoplay 对抗审计

> **版本**：TH06NC（`th06nc.exe`，imagebase `0x140000000`）。裸地址默认属该版本。
>
> 姿态：默认我们写错了，去二进制找证伪证据。逐条给 **CONFIRMED / REFUTED / OPEN**。
> 机器可复核的部分落在 `native/check.py`（A–G 七节），文字部分负责说清「为什么这么查」。

**总判定：静态审计通过，⏳ 实机未验证。** 见 §E。

```sh
python3 mods/th06nc/autoplay/native/check.py     # 全过
```

---

## A. ABI / 栈平衡

**整节 N/A —— mod 不调用任何引擎函数。** 与 mouse-control 同一设计选择：写入面是一个 dword，
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
- [x] **写入点时序** —— OPEN（与 mouse-control 同）：在 `Player__OnUpdate` 入口写，正是它马上要读的那份；
      炸弹的上升沿由它自己比 `INPUT_PREV`，mouse-control 中键实机放过炸弹 → 同机制。
- [x] **静态缓冲** —— CONFIRMED：`ap_world_t`（≈23 KB）与 `ap_policy_t` 都是静态，回调内零堆分配。
- [x] **弱锚字段的失效后果** —— 评估过：`bossId`（weak）错 → 只影响次要目标（对齐谁的 x），不致死；
      `hitboxDimensions`（weak）错 → 敌人本体被当成过大 / 过小的障碍，前者过于谨慎、后者可能撞敌。

## D. 量纲 / 常识关

- [x] 自机速度 4/2（灵梦）5/2.5（魔理沙），24 帧外推 = 最多 96 px，关卡区 384 宽 —— 量级合理。
- [x] 弹半径 `min(grazeSize)/2`：TH06 弹的 `grazeSize` 常见 8–16 → 半径 4–8 px，与视觉一致。
- [x] 判别式单测（`tests/test_policy.c`）：直落弹必须横躲、贴墙时不选撞墙方向、四面围死才用炸弹、
      没炸弹不按、冷却期不按、敌人本体是障碍、出生 / 死亡态全零。几何取值都能区分对错。
- [x] 抽取器单测（`tests/test_extract.c`）：伪造 12 MB 镜像，每个字段放非默认值，验坐标平移、
      空槽不入表、可动区退回默认、状态 1 不可操作。

## E. 收尾

- [x] 汇编后比对反汇编 —— CONFIRMED（`check.py` C 节）。
- [x] 可一键关掉 —— CONFIRMED：F9；或删 DLL。
- [ ] **游戏内实跑** —— **OPEN**。开发机是 Linux。验收表在 `README.md`「怎么确认成功」，
      五条按序看。跑完把结果（含崩溃与「不像在躲」的观察）回填到这里。

### E.1 实跑记录

（待填）

## F. UI / 动画复刻

N/A —— 本 mod 不画任何东西。
