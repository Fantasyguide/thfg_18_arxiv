/* youkai_core.h —— 108「永夜抄」妖机体 / 使魔的**纯逻辑**层。
 *
 * 只 <stdint.h>，不碰引擎、不进 DLL 的断点路径 ⇒ 主机 `make test-host` 口径直接测
 * （tests/test_youkai_core.c）。引擎侧在 youkai.c。
 *
 * 设计唯一真源：`docs/superpowers/specs/2026-09-12-youkai-108-design.md`。
 * 一手：`local/_scratch/research-108-shooter-tick.md`（止射闸）、
 *       `local/_scratch/research-108-damage-transfer.md`（伤害转交）、
 *       `local/_scratch/research-death-signal-survey.md`（敌人移除点全集）。
 * 版本：TH18 v1.00a（裸地址默认属该版本）。可信度：✅ 一手 · 🟡 待实跑 · ★ 占位/TODO。
 *
 * 本文件按 spec 的小节组织：§3.1 止射闸 / §3.2 本体贴图 / §3.3 子机 / §3.4 弹幕 /
 * §3.5 发射口→使魔（1:1 映射与状态差分）/ §3.6 击破信号 / §3.7 伤害转交 1/2 / §4 数值。
 *
 * ⚠ **与 spec §3.1 / §3.4.1 的一处偏离（用户口径，2026-09-12）**：spec §3.1 的止射闸判据是
 *   「108 active && 聚焦」；本实现在它之上**多加一条「本帧妖侧子机数 > 0」** ⇒ 子机为 0 时**放行原生弹**。
 *   理由：膛口恒取子机（`0x40a9c0`）⇒ 无子机 = 妖侧无弹；照 spec 字面压掉原生弹会让那一档「一发弹都没有」。
 *   这不是复刻原版（TH08 妖侧不看火力、一按低速就有子机），是**用户 2026-09-12 拍的口径**，
 *   见 `yk_should_suppress_native` 的注释。
 *
 * ★ 所有「需要资产 / 需要用户决策」的点都是**具名常量 + TODO**，一个数值都不自己发明 ——
 *   缺资产的那几项一律取「**不做**」这个语义（占位值 = 不覆盖 / 不抢写 / 不持有），
 *   这样机制是完整的、单测能覆盖，而实机上不会替用户拍板。清单见 youkai.c 文件尾。
 */
#pragma once
#include <stdint.h>

/* ==================================================================== §3.1 止射闸
 * 「卡 active」由 SDK 保证（这条询问只对**挂在卡链表上**的行为调，见 sdk.c `BP_ce_shot_set_index`）——
 * 被动卡一旦拥有就一直在链上。
 * ⚠ 与 117 咆哮的次序由 SDK 定：**先问 set index，有卡覆盖了（咆哮）就不抑制**（已实装在 sdk.c）。
 * 返回非 0 = 请求 SDK 本帧跳过原生行循环（`retaddr = 0x45eca0`，卡广播照跑）。
 *
 * ★★ 判据 = **三条**（顺序即短路顺序）：
 *   ① `player_present` —— 有自机（标题 / 过场不发弹，那时改 `retaddr` 只是白多一次控制流改动）；
 *   ② `focused` —— 低速位（`player+0x476cc != 0`）；①② = **spec §3.1 原文**。
 *   ③ **本帧妖侧子机数 > 0** —— ❓ **口径：`count == 0` 放行原生弹（用户 2026-09-12 定）**。
 *      **不是一手复刻**：原版永夜抄的妖侧不存在「低火力就没子机、于是什么都不放」这个问题
 *      （TH08 妖侧不看 P、一按低速就有，见下面的数量表），所以「没子机就别压」这条**只能**是口径。
 *      为什么需要它：妖侧弹的膛口**恒取子机**（`0x40a9c0` 一手，AUDIT §BO-5）⇒ `count == 0` 时
 *      妖侧一发都发不出来，此时若照旧压原生弹，**玩家这一档一发弹都没有**。
 *      原本的行为（= spec §3.1 + §3.4.1 的字面组合）就是那个样子，2026-09-12 按用户口径改掉。
 *      同一条门的**数量事实来源只有一个**：`YK_OPTION_BY_CHAR` + `yk_option_count()`（经 `yk_option_wanted`），
 *      与子机声明层（youkai.c 的 `yk_option_declare`）查的是同一张表、同一帧的同一组
 *      `character` / `power_level` ⇒ 两边不可能各说各话。 */
int yk_should_suppress_native(int player_present, int focused, int character, int power_level);
/* 只保留「有自机 + 聚焦」的旧形状（= 无子机数信息的调用点 / 单测用的默认机体档 0）。
 * ⚠ 引擎侧**不要**再用它：真实判据一律走上面那条四参函数。 */
int yk_suppress_native_shot_spec31_legacy(int player_present, int focused);

/* ================================================================= §3.2 本体贴图
 * 本体 VM = `player+0x14`，换贴图 = 写 `vm+0x24`（sprite id）；脚本每帧写 sprite
 * ⇒ 必须 `on_draw`（帧末）每帧抢写。返回 **< 0 = 本帧不抢写**。
 *   高速 = 原机体 ⇒ 恒 -1（这是**设计**，不是占位：spec §0「高速 = 原机体」）。
 *   低速 = 妖侧（早苗 → 射命丸文，TH16 `pl03.anm`）⇒ ★ TODO(资产)：见下方常量。 */
#define YK_BODY_SPRITE_UNFOCUSED (-1)   /* 高速：原机体（设计，永久）*/
#define YK_BODY_SPRITE_FOCUSED   (-1)   /* ★ TODO(资产 §5)：妖侧本体 sprite 号。资产未接进 pl0X.anm ⇒ 不抢写 */
int yk_body_sprite(int focused);

