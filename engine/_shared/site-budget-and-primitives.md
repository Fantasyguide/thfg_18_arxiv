# 站点预算与「引擎原语」落地表（跨版本）

> **版本**：跨版本（TH08 v1.00d / TH09 v1.50a / TH12 v1.00b / TH18 v1.00a）。本文所有地址都带版本前缀
> （`th08:0x…` 这种）；本文只谈**引擎侧怎么拦、怎么造**，**UI/动画不在范围内**（用户 2026-10-01 口径）。
> 方法：`tooling/xref.py`（调用者扫描，节布局从 PE 头读、符号表优先取样本目录、其次社区命名金矿
> `local/vendor/th-re-data/data/<版本>/funcs.json`）+ `local/_scratch/thdis.py`（RVA 反汇编）。

## 1. 结论先行：预算 = **全进程共享**（分片实验已证伪 per-patch）

2026-09-17 在 TH18 上做的分片实验：把同一个 patch 的站点声明拆到**两个 archive**（A 18 条 + B 17 条，
合计 **35 = 单档总数**、每条声明都有已导出的 handler），两档全挂后**仍然崩**，崩法与单档一致
（跳进 thcrap 跳板池未写区 ⇒ 执行 `00 00` ⇒ `C0000005h`（Windows 访问违例异常码） 写 `0x1`；或走到 `call [eax+4]`、`eax = 0`）。

⇒ **站点名额是全进程共享的硬资源**，「多 archive 分片」不提高上限。实测 TH18 的安全线在 **22～24 条**之间
（22 条安全、24 条崩；口径 = 该 patch 的 `breakpoints` 条数）。

**三个推论（本目标的设计基础）**

1. **每个作品各有一份额度**（每个游戏是独立进程）⇒ TH08 / TH09 / TH12 各自要**先量自己的上限**，
   不要拿 TH18 的 24 直接套（不同作的站点数/池布局不同）。
2. **「调用」不计入预算** —— 只有 thcrap 需要插断点的**入场时刻**才占名额。所以**能调用引擎函数就别拦入口**：
   这条把很多原语从"必须新站点"变成"零站点"。
3. **观察者/诊断钩子不进正式 patch**（放 `patch-test`），它们和功能抢同一份额度。

## 2. 八个原语的落地表（一手已确认项 + 缺口）

