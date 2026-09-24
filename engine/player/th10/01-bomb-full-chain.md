# TH10 灵击 —— 门槛与代价、伤害链、消弹半径曲线、震屏与 boss 判据

> **版本**：TH10 v1.00a（`th10.exe`，487,936 B，imagebase `0x400000`）。本文裸地址默认属该版本；引用其他版本须写成 `th11:0x…`。
> **方法**：一手平坦反汇编（产物 `local/_scratch/th10_full.asm`）+ 机械可达性闭包脚本（`int3` 切边界 + call 目标扩展）+
> `local/th10.v1.00a/dat/bullet.anm` 原始字节直读 + ExpHP 符号只取词汇。
> **可信度**：门槛/消耗/伤害链/半径曲线/震屏否定/boss 判据全部 ✅（指令原文 + 机械闭包 + anm 原字节）；
> 伤害表的**缩放链**经 2026-09-25 订正（见 §2.3）：「`player_state ∈ {0,2}` 才除 5」曾是 🟡 推测，
> 现已用跳转表 `0x426344` + 灵击门槛的位置证明**正常游玩 = 状态 1、那道 `/5` 不执行**（✅ 一手）。

## 0. 一句话结论

| 问题 | 一手答案 |
| --- | --- |
| 能不能放 | `CURRENT_POWER / 20 != 0`（**火力 ≥ 1.00**）+ `INPUT(0x474e5c) & 0x2`（X 键）；对话 VM 忙 / 灵击进行中不干 |
| 代价 | `add WORD ds:0x474c48, 0xffec` = **−20 单位 = −1.00 火力**；**代码里没有下限钳位**（可花到 0.00） |
| 决死灵击 | 同一门槛、同一 −1.00 消耗、**不扣信仰**；额外 `Timer(player+0x474, 60)` + `Timer(player+0x430c, 200)` |
| 伤不伤敌人 | **伤**。`Bomb::damage_bonus_at` `0x4059f0` 被**玩家→敌人伤害例程** `0x428630` 回调（`0x428891`），返回值累加进伤害、最终在 `0x40e1b6` 扣敌人 HP |
| 消弹半径 | **不是 32.0f**：`Bomb::on_tick` 每帧用灵击 anm VM 的 `scale.y` 覆盖它（`0x405813`）；曲线 = `scaleTime(120, 6, 16.0 → 160.0)` ⇒ 灵梦 160 / 魔理沙 112 / 符卡中 64 |
| 震屏 | ❌ **没有**（机械否定 + `ScreenEffect` 结构上没有平移字段） |
| 谁是 boss | `enemy+0x1444` 的 **bit `0x8000`**（ECL opcode 332 `setBoss` 置位），槽表 `ENEMY_MANAGER+0x10+4*slot` |

## 1. 门槛与代价（`Player::on_tick`）

```
; 通常（存活）分支 0x425964–0x425a24
0x425964: mov eax,ds:0x47770c ; GUI_PTR ; test/je      ; 无 GUI 不干
0x425971: mov ecx,[eax+0x9eb8] ; test/jne              ; 对话 VM 忙 → 不干
0x42597f: mov esi,ds:0x4776ec ; test [esi+0x28]/jne    ; 灵击进行中 → 不干
0x425998: movsx ecx,WORD PTR ds:0x474c48               ; ★ 火力（uint16）
0x4259a6: sar edx,0x3（配魔数 0x66666667）              ; = 火力 / 20
0x4259b0/0x4259b3: test cx,cx ; je 0x425a24            ; ★ 商 == 0 ⇒ 不可发动
0x4259b5: test BYTE PTR ds:0x474e5c,0x2 ; je 0x425a24  ; ★ X 键（bit1；bit2=0x4 是低速）
0x4259be: push 0x10e ; lea eax,[ebp+0x430c] ; call 0x405410   ; 无敌 270 帧
0x4259cf: call 0x405860                                ; Bomb::start
0x4259d4: add WORD PTR ds:0x474c48,0xffec              ; ★ 火力 −20 单位 = −1.00
0x425a15: push 0xbb8 ; call 0x412e70                   ; ★ 信仰 −3000（显示）
---
; 决死灵击 0x425bd9–0x425c8e：同样的 idiv 门槛（0x425bfe/0x425c0d）、同样的 −20（0x425c43）
0x425c20: push 0x3c ; Timer::set(player+0x474, 60)     ; 另 200 帧无敌（push 0xc8 → player+0x430c）
0x425c84: mov DWORD PTR [ebp+0x458],1                  ; player_state = 1
; ★ 全函数无 0x412e70 调用 ⇒ 决死不扣信仰
```

