# TH08 妖侧子机（使魔）的**位置与运动** —— 逐机体写死的回调 + 一条够不到的「追敌人」分支

> **版本**：TH08 v1.00d（`th08.exe`，imagebase `0x400000`）。本文裸地址默认属该版本；引用 TH18 须写成 `th18:0x…`。
> **方法**：一手反汇编（`local/_scratch/th08_full.asm`，`objdump -d -M intel`）+ `.rdata` 常量直读
> + 社区反编译 `local/vendor/th08_dec` 当**结构图**（那几个回调在 decomp 里没有函数体，只有名字↔地址）。
> **可信度**：§1–§3 的地址/常量/分支 ✅ 一手；§4 的负结论 ✅（逐条核过引用面）；§5 的「观感」🟡。
> **缘起**：卡 108 的单卡实跑第 1 条「灵梦机体妖子机本身应当追踪上去」（`SINGLE-CARD-TEST.md` §th08 #1）。

## 0. 一句话结论

TH08 的妖侧子机**没有数据表**：进/出低速那一刻，引擎按**机体号**给每个子机装两个回调
（生成 `0x4c7d40`、每帧 `0x4c7e10`），**位置与运动逐机体写在代码里**：

| 机体 | 每帧回调 | 妖侧子机位置 | 运动 |
| --- | --- | --- | --- |
| 0 霊夢・**紫** | `0x44e3a0` | **1 个：(0, −96)** | ★ **弹簧跟随自机**（会滞后） |
| 1 魔理沙・**アリス** | `0x44ea40` | **1 个：(0, −32)** | 每帧直接吸附（无速度） |
| 2 咲夜・**レミリア** | `0x44eb70` | **4 个**：`(−30,−16) (−10,−32) (+10,−32) (+30,−16)` | 每帧直接吸附 |
| 3 妖夢・**幽々子** | `0x44f930` | 2 个（本卡的早苗走 TH16 文，不涉及） | — |

★ 并且：**`0x44e3a0` 里那条「追敌人」的分支在 v1.00d 够不到**（§4）——
所以「追踪」的**可观测含义**是**弹簧跟随自机**，不是追敌人。这一条直接决定 108 该复刻什么。

## 1. 发现：两个回调表 + 装载点

**① 表**（一手：`.data` 直读，`DIFFABLE_STATIC` 的名字来自 `local/vendor/th08_dec/src/Player.cpp:48-51`）

| 表 | 地址 | 形状 | 用途 |
| --- | --- | --- | --- |
| `g_OptionInitCallbacks` | `0x4c7d40` | `u32[8][4]` | **每帧**调（`Player::UpdateMovement` 的 `0x44b0ac` 一带：`(this->*options[k].unk2ec)()`）|
| `g_OptionUpdateCallbacks` | `0x4c7e10` | `u32[8][4]` | **绘制**调（`Player::on_draw`：`(player->*options[i].func)()`）|
| `g_SpiritOptionInitCallbacks` | `0x4c7e00` | `u32[4]` | 机体 3 的**人侧**那一半（TH16 那套，本卡不涉及）|
| `g_SpiritOptionUpdateCallbacks` | `0x4c7ed0` | `u32[4]` | 同上 |

**② 逐字节读出来的表**（`0x4c7d40` / `0x4c7e10`，128 字节各）

| 行 | Init（每帧） | Update（绘制） | 非 0 项数 |
| --- | --- | --- | ---: |
| 0 霊夢 | `0x44e3a0` | `0x44e9e0` | 1 |
| 1 魔理沙 | `0x44ea40` | `0x44e9e0` | 1 |
| 2 咲夜 | `0x44eb70` ×4 | `0x44e9e0` ×4 | 4 |
| 3 妖夢 | `0x44f930` ×2 | `0x44e9e0` ×2 | 2 |
| 4/6 | 0 | 0 | 0（单人模式的空行）|
| 5 霊夢（单人）| `0x44e3a0` | `0x44e9e0` | 1 |
| 7 魔理沙（单人）| `0x44ea40` | `0x44e9e0` | 1 |

⇒ **每帧回调逐机体不同**（0/1/2 各一支），**绘制回调八行共用一支** `0x44e9e0`。

**③ 装载点**（一手）

```
44b0ac: ...  (进低速那一帧)
        memset(opt, 0, sizeof(PlayerOption))
        *(i32*)&opt->unk2ec = g_OptionInitCallbacks[g_PlayerCharacter][i]   ; 每帧
        (i32&)opt->func     = g_OptionUpdateCallbacks[g_PlayerCharacter][i] ; 绘制
        opt->unk2c8 = 1 ; opt->timer2e0.SetCurrent(0) ; opt->unk2d0 = i     ; ★ i = 子机序号
```

