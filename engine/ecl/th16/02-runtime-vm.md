# 02 — ECL 运行时 VM 架构(社区知识 + TH16 结构/地址 + 本仓库 exe 确认)
> **版本**：TH16 v1.00a（`th16.exe`，imagebase `0x400000`）。本文裸地址默认属该版本；引用其他版本须写成 `th18:0x…`。
>

> **这是什么**:现代东方(TH10+「新引擎/ECL V2」,直接覆盖 TH16)ECL **运行时解释器**的架构参考。补 `00-thecl-format-reference.md`(格式侧)、`01-ecl-context-and-variables.md`(变量/上下文一手反编译)之缺,聚焦**VM 怎么跑**。
> **来源**:deep research(2026-06-09,5 角度→17 源→25 claim 3 票对抗验证,24 confirmed/1 killed)。最强 TH16 一手证据 = **ExpHP `exphp-share/th-re-data`**(`data/th16.v1.00a/`,版本钉死 = HSiFS),含结构体布局 + **命名的 VM 函数地址表**;运行模型来自 **Priw8** 教程;变量模型 thecl/truth/Mddass。
> **可信度**:结构体/函数地址 = ✅✅(ExpHP 一手 RE,且与本仓库 exe 独立吻合,见 §5);运行控制流 = 🟡(ExpHP 给的是**数据布局 + 函数名**,**per-frame 派发循环/CALL 调度细节尚未反编译**——本仓库待自证);pytouhou 帧门控 = 概念可借鉴但**仅老引擎(TH06)**,勿外推。仅 TH16。

> **★ 已落 Ghidra(2026-06-10)**:`zEclLocation/zEclStack/zEclSubroutinePtrs/zEclRawInstructionHeader/zEclFileManager/zEclVm/zEclRunContext` 7 个结构体已建进 th16 DB(偏移逐字段对齐 ExpHP),并把 9 个 ECL 函数首参 retype 为 `zEclRunContext*`/`zEclStack*`/`zEclFileManager*`(`ecl_run` 等的反编译已用字段名)。4 个 __thiscall 函数首参待 headless driver 补(MCP 改不了 auto-param)。可复现:`tooling/ghidra/scripts/apply_th16_ecl_names.py`。

---

## 1. 核心模型:每敌机一个解释器(无全局 VM)— Priw8 ✅

- **没有全局 ECL 解释器**:每个**敌机自带一个解释器**;**没有敌机 = 没有 ECL 在跑**。
- 每个敌机有一条**必需的 `main` 调用栈**,可并发若干**额外栈**;**main sub 跑完 → 敌机自动消失**。
- **额外栈 = 异步调用 sub**(`async`)产生 → 挂成链表(见 §2 `async_list_head`)。
- 现代引擎(MoF/TH10 起,V2)**砍掉了独立 timeline**,改用 **ECL 脚本 spawn 敌机**驱动关卡;**所有 sub 命名**以支持动态链接(按名解析,见 `find_sub_by_name`)。

> 来源:Priw8 `content/ecl-tutorial/2.md`("every enemy has its own interpreter, a global interpreter does not exist")、Mddass V2、truth。Priw8 例子是 TH17,TH16 同代内插(强,但属内插)。

---

## 2. ★ TH16 运行时结构体布局(ExpHP th-re-data,`th16.v1.00a`)✅✅

> 偏移逐字段引自 `vendor/th-re-data/data/th16.v1.00a/type-structs-own.json`。这是**真实 TH16 二进制的数据形状**,是本主题最硬的一手证据。

**`zEclVm`**(size 0x120c)— 每敌机的 VM 实例:
| 偏移 | 字段 | 说明 |
| --- | --- | --- |
| 0x0 | vtable | `zVTableEcl*` |
| 0xc | context | `zEclRunContextHolder`(内嵌) |
| 0x11f8 | file_manager | `zEclFileManager*` |
| 0x11fc | enemy | `zEnemy*`(宿主敌机回指) |
| 0x1200 | async_list_head | `zEclRunContextList*`(异步 sub 链表头) |

**`zEclRunContextHolder`**(0x11ec):`current_context`(`zEclRunContext*`)@0x0 + `primary_context`(内嵌 main 上下文)@0x4。

