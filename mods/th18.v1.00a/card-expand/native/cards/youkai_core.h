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

/* ================================================================= §3.2 本体外观（妖侧 = TH08 那五支本体脚本）
 * **一手**：TH08 的**人/妖两侧用的是同一台本体 VM，只换脚本号** —— 本体 VM（`player+0x10` 那一格）
 *   由 `SetAndExecuteScriptIdx` 换脚本：人侧 idle = **0** / 移动 = **1..4**，
 *   妖侧 idle = **5** / 移动 = **6..9**（同一个 `playerNN.anm`；`th08:0x44d650` 一带的注册 +
 *   `th08:0x44aec0` 的 `SET_MOVE_ANIM`）。**进出妖侧那一帧是显式设脚本**：
 *     进妖侧 `th08:0x44b12b` `push 0x5`（妖侧 idle）→ `th08:0x44b13a call 0x4069f0`
 *     出妖侧 `th08:0x44b385` `push 0x0`（人侧 idle）→ `th08:0x44b394 call 0x4069f0`
 *   两处后面都紧跟 `player+0xe2a9c = 0`（= `velocityX`，`th08:0x44b142` / `th08:0x44b39c`）
 *   ⇒ **下一帧必定触发一次移动动画状态机**。
 *   ⚠ vendor 反编译 `Player.cpp:942` 把「出妖侧」那份也印成 `5` —— **指令是 `push 0x0`**，
 *     以 `local/_scratch/th08_full.asm` 的 `th08:0x44b385` 为准。
 *
 * **TH18 侧一一对应**：`th18:0x45b610`–`0x45b7a8`（在 `Player__sub_45b170` 内）就是同一套状态机。
 *   逐条读出来的判据（`edx` = 本帧输入速度 x = `player+0x47934`，`[edi+0x47934]` = 上一帧那个值）：
 *     `dx < 0 && px >= 0` → 装脚本 **1**（起步向左，`th18:0x45b62a`）
 *     `dx == 0 && px < 0` → 装脚本 **2**（从左侧停下，`th18:0x45b766`）
 *     `dx > 0 && px <= 0` → 装脚本 **3**（起步向右，`th18:0x45b7a6` → `0x45b663`）
 *     `dx == 0 && px > 0` → 装脚本 **4**（从右侧停下，`th18:0x45b66a`）
 *     四条都不成立 ⇒ **不动**（引擎保持当前脚本）；开局 = 脚本 **0**（`th18:0x45a8e0 push 0x0`）。
 *   ⇒ 与 TH08 的 `SET_MOVE_ANIM(1,2,3,4)` **逐槽同序**（a=起步左 / b=从左停 / c=起步右 / d=从右停）
 *     ⇒ **妖侧脚本号 = 人侧 + 5**（idle 0 → 5），这正是 TH08 自己的编号法。
 *
 * **结构证据（第二条独立链）**：TH18 原生 `pl00.anm` 的 script3/4 与 script1/2 **同形状但用
 *   sprite 16..23**，而 TH08 的 3/4 用 `flipX()`；实测 `mirror(sprite8..15)` vs `sprite16..23`
 *   的 MAE ≈ 8（不镜像 27–54）⇒ **16..23 就是 8..15 的镜像**，即 TH18 把 TH08 的 `flipX`
 *   预先烤成了贴图 ⇒ 两作的 script0..4 是同一套「idle + 四个移动态」，可以逐槽对搬。
 *
 * 本卡的落点（**路 A**，owner 2026-09-22 裁决）：把 TH08 的妖侧脚本 5..9 **逐条追加进 `pl0X.anm`**
 *   （pl03 用 TH16 `pl03.anm` 的 0..4 = 射命丸文），低速时把本体 VM **整支换**过去、高速换回引擎那支。
 *   换脚本三步曲与理由见 `youkai.c` 的 `yk_body_load_script`。
 *   一手全文见 `engine/player/th08/03-youkai-body-and-gauge.md` §1 / §1.4。 */
#define YK_BODY_STATES        5      /* = 引擎那 5 支本体脚本（追加进 pl0X.anm 的就是它们）*/
#define YK_BODY_IDLE          0      /* 人侧脚本 0 / 妖侧脚本 5 */
#define YK_BODY_MOVE_L        1      /* 人侧 1 / 妖侧 6（起步向左）*/
#define YK_BODY_STOP_L        2      /* 人侧 2 / 妖侧 7（从左侧停下）*/
#define YK_BODY_MOVE_R        3      /* 人侧 3 / 妖侧 8（起步向右）*/
#define YK_BODY_STOP_R        4      /* 人侧 4 / 妖侧 9（从右侧停下）*/

/* 引擎那条移动动画状态机的**纯逻辑复刻**：返回本帧该用哪个状态（0..YK_BODY_STATES-1）。
 * `dx` = 本帧输入速度 x（`player+0x47934`）；`prev_dx` = 上一次调用时传进来的 `dx`
 * （引擎在同一个函数里先把它当「上一帧」读、再覆盖成本帧值 ⇒ 我们自己在卡状态里留一份即可）；
 * 四个条件都不成立 ⇒ 返回 `last`（引擎保持原脚本）。`last` 越界一律钳成 `YK_BODY_IDLE`。 */
int yk_body_state(int dx, int prev_dx, int last);

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

/* ====================================== ★★ 妖侧子机的**追敌 + 旋转**（TH08 灵梦那一支，2026-09-22 晚）
 * >> owner 单卡实跑（原话）：「th08 灵梦妖子机肯定是**八云蓝在敌人面前旋转**，甚至包括
 *    旋转的贴图和旋转动画（虽然只是 rotate）」+ 同轮第 2 条「添加的妖子机本体应当追着敌人动」。
 *
 * **一手**（`th08:0x44e3a0` 的子状态机 + `th08:0x44e8d0` + `th08:0x44e770` 尾巴 + `player00.anm` script18）：
 *   · 只有**机体 0（霊夢・八雲紫/藍）**有这一支：`g_OptionInitCallbacks[0][0] = th08:0x44e3a0`，
 *     机体 1/2 的每帧回调（`th08:0x44ea40` / `th08:0x44eb70`）**没有子状态机、也不旋转**。
 *   · 子状态 `option+0x2cc` 有 4 格（跳表 `th08:0x44e755` = {`0x44e484`,`0x44e532`,`0x44e5e0`,`0x44e68b`}）：
 *     0/1/2 = **跟随自机**（`th08:0x44e770` 弹簧，三格之间按**速度 vx 的符号**切换待机循环 A/B），
 *     **3 = 追敌**（`th08:0x44e8d0`）。
 *   · **追敌那一格的目标与运动**（`th08:0x44e8d0` 逐指令）：
 *       `target = *(void **)(player + 0xe2abc)`（一只敌人的指针；`0x44e8f1` 读它）
 *       `goal   = target->final_pos`（`zEnemy +0x2d88`，vendor 结构体表 `type-structs-own.json`）
 *       `goal.y += 32.0`（`0x4b42cc`）且 `if (goal.y < 32.0) goal.y = 32.0`
 *       然后 = **与跟随自机完全同一条弹簧**：`delta = (goal − pos) × (1/16)`；
 *       `vel += (delta − vel) × 0.2`；`pos += vel`；`|vel.x| < 0.05 ⇒ vel.x = 0`（死区）。
 *   · **进入追敌**（`th08:0x44e770` 尾巴，跟随自机时每帧判）：`Timer(&player+0xe2ac4)` 到期
 *       **且** `player+0xe2abc != 0` **且** `Timer(&option+0x2e0)` 满 **10 帧**（`0x44e892 push 0xa`）
 *       ⇒ `AnmVm::SetInterrupt(option, 3)`（`0x44e8a6`）+ 子状态 = 3（`0x44e8b3`）。
 *   · **退出追敌**（`th08:0x44e68b`）：目标没了 / 计时器到 0 ⇒ `player+0xe2abc = 0` +
 *       `SetInterrupt(option, 1)`（`0x44e6dd`）+ 子状态回 0（= 待机循环 A）。
 *   · **旋转是贴图层面的**（owner 口径对上）：`player00.anm` script18 的标签 3 = `sprite(sprite30)`
 *       + `angleVel(0, 0, 0.62831855f)` + `stop()` ⇒ **0.2π rad/帧 = 36°/帧，10 帧一圈**；
 *       标签 4 是反向（`-0.62831855f`）——⚠ 全 exe 里**没有任何一处**发标签 4（玩家侧发往子机 VM 的
 *       `call 0x407120` 逐处看过）⇒ 反向那一段在原版是死代码，本卡也不发。
 *
 * ⚠⚠ **一手与 owner 记忆的冲突（照实登记，别默默改）**：那条目标字段 `player+0xe2abc` 在**我们的样本**
 *   （TH08 v1.00d）里**全 exe 只有 3 处写点、写的都是 0**（`0x44e455`/`0x44e6d3`/`0x44e8c4`；
 *   同一字段的 10 处引用逐条按指令边界看过：7 读 + 3 写 0，`METHOD.md` 硬规矩 4）⇒ 严格地说，
 *   **追敌 + 旋转这一段在本 build 里够不到**（前一轮因此写了「不追敌」的结论）。
 *   owner 2026-09-22 晚口径改判：「肯定是八云蓝在敌人面前旋转」—— 代码形状（子状态 3 + 标签 3 的
 *   `angleVel` 旋转 + `sprite30`）与 owner 的记忆**逐条吻合**，矛盾的只有「谁写那个目标指针」这一格。
 *   ⇒ **本卡按一手的形状实现**，只把「目标从哪来」换成 TH18 上真拿得到的量：
 *     **先取 boss（`mgr+0x48`，可锁定的那只）**，没有 boss 时取**最近的敌人**（排除我们自己的使魔）
 *     —— 这一格标 ❓ 自定（登记 `TUNING.md` §2.0y），其余（弹簧式、+32、10 帧门、旋转标签、
 *     速度符号 ⇒ 循环 A/B）全部一手。*/
#define YK_OPT_CHASE_GOAL_DY   32.0f   /* `th08:0x44e8d0`：`goal.y += 32.0`（`0x4b42cc`），并夹 `>= 32.0` */
#define YK_OPT_CHASE_MIN_AGE   10      /* `th08:0x44e770` 尾巴：`Timer(&option+0x2e0)` 满 10 帧才准进追踪 */
#define YK_OPT_SPIN_LABEL       5      /* 我们 `pl0X.anm` 里那一支的**旋转**段（TH08 原号 3，映射见生成器）*/
#define YK_OPT_IDLE_LABEL       2      /* 我们那一支的**待机循环 A**（TH08 原号 1）*/

/* 追敌时的「家位置」= 目标坐标 + (0, `YK_OPT_CHASE_GOAL_DY`)，`y` 夹到 `>= YK_OPT_HOME_Y_MIN`
 * （逐条照抄 `th08:0x44e8d0`；`target_xy` = 那只敌人的 `final_pos`）。越界参数 ⇒ 返回 0、`out` 不动。*/
int yk_option_chase_goal(const float target_xy[2], float out[2]);

