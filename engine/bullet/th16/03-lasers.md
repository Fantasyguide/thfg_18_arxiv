# 03 — TH16 激光子系统(EX_LASER)
> **版本**：TH16 v1.00a（`th16.exe`，imagebase `0x400000`）。本文裸地址默认属该版本；引用其他版本须写成 `th18:0x…`。
>

> **对象**:TH16《鬼形兽》`th16.exe`,imagebase `0x400000`。日期 2026-06-09。
> 来源:主控 inline 钉锚点(构造器 + RTTI vtable 名)→ 子 agent 深挖 → 主控**一手复核碰撞几何**。
> 可信度 ✅一手 / 🟡 agent 单源未逐个亲反 / ❓存疑。仅 TH16。
>
> 激光由弹 VM 的 **opcode `0x8000000`(EX_LASER)** 生成(见 `01-core-engine.md` §3)。它不是弹池对象,
> 是**独立对象类**,住在**激光管理器 `DAT_004a6ee0`**(≠弹 `DAT_004a6dac`、≠敌 `DAT_004a6dc0`)。

## 0. 速览

- **5 个激光类**(名来自 exe 的 MSVC RTTI vtable,= 二进制自带,强证):`LaserLineInf`(线)、`LaserInfiniteInf`(无限/激光柱)、`LaserCurveInf`(曲线/蛇)、`LaserBeamInf`(粗 beam)、`LaserDataInf`(公共基类,全 stub)。
- **生成两条路**:子弹 VM EX_LASER(`bullet_vm_exec` opcode `0x8000000`,仅 line/inf)/ ECL(4 个 opcode 经 `allocate_new_laser`,全 4 类型)。详见 §3a / §6a。
- **碰撞 = 旋转 OBB**(把自机转进激光局部系测长×宽盒),✅ 主控亲验,与弹的圆/矩形判定**不同**。命中调同一个 `player_on_death`(`0x443f10`)。
- **vtable 驱动**:每帧 tick / draw / collide / graze / bomb-cancel-rect / bomb-cancel-circle 各占一个槽。

## 0.5 ★ 三件事(显示 / 判定 / Bomb 取消)清晰梳理(✅ 全部主控一手 2026-06-27)

> 这一节回答"激光怎么显示、怎么判玩家、bomb 怎么取消激光"。读完不用翻别处。

### A. 显示(每帧画法,prio 0x23 on_draw 链)

```
Window::do_frame → run_all_on_draw                    [engine/_shared/frame-loop.md]
 └ LaserManager::on_draw_23 (LAB_00431720,匿名,prio 0x23)
    │ - 读 GAME_THREAD+0x88 做暂停门
    │ - 走 g_laser_mgr+0x14 活动链
    │ - 每节点 node[4]==1(dying)跳过,否则:
    └→ vtable[5] = LaserLine::on_draw 0x433720 / LaserInfinite::on_draw 0x4357a0 / 曲线/beam 各自
       │ - 算 angle+π/2,归一化到 [-π,π](★ 两端都闭、上限 34 次,跟数学模块同套)
       │ - 把激光的 (pos, angle, sprite 状态) 写进对象内嵌的 ANM 子精灵
       │ - 对每个子精灵调 AnmManager::draw_vm 真画
       └→ 子精灵数:LaserLine 三个(body +0x92c / head +0x1524 / tail +0xf28);
                    LaserInfinite 两个(body +0x950 / tip +0xf4c)
```

**关键事实**:
- **激光精灵 = `bullet.anm` 图集里的 sprite**,`LaserManager::initialize` 预加载到 `g_laser_mgr+0x604`,不是单独的 laser.anm。
- **激光自己不画像素**——它只更新自带的若干个 ANM "子 VM"(各自跑 anm 脚本),由 `AnmManager::draw_vm` 统一渲染。所以**激光显示 = "vtable 准备数据 + 委托给 AnmManager"**。

### B. 玩家判定(每帧 tick → 旋转 OBB)

