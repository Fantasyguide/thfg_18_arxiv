# TH08 「人妖」两件事：**妖侧本体外观**与**人妖槽**（卡 108 的 #2/#4）

> **版本**：TH08 v1.00d（`th08.exe`，imagebase `0x400000`）。本文裸地址默认属该版本；引用 TH18 须写成 `th18:0x…`。
> **方法**：社区反编译 `local/vendor/th08_dec`（`src/Player.cpp` / `GameManager.cpp` / `AsciiManager.cpp`）
> 当**源码级**依据（用户 2026-09-16 口径「你可以认为反编译 = 源代码」），关键处再用本仓自己的
> `local/_scratch/th08_full.asm` 与 `.data`/`.rdata` 直读**逐指令复核**。
> **可信度**：§1 ✅ 一手（反编译 + 逐指令）；§2 的**机制** ✅、**限值** 🟡（见 §2.4：本 build 里没有写点）。
> **缘起**：`SINGLE-CARD-TEST.md` §th08 #2「缺少妖侧本体」、#4「缺少人妖槽」。

## 0. 一句话结论

| 现象 | 原作是什么（一手） | 卡 108 现状 |
| --- | --- | --- |
| **#2 妖侧本体** | 自机本体**就是同一台 VM**，只换脚本号：人侧 idle = `0` / 移动 = `1..4`，**妖侧 idle = `5` / 移动 = `6..9`**（同一个 `playerNN.anm`）| ❌ 不换（`YK_BODY_SPRITE_FOCUSED = -1`）；**资产已备**（见 §1.3）|
| **#4 人妖槽** | 一个 i16 标量 + 4 个阈值 + 7 个写入者；HUD = `ascii.anm` 的 4 支脚本 | ❌ 完全没有 |

## 1. 发现：妖侧本体 = **同一个 `playerNN.anm` 里 5..9 号脚本**

### 1.1 一手：脚本号的选取点

`Player::HandleDeath` / `Player::Initialize` / `Player::UpdateMovement` 三处都按
「**人 / 妖**」二选一（`th08_dec/src/Player.cpp:652`、`:657`、`:660`、`:1426`、`:1430`）：

```c
/* Player.cpp:652-661（复活重定位那段，逐条同形） */
if (g_PlayerCharacter < 4 && this->isYoukaiMode == 0) goto setForm0;
if (g_PlayerCharacter & 1)                            goto setForm5;   /* 单人模式的妖侧机体 */
setForm0: this->anm->SetAndExecuteScriptIdx((AnmVm *)this->unk_10, 0);  /* ★ idle 0 = 人侧 */
          goto setFormDone;
setForm5: this->anm->SetAndExecuteScriptIdx((AnmVm *)this->unk_10, 5);  /* ★ idle 5 = 妖侧 */
setFormDone:;
```

移动动画则由宏 `SET_MOVE_ANIM(a,b,c,d)`（`Player.cpp:798-806`）按**横向速度过零**切换，
它的注释就是答案（原文：`Pick the movement animation (normal 1-4, youkai 6-9)`）：

```c
#define SET_MOVE_ANIM(a, b, c, d)                                  \
    if (speedX < 0.0f && this->velocityX >= 0.0f) … script a;      \
    else if (speedX == 0.0f && this->velocityX < 0.0f) … script b; \
    else if (speedX > 0.0f && this->velocityX <= 0.0f) … script c; \
    else if (speedX == 0.0f && this->velocityX > 0.0f) … script d
/* 调用处两半：人侧 SET_MOVE_ANIM(1,2,3,4)、妖侧 SET_MOVE_ANIM(6,7,8,9)（Player.cpp:978/1013 一带）*/
```

⇒ **人侧 = 脚本 0（idle）+ 1..4（四向移动）；妖侧 = 脚本 5（idle）+ 6..9（四向移动）**，
**都在同一个 `playerNN.anm` 里**（`player->anm = g_AnmManager->GetAnm(5/…)`，`Player.cpp:1415`）。

### 1.2 交叉印证（第二条独立链）

- 进/出低速的**闪光特效**用的是**另一支 VM**（`player->unk_10` 之外的 `effectVm`）：
  `SetAndExecuteScriptIdx((AnmVm *)this->unk_10, 5)` 与 `SpawnEffect(0x1d / 0x1c, …)` 同段（`Player.cpp:878/942`）；
- 该 `unk_10` 就是**本体 VM**：`Player::on_draw` 末尾 `DrawNoRotation((AnmVm *)&player->unk_10)`（`Player.cpp:1342`）；
- ⇒ 「脚本 5 = 妖侧 idle」与「换形态时那一下闪光」是**两件事**，别把 `SpawnEffect` 当成换本体。