/* ==================================================================== §3.3 子机
 * 换位（低速阵型）是**原生双偏移** `+0x64/+0x68` vs `+0x6c/+0x70`，引擎每帧自己取用，卡侧零成本。
 * 换图 / 数量 / **逐格阵型**走 **SDK 的低速子机声明层**（spec §3.3.1，sdk.h 的
 * `ce_option_pose_declare` / `ce_option_pose_winner` / ce_hooks_t 的 `on_option_pose_apply`）：
 * 各卡只**声明**，层选举 **id 最小者落笔** ⇒ **108(108) < 111(111)** ⇒ 108 胜、且只有它写字段。
 * 本卡只回答三个问题：**低速声明什么数量**、**每格摆在哪**、**高速退出声明**（用户定案 5
 * 「高速互不影响」= 高速时不占位，111 在高速自然不受 108 影响）。*/

/* ---- ★ 低速子机**数量**表（用户 2026-09-12 定案：「看不同机体在永夜抄与天空璋（文文）的数量」）----
 * `[机体][火力档]`，机体 = `CE_ADDR_CHARACTER` `0x4cccf4` 的 0..3 = 霊夢 / 魔理沙 / 咲夜 / 早苗，
 * 火力档 = `CURRENT_POWER / POWER_PER_LEVEL(100)` 钳到 0..4（= `yk_shot_tier()`，与 `th18:0x45d604`
 * 的算式同款）。逐行的**一手出处**：
 *
 *   行 0 霊夢 → 妖侧 = 八雲紫（TH08 `pl00`）   : **1，与 P 无关**
 *   行 1 魔理沙 → 妖侧 = アリス（TH08 `pl01`） : **1，与 P 无关**
 *   行 2 咲夜 → 妖侧 = レミリア（TH08 `pl02`） : **4，与 P 无关**
 *        ↑ 一手：TH08 激活表 `th08:0x4c7d40[shottype][4]`（进低速那一帧读，`th08:0x44b0ac`；
 *          非 0 项数 = 活的格数），行 0/1 只有第 1 项非 0、行 2 四项全非 0；独立印证 = 8 张 `ply*.sht`
 *          的子机索引集合（`ply00as={0,1}` / `ply01as={0,1}` / `ply02as={0,1,2,3,4}`；`N=0` 是本体）。
 *          `GameManager::get_current_power` `th08:0x422480` 的 22 个 call **没有一个落在子机链** ⇒ 与 P 无关。
 *   行 3 早苗 → 射命丸文（TH16 `pl03`）        : **`floor(P/100)` = 0/1/2/3/4**
 *        ↑ 一手：`th16:0x444100 mov eax,ds:0x4a57e4`（CURRENT_POWER）→ `th16:0x444106 idiv
 *          DWORD ds:0x4a57ec`（÷100 = 档）→ 建 `档` 格（`th16:0x4444aa cmp edx,ecx`），
 *          `th16:0x4444c1 cmp edx,0x4` 起把剩下的格清空 ⇒ 档 0 = **0 个**。
 *
 * ⚠ 三行常数就把 5 个档写同一个数（形状见 spec §12.4：一张表同时表达「固定」与「随火力变」，
 *   换口径只改这张表、不动调用点）。
 * ⚠ 这里说的是**本卡声明几个妖侧子机**，**不是**屏幕上的子机总数 —— TH18 档 ≥ 1 时
 *   原生自己也有 `档` 个子机（`th18:0x45dbcf` 的循环），那是另一个数组（spec §12.3 的 ⚠）。
 * ⚠ 档 0 的那一格：TH08 妖侧**不看 P、一按低速就有**（所以行 0–2 的档 0 仍是 1/1/4，与原作一致），
 *   而 TH16/TH18 档 0 = 0（行 3 照抄）。*/
#define YK_OPTION_TIERS   5      /* 列数 = TH18 火力档（= YK_SHOT_TIERS / `CE_ADDR_MAX_POWER`/100 + 1）*/
#define YK_OPTION_CHARS   4      /* 行数 = `CE_ADDR_CHARACTER` 的 0..3 */

static const unsigned char YK_OPTION_BY_CHAR[YK_OPTION_CHARS][YK_OPTION_TIERS] = {
    /* 霊夢   → th08 pl00 妖侧 = 八雲紫   */ { 1, 1, 1, 1, 1 },
    /* 魔理沙 → th08 pl01 妖侧 = アリス   */ { 1, 1, 1, 1, 1 },
    /* 咲夜   → th08 pl02 妖侧 = レミリア */ { 4, 4, 4, 4, 4 },
    /* 早苗   → th16 pl03      = 射命丸文 */ { 0, 1, 2, 3, 4 },
};

/* 本帧该声明几个妖侧子机。`focused` = 低速位（`player+0x476cc`，spec §3.1）；
 * 高速恒 0（= 不参与声明）。character / power_level 越界一律钳进表内（引擎读到的值不信任）。 */
int yk_option_count(int focused, int character, int power_level);
/* = count > 0：这张卡本帧参不参与声明（子机 = 妖侧弹的膛口 ⇒ 顺带是「能不能发弹」的判据）。 */
int yk_option_wanted(int focused, int character, int power_level);

