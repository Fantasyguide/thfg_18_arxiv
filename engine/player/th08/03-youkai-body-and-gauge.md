# TH08 「人妖」两件事：**妖侧本体外观**与**人妖槽**（卡 108 的 #2/#4）

> **版本**：TH08 v1.00d（`th08.exe`，imagebase `0x400000`）。本文裸地址默认属该版本；引用 TH18 须写成 `th18:0x…`。
> **方法**：社区反编译 `local/vendor/th08_dec`（`src/Player.cpp` / `GameManager.cpp` / `AsciiManager.cpp`）
> 当**源码级**依据（用户 2026-09-16 口径「你可以认为反编译 = 源代码」），关键处再用本仓自己的
> `local/_scratch/th08_full.asm` 与 `.data`/`.rdata` 直读**逐指令复核**。
> **可信度**：§1 ✅ 一手（反编译 + 逐指令，**2026-09-22 补齐换形那一帧与 TH18 侧的一一对应**）；
> §2 的**机制** ✅、**限值** ✅（2026-09-22 订正：写点在 `th08:0x44d9ee`–`0x44da8f`，见 §2.4；
> TH18 侧没有这套字段，见 §2.5）。
> **缘起**：`SINGLE-CARD-TEST.md` §th08 #2「缺少妖侧本体」、#4「缺少人妖槽」。

## 0. 一句话结论

| 现象 | 原作是什么（一手） | 卡 108 现状 |
| --- | --- | --- |
| **#2 妖侧本体** | 自机本体**就是同一台 VM**，只换脚本号：人侧 idle = `0` / 移动 = `1..4`，**妖侧 idle = `5` / 移动 = `6..9`**（同一个 `playerNN.anm`）| ✅ **2026-09-22 已实装（路 A）**：55 张经 `build_player_anm.py` 追加进 `pl0X.anm`，低速整支换脚本 —— 见 §1.4/§1.5（🟡 未实跑）|
| **#4 人妖槽** | 一个 i16 标量 + 4 个阈值（**6 个限值字段**，写点见 §2.4）+ 7 个写入者；HUD = `ascii.anm` 的 4 支脚本 | ❌ 完全没有（TH18 也没有，见 §2.5）|

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

**owner 2026-09-22 裁决：走路 A**（「那就按照推荐来」）。⚠ 表里 A 那一行的「`pl0X.anm` 一个字节都不改」
是**旧政策**：本仓的实际口径一直是「**只追加**」（`build_player_anm.py` 文件头 + `verify_rebuilt`
逐 entry/script/贴图块对账）—— 113 的灵界弹、117 的兽灵弹都是这么进去的。路 A 因此成立。

### 1.5 ✅ **路 A 的实装（2026-09-22）**：追加 20 支脚本 + 4 个 entry，低速**整支换脚本**

**数据侧**（`assets/build_player_anm.py`，只追加、原生部分逐字节不变）：

| 机体 | 源 | 帧数 / 帧尺寸 | 新 sprite | 新脚本号 |
| --- | --- | --- | --- | --- |
| pl00 霊夢→八雲紫 | TH08 `player00.anm` script5..9 | 11 帧 30×46 | 86..96 | 43..47 |
| pl01 魔理沙→アリス | TH08 `player01.anm` script5..9 | 11 帧 30×46 | 96..106 | 49..53 |
| pl02 咲夜→レミリア | TH08 `player02.anm` script5..9 | 9 帧 **46×46** | 101..109 | 32..36 |
| pl03 早苗→射命丸文 | **TH16** `pl03.anm` script0..4 | 24 帧 30×46 | 97..120 | 34..38 |

- **贴图** = `assets/ability/youkai/YOUKAI_{YUKARI,ALICE,REMILIA}_S*.png` +
  `assets/ability/sanae_youkai/YOUKAI_AYA_S*.png`（就是 §1.3 那 55 张）。逐张复核过：PNG 里那段
  **有效画区**与原版切图**逐像素相等**（55/55，MAE = 0.000）。⚠ **sprite 矩形必须写原版那个**
  （30×46 / 46×46），不是画布的 32×64 / 64×64 —— 引擎按矩形中心定锚点
  （`AnmVm__set_sprite` `th18:0x477b00` 把 w/256、h/256 写进 `vm+0x3d4/+0x3e8`）。
- **脚本正文逐条抄源**（含 TH08 script8/9 的 `flipX()`；TH18 的 v8 支持它，原生 `enemy.anm` 等处 50+ 次），
  只做两件事：`sprite()` 号重编号 + **给 TH08 源补上 TH18 原生本体脚本同款的三行前导**
  （`-1: type(0); layer(14); resampleMode(1); 0:`）。为什么必须补：`AnmVm__load_script`
  `th18:0x407420` 把整块脚本模板 `rep movs` 0x151 dword 拷进 VM（只保留层 `vm+0x18` 与 `vm+0x548`）
  ⇒ 前导不跟着来就会停在模板默认值上；TH16 的源脚本自带前导 ⇒ 不补。

**引擎侧**（`native/cards/youkai.c` 的 `yk_body_sync` / `yk_body_load_script`）：

