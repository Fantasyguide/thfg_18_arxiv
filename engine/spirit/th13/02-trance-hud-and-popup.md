# th13 灵界 HUD 与「收神灵」提示 —— 倒计时位置·颜色 / 灵界背景真身 / 三种弹窗

> **版本**：TH13 v1.00c（`th13.exe`，md5 `6f39c0708ee4daa94f5053526c4e10cc`，790,528 B，imagebase `0x400000`）。本文裸地址默认属该版本；引用其他版本一律写成 `th18:0x…`。
> **方法**：Ghidra 12.1.3 headless 反编译/反汇编（工程 `local/th13.v1.00c/ghidra_projects/th13.exe.gpr`）＋ **capstone 5.0.7 直读 exe 线性反汇编**（不抢工程锁）＋ `thdat`/`thanm` 解包读 anm 脚本＋ th-re-data 结构体对账。TH18 侧只做「残留检索」，每处都标 `th18:`。
> **可信度**：结论以 ✅ 一手（附地址或脚本号）为主；**「某物不存在」一律 🟡**；未能钉死的点集中在 §5。
> **与 [`01-spirit-and-trance.md`](01-spirit-and-trance.md) 的关系**：本文是它的续篇，**订正／细化了它 4 处**（§1.3 闪烁色、§2.1 背景启动点、§3.1 提示真身、§3.2 升格脚本号），那篇正文不改。

## 0. 一句话结论

| 问题 | 结论（含可信度） |
| --- | --- |
| 倒计时画在哪 | 文本创建坐标 = **(224, 112)**、水平居中、**顶对齐**；**不是屏幕最顶边**（y=112 ≈ 480 高的 23% 处）✅ |
| 倒计时什么色 | 默认白 `0xffffffff`；闪烁色 = **`0xffff0000`（不透明的红）**，不是蓝 ✅ |
| 倒计时字体 | TH13 ASCII **font_id 7**（与 font 6 同一套大字字形表）；字号由环境 scale 1.0 决定 ✅ |
| 是不是半透明 | **不是**：两条颜色的 alpha 都是 `0xff`，代码里没有 alpha 动画 ✅ |
| 灵界背景是什么 | `effect.anm` 里那台 **effect 0** 宿主 VM 的 5 个孩子之一 = **`astral.anm` script0**（512×512 全屏贴图、blendMode 7、自转）+ 4 块幕布 + 1 次只写 alpha 的游戏区四边形 ✅ |
| 收集/满格/满槽的提示 | 都是 **`front.anm` 一次性 VM**（96–110 号脚本，"+1/+2/+10/+100" 小图标），不是卡片/横幅；满一格 = 108/109，满槽 = 110 ✅ |
| TH18 有没有残留 | **弹窗机制是活的祖先（能复用）**；TH13 的提示贴图/脚本/anm **一个都没有**；唯一真残骸 = **5 个神灵音效** ✅/🟡 |

## 1. 灵界倒计时（用户 ⑧ 前半）

### 1.1 两个函数、一次创建

| 环节 | 地址 | 一手证据 |
| --- | --- | --- |
| 绘制入口（受灵界标志闸） | `0x405d60` | `TEST dword [0x4be830],0x100; JE ret; JMP 0x405940` —— 非灵界直接返回 |
| 注册优先级 | `0x405602` | `MOV EBX,0x33; MOV [ESI+8],0x405d60; CALL 0x470a40`（register__on_draw，**优先级 51**）|
| 绘制体 | `0x405940` | 设环境 → `FUN_00403e70("%.2fs")` → 还原环境 |
| 文本创建 | `0x403e70` → `0x403da0` | `_vsprintf` 后调 `AsciiManager__create_string`（`this`=ESI、文本=EAX、**位置向量=EBX**）|

⇒ 每帧都**重新创建**一条字符串（记录 `+0x12c` 的存活帧数来自环境 `+0x19180`，该字段**全 exe 无人写** ⇒ 恒 0 ⇒ 串只活一帧）✅。

### 1.2 位置：一手常量 (224, 112)

`0x405946`–`0x405980` 把三个 float 压进栈上向量，`0x4059b9 LEA EBX,[EBP-0xc]` 传进去：

| 分量 | 常量地址 | 值 | 一手指令 |
| --- | --- | --- | --- |
| x | `0x4aec9c` | **224.0** | `FLD [0x4aec9c]; FSTP [EBP-0xc]`（`0x405946`）|
| y | `0x4aeb50` | **112.0** | `FLD [0x4aeb50]; FSTP [EBP-8]`（`0x405957`）|
| z | — | 0.0 | `FLDZ; FSTP [EBP-4]`（`0x40596a`）|

文本值 = `CURRENT_SPIRIT_GAUGE / (double)0x4ae898（=60.0）`，格式 `"%.2fs"`（串在 `0x4aa3b4`）⇒ 满槽 600 → `10.00s` ✅。

