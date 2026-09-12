# anm/th07 §1 — TH07 的 anm 全局 id 空间：`loadAnm` 给每个 entry 分配的 base 怎么前进

> **版本**：TH07 v1.00b（`th07.exe`，imagebase `0x400000`）。本文所有地址都带 `th07:` 前缀，均为该版本。
> **方法**：一手平坦反汇编（`objdump -d -M intel` 产出 `local/_scratch/th07_full.asm`，172,741 行）
> + exe `.data`/`.rdata` 常量与静态表直读 + 自写 anm v2 解析器逐个 entry 核对 `local/th07.v1.00b/dat/` 全部 63 个 `.anm`
> + `thanm -l 7` 打印脚本原文 + ExpHP `local/vendor/th-re-data/data/th07.v1.00b/`（funcs / type-structs-own）交叉对名。日期 2026-09-12。
> **可信度**：§1、§2、§4 的地址 / 指令 / 数值全 ✅ 一手（含 §4.5 的「第 3 个指针不是 draw」，全 `.text` 三个调用点穷举）；
> §3.1/§3.2 的全量扫描 ✅（自写解析器 + 一手读 loader）；§3.3 的「每个 `.anm` 的 base / index」表 🟡（批量提取，只逐指令复核了其中 4 处）；
> §5 全为**未验**声明，§6 全是未定/未查。

## 0. 一句话结论

TH07 的 anm 不是「每文件一套脚本号」，而是**一个全局 id 空间**：`loadAnm(index, 文件名, base)` 把文件里
每个 entry 的 sprite / script 按 `base + 文件内 id` 铺进 `AnmManager` 的两个大数组，
**base 在每个 entry 之后前进**。这条前进规则是「引擎侧拿到一个全局 id（effect type 表、道具脚本号）
⇒ 反推它落在哪个 `.anm` 的哪个 entry 的第几支脚本」的唯一钥匙。

| 问题 | 答案 | 一手依据 |
| --- | --- | --- |
| 全局 id 从哪来 | `base + 文件内 id`（sprite 与 script **共用同一个 base**，两个数组各自按 id 索引） | 写：`th07:0x44e47f`（script）· 读：`th07:0x450e7b`（sprite） |
| entry 之间 base 怎么前进 | 下一 entry 的 base = 本 entry base + **max(全部 sprite id, 全部 script id) + 1**（有符号比较，下限 0） | `th07:0x44e05e`（加）· `th07:0x44e4c6`–`th07:0x44e4c9`（+1） |
| 常见说法「= 上一 entry 的 sprite 数」 | **只是特例**：该 entry 的 id 恰为 `0..n−1` 连续、且 script id 不超过时才成立 | §3.1 |
| 能不能当通则用 | ❌ 不能：63 个零售 `.anm`、101 个 entry 里 **25 个**前进量 ≠ sprite 数 | §3.1 表 |
| 引擎侧怎么消费 | `EffectManager` 的 type→id 表 `th07:0x49efc0` → id → `AnmManager+0x28ef0+id*4` = 脚本指针 | §4 |

## 1. 规则的一手地址与指令

### 1.1 外循环：entry 链 + base 前进（`th07:0x44df90`）

```
AnmManager::loadAnm(index /*[ebp+8]*/, filename /*[ebp+0xc]*/, base /*[ebp+0x10]*/)   // thiscall, ret 0xc
    ecx = AnmManager（全局单例 ds:0x4b9e44，即在 th07:0x434114 以 calloc(0x17e560) 建的那个）
```

| 地址 | 指令要点 | 语义 |
| --- | --- | --- |
| `th07:0x44df9e` | `call 0x431330`（ExpHP `sub_431330_load_file`） | 读文件 → `[ebp-0x10]` = **首个 entry 指针** |
| `th07:0x44dfa6` | `[ebp-0xc] = 1` | 传给子函数的「首个 entry」标志（第 4 参） |
| `th07:0x44dfad` | `[ebp-0x8] = [ebp+8]` | 记下起始 index（只用于收尾记账） |
| `th07:0x44dff3` | `call 0x44e070`（参数：index, entry, base, flag） | **每个 entry 一次**，返回值 → `[ebp-0x4]` |
| `th07:0x44dffb` | `cmp [ebp-0x4],0 ; jge` | 返回值 < 0 ⇒ 中止整个 load（返回该负值） |
| `th07:0x44e01f` | `[ebp+8] += 1` | **每过一个 entry，anm index 自增 1**（index 是「扁平 entry 号」，不是文件号） |
| `th07:0x44e028` | `cmp [entry+0x38],0 ; jne` | `next_offset`（ExpHP `zAnmRawEntryHeader.next_offset`）为 0 = **链尾** ⇒ 收尾返回 0 |
| `th07:0x44e04e` | `[ebp-0x10] += [entry+0x38]` | 走下一个 entry |
| **`th07:0x44e05b`–`0x44e061`** | `mov ecx,[ebp+0x10]` / `add ecx,[ebp-0x4]` / `mov [ebp+0x10],ecx` | ★ **base += 子函数返回值**（`add` 在 `0x44e05e`） |
| `th07:0x44e06c` | `ret 0xc` | |

