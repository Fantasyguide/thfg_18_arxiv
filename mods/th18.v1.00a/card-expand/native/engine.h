/* engine.h —— 行为卡能碰的引擎地址 / 偏移。只放一手确认过的，每条带出处。
 *
 * 版本：TH18 v1.00a（imagebase 0x400000）。全是绝对地址（游戏 exe 不重定位：imagebase 固定，
 * 现有 100 处 binhack 也是这么写的）。出处缩写：
 *   OM  = engine/card/th18/01-object-model.md   SM = engine/card/th18/05-shop-and-money.md
 *   HK  = engine/card/th18/03-hooks.md          PL = engine/player/th18/01-position-and-state-timers.md
 *   SDK = ../SDK.md（本会话 Ghidra 一手，AUDIT §O）
 */
#pragma once
#include <stdint.h>

/* ---- 全局 ---- */
#define CE_ADDR_SCORE              0x4cccfc   /* int，封顶 999999999（SM §1）*/
#define CE_ADDR_DIFFICULTY         0x4ccd00   /* int：0 EASY / 1 NORMAL / 2 HARD / 3 LUNATIC / **4 EXTRA**。
                                                 * `GlobalsInner` 首字段区（与 `cards/mof.c` / `season.c` / `senro.c` 同址，只读）；
                                                 * 计分层拿它选身价上下限与 EASY 修正（`0x446b20 cmp eax,4` 的跳表索引）*/
#define CE_ADDR_GRAZE              0x4ccd0c   /* int：本 run 累计擦弹数（`GLOBALS.inner+0x30`）——
                                                 * 计分层的「擦弹倍率」= min(graze × 0.0001, 8.99) 的输入 */
#define CE_ADDR_MONEY              0x4ccd34   /* int（SM §1）*/
#define CE_ADDR_MONEY_TOTAL        0x4ccd30   /* int，只记收入（SM §1）*/
#define CE_ADDR_GLOBALS_INNER      0x4cccdc   /* zGlobalsInner（命 / bomb 的加法函数的 this）*/
#define CE_ADDR_CURRENT_LIVES      0x4ccd48   /* = GlobalsInner+0x6c */
#define CE_ADDR_LIVES_MAX          0x4ccd54   /* ExpHP LIVES_STOCK_cardfed_cap7 = GlobalsInner+0x78：残机上限，CardLife 每张 +1 钳 7 */
#define CE_ADDR_CURRENT_BOMBS      0x4ccd58   /* = GlobalsInner+0x7c */
#define CE_ADDR_BOMB_FRAGMENTS     0x4ccd5c   /* = GlobalsInner+0x80；HUD 炸弹行的第二个参数 */
#define CE_ADDR_MAX_BOMBS          0x4ccd64   /* = GlobalsInner+0x88，CardBomb 每张 +1 钳 7 */
#define CE_ADDR_GAME_THREAD_PTR    0x4cf2e4   /* 非 0 = 在游戏线程（原生即时卡 dtor 的门）*/
#define CE_ADDR_CARD_PRICE_BY_TIER 0x4b35c4   /* int[15]，price_tier → 金钱（SM §「价格表」一手 dump）*/
#define CE_ADDR_MONEY_ITEMS        0x4ccd20   /* int，吃到的金钱道具个数（collect_money_item 0x446d22 inc）*/
#define CE_ADDR_MONEY_BASE         0x4ccd24   /* int：本颗道具的**身价**（单位 = 点 × 100 = 显示值 × 100；
                                                 * 一手 2026-09-13：`0x446b53`–`0x446bf1` 的三条难度支把钳位后的身价写进来，
                                                 * `0x446bf3` 的 `ja` 支又直接读回 ⇒ **站点 `0x446bf9` 处 `ecx` 恒等于它**
                                                 * （sdk.c 的 `BP_ce_item_score_in` 用这条做站点自检）。
                                                 * 同一格也被大力道具 `0x446870` 走一遍（两条收集路径共用这个全局）。*/
#define CE_ADDR_MONEY_BASE_MIN     0x4ccd28   /* int：身价下限（`0x44286e` 写 = 表 `0x4b666c` 值 ×100 ⇒ 1,000,000）*/
#define CE_ADDR_MONEY_BASE_MAX     0x4ccd2c   /* int：身价上限（`0x442898` 写 = 表 `0x4b6654` 值 ×100，逐难度）*/
#define CE_ADDR_CURRENT_POWER      0x4ccd38   /* OM §7；= GlobalsInner+0x5c（spend_power 0x457486 读写）*/
#define CE_ADDR_MAX_POWER          0x4ccd3c
#define CE_ADDR_POWER_PER_LEVEL    0x4ccd40   /* int（= 100）：一档火力，= GlobalsInner+0x64（spend_power 0x457489；Tsukasa 0x410e6d 门槛 = 2 档）*/
#define CE_FN_SPEND_POWER          0x457480   /* GlobalsInner__spend_power：thiscall(GlobalsInner*; amount) ret 4。CURRENT_POWER <= 一档时拒绝（返回 0、不扣）；
                                                 * 否则扣 amount 并钳到一档（永远保留 1.00），返回「档数是否变了」（0/1）。原生调用点：
                                                 * CardTsukasa__c_press 0x410f1c（push [0x4ccd40]; ecx = 0x4cccdc; call）、商店 0x418477、死亡 0x45c154。AUDIT §V */
#define CE_FN_PLAYER_REPOPULATE_OPTIONS 0x45d5e0 /* Player__repopulate_options_and_notify_cards：thiscall(this = PLAYER+0x620; 一个从不读取的栈参) ret 4
                                                 * （尾 0x45dd0b `ret 4`；Tsukasa 0x410f27 `push ecx; lea ecx,[player+0x620]; call`）。按火力档重建自机子机、
                                                 * 尾部广播 on_power_level_change。扣火力后必须调（Tsukasa / 商店都这么做）。AUDIT §V */
#define CE_PLAYER_INNER            0x620      /* zPlayer.inner（repopulate 的 this；首字段就是 pos，所以与 CE_PLAYER_X 同址）*/
/* ---- ★ 判定点几何（2026-09-13 一手；TH18 **四个机体原生统一**）----
 * 一手：`Player__reset` `th18:0x45ae99`–`0x45aed5` 逐条写下面六个字段：
 *   `+0x4799c` = **圆半径（非聚焦）** `0x40400000` = 3.0
 *   `+0x479a0` = **圆半径（聚焦）**   = 3.0
 *   `+0x479a4` / `+0x479a8` = **AABB 半宽/半高（非聚焦）** `0x3fc00000` = 1.5
 *   `+0x479b0` / `+0x479b4` = **AABB 半宽/半高（聚焦）** = 1.5
 * 读点（一手，穷举）：`th18:0x45cc20`（AABB 判定）`0x45cd90`（子弹圆判定）`0x45cea0`（OBB 判定）
 *   —— 全在受击链上，见 `engine/player/th18/03-player-hit-gate.md` §2。
 * ★★ **原生先例（这就是"能改"的证明）**：`CardAya__on_load` `th18:0x40c7b5` 直接写
 *   `[player+0x4799c] = 1.0` / `[+0x479a4] = [+0x479a8] = 0.5` —— 原生卡把自机判定点缩小，
 *   用的就是这几个字段（它只写**非聚焦**那三个）。
 * ⚠ 只有 `Player__reset` 与 `CardAya__on_load` 两处写它们（穷举 xref）⇒ **不是每帧重写**，
 *   所以卡侧写一次就留得住；但 `Player__reset`（关卡开始 / 复活）会覆盖回来 ⇒ 我们**每帧写**最稳。
 * ⚠ 「判定点指示」（聚焦时那个红点）**不吃这些字段**（它有自己的绘制尺寸）⇒ 改判定点不会改指示。*/
#define CE_PLAYER_HITBOX_R_NF      0x4799c    /* float：圆半径（非聚焦）原生 3.0 */
#define CE_PLAYER_HITBOX_R_F       0x479a0    /* float：圆半径（聚焦）  原生 3.0 */
#define CE_PLAYER_HITBOX_HW_NF     0x479a4    /* float：AABB 半宽（非聚焦）原生 1.5 */
#define CE_PLAYER_HITBOX_HH_NF     0x479a8    /* float：AABB 半高（非聚焦）原生 1.5 */
#define CE_PLAYER_HITBOX_HW_F      0x479b0    /* float：AABB 半宽（聚焦）  原生 1.5 */
#define CE_PLAYER_HITBOX_HH_F      0x479b4    /* float：AABB 半高（聚焦）  原生 1.5 */
#define CE_PLAYER_IS_FOCUSING      0x4779c    /* dword：非 0 = 聚焦（`0x45cc4x` 一带按它选上面两套）*/
#define CE_FN_RNG_RAND_DWORD       0x402740   /* Rng__rand_dword：thiscall(zRng*)，无栈参，裸 ret，eax = 32 位随机（两步 16 位生成器拼起来；rng+4 计数 += 2）*/
#define CE_ADDR_REPLAY_SAFE_RNG    0x4cf288   /* zRng REPLAY_SAFE_RNG（ExpHP）：弹幕 / 敌人 / 自机弹 / 原生卡（Suwako、Yachie、Miko）都用它；
                                                 * 0x4cf280 那个是商店 / UI 的非回放 RNG（pick_weighted_random_offer 0x41717b）。gameplay 随机一律走 0x4cf288。AUDIT §V */
#define CE_ADDR_PLAYER_PTR         0x4cf410   /* zPlayer*（ExpHP statics；Nitori/Momoyo 都从这读）*/
#define CE_ADDR_CHARACTER          0x4cccf4   /* int32 角色号 0..3（灵梦/魔理沙/咲夜/早苗）。一手
                                                * `Player__initialize` `0x45a7a3`：`preload_anm(9, (&PTR_s_pl00_anm)[CHARACTER])`
                                                * —— 残像/主炸等按角色分支的表都用它当索引 */
#define CE_ENEMY_DROP_MAIN         0x00       /* zEnemyDrop（enemy+0x623c；0x430510 的 ecx 就是它，0x430499 lea ecx,[edi+0x623c]）：
                                                 * +0x00 main_drop：dropMain / enmCreate 第 6 参的那一颗，引擎在调 0x430510 之前撒（0x430469），调完清零（0x4304aa）*/
#define CE_ENEMY_DROP_COUNTS       0x04       /* +0x04 ex_drop[20]：dropExtra 表，20 个 int32（(type-1)*4）。
                                                 * Enemy__drop_items_and_notify_cards 0x430510 按 type 1..0x13 逐个 spawn（第 20 格不撒），
                                                 * 广播完 vtable+0x30 后 memset(+0x04, 0, 0x50)。AUDIT §S / S10 */
#define CE_ENEMY_DROP_TYPES        20
#define CE_ENEMY_DROP_TIMED_COUNTS 0x54       /* +0x54 timed_ex_drop[20]：**按出场计时的加成表**，同 20 个 int32。
                                                 * ECL opcode 573（社区 eclmap 无名）→ 0x433a4a → FUN_0042d020(drop, type, count)
                                                 * 写 `drop[type*4+0x50]`（type 1 → +0x54）。结算见 CE_ENEMY_DROP_TIMED_TIME */
#define CE_ENEMY_DROP_AREA_X       0xbc       /* +0xbc/0xc0 = **掉落区半径**（float rx / ry）：`dropArea` 的
                                               * handler `0x42d090 movss [ecx+0xbc],xmm1 / [ecx+0xc0],xmm2` 写它们；
                                               * 读点 = 附加掉落循环 `0x430510`（`0x430580`/`0x430591`）⇒ 一次爆
                                               * 多颗时每颗的**落点偏移**就是按这两半径 × 随机角铺开的
                                               * （见 `cards/faith_core.h` 的 `faith_core_scatter_off()`）。*/
#define CE_ENEMY_DROP_AREA_Y       0xc0
#define CE_ENEMY_DROP_AREA_DEFAULT 32.0f      /* 一手：敌机死亡路径自己把 `0x42000000`（= 32.0f）写进上面两个字段
                                               * （`0x430766` / `0x430773`）⇒ 没调 `dropArea` 的敌人也用 32×32。*/