```
Window::do_frame → run_all_on_tick                     [prio 0x1b]
 └ LaserManager::on_tick_1b (0x4316b0, 暂停门 thunk)
    └ LaserManager::on_tick_1b__body (0x431510)
       │ 走 g_laser_mgr+0x14 活动链,每节点按 flags 路由:
       │  (a) node[4]==1 ∣∣ (node[3]&6)≥4 → DEATH (vtable[6] destroy + 解链 + heap free)
       │  (b) (node[3]&8)==0 → vtable[4] 主调度(LaserLine/Inf/Curve/Beam_frame_tick)
       │         └→ vtable[13] = LaserXxxx::collide_player → player_collide_laser_obb (0x443af0)
       │              · 把自机 (player+0x610/614) 平移 - 激光原点,
       │                旋转(crt_sinf/cosf(-angle))进激光局部系
       │              · 对 [0,length] × [-width,+width] 盒 + 自机判定框 (player+0x2c748/2c74c) 撑大
       │              · 命中 → ret 1 + 调 player_on_death (无敌/状态门和弹判定一致)
       │              · 擦弹环 → ret 2 + 每 3 帧调 player_graze(line)
       │              · 未中 → ret 0
       │  (c) (node[3]&8)!=0 → vtable[13](1) 仅判定模式(不跑 frame_tick,只判碰撞)
```

**关键事实**:
- **判定算法**:**旋转 OBB**(把世界系自机转进激光局部系再测轴对齐盒)。与弹的圆判(`player_collide_circle`)/ 矩形判(`player_collide_rect`)算法**不同**,但**死亡入口共用 `player_on_death`**(无敌位 `+0x1663c`、状态 `+0x165a8∉{2,3,4}` 门一致)。
- **无限激光只在 state 4(扩张)/2(满)生效**;等待 / 收缩期 vtable[13] 直接 ret 0。
- **暂停门**`on_tick_1b` thunk:bit0/bit0x400 跳过;bit2 慢动作模式时 `GAME_SPEED=0` 调 body 再恢复。

### C. Bomb 怎么取消激光

**调用方:bomb 的 method_10**(=各角色 / 季节的"释放"/"炸弹"清场),例:

| Bomb | method_10 起点 | 矩形/圆 | 备注 |
| --- | --- | --- | --- |
| BombSubDoyou(土用)| `0x40E3C0` | 圆 vtable[9] | 主控亲反 ✅ |
| BombSubSpring/Summer/Fall/Winter | `0x40Exxxx/0x40Fxxxx/0x4104xx/0x4117xx` | 圆 vtable[9](推) | 🟡 调用形式同 Doyou |
| BombMainCirno | `0x40F4C0..` | 圆 vtable[9](推) | 🟡 |
| BombReimu(灵梦)| `0x411xxx` | **矩形 vtable[8](推)** | 🟡 灵梦 box 弹幕清场 |

**机制(以 Doyou 圆形释放为例,主控一手 ✅)**:

```c
// in BombSubDoyou::method_10 @ 0x40E3C0:
center = anm_vm_get_blast_center(bomb->anm_id);              // 从 release anm vm 取中心
radius = anm_vm[+0x50];                                       // ANM vm 的缩放/半径字段
BulletManager::cancel_radius_as_bomb(center, 4);              // ① 清弹幕(BulletManager 单独走)

piVar3 = *(g_laser_mgr + 0x14);                               // ② 激光自己跑迭代(没有 manager 级 helper)
while (piVar3 != 0) {
  next = piVar3[2];
  if (piVar3[4] != 1)                                         // 非 dying
    (**(*piVar3 + 0x24))(center, radius, 4, 1);              // ★ vtable[9] = cancel_as_bomb_circle
                                                              //   arg4=1 → 尊重 this+0x5c8 免疫位
  piVar3 = next;
}
```

**每激光的取消方法**(`vtable[9]`=圆 / `vtable[8]`=矩形,各类自己实现):

| 类 | vtable[8] 矩形 | vtable[9] 圆 |
| --- | --- | --- |
| **LaserLine** | `cancel_as_bomb_rectangle` `0x433860` | `cancel_as_bomb_circle` `0x434730` |
| **LaserInfinite** | `cancel_as_bomb_rectangle` `0x435880` | `cancel_as_bomb_circle` `0x436670` |
| **LaserCurve** | `cancel_as_bomb_rectangle` `0x4397d0` | `cancel_as_bomb_circle` `0x43a2f0`(主控亲反 ✅) |
| **LaserBeam** | `0x430EA0` **= `ret 0`** | `0x430EB0` **= `ret 0`** |