**`zEclRunContext`**(size 0x11e8)— 一条调用栈的执行上下文(main 或 async 各一):
| 偏移 | 字段 | 说明 |
| --- | --- | --- |
| 0x0 | **time** | float,本栈的帧计时(wait 门控用) |
| 0x4 | **cur_location** | `zEclLocation` = **程序计数器(PC)** |
| 0xc | **stack** | `zEclStack`(操作数栈 + 栈式局部变量) |
| 0x1014 | async_id | int32(`async id` 调用赋,供按 id kill) |
| 0x1018 | enemy | `zEnemy*` |
| 0x101c | __set_by_ins_20 | ins 20 写 |
| 0x1020 | **difficulty_mask** | uint8,**本栈的难度过滤掩码** |
| 0x1024 | float_i | `zInterpFloat[8]`(8 路浮点插值器) |
| 0x11a4 | float_i_locs | `zEclLocation[8]` |
| 0x11e4 | __set_by_ins_18_19 | ins 18/19 写 |

**`zEclLocation`**(8 字节)= **PC**:`subroutine_index` int32 @0x0 + `offset_from_first_instruction` int32 @0x4 → **PC 是 (sub 索引, 字节偏移) 二元组**,不是扁平地址。

**`zEclStack`**(size 0x1008):`data` = `union zEclStackItem[0x400]` @0x0(1024 槽)+ `stack_offset` int32 @0x1000 + `base_offset` int32 @0x1004 → **基址(base_offset)+ 偏移的栈帧寻址**(栈式局部变量模型)。

**`zEclRunContextList`**(0x10):`entry`(`zEclRunContext*`)+ `next` + `prev` + `__seldom_used` → **双向链表**(async 栈挂这)。

**`zEclFileManager`**(0x1098):`file_count`、`subroutine_count`、`file_data_pointers[0x20]`、`subroutines`(`zEclSubroutinePtrs*`)。
**`zEclSubroutinePtrs`**(8):`name`(char*)+ `bytecode`(void*)→ **按名→字节码**的 sub 表(`find_sub_by_name` 遍历它)。

**`zEclRawInstructionHeader`**(0x10)= 运行时看到的指令头,**与 thecl `th10_instr_t` 逐字段吻合**(印证 `00-*` §1.3):`time` i32 / `opcode` u16 / `total_size` u16 / `variable_mask` u16 / `rank_mask` u8 / `parameter_count` u8 / `num_stack_refs_in_parameters` u8。
**`zEclRawSubHeader`**(0x10)= `"ECLH"` + `offset_to_sub_data` + 8 字节零(= `00-*` 的 ECLH)。

---

## 3. ★ TH16 ECL VM 函数地图(ExpHP `funcs.json` 地址)✅✅

> 直接可导进 Ghidra(下一步 `apply_th16_ecl_names.py`)。

**解释器核心**:
| 地址 | 名 | 角色 |
| --- | --- | --- |
| **`0x472030`** | `EclRunContext::ecl_run` | ★ **per-frame opcode 解释循环**(读 `cur_location`,按 time 门控推进 PC,派发 opcode) |
| `0x473bc0` | `Enemy::ecl_run` | 敌机侧每帧入口(驱动其 VM) |
| `0x471db0` | `EclRunContext::call_sub` | **CALL 机制**(sub 调用,压栈帧) |
| `0x474860` | `EclStack::ecl_return` | RET(弹栈帧) |
| `0x474740` | `EclFileManager::find_sub_by_name` | 按名解析 sub |
| `0x4747d0` | `EclRunContext::get_subroutine_ptr` | 取 sub 字节码指针 |
| `0x474890` | `Enemy::load_sub_by_name` | 敌机加载 sub |

**栈/参数/变量访问**:
| 地址 | 名 | 角色 |
| --- | --- | --- |
| `0x473c90` / `0x473d40` | `EclRunContext::ecl_get_int_arg` / `_float_arg` | **栈式局部/参数读**(用 base_offset) |
| `0x473e40` / `0x473ef0` | `..._int_arg_given_value` / `_float_arg_given_value` | 给定值变体 |
| `0x474330` / `0x4743a0` | `get_int_arg0_ptr` / `get_float_arg_ptr` | 取参数地址 |
| `0x473fe0` / `0x474090` | `ecl_push` / `ecl_pushf` | 压栈 |
| `0x423810` / `0x423f80` | `Enemy::ecl_get_int_global` / `_int_global_ptr` | **全局/特殊变量读/取址(int)** |
| `0x424110` / `0x424c10` | `Enemy::ecl_get_float_global` / `_float_global_ptr` | **全局/特殊变量读/取址(float)** ← §5 本仓库一手坐实 |