/* ---- ★ 逐格阵型（spec §12.4 的形状缺口；为什么必须逐格，一手见 sdk_core.h 的 `CE_OPTION_POSE_CELLS`）----
 * 每格偏移取自**本作（TH18）原生 `pl0X.sht` 自己的位置块**，与引擎给原生子机摆的阵型同一份数据：
 *   非聚焦块 `+0x40`、聚焦块 `+0x90`，每格 `(x,y)` float、单位 = **像素**；
 *   格号 `k = T[count-1] + i`（`T = {0,1,3,6,10,11,13,...}`，一手 `th18:0x45d8fc`：
 *   `edx = TRI[档−1] + i`，`movss xmm0,[ecx+edx*8+0x40]`）⇒ 「声明 N 个」时用的就是引擎给
 *   **N 个原生子机**准备的那 N 个位置。
 * ⚠ **咲夜（机体 2）的那两块不是像素**：`x` 是**弧度**、`y` 是**半径**（一手：原生 `pl02.sht` 的
 *   `(0, 38) (0.3,38) (-0.3,38) …`；引擎对机体 2 另挂 `th18:0x45dd10` 每帧把极坐标转成位置）。
 *   本表里机体 2 那一行已经**离线换算成像素**（换算规则一手：`th18:0x45dd10` 读 `option+0xa8` 的角度
 *   加 `player+0x4790c`（`Player::reset` `th18:0x45ade6` 置 **−π/2**）→ `call 0x45e300`（= `(r·cosθ, r·sinθ)`，
 *   `fsincos` + `fmul`；同族见 `engine/_shared/math-and-prng.md` §1.5）→ 乘 `0x4b9464`（= **−128.0f**）
 *   再**减**自机坐标 ⇒ 位移 = `(r·cosθ, r·sinθ)`、θ = −π/2 + 表里的角度）。
 *   取「静止（lean = −π/2）」那一态 ⇒ 4 格 = 头顶上一圈扇形。**偏差**：原生的环会随左右移动的
 *   lean 角转，我们的是**静态**位置（要转动得每帧算三角函数 = 引 libm，本仓 .c 禁 x86 浮点库，故不做）🟡。
 * ⚠ 位置这一项的**口径**：数量照各自的原作（上表），**位置照本作同机体同档的原生阵型**（一手数据）。
 *   这不是「复刻 TH08 的妖侧阵型」—— TH08 每格子机的阵型烧在各自脚本/表里，本轮没有一手数据 ⇒ 标 ❓自定。*/
#define YK_OPTION_CELLS   4      /* = `CE_OPTION_POSE_CELLS`（youkai.c 有 _Static_assert 对账）*/

typedef struct {
    float nfx, nfy;   /* 非聚焦块（`sht +0x40`）该格的偏移，像素 */
    float fx,  fy;    /* 聚焦块（`sht +0x90`）该格的偏移，像素 */
} yk_opt_off_t;

/* 取 `count` 个妖侧子机里第 `cell` 格的偏移（`count` = 1..YK_OPTION_CELLS，`cell` = 0..count-1）。
 * 返回 1 = 填好 `*out`；0 = 参数越界（`*out` 不动）。**无浮点运算**（纯查表搬运）。*/
int yk_option_offsets(int character, int count, int cell, yk_opt_off_t *out);

/* ============================================================ ★★ th08 #1：妖侧子机的**位置**
 * >> 2026-09-22（P4）。owner 单卡实跑 #1：「灵梦机体妖子机本身应当追踪上去」。
 *
 * **一手（原作怎么做）**：TH08 的妖侧子机**位置是逐机体写死的代码**，不是数据表 ——
 * 引擎在进/出低速那一帧把两个回调装进每个子机（`th08:0x44b0ac` 一带）：
 *   生成回调 `g_OptionInitCallbacks[character][i]` @ `th08:0x4c7d40`、
 *   每帧回调 `g_OptionUpdateCallbacks[character][i]` @ `th08:0x4c7e10`
 *（表是 8×4，前 4 行 = 四个机体；后 4 行是「单人模式」那一半）。逐角色读出来：
 *
 * | 机体 | 每帧回调 | 妖侧子机的位置 | 运动 |
 * | --- | --- | --- | --- |
 * | 0 霊夢・紫 | `th08:0x44e3a0` | **1 个：(0, −96)**（`th08:0x4b42c4` = 96.0），`y` 夹到 `>= 32`（`th08:0x4b42cc`）| **弹簧跟随**（`th08:0x44e770`）|
 * | 1 魔理沙・アリス | `th08:0x44ea40` | **1 个：(0, −32)**（`y -= 32.0`）| **每帧直接吸附**（无速度）|
 * | 2 咲夜・レミリア | `th08:0x44eb70` | **4 个**：`(−30,−16) (−10,−32) (+10,−32) (+30,−16)` | 每帧直接吸附 |
 * | 3 妖夢・幽々子 | `th08:0x44f930` | 2 个（本卡不做：早苗的妖侧是 TH16 文）| — |
 *
 * 咲夜那 4 格来自 `th08:0x44eb70` 的**子机序号跳表** `th08:0x44ee4b` → 四个分支：
 * `idx0 x-=30.0 y-=16.0` / `idx1 x-=10.0 y-=32.0` / `idx2 x+=10.0 y-=32.0` / `idx3 x+=30.0 y-=16.0`
 *（常量 `th08:0x4b4534` = 30.0 / `th08:0x4b4390` = 10.0 / `th08:0x4b42d4` = 16.0 / `0x4b42cc` = 32.0）。
 *
 * **弹簧的逐步式**（`th08:0x44e770`，逐指令）：
 * ```
 * delta = (goal − pos) * (K / 16.0)      ; K = ds:0x4b4338 = 1.0、16.0 = 0x41800000，见 th08:0x40b470
 * vel  += (delta − vel) * 0.2            ; 0x3e4ccccd
 * pos  += vel                            ; th08:0x410a70 两次（先累加 vel，再把 vel 加到 pos）
 * ```
 * 其中 `goal = 自机坐标 + 该格的家偏移`（`y` 先夹到 `>= 32`）；这是**跟随自机时**的目标。
 *
 * ⚠ **一条一手负结论（很重要，别把它当漏做）**：TH08 的 `0x44e3a0` 里**还有一条「追敌人」分支**
 *   （子状态 3 → `th08:0x44e8d0`，目标 = `player+0xe2abc` 指向那只敌人的坐标），**但它在 v1.00d
 *   里够不到**：`player+0xe2abc` 全 exe 只有两处写点、**写的都是 0**（`th08:0x44e455` /
 *   `th08:0x44e6d3`），没有任何地方写入过目标指针（同一字段的 10 处引用逐条看过：8 读 + 2 写 0；
 *   子机弹那道"锁定追踪"`th08:0x450240` 的 `0x450279` 也读同一个字段 ⇒ 同样够不到）。
 *   ⇒ **Reimu 的妖侧子机在 v1.00d 不会追敌人**；「追踪」= **弹簧跟随自机**（会滞后，看得见）。
 *   ⇒ 本卡按这条实现，**不做**追敌人（见报告 §#1）。
 *
 * 本卡的落点：`on_option_pose_apply` 每帧把**弹簧算出来的位置 − 自机坐标**写进聚焦偏移字段
 *   ⇒ 引擎照旧 `pos = 自机 + 偏移`（`th18:0x45bce8`–`0x45bd08`），观感 = TH08 的滞后跟随。
 *   只有机体 0 需要状态（`yk_opt_spring_state_t`）；另两个机体一帧内就吸附到位 ⇒ 静态偏移即可。*/