`th07:0x44dfd3` 那个 `mov ecx,1 / test ecx,ecx / je` 是 MSVC 的 `while (1)` 惯用法，出口全在返回语句上。

### 1.2 前进量 = `th07:0x44e070` 的返回值 = `max(所有 id) + 1`

子函数签名（`ret 0x10`，ecx = AnmManager）：`f(index /*[ebp+8]*/, entry /*[ebp+0xc]*/, base /*[ebp+0x10]*/, flag /*[ebp+0x14]*/)`。

| 地址 | 指令要点 | 语义 |
| --- | --- | --- |
| `th07:0x44e07c` | `[ebp-0x4] = 0` | 累加器初值 **0**（所以负 id 永远不会拉低它） |
| `th07:0x44e0d8` | `cmp [entry+0x28],2 ; je` | entry 版本必须 **2**（TH07 只吃 v2） |
| `th07:0x44e0fe` | `[entry+0x8] = index` | ExpHP 名 `rt_textureslot`：文件里是 0，载入时写 index |
| `th07:0x44e2c0` / `th07:0x44e2e1` | `[ebp-0x38] = entry+0x40`；`cmp i,[entry+0x0]` | **sprite 偏移表**在 `entry+0x40`（`[entry+0x0]` = `num_sprites` 项） |
| `th07:0x44e2f5` | `add ecx,[eax]` | 表项是**相对 entry 的偏移**，解引用得 sprite 定义 |
| `th07:0x44e3b8`–`th07:0x44e3c7` | `cmp [ebp-0x4],[sprite_def]` / `jge` / 取大 | 累加器 = max(累加器, **sprite.id**)，`sprite_def+0` = id |
| `th07:0x44e3cf` / `th07:0x44e3d2` | `eax = sprite.id + base`；`cmp eax,0xa00` | 全局 id 上限 **`0xa00` = 2560**（与 ExpHP `zAnmManager.sprites[0xa00]` 吻合） |
| `th07:0x44e403` | `call 0x44e780` | 安装 sprite 到 `AnmManager+0x60 + 全局id*0x40` |
| `th07:0x44e40d` / `th07:0x44e42e` | sprite 表之后紧接 **script 表**；`cmp i,[entry+0x4]` | 表项 8 字节 `{u32 id; u32 offset}`，项数 = `num_scripts` |
| `th07:0x44e459`–`th07:0x44e468` | 同样的取大 | 累加器 = max(累加器, **script.id**) |
| **`th07:0x44e47f`** | `[AnmManager + (base+id)*4 + 0x28ef0] = entry + script.offset` | ★ 全局脚本 id → **脚本指令指针** |
| **`th07:0x44e491`** | `[AnmManager + (base+id)*4 + 0x2b6f0] = base` | ★ 全局脚本 id → **它属于哪个 base**（§4.6 的唯一读者） |
| `th07:0x44e4ac` / `th07:0x44e4bf` | `[AnmManager + index*0xc + 0x2def0] = entry`；`+0x2def4] = base` | 按 index 的载入记录（`+0x2def8` = 子 entry 数，`th07:0x44e03d` 写） |
| **`th07:0x44e4c6`–`th07:0x44e4c9`** | `mov eax,[ebp-0x4]` / `add eax,0x1` | ★ **返回值 = max + 1**（`ret 0x10`） |

⇒ 精确表述：**前进量 = max(该 entry 全部 sprite id ∪ 全部 script id) + 1**（有符号、下限 0；`jge` 是有符号比较，
所以 `title01.anm` 里那批**负 script id** 不参与取大）。写在 `th07:0x44e05e`。

> 「= sprite 数」是它的**特例**：当 sprite id 恰为 `0..num_sprites−1`（连续）**且**没有更大的 script id 时，
> `max(所有 id) = num_sprites − 1` ⇒ 前进量 = `num_sprites`。etama.anm 的 4 个 entry 全部落在这个特例里（§2）。

### 1.3 顺带读到的硬边界（都在同两个函数里）

| 边界 | 检查点 | 违反时 |
| --- | --- | --- |
| anm index < **`0x32`**（50） | `th07:0x44e0a3` `cmp [ebp+8],0x32` | 整个 `loadAnm` 返回 `-1`（不载） |
| entry version == **2** | `th07:0x44e0d8` | 该 entry 返回 `-1` ⇒ `loadAnm` 中止 |
| 全局 id < **`0xa00`**（2560，sprite 与 script **各查一遍**） | `th07:0x44e3d2` / `th07:0x44e43b` | 返回 `-1` ⇒ `loadAnm` 中止 |
| entry 链终点 = `next_offset(+0x38) == 0` | `th07:0x44e02c` | 正常结束（返回 0） |
| entry 有 **0 sprite / 0 script** | 无特判：两个循环直接不进 | 累加器保持初值 0 ⇒ **前进量 = 1**（base 下界每 entry +1） |

