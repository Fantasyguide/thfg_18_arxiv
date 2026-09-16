# TH11 六个支援特技 —— 效果侧：缺口瞬移、全屏回收、形态切换与子机数组

> **版本**：TH11 v1.00a（`th11.exe`，688,128 B，imagebase `0x400000`）。本文裸地址默认属该版本；引用其他版本须写成 `th10:0x…`。
> **方法**：一手平坦反汇编（产物 `local/_scratch/th11_full.asm`）+ ExpHP `zPlayer`/`zPlayerOption` 结构交叉 +
> disp32 字节穷举（**只当候选发现器**，判定一律回到指令边界）。
> **可信度**：缺口瞬移全状态机、萃香/文的判据与效果、帕秋莉形态切换、子机数组、**荷取罩子全链（2026-09-16 补）** ✅；
> 「文的速度来自 SHT」的具体数值 🟡。
> **判据侧**（输入位、8 帧窗口、组合位的产生点）见 [`../../input/th11/01-special-detection.md`](../../input/th11/01-special-detection.md)。

## 0. 一句话结论

六条特技**不是一套机制**：组合号 = `3×CHARACTER(0x4a5710) + SUBSHOT(0x4a5714)` ∈ 0..5（`0x43064c`–`0x43064f`），
在 `Player::move` `0x430290` 里用**一条 if-else 链**（不是跳表）分派，每条各带自己的计数器字段；
面板数值（速度/火力/子机角距）全部来自 `pl*.sht`，**特技代码不在 SHT 里**。

| 组合 | 角色 | 效果（本文） | 判据（[input/th11/01](../../input/th11/01-special-detection.md)） |
| --- | --- | --- | --- |
| 0 | 霊夢×紫 | **缺口瞬移**：贴边后同方向 8 帧内再按 ⇒ 45 帧演出、位移 ±208 px | `+0x920`/`+0x924` 状态机 |
| 1 | 霊夢×萃香 | **全画面道具回收**（`state 1 → 3`） | `+0x8ba4` > 10 帧 |
| 2 | 霊夢×文 | 拖尾特效（速度本身来自 SHT） | `+0x8ba8` > 10 帧 |
| 3 | 魔理沙×爱丽丝 | 被动：子机数组本身就是 8 槽 | 无 |
| 4 | 魔理沙×帕秋莉 | **子机形态切换**（5 档循环） | `0x4c93cc & 0x400`（射击+↑） |
| 5 | 魔理沙×荷取 | **水泡罩子**：挡一次中弹（不扣命）+ 没被撞 ⇒ 退半档火力 | 无（在 Bomb 状态机里） |

## 1. 组合 0（霊夢×紫）缺口瞬移

函数 `Player::reimu_a_tick_gapping_state` `0x430e50`（每帧由 `0x43075e` 调用）；字段
`zPlayer.reimu_a_gapping_state` = `+0x920`、`reimu_a_frames_in_gapping_state` = `+0x924`。

| 状态 | 行为（一手） |
| --- | --- |
| 0 | 空闲；`0x430e50` 末尾只 `frames++` |
| 1 / 3 | 朝左 / 朝右的「第一次按下」已记录（由 `0x4307de` / `0x43080f` 在**自机已被钳到该侧边缘**时置位） |
| 2 / 4 | 等待第二次同方向（**窗口 `frames ≤ 8`**，`0x430ee5` / `0x430f8a`；超时 `0x431056` 归零；中途按上/下或 GUI/Boss 条件不满足也会取消） |
| 99 / 100 | 瞬移演出：`x += 0xD000`（左→右）或 `x += 0xffff3000`（右→左），并给 8 个 option 置「重生」位；`frames ≥ 0x2d`(45) 后归零 |

