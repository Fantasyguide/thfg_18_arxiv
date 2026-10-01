# 交接：108 的 Shift 切侧闸接入「灵界 / 暴走咆哮」跨卡只读查询

> **版本**：TH18 v1.00a（`th18.exe`，imagebase `0x400000`）。本文裸地址默认属该版本；引用其他版本一律带前缀（`th13:0x…` / `th17:0x…`）。
> **方法**：一手读工作树源码（行号快照 = 2026-10-01 18:22）+ gcc 预处理 / `objdump -dr` + 本机实跑日志。
> **可信度**：字段 / 行号 / 判据 ✅（逐条给行号，本机核过）；方案取舍与实装代价 🟡（未实装、未实跑）。
> **读者**：接手这条跨卡只读查询的**统筹 agent**。本文自包含，不读别的文档也能动手。
> **相关**：[`HANDOFF-108.md`](HANDOFF-108.md)（108 条目 87）、[`AUDIT.md`](AUDIT.md) §BE 详录 #201、[`SDK.md`](SDK.md) §6.1（声明层）。
> **⚠ 落点改名**：任务原定 `HANDOFF-CROSSCARD-STATE.md`，该路径已被统筹的**跨卡状态台账**占用（提交 `3857e8d4`，已入库）⇒ 本文落在 `HANDOFF-108-CROSSCARD-SHIFT.md`；台账要不要加一行指过来，由统筹定。
> **⚠ 行号会漂**：108 那一侧正被 th08 线并发修 P0（见 §0.1）⇒ 落地前用 §7 的 `grep -n` 现算行号，别照抄本文数字。

## §0 需求一句话

108（TH08 永夜抄系统卡，`native/cards/youkai.c`）「放 B 期间锁形态」的总门已经写好，但**灵界（113）与暴走咆哮（117）两支是恒 0 的 TODO 桩**：这两个状态是那两张卡的**私有变量**，108 没有合法只读路径。owner 决定把这条**跨卡只读查询**交给统筹落地 ⇒ 本文给出两边真实现状、两个候选方案、以及可照抄的落地清单与验收判据。

`youkai_core.h:1290` 的 `yk_side_switch_allowed(trance, roar, real_bomb)` 语义已经是「三者取或，任一成立 ⇒ 不切侧」（`youkai_core.c:871`：`return (trance || roar || real_bomb) ? 0 : 1;`）⇒ **要改的只有喂进去的 `trance` / `roar` 两个实参**。

## §0.1 前置状态：108 侧刚出的 P0（并发线正在修，别在这里重复修）

- 现象：装了 108 一进关卡即爆栈（dump `code=0xc00000fd` STACK_OVERFLOW）。
- 根因（✅ 已核）：`yk_focused()` 里那句本该是 `CE_PLAYER_FOCUSED()`（`native/engine.h:435`，`player+0x476cc`）的读法被批量替换时一起换掉了，变成 `return yk_focused() ? 1 : 0;` = **自递归**；`s_side_lock` 初值 −1 ⇒ 正常游玩每帧都进。
- 现状：**工作区（未提交）已修**（`git status` 显示 `M native/cards/youkai.c`）：新增 `yk_real_focused()`（`youkai.c:371`）+ 重入守卫 `yk_focused()`（380–397），旧的自递归函数保留为 `yk_focused_buggy()`（398–402）当反面教材。
- **已发布的那版仍有这个 bug**：`native/th18_card_expand.dll` 的 md5 = `88e828dfac4c3a8aea79ad3ebeef7ede`，等于 `RELEASE-STATE.json` 的 `last_release.dll_md5`。
- ⇒ **验收第 0 步**：先有一版含该修复的 DLL，否则「灵界/咆哮下按 Shift」根本测不到（第一帧就爆栈）。本文是只写文档的任务，**一个 `.c/.h` 都没动**。
- ⚠ **本文档描述的 108 行为需要「含该 P0 修复」的 DLL 才能实跑验证** —— 拿旧版（`88e828…`）去测只会得到爆栈（`0xc00000fd`），那不是本功能的失败。