**空 entry 没有专门分支** ✅：`num_sprites = num_scripts = 0` 的 entry 也会正常走完、并让 base 前进 1。
零售确实存在这种 entry（如 `face_01_00.anm` entry0：5 sprites / 0 scripts；`ascii.anm` entry1：108 sprites / 0 scripts）。

## 2. 三处交叉印证

### 2.1 `text.anm` 1792 + 36 = `capture.anm` 1828

| 环节 | 一手 |
| --- | --- |
| `text.anm` 载入 base | `th07:0x438c3c` `push 0x700` + `th07:0x438c41` `push "data/text.anm"`（`0x497018`）+ `th07:0x438c46` `push 0` → `th07:0x438c4e` `call 0x44df90` |
| `text.anm` entry0 sprite 数 | **36**（自解析：`num_sprites = 36`，sprite id `0..35` 连续 ⇒ 前进量 36）；1792 + 36 = **1828** |
| `capture.anm` 载入 base | `th07:0x401da1` `push 0x724`（= 1828）+ `th07:0x401da6` `push "data/capture.anm"`（`0x498a14`）+ `th07:0x401dab` `push 4` → `th07:0x401db3` `call 0x44df90` ✅ |

两个字符串地址用只读直读核对过（`data/text.anm` / `data/capture.anm`）。

### 2.2 `etama.anm` 512 → 680 → 730 → 731，接 `eff01.anm` 的 732

`etama.anm` 载入 base = **`0x200`**：`th07:0x426f9c` `push 0x200` + `th07:0x426fa1` `push "data/etama.anm"`（`0x498524`）
+ `th07:0x426fa6` `push 0xb` → `th07:0x426fae` `call 0x44df90`。自解析该文件（v2、4 个 entry）：

| entry | 文件内偏移 | num_sprites | max sprite id | num_scripts | max script id | 前进量 | 该 entry 的 base |
| --- | --- | --- | --- | --- | --- | --- | --- |
| 0 | `0x0` | 168 | 167 | 25 | 24 | 168 | 512（`0x200`） |
| 1 | `0x21644` | 50 | 49 | 38 | 37 | 50 | **680**（`0x2a8`） |
| 2 | `0x42d58` | 1 | 0 | 1 | 0 | 1 | 730（`0x2da`） |
| 3 | `0x62e64` | 1 | 0 | 1 | 0 | 1 | **731**（`0x2db`） |

- 512 + 168 = 680 ✅（entry1 = `data/etama/etama2.png`）
- 680 + 50 = 730 ✅（entry2 = `etama3.png`）、730 + 1 = 731 ✅（entry3 = `etama4.png`）
- **731 + 1 = 732** = `eff01.anm` 的载入 base：`th07:0x41ce21` `push 0x2dc` + `th07:0x41ce26` `push "data/eff01.anm"`（`0x4986a4`）
  + `th07:0x41ce2b` `push 0x11` → `th07:0x41ce33` `call 0x44df90` ✅

### 2.3 道具脚本 = `0x2c4 + type` 落在同一段

- `th07:0x4328ea` `mov edx,[ebp+0xc]`（= item type）、**`th07:0x4328ed` `add edx,0x2c4`** ⇒ 脚本全局 id = `708 + type`；
  随后（`th07:0x432912`）`eax = [AnmManager + 0x28ef0 + (type+0x2c4)*4]` 取脚本指针 ✅
- base(entry1) = 680 ⇒ `708..717` = entry1 的**文件内 id 28..37**（entry1 的 script id 恰好是 `0..37` 连续）
- 自解析 + `thanm -l 7` 双向确认：这 10 支就是十连发道具脚本 —— `script53`…`script62` =
  `ins_3(sprite4)` … `ins_3(sprite12)` 各配一条 `ins_2()`（thanm 编号 = entry0 的 25 支 + entry1 的 0..37 ⇒ `53..62` = entry1 的 28..37）✅

## 3. 全量扫描：这条规则在零售文件里长什么样

### 3.1 前进量 ≠ sprite 数的 entry（自解析 63 个 `.anm` / 101 个 entry ⇒ **25 个**）

方法：按 §1.2 的算法复算 `max(sprite id ∪ script id, 下限 0) + 1`；id 按 **i32** 读（loader 用 `jge` 有符号比较）。

| 文件 | entry | num_sprites | max sprite id | max script id | 前进量 |
| --- | --- | --- | --- | --- | --- |
| `player00.anm` | 0 | 31 | 133 | **143** | **144** |
| `player01.anm` | 0 | 29 | 129 | 129 | **130** |
| `player02.anm` | 0 | 17 | 129 | 129 | **130** |
| `stg4enm.anm` | 0 | 87 | 86 | **164** | **165** |
| `stg5enm.anm` | 0 | 80 | 79 | **147** | **148** |
| `stg6enm.anm` | 0 | 79 | 78 | **146** | **147** |
| `stg3enm.anm` | 0 | 52 | 51 | **133** | **134** |
| `stg1enm.anm` | 0 | 48 | 47 | **137** | **138** |
| `stg8enm.anm` | 0 | 75 | 74 | **142** | **143** |
| `stg7enm.anm` | 0 | 79 | 78 | **140** | **141** |
| `stg2enm.anm` | 0 | 40 | 39 | **133** | **134** |
| 其余 14 个 | — | — | — | — | 见下 |

