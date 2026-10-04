/* 花映塚 × 獣王園 合成卡（card-expand id 109）—— 卡侧接线。
 * 纯逻辑在 pofv_core.{h,c}（主机单测 `tests/test_pofv_core.c`）；本文件是**引擎面**：
 * 输入、消弹调用点、无敌声明、伤害源、灵魂池、**灵力道具池**、生命周期清理。
 *
 * 机制与设计依据
 *   · 机制记录（用户口径一手抄录）：`mods/th18.v1.00a/card-expand/IDEAS.md` §「109 花映塚」
 *   · 白弹阈值 T = **半径 ≤ 4.0**（一手）：`docs/superpowers/specs/2026-09-11-system-card-spec-review.md` §5.4
 *   · 拦 Z / 死亡事件 / 消弹接口：同文 §11 #11 #47、§6.4；契约见 `EVENTS-CONTRACT.md`
 *   · ★★ **TH09 一手（2026-09-22，P5 #1/#3/#4/#5 那一批）**：
 *     `engine/card/th09/01-pofv-charge-and-cancel-fields.md`（蓄力/拦 Z 的闸 = 10 帧、
 *     气槽 = 4×100 且蓄力量被它钳上界、消弹圈 = **锚定场**（r₀ + dr×帧、有限寿命）、
 *     CA 三级的一手半径 192/256/448）。
 *   · 本卡 spec：`docs/superpowers/specs/2026-09-12-pofv-109-design.md`（数值表 / 未定项都在那）
 *   · ★★★ **合成卡规格（owner 2026-09-28 拍板）**：`SPEC-109-FUSION.md` + `HANDOFF-109.md`
 *     —— 109 = 「花映塚 × 獣王園」合成卡。
 *     **第 1 步已落地（2026-10-01）**：① 拦 Z 整条删除（owner：「不拦截 Z 了，太屎了」）；
 *     ② 蓄力键改 **按住 C 的电平**（`CE_INPUT_HELD() & 0x400`）；③ Z 回纯原生射击。
 *     **第 2 步（进行中）**：④ **灵力道具**（口径 5）—— 充能不再直加点，改成
 *     「消弹 / 击破 / 结界碰弹 → 掉实体 → 拾取 → 涨槽」；池在 `pofv_core.h` §E，
 *     ⚠ **形态 = 自建实体池，不借原生道具 type**（三路互证无干净空号，见 §E 的长注）。
 *     余下：固灵结界（几何 / 视觉 / 沈静化）+ 自制灵（ECL + 循环撒 + boss 不撒）。
 *
 * 键位（owner 2026-09-28 口径，最高优先级；与旧文档冲突时以口径为准）
 *   · **按住 C = 蓄力**（电平：按住即在涨档，松开结算；**不需要本卡被选中**）；
 *     一手依据 = TH09 的「按住射击满 10 帧进蓄力支」，本卡**只保留蓄力的律、去掉拦 Z 那半**
 *     （一手事实仍在 `th09:0x41fa17`，是刻意不复刻 —— 登记 `TUNING.md` §2.0v #1）。
 *   · **Shift = 低速 / 固灵**（展开吸灵领域；低速本身是原生的，本卡只加「领域」这一层）。
 *     ⚠ 合成卡的「固灵结界」在 SPEC 里是 **C 侧常驻**（口径 4），本文件这一层是**待重做的旧实现**。
 *   · **Z = 纯原生**（点射 / 连射全交回引擎，本卡不拦、不写输入位）。
 *   · **X = 原生炸弹**（QCA 已放弃，见 `pofv_core.h`）。
 *
 * ★ 性能（review §4.1 强制）：`on_enemy_death` 在**清场帧**一次被调 100–300 次，
 *   回调里扫弹池 = O(n²) ⇒ 回调**只登记**（定档 + 起一个暴风场 + 掉一颗灵魂 + 掉一把灵力道具，
 *   全 O(1)：灵力道具那次是「线性找空槽 ≤ 48 次比较」），
 *   真正的消弹在 `on_tick_2` 帧末按 `POFV_STORM_SCANS_PER_FRAME` 的预算做。
 *
 * ★ 长持续效果（CLAUDE.md / METHOD.md 铁律）：本卡跨帧留下的东西 = **暴风场池** + **灵力道具池**
 *   （每颗一台跟随 VM）+ **灵**（真敌人 + 每只一台跟随 VM）+ **固灵结界的跟随 VM** + 结界闩锁 +
 *   无敌声明层的一格。**全部**在 `clear_card()`（`on_run_reset` / `on_delete` 共用）里收
 *   —— 删卡发生在商店里、那一刻 tick 停着，画面上的 VM 与场上的真敌人不会自己走。
 */
#include "sdk.h"
#include "pofv_core.h"
#include <string.h>

/* ============================ 引擎面：engine.h / sdk.h 里没有包装的裸地址 ============================
 * 0x428e90  Bullet__cancel —— ★thiscall(弹指针; mode)。
 *           ★ **2026-09-30 订正**（依据 = `engine/bullet/th18/01-cancel-to-item.md` §1/§2.1（一手））：
 *           ① 旧注说「全 EXE 的消弹入口**只有四个**」⇒ **五个** —— 漏了 **`0x4294b0`**
 *              （`cancel_rect_as_bomb(pos*, size*, mode, tag)`，矩形入口，`0x4294b0`–`0x42979f`；
 *              调用点 4 个：`0x40ba0b` 原生卡 19 / `0x40f4cb` 卡 46 / `0x420eb2` BombMarisa / `0x435a6a` ECL op 712）。
 *           ② `mode` 原样给被调的 `0x4290e0`，真值表（§2.1 逐字节）：
 *              **0 = 什么都不撒**（`test edx,edx / je`，连计数器都不动）；
 *              **1 = 撒 type 9**（小分道具，`0x4291a5 push 9`）；
 *              **3 = 什么都不撒、只 `[mgr+0x7a41d0]++`**（`0x4291b5 jne 0x429248` —— 旧注把 3 与 4 说反了）；
 *              **4 = 多数 type 9 + 少数 type 2**（≈36.7%，`0x4291b2`–`0x429243` 三路分流）。
 *              1/4 两支都走 `0x446f40` 撒原生道具，且都要过**场地门**（贴边消掉的弹零产出）。
 *           「消一堆但放过某些」只能自己扫池逐弹调它。
 *           ⚠ **它不认 `+0x24` 的不可消位**（★ 2026-09-12 asm 复核：`+0x24` 只被 `0x429370` 查——
 *           `0x4293c8 cmp DWORD PTR [esi+0x24],0x0 / jne`；**`0x429250` 全函数体一次都没读 `+0x24`**，
 *           `0x4297a0` 全屏版同样不查 ⇒ 原注释「0x429250 / 0x429370 才认」是错的，只有后者认）。
 *           ⇒ 逐弹调它的**蓝圈**能消掉带 `+0x24` 的弹，而走 `0x429370` 的**红圈反而消不掉**这类弹
 *           （半径型「不可消」没有这个位，所以照旧会被红圈消掉 —— 见 `cancel_all_in_circle` 的注释）。
 * 0x658     zBullet.radius（★弹半径；弹种表 `0x4c5f90` stride `0x118`、出生时从**表项 +0x104 = 0x4c6094**
 *           拷入，一手 asm `0x425998 movss xmm0,[eax+0x4c6094]` → `0x4259a8 movss [edi+0x658],xmm0`）
 * 0x24      zBullet 的不可消位（列在这里只为说明上面那条差异，本卡**不写**它）
 * 扫池常量全部来自 engine.h：槽 0 = `CE_BM_BULLETS`、2000 槽、stride `0xfa0`、state `+0xf68`、pos `+0x638`。
 */
#define POFV_FN_BULLET_CANCEL   0x428e90
#define POFV_BULLET_RADIUS      0x658
#define POFV_BULLET_PROTECT     0x24

/* 输入位（`0x4ca428` 的掩码 = replay 录的那一份 ⇒ 卡侧读它**回放安全**；engine.h 的注释）。
 * ★ 合成卡（owner 2026-09-28）：蓄力键 = **C 的电平**（`0x400`，仍在 replay 掩码低 16 位内
 *   ⇒ 回放天然可复现，见 SPEC-109-FUSION §6 #7）。Z **不再被本卡读**（原生射击全交回引擎）。
 * ★ 2026-10-01 owner 追加口径：**早苗的固灵结界半径 = 按住 Shift 的时长**（「把 shift 时长当作
 *   某种蓄力」）⇒ Shift（`0x008`）**重新被本卡读**（只读，不改位）。⚠ 旧的「Shift 开固灵吸灵领域」
 *   那条（随活化灵池撤掉）**不恢复**：这里读 Shift 只服务结界的半径。*/
#define POFV_INPUT_CHARGE      0x400u   /* C（= 蓄力电平；engine.h：`0x45c084` 读的 C 位）*/
#define POFV_INPUT_FOCUS       0x008u   /* Shift（= 低速 / 聚焦）—— 只读：早苗结界的「蓄力」输入 */

/* 两个跟随 VM 的**绘制层**（❓ 自定；层号在 `CE_ANM_VM_LAYER`，脚本里那两条都故意不写 `layer()`）：
 * 结界在**灵**之下（结界是地面上的场、灵站在场里）、两者都在自机之下。*/
#define POFV_AURA_VM_LAYER         5
/* ★★ 2026-10-01：灵一旦**没拿到跟随 VM** 就每这么多帧重试一次（见 `ghost_vm_sync` 里那段）。
 *   原来只试一次、失败即永久放弃 ⇒ owner 看到「场上在掉**不可见**的灵」。取 8 帧
 *   （≈0.13 s 一次，灵通常活几百帧 ⇒ 有足够机会补上）。*/
/* ★ 灵贴图的帧保持帧数：4 帧轮播、每帧停这么多（≈0.1 s，与其它 sprite 动画同节奏）。*/
#define POFV_GHOST_FRAME_HOLD      6
#define POFV_GHOST_VM_RETRY        8
#define POFV_GHOST_VM_LAYER        6
/* ★ 一手（2026-10-01）：**收取音** —— TH16 季节道具收进时播 `se_item00`（`0x25`），
 *   116 `season.c` 的 `SEASON_SE_GET` 就是它（一手 `th16:0x42fea5` / `0x42feae`）。
 *   owner 口径「灵力道具得按照**类季节道具**做」⇒ 收取这一下的音效照抄季节道具。*/
/* ★★ 2026-10-01（owner 的 C3/C4 最终模型）：**发射口钉在「版底 boss 位」** ——
 *   「自机都是从下往上打，上面是 boss…我们需要把**另一侧的场地拉过来，并且 180° 反转**！
 *    也就是说，在**我们的版底生成一个 boss**…你发弹得按照**我是 boss 往上发**」。
 *   一手（场地尺寸）：`engine/player/th06nc/01-main-loop-and-collision.md` §arcadeRegion：
 *   `arcadeRegionSize = (384, 448)` ⇒ 本卡取 **x = 384/2 = 192**（正中）、
 *   **y = 440**（版底，留 8 px）。⚠ y 取具体值属 **❓ 自定**（一手只给了 448 这个高）。
 *   一手（怎么钉）：`CE_OPT_POS_X/Y` = 子机的**出膛坐标**，`CE_FN_TICK_SHOOTERS_FOR_CARD`
 *   就是从这里出膛（`engine.h` 里 `0x40a9c0` 的注释）；引擎每帧会按「跟随玩家」重算它，
 *   而 `on_tick_2`（AbilityManager tick，优先级 0x16）在那之后跑 ⇒ **我们在发弹前覆盖即可生效**。*/
/* ★★★ 订正（2026-10-01）：**世界坐标的 x 是「以 0 为中心」的**，不是 `[0, 384]`！
 *   一手（`engine/player/th18/01-position-and-state-timers.md`）：玩家钳位
 *     `x ∈ [-0x5c00, 0x5c00]` ⇒ **±184 px**、`y ∈ [0x1000, 0xd800]` ⇒ **[32, 432] px**
 *   ⇒ 弹幕区 **x ∈ [-192, +192]**（中心 **0**）、**y ∈ [0, 448]**（玩家自己内缩到 [32,432]）。
 *   交叉自洽：世界 ±192 加屏幕偏移 224（= 32 + 192）就是场地的屏幕范围 **32..416**
 *   = `arcadeRegionTopLeftPos (32,16)` + 宽 384 ✓✓。
 *   ⚠ 我原来写的 **192 是「右墙」不是「中心」**（`TRANCE_SPIRIT_BOUNCE_X = 192` 也是墙）——
 *     那版会让**整套弹幕（sht 与 ECL 都是）在屏幕最右边发射**。*/
#define POFV_BOSSATK_ANCHOR_X    0.0f   /* ★ 世界水平中心（订正后）*/
#define POFV_BOSSATK_HALF_W    192.0f   /* ★ 弹幕区半宽（世界 x ∈ ±192）*/
#define POFV_BOSSATK_ANCHOR_Y  440.0f   /* ★ 版底：世界底 = 448、玩家最多到 432 ⇒ 取 440（在玩家够不到、又在场内）*/
/* ★★ 2026-10-01（owner 的混合路线）：**C3/C4 的 ECL 侧载体** —— `assets/ecl/pofv109_boss.ecl.txt`
 *   的 `CEPofvBoss109`（= 由 TH09 `pl00.ecl` 的环弹幕逐条转写来的伪 boss）。
 *   伪 boss 口径：它只是个**发弹用的隐形架子** —— 不设 boss 标志、不挂血条/名字、不用体术判定。*/
#define POFV_BOSSATK_ECL_NAME   "CEPofvBoss109"   /* 必须与 `assets/ecl/pofv109_boss.ecl.txt` 同名 */
/* ★★ 2026-10-01（C3/C4 的「**自伤源**」，owner：「这些攻击是会对敌人造成伤害的」）：
 *   载体的弹是**敌弹**（ECL `etNew` 族）——引擎只会让它们撞**自机**，**不会伤敌机** ⇒ 卡侧补一套。
 *
 *   ❓ **本版是「覆盖区」近似，不是「逐弹命中」**（登记 `TUNING.md` §2.0v-7）：
 *     逐弹版必须能认出「哪颗弹是我们发的」，而敌弹池是**共用**的；唯一的干净判据是弹的**类型**
 *     （弹种表 `0x4c5f90`，stride `0x118`）—— 但**弹结构里存不存类型/类型指针，仓库还没定位**
 *     （`engine/bullet/th18/01` §4 那条 ❓ 就是它）。⇒ 先用「锚点上方的覆盖区」结算，
 *     等那个字段定位了再换成逐弹（接口不用改：都走 `ce_damage_rect`）。
 *   ⚠ 数值（伤害 / 覆盖尺寸 / 持续帧）**一律占位**（owner：「dps 还得重新考虑，可以先填占位」）。*/
/* ★★ 2026-10-01：这三个数**不再凭手感**，而是被一手「每帧总伤害上限 = 90」推出来的
 *   （`sht+0x28`，见 `TUNING.md` 的「上限底值」一节）：
 *     - 旧值 `_MAX = 64 × _DMG = 6` ⇒ 潜在 **384/帧**，**被钳到 90** ⇒ 白建掉 80% 的源；
 *     - 新值 `_MAX = 16 × _DMG = 5` = **80/帧 ≤ 90** ⇒ **一帧都不浪费、也不触发截断** ✓；
 *     - 敌人压在弹幕里一般重叠 1–4 格 ⇒ **5–20/帧**（手感量级与暴风的 40/帧 同阶，偏轻）。
 *   ⚠ 仍是占位（owner 说等实跑）——但它现在**处在一手的约束边界内**，不是随手写的数。*/
/* ★★★ 2026-10-01 三改（owner 实跑：「**无缘无故怎么有暴风，都死了？**」）：
 *   原来 `_DMG = 5` × 最多 16 格 ⇒ 敌人压在弹幕里约 **5–20/帧** ⇒ **屏幕上所有敌人秒死** ⇒
 *   每只都触发一次暴风（`on_enemy_death` 里「击破 ⇒ 起暴风」）⇒ 看着就是「无缘无故到处爆、都死了」。
 *   ⇒ **降到 1**：每帧约 **1–4/帧**（= 重叠格数），小妖精（数百 HP）要几十秒、boss 更久 ✓；
 *     总量上限 16/帧，远低于一手天花板 90 ⇒ 仍然「一帧都不浪费」✓。
 *   ⚠ 仍是占位（owner 说等实跑）——但方向明确：**这一档是「持续伤害」不是「爆发」**。*/
#define POFV_BOSSATK_DMG         1
#define POFV_BOSSATK_DMG_W     384.0f    /* 覆盖宽（= 场地宽 384，一手 `arcadeRegionSize`）*/
#define POFV_BOSSATK_DMG_H     320.0f    /* 覆盖高（自锚点往上）*/
#define POFV_BOSSATK_DMG_LIFE    2       /* 源存活帧（照风暴那一支的量级）*/
#define POFV_BOSSATK_DMG_CELL   24.0f    /* ❓ 逐弹版：每颗弹那一发小方框的边长（占位）*/
#define POFV_BOSSATK_DMG_MAX      16     /* ★ 见上：`_MAX × _DMG = 80 ≤ 90`（一手天花板）*/
#define POFV_BOSSATK_NO_HIT      0x200u  /* ★ 见下：载体的弹必须置这一位才打不到自机 */
/* ★★ 2026-10-01：载体的弹**绝不能打到自机** —— 它是「我」发的，而且从**版底往上**发，
 *   而自机通常就在锚点正上方 ⇒ 不处理的话 = **自己打自己**（每张符卡都变成自杀）。
 *   一手：`bullet+0x20`（`CE_BULLET_FLAGS`）的 **bit `0x200` = 跳过碰撞判定**
 *     （`engine/bullet/th18/01-cancel-to-item.md` 的「未解」条目 1：三个读点 `0x4245a9` /
 *      `0x42486d` / `0x424d78` **都用来跳过碰撞判定**）。
 *   ⚠ 该位的**写侧来源**在本仓仍是 ❓（同一条记着「哪条路把 `0x200` 塞进 flags 未解」）⇒
 *     这是**我们主动置位的首次使用** ⇒ 登记为「待实跑确认」；观感/判定不对就回退这一行。*/

