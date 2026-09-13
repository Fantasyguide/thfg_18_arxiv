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

原生分配器 `allocate_new_card` 对 id ≥ 57 全走 case 56（`0x411489`）：游戏自己 `new(0x54)` 一个基类 `zAbility`，
虚表 `0x4b4c78`，然后进公共尾段。尾段第一条真正的写是 `0x412cec mov [esi+4], ebx`（**esi = 卡对象，ebx = id**）。

SDK 在这里挂 thcrap 断点 `ce_card_bind`（cavesize 6：这条 + 后面的 `mov eax,[edi+0x28]`，都无相对寻址）：

```
if (behavior_by_id[ebx] != NULL)  *(void **)esi = 那张卡的虚表;
return 1;   /* 放行原指令 */
```

换完虚表之后尾段照常：`mode 0/2` 调 ctor 槽（`0x412d11`）、`mode 2/3` 调 dtor 槽、入链、`owned[id]=1`、`+0x4c` = 表行指针、
建 HUD。所以：

- **构造 / 析构语义与原生完全一致**（`02-lifecycle.md` §2）：ctor 返回非 0 = 当场删卡 = 即时卡。
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
| +0x0c..+0x1b | 链表结点（**表头在 `mgr+0x18`、首结点在 `mgr+0x1c`**；`0x412e90` 插入）|
| +0x30、+0x44 | 位标志（case 56 清 bit0）|
| +0x48 | 充能时长（主动卡）|
| +0x4c | `zTableCardData*`（尾段写）|
| +0x50 | flags：bit0 = 存档/replay 装载，bit3 = 主动，bit6 = 装备，bit5 = 开火中。绑定时 SDK 按 JSON 改：`active_recharge` → 主动写法；`category: 1` → 原生装备写法 `& ~0x0a \| 0x44`（HUD 装备行 / `num_equipment`，AUDIT X11）|

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
| `ce_hud_vm_text_*` | **贴图 VM 版数字/符号**（TH18 font 2 原生字形，暂停时会被模糊罩盖住）（§16）| `ce_anm_spawn_ui` + `ce_anm_set_sprite` |

### 5.1 语音

**语音就是 SE**，只是 id 落在扩容出来的 `0x54`–`0x73`（32 个槽）。可叠加、跟随游戏的 SE 音量、
不做独占通道、不打断、过关不停 —— 设计决定见
[`docs/superpowers/specs/2026-09-05-voice-expand-design.md`](../../../docs/superpowers/specs/2026-09-05-voice-expand-design.md) §4。

三步：

1. wav 放 `assets/voice/<NAME>.wav`（PCM，建议 16-bit 单声道），`assets/voice/ORDER.txt` 加一行 `NAME`；
2. `patch/th18/voice.js` 加一条 `{"KEY": {"wav": "NAME", "id": 0x54+行号, "volume_db": 0, "priority": 100}}`；
3. `make voice`（会与 ORDER.txt 对账，不一致直接停）→ 代码里 `ce_play_voice(NAME, player_x())`。

细则见 [`assets/voice/README.md`](assets/voice/README.md)。**响度要对齐基准**：
原生 wav 一律 peak 归一化、响度差异全靠行内 dB 衰减（且只能衰减），所以新音效的
方均振幅要自己做到位 —— 目标 −10 dBFS，`make voice` 会检查。引擎侧的表结构与两条不变式见
[`engine/_shared/th18-sound-table.md`](../../../engine/_shared/th18-sound-table.md)，
站点与审计见 [`AUDIT.md`](AUDIT.md) §Q。

**别用裸数字**：`0x00`–`0x53` 是原生音（一个空闲 id 都没有），`0x54` 起才是我们的。
`ce_play_voice` 走 `voice_ids.h` 的宏，写错 NAME 编译期就报错。

## 6. SDK 事件（虚表之外）

有些效果不在 21 个槽里。SDK 用 thcrap 断点补，回调写在同一张 `CE_CARD` 表里，SDK 在断点里沿 `mgr+0x1c` 起的卡链表找出有该回调的对象逐个调（**`+0x18` 是表头、`+0x1c` 才是首结点**，`engine.h` 的 `CE_MGR_CARD_LIST_FIRST`）：