/* 本帧该不该在「追敌」态（纯逻辑，`th08:0x44e770` 尾巴那三个门里我们复刻得到的两个）：
 *   `target_present`（目标存在）+ `age_frames >= YK_OPT_CHASE_MIN_AGE`（子机出场够 10 帧）。
 * `age_frames` 越界（< 0）当 0。*/
int yk_option_chase_want(int target_present, int age_frames);

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
/* ★★ 2026-09-22（工单 E 第 2 条）：使魔 HP = 100，**一手取自 TH08 `familiarCreate` 的 life 参数**。
 *
 * **一手（全量，可复算）**：`thecl -d 8 -m th08.eclm` 反汇编 24 个 `ecldata*.ecl`，逐处取
 * `familiarCreate[A-Z]*("SubN", x, y, life, drop, score)` 的第 4 参 ⇒ **990 处**可有字面值：
 *   · 众数 = **100（159 处）**；其后 13500(90) / 300(68) / 1200(66) / 200(59) / 600(51) /
 *     5000(44) / 1000(37) / 4000(34) / 20(32) / 3000(30) / 700(30) / 500(28) / 1500(26) …
 *   · 只看**我们已经移植外观的那四套**（子程序的 `anmSet(K)`，K = 53..56）：**100 仍是众数（68 处）**
 *     （其后 1200(66) / 200(59) / 300(56) / 13500(40)…）—— 出处
 *     `engine/enemy/th08/01-familiars-and-anm.md` §4。
 * ⇒ **原作没有统一值**（同一波里各 `familiarCreate` 各给各的），本卡取**众数 100**。
 *
 * **为什么不照抄更大的那些档**（硬约束）：`EnemyManager__allocate_new_enemy` `th18:0x42d8f6` 在 spawn
 *   的 life ≥ 1000 时置 `flags_lo |= 0x40000000`（锁定黑名单 ⇒ 诱导打不到使魔）⇒ TH08 里 1200 / 13500 /
 *   5000 / 4000 这些档**在本卡的 spawn 路径上不可用**；可用的字面值里 100 就是众数。
 *
 * ⚠ **这条顺手救活了 1/2 转嫁**：`life = 1` 时 `yk_sample_life` 的每帧掉血至多 1，而
 *   `yk_half_with_carry(1)` = **0** ⇒ `tr.pending` 恒 0 ⇒ `ce_damage_rect` 那条转嫁源**从未被建过**
 *   （数值上是死代码）。100 点血让 §3.7 真正跑到。
 * ⚠ 仍是**数值设计项**（owner 可调）：只改这一行，`yk_clamp_spawn_life` 会拦住 >= 1000 的误填；
 *   已登记 `TUNING.md`。*/
#define YK_FAM_LIFE       100

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
    uint8_t  rearm_pending; /* ★★ 2026-09-22（工单 E / route A）：**「下一波发弹」请求**。
                          *   闸（`yk_shot_slot_gate`，在 `etOn` 那一刻被调用）夹住一个槽时置 1；
                          *   `yk_note_slot`（卡 tick）**同帧消费**它 ⇒ 清 `broken`/`fam_seen`/
                          *   `no_fam_frames` 并让 1:1 映射把使魔建回来。语义 = owner 6⑦ 的
                          *   「下一波发弹时重建」（TH08 的使魔就是每波 `familiarCreate` 重建的，
                          *   见 `engine/enemy/th08/01-familiars-and-anm.md` §3）。
                          *   归零口 = 消费 / `yk_fam_created` / 退休 / `yk_map_reset`。
                          *   ★ 位置在 `fam_seen` 与 `no_fam_frames` **之间**：正好吃掉
                          *     `uint16_t` 对齐留出的那 1 字节空洞 ⇒
                          *     **`sizeof(yk_slot_t)` 仍是 28 B**（单测 `== 28u` 盯着；放错位置会变 32）。*/
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
    uint32_t  rearms;    /* ★★ 2026-09-22（route A）：闸**掐掉过几次**（每个夹住的槽每置一次
                          *   `rearm_pending` 记一次，只增不清）。卡侧打一行日志 —— 它**在涨**就是
                          *   route A 生效的直接证据（对照 `heals`：TTL 那条现在应当基本不涨）。
                          *   `sizeof(yk_map_t)` = 32×28+8 = 904 B。*/
    uint32_t  releases;  /* ★★★ 2026-09-22（P8-3 / **route A′**）：闸**在第二问主动放行**的次数
                          *   （`yk_shot_slot_gate` 里看到 `rearm_pending` 已置 ⇒ 清夹持态并 return 0）。
                          *   与 `rearms` 成对：`rearms` 涨 = 掐过，`releases` 涨 = 恢复不再依赖卡 tick。
                          *   `sizeof(yk_map_t)` = 32×28+12 = 908 B（单测上限 1024 仍有余量）。*/
} yk_map_t;

unsigned   yk_map_heals(const yk_map_t *m);      /* 自愈计数（#7 记账；NULL ⇒ 0）*/
unsigned   yk_map_rearms(const yk_map_t *m);     /* 「掐掉过几次」计数（route A 记账；NULL ⇒ 0）*/
unsigned   yk_map_releases(const yk_map_t *m);   /* 「第二问放行」计数（route A′ 记账；NULL ⇒ 0）*/

/* ★★★ 2026-09-22（P1，实跑日志直证）：**只读**判据「这个 id 是不是我表里的使魔」。
 *
 * 为什么必须单独有它（**这条是一个真 bug 的根因**）：`yk_fam_broken` 虽然是 `int` 返回，
 *   但它**有副作用**（清 `fam_id`、置 `broken`）—— 它是**事件处理**，不是判据。
 *   卡侧「追踪目标解析」（`youkai.c` 的 `yk_chase_target_resolve`）曾经拿它当**纯谓词**用：
 *   `if (yk_fam_broken(&s_map, id)) continue;`（注释原文：我们自己的使魔不当目标）
 *   ⇒ 每帧、对**每一只可锁定的活使魔**都走了一次「使魔被打死」的处理：
 *     ① 表条目被清 `fam_id`（卡从此不认识那只使魔 ⇒ 那只**真敌人永久留在场上**：不再钉位置、
 *        不再随松 Shift 撤走 —— owner 报的「敌方使魔滞留」）；
 *     ② 跟随 VM 的记录**没人收**（那条记录只在 `yk_fam_vm_kill_id` / `…_kill_all` 里清）
 *        ⇒ 32 格 `s_fam_vm` 表泄漏，之后造的使魔 `ce_anm_spawn` 拿不到格子 ⇒
 *        **画不出来**（日志：`familiar follow-VM table full (32) - familiar 193/194/… stays invisible`）；
 *     ③ 槽被标 `broken` ⇒ 发射口闸夹上 ⇒ 下一次 `etOn` 走 route A 重建一只**新的**使魔
 *        （新的 `fam_id`）⇒ 泄漏①逐波累积。
 *   ⇒ 判据与事件从此分开：**判据走本函数（纯读）**，事件仍走 `yk_fam_broken` / `yk_fam_gone`。
 *     （`tests/test_youkai_core.c` 有一条断言专门钉「本函数不改表」。）*/
int        yk_map_is_fam(const yk_map_t *m, uint32_t fam_id);

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

/* ============================ ★★ §3.5.2 route A：「下一波发弹」= 闸的复位口（2026-09-22，工单 E）
 *
 * **为什么需要它**（一手根因，全文 `engine/enemy/th18/07-emitter-slots-waves-and-phase-transitions.md`）：
 * 闸原本的复位口是 `props[i].type` 的 **inactive→active 边沿**（= ECL 重新武装），但
 *   · TH18 的 ECL 里 `etNew(i)`（清零）与同槽 `etSprite(i,…)`（写回）**永远同帧**
 *     （`dat/` 里 ECL 全集 22 份、**282/282 处**下一句就是同槽 `etSprite`，中间没有一次 `wait()`）
 *   · 卡每帧只采样一次该字段（AbilityManager 节点 `0x16` < 敌人 ECL `0x1b`）
 * ⇒ **那个边沿在实战里观测不到**，`broken`/`fam_seen` 只能靠 `YK_EMIT_BROKEN_TTL` 到期才清；
 *   而 boss 每段攻击开头的 `enmKillAll()`（opcode 525 → `0x434e1b` → `0x430710`，杀所有
 *   `flags_lo & 0xc004a0 == 0` 的敌人 —— 我们的使魔正是）**必然**在换阶段那一帧把使魔清光 ⇒
 *   「击破 boss 第一个阶段后再不发弹」。
 *
 * **route A 的做法（不加站点）**：把复位口换成**引擎自己的发弹事件** —— 这个槽下一次 `etOn`
 * （就是本函数被调用的那一刻）就是「下一波发弹」。于是：
 *   · 夹住时：本帧**照旧掐**（`retaddr = 0x43402d`，一帧都不多发 ⇒ spec §9.6「打掉使魔 ⇒ 那个发射口
 *     停火」保持），同时置 `rearm_pending` 并给 `m->rearms` 记一笔；
 *   · 卡 tick 的 `yk_note_slot` **消费** `rearm_pending` ⇒ 清闸 + 同帧按 1:1 建回使魔；
 *   · 下一次 `etOn`（通常就是下一帧）问闸时使魔已活 ⇒ 放行。
 * ⇒ 每个使魔的「停火窗口」从**至多 120 帧**缩短成**至多 1 条 `etOn`**（这正是「下一波重建」）。
 *
 * ⚠ 口径（写给读的人）：本函数是 `yk_shot_slot_suppressed` 的**带副作用版**，卡侧只调本函数；
 *   纯判据那支保留给单测与「只想问、不想记」的场合。
 * ⚠ 与 `YK_EMIT_BROKEN_TTL`（120 帧到期自愈）的分工：**route A 是主路**（波次级，正常游戏里总会有
 *   下一次 `etOn`）；TTL 是**兜底**（ECL 再也不 `etOn` 这个槽、或 `etOn` 永远被更前面的卡掐掉时）。
 *   两者都清同一组标志，幂等。 */
int        yk_shot_slot_gate(yk_map_t *m, uint32_t enemy_id, int slot, uint32_t have_fam_id);

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

/* ==================================================== §3.10 人妖槽（TH08 的 `youkaiGauge`）
 * 一手全文：`engine/player/th08/03-youkai-body-and-gauge.md` §2（2026-09-22 订正版）。
 *
 * 它是**一个 i16 标量**（TH08 落在 `[GAME_MANAGER+8]+0x22`）：
 *   · **负 = 人侧、正 = 妖侧**（人侧判据是 `≤` 一个负阈值）；量程 ±10000。
 *   · 唯一入口 `GameManager::AddToYoukaiGauge(delta, unconditional)` `th08:0x43c0bb`：
 *     `gauge += delta` 然后**夹到 `[+0x3ddf8, +0x3ddfa]`**，并写显示副本 `+0x20`。
 *     ⚠ 第一道闸：`ds:0x17d6ed4 != 0 && delta == 0 的调用` 被整条丢掉 —— 那条是「符卡中冻结」，
 *       ❓ 语义未钉（名片场），**本卡不实现**（登记见报告）。
 *   · **6 个限值字段**（`th08:0x44d9ee`–`0x44da8f` 一手写点，本卡侧自建）：
 *     human_limit −10000 / youkai_limit +10000 / human_effects −8000 / youkai_effects +8000 /
 *     blue_tint −2000 / red_tint +2000。TH18 **没有这套字段**（见该文 §2.5 的三条负证据）。
 *   · 4 个判定（`th08:0x406d10/40/70/a0`，一律 `setle`/`setge`，**取等号**）：
 *     极人 `g <= −8000`、中人 `g <= −2000`、极妖 `g >= +8000`、中妖 `g >= +2000`。
 *   · TH08 里还有「死区归零」（`th08:0x44bf61`，前置 `fabs(gauge) <= 9.0` ⇒ 写 0）与
 *     「死亡清零」（`th08:0x44ac09` 调 set_youkai_gauge(0)）。 */