对齐（`0x40596c`/`0x405976`）：`[mgr+0x19184]=0`（水平）、`[mgr+0x19188]=1`（垂直）；在 `0x404413` 与 `0x404486` 两个分支里读到这两个字段 ⇒ **0 = 居中、1 = 顶**（与 TH18 同义，但这是 TH13 自己代码里的用法）✅。所以：**字符串的水平中心在本行的 x 处，字形顶边贴在 y 处。**

**「是不是屏幕顶部」——不是。** y=112 在 480 高的画面上约 **23% 处**（§2.4 那次覆盖游戏区的四边形是 y=16..464 🟡，即使按它算，112 也比游戏区顶边低约 96 px）。它**唯一配得上「最上面」的地方是绘制顺序**：该串落在 ASCII 的 **group 0 绘制趟 = 优先级 `0x4a`(74)**，是全场最后一个 Anm/ASCII 绘制趟（Anm 层桩最高只到 `0x47`(71)，Supervisor 收尾在 `0x4c`(76)）⇒ **压在所有东西之上** ✅。

> ⚠️ **ASCII 坐标空间的原点没钉死** ❓：TH13 的 `create_string` 把位置**原样**拷进 VM 的 `pos`（`0x4042d0`–`0x4042e8`，无缩放——TH18 那边会乘 `DAT_0056aca0`），所以 (224,112) 的绝对值取决于该趟 ASCII 绘制用的相机。同帧另两条 ASCII 文本（符卡编号 `(318,192)`、符卡计时 `(362,198)`，见 `Gui__on_draw_2e` `0x427ab2` 一带）都落在 0..640/0..480 内，倾向「屏幕 640×480、左上原点」🟡；`x=224` 正好是 32..416 那个游戏区的水平中心，与「居中」自洽 🟡。**两条读法下「不是屏幕最顶边」都成立**。

### 1.3 颜色与闪烁：逐条核实（本节订正 01 文档）

`0x405983`–`0x4059a6` 的门（`EAX = CURRENT_SPIRIT_GAUGE`）：

| 条件 | 一手 | 结果 |
| --- | --- | --- |
| `gauge < 0x1e`(30) 且 `gauge & 1 == 0`（偶） | `0x405988 CMP EAX,0x1e`／`0x40598d TEST AL,1; JZ skip` | **跳过**染色 → 用环境色 |
| `gauge < 30` 且 **奇数** | 同上（不跳） | **染** `0xffff0000` |
| `30 <= gauge < 0x5a`(90) 且 `gauge % 8 == 0` | `0x405993 CMP EAX,0x5a; JGE skip`；`0x405998 AND EAX,0x80000007`…`0x4059a4 JNZ skip` | **染** `0xffff0000` |
| `gauge >= 90` | `0x405993 JGE` | **跳过**（永远白）|

染色指令只有一条：`0x4059a6 MOV dword [ESI+0x19160],0xffff0000`；绘制末尾 `0x405a02 MOV dword [ECX+0x19160],0xffffffff` 还原白色。

**颜色语义（关键订正）**：`D3DCOLOR` 是 **ARGB**（顶点色按 D3D9 `D3DCOLOR` 直写），TH13 自己的一手证据是 anm `rgb/rgb2` 指令的字节序：`0x463abf` 起 `arg1→[ebx+0x52e]`、`arg2→[ebx+0x52d]`、`arg3→[ebx+0x52c]`，而 anm 的实参序是 `(r,g,b)` ⇒ 高位字节 = **A、然后 R、G、B**。于是：

- `0xffff0000` = A=255, R=255, G=0, B=0 = **红色**（不是 [`01`](01-spirit-and-trance.md) §8 写的蓝 `(0,0,255,255)`——那是把 dword 误读成字节数组的结果）。
- 闪烁规律 = 「**剩 0.5 秒内隔帧闪**，剩 0.5–1.5 秒每 8 帧闪一次，1.5 秒以上不闪」——降级警告红的观感成立 ✅。

### 1.4 字体：font_id 7（大字）

| 项 | 一手 |
| --- | --- |
| 字体字段 | `[mgr+0x19178] = 7`（`0x405960`），绘制末尾还原 0（`0x4059ec`）|
| 字形表 | ASCII 绘制里 `switch(font)`（`0x404653`，跳表 `0x404b28`）⇒ **font 6 与 7 共用 `0x404920` 分支** |
| 字形索引 | `0x404993 LEA ECX,[EAX+EAX*8+0x855]`（`char*9 + 0x855`）；`.` → `0x52f8`、`s` → `0x5340`（`0x404954`/`0x40497e`）|
| 字距 | 该分支 `FMUL qword [0x4aebb0]（=30.0）` ⇒ 每字步进 = `scale_x × 30`（小号 font 2/3 是 9）|
| 缩放 | 环境 `[mgr+0x19164]/[mgr+0x19168]`，创建时被沿途代码设成 1.0（`0x4059cf`–`0x4059e0` 还原）|

⇒ 「TH13 的大号 ASCII 数字」= font 6/7；`ascii.anm` 由 `0x40393e` 预载（slot **2**）✅。字形号→精灵的对应走 `[ascii_anm+0x11c] + 索引*8` 的指针表（`0x404679`），**表内布局未逐项解**❓（见 §5）。