#define CE_ENEMY_DROP_TIMED_TIME   0xa4       /* +0xa4 timed_ex_drop_time（窗口 T，帧）。ECL opcode 574 → 0x433a70
                                                 * → FUN_0042d040(drop, T)：写 +0xa4 并把计时器复位。
                                                 * 结算（0x430510 一手）：`n = ex_drop[i] + timed_ex_drop[i] × (T − timer)/T`
                                                 * ⇒ 出场 T 帧内给满、之后只剩 ex_drop。计时器 = +0xa8（cur 在 +0xac），
                                                 * 由敌人每帧入口 FUN_0042ff80 的 0x430161 段内联 +1。原生普查：T=40..900、
                                                 * 计数 1..20，越大只窗口越长、给得越多（engine/season/th18/01 §1b′）*/
#define CE_ADDR_BOMB_MGR           0x4cf2b8   /* 炸弹管理器*；+0x30 = 「正在放炸弹」（do_bomb 置 1，Bomb__can_bomb_and_deathbomb_check 0x420420 查它；道具磁力判定 0x445bc4 也查它决定吸不吸）*/
#define CE_ADDR_ABILITY_MGR_PTR    0x4cf298   /* zAbilityManager*（OM §4）*/
#define CE_ADDR_BULLET_MGR_PTR     0x4cf2bc   /* zBulletManager*（ExpHP statics；cancel_all 0x4297a9 读）*/
#define CE_FN_PLAYER_DIE           0x45d3a0   /* Player__die：thiscall(zPlayer*)，无栈参、裸 ret。
                                                 * 全 exe 只有 3 处 call（0x45cd27/0x45ce81/0x45d060，全在中弹判定链上）、
                                                 * 0 处 jmp、0 处数据表引用。它**内部**干完「状态→4 + 死亡特效 +
                                                 * 8 帧决死窗 + 末尾广播卡虚表 +0x10」，**不扣命**（扣命在
                                                 * commit_death_and_enter_state2 0x45d090，唯一调用点 0x45c135）。*/
#define CE_FN_PLAYER_DIE_RET       0x45d584   /* 上者**唯一**的 `c3`：跳过整段 die 体时把 retaddr 改到这里
                                                 * （此刻栈上只有调用者返回地址 ⇒ 栈干净）。
                                                 * 一手全链见 engine/player/th18/03-player-hit-gate.md */
#define CE_ADDR_GUI_PTR            0x4cf2e0   /* zGui*（ExpHP）；+0x1b0 = msg（对话 VM，非 0 = 对话中）。C 键门控 0x45c069、Tenshi state0 0x40ea0d 读它 */
/* ★ 2026-09-15：zGui 的标志字 + 「画开局那条收点线指示线」的位（一手 `th18:0x43af0b` 读、`th18:0x40c288` 写）。
 *   全 exe 只有原生卡 22 ITEM_LINE（CardKanako）的 on_load 置它；置上后引擎自己用 `front.anm` script 0x39
 *   把那条线画出来，位置**现读** `player+0x47998`（`0x43afe1`：`y640 = poc×2 − 80`）。
 *   见 engine/player/th18/05-poc-height-and-indicator.md §3。*/
#define CE_GUI_FLAG_WORD           0x194      /* zGui+0x194：标志字（另有 bit 0x100，`0x443947` 读）*/
#define CE_GUI_FLAG_POC_LINE       0x2000     /* bit13：画收点线指示线（原生卡 22 置；**只置不清**）*/
#define CE_ADDR_ENEMY_MGR_PTR      0x4cf2d0   /* zEnemyManager*；+0x198 = enemy_count_real（ExpHP）。== 0 时 C 键与充能都停（0x45c07b / 0x40ea1f）*/
#define CE_ADDR_INPUT_PRESSED      0x4ca434   /* 本帧上升沿；bit 0x400 = C 键（0x45c084）*/
#define CE_ADDR_INPUT_HELD         0x4ca428   /* 本帧按住的位（录制时原始位、回放时 replay 写入；04-active-cards.md §1）。0x008 = 低速 / 聚焦（0x45b3fa 读它）*/
#define CE_INPUT_FOCUS             0x008
#define CE_INPUT_HELD()   (*(uint32_t *)CE_ADDR_INPUT_HELD)
#define CE_ADDR_SHOP_PTR           0x4cf2a4   /* zAbilityShop*：GameThread__on_tick 0x443bed 写、析构 0x417857 清零；敌人 / 弹幕 / GUI 非 0 即冻结（SM §3.5）*/
#define CE_ADDR_TIME_IN_STAGE      0x4ccce8   /* int：GameThread__on_tick 尾 0x443d16 inc（ExpHP TIME_IN_STAGE）*/
#define CE_ADDR_CURRENT_STAGE      0x4cccdc   /* int：= GlobalsInner 首字段（ExpHP CURRENT_STAGE_globalsinner_base）*/
#define CE_ADDR_PRACTICE_STAGE     0x4c5f8c   /* int：练习模式选的关（stage select 0x46769d 写），-1 = 非练习；商店 0x417cc7 读它决定 30 帧自动退 */
#define CE_GT_FLAGS                0xb0       /* zGameThread+0xb0：位 0x20000 = 请求开商店（MSG opcode 36 置 0x440d83；GameThread 0x443b05 测、0x443c17 清）*/
#define CE_GT_REPLAY_PLAYING       0xd0       /* zGameThread+0xd0：非 0 = replay 回放中（商店 0x417cd8、sub_417880 都看它）*/
#define CE_GT_FLAG_OPEN_SHOP       0x20000
#define CE_ADDR_LIFE_FRAGMENTS     0x4ccd4c   /* = GlobalsInner+0x70（ExpHP LIFE_FRAGMENTS）*/
#define CE_ADDR_SPELLCARD_PTR      0x4cf2c0   /* zSpellcard*（ExpHP SPELLCARD_PTR）；GameThread__thread_start 0x44308c 写 */
#define CE_SPELL_FLAGS             0x78       /* bit0 符卡进行中（0x409b10）、bit1 奖励存活（0x42d640，收符卡拿奖励 / Sannyo 碎片看它）、bit3 耐久符卡（ECL 542 → 0x42d650；超时按收符卡算）、bit7 已超时（步进 0x42eff4 置）*/
#define CE_SPELL_FLAG_ACTIVE       0x1
#define CE_SPELL_FLAG_BONUS        0x2
#define CE_SPELL_FLAG_SURVIVAL     0x8
#define CE_SPELL_FLAG_TIMED_OUT    0x80
#define CE_SPELL_BONUS             0x7c       /* int：符卡奖励（0x42a320 写，超时 0x42f00c 清零，收符卡 0x42a780 计分）*/
#define CE_EM_CAN_CAPTURE          0x44       /* zEnemyManager.can_still_capture_spell（ExpHP；超时路径清 0）*/
#define CE_EM_BOSS_IDS             0x48       /* int[4] boss enemy_id（get_boss_enemy_full 0x4237f0 按它走链表）*/
#define CE_EM_BOSS_SLOTS           4
#define CE_EM_ENEMY_LIST_HEAD      0x18c      /* zEnemyList* {entry, next, prev}（ExpHP active_enemy_list_head）*/
/* ---- anm 源（`anmSelect(N)` 选中的那个文件）与它的 **sprite 表** —— ★★ 2026-09-18（110 变绿）----
 * 一手（`AnmLoaded__set_sprite` `0x477b00` 逐条 + `anmSetSprite` 的四分支 `0x436c16`–`0x436c4e`）：
 *   · `N` 存在 `zEnemyData+0x264`（绝对 `enemy+0x1490`），取文件时查
 *     `[[0x4cf2d0] + N*4 + 0x168]` ⇒ **N=2（`enemy.anm`）在 `zEnemyManager+0x170`**
 *     （`0x436c49 mov eax,ds:0x4cf2d0` / `0x436c4e mov ecx,[eax+ecx*4+0x168]`）。
 *   · 选中的对象是 `zAnmLoaded*`：`+0x108` 已装载标志（0 ⇒ `set_sprite` 直接返回 −1）、
 *     **`+0x11c` = sprite 表指针**（整文件一张，按**全局 sprite 号**索引）。
 *   · 表项：`ecx = id*0x11`（dword）⇒ **每项 0x44 字节**；`set_sprite` 读 +0x24/+0x28/+0x2c/+0x30
 *     （UV 矩形）与 **+0x34/+0x38（宽/高两个 float）**；`0x477b24 mov [edx+0x24],eax` 把 sprite 号
 *     写进 `vm+0x24`（= `CE_ANM_VM_SPRITE_ID`）—— 脚本每条 `ins_300(spriteN)` 都会重写它并重取 UV。
 * ★ 用途：**换贴图 = 换 sprite 表项**（110 的「把一部分妖精改成绿妖精」就是整项拷贝同一形状的
 *   绿色变体 12 项；表与一手见 `cards/mof_green_core.h` 与 `REPORT-110-GREEN-FAIRY.md`）。*/
#define CE_EM_ANM_SLOT_BASE        0x168      /* zAnmLoaded* [14]：下标 = N（0/1 被硬编码分支抢占 ⇒ 关卡 anim{} 从 N=2 起）*/
#define CE_EM_ANM_ENEMY            0x170      /* = SLOT_BASE + 2*4 ⇒ `enemy.anm`（N=2）*/
#define CE_ANM_LOADED_READY        0x108      /* zAnmLoaded.is_loaded（0 ⇒ 表还没建好）*/
#define CE_ANM_LOADED_SPRITES      0x11c      /* zAnmLoaded.sprite_table（`zAnmSprite*`）*/
#define CE_ANM_SPRITE_STRIDE       0x44       /* 一个 sprite 表项的字节数（= 0x11 个 dword）*/
#define CE_ANM_SPRITE_WH           0x34       /* 表项里宽/高两个 float 的位移（同形判据读它）*/
#define CE_ENEMY_ID                0x6830     /* zEnemy.enemy_id */
#define CE_ENEMY_DDC_UNUSED        0x6838     /* ★ zEnemy 里的**死字段**（ExpHP `__only_used_in_ddc`）：
                                               * 全 exe 只有一处 `mov [esi+0x6838],0`（`0x42e427`，敌人初始化）——
                                               * 也就是**每个敌人出生时被清零、之后再没人读写**。
                                               * 季节卡拿它当「这个敌人已经掉过季节点」的闩锁（见 season.c）。*/
#define CE_ENEMY_DATA              0x122c     /* zEnemyData 在 zEnemy 里的偏移：enemy+0x6374 = data.interrupts(+0x5148)、enemy+0x14ec = data.time_in_ecl.cur(+0x2c0)、enemy+0x6220 = data.life(+0x4ff4)（步进 0x42ed40 三处交叉核对）*/
#define CE_ED_TIME_ALIVE           0x2d4      /* ★ zEnemyData.time_alive.current（zTimer 的 current 在 +0x4）。
                                               * 一手：敌人每帧入口 `FUN_0042ff80`（`this` = zEnemyData 基址）
                                               * 尾部 `0x430209` `LEA ECX,[ESI+0x2d0]; CALL 0x405990`
                                               *（`Timer::increment`，每帧 +1，随游戏速度 `0x4ccbf0` 缩放；
                                               * 同段 `[ESI+0x50f0]/[ESI+0x5104]` 就是 invuln/no_hitbox 计时器）。
                                               * 绝对偏移 = `CE_ENEMY_DATA + 0x2d4` = enemy+0x1500。季节卡拿它算
                                               * TH16 的掉落计时衰减（`dropSeason(60,8,1)`：出场 60 帧内
                                               * 击破给 8，之后线性降到 1）。*/