#define YK_GAUGE_SPAN             10000   /* = ±limit（HUD 的量程：条宽 112 px / 图标 56 px 都是相对它算的）*/
#define YK_GAUGE_HUMAN_LIMIT     (-10000) /* `th08:0x44d9ee` 0xd8f0 */
#define YK_GAUGE_YOUKAI_LIMIT      10000  /* `th08:0x44da09` 0x2710 */
#define YK_GAUGE_HUMAN_EFFECTS    (-8000) /* `th08:0x44d9f7` 0xe0c0 → 极人判定 */
#define YK_GAUGE_YOUKAI_EFFECTS     8000  /* `th08:0x44da12` 0x1f40 → 极妖判定 */
#define YK_GAUGE_BLUE_TINT        (-2000) /* `th08:0x44da00` 0xf830 → 中人判定 */
#define YK_GAUGE_RED_TINT           2000  /* `th08:0x44da1b` 0x7d0  → 中妖判定 */
#define YK_GAUGE_DEAD_ZONE            9   /* `ds:0x4b6f28` = 9.0：`|g| <= 9 ⇒ 0`（`th08:0x44bf4d`/`:0x44bf61`）*/
#define YK_GAUGE_PER_ORB            111   /* 收一颗刻符的人妖槽增量（`th08:0x44139e`/`:0x4413b0`）—— 聚焦 = +，否则 − */

/* ★★★ 2026-09-22 夜（owner 工单 **P-E 单人模式的反向封顶**）—— 本 mod 恒单人 ⇒ 必须实现。
 *
 * owner 口径（`HUMAN-YOUKAI-GAUGE.md` §1.8）：「人间状态的单人模式只能达到**人间逢魔 100%**，
 *   妖怪状态的单人模式只能达到**妖怪逢魔 100%**，其相反方向的逢魔**不可达到，只能到 20%**
 *   （单人妖梦例外，两侧都能到 50%）」。
 *
 * **一手（我们样本，逐指令；`Player::on_registration th08:0x44d650`）**：
 *   · 门：`th08:0x44da99 call 0x42f230`（`solo_human` = `shottype >= 4 && (shottype & 1) == 0`）/
 *     `th08:0x44dac4 call 0x42f270`（`solo_youkai` = `shottype >= 4 && (shottype & 1) == 1`）。
 *   · 单人 ⇒ 把**反侧 limit** 收成 **±2000**：`th08:0x44daa2 mov WORD [0x164d302],0x7d0`（+2000，
 *     妖侧）/ `th08:0x44dacd mov WORD [0x164d300],0xf830`（−2000，人侧）；同段还有 **±2001 的哨兵**
 *     （`th08:0x44dab4` 写 `0x7d1` / `th08:0x44dae6` 写 `0xf82f` —— 那是「反向那一格」的极限+1，
 *     用来保证比较取等号时也到不了）。
 *   · 而 **effects（逢魔线 ±8000）不动** ⇒ 反侧**永远到不了 ±8000** ⇒ 反侧逢魔不可达 ✔ 与 owner 一致。
 *   · 单人妖梦（shottype **10**，`th08:0x44da57`）：两侧 effects 都收到 ±3000、limit ±5000 ⇒ 两侧都能逢魔
 *     —— 但 **TH18 没有妖梦这个机体** ⇒ 本卡按默认（±8000 / 单人 2000 封顶）落，**不发明例外**（规格 §4）。*/
#define YK_GAUGE_SOLO_OTHER_LIMIT   2000   /* 一手 `th08:0x44daa2` 0x7d0 / `th08:0x44dacd` 0xf830 */

/* 「当前形态」的同一对 limit（`focused` = 低速 = 妖侧）：
 *   妖侧（focused）⇒ human_limit = **−2000**、youkai_limit = **+10000**；
 *   人侧         ⇒ human_limit = **−10000**、youkai_limit = **+2000**。
 * 指针一律 `NULL` 安全（任一为 NULL 就跳过那一项）。 */
void yk_gauge_limits_solo(int focused, int32_t *human_limit, int32_t *youkai_limit);
/* 夹到给定的一对 limit（`yk_gauge_clamp` 的一般化；一手比较口径同 `th08:0x43c0fa`/`:0x43c131`）。*/
int  yk_gauge_clamp2(int gauge, int32_t human_limit, int32_t youkai_limit);
/* 夹紧后的加法（用给定的一对 limit）。 */
int  yk_gauge_add2(int gauge, int delta, int32_t human_limit, int32_t youkai_limit);

int yk_gauge_clamp(int gauge);                 /* 夹到 [ HUMAN_LIMIT, YOUKAI_LIMIT ]（`th08:0x43c0fa`/`:0x43c128`）*/
int yk_gauge_add(int gauge, int delta);        /* 夹紧后的加法（= AddToYoukaiGauge 的算术部分）*/
int yk_gauge_dead_zone(int gauge);             /* `|g| <= 9 ⇒ 0`（`th08:0x44bf61`）*/
int yk_gauge_is_extremely_human(int gauge);    /* `g <= −8000` */
int yk_gauge_is_moderately_human(int gauge);   /* `g <= −2000` */
int yk_gauge_is_extremely_youkai(int gauge);   /* `g >= +8000` */
int yk_gauge_is_moderately_youkai(int gauge);  /* `g >= +2000` */
int yk_gauge_orbs_delta(int focused);          /* 收刻符时该加多少（聚焦 +111 / 否则 −111）*/

/* ★★★ 2026-09-22（P3 第二批）：TH08 人妖槽的**第四条涨源** + `AddToYoukaiGauge` 的**第一道闸**。
 *
 * **一手（逐条，样本 + decomp 双侧）**：
 *   · `Player.cpp:445-452`（`Player::UpdateShooting` 里 `shotActive != 0` 那一支，本样本
 *     `th08:0x44c650`）：`if (shotType < 4) { shotType & 1 ? +X : −X }`，`X = 0x6590 / shotInterval`；
 *     `shotInterval` 在 `Player.cpp:541` 被写死 `0x3e7`（999）⇒ **X = 26000/999 = 26**（整数除法）。
 *     `shotType = isYoukaiMode`（`Player.cpp:500`；B 型再 `+= 2`）⇒ `shotType & 1` **就是人/妖侧**。
 *     第三个参数 `param_2 = 1` ⇒ 这一条**不过闸**（下一行）。
 *   · `GameManager::AddToYoukaiGauge`（`th08:0x43c0bb`）开头那道闸（逐指令）：
 *     `0x43c0c2 cmp DWORD PTR ds:0x17d6ed4,0` → `0x43c0cb cmp DWORD PTR [ebp+0xc],0` ⇒
 *     **非 0 且 `param_2 == 0` ⇒ `jmp 0x43c15b`（整条丢掉）**。`0x17d6ed4 = g_Player + 0xfdc`
 *     （`reccmp-globals.csv` 的 `g_Player = 0x17d5ef8` + `Player.hpp:190` 的偏移注释）
 *     = **`Player::shotActive`**（射击进行中）⇒ 旧注「符卡中冻结」是误读。
 *     ⇒ 含义：**射击中，击破 ±200 与收刻符 ±111 都不进槽**，只有「射击中每帧 ±26」在动。
 *
 * **TH18 侧的映射（❓ 口径，登记 `TUNING.md` §2.0y）**：TH18 没有 `shotActive` 这个状态量
 *   （它的射击是每帧按 rate 发，没有 TH08 那种 999 帧的「射击周期」）⇒ 本卡用
 *   **「射击键按住」**（`CE_INPUT_HELD & 0x1`）当判据，卡侧每帧把它传给这两个纯函数。*/
#define YK_GAUGE_SHOOT_DELTA 26     /* ★ **放符卡/炸弹期间**每帧的推进量（= 0x6590(26000)/999）
                                     *   ⚠ 2026-09-22 夜订正：这条**不是「普通开火」**（见下）。*/
/* ★★★ 2026-09-22 夜（P-C 订正，owner 裁「按一手」）：**持续开火推进 = 时间斜坡**，不是恒定 ±26。
 *
 * 一手（`Player::move th08:0x44aec0`，唯一写点 `th08:0x44bedf call 0x43c0bb`）：
 *   `th08:0x44be67` 取 `player+0xe2ae8`（**连续推进帧数**）→ `fcomp 300.0f`（`th08:0x4b6f30`）
 *   → 封顶 `21.0f`（imm `0x41a80000`）→ `fdiv 15.0f`（`th08:0x4b6e94`）→ float→int ⇒
 *   **`delta = floor(min(帧数, 300) / 15)`：0→1→…→21，第 301 帧起恒 21**；方向 = `player+3`（人侧取负）。
 *   清零时机：**换形**（`th08:0x44b1ac` / `:0x44b3fe`）与**停火 ≥4 帧**（`th08:0x44bf04 push 4`）。
 *   另有 `player+0xe2ad0`（**连停火帧数，上限 30**）：> 0 时推进支**只减不加**（`th08:0x44be5d`，预热）。
 *   ⇒ **一手没有 10%/25% 的位置门槛**（owner 那条在 v1.00d 里不存在）。
 *
 * ⚠ `shotActive`（`player+0xfdc`）**不是「开火」，是「正在放符卡/炸弹」**（`th08:0x44be0f` 用它跳过推进支；
 *   我们的对应读数 = `th08` 无、**TH18 的 `[bomb_mgr+0x30]`**）。⇒ 放符卡期间只有 `YK_GAUGE_SHOOT_DELTA`
 *   那一项在动（`th08:0x44c820` 一族），普通开火走上面的斜坡。*/
#define YK_GAUGE_RAMP_DIV        15   /* `th08:0x4b6e94` = 15.0f */
#define YK_GAUGE_RAMP_CAP        21   /* imm `0x41a80000` = 21.0f（封顶）*/
#define YK_GAUGE_RAMP_FRAMES    300   /* `th08:0x4b6f30` = 300.0f（帧数上限，301 帧起恒 21）*/
#define YK_GAUGE_IDLE_MAX        30   /* `player+0xe2ad0` 的上限（`th08:0x44c007`）*/
#define YK_GAUGE_STOP_CLEAR       4   /* `th08:0x44bf04 push 4`：停火 ≥4 帧 ⇒ 推进帧数清零 */

