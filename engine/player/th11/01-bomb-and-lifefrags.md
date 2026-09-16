# TH11 灵击与残机碎片 —— `BOMB_COST` 门槛、矩形伤害源、清弹与「5 片 = 1 命」

> **版本**：TH11 v1.00a（`th11.exe`，688,128 B，imagebase `0x400000`）。本文裸地址默认属该版本；引用其他版本须写成 `th10:0x…`。
> **方法**：一手平坦反汇编（产物 `local/_scratch/th11_full.asm`）+ `thdat -x 11` 解出的 6 个 `pl*.sht` 记录直读 +
> 常量按节区 VMA 直读原始字节 + ExpHP 结构 `zPlayerDamageSource` / `zGlobals` 交叉。
> **可信度**：门槛/消耗/伤害源原语/各 bomb 三元组/清弹/命碎链 ✅；
> 「`frames` 与 `half_h` 谁排第 3」靠 ExpHP 的 `__int_arg_3_eg_50` 单源佐证 ⇒ 🟡；震屏 ❓倾向否定。

## 0. 一句话结论

| 项 | 一手 |
| --- | --- |
| 门槛 | `CURRENT_POWER / BOMB_COST != 0`（**不是** TH10 的「≥1.00」），两个分支（通常 / 决死）共用 |
| 消耗 | `CURRENT_POWER -= BOMB_COST`（`0x4311f1` / `0x43129d`）；裸 `sub`，**没有下限钳位**（❓ 未穷举全部写点） |
| `BOMB_COST` 来源 | 自机文件 `pl*.sht` 记录 `+0x24`；`MAX_POWER = rec[+0x20] × rec[+0x24]`（`0x42fa69`） |
| 「扣 1.00 火力」 | 🟡 **只在 5/6 个自机上成立**：5 个自机 `+0x24 = 20`（= 1.00），**魔理沙A 是 12（= 0.60）** |
| 伤害 | 走 `Player::sub_433a90_creates_damage_source`（矩形伤害源 × 存活帧数），**全 exe 只有 4 条路径** |
| 清弹 | 每个 bomb tick 都调 `0x424fe0`（消弹圈） |
| 震屏 | ❓ **倾向否定**：bomb 的 4 条路径都不触碰相机/屏幕偏移字段，也没有 `(时间, 起, 止)` 三参原语 |
| 命碎 | 载体是**残机** `0x4a5718`（钳 9）与**碎片计数** `0x4a571c`（0..4）；**5 片 = 1 命**（`0x4209b0` 里兑换） |

## 1. 门槛与消耗

```
; 通常（存活）分支 0x4311cd–0x4311f9
4311b5: cmp DWORD PTR [eax+0x4438],0x0   ; Gui 对话 VM 忙 → 不干
4311c7: cmp DWORD PTR [eax+0x3c],0x0     ; Bomb 进行中 → 不干
4311cd: mov eax,ds:0x4a56e8              ; ★ CURRENT_POWER
4311d3: idiv DWORD PTR ds:0x4a574c       ; ★ ÷ BOMB_COST
4311d9/4311db: test eax,eax ; je 0x4311fe ; 商 == 0 ⇒ 不可发动
4311dd: test BYTE PTR ds:0x4c93c0,0x2    ; ★ X 键（INPUT = 0x4c93c0）
4311e6: call 0x406510                    ; Player::do_bomb（按自机派发到 6 个 Bomb::start）
4311f1: sub DWORD PTR ds:0x4a56e8,edx    ; ★ −= BOMB_COST
4311f9: call 0x432cc0                    ; repopulate_options
---
; 决死（被弹窗口内）分支 0x431265–0x4312b2：同一个 idiv、同一个 −BOMB_COST（0x43129d）
431286: push 0x3c ; Timer::set(player+0x944, 60)
4312aa: mov DWORD PTR [esi+0x928],0x1    ; player_state = 1
```

⇒ **决死分支没有额外代价**（TH11 没有信仰，也就没有 TH10 那条「−3000 信仰」）。

### 1.1 `BOMB_COST` / `MAX_POWER`：`pl*.sht` 记录的两个字段

```
42fa66: mov edx,[esi+0x24] ; imul edx,[esi+0x20] ; mov ds:0x4a5748,edx   ; MAX_POWER = +0x20 × +0x24
42fa73: mov eax,[ebp+0x92c] ; mov ecx,[eax+0x24] ; mov ds:0x4a574c,ecx   ; BOMB_COST = +0x24
```

| 自机 | `+0x20` | `+0x24` | `MAX_POWER` | 显示 |
| --- | --- | --- | --- | --- |
| `pl00a/b/c.sht`（灵梦 A/B/C） | 4 | 20 | 80 | 4.00 |
| `pl01a.sht`（魔理沙 A） | **8** | **12** | 96 | **4.80** |
| `pl01b/c.sht`（魔理沙 B/C） | 4 | 20 | 80 | 4.00 |

