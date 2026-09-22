# TH18 原生卡 42「小槌」的挥击清弹 & TH17 妖梦的蓄力 —— 给卡 111（芙兰蓄力清屏）找一手音效与动画

> **版本**：TH18 v1.00a（`th18.exe`，imagebase `0x400000`）为默认；引用其他版本一律带前缀，如 `th17:0x44a9c7`。
> **方法**：一手平坦反汇编（TH18 用 `local/_scratch/th18.dis`；TH17 现场 `objdump -d -M intel` 取）
> + ExpHP `th-re-data` 的 `th18`/`th17` 函数名与结构体 + `thanm -l` 读 anm 脚本 + `thdat -x` 解 `th17.dat`。
> **可信度**：卡 42 的地址/参数/anm/SE ✅ 一手；TH17 妖梦蓄力的字段、阈值、SE ✅ 一手；
> TH17 蓄力「视觉」的 anm 脚本 ✅ 一手（但**是不是游戏中真正在放的那一支**为 🟡）；
> TH17 音效 id→wav ✅ 一手（从它的播放表本体解出）；落地建议一节整节是 ❓ 自定。
> **⚠ 本文的两次自我订正**：§1.5（卡 42 的两条 SE 名，第一版写成了 `se_ch00`/`se_kira00`）与
> §2.2（TH17 音效表的**读法**与**字段列**，第一版按裸文件偏移读到了 BSS 全 0 区）。
> 两处都保留了「错在哪 / 怎么发现的」的痕迹 —— 别再照着旧结论办事。

## 0. 一句话结论

- **TH18 原生卡 42（KOZUCHI 小槌）的「挥击」= `CardShinmyoumaru__on_tick_2` 里的一个 4 段状态机**：
  小槌跟随玩家 → 20 帧升到 `-320` → 播 `se_ok00` + 半径清弹 10 帧 → 20 帧后收尾。清弹半径 = `30.0 + 4×n`（n=1..10）**像素**，
  出点模式 = `4`（**弹幕与激光都撒道具**，全 57 张原生卡里唯一一张）。
- 它的**特效**是一条 `ability.anm` 的 ANM VM：**script 0x1c（28）**，世界层 `layer(13)`，sprite 只在一开始 `set_sprite(0x29=41)` 写一次，
  之后**全部靠脚本自己切图**（sprite85/86/87 = 48×47 三帧）。存活 ≈ **29 帧**（脚本 `interruptLabel(1)` 那支自己 `delete()`）。
- 它的**音效**两条（都在 TH18 表里核过）：起手 `0x1e se_graze.wav`（−7 dB / 20）、挥击 `0x29 se_ok00.wav`（−3 dB / 100）。
- **TH17 妖梦的蓄力不是按键，是「按着射击键」**：`player+0x19084` 计数，阈值 **60 帧**，
  蓄力音 `se_cat00.wav`（TH17 cfg `0x1e`）与 `se_release.wav`（TH17 cfg `0x37`）**每帧各起一声**（叠成嗡鸣），
  蓄满那一帧只是**停**旧音 + 重起 `0x37` + `interrupt_tree(id, 2)`（**TH17 蓄满时不加新音**）。
  ⚠ **`interrupt 2` 做的不是淡出**（2026-10-01 订正）：那支光环（`pl02.anm` **script19/20**，非 script30）的
  `interruptLabel(2)` = `scale 1.2→1.5` + `pos y −8→−20` + 继续 8 帧循环 = **「满」之后变大并继续动**；
  淡出是它 `interruptLabel(1)` 那支。
- 视觉那支 VM = 一次 `scaleTime(60,1,600,600)` 的 **`drawPoly(1.0f, 32)` 光环**（60 帧长到 600 倍），
  脚本原文在 **TH17 `pl02.anm` script30** 与 **TH18 原生 `ability.anm` script49** 里几乎逐字相同 ⇒ **两作都能直接借配方**。
  （⚠ **2026-10-01 订正**：**起手建的那支不是 script30 而是 script19/20**（sprite 动画环）—— 见 §2.3 的订正块。）
- ⚠ 但 TH17 的蓄力**不消弹**（它是超广范围斩击的**伤害**蓄力）、阈值也不同（60 帧 ≠ 我们的 300 帧）
  ⇒ 我们只能复刻「音效 + 光环」这两块**表现**，机制与数值一律 ❓ 自定。

## 1. TH18 原生卡 42 = KOZUCHI 小槌（Shinmyoumaru）

### 1.1 入口与对象

| 项 | 值 | 一手 |
| --- | --- | --- |
| 类 | `CardShinmyoumaru` | `local/th18.v1.00a/th18-funcs.json` |
| vtable | `0x4b50f0`（构造里 `mov [esi],0x4b50f0`） | `0x411e5b` |
| 构造（recharge=2400） | `0x411e05` 起（`push 0x68` 分配 → `mov [esi+0x48],0x960`） | `0x411e97` |
| `c_press`（虚表 +0x20 槽） | **`0x40f0f0`** | `th18-funcs.json` |
| `__on_tick_2`（虚表 +0x24 槽） | **`0x40eed0`** | `th18-funcs.json` |
| 对象大小 | `0x68` 字节（`push 0x68` → `0x48dc71` = operator new） | `0x411e05` |

卡自己的字段（相对卡片对象 `card`）：

| 偏移 | 含义 | 一手 |
| --- | --- | --- |
| `+0x1c` | **小槌 ANM VM 的 id**（c_press 存、每帧 `get_vm_with_id` 找回来） | `0x40f1f9` 写 / `0x40ef2e` 读 |
| `+0x20` | 上一帧的「经过帧」快照 | `0x40f225` 置 −1、`0x40f069` 每帧刷 |
| `+0x24` | **经过帧**（int，每帧 +1 或 +时间缩放） | `0x40f0a8` / `0x40f09c` |
| `+0x28` | 同一个量的 float 版；**状态 3 里当清弹半径累加器用** | `0x40f08f` / `0x40efed` |
| `+0x2c` | 方向/档位，c_press 置 0 | `0x40f20d` |
| `+0x30` / `+0x44` | zTimer 的「有效」位（位 0） | `0x40f203` / `0x40f24e` |
| `+0x38` / `+0x3c` / `+0x40` | 充能剩余（int / float / 上一帧） | `0x40f266`–`0x40f26f` |
| `+0x58 / +0x5c / +0x60` | 小槌的 x/y/z（**ECL 坐标**，y 起点抄玩家 `player+0x620`） | c_press `0x40f117`–`0x40f129` |
| `+0x64` | c_press 里清 0 的收尾字段 | `0x40f272` |

### 1.2 状态机（`0x40eed0`，跳表在 `0x40f0c0`）

```
40eed3: mov eax,[edi+0x54] ; cmp eax,3 ; ja 0x40f04e ; jmp [eax*4+0x40f0c0]
   表: [0x40eee6, 0x40eef4, 0x40ef6d, 0x40efdc]   ; state 0/1/2/3
```

| state | 地址 | 行为（一手） |
| --- | --- | --- |
| 0 | `0x40eee6` | 清 `card+0x50` 的 `0x20` 位、`call 0x408bf0`（重算充能显示），什么都不做 |
| 1 | `0x40eef4` | ① `card+0x58/5c/60 = player+0x620/624/628`（**每帧跟随玩家**）② `card+0x5c -= 16.0f`（`0x4b9288`）⇒ 小槌上升 ③ `vm = get_vm_with_id(card+0x1c)`（`0x488b40`）④ `vm+0x5 …（明细见文末「附：表格长单元格搬出」） |`
| 2 | `0x40ef6d` | ① `card+0x5c -= 84.0f`（`0x4b92f0`）② `if (经过帧 > 0xa)`：`state=3` + **扫音效池 `0x56ad9c` 找 `0x1e`**（`cmp ecx,0x1e`；池里有就占它、计数置 `-1`，池空则新占一槽、计数 `0x1e`）③ `Soun …（明细见文末「附：表格长单元格搬出」） |`
| 3 | `0x40efdc` | ① `if (经过帧 < 0xa)`：**清弹**（见 §1.3）② `if (经过帧 > 0x28)`：`state=0` + `AnmManager__sub_488cf0(card+0x1c)`（删 VM）+ `card+0x1c = 0` |

**尾段**（`0x40f04e`，四个 state 都汇合到这里）：`card+0x20 = card+0x24`（快照），
再按 `card+0x2c` 选时间缩放表 `0x4b35c0`（`[0]=0x4ccbf0`、`[1]=0`）决定 `经过帧 += 表项值`（正常 = `1.0f` @ `0x4b9178`）。