其余（前进量 / sprite 数）：`capture.anm` e0 **4/3** · `face_mr00|rm00|sk00.anm` e0 **8/5** · `result00.anm` e2 **23/7** ·
`stg2bg.anm` e0 **9/7** · `stg4bg4.anm` e0 **5/3** · `stg5bg|stg7bg|stg8bg.anm` e0 **26/15** · `stg6bg.anm` e0 **5/4**、e1 **5/1** ·
`title01.anm` e7 **5/4**、e8 **29/13**。

### 3.2 ⚠ 会改变**后续 entry 的 base** 的四个文件（首 entry 就不等）

| 文件 | entry0（前进量 / sprite 数） | 后果 |
| --- | --- | --- |
| `stg6enm.anm` | **147 / 79** | entry1 的 base 比「按 sprite 数算」**高 68** |
| `face_mr00.anm` | **8 / 5** | entry1 的 base 高 3 |
| `face_rm00.anm` | **8 / 5** | entry1 的 base 高 3 |
| `face_sk00.anm` | **8 / 5** | entry1 的 base 高 3 |

⇒ 把规则简写成「= sprite 数」在**零售文件上就会算错 base**（哪怕只差 1）。本文的两个实例（731 / 690）与
§2.3 的道具链**不受影响**，因为 `etama.anm` 的 4 个 entry 都在特例里。

### 3.3 每个 `.anm` 的载入 base 与 index（🟡 批量提取，仅 4 处逐指令复核）

`loadAnm` 的 60 个调用点，前三个 `push` = `(base, 文件名, index)`（cdecl 右到左 ⇒ arg0 = index ✅）。
下表只列与本文相关的以及几个反例；未逐点复核的标 🟡。

| 文件 | base | index | 备注 |
| --- | --- | --- | --- |
| `data/text.anm` | 1792（`0x700`） | 0 | ✅ 逐指令复核 |
| `data/etama.anm` | 512（`0x200`） | 11（`0xb`） | ✅ 逐指令复核（占 index 11–14，4 个 entry） |
| `data/capture.anm` | 1828（`0x724`） | 4 | ✅ 逐指令复核 |
| `data/eff01.anm` | 732（`0x2dc`） | 17（`0x11`） | ✅ 逐指令复核 |
| `data/eff02..eff08.anm` | 732 / 733 / 734 | 17 / 18 / 19 | 🟡 按调用点自动提取；多个 eff 复用同一 base（同一时刻只载一个） |
| `data/player00..02.anm` | 1024（`0x400`） | 10（`0xa`） | 🟡 三者共用 base/index |
| `data/stgXbg.anm` | 768（`0x300`） | 5 | 🟡 stg4bg2..5 依次 784/800/816/832、index 6..9 |
| `data/title01.anm` | 2304（`0x900`） | 32（`0x20`） | 🟡 7 个 entry，见 §3.1 |

> **index 不是文件号**：它是「扁平 entry 号」（§1.1 `th07:0x44e01f` 每 entry +1），所以 `etama.anm` 占
> index 11..14、`eff01.anm` 占 17..（v1.00b 里 eff01 单 entry）。index 的上限 `0x32` 是**全游戏**的配额，不是每文件。

### 3.4 ⚠ 与既有文档的差异（`engine/item/th07/01-sakura-items.md` §3.3）

那份文档（别人的结论，本文**不改它**）把前进规则写成
「下一个 entry 的 base = 本 entry base + `max(sprite.id, script.id)`」，并据此给出
`entry1 base = 0x2A7`（679）、`entry2/3 base = 0x2E5`（741）、道具脚本「raw id `29 + type`」、
sprite「local `5 + type` / `15 + type`」。以本文的一手读法：

| 项 | 那份文档 | 本文（一手） | 依据 |
| --- | --- | --- | --- |
| 前进公式 | `+ max(id)` | `+ max(id) + 1` | `th07:0x44e4c9` `add eax,0x1` |
| entry1 base | 679（`0x2A7`） | **680（`0x2A8`）** | §2.2 复算 |
| entry2 / entry3 base | 741（`0x2E5`） | **730 / 731** | 512+168+50+1 |
| 道具脚本文件内 id | `29 + type` | **`28 + type`** | 708 = 680+28 |
| 道具 sprite 文件内 id | `5 + type` / `15 + type` | **`4 + type` / `14 + type`** | 684 = 680+4 |
| 全局号（`0x2c4+type` / `0x2ac+type` / `0x2b6+type`） | 正确 | 同 | — |

⇒ 那份文档的**全局号全对**，只有「base = `0x2A7`」及其派生出来的 local/raw 号整体差 1。两条独立佐证：
(a) 道具脚本 10 支（type 0..9）= 全局 `708..717`，必须落在 entry1 的 id `0..37` 里 —— 以 680 计正好是 `28..37`（吃满），
以 679 计会要 id `29..38`，而 **id 38 不存在**；
(b) `EffectManager` 表（§4）里 type `0x1d` 的 id = **690** = 680 + entry1 的脚本 id 10，若 base 是 679 就凑不出 690。