- 换脚本三步曲**逐条照抄引擎自己的两处用法**（`Player__initialize` `th18:0x45a8dd`–`0x45a8fe`
  与移动动画状态机 `th18:0x45b621`–`0x45b645`）：`0x407420`（load，ret 8）→ `vm+0x5c0 = 0`、
  `vm+0x5bc = 0` → `AnmVm__run` `0x478580`（thiscall(vm)，裸 ret）。
- 状态机 = `th18:0x45b610`–`0x45b7a8` 的纯逻辑复刻（`youkai_core.c` 的 `yk_body_state`）：
  `dx<0&&px>=0 → 1` / `dx==0&&px<0 → 2` / `dx>0&&px<=0 → 3` / `dx==0&&px>0 → 4` / 否则不变；
  `dx` = `player+0x47934`（全 exe 只有这 5 处引用，都在同一函数里）。
- **换形那一帧**（TH08 的对应物）：进低速 ⇒ 妖侧 idle（`th08:0x44b12b push 0x5`）、
  出低速 ⇒ 人侧 idle（**`th08:0x44b385 push 0x0`** —— ⚠ vendor 反编译 `Player.cpp:942` 把它印成
  `5`，**指令是 0**），两处都还把 `velocityX`（`player+0xe2a9c`）清 0 ⇒ 下一帧必定触发一次状态机；
  我们照做（把 `prev_dx` 清 0）。

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
  （`AsciiManager.cpp:198-201` 的三次 `SetAndExecuteScriptIdx` + 游标）；**百分比数字 = `script4`**
  （`AsciiManager.cpp:202`），它的正文只有 `sprite(sprite136); zWriteDisable(1); static();`
  —— 一支**可换字的画布**，引擎在一片里反复 `SetSprite` + `DrawNoRotation` 画完那串数字（见下）。
- 人间/妖怪图标的 x 偏移由**阈值**决定：`humanIcon.pos.x -= humanLimit * 56.0f / -10000.0f`、
  `youkaiIcon.pos.x += youkaiLimit * 56.0f / 10000.0f`（`AsciiManager.cpp:209-210`）
  ⇒ 槽的两端就是两个限值，**量程 ±10000、条宽 112 px、图标各 56 px**。

#### 2.3a ★ 贴图与「停稳位置」（2026-09-22 晚，卡 108 的 owner 第 3 条）

`thanm -l 8 -m local/vendor/thpages/static/mapfile/v2.anmm local/th08.v1.00d/dat/ascii.anm`
逐条读出来的矩形（**全部在 entry0 = `data/ascii/ascii.png`**，256×256）：

| 件 | script | sprite | 矩形 (x,y,w,h) | 锚点 |
| --- | --- | --- | --- | --- |
| 槽条 | 5 | **155** | `(0, 224, 128, 16)` | `anchorTopLeft` |
| 人间图标 | 6 | **153** | `(160, 208, 16, 16)` | `anchorTopLeft` |
| 妖怪图标 | 7 | **154** | `(176, 208, 16, 16)` | `anchorTopLeft` |
| 游标 | 8 | **152** | `(128, 208, 8, 12)` | **无 anchor 调用 ⇒ 中心** |
| 百分比字形 | 4 | **136..148**（= `0x88..0x94`）| `(i*8, 208, 8, 12)`、i = 0..12 | 中心 |

字形号序（`DrawPercentage` `0x405e10` 的注释 + 逐条 `SetSprite`）：**0x88..0x91 = '0'..'9'、
0x92 = '%'、0x93 = '.'、0x94 = '-'**。

⚠ **停稳位置不是脚本里那个 `pos()`**：`script5` 的正文是 `pos(32, 465)` → `posTime(40, 0, 32, 449, 0)`
⇒ **465 是滑入动画的起点（480 高的屏外）**，停稳在 **`(32, 449)`**；两个图标与游标本来就在 449/453。
⇒ 槽条的 y 是 **449**，与图标**同高**（卡 108 原来照 `pos()` 抄了 465 ⇒ 条比图标低 16 px）。

百分比数字的排布（`DrawPercentage`，全部一手）：

```
n = 4 (+1 负号) (+3 若 |v| >= 10000 / +2 若 >= 1000 / +1 否则)
x -= n*3.5 − 3.5 − 4.0                      ; 居中
负号 '-'        x += 7
|v| >= 10000 ： '1' '0' '0' 各 +7，'.' +5，两个小号 '0' '0' +5 / +5
|v| >= 1000  ： 整数两位 +7/+7，'.' +5，小号两位 +5 / +5
否则         ： 整数一位 +7，'.' +5，小号两位 +5 / +5
最后 '%'；小号 = scale 0.8 且 y += 2（画完复位）
```
数字的颜色 = 与槽条**同一套**五档染色（`AsciiManager.cpp:1030-1058`：极人 `0x7070ff` / 中人 `0xb0b0ff` /
极妖 `0xff7070` / 中妖 `0xffb0b0` / 其余 `0xffffff`）。

### 2.3b ★★★ 坐标空间（搬到 TH18 时的坑）—— **2026-09-22 一手 + 实跑双双定案：这类 VM 是「1 单位 = 1 表面 px」**