⇒ **时间线**：state 1 跑 21 帧（经过帧 0→20，`>0x14` 才跳）、state 2 跑 11 帧（21→30，`>0xa` 才跳）、
state 3 跑 11 帧（31→41，`>0x28` 才收）+ 首次进 state 3 那一帧就清弹 ⇒ **清弹共 10 帧（经过帧 31..40）**。
小槌 y 从玩家 y 起、每帧 −16，20 帧后到 `y − 320`（= 弹幕区顶 `y=0` 附近）—— 与社区口径「20f 后挥击」逐字吻合。

### 1.3 清弹：半径怎么算出来的

```
40efe0: mov  eax,[edi+0x24]              ; 经过帧
40efe3: cmp  eax,0xa
40efe6: jge  0x40f033                    ; 经过帧 >= 10 ⇒ 本帧不清弹
40efe8: movss xmm2,[edi+0x28]            ; 累加器
40efed: mulss xmm2,[0x4b9248]            ; × 4.0f
40eff8: push  0x1869f                    ; max_count = 99999
40effd: addss xmm2,[0x4b92b0]            ; + 30.0f   ⇒ xmm2 = 半径（px）
40f005: push  0x4                        ; mode = 4
40f007: lea   esi,[edi+0x58]             ; pos = 小槌当前位置
40f00b: call  0x429370                   ; BulletManager__cancel_radius_as_bomb
...
40f010-0x40f02a: 同样算一遍半径 → push 0x4 / push pos → call 0x449010（激光侧）
40f02f: 尾段把 card+0x28 += 4.0f 且 card+0x24 += 1
```

| 项 | 值 | 一手 |
| --- | --- | --- |
| 入口（弹幕） | `BulletManager__cancel_radius_as_bomb` `0x429370`，ABI `(pos*, mode, max_count, radius_xmm2)` `ret 0x10` | `0x40f00b` |
| 入口（激光） | `LaserManager__cancel_in_radius` `0x449010`，`(pos*, B, A)` + `xmm2` 半径 | `0x40f02a` |
| `mode` | **4**（弹幕与激光都传 4） | `0x40f005` / `0x40f01f` |
| `max_count` | `0x1869f` = 99999 | `0x40eff8` |
| 半径 | **`30.0f + 4.0f × n`**，n = 状态 3 内的第几帧（1..10）⇒ **34 → 70 px** | `0x4b92b0` = 30.0f、`0x4b9248` = 4.0f（实读） |
| 累加点 | `card+0x28`（每帧 +4.0f，尾段 `0x40f0a9`） | `0x40f0a9` |
| 位置 | `card+0x58`（= 小槌当前位，即「挥击点」） | `0x40f007` |
| 出点 | `mode 4` ⇒ **弹幕与激光都撒实体**；全表只有卡 42 用 4 | 见 [`../../bullet/th18/01-cancel-to-item.md`](../../bullet/th18/01-cancel-to-item.md) §4.2 |

★ **半径是 ECL 空间的「像素」而不是 ECL 单位**：`0x429370` 里 `xmm2` 直接与 `(x,y)` 差值的平方比较（`0x429400` 起
`subss` 后两次 `mulss` 再 `comiss`），**没有任何倍率**（唯一的 `×0.5` @ `0x4b9138` 乘的是**弹丸自身的尺寸字段**
`[esi+0x658]`，对 type1/2 的普通弹该项为 0）⇒ 传 34 就是 **34 px**。
旁证：同族卡 43 KANAME 传 `18.0`（`0x4b9290`）、49 BASSDRUM 传 `34.0`（`0x4b92b8`）、53 RICEBALL 传 `48.0`（`0x4b92cc`），
这些量级只有按「px」才说得通（若按 ECL 单位要 ×2 才对上屏幕）。

### 1.4 特效：哪一支 anm、哪个 script、哪个 sprite

c_press 里建 VM 的那一段（**照 `engine/anm/th18/01-vm-instantiate.md` §1 的 Tenshi 配方**）：

```
40f15c: mov   ecx,[ebp-0x4]        ; ecx = ABILITY_MANAGER->ability_anm  (= [0x4cf298]+0x0c)
40f157: call  0x489320             ; AnmManager__allocate_new_vm
40f161: push  0x1e                 ; ← 这里传的是 SE id，不是脚本号（见下）
40f163: push  esi                  ; esi = 新 VM
40f164: call  0x407420             ; AnmManager__sub_407420(anm?, vm, script) —— 按脚本号克隆模板
40f172: and   ecx,0xffdfffff
40f178: mov   [esi+0x18],0xd       ; layer = 13
40f17f: or    ecx,0x101000
40f1a6: movq  [esi+0x5f0],xmm0     ; entity_pos = 卡记下的玩家坐标
40f1c4: call  0x478580             ; AnmVm__run（跑一帧）
40f1d8: call  0x488350             ; AnmManager__insert_in_world_list_back → 返回 id
40f1f9: mov   [edi+0x1c],esi       ; 存 id
40f22c: movss xmm2,[ebx] ; push 0x1e ; call 0x476c70   ; 起手音 id 0x1e = se_graze.wav（§1.5）
40f237: movd  xmm0,[edi+0x48]      ; 0x960 = 2400 = 充能（不是脚本号！）
```

⚠ **脚本号不在这段里**。`0x407420` 的脚本号要从卡表读：卡 42 每次发动都**重算充能**，
`[edi+0x48]` 在那之后是充能值 ⇒ 不能当脚本号。**小槌 VM 的脚本与 sprite 由下面两条独立证据钉死**：

| 项 | 值 | 一手 |
| --- | --- | --- |
| anm 文件 | 原生 **`ability.anm`**（`ABILITY_MANAGER+0x0c`，引用计数 +1 在 `0x40f151`） | `0x40f133`–`0x40f151` |
| script 号 | **`0x1c` = 28** | `ability.anm` script28 正文与卡的行为逐条对上（下表） |
| sprite 号 | `set_sprite(vm, 0x29)` = **41** —— **只在建 VM 时写一次** | `0x40f161`（VM 版 instantiate 的 `+0x10` = sprite 实参） |
| 存活 | **≈ 29 帧**（脚本 `interruptLabel(1)` 分支：`+8` 后 `delete()`）；卡侧另在 `经过帧 > 0x28` 时兜底 `sub_488cf0` | script28 / `0x40f042` |
| 层 / 混合 / 类型 | `layer(13)`、`blendMode(0)`、`type(1)`、`zWriteDisable(1)`、`alpha(255)` | script28 `0x877`–`0x884` |
| 缩放 | `scale(0.0f, 2.8f)` + `scaleTime(20, 4, 1.0f, 1.0f)` ⇒ y 从 2.8 拉到 1.0，20 帧见 4 帧缓动 | script28 |
| 位置 | **脚本里没有 `pos()`** ⇒ 位置全由卡侧写 `entity_pos`（`vm+0x5f0` = `card+0x58`） | `0x40ef37` / `0x40f1a6` |
| 颜色 | 默认；只在 `interruptLabel(2)` 里 `color(0,255,255)` → `+3` → `color(255,255,255)` → `caseReturn()` | script28 |

script28（`ability.anm`，原文）逐条对上卡侧：

```
script script28 {
    layer(13); blendMode(0); type(1); alpha(255); zWriteDisable(1);
    scale(0.0f, 2.8f);
    scaleTime(20, 4, 1.0f, 1.0f);      ← 20 帧：对应 state 1 的「上升 20 帧」
offset100:
    sprite(sprite85);                  ← (0,160,48,47)
+6: sprite(sprite86);                  ← (48,160,48,47)
+6: sprite(sprite87);                  ← (96,160,48,47)
+6: jmp(offset100, 0);                 ← 三帧循环看，每 6 帧换一张
    stop();
    interruptLabel(2);                 ← 「蓄力完成」那一下：闪一下青
    color(0, 255, 255);
+3: color(255, 255, 255);
    caseReturn();
    interruptLabel(1);                 ← 「挥击」那一下 = 卡侧 0x40f03f 调 sub_488cf0(card+0x1c)
    scaleTime(8, 4, 2.0f, 2.0f);
    alphaTime(8, 0, 0);
+8: delete();
}
```

**逐帧量（把卡侧与脚本侧合起来）**：