/* ★★ 2026-10-01：C3/C4 的**发射音**（一手 = TH09 `arg9` 的 **bit9** → `0x43e380` = 播音）。
 *   ⚠ **不在 ECL 里做**：TH18 对应的是 `etSound(id, sfx)`(608)，而 **`thecl` 能编、却 dump 不了 608**
 *     （实测 `thecl: error when dumping opcode 608 (etSound)`）—— 本管线**要往返反编译**（自洽 1/2）
 *     ⇒ 用它会把构建弄红。改由卡侧按**同一节拍**（ECL 里每 40 帧一轮）播。
 *   音 = **`0x15` = `se_tan00`**（弾音）—— 一手：名表 `th18:0x4b47a0` 的 `[5] = se_tan00.wav`；
 *     `senro.c` 记「TH18 有三格 se_tan00：`0x15`/`0x18`/`0x1b`」，取第一格。
 *   🟡 一手只给了「要不要播」这一位、没给具体 id ⇒ 这个 id 是**就近取的原生弾音**（登记 `TUNING.md`）。*/
#define POFV_BOSSATK_SE         0x15    /* se_tan00（弾音）*/
#define POFV_BOSSATK_SE_PERIOD  40      /* 与 ECL 的 40 帧一轮同拍 */
static int32_t s_bossatk_se_timer;
/* ★ 扫弹的**轮转起点**（每帧换一批）：见 `bossatk_damage()` 里「公平性」那段长注释。*/
static uint32_t s_bossatk_scan_rot;
#define POFV_POWER_SE_GET       0x25
/* ★ 一手（2026-10-01）：**升档音** —— TH16 季节道具**升档**（`item_collect_season`）播 `0x3f`，
 *   116 `season.c` 的 `SEASON_SE_LEVEL` 就是它（一手出处见那份文件；**不是** `se_extend`）。
 *   本卡的语义对照：**充能升档**（气槽实心格 +1）⇒ 借同一个音（借做法已登记 `AUDIT` §BE #166）。*/
#define POFV_POWER_SE_LEVEL     0x3f
#define POFV_POWER_VM_LAYER        7    /* 灵力道具在最上（要一眼可判「捡得到」）*/
/* ⚠ 2026-10-01 已**废弃**（保留一行说明，免得下一个人再找它）：
 *   灵力道具的贴图从「TH18 原版 P 点 `(0,0,32,32)`」换成了「**TH16 季节点 `sprite519` = (224,0,16,16)`**」
 *   （owner 实跑「样式不对」），画法也改成逐条照抄 TH16 `script129`
 *   ⇒ **缩放整个交给脚本 374 owning**（16 帧 2.0→1.0、之后 300 帧 1.0→0.7），
 *   卡侧**不再每帧写 `ce_anm_set_scale`**（会与 `scaleTime` 打架）。
 *   ⇒ 原 `POFV_POWER_VM_SCALE` 已删；`power_vm_sync()` 里那一行现在什么都不写。*/

/* 消弹调用的 max_count（照 Tenshi 的 0x1869f 量级；本卡两处都不想被截断）。*/
/* ★ 2026-10-01（owner 甜头口径「抵挡护盾」）：`th18:0x45d590` = `Player__cancel_impending_death`
 *   （thiscall(Player*)、无栈参、裸 `ret`）—— 救援层的救命调用。一手与同款用法：
 *   `cards/senro.c`（107 结界）/ `cards/animal.c`（117 咆哮）/ `cards/bomb10.c`（110/111 灵击）/ 113。*/
#define POFV_FN_CANCEL_IMPENDING_DEATH  0x45d590

/* 护盾的**跟随 VM** 尺寸与层（❓ 自定；层与结界同一档、在它上面一点）。 */
/* ★ 外层黄光相对内层白圆的缩放（❓ 自定：外发光比本体大一圈 ⇒ 1.25）。*/
#define POFV_SHIELD_GLOW_SCALE    1.25f
#define POFV_SHIELD_VM_LAYER        8
#define POFV_SHIELD_VM_SCALE     48.0f

/* ★ 2026-10-01（owner）boss attack：卡侧自己申请**一格子机**当发射口
 *   （owner 选了「原生自机弹幕」那条路）⇒ 需要一个子机**本体**脚本。
 *   ★ 2026-10-01 owner 实跑订正：「**默认样式的子机突然多出来一个**」—— 根因就是原来**借**了
 *     `CE_ANM_ABILITY_SCRIPT_SEASON_OPT`（116 季节子机**本体**脚本）⇒ 那个发射口被**画成了**
 *     一颗季节子机。现在改用**自己的一条隐形脚本** `CE_ANM_ABILITY_SCRIPT_POFV109_PORT`
 *     （`alpha(0); stop();` —— 同 `bullet.anm` script0 的「不可见」写法）⇒ 只剩膛口，不画东西。
 *   位置 = 自机（opt offset 0）——弹从自机身上往上出。*/
#define POFV_BOSSATK_OPT_ANM   CE_ANM_ABILITY_SCRIPT_POFV109_PORT

#define POFV_CANCEL_MAX        9999

/* 蓝圈的「有攻击力」：照 Remilia `0x40f478` / season.c 的矩形伤害源写法。
 * ❓ 自定（TODO(th09)：原版爆风伤害未逆到）+ 抬每帧上限（照 Remilia 写 300）。*/
/* ★★ 2026-10-01（owner：「暴风会伤害到屏幕外」）：**场地的世界坐标范围** —— 伤害矩形要裁到这里面。
 *   一手（`engine/player/th18/01-position-and-state-timers.md`）：弹幕区 x ∈ ±192、y ∈ [0,448]
 *   （玩家自己内缩到 ±184 / [32,432]）。*/
/* 场的视觉（脚本 402）画在第几层 + 两种场的染色（`ce_anm_set_color` 是 **AARRGGBB**）。*/
#define POFV_STORM_VM_LAYER        6
#define POFV_FIELD_RGBA_STORM 0xa04080ffu   /* 暴风 = 蓝（代码里一直叫它「蓝圈」）*/
#define POFV_FIELD_RGBA_CA    0xa0ff4040u   /* 蓄力攻击红圈 = 红（一手就叫红圈）*/
/* ★★ 2026-10-01（owner：「把连锁UI做出来」）：连破 UI 的位置/配色/保持帧。
 *   坐标是 **640×480 屏幕空间**（`ce_hud_number_set`/`ce_hud_text` 都收这个空间）。
 *   位置取**左上角**（owner 记的 TH19 位置）；配色取暖橙（与 TH19 的计数器同族的暖色）。*/
/* ★ 一手：TH19 那个计数器 UI 的**显示门槛 = 10**（`0x51d84e cmpl $0xa` / `0x51d855 jl`）。*/
/* ★★ 2026-10-01（owner 给了字体出处「看字体基本就是这两个之一」）：hits UI **自带字形**。
 *   8 台 VM = 4 个标签字形（H/I/T/S）+ 最多 4 位数字；字形来自 TH19 `ascii_norm2.png` 的切片
 *   （见 `assets/ability/make_pofv109_hits_art.py`），画法复用 `scripts/105_hud_number.anm.txt`
 *   （单 sprite VM，卡侧 `ce_anm_set_sprite` 换字形）——**不再借用共享的 `ATLAS_HUD_DIGIT`**
 *   （那套是真银圣剑的 `ability/truesilver/DIGIT_*.png` ✗）。*/
/* ★ 描边那套字形（`*_EDGE`）的染色 = **黑**（AARRGGBB）；差值 = 同一字形的白字号 → 描边号。*/
#define POFV_HITS_EDGE_RGBA  0xff000000u
/* ★ ORDER.txt 里每个字形是「白、描边」**相邻两条** ⇒ 描边号 = 白字号 + 1（已核：D0=577/D0_EDGE=578）。*/
#define POFV_HITS_EDGE_DELTA       1u
#define POFV_HITS_UI_VMS          16   /* 8 个字形 × (描边 + 白字) */
#define POFV_HITS_UI_DIGITS        4
#define POFV_HITS_UI_PITCH      14.0f   /* ❓ 自定：显示步距（一手 24 是**图集**步距，不是显示间距）*/
#define POFV_HITS_UI_GAP         6.0f   /* 标签与数字之间的空档 */
#define POFV_HITS_UI_MIN          10
#define POFV_CHAIN_UI_X         52.0f
#define POFV_CHAIN_UI_Y         40.0f
#define POFV_CHAIN_UI_DX        22.0f   /* 数字字距 */
#define POFV_CHAIN_UI_HOLD      90     /* 最后一次连破之后还显示多少帧（1.5 s）*/
#define POFV_CHAIN_UI_BGRA 0xff40a0ffu /* 暖橙（AARRGGBB）*/
#define POFV_FIELD_HALF_W      192.0f
#define POFV_FIELD_H         448.0f
#define POFV_DMG_CAP           300
#define POFV_BLAST_DMG           40
#define POFV_BLAST_DMG_LIFE       2

/* ★★ 2026-09-20 改（P1 #th09-2「一直会播放奇怪的 Extend 音效」）：
 *   原值 = `CE_SE_EXTEND`（TH18 `0x11` = `se_extend.wav`）—— 那只是"听着像叮"的 ❓自定借用，
 *   每升一级响一次残机奖励音，用户实跑听成"一直在响的奇怪 Extend 音效"。
 *   **一手（已逆出 TH09 v1.50a 的音效表与播放站点，直读 `local/th09.v1.50a/th09.exe`）**：
 *   · id 表 = `th09:0x4a1f80`（54 项 × 8 字节：`+0` = wav 名表下标、`+4` = 音量 word）、
 *     名表 = `th09:0x4a2130`（39 项）、播放器 = `th09:0x43e2f0`（id + 裸声像）/ `th09:0x43e380`（id + 世界 x）。
 *   · 蓄力 gauge = `player+0x30384`（float，每 100 帧一档：`th09:0x41fa88 imul 0x51eb851f; sar edx,5`）。
 *   · **起手那一声**：`th09:0x41fa5e push 0x2b` + `0x41fa65 call 0x43e2f0`，条件 = 「gauge == 0」
 *     ⇒ id `0x2b` = **`se_charge00.wav`**。
 *   · **每升一级那一声**：`th09:0x41fb31 push 0x32` + `0x41fb38 call 0x43e2f0`，条件 = 「
 *     `(int)gauge/100` 比上一帧大」（`th09:0x41fb15 cmp edi,eax` / `jge`）⇒ id `0x32` = **`se_chargeup.wav`**。
 *   ⇒ 本卡的「升级 → 叮一声」= 一手的**每升一级**那一声 ⇒ 取 `se_chargeup`（TH18 71 枚里**没有**这枚，
 *     走音效表扩容通道：`CE_VOICE_CHARGEUP` = `0x5a`，源 `local/th09.v1.50a/th09.dat` 里单抽，
 *     `assets/voice/derive_retail_se.py` 的 `TABLE` 有完整一手出处）。
 *   ⚠ 一手里这一格的音量是 `-900`（百分之一 dB），我们**不衰减**（`voice.js` 里 `volume_db: 0`）
 *     —— 原值太轻，听不出"叮"；这是唯一一处刻意偏离一手的量（登记 `TUNING.md` §2.0t）。
 *   ⚠ 一手还有另外两声（起手 `se_charge00`、放出去 `se_exattack` `0x2d` `th09:0x41fa5e` 附近），
 *     本卡**没接**（用户报的只是"升级音不对"）—— 要补的话各一行，出处已记在上面的表里。*/
#define POFV_SE_CHARGE_DING     CE_VOICE_CHARGEUP

/* ============================ 卡侧状态 ============================ */

/* 池是**文件级**：`ce_state` 一张卡只有 496 字节（`CE_STATE_BYTES` 512 − `CE_STATE_RESERVED` 16），
 * 装不下 8 个暴风 + 48 颗灵力道具 + 4 只灵 ⇒ 照 116 / 117 的先例放文件级静态池（`animal.c` 的 `s_tok`）。
 * ⚠ 代价：清理必须**完全不依赖 tick**（商店里 tick 停着）⇒ 两条删卡路都在 `clear_card()` 里清。*/
static pofv_storm_pool_t  s_storm;
static pofv_power_pool_t  s_power;      /* ★ 2026-10-01（口径 5）：灵力道具池（自建实体，非原生道具）*/
static pofv_ghost_pool_t  s_ghost;      /* ★ 2026-10-01（口径 8/9/10）：灵的**句柄池**（本体是真敌人）*/
static int32_t            s_ghost_vm[POFV_GHOST_MAX];   /* 每只灵的**跟随 VM** id（0 = 没有）*/
/* ★ 每个**场**（暴风 / CA 红圈）的跟随 VM id（0 = 没有）；驱动在 `storm_vm_sync()`。*/
static int32_t            s_storm_vm[POFV_STORM_MAX];
/* ★ 109 自己的 hits UI 字形 VM（4 标签 + 4 数字；见 `hits_ui_sync()`）。*/
/* ★★ 2026-10-01（owner：「**至少等到第一波怪出现再从场外出灵吧**」）：
 *   闩锁 —— 场上**真出现过敌人**之后才置 1；在那之前一只灵都不撒（开局不再凭空冒灵 ✗）。
 *   ⚠ 声明必须在 `clear_card()`（它会清零）之前 ⇒ 放在这一组静态量里。*/
static int32_t            s_ghost_wave_seen;
static uint32_t           s_hits_vm[POFV_HITS_UI_VMS];
static int32_t            s_frame;      /* 游玩帧计数（连爆窗口的帧源；只在 on_tick_2 前进）*/
/* 诊断账（都在 `clear_card()` 里清；`verify.py` 的场景吃这几行）。*/
static int32_t            s_power_got;  /* 本次段累计收取的灵力道具颗数 */
static int32_t            s_aura_hits;  /* 本次段结界碰敌弹的累计颗数 */
static int32_t            s_boss_carrier;               /* ★ 伪 boss 载体 enemy_id（0 = 没有）*/
static int32_t            s_power_vm[POFV_POWER_MAX];   /* 每颗灵力道具的跟随 VM id（0 = 没有）*/

typedef struct {
    ce_card_t  *card;
    pofv_state_t st;
    int32_t     prev_charge;    /* 上一帧 C（边沿检测；合成卡 2026-09-28 起取代 prev_shot）*/
    pofv_invuln_t invuln;       /* CA 无敌窗口（声明层：每帧声明剩余量 ⇒ 不会被别人截短；
                                 * 状态机在 `pofv_core.c` 的 `pofv_invuln_*`，有主机单测）*/
    int32_t     ghost_logged;   /* 本次「灵池满 / 没登记上」只记一行（防刷屏）*/
    int32_t     cap_logged;     /* 本次「爆风队满丢弃」只记一行 */
    int32_t     power_logged;   /* 本次「灵力道具池满丢弃」只记一行 */
    int32_t     shield_logged;  /* 本次「护盾吃掉一发」只记一行 */
    pofv_bossatk_t bossatk;     /* ★ boss attack（C3/C4；见 pofv_core.h §I）*/
    ce_option_t opt;            /* ★ 发射口（自己申请的**一格子机**）*/
    int32_t     bossatk_logged; /* 「boss attack 起手」只记一行 */
    /* ★★ 2026-10-01（owner：「**把连锁UI做出来**」/「th19 左上角是有连锁UI的」）：
     *   左上角的**连破档**显示（`ce_hud_number_t` = 用 ANM 图元画的数字，TH18 原生 font 2 字形；
     *   文本标签走 `ce_hud_text`）。 */
    ce_hud_number_t chain_num;
    int32_t     chain_ui_on;    /* 上一帧是否在显示（用于「关掉时清一次」）*/
} pofv_card_t;

_Static_assert(sizeof(pofv_card_t) <= 496 - 16, "ce_state 用户区上限 496 字节");

/* ★ hits UI 的两个函数（定义在文件后段；`clear_card()` 要用 `hits_ui_kill` ⇒ 在结构体之后先声明）。*/
static void hits_ui_kill(void);
static void hits_ui_sync(pofv_card_t *s);

static pofv_card_t *st(ce_card_t *c)
{
    pofv_card_t *s = ce_state(c, pofv_card_t);
    if (s) s->card = c;
    return s;
}

/* ============================ 消弹调用点（两条路） ============================ */

typedef void (__attribute__((thiscall)) *pofv_fn_bullet_cancel_t)(void *bullet, int mode);

/* ★★ 2026-10-01（真 bug 预防）：这颗**敌弹**是不是**我们自己**的 C3/C4 弹幕（伪 boss 载体发的）？
 *
 *   是 ⇒ **结界 / 暴风 / CA 场都不吃它**。理由：载体的弹走的是**敌弹池**，而我们的三种清弹机制
 *   都是「扫敌弹池」⇒ 不排除的话，**我们自己的符卡会被自己的清弹吃掉**（观感直接坏掉，
 *   而且暴风还会拿自己的弹去连爆/回气）。
 *
 *   判据 = **弹种类型**在不在我们 ECL 用到的集合里（`pofv_bossatk_bullet_type_ours()`）；
 *   类型指针的一手出处见 `engine.h` 的 `CE_BULLET_TYPE_REC`（`th18:0x42589b`）。*/
static int is_our_barrage(const uint8_t *b)
{
    uint32_t rec = *(const uint32_t *)(b + CE_BULLET_TYPE_REC);
    if (rec < CE_BULLET_TYPE_TABLE) return 0;
    if ((rec - CE_BULLET_TYPE_TABLE) % CE_BULLET_TYPE_STRIDE) return 0;
    return pofv_bossatk_bullet_type_ours((rec - CE_BULLET_TYPE_TABLE) / CE_BULLET_TYPE_STRIDE);
}