## §1 需求与 owner 原话

> **owner 逐字**（留在 `youkai.c:348-351` 与 `AUDIT.md` §BE 详录 #201）：
> 「当处于**灵界状态** or **暴走咆哮状态**的时候，**可以改主机贴图**，但是 **shift 别把妖子机放出来**」。

拆成三条可判定的要求：

1. **两个状态任一成立时，按 Shift 不切侧** —— 不放/不收 108 的妖子机、不换妖侧出弹组、不认领妖侧 shooterset。
2. **主机贴图照变** —— 那是引擎 / 113 / 117 自己的事，108 **一行都不碰**（这条不是 108 的验收项，是「别顺手改」的红线）。
3. **灵击不算 B、不受影响** —— 已有的「决死窗口里按的 B」判据不动（`youkai_core.c:865` + 单测 `native/tests/test_youkai_core.c:1355-1363`）。

## §2 两边现状（行号 = 18:22 快照）

### §2.1 108 侧：门已经通了，缺的只是两个实参

| 件 | 位置 | 现状 |
| --- | --- | --- |
| 锁值 | `youkai.c:358` `s_side_lock` | −1 = 不锁 / 0 = 锁人侧 / 1 = 锁妖侧；`on_run_reset` 5168、`on_delete` 5419 各复位一次 |
| 「本卡眼中的侧」 | `youkai.c:380` `yk_focused()` | 锁定期间恒返回 `s_side_lock`，否则 = `yk_real_focused()`（`player+0x476cc`）|
| 状态机 | `youkai.c:422` `yk_side_lock_tick()` | 每帧一次（`on_tick_2` 开头调）；`yk_side_switch_allowed(...)` 在 **440** 行 |
| **两支 TODO 桩（要改的就是这里）** | **`youkai.c:412` `yk_trance_active()` / `413` `yk_roar_active()`** | **恒 `return 0`** ⇒ 目前门只对「真 B」生效 |
| 纯判据 | `youkai_core.c:865/871`，声明 `youkai_core.h:1288/1290` | 有主机单测 9 条（`test_youkai_core.c:1355-1363`），**落地时不用动** |

桩上面的注释（`youkai.c:404-411`）已经写了两条候选路线，本文 §3 把它坐实（其中「`cards_def` 弱符号导出面」这句要订正：仓里**没有**弱符号机制，见 §3.2）。

### §2.2 113 侧：灵界「生效」的真字段是 `core.phase`（★ 订正：不是 `p->state`）

- 卡私有状态 = `trance_card_t`（`native/cards/trance.c:624`），第一个成员就是 `trance_state_t core;`（**625**）；卡内访问器 `st()` 在 **890**。
- **灵界窗口本身只有 `phase` 一个字段**：`native/cards/trance_core.h:226` `int32_t phase;`，取值（`trance_core.h:188-190`）：
  - `TRANCE_PHASE_IDLE` = **0** ⇒ **不在灵界**
  - `TRANCE_PHASE_MANUAL` = **1** ⇒ 手动/自动激活的灵界
  - `TRANCE_PHASE_DEATH_FREE` = **2** ⇒ 中弹救下的灵界（结束时补死）
- ⇒ **「灵界生效」的判据 = `core.phase != TRANCE_PHASE_IDLE`**（等价写法在 `trance.c:964`：`c->in_trance = (s->core.phase != TRANCE_PHASE_IDLE) ? 1 : 0;`）。
- 状态迁移都在 `trance_core.c`：**156**（已在灵界就不给再激活）、**170**（`begin` 写 1 或 2）、**195**（已在灵界的处理）、**214-216**（`end` 写回 `IDLE`）、**300**（`tick` 把 phase 作为出参给出）。
- 卡侧每帧唯一入口 = `on_item_frame`（`trance.c:2797`），`trance_tick(&s->core, s->frame)` 在 **2844**；另有 `on_tick_2`（2149）、`on_run_reset`（4381）、`on_delete`（4392，走 `trance_clear`）。
- 🚫 **重要订正**：`trance.c` 里的 `p->state` **不是**灵界窗口 —— 那是**神灵实体**的运动 state 机（`TRANCE_STATE_DRIFT`=1 / `ATTRACT`=2 / `REIMU_SLOW`=4，`trance_core.h:947-949`；写点 `trance.c:1700`，步进 2038-2040）。AUDIT §BE #201 与 108 的 TODO 注释都写成 `p->state`，落地时**必须改用 `core.phase`**。