#define CE_ED_TIME_IN_ECL          0x2bc      /* zTimer{prev,cur,cur_f,…}：中断槽的超时阈值拿它比（0x42edf5 起：slot.time <= cur → 超时子程序）*/
#define CE_ED_INTERRUPTS           0x5148     /* zEnemyInterrupt[8]：{+0 hp_value, +4 time, +8 sub_life[0x40], +0x48 sub_timeout[0x40]}，stride 0x88；活动槽 = 第一个 hp_value > -1 && time > 0 */
#define CE_ED_INTERRUPT_STRIDE     0x88
#define CE_ED_INTERRUPT_SLOTS      8
#define CE_FN_GUI_UPDATE_LIVES     0x441f10   /* thiscall(gui; lives, fragments, max) ret 0xc：刷 HUD 残机行（死亡 0x45c2xx、商店复原 0x4179xx 都调；一手反汇编 AUDIT O28）*/
#define CE_SE_INVALID              0x10       /* 无效操作（商店买不起同款）*/
#define CE_FN_BULLET_CANCEL_ALL    0x4297a0   /* thiscall(BULLET_MANAGER; 一个从不读取的栈参) ★ret 4（0x429a0e）：全屏消弹（弹 → 点道具 + 音效 0x47）。函数体自己读全局 0x4cf2bc；原生 ECL 消弹 case 0x434d48 `mov ecx,[0x4cf2bc]; push 0; call`（AUDIT O28h′）*/
#define CE_FN_BULLET_CANCEL_RADIUS_AS_BOMB 0x429370 /* stdcall(pos*, mode, max_count, tag) + ★XMM2 = 半径；四个出口都 ret 0x10。命中：state∈{1,2} 且 bullet+0x24==0 且 dist² ≤ (弹半径(+0x658)×0.5 + R)²；每消一发 Bullet__cancel(b, mode) 0x428e90 + counter++；消满 max_count 立即返回（Tenshi 0x40eb13..0x40eb2f，AUDIT O29a）*/
#define CE_BM_CANCEL_COUNTER       0x7a41e8   /* int：cancel_radius_as_bomb 每消一发 ++；Tenshi 每帧调前清零、调后读（0x40eb56）。ExpHP __some_cancel_related_counter */
#define CE_FN_PLAYER_DMGSRC_RECT   0x45dfa0   /* stdcall(center*, angle, 寿命帧, 每次伤害) + ★XMM2 = 宽、XMM3 = 高；ret 0x10；返回 1-based 槽号。矩形伤害源（flags &~6|1）：+0xc 角度、+0x14/+0x18 宽高、+0x7c 累计上限 9999999、+0x80 命中间隔 1。Remilia 0x40f4a6 调 (玩家上方, 0.0, 2, 200) 宽 32（AUDIT O29b）*/
#define CE_PLAYER_DMGSRC_POOL      0x20574    /* zPlayerDamageSource[0x400]，stride 0x9c；flags bit0 active、bit1 圆形（create_45de40）/ 清 = 矩形（engine/player/th18/02-damage-sources.md）*/
#define CE_PLAYER_DMGSRC_STRIDE    0x9c
#define CE_PLAYER_DMGSRC_SLOTS     0x400
#define CE_PLAYER_DAMAGE_CAP       0x47984    /* int：敌人侧 enm_compute_damage_sources 0x45f0f0 把本帧总伤害钳到它（0x45f28b）；GameThread 0x443d3b 每帧从 sht+0x28 复位；Remilia 0x40f48e 抬到 300。本 mod 照 Remilia 在卡 tick 里抬（ce_raise_damage_cap，炎魔爆炸帧；AUDIT V20）*/
#define CE_ADDR_EFFECT_MGR_PTR     0x4cf2c8   /* ★ 2026-09-16m：zEffectManager*（一手：`0x45e1df` 读它、
                                              * 再取 `+0xc` 当 effect.anm 的 zAnmLoaded 传给 `0x407420`）*/
#define CE_OFF_EFFECT_MGR_ANM      0x0c       /* ★ 同上：EffectManager + 0xc = **effect.anm 的 zAnmLoaded** */
#define CE_ADDR_ANM_MANAGER_PTR    0x51f65c   /* zAnmManager*（ExpHP ANM_MANAGER_PTR；interrupt_tree 0x488be3 读）*/
#define CE_FN_ANM_GET_VM_WITH_ID   0x488b40   /* thiscall(ANM_MANAGER; id) ret 4 → zAnmVm* / 0（fast 数组 0x624 步长或 world/ui 链表；AUDIT O29c）*/
#define CE_FN_ANM_INTERRUPT_TREE   0x488be0   /* stdcall(id, n) ret 8：自取 ANM_MANAGER_PTR，找到 VM 写 +0x494 = n 并递归子树（Tenshi 到时长调 (id, 1)）*/
/* ---- HUD 文字（zAsciiManager）。一手 2026-09-08：原生 `CardMomoyo__on_draw` `0x4110a0` 的调用点逐指令读出来的
 * （AUDIT §AC）——它就是「一张卡在 HUD 上画数字」的现成先例，走的也是虚表 `+0x48 on_draw`。 */
#define CE_ADDR_ASCII_MANAGER_PTR  0x4cf2ac   /* zAsciiManager*（th-re-data statics 同名，交叉对上）*/
#define CE_ADDR_UI_SCALE           0x56aca0   /* float：全局 UI 缩放（`DAT_0056aca0`）。1280×960 → 2.0。
                                               * 三个用途：世界坐标→表面 px 的换算、ASCII 记录位置 ×它、
                                               * 以及 ascii 管理器按它选字形文件（>1.6 ascii1280 / >1.1 ascii_960 / 否则 ascii，
                                               * `0x419390`）。`ce_hud_vm_text_set` 用它把 ASCII 坐标换成表面 px。*/
/* ★★ 画字符串**必须**走 `create_string` `0x419b90`：thiscall(mgr, pos[3], text)。
 * 它把环境字体/分组/缩放/对齐抄进记录，之后由 ascii 的绘制趟按分组画。
 * ⚠️ **别用 `drawf_debug` `0x41a110`**（旧的 CE_FN_ASCII_DRAWF）：它 create_string 之后
 * 还会写 `mgr + 0xc0c + num_strings*0x138 = 1`，而这个地址正好是**刚建那条记录的 `+0x120`**
 * （记录基址 = `mgr+0xc24+idx*0x138` ⇒ `0xc0c + 0x138 = 0xd44 = 0xc24 + 0x120`）——
 * 也就是**把字体强制改成 1**，我们设的 font 全部无效（2026-09-10 反汇编坐实）。*/
#define CE_FN_ASCII_CREATE         0x419b90
#define CE_FN_ASCII_DRAWF          0x41a110   /* ⚠️ 会把字体强制成 1；只在「就想要字体 1」时用 */
#define CE_ASCII_COLOR             0x19228    /* 字色 BGRA 一个 dword（`0x411245 mov [ecx+0x1922b], al` = alpha 在 +3）*/
#define CE_ASCII_COLOR_WHITE       0xffffffffu /* Momoyo 画完还原成它（`0x41128a`）*/
/* zAsciiManager 的其余绘制状态（Ghidra 结构体 zAsciiManager，字段名与偏移一手）。
 * 画完必须还原 —— 引擎自己后面的绘制全靠这些环境值。*/
#define CE_ASCII_COLOR2            0x1922c    /* 描边 / 副色 BGRA */
#define CE_ASCII_SCALE             0x19230    /* zFloat2；引擎常用 1.0，小字用 0.6 */
#define CE_ASCII_SHADOW_DARK       0xff000000u /* 黑描边：压在花花绿绿的 HUD 上最清楚 */
#define CE_ASCII_DRAW_SHADOWS      0x19240    /* int：1 = 画描边遍（`create_string` 抄进记录 `+0x124`）*/
#define CE_ASCII_FONT              0x19244    /* int */
#define CE_ASCII_GROUP             0x19248    /* int */
#define CE_ASCII_PAGE              0x2c       /* int：**`zAnmManager.preloaded` 的下标**（`draw_string` 现读）
                                                  * ⇒ 2 = `anm_02_ascii`；**不是** 10 + entry 号 */
#define CE_ASCII_ALIGN_H           0x19250    /* int：**2 = 右对齐**（金钱 / 总分一手确认）；1 是引擎的常态值、实测表现为左对齐；
                                                  * 于是 0 只能是居中 —— 实跑再确认一次（AUDIT §AI）*/
#define CE_ASCII_ALIGN_V           0x19254    /* int：垂直对齐（抄进记录 `+0x134`）。一手 `0x41a2b0`：
                                                  * 0 = 居中（退半个行高）、1 = 顶、2 = 底（退一个行高）；
                                                  * 行高 = `scale_y × 10`（font 2/3）*/
#define CE_ASCII_ALIGN_CENTER      0
#define CE_ASCII_ALIGN_LEFT        1
/* 金钱那行的排版（`FUN_0043cd00` 在 (620, 204) 处设的）：比默认 font 0 大一圈，右对齐。
 * 想让自己的数字看着和金钱是一套，就照抄这三个。*/
#define CE_ASCII_FONT_MONEY        10
#define CE_ASCII_GROUP_MONEY       3
#define CE_ASCII_ALIGN_RIGHT       2
/* Gui 的 HUD 文字坐标空间（`FUN_0043cd00` 里全是字面量）：火力 "%d." 在 (540, 182)、小数 (560, 189) scale 0.6、
 * "/%d." (574, 182)、金钱右对齐 (620, 204)。我们的倍率读数贴在火力左边。 */
#define CE_HUD_POWER_X             540.0f
#define CE_HUD_POWER_Y             182.0f
#define CE_HUD_POWER_COLOR         0xff800000u /* 火力那行的字色（B=0 G=0 R=0x80 A=0xff），照抄以便看起来是一体的 */
#define CE_HUD_MONEY_COLOR         0xff808000u /* 金钱那行的字色（B=0 G=0x80 R=0x80，FUN_0043cd00 在 (620, 204) 处设的），持仓净值贴着它画 */
#define CE_HUD_DMG_X               478.0f      /* 倍率读数：贴在火力左边，同一行。"x1.60" 五个字约 55 px ⇒ 478..533，刚好不压到 540 的火力，也在侧栏里（弹幕区右边缘约 x 416）。**待实跑微调** */

#define CE_ANM_VM_POS              0x5f0      /* float3 实体坐标（Tenshi/Miko/Remilia 每帧写；engine/anm/th18/01-vm-instantiate.md）*/
#define CE_ANM_VM_COLOR2           0x528      /* D3DCOLOR：`color2()` 写的第二个色（渐变条靠它）*/
#define CE_ANM_VM_COLOR1           0x524      /* D3DCOLOR 0xAARRGGBB：Tenshi 0x40eb4c 无命中写 0xffffffff、0x40eb60 有命中写 0xff0080ff（= 蓝 (0,128,255)）*/
#define CE_ANM_VM_LAYER            0x18       /* ★ int32：**绘制层**（世界列表成员按它分桶）。
                                              * 一手（2026-09-16w，子 agent 诊断 + 本卡复核）：
                                              *   · 写入口只有一条 = 脚本的 `layer()` 指令：opcode 304 → handler `0x47b67b`
                                              *     → `AnmManager__set_layer` `0x4066b0`（`mov [ecx+0x18],edx`）；
                                              *     `AnmLoaded__instantiate_vm_to_world_list_back` `0x405bf0` 也会写（`layer < 0x18` 时）。
                                              *   · 读入口 = `AnmManager__render_layer` `0x488260`：`0x4882a1 mov edx,[edi+0x18]`
                                              *     → `0x4882b0 cmp ecx,[ebp+8]` → `0x4882b3 jne` 跳过（只画本趟桶号）。
                                              *   · 游玩区的桶 = **层 12–19**（相机 3）；层 0–2 = 3D 背景那一趟（相机 2）。
                                              *   ⚠ **谁会替 VM 设层**：只有「子机本体」那条路 —— `Player__allocate_option`
                                              *     `0x40a8bc` 无条件写 0xe(=14)；**卡子机弹那条路不会**（`AnmManager__instantiate`
                                              *     `0x407420` 两参、无层参数，且显式保留 `vm+0x18`；空闲池是 `memset` 0）
                                              *     ⇒ **弹脚本自己必须写 `layer()`**，否则弹被画进背景那一趟 = 看不见。
                                              *     先例：零售 `pl01.anm` script5 的 `-1: layer(13)`、本仓 282/128/131 用 13。*/
