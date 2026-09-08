# SDK —— 给一张新卡写行为

> **版本**：TH18 v1.00a（`th18.exe`，imagebase `0x400000`）。本文裸地址默认属该版本；引用其他版本须写成 `th16:0x…`。
> 面向**写行为的人**：一张卡的登记在 [`DATA.md`](DATA.md)（JSON），行为在这里（C）。两层按 id 绑定，各自能独立验证。
> 本文既是使用说明也是设计文档；机制上的每一条断言都有 AUDIT §O 的条目对应。

## 0. 一张有行为的卡长什么样

```c
/* native/cards/sk.c —— 黑桃 K：自机弹伤害 ×1.1 */
#include "sdk.h"

static int on_bullet_created(ce_card_t *c, void *bullet)
{
    (void)c;
    int32_t *dmg = (int32_t *)((uint8_t *)bullet + CE_BULLET_DAMAGE);
    *dmg = *dmg * 11 / 10;
    return 0;
}

CE_CARD(61, .on_bullet_created = on_bullet_created);
```

加上 `th18/cards.js` 里的 `"61": {…}`，这张卡就完整了。`CE_CARD` 展开成：这张卡的回调表、一份 21 槽虚表（基类拷贝，用到的槽换成桩）、
一条登记进 SDK 注册表。**不用改任何别的文件**——`native/cards/*.c` 由 Makefile 通配。

## 1. 机制：断点换虚表，跳转表不动

零售分配器 `allocate_new_card` 对 id ≥ 57 全走 case 56（`0x411489`）：游戏自己 `new(0x54)` 一个基类 `zAbility`，
虚表 `0x4b4c78`，然后进公共尾段。尾段第一条真正的写是 `0x412cec mov [esi+4], ebx`（**esi = 卡对象，ebx = id**）。

SDK 在这里挂 thcrap 断点 `ce_card_bind`（cavesize 6：这条 + 后面的 `mov eax,[edi+0x28]`，都无相对寻址）：

```
if (behavior_by_id[ebx] != NULL)  *(void **)esi = 那张卡的虚表;
return 1;   /* 放行原指令 */
```

换完虚表之后尾段照常：`mode 0/2` 调 ctor 槽（`0x412d11`）、`mode 2/3` 调 dtor 槽、入链、`owned[id]=1`、`+0x4c` = 表行指针、
建 HUD。所以：

- **构造 / 析构语义与零售完全一致**（`02-lifecycle.md` §2）：ctor 返回非 0 = 当场删卡 = 即时卡。
- **对象由游戏分配和释放**，大小固定 0x54；SDK 不碰堆。
- **没有手写机器码**：桩是编译器生成的 `__thiscall` 函数。
- 没登记行为的新卡什么也不发生（虚表仍是基类 = 全 `ret`），和现在一样。

## 2. 对象与虚表

基类对象 0x54 字节（一手，case 56 与尾段）：

| 偏移 | 字段 |
| --- | --- |
| +0x00 | 虚表 |
| +0x04 | `card_id` |
| +0x08 | `array_index`（分配序号）|
| +0x0c..+0x1b | 链表结点（`mgr+0x18` 表头，`0x412e90` 插入）|
| +0x30、+0x44 | 位标志（case 56 清 bit0）|
| +0x48 | 充能时长（主动卡）|
| +0x4c | `zTableCardData*`（尾段写）|
| +0x50 | flags：bit0 = 存档/replay 装载，bit3 = 主动，bit6 = 装备，bit5 = 开火中。绑定时 SDK 按 JSON 改：`active_recharge` → 主动写法；`category: 1` → 零售装备写法 `& ~0x0a \| 0x44`（HUD 装备行 / `num_equipment`，AUDIT X11）|

21 个槽（`03-hooks.md` §1 的语义；签名由基类实现的 `ret N` 定，`this` 在 ecx）：

| 槽 | SDK 回调名 | 签名 | 备注 |
| --- | --- | --- | --- |
| +0x00 | `ctor` | `int (ce_card_t*)` | 返回非 0 → 当场删卡 |
| +0x04 | `dtor` | `int (ce_card_t*)` | 同上；商店购买先 ctor 再 dtor |
| +0x08 | SDK 主动卡状态机 | | C 键发动；无 `active_recharge` 时等价于基类 `ret 0` |
| +0x0c | `on_death_after_deathbomb` | `int (ce_card_t*, uint32_t)` | 返回非 0 = 救下了 |
| +0x10 | `on_death_before_deathbomb` | `int (ce_card_t*)` | |
| +0x14 | `on_death_frame2` | `int (ce_card_t*)` | 死亡结算之后（扣钱扣火力已发生）|
| +0x18 | `on_power_level_change` | `int (ce_card_t*)` | 火力档变 / 开局 / 关店后广播；**装备卡在这里生成子机**（§12）|
| +0x1c | `on_tick_shooters` | `int (ce_card_t*, uint32_t, uint32_t)` | 每射击帧（**只在按住射击键时**广播）|
| +0x20 | `on_load` | `int (ce_card_t*)` | 返回值 OR 累加 |
| +0x24 | `on_tick` | `int (ce_card_t*)` | 玩家侧每帧 |
| +0x28 | `on_bullet_created` | `int (ce_card_t*, void *bullet)` | 每颗自机弹。**加伤害别写这里**，用 §13 的伤害层 |
| +0x2c | `on_tick_2` | `int (ce_card_t*)` | 管理器侧每帧；菜单 / 商店里**不跑** |
| +0x30 | `on_enemy_drop` | `int (ce_card_t*, uint32_t, uint32_t)` | 敌人掉落道具后；两参语义 🟡 |
| +0x34 | `on_stage_start` | `void (ce_card_t*)` | 每关开场 |
| +0x38..+0x40 | 基类透传 | | 充能计时器三件套，HUD 与 replay 使用 |
| +0x44 | `on_hud_anm` | `int (ce_card_t*, uint32_t)` | |
| +0x48 | `on_draw` | `int (ce_card_t*)` | |
| +0x4c | `on_run_reset` | `void (ce_card_t*)` | 局末重置 |
| +0x50 | `on_delete` | `void (ce_card_t*)` | 删对象：SDK 先调它（私有状态仍在，卡删自己的 VM 等），再释放私有状态、跳基类 `0x411410`。局中被倒吊人摧毁只走这里，`on_run_reset` 不会来 |

