/* 伟大之作（id 92）—— **主动卡**，致敬《Noita》的「伟大之作」（The Great Work）。
 *
 * ## owner 2026-10-03 逐字
 * 「noita卡：伟大之作：把所有敌人+弹幕变成金钱道具（类P点的那个）。冷却时间按照转换的弹幕数量和
 *   消灭敌人的血量等比例计算。敌人也按照剩余血量可以转换出多个金钱。如果是 boss 期间就会直接击破一个阶段。」
 *
 * ## 一次发动做四件事（逐条落点）
 * ① **弹幕 → 金钱**：扫引擎的子弹池（`CE_BM_BULLETS` 起 `CE_BM_BULLET_COUNT = 2000` 槽，
 *    `state ∈ {1,2}` = 活动），先记下每颗的位置，再逐颗 `Bullet__cancel(b, 0)` —— **mode 0 = 纯删除**
 *    （一手：0/3 不撒任何实体、1 会撒 type 9、4 会撒 type 9 + type 2），然后由我们自己在那些位置撒
 *    **item type 2 = 金钱道具**（一手 `engine/faith/th18/01-point-item-value.md` §3：收下 = `MONEY += 1`
 *    + `SCORE += 身价/10`；`0x446b00` 那条路就是它）。走 mode 0 还顺带避开两件事：不推那条 RNG、
 *    不搅「本帧消弹颗数」的共用闩锁（见 `engine.h` 的 `CE_FN_BULLET_CANCEL` 注）。
 * ② **敌人 → 金钱**：每个可锁定的敌人按**剩余血量**（`CE_ENEMY_HP_CUR`）换 `gw_money_for_hp()` 枚金钱，
 *    再用一发「等于它剩余血量」的伤害源把它打死（走引擎自己的死亡路径 ⇒ 分数 / 死亡演出 / 掉落照常）。
 * ③ **boss 期间 = 直接击破一个阶段**：带 `CE_ENEMY_FLAG_BOSS`（flags bit23，一手 = ECL `setBoss`）的敌人
 *    不撒金钱、改成吃一发「等于它当前阶段剩余血量」的伤害 ⇒ 正好把当前阶段打掉、进下一段。
 *    ⚠ 伤害要 `ce_raise_damage_cap()` 抬当帧上限，否则被引擎的「每敌每帧上限」削掉。
 * ④ **冷却按转换量算**：`gw_recharge(弹幕数, Σ血量)`（纯逻辑 + 主机单测）⇒ 直接写这张卡的充能三元组
 *    （`card+0x34/+0x38/+0x3c` = zTimer 的 prev/cur/cur_f，`card+0x48` = 总时长；照引擎写充能那一支）。
 *
 * ## ❓ 自定（登记 `TUNING.md` §2.10）
 * 两条公式的系数（每 20 血 1 枚、单敌上限 40 枚、一次最多 256 颗弹、冷却底价/单价/上下限）、
 * 金钱撒落的散开角度、以及「敌人的正常掉落**不压制**」（要压制得走掉落倍率层，见 TUNING 那条）。 */
#include "sdk.h"
#include "great_work_core.h"
#include "faith.h"      /* ★ 通道②：共享信仰点池（107 樱点 / 110-111 信仰点）的清扫接口 */

#define GW_ITEM_MONEY_DEFAULT 1        /* ❓ 自定：其余可收集道具统一 1 枚金钱 */
#define GW_ITEM_TYPE_MONEY_CARD 0x12   /* 金钱**卡**（道具表 0x12 行 → card_id 5）*/
#define GW_ITEM_TYPE_CARD_LO    0x10   /* 卡道具区间：0x10 残碎卡 / 0x11 B 碎卡 / 0x12 钱卡 / 0x13 */
#define GW_ITEM_TYPE_CARD_HI    0x13
#define GW_ITEM_MONEY_BOMB    1        /* ❓ 自定：**原生 type 7「整颗 Bomb」道具** = 一颗炸弹 ⇒ 1 枚。
                                        *   ⚠ 112 的**压缩包**（`ufo_drop_t` 的袋子）不在这里 —— 它带账
                                        *   （`cnt_p`/`cnt_money`），由 112 自己的清扫钩子按「吸收量」换钱 */