| 帧（经过帧） | 小槌 y 偏移 | 半径 | 脚本状态 | anm sprite |
| --- | --- | --- | --- | --- |
| 0 | 玩家 y | — | `scale` 0→…、`scaleTime(20,4,1,1)` 起步 | 建 VM 时 `set_sprite(41)` = `(32,80,32,16)` |
| 1..20 | 每帧 −16 | — | 脚本还没到 `offset100`，停在第 0 帧的 scale 缓动上 | 41（静止） |
| 21..30 | 每帧 −84 | `30+4n` = 34..70 | 已过 20 帧 ⇒ 落到 `offset100` 的三帧循环 | 85 / 86 / 87（每 6 帧轮换） |
| 31..40 | 不动 | 不再清弹（`经过帧 ≥ 10`） | 仍在循环（`jmp(offset100, 0)` 无次数上限） | 85/86/87 |
| 41 | `sub_488cf0` 删 VM | — | `interruptLabel(1)` 或卡侧兜底删除 | `delete()` |

⚠ **证据强度分级**（作者自评，避免把「对齐」写成「读到」）：
- **直接一手**：两条 SE（`0x40f230` / `0x40efb7` 的 `push` 立即数）· 半径常量与 `mode=4` · 状态机与帧数 ·
  层 13（`0x40f178`）· 脚本 28 与 85/86/87 **这两个 sprite 号来自脚本正文自己**（不是我的推断）。
- **推断（🟡）**：`set_sprite` 那一格 = **sprite 41**（因为它是 `0x407420` 的第 3 个实参位，见 §5 #1）；
  以及「script 28 就是这张卡起的那一支」——依据有两条：① 时间线唯一吻合（只有它同时满足「20 帧缩放 + 之后接三帧循环 + 层 13」）；
  ② 它紧挨在 **script 29 `sprite(sprite106)`**（`0x40f161` 那个立即数所指的 sprite 106）**前面一条**，
  与「卡用相邻的 script/sprite 一对」的原生习惯一致。

### 1.5 音效（在 `engine/_shared/th18-sound-table.md` §7 里核过）

> ⚠ **表读取踩过的两个坑（都记在这里）**
> ① `0x4c9b80` 这类地址落在 `.data` 的**裸数据区**（raw `0xc1a00` 起）⇒ 必须按节映射读
>   （`节 raw off + (VA − 节 VA)`）。直接 `VA − 0x400000` 会读到别处（TH17 侧更狠：`.data` 尾部是 BSS，
>   那个错法直接读出一片全 0 —— 见 §2.2）。
> ② **行的下标就是 id**：`SoundManager` 构造（`0x476457`–`0x476472`）用 `cmp [eax],edx / inc edx / cmp edx,0x54`
>   给 84 行各配一个 `slot[i].cfgptr`，`play_sound` 再用 `movsx …,WORD PTR [id*0x14 + 0x4c9b8a]` 取该行的音量/优先级
>   ⇒ **`行地址 = 0x4c9b80 + id*0x14`**，`+0x00` 是**槽号**而不是 id。

| 时机 | SE id | wav | 表里音量/优先级 | 一手 |
| --- | --- | --- | --- | --- |
| 起手（小槌出生） | `0x1e` | **`se_graze.wav`** | −7 dB / 20 | `0x40f230`（`push 0x1e` + `call 0x476c70`） |
| 挥击（小槌到位） | `0x29` | **`se_ok00.wav`** | −3 dB / 100 | `0x40efb7`（`push 0x29` + `call 0x476c70`） |
| 挥击前**扫 12 槽找 `0x1e`**（命中就把该槽计数置 `-1` = 这一帧不重播） | `0x1e` | — | — | `0x40ef84`–`0x40efae`（`0x56ad9c` 池，`cmp ecx,0x1e`） |
| 声像 | `xmm2` = `card+0x58`（小槌的世界 x） | — | — | `0x40f22c` / `0x40efb2` |

**cfg 行直接读 exe 复核**（TH18 表基址 `0x4c9b80`、stride `0x14`，`id → wav = 名表[ 行.+0x04 ]`；
名表 = `0x4b47a0`（72 项），`名表[0] = 0x4b8904 = "se_plst00.wav"` 实读）：

| id | 行地址 | `+0x00`（槽号） | `+0x04`（wav 下标） | `+0x0a` lo/hi | 结论 |
| --- | --- | --- | --- | --- | --- |
| `0x1e` | `0x4c9dd8` | `43` | `22` | −7 / 20 | 名表[22] = `se_graze.wav` ✅ |
| `0x29` | `0x4c9eb4` | `8` | `8` | −3 / 100 | 名表[8] = `se_ok00.wav` ✅ |
| （对照）`0x2a` | `0x4c9ec8` | `30` | `30` | −3 / 100 | 名表[30] = `se_ch00.wav` —— **这才是 TH18 的「儲力/チャージ」音** |
| （对照）`0x37` | `0x4c9fcc` | `55` | `43` | −3 / 100 | 名表[43] = `se_ch02.wav` |

★ **勘误（本节第一版把这两条都写错了）**：卡 42 的两条 SE 是 **`se_graze` + `se_ok00`**，
不是 `se_ch00` + `se_kira00`。两作的 id→wav **不是同一套**（TH18 `0x1e` = `se_graze`，TH17 `0x1e` = `se_cat00`），
所以我先前写的「起手音天然跨作一致」**不成立**。
⚠ `0x40ef84` 那段的语义也要收窄：它**不是「掐掉正在响的那一声」**，而是 TH18 音效池的**通用去重**
（池里已有同 id ⇒ 把该槽计数置 `-1`；池空则新占一个槽并把计数设 `0x1e`=30）——
与 TH17 的 `0x4654f0`/`0x4655a0` 是同一套「同帧同 id 只响一次」机制。

## 2. TH17 妖梦（pl02）的蓄力

> TH17 的玩家 = pl00 灵梦 / pl01 魔理沙 / **pl02 妖梦**（`th17.dat` 里只有 pl00/pl01/pl02 三支 anm ⇒ 妖梦 = pl02）。

### 2.1 状态载体与阈值（一手）

| 项 | 值 | 一手 |
| --- | --- | --- |
| 状态机入口 | `Player::tick_shooting_state` **`th17:0x44a8d0`**（`edi` = `zPlayer*`）；门口 `0x44a8e9` 判 `[edi+0x18db0]==1`（本机是玩家），`0x44a8fa` 判 `ds:0x4b59f4==2`（**妖梦专属那一段**） | ExpHP `funcs.json` + 该址 |
| **蓄力计时器** | **`player+0x19084`**（int；`0` = 起手那一帧、`1..59` = 蓄力帧数、`60` = **正好蓄满那一帧**、`<0` = 松手后的收尾） | 全 exe 只有 14 个访问点（§2.4） |
| 它就是 zTimer 的 `current` | 结构基址 `player+0x19080`（`prev@+0x19080`、**`current@+0x19084`**、`current_f@+0x19088`）：`Timer::set_value`（`0x405bc0`）写 `[ecx]=n−1 / [ecx+4]=n / [ecx+8]=n`，而调用点传的 `ecx` 正是 `player+0x19080` | `0x44a959`+`0x44a961`、`0x44ab42`+`0x44ab56` |
| 初值 | `Player::initialize` `th17:0x446d40`：`mov [esi+0x19084],0`；`Player::die` 同址清零 `th17:0x448b4c` | 该址 |
| **阈值** | **`0x3c` = 60 帧**（`cmp [edi+0x19084],0x3c`） | `0x44aa2b`、`0x44aa89`、`0x44ab3b` |
| 每帧递增 | `Timer::increment(player+0x19080)` ⇒ `current(+0x19084) += 1` | `0x44aa01`、`0x44aaf4`、`0x44abe1`（三次调用点，`ecx` 都是 `player+0x19080`） |
| 松手后的**续期** | `Timer::set_value(player+0x19080, −40)`（`0x44a95f`+`0x44a961`、`0x44ab54`+`0x44ab56`） | 见下 |
| 起手音/循环音的**停**接口 | **`SoundManager__stop_sound` `th17:0x4655d0`**（stdcall(id) `ret 4`；两支实现：`id >= 0` 走 `0x526274` 那 0x50 个槽的链表（vtable `+0x24`/`+0x48`），`id < 0` 走 `0x52480c` 的 12 槽池 ⇒ **两支都是「把池里同 id 的条目清掉」**，不是播放） | `0x44aa72`、`0x44aa79`、`0x44ad00`、`0x44ad07` |
| 特效 VM 的删除 | `AnmManager::sub_476620(id)`（同一个 id 也走 `AnmManager::interrupt_tree` `th17:0x476510`） | `0x44aa84`、`0x44ad12` |

**状态机形状（五条分支，全读）**：