桩按**虚表**分派，不按对象：`CE_CARD` 为这张卡生成独立的桩函数集，直接调它的回调，没有运行时查表。
没写的回调保持基类槽（`xor eax,eax; ret`）。

`ce_card_t` 就是对象指针的别名，SDK 提供取值宏：`ce_card_id(c)`、`ce_card_entry(c)`（表行）、`ce_card_flags(c)`；
私有状态 `ce_state(c, T)`（§4）。透传槽由门里的守卫核对（AUDIT O4）。

## 3. 登记与对账

```c
CE_CARD(id, .回调 = 函数, ...);      /* 一张卡一条，放在 native/cards/<name>.c */
```

门里 `cards.js` 装完之后（`ce_cards_load` 之后、`ce_menu_setup` 之前）做注册错误与跨层契约检查。注册表按 id 直接索引，遍历顺序另存用于稳定日志：

| 情况 | 结果 |
| --- | --- |
| JSON 有、C 有 | 绑定；日志 `sdk: 61 bound (.on_bullet_created = on_bullet_created)` |
| JSON 有、C 无 | 允许；汇总行里计数 `N registered cards without behavior`——开发期正常 |
| C 有、JSON 无 | **FAIL**：有行为却没登记的卡进不了游戏，是 bug |
| C 里 id 越界、同一 id 两次或注册表溢出 | **FAIL**：constructor 记录首个错误，门统一拒绝启动并还原分配器上界 |
| JSON `category: 0`，C 没有 `active_recharge` | **FAIL**：主动类别与行为不一致 |
| C 有 `active_recharge`，JSON `category` 不是 0 | **FAIL**：主动行为与数据不一致 |

登记靠 `__attribute__((constructor))`（mingw 的 `.ctors`，DLL 装载时跑），加卡不改任何清单；门里再把已登记的行为与 JSON 对账。
`cards.js` 会先解析并校验完整批次，再一次性发布表行、名字和计数；后面的坏条目不会留下半张已写入的卡。

## 4. 私有状态

对象没有余量。`ce_state(c, size)` 返回这张卡对象的私有内存（首次调用分配并清零，同一对象重复调用返回同一块）。
实现：DLL 侧 256 槽表，键 = 对象指针；`operator_delete` 桩释放（`reset_cards` / 局末 `recount` / 即时卡删除都走这个槽，不会漏）。
`size` 上限 `CE_STATE_BYTES − 16` = 496 字节（2026-09-08 块 256 → 512，股市 id 80 要 430+）；第一批只有 10♠ 用它（金钱道具计数）；A♠ 靠计时器的 {279,280} 签名识别「刚复活」，不用状态。
块头 `CE_STATE_RESERVED`（16 字节）是 SDK 的（主动卡状态机），`ce_state` 给卡的是其后的空间——一张卡既是主动卡又有私有状态时两者不再互踩（青眼白龙是第一张）。

## 5. 引擎访问：`engine.h`

只放**一手确认过**的地址与偏移，每条带出处：

| 符号 | 值 | 出处 |
| --- | --- | --- |
| `SCORE` | `0x4cccfc` | `05-shop-and-money.md` §1 |
| `MONEY` / `MONEY_TOTAL_COLLECTED` | `0x4ccd34` / `0x4ccd30` | 同上 |
| `CURRENT_POWER` / `MAX_POWER` | `0x4ccd38` / `0x4ccd3c` | `01-object-model.md` §7 |
| `ABILITY_MANAGER_PTR` | `0x4cf298` | 卡链表首结点 `+0x1c`（结点 `{card,next,prev}`）、`owned[]` `+0xd70`、充能倍率 `+0xc58` |
| `PLAYER_PTR` | `0x4cf410` | 移速倍率 `+0x477ec`、无敌 zTimer `+0x47774`、道具回收四参 `+0x47988..94`、状态机 `+0x476ac`、聚焦 `+0x476cc` |
| 自机弹 | `bullet+0x9c` | int 伤害（`PlayerBullet__create` 写；`CardMomoyo` 覆写）|