### 1.5 是不是半透明：不是

- 两条颜色（白 `0xffffffff`、红 `0xffff0000`）的 alpha 都是 **0xff**；
- 该记录的颜色在创建时被原样抄进 `vm1.color_1`（`[mgr+0x53c] = vm1+0x528`，`0x4049e4`/`0x404a55`）✅；
- 顶点色 = `vm+0x528`（或按模式的 `+0x52c`）**直接进顶点缓冲**（`0x4677eb`/`0x4677f3` → 写 `0x4e4820`），中途只有 AnmManager 的全局调制色 `ANM_MANAGER+0xf53c40`（`0x4677f9`）——而**灵界路径全 exe 没有一处写它**（写点只有 `0x407285`、`0x43c57b`）✅。

⇒ **倒计时文本是不透明的白/红**；用户感到「半透明」若来自实机观感，源头不在文本记录色（要么是覆盖在画面上的灵界背景层，要么是记忆偏差）❓——**别把「半透明」写进复刻需求**；要半透明得自己加（且 TH13 原版没有先例）。

## 2. 灵界背景（用户 ⑧ 后半）

### 2.1 启动点：`0x405ac5` 起的是 **effect 0**，不是直接起 anm（订正 01 文档）

`TranceManager::begin` `0x405a20` 里：

```asm
00405aaa  PUSH 0x0            ; 第 3 参 = 0
00405aac  PUSH 0x0            ; 第 2 参 = 效果 id = 0 ★
00405aae  LEA EDX,[EBP-0x108] ; 第 1 参 = 出参（拿 VM id）
00405ab4  PUSH EDX
00405ab5  XOR ECX,ECX         ; this = 0（无宿主）
00405ac5  CALL 0x415650       ; EffectManager::spawn_effect(id=0)
00405aca  MOV EAX,[EAX]       ; VM id
00405ad2  MOV [ESI+0x38],EAX  ; 存进 TranceManager+0x38
00405acd  MOV EBX,0x9
00405ad5  CALL 0x46fc30       ; anm_set_interrupt_by_id(vm, 9) ★ 状态翻到“灵界中”
```

- `FUN_00415650` = **EffectManager 的 spawn**（`EFFECT_MANAGER_PTR` `0x4c2180`；表 `0x4bba80`，stride `0x18`：`{i16 anm_slot; i16 script; fn setup; fn a; fn b; fn c}`）✅。
- effect 0 = `{anm_slot 0 = effect.anm, script 0}`（slot 表 `0x415370` 把 `effect.anm` 装在 `mgr+0x10`、`bullet.anm` 在 `mgr+0x14`）✅。effect.anm 的 script0 是空壳（`alpha(0); stop()`），**真正的活都在 C++ 回调里**。
- `anm_set_interrupt_by_id` `0x46fc30`：`ECX`=VM、**`EBX`=中断号**，写 `vm+0x4a0`（pending_interrupt）并调 `vm+0x59c` 的 on_interrupt ✅。

### 2.2 effect 0 的五个部件

| 部件 | 地址 | 干什么（一手）|
| --- | --- | --- |
| setup | `0x402ff0` | `malloc(0x1c3c)` 挂到 host VM `+0x58c`；前 4 格 = 4 台子 VM（模板来自 effect.anm）；**第 5 格 `+0x1690` = 从 `[TRANCE_MANAGER+0x10]`（astral.anm 句柄）拷 script0 模板** ✅ |
| on_tick | `0x403260` | 逐帧 `AnmVm__run` 4 台子 VM，全灭则回收 |
| on_draw | `0x4032d0` | 见 §2.4 |
| on_destroy | `0x4035d0` | 空（`xor eax,eax; ret`）|
| on_interrupt | `0x4035e0` | `switch(id)`：**1** → 用 effect.anm script **7/8/9/10** 重建 4 台子 VM（`0x40360c LEA EAX,[ESI+7]`）；**7/8/9** → 设 host 层 = 0x1a/0x14/6 并置状态字|

> 尾段把 4 台子 VM 的 entity_pos 设成 **(320,240,0)**（`0x4aea3c`/`0x4aea38`）

进出各一次：**begin = 中断 9**（`0x405ad5`）→ 状态字段 `+0x1c34 = 2`；**end = 中断 1**（`0x405c48 MOV EBX,1` → `0x405c4d CALL 0x46fc30`）→ 幕布重扫 + 状态脱离 2。

### 2.3 `astral.anm` script0 逐指令 ✅（`thanm -l 13` 一手）

```
-1: type(1); anchor(0,0); sprite(sprite0); angleVel(0,0,0.010471975);
    pos(320.0, 224.0, 0.0); scale(1.15, 1.15); alpha(1); blendMode(7);
 0: stop();
```