### 1.3 卡 108 的资产**已经切好了**（在仓里）

| 机体 | 妖侧 | 已切进 `ability.anm` 的脚本 | 来源 |
| --- | --- | --- | --- |
| 0 霊夢 | 八雲紫 | `assets/ability/scripts/189_youkai_yukari_s5.anm.txt` … `193_…_s9` | TH08 `player00.anm` script 5..9 |
| 1 魔理沙 | アリス | `194_youkai_alice_s5` … `198_…_s9` | TH08 `player01.anm` script 5..9 |
| 2 咲夜 | レミリア | `199_youkai_remilia_s5` … `203_…_s9` | TH08 `player02.anm` script 5..9 |
| 3 早苗 | 射命丸文 | `218_youkai_aya_s0` … `222_…_s4` | TH16 `pl03.anm` script 0..4 |

⚠ **但它们在成品里目前是"空白"**：2026-09-13 的图集合并按**用户裁决**把这一批
（108 妖侧本体 55 张）**排进 `REF … RETIRED_BLANK`**（1×1 全透明），
理由与实装记录见 `ATLAS-MERGE-AUDIT.md` §9 与 `mods/th18.v1.00a/card-expand/NEXT.md:2204`
（原文：「108 妖侧本体 55 张按用户裁决**不进图集但源图不删**（走 `REF` → 1×1 `RETIRED_BLANK`）」）。
⇒ 脚本能编、能挂，但**切图源还在 `assets/ability/`、只是没进图集** ⇒ 要走 §1.4 的路之前，
得先把那一批从 `REF RETIRED_BLANK` 换回真图（`ORDER.txt` 的 `REF` 行）。
⇒ **缺口是"路由 + 一行 ORDER 决定"，不是"没有图"**；路由见 §1.4。

### 1.4 ❓ 路由：三条路，都不轻（**需 owner 拍板**）

| 路 | 做法 | 代价 | 一手依据 |
| --- | --- | --- | --- |
| **A. 写本体 VM 的 sprite** | `on_draw` 里每帧把本体 VM 的 sprite 换成 5..9 那一套 | ❌ **要求那些 sprite 在 `pl0X.anm` 里**（贴图按 VM 的 `vm+0x20` 页取，`th18:0x481117`–`0x481136`）；而本仓铁律是 **`pl0X.anm` 一个字节都不改**（`assets/build_player_anm.py:2`，AUDIT §AL33）⇒ 除非改政策 | `th18:0x477b00`（`ce_anm_set_sprite`）+ `th18:0x4810d0` |
| **B. 跟随 VM + 藏本体** | 起一个跟随自机的 VM 画妖侧图，再把**本体藏起来** | ❌ 「藏本体」没有安全字段：状态闸 `player+0x476ac`（属玩家状态机）、`vm+0x524` alpha / `vm+0x54` scale（**玩家每帧自己写**）全都没坐实 | 见 `docs/superpowers/specs/2026-09-12-youkai-108-design.md` §11.3.3(b) |
| **D. 跳过「画本体」那一步** | 在 `th18:0x45cafc`/`0x45cb06` 附近下站点、自己画 | ⚠ **新站点**（预算余量 0）⇒ 按本包纪律**先停下报 owner**；且窗口要过 `sitecheck` W1–W6 | `th18:0x45cac0`（`Player__on_draw` 优先级 `0x1d`）|

**本卡的现状** = 路 C（什么都不做，只登记），`youkai.c` 的 `on_draw` 那一段是**死代码**
（`YK_BODY_SPRITE_FOCUSED = -1` 恒假）。这一条**故意不动**：它需要 owner 在 A/B/D 里选一条。

## 2. 发现：人妖槽（`youkaiGauge`）

### 2.1 数据布局（逐指令核过）

| 量 | 位置 | 一手 |
| --- | --- | --- |
| `g_GameManager` | `0x160f508` | 全 exe 352 处 `mov ecx,0x160f508`（`this`）|
| `globals`（=`ZunGlobals*`）| `[0x160f508+8]`（`ds:0x160f510`）| `th08:0x406d14`–`0x406d1d` |
| **`youkaiGauge`**（i16）| `globals + 0x22` | `th08:0x406d1d movsx edx,WORD PTR [ecx+0x22]` |
| `youkaiGaugeCopy`（i16）| `globals + 0x20` | `th08:0x43c153`–`0x43c157` |
| 4 个阈值（i16）| `g_GameManager + 0x3ddf8/0x3ddfa/0x3ddfc/0x3ddfe`（另 +0x3de00/0x3de02）| `th08:0x406d24` / `:0x406d84` / `:0x406d54` / `:0x406db4` |

