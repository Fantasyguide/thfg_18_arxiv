# TH06NC 玩家主循环与碰撞/擦弹判定

> **版本**：TH06NC（`th06nc.exe`，MD5 `b0cc28ec904e9efce2439ad857c938b5`，imagebase `0x140000000`）。
> 本文裸地址默认属该版本；引用其他版本须写成 `th06:0x…`。

状态：5 个函数已定位 ✅ 一手；两处**未解**明确标出（见 §5）。

ECL 那套 opcode 坐标系在这里用不上——玩家主循环是 chain 回调（**没有直接 xref**），
碰撞/擦弹是从弹幕主循环里调的。换了三个锚。

---

## 1. 三个锚

| 锚 | 怎么用 |
| --- | --- |
| **输入位图** | `0x1400127d0` 生成的逻辑按钮位图，bit0=射击 / bit1=炸弹 / bit2=低速 / bit4–7=方向。全库同时测这几类位的函数**只有一个** |
| **调用侧画像** | TH06 的 `BulletManager::OnUpdate` 调 `CheckGraze` / `CalcKillBoxCollision` / `CalcLaserHitbox`；NC 的对应函数也正好调三个同邻域函数 |
| **参数与内容** | `CalcLaserHitbox` 的 6 个参数逐个吻合；炸弹裁剪循环与 CC0 逐行同构 |

## 2. 已定位

| 地址 | 名字 | 依据 |
| --- | --- | --- |
| `0x140068820` | `Player__OnUpdate` | 炸弹按下沿、低速标志写 `p+0x785c`、8 分支移动 switch、射击位；**调用者 0 个**（chain 回调特征） |
| `0x140010870` | `BulletManager__OnUpdate` | 调用三个玩家碰撞入口 + `AnmManager__ExecuteScript` + `EffectManager__SpawnParticles` |
| `0x140068160` | `Player__AddedCallback` | 加载玩家 ANM；写出生位置、判定半径、道具吸取尺寸 |
| `0x14006a980` | `Player__CalcKillBoxCollision` | 先炸弹裁剪，命中则喷粒子 + 置死亡态 + 放音效 **4 (PICHUN)** |
| `0x14006a8c0` | `Player__BombProjectileClipsBox` | 遍历 `p+0x7754`，跳过 x==0 项，半宽 AABB。**我们自己起的名** |
| `0x14006aba0` | `Player__CalcLaserHitbox` | 签名 `(center, size, rotation, angle, canGraze)` 与 CC0 逐参吻合 + `cosf`/`sinf` |

结构体偏移（都是从指令直接读出）：

| 偏移 | 字段 |
| --- | --- |
| `+0x7754` | `bombProjectiles`（数组，x==0 表示未用） |
| `+0x785c` | `isFocused` |

## 2.5 玩家坐标在哪读

**`g_Player = 0x1404ff3a0`**（由弹幕 tick 的调用点给出：
`LEA RCX,[0x1404ff3a0]` / `CALL Player__CheckGraze`）。
坐标在 **`p+0x7730`**，绝对地址 **`0x140506ad0`**（x / y / z 三个 f32）。

**写侧**只有主循环：

```
1400693a0  ADDSS XMM6,[RDI + 0x7730]     ; 每帧位移累加
1400693b0  MOVSS [RDI + 0x7730],XMM6
1400693cd  MOVSS [RDI + 0x7730],XMM1     ; 边界钳制
```

**读侧有 18 个函数**用绝对地址读它——玩家自己的 tick 反而不在其中（它走 `param_1+ 偏移`）。
最有意思的一处是 **ECL VM 内联的 `Player::AngleToPlayer`**：

```
1400222c8  MOVSS XMM0,[0x140506ad4]   ; 玩家 y
1400222d0  SUBSS XMM0,[RDI + 0xb4]    ; − 敌机 y
1400222d8  MOVSS XMM1,[0x140506ad0]   ; 玩家 x
1400222e0  SUBSS XMM1,[RDI + 0xb0]    ; − 敌机 x
140022310  CALL atan2f
```