辅助函数随卡长出来（方案 3 的约定：先在卡里写，出现第二次就提炼进 `sdk.h`）。已有的（`sdk.h`，调用约定一手，AUDIT O16）。
**加新的引擎调用前必看它全部 `ret N` 出口**——`Timer__*` 是 `ret 4` 却看着像无参 thiscall，第一版就栽在这（AUDIT O23）：

| 函数 | 干什么 | 引擎侧 |
| --- | --- | --- |
| `ce_give_card(id, mode, notify)` | 给玩家一张卡 = 商店成交的两步；分配返回 `-1` 时不调用 `mark_obtained` | `allocate_new_card(mgr; id, mode)` `0x411460` thiscall `ret 8` + 成功后 `mark_obtained(id, notify)` `0x418de0` fastcall |
| `ce_shop_pick_random(lo, hi, exclude[], n)` | 按商店随机池的规则抽一张（未拥有、本关可用、按权重、游戏 RNG）| `pick_weighted_random_offer` `0x416f50` fastcall(ecx=out, edx=lo; hi, exclude, n) `ret 0xc` |
| `ce_table_entry(id)` / `ce_entry_id(e)` | 表行（ctor 里 `card+0x4c` 还没写，用这个）| `TableCardData__get` `0x407d70` fastcall(ecx=id) |
| `ce_log(fmt, …)` | 一行进 `th18_card_expand.log` | — |
| `ce_play_sound(id, x)` | 音效，x = 世界 x（声像）| `0x476c70` stdcall(id) + xmm2 `ret 4` |
| `ce_play_voice(NAME, x)` | 语音（扩展 id `0x54`–`0x73`）| 同上；`NAME` 来自 `assets/voice/ORDER.txt`，见 §5.1 |
| （SDK 内部）`Timer__decrement / increment` | 充能 / 经过帧 | `0x409750` / `0x405990` thiscall(zTimer*, 未用栈参) **`ret 4`** |
| `CE_BULLET_MGR()` + `CE_BM_*` / `CE_BULLET_*` | 子弹池：2000 张，起点 `+0xec`，stride `0xfa0`，状态 `+0xf68`，`velocity/speed/angle` `+0x644/+0x650/+0x654` | `cancel_all` `0x4297a0` 的扫法 + ExpHP 结构 |
| `ce_cancel_radius(pos, r, max, mode)` | 半径消弹（弹 → 点道具），返回消掉的弹数；max 消满即停 | `0x429370` stdcall(pos, mode, max, tag) + XMM2 半径 `ret 0x10`；计数器 `mgr+0x7a41e8`（AUDIT O29a）|
| `ce_damage_rect(center, angle, life, dmg, w, h)` | 自机侧矩形伤害源：敌人自己判重叠扣血，每帧总量钳 `player+0x47984` | `0x45dfa0` stdcall + XMM2 宽 / XMM3 高 `ret 0x10`（O29b）|
| `ce_anm_get_vm / set_pos / set_color` | 按 id 找 VM、写 `vm+0x5f0` 坐标 / `vm+0x524` 颜色 | `0x488b40` thiscall(mgr; id) `ret 4`（O29c）|
| `ce_anm_interrupt(id, n)` / `ce_anm_delete(id)` | 触发脚本 `interruptLabel(n)`（Tenshi 用 1 收场）/ 标记删除 | `0x488be0` stdcall `ret 8` / `0x488cf0` stdcall `ret 4` |
| `ce_spawn_item(type, pos3, angle, speed)` | 池里放一个道具（type 1/2/4/6/9–0xe…），返回道具指针、池满失败 0。角度 −π/2、速 2.2 与原生 main-drop 同参 | `ItemManager__spawn_item` `0x446f40` thiscall（ecx = mgr `0x4cf2ec`）`ret 0x20`：8 栈参 (type, pos*, junk, angle, speed, timer, u7, u8)，**尾两槽必须压**（AUDIT §Z）|
| `ce_add_score_delta(delta)` | 分数增减（可为负），钳 `0..999999999` | `SCORE` `0x4cccfc` int32（`05-shop-and-money.md` §1）|
| `ce_player_pos()` / `ce_player_x()` | 自机 float3 坐标 / 只要 x（放音效的声像位置）；指针为空时 NULL / 0 | `CE_PLAYER_X` `0x620` 是 float3 基址 |
| `ce_enemy_pos(e)` / `ce_enemy_lockable(e)` | 敌人 float3 坐标 / 可锁定（非空且不带 `NO_LOCK`）| `CE_ENEMY_POS_X` `0x1270` |
| `CE_ENEMY_FOREACH(em, e)` | 沿敌人链表遍历（带 guard；`e` 可能为空，用 `ce_enemy_lockable` 一并滤）| 结点 {entry, next, prev} |
| `ce_spell_flags()` / `ce_spell_active()` | 符卡标志位 / 「现在在打符卡吗」；管理器为空时读作 0 | `SPELLCARD+0x78` |
| `ce_refuse(card, why)` | 主动卡拒绝发动：无效音 + 一行日志 + `CE_ACTIVATE_REFUSED` | 见 §9 |
| `ce_enemy_by_id(em, id)` | 按 `enemy_id` 在敌人链表里找 | `get_boss_enemy_full` `0x4237f0` 的走法 |
| `ce_option_*` | 装备卡子机槽：认领 / 申请 / 补申请 / 收起（§12）| `Player__allocate_option` `0x40a790` |
| `ce_hud_digit_*` | HUD 小卡上的一位数字：挂图标 / 设值 / 清除（§15）| `ce_anm_spawn_ui` + `ce_anm_set_sprite` |