### 2.2 写入者：唯一的入口 + 7 个调用点

**入口** `GameManager::AddToYoukaiGauge(delta, unconditional)` `0x43c0bb`（一手逐指令）：

```
43c0c2  cmp DWORD PTR ds:0x17d6ed4,0x0     ; ★ 全局「暂停/练习」开关
43c0c9  je  0x43c0d6
43c0cb  cmp DWORD PTR [ebp+0xc],0x0        ; 第二参 = 0（= "受闸"）
43c0cf  jne 0x43c0d6
43c0d1  jmp 0x43c15b                        ; ⇒ **自机正在放符卡/射击时，增量为 0 的那些调用被整条丢掉**
43c0d6  eax = globals; eax = sign16([eax+0x22]) ; eax += delta
43c0e9  WORD [globals+0x22] = ax
43c0fa  cmp gauge, sign16([this+0x3ddf8])  ; 下限
43c103  jge …                              ; 低于下限 ⇒ 夹到下限
43c128  cmp gauge, sign16([this+0x3ddfa])  ; 上限
43c131  jle 0x43c147                       ; 高于上限 ⇒ 夹到上限
43c157  WORD [globals+0x20] = gauge        ; gaugeCopy = gauge
```

> 反编译版（`GameManager.cpp:1306`）把第一道闸写成 `if (g_Player.shotActive != 0 && param_2 == 0) return;`
> —— 与逐指令读到的一致（`0x17d6ed4` = 那个"符卡中"的全局，🍚 名字未定）。

**7 个调用点**（全 exe 穷举 `call 0x43c0bb`；delta 与归属）：

| 调用点 | delta | 触发 | 一手 |
| --- | --- | --- | --- |
| `EnemyManager` 附近 `0x42da2a` 一带 | **+0xc8 (200)** / **−0xc8** | 敌人被击破（`isYoukaiMode` ⇒ +、否则 −，同段两次调用）| 反编译 `EnemyManager.cpp:670-676` |
| `ItemManager`（收集道具）| `isYoukaiMode ? +itemScore : −itemScore` | 每收一颗普通道具 | `ItemManager.cpp:597` |
| `Player` 开火 | `±0x6590 / shotInterval` | 射击帧（人/妖反向）| `Player.cpp:447/451` |
| `Player::UpdateMovement` 聚焦漂移 | `±(youkaiDelta * g_ShotSpeed)` | 低速按住/松开的那段状态机（`youkaiDelta ∈ {2,3,5}` 或 `{−2,−3,−5}`）| `Player.cpp:1097/1127` |

### 2.3 读者：4 个阈值判定 + HUD

```c
GaugeIsExtremelyHuman()  0x406d10 : gauge <= [this+0x3ddfc]     (setle)
GaugeIsModeratelyHuman() 0x406d40 : gauge <= [this+0x3de00]     (setle)
GaugeIsExtremelyYoukai() 0x406d70 : gauge >= [this+0x3ddfe]     (setge)
GaugeIsModeratelyYoukai()0x406da0 : gauge >= [this+0x3de02]     (setge)
```
⚠ 反编译的字段名（`…EffectsThreshold` / `…TintThreshold`）与「Extremely / Moderately」的搭配
**和源码注释不一致**（`GaugeIsModeratelyHuman` 读的是名字叫 Tint 的那一格）⇒ 以**地址**为准。

**「极端」档的可见后果**（全 exe 读 `0x406d10`/`0x406d70` 的调用点）：

| 读点 | 后果 |
| --- | --- |
| `0x405ba4`–`0x406d10` 一族 | 给 HUD 数字**染色**（`c6 81 6e 14 00 00 70` = 设 ASCII 颜色三字节）|
| `EnemyManager.cpp:765` | `GaugeIsExtremelyHuman() \|\| GaugeIsExtremelyYoukai()` ⇒ 敌人被击破时的掉落/表现分档 |
| `Item::CollectTimeOrb` `0x4413cb` | 时点收集那一帧给人妖槽 `±111`（聚焦 = +111）|
| `Item::CollectPoint` `0x440f03` | **极端人间 ⇒ 蓝点分数 ×2**（`if (GaugeIsExtremelyHuman()) score += score`）|
| `PlayerBullet` 染色（`0x44fd58`/`0x44fd6f`）| 极端妖气 ⇒ 使魔弹**染色 / 加伤**（行字段 `+0x1e`）|
| `GameManager::GetShotSpeed` 一族 | 极端档改**射击间隔**（`g_ShotSpeed`）|