与 CC0 的 `atan2(positionCenter.y - pos->y, positionCenter.x - pos->x)` 逐项吻合 ✅。
这段同时**独立印证了敌机 `position = enemy+0xb0`**
（[`../../ecl/th06nc/04`](../../ecl/th06nc/04-enemy-struct.md) 从别的路径得到过同一结论）。

ECL 还把三个分量各当一个伪变量（`0x1400222a4` / `b0` / `bc` 三个 `LEA`）。

### 出生坐标：与 TH06 **完全一致**（此处有一次误报，已撤回）

`0x140068dc5` 起的常量初始化：

| 路径 | TH06（CC0） | TH06NC |
| --- | --- | :---: |
| 初次出生 `Player__AddedCallback` | (192, 384, **0.49**)　`Player.cpp:98-100` | (192, 384, **0.49**) ✅ |
| 死亡复活 `Player__OnUpdate` | (192, 384, **0.2**)　`Player.cpp:241-243` | (192, 384, **0.2**) ✅ |

x/y 说明关卡区仍是 384×448。

> ⚠️ **这里我误报过一次**：只看主循环里的 `0.2` 就去和 CC0 的**初始化**值 `0.49` 对比，
> 报成「NC 改了出生 z」。实际是**拿 NC 的复活路径比 CC0 的初始化路径**——
> 两边都是初始化 0.49、复活 0.2，没有差异。周边也逐条吻合：
> 30 帧无敌判定（NC 的 `CMP ECX,0x1e`）与 `scaleX/Y = 3.0` 都在。
> **教训：比对前先确认两边处在同一条代码路径上。**

已定位的玩家字段：

| 偏移 | 字段 | 依据 |
| --- | --- | --- |
| `+0x7730` | `positionCenter`（3×f32） | 主循环读改写；18 个函数按绝对地址读 |
| `+0x7750` | `playerDirection` | 移动 switch 里写 0..8 |
| `+0x774c` | `hitboxRadius` = **1.25** | 被弹与擦弹判定共用 |
| `+0x7898` | `playerState` | 枚举值与 TH06 相同 |
| `+0x7754` | `bombProjectiles` | `0x14006a8c0` 遍历，跳过 x==0 |
| `+0x785c` | `isFocused` | `= (input & 4) != 0` |

## 2.7 ★ 被弹判定：TH06 是方形，NC 是**圆形**

`Player__CalcKillBoxCollision` 的核心是：

```c
fVar14 = *(float *)(param_1 + 0x774c) * *(float *)(param_1 + 0x774c);   // r²
fVar9  = dy*dy + dx*dx;                                                 // 距离²
if (r² >= 距离²) → 命中
```

TH06 完全不是这样。它的 `Player` 有**三个真字段** `hitboxTopLeft` / `hitboxBottomRight` /
`hitboxSize`（`Player.hpp:213-217`），判定是轴对齐盒对盒。
**NC 里这三个字段不存在**，也找不到任何 `positionCenter ± hitboxSize` 的计算——
只有 `+0x774c` 一个标量半径。

### 数值没变，形状变了

`Player__AddedCallback` 的 `MOV qword ptr [RBX+0x774c], 0x3fa00000` = **1.25f**，
与 TH06 的 `hitboxSize.x = 1.25` **同值**。所以 NC 的圆**内接**于 TH06 的方，四角被削掉：

| | TH06 | TH06NC |
| --- | --- | --- |
| 形状 | 轴对齐盒，半宽 1.25 | **圆，半径 1.25** |
| 面积 | `(2×1.25)² = 6.25` | `π×1.25² ≈ 4.909` |
| 相对 | — | **小约 21%** |

`param_4` 选择比较方式：`0` = 圆对轴对齐盒（把玩家中心钳制到弹幕盒内再算距离），
`1` = 圆对圆。TH06 只有盒对盒一种。

### 范围：只有被弹判定变圆

道具吸取仍是 AABB——`+0xa2ac = +0x9f2c + +0x7734`（`grabItemSize` 与 `position` 相加），
且 `grabItemSize = (12.0, 12.0, 5.0)` 与 TH06 逐值相同。