### 5.1 语音

**语音就是 SE**，只是 id 落在扩容出来的 `0x54`–`0x73`（32 个槽）。可叠加、跟随游戏的 SE 音量、
不做独占通道、不打断、过关不停 —— 设计决定见
[`docs/superpowers/specs/2026-09-05-voice-expand-design.md`](../../../docs/superpowers/specs/2026-09-05-voice-expand-design.md) §4。

三步：

1. wav 放 `assets/voice/<NAME>.wav`（PCM，建议 16-bit 单声道），`assets/voice/ORDER.txt` 加一行 `NAME`；
2. `patch/th18/voice.js` 加一条 `{"KEY": {"wav": "NAME", "id": 0x54+行号, "volume_db": 0, "priority": 100}}`；
3. `make voice`（会与 ORDER.txt 对账，不一致直接停）→ 代码里 `ce_play_voice(NAME, player_x())`。

细则见 [`assets/voice/README.md`](assets/voice/README.md)。**响度要对齐基准**：
零售 wav 一律 peak 归一化、响度差异全靠行内 dB 衰减（且只能衰减），所以新音效的
方均振幅要自己做到位 —— 目标 −10 dBFS，`make voice` 会检查。引擎侧的表结构与两条不变式见
[`engine/_shared/th18-sound-table.md`](../../../engine/_shared/th18-sound-table.md)，
站点与审计见 [`AUDIT.md`](AUDIT.md) §Q。

**别用裸数字**：`0x00`–`0x53` 是零售音（一个空闲 id 都没有），`0x54` 起才是我们的。
`ce_play_voice` 走 `voice_ids.h` 的宏，写错 NAME 编译期就报错。

## 6. SDK 事件（虚表之外）

有些效果不在 21 个槽里。SDK 用 thcrap 断点补，回调写在同一张 `CE_CARD` 表里，SDK 在断点里沿 `mgr+0x18` 卡链表找出有该回调的对象逐个调：

| 事件 | 断点 | 回调 | 本批用 |
| --- | --- | --- | --- |
| `on_item_score` | `0x446cf6`（`collect_money_item` 里 `lea eax,[edi+0xc2c]`，esi = 身价，已钳 ≥10；之后 `push esi` 给弹窗、`mul esi` 计分）| `void (ce_card_t*, int32_t *value)` | 暂无（留给以后的得分卡）|
| `on_item_money` | `0x446d28`（同函数尾部 `inc [MONEY_TOTAL]`，紧接 `inc [MONEY]`）；断点把 `*bonus` 同时加进 `MONEY` 与 `MONEY_TOTAL_COLLECTED` | `void (ce_card_t*, int32_t *bonus)` | 10♠：每第 10 个金钱道具 `*bonus += 1` |
| `on_enemy_drop_pre` | `0x430510`（`Enemy__drop_items_and_notify_cards` 入口，撒之前）| `void (ce_card_t*, int32_t *counts)` | 加倍：掉落 ×2 |
| `on_bomb_spent` | `0x4203bc`（`do_bomb` 里 `consume_bomb` 刚返回）| `void (ce_card_t*)` | 腐化：扣上限 |
| `on_hud_anm`（虚表 `+0x44`）| 不是事件：引擎建好本卡的 HUD 图标后（`0x408fc4`）把图标根 VM 的 anm id 交给卡；图标重建时会再调一次。真银圣剑用它在图标上叠数字（`ce_anm_spawn_ui` + `ce_anm_set_sprite`，AUDIT §X）| `int (ce_card_t*, uint32_t anm_id)` | 真银圣剑：耐久数字 |
| `on_shop_open` | **不是新断点**：`shop.c` 的 `ce_shop_reopen`（`0x443b05`）在 MSG 请求开第一家店那一帧调 `ce_sdk_on_shop_open()`；商店对象这一帧稍后才建，改 `MONEY` 店里看到的就是改后的值；重开的第二家不算 | `void (ce_card_t*)` | 黄金小丑：金钱 +20％ |
| `on_item_frame` | `0x446ee3`（`ItemManager__on_tick` `0x446ec0` 最后一道门卫 `cmp [0x4cf2a4],0`）。每个正常游玩帧、道具池 body 前到一次；handler 只看**声明了该槽的卡**，不读 CMP 结果 | `void (ce_card_t*)` | 辉针城：批开时查池，池空 → 关批结算 |
| `on_item_collect` | `0x44625f`（type 分派前一条 `mov edx,[edi+0xc78]`，副作用未发生）。type 用 `*(uint32_t*)(item+0xc78)` 读，别信 EDX（窗前值不可靠） | `void (ce_card_t*, uint32_t type, const void *item)` | 辉针城：批集计数 + stash SCORE |
| `on_item_commit` | `0x44658a`（共享收尾起点 `movss xmm2,[esi]` + `push 0x25`）。本道具分值**已按 type 计入 SCORE**、公共收尾（清槽等）之前 | `void (ce_card_t*)` | 辉针城：`(SCORE − stash)` 差量入批 |
| `on_shot_damage` | `0x45f2a2`（`enm_compute_damage_sources` `0x45f0f0` 计分路径 `mov eax,0xee666667` + `imul esi`）。每帧每敌一次可计分命中：dmg 已钳 ≥1、pos = 敌世界坐标 (x,y)；聚焦 / 炸弹 / param_6≠0 的路径被窗口前判定挡掉（handler 再门一道，纵深防御） | `void (ce_card_t*, int32_t damage, const float *pos)` | 辉针城：非聚焦射击命中按帧节流掉 P |