> **订正**：本节 2026-09-22 晚那一版写的是「没有 `resolutionMode` ⇒ 坐标按设计空间解释、绘制时 ×2
> ⇒ 卡侧必须写 640 空间值」。**那一版是反的**（卡 108 因此把 HUD 改坏了一版）。下面是逐指令复核。

**① 一手（`th18:0x4063d0` = 「VM 位置 → 屏幕坐标」那一步；绘制路径上的调用点 `th18:0x481770`）**：

```
4063e8  mov  eax,[edi+0x538]               ; edi = vm
4063ee  and  eax,0x1c00000                 ; resolutionMode 占 bit22–24
4063f3  cmp  eax,0x400000 ; je  406421     ; mode 1 → xmm1 = [0x56aca0]              （= 世界倍率）
4063fa  cmp  eax,0xc00000 ; je  406421     ; mode 3 → 同上
406401  cmp  eax,0x800000 ; je  40640f     ; mode 2 → xmm1 = [0x56aca0] × [0x4b9138]
406408  cmp  eax,0x1000000; jne 406451     ; mode 4 → 同 mode 2；★ 其余（含 mode 0 = 不写）⇒ 跳到 406451
40640f  movss xmm1,[0x56aca0]; mulss xmm1,[0x4b9138]        ; [0x4b9138] = **0.5**（.rdata 直读 0000003f）
406429  movss xmm0,[esi] / mulss xmm0,xmm1 / movss [esi],xmm0 ; x/y/z 三分量逐一乘（esi = &pos）
```

⇒ **mode 0（脚本里不写 `resolutionMode`）那个分支根本不进乘法**，位置按原值用；`[0x56aca0]` 是运行时
世界倍率（1280×960 下 = **2.0**）⇒ **mode 0 与 mode 2 在本作的实际分辨率下等价（都 ×1）**。
**尺寸**那一支逐位同形（`th18:0x4817c6`：mode1 → ×2、mode2 → ×1、**mode 0 → 直通**，`0x481819`
两句 `movss` 原样取用）⇒ 结论：**这类 VM 的 1 单位 = 1 表面 px；1 texel = 1 表面 px**。

**② 实跑（决定性，同一轮里的 owner 原话）**：卡 108 曾按「必须写 640 空间」实现（发布 md5
`ea93d66e0957c0d39dc8dd29d4e84f2a` = 日志 `th18_card_expand.log` 2026-09-22 18:20 那次装的 DLL），
owner 报 **「百分比位置完全错位，高了半个屏幕」**（那时 `text_y` 写 427 ⇒ 屏上就在 427，正确值 854）
＋ **「人妖 HUD 整体比原版缩小了 50%」**（16×16 的原生贴图按 16 px 画）。这两条**只在 ×1 下成立** ——
若真是「不写 ⇒ ×2」，位置会落在 854、贴图会是 32 px，owner 就不会报它们。

**③ 落笔纪律（TH18 侧）**：TH08 的一手几何（`(32,449)` 那一套）是 **640 空间**的字面量
⇒ 落到这类 VM 上要 **×2**：位置、逐字形步进、以及贴图 scale **三者一起乘**；
⚠ **别改成 `resolutionMode(1)`**（那才真 ×2 ⇒ 整块跑到屏外）；写 `(2)` 与不写在本作分辨率下等价。
⚠ 本节 ① 与 `mods/th18.v1.00a/card-expand/HUD-LAYOUT.md` §4.1b 的「不写 ⇒ 坐标 ×2」那一行**冲突**，
  已在该节留订正块；两处复核的是**同一段指令**，以本节为准（另有实跑合证）。


### 2.4 ✅ **限值的写点找到了**（2026-09-22 订正本文旧版的「没有写点」）

> **订正**：本文 2026-09-16 那一版把这一节写成 🟡「本 build 里没有写点」。**那是错的** ——
> 否定的结论尤其要回指令边界复核（[`METHOD.md`](../../../METHOD.md) 的硬规矩 4）。写点就在
> `Player::on_registration` `th08:0x44d650` 内，用**绝对寻址**一次性写全 6 个（一手逐条）：

```
44d9ee  mov WORD PTR ds:0x164d300,0xd8f0   ; human_limit    = −10000   （GAME_MANAGER+0x3ddf8）
44d9f7  mov WORD PTR ds:0x164d304,0xe0c0   ; human_effects  = −8000    （+0x3ddfc）
44da00  mov WORD PTR ds:0x164d308,0xf830   ; blue_tint      = −2000    （+0x3de00）
44da09  mov WORD PTR ds:0x164d302,0x2710   ; youkai_limit   = +10000   （+0x3ddfa）
44da12  mov WORD PTR ds:0x164d306,0x1f40   ; youkai_effects = +8000    （+0x3ddfe）
44da1b  mov WORD PTR ds:0x164d30a,0x7d0    ; red_tint       = +2000    （+0x3de02）
```

`0x164d300 = 0x160f508 + 0x3ddf8` ⇒ 六格**连号**，与 §2.3 那四个判定函数读的格子**逐一对上**
（human_limit / youkai_limit / human_effects / youkai_effects / blue_tint / red_tint）。