| 常量 | 值 | 一手 |
| --- | --- | --- |
| 二次窗口 | **8 帧** | `0x430ee5` / `0x430f8a` |
| 演出时长 | **45 帧** | `0x43104d cmp DWORD PTR [ebx+0x924],0x2d` |
| 瞬移位移 | **`0xD000` = ±208.0 px**（`pos__subpixel` 单位 **1/256 px**） | `0x430ff9` / `0x431012` |
| x 钳位 | **`±0x5c00` = ±92.0 px**（每帧无条件写） | `0x4307ea` / `0x43081f` |
| y 钳位 | `[0x1000, 0xd800]` = [16.0, 216.0] | `0x430829` 起 |
| 音效 | **`0x30`**（左 `3+0x2d`、右 `4+0x2c`） | `0x430f1c` / `0x430fc0` |
| 触发门的取消条件 | `INPUT & 0x9`（上/下）、GUI 对话 VM 非 0、`[ENEMY_MANAGER+0x70] != 0` | `0x430e79`–`0x430e99` |

⇒ 用户口径的**「松开射击键后」被证伪**（全函数唯一的输入读点是 `0x430e79` 的 `&0x9`）；
「快速按两下方向键」**成立但需加限定**：必须是**贴到边缘之后、同方向**的两次（不是任意位置双击）。
🟡 未解：`+0x888`(pos.x subpixel) 的原点归属 —— `±92` 与 `±208` 反推不出自洽的屏幕半宽（384/2 = 192），
但这两个数是**同一坐标系内的相对量**，复刻可直接用。

## 2. 组合 1（霊夢×萃香）全画面道具回收

```
0x43077a: cmp edx,1 ; jne            ; 组合号 == 1
0x43077e: cmp DWORD PTR [ebp+0x91c],0 ; attempted_direction == 0
0x430786: test BYTE PTR ds:0x4c93c0,0x9 ; jne 0x4307a5   ; 按了上/下 → 计数归零
0x43078f: add DWORD PTR [ebp+0x8ba4],1
0x430795: cmp DWORD PTR [ebp+0x8ba4],0xa ; jle  → 继续等
0x43079e: call 0x4245e0               ; ★ 回收全画面道具
```

`0x4245e0`（ExpHP 名 `ItemManager::reimu_b_autocollect`）：`eax = ITEM_MANAGER_PTR(0x4a8e90) + 0x478`、
循环 `0x896` = 2198 次、stride `0x478`：`if ([eax] == 1) { [eax] = 3 ; [eax+0xc] = sht_file+0x8 }`
⇒ **把落下中的道具（state 1）逐一切成 state 3（吸附玩家）**，目标 y 取 `sht_file+0x8`。
（`zItemManager.normal_items` = 池 +0x14、`zItem` 尺寸 `0x478` —— 与 ExpHP 结构一致。）

★ **到达频率 = 每帧，不是「触发一次」**（2026-09-16 复核，用来解释 TH18 卡侧的实跑反馈）：
`0x43078f` 的 `inc` 与 `0x430795` 的 `cmp …,0xa` **都不清计数器**；`+0x8ba4` 全 exe 只有三个访问点
（`0x43078f inc` / `0x430795 cmp` / `0x4307a5 mov …,0` = 按了上/下时归零）
⇒ 计数器一旦 > 10，就**每帧**都走到 `0x43079e` ⇒ 松手期间是**连续**全收。
★ 费用那一面：`0x4a56f4`（交信度）的写点穷举见
[`engine/faith/th11/02-koushin-signal.md`](../../faith/th11/02-koushin-signal.md) §1/§4 —— **7 处全是加值或清零**，
**没有任何一处是「特技花掉交信度」** ⇒ 一手这三条特技（紫的缺口 / 萃香的全收 / 文的拖尾）**都不收费**。

## 3. 组合 2（霊夢×文）「超高速」

判据字段 `+0x8ba8`（ExpHP 名 `reimu_c_frames_without_z_or_shift`）**全 exe 只有 3 个访问点**
（`0x430660 inc` / `0x430666 cmp …,0xa` / `0x4306c5 mov …,0x0`）⇒ **它从不参与速度计算**，只决定 `>10` 帧后是否播拖尾特效。

「超高速」的本体是 **SHT 数值**：

