# item 逆向 01：TH11 道具类型表、对象字段与收集弹窗（`+N`）

> **版本**：TH11 v1.00a（`th11.exe`，688,128 B，imagebase `0x400000`）。本文裸地址默认属该版本；引用其他版本须写成 `th10:0x…`。
> **方法**：一手平坦反汇编（产物 `local/_scratch/th11_full.asm`）+ 跳表/字节表原值直读 +
> ExpHP `zItemManager`/`zItem` 结构交叉 + disp32 字节穷举（只当候选发现器）。
> **可信度**：类型跳表、各档数值与颜色、弹窗函数与寿命/衰减 ✅；
> 「11 槽 vs 13 槽」（作者自己标出的自相矛盾）、颜色落点偏移、上飘终点 🟡/❓。

## 0. 一句话结论

TH11 的道具在引擎里是一个**池 + 一张 11 项类型跳表**：`[item+0x468]` = type（1..11），
`[item+0x464]` = state，`[item+0x46c]` = 子型；分派入口在 `ItemManager::on_tick__body` `0x423550`：

```
423a75: mov eax,DWORD PTR [edi+0x468] ; dec eax ; cmp eax,0xa ; ja → 退出
423a89: jmp DWORD PTR [eax*4+0x423f00]        ; ★ 索引 = type − 1
```

**收集时的 `+N` 文本**走一个独立的弹窗管理器：全局指针 `0x4a8ec0`、函数 `0x438440`
（`eax = 数值`，栈参 `(pos, ?, color)`），槽数组 `mgr+0x44c`、stride `0x40`、**寿命 60 帧**、
数值每帧 `−= GAME_SPEED × 1.01` 线性归零、数字逐字节画。全 exe **9 个调用点**（8 个在 `0x423550` 里）。

## 1. 类型跳表与各档语义

跳表原值（11 项，`struct.unpack('<11I')`）：

```
th11:0x423f00 = {0x423b34, 0x423b7c, 0x423c82, 0x423cf6, 0x423c3e, 0x423a90, 0x423d97, 0x423cc4, 0x423cd7, 0x423b34, 0x423cf6}
```

| type | 处理段 | 语义 | 一手数值 |
| --- | --- | --- | --- |
| 1 / 10 | `0x423b34` | 小 P（火力 +1 单位） | `mov eax,1; call 0x420a00`；RANK += `0xc` |
| 2 | `0x423b7c` | **点道具（蓝点）** | 分值公式见 [faith/th11/01 §2](../../faith/th11/01-faith-and-points.md) |
| 3 | `0x423c82` | **绿点（大）** | `add_to_piv(+10000)` = 显示 +100；弹窗绿 |
| 4 / 11 | `0x423cf6` | 大 P | `collect_power(POWER_PER_LEVEL)` = +1.00 火力；RANK += `0x18` |
| **5** | `0x423c3e` | ★ **残机碎片** | `call 0x4209b0`（碎片 +1，**满 5 换 1 命**）；RANK += `0x100` |
| 6 | `0x423a90` | 满 P（把火力补到 `MAXIMUM_POWER`） | `collect_power(ds:0x4a5748)`；RANK += `0xc` |
| **7** | `0x423d97` | ★ **1UP** | `call 0x420950`（残机 +1，钳 9）；RANK += `0x100` |
| 8 | `0x423cc4` | **绿点（小）** | `add_to_piv(+10)` = 显示 +0.1；**另 +10 分**（`0x423ccd call 0x40ceb0`） |
| 9 | `0x423cd7` | **绿点（中）** | `add_to_piv(+1000)` = 显示 +10；弹窗绿 |

> **订正**：`0x420950` = `CURRENT_LIVES(0x4a5718)++`（钳 9）⇒ **type 7 是 1UP**；
> `0x4209b0` = 碎片计数 `0x4a571c` +1、**满 5** 归零并 `call 0x420950` ⇒ **type 5 是命碎**。
> 有一份研究稿把两者对调、并把「5 片换 1 命」写成 10 片（见 [player/th11/01 §4](../../player/th11/01-bomb-and-lifefrags.md)）。

**满火力改写（`0x424333`）**：字节表 `th11:0x424578` = `[0,1,1,0,1,1,1,1,1,0,0]`（下标 = type−1）、跳表 `0x424570`
⇒ **type 1/4/10/11（所有 P 点）在火力满（`≥ MAXIMUM_POWER`）时被改写成 type 9**（绿点·中）。

## 2. 道具对象字段与贴图