| 事件 | 断点 | 回调 | 本批用 |
| --- | --- | --- | --- |
| `on_item_score` | **两个站点同一份 handler**：`0x446cf6`（向上分支：线上 / `state==3`）+ **`0x446caa`**（线下且非真空飞行；`0x446cb2 jmp 0x446cfc` 会绕过前者）。esi = 身价 | `void (ce_card_t*, int32_t *value)` | 暂无（留给以后的得分卡）|
| `on_item_money` | `0x446d28`（同函数尾部 `inc [MONEY_TOTAL]`，紧接 `inc [MONEY]`）；断点把 `*bonus` 同时加进 `MONEY` 与 `MONEY_TOTAL_COLLECTED` | `void (ce_card_t*, int32_t *bonus)` | 10♠：每第 10 个金钱道具 `*bonus += 1` |
| `on_enemy_drop_pre` | `0x430510`（`Enemy__drop_items_and_notify_cards` 入口，撒之前）| `void (ce_card_t*, int32_t *counts)` | 加倍：掉落 ×2 |
| `on_bomb_spent` | `0x4203bc`（`do_bomb` 里 `consume_bomb` 刚返回）| `void (ce_card_t*)` | 腐化：扣上限 |
| `on_hud_anm`（虚表 `+0x44`）| 不是事件：引擎建好本卡的 HUD 图标后（`0x408fc4`）把图标根 VM 的 anm id 交给卡；图标重建时会再调一次。真银圣剑用它在图标上叠数字（`ce_anm_spawn_ui` + `ce_anm_set_sprite`，AUDIT §X）| `int (ce_card_t*, uint32_t anm_id)` | 真银圣剑：耐久数字 |
| `on_shop_open` | **不是新断点**：`shop.c` 的 `ce_shop_reopen`（`0x443b05`）在 MSG 请求开第一家店那一帧调 `ce_sdk_on_shop_open()`；商店对象这一帧稍后才建，改 `MONEY` 店里看到的就是改后的值；重开的第二家不算 | `void (ce_card_t*)` | 黄金小丑：金钱 +20％ |
| `on_item_frame` | `0x446ee3`（`ItemManager__on_tick` `0x446ec0` 最后一道门卫 `cmp [0x4cf2a4],0`）。每个正常游玩帧、道具池 body 前到一次；handler 只看**声明了该槽的卡**，不读 CMP 结果 | `void (ce_card_t*)` | 辉针城：批开时查池，池空 → 关批结算 |
| `on_item_collect` | `0x44625f`（type 分派前一条 `mov edx,[edi+0xc78]`，副作用未发生）。type 用 `*(uint32_t*)(item+0xc78)` 读，别信 EDX（窗前值不可靠） | `void (ce_card_t*, uint32_t type, const void *item)` | 辉针城：批集计数 + stash SCORE |
| `on_item_commit` | `0x44658a`（共享收尾起点 `movss xmm2,[esi]` + `push 0x25`）。本道具分值**已按 type 计入 SCORE**、公共收尾（清槽等）之前 | `void (ce_card_t*)` | 辉针城：`(SCORE − stash)` 差量入批 |
| `on_shot_damage` | `0x45f2a2`（`enm_compute_damage_sources` `0x45f0f0` 计分路径 `mov eax,0xee666667` + `imul esi`）。每帧每敌一次可计分命中：dmg 已钳 ≥1、pos = 敌世界坐标 (x,y)；聚焦 / 炸弹 / param_6≠0 的路径被窗口前判定挡掉（handler 再门一道，纵深防御） | `void (ce_card_t*, int32_t damage, const float *pos)` | 辉针城：非聚焦射击命中按帧节流掉 P |
| `on_enemy_death` | `0x430240`（`FUN_00430240`，SDK 名 `Enemy__die`；**在引擎撒主掉落之前**、链表仍挂着）| `void (ce_card_t*, const ce_enemy_death_t*)`；`kind` 用**栈扫描**（`regs->esp` 起扫 12 dword 命中七条 call site；`0x42f3b7` = 打死主路）；事件带 `retaddr/esp/ret_slot` | 共享事件（109/110/112/113）|
| `on_native_shot_suppress` | 复用 `0x45ebfb`（与 `on_shot_set_index` 同点）：**先问 set index，有覆盖则不抑制** | `int (ce_card_t*)`，非 0 = 本帧跳过原生行循环 | 108（低速妖机体）/ 109（蓄力拦 Z）|
| `on_shot_damage_any` | **与 `on_shot_damage` 同一个断点**（`0x45f2a2`）、同一份实参，但**不按聚焦过滤**（仍排除 bomb 生效帧与 `param_6 != 0` 的路径）；`focused` 由 SDK 传 | `void (ce_card_t*, int32_t dmg, const float *pos, int focused)` | 107（森罗结界要高速/低速两条命中通道）|