| 项 | 值 / 含义 |
| --- | --- |
| sprite0 | entry0 `effect/eff_astral.png`，**512×512 整张**（`sprite0: {0,0,512,512}`）|
| `anchor(0,0)` | **居中**（0 = 居中系数，同 TH18 `engine/anm/th18/03-ascii-text.md` §2f 的锚点表；TH13 未另证 🟡）|
| `angleVel z` | `0.010471975` rad/帧 = 0.6°/帧 ⇒ **360° 用 600 帧 = 10 秒/圈**（“缓慢自转”）|
| `scale(1.15)` | 588×588 px，**盖住整屏**（中心在 (320,224)，屏幕中心 (320,240)）|
| `alpha(1)` | 写 `color_1.A = 1`（anm `alpha` 指令 = `0x463a37 MOV byte [ebx+0x52b],AL`）——**极低**，但见下 |
| `blendMode(7)` | **`DESTALPHA(7)/INVDESTALPHA(8)/ADD`**（TH13 自己的跳表 `0x467320` → `0x4671e3`）⇒ RGB = `src×dst.a + dst×(1−dst.a)`，**源 alpha 不参与** ⇒ `alpha(1)` 不会让它隐形 ✅ |
| 位置跟随谁 | **不跟随**：坐标是常量 (320,224)，无父 VM（`entity_pos` 由回调设成 (320,240) 的是那 4 台幕布）|
| 淡入淡出 | **没有**：script0 只有 `-1:` 与 `0: stop()`，无 `alphaTime`、无 `delete` ⇒ **常驻整个灵界窗口** |

### 2.4 唯一的整屏 D3D 运算：一次「只写 alpha」的游戏区四边形 ✅

`0x4032d0`（host VM 的 on_draw）里，**仅当状态字 `+0x1c34 == 2`（灵界中）**：

1. `flush_sprites` 后直接调 D3D 设备（`0x4dc6a8`）设状态：`SetRenderState(0xf←0)` 关 Z、`(0xce←1)` 开 separate-alpha-blend、`(0x13←1 SRCBLEND=ZERO)`、`(0x14←2 DESTBLEND=ONE)`、`(0xab←1 BLENDOP=ADD)`、`(0xcf←2 SRCBLENDALPHA=ONE)`、`(0xd0←1 DESTBLENDALPHA=ZERO)`、`(0xd1←1 BLENDOPALPHA=ADD)`；
2. 顶点 4 个（stride `0x14` = x,y,z,rhw,color）+ `SetFVF(0x44)`（XYZRHW|DIFFUSE）+ `DrawPrimitiveUP(5, 2, …)`：坐标 **(128,16)-(512,464)** = 384×448 = 游戏区大小、以屏幕中心为中心；**顶点色 = `0x00000000`**（alpha 0）。

⇒ **RGB 结果 = 保留目标、alpha 结果 := 0**，即把游戏区那一块的 alpha 通道清零（一次「掩码」写入）。**这不是变暗/变色**。
3. 随后 `0x403570` 循环 4 次 `AnmManager__draw_vm(mgr, 子VM_i)`（幕布），`0x403587 CMP [EBX+0x1c34],2; JNE skip` → **只有灵界中才**再画第 5 台 `EBX+0x1690` = **astral script0** ✅（`0x403591`–`0x40359e`）。

### 2.5 「还有没有别的整屏效果」——扫过的面（找不到不写「没有」）

| 扫的面 | 方法 | 结果 |
| --- | --- | --- |
| 灵界标志 `GAME_FLAGS 0x4be830` bit `0x100` 的**所有读点** | `find_imm_refs th13 0x4be830`（83 处）逐条看助记符 | 与绘制有关的只有 3 类：`0x405d60`（倒计时）、`0x40e7c2`/`0x40e7f2`（`BulletManager__on_draw_*`：灵界中子弹换外观）、`0x41312d`/`0x4131e2`（`Spellcard__on_draw`，读它算字色透明度 `0x4131e2` 一带）；其余全是逻辑（免伤闸、死亡链、敌弹生成、BGM、菜单）✅ |
| 直接碰 D3D 设备的函数 | 扫 `0x4dc6a8` 全部 321 个引用点，按 4KB 分页归类 | 灵界/神灵子系统里**只有 `0x4032d0` 一处**；其余集中在 `0x402xxx`/`0x403xxx`（特效系统）、`0x43axxx`–`0x43cxxx`（Supervisor 的渲染阶段）、`0x46xxxx`（AnmManager）✅ |
| AnmManager 的**全局调制色/淡入淡出** `ANM_MANAGER+0xf53c40`（顶点路径 `0x4677f9` 起会逐通道乘它） | 扫写点 | 只有 `0x407285`、`0x43c57b`（关卡淡入淡出/初始化）；**灵界路径一个写点都没有** ⇒ 灵界不改全局 sprite 染色 ✅ |
| 切渲染目标 | 同上（设备引用普查） | 灵界路径内没有 SetRenderTarget 一类的调用（唯一设备使用者是 `0x4032d0`，且它只改 render **state**）🟡（此判据只覆盖「直接读设备全局」的调用，间接缓存 vtable 的写法看不见）|
| 颜色常量 | `d3dcolor` 立即数扫描（`00 00 00 FF` 等 174 处）| 灵界相关只有 `0x4059a6`（倒计时红）与 `0x4032d0` 那组 render state ✅ |

