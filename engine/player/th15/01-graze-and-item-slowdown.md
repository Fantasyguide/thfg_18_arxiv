# th15 擦弹：计数、擦弹特效、以及「道具减速」 — 一手

> **版本**：TH15 v1.00b（`th15.exe`，md5 `bddd41a586e09e4447f795a14009d68a`，imagebase `0x400000`）。
> 本文裸地址默认属该版本；引用其他版本须写成 `th18:0x…`。
> **方法**：Ghidra 12.1.3 反编译 + ExpHP `th-re-data`（结构体/常量名）+ 反汇编交叉；
> `effect.anm` 用 `thdat -x` + `thanm -l` 解出。
> **可信度**：计数与减速链路 ✅；子弹染色 ✅；「全屏橙色滤镜」= **负结论** ✅（见 §5）。

## 0. 一句话结论

TH15 的擦弹落点是 `Player::do_graze` `0x4573a0`：加两个计数、加 100 点 PIV、
在擦弹点放一团**橙色加法混合火花**（`effect.anm` script 24）、弹一行擦弹数的浮字。
**「道具减速」不是 do_graze 做的**——四个擦弹判定点各自在调完 do_graze 之后
把 `ItemManager` 里的 `lolk_item_slowdown_factor` 打成 **0.3**，道具 tick 每帧读它当位移乘数，
而它自己每帧 **+0.1** 往 1.0 爬回去。

## 1. `Player::do_graze` `0x4573a0` ✅

参数 = 擦弹点的坐标（`[esp+4]` 指向 zFloat2）。函数体只做四件事：

| 动作 | 地址 | 细节 |
| --- | --- | --- |
| `GRAZE_TOTAL += 1` | `0x4573ce` | 目标 `0x4e741c`，封顶 `0x5f5e0ff`（99,999,999） |
| `GRAZE_IN_CHAPTER += 1` | `0x4573d9` | 目标 `0x4e7420`，同一封顶 |
| `CURRENT_PIV += 100` | `0x4573f5` | 目标 `0x4e7434`，被 `MAXIMUM_PIV` `0x4e743c` 夹住 |
| 放火花 + 弹字 | `0x457456` 起 | 见 §2 / §3 |

> 一次擦弹 = **100 PIV**。章节奖结算还会再往 PIV 里加一坨（[chapter/th15/01 §2a](../../chapter/th15/01-chapter-and-bonus.md)）。

### 1b. 擦弹判定：两种模式（与 TH18 逐常量相同）✅

| 模式 | 用它的子弹 | 擦弹条件 | 边距常量 |
| --- | --- | --- | --- |
| **圆形** | `flags & 0x10` | `d² < (r + max(br/2.5, **40.0**))² + br²`，`r` = 自机判定圆半径（SHT） | `th15:0x4cff28`=2.5、`th15:0x4cffb8`=40.0（`th15:0x455d00`） |
| **矩形** | 其余 | 弹**中心** vs `PLAYER->__hurtbox` 外扩 **24.0** | `th15:0x4cffa0`=24.0（`th15:0x455be0`） |

- **圆形模式的 40 px 边距就是那个「额外的圈」**（常见圆弹 br≈4–8 → 有效擦弹半径 ≈ 43–50 px），
  比矩形的 24 px 大得多。TH18 的对应函数 `0x45cd90` / `0x45cc20` 用的是**同样的常量**
  （`0x4b921c`=2.5、`0x4b92c4`=40.0、`0x4b92a4`=24.0）——两作判定几何一致。
- 第三个函数 `th15:0x455e10` 是**旋转矩形（OBB）**判定（激光/旋转弹用），擦弹边距 = 弹自身尺寸。
- 差别只在「同一颗弹能擦几次」：TH15 有 per-bullet 计时器 `bullet+0x1464`
  （擦后置 `0x1e`=30 帧，`th15:0x419cc5` 的 `idiv` 余数门）→ 能反复擦；
  TH18 擦后置 `bullet+0x20 |= 4` 一生一次（[`../th18/02`](../th18/02-graze-and-item-slowdown.md) §1b）。

### 1c. 擦弹/减速的「圈」美术 ✅

TH15 `effect.anm` 的 **`eff_sloweffect.png`**（128×64 = 两个 64×64 sprite：左半实心爆点、
右半**空心环**，亮环半径 ≈22 px）由脚本 **25 / 26 / 27** 使用；起 VM 的是
`Player::on_tick` 里的 `th15:0x4540e0`（`0x45425b` 起 script26=0x1a、`0x4548b2` 起 script27=0x1b），
VM 句柄存在 `player+0x16224` / `+0x16228`，可见性由 `player+0x16240`（= 有敌 && 动画帧 ≥4 && 低速位）
与 `player+0x16230`（只读、全 exe 无写者 → 那条分支是死的）控制。