/* 路 ①：**只消白弹**（蓝圈）—— 引擎没有带 exclude 的半径消弹接口（review §6.4）⇒ 自己扫池。
 * 命中判据**逐字照抄** `0x429370`：state ∈ {1,2}、dist² ≤ (弹半径 × 0.5 + R)²
 * —— 这里的「弹半径」用的是**这颗弹自己的** `+0x658`（`br * 0.5f + r`），与引擎一致；
 * T = 4.0（一手 §5.4：半径 ≤ 4.0 才算白弹）只作**筛选门**（`pofv_is_white_bullet`），
 * **不**替换判定式里的弹半径。⚠ 2026-09-12 审计订正：本注释原来写「把弹半径 × 0.5 换成 T」，
 * 与下面的代码不符（代码是对的、注释是错的）。
 * ⚠ 跳过 `+0x24` 的不可消位**不适用于这里**：本函数只按半径筛（红圈才需要「消一切」）。
 * 成本：2000 槽 × 1 次比较（早停于 max_count）——每帧最多 POFV_STORM_SCANS_PER_FRAME 次（暴风场）。 */
static int cancel_white_in_circle(const float *pos, float r, int max_count, int mode)
{
    uint8_t *bm = CE_BULLET_MGR();
    uint32_t i;
    int n = 0;

    if (!bm || !pos || r <= 0.0f || max_count <= 0) return 0;
    /* ★ 公平性（2026-10-01）：扫池是**定序**的，而每帧只允许建 `POFV_BOSSATK_DMG_MAX` 个源
     *   ⇒ 不加轮转的话，**每帧都是同一批（池里靠前的）弹拿到伤害源**，池里靠后的弹**永远轮不到**
     *   （观感就是「同屏的弹有的有判定、有的没有」）。⇒ 每帧把起点往前挪一个「批次」，
     *   整个池 `CE_BM_BULLET_COUNT / MAX` 帧走一圈，所有弹**均摊**到伤害源。*/
    for (i = 0; i < CE_BM_BULLET_COUNT; ++i) {
        uint32_t _slot = (i + s_bossatk_scan_rot) % CE_BM_BULLET_COUNT;
        uint8_t *b = bm + CE_BM_BULLETS + (size_t)_slot * CE_BULLET_STRIDE;
        uint16_t state = *(const uint16_t *)(b + CE_BULLET_STATE);
        if (is_our_barrage(b)) continue;      /* ★ 我们自己发的 C3/C4 弹幕：三种清弹都不吃 */
        float br, dx, dy, rr;
        if (state != 1 && state != 2) continue;        /* 0 = 空槽；3 / 4 = 别的状态（★ 2026-09-12 asm：
                                                        * 三个消弹入口写的是 **4**，3 来自另一条路
                                                        * `0x4249be`；本卡只认 1/2，两种写法都安全）*/
        br = *(const float *)(b + POFV_BULLET_RADIUS);
        if (!pofv_is_white_bullet(br)) continue;       /* ★ 白弹阈值：0.0 的哨兵弹种在这里被跳掉 */
        dx = *(const float *)(b + CE_BULLET_POS) - pos[0];
        dy = *(const float *)(b + CE_BULLET_POS + 4) - pos[1];
        rr = br * 0.5f + r;                            /* 照 0x429370 的判定式 */
        if (dx * dx + dy * dy > rr * rr) continue;
        ((pofv_fn_bullet_cancel_t)POFV_FN_BULLET_CANCEL)(b, mode);
        if (++n >= max_count) break;
    }
    return n;
}

/* 路 ②：**圈内所有弹幕**（红圈）—— 调引擎的 `ce_cancel_radius`（`0x429370`），
 * ★ **消弹颗数用它自己的返回值**（sdk.h：调前清 `CE_BM_CANCEL_COUNTER` `0x7a41e8`、调后读 = 消掉几颗）。
 * mode=0 = 纯删除（一手「红圈消掉的白弹**直接消失**」）。
 * ⚠ 覆盖范围**不是「真的所有」**：`0x429370` 查 `bullet+0x24`（`0x4293c8 cmp [esi+0x24],0 / jne`）⇒
 *   带这个「保护位」的弹**红圈消不掉**（要消它只能自己扫池逐弹调 `0x428e90`，见路 ①）。
 *   半径型「不可消」弹（`+0x658 > 4.0`）没有这个位 ⇒ 照旧会被红圈消掉，spec 的「C4 全屏清干净」成立。
 *   一手引擎面依据见本文件头的裸地址段（2026-09-12 asm 复核）。*/
static int cancel_all_in_circle(const float *pos, float r, int max_count, int mode)
{
    return ce_cancel_radius(pos, r, max_count, mode);
}

static int pofv_cancel_circle(const float *pos, float r, int white_only, int max_count, int mode)
{
    return white_only ? cancel_white_in_circle(pos, r, max_count, mode)
                      : cancel_all_in_circle(pos, r, max_count, mode);
}

/* ============================ 固灵结界（口径 4 / 6） ============================
 * 键位（口径 4）：**常驻**（与 C 的档位无关；C 只管蓄力）。几何 = 一手 TH19 Reimu 圆
 *   （`pofv_aura_hit`，§F）。**不做减速**（口径 3：TH19 的三个 `0.5f` 一个都不写）。
 * 命中链（口径 6）：碰敌弹 ⇒ 掉**灵力道具**（每颗弹只算一次 + 每帧上限）；
 *   对灵 ⇒ 沈静化（见 §「灵」那一节）。
 * ⚠ 结界**不消弹** —— 消弹是暴风（蓝圈）与 CA（红圈）的事。 */

/* ★ 「每颗弹只算一次」的闩锁（口径 6）。槽被回收（state 归 0）时现场清零
 *   ⇒ 同一槽里的新弹可以再掉一次。
 *   ⚠ 与弹池同尺寸（`CE_BM_BULLET_COUNT` = 2000）⇒ 2 KB，放文件级静态区（同两个池的先例）。
 *   ❓ 自定：原版的「每颗弹只算一次」闩锁位没用弹上的死字段（本轮没找到干净的死字段），
 *     而是卡侧按**槽号**记 —— 代价是「同一帧内弹被消掉、新弹复用同槽」会漏掉一次掉落（极罕见）。*/
static uint8_t s_aura_done[CE_BM_BULLET_COUNT];

/* ---- ★★ 2026-10-01 owner 追加口径：结界**按自机角色**取不同游戏的版本 ----
 *   「固灵结界 **咲夜取 th09，早苗取 th19**，灵梦和魔理沙**待定**」＋
 *   「（早苗的半径）**把 shift 时长当作某种蓄力**」。档案表在 `pofv_core.h` §F（有主机单测）。
 *
 * 本帧算出来的三件东西放在文件级（`ghost_tick` / 弹扫描 / VM 都要用同一份）：
 *   `s_aura`（档案）/ `s_aura_cx,cy`（圆心）/ `s_aura_r`（判定半径 = 绘制半径）。 */
static pofv_aura_profile_t s_aura;
static float   s_aura_cx, s_aura_cy;    /* 圆心：STATIC = 自机；FIELD = **锚定点**（一手 TH09 的锚定语义）*/
static float   s_aura_r;                /* 本帧的判定半径（≤ 0 = 结界没展开）*/
static int32_t s_aura_age;              /* FIELD：本个场已活了几帧（−1 = 需要（重）出生）*/
static int32_t s_shift_frames;          /* 连续按住 Shift 的帧数（早苗的「蓄力」）*/
static int32_t s_aura_char = -1;        /* 上一次打过日志的角色号（换角色时打一行，便于实跑判读）*/
static pofv_storm_profile_t s_storm_prof;   /* ★ 本帧的暴风档案（按自机角色；owner 第四/第五条）*/

/* 结界的**跟随 VM**（判定半径 = 绘制半径，一手 TH19 §7.1/§9.2）。
 * 懒建：第一帧 tick 时起（`on_ctor` 发生在商店里，ANM 那边不一定就绪）；
 * `clear_card()` 里删（删卡那一刻 tick 停着 ⇒ 必须自己收，METHOD 铁律）。*/
static int32_t s_aura_vm;
static int32_t s_shield_vm;             /* ★ 护盾**内层白圆**的跟随 VM（0 = 没起；见 §H）*/
static int32_t s_shield_vm2;            /* ★ 护盾**外层黄光**的跟随 VM（先起 ⇒ 画在下面）*/
static int32_t s_shield_broken_log;     /* 破盾那一发只打一行日志 */

/* 每帧一次：选档案 → 推 Shift 蓄力 → 算圆心/半径 → 同步 VM。之后再拿这份状态去判弹与灵。 */
static void aura_update(void)
{
    const float *pp = ce_player_pos();
    int character = *(const int32_t *)CE_ADDR_CHARACTER;

    pofv_aura_profile_for(character, &s_aura);
    pofv_storm_profile_for(character, &s_storm_prof);   /* ★ 暴风也按角色（owner 第四/第五条）*/
    if (character != s_aura_char) {          /* 换角色 / 首次：打一行（实跑判读用）*/
        s_aura_char = character;
        ce_log("pofv: aura profile for character %d -> mode %d (%s), pad_linear %d, "
               "r_fixed %d, K %d, r0 %d, dr %.2f, life %d, grow %d",
               character, (int)s_aura.mode,
               s_aura.mode == POFV_AURA_FIELD ? "TH09 anchored field" : "TH19 growing circle",
               (int)s_aura.pad_linear, (int)s_aura.r_fixed, (int)s_aura.k,
               (int)s_aura.r0, (double)s_aura.dr, (int)s_aura.life, (int)s_aura.grow_frames);
        ce_log("pofv: storm profile -> chain %d, kills_ghosts %d (%s)",
               (int)s_storm_prof.chain, (int)s_storm_prof.kills_ghosts,
               s_storm_prof.chain ? "TH09 chain storm" : "TH19 cancel-only storm");
    }

    /* Shift 时长 → TH19 的蓄力量（owner 口径；❓ 每帧 2 ⇒ 100 帧满）*/
    if (CE_INPUT_HELD() & POFV_INPUT_FOCUS) {
        if (s_shift_frames < 1000000) ++s_shift_frames;
    } else {
        s_shift_frames = 0;
    }

    /* ★★★ 2026-10-01 owner **订正**（实跑 #1）：「一手 **th09 和 th19 都是蓄力同时张开结界**，
     *   有时结界大小还和蓄力时长有关。目前我们拆分了，**蓄力走 C，结界走 shift，但是其他什么都没变**」。
     *   ⇒ **Shift = 张开/收起结界的那只手**；**没按住 Shift = 结界根本没张开** ——
     *     判定半径 0（不判弹、不沈静化）、UI 不画。**不是**一进关就挂着一个渐变圆。
     *   ⚠ 这条**覆盖**了早先「口径 4 = 常驻」的读法（那时把「常驻」读成了「一直画着」；
     *     一手的「常驻」指的是**它跟着蓄力一起开着**，不是「独立于蓄力的常驻 UI」）。*/
    if (s_shift_frames <= 0) {
        s_aura_age = -1;          /* 下次按下 = 重新张开（咲夜的锚点取按下那一刻的自机位置）*/
        s_aura_r = 0.0f;          /* 判定半径 0 ⇒ 不判弹 / 不沈静化 / 跟随 VM 的 scale 也是 0 */
    } else if (s_aura.mode == POFV_AURA_FIELD) {
        /* ★ TH09 的锚定场：圆心**锚在张开那一刻的自机位置**、逐帧扩张；按住期间一直长。*/
        if (s_aura_age < 0) {
            if (pp) { s_aura_cx = pp[0]; s_aura_cy = pp[1]; }
            s_aura_age = 0;
        }
        pofv_aura_radius(&s_aura, s_aura_age, 0, &s_aura_r);
        /* 按住期间**只长这一个场**（不再「到点在自机处重生」—— 那是旧口径；现在松开 Shift
         * 就整个收起，再按一次才重新张开 ⇒ 与一手「蓄力张开、松手收起」同形）。*/
        if (s_aura_age < 1000000) ++s_aura_age;
    } else {
        /* ★ STATIC（TH19 系 / 占位档）：**也跟着 `dr` 长**（owner：「aura 和锚定场本质上是同一个
         *   东西」），只是**长到蓄力给的上限就停**；Shift 一松、蓄力归零 ⇒ 缩回没有、下次从 0 长。*/
        int32_t charge = pofv_aura_shift_charge(s_shift_frames);
        if (pp) { s_aura_cx = pp[0]; s_aura_cy = pp[1]; }
        if (s_aura_age < 0) s_aura_age = 0;
        if (charge <= 0) s_aura_age = 0;
        /* 占位档（`k == 0`，灵梦 / 魔理沙 / 未识别）不吃蓄力 ⇒ 半径恒为 `r_fixed`；
         *   它同样被上面那道 **Shift 门**管着（没按 = 0）—— 「一上来就是一个渐变圆 UI」正是它之前的毛病。*/
        pofv_aura_radius(&s_aura, s_aura_age, charge, &s_aura_r);
        if (s_aura_age < 1000000) ++s_aura_age;
    }

    /* 视觉：懒建 + 每帧圆心/缩放（半径 ≤ 0 ⇒ 缩放 0 = 看不见）*/
    if (!s_aura_vm)
        s_aura_vm = (int32_t)ce_anm_spawn(CE_ABILITY_ANM(), CE_ANM_ABILITY_SCRIPT_POFV109_AURA,
                                          POFV_AURA_VM_LAYER);
    if (s_aura_vm) {
        float sc = s_aura_r > 0.0f ? s_aura_r : 0.0f;
        ce_anm_set_pos((uint32_t)s_aura_vm, s_aura_cx, s_aura_cy, 0.0f);
        ce_anm_set_scale((uint32_t)s_aura_vm, sc, sc);   /* ★ 判定 = 绘制 */
    }
}

/* ★ 护盾（§H）的跟随 VM：**只在有盾时**显示；破盾那一帧发一次 anm 中断 1（= 一手破壊点给视觉弹
 *   发的事件 1）然后隐掉。用的是同一台 VM（不反复建删 ⇒ 不会有 VM 生命周期残留）。*/
static void shield_vm_update(const pofv_card_t *s)
{
    const float *pp = ce_player_pos();
    int up = pofv_shield_up(&s->st);

    if (!pp) return;
    /* ★★★ 2026-10-01（owner：「罩子的指示是一个白色半透明的圆形，**外面是黄色的外发光**」＋
     *   「**缺少罩子**」）：**两层 VM** —— 外层黄光（脚本 403）**先起** ⇒ 引擎「后起者画在上面」
     *   ⇒ 黄光在**下**、白圆在**上** ✓；两台的坐标相同、黄的那台**放大一圈** ✓。
     *   ⚠ 为什么不用 TH19 那种一条指令的双色圆：那是 **`ins_617`**，**TH18 的 anm 解释器不认得** ✗
     *     （与早先 ECL 在 609+ 上两作编号不同同一类坑）⇒ 用两台 TH18 原生 `drawPoly` 等价实现 ✓。*/
    if (!s_shield_vm2)
        s_shield_vm2 = (int32_t)ce_anm_spawn(CE_ABILITY_ANM(), CE_ANM_ABILITY_SCRIPT_POFV109_SHIELD_GLOW,
                                             POFV_SHIELD_VM_LAYER);
    if (!s_shield_vm)
        s_shield_vm = (int32_t)ce_anm_spawn(CE_ABILITY_ANM(), CE_ANM_ABILITY_SCRIPT_POFV109_SHIELD,
                                            POFV_SHIELD_VM_LAYER);
    if (s_shield_vm2) {
        float sc2 = up ? POFV_SHIELD_VM_SCALE * POFV_SHIELD_GLOW_SCALE : 0.0f;
        ce_anm_set_pos((uint32_t)s_shield_vm2, pp[0], pp[1], 0.0f);
        ce_anm_set_scale((uint32_t)s_shield_vm2, sc2, sc2);
    }
    if (!s_shield_vm) return;
    ce_anm_set_pos((uint32_t)s_shield_vm, pp[0], pp[1], 0.0f);
    ce_anm_set_scale((uint32_t)s_shield_vm, up ? POFV_SHIELD_VM_SCALE : 0.0f,
                     up ? POFV_SHIELD_VM_SCALE : 0.0f);
    if (!up && s_shield_broken_log) {            /* 从「有」掉到「无」的那一帧：破盾一闪 */
        ce_anm_interrupt((uint32_t)s_shield_vm, 1);
        s_shield_broken_log = 0;
    }
}

/* ★ boss attack 的开火（owner 选了**原生自机弹幕**那条路）：
 *   每帧把本卡自己的计时器当 `short_timer` 传进 `ce_tick_shooters_for_card()`，
 *   于是 shooter 行里的 `fire_rate` = 「每 N 个我们的计数发一次」（同豌豆先例）。
 *   行数据在 `assets/sht/append_shooterset.py` 的 `_pofv109_bossatk_sets()`：
 *   C3 = 1 条（`CE_SHT_SET_POFV109_BOSSATK_C3`）/ C4 = 3 条 ±8° 扇面（`_C4`），全行 `angle = −π/2`
 *   = **由下至上**（owner：「对战作是由上至下，这里面只有我自己，所以是由下至上」）。
 *   ⚠ 观感：owner「最好观感一致」⇒ 贴图最终要换成**敌弹样式 + alpha ≤128**（TUNING §2.0v-4 #51/#52）；
 *     现在借的是自机弹样式（按 owner 的透明度铁律**豁免**额外 alpha）。*/