→ ★ **LaserBeam 完全 bomb 免疫**(slot 8/9 都是空实现,bomb 调了等于没调)。

**取消内部干啥**(以 `LaserCurve::cancel_as_bomb_circle` `0x43a2f0` 一手反读):
1. **免疫检查**:`if (param_4 != 0 && this+0x5c8 != 0) return;` —— `+0x5c8` 是激光的**自标 bomb 免疫位**,bomb 设 param_4=1 时尊重它。
2. **逐段测试**:走 `this+0x1524` 段数组(共 `this+0x5f4` 段),每段 (xy) 算 `dist²` vs `radius²`。
3. **命中段处理**:
   - 计入命中数 + 标记本段 hit;
   - **每 10 段命中才掉道具一次** `gen_items_from_cancel(seg_xy, param_3)` + 经 `AnmLoaded::create_40e5c0` 生取消粒子(防爆量、典型 1 道具/数段)。
4. **善后两条路**:
   - 全段都命中 → `this+0xc |= 2`(标志位 2)→ **下一帧 `on_tick_1b__body` 的 `(node[3]&6)≥4` 路由把它送进 DEATH**(vtable[6] destroy + 解链 + heap free)。
   - 部分命中 → 把剩余未命中段往前 memmove 压实 + 减计数(等于**截短激光**,留未命中部分继续跑)。
5. **时间步还要前移**`+0x44/+0x48`(类似 frame_tick 里的时间步,但用 `-iVar9` 倒退或快进——具体语义 🟡)。

**取消的"声音/分数副作用"**:`gen_items_from_cancel` 生分数道具;ANM 粒子是常见的"激光碎裂"白色闪光。**没有**直接玩家加分调用、没有 score popup 写入(那部分在 player/ 那边的 graze 路径)。

### 三件事的内核相互关系

- **显示 (A)** 和**判定 (B)** 是**完全独立的两条 vtable 槽**:slot 5 = on_draw、slot 4/13 = on_tick/collide;前者只读对象状态喂 AnmManager,后者只跑几何 + 改状态。两者**互不调用**。
- **Bomb 取消 (C)** 主动**改对象状态**(标 +0xc 位 2 或截短段数组),改完之后**下一帧的 (B) 的 `on_tick_1b__body` 路由层**(看 `node[3]&6`)自然把激光走到 DEATH。所以 bomb 不直接调 destroy,**bomb 设旗,tick 后清扫**——典型的 game-loop 解耦设计。
- **LaserBeam 的特殊性**:bomb-cancel 两槽都 `ret 0`,这是引擎里**少数 vtable 故意空实现**的例子(用语义传达"我不参与这套交互")。

## 1. 类 / vtable / 构造器(✅ RTTI 名 + 一手)

| 类 | vftable | 对象大小 | 构造器 | EX_LASER |
| --- | --- | --- | --- | --- |
| `LaserLineInf` | `0x492424` | `0x1b20` | `0x431130` | EX_LASER `a=0` **或** ECL |
| `LaserInfiniteInf` | `0x4923b8` | `0x1548` | `0x431860` | EX_LASER `a=1` **或** ECL |
| `LaserCurveInf` | `0x4922e0` | **`0x1568`** | `0x431900` | **仅 ECL**(`allocate_new_laser` type=2)|
| `LaserBeamInf` | `0x49234c` | **`0x1f28`** | `0x4318c0` | **仅 ECL**(`allocate_new_laser` type=3)|
| `LaserDataInf` | `0x492490` | — | — | 公共基类(vtable 全 stub) |

> 上表 curve=`0x1568`/beam=`0x1f28` 是从 `LaserManager::allocate_new_laser`(`0x431760`)的 `switch(type)` 里的 `operator_new(size)` 实读(早期猜的 0x1b20 错)。线/无限两路创建:**(A) bullet VM EX_LASER**(`bullet_vm_exec` opcode `0x8000000`,inline 灌 ring,只能 a=0/1)/ **(B) ECL → `allocate_new_laser`**(分发器,4 种 type 都可,见 §3a)。