#define CE_ANM_VM_SPRITE_ID        0x24       /* int32：当前 sprite 号（ExpHP zAnmVmPrefix.sprite_id；残像照自机当下那帧取）*/
#define CE_ANM_VM_FVARS            0x4b4      /* float[4] 脚本变量 %F0–%F3（ExpHP zAnmVmPrefix float_script_vars）；C 写、脚本 scale(%F0, …) 读 */
#define CE_ANM_VM_ROTATION         0x3c       /* zFloat3 rx/ry/rz（zAnmVmPrefix；自机弹每帧更新 `0x45edb0` 写 +0x44 = rz = 弹的实时角度）*/
#define CE_ANM_VM_SCALE            0x54       /* zFloat2 scale x/y：HUD 充能条 0x408a53 写 +0x58 = fill 比例（thpages drawRect：动态尺寸就改 scale）；ExpHP zAnmVmPrefix（AUDIT O29k）*/
#define CE_FN_SCREEN_EFFECT_NEW    0x476060   /* ScreenEffect 工厂：★fastcall(ecx = type, edx = time; start, end, p5, draw_prio) ret 0x10（尾 0x4760a8）。
                                                 * new(0x40) + ScreenEffect__operator_new 0x4760b0：type 1 = 震屏（on_tick 0x475c70：强度按 time 从 start 线性到 end，
                                                 * 每帧用 UI RNG 0x4cf280 在 {0, +I, −I} 里抽 camera_1/camera_3 的 x / y 偏移；到时返回 7 自删，不进 replay 流）。
                                                 * ECL 517 setScreenShake(time, start, end) 的 case 0x434d8d 就是 `ecx=1; edx=time; push 0x5b; push 0; push end; push start; call`。AUDIT §V17 */
#define CE_SCREEN_EFFECT_SHAKE     1
#define CE_SCREEN_EFFECT_DRAW_PRIO 0x5b       /* ECL 517 传的最后一参（type 1 不注册 on_draw，用不到，照传）*/
#define CE_FN_LASER_CANCEL_ALL     0x449090   /* (mode, unused) 两个栈参 ret 8，ecx 不用（函数自己读 LASER_MANAGER 0x4cf3f4）；ECL 消弹 case 紧跟 cancel_all 调 (1,0) / (0,0)，玩家死亡 0x45c3cc 调 (1, 垃圾)；对每条激光调 vtable+0x28(mode, 0)（AUDIT O28h）*/

#define CE_SCORE()        (*(int32_t *)CE_ADDR_SCORE)
#define CE_MONEY()        (*(int32_t *)CE_ADDR_MONEY)
#define CE_MONEY_TOTAL()  (*(int32_t *)CE_ADDR_MONEY_TOTAL)
#define CE_CURRENT_LIVES()  (*(int32_t *)CE_ADDR_CURRENT_LIVES)
#define CE_LIVES_MAX()      (*(int32_t *)CE_ADDR_LIVES_MAX)
#define CE_CURRENT_POWER()  (*(int32_t *)CE_ADDR_CURRENT_POWER)
#define CE_POWER_PER_LEVEL() (*(int32_t *)CE_ADDR_POWER_PER_LEVEL)
#define CE_CURRENT_BOMBS()  (*(int32_t *)CE_ADDR_CURRENT_BOMBS)
#define CE_MAX_BOMBS()      (*(int32_t *)CE_ADDR_MAX_BOMBS)
#define CE_BOMB_FRAGMENTS() (*(int32_t *)CE_ADDR_BOMB_FRAGMENTS)
#define CE_GAME_THREAD()    (*(void **)CE_ADDR_GAME_THREAD_PTR)
#define CE_SHOP_PTR()       (*(void **)CE_ADDR_SHOP_PTR)
#define CE_TIME_IN_STAGE()  (*(int32_t *)CE_ADDR_TIME_IN_STAGE)
#define CE_CURRENT_STAGE()  (*(int32_t *)CE_ADDR_CURRENT_STAGE)
#define CE_PRACTICE_STAGE() (*(int32_t *)CE_ADDR_PRACTICE_STAGE)
#define CE_LIFE_FRAGMENTS() (*(int32_t *)CE_ADDR_LIFE_FRAGMENTS)
#define CE_SPELLCARD()      (*(uint8_t **)CE_ADDR_SPELLCARD_PTR)
#define CE_OWNED_ARRAY()    ((const int32_t *)(CE_ABILITY_MGR() + CE_MGR_OWNED))
#define ce_price_for_tier(t)  (((t) >= 0 && (t) < 15) ? ((const int32_t *)CE_ADDR_CARD_PRICE_BY_TIER)[(t)] : 0)
#define CE_PLAYER()       (*(uint8_t **)CE_ADDR_PLAYER_PTR)
#define CE_BOMB_MGR()     (*(uint8_t **)CE_ADDR_BOMB_MGR)
#define CE_BOMB_ACTIVE()  ((CE_BOMB_MGR() != 0) && *(int32_t *)(CE_BOMB_MGR() + 0x30) != 0)   /* 放弹中（mgr+0x30 布尔；空指针安全。见 CE_ADDR_BOMB_MGR 出处）*/
#define CE_BOMB_TIME      0x38       /* zBomb.bomb_time_and_release_cooldown.current（放弹帧钟，原版道具 V 门拿它比 0x3c）*/
#define CE_BOMB_COLLECTING() (CE_BOMB_MGR() != 0 && *(int32_t *)(CE_BOMB_MGR() + 0x30) == 1 && \
                              *(int32_t *)(CE_BOMB_MGR() + CE_BOMB_TIME) < 0x3c)   /* ★ 原版「放 B 收道具」那一门（`0x445bc4`–`0x445bce`，一手）：+0x30 == 1 且帧钟 < 60 */

#define CE_ABILITY_MGR()  (*(uint8_t **)CE_ADDR_ABILITY_MGR_PTR)
#define CE_BULLET_MGR()   (*(uint8_t **)CE_ADDR_BULLET_MGR_PTR)
#define CE_GUI()          (*(uint8_t **)CE_ADDR_GUI_PTR)
#define CE_ENEMY_MGR()    (*(uint8_t **)CE_ADDR_ENEMY_MGR_PTR)
#define CE_ANM_MANAGER()  (*(uint8_t **)CE_ADDR_ANM_MANAGER_PTR)
#define CE_GUI_MSG            0x1b0       /* zGui.msg：对话中 */
#define CE_GUI_MSG_ACTIVE()   (CE_GUI() != 0 && *(int32_t *)(CE_GUI() + CE_GUI_MSG) != 0)   /* 对话中（指针非 0）；引擎自己的门见 `0x445bd0` 系 */
#define CE_EM_ENEMY_COUNT     0x198       /* zEnemyManager.enemy_count_real */

/* ---- zAbilityManager ---- */
#define CE_MGR_CARD_LIST_HEAD      0x18       /* 表头；首结点在 +0x1c（尾段 lea ecx,[edi+0x18]；on_tick 0x408683 读 +0x1c）*/
#define CE_MGR_CARD_LIST_FIRST     0x1c
#define CE_MGR_OWNED               0xd70      /* int[255]，本 mod 搬过来的（战线 C）*/
#define CE_MGR_RECHARGE_MULT       0xc58      /* float，reset 置 1.0（OM §5）*/
#define CE_MGR_SELECTED_ACTIVE     0x38       /* 选中的主动卡（C 键分派 0x45c090）*/
#define CE_MGR_NUM_TOTAL           0x28       /* 四个计数：recount 0x4080e0 `dec [edi+0x28]` / `inc [edi+0x2c|0x30|0x34]`（AUDIT §W）*/
#define CE_MGR_NUM_ACTIVE          0x2c
#define CE_MGR_NUM_EQUIPMENT       0x30
#define CE_MGR_NUM_PASSIVE         0x34
#define CE_MGR_ABILITY_ANM         0x0c       /* AnmLoaded*：ability.anm（场上特效）。ExpHP zAbilityManager +0x0c；CardTenshi__c_press 0x40ebf0 `mov ecx,[eax+0xc]` 取它起 script 0x1c。★ 第一版写成 0x10 拿到了 abcard，script68 越界成垃圾 VM（AUDIT O24′）*/
#define CE_MGR_ABCARD_ANM          0x10       /* AnmLoaded*：abcard.anm（卡图；HUD/编成/图鉴起脚本后 set_sprite）。ExpHP +0x10 */
#define CE_MGR_ABMENU_ANM          0x14       /* AnmLoaded*：abmenu.anm（编成 / 图鉴 UI）。ExpHP +0x14 */

/* 卡链表结点（= card+0xc）：{+0 card*, +4 next, +8 prev}（0x412e90 插入；0x408690 遍历 mov ecx,[edi]; mov edi,[edi+4]）*/
#define CE_NODE_CARD               0x0
#define CE_NODE_NEXT               0x4
#define CE_NODE_PREV               0x8        /* recount 0x40811b..0x408132 摘链：next->prev = prev; prev->next = next */

/* ---- zCardBaseClass（0x54 字节；case 56 与尾段一手，SDK §2）---- */
#define CE_CARD_VTABLE             0x00
#define CE_CARD_ID                 0x04
#define CE_CARD_ARRAY_INDEX        0x08
#define CE_CARD_LIST_NODE          0x0c
#define CE_CARD_OPTION_ANM_ID      0x1c       /* 装备卡子机的 anm vm id —— Player__allocate_option `0x40a790` 顺手写这里；
                                                 原生 `CardReimu1__operator_delete` `0x40ab20` 在析构时按它删 VM（ExpHP anm_id_for_ingame_effect）*/
#define CE_CARD_ELAPSED_TIMER      0x20       /* zTimer：激活经过帧（每帧 Timer__increment；OM §3 订正）*/
#define CE_CARD_RECHARGE_TIMER     0x34       /* zTimer：充能倒计时（空闲时 Timer__decrement；c_press 门控 +0x38 <= 0）*/
#define CE_CARD_RECHARGE_TIME      0x48
#define CE_FLAG_FROM_SAVE          0x01       /* flags bit0：存档 / replay 装载（mode & 1）；局末 recount 只删 bit0 == 0 的卡 */
#define CE_FLAG_ACTIVE             0x08       /* flags bit3：主动卡（尾段按它入 selected / num_active）*/
#define CE_FLAG_EQUIPMENT          0x40       /* flags bit6：装备卡（recount 0x408164 test al,0x40 → num_equipment）*/
#define CE_FLAG_FIRING             0x20       /* flags bit5：正在释放（HUD 配色，method_40）*/
#define CE_FLAG_ACTIVE_CLEAR       0x46       /* 主动卡 case 清的位（0x411d83 and ~0x46 | 8）*/
#define CE_FLAG_EQUIPMENT_SET      0x44       /* 原生装备卡 case 的写法：flags & ~0x0a | 0x44（allocate_new_card 里 REIMU_OP…NUE_OP 那 14 个 case；case 56 走被动 & ~0x4a | 4）。SDK 绑定时对 category 1 的卡照此改（AUDIT X11）*/
#define CE_FLAG_EQUIPMENT_CLEAR    0x0a
#define CE_FLAG_EQUIP_RESET_BIT1   0x02       /* flags bit1：🟡 语义未确认 —— 只知道原生装备卡在 `method_4C` / `____on_load__2`
                                               * （`CardReimu1` `0x40aad0` / `0x40aab0`）里清它，且它在 EQUIPMENT_CLEAR(0x0a) 里。
                                               * 名字只描述「原生在收起子机时清的那一位」，不主张它是什么。SDK 的 ce_option_drop 照抄。*/
#define CE_CARD_TABLE_ENTRY        0x4c
#define CE_CARD_FLAGS              0x50       /* bit0 存档/replay 装载，bit3 主动，bit5 开火中，bit6 装备 */
#define CE_CARD_OBJECT_SIZE        0x54