#define GW_FLASH_LAYER        19       /* 同脚本 397 的 `layer(19)`（世界层最上、HUD 之下）*/
/* ★★ 2026-10-03 owner 的**演出口径**（三轮叠加，最终版）：
 *   ① 「照着原生卡**丰聪耳神子**的卡写【やんごとなき威光】，中心选取到释放点」；
 *   ② 「乘法滤镜和闪光能不能变成**圆形扩张范围**？」；
 *   ③ 「**威光的光线最先出来，几乎同时圆盘长出来开始**，但是由于慢所以一开始相对注意不到」＋
 *      「**先慢再快**，顶点的时候加法和乘法都变成**纯白色**」＋「顶点也是**慢慢**变成纯白，不是瞬间」＋
 *      「然后**全屏死亡**」。
 *   ⇒ 三个脚本按原生 `CardMiko` 的配方照抄（`native/build/tex-ability/spec.txt` 里有原生
 *      script47/48/49 的反编译）：
 *      · **光线**：脚本 400（总控，照原生 **script47**：20 批 × 5 支、每 2 帧一批、92 帧后删）→
 *        每支 = 脚本 399（照原生 **script48**：随机角度 + 从释放点往外长 + 加算，**颜色改金** ——
 *        原生那支是 `color(128,128,255)`/`color2(255,32,255)` **蓝紫**）；
 *      · **圆盘**：脚本 397（加法闪光）/ 398（乘法滤镜）—— 都是 `drawPoly(1.0f,32)` 实心盘
 *        （原生 **script49** 同配方），0→24 帧**慢**长到 90 半径、24→34 帧**快**冲到 640
 *        （罩满游玩区），颜色一路**慢慢**转成纯白 ⇒ **顶点 = 纯白 + 满 alpha = 全屏白（"全屏死亡"）**。
 *   三者都在释放点、同一帧起（"几乎同时"）；光线在层 6（原生层号），圆盘在层 19。*/
#define GW_RAYS_SCRIPT        400      /* 金色威光总控（照原生 script47）*/
#define GW_RAYS_LAYER         6        /* 原生层号（script47/48 的 layer(6)）*/
/* 三个脚本的自然寿命（比脚本里的 `delete()` 略短 —— **必须在 VM 还活着时清句柄**，
 *  否则 VM 池把 id 回收给别人之后，我们会去动别人的 VM）。*/
#define GW_RAYS_LIFE          92       /* 脚本 400：92 帧后自己删 */
#define GW_TINT_LIFE          40       /* 脚本 398：34 顶点 + 8 停留 → 42 删 */
#define GW_FLASH_LIFE         74       /* 脚本 397：34 顶点 + 8 停留 + 6 快落 + 30 长尾 → 78 删 */
#define GW_CARD_SE          0x4d       /* 发动音：0x4d se_release（与杰拉 / 鞘翅重锤同）*/
#define GW_ENEMY_HIT_H      44.0f

typedef struct {
    uint32_t fired;         /* 发动过几次（诊断）*/
    int32_t  last_frames;   /* 上一次算出来的冷却帧数（诊断）*/
    int32_t  last_bullets;
    int64_t  last_hp;
    int32_t  last_money;
    int32_t  last_phase_breaks;
    /* ★★ 2026-10-03 兜底（owner 实测「黄金特效变成永久的了」之后加的）：两个演出 VM 的句柄。
     *   它们本来是**自毁**的（脚本 397/398 末尾 `delete()`），但那次我误在滤镜脚本里写了 `stop()`
     *   ⇒ 自毁永远到不了 ⇒ 金色永久留在屏幕上 ✗。**脚本已经修好**，这里再加一道保险：
     *   换关 / 局末 / 删卡时把它们收掉（`delete()` 幂等；被收过的 id 再收无害）。*/
    uint32_t fx_flash_id;
    uint32_t fx_tint_id;       /* 乘法金色滤镜（脚本 398）的 VM 句柄 */
    uint32_t fx_rays_id;       /* ★ 金色威光总控（脚本 400）的 VM 句柄 */
    int32_t  fx_rays_left;     /* 它还活着的帧数（脚本自己会删；我们在**它还活着**时清句柄）*/
    int32_t  fx_tint_left;     /* 乘法滤镜（脚本 398）剩余帧数 */
    int32_t  fx_flash_left;    /* 加法闪光（脚本 397）剩余帧数 */
} gw_card_t;