> 建议（不由本文执行）：把那份文档的 §3.3 公式补 `+ 1`、`0x2A7 → 0x2A8`，并把它表中的 local/raw 号减 1；
> `engine/item/OVERVIEW.md` 的「类型 → sprite 的落点 ✅ `0x2ac+type` / `0x2b6+type`」是**全局号**，不用动。

## 4. `EffectManager` 的 type → id → (anm, 脚本) 全链路

### 4.1 表 `th07:0x49efc0`（静态表，stride `0xc`）

`{u16 id; u16 pad; void* update; void* on_create}`（直读 exe 的 `.data`）：

| 成分 | 偏移 | 一手 |
| --- | --- | --- |
| 全局脚本 id | `+0x0`（u16） | `th07:0x41c670` `imul edx,edx,0xc` / `th07:0x41c673` `mov eax,[edx+0x49efc0]` |
| 保留 | `+0x2` | 实测恒 0 |
| update 指针 | `+0x4` | 拷贝进 `zEffect+0x2c8`（`th07:0x41c6d9`），每帧被调（§4.5） |
| 创建钩子 | `+0x8` | 构造末尾调一次（`th07:0x41c769`），**返回值非 0 就杀实例** |

读出值（节选，全表可用附录 A 的命令复现）：

| type | id | 落在哪 | update | 创建钩子 |
| --- | --- | --- | --- | --- |
| `0x00` | 683 | `etama.anm` entry1 文件内脚本 3 | 0 | 0 |
| `0x04`–`0x0b` | 691–698 | `etama.anm` entry1 文件内脚本 11–18（连号） | `th07:0x41a4f0` | `th07:0x41a370` |
| `0x10` | 732 | 当前关 `eff0X.anm` entry0 脚本 0（载入 base 就是 732） | 0 | 0 |
| `0x19` | 730 | `etama.anm` entry2（base 730，文件内脚本 0） | `th07:0x41c1b0` | 0 |
| **`0x1c`** | **731** | `etama.anm` entry3（base 731，文件内脚本 0） | `th07:0x41c1b0` | 0 |
| **`0x1d`** | **690** | `etama.anm` entry1（base 680，文件内脚本 10） | **`th07:0x41ad10`** | **`th07:0x41aa60`** |
| `0x1a/0x1b/0x1e/0x1f` | 703 | `etama.anm` entry1 文件内脚本 23（同一支脚本 + 4 个不同钩子） | `th07:0x41aef0` | `th07:0x41b4a0` 等 4 个 |

**type `0x00`–`0x21` 里除 `0x10` 外，id 全部落在 512..731 ⇒ 全部是 `etama.anm` 的脚本**（32 / 48 个 type）——
这就是本节这条规则的实际用途：把一个整数还原成「哪个 `.anm` 的第几支脚本」。

⚠ 另一半要小心：`0x10` 的 732 与 `0x17` 的 772 指向**当前关的 `eff0X.anm` 段**（载入 base 732/733/734，§3.3），
而零售各 `eff0X.anm` 只有 **1–2 支**脚本（`thanm -l` 实测：eff01/02/03/04 = 1、eff05/06/07/08 = 2）⇒ **772 在实际载入的文件里没有对应脚本**。
type `0x22` 起该字段读出 31632 / 33040 这类**像文件偏移的值**，不是 id ⇒ 那一段可能不是同一张表。两者都进 §6 ❓。

### 4.2 三个 spawn 函数怎么用这张表

| 函数 | 特性 | 一手 |
| --- | --- | --- |
| `th07:0x41c1c0`（ExpHP `EffectManager::create_effect`） | 落**轮转池** `general_effects[0x190]`（`th07:0x41c1cc` 读 `[mgr]` 游标 ×`0x2d8` + `0x1c`），游标自增、到 400 回绕 | `th07:0x41c202`–`th07:0x41c218` |
| `th07:0x41c610`（ExpHP `EffectManager::sub_41c610`，`ret 0x14`） | 落**指定槽** `indexed_effects[arg2]` = `mgr+0x1c+(arg2+0x190)*0x2d8`（ExpHP `zEffectManager`：`general_effects[0x190]@+0x1c`、`indexed_effects[0x9]@+0x4719c`） | `th07:0x41c61c`–`th07:0x41c62e` |
| 两者共用的三句 | `[vm+0x1d8] = (u16)id` → `eax = [AnmManager + 0x28ef0 + id*4]` → `call 0x44ea20`（ExpHP `AnmLoaded::load_script_into_vm`） | `th07:0x41c68c` / `th07:0x41c699` / `th07:0x41c6a8`；`th07:0x41c2a8` 同 |

`EffectManager` 单例 = `th07:0x12fe250`（`.data` BSS；本文查到的调用点都写成 `mov ecx,0x12fe250`）。
每帧 tick = `th07:0x41c790`：扫 0x198 个槽（stride `0x2d8`），活跃位 = `zEffect+0x2cc`，
先调 `[slot+0x2c8]`（返回非 1 ⇒ 杀），再 `call 0x450d60`（anm VM 每帧解释器，分派表 `th07:0x4540da`）。