| 字段 | 偏移 | 一手 |
| --- | --- | --- |
| type | `+0x468` | `0x423a75: mov eax,[edi+0x468]` |
| state | `+0x464` | `0x42380b: mov DWORD PTR [edi+0x464],0x3`（被收走 = 3） |
| 子型 | `+0x46c` | `ItemManager::spawn_item` 写入段 |
| 位置 | `+0x434` / `+0x438` / `+0x43c` | `0x4239e9: lea esi,[edi+0x434]`（弹窗位置来源） |
| 吸附目标 y | `+0x470` | `0x423805: fstp DWORD PTR [edi+0x470]` |
| 自带 zTimer | `+0x450`…`+0x463` | `0x4242f1`（`current = 0`）/ `0x424301`（`game_speed = &0x4a7948`） |
| 贴图 anm id | = **type + `0x60`** | `0x4243ed: add ebx,0x60`；type 10/11 特例 `0x4243af`(`0x61`)/`0x4243d5`(`0x64`) |
| 池 | `zItemManager.normal_items`（`+0x14`）、`zItem` = `0x478` 字节 | `0x4245ec add eax,0x478` / `0x4245f1 mov edx,0x896`（2198 个） |

## 3. 收集弹窗（`+N`）：`0x438440`

### 3.1 结构与槽

| 项 | 一手 |
| --- | --- |
| 全局指针 | `0x4a8ec0`（`0x438448: mov edi,DWORD PTR ds:0x4a8ec0`） |
| 对象大小 | `0x78c`（`0x437d1e: push 0x78c`） |
| 槽数组 | `mgr + 0x44c`，stride `0x40`（`0x438463: lea esi,[eax+edi*1+0x44c]`，`eax = head << 6`） |
| ring head | `mgr + 0x14`；`cmp [edi+0x14],0xa` 后归零（`0x43844e`/`0x438456`）⇒ **11 个有效槽** |
| busy | `slot + 0x38`（`0x43846a: mov BYTE PTR [esi+0x38],1`） |
| 数字文本 | `slot + 0x00..0x37`（ASCII 逐位**倒序**写入）、位数 `slot + 0x39`；值 < 0 时写 `'\n'`（`0x43851c`） |
| 位置 | `slot + 0x0c / +0x10 / +0x14`（`0x438500`–`0x43850f` 从 `&item->pos` 拷 3 个 dword） |
| 值（float） | `slot + 0x10`；tick 里 `0x43804a fld [esi-0x14]` / `0x438057 fstp [esi-0x14]`（两个基点差 `0x24`） |
| 寿命 | **60 帧**：`0x43809f: cmp DWORD PTR [esi-0x4],0x3c` → `0x4380a5: mov BYTE PTR [esi+0x14],0` |

> ⚠️ **结构上的自相矛盾（如实记录）**：`cmp head,0xa` 说 **11 槽**，而初始化/tick/draw 三个循环都用 **13**
> （`0x437d08 mov ecx,0xc` / `0x43803f mov edi,0xd` / `0x438126 mov [esp+0x20],0xd`）。
> 若真是 13 槽，初始化循环的范围会与 `slot[1..12]` 重叠。⇒ **照抄「11 取模」是安全的**（那是指令原文），
> 但「13 到底是什么」标 🟡。

### 3.2 tick（`0x438020`）与 draw（`0x4380d0`）

```
438021/438028/438030: fld QWORD PTR ds:0x497e60 (0.5) / ds:0x497f28 (0.99) / fld DWORD ds:0x497f24 (1.01f)
43804a: fld [esi-0x14] ; 43804d: fld ds:0x4a7948 (GAME_SPEED) ; 438053: fmul st,st(5)（×1.01）
438055: fsubp st(1),st ; 438057: fstp [esi-0x14]        ; ★ 值 −= GAME_SPEED × 1.01（线性归零）
43809f: cmp [esi-0x4],0x3c ; 4380a5: mov BYTE PTR [esi+0x14],0   ; ★ 60 帧到点 → busy = 0
```

⇒ `+N` 的「跳字」= **显示当前残值**、每帧减 `GAME_SPEED × 1.01`；60 帧后槽释放。
draw 用 `zInterpFloat3`（起点 `slot+0x0c`、终点 `slot+0x30`）做位置插值，比例常量 `0.99`↔`1.01`（≈ ±1% 微动）；
数字逐字节取自槽内 ASCII 区。

### 3.3 逐档颜色（颜色在调用点上是**字面常量**）

