/* mof_green_core.h —— 系统卡 110（风神录）：「把一部分妖精改成绿妖精」的**纯逻辑层**。
 * 主机单测：`tests/test_mof_green_core.c`。
 *
 * ## 这是什么（用户 2026-09-18）
 *
 * 用户口径：「**更改部分妖精到绿妖精**：改其**精灵**和**掉落物**，并且**跟着卡走**（没有卡的时候不改）」；
 * 掉落那半的裁决是「**替换**：清掉原生 P/金钱，只撒大信仰点」；
 * 名单那半的裁决是「**固定几只**……我不知道，得看情况」⇒ 因此名单做成**表 + 每行一个开关**，
 * 改名单 = 改这一行（不碰任何机制）。全量预研见 `REPORT-110-GREEN-FAIRY.md`。
 *
 * ## 为什么能只靠换贴图表项就「变绿」（一手）
 *
 * TH18 自己的 `enemy.anm` 里，每一个「A 族妖精」剪影都有**绿色同形变体**：
 * 逐 pose 的 alpha 掩码 IoU 中位 **1.000**、每条 pose 的 alpha 像素数**逐条相等**
 * （⇒ 不是「长得像」，是**同一张剪影换调色板**）。工具：`native/tools/measure_enemy_variants.py`。
 *
 * 于是「变绿」= 把**源变体的 12 个 sprite 表项**整项覆盖成**绿色变体的 12 项**
 * （`AnmLoaded__set_sprite` `th18:0x477b00` 一手：`anm+0x11c` 是表指针、每项 **0x44** 字节、
 *   `id*0x11` 个 dword；脚本每条 `ins_300(spriteN)` 都会重写 `vm+0x24` 并重取 UV）
 * ⇒ **零新增断点、零 VM 重建、与 ECL 零交互**。本层只做「内存块的原语」，不认任何引擎地址
 * （表指针与 `anm` 的解析在 `cards/mof.c`）。
 *
 * ## 一手数据（表本体的每一行都来自这里）
 *
 * `enemy.anm`（`anmSelect(2)`）里三个形状组，每组四支变体（sprite 基址 / 脚本区间 / 实测色相）：
 *
 *   形状组 A（基址 0）：  0 蓝 228 ｜ 12 红 12 ｜ **24 绿 128** ｜ 36 橄榄 60
 *   形状组 B（基址 220）：220 蓝 196 ｜ 232 红 9 ｜ **244 绿 136** ｜ 256 橙 57
 *   形状组 C（基址 160）：160 蓝 233 ｜ 172 紫 300 ｜ **184 绿 165** ｜ 196 紫 300
 *
 * **重映射三组同式**：`+10 脚本 ⟺ +24 sprite`（蓝→绿）、`+5 ⟺ +12`（红→绿）、`−5 ⟺ −12`（第四色→绿）。
 * （脚本差 ÷ 5 = 变体差；sprite 差 = 变体差 × 12 ⇒ 两组永远同步。单测钉住这条不变式。）
 *
 * ⚠ **不能变绿的**（该剪影在 TH18 里只有一支，没有同形异色）：**大妖精 `GirlD01/02/03`**
 * （基址 96）、`GirlB01/02`（48/60）、`GirlC01`（72/84 与 292/304）、`Phantom*`（148 那一片）
 * ⇒ 要它们也变绿只能**外供 TH10 贴图**（重新打包 `enemy.anm`，成本高一个数量级，见报告 §5）。
 * ✅ **本来就是绿的、且已经在宿主表里的**：`(2,10)` st05 `GirlBlueA03` / st06 `GirlA04`、
 * `(2,56)` `Cir01a/Cir02a`、`(2,68)` st01 `Cir00m`、`(2,80)` st01 `Cir00bm`
 * —— ★ 用户 2026-09-18 点名的「**一面开头的红绿阴阳玉**」正是 `Cir00p`（红 0°）与
 *   `Cir00m`（绿 127°）这一对，**绿的那只早就是宿主了**（10 个站点），无需再改。
 */
#pragma once
#ifndef MOF_GREEN_CORE_H
#define MOF_GREEN_CORE_H
#include <stdint.h>