/* 射击中每帧的槽增量（把当前形态推得更极端）：低聚（妖侧）= +26 / 否则 −26。
 * `focused` = 本卡口径的「妖侧」（= 低速位）。*/
int yk_gauge_shoot_delta(int focused);
/* **持续开火推进**：`floor(min(帧数, 300)/15)`，封顶 21（一手 `th08:0x44be67`–`0x44bea8`）。*/
int yk_gauge_ramp_delta(int adv_frames);
/* **停火回中**的每帧量（一手 `th08:0x44bf70`–`0x44bfde`）：`|g| >= 8000 ⇒ ∓5`、`>= 2000 ⇒ ∓3`、`g > 0 ⇒ ∓2`、
 * 反向同理 ⇒ 返回**带符号**的 delta；死区由调用方先判（一手 `th08:0x44bf5a` 直接 `set 0`）。*/
int yk_gauge_center_delta(int gauge);
/* `AddToYoukaiGauge` 的第一道闸：返回 1 ⇒ **本帧 `param_2 == 0` 的加数全被丢掉**
 * （击破 ±200 / 收刻符 ±111）。`shooting` = 卡侧读到的「射击中」。*/
int yk_gauge_gate_blocks(int shooting);
/* 百分比文字/槽条的染色（一手 `AsciiManager.cpp:1035-1058`）：极人 0x7070ff、中人 0xb0b0ff、
 * 极妖 0xff7070、中妖 0xffb0b0、其余 0xffffff（每通道 8 位，返回 0xRRGGBB）。*/
uint32_t yk_gauge_color(int gauge);

/* HUD 几何（**全部一手**，`AsciiManager::InitializeVms` + `AsciiManager::on_draw`）：
 * 640 空间的一手值 —— 槽条左上 **`(32,449)`** 128×16、人间图标 `(32,449)`、妖怪图标 `(144,449)`
 * （都是 16×16，三个 VM 都 `anchorTopLeft()`）、游标中心 `x = 96 + g·112/20000`、`y = 453`
 * （8×12，**没有** anchor 调用 ⇒ 锚点 = 心）、百分比数字 `x = 96 + g·80/20000`、`y = 游标 y − 7 = 446`。
 * ⚠ **槽条那个 y 是 449、不是 465**（2026-09-22 晚订正）：`ascii.anm` script5 的正文是
 *   `pos(32, 465)` → `posTime(40, 0, 32, 449, 0)` ⇒ **465 只是滑入动画的起点（在 480 高的屏外）**，
 *   停稳位置是 **449**。原来本卡写 465 ⇒ 槽条比两个图标/游标低 16 px（owner 报的「hud 不对」之一）。
 * 图标 x 由**限值**决定：`88 + limit·56/10000` ⇒ 本卡的 ±10000 给出 32 / 144（= 条的两端）。 */
/* ★★★ **全部输出都是表面 px（= 1 单位 = 1 屏上像素）** —— 2026-09-22（P2）**实跑定案**，
 *   订正 2026-09-22 晚那一版（它按「640 空间」输出，**改反了**）。
 *
 * **一手（`th18:0x4063d0` = VM 的「位置/尺寸 → 屏幕」那一步，逐指令）**：
 *   ```
 *   4063e8  mov  eax,[edi+0x538]        ; edi = vm
 *   4063ee  and  eax,0x1c00000          ; = resolutionMode 的 3 位
 *   4063f3  cmp  eax,0x400000  / je → 406421 : xmm1 = [0x56aca0]        ; mode 1 → ×世界倍率（2）
 *   4063fa  cmp  eax,0xc00000  / je → 406421 : 同上                       ; mode 3 → 同上
 *   406401  cmp  eax,0x800000  / je → 40640f : xmm1 = [0x56aca0]×[0x4b9138] ; mode 2 → ×(2×0.5) = ×1
 *   406408  cmp  eax,0x1000000 / jne → 406451: **mode 0（不写）⇒ 一个字节都不乘 = ×1**
 *   406429  movss xmm0,[esi] / mulss xmm0,xmm1 / movss [esi],xmm0 …    ; x/y/z 三分量逐一乘
 *   ```
 *   `[0x4b9138]` = **0.5**（`.rdata` 直读 4 字节 `00 00 00 3f`）、`[0x56aca0]` = 世界倍率（1280×960 下 2.0）。
 *   同一判定在**尺寸**那一支**逐位同形**（`th18:0x4817c6`：mode1 → ×2、mode2 → ×1、**mode0 → 直通**）。
 *   ⇒ **mode 0 与 mode 2 在 1280×960 下等价（都是 ×1 = 表面 px）**；`HUD-LAYOUT.md` §4.1b 里
 *     「不写 `resolutionMode` ⇒ 坐标 ×2」那一行**与这段指令不符**（本卡按指令 + 实跑定案，见下）。
 *
 * **实跑判据（决定性，2026-09-22 那次 `th18_card_expand.log`）**：那一跑装的 DLL 是
 *   `ea93d66e0957c0d39dc8dd29d4e84f2a`（= 18:13:42 的发布，也就**正是**写 640 空间那一版）
 *   ⇒ owner 报「百分比**高了半个屏幕**」（#14：我们把 `text_y` 写 427，画面上就在 427 ⇒
 *     正是 854 的一半）＋「人妖 HUD **整体缩小 50%**」（#15：16×16 的原生贴图按 16 px 画 = 原版的一半）
 *   ⇒ **×1 生效**（若真按「不写 ⇒ ×2」，位置会落在 854、尺寸会是 32 px，两条抱怨都不会出现）。
 *   ⚠ 这条**同时**解决了本卡内部的自相矛盾：HUD 货架块一直按**表面 px** 报（`YK_HUD_BLOCK_W 256` =
 *     128 个 640 空间 px ×2）—— 只有 `yk_gauge_hud()` 的输出被改成 640 空间，于是「块 256 宽、
 *     内容 128 宽」；现在两者一致。
 *   ⚠ 若将来要改成「脚本显式写 `resolutionMode(2)` + 表面 px」，数值**一个都不用动**（1280×960 下
 *     mode 2 也是 ×1）；但**别**写 `resolutionMode(1)`（那才会真 ×2 ⇒ 整块跑到屏外）。*/
typedef struct {
    float bar_x, bar_y;             /* 槽条 VM 位置（**表面 px**，左上；VM 用 anchorTopLeft）*/
    float human_x, human_y;         /* 人间图标（同上，anchorTopLeft）*/
    float youkai_x, youkai_y;       /* 妖怪图标（同上，anchorTopLeft）*/
    float cursor_x, cursor_y;       /* 游标（同上，**锚点 = 心 ⇒ 这是中心点**）*/
    float text_x, text_y;           /* 百分比数字的**基准点**（表面 px；逐字形的偏移见下）*/
    float night_x, night_y;         /* ★ 2026-09-22 夜：#27/#28 的**夜点数字**左对齐基准点（表面 px）
                                     *   一手：y=460（= 槽条 449 + 11）、x 从 80 起、7 px/字、
                                     *   **前导零不画也不占位**（`th08:0x405d11`–`0x405e01`）。
                                     *   ⚠ 与百分比（y=446）**上下错开 14 px**（640 空间）。*/
    float px;                       /* ★ 本帧的「1 个 640 空间单位 = 多少表面 px」（TH18 = `1/to640` = 2）
                                     *   —— 字形偏移、字形 scale、以及四件贴图的 scale 都要乘它：
                                     *   贴图是 TH08 的**原生尺寸**（16×16 / 128×16 / 8×12），
                                     *   在 1280×960 上要 ×2 才与原版同观感（#15「缩小 50%」的修法）。*/
} yk_gauge_hud_t;

/* `anchor[2]` = HUD 块的**左下角**（**表面 px**，= `ce_hud_slot_t` 的 x/y）；
 * `to640` = 表面→640 的比例（= `CE_HUD_SURFACE_TO_640`，TH18 是 0.5）⇒ 内部倍数 `px = 1/to640 = 2`：
 * 一手几何是 640 空间的字面量（TH08 的 `ascii.anm`），**一律先 ×px 换成表面 px 再落到 VM 上**。*/
void yk_gauge_hud(int gauge, const float anchor[2], float to640, yk_gauge_hud_t *out);

/* ---- ★★ 人妖槽的**百分比数字**：逐字形布局（一手 `AsciiManager::DrawPercentage` `th08:0x405e10`）----
 * 一手形状（TH08 用**同一个 VM** 反复换 sprite + 改 pos/scale 再画，见 `AsciiManager.cpp:1095-1225`）：
 *   `n = 4 (+1 负号) (+3 若 ≥10000 / +2 若 ≥1000 / +1 否则)`；起始 `x -= n*3.5 − 3.5 − 4.0`（居中）
 *   负号 `'-'` 步进 7；`≥10000` ⇒ `"100"`(各 7) + `'.'`(5) + 两个小号 `"00"`(5,5,7)；
 *   `≥1000` ⇒ 两位整数(7,7) + `'.'`(5) + 两个小号(5,7)；否则 ⇒ 一位整数(7) + `'.'`(5) + 两个小号(5,7)；
 *   最后 `'%'`。**小号 = scale 0.8 且 y += 2**（画完复位）。
 * 字形号（= TH08 的 sprite 0x88..0x94 相对序）：`0..9` = '0'..'9'、`10` = '%'、`11` = '.'、`12` = '-'。*/
#define YK_GAUGE_GLYPH_PERCENT 10   /* sprite 0x92 */
#define YK_GAUGE_GLYPH_DOT     11   /* sprite 0x93 */
#define YK_GAUGE_GLYPH_MINUS   12   /* sprite 0x94 */
#define YK_GAUGE_TEXT_MAX      8    /* 最多 = '-' + "100.00" + '%'（`abs(gauge) >= 10000` 且为负）*/
typedef struct {
    uint8_t glyph;   /* 0..12（上面三个宏 + 数字）*/
    float   dx;      /* 相对 `text_x` 的 x 偏移（**640 空间 px**，已含居中 —— 落到 VM 前要 × `px`）*/
    float   dy;      /* 相对 `text_y` 的 y 偏移（同上；小号位 = +2）*/
    float   scale;   /* 1.0 / 0.8（小号两位）——落到 VM 前要 × `px`（贴图是原生尺寸）*/
} yk_gauge_glyph_t;
/* 把 `gauge`（−10000..+10000）拆成一串字形；返回字形数（1..`YK_GAUGE_TEXT_MAX`）；
 * `out` 为 NULL 或 `gauge` 越界（钳到 ±10000 后照算）都不会崩。**无浮点库调用**。*/
int yk_gauge_text_glyphs(int gauge, yk_gauge_glyph_t *out);
/* ★★★ 2026-09-22 夜（#27/#28）：**夜点数字的字形**（一手 `AsciiManager::OnDrawHighPriImpl th08:0x405d11`–`0x405e01`）。
 * 布局 = **左对齐、7 px/字、前导零不画也不占位**（一手：每画出一位才 `pos.x += 7.0`）⇒ `dx = i*7`、`dy = 0`、`scale = 1.0`。
 * 返回位数（1..`YK_NIGHT_TEXT_MAX`；`night <= 0` ⇒ 1 位 '0'）。**无浮点库调用**。*/