公共基类 ctor `0x430fc0`;ANM 子精灵 init `0x4093f0`(线 3 个子精灵 body/head/tail,无限 2 个)。

## 2. vtable 槽职能(🟡 agent 枚举,主控验了碰撞槽)

> 槽 N = vtable + N×4。下表以 LaserLine / LaserInfinite 为例。

| 槽 | 职能 | LaserLine | LaserInfinite |
| --- | --- | --- | --- |
| 1 | 每帧 UPDATE(跑自身 VM/移动) | `0x431fe0` | `0x436fd0` |
| 3 | INIT(= vtable+0xc,spawn 后调,灌帧字段) | `0x431b30` | `0x435050` |
| 4 | **每帧主调度**(调 update + 碰撞 + 状态机) | `0x432f40` | `0x4352f0` |
| 5 | DRAW(委托 AnmManager) | `0x433720` | `0x4357a0` |
| 7 | GRAZE 循环(逐段计擦弹) | `0x434010` | `0x436010` |
| **8** | ★ **bomb 取消(矩形)** = `cancel_as_bomb_rectangle` | `0x433860`(1968 B,**非** ret0!)| `0x435880`(1922 B) |
| **9** | ★ **bomb 取消(圆)** = `cancel_as_bomb_circle` | `0x434730` | `0x436670` |
| 13 | **玩家碰撞 hit-test**(→ `0x443af0`) | `0x433510` | `0x435610` |
| 10 | 死亡/清除特效 | `0x434cd0` | `0x436c70` |
| 20 | 线激光屏缘**分裂/绕环**(到屏边生子段) | `0x432620` | (stub) |

> ★ **slot 8/9 纠错**(2026-06-27):早期 agent 把这两槽误标为"命中伤害派发(ret0)/split-respawn",实测是 **bomb 取消 矩形/圆** 两套(各类 1.4–2KB,做段过滤 + 道具掉落 + 善后)。LaserBeam 这两槽是 **`ret 0`**(`0x430EA0`/0x430EB0)= **bomb 免疫**。完整调用链与内部逻辑见 §0.5 C 节。

## 3. 对象结构(✅ 基类偏移一手;🟡 部分子类字段 agent)

**公共基类**(ctor `0x430fc0`):`+0x10` 状态(1 扩张/2 满/4 收缩/5 完)、`+0x14` 链表 next、`+0x54/58/5c` **起点 xyz**、`+0x60/64/68` 速度、`+0x6c` **角**、`+0x70` **当前长度**、`+0x74` **当前半宽**、`+0x78` 扩张速率、`+0x7c` 枢轴偏移、`+0x80` 句柄、`+0xb0` 擦弹计时。

**LaserInfinite 时序状态机**(✅ `0x4352f0`):`+0x181` start_time、`+0x182` expand_time、`+0x183` duration、`+0x184` stop_time、`+0x17c` 角速。状态 `3 等待→4 扩张(宽线性增)→2 满→5 收缩→free`。**碰撞只在 state 4/2 生效**(等待/收缩期无判定)。

## 3a. ★ 两条创建路径(✅ 主控一手 2026-06-13)

激光与子弹**创建机制不同**(用户提到的"ECL 创建激光逻辑不一样"在这里):

```
路径 A: 子弹 VM(EX_LASER, 弹运动字节码 opcode)         路径 B: ECL → allocate_new_laser
  bullet_vm_exec @0x413860  case 0x8000000              EnemyData__ecl_run_over_300 @0x41dcb0
   └ if instr[4]==0 → operator_new(0x1b20)/line_ctor      ├ 4 个 case-arm 在 0x421AC1 / CA1 / E05 / F8F
   └ if instr[4]==1 → operator_new(0x1548)/inf_ctor        └ 各传不同 type/参数 →
   └ inline 灌 ring + 计数 + vtable[3](frame)             LaserManager::allocate_new_laser(type, &frame)  @0x431760
                                                            └ cap 512 + 句柄 + switch(type 0..3):
   → 只能生 line/inf;参数从弹的 instr 字段编进帧             0: new(0x1b20)/line_ctor   ← 也能从 EX_LASER 来
                                                              1: new(0x1548)/inf_ctor    ← 也能从 EX_LASER 来
                                                              2: new(0x1568)/curve_ctor  ★ 仅 ECL
                                                              3: new(0x1f28)/beam_ctor   ★ 仅 ECL
                                                            链入 ring(+0x5e0)+ 计数(+0x5e4) + vtable[3](frame)
```