**局中删一张卡**：`ce_destroy_card(victim)`（`sdk.c`，AUDIT §W）照 recount `0x4080e0` 的删卡分支——摘链、按 flags 减类别计数、
`num_total−1`、`owned[id]` 清零、虚表 `+0x50` 删对象、引擎自己的 `create_all_card_lists_for_hud(mgr, 0)` 重画 HUD。
**当前选中的主动卡也能删**（2026-09-09）：删前把 `mgr+0x38` 摘空、删后调引擎的 `set_selected_active_card(-1)` 重选并刷 HUD（AUDIT §AJ）。只能在没人遍历链表的时候调（`ctor` / 事件断点里可以，`on_tick_2` 这类遍历桩里不行）。
`ce_pick_random_card(exclude)` 用游戏 RNG 在链表里挑一张（只排除 `exclude` 自己）。倒吊人是第一个用户。

以后加事件照此：一个断点 + 一个回调名 + AUDIT 一条。

> **⚠ 多卡同装时谁说了算 → 见 [`EVENTS-CONTRACT.md`](EVENTS-CONTRACT.md)**：每个事件属于哪一类合成语义
> （累加 / 聚合公式 + 单一落笔 / 先加后乘 / 选举覆盖 / OR / 广播只读），规则**不依赖卡链顺序**。
> 该文同时记着三处已定裁决：`on_shot_set_index` **咆哮(117) 优先**、低速子机**id 最小者落笔**（108 胜 111）、
> `on_enemy_drop_pre` 的**乘数作用在最终总数上**（= 加倍(69) 的意图是 ×2，即使实现写的是 +1）。

### 6.1 声明层（2026-09-12，系统卡共用）

多张卡同时想改同一个引擎状态时，**不许各写各的**（谁后遍历到谁说了算 = 依赖卡链顺序）。统一写成
「卡**声明**意图 → 层**聚合** → 每帧由 SDK 或**被选举的那张卡**落一笔」。纯逻辑在 `sdk_core.c`
（主机单测 `tests/test_sdk_core.c`，无引擎依赖），落笔在 `sdk.c` / `sdk.h`。合成语义分类见
[`EVENTS-CONTRACT.md`](EVENTS-CONTRACT.md) §1。