#define YK_NIGHT_TEXT_MAX 8
int yk_night_text_glyphs(int32_t night, yk_gauge_glyph_t *out);

/* ============================================== §3.11 永点（刻符）→ PIV（= 额外最大得点）
 * 一手：`GameManager::sub_418220` `th08:0x418220` 与 `Item::collect_time_orb` `th08:0x4412b0`。
 *
 * ```
 * 418241  current_time_orbs(+0x3c) += delta
 * 41825c  total_time_orbs(+0x44)   += delta          ; ★ 先加
 * 418288  if (delta <= 0) 跳出                        ; `jle`（41828c）—— **负增量不碰 PIV**
 * 418294  delta += (total_time_orbs & 1)             ; ★ 用**加完之后**的奇偶
 * 4182ab  cdq / sub / sar 1                          ; C 整除 2
 * 4182b0  eax *= 10
 * 4182bc  PIV(+0x24) += eax                          ; PIV 就是点道具分值的**基数**（`th08:0x440e50`）
 * ```
 * 逐颗（delta = 1）呈 **+10 / 0 / +10 / 0 …**（总数为奇数那颗 +10）⇒ 平均 +5/颗。
 * 第二条账（**SC 捕获结算**，`th08:0x41731e` + `:0x41732c`–`:0x41733d`）：
 * `AddTimeOrbs(count)` 之后**再** `PIV += [zSpellcard+0x10] × 10` —— 两笔都算（用户 2026-09-18 口径：
 * 「有意无意就按照复刻即可」，不要替 ZUN 去重）。 */
void    yk_piv_reset(int32_t *piv, int32_t *orbs);
/* `delta` 颗刻符入账（本卡只有 `delta = 1` 与死亡罚分的负数；负数**不动** `*piv`，只动计数）。*/
void    yk_piv_add(int32_t *piv, int32_t *orbs, int32_t delta);
/* SC 捕获那份额外账：`*piv += count × 10`（`th08:0x41732c`）。*/
void    yk_piv_spell_bonus(int32_t *piv, int32_t count);
/* 刻符的道具分值（`Item::collect_time_orb` 的分数那一段，`th08:0x4412c3`–`0x4412ee`）：
 * `stage_point_items >= 2000 ⇒ 10000`，否则 `max(100, (point_items / 2) × 10)`。 */
int32_t yk_orb_score(int32_t stage_point_items_in_game, int32_t point_items_in_game);
#define YK_ORB_SCORE_MIN        100
#define YK_ORB_SCORE_MAX        10000
#define YK_ORB_SCORE_STAGE_GATE 2000

/* ================= ★★ 刻符（type 7）的**运动**常量（一手，2026-09-22 晚 owner 第 4 条）=========
 * >> owner 原话：「永点的**吸收速度**不对，很慢，像是有一个很小的加速度似的，**应该刚开始就满速**」。
 *
 * 一手（`engine/item/th08/01-time-orbs.md` §2/§7 + 反编译 `ItemManager.cpp:180-305`，逐条地址）：
 *   · **出生状态看来源**（`ItemManager::spawn_item` 的第三个参数）：
 *       「击破敌机」`th08:0x42b048` 与「自机弹打死敌机」`th08:0x451892` ⇒ `state = 3`（先上浮）；
 *       **擦弹** `th08:0x431650` 一族 / **消弹** `th08:0x4308e5` 一族 / 连锁尾巴 ⇒ `state = 1`
 *       ⇒ **一生成就归航、当帧就是满速**（没有加速段）。
 *   · **归航速度 = 当前位置那一份 `.sht` 头里的 `itemCollectSpeed`**：`th08:0x44088e` 读
 *       `[ds:0x18b896c]+0x14`（`0x18b896c` = 已装载的 `PlayerRawShtFile*`），另一头
 *       `th08:0x44c1b0` = `Player::AngleToPlayer`（反编译 `Player.cpp:121` 的 `atan2f`）拿方向 ⇒
 *       `th08:0x4408a5 call 0x4286e0` 用 (角度, 模长) 造速度 Float3 ⇒ **每帧重设成满速、无加速度**。
 *       实测 `+0x14`：`ply00as`（霊夢・紫）**10.0** / `ply01as`（魔理沙・アリス）**12.0** /
 *       `ply02as`（咲夜・レミリア）10.0 / `ply03as`（妖夢・幽々子）10.0（本仓 `dat/ply*.sht` 直读）。
 *   · **收集盒**（`Player::CalcItemBoxCollision` `Player.cpp:100`）：拿道具的 AABB（自身尺寸的一半）
 *       与玩家的「抓取盒」相交。抓取盒 = `player+0x3ec` ± 各轴（`Player.cpp:1059`），其值 =
 *       **`[.sht+0x18] / 2`**（`th08:0x44d83f`–`0x44d873`）= `itemCollectRadius/2`：
 *       pl00/pl02/pl03 = **12.0**、pl01 = **16.0**；再叠加道具自己的半宽（16×16 的贴图 ⇒ 8）
 *       ⇒ 我们那套「半径」口径下，一手的等效值 = **20 / 24 / 20 / 20**。
 *   · **下落（state 0）**（`th08:0x440936` 一族 + 反编译 `ItemManager.cpp:268-276`）：
 *       `vx = 0`、`vy` 夹到 `>= ds:0x4b5b2c (−2.2)`、每帧 `vy += ds:0x4b44c0 (0.03)`、
 *       终速 `0x40400000 (3.0)`；**掉到 `arcadeRegionSize.y + 16` 以下就整颗删掉（不给分）**
 *       （`ItemManager.cpp:271-277`：`state == 0 && arcadeRegionSize.y + 16 <= pos.y ⇒ Delete()`）
 *       —— 这条**不是**「落到回收线就收」！回收是**接触抓取盒**（上一条），掉出下沿是**损失**。
 *       TH18 的游玩区高同为一手 **448.0f**（`sdk_core.h` 的 `CE_POC_HEIGHT_MAX_BITS`）⇒ 下沿 = 464.0。
 *   · 中弹/暴毙那一刻：`state 1 → 0`，`vx = 0`、`vy = −0.9`（`th08:0x441574` 批量版）/ `−0.7`（`th08:0x4408e0` 每帧版）。*/
#define YK_ORB_STATE_FALL       0      /* 一手 `ITEM_STATE_DEFAULT`：普通下落 */
#define YK_ORB_STATE_HOMING     1      /* 一手 `ITEM_STATE_UNK1`：归航（当帧满速）*/
#define YK_ORB_STATE_SPAWN      3      /* 一手 `ITEM_STATE_UNK3`：出生上浮（不可收）*/
#define YK_ORB_FALL_GRAVITY     0.03f  /* `ds:0x4b44c0`（`th08:0x4409c8`）*/
#define YK_ORB_FALL_TERMINAL    3.0f   /* `0x40400000`（`th08:0x4409bb`）*/
#define YK_ORB_FALL_VY_MIN     (-2.2f) /* `ds:0x4b5b2c`（`th08:0x44091c`）*/
#define YK_ORB_LOST_Y           464.0f /* `arcadeRegionSize.y (448) + 16`（`ItemManager.cpp:271`）*/

/* 归航速度（px/帧）= 一手 `.sht` 头的 `itemCollectSpeed`（+0x14）、收集半径（px）= 抓取盒半宽
 * （`itemCollectRadius/2`）+ 刻符贴图半宽 8。`character` = TH18 机体号：
 *   0 霊夢（TH08 pl00）speed 10.0 / r 20.0  ·  1 魔理沙（pl01）**12.0 / 24.0**
 *   2 咲夜（pl02）10.0 / 20.0  ·  3 早苗（pl03）10.0 / 20.0；越界（含 < 0）取 0 号那一档。
 * ⚠ **出口是 `float *` 而不是返回值**：本仓 32 位目标的 `.c` 里**返回 float 的函数必然走 x87**
 *   （i386 ABI 的浮点返回值在 ST(0)），`make dllx87` 门会当场红 —— 所以这里照 `yk_opt_spring_step`
 *   的先例用**指针出口**。任一指针为 NULL ⇒ 跳过那一项；两个都 NULL ⇒ 什么都不做。*/
void yk_orb_motion_params(int character, float *home_speed, float *pick_r);

/* ============ ★★★ §3.11b 击破符卡的「螺旋撒点」（一手参数，见 engine/item/th08/01-time-orbs.md §5.3）===
 * owner 口径：「击破符卡会有一个**螺旋式**的撒永点，无敌一段时间后自动满速吸收」。
 *
 * 一手（`Spellcard::sub_416b10` `th08:0x416b10`，喷点主体 `0x417533`–`0x4176e1`，逐指令）：
 *   · **门**：`Timer(owner+0x338)` 命中 **8** ⇒ **每 8 帧一批**（`0x41751a push 8` → `0x41752e call 0x4066c0`）；
 *   · **角度**：`θ(t) = ((t − 10) × 2π / 40) − π/2`，`t` = 那个计时器的帧数
 *     （常量 `0x4b4390` = **10.0**、`0x4b4460` = **2π**、`0x4b432c` = **40.0**、`0x4b4468` = **π/2**）
     *     ⇒ **每帧 9°**（40 帧一圈）、`t = 10` 时指向**正上方**；
 *   · **半径**：**128.0**（`0x417593 push 0x43000000` → `0x4286e0` 造 (角度, 模长) 的速度向量）；
 *   · **绕谁**：绕**符卡所有者（boss）的位置**（`[owner+0xf8] + 0x2e0` 再 `0x410a70` 向量加）；
 *   · **每批颗数**：`k = min(spellcard[+0x10], 7)` **同一角** ＋ **固定 6 颗 +π**（第二段角 = 第一段 +π，
 *     `0x417639 push 0x40490fdb` = **π**；⚠ 本文旧版曾写「+π/2」，已订正）⇒ 一批 **13** 颗；
 *     额度两段扣减：`spellcard[+0x10] -= k`（`0x41762d`）、再 `-= 6` 并夹到 0（`0x4176e1`）；
 *   · **出生态**：`spawn_item(pos, type 0xa, state 0)` → 被 `0x440124` 改写成 **type 7 + state 5**
 *     （= 我们的 `YK_ORB_STATE_SPAWN`：先上抛、`vy > 0` 才转归航；上抛期间**进不了收集判定**）。
 *
 * 下面两个纯函数把「角度」与「每批颗数」抽出来，便于主机单测与卡侧共用（无浮点库调用）。*/
#define YK_SPIRAL_BATCH_FRAMES 8        /* `0x41751a push 8`：每 8 帧一批 */
#define YK_SPIRAL_FIRST_MAX    7        /* `0x4175cc cmp ..,7 / jle`：同角一批至多 7 颗 */
#define YK_SPIRAL_SECOND_N     6        /* `0x4176be cmp [ebp-8],6 / jge`：第二段固定 6 颗（+π）*/
#define YK_SPIRAL_RADIUS       128.0f   /* `0x43000000`（`0x417593`）*/