### §2.3 117 侧：暴走咆哮「生效」= `ready || hypering`（现成判据已经在卡里）

- 卡私有状态 = `animal_card_t`（`native/cards/animal.c:510`），成员 `animal_core_t core;`（**512**）；访问器 `st()` 在 **589**。
- **判据现成**：`animal.c:656` `static int roaring(animal_card_t *s) { return s->core.ready || s->core.hypering; }`；上方注释（650-655）写明一手语义：
  - `ready` = **满槽自动就绪**（TH17 `HYPER_FLAGS` bit1，`th17:0x40fc8a` 置位）⇒ **咆哮不等按键**；
  - `hypering` = **40 帧霊撃**（`ANIMAL_HYPER_FRAMES` = 40，`animal_core.h:36`）= 把咆哮用掉。
- 字段：`animal_core.h:53` `ready`、`:54` `hypering`；计时 `duration`（:57）、`time_left`（:58）—— **计时不改变判据**，只影响时长。
- 状态迁移都在 `animal_core.c`：`ready = 1` @**63**；就绪窗口超时清 0 @**114**；霊撃起手 `hypering = 1` + `ready = 0` @**123-125**；霊撃结束清 `hypering` @**136**；`animal_clear_and_restore` 两者一起清 @**149-150**。
- 117 每帧入口 `on_tick_2`（`animal.c:3878`）；清理总口 `clear_card()`（4028）—— `on_stage_start` / `on_run_reset` / `on_delete` 三个钩子都走它（见文件尾部 `CE_CARD(117, …)`）。
- ⇒ **零发明**：只要每帧问一句 `roaring(s)` 就能覆盖所有入口/出口（见 §3 推荐）。

## §3 两个候选方案

> 共同前提：**108 只读**，113/117 各自写自己的位；谁的卡谁负责在删卡时收干净（`SDK.md:251` 的铁律）。
> 共同风险：进/出状态那一帧存在**排序窗口**（同一帧里 108 的 `on_tick_2` 与 113/117 的声明谁先谁后取决于卡链顺序）⇒ 下面两个方案都要求**粘性**（状态边沿置/撤），不要做「当帧有效、跨帧自过期」的帧内通报。

### §3.1 方案① 共享件加一对「跨卡只读标志」API（推荐）

**接口**（名字照 108 的 TODO 注释，语义常量化，别用裸 bit）：

```c
/* native/sdk_core.h —— 纯逻辑层（可上主机单测 native/tests/test_sdk_core.c） */
#define CE_CARD_FLAG_TRANCE  0x01u   /* 灵界进行中（113 声明） */
#define CE_CARD_FLAG_ROAR    0x02u   /* 暴走咆哮中（117 声明） */

void     ce_card_flag_set(uint32_t card_id, uint32_t bits);  /* 粘性：置/撤（bits = 0 全撤） */
uint32_t ce_card_flag_get(uint32_t card_id);                 /* 只读；没有这张卡 = 0 */
int      ce_card_flag_any(uint32_t bits);                    /* 只读聚合：任一张在场卡命中 */
```

**改动面**：

