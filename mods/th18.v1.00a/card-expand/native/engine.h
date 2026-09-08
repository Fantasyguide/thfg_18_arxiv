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
#define CE_ADDR_MONEY              0x4ccd34   /* int（SM §1）*/
#define CE_ADDR_MONEY_TOTAL        0x4ccd30   /* int，只记收入（SM §1）*/
#define CE_ADDR_GLOBALS_INNER      0x4cccdc   /* zGlobalsInner（命 / bomb 的加法函数的 this）*/
#define CE_ADDR_CURRENT_LIVES      0x4ccd48   /* = GlobalsInner+0x6c */
#define CE_ADDR_LIVES_MAX          0x4ccd54   /* ExpHP LIVES_STOCK_cardfed_cap7 = GlobalsInner+0x78：残机上限，CardLife 每张 +1 钳 7 */
#define CE_ADDR_CURRENT_BOMBS      0x4ccd58   /* = GlobalsInner+0x7c */
#define CE_ADDR_BOMB_FRAGMENTS     0x4ccd5c   /* = GlobalsInner+0x80；HUD 炸弹行的第二个参数 */
#define CE_ADDR_MAX_BOMBS          0x4ccd64   /* = GlobalsInner+0x88，CardBomb 每张 +1 钳 7 */
#define CE_ADDR_GAME_THREAD_PTR    0x4cf2e4   /* 非 0 = 在游戏线程（零售即时卡 dtor 的门）*/
#define CE_ADDR_CARD_PRICE_BY_TIER 0x4b35c4   /* int[15]，price_tier → 金钱（SM §「价格表」一手 dump）*/
#define CE_ADDR_MONEY_ITEMS        0x4ccd20   /* int，吃到的金钱道具个数（collect_money_item 0x446d22 inc）*/
#define CE_ADDR_CURRENT_POWER      0x4ccd38   /* OM §7；= GlobalsInner+0x5c（spend_power 0x457486 读写）*/
#define CE_ADDR_MAX_POWER          0x4ccd3c
#define CE_ADDR_POWER_PER_LEVEL    0x4ccd40   /* int（= 100）：一档火力，= GlobalsInner+0x64（spend_power 0x457489；Tsukasa 0x410e6d 门槛 = 2 档）*/
#define CE_FN_SPEND_POWER          0x457480   /* GlobalsInner__spend_power：thiscall(GlobalsInner*; amount) ret 4。CURRENT_POWER <= 一档时拒绝（返回 0、不扣）；
                                                 * 否则扣 amount 并钳到一档（永远保留 1.00），返回「档数是否变了」（0/1）。零售调用点：
                                                 * CardTsukasa__c_press 0x410f1c（push [0x4ccd40]; ecx = 0x4cccdc; call）、商店 0x418477、死亡 0x45c154。AUDIT §V */
#define CE_FN_PLAYER_REPOPULATE_OPTIONS 0x45d5e0 /* Player__repopulate_options_and_notify_cards：thiscall(this = PLAYER+0x620; 一个从不读取的栈参) ret 4
                                                 * （尾 0x45dd0b `ret 4`；Tsukasa 0x410f27 `push ecx; lea ecx,[player+0x620]; call`）。按火力档重建自机子机、
                                                 * 尾部广播 on_power_level_change。扣火力后必须调（Tsukasa / 商店都这么做）。AUDIT §V */
#define CE_PLAYER_INNER            0x620      /* zPlayer.inner（repopulate 的 this；首字段就是 pos，所以与 CE_PLAYER_X 同址）*/
#define CE_FN_RNG_RAND_DWORD       0x402740   /* Rng__rand_dword：thiscall(zRng*)，无栈参，裸 ret，eax = 32 位随机（两步 16 位生成器拼起来；rng+4 计数 += 2）*/
#define CE_ADDR_REPLAY_SAFE_RNG    0x4cf288   /* zRng REPLAY_SAFE_RNG（ExpHP）：弹幕 / 敌人 / 自机弹 / 零售卡（Suwako、Yachie、Miko）都用它；
                                                 * 0x4cf280 那个是商店 / UI 的非回放 RNG（pick_weighted_random_offer 0x41717b）。gameplay 随机一律走 0x4cf288。AUDIT §V */