#define YK_OPT_SPRING_K      1.0f     /* `ds:0x4b4338`（= 1.0）*/
#define YK_OPT_SPRING_DIV    16.0f    /* `th08:0x44e770` 压的那个 16.0（`0x41800000`）*/
#define YK_OPT_SPRING_DAMP   0.2f     /* `0x3e4ccccd` */
#define YK_OPT_HOME_Y_MIN    32.0f    /* `0x4b42cc`：家位置的 y 下限（TH08 把子机压在屏上沿以下）*/
#define YK_OPT_HOME_Y_REIMU  (-96.0f) /* `0x4b42c4` = 96.0，`y -= 96` */
#define YK_OPT_HOME_Y_MARISA (-32.0f)
#define YK_OPT_HOME_X_SAKUYA_INNER (-30.0f)  /* `0x4b4534` = 30.0 */
#define YK_OPT_HOME_X_SAKUYA_OUTER (-10.0f)  /* `0x4b4390` = 10.0 */
#define YK_OPT_HOME_Y_SAKUYA_OUTER (-16.0f)  /* `0x4b42d4` = 16.0 */

/* 某一格的家偏移（像素，屏幕空间、+y = 下）。`character` 越界（含早苗 3）⇒ 返回 0 = 没有一手数据
 *（调用者回落到 TH18 原生阵型表 `YK_OPT_BLOCK`）。 */
int yk_option_home_xy(int character, int cell, float out[2]);

/* 只有机体 0 走弹簧（TH08 `0x44e3a0` 的 `th08:0x44e770`）；其余机体每帧直接吸附。 */
int yk_option_uses_spring(int character);

/* 弹簧一步（**就地**更新 pos / vel；`goal` = 家位置）。逐步式与常量见上面那段。 */
void yk_opt_spring_step(float pos[2], float vel[2], const float goal[2]);

/* 家位置 = 自机坐标 + `yk_option_home_xy`，并把 `y` 夹到 `>= YK_OPT_HOME_Y_MIN`
 *（TH08 `th08:0x44e770` 里那一步；`character` 没有一手数据 ⇒ 返回 0、`out` 不动）。*/
int yk_option_home_from_player(int character, int cell, const float player_xy[2], float out[2]);

/* 声明的脚本和手里那个子机的脚本不一样 ⇒ 必须重申请（脚本是申请时定死的）。
 * `have < 0` = 手里还没有子机 ⇒ 不算「变了」。 */
int yk_option_script_changed(int want, int have);

/* =================================================================== §3.4 弹幕
 * 形态一档固定（用户 2026-09-12 拍板；★ 相对 TH08 是**主动简化** ⇒ ❓ 自定）、
 * **数值随 P 变**（这半条与原作一致）：`on_bullet_created` 里按 `bullet+0xac` 的打包 set id
 * 认出妖侧弹 ⇒ 改 `bullet+0x9c`。
 *   档 = `CURRENT_POWER / POWER_PER_LEVEL(100)` 钳到 [0,4]（TH18 五档；TH08 是六档阈值
 *   `8/24/48/80/128/999`，人妖两表共用 —— 映射到 TH18 五档是**本卡的决定**，见报告）。
 *
 * ★ 2026-09-12 一手补齐：TH08 妖侧每档伤害**已经抽出来了**（`rec[+0x1c]`，读点
 *   `th08:0x451805`；来源 `local/th08.v1.00d/dat/ply03as.sht`，解析脚本见报告）。
 *   但**不能照抄进下面那张表**：TH08 的「每发伤害」是配着「路数」一起设计的 ——
 *   满 P 档 9 发里单发 22–30，最低档 3 发里单发 36–48，**单发伤害随 P 下降**、靠路数变多
 *   把总 DPS 顶上去。我们的形态是**一档固定**（见下），把 TH08 的单发值搬进五档表
 *   会让「P 越高越弱」。⇒ 五档保持 `YK_SHOT_DMG_NOT_SET`（不覆盖），
 *   **这是决定不是占位遗漏**：要不要另设计一条随 P 上升的曲线由用户定（报告 §4 ❓）。*/