### 2.6 「背景变的是哪一层」

**世界层（游戏区内的 sprite 层），不是 UI 层** ✅，依据三条：

1. astral script0 由 effect 系统的 on_draw 用 `AnmManager__draw_vm` **即时画**（不走 UI 列表的层号折叠），它和 4 台幕布都画在 host VM 所在的那一趟里；host 层被设成 **6 / 0x1a / 0x14**（`0x402ff0` 的 `[param_1+0x24]=0x1a`、`0x4035e0` 的 case 7/8/9）——都是世界列表的层 ✅。
2. 那次 alpha 四边形用的是 `XYZRHW`（**预变换屏幕坐标**），盖的正是 384×448 的游戏区 ✅。
3. **层号语义（TH13 内部对照）**：世界内的特效用低层（神灵贴图 `layer(6)`、灵界形态 `layer(13)`、收集提示 `layer(11)`），HUD 用高层（灵界槽 17、符卡 19、`0x528,32` 那批 23、血量条 25，全在 `front.anm`）⇒ host VM 的层 6/0x1a/0x14 落在**世界层一侧** 🟡。

⇒ 用户说的「整体背景也会变」= **游戏区那一层被灵界背景盖住**（旋转的 512×512 灵力图案 + 深蓝幕布），HUD/ASCII 层不受影响。

## 3. 收神灵 / 满一格 / 满槽的提示（用户 ②③）

### 3.1 收一颗神灵：**`front.anm` 一次性小图标**（不是卡片、不是横幅、不是数字）

收集分派在 `SpiritItem__on_tick` `0x438730`（分散半径 `dist²<400` 那一支，`0x438b17`）：

| 步 | 一手 |
| --- | --- |
| 播音 | `0x438b2b MOV EDI,0x3f` + `0x438b38 CALL 0x460c50` ⇒ **收集音 = 0x3f** |
| 槽 +2（所有类型） | `0x438b3d LEA EAX,[EDI-0x3d]`(=2) + `0x438b45 CALL 0x43d530` |
| 奖励分派 | `0x438b4a` 读 `[ebx+4]`（type）→ 跳表 `0x438e3c`（普通）／`0x438e4c`（灵界中，由 `0x438a59 TEST [ebx+0x84],1` 选）|

**弹窗真身** = `FUN_0046f820(&out, script, pos, 0, -1, 0)`，其中 anm 句柄 = `[GUI_PTR+0x2a8]`（`0x438b7c MOV EDX,[0x4c2190]` = `GUI_PTR`）⇒ **`front.anm`**：GUI 初始化里 `0x424f00`–`0x424f12` 是 `ECX="front.anm"(0x4aaca4) + EAX=5 + CALL 0x46df40` 后 **`MOV [EDI+0x2a8],EAX`**（EDI=GUI）✅；`pos` = 神灵自己的位置（`0x438b79` 推 `<spirit>+0x14`）✅。

| type | 奖励 | 普通脚本 | 灵界脚本 |
| --- | --- | --- | --- |
| 0 生命碎片 | +1 / **+2** | **0x60**（sprite66）| **0x61**（sprite67）|
| 1 最大得点 | +10 / **+100** | **0x66**（sprite72）| **0x67**（sprite73）|
| 2 符卡碎片 | +1 / **+2** | **0x63**（sprite69）| **0x64**（sprite70）|
| 3 分数 | 加分 | **数字弹窗**（`0x44ad60`，黄 `0xffffff00`）| 同一支（`0x438dce`）|

脚本形状（96–110 共 15 支，逐指令一手；96–106 = 50 帧版，107–110 = 100 帧版只改 `posTime(100,…)`/`wait(96)`）：

```
-1: sprite(spriteN); ins_431(1); randMode(1); resampleMode(1);
    anchor(0,2); layer(11); scale(1.0,0.0); scaleTime(4,0,1.0,1.0);
    posTime(50,1, 0.0,-16.0,0.0);          // 50 帧内上浮 16 px
 0: wait(46); scaleTime(4,0,1.0,0.0);      // 收起
+4: delete();                              // ⇒ 寿命 50 帧（≈0.83 s）
```

> 4 台幕布（effect.anm 3–10）另有一套：`scrollMode(0,1)` 拉出 3×12 倍的 `effect/eff_switch.png`、`color(32,16,96)` 深蓝紫、`posTime(30,1,…)` 30 帧扫过屏幕、末尾 `delete()`；**script3 例外**——t=45 换成 `sprite(-1); drawRect(640.0,480.0)` 并 `stop()`，会留下一块常驻全屏方块（脚本里都没有 `layer()`，因为它们是效果系统 `draw_vm` 即时画的）✅。