#define CE_ADDR_PLAYER_PTR         0x4cf410   /* zPlayer*（ExpHP statics；Nitori/Momoyo 都从这读）*/
#define CE_ENEMY_DROP_MAIN         0x00       /* zEnemyDrop（enemy+0x623c；0x430510 的 ecx 就是它，0x430499 lea ecx,[edi+0x623c]）：
                                                 * +0x00 main_drop：dropMain / enmCreate 第 6 参的那一颗，引擎在调 0x430510 之前撒（0x430469），调完清零（0x4304aa）*/
#define CE_ENEMY_DROP_COUNTS       0x04       /* +0x04 ex_drop[20]：dropExtra 表，20 个 int32（(type-1)*4）。
                                                 * Enemy__drop_items_and_notify_cards 0x430510 按 type 1..0x13 逐个 spawn（第 20 格不撒），
                                                 * 广播完 vtable+0x30 后 memset(+0x04, 0, 0x50)。AUDIT §S / S10 */
#define CE_ENEMY_DROP_TYPES        20
#define CE_ADDR_BOMB_MGR           0x4cf2b8   /* 炸弹管理器*；+0x30 = 「正在放炸弹」（do_bomb 置 1，Bomb__can_bomb_and_deathbomb_check 0x420420 查它；道具磁力判定 0x445bc4 也查它决定吸不吸）*/
#define CE_ADDR_ABILITY_MGR_PTR    0x4cf298   /* zAbilityManager*（OM §4）*/
#define CE_ADDR_BULLET_MGR_PTR     0x4cf2bc   /* zBulletManager*（ExpHP statics；cancel_all 0x4297a9 读）*/
#define CE_ADDR_GUI_PTR            0x4cf2e0   /* zGui*（ExpHP）；+0x1b0 = msg（对话 VM，非 0 = 对话中）。C 键门控 0x45c069、Tenshi state0 0x40ea0d 读它 */
#define CE_ADDR_ENEMY_MGR_PTR      0x4cf2d0   /* zEnemyManager*；+0x198 = enemy_count_real（ExpHP）。== 0 时 C 键与充能都停（0x45c07b / 0x40ea1f）*/
#define CE_ADDR_INPUT_PRESSED      0x4ca434   /* 本帧上升沿；bit 0x400 = C 键（0x45c084）*/
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
#define CE_ENEMY_ID                0x6830     /* zEnemy.enemy_id */
#define CE_ENEMY_DATA              0x122c     /* zEnemyData 在 zEnemy 里的偏移：enemy+0x6374 = data.interrupts(+0x5148)、enemy+0x14ec = data.time_in_ecl.cur(+0x2c0)、enemy+0x6220 = data.life(+0x4ff4)（步进 0x42ed40 三处交叉核对）*/
#define CE_ED_TIME_IN_ECL          0x2bc      /* zTimer{prev,cur,cur_f,…}：中断槽的超时阈值拿它比（0x42edf5 起：slot.time <= cur → 超时子程序）*/
#define CE_ED_INTERRUPTS           0x5148     /* zEnemyInterrupt[8]：{+0 hp_value, +4 time, +8 sub_life[0x40], +0x48 sub_timeout[0x40]}，stride 0x88；活动槽 = 第一个 hp_value > -1 && time > 0 */
#define CE_ED_INTERRUPT_STRIDE     0x88
#define CE_ED_INTERRUPT_SLOTS      8
#define CE_FN_GUI_UPDATE_LIVES     0x441f10   /* thiscall(gui; lives, fragments, max) ret 0xc：刷 HUD 残机行（死亡 0x45c2xx、商店复原 0x4179xx 都调；一手反汇编 AUDIT O28）*/
#define CE_SE_INVALID              0x10       /* 无效操作（商店买不起同款）*/
#define CE_FN_BULLET_CANCEL_ALL    0x4297a0   /* thiscall(BULLET_MANAGER; 一个从不读取的栈参) ★ret 4（0x429a0e）：全屏消弹（弹 → 点道具 + 音效 0x47）。函数体自己读全局 0x4cf2bc；零售 ECL 消弹 case 0x434d48 `mov ecx,[0x4cf2bc]; push 0; call`（AUDIT O28h′）*/
#define CE_FN_BULLET_CANCEL_RADIUS_AS_BOMB 0x429370 /* stdcall(pos*, mode, max_count, tag) + ★XMM2 = 半径；四个出口都 ret 0x10。命中：state∈{1,2} 且 bullet+0x24==0 且 dist² ≤ (弹半径(+0x658)×0.5 + R)²；每消一发 Bullet__cancel(b, mode) 0x428e90 + counter++；消满 max_count 立即返回（Tenshi 0x40eb13..0x40eb2f，AUDIT O29a）*/
#define CE_BM_CANCEL_COUNTER       0x7a41e8   /* int：cancel_radius_as_bomb 每消一发 ++；Tenshi 每帧调前清零、调后读（0x40eb56）。ExpHP __some_cancel_related_counter */
#define CE_FN_PLAYER_DMGSRC_RECT   0x45dfa0   /* stdcall(center*, angle, 寿命帧, 每次伤害) + ★XMM2 = 宽、XMM3 = 高；ret 0x10；返回 1-based 槽号。矩形伤害源（flags &~6|1）：+0xc 角度、+0x14/+0x18 宽高、+0x7c 累计上限 9999999、+0x80 命中间隔 1。Remilia 0x40f4a6 调 (玩家上方, 0.0, 2, 200) 宽 32（AUDIT O29b）*/
#define CE_PLAYER_DMGSRC_POOL      0x20574    /* zPlayerDamageSource[0x400]，stride 0x9c；flags bit0 active、bit1 圆形（create_45de40）/ 清 = 矩形（engine/player/th18/02-damage-sources.md）*/
#define CE_PLAYER_DMGSRC_STRIDE    0x9c
#define CE_PLAYER_DMGSRC_SLOTS     0x400
#define CE_PLAYER_DAMAGE_CAP       0x47984    /* int：敌人侧 enm_compute_damage_sources 0x45f0f0 把本帧总伤害钳到它（0x45f28b）；GameThread 0x443d3b 每帧从 sht+0x28 复位；Remilia 0x40f48e 抬到 300。本 mod 照 Remilia 在卡 tick 里抬（ce_raise_damage_cap，炎魔爆炸帧；AUDIT V20）*/
#define CE_ADDR_ANM_MANAGER_PTR    0x51f65c   /* zAnmManager*（ExpHP ANM_MANAGER_PTR；interrupt_tree 0x488be3 读）*/
#define CE_FN_ANM_GET_VM_WITH_ID   0x488b40   /* thiscall(ANM_MANAGER; id) ret 4 → zAnmVm* / 0（fast 数组 0x624 步长或 world/ui 链表；AUDIT O29c）*/
#define CE_FN_ANM_INTERRUPT_TREE   0x488be0   /* stdcall(id, n) ret 8：自取 ANM_MANAGER_PTR，找到 VM 写 +0x494 = n 并递归子树（Tenshi 到时长调 (id, 1)）*/
#define CE_ANM_VM_POS              0x5f0      /* float3 实体坐标（Tenshi/Miko/Remilia 每帧写；engine/anm/th18/01-vm-instantiate.md）*/
#define CE_ANM_VM_COLOR1           0x524      /* D3DCOLOR 0xAARRGGBB：Tenshi 0x40eb4c 无命中写 0xffffffff、0x40eb60 有命中写 0xff0080ff（= 蓝 (0,128,255)）*/
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
#define CE_ABILITY_MGR()  (*(uint8_t **)CE_ADDR_ABILITY_MGR_PTR)
#define CE_BULLET_MGR()   (*(uint8_t **)CE_ADDR_BULLET_MGR_PTR)
#define CE_GUI()          (*(uint8_t **)CE_ADDR_GUI_PTR)
#define CE_ENEMY_MGR()    (*(uint8_t **)CE_ADDR_ENEMY_MGR_PTR)
#define CE_ANM_MANAGER()  (*(uint8_t **)CE_ADDR_ANM_MANAGER_PTR)
#define CE_GUI_MSG            0x1b0       /* zGui.msg：对话中 */
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
                                                 零售 `CardReimu1__operator_delete` `0x40ab20` 在析构时按它删 VM（ExpHP anm_id_for_ingame_effect）*/