直读复现：`thdat -x 11 local/th11.v1.00a/th11.dat -C /tmp/th11x` 后
`struct.unpack_from('<2I', open('pl00a.sht','rb').read(), 0x20)` → `(4, 20)`。

### 1.2 「1.00 火力 = 20 单位」（HUD 一手）

```
41cbfe: mov eax,ds:0x4a56e8 ; 41cc1d: idiv ds:0x4a574c      ; 商 → 格式串 0x494180 "%d."
41cc7a: mov eax,edx (余数) ; imul eax,eax,0x64 ; idiv edi(=BOMB_COST)   ; → 格式串 0x494184 "%.2d"
41cca1: mov eax,ds:0x4a5748 ; … idiv ds:0x4a574c ; 格式串 0x49418c "/%d."
```

⇒ `CHARA_POWER = 100 × POWER / BOMB_COST`，小数位上限 `.95`（因为余数 ≤ `BOMB_COST−1`）；
**`BOMB_COST` 数值上就是「1.00 火力」** ⇒ 门槛 `POWER/BOMB_COST ≥ 1` 就是「显示火力 ≥ 1.00」，
只有**魔理沙A** 的消耗是 12/20 = **−0.60 显示火力**。

## 2. 伤害源原语 `0x433a90`

**调用约定**（一手）：`eax = player`，5 个栈参 `(&pos, dmg_per_frame, frames, half_w, half_h)` —— **第一个 push 是 `&pos`**。

| 参数 | 落点 | 证据 |
| --- | --- | --- |
| `&pos` | `slot+0x18/0x1c/0x20` | `433ad6: mov eax,DWORD PTR [ebp+0x0]`（只在「第一参是结构指针」解释下成立） |
| `dmg_per_frame` | `slot+0x00`（float） | `433ae4: fstp DWORD PTR [esi]` |
| `frames` | `slot+0x50` / `+0x54`(float 副本) / `+0x4c`(倒计时) | `433b1b: mov eax,[esp+0x18]` → `433b23: mov [esi+0x50],eax` |
| `half_w` | `slot+0x04`（float） | `433aed: fstp DWORD PTR [esi+0x4]` |
| `half_h` | `slot+0x60` | `433b2d: mov eax,[esp+0x1c]` → `433b31: mov [esi+0x60],eax` |
| 结构自带常量 | `slot+0x68` = `0xf423f`(999999) / `slot+0x6c` = `4` | `433b3b` / `433b42`，与 ExpHP `__field_68_init_999999` / `__field_6c_init_4` 逐字吻合 |

槽：`player+0x7c9c` 起、**stride `0x74`、32 槽**（`433aaa: cmp eax,0x20`）。
🟡 「`[esp+0x18]` = frames、`[esp+0x1c]` = half_h」这两条语义只靠 ExpHP 把 `+0x60` 命名成
`__int_arg_3_eg_50`（典型值 50）佐证 —— 灵梦A 的 `push 0x32`(50) 确实落在 `+0x60`。

### 2.1 六个 bomb 的真实数值（全部一手）

| 自机 | Bomb 入口 | 伤害源建立点 | 每帧伤害 | 存活帧数 | 半宽 × 半高 | 吃满总伤害 |
| --- | --- | --- | --- | --- | --- | --- |
| 灵梦A | `0x45dd70` | `0x45dec0` | **32.0f** | **30** | 10.0f × 50 | **960** |
| 灵梦B / C | `0x407a40` / `0x407bb0` | `0x407923` | 32.0f | **75** | 20.0f × 30 | **2400** |
| 魔理沙A | `0x406920` | 不建伤害源（弹幕 + 碰撞回调） | 回调返回 **50**（`0x406ebf`）/ 特例 **8**（`0x406f77`） | 弹幕寿命 | 自身碰撞框 | — |
| 魔理沙B / C | `0x407130` / `0x407680` | `0x407923` | 32.0f | 75 | 20.0f × 30 | 2400 |
| ★ 被弹（复仇）bomb | `0x4314a9` | `0x4314dd` | 32.0f | **150** | 30.0f × 30 | **4800** |
| （对照）玩家子弹 | — | `0x434b3f` | 32.0f | 剩余子弹数/3 | 16 × 13 | 不是 bomb |