| 原语（作品）| 一手落点 | 站点预算 | 状态 |
| --- | --- | --- | --- |
| **创建使魔 (08)** | `th08:0x450f60 Player::shoot_bullet(s?)`，**唯一调用者** `th08:0x451595`（`Player::tick_shooting_state` 区内）。序言：`byte [arg+3]` 闸 → `[arg+0xe2a74]` 行表指针 + `0x38` 步长；逐行 `call [row+0x28]`；返回 1 ⇒ 武装该 VM（`flags \|= 0x2000`、`state=1`、`vm+0x480=row`、`vm+0x474/+0x478/+0x47c ← row+0x2c/+0x30/+0x34`）| **0 条**（数据驱动：我们只提供行数据 + 六个回调；回调经 `row+0x28..0x34` 数据引用，不占站点）| ✅ 一手（沿用 `engine/player/th08/01-youkai-familiars-first-hand.md`）|
| **关闭发弹口 (08)** | 闸链一手（见 §2.1）：`th08:0x451500` 里四道闸之后才 `call th08:0x450f60` | **优先 0 条**：使魔弹是**我们的行数据**，把 `rate` 设计成永不命中即可；要「随时按住/放开」才需要 **1 条**每帧家务 | ✅ 闸链一手 |
| **创建敌人 (12)** | `th12:0x412990 EnemyManager::allocate_new_enemy`（见 §2.2）：`operator new(0x27b8)` 造敌人对象；调用者 = `EnemyData__run_ecl_256_plus`（ECL opcode 256+，3 处）+ 调试视图 | **0 条**（从每帧家务里直接调用即可；ECL 本来就是它的常规入口）| ✅ 调用链 + 对象尺寸一手；参数语义待核 |
| **收点 (12)** | `th12:0x425c20 ItemManager::on_tick__body`（+ `ItemManager::initialize 0x425940`、`operator new 0x425b10`）| **1 条**（与 TH18 的 `ce_item_frame` 同一形态：一条每帧家务站点承载所有道具事件）| 🟡 落点已定位，道具结构/字段未核 |
| **拦截Z (09)** | ✅ 一手：输入块 P1 `th09:0x4ace18` / P2 `th09:0x4acf34`（stride `0x11c` 双人），按住位图 `+0x2c`；**掩码 1 = Z**（明细见 §2.3 Round 6）| **0 条**（TH18 侧走既有共享站点 `th18:0x45ebfb` 的拦 Z 回调；TH09 侧只读输入位）| ✅ 一手（输入块 + 掩码表）|
| **创建灵魂 (09)** | 候选：`th09:0x412960 BulletManager::shoot_one_bullet`、`th09:0x4130f0 shoot_fairy_bullets_array`、`th09:0x4131c0 shoot_rival_bullets_array` ——即「妖精/对手弹阵」两条数组发射（PoFV 里被打掉的妖精会掉霊）| 待算：若走 `BulletManager` 的常规发射，**0 条**（同 TH12 的 ECL 路线：直接调用）| 🟡 候选已定位，霊的对象身份待核 |
| **蓄力 (09)** | 🟡 落点收窄：**状态计时器 `player+0x303c8`** + 计数域 `player+0x30334`（被置 `0x3c` = 60 帧）；状态体 `th09:0x41cb50` 做缩放渐变 ⇒ **蓄力态候选 = 状态枚举 4**（`th09:0x41e720` 按状态计时器比较）；攻击侧候选 = `ExAttackManager`（见 §2.3）| 待算（预期 **0–1 条**：读状态即可；要拦「不可蓄力」才需 1 条）| 🟡 两条一手在手，待一条反证 |
| **暴风连锁 (09)** | 机制已定（用户口径）：小怪爆炸=暴风 ⇒ 引爆范围内其他小怪 + 消掉所有小弹。**TH18 落地 = 卡 109 已实现**（`pofv.c`）；TH09 侧锚点见 §2.3 | **0 条**（全走既有共享原语：`on_enemy_death` / `on_tick_2` / `ce_damage_rect` / `Bullet__cancel` 调用）| ✅ TH18 侧 0 站点；TH09 侧「原版量」仍 🟡 |

### 2.1 关闭发弹口：`th08:0x451500` 的四道闸（一手逐行）

```
0x451507  cmp dword [0x164d2c8], 0x14   ; 全局帧计数 < 20 ⇒ 本帧直接 return 0（开场保护）
0x451517  ecx = this + 0xe2ac4          ; ★「射击计时器」子对象（Timer）
0x451520  call th08:0x40d3b0            ;   Timer::get_current         = *(int*)(this+8)
0x451525  test eax,eax / jge            ;   返回值 < 0 ⇒ return 0
0x451530  call th08:0x451d50            ;   第二道闸（非 0 ⇒ return 0）
0x45154c  call th08:0x40d3d0            ; ★ Timer::previous_ne_current = *(int*)(this+8) != *(int*)(this)
0x451551  test eax,eax / je 0x45159a    ;   「本帧计时器没推进」⇒ **跳过生成**
0x451555  cmp dword [0x17d6ed4], 0      ;   某模式标志；非 0 时再看下一行
0x45155e  movzx eax, byte [0x164d0b1]   ;   状态字节 == 1 / 7 / 6 ⇒ 也跳过生成
0x451595  call th08:0x450f60            ; ★ 生成使魔弹（行表驱动）
```

**跨版本确认**：`Timer::get_current`（`th08:0x40d3b0`）= `[this+8]`、`Timer::previous_ne_current`（`th08:0x40d3d0`）
= `[this+8] != [this]` ⇒ **「本帧计时器是否推进」是发弹的节拍源**，与 TH11 / TH18 的「短计时器 + 每行 `rate`」
是**同一设计**（那两作的计时器分别反过 `th11:0x4343d1` 区、`th18:0x45eccb` 区）。

**站点核算**：`player+0xe2ac4` 是 Timer 子对象，其 `+8` 就是当前计数 ⇒ 「关闭发弹口」的实现优先级是
**数据层（0 站点）→ 每帧家务（1 站点）**：前者改我们自己的行表；后者才需要一条入场时刻，
在生成器之前把计时器/闸条件按住。