**HUD**（`AsciiManager::DrawYoukaiGauge` 一族，`AsciiManager.cpp:1016-1075`）：

```c
gauge1 = GetYoukaiGauge();
youkaiGaugeCursor.pos.x = gauge1 * 112.0f / 2.0f / 10000.0f + youkaiGauge.pos.x + 64.0f;   // :1019
gauge2 = (gauge < 0) ? … : GetYoukaiGauge();
    …pos.x = gauge2 * 80.0f / 2.0f / 10000.0f + youkaiGauge.pos.x + 64.0f;                    // :1024
percentageText.pos.y = cursor.pos.y - 7.0f;                                                   // :1026
DrawPercentage(&percentageText.pos, GetYoukaiGauge(), …);                                     // :1066
```
- **4 支 VM**（`ascii.anm`）：槽条 `script5`、人间图标 `script6`、妖怪图标 `script7`、游标 `script8`
  （`AsciiManager.cpp:198-201` 的三次 `SetAndExecuteScriptIdx` + 游标）；
- 人间/妖怪图标的 x 偏移由**阈值**决定：`humanIcon.pos.x -= humanLimit * 56.0f / -10000.0f`、
  `youkaiIcon.pos.x += youkaiLimit * 56.0f / 10000.0f`（`AsciiManager.cpp:209-210`）
  ⇒ 槽的两端就是两个限值，**量程 ±10000、条宽 112 px、图标各 56 px**。

### 2.4 🟡 **限值在本 build 里没有写点**（一条要留意的负结论）

**发现**：把 `.text` 全文搜 6 个字段的位移字节（`f8 dd 03 00` / `fa dd 03 00` / `fc dd 03 00` /
`fe dd 03 00` / `00 de 03 00` / `02 de 03 00`）⇒ 各命中 1–2 处，**逐条看都是读**（`movsx`），
**没有任何写**；绝对寻址形式（`mov [0x16ed3xx],…`）也不存在。

**推测**：① 这几个字段在 v1.00d 是纯读的（值来自 `.data` 初值）；② 或者写入发生在别处（`.data1` / 另一构建）。

**验证 / 反证**：`g_GameManager` 落在 `.data` 的**零填充尾段**
（section 头：`.data` VA `0xc6000`、VirtualSize `0x1413840`、RawSize 仅 `0x6e00`；
`0x160f508 - 0x400000 = 0x120f508` 远在 raw 之外）⇒ **装入时是 0**。
而 `0x160f508` 本身就是**它自己**（全 exe 352 处 `mov ecx,0x160f508`），对象没有别的基址。
⇒ 若真为 0，`AddToYoukaiGauge` 会把槽夹在 `[0,0]`、两个图标的偏移也是 0 —— 与「游戏里槽会动」的常识冲突。

**结论**：🟡 **机制成立、限值来源未定**（要在实机/调试器里读一次 `[0x160f508+0x3ddf8]` 才能定）。
⇒ 这条**不影响**卡 108 的实现路径选择，但**影响能不能照抄数值**：
在拿到真实限值之前，人妖槽的**量程与阈值**只能标 `❓ 自定`（或用 `±10000` 这个从 HUD 反推出来的量程）。

## 3. 对卡 108 的落点（TH18 侧）

| 项 | TH18 有没有现成的 | 建议 |
| --- | --- | --- |
| #2 妖侧本体 | ❌ 没有「换本体脚本」的 SDK 面；`pl0X.anm` 禁改 | **等 owner 在 A/B/D 里选一条**（§1.4）|
| #4 人妖槽（数值）| ❌ 没有 | 卡侧自建 i16 标量 + 4 阈值；7 个写入者里**能复刻的 4 个**：击破敌（±200）、收道具（±itemScore）、射击帧（±`0x6590/shotInterval`）、聚焦漂移（±`youkaiDelta*g_ShotSpeed`）——都需要「击破/收集」回调（`on_enemy_death` / `on_item_collect` 已有）|
| #4 人妖槽（HUD）| ✅ **有现成的 HUD 层**（`ce_hud_number_*` / `ce_hud_vm_text_*` / `ce_hud_slot_*`）| 照 `AsciiManager` 的**几何**（条宽 112、图标 56、量程 ±10000、百分比文字在游标上方 7 px）画；贴图走 `ascii.anm` 的 4 支脚本**原生切图**（不许自绘）|