/* `t` = 该计时器的帧数（我们从 10 起算，见卡侧）。返回弧度：`((t−10)·2π/40) − π/2`。 */
void  yk_spiral_angle(int t, float *out);
/* 本批同角的颗数 = `min(quota, 7)`（quota <= 0 ⇒ 0）。 */
int   yk_spiral_first_count(int quota);

/* ★★★ 2026-09-22（owner 口径细化 · 永点的两条来源）：**「自动吸收」= state 1（生成即满速归航）**。
 *
 * **owner 原话（逐字）**：「th08 一手永点大概是：**极端人形态爆点是落下即消失，自动吸收**；
 *   敌人死亡爆点是「无敌」上抛，然后**自动地满速吸收**」。
 *
 * **一手（逐站点读 push 的 type/state，本样本）**：
 *   | 站点 | 触发 | push |
 *   | --- | --- | --- |
 *   | `th08:0x42b048`（`Enemy::sub_42adb0`，通用击破、带连锁段数公式）| 敌死 | state **3** / type 7 |
 *   | `th08:0x42da54`（`EnemyManager::on_tick` 死亡块）| **極端人間 或 極端妖怪**（`0x42da2a`/`0x42da38` 两问取**或**，两者皆非才跳过）| state **1** / type 7（**一颗**，无循环）|
 *   | `th08:0x451892`（`Player::compute_damage_to_enemy`）| 極端人間 + 目标**已在死亡段**（`[eax+0x1e] < 0`）+ 自机弹 | state **3** |
 *   | `th08:0x44aac9` 一族（擦弹）| 擦弹 | `type 0xa` ⇒ 被 `0x440124` 改写成 **type 7 + state 5** |
 *   ⇒ **state 1 = 当帧就满速归航**（「自动吸收」那一档）；state 3/5 = 先上抛、`vy > 0` 才转归航。
 *   ⇒ **「无敌」= state 3 在收集分派里被显式排除**：`th08:0x4409f2`–`0x4409ff`
 *     （`movsx ecx,[item+0x2d7]; cmp ecx,3; je 0x440c37`）⇒ **上抛期间根本进不了收集判定**，
 *     这正是 owner 记忆里那个「无敌」的上抛过程。
 *   ⇒ ⚠ 但「吸收」本身**仍然是抓取盒相交**：`0x440a17 call 0x44a5a0`（`Player::CalcItemBoxCollision`，
 *     盒 = `[.sht+0x18]/2`，TH08 一手 = 20/24）——**一手里没有任何「免半径自动入账」的状态**。
 *     所谓「自动」= 道具自己飞过来（state 1 归航），玩家不需要去够它。
 *
 * 本卡落点：**两条来源分开** —— ① 通用击破批 = state 3（上抛，收不到）；② 極端人/妖那一颗 = state 1
 *   （当帧满速归航）。判据就是一个纯函数（下面这个），卡侧按 gauge 的极值档调用。*/
int yk_orb_instant_absorb(int gauge);   /* 極端人 或 極端妖 ⇒ 1（一手 `0x42da54` 的或门）*/

/* ============ ★★★ §3.11c 擦弹：**每擦到一颗弹给几颗时点**（TH08 `Player::sub_44a930`）==========
 * 一手 = TH08 v1.00d（`local/th08.v1.00d/th08.exe`，平坦反汇编），函数 = **`Player::sub_44a930`**
 *   （`th08:0x44a930`–`th08:0x44ab36`，`ret 0x8` = 两参：`(Float3 *pos, i32 flag)`）。
 *   ⚠ **不是** `Player::sub_44a6a0`：那个是 `th08:0x44a6a0`–`th08:0x44a928`（`ret 0x14` = 五参）的
 *     **激光判定**函数，它在 `th08:0x44a8d1` 以 `flag = 1` 调进本函数；vendor `funcs.json` 把这两个函数
 *     合并成一条（`0x44a6a0` 名字一直盖到 `0x44ab40`）⇒ **别照它读边界**，按 `ret imm16` 切。
 *   调用点穷举（`grep 'call   0x44a930'` 全文只两处）：`th08:0x44a58f`（弹擦弹路径，
 *     `push 0x0`）/ `th08:0x44a8d1`（激光擦弹路径，`push 0x1`）⇒ **`flag` = 「是不是激光擦弹」**。
 *
 * 逐指令（三处 `call 0x4400a0` = `ItemManager::spawn_item(pos, type, state)`；push 顺序 state/type/pos
 *   ⇒ 三处都是 `spawn_item(pos, type = 0xa, state = 1)`，而 type 0xa 会被 `th08:0x440124` 当场改写成
 *   **type 7 + state 5**）：
 *
 *   `th08:0x44aa86` `ecx = GameManager; call 0x42f230`   ; `shottype_is_solo_human`
 *   `th08:0x44aa92` `je 0x44aaa0`                        ; 不是「人単独」⇒ 跳过下一条
 *   `th08:0x44aa94` `movzx edx, BYTE ds:0x164d0b1`      ; = `GameManager+0x3dba9` = **shottype**
 *   `th08:0x44aa9e` `jne 0x44ab14`                       ; 人単独 且 shottype != 0xa ⇒ **一颗都不给**
 *   `th08:0x44aaa0` `ecx = 0x577f20; call 0x42f1f0`      ; `EnemyManager::is_boss_active`
 *   `th08:0x44aaac` `je 0x44ab14`                        ; 场上没 boss ⇒ **一颗都不给**
 *   `th08:0x44aab3` `call 0x406d70`                      ; `GaugeIsExtremelyYoukai`（槽 >= **+8000**）
 *   `th08:0x44aaba` `je 0x44ab14`                        ; 不是极妖 ⇒ **一颗都不给**
 *   `th08:0x44aac9` `call 0x4400a0`                      ; ★ **第 1 颗**
 *   `th08:0x44aace` `cmp DWORD [ebp+0xc],0 / jne 0x44ab14` ; `flag != 0`（激光）⇒ 只给这一颗
 *   `th08:0x44aad9` `[ecx = SPELLCARD 0x4ea670] call 0x4178a0` ; = `SPELLCARD.flags & 1`
 *   `th08:0x44aae0` `je 0x44ab14`                        ; **没在打符卡** ⇒ 只给这一颗
 *   `th08:0x44aaef` `call 0x4400a0`                      ; ★ **第 2 颗**
 *   `th08:0x44aaf9` `call 0x42f270`                      ; `shottype_is_solo_youkai`
 *   `th08:0x44ab00` `jne 0x44ab14`                       ; 「妖単独」⇒ 不给第 3 颗
 *   `th08:0x44ab0f` `call 0x4400a0`                      ; ★ **第 3 颗**
 *
 * ⇒ **三道门是嵌套的**（后一颗都要求前一颗已经给出）⇒ 每擦到一颗弹给 **1 / 2 / 3** 颗：
 *
 * | # | 生成点 | 门（逐指令） | 判的是什么 |
 * | --- | --- | --- | --- |
 * | 1 | `th08:0x44aac9` | `!(solo_human && shottype != 0xa)` **且** `is_boss_active()` **且** `gauge_is_extremely_youkai()` | 机体不是「人単独」（妖夢 0xa 例外）、场上**有 boss**、人妖槽 **>= +8000（80%）** |
 * | 2 | `th08:0x44aaef` | `flag == 0`（**不是激光擦弹**）**且** `SPELLCARD.flags & 1` | 弹路径、**符卡进行中** |
 * | 3 | `th08:0x44ab0f` | `!shottype_is_solo_youkai()` | 不是「**妖単独**」机体 |
 *
 * 两条谓词的逐指令（都在 `GameManager`，`shottype` = `ds:0x164d0b1` = `GM+0x3dba9`）：
 *   · `shottype_is_solo_human` `th08:0x42f230`（`0x42f243 cmp ecx,0x4 / jl` → `0x42f252 and eax,1 / jne`）
 *     = `shottype >= 4 && (shottype & 1) == 0`；
 *   · `shottype_is_solo_youkai` `th08:0x42f270`（同形，`0x42f297 je`）
 *     = `shottype >= 4 && (shottype & 1) != 0`。
 *   ⇒ TH08 的 shottype：0..3 = 四**组**（霊夢&紫 / 魔理沙&アリス / 咲夜&レミリア / 妖夢&幽々子）、
 *     4..11 = 单人（偶数 = 人型那一半、奇数 = 妖型那一半；**0xa = 妖夢**，也是唯一「人単独但照样给」的）。
 *   · `is_boss_active` `th08:0x42f1f0`：扫 `ENEMY_MANAGER.bosses[0..7]`
 *     （`0x577f20 + 0x9dcda0 + 4i`；ExpHP `zEnemyManager` 该字段名就是 `bosses`）⇒ **任一非 0 即真**。
 *   · `SPELLCARD.flags & 1`：置位 = 符卡声明那条 ECL opcode `Spellcard::ecl_spell th08:0x4152a0`
 *     （`0x4152cf or ecx,0x1` → `0x4152d8 mov [edx],ecx`，同段还 `or` 了 bit2）；
 *     清位 = `Spellcard::ecl_spell_end th08:0x4161b0`（`0x4161c7 and ecx,0x1 / je 0x416a86` **整段早退**，
 *     随后 `0x4161e0 and eax,0xfffffffe` 清掉）⇒ **bit0 = 「符卡进行中」**，与 TH18 的
 *     `CE_SPELL_FLAG_ACTIVE`（`zSpellcard+0x78` bit0）**同一语义**。
 *   · ⚠ **一手没有单帧上限**：一帧擦到 N 颗弹 = 本函数被调 N 次（`BulletManager::on_tick` 逐弹各判一次）。
 *
 * TH18 侧三个输入怎么取（本卡）：
 *   · `spell_active` = `ce_spell_active()`（`sdk.h`：读 `zSpellcard+0x78` 的 bit0，一手 `th18:0x409b10` 置位）；
 *   · `shottype` = **TH18 的机体号 0..3 直接当 TH08 的「组」号**（owner §4 的映射
 *     「灵梦/紫 · 魔理沙/爱丽丝 · 咲夜/蕾米莉亚 · 早苗/文」把 TH18 每个机体都映到 TH08 的一个**组**）
 *     ⇒ `solo_human` / `solo_youkai` 两条在本卡**恒为假** ⇒ 本卡只可能给 **1 颗**（没在打符卡）
 *       或 **3 颗**（符卡进行中）；**第 2 颗那一档（「妖単独」）在 TH18 没有对应机体**
 *       —— 这是映射的必然结果，不是漏读（登记 `TUNING.md` §2.0y #6 / `AUDIT.md` §BE）；
 *   · `flag`（弹 / 激光）：TH18 的擦弹计数 `0x4ccd0c` 是**一个累计量**、分不出来源 ⇒ 一律按**弹**
 *     （`flag = 0`）算。⚠ 同一条累计量也读不出「被擦的是哪颗弹」（落点仍取自机，见 `TUNING.md` §2.0y #12）。
 *
 * ⚠ **本函数只覆盖「机体轴 + flag + 符卡」这三道**；第 1 颗前面那三条**场上门**（有 boss / 极妖 >= +8000 /
 *   人単独且非妖夢）由下面的 `yk_graze_field_gate()` 评 —— 2026-09-23 **统筹拍「按一手接上」** + 具名开关回退。*/
