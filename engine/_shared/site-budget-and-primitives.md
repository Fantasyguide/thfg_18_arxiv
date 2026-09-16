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
| **拦截Z (09)** | 锚点：`th09:0x415cd0 InputManager::is_rising_edge`（调用者 `GuiMsgVm::run`、`Player::draw_bullets`）+ **`th09:0x403c20 Player::__set_some_state_enum`**（玩家状态机 = 拦截/强制的落点候选）| 待算：拦「射击键是否按下」的判定 ⇒ **1 条**；只**读**输入则 0 条（每帧读同一份状态）| 🟡 两个锚点已定位 |
| **创建灵魂 (09)** | 候选：`th09:0x412960 BulletManager::shoot_one_bullet`、`th09:0x4130f0 shoot_fairy_bullets_array`、`th09:0x4131c0 shoot_rival_bullets_array` ——即「妖精/对手弹阵」两条数组发射（PoFV 里被打掉的妖精会掉霊）| 待算：若走 `BulletManager` 的常规发射，**0 条**（同 TH12 的 ECL 路线：直接调用）| 🟡 候选已定位，霊的对象身份待核 |
| **蓄力 (09)** | **缺口**（同上）| 未定 | ❌ 需自反 |
| **暴风连锁 (09)** | 候选：`th09:0x4157a0 ExAttackManagerItem::constructor`（EX 攻击系统；`Spellcard::*` 8 个函数是符卡/攻击面）| 待算 | 🟡 候选已定位；⚠「暴风连锁」这个说法需要用户确认对应哪个机制 |

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
| 蓄力 | `th09:0x403c20 Player::__set_some_state_enum`（状态机）+ `Timer` 一族（`is_ge`/`is_gt`/`is_eq`/`subframe`）| 状态枚举里哪个值 = 蓄力中；蓄力量存在哪个 timer/字段 |
| 暴风连锁 | `th09:0x4157a0 ExAttackManagerItem::constructor` + `Spellcard::*`(8) | ⚠ **需要用户确认「暴风连锁」指哪个机制**（PoFV 的 EX 攻击？符卡攻击？还是对手弹阵的连锁）——在那之前只登记候选，不猜 |

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

**计划**：① 读 `th09:0x41e900 Player::on_tick` 对 `[player+0x00]` 的 switch ⇒ 枚举取值表（拦截Z / 蓄力 都在这张表上）；
② 读 `BulletManager::shoot_*` 三条的参数与写入的池（定「霊」的对象身份）；
② 再读 `BulletManager::shoot_*` 三条的参数与写入的池（定「霊」的对象身份）；
③ 最后按用户确认的口径核 `ExAttackManagerItem`/`Spellcard`。每步都要一手（地址 + 字段 + 调用链），
并顺手核算站点（能调用就不拦）。

## 3. 兼容契约（草案，待 106–117 全装验收）

- **状态所有权**：原语只**读**引擎状态、只在**引擎自己的收尾之前**落笔；跨卡共享的状态（分数、P 点、
  道具类型改写…）走**声明式层**（TH18 的既有做法：`ce_score_*` / `ce_drop_*` / `ce_full_p_gate_*`），
  每层有**明确的让位阶梯**（TH18 实例：`117 > 113 > 108 > 111`）。
- **站点所有权**：一条站点只属于**一个**承载者；多条卡要用同一时刻 ⇒ 走**一个 handler 派发多个回调**
  （TH18 的 `ce_item_frame` 就是这么用的）⇒ 这是把 25 个入场时刻压进预算的唯一办法。
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