| 层 | 卡侧 API | 聚合 / 落笔 | 用户 |
| --- | --- | --- | --- |
| 无敌帧 | `ce_invuln_request(id, frames)`（每帧声明**剩余**帧数）| 取 **max**；SDK 在每帧 `ce_item_frame` 断点里 `ce_invuln_apply()` 落 `player+0x47774`（只写更长值）| 107 结界结束 0.67s、109 爆风 C1–C4、117 霊撃 |
| 得分 | `ce_score_declare(id, value)` + `ce_score_mul_declare(id, permille)`（乘数附加项：110×111 同装时的「信号强度」）；落笔者读两个聚合值 | **Σ 各卡贡献**；落笔者 = **id 最小**（`ce_score_applier()`），由它写 SCORE | 110（信仰）+ 111（信号）|
| 「本帧已消过弹」 | `ce_cancel_latch_claim(id)` | 先到先得（OR）；SDK 每帧 reset | 107 结界灵击 + 117 霊撃 同帧 |
| 子机姿态 | `ce_optpose_declare(id, count, anm_script, fx, fy, ux, uy)` | **选举覆盖**：`ce_optpose_winner()` = id 最小，赢家独占落笔 | 111（低速子机）、108 |
| 掉落乘数 | `ce_drop_mult_set(id, permille)`（1000 = ×1，粘性）| **先加后乘**：各卡只做加法，`ce_drop_apply(counts)` 在 `BP_ce_enemy_drop` 的卡循环**之后**一次性乘 | 69（加倍）|
| HUD 槽位 | **卡侧用 `ce_hud_slot_claim_card(id, blk, out)`**（认领前推进「关卡纪元」⇒ 每关开场重排；裸 `ce_hud_slot_claim` 不重排）（`ce_hud_slot_get` 取最新值）；整摞淡出读 `ce_hud_near_corner()`（SDK 每帧推进一次，卡别自己调 `*_update`）| 底部货架按 **card_id 升序**确定性排布（[`HUD-LAYOUT.md`](HUD-LAYOUT.md) §2）；认领幂等、局中删卡不重排 | 110/111/113/116/117 |
| 敌人死亡 | `on_enemy_death`（只读广播）| 广播只读：谁都不许在回调里析构敌人 / 缓存指针 | 109/110/112/113 |
| 自建真敌人 | `ce_spawn_enemy(ecl_name, spawn)`（`CE_SPAWN_*` 字段表）| —（直接调引擎 `0x42d7d0`，返回敌人指针）| 108 使魔、112 彩虹令牌 |

**★ HUD 的层硬规则（2026-09-12 一手复核，`HUD-LAYOUT.md` §4.1）**：暂停模糊罩 = `text.anm` script59 的 UI VM（**层 38**）；
`0x4882b8` 把 layer 24–32 折到组 37–45、**其它任何层兜底折到 39** ⇒
**只有 layer 24 / 25 会被罩住**（26+ 与 20–23 都在罩子之后画 ⇒ 暂停时依然清晰）。
⇒ 新卡的 HUD 图元/文字一律用 **layer 25** 的贴图 VM（`ce_hud_vm_text_*` / `ce_hud_digit_*` / `ce_hud_number_*` / 自己起的脚本）；
**ASCII 文字（`ce_hud_text*`）结构上不可能被罩住**，所以 HUD 数字别用它。

**★ 编译期硬门之一：卡私有状态 ≤ 496 字节。** `ce_state(c, T)` 走 `ce_state_user()`，它只接受
`size <= CE_STATE_BYTES(512) − CE_STATE_RESERVED(16) = 496`，**超了会静默返回 NULL**（运行时才发现）。
结构体里一律加 `_Static_assert(sizeof(T) <= 496, "ce_state 上限 496 字节")` 把它顶到编译期。
（2026-09-12 踩过：113 的击破队列按 64 格存 float2 = 512 B ⇒ 结构体 600 B。）

**★ 编译期硬门之二（`make dllx87`）：我们的 object 里不许有 x87 指令。** i686 上 x87 有三个来源：
① **`float` 返回值走 `st(0)`** ⇒ 任何返回 `float` 的非 `static inline` 函数都会顶红这道门；
② **`float/double` ↔ `int` 的转换**（`fldl`/`fistpll`/`fldcw` 一组）；③ 变参 `printf` 里的浮点。
⇒ core 尽量**整数化**（比例用千分比/百万分比存 `int32`），几何/换算函数写成 `static inline float …`
（实现放头里）或 out 参数（`void f(..., float *out)`）。自检：
`i686-w64-mingw32-objdump -d cards/<x>.o | grep -cE 'f(ld|st|add|sub|mul|div|ild|ist|xch|com)'` 必须是 `0`。