| 分支 | 入口 | 条件 | 干什么 | 一手 |
| --- | --- | --- | --- | --- |
| **A 起手** | `0x44a9a7` | `+0x19084 == 0`（本帧刚按下射击键） | ① `play_sound_at_position(0x1e, x=player+0x610)` ② 用 `ds:0x4b77d0` 的 anm 建一支 VM：`AnmLoaded | `0x44a9b7`–`0x44aa08` |
| **B 蓄力中** | `0x44aa01` | `+0x19084 == 0` 或 `> 0`（两条汇合） | ① `play_sound_at_position(0x1e, x)` ② `play_sound_at_position(0x37, x)` ③ `if (+0x19084 != 0x3c) return`（**没满就到此为止**） | `0x44aa0d`–`0x44aa32` |
| **C 正好蓄满（`== 60`）** | `0x44aa38` | `+0x19084 == 0x3c` | ① **停** `0x1e`（`0x44aa3a`）② `play_sound_at_position(0x37, x)`（`0x44aa49`）③ **`AnmManager::interrupt_tree(player+0x19094, 2)`**（`0x44aa56`）④ 返回 | `0x44aa38`–`0x44aa65` |
| **D 蓄力中/刚满（另一条路，`> 0`）** | `0x44aa66` | `+0x19084 > 0` | ① 停 `0x1e`、停 `0x37`（`0x44aa70` / `0x44aa77`）② `sub_476620(player+0x19094)` 删 VM、`+0x19094  | `0x44aa66`–`0x44ab17` |
| **E 松手后收尾（`< 0`）** | `0x44aaad` | `+0x19084 < 0` | ① 把「上一帧位置」与「当前玩家位」各写进两支 effect 的实体坐标（`0x44a0f0` 调两次）② `do_shooting(0x28 + charge, 0)` —— **`0x28` = 40**，把负的 `charge` 推回 0..39 当**射击组偏移** ③ `Timer::increment` ④ `if (+0x19084 < 0) return`，否则落到 F | `0x44aaad`–`0x44ab05` |
| **F 归零** | `0x44ab06` | 上一步推到了 `≥ 0` | `Timer::set_value(+0x19080, 0)`（**回到「本帧刚按下」态 ⇒ 可以马上再来一轮**） | `0x44ab06`–`0x44ab17` |
| **G 收尾** | `0x44ace4` | `+0x18db0 != 1` 或 `+0x19084 == 0` | 清 `+0x19040`/`+0x19044`、停 `0x1e`/`0x37`、删 VM、`+0x19084 = 0`、`+0x19080 = −1` | `0x44ace4`–`0x44ad5b` |

★ **`set_value(−40)` 就是 `+40` 的逆**（一手）：`Timer::set_value`（`0x405bc0`）写
`[ecx] = n−1`、`[ecx+4] = n`、`[ecx+8] = (float)n`，而 `ecx` = `player+0x19080` ⇒ `current(+0x19084) = n`。
于是 `−40` 让**恰好 40 帧后** `current` 回到 0（分支 F）⇒ 与分支 E 的 `+0x28`(=40) 精确咬合。
⚠ **修正**（本轮之前若有人读成「60 帧缓冲」是错的）：**40 帧**。

⚠ 一处**未解**：`0x44ab94` 把 `[edi+0x18dd0]` 存起来、`0x44aba4` 临时置 1、`0x44abdb` 再还原 ——
也就是「建/删 VM 那两行的前后，暂时打开了 `+0x18dd0` 这个位」，机制没读完 🟡（不影响取 SE：两处 `play_sound` 都在 `+0x19094` 操作之前）。

### 2.2 SE id + wav 名（TH17 表本体解出，一手）

⚠ **本节经历了一次自我订正，两个错都记在这里**（避免下次再踩）：
1. **表在 `.data` 里，读法必须按 PE 节映射**。第一版我用裸文件偏移（`VA − 0x400000`）读，
   而 TH17 的 `.data`（VA `0x4ad000`、vsz `0x7de34`）**只有前 `0x9c00` 字节在文件里**，其余是 BSS 风格的全 0
   ⇒ 那个读法落到全 0 区，我据此编出过一张**完全错的** id→wav 表。
   正确读法：`fileoff = 节 raw off + (VA − 节 VA)`（`.data`：`0xab800 + VA − 0x4ad000`）。
2. **行的下标就是 id，但字段列与我第一版写的完全不同**（详见下表）。

**① 表在哪 / 多少行（一手）**

| 证据 | 内容 |
| --- | --- |
| 载入循环 | `th17:0x464f4c`：`mov esi,0x4b2bf4` … `add esi,0x14` … `cmp esi,0x4b3234` ⇒ 第一行首 `0x4b2bf4`、终止行首 `0x4b3234`、stride `0x14` ⇒ **行数 81** |
| 播放/停止函数读的字段 | `0x4654f0` / `0x465450` / `0x4655a0`：`movsx esi/edi,WORD PTR [eax*4+0x4b2bfa]`（`eax = id*5`）⇒ 读的是 **`id*0x14 + 0x4b2bfa`** = **`0x4b2bd0 + id*0x14 + 0x0a`** ⇒ 该字段是这行的 **`+0x0a`** |
| ★ 陷阱 | **`0x4b2bf4` 不是第 0 行的行首**：`+0x00` 相对 `0x4b2bf4` 是 `+0x04`，而第 0 行（`id 0`）的行首是 **`th17:0x4b2bd0`**（`0x4b2bf4 − 0x24 = 0x4b2bd0`；该行 `+0x00 = 0xffffffff` …（明细见文末「附：表格长单元格搬出」） |

**② 行内字段（一手：`0x464f5e` 的载入 + `0x4654f0` 的读取 + 逐行实读闭合）**

行是 5 个 dword（`+0x00 / +0x04 / +0x08 / +0x0c / +0x10`），字段落在 word 上：

| 偏移 | 语义 | 一手 |
| --- | --- | --- |
| `+0x00`（word） | **wav 名表下标**（`id 0x1e → 12`、`id 0x37 → 0`） | `0x464f5e mov eax,[esi]` → `0x464f62 push [eax*4+0x4a1060]` |
| `+0x02`（word） | **音量**（有符号；实测 −500 / −600 / 0 ⇒ 除以 100 得 dB，量纲与 TH18 同） | 逐行实读（`id 0x1e → 0xfe0c`） |
| `+0x04` / `+0x06`（word） | 恒 0（未解） | 逐行实读 |
| `+0x08`（dword） | 常量 `1`（TH18 里对应位置在 `+0x10`） | 逐行实读 |
| **`+0x0a`**（word） | **优先级** | `0x46550d movsx edi,WORD PTR [eax*4+0x4b2bfa]`（`= 行 + 0x0a`）✅ |
| `+0x0c`（dword） | **槽号 / 池索引**（`id 0x1e → 42`、`id 0x37 → 53`；与 TH18 表的 `+0x00` 同角色） | 逐行实读 |
| `+0x10`（dword） | 另一个索引（`id 0x1e → 22`、`id 0x37 → 42`） | 逐行实读 |

**③ 名表**：`th17:0x4a1060`（**69 项**，每项 4 字节，值是**字符串的 VA**；`名表[0] = 0x4a3354 = "se_plst00.wav"`）。
⚠ 名表本身也踩同一个坑：它的**裸文件偏移是 `0xa0460`**（`0x4a1060 − 0x400000 − 0x400`），
要按节映射读；字符串本体在 `.rdata`（`0x99400 + VA − 0x49a000`）。
载入循环 `0x464f78` 的 `add esi,0x14 / add edi,0x18` 是「cfg 行 stride `0x14`、slot 记录 stride `0x18`」的独立佐证。

**④ 逐条（本次实读）**

| 时机 | SE id | wav | 行地址 | 音量 / 优先级 / 槽 | 一手 |
| --- | --- | --- | --- | --- | --- |
| **蓄力起手** | `0x1e` | **`se_cat00.wav`** | `th17:0x4b2e28`（wav 下标 **12**） | −5.00 dB / 0 / 42 | `0x44a9c7`（`push 0x1e` + `call 0x4654f0`，`xmm2` = `player+0x610`） |
| **蓄力中（每帧）** | `0x1e` | **`se_cat00.wav`** | 同上 | 同上 | `0x44aa17`（`call 0x4655a0`，`esi` = `player+0x610`） |
| **蓄力中（每帧，叠一层）** | `0x37` | **`se_release.wav`**（wav 下标 66） | `th17:0x4b301c` | +0.00 dB / 0 / 53 | `0x44aa26`（`call 0x4655a0`） |
| **蓄满那一帧** | `0x37` | **`se_release.wav`** | 同上 | 同上 | `0x44aa49`（`call 0x4654f0`）+ `0x44aa3a` 先**停** `0x1e` |
| **松手/被打断的收尾** | — | **停** `0x1e` 与 `0x37` | — | — | `0x44aa70` / `0x44aa77`（`call 0x4655d0`）；`0x44ad00` / `0x44ad07` 同 |
| （对照）TH17 的斩击音 | `0x2d` | `se_slash.wav` | `th17:0x4b2f54`（wav 下标 **29**） | −3.00 dB / 0 / 32 | 表本体实读 ✅（**不是**这段代码里的调用点，🟡） |