**高位 opcode(≥300 游戏指令)→ 敌机/弹幕 handoff**:
| 地址 | 名 | 角色 |
| --- | --- | --- |
| **`0x41dcb0`** | `EnemyData::ecl_run_over_300` | ★ **opcode ≥300(enmCreate/move/anm/et*…)的处理器**(= README §A 的"开火点 FUN_0041dcb0",身份确定) |
| `0x41dca0` | `Enemy::ecl_run_over_300__trampoline` | 跳板 |
| `0x423050` | `EnemyData::ecl_enm_create` | enmCreate(ins 300/304) |
| `0x423260` / `0x4233a0` | `ecl_anm_set_sprite` / `ecl_sub_anm_various` | anm 指令 |
| `0x4319e0` | `LaserManager::ecl_545_impl__cancels_all` | ins 545 激光 |
| `0x42c240` | `ecl_554_stage_logo` | ins 554 |
| `0x402b70`/`0x406320` | `ecl_move_rand_*` | 移动指令族 |

> **关键结论**:ECL 派发是**分段**的——低位系统 opcode(0-93,VM 核:栈/变量/算术/CALL)在 `ecl_run` 内处理;高位游戏 opcode(≥300)甩给 `ecl_run_over_300`(大概率大 switch)。这解释了为何 `01-*` 看到的不是一张扁平 200+ handler 表。

---

## 4. 帧/时间门控 + opcode 派发(机制,🟡 借 pytouhou 概念)

每条指令带目标帧(`time`);运行循环:**指令帧 > 当前帧 → break;指令帧 ≤ 当前帧 → 推进 PC;== 当前帧 → 派发执行;帧计数 +1**。难度过滤:按 `rank_mask` / 上下文 `difficulty_mask` 跳过不匹配难度的指令。
> ⚠️ 此循环的**确切代码** = pytouhou(`eclrunner.py`)的**老引擎(TH06)**实现,**概念**与 TH16 的 `zEclRunContext.time`/`cur_location` 一致,但 **opcode 号、栈 vs 寄存器、派发表机制都不同**——TH16 的真实循环须反 `ecl_run`(`0x472030`)自证。pytouhou 老引擎 CALL=ins 35(**不是** TH16 的 11/15/16),绝不可混。

---

## 5. ★ 与本仓库 exe 反编译的交叉确认(`01-*`)

本仓库**独立**一手反出的"`0x4921ac..` 变量访问器表"四项,与 ExpHP 命名**逐一吻合**:
`0x423810`=get_int_global、`0x423f80`=get_int_global_ptr、`0x424110`=get_float_global(我反出读特殊变量,返回 float)、`0x424c10`=get_float_global_ptr。**双向独立印证**(我未看 ExpHP 命名即反出同义)。

**两套变量命名空间(澄清 `01-*`)**:
- **"global"/敌机寄存器**(负 id 特殊变量,含 EI0-3=-9985..-9982 / EF0-3=-9981..-9978、RNG、位置)→ `Enemy::ecl_get_*_global`,存 **zEnemy 字段**(`01-*` §2 实测 EI0-3@enemy+0x1498、EF0-3@+0x14a8 等)。
- **"arg"/栈局部**(sub 参数、`var` 局部)→ `EclRunContext::ecl_get_*_arg`,存 **zEclStack**(base_offset+偏移)。
> 故 `01-*` 说的"ECL 上下文=敌机对象"**对全局寄存器成立**;但还存在独立的 `zEclRunContext`(含 PC、time、操作数栈),其 `enemy`@0x1018 回指敌机。两套存储并存,别混。

---

## 6. 变量/寄存器模型(thecl/truth/Priw8/Mddass)

- 变量按**数字 id**寻址,truth 记 `REG[<n>]`,sigil `$`=int / `%`=float(与 thecl 一致)。
- **I3=-9982 / F3=-9978 不是专用硬件寄存器**,而是**普通局部变量 EI3/EF3**,被 thecl **按约定**借作返回寄存器(非 inline 调用才用)。连续块:**EI0..EI3 = -9985..-9982**(int)、**EF0..EF3 = -9981..-9978**(float)。(thecl.h `TH10_VAR_I3/F3`;truth;Priw8 教程 05/06;父仓库 THTK-Studio 仓库的 `tools/th20.eclm` 有 EI/EF 名。)
- 难度/rank(Mddass,🟡 单二手源,待 exe 复核):`-9960`=难度、`-9959`=rank(E0/N1/H2/L3/X4/O5)、`-9953..-9950`=各难度 bool。注意这与 §2 的**每栈 `difficulty_mask`@0x1020 是不同机制**(后者 ExpHP 一手确认,前者全局寄存器待证)。