⇒ 「非决死灵击 −3000 信仰」✅ 与代码吻合（[faith/th10/01 §3.2](../../faith/th10/01-faith-and-points.md)）。
⇒ ⚠ **别把「钳在最低档」搬到 TH18**：TH18 的 `ce_spend_power` 钳 1 档是 TH18 的实现；TH10 允许花到 0.00。

## 2. 伤害链：`Bomb` 不自己打人，它提供「伤害加成」

### 2.1 入口穷举与机械闭包（先得到「打不到」的中间结论）

| 函数 | 地址 | 全部调用点 |
| --- | --- | --- |
| `Bomb::start` | `0x405860` | `0x4259cf`、`0x425c3e`（只有这两个，都在 `Player::on_tick` 里） |
| `Bomb::on_tick` | `0x405750` | `0x405843`（函数指针注册进 update registry） |
| `sub_405ac0`（消弹/消激光） | `0x405ac0` | `0x405816`（只在 `Bomb::on_tick` 内） |
| `Bomb::damage_bonus_at`（本文命名） | `0x4059f0` | `0x428891`（**在 `0x428630` 里**） |

从 `Bomb::start`/`Bomb::on_tick` 出发的可达函数共 **186 个**，其中**不含** `0x428630`（玩家→敌人伤害函数）、
`0x40e13c`（它的唯一调用点）、`0x40e1b6`（敌人 HP 扣减）—— 也就是说**顺着 `Bomb` 往下走，到不了敌人 HP**。
**但调用方向是反的**：

```
; 0x428630（玩家子弹 vs 敌人的伤害函数）尾部
0x428889: mov ecx,ds:0x4776ec      ; ecx = BOMB_PTR
0x42888f: mov eax,edi              ; &enemy.pos
0x428891: call 0x4059f0            ; ★ bomb->damage_bonus_at(&enemy.pos)
0x428896: mov ecx,[esp+0x54]       ; 伤害累加器
0x42889a: add ecx,eax              ; ★ 累加
（同一累加器里玩家子弹的伤害也在这里加：0x4287c8）
⇒ 返回值 → 0x428a6d 取出 → 0x428ab9 ret 0xc
```

落地（`EnemyData::on_tick` `0x40dc80`）：`0x40e13c call 0x428630`（**全 exe 唯一调用点**）→
`0x40e14e`–`0x40e165`：`player_state ∈ {0,2}` 时 **÷5** → boss 且符卡中再 **÷5**（共 ÷25，下限 1，`0x40e19e`）
→ `0x40e1b6 sub DWORD PTR [ebp+0x13c0],ecx`（**敌人 HP 的唯一扣减点**）。
频率 = **每个存活敌机每帧一次**；持续 = 灵击 anm VM 的存活期（VM 没了 `bomb+0x3c` 查询返回 0）。
顺带计分：`0x428a6d`–`0x428aa5` 把同一伤害 `/10` 后当「伤害分」加进 `CURRENT_SCORE`。

### 2.2 `0x4059f0` 的返回表（原始值，未过 §2.1 的缩放）

```
先判 d² = (ex−bomb.x)² + (ey−bomb.y)² < r²（r = bomb+0x3c，严格小于才进内圈）
bomb+0x44 = 1 iff（符卡 flags&1）且 [spellcard+0x3738] ≥ 60 帧（Bomb::start 的 0x4059bb–0x4059e7 定）
```

| 敌人位置 | 上下文 | 原始值 |
| --- | --- | --- |
| 灵击未发动（`[bomb+0x28]==0`） | — | 0 |
| **内圈** `d² < r²` | 符卡中且 `bomb+0x44==0`（符卡前 60 帧） | **3** |
| | 符卡中且 `bomb+0x44==1` | **38**（`0x26`） |
| | 无符卡且有 boss（`bosses[0] != 0`） | **38** |
| | 无符卡且无 boss | **5** |
| **圈外** `d² ≥ r²` | 符卡中且 `bomb+0x44==1` | 灵梦 **26** / 魔理沙 **23** |
| | 无符卡、有 boss、`bomb+0x44==0` | 灵梦 **20** / 魔理沙 **16** |
| | 其余 | 0 |