_Static_assert(sizeof(gw_card_t) <= 496, "great_work: private state overflows CE_STATE_BYTES");

typedef void (__attribute__((thiscall)) *gw_fn_bullet_cancel_t)(void *bullet, int mode);

/* 静态缓冲：不占引擎的栈（`on_activate` 跑在引擎的调用栈里）。本卡只在主线程、且 on_activate
 * 不会重入 ⇒ 文件级 static 是安全的（与 108/113 的池同一个口径）。*/
static float       s_bullet_pos[GW_BULLET_MAX][3];
static uint32_t    s_bullet_rnd[GW_BULLET_MAX];

/* ★ 自机也变黄金（owner 2026-10-03 的平衡）：调引擎自己的 `Player__die`。
 *   一手：`Player__die` = `0x45d3a0`（thiscall(Player*)、无栈参、裸 ret；内部干「状态→4 + 死亡特效 +
 *   8 帧决死窗 + 末尾广播卡虚表 +0x10」，**不扣命** —— 扣命在 `commit_death_and_enter_state2`），
 *   调用先例 = `cards/youkai.c` 的 `YK_FN_PLAYER_DIE` / `cards/trance.c` 的 `TRANCE_FN_PLAYER_DIE`。*/
#define GW_FN_PLAYER_DIE 0x45d3a0u
typedef void (__attribute__((thiscall)) *gw_fn_player_die_t)(void *player);
/* `Player__commit_death_and_enter_state2` = `0x45d090`（thiscall(Player*)）：**真正扣命 / 记 miss /
 * 状态→2（死亡动画）**。全 exe 只有 1 个调用点 `0x45c135`（状态 4 的 tick，决死窗走完/没救回时）。
 * owner 2026-10-03：「**我觉得不应该决死**」⇒ 我们走"开窗后**立刻**结算"，等价于窗口当场到期
 * （跳掉那 8 帧 + 跳掉按 X 的机会），但**开窗那一调照走** —— 引擎在 `Player__die` 里会广播卡虚表
 * +0x10（救援层 / 各类保护就挂在那儿）。*/
#define GW_FN_COMMIT_DEATH 0x45d090u
typedef void (__attribute__((thiscall)) *gw_fn_commit_death_t)(void *player);

/* 把这张卡的充能**当帧改成** `frames`（照引擎写充能那一支：zTimer 的 prev/cur/cur_f + 总时长）。*/
static void gw_set_recharge(ce_card_t *c, int32_t frames)
{
    uint8_t *p = (uint8_t *)c;
    if (!p || frames < 0) return;
    *(int32_t *)(p + CE_CARD_RECHARGE_TIMER)     = frames - 1;      /* +0x34 prev */
    *(int32_t *)(p + CE_CARD_RECHARGE_TIMER + 4) = frames;          /* +0x38 cur  */
    *(float   *)(p + CE_CARD_RECHARGE_TIMER + 8) = (float)frames;   /* +0x3c cur_f*/
    *(int32_t *)(p + CE_CARD_RECHARGE_TIME)      = frames;          /* +0x48 总时长 */
}


/* ★★ 2026-10-03 owner：「你得把**场上的所有可收集道具也变成金钱**，比如季节道具、用点。
 *   **特别地**：112 压缩包 = **等量于吸收量**的金钱；112 飞碟 token 与 117 动物灵 token 都变成 **10 个**。」
 *
 *   ⇒ 按 88 杰拉那套**三段通道**做（同一张图景：一半"掉落物"根本不在引擎池里）：
 *     ① **引擎道具池**（本函数扫）：P / 金钱 / 命碎 / 符卡碎 / 分数点 / 卡道具 0x10–0x13 /
 *        别的卡用 `ce_spawn_item()` 撒的一切（含 112 的**整颗 B = 压缩包**，type 7）——
 *        做法：**把道具的 type 改成 2（金钱）**（引擎是**收的那一刻**读 type 决定给什么，
 *        一手 `0x44625f mov edx,[edi+0xc78]` ⇒ 改这里就够，而且原道具照走引擎自己的收集路径
 *        ⇒ 不会留孤儿 VM）＋ 差多少补几枚 ⇒ 合计正好 `gw_item_money_value(type)`；
 *     ② **共享信仰点池**（107/110/111）：走 `faith.h` 的公开 API（见 `great_work.c` 的调用点）；
 *     ③ **各卡私有池**（108 刻符 / 109 灵力道具 / 112 飞碟 token 与压缩包 / 116 季节道具 /
 *        117 动物灵 token …）：`ce_sweep_bump()` 一次，那些卡在自己的 tick 里看到代数变了就
 *        把自己那批清成钱（生产者侧见 `sdk_core.h` 的清扫闸段）。*/