---

## 7. 验证边界 / caveats(来自报告)

1. **唯一 TH16 一手二进制证据 = ExpHP 静态结构/函数布局**——证了数据形状(PC/time/stack+base_offset/async_id/difficulty_mask/异步链表),**未证控制流**:per-frame 派发循环、CALL(11)/CALL_ASYNC(15)/CALL_ASYNC_ID(16) 如何压栈/调度、async_list_head 如何每帧遍历、time 如何推进 PC——**全靠数据布局 + pytouhou 类比推断,须反 exe 自证**。
2. **pytouhou = 仅 TH06 老引擎**(寄存器式、CALL=ins35);帧门控概念可借,opcode/调用机制不可外推。
3. **Mddass rank 寄存器(-9960/-9959/-9953..)= 单一二手源**(WebFetch 418,经搜索引文,均回溯同页)→ 标 🟡 待 exe 复核。
4. Priw8/ECLjs/ECLplus = TH17/通用现代引擎,非 TH16 专属(同代内插)。ECLjs `ins_0..93` 是其**选择实现的子集**,非真实 opcode 表大小;ECLplus `enm*` 是 mod 自加指令,非原版 opcode。
5. 被**证伪**(0-3)的一条:某 agent 臆造的 pytouhou ECLRunner 字段表——勿用。

---

## 8. 开放问题 → 下一步 exe(顺 ExpHP 地址直插)

1. **反 `EclRunContext::ecl_run`(`0x472030`)** = 真实 per-frame 派发循环:确认 opcode switch/表形态、读 `cur_location`、time 门控推进 PC。← 最高优先,解决"派发器形态"。
2. **反 `call_sub`(`0x471db0`)** + RET(`0x474860`):CALL(11) 往 zEclStack 压什么(stack_offset/base_offset 互动);**CALL_ASYNC(15)** 如何 new + 链入 `async_list_head`;`async_id` 与 CALL_ASYNC_ID(16) 的 kill-by-id。
3. **反 `ecl_run_over_300`(`0x41dcb0`)**:≥300 游戏 opcode 的 switch → 逐个对 `00-*` 的格式 id + `ECL-info` 名;尤其 **fire/enmCreate(`ecl_enm_create` `0x423050`)→ 弹幕引擎 handoff**(接 `engine/bullet/th16/01` §6 fire 描述符 / README §A 三开火点 `0x41dcb0`/0x431fe0/0x438cb0)。
4. 难度过滤实现:`difficulty_mask`@0x1020 在循环里怎么用,vs 全局 rank 寄存器。

---

## 9. 来源(质量分级)
- **一手 / TH16**:`exphp-share/th-re-data`(`th16.v1.00a` 结构+函数,✅✅)— 已克隆 `vendor/th-re-data`(gitignore)。
- **一手 / 现代引擎**:Priw8 `priw8.github.io`(教程+vars+eclmap-16)、ExpHP `truth`(REG 模型)、thtk `thecl.h`(I3/F3)、Priw8 `ECLjs`/`ECLplus`、父仓库 THTK-Studio 仓库的 `tools/th20.eclm`。
- **二手**:Mddass touhouwiki `.../ECL/V2`(V2 综述、rank 寄存器 🟡)。
- **仅老引擎(勿外推 TH16)**:pytouhou `eclrunner.py`/`ecl.py`(TH06)、pytouhou TH06 doc、ExpHP anm gist。

## 关联
- 格式侧 opcode 表 / 二进制:`00-thecl-format-reference.md`。
- 变量/上下文一手反编译:`01-ecl-context-and-variables.md`(本文 §5 衔接)。
- 弹幕引擎(fire 下游):`engine/bullet/th16/01-core-engine.md` §6;README §A 三开火点。
- 纪律:`METHOD.md`;memory `re-overclaim-guard`/`re-evidence-chain-discipline`/`re-workflow-fanout-cost`。