/* 每个变体 = 12 个连续 sprite（一手：`thanm -l` 逐条；5 个脚本共用这一块）。*/
#define MOF_GREEN_SPRITES 12
/* 每个变体 = 5 个**连续脚本**（pose 0..4）——「+10 脚本 ⟺ +24 sprite」那条不变式的步长就是它。
 * 一手：三组变体的脚本基址差都能被 5 整除（`mof_green_core.c` 的表 + 头注释那张色相表）。*/
#define MOF_GREEN_POSE_MAX 5
/* 一个 sprite 表项的字节数（一手 `0x477b1d`：`ecx = id*0x11` 个 dword ⇒ id*0x44）。*/
#define MOF_GREEN_ENTRY   0x44
/* 一项里「宽/高」两个 float 的位移（一手 `0x477b86 movss xmm3,[eax+ecx*4+0x38]` /
 * `0x477b94 movss xmm2,[eax+ecx*4+0x34]`）——同形判据在**运行期**的落地：这两条必须逐条相等。*/
#define MOF_GREEN_WH_OFF  0x34
/* 一个「对」要备份/覆盖的字节数。*/
#define MOF_GREEN_PAIR_BYTES (MOF_GREEN_SPRITES * MOF_GREEN_ENTRY)

typedef struct {
    int16_t     src_sprite;   /* 源变体的首个 sprite（12 个连续）*/
    int16_t     dst_sprite;   /* 绿色变体的首个 sprite */
    int16_t     src_script;   /* 源变体的首个脚本（死亡判据用；该变体共 5 个脚本）*/
    int16_t     dst_script;   /* 绿色变体的首个脚本 */
    uint8_t     enabled;      /* ★ 名单开关：0 = 这一对不参与（默认哪些见 .c 的表注释）*/
    uint8_t     stage_mask;   /* ★ 只在这些关生效：bit(stage-1)，stage 是 `CE_CURRENT_STAGE()` 的
                               *   **1-based** 值；**0 = 所有关**。用户 2026-09-18 口径「一波改一两只/
                               *   纯信仰点掉落的没那么多」⇒ 同一支妖精**跨关共用外观**（`S=184` 同时被
                               *   st02/st03/st07 用），不按关收窄就会一次改掉三关 ⇒ 有了这一列才能
                               *   做到「只补 st03」「只在某关的那一支变绿」。*/
    const char *who;          /* 诊断串：这一对是「谁」*/
} mof_green_pair_t;

#define MOF_GREEN_PAIR_MAX 8

/* 表本体（顺序无关；`MOF_GREEN_PAIR_MAX` 行为全 0 视为「没有这一行」）。*/
extern const mof_green_pair_t mof_green_pairs[MOF_GREEN_PAIR_MAX];

/* 在场（enabled）的对数。*/
int mof_green_active_count(void);

/* ★ 2026-09-18c：把「计划 / 判据」做成**带表参数**的版本 —— 默认名单已全部关掉（用户选了「不碰
 * 原版妖精」那条路），但机制与判据仍必须可测 ⇒ 单测用一张**合成表**就能把两条路都钉住。
 * 下面两个是卡片实际调用的入口（用全局表）。*/
void mof_green_plan_tbl(const mof_green_pair_t *tbl, int n, int32_t stage, uint32_t applied,
                        uint32_t *to_apply, uint32_t *to_restore);
const mof_green_pair_t *mof_green_lookup_tbl(const mof_green_pair_t *tbl, int n, uint32_t applied,
                                             int32_t anm_src, int32_t anm_script);

/* 死亡判据：这只敌人（`anm_src`/`anm_script` = `enemy+0x1494/+0x1498` 的读数）是不是
 * 「我们**此刻真的**把它变绿了」的那一支 ⇒ 返回那一对（没有 = NULL）。
 * ★ `applied` = 卡侧维护的「已改」位掩码（bit i = 第 i 行已生效）。**判据必须用「已改」而不是
 *   「表里开着」**：体检失败 / 关卡不在 `stage_mask` 里 / 卡刚进场还没抹上 ⇒ 它看起来仍是原色，
 *   这时按宿主给点就会出现「蓝妖精掉绿点」（用户口径是「变绿 = 掉绿点」，两者不许分开）。
 * ★ 只有 `anm_src == 2`（`enemy.anm`）才算 —— 变体表全部来自这个文件。*/
const mof_green_pair_t *mof_green_lookup(uint32_t applied, int32_t anm_src, int32_t anm_script);