/* ★★ 2026-10-01：**伪 boss 载体**（C3/C4 的 ECL 侧）的生命周期 —— 刷 / 每帧喂 / 喊走。
 *
 *   载体 = `CEPofvBoss109`（`assets/ecl/pofv109_boss.ecl.txt`）：
 *     · 本体隐形（ECL 侧 `anmSetSprite(0,-1)`），判定收到 1×1 ⇒ **不挡自机、没有体术判定**；
 *     · 位置由这里每帧写 `%F2/%F3`（= 版底锚点）—— 与灵同一套通道；
 *     · `$I0` = 状态（0 正常 / 999 喊走）；`$I1` = **档位**（1..4 ⇒ ECL 里算路数与速度）。
 *   ⚠ 伪 boss 口径（owner）：「**其他什么 boss 标识都别干**」⇒ 本文件对它**只**做三件事：
 *     刷（`ce_spawn_enemy`）、每帧喂坐标与档位、喊走（`$I0 = 999`）。
 *     不设 boss 标志、不挂血条/名字、不进 boss 表、不做出场特效。
 *   ⚠ 长持续效果纪律：它跨帧存在 ⇒ 删卡/中断的路（`clear_card`）里必须喊走（见本文件 `clear_card`）。*/
static void bossatk_carrier_stop(void)
{
    uint8_t *e;
    if (!s_boss_carrier) return;
    e = ce_enemy_by_id(CE_ENEMY_MGR(), s_boss_carrier);
    if (e) *(int32_t *)(e + CE_ENEMY_ECL_I0) = 999;     /* 喊走：ECL 侧飞离自删 */
    ce_log("pofv: boss carrier %d told to leave", (int)s_boss_carrier);
    s_boss_carrier = 0;
    s_bossatk_se_timer = 0;
}

static void bossatk_carrier_sync(int32_t level)
{
    uint8_t spawn[CE_SPAWN_SIZE];
    uint8_t *e;
    float *sp, *ef;

    if (s_boss_carrier) {
        e = ce_enemy_by_id(CE_ENEMY_MGR(), s_boss_carrier);
        if (!e) { s_boss_carrier = 0; }                 /* 它自己没了（被删/离场）*/
    }
    if (!s_boss_carrier) {
        memset(spawn, 0, sizeof spawn);
        sp = (float *)(spawn + CE_SPAWN_POS);
        sp[0] = POFV_BOSSATK_ANCHOR_X;                  /* ★ 版底锚点 */
        sp[1] = POFV_BOSSATK_ANCHOR_Y;
        sp[2] = 0.0f;
        *(int32_t *)(spawn + CE_SPAWN_SCORE) = 0;
        *(int32_t *)(spawn + CE_SPAWN_MAIN_DROP) = 0;   /* 奖励全走灵力道具（口径 5）*/
        *(int32_t *)(spawn + CE_SPAWN_LIFE) = 999;      /* ⚠ < 1000（引擎的锁定阈值，见 engine.h）*/
        *(int32_t *)(spawn + CE_SPAWN_ECL_I + 0) = 0;   /* $I0 = 正常 */
        *(int32_t *)(spawn + CE_SPAWN_ECL_I + 4) = level;        /* $I1 = 档位 */
        *(int32_t *)(spawn + CE_SPAWN_ECL_I + 8) =
            *(const int32_t *)CE_ADDR_CHARACTER;                /* $I2 = 角色（0 灵梦/1 魔理沙/2 咲夜）*/
        ef = (float *)(spawn + CE_SPAWN_ECL_F);
        ef[2] = sp[0];                                  /* %F2/%F3 = 初始目标（ECL 第一帧就走过去）*/
        ef[3] = sp[1];
        e = (uint8_t *)ce_spawn_enemy(POFV_BOSSATK_ECL_NAME, spawn);
        if (!e) { ce_log("pofv: boss carrier spawn FAILED (pool / ECL)"); return; }
        s_boss_carrier = *(int32_t *)(e + CE_ENEMY_ID);
        /* ★ 免伤：它是架子不是敌人（ECL 侧判定已是 1×1，这里再加一道引擎闸，防自机弹误伤）*/
        *(uint32_t *)(e + CE_ENEMY_FLAGS) |= CE_ENEMY_FLAG_NO_DAMAGE;
        ce_log("pofv: boss carrier spawn id %d at (%d,%d) level %d",
               (int)s_boss_carrier, (int)sp[0], (int)sp[1], (int)level);
    }
    e = ce_enemy_by_id(CE_ENEMY_MGR(), s_boss_carrier);
    if (!e) return;
    *(float *)(e + CE_ENEMY_ECL_F2) = POFV_BOSSATK_ANCHOR_X;    /* 钉在版底锚点 */
    *(float *)(e + CE_ENEMY_ECL_F3) = POFV_BOSSATK_ANCHOR_Y;
    *(int32_t *)(e + CE_ENEMY_ECL_I0 + 4) = level;              /* $I1 = 档位 */
    *(int32_t *)(e + CE_ENEMY_ECL_I0 + 8) =
        *(const int32_t *)CE_ADDR_CHARACTER;                    /* $I2 = 角色 */
    /* ★ 发射音（见常量处）：与 ECL 的 40 帧一轮同拍；首次也响（计时器 0 起算 ⇒ 先 ++ 再判）。*/
    if (++s_bossatk_se_timer >= POFV_BOSSATK_SE_PERIOD) {
        s_bossatk_se_timer = 0;
        ce_play_sound(POFV_BOSSATK_SE, POFV_BOSSATK_ANCHOR_X);
    }
}

/* ★ C3/C4 的「自伤源」（近似；见常量处的长注释）：在**锚点上方的覆盖区**里每帧结算一次伤害。
 *   中心 = 锚点往上半个覆盖高（这样覆盖区正好盖住朝上发的弹幕）。*/
static void bossatk_damage(void)
{
    uint8_t *bm = CE_BULLET_MGR();
    uint32_t i;
    int made = 0;

    if (!bm) return;
    /* ★★ 2026-10-01（**逐弹版**，owner：「这些攻击是会对敌人造成伤害的」）：
     *   逐颗扫敌弹，只挑「**弹种属于我们那几对 `(type, subtype)`**」且**在弹幕覆盖区内**的，
     *   在**每颗弹的位置**各来一发 `ce_damage_rect`（小方框）⇒ 命中感 = 弹打到谁谁掉血。
     *   一手（弹种指针）：`CE_BULLET_TYPE_REC`（`engine.h`，`0x42589b`）。
     *   ⚠ 两道过滤缺一不可：① 类型（我们 ECL 用到的 `etSprite` 类型集合）；
     *     ② 覆盖区（`x` 在场地内、`y` 自锚点往上一个高）—— 因为类型 0/2 之类的**原生敌人也在用**，
     *     只按类型会把别人的弹也算进来（那道闸就是为此留的）。*/
    for (i = 0; i < CE_BM_BULLET_COUNT; ++i) {
        uint8_t *b = bm + CE_BM_BULLETS + (size_t)i * CE_BULLET_STRIDE;
        uint16_t state = *(const uint16_t *)(b + CE_BULLET_STATE);
        uint32_t rec;
        uint32_t type;
        float bx, by, vx, vy;
        float pos[3];

        if (state != 1 && state != 2) continue;
        rec = *(const uint32_t *)(b + CE_BULLET_TYPE_REC);
        if (rec < CE_BULLET_TYPE_TABLE) continue;                  /* 还没建/不合法 */
        if ((rec - CE_BULLET_TYPE_TABLE) % CE_BULLET_TYPE_STRIDE) continue;   /* 不在表上（对齐校验）*/
        type = (rec - CE_BULLET_TYPE_TABLE) / CE_BULLET_TYPE_STRIDE;
        if (!pofv_bossatk_bullet_type_ours(type)) continue;         /* ① 弹型属于我们 ECL 用到的集合 */

        /* ★★★ 2026-10-01 **重排（修两个真 bug）**：四道过滤必须在**施加任何效果之前**全部跑完。
         *   ① 原实现把「置免撞位」写在覆盖区判定**之前** ⇒ **任何**「弹型撞上我们那 8 个之一 + 往上飞」
         *      的弹（**包括原生敌人的弹**）都会被打上免撞 ⇒ **原生弹变无害** ✗；
         *   ② 原「从锚点发散」那道判定用的 `bx/by` **当帧还没赋值**（读栈上垃圾）⇒ 过滤不可预测 ✗。
         *   ⇒ 现在顺序：① 弹型 → ② 位置（覆盖区）→ ③ 方向 → ④ 发散 → **然后才动手**。*/
        bx = *(const float *)(b + CE_BULLET_POS);
        by = *(const float *)(b + CE_BULLET_POS + 4);
        if (bx < -POFV_BOSSATK_HALF_W || bx > POFV_BOSSATK_HALF_W) continue;  /* ② 世界 x ∈ ±192 */
        if (by > POFV_BOSSATK_ANCHOR_Y + 8.0f) continue;            /* ② 不能低于锚点 */
        if (by < POFV_BOSSATK_ANCHOR_Y - POFV_BOSSATK_DMG_H) continue;  /* ② 往上一个高 */
        vx = *(const float *)(b + CE_BULLET_VELOCITY);
        vy = *(const float *)(b + CE_BULLET_VELOCITY + 4);
        if (vy >= 0.0f) continue;                                   /* ③ 只认往上飞的 */
        if ((bx - POFV_BOSSATK_ANCHOR_X) * vx + (by - POFV_BOSSATK_ANCHOR_Y) * vy < -8.0f)
            continue;                                               /* ④ 从锚点向外发散 */

        /* ── 到这里才确认「这颗是我们的弹」⇒ 施加两个效果 ─────────────────────────────── */
        /* ★★ 置「跳过碰撞判定」⇒ 这颗弹**打不到自机**（见 `POFV_BOSSATK_NO_HIT` 的长注释）。*/
        *(uint32_t *)(b + CE_BULLET_FLAGS) |= POFV_BOSSATK_NO_HIT;
        pos[0] = bx; pos[1] = by; pos[2] = 0.0f;
        ce_damage_rect(pos, 0.0f, POFV_BOSSATK_DMG_LIFE, POFV_BOSSATK_DMG,
                       POFV_BOSSATK_DMG_CELL, POFV_BOSSATK_DMG_CELL);        if (++made >= POFV_BOSSATK_DMG_MAX) break;                  /* 每帧上限（别把源池刷爆）*/
    }
    s_bossatk_scan_rot = (s_bossatk_scan_rot + POFV_BOSSATK_DMG_MAX) % CE_BM_BULLET_COUNT;
}

/* ★★ 2026-10-01（owner：「最好是**除了透明度之外完全复刻符卡**，**包括对于敌机的「自机狙」**等等」）：
 *   **自机狙** —— 把「版底锚点 → **最近的可锁定敌人**」的方位角写进 `PLAYER + CE_PLAYER_AIM_ANGLE`，
 *   给那几行 `func_on_init = 5` 的 shooter 在建弹时取用（出膛定向，**不是追踪**）。
 *
 * 一手（逐条）：
 *   · `engine/sht/th18/02-shooter-record.md` §5：`CardAlice__on_shoot` `0x40b4e0` 的完整链路 ——
 *     最近敌人搜索 `0x438cb0`（半径 `0x4b93b0` = **512.0**）→ `0x4a94a0` `atan2` → `0x40b5fa`
 *     `fstp [player+0x479cc]` → 再 `call 0x40a9c0` 发弹；行侧 `func_on_init = 5`（`0x4612d0`）
 *     把那个角覆写到 `bullet+0x64`。
 *   · 角度：`ce_atan2()`（`sdk.h`，包的是同一个 `0x4a94a0`）。
 * ⚠ 「锁不锁」用引擎自己的过滤 `ce_enemy_lockable()`（= 非空且不带 NO_LOCK）——与 108/109 别处同款。
 * ⚠ 没有可锁定敌人时**什么都不写**（保留上一次的角度；`func_on_init=5` 那几行照旧出膛，
 *   方向 = 上一次的瞄准角 ⇒ 观感上是「朝上次目标」而不是「朝天上乱飞」）。*/
static void bossatk_aim(void)
{
    uint8_t *em = CE_ENEMY_MGR();
    uint8_t *e, *best = 0;
    const float ax = POFV_BOSSATK_ANCHOR_X, ay = POFV_BOSSATK_ANCHOR_Y;
    float best2 = 512.0f * 512.0f;              /* ★ 一手：CardAlice 的搜索半径（`0x4b93b0` = 512.0）*/

    CE_ENEMY_FOREACH(em, e) {
        const float *p;
        float dx, dy, d2;
        if (!ce_enemy_lockable(e)) continue;
        /* ★★ 2026-10-01（真 bug 修正）：**别把自机狙瞄到我们自己的伪 boss 载体上**。
         *   载体是**我们**在版底刷的敌人（`s_boss_carrier`）—— `CE_ENEMY_FLAG_NO_DAMAGE` 只保证
         *   「打不掉」，**不等于「不可锁定」**（`CE_ENEMY_FLAG_NO_LOCK` 是一组语义位构成的掩码
         *   `0x0c000021`，没有单独一位可安全置）⇒ 这里按 **enemy_id 精确排除**。*/
        if (s_boss_carrier && *(const int32_t *)(e + CE_ENEMY_ID) == s_boss_carrier) continue;
        p = ce_enemy_pos(e);
        if (!p) continue;
        dx = p[0] - ax; dy = p[1] - ay;
        d2 = dx * dx + dy * dy;
        if (d2 < best2) { best2 = d2; best = e; }
    }
    if (best) {
        const float *p = ce_enemy_pos(best);
        uint8_t *pl = CE_PLAYER();
        if (pl) pofv_atan2(p[1] - ay, p[0] - ax, (float *)(pl + CE_PLAYER_AIM_ANGLE));
    }
}

static void bossatk_fire(ce_card_t *c, pofv_card_t *s)
{
    int set;
    uint8_t *o;

    if (!pofv_bossatk_tick(&s->bossatk)) {
        bossatk_carrier_stop();

    s_ghost_wave_seen = 0;      /* ★ 换关/重来：重新等第一波怪 */

    /* ★ hits UI 的字形 VM（长持续效果纪律）。*/
    hits_ui_kill();

    /* ★ 场的 VM 也要收干净（长持续效果纪律：删卡/换关/重开那一刻 tick 全停，只有这里跑得到）。*/
    {
        int vi;
        for (vi = 0; vi < POFV_STORM_MAX; ++vi) {
            if (s_storm_vm[vi]) { ce_anm_delete((uint32_t)s_storm_vm[vi]); s_storm_vm[vi] = 0; }
        }
    }                         /* ★ 打完了 ⇒ 喊载体走（别留孤儿敌人）*/
        return;
    }
    /* ★★ 2026-10-01（owner 定的**混合路线**）：**C1/C2 = sht 自机弹**（原生伤敌机 + 自机狙打敌机）；
     *   **C3/C4 = ECL 侧**（`assets/ecl/pofv109_boss.ecl.txt` 的伪 boss 载体）⇒ 这两档**不发 sht 行**
     *   （它们原来那两组已经在 `append_shooterset.py` 里整组撤掉，槽号也不再引用）。
     *   ⚠ 自伤源与载体都在下面 `level >= 3` 那一支里（本函数后半）。*/
    if (s->bossatk.level >= 3) {
        bossatk_carrier_sync(s->bossatk.level);      /* ★ C3/C4：ECL 侧载体（+ 下面的自伤源）*/
        bossatk_damage();
        return;                                      /* ★ 不再走 sht 那条路 */
    }
    set = (s->bossatk.level == 1) ? CE_SHT_SET_POFV109_BOSSATK_C1
                                  : CE_SHT_SET_POFV109_BOSSATK_C2;
    o = ce_option_get(c, &s->opt, 0, POFV_BOSSATK_OPT_ANM, 30, "pofv109");
    if (!o) return;                                     /* 拿不到发射口：这一帧不发 */
    /* ★★ owner 的最终模型：**不是从自机身上发，而是从「版底的 boss 位」往上发**。
     *   直接把出膛坐标（`CE_OPT_POS_X/Y`，定点 ×128）覆盖成那个固定锚点 ——
     *   引擎本帧早些时候已按「跟随玩家」写过一次，我们在它之后写 ⇒ 当帧的发射点就是这里
     *   （下一帧引擎再写回玩家身边，我们再覆盖，如此往复）。*/
    *(int32_t *)(o + CE_OPT_POS_X) = (int32_t)(POFV_BOSSATK_ANCHOR_X * 128.0f);
    *(int32_t *)(o + CE_OPT_POS_Y) = (int32_t)(POFV_BOSSATK_ANCHOR_Y * 128.0f);
    bossatk_aim();                                      /* ★ 自机狙：先定方位角，再发弹 */
    ce_tick_shooters_for_card(o, (int)s->bossatk.left, 0, set);
}

/* 灵力道具的**跟随 VM 同步**（每颗一台；`pofv_power_t` 是纯逻辑、不认识 VM ⇒ 在这里按槽对齐）：
 * 活着的有 VM 就挪位置、没 VM 就起一台；槽空了就删 VM。每帧 O(48)，很便宜。*/
static void power_vm_sync(void)
{
    int i;

    for (i = 0; i < POFV_POWER_MAX; ++i) {
        const pofv_power_t *it = &s_power.s[i];
        /* ★ 一手（2026-10-01）：**「待发射」的错帧里不建 VM** —— TH16 的季节道具就是这么干的
         *   （延时里 `item+0xc50 = 6`，延时结束才 `mov [item+0x530], 0xffffffff` 去建 VM；
         *   116 `season.c` 562–565 行同款）。⇒ 本卡也让 VM 从 `delay` 归零那一帧才出现，
         *   出场观感（`scaleTime` 0.5 → 1.0）正好落在「开始飞散」的瞬间。*/
        if (it->active && it->delay == 0) {
            if (!s_power_vm[i])
                s_power_vm[i] = (int32_t)ce_anm_spawn(CE_ABILITY_ANM(),
                                                      CE_ANM_ABILITY_SCRIPT_POFV109_POWER,
                                                      POFV_POWER_VM_LAYER);
            if (s_power_vm[i]) {
                ce_anm_set_pos((uint32_t)s_power_vm[i], it->x, it->y, 0.0f);
                /* ★ 2026-10-01：**不再每帧写 scale** —— 脚本 374 现在自己 owning 缩放
                 *   （照 TH16 `script129`：16 帧 2.0→1.0、之后 300 帧 1.0→0.7）；
                 *   卡侧再写会与 `scaleTime` 打架（`sdk.h` 的 `ce_anm_set_scale` 注释）。*/
                (void)0;
            }
        } else if (s_power_vm[i]) {
            ce_anm_delete((uint32_t)s_power_vm[i]);
            s_power_vm[i] = 0;
        }
    }
}