### 2.2 创建敌人：`th12:0x412990` 的对象尺寸与入口（一手）

```
0x412990  push ecx / push ebx / mov ebx,[0x4b43dc] / push ebp/esi/edi
0x41299b  push 0x27b8                    ; ★ 敌人对象 = 0x27b8 字节
0x4129a0  call th12:0x46c9ea             ;   operator new（失败则跳过，返回 0 路径在 0x4129bc）
0x4129ac  mov ecx,[esp+0x18]             ; ★ 唯一的栈参（在 5 个 push 之后 = 第 1 个参数）
0x4129b1  push ecx / mov edi,eax …       ;   把它写进新对象，然后走公共初始化
```

**调用者一手**：`EnemyData__run_ecl_256_plus`（`th12:0x4156dc` / `th12:0x4157aa` / `th12:0x4158a3` 三处 = **ECL opcode 256+**）
+ 调试视图（无关）。⇒ **敌人创建是 ECL 驱动的**：我们既可以照它的样子手工构造参数调用，
也可以直接跑一段 ECL（两条路都**不占站点**）。

**待核（下一轮）**：那 3 处调用点怎么构造参数（在 `EnemyData__run_ecl_256_plus` 里逐行读），
以及 `type-structs-own.json` 里 `Enemy` 的字段表（这次按名字搜没搜到，需要换搜法）。

### 2.3 TH09 的缺口清单与反汇编计划（Round 3 已做第一步）

**TH09 的骨架（社区命名，247 条）**：`Supervisor`(20) / `AnmManager`(15) / **`Timer`(14)** / `BulletManager`(13) /
`Bullet`(11) / `Player`(10) / `EnemyManager`(9) / `Spellcard`(8) / `EffectSys`(8) / `GuiSide`(8) / `AsciiManager`(8) /
`Stage`(7) / `Gui`(7) / `ReplayManager`(7) / `MainMenu`(7)。

**三个缺口的候选锚点（本轮定位）**

| 缺口 | 候选 | 下一步要核什么 |
| --- | --- | --- |
| 创建灵魂 | `th09:0x412960 shoot_one_bullet`、`th09:0x4130f0 shoot_fairy_bullets_array`、`th09:0x4131c0 shoot_rival_bullets_array` | 「霊」到底是哪类对象（`Bullet` 池里的一种类型？还是独立对象？）——先读这三条函数的参数与它们填的池 |
| 蓄力 | `th09:0x41e900 Player::on_tick` 的**状态分派**（对 `player+0x00` 的 case 1/2/4/5）+ `Timer` 一族 + `th09:0x404ac8`（置状态后设 Timer）| 哪个 case = 蓄力中；蓄力量存在哪个 timer/字段 |
| 暴风连锁 | ✅ **机制已由用户口径钉死**（小怪爆炸 ⇒ 暴风 ⇒ 引爆其他小怪 + 消所有小弹）：原候选 `th09:0x4157a0`（EX 攻击面）**撤销** | TH09 侧「敌死 ⇒ 暴风 + 消弹」那个函数（见下方 Round 5 的缺口 ①）|

**Round 4 追加的一手（TH09 玩家侧骨架）**

- `th09:0x403c20 Player::__set_some_state_enum` 是**一行 setter**：`mov [ecx], eax` ⇒ **`player+0x00` = 状态枚举**
  （⇒ 拦截/强制状态的最省实现 = 写这个字段，**0 条站点**；要拦"是谁改的"才需要站点）；
- 它的调用者有 7 处：`th09:0x403c64`/`th09:0x403cc4`/`th09:0x403d2a`（玩家初始化段）、`th09:0x404ac8`、`th09:0x411335 EnemyManager::on_tick`、
  `th09:0x416635 GuiMsgVm::run`（脚本事件）、`th09:0x41cc34 GameManager::initialize`；
- **`th09:0x404ac8` 的形态**（一手）：`call th09:0x403c20`（置状态）→ `mov ecx,[esi+0x78]` + `add ecx,0x303c8` → `push 2`
  → `call 0x401500`（Timer 设置）⇒ **每个状态配一个 Timer**，蓄力计时很可能就在这条线上；