### 2.3 缩放链：那一句 `/5` **不是**全局缩放（★★ 2026-09-25 订正）

`0x4059f0` 的返回值在 `EnemyData::on_tick` 里被加上玩家子弹的伤害（`0x4287c8` 同一条累加器
`[esp+0x54]`，`0x42889a` 加灵击那笔），然后：

```
0x40e141: mov ecx,eax              ; ecx = 本帧总伤害（自机弹 + 灵击加成）
0x40e148: mov eax,[player+0x458]   ; ★ player_state
0x40e14e: cmp eax,2 / je 0x40e157  ; state == 2 ⇒ /5
0x40e153: test eax,eax / jne 0x40e167 ; state != 0 ⇒ 跳过
0x40e157–0x40e165: ecx /= 5        ; 魔数 0x66666667 + sar 1
0x40e167: test ecx,ecx / je 0x40e26b  ; 0 ⇒ 整笔跳过
0x40e16f–0x40e17c: [spellcard+0x378c] bit0 == 0 ⇒ 不再除
0x40e17e–0x40e186: [enemy+0x1444] bit 0x8000 == 0（非 boss）⇒ 不再除
0x40e188–0x40e196: ecx /= 5
0x40e19a–0x40e19e: ecx <= 0 ⇒ ecx = 1（**下限 1**）
0x40e1b6: sub [enemy+0x13c0],ecx   ; 敌人 HP
```

**关键：`player+0x458` 是 0..4 的状态机，而「正常游玩」= 状态 1，不是 0。** 证据（全部一手）：

| # | 断言 | 证据 |
| --- | --- | --- |
| 1 | 状态是 0..4 的派发 | `0x425739` 读 `player+0x458` → `0x425744 jmp [eax*4+0x426344]`；表项 = `0x425751` / **`0x425964`** / `0x425aa9` / `0x425d25` / `0x425a96`（详录见 §2.3.1）|
| 2 | **状态 1 = 正常游玩** | 表项 1 的入口 `0x425964` 就是**灵击那一段**（火力 /20 → X 键 → `Bomb::start` → 火力 −20）⇒ 只有状态 1 里按得动 B（逐条见 §2.3.2）|
| 3 | 状态 0 = 入场 / 复活上升 | `0x425751` 每帧写 `player+0x3d0 = 0xbb80 − t·0x1f40/0x3c`（y 480.00 → 0）；满 60 帧后 `0x42590f` 写 1；复活那一路 `0x425cbb` 写回 0（见 §2.3.3）|
| 4 | 状态 2 = 被弹 | `0x426a44 mov [player+0x458],2`，紧跟 `0x426a2a mov ds:0x474c4c,0x1388` |
| 5 | 全部写入点只有 8 处 | `0x406c2d` / `0x41c137` / `0x424d90`(1) / `0x42590f`(1) / `0x425c84`(1) / `0x425cbb`(0) / `0x426a44`(2) / `0x426cff`(4)（全 exe 扫描 `mov …[reg+0x458],…` 无遗漏） |

#### 2.3.1 §2.3 表 1 的原始指令

```
0x425739: 8b 85 58 04 00 00  mov eax,DWORD PTR [ebp+0x458]
0x42573f: 83 f8 04           cmp eax,0x4
0x425744: ff 24 85 44 63 42 00  jmp DWORD PTR [eax*4+0x426344]
0x426344（5 项，u32 LE）: 0x425751 0x425964 0x425aa9 0x425d25 0x425a96
```

#### 2.3.2 表 2：表项 1 = `0x425964` 的逐条（`Player::on_tick` 的灵击段）

```
0x425964: a1 0c 77 47 00        mov eax,ds:0x47770c        ; GUI
0x425971: mov ecx,[eax+0x9eb8] / test / jne                  ; 对话 VM 忙 ⇒ 不干
0x42597f: mov esi,ds:0x4776ec / test [esi+0x28] / jne        ; 灵击进行中 ⇒ 不干
0x425998: 0f bf 0d 48 4c 47 00  movsx ecx,WORD PTR ds:0x474c48  ; 火力
0x4259a6: （魔数 0x66666667）                                ; /20
0x4259b0: test cx,cx / 0x4259b3 je                            ; 商 == 0 ⇒ 不可发动
0x4259b5: f6 05 5c 4e 47 00 02  test BYTE PTR ds:0x474e5c,0x2  ; X 键
0x4259be: push 0x10e                                          ; 无敌 270 帧
0x4259cf: call 0x405860                                       ; Bomb::start
0x4259d4: 66 81 05 48 4c 47 00 ec ff  add WORD PTR ds:0x474c48,0xffec  ; 火力 −20
```