/* ★★ 2026-10-03b（92 伟大之作的**清扫闸**；统筹接 `HANDOFF-86-92-AND-SWEEP.md` §3 的 T2）：
 *   本卡这一批 = **灵力道具**（`s_power`，自建实体、不在引擎道具池里）⇒ 只能本卡自己清。
 *   做法：先按颗在原位撒金钱（池满走 `ce_money_pocket()` 的同一条记账），**再把池整批标空**——
 *   紧接着的 `power_vm_sync()` 会看到「槽已空、VM 还在」那一支，把每颗的跟随 VM 删掉
 *   （`pofv.c` 的 `else if (s_power_vm[i])` 分支）⇒ **消失仍然走本卡自己的收尾**，不留幽灵 VM。
 *   ⚠ 不走 `pofv_power_clear()`：那支会 `pofv_power_pool_init()` 连**诊断计数**一起归零（日志口径会乱），
 *     而且它只清池、不放钱。这里只把 `active` 标空 + `live` 归零，诊断字段全留。
 *   ❓ 面值自定（1 颗灵力道具 = `POFV_SWEEP_MONEY_PER_ITEM` 枚金钱）—— 见 `TUNING.md` 的 109 段。*/
#define POFV_SWEEP_MONEY_PER_ITEM 1
static uint32_t s_sweep_seen;

static void pofv_sweep_to_money(void)
{
    int i, n = 0, k;
    for (i = 0; i < POFV_POWER_MAX; ++i) {
        const pofv_power_t *it = &s_power.s[i];
        float p[3];
        if (!it->active) continue;
        p[0] = it->x;
        p[1] = it->y;
        p[2] = 0.0f;
        for (k = 0; k < POFV_SWEEP_MONEY_PER_ITEM; ++k) {
            if (!ce_spawn_item(2u, p, -1.5707964f, 1.7f)) ce_money_pocket(1);
        }
        ++n;
    }
    if (n) {
        for (i = 0; i < POFV_POWER_MAX; ++i) s_power.s[i].active = 0;
        s_power.live = 0;                  /* ⚠ 只动这两个；诊断计数（spawned/absorbed/expired）原样留 */
        ce_log("pofv: 92 清扫 —— %d 颗灵力道具换成金钱（每颗 %d 枚）", n, POFV_SWEEP_MONEY_PER_ITEM);
    }
}

/* 扫一遍弹池：把「在结界内且本颗还没算过」的弹标成已算，最多把 `cap` 个位置抄进 ox/oy。
 * 返回本帧**新命中**的颗数（可能 > cap ⇒ 多出来的面值由打包摊到那 cap 颗上）。 */
static int aura_scan_bullets(const float *center, float r, int pad_linear, float *ox, float *oy, int cap)
{
    uint8_t *bm = CE_BULLET_MGR();
    uint32_t i;
    int n = 0;

    if (!bm || !center || r <= 0.0f) return 0;
    for (i = 0; i < CE_BM_BULLET_COUNT; ++i) {
        uint8_t *b = bm + CE_BM_BULLETS + (size_t)i * CE_BULLET_STRIDE;
        uint16_t state = *(const uint16_t *)(b + CE_BULLET_STATE);
        if (is_our_barrage(b)) continue;      /* ★ 我们自己发的 C3/C4 弹幕：三种清弹都不吃 */
        float br, bx, by, dx, dy;
        if (state != 1 && state != 2) { s_aura_done[i] = 0; continue; }  /* 空槽 / 别的状态：清闩锁 */
        if (s_aura_done[i]) continue;                                    /* ★ 这颗弹已经算过 */
        br = *(const float *)(b + POFV_BULLET_RADIUS);
        if (!(br > 0.0f)) continue;                                      /* 半径 0 的哨兵弹种：不算 */
        bx = *(const float *)(b + CE_BULLET_POS);
        by = *(const float *)(b + CE_BULLET_POS + 4);
        dx = bx - center[0];
        dy = by - center[1];
        /* pad = 弹半径（一手 TH19）；几何支按角色档案（Reimu 支 / Sanae 支）。*/
        if (!pofv_aura_hit_ex(dx, dy, r, br, pad_linear)) continue;
        s_aura_done[i] = 1;
        if (n < cap) { ox[n] = bx; oy[n] = by; }
        ++n;
    }
    return n;
}

/* ★★ 调试快捷键（owner 2026-10-01）：「**109 空格 = 发 C4**」。
 *
 *   **空格 = 本仓既定的调试键**：`sdk.h` 的 `CE_VK_SPACE`（读 `GetKeyboardState`），
 *   owner 2026-09-21 口径 —— 111 灵界调试 / 112 令牌调试 / 106 满 P 测试键都在用它，
 *   并且明文写着「**玩法功能禁止占用空格**」⇒ 本条是**调试**，不是玩法绑定。
 *
 *   ⚠ 为什么不能走输入掩码：**空格根本没进 TH18 的掩码** —— 引擎的键盘扫描
 *     （`th18:0x401c50` 起）只读 `Z/X/Shift/方向/Esc/C/D/Home/P/Enter/End/R`，
 *     连 `GetKeyboardState` 缓冲区的偏移里都没有 `0x20`
 *     （`engine/input/th18/01-key-mapping.md` §2.1）⇒ 只能卡侧自己去问 Windows，`ce_key_down()` 就是它。
 *   ⚠ 也因此它**不进 replay 输入流**（回放只录 `th18:0x4ca428` 的低 16 位）⇒ 回放里不重现，
 *     也**不需要**去占掩码空闲位。
 *   ⚠ 正式发布口径下要撤（登记 `TUNING.md` §2.0v-6）。
 *
 *   按下沿触发一次：`ca_effect(C4)` = 一手 C4 那一整套（**扩张的消弹圈** L3 + 抵挡护盾 +
 *   boss attack 弹幕 + 自己是 boss）。⚠ **调试用不计气槽代价**（方便反复试）。*/
static int32_t s_dbg_space = 0;
static void ca_effect(pofv_card_t *s, int level, int grant_invuln);   /* 定义见下（本函数要用）*/

static void debug_hotkeys(pofv_card_t *s)
{
    int down = ce_key_down(CE_VK_SPACE) ? 1 : 0;

    if (down && !s_dbg_space) {
        ce_log("pofv: DEBUG hotkey SPACE -> fire C4 (CA L3 expanding circle + shield + boss attack; free)");
        ca_effect(s, POFV_CA_LEVEL_MAX, /*grant_invuln=*/1);
    }
    s_dbg_space = down;
}

/* ============================ 效果 ============================ */

/* 无敌：走 EVENTS-CONTRACT §4 的**声明层**（各卡声明、层取最大值、只在更长时写 `player+0x47774`），
 * 不直接写计时器 —— 否则会把 107 结界 / 117 霊撃 给的长无敌截短。
 * 窗口内**每帧声明剩余帧数**（层取 max ⇒ 既不会截短别人，也不会被别人截短）。
 * ★ 2026-09-12 审计修（任务 4）：**帧数改由 `pofv_core` 的状态机算**（`pofv_invuln_*`）。
 *   旧实现在 `on_tick_2` 里写 `if (invuln_left > 0) { request(left); --left; }`，而起窗帧
 *   （下面的 `ca_invuln_grant()`）**也**声明了一次 `frames` ⇒ 起窗帧之外又多声明了 N 帧
 *   ⇒ C1 实际护住 F..F+8 = **9 帧**，比一手（spec §5.1「C1 无敌 = 8 帧」）多 1 帧。
 *   现在的契约：**声明量 > 0 的帧数 = 档位值**（起窗帧 = 第 1 帧，之后 N−1 … 1，第 N+1 帧退声明）。*/
static void ca_invuln_grant(pofv_card_t *s, int level)
{
    int32_t frames;

    if (!s) return;
    frames = pofv_invuln_grant(&s->invuln, level);   /* 起窗帧 = 第 1 帧 */
    if (frames <= 0) return;                         /* 等级非法 ⇒ 不开窗 */
    ce_invuln_request(ce_card_id(s->card), frames);
    ce_invuln_apply();
}

/* 蓝色爆风（一手：**击破妖精 / 幽灵时产生**、**有攻击力**、消掉圈内**白弹**、按连爆质量回气）。
 * ⚠ 原版「被消的白弹以同样速度投到对手场地」在单机没有对应 ⇒ 用户口径改成**转成分**：
 *   逐弹 `Bullet__cancel(b, mode = 1)`（引擎 `0x4290e0`：加分 + 掉点道具），不另造道具。
 *
 * ★★ 2026-09-22（P5 #4）**重做成「场」**：一个暴风不是一口气消完，而是**每帧**按当前半径判一次
 *   （`pofv_storm_radius`：一手 TH09 节点律 `r = r0 + dr × age`，`dr` = **1.6 px/帧**），
 *   活 `pofv_storm_life_for_chain(档)` 帧 ⇒ 妖精是**飞进来**才被点着，且**连锁前沿 = 1.6 px/帧
 *   ≈ 妖精速度**（owner 2026-10-01：「连锁爆破在敌人链条上的移动速度和敌人链条本身移动的速度相当」）。
 *   ⇒ 连锁由妖精的到场率决定，不再是一个 30 帧窗口的节拍器（用户：「连锁不该这么快」）。
 *   每帧的成本闸 = `POFV_STORM_SCANS_PER_FRAME`（整池扫弹的次数；伤害源那一条很便宜，不做闸）。 */
/* ★★ 2026-10-01（owner：「**暴风缺少UI**」）：场池的**视觉** —— 每个活着的场一台跟随 VM。
 *
 *   此前暴风**完全没有画面**（只有伤害 + 消弹）⇒ 玩家看到「无缘无故掉血/敌人死」。
 *   本函数每帧：活着的场 ⇒ 补 VM（没有就起一台）+ 写坐标 + **写 scale = 当前半径** + 按 kind 染色；
 *   槽空了 ⇒ 删 VM。半径 = `r0 + dr × age`（与判定/消弹用的是**同一个** r ⇒ **判定 = 绘制** ✓，
 *   照结界那套一手口径）。
 *   ❓ 颜色自定见脚本 402 的注释（暴风 = 蓝、CA 红圈 = 红）。*/
static void storm_vm_sync(void)
{
    int i;
    for (i = 0; i < POFV_STORM_MAX; ++i) {
        const pofv_storm_t *bl = pofv_storm_slot(&s_storm, i);
        if (!bl || !bl->active) {
            if (s_storm_vm[i]) { ce_anm_delete((uint32_t)s_storm_vm[i]); s_storm_vm[i] = 0; }
            continue;
        }
        if (!s_storm_vm[i])
            s_storm_vm[i] = (int32_t)ce_anm_spawn(CE_ABILITY_ANM(),
                                                  CE_ANM_ABILITY_SCRIPT_POFV109_FIELD,
                                                  POFV_STORM_VM_LAYER);
        if (!s_storm_vm[i]) continue;                      /* anm 还没就绪：下一帧再补 */
        {
            float r = bl->r0 + bl->dr * (float)bl->age;
            ce_anm_set_pos((uint32_t)s_storm_vm[i], bl->x, bl->y, 0.0f);
            ce_anm_set_scale((uint32_t)s_storm_vm[i], r, r);
            ce_anm_set_color((uint32_t)s_storm_vm[i],
                             bl->kind == POFV_STORM_KIND_CA ? POFV_FIELD_RGBA_CA
                                                            : POFV_FIELD_RGBA_STORM);
        }
    }
}

/* ★★★ 2026-10-01（owner 实跑：「**暴风会伤害到屏幕外**」「大部分都是**无缘无故一进屏幕就死了**」）：
 *   场的伤害矩形原来是 `r*2 × r*2`（以场心为中心）⇒ 场心靠边/靠上时矩形**溢出场地**，
 *   把**还没进屏**的敌人（`y < 0` 或 `|x| > 192`）也一起打死 ✗ —— 这正是「一进屏幕就死」。
 *
 *   ⇒ **先把矩形裁到场地内**：求「场心 ± r」与场地矩形（世界 x ∈ ±192、y ∈ [0,448]）的**交集**；
 *     空了就**不放伤害源**；否则以交集为矩形。这样屏外的敌人**永远**吃不到这个场 ✓。
 *
 *   ⚠ 拆成独立函数是**故意的**：这段有 4 个额外浮点局部，塞回 `do_storm_frame()` 会让那个函数
 *     的寄存器压力顶到 `ce_cancel_radius()`（`sdk.h`）那串内联汇编的上限 ⇒ 编译报
 *     `inline assembly requires more registers than available`（我本轮就撞了一次）。*/
static void storm_damage_clipped(const pofv_storm_t *bl, float r)
{
    float x0 = bl->x - r, x1 = bl->x + r;
    float y0 = bl->y - r, y1 = bl->y + r;
    float pos[3];
    if (x0 < -POFV_FIELD_HALF_W) x0 = -POFV_FIELD_HALF_W;
    if (x1 >  POFV_FIELD_HALF_W) x1 =  POFV_FIELD_HALF_W;
    if (y0 < 0.0f) y0 = 0.0f;
    if (y1 > POFV_FIELD_H) y1 = POFV_FIELD_H;
    if (x1 <= x0 || y1 <= y0) return;                 /* 交集空（场整个在屏外）⇒ 不打 */
    pos[0] = (x0 + x1) * 0.5f;
    pos[1] = (y0 + y1) * 0.5f;
    pos[2] = 0.0f;
    ce_raise_damage_cap(POFV_DMG_CAP);
    ce_damage_rect(pos, 0.0f, POFV_STORM_DMG_LIFE, POFV_BLAST_DMG, x1 - x0, y1 - y0);
}

static void do_storm_frame(pofv_card_t *s, const pofv_storm_t *bl, int *scan_budget)
{
    float pos[3], r;
    int n = 0;

    if (!s || !bl || !bl->active) return;
    pofv_storm_radius(bl, &r);          /* ★ 每槽自带 r0/dr ⇒ **逐帧扩张** */
    if (r <= 0.0f) return;
    pos[0] = bl->x; pos[1] = bl->y; pos[2] = 0.0f;

    /* ★★★ 2026-10-01（owner：「**消弹圈是逐帧扩张的场**」）：蓄力攻击的红圈走**同一个场池**，
     *   但它是**另一回事** —— 一手：红圈消掉圈内**所有**弹幕（含不可消）、**不回复气**、不连爆、
     *   不掉灵力道具、也不给敌人伤害。⇒ 每一帧按**当前半径**（= 这一帧长到多大）扫一遍整池。*/
    if (bl->kind == POFV_STORM_KIND_CA) {
        if (scan_budget && *scan_budget > 0) {
            --*scan_budget;
            n = pofv_cancel_circle(pos, r, /*white_only=*/0, POFV_CANCEL_MAX,
                                   POFV_BULLET_CANCEL_MODE_CA);
            pofv_account_ca(&s->st, n);                 /* 一手：红圈不回复气（只填 SP）*/
        }
        if (n > 0)
            ce_log("pofv: CA field L%d age %d r %d cancelled %d (all bullets, sp %d)",
                   (int)bl->life, (int)bl->age, (int)r, n, (int)s->st.sp);
        return;
    }

    /* 一手：蓝圈只消**白弹**（半径 ≤ 4.0）。整池扫弹是 O(2000)，按帧预算出闸。 */
    if (scan_budget && *scan_budget > 0) {
        int32_t vpts = 0;
        --*scan_budget;
        n = pofv_cancel_circle(pos, r, /*white_only=*/1, POFV_CANCEL_MAX, POFV_BULLET_CANCEL_MODE_BLAST);
        pofv_account_blast(&s->st, n, bl->chain);
        /* ★ 2026-10-01（口径 5）：消弹的充能**改经灵力道具** ——
         *   一颗白弹的面值 = `POFV_POWER_PTS_CANCEL × 连爆档`（`pofv_power_value_cancel(1, chain)`），
         *   按**打包**撒成 ≤ `POFV_POWER_SPAWN_PER_FRAME` 颗实体（**总面值守恒**；位置 = 场心）。
         *   ⚠ 位置取场心（❓ 自定）：一手 TH19 的灵力道具从**敌人/霊死亡处**掉，
         *     而「消弹也掉」本来就是 owner 口径 5 的加项；场心 = 这一波弹被消掉的地方。*/
        pofv_power_spawn_packed(&s_power, bl->x, bl->y, n,
                                pofv_power_value_cancel(1, bl->chain),
                                POFV_POWER_SPAWN_PER_FRAME, POFV_POWER_SRC_CANCEL, &vpts);
    }

    /* 一手：蓝圈**有攻击力** ⇒ 覆盖圈面的矩形伤害源（照 Remilia / season.c）。
     * ★ 场每帧都放一个：这就是「妖精飞进来才被点着」的那只手（旧实现只放 2 帧）。
     * ★★ 2026-10-01（owner 第四条「th09 暴风可以直接打死 th09 幽灵」/ 第五条「th19 没有暴风连锁，
     *   只是暴风消弹」）：**伤害源只在「能打死幽灵」的那一档放** —— 早苗（TH19）档**只消弹**。*/
    if (s_storm_prof.kills_ghosts) storm_damage_clipped(bl, r);

    /* ★★ 2026-10-01（口径 10）：**旧的自建活化灵池已整条撤掉** —— 灵现在是**真敌人**
     *   （`ce_spawn_enemy` + `CESpirit109`），与普通敌机一样吃上面这个 `ce_damage_rect`
     *   与自机弹 ⇒ 「被爆风打死的灵也会起连锁场 + 掉灵力道具」由 `on_enemy_death`
     *   那条**统一路径**处理（不必也不能在这里自己扫一个卡侧池）。*/
    if (n > 0)
        ce_log("pofv: blast chain %d r %d cancelled %d white (gauge %d, sp %d, power %d)",
               (int)bl->chain, (int)r, n, (int)s->st.gauge, (int)s->st.sp, (int)s_power.live);
}

