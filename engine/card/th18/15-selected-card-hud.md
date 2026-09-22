# TH18 卡牌系统 — 左下角「当前（选中）主动卡」UI 全链路

> **版本**：TH18 v1.00a（`th18.exe`，imagebase `0x400000`）。本文裸地址默认属该版本；引用其他版本须写成 `th16:0x…`。
> **方法**：一手反汇编（`objdump -d` 逐条 + Ghidra 12 反编译，db `th18`）+ `thanm spec` 解出的 `abcard.anm.txt` +
> ExpHP `th-re-data` 的 `zAnmVm` / `zAnmManager` 结构体名。
> **可信度**：调用链 / 脚本号 / 常量 / 判据 ✅；块的**屏幕矩形** 🟡（一个 48 px 的水平锚点歧义，见 §5）。
> **前置**：[`01-object-model.md`](01-object-model.md) §5、[`04-active-cards.md`](04-active-cards.md) §6、
> [`../../anm/th18/01-vm-instantiate.md`](../../anm/th18/01-vm-instantiate.md)。

## 0. 一句话结论

左下角那块「当前主动卡」= **`abcard.anm` script0 的一棵 5 个 VM 的树**，id 存在 `ABILITY_MANAGER->+0x3c`；
它显示的是**当前选中卡的 `sprite_small` 卡图 + 原生 `ability/abframe.png` 卡框**，以及**充能进度条**
（靠 `scale.y` / `uv_scale.y` 缩放实现）。
**出现/消失只有一条判据**：`AbilityManager__update_selected_active_card_hud` `0x408c30` 按
`ABILITY_MANAGER->+0x38`（选中的主动卡对象）为 0 与否，递归置/清整棵树的 `visible` 位
（`vm+0x534` **bit1**）—— 为 0 时**整棵树一个像素都不画**（不是画成 0 长度序列）。
**动态**：出场 20 帧淡入（`alphaTime(20, 0, 255/224/64)`）；玩家压到左下角时
**8 帧线性压到 alpha 64**（`interruptLabel(5)`），离开时 **8 帧线性淡回**（`interruptLabel(4)`）。

## 1. 这块 UI 是什么（anm 侧，一手）

文件 `local/th18.v1.00a/anm/abcard/abcard.anm`，脚本号 **script0**（树根）+
`scriptNew` 出来的 **script1 / script2 / script3 / script4**：

```
script script0 {                       // 根：自己不画（sprite(-1)），只为给子 VM 定变换
-1:  sprite(-1); ins_431(1); randMode(1); type(1);
     scale(0.75f, 0.75f);              // ★ 根没有 sprite ⇒ 这个 0.75 是给子 VM 继承的
     pos(68.0f, 924.0f, 0.0f);         // ★ 表面 px（见 §5）
     anchor(1, 2);                     // 水平锚 1（左）· 垂直锚 2（底）→ 块的**左下角** = (68, 924)
     layer(24); resolutionMode(2); alpha(0);
0:   scriptNew(script3); scriptNew(script4); scriptNew(script1); scriptNew(script2);
+20: stop();
}
```

| script | sprite | layer | color | 出场 alpha | 作用（一手） |
| --- | --- | --- | --- | --- | --- |
| 1 | `sprite0` | 24 | `(0, 32, 0)` | `alphaTime(20,0,255)` | 卡框的**暗描边**层 |
| 2 | `sprite0` | 25 | `(255,255,255)` | `alphaTime(20,0,255)` | 卡框主体（**充能条就在它身上**，§3）|
| 3 | `sprite3` | 25 | 默认 | `alphaTime(20,0,64)` | 卡图（**充能条也在它身上**，§3）|
| 4 | `sprite3` | 24 | `(32,32,32)` | `alphaTime(20,0,224)` | 卡图的暗层 |

- `sprite0` = `abcard.anm` entry0 的 sprite0 = **`ability/abframe.png` (0,0,64,80)** —— 原生卡框。
- `sprite3` = entry3 的 sprite = **`ability/dummy.png` (0,0,64,80)** —— **占位**，运行时被引擎用
  卡表的 `sprite_small` 覆盖（§2 第 3 步）。`sprite` 号在 `abcard.anm` 里是**跨 entry 连续**的
  （118 个 entry 各 1 个 sprite ⇒ `spriteN` = entry N 的那一张）。
- 子 VM 不写 `scale`/`pos` ⇒ 随父（根那段 `scale(0.75,0.75)` 写在**没有 sprite 的根**上，
  这是「子 VM 继承父变换」的自证）。⇒ 块 = 64×80 × 0.75 = **48×60 表面 px**。

