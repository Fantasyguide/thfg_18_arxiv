# TH18 卡片商店与金钱系统

> **版本**：TH18 v1.00a（`th18.exe`）。本文裸地址默认属该版本；引用其他版本须写成 `th16:0x…`。
> **方法**：一手反编译 `AbilityShop__*`、`CardShop__*`、`GlobalsInner__spend_power`、
> `ItemManager` 的道具分派与 `Player__on_tick__body` 的死亡结算；`CardData__is_available_at_stage`
> 的返回值走**反汇编**（反编译器把它误标成 `bool`）。
> **可信度**：机制 ✅；标 🟡 处逐条注明。
> **前置**：[`01-object-model.md`](01-object-model.md)、[`02-lifecycle.md`](02-lifecycle.md)。

## 0. 一句话结论

`MONEY` `0x4ccd34` 是一个**关卡内赚、关卡末花**的循环货币，而且**同时是计分乘数**：
金钱越多，金钱道具和小分道具给的分越高。商店把它换成卡；钱不够可以用**火力**补差价，
但那样会**把剩余金钱全部清零**。死亡按 `min(MONEY/3, 100)` 罚款。

## 1. 金钱的全部收支（一手，穷举）

| 方向 | 事件 | 数量 | 证据 |
| --- | --- | --- | --- |
| 收 | 吃到金钱道具（item type **2**）| `MONEY += 1` | `ItemManager__collect_money_item` `0x446B00` |
| 收 | `CardNazrin`（id 5 PENDULUM）获得时 | `+50` | `0x40A020` |
| 收 | `CardYachie`（id 34 MONEY）在场 | 敌人掉的金钱道具 **×(1 + 1/3)** | `0x40D630`，见 [`03-hooks.md`](03-hooks.md) §3 |
| 支 | 商店购卡 | `-price` | `AbilityShop__on_tick` `0x4185C7` |
| 支 | 商店用火力补差价 | **`MONEY = 0`** | 同上 |
| 支 | 空白卡（id 0）换购 | **`MONEY = 0`** | `0x418496` |
| 支 | 六文钱（id 35 ROKUMON）救命 | `-200` | `Card__death_save_money_revive` `0x40D840` |
| 支 | 中弹死亡 | `-min(MONEY/3, 100)` | `Player__on_tick__body` `0x45C21B` |
| 回 | `CardTewi`（id 24）在场时的死亡 | 罚款被回填 | `0x40A720` 快照 → `0x40A730` 写回 |

另有 `MONEY_TOTAL_COLLECTED` `0x4ccd30`（本仓命名）：**只记收入不记支出**，
`ItemManager__collect_money_item` 与 `CardNazrin__constructor` 同步 `+1` / `+50`，
死亡惩罚和购买都不动它。`CardTewi` 回填时**两个都写**。

## 2. ★ 金钱同时是计分乘数（新结论）

`ItemManager__collect_money_item` `0x446B00` 在给 `MONEY += 1` 之前先算一个**道具身价**：

```c
// 难度 DAT_004ccd00: 0=Easy 用系数 0x4b93ac，1..3 用 1000.0，4(Extra) 另走一支
value = (MONEY * k + k) * DAT_004b9304;
MONEY_ITEM_SCORE_VALUE = clamp(value, DAT_004ccd28, DAT_004ccd2c);   // 0x4ccd24
// 在自动回收线以上吃满值，以下按到 poc_height 的距离线性打折
SCORE(0x4cccfc) += 到手值 / 10;                                       // 封顶 999999999
```

而**小分道具（item type 9–0x0e）的分值直接是 `(MONEY / 10) * 10 / 10`**
（`ItemManager__on_tick__body` `0x445A80` 的 case 9..0xe）。

- **结论**：**`MONEY` 既是购卡货币，也是本作的计分乘数** ✅（TH18 v1.00a）。
  「攒钱不花」在计分玩法里有独立价值，这解释了为什么买卡的成本设计得这么重。
- **证据**：`0x446B00`、`0x445A80` case 9–0xe、`0x4ccd24`/`0x4ccd28`/`0x4ccd2c`。
- 🟡 未做：`0x4ccd28`/`0x4ccd2c`（身价上下限）与 `0x4b93ac`/`0x4b9304` 的具体数值未 dump。