#### 2.3.3 表 3：状态 0 的出入

```
入场（表项 0 = 0x425751 内）：
0x425751: mov ecx,[ebp+0x478]; imul ecx,ecx,0x1f40（=8000）;（魔数 0x88888889）; ; = t·8000/60
0x425770: mov eax,0xbb80（=48000 = 480.00，1/100 px）; sub eax,ecx; mov [ebp+0x3d0],eax  ; y 逐帧上升
0x425902: cmp DWORD PTR [ebp+0x478],0x3c ; jl 0x425d72     ; 未满 60 帧 ⇒ 本帧结束
0x42590f: c7 85 58 04 00 00 01  mov DWORD PTR [ebp+0x458],1 ; ★ 满 60 帧 ⇒ 状态 1（正常游玩）

复活（0x425c90 附近那一块）：
0x425cfc: mov DWORD PTR [ebp+0x3d0],0xbb80   ; y 放回 480.00
0x425cbb: 89 bd 58 04 00 00     mov DWORD PTR [ebp+0x458],edi  ; edi = 0 ⇒ ★ 状态 0（再走一次上升）
0x425ceb: push 0x118 ; lea eax,[ebp+0x430c] ; call 0x405410   ; 无敌 280 帧
```

⇒ 那句 `/5` 是**「入场 / 被弹期间」的伤害惩罚**，**正常游玩不执行**；
⇒ 正常游玩里唯一生效的是**第二条** `/5`：`该敌是 boss` 且**符卡进行中**（下限 1）。
⇒ 由此得到的实得值（每敌每帧）：内圈·无符卡·有 boss = **38**；内圈·符卡·大灵击 = **38/5 = 7**；
内圈·符卡·非大灵击 = **min(1, …) = 1**（先判 0 跳过、后除并保底，顺序见上）；
圈外·无符卡·有 boss = **20 / 16**；圈外·符卡·大灵击 = **5 / 4**。

> ⚠ **这是一次自我订正**：本文件 2026-09-15 那一版把这条写成「`player_state ∈ {0,2}` 才除 5，
> 游玩中恒 0」，并在 🟡 里承认「`player+0x458` 的全部写入点未穷举」。按那个推断实现出来的
> mod 侧伤害**恒为原版的 1/5**（符卡里对 boss 更是 1/25 → 被下限 1 吃掉），
> 实跑手感即「灵击伤害偏小」。上述 5 条证据是 2026-09-25 补做的。

## 3. 消弹半径的真实曲线

```
Bomb::start:  0x4058b5: mov DWORD PTR [ebp+0x3c],0x42000000   ; 32.0f（引导值）
              0x4058bc: mov DWORD PTR [ebp+0x40],0x40800000   ; 4.0f  ← 死字段（全 exe 无人读）
Bomb::on_tick:0x40580e: mov eax,[ebx+0x40]      ; ebx = AnmManager::get_vm_by_id(bomb+0x2c)
              0x405813: mov [edi+0x3c],eax      ; ★ 每帧覆盖成 AnmVm.scale.y（vm+0x40）
              0x405816: call 0x405ac0           ; 然后用它消弹
消弹：        0x405b13: call 0x408100  cancel_circle(bm, &bomb.pos(0x30..0x38), r, …)
              0x405af9: mov eax,[esi+0x3c]      ; arg1 = radius
              0x40818d: fadd [esp+0x28]         ; ★ 判定 = 灵击半径 + 敌弹自身半径×0.5
中心：        0x4057b6–0x4057de: bomb.y 每帧 −0.5 / −1.0（灵梦）/ −1.3（魔理沙）
```

曲线来自 `bullet.anm`（字符串 `0x46cd88` 于 `0x405e26` 被 preload）的脚本 `0x190/0x197`（灵梦/魔理沙）、
`0x1b9`（符卡中 `[spellcard+0x3788] ∈ 0x5d..0x60 ∪ {0x6d}`）：