/* 这一行在当前关生效吗。`stage` = `CE_CURRENT_STAGE()`（1-based；<=0 / >7 一律当「不在」。*/
int mof_green_pair_in_stage(const mof_green_pair_t *p, int32_t stage);

/* 逐帧计划：给定当前关与「已改」掩码，报出这一步要 apply / restore 的位掩码。
 * 两个输出都是**相对当前状态**的动作（apply ∩ applied == 0、restore ⊆ applied）。*/
void mof_green_plan(int32_t stage, uint32_t applied, uint32_t *to_apply, uint32_t *to_restore);

/* ============================ ★ 2026-09-18：「每波多刷绿的」的**队形槽位** ============================
 * 用户口径：「按照**几何形状**，每波多**一对**，或者多**一只**」⇒ 补刷的那些要**接在这一波队形外面**，
 * 而不是随便找个地方冒出来。卡侧每帧扫敌人、把**本波**已经观测到的出生点 x 坐标喂进来，本函数
 * 算出「接下来几格」的 x（纯函数 ⇒ 主机单测）。
 *
 * ★★ 2026-09-22（th10 #2「绿色妖精队列错误」）：槽位的锚点从 `|x|max` 改成**观测到的端点**
 *   （`xmin` / `xmax` 有符号），理由见 `.c` 里那段 —— 旧写法在「观测点全偏在一侧」的波里
 *   （TH18 一面道中的竖直列就是）会把补刷的那只甩到**屏幕另一侧**。一手依据 =
 *   TH10 `stage01.ecl` **循环之后紧跟一条** `ins_257("Ecl_GGirl00", %B, -16.0f, 20, 1000, 3)`
 *   （`%B` = 循环最后一格 **+间距**）⇒「排在**下一格**」才是复刻。
 *
 * 规则（`❓ 自定`；登记 `TUNING.md` §2.0h）：
 *   · `d` = 相邻 |x| 的最小正间距（观测不到 ⇒ `MOF_GREEN_EXTRA_SLOT_MIN`）；
 *   · `count = 1` ⇒ 补**人少的那一侧**（左右一样多 ⇒ 右侧）的**下一格**：右侧 = `xmax + d`、
 *     左侧 = `xmin − d`；
 *   · `count = 2` = 一对（`xmin − d` 与 `xmax + d`）；`count = 4 / 6` = 由内向外 `±2d / ±3d`……
 *     ★ 后一种形态是给「**整群绿妖精**」准备的：TH11 的实测是「一波要么 0 只、要么 5~80 只」
 *       （`engine/faith/th11/03-faith-drop-distribution.md` §3），所以 `count` 要能上到 8。
 * 返回写进 `out_x` 的个数（`out_max` 上限）。`n <= 0` ⇒ 返回 0（这一波还没看到任何妖精）。*/
#define MOF_GREEN_EXTRA_SLOT_MIN 40.0f
#define MOF_GREEN_EXTRA_SLOT_MAX 8
int mof_green_extra_slots(const float *x, int n, int count, float *out_x, int out_max);

/* ★★ 2026-09-18c（用户实跑口径）：「**妖精应当跟随那个队列才对**」——
 * 补刷的那只要**跟着同一波的妖精一起动**，做法 = 卡侧每帧把「目标妖精坐标 + **出生时定下的队形偏移**」
 * 写进它自己的 ECL float 槽（`%F2/%F3` = `enemy+0x14d0/+0x14d4`），子程序只负责 `movePosTime` 走过去。
 * 本函数就是那个偏移（纯算术 ⇒ 可离线单测）：`off = 补刷槽位 − 目标出生点`。*/
static inline void mof_green_extra_follow_offset(float slot_x, float slot_y, float tgt_x, float tgt_y,
                                                 float *off_x, float *off_y)
{
    if (off_x) *off_x = slot_x - tgt_x;
    if (off_y) *off_y = slot_y - tgt_y;
}