```
Player::initialize 0x42fa20: fld [sht+0x10] × 128.0 → player+0x890  normal_speed__subpixel
                   0x42fa38: fld [sht+0x14] × 128.0 → player+0x894  focus_speed__subpixel
                   0x42fa48: fld [sht+0x18] × 128.0 → player+0x898  normal_speed_over_sqrt2
                   0x42fa58: fld [sht+0x1c] × 128.0 → player+0x89c  focus_speed_over_sqrt2
Player::move 0x4306ce: 目标位移 × GAME_SPEED(0x4a7948) → player+0x8a0 (attempted_delta_pos)
```

⇒ **判据 = `INPUT & 0x9 == 0` 连续 >10 帧，且不含射击位**（三处访问全列出）——与「松开射击键」的口径不符。
🟡 「文的速度天生高」只到「速度字段来自 SHT」这一步：`th11.dat` 里的 `pl00c.sht` 数值本轮未取。

## 4. 组合 3（魔理沙×爱丽丝）「子机上限 8」

**不存在「上限常量」**：上限就是 `zPlayer.options[0x8]` 这个数组本身
（`+0x7570`、元素 `zPlayerOption` = `0xe4` 字节、已启用数量 = `+0x7c90`；`0x7570 + 8×0xe4 = 0x7c90` ✓）。

- 分配/回收在 `Player::repopulate_options` `0x432cc0`：模式 A 由 `CURRENT_POWER / POWER_PER_LEVEL` 决定数量
  （`0x432cee idiv ds:0x4a574c`），模式 B 直接发满 **8 槽**（`0x432e52 mov [esp+0x10],8`）。
- `+0x7c90` 的全部 12 个访问点**没有任何 8 之外的钳位** ⇒ 组合 3 与「能带几个子机」无关。
- 组合号只决定子机的 **sprite script**：跳表 `0x43365c`（`0x432dae jmp [eax*4+0x43365c]`）
  = 组合 0→`0x17`、1→`0x18`、2→`0x19`、3→`0x28`、4→`0x29`、5→`0x2a`。
- 🟡 「只有爱丽丝填满 8」的原因推测是**其它机体的 SHT 在满火力时只填 4 个**（SHT 未解，未证）。

## 5. 组合 4（魔理沙×帕秋莉）形态切换

```
0x4308fe: test DWORD PTR ds:0x4c93cc,0x400 ; je 0x43096a   ; ★ 组合位（射击+↑，见 input 篇）
0x43090a: mov eax,[ebp+0x8bac] ; inc eax ; cdq / mov ecx,5 / idiv ecx
0x430921: mov [ebp+0x8bac],edx        ; ★ formation = (formation+1) % 5
0x430929: lea edi,[ebp+0x7620]        ; option[0].__anm_ids[0]（zPlayerOption+0xb0）
0x430949: add eax,0x22                ; ★ anm script = 0x22 + formation（0x22..0x26）
0x43094c: call 0x455a00 ; 0x430957: mov [edi],edx    ; 写回 option 的 anm id
0x43096a–0x430b3a: 逐个 option 把目标角/距从 zPlayerOption+0x74 的
                   marisa_b_preferred_offsets_by_formation[formation] 拷进运行时目标字段（+0x6c/+0x70…）
```

⇒ 五档循环、**无帧冷却**（`0x4308fe` 是电平判定）；切换后立刻进入 8 帧「重放录制位」驱动（`0x436106`–`0x43613a`）。
🟡 「0 = 火 → 水 → 木 → 金 → 土」这层命名来自用户口径，**没有**从 `front.anm` script `0x22..0x26` 的贴图对出来。
❓ 长按（射击+↑）是否会每帧切一档：没找到冷却字段，也未实测。

## 6. 组合 5（魔理沙×荷取）：放 B 起一个**水泡罩子**，没被撞就退**半档火力**

**不是「不中弹则不算用过」这样一个标志位** —— 一手是一支**特效实体（水泡）+ 两条出路**。全链如下。

### 6.1 放 B 那一帧：`Bomb::start__marisa_c` `th11:0x407680`