## 3. 商店对象

`AbilityShop__initialize` `0x4171B0` 注册两个 UpdateFunc：`on_tick` 优先级 **0xc**、
`on_draw` 优先级 **0x51**；建两个 `MenuSelect`（`this+0xc` 标签游标、`this+0xe4` 卡格游标）。

| 字段 | 语义 |
| --- | --- |
| `this+0xa30` | offer 数组（每项 = `zTableCardData*`）|
| `this+0xa2c`、`this+0xec` | offer 数量 |
| `this+0xe34` | **卡组里有空白卡（id 0）** → 进 state 3 而不是 state 2 |
| `this+0xe38` | 状态机（见 §5）|

## 3.5 ★ 开店 / 冻结 / 关店的完整链（谁开的门，谁在等）

> 2026-09-04 一手补全，起因是想让商店每关开两次（`mods/th18.v1.00a/card-expand`，AUDIT §P）。

**开店不是 ECL 干的，是 MSG。** 关末对话脚本（`stNNd.msg`）的 opcode **36**（`'$'`）在
`GuiMsgVm__run` `0x440D83` 只做一件事：`GAME_THREAD+0xb0 |= 0x20000`，然后 MSG 指针前进、**不等待**
（`0x440D8D..0x440DA9`）。全二进制里给这个位置位的只有它（`search_text 0x20000` 穷举）。

**建店在 `GameThread__on_tick` `0x443860`**（UpdateFunc 优先级 **0x10**，`0x4429FF` 处注册）：

```c
// 0x443AFF..0x443C21
if (this->flags_b0 & 0x20000) {
    shop = operator_new(0xe3c); memset …;             // 0x443B10..0x443BDB
    ABILITY_SHOP_PTR(0x4cf2a4) = shop;  *shop |= 2;   // 0x443BED
    if (PLAYER_PTR) FUN_00416cd0(player);             // 收起子机（中断 3）、player+0x4779c |= 2
    if (AbilityShop__initialize(shop, {448.0, 32.0, 0})) FUN_004176e0(shop);   // 失败即析构
    this->flags_b0 &= ~0x20000;                       // 0x443C17
}
```

**冻结靠一个指针。** 没有任何「等商店」的事件；下面这些 on_tick 都是 `ABILITY_SHOP_PTR != 0` 就跳过本帧
（`get_xrefs_to 0x4cf2a4` 穷举）：`EnemyManager__on_tick` `0x42DF8B`、`BulletManager__on_tick` `0x424E8B`、
`LaserManager__on_tick` `0x4488C4`、`Player__on_tick` `0x45CAA0` / `__body` `0x45C0C1`、`ItemManager__on_tick` `0x446EE3`、
`AbilityManager__on_tick__stub` `0x408A90`、`Gui__on_tick` `0x43C889`（**MSG 也在这里停**）、replay 录制 / 回放
`0x4629A2` / `0x462AA5`（商店期间不录不放）。**不看**它的：`Stage`（背景照常滚）、`Spellcard`（符卡计时照常走）——
所以关卡中间开店在技术上可行，但只该在对话里开（对话时这两样本来也不在活动）。

**关店由商店自己完成**（on_tick 优先级 **0xc**）：state 5（成交后 30 帧）/ 4（空白卡确认后）/ 9 → `FUN_004132b0`（清文案）
→ `AbilityShop__sub_417880`（replay 存/复原卡组）→ `FUN_004176e0`（注销两个 UpdateFunc、杀全部 VM、
**`0x417857` 把 `0x4cf2a4` 清零**、`delete`）。还有两条不经成交的退出：`0x417CC7` 起，`timer == 0x1e` 且
（replay 回放 `GAME_THREAD+0xd0 != 0` ‖ `DAT_004cccc8 & 0x30` ‖ **练习模式 `0x4c5f8c >= 0`**）→ 30 帧自动退；
暂停菜单开着（`0x4cf40c+0x1ec != 0`）时只有 state 4/5/9 能继续退。