常量原值：`0x497ef0` = 32.0f（每帧伤害）、`0x4984c8` = 20.0f、`0x49822c` = 10.0f、`0x497ff0` = 30.0f、`0x498258` = 1.4f。
**全 exe `call 0x433a90` 只有 4 处**（`0x407923`、`0x4314dd`、`0x434b3f`、`0x45dec0`）⇒ 没有第五条路径。
伤害消费侧 `EnemyData::take_damage_from_player` `0x4347f0` 在 `0x434b78` 起遍历这 32 个槽做 AABB 判定
⇒ **「矩形伤害源 × 存活帧数」这一对语义与 TH18 的 `ce_damage_rect(pos, 0, life, dmg, w, h)` 同形**。
⇒ **原版量级是「每帧 32 / 存活 30–75 帧 / 20×100…60×60 px」**，既不是全屏、也不是 2 帧。

### 2.2 清弹

每个 bomb tick 调 `0x424fe0`（消弹圈，参数含半径常量 `0x497ef0` = 32.0f）：`0x45df5a`（灵梦A）、
`0x406fdf` / `0x407002`（魔理沙A）；另有 `0x40af90`（子弹碰撞）。**消弹半径是按 bomb 定的常量**，
占位里的「全屏半径 4000.0f」在原版没有对应物。

## 3. 震屏：❓ 倾向否定

- 全 exe **没有** `(时间, 起始, 结束)` 三参一组的原语（对 `call 0x433a90` 与 `call 0x438440` 穷举后确认）。
- 存在的同族物是**相机/演出旋转**：`0x42aa2d`（`fld [edi+0x48]` → `call 0x45f9da`，`arg3 = 周期/时长`，
  `0x497fa0` = 1800.0f、`0x497fa4` = π/10）与三条世界变换（`0x42b6d0` / `0x4470da` / `0x44d66a`）。
  相机字段：`SUPERVISOR(0x4c3280)+0x434` / `+0x54c`；屏幕偏移写在 `ANM_MANAGER+0xb0`/`+0xb4`（`0x42a94c`/`0x42a958`）。
- bomb 的 4 条路径都**不触碰**这些字段 ⇒ 原版灵击没有震屏。
- ❓ 未取到：STD（`stageNN.std`）的指令表或 `0x42a970` 段的调用者清单 —— 这是「震屏到底是不是从 STD 走的」的下一步入手点。

## 4. 残机碎片（★ 本轮订正了两处研究稿的说法）

```
Globals::sub_420950   ; 1UP
420950: mov eax,ds:0x4a5718 ; inc eax ; cmp eax,0x9 ; mov ds:0x4a5718,eax
420960: 超过 9 → 钳 9         ; 否则 播音效 0x2c(se_extend) + call 0x41f010 + call 0x41a060（刷新 HUD）
Globals::sub_4209b0   ; ★ 残机碎片 +1，满 5 片换 1 命
4209b0: cmp DWORD PTR ds:0x4a5718,0x9 ; jl 0x4209c4 ; 否则 0x4a571c = 0 后返回（已满 9 命，碎片作废）
4209c4: mov eax,ds:0x4a571c ; inc eax ; cmp eax,0x5 ; mov ds:0x4a571c,eax
4209d4: 达到 5 → ds:0x4a571c = 0 ; call 0x420950（★ 换 1 命）
```

| 字段 | 地址 | 语义 | 一手 |
| --- | --- | --- | --- |
| `CURRENT_LIVES` | `0x4a5718` | 残机（**钳 9**） | `0x420950` / `0x4209b0` / 开局 `0x41fa38` |
| 残机碎片计数 | `0x4a571c` | 0..4，满 5 归零并 +1 命 | `0x4209b0`；HUD `Gui::update_lives` 第二参（`0x41a7c7`） |
| 残机图标 | — | sprite = **`7 + n`**（`0x41a0b6 add edx,0x7`），最多 9 格 | `Gui::update_lives` `0x41a060` |

### 4.1 两种发放路径（`0x420950` 全 exe 只有 3 个调用点）

| 路径 | 一手 | 说明 |
| --- | --- | --- |
| **捡到 type 5 道具**（= 命碎） | `0x423c3e: call 0x4209b0`（道具类型跳表 `0x423f00` 第 5 项） | 同分支 `0x4a5744`(RANK) += `0x100`，钳 ±`0x400` |
| 捡到 type 7 道具（= **1UP**） | `0x423d97: call 0x420950`（跳表第 7 项） | 直接 +1 命（钳 9），RANK += `0x100` |
| **分数跨阈值** | `0x41efd8`/`0x41efe3: cmp edx,DWORD PTR [eax*4+0x4a3b1c]` → `0x41f001: call 0x420950` + `0x41f006: inc ds:0x4a5750` | 表索引是「已发碎片计数」`0x4a5750`；EX 走另一张表 `0x4a3d60` |

**★ 订正**：