/* 红色消弹圈（一手：**使用蓄力攻击时产生**、消掉圈内**所有弹幕**（含不可消）、
 * 被消的白弹**直接消失**且**不回复气**）。*/
static void ca_effect(pofv_card_t *s, int level, int grant_invuln)
{
    const float *pp = ce_player_pos();
    float pos[3], r;
    int n;

    if (!s || !pp) return;
    /* ★★★ 2026-10-01（owner：「**消弹圈是逐帧扩张的场**」）：**不再**一次性按终点半径消一圈，
     *   而是起**两个扩张的场**（与暴风**共用同一个场池** —— owner：「aura 和锚定场本质上是
     *   同一个东西」的延续）：
     *     · **「松手那一下」**（一手 `0x41e875`：16 / 20 / 8 → 176）—— **任何一次松手都起**；
     *     · **该档的圈**（一手 L1/L2/L3：0/4.0/48→192、0/4.0/64→256、0/3.5/128→448）。
     *   两个场都在 `do_storm_frame()` 里**逐帧按当前半径**扫弹。*/
    pos[0] = pp[0]; pos[1] = pp[1]; pos[2] = pp[2];
    pofv_ca_field_start(&s_storm, pos[0], pos[1], 0);          /* 「松手那一下」*/
    n = pofv_ca_field_start(&s_storm, pos[0], pos[1], level);  /* 该档的圈 */
    (void)n;
    pofv_ca_circle_radius(level, &r);              /* 只用于日志（该档的**终点**半径）*/
    n = 0;                                         /* 本帧的消弹数由场那边每帧自己记 */
    if (grant_invuln) ca_invuln_grant(s, level);   /* 一手：CA 给无敌（C1 = 8 帧）*/
    /* ★★ 2026-10-01（owner 甜头口径）：**C3 / C4 的蓄力攻击发动 ⇒ 把抵挡护盾（重新）架上**
     *   —— 一手 TH19 `FUN_0040b420` 的唯二调用点就是阶段 3（EXスペシャル）/ 阶段 4（ボスアタック）。
     *   ⚠ `grant_invuln` 只是「这一次调用要不要给 CA 无敌」的区别（boss attack 占位那条不给），
     *     与架盾无关 ⇒ 两条路都架。*/
    if (pofv_shield_arm(&s->st, level)) {
        s->shield_logged = 0;
        ce_log("pofv: shield ARMED by C%d charge attack (parts 1; TH19 auto-barrier)", level);
    }
    /* ★★ 2026-10-01（owner）：**C3 / C4 的蓄力攻击同时起 boss attack** ——
     *   「自己会额外发射如对战作送到对面的攻击……往"上"打……会对敌人造成伤害」；
     *   「C3 也有……只不过自己不是 boss」；一手 TH09 `0x41f999` 支。*/
    if (pofv_bossatk_start(&s->bossatk, level)) {
        s->bossatk_logged = 0;
        ce_log("pofv: BOSS ATTACK started by C%d charge attack (%s; %d frames)",
               level, pofv_bossatk_is_boss(&s->bossatk) ? "I am the boss" : "extra attack only",
               (int)s->bossatk.left);
    }
    ce_log("pofv: CA L%d r=%d cancelled %d bullets (sp %d, gauge %d)",
           level, (int)r, n, (int)s->st.sp, (int)s->st.gauge);
}

/* SP 到档（一手：10 万 / 30 万 / 50 万）→ 原版发动 **boss attack**（把 CCA 压到**对手场地**）。
 * ⚠ 单机没有对手 ⇒ **占位**：无代价发动一次最高级红圈（全屏消弹 + 转分），**不给无敌**。
 * 数值与形态待用户拍板（spec §未定项第 2 条：「CCA / Boss 攻击 → 打击 boss / 全屏攻击 / 场地滞留弹幕」）。*/
static void do_boss_attack(pofv_card_t *s)
{
    if (!s) return;
    ce_log("pofv: SP %d crossed stage %d -> boss attack (placeholder: free C%d circle)",
           (int)s->st.sp, (int)s->st.sp_stage, POFV_CA_LEVEL_MAX);
    ca_effect(s, POFV_CA_LEVEL_MAX, /*grant_invuln=*/0);
}

/* ============================ 灵（自制真敌人；口径 8/9/10） ============================
 * 本体 = **TH12 UFO 型真敌人**：`ce_spawn_enemy("CESpirit109", &spawn)`（ECL 见
 *   `assets/ecl/spirit109.ecl.txt`，已由 `assets/ecl/make_ufo_ecl.py` 并进 `default.ecl`）。
 * 卡侧与 ECL 之间只有**两个现成字段**（都不是新站点）：
 *   · `%F2/%F3`（`CE_ENEMY_ECL_F2/F3` = `enemy+0x14d0/+0x14d4`）= 本帧目标坐标
 *     —— 先例 = 110 的绿妖精（`cards/faith.c` 的 `gf_follow_tick()`，同一通道）；
 *   · `$I0`（`CE_ENEMY_ECL_I0` = `enemy+0x14b8`）= 状态：**0 正常 / 1 沈静化 / 999 卡侧喊走**。
 * ⚠ 卡侧**不缓存敌人指针**（`sdk.h` 的纪律）：每帧用 `ce_enemy_by_id()` 按 enemy_id 找。
 * 三道门都在纯逻辑里（`pofv_ghost_*`，有主机单测）：同屏上限 **4**（★ 一手 TH09）、
 *   周期 **600 帧**（❓ owner 2026-10-01 拍）、**任何 boss 在场就不撒**（口径 9 方案甲）。 */

#define POFV_GHOST_ECL_NAME     "CESpirit109"   /* 必须与 `assets/ecl/spirit109.ecl.txt` 的子程序名一致 */
#define POFV_GHOST_HOMING_SPEED   0.8f  /* ❓ 自定（索敌：每帧朝自机飘多少 px）*/
/* ★★ 2026-10-01（owner：「**开头依旧有一只灵直接出现，很奇怪，至少等到第一波怪出现再从场外出灵吧**」）：
 *   ① **等第一波怪**：`s_ghost_wave_seen` 闩锁 —— 直到「场上真出现过敌人」才允许第一次撒（见 tick）；
 *   ② **从场外出灵**：出生点改成**场地外**（世界 x ∈ ±192、y ∈ [0,448] 之外），
 *      由灵自己的索敌（ECL 里朝 `%F2/%F3` 飘）把它带进场 ✓ —— 不再在自机旁边凭空冒出来 ✗。*/
/* ★★★ 2026-10-01 订正（owner 引 TH09 wiki 原文）：
 *   「**幽灵：从上方落下**、可攻击的**鸟状**飞行物体，有**绿黄红三种**，由击破妖精来生成并送去对方场地」。
 *   ⇒ 「从场外出灵」= **从上方落下来** ✓ —— 我上一版写成从**左边** `(−220,−40)` 进来 ✗（难怪奇怪）。
 *   ⇒ 出生点改成**顶边之上、横向在场地内**；灵自己的索敌（ECL 朝 `%F2/%F3` = 自机）把它**带下来**，
 *     正好就是「从上方落下 + 会朝你来」的观感 ✓。
 *   （另：owner 说「单人没有对方场地可送 ⇒ 两种灵都只能**自动生成**」⇒ 卡里那套 600 帧循环撒 ✓ 不动。）*/
/* ★★ 2026-10-01 owner：「**在场外消失时引擎特性没办法**」＋「**把它们在版顶内部生成的动画做出来吧**」
 *   ⇒ 不再放到场外（`y = −40` ✗ 会被引擎当越界**瞬间删掉** ⇒ 五只灵一只都看不见 ✓ 根因就是它）。
 *   ⇒ 改成**版顶内部**（世界 y = 16，场地 y∈[0,448] 之内 ✓、又在自机可动范围 [32,432] 之上 ✓），
 *     并用**生成动画**（脚本 372 的淡入 `alphaTime`）补上「从上方出现」的观感 ✓。*/
#define POFV_GHOST_SPAWN_Y      16.0f
#define POFV_GHOST_OUT_DX      48.0f   /* ❓ 自定：横向散布（每只错开一点，别叠成一条竖线）*/
/* ★★ 2026-10-01 二改：脚本 372 现在画的是 **TH19 原版動物霊贴图**（`animal_spirits.png` 的
 *   `sprite344–347`，各 **48×32**）—— 第一次换成 TH09 的图被 owner 打回（「怎么上来有个莉莉白
 *   在靠近，灵不长这样」：那是大妖精样、不是灵）。
 *   ⇒ 缩放取 **0.6**：48×32 × 0.6 ≈ **29×19 px**，与灵的判定 **20×20**
 *     （`spirit109.ecl.txt` 的 `setHurtbox(20,20)`）大致对得上。
 *   ⚠ **换贴图必须同步改这里**（旧值 0.4 是配 64×64 那版的）。*/
/* ★★ 2026-10-01（owner：「**灵贴图的大小应当是大一倍**」）：0.6 → **1.2**（正好一倍）。*/
#define POFV_GHOST_VM_SCALE      1.2f

/* boss 判据（口径 9 方案甲）：`mgr+0x48` 的 `int[4]` boss enemy_id（一手 `get_boss_enemy_full`
 * `0x4237f0` 就是按它走链表找），逐个问「还在不在」。任何 boss（含道中）在场 ⇒ 不撒灵。 */
static int boss_enemy_present(void)
{
    uint8_t *em = CE_ENEMY_MGR();
    int i;

    if (!em) return 0;
    for (i = 0; i < CE_EM_BOSS_SLOTS; ++i) {
        int32_t id = *(const int32_t *)(em + CE_EM_BOSS_IDS + (size_t)i * 4);
        if (id > 0 && ce_enemy_by_id(em, id)) return 1;
    }
    return 0;
}

/* 每只灵的**跟随 VM**（本体在 ECL 侧 `anmSetSprite(0, -1)` 拆槽 ⇒ 不画 VM 就是隐形的）。*/
static void ghost_vm_kill(int slot)
{
    if (slot < 0 || slot >= POFV_GHOST_MAX) return;
    if (s_ghost_vm[slot]) {
        ce_anm_delete((uint32_t)s_ghost_vm[slot]);
        s_ghost_vm[slot] = 0;
    }
}

static void ghost_vm_spawn(pofv_card_t *s, int slot, float x, float y)
{
    uint32_t id;

    if (slot < 0 || slot >= POFV_GHOST_MAX) return;
    ghost_vm_kill(slot);
    id = ce_anm_spawn(CE_ABILITY_ANM(), CE_ANM_ABILITY_SCRIPT_SPIRIT109, POFV_GHOST_VM_LAYER);
    if (!id) {
        if (s && !s->ghost_logged) {
            ce_log("pofv: ghost follow-VM spawn FAILED (anm not ready?) — ghost stays invisible");
            s->ghost_logged = 1;
        }
        return;
    }
    s_ghost_vm[slot] = (int32_t)id;
    ce_anm_set_pos(id, x, y, 0.0f);
    ce_anm_set_scale(id, POFV_GHOST_VM_SCALE, POFV_GHOST_VM_SCALE);
}

/* 撒一只灵（真敌人）。返回槽号；−1 = 没撒成（spawner 拒绝 / 池满 ⇒ 已把那只见不得光的喊走）。 */
static int ghost_spawn(pofv_card_t *s, int32_t kind)
{
    uint8_t spawn[CE_SPAWN_SIZE];
    const float *pp = ce_player_pos();
    float *sp;
    uint8_t *e;
    int32_t id;
    int slot;

    memset(spawn, 0, sizeof spawn);
    sp = (float *)(spawn + CE_SPAWN_POS);
    /* ★ 2026-10-01 订正（一手 TH09 wiki：「**幽灵：从上方落下**…」）：
     *   出生点 = **顶边之上**（世界 y = −40）、横向在场地内**错开散布** ⇒ 灵从**上方**落进场 ✓。
     *   （上一版从左边 `(−220,−40)` 进来 ✗ 已撤；「场外」在这里就是「版面上方」✓。）*/
    sp[0] = (float)((int)(s_ghost.spawned % 7) - 3) * POFV_GHOST_OUT_DX;
    sp[1] = POFV_GHOST_SPAWN_Y;
    sp[2] = 0.0f;
    *(int32_t *)(spawn + CE_SPAWN_SCORE) = 0;
    *(int32_t *)(spawn + CE_SPAWN_MAIN_DROP) = 0;   /* 奖励全走卡侧的灵力道具（口径 5）*/
    *(int32_t *)(spawn + CE_SPAWN_LIFE) = POFV_GHOST_HP;
    *(int32_t *)(spawn + CE_SPAWN_ECL_I) = 0;       /* $I0 = 0（正常）*/
    ((float *)(spawn + CE_SPAWN_ECL_F))[2] = sp[0]; /* %F2 = 初始目标（ECL 第一帧就 movePosTime 到它）*/
    ((float *)(spawn + CE_SPAWN_ECL_F))[3] = sp[1]; /* %F3 */

    e = (uint8_t *)ce_spawn_enemy(POFV_GHOST_ECL_NAME, spawn);
    if (!e) return -1;                              /* spawner 拒绝（池满 / ECL 没加载）*/
    id = *(int32_t *)(e + CE_ENEMY_ID);
    slot = pofv_ghost_register(&s_ghost, id, kind);
    if (slot < 0) {
        /* 池满（正常到不了：节拍已按同屏上限挡过）⇒ 喊刚建的那只走（$I0 = 999 飞离自删），
         * 绝不留一只没人管的孤儿敌人。*/
        *(int32_t *)(e + CE_ENEMY_ECL_I0) = 999;
        return -1;
    }
    s_ghost.s[slot].x = sp[0];
    s_ghost.s[slot].y = sp[1];
    ghost_vm_spawn(s, slot, sp[0], sp[1]);
    ce_log("pofv: ghost spawn kind %d id %d at (%d,%d) [%d live, %d made]",
           (int)kind, (int)id, (int)sp[0], (int)sp[1], (int)s_ghost.live, (int)s_ghost.spawned);
    return slot;
}

/* 每帧：① 逐只跟随 + 结界沈静化 + 失踪回收；② 沈静化到点成佛；③ 循环撒。*/
static void ghost_tick(pofv_card_t *s)
{
    uint8_t *em = CE_ENEMY_MGR();
    const float *pp = ce_player_pos();
    int32_t slots[POFV_GHOST_MAX];
    int i, n;

    if (!s) return;

    /* ① 逐只：找到敌人就跟随（写 %F2/%F3），找不到就回收槽（被击破的已在 on_enemy_death 处理）。*/
    for (i = 0; i < POFV_GHOST_MAX; ++i) {
        pofv_ghost_t *g = &s_ghost.s[i];
        uint8_t *e;
        if (!g->active) continue;
        e = ce_enemy_by_id(em, g->enemy_id);
        if (!e) {                                   /* 被清场（enmKillAll）/ 已经自删 ⇒ 槽回收 */
            ghost_vm_kill(i);
            pofv_ghost_release(&s_ghost, i);
            continue;
        }
        g->x = ((const float *)ce_enemy_pos(e))[0];
        g->y = ((const float *)ce_enemy_pos(e))[1];

        if (!g->calmed && pp) {
            float dx = pp[0] - g->x, dy = pp[1] - g->y;
            float ax = dx < 0.0f ? -dx : dx, ay = dy < 0.0f ? -dy : dy;
            float m = ax > ay ? ax : ay;
            float tx = g->x, ty = g->y;
            if (m > 1.0f) {                          /* 索敌：朝自机飘（Chebyshev 归一，不引 libm）*/
                float step = POFV_GHOST_HOMING_SPEED;
                if (step > m) step = m;
                tx += dx / m * step;
                ty += dy / m * step;
            }
            *(float *)(e + CE_ENEMY_ECL_F2) = tx;
            *(float *)(e + CE_ENEMY_ECL_F3) = ty;
            /* 口径 6：结界碰灵 ⇒ 沈静化（写 $I0 = 1；ECL 那边 lifeSet(50) + 停住）。*/
            if (pofv_aura_hit_ex(g->x - s_aura_cx, g->y - s_aura_cy, s_aura_r, 0.0f,
                                 s_aura.pad_linear)
                && pofv_ghost_calm(&s_ghost, i)) {
                *(int32_t *)(e + CE_ENEMY_ECL_I0) = 1;
                ce_log("pofv: ghost %d calmed by the barrier at (%d,%d)",
                       (int)g->enemy_id, (int)g->x, (int)g->y);
            }
        }
        /* ★★ 2026-10-01（owner 实跑：「**场上似乎在掉不可见的灵**」）：**没 VM 就重试**。
         *   原来只在出生那一刻试一次（`ghost_vm_spawn` 失败只记一条日志
         *   ——原文就是 `"ghost stays invisible"`）⇒ 那一刻能力 anm 还没加载好 / VM 池恰好满，
         *   那只灵就**一辈子不可见** ✗。⇒ 每 `POFV_GHOST_VM_RETRY` 帧补一次。*/
        if (!s_ghost_vm[i]) {
            if ((s_frame % POFV_GHOST_VM_RETRY) == 0) ghost_vm_spawn(s, i, g->x, g->y);
            if (!s_ghost_vm[i]) continue;                 /* 还没补上：这帧先跳过 */
        }
        ce_anm_set_pos((uint32_t)s_ghost_vm[i], g->x, g->y, 0.0f);
        /* ★★ 2026-10-01（owner：「**五种灵都做了吗**」）：**按种类选行、按帧轮播** ——
         *   5 种各一行 4 帧（`ORDER.txt` 里 20 条连续 ⇒ `SPIRIT0_0 + kind*4 + frame` ✓）。
         *   anm 脚本里写死一行换不了种类 ✗ ⇒ **由卡侧每帧写 sprite**（脚本 372 已简化成静态默认帧）✓。*/
        {
            uint32_t sp = (uint32_t)CE_ANM_ABILITY_SPRITE_POFV109_SPIRIT0_0
                        + (uint32_t)((g->kind % POFV_GHOST_KIND_MAX) * 4)
                        + (uint32_t)((s_frame / POFV_GHOST_FRAME_HOLD) % 4);
            ce_anm_set_sprite(CE_ABILITY_ANM(), (uint32_t)s_ghost_vm[i], sp);
        }
    }

    /* ② 成佛（沈静化满 `POFV_GHOST_CALM_FRAMES`）：喊 ECL 走（$I0 = 999 ⇒ 飞离自删）+ 收槽 + 删 VM。
     *    ⚠ `pofv_ghost_tick()` 已就地释放槽（`active = 0`），这里只读它的 enemy_id 去喊话。*/
    n = pofv_ghost_tick(&s_ghost, slots, POFV_GHOST_MAX);
    for (i = 0; i < n && i < POFV_GHOST_MAX; ++i) {
        int32_t k = slots[i];
        uint8_t *e = ce_enemy_by_id(em, s_ghost.s[k].enemy_id);
        if (e) *(int32_t *)(e + CE_ENEMY_ECL_I0) = 999;
        ghost_vm_kill(k);
        ce_log("pofv: ghost %d reached buddhahood (%d calm frames) — no drop (not a kill)",
               (int)s_ghost.s[k].enemy_id, POFV_GHOST_CALM_FRAMES);
    }

    /* ★★ 2026-10-01（owner：「**至少等到第一波怪出现再从场外出灵吧**」）：闩锁 ——
     *   扫到**任何一只活着的敌人**就把 `s_ghost_wave_seen` 置 1（只扫到置上为止，之后不再扫）。*/
    if (!s_ghost_wave_seen) {
        uint8_t *em0 = CE_ENEMY_MGR();
        uint8_t *e0 = 0;                     /* ★ 宏会把 e0 赋值 ⇒ 必须先声明 */
        if (em0) {
            CE_ENEMY_FOREACH(em0, e0) {
                (void)e0;
                s_ghost_wave_seen = 1;
                break;
            }
        }
    }

    /* ③ 循环撒（口径 9）：600 帧一只 / boss 在场不撒 / 同屏 ≤ 4 —— 三道门都在 core 里。
     *   ★ 外加 owner 那道门：**第一波怪出现之前一只都不撒**（见上面的闩锁）。*/
    {
        int32_t kind = 0;
        if (s_ghost_wave_seen && pofv_ghost_cycle_tick(&s_ghost, boss_enemy_present(), &kind)) {
            if (ghost_spawn(s, kind) < 0) {
                if (!s->ghost_logged) {
                    ce_log("pofv: ghost spawn failed (spawner refused) — skipped this cycle");
                    s->ghost_logged = 1;
                }
            }
        }
    }
}