int yk_shottype_is_solo_human(int shottype);   /* `shottype >= 4 && (shottype & 1) == 0` */
int yk_shottype_is_solo_youkai(int shottype);  /* `shottype >= 4 && (shottype & 1) != 0` */

/* ---- 第 1 颗前面那三条**场上门**的判据码（`th08:0x44aac9` 之前的三条分支）---------------------
 * 逐指令（顺序 = 一手的分支顺序；先命中哪条就报哪条，卡侧照这个顺序打日志）：
 *   `th08:0x44aa92 je 0x44aaa0` → `th08:0x44aa94 movzx edx,BYTE ds:0x164d0b1` → `:0x44aa9e jne 0x44ab14`
 *     ⇒ **人単独（`>= 4 && 偶`）且不是妖夢（0xa）** ⇒ 一颗都不给；
 *   `th08:0x44aaac je 0x44ab14` ⇒ `EnemyManager::is_boss_active()`（`th08:0x42f1f0`）假 ⇒ 一颗都不给；
 *   `th08:0x44aaba je 0x44ab14` ⇒ `GaugeIsExtremelyYoukai()`（`th08:0x406d70`，阈值 `GM+0x3ddfe` = **+8000**）假。
 * 返回值**只用于判定与日志**（`OK` = 三条全过 = 放行）。`OFF` 那一档 = ❓ 开关（卡侧 `YK_GRAZE_FIELD_GATES`）。*/
#define YK_GRAZE_GATE_OK        0   /* 三条门全过 ⇒ 第 1 颗照给 */
#define YK_GRAZE_GATE_SHOTTYPE  1   /* `th08:0x44aa9e`：人単独且不是妖夢（0xa）—— 本卡映射下**恒不成立** */
#define YK_GRAZE_GATE_BOSS      2   /* `th08:0x44aaac`：`is_boss_active()` 假（boss 槽全 0）*/
#define YK_GRAZE_GATE_GAUGE     3   /* `th08:0x44aaba`：槽 < +8000（极妖假）*/
#define YK_GRAZE_GATE_OFF       4   /* ❓ 开关关掉：三条门整段不评（= 「聚焦即撒」老行为；门 A/B 照样算）*/

/* 评第 1 颗前面那三条门（`enabled` = 卡侧 `YK_GRAZE_FIELD_GATES`）。返回上面五个码之一。 */
int yk_graze_field_gate(int enabled, int boss_active, int gauge, int shottype);
/* 每擦到一颗弹给几颗时点（0 / 1 / 2 / 3）。`field_gate` = 上一行的返回值（非 OK/OFF ⇒ **0 颗**）；
 * `bullet_path` = `flag == 0`（弹路径）。后三道门**嵌套**（逐指令见本小节顶部）。*/
int yk_graze_orbs_per_bullet(int field_gate, int bullet_path, int spell_active, int shottype);

/* ============ ★★★ 2026-09-22（owner #26 · **风味选项**）决死窗口里全屏敌弹变红 ==================
 * owner 原话：「有 th08 的时候，**决死状态会导致画面上所有弹幕变红**」—— 这是**风味**（要一个开关）。
 * owner 2026-09-22 裁：**不抽共享件**（「应该不会再有染色需求了，抽共享也行，不抽也行」）
 *   ⇒ 染色留在 108 卡内（`youkai.c`），115（`lolk.c`）一个字都不动。
 *
 * 一手/口径（**这条是风味，不是复刻**，登记 `TUNING.md`）：
 *   · **决死窗口 = `player + 0x476ac == 4`**（`CE_PLAYER_STATE`）：一手 `th18:0x45d4e4
 *     movl $0x4,0x476ac(%edi)` 是全 exe **唯一**把那一格写成 4 的地方；先例 = `mof_core.c` /
 *     `signal_core.c` 的同名判据（两边都是「core 只认『窗口 = state 4』，那个 4 由卡侧读进来」）。
 *   · **颜色**：TH08 的 `Player::begin_death`（`th08:0x44ab40`）里**没有**逐弹改色的循环
 *     （逐指令看过：起特效 0x06 + 置玩家状态 + 屏效，没碰弹）⇒ 「决死染红」不是 TH08 行为。
 *     颜色本身取 TH08 自己的一枚红：`PlayerBombArea::reset`（`th08:0x44e370`）在 `:0x44f0c0` /
 *     `:0x44f728` 给 VM 写 **`0xffff8080`**（A=ff R=ff G=80 B=80，柔和的红）—— 同 `lolk.c` 取
 *     TH15 那一枚黄的规矩（那一枚的来源写在它的注释里）。
 *
 * 这里只放**三条纯逻辑**（无引擎地址、无浮点）⇒ 主机单测直接锁住两个不变量：
 *   ① 染色**只换 RGB、alpha 原样保留**（115 第三十三轮的实跑教训：把含 alpha 的快照写回去
 *      会把弹钉死在低 alpha 上 ⇒ 「变透明」）；
 *   ② 还原 = **当前 alpha + 快照 RGB**（同一件事的另一半）。*/
#define YK_DEATHBOMB_STATE 4             /* 一手 `th18:0x45d4e4`：全 exe 唯一写 4 的点 */
#define YK_TINT_RED        0xffff8080u   /* 一手 TH08 红（见上：`th08:0x44f0c0` / `:0x44f728`）*/

/* 决死窗口 = state 4 ⇒ 1。 */
int      yk_deathbomb_window(int32_t player_state);
/* 染色：`(cur & 0xff000000) | (YK_TINT_RED & 0x00ffffff)` —— **只动 RGB**。 */
uint32_t yk_tint_apply(uint32_t cur);
/* 还原：`(cur & 0xff000000) | (orig & 0x00ffffff)` —— 取**当前**的 alpha + **快照**的 RGB。 */
uint32_t yk_tint_restore(uint32_t cur, uint32_t orig);

/* ============ ★★★ 2026-09-22 夜（owner 工单 P-A · **夜点**）==========================================
 * owner 口径（`HUMAN-YOUKAI-GAUGE.md` §1.1/§1.2）：
 *   「夜点的初值按难度：初月 60000 / 三日月 100000 / 上つ弓张 200000 / 待宵 300000」
 *   「游戏中的**蓝点的最大分等于当前夜点值**，每得到 **2 个刻符**可增加 **10 点夜点**」。
 *
 * **一手（我们样本 `local/th08.v1.00d/th08.exe`，逐指令）**：
 *   · 夜点 = `zGlobals.pointItemValue`（`globals+0x24`；`globals` = `[0x160f508+8]`）；
 *   · **初值四档**：`th08:0x43af1b mov eax,ds:0x160f538`（difficulty）→ switch 0..4 写 `[globals+0x24]`：
 *     `th08:0x43af49` = **60000**、`th08:0x43af58` = **100000**、`th08:0x43af67` = **200000**、
 *     `th08:0x43af76` / `:0x43af85`（Extra） = **300000**；
 *   · **每 2 刻符 +10**：`GameManager::AddTimeOrbs th08:0x418220` 体内
 *     `th08:0x418294 and eax,1`（**累计刻符数的奇偶**）→ `th08:0x41829d add`（`orbs += total & 1`）
 *     → `th08:0x4182ab sar 1` → `th08:0x4182b0 imul 0xa` → `th08:0x4182bc mov [globals+0x24]`；
 *     门 = `th08:0x418227 orbs >= 0`、`th08:0x418288 orbs > 0` 才进（负数只走 currentTimeOrbs 夹 0 那支）。
 *   · **蓝点最大分 = 夜点**：`Item::CollectPoint`（`ItemManager.cpp:430-460`）里
 *     `itemScore = (y < pocY) ? pointItemValue : pointItemValue/2 − (y − pocY) × (pointItemValue/1000)`，
 *     再十位取整、`isMaxValue` 后恒为满值 ⇒ **改的是道具分值本身**（不是结算系数）。
 *
 * ⚠ 本卡此前把 `piv`（= TH08 的 `pointItemValue`，我们叫「永点 PIV」）从 **0** 起 —— 一手是上面那四档
 *   （本批修）。「每 2 刻符 +10」那条公式早在 `yk_piv_add` 里就是一手照抄（含奇偶修正），这里不重复。
 * ⚠ 夜点在 TH08 里就是屏幕中间下方那个数（`HUMAN-YOUKAI-GAUGE.md` §0 词表）⇒ 与 PIV 是**同一个量**。*/
#define YK_NIGHT_POINT_GRADES 5
int32_t yk_night_point_init(int32_t difficulty);          /* 0..4 → 60000/100000/200000/300000/300000 */
/* **蓝点分**（TH08 一手算式，整数版）：自机在收点线以上 ⇒ 满值（= 夜点）；以下 ⇒ 按 y 距离衰减。
 * `dist` = `y − pocY`（> 0 = 线下方，单位与 y 同为 640 空间的世界 y）。★ 只做整数（本仓 x87 门）。*/
int32_t yk_blue_point_score(int32_t night, int32_t dist, int32_t is_max_value);

/* ============ ★★★ 2026-09-22 夜（owner 工单 **P-B 逢魔**）============================================
 * owner 口径（`HUMAN-YOUKAI-GAUGE.md` §1.5/§1.7）：「人间 −80% 或 妖怪 +80% 以上称为逢魔（妖梦人间率例外 30%）」
 *   「逢魔时分数会自动不断加，90% 以上达到最快加分速度；每击破一个敌人可获得 2 个刻符」。
 *
 * **一手（我们样本，逐指令；owner 2026-09-22 裁：四条冲突一律按一手）**：
 *   · **门** = 两个「极」谓词取或：极人 `th08:0x406d10`（`gauge <= GM+0x3ddfc`，默认 **−8000**）/
 *     极妖 `th08:0x406d70`（`gauge >= GM+0x3ddfe`，默认 **+8000**）⇒ `setle`/`setge` 都是**含等号**。
 *     **没有状态位**（每次现算）；阈值按 shottype 改（妖梦人型 −3000 等，见规格 §3bis.1）。
 *   · **加分 = 100 显示分/帧、只有一档**：`Player::on_tick th08:0x44c390` 的 `th08:0x44c554 push 0x64`
 *     → `th08:0x44c55b call 0x4181f0`（人侧）/ `th08:0x44c58c`+`:0x44c593`（妖侧）；`if/else` 不双算。
 *     门还有一条 **`th08:0x44c504 call 0x4358bb`（`Gui::IsMsgActive() == 0`）** —— **对话中不加分**。
 *     ⚠ **owner 的「≥90% 最快档」在一手里不存在**（全 exe 无 9000 比较；立即数 9000 在 `.text` 一次都不出现）。
 *   · **击破掉刻符 = 逢魔时 1 颗**（`EnemyManager::on_tick` 死亡公共尾 `th08:0x42da2a`/`th08:0x42da38`
 *     ⇒ `th08:0x42da41 push 1` / `th08:0x42da54 spawn_item(type 7)`），**出生态 = state 1**（生成即满速归航）。
 *     人间逢魔且**自机弹击杀**再加 1 颗（`th08:0x451860`/`:0x45187f`）⇒ 一手是「+1（人人/妖怪都算）
 *     + 人间逢魔自机弹击杀再 +1」；**没有 `AddTimeOrbs(2)` 这个调用**。
 *   · 妖梦人型的 30% 例外在 TH18 **没有对应机体** ⇒ 阈值这一格按「默认 ±8000」落（规格 §4 的口径）。*/
