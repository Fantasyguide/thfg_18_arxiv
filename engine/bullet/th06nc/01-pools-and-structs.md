# TH06NC 敌弹子系统：两个池与两个结构体

> **版本**：TH06NC（`th06nc.exe`，MD5 `b0cc28ec904e9efce2439ad857c938b5`，imagebase `0x140000000`）。
> 本文裸地址默认属该版本；引用原版须写成 `th06:0x…`。

状态：池的骨架 ✅ 一手且**三方自洽**；`Bullet` 31 个字段、`Laser` 21 个字段全部 `code`。
底本是 CC0 [`th06-decomp`](https://github.com/GensokyoClub/th06) 的 `BulletManager.hpp/cpp`——
**两边结构体都重排过**，所以只借"有哪些字段"，偏移一律自己读。

> 本文的反汇编与反编译全部来自 **MCP 直查 Ghidra 库**，不是 HTML 导出（导出会滞后于 apply）。

---

## 1. 池的骨架

`g_BulletManager` = `0x1403ec2a0`（ECL 那一轮独立得到，见
[`../../ecl/th06nc/04`](../../ecl/th06nc/04-enemy-struct.md) §2）。

| 偏移 | 内容 | 来源 |
| --- | --- | --- |
| `+0x00000` | `timer`（`ZunTimer`，8 字节） | `*param_1 = param_1[1]; param_1[1]++` |
| `+0x00008` | **`bullets[640]`**，步长 **`0x620`** | `pfVar17 = param_1 + 2`；`+= 0x188` 个 float |
| `+0xf5008` | `bulletCount`（i32） | `param_1[0x3d402]`，每帧清零后按活跃弹累加 |
| `+0xf500c` | ❓ 12 字节未知 | — |
| `+0xf5018` | **`lasers[64]`**，步长 **`0x298`** | 见 §1.2 |

### 1.1 三方自洽

```
0x08 + 640 × 0x620            = 0xf5008   ← 正好是 bulletCount 的位置
0x1403ec2a0 + 0xf5018         = 0x1404e12b8 ← 正好是 SpawnLaserPattern 里的立即数
laser 游标 0xf502c − 0xf5018  = 0x14      ← 于是 OnUpdate 的 [+0x260] = laser+0x274，
                                            与 SpawnLaserPattern 的 in-use 判据同址
```

三条互相独立，任何一个数取错都会立刻塌。前两条已写成 `validate()` 的断言。

### 1.2 激光池的立即数是直接读出来的

`BulletManager__SpawnLaserPattern` `th06nc:0x1400105e0` 的找空闲槽循环：

```
14001060a  LEA RBX,[0x1404e12b8]          ; 池基址
140010611  CMP byte ptr [RBX + 0x274],0x0 ; inUse
14001061c  ADD RBX,0x298                  ; 步长
140010623  CMP EAX,0x40                   ; 64 条
140010781  MOV byte ptr [RBX + 0x274],0x1 ; 占用
```

### 1.3 与 TH06 的差异

| | TH06 | TH06NC |
| --- | --- | --- |
| `sizeof(Bullet)` | `0x5c4` | **`0x620`** |
| `bullets[]` 数量 | 640 | **640（没变）** |
| `sizeof(Laser)` | `0x270` | **`0x298`** |
| `lasers[]` 数量 | 64 | **64（没变）** |
| `BulletManager` 开头 | `bulletTypeTemplates[16]`（`0x5600`） | **`ZunTimer`，随后直接就是 `bullets`** |

**数量都没变，结构体都变大了。** 模板数组在 NC 里不在开头 ❓ 位置未定位。

## 2. `Bullet`（步长 `0x620`）

`BulletManager__OnUpdate` `th06nc:0x140010870` 里 `switch (state)`，与 CC0 的
`BULLET_STATE_*` 同构：`0` 未用、`1` 已发射、`2/3/4` 三档出现动画、`5` 消失中。

三档出现动画各有自己的 `AnmVm` 与速度分母：

| state | `AnmVm` 偏移 | 分母常量 |
| --- | --- | --- |
| 2 | `+0x170` | `th06nc:0x14030cdd8` |
| 3 | `+0x290` | `th06nc:0x14030cdf0` |
| 4 | `+0x3b0` | `th06nc:0x14030ce10` |

### 字段

| 偏移 | 字段 | 证据 |
| --- | --- | --- |
| `+0x000` | `speed` | 各分支末尾取它乘 cos/sin 写进 `velocity` |
| `+0x004` | `dirChangeInterval` | 与 `timer.current` 一起算 `(numTimes+1)*interval` |
| `+0x008`–`+0x010` | `velocity` | `= cos/sin(angle) * speed` |
| `+0x014` | `exFlags`（u16） | 测过 `0x1/0x10/0x20/0x40/0x80/0x100/0x400/0x800` |
| `+0x01c` / `+0x020` | `dirChangeNumTimes` / `MaxTimes` | 到达上限就清掉对应 `exFlags` 位 |
| `+0x024` | `currentSpeed` | 转向时从 `speed` 刷新；斜坡阶段按比例插值 |
| `+0x028` / `+0x02c` | `timer.previous` / `.current` | 循环末尾 `previous = current; current++` |
| `+0x030`–`+0x038` | `pos` | `pos += velocity`；`CalcKillBoxCollision` 的第二实参 |
| `+0x040` | `angle` | cos/sin 的输入；出界反射时按象限折叠 |
| `+0x044` | `state`（i16） | switch 的分支变量 |
| `+0x048` | `dirChangeEndTime` | 与 `timer.current` 比较 |
| `+0x050` | `sprite`（`AnmVm`） | `AnmManager__ExecuteScript(g_AnmManager, bullet+0x50)` |
| `+0x150` | `spriteDesc`（指针） | 取 `+0x1c`/`+0x20` 当半高/半宽做出界判定 |
| `+0x5f4` / `+0x5f8` | `grazeSize` | `CalcKillBoxCollision` 的第三实参 |
| `+0x600` | `dirChangeRotation` | 每次转向累加到 `angle` |
| `+0x604` | `offscreenFrames`（u16） | 出界累加，**超过 `0xff` 才真回收** |
| `+0x608`–`+0x610` | `ex4Acceleration` | `exFlags` 位 `0x10`：`velocity += 它`，再 `atan2` 更新 `angle` |
| `+0x614` | `angleDelta` | 转向时加到 `angle`（或直接赋值） |
| `+0x618` | `isGrazed`（u8） | 擦过一次后置 1，之后不再判定 |

### 回收有两道门

出界不会立刻回收：`offscreenFrames` 要累到 **超过 `0xff`**（且 `exFlags & 0xdc0 == 0`）
才 `memset(bullet, 0, 0x620)`。一旦回到界内就清零重来。

## 3. `Laser`（步长 `0x298`）

| 偏移 | 字段 | 偏移 | 字段 |
| --- | --- | --- | --- |
| `+0x004` | `width` | `+0x264` | `flags`（u16） |
| `+0x008`/`+0x00c` | `timer` | `+0x268` | `hitboxStartTime` |
| `+0x010` | `startOffset` | `+0x26c` | `speed` |
| `+0x014` | `endOffset` | `+0x270` | `duration` |
| `+0x018` | `despawnDuration` | `+0x274` | `inUse`（u8） |
| `+0x020` | `vm0`（`AnmVm`） | `+0x278`–`+0x280` | `pos` |
| `+0x140` | `vm1`（脚本号 `0x21c`） | `+0x284` | `color`（u16） |
| `+0x260` | `angle` | `+0x288` | `startLength` |
| | | `+0x28c` | `startTime` |
| | | `+0x290` | `state`（u8） |

判据两条最硬：`startOffset` 被 `endOffset − startLength` 追着走（`+0x010`、`+0x014`、`+0x288`
三者的关系与 CC0 同式），`inUse` 由 `SpawnLaserPattern` 与 `OnUpdate` **两处独立指向同一偏移**。

`flags` 位 0 选两种消失方式：按宽度收缩（改 `vm0.scaleY`）还是按 alpha 淡出。

## 4. 顺带验证了三条旧结论

这一轮读到的代码同时印证了别处已经记下的东西——都不是重新推导，是交叉命中：

1. **擦弹逻辑确实内联在这里**（[`../../player/th06nc/01`](../../player/th06nc/01-main-loop-and-collision.md) §2.8）：
   `g_GrazeInStage < 99999`、`g_Score += 500`、音效 `0x1e`、粒子 id 8，全在这个循环里。
2. **擦弹半径的算式对上了**（同上 §2.9）：`min(grazeSize.x, grazeSize.y) × 0.5 + 20 + hitboxRadius`,
   其中 `hitboxRadius` 读的是 `th06nc:0x140506aec` = `g_Player + 0x774c` ✅。
3. **`CalcKillBoxCollision` 的第四个参数确实是形状选择**（同上 §2.7）：
   这里传 `1`（圆对圆），而 `EnemyManager__OnUpdate` 传 `0`（圆对盒）。
   两处取值不同，正好证实它是选择器而不是常量。

还有第四条：`ZunTimer` 在 `Bullet`（`+0x028`）、`Laser`（`+0x008`）、`BulletManager`（`+0x000`）
里**都是 8 字节**，这是该结论的第五、六、七次独立印证
（[`../../player/th06nc/01`](../../player/th06nc/01-main-loop-and-collision.md) §2.13）。

## 5. 未解

1. ❓ `BulletManager + 0xf500c` 的 12 字节。
2. ❓ `bulletTypeTemplates[16]` 在 NC 里的位置（TH06 在结构体开头，NC 不在）。
3. ❓ `Bullet` 里 `+0x054`–`+0x14f`、`+0x1a0`–`+0x28f` 等大段仍未拆（多为 `AnmVm` 内部）。
4. ❓ `exFlags` 各位的完整语义：本文只标出被测过的 8 位，未逐位对照 CC0 的 `exFlags`。

## 6. 复现

```sh
# MCP（推荐）：open_database 指向 local/th06nc/th06nc.exe，会复用我们的工程
#   decompile_function BulletManager__OnUpdate
#   disassemble_function 0x1400105e0
python3 -c "import sys; sys.path.insert(0,'tooling/ghidra'); \
  import th06nc_bullet_spec as s; print(s.validate())"
```