static int32_t gw_item_money_value(int32_t type)
{
    if (type == 7) return GW_ITEM_MONEY_BOMB;      /* 原生「整颗 Bomb」= 112 的压缩包 */
    return GW_ITEM_MONEY_DEFAULT;
}

static int gw_take_items(int32_t *money)
{
    uint8_t *mgr = CE_ITEM_MANAGER();
    int i, n = 0, pocket = 0;

    if (!mgr) return 0;
    for (i = 0; i < (int)CE_ITEM_SLOTS; ++i) {
        uint8_t *it = mgr + CE_ITEM_POOL_OFF + (size_t)i * CE_ITEM_STRIDE;
        int32_t st = *(int32_t *)(it + CE_ITEM_STATE);
        int32_t type, val, k;
        if (st == 0) continue;                          /* 空槽 / 已删 */
        type = *(int32_t *)(it + CE_ITEM_TYPE);
        /* ★★ owner 2026-10-03：「**掉钱卡的实体不需要转换 —— 它已经是钱了**；其他的**残碎卡 B 碎卡
         *   转换成钱卡**」。两个「已经是钱」的都不动：
         *     · type 2  = 金钱**道具**（金币点，`MONEY += 1`）；
         *     · type 0x12 = 金钱**卡**（TH18 道具表 `0x4b4020` 的 0x12 行 → card_id 5；见 AUDIT §BE
         *       的 AL12c：「type 2 = 金钱点；type 0x12 = 金钱卡」）
         *   其余卡道具（0x10 残碎卡 / 0x11 B 碎卡 / 0x13）**改成 0x12 钱卡**（卡道具在收集时才
         *   `allocate_new_card(…, card_id, 0)` ⇒ 改 type 就换了那张卡）；非卡道具一律变金钱**道具**
         *   （type 2），值 > 1 的（112 压缩包）再补几枚。*/
        if (type == 2 || type == GW_ITEM_TYPE_MONEY_CARD) continue;
        if (type >= GW_ITEM_TYPE_CARD_LO && type <= GW_ITEM_TYPE_CARD_HI) {
            *(int32_t *)(it + CE_ITEM_TYPE) = GW_ITEM_TYPE_MONEY_CARD;   /* 卡道具 ⇒ 钱卡（不改枚数）*/
            ++(*money);
            ++n;
            continue;
        }
        val = gw_item_money_value(type);
        *(int32_t *)(it + CE_ITEM_TYPE) = 2;            /* ★ 原道具变金钱道具（收的那一刻才读 type）*/
        ++(*money);
        ++n;
        for (k = 1; k < val; ++k) {                     /* 差几枚就在原地补几枚 */
            float p[3];
            uint32_t r = ce_rand();
            p[0] = *(float *)(it + CE_ITEM_POS);
            p[1] = *(float *)(it + CE_ITEM_POS + 4);
            p[2] = *(float *)(it + CE_ITEM_POS + 8);
            if (ce_spawn_item(2u, p, ((float)(int32_t)(r & 0xffffu) / 32768.0f - 1.0f) * 1.5707964f, 1.6f))
                ++(*money);
            else { ce_money_pocket(1); ++(*money); ++pocket; }
        }
    }
    if (pocket) ce_log("great_work: 池满 ⇒ %d 枚（道具那路）直接进腰包", pocket);
    return n;
}


