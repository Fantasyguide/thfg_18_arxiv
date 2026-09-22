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