贴图 = `front/front00.png`（entry0，512×512）+ sprite **66–80**（24×8 / 32×8 / 32×16 三档小条），**像素里就是「+1 / +2 / +10 / +100」这类文字**（我把 66–80 逐格导出成字符画核对过：`+1`=66、`+2`=67、`+10`=72、`+100`=73，与奖励数值一一对上）✅。位置 = 收集点、**不跟随任何人**、层 11、锚点底部居中、先纵向拉伸再收起。

**type 3 的特例**：`0x44ad60` 是 **`zAsciiPopupManager`**（`POPUP_MANAGER_PTR` `0x4c22d4`，对象 `0x8fc` B，内嵌 AnmVm 在 `+0x18`，弹窗记录 **`+0x5bc` × 13 槽、stride `0x40`**，工程里只用 10 个）：把得分数值**逆序写成裸 digit 字节**、位置 `+0xc/+0x10/+0x14`、色 `+0x1c`（调时传 `0xffffff00` = 黄）、计时 `+0x28` = `&GAME_SPEED`；`PopupManager__on_tick` `0x44a970` 每次把 `+0x20` 加 1，**> 0x3c(60) 就清 busy** ⇒ 寿命 **60 帧** ✅；字形来自 **`ascii.anm`**（`PopupManager__initialize` `0x44a6f0`：`[mgr+0x10] = ASCII_MANAGER_PTR->ascii_anm`，字形基址按 `+0x20` 的档位在 `0x6e4/0x747/0x7a1` 三套数字里选）✅。

### 3.2 满一格（每 200）/ 满槽（600）：同一支函数、不同脚本号（订正 01 文档 §5）

`FUN_0043d530`（`0x43d530`，ECX = `&CURRENT_SPIRIT_GAUGE`、EAX = 增量）：

```c
old = *gauge; *gauge = min(600, old + add);
if (old/200 != *gauge/200) {                    // ★ 跨过 200 的倍数才触发
    FUN_00460bc0(?, 0x44);                       // 音效 0x44（0x43d574 把 0x44 交给它）
    pos = {player.x, player.y - 32.0, player.z};
    FUN_0046f820(&out, *gauge/200 + 0x6b, &pos, 0, -1, 0);   // ★ front.anm 脚本
}
Gui__sub_42b030_spirit_related();               // 刷槽 HUD
```

- `*gauge/200` 只会是 **1 / 2 / 3** ⇒ 实际脚本号 = **108 (0x6c) / 109 (0x6d) / 110 (0x6e)**；**107 (0x6b) 到不了**（[`01`](01-spirit-and-trance.md) §5 写的「107/108/109」是 off-by-one）✅。
- **满一格（第一次/第二次跨档）= 108 或 109**（sprite78/79，32×16），**满槽（600）= 110**（sprite80，32×16）——同一支脚本形状，只是**寿命 100 帧**（`posTime(100,1,…)` + `wait(96)`），贴图不同 ✅。
- 位置 = **自机上方 32 px**、层 11；样式与 §3.1 **确实是同一个模子**（用户说的「一样的样式」成立，差别只在时长与图）✅。
- 满槽还额外意味着「可以按灵界键」：HUD 子脚本 172 的 `pending_interrupt = 2/3` 两态由 `0x42b030` 每帧写（阈值 200，见 01 文档 §8）——**600 本身没有独立的画面变化**（没有额外特效/闪光），一手范围内就是这样 ✅。

### 3.3 与用户「类似 TH14 卡 bonus」这个类比的差异（照实写）

- 用户类比的那类东西是**文字/横幅**；TH13 的真身是**24×8～32×16 的小图标条**（贴图里画着「+1」这种字样），**50/100 帧自灭**、只上浮 16 px、不跟随、不遮挡。
- 唯一像「数字弹窗」的只有 type 3（分数）那支，而它是**全局共用的 `zAsciiPopupManager`**（道具得分等也用同一支：调用点 `0x42e8e9`、`0x42e92e`、`0x42e981`、`0x42e9db`、`0x42eae7`、`0x42ee02`、`0x42ee6d` 全在 `ItemManager` 一带）✅。
- **不许**把「TH14 卡 bonus」当结论写；TH13 里没有卡片状/横幅状提示 ✅。

### 3.4 引擎侧还是脚本侧

**引擎侧（C++）**：由 `SpiritItem__on_tick` / `FUN_0043d530` 直接 `FUN_0046f820` spawn 一台 anm VM（脚本号是立即数），或调 `0x44ad60` 建弹窗记录；**ECL 完全不参与**（ECL 只负责 `spec0-3` 让敌人掉神灵，见 01 文档 §3.3）✅。

## 4. TH18 残留与可复用面（用户 ②「有没有代码残留」）

### 4.1 逐条检索面 → 结论