## 2.8 擦弹逻辑被内联进了弹幕主循环

按音效 id 分（CC0 枚举：4 = `SOUND_PICHUN`，30 = `SOUND_GRAZE`）：

| 函数 | 入队的音效 | 结论 |
| --- | --- | --- |
| `0x14006a980` | 只有 **4** | 纯死亡判定 = `CalcKillBoxCollision` |
| `0x14006aba0` | **4 + 30** | 激光判定，擦弹与死亡都管，与 CC0 的 `CalcLaserHitbox` 一致 |
| `0x140010870` | **30** | **弹幕 tick 自己放擦弹音**——`CheckGraze` / `ScoreGraze` 被内联进去了 |

> ⚠️ **我一度把 `0x14006a980` 命名成 `Player__CheckGraze` 并发布了。**
> 它放的是音效 4 而不是 30，是死亡不是擦弹。
> **音效 id 这个判据比函数大小和调用顺序都硬。**

## 2.9 擦弹判定：也是圆，而且复用同一个半径字段

`BulletManager__OnUpdate` 里内联的那段（`0x1400112b5` 一带）：

```c
// 弹幕中心 (cx, cy) = pfVar9[0xc], [0xd]；尺寸 (sx, sy) = pfVar9[0x17d], [0x17e]
// 1) 先用「弹幕盒各向外扩 20」做炸弹裁剪
BombProjectileClipsBox(&g_Player, cx-sx*0.5-20, cy-sy*0.5-20,
                                  cx+sx*0.5+20, cy+sy*0.5+20);
// 2) 擦弹半径
r = min(sx, sy) * 0.5 + 20.0 + g_Player[+0x774c];
// 3) 圆判定
if ((py-cy)² + (px-cx)² < r²) { 计数++；喷粒子；分数 +500；音效 30 }
```

`+0x774c` 就是被弹判定用的那个半径字段——**两种判定复用同一个值**。

### 与 TH06 逐项对照

| | TH06（CC0 `CheckGraze` + `ScoreGraze`） | TH06NC |
| --- | --- | :---: |
| 弹幕盒外扩 | 20.0 | **20.0** ✅ |
| 判定形状 | AABB 对 AABB | **圆对圆** |
| 死亡/出生时跳过 | `playerState == DEAD \|\| == SPAWNING` | `!= 1 && != 2` ✅ |
| 放炸弹时不计数 | `bombInfo.isInUse == 0` | `g_BombInUse == 0` ✅ |
| **关卡内擦弹上限** | **9999** | **99999** ❌ |
| 总擦弹上限 | 999999 | **999999** ✅ |
| 每次分数 | +500 | **+500** ✅ |
| 粒子 | `(玩家+弹幕)/2`，效果 8，1 个，白 | **完全相同** ✅ |
| 音效 | `SOUND_GRAZE` | **30** ✅ |

**唯一的数值差异是关卡内擦弹上限被抬高了 10 倍**（9999 → 99999）。
其余全部逐项一致——变的还是形状。

### 顺带解出的全局

| 地址 | 含义 |
| --- | --- |
| `0x1404ff0cc` | `g_GrazeInStage`（上限 99999） |
| `0x1404ff0d0` | `g_GrazeInTotal`（上限 999999） |
| `0x1404f2798` | `g_Score` |
| `0x140509268` | `g_BombInUse` |

以及 **`g_Player + 0x7898` = `playerState`**，枚举值与 TH06 的
`ALIVE/SPAWNING/DEAD/INVULNERABLE = 0/1/2/3` **逐值相同**。

> ⚠️ 本文早前把 `+0x773c` 标成 `playerState` —— **错的**（它被写成 8，不是合法状态值）。
> 真正的 `playerState` 在 `+0x7898`。`+0x773c` 是什么仍未知。

## 2.10 移动速度：四个字段，斜向是**算出来的**

> 这一节是做 [`mouse-control`](../../../mods/th06nc/mouse-control/README.md) 时补的——
> 那个 mod 要读「本帧的合法步长」，发现本文当时并没有写过速度字段。

