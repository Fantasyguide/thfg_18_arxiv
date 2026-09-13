# TH18 `pl0X.sht` — 文件布局、装载解析、shooterset 索引分配

> **版本**：TH18 v1.00a（`th18.exe`，imagebase `0x400000`）。本文裸地址默认属该版本；引用其他版本须写成 `th16:0x…`。
>
> 一手：2026-09-05，Ghidra（ghidra-re MCP）反编译 `Player__initialize` `0x45a7a0` / `Player__tick_shooting_state` `0x45ea00`
> + 本地逐字节解析 `local/th18.v1.00a/dat/pl00–pl03.sht`（四文件 268 条 shooter 全扫）。两侧互相印证。

## 0. 一句话结论

一个 `pl0X.sht` 里装的**不只是自机主炮**：`+0xe0` 是一张 **40 项**的 shooterset 偏移数组，
其中 `0x00`–`0x09` 是主炮（火力档 × 聚焦），**`0x0a`–`0x16` 是 13 组「装备卡子机」的弹幕**，
**`0x17`–`0x27` 是 17 个空位**。四个角色的文件在这一层完全同构。

## 1. 装载：`Player__initialize` `0x45a7a0` ✅

```c
buf = load_file(sht_name_table[DAT_004cccf8 + DAT_004cccf4]);   // 名字表 0x4b7088：pl00/01/02/03.sht
player + 0x47940 = buf;                                          // 之后所有取表都从这里出发
```

- 文件名表 `0x4b7088`（4 × `char*`），下标 = `0x4cccf8`（自机 B/A 之类）+ `0x4cccf4`（角色）。
- **跨局缓存**：`0x570920` 非 0 时直接接管它当 `player+0x47940` 并把该全局清零（`0x45a7cf`），
  这条路**跳过下面整个解析**——所以解析每次开局只做一遍。
- 装载后立刻被改写的两个头字段：`sht[0x24] := 100`（`0x45a968`），随后
  `0x4ccd3c := sht[0x24] * sht[0x20]`（= 最大火力 400）、`0x4ccd40 := sht[0x24]`（= 每档火力 100）。

## 2. 布局

| 区 | 偏移 | 内容 | 证据 |
| --- | --- | --- | --- |
| 头 | `+0x00` u16 | `5` = 火力档数 + 1 | 四文件同值 |
| | `+0x02` u16 | **`40` = 偏移数组长度**（解析循环的上界）| `0x45a813` `CMP AX,[EDX+0x2]`、`0x45a892` |
| | `+0x10`–`+0x1f` | 四个 float 移动速度：装载时 `× 0x4b9310` 取整写进 `player+0x477a4+i` | `0x45a940`–`0x45a960` |
| | `+0x20` i32 | `4` = `pwr_lvl_cnt` | `0x45a978` |
| | `+0x24` i32 | 文件里是 `40`，**装载时被写成 100** = 每档火力 | `0x45a968` |
| | `+0x28` i32 | **`max_dmg`**：Reimu 90 / Marisa 160 / Sakuya 60 / Sanae 120 | 见 §5 |
| option 位置块 | `+0x40`–`+0xdf` | `0xa0` 字节（与 `th13` 同尺寸；本文不展开）| 逐字节 |
| **偏移数组** | `+0xe0`–`+0x17f` | 40 × u32，**用了 23 项** | §3 |
| shooterset 数据 | `+0x180` 起 | shooter stride `0x5c`，组间 4 字节 `0xFFFFFFFF` | §3、[`02`](02-shooter-record.md) |

`+0x180` 是**硬编码常量**（`0x45a830` `LEA EAX,[EDX+0x180]`），不是从头部算出来的——
也就是说偏移数组的 40 项长度是格式写死的，不可变。

## 3. 解析循环 `0x45a830`–`0x45a89c` ✅

```c
for (i = 0; i < *(u16*)(buf + 0x02); i++) {              // 40 次
    *(int*)(buf + 0xe0 + i*4) += buf + 0x180;            // 偏移 → 绝对指针（就地改写）
    for (row = *(char**)(buf + 0xe0 + i*4); *(i8*)row >= 0; row += 0x5c) {
        row[0x2c] = table_0x4b4230[row[0x2c]];           // func_on_init
        row[0x30] = table_0x4b4210[row[0x30]];           // func_on_tick
        row[0x34] = table_0x4cf414[row[0x34]];           // func_on_draw（在 .data，运行时填）
        row[0x38] = table_0x4b41f0[row[0x38]];           // func_on_hit
    }
}
```

### ★ 一条承重不变式：空位靠「`0 → 0` 幂等」活着

尾部 17 个空位的值是 `0`，`+= buf + 0x180` 后**全都指向 shooterset 0**，于是 set 0 的
func 字段会被**重复解析 17 次**。这本该炸（第二遍拿已经解析出来的函数指针当下标去查表）——
retail 不炸，是因为：