★ **两作的 id→wav 不是同一套**（一手对照）：TH18 `id 0x1e` = `se_graze.wav`、`id 0x2a` = `se_ch00.wav`；
TH17 `id 0x1e` = `se_cat00.wav`、`id 0x37` = `se_release.wav`。⇒ 复刻时**按 wav 名挑**，别按 id 抄。
两作的素材包里都有 `se_cat00.wav` / `se_release.wav` / `se_ch00.wav` / `se_ch02.wav` / `se_ok00.wav`。

⚠ **`0x4654f0` / `0x4655a0` 都是「一帧只播一次」的语义**（两支各管一个池：
`0x4655a0` 用 `th17:0x526274` 的 `0x18` 步长链表槽、`0x4654f0` 用 `th17:0x52480c` 的 12 槽池 /
计数 `th17:0x52483c`；池里已有同 id 就只刷声像、不重播）⇒ TH17 的「每帧都调」实际听感是**每帧起一声**
（短音叠成嗡鸣），不是长音循环。

⚠ 读这段代码时**别把 `0x4655d0` 当成「又播了一次」**。它的入口是 `test eax,eax / jns`：
- `id >= 0`（`0x4655db` 起）：以 `esi = 0x526274`、步长 `0x18` 遍历到 `0x5269f4`，
  对每个 `[esi] != 0` 的槽调 `vtable+0x24` / `vtable+0x48`，并在 `0x4655e2` 把 `[esi+0x14]` 清 0；
- `id < 0`（`0x465618` 起）：在 `0x52480c` 的 12 项表里找同 id / 空位，命中就把 `0x52483c` 那一格的计数写 `-1`。

两支**都没有任何播放动作** ⇒ 它就是**停**／清理。（与 TH18 卡 42 的 `0x40ef84` 那段手写槽扫描同语义。）

⇒ 结论：**蓄力期间确实有音**（`se_cat00` + `se_release` 每帧叠）；**蓄满没有任何新音**
（那一帧只是**停** `0x1e` + 重起 `0x37` + `interrupt_tree(…,2)`；⚠ 中断 2 在光环那一支做的是**放大 + 继续动**、不是淡出，见 §2.3 订正块）。
（若要给「蓄满」一个明确的报号音，TH17 里最近的同族一手是 `0x2d se_slash.wav`，🟡 但没有实证调用点。）

### 2.3 蓄力的动画 / 特效

> ★★ **2026-10-01 订正（P7 / 111 工单；`AUDIT.md` §BE-FI）**：**起手建的那支 VM 就是 `pl02.anm`
> script19/20**，不是本节下面写的 script30 —— 两条独立证据：
> ① `0x44a9cc` 那两行立即数就是 `0x13`/`0x14` = **19/20**（本节作者自己记的，只是没去读脚本正文）；
> ② **只有 script19/20 带 `interruptLabel(2)`**，而蓄满那一帧卡侧正是 `interrupt_tree(vm, 2)`
> （`0x44aa56`）——「满那一帧那一支确实有反应」这条只有 19/20 成立。两支都是
> `layer(13); type(0); resampleMode(1);` + **8 帧 sprite 动画（每帧 3 帧一换）**：
> script19 = sprite37..44、script20 = sprite45..52，都是 `(64×64)`（`pl03.png` y160/y224 两行与
> y288/y352 两行）⇒ **蓄力光环是「sprite 动画环」，不是 `drawPoly` 实心盘**；
> 它的中断 2 = `scale(1.2→1.5)` + `pos y(−8→−20)` + 从头开始继续 8 帧循环（**放大 + 继续动，不是闪**）。
> script30（下面那段 `drawPoly` 实心盘）**只有 `interruptLabel(1)`** ⇒ 它**收不到**蓄满那一帧的中断 2，
> 所以它不可能是「起手建的那支」。**下面这段 script30 的正文仍然有效**（它是 TH17 里另一支
> `drawPoly` 光环配方，本仓 277 的上一版就是抄它），只是**不能当成「蓄力光环本体」**引用。

**① 蓄力光环（`drawPoly` 多边形，不是 sprite）** —— 起手时 `0x44a9f4` 调 `AnmLoaded::create_4112b0`
（`th17:0x4112b0`，内联版 `AnmLoaded__instantiate_vm_to_world_list_back`），参数里 `[ebp+0xc]` = 脚本号、
由 `0x44a9cc` 的 `cmp ds:0x4b59f8,0` 二选一 —— **`0x14`（20）或 `0x13`（19）**（= script19/20，见上面的订正）。

**它的配方我在 TH17 `pl02.anm` script30 里找到了逐字同形的原文**（🟡 ~~但「script30 就是它」这一步是**内容对齐**，
不是读那个动态立即数得到的~~ ⇒ **2026-10-01 已证伪：它是另一支，真身是 script19/20**）：

```
script script30 {                      // TH17 pl02.anm（`thanm -l 17 pl02.anm -m v8.anmm`）
    sprite(-1);                        // ★ 不绑 sprite：靠 drawPoly 画多边形
    layer(5);
    drawPoly(1.0f, 32);                // 32 边形；第 1 参 = 1.0f（两作都写这个值，照抄别解释）
    alpha(64);
    blendMode(2);
    color(32, 224, 255);
    color2(128, 224, 255);
    alpha2(0);
    scaleTime(60, 1, 600.0f, 600.0f);  // ★ 60 帧从 1 倍长到 600 倍 —— 与蓄力阈值 60 帧同长
+40: // 40
    alpha2Time(20, 0, 64);
    colorTime(250, 0, 355, 255, 32);
    color2Time(250, 0, 255, 255, 32);
    alphaTime(250, 0, 128);
    alpha2Time(250, 0, 128);
    stop();
    interruptLabel(1);
    alphaTime(20, 0, 0);
+20: // 60
    delete();
}
```

**★ 逐帧量**：`0..40` 帧只有一条 `scaleTime(60, 1, 600, 600)`（线性、缓动 1）⇒ 半径 = `1 + (600−1)×t/60` 倍；
`alpha2` 从 0 线性到 64（`+0..20`）；`color` 从 `(32,224,255)` 缓到 `(355,255,32)`（250 帧，实际永远走不到）；
`interruptLabel(1)`（= 蓄满那一下 `interrupt_tree(id,2)`？**注意卡侧传的是 2**，脚本里只有 `1`）
⇒ 从 40 帧起 `alphaTime(20,0,0)` 淡出、60 帧 `delete()`。

**② TH18 里有一份几乎逐字相同的配方**（对我们是好消息：不用跨作搬贴图）：

```
script script49 {                      ; TH18 ability.anm（原生，entry 归属见下）
    sprite(-1); layer(6); drawPoly(1.0f, 32); alpha(1); blendMode(0);
    color(128, 32, 32); color2(255, 255, 255); alpha2(255);
    scaleTime(60, 1, 600.0f, 600.0f);  // ★ 同一条 600 倍 / 60 帧
    alpha2Time(60, 0, 0); alphaTime(60, 0, 0);
+60: delete();
}
```

⇒ **`drawPoly` 不消耗 sprite**（两作的蓄力光环都写 `sprite(-1)`）⇒ 「从哪个 anm 抠哪个 sprite」这一问
**对光环本身不成立**（没有 sprite 可抠）。要复刻它只有两条路，见 §3.4。

**③ sprite 方面的候选**（如果我们要做「实体小槌/斩击」而不是多边形）：