**帧内顺序决定「再开一家」没有空档**（th18 一手 `register__on_tick` 的优先级参数）：商店 **0xc**（`0x4171DE`）→
GameThread **0x10**（`0x442A97`）→ EnemyManager **0x1b**（`0x42DCD8`）→ Gui **0x21**（`0x43BAC5`）。商店在自己的 tick
里析构、指针归零；同一帧稍后 GameThread 若再看到 `0x20000` 就立刻重建；敌人与 MSG 这一帧看到的指针仍非零。
卡牌扩展 mod 正是在 `0x443B05`（`test eax,0x20000`）上把位加回去实现「每关进店两次」的。
（th16 的表在 [`../../_shared/frame-loop.md`](../../_shared/frame-loop.md)：Enemy 0x1a / Gui 0x20，th18 各 +1，别混用。）

- **结论**：开店 = MSG opcode 36 置位 + GameThread 建对象；冻结 = 各子系统看 `0x4cf2a4`；关店 = 商店自析构清指针 ✅（TH18 v1.00a）。
- **证据**：`0x440D83`、`0x443AFF..0x443C21`、`0x417857`、`0x417CC7..0x417D2D`、`get_xrefs_to 0x4cf2a4`（17 处）、
  上面四个 `register__on_tick` 调用点的立即数。

## 4. offer 是怎么凑出来的（`0x4171B0`，一手逐段）

**第一段：三档随机抽（+ 招财猫加抽）**

```
pick(10, 0xe)                 // 高价档 300–500
if 卡组有 id 0x26(38 MANEKI):  pick(1, 0xe)
pick(7, 9)                    // 中价档 200–280
if 卡组有 0x26:                pick(1, 0xe)
pick(1, 6)                    // 低价档  50–180
if 卡组有 0x26:                pick(1, 0xe)
```

→ **常态 3 张随机；带招财猫 6 张**（额外 3 张不限价档）。与 THBWiki「携带招财猫额外抽三张」吻合。

**`CardShop__pick_weighted_random_offer(out, tier_lo, tier_hi, 已抽数组, 已抽数)` `0x416F50`**
遍历 card_id **0–55**（`0xc84` 起步、`< 0xd64` 止，**不含 id 56/57 两个菜单哨兵**），逐条过滤：

1. `mgr+0xc84 + id*4 == 0`（本局未拥有）**或** `entry+0x1c != 0`（可重复购买）；
2. `CardData__is_available_at_stage(entry) == **1**`（注意：**==1**，所以本关专属卡不进随机池）；
3. `tier_lo <= entry+0x10 <= tier_hi`；
4. `entry+0x14 != 0 && entry+0x14 != 6`；
5. 不与已抽的重复。

过关的卡按 `entry+0x14`（权重）**压入那么多份**；若 `SCOREFILE + 0x5f588 + id == 0`（从没拿到过）
**再额外压 5 份**（是 **+5 的加法**，不是 ×5）。最后 `Rng__rand_dword(&DAT_004cf280) % 总份数` 取一张。

**第二段：两个「保证」循环**（同样的拥有/可重复门控）

- **loop1**：`is_available_at_stage() == 1` **且** `entry+0x14 == 0` → 必加。
  `+0x14 == 0` 的正是 **EXTEND / BOMB / PENDULUM / DANGO**（card id **1 / 2 / 5 / 6**）这批通用资源卡。
  ⚠ **EXTEND2 / BOMB2（id 3 / 4）不在这批里**——它们 `+0x14 == 6`（**订正**，见 §4.1）。
- **loop2**：`is_available_at_stage() == **2**` → 必加。**这就是「本关专属卡必出」的唯一路径。**

### 4.1 `entry+0x14` 不是纯权重（2026-09-10 一手订正）

随机池在「压份」之前有**两条拒绝**：`0x4170a0` `cmp [eax],0 / je 拒绝`、`0x4170cf` `cmp [eax],6 / je 拒绝`
（`eax = entry + 0x14`，`0x41709a`）。通过后按 `entry+0x14` 往候选数组里压 **那么多份**
（`0x41710d` `rep stosd` + `0x417115` `add edi,ebx`）⇒ **0 与 6 都是「不进随机池」的哨兵**，
`≥1 且 ≠6` 才是真权重（实测 id 7 = 4，id 38/39/40 = 2）。