```
script400: op50 scale(16.0,16.0) 然后 op60 scaleTime(120, 6, 16.0f, 160.0f)
script407: … 目标 112.0f          script441: … 目标 64.0f
（原始字节直读：d.find(struct.pack('<ii ff',120,6,16.0,160.0)) 唯一命中 file 0xd3c70）
```

| 时间 | 半径 |
| --- | --- |
| 帧 0 | 16.0（`scale(16,16)`） |
| 帧 0 → 120 | **线性 16.0 → 160.0**（灵梦；魔理沙 112.0、符卡中 64.0） |
| 帧 > 120 | 保持目标值（脚本不再有 scale 指令） |

🟡 VM 确切寿命未定（脚本尾部有 `ins_6/ins_5` 组成的 80×30 帧计数循环），但**半径在 120 帧就顶满**，不影响曲线结论。

## 4. 震屏：❌ 没有（机械否定）

- `ScreenEffect::operator new` = `0x43c8b0`（`push 0x44` + `call 0x43c970`）的全部 9 个调用点
  （`0x402a61`、`0x404540`、`0x40c25a`、`0x410dad`、`0x416d85`、`0x4186bd`、`0x430eb3`、`0x43134d`、`0x4319d2`）
  **没有一个在灵击闭包内**（闭包脚本证实 `0x43c8b0`/`0x43c970` 均不可达）。
- `ScreenEffect` 是**全屏 alpha 覆盖层**：变体 5 的绘制用写死的矩形 `(32,16)-(416,464)`（`0x43c51a`–`0x43c53a`），
  变体 0 的 on_draw 直接全屏 clear（`mov ds:0x491cfc,0x280` / `0x491d00,0x1e0`）—— **结构里没有平移字段**，做不出震屏。
- 全 exe 名字带 shake 的只有 msg 脚本的 `shakePlayer`（`0x41623c`）/ `shakeEnemy`（`0x41624e`）：那是抖**精灵**。
- 灵击的「冲击感」实际来自 `sub ds:0x474c98, 0x80`（`0x405987`–`0x4059b8`，RANK 旋钮，钳 ±`0x400`）。

⇒ ✅ 一手否定：**TH10 灵击没有任何震屏调用**。❓ 仍未解：TH10 是否存在*任何*独立震屏设施（若有，灵击也不触发它）。

## 5. boss 判据

```
ECL opcode 332 setBoss（0x40ff94）
注册：0x40ffec: or ecx,0x8000 / 0x40fff2: mov [ebx+0x1444],ecx        ; ★ 标志位
      0x40fff8: mov ecx,[ebx+0x14d8]                                   ; zEnemyData+0x49c = full zEnemy*
      0x40fffe: mov [edx+eax*4+0x10],ecx                               ; ★ ENEMY_MANAGER+0x10+4*slot = 该敌
      0x410002: mov [ebx+0x1450],eax                                   ; 记下槽号
反注册：0x40ffb7: mov DWORD PTR [eax+edx*4+0x10],0 / 0x40ffbf: and [ebx+0x1444],0xffff7fff
```

| 消费点 | 指令 | 语义 |
| --- | --- | --- |
| `0x40e184` | `mov eax,[ebp+0x1444]` / `84 e4` / `jns` | boss 受伤害时再 ÷5，且保底 1 |
| `0x405a58` | `mov edx,[ENEMY_MANAGER+0x10]` + `neg/sbb/and 0x21/add 5` | 灵击内圈：有 boss ⇒ 38，否则 5 |
| `0x405a95` | 同上 | 圈外只有在有 boss 时才有伤害 |

⇒ 「有没有 boss」= **`[0x477704+0x10] != 0`**（`bosses[0]`）。**不要用 `Spellcard` 状态代替**：
`SPELLCARD_PTR = 0x4776f4`，`[spellcard+0x378c] bit0` 才是「符卡进行中」；boss 非符阶段仍应算 boss。
ECL 侧相关 opcode（ExpHP 名 + 一手地址）：`331__lifeSet` `0x40ff25`、`332__setBoss` `0x40ff94`、
`335__setInvuln` `0x41001f`、`347__lifeMarker` `0x40fc44`、`345__enmKillAll` `0x410079`。

## 6. 结论表 / 还缺什么