| 地址 | 指令（原文） | 落点 / 语义 |
| --- | --- | --- |
| `th11:0x407681` | `fld ds:0x497ef0`(=32.0f) → `th11:0x40768d fstp [esi+0x490]` | 破罩消弹圈**初始半径** 32.0 |
| `th11:0x407699` | `fld ds:0x49822c`(=10.0f) → `th11:0x40769f fstp [esi+0x494]` | 该圈**每帧增量** 10.0 |
| `th11:0x407693`–`0x4076be` | `[bomb+0x484/+0x488/+0x48c]` ← `player+0x87c/0x880/0x884` | `zBomb.__center_pos` = 自机坐标 |
| `th11:0x4076c1`–`0x4076f9` | 初始化 `bomb+0x28` 那支 zTimer（`+0x2c` = current = 0） | **第二支 timer**（罩子计时器）；`bomb+0x14` 那支才是 bomb 时长 |
| `th11:0x4076e7` / `th11:0x4076ec` | `push 0x16` / `push 0x3a` | 起特效的栈参 = (**layer 22**, **script 58**) |
| `th11:0x407700` / `th11:0x407705` | `mov edx,[player+0x10]` / `call 0x455b10` | `player+0x10` = **已加载的角色 anm**（`pl01.anm`），脚本号是该文件全局下标 |
| `th11:0x40770a` | `mov edx,[eax]` → `[esi+0x40]` | `zBomb.__anm_id` = 那支 VM 的 id |
| `th11:0x407714` | `or DWORD PTR [eax+0x8bc4],0x4` | **bit2 = 「荷取罩子在」** |

### 6.2 那支特效 = `pl01.anm` **script58**（水泡，**920 帧**）

`thanm -l 11 … -m local/vendor/eclmap/anmmap/v4.anmm` 读出的正文（TH11 anm = v7，`case` 在 v8 写作 `interruptLabel`）：

```
script58 { layer(5); blendMode(1); type(1); sprite(sprite76);          // sprite76 = (1,1,254,254)
           angleVel(0,0,-0.05235988f);                                  // −3°/帧自转
           alpha(0); alphaTime(10,0,255); scale(0,0); scaleTime(20,0,1,1);
+20:       scaleTime(900, 0, 0.1f, 0.1f);
+900:      delete();                                                    // ★ 第 920 帧自删
           case(1); scaleTime(30,0,3.0f,3.0f); alphaTime(30,1,0);        // ★ 破罩动画
+30:       delete(); }
```

- 贴图在 `pl01.anm` **entry3**（`player/pl01/pl01c.png`，256×256）：整张就是一张**半透明青色水泡**
  （中心 `(0,84,76,118)`、边缘 `(0,162,147)`；`alpha > 32` 的像素占 68.5%）⇒ **是真贴图**，不是 `sprite(-1)+drawPoly` 那一族。
- **跟人**：`Bomb::on_tick__marisa_c` 每帧把 `bomb->center_pos` 抄成 `player.pos` 再
  `th11:0x407892 call 0x456430`（`AnmVm::set_entity_pos_plus_ecl_origin`，连同子 VM 一起搬）
  ⇒ 泡**锁在自机身上**（`th11:0x407866`–`0x407892`）。
- 副产物：每帧还写全局亮度色 `stage+0x2328`（`0xff808080` = ×1.0 → `0xff202020` = ×0.25，
  t = 0..30 渐暗、890..920 渐亮，`th11:0x407744`–`th11:0x40781e`）—— 它的**收尾点正好也在 920**，
  是「罩子寿命 = 920 帧」的独立佐证。

### 6.3 两条出路（都在 `Bomb::on_tick__marisa_c` `th11:0x407720` 里）

**出入口**：`th11:0x40772c mov eax,[edi+0x40]` → `th11:0x407737 call 0x4561e0`（按 id 取 VM）
⇒ `esi = VM 指针`；`esi == 0` 就是「**泡已经没了**」。