| card id | 卡 | `+0x14` | 商店怎么拿到 |
| --- | --- | --- | --- |
| 1 EXTEND / 2 BOMB | 整残 / 整炸 | **0** | **loop1 必上架 —— 唯一来路**（`+0x1c == 1` ⇒ **同一局可反复买**）|
| 3 EXTEND2 / 4 BOMB2 | 残碎 / 符卡碎 | **6** | **永不上架** ⇒ 只能靠道具掉落（`item type 0x10` / `0x11` → `allocate_new_card(mode 0)`）|
| 5 PENDULUM / 6 DANGO | 金钱 +50 / 火力 +0.5 | 0 | loop1 必上架 **且** 可掉落（`0x12` / `0x13`）|

> 后果：**「整」只能买、「碎」只能掉**。对齐证据见
> [`06-resource-economy.md`](06-resource-economy.md) §4 的 `ITEM_TYPE_TABLE 0x4b4020`。

**第三段**：把 `local_e8[]` 去重复制进 `this+0xa30`，写回数量；扫链表设 `this+0xe34`。

## 5. ★ `CardData__is_available_at_stage` `0x416E10` 的返回值不是布尔

Ghidra 把它标成 `bool`、反编译显示 `return true` —— **是假象**。反汇编实证：

| `dmode`(`entry+0x18`) | 行为 | 返回 |
| --- | --- | --- |
| 0 | 恒可用 | 1（`0x416EF9` `MOV EAX,1`）|
| 1–5 | `CURRENT_STAGE == dmode` | **2**（`0x416E2C`/`E51`/`E60`/`E6F`/`E7E` 均 `MOV EAX,2`）|
| 1–5 | 不匹配 | 落到解锁位 |
| 6 | 关卡 1,2 | 1 |
| 7 | 关卡 1,2,3 | 1 |
| 8 | 关卡 2,3,4 | 1 |
| 9 | 关卡 3,4,5 | 1 |
| 10 | 关卡 4,5 | 1 |
| 6–10 | 不在区间 | `Rng % 5 == 0 ? 1 : 0`（**1/5 概率仍出现**，`0x416E93`）|
| 0xb | 关卡 1–5 → 解锁位；否则 0 | 0/1 |
| 0xc | 解锁位 | 0/1 |
| 其他 | — | 0 |

「解锁位」= `SCOREFILE_PTR + 0x5f588 + card_id` 的字节（`0x416E32` `SETNZ AL`）。

- **结论**：**`dmode` 是关卡（不是难度）门控；返回 2 = 本关专属，被 loop2 强制塞进商店** ✅。
  原 ExpHP/我们早期的 `_for_difficulty` 命名已订正为 `_at_stage`。
- **`dmode` 1–5 的六张卡**（id→类由 `allocate_new_card` 的跳转表钉死）：

  | id | 内部名 | 类 | 关 |
  | --- | --- | --- | --- |
  | 38 | MANEKI | `CardMike` | 1 |
  | 39 | YAMAWARO | `CardTakane` | 2 |
  | 40 | KISERU | `CardSannyo` | 3 |
  | 51 | MAGATAMA | `CardMisumaru` | 4 |
  | 52 | CYLINDER | `CardTsukasa` | 5 |
  | 53 | RICEBALL | `CardMegumu` | 5 |

- **`dmode == 12` 的卡靠解锁位**，而解锁位由通关授予（[`02-lifecycle.md`](02-lifecycle.md) §9）：
  id 9/11/13/15（各角色第二种自机卡）按通关角色解锁，id 54 MUKADE 由通关 Extra 解锁。
  **这两件事互相印证**：dmode 12 = 「打通了才卖」。

## 6. 定价与购买（`AbilityShop__on_tick` `0x417CC0`，状态机 `this+0xe38`）

**价格表 `CARD_PRICE_BY_TIER` `0x4b35c4`**（15 档，一手 dump）：

```
档位 0  1  2  3  4  5   6   7   8   9   10  11  12  13  14
金额 0 50 80 100 100 140 180 200 240 280 300 350 400 450 500
```

`CardShop__price_for_tier` `0x416DD0`：查表后，**若卡组里有 id `0x27`（39 YAMAWARO 打折卡）
→ `price = price * 5 / 10`（半价）**。