- 玩家侧其余锚点：**`th09:0x41e900 Player::on_tick`**（枚举 switch 在此）、`th09:0x41f030 PlayerBullet::constructor`
  （TH09 自机弹是独立类，与"霊"不是一回事，别混）、`th09:0x41f010 PlayerOption::constructor`、
  `th09:0x41f680 Player::draw_bullets`。

**Round 5 追加的一手：状态分派表 + 弹池布局 + 暴风连锁的落地核对**

- **状态分派已定**（一手）：`th09:0x41e900 Player::on_tick` 里反复出现 `mov ecx,esi` + `call th09:0x435ec0`，
  而 `th09:0x435ec0` = `mov eax,[ecx]; ret`（**纯取值器**，一手）⇒ 这串 `cmp eax,N` 读的就是 **`player+0x00`**。
  取值表（一手）：**case 5 → 直接早退**（`th09:0x41eb03`，整帧不做事）；case 4 → `call th09:0x41e720`（Player 区内未命名）
  + `test eax` 分支；case 2 → `call th09:0x44ab20`；case 1 → 落到 `th09:0x41cb50`；case 4 / 2 的「真」分支汇合到 `th09:0x41cb50`
  再 `call th09:0x41cc80`。⇒ **蓄力/拦截Z 的落点就是这张表上的某个 case**（0/3/6/7 等值未见于本函数，属默认路径）。
- **弹池布局（Round 6 修正后）**：池基址 **`BulletManager+0x1a900`**、**176 槽**、**stride `0x10c4`**、
  槽内 **`+0xdbe` 一个 word = 槽类型/占用**（`th09:0x412960 shoot_one_bullet` 的空槽搜索就是扫它：`0` = 可用、`6` 参与分层）。
  两条独立证据自洽：① 扫描步长 `add ebx,0x10c4`（一手）；② `th09:0x4123c0 BulletManager::reset` 自己把
  `[edx+0x25e1b0] = 池基址`、`[edx+0x25e1b4] = th09:0xd2fc0`（下一池）存下来，而
  `0x1a900 + 176 × 0x10c4 = 0xd2fc0` **分毫不差**（`th09:0x4123c0` 一手 + 算术）。
  ⚠ **更正**：Round 5 曾据 `reset` 里的一个循环写「175 槽 / stride `0xcae`」——那是**过度推断**，
  该循环的步长与弹池不符（疑为别一数组，未定），已按上面两条证据改正。
  管理字段三个：`+0x25e1b0`（池指针）/ `+0x25e1b4`（下一池指针）/ `+0x25e1b8`（常数 5）。
  同一布局在 `th09:0x40c530` 里也被用（`[eax+0x320]`→`[ecx+8]`→`+0x1a900`）。
- **TH18 落地核对（一手，已实现）**：用户口径与卡 109 的实现在语义上**一致**——
  ① 爆炸 = `on_enemy_death`（断点 `th18:0x430240`），只认 `KILLED` + `F_REAL`（`pofv.c:299`/`:300`）；
  ② 暴风 = `do_blast` 里的 `ce_damage_rect`（`pofv.c:207`，**自机侧伤害源 ⇒ 真会伤到小怪**）
  ⇒ 被打爆的小怪**再次触发 `on_enemy_death`** ⇒ **连锁确实是经真小怪传播的**（不是只经自建灵魂池）；
  ③ 消小弹 = 蓝圈自扫 + `th18:0x428e90 Bullet__cancel`（mode 1；判据半径 ≤ 4.0 = 白弹）；
  ④ 卡自己的灵魂池是**第二层**（供掉落/SP 与"灵魂被打爆也放暴风"），**不是原版连锁的必要件**。
- **站点核算 = 0**：`on_enemy_death` / `on_tick_2` 是既有共享回调，`ce_damage_rect` / `ce_cancel_radius` /
  `ce_raise_damage_cap` 是既有 SDK 接口，`Bullet__cancel` 是**函数调用**（调用不占站点预算）⇒
  **暴风连锁"抛得起来"这件事已经由 109 卡本身证明了**。