| 出路 | 一手 | 效果 |
| --- | --- | --- |
| **没被撞**（泡自己走到 920 帧自删） | `th11:0x407828 test esi,esi` / `jne` → 落 `th11:0x40782c` 那支 | 清 bit2（`th11:0x407837`）、SE **0x33**、**`collect_power(10)`**（`th11:0x40784d`）、重建子机（`th11:0x407852`） |
| **被撞**（bit2 清 **或** `player+0x928 ∈ {2,4}`） | `th11:0x407897` / `je 0x4078b0` / `th11:0x4078a0`–`0x4078ae` | 走 `th11:0x4078b0` 那支：**撤销这一下中弹**（`th11:0x4078c4 mov [player+0x928],1`）、SE **0x26**、建一个 75 帧伤害源（`th11:0x407923`） |

两条出路的**逐条地址与参数**：

- 没被撞：`th11:0x407837 and DWORD PTR [eax+0x8bc4],0xfffffffb`（清 bit2）→
  `th11:0x40784a lea eax,[esi+0xa]`（`esi` = 0 ⇒ **10**）→ `th11:0x40784d call 0x420a00`（`collect_power`，`0x4a56e8 += eax`，钳 `0x4a5748`）→ `th11:0x407852 call 0x432cc0`（重建子机）。
- 被撞：伤害源那一组 = `th11:0x40790c push 0x4b`(75 帧) / `th11:0x40790e push 0x1e`(半高 30) /
  `th11:0x407906 fld ds:0x4984c8`(半宽 20.0) / `th11:0x407919 fld ds:0x497ef0`(伤害 32.0) /
  `th11:0x407923 call 0x433a90`（中心 = `&bomb+0x484` = 自机位置）。

被撞之后的 30 帧（`th11:0x407928` 起，门 = `cmp [edi+0x2c],0x1e`）：每帧
`& [player+0x8bc4],~4` → `th11:0x40793d call 0x4070e0`（`anm_trigger_leave_by_id` = 让泡播 `case(1)` 破罩动画）
→ `th11:0x407942 call 0x4079c0`（消弹，半径取 `bomb+0x490`）→ `+0x490 += +0x494`（**先消弹后加** ⇒ 32, 42, … 322）
→ `th11:0x407961 push 0x28` / `th11:0x407963 call 0x406860`（= `set_iframes(40)`，**每帧刷**）；
`[bomb+0x2c] > 30` ⇒ 删泡 VM（`th11:0x407980 call 0x4562e0`）+ bomb 结束（返回 −1）。

**★ 量纲**：`collect_power` 的其它调用点是「满火力道具 = `MAX_POWER`」「小 P = 1」「大 P = `BOMB_COST`」
（`th11:0x423a95` / `th11:0x423b39` / `th11:0x423cfb`），而 `BOMB_COST` 是 per-shot 字段（`pl*.sht+0x24`）
= **20**（荷取 `pl01c.sht` = 20 = 1.00 火力）⇒ **退款 10 单位 = 半档 = 退回一半消耗**。

**★ 「不撞」有两条独立写点，规则同一条**：除了上面那条，`Bomb::destructor` `th11:0x406260` 里
再写了一遍（`th11:0x40628e cmp [bomb+0x3c],0` + `th11:0x4062a3 cmp edx,5` 机体门 ⇒
`th11:0x4062b1 lea eax,[ebp+0xa]` → `th11:0x4062b7 call 0x420a00`）⇒ 换关 / 退出时罩子还活着**也退**。

⇒ **结论**：一手**没有**「记一次 no miss / 发命碎」这种账（命碎那条路见
[`01-bomb-and-lifefrags.md`](01-bomb-and-lifefrags.md) §4，是 ✅ 一手否定）；
「用了灵击只要不中弹就不算使用」的**一手指纹 = 退半档火力**，判据 = **泡的 VM 还在不在**。

### 6.4 与其它机体的关系（同一个 Bomb 单例）

`Bomb::operator new` `th11:0x406440` 建 **0x4a0 字节单例**放 `BOMB_PTR ds:0x4a8d64`；
`Player::do_bomb` `th11:0x406510` 与 `Bomb::on_tick` `th11:0x406620` 各是一张 6 项跳表
（`3×CHARACTER + SUBSHOT`），荷取是第 6 项。**唯一不给持续无敌的机体就是荷取**：
全 exe 的 `call 0x406860`（`set_iframes`）只有 5 处，荷取那处（`th11:0x407963`）在**破罩之后**才跑
⇒ 罩子活着时是「**挡一次**」，不是无敌。另外 `[bomb+0x3c] != 0` 时 `do_bomb` 直接返回 −1
⇒ **罩子没结束前放不出第二发 B**（一手）。