| state | 做什么 |
| --- | --- |
| 0 / 1 | 入场动画（60 帧）→ 建卡格图标；**未买过的卡挂一个额外「NEW」VM**；**买不起的卡置灰**（`FUN_00488c60(vm, 3)` = 给图标 VM 及其子树挂中断 3；`0x417FBE push 3`，早先误记成 2）；然后进 state `2 + (this+0xe34 != 0)` |
| 2 | 浏览。上下键移动游标；确认键 `MENU_INPUT & 0x80001` → 询价（见下）|
| 3 | **空白卡分支**（§7）|
| 4 / 5 / 9 | 退出商店（回 `AbilityShop__sub_417880` 存/复原，再 `FUN_004176e0` 析构）。4 = 空白卡确认后等确认键；**5 = 成交后 30 帧**（`0x418614 cmp ecx,0x1e`）；9 = 10 帧。三个状态的设置点全在本函数内 |
| 6 / 7 | 确认对话框（6 = 正常付款，7 = 用火力补差价）|
| 8 | 「买不起」提示，回 state 2 |

**询价分支**（state 2）：

```c
price = price_for_tier(offer->tier);
if (MONEY < price) {
    if (CURRENT_POWER - 100 + MONEY < price) { 音效 0x10; → state 8; }   // 买不起
    else                                     { 音效 0x10; → state 7; }   // 可用火力补
} else                                       { 音效 0x07; → state 6; }
```

**成交**（state 6/7 选「是」，`0x4185C7`）：

```c
allocate_new_card(mgr, offer->card_id, 2);          // mode 2 → ctor 后再 dtor（即时卡当场消耗）
CardCollection__mark_obtained_and_notify(id, 0);    // 置解锁位
price = price_for_tier(offer->tier);
if (price <= MONEY) { MONEY -= price; }
else {
    GlobalsInner__spend_power(&GlobalsInner, price - MONEY);   // 返回「火力档是否变了」
    if (变了 && PLAYER_PTR) Player__repopulate_options_and_notify_cards(...);
    MONEY = 0;                                                  // ★ 剩余金钱一并清零
}
```

`GlobalsInner__spend_power` `0x457480`：
`CURRENT_POWER <= POWER_PER_LEVEL` 时**直接拒绝**（返回 false，不扣）；否则扣款并钳到
`POWER_PER_LEVEL`（**永远保留一档火力**），返回 `(旧值/档) != (新值/档)`。

- **结论**：**「火力换钱」不是等价兑换 —— 一旦动用火力，本来有的金钱也全没了** ✅。
  与 THBWiki「透支后清零」吻合，并给出了「至少保留 1.00 火力」这一硬下限。

## 7. ★ 空白卡（id 0 / `CardChimata`）的效果实现在商店里

`CardChimata` 类只有 `operator_delete`，是个 stub。真正的效果在 `AbilityShop`：

- `initialize` 尾部扫链表，若有 card_id 0 → `this+0xe34 = 1`；
- state 1 因此转入 **state 3** 而非 2；
- state 3 收到确认（`MENU_INPUT & 0x80003`，`0x41842A` 起）：

```c
AbilityManager__reset_cards(mgr, 0);                 // ★ 弃掉整副卡组（不从存档重建）
for (offer in offers)
    if (offer->+0x1c == 0) {                         // 非「可重复购买」的卡才发
        allocate_new_card(mgr, offer->card_id, 2);
        CardCollection__mark_obtained_and_notify(id, 0);
    }
MONEY = 0;                                            // ★ 全部财产
```

- **结论**：**空白卡 = 「弃掉当前所有卡 + 免费拿走本次商店全部非消耗型待售卡 + 金钱清零」** ✅。
  这条此前是社区单源（[`09-community-crosscheck.md`](09-community-crosscheck.md) §4 的开放项），**现已一手闭合**。
- 早前记的「效果在 `FUN_00430d30` 里靠 `card_exists(0)` 触发」是另一处引用；
  **商店这条才是玩家看到的那个机制**。

## 8. 卡也能不经商店直接掉落

`ItemManager__on_tick__body` `0x445A80` 的道具分派里，type `0x10`–`0x13`：

```c
allocate_new_card(ABILITY_MANAGER_PTR, ITEM_TYPE_TABLE_sprite_and_card[type].card_id, 0);
```