### 4.3 实例 A：type `0x1c` → **731** = `etama.anm` entry3 的文件内脚本 0

`thanm -l 7` 把它印成 `script 0 script64`（thanm 是跨 entry 连续编号：entry0 的 25 + entry1 的 38 + entry2 的 1 = 64）✅

```
ins_3(sprite0)          // sprite0 = 整张 data/etama/etama4.png 256×256
ins_13(0, 0, 0.0031415927)
ins_8(0) ; ins_34(30, 0, 160) ; ins_7(1,1) ; ins_37([10000], 32)
+1: ins_79([10000]) ; ins_1()
```

| 触发 | 一手 |
| --- | --- |
| **开结界** | `th07:0x441a8d`–`th07:0x441a9d`：`push -1 / push 1 / push 4 / push &自机+0x930 / push 0x1c`，`th07:0x441aa4` `call 0x41c610`（ecx = `0x12fe250`）⇒ **`indexed_effects[4]`** |
| 返回值 | `th07:0x441b74` `[自机+0xb7e6c] = eax`（结界 VM 句柄挂在自机上） |
| **崩结界（灵击）** | `th07:0x441bd9` 先看 `[自机+0xb7e6c]`：非 0 就 `th07:0x441bee` `mov BYTE PTR [旧VM+0x2cc],0`（**杀旧 VM**）+ `th07:0x441bf8` 把句柄置 0；再 `th07:0x441c02`–`th07:0x441c19` 用同一组参数（`push 0x1c`、槽 4）`call 0x41c610` 起新的 ✅ |

### 4.4 实例 B：type `0x1d` → **690** = `etama.anm` entry1 的文件内脚本 10

`thanm -l 7` 印成 `script script35`（25 + 10 = 35 ✅）；

```
ins_3(sprite28)                        // 文件内 sprite 28 = { x:128, y:112, w:64, h:64 } ← 飞出去的那 32 颗
ins_30(1) ; ins_60([10004.0f], 6.2831855f) ; ins_52(...) ; ...
```

| 环节 | 一手 |
| --- | --- |
| 32 颗的循环 | `th07:0x441dc9` `[ebp-0xc] = 0xC0490FDB`（= **−π**）；循环体 `th07:0x441df4`–`th07:0x441e38`；计数 `th07:0x441dee` `cmp [ebp-0x8],0x20`（**32**，body 先跑、`th07:0x441dd9` 在循环底递增） |
| 步长 | `th07:0x441de5` `fadd ds:0x498cfc` = **`0.19634955`** = 2π/32（`0x3e490fdb`；2π/32 = 0.19634954） |
| 起一颗 | `th07:0x441e01` `push 0x1d` → `th07:0x441e08` `call 0x41c1c0` ⇒ 落**轮转池**（不是指定槽） |
| 速度方向 | `th07:0x441e13` `call 0x48bbf0`（fcos）→ `[vm+0x294]`；`th07:0x441e27` `call 0x48bb40`（fsin）→ `[vm+0x298]`（覆盖创建钩子给的随机方向） |
| 运动 | update = `th07:0x41ad10`（公式见下），每帧把算出的 `pos` 写进 `vm.pos(+0x1c8)` 后返回 1（活着） |
| `home` 是谁 | 创建钩子 `th07:0x41aa60`：`[effect+0x288..0x290] = [effect+0x24c..0x254]`（= 构造时从 spawn 参数拷进来的位置）⇒ **= 出生点** ✅ |
| 随机初速 | 同一钩子随后 `th07:0x431900`（ecx = `0x49fe20`）取随机 → `×[0x498a60]`(2π) `−[0x498a64]`(π) → cos/sin 写同两个字段（被结界循环覆盖） |

`th07:0x41ad10` 的一手算式（`fild`/`fadd`/`fmul`/`fdiv`/`fadd` 逐条）：

```
t   = (i32[effect+0x2c0] + f32[effect+0x2bc]) × 256.0f ÷ 30.0f     // th07:0x498a98 = 256、th07:0x498b00 = 30
pos = home[effect+0x288] + v[effect+0x294 / +0x298 / +0x29c] × t
```

`effect+0x2b8` 是个 Timer（初值 `{-999, 0, 0}`，每帧由 `th07:0x43958d` 推进）：`+0x2c0` = 已过帧数（int）、
`+0x2bc` = 小数部分（float）⇒ `t` 就是「帧 × 256/30」。

### 4.5 ⚠ 表里第 3 个指针（`+0x8`）不是「draw」

- **全 `.text` 只有三个调用点**（`th07:0x41c385` / `th07:0x41c59b` / `th07:0x41c769`），全在**三个 spawn 函数的构造末尾**；
  模式是 `cmp [type*0xc+0x49efc8],0 ; je` → `call` → **返回值非 0 就把 `zEffect+0x2cc` 置 0（杀实例）** ⇒ 它是一次性的**创建钩子**。