- **仍缺（不猜，逐条挂账）**：① 原版 TH09「敌死 ⇒ 生成暴风 + 消弹」那个函数（看 `th09:0x410730 EnemyManager::on_tick`
  的死亡分支；它 3 次调 `th09:0x40cc00`，而后者是"数组按索引追加 + 递增"的助手 —— 是否是暴风生成器**未定**）；
  ② 暴风半径 / 连锁窗口 / 白弹阈值的**原版量**（109 现在全是 `❓ 自定` 占位，spec §5.2 已逐条声明）；
  ③ 175 槽池里「小弹 vs 大弹」的分类位（蓝圈判据的原版对应物）。

**Round 6 追加的一手：TH09 输入块（拦截Z 的落点）+ 状态枚举取值 + ExAttack 假说**

- **输入块已定**（一手）：`InputManager` 的方法都是「位图 & 掩码」两行：
  `th09:0x415cb0 is_active` = `*(uint16*)(this+0x2c) & mask`（**按住**）、
  `th09:0x415cd0 is_rising_edge` = `*(uint16*)(this+0x32) & mask`（**本帧按下**）；掩码走栈参数（`ret 4`）。
  两个块：**P1 `th09:0x4ace18` / P2 `th09:0x4acf34`，stride `0x11c`**（PoFV 是双人 ⇒ 两套输入记录）。
- **掩码 1 = Z（射击）**（一手，跨版本同表：TH18 的射击位也是 `0x001`）：`th09:0x41f872` / `th09:0x41f9fa`
  （玩家区）与 `th09:0x42025a` / `th09:0x420552`（`ReplayManager` 录制/回放）都 `push 1` + `call th09:0x415cb0`；
  另有掩码 `0x2`（X）与方向组合 `0x10/0x40/0x50/0x60/0x80/0x90/0xa0`、确认 `0x100`
  （`GameManager::initialize` 那批 = 开局方向组合判定）。
  ⇒ **拦截Z 的原版落点 = 「按住位 & 1」这个判定的下游**；我们在 TH18 侧**不碰输入位**（`th18:0x4ca428` bit `0x001` 只读），
  走既有共享站点 `th18:0x45ebfb` 的 `on_native_shot_suppress` ⇒ **0 条新站点**。
- **状态枚举取值（一手）**：setter `th09:0x403c20` 收**栈参数**；实测取值 =
  **0**（`th09:0x41ccff`，`GameManager::initialize` 初始化）、**1**（`th09:0x41662a`，`GuiMsgVm` 脚本事件）、
  **3**（`th09:0x41cc34` 处「开局倒数 30 帧」之后；`th09:0x404ac8`；玩家区 `th09:0x41e867` 等 6 处）。
  `Player::on_tick` 的分派只对 **1/2/4/5** 走专属路径（3 = 常规对战，走默认路径）。
- **状态计时器 + 蓄力候选**（一手）：case 体 `th09:0x41cb50`（状态 1 直接落到它；2/4 条件成立后也落到它）
  `lea edi,[esi+0x303c8]`（= **玩家状态 Timer**）→ `mov dword [esi+0x30334], 0x3c`（**60 帧**）→ 对
  `player+0xd8`/`+0xdc` 做浮点渐变（另 `lea ebx,[esi+0xc0]`）⇒ 形态像**蓄力态的缩放脉冲**；
  case 4 的条件体 `th09:0x41e720` 正是拿这个 Timer 与常量比 ⇒ **🟡 蓄力态候选 = 状态 4**。
- **蓄力攻击的载体候选**（🟡，Round 3 的候选改挂到这里）：`ExAttackManager`（`th09:0x415340 on_tick`）是
  **链表式攻击对象管理器**：`lea esi,[ebx+0x1c]`（节点基），节点 `+0xc` = 活跃位、`+0x40`/`+0x48` = 两个回调、
  `+0x1c` = next，遍历上限 `0x100`；`th09:0x4157a0 ExAttackManagerItem::constructor` 无直接调用者
  （经分配器/vtable 构造），`th09:0x4157b0` 只被它自己的 `operator new` 调 ⇒ **要定「EX 攻击 = 蓄力攻击」还缺一条**：
  找到"按 Z 长按 ⇒ 生成该管理器节点"的那条链（下一轮）。