#define YK_SHOT_TIERS            5      /* TH18 火力档 0..4（CURRENT_POWER 0..400 / 100）*/
#define YK_SHOT_POWER_PER_LEVEL  100    /* = `CE_ADDR_POWER_PER_LEVEL`（0x4ccd40）*/
#define YK_SHOT_DMG_NOT_SET      0      /* 档位表里这个值 = 「不覆盖 `bullet+0x9c`」*/
/* 妖侧 shooterset 槽 = **覆盖**原生那格空槽（不是追加 —— 偏移数组 40 格已经全满）。
 * 一手：四份原生 `pl0X.sht` 的 `+0x02` 都是 40、`0x00`–`0x16` 逐项非 0、`0x17`–`0x27` 是本 mod
 * 其余四张卡（`native/sht_ids.h`）⇒ 没有「新索引」可加；而 `0x16` 是原生**唯一没有卡映射**的一格
 * （扫全 exe，调 `Player__tick_shooters_for_ability_card` `0x40a9c0` 的 11 处压栈立即数 =
 * `{0x0a,0x0b,0x0c/0x0d,0x0e,0x0f,0x10,0x11,0x12,0x13,0x14,0x15}`，没有 `0x16`；
 * 主炮 `0x45ea00` 的公式只给 `0x00`–`0x09`）。
 * 分配／写入它的是 `assets/sht/append_shooterset.py` 的 `REPLACE` 表；那份脚本会回头核对
 * 本文件里的这两个常量，对不上就让 `make sht` 失败（防两边静默漂移）。
 * 内容 = TH08 v1.00d `ply03as.sht`（妖夢＆幽々子 的**妖侧**）第 5 档（`thr = 128` = 满 P）
 * 的 9 行逐字段翻译，`fire_rate = 3/5/10/6` ⇒ **除零风险随覆盖一起消失**（原生那格是 0，
 * 而 `0x40a9c0` 的 `idiv` 在 `0x40aa4f` 没有零分支）。逐字段出处见该脚本的 `REPLACE` 注释。*/
#define YK_SHOT_SET_YOUKAI       0x16
#define YK_SHOT_SET_YOUKAI_ROWS  9      /* 那组有几行（行号进 `packed_id` 低 8 位；认出妖侧弹只看 set 字节）*/

/* ★★ 2026-09-22（P4 · th08 #5「妖侧子机发射的弹幕都相同、火力约 4P」）：
 * **妖侧不再只有一格** —— 现在是「**逐角色 × 逐火力档**」的 15 格（照抄 TH08 `plyNNas.sht`
 * 的 slot 0..4，一手见 `engine/sht/th08/01-youkai-shootersets.md`），`0x16` 那一格**保留**
 * （早苗的妖侧是 TH16 射命丸文，本批没有一手行数据 ⇒ 她回落到 `0x16`）。
 *
 * 槽号由生成物 `native/sht_ids.h` 给出（`CE_SHT_SET_YOUKAI_R0..S4`），纯逻辑层**不 include
 * 生成物**（要保持「只 <stdint.h>、能主机单测」）⇒ 把这三个数**由调用者传进来**。
 * `YK_SETS_SINGLE_ONLY` = 只有 `0x16` 那一格（旧行为；单测与「生成物没到」的场合用）。*/
typedef struct {
    uint32_t single;       /* `0x16`（原生唯一无卡映射的一格；早苗回落用）*/
    uint32_t tier_first;   /* 15 格的第一格（`CE_SHT_SET_YOUKAI_R0`）*/
    uint32_t tier_count;   /* 15（`tier_count == 0` = 没有逐档组，只看 `single`）*/
} yk_sets_t;

#define YK_SETS_SINGLE_ONLY(single_)  { (single_), 0u, 0u }
/* 逐档组的格数 = 3 个 TH08 角色（霊夢/魔理沙/咲夜）× YK_SHOT_TIERS 档。
 * ⚠ 生成器（`assets/sht/append_shooterset.py` 的 `_youkai_tier_sets`）与卡侧**各自**写死
 *   这个布局 ⇒ 两边各有一条对账：Python 侧按名字顺序生成、C 侧用 `_Static_assert` 钉槽号。*/
#define YK_YOUKAI_TIER_SETS  (3 * YK_SHOT_TIERS)

/* `set` 是不是本卡的妖侧组（`s == NULL` ⇒ 退回「只看 `single`」）。 */
int     yk_set_is_youkai(uint32_t set, const yk_sets_t *s);
int     yk_shot_tier(int power);
int32_t yk_shot_tier_damage(int tier);
/* 打包 id = `(set << 8) | 组内行号`（`0x45ec69` 打包 / `0x45ee10` 回读）。 */
int     yk_bullet_is_youkai(uint32_t packed_id, const yk_sets_t *s);
/* 1 = 要覆盖；*out = 覆盖后的值（档位表是占位 0 时恒返回 0）。 */
int     yk_bullet_damage_apply(int32_t current, uint32_t packed_id, int power,
                               const yk_sets_t *s, int32_t *out);
/* 妖侧 shooterset 能不能用：记录首字节 = `fire_rate`（i8）。`0` ⇒ 资产没装（会除零）、`< 0` ⇒ 终止符。 */
int     yk_shotset_usable(int fire_rate);

/* ============================================================ §3.5 发射口 → 使魔
 * 1:1 映射：该敌当前 `props[i].type(+0x00, u16) != 0` 的发射口槽数（实测以 1 为主、boss 最多 4）。
 * 每帧**状态差分**：活跃且无使魔 → 建；不活跃且有使魔 → 撤；本帧没扫到（敌人没了）→ 撤 + 退休。
 * 地图键 = `(enemy_id, 发射口槽)`（**不是指针**：敌人是堆对象，`allocate_new_enemy` = `new`）。 */
