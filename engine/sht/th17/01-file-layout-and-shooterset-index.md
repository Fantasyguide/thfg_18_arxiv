# TH17 `pl0X?.sht` — 文件布局、装载解析与 shooterset 索引（含 TH17 `0x58` / TH18 `0x5c` 的 4 字节差异）

> **版本**：跨版本（TH17 v1.00b / TH18 v1.00a）。本文出现的地址一律带 `th17:` / `th18:` 前缀。
> **方法**：TH17 一手反汇编（`th17:0x4463f0` 起的 `Player__initialize` 解析循环）+ 全部六个 `pl0X?.sht` 逐字节解析；
> TH18 侧布局与字段引用现有 [`../th18/01-file-layout-and-shooterset-index.md`](../th18/01-file-layout-and-shooterset-index.md)、
> [`../th18/02-shooter-record.md`](../th18/02-shooter-record.md)。
> **可信度**：布局 / 解析器 / 索引语义 ✅；字段差异映射 ✅（有六文件逐字节对拍）；未在咆哮组使用的 TH17 表项标 🟡/❓。

## 0. 一句话结论

TH17 的 `pl0X?.sht` 在文件 `+0xe0` 有 **20 项** u32 偏移数组，`+0x130` 是数据基址，shooter stride **`0x58`**；
一组 shooter 以首字节 `fire_rate` 的符号位结束。索引 `0..9` 是主炮，`10..14` 是狼咆哮，`15..19` 是鹫咆哮。
TH17 记录与 TH18 记录只差 **4 字节**：TH18 在 `+0x28` 插入 `+0x28..+0x2b`（含 `fire_rate2`/`start_delay2`），
所以 TH17 `+0x28` 起的字段整体搬到 TH18 的 `+0x2c` 起，TH17 `+0x48` 对应 TH18 `+0x4c` 的横移倾斜系数。

## 1. 样本与解析器一手证据

- **① 发现**：`th17:0x4463f0` 的 `Player__initialize` 从 `th17:0x4a2128`/`th17:0x4a214c` 的文件名表取 `pl0X?.sht`，
  用 `th17:0x4020d0` 读入后写入 `player+0x1903c`。
- **② 推测**：`player+0x1903c` 是本次加载的 `.sht` 基址，后续所有偏移都从这里出发。
- **③ 验证**：`th17:0x446480` 是 `lea eax,[edx+0x130]`；`th17:0x446486` 是 `add dword ptr [esi+edx],eax`；
  `th17:0x44648f` 从 `[esi+edx]` 取出组指针；`th17:0x4464a0` 起把 shooter 的 func 索引就地替换成函数指针。
  自写解析器对六个文件按同一公式解析，所有组都能在终止符处整洁结束（见 §6 哈希）。
- **④ 结果**：解析器 = 偏移数组就地加 `base+0x130`，再逐条 shooter 解析；✅ 仅 TH17 v1.00b。
- **⑤ 证据**：`local/th17.v1.00b/th17.exe`、`th17:0x4463f0`、`th17:0x446480`、`th17:0x446486`、
  `th17:0x446492`、`th17:0x4464a0`–`th17:0x4464d7`。

## 2. 文件布局

| 区 | 文件偏移 | 内容 | 证据 |
| --- | --- | --- | --- |
| 头 | `+0x00` u16 | `5` = 火力档数 + 1 | 六文件同值 |
| | `+0x02` u16 | **`20` = 偏移数组长度** | `th17:0x44646b` 读 `[edx+0x2]` 当上界 |
| | `+0x04`–`+0x3f` | 移动速度 / option 位置等头数据 | 本次不展开 |
| 偏移数组 | `+0xe0`–`+0x12f` | 20 × u32，装载时逐项加 `base+0x130` | `th17:0x446480` `lea eax,[edx+0x130]` |
| shooter 数据 | `+0x130` 起 | stride **`0x58`**；组间 4 字节 `0xFFFFFFFF` 终止符 | `th17:0x4464a3` `lea ecx,[ecx+0x58]`；`th17:0x446492` / `th17:0x4464d7` |

## 3. 解析循环（反汇编摘要）

```asm
446480  lea eax,[edx+0x130]          ; data_base
446486  add dword ptr [esi+edx],eax  ; offs[i] += data_base
44648f  mov ecx,[esi+edx]            ; set pointer
446492  cmp byte ptr [ecx],0         ; fire_rate 符号位?
446495  jl  4464e2                   ; <0：本组结束
4464a0  mov eax,[ecx+0x28]           ; func_on_init index
4464a3  lea ecx,[ecx+0x58]           ; stride 0x58
4464a6  mov eax,[eax*4+0x4a0fc0]     ; init table
4464ad  mov [ecx-0x30],eax           ; 回写 init
...                                   ; tick @0x2c / draw @0x30 / hit @0x34
```

> ① 发现：`th17:0x4464a0` 的四个字段是 `+0x28/+0x2c/+0x30/+0x34`。
> ② 推测：TH17 的 func 字段比 TH18 早 4 字节。
> ③ 验证：六个文件都能用 `0x58` 解析；终止符只在首字节 `<0` 时出现。
> ④ 结论：TH17 stride `0x58`，func 字段 `+0x28/+0x2c/+0x30/+0x34` ✅。
> ⑤ 证据：`th17:0x4464a0`–`th17:0x4464d7` 与 `engine/sht/th17/02-hyper-shot-groups.md` 的解码表。