#define YK_FENGMA_SCORE_PER_FRAME 100    /* 一手 `th08:0x44c554 push 0x64`（显示分/帧，一档）*/

/* 逢魔判定：`gauge <= human_eff（默认为负）` **或** `gauge >= youkai_eff（默认为正）` ⇒ 1。 */
int yk_fengma(int32_t gauge, int32_t human_effects, int32_t youkai_effects);
/* 逢魔击破该掉几颗刻符（一手：逢魔 ⇒ 1；人间逢魔 + 自机弹击杀 ⇒ 2；其余 ⇒ 0）。
 * `is_human_fengma` = 人侧那一条门为真（`gauge <= human_eff`）；`by_player_bullet` = 这一下是自机弹打死的。*/
int yk_fengma_kill_orbs(int fengma, int is_human_fengma, int by_player_bullet);

/* ============ ★★★ 2026-09-22 夜（owner #27）：**刻符的两支弹字**（+N 提示）======================
 * owner 原话：「**+刻符也有 UI，这个估计也得 VM 贴图**」。一手（`engine/player/th08/03-youkai-body-and-gauge.md` §2.7.3）：
 *   · **支 1「+1」** = `CreateTimePopup`（`th08:0x403460`）—— 击破魔使那一支
 *     `Enemy::sub_42adb0 th08:0x42b296`：`CreateTimePopup(&enemy+0x2d88, 1, 0, 0xFFFFFFFF)`，紧跟掉 1 颗刻符；
 *     **90 帧、位置不动**；文本布局 `[15][param3][14][number][13]`（13 = '+' = sprite149，逐像素确认）。
 *   · **支 2「分值」** = `CreatePlayerPointPopup`（`th08:0x403330`）—— 收刻符那一支
 *     `Item::collect_time_orb th08:0x441348`；值 = 100..10000（`yk_orb_score()` 已落）、颜色黄 `0xDFFFEF80` / 白 `0xDFFFFFFF`；
 *     **60 帧**、每帧 `y −= 0.5 × framerateMultiplier`。
 *   · **两支都没有时间淡出**：alpha 按**与自机的距离**现算（`th08:0x4054e3`–`0x405545` / `:0x405983`–`0x4059f1`）。
 *   · 落点 = **相对被收的道具 / 被击破的敌人 + 战场原点 (32,16)**（`th08:0x43a126`）⇒ 与世界层同一套坐标。*/
#define YK_POPUP_SCORE_LIFE   60   /* 一手 `0x3c`：score/点数弹字 60 帧 */
#define YK_POPUP_TIME_LIFE    90   /* 一手 `0x5a`：time/魔使弹字 90 帧（位置不动）*/
#define YK_POPUP_GLYPH_PLUS   13   /* 字形索引 13 = '+'（TH08 `ascii.anm` sprite149，逐像素确认）*/

/* 弹字的 alpha（一手三段整数式）：`d = dx² + dy²`（相对自机，单位 = 世界单位取整、按 8 位截断）。
 *   `d > 0x1000 ⇒ 0xD0`；`d > 0x400 ⇒ ((d − 0x400) × 128) / 0xc00 + 0x50`；否则 `0x50`。*/
int yk_popup_alpha(int32_t dx, int32_t dy);
/* 十进制字形（高位在前，返回位数；`value <= 0` ⇒ 1 位 '0'）。`out` 最多写 `max` 位。*/
int yk_popup_digits(int32_t value, uint8_t *out, int max);
/* ============ ★★★ 2026-09-22 夜（owner **P-D 缩水版**）：击破魔使的三条 ==========================
 * owner 口径（`HUMAN-YOUKAI-GAUGE.md` §1.4）：「每击破 1 个魔使减去当前操术的**百分数的 10%**
 *   （妖梦人型为例外，只减 5%）」；owner 2026-09-22 裁：**按一手**。
 *
 * **一手（我们样本）**：唯一写点 `th08:0x42b223`（在 `Enemy::sub_42adb0` 的 `0x42b1e5` 分支里）：
 *   · 门 `th08:0x42b1e8 call 0x41fd20`（`[enemy+0x2da4] != 0` =「我是魔使」）+ `th08:0x42b1f5`（arg1 = 真击破）；
 *   · `th08:0x42b20e get_youkai_gauge()` → `th08:0x42b213 neg` / `th08:0x42b216 mov ecx,0xc` / `th08:0x42b21b idiv`
 *     ⇒ **`delta = −gauge/12`**（除数立即数 **0xc**；**没有 10%/5% 两个分支**，也没有 shottype 判定）；
 *   · 连带两条：同支 `th08:0x42b29b SpawnItem(type 7, state 1)`（**固定掉 1 颗刻符**）与
 *     `th08:0x42b240 Timer(0x18b89d4).set(0x32 = 50)` —— 后者把收刻符那笔 ±111 **门住 50 帧**
 *     （`th08:0x44138e`–`0x44139c`；见 `Item::collect_time_orb`）。
 *   ⇒ owner 口径与一手不同（10%/5% vs 1/12），**按一手**，登记 `TUNING.md` §2.0zk。*/
#define YK_FAM_KILL_GAUGE_DIV   12   /* 一手 `th08:0x42b216 mov ecx,0xc`（`idiv` ⇒ 向零截断）*/
#define YK_ORB_SILENCE_FRAMES   50   /* 一手 `th08:0x42b240 Timer(0x18b89d4).set(0x32)` */

/* 击破一个魔使的人妖槽增量：**−gauge/12**（C 的 `/` 对负数向零取整，与一手 `idiv` 同义）。*/
int yk_gauge_fam_kill_delta(int32_t gauge);
/* ============ ★★★ 2026-09-22 夜（owner **批次 3**）：**人侧撞到使魔 ⇒ MISS** ====================
 * owner 口径：「**人间操术下撞击会 MISS**；妖怪操术下**不可见但可穿梭其间**」（`TH08-ITEMS-AND-SC.md` §2b）。
 *
 * **一手（两个半径，都是我们样本/本仓已有的一手值）**：
 *   · 自机判定半径 = `player+0x4799c`（非聚焦，原生 **3.0**）/ `+0x479a0`（聚焦，原生 3.0）
 *     —— `engine.h` 的 `CE_PLAYER_HITBOX_R_NF/_F`；
 *   · 使魔 hurtbox = 惰性真敌人的默认 hurtbox **24.0** 的一半 ⇒ **12.0**（`YK_FAM_HURTBOX_HALF`，
 *     spec §6.1）。
 * ⇒ 判据 = **圆 vs 圆**：`dx² + dy² <= (自机半径 + 12.0)²`（人侧 3.0 + 12.0 = **15.0** 内触发）。
 *   ⚠ TH08 那边是「敌本体碰撞体 vs 自机判定点」的盒判定；我们这里两个都是圆（引擎给我们的读数就是半径），
 *   所以用圆 vs 圆 —— 这是**口径**（登记 `TUNING.md`），不是逐位复刻。*/
#define YK_FAM_HIT_REACH  (YK_FAM_HURTBOX_HALF)   /* = 12.0（自机半径由调用方传入，见卡侧读 `+0x4799c`）*/
/* `dx/dy` = 自机 − 使魔（世界单位）；`hit_r` = 自机判定半径（一手 3.0）。⇒ 1 = 撞上。 */
int yk_fam_hit_player(float dx, float dy, float hit_r);
/* ============ ★★★ 2026-09-22 夜（Time 的**第三道门**）：**损命窗口内抓取盒整段关闭** ============
 * owner 口径（TH08-ITEMS-AND-SC §1.1 的 Time 那一档）：「**恰好在自机损命时出现的那颗不能自动吸收**」。
 *
 * **一手（TH08）**：`Player::CalcItemBoxCollision th08:0x44a5a0` 读 `ds:0x17d5ef8`（playerState）：
 *   `th08:0x44a5c1 je`（**0 = ALIVE 放行**）/ `th08:0x44a5c9 cmp 3 je`（**3 = INVULNERABLE 放行**）/
 *   `th08:0x44a5d4 cmp 4 je`（**4 = BORDER 放行**）/ 否则 `th08:0x44a5d9 xor eax,eax`（**其余一律「没碰到」**）
 *   ⇒ **白名单 = {0, 3, 4}**，被挡的是 {1 SPAWNING, 2 DEAD}。
 *
 * **TH18 侧的状态编号（本批补的一手，`engine/player/th18/01-…`/`03-player-hit-gate.md`）**：
 *   `player+0x476ac` 是状态机 `switch (0..4)`；**移动只在 case 1 调**（`th18:0x45c0cc`）⇒ **1 = 存活可动**；
 *   `Player__die th18:0x45d3a0` 置 **4 = 决死窗口**（`th18:0x45d4e4 movl $0x4`）；
 *   `Player__commit_death_and_enter_state2 th18:0x45d090`（8 帧后）⇒ **2 = 死亡段**（miss 已记、不可动）；
 *   命中链在 `+0x476ac ∈ {2,4,3}` 时一律早退（`th18:0x45cd10` 一族）⇒ 3 = 无敌/复活那一档。
 * ⇒ **等位映射**：TH08 `{0 ALIVE, 3 INVULNERABLE, 4 BORDER}` ↔ **TH18 `{1, 3, 4}`**；
 *   被挡的 TH08 `{1 SPAWNING, 2 DEAD}` ↔ **TH18 `{0, 2}`**（0 = 初生/未就位）。
 * ⚠ 这条是**等位映射**（不是逐位复刻）：TH18 的 0 是「初生」、TH08 的 0 是「存活」，两个 0 语义相反 ⇒ 映射按**语义**而非编号。
 *   🟡 残余：TH18 的 state 0 具体是什么（`Player::initialize` 之后到第一帧之间）没有逐指令钉死；若实跑发现开局收不到东西，
 *   把 `YK_ITEM_BOX_STATE_INIT` 一并放行即可（一行）。*/
#define YK_ITEM_BOX_STATE_INIT 0   /* TH18「初生/未就位」（TH08 的 SPAWNING 对应位；本卡**不放行**）*/
#define YK_ITEM_BOX_STATE_ALIVE 1  /* TH18 存活可动（移动只在 case 1）*/
#define YK_ITEM_BOX_STATE_DEAD 2   /* TH18 死亡段（`th18:0x45d090` 之后）*/
#define YK_ITEM_BOX_STATE_INVULN 3 /* TH18 无敌/复活档（命中链在 3 时早退）*/
#define YK_ITEM_BOX_STATE_BORDER 4 /* TH18 决死窗口（`Player__die` 置 4）*/

/* 抓取盒开着吗：一手白名单 `{ALIVE, INVULNERABLE, BORDER}` ⇒ 1；其余（含初生/死亡/野值）⇒ 0。 */
int yk_item_box_open(int32_t player_state);