#define YK_EMIT_SLOTS     16     /* `zEnemyData.bullet_props[0x10]`（IDEAS.md §1.3）*/
#define YK_FAM_MAX        32     /* 工程护栏（**不是**游戏性数值）。spec §4 没给上限 ⇒ TODO(决策)。
                                  * 32 条 × **28 B** = 896 B 的**文件级静态**表（不进卡的 512 B 私有状态预算，
                                  * 照 117 动物灵 `s_tok[40]` 的做法；单测断言 ≤ 1024 B）；每只使魔 = 一次
                                  * 0x683c 字节堆 `new` + 一个敌人 tick ⇒ 表满时**放弃这一只**（不崩、不排队）。*/
#define YK_LIFE_LIMIT     1000   /* ★ 硬约束（spec §3.5 / §6.1）：spawn 参数 `+0x14` 必须 < 这个数，
                                  * 否则 `flags_lo` 位 30 被置 ⇒ 进锁定黑名单 ⇒ 诱导打不到使魔。*/
#define YK_LIFE_MAX_ALLOWED 999
#define YK_LIFE_UNSAMPLED INT32_MIN   /* `prev_life` 未采样（第一次看到这只使魔）*/
/* ★ TODO(数值设计 §10⑥)：使魔 HP **待用户定**（spec §4 表里是 ⏳）。占位 = 1：
 *   「一发即破」—— 它**不是**数值设计结果，只满足两条：① 必须 < `YK_LIFE_LIMIT`（硬约束，上面那条）；
 *   ② 让「使魔可打 → 击破 → 关那个槽 → 1/2 转嫁」这条链在实跑里**一发就能验完**。
 *   定案后改这一行即可（`yk_clamp_spawn_life` 会拦住 >= 1000 的误填）。*/
#define YK_FAM_LIFE       1

/* ★★ 2026-09-22（P4 · th08 #7 「击破 boss 一个阶段后 boss 几乎不再发弹」的修法）：
 * **发射口闸的自愈上限**。
 *
 * **发现**（代码 + 引擎事实）：闸的判据是「这一槽曾经有过我们的使魔、现在没了」，而它唯一的复位口
 *   是 `yk_note_slot` 的 `props[i].type` **inactive→active 边沿**（= ECL 先 `etNew(i)` 清零 type、
 *   再 `etSprite(i)` 写回；一手 `43404e` 那条 4 字节写 type）。**只要 ECL 不再走这条边沿，
 *   那一槽就永久哑火**：闸恒返回 1（SDK 置 `retaddr = 0x43402d` ⇒ `etOn` 本帧不发射），
 *   而且使魔也永远不重建（`broken` 挡着）。
 *   这条路径在 boss 战里**必然发生**：boss 的发射口在整个阶段里往往**持续活跃**
 *   （反复 `etOn`、中间不调 `etNew`），而我们的使魔（`YK_FAM_LIFE = 1` 占位值）几乎必然被打掉
 *   （它钉在发射口上、与 boss 本体重叠 ⇒ 自机打 boss 的弹顺带就把它打死）⇒
 *   击破一个阶段之后那些槽就再也不发弹 —— 正是 owner 报的现象。
 *
 * **修法（不改闸的语义，只给它加「到期」）**：一个槽进入「记忆里有使魔、当前没有」的状态后开始计数；
 *   连续 `YK_EMIT_BROKEN_TTL` 帧都这样（既没重建成功、也没被重新武装）⇒ **无条件放开**：
 *   清 `fam_seen`（闸放行）+ 清 `broken`（允许 1:1 映射把使魔建回来）。计数在 `yk_fam_created`
 *   （重建成功）与「重新武装」边沿处归零。
 *
 * **为什么必须有**（三条独立理由，任一条成立就够）：
 *   ① 复位口完全依赖 ECL 的写法 —— 那是**别人的数据**，不能假设它一定调 `etNew`；
 *   ② 使魔建失败（表满 / 堆拒绝）也落在「记忆里有、当前没有」，而那**不是**「被打死」；
 *      照旧夹着 = 一个与我们无关的运气事件让敌人永久哑火；
 *   ③ 删卡 / 松低速 / 局末会走 `yk_map_reset` 放开，但**关卡中途**没有别的东西能放开。
 *
 * **代价（🟡 记账）**：原设计（spec §9.6）是「打掉使魔 ⇒ 那个发射口停火」**直到 ECL 重新武装**；
 *   现在变成「停火至多 TTL 帧」。TTL 是本仓**自定**值（原作没有这条闸 ⇒ 没有一手可抄）⇒
 *   已登记 `TUNING.md`。实跑若觉得「打掉使魔的收益窗口太短 / 太长」，只改这一个数。 */
#define YK_EMIT_BROKEN_TTL 120   /* ❓ 自定：闸的自愈期限（帧，= 2 s @60fps）*/

/* 槽位 → 使魔皮肤**变体**（0..3）。卡侧把变体映射到 `anm_ids.h` 的脚本宏（**不写死数字**）：
 * `CE_ANM_ABILITY_SCRIPT_YOUKAI_FAMILIAR_S53 + 变体`。
 * 语义（用户 2026-09-12 定案，spec §3.5）：**使魔数量 = 该敌当前活跃发射口槽数（1–4）
 * ⇒ 四色正好一槽一套，槽 i 用 `s53+i` 那一套**。四色原版切图 = TH08 `enemy.anm` script53..56
 * （红 / 绿 / 蓝白 / 黄），已切进 `assets/ability/scripts/206_youkai_familiar_s53.anm.txt` … `209_…_s56`。
 * 槽域是 0..`YK_EMIT_SLOTS`-1 而四色只有 4 套 ⇒ 槽 >= 4 取 `slot & 3`
 * （❓ **自定兜底**：规格说实测最多 4 个活跃槽，这条理论上走不到；不是一个「第 5 种颜色」的承诺）。*/
int     yk_fam_variant(int slot);