- **工具**：本轮的调用点/掩码扫描器与数据引用扫描器先落在 `local/_scratch/`（`thargs.py` / `threfs.py`），
  验证有效后再提升进 `tooling/`（它们把"某函数的参数是什么"变成可批量的一手证据）。

**Round 7 追加的一手：两个全局句柄的归属纠正（ExAttack 假说被否）**

- **纠正 ①**：`th09:0x4a7e0c` **不是** ExAttackManager，而是**效果管理器实例**
  —— 开机在 `th09:0x41b2c6` 一带用 `call th09:0x40d320(2, 0x320)` 分配（对象 0x320 字节）并存入该句柄；
  它的方法 `th09:0x40ccc0`（社区名 `EffectManager::sub_40ccc0_create_something`）签名 = `(this, int 类型, float* 位置缓冲)`。
  引用它的 5 处 = 玩家区 4 处（`th09:0x41d479`/`th09:0x41d5f4`/`th09:0x41ddd5`/`th09:0x41e234`）+ `th09:0x410691`（`EnemyManager::on_tick`）
  ⇒ **那是"生成特效"，不是"生成攻击"**（我上一轮的推测撤销）。
- **纠正 ②**：**ExAttackManager 的句柄 = `th09:0x4a7e3c`**（开机 `call th09:0x4157d0` 之后存入）。
  它的条目生成方法 = `th09:0x4151f0`（`this = [0x4a7e3c]` + 一个**小整数 id**），调用者一手：
  `EnemyManager::ecl_run` 里 6 处，id = `1` / `7` / `8` / `9` / `0x17` / `0x18`，以及 `Enemy::sub_40c530` 里
  `1 - [enemy+0x31c]` 那类派生 id；另有 `th09:0x44xxxx` 区的另一批 **4 参调用**（id `0`/`2`/`3`/`5`/`6`/`0xd`/`0xe`/`0xf`/`0x10`）。
  ⇒ **「EX 攻击 = 玩家蓄力攻击」这条假说被否**：ExAttack 是 **ECL / 攻击 id 驱动**的（敌侧），
  与玩家按 Z 长按无关 ⇒ **蓄力(09) 的载体重新变回缺口**（只留住 `player+0x303c8` 状态 Timer + 状态 4 这条线）。
- **顺带一手**：玩家对象数组 = **`th09:0x4a7d94` + i × 0x38**（开机循环里用 `th09:0x41a5d0` 分配 3 项），
  与 `GuiMsgVm::run` 里 `imul ecx,ecx,0x38` 的用法一致。

**计划**：① 追 `th09:0x41cb50` 的调用上游 + 谁用 `eax=4` 调 setter ⇒ 定蓄力态（含反证：它是"复活无敌"还是"蓄力"）；
② 读 `BulletManager::shoot_*` 三条的参数与写入的池（定「霊」的对象身份）；
③ 追 `th09:0x410730 EnemyManager::on_tick` 的死亡分支 ⇒ 定暴风生成器与它的消弹调用。
每步都要一手（地址 + 字段 + 调用链），并顺手核算站点（能调用就不拦）。

## 3. 兼容契约（草案，待 106–117 全装验收）

- **状态所有权**：原语只**读**引擎状态、只在**引擎自己的收尾之前**落笔；跨卡共享的状态（分数、P 点、
  道具类型改写…）走**声明式层**（TH18 的既有做法：`ce_score_*` / `ce_drop_*` / `ce_full_p_gate_*`），
  每层有**明确的让位阶梯**（TH18 实例：`117 > 113 > 108 > 111`）。
- **站点所有权**：一条站点只属于**一个**承载者；多条卡要用同一时刻 ⇒ 走**一个 handler 派发多个回调**
  （TH18 的 `ce_item_frame` 就是这么用的）⇒ 这是把 25 个入场时刻压进预算的唯一办法。