## 2. 谁画它：四个引擎函数（一手调用链）

| 步 | 函数 | 地址 | ABI | 做什么 |
| --- | --- | --- | --- | --- |
| 1 建 | `FUN_00407f10`（关卡开场辅助，未命名） | `0x407f10` | 无参 | 删旧的 `mgr+0x3c` VM → 从 `mgr+0x10`（= `abcard_anm`）起 **script 0** → 实体坐标清零 → 进 **world 列表** → 写 `mgr+0x3c` = 新 id → 调第 3、4 步 |
| 2 门 | `AbilityManager__on_tick` | `0x408640` | thiscall(`mgr`) | 每帧：左下角压暗闸（§4）+ 对选中卡调第 3 步 |
| 3 绘 | `AbilityManager__draw_active_card_hud_entry` | `0x408890` | thiscall(`mgr`; id, i2, i3, card, f) | **充能条**：`fill = 1 − card+0x3c / card+0x48` → 写子 VM 的 `scale.y`/`uv_scale.y`/`uv_scroll.y`；按 `is_firing()` 与充能档发 `interrupt 2/3` 并配色 |
| 4 显隐 | `AbilityManager__update_selected_active_card_hud` | `0x408c30` | thiscall(`mgr`)，**无栈参、`ret`** | 按 `mgr+0x38` 递归置/清整棵树的 `visible`；非 0 时把 `sprite_small` 写进 script3/script4 |

逐条证据：

- **建**（`0x407f10`）：`mov ebx,[edi+0x10]`（`abcard_anm`）→ `AnmManager__allocate_new_vm` `0x489320`
  → `AnmManager__sub_407420(anm, vm, 0)`（脚本号 **0**）→ `or [esi+0x538],0x1000`
  → `AnmManager__insert_in_world_list_back` `0x488350`（**world 列表**，不是 UI 列表）→
  `mov [edi+0x3c],esi`（id）→ `call 0x408c30` → `call 0x408d00`。
  调用点：`0x444350`（`FUN_00443e60` 内）与 `0x444599`（`FUN_004443c0` 内）—— 两处都在
  `GameThread::on_tick__stub` `0x443d70` 与 `GameThread::end_stage` `0x444650` 之间 ⇒ **关卡开场**；
  两处前面都是一长串 `or [anm+4],2`（把各 anm 重新装载）。🟡 这两个 `FUN_` 的确切名字没定。
- **显隐判据**（`0x408c30`）：`mov eax,[ecx+0x38]; test eax,eax; jne 0x408c75`；
  `mgr+0x38 == 0` 分支 = `and [vm+0x534],~2`（`0x408c50`）再对 `vm+0x598`（子链表）逐个调 `FUN_00488f20`
  （同一个 `&= ~2`，递归）；非 0 分支 = `or [vm+0x534],2`（`0x408c8d`）+ `FUN_00488ef0` 递归置位，
  然后 `FUN_00489230(&mgr+0x3c, 4, 0)` / `(…, 3, 0)` 取到 **script4 / script3** 那两颗子 VM，
  各调 `AnmLoaded__set_sprite(anm, vm, card->table_entry(+0x4c)->+0x30)`（**`sprite_small`**）。
- **`FUN_00489230(&id, 脚本号, 第几颗)`**：内部 `FUN_004890b0` 沿 `vm+0x594`（子链表）递归找
  `*(short*)(child+0x4a0) == 脚本号` 的第 N 颗；`+0x4a0` 是**脚本号**（写它的唯一处 = 建 VM 时
  的 `0x486a35 mov WORD [esi+0x4a0],cx`，`cx` 是脚本表下标）。
- **充能条**（`0x408890`）：`xmm1 = card+0x3c / card+0x48`（充能**剩余比**）、`fill = 1.0 − xmm1`；
  子 VM 取 `FUN_00489230(&id, 2, 0)`（= script2）与 `(…, 3, 0)`（= script3）；收尾写
  `[vm+0x58] = fill`（`scale.y`）、`[vm+0x70] = fill`（`uv_scale.y`）、
  `[vm+0x80] = (1 − fill) × 0.625`（`uv_scroll_pos.y`，常量 `0x4b9148` = 0.625），并 `or [vm+0x534],0x18`。
  ⇒ 「充能条」不是一条独立的条，而是**卡框与卡图从底往上长**。