/* ============================ 钩子 ============================ */

/* 敌人死亡（断点 `0x430240`，**在引擎撒主掉落之前**、链表仍挂着）。共享广播，只读。
 * 四张卡要的「击破」= `kind == CE_EDEATH_KILLED`（一手：`retaddr 0x42f3b7`，life ≤ 0 的主路）。
 * ⚠ 回调里**只做 O(1) 的事**：定档 + 起一个场（≤8 次距离比较 + 一次 spawn）（review §4.1）；
 *   不许析构敌人、不许缓存 `ev->enemy`、不改掉落表（110 才需要改）。
 *
 * ★★ 2026-09-22（P5 #4）：**连爆档的来源换了** —— 旧实现用「距上次击破 ≤30 帧就 +1 档」的窗口
 *   自己往上蹿（用户：「暴风的连锁似乎不该这么快」）。现在的档 = **点着它的那个场的档 + 1**
 *   （`pofv_storm_chain_at()`；没有场盖住它 ⇒ 自机弹打死的 ⇒ 1 档）。
 *   ⇒ 连锁只能跟着妖精走进场，档位由**妖精的到场率**决定 —— 正是 owner 说的
 *   「因为妖精的移动，连锁暴风实际上相对世界几乎静止」。 */
static void on_enemy_death(ce_card_t *c, const ce_enemy_death_t *ev)
{
    pofv_card_t *s = st(c);
    int chain, covering;

    if (!s || !ev) return;
    if (ev->kind != CE_EDEATH_KILLED) return;
    if (!(ev->flags & CE_EDEATH_F_REAL)) return;

    covering = pofv_storm_chain_at(&s_storm, ev->x, ev->y);
    chain = (s_storm_prof.chain && covering > 0) ? covering + 1 : 1;   /* ★ TH19 档不连锁 */
    if (chain > POFV_BLAST_CHAIN_MAX) chain = POFV_BLAST_CHAIN_MAX;
    s->st.chain_kills = chain;
    s->st.last_kill_frame = s_frame;
    pofv_account_kill(&s->st);
    /* ★★ 2026-10-01（owner：「**th19 一手是灵的消弹半径是杂鱼消弹半径的可能两倍**」）：
     *   死的是**我们自己撒的灵**时，起的那一下**半径取两倍**（= `dr × 寿命` 里的寿命 ×2 ✓，
     *   `dr` 与一手的档→半径表都不动）。⇒ 用「任意场」入口显式给寿命，杂鱼那条路不变。*/
    {
        int gslot_early = pofv_ghost_find(&s_ghost, (int32_t)ev->id);
        int ok;
        if (gslot_early >= 0) {
            int32_t life2 = pofv_storm_life_for_chain(chain) * POFV_GHOST_CANCEL_MUL;
            ok = pofv_storm_spawn_field(&s_storm, ev->x, ev->y, POFV_STORM_R0, POFV_STORM_DR,
                                        life2, POFV_STORM_KIND_STORM) >= 0;
        } else {
            ok = pofv_storm_spawn(&s_storm, ev->x, ev->y, chain) >= 0;
        }
        if (!ok) {
            if (!s->cap_logged) {
                ce_log("pofv: blast queue full — storm pool full, chain storm dropped");
                s->cap_logged = 1;
            }
        }
    }
    /* ★ 2026-10-01（口径 5）：击破 ⇒ 在**原位**掉一把灵力道具（充能的那笔账从直加点改成经道具）。
     * O(1)（一次线性找空槽，≤ `POFV_POWER_MAX` 次比较）—— 清场帧 100–300 次也够便宜。*/
    /* ★ 2026-10-01（owner：连破/击中数那条**改由 hits 计数器**管，见下面的 `on_bullet_hit`）：
     *   击破本身**仍然掉一颗**（这是本卡原来的口径，owner 没让改）。
     *   另外每满 `POFV_HITS_PER_DROP` 次**命中**额外掉一颗（一手 TH19「满 100 → 霊力アイテム」）。*/
    if (pofv_power_spawn(&s_power, ev->x, ev->y, POFV_POWER_PTS_KILL, POFV_POWER_SRC_KILL) < 0) {
        if (!s->power_logged) {
            ce_log("pofv: power pool full (%d) — power item dropped", POFV_POWER_MAX);
            s->power_logged = 1;
        }
    }

    /* ★★ 2026-10-01（口径 8/9/10）：如果死的这只正是**我们撒的灵** ⇒
     *   回收句柄（槽 + 跟随 VM）+ 掉「**一把**」灵力道具（口径 6「击破掉一把」）。
     *   ⚠ 只读 `ev->id`（不缓存敌人指针）；`pofv_ghost_find` 是 O(4)。*/
    {
        int gslot = pofv_ghost_find(&s_ghost, (int32_t)ev->id);
        if (gslot >= 0) {
            int32_t gpts = 0;
            ghost_vm_kill(gslot);
            pofv_ghost_release(&s_ghost, gslot);
            ++s_ghost.killed;
            pofv_power_spawn_packed(&s_power, ev->x, ev->y, POFV_GHOST_DROP_ITEMS,
                                    POFV_GHOST_PTS_KILL, POFV_GHOST_DROP_ITEMS,
                                    POFV_POWER_SRC_KILL, &gpts);
            ce_log("pofv: ghost %d killed at (%d,%d) — %d power items worth %d pts [%d live]",
                   (int)ev->id, (int)ev->x, (int)ev->y, POFV_GHOST_DROP_ITEMS, (int)gpts,
                   (int)s_ghost.live);
        }
    }
}

/* ★★ 2026-10-01（owner 甜头口径「抵挡护盾……**保命等级最优先，无论如何先消耗它**」）——
 *   中弹（虚表 `+0x10`，救援层的**最早那一层**：比决死 B 与 113 的 `+0x0c` 都早）：
 *   **有盾就无条件吃**（不与其他救援者抢，也不让位 —— `EVENTS-CONTRACT.md` §5.2 的语义是
 *   「一次中弹把当前所有救援型提供者都烧掉」，所以 107 / 117 照旧烧它们自己的）。
 *
 *   一手对应（TH19）：被弹处理 `FUN_00549570` 开头 `if (PlayerInf+0x18 == 1 && バリア吸收() != 0) return;`
 *   ⇒ **有盾时这一发根本不进「真正的被弹处理」**；`FUN_00544c90` 里 `if (*p != 1) return 0` ⇒ 挡 1 发。
 *   ⚠ 本卡没有 TH19 那层「命中的前处理」⇒ 只能在救援层里等价成「吃盾 + 取消这次死亡」，
 *     代价是**盾破的那一发仍然走了一遍「死亡已发起」**（会掉决死窗那种瞬间）。登记 `TUNING.md` §2.0v-3。*/
static int on_death_before_deathbomb(ce_card_t *c)
{
    pofv_card_t *s = st(c);
    void *p;

    if (!s) return 0;
    if (!pofv_shield_take(&s->st)) return 0;       /* 没盾 ⇒ 什么都不做（107/117/决死 B/113 照走）*/

    (void)ce_rescue_note_claim(109);               /* 报到（唯一消费者是最后一家 113；返回值不用）*/
    p = CE_PLAYER();
    if (p) ((void (__attribute__((thiscall)) *)(void *))POFV_FN_CANCEL_IMPENDING_DEATH)(p);
    /* 吃盾后给一小段无敌（❓ 自定：与 107 / 117 同一档 40 帧）—— 否则下一颗弹立刻再吃一次。*/
    ce_invuln_request(ce_card_id(c), POFV_SHIELD_INVULN_FRAMES);
    ce_invuln_apply();
    s_shield_broken_log = 1;                       /* 让跟随 VM 下一帧发「破盾一闪」（事件 1）*/
    if (!s->shield_logged) {
        s->shield_logged = 1;
        ce_log("pofv: shield ABSORBED the hit — death cancelled, %d frames invuln (parts 0 left)",
               POFV_SHIELD_INVULN_FRAMES);
    }
    return 0;   /* 返回值被 Player__die 忽略；救命靠 cancel_impending_death 的副作用（107/117 同款）*/
}

/* ★★ 2026-10-01（合成卡 owner 口径 2）：**蓄力 = 按住 C 的电平**推进。
 *   上升沿 `begin` / 每帧 `hold` / 松手 `release` 三步都在这里；
 *   **Z 完全不参与** —— 原生射击照常（拦 Z 那条 `on_native_shot_suppress` 已整条删除，
 *   一手 10 帧闸见 `pofv_core.h` 的历史注释，本卡刻意不复刻）。*/
/* ★★ 2026-10-02（Jera 88「杰拉」）：本卡场上的**灵力道具**交给 SDK 的「复制代数闸」——
 * 每帧上报可见数，代数一变就把活着的道具按**同坐标 / 同打包值 / 同来源**各重撒一颗
 * （`pofv_power_spawn` 是它唯一的出生口 ⇒ 复制品与原件逐字同源，连飞散动态都照原样重抽）。
 * 语义见 `sdk_core.h` 末节与 `cards/jera.c` 文件头。
 * ⚠ 先快照 `active`：`pofv_power_spawn` 找的是**空槽**（不一定是池尾）⇒ 边遍历边生成会复制到复制品。*/
static uint32_t s_dup_seen;

static void pofv_dup_run(void)
{
    uint8_t was[POFV_POWER_MAX];
    int i, n = 0;
    for (i = 0; i < POFV_POWER_MAX; ++i) was[i] = (uint8_t)(s_power.s[i].active ? 1 : 0);
    for (i = 0; i < POFV_POWER_MAX; ++i) {
        const pofv_power_t *it = &s_power.s[i];
        if (!was[i] || !it->active) continue;
        if (pofv_power_spawn(&s_power, it->x, it->y, it->value, it->src) < 0) break;   /* 池满就停 */
        ++n;
    }
    ce_log("pofv: jera dup %d power items (live %d/%d)", n, (int)s_power.live, POFV_POWER_MAX);
}

/* ★★ 2026-10-01（owner：「看字体基本就是这两个之一」）：hits UI 的**自带字形**绘制。
 *
 *   字形 = TH19 `ascii/ascii_norm2.png` 的切片（`POFV109_HITS_D0..D9` / `_H` `_I` `_T` `_S`）；
 *   VM = 复用共享脚本 `CE_ANM_ABILITY_SCRIPT_HUD_NUMBER`（`sprite(@TS_DIGIT_0); …; stop();`
 *        —— 一个**单 sprite VM**，字形由卡侧 `ce_anm_set_sprite` 换）✓。
 *   显示条件：`hits >= POFV_HITS_UI_MIN`（一手门槛 10，见 `0x51d84e`）；否则全部删掉。
 *   ⚠ 前导零不画（`hits = 42` 只画两位）—— 与「多少 hits」的读数习惯一致。*/
static void hits_ui_kill(void)
{
    int i;
    for (i = 0; i < POFV_HITS_UI_VMS; ++i) {
        if (s_hits_vm[i]) { ce_anm_delete(s_hits_vm[i]); s_hits_vm[i] = 0; }
    }
}

static void hits_ui_sync(pofv_card_t *s)
{
    /* 每个字形两台 VM：**描边（`*_EDGE`，染黑）先起 ⇒ 画在下面**，白字后起 ⇒ 画在上面 ✓
     * （一手用法：owner「可能是 `ascii_bigf.png` **染成黑色叠在下面**」）。*/
    static const uint32_t label[4][2] = {
        { CE_ANM_ABILITY_SPRITE_POFV109_HITS_H_EDGE, CE_ANM_ABILITY_SPRITE_POFV109_HITS_H },
        { CE_ANM_ABILITY_SPRITE_POFV109_HITS_I_EDGE, CE_ANM_ABILITY_SPRITE_POFV109_HITS_I },
        { CE_ANM_ABILITY_SPRITE_POFV109_HITS_T_EDGE, CE_ANM_ABILITY_SPRITE_POFV109_HITS_T },
        { CE_ANM_ABILITY_SPRITE_POFV109_HITS_S_EDGE, CE_ANM_ABILITY_SPRITE_POFV109_HITS_S },
    };
    uint32_t want[POFV_HITS_UI_VMS];
    float    xs[POFV_HITS_UI_VMS];
    int      n = 0, k, lead, v, g;
    void    *anm = CE_ABILITY_ANM();

    if (!s || !anm) return;
    if (s->st.hits < POFV_HITS_UI_MIN) { hits_ui_kill(); return; }

    for (g = 0; g < 4; ++g) {                       /* 标签 "HITS" */
        for (k = 0; k < 2; ++k) {
            want[n] = label[g][k];
            xs[n] = POFV_CHAIN_UI_X + POFV_HITS_UI_PITCH * (float)g;
            ++n;
        }
    }
    v = s->st.hits;
    {   int d[POFV_HITS_UI_DIGITS], pos;
        for (k = POFV_HITS_UI_DIGITS - 1; k >= 0; --k) { d[k] = v % 10; v /= 10; }
        lead = 0;
        while (lead < POFV_HITS_UI_DIGITS - 1 && d[lead] == 0) ++lead;
        for (k = lead, pos = 0; k < POFV_HITS_UI_DIGITS; ++k, ++pos) {
            uint32_t base = (uint32_t)(CE_ANM_ABILITY_SPRITE_POFV109_HITS_D0 + d[k]);
            for (g = 0; g < 2; ++g) {              /* 0 = 描边、1 = 白字 */
                want[n] = base + (g == 0 ? POFV_HITS_EDGE_DELTA : 0u);
                xs[n] = POFV_CHAIN_UI_X + POFV_HITS_UI_PITCH * 4.0f + POFV_HITS_UI_GAP
                        + POFV_HITS_UI_PITCH * (float)pos;
                ++n;
            }
        }
    }

    for (k = 0; k < POFV_HITS_UI_VMS; ++k) {
        if (k >= n) { if (s_hits_vm[k]) { ce_anm_delete(s_hits_vm[k]); s_hits_vm[k] = 0; } continue; }
        if (s_hits_vm[k] && !ce_anm_get_vm(s_hits_vm[k])) s_hits_vm[k] = 0;
        if (!s_hits_vm[k]) {
            float pos[3];
            if (k % 2 == 0) continue;               /* ★ 描边必须先起：等白字那台落下再补（下一帧起）*/
            pos[0] = xs[k]; pos[1] = POFV_CHAIN_UI_Y; pos[2] = 0.0f;
            s_hits_vm[k - 1] = ce_anm_spawn_ui(anm, CE_ANM_ABILITY_SCRIPT_HUD_NUMBER, pos);
            s_hits_vm[k]     = ce_anm_spawn_ui(anm, CE_ANM_ABILITY_SCRIPT_HUD_NUMBER, pos);
            if (!s_hits_vm[k]) { if (s_hits_vm[k - 1]) { ce_anm_delete(s_hits_vm[k - 1]); s_hits_vm[k - 1] = 0; } continue; }
            ce_anm_set_color(s_hits_vm[k - 1], POFV_HITS_EDGE_RGBA);   /* 描边 = 黑 */
        } else {
            ce_anm_set_pos(s_hits_vm[k], xs[k], POFV_CHAIN_UI_Y, 0.0f);
        }
        ce_anm_set_sprite(anm, s_hits_vm[k], want[k]);
    }
}