/* ① 弹幕 → 金钱。返回转换掉的弹幕数。*/
static int gw_take_bullets(int32_t *out_money)
{
    uint8_t *bm = CE_BULLET_MGR();
    float    (*pos)[3] = s_bullet_pos;
    uint32_t *rnd = s_bullet_rnd;
    int      n = 0, spawned = 0, i;
    uint32_t seed = ce_rand();

    *out_money = 0;
    if (!bm) return 0;

    for (i = 0; i < CE_BM_BULLET_COUNT; ++i) {
        uint8_t *b = bm + CE_BM_BULLETS + (size_t)i * CE_BULLET_STRIDE;
        int32_t st = (int32_t)*(uint16_t *)(b + CE_BULLET_STATE);
        if (st != 1 && st != 2) continue;                 /* 0 空槽 / 3·4 已在收尾 */
        seed = seed * 1103515245u + 12345u;               /* our LCG（不用引擎那两支走 x87 的）*/
        if (n < GW_BULLET_MAX) {
            pos[n][0] = *(float *)(b + CE_BULLET_POS);
            pos[n][1] = *(float *)(b + CE_BULLET_POS + 4);
            pos[n][2] = *(float *)(b + CE_BULLET_POS + 8);
            rnd[n] = seed;
            ++n;
        }
        ((gw_fn_bullet_cancel_t)CE_FN_BULLET_CANCEL)(b, 0);   /* ★ mode 0 = 纯删除（不撒原生道具）*/
    }

    for (i = 0; i < n; ++i) {                             /* 先全删完再撒，避免撒出来的被自己的扫帚扫掉 */
        float ang = -1.5707964f;
        if (GW_MONEY_ANGLE_SPREAD) {
            /* 用我们自己的 LCG 抽一个 [−π, π) 的方向（replay-safe：不碰引擎 RNG）*/
            uint32_t r = rnd[i];
            ang = ((float)(int32_t)(r & 0xffffu) / 32768.0f - 1.0f) * 3.14159265f;
        }
        if (ce_spawn_item(2u, pos[i], ang, 1.6f)) ++spawned;   /* type 2 = 金钱道具 */
    }
    *out_money = spawned;
    return n;
}

/* ②③ 敌人 → 金钱 / boss → 破一阶段。
 * ⚠ 伤害上限是**本帧总伤害**（`player+0x47984`，`enm_compute_damage_sources 0x45f0f0` 用它钳本帧总和）
 *   ⇒ 必须先统计完再一次性抬，否则逐只抬会互相盖掉（第二只看到的还是第一只的上限）。*/
static int gw_take_enemies(int64_t *hp_sum, int32_t *money, int32_t *breaks)
{
    uint8_t *em = CE_ENEMY_MGR();
    uint8_t *e;
    int n = 0, dead = 0, no_dmg = 0, pocket = 0;

    *hp_sum = 0;
    *money = 0;
    *breaks = 0;
    if (!em) return 0;

    /* ★★ 2026-10-03 owner 第二次实测：「**？还没死**」—— 根因是**杀法**，不是过滤：
     *   原来靠 `ce_damage_rect()` 出一条伤害记录，而引擎的伤害路径有**两道闸**
     *   （`0x42f2b9` 的免伤位 bit4、`0x42f2c2` 的 `enemy+0x6320` 无敌计时器）
     *   ⇒ 中妖精那种「能打但此刻带无敌/免伤」的目标**钱掉了、人没死** ✗。
     *   一手：死亡判定在 `0x42f392` —— `life.current <= 0 && !(flags_lo & 0x80)` → `Enemy__die 0x430240`
     *   （见 `engine/enemy/th18/06-enemy-inert-flag.md` §⑦）。两道闸只掐 `life` 的**扣除**，不掐死亡判定。
     *   ⇒ **直接把 `CE_ENEMY_HP_CUR` 写 0**：必然进死亡路径（掉落 / ECL 段推进照常），且绕过两道闸。
     *   boss 同样写 0 ⇒ 引擎按段推进（= owner 要的「直接击破一个阶段」）。
     *   ⚠ 因此**不再需要**伤害记录 / 抬每帧伤害上限（`GW_ENEMY_HIT_W` / `GW_LETHAL_SLACK` / `s_target[]`
     *     都已删掉）—— 也不再有两遍遍历（不必缓存敌人指针，符合同一帧内只读的纪律）。*/
    CE_ENEMY_FOREACH(em, e) {
        int32_t hp;
        uint32_t flags;
        float center[3];
        if (!e) continue;
        if (n >= GW_ENEMY_MAX) break;
        hp = *(int32_t *)(e + CE_ENEMY_HP_CUR);
        if (hp <= 0) { ++dead; continue; }                       /* 已死 / 正在收 */
        flags = *(uint32_t *)(e + CE_ENEMY_FLAGS);
        if (flags & CE_ENEMY_FLAG_NO_DAMAGE) ++no_dmg;           /* 免伤位：照样杀，只记一笔 */
        center[0] = ce_enemy_pos(e)[0];
        center[1] = ce_enemy_pos(e)[1];
        center[2] = 0.0f;
        *hp_sum += hp;

        if (flags & CE_ENEMY_FLAG_BOSS) {                        /* boss：收掉这一阶段（不撒钱）*/
            ++(*breaks);
        } else {                                                 /* 普通敌人：按剩余血量换金钱 */
            int32_t k, cnt = gw_money_for_hp(hp);
            for (k = 0; k < cnt; ++k) {
                float p[3];
                float ang = -1.5707964f;
                if (GW_MONEY_ANGLE_SPREAD) {
                    uint32_t r = ce_rand();
                    ang = ((float)(int32_t)(r & 0xffffu) / 32768.0f - 1.0f) * 3.14159265f;
                }
                p[0] = center[0];
                p[1] = center[1];
                p[2] = 0.0f;
                if (ce_spawn_item(2u, p, ang, 1.8f)) ++(*money);
                else { ce_money_pocket(1); ++(*money); ++pocket; }   /* 池满 ⇒ 直接进腰包 */
            }
        }
        *(int32_t *)(e + CE_ENEMY_HP_CUR) = 0;                   /* ★ 唯一的杀法 */
        ++n;
    }
    if (pocket)
        ce_log("great_work: 池满 ⇒ %d 枚金钱直接进腰包（MONEY +%d）", pocket, pocket);
    if (no_dmg)
        ce_log("great_work: 其中 %d 只带免伤位 —— 照写 life = 0 杀掉（不走伤害闸）", no_dmg);
    if (n == 0 && dead)
        ce_log("great_work: 没可转的敌人（已死/收尾 %d）", dead);
    return n;
}

