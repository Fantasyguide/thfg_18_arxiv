# autoplay —— TH06NC「AI 代打」原型（算法内嵌，单 DLL）

> **版本**：TH06NC（Steam 2026 重制版，`th06nc.exe`，MD5 `b0cc28ec904e9efce2439ad857c938b5`，
> imagebase `0x140000000`）。本文裸地址默认属该版本。

装上就自己打：每帧读出自机 / 全部敌弹 / 全部敌人，跑一个内嵌的躲弹策略，
把「这一帧该按哪些键」写进游戏的逻辑按钮位图。**F9** 开关。

这是 [`docs/superpowers/specs/2026-09-10-ai-agent-bridge-design.md`](../../../docs/superpowers/specs/2026-09-10-ai-agent-bridge-design.md)
的**原型先行**版本：协议、网络、Python 全部推后，只留下「内存 → 中立快照 → 决策 → 输入」这条链路。
快照结构（`native/world.h`）就是将来协议编码器的输入，字段名照 spec §4.3.2 起。

> ⚠️ **未实机验证。** 开发机是 Linux，跑不了这个游戏。静态审计（`check.py` 全过）、
> 宿主单测（策略 + 抽取器）、交叉编译产物格式核验都过了，但**没人真的启动过它**。
> 首次使用请按下面「怎么确认成功」逐条看。详见 [`AUDIT.md`](AUDIT.md) §E。

## 原理

三步，全在 `Player__OnUpdate` 入口的跳板回调里跑（与 [`mouse-control`](../mouse-control/README.md) 同一个 hook 点）：

1. **抽取**（`native/extract.c`）：只读。自机坐标 / 判定半径 / 状态 / 速度，
   640 槽弹池里 `state != 0` 的弹（位置、速度、判定半径），256 槽敌池里活着的敌人，
   命 / 炸弹 / 火力 / 分数 / 擦弹、符卡进行中、可动区。坐标统一平移到**中轴原点**（x ∈ [-192,192]）。
2. **策略**（`native/policy.c`）：枚举 18 个候选（八向 + 不动 × 常速 / 低速），
   把每颗弹按 `pos + vel × t` 线性外推 24 帧、自机按候选速度走（撞可动区就停），
   取时间窗内「自机到最近弹的净距」当安全度；敌人本体按静止盒算。选最安全的，
   安全度都够时按次要目标打破平手（对齐 boss 的 x、回到 y=384、少动）。
   每个候选都躲不开且 6 帧内必中 → 用炸弹（有炸弹、且距上次 ≥ 90 帧）。一直射击。
3. **注入**（`native/hook.c`）：`INPUT_HELD` 的 7 个逻辑按钮位整体覆盖，其余位不碰。
   钳位、低速、炸弹的按下沿判定都由游戏自己做。

**玩家对象零写入**：整个 mod 只写 `INPUT_HELD` 一个 dword。

## 装法

**一个文件，拖进游戏目录（和 `th06nc.exe` 同一层）**：

```
<游戏目录>/
  th06nc.exe
  VCRUNTIME140_1.dll     <- 就是它
```

投递方式与 mouse-control 完全相同（静态导入表里的 `VCRUNTIME140_1.dll` 代理，
只转发 `__CxxFrameHandler4`），论证见 [`../mouse-control/TARGET.md`](../mouse-control/TARGET.md) §3。

> ⚠️ **不能和 mouse-control 同时装**：两个 mod 是同名文件，也都写 `INPUT_HELD`。装谁就只放谁。

**卸载**：删掉 `VCRUNTIME140_1.dll`（ini 和 log 可一并删）。

### 自己编译

```sh
cd mods/th06nc/autoplay/native
python3 check.py      # 地址对账 + 跳板反汇编 + 代理导出 + 宿主单测，全过再谈跑
bash build.sh         # 出 VCRUNTIME140_1.dll（llvm/clang 交叉编译，见全局笔记的 xwin 段）
make -C tests test    # 单独跑宿主单测
```

## 用法

| 操作 | 效果 |
| --- | --- |
| （无） | 装上即开启。进关卡后自机自己动、自己射、必要时自己炸 |
| **F9** | 开 / 关。关掉后键盘立即恢复 |
| 键盘 | 开启期间 Z / X / Shift / 方向键**全部失效**（每帧被覆盖）；关掉即恢复 |

- 只在游戏是前台时接管，alt-tab 出去不会走火。
- 出生 / 死亡动画期间不接管（那时游戏也不读方向位）。
- 菜单 / 对话 / 暂停里 `Player__OnUpdate` 不跑或自机状态不可操作，回调自然不写。

## 配置

可选。游戏目录下的 `th06nc_autoplay.ini`：

```ini
[autoplay]
enabled       = 1     ; 1 = 启动即开启（默认），0 = 默认关闭、按 F9 再开
toggle_vk     = 120   ; 开关键虚拟键码，120 = 0x78 = F9
horizon       = 24    ; 弹道外推帧数（1–120）。大 = 更早躲、更容易被远处的弹吓跑
bomb_cooldown = 90    ; 两次炸弹之间最少帧数
log_every     = 60    ; 每 N 帧记一行状态；0 = 不记
```

## 怎么确认成功