static int on_tick_2(ce_card_t *c)
{
    pofv_card_t *s = st(c);
    int charge, lvl = 0;
    int32_t i;
    int scan_budget;
    const pofv_storm_t *bl;

    if (!s) return 0;
    ++s_frame;
    ce_dup_publish((uint32_t)ce_card_id(c), (int)s_power.live);
    if (ce_dup_should_run(&s_dup_seen)) pofv_dup_run();

    /* ★★ 2026-10-01：hits UI —— 自带 TH19 字形（见 `hits_ui_sync()` 的长注释）。*/
    hits_ui_sync(s);

    /* 对话中：蓄力打断、不发动（照 117 的处理）；商店里 AbilityManager 整个冻着，进不到这里。
     * 无敌窗口也在对话里冻结（引擎自己的计时器同样不走）。*/
    if (CE_GUI_MSG_ACTIVE()) {
        pofv_charge_cancel(&s->st);
        s->prev_charge = 1;   /* 当成「一直按着」⇒ 对话结束后要重新按才会开始蓄力（不误发）*/
        return 0;
    }

    /* 无敌声明：窗口内每帧声明剩余帧数（层取最大值），窗口外退掉声明。
     * ★ 任务 4：剩余量由 `pofv_invuln_tick()` 算（**先递减再声明**）⇒ 声明量 > 0 的帧数
     *   恰好 = `pofv_ca_invuln_frames(level)`（旧写法多 1 帧）。返回 0 = 窗口结束 ⇒ 退声明。*/
    {
        int32_t decl = pofv_invuln_tick(&s->invuln);
        ce_invuln_request(ce_card_id(c), decl);
        if (decl > 0) ce_invuln_apply();
    }

    /* ★ 合成卡（owner 2026-09-28）：蓄力 = **按住 C 的电平**（不是边沿触发的一次性动作）。
     *   按住期间的上升沿 `begin`、每帧 `hold`、松开 `release` 三步都在这里；
     *   Z 不参与（原生射击不受本卡任何影响 —— 拦 Z 已整条删除）。
     *   ⚠ 旧实现里 Shift（`focus`）还会开「固灵吸灵领域」——**那条已在 2026-10-01 整条撤掉**：
     *     口径 4 的固灵结界是**常驻**的（见下面那个 aura 块），与 Shift 无关。*/
    charge = (CE_INPUT_HELD() & POFV_INPUT_CHARGE) != 0;

    if (charge && !s->prev_charge) pofv_charge_begin(&s->st);
    if (charge) {
        if (pofv_charge_hold(&s->st))                     /* 升级 →「叮」一声（一手：每升一级）*/
            ce_play_sound(POFV_SE_CHARGE_DING, ce_player_x());
    } else if (s->prev_charge) {
        if (pofv_charge_release(&s->st, &lvl)) ca_effect(s, lvl, /*grant_invuln=*/1);
        /* 返回 0 = 没蓄到 C1（点射）/ 气不够 ⇒ 什么都不做 */
    }
    s->prev_charge = charge;

    debug_hotkeys(s);   /* ★ 调试：空格 = 发 C4（见上面 `debug_hotkeys()` 的长注）*/

    /* ★ 2026-10-01 owner 追加口径：结界的档案按**自机角色**选（咲夜 = TH09 锚定场 /
     *   早苗 = TH19 圆 + Shift 蓄力 / 灵梦·魔理沙 = 占位）。
     *   ⚠ 必须先算（`ghost_tick` 的「结界碰灵 ⇒ 沈静化」要用本帧的圆心/半径/几何支）。*/
    aura_update();
    shield_vm_update(s);     /* ★ 护盾的视觉（有盾才显示；与救援层无关，纯表现）*/
    bossatk_fire(c, s);      /* ★ C3/C4 的 boss attack：往"上"打的原生自机弹幕 */

    /* ★ 2026-10-01（口径 8/9/10）：**灵**（真敌人）—— 跟随 / 索敌 / 结界沈静化 / 循环撒
     *   （三道门：600 帧、boss 在场不撒、同屏 ≤ 4）全在 `ghost_tick()` 里。 */
    ghost_tick(s);

    /* 帧末统一推进**暴风场**（review §4.1 的预算仍然在）：
     * 每个活着的场**每帧**都判一次（伤害源 + 圈内灵），整池扫弹按 `POFV_STORM_SCANS_PER_FRAME`
     * 出闸 —— 这就是「场」与「一次性爆风」的全部差别（妖精飞进来才被点着 ⇒ 连锁跟着妖精走）。*/
    scan_budget = POFV_STORM_SCANS_PER_FRAME;
    for (i = 0; i < POFV_STORM_MAX; ++i) {
        bl = pofv_storm_slot(&s_storm, (int)i);
        /* ★ 2026-10-01：**暴风与 CA 红圈共用这一个场池**，所以这里一次遍历两种都跑到
         *   （`do_storm_frame()` 按 `kind` 分流）。⚠ 扫描预算 `POFV_STORM_SCANS_PER_FRAME`
         *   是**两种场共用**的 ⇒ 同屏场多时每个场扫到的频率会降。*/
        if (bl && bl->active) do_storm_frame(s, bl, &scan_budget);
    }
    /* 推进放在后面：本帧出生/本帧结算过的场，age 从这一帧开始算（寿命 = 该档的 `R_k / dr` 帧）。*/
    pofv_storm_tick(&s_storm);

    /* ★★ 2026-10-01：**场的视觉**（owner：「暴风缺少UI」）—— 每帧跟一遍场池（起/挪/缩放/染色/删）。*/
    storm_vm_sync();

    /* ★ 2026-10-01（口径 4 + 6）：**固灵结界**（常驻）——
     *   碰敌弹 ⇒ 掉灵力道具（每颗弹只算一次 + 每帧上限 + **打包面值守恒**）。
     *   ⚠ 位置 = 被碰到的弹的**原位**（一手 TH19 的灵力道具就从实体原位掉）。 */
    {
        if (s_aura_r > 0.0f) {
            float ctr[3];
            float ox[POFV_AURA_ITEMS_PER_FRAME], oy[POFV_AURA_ITEMS_PER_FRAME];
            int hit;
            ctr[0] = s_aura_cx; ctr[1] = s_aura_cy; ctr[2] = 0.0f;
            hit = aura_scan_bullets(ctr, s_aura_r, s_aura.pad_linear, ox, oy,
                                    POFV_AURA_ITEMS_PER_FRAME);
            if (hit > 0) {
                int32_t apts = 0;
                pofv_power_spawn_packed_at(&s_power, ox, oy, hit, POFV_POWER_PTS_AURA,
                                           POFV_AURA_ITEMS_PER_FRAME, POFV_POWER_SRC_AURA, &apts);
                s_aura_hits += hit;
                if (hit >= POFV_AURA_ITEMS_PER_FRAME)     /* 每帧上限生效时留一行（诊断）*/
                    ce_log("pofv: aura touched %d bullets (frame cap %d, packed %d pts)",
                           hit, POFV_AURA_ITEMS_PER_FRAME, (int)apts);
            }
        }
    }

    /* ★ 2026-10-01（口径 5）：灵力道具推进 —— 飘落 / 磁吸 / 收取。
     *   收取的那笔面值**当场进攻击槽**（`pofv_gauge_add_points`），这是充能的**唯一**入口。 */
    {
        int32_t got_pts = 0;
        int got_n = pofv_power_tick(&s_power, ce_player_pos(), &got_pts);
        if (got_n > 0 && got_pts > 0) {
            ce_play_sound(POFV_POWER_SE_GET, ce_player_x());   /* ★ 一手：收取音（见常量处）*/
            {
                int32_t lv0 = s->st.gauge;
                pofv_gauge_add_points(&s->st, got_pts);
                /* ★ 一手：**升档**另有音（TH16 季节道具升档播 0x3f）—— 本卡的对照事件 = 气槽 +1 */
                if (s->st.gauge != lv0) ce_play_sound(POFV_POWER_SE_LEVEL, ce_player_x());
            }
            s_power_got += got_n;
            ce_log("pofv: power +%d (%d items, gauge %d.%d)", (int)got_pts, got_n,
                   (int)s->st.gauge, (int)(s->st.gauge_pts * 100 / POFV_GAUGE_PTS_PER_LEVEL));
        }
    }
    if (ce_sweep_should_run(&s_sweep_seen)) pofv_sweep_to_money();   /* ★ 92 清扫：灵力道具 → 金钱 */
    power_vm_sync();                            /* ★ 灵力道具的视觉（每颗一台跟随 VM）*/

    /* SP 到档 → boss attack（一手：10 万 / 30 万 / 50 万）。*/
    if (pofv_sp_take_attack(&s->st)) do_boss_attack(s);

    return 0;
}

/* ============================ 生命周期 ============================ */

/* 收干净：**暴风场池** + **灵力道具池** + **灵（敌人 + 跟随 VM）** + 结界闩锁 + 无敌声明。
 * ⚠ 删卡发生在**商店里**（倒吊人 / 原版空白卡清空整副牌），那一刻 AbilityManager 冻着、
 *   卡的 tick 全停 ⇒ 只能靠 `on_delete`（虚表 `+0x50`）收。
 *   ⇒ 灵的敌人与 VM **也必须在这里自己收**（tick 里那套回收逻辑这一帧不会跑）。*/
static void clear_card(ce_card_t *c)
{
    pofv_card_t *s = st(c);
    uint8_t *em = CE_ENEMY_MGR();
    int i;

    /* ★ 诊断账（都在清池**之前**打）：本函数同时服务三条路（每关重挂 / 一局重开 / 删卡）
     *   ⇒ 这行是「本段账」，关卡换段也会打一次（数据更多，不是错）；全 0 时不出声（不刷屏）。*/
    if (s_power.spawned || s_power_got || s_power.expired || s_power.dropped)
        ce_log("pofv: power run done - %d items made, %d picked (+%d pts), %d expired, %d packed away (pool %d)",
               (int)s_power.spawned, (int)s_power_got, (int)s_power.absorbed,
               (int)s_power.expired, (int)s_power.dropped, POFV_POWER_MAX);
    if (s_aura_hits)
        ce_log("pofv: aura run done - %d bullets touched by the barrier (last r = %d, mode %d, char %d)",
               (int)s_aura_hits, (int)s_aura_r, (int)s_aura.mode, (int)*(const int32_t *)CE_ADDR_CHARACTER);
    if (s_ghost.spawned || s_ghost.killed || s_ghost.calmed_total || s_ghost.expired || s_ghost.live)
        ce_log("pofv: ghost run done - %d made, %d killed, %d calmed, %d reached buddhahood, %d still live",
               (int)s_ghost.spawned, (int)s_ghost.killed, (int)s_ghost.calmed_total,
               (int)s_ghost.expired, (int)s_ghost.live);
    s_power_got = 0;
    s_aura_hits = 0;

    /* ★ 伪 boss 载体（C3/C4 的 ECL 侧）：**必须喊走** —— 它跨帧活着，而删卡/换关都发生在这里
     *   （那一刻卡的 tick 已经停了，只有 on_delete 这条路跑得到；见 METHOD 的「长持续效果」铁律）。
     *   不喊走就会留一只没人管的孤儿敌人（不设标志、不掉东西，但会一直发弹）。*/
    bossatk_carrier_stop();

    /* ★ 灵：先喊所有活着的灵「走」（`$I0 = 999` ⇒ ECL 那边飞离自删），再收 VM，最后清句柄池。
     *   ⚠ 顺序不能反：清池之后就找不到 enemy_id 了 ⇒ 会留下一批没人管的孤儿敌人。*/
    for (i = 0; i < POFV_GHOST_MAX; ++i) {
        if (!s_ghost.s[i].active) continue;
        if (em) {
            uint8_t *e = ce_enemy_by_id(em, s_ghost.s[i].enemy_id);
            if (e) *(int32_t *)(e + CE_ENEMY_ECL_I0) = 999;
        }
        ghost_vm_kill(i);
    }
    pofv_ghost_clear(&s_ghost);
    memset(s_ghost_vm, 0, sizeof s_ghost_vm);

    /* ★ 结界的跟随 VM：同样必须在这里删（删卡那一刻 tick 停着，画面上的环不会自己走）。*/
    if (s_aura_vm) { ce_anm_delete((uint32_t)s_aura_vm); s_aura_vm = 0; }
    if (s_shield_vm) { ce_anm_delete((uint32_t)s_shield_vm); s_shield_vm = 0; }
    if (s_shield_vm2) { ce_anm_delete((uint32_t)s_shield_vm2); s_shield_vm2 = 0; }
    s_shield_broken_log = 0;
    if (s->opt.ptr) ce_option_drop(c, &s->opt);   /* ★ 发射口（子机槽）必须收干净 */
    pofv_bossatk_stop(&s->bossatk);
    s->bossatk_logged = 0;
    s_aura_r = 0.0f;                 /* 结界状态复位（下一帧 aura_update 重新选档案）*/
    s_aura_age = -1;
    s_shift_frames = 0;
    s_aura_char = -1;

    /* ★ 灵力道具的跟随 VM：删卡时场上的每一颗都要删（否则留下满屏幽灵光球）。*/
    for (i = 0; i < POFV_POWER_MAX; ++i) {
        if (s_power_vm[i]) { ce_anm_delete((uint32_t)s_power_vm[i]); s_power_vm[i] = 0; }
    }

    pofv_storm_pool_init(&s_storm);
    pofv_power_clear(&s_power);                 /* ★ 灵力道具池：删卡/重开/换关都清干净 */
    memset(s_aura_done, 0, sizeof s_aura_done); /* ★ 结界闩锁：与弹池一起清（删卡后不留幽灵标记）*/
    s_frame = 0;
    if (!s) return;
    pofv_state_init(&s->st);
    ce_invuln_request(ce_card_id(c), 0);   /* 退出无敌声明层（不截短别人已给的计时器）*/
    pofv_invuln_init(&s->invuln);          /* 窗口清零（声明已在上一条退掉）*/
    s->prev_charge = 0;
    s->ghost_logged = 0;
    s->cap_logged = 0;
    s->power_logged = 0;
    s->shield_logged = 0;
    /* ★ boss attack：收手 + **删掉自己那格子机**（长持续效果必须在 on_delete 里收干净，
     *   METHOD 铁律 —— 删卡发生在商店、那一刻卡的 tick 全停）。*/
    if (s->opt.ptr) ce_option_drop(c, &s->opt);
    pofv_bossatk_stop(&s->bossatk);
    s->bossatk_logged = 0;
}

static int on_ctor(ce_card_t *c)
{
    pofv_card_t *s = st(c);
    if (!s) return 0;
    clear_card(c);                          /* 新对象 / 每关重挂：清成初始态（含文件级池）*/
    /* ★★ 2026-10-01（owner：「**特别地，获取109这张卡自动赠送一个罩子**」）：
     *   拿到卡的这一刻**直接给一个罩子**（= 蓄力到 C3/C4 才会有的那个抵挡盾）。
     *   ⚠ 必须放在 `clear_card()` **之后**（那函数会把 `st.shield` 清 0）。
     *   走的是同一个入口 `pofv_shield_arm()`（档位传「能起盾的最低档」✓ 一手：只有阶段 3/4 回复，
     *   见 `0x40b420`）—— 不另开一条写字段的路。*/
    if (pofv_shield_arm(&s->st, POFV_SHIELD_ARM_LEVEL))
        ce_log("pofv: card acquired → one barrier (罩子) granted");
    ce_log("pofv: ctor (T = %.1f white-bullet radius; charge on hold-C, no Z suppression; "
           "power items = self-built pool; ghosts = real enemies via %s)",
           POFV_WHITE_R_MAX, POFV_GHOST_ECL_NAME);
    return 0;
}

static void on_run_reset(ce_card_t *c) { clear_card(c); }
/* ★★★ 2026-10-01（owner：「**th19 有一个多少 hits 的 UI**」）：**击中数**计数。
 *
 *   一手（`sdk/public.h`）：站点 `ce_bullet_hit`（`0x45f466`）= 「**一发自机弹 × 一个敌机**」的命中；
 *   一手（`engine/versus/th19/04-manual-spec.md` 第 7 行）：**「combo 满 100 → kind 9」**
 *     （kind 9 = **霊力アイテム**）⇒ **每满 100 hits 掉一颗灵力道具** ✓。
 *
 *   ⚠ 回调纪律（照 108）：**只做最少的活**（累加 + 偶尔掉一颗道具），别在这里扫池/建 VM
 *     —— 它会以「一发自机弹 × 一个敌机」的颗粒度被调（大连爆一帧可能几十次）。*/
static void on_bullet_hit(ce_card_t *c, const ce_bullet_hit_t *ev)
{
    pofv_card_t *s = st(c);
    int32_t full;
    if (!s || !ev) return;
    full = pofv_hits_add(&s->st, 1);
    while (full-- > 0) {                       /* 每跨过 100 次命中：在命中点掉一颗 */
        if (pofv_power_spawn(&s_power, ev->x, ev->y, POFV_POWER_PTS_KILL, POFV_POWER_SRC_KILL) < 0) {
            if (!s->power_logged) {
                ce_log("pofv: power pool full (%d) — hits drop lost", POFV_POWER_MAX);
                s->power_logged = 1;
            }
            break;
        }
    }
}

static void on_delete(ce_card_t *c)    { clear_card(c); }

/* ★ 合成卡（owner 2026-09-28）：**没有** `.on_native_shot_suppress` —— Z 是纯原生的。
 *   （旧值 `on_native_shot_suppress = on_native_shot_suppress` 已删；见本文件头的键位段。）*/
CE_CARD(109,
        .ctor = on_ctor,
        .on_death_before_deathbomb = on_death_before_deathbomb,   /* ★ 抵挡护盾（救援层最早那一层）*/
        .on_enemy_death = on_enemy_death,
        .on_bullet_hit = on_bullet_hit,   /* ★ 击中数（TH19 那个 hits UI 的来源）*/
        .on_tick_2 = on_tick_2,
        .on_run_reset = on_run_reset,
        .on_delete = on_delete);