| 文件 | 改什么 |
| --- | --- |
| `native/sdk_core.h` / `sdk_core.c` | 一张定长小表（按 id 索引或 8 槽）+ 上面三个函数（纯逻辑、无引擎依赖 ⇒ 可直接主机单测）|
| `native/sdk.h` | `ce_layer_drop_card()`（`sdk.h:1044-1076`）加一行 `ce_card_flag_set(id, 0)` —— 删卡/局末/换关的唯一收口 |
| `native/cards/trance.c` | `on_item_frame`（2797）里 `trance_tick`（2844）后一句：`ce_card_flag_set(TRANCE_NUM, (s->core.phase != TRANCE_PHASE_IDLE) ? CE_CARD_FLAG_TRANCE : 0u);` |
| `native/cards/animal.c` | `on_tick_2`（3878）里一句：`ce_card_flag_set(ANIMAL_ID_NUM, roaring(s) ? CE_CARD_FLAG_ROAR : 0u);`（`clear_card()` 由 `ce_layer_drop_card` 兜底，仍建议显式撤一行）|
| `native/cards/youkai.c` | **412/413** 两支桩改成 `ce_card_flag_any(CE_CARD_FLAG_TRANCE)` / `ce_card_flag_any(CE_CARD_FLAG_ROAR)` |

**优点**：与仓里既有架构同向（`SDK.md` §6.1：「卡**声明**意图 → 层**聚合**」，「规则**不依赖卡链顺序**」）；纯逻辑可单测；删卡有唯一自动收口；以后别的卡要读同一件事现成可用。
**缺点 / 风险**：**动共享件 = 改所有人的卡** ⇒ 必须按协作纪律登记 + 跑回归（见 §5）；通用 flag 池若不加语义常量容易被滥用（用 `CE_CARD_FLAG_*`，别在卡里写裸 bit）。
**排序细节**：若做成「每帧重申 + `frame == ce_sdk_frame()` 才算数」（照 `ce_fairy_claim_group`，`sdk.c:3722`）会**依赖卡链顺序** —— 108 若排在 113 前面就会读到上一帧 ⇒ 违反 `SDK.md:210`。所以本层必须**粘性**。

### §3.2 方案② 各卡导出只读查询（不碰共享件）

**接口**：

```c
/* native/cards/trance.c（113 导出） */ int card113_trance_active(void);
/* native/cards/animal.c（117 导出） */ int card117_roar_active(void);
```

**实现**：每张卡维护一份**文件级 static 镜像位**（`s_alive = phase != IDLE` / `roaring(s)`），在自己每帧的钩子里刷新，在 `on_delete` / `on_run_reset`（117 还要经 `clear_card`）清零；108 在 `youkai.c` 里 `extern` 声明后调用。**别放进 `youkai_core.c`** —— 那个文件要单独链主机单测（`native/Makefile:238`），多一个外部符号会链接失败。

**可行性（✅ 已核）**：所有 `cards/*.c` 由通配编进同一个 DLL（`native/Makefile:166` `SRCS := … $(wildcard cards/*.c)`、167 `OBJS`、174-175 链接）⇒ 非 static 符号天然可跨卡引用；`def` 只管导出、不管内部互引。**仓里没有弱符号机制**（`grep -rn weak native/` 零命中）⇒ 写错符号名是**链接期报错**，不会静默成 0（这点比 108 注释里的假想安全）。

**优点**：不碰 `sdk.*` ⇒ 不触发「改所有人的卡」的回归面；改动量小。
**缺点 / 风险**：① 这是仓里**第一个「卡直接调卡」**的先例（现在一处都 `grep` 不到），方向与 `SDK.md` §6.1 相反，容易被后来者扩散；② 每张卡都要记得在删卡路径清镜像位，漏一个 = 108 永久锁死（113/117 的清理点还不止一个）；③ 113/117 的真状态在 `ce_state`（**按实例**），导出函数必须多维护一份进程级镜像，多一份要同步的状态；④ 同 id 多实例语义未定义（后写者覆盖）。
**订正**：108 注释里的「`cards_def` 弱符号导出面」**不存在**，不要按那条找。

### §3.3 推荐