/* ============================ ★ 2026-09-18c：「妖精」的判据（用户口径，可离线单测） ============================
 * 用户原话：「不是**所有 for 循环**都加绿妖精，而是**妖精循环**加绿妖精」+「**三只大蝴蝶那里不应该有绿妖精！**」
 * ⇒ 「补刷绿妖精」只允许发生在**妖精波**里，而且**这一波只要出现别的种类**（大妖精/阴阳玉/幽灵/boss…）
 *   就**撤销**本次补刷（否则上一波妖精触发的补刷会落到它们的画面上 —— 实跑就这么被看到的）。
 *
 * 判据 = 这只敌机**长什么样**（`enemy.anm` 的扁平脚本号；引擎里拿不到"哪段 ECL 循环"，外观是唯一稳的
 * 机器可读身份）：妖精集 = `0–39`（A 族 + GirlB/GirlC）、`159–178`（C 族，ECL 零站点）、`184–203`（B 族）。
 * **不是**妖精的（因此不会触发、出现即撤销）：大妖精 `40–44 / 224–228`（64×64，实跑里用户叫它**大蝴蝶**）、
 * 阴阳玉 `Cir*` `45–80`、`Phantom*` `91–110`、以及所有 boss/中 boss 的脚本、与任何非 `enemy.anm` 的外观。*/
int mof_green_is_fairy(int32_t anm_src, int32_t anm_script);

/* ---- 内存块原语（`tbl` = sprite 表基址；都只碰 `tbl` 与 `backup`，不认引擎地址）---- */

/* 运行期同形体检：源/绿两支的第 i 项宽高必须逐条相等（不等 ⇒ 常量与真表对不上，别改）。*/
int mof_green_pair_sane(const uint8_t *tbl, const mof_green_pair_t *p);

/* 「已经变绿了吗」（源 12 项与绿 12 项逐字节相等 ⇒ 幂等闩锁）。*/
int mof_green_pair_is_green(const uint8_t *tbl, const mof_green_pair_t *p);

/* 第一次接管：把源的 12 项备份到 `backup`（= 本对的 `MOF_GREEN_PAIR_BYTES` 字节），再把绿的拷过去。*/
void mof_green_pair_swap(uint8_t *tbl, const mof_green_pair_t *p, uint8_t *backup);

/* 重放（`anm` 被重载 / 换关后表项被打回原色）：只覆盖，**不重取备份**（备份是原始内容）。*/
void mof_green_pair_apply(uint8_t *tbl, const mof_green_pair_t *p);

/* 还原（删卡 / 换局）：把备份写回去。*/
void mof_green_pair_restore(uint8_t *tbl, const mof_green_pair_t *p, const uint8_t *backup);

/* ============================ ★★★ 2026-09-25：**实例级「改妖精」**（owner 口径） ============================
 * owner（逐字）：「绿妖精我放弃了，**走改妖精路线**吧，改的数量就是**只有杂鱼绿色妖精**，记得和 th12 卡得
 *   **错开改，别改成同一只**」+「数量就按照**改一只（对）**就行，和 th12 基本一致」。
 * ⇒ 不再整组换 sprite 表（那样「蓝妖精全变绿」，owner 实跑当场否掉），而是**按波挑 1~2 只**
 *   把那一只的**外观脚本**换成同一形状的绿色变体（下面前三个纯函数就是这件事的判据）。
 *
 * 一手不变式（`mof_green_core.h` 头注释那张表）：**一个变体占 5 个脚本 + 12 个 sprite**，
 * 蓝→绿 = **脚本 +10 ⟺ sprite +24**（红→绿 +5 / 第四色→绿 −5）⇒「第 k 个 pose」保持一一对应。
 * 所以「把这只妖精变绿」= 让它的皮播 `dst_script + (当前蓝脚本 − src_script)`。*/
int32_t mof_green_green_script(const mof_green_pair_t *p, int32_t blue_script);
/* 反过来问：`script` 是不是这一行**已经变绿**的那 5 个之一（幂等闩锁）。*/
int     mof_green_is_green_script(const mof_green_pair_t *p, int32_t script);
/* 「镜像伙伴」判据（112 的星莲船载体用的同一对容差：`|x1+x2| ≤ 16`、`|y1−y2| ≤ 24`）——
 * owner 口径「改一只（对）」，那一**对**就是波里左右对称的那两只。*/
#define MOF_GREEN_SYM_XTOL 16.0f
#define MOF_GREEN_SYM_YTOL 24.0f
int     mof_green_mirror_ok(float x1, float y1, float x2, float y2);
/* 「本关能改的行」：`enabled` ∧ 关掩码命中（纯函数；卡侧用它决定扫不扫）。*/
int     mof_green_row_armed(const mof_green_pair_t *p, int32_t stage);

#endif /* MOF_GREEN_CORE_H */