表 `0x4b4020`（本仓命名 `ITEM_TYPE_TABLE_sprite_and_card`，stride 8，索引 = 道具 type；
第一个 dword 是 sprite，第二个另有他用）—— 一手 dump：

| 道具 type | card_id | 内部名 |
| --- | --- | --- |
| 0x10 | 3 | EXTEND2（残机碎片）|
| 0x11 | 4 | BOMB2（符卡碎片）|
| 0x12 | 5 | PENDULUM（+50 金）|
| 0x13 | 6 | DANGO（+0.50 火力）|

**全是 ctor 型即时卡**——而 mode 0 恰好只调 ctor（[`02-lifecycle.md`](02-lifecycle.md) §3），自洽。
**boss 卡不会靠道具掉落。**

道具 type 速查（一手，来自同一个 switch）：

| type | 效果 |
| --- | --- |
| 1 | 火力道具（满火力时转分）|
| **2** | **金钱**：`MONEY += 1` + 计分 |
| 3 | 火力（`spend_power` 的反向：加一档）|
| 5 | 残机（钳到 `LIVES_STOCK`）|
| 6 | 符卡碎片：`BOMB_FRAGMENTS += 1`，**满 3 进位** |
| 7 | 符卡（`CURRENT_BOMBS += 1`，钳 `MAX_BOMBS`，音效 `0x2e`）|
| 8 | 火力全满 |
| 9–0x0e | 小分：`SCORE += (MONEY/10)*10/10` |
| 0x10–0x13 | 卡（见上）|

## 9. 死亡的金钱惩罚（`Player__on_tick__body` case 2，第 3 帧）

```c
GlobalsInner__spend_power(&GlobalsInner, POWER_PER_LEVEL);   // 掉一整档火力
撒 7 个道具;
lost = MONEY / 3;  if (lost > 100) lost = 100;
MONEY -= lost;                                                // ★
for (card in list) card->vtable[0x14]();                      // 补偿类卡在此回填
Player__repopulate_options_and_notify_cards(...);
```

- **结论**：**死一次固定损失 1/3 金钱，上限 100** ✅。`CardTewi`（id 24）通过在
  `vtable+0x0c` 快照、在 `vtable+0x14` 写回来完整抵消它（[`03-hooks.md`](03-hooks.md) §4）。

## 10. 持久化、replay 与解锁

- **卡组** 存在 `SCOREFILE_PTR + 0x5f608 + slot*0x10`（id 字节）+ `+0x5f678 + slot*4`（张数）。
- **解锁位** `SCOREFILE_PTR + 0x5f588 + card_id`：影响商店随机权重（未解锁 +5 份）、
  `dmode` 0xb/0xc 的可用性、以及能否作为初始装备。
- **`CardCollection__mark_obtained_and_notify` `0x418DE0`**：置位 → 若注册表**前 56 项**全置位
  则发成就 `0x1d`(29) → 排入「获得新卡」弹窗（`DAT_004cf2a8`，条目 = `param2 << 8 | card_id`）。
- **replay**：`AbilityShop__sub_417880` `0x417880` 在商店进/出时存/复原卡组、选中卡与每卡充能
  （详见 [`02-lifecycle.md`](02-lifecycle.md) §7）。

## 11. Follow-up

- ⏳ 商店 UI 的 anm 脚本编号与 state 0/1 入场细节（非机制，未展开）。背景 VM 的中断：6 = 成交收店、7 = 空白卡、`0x1b` = 取消回浏览、
  `7+i` / `0x11+i` = 确认框游标、`0x25` = 买不起；图标 VM：3 = 置灰、`0xb..0x15` = 游标移动。
- ⏳ `FUN_00416cd0` `0x416cd0`（开店前对 player 做的事：16 个子机 VM 中断 3、`player+0x4779c |= 2`、`+0x477e8 = 0`）未命名。
- ⏳ MSG opcode 36 之后关末脚本还写了什么（是否紧跟结束）；`stNNd.msg` 未 dump。
- 🟡 `0x4ccd28`/`0x4ccd2c`（金钱道具身价上下限）与难度系数常量未 dump（§2）。
- 🟡 `DAT_004ccd00`（难度）与 `DAT_004cccf4`/`DAT_004cccf8`（角色/子机）编码未逐值核对。