**方案①**，但**语义化命名 + 粘性 + 纯逻辑进 `sdk_core.c` + `ce_layer_drop_card` 收口**。
理由：仓里已经有这套层与配套纪律（`SDK.md` §6.1、`AUDIT.md` §BE），108 的 TODO 注释也是朝这个方向写的；方案②虽然少动一个文件，但换来一个不该有的跨卡调用先例 + 每卡一份镜像状态 + 多处清理责任。若统筹判断「现在不能动共享件」，方案②可以落地，但请把「只许这一处、且必须粘性」写进注释。

## §4 给统筹 agent 的落地清单

**接口签名（方案①）**：`void ce_card_flag_set(uint32_t card_id, uint32_t bits);` / `uint32_t ce_card_flag_get(uint32_t card_id);` / `int ce_card_flag_any(uint32_t bits);`（位：`CE_CARD_FLAG_TRANCE` = 0x01、`CE_CARD_FLAG_ROAR` = 0x02）。
**接口签名（方案②）**：`int card113_trance_active(void);` / `int card117_roar_active(void);`。

| # | 谁 | 文件 : 位置 | 改什么 |
| --- | --- | --- | --- |
| 0 | th08 线 | `native/cards/youkai.c` | 先合入 §0.1 的 P0 修复（工作区已有）⇒ 出一版新 DLL，否则验收测不到 |
| 1 | 统筹 / 113 作者 | `native/cards/trance.c` : `on_item_frame`（2797）`trance_tick`（2844）之后 | 一句 `ce_card_flag_set(TRANCE_NUM, core.phase != TRANCE_PHASE_IDLE ? CE_CARD_FLAG_TRANCE : 0)` |
| 2 | 统筹 / 117 作者 | `native/cards/animal.c` : `on_tick_2`（3878） | 一句 `ce_card_flag_set(ANIMAL_ID_NUM, roaring(s) ? CE_CARD_FLAG_ROAR : 0)`；`clear_card()`（4028）显式撤 |
| 3 | 统筹 | `native/sdk.h` : `ce_layer_drop_card`（1044-1076） | 加 `ce_card_flag_set(id, 0)`（删卡/局末/换关自动收）|
| 4 | 统筹 | `native/sdk_core.h` / `sdk_core.c` | 表 + 三个函数（纯逻辑）|
| 5 | 统筹 / 108 作者 | `native/cards/youkai.c` : **412 / 413** | 两支桩改成读层（只改这两行）|
| 6 | 统筹 | `native/tests/test_sdk_core.c` | 新增用例：置/撤/任意聚合/删卡撤（`test_youkai_core.c` 不动）|

**验收判据（灵界 / 咆哮各跑一遍，看日志关键字）**：

1. 灵界中按 Shift：主机贴图**照变**（113 的 VM 自己那套，108 不参与）；108 侧**不放/不收妖子机、不换妖侧出弹组**；退出灵界后按 Shift **恢复**可切。
2. 咆哮中（满槽 `ready` 或 40 帧 `hypering`）同上。
3. 真 B 中仍然锁（既有行为，`test_youkai_core.c:1355-1363` 9 条不动）。
4. **灵击**（决死窗口里按 B）**不锁**、照常可切。
5. 边界：进/出状态那一帧允许 **1 帧延迟**（粘性标志的固有代价）；但**不许永久卡住** —— 删卡/换关/局末之后再按 Shift 必须恢复。
6. 闸：`cd mods/th18.v1.00a/card-expand/native && make -s check`（rc = 0；它已包含 `tooling/check-docs.py`）。

## §5 纪律（动手前照读）

- **改 113 / 117 = 改别人的卡** ⇒ 在 `AUDIT.md` 的 **§BE「非系统卡改动清单（协作须知）」** 逐条登记（`文件 → 改了什么 → 为什么 → 谁改的 → 怎么验证的`）+ **在被改文件里留一段注释**说明来龙去脉（作者靠这份清单自查，登记一次都不能省）。
- **走方案① = 动共享件 = 改所有人的卡** ⇒ 报告里单开一节写「对既有卡的影响面 + 回归跑了什么」；层按 `SDK.md` §6.1 写，纯逻辑进 `sdk_core.c`。
- 108 与 113/117 都属系统卡（id 106–117）：本次是 owner 点名让统筹做跨卡查询，**落地人仍要记 §BE**。
- **不加新断点站点**（本功能零站点；`sites.py` 已超记账线，见统筹台账 §3.4）。
- **别动主机贴图**：`yk_side_lock_tick` 只钉「本卡眼中的侧」，引擎字段一个字节都不写。