**局中删一张卡**：`ce_destroy_card(victim)`（`sdk.c`，AUDIT §W）照 recount `0x4080e0` 的删卡分支——摘链、按 flags 减类别计数、
`num_total−1`、`owned[id]` 清零、虚表 `+0x50` 删对象、引擎自己的 `create_all_card_lists_for_hud(mgr, 0)` 重画 HUD。
**当前选中的主动卡也能删**（2026-09-09）：删前把 `mgr+0x38` 摘空、删后调引擎的 `set_selected_active_card(-1)` 重选并刷 HUD（AUDIT §AJ）。只能在没人遍历链表的时候调（`ctor` / 事件断点里可以，`on_tick_2` 这类遍历桩里不行）。
`ce_pick_random_card(exclude)` 用游戏 RNG 在链表里挑一张（只排除 `exclude` 自己）。倒吊人是第一个用户。

以后加事件照此：一个断点 + 一个回调名 + AUDIT 一条。

## 7. 第一批：黑桃五张

| 牌 | id | 槽 / 事件 | 实现 | 状态 |
| --- | --- | --- | --- | --- |
| 10♠ 道具金钱 +10％ | 58 | `on_item_money` | 私有状态计数，每第 10 个金钱道具 `*bonus += 1`（确定性计数而非随机：replay 靠输入重放，自带随机会失同步）| ✅ 已反（AUDIT O10、O14）|
| J♠ 移速 +10% | 59 | `on_tick_2` | `player+0x477ec`（每帧移速倍率）`*= 1.1`。Player tick 末尾复位 1.0，AbilityManager tick（优先级 0x16）先于 Player（0x17），所以只能在 `on_tick_2` 写；`on_tick` 在复位前，白写 | ✅ 已反（AUDIT O7）|
| Q♠ 道具回收范围 | 60 | `on_load` | 抄 `CardNitori` 的 `on_load`：`attract_speed / collect_radius / attract_r_focused / attract_r_unfocused` = {10, 30, 250, 250}（默认 {5,30,70,70}，Nitori {10,30,110,110}；字段名来自 ExpHP）| ✅ 已反 |
| K♠ 伤害 ×1.1 | 61 | `on_bullet_created` | `bullet+0x9c`（int）`= d*11/10`；`PlayerBullet__create` 在调槽前已写好它 | ✅ 已反（AUDIT O9）|
| A♠ Miss 后无敌 +50% | 62 | `on_tick_2` | 无敌 zTimer `player+0x47774`；复活把它置成 {prev 279, cur 280, 280.0}，这个组合只在刚置好那一帧出现 → 改成 420。`+0x14` 槽在复活置值**之前**触发，所以不用它 | ✅ 已反（AUDIT O8）|

数值（`price_tier` / `weight` / sprite）在 `patch/th18/cards.js` 里给（`_255` 自带的卡池，`initial_unlocked: 1`）；sprite 先全用 116/117 占位。
已实装卡的总表在 [`CARDS.md`](CARDS.md)（含 63 强欲之壶：第一张**即时卡**，`ctor` 里 `ce_shop_pick_random` ×2 → `ce_give_card` → `return 1` 当场销毁；`deck_visible: 0`）。
「皇家同花顺」（五张齐 → 隐藏效果）不在本批：接缝是购买 `AbilityShop__on_tick` `0x4185c7` + `owned[]`。

## 8. 开发循环

`th18/cards_dev.js`（只放 `_test`，读法同 `cards.js`；实现在 `cards.c` / `bp_trace.c`）：

```json
{ "start_deck": [58, 59, 60, 61, 62], "trace": true }
```

- `start_deck`：`_test` 的初始卡组钩子改成断点 `ce_test_deck`（`0x407ee3`），空槽依次填这些 id；每次 `reset_cards` 从头发。
- `trace`：每张卡每个槽第一次被调记一行 `trace: card 62 on_tick_2 (+0x2c) first hit`；绑定时记 `bound to vtable`。

验收 = 日志里 `sdk: 58 bound (.on_item_score = on_item_score)` 五行 + trace 行 + 游戏里体感（移速、回收、无敌时长可目测；伤害与得点看数字）。

## 9. 主动卡（C 键）

零售 12 张主动卡共用一套模板（`04-active-cards.md` §3–§5，样本 Tenshi）；SDK 把它做成三个回调字段（AUDIT O19–O21）：

```c
CE_CARD(64, .active_recharge = 3600, .on_activate = f, .on_active_tick = g);
```