#define CE_CARD_ELAPSED_TIMER      0x20       /* zTimer：激活经过帧（每帧 Timer__increment；OM §3 订正）*/
#define CE_CARD_RECHARGE_TIMER     0x34       /* zTimer：充能倒计时（空闲时 Timer__decrement；c_press 门控 +0x38 <= 0）*/
#define CE_CARD_RECHARGE_TIME      0x48
#define CE_FLAG_FROM_SAVE          0x01       /* flags bit0：存档 / replay 装载（mode & 1）；局末 recount 只删 bit0 == 0 的卡 */
#define CE_FLAG_ACTIVE             0x08       /* flags bit3：主动卡（尾段按它入 selected / num_active）*/
#define CE_FLAG_EQUIPMENT          0x40       /* flags bit6：装备卡（recount 0x408164 test al,0x40 → num_equipment）*/
#define CE_FLAG_FIRING             0x20       /* flags bit5：正在释放（HUD 配色，method_40）*/
#define CE_FLAG_ACTIVE_CLEAR       0x46       /* 主动卡 case 清的位（0x411d83 and ~0x46 | 8）*/
#define CE_FLAG_EQUIPMENT_SET      0x44       /* 零售装备卡 case 的写法：flags & ~0x0a | 0x44（allocate_new_card 里 REIMU_OP…NUE_OP 那 14 个 case；case 56 走被动 & ~0x4a | 4）。SDK 绑定时对 category 1 的卡照此改（AUDIT X11）*/
#define CE_FLAG_EQUIPMENT_CLEAR    0x0a
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
#define CE_PLAYER_X                0x620      /* float px（PL §1）*/
#define CE_PLAYER_Y                0x624
#define CE_PLAYER_STATE            0x476ac    /* PL §3 */
#define CE_PLAYER_FOCUSED()        (*(int32_t *)(CE_PLAYER() + 0x476cc))   /* 聚焦位 = player+0x476cc（PL §3）：每帧 = INPUT_HELD>>3 & 1，dword 写
                                                                              （0x45b414 `89 97 cc 76 04 00` mov [edi+0x476cc],edx；0/1）*/