`Player__OnUpdate` 的 8 分支移动 switch（`th06nc:0x1400690bc` 的跳转表）里，
每个分支都是「按不按低速，二选一」：

```
140069129  SETNZ AL                        ; AL = (input & 4) != 0
14006912c  MOV byte ptr [RDI+0x785c],AL    ; -> isFocused
140069132  TEST EDX,EDX
140069134  JZ 0x140069143
140069136  MOVSS XMM6,[RDI + 0x7864]       ; 低速 -> +0x7864
140069143  MOVSS XMM6,[RDI + 0x7860]       ; 常速 -> +0x7860
```

四个斜向分支不读这两个，改用 `CMOVZ` 在另一对之间选：

```
1400691c8  MOV EAX,0x786c                  ; 低速斜向
1400691cd  MOV ECX,0x7868                  ; 常速斜向
1400691d2  CMOVZ EAX,ECX                   ; ZF=1（没按低速）-> 0x7868
1400691d5  MOVSS XMM6,[RAX + RDI*0x1]
```

| 偏移 | 字段（CC0 名） | 值来自 |
| --- | --- | --- |
| `+0x7860` | `characterData.orthogonalMovementSpeed` | 速度表 |
| `+0x7864` | `characterData.orthogonalMovementSpeedFocus` | 速度表 |
| `+0x7868` | `characterData.diagonalMovementSpeed` | `+0x7860 / sqrtf(2)` |
| `+0x786c` | `characterData.diagonalMovementSpeedFocus` | `+0x7864 / sqrtf(2)` |

### 斜向速度是初始化时现算的

`th06nc:0x14006835e` 一次 `MOVUPS` 从速度表读 16 字节整块写进 `+0x7860`，
四个字段先都拿到表里的原值；随后 `+0x7868/+0x786c` 被除以 `sqrt(2)` 覆盖掉：

```
14006835e  MOVUPS XMM0,[RAX + RBP*0x1 + 0x3de3b0]   ; 表项 = 32 字节
140068366  MOVUPS [RBX + 0x7860],XMM0
140068387  SQRTSS XMM0,XMM2                          ; XMM2 = 2.0
1400683ef  DIVSS XMM1,XMM0                           ; +0x7860 / sqrt(2)
14006840c  MOVSS [RBX + 0x7868],XMM1
140068414  MOVSS [RBX + 0x786c],XMM2
```

CC0 的 `Player.cpp:115-116` 是同一个式子：

```cpp
p->characterData.diagonalMovementSpeed = p->characterData.orthogonalMovementSpeed / sqrtf(2.0);
p->characterData.diagonalMovementSpeedFocus = p->characterData.orthogonalMovementSpeedFocus / sqrtf(2.0);
```

而 `Player.cpp:694-728` 那个 `isFocused ? …Focus : …` 加符号翻转的 switch，
与上面反汇编出的八个分支**逐分支同构** ✅。

### 速度表：`th06nc:0x1403de3b0`，步长 32 字节

索引 = `shotType + character * 2`（`th06nc:0x1404f1e80` = 角色，`0x1404f1e81` = 机型）：

| 索引 | 角色 | 正交 | 低速 |
| --- | --- | --- | --- |
| 0 / 1 | 灵梦 A / B | 4.0 | 2.0 |
| 2 / 3 | 魔理沙 A / B | 5.0 | 2.5 |

**与 TH06 逐值相同** ✅，且同角色两个机型共享速度（TH06 也如此）。
量纲关：低速正好是常速的一半，斜向 = 4/√2 ≈ 2.83，都落在合理范围。

> 表项 32 字节里只用掉了前 16。后 16 字节（`+0x3de3c0`）被写到 `player+0x7870`，
> 四个角色都是 `{2.104, 0, 2.104, 0}` ❓ 含义未知，本文不下结论。

## 2.11 自机可动区域：钳位边界与坐标系确认

做 [`mouse-control`](../../../mods/th06nc/mouse-control/README.md) 时需要「自机坐标的合法范围」，
顺手把这条钉死了。`Player__OnUpdate` 在写完位置之后立刻钳位：