| 源 | sprite | 源矩形 | 出处 | 备注 |
| --- | --- | --- | --- | --- |
| TH18 原生 `ability.anm` entry0（`player/option/op00.png`，256×512） | **41** | `(32, 80, 32, 16)` | `set_sprite(0x29)` @ `0x40f161` | 卡 42 建 VM 时那一次 |
| 同上 | 85 / 86 / 87 | `(0,160,48,47)` / `(48,160,48,47)` / `(96,160,48,47)` | script28 三帧循环 | **小槌挥击的三帧**，可直接复用 |
| 同上 | 84 | `(224, 80, 32, 32)` | script27（同族另一支效果） | 备选 |
| 同上 | 106 | `(0, 0, 64, 64)` | entry4 `player/option/item09.png` | `drawPoly` 系脚本常用的 64×64 贴图，**能不能拿来当环要看实图** 🟡 |
| 同上 | 108 | `(1, 1, 254, 254)` | `ability.anm` 的某 entry（`sprite108`） | 🟡 归属 entry 未定（累计编号落在 entry5/6 边界） |

⚠ `drawPoly(1.0f, 32)` 的第 1 参到底是「UV 缩放」还是「半径」没核（thpages 的 `v8.anmm` 只给助记符），
⇒ **复刻光环时 `drawPoly` 的两个参数按 TH17/TH18 原文照抄**（都是 `1.0f, 32`），别自己解释。

### 2.4 「全 exe 只有哪些点碰蓄力字段」—— 否定结论的证据（按 `METHOD.md` 硬规矩 4 逐条回到指令边界）

`player+0x19084` 的 14 个访问点（`objdump`/平坦反汇编逐行确认，非字节串扫描）：

| 地址 | 指令 | 分类 |
| --- | --- | --- |
| `0x446d40` | `mov [esi+0x19084],0` | 初始化 |
| `0x448b4c` | `mov [edi+0x19084],0` | 死亡清零 |
| `0x44a92b` | `cmp [edi+0x19084],0` | 读（门） |
| `0x44a9a7` | `mov eax,[edi+0x19084]` | 读（起手判 0） |
| `0x44aa2b` | `cmp [edi+0x19084],0x3c` | 读（**阈值**） |
| `0x44aa66` | `mov eax,[edi+0x19084]` | 读 |
| `0x44aa89` | `cmp [edi+0x19084],0x3c` | 读（阈值） |
| `0x44aad1` | `mov eax,[edi+0x19084]` | 读 |
| `0x44aaf9` | `cmp [edi+0x19084],0` | 读 |
| `0x44ab18` | `mov eax,[edi+0x19084]` | 读 |
| `0x44ab3b` | `cmp [edi+0x19084],0x3c` | 读（阈值） |
| `0x44abb8` | `mov eax,[edi+0x19084]` | 读 |
| `0x44ace4` | `cmp [edi+0x19084],0` | 读 |
| `0x44ad3e` | `mov [edi+0x19084],0` | 写（清零） |

⇒ **除了「每帧 `Timer::increment`」这一条（`0x44aa01`，写在 `+0x19084` 上）之外，没有任何一处把蓄力值用去算伤害/速度/范围**
（`0x44aad1` 的 `+0x28` 加在 `do_shooting` 的第 2 参上，那是**子弹分组索引偏移**不是伤害；`do_shooting` 内部读 `+0x19084` 的地方还没逐行读完 🟡）。

**⇒ 结论**：TH17 妖梦的「蓄力」= **按着射击键 60 帧**（不是按 Shift），满是**做斩击**、**不消弹**。
⇒ 用户口径里「按住 Shift 蓄力 5s（300 帧）」**不是 TH17 的机制**，我们**只能借它的表现层**（音效 + 光环）。✅

## 3. 落地建议：我们这边要动什么

> **本节整节是 ❓ 自定**（除标注「复刻」的行）。按 `CLAUDE.md` 的六问，第 1 问（原版是什么）与第 3 问（TH18 有没有现成代码）
> 在 §1/§2 已经答完；下面的取舍全部要进 `TUNING.md`。

### 3.1 复刻 vs 自定 一览

| 元素 | 类别 | 一手出处 |
| --- | --- | --- |
| 蓄力起手音 = **`se_cat00.wav`** | **复刻** | TH17 `th17:0x44a9c7`（TH17 cfg 行 `0x4b2e28`、wav 下标 12）。⚠ **按 wav 名搬，别按 id 搬**：卡 42 的 `0x1e` 在 TH18 里是 `se_graze` |
| 蓄满/发动音 | **复刻（TH18 口径）** = `se_ok00.wav`（TH18 cfg 行 `0x4c9eb4`，卡 42 的挥击音）；**TH17 口径下「蓄满」不加新音**（那一帧只是停 `0x1e` + 重起 `0x37` + `interrupt_tree(…,2)`；该中断在光环那一支 = **放大 1.2→1.5 + 上移 + 继续动**） | `0x40efb7`（✅）· `th17:0x44aa38`–`0x44aa56`（✅） |
| 蓄力**中**的音 | **复刻** = TH17 叠两层：`se_cat00.wav`（cfg `0x1e`）+ `se_release.wav`（cfg `0x37`），**每帧各起一声**、定位播放（x = 玩家 x） | `th17:0x44aa17` / `0x44aa26` |
| 蓄满/松手时**停掉**起手音与循环音 | **复刻** | TH17 `0x44aa3a`/`0x44aa72`/`0x44ad00`（`0x4655d0`）；卡 42 `0x40ef84`（扫池把计数置 −1，同语义） |
| 清弹半径 = `30.0f + 4.0f×n`（34→70 px） | ❌ **不复刻**（那是小槌的 10 帧挥击，我们是「2×擦弹圈」） | `0x40efe8`–`0x40f005` |
| 清弹出点 mode = 4（撒道具） | ❌ **不复刻**（用户口径是「消除」，我们现有实现走 mode 0） | `0x40f005` / `signal.c` 的 `ce_cancel_radius(..., 0)` |
| 清弹发作时长 10 帧 | ❓ 自定（我们是一次性结算，用户没要求持续） | — |
| 光环动画（`drawPoly` 32 边形，60 帧长到 600 倍） | **复刻配方**（TH17 `pl02.anm` script30；TH18 `ability.anm` script49 基本同形） | §2.3 |
| 蓄力 300 帧 / 按 Shift / 松手清屏 | ❓ 自定（用户 2026-09-15 口径，不是原版） | `signal_core.c` 的 `SIGNAL_FLANDRE_CHARGE_FRAMES` |
| 「蓄力中每帧播循环音」的**节流**（原版是每帧都调 `play_sound`，靠 SoundManager 的 12 槽去重） | ❓ 自定：我们**不**每帧播，改成按帧间隔播（见 §3.3） | 取舍理由：TH17 的每帧重播依赖它自己的槽管理，我们不确定同一套语义 |

### 3.2 要从 anm 抠哪些 sprite

> **做法照 `mods/th18.v1.00a/card-expand/assets/ability/make_mof_art.py` 的文件头**（读 `thanm -l` 的 sprite 矩形表 →
> 在源 png 上按矩形裁 → 补进 `assets/ability/` 的图集 → 追加 `ORDER.txt` 的一行/一段）。

| 想要的东西 | 源 anm | 源 entry | 源 sprite | **源矩形** | 目标 token（建议） |
| --- | --- | --- | --- | --- | --- |
| 小槌（跟随体） | TH18 `ability.anm`（`local/th18.v1.00a/anm/ability/`，`player/option/op00.png`） | entry0 | 41 | `(32, 80, 32, 16)` | `FLAN_MALLET_IDLE` |
| 小槌挥击三帧 | 同上 | entry0 | 85 / 86 / 87 | `(0,160,48,47)` / `(48,160,48,47)` / `(96,160,48,47)` | `FLAN_MALLET_SWING_0/1/2` |
| 斩击/爆闪（备选） | 同上 | entry0 | 84 | `(224, 80, 32, 32)` | `FLAN_SLASH` |
| 光环（**如果**决定不用 `drawPoly`） | 同上 | entry4 | 106 | `(0, 0, 64, 64)` | `FLAN_CHARGE_RING` 🟡 先看图 |

⚠ **抠图前先实图确认**（`local/th18.v1.00a/anm/ability/player/option/op00.png` 已由 `thdat -x` 解出，只读一张 png 即可），
`drawPoly` 系那张 64×64 到底长什么样我这一轮没看（不是方块图就是环）⇒ 标 🟡。

### 3.3 要新增的 `assets/ability/scripts/` 脚本（**建议**，本轮没改任何文件）

编号口径：`ORDER.txt` 头两行写「第 k 个数据行 → entry (7+k)、sprite (109+k)」，
当前最后一个 sprite 是 **`SIGNAL_MUL` = sprite 430 / entry 52**（`native/anm_ids.h:652` 实证）
⇒ **下一个可用的 sprite 号 = 431、下一个新 entry = 53**（新开一个 `ATLAS` 段就是 entry 53 起，成员按段内顺序拿连续 sprite 号）。
⚠ 这条是**读 `ORDER.txt` + 生成的 `anm_ids.h` 现算的**，实施时以重新生成的 `anm_ids.h` 为准。