**★ 铁律（`METHOD.md`）**：这些层都是**跨帧**的 ⇒ 卡被删时要收干净。`CE_CARD` 的 `on_run_reset`（虚表 `+0x4c`）
与 `opdelete`（`+0x50`）现在都会自动调 `ce_layer_drop_card(ID)`：清伤害/无敌/得分/子机姿态/掉落乘数并
`ce_hud_slot_release()`。**卡自己的**队列、池、VM 仍要自己清（删卡只跑得到这两个回调 + `on_delete`）。

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

原生 12 张主动卡共用一套模板（`04-active-cards.md` §3–§5，样本 Tenshi）；SDK 把它做成三个回调字段（AUDIT O19–O21）：

```c
CE_CARD(64, .active_recharge = 3600, .on_activate = f, .on_active_tick = g);
```

| 字段 | 语义 |
| --- | --- |
| `active_recharge` | 充能帧数（装填时 × `mgr+0xc58` 倍率）。非 0 = 主动卡：绑定时 SDK 把 flags 改成主动 case 的写法（`& ~0x46 \| 8`）、写 `+0x48`、按 Tenshi 初始化两个 zTimer——引擎据此把它放进主动卡组 / HUD / C 键分派 |
| `on_activate(c)` | C 键发动（门：state 空闲且充能到底；SDK 已装填充能、置「释放中」）。返回 0 = 瞬发，直接收尾；1 = 进持续态；`CE_ACTIVATE_REFUSED` = 条件不满足（SDK 退回充能、不算发动；卡自己放 0x10 无效音）|
| `on_active_tick(c, elapsed)` | 持续态每帧；返回 0 = 结束进收尾 |

SDK 在 `+0x2c` 桩里先跑状态机（空闲：清释放位、门控下递减充能；收尾：经过帧 > 8 回空闲；每帧门控下递增经过帧；
门控 = 原生的：`zGui.msg == 0`（不在对话）且 `zEnemyManager.enemy_count_real != 0`——**场上没敌人时 C 键无效、充能也不走**），
再调卡自己的 `on_tick_2`；`+0x34`（关卡开场）清状态与经过帧、不动充能，`+0x4c`（局末）连充能一起清——与原生一致。
状态放私有状态（原生放 `+0x54`，超出 0x54 字节的基类对象）。充能存取 / HUD / replay 用的槽 `+0x38..+0x40` 透传基类。
主动卡的 JSON：`category: 0`。

引擎辅助：`ce_play_sound(id, x)`（`0x476c70` stdcall + xmm2，三行内联汇编，AUDIT O22）；
`ce_anm_spawn(anm, script, layer)`（`0x405bf0` thiscall ret 0x10，AUDIT O24）：从 `CE_ABILITY_ANM()`（`ability.anm`）起一个脚本挂 world 列表，
实体坐标 (0,0,0) = 场地正中，返回 anm id。特效脚本与卡图副本用 [`assets/ability/`](assets/ability/README.md) 追加进 `ability.anm`，
脚本号 / sprite 号由 `build_ability.py` 生成到 `anm_ids.h`（`CE_ANM_ABILITY_SCRIPT_*`）。样例：反转牌 `on_activate` 起 `script68` 亮牌一圈。
引擎侧一手：[`engine/anm/th18/01-vm-instantiate.md`](../../../engine/anm/th18/01-vm-instantiate.md)。
`ce_add_life()` / `ce_add_bomb()`（`0x4575f0` 裸 ret / `0x457690` ret 4 带 dummy，AUDIT O26）：引擎自己的加法，钳上限、放音效、起特效；上限要一起涨照原生先 `+1` 钳 7。
`ce_gui_update_lives()`（`0x441f10` thiscall ret 0xc，AUDIT O28c）：改了 `CURRENT_LIVES` 之后刷 HUD 残机行。
`ce_owned(id)`：读本 mod 搬迁后的 `owned[]`（`mgr+0xd70`）。注意 `owned[自己]` 在 ctor 之后才置 1。
★ **ctor 每关开始会再被调一次**（引擎对卡组里每张卡调 +0x00，实跑证实）：「获得即触发」的效果在 ctor 里先 `if (!ce_fresh_acquire(c)) return 0;`（= `owned[自己] == 0`）。即时卡（返回 1 当场销毁）不受影响。集卡判定用 `ce_royal_flush_ready(owned, self, set, n)`（sdk_core，有单测）。