- `opt->unk2d0 = i` 就是**子机序号**（0..3）—— 咲夜那支靠它选位置（§3.3）。
- `opt->unk2c8` = 子机状态：**1 = 出生 / 2 = 活动 / 3 = 退休**（三个回调开头的 `cmp` 分派）。
- 绘制回调 `0x44e9e0`（一手逐指令）：`vm.pos = option.pos(+0x2a4) + g_PlayerPos(0x164d2dc/0x164d2e0)`、
  `vm.z = 0.49`，然后 `call 0x463210`（AnmManager 的绘制）⇒ **它只画，不动位置**。

## 2. 发现：机体 0（霊夢・紫）`0x44e3a0`

结构（一手逐指令）：

```
switch (option->state /* +0x2c8 */):
  case 1:   /* 出生 */
      player->anm->SetAndExecuteScriptIdx(option /* AnmVm */, 0x12)   ; 44e3d7 push 0x12
      option->pos (+0x2a4) = player->positionCenter (+0x2b4)          ; 44e3eb..44e407 拷 12 B
      option->pos.y -= 96.0f                                          ; ds:0x4b42c4 = 96.0
      if (option->pos.y < 32.0f) option->pos.y = 32.0f                ; ds:0x4b42cc = 32.0
      option->state = 2 ; player->target (+0xe2abc) = 0
  case 2:   /* 活动：子状态 option+0x2cc */
      idx = option->sub /* +0x2cc */; jump [0x44e755 + idx*4]         ; 表 = {44e484, 44e532, 44e5e0, 44e68b}
      …（0/1/2 = 家用弹簧 `0x44e770`；3 = 追敌人 `0x44e8d0`，见 §4）
  case 3:   /* 退休 */ option->timer2e0 命中 0x10 ⇒ 清 unk2ec / func ⇒ 子机自然消失
```

**家位置**（`FUN_0044e770`，一手逐指令）

```
home = player->positionCenter          ; +0x2b4
home.y -= 96.0f                        ; 0x4b42c4
if (home.y < 32.0f) home.y = 32.0f     ; 0x4b42cc
delta = home - option->pos             ; call 0x4090d0（Float3 减法：out = this − b）
delta *= (K / 16.0f)                   ; call 0x40b470；K = ds:0x4b4338 = 1.0、16.0 = 0x41800000
                                       ;   0x40b470 的式子 = `this *= (ds:0x4b4338 / arg)`
vel  += (delta - vel) * 0.2f           ; 0x4090d0(delta − vel) → 0x409120(×0.2) → 0x410a70(vel += …)
option->pos += option->vel             ; 0x410a70(pos, &vel)
```

⇒ **每帧一步的弹簧**：`vel = vel·0.8 + (goal − pos)·(1/16)·0.2`、`pos += vel`。
它**不是**瞬移：自机一停下，子机还要几十帧才追上（这就是玩家看到的"追踪"感）。

## 3. 发现：机体 1 / 2 的位置

### 3.1 机体 1（魔理沙・アリス）`0x44ea40`

```
case 1: player->anm->SetAndExecuteScriptIdx(option, 0x1d) ; state = 2    ; 44ea6f push 0x1d
case 2: if (player->shotActive == 0) {                                  ; 44ea8d
            option->pos = player->positionCenter
            option->pos.y -= 32.0f                                      ; ds:0x4b42cc = 32.0
        }
case 3: 同上 + 计时器 0x10 帧后退休
```
⇒ **每帧直接吸附**到自机上方 32 px（没有速度、没有滞后）。

### 3.2 机体 2（咲夜・レミリア）`0x44eb70`

```
case 1: player->anm->SetAndExecuteScriptIdx(option, 0x18) ; state = 2    ; 44eba7 push 0x18
        option->pos(+0x2b0) = player->positionCenter
        switch (option->index /* +0x2d0 */)  jump [0x44ee4b + idx*4] = {44ec06, 44ec48, 44ec87, 44ecc6}
```

| `idx` | 分支 | x | y | 另写 `+0x2d8`（朝向角）|
| ---: | --- | --- | --- | --- |
| 0 | `44ec06` | `-= 30.0`（`0x4b4534`）| `-= 16.0`（`0x4b42d4`）| 0 |
| 1 | `44ec48` | `-= 10.0`（`0x4b4390`）| `-= 32.0`（`0x4b42cc`）| `π`（`0x40490fdb`）|
| 2 | `44ec87` | `+= 10.0` | `-= 32.0` | 0 |
| 3 | `44ecc6` | `+= 30.0` | `-= 16.0` | `π` |