| 调用点 | type | `push` 的颜色 | 语义 |
| --- | --- | --- | --- |
| `0x423ab2` / `0x423b56` | 1 / 10 | `0xffffff40` | 火力 +N |
| `0x423bd8` | 2 | `0xffffffff`（白） | 蓝点（交信度**未满**，社区叫「白回収」） |
| `0x423c2e` | 2 | `0xffffff00`（黄） | 蓝点（交信度**已满**） |
| `0x423cb4` | 3 | `0xff00ff00`（绿） | 绿点（大），数字 = `ebx/100` = 100 |
| `0x423ce2` | 8 | `0xff00ff00`（绿） | 绿点（小），数字固定 `0xa` = 10 |
| `0x423d28` / `0x423d8d` | 4 / 11 | `0xffffff40` / `0xffff4040` | 火力（半额 / 无加） |
| —— | 5 / 7 | **不调 `0x438440`** | 命碎与 1UP 都不弹 `+N` 字（1UP 另有提示指针 `0x437d31`/`0x437f16`/`0x437fa5`） |

**触发时机 = 道具进回收框那一帧**（`0x423a36 jnp 0x423dc5` 的 AABB 分支内）：弹窗、加分、加火力同帧完成。
第 9 个调用点 `0x4133ae` 在 `EnemyManager`/`Spellcard` 一带，**不是道具**（❓ 未查）。

> 与 TH18 的对照（可用的部分）：`pos` 字段**完全对得上**（`+0x0c`）；busy 同义（TH11 `+0x38` ↔ TH18 `+0x3c`）；
> stride `0x40` ↔ `0x48`；槽数 11 ↔ 18。**对不上的是颜色槽**（TH11 未定位）与文本字节区。

## 4. 结论表 / 还缺什么

| # | 断言 | 可信度 |
| --- | --- | --- |
| 1 | 类型分派 = `[item+0x468] − 1` 的 11 项跳表 `0x423f00` | ✅ 一手（原值已列） |
| 2 | type 5 = 命碎（`0x4209b0`，5 片换 1 命）、type 7 = 1UP（`0x420950`） | ✅ 一手（含订正） |
| 3 | 满火力时 type 1/4/10/11 → 9 | ✅ 一手（字节表原值） |
| 4 | 弹窗函数 `0x438440`，全局 `0x4a8ec0`，9 个调用点（8 个在道具分发里） | ✅ 一手 |
| 5 | 槽 stride `0x40`、位置 `+0x0c`、busy `+0x38`、位数 `+0x39`、值线性衰减 `×1.01/帧`、**寿命 60 帧** | ✅ 一手（指令原文） |
| 6 | 各档弹窗颜色在调用点上是字面常量：绿点绿、蓝点白/黄、火力黄系 | ✅ 一手（逐条 `push` 字节） |
| 7 | ring 取模 = 11（`cmp head,0xa`），而三个循环用 13 | 🟡 指令原文 ✅ / 语义未解 |
| 8 | 贴图 anm id = `type + 0x60`（type 10/11 特例 `0x61`/`0x64`） | ✅ 一手 |

**还缺什么**

- ❓ 道具 type → 贴图身份的**视觉**确认：`th11.dat` 的 `item.anm` 未解包（`type+0x60` 指向的是子弹管理器里那份 anm，不是 front.anm）。
- 🟡 弹窗**颜色字段的落点偏移**：`0x438440` 全文里找不到第 3 个 push（颜色）的写入点（`[esi+0x14]` 是 `pos.z`），颜色可能在 draw 侧按当前值选。
- 🟡 上飘终点（`slot+0x30` 的 `zInterpFloat3` target）在 `0x438440` 里没有被写，是 tick 里算出来的。
- 🟡 `0x4384f9` 写 `slot+0x1c = 0xffffffff`（「寿命占位」）与 tick 的 60 帧冲突，倾向以 60 为准。
- ❓ `0x4133ae` 那个非道具弹窗的语义。
- ❓ 道具池的**生成口**（`spawn_item` 全签名与调用点清单）本轮没有系统整理 —— 与 [th07/th08 的 `spawn_item`](../OVERVIEW.md) 对照。

## 附录：复现命令

```bash
E=local/th11.v1.00a/th11.exe
objdump -d -M intel --start-address=0x423a70 --stop-address=0x423f40 $E   # 类型分派 + 弹窗调用点
objdump -d -M intel --start-address=0x438440 --stop-address=0x438530 $E   # 弹窗 add
objdump -d -M intel --start-address=0x438020 --stop-address=0x4380d0 $E   # 弹窗 tick（寿命/衰减）
objdump -d -M intel --start-address=0x420950 --stop-address=0x420a00 $E   # 1UP / 碎片兑换
python3 -c "
import struct;d=open('local/th11.v1.00a/th11.exe','rb').read()
g=lambda v,n: d[0x400+(v-0x400000-0x1000):][:n]
print([hex(struct.unpack('<I',g(0x423f00+4*i,4))[0]) for i in range(11)])
print([hex(d[0x8a400+(0x424578-0x400000-0x48b000)+i]) for i in range(11)])"
```