先看 `<游戏目录>/th06nc_autoplay.log`（目录只读时退到 `%TEMP%`）。
**如果一个文件都没有**，用 [DebugView](https://learn.microsoft.com/sysinternals/downloads/debugview)
（管理员运行，勾 Capture Win32）看——每行日志同时走 `OutputDebugStringA`。

日志开头应该是：

```
th06nc autoplay —— imagebase=00007FF7XXXXXXXX
[who] 代理 = VCRUNTIME140_1.dll
[who] 本 DLL  = ...\VCRUNTIME140_1.dll
[who] 游戏目录 = ...
[ini] enabled=1 toggle_vk=0x78 horizon=24 bomb_cooldown=90 log_every=60
[guard] 序言签名匹配，跳板就绪（F9 开关 autoplay）
[diag] agent_tick 首次触发
[diag] 首次生效 frame=… pos=(0.0,384.0) r=1.25 speed=4.0/2.0 bullets=0 enemies=1 area=[-184,184]x[16,432] -> held=0x01
[tick] frame=… pos=(…) st=0 b=… e=… lives=… bombs=… phase=0x41 held=0x…
```

进关卡后五条验收，**按顺序看，前一条不过后面的没意义**：

| # | 看什么 | 通过说明 | 不通过说明什么 |
| --- | --- | --- | --- |
| 1 | `[diag] 首次生效` 里 `pos=(0.0,384.0)`、`r=1.25`、`speed=4.0/2.0`（灵梦）或 `5.0/2.5`（魔理沙）、`area=[-184,184]x[16,432]` | 自机字段与可动区读对了 | 偏移错或 `g_Player` 错 |
| 2 | `[tick]` 行的 `b=` 随画面上的弹数涨落、`e=` 与画面敌人数同量级（关卡脚本本身可能占 1 个槽） | 弹池 / 敌池布局读对了 | 池基址 / 步长 / state 偏移错 |
| 3 | 自机一直在射击，且**朝 boss 的 x 对齐** | 注入路线成立、次要目标生效 | `held` 写了但游戏没读 → 时序问题，见 [`TARGET.md`](TARGET.md) §5 |
| 4 | 弹靠近时自机会让开；密集弹幕下有 `[bomb]` 行且画面真的放了炸弹 | 弹道外推 + 炸弹按下沿都成立 | 只让不炸 → 炸弹边沿被 `INPUT_PREV` 吞了，见 `TARGET.md` §5 |
| 5 | 按 F9 后键盘立即接管 | 覆盖是逐帧的、没有残留 | — |

判定半径那条（弹的 `radius = min(grazeSize)/2`）是 🟡 推定，症状是「明明看着没碰到却死了」或「贴着弹擦过去」：
调 `horizon` 没用，得回 [`engine/bullet/th06nc/01`](../../../engine/bullet/th06nc/01-pools-and-structs.md) 核 `CalcKillBoxCollision` 用的半径。

## 没反应？按这张表查

| 日志情况 | 说明 | 怎么办 |
| --- | --- | --- |
| log 文件和 DebugView 都没有输出 | 代理根本没被加载 | 文件名正好是 `VCRUNTIME140_1.dll`、和 `th06nc.exe` 同一层；没装 mouse-control 的同名文件 |
| 启动就弹 `0xc0000142` | 接不上 System32 的真身，**刻意**响亮失败 | 确认 `C:\Windows\System32\VCRUNTIME140_1.dll` 存在 |
| `[guard] …序言与预期不符，永久停用` | 不是这个 build | 换回记录在案的版本，或按 [`TARGET.md`](TARGET.md) §7 重取 |
| 只有 `[guard] …跳板就绪`，没有 `[diag]` | hook 装上了但没被触发 | 先进关卡 |
| 有 `[diag] agent_tick 首次触发` 但一直 `playerState=…本帧不接管` | 自机状态不是 0/3 | 看打出的值；若长期是 1/2 之外的值，状态枚举有误 |
| 自机抖动 / 撞墙来回 | 策略在两个等分候选间摆 | 先调大 `horizon`；仍抖就是评分函数的事，见 `policy.c` 的次要项 |

## 目录

```
autoplay/
├── README.md              # 你在这
├── TARGET.md              # ★ 死绑登记：hook 点 / 读取点 / 换版本要重取什么
├── AUDIT.md               # 对抗审计 + 实跑记录
└── native/
    ├── th06nc.h           ★ 全部死绑量集中一处
    ├── world.h            与作品无关的一帧快照（= 将来协议编码器的输入）
    ├── extract.c/.h       th06nc 内存 → 快照（只读；宿主可测）
    ├── policy.c/.h        内嵌策略（纯 C；宿主可测）
    ├── hook.c             跳板 / 每帧回调 / 守卫 / 日志 / ini
    ├── proxy_vcr.c        VCRUNTIME140_1 代理（照抄 mouse-control）
    ├── build.sh           交叉编译
    ├── check.py           机器核对（AUDIT.md 的支撑）
    └── tests/             gcc 单测：test_policy.c / test_extract.c
```

## 还没做

- **激光**：池已知（`th06nc:0x1404e12b8`，64 × `0x298`），策略里还没当障碍。第二版加。
- **道具**：不捡。
- **符卡 / 剩余时间**：不读。
- **协议 / 网络 / Python 客户端**：按 spec 推后；`world.h` 已按它的字段名起，接上时只加编码器。