1. 有研究稿把 `0x4a5718` 叫「残机碎片」、`0x4a571c` 另算，并说「**10 个碎片 = 1 残机**」。
   **一手是 5**：`0x4209c9 inc eax` / `0x4209ca cmp eax,5` / `0x4209d4` 归零 + `call 0x420950`。
   `0x4a5718` 是**残机**（`420956: cmp eax,0x9`，`Gui::update_lives` 的实参也是它），碎片是 `0x4a571c`。
2. 该稿还把「捡到 type 7 = 命碎」当成主路径：**type 7 是 1UP**，命碎是 **type 5**（跳表原值见 [item/th11/01 §1](../../item/th11/01-item-types-and-popup.md)）。
3. `0x4a5744` **不是**「屏幕闪光/抖动标量」，它是 **RANK**（`GLOBALS+0x64`，钳 ±`0x400`）。

❌ **「no-miss 过关发碎片」不存在**：`0x420950`/`0x4209b0` 全文读过、`0x4a5718` 全交叉引用与 `0x4a571c` 全交叉引用核过，
**没有任何 no-miss / 阶段边界判据**；发放只有「捡道具」与「分数跨阈值」两条。
❓ 完整的**分数→碎片阈值表**只取到第 0 项（`0x4a3b1c` = `0x3b9aca00` = 10 亿）：该地址起的后续项已被颜色表
（`0xff808080` 一族）覆盖，EX 表 `0x4a3d60` 同理（第 0 项 10 亿、第 1 项 0）。

## 5. 结论表 / 还缺什么

| # | 断言 | 可信度 |
| --- | --- | --- |
| 1 | 门槛/消耗 = `POWER/BOMB_COST`，`BOMB_COST` 来自 `pl*.sht+0x24`（20 或 12），`MAX_POWER = +0x20×+0x24` | ✅ 一手 + 资源直读 |
| 2 | 「扣 1.00 火力」对 5/6 自机成立；**魔理沙A 是 −0.60** | ✅ 一手（`.sht` 字段） |
| 3 | 决死灵击同门槛同消耗，无额外代价 | ✅ 一手 |
| 4 | 伤害走 `0x433a90` 的矩形伤害源，`call` 只有 4 条路径；灵梦A 32/f×30f、半宽10×半高50 | ✅ 一手；参数序第 3/5 位 🟡 |
| 5 | 魔理沙A 走弹幕 + 碰撞回调（返回 50 / 特例 8），不建伤害源 | ✅ 一手；「何时返回 8」🟡 |
| 6 | 每个 bomb tick 调 `0x424fe0` 清弹 | ✅ 一手 |
| 7 | **灵击没有震屏**（倾向否定，缺 STD 一手） | ❓ |
| 8 | 命碎 = type 5 道具（`0x4209b0`），**5 片 = 1 命**（`0x420950`）；type 7 = 1UP | ✅ 一手（含三处订正） |
| 9 | 「no-miss 过关发碎片」不存在；发放只有道具与分数阈值两条 | ✅ 一手否定 |
| 10 | 分数阈值表只暴露第 0 项（10 亿） | ❓ |

**还缺什么**

- 🟡 `sub_433a90` 第 3/5 参的语义（frames vs half_h）单源（ExpHP `eg_50`）。
- 🟡 魔理沙A 碰撞回调返回 8 的触发条件。
- ❓ 灵击是否另有清弹半径的 per-bomb 常量表（本轮只核到 `0x497ef0` = 32.0f 被复用为参数，未逐 bomb 追参）。
- ❓ 完整分数→碎片阈值表（另一张表的位置，或实跑 dump `0x4a5750` 与 `Gui+0x43e4`）。
- ❓ `CURRENT_POWER` 的全部写点未穷举 ⇒ 「可扣到 0.00」是推测。

## 附录：复现命令

```bash
objdump -d -M intel --start-address=0x431070 --stop-address=0x431bc0 local/th11.v1.00a/th11.exe   # 门槛/消耗/决死
objdump -d -M intel --start-address=0x42f8a0 --stop-address=0x42fb00 local/th11.v1.00a/th11.exe   # BOMB_COST 来源
objdump -d -M intel --start-address=0x433a90 --stop-address=0x433f90 local/th11.v1.00a/th11.exe   # 伤害源原语
objdump -d -M intel --start-address=0x420950 --stop-address=0x420a00 local/th11.v1.00a/th11.exe   # 1UP / 碎片兑换
objdump -d -M intel --start-address=0x41ef80 --stop-address=0x41f020 local/th11.v1.00a/th11.exe   # 分数阈值发碎片
source tooling/env.sh
$THDAT -x 11 local/th11.v1.00a/th11.dat -C /tmp/th11x
python3 -c "import struct;d=open('/tmp/th11x/pl01a.sht','rb').read();print(struct.unpack('<2I',d[0x20:0x28]))"   # (8, 12)
```