| 字段 | 语义 |
| --- | --- |
| `active_recharge` | 充能帧数（装填时 × `mgr+0xc58` 倍率）。非 0 = 主动卡：绑定时 SDK 把 flags 改成主动 case 的写法（`& ~0x46 \| 8`）、写 `+0x48`、按 Tenshi 初始化两个 zTimer——引擎据此把它放进主动卡组 / HUD / C 键分派 |
| `on_activate(c)` | C 键发动（门：state 空闲且充能到底；SDK 已装填充能、置「释放中」）。返回 0 = 瞬发，直接收尾；1 = 进持续态；`CE_ACTIVATE_REFUSED` = 条件不满足（SDK 退回充能、不算发动；卡自己放 0x10 无效音）|
| `on_active_tick(c, elapsed)` | 持续态每帧；返回 0 = 结束进收尾 |

SDK 在 `+0x2c` 桩里先跑状态机（空闲：清释放位、门控下递减充能；收尾：经过帧 > 8 回空闲；每帧门控下递增经过帧；
门控 = 零售的：`zGui.msg == 0`（不在对话）且 `zEnemyManager.enemy_count_real != 0`——**场上没敌人时 C 键无效、充能也不走**），
再调卡自己的 `on_tick_2`；`+0x34`（关卡开场）清状态与经过帧、不动充能，`+0x4c`（局末）连充能一起清——与零售一致。
状态放私有状态（零售放 `+0x54`，超出 0x54 字节的基类对象）。充能存取 / HUD / replay 用的槽 `+0x38..+0x40` 透传基类。
主动卡的 JSON：`category: 0`。

引擎辅助：`ce_play_sound(id, x)`（`0x476c70` stdcall + xmm2，三行内联汇编，AUDIT O22）；
`ce_anm_spawn(anm, script, layer)`（`0x405bf0` thiscall ret 0x10，AUDIT O24）：从 `CE_ABILITY_ANM()`（`ability.anm`）起一个脚本挂 world 列表，
实体坐标 (0,0,0) = 场地正中，返回 anm id。特效脚本与卡图副本用 [`assets/ability/`](assets/ability/README.md) 追加进 `ability.anm`，
脚本号 / sprite 号由 `build_ability.py` 生成到 `anm_ids.h`（`CE_ANM_ABILITY_SCRIPT_*`）。样例：反转牌 `on_activate` 起 `script68` 亮牌一圈。
引擎侧一手：[`engine/anm/th18/01-vm-instantiate.md`](../../../engine/anm/th18/01-vm-instantiate.md)。
`ce_add_life()` / `ce_add_bomb()`（`0x4575f0` 裸 ret / `0x457690` ret 4 带 dummy，AUDIT O26）：引擎自己的加法，钳上限、放音效、起特效；上限要一起涨照零售先 `+1` 钳 7。
`ce_gui_update_lives()`（`0x441f10` thiscall ret 0xc，AUDIT O28c）：改了 `CURRENT_LIVES` 之后刷 HUD 残机行。
`ce_owned(id)`：读本 mod 搬迁后的 `owned[]`（`mgr+0xd70`）。注意 `owned[自己]` 在 ctor 之后才置 1。
★ **ctor 每关开始会再被调一次**（引擎对卡组里每张卡调 +0x00，实跑证实）：「获得即触发」的效果在 ctor 里先 `if (!ce_fresh_acquire(c)) return 0;`（= `owned[自己] == 0`）。即时卡（返回 1 当场销毁）不受影响。集卡判定用 `ce_royal_flush_ready(owned, self, set, n)`（sdk_core，有单测）。

## 10. 边界与不做的

- 即时卡：`ctor` / `dtor` 里施加效果后 `return 1`（零售同款）；这种卡 `deck_visible: 0`（mode 1 不调 ctor，初始携带会是死卡）。ctor 里可以再调 `allocate_new_card`（AUDIT O17）。
- 激光：`LASER_MANAGER` 另一套，反转牌不动它。
- 对象大小固定 0x54：要更大对象的卡走第 4 节的私有状态，不改分配。
- 事件断点每个都要过 AUDIT；本批只加一个。
- 图鉴 ≤ 127 条目（新卡 ≤ 71）、`abcard.anm` sprite 追加：另外的会话。

## 13. 自机弹伤害倍率层

「给自机弹加伤害」这件事出现三次之后收进了 SDK（2026-09-08，AUDIT §AA）。卡不再自己写 `bullet+0x9c`：

```c
CE_CARD(61, ..., .bullet_damage_permille = 1100);       /* K♠：常数 ×1.1，写完就不用管了 */
CE_CARD(76, ..., .bullet_damage_permille = CE_DMG_UNIT);/* 古锭刀：初值 ×1 */
    ce_set_bullet_damage_permille(c, CE_DMG_UNIT * 2);  /*   终符沿上报一次新值 */
```

千分比，`CE_DMG_UNIT` = 1000 = ×1。SDK 把所有**在场**参与者的值连乘成聚合倍率，
挑其中 **id 最小的一张**当「落笔人」，只有它在 `+0x28` 桩里对 `bullet+0x9c` **就地乘一次**。于是：