## 4. shooterset 索引分配

`Player__do_shooting` `th17:0x44a710` 的索引公式：

- 平时：`idx = 火力档(0..4) (+5 若聚焦)`；
- 咆哮且 `hyper_type == 1`（狼）：`idx = 火力档 + 10`；
- 咆哮且 `hyper_type == 3`（鹫）：`idx = 火力档 + 15`；
- 聚焦与否在咆哮分支中不改变索引。

`th17:0x4b5abc` = `hyper_type`，`th17:0x4b5ac4` = `HYPER_FLAGS`（bit1 = 咆哮中）。
所以 `10..14` 与 `15..19` 分别覆盖火力档 0..4；其中 `set10`/`set15` 是空组（只有 4 字节终止符），
`set11..14`/`set16..19` 才有实体记录。逐字节点：见 `02-hyper-shot-groups.md`。

## 5. ★ TH17 `0x58` vs TH18 `0x5c`：那 4 字节到底在哪

**结论：插入点在 TH18 `+0x28`，不是文件头，也不是每条 shooter 的尾部。**
TH18 在 `+0x28..+0x2b` 多了 4 字节（`+0x28`/`+0x29` 是填充/未知，`+0x2a` = `fire_rate2`，`+0x2b` = `start_delay2`）；
TH17 记录没有这 4 字节，因此 TH17 `+0x28..+0x57` 的所有字段整体对应 TH18 `+0x2c..+0x5b`：

| TH17 偏移 | TH18 偏移 | 字段 |
| --- | --- | --- |
| `+0x00`–`+0x27` | `+0x00`–`+0x27` | rate/delay/damage/偏移/命中尺寸/角度/速度/slot/mode/anm/sfx/hit_fx；一一对应 |
| `+0x28` | `+0x2c` | `func_on_init`（索引，装载后变指针） |
| `+0x2c` | `+0x30` | `func_on_tick` |
| `+0x30` | `+0x34` | `func_on_draw`（零售全 0） |
| `+0x34` | `+0x38` | `func_on_hit` |
| `+0x38` | `+0x3c` | 咆哮/主炮用到的额外 float/int 段（TH17 pl01 的 Marisa 主炮这里非 0） |
| `+0x48` | `+0x4c` | **横移倾斜系数**（TH18 `init3` 唯一读的尾部字段） |
| `+0x54`–`+0x57` | `+0x58`–`+0x5b` | 最后 4 字节；TH17 咆哮记录全 0 |

**一手对拍（Marisa 聚焦主炮 set6）**：TH17 `pl01a.sht` 的 `+0x38 = 0x32`、`+0x3c = 0x0f`；
同一组功能在 TH18 `pl01.sht` 里是 `+0x3c = 0x32`、`+0x40 = 0x0f`、`+0x38` 已经是 `func_on_hit` 索引 `3`。
这正是 `+4` 搬移的硬证据。

## 6. 空组与逐文件对拍

- `set10` / `set15` 的偏移指向 4 字节 `ff ff ff ff`，即空组。
- `set11`/`12`/`13`/`14` 四组逐字节相同；`set16`/`17`/`18`/`19` 四组逐字节相同。
- 同一角色的 `a/b/c` 三个 shot type 在咆哮组也逐字节相同；下表哈希覆盖六个文件。

| 文件 | 大小 | WOLF set11（=12/13/14）SHA-256 前 16 | EAGLE set16（=17/18/19）SHA-256 前 16 |
|---|---:|---|---|
| `pl00a.sht` | 9008 | `dcad6ee4709c55bd` | `2c95dd5ad034ee9a` |
| `pl00b.sht` | 9008 | `dcad6ee4709c55bd` | `2c95dd5ad034ee9a` |
| `pl00c.sht` | 10064 | `dcad6ee4709c55bd` | `2c95dd5ad034ee9a` |
| `pl01a.sht` | 7424 | `68b974cfcccd820a` | `9394da4a316e6c55` |
| `pl01b.sht` | 7424 | `68b974cfcccd820a` | `9394da4a316e6c55` |
| `pl01c.sht` | 7424 | `68b974cfcccd820a` | `9394da4a316e6c55` |
| `pl02a.sht` | 12440 | `93da34224e26882e` | `fad1ff6941f19806` |
| `pl02b.sht` | 12000 | `93da34224e26882e` | `fad1ff6941f19806` |
| `pl02c.sht` | 13760 | `93da34224e26882e` | `fad1ff6941f19806` |

复现：本文件的表由脚本按 `stride 0x58`、`data_base +0x130` 独立解码；哈希取「首记录到终止符后 4 字节」。

## 7. 证据与复现

- 反汇编：`local/th17.v1.00b/th17.exe`；`th17:0x4463f0`、`th17:0x446480`、`th17:0x446486`、`th17:0x446492`、`th17:0x4464a0`。
- 解析输入：`local/th17.v1.00b/dat/pl00a.sht` … `pl02c.sht`。
- 索引：`th17:0x44a710`、`th17:0x4b5abc`、`th17:0x4b5ac4`。
- 相关：[`02-hyper-shot-groups.md`](02-hyper-shot-groups.md)、[`../th18/01-file-layout-and-shooterset-index.md`](../th18/01-file-layout-and-shooterset-index.md)、
  [`../th18/02-shooter-record.md`](../th18/02-shooter-record.md)。