- 为什么 `mgr+0x3c` 那棵树在 **world 列表** 却能画在屏幕上：`AnmManager__render_layer` `0x488260`
  对 world 列表的折算是 `layer ∈ [37,45] ⇒ 组 = layer − 13`、**其余组 = layer**（`0x4882a1` 起）；
  `layer(24)` ⇒ 组 24，而组 24 由 `0x487bb6` 那一趟渲染到表面 `0x4cd314`（`0x487b82`–`0x487bb6`）。
  UI 列表那条另算（`layer ∈ [24,32] ⇒ 组 = layer + 13`，我们的 HUD 走 37/38）。
  ⇒ **两棵树用的是同一个「表面 px」坐标系**（同一 anm 里 script5 的 `pos(1056, 537)` 已被实跑钉成表面 px）。

## 3. `visible` 位 = **整块 UI 存在与否**的唯一判据（一手）

`vm+0x534`（TH18 的 `flags_lo`；ExpHP `zAnmVmPrefix.flags_lo`）：

| 证据 | 内容 |
| --- | --- |
| 指令 | anm opcode **310 = `visible`**（`local/vendor/thpages/static/mapfile/v8.anmm:80`）→ `AnmVm__run` 里 `0x47a0c8 or [ebx+0x534],2` / `0x47a0e3 and [ebx+0x534],~2` |
| 读者 | `0x481248`（顶层绘制，`AnmManager__draw_vm` 一族）`mov eax,[edi+0x534]; test al,1; je 退出; **test al,2; je 退出**; test [edi+0x538],0x180; jne 退出` |
| 语义 | bit1 = 0 ⇒ **这个 VM 连顶点都不生成**；`render_layer` 不看它（只看 `flags_hi & 0x180`），所以是**绘制前的一道门** |

⇒ `update_selected_active_card_hud` 在 `mgr+0x38 == 0` 时对**整棵树**（根 + 4 个子）清 bit1
= **框、卡图、卡图的暗层、卡框的描边层一起消失**，不是「画一条 0 长度的序列」。
`AnmManager__sub_488d50` `0x488d50`（删 VM 用）与 `FUN_00488ef0`/`FUN_00488f20`（置/清整棵子树）
是同一位的另外三个读写点。

## 4. 动态（一手数值 + 脚本号）

### 4.1 出场 / 消失 / 换卡

- **出场**：VM 一建出来就带 `alpha(0)`，四个子脚本各自的 `-1:`/`0:` 段给
  `alphaTime(20, 0, 255)`（script1/2）、`alphaTime(20, 0, 64)`（script3）、`alphaTime(20, 0, 224)`（script4）
  ⇒ **20 帧线性淡入**。没有位移、没有缩放动画。
- **消失**：**没有过渡**——`update_selected_active_card_hud` 直接清 `visible`
  （一手：`0x408c50 and [eax+0x534],0xfffffffd`，当帧生效）。子 VM **不被销毁**，只是不画。
- **换卡**（D 键 / 卡组变化）：`AbilityManager__set_selected_active_card` `0x408b00` 命中后
  `call 0x408c30`（`0x408b74`）⇒ 同样**无过渡**，当帧换成新卡的 `sprite_small`。
  卡图只换 sprite，**框与位置一个像素都不动**。

### 4.2 玩家压到左下角 ⇒ 压暗（8 帧 / alpha 64）

`AbilityManager__on_tick` `0x408640` 里的闸（常量直读）：

| 方向 | 条件 | 动作 |
| --- | --- | --- |
| 进（`mgr+0x40 == 0` 时判） | `player+0x620 < −64.0`（`0x4b945c`）**且** `player+0x624 > 368.0`（`0x4b937c`）| `push 5; push [mgr+0x3c]; call 0x488c60` ⇒ `mgr+0x40 = 1` |
| 出（`mgr+0x40 != 0` 时判） | `player+0x624 < 352.0`（`0x4b9374`）**或** `player+0x620 > −48.0`（`0x4b9454`）| `push 4; push [mgr+0x3c]; call 0x488c60` ⇒ `mgr+0x40 = 0` |

- `player+0x620` / `+0x624` = 自机**内侧结构的 x / y**（ECL 单位，x ∈ [−184,184]、y ∈ [32,432]）
  ⇒ 一个自机坐标矩形：**进 = `x < −64 ∧ y > 368`**、**出 = `x > −48 ∨ y < 352`**
  ⇒ **两个轴各 16 px 滞回带**。
- `FUN_00488c60(id, n)` = `get_vm_with_id` → 先 `call [on_interrupt 表 + …]`、再 `[vm+0x494] = n`、
  然后 **`AnmVm__run(vm)` 立刻跑一帧**，对子 VM 逐个同做（`0x488c60`–`0x488cf0`）。
  与 `AnmManager__interrupt_tree` `0x488be0` 的差别就是**多跑这一帧**（出场的 `alphaTime` 当场起步）。