static int on_activate(ce_card_t *c)
{
    gw_card_t *s = ce_state(c, gw_card_t);
    int64_t hp_sum = 0;
    float release[3] = { 0.0f, 224.0f, 0.0f };
    int32_t money = 0, breaks = 0, bullets, frames, enemies, items;
    int32_t faith_money = 0;
    int faith_n;
    float px = ce_player_x();

    if (!s) return 0;

    bullets = gw_take_bullets(&money);
    enemies = gw_take_enemies(&hp_sum, &money, &breaks);
    items = gw_take_items(&money);                        /* ★ 通道①：引擎道具池全清成钱 */
    /* ★ 通道②：**共享信仰点池**（107 樱点 / 110-111 信仰点·绿点）——
     *   owner 2026-10-03：「110-111 得响应清扫，107 是樱点，也得响应清扫」。
     *   池是共享的 ⇒ 清扫做在 `faith.c` 里一份，三张卡一起响应（不必各自再挂）。
     *   `faith_n` = 换掉的颗数（进「有没有东西可转」的判据）；`faith_money` 已进钱包（撒道具 / `ce_money_pocket`）。*/
    faith_n = faith_sweep_to_money(&faith_money);

    /* ★ 通道③：让**有私有池的卡**在自己的 tick 里把场上那批也清成钱（代数闸，见 sdk_core.h）。
     *   接了的：108 刻符 / 109 灵力道具 / 112 飞碟 token / 116 季节道具 / 117 动物灵 token。
     *   ★ 通道②（共享信仰点池）已在上面直接清掉了（`faith_sweep_to_money()`）—— 那一池是共享的，
     *   做在层里一份、三张卡（107/110/111）一起响应，不需要走代数闸。*/
    if (items || bullets || enemies || faith_n)
        ce_sweep_bump();

    if (bullets == 0 && enemies == 0 && items == 0 && faith_n == 0)   /* 空场：不烧充能 */
        return ce_refuse("great_work", "nothing to convert (no live bullet, no enemy, no item, no faith dot)");

    frames = gw_recharge(bullets, hp_sum);
    gw_set_recharge(c, frames);                           /* ④ 冷却按转换量等比例 */

    ++s->fired;
    s->last_frames = frames;
    s->last_bullets = bullets;
    s->last_hp = hp_sum;
    s->last_money = money;
    s->last_phase_breaks = breaks;
    ce_play_sound(GW_CARD_SE, px);

    /* ★ 释放点 = **发动那一刻的自机坐标**（owner：「中心就选取到释放点」；原版 `CardMiko` 也是这么取，
     *   `0x40e5c0` 的 `movq xmm0,[player+0x620]`）。下面三个演出共用它当中心。*/
    {
        const float *pp = ce_player_pos();
        release[0] = pp ? pp[0] : 0.0f;
        release[1] = pp ? pp[1] : 224.0f;
        release[2] = 0.0f;
    }

    /* 金色演出（同一帧起、都以释放点为心，owner 的"几乎同时"）：
     *   · 光线总控（脚本 400，层 6）—— 最先出来的那 100 支金色光线（总控自己 92 帧后删，
     *     子 VM 各自 90 帧自毁）；
     *   · 圆形扩张的乘法金色滤镜（脚本 398，层 19）与加法金色闪光（脚本 397，层 19）
     *     —— 慢起、先慢再快，顶点（≈34 帧）一起变成纯白 ⇒ 全屏白。*/
    s->fx_rays_id = ce_anm_spawn(CE_ABILITY_ANM(), GW_RAYS_SCRIPT, GW_RAYS_LAYER);
    if (s->fx_rays_id) {
        ce_anm_set_pos(s->fx_rays_id, release[0], release[1], release[2]);
        s->fx_rays_left = GW_RAYS_LIFE;
    }
    s->fx_tint_id = ce_anm_spawn(CE_ABILITY_ANM(), CE_ANM_ABILITY_SCRIPT_GREAT_WORK_TINT, GW_FLASH_LAYER);
    if (s->fx_tint_id) {
        ce_anm_set_pos(s->fx_tint_id, release[0], release[1], release[2]);
        s->fx_tint_left = GW_TINT_LIFE;
    }
    s->fx_flash_id = ce_anm_spawn(CE_ABILITY_ANM(), CE_ANM_ABILITY_SCRIPT_GREAT_WORK_FLASH, GW_FLASH_LAYER);
    if (s->fx_flash_id) {
        ce_anm_set_pos(s->fx_flash_id, release[0], release[1], release[2]);
        s->fx_flash_left = GW_FLASH_LIFE;
    }

    /* ★★ 2026-10-03 owner 的**平衡**：「伟大之作原作是会把**自机也变成黄金**，那就是**再减少冷却**，
     *   但是**会把我也杀死**」——**除非我处于无敌状态**。
     *   ⇒ 转换做完之后调引擎自己的 `Player__die`（thiscall(Player*)；先例 `youkai.c`/`trance.c`），
     *     走的是**原生 MISS 链**（状态→4 决死窗 → 没救回才 commit：扣命/炸弹归 3/掉一档），
     *     所以还留了 8 帧决死窗（按 X 能救回来 —— owner 没要求秒杀，先按原生的来）。
     *   ⇒ 无敌中（复活 280 帧 / 决死救回 60 / 炸弹各自那段 —— `player+0x47774`）**不杀**：
     *     否则"刚复活就用一次"会被自己的卡秒掉，等于把无敌帧吃掉了。
     *   ⚠ 只在**真的转换了东西**之后才走到这里（空场在上面 `ce_refuse` 早退了）⇒ 拒绝时不自杀。*/
    {
        ce_timer_t *inv = CE_PLAYER_INVULN();
        uint8_t *pl = CE_PLAYER();
        if (pl && (!inv || inv->cur <= 0)) {
            ((gw_fn_player_die_t)(uintptr_t)GW_FN_PLAYER_DIE)(pl);
            /* ★ 不给决死窗：`Player__die` 只是把状态推到 **4（决死窗）** 并广播卡虚表 +0x10（救援/保护
             *   就挂在那里）；**还停在 4** 就立刻结算（= 窗口当场到期，按 X 也来不及）。
             *   若这一调之后状态已经不是 4（被救援层/保护接住了）⇒ **不补刀**，尊重那道保护
             *   —— owner：「那些什么**咆哮状态**之类的**可以挡**」（咆哮的无敌 = `set_invuln(60)`
             *   写的就是 `player+0x47774`，上面那道 `inv->cur` 门已经先挡了一层）。*/
            if (*(int32_t *)(pl + CE_PLAYER_STATE) == 4) {
                ((gw_fn_commit_death_t)(uintptr_t)GW_FN_COMMIT_DEATH)(pl);
                ce_log("great_work: 自机也变成黄金 —— 开窗后**立刻结算**（无决死机会）");
            } else {
                ce_log("great_work: 自机也变成黄金 —— 但被保护/救援接住了（状态 %d）⇒ 不补刀",
                       *(int32_t *)(pl + CE_PLAYER_STATE));
            }
        } else {
            ce_log("great_work: 自机在无敌中 ⇒ 豁免这一死（owner 口径）");
        }
    }

    ce_log("great_work: converted %d bullets + %d enemies + %d items + %d faith dots "
           "(%d from faith) (hp %lld) -> %d money items, "
           "%d boss phase(s) broken; recharge %d frames (%.1f s)",
           bullets, enemies, items, faith_n, (int)faith_money, (long long)hp_sum, money, breaks,
           frames, (double)frames / 60.0);
    return 0;
}

