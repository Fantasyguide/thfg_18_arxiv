/* test200.c —— **编号 200 的测试卡**：专门用来把「id > 127」的全链路一次走通。
 *
 * 为什么要有（2026-09-19）：新卡上限历史上被两处符号界压到 127 / 71 ——
 *   ① 图鉴条目数的两处**循环上界** `cmp r,imm8`（符号扩展 ⇒ imm8 ≥ 0x80 变负 ⇒ 循环零圈、
 *      图鉴整表空掉，AUDIT §AG 的症状链）。2026-09-19 已**原地换编码**成无符号字节比较
 *      （`sites.py` 的 `MENU_COUNT_BCMP`，上界 127 → 255），这条墙没了；
 *   ② AUDIT §Z11 / §G7 那条「编成 deck 槽把 id > 0x7f 当负值剔除」的现象（当时表行只有 58，
 *      分配器跳转表 `th18:0x412dac` 也就 58 项 ⇒ 越界读；现在表行与跳转表都是 `CE_MAX_ROWS`
 *      = 255 项，越界那条因不存在了，但**墙位当年没钉死**，仍标 🟡）。
 * 光在文档里推不算数，最省事的复现办法就是**真放一张 id = 200 的卡**：装进卡组打一局，
 * 看它能不能进图鉴、能不能装进槽、效果生不生效。这就是这张卡的全部用途。
 *
 * 效果（故意做到最小、最一眼可辨）：**自机弹伤害 ×2**（`bullet_damage_permille = CE_DMG_UNIT*2`）。
 *   走 SDK 的伤害层（AUDIT §AA，`ce_dmg_set` 聚合后由 id 最小的参与者落笔一次），
 *   不自己写 `bullet+0x9c` ⇒ 不会与别的卡抢同一条链。
 *   确认方式：同一关同一敌人，装上它明显更快打死（或者看日志的 ctor 行）。
 *
 * `ctor` 里留一行日志：`test200: loaded ...` ⇒ **不用进游戏**也能从 thcrap 日志确认它被装载了
 *   （载入 = 存档卡组 / 商店 / 图鉴任一路径碰到它都会 allocate）。
 *
 * 资源与出场：
 *   · 卡图借**原生** abcard entry 4/5（id 0「空白卡」那一对 —— 一眼看得出是占位） —— 不在 `assets/cards/ORDER.txt` 里的卡只能用原生索引
 *     （`assets/build_abcard.py` 的 `check_cards_js`），所以这张卡**不需要新美术**、不占 entry 配额。
 *   · `initial_unlocked: 1` ⇒ 一开局就在图鉴与商店里可见；要直接进卡组就写 `mods\start_deck.js`
 *     （`{"mode":"exact","cap":3,"deck":[200,113,110]}`，改完删掉同目录 `start_deck.applied` 或改动任一字节）。
 *
 * ⚠ **测试卡**：正式发布前挪走（图鉴 / 卡组条目与商店池是给真卡留的；`CAPACITY.md`）。
 *   删它 = 删本文件 + 删 `patch/th18/cards.js` 的 "200" 条目（两处一起删，别只删一边：
 *   cards.js 有 id 但没行为 = 一张无行为的空卡，不会报错，只会让人误判测试结果）。*/
#include "sdk.h"

static int ctor(ce_card_t *c)
{
    ce_log("test200: loaded — id=%u (>127 test card), self-shot damage x2", ce_card_id(c));
    return 0;                       /* 0 = 留在卡组里（返回 1 才是「即时卡：当场销毁」）*/
}

CE_CARD(200,
        .ctor = ctor,
        .bullet_damage_permille = CE_DMG_UNIT * 2);