#define CE_PLAYER_INVULN_TIMER     0x47774    /* zTimer{prev,cur,float}；复活 0x45c35e 置 {0x117,0x118,280.0}，决死救回 60，炸弹各自置（SDK）*/
#define CE_PLAYER_SPEED_MULT       0x477ec    /* float；Player tick 末尾 0x45c702 复位 1.0；移动 0x45b5b6 读。AbilityManager tick(0x16) 先于 Player(0x17) → on_tick_2 里写生效（SDK）*/
#define CE_PLAYER_SPEED_BASE       0x477f0    /* float×2，每帧从 0x5217dc 复制（0x45c717）*/
#define CE_PLAYER_SHT_PTR          0x47940    /* zPlayer.sht_file*：GameThread 0x443d32 `mov eax,[ecx+0x47940]; mov eax,[eax+0x28]` 每帧复位伤害上限 */
#define CE_SHT_MAX_DMG             0x28       /* sht 头 i32 max_dmg：Reimu 90 / Marisa 160 / Sakuya 60 / Sanae 120（engine/sht/th18/01 §5）；古锭刀终符抬到它的 2 倍 */
#define CE_PLAYER_DAMAGE_MULT      0x47980    /* float；EnemyManager tick 读后复位 1.0（0x42e02f）——别用，顺序不可控 */
#define CE_PLAYER_ITEM_ATTRACT_SPD 0x47988    /* float：Player__reset {5,30,70,70}，Nitori on_load {10,30,110,110}（ExpHP 名 + SDK）*/
#define CE_PLAYER_ITEM_COLLECT_R   0x4798c
#define CE_PLAYER_ITEM_ATTRACT_RF  0x47990    /* focused */
#define CE_PLAYER_ITEM_ATTRACT_RU  0x47994    /* unfocused */
#define CE_PLAYER_POC_HEIGHT       0x47998    /* Kanako 置 224.0 */