| 建议脚本 | 干什么 | 配方出处 | 备注 |
| --- | --- | --- | --- |
| `FLAN_CHARGE_RING`（建议接在现有脚本号之后，本轮末尾是 `scripts/` 里已有的最大号） | 蓄力光环：`sprite(-1); layer(13); drawPoly(1.0f, 32); blendMode(2);` + 按蓄力进度调 `scale` | **TH17 `pl02.anm` script30 / TH18 `ability.anm` script49** | ✅ 逐字照抄两参 `1.0f, 32`；**不写 `pos()`**（位置由卡侧 `entity_pos` 给，见 `engine/anm/th18/01` §3a） |
| `FLAN_CHARGE_DONE`（可选） | 蓄满那一下的闪白：`color(0,255,255)` → 3 帧 → `color(255,255,255)` | TH18 `ability.anm` script28 的 `interruptLabel(2)` 分支 | 对应卡侧的 `interrupt_tree(id, n)` |
| `FLAN_MALLET_*`（可选，只在决定搬小槌时才要） | 小槌跟随 + 挥击三帧 | TH18 `ability.anm` script28 | 层 13 / `blendMode(0)` / `type(1)` / `scale(0.0,2.8)`+`scaleTime(20,4,1,1)` |

⇒ 具体编号**不要**在文里写死（`ORDER.txt` 只许 append，建议在实施时用 `native/anm_ids.h` 重新生成后的名字引用，
脚本里写 `@FLAN_CHARGE_RING`）。

### 3.4 卡侧落点

| 落点 | 文件 / 函数 | 要干什么 | 类别 |
| --- | --- | --- | --- |
| 蓄力每帧 | `mods/th18.v1.00a/card-expand/native/cards/signal.c` 的 `signal_support_flandre()`（`signal. | ① 在 `signal_flandre_tick()` 返回 0（还在蓄）时按**帧间隔**起 `se_cat00`（复刻 TH17 的每帧音；TH17 是每帧都调、靠 Sound |  | 4`）顺手把 VM 删掉 | 复刻 + ❓自定 |
| 蓄力中每帧 | 同上 | 起/更新光环 VM：首次进蓄力时 `ce_anm_spawn(CE_ANM_ABILITY_RING, …, layer 13)`，每帧把 VM 推向玩家位（TH17 是 `+0x19098` 那对坐标 + `0x44ab84` 写回）；**不要**在脚本里写 `pos()` | 复刻配方 / ❓ 自定参数 |
| 蓄满 | `signal_core.c` 的 `signal_flandre_tick()`（`f->armed = 1` 那一行，`signal_core.c:693`） | 那一帧播 `se_ok00`（TH18 卡 42 的挥击音 `0x29`，**复刻**；若照 TH17 口径则**只停旧音不加新音**），并 `interrupt_tree(vm_id, 2)` 让环淡出 | 复刻 |
| 松手清屏那一段 | `signal.c` 里 `ce_cancel_radius(pos, r, SIGNAL_BOMB_CANCEL_MAX, 0)` 那一块（`signal.c:1429`–`1449`） | **不动半径与 mode**（我们的是「2×擦弹圈」，`mode 0` = 纯删除，用户口径）⇒ 只在这一段前后加：删 VM、停循环音、播清弹音（原版卡 42 是 `se_ok00`）。⚠ 若改成 `mode 4` 就会**撒道具**（那是卡 42 的语义，不是用户的） | ❓ 自定（保留现状） |
| 被打断（`interrupted`） | `signal.c:1418` 那一支 | 删 VM + 停音；`signal_flandre_reset()`（`signal.c:1571`）里补一句同样的清理 | 复刻（TH17 `0x44ace4` 那支就是「停音 + 删 VM + 归零」） |
| `on_delete` | `signal.c` 的卡删除路径 | ⚠ 按 `METHOD.md` 的「长持续效果的卡必须交代被删掉会怎样」：**新加的 VM id / 音效闩锁**都要在这里收干净（商店里 tick 停着，只有 `on_delete` 跑得到） | ❓ 自定（纪律要求） |

### 3.5 三个必须实跑确认的点（进 `NEXT.md`）

1. TH18 的 `ability.anm` 只有 **7 个 entry**（`thanm -l` 实测），我们追加的是 entry 53 起；
   `drawPoly` 在**世界层（layer 13）**下能不能正常画（原生 `drawPoly` 出现在 `layer(3)` / `layer(5)` / `layer(6)` / `layer(18)`）🟡。
2. `se_cat00.wav` 每帧起的听感（TH17 每帧调 `play_sound`，靠 SoundManager 12 槽去重；我们按帧间隔播）⇒ 听感属用户口径。
3. 蓄力 300 帧的光环要做多长（`scaleTime(60,1,600,600)` 是 60 帧的量；300 帧要重算曲线，**这是 ❓ 自定**）。

## 4. 复现命令

```bash
# --- 通用：按 PE 节映射读 exe（别用 VA-0x400000；TH17 的 .data 尾部是 BSS，会读出一片 0）---
python3 - <<'PY'
import struct
d=open('local/th18.v1.00a/th18.exe','rb').read()          # 换 th17 路径即可
pe=struct.unpack_from('<I',d,0x3c)[0]; nsec=struct.unpack_from('<H',d,pe+6)[0]
opt=pe+24; sectab=opt+struct.unpack_from('<H',d,pe+20)[0]
secs=[]
for i in range(nsec):
    o=sectab+i*40
    nm=d[o:o+8].rstrip(b'\0').decode()
    vsz,va,rsz,ro=struct.unpack_from('<IIII',d,o+8)
    secs.append((nm,0x400000+va,vsz,ro,rsz))
def foff(va):
    for nm,sva,vsz,ro,rsz in secs:
        if sva<=va<sva+rsz: return ro+(va-sva)
def cstr(va):
    o=foff(va); return d[o:d.index(b'\0',o)].decode('ascii','replace')
# TH18：id→wav   （cfg 0x4c9b80 + id*0x14 的 +0x04 是名表下标；名表 0x4b47a0，72 项）
NT=foff(0x4b47a0); names=[cstr(struct.unpack_from('<I',d,NT+4*i)[0]) for i in range(72)]
for sid in (0x1e,0x29,0x2a,0x37):
    row=0x4c9b80+sid*0x14
    wi=struct.unpack_from('<I',d,foff(row)+4)[0]
    lo,hi=struct.unpack_from('<HH',d,foff(row)+8)
    print(hex(sid), names[wi], (lo if lo<0x8000 else lo-0x10000)/100.0, hi)
PY

# --- TH18 卡 42 ---
sed -n '/^  40eed0:/,/^  40f0f0:/p' local/_scratch/th18.dis        # on_tick_2 全局
sed -n '/^  40f0f0:/,/^  40f280:/p' local/_scratch/th18.dis        # c_press
grep -n "40f00b:\|40f02a:\|40f161:\|40f230:\|40efb7:\|40ef84:" local/_scratch/th18.dis
sed -n '877,904p' local/th18.v1.00a/anm/ability/ability.anm.txt  # script28
grep -n "sprite41:\|sprite85:\|sprite86:\|sprite87:" local/th18.v1.00a/anm/ability/ability.anm.txt

# --- TH17 妖梦蓄力 ---
objdump -d -M intel --start-address=0x44a8d0 --stop-address=0x44ad70 local/th17.v1.00b/th17.exe
objdump -d -M intel --start-address=0x4654f0 --stop-address=0x4656a0 local/th17.v1.00b/th17.exe  # 播放/停止
grep -n "84 90 01 00" local/_scratch/th17/th17.asm      # 蓄力字段的全部访问点
#   cfg  = th17:0x4b2bd0（81 行 × 0x14；+0x00 word=wav 下标 / +0x02 word=音量(s16,÷100=dB)
#          / +0x0a word=优先级 / +0x0c dword=槽号）
#   ⚠ 必须按 PE 节映射读：fileoff = 节 raw off + (VA − 节 VA)（.data 是 0xab800 + VA − 0x4ad000）
#      裸 VA−0x400000 会落到 BSS 全 0 区（这就是本文 §2.2 第一次读错的原因）
#   名表 = th17:0x4a1060（69 项，值是字符串 VA；裸文件偏移 0xa0460，字符串本体在 .rdata）
./local/vendor/thtk/build/thdat/thdat -x 17 local/th17.v1.00b/th17.dat pl02.anm
./local/vendor/thtk/build/thanm/thanm -l 17 pl02.anm -m local/vendor/thpages/static/mapfile/v8.anmm > pl02.spec.txt
sed -n '/^script script30 {/,/^}/p' pl02.spec.txt
```