- 子脚本的应答（`abcard.anm`，逐条）：

```
script1 / script2:
    interruptLabel(5): alphaTime(8, 0, 64);   caseReturn();   // 压暗：8 帧线性到 64
    interruptLabel(4): alphaTime(8, 0, 255);  caseReturn();   // 淡回：8 帧线性到 255
script3:
    interruptLabel(5): alphaTime(8, 0, 64);   caseReturn();
    interruptLabel(4): alphaTime(8, 0, 64);   caseReturn();   // 卡图这层本来就停在 64
script4:
    interruptLabel(5): alphaTime(8, 0, 64);   caseReturn();
    interruptLabel(4): alphaTime(8, 0, 224);  caseReturn();
```

⇒ **原生「压暗」的统一值 = 终点 alpha 64（25% 不透明）、过渡 8 帧线性**，方向翻转从当前 alpha 起算
（`alphaTime` 的 mode 0 语义）。**淡回没有另一个超时**：`mgr+0x40` 是闩锁，离开那一帧才发 4。

### 4.3 充能条与「正在发动」的配色（脚本号）

`interrupt 2 = 充能满`、`interrupt 3 = 充能中`；`draw_active_card_hud_entry` 另外**直接写颜色**：

| 状态 | 谁写 | 值 |
| --- | --- | --- |
| 空转（`card+0x38 == 0`，充能到底） | `0x4088e8` 之后 | `interrupt 2` + `color_1 = 0xffff4040`（红）|
| 充能中 | 同上 | `interrupt 3` + `color_1` 由 `fill` 插值（G 通道 `fill×64 + 128`、R/B `fill×64`）|
| 正在释放（`is_firing()`，虚表 `+0x40`）| 同上 | `interrupt 2` + `mgr+0x48 & 3` 的 3 帧循环配色 |

## 5. 块的屏幕矩形（🟡 一个 48 px 歧义）

一手：`pos(68.0f, 924.0f, 0.0f)`、`anchor(1, 2)`、`scale(0.75, 0.75)`、`resolutionMode(2)`。

- **位置与尺寸空间** ✅：同一个 `abcard.anm` 里 script5（卡组图标行）的 `pos(1056, 537)` 已被
  实跑钉成 **表面 px**（[`mods/th18.v1.00a/card-expand/AUDIT.md`](../../../mods/th18.v1.00a/card-expand/AUDIT.md) §X5），
  两个脚本同为 `resolutionMode(2)` ⇒ (68, 924) 也是表面 px。
- **矩形**：48×60（64×80 × 0.75）⇒ 若 `anchor(1,2)` = **左·底**（社区口径，本仓自己几支脚本也按 1 = 左用），
  块 = 表面 **x 68..116 / y 864..924**；若 1 = 右则 = x 20..68 / y 864..924。
  🟡 **差的这 48 px 不影响任何结论**：两种解释下这块都在**游玩区左边缘（257）之左**。
- ⚠ **`HUD-LAYOUT.md` §1.3 的「原版在 x < 1000、y > 850 什么都不画」是错的**（本文 §1 就是反例）——
  那份表只普查了 `zGui__on_draw` 的 ASCII 文字与 HUD 图标排版，**漏了这棵 VM**（它在 world 列表，
  不在 `Gui` 的文字表里）。已在 `HUD-LAYOUT.md` §1.3 原地订正。

## 6. 给改造者的接缝清单

| 想改什么 | 改哪 |
| --- | --- |
| 有/无当前卡 ⇒ 整块显隐 | `ABILITY_MANAGER->+0x38`（选中对象）。把它置/清之后**必须**调 `AbilityManager__update_selected_active_card_hud` `0x408c30`（`set_selected_active_card` 成功时才会替你调；置空时**不会**）|
| 那块显示哪张卡图 | 卡表行的 `+0x30`（`sprite_small`，在 `abcard.anm` 的 **sprite 号空间**里）|
| 框 / 位置 / 层次 / 动画 | `abcard.anm` script0–4（**别在 C 里另写一套**）|
| 压暗的闸 | `0x4086cc`–`0x408747` 的四个常量（`0x4b937c/0x4b945c/0x4b9374/0x4b9454`）|
| 让新卡有那张图 | `abcard.anm` 每卡 +2 entry（`*_max` / `*_min`），本 mod 的 `assets/build_abcard.py` 就是干这个 |

⚠ **别按 `sprite_large`（`+0x2c`）找这张图**：这张 UI 只读 `+0x30`（`sprite_small`）；
`+0x2c` 给的是编成/图鉴那种大图。