```
1400693c0  MOVSS XMM1,[0x1404ff0e0]      ; movementArea.topLeft.x
1400693cb  JBE  ...                      ; pos.x < 左界 -> 取左界
1400693df  MOVSS XMM0,[0x1404ff0e8]      ; movementArea.size.x
1400693e7  ADDSS XMM0,XMM1               ; 右界 = 左界 + 宽
1400693fb  MOVSS XMM1,[0x1404ff0e4]      ; .topLeft.y  （同样的四步）
14006941a  MOVSS XMM0,[0x1404ff0ec]      ; .size.y
```

| 地址 | 含义 |
| --- | --- |
| `th06nc:0x1404ff0e0` | `playerMovementAreaTopLeftPos.x` |
| `th06nc:0x1404ff0e4` | `playerMovementAreaTopLeftPos.y` |
| `th06nc:0x1404ff0e8` | `playerMovementAreaSize.x` |
| `th06nc:0x1404ff0ec` | `playerMovementAreaSize.y` |

四个都在 `.data`，**静态初值为 0，运行时才填**——直接读文件会得到 0，别上当。
写入方没找到（应是整块 `memcpy`，地址不以 RIP 相对形式出现）❓。

### 取值：与 TH06 逐值相同 ✅（由运行期行为反推）

静态读不到值，但**钳位行为本身泄露了它**。mod 的日志里有一条目标被钳住的样本：

| 客户区 | 光标 x | 未钳的目标 x | 日志里的目标 x |
| --- | --- | --- | --- |
| 1280×720 | 872 | 442.7 | **376.0** ← 被钳 |

即 `topLeft.x + size.x = 376.0`。对照 CC0 `GameManager.cpp:290-293`：

```cpp
mgr->playerMovementAreaTopLeftPos.x = 8.0;    // 8 + 368 = 376 ✅
mgr->playerMovementAreaTopLeftPos.y = 16.0;
mgr->playerMovementAreaSize.x = 368.0;
mgr->playerMovementAreaSize.y = 416.0;
```

**`8 + 368 = 376`，与 NC 实测的钳位值一致** ✅。同一处还给出 TH06 的
`arcadeRegionTopLeftPos = (32,16)`、`arcadeRegionSize = (384,448)`。

> 这条是**运行期观测**得到的（一个样本），不是静态一手，所以只钉住了 x 的右界；
> 另外三个量按「与 TH06 相同」推定，标 🟡。mod 不依赖推定值——它每帧读真值。

旁证：`th06nc:0x140036260`（timeline 的敌人生成）用
`rand() * [0x1404ff0e8]` 生成随机出生 x，即它确实是**关卡区宽度**，
与自机坐标同一坐标系 ✅。

### 坐标系与 TH06 完全一致 ✅

`Player__AddedCallback` 的出生坐标是**硬编码立即数**：

```
14006822e  MOV dword ptr [RBX + 0x7730],0x43400000   ; 192.0f
140068238  MOV dword ptr [RBX + 0x7734],0x43c00000   ; 384.0f
140068242  MOV dword ptr [RBX + 0x7738],0x3efae148   ; 0.49f
```

对照 CC0 `Player.cpp:98-100`：`positionCenter.x = arcadeRegionSize.x / 2`（384/2 = **192** ✅）、
`.y = arcadeRegionSize.y - 64`（448-64 = **384** ✅）、`.z = 0.49` ✅。
三个值逐个对上 —— **`positionCenter` 是关卡区局部坐标，原点在关卡区左上角，
x ∈ [0,384]、y ∈ [0,448]，与 TH06 相同**。

### ⚠️ 但屏幕布局与 TH06 **不同** —— NC 把关卡区居中了

坐标系相同不代表屏幕映射相同。按 TH06 的 `GAME_REGION_LEFT = 32` 去反推光标位置，
自机会落在光标**右侧约 200px**。实测（[`mods/th06nc/mouse-control`](../../../mods/th06nc/mouse-control/TARGET.md) §6
在 1280×720 与 1920×1080 各标定一次）解出 NC 的布局：