- set 0 的四个 func 索引在四个文件里**全是 0**（逐字节确认）；
- 四张表的 `[0]` 项都是 NULL：`0x4b4230[0] = 0`、`0x4b4210[0] = 0`、`0x4b41f0[0] = 0`（直读 exe）。

于是 `0 → table[0] → 0`，重复多少次都一样。

> **给改 sht 的人**：往空位里填东西是安全的（填了就只解析一次），但
> **shooterset 0 的四个 func 字段必须保持 0**，否则剩下的空位会把它二次解析成野指针。

## 4. shooterset 索引分配 ✅

主炮的选择在 `Player__tick_shooting_state` `0x45ea00`：

```c
idx = CURRENT_POWER / 0x4ccd40;                 // 火力档 0..4（每档 100）
if (player + 0x476cc /* 聚焦 */) idx += 1 + sht[0x20];   // +5
row = *(char**)(sht + 0xe0 + idx*4);
```

装备卡的选择是**逐卡烘死的立即数**，见 [`02`](02-shooter-record.md) §3 与
[`../../card/th18/08-catalog.md`](../../card/th18/08-catalog.md) §D。合起来：

| 索引 | 是谁 | 出处 |
| --- | --- | --- |
| `0x00`–`0x04` | 非聚焦，火力档 0–4 | `0x45ea00` 公式 |
| `0x05`–`0x09` | 聚焦，火力档 0–4 | 同上 |
| `0x0a` | REIMU_OP（8）| `CardReimu1__on_shoot` `0x40ab00` 的 `PUSH 0xa` ✅本会话 |
| `0x0b` | MARISA_OP（10）| 调用点 `0x40ad2e` |
| `0x0c` / `0x0d` | SAKUYA_OP（12），按聚焦换表 | 调用点 `0x40af80` |
| `0x0e` | SANAE_OP（14），组内 2 条 | 调用点 `0x40b19e` |
| `0x0f` | ALICE_OP（17），锁敌 | `CardAlice__on_shoot` `0x40b4e0` ✅本会话 |
| `0x10` | CIRNO_OP（18）| 调用点 `0x40bb3e` |
| `0x11` | YOUMU_OP（16）| 调用点 `0x40b3ce` |
| `0x12` | REIMU_OP2（9）| 调用点 `0x40ac1e` |
| `0x13` | MARISA_OP2（11）| 调用点 `0x40ae3e` |
| `0x14` | SAKUYA_OP2（13），组内 2 条 | 调用点 `0x40b08e` |
| `0x15` | SANAE_OP2（15）| 调用点 `0x40b2ae` |
| `0x16` | ❓ **无对应卡** | §6 |
| `0x17`–`0x27` | **空位（值 0）** | 逐字节 |

> **证据分级**：`0x0a` 与 `0x0f` 两行本会话直接反汇编到立即数（✅）；其余 11 行的「索引 ↔ 卡」
> 取自 [`../../card/th18/08-catalog.md`](../../card/th18/08-catalog.md) §D（前一会话的一手），
> 本会话只核了「这 11 个调用点确实都调 `Player__tick_shooters_for_ability_card` `0x40a9c0`」
> （`get_xrefs_to` 共 11 个调用点，与 §D 的 11 张卡一一对应）。

**13 组卡子机在四个角色的文件里位置一一对应**（索引相同、组内条数相同），差的只是弹的数值——
所以卡里烘死的索引对四个自机都成立，这也是它能烘死成立即数的原因。

## 5. 四个文件的差异

| 文件 | 大小 | `+0x28` max_dmg | 主炮组内条数（set 0–4）|
| --- | --- | --- | --- |
| `pl00.sht`（Reimu）| 6456 | 90 | 2/3/4/5/6 |
| `pl01.sht`（Marisa）| 5536 | 160 | 2/3/4/5/6 |
| `pl02.sht`（Sakuya）| 7192 | 60 | 2/4/6/7/10 |
| `pl03.sht`（Sanae）| 7376 | 120 | 2/4/6/8/10 |

`+0x28` = `max_dmg` 的交叉印证：`GameThread__on_tick` `0x443d3b` 每帧用 `sht+0x28` 复位
`player+0x47984`（敌人侧每帧伤害上限，`enm_compute_damage_sources` `0x45f0f0` 拿它钳），
青眼白龙（card-expand id 67）实跑日志里打出来的 `cap` 就是这个值。

## 6. 开放

- ❓ **索引 `0x16` 没有任何一张原生卡对得上**：组内 1 条，`fire_rate = 0`（真被 `% fire_rate`
  除会崩）、`speed = 0`、`angle = 0`、`dmg = 16`、`func_on_init = 7`。像是留着没用完的槽，
  或由某条不走 `tick_shooters_for_ability_card` 的路径使用。**别碰它。**
  *验法*：`get_xrefs_to` 四张 func 表的第 7 项（`0x4613b0`），看谁引用。
- 🟡 option 位置块（`+0x40`–`+0xdf`，`0xa0` 字节）的三角填充规则未逐字段核对；本仓暂时用不上。