/* ★ 演出 VM 的兜底收尾（脚本已自毁，这里防"脚本卡住"那种事故 —— 见 `fx_flash_id` 的注）。
 * 换关 / 局末 / 删卡三条路都走它；`ce_anm_delete` 幂等，句柄清零后再调无害。*/
static void gw_fx_kill(ce_card_t *c)
{
    gw_card_t *s = ce_state(c, gw_card_t);
    if (!s) return;
    if (s->fx_rays_id)  { ce_anm_delete(s->fx_rays_id);  s->fx_rays_id = 0;  s->fx_rays_left = 0; }
    if (s->fx_tint_id)  { ce_anm_delete(s->fx_tint_id);  s->fx_tint_id = 0;  s->fx_tint_left = 0; }
    if (s->fx_flash_id) { ce_anm_delete(s->fx_flash_id); s->fx_flash_id = 0; s->fx_flash_left = 0; }
}

/* ★★ owner 2026-10-03 两条补充：
 *   ① 「**全屏死亡的时候是光最强的时候**」—— 397 的 alpha 与半径都在第 34 帧同时到顶
 *      （alpha 255 + 半径 640 + 纯白）⇒ 最亮那一下正好就是罩满全屏那一下（脚本里已对齐）；
 *   ② 「那些放射光线、圆盘中心**还是跟着自机可以移动的**」—— 每帧把三个演出 VM 贴回自机
 *      （**原版 `CardMiko::__on_tick_2` `0x40e3d0` 就是每帧 `0x488b40(id)` 取 VM 再写
 *      `player+0x620`，我们照抄**）。光线总控跟着自机 ⇒ 它每 2 帧 `scriptNew` 出去的子光线
 *      就从**当时**的自机位置射出（已射出的那几支留在原地，与原版一致）。
 *   ⚠ 句柄清理：脚本自己 `delete()`，这里只把句柄清 0（**不按 id 删**）—— 清的时刻都取在
 *     脚本寿命**之内**（GW_*_LIFE 略小于脚本的 delete 帧），所以绝不会去动被池子回收后
 *      落到别人头上的同号 VM。三条生命周期（换关/局末/删卡）那条路才真删（那时它们一定还在窗口内）。*/