**两条路殊途同归**:都用同一个 `vtable[3]`(INIT)灌帧、链同一管理器 `g_laser_mgr`(`DAT_004a6ee0`)。差别在:
- **生成 type 范围**:EX_LASER 仅 0/1;ECL 全部 0/1/2/3。
- **谁建帧**:EX_LASER 从父弹的 `instr[..]` 拼装;ECL 用 `EclRunContext::ecl_get_int_arg / ecl_get_float_arg`(`0x473c90/0x473d40`)从 ECL 字节码取参。
- **是否复制父字节码**:EX_LASER 因为发生在弹 VM 里、parent 是弹,会顺手把父弹 `obj+0xc88` 字节码也拷一份(子弹分裂语义);ECL 没这码事,激光从纯 ECL 参数构造,**无字节码继承**。
- **共享**:`bullet.anm` 精灵图集(`LaserManager::initialize` 预加载到 `mgr+0x604`)、ring/cap/handle 机制、vtable[3] INIT 入口、vtable[1/4/5/6/13] 运行时分派、`g_laser_mgr` 唯一池。

> **澄清:激光不携带 ECL 运行上下文。** `LaserManager__on_tick_1b__body` 死亡路径调的 `ecl_free_runcontext` 其实就是 `global_heap_free_stub_47514b` 的 14 字节包装(th-re-data 命名误导)——只是 heap free。**激光是 vtable-driven 对象,没有内嵌 etEx 那种字节码 VM**——这点和子弹架构截然不同。

**4 个 ECL 创建 opcode 的逐 case 参数布局**(opcode 号 + frame 字段映射 + side-effect):由子 agent 扫 `EnemyData__ecl_run_over_300` 中那 4 处,结果待入库后补到 §6a。

## 4. ★ 玩家碰撞 = 旋转 OBB(✅ 主控一手复核 `0x443af0`)

```c
// player_collide_laser_obb(this, laser_origin_xy, half_len_bound(param_2), invuln(param_3)), 角∈XMM2, 半宽∈XMM3
dx = player_x(+0x610) - origin_x;  dy = player_y(+0x614) - origin_y;
s = sin(-angle); c = cos(-angle);                 // crt_sinf/crt_cosf(角取反 ^DAT_00494890)
along = dy*s + dx*c;   perp = dx*s - dy*c;         // 把自机转进激光局部系
if ( perp - hitX·K <= half_len  &&  along - hitY·K <= width·0.5
  && 0 <= perp + hitX·K        &&  width·(-1) <= hitY·K + along ) {   // 命中盒(自机判定框 +0x2c748/+0x2c74c 撑大)
   if (落在外圈带) return 2;                        // 擦弹
   if (非安全 && param_3==0 && 自机非死/复活(+0x165a8∉{2,3,4}) && 非无敌(+0x1663c<1))
       { player_on_death(player); return 1; }        // 命中 → 自机死(同弹的死亡函数!)
}
return 0;
```

→ 这是把自机平移差**旋转进激光局部坐标**后,对 `[0,长度]×[-宽,+宽]` 盒做判定的 **OBB**;命中/擦弹/未中三态,命中调 `player_on_death`,无敌/状态门与弹判定**逐项一致**。**与弹的圆(`0x4439e0`)/矩形(`0x4438c0`)判定是不同算法。** ✅

- 线激光 wrapper `0x433510`:据 `+0x608&2` 选"中点"或"原点"为判定中心;命中每 3 帧调一次 `FUN_00444cf0`(擦弹计分)。
- 无限 wrapper `0x435610`:state∈{4,2} 且长度>0 才判;半宽传 `+0x70 × ~0.5`。