⇒ 4 格 = 自机上方一个**扁梯形**（内侧 ±10/−32、外侧 ±30/−16）。`+0x2d8` 是左右朝向（内外侧相反）。

## 4. ★★ 发现：`player+0xe2abc`（"锁定目标"）**全 exe 只被写 0** ⇒ 追敌人分支够不到

**① 客观事实**：把 `th08.exe` 全文件按 `0xe2abc` 的位移字节（`bc 2a 0e 00`）穷举 =
**10 处**，逐条按**指令边界**读（`METHOD.md` 硬规矩 4）：

| 地址 | 指令 | 语义 |
| --- | --- | --- |
| `0x44e455` | `mov DWORD PTR [ecx+0xe2abc],0x0` | **写 0**（霊夢子机出生时清）|
| `0x44e68e` | `cmp DWORD PTR [edx+0xe2abc],0x0` | 读（子状态 3 的门）|
| `0x44e6c7` | `cmp …` | 读 |
| `0x44e6d3` | `mov DWORD PTR [edx+0xe2abc],0x0` | **写 0**（子状态 3 退出时清）|
| `0x44e889` | `cmp …` | 读（`0x44e770` 尾巴）|
| `0x44e8c2` | `mov … ,0x0` | **写 0** |
| `0x44e8ef` | `mov ecx,DWORD PTR [eax+0xe2abc]` | 读（`0x44e8d0`：拿目标指针 → `[target+0x2d88]` 当目标坐标）|
| `0x450279` | `cmp DWORD PTR [eax+0xe2abc],0x0` | 读（`0x450240`：子机弹的"锁定追踪"门）|
| `0x450299` / `0x4502b5` | `mov eax,DWORD PTR [edx+0xe2abc]` | 读（同上，取目标坐标）|

⇒ **两个写点写的都是 0，没有任何一条指令把目标指针写进去**。

**② 推测**：这条"锁定目标"机制在 v1.00d 是**未接线**的（残留代码），
或者它属于另一个构建（本仓的样本与自带 sha256 的 decomp 不是同一个 1.00d 制品）。

**③ 验证**：
- 换一种取址形式再搜（`mov reg,ds:…` 绝对寻址、`[reg+reg*4+disp]`、`[reg+ebp+disp]`）——
  位移字节相同 ⇒ 都会落进上面那 10 条，没有第 11 条；
- **独立手段**：全 exe 搜"目标指针常见来源"（最近敌搜索 `0x438cb0` 一族）——它是 TH18 的函数名，
  TH08 侧没有等价调用点写这个字段；
- **领域常识对照**：IN 的"追踪护符"是**人侧**（`ply00a`）而不是妖侧（`ply00as`）——
  两个读它的地方（子机追敌 + 子机弹锁定）**都在妖侧**，与人侧招牌不冲突，所以"够不到"不违反常识。

**④ 结论（2026-09-22 晚**订正为两段式**）**：
- **代码形状上，追敌这一支是完整存在的**（`0x44e8d0`：目标 → `final_pos + 32` → 同一条弹簧，见 §4a）；
- 但**它的门（`player+0xe2abc`）在本 build 里够不到**（写点只有 3 处、写的都是 0）
  ⇒ 严格地说，**v1.00d 的玩家侧代码不会主动进入那一格** ✅ 高（逐指令穷举 + 独立复核）。
- ★★ **owner 2026-09-22 晚口径（原话）**：「th08 **灵梦妖子机肯定是八云蓝在敌人面前旋转**，甚至包括
  旋转的贴图 and 旋转动画（虽然只是 rotate）」⇒ 与代码形状（`sprite30` + `angleVel` 的旋转段 +
  子状态 3 的家位置 = 敌人坐标）**逐条吻合**，唯一矛盾的是「谁写那个目标指针」。
  按 METHOD 的口径：**一手形状为准、冲突点写清**；卡 108 因此**按一手形状实现**，
  只把「目标从哪来」换成 TH18 上能拿到的量（❓ 自定 = boss → 最近的敌人），见 §4a 与 §5。

**⑤ 证据**：上表的 10 条地址 + `local/_scratch/th08_full.asm`；原始字节搜 `bc 2a 0e 00` 得 10 处。

