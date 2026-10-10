# 108 · 「TH18 发弹点 → 使魔壳」映射：纯静态核实（不实跑）

> **版本**：TH18 v1.00a（`th18.exe`，imagebase `0x400000`）。本文裸地址默认属该版本；TH08 侧一律写 `th08:0x…`。
> **方法（纯静态，只读）**：① `local/_scratch/thdis.py`（capstone 32 位）逐指令读 `local/th18.v1.00a/th18.exe`；
> ② 全 `.text` 线性反汇编扫「`props+0x08/0x0c/0x10` 的全部写点/读点」（`capstone`，`skipdata=True`）；
> ③ 直读 `.rdata` 里的浮点常量（`0x4b916c`/`0x4b9178`/`0x4b947c`）；④ 卡侧**只读**复核（`youkai.c`）。
> **未做**：未跑游戏、未用日志、未动任何实跑数据。本文取代/订正下列文档里与一手不符的表述：
> `HANDOFF-108-EMITTER-MODEL.md`（§4.3 的「活偏移恒 0」）、`HANDOFF-108-EMITTER-COVERAGE-TH18.md`（§4.1 的字段标签）。
> **可信度**：§1 的每条 ✅（逐指令 + 常量直读）；§3 的映射形式 ✅（由 §1 直接推出）；
> §4 的缺陷 D1/D2/D3 ✅（可静态证）；§6 的未定项 🟡。

## 0. 最短结论

1. **引擎的「发弹点」就是 `props[i]+0x08/+0x0c/+0x10` 三个 float（世界坐标）**，`etOn` 现算现写、
   真发弹函数**原样读走**（F2/F3）。除此之外没有任何每帧同步 —— 所以它是**上一发的快照**，不是活坐标。
2. **它的算法只有两条**（F2）：相对路 `= 敌人位置(此刻) + offsets[i]`；绝对路 `= origins[i].xy + offsets[i].xy`。
   两条都能在卡 tick 里**每帧重算**（`offsets`/`origins` 是敌人身上的活数组）⇒ **壳不需要靠缓存活着**。
3. **这三格还有第二种含义**（F4）：`laserNew` 把它的前三个实参写进同一格，`etCopy` 整块拷贝。
   ⇒ **把 `props+0x08` 当「绝对世界坐标」读是错的**（会读到 `512,512` 这类激光几何参数，或别人的旧快照）。
4. **缓存（latch）只对一种情形必要**：`etOffset(非零) → etOn → etOffset(0,0)` 同帧收回（F2/F4）。
   其余情形「每帧按 ② 重算」与缓存**逐值等价**。
5. **时序是定的**（F5）：卡 tick（`AbilityManager` 节点 `0x16`）**早于**敌人 ECL（`EnemyManager` 节点 `0x1b`）
   ⇒ 壳的位置天然滞后**一帧**；站点写下的缓存**下一帧**才被消费。
6. 现状代码与以上一手事实有 **3 处可静态证实的缺陷**（§4 的 D1/D2/D3）+ 2 处会误导后来者的标签错误（D5/D6）。

## 1. 一手事实（逐指令）

### 1.1 槽、偏移、原点、站点（✅）