## 5. 还缺什么（明确未取到）

| # | 缺什么 | 试过哪些方法 | 影响 |
| --- | --- | --- | --- |
| 1 | 卡 42 建 VM 时 `0x40f161 push 0x1e` 的**语义**（脚本号还是 sprite） | 读了 `0x489320`/`0x407420`/`0x488350` 的入口与参数用法；Ghidra 工程锁被占（GUI/MCP/driver 三者互斥）没跑 pyghidra 反编译 | 🟡 但脚本 28 / sprite 41 已用**内容逐条对齐**另证，不影响落地 |
| 2 | ~~TH17 蓄力「起手建的那支 VM」的**真脚本号**~~ | **2026-10-01 结案**：立即数就是 **19/20**（见 §2.3 的订正块），且只有它们带 `interruptLabel(2)`（与 `0x44aa56` 对得上）| ✅ 不再缺；`ds:0x4b59f8` 的运行时值只决定 19 还是 20（两支持平，不影响任何结论）|
| 3 | TH17 cfg 行的 `+0x04`/`+0x06` 两个 word 的语义（恒 0）与 `+0x10` 那个索引的用途 | 逐行实读都是常量/序列，没找到读它们的代码（`find_imm_refs` 式扫未做） | 不影响取 SE |
| 4 | `drawPoly(1.0f, 32)` 两个参数的语义（UV 缩放？半径？） | 查了 `v8.anmm` 只有助记符；没反 `AnmVm__run` 的 opcode 604 分支（13086 字节，未逐段读） | 复刻时**照抄原值**即可绕开 |
| 5 | TH18 `ability.anm` entry4 `sprite106` 的**实图**（能不能当环） | 未看图（`thdat -x` 已解出 png，本轮没读） | 🟡 抠图前必须看一眼 |
| 6 | TH17 `set_value(-40)` 里 40 的理由 | 找过阈值常量表，没有旁证 | 不影响我们（我们不复刻续期语义） |

## 6. 一句话总结

**取到了**：卡 42 的 `on_tick_2`（`0x40eed0`）四段状态机、半径公式（`30.0 + 4.0×n`，`mode 4`）、
它的 anm（`ability.anm` script 0x1c=28、sprite 0x29=41、三帧 85/86/87、层 13、存活≈29 帧）与两条 SE
（`0x1e se_graze` / `0x29 se_ok00`，都在 TH18 音效表里核过）；
TH17 妖梦蓄力的字段（`th17:player+0x19084`）、阈值（60 帧）、**TH17 音效表本体**（cfg `th17:0x4b2bd0` + 名表 `th17:0x4a1060` ⇒
`0x1e se_cat00` / `0x37 se_release` 每帧叠、蓄满只停不加）、以及**光环动画的一手脚本**
（TH17 `pl02.anm` script30 ≈ TH18 原生 `ability.anm` script49）。

**还缺**：§5 那 6 条（都不挡落地；最要紧的是 1 和 2 的「动态立即数语义」，已用内容对齐旁证）。

**按这份文档实施需要新增/修改的文件清单**：

| 动作 | 文件 |
| --- | --- |
| 新增 | `mods/th18.v1.00a/card-expand/assets/ability/scripts/<新号>_flan_charge_ring.anm.txt`（配方抄 TH17 script30 / TH18 script49） |
| 修改（append 只许追加） | `mods/th18.v1.00a/card-expand/assets/ability/entries/ORDER.txt`（新 `ATLAS` 段，起点 sprite 431 / entry 53） |
| 新增（切图产物） | `mods/th18.v1.00a/card-expand/assets/ability/flan/*.png` + `assets/ability/make_flan_art.py`（若有 sprite 要抠） |
| 重新生成 | `mods/th18.v1.00a/card-expand/native/anm_ids.h`、`native/build/ability.anm`（跑 `assets/build_ability.py`） |
| 修改 | `mods/th18.v1.00a/card-expand/native/cards/signal.c`（`signal_support_flandre()`：音效 + VM 启停 + `on_delete` 清理） |
| 修改 | `mods/th18.v1.00a/card-expand/native/cards/signal_core.{c,h}`（`signal_flandre_t` 加「VM id / 音效闩锁 / 已播帧」字段） |
| 登记 | `mods/th18.v1.00a/card-expand/TUNING.md`（§3.1 里所有 ❓ 自定项）、`NEXT.md`（§3.5 的三条实跑验收）、`AUDIT.md` §BE（**动了 `assets/ability/scripts/**` 与 `ORDER.txt` = 共享件，必须登记**） |

## 附：表格长单元格搬出

> 2026-10-01：这些内容原先塞在表格单元格里（超过文档门 200 字符上限），**只做搬家、不改一字**；表格留摘要。

- **1**：① `card+0x58/5c/60 = player+0x620/624/628`（**每帧跟随玩家**）② `card+0x5c -= 16.0f`（`0x4b9288`）⇒ 小槌上升 ③ `vm = get_vm_with_id(card+0x1c)`（`0x488b40`）④ `vm+0x5f0/5f4/5f8 = card+0x58/5c/60`（**entity_pos**）⑤ `if (经过帧 > 0x14) { state=2; Timer__set_value(card+0x20, 0) }`
- **2**：① `card+0x5c -= 84.0f`（`0x4b92f0`）② `if (经过帧 > 0xa)`：`state=3` + **扫音效池 `0x56ad9c` 找 `0x1e`**（`cmp ecx,0x1e`；池里有就占它、计数置 `-1`，池空则新占一槽、计数 `0x1e`）③ `SoundManager__play_sound_at_position(0x29, x=card+0x58)`（`0x476c70`）④ `Timer__set_value(card+0x20, 0)`
- **★ 陷阱**：**`0x4b2bf4` 不是第 0 行的行首**：`+0x00` 相对 `0x4b2bf4` 是 `+0x04`，而第 0 行（`id 0`）的行首是 **`th17:0x4b2bd0`**（`0x4b2bf4 − 0x24 = 0x4b2bd0`；该行 `+0x00 = 0xffffffff` = 空行，正好是 `id 0` 没被用的那一行）。<br>⇒ **`行地址 = 0x4b2bd0 + id × 0x14`**，本文所有行地址都按这条算

## 附：表格长单元格搬出

> 2026-10-01：下列表格行的单元格超过文档门 200 字符上限，**只做搬家、不改一字**（表格里留每格 <=90 字符的摘要）。

- **A 起手** ｜ `0x44a9a7` ｜ `+0x19084 == 0`（本帧刚按下射击键） ｜ ① `play_sound_at_position(0x1e, x=player+0x610)` ② 用 `ds:0x4b77d0` 的 anm 建一支 VM：`AnmLoaded::create_4112b0`（脚本号 `0x14`/`0x13` 按 `ds:0x4b59f8` 二选一，见 §2.3）→ `id` 存 **`player+0x19094`** ③ `Timer::increment` ｜ `0x44a9b7`–`0x44aa08`
- **D 蓄力中/刚满（另一条路，`> 0`）** ｜ `0x44aa66` ｜ `+0x19084 > 0` ｜ ① 停 `0x1e`、停 `0x37`（`0x44aa70` / `0x44aa77`）② `sub_476620(player+0x19094)` 删 VM、`+0x19094 = 0` ③ `if (+0x19084 < 60)` **`Timer::set_value(+0x19080, 0)`**（清零重来）；否则 `Timer::set_value(+0x19080, −40)`（**续期**） ｜ `0x44aa66`–`0x44ab17`
- 蓄力每帧 ｜ `mods/th18.v1.00a/card-expand/native/cards/signal.c` 的 `signal_support_flandre()`（`signal.c:1401`） ｜ ① 在 `signal_flandre_tick()` 返回 0（还在蓄）时按**帧间隔**起 `se_cat00`（复刻 TH17 的每帧音；TH17 是每帧都调、靠 SoundManager 12 槽去重，我们的节流是 ❓ 自定）；② 起手那一帧（`charge` 由 0 → 1）播 `se_cat00`（**复刻**）并记「起手音已播」闩锁，打断/松手时**停掉**它；③ 依赖现有 `interrupted` 判断（`pstate == 2 ｜  ｜ 4`）顺手把 VM 删掉 ｜ 复刻 + ❓自定