### 4a. ★★ 追敌那一格的一手全文（`th08:0x44e8d0` + 进出两侧）

| 件 | 一手 |
| --- | --- |
| 子状态表（4 格）| `option+0x2cc`，跳表 `th08:0x44e755` = {`0x44e484`(0) `0x44e532`(1) `0x44e5e0`(2) `0x44e68b`(3)}；0/1/2 = 跟随自机（按 `vel.x` 符号切待机循环 A/B），**3 = 追敌** |
| 进追敌的门（跟随弹簧尾巴）| `th08:0x44e770`：`player+0xe2ac4` 计时到期 + 目标非 0 + 子机出场满 **10** 帧（`0x44e892 push 0xa`）⇒ `SetInterrupt(option, 3)`（`0x44e8a6`）+ 子状态 = 3 |
| 追敌的家位置 | `0x44e8ef mov ecx,[eax+0xe2abc]`（目标指针）→ `add ecx,0x2d88`（`zEnemy::final_pos`，vendor `type-structs-own.json`）→ `fadd ds:0x4b42cc`（**+32.0**）→ `fcomp 32.0` / `test ah,0x5` / `jp` ⇒ **y 夹到 >= 32** |
| 追敌的运动 | 与跟随自机**同一条弹簧**（`0x44e939`–`0x44e9ab`）：`delta = (goal − pos) × (1/16)`；`vel += (delta − vel) × 0.2`；`pos += vel`；`|vel.x| < 0.05 ⇒ vel.x = 0` |
| 退出的门 | `0x44e68b`：目标没了 / `Timer(&player+0xe2ac4)` 归零 ⇒ `player+0xe2abc = 0` + `SetInterrupt(option, 1)`（`0x44e6dd`）+ 子状态 = 0 |
| **旋转**（贴图层）| `player00.anm` script18 标签 3 = `sprite(sprite30)` + `angleVel(0,0,0.62831855f)` + `stop()` ⇒ **36°/帧**；标签 4（反向）全 exe 无人发 ⇒ 死代码 |
| 只有机体 0 有 | `g_OptionInitCallbacks[0][0] = 0x44e3a0`；机体 1/2 的 `0x44ea40` / `0x44eb70` **没有子状态机、也不旋转** |

⚠ **标签号的跨版本差异**：卡 108 的脚本是 TH08 script18 逐条抄进 `pl0X.anm` 的（生成器
`make_youkai_option_art.py` 的 `LABEL_MAP`），而 TH18 引擎只自动发 1/2/3（销毁/出场/收起）
⇒ 原号 3/4 被挪到 **5/6**（引擎不会发）⇒ **卡侧要自己发 5**（`youkai.c` 的 `ce_anm_interrupt(vm, YK_OPT_SPIN_LABEL)`）。
TH18 的中断扫描是**从脚本开头**线性找 `interruptLabel`（`th18:0x4894d0` 取脚本基址 → `0x478823` 起逐条扫），
所以「从旋转段再发 2 回到待机循环」是**可行**的（本文件旧版的 🟡 说法在此订正）。

## 5. 对卡 108 的落点（TH18 侧）

| TH18 现状 | 一手依据 | 处置 |
| --- | --- | --- |
| 妖侧子机的位置用 **TH18 原生 `pl0X.sht` 阵型**（聚焦块 `+0x90`，格号 `T[N−1]+i`）| `th18:0x45d8fc`（引擎给原生子机摆位）| ❌ 与原作无关：灵梦变成**上方 20 px**，原作是 **96 px** |
| 子机位置是**静态偏移**（引擎每帧 `自机 + 偏移`，`th18:0x45bce8`–`0x45bd08`）| `th18:0x45bce8` | ✅ 用「自己积分位置、把 `位置 − 自机` 当偏移写下去」就能复刻弹簧（卡 108 的 `on_option_pose_apply`）|
| **机体 0 的追敌 + 旋转**（owner 第 2 条）| §4a | ✅ 卡 108 已按一手形状落地（`yk_option_chase_goal` / `yk_option_chase_want` + 发旋转标签）；**目标来源** ❓自定，见卡的报告 |

⇒ 卡 108 的落笔（2026-09-22）：机体 0/1/2 改用**本表的 TH08 妖侧阵型**；
机体 0 另加**弹簧积分**（`yk_opt_spring_step`，逐步式与常量照 §2）。
机体 3（早苗 = TH16 文）没有 TH08 一手数据 ⇒ 仍用 TH18 原生阵型（🟡 见报告）。