## 10. 边界与不做的

- 即时卡：`ctor` / `dtor` 里施加效果后 `return 1`（原生同款）；这种卡 `deck_visible: 0`（mode 1 不调 ctor，初始携带会是死卡）。ctor 里可以再调 `allocate_new_card`（AUDIT O17）。
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
另外原生 `CardMomoyo`（54）是「从 shooter 基值重算并覆写」派，与我们谁靠后谁说了算——
所以本层选择**就地乘**而不是重算，不然带着她时她的擦弹加成会被抹掉（AUDIT AA3）。

## 14. 在 HUD 上写字：`ce_hud_text`

```c
ce_hud_text(478.0f, 182.0f, 0xff800000u, "x%d.%02d", n, frac);
```

原生 `CardMomoyo__on_draw` `0x4110a0` 就是「一张卡在 HUD 上画数字」的先例，这个原语照它复刻（AUDIT §AC）。三条规矩：

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
    └── 带纯逻辑的还有一个 <名>_core.c/.h（主机单测），如 collect2x_core、blue_eyes_core
    ├── s10.c  sj.c  sq.c  sk.c  sa.c  peashooter.c  ouroboros.c …
    └── 带纯逻辑的还有一个 <名>_core.c/.h（主机单测），如 blue_eyes_core、broken_core_core
```

审计：[`AUDIT.md`](AUDIT.md) §O。

## 12. 装备卡（子机 + 自己的弹幕）

原生的「子机会开火」那类卡（各角色 `*_OP`）是两个槽的事，本 mod 现在能照着写。
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

- **子机指针别存 `card+0x54`**（原生存那儿）：新卡对象只有 `0x54` 字节，那已经在对象外。用 `ce_state()`。
  槽是 12 个的公共池，每次用前认领一次（`option+0x00 != 0 && option+0xd0 == card+0x08`）——
  这套认领 / 申请 / 收起 2026-09-08 收进了 SDK 的 `ce_option_*`（AUDIT §AD），卡里不用再抄一遍。
- **弹幕数据**要往四个 `pl0X.sht` 里加一组 shooterset：`assets/sht/append_shooterset.py` 的 `APPEND` 表加一项，
  `make sht` 生成 `build/sht/*.sht` 与 `sht_ids.h`（`CE_SHT_SET_<NAME>`）。原生用了 23 项、总共 40 项，
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
  要么把伤害拆成多发小弹（原生子机卡都是 6–32），要么在 `on_tick_2` 里 `ce_raise_damage_cap()`
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

## 16. 贴图 VM 版数字：`ce_hud_vm_text_*`（★ 想被暂停模糊罩盖住就用它）

```c
static ce_hud_vm_text_t s_digit;          /* 状态放 DLL 静态区（一张卡同时只在场一个）*/

char buf[8];
snprintf(buf, sizeof buf, "%d", level);
ce_hud_vm_text_set(&s_digit, 182.0f, 455.0f, 0xffffffffu,
                   CE_ASCII_ALIGN_CENTER, 0.0f /* 缩放用环境值 */, buf, "season");