/* 基类虚表（终值，case 56 写入）与 21 个槽的实现（本会话读 0x4b4c78，SDK §2）*/
#define CE_ADDR_BASE_VTABLE        0x4b4c78
#define CE_BASE_SLOT_CTOR          0x413010
#define CE_BASE_SLOT_C_PRESS       0x413030
#define CE_BASE_SLOT_METHOD_38     0x4130f0
#define CE_BASE_SLOT_METHOD_3C     0x413130
#define CE_BASE_SLOT_METHOD_40     0x413140
#define CE_BASE_SLOT_OPDELETE      0x411410   /* CardNull__operator_delete(this, flag)，thiscall + 1 栈参 */

/* ---- zPlayer ---- */
/* ---- 卡组容量 / 存档（dev 用；一手：`0x407ec5`..`0x407efe` 的 reset_cards 循环）---- */
#define CE_ADDR_SCOREFILE_PTR    0x4cf41c   /* zScoreFile*（`0x407ec5 mov ecx,[0x4cf41c]`）*/
#define CE_DECK_CAP_OFF          0x5f678    /* scorefile + shottype*4 + 它 = 该槽卡数上限（循环每轮重读）*/
#define CE_DECK_SHOTTYPE_OFF     0xc5c      /* 卡对象 +0xc5c = 当前 shot type */

#define CE_PLAYER_X                0x620      /* float px（PL §1）。★ 是 float3 的基址：+0x624 py、+0x628 pz —— 青眼 / 豌豆射手 /
                                               * 炎魔都在读 [1] [2]，用 `ce_player_pos()` 而不是各自转指针 */
#define CE_PLAYER_Y                0x624
#define CE_PLAYER_STATE            0x476ac    /* PL §3 */
#define CE_PLAYER_FOCUSED()        ((CE_PLAYER() != 0) && *(int32_t *)(CE_PLAYER() + 0x476cc) != 0)   /* 聚焦位 = player+0x476cc（PL §3；空指针安全，同 CE_BOMB_ACTIVE 口径）：每帧 = INPUT_HELD>>3 & 1，dword 写
                                                                              （0x45b414 `89 97 cc 76 04 00` mov [edi+0x476cc],edx；0/1）*/
#define CE_PLAYER_INVULN_TIMER     0x47774    /* zTimer{prev,cur,float}；复活 0x45c35e 置 {0x117,0x118,280.0}，决死救回 60，炸弹各自置（SDK）*/
#define CE_PLAYER_SPEED_MULT       0x477ec    /* float；Player tick 末尾 0x45c702 复位 1.0；移动 0x45b5b6 读。AbilityManager tick(0x16) 先于 Player(0x17) → on_tick_2 里写生效（SDK）*/
#define CE_PLAYER_SPEED_BASE       0x477f0    /* float×2，每帧从 0x5217dc 复制（0x45c717）*/
#define CE_PLAYER_SHT_PTR          0x47940    /* zPlayer.sht_file*：GameThread 0x443d32 `mov eax,[ecx+0x47940]; mov eax,[eax+0x28]` 每帧复位伤害上限 */
#define CE_SHT_MAX_DMG             0x28       /* sht 头 i32 max_dmg：Reimu 90 / Marisa 160 / Sakuya 60 / Sanae 120（engine/sht/th18/01 §5）；古锭刀终符抬到它的 2 倍 */
/* ★ 2026-09-13（卡 113 灵界）：`pl0X.sht` 影像里**组指针数组**的偏移（40 项，解析时已被重定位成绝对指针）。
 *   一手：解析循环 `th18:0x45a830`–`0x45a843`（`mov esi,0xe0` / `add [esi+edx],eax` / `mov ecx,[edx+esi]`）；
 *   主炮 `th18:0x45ebfb` 与装备子机 `th18:0x40a9e7` 都读 `[sht + 0xe0 + idx*4]`。
 *   ⇒ 写一格的写法 = `*(uint32_t*)(sht + CE_SHT_SET_ARRAY + idx*4) = 绝对行指针`
 *     （卡 113 的灵界组就用它 —— 见 `cards/trance.c` 的「灵界主炮组」段）。*/
#define CE_SHT_SET_ARRAY           0xe0
#define CE_PLAYER_DAMAGE_MULT      0x47980    /* float；EnemyManager tick 读后复位 1.0（0x42e02f）——别用，顺序不可控 */
#define CE_PLAYER_ITEM_ATTRACT_SPD 0x47988    /* float：Player__reset {5,30,70,70}，Nitori on_load {10,30,110,110}（ExpHP 名 + SDK）*/
#define CE_PLAYER_ITEM_COLLECT_R   0x4798c
#define CE_PLAYER_ITEM_ATTRACT_RF  0x47990    /* focused */
#define CE_PLAYER_ITEM_ATTRACT_RU  0x47994    /* unfocused */
#define CE_PLAYER_POC_HEIGHT       0x47998    /* ★ 收点线（PoC 高度）float：`Player__reset` 写 128.0（`0x45ae8f`）、
                                              * 原生卡 22 ITEM_LINE（Kanako）on_load 写 224.0（`0x40c275`）—— 全 exe 只有这两个写点，
                                              * 没有每帧写点。**别直接读写它**：走共用件的 `ce_poc_declare_*()` 声明 + `ce_poc_height()` 读生效值
                                              * （`sdk_core.h` 的 PoC 高度层；一手见 engine/player/th18/05-poc-height-and-indicator.md）*/

/* ---- 自机弹池（`Player__tick_bullets` `0x45edb0` 一手：`LEA EDI,[ECX+0x1570]`、512 槽、
 * 循环尾 `ADD EDI,0xf8` ⇒ 步长 **0xf8**；循环里 `EDI = 槽+0x14`，故 `[EDI+0x78]` = 槽+0x8c = 状态）---- */
#define CE_PB_POOL                 0x1570     /* zPlayer + 0x1570 */
#define CE_PB_COUNT                512
#define CE_PB_STRIDE               0xf8
#define CE_PB_VM_ID                0x08       /* 弹的 anm vm id（激光 = 束 VM；tick2 收束时对它发 interrupt 1）*/
#define CE_PB_STATE                0x8c       /* 1 = 活动、2 = 完成（`0x45fc80` 自杀段 / `0x45f6f0` 命中段都写 2）*/
#define CE_PB_PACKED_ID            0xac       /* `PlayerBullet__create` `0x45e35c` 写 `(set<<8)|row`；`tick2`/命中回查都用它解 shooter 行 */
#define CE_PB_OPTION               0xe4       /* 指向发射它的子机 zPlayerOption*（`PlayerBullet__create` `0x45e3ae` 从第 5 个栈参写）*/
#define CE_PB_DAMAGE_IDX           0xb0       /* 伤害源下标（`tick_bullets` `0x45ee6f` 用它算 `PLAYER+0x204d8+idx*0x9c`）*/

/* ---- mode=2 激光的「每槽一发」去重表（`player+0x476fc`，30 项，一手）----
 * `PlayerBullet__create` `0x45e49d` 写当前火力档；`Player__shoot_one_bullet` `0x45e96e` 见
 * 该槽非 0 就整条 `mode=2` row 早退（`0x45e9c6 ret 0x10`）；`tick2` 自杀段 `0x4602c1` 清 0。
 * 引擎自己的收束顺序 = 状态置 2（`0x460296`）→ VM interrupt 1（`0x4602a0`）→ 清这个槽
 * （`0x4602c1`）。少清这一格 = 该子机之后再也发不出 mode=2 激光，直到重开。*/
#define CE_PLAYER_MODE2_LOCK_BASE   0x476fc  /* dword[30]；初始化 `0x45ad54` memset 0x78 字节 */
#define CE_PLAYER_MODE2_LOCK_SLOTS  30
#define CE_SHT_ROW_OPT_SLOT        0x20      /* shooter 行低 4 位 = option 槽；>=0x10 时引擎取高 4 位为去重槽 */
#define CE_SHT_ROW_MODE            0x21      /* 2 = mode=2 激光 */

/* ---- zPlayer 的头两个 anm 指针（`Player__initialize` `0x45a7b8` / `0x45a7c3` 一手：
 * `preload_anm(9, pl0X.anm)` 的返回值同时写进 `+0x0c` 与 `+0x10`）---- */
#define CE_PLAYER_ANM              0x0c       /* AnmLoaded*：**自机自己的 anm**（pl0X.anm，残像用它起脚本）*/
#define CE_PLAYER_ANM_CURRENT      0x10       /* 同上，但开火期间被换成 ability.anm（0x40a9c0），别拿它当玩家 anm */
#define CE_PLAYER_VM               0x14       /* 自机本体 zAnmVm（`Player__on_tick__body` 里 `LEA ECX,[EDI+0x14]`）*/

typedef struct { int32_t prev; int32_t cur; float cur_f; } ce_timer_t;   /* zTimer 前三个字段（Bomb / Card 的写法一致）*/
#define CE_PLAYER_INVULN()   ((ce_timer_t *)(CE_PLAYER() + CE_PLAYER_INVULN_TIMER))
#define CE_RESPAWN_INVULN_FRAMES   0x118      /* 280 */

/* ---- 可从卡里调的引擎函数（调用约定一手：SDK.md / AUDIT §O16）---- */
#define CE_FN_ALLOCATE_NEW_CARD    0x411460   /* thiscall(mgr; id, mode)：mode 0 道具 / 1 存档 / 2 购买 / 3 replay；返回 num_total，-1 = 拒 */
#define CE_FN_CREATE_HUD_LISTS     0x408d00   /* AbilityManager__create_all_card_lists_for_hud：thiscall(mgr; int) ret 4：先按 mgr+0x58 删旧图标 VM，再按三个计数重画三行；分配尾段 mode != 1 时调 (mgr, 0)（AUDIT §W）*/
#define CE_FN_SET_SELECTED_ACTIVE  0x408b00   /* AbilityManager__set_selected_active_card：thiscall(mgr; delta) ret 4，返回 1 = 选中了。
                                                 * delta = -1 且当前选中为空时，从链表**尾部**往回找第一张带 flags bit3（主动）的卡，
                                                 * 写 mgr+0x38 并调 update_selected_active_card_hud 刷图标。找不到就返回 0、保持为空。
                                                 * D 键切卡走的就是它（0x45c0b0）。删掉「当前选中的主动卡」之后靠它重选（AUDIT §W）。*/
#define CE_FN_MARK_OBTAINED        0x418de0   /* fastcall(id, notify)：置解锁位（本 mod 的断点把新 id 转进影子 + side-car）*/
#define CE_FN_TABLE_GET            0x407d70   /* fastcall(id) → zTableCardData*；未命中回落 NULL 行 */
#define CE_FN_SHOP_PICK_RANDOM     0x416f50   /* fastcall(out*, tier_lo; tier_hi, exclude[], n)：商店随机池抽一张（未拥有、本关可用、按权重、游戏 RNG）；返回非 0 = 抽到，*out = 表行 */
#define CE_FN_TIMER_DECREMENT      0x409750   /* thiscall(zTimer*; 一个未用栈参) ret 4：prev = cur, cur_f -= 游戏速度, cur = (int)cur_f */
#define CE_FN_TIMER_INCREMENT      0x405990   /* thiscall(zTimer*; 一个未用栈参) ret 4：同上方向相反。★ 调用方必须压那 4 字节 */
#define CE_SE_EXTEND     0x11   /* 音效 id（engine/_shared/th18-sound-table.md 一手）*/
#define CE_SE_BOMB       0x2c
#define CE_SE_CARDGET    0x2e
#define CE_SE_PIN00      0x39   /* 收点结算钟声：TH14 v1.00b 原版结算同 id 同 wav 同调校（AUDIT §Z17）*/
#define CE_SE_RELEASE    0x4d   /* Tenshi 发动 */
#define CE_SE_CHANGEITEM 0x4e   /* 切主动卡 */
#define CE_SE_TROPHY     0x4f
#define CE_FN_PLAY_SOUND           0x476c70   /* stdcall(id) + xmm2 = 世界 x（声像）；ret 4 */
#define CE_FN_ADD_LIFE             0x4575f0   /* thiscall(GlobalsInner*)，无栈参，裸 ret：CURRENT_LIVES+1 钳 LIVES_MAX、清碎片、音效 0x11、起特效、extend 计数 +1。CardLife/Mokou dtor 调它。AUDIT O26 */
#define CE_FN_GUI_UPDATE_BOMBS     0x4420e0   /* thiscall(gui; bombs, fragments, max) ret 0xc：刷 HUD 炸弹行。
                                                 * 与残机那个 0x441f10 完全对称；consume_bomb 0x457501 与每关开场 0x442798 都这么调。AUDIT §T */
