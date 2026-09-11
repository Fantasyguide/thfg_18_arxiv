# autoplay —— TH06NC「AI 代打」原型（算法内嵌，单 DLL）

> **版本**：TH06NC（Steam 2026 重制版，`th06nc.exe`，MD5 `b0cc28ec904e9efce2439ad857c938b5`，
> imagebase `0x140000000`）。本文裸地址默认属该版本。

装上就自己打：每帧读出自机 / 全部敌弹 / 全部敌人 / 激光，跑一个内嵌的躲弹策略，
把「这一帧该按哪些键」写进游戏的逻辑按钮位图。**C 键**在三种模式间轮换：

| 模式 | 屏幕文字 | 行为 |
| --- | --- | --- |
| MANUAL | `MANUAL MODE`（灰） | 不插手，键盘照常。**默认** |
| AUTO | `AUTO MODE`（黄） | AI 接管；**按住鼠标左键**时自机被引导朝光标走，松开即回默认行为。躲弹始终优先于引导 |
| BYPASS | `BYPASS MODE`（红） | AI 完全接管 |

模式名下面一行小字 `(press c to switch)`。

这是 [`docs/superpowers/specs/2026-09-10-ai-agent-bridge-design.md`](../../../docs/superpowers/specs/2026-09-10-ai-agent-bridge-design.md)
的**原型先行**版本：协议、网络、Python 全部推后，只留下「内存 → 中立快照 → 决策 → 输入」这条链路。
快照结构（`native/world.h`）就是将来协议编码器的输入，字段名照 spec §4.3.2 起。

> ✅ **2026-09-11 实机首跑通过**（采样版策略）：能跑、能躲弹。细节见 [`AUDIT.md`](AUDIT.md) §E.1。
> ⏳ 之后换成的**势场版策略**、C 键开关、屏幕文字三项**尚未实机**。

## 原理

三步，全在 `Player__OnUpdate` 入口的跳板回调里跑（与 [`mouse-control`](../mouse-control/README.md) 同一个 hook 点）：

1. **抽取**（`native/extract.c`）：只读。自机坐标 / 判定半径 / 状态 / 速度，
   640 槽弹池里 `state != 0` 的弹（位置、速度、致死半径），256 槽敌池里活着的敌人（含「引擎会不会拿它撞自机」的门），
   64 条激光里预警中 / 有效的（起点、角度、沿线起止、半高、伸长速度、还有几帧开始杀），
   命 / 炸弹 / 火力 / 分数 / 擦弹、符卡进行中、可动区。坐标统一平移到**中轴原点**（x ∈ [-192,192]）。
2. **策略**（`native/policy.c`）：**速度势场（软层）+ 一帧速度障碍掩码（硬层）**。
   每颗弹按 `pos + vel × t` 取与自机的最近接近点，净距小于 `safe_r` 的才产生斥力，
   大小 `exp(−t*/τ) × ((safe_r − d*)/safe_r)²`，再加一个朝可动区中心一侧的切向分量让自机顺着弹流滑；
   敌人本体按致死盒的钳位距离算；激光按旋转矩形的钳位距离算、预警线按「还有几帧开始杀」折扣；
   墙有软斥力（底边单独减弱）；引力很弱（x 对齐 boss、y 回 home_y=384，不拉向 boss 的 y），只在安静 30 帧后起作用。
   合力先指数平滑，再与八向点积取最大——换向要新方向高出 25% 且换后至少保持 4 帧，起停和低速键都是
   施密特触发（连续向量落到八向上的抖动全靠这些滞回压住）；硬层用 3 帧线性外推禁掉「马上会撞」的方向，九向全禁才用炸弹。
   为什么这么选、每一条的出处，见下面「策略：为什么是势场 + VO 掩码」。一直射击。
3. **注入**（`native/hook.c`）：`INPUT_HELD` 的 7 个逻辑按钮位整体覆盖，其余位不碰。
   钳位、低速、炸弹的按下沿判定都由游戏自己做。
4. **屏幕文字**：每帧往 `g_AsciiManager` 的字符串表追加一条记录，写法照抄引擎画 FPS 的那段
   内联 `AddString`（[`engine/anm/th06nc/03`](../../../engine/anm/th06nc/03-ascii-manager-strings.md)）。

**玩家对象零写入**：写游戏内存的只有 `INPUT_HELD` 一个 dword 和 AsciiManager 的一条字符串记录。

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
| （无） | 装上即 MANUAL，按 C 切到 AUTO / BYPASS 后自机自己动、自己射、必要时自己炸 |
| **C** | 轮换 MANUAL → AUTO → BYPASS → MANUAL（NC 本身不用 C 键） |
| **鼠标左键**（AUTO） | 按住：自机朝光标走（光标钳到可动区内）；有弹威胁时躲弹优先。松开：回到默认（对齐 boss、待在中下） |
| （屏幕） | 关卡区左上角一行模式名，走游戏自己的 AsciiManager，与 FPS 显示同一条路 |
| 键盘 | AUTO / BYPASS 下 Z / X / Shift / 方向键**全部失效**（每帧被覆盖）；MANUAL 即恢复 |