| | TH06 | TH06NC |
| --- | --- | --- |
| 虚拟屏 | 640 × 480 | 高固定 **480**，宽随窗口宽高比扩展 |
| 缩放 | — | `s = 客户区高 / 480` |
| 关卡区水平位置 | 左边界固定 `x = 32`，右侧留 HUD | **水平居中** |
| 关卡区顶边 | `y = 16` | `y = 16`（相同） |
| 关卡区尺寸 | 384 × 448 | 384 × 448（相同） |

三重验证：`s = ch/480` 在两组上精确命中（1.5000 / 2.2500）；两个分辨率反推出的
左边界一致（虚拟 235.33 / 235.11，居中值 234.67）；模型预测 1920×1080 下偏右
216px，与实际观测吻合。

> 🟡 这是**从实测数据拟合**的，不是从二进制读出的常量。对应的
> `arcadeRegionTopLeftPos` ❓ **在 NC 里没找到**——很可能压根不存在，
> 关卡区的定位由 D3D 视口/投影矩阵完成。两组样本都是 16:9。

## 3. ⚠️ 顺序推断在这里翻了车

第一版我按「地址顺序 = 源码顺序」把 `BulletManager__OnUpdate` 调的三个函数
对到 TH06 的 `CalcKillBoxCollision` / `CheckGraze` / `CalcLaserHitbox`——
大小趋势（185 / 533 / 960 对 432 / 477 / 1180）看着也支持。

**内容一验就塌了**：`0x14006a8c0` 根本不是 `CalcKillBoxCollision`，
而是 CC0 **内联在 `CheckGraze` 里**的炸弹裁剪循环。

> **教训**：跨版本对照里，地址顺序和函数大小都只能提出候选，**判定必须靠内容**。

## 4. 一个反常方向：NC 在这里反而拆了函数

平常的规律是 NC 内联得比 TH06 多（[`../../ecl/th06nc/03`](../../ecl/th06nc/03-opcode-crossmatch.md) §4：
`EnemyEclInstr::GetVar` 被内联进 25 个 handler）。但炸弹裁剪循环相反——
CC0 把它写在 `CheckGraze` 里，NC 把它**抽成了独立函数** `0x14006a8c0`。

所以「NC 内联更多」是**统计倾向，不是规则**，别拿它当推断前提。

## 5. 未解

1. ✅ ~~多出来的 `char` 参数~~ —— 已解：它选圆对盒（0）还是圆对圆（1），见 §2.7。
2. ✅ ~~`ScoreGraze` 没定位到~~ —— 已解：擦弹整体被内联进弹幕 tick，见 §2.8。
3. ✅ ~~hitbox 数值找不到~~ —— 已解。当初搜不到的原因很实在：这些常量是
   **代码里的立即数**（`MOV dword ptr [...], 0x41400000`），不在浮点常量池里，
   扫 `.rdata` 永远扫不到。
4. ✅ ~~擦弹用的是圆还是盒~~ —— 已解，见 §2.9：也是圆，半径
   `min(弹宽,弹高)/2 + 20 + 1.25`，复用被弹判定那个半径字段。
5. ❓ **`arcadeRegionTopLeftPos` 在哪**：关卡区在窗口里的定位。实测布局已解出
   （§2.11：宽高比自适应 + 水平居中），但**代码里找不到对应常量**，
   推测由 D3D 视口/投影矩阵完成，未验证。
6. ❓ **`+0x773c` 是什么**：init 与死亡判定都写 8，曾被误标成 `playerState`。
7. — 射击、Bomb 的具体逻辑（`FireBullet*` 四个自机型、`BombData`）本文没碰。

## 6. 复现

```sh
source tooling/env.sh
python3 tooling/ghidra/apply_th06nc_player.py --dry-run   # functions=5
tooling/ghidra/serve.sh th06nc 6092                       # 0x140068820 = 主循环
tooling/ghidra/serve.sh th06.v1.02h 6094                  # 0x4288c0 / 0x427860 = 对照
```