- **单次取整**。三层 +20% 加法叠、基值 90：逐张各自取整会走成 `90 → 108 → 129 → 154`，一次算完是 **144**。
- **与卡在链表里的顺序无关**，同一副牌组每次结果相同。
- **每帧伤害上限自动跟着抬**：落笔人在 `+0x2c` 里 `ce_raise_damage_cap(sht+0x28 基值 × 聚合)`。
  这条以前要每张加伤害的卡自己记得，K♠ 就漏了——贴脸打 boss 时上限（Reimu 90 / Sakuya 60）本来就被自机弹吃满，
  只翻弹不抬上限等于白翻（§V20 / §Y5 / §AJ10 是同一件事的三次重犯）。
- **生命周期不用管**：绑定时登记声明的初值、`+0x50`（含倒吊人摧毁）退出聚合、`+0x4c`（局末）回到初值。

**HUD 上有读数**：聚合倍率不是 ×1 时，落笔人会在 `+0x48 on_draw` 里把 `x1.60` 写在**火力值左边**
（`ce_hud_text`，§14）。卡不用管这件事；没有加伤害的卡在场时一个字都不画。

**边界写在名字里**：`bullet_damage` 只管 `bullet+0x9c`。炸弹、以及 `ce_damage_rect` 建的伤害源
（青眼光束 / 炎魔火球 / 破损核心电弧 / 真银圣剑挥砍）走另一条管线，**不**受它影响。
另外零售 `CardMomoyo`（54）是「从 shooter 基值重算并覆写」派，与我们谁靠后谁说了算——
所以本层选择**就地乘**而不是重算，不然带着她时她的擦弹加成会被抹掉（AUDIT AA3）。

## 14. 在 HUD 上写字：`ce_hud_text`

```c
ce_hud_text(478.0f, 182.0f, 0xff800000u, "x%d.%02d", n, frac);
```

零售 `CardMomoyo__on_draw` `0x4110a0` 就是「一张卡在 HUD 上画数字」的先例，这个原语照它复刻（AUDIT §AC）。三条规矩：

- **只能在虚表 `+0x48 on_draw` 里调**。`AbilityManager__on_draw` `0x408ab0` 每帧遍历卡链表调它；
  在 `on_tick_2` 里调是 tick 阶段，不对。
- **坐标用 Gui 自己那套字面量空间**（`FUN_0043cd00` 里全是常数）：火力 `"%d."` 在 `(540, 182)`、
  小数 `(560, 189)` scale 0.6、`"/%d."` `(574, 182)`、金钱右对齐 `(620, 204)`。不用做分辨率换算。
- **颜色是 BGRA 一个 dword**，画完自动还原成白。想和火力看起来是一体的就用 `CE_HUD_POWER_COLOR`。

⚠️ 字号 / 对齐用的是**环境值**（照 Momoyo，我们只设字色）—— 所以位置和大小要实跑一次才能定死，
和真银圣剑的 HUD 数字是同一句话。

## 11. 文件

```
native/
├── sdk/public.h     稳定作者接口：ce_card_t、hook 类型、状态与公开动作声明
├── sdk.h            TH18 v1.00a 兼容入口：包含 public.h，并提供 CE_CARD、对象访问器与引擎 helper
├── engine.h         一手地址 / 偏移（每条带出处）
├── sdk_core.h/.c    O(1) 注册表 / 跨层契约 / 状态槽（纯逻辑，make test-host）
├── sdk_actions.h/.c 可注入的 give-card 事务（纯逻辑，make test-host）
├── sdk.c            断点 ce_card_bind、ce_item_score；门里的守卫与对账；trace
├── anm_ids.h        生成物：ability.anm 里我们追加的 sprite / 脚本号（make anm）
├── sht_ids.h        生成物：pl0X.sht 里我们追加的 shooterset 索引（make sht，§12）
└── cards/           每张卡一个 .c（Makefile 通配）
    ├── s10.c  sj.c  sq.c  sk.c  sa.c （黑桃五张；其余每卡一个 .c，见 CARDS.md：double.c、collect2x.c …）
    └── 带纯逻辑的还有一个 <名>_core.c/.h（主机单测），如 double_core、collect2x_core
    ├── s10.c  sj.c  sq.c  sk.c  sa.c  peashooter.c  ouroboros.c …
    └── 带纯逻辑的还有一个 <名>_core.c/.h（主机单测），如 blue_eyes_core、broken_core_core
```

审计：[`AUDIT.md`](AUDIT.md) §O。

## 12. 装备卡（子机 + 自己的弹幕）

零售的「子机会开火」那类卡（各角色 `*_OP`）是两个槽的事，本 mod 现在能照着写。
**破损核心（71）**（`cards/broken_core.c`）只用了前一半（子机），后一半（SHT 连射）它第一版用过、
后来换成定点伤害源——它要的是「瞬发单体」，自机弹天生要飞过去。
**下面 ②–⑤ 的第一个真用户是豌豆射手（77）**（`cards/peashooter.c`，两组 shooterset 切换 + 自己数 short_timer，AUDIT §AJ）。引擎一手在
[`engine/sht/th18/`](../../../engine/sht/th18/README.md) 与
[`engine/card/th18/03-hooks.md`](../../../engine/card/th18/03-hooks.md) §5，审计 AUDIT §U。