static int on_tick_2(ce_card_t *c)
{
    gw_card_t *s = ce_state(c, gw_card_t);
    const float *pp;
    if (!s) return 0;
    pp = ce_player_pos();
    if (pp) {
        if (s->fx_rays_id)  ce_anm_set_pos(s->fx_rays_id,  pp[0], pp[1], 0.0f);
        if (s->fx_tint_id)  ce_anm_set_pos(s->fx_tint_id,  pp[0], pp[1], 0.0f);
        if (s->fx_flash_id) ce_anm_set_pos(s->fx_flash_id, pp[0], pp[1], 0.0f);
    }
    if (s->fx_rays_id  && --s->fx_rays_left  <= 0) s->fx_rays_id  = 0;
    if (s->fx_tint_id  && --s->fx_tint_left  <= 0) s->fx_tint_id  = 0;
    if (s->fx_flash_id && --s->fx_flash_left <= 0) s->fx_flash_id = 0;
    return 0;
}

static void on_stage_start(ce_card_t *c) { gw_fx_kill(c); }
static void on_run_reset(ce_card_t *c)   { gw_fx_kill(c); }
static void on_delete(ce_card_t *c)      { gw_fx_kill(c); }

CE_CARD(92, .active_recharge = GW_BASE, .on_activate = on_activate, .on_tick_2 = on_tick_2,
            .on_stage_start = on_stage_start, .on_run_reset = on_run_reset, .on_delete = on_delete);