## 5. 线 vs 无限(✅/🟡)

| | LaserLine `0x1b20` | LaserInfinite `0x1548` |
| --- | --- | --- |
| 生命周期 | 定长段,匀速增长到 `length=m` | 4 段状态机(等待/扩张/满/收缩) |
| 时序参数 | 无 | start/expand/duration/stop_time |
| 子精灵 | 3(body/head/tail) | 2(body/head) |
| 屏缘 | 槽20:出屏+对侧重入,边界处生新段 | 无 |
| shot/transform 音 | 来自 ECL 参数 `a2`/`b2` | **硬编码 18 / −1**(spawn 里 `0x12`/`0xffffffff`) |
| 宽度 | 固定 `s2` | 0→`r2` 扩张再收缩 |

## 6. EX_LASER 参数布局交叉验证(✅✅ 与 `engine/ecl/th16/ECL-info.md` 逐位吻合)

无限激光(`a=1`)spawn 帧(弹 VM `0x413860` 的 `0x8000000` 分支)**逐位实测吻合社区**:`flags=(d&0xFD)|2`(`instr[7]`)、`effect_index=(d&0xFF00)>>8`、`delete_current=(d&0x10000)>>16`、`start/expand/duration/stop=instr[5/6/0xf/0x10]`、`shot_sound=18`、`transform_sound=-1`。线激光(`a=0`):`sprite/color/delete=instr[4]` 字节、`shot_sound=instr[5]`、`transform_sound=instr[6]`、角/速走 ±999990 阈值族。**这是 EX_LASER 在 exe 里的强佐证。**

## 6a. ★ 4 个 ECL 创建 opcode 逐表(子 agent 扫 + 主控字节级复核 ✅)

`EnemyData__ecl_run_over_300`(`0x41dcb0`)用**两级跳转表**派发 ≥300 的 ECL opcode:
- **字节索引表**`ECL_RUN_OVER_300_BYTE_INDEX`(`0x422D70`,174 字节):`byte[opcode-300] = entry_idx`。
- **跳转表**`ECL_RUN_OVER_300_JUMP_TABLE`(`0x422AB8`,174×4 字节):`JMP table[entry_idx]`。

主控读了原字节实证 4 个 opcode 走向(2026-06-27):

| ECL opcode | byte[op-300] | jump[entry] | case start | laser type | 类 | ECL args | frame 构建 |
| --- | --- | --- | --- | --- | --- | --- | --- |
| **702** | `0x9B`(155) | `0x00421912` | `0x421912` | 0 | LaserLine | 1(shooter slot)| memset 0x358 |
| **703** | `0x9C`(156) | `0x00421ACB` | `0x421ACB` | 1 | LaserInfinite | 2(slot, +0x48)| **`LaserInfiniteInner::clear`** `0x411860` |
| **711** | `0xA4`(164) | `0x00421E0F` | `0x421E0F` | 2 | LaserCurve | 1(slot)| memset 0x358 |
| **713** | `0xA6`(166) | `0x00421CAB` | `0x421CAB` | 3 | LaserBeam | 2(slot, +0x28)| memset 0x354 |

**4 个 opcode 的共同模式**:
- **入口**:`EnemyData::ecl_get_int_arg__trampoline`(`0x4251d0`,= `EclRunContext::ecl_get_int_arg` 的薄包装,经 `this+0x44b8+0xc` 取 EclRunContext)取 ECL arg[0] = **shooter slot 号**。
- **shooter struct 定位**:`EnemyData+0x5C0 + slot*0x380`——slot 索引到一个 `0x380` 字节的"射手槽",里面**预先存了激光的所有配置参数**(角/速/长/宽/sprite/color/flags…)。这就是 ECL 的"射手"机制:ECL 先用别的 opcode 配置 shooter slot,再用 spawn opcode 触发。
- **shooter 字段映射**(各激光帧的来源):
  - 浮点 `shooter+0x5A0` `+0x5A4` `+0x5A8` `+0x5AC` `+0x5B4` `+0x5BC` `+0x8D8`(角/速/长/宽/几何参数族)
  - 整型 `shooter+0x598` `+0x59C` `+0x8E8` `+0x8EC` `+0x8F0` `+0x8F4` `+0x908` `+0x90C` `+0x8F8`(type/color/flags 族)
