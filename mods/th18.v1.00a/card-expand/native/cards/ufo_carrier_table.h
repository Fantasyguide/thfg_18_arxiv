/* 由 native/tools/gen_ufo_carrier_table.py 生成 —— 别手改；改表请改那个脚本再跑一遍。
 * 生成命令：python3 mods/th18.v1.00a/card-expand/native/tools/gen_ufo_carrier_table.py
 *
 * 卡 112（星莲船）的 **UFO 令牌宿主表**：键 = 敌人「长什么样」的机器可读身份
 *   `N` = `anmSelect` 的选择子（`enemy+0x1494`）、`S` = `K==0` 那次 `anmSetMain`/`anmSetSprite`
 *   的主脚本（`enemy+0x1498`）—— 与 110 的大信仰点表（`mof_big_faith_table.h`）同一套判据。
 *
 * owner 2026-09-24：「th12 原版掉落飞碟的怪……只有**大蝴蝶**和**杂鱼妖精**（+ boss）；
 *   **得改谁能飞碟掉落，并且还得给掉的那个改帖图**」⇒ 表里的敌人才掉令牌，
 *   并且它们会被 C 侧换上 TH12 的皮肤（脚本 351/352 走位 + 353 头顶旋转飞碟）。
 *
 * 一手的宿主子程序（TH12，逐关）与「换哪张皮」的理由：见生成器文件头 + `TUNING.md` §2.0zzm。
 * 下方每行的注释 = 本行的 (N,S) 在 TH18 里由**哪些同名子程序**使用 + 精灵尺寸（量出来的）。
 */
#pragma once
#ifndef UFO_CARRIER_TABLE_H
#define UFO_CARRIER_TABLE_H
#include <stdint.h>

typedef enum {
    UFO_CARRIER_KIND_FAIRY = 0,   /* 换「杂鱼妖精」皮：脚本 352（32×32 四帧走位）*/
    UFO_CARRIER_KIND_BFLY  = 1,   /* 换「大蝴蝶」皮：脚本 351（64×64 四帧走位）*/
} ufo_carrier_kind_t;

typedef struct {
    int16_t  n;        /* anmSelect 选择子（enemy+0x1494）*/
    int16_t  s;        /* 主脚本（enemy+0x1498，K==0 的那次）*/
    uint8_t  kind;     /* ufo_carrier_kind_t */
    uint8_t  sprite_w; /* 主脚本第一张精灵的宽（仅供人读；分档就是靠它）*/
    uint8_t  sprite_h;
    uint8_t  glow;     /* ★ 宿主自己是不是辉光妖精（它那支脚本的 sprite ⊂ 220..267）
                        *   ⇒ 112 挑皮用：辉光妖精穿辉光黄，否则穿普通黄（owner 2026-10-03）*/
    int16_t  hp_min;   /* 创建点 hp 的最小值（owner 第 12 轮的分档判据：≤ 400 = 一碰就碎）*/
    int16_t  hp_max;
    int16_t  skin_scale_permille;  /* 皮的缩放（千分比）：宿主原始精灵 max(w,h) ÷ 皮的原始边长*/
    const char *who;   /* 一手出处（TH12 侧的子程序名）*/
    const char *stages;/* 在 TH18 的哪几关出现 */
} ufo_carrier_key_t;

/* ★ 表（按 (N,S) 升序；一条 = 一个"外观"）*/
static const ufo_carrier_key_t UFO_CARRIER_KEYS[] = {
    {    2,    0, UFO_CARRIER_KIND_FAIRY,  32,  32,  0,   100,   300,  1000, "TH18 蓝妖精", "st1/st2/st5/st6" },
    {    2,    5, UFO_CARRIER_KIND_FAIRY,  32,  32,  0,   100,   500,  1000, "TH18 妖精、TH18 红妖精", "st1/st2/st5/st6" },
    {    2,   10, UFO_CARRIER_KIND_FAIRY,  32,  32,  0,   120,   120,  1000, "TH18 蓝妖精", "st5" },
    {    2,   15, UFO_CARRIER_KIND_FAIRY,  32,  32,  0,   120,   500,  1000, "TH18 妖精、TH18 红妖精", "st5" },
    {    2,   40, UFO_CARRIER_KIND_BFLY ,  64,  64,  0,   600, 10000,  1000, "TH12 GirlC01、TH12 GirlD01、TH12 GirlD02、TH12 GirlD03", "st1/st3/st4/st5/st6/st7" },
    {    2,   91, UFO_CARRIER_KIND_FAIRY,  32,  32,  0,   120,   400,  1000, "TH12 Phantom01、TH12 Phantom01d、TH18 幻影妖精", "st1/st3/st4/st5/st6/st7" },
    {    2,   99, UFO_CARRIER_KIND_FAIRY,  32,  32,  0,   120,   400,  1000, "TH12 Phantom01、TH12 Phantom01c、TH12 Phantom01d、TH18 幻影妖精", "st1/st2/st3/st5/st7" },
    {    2,  103, UFO_CARRIER_KIND_FAIRY,  32,  32,  0,   300,   300,  1000, "TH18 幻影妖精", "st2/st3/st5/st7" },
    {    2,  184, UFO_CARRIER_KIND_FAIRY,  32,  32,  1,   100,   300,  1000, "TH18 蓝妖精", "st2/st3/st7" },
    {    2,  189, UFO_CARRIER_KIND_FAIRY,  32,  32,  1,   100,   500,  1000, "TH18 红妖精", "st2/st3/st7" },
    {    2,  224, UFO_CARRIER_KIND_BFLY ,  64,  64,  1,   800, 10000,  1000, "TH12 GirlD01、TH12 GirlD02、TH12 GirlD03", "st2/st7" },
};
#define UFO_CARRIER_KEY_COUNT (int)(sizeof(UFO_CARRIER_KEYS) / sizeof(UFO_CARRIER_KEYS[0]))

#endif  /* UFO_CARRIER_TABLE_H */