- 每帧的绘制不在表里：tick（`th07:0x41c790`）走 `[slot+0x2c8]`（= 表的 `+0x4`，`update`）之后调
  `th07:0x450d60`（anm VM 每帧解释器）—— 效果的外观由**它自己的 anm 脚本**决定（脚本里 `sprite` 指令，见 §4.6）。
- ⇒ 建议把表记作 `{u16 id; u16 pad; void* update; void* on_create}`；把 `+0x8` 叫「draw」会误导（type `0x1c` 的该项恒 0，
  但它明明画出了整个结界环）。

### 4.6 反查方向：脚本里的 `sprite` 号是**文件内**号

- anm VM 分派（`th07:0x450d60`，表 `th07:0x4540da`，index = opcode+1，越界检查 `th07:0x450dd9` `cmp ...,0x52`；
  opcode 取 `th07:0x450dc1` `movsx ecx,WORD PTR [eax]`，`time` 在 `+4`）中，**opcode 3 = `sprite`** → case `th07:0x450e1f`：
  `[insn+6]&1` 判参数是否变量 → 取 `[insn+8]` → **`add edx,[AnmManager + (i16)[vm+0x1d8]*4 + 0x2b6f0]`** → `call 0x44e8e0`（ExpHP `AnmManager::set_vm_sprite`）
- 即：脚本里写的是**该文件内的 sprite 号**，引擎用「本 VM 的全局 id（`vm+0x1d8`）→ base」表现场加出全局号。
  所以 §4.4 的 `ins_3(sprite28)` 落到全局 **680 + 28 = 708** = `{128,112,64,64}` ✅（`thanm -l` 同一读数）
- `th07:0x2b6f0` 表的写点是 §1.2 的 `th07:0x44e491`、清零点 `th07:0x44e640`；**读者只有 `th07:0x450e7b` 这一处**（全 `.text` grep）。

## 5. 适用性边界（⚠ 别外推）

| 范围 | 结论 | 状态 |
| --- | --- | --- |
| TH07 v1.00b | 本文全部内容 | ✅ 一手 |
| TH07 其它 build（v1.00a 等） | **未验**：地址是 imagebase `0x400000` 的绝对 VA，换 build 必须重定位 | ❓ 未验 |
| TH08 / TH09 / TH10 的 anm | **未验**。它们的存在性只能靠「version 字段」推（`0`=TH06、`2`=TH07、`3`=TH08/09、`4`=TH09.5/10、`8`=TH13+，见 `local/vendor/thtk/extlib/thtypes/anm_types.h`），**前进规则是否同型没有查** | ❓ 未验 |
| TH16 / TH18 | **不是这套**：那两作按「每文件扁平脚本号」索引（[`../th18/04-anm-slot-selection.md`](../th18/04-anm-slot-selection.md) §4 是别作的一手结论），**不要拿本文的 base 前进规则去套** | ❌ 不同型（据别作文档） |
| `0xa00` / `0x32` 两个上限 | 是 TH07 的常量，别当跨作通则 | ✅ 一手（本作） |
| 「前进量 = sprite 数」 | **本作就有 25 个反例**，任何版本都别当通则 | ❌ 已被本文证伪 |

## 6. ❓ 未定 / 未查

- ❓ `AnmManager+0x28acc + index*4`（`th07:0x44e240` 写 = `entry + [entry+0x1c]`，ExpHP 把 `+0x1c` 叫 `name_offset`）
  全 `.text` 找不到读点，是死写还是另有寻址形式未查。
- ❓ `AnmManager+0x2def0 + index*0xc`（`{raw entry, base, 子 entry 数}`）除 loader 家族（`th07:0x44e4e0` 等）外没找到读者
  ——「有没有游戏逻辑侧读者」只做了 grep 级否定，未逐条验证。
- ❓ `entry+0x20` 被 loader 写成 **base**（`th07:0x44e2ba`），ExpHP 名 `__unused_1`：运行时有没有读者未查。
- ❓ `th07:0x44e4e0`（loadAnm 里 `th07:0x44e0ca` 调、另一处也在用）的职责只读到「按 index 走 entry 链 / 取 base」，
  完整语义（是否与 v0 分支和 `name_2_offset` 路径相关）未查完。
- ❓ opcode 2（`th07:0x450e08`：`vm+0x1e0 = 0` 后返回 1）与 opcode −1 / 1（`th07:0x450df3`：清 `flags(+0x1c0)` bit0 后**直落** op2 的代码）
  的社区命名、以及「opcode 1 与 −1 是否真的共享同一语义」未查；本文只知道指令行为。
- ❓ `zEffect` 的 `+0x24c`（spawn 位置副本）、`+0x258`、`+0x290`、`+0x2b8`（Timer：`{−999, 0, 0}` 初值 + `th07:0x43958d` 每帧推进）
  的字段语义只读到「读写点」，没有完整字段图；`+0x2d0`/`+0x2d4`（tick 里按 `{1,3}` 挂链表）未解。