```c
typedef struct { ce_option_t opt; bc_state_t st; } bc_card_t;   /* 子机槽存私有状态，不是 card+0x54 */

/* ① 生成子机：脚本号是 ability.anm 的（子机长什么样由我们画）*/
static int on_power_level_change(ce_card_t *c) {
    bc_card_t *s = ce_state(c, bc_card_t);
    if (s) ce_option_alloc(c, &s->opt, 0x18 /* 横向偏移 */, CE_ANM_ABILITY_SCRIPT_BROKEN_CORE_ORB, "broken_core");
    return 0;
}
/* ②a 每帧拿它：认领（槽是公共池，会被回收）；没有就按 60 帧的间隔补申请一次 */
uint8_t *o = ce_option_get(c, &s->opt, 0x18, CE_ANM_ABILITY_SCRIPT_BROKEN_CORE_ORB, 60, "broken_core");
if (!o) return 0;
/* ②b 开火：给一组 shooter 的索引；弹从子机位出膛（坐标用 ce_option_x/y）*/
ce_tick_shooters_for_card(o, short_timer, long_timer, CE_SHT_SET_BROKEN_CORE);
/* ③ on_load / on_run_reset 收起 */
ce_option_drop(c, &s->opt);
```

四条要点：

- **子机指针别存 `card+0x54`**（零售存那儿）：新卡对象只有 `0x54` 字节，那已经在对象外。用 `ce_state()`。
  槽是 12 个的公共池，每次用前认领一次（`option+0x00 != 0 && option+0xd0 == card+0x08`）——
  这套认领 / 申请 / 收起 2026-09-08 收进了 SDK 的 `ce_option_*`（AUDIT §AD），卡里不用再抄一遍。
- **弹幕数据**要往四个 `pl0X.sht` 里加一组 shooterset：`assets/sht/append_shooterset.py` 的 `APPEND` 表加一项，
  `make sht` 生成 `build/sht/*.sht` 与 `sht_ids.h`（`CE_SHT_SET_<NAME>`）。零售用了 23 项、总共 40 项，
  **还剩 16 个空位**。脚本每次都回读校验解析不变式（AUDIT U5）。
- **弹的贴图也在 `ability.anm`**：引擎开火期间把 `player+0x10` 换成它，所以 shooter 的 anm 字段填
  `CE_ANM_ABILITY_SCRIPT_*`。不用碰四个 `pl0X.anm`。
- **想瞄准**：把角度写 `player+0x479cc`，该组 shooter 的 `func_on_init` 填 `5`（`CardAlice` 同款，
  出膛定向、不追踪）；贴图朝向在脚本里写 `rotateAuto(1)` 就够，引擎每帧跟着弹的实时角度转。
- **想不按射击键也打**：`on_tick_shooters`（`+0x1c`）只在按住射击键时广播。改挂 `on_tick_2`，
  shooter 的 `fire_rate` 填 1、调用时 timer 传 0（`0 % 1 == 0` 必发），节奏交给自己的计数器。
  ⚠️ `fire_rate` **不能填 0**：这条路径上是裸取模，会除零。
- **节奏自己数**：`ce_tick_shooters_for_card` 的 `short_timer` 是我们传的，而引擎拿它**只做**
  `timer % fire_rate == start_delay` 这一处判定（往下传给 `PlayerBullet__create` 的那份根本没被用，AUDIT Z3）。
  所以别用引擎自己的 `player+0x476d4`——它只在 0..14 循环、松开射击键还会跳，表达不出「85 帧一颗」这种节奏。
  自己维护一个计数器，换形态时清 0，切过去那一帧就能立刻出膛（豌豆射手 77 的做法）。
- **每敌每帧伤害上限**：单发伤害超过 `sht+0x28`（Reimu 90 / Marisa 160 / Sakuya 60 / Sanae 120）就会被砍。
  要么把伤害拆成多发小弹（零售子机卡都是 6–32），要么在 `on_tick_2` 里 `ce_raise_damage_cap()`
  （只在自己的弹可能命中的那几帧抬，别常开——那是对自机弹的隐形加强）。

## 15. HUD 小卡上的一位数字：`ce_hud_digit_*`

一张卡想在自己的 HUD 图标上显示一个数字（真银圣剑的剩余耐久、衔尾蛇的层数）时，三行就够：

```c
typedef struct { ob_state_t st; ce_hud_digit_t hud; } ob_card_t;

static int on_hud_anm(ce_card_t *c, uint32_t id)      /* +0x44：引擎交来图标根 VM 的 anm id */
{ ce_hud_digit_attach(&s->hud, id, (int)s->st.stacks, "ouroboros"); return 0; }

static int on_tick_2(ce_card_t *c)                     /* 每帧对账：图标会被引擎重建 */
{ if (s->hud.icon_id) ce_hud_digit_set(&s->hud, (int)s->st.stacks, "ouroboros"); return 0; }

static void on_run_reset(ce_card_t *c) { ce_hud_digit_clear(&s->hud); }
```

SDK 读图标根 VM 的实体坐标（`vm+0x5f0`）、在 **UI 列表**里起 `ability.anm` 的 `HUD_DIGIT` 脚本钉在同一点，
值变了 `set_sprite` 换数字；图标没了删数字、图标重建时旧数字先删。值钳在 `0..CE_HUD_DIGIT_MAX`（贴图只有十张）。
⚠️ 数字**相对图标的偏移写在脚本里**，是唯一要实跑调的量（日志会打图标坐标）。机制与一手在 AUDIT §X。