#define CE_FN_DO_BOMB              0x420360   /* void do_bomb()：无参 cdecl，plain ret。置 +0x30、扣炸弹（0x4574d0 ★钳 0 不会变负）、
                                                 * se 0x2c（声像取 PLAYER+0x620）、CALL vtable+4 起各自机的炸弹。返回 0 = 放出去了、-1 = +0x30 或 +0xa0 非零被拦。
                                                 * 引擎调用点 Player__on_tick__body 0x45c051（case 1）与 0x45c2c3（决死窗口）。AUDIT §R */
#define CE_FN_ADD_BOMB             0x457690   /* thiscall(GlobalsInner*; 一个未读栈参) ★ret 4：CURRENT_BOMBS+1 钳 MAX_BOMBS、清碎片、音效 0x2e、刷 HUD。CardBomb dtor 调它。AUDIT O26 */
#define CE_FN_ANM_INSTANTIATE_UI_BACK    0x409670 /* AnmLoaded__instantiate_vm_to_ui_list_back：thiscall(AnmLoaded*; int* out_id, int script, float* pos3 或 NULL, int 未读) ★ret 0x10（第四个栈参不读但要压；反汇编 0x409746）。HUD 卡图标就是这条链（0x408de0 内联版）。AUDIT §X */
#define CE_FN_ANM_SET_SPRITE             0x477b00 /* AnmLoaded__set_sprite：thiscall(AnmLoaded*; vm, sprite_idx) ret 8（两个出口都 ret 8；anm/th18/01 §0）。HUD 图标 0x408fxx 用它换卡图 */
#define CE_FN_ADD_LIFE_FRAGMENT          0x457570 /* GlobalsInner 加 1 个残机碎片：thiscall(GlobalsInner*; 一个未读栈参) ★ret 4（两个出口 0x457585 / 0x4575e2）。CURRENT_LIVES >= LIVES_MAX 时清零不加；满每命碎片数进位 0x4575f0；刷 HUD 0x441f10。调用方：CardLifeFragment ctor、CardNarumi 每关、道具 type 0x10。AUDIT §X */
#define CE_FN_ANM_INSTANTIATE_WORLD_BACK 0x405bf0 /* AnmLoaded__instantiate_vm_to_world_list_back：thiscall(AnmLoaded*; int* out_id, int script, int layer, void** out_vm) ret 0x10。建 VM、实体坐标 (0,0,0)、layer<0x18 时写 vm+0x18、AnmVm__run 一帧、挂 world 列表尾；内部自己进临界区。AUDIT O24 */
#define CE_FN_ANM_SET_SPRITE       0x477b00   /* AnmLoaded__set_sprite：thiscall(AnmLoaded*; vm, sprite_idx) ret 8（备查，特效脚本自己 sprite() 就不用）*/
#define CE_FN_ANM_DELETE_BY_ID     0x488cf0   /* stdcall(anm_id) ret 4：按 id 标记删除 VM 及其子树（Tenshi 收尾用；一次性脚本自 delete() 就不用）*/
/* ---- 装备卡：子机（option）与 SHT（engine/sht/th18/，engine/card/th18/03-hooks.md §5）---- */
#define CE_FN_PLAYER_ALLOCATE_OPTION 0x40a790 /* ★thiscall(card; card, off_x, card, off_y, ability_script) ret 0x14
                                                 —— 五个栈参里第 1、3 个是 card 自己（`CardReimu1__on_power_level_change` 0x40aae0
                                                 的压栈序：push script / push off / push ecx / push off / push ecx）。
                                                 从 PLAYER->inner.equipment 的 12 个槽找空位；最后一参是 **ability.anm** 的脚本号。
                                                 返回 zPlayerOption*（满了返回 NULL）；顺手把子机的 anm vm id 写进 card+0x1c。AUDIT U1 */
#define CE_FN_ALLOCATE_NEW_ENEMY 0x42d7d0 /* ★EnemyManager__allocate_new_enemy：thiscall(mgr; name, spawn*, unused) ★ret 0xc
                                              —— 一手：`0x42d7d6 push 0x683c` 分配 → `0x42d7ed call 0x42e100`
                                              Enemy__constructor(名字表 = `enemy+0x1218` 二分查 ECL 子程序，未命中写 -1)
                                              → 拷 spawn 字段 → `0x42d9e7` 挂 `mgr+0x190` 链表、`0x42d9ef inc [mgr+0x198]`、
                                              `0x42d9ed mov eax,esi` **返回敌人指针**。原生调用点 `0x40ff46`（CardLilyWhite：
                                              `push 垃圾 / mov ecx,mgr / push &spawn / push 名字`）。★ 死亡栈里禁止调（DEATH §2.4）。*/
/* spawn 结构（0x54 字节；一手字段表 plans/2026-09-10-ufo-card-feasibility.md §1.1）*/
#define CE_SPAWN_POS        0x00   /* float3 出生世界坐标 → `enemy+0x12b4`（zEnemyData.abs_pos）*/
#define CE_SPAWN_SCORE      0x0c   /* int32 score_reward → `enemy+0x621c`*/
#define CE_SPAWN_MAIN_DROP  0x10   /* int32 主掉落道具类型（0 = 不掉落）→ `enemy+0x623c`（zEnemyDrop.main_drop）*/
#define CE_SPAWN_LIFE       0x14   /* int32 → `enemy+0x6220/0x6224/0x6228`（current/maximum/remaining）
                                    * ⚠ >= 1000 时 `0x42d8f6` 会置 `flags_lo |= 0x40000000`（锁定黑名单，读点未定位）
                                    * ⇒ 自建载体一律钳 < 1000（108 spec §3.5、youkai_core.h 的 yk_clamp_spawn_life）*/
#define CE_SPAWN_FLAG_A     0x18   /* int32，仅 bit0 → `flags_lo |= 0x80000`*/
#define CE_SPAWN_FLAG_B     0x1c   /* int32，仅 bit0 → `flags_lo |= 0x4000000`*/
#define CE_SPAWN_ECL_I      0x20   /* int32[8]  → ECL `$I0..$I7`*/
#define CE_SPAWN_ECL_F      0x40   /* float[4]  → ECL `$F0..$F3`*/
#define CE_SPAWN_PARENT     0x50   /* int32 parent_enemy_id → `enemy+0x6834`*/
#define CE_SPAWN_SIZE       0x54

#define CE_FN_TICK_SHOOTERS_FOR_CARD 0x40a9c0 /* stdcall(option, short_timer, long_timer, sht_set_index) ret 0x10：
                                                 取 *(int*)(PLAYER+0x47940)+0xe0+idx*4 那组 shooter，逐条判
                                                 `timer % fire_rate == start_delay` 就发；弹从 option+0x5c/+0x60 出膛。
                                                 ★ fire_rate == 0 会整数除零（这条路径没有零分支）。
                                                 开火期间把 player+0x10 换成 ability.anm，所以 shooter 的 anm 字段按 ability.anm 解释 */
#define CE_OPT_IN_USE              0x00       /* zPlayerOption：非 0 = 占用中（allocate 找空位看它）*/
#define CE_OPT_POS_X               0x5c       /* 定点数，× 1/128 = 像素（`0x40a9c0` 出膛坐标就取这两个）*/
#define CE_OPT_POS_Y               0x60
#define CE_OPT_OFF_NF_X            0x64       /* 非聚焦位置偏移 x（定点 ×1/128）：`Player::allocate_option` `th18:0x40a80a` 写 0，
                                               * 原生卡 `th18:0x40b8c1` 写它 → 卡可以自己摆阵型 */
#define CE_OPT_OFF_NF_Y            0x68       /* 非聚焦位置偏移 y（allocate_option 从第 2 个栈参 ×128 写，`th18:0x40a7fc`）*/
#define CE_OPT_OFF_F_X             0x6c       /* 聚焦位置偏移 x（allocate_option 写 0，`th18:0x40a811`）*/
#define CE_OPT_OFF_F_Y             0x70       /* 聚焦位置偏移 y（allocate_option 从第 5 个栈参 ×128 写，`th18:0x40a805`）*/
#define CE_OPT_ANM_ID              0xb0       /* 子机的 anm vm id */
#define CE_OPT_OWNER_INDEX         0xd0       /* = 建它那张卡的 card+0x08（array_index）；用来认领「这个槽还是我的吗」*/

/* ---- ★★ 2026-09-16：**原生子机数组**（「自机本体的那 4 格子机」；与卡申请的池 `inner+0x410` 是两回事）
 * 一手（本轮在 `th18.exe` 上逐指令核对；回流见 `engine/sht/th18/03-func-tables-timers-and-dead-fields.md` §7）：
 *   · **数组**：`inner+0x50`（= `zPlayer*+0x670`），**4 格**，stride **0xf0**
 *     （`Player__repopulate_options_and_notify_cards` `0x45d5e0` 的 `lea ebx,[ecx+0x50]` + `0x45d81e mov edi,4`；
 *      每帧跟随更新 `FUN_0045bc90` 的两处调用 `0x45b8ea push 4`（原生 4 格）/ `0x45b8fa push 0xc`（卡池 12 格））；
 *   · **每格字段**：`+0x00` 状态（2 = 已启用）、`+0x5c/+0x60` 出膛点（定点，跟随更新每帧写）、
 *     `+0x64/+0x68` 非聚焦偏移、`+0x6c/+0x70` 聚焦偏移、`+0xb0` 子机 VM（普通形）、
 *     `+0xb4` 子机 VM（**满火力形**）、`+0xd0` 格序号；
 *   · 两张子机贴图脚本表（按机体）：普通形 `0x4b7020[char]` = {15,17,12,10}、
 *     满火力形 `0x4b7030[char]` = {16,18,13,12}；
 *   · **谁从这 4 格出弹**：主炮行循环 `0x45ec4c`–`0x45ec5d`：`slot = row[0x20]`，`slot == 0` ⇒ 膛口传 **NULL**
 *     （自机本体），否则传 `PLAYER+0x580+(slot&0xf)*0xf0` ⇒ slot 1..4 = 这 4 格；
 *     随后 `Player__shoot_one_bullet` `0x45e930` 拿它当膛口（读 `+0x5c/+0x60`）。
 *   · 卡申请的池**正好接在它后面**：`inner+0x410` = `zPlayer*+0xa30`（`Player__allocate_option` `0x40a7a4`）
 *     ⇒ **两个指针区间不相交** —— 这就是 SDK 分辨「这一发是不是原生子机的弹」的判据（见 `sdk.c` 的
 *     `BP_ce_native_option_muzzle`）。*/
#define CE_NATIVE_OPT_COUNT        4
#define CE_NATIVE_OPT_STRIDE       0xf0
#define CE_NATIVE_OPT_INNER_BASE   0x50       /* inner 口径的数组基址 */
#define CE_NATIVE_OPT_PLAYER_MUZ0  0x670      /* zPlayer* 口径：slot 1 的膛口指针 = 0x620 + 0x50 */
#define CE_NATIVE_OPT_PLAYER_END   0xa30      /* zPlayer* 口径：数组尾 = 0x670 + 4×0xf0 = 卡池起点 */
#define CE_NATOPT_STATE            0x00       /* int：2 = 本格已启用（repopulate 写）*/
#define CE_NATOPT_POS_X            0x5c       /* 定点出膛点（跟随更新每帧写）*/
#define CE_NATOPT_POS_Y            0x60
#define CE_NATOPT_NF_X             0x64       /* 非聚焦偏移（`0x45d902` 从 sht 的 `+0x40` 块 ×128 写）*/
#define CE_NATOPT_NF_Y             0x68
#define CE_NATOPT_F_X              0x6c
#define CE_NATOPT_F_Y              0x70
#define CE_NATOPT_VM_MAIN          0xb0       /* 普通形子机 VM id（脚本表 `0x4b7020`）*/
#define CE_NATOPT_VM_MAX           0xb4       /* **满火力形**子机 VM id（表 `0x4b7030`；`CURRENT_POWER >= MAX_POWER` 才建）*/
#define CE_NATOPT_INDEX            0xd0       /* 格序号 */
#define CE_OFF_OPT_LAST_LEVEL      0x470d8    /* inner：**上次按哪一档建过子机**（repopulate 的短路缓存；
                                               * 读点 `0x45d84d cmp [esi+0x476f8],eax`（esi = PLAYER）；
                                               * 写 -1 ⇒ 下一次一定重走「重建 / 清空」那条路）*/