typedef struct { int32_t prev; int32_t cur; float cur_f; } ce_timer_t;   /* zTimer 前三个字段（Bomb / Card 的写法一致）*/
#define CE_PLAYER_INVULN()   ((ce_timer_t *)(CE_PLAYER() + CE_PLAYER_INVULN_TIMER))
#define CE_RESPAWN_INVULN_FRAMES   0x118      /* 280 */

/* ---- 可从卡里调的引擎函数（调用约定一手：SDK.md / AUDIT §O16）---- */
#define CE_FN_ALLOCATE_NEW_CARD    0x411460   /* thiscall(mgr; id, mode)：mode 0 道具 / 1 存档 / 2 购买 / 3 replay；返回 num_total，-1 = 拒 */
#define CE_FN_CREATE_HUD_LISTS     0x408d00   /* AbilityManager__create_all_card_lists_for_hud：thiscall(mgr; int) ret 4：先按 mgr+0x58 删旧图标 VM，再按三个计数重画三行；分配尾段 mode != 1 时调 (mgr, 0)（AUDIT §W）*/
#define CE_FN_MARK_OBTAINED        0x418de0   /* fastcall(id, notify)：置解锁位（本 mod 的断点把新 id 转进影子 + side-car）*/
#define CE_FN_TABLE_GET            0x407d70   /* fastcall(id) → zTableCardData*；未命中回落 NULL 行 */
#define CE_FN_SHOP_PICK_RANDOM     0x416f50   /* fastcall(out*, tier_lo; tier_hi, exclude[], n)：商店随机池抽一张（未拥有、本关可用、按权重、游戏 RNG）；返回非 0 = 抽到，*out = 表行 */
#define CE_FN_TIMER_DECREMENT      0x409750   /* thiscall(zTimer*; 一个未用栈参) ret 4：prev = cur, cur_f -= 游戏速度, cur = (int)cur_f */
#define CE_FN_TIMER_INCREMENT      0x405990   /* thiscall(zTimer*; 一个未用栈参) ret 4：同上方向相反。★ 调用方必须压那 4 字节 */
#define CE_SE_EXTEND     0x11   /* 音效 id（engine/_shared/th18-sound-table.md 一手）*/
#define CE_SE_BOMB       0x2c
#define CE_SE_CARDGET    0x2e
#define CE_SE_PIN00      0x39   /* 收点结算钟声：TH14 v1.00b 原生结算同 id 同 wav 同调校（AUDIT §Z17）*/
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
#define CE_FN_TICK_SHOOTERS_FOR_CARD 0x40a9c0 /* stdcall(option, short_timer, long_timer, sht_set_index) ret 0x10：
                                                 取 *(int*)(PLAYER+0x47940)+0xe0+idx*4 那组 shooter，逐条判
                                                 `timer % fire_rate == start_delay` 就发；弹从 option+0x5c/+0x60 出膛。
                                                 ★ fire_rate == 0 会整数除零（这条路径没有零分支）。
                                                 开火期间把 player+0x10 换成 ability.anm，所以 shooter 的 anm 字段按 ability.anm 解释 */