- 只在游戏是前台时接管，alt-tab 出去不会走火。
- 出生 / 死亡动画期间不接管（那时游戏也不读方向位）。
- 菜单 / 对话 / 暂停里 `Player__OnUpdate` 不跑或自机状态不可操作，回调自然不写。

## 配置

可选。游戏目录下的 `th06nc_autoplay.ini`：

```ini
[autoplay]
start_mode    = 0     ; 启动模式：0 MANUAL（默认）/ 1 AUTO / 2 BYPASS
toggle_vk     = 67    ; 开关键虚拟键码，67 = 0x43 = C；想用 F9 填 120
horizon       = 60    ; 最近接近点的搜索窗（帧）
tau           = 20    ; 时间折扣常数（帧）：t* 帧后才到的弹，权重 exp(−t*/tau)
safe_r        = 48    ; 最近净距超过它的弹不产生斥力（px）
vortex_pct    = 80    ; 切向分量 / 法向分量（百分比）。0 = 纯法向斥力（会在弹流前来回顶）
vo_frames     = 3     ; 硬层：这么多帧内会撞的方向直接禁用；九向全禁 → 炸弹
slow_dist     = 40    ; 有弹已在这个距离内就按低速
att_x_pct     = 12    ; x 引力（对齐 boss）百分比，相对斥力满值 100；要弱
att_y_pct     = 12    ; y 引力（回 home_y）百分比
home_y        = 384   ; 无威胁时想待的 y（中下）
wall_margin   = 24    ; 左 / 右 / 上边界的软斥力作用距离（px）
wall_margin_bottom = 12 ; 下边界单独一档
wall_bottom_pct = 50  ; 下边界斥力系数百分比
smooth_pct    = 40    ; 合力指数平滑：F = 40% 本帧 + 60% 上帧。100 = 不平滑
switch_hyst_pct = 25  ; 换方向滞回：新方向得分要高出当前 25% 才换
hold_frames   = 4     ; 换向后最少保持帧数（被硬层禁用或得分转负除外）
slow_exit     = 60    ; 低速退出距离（进入是 slow_dist），施密特触发
slow_hold     = 10    ; 低速状态最少保持帧数
bomb_cooldown = 90    ; 两次炸弹之间最少帧数
log_every     = 60    ; 每 N 帧记一行状态；0 = 不记
show_text     = 1     ; 屏幕上显示 AI Control On/Off
text_scale_pct= 75    ; 文字缩放百分比（字宽 12 × 缩放）
text_x        = 4     ; 相对关卡区左上角的偏移（虚拟屏单位，高 480）
text_y        = 20
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
[guard] 序言签名匹配，跳板就绪（vk=0x43 轮换 MANUAL / AUTO / BYPASS）
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
| 5 | 按 C 轮换三种模式，左上角文字随之变；MANUAL 下键盘立即接管 | 覆盖是逐帧的、没有残留；AsciiManager 记录写对了 | 文字不出现 → 见 `TARGET.md` §5 的 AsciiManager 一节，先看 `numStrings` 是否被别处占满 |
| 6 | AUTO 下按住左键，自机朝光标走并停在光标附近 | 光标→关卡区的布局模型成立（16:9 实测过） | 整体偏一个固定量 → 非 16:9 窗口，布局模型待标定（同 mouse-control §6） | 覆盖是逐帧的、没有残留 | — |

判定式是照 `Player__CalcKillBoxCollision` 一手复刻的（弹：`min(grazeSize)/2 + 1.25` 圆对圆；
敌人本体：`hitboxDimensions/3` 半宽盒、圆对盒、且只对 `flags & 3 == 3` 的敌人），
见 [`TARGET.md`](TARGET.md) §6。「看着没碰到却死了」的话先怀疑时序，不是半径。

## 策略：为什么是势场 + VO 掩码

2026-09-11 调研后定的（来源见文末）。要点：

- 经典人工势场只看位置，四个公认的病：局部极小、目标不可达、窄缝振荡、**不知道障碍在动**。
  弹幕里最致命的是「合力为零但正在被撞」。业内对快速动态障碍的成熟做法是**硬约束 + 软偏好两层**：
  用速度障碍（VO / ORCA）之类在速度空间划出「选了就撞」的集合当硬约束，再用势场或代价函数选偏好。
  公开的、读内存的东方 bot twinject 也是拿约束型 VO 当主力，势场只是备选。
- 势场这边选了三个变体叠加：**速度势场**（Ge & Cui 2002：斥力看相对速度沿视线的分量，迎面弹才推）、
  **预测势场**（把势放在最近接近点而不是当前点）、**涡旋场**（加切向分量，绕着障碍滑而不是被顶回来）。
- **时间折扣** `exp(−t*/τ)` 一个因子承担两件事：越远的未来越不紧迫；线性外推越远越不可信
  （NC 的弹有 `exFlags` 转向 / 加速，本策略没读它们）。
- 斥力大小用无量纲的 `((safe_r − d*)/safe_r)²`，净距超过 `safe_r` 的弹完全不参与——几百颗弹的
  `1/d²` 尾巴叠起来会变成一堵假墙，这是 APF 在密集场景里的另一个常见病。
- 引力受「安静 30 帧」门控 + 上一帧方向惯性，压 APF 的经典振荡（在斥力边界上来回进出）。
- 旧的采样版（DWA 型：18 候选 × 24 帧外推取最小净距）退役进 `native/archive/`，它的一帧极简形态
  留作硬层安全网。逐条决策记录在 `native/policy.c` 头部（D1–D8）。

来源：*Motion Planning in Dynamic Environments: A Survey from Classical to Modern Methods*（arXiv 2606.02677）；
Ge & Cui, *Dynamic Motion Planning for Mobile Robots Using Potential Field Method*, Autonomous Robots 2002；
*Predictive Angular Potential Field-based Obstacle Avoidance for Dynamic UAV Flights*（arXiv 2208.05873）；
*Vortex Artificial Potential Field for Mobile Robot Path Planning*（J. Phys.: Conf. Ser. 2406）；
Netdex/twinject 及其 LoLK 扩展记（netdex.org, 2018）。

## 没反应？按这张表查

| 日志情况 | 说明 | 怎么办 |
| --- | --- | --- |
| log 文件和 DebugView 都没有输出 | 代理根本没被加载 | 文件名正好是 `VCRUNTIME140_1.dll`、和 `th06nc.exe` 同一层；没装 mouse-control 的同名文件 |
| 启动就弹 `0xc0000142` | 接不上 System32 的真身，**刻意**响亮失败 | 确认 `C:\Windows\System32\VCRUNTIME140_1.dll` 存在 |
| `[guard] …序言与预期不符，永久停用` | 不是这个 build | 换回记录在案的版本，或按 [`TARGET.md`](TARGET.md) §7 重取 |
| 只有 `[guard] …跳板就绪`，没有 `[diag]` | hook 装上了但没被触发 | 先进关卡 |
| 有 `[diag] agent_tick 首次触发` 但一直 `playerState=…本帧不接管` | 自机状态不是 0/3 | 看打出的值；若长期是 1/2 之外的值，状态枚举有误 |
| 自机在弹流前来回顶 | 切向分量不够 | 调大 `vortex_pct`；仍抖看 `[tick]` 行的 `F=` 是不是在正负间跳 |
| 远处的弹一出现就吓跑 | 时间折扣太弱 | 调小 `tau`（或调小 `safe_r`） |
| 贴着弹过去、偶尔被擦死 | 硬层视界太短或斥力起得太晚 | 调大 `vo_frames`（4–5）或 `safe_r` |
| 无弹时在中轴附近来回走 | 引力 / 惯性把自机推过死区 | 2026-09-11 已修（惯性只在合力同向时加）；再犯就调小 `att_*_pct` |
| 总往上冲到 boss 脸上 | 涡旋选边参考点太高 | 已改为朝 `(0, home_y)` 选边；仍有就调低 `home_y` 或 `vortex_pct` |
| 撞激光 | 预警期太短 / 伸长太快 | 调大 `vo_frames`；看 `[tick]` 的 `l=` 是否非零（为零 = 激光没读到） |
| 移动抽搐 | 八向量化的扇区边界 / 起停 / 低速键翻转 | 调大 `smooth_pct` 的反面（调小数值 = 更平滑）、`switch_hyst_pct`、`hold_frames`；反应变慢就往回调 |

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
    ├── policy.c/.h        内嵌策略：速度势场 + VO 掩码（纯 C；宿主可测；头部有决策记录）
    ├── archive/           退役的采样版策略（不进构建）
    ├── hook.c             跳板 / 每帧回调 / 守卫 / 日志 / ini
    ├── proxy_vcr.c        VCRUNTIME140_1 代理（照抄 mouse-control）
    ├── build.sh           交叉编译
    ├── check.py           机器核对（AUDIT.md 的支撑）
    └── tests/             gcc 单测：test_policy.c / test_extract.c
```

## 还没做

- **激光的转角**：按当前 `angle` 静止处理；扫射型激光的转动没读。
- **道具**：不捡。
- **符卡 / 剩余时间**：不读。
- **协议 / 网络 / Python 客户端**：按 spec 推后；`world.h` 已按它的字段名起，接上时只加编码器。