## §6 不确定项 🟡（如实列出）

1. **117 是否还有别的咆哮入口**：我只核到 `roaring()` = `ready || hypering`（`animal.c:656`）与 `on_ready` 单点（3068）；`on_death_frame2` / `on_death_before_deathbomb` 的救援路径是否会开 hyper 我没逐条走完 ⇒ 用「每帧按 `roaring(s)` 重算」正好绕开这个未普查项。
2. **113 的灵界是否还有第三个来源**：`phase` 的两条来源（1 手动 / 2 中弹救下，`trance_core.c:170`）已核；是否还有别的卡/机制把它开起来没普查 ⇒ 同上，按 `core.phase != IDLE` 每帧重算即可覆盖。
3. **同 id 多实例**：我没核实「同一 id 是否可能同时在场两张」（影响方案②的镜像语义；也影响方案①的表是「按 id」还是「按实例」）。🟡 若可能重复，方案①的表要改成「每槽一个 card 指针」，或写死「后声明者覆盖」。
4. **进/出那一帧的排序**：108 的 `on_tick_2` 与 113/117 的声明谁先谁后**未实跑验证**；本文按「粘性 + 1 帧延迟可接受」设计，未实跑。
5. **方案②的 `roaring()` 不能直接导出**（它是 static 且带 `animal_card_t *` 形参）⇒ 要另写无参导出函数读镜像位；这一步没写，留给落地人。
6. 本文行号是 18:22 快照，而 `youkai.c` 正被并发修改 ⇒ **任何行号落地前重算**（§7 第一条）。

## §7 复核命令（可复制）

```bash
cd /home/nipporita/renkolab
D=mods/th18.v1.00a/card-expand

# 现场行号（别信本文的快照数字）
grep -n "yk_trance_active\|yk_roar_active\|yk_focused\|yk_real_focused\|s_side_lock = -1" $D/native/cards/youkai.c

# 108：门与 owner 原话
sed -n '347,455p' $D/native/cards/youkai.c
grep -n "yk_side_switch_allowed\|yk_is_real_bomb" $D/native/cards/youkai_core.c $D/native/cards/youkai_core.h

# 113：灵界相位（真字段，不是 p->state）
grep -n "TRANCE_PHASE_IDLE\|TRANCE_PHASE_MANUAL\|TRANCE_PHASE_DEATH_FREE\|int32_t phase;" $D/native/cards/trance_core.h
grep -n "s->phase = \|phase != TRANCE_PHASE_IDLE" $D/native/cards/trance_core.c $D/native/cards/trance.c | head
grep -n "TRANCE_STATE_DRIFT" $D/native/cards/trance_core.h          # 这才是 p->state 的取值

# 117：咆哮判据
sed -n '650,657p' $D/native/cards/animal.c
grep -n "st->ready = \|st->hypering = " $D/native/cards/animal_core.c
grep -n "static void clear_card" $D/native/cards/animal.c

# P0 证据（自递归那一版在已发布 DLL 里）
cd $D/native && gcc -E -I. cards/youkai.c | grep -A4 "^static int yk_focused"
objdump -dr cards/youkai.o | grep -A4 "yk_focused"
md5sum th18_card_expand.dll        # 应等于 RELEASE-STATE.json 的 last_release.dll_md5

# 闸（文档闸已含在 make check 里）
cd /home/nipporita/renkolab/mods/th18.v1.00a/card-expand/native && make -s check
cd /home/nipporita/renkolab && python3 tooling/check-docs.py
```