/* 卡被删 / HUD 收起时： */
ce_hud_vm_text_clear(&s_digit);
```

**为什么需要它**：`ce_hud_text`（§14）走 ASCII 字符串，而 **ASCII 文字永远进不了暂停模糊罩下面** ——
模糊罩是 `text.anm` script59 的 UI VM（层 38 → 绘制优先级 `0x3e`），四个 ASCII 绘制趟里
只有组 1 比它早（`0x37`），但那一趟跑在 **surface 1 + 相机 0** 上（`0x2c` 切到 surface 1、`0x3a` 才回后备缓冲），
实跑**数字全看不见**（用户 2026-09-10 实测）。贴图 VM 放 **UI 层 25** 就解决了：
`AnmManager__render_layer` `0x4882b8` 把层 25 折到 **38** ⇒ 与模糊罩**同一个绘制桩**（`0x3e`），
而罩子是暂停时**新起**的 VM（尾插 = 画在后面）⇒ 我们的字被罩住；平时没有罩子 = 全分辨率。

**排版**（一手：`AsciiManager__draw_string` `0x41a2b0` 的 font 2/3 分支）：

| 项 | 值 |
| --- | --- |
| 坐标空间 | 与 `ce_hud_text` 相同（640×480），内部 × `DAT_0056aca0` 换表面 px |
| 字距 | `scale_x × 7`；**`.` 特例 `× 4`** |
| 水平对齐 `align_h` | `0` 居中（每字退半个字距）、`1` 左、`2` 右（退满整串宽）|
| 垂直对齐 | 取 **ASCII 环境的 `align_v`**（`mgr+0x19254`）：0 居中（退半行高）、1 顶、2 底；行高 = `scale_y × 10` |
| 字形 | TH18 **font 2** 的原生切图（`make_font2_art.py` → `ability.anm` 的 `FONT2_*`，脚本 136，16×32 entry 里 16×20 字形居中）|

认得的字符：`0-9 . % + -`（其余按字距跳过、不画；空格就是「跳过一个字距」）。
⚠️ 引擎有个怪癖：**空格紧跟 `.` 之后时字距会沿用 `.` 的 4**（`local_c` 只在画字时更新）——
本 SDK 不复制这个怪癖（我们的串里不会出现 `.`+空格）。


## 17. HUD 布局：左下角槽位的堆叠（待做，2026-09-10 登记）

**现状**：每张带左下角 HUD 的卡（季节 116、鬼形兽 117）各自 `ce_anm_spawn_ui` 自己的 VM，
坐标写死在卡里（表面 px）。**用户 2026-09-10 口径**：先按原版复刻、**允许不同卡重叠**；
最终形态是「多张左下角 UI 的卡**竖直堆叠**，或部分改放右下角」。

**要做的**：一个共享的 HUD 槽位分配器（`ce_hud_slot_*`）：

- 卡按「我要一块多高、优先左下还是右下」申请，分配器按注册顺序把左下角的块从上往下堆
  （或右下角同理）并返回锚点；
- 卡自己的 VM 仍自己管，只是坐标从分配器拿；
- 与 §15 / §16 的 HUD 数字 / 文字原语共用同一套锚点；
- 能表达「互斥槽」——一次装备的槽位类卡（信号 / 灵界 / 信仰）本来就约定共享同一块显示区
  （[`IDEAS.md`](IDEAS.md) §4）。

**层位**：想被暂停模糊罩盖住，VM 必须放 **UI 层 25**（`AnmManager__render_layer` 折到 38 ⇒
与模糊罩同一个绘制桩）—— 见 §16 与 [`engine/anm/th18/03`](../../../engine/anm/th18/03-ascii-text.md) §2f。

## §改发弹 / 改弹道：踩过的坑与铁律（2026-09-13 总结；108 必读）

> 来源：117「鬼形兽」复刻 TH17 咆哮时，为了「换整组 shot set」「咆哮期间停掉子机普通弹」
> 「收掉换组前打出去的旧束」一共踩了三次坑。108（永夜抄使魔）与 77 / 116 也走同一条子机路径，
> 同一类坑会原样重现 —— 动手前先读这一节。

### 1. 断点站点必须 ≥ 5 字节（否则新卡全废）

- thcrap 的 sourcecave 下限是 **5 字节**；窗口更小 ⇒ 断点**静默不装**。
- 后果不是"少个功能"，而是**灾难链**：`ce_sdk_setup` 回读失败 → SDK 门 FAIL →
  `restore_alloc_bound` 把 `allocate_new_card` 的界打回原生 56 → **所有新卡 id 都造不出卡对象**；
  而存档 / 编成里还写着新 id ⇒ 玩家看到「卡图变垃圾（`滚木`）+ 编成槽当场消失、后槽前移、可带数 −1」。
- 现在有构建期硬门：`native/sites.py` 对任何 `cavesize < 5` 直接报错。
- 站点还要**不含相对寻址/跳转目标**（thcrap 的 `cave_fix` 只重定位开头的 `e8/e9`；
  `0f 84` 之类复制进 cave 会按原位移跳飞）。

### 2. 换组（shot set 重定向）管不到**已经发出去的弹**

- 主炮路径 `Player__tick_shooting_state th18:0x45ea00` 的 set 查询**只有一处**：`0x45ebfb`
  （`mov edi,[edi+edx*4+0xe0]`）；聚焦位只是在 `0x45ebf2..f8` 给 `edx` 加 `1+sht[0x20]`，
  两条通道都落到同一个站点 ⇒ 想做「按聚焦分派」不必找第二处。
- 但 `mode=2` 激光的**存活完全不跟 set**：`tick2`（`0x45fc80`）的自杀条件只有
  `0x4601e0 cmp [player+0x476d4],0 / jl`（= **射击短计时 < 0**，即松开 Z），
  而且 `0x45fcc8 mov eax,[edi+0xe4]` 每帧把束**钉在发射它的子机上**。
  ⇒ 换组前打出去的束会一直挂着，看起来就是「子机还在打普通激光」。

### 3. 手工收束一条 `mode=2` 束：必须照引擎的**三步**走

引擎自己的自杀段（`tick2` `0x46026f..0x4602c1`）依次做：

| 步 | 地址 | 动作 |
|---|---|---|
| 1 | `0x460296` | `slot+0x8c = 2`（state） |
| 2 | `0x4602a0` | `AnmManager__interrupt_tree(vm, 1)`（束 VM 收尾） |
| 3 | `0x4602c1` | **`player+0x476fc + idx*4 = 0`（去重/占用锁清 0）** |

- **漏第 3 步的后果（我们踩过）**：`Player__shoot_one_bullet 0x45e954..0x45e976` 见到该槽非 0
  就**整条 `mode=2` row 早退 `ret 0x10`** ⇒ 那个**子机槽永久不再发射**（用户看到「2p 只有右侧发 /
  4p 最左不发」，`retry` 重开一局才恢复，因为只有重开才重置 `player+0x476fc`）。
- 解码第 3 步的 idx：`0x4602a8` 同款 —— `slot == 0` 跳过；`< 0x10` 取**低 4 位**，否则取**高 4 位**。
- 弹槽布局（一手 `Player__tick_bullets 0x45edb0`）：基址 `player+0x1570`、**512** 槽、
  stride **0xf8**；state 在 `slot+0x8c`、VM 句柄在 `slot+0x08`。（旧注释写的「512 × 0x9c」是笔误。）
- 现成实现：`native/sdk.c` 的 `ce_kill_retail_option_lasers()` + `ce_mode2_lock_clear()`，
  只收 `packed set ≤ 0x15` 且 `row+0x21 == 2` 的弹，且**只在 117 咆哮期每帧调用**。

### 4. 通用铁律（108 / 77 / 116 同用）

1. **只写引擎自己也写的字段、按同样顺序**；能少写一格就少写一格。
2. 处理范围要**收得足够窄**：117 只管 `0x1e..0x27`；108 的组是 `0x16`、77/116 是 `0x17..0x1d`
   —— 别家的弹一个字节都别碰。
3. 想去掉一条弹/一束光：优先**让引擎自己走自杀段**（哪怕只是把它的计时器推到条件成立），
   其次才是手写那三步；手写时必须把**状态类副作用**（锁、计时器、VM 句柄）在同一条路径上复原。
4. 任何「拦住 / 跳过」型断点（跳到函数自己的收尾，如 `0x40a9c3 → 0x40aa9e`）必须核对**栈平衡**
   —— `ret 0x10` 的收尾是函数自己的 epilogue，用错地址会踩坏调用者。
5. 门失败要让玩家**看得出原因**（日志 + 每关卡对象 `ce_integrity_check()` 自检），
   而不是把新卡变成垃圾图。