- ❓ §3.3 的表是脚本批量提取（🟡），只有 text / etama / capture / eff01 四处逐指令复核过；其余 56 处未逐点核对，
  且提取假设「紧邻 call 的 3 个 push 都属于该调用」——已知至少 2 处（`th07:0x422d0f`、`th07:0x422d3d`）不成立。
- ❓ 未查：type `0x22`–`0x2f` 在 `th07:0x49efc0` 上的读法 —— 那几个 `+0x0` 读出的是 31632 / 33040 这类像**文件偏移**的值，
  与 `0x00`–`0x21` 的 id 语义不同（可能是另一张表的开头，或另有一条 spawn 路径）。
- ❓ 未查：type `0x17` 的 id 772 超出当前关 `eff0X.anm` 的脚本数（各文件 1–2 支），
  它到底有没有在零售里被生成、以及被生成时会怎样（读到 `0x28ef0` 表里的垃圾指针？）—— 没查。
- ❓ 未查：index 空间里哪些槽是空的（本文只扫了 60 个 `loadAnm` 调用点，没扫「谁按 index 取载入记录」）。

## 附录 A：复现命令

```bash
# 1) 一手平坦反汇编（已存在，勿重跑）
#    objdump -d -M intel local/th07.v1.00b/th07.exe > local/_scratch/th07_full.asm

# 2) 规则本体（外循环 + 前进 + 上限）
objdump -d -M intel --start-address=0x44df90 --stop-address=0x44e070 local/th07.v1.00b/th07.exe
objdump -d -M intel --start-address=0x44e070 --stop-address=0x44e4e0 local/th07.v1.00b/th07.exe

# 3) effect type 表（stride 0xc，type 0x1c / 0x1d）
python3 -c "
import struct;d=open('local/th07.v1.00b/th07.exe','rb').read()
IMG=0x400000; SEC=[('.rdata',577536,59424,573952),('.data',638976,15503960,633856)]
def rd(va,n):
    r=va-IMG
    for _,sva,vsz,ra in SEC:
        if sva<=r<sva+vsz: return d[ra+r-sva:ra+r-sva+n]
for t in (0x19,0x1c,0x1d):
    i,p,u,c=struct.unpack('<HHII',rd(0x49efc0+t*0xc,0xc)); print(hex(t),i,hex(u),hex(c))"

# 4) anm 文件侧（v2 entry 布局 = anm_header06_t）
local/vendor/thtk/build/thanm/thanm -l 7 local/th07.v1.00b/dat/etama.anm | less
#    entry3 sprite0 = 256×256 / entry1 sprite28 = {128,112,64,64}；script35 / script64 原文
```

## 证据指针

`loadAnm` `th07:0x44df90`（外循环 `th07:0x44dfd3`；每 entry 调用 `th07:0x44dff3`；**base 前进 `th07:0x44e05e`**）·
每 entry 子函数 `th07:0x44e070`（累加器初值 `th07:0x44e07c`；sprite max `th07:0x44e3b8`；script max `th07:0x44e459`；
脚本表写点 `th07:0x44e47f`；**base 表写点 `th07:0x44e491`**；**返回 `max+1` `th07:0x44e4c6`–`0x44e4c9`**）·
上限检查 `th07:0x44e0a3`（index < `0x32`）· `th07:0x44e3d2` / `th07:0x44e43b`（全局 id < `0xa00`）·
sprite 读点（脚本内号 + base）`th07:0x450e7b` → `AnmManager::set_vm_sprite` `th07:0x44e8e0` ·
anm VM 解释器 `th07:0x450d60`（分派 `th07:0x450dec`，表 `th07:0x4540da`，opcode 3 = `sprite` 的 case `th07:0x450e1f`）·
`EffectManager` 单例 `th07:0x12fe250` · type 表 `th07:0x49efc0` · spawn `th07:0x41c1c0` / `th07:0x41c610` · tick `th07:0x41c790`（`[slot+0x2c8]` 调用 `th07:0x41c882`）·
载入调用点 `th07:0x426fae`（etama，base `0x200`）· `th07:0x438c4e`（text，`0x700`）· `th07:0x401db3`（capture，`0x724`）· `th07:0x41ce33`（eff01，`0x2dc`）·
道具脚本号 `th07:0x4328ed`（`add edx,0x2c4`）· 结界 `th07:0x441aa4` / `th07:0x441c19`（type `0x1c`，槽 4）· 32 颗 `th07:0x441dd9`–`th07:0x441e38`（type `0x1d`）·
type `0x1d` 的 update `th07:0x41ad10` / 创建钩子 `th07:0x41aa60` · 常数 `th07:0x498a98`(256) `th07:0x498b00`(30) `th07:0x498cfc`(2π/32) `th07:0x498a60`(2π) `th07:0x498a64`(π) ·
结构名 ExpHP `local/vendor/th-re-data/data/th07.v1.00b/type-structs-own.json`（`zAnmManager` / `zAnmRawEntryHeader` / `zAnmVm` / `zEffectManager` / `zEffect`）·
样本 `local/th07.v1.00b/dat/etama.anm` 等 63 个 `.anm`。