**为什么旧版会漏**（两个错叠在一起）：旧版搜的是「**位移**字节 `f8 dd 03 00` …」，而这 24 条写
全是**绝对寻址**（`66 c7 05 <addr32>`），一个位移字节都不含；它另外猜的绝对形式 `0x16ed3xx`
**多算了一位**（真值 `0x164d30x`）。⇒ 一次「搜错形式 + 猜错地址」就得出一条错的否定结论。

**写点全集**：全 exe 对 `0x164d300`–`0x164d30a` 的写 = **24 条**，**全部**在
`th08:0x44d9ee`–`0x44da8f` 这一段里（同一函数里另有 6 处是读）。它按 `shottype`
（`GAME_MANAGER+0x3dba9` = `0x164d0b1`）分支写五套值：默认 / `==3` / `==10` / solo-human /
solo-youkai，逐套数值见 [`research-th08-night-points.md`](https://) 的同名小节（本机
`local/_scratch/research-th08-night-points.md` §3.3，与本节互为独立复核，结论一致）。
（同段的 `0x164d30c` 是**另一格** dword，不属这 6 个限值。）

**对卡 108 的意义**：限值**可以照抄**（不必再标 `❓ 自定`）：量程 ±10000、四档 ±8000/±2000
（solo 机体把**自己那一侧**的 limit 收到 ±2000 ⇒ 见 §3.3 的两条推论）。
⚠ 但 **TH18 侧没有这套东西**（见 §2.5），所以卡侧仍要自己维护 6 个限值 —— 数值有出处，载体没有。

### 2.5 ❌ **TH18 侧没有「人妖槽」**（一手负结论，三条独立证据）

| 证据 | 一手 |
| --- | --- |
| 四个**特征常量**在 `th18.exe` 里一次都不出现 | `th18.dis` 全文 grep：`0xd8f0` / `0xe0c0` / `0xf830` / `0xec78` 各 **0** 命中 |
| TH18 的 globals（`0x4cccdc`–`0x4cd7xx` 一带）里没有这种**成对的 i16 阈值** | 全 exe 对 `0x4cxxxx` 的 **word** 绝对写只有 2 条（`0x4cd00e`、`0x4cd76c`），与这 6 个值无关 |
| 符号表里没有任何 gauge / youkai / human 相关函数 | `local/th18.v1.00a/th18-funcs.json`（2333 条）逐条筛名字：0 命中 |

⇒ **结论**：TH18 没有「人妖槽」这个系统（它没有人类/妖怪双形态），**没有对应字段、更没有写点**；
卡 108 若要做人妖槽，**6 个限值必须由卡自己维护**（数值抄 §2.3/§2.4，载体自建）。

### 2.6 ★★★ 人妖切换那一刻：**本体**的光环 + 两下爆闪（2026-09-22 夜一手，卡 108 #23）

> 起因：owner「人妖切换的时候**在本体会有一个机体印象色的光环**」。
> 全部一手来自 `Player::move`（`th08:0x44aec0`，社区名）里的**两支**——进妖侧那支从 `0x44b11f` 起、
> 妖→人那支从 `0x44b379` 起；两者都只做「换本体脚本 + 特效 + 计时器」，**不改弹、不改判定**。

#### 2.6.1 光环（持续特效）= `etama.anm` script54

| 项 | 一手 |
| --- | --- |
| 起 | `0x44b171` 门 `player+0xbe834 == 0` → `0x44b17d`–`0x44b194` 五次 `push` + `call 0x425870`（5 参，见下）|
**那五个实参逐条**（`ret 0x14` ⇒ 恰好 5 参）：`push -1`（→ `vm+0x1f0` 颜色）/ `push 1`（未读，ZUN 的填充位）/
`push 2`（**索引槽位**，`0x42587a add eax,0x280; imul eax,eax,0x360`）/ `push &player+0x2b4`（位置）/
`push 0x16`（模板号）；`ecx = 0x4ece60`（EffectManager），callee 名 =
`EffectManager::sub_425870_creates_indexed_effect`（re-data），它再用 manager 的 `+0x8b054`（`etama_anm`）装脚本。

| 模板 | `0x4c6d30[0x16]` = **script54**（init `0x426c40`、update 0）|
| 句柄 | `0x44b19c mov [player+0xbe834], eax`（re-data 名 `zPlayer.__effect_be834`）|
| 位置 | init `0x426c40`：`0x426c5f mov ecx,ds:0x17d61ac` 一族 ⇒ 每帧把 VM 位置钉到 **`g_Player+0x2b4`**（`0x17d5ef8 + 0x2b4`）|
| 颜色 | 第 5 参 `-1` → `0x425943 mov [vm+0x1f0], eax` = `zAnmVm.color_1` **中性**（`0xFFFFFFFF`）⇒ **不逐机体变色** |
| 贴图 | `etama2.png` sprite218 `(0,112,64,64)` 64×64：可见 1380 px、均值 RGB ≈ **(227,133,133)**、色相直方图「红 940 + 白/灰 440」⇒ 淡红·粉白火花云 |
| 脚本行为 | 淡入 20 帧 → `stop()` → `interruptLabel(1)` 淡出 30 帧 → `delete()`；自转 **1.5°/帧**（逐条见下）|
| 销毁 | 妖→人那支 `0x44b3d8`–`0x44b3e8`：`SetInterrupt(光环, 1)`（`0x407120`）⇒ 30 帧淡出后自删；`0x44b3eb` 清句柄 |


**script54 正文逐条**（`thanm -l 8 -m v2.anmm etama.anm`）：`sprite(sprite218)` / `zWriteDisable(1)` /
`alpha(0)` / `fsetRand(%F0, 1.0f)` / `fsetAdd(%F0, %F0, 0.5f)` / `fset(%F0, 0.02617994f)` /
`angleVel(0.0f, 0.0f, %F0)`（随机基准 + 每帧 +1.5°）/ `alphaTime(20, 1, 255)` / `+20: stop()` /
`interruptLabel(1){ alphaTime(30, 1, 0); +30: delete(); }`。
⚠ 这三个浮点助记符在 v8 anmm 里**都有**（`fset` 101 / `fsetAdd` 113 / `fsetRand` 123）⇒ 搬到 TH18 **零翻译**。

**负结论（省后人白找）**：把 `Player::move`（`0x44aec0`–`0x44d000`）里所有落在 `.rdata/.data` 的引用
全捞出来看过一遍：15 个引用**全是函数指针/结构体**，**没有逐机体颜色表**；光环的贴图来自共享的
`etama.anm`（不是各机体自己的 `playerNN.anm`）⇒ **TH08 的这圈光环是固定色**，
「机体印象色」在样本里**找不到出处**（owner 的口径与一手不符，如实登记在 `TUNING.md` §2.0zd）。

#### 2.6.2 两下**一次性**爆闪（本体）—— 与使魔那一对**不是同一对**

| 方向 | 一手 | 模板 → 脚本 | 颜色（`0x425430` 的第 4 参）|
| --- | --- | --- | --- |
| 人→妖 | `0x44b155 push 0x80ff8080 / push 1 / push &player+0x2b4 / push 0x1d` → `0x44b16c call 0x425430` | `[0x1d]` = script**58**（`alpha(0)→255`、`scale 5.0→0.4`，`+10: delete()`）| `0x80ff8080` |
| 妖→人 | `0x44b3af push 0x808080ff / … / push 0x1c` → `0x44b3c7` 同函数 | `[0x1c]` = script**57**（`alpha(0)`、`scale 0.5→5.0`）| `0x808080ff` |

两支都用 `sprite193`（`etama2.png (65,209,30,30)`）——**与使魔那对（`FUN_0042c420` 的 script59/60）用的
是同一张贴图但不同的脚本**：59 = `alpha(96)→0`（起在**敌人**身上）、58 = `alpha(0)→255`（起在**自机**身上）。
⇒ 移植时**别把这两对混成一对**。

#### 2.6.3 `player+8` 的真身（re-data 名 `__maybe_time_in_focused_unfocused_state` 是**错的**）

它是**人妖切换的进度计时器（0..7）**，两支共用：

- 切换开始那帧清零：`0x44b1a5`（人→妖）/ `0x44b3f8`（妖→人）；
- 之后每帧 `+1`：`0x44b1be inc ecx`（另一支同理）；
- `>= 7` 才把 **`player+5 = is_youkai`** 置 1：`0x44b1d0 cmp [player+8],7` / `0x44b1d9 mov BYTE [player+5],1`
  ⇒ **视觉当帧就换**（`0x44b13a push 5` 换本体脚本发生在计时器清零之前），而**标志位要等 7 帧**；
- 两下爆闪的门是 **`>= 4`**：`0x44b14f` / `0x44b3a9`。

**同区块的旁支（别与光环搞混）**：进妖侧那一支外面还有 `[0x164d0b1] < 4` 的门
（`0x164d0b1` = `GAME_MANAGER+0x3dba9` = `shottype`）—— `>= 4` 是**单人机体**（没有妖侧搭档）
⇒ 不换本体脚本、也不起光环（`0x44b11f` / `0x44b379` 两处同形）。

**另一支「极端槽位光环」**（不是 Shift 光环）：`0x44c02e`–`0x44c051` 在
`gauge_is_extremely_human` **或** `gauge_is_moderately_youkai`（`0x406d10` / `0x406d70`）时起
`SpawnEffect(0x19, &player+0x2b4, slot 8, -1)` ⇒ 脚本**35**（sprite196、`blendMode(1)`、−11.25°/帧），
句柄存 `player+0xe2b24`；`Player::on_tick`（`0x44c390`）按全局 `0x160f534` 开关它的 `vm+0x1f8` bit19。

## 3. 对卡 108 的落点（TH18 侧）

| 项 | TH18 有没有现成的 | 建议 |
| --- | --- | --- |
| #2 妖侧本体 | ❌ 没有「换本体脚本」的 SDK 面；`pl0X.anm` 禁改 | **等 owner 在 A/B/D 里选一条**（§1.4）|
| #4 人妖槽（数值）| ❌ 没有 | 卡侧自建 i16 标量 + 4 阈值；7 个写入者里**能复刻的 4 个**：击破敌（±200）、收道具（±itemScore）、射击帧（±`0x6590/shotInterval`）、聚焦漂移（±`youkaiDelta*g_ShotSpeed`）——都需要「击破/收集」回调（`on_enemy_death` / `on_item_collect` 已有）|
| #4 人妖槽（HUD）| ✅ **有现成的 HUD 层**（`ce_hud_number_*` / `ce_hud_vm_text_*` / `ce_hud_slot_*`）| 照 `AsciiManager` 的**几何**（条宽 112、图标 56、量程 ±10000、百分比文字在游标上方 7 px）画；贴图走 `ascii.anm` 的 4 支脚本**原生切图**（不许自绘）|

## ★ 附：人/妖的本体动画脚本对（一手 `local/vendor/th08_dec/src/Player.cpp`）—— 与 108 卡侧逐条对照

> 起因：owner 2026-09-22「th08 是有反编译源代码的，使魔的样子会随着是否 shift（人/妖）变」。
> 本节回答**玩家侧**那一半：人/妖换的到底是「换整支脚本」还是「同一脚本换 sprite」。结论：**换整支脚本，
> 且脚本号就是「人侧 + 5」**。地址/行号均为 TH08 v1.00d 反编译源码（本机 `local/vendor/th08_dec`）。

### 1. 发现（行号级）

| 态 | 人侧脚本号 | 妖侧脚本号 | 出处 |
| --- | --- | --- | --- |
| 站立（idle）| **0** | **5** | `Player.cpp:652-660`（`isYoukaiMode == 0` ⇒ `setForm0`；否则 `g_PlayerCharacter & 1` 选 5 —— 见 §3 的差异）|
| 移动四态 | **1 / 2 / 3 / 4** | **6 / 7 / 8 / 9** | `Player.cpp:1013-1030`（`isYoukaiMode != 0` ⇒ `SET_MOVE_ANIM(6,7,8,9)`，否则 `SET_MOVE_ANIM(1,2,3,4)`）|

- 宏 `SET_MOVE_ANIM(a,b,c,d)` 在 `Player.cpp:798-806`：**按横向速度的边沿**选脚本 ——
  `speedX < 0 && velocityX >= 0`（向左起步）→ `a`；`speedX == 0 && velocityX < 0`（左停）→ `b`；
  `speedX > 0 && velocityX <= 0`（向右起步）→ `c`；`speedX == 0 && velocityX > 0`（右停）→ `d`。
  调用点先对 `speedX/speedY` 乘 `unk404/unk408`（该形态的速度系数）再比。
- 换态的两条路：`Player.cpp:900`（`isYoukaiMode = 1`）/ `:965`（`= 0`），换态途中显式设脚本 **5**（`:878` / `:942`）。

### 2. 与 108 卡侧的对照

| 项 | TH08 原版（本节一手）| 108 卡侧 | 判定 |
| --- | --- | --- | --- |
| 每侧脚本数 | **5**（idle + 4 移动态）| `YK_BODY_SCRIPT[4][5]` | ✅ 同形 |
| 妖侧怎么取 | **人侧 + 5**（0↔5、1..4↔6..9）| 「进妖侧脚本 = 人侧 + 5」| ✅ **对得上** |
| 换法 | `SetAndExecuteScriptIdx` 整体换脚本（不是换 sprite）| 低速换整支脚本（`yk_body_load_script`）| ✅ 同形 |
| 触发 | 站立/移动由**输入速度边沿**选 | `yk_body_state`（自复刻的状态机）| ⚠ 要逐条核对四个边沿条件（见 §4）|

### 3. TH08 还有、我们没有的两个东西（不影响结论，但要写清）

1. **第三态 `isYoukaiMode == 2`**：`Player.cpp:1534` 在初始化里把它置 2；而**判定用的 `IsYoukai()`
   返回的是另一个字段**（`Player.cpp:353-356`：`return this->unk5;`，`th08:0x40bc40`）⇒
   「`isYoukaiMode`」与「`IsYoukai()`」在 TH08 里**不是同一个读数**（🟡 `unk5` 的写点未查）。
   动画那条判据用的是 `isYoukaiMode != 0`（`:1013`）⇒ **2 也走妖侧动画**。
2. **`g_PlayerCharacter >= 4` 的 solo 分支**：TH08 的 8 个可选角色里 4..7 是「直接选妖侧角色」的单人模式，
   它们的 idle 用 `g_PlayerCharacter & 1` 选 0/5（`Player.cpp:652-660`、`:1020-1028`）。
   TH18 只有 4 个机体 + Shift 二态 ⇒ **我们不需要这个维度**（这也解释了为什么我们的表是 `[4][5]`）。

### 4. 缺口（🟡）

- `Player.cpp:798-806` 的四个边沿条件要与我们 `youkai_core.h` §3.2 的状态机**逐条**对一遍
  （本轮只给一手原文，未做逐条对照）——这是「路 A 本体」实跑验收前唯一还欠的静态对账。
- `unk5`（`IsYoukai()` 的真身）的写点未查；它与人妖槽的关系见本文件 §2（人妖槽）那几节。

## 2.7 ★★★ 左下角那块 HUD 的**完整元素清单**（2026-09-22 夜一手；卡 108 的 #27/#28 以此为准）

> **绘制者**：`AsciiManager::OnDrawHighPrioImpl th08:0x405420`（挂 AsciiManager draw 链**优先级 14**，注册点 `th08:0x402780`）。
> **整块的门**：`th08:0x405acd`–`th08:0x405af7` = `youkaiGauge.IsVisible()`（`vm+0x1f8` bit0，`th08:0x4066e0`）
> ⇒ 为 0 时下面 **6 件（含夜点）一起不画**。绘制顺序 = 游标 → 槽条 → 人图标 → 妖图标 → 百分比 → **夜点（最上层）**。

| # | 元素 | anm / 脚本 / sprite（矩形）| 坐标 · 锚点 | 写值字段 | 一手 |
| --- | --- | --- | --- | --- | --- |
| 1 | **游标** | `ascii.anm` entry0 script8 + sprite **152** (128,208,8,12) | y=**453**；x=**96 + g·112/20000** | `globals+0x22` | x 写点 `th08:0x405afd`–`0x405b31`；draw `th08:0x405b47` |
| 2 | **槽条** | script5 + sprite **155** (0,224,128,16) | `pos(32,465)` → `posTime(40 帧)` → 停稳 **(32,449)** | — | draw `th08:0x405cbd` |
| 3 | **人图标** | script6 + sprite **153** (160,208,16,16) | 基线 (88,449)，`x −= humanLimit·56/−10000` ⇒ **(32,449)** | `GM+0x3ddf8` | `th08:0x402709`–`0x40272e` |
| 4 | **妖图标** | script7 + sprite **154** (176,208,16,16) | `x += youkaiLimit·56/10000` ⇒ **(144,449)** | `GM+0x3ddfa` | `th08:0x402734`–`0x402759` |
| 5 | **百分比数字** | script4 + 字形 **136..148**（8×12；'0'..'9' = 136..145、'%'=146、'.'=147、'-'=148）| y=**446**；x=96+g·80/20000 再按位数居中 | `globals+0x22` | `th08:0x405b4c`–`0x405b86`；`th08:0x405d0c call 0x405e10` |
| 6 | **★ 夜点（8 位数字）** | **同一个 VM（`+0x127c`）** + 字形 **136..145**（`digit + 0x88`）| 左对齐起 **(80,460)**，7 px/字 | **`globals+0x24`**（`zGlobals.pointItemValue`）| `th08:0x405d11`–`0x405e01` |

**五档染色**（槽条 / 百分比 / 夜点**共用**；写 `percentageText.color`@`+0x146c` 再拷给槽条 @`+0x9dc`）：
极人 `0x7070ff`、中人 `0xb0b0ff`、极妖 `0xff7070`、中妖 `0xffb0b0`、其余 `0xffffff`（`th08:0x405bc2`–`0x405c9b`）；
**alpha 取槽条自己的 alpha** ⇒ 夜点跟着槽条一起淡入淡出。

**显隐时机**：脚本 load 后默认段（`interruptLabel(-1)` → `stopHide()` = 隐藏）；`SetGaugeInterrupt(1)`（`th08:0x4070b0`，
`Player::on_registration th08:0x44d9bf` 调）⇒ 置可见 + `alphaTime(15,0,255)` + **40 帧滑入**；玩家压到屏幕左下
（`player+0x2b8 ≥ 400.0`@`th08:0x4b4aa8` 且 `player+0x2b4 < 160.0`@`th08:0x4b4554`，`th08:0x44d4b0`–`0x44d522`）
⇒ 中断 2 = **alpha 64**（变暗）；离开 ⇒ 中断 3 = 回 255；`SetGaugeInterrupt(99)`（`th08:0x44dc88`）⇒ 隐藏。

### 2.7.1 ★ owner #28「叠在进度条上那个最大得点量级的数」= **夜点本体**（并订正一条旧记录）

- **就是 `zGlobals.pointItemValue`（`globals+0x24`）** = 我们说的夜点：`th08:0x405d18 mov ecx,ds:0x160f510`
  （globals 指针）→ `th08:0x405d1e mov edx,[ecx+0x24]` → `divisor = 10^7` 逐位取 8 位、`SetSprite(digit + 0x88)`、
  每画出一位 `pos.x += 7.0`。
- **前导零不画也不占位** ⇒ 从 **x=80** 左对齐（`60000` = 5 格；8 位满格到 x≈133）；字形 8×12 **中心锚点**
  ⇒ 占 y ≈ 454..466 —— **正好压在槽条那条轨上**（owner 的「overlay 在进度条上」✅）。
- **与百分比是两个元素**（各自绘制点、各自 x 公式），**共用同一 VM 与五档颜色**；y=446 vs y=460 ⇒ **上下错开 14 px**。
- ⚠ **订正**：`HUMAN-YOUKAI-GAUGE.md` §3bis.0 曾写「夜点 = 屏幕**中间下方**那个数」——**那是错的**；
  一手它画在**左下角槽条上 (80,460)**、与槽条同门同 alpha 同色、随槽条 40 帧滑入，**没有第二处绘制**。
- **刷新**：每帧现算（不需要"显示副本"字段）。

### 2.7.2 那块 HUD 上**没有**刻符计数/图标；刻符真正画在哪

1. **穷举**：`IsVisible()` 门之后到函数尾（`th08:0x405af7`–`0x405e09`）的全部绘制点 = §2.7 那 6 条；夜点循环字形是 `digit+0x88`，**不含 orb 图**。
2. **刻符计数在右上信息列 (488,184)**：`th08:0x436e2c`–`0x436f32`（值 = `GetTimeOrbs()` `th08:0x421b60` = `globals+0x3c`，
   阈值 = `th08:0x421b80` = `globals+0x40`；`"%d"` + `/` + 阈值；x=488.0@`th08:0x4b4cfc`、y=184.0@`th08:0x4b4d0c`、
   +13/字、超阈值 `SetColor(0xfffff0c0)`）；字形来自 **`asciis.png`(entry1)**，索引 `ASCII + 0x8a`（`th08:0x402d5b`）。
3. **刻符图标本身是道具**：`etama.anm` entry1 sprite **179**（itemType 7 → `+0xac` 映射，`th08:0x4416fb`），画在道具自己的世界位置。
4. **那块区域唯一「悬空」物**：AsciiManager `+0x1520` 的 script9 VM（sprite156、`scaleTime` 脉冲）**每帧 run 但从不绘制/定位**，
   全 exe 也无任何指针引用其地址 `th08:0x4ce340` ⇒ **死代码，别照抄**。

### 2.7.3 「+N 弹字」的四个入口（owner 的「+刻符 提示」= 这张表）

| 入口 | 地址（签名）| 数组 / 容量 | 文本构造 | 寿命 / 运动 |
| --- | --- | --- | --- | --- |
| `CreateScorePopup` | **`th08:0x403200`** `(Float3*, i32 number, D3DCOLOR)` | `+0xb4dc`，720×0x38 | 逐位 `number%10`（`<0` ⇒ 字符 10）| **60 帧**；每帧 `y −= 0.5×framerateMultiplier` |
| `CreatePlayerPointPopup` | **`th08:0x403330`** 同签名 | 同数组 `[720..722]` | 同上 | 同上 |
| `CreateTimePopup` | **`th08:0x403460`** `(pos, number, param3, color)` | `+0x15304`，128×0x38 | `[15][param3 位][14][number 位][13]` | **90 帧，位置不动** |
| `CreateFamiliarPopup` | **`th08:0x403600`** 同 4 参 | 同数组同索引 | 同上，`timer = 88` | 90 帧 ⇒ 实活 ~2–3 帧（Gui 每帧重建）|

- **收刻符那支显示的是「分值」而不是 +1**：`th08:0x441348` 调 **`CreatePlayerPointPopup(&item+0x2a4, value, color)`**；
  `value = (globals+0x30)/2×10`（下限 100），`globals+0x2c ≥ 2000` ⇒ `0x2710`，flag `0x18b8974 != 0` ⇒ 100
  （`th08:0x4412c3`–`0x441304`）；颜色黄 `0xDFFFEF80` / 白 `0xDFFFFFFF`（`th08:0x441316`–`0x44132e`）。
  同函数另有 `AddScore(value)` 与 `AddTimeOrbs(1)` ⇒ **「刻符 +1」是计数（右上），不是弹字**。
- **弹「+1」的是击破魔使那一支**：`th08:0x42b296 CreateTimePopup(&enemy+0x2d88, 1, 0, 0xFFFFFFFF)`，紧跟 `th08:0x42b29b SpawnItem(type7, state1)`；
  同函数另有 `th08:0x42b122`（number = `enemy+0x3380`、色 `0xfff0f00f`、调用前 `SetScale(2,2)`）与 `th08:0x42af78`（number = 魔使数、按数量分档色）。
- **字形**：全走 `ascii.anm` entry0。time/魔使弹字 = `popupText`(`+0x548`) → `GetSprite(char+0x88)` = sprite **136..151**
  （字符 13/14/15 ⇒ **149**（**逐像素确认是「+」**）/150/151；150/151 未定名 ❓）。分数弹字 = `smallScoreText`(`+0x2a4`) →
  `GetSprite(char)`/`+0xb`/`+0x15` = sprite 0..10 / 11..21 / 21..31，帧切换 timer<52 / <56 / 否则（`th08:0x405599` 一族）。
- **排版/上浮/淡出全在代码里**：score 弹字 `pos.x = position.x − charCount×4`、逐字 +8（`th08:0x405462`）；
  time 弹字 `− charCount×3.5`、逐字 `+7×scale`（`th08:0x405905`）。**落点 = 相对被收的道具 / 被击破的敌人**
  （`item+0x2a4` / `enemy+0x2d88`）+ 战场原点 (32,16)（`+0x164d2dc`/`+0x164d2e0`，写点 `th08:0x43a126`）
  ⇒ **与左下角固定 HUD 无关**。**alpha 按与自机距离现算**（`d > 0x1000 ⇒ 0xD0`；`d > 0x400 ⇒ ((d−0x400)×128)/0xc00+0x50`；
  否则 `0x50`；`th08:0x4054e3`–`0x405545` / `th08:0x405983`–`0x4059f1`）⇒ 传入的 `0xDF` alpha **只保留 RGB**。