`script27`（那个环，1:1 参数）：

```
-1: sprite(sprite24); layer(6); type(1); blendMode(2);
    alpha(0);  alphaTime(30, 0, 96);          # 30 帧从全透明淡入到 96/255
    scale(4.3f, 4.3f); scaleTime(12, 4, 2.0f, 2.0f);   # 12 帧由大变小
    angleVel(0, 0, 0.017453292f);             # 慢自转
 0: stop(); interruptLabel(1); alphaTime(12, 0, 0);   # 被打断 → 12 帧渐隐
+12: delete();
```

- **叠法是 `blendMode(2)` = 反向减算** `dst − src×alpha`（`th15:0x47d840` case 2：
  SRCBLEND=SRCALPHA、DESTBLEND=ONE、BLENDOP=REVSUBTRACT）→ 这个圈是**暗环**，不发光。
  完整表见 [`anm/th15/01`](../../anm/th15/01-blend-modes.md)。
- 出现 = 淡入 + 由大变小；消失 = 直接渐隐（`script25/26` 那对用的是左半的实心爆点，消失是缩到 0）。

## 2. 擦弹火花 = `effect.anm` script **24** ✅

`AnmManager__allocate_vm` `0x489460` → `AnmLoaded__copy_vm(effect_anm, id=0x18)` `0x40b880`
（`id` 是**文件内 script 全局序号**，不是 entry 序号）→ 位置写在擦弹点 → 插 world list。

`local/th15.v1.00b/anm/effect/effect.anm.txt` script24 一手：

```
sprite(sprite7); layer(18); type(1); blendMode(1); alpha(128);
color(255, 192, 128);            # 橙
scale(0.7f, 0.7f); scaleTime(12, 1, 0.0f, 0.0f);
0: circlePosRand(%F1, %F2, 48.0f, 64.0f); posTime(12, 4, %F1, %F2, 0.0f);
+12: delete();
```

`blendMode(1)` = 加法混合。**这是 TH15 擦弹视觉里唯一的橙色来源**（§5）。

## 3. 擦弹浮字 ✅

`PopupManager::generate_small_score_popup` `0x45ef60`，参数：擦弹点、`GRAZE_TOTAL`、色
`0xffc0c0ff`（A=ff R=c0 G=c0 B=ff，**淡蓝紫**）、音效 `0x2a`（`SoundManager::play_sound_at_position`）。

## 4. 道具减速因子 ✅

### 4a. 因子住在哪

`zItemManager.inner` 的最后一个字段 `lolk_item_slowdown_factor`（float）：
偏移 `+0xe5dee0`（inner 内）→ 绝对 `ITEM_MANAGER+0xe5def0`。

| 动作 | 地址 | 指令 |
| --- | --- | --- |
| setter（ExpHP 误命名为 `get_`） | `0x418bc0` | `movss [ecx+0xe5def0],xmm1; ret`（thiscall，值在 xmm1） |
| 擦弹时置 **0.3** | 见 §4b | `mov DWORD [eax+0xe5def0],0x3e99999a` |
| 每帧恢复 **+0.1**（上限 1.0） | `0x440161`–`0x440183` | `if (1.0 > f) f += 0.1`（`0x4cfe98`=1.0，`0x4cfdf0`=0.1） |
| 清场时复位 **1.0** | `0x421323` | `ItemManager::destroy_all` |
| 当位移乘数用 | `0x43f97b` / `0x43f9d6` | `ItemManager::on_tick__body` `0x43f830` |

道具 tick 里的形状（一手）：

```
vel.x/y/z = item.vel * GAME_SPEED(0x4e73e8) * factor
pos += vel
vel.z    += GAME_SPEED * 0.03 * factor          # 重力项也吃这个因子
```

`0x4cfdc4` = `0.03`（重力步长），`0x4e73e8` = `GAME_SPEED`（1.0）。

### 4b. 打 0.3 的只有「子弹 + 三种激光」，敌机擦弹**不打**

全库一共 5 个 `do_graze` 调用点，但 `mov …,0x3e99999a`（0.3）只有 **4** 处：