#define CE_OPT_IN_USE              0x00       /* zPlayerOption：非 0 = 占用中（allocate 找空位看它）*/
#define CE_OPT_POS_X               0x5c       /* 定点数，× 1/128 = 像素（`0x40a9c0` 出膛坐标就取这两个）*/
#define CE_OPT_POS_Y               0x60
#define CE_OPT_ANM_ID              0xb0       /* 子机的 anm vm id */
#define CE_OPT_OWNER_INDEX         0xd0       /* = 建它那张卡的 card+0x08（array_index）；用来认领「这个槽还是我的吗」*/
#define CE_SUBPIXEL_TO_PIXEL       (1.0f / 128.0f)   /* `0x4b908c` */
#define CE_PLAYER_AIM_ANGLE        0x479cc    /* float：卡把瞄准角写这里，shooter 的 func_on_init = 5（`0x4612d0`）
                                                 在建弹时用它覆写 bullet+0x64。CardAlice `0x40b5fa` 同款用法 */
/* ---- zEnemy（`CardAlice__on_shoot` 0x40b4e0 与最近敌人搜索 0x438cb0 一手）---- */
#define CE_ENEMY_POS_X             0x1270     /* = data(+0x122c).final_pos(+0x44).pos */
#define CE_ENEMY_POS_Y             0x1274
#define CE_ENEMY_FLAGS             0x635c
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
#define CE_BULLET_VELOCITY         0x644      /* float3；普通子弹每帧 pos += velocity（0x423f1f..）*/
#define CE_BULLET_SPEED            0x650      /* float；ex 状态改速时从 speed/angle 重算 velocity（0x429bc0）*/
#define CE_BULLET_ANGLE            0x654      /* float；tick 里归一到 (-π, π]（0x4241b8..）*/
#define CE_BULLET_STATE            0xf68      /* uint16：0 = 空槽，3 = 消弹中（cancel_all 都跳过）*/

/* ---- zPlayerBullet ---- */
#define CE_BULLET_DAMAGE           0x9c       /* int；PlayerBullet__create 0x45e396 写、0x45e7f5 调槽 +0x28、0x45e837 用；Momoyo 覆写（SDK）*/

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
#define CE_FN_SPAWN_ITEM          0x446f40   /* ★ 栈参 8 个、callee `ret 0x20`（0x446f56/0x447137 `c2 20 00`）：
                                                ECX = CE_ITEM_MANAGER()（0x430473）；栈参自低向高 (type@[ebp+8], pos float3*@[ebp+0xc],
                                                junk@[ebp+0x10]（原生从不写——0x43047c 前 `sub esp,8` 的未初始化区；我方传 0）, angle@[ebp+0x14],
                                                speed@[ebp+0x18], timer@[ebp+0x1c]（原生撒落传 0 → 出生 state 1 + 立即调 0x4472b0）,
                                                [ebp+0x20]@未读, [ebp+0x24]@未读)。type≥0x14 → 返回 0；type 1..0x13 → 返回槽指针。
                                                原生 main-drop 实参 = (type, 敌pos, 垃圾, −1.5707964f, 2.2f, 0)；我方一律补满 8 参。
                                                ★ 与初版「RET 0x1c / 6 参」记法不同——0x430479 `sub esp,8` 起尾两槽必须压（AUDIT）*/
#define CE_ITEM_MANAGER()         (*(uint8_t **)CE_ADDR_ITEM_MANAGER_PTR)

/* ---- 弹字 ring（PopupManager* = [0x4cf420]；2026-09-08 一手反汇编，进 AUDIT §Z）----
 * mgr 布局：+0x10 = 数字槽分配计数器（adder 0x4645f0 用它当槽号、≥10 归零 wrap）。
 * 18 槽 @ mgr+0x624，stride 0x48：k0–12 = 数字槽（adder 0x4645f0 只写 10 个：计数器 wrap at 10）、
 * k13–17 = 倍率槽（rate 区 @ mgr+0x9cc）。tick 0x464020 遍历 13 槽（0x464049 `lea 0x64c(%edi)`，edx=slot+0x28）
 * 再 5 槽（0x4640e0 `lea 0x9f4(%edi)`）；draw 0x464180 同构。 */
#define CE_ADDR_POPUP_MGR_PTR     0x4cf420
#define CE_POPUP_RING             0x624      /* 18 槽环首（stride 0x48，共 0x510）；k13 起 = rate 区 */
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