/* 发射点世界坐标（`etOn` 的算法，IDEAS.md §1.3）：`origins[i].z > 0.9` = 绝对模式。 */
#define YK_ORIGIN_ABS_THRESHOLD 0.9f
void yk_emit_world_pos(const float enemy_xy[2], const float off3[3], const float org3[3], float out_xy[2]);
/* 活跃判据：`props[i].type != 0`（`etNew` 清零、只有 `etSprite` 写它，spec §6.3）。 */
int  yk_emit_active(uint16_t prop_type);

enum { YK_ACT_NONE = 0, YK_ACT_CREATE = 1, YK_ACT_WITHDRAW = 2 };

typedef struct {
    int      kind;       /* YK_ACT_* */
    uint32_t enemy_id;   /* 发射口所属敌人（建的时候要它的指针 / 撤的时候只是记账）*/
    uint32_t fam_id;     /* 使魔的 `enemy_id`（WITHDRAW 时有意义；每帧用 id 回查，不缓存指针）*/
    int      slot;       /* 发射口槽 0..15 */
    float    x, y;       /* 发射点世界坐标（本帧）*/
} yk_action_t;

typedef struct {
    uint32_t enemy_id;   /* 0 = 空条目（`enemy_id` 单调递增、不复用，见 engine.h `CE_ENEMY_ID`）*/
    uint32_t fam_id;     /* 0 = 这个槽没有活着的使魔 */
    int32_t  prev_life;  /* 使魔 `life.current` 的上一帧值（YK_LIFE_UNSAMPLED = 还没采过）*/
    float    x, y;       /* 发射点世界坐标（每帧重算，卡侧按它钉使魔位置）*/
    uint8_t  slot;
    uint8_t  seen;       /* 本帧扫到过这个 (敌人, 槽)*/
    uint8_t  active;     /* 发射口本帧活跃（`props[i].type != 0`）*/
    uint8_t  broken;     /* 使魔被打死 ⇒ 该槽「被打破」：不再重建，等 ECL 重新武装（inactive→active）*/
    uint8_t  fam_seen;   /* ★ 2026-09-12：这一槽**曾经有过我们的使魔**（关闸「曾经有过」的记忆）。
                          *   与 `fam_id` 分开：`yk_fam_broken` / `yk_fam_gone` 都会清 `fam_id`，
                          *   但闸要一直夹到 ECL 重新武装（inactive→active 边沿）为止。
                          *   塞进原有 padding ⇒ `yk_slot_t` 仍是 24 B（单测有 sizeof 断言）。*/
    uint16_t no_fam_frames; /* ★ 2026-09-22（#7）：`fam_seen == 1 && fam_id == 0` 已经连续多少帧
                          *   （= 闸夹着、使魔又不在）。到了 `YK_EMIT_BROKEN_TTL` 就自愈
                          *   （清 `fam_seen` + 清 `broken`）。归零口 = `yk_fam_created` /
                          *   重新武装边沿 / 条目退休。**正好塞进 padding ⇒ 仍是 28 B**。*/
} yk_slot_t;             /* 28 B × YK_FAM_MAX（2026-09-12 加 fam_seen 后 24→28，见该字段注）*/

typedef struct {
    yk_slot_t e[YK_FAM_MAX];
    uint32_t  heals;     /* ★ 2026-09-22（#7）：发射口闸自愈过几次（每自愈一格 +1，只增不清）。
                          *   卡侧每帧读一次、与自己的快照比对后打日志 ⇒ 实跑里一眼看出
                          *   「boss 不发弹」到底是闸夹死（heals 涨）还是别的原因。
                          *   放在表尾（`yk_slot_t` 的布局不变）⇒ `sizeof(yk_map_t)` = 32×28+4 = 900 B。*/
} yk_map_t;

unsigned   yk_map_heals(const yk_map_t *m);      /* 自愈计数（#7 记账；NULL ⇒ 0）*/

void       yk_map_reset(yk_map_t *m);
unsigned   yk_map_fam_count(const yk_map_t *m);
yk_slot_t *yk_map_find(yk_map_t *m, uint32_t enemy_id, int slot);
yk_slot_t *yk_map_find_fam(yk_map_t *m, uint32_t fam_id);
void       yk_frame_begin(yk_map_t *m);
/* 扫到一个发射口槽。返回 1 = `*act` 里有一条要执行的动作（建 / 撤）。 */
int        yk_note_slot(yk_map_t *m, uint32_t enemy_id, int slot, int active, const float xy[2],
                        yk_action_t *act);
/* 帧末：本帧没扫到的条目 = 敌人没了 ⇒ 有使魔就撤，然后退休该条目。返回动作数。 */
int        yk_frame_end(yk_map_t *m, yk_action_t *out, int max);
/* 卡侧建完使魔后回填（记 fam_id、把 life 采样清成「未采样」）。 */
void       yk_fam_created(yk_map_t *m, uint32_t enemy_id, int slot, uint32_t fam_id);
/* §3.6 击破信号：使魔被打死 ⇒ 那个槽「被打破」。返回 1 = 这只使魔是本卡的。 */
int        yk_fam_broken(yk_map_t *m, uint32_t fam_id);
/* 使魔查不到了（过场 `destroy_all` / 静默移除 / 我们自己刚标了删除还没被 sweep）⇒ 松开。
 * ★ 2026-09-12：**不置 `broken`**（唯一复位口是 inactive→active 边沿，置了会把「下一帧补建」堵死；
 *   既有单测 `槽 3 静默移除 ⇒ 下一帧重建` 钉的就是这条）。关闸用的「曾经有过」记忆改由
 *   `fam_id` 承担 —— 清 0 那一刻闸夹上，重建回填后自动放开。 */
void       yk_fam_gone(yk_map_t *m, uint32_t fam_id);
/* 全部撤（高速 / miss / 局末 / 删卡）。返回动作数。 */
int        yk_withdraw_all(yk_map_t *m, yk_action_t *out, int max);