| 判定点 | `do_graze` 调用 | 置 0.3 | 结论 |
| --- | --- | --- | --- |
| 子弹（`Bullet::sub_419a50`） | `0x419cd1` | `0x419d05` | ✅ 减速 |
| `LaserLine::method_34` | `0x443821` | `0x443834` | ✅ 减速 |
| `LaserInfinite::method_34` | `0x445cbf` | `0x445cd2` | ✅ 减速 |
| `LaserCurve::method_34` | `0x4487f3` | `0x448888` | ✅ 减速 |
| 敌机（`FUN_00427a10`，未命名） | `0x428031` | — | ❌ **不减速** |

敌机那条分支（`0x428031` 后紧跟的是 `[edi+0x124]` 取 VM、`[ecx+0x53c]` 染色）
只计数、不碰道具因子——所以「贴敌机擦弹」不会让道具慢下来，**只有擦子弹和激光会**。

### 4c. 同一颗子弹的擦弹冷却

`Bullet::sub_419a50` 里擦弹前有 `cmp [edi+0x1440],[edi+0x143c]` + `idiv [edi+0x1464]`：
只有余数为 0 才调 do_graze；擦完在 `0x419da1` 把 `[edi+0x1464] = 0x1e`（30 帧）。
→ **同一颗子弹每 30 帧最多被算一次擦弹**。

### 4d. 擦弹后的子弹染色：**变黄** ✅（2026-09-09 勘误）

擦弹分支内 `th15:0x419d3d`–`0x419d58`（`Bullet::sub_419a50`）：

```
[bullet+0x564] = 0x80                       ; B = 128
[bullet+0x565] = min((0x68 - [bullet+0x1454]) * 2, 0xf0)   ; G，随计时器递减而升
[bullet+0x566..0x567] = 0xffff              ; R = 255, A = 255
```

D3DCOLOR 在内存里是 **BGRA** 字节序（`+0x564`=B、`+0x565`=G、`+0x566`=R、`+0x567`=A），所以
颜色 = `RGB(255, G, 128)`，G 从 0 涨到 240 —— 从**品红**扫到**亮黄**，也就是玩家看到的
「被擦到的弹幕变黄」。染色只在 `[bullet+0x1454] < 0x2d`（45）时写，计时器归零后恢复原色。

> ⚠️ 勘误：本文 2026-09-09 早些时候把这三行读成了「淡青 → 白」（漏了 BGRA 字节序），
> 实际是**品红 → 亮黄**。TH18 没有这一步（擦后只置 `bullet+0x20 |= 4`，不改颜色）。

## 5. 负结论：「全屏橙色滤镜」不存在 ✅（evidence-complete）

用户体感里的「擦弹时蒙上一层橙色滤镜」，在 TH15 v1.00b 里**找不到全屏实现**：

1. **anm 层**：`local/th15.v1.00b/anm/` 全部 27 个 anm 里，橙色 `color(255, 1xx, …)` 只出现
   **一处**——`effect.anm` script24（§2 的擦弹火花）。没有全屏大小的橙色 quad
   （`scale ≥ 10` 的只有 effect script 里的几根光柱）。
2. **减速因子层**：全库读 `ITEM_MANAGER+0xe5def0` 的地方只有道具 tick 与它自己的恢复
   （`0x43f97b`/`0x43f9d6`/`0x43f9fc`/`0x440161`），**没有任何渲染代码读它**——
   所以滤镜不是「减速时给屏幕上色」。
3. **D3D 层**：`Supervisor::enable_d3d_fog` `0x44d5f0` / `disable_d3d_fog` `0x44d630` /
   `sub_44d670` 三个函数**全库零调用方**（`-x` 实测），没有雾/全屏染色路径。

→ 结论：TH15 擦弹的橙色 = **一堆橙色加法火花叠在屏幕上**的观感；
逐弹染色是**青色**；道具减速本身**没有**任何屏幕后处理。

> 如果后面在实机上仍能看到明显全屏偏色，优先怀疑：显卡驱动/录屏的色调、或
> 「分辨率 mode 2」下的另一套 anm（`ascii_960.anm` / `ascii_1280.anm`）——
> 这两套本次只解了 `ascii.anm`，**未逐条比对** 🟡。

## 6. 与 TH16/TH18 的关系

- TH16 有同构的 `player_graze` `th16:0x444cf0`（`GRAZE`/`GRAZE_IN_CHAPTER` 各 +1、
  同一个 anm id 0x18 火花、同一个 `PopupManager` 浮字、同一套 `ItemManager__spawn_item`），
  见 [player/th16/01 §2c](../th16/01-hit-life-system.md)——**引擎那套是跨版本残留**。
- TH16 的 `player_graze` **没有**「置道具减速因子 0.3」这一步：`lolk_` 前缀说明这个因子是
  TH15 特有（LoLK = Legacy of Lunatic Kingdom）。TH18 有没有，**未验** 🟡。