| 项 | 值 | 一手证据 |
| --- | --- | --- |
| 数据基址 | etOn/etNew 的 `edi` = `&zEnemyData` = `zEnemy+0x122c` | etOn `0x433f9e lea ecx,[edi+0x44]` 取敌人位置；etNew `0x433de7 mov ecx,[ebp-0x570]` 与 edi 同值 |
| `props[i]` | `edi + i*0x488 + 0x598`（= `zEnemy+0x17c4`） | `0x434013 lea eax,[esi+0x598]`；`0x43404e mov [esi+edi+0x598],eax`；etNew `0x433e05 call 0x48f8f0`（memset 0x488）|
| `offsets[i]` | `edi + i*0xc + 0x4e58`（x/y/z 三 float，**步长 0xc**）| `0x433fb4/0x433fbd/0x433fc6 addss …, [edi+esi*4+0x4e58/0x4e5c/0x4e60]`（`esi = 3*slot`）|
| `origins[i]` | `edi + i*0xc + 0x4f18`（x/y/**flag**）| `0x433f46 movss xmm0,[edi+ecx*4+0x4f20]`（flag）；`0x433f6d/0x433f76` 取 x/y |
| 站点 `0x433ffa` | `movss xmm1,[edi+0x5144]` → `call 0x42cca0`（置保护半径）→ `0x434013` 取 `&props[i]` → `0x43401a call 0x427810` → `0x434028` 复位 | 槽号在 `0x433f3d mov [ebp-0x56c],eax`；`esi = edi + slot*0x488` |

> `0x413280`（相对路的「取位」函数）= **`mov eax,ecx; ret`**（恒等）⇒ `0x433fa1` 拿到的就是 `&data+0x44`，
> 即 `CE_ENEMY_POS_X`（`zEnemy+0x1270`）。**相对路加的是敌人本帧位置，没有插值/平滑。**

### 1.2 `etOn`（opcode 601，handler `0x433f34`）写发弹点的两条路（✅）

```
433f34  push 0 / mov ecx,edi / call 0x438aa0     ; slot = arg(0)
433f3d  mov [ebp-0x56c],eax                      ; 槽号
433f43  lea ecx,[eax+eax*2]
433f46  movss xmm0,[edi+ecx*4+0x4f20]            ; origins[slot].z
433f52  comiss xmm0,[0x4b916c]                   ; 0.9f
433f59  jbe 0x433f9e                             ; z <= 0.9 ⇒ 相对路
        --- 绝对路（0x433f5b..0x433f9c）---
433f5b  movss xmm2,[edi+esi*4+0x4e58]            ; offsets.x
433f64  movss xmm1,[edi+esi*4+0x4f1c]            ; origins.y
433f6d  addss xmm2,[edi+esi*4+0x4f18]            ; + origins.x
433f76  addss xmm1,[edi+esi*4+0x4e5c]            ; + offsets.y
433f8a  movq  [esi+0x5a0],xmm2                   ; props+0x08/+0x0c = origins.xy + offsets.xy
433f92  mov   [esi+0x5a8],0                      ; props+0x10 = 0
433f9c  jmp 0x433ffa
        --- 相对路（0x433f9e..0x433ff4）---
433f9e  lea ecx,[edi+0x44] / call 0x413280       ; eax = &敌人位置(float3)
433fa6  movss xmm2,[eax] ; xmm1,[eax+4] ; xmm0,[eax+8]
433fb4  addss xmm2,[edi+esi*4+0x4e58]            ; + offsets.x
433fbd  addss xmm1,[edi+esi*4+0x4e5c]            ; + offsets.y
433fc6  addss xmm0,[edi+esi*4+0x4e60]            ; + offsets.z
433fec  movq  [esi+0x5a0],xmm2                   ; props+0x08/+0x0c
433ff4  mov   [esi+0x5a8],eax                    ; props+0x10
```

⇒ **三种量必须分清**（卡侧此前把它们混了）：

| 量 | 坐标系 | 谁写 |
| --- | --- | --- |
| `props[i]+0x08/0x0c/0x10` | **世界** | `etOn`（上面两条路）/ `laserNew` / `etCopy` |
| `offsets[i]`（`0x4e58`）| **相对敌人** | `etOffset`（`0x434066` x/y）、`etOffsetRad`（`0x43409b` 极坐标→xy）、`etNew` 清零 |
| `origins[i]`（`0x4f18`）| **世界（绝对锚）** | `etOffsetAbs`（`0x43414e` x/y + flag） |

### 1.3 真发弹只读 `props`（✅）

- `0x427810`（`__stdcall`，1 个栈参 `&props[i]`）：`mov edi,[ebp+8]`；瞄准用
  `0x427841 subss xmm0,[edi+0xc]` / `0x427846 subss xmm1,[edi+8]`（= props 的 y/x 对自机做 atan2）；
  双层循环上限 = `word [edi+0x46e]`（`0x42789e`）与 `word [edi+0x46c]`（`0x4278b4`）；
  `0x4278c9 call 0x424fe0(props, i, j, angle)`。
- `0x424fe0`（单发）：`0x4251ea movq xmm0,[esi+8]` → `0x4251ef movq [edx],xmm0`（子弹 x/y）；
  `0x4251f3 mov eax,[esi+0x10]` → `0x4251f6 mov [edx+8],eax`（z）；
  `0x4251f9 movss xmm0,[esi+0x24]`（`etDist`）非零 ⇒ `0x425219 call 0x429bc0` 后加到 `[edx]`/`[edx+4]`。

⇒ **子弹的出生点 = `props+0x08/+0x0c/+0x10`（再沿角度推 `props+0x24` 像素）。** 这是「发弹点」的定义。

### 1.4 `props+0x08/0x0c/0x10` 的**全部**写点与读点（全 `.text` 扫描，✅）

| 地址 | 指令 | 谁 / 含义 |
| --- | --- | --- |
| `0x433f8a`+`0x433f92` | `movq [esi+0x5a0],xmm2` / `mov [esi+0x5a8],0` | `etOn`（op **601**）**绝对路** |
| `0x433fec`+`0x433ff4` | `movq [esi+0x5a0],xmm2` / `mov [esi+0x5a8],eax` | `etOn` **相对路**（= 真正的发弹点）|
| `0x434b5b`/`0x434b6d`/`0x434b7f` | `movss [esi+edi+0x5a0/0x5a4/0x5a8],xmm0` | **`laserNew`（op 700）**：三个**独立**实参 = **长度三元组**（见下）|
| `0x433ee1` | `rep movsd`（`ecx=0x122`）+ `0x433ef8`/`0x433f18` 拷 offsets/origins | **`etCopy`（op 614）**：整格拷贝（含这三格）|
| `0x433e05` | `memset(props[i],0,0x488)`（`call 0x48f8f0`）| **`etNew`（op 600）** 清零 |
| 其余命中 | `0x4074f2`/`0x41b6bd`/`0x452b17`/`0x484fc6`/`0x4886ab`/`0x489400` | 别的结构体（与 `zEnemyData` 无关）|

★ **`laserNew` 写进去的不是坐标，是「长度三元组」**（第二遍静态复核，逐指令 + 三个独立消费者）：
`arg1 → props+0x08` = **当前长度**（`obj+0x70`，消费者 `0x449438`/`0x449460`，逐帧向目标长 `0x44a66e`/`0x44a697`）、
`arg2 → props+0x0c` = **目标长度**（`obj+0x798`，`0x44a676`）、`arg3 → props+0x10` = **长度上限**（`obj+0x7a0`，`0x44a6f7`）、
`arg4 → props+0x448`（`0x434b8d`）。半径用法 = `0x44a74a` 取 `[ebx+0x70] × 角度 [ebx+0x6c]` → `call 0x452c90`。
⇒ `laserNew(slot, 当前长, 目标长, 上限, id)`；原生实参见到 `(512,512,0)` / `(0,256,0)` / `(16,160,0)` / `(-1,-1,-1)`（哨兵义 🟡）。

**读点（同一次扫描）**：子弹 `0x4251ea`；瞄准 `0x427841`/`0x427846`；`laserOn`（op 702）的
`0x4351f5`/`0x435206`/`0x435224`（**读出来当描述符的「长度」格，不是起点**）；`laserStOn`（op 703）的 `0x4353c2`/`0x4353f0`/`0x435587`。
⇒ **这三格是「弹出生点」与「激光长度」两义共用的同一格**；只有 `etOn` 之后、下一次改写之前，
它才等于「上一发的出生点」，而且**只在相对路里**才等于「敌人位置 + 活偏移」。

光束的真正起点**不是** `props+0x08`：`laserOn`（`0x435104`–`0x435196`）用与 `etOn` **同一个闭式**现算原点
（绝对路 `0x435144`/`0x43514c`、相对路 `0x435156 call 0x42d1e0` → `0x435186/0x43518e/0x435196`），写进**局部描述符**；
`0x448920` 把描述符交给各 mode 的 initialize（mode0 `0x4490d0`：`0x4493d2 movq xmm0,[ebx+0x788]` → `0x4493e0 movq [ebx+0x54]`）
⇒ **起点 = 描述符头三个 float = `laserOn` 现算的原点**。`call 0x448920` 全量 **10 处**
（`0x427270` / `0x435273` / `0x43545f` / `0x4355cb` / `0x435756` / `0x449f40` / `0x44a00b` / `0x44a0d9` / `0x44a1ac` / `0x451c5b`；
既有 HANDOFF 的 4 处只是「ECL 侧 4 个 case」）。

### 1.5 时序：卡 tick 早于 ECL（✅，注册点优先级）

| 节点 | 优先级 | 一手 |
| --- | --- | --- |
| `AbilityManager`（卡的 `on_tick_2` 在这里）| **`0x16`** | `0x408475 push 0x16` → `0x40847f call 0x401180` |
| `Player` | `0x17` | `0x45a8aa push 0x17` → `0x45a8b4 call 0x401180`（on_tick `0x45caa0`）|
| `EnemyManager`（敌人 ECL）| **`0x1b`** | `0x42dcce push 0x1b` → `0x42dcd8 call 0x401180` |

`register__on_tick`（`0x401180`）把节点按 `+0x00` **升序**插入（`0x4011d5 cmp [eax],esi` / `0x4011d9 jge`；
字段布局见 [`frame-loop.md`](../../../engine/_shared/frame-loop.md) §4.2）。
⇒ 一帧内的顺序 = **卡 tick(0x16) → 自机(0x17) → 敌人 ECL(0x1b)**。
⇒ ① 站点（在 ECL 里）写下的缓存，**下一帧**的卡 tick 才看到；
② 卡 tick 里读到的敌人位置是**本帧 ECL 跑之前**的值 ⇒ 壳若按「当前敌位」重算，天然滞后一帧（≤1 帧位移，量级 = 敌人每帧位移）。

### 1.6 `props[i]` 字段图（第二遍独立全量扫描，✅；订正本仓多处文档）

| 偏移 | 语义 | 唯一/主要写者 |
| --- | --- | --- |
| `+0x00` / `+0x04` | `type`（弹种表下标）/ `color` | `etSprite`（op 602 / `0x434032`）`0x43404e` / `0x43405a` |
| `+0x08`/`+0x0c`/`+0x10` | `offset_final`（**世界坐标**）| `etOn`；`laserNew` 时是**长度三元组** |
| `+0x14` / `+0x18` | **`angle_1` / `angle_2`** | `etAngle`（op 604 / `0x4341cc`）`0x4341ed` / `0x434200` |
| `+0x1c` / `+0x20` | `speed_1` / `speed_2`（线性速度两端，不是加速度）| `etSpeed`（op 605 / `0x43420e`）`0x434226` / `0x434238` |
| `+0x24` | `distance`（`etDist`）| **`etDist` 唯一**（op 627 / `0x434126`）`0x434140` |
| **`+0x46c` / `+0x46e`** | **`count_1` / `count_2`（两个 i16）** | **`etCount`（op 606 / `0x434246`）`0x434262` / `0x43426f`** |
| `+0x470` | `aim_type`（13 路角度合成跳表 `0x425adc`）| `etAim`（op 607）`0x434849`（只写低 16 位）|
| `+0x474` | `sound_flag` | **只有 `etNew`**（`0x433e4d` = `0x23`）|
| `+0x478` / `+0x47c` | 音效 id | `etSound`（op 608）`0x434872` / `0x43487e` |
| `+0x448` / `+0x458..0x468` | 激光参数 | `laserNew` / `laserTiming`（op 700/701）|

- `offsets[i] = +0x4e58 + i*0xc`（x/y 由 `etOffset` op 603 / `etOffsetRad` op 626 写；**`.z` 全 exe 无写者**）；
  `origins[i] = +0x4f18 + i*0xc`（`etOffsetAbs` op 628 写 xy；z 由 x 推：`x >= -990` ⇒ 1.0，否则 0）；`et_ex_index[i] = +0x4e18 + i*4`。
- **`etClear`/`etClearAll`/`etCancel`/`etCancel2`/`etClear2` 完全不碰 `props`**（打的是子弹/激光管理器 `0x4cf2bc`/`0x4cf3f4`）
  ⇒ **不能拿它们当「重置某个槽」**。`etCopy` 拷整 `props` + `offsets` + `origins`（**不拷** `et_ex_index`）。
- ★ **`etOnAuto` / `etDelay` 在 TH18 不存在**（`th18.eclm` 无此名；只在 `th08.eclm:105/107`；结构里也没有「连发周期」字段）。
  ⚠ [`HANDOFF-108-EMITTER-MODEL.md`](HANDOFF-108-EMITTER-MODEL.md) §1.2 把它们当作两作共有 —— 那是 **TH08 侧**的事实，TH18 侧不成立。
- ★ **`props+0x00 != 0` 的语义只到「本槽被 ECL 装了非零弹种」**：`type` 是弹种表下标（`0x425770 imul eax,[props],0x118`，
  表 `0x4c5f90`，**下标 0 合法**），且原生有 **5 处** `etSprite(slot,0,·)` 的槽照样由 `laserOn` 发激光
  （`0x4351a4` 把 type 抄进描述符，**没有 0 分支**）⇒ 它是**电平**、不是「真发弹点」的充分条件。
- ★ **静态字段图里没有任何「正在发弹」位**（TH18 无使能位）⇒ 判「这一槽此刻真的出弹」只能骑在
  `etOn` 站点 `0x433ffa` 的事件上；字段侧最多退到 `count_1 && count_2`（`etNew` 默认 `1/1` ⇒ 不区分波次）。

### 1.7 绝对模式的开关（✅，与引擎同一常量）

`etOffsetAbs`（`0x43414e`）：arg1→`origins[i].x`（`0x434173`）、arg2→`origins[i].y`（`0x43418a`）；
随后 `0x434193 comiss [0x4b947c], origins[i].x`（`0x4b947c` = **-990.0f**，直读 `.rdata`）
⇒ `x >= -990` 置 `origins[i].z = [0x4b9178]`（**1.0f**，`0x4341b6`），否则置 **0**（`0x4341a8`）。
`0x4b916c` 直读 = **0.9f** ⇒ 引擎自己的判据就是 `origins[i].z > 0.9f`（`0x433f52`）。

## 2. 由一手事实直接推出的「正确映射形式」

- **身份**：`(enemy_id, slot)`。槽号在原生 ECL 里不轮换（可变槽号都是子程序形参、调用方给常量），
  且 `etNew` 与同槽 `etSprite` 永远同帧（282/282，见
  [`engine/enemy/th18/07`](../../../engine/enemy/th18/07-emitter-slots-waves-and-phase-transitions.md) §2）⇒ 每帧采样看不到「未武装」态。
  ★ `enemy_id` 本身的字段偏移也复核过（✅）：`zEnemy+0x6830` —— 全 `.text` 有 **41 处**把它当 id
  读/比较（`0x4099b2`/`0x40b522`/`0x42d523`/`0x42ee34`…），且**写点 `0x42e3f6`**（`mov eax,[ecx+0x90]`
  → `mov [esi+0x6830],eax`，`ecx` = 敌人管理器）就是**分配**处 ⇒ 与 `engine.h` 的 `CE_ENEMY_ID` 一致，
  且 id 来自管理器的**单调计数器**（`mgr+0x90`）⇒ 不复用（定位缓存的键因此安全）。
- **位置**：壳应当站在「**引擎此刻若 `etOn(i)` 会算出的那个点**」。这有两个（且只有两个）可每帧重算的闭式：

| 模式 | 判据 | 目标点 |
| --- | --- | --- |
| 相对 | `origins[i].z <= 0.9f` | `敌人位置(本帧) + offsets[i]` |
| 绝对 | `origins[i].z > 0.9f` | `origins[i].xy + offsets[i].xy` |

- **缓存（latch）的必要性**：只有「`etOffset`/`etOffsetRad` 设好 → `etOn` → **同帧内**被改写/清零」这一族
  （`etOn` 用的偏移在 tick 采样时已经不存在）才需要缓存；缓存里必须同时记**模式**，
  否则绝对模式会被当成相对量 ⇒ 壳被母机拖着走（见 D2）。
- **不属于映射、但必须记账的表示力上限**：① 同帧同槽 N 次 `etOn`（最多 6 次/帧）只能是 1 壳 = 1 个代表点；
  ② 激光通道（`laserNew`/`laserOn`/`laserStOn`）与我们「打死壳 ⇒ 掐 `etOn`」的闸无交集；
  ③ `etDist` 让弹比 `props+0x08` 再远 `dist` px；④ `etCopy` 让两槽的 `props` 完全同值。

## 3. 现状代码逐条对账（`native/cards/youkai.c`，只读快照）

| # | 代码位置 | 现状 | 与一手事实的关系 |
| --- | --- | --- | --- |
| D1 | `youkai.c:3054`–`3057`（tick 的 `else` 回落）| 没缓存时 `wxy = props+0x08/+0x0c`（当世界坐标）| ❌ 错：这三格可能是**别人的旧快照**（母机已移动）/ `laserNew` 的几何参数 / `etCopy` 来的值 |
| D2 | `youkai.c:7113`–`7117`（站点写缓存）| 一律存 `props − 敌人位置`（相对量）| ❌ 错：绝对模式（`origins.z > 0.9`）存的是「绝对锚 − 敌人位置」⇒ 消费端 `敌人位置 + 它` 把壳拖着走 |
| D3 | `youkai.c:3158` `yk_emit_on_body(off3, org3)` | `off3` 传的是**世界发弹点**、`org3` **未初始化**（`(void)org3`，`youkai.c:3059`）| ❌ 且 UB：绝对判定读野值；「本体口」的真判据（活偏移 `(0,0)`）从没被执行到 |
| D4 | `youkai.c:3007`/`3043`–`3046` 注释 | 「`data+0x4e58` 在 TH18 **恒 0**」 | ❌ 与一手矛盾：`etOffsetRad`（`0x434107`/`0x434118`）就写这个数组，`etOn` 相对路（`0x433fb4`）就加它 |
| D5 | `youkai.c:3186`/`3209`/`3237`/`3269`/`3290` 的 `count_1/count_2` | 读 `props+0x14/+0x18` | ❌ 标签错：真计数在 `props+0x46c/+0x46e`（两个 i16，F6）|
| D6 | `youkai.c:2836` `COORD-SPAWN ... source_props` | 读的是**我们自己那只壳**的 props | ❌ 标签错：源应当是 `act->enemy_id` 的敌机（同函数上面 5 行已经这么做了）|

## 4. 修法（本轮**已落地并过闸**，逐条对应）

| # | 落笔（`native/`）| 内容 |
| --- | --- | --- |
| D1 | `cards/youkai.c` 的 tick 位置块 | 回落不再读 `props`，改调 **`yk_emit_world_pos(epos, offsets[i], origins[i], wxy)`**（`cards/youkai_core.c:363`，带单测）——与引擎两条路**逐条同形** |
| D2 | `cards/youkai.c` 站点 + 消费端 · `cards/youkai_core.c/.h` | 缓存加 `abs_mode`；站点按 `origins[i].z > 0.9f` 分流（绝对槽存**世界坐标**、相对槽存**相对量**）；消费端调新纯函数 **`yk_emit_latch_reconstruct()`** |
| D3 | `cards/youkai.c` | 删掉 tick 里那次 `yk_emit_on_body(off3, org3)`（`off3` 是世界点、`org3` **未初始化**=UB）；本体口由 `YK_EMIT_SHELL_MIN_DIST` 距离闸唯一裁决，台账格 `emit_body_skipped` 改在距离闸里按「与母机重合」计数 |
| D5/D6 | `cards/youkai.c` 的日志 | `count_1/count_2` 改读**真字段** `props+0x46c/+0x46e`（i16）；`COORD-SPAWN` 的 `source_props` 改读母机（原来读的是我们那只壳）；TRACK/POST/SAMPLE 的 `src=` 换成四值来源 `latch-abs`/`latch-rel`/`live-abs`/`live-rel` |
| 另 | `cards/youkai.c` | 顺手修一处**双层同函数调用**（`if (!seen()) { if (!seen()) ++count; log }`）⇒ `s_emit_bodyport` 原来**永不增长**；并删掉一处丢弃返回值的死调用 |

**单测**：`tests/test_youkai_core.c` 新增「定格值重建」6 条断言（相对跟随 / 绝对钉住 / 空指针安全），
`make check` 全绿（`63 checks passed`）。**判读**：实跑日志里位置来源会打 `src=latch-abs|latch-rel|live-abs|live-rel`
（旧版只有 `latch`/`props` 两值，且 `props` 那一支是错的）。

## 5. 为什么「每帧重算」不破坏 `etOffset→etOn→etOffset(0)` 那一族

站点**仍然写缓存**（这一条不变），只是缓存里存的是**按模式归一化**的量，且回落分支有了正确的闭式。
触发顺序（§1.5）不变：帧 N 的 ECL 写缓存 → 帧 N+1 的 tick 消费。⇒ 同帧收回那一族照旧被缓存救住；
而没有缓存（从没 `etOn` 过 / 缓存被 32 格环挤出 / 卡是中途装上的）的槽，
回落值从「陈旧的绝对快照 / 激光长度」变成「引擎此刻会用的那个点」——这正是 owner 报的
「壳冻住不跟母机动」那一类现象的正确解法。
★ 附带：`laserOn` 算激光起点用的是**同一个闭式**（§1.4）⇒ 激光槽（`type != 0` 那些）的壳现在也落在激光起点上。

## 6. 🟡 未定

| # | 未定 | 现状 |
| --- | --- | --- |
| 1 | `laserNew(slot, a, b, c, d)` 的 `-1,-1,-1` 哨兵 | 三格语义已定（当前长/目标长/上限，§1.4）；只有「-1 = 不改」这一步没追 |
| 2 | 缓存环（32 格）在密集波里的挤出率 | 只能实跑计数（`NOLATCH` 行）——**本轮的静态结论：挤出不再等于「冻结」**（回落到闭式重算） |
| 3 | 同帧多原点（最多 6 次/帧）的观感 | 表示力上限，已按「1 槽 1 壳 = 视觉代理」在册 |
| 4 | `props+0x480/+0x484`、`offsets[i].z` | 全 exe **无写者**（只有 `memset`/`etCopy` 会带过去）⇒ 语义未定，但不影响映射 |

## 7. 相关

- 建模与普查：[`HANDOFF-108-EMITTER-MODEL.md`](HANDOFF-108-EMITTER-MODEL.md) ·
  [`HANDOFF-108-EMITTER-COVERAGE.md`](HANDOFF-108-EMITTER-COVERAGE.md) ·
  [`HANDOFF-108-EMITTER-COVERAGE-TH18.md`](HANDOFF-108-EMITTER-COVERAGE-TH18.md)
- 引擎侧波次/阶段：[`engine/enemy/th18/07-emitter-slots-waves-and-phase-transitions.md`](../../../engine/enemy/th18/07-emitter-slots-waves-and-phase-transitions.md)
- 主循环优先级：[`engine/_shared/frame-loop.md`](../../../engine/_shared/frame-loop.md) §4
- 卡侧代码：[`native/cards/youkai.c`](native/cards/youkai.c) · 纯逻辑与单测：[`native/cards/youkai_core.c`](native/cards/youkai_core.c) ·
  [`native/tests/test_youkai_core.c`](native/tests/test_youkai_core.c)