/* ⚠ `Player__shoot_one_bullet` `0x45e930`（stdcall(player*, packed_id, timer, pos*, option*) **ret 0x10**；
 *   packed_id = (set<<8)|行号；`option` = 膛口（NULL = 自机本体）；返回 0 = 发了 / -1 = 没发 —
 *   **调用方不检查返回值**；一手调用点三处：主炮行循环 `0x45ec73`、卡路径 `0x40aa80`、
 *   `0x410622`）**故意不给宏**：本 mod 不直接调它（SDK 是在它的入口下断点拦发，用 sites.py 的 RVA）；
 *   而 117 的 `cards/animal.c` 已经有一份同地址的局部宏（`CE_FN_PLAYER_SHOOT_ONE_BULLET`），
 *   在这里再定义一个会让那份 `-Wmacro-redefined` ⇒ **地址只在注释里**。*/
#define CE_SUBPIXEL_TO_PIXEL       (1.0f / 128.0f)   /* `0x4b908c` */
#define CE_PLAYER_AIM_ANGLE        0x479cc    /* float：卡把瞄准角写这里，shooter 的 func_on_init = 5（`0x4612d0`）
                                                 在建弹时用它覆写 bullet+0x64。CardAlice `0x40b5fa` 同款用法 */
/* ---- zEnemy（`CardAlice__on_shoot` 0x40b4e0 与最近敌人搜索 0x438cb0 一手）---- */
#define CE_ENEMY_DROP              0x623c     /* zEnemy.drop（zEnemyData+0x5010；SDK 的 on_enemy_drop_pre 拿到的就是这个指针）*/
#define CE_ENEMY_OWN_BOSS_ID       0x636c     /* zEnemyData.own_boss_id（非 0 = 关底/道中 boss）*/
#define CE_ENEMY_ID                0x6830     /* zEnemy.enemy_id：**每个敌人实例唯一**、单调递增。
                                                 * 一手 `Enemy__constructor` `0x42e3f0`：
                                                 * `id = mgr[0x90]; mgr[0x94] = mgr[0x90]; mgr[0x90] = id + 1`
                                                 * （`0x42e3fc`–`0x42e408`）⇒ 内存复用也不会撞号，
                                                 * 拿去重（同一个敌人的多次掉落回调）比指针可靠。*/
#define CE_ENEMY_POS_X             0x1270     /* = data(+0x122c).final_pos(+0x44).pos。★ 同为 float3 基址；用 `ce_enemy_pos(e)` */
#define CE_ENEMY_LIST_GUARD        4096       /* 敌人链表遍历的护栏（坏链表不转死）；ce_enemy_by_id / CE_ENEMY_FOREACH 共用 */
#define CE_ENEMY_POS_Y             0x1274
#define CE_ENEMY_FLAGS             0x635c
/* ★ 2026-09-13 第五轮（113 的「Boss 随血量掉灵」= TH13 通道 A 的落点）：伤害施加点在
 *   `EnemyData::step_game_logic` `0x42ed40` 的 `0x42f2cb add [ebx+0x5008],esi`
 *   （`ebx` = **zEnemyData\*** = `zEnemy*+0x122c`）⇒ 下面的偏移一律**换算成 zEnemy\*** 口径。
 *   一手依据与整链见 `REPORT-BOSS-SPIRIT-DROP.md` §2–§4。*/
#define CE_ENEMY_HP_CUR            0x6220     /* zEnemyData.life.current（`ebx+0x4ff4`；4ff4+122c=6220 ✅）*/
#define CE_ENEMY_HP_MAX            0x6230     /* zEnemyData.life.max（`ebx+0x5004`；5004+122c=6230 ✅）*/
#define CE_ENEMY_FLAG_BOSS         0x800000u  /* flags_lo **bit23** = ECL opcode 512 `setBoss`（`0x433bf5 or ecx,0x800000`）。
                                               * 一手普查 22 个 .ecl：只有 st0Xbs/st0Xmbs 各一次 ⇒ 该位 ⟺ boss **或道中 boss**。
                                               * 引擎自己也用它（`0x42eed0 test [ecx+0x635c],0x800000` 开 boss 血条段）。*/
#define CE_ENEMY_FLAG_NO_LOCK      0x0c000021 /* 任一位置起 = 不可锁定（死亡 / 无敌 / 未登场…）*/

#define CE_MODE_ITEM    0
#define CE_MODE_SAVE    1
#define CE_MODE_SHOP    2
#define CE_MODE_REPLAY  3

/* ---- zTimer（20 字节；Tenshi case 0x411d9c.. 的初始化；Timer__decrement 读 +0xc 当游戏速度源索引）---- */
typedef struct { int32_t prev; int32_t cur; float cur_f; int32_t speed_src; uint32_t control; } ce_ztimer_t;

/* ---- zBulletManager / zBullet（ExpHP 结构 + cancel_all 0x4297a0 的扫描：起点 mgr+0xec，2000 张，stride 0xfa0）---- */
#define CE_BM_BULLETS              0xec       /* 第 0 张（ExpHP 叫 list_0_tail_dummy_bullet；cancel_all 从它起扫 0x7d0 张）*/
#define CE_BM_BULLET_COUNT         0x7d0
#define CE_BULLET_STRIDE           0xfa0
#define CE_BULLET_FLAGS            0x20
#define CE_BULLET_POS              0x638      /* float3 */
/* ★★ 2026-09-16m（共享件新增）：**引擎自己的擦弹函数** —— `Player__do_graze`。
 * 一手（`local/_scratch/th18.dis` 直读 + 逐条读完整个函数体）：
 *   `0x45e160: 53 8b dc …` / `0x45e290: c2 04 00` ⇒ **`void __stdcall do_graze(const float *src_pos)`**
 *   （1 个栈参、**无 this**、`ret 4` ⇒ callee 清栈，调用方**不要**再 `add esp`）。
 *   `src_pos` = 「源物件的坐标 (x,y)」的指针（原生 5 个调用点：子弹传 `bullet+0x638`、
 *   敌本体传自机坐标、激光传算出来的点）。callee **只读**它。
 * 它做什么（全函数只有 6 个 call，逐条）：
 *   ① `++[0x4ccd0c]`（GRAZE）与 `++[0x4ccd10]`（章节擦弹），各钳 `0x5f5e0ff`；
 *   ② 火花位置 = `(src + 自机) × 0.5`（`0x4b9138`)；
 *   ③ `inc [effect.anm 的 zAnmLoaded + 0x134]`（活动 VM 计数）；④ 声音线程锁；
 *   ⑤ `AnmManager::allocate_new_vm`（freelist 空则 `malloc(0x60c)`）；⑥ 起 **`effect.anm` script 0x18**
 *      （一颗火花，13 帧后 `delete()`）+ 加法混合 + `AnmVm::run` + 挂世界链；
 *   ⑦ `SoundManager::play_sound_at_position(id = 0x2a = `se_graze.wav`, 声像 = src.x)`。
 * **它不做的事**（重要）：**没有**得分/弹窗、**没有**道具生成、**没有**无敌或判定改动，
 *   也**没有**任何「每帧一次」的守卫 ⇒ **调用即生效，调 N 次 = +N 计数 + N 颗火花 + N 次音效注册**
 *   （同帧同 id 的音效只会响 1 次，但逐帧调用就是逐帧重播）。
 * ⚠ **它一处都不查 NULL**：`PLAYER_PTR` / `[0x4cf2c8]`（EffectManager）/ `[0x4cf2c8]+0xc`（effect.anm）/
 *   `[0x51f65c]`（全局 AnmManager）任一为空就是访问违例 ⇒ **调用方必须自己门住**
 *   （封装见 `sdk.h` 的 `ce_do_graze()`）。*/
#define CE_FN_DO_GRAZE             0x45e160
#define CE_BULLET_VELOCITY         0x644      /* float3；普通子弹每帧 pos += velocity（0x423f1f..）*/
#define CE_BULLET_SPEED            0x650      /* float；ex 状态改速时从 speed/angle 重算 velocity（0x429bc0）*/
#define CE_BULLET_ANGLE            0x654      /* float；tick 里归一到 (-π, π]（0x4241b8..）*/
#define CE_BULLET_STATE            0xf68      /* uint16：0 = 空槽，3 = 消弹中（cancel_all 都跳过）*/

/* ---- zPlayerBullet ---- */
#define CE_BULLET_DAMAGE           0x9c       /* int；PlayerBullet__create 0x45e396 写、0x45e7f5 调槽 +0x28、0x45e837 用；Momoyo 覆写（SDK）*/

/* ★★ 2026-09-16zd：**自机弹的 `+0xe8`** —— 引擎在 `PlayerBullet__create 0x45e320` 里把它当
 * **回调指针**用：`0x45e8ad mov eax,[edi+0xe8]` → `0x45e8b9 je`（0 就跳过）→ `0x45e8c0 call eax`
 * （`ecx = edi` = thiscall，`edi` = **本颗弹**）。
 * 一手证据（崩溃 dump，`local` 外那台机器的 `th18_modkit/crashdumps/`）：
 *   · `th18.exe.119420.dmp`（2026-09-15）：EIP = `0x1`，栈顶返回地址 `0x0045e8c2`（= 该 call 的下一条）
 *     ⇒ **那一格 = 1** ⇒ `call 1` ⇒ `0xC0000005`；
 *   · `th18.exe.87856.dmp`（2026-09-16 21:53）：`0xC0000005`、写 `0x1`、`Eax = 1`，上层调用链同型。
 * · 全 exe 扫描：引擎自己对 `+0xe8` 只写**指针 / 0 / -1**，**从不写 1** ⇒ 那个 1 来自外部写入者或
 *   复用槽的残留（本仓的写入者已普查：唯一一处"写 1 到 +0xe8"在 117 的**卡状态**里，与弹无关）。
 * · 结构尺寸：社区结构表 `zPlayerBullet` = **0xf8 字节**（`0x0` … `0xf8 __end`）⇒ `+0xe8` 在弹内。
 * 落点：`sdk.c` 的 `BP_ce_bullet_damage_final`（`0x45e7ff`，**每次造弹都过**、`edi` = 弹）里加闸 +
 * 普查日志（见那里的注释）。*/
#define CE_BULLET_CALLBACK         0xe8
/* `+0xac` = **本发弹的"set<<8 | 行号"**（一手：`PlayerBullet__create 0x45e35c mov [edi+0xac],ecx`，
 * `ecx` = `Player__shoot_one_bullet` 传进来的 `set*256|row`；117 里那份同名局部宏 = `ANIMAL_BULLET_PACKED_ID`）。*/
#define CE_BULLET_SET_ROW          0xac

/* ---- 道具池 zItem（ItemManager* = [0x4cf2ec]；2026-09-08 一手，进 AUDIT 登记）---- */
#define CE_ADDR_ITEM_MANAGER_PTR  0x4cf2ec   /* ItemManager*：spawn 调用方从这取 ecx（0x430473 `8b 0d ec f2 4c 00`）*/
#define CE_ITEM_POOL_OFF          0x14       /* 池首槽 = mgr+0x14（body 0x445a80 起点 `lea 0x14(%ecx),%edi` 0x445ac0）；
                                                slot_i = mgr+0x14 + i×0xc94（循环 0x4467e9 `add edi,0xc94` / 0x4467fc `cmp eax,0x1258` jl 回头）*/