/* ==================================================== §3.5.1 发射口关闸（精确闸，2026-09-12）
 * 新 SDK 面 `on_enemy_shot_slot`（断点 `0x433ffa` = etOn 的「发弹三步」汇聚点，一手见 sdk.c）：
 * 返回非 0 ⇒ SDK 置 `retaddr = 0x43402d` ⇒ **本帧这一槽不发射**（零内存写、每帧每槽问一次、可逆）。
 *
 * **本卡判据**：`(enemy_id, slot)` 这个发射口**曾经有过我们的使魔、现在没了** ⇒ 掐掉本帧发射。
 *   `have_fam_id` = 卡侧按 id 回查当前活着的使魔（0 = 查不到）。
 *   返回 1 = 抑制。
 *
 * ⚠ **为什么不能只看「没有使魔就掐」**：`have_fam_id == 0` 有三种来路，只有②该掐 ——
 *   ① 我们还没为它建过（建被排到下一个 `on_tick_2`）⇒ **必须放行**，否则第一帧就把槽掐死、
 *      永远建不出使魔；
 *   ② 有过、现在没了 ⇒ 掐；
 *   ③ 表里没有这个 `(enemy_id, slot)` 条目（表满 / 已被 `yk_frame_end` 退休）= 我们**从没管过** ⇒ 放行。
 * ⇒ 「曾经有过」这一位记忆必须**单独存**（`yk_slot_t.fam_seen`）：`fam_id` 会被 `yk_fam_broken` /
 *   `yk_fam_gone` 清掉、`broken` 会被重新武装清掉，两者单独用都会漏（实测：用 `fam_id` 时
 *   「静默移除后那一帧」判不出来，用 `broken` 时「被打死后又武装过」判不出来）。
 *   复位口 = `yk_note_slot` 的 inactive→active 边沿（ECL 重新武装）+ `yk_map_reset`（删卡等）。
 *   **该函数不读 `props[].type`** ⇒ 不受「`etOn` 与 `etSprite` 谁先写 type」这个未定量的影响。 */
int        yk_shot_slot_suppressed(const yk_map_t *m, uint32_t enemy_id, int slot, uint32_t have_fam_id);

/* 硬约束护栏：spawn 的 life 参数 >= 1000 会进锁定黑名单 ⇒ 钳到 999（并在卡侧记一条日志）。 */
int32_t    yk_clamp_spawn_life(int32_t life);

/* =============================================================== §3.7 伤害转交 1/2
 * 触发器 = 使魔自己的 `life.current`（`enemy+0x6220`）**逐帧差分**（不是 `on_shot_damage`：
 * 那个回调第一行就有 `CE_PLAYER_FOCUSED()` 门，而妖机体 = 低速 = 聚焦 ⇒ 卡生效期间一次都不来，
 * 见 research-108-damage-transfer §0）。
 * 落点 = **每帧新建**一条 `ce_damage_rect` 打在 boss 坐标（一源一敌一辈子只结算一次，tag 守卫
 * `0x45f45a` ⇒ 不能复用）+ 同帧 `ce_raise_damage_cap(基值 + 转嫁值)`。
 * 比例 = 1/2（**用户 2026-09-12 定案**；❓ 自定 —— TH08 没有这条管线，不是复刻，spec §3.7）。*/
#define YK_TRANSFER_DEN 2                       /* 1/2 */
#define YK_TRANSFER_CAP_PER_FRAME 0             /* ★ TODO(决策 §7.5-1)：单帧转嫁上限。0 = 无上限（占位）。
                                                 *   研究提醒：使魔一刀 2000 ⇒ boss 一帧 1000，会顶掉 boss 的
                                                 *   每帧配额并放大计分 ⇒ 要不要封顶由用户定。*/
typedef struct {
    int32_t carry;      /* 奇数额外那半的进位（累计到偶数再补，见 yk_half_with_carry）*/
    int32_t pending;    /* 本帧要转嫁的量（卡侧转完清零；取不到 boss 时按研究 §4 丢掉并记账）*/
} yk_transfer_t;

void    yk_transfer_reset(yk_transfer_t *t);
/* delta 的一半（向下取整），余数进 `*carry` ⇒ 连续两帧的奇数伤害**不丢半分**。 */
int32_t yk_half_with_carry(int32_t delta, int32_t *carry);
void    yk_transfer_feed(yk_transfer_t *t, int32_t delta);
/* cap <= 0 = 无上限（占位默认）；否则钳到 cap。 */
int32_t yk_transfer_clamp(int32_t pending, int32_t cap);
/* 采样一只使魔的 life.current：返回本帧掉血量（>0）。
 * 第一次采样 / 换了一只使魔（`fam_id` 变了）/ life 变大（引擎自己重算）都返回 0。 */
int32_t yk_sample_life(yk_slot_t *e, uint32_t fam_id, int32_t life_now);
/* 自伤保护：转嫁源打在 boss 坐标的小盒上。若某只使魔**也**在这个盒子里
 * （boss 的发射口就在本体上 ⇒ 使魔被钉在本体中心，这很常见），它这一帧掉的血是我们自己的源打的，
 * 计入转嫁会**每帧自激**（research §3② 那条回路）。盒 = 以中心为心、宽 w × 高 h，
 * 四周各加 `YK_FAM_HURTBOX_HALF`（惰性真敌人的默认 hurtbox 24.0 的一半，spec §6.1）。
 * 返回非 0 = 在盒内 ⇒ 这一帧的掉血**不计入**转嫁。 */
#define YK_FAM_HURTBOX_HALF 12.0f
int     yk_in_transfer_zone(float fx, float fy, float cx, float cy, float w, float h);