- **spawn 位置**:所有 case 都做一次 `COMISS shooter[+0x3EA0*4]` 判分支:正值 → 绝对坐标;否则 → 相对 `EnemyData+0x44/+0x48`(敌机当前位置)。打包到 frame `+0x00`(XY)/`+0x08`(Z 或 focus)。
- **type 标记**:`frame[type_field] |= 0x1`(line/curve)或 `0x2`(inf)(beam 无此 OR)——疑为 "delete current bullet"/laser 子类的 flag。
- **side effect**:**全部 4 个 opcode 都没有 "delete current bullet" / kill caller 调用**,与 EX_LASER 不同(EX_LASER 的 `d&0x10000` 可调 `FUN_00416840(parent,0)` 删父弹)。

**与 EX_LASER(bullet VM)路径的对比**:

| | EX_LASER(bullet VM) | 4×ECL opcode |
| --- | --- | --- |
| 可生 type | 0/1(line/inf) | 0/1/2/3(全部 4 种) |
| 参数来源 | 父弹 instr 字段(VM 当前指令) | shooter slot(ECL 别处预配的"射手") |
| frame 构建 | inline 拼装到 stack | memset(line/curve/beam) 或 `LaserInfiniteInner::clear`(inf) |
| 拷父字节码 | ✅(0xc6 dwords 进子激光) | ❌(无字节码继承) |
| 删父弹副作用 | ✅(可选,`d&0x10000`)| ❌ |
| 入口 | `bullet_vm_exec` opcode `0x8000000` | 4 case 各调 `allocate_new_laser(type,&frame)` |
| 共享 | vtable[3] INIT、`g_laser_mgr`、bullet.anm 图集 | 同 |

> **🟡 frame 字段映射细节**:上面"shooter 字段→frame 偏移"的逐项对应是子 agent 扫的(子 agent 一手指令级,主控字节级核了 opcode/类/case 起点 ✅;**字段→frame 的具体偏移未逐条复核**,沿用 🟡 标级。需要时按 case_start 地址跟进。)
> **🟡 Inner 结构**:`LaserInfiniteInner`(0x378 字节)已 th-re-data 命名,其余 3 类(`LaserLineInner`/`LaserCurveInner`/`LaserBeamInner`)只在 case 里以匿名 frame 出现,th-re-data 未提供 clear helper(可能只是不需要,memset 即可)。

## 7. 管理器全图(✅ 全部一手 + th-re-data,2026-06-13 收口)

`g_laser_mgr` = `DAT_004a6ee0`(ExpHP `LASER_MANAGER_PTR`)。字段图、init / tick / dtor 全链路:

| 偏移 / 函数 | 内容 | 实证 |
| --- | --- | --- |
| **`LaserManager::initialize` `0x431330`** | 预加载 **`bullet.anm`**(`AnmManager::preload_anm(7,"bullet.anm")` → `mgr+0x604`,**激光精灵 = 子弹精灵图集,不是单独 laser.anm**);`UpdateFunc::new(on_tick_1b)` + `register__on_tick(prio 0x1b)` → `mgr+4`;`UpdateFunc::new(LAB_00431720)` + `register__on_draw(prio 0x23)` → `mgr+8`;`mgr+0x5e0=mgr+0xc`(ring 头哨兵)。 | ✅ |
| **`LaserManager::on_tick_1b` `0x4316b0`** | **暂停门 thunk**(注册的就是它):读 `GAME_THREAD_PTR+0x88`,bit0/bit0x400→跳过;bit2→暂时 `GAME_SPEED=0` 调 body 再恢复;否则直接调 body。 | ✅ |
| **`LaserManager::on_tick_1b__body` `0x431510`** | ★ **真正的每帧 tick**。遍历 `mgr+0x14` 活动链,每节点按 flags 路由:`node[4]==1 ∣∣ (node[3]&6)≥4` → DEATH(vtable[6] destroy + 解链 + `ecl_free_runcontext`=heap free);`(node[3]&8)==0` → vtable[4] frame_tick(返 0 推时步、返非 0 也 DEATH);else → vtable[13](1) 仅碰撞模式。 | ✅ |
| **`LaserManager::allocate_new_laser(type,&frame)` `0x431760`** | 统一分发器(见 §3a):cap 512 + 句柄 + `switch(type)` 4 个 ctor + 链入 ring + `vtable[3](&frame)` INIT。 | ✅ |
| `LaserManager::destroy_all` `0x42cb00` | 走活动链全部释放(供 dtor 用)。 | ✅ ExpHP |
| **`laser_mgr_dtor` `0x4313b0`** | ★ **析构**(早期被我误标为 tick,已纠正):释 `mgr+4/+8` 两 UpdateFunc + `destroy_all` + `g_laser_mgr=0`。 | ✅ |
| 字段:`mgr+4` | tick UpdateFunc handle(prio 0x1b) | ✅ |
| 字段:`mgr+8` | draw UpdateFunc handle(prio 0x23) | ✅ |
| 字段:`mgr+0xc..` | ring 哨兵区 | ✅ |
| 字段:`mgr+0x14` | **活动链表头**(on_tick 遍历的就是它) | ✅ |
| 字段:`mgr+0x5e0` | ring 最新头(新建从这里压进) | ✅ |
| 字段:`mgr+0x5e4` | 存活数(cap `0x200`=512) | ✅ |
| 字段:`mgr+0x5e8` | 句柄序(wrap 到 `0x10000`) | ✅ |
| 字段:`mgr+0x604` | **bullet.anm 预加载 handle** | ✅ |
| 字段:`mgr+0x608` | 擦弹累计 | 🟡(agent) |

**优先级 0x1b** 把激光 tick 在主循环里钉在 **Enemy(0x1a)和 Bullet(0x1c)中间**(`run_all_on_tick`,见 `engine/_shared/frame-loop.md`)——这条信息也补到 main-loop doc 了。

## 8. 开放 / 待挖

1. ✅ **曲线/beam 激光生成路径** 已结案(本会话 2026-06-13):**仅 ECL** → `allocate_new_laser(type=2/3, &frame)`,4 个 ECL opcode 各在 `EnemyData__ecl_run_over_300` 内(`0x421AC1`/CA1/E05/F8F)。逐 opcode 的 ECL 号 + 参数布局正由子 agent 扫,结果入 §6a。曲线段内部更新链 `0x437ee0/0x438370` 仍未亲反 🟡。
2. **graze 计分公式**(`0x434010`):增量 `+0x608` 的具体算法(随宽/色/缩放)未化简。🟡
3. **`0x443af0` 里 `DAT_004945e0/0049449c/0049471c`** 撑大自机判定框的标量未 PE 实测(≈1/1/−1)。🟡
4. **`playershot_tick_laser_idx2`(`0x446260`)** 是**自机激光**,与敌激光是两套,未碰。
5. ✅ **每激光"ECL 上下文"假说已证伪**:`ecl_free_runcontext` 实为 heap free 14 字节包装,激光是 vtable-driven、无内嵌字节码 VM(见 §3a 澄清)。
6. **`LaserManager::on_tick_1b__body` 里 vtable[6] DEATH 槽**:LaserLine 槽 6 是 `ret 0`(03 §2),说明它不需要额外清理,仅靠 `destroy_all`/`ecl_free_runcontext` 就够;但 LaserCurve 槽 6 = `0x437760`(实做事)未反 🟡。
7. **`LaserManager::on_tick_1b` 的 bit 2 是不是慢动作模式**:thunk 在 bit 2 时把 GAME_SPEED 归零调 body 再恢复,语义待核(对照 `__ptr_GAME_SPEED_MULT_FROM_ECL` 用法应能定)。🟡

## 关联
- 生成来源:`01-core-engine.md` §3 opcode `0x8000000` + §8 EX_LASER。
- 弹 VM 模型(激光是其 spawn 的产物):`02-bullet-vm-model.md`。
- ECL 参数:`engine/ecl/th16/ECL-info.md`(EX_LASER 段,已交叉验证)。
- 落盘脚本:`tooling/ghidra/scripts/apply_th16_bullet_names.py`。
</content>