- **事件广播共享（暴风连锁的实例）**：`on_enemy_death`（`th18:0x430240`）是**一次广播、多卡消费**；
  暴风连锁靠「自己的伤害源打死的小怪**再次触发同一广播**」成链 ⇒ **一条站点同时支撑多条卡的连锁**，不额外花预算。
  ⇒ 全装验收要专门看「同一次击破被多张卡各记一次」是否互相干扰（卡侧只许**读** `ev`：禁止析构敌人 / 改掉落表）。
- **判据（全装验收）**：装上 106–117 全部系统卡后，逐个原语做一次最小验证（能造/能拦/能收），
  并确认没有卡因为"站点被占"而静默失效（TH18 侧有 `sites.py check` 的双向对账 + `FAIL:` 日志可判）。

## 4. 下一步（按代价排）

1. **TH08 两个原语**：把"关闭发弹口"的判定点逐行核掉（`th08:0x451500` 附近），并给出 TH08 的**预算实测方案**；
2. **TH12 两个原语**：核 `allocate_new_enemy` 的参数结构（`type-structs-own.json` 里可能已有 `Enemy`）+ 量 TH12 的预算；
3. **TH09 三个缺口**：写「缺口清单 + 反汇编计划」（先用 `xref.py` 反查"谁读输入/谁生成弾/谁写计时"），
   再逐条一手；
4. 最后做**兼容矩阵**与 106–117 全装验收。

## 关联

- 站点预算的实测过程与崩溃签名：`mods/th18.v1.00a/card-expand/AUDIT.md` §BE-DN、`local/MEMORY.md` §1/§8（本机）；
- TH08 使魔的行结构与六个回调：`engine/player/th08/01-youkai-familiars-first-hand.md`；
- 更新链（谁在什么时候 tick）：`engine/_shared/frame-loop.md`（§4 = TH18 逐条对照）；
- 工具：`tooling/xref.py`、`tooling/crashdump/dmp.py`（+ README）。

## 4. 验证批次台账（每个原语的第 ④ 件）

**一键入口**：`mods/th18.v1.00a/card-expand/native/tools/verify.py`（发布仍**只有** `tools/deploy.py` 一条路，
本脚本只是调用它）。三种用法：

```bash
python3 native/tools/verify.py --list                  # 看台账
python3 native/tools/verify.py storm-chain --deploy    # 先发布、再打印操作单
python3 native/tools/verify.py all --check             # 用户实跑后，读日志判读（退出码 1 = 未通过）
```

**为什么不是"按键触发"**：用户 2026-09-13 口径 = **发布版关闭调试键**（`ANIMAL_DEBUG_KEY 0`）
⇒ 判读改走**日志**：`--check` 只取日志**本次启动段**（分界线 `=== th18_card_expand … ===` 之后），
逐条比对「必需判据 / 旁证 / 反例」。

| 原语 | 卡 | 必需判据（日志）| 缺口 |
| --- | --- | --- | --- |
| 拦截 Z (09) | 109 | `pofv: ctor (T = …` + `pofv: CA L…` | 拦 Z 本身无逐帧日志（靠 CA 行间接证明），可接受 |
| 蓄力 (09) | 109 | `pofv: CA L…`（档位/半径）| 档位数值仍是 `❓ 自定` 占位 |
| 暴风连锁 (09) | 109 | `pofv: blast chain N r …`（N 递增 = 经真小怪传播）| 反例 `blast queue full` 需一起判读 |
| 创建灵魂 (09) | 109 | `pofv: run done - N spirits spawned…`（本轮补的灵魂账，过关/删卡/重开都打）+ `guzei activated` 旁证 | 无（账只在有值时出声）|
| 创建使魔 (08) | 108 | `youkai: run done - N familiars spawned` | 无 |
| 关闭发弹口 (08) | 108 | `youkai: emitter gate ACTIVE - slot …` | 无 |
| 创建敌人 (12) | 112 | `ufo: ce_spawn_enemy(` | 无 |
| 收点 (12) | 112 | `ufo: BREAK color … P + … money absorbed` | 无 |

**总验收（106–117 全装）**：`verify.py all --check` 一次跑完八个原语的判据；另外要确认
**每张卡自己都在场**（装载自检行：`pofv: ctor …`、`trance: load — …` 等）——
少一张卡的判据全绿是**假绿**，所以矩阵里每个原语都要记「哪张卡提供判据」。