| 检索面 | 方法（可复现） | 结论 |
| --- | --- | --- |
| TH13 弹窗 adder `0x44ad60` ↔ TH18 | 两侧反编译对形状（digit 生成 / ring 计数 / 字段布局）| **同源、而且活着**：TH18 `PopupManager__operator_new` `th18:0x463dd0` + adder `th18:0x4645f0` + tick `th18:0x464020` + draw `th18:0x464180` ✅ |
| TH13 分档 `0x43d530` 的签名 | TH18 全 exe 扫 magic `0x51EB851F` + `sar edx,6`（=/200）、常量 `0x258`(600)/`0xc8`(200) | **不存在** ✅（TH18 的 `0x51eb851f` 后继全是 `/100`）|
| TH13 倒计时 `0x405940` 的格式串 | 两 exe 扫 `"%.2fs"` | TH13 唯一（`0x4aa3b4`）；**TH18 零命中**（TH18 有的是 `"%.2ds"` `th18:0x4b65c8`，原生秒表 HUD，别当残留）✅ |
| TH13 灵界 anm/贴图 | `thdat -l 18` + 扫 dat 内文件名 | `astral.anm`、`littlegods`、`eff_astral` **全无** ✅ |
| TH13 收集提示脚本（形状） | `thanm -l 18 front.anm` 后按指令序列搜 | **TH18 的 `front.anm` script 85–99 与 TH13 的 96–110 逐指令同形**（同 `anchor(0,2)+layer+scaleTime(4,0,…)+posTime(50/100,-16)+wait(46/96)+delete`）✅ —— **数据在** |
| 上面那 15 支有调用者吗 | 扫 `th18:0x405bf0`（world 实例化）111 个调用点的脚本实参 + 扫 `6a 55..6a 63` 立即数 | **没找到** 🟡（不等于死；Th18 侧另用 `PopupManager` 做道具弹字）|
| 无调用者死代码 | TH18 funcs `xrefs==0` 计数 + 已知 cmd 9 先例 | 未做全量 data-ref 穷举 ❓（300 个零 xref 函数绝大多数是虚表方法）|
| **音效** | `thdat -l 18`/`-l 13` 条目名对账 | **`se_lgods1..4.wav` + `se_lgodsget.wav` 五个全在 `th18.dat` 里**（TH13 侧同名）✅ —— **真残骸** |

### 4.2 TH18 原生弹窗的接口（与 TH13 的差异）

| 项 | TH13（`zAsciiPopupManager`）| TH18（`PopupManager`）|
| --- | --- | --- |
| 全局 / 构造 | `0x4c22d4` / `0x44a8b0`（对象 `0x8fc`）| `th18:0x4cf420` / `th18:0x463dd0`（`0x1044`）|
| 内嵌 VM | `+0x18` | `th18:+0x18` |
| 记录数组 | `+0x5bc`，**stride `0x40`**，13 槽（只用 10）| `th18:+0x624`，**stride `0x48`**，18 槽 |
| 数字字体 | `ascii.anm`（`[mgr+0x10]`）| `ascii_anm`（`[mgr+0xc]`），字形基址 `0x121/0x12c/0x136` |
| 寿命 | `+0x20` 计数 > 60 帧清 busy | 数字槽 age > `0x3c` 清 busy；倍率槽每帧 alpha −4 淡出 |
| 额外槽位 | — | **k13–17 = 倍率槽**（`+0x40` int 值 / `+0x44` float 倍率 → `"BONUS %.1f"` / `"NO BONUS"`），**TH18 原生没有任何写者**（adder 只写 k0..k9）⇒ 这是 TH14 血统的空槽，mod 的 `ce_popup_mult` 就是在复活它 |
| 现成实现 | — | `mods/th18.v1.00a/card-expand/native/cards/collect2x.c`（`ce_popup_mult`）、`senro.c` 同款拷贝；常量表在 `mods/th18.v1.00a/card-expand/native/engine.h` 的 `CE_POPUP_*` |

### 4.3 TH13 真身 ↔ TH18 原生 逐条对照

| 维度 | TH13 原版真身 | TH18 原生 | 判定 |
| --- | --- | --- | --- |
| 收神灵的小图标 | `front.anm` 96–110（50/100 帧、上浮 16 px、层 11）| 同形脚本 85–99 在数据里，**未找到调用者** | **只能仿制**（贴图不同：TH18 那批是 48×16/64×16/64×32，且没有 TH13 的「+1/+2」语义）|
| 收 type3 的分数数字 | `zAsciiPopupManager`（60 帧、黄色）| 同族 `PopupManager` 数字槽（60 帧、任意色）| **能复用**（照 adder/字段写）|
| 满一格 / 满槽 | `front.anm` 108/109/110 + 音效 `0x44` | **没有对应 anm/贴图** | **只能仿制**（音效另说）|
| 升格/满槽的音效 | `0x44`（跨档）、`0x3f`（收集）、`0x3b+type`（生成）| **`se_lgods*` 五个 wav 都在**，id `0x3b–0x3f`（`th18-sound-table.md` §7 + 运行时 cfg 映射两源一致）| **能复用（白捡）** ✅ |
| 倒计时 `%.2fs` | TH13 `0x405940`（font 7、白/红闪）| 无 | **只能仿制**（TH18 用 ASCII `create_string` 自己画；注意 ASCII 趟在暂停模糊罩之后 ⇒ 不会被罩住）|
| 灵界背景 / 幕布 | effect 0 + `astral.anm` script0 + effect.anm 3–10 | 两边的 anm 全无 | **完全不存在** ❌ |
| 暂停模糊罩（暂停时的糊罩）| TH13 侧**未查**（本仓无记录）❓ | 罩 = UI layer 38 → 桩优先级 `th18:0x3e`；ASCII 趟在 `0x48` ⇒ 罩不住 | 复刻时自己选「罩得住（贴图 VM，层 25）／罩不住（ASCII）」|

