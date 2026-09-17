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

#endif /* MOF_GREEN_CORE_H */