#define CE_ITEM_SLOTS             0x1258     /* 4696 槽；池尾 mgr+0xe6baf4。与 TH15 道具池上限同源（th15but14 宿主 `cmp eax,0x1258`）*/
#define CE_ITEM_STRIDE            0xc94
#define CE_ITEM_POS               0xc2c      /* float3 世界坐标 x,y,z（spawn 0x446fba movq + [eax+8] 拷贝；collect 半径判定 0x446227 读）*/
#define CE_ITEM_STATE             0xc74      /* int 状态机（2026-09-08 写点普查全链一手，AUDIT §Z21/§Z22 —— 本注为校正后模型）：
                                                0 = 空/已删（commit 尾 0x446595、出界 0x445e38 清 0；循环头 0x445ae8 je 跳槽）
                                                1 = 抛物/坠落体（CE_FN_SPAWN_ITEM 常规分支 0x446fac 置 1；spawn 第 6 参可给
                                                    intro 倒计时 +0xc84（0x445b78 系），到期调 0x4472b0；此后每帧跑 V 门
                                                    0x445b9a —— 命中翻 3（0x445bb9 系），无 V 则 0x445be6 起重力坠落、
                                                    过 0x44620a 共享尾收点检查、出界删 0x445e38；全程可 30px 触收/70px 环收）
                                                2 = 延迟释放下落体（state 5 intro 到期 0x445b0f 置 2 + FX 0x407420；纯重力
                                                    积分到尾 vel_y ≥ 0 → 0x445f4e/0x445f59 落穿 0x445f5b → 0x445f63 翻 3；
                                                    ★ 只影响 state-5 出生的 9–0xe 族，type 1/2 不经过 2/5）
                                                3 = 真空飞行（翻 3 后 0x445f6f 置位；飞向自机，跳过 70px 环 0x4465e0 系，
                                                    直飞至 <30px 分发 0x44625f；★ type {1,2} 的 state 3 = 「飞过来的收点
                                                    道具」（V 门产物；批成员/续批谓词 —— handler 0x44699e/0x446c0a
                                                    分发时读 state==3 给全额，引擎同一谓词）
                                                4 = 70px 环吸附（0x446647 置 4，速度 = player+0x47988 系；state 3/4 都跳过
                                                    环入口；state 4 case 每帧重查 V（0x445fe4 系）→ 命中翻 3；
                                                    自机 state 4 时 0x446146 放回 1）
                                                5 = 出生 intro（CE_FN_SPAWN_ITEM 的 type 9–0xe 分支 0x44713a 置 5 + 帧计数
                                                    反聚集散布（0x400/0x200/0x100 分档）+0xc84 倒计时；到期置 2 + FX 0x445b0f）
                                                吸附速度存 +0xc80，来源 player+0x47988。state 写点全集 = 0x445b0f/0x445e38/
                                                0x445f6f/0x446146/0x446595/0x446647 + 出生 0x446fac(=1)/0x4471d3(=5) */
#define CE_ITEM_STATE_VACUUM      3          /* 真空飞行（V 门产物）；辉针城批成员/续批谓词（§Z22）*/
#define CE_ITEM_TYPE              0xc78      /* 1=P、2=金钱（collect_money_item 0x446b00 每枚 +1 钱）、4=命碎、6=符卡碎、
                                                9–0xe = 分数转换点族（§Z19：0x44685c 映射表六 type 共指加分 handler 0x446547，
                                                强制收集产物，非玩家收点道具）；0x44625f `mov edx,[edi+0xc78]` 起 type 分派：
                                                0x44685c 映射表 + 0x446830 跳表；spawn 0x447051 写 */
#define CE_ITEM_MAG_SPEED         0xc80      /* float：磁力吸附速度（= player+0x47988；见 CE_ITEM_STATE 出处）*/
#define CE_ITEM_TIMER             0xc84      /* int：延迟/倒计时（spawn 第 6 栈参；state 1/5 各用其一，0x445b7e / 0x445af3）*/
#define CE_ITEM_SOUND_ID          0xc90      /* int32：**出生 intro 结束那一帧**由引擎播的 SE id（<0 = 不播）。
                                                一手 `th18:0x447323`：`eax=[edi+0xc90]; if (eax>=0) call 0x476be0(eax,…)`
                                                （0x476be0 = 12 通道的 SE 播放器）。原生 spawn 把它**恒写 -1**
                                                （`th18:0x447057` / `0x447295`）⇒ 原生道具这条音是**死的**；
                                                本卡拿它当「道具出现」的播音（每颗各自在自己的 intro 末尾响）。
                                                ★ 只有 type {1,2} 的 intro-end 走这条路（`0x4472ee`），
                                                别的 type 在 `0x4472e4` 段直接跳走 ⇒ 对它们写这个字段无效。
                                                ⚠️ **2026-09-10 实跑结论：本卡往这里写 SE id（0x0d / 0x0e 都试过）
                                                听不到声音** ⇒ 改在 C 侧 `ce_play_sound`。字段本身的一手结论保留备查。*/
#define CE_FN_POLAR_TO_VEL         0x447430   /* FUN_00447430：thiscall(float3 *out; angle, speed) `ret 8` —— FSINCOS 一次算出
                                                (cos·speed, sin·speed)。道具的吸引飞行 / TH16 的弹出生都用它（TH16 同款 `0x430df0`）。
                                                卡里用 `ce_polar_to_vel()`，别自己写 sin/cos（DLL 不链 libm）*/
#define CE_FN_SPAWN_ITEM          0x446f40   /* ★ 栈参 8 个、callee `ret 0x20`（0x446f56/0x447137 `c2 20 00`）：
                                                ECX = CE_ITEM_MANAGER()（0x430473）；栈参自低向高 (type@[ebp+8], pos float3*@[ebp+0xc],
                                                junk@[ebp+0x10]（原生从不写——0x43047c 前 `sub esp,8` 的未初始化区；我方传 0）, angle@[ebp+0x14],
                                                speed@[ebp+0x18], timer@[ebp+0x1c]（原生撒落传 0 → 出生 state 1 + 立即调 0x4472b0）,
                                                [ebp+0x20]@未读, [ebp+0x24]@未读)。type≥0x14 → 返回 0；type 1..0x13 → 返回槽指针。
                                                原生 main-drop 实参 = (type, 敌pos, 垃圾, −1.5707964f, 2.2f, 0)；我方一律补满 8 参。
                                                ★ 与初版「RET 0x1c / 6 参」记法不同——0x430479 `sub esp,8` 起尾两槽必须压（AUDIT）*/
#define CE_ITEM_SLOWDOWN          0xe6bb14   /* zItemManager.lolk_item_slowdown_factor（float）—— TH15 擦弹减速的残骸：
                                                引擎每帧照旧把飘落态位移与重力增量乘它、照旧 +0.1/帧 回 1.0，
                                                但 TH18 自己从不写 0.3（全 exe 6 处引用：1 写 1.0 / 3 读 / 恢复 1 读 1 写）。
                                                ★ 绀珠传卡（115）每帧写 0.3 就是在复活 TH15 的机制；卡里的季节点也读它，
                                                免得同一屏上 P 点慢了、季节点不慢。见 `engine/player/th18/02` */
#define CE_ITEM_MANAGER()         (*(uint8_t **)CE_ADDR_ITEM_MANAGER_PTR)

/* ---- 弹字 ring（PopupManager* = [0x4cf420]；2026-09-08 一手反汇编，进 AUDIT §Z）----
 * mgr 布局：+0x10 = 数字槽分配计数器（adder 0x4645f0 用它当槽号、≥10 归零 wrap）。
 * 18 槽 @ mgr+0x624，stride 0x48：k0–12 = 数字槽（adder 0x4645f0 只写 10 个：计数器 wrap at 10）、
 * k13–17 = 倍率槽（rate 区 @ mgr+0x9cc）。tick 0x464020 遍历 13 槽（0x464049 `lea 0x64c(%edi)`，edx=slot+0x28）
 * 再 5 槽（0x4640e0 `lea 0x9f4(%edi)`）；draw 0x464180 同构。 */
#define CE_ADDR_POPUP_MGR_PTR     0x4cf420
#define CE_POPUP_RING             0x624      /* 18 槽环首（stride 0x48，共 0x510）；k13 起 = rate 区 */
#define CE_POPUP_NUM_SLOTS        13       /* 数字槽个数（一手：tick `0x464020` 第一环 `mov esi,0xd` /
                                              * `lea 0x64c(%edi)` ⇒ k0..k12；draw 同构 `0x4641c0` 起）。
                                              * ⚠ 原生 adder `0x4645f0` 只写 k0..k9（计数器 wrap at 10），
                                              * 另外 3 格是余量 —— 卡侧写满 13 格是安全的（环里只判 busy）。*/
#define CE_POPUP_RING_RATE        0x9cc      /* = RING + 13×0x48：5 个倍率槽起点 */
#define CE_POPUP_RATE_SLOTS       5
#define CE_POPUP_STRIDE           0x48
/* slot 字段（busy @ +0x3c；全部对 adder 0x4645f0 / tick / draw 交叉核对）：
 *   +0x00  数字槽：ASCII 数字串（adder 逆序写 0x30+digit… 见下）；倍率槽不用
 *   +0x0c  float3 世界坐标 x,y,z（adder `movq (%eax)→+0xc` + `movl +0x8→+0x14`）
 *   +0x18  float scale（adder 写 1.0f `0x3f800000`；tick 拿它乘帧常量当弹出动画）
 *   +0x1c  dword color 0xAARRGGBB（adder 从栈参抄；tick 淡出 = 每帧 alpha−4 写回，≤0 → 清 busy）
 *   +0x20  int −1（adder 初值）
 *   +0x24  int age（adder 初值 0；tick 每帧 +1：-0x4(%edx) 读、-0x8(%edx)=+0x20 留上一帧）
 *   +0x28  int 0（adder 初值）
 *   +0x2c  int 0（adder：+0x30 bit0 未置时清 0 再置 bit0）
 *   +0x30  dword 动画类标志（adder 初值 = 置 bit0；tick `0x4(%edx)`=+0x2c ≥1 则清 0 → 表 0x4b35c0[0]）
 *   +0x3c  byte busy（1 = 占用；tick 数字槽 age>0x3c 直接清 0；倍率槽走 alpha 淡出后清）
 *   +0x3d  byte 数字槽 = 数字串长（adder `movb %dl`）；倍率槽 draw 不读
 *   +0x40  int value —— 倍率槽专属：draw 0x464553 以它作 "%d"（fmt 0x4b61a4）的实参；<0 → 走 "NO BONUS"
 *   +0x44  float mult —— 倍率槽专属：draw 0x464522 cvtps2pd 后作 "BONUS %.1f"（fmt 0x4b71f8）的实参
 * draw 倍率槽两连文本调用 0x419cb0（this 显式传 [0x4cf2ac]）：slot 颜色先抄进 文本对象+0x19228
 * （0x4644c2）；"%d" 的 y 再偏 +0x4b9278（低一行的数值）。数字槽由 draw 0x4641f5 起直接走字形路径，
 * 字符偏移表 +0x121/+0x12c/+0x136（0x51f65c 字形源）。adder 0x4645f0 写数字串是**裸 digit 字节**
 * （0..9，非 0x30+digit），负值写 0x0a —— draw 对裸字节加基再取字形，勿在串里写 ASCII。 */
#define CE_POPUP_BUSY             0x3c
#define CE_POPUP_POS              0x0c
#define CE_POPUP_COLOR            0x1c
#define CE_POPUP_AGE              0x24
#define CE_POPUP_VALUE            0x40      /* 倍率槽 */
#define CE_POPUP_MULT             0x44      /* 倍率槽 */
#define CE_FN_TEXT_ADD            0x419cb0  /* draw 0x464180 的文本管理器接口（本 mod 不直接调；记档）*/
#define CE_POPUP_MGR()            (*(uint8_t **)CE_ADDR_POPUP_MGR_PTR)