### 4.4 结论与代价

**（a）能复用**：TH18 原生弹窗的**字段写入法**（`ce_popup_mult` 那套：写 `mgr+0x624 + k*0x48` 的位置/颜色/值/倍率，busy 最后置）——代价 0 新 ABI，几十行；**5 个神灵音效**（`0x3b–0x3f`）——代价 0。
**（b）只能仿制**：收集小图标、满格/满槽提示、倒计时。代价 = 自造贴图（或改用 TH18 现成 `effect.anm`/`text.anm` 的元素）+ 1 个 anm 脚本 + 触发点；若要求「跟暂停模糊罩一起糊」得做成 UI 层 25 的贴图 VM，否则用 ASCII（清晰但不糊）。
**（c）完全不存在**：`astral.anm`/`littlegods`/`eff_astral` 与灵界背景那一整套（TH18 里连贴图都没了）——**必须从零做**，且**没有原版素材可切**（TH13 的贴图不能进本仓）。

## 5. 反证面与未查实项

| 事项 | 状态 |
| --- | --- |
| ASCII 坐标空间的**原点**（(224,112) 是屏幕坐标还是游戏区坐标）| ❓ 两种读法都列出（§1.2）；未能找到钉死它的常量/调用 |
| ASCII font 6/7 的**字形号 → `ascii.anm` 精灵**映射（`[anm+0x11c]+idx*8` 的表内布局）| ❓ 索引公式一手，表内 stride/记录形状未解 |
| `anchor(0,0)` = 居中（astral script0 的 512×512 是否真以屏幕中心为心）| 🟡 用的是 TH18 的锚点系数表口径；TH13 未逐表复核 |
| effect 0 的「alpha 四边形」到底让画面**看起来**怎样 | 🟡 我只能给出状态与顶点（RGB 不变、alpha 归 0）+ 三层绘制顺序；观感需要实机截图 |
| 4 台幕布与 astral 同时存在时的先后/叠加观感 | 🟡 同上（顺序一手：四边形 → 4 幕布 → astral）|
| TH18 的 `front.anm` 85–99 到底有没有调用者 | 🟡 扫了 world 实例化 111 点与 `push imm8` 面，**没找到**；不等于死 |
| TH13 侧 96–110 这 15 支里哪几支**真的**被引擎用到（我只逐条读到了 96/97/99/100/102/103/108/109/110）| 🟡 其余（98/101/104–107）没找到调用点，但我的立即数扫描本身不完整，不下「死脚本」结论 |
| 满槽（600）是否有额外画面/音效 | ✅ 一手范围内没有（只有 108/109/110 那一支；HUD 172 的两态阈值是 200）|
| 「倒计时半透明」的来源 | ❓ 文本颜色 alpha=0xff；用户观感另有来源 |
| TH13 `%.2fs` 是否在别的版本还在 | th18 ❌（另一路扫过；TH18 是 `%.2ds`）|

## 6. 可信度与复核入口

- ✅ **一手**：§1 的全部（位置常量、闪烁条件与真色、font 7、alpha=0xff）、§2 的启动点/effect 0 五部件/astral 逐指令/blendMode 7/alpha 四边形/0x100 读点普查、§3 的分派表与脚本号、寿命与贴图对应、§4 的 th18 侧「无 astral/littlegods」与音效残留。
- 🟡 **推断/单源**：§1.2 的坐标空间读法、§2.6 的层级判定、§4.1 的「TH18 front.anm 85–99 无调用者」。
- ❓ **未解**：§5 表内各项。
- **复核命令**：
  `python3 tooling/ghidra/scripts/decompile_funcs.py th13 0x405940 0x405a20 0x402ff0 0x4032d0 0x4035e0 0x43d530 0x44ad60`、
  `python3 tooling/ghidra/scripts/dump_func_asm.py th13 0x405940 0x405ac5 0x403570`、
  `$THANM -l 13 -o -m local/vendor/thpages/static/mapfile/v8.anmm local/th13.v1.00c/dat/astral.anm`（`front.anm`/`effect.anm` 同法）。
- **TH18 侧交叉**：本文 §4 的 TH18 结论由一趟只读子调查独立复核（`th18:0x463dd0`/`th18:0x464180`/`th18:0x4645f0` 反编译、`th18.dat` 225 条对账、`"%.2fs"` 零命中），与本文一致；`th18-sound-table.md` §7 的 `0x3b–0x3f` 一行与我这次 `thdat -l 18` 的条目清单互相印证。