| # | 断言 | 可信度 |
| --- | --- | --- |
| 1 | 门槛 = `火力/20 != 0`（≥1.00），键 = `INPUT & 0x2`，消耗 = −20 单位，无下限钳位 | ✅ 一手 |
| 2 | 决死灵击同门槛同消耗、**不扣信仰**，另设 60/200 帧两个计时器 | ✅ 一手 |
| 3 | 灵击**伤敌人**：`0x4059f0` 被 `0x428630` 回调，值累加后在 `0x40e1b6` 扣 HP，每敌每帧一次 | ✅ 一手 + 机械闭包 |
| 4 | 伤害按「内圈/圈外 × 符卡/无符卡 × 有 boss/无 boss」给 3/5/38/26/23/20/16；**正常游玩不过第一道 `/5`**（那是状态 0/2 的惩罚），只有「该敌是 boss 且符卡中」再 `/5`（下限 1） | ✅ 一手（值表 + 状态机，见 §2.3）|
| 5 | 消弹半径由 anm `scaleTime(120,6,16→160/112/64)` 驱动，每帧覆盖 `bomb+0x3c`；判定 = 半径 + 敌弹半径×0.5 | ✅ 一手；🟡 VM 寿命 |
| 6 | `bomb+0x40`（4.0f）是死字段 | ✅ 一手（12 处 `BOMB_PTR` 引用全核） |
| 7 | **TH10 灵击没有震屏** | ✅ 一手否定（机械闭包 + 结构） |
| 8 | boss 判据 = `enemy+0x1444` bit `0x8000`（opcode 332）与 `ENEMY_MANAGER+0x10` 槽表 | ✅ 一手 |
| 9 | 灵击同时作废当前符卡奖励（`0x40550f` 把 `zSpellcard+0x3790` 清零 + 清激活位） | ✅ 一手 |
| 10 | TH10 的擦弹只有表现（判定 `0x4266b0` 返回 2、script434 火花、`se_graze.wav`），**没有计数器** | ✅ 一手（见 [faith/th10/01 §4](../../faith/th10/01-faith-and-points.md)） |

**还缺什么**

- 🟡 三个伤害缩放条件的语境（`player_state ∈ {0,2}`、符卡宣言 60 帧抑制）。
- 🟡 灵击 anm VM 的确切寿命（尾部 80×30 帧计数循环）。
- ❓ TH10 是否存在独立于 `ScreenEffect` 的震屏设施（例如直接改 D3D 投影）—— 本轮只证明了**灵击不触发**。
- ❓ boss 槽表 `ENEMY_MANAGER+0x10` 的槽位上限（`zEnemyManager` 里该字段只有 `__unknown` 标注）。

## 附录：复现命令

```bash
objdump -d -M intel --start-address=0x425960 --stop-address=0x425d00 local/th10.v1.00a/th10.exe   # 门槛/消耗/决死
objdump -d -M intel --start-address=0x4059f0 --stop-address=0x405b3c local/th10.v1.00a/th10.exe   # 伤害加成 + 消弹
objdump -d -M intel --start-address=0x40e128 --stop-address=0x40e1c0 local/th10.v1.00a/th10.exe   # 敌人 HP 扣减
objdump -d -M intel --start-address=0x40ff94 --stop-address=0x41001f local/th10.v1.00a/th10.exe   # setBoss
grep -nE "call +0x428630\b|call +0x43c8b0\b" local/_scratch/th10_full.asm
python3 -c "import struct;d=open('local/th10.v1.00a/dat/bullet.anm','rb').read();print(hex(d.find(struct.pack('<ii ff',120,6,16.0,160.0))))"
# ★ 2026-09-25 订正用的那五条（§2.3）
objdump -d -M intel --start-address=0x425730 --stop-address=0x425760 local/th10.v1.00a/th10.exe   # player_state 派发
python3 -c "import struct;d=open('local/th10.v1.00a/th10.exe','rb').read();print([hex(x) for x in struct.unpack('<5I', d[0x426344-0x400000-0xC00:][:20])])"  # 跳转表 0x426344
objdump -d -M intel --start-address=0x4258f0 --stop-address=0x425920 local/th10.v1.00a/th10.exe   # 状态 0 → 1（满 60 帧）
objdump -d -M intel --start-address=0x425ca0 --stop-address=0x425cd0 local/th10.v1.00a/th10.exe   # 复活写回状态 0 + y=480
grep -nE "58 04 00 00" local/_scratch/th10_full.asm | grep -E "mov +DWORD PTR \[[^]]*\+0x458\],"   # 全部 8 个写入点
```