## 7. 结论表 / 还缺什么

| # | 断言 | 可信度 |
| --- | --- | --- |
| 1 | 组合号 = `3×CHARACTER + SUBSHOT`，六条特技在 `Player::move` 里用 if-else 链分派 | ✅ 一手 |
| 2 | 缺口瞬移：8 帧窗口 / 45 帧演出 / ±208 px / x 钳 ±92 px / 音效 `0x30` / 状态字段 `0x920`·`0x924` | ✅ 一手（含 ExpHP 名双源） |
| 3 | 缺口**不要求松射击键**（全函数唯一的输入读点是 `&0x9`） | ✅ 一手否定 |
| 4 | 萃香：`+0x8ba4 > 10` 帧 ⇒ `0x4245e0` 把整池落下道具切成 state 3 | ✅ 一手 |
| 5 | 文：`+0x8ba8` 只有 3 个访问点、只驱动特效；速度来自 SHT | ✅ 判据；🟡 SHT 数值 |
| 6 | 「子机上限 8」= 数组长度，不存在上限常量；组合号只选 sprite script | ✅ 一手 |
| 7 | 帕秋莉：`formation = (n+1)%5`、script `0x22+n`、偏移表 `zPlayerOption+0x74` | ✅ 一手；🟡 五档命名 |
| 8 | 荷取：`pl01.anm` script58 水泡（920 帧 / 破罩 `case(1)`）、挡一次中弹、没被撞退**半档**（`collect_power(10)` vs `BOMB_COST` 20）| ✅ 一手 |

**还缺什么**

- ❓ 缺口 `+0x888` 坐标系原点（见 §1）。
- ❓ `0x4c92b0`/`0x4c92b4`（菜单重复键判定用的现帧/上帧对）的真实来源 —— 字节子串穷举在这里不可靠（立即数假命中）。
- 🟡 `pl00c.sht` 的速度值与 6 个 `pl*.sht` 的完整字段。
- 🟡 帕秋莉长按的实际切换速率。
- 🟡 荷取罩子那条路的 SE 号（`0x26` 破罩 / `0x33` 没被撞）只取到 id（`th11:0x44a1e0` / `th11:0x44a260`），**没对到具体 wav 文件名**。
- ❓ 破罩后 `bomb_count`（`ENEMY_MANAGER+0x14`）会不会每帧 +1：`th11:0x407937` 每帧清 bit2 ⇒ 下一帧又进 restart 块；「只计一次」的一手保证没取到（不影响罩子/退款的结论）。

## 附录：复现命令

```bash
E=local/th11.v1.00a/th11.exe
objdump -d -M intel --start-address=0x430e50 --stop-address=0x431070 $E   # 缺口状态机
objdump -d -M intel --start-address=0x430290 --stop-address=0x4308a0 $E   # 起手 + 钳位 + 分派
objdump -d -M intel --start-address=0x4308e3 --stop-address=0x430b40 $E   # 帕秋莉形态切换
objdump -d -M intel --start-address=0x4245e0 --stop-address=0x424620 $E   # 萃香全收
objdump -d -M intel --start-address=0x432cc0 --stop-address=0x432f20 $E   # 子机数组/数量
objdump -d -M intel --start-address=0x407680 --stop-address=0x4079a0 $E   # 荷取罩子（start + on_tick 全链）
objdump -d -M intel --start-address=0x406860 --stop-address=0x4068a0 $E   # set_iframes(n)
./local/vendor/thtk/build/thanm/thanm -l 11 local/_scratch/th11sht/pl01.anm \
  -m local/vendor/eclmap/anmmap/v4.anmm | sed -n '/^script script58 /,/^}/p'   # 水泡脚本正文
```
