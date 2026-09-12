# AUDIT —— card-expand 对抗审计

> **版本**：TH18 v1.00a（`th18.exe`，imagebase `0x400000`）。本文裸地址默认属该版本；引用其他版本须写成 `th16:0x…`。
> 姿态：**默认自己写错了，去二进制找证伪证据**。逐条给 CONFIRMED / REFUTED / OPEN。
> 清单来自 [`../../_template/AUDIT-checklist.md`](../../_template/AUDIT-checklist.md)。

## 0. 状态

**A–D + E 第一、二块全部实跑通过（2026-09-02，用户 Windows 实机，thcrap 2024-11-06 stable）。**

| 步 | patch | 日志关键行 | 游戏内 |
| --- | --- | --- | --- |
| 1 | `th18_card_expand` | `OK: table filled (58 rows @ 03850000), 100/100 sites verified` | 与香草无差别 |
| 2 | `th18_card_expand_255` | `jumptable: 255 entries … allocator bound = 254` → `OK … 255 rows … allocator relocated, 100/100` | 与香草无差别 |
| 3 | + `th18_card_expand_test` | `trace: allocate_new_card(id=58, mode=1)  <- NEW ID`（×4，两次 reset_cards 各两张）| 不崩；卡组编成选它时提示「未获取」——当时预期，D 已解 |
| C+D+E1 | `_255`（含 manager 扩容、影子存档、文案重定向）+ `_test` | 用户报告通过（2026-09-02 晚，日志未留档）| id 58 能获得、重启后仍解锁、名字「测试卡牌 58」 |

`gate: BP_ce_gate fired at ScoreFile__load` 每次都在——断点门成立。
第 3 步的 trace 里 `id=42` 是默认卡组的 KOZUCHI，`id=16 / 24` 是局中获得的卡，
`reset_cards` 每次过场跑两遍所以成对出现。

## A. ABI / 栈平衡

| # | claim | 结论 |
| --- | --- | --- |
| A1 | 初始化 codecave 不调用任何引擎函数 | **CONFIRMED** —— 全文只有 `cld/pushad/mov/rep movsd/popad/ret`，见 A5 反汇编 |
| A2 | 调用约定是 cdecl、调用方清栈，所以结尾用裸 `ret` | **CONFIRMED** —— `plugin.h:71` `typedef void (TH_CDECL *mod_call_type)(void *param)`；`plugin.cpp` 的 `mod_funcs_t::run` 直接 `func(param)` |
| A3 | cdecl 要求被调方保留的 `ebx/esi/edi/ebp` 都被保住 | **CONFIRMED** —— `pushad`/`popad` 覆盖全部 8 个通用寄存器 |
| A4 | `rep movsd` 依赖 DF=0 | **CONFIRMED** —— 入口显式 `cld`；且 `popad` 不恢复标志，返回时 DF 仍为 0，符合 ABI 要求 |
| A5 | 生成的机器码就是设计的那些指令 | **CONFIRMED** —— 见下 |
| A6 | FPU 栈平衡 | **CONFIRMED** —— 全程不碰 x87/SSE |

反汇编（`<…>` 表达式用 `deadbeef` 占位，每个恰好 4 字节）：

```
rows=58                          rows=255 追加的填充循环
 0: cld                          13: mov  ebx, 0xc5        ; 255-58 = 197
 1: pusha                        18: mov  esi, <新表+0xb60> ; 行 56 (NULL)
 2: mov  edi, <新表>             1d: mov  ecx, 0xd          ; 0x34/4 = 13
 7: mov  esi, <Rxc53c0>          22: rep  movsd
 c: mov  ecx, 0x2f2   ; 754      24: dec  ebx
11: rep  movsd                   25: jne  0x18              ; 回到 mov esi
13: popa
14: ret
```

`jne 0x18` 的目标正好落在循环体首指令上 —— **CONFIRMED**（rel8 = −15，体长 13 + 跳转 2）。

## B. 写入点

| # | claim | 结论 |
| --- | --- | --- |
| B1 | 100 处 `expected` 与 exe 实际字节逐字节一致 | **CONFIRMED** —— `make verify` 从**已生成的 patch 文件**回到 exe 重新比对，不是复用生成器的中间结果 |
| B2 | 只替换 4 字节常量，opcode 一字不动 | **CONFIRMED** —— `verify_patch` 第 ③ 项逐条比对 `code` 前缀与 `expected` 前缀 |
| B3 | `code` 与 `expected` **渲染后等长** | **CONFIRMED** —— 二者由同一条记录产出。⚠️ 这条不是形式主义：长度不等时 thcrap 会「记一行日志然后**跳过校验**」（`binhack.cpp:1264`），护栏白装 |
| B4 | 绝对 `<…>` 用对了，不该是相对 `[…]` | **CONFIRMED** —— 100 处全是 `mov r32, imm32` / `cmp r32, imm32` / `add`/`lea`，常量是**当值用的地址**，不是 rel32。（`tracking-laser` 的历史教训正相反：数据指针表槽误写成 `[…]`）|
| B5 | 目标落在 `.text`，thcrap apply 时会设页可写 | **CONFIRMED** —— `.text` VA `0x401000` 长 `0xab800`，100 处全在内 |
| B6 | 零售表本身不被写 | **CONFIRMED** —— 只在初始化时**读**它一次（`rep movsd` 的源） |

## C. 完整性 —— 「100 处就是全集」怎么证的

这是本 mod 最要紧的一条：**漏一处的后果远大于改错一处**。三层证据：

| # | 检查 | 结果 |
| --- | --- | --- |
| C1 | 骨架锚点（`add r32,0x34` 紧跟 `inc r32`）在全 `.text` 的出现次数 | **正好 25 处**，与 25 个内联查表一一对应 |
| C2 | 扫「表基 / 回退行 / 表尾」三段值，列出不在骨架里的 | 15 处，**全部**是 `0x4c5f88` / `0x4c5f8c` 那两个热全局 |
| C3 | 扫**整张表区间** `0x4c53c0`–`0x4c5f88` 的全部 4 字节引用 | `.text` **0 处**、数据节指针 **0 处** |

C3 是补的：C2 只扫三小段，扫不到「直接引用第 12 行」这种写法
（`0x4c53c0 + 12*0x34 = 0x4c56d0` 落在三段之外）。补扫后确认没有这种引用。

★ **C2 当场救回一个真站点**：`CardCollection__mark_obtained_and_notify` 的
`mov edx, 0x4c53c4` `0x418e0e` 是**表遍历**，按计数收尾（`cmp ecx, 0x38`）而不是按地址，
骨架不同，被 C1 漏掉。补进 `find_walks()` 后总数从 99 变 100。
**这条审计路径必须留着。**

## C′. 阶段性复审（2026-09-02）—— 「覆盖够不够」从四个方向攻

审计 C 证的是「表的每一处引用都在 100 处里」。但用户问的是另一件事：
**卡牌子系统有没有哪块逻辑绕过了这 100 处、仍然在读旧表？** 分四个方向攻：

### C′1. 从函数名单反推 —— 没有游离的读表者

卡牌子系统 51 个已命名函数里，**9 个**含站点；另有 **4 个**调用非内联的
`TableCardData__get` `0x407d70`（`allocate_new_card`、`AbilityMenu__on_tick`、
`CardShop__pick_weighted_random_offer`、`FUN_00419170`）——函数本体已 patch，等价覆盖。
其余 38 个不碰表：它们要么只用 `card->id` 与对象自身字段，要么用
**别处查表得到的指针**（`zAbilityMenu.__card_ptrs_0xfec`、商店 offer 数组）——
那些指针来自已 patch 的查表，指向新表，行为一致。

**结论：CONFIRMED** —— 不存在「既不含站点、又不调 get、却读表」的函数，
因为 C3 已证 `.text` 里对表区间的引用只有那 100 处。

### C′2. 三种「不经查表也会碰表」的写法 —— 都不存在

| 写法 | 长什么样 | 会落在哪段 | 实测 |
| --- | --- | --- | --- |
| 指针 → 下标 | `sub reg, 0x4c53c0` / `imul`+`div` | 表基段的**非 mov/add/lea** 立即数 | **0 处** |
| 与 NULL 行指针比较 | `cmp reg, 0x4c5f20` | 回退段的 **cmp** | **0 处**（25 处全是 `mov`）|
| 静态存的行指针 | `.data` 里的 `zTableCardData*` | 数据节 4 字节对齐指向表内 | **0 处** |

第二条顺带说明游戏判「查不到」靠的是 `entry->id == 56` 或 `entry->+0x0c == 4`
（[`engine/card/th18/11`](../../../engine/card/th18/11-sentinels-56-57.md) §2），
**不是**比较指针。搬表不会改变这个判定。

### C′3. 有符号比较的隐患 —— 不成立

25 处 END 之后都是 **`jl`（有符号）**。codecave 若落在 ≥ `0x80000000`，
`cmp p, END; jl` 会翻转。核对 PE：`Characteristics = 0x0103`，
**无 LARGEADDRESSAWARE** → 32 位进程用户空间 < `0x80000000`，`VirtualAlloc` 给不出高地址。
**REFUTED（不构成问题）**，但换 build 若加了 LAA 要重看。

### C′4. 与其它 patch 撞车 —— vendor 内 0 处，`base_tsa` 待用户实测

100 处 5–7 字节写入点，任何一处被别的 binhack/breakpoint 覆盖，后应用的那个会
「expected 不匹配 → 静默跳过」。新增 `sites.py conflicts <其它.js>` 做区间交集。

- vendor 里的 ExpHP 5 个 th18 patch（9 个 hackpoint）：**0 处重叠**。
- **`base_tsa`：已实测 —— CONFIRMED，0 重叠。**发布仓库 `th18_modkit` 自带
  thcrap 2024-11-06 及其 `base_tsa/th18.js` + `th18.v1.00a.js`（`addr` 与 `cavesize`
  分在两个文件、地址用 `Rx` 记法——`conflicts` 已改为认这两样）。
  对 base_tsa 70 处 + `th18_mouse_control` + `renko`：**0 重叠**。
  顺带：base_tsa 在 `0x41669c` / `0x4167b0` 挂了卡牌文案（`gentext#card_name/desc`）的
  翻译钩子，紧邻 `imul $0x1c0` 的文案读取——**战线 E 搬文案缓冲时要与它对账**。

### C′5. 第 1 步刻意**不**覆盖的（这不是漏）

行数仍 58，所以 [`11`](../../../engine/card/th18/11-sentinels-56-57.md) §5 的另外 33 处
边界（字面 56/57、`mgr+0xc84` 数组、`zAbilityMenu.__card_ids`、存档数组）**一处不动**，
它们的语义在 58 行下与香草完全相同。这些是战线 B–E 的事，见
[`../card-rework/PLAN-255-ids.md`](../card-rework/PLAN-255-ids.md)。

## D. 量纲 / 算术

| # | claim | 结论 |
| --- | --- | --- |
| D1 | `58 * 0x34 = 3016`，`3016 / 4 = 754 = 0x2f2` 整除 | **CONFIRMED**（`0x34` 可被 4 整除，`rep movsd` 不会剩尾巴）|
| D2 | 拷贝 3016 字节 = 读 `0x4c53c0`–`0x4c5f88`，**不碰**那两个热全局 | **CONFIRMED** —— 末字节是 `0x4c5f87`，`0x4c5f88` 是全局的第一个字节 |
| D3 | rows=255 时 cave 尺寸 = 拷贝量 + 填充量，不多不少 | **CONFIRMED** —— `3016 + 197 × 52 = 13260 = 0x33cc` = 声明的 `size` |
| D4 | 回退行偏移 `56 × 0x34 = 0xb60` | **CONFIRMED** —— 与 `0x4c5f20 − 0x4c53c0 = 0xb60` 一致 |
| D5 | `<codecave:NAME+OFF>` 的 OFF 是**十六进制** | **CONFIRMED** —— `expression.cpp` `GetCodecaveAddress` → `strtouz(…, 16)`。⚠️ 写成看着像十进制的 `+58` 会被当成 `0x58`。生成器一律输出裸十六进制 |

## E. thcrap 行为核对（都是从源码读的，不是记忆）

| # | claim | 结论 |
| --- | --- | --- |
| E1 | `size` 而无 `code` 的 codecave 合法，默认 READWRITE | **CONFIRMED** —— `binhack.cpp:1594` 只在两者皆无时忽略；access 默认见 `codecave_from_json` 尾部 |
| E2 | `export` 要求 access 是 EXECUTE 或 EXECUTE_READ | **CONFIRMED** —— `binhack.cpp:1544`。所以 init cave 声明 `"access": "RX"` |
| E3 | codecave 之间可互相引用 | **CONFIRMED** —— 第二遍先 `func_add` 记录全部地址，第三遍才渲染 `code` |
| E4 | `*_patch_init` 会被调用 | **CONFIRMED** —— `binhack.cpp:1724` → `plugin.cpp:304` `patch_func_init`；`mod_funcs_t::build` 取 `_patch_` 之后的后缀作 key，`th18_card_table_patch_init` → `init` |
| E5 | ⚠️ `*_patch_init` **早于** binhack 应用 | **CONFIRMED** —— `patch_func_init` 在 `codecaves_apply` 末尾，而 `runconfig.cpp:655-656` 是先 codecaves 后 binhacks。**所以它能用来预填新表，但不能用来验证 binhack 是否都打上了** |
| E6 | 页保护在 `patch_func_init` 之后才设 | **CONFIRMED** —— `codecaves_apply` 的 `VirtualProtect` 循环在 `patch_func_init` 之后，所以初始化时整块还是 `PAGE_EXECUTE_READWRITE`，写得进去 |

## F. 仓库纪律

| # | claim | 结论 |
| --- | --- | --- |
| F1 | 不分发游戏字节 | **CONFIRMED** —— 新表内容**运行时**从用户自己的 exe `memcpy`，patch 里只有地址、原字节和源码 |
| F2 | 不写死 `0x400000` | **CONFIRMED** —— 零售表地址用 thcrap 的 `Rx` 记法（`<Rxc53c0>`，`expression.h:310`：相对模块基址）|
| F3 | 可回滚 | **CONFIRMED** —— 从 run config 的 patch 栈里移除即可；不写存档、不改磁盘上的任何游戏文件 |

## H. 全有或全无的门（`th18_card_expand.dll`）

**起因（用户复审）**：`*_patch_init` codecave 是否被自动调用取决于 thcrap 版本；
没被调用时新表全零、100 处 binhack 指向空表，**而日志一切正常**。
E5 已经说明它验不了 binhack；这一条说明它连「表填了没」都给不出证据。

| # | claim | 结论 |
| --- | --- | --- |
| H1 | ~~`*_mod_post_init` 会被调用~~ | **REFUTED（实跑）** —— 日志停在 `plugin_init`。根因见 §H′；改为断点 `ce_gate`（H15）|
| H2 | 名字匹配：`th18_card_expand_mod_post_init` → 后缀 `post_init` | **CONFIRMED** —— `mod_funcs_t::build` 用 `strstr(name, "_mod_")`，只有一处 `_mod_` |
| H3 | 签名 `void (TH_CDECL*)(void*)` | **CONFIRMED** —— `plugin.h:71`；DLL 里声明为 `void __cdecl f(void*)` |
| H4 | `func_get` / `log_printf` 是 C 链接导出，可 `GetProcAddress` | **CONFIRMED** —— `thcrap.h:57 extern "C"`；`plugin.h:24`、`log.h:36` 皆 `THCRAP_API` |
| H5 | codecave 在 post_init 时可写 | **CONFIRMED** —— `access: "RW"` → `PAGE_READWRITE`（E1）|
| H6 | 验证算式：改后 4 字节 = `cave + 类别基偏移 + 字段`，前缀不变 | **CONFIRMED** —— `sites_gen.h` 与行数无关（58/255 逐字节相同）；rows 由第一处 END 站点已写入的尾界反推（`derive_rows`：必须 ≥ cave、整除 `0x34`、落在 58–255），反推失败即 FAIL |
| H7 | 填表的最小自证 | 行 0 的 id == 0、行 56 的 id == 56，任一不符即 FAIL 并停手 |
| H8 | 导出无装饰名 / 32 位 / 只依赖 kernel32+msvcrt / 自身零 x87 | **CONFIRMED** —— `make dllverify dllx87`；x87 只查我们自己的目标文件（整个 DLL 的 42 条来自 static-libgcc 运行时）|
| H10 | 自检①：`thcrap_plugin_init` 验零售表签名（行 0 id==0、行 56 id==56、名字 `"NULL"`），不符则返回 1 自卸载 | **CONFIRMED** —— `dll_main.c`；`IsBadReadPtr` 兜住名字指针非法的情况 |
| H9 | 拿不到 thcrap 导出时的行为 | 降级：写 `th18_card_expand.log`，**不填表**并明说 —— 宁可不装，不静默 |
| H11 | 发布仓库那份 thcrap（2024-11-06 stable）支持本 mod 用到的三样 | **CONFIRMED** —— 拉 GitHub 同日提交 `aeb9155` 的源码核对：`GetCodecaveAddress` 有 `+` 偏移解析、`binhack.cpp` 有 `patch_func_init`、`init.cpp:416` 有 `post_init`；DLL 里 `strings` 也见 `_patch_` 与 `func_get`/`log_printf` |
| H12 | 两个行数的 patch 同时进栈 | **兜住** —— 搬表只有先到者生效，但分配器上界 binhack 两边都能打上；DLL 检出 `rows` 与上界不符即 FAIL 并把 `0x411479` 写回 `0x38`（`restore_alloc_bound`） |
| H14 | 日志落在游戏 exe 目录而不是 CWD | **CONFIRMED（修过一次）** —— 注入期间 CWD 是 `thcrap/bin`（`inject.cpp:355` 设、`:384` 才恢复），`plugin_init`/`post_init` 都在其间；第一版用相对路径把日志写进了 `thcrap/bin/`。现从 `GetModuleFileNameA(NULL)` 拼绝对路径，与 mouse-control 一致 |
| H15 | 自检门改为断点 `ce_gate` @ `ScoreFile__load` `0x4637d0`，`cavesize` 5 | **CONFIRMED** —— 见 §H′ |
| H13 | DLL 构建可复现 | **CONFIRMED** —— `-Wl,--no-insert-timestamp`，两次构建 md5 相同；否则每次 `release` 都会产生只差时间戳的假提交 |

**这一节把「表空」「binhack 漏了」两种静默失败都变成了日志里的一行红字。**

### H′. 为什么 `post_init` 没跑、为什么换成断点

**实跑现象**：`th18_card_expand.log` 停在 `guard: retail table signature ok`，`post_init` 一行没有。

**根因**（`plugin.cpp:301-308`，2024-11-06 与 master 同）：`mod_funcs.merge(mod_funcs_new)` 是
`std::unordered_map::merge`，**不合并目标里已存在的 key**；`init.cpp:327` 先 `plugin_init(hThcrap)`
把 thcrap.dll 自己的 `steam_mod_post_init` / `motd_mod_post_init` 注册进去，`post_init` 已被占，
插件的被静默丢弃。`init` / `detour` 之所以能跑，是因为它们「直接在新表上 `run`」再合并。
另：`post_init` 每个 init stage 结束各跑一次，base_tsa th18 有 3 个 stage。升级 thcrap 无用。

**断点 `ce_gate`** @ `ScoreFile__load` `0x4637d0`，`cavesize` 5：

- 原字节 `55 8b ec 6a ff` = 三条完整指令、无相对寻址；
- 该函数只被 `0x452cde` 调一次，且是**最早碰卡表**的函数（两次调 `init_unlocked_cards_from_table`）；
- 断点声明在本 patch = 最后一个 init stage，**能触发即证明全部 stage 已应用**；
- 与 base_tsa 70 处 + 另两个 patch：0 重叠；`BP_ce_gate` 用 static 只跑一次；
- 启动器底部的交叉检查看得见 `ce_gate` ↔ `BP_ce_gate`。

## I. 战线 B —— 分配器搬迁

| # | claim | 结论 |
| --- | --- | --- |
| I1 | 两处 binhack 同长 | **CONFIRMED** —— `83 fb 38`→`83 fb fe`（3）；`ff 24 9d <disp32>`（7），只换 disp32 |
| I2 | `cmp ebx, rows-1; ja` 的语义 = 允许 id 0..rows-1 | **CONFIRMED** —— 无符号 `ja`，与原 `0x38` 同构 |
| I3 | 跳转表项 57..254 指向 case 56 函数体是安全的 | **CONFIRMED** —— case 56 的序列以 `jmp 0x412cd5` 收尾，栈上正好留 memset 的 `0xc` 字节（§A 已审）；**我们没写任何新的 case，栈契约由原代码自己满足** |
| I4 | 公共尾段把 `card->id` 写成真实 id 而不是 56 | **CONFIRMED** —— `0x412cec mov [esi+4], ebx`，`ebx` = `[ebp+8]` 即调用者给的 id |
| I5 | 基类虚表 `0x4b4c78` 无空槽 | **CONFIRMED** —— 22 项全非零，实读函数体全是 `xor eax,eax; ret[ imm]` / `ret` / 三个读字段的小函数 |
| I6 | id 58 的 `owned[id]` 写不越界 | **CONFIRMED（仅 58）** —— `0x412d42` 写 `mgr+0xc84+id*4`；58 → `+0xd6c`，对象止于 `0xd70`。**59 起越界**，战线 C 之前禁止 |
| I7 | 跳转表全零时的后果 | **REFUTED（已兜住）** —— 那是 `jmp [0]` 必崩，比表空更糟。所以跳转表拷贝**同时**放在 `_patch_init` 保险和 DLL 权威两处；DLL 还核对项 56 == case56、两处 binhack 已生效，不符即 FAIL |
| I8 | 有符号 `jl` 对 255 行的新表尾界 | **CONFIRMED** —— 同 C′3，无 LAA，codecave < `0x80000000` |
| I9 | 测试 cave 的 flags | **CONFIRMED** —— 原 `movzx` 不设 flags；cave 里 `cmp` 设了；返回后紧接 `push eax; call`，无人读 flags（`0x407eeb`/`0x407eec`）|
| I10 | 测试 cave 的栈 | **CONFIRMED** —— `call` 压 4 字节、`ret` 弹回，cave 内不动栈；只写 `eax`，与原指令的目的寄存器相同 |
| I11 | 追踪断点只读 | **CONFIRMED** —— 读 `[ebp+8]`/`[ebp+0xc]`（序言 `push ebp; mov ebp,esp` 已执行），返回 1 照常执行；不调 thcrap API，零 x87 |
| I12 | `0x407ee3` 与 `0x411469` 不与正式 patch 的站点重叠 | **CONFIRMED** —— 前者在 `reset_cards`（无站点），后者在 `0x411479` 之前 7 字节，不相交 |

## J. 战线 C —— `zAbilityManager` 扩容

| # | claim | 结论 |
| --- | --- | --- |
| J1 | 12 处全部同长 | **CONFIRMED** —— `push imm32`×3、`lea r,[r+disp32]`、`mov r,imm32`×4、`mov [r+r*4+disp32],imm32`、`cmp r,imm32`×3；`make verify` 逐条核对原字节 |
| J2 | `0xd70` 在 `.text` 只有这 3 处与本对象有关 | **CONFIRMED** —— 全二进制扫 `$0xd70`：另一处在 CRT 的 `parse_integer`，无关 |
| J3 | 没有代码用 `+0xd6c`（旧余量）或旧区 `0xc84`–`0xd64` 以外的偏移 | **CONFIRMED** —— `0xd6c(` 的两处在 `Bullet__*`，别的对象；`0xc84(` 的 5 处在 `ItemManager`，别的对象 |
| J4 | 商店循环上界抬到 255 会崩 | **REFUTED（所以没抬）** —— 第一轮筛选 `owned==0 → is_available==1 → +0x14==0`；NULL 行与全部 NULL 副本（回退到同一行，`is_available` 读的是**回退行的 id 56**）都能过，候选数 ~198 > 栈数组 `[ebp-0xe4]` 的 57 槽。上界保持 56 项（`+0xe50`），与香草等价 |
| J5 | `reset_cards` 清 255 项不越界 | **CONFIRMED** —— `rep stosd` 写 `+0xd70`..`+0x116c` = 新对象末尾 |
| J6 | 旧区 `0xc84`–`0xd64` 留着不用是否有害 | **CONFIRMED 无害** —— 无人再读写；ExpHP 把 `0xc68` 起 32 字节标成 `__thread`，与 `0xc84` 重叠是它的标注误差，不是我们的写入 |
| J7 | DLL 核对 | **CONFIRMED** —— `check_grow` 按运行时 rows 算改后字节逐条比对，任一不符 FAIL 并还原分配器上界 |

**新登记的边界（#34）**：商店候选数组 `[ebp-0xe4]`（`AbilityShop__initialize`）57 槽、
`pick_weighted_random_offer` 的 `0x8c0` 局部缓冲——**同时可进商店的卡 ≤ 57**，是栈帧大小，
战线 E 让新卡进商店池时必须一并处理。

## K. 战线 D —— unlocked_cards 影子数组 + side-car

**没有一个字节手写机器码**：9 处读由生成器改写 ModRM（去 SIB），写入点与两处同步点走 thcrap 断点，
逻辑全在 C（`native/unlocked.c`）。审计重点因此落在「改写留对了寄存器」和「断点语义」上。

| # | claim | 结论 |
| --- | --- | --- |
| K1 | `.text` 里 imm32 == `0x5f588` 恰好 11 处 = 9 读 + 1 写 + 1 `lea` | **CONFIRMED** —— 全 `.text` 扫描；`make check` 现在断言 9 处读且无未解释命中，多一处少一处都停 |
| K2 | ~~9 处读的 SIB **index 就是 id**，改成 `[index+SHADOW]`~~（NEXT.md / PLAN 的表）| **REFUTED（3/9）** —— 见 §K′ |
| K3 | 「最近一条装载」到站点之间没有改写被留下的寄存器 | **CONFIRMED（人工，9 处逐条）** —— 见 §K′；换 build 必须重看 |
| K4 | 改写后指令 = 同 opcode、同 reg 字段、`[留下的寄存器+disp32]`、同 imm8、nop 补位 | **CONFIRMED** —— 9 处新旧字节各喂 GNU objdump（`-b binary -m i386`）反汇编比对：如 `cmp BYTE PTR [esi+eax*1+0x5f588],0x0` → `cmp BYTE PTR [esi+SHADOW],0x0 ; nop` |
| K5 | 没有寄存器落在 ModRM 的 rm=100/101（esp/ebp 需要 SIB / 无 base）| **CONFIRMED** —— 留下的是 edx/esi/ebx/edx/edx/esi/ecx/edi/eax；`decode_disp32_op` 对 rm∈{4,5} 直接报错 |
| K6 | 断点返回 0 = 跳过原指令、从 `addr+5` 继续，且 `+5..+cavesize` 是 nop | **CONFIRMED** —— `breakpoint.cpp`：`asm_buf[0]=CALL_NEAR_REL32`，其后 `memset(…, x86_NOP, cavesize-5)`；`bp_entry.asm`：返回 0 时不改 `[ebp+0x24]`（retaddr），`popa; ret` 回到 `addr+5` |
| K7 | `0x418e04` `mov byte [esi+edi+0x5f588],1` 处 edi = id、esi = 存档 | **CONFIRMED** —— `0x418de8 mov esi,[0x4cf41c]`；`0x418df1 mov edi,ecx`（fastcall 第一参）；同函数 `0x418df6` 的读也是 `[esi+edi]` |
| K8 | id < 57 放行原指令 ⇒ 零售数组照常写、存档格式不变 | **CONFIRMED** —— 原指令在 thcrap sourcecave 里原样执行；存档序列化只读 `+0x5f4b8` 子对象（`FUN_00463b30`），我们从不写它 |
| K9 | id ≥ 57 跳过原指令 ⇒ 不再写 `unlocked_cards[57..]` 之后的未知区 | **CONFIRMED** —— 返回 0；影子数组 256 字节，`id >= 255` 另有守卫 |
| K10 | `0x46398a` 时 ebx = 存档且零售数组已是最终值 | **CONFIRMED** —— `0x463984 mov [0x4cf41c],ebx` 前一条；`FUN_004639b0(_Dst)`（解析主存档）在 `0x463965` 已跑完；断点里再与全局比对，不等则用全局并记日志 |
| K11 | 三个断点站点无相对寻址、落在指令边界 | **CONFIRMED** —— `c6 84 3e 88 f5 05 00 01`（8）、`8d b3 b8 f4 05 00`（6）、`8d 83 88 f5 05 00`（6），Ghidra 反汇编各为一条完整指令；`expected` 由生成器从 exe 现取 |
| K12 | `unlock_all` 只有作弊菜单调 | **CONFIRMED** —— xref 仅 `MainMenu__tick_menu_0b_score_ranking` `0x469259`、`MainMenu__tick_menu_17_achievement` `0x46d869`；断点镜像 `memset(…,1,0x38)` 的范围 = 影子[0..55] |
| K13 | side-car 路径：游戏存档目录缓冲 `0x568c61` 尾带 `\` | **CONFIRMED** —— `Window__sub_4726a0`：`GetEnvironmentVariableA("APPDATA")` + `\ShanghaiAlice` + `\th18` + `\`；WinMain 退出时直接拼 `"th18.cfg"`。DLL 校验 `X:` 或 `\` 开头、尾 `\`，否则退到 exe 目录 |
| K14 | 断点里做文件 I/O 安全 | **CONFIRMED** —— 游戏线程、绝对路径（不受 thcrap 注入期 CWD 影响，H14）；`fopen/fwrite/MoveFileExA` 无 x87；`make dllx87` = 0 |
| K15 | 与其它 patch 撞车 | **CONFIRMED 0 处** —— `make conflicts` 对 base_tsa（70 hackpoint）+ mouse_control + renko |
| K16 | 测试钩子从断点里调 `mark_obtained(id,1)` | **CONFIRMED** —— `__fastcall`（ecx=id, edx=notify），无栈参数，自己 `push ebx/esi/edi`，`ret` 收尾，无浮点；objdump 见 `mov ecx,esi; mov edx,1; call eax`。只在 patch-test 进栈时存在 |
| K17 | 影子[57] 之后 vs 零售 `uint8_t[0x39]` | **CONFIRMED** —— ExpHP `type-structs-own.json`：`0x5f588 unlocked_cards uint8_t[0x39]`，正好 0..56；`init_from_table` 循环 `cmp edx,0x39; jl` 也止于 56 |
| K18 | 没有 DLL 时 D 的 patch 会怎样 | **已知退化** —— 影子全零 ⇒ 所有卡「未获取」。`_255` 本来就要 DLL（B 的兜底也在 DLL），README 明写 |

### K′. K2 / K3 —— 存档指针在 SIB 的哪一格是随机的

`[base+index*1+disp32]` 在语义上对称，编译器把存档指针放 base 还是 index 没有规律。
9 处里 **3 处放在 index**，按 NEXT.md 那张表改会用存档指针去下标影子数组——不崩、全错：

| 站点 | 原 | 前面那条装载 | 存档在 | 该留 | NEXT.md 表里留的 |
| --- | --- | --- | --- | --- | --- |
| `0x4149ec` | `[esi+eax]` | `0x4149e7 mov eax,[0x4cf41c]` | **index** | esi | eax ✗ |
| `0x416e3d` | `[edx+ecx]` | `0x416e37 mov ecx,[0x4cf41c]` | **index** | edx | ecx ✗ |
| `0x417ea3` | `[ecx+eax]` | `0x417e9e mov eax,[0x4cf41c]` | **index** | ecx | eax ✗ |

其余 6 处存档在 base，表是对的。生成器现在从站点前 48 字节内最近一条 `mov r32,[SCOREFILE_PTR]`
决定丢哪个寄存器，对账器独立再算一遍；找不到装载或装载的寄存器不在 base/index 里就拒绝生成。

**K3 的人工核对**（objdump 看每处前 8 条，中间有没有改写被留下的寄存器）：

| 站点 | 留 | 装载与站点之间的指令 | 结论 |
| --- | --- | --- | --- |
| `0x41440b` | edx | `mov [esi],0`；`mov edx,[edx*4+0x4b3600]`（顺序表 → id，正是要留的值）| ✅ |
| `0x4149ec` | esi | 无 | ✅ |
| `0x416590` | ebx | `addss xmm0,xmm1` | ✅ |
| `0x41694e` | edx | `push esi`；`mov esi,ecx` | ✅ |
| `0x416e3d` | edx | 无 | ✅ |
| `0x417125` | esi | `mov ebx,[ebp+0x10]` | ✅ |
| `0x417ea3` | ecx | 无（`mov ecx,[eax+4]` 在装载**之前**，取的就是 id）| ✅ |
| `0x418df6` | edi | `mov ebx,edx`；`push edi`；`mov edi,ecx`；`mov [ebp-4],edi` | ✅ |
| `0x418e15` | eax | 45 字节：`mov ebx,edx` … `xor ecx,ecx`；`mov edx,0x4c53c4`；`mov eax,[edx]`（表行 id）| ✅ |

**REFUTED 的那条（K2）是本战线最值得记的**：NEXT.md 那张「已算好长度」的改写表里三行留错了寄存器。
教训写进 `sites.py`：任何依赖「编译器把什么放在哪一格」的假设都要从上下文重取，不能从指令形态推。

**实跑通过**（2026-09-02，用户报告；见 §0）。

## L. 战线 E 第一块 —— id ≥ 57 的文案重定向

三处 `imul r, id, 0x1c0` 挂断点，把 r 改成「加上基址后落进 DLL 缓冲」的偏移。对象不扩。

| # | claim | 结论 |
| --- | --- | --- |
| L1 | `zAbilityText` 按 id 取文案的读只有这 3 处 | **CONFIRMED** —— `.text` 里 imm32 `0x1c0` 共 37 处，其余是别的对象的 `+0x1c0` 字段（`8b 8f c0 01 00 00` 形态）、`ScoreFile` 的 `0x463708`、CRT；PLAN §2 E 同结论。写入点 `0x41623d`（文案文件解析器）只写零售 id，不动 |
| L2 | 三处 imul 后紧跟 `add r, 基址`，中间无人读 flags | **CONFIRMED** —— `0x41669a add ecx,edi`；`0x416780 add edi,0x40 … 0x416797 add edi,eax`（中间是 mov/movss）；`0x419270 add eax,[0x4cf29c]` |
| L3 | 三处的基址都是 `ABILITY_TXT_PTR` 那个对象 | **CONFIRMED** —— `0x41655d mov edi,[0x4cf29c]`（FUN_00416540 全程不改 edi 直到 `0x416780`）；`0x419270` 直接读全局。所以 `ext - [0x4cf29c]` 这个相对偏移对三处都对 |
| L4 | `0x416779` 时 `[ebp+0xc]` 是 id | **CONFIRMED** —— 序言 `push ebp; mov ebp,esp; and esp,-8`，`ret 0x10` 四个栈参，`0x416550 mov ebx,[ebp+0xc]` 同一个值；此时 ebx 已被 `0x4166b8` 改写，所以不能用 ebx |
| L5 | 零售 57 张 = `0x63c0`，第 57 张的位置就是尾部字段 | **CONFIRMED** —— `0x4165ae mov eax,[edi+0x63c0]`、`0x41676b lea esi,[edi+0x63c4]`；57 × `0x1c0` = `0x63c0`。阈值取 57：id 57（BACK）也重定向，零售从不为它渲染文案，无观察差异 |
| L6 | 名字被当作格式串 | **CONFIRMED** —— `FUN_004873f0` 先 `FUN_00404e40(param_7, …)`（vsprintf 类）再渲染；占位文案不含 `%` |
| L7 | UTF-8 能显示 | **CONFIRMED（源码）** —— win32_utf8 `MultiByteToWideCharU`：先 `CP_UTF8 + MB_ERR_INVALID_CHARS`，失败退 `fallback_codepage`；base_tsa 是依赖，textdisp 一定在。字宽算式用 `strlen`，3 字节/字比 Shift-JIS 多，字会略挤——外观问题，不是安全问题 |
| L8 | 断点里 `GetModuleHandleA` / 读全局 | **CONFIRMED** —— 无 x87、无 thcrap API；`make dllx87` = 0 |

**实跑通过**（2026-09-02，用户报告；见 §0）。

## M. 战线 E 第二块 —— 图鉴 / 编成（顺序表搬迁 + `zAbilityMenu` 扩容 + 条目数）

| # | claim | 结论 |
| --- | --- | --- |
| M1 | 顺序表 `0x4b3600` 的引用 = 6 处 + 尾界 1 处 | **CONFIRMED** —— 见 §M′ |
| M2 | 图鉴条目数的 7 处 `0x38` 都是「条目数」语义 | **CONFIRMED** —— 见 §M′ |
| M3 | ~~PLAN：8 处 `0x38 → 0xff`~~ | **REFUTED** —— 两处 `cmp r,imm8` 符号扩展，`0xff` = −1 → `jl` 永不成立；且图鉴只该列已注册的新卡。见 §M′ |
| M4 | 另两处 `cmp [eax+4],0x38`（`0x415070` `0x415e63`）不是条目数 | **CONFIRMED** —— `eax` 来自 `+0xfec` 卡指针数组，`[eax+4]` = `card->id`，比较的是「是不是空槽 56」。不动 |
| M5 | `0x41495c mov edi,0x38` 是编成前清理 anm id 数组的循环上界 | **CONFIRMED** —— 清 `[esi-0x400]`/`[esi]`（`+0x7ec`/`+0xbec`）各 56 项；数组本来就是 `[0x100]`（ExpHP 结构体）；抬到 255 只多清 199 个本来就是 0 的槽（`0x488cf0` 对 id 0 无事）|
| M6 | `__card_ids` 的访问 = 14 直接 + 2 游标相对 | **CONFIRMED** —— 见 §M′ |
| M7 | `zAbilityMenu` 大小 `0x13fc` 只有 3 处 | **CONFIRMED** —— operator new `0x413817`、memset `0x413831`、sized delete `0x413abb`（`call 0x48dca1`）；全 `.text` 的 `0x13fc` 只有这 3 处。新大小 `0x17f8`，`__card_ids` 搬到 `+0x13fc`（255 项）|
| M8 | 顺序表填充值 57（BACK）在两个菜单里都不可见 | **CONFIRMED** —— 编成循环先查 `+0x20`（`0x4149de cmp [eax],0`），BACK 行 `+0x20 = 0` → 跳过；图鉴按条目数 56+N 走，填充在 NULL(56) 之后走不到 |
| M9 | 图鉴条目非 7 的倍数（56+N）时最后一行能到 | **CONFIRMED（实跑，2026-09-02）** —— 57 条目，图鉴里第 57 项「测试卡牌 58」可达可选；编成里也能选进卡组。翻页代码 `sub_415b70` 对非整行无特殊处理 |
| M10 | 门里写代码段的时机 | **CONFIRMED** —— `ce_gate` 在 `ScoreFile__load` 入口，主菜单 / 任何 `zAbilityMenu` 都还没创建；`VirtualProtect` 与 `restore_alloc_bound` 同一做法；写之前核对该处仍是 `0x38` |
| M12 | 新卡按类别插进零售顺序表的同类别区段（2026-09-06） | **CONFIRMED（主机单测）** —— 见下「M12 证据」；可见条目数仍 = 56 + N，M2 / M8 / M9 的结论不变 |
| M11 | 新增 27 处 binhack 全同长、只换常量 | **CONFIRMED** —— `make verify`；新旧字节各喂 objdump 逐条比对（`push 0x13fc→0x17f8`、`[edi+eax*4+0x304]→[…+0x13fc]`、`[esi-0x4e8]→[esi+0xc10]`、顺序表 6 处 `[r*4+T]→[r*4+cave]`、尾界 `cmp eax,T_END→cave+0x3fc`）|


### M′. M1 / M2 / M3 / M6 的证据

**M1 顺序表引用**：全 `.text` 扫 `[0x4b3600, 0x4b36e4]` 的 imm32，命中 8 处：
`0x414401` `0x4145f8` `0x414639` `0x41499f` `0x415681` `0x4156b6`（引用）、`0x414b54`（尾界）、
**`0x4337f7 mov eax,[eax*4+0x4b36e4]`——紧邻的另一张表的基址，值相同，显式排除**；生成器要求它必须被扫到且不改。

**M2 七处 `0x38` 的语义**：

| 站点 | 指令 | 语义 |
| --- | --- | --- |
| `0x4137bb` | `mov [this+0x1c4],0x38` | initialize：MenuSelect 项数 |
| `0x414394` / `0x41439e` | `mov [this+0x300],0x38` / `[this+0x1c4]` | 图鉴 fill：当前项数 |
| `0x4145e2` | `cmp eax,0x38`（imm8） | 图鉴 fill 循环上界 |
| `0x41570d` | `mov eax,0x38; idiv [0x5704bc]` | 行数 = 条目数 ÷ 列数（图鉴 7 列 / 编成 10 列） |
| `0x4157cb` | `cmp edx,0x38`（imm8） | 高亮循环上界 |
| `0x415817` | `mov edi,0x38` | 退出时遍历 vm 数组 |

后三处在 `0x415660..` 的图鉴分支里（用顺序表 + `+0x1bc` 直接取 id，不经 `__card_ids`）。

**M3 为什么不是 `0xff`**：两处 imm8 符号扩展；且条目数 = 56 + 已注册新卡数，不是 255（否则图鉴列出 198 个 NULL 副本）。
改为 DLL 在门里现写 7 处，上限 127。

**M6 `__card_ids` 访问点**：AbilityMenu 全部函数（`0x413470..0x4160b0`）objdump 过滤位移 ∈ [`0x304`,`0x3e4`) 与负位移：
直接 `+0x304` 14 处；`0x4145d2 [esi-0x4e8]`（esi 走 `+0x7ec`）、`0x414b3f [eax-0x8e8]`（eax 走 `+0xbec`）→
改成 `+0xc10` / `+0x810`，`0x7ec+0xc10 = 0xbec+0x810 = 0x13fc`。全二进制 `+0x304` 的其它 6 处在
`0x405e21..0x406349`（另一类对象）与 CRT `0x491b26`，不是菜单。

**M12 证据**：零售表 `0x4b3600` 逐张读表行 `+0x0c`：0..55 依次是 BLANK(2)、资源(3)×7、子机装备(1)×14（REIMU_OP…NUE_OP、MAGATAMA）、
被动(2)×21、主动(0)×12、MAGATAMA2(2)——本来就按类别分块。`ce_build_order`（`cards_def.c`，`test_cards_def` 覆盖）把每个新 id 排在
该类别**最长连续段**的最后一张之后（第一版用「最后一张同类别」，被动会跟到掉队在末尾的 MAGATAMA2 后面、落在主动区之后——改掉）、
同类别新卡保持注册序，类别在零售表里不存在的排在所有零售卡之后，再 NULL(56)、余下 BACK(57)。
类别字段 `+0x0c` 在 `.text` 里只有两处内联读取（`0x416974` / `0x41697e`，都在 `FUN_00416940`）：菜单按「类别 ≠ 4」决定显示说明面板，
不参与商店 / 玩法，所以改类别只影响排版。模拟结果（本机按零售表 + `cards.js` 算）：资源区末尾 强欲之壶、方片 2；装备区末尾 破损核心；
被动区末尾 黑桃 ×5、黄昏、加倍、腐化；主动区末尾 反转牌、神之宣告、青眼白龙、炎魔之王；MAGATAMA2 仍孤零零在最后（零售原样）。
类别从已装载的表读（`0x407d70` fastcall(id)，门里此时表已填）；日志 `menu: new card <id> (category c) at order[i], after id <prev>` 逐张打出。

**实跑通过**（2026-09-02，用户报告；见 §0）。

## N. 战线 E 第 7 段（商店）+ 第 10 段（JSON 装载器）

商店三处循环上界从 56 抬到 rows；幻影 id（查表回落到 NULL 行 56）与 BACK（57）靠 cave 里这两行的
`+0x14`（权重）改成 6 排除：随机池要 `≠0 && ≠6`、保证 loop1 要 `==0`、loop2 要 `is_available==2`（dmode 1–5）。
**没有新的机器码 / 断点**；成立的前提是 `+0x14` 除商店外无人读——N1。

| # | claim | 结论 |
| --- | --- | --- |
| N1 | 表行 `+0x14` 只被商店读 | **CONFIRMED** —— 三路穷举，见下「N1 证据」 |
| N2 | 随机池缓冲 = 560 个 dword，无越界检查 | **CONFIRMED** —— 见下「N2 证据」 |

**N1 证据**（`+0x14` 的读者）：

- (a) 25 个内联查表实例（`sites.py list`，锚点扫描是全集）里返回 `+0x14` 的只有 `0x417085` `0x4170b4`
  （`pick_weighted_random_offer`）与 `0x4174c5`（`AbilityShop__initialize` loop1）。
- (b) `TableCardData__get` `0x407d70` 的 6 个调用者（`get_xrefs_to`）：`0x4170f9 mov ebx,[edx+0x14]` 是同一商店函数的压入份数；
  `allocate_new_card` 把指针存进 `card+0x4c`，函数内 57 处 `[esi+0x14]` 全是卡**对象**的字段清零；
  `FUN_00419170`（获得通知）只读 `+0x2c`；`AbilityMenu__on_tick` 三处只读 `+0x0c` / 传给绘制，
  其两处 `[ecx+0x14]` 的 ecx 来自全局 `0x4cf298`，不是表行。
- (c) 全二进制 2333 个函数反汇编扫 `mov r,[r+0x4c]` 后 10 条内 `[r+0x14]`（`card->entry->weight` 形态）：0 命中。

**N2 证据**：`0x416f53 sub esp,0x8d4`；`0x416f6e push 0x8c0` memset 清 `[ebp-0x8c4]`；压入用 `rep stosd`
（`0x41710d`，eax = 表行指针，ecx = 权重）与 `mov [ebp+edi*4-0x8c4],edx`（`0x41712f`，从没拿过再压），
计数在 `[ebp-0x8c8]`；循环里没有与 `0x8c0` 的比较。`0x8c0/4` = **560**，装载器按 `Σ(weight+5)` 保守核对。

**装载器（`cards.c`）**：

| # | claim | 结论 |
| --- | --- | --- |
| N3 | 装载器只写数据 codecave，不写代码段 | **CONFIRMED** —— 写入点：cave 第 `id` 行（`id ∈ [58, rows)`，rows 从 patch 反推）与 DLL 自己的 `s_ext` 文案缓冲；56 / 57 行 `+0x14 := 6` 在 `fill_table`（紧跟拷表，**任何后续 FAIL 都不影响它**，否则上界已是 255 的商店会被幻影灌爆）。代码段立即数仍只有 `menu.c` 写 |
| N4 | `internal_name` 指针不会把 `ability.txt` 的解析器引到新 id | **CONFIRMED（修过）** —— 见下「N4 证据」 |
| N5 | 把零售 NULL / BACK 行的 `+0x14` 从 0 改成 6 只影响商店 | **CONFIRMED** —— N1 穷举；商店里 NULL 零售本就到不了（上界 56），BACK 同 |
| N6 | 商店上界 = rows 后每次开店多扫 199 个 id | **CONFIRMED 可忽略** —— 每个 id 一次线性查表（255 行）× 3 循环，只在 `AbilityShop__initialize` 跑一次 |
| N7 | jansson `json_t` 布局假设失效时不会误读 | **CONFIRMED** —— 只读 `->type`；根的 `type != 0`（OBJECT）直接 FAIL；每个字段再各查一次类型，类型不符 FAIL |
| N8 | 全有或全无 | **CONFIRMED** —— 任一张卡出错就 `s_count = 0`、返回 0 → `restore_alloc_bound`；但**已写进 cave 的行留着**（幻影：`+0x04` 是新 id 但分配器上界已回 56，谁也分配不到；图鉴按注册表 0 张不显示）|

**N4 证据**：`AbilityText__parse_ability_txt` `0x4160b0` 对每个 `@NAME` token 扫全表（`0x41612f` 起，尾界已是 cave+255 行）
逐字节比对 `row+0x00` 指向的串（`0x416140..0x41615a`），命中则 `ebx = &row->id`（`0x41617c`），随后按
`txt + (id*7+line)*0x40` 写 7 行（`0x41622f..0x41624f`）。id ≥ 57 落在 `zAbilityText`（`0x63e0`）之外。
修法：DLL 存的名字带 `'\n'` 前缀（`cards.c`），token 由行解析而来不可能含换行 → 永不命中。
零售 NULL 副本行的 `+0x00` 仍指零售 "NULL"，扫描先命中第 56 行，副本永不被选中。

## O. 行为 SDK（第 9 段）—— 断点换虚表 + `__thiscall` 桩

设计见 [`SDK.md`](SDK.md)。**没有手写机器码**：两个断点 + 编译器生成的 `thiscall` 桩；对象由游戏分配 / 释放。

| # | claim | 结论 |
| --- | --- | --- |
| O1 | `0x412cec` 处 esi = 卡对象、ebx = id；断点 cavesize 6 盖住的两条指令无相对寻址 | **CONFIRMED** —— 见下「O1 证据」 |
| O2 | 换完虚表后 ctor 槽仍会被按零售语义调用 | **CONFIRMED** —— ctor 调用在 `0x412d11`，晚于 `0x412cec`；`mode&1` 判定（`0x412cf9..0x412d0b`）与 dtor（`0x412d35`）不动 |
| O3 | 基类 21 槽的签名 = `ret N`；桩的 ABI 与之一致 | **CONFIRMED** —— 见下「O3 证据」 |
| O4 | 桩不用的槽透传基类实现是安全的 | **CONFIRMED** —— +`0x08`/+`0x38`/+`0x3c`/+`0x40` 直接填基类地址；门里 `ce_sdk_setup` 把 `0x4b4c78` 的 0/2/14/15/16/20 槽与 `engine.h` 常量比对，不符 FAIL（布局守卫）|
| O5 | +`0x50` 桩的尾跳 | **CONFIRMED** —— `ce_state_free(this)` 后 `jmp 0x411410`，ecx 重新装 this，栈上的 flag 参数原样留给基类的 `ret 4`（objdump：`mov ecx,esi; pop esi; jmp eax`）|
| O6 | 私有状态不会泄漏 | **CONFIRMED** —— 所有销毁路径都走 +`0x50` 槽（`reset_cards` / `recount` / 即时卡当场删 `0x412d1d`），SDK 在那释放；256 槽 = 一局卡数上限（`0x411469`）|
| O7 | 移速倍率在 `on_tick_2` 里写才生效 | **CONFIRMED** —— 见下「O7 证据」 |
| O8 | `player+0x47774` 是复活无敌计时器，`{0x117,0x118}` 只在刚置好那一帧出现 | **CONFIRMED（推断链）** —— 见下「O8 证据」，实跑看 A♠ |
| O9 | `bullet+0x9c` 在 +`0x28` 槽被调时已是最终基础伤害 | **CONFIRMED** —— `PlayerBullet__create` `0x45e396` 写、`0x45e7f5` 调槽、`0x45e837` 取用；`CardMomoyo__on_bullet_init` `0x411083` 同一字段覆写 |
| O10 | `0x446cf6` 处 esi = 道具身价且改它同时影响弹窗与计分 | **CONFIRMED** —— `0x446cf3 cmovle esi,eax`（钳 ≥10）之后；`0x446cfc push esi` 给弹窗 `0x4645f0`，`0x446d0d mul esi` → `SCORE += esi/10`；`lea eax,[edi+0xc2c]` 6 字节不含相对寻址 |
| O11 | 测试卡组断点复刻原 movzx 语义 | **CONFIRMED** —— 原指令 `movzx eax, byte [eax+esi+0x5f608]`；断点读同一字节，写 eax，返回 0 跳过；不设 flags（原指令也不设，后随 `push eax; call`）；esi 是槽序号（循环 `0x407ed4..0x407f05` 里 `inc esi`），esi==0 时游标归零 |
| O12 | 事件断点里遍历卡链表安全 | **CONFIRMED** —— 表头 `mgr+0x18`、首结点 `[mgr+0x1c]`、结点 `{card,next,prev}`（`0x412e90` / `0x408690`）；`AbilityManager__on_tick` 用同一走法；加 256 步护栏 |
| O13 | 对账方向 | **CONFIRMED** —— C 有行为 JSON 无 → FAIL（有行为却不可见是 bug）；JSON 有 C 无 → 允许（开发期正常，日志计数）|
| O14 | `0x446d28` 处加钱与零售路径一致 | **CONFIRMED** —— 见下「O14 证据」 |
| O15 | 10♠ 不引入 replay 失同步 | **CONFIRMED** —— 用私有状态计数（每第 10 个），不用任何 RNG；replay 重放同样的拾取序列得到同样的加钱 |
| O16 | `sdk.h` 四个引擎调用的约定 | **CONFIRMED** —— 见下「O16 证据」 |
| O17 | 在 ctor 里再调 `allocate_new_card` 是安全的（强欲之壶）| **CONFIRMED** —— 见下「O17 证据」 |
| O19 | 绑定时把被动对象改成主动的写法与零售等价 | **CONFIRMED** —— 见下「O19 证据」 |
| O20 | SDK 的 c_press / 状态机 / 重置与 Tenshi 模板逐条对应 | **CONFIRMED** —— 见下「O20 证据」 |
| O21 | 反转牌扫子弹池的范围与字段 | **CONFIRMED** —— 见下「O21 证据」 |
| O22 | `ce_play_sound` 的内联汇编（SDK 里唯一一处）| **CONFIRMED** —— 见下「O22 证据」 |
| O23 | ~~Timer 函数是无栈参的 thiscall~~ | **REFUTED（实跑崩溃）** —— 见下「O23」。修正：`thiscall(zTimer*, unused)`，调用时压一个 0 |
| O18 | 壶不会抽到自己 / 无限递归 | **CONFIRMED** —— 排除表里放自己的表行（`pick` 按 `entry->id` 排除，`0x4170e6`）；再加静态深度护栏（嵌套 > 0 直接返回 1）；随机走商店自己的 `pick`，RNG 与商店同源 |

**O1 证据**：尾段 `0x412cec mov [esi+4],ebx`（`89 5e 04`）紧接 `0x412cef mov eax,[edi+0x28]`（`8b 47 28`），都无相对寻址。
ebx 自 `0x411479 cmp ebx,0x38` 起就是 id；esi 自各 case 的 `mov esi,eax`（`new` 的返回）起是对象；
`0x412ce4 test esi,esi; jz` 在前，断点里再判 esi≠0。

**O3 证据**：读 `0x4b4c78` 的 21 个指针逐个反汇编，带栈参的只有 +`0x0c`(`ret 4`)、+`0x1c`(`ret 8`)、+`0x28`(`ret 4`)、
+`0x30`(`ret 8`)、+`0x44`(`ret 4`)，其余裸 `ret`。桩用 `__attribute__((thiscall))`，objdump 确认 this 在 ecx、
一参桩 `ret 4`、二参 `ret 8`（checklist 的 ABI 项）。

**O7 证据**：`player+0x477ec` 由移动函数 `0x45b5b6` 读、Player tick 末尾 `0x45c702` 复位 1.0。UpdateFunc 优先级
AbilityManager `0x16` < Player `0x17`（注册点 `0x40847f` / `0x45a8b4`，同一注册函数 `0x401180`）→ 管理器 tick 里的写
在同一帧被移动读到；`on_tick`（+`0x24`，`0x45c0e5`）在复位之前，白写。BombMarisa（`0x19`）写 0.5 是同一字段。

**O8 证据**：写入者穷举（全二进制扫 `0x47774/78/7c`）：复活 `0x45c35e`（280）、决死救回两处（60）、四个角色炸弹
（40 / 120 / 110 …）、ECL `515 setInvuln`；zTimer `{prev,cur,float}` 布局与 `CardAya__on_tick` 的读写一致。
置值时 prev = cur−1，之后每帧 prev = 上一帧 cur，(280, 279) 不会再现。

**O14 证据**：`collect_money_item` 尾部依次 `0x446d22 inc [0x4ccd20]`（道具计数）、`0x446d28 inc [MONEY_TOTAL]`、
`0x446d2e inc [MONEY]`、`0x446d34 mov [SCORE],eax`。断点盖 `0x446d28` 一条（6 字节，无相对寻址），在它之前把
`MONEY` / `MONEY_TOTAL` 各 += bonus，原两条 `inc` 照常，两个全局始终同步（帝的回填与「累计收集」统计不受影响）；不碰 eax（分数）。

**O16 证据**：`allocate_new_card` `0x411460`：ecx = mgr（`0x4185c1 mov ecx,[0x4cf298]`），`push 2; push id`，尾 `ret 8` → thiscall 两栈参。
`mark_obtained` `0x418de0`：`mov ecx,id; xor edx,edx; call`，尾裸 `ret` → fastcall 无栈参。`TableCardData__get` `0x407d70`：
`cmp [eax],ecx` 用 ecx 当 id，裸 `ret` → fastcall。`pick_weighted_random_offer` `0x416f50`：调用点 `mov ecx,eax(out); mov edx,0xa; push 0; push eax(excl); push 0xe`，
尾 `ret 0xc` → fastcall(ecx, edx) + 三栈参（hi, exclude, n），返回 eax 非 0 = 抽到。`pot.o` 的 objdump：ecx/edx 装法与压栈顺序逐条对上，
调用后无 `add esp`（被调方清栈）。

**O17 证据**：外层分配在 `0x412d11` 调 ctor 时，只写过卡对象自己的字段（`+0x04/+0x08/+0x50`），还没动 `mgr`（计数 `+0x28`、链表 `+0x18`、`owned[]`）。
内层 `allocate_new_card(mode 2)` 完整走一遍尾段（入链、计数、HUD 重建）。回到外层：ctor 返回非 0 → `0x412d17` 删自己 → `0x412d20 mov eax,[edi+0x28]` 取**当前**计数返回；
edi/esi 由我们的 thiscall 桩按 ABI 保住。外层不再使用任何在 ctor 之前读取的 mgr 状态。零售的 `CardNazrin__constructor` 也在 ctor 里改全局（`MONEY`），只是没重入分配器。

**O19 证据**：主动 case（id 43 `0x411d80`）：`flags = (flags & ~0x46) | 8`、`+0x48 = 0xe10`、`+0x54 = 0`、两组 zTimer
`{prev −1, cur 0, cur_f 0, +0xc 0, control |= 1}`，然后 `jmp 0x412ce4`——**跳过** `0x412cd5` 的被动归一化（`& ~0x4a | 4`）。
我们的对象走 case 56 + `0x412cd5`，绑定断点在 `0x412cec`（归一化之后、`0x412d59 test al,8` 之前），在那里按主动 case 的写法重写 flags 与计时器，
尾段随后把它入 `selected_active_card` / `num_active` / HUD，与零售无差。`+0x54` 不属于我们的对象，state 放私有状态；引擎从不读它。

**O20 证据**：C 键分派 `0x45c084`：`INPUT_PRESSED & 0x400` → `mgr+0x38` 的槽 +`0x08`。Tenshi `c_press` `0x40ebf9`：门 `+0x54 == 0 && +0x38 <= 0`；
装填 `+0x38 = (int)(+0x48 × mgr+0xc58)`、`+0x3c = 同 float`、`+0x34 = int − 1`；经过帧 `+0x20` 组清零；`flags |= 0x20`。
`__on_tick_2` `0x40e8c0`：state 0 清 `0x20`、门控（`GUI_PTR->msg(+0x1b0) == 0 && ENEMY_MANAGER->enemy_count_real(+0x198) != 0`，ExpHP 名）下 `+0x38 > 0` 才 `Timer__decrement(+0x34)`；
state 2 清 `0x20`、`+0x24 > 8` 回 0；末尾门控下 `Timer__increment(+0x20)`。`__on_load__2` 只清状态与经过帧，`method_4C` 再清充能。
两个 Timer 函数 thiscall(ecx = zTimer*)、无栈参（`0x409750` / `0x405990`）。SDK 逐条照抄；差异只有 state 的存放位置。

**O21 证据**：`cancel_all` `0x4297a0`：`edi = BULLET_MANAGER + 0x5a8`，2000 次，`add edi, 0xfa0`，状态 `word [edi+0xaac]`（0 / 3 跳过）。
ExpHP `zBullet.state = +0xf68` ⇒ 第 0 张在 `mgr + 0x5a8 + 0xaac − 0xf68 = mgr + 0xec`（= ExpHP 的 `list_0_tail_dummy_bullet`），
`bullets[0x7d0]` 紧随其后（`0xec + 0xfa0 = 0x108c` ✓）。`velocity +0x644 / speed +0x650 / angle +0x654`：`Bullet__on_tick` `0x423f1f..` 用 velocity 推位置，
`0x424083` 从 speed/angle 经 `0x429bc0`（`fsincos`）重算 velocity，`0x4241b8..0x424227` 把 angle 归一到 (−π, π]。反转两边都翻，与任一路径自洽。

**O22 证据**：`0x476c70`：`mov edx,[ebp+8]` 取 id、`mulss xmm2,…` 取声像 x、尾 `ret 4` → stdcall(id) + xmm2。
内联：`movss x,xmm2; push id; call`，不清栈；clobber 列出全部 caller-saved 通用寄存器与 xmm0–7；`reverse.o` objdump 与之一致。

**O23（2026-09-03 实跑崩溃 → 根因）**：`Timer__decrement` `0x409750` 与 `Timer__increment` `0x405990` 的**两条**出口都是 `ret 4`，
Tenshi 调它们之前都有一条 `push ecx`（`0x40ea2e`、`0x40ead3`）——thiscall + 一个从不读取的栈参。SDK 第一版按「无栈参」调，
每次调用被调方多弹 4 字节，`AbilityManager__on_tick` 的栈随之上移，下一次 UpdateFunc 分派（`0x401355 call [eax]`）取到堆地址
→ `C0000005 … could not be executed at 0x0717d458`，日志停在 `card 64 on_tick_2 first hit`。**教训写进 checklist**：
调任何引擎函数前必须看它**全部** `ret N` 出口，不能只看序言；O16 的四个函数当时看了，Timer 这两个只看了头。

**O24（2026-09-04）反转牌发动亮牌：`ce_anm_spawn` 调 `AnmLoaded__instantiate_vm_to_world_list_back` `0x405bf0`**。
一手：`mov edi, ecx`（this = `AnmLoaded*`），`[ebp+8]` out_id、`[ebp+0xc]` script、`[ebp+0x10]` layer、`[ebp+0x14]` out_vm，
**唯一出口 `ret 0x10`**（O23 教训：全部出口都看了）。内部自己进 `DAT_00521738` 临界区，调用方不用管；实体坐标固定 (0,0,0)。
`CardTenshi__c_press` `0x40ebf0` 内联的就是这套（层 13、脚本 `0x1c`、坐标改玩家位），证明从 c_press 时点调它安全。
C 侧用 `__attribute__((thiscall))` 函数指针，4 个栈参；`reverse.o` objdump：`mov ecx,[0x4cf298+0x10]`、`push out_vm; push 0x10; push 0x44; push out_id; call`，
call 后直接 `mov eax,[esp]` 读 id、**无 `add esp`**——与被调方清栈一致。`ce_anm_spawn` 对 `anm == NULL` 早退返回 0。
备查未用：`AnmLoaded__set_sprite` `0x477b00` thiscall ret 8（两出口）、`0x488cf0` stdcall(id) ret 4。
ANM 侧：`ability.anm` 追加 entry7（`REVERSE.png`，字段抄 abcard `BLANK_max`）/ `sprite109`、`script68`（`assets/ability/scripts/68_reverse_flash.anm.txt`），
`build_ability.py` 校验零售 7 entry / 68 脚本 / 贴图逐项不变，`thanm -l` 往返一致。脚本自 `delete()`，卡对象不记 id。
~~🟡 世界层投影是否透视未验~~ → 实跑显示不对；根因：模式 8 走 WORLD 矩阵不加相机 2 的区域原点偏移（`AnmManager+0xd0/+0xd8`），放层 16 会整体偏位（`engine/anm/th18/01-vm-instantiate.md` §3b）。改为零售配方 `layer(20); resolutionMode(1); type(8);`，`ce_anm_spawn` 的层参数同步 20（脚本内 `layer()` 会覆盖，纯注释意义）。复跑仍全空白 → 见 O24′。

**O24′（根因，2026-09-04 二轮）**：`engine.h` 把 `CE_MGR_ABILITY_ANM` 写成 `0x10`——那是 **abcard_anm**。ExpHP `zAbilityManager`：`+0x0c ability_anm / +0x10 abcard_anm / +0x14 abmenu_anm`；`CardTenshi__c_press` `0x40ebf0` 取 `[ABILITY_MANAGER_PTR+0xc]` 起 script `0x1c` 与之一致。错拿 abcard.anm（18 个脚本）后 `AnmManager__sub_407420` **无边界检查**地把脚本表第 68 项（表外内存）拷成 VM → 垃圾 VM，什么都画不出，两版（层 16 / 层 20）全空白。来源是 `10-extensibility-limits.md` §5 旧文一句写反的偏移，没有对着结构体 / 反汇编重取——**又一次 K′/M6/O1 教训：偏移必须从上下文重取，不能抄文档**。修正为 `0x0c`，2D 零售配方脚本先跑基线（实跑 ✅ 显示在上边框中点 = ECL 原点），再切回 type(8) 层 20（实跑 ✅ 有透视；锚点顶边对齐，pos y 上移 96 补偿）。
附带订正：O24 里「层 16 会偏位」的推断经 `02-render-stages.md` 一手对表后不成立（层 12–19 在相机 3 下，偏移为 0）。

**O25（2026-09-04）方片 2 `d2.c`**：只读数据，无新引擎调用——价格表 `CARD_PRICE_BY_TIER` `0x4b35c4`（int[15]，SM 一手 dump）按表行 `+0x10` 的 tier 取价，越界返回 0；
写 `MONEY` `0x4ccd34` / `MONEY_TOTAL` `0x4ccd30`（SM §1）。时序依据：成交 `0x4185c7` 先 `allocate_new_card(mode 2)`（ctor）后扣款，故 ctor 写 `2·M − price`。
M < price 的火力补差价路径不动 MONEY（游戏随后清零）。**未实跑。**

**O26（2026-09-04）皇家同花顺 `cards/royal.c`**（黑桃五张共用 ctor）。触发不开新断点：成交 `allocate_new_card(mode 2)` 调 ctor（`0x412d11`），`owned[id]=1` 在其后（`0x412d42`），ctor 里查其余四张 `owned[]`（`mgr+0xd70`，本 mod 搬迁后的数组）齐了即第五张，天然只触发一次；纯函数 `ce_royal_flush_ready` 有单测。
两个新引擎调用：**`0x4575f0` 加命**——`mov esi,ecx`（this = GlobalsInner `0x4cccdc`），不读栈参，唯一出口裸 `ret`；`[esi+0x6c]` CURRENT_LIVES 钳 `[esi+0x78]` LIVES_MAX，内部再调 `0x405bf0` 起特效、`0x476be0` 放音效 `0x11`。**`0x457690` 加 bomb**——this = ecx，`[ecx+0x7c]` CURRENT_BOMBS 钳 `[ecx+0x88]` MAX_BOMBS，音效 `0x2e`，**唯一出口 `ret 4`** 但函数体不读任何栈参（O23 同款），C 侧签名带一个 dummy int。`royal.o` objdump：加命 `mov ecx,0x4cccdc; call`（无 push）；加 bomb `mov ecx; push 0; call`，call 后无 `add esp` —— 与两函数的出口一致。上限照 `CardLife__destructor` `0x409b80` / `CardBomb__destructor` `0x409c20`：先 `+1` 钳 7 再调加法。门：`GAME_THREAD_PTR` `0x4cf2e4` 非 0（零售即时卡 dtor 的门）。ctor 返回 0 保留卡。
实跑（2026-09-04）：购买第五张触发 ✅，但**之后每关开始又触发一次**（共 6 次）——引擎在每关开始对卡组里每张卡再调一次 +`0x00`（trace：初始携带的 64 `on_stage_start` 后紧跟 `ctor`；02-lifecycle 只记了 mode 0/2 调 ctor，漏了这条）。修：ctor 开头 `ce_fresh_acquire(c)`（= `owned[自己] == 0`；获得路径里 owned 在 ctor 之后 `0x412d42` 才置 1，关卡开始那次早已是 1）。复跑 ✅ 只触发一次。
演出：`ability.anm` 追加 entry8–14（`ROYAL_FLUSH.png` / 五张黑桃卡图副本 / `GOLD_888.png`）+ script69–76（父脚本 70 用 `scriptNew` 起七个子脚本，时间线写死在各子脚本里，194 帧一起 `delete()`），`ce_anm_spawn`（O24 同一调用）+ `ce_play_sound(0x4f = se_trophy)`（O22 同一调用；id 表一手见 `engine/_shared/th18-sound-table.md`）。trophy 音效要在第 60 帧放，C 里没有延时：五张黑桃都挂 `on_tick_2` → `ce_royal_tick`，只有触发那张卡（`s_fx_card`）递减倒计时。无新引擎接口。**演出待实跑。**

**O27（2026-09-04）开发辅助，只进开发用 patch（① 在 `_test`；② 自 2026-09-04 拆成独立可勾选的 `_devstage`）**：① `cards_dev.js` 的 `retail_weight` / `new_weight`——纯数据，改 cave 表行 `+0x14`，在 `ce_shop_capacity_check` 之前应用（否则池子超 560 的 FAIL 会漏）；跳过 id 0 / 56 / 57 与 weight ∈ {0,6} 的行；`ce_weight_override` 有单测。② st01–st06 空壳源文本入库（`assets/ecl/stNN.ecl.txt`，`--regen-shells` 生成：头部 + `main` + `LogoEnemy` + `MainBoss`，`main` 里 `@MainFront()`…`setChapter(41)` 前的道中段与 `debug22` 全删，零售敌机子程序不留；六关 main 同构、`MainBoss` 只引用 bs 文件里的 `Boss`/`BossM`），thecl 直接编；`stNNbs.ecl`（满是符卡脚本）由 `make_dev_ecl.py` 现场反编译零售文件、血量常数等比 ÷100（`lifeSet`、`setInterrupt(槽, 血量, 超时, 子)` 的血量、`lifeMarker` 的血量；超时不动）、编回（往返字节一致）。第一版只把 `lifeSet` 改 1、阈值 2100 没动，实跑血条 / 阶段切换乱掉——阈值必须一起缩。③ ~~`start_money`~~：试过在 `BP_ce_test_deck` 第 0 槽写 `MONEY`，实跑不生效且 `reset_cards` 一局多次会反复重置，已回退（97155d5）；改为 ECL 让 boss 死时多掉金钱道具（`BossDead` 里 `dropExtra(2, 300)` + `dropItems()`，type 2 = 金钱道具，每个 +1）。

**未实跑。** 各卡的验收在 [`NEXT.md`](NEXT.md) §1。

**O28（2026-09-04）神之宣告 `cards/judgment.c`**（主动卡：符卡中按 C，残机减半向上取整，符卡按超时收场）。
引擎链一手见 [`engine/ecl/th18/01-boss-interrupts-and-spellcard.md`](../../../engine/ecl/th18/01-boss-interrupts-and-spellcard.md)。
**不开新断点**，四件事全走零售路径：

| # | claim | 结论 |
| --- | --- | --- |
| O28a | 把 `data.time_in_ecl` 写成活动中断槽的 `time` 等价于自然超时 | **CONFIRMED** —— 见下「O28a 证据」 |
| O28b | 耐久符卡也按失败 | **CONFIRMED** —— 见下「O28b 证据」 |
| O28c | `0x441f10` 刷 HUD 残机的约定 | **CONFIRMED** —— 见下「O28c 证据」 |
| O28d | boss 找法与多 boss | **CONFIRMED** —— 照 `0x4237F0`：`boss_ids[4]` → `+0x18c` 链表比 `+0x6830`；对每个 boss 各推一次，4096 步护栏 |
| O28e | 拒绝发动不丢充能 | **CONFIRMED** —— SDK 在 `on_activate` 返回 `CE_ACTIVATE_REFUSED` 时把刚装填的充能计时退回 `{-1, 0, 0.0}`（= 充满态）、清释放位、state 留 0；与 Tenshi 的门控 `state == 0 && +0x38 <= 0` 一致 |
| O28g | 自然超时到 ECL 523 清 bit0 之间按 C | **PARTIAL（审阅补）** —— 见下「O28g 证据」 |
| O28h | 全屏消弹调用的两个引擎函数的约定 | **REFUTED → 修正（实跑崩溃 2026-09-04）** —— 见下「O28h」；`BulletManager__cancel_all` 不是无参，是 `ret 4` |
| O28f | 残机减半的边界 | **CONFIRMED** —— `ce_judgment_cost`：`lives <= 0 → 0`（拒绝），否则 `(lives+1)/2`（单测 7 项）；只动 `CURRENT_LIVES`，碎片不动 |

**O28a 证据**：C 键在 Player tick（`0x17`）、判定在 EnemyManager tick（`0x1b`，Enemy tick 体 `0x42ff80` 里比较先于 `Timer__increment`）：**同一帧**收场；ECL VM `0x430d30` 只在宣言 case `0x434fc2` 碰 `+0x2bc`，`wait` 用别的计时器，改它不会提前唤醒脚本； `0x42ED40` 每帧取第一个 `hp_value > -1 && time > 0` 的槽，`slot.time <= cur` 即走超时段（血量钉到阈值、`sub_timeout`、Spellcard 置 `0x80` / 清奖励）；比较是 `<=`，写成等值下一帧必触发；`prev` 一并写，`cur_f` 同步（zTimer 三件套，O23 教训）

**O28b 证据**：超时段对 `flags & 8` 的槽走 `(flags & 9) == 9` = 收下；我们在写计时的同一帧清 bit1、`bonus = 0`、`can_still_capture_spell = 0`，结束 `0x42A780` 看 bit1 走失败演出，ECL 523 的 Sannyo 碎片也看 bit1。**限制**：耐久符卡的 `sub_timeout` 脚本本身是「守住」分支，脚本里写的掉落照旧；`0x42ed40` 耐久路径累加的 `0x4ccd6c` 无人读

**O28c 证据**：一手反汇编：`mov edx,ecx`（this = gui），读 `[ebp+8]` 命数、`[ebp+0xc]` 碎片、`[ebp+0x10]` 上限，`ret 0xc`；零售在 `0x417a10..0x417a28`（`AbilityShop__sub_417880`）与 `0x45d1aa..0x45d1bd`（`Player__commit_death_and_enter_state2`）都以 `push [0x4ccd54]; push [0x4ccd4c]; push lives` 调它

**O28h**（订正）：第一版只看了 `0x4297a0` 的序言与 Ghidra 的 `void(void)` 签名，断言「无参裸 ret」——**错**。整函数反汇编到尾：`0x429a0e ret 4`；零售调用现场 `0x434d48 mov ecx,[0x4cf2bc]; push 0; call`、`0x434e01` 同款、MSG 构造 `0x43e2e2 push ecx; call`——thiscall + 一个从不读取的栈参。按无参调多弹 4 字节，`on_activate` 的 `ret` 把栈上的卡对象指针（`0x17d2e090`）当返回地址跳飞：崩溃报告 `[ESP+8]` = 返回 `ce_sdk_c_press` 的地址、PC = 卡对象地址，与 4 字节偏差完全吻合。修正：`thiscall(BULLET_MANAGER; 0)`。**与 O23 同一个坑：调引擎函数前必须看全部 `ret N` 出口，反编译签名不算数。**`LaserManager__cancel_all` `0x449090`（这条第一版就对）：`mov ecx,[0x4cf3f4]` 自取 this，读 `[ebp+8]` 当 mode 传给每条激光的 `vtable+0x28(mode, 0)`，`ret 8`（两个栈参，`0x4490c4`）；ECL 两处 `push 0; push 1/0; call`，玩家死亡 `0x45c3c9 push ecx; push 1; call`（第二参从不读）。我们照 ECL 写 `(1, 0)`，声明为 stdcall 两参。调用时机在写完中断槽计时之后、同一 `on_activate` 内。

**O28g 证据**：非耐久符卡：超时段置 bit7（`0x42a320` 每张新符卡清），卡里 `flags & 0x80` → 拒绝。耐久符卡自然超时不置 bit7、`enemy+0x635c` 的超时位也被清，只靠「活动槽已清（hp_value = -1）→ 找不到槽 → 拒绝」兜底；ECL 预装多个带超时的槽时会误伤下一段。实跑清单：符卡刚超时那几帧连按 C 看是否 refuse


**O29（2026-09-04）青眼白龙 `cards/blue_eyes.c`**（主动卡：献祭 1 残机召唤跟随龙，2500 挡弹，每 300 帧一道矩形伤害光束）。设计 `docs/superpowers/specs/2026-09-04-blue-eyes-design.md`。

**O29a** `BulletManager__cancel_radius_as_bomb` `0x429370` = stdcall 四栈参 + XMM2 半径，ret `0x10`；计数器 `mgr+0x7a41e8`。**证据**：反汇编四个出口都 `ret 0x10`；序言 `movaps xmm0,xmm2` 存 `[ebp-4]` 当半径；Tenshi `0x40eb13` `movss xmm2,[0x4b9290]`(18.0) 后 `push 0; push 0x1869f; push 0; push ebx; call`；`0x40eb46..0x40eb6a` 调前 `mov [ecx+0x7a41e8],0` 调后读。🟡 静态一致，待实跑。

**O29b** `FUN_0045dfa0` = stdcall(center*, angle, life, dmg) + XMM2 宽 / XMM3 高，ret `0x10`，返回 1-based 槽号。**证据**：反汇编 `movss [ebp-8],xmm3; movss [ebp-4],xmm2`、两个出口 `ret 0x10`、`+0x14/+0x18` 收 XMM、`[ebp+0xc]` 角度归一到 (−π, π]；Remilia `0x40f478 movaps xmm3,xmm2`(高) `0x40f480 movss xmm2,[0x4b92b4]`(宽 32) `push 0xc8; push 2; push 0(角度); push eax(center); call`。🟡

**O29c** `AnmManager__get_vm_with_id` `0x488b40` thiscall(mgr; id) ret 4；`interrupt_tree` `0x488be0` stdcall(id, n) ret 8；`0x488cf0` stdcall(id) ret 4。**证据**：`0x488b47 mov esi,ecx`、三个出口 `ret 4`；`0x488be3 mov ecx,[0x51f65c]`（自取 this）、`0x488c4e ret 8`；三张零售卡都 `push [esi+0x1c]; mov ecx,[0x51f65c]; call 0x488b40` 后写 `[eax+0x5f0]`。🟡

**O29d** 献祭：`CURRENT_LIVES < 1` 拒绝；−1 后 `ce_gui_update_lives`——同神之宣告 O28c 的路径。✅ 复用。

**O29e** ~~不写 `player+0x47984`~~ → **O29e′（2026-09-06）**：光束帧 `ce_raise_damage_cap(300)`，与炎魔 V20 同一条证据链（自机弹共用配额、Remilia 先例、GameThread 每帧复位）。用户改定。✅

**O29f** 私有状态与 SDK 主动卡状态机同槽 → 块头预留 16 字节（`ce_state_user`），主机单测 `test_sdk_core` 的块头用例。✅ 单测。

**O29g** `bullet+0x24 != 0` 的弹穿过龙（`0x429370` 的命中条件；零售炸弹同款）。✅ 设计。

**O29i**（教训，静态发现）内联汇编里**先 push 再引用 `"m"` 操作数会错位**：第一版 `ce_damage_rect` 的 `pushl %[ang]` 用了 `"m"(angle)`，编译成 `pushl 0x8(%esp)`——前面已经压了两个值，esp 相对偏移指向别的槽，压进去的是垃圾。objdump 核对调用现场时抓到；改成先 `memcpy` 成整数位、`"r"` 约束再压。**规则：内联汇编里压栈的参数一律走寄存器；`"m"` 只用在任何 push 之前的 `movss`。** `ce_cancel_radius` 与 `ce_play_sound` 本来就符合。

**O29j**（🟡 视觉，实跑看）光束 = 父 VM（C 每帧钉在龙头）+ `scriptNew` 子 VM：子的原点是父位置（thpages `ignoreParent` 说明反推子默认继承父的位置 / 旋转 / 缩放）。为不押注旋转继承，父脚本不转、每个子脚本自己 `rotate(0,0,−π/2)`、彩光沿 −y 飞（不用 `attached`/ins 314）：继承与否结果都朝上。贴图是零售 `pl01b`/`pl01b2`（ORDER 里 `local/` 源，构建时取，仓库不含字节）。

**O29k** 血条 = 两个根 VM（script86 底槽 / 87 填充，`sprite(-1); drawRect(1,1)`），C 每帧写 `vm+0x54/+0x58`（scale x/y）与位置、颜色。**证据**：ExpHP `zAnmVmPrefix` scale = +`0x54` zFloat2（同结构 color_1 = +`0x524` 已被 Tenshi 一手印证）；零售 HUD 充能条 `draw_active_card_hud_entry` `0x408a53`/`0x408a70` 把 fill 比例写进 `vm+0x58`（同时 `+0x70` uv_scale、`+0x80` uv 滚动、`flags |= 0x18`）——「代码写 scale」是零售做法，thpages `drawRect` 也这么建议。脚本里没有 scaleTime 在跑，写入不被插值器覆盖。颜色 D3DCOLOR `0xAARRGGBB`（Tenshi 的 `0xff0080ff` = 蓝 (0,128,255)，第一版文档写成橙是读反了字节序）。🟡 drawRect 是否吃 scale / 层 13 上的表现实跑看。

**O29h** 龙 VM 被引擎删掉（关卡切换 / 消弹演出）时卡不悬空：`on_active_tick` 每帧先 `ce_anm_get_vm`，为 0 就结束持续态；`on_stage_start`/`on_run_reset` 主动删。🟡 实跑。

## P. 商店走两遍（每关进店 2 次）—— 两个放行断点

引擎链一手见 [`engine/card/th18/05-shop-and-money.md`](../../../engine/card/th18/05-shop-and-money.md) §3.5。
**没有手写机器码、不改控制流**：两个断点都返回 1 放行原指令；唯一的「改动」是给 `eax` 加一位，
让零售自己的建店代码（`0x443b10..0x443c17`）再跑一遍。状态机在 `shop_core.c`（主机单测 `test_shop_core.c`）。

| # | claim | 结论 |
| --- | --- | --- |
| P1 | `0x4183ea` 盖住的 12 字节是四条完整指令且无相对寻址 | **CONFIRMED** —— `6a 06` / `6a 00` / `6a 06` / `8d 8f 28 02 00 00`（push 6; push 0; push 6; lea ecx,[edi+`0x228`]）；`sites.py` 生成时与 exe 逐字节对账，门里再核 `e8` + 7 个 `90` |
| P2 | `0x4183ea` 只在「玩家选是、真的成交」时经过 | **CONFIRMED** —— 它在 `case 6/7` 的 `param_1[3]==0` 分支里，前一条是 `0x4183e5 call FUN_00416c80(this,5)`；空白卡路线（state 3 → `0x418584` 置 4）、取消（`0x418551` 置 2）、买不起（state 8）都不经过 |
| P3 | `0x443b05` 处 `eax` = `GameThread+0xb0`、`esi` = this；改 `eax` 后原 `test` 算出的 flags 正是零售语义 | **CONFIRMED** —— 见下「P3 证据」 |
| P4 | 重开与关店在同一帧，敌人 / MSG 看不到指针为 0 | **CONFIRMED** —— 商店 on_tick 0xc（`0x4171de`）在自己 tick 里 `0x417857` 清指针；GameThread `0x10`（`0x442a97`）本帧随后建店 `0x443bed`；EnemyManager `0x1b`（`0x42dcd8`）、Gui `0x21`（`0x43bac5`）在后 |
| P4′ | GameThread 在 `0x443b05` 之前的提前返回不会造成空档 | **PARTIAL** —— 见下「P4′」：两条提前返回若恰在关店那一帧命中，晚一帧重开（敌人 / MSG 跑一帧）；日志标 `late` 实跑核对 |
| P5 | 第二家店与第一家共用零售代码，无新对象布局假设 | **CONFIRMED** —— 建店 / `AbilityShop__initialize` / on_tick / 析构全是零售路径；offer 重抽自动排除已拥有（`mgr+0xd70` owned，`pick` 与两个保证循环同一门控，SM §4） |
| P6 | 练习 / replay 回放 / 空白卡 / 中途退出都不会多开店 | **CONFIRMED** —— 见下「P6 证据」 |
| P7 | replay 一致性 | **CONFIRMED** —— 见下「P7 证据」 |
| P8 | 暂停菜单期间不会出错 | **CONFIRMED** —— 见下「P8 证据」 |

**P7 证据**：商店抽卡 `0x41717b mov ecx,0x4cf280`（REPLAY_UNSAFE_RNG：AnmVm / ItemManager / StageInner 用；同步 RNG 在 `0x4cf288`，Bullet / ECL / ReplayManager 用），第二家店多抽一次不碰同步 RNG；此外 录制 / 回放在商店期间都停（`0x4629a2` / `0x462aa5` 看指针）；`sub_417880` 每次关店都覆写本关卡组快照与 GlobalsInner（含金钱），回放时第一家店 30 帧内复原最终快照后退出、不成交、不重开 → 与录制时两家店买完的结果一致。实跑核对


**P3 证据**：`0x443aff mov eax,[esi+0xb0]`；`0x443874 mov esi,ecx`（fastcall this）；esi 在 `0x443a8e mov esi,1` 被借用后于 `0x443a96` 从 `[esp+8]`（`0x443877` 存的 this）重载，`0x443a75` 的跳过支路不碰它——两条路到 `0x443b05` 都是 this；`test eax,0x20000` 由 cave 里执行，
`0x443b0a jz` 看它的 ZF；创建后 `0x443c17 and [esi+0xb0],~0x20000`、`0x443c21` 重读 —— 内存位我们不动，不会残留。

**P4′**：`0x44387b`（`[0x4cd624] & 0x180` → return 0，线程注销）与 `0x4439ed`（`this+0xb0 & 4` → 置 `0x80` 后 return 1）都在 `0x443b05` 之前。
前者是关闭 / 切模式，商店时段不会命中；后者的位 4 语义未钉死。真命中那一帧本断点不跑，下一帧才重开，中间敌人 `0x1b` / Gui `0x21` 各跑一帧。
`BP_ce_shop_reopen` 把重开帧与成交帧的差记进日志（零售应为 +31），> 31 标 `(late, gap!)`；`test_pause_after_purchase` 钉住「暂停不走帧号」的前提。

**P6 证据**：练习（`0x4c5f8c >= 0`）与回放（`0x417cd8` 先要 `[0x4cf418] != 0` 再看 `GT+0xd0`）在 `0x417cc7` 30 帧自动退、空白卡走 state 3/4，三者都不经 `0x4183ea`；
`blocked` 再挡一道。中途退出：`ce_shop_on_gamethread` 要求同关且距成交 ≤ 300 帧（`TIME_IN_STAGE` `0x4ccce8` 由 GameThread 尾
`0x443d16` 递增），否则作废；MSG 下一关置位时名额重算。

**P8 证据**：成交后暂停：商店的 UpdateFunc 是 `or [esi+4],2`（`0x4171d4` 一带，暂停时仍 tick），state 5 在暂停下跳过倒计时直接退（`0x417d14 → 0x417d2d`），指针清零 → GameThread 若跑到 `0x443b05` 就重开，
新店 state 0 在暂停下 `return 1` 等到恢复。从暂停退到标题：teardown `0x443711` 走 `sub_417880` → 析构，GameThread 不再 tick，
成交记录靠 P6 的关卡 / 帧数校验作废。

**实跑要看的**：日志 `shop: 2 visits per stage` → 过关 `shop: opened by msg (visit 1/2 …)` → 买 → `shop: bought (…)` →
`shop: reopen (visit 2/2 …)` → 商品重抽且不含刚买的 → 买 → 正常进下一关；replay 回放同一局不出现 `reopen`。

## Q. 音效表扩容（第 11 段）—— 四张表搬进 codecave + 32 个语音 id

一手 [`engine/_shared/th18-sound-table.md`](../../../engine/_shared/th18-sound-table.md)；
设计 [`docs/superpowers/specs/2026-09-05-voice-expand-design.md`](../../../docs/superpowers/specs/2026-09-05-voice-expand-design.md)。
**51 处 binhack 全是「只换那 4（或 1）字节常量」**，opcode 一个不动；唯一手写的机器码是
`th18_snd_patch_init` 那 56 字节（objdump 逐条核对，见 Q1）。站点表 `native/sound_sites.py`、
发射 `native/sound_emit.py`、DLL `native/sound.c`、验收 `make snd-check`（301 条断言）。

| # | claim | 结论 |
| --- | --- | --- |
| Q1 | `patch_init` 的 ABI 与时机正确 | **CONFIRMED** —— 见下「Q1 证据」 |
| Q2 | **I1**：116 行的 `+0` 恰好是 `0..0x73` 的置换 | **CONFIRMED（静态）** —— 见下「Q2 / Q3：两条不变式」 |
| Q3 | **I2**：每行 `+4` 指向的 blob 槽最终非 NULL | **CONFIRMED** —— 同上 |
| Q4 | `0x4713d8` 的 blob 释放尾界刻意只扩基址不扩界 | **CONFIRMED（有意为之）** —— 见下「Q4 证据」 |
| Q5 | `0x45ff38` 硬编码引用的槽 20 改写后仍指同一个音 | **CONFIRMED** —— 见下「Q5 证据」 |
| Q6 | `0x4766ef` 的 cfg 尾界是「行 `+4`」游标，不是行首 | **CONFIRMED** —— 见下「Q6 证据」 |
| Q7 | 51 处站点没有漏、没有误伤 | **CONFIRMED** —— 见下「Q7 证据」 |
| Q8 | 两处计数界的宽度与新值 | **CONFIRMED** —— 见下「Q8 证据」 |
| Q9 | `ce_snd_gate` 的落点与放行 | **CONFIRMED** —— `0x476410` 入口 `55 8b ec 6a ff`（5 字节、无相对寻址，与 `ce_gate` 的 `0x4637d0` 同款）；它就是 SoundManager 的初始化本身，断点返回 1 放行，不改控制流 |
| Q10 | 全有或全无 | **CONFIRMED** —— 自检任一条不过就把 32 个新行退回骨架（`+4/+8/+0x10` 清零），零售 84 个音一字不动 |
| Q11 | `voice.js` 的 id 不靠迭代顺序 | **CONFIRMED** —— 见下「Q11 证据」 |
| Q12 | replay 决定性 | **CONFIRMED** —— `play_sound` 只碰队列与 buffer，不动任何 RNG、不改游戏状态；门只跑一次 |
| Q13 | `+0xc` / `+0x10` 的语义 | 🟡 **未验** —— 见下「Q13」 |
| Q14 | cfg 行 `+8` 的语义（**订正**）| **CONFIRMED** —— 见下「Q14 证据」 |
| Q15 | 新音效的响度基准 | **CONFIRMED（实测）** —— 见下「Q15」 |

**Q1 证据**：`mod_call_type` 是 cdecl 一参、调用方清栈（`plugin.h:71`），所以结尾是裸 `ret` 不是 `ret 4`；
`pushad`/`popad` 覆盖被调方须保留的 ebx/esi/ebp/edi；`cld` 显式保证 `rep movsd` 的 DF=0。
时机：`patch_func_init` 在 `codecaves_apply` 末尾（`binhack.cpp:1724`），**早于** `binhacks_apply`
（`runconfig.cpp:655-656`）——「先把表填好、再让改过的代码去读它」正落在这个缝里，
与卡表的 `th18_card_table_patch_init` 同一条路，已现网验证。
★ 版权红线：cfg 表内容与 wav 名字符串一律 `<Rx…>` 运行时从**用户自己那份 exe** 拷，patch 里零字节。

**Q2 / Q3：两条不变式**。零售 84 行直读 exe 验过 `+0` 是 `0..0x53` 的置换；新行由 patch_init 的骨架
循环写 `84+k`，`+4 = 0`（指零售 wav 0 `se_plst00`）——**没被登记的新 id 只是重复一个零售音**。
违反 I1 的后果是**跑飞不是崩**：循环 1 的扫描 `0x476460`（`CMP [EAX],EDX / ADD EAX,0x14 / JNZ`）没有上界。
违反 I2 的后果是**挂死**：`0x4776f0` 遇 NULL blob 进 `0x477768` 的 `Sleep(10)` 等待循环，
只有 `[0x5704a4] == 2` 才脱身。`ce_snd_gate` 逐行断言两者，不过就还原。
I2 只对 `>= 72` 的下标断言 —— 零售 `0..71` 由预加载线程异步填，门里可能还没好，那个等待循环正是为它们写的。

**Q4 证据**：语音 blob 的字节来自 thcrap 的 `stack_game_file_resolve`（thcrap 的堆），
而 WinMain 的释放循环 `0x4713c7` 用的是游戏的 `0x491a3f`（free）。**跨堆释放必崩。**
所以 `0x4713d8` 的尾界停在零售 72（`<codecave:th18_snd_blobs+120>`）：引擎只回收自己那 72 个，
我们的 32 个谁都不释放。此刻进程正在退出，代价到此为止 —— 用一个有界的泄漏换掉一整类崩溃。

**Q5 证据**：槽 20 = id `0x14` `se_lazer02`（`+0xc = 1`，两个循环音之一，玩家常驻激光音）。
binhack 换成 `<codecave:th18_snd_slots+1e0>`（`20 × 0x18`），`test_sound_emit.py` 钉住这个偏移，
`ce_snd_gate` 再断言该槽的 cfg 行 `+4 == 0x26`。**错了不崩，只是玩家激光没声音** ——
这批里最容易漏检的一处，所以给了独立断言。

**Q6 证据**：循环 2 的 `ESI` 从 `0x4c9b84`（`CFG_BASE + 4`）起、步长 `0x14`，
所以零售尾界 `0x4ca214 = base + 4 + 84*0x14`（注意 `base + 84*0x14 = 0x4ca210` 是另一个全局）。
**计划稿一度写成 `base + 116*0x14`，差 4 字节会少建一行**；`test_sound_sites.py` 用零售立即数反推验证这条算术。

**Q7 证据**：用 `x86imm._decode` 从**已知指令起点正向解码**，不用 `classify` 的回溯 ——
`b8 80 9b 4c 00`（`mov eax,imm32`）会被前一条的尾字节 `ff` 接成 `ff /7`，两种读法都自洽而 tier 判错。
完整性审计扫四段零售地址区间，26 处未覆盖的同值引用**逐个用 Ghidra 操作数级引用交叉验证过真身**：
`0x4ca210` 是输入状态全局（`TEST dword [0x4ca210],0x80103`）、`0x4ca214` 是另一个普通全局、
`0x56d104` 是 SoundManager `+0x2388` 的字段、`0x4b48c0` 是 ANM 的另一张表、
`0x56cee8` 是字节级假阳性（Ghidra 操作数级引用一条都没有）。

**Q8 证据**：`0x401139` 是 imm32（`81 fa e0 07 00 00`）→ `0xae0 = 116 × 0x18`；
`0x476472` 是 **imm8**（`83 fa 54`）→ `0x74 = 116`，仍塞得下（≤ `0x7f`，且 `JL` 看的是有符号）。
`0x4767d8`（预加载 wav 数 `0x48`）**刻意不改** —— 预加载线程仍只从 dat 读零售 72 个，
改了它会让引擎去 dat 里找不存在的语音文件；扫描器每次校验它没变。

**Q11 证据**：thcrap 把栈里每个 patch 的 `voice.js` **深合并**成一个对象，合并后的迭代顺序不由我们决定。
所以 id 显式写在 JSON 里，DLL 按 id 定位 cfg 行并查重；`assets/build_voice.py` 在构建期与
`ORDER.txt` 的行号对账，不一致直接 FAIL（同 `assets/README.md` 对 `cards.js` / ORDER 的既有做法）。

**Q14 证据（订正）**：`+8` **低 word = 音量**（DirectSound 百分之一 dB，≤ 0 的衰减）、
**高 word = 优先级**（0..100）—— 与本节初稿及 2026-09-04 版文档写反了。消费者 `0x477611`
取 `movsx word [cfg行+8]`（低 word），`0x47764d` `SetVolume(vt+0x3c)`：

```
s = [0x5704b4]/100.0        // SE 音量设置；为 0 时直接 SetVolume(-10000) 静音
vol = (1 − (1 − s)³) × (低word + 5000) − 5000
```

（`0x4b9178 = 1.0`、`0x4b9304 = 100.0`，直读 exe。）设置拉满时 `vol` 就等于低 word。
**声像不在表里** —— `0x4775d9 SetPan` 用的是队列里存的世界 x（`play_sound` 的 xmm2 参数）。
影响：`sound.c` 一度按「高 word = 音量、低 word = 声像」写，`voice.js` 的 `volume`/`pan`
两个字段语义都是错的（凑巧 `volume:100 / pan:0` 落成「优先级 100 / 0 dB 不衰减」才没暴露）。
已改成 `volume_db`（−5000..0）+ `priority`（0..100），旧字段出现即报错。

**Q15**：`SetVolume` 范围是 `−10000..0`，零售 84 行的低 word 也全部 ≤ 0 ——
**那个字段只能衰减不能增益，所以响度只能由 wav 承载**。实测零售 71 个 wav：
peak 中位 `−0.06` dBFS（一律 peak 归一化）、RMS 中位 `−13.1`、最响 `se_enep01` `−4.5`
（`se_release` `−5.1`）。语音基准定 `VOICE_RMS_TARGET = −10.0` dBFS、peak ≤ `−0.5`，
记在 `engine/_shared/th18-sound-table.md` §9 与 `assets/build_voice.py` 的同名常量，
`make voice` 每次打印实测值、偏离 ±4 dB 提醒。

**Q13**：`+0xc == 1` 只有 id `0x14`（`se_lazer02`）与 `0x37`（`se_ch03`）两个持续音，推测是循环标志；
`+0x10 == 0` 的六个是 `0x00` `0x01` `0x07` `0x08` `0x09` `0x0a`（全是菜单音）。
新行取 `+0xc = 0`、`+0x10 = 1`（跟游戏内 SE 一致）。**猜错的后果是语音表现异常，不是崩。**

**实跑要看的**：`snd: caves …` → `snd: voice id 0x54 "ROYAL_RAGTIME" …` →
`snd: OK 1 voices, 116 rows, I1/I2 hold` —— 这三行与是否触发无关，扩表本身就验掉了。
听得到的验证在皇家同花顺：买齐五张黑桃 → `royal: show anm id … + ragtime`，
拉格泰姆从演出第 0 帧起，帧 60 的 trophy 音效**叠在**它第二小节的强拍上（可叠加）；
Alt-Tab 切出切回后仍能放（`0x45a4a0` 的重播循环）；
退出游戏不崩。**没有任何 `snd:` 行** = 断点没触发；**卡在黑屏** = I2 被违反且自检没拦住；
**玩家激光没声音** = Q5。

## R. 黄昏（id 68）—— 最后一颗炸弹自动再放一发

**没有手写机器码、不开断点、不改任何引擎字节**：只在 `on_tick_2` 里读一个标志、在边沿上调一次
引擎自己的 `do_bomb()`。一手全部来自 2026-09-05 的反汇编。

| # | claim | 结论 |
| --- | --- | --- |
| R1 | `[0x4cf2b8]+0x30` 是「正在放炸弹」标志 | **CONFIRMED** —— 见下「R1 证据」 |
| R2 | `do_bomb` = `0x420360`，无参 cdecl，plain `ret` | **CONFIRMED** —— 见下「R2 证据」 |
| R3 | 第二发不会把炸弹数扣成负 | **CONFIRMED** —— 扣数在 `0x4574d0`：`add [edx+0x7c],-1` 后立刻 `jns` / `mov [edx+0x7c],0` **钳 0**，再 `cmovg` 钳 MAX。所以我们**完全不碰** `CURRENT_BOMBS` |
| R4 | 从 AbilityManager tick 里调 `do_bomb` 是安全的 | **PARTIAL** —— 见下「R4」 |
| R5 | 只接一次，不会无限连 | **CONFIRMED** —— 接出来的那一发带 `chained` 标记，它的 0→1 边沿不再武装；它结束时清标记 |
| R6 | 失败是安全的 | **CONFIRMED** —— `do_bomb` 自己的两个守卫（`+0x30` 正在放、`+0xa0`）照常生效，被拦下只 `or eax,-1; ret`，不改任何状态；我们据此把 `chained` 退回 |
| R7 | 自机指针为空时不调 | **CONFIRMED（防御）** —— `do_bomb` `0x4203d4` 读 `[0x4cf410]+0x620` 当声像，为空会崩；`on_tick_2` 里先判 `CE_PLAYER()` |
| R8 | `+0x30` 由谁清零 | 🟡 **未验** —— 见下「R8」 |

**R1 证据**：`do_bomb` `0x420382` `mov [esi+0x30],1`（`esi = [0x4cf2b8]`），且入口 `0x42036b`
`cmp [esi+0x30],0 / jnz 失败`；`Bomb__can_bomb_and_deathbomb_check` `0x420432` 也查
`cmp [eax+0x30],1 → 不可炸`。三处互证它就是「这一发还在进行」。

**R2 证据**：`0x420360` 序言 `push ebp; mov ebp,esp; push ecx; push esi`，收尾
`pop esi; mov esp,ebp; pop ebp; ret`（**plain ret，无栈参**）；两个出口分别 `xor eax,eax`（成功）
与 `or eax,-1`（被拦）。引擎调用点：`Player__on_tick__body` `0x45c051`（case 1，
先 `call 0x420420` 查可炸、再 `test byte [0x4ca434],2` 查炸弹键）与 `0x45c2c3`（决死窗口）。

**R4**：`on_tick_2` 的宿主是 `AbilityManager__on_tick` `0x408640`，与 `Player__on_tick__body`
同在游戏线程、同一帧内，只是**早于**玩家 tick。`do_bomb` 只写炸弹管理器自己的字段、调
`GlobalsInner` 的扣数、放音效、再走炸弹对象的虚表 —— 都不依赖「玩家 tick 已经跑过」。
**但这条只是静态推断，实跑要确认第二发的表现与手动放的一致**（尤其决死窗口期间）。

**R8**：只确认了谁**置 1**（`do_bomb`）和谁**读**（`can_bomb`、我们），没确认谁清零
——它应该在炸弹结束时由炸弹对象自己清。**本设计对此免疫**：清不了就是永远等不到 1→0 边沿、
永远不接第二发，不崩不错乱；日志 `dusk:` 一行都没有就是这个情况。

**实跑要看的**：`_test` 起手卡组已带 68。把炸弹用到只剩 1 颗 → 再放一发 → 那一发结束时
日志 `dusk: last bomb spent -> chaining a second one (do_bomb -> 0)`，画面上炸弹**再放一次**，
右上角炸弹数仍是 0（第二发的扣数被钳住）。`-> -1` = 被守卫拦下（R6）；一行都没有 = R8。
剩 2 颗以上时用炸弹**不该**有任何 `dusk:` 行。

## S. 加倍（id 69）—— Miss 掉 2 命 + 敌人掉落 ×2

桥牌的 Double：罚分加倍、奖励也加倍。**没有手写机器码**；一个新断点（放行原指令）+ 一个虚表槽。

| # | claim | 结论 |
| --- | --- | --- |
| S1 | 掉 2 命必须挂 `+0x0c`，不能挂 `+0x14` | **CONFIRMED** —— 见下「S1 证据」 |
| S2 | game over 判的是 `< 0`，不是精确 `== -1` | **CONFIRMED** —— 见下「S2 证据」 |
| S3 | 残机不会被我们压成负数 | **CONFIRMED** —— 只在 `CURRENT_LIVES > 0` 时多扣；引擎那一下照常，`< 0` 就是它自己要的 game over |
| S4 | `0x430510` 是 thiscall、`ecx` = 敌人，入口可挂断点 | **CONFIRMED** —— 见下「S4 证据」 |
| S5 | 掉落数表在 `enemy+0x04`，20 个 int32 | **CONFIRMED** —— 见下「S5 证据」 |
| S6 | 在撒**之前**改 counts，引擎会照单全收 | **CONFIRMED** —— 断点在函数入口（`sub esp,0x20` 之后什么都还没做），读表的第一处是 `0x43054a mov edi,[eax-0x50]` = `[enemy+0x04]`，在我们之后 |
| S7 | 为什么不用 `+0x30` 槽 | **CONFIRMED** —— 见下「S7 证据」 |
| S8 | 断点是全局的，效果却只在带卡时生效 | **CONFIRMED** —— 断点沿 AbilityManager 的卡链表派发 `on_enemy_drop_pre`，只有登记了这个槽的卡（只有 69）会被调到；不带这张卡时链表里没有它，counts 一个字节都不动 |
| S9 | 道具池会不会被翻倍撑爆 | 🟡 **未验** —— 见下「S9」 |
| S10 | **实跑退回（2026-09-07）**：小怪掉落没加倍 | **CONFIRMED 根因 + 已修** —— 见下「S10 证据」：小怪的掉落是 `dropMain` 的**主掉落**（`zEnemyDrop+0x00`），引擎在调 `0x430510` **之前**就撒了，我们只翻了 `+0x04` 的附加表（`dropExtra`，杂兵基本不用）。修：主掉落类型补进附加表对应格 +1，引擎的附加循环再撒一颗 |

**S1 证据**：死亡序列（`engine/card/th18/03-hooks.md` §4）是
`决死窗口耗尽 → for card: acc |= vtable[0x0c]` → `acc == 0` 才进
`Player__commit_death_and_enter_state2` `0x45D090`（**扣命、判 game over**）→ 状态 2 第 3 帧
才轮到 `vtable[0x14]`。所以只有 `+0x0c` 在引擎扣命**之前**。挂 `+0x14` 的话惩罚与判定都
结束了，我们再扣一命就绕过了 game over，残机能停在 −1 而游戏继续。
与 `CardTewi` 在 `+0x0c` 里只记账（`0x40A720` 存 MONEY）然后 `return 0` 是同一手法。

**S2 证据**：`0x45d19b` 起
`mov eax,[0x4ccd48] / sub eax,1 / mov [0x4ccd48],eax / **js** 0x45d1c7`（< 0 就跳过刷 HUD），
随后 `0x45d1c7 test eax,eax / **js** 0x45d21c`（< 0 就跳过重置炸弹，直奔状态 2 的 game over 路径）。
**两处都是 `js`（判符号位）**，所以 1 → 0 → −1 会正常触发 game over；若是精确 `== -1` 就会被跳过。

**S4 证据**：`0x430510` 入口 `55 8b ec 83 ec 20`（`push ebp; mov ebp,esp; sub esp,0x20`）
—— 6 字节、无相对寻址，与 `ce_item_money` 那批同款形状；`0x430518 mov [ebp-0x4],ecx`
把 `this` 存下来，之后全程用它，**thiscall、`ecx` = 敌人**。

**S5 证据**：`0x430539 lea eax,[ecx+0x54]` 存进 `[ebp-0xc]`，
`0x43054a mov edi,[eax-0x50]` = `[enemy+0x04]` 取 type 1 的掉落数；
广播后 `memset(&enemy+0x04, 0, 0x50)` = `0x50` 字节 = **20 个 int32**（type 1..`0x13`）。
`enemy+0x54` 是另一个字段（`0x43055b mov ecx,[eax]` 读它做部分击破的折算），**不在表内、不要碰**。

**S7 证据**：`+0x30` 是**撒完之后**才广播，且广播完立刻 `memset(&enemy+0x04, 0, 0x50)` ——
在那里改已经晚了，只能自己再调 `ItemManager__spawn_items` `0x446f40`
（thiscall + **8 个栈参**、`ret 0x20`）。而零售仅有的两个调用方 `CardKaguya` `0x40D2E0`
与 `CardYachie` `0x40D630` **都留了没填的参数槽**，8 个槽的语义推不全，ABI 风险不小。
改 counts 让引擎自己撒，各 type 的角度 / 速度 / 位置一个都不用我们管。

**S9**：翻倍后同一帧撒的道具数是零售的两倍，理论上可能顶到 ItemManager 的池上限。
零售自己在 boss 击破时就会撒很多，池应该有余量；且撒不下时引擎自己会失败返回，不会崩。
实跑时看大批敌人同时死的场面有没有掉落变少 / 卡顿。

**S10 证据**：`0x430510` 的 `ecx` 不是敌人而是 `zEnemyDrop`（`0x430499 lea ecx,[edi+0x623c]`；ECL `dropItems` 的 `0x42d0c0` 同样 `mov ecx,esi` = 该结构）。
结构：`+0x00 main_drop`、`+0x04 ex_drop[20]`（ExpHP `zEnemyDrop`，与函数内 `local_8+0x54-0x50` 一致）。两处调用点都是
`if (main_drop) spawn_item(main_drop, pos, -π/2, 2.2, 0)` → **然后**才 `call 0x430510` → 之后 `main_drop = 0`（`0x430469..0x4304aa`、`0x42d0c6..0x42d0fd`）。
ECL 一手：st01–07 共 `dropMain` 50 处（类型 1 / 2 / 16 / 19）、`enmCreate` 第 6 参 0 / 1 / 2 / 16–19，`dropExtra` 95 处几乎全在 boss / 中型敌人的 `BossItem` 里
——所以杂兵 = 主掉落一颗，之前的实现对它一个字节都没改。附加循环只撒 type 1..`0x13`（`0x12 < i` 退出），第 20 格永远不撒
（层里记作 `native/sdk_core.h` 的 `CE_DROP_MAX_MAIN 19`）。
**修法（2026-09-12 改为乘法层）**：卡 69 不再自己算补偿，只在 `on_load` 里声明 `ce_drop_mult_set(69, 2000)`；
SDK 的掉落层在 `BP_ce_enemy_drop` 的**卡循环之后**一次性 `ce_drop_apply(counts)`：
`final[t] = (base[t] + Σ 加法) × Π 乘数`（`base[main] = counts[main] + 1` 即那颗已飞出的主掉落），
再写回 `counts[t] = final[t] − (1 if t == main)`，撒的活仍由引擎的附加循环干（散射位置按 `dropArea`，方向朝上）。
⇒ 与卡链顺序无关，且**别的卡加到的那颗也一起翻倍**（`(1+1)×2 = 4`，旧实现给 `1×2+1 = 3`）。
纯逻辑单测在 `tests/test_sdk_core.c` 的掉层用例（原 `cards/double_core.c` 退役）。
**2026-09-12 对抗审计抓到两处**：① 层原来收的是 `counts`（= 基址 +0x04）而不是 `zEnemyDrop` **基址**
⇒ 整体错开一个 type 格（主掉落补偿补到别的类型上）；已改成传 `regs->ecx` 本身。
② `zEnemyDrop` 除 `+0x04 ex_drop[20]` 外还有 `+0x54 timed_ex_drop[20]`（引擎撒 `ex[i] + timed[i]×(T−timer)/T`，
一手 `0x43053f`–`0x430570`）⇒ 只乘 ex 表会漏掉计时加成那些颗；现在两张表都乘（timed 不做「已撒一颗」补偿）。
**⏳ 未复验**：S10 的实跑验收（主掉落 ×2、非 0 附加 ×2、第 20 格不撒）在层改完后必须重跑一遍。

**实跑要看的（层改完后必须重跑）**：`_test` 起手卡组已带 69。打杂兵 → 掉落**明显翻倍**（火力、点数、金钱都是）；
Miss 一次 → 残机一次少 **2**，日志
`double: miss costs 2 lives (now N before the engine's own -1)`；
残机 1 时 Miss → 直接 game over（不该出现残机 −1 还能玩）。
`sdk:` 那行应报 `ce_enemy_drop` 断点已应用，否则门会 `FAIL`。

## T. 腐化（id 70）—— 放炸弹消耗上限而不是当前数

**没有手写机器码**：一个新断点（放行原指令）+ 两个虚表槽。

| # | claim | 结论 |
| --- | --- | --- |
| T1 | `0x4203bc` 不多不少覆盖每一次炸弹消耗 | **CONFIRMED** —— 见下「T1 证据」 |
| T2 | 断点落点可用（5 字节、无相对寻址）| **CONFIRMED** —— `a1 c0 f2 4c 00` = `mov eax, ds:0x4cf2c0`，绝对 moffs，整条可搬进 cave |
| T3 | `CURRENT_BOMBS += 1` 是**精确**还原 | **CONFIRMED** —— 见下「T3 证据」 |
| T4 | 每关开场会把当前数压回 `min(3, 上限)`，需要抵消 | **CONFIRMED** —— 见下「T4 证据」 |
| T5 | 上限只减不增、下限 0 | **CONFIRMED** —— `if (MAX > 0) MAX -= 1`；上限 0 时 consume 的 `min(cur,max)` 会把当前数也钳成 0，`can_bomb`（`0x420420` 查 `CURRENT > 0`）随即拒绝，炸弹彻底用完 |
| T6 | HUD 刷新用对了函数 | **CONFIRMED** —— `0x4420e0` thiscall(gui; bombs, fragments, max) `ret 0xc`，与残机的 `0x441f10` 完全对称；零售在 consume（`0x457501`）与每关开场（`0x442798`）都这么调 |
| T7 | 与黄昏（68）同时携带不会打架 | **CONFIRMED** —— 见下「T7」 |
| T8 | 上限顶到 7 不会越界 | **CONFIRMED** —— 7 就是零售开局值（`0x4428ed` / `0x442920` / `0x442957` 三处 `mov [0x4ccd64],7`），也是 `CardBomb` 加上限的钳位值，HUD 画得下 |

**T1 证据**：全 `.text` 扫 `E8` 相对调用，目标为 `consume_bomb` `0x4574d0` 的**只有 `0x4203b7`
一处**，而它就在 `do_bomb` 里。所以挂在它返回后的 `0x4203bc`，覆盖面 = 每一次炸弹消耗，
且不会被别的路径误触发。同帧改完，玩家看不到 HUD 闪动（用 `on_tick_2` 的边沿要等下一帧）。

**T3 证据**：`consume_bomb` `0x4574d0` 做三件事：`add [edx+0x7c],-1`；`js → mov [edx+0x7c],0`
（钳 0）；`cmovg` 钳 MAX。进 `do_bomb` 之前 `Bomb__can_bomb_and_deathbomb_check` `0x420420`
已保证 `CURRENT > 0`，而不变式 `CURRENT <= MAX` 恒成立，所以**两次钳位都不会触发** ——
扣完必然正好是 `CURRENT-1`，`+= 1` 精确还原。

**T4 证据**：`GameThread__thread_start` 在 `0x442745`–`0x44278f`：
`eax = 3; cmp [MAX],eax; cmovl eax,[MAX]; … mov [CURRENT],eax` = **`CURRENT = min(3, MAX)`**，
**不碰 MAX**（那三处 `MAX = 7` 在 `cmp edx,2` / `test edx,edx` 的分支链里，是开局初始化，
同时还设残机 0/2/7）。所以腐化的上限会在一局内持续累积 ✅；但当前数会被压回 3，
显示出来的"预算"就不对了。卡的 `on_stage_start`（`+0x34`，同一函数 `0x442F98` 广播、
**在 `0x44278f` 之后**）把当前数拉回 = 上限。**它不加东西，只是不让补给把预算显示改小。**

**T7**：黄昏靠「炸弹结束时 `CURRENT == 0`」判断是不是最后一颗。腐化把当前数还原了，
所以两张同时在场时，黄昏只在**上限降到 0**（当前数也被钳成 0）那一发才接第二发 ——
语义自洽：那确实是最后一发。时序上也不打架：腐化在断点里同帧改完，黄昏的 `on_tick_2`
下一帧读到的已经是改好的值。

**实跑要看的**：`_test` 起手卡组已带 70。开局日志
`corruption: acquired — bombs 7/7 (…)`；放一发 → **炸弹数 7 → 6**（不是"当前数 3 → 2"），
日志 `corruption: bomb spent the cap — bombs 6/6 left`；过关后仍是 6/6（**不该**回到 3）；
放到 0/0 后按炸弹键没反应。`sdk:` 那行要报 `ce_bomb_spent` 断点已应用，否则门会 `FAIL`。

## U. 破损核心（id 71）—— 第一张走零售装备卡机制的卡（子机）+ 定点伤害源闪电

**没有手写机器码**：一处内联汇编照零售调用点复刻 `Player__allocate_option` 的压栈序；伤害走 `ce_damage_rect`
（Remilia / 青眼同款）；特效是两条 ANM VM。**不改任何游戏资源文件**（第一版曾追加 `pl0X.sht` 的 shooterset 开火，
2026-09-05 改掉：那条路弹要飞过去，与「瞬发单体」相悖；工具与研究留着，见 U16）。
引擎一手：[`engine/card/th18/03-hooks.md`](../../../engine/card/th18/03-hooks.md) §5、
[`engine/player/th18/02-damage-sources.md`](../../../engine/player/th18/02-damage-sources.md)、
[`engine/sht/th18/`](../../../engine/sht/th18/README.md)。

| # | claim | 结论 |
| --- | --- | --- |
| U1 | `Player__allocate_option` 的调用约定 | **CONFIRMED** —— 见下「U1 证据」 |
| U2 | 子机指针不能存 `card+0x54` | **CONFIRMED** —— 见下「U2 证据」 |
| U3 | 认领子机槽不会误用别人的 | **CONFIRMED** —— 12 个槽是公共池；用前查 `option+0x00 != 0` 且 `option+0xd0 == card+0x08`（`0x40a790` 把后者写进前者）。不成立就松手重申请 |
| U4 | 子机的贴图来自 `ability.anm` | **CONFIRMED** —— `allocate_option` 的最后一参是脚本号，取 `AbilityManager->ability_anm`（Reimu 2、Alice `0x10`）。我们给 script88 |
| U5 | 子机坐标的读法 | **CONFIRMED** —— `option+0x5c/+0x60` 是定点 int32，`× 0x4b908c = 1/128` 变像素（`0x40a9c0`、`0x40b6f4` 两处一手）|
| U6 | 开火挂 `on_tick_2` | **CONFIRMED** —— AbilityManager 每帧、菜单 / 商店不跑；节奏是 C 的计数器（`bc_tick`，主机单测）。不依赖 `+0x1c on_tick_shooters`（那个只在按住射击键时广播）|
| U7 | 遍历敌人链表安全 | **CONFIRMED** —— 与零售最近敌人搜索 `0x438cb0` 同一套规则（`EnemyManager+0x18c`、跳过 `+0x635c & 0xc000021`、坐标 `+0x1270/+0x1274`）；只在 `on_tick_2` 读，不跨帧缓存 enemy 指针 |
| U8 | 定点伤害源钉在目标上 = **单体** | **CONFIRMED** —— 见下「U8 证据」（2026-09-06 起判定 32×32、一道拆 4 帧 × 30）|
| U9 | 同一敌人只结算**一次** | **CONFIRMED** —— 见下「U9 证据」 |
| U10 | 判定框的 w / h / angle 约定 | **CONFIRMED** —— `0x45f0f0` 矩形分支：把 `敌人 − 源` 旋转 `−angle` 后，`w/2` 比沿 angle 的轴、`h/2` 比垂直轴，敌人按半径 `param_3` 的圆做圆角矩形判定。我们 angle = 0、24×24，就是轴对齐的小方块 |
| U11 | 伤害走正常管线 | **CONFIRMED** —— 同一函数末尾 `min(本帧合计, player+0x47984)` 钳每帧上限、`0x4cccfc` 计分。与主炮 / 青眼同一条路 |
| U12 | 伤害会被每帧上限钳 | **已规避（2026-09-06）** —— 一道 120 拆成 4 帧 × 30：单帧 30 + 主炮同帧最多约 31（Sakuya P4 名义 1880/60）≈ 61，只在 Sakuya 满火力时被 60 钳掉 1；其余自机（Reimu 90 / Marisa 160 / Sanae 120）不钳。第一版 80 单帧落地时 Sakuya 会被钳到 60、再被主炮分走，实际只剩约 30 |
| U13 | 电弧 / 火花 VM 的 pos / rotation / scale 由 C 写不会被脚本盖掉 | **CONFIRMED** —— 见下「U13 证据」 |
| U14 | replay 安全 | **CONFIRMED** —— 节奏只依赖帧计数，目标只依赖敌人坐标，角度 / 距离是我们自己的确定性 SSE 算术（`bc_atan2f` 多项式、`sqrtss`；`-fno-math-errno` 让它不掉进 libm）。`make dllx87` 仍报 0 |
| U15 | 不改任何游戏资源 | **CONFIRMED** —— 只多 `ability.anm` 的两个 entry + 三个脚本（本来就在重建）。`pl0X.sht` 不再发（`append_shooterset.py` 的 `APPEND` 为空 → 不产出；`release.py` 目录镜像会把 modkit 里旧的删掉）|
| U16 | SHT 工具与研究仍成立 | **不受影响** —— `engine/sht/th18/` 两篇是独立结论；`assets/sht/append_shooterset.py` 的三条不变式校验照旧；`sdk.h` 的 `ce_tick_shooters_for_card` 留着（无调用者）。将来真要「子机连射」的卡，`APPEND` 加一项就回来 |

**U1 证据**：`CardReimu1__on_power_level_change` `0x40aae0` 逐指令是
`push 2 / push 0x30 / push ecx / push 0x30 / push ecx`、`ecx = card`、`call 0x40a790`；
调用方**不动 esp** ⇒ callee `ret 0x14`。**五个栈参里第 1、3 个是 card 自己**，
第 2、4 个是非聚焦 / 聚焦的横向偏移，第 5 个是 `ability.anm` 的脚本号。反编译给的形参名会骗人，
`sdk.h` 里按指令序原样复刻。

**U2 证据**：新卡对象是基类 `zAbility`，`new(0x54)` 固定 `0x54` 字节（`SDK.md` §2），
零售存子机指针的 `+0x54` 已经在对象外。改存 `ce_state()`。
反过来 **`card+0x1c` 是安全的**：`allocate_option` 自己往那儿写子机的 anm vm id，
SDK 的对象布局里 `+0x18`/`+0x1c` 没人用（列表结点占 `0x0c`–`0x17`，计时器从 `0x20` 起）。
零售的 `CardReimu1__operator_delete` `0x40ab20` 在析构时按它删 VM；我们在 `on_load` / `on_run_reset` 里做同一件事。

**U13 证据**：script89 不碰 `rotate*`；第 0 帧的 scale 由 C 直接写，第 1 帧起脚本每帧
`scale(%F0, 随机 %F1)` 抖动，其中 `%F0` = C 写进 `vm+0x4b4`（`float_script_vars`，ExpHP zAnmVmPrefix）的 scale.x。
script90 的 `scaleTime` 只在火花自己身上。rotation 字段 `vm+0x3c`（自机弹每帧更新 `0x45edb0` 写 `+0x44` = rz）。

**U8 证据**：`ce_damage_rect(center = 目标坐标, angle 0, life 2, dmg 30, 32 × 32)` × 连续 4 帧（开火帧 + 后 3 帧，坐标是开火时记下的）。判定只罩住目标中心
±16 px；别的敌人要被打到得和目标叠在一起。这与 Remilia 脉冲（`0x40f4a6`，宽 32 矩形）、
Tenshi 要石是同一个原语，只是尺寸小、寿命短。

**U9 证据**：`enm_compute_damage_sources` `0x45f0f0` 在结算前：
`if (param_7 != 0) { if (src+0x84 == param_7) skip; src+0x84 = param_7; }` —— `param_7` 是被检查的敌人的 tag，
一个伤害源对**同一个**敌人连续几帧只结算第一帧。所以寿命 2 帧 = 正好一次 30（另一帧被 tag 守卫跳过）；4 个源 = 4 × 30 = 一道 120，
寿命给 2 只是为「源在 AbilityManager tick 建、EnemyManager tick 结算」留一帧余量。

**实跑要看的**：`_test` 起手卡组已带 71。日志应有
`sdk: 71 bound (.on_power_level_change = …, .on_tick_2 = …, .on_load = …, .on_run_reset = …)`
→ 进关 `broken_core: option allocated (ptr …, anm id …)` → 有敌人时每 1 秒一行
`broken_core: fire #N at frame …, orb (x, y) -> target (x, y) dist … angle …`（只打前 3 发与每第 25 发）。
画面：自机右侧一颗青白电球（慢转、亮度呼吸）；每 1 秒一道电弧**瞬间**连到最近的敌人、命中点一团火花、
电流噪声，敌人血条掉一小截（boss 血量 ÷100 的 devstage 里看得很清楚）；没有敌人时攒着不发。
进商店电球收起、出店回来；过关不消失（装备卡）。
崩溃优先怀疑：U1 的压栈序、U3 的槽认领、`ce_damage_rect` 的栈参（青眼同一个包装，O29b）。
视觉怀疑：电弧方向 / 长度不对 → U10 的角度约定或 `anchor(1, 0)` 的语义（青眼光束用的同一招，O29）。

## V. 炎魔之王拉格纳罗斯（id 72）—— 扣火力的主动召唤物 + 飞行火球（8 个定点伤害源）

**没有手写机器码**：三个新引擎入口（扣火力 / 重建子机 / RNG）都是普通函数指针调用，签名从零售调用点原样抄来；
挡弹 / 伤害源 / ANM 全是青眼与破损核心用过的包装。**不改任何游戏资源文件**（`ability.anm` 多三个 entry + 四个脚本，
`abcard.anm` 多一对卡图，两条语音走 §Q 的机制）。设计 `docs/superpowers/specs/2026-09-06-firelord-design.md`。
零售原型：`CardTsukasa__c_press` `0x410e60`（本作唯一一张按 C 扣火力的卡）。

| # | claim | 结论 |
| --- | --- | --- |
| V1 | `GlobalsInner__spend_power` 的调用约定 | **CONFIRMED** —— 见下「V1 证据」 |
| V2 | 门槛 3.00 而不是 2.00 | **CONFIRMED（设计）** —— 见下「V2 证据」：`spend_power` 永远保留 1.00，零售 Tsukasa 的门槛就是「成本 + 一档」 |
| V3 | `Player__repopulate_options_and_notify_cards` 的调用约定 | **CONFIRMED** —— 见下「V3 证据」 |
| V4 | 在我们的 `on_activate`（SDK 桩里）调 repopulate 安全 | **CONFIRMED** —— Tsukasa 就是在 `c_press`（同一条 C 键分派链）里调的；尾部的 `on_power_level_change` 广播让装备卡重申请子机，与火力档变化时的零售行为相同。我们的 `on_power_level_change` 桩不会重入 `on_activate` |
| V5 | RNG 选对实例、调用约定对 | **CONFIRMED** —— 见下「V5 证据」：`0x402740` thiscall 无栈参；`0x4cf288` 是 gameplay 的 replay 安全流，`0x4cf280` 是商店 / UI 流 |
| V6 | 多抽 RNG 不破坏 replay | **CONFIRMED（推理）** —— 抽取只发生在确定性的帧（第 480k 帧、到位帧），次数只依赖敌人数与帧号；同输入同种子必然同序列。带 mod 的 replay 本来就只能带 mod 看（任何改行为的卡都如此），与零售卡（Suwako 1/8 爆炸也抽这条流）同一性质 |
| V7 | 火球爆炸恰好 600 且不被每帧上限钳 | **CONFIRMED** —— 12 帧各起一个**新** `ce_damage_rect`（寿命 2、50、96×96）；tag 守卫（U9）让一个源对同一敌人只算一次 ⇒ 12 × min(50, 每帧上限)；Sakuya 的 60 ≥ 50 ⇒ 不钳（U12 同款）。2026-09-06 由 8 帧 × 64×64 加到 12 帧 × 96×96 |
| V8 | 火球非追踪、按开火时坐标落地 | **设计** —— `fl_launch` 记下 `(tx, ty)`，`n = ⌊d/8⌋ + 1` 帧恰好到点（每帧位移 = d / n，不过头）；敌人走开就打空。原地投（d = 0）至少飞 1 帧、不除零（单测覆盖）|
| V9 | 火球 / 本体 VM 的 pos / rotation 由 C 写不会被脚本盖掉 | **CONFIRMED** —— script92 不碰 `rotate*`（scale 归它的 `scaleTime` 脉动循环，U13 同一分工）；script91 **不碰 pos**（青眼 script78 的 `posTime(30, 4, 0,0,0)` 空转循环这里改成 `nop()`，pos 全归 C）；script93 / 94 一次性、钉在起它那帧的坐标 |
| V10 | 复用青眼 script86 / 87 做血条 | **CONFIRMED** —— 两个脚本是 `drawRect(1,1)` 根 VM，没有任何卡专属的东西；pos / scale / color 全由 C 每帧写（O29k）。两张卡各起自己的一对 VM，互不干扰 |
| V11 | 敌人链表遍历 | **CONFIRMED** —— 与 U7 同一套（`+0x18c` 链表、`+0x635c & 0xc000021`、`+0x1270/+0x1274`），两遍都在同一个 `on_active_tick` 里、不缓存 enemy 指针 |
| V12 | replay 安全的算术 | **CONFIRMED** —— 随机只来自游戏 RNG；smoothstep / 位移 / `bc_atan2f` / `sqrtss` 全是确定性 SSE；`make dllx87` = 0 |
| V17 | 震屏工厂 `0x476060` 的调用约定 | **CONFIRMED** —— 见下「V17 证据」：fastcall(ecx = type, edx = time; start, end, 0, `0x5b`) `ret 0x10`；type 1 的 on_tick 用 UI RNG，不进 replay 流 |
| V18 | 粒子 / 火星脚本里的 `%RANDF` 不进 replay 流 | **CONFIRMED** —— ANM 的随机读 `0x4cf280`（AnmVm 代码 `FUN_00405d70` / `FUN_00407590` 的 xref），与商店 / UI 同流。VM 量：飞行粒子 + 身体火焰各 2 颗/帧 × 27 帧、火星 10 颗 × 33 帧 ⇒ 同时 ≤ ~120 个一次性 VM |
| V19 | 登场聚气期间本体不存在 | **CONFIRMED（代码）** —— `intro_left > 0` 时 `on_active_tick` 只走登场分支：不查本体 VM、不消弹、不计帧；`fl_step` 早返回。`dismiss` 在聚气中也能撤（`intro_left` 计入「有东西要清」）。聚气粒子 6 颗/帧 × 20 帧 ⇒ 峰值 +120 个一次性 VM，只持续 1 s |
| V16 | 呼吸浮动 `fl_bob_dy` 不引 x87 | **CONFIRMED** —— 头里的 `static inline`（bc_atan2f 同理：i386 返回 float 走 st0）；`make dllx87` = 0。判定 / 血条 / 画面三者用同一个浮动值，不会「看着挡住了实际没挡」|
| V13 | 私有状态放得下 | **CONFIRMED** —— `sizeof(fl_state_t)` = 112 ≤ `CE_STATE_BYTES − CE_STATE_RESERVED` = 240 |
| V14 | 语音登记与响度 | **CONFIRMED** —— ORDER 第 1、2 行 → id `0x55` / `0x56`（`make voice` 与 `voice.js` 对账）；转换后 RMS −10.7 / −11.2，在 −10 ± 4 dB 带内；攻击语音 4.3 s < 8 s 周期，不自叠 |
| V20 | 爆炸帧抬 `player+0x47984` 到 300 | **CONFIRMED** —— 见下「V20 证据」：Remilia 同款、时序保证只对当帧生效 |
| V21 | 火球消弹不与本体挡弹串扰、不动 hp | **CONFIRMED** —— `ce_cancel_radius` 调前清 `0x7a41e8` 计数器、调后读回，三次调用（本体 / 飞行 / 落地）各自独立；hp 只按本体那次的返回值扣。`max_count` 9999 不会提前停 |
| V15 | HUD 火力显示不用手动刷 | **CONFIRMED** —— Tsukasa / 商店 / 死亡三处扣火力都没调任何 HUD 刷新函数（对比残机 / 炸弹行要调 `0x441f10` / `0x4420e0`）；火力条每帧读 `CURRENT_POWER` |

**V20 证据**：自机弹的伤害也走同一个源池（`PlayerBullet__create` 建源、`Player__tick_bullets` 每帧刷 dmg；`engine/player/th18/02-damage-sources.md` §0、th16 §08 同构），
敌人侧 `0x45f0f0` 把命中它的所有源**合计**钳 `player+0x47984`（`0x45f28b`）⇒ 上限是「每个敌人每帧」的总配额，自机弹与卡的源共用。第一版 50 × 12 在自机开火时几乎被吃光。
零售先例 `CardRemilia__on_tick_2` `0x40f48e`：`mov [edi+0x47984], 0x12c` 紧接着 `call 0x45dfa0` 放 200 的源。复位点 `GameThread__on_tick` `0x443d3b` 每帧从 `sht+0x28` 写回；
tick 顺序 GameThread `0x10` → AbilityManager `0x16`（我们的 `on_active_tick` 在这里）→ Enemy `0x1b` ⇒ 写入只影响当帧的敌人结算，下一帧自动复位，不用还原、不留残值。
`ce_raise_damage_cap` 只抬不降（已 ≥ 300 时不写）。300 − max(自机弹上限 160) = 140 > 100 ⇒ 火球的 100 整份生效。

**V1 证据**：`0x457480`：`mov eax, ecx`（this = GlobalsInner `0x4cccdc`）、`mov edi, [eax+0x5c]`（CURRENT_POWER）、`mov ecx, [eax+0x64]`
（POWER_PER_LEVEL）、`cmp edi, ecx; jg` 否则 `xor eax, eax; ret 4`；`mov edx, [ebp+8]`（amount）、`sub edi, edx`、写回、`< ecx` 则钳回 ecx；
`(旧值+amount)/档 != 新值/档` → `setnz cl; mov eax, ecx`；两个出口都 **`ret 4`**。零售调用点 `0x410f1c` `push [0x4ccd40]; call`（ecx 早已 = `0x4cccdc`）。
⇒ `thiscall(GlobalsInner*; int amount) ret 4`，返回 0/1。`sdk.h` 的 `ce_spend_power` 照此。

**V2 证据**：`spend_power` 在 `CURRENT_POWER <= 一档` 时拒绝不扣（`0x45748c..0x457494`），扣完 `< 一档` 钳回一档（`0x45749f..0x4574a6`）：
**永远保留 1.00**。零售 Tsukasa 成本 1 档、门槛 2 档（`0x410e6d`：`eax = [0x4ccd40]*2; cmp [0x4ccd38], eax; jl → 0x476be0(0x10)`）——
「成本 + 一档」是零售自己的规矩；我们成本 200、门槛 300。只要求 ≥ 200 的话 250 时实际只扣 150。

**V5 证据**：`Rng__rand_dword` `0x402740`：`mov esi, ecx`（this = zRng*）、无栈参、裸 `ret`、`eax = (hi16 << 16) | lo16`（两步 16 位生成器，`rng+4 += 2`）。
`0x4cf288` = ExpHP `REPLAY_SAFE_RNG`，xref 全是 gameplay：`Bullet__run_ex`、`Enemy__drop_items_and_notify_cards`、`PlayerBullet__create`、
`CardSuwako` / `CardYachie` / `CardMiko`、`BombSanae`。`0x4cf280` 的 xref 是 `CardShop__pick_weighted_random_offer`、`ItemManager`、UI（非回放流）。
⇒ gameplay 随机走 `0x4cf288`；`sdk.h` 的 `ce_rand` 就是 `thiscall(0x4cf288)`。

**V17 证据**：ECL 517 `setScreenShake(time, start, end)` 的 case（派发 `0x430dd4` 跳转表第 116 项 → `0x434d8d`）：
`push 0x5b; push 0; get_int_arg(2) → push; get_int_arg(1) → push; get_int_arg(0) → edx; mov ecx, 1; call 0x476060`。
`0x476060`：`new(0x40)`、`memset`、`ScreenEffect__operator_new(this; type, time, start, end, p5, prio)` `0x4760b0`，尾 `0x4760a8` **`ret 0x10`**（四个栈参）。
type 1 → 只注册 on_tick `0x475c70`（`register__on_tick(…, 0x14)`），不注册 on_draw（所以 `0x5b` 用不到）；写 `+0x18 = time`、`+0x1c = start`、`+0x20 = end`。
`0x475c70` 每帧：强度 = start + (end − start) × 已过 / time，`Rng__rand_dword(0x4cf280) % 3` 决定 x / y 各取 {0, +I, −I}，写 `SUPERVISOR.camera_1/camera_3` 的偏移；
到时返回 7（UpdateFunc 自删）。零售同一工厂的其他用法：Sanae 炸弹 `FUN_00476060(8, 4, 1, 0x3c, 10, 0x5b)`（type 8 是别的效果）。
`sdk.h` 的 `ce_screen_shake(time, start, end)` = `fastcall(1, time; start, end, 0, 0x5b)`。

**V3 证据**：`0x45d5e0` 入口 `mov [esp+0x18], ecx`（this），全程读 `ecx+0x471dc` / `ecx+0x50`… 即 `PLAYER+0x620+…`；尾 `0x45dd0b` **`ret 4`**。
调用点 Tsukasa `0x410f27`：`push ecx; mov ecx, [0x4cf410]; lea ecx, [ecx+0x620]; call 0x45d5e0`，调用后不动 esp ——
压的那个 `ecx` 就是被 `ret 4` 清掉的哑栈参（函数体不读 `[ebp+8]`）。⇒ `thiscall(PLAYER+0x620; 一个未读栈参) ret 4`。
`sdk.h` 的 `ce_repopulate_options` 传 `p + CE_PLAYER_INNER` 与一个 0。

**实跑要看的**：见 [`NEXT.md`](NEXT.md) §1i。崩溃优先怀疑 V1 / V3（两个 `ret 4` 抄错就是栈飞）、V5（`ce_rand` 的 thiscall）；
视觉怀疑 V9（火球方向 = `vm+0x44`，贴图朝 +x）。

## W. 倒吊人（id 74）+ 黄金小丑（id 73）—— 局中删卡原语 `ce_destroy_card` + 过关结算事件 `on_shop_open`

**没有手写机器码、没有新断点、不改任何字节**。删卡是 recount `0x4080e0` 删卡分支的单张复刻 + 引擎自己的 HUD 重画 `0x408d00`；
`on_shop_open` 挂在 §P 已有的 `ce_shop_reopen` 断点的 MSG 开店分支上。零售先例：商店空白卡在店里 `reset_cards(mgr, 0)` 整副删卡（`0x41842a`，05 §7）。
纯逻辑 `ce_interest` / `ce_pick_index` 主机单测（`test_sdk_core.c`）。

| # | claim | 结论 |
| --- | --- | --- |
| W1 | 链表结点布局与摘链写法 | **CONFIRMED** —— 见下「W1 证据」：`prev` 永不为 NULL，表头无尾指针缓存 |
| W2 | 计数就地减 = recount 重数 | **CONFIRMED** —— 见下「W2 证据」 |
| W3 | 删对象的调用约定 | **CONFIRMED** —— `0x408147` `push 1; call [eax+0x50]`（ecx = 卡）= `thiscall(this; 1)`，`ce_base_del_t` 同签名（O5 的 opdelete 桩照它写）。我们的卡走桩（先 `ce_state_free`），零售卡走自己的 `operator_delete` |
| W4 | HUD 重画 | **CONFIRMED** —— 见下「W4 证据」：店里调有先例、只看三个类别计数、先减计数再调顺序正确 |
| W5 | 选中的主动卡不能删 | **CONFIRMED（设计）** —— 见下「W5 证据」 |
| W6 | 倒吊人 ctor 时链表里没有自己、也没人在遍历 | **CONFIRMED** —— 见下「W6 证据」 |
| W7 | 删装备卡：子机怎么办 | **CONFIRMED（先例）** —— 零售空白卡在店里 `reset_cards(mgr, 0)` 同样把装备卡连子机留到关店，关店后 repopulate 广播重建（SDK §12）。破损核心的子机指针在 `ce_state` 里随桩释放，广播时重新认领 |
| W8 | 删存档带来的卡（bit0 = 1）只影响本局 | **CONFIRMED** —— 下一局 `reset_cards(reload)` 从存档重建（02 §6）；存档的卡组槽只在编成菜单写。与 Balatro 的 run 内摧毁语义一致 |
| W9 | `owned[id] := 0` 的后果 | **设计** —— 商店第一轮筛 `owned == 0`，清零后它能再刷出（Balatro 里被摧毁的小丑也会再出现）；`ce_fresh_acquire` 把再买视作新获得（腐化再补满）——预期。解锁位不动 |
| W10 | replay | **CONFIRMED（推理）** —— 随机只来自 replay 安全流 `0x4cf288`（V5），抽取在成交那一帧；商店进出 `0x417880` 把当时的卡组写进 replay 分段，删后的卡组被记下。利息只依赖 `MONEY` |
| W11 | 倒吊人永远走不到「火力补差价」 | **CONFIRMED** —— ctor 在扣款前 `MONEY += 300`，价位档 3（≤ 180）< 300 ⇒ `price <= MONEY` 恒成立。没有可删的卡时不给钱，此时才可能走火力补差价（与零售任何卡相同）|
| W12 | `on_shop_open` 的时点与重开 | **CONFIRMED** —— `0x443b05` `test eax, 0x20000` 是 GameThread 建店（`0x443b10..`）前的判断，`flag_set` 只在 MSG opcode 36 那一帧为真（§P）；重开分支不调。练习 / replay 里 MSG 照样置位 ⇒ 利息照样加（录制时也加了，回放一致）|
| W13 | 溢出 | **CONFIRMED** —— `ce_interest` 用 `int64_t` 乘再除；`MONEY` 引擎没有上限钳（方片 2 翻倍同样不钳）。HUD 金钱位数 🟡 未查 |
| W14 | trace 槽表 | **修正** —— `CE_TRACE_SLOTS` 23 → 26：此前 `0x5c` / `0x60` 的 trace 因 `slot/4 ≥ 23` 被静默丢弃，现在连同 `0x64` 都能记 |

**W1 证据**：结点 = `card+0xc` `{+0 card*, +4 next, +8 prev}`。recount `0x40811b..0x408145`：`next->prev = prev`（`+8`）、`prev->next = next`（`+4`）、
自身两指针清零。插入 `0x412e90` 永远追加到尾、新结点 `prev` = 末结点（空表时 = 表头 `mgr+0x18`，其 `+4` 正是 `mgr+0x1c`）——**`prev` 永不为 NULL**，
所以「删第一张」也会把表头的首指针改对。表头第四字段 `mgr+0x24` 由 `reset_cards` `0x407da0` 置 0，插入只在它非 0 时维护 ⇒ 没有需要修的尾指针缓存。

**W2 证据**：recount `0x408158..0x40816d`：`flags & 8` → `+0x2c`、`& 0x40` → `+0x30`、否则 `+0x34`；`0x40814c` `dec [edi+0x28]`。
分配尾段 `0x412d5x` 按同一分法加。三计数只被这两处与 `reset_cards` 写，就地减一与整体重数结果相同。

**W4 证据**：`0x408d00` `thiscall(mgr; int)` `ret 4`（`0x408dd2`）：先按 `mgr+0x58` 的 256 个 id 删旧图标 VM（`0x408d20..0x408d73`，跳过 `+0x538 & 0x10000000` 的），
再在 `GAME_THREAD_PTR != 0` 时按**三个类别计数**（`0x408d93` `push [ebx+0x2c]` / `0x408d9f` `+0x30` / `0x408db7` `+0x34`）画三行。
分配尾段 `mode != 1` 时调 `(this, 0)`（商店购买 = mode 2 就在店里调它）⇒ 店里调有先例。**它不看 `num_total`**，且尾段是先调它再 `num_total++`——我们先减计数再调，顺序正确。

**W5 证据**：`mgr+0x38` 在分配尾段被置成最新的主动卡（`0x412d5x`），C 键分派 `0x45c090` 读它；删掉它就是悬指针。`ce_destroy_card` 比对后拒绝；
`ce_pick_random_card` 一开始就不把它列入候选。非选中的主动卡可删（只减 `+0x2c`，`+0x38` 不动）。recount 那套「从尾往前重选 + 重建图标 VM」（`0x408201..0x408295`）刻意不做。

**W6 证据**：分配尾段先调 ctor（`0x412d0d`）、非 0 返回就 `operator_delete` 早返回，`FUN_00412e90` 入链在其后；`ce_pick_random_card(c)` 再显式排除自己。
调用链 `AbilityShop__on_tick` 成交分支 `0x4185c7` → `allocate_new_card` → ctor，不在任何链表遍历里；经强欲之壶 `ce_give_card` 进来同理（壶的 ctor 也在这条链上）。
`on_tick_2` 等遍历桩里**不能**调 `ce_destroy_card`（头文件注释写明）。

**实跑要看的**：见 [`NEXT.md`](NEXT.md) §1j。崩溃优先怀疑 W3（`+0x50` 的 thiscall）、W4（`0x408d00` 的 `ret 4`：店里 HUD 重画后立刻崩就是它）；
视觉怀疑 W4（删后 HUD 行没重排 / 幽灵图标 = `mgr+0x58` 删 VM 那一步没生效）。

## X. 真银圣剑（id 75）—— 炸弹「挥砍」+ 残机碎片 + HUD 图标上叠数字

**没有手写机器码、没有新断点、不改任何字节**。三个新引擎入口都是普通函数指针调用，签名从反汇编出口与零售调用点取；
伤害源 / 抬上限 / 删 VM 全是炎魔用过的包装。`ability.anm` 多 10 个 32×32 entry + 一个脚本。纯逻辑 `truesilver_core.c` 主机单测。

| # | claim | 结论 |
| --- | --- | --- |
| X1 | `AnmLoaded__instantiate_vm_to_ui_list_back` `0x409670` 的调用约定 | **CONFIRMED** —— 见下「X1 证据」：thiscall + 四个栈参、第四个不读、`ret 0x10` |
| X2 | `AnmLoaded__set_sprite` `0x477b00` | **CONFIRMED** —— `anm/th18/01-vm-instantiate.md` §0：thiscall(anm; vm, idx) 两个出口都 `ret 8`；HUD 图标 `0x408fxx` 就这么调（`set_sprite(abcard, child, entry+0x30)`）。我们传 ability.anm 与它自己的 sprite 号 |
| X3 | 加碎片 `0x457570` | **CONFIRMED** —— 见下「X3 证据」：thiscall(GlobalsInner; 哑参) `ret 4`，两个出口一致；语义 = 零售碎片道具 |
| X4 | 挥砍伤害与上限 | **CONFIRMED（推理）** —— 4 帧 × 100 全场矩形源，同帧还有炸弹自己的源与自机弹，三者合计钳 `player+0x47984`；抬到 400 = 炎魔的 300 再留 100 给炸弹。炸弹单帧源 > 300 的角色（🟡 未逐个量）会挤掉一部分，实跑看 boss 血条 |
| X5 | 数字与图标的相对位置 | **设计（待实跑）** —— 见下「X5 证据」：实体坐标抄图标根 VM `+0x5f0`，脚本 pos = 根脚本 + (24, 24) |
| X6 | 数字 VM 不残留 | **CONFIRMED（代码）** —— 每帧 `digit_sync`：图标 `get_vm_with_id` 为 0 → 删数字；`on_hud_anm` 再来（HUD 重建）→ 先删旧再起新；`on_run_reset` 删；数字 VM 自己死了（UI 列表被引擎整清）→ 重起。`ce_anm_delete` 走 `0x488cf0`（标记删除并递归子树），与青眼血条同款 |
| X7 | `on_hud_anm` 槽的时序 | **CONFIRMED** —— `0x408fc4` 在图标根 VM 与两个子 VM 都建好、`set_sprite` 之后调 `vtable+0x44(id)`；我们在里面调 `0x409670` 起自己的 VM = 引擎同一帧同一函数里刚做过的事（它内联了同一段）。不在任何链表遍历里改链表 |
| X8 | `on_bomb_spent` 里调加碎片 | **CONFIRMED** —— 断点在 `do_bomb` 内（§T），Player tick 里；`0x457570` 的零售调用方 `ItemManager__on_tick__body` 也是普通 tick 上下文，内部只改 GlobalsInner 与刷 HUD。与腐化同一分派链，顺序无关 |
| X9 | 耐久不跨关、进店不清 | **设计** —— `on_stage_start`（`+0x34`，每关开场）回满；ctor 每关开场也会再调一次，幂等；商店里买到 → ctor 给 2，下一关开场再回满。爆发中过关清掉（单测覆盖）|
| X11 | category 1 → 装备 flags | **CONFIRMED** —— 见下「X11 证据」：零售装备 case 写 `& ~0x0a \| 0x44`，绑定断点在 case 之后、`bit0 := mode & 1` 之前，改后 bit0 仍由原指令写 |
| X10 | 决死炸 / 黄昏接的第二发 | **设计** —— 都经 `do_bomb` 扣数 ⇒ 都算挥砍。黄昏 + 真银：最后一颗接出来的那发也吃耐久 |

**X5 证据**：图标根脚本 abcard script5 `pos(1056, 537)`，子脚本 6–11 `pos(0,0)`（子随父）；引擎把每张图标的排版坐标写进根 VM 实体坐标 `+0x5f0`
（`0x408de0`：`pfVar6[0x17c..0x17e] = local_2c/local_10`，x 按 80 px 步进、行 y 0 / 130 / 260）。我们的 VM 实体坐标抄它、脚本 `pos(1080, 561)` = 根脚本 + (24, 24)。
若实跑发现 `+0x5f0` 不是排版坐标（脚本 `pos` 覆盖了它），改抄脚本 pos 字段即可——日志已打三个分量。

**X11 证据**：`allocate_new_card` 的 57 路 case 里 flags 四种写法：资源 / 即时 `& ~0x4c | 2`、被动（含 case 56）`& ~0x4a | 4`、装备 `& ~0x0a | 0x44`（REIMU_OP…NUE_OP 14 个）、主动 `& ~0x46 | 8`。
公共尾段 `0x412cec`（绑定断点）之后才 `flags = flags & ~1 | (mode & 1)`（`0x412cfe`），所以绑定时改 bit2 / bit6 不会碰 bit0。bit6 的读者只有 HUD 分行 `0x408de0`（`param_3 == 1` 行取 `flags & 0x40`）
与计数 `0x412d6c` / recount；`on_power_level_change` 是全卡广播，不看它（破损核心此前没有 bit6 也能申请子机）。`ce_destroy_card` 按同一 flags 减计数，自洽。

**X1 证据**：`0x409670` 入口 `mov edi, ecx`（this = AnmLoaded）；`push [ebp+0xc]`（script）→ `0x407420` 拷脚本；`[ebp+0x10]` 非 0 时 `movq xmm0, [eax]; movq [esi+0x5f0], xmm0; mov eax, [eax+8]; mov [esi+0x5f8], eax`（pos 三分量）否则清零；
`[ebp+8]` = out_id（`mov [edi], 0` 后 `mov [edi], ecx` 写回 id）；`[ebp+0x14]` 全程不读；唯一出口 `0x409746` **`ret 0x10`**。⇒ thiscall(anm; int* out_id, int script, float* pos, int unused) ret `0x10`。
`sdk.h` 的 `ce_anm_spawn_ui` 压四个参。层不是参数（世界版 `0x405bf0` 写 `vm+0x18`，这里没有）—— 靠脚本里的 `layer(26)`。

**X3 证据**：`0x457570`：`mov esi, ecx`（this = GlobalsInner `0x4cccdc`）；`CURRENT_LIVES(+0x6c) >= LIVES_MAX(+0x78)` → `+0x70 = 0; ret 4`；否则 `+0x70`（LIFE_FRAGMENTS）+1，
按难度表（`0x4b40c8` / extra `0x4b3ff4`，下标 `+0x74`）取每命碎片数，`while (frag >= n) { frag -= n; 0x4575f0(加整命) }`，最后 `0x441f10(GUI; lives, frag, max)` 刷 HUD；出口 `0x4575e2` **`ret 4`**。
调用方 `CardLifeFragment__constructor` `0x409cc0`（先 `LIVES_MAX` +1 钳 7 再调它——那是碎片**卡**的额外效果，我们不要）、`CardNarumi` 每关、道具 type `0x10`。⇒ thiscall(GlobalsInner; 哑参) ret 4，`ce_add_life_fragment` 与 `ce_add_bomb` 同形。

**实跑要看的**：见 [`NEXT.md`](NEXT.md) §1k。崩溃优先怀疑 X1 / X3 的哑参、X2 的 thiscall；视觉看 X5。

## Y. 古锭刀（id 76）—— 终符期间自机弹伤害 ×2

**没有手写机器码、没有新断点、不调任何新引擎函数、不改任何字节**。读的都是已在用的结构（神之宣告的 boss / 中断槽链、K♠ 的 `bullet+0x9c`、
炎魔的 `ce_raise_damage_cap`），新读一个 `sht+0x28`。纯逻辑 `guding_core.c` 主机单测（26 例：活动槽选择、`Dead` 匹配含 `0x40` 满长无终止符、状态沿、倍率）。

| # | claim | 结论 |
| --- | --- | --- |
| Y1 | 「终符」= 活动中断槽子程序名含 `Dead` | **CONFIRMED（ECL 一手，全覆盖）** —— 见下「Y1 证据」：七关 boss + Extra 道中最后一张符卡的槽名全是 `BossDead` / `MBossDead`，其余 `Boss2`…`Boss10`；主线道中无符卡 |
| Y2 | 活动槽 = 第一个 `hp_value > -1 && time > 0` | **CONFIRMED** —— 与引擎超时判定同一条规则（`0x42edf5`，§O28）；神之宣告 `expire_current_attack` 已实跑用它。Extra 道中槽 1 常驻 `MBossDead`（整体计时）但槽 0 在前，Card1 / Card2 期间取到的是 `MBoss2` / `MBoss3` ⇒ 不误判（单测覆盖） |
| Y3 | 4 参 `setInterrupt` 把名字写进 `sub_life` 与 `sub_timeout` 哪一个 | **推理（领域）** —— 击破走 `sub_life`（`+8`）、超时走 `sub_timeout`（`+0x48`）（§2 ③ / ①），而终符无论击破还是超时都进 `BossDead` ⇒ 两个都是它。代码两个都查、任一含 `Dead` 即算（`gd_slot_is_final`），不押注哪个 |
| Y4 | 为什么不用 `stars`（ECL 540） | **CONFIRMED 反例** —— st07mbs `MBoss2` / `MBoss3` 都写 `stars(0)`，Card2 会被当终符；`MBoss1` 根本不写。主线 boss 倒是严格（`stars(N)` = 本符之后还剩几张） |
| Y5 | 翻倍两处一起 | **CONFIRMED（V20 同一事实）** —— 自机弹与卡的源共用每敌每帧上限，贴脸时已吃满，只翻弹不抬等于白翻。抬到 **sht 基值** ×2 而非当前值 ×2，见下「Y5 证据」 |
| Y6 | 时序 | **CONFIRMED** —— GameThread（`0x10`）复位上限 → AbilityManager tick（`0x16`）`on_tick_2` 判定 + 抬 → Player tick（`0x17`）建弹调 `+0x28` 读同帧判定 → Enemy（`0x1b`）结算。`on_bullet_created` 只读私有状态，不再走链表 |
| Y7 | 边界 | **设计** —— 符卡超时：引擎把槽 `hp_value` 写 -1（§2 ①）⇒ 活动槽消失 ⇒ 判定同帧落下，bit0 还挂着的窗口不翻。神之宣告推超时同理。过关 `on_stage_start` 清 `active`；局末 `on_run_reset` 清统计。`sht` 指针空（进关前）→ 上限不抬、弹也没有 |
| Y8 | 炸弹 / 其他卡不翻 | **设计（用户定）** —— 只改 `bullet+0x9c`；炸弹与 `ce_damage_rect` 的源不经 `+0x28` 槽。抬上限对它们只是「更松」，不是「翻倍」 |

**Y1 证据**：`thecl -d 18 -m th18.eclm` 反出 `st01–07bs.ecl` / `st01–07mbs.ecl`，每张符卡前的 `setInterrupt(0, 0, t, …)` 按文件顺序：
st01 `Boss2 BossDead`、st02 / st03 / st04 `Boss2 Boss3 BossDead`、st05 `Boss2 Boss3 Boss4 BossDead`、st06 `Boss2…Boss6 BossDead`、
st07 `Boss2…Boss10 BossDead`（Card10 终符，Card9 耐久符去 `Boss10`）、st07mbs `MBoss2 MBoss3 MBossDead`。
主线道中（st01–06mbs）无 `spell` / `spellEx`，只有 `setInterrupt(1, 0, t, "MBossDead")` 计时非符——符卡位不置，不触发。
表格在 [`engine/ecl/th18/01`](../../../engine/ecl/th18/01-boss-interrupts-and-spellcard.md) §5。

**Y5 证据**：`0x443d32` `mov eax,[ecx+0x47940]; mov eax,[eax+0x28]; mov [ecx+0x47984],eax`（GameThread 每帧复位，一手反汇编）⇒ `player+0x47940` 是 sht 指针、
`sht+0x28` 是 `max_dmg`（`engine/sht/th18/01` §5 的数值表）。取基值 ×2 而不是当前值 ×2：同帧炎魔 / 青眼可能已把它抬到 300，再翻会变 600。`ce_raise_damage_cap` 只抬不降。

**实跑要看的**：见 [`NEXT.md`](NEXT.md) §1l。崩溃优先怀疑：`find_enemy` 链表（与神之宣告同一段代码，已实跑）；视觉：终符血条掉速对比前一张符卡。

## Z. 辉针城（id 114，原 254）—— 收点批结算 + 非聚焦命中掉 P（collect-2x）

四个新 SDK 断点（全放行原指令、无手写机器码）+ 四个 hooks 尾槽 + 纯逻辑 core；批次状态机与掉 P 节流策略
（2026-09-08 由掉钱改 P，Z13）在 `collect2x_core`（主机单测），卡文件只做接线。弹字呈现走原生倍率环（Task 8）。
**本段 = Task 4 断点先行的静态断言；卡行为断言与实跑结论在 Task 9 追加。🟡 = 未实跑。**
**辉针城整卡实跑通过（2026-09-09 整机回归，Z22 成员门「有就起效、没有不起效」闭环）—— 见 Z22 证据尾注。**

| # | claim | 结论 |
| --- | --- | --- |
| Z1 | 四窗字节与 sites.py 表一致、窗内无跳转目标 | **CONFIRMED（静态）** —— expected hex 逐窗对照原始 exe 相等（`emit_sdk_breakpoints` 每跑必验，gen 已过）；全图 33445 条分支目标无一落在窗内（起点命中安全：断点覆盖后跳向窗口首字节 = 跳向补丁指令） |
| Z2 | 每窗是单条 / 两条完整指令、无相对寻址 → thcrap 可整窗搬进 cave | **CONFIRMED（静态）** —— objdump 对齐反汇编逐窗验证（各窗指令见下证据）；expected hex 与原始 exe 逐字节相等由 gen 每跑必验 |
| Z3 | `BP_ce_item_collect` 读窗前值：type 由窗内指令装载，handler 读内存 `[edi+0xc78]` 而非 EDX | **CONFIRMED（静态）** —— 断点在原指令**前**触发（thcrap 惯例，§E）；代码照此实现 |
| Z4 | `ce_item_frame` 每个正常游玩帧、道具池 body 前到达一次 | 🟡 **未实跑** —— `0x446ec0` 门卫链：`GAME_THREAD` 状态与 `0x400`（商店）前段已排除，`[0x4cf2e4]` 空或位测试失败即跳过 body（跳转细节见下证据）。trace 首命中 + 帧计数（Task 9）验「每帧恰一次」 |
| Z5 | `ce_item_commit` = 各收集路线共同收尾起点（分值已入 SCORE） | 🟡 **未实跑（覆盖风险点）** —— 反汇编看线/半径两路都汇聚 `0x44658a`；**若实跑发现某路线直通 case 不经此点，批入账点补在 commit BP**（见 plan §分数补差） |
| Z6 | `BP_ce_shot_damage` 只触发「非聚焦、非炸弹的射击命中」（dmg 钳后 ≥1、pos = 敌世界坐标）| 🟡 **未实跑** —— handler 门 = `esi==0` / `param_6≠0` / 聚焦位 / bomb 任一 → 放行不触发；窗前已判 param_6 与 esi，守卫失败路跳不到此窗（纵深防御，见下证据）。实跑看：聚焦 / 炸弹不掉钱、非聚焦命中按间隔掉 |
| Z7 | `ce_item_pool_any_flying` 谓词 = 批未闭谓词（type∈{1,2,9–0xe} 且 state∈{1,3,4}） | 🟡 **未实跑** —— state 机为 2026-09-08 spawn/body 一手（`0x446fac` 置 1、`0x445f6f` 置 3、`0x446647` 置 4）；「state 5 延迟段不入谓词」是否漏批（收点弹字已出但延迟段道具后补）由 Task 9 批次对照实跑定案 |
| Z8 | hooks 尾槽 designated-init 增量：既有卡不受影响 | **CONFIRMED（静态）** —— `ce_hooks_t` 尾部追加、`CE_CARD` 的 `__VA_ARGS__` 是 designated init（C99 保证未列字段 = 0/NULL）；全量主机套件与全部既有卡编译通过 |
| Z9 | 批开窗口外的帧不发全池扫描（`0x1258` 槽 × `0xc94` ≈ 4 MB 跨度） | **CONFIRMED（静态）** —— 扫描函数只在 Task 6 的批开分支调用（文档写明）；函数自身有类型/状态过滤早停 |
| Z10 | 倍率弹字 = 纯视觉写：helper 只写 PopupManager 环槽（busy 最后置，字段集 = adder `0x4645f0` 同款 + 值/倍率）；原生 tick `0x464020`/draw `0x464180` 自驱位移动画、α 淡出与 busy 回收 | **CONFIRMED（静态）** —— 反汇编两函数只读写环槽字段与文本对象 `[0x4cf2ac]`（draw 抄槽色进 +`0x19228`）；不触 SCORE/道具池/卡状态；值 = 已入账结算分副本（分数由 ce_add_score_delta 独立管）；零 RNG 消费、不进 replay；画面 Task 9 双跑复核 |

**Z4 证据**：`0x446ec0` 对齐反汇编（objdump PE 直解）：
`mov edx,[0x4cf2e4] / test edx,edx / je 0x446ee3`（mgr 空直落门卫）、`mov edx,[edx+0xb0]` 后
位测试 `jne 0x446eec`（跳过门卫提前 `mov eax,1; ret`）、`0x446ee3: cmp dword [0x4cf2a4],0 / je 0x446ef2`（进 body）。
`0x446eec` 是提前返回桩、`0x446ef2` 是 body 继续 —— `0x446ee3` 恰好是「最后一次门卫、决定 body 是否跑」的位次。

**Z7 证据**：state 机写点一手：出生 `0x446fac movl $1,0xc74(esi)`（state=1）、磁力吸附 `0x445f6f` 置 3 /
`0x446647` 置 4、分派瞬态 2（`0x445b0f`/`0x44625f` 路）、收尾清 0 `0x446595`；type 于 spawn `0x447051` 写入 `+0xc78`。
磁力判定对 type 9–0xe 除外（`0x4465da`+ 段）——所以小分自己不会飞向自机（只能回收线收），批未闭谓词仍以 state 1（在飞）兜底等它落地。

| Z11 | 卡组编成 deck 槽能装任意 ≤`0x7f` 的卡 id；>`0x7f` 被当负值剔除 | 🟡 **实跑现象 + 一手半程（2026-09-08）** —— 见下「Z11 证据」；处置：本卡 254 → 127（降序天花板 127，后卡 126…），复跑验 |

**Z11 证据（2026-09-08，用户实跑 + objdump）**：编成 deck [2 张老卡] 时把辉针城（254 = `0xfe`）手动装进第 1 格 →
该格**当场消失、后槽前移、可带数 −1**（现象）；老卡 id 58–76（≤`0x7f`）全部正常，254 是第一个越界 id。
deck 槽 = save+`0x5f608`（字节槽，reset_cards `0x407ee3 movzbl` 读）；写槽路径 `0x41511c mov %cl,0x5f608(%edx,%eax,1)`
只存不验（cl 全 0..255），空槽哨兵 = id 56（`0x415070 cmpl $0x38,4(%eax)` 语境为「槽里是空卡才放行」）——
**墙不在写槽点本身**，在槽显示/重建/存盘链的某处符号扩展（≥`0x80` → 负 → 剔除压缩；未钉到指令）。
处置 = 不押注墙位，新卡 id 上限降至 127（`0x7f`，符号界内最大值），降序分配后卡 126、125…。
**若 127 复跑仍被剔** → 墙比符号界更怪，回钉 `0x415af0` 重建链（AUDIT §G 挂账）。

**Z10 证据（2026-09-08 一手 objdump）**：rate 槽生命周期 = tick `0x464020` 第二循环（`0x4640e0 lea 0x9f4(%edi)`，ecx = slot+`0x28`，busy@`0x14`）：age 字段 +`0x24` 每帧 +1（`0x46413d`）、>`0x3c`（60 帧）起 color@`-0xc` = slot+`0x1c` 的 alpha 字节 −4（`0x464145`–`0x464165`），≤0 清 busy（`0x464154`）——**原生自动回收**。draw `0x464180` 第二循环（esi = slot+`0x1c`）每槽两连 `0x419cb0`（this = `[0x4cf2ac]`，显式压栈）：格式串 `0x4b71f8` = `"BONUS %.1f"`（mult float@`0x28`→slot+`0x44`，cvtps2pd）、`0x4b61a4` = `"%d"`（值 int@`0x24`→slot+`0x40`）；值 <0 分支走 `0x4b7204` `"NO BONUS"`。槽色先抄进文本对象 +`0x19228`（`0x4644c2`）。adder `0x4645f0` 的字段写序 = 本 helper 的复位模板（+`0x20`=-1 / +`0x24`=0 / +`0x28`=0 / scale=1.0 / bit0 动画标志；★原生回收只清 busy，不复零的话 tick 看到残留 age>60 当帧淡出、位移 float 残留会瞬移）。

| Z12 | 碎片奖励 = TH18 原生卡道具 type `0x10`/`0x11`（非 4/6 宝石残留）| 🟡 **一手 2026-09-08（表 dump + 分派 + spawner + 半径门四路互证）+ 处置待实跑** —— 见下「Z12 证据」 |

**Z12 证据（2026-09-08，用户实跑纠偏「虹龙洞原生碎片就是卡」+ objdump/PE 一手）**：道具 type 4/6 残机碎/符卡碎**宝石**是旧引擎血统残留——虹龙洞自己的资源卡掉落 = 道具 type `0x10`–`0x13` 卡道具：
- 表 `0x4b4020`（stride 8）一手 dump：type `0x10`–`0x13` 行第一 dword = **card_id 3/4/5/6**（EXTEND2/BOMB2/PENDULUM/DANGO，05-shop §8 自洽）；type 1–0xe 第一 dword = 连续 anm 脚本 `0x70`–`0x7d`。
- 收集分派 `0x445A80` case `0x10`–`0x13` → `allocate_new_card(mgr, table[type].card_id, 0)`（mode 0 只调 ctor → ctor 型即时卡当场生效：id 3 CardLifeFragment / id 4 CardBombFragment）。
- spawner `0x446f40`：type ≥`0x14` → 0；1..`0x13` 收；type 0xf 在 `0x446ff7` 重映射为 6；type `0x10`–`0x13` 走 `0x447089` 特化卡 sprite 分支（非 `0x4470d4` 通用双 VM）；pos/angle/speed/timer 栈参两分支共用 → `ce_spawn_item` 直发 `0x10`/`0x11` 即可。
- **收集半径门**（弹点锚定的物理依据）：`0x45ae67` 起初始化 [player+`0x47988`]=5.0（磁速）、[+`0x4798c`]=`0x41f00000`=**30.0**（直接收集闸 `0x44624a`：dist²<30² 落 `0x44625f` 分派）、[+`0x47990`/94]=`0x428c0000`=**70.0**（swoosh 闸 `0x4465fb`/`0x44660e`，type 9–0xe 豁免 `0x44661d` 系）、[+`0x47998`]=128（`0x45ae8f`）/224（`0x40c275`）= 回收线（player.y ≤ 线 → 无条件真空 `0x445ffc` 系）。
- 处置：`C2X_FRAG_BOMB` 6→`0x11`、`C2X_FRAG_LIFE` 4→`0x10`；弹点 −100@2.2 → **−76@1.2**（70+6 余量内最短可见弧 ~1.4s；原弧顶 180px 被用户嫌「位置远」）。settle 日志 type 改 hex（新 DLL 格式记号）。观感与卡外观实跑复核。
- ★ 首轮复跑仍见 `frag type 0x6`（hex 记号已生效 = 新 collect2x.o、旧 core.o）：根因 = `native/Makefile` `%.o` 规则依赖表漏 `cards/*.h` —— core.h 常量改动不触发 `collect2x_core.o` 重编，旧 6/4 烘焙进 DLL。处置（2026-09-08）：依赖表追加 `$(wildcard cards/*.h)`，删两个 .o 强重建后核验 core.o 立即数 `0x11`/`0x10`、4/6 归零。教训：**改任何 cards/*.h 后核对 make 是否真的重编了 core .o**（或直接 `make clean` 级重建）。

| Z13 | 被动二掉落物：钱 type 2 → **P 火力道具 type 1**（设计变更）| ✅ **2026-09-09 整机回归实跑通过**（Z22 同场）—— 掉 P 成行：`collect2x: P drop (power …, t=…) @(敌身位)`、间隔随火力节流（Z15 落点沿用）；聚焦/炸弹负例门控仍待专项（Z6）。历史：2026-09-08 掉钱未现 → 用户拍板改 P，见下「Z13 证据」 |

**Z13 证据（2026-09-08 实跑）**：卡片碎片修复后（Z12）复跑，frag 卡正常；被动二钱掉落仍不出现——**换 P 是产品决策不是引擎结论**：P type 1 ∈ 计数批集 → 掉 P 被吃拉长收点批（梦A式，预期内）；P 满 4.00 引擎转分无碍。接线 = `spawn_item_at(1, …)`（同 2.2 初速）；core 侧仅改名（`c2x_drop_money`/`C2X_MONEY_INTERVAL_*` → `c2x_maybe_drop`/`C2X_DROP_INTERVAL_*`，语义不变）。若掉 P 复跑仍不见 → 回头查 `ce_shot_damage` `0x45f2a2` 断点链（聚焦位 / bomb / param_6 门控是否误杀高速段），AUDIT §G 挂账。

| Z14 | 碎片延迟不弹 + 掉 P 三连零出现的**共同根因 = 帧钟选错**（次因 pos 槽读错）| 🔧 **静态 CONFIRMED + 修复处置待实跑（2026-09-08 第二跑）** —— 见下「Z14 证据（第二跑）」 |
| Z15 | P 落点定案：`0x45f0f0` 的 arg1（`[ebx+8]` 槽）是**指针**——指向本帧被结算敌人（身体）的 {x,y}；handler 缺一层解引用（探针已删）| ✅ **实跑确认（2026-09-08 第三跑：「P预期正常」）** —— P 从被打中的敌人身上掉，见下「Z15 证据」 |
| Z16 | 收点完毕音：本卡全链路无音效代码、结算又是合成事件 → 结算帧播 `ce_play_sound(0x2e)`（se_cardget，商店买符卡同款）| ⚠️ 未实跑即被 Z17 一手替代（`0x2e` 商店语义不符，见「Z17 证据」）—— 处置已改 `0x39`，Z16 原处置作废 |

**Z14 证据（2026-09-08 第二跑，用户实跑：「碎片完全不生成了」且旧问题依旧 → 根因落定）**：碎片延迟一挂上
zTimer 就完全不弹、P 依然零出现，两个症状共同指向 `frame_now` 的时钟源 `card+0x20` —— 该钟对**被动卡是死的**：
- 引擎侧（01-object-model §3）：`+0x20` 每帧 `Timer__increment` 的一手证据全在**主动卡**路径（`0x40E8C0`
  `CardTenshi____on_tick_2` 等 12 张主动卡 c_press 模板：函数末尾无条件递增 +`0x20`、触发时清零）；本卡
  `0x54` 纯被动（无释放状态机）不在这条路径上。
- SDK 侧（sdk.c `ce_sdk_active_tick` 首行）：`if (!h->active_recharge) return;` —— 经过帧只有**主动卡**
  （`active_recharge` ≠ 0）才被帮手 tick 递增；被动卡从不碰 +`0x20`。
- 后果链：死值 0 → 结算登记 `pending_at = 0 + 20` 后每帧 `now(0) < 20` **永不弹**（碎片完全不生成，正是用户
  所见）；节流 `next_drop_at` 首枪过（`0 ≥ 0`，落点 pos 垃圾不可见）后 `96` 永远够不着（钱/P 三轮零可见）。
  **先前「掉钱不掉 = pos bug」只对了一半：节流钟根本没走。**
- 修复（collect2x.c）：帧源弃 zTimer → 接线层自计数 `s_play_frame`——`on_item_frame` 分派点 `0x446ee3`
  是 ItemManager tick `0x446ec0` 包装的门卫 CMP：包装先查 `[0x4cf2e4]+0xb0` 状态位与开店 `[0x4cf2a4]`，
  冻结帧连 CMP 都到不了（sites.py 711 一手 disasm；与 Z4 证据同款）→ **每次分派 = 一个正常游玩帧**；
  run reset 归零。碎片 pending 与掉 P 节流共用此钟（语义 = TH14 的「游玩帧」节奏）。P 落点本轮锚自机上方
  76px（碎片同锚实测可见；pos 槽真命中点由探针定位后换回、删探针）。日志 settle / spawn / P drop 行加 `t=%d`
  便于对照节奏。

**实跑要看的**：见 [`NEXT.md`](NEXT.md)（Task 9 追加 §行）。`sdk:` 启动日志应报四个新断点已应用（否则门 `FAIL`）；
trace 首次命中行应出现 `on_item_frame / on_item_collect / on_item_commit / on_shot_damage` 各一次。

**Z15 证据（2026-09-08 第三跑，用户：「1、掉p是从敌人掉；2、似乎没有collect2x-dbg：」）**：P 落点定案——
pos 参数槽（`[ebx+8]`）当年被当坐标直接读 = 把**槽地址**当 float 对（指针位型 ≈ 亚正规 ≈ 世界原点，正好解释
「P 掉在 (0,400) 附近/不可见」），真值是**槽里的指针**指向的 {x,y}：
- callee `0x45f0f0` 是 `push ebx; mov ebx,esp` 栈帧别名，`[ebx+8]` = arg1 参数槽。全量重读 disasm：
  1024 源槽扫描环 `0x45f1c0`–`0x45f282`；**重叠几何以 arg1 为固定点**：圆形分支 `0x45f2f3` `eax=[ebp-0x40]`（=arg1）
  减每源中心 `[edi-0x4]/[edi]`（rec+`0x1c`/`0x20`）取 dx/dy → 点对旋转源判重叠；矩形分支 `0x45f1fc` 系把 arg1
  与 arg2 两对坐标传 `0x404440`。arg1 全扫描期恒定 → 就是**本调用被结算那个身体**（调用方 = 敌人侧 step）。
- 调用方核对（`0x42ee75` 段：`push edi` 作 arg1 + 同函数 ebx+`0x5588`/`0x120` = 大型敌结构；`0x43955b` 段：
  arg1 = `lea [esp+0x38]` 调用方帧缓冲（敌人坐标每帧拷贝），arg2 = `lea [esp+0x2c]`、arg3 = ebx）——
  两类调用方 arg1 都指向敌人坐标对。award `0x45f2a2` 在循环后（esi = min(`[player+0x47984]`, 剩余预算
  [ebp-`0x3c`])，门 param_6==0 && esi>0），每调用一次。
- `[player+0x47984]` 语义复核（仅 3 处引用）：`0x443d3b` GameThread 每帧 `= [[player+0x47940]+0x28]`
  （当前射击模式的每帧伤害额度，**常量非累加器**）；`0x40f48e` = `create_damage_source_rect` 调用侧把额度
  置 `0x12c`=300（**不是**「Remilia 写 300」的独立写入）。→ award 是否命中门控**静态无法定案**（award 路径
  不读任何「本调用开火过」标志；早退只有池空 `0x45f14c` / `[0x4cf2b8]+0x30`==0 / getter ≤0）。观测点见
  NEXT：**停火（弹池未空、敌人存活）时敌人身上还掉不掉 P** —— 掉 = award 与命中无关，下一步在 `0x45f171`
  BP 捕获初始预算、ebp 键控做「本调用真开火」门。
- 修复（sdk.c）：pos = `*(float const *const *)(ebx+8)` + 可空防御；**探针（collect2x-dbg 一次性日志）删除**
  —— 问题已由静态定案（用户未见该行：与掉 P 同 handler，可能贴取被截断，不再追）。collect2x.c：`spawn_item_at`
  改从 `pos[0],pos[1]`（敌人中心，初速 2.2 向上小抛 = 原生 main-drop 实参，引擎道具重力带回 swoosh/自机行），
  P drop 日志打敌人坐标。
- 「收点音效」推测 = 引擎内置（用户点 3）—— **第四跑推翻（Z16）**：结算/碎片链上无任何原生时刻会响
  （本卡全链路原无音效代码），引擎不会自己播。

**实跑要看的**：见 [`NEXT.md`](NEXT.md)（Task 9 追加 §行）。`sdk:` 启动日志应报四个新断点已应用（否则门 `FAIL`）；
trace 首次命中行应出现 `on_item_frame / on_item_collect / on_item_commit / on_shot_damage` 各一次。

**Z16 证据（2026-09-08 第四跑，用户：「P预期正常。音效没有播放」）**：P 落点实跑通过（Z15 关闭）。
音效缺 = **结算帧无声**——run2 结算能跑但全程无声、run3 猜「引擎内置」（DLL 无音效资产）、run4 仍无声
→ 定案非内置。根因 = 结算是**合成事件**（无引擎原时刻）且全链路从未调 `ce_play_sound`。当时的处置 =
批闭播 `ce_play_sound(0x2e se_cardget)`（05-shop §288 商店买符卡同款）——**未实跑即被 Z17 一手推翻**
（`0x2e` 是商店语义；真源 = TH14 原生 `0x39` se_pin00），Z16 处置作废，代码已改。

| Z17 | 收点音**一手定源**：TH14 ≥20 五档弹字前（on_tick__body 尾 `0x438dfe` 系）一律 `push 0x39 → play_sound_at_position`；cfg `0x39` = `se_pin00.wav`，TH18 同 wav 同调校 → 弃 `0x2e`、直用 `CE_SE_PIN00` | 🔧 **修复处置待实跑（2026-09-08 第五跑：听 `0x39` 钟声）** —— 见下「Z17 证据」 |

**Z17 证据（2026-09-08，用户问「th14 or th15 收点结算的原生音效是啥」后一手分析）**：材料 =
`local/_scratch/th14_100b.exe`（前轮已拉，与 [th14] 目录零售 exe 同 md5 家族）+ th-re-data
th14.v1.00b funcs.json 命名。链：**播放点** → **id** → **cfg** → **名表** → **跨作对表**：

1. **播放点（一手反汇编）**：`ItemManager::on_tick__body` `0x438550` 的结算尾 `0x438dfe` 起：
   `[0x4f5884]` 批次倒计时归零 → 按 `[0x4f5880]` 收点计数分档（`0x3c`/`0x32`/`0x28`/`0x1e`/`0x14`），
   五档各自在弹字（`0x458000` 倍率加法器，色 `0xffffff00`/`0xe0ffff60`/`0xd0c0ff80`/`0xc0a0ffa0`/`0xb040ffff`）
   **之前**播声：`push 0x39; call 0x46e470`（SoundManager::play_sound_at_position，@ `0x438e09`/
   `0x438e60`/`0x438eb2`/`0x438f04`/`0x438f4e`；声像 xmm2 ← `[esp+0x28]` = 批位 x——438e03 起 movss）。
   5–19 = 灰字（`0x80a0a0a0`, value −1）**无音**；<5 全无。碎片 spawn（ItemManager::spawn_item
   `0x4397b0`，type `0x28`/0xc 系 @ `0x438fda`+）无声——「收点结算的音」只有这一声 `0x39`。
2. **id → wav（一手 cfg + 名表）**：cfg 行基 `0x4d61b8`、stride `0x14`、行构 {+0 id, +4 wav 下标,
   +8 低字音量(dB) 高字优先级, +0xc 循环, +`0x10` 游戏内标志}（play 函数 `movsx word [id×0x14+0x4d61c2]`
   即行 +8 低字对得上）。行 `0x39`（`0x4d662c`）：`39 00 00 00 | 2e 00 00 00 |
   38 ff 64 00 | …` → 下标 `0x2e` → 名表 `0x4d60b0`（65 个 char*：64 名 + NULL，与 cfg 首尾相接
   `0x4d61b4`）第 `0x2e` 项 = **`se_pin00.wav`**（非按字符串序数——序数第 46 是 pin01，以指针表为准）。
3. **跨作对表**：TH18 的 `0x39` 行（engine/_shared/th18-sound-table.md §7 一手）= `se_pin00.wav`，
   **−2dB / prio 100 / 非循环 / 游戏内**——与 TH14 行 `0x39` 调校逐项相同（TH14 行: `0xff38`=−200 即
   −2.00dB、prio `0x64`=100、+0xc=0、+`0x10`=1）。同 wav、同 id、同调校 → 收点结算音直用
   `CE_SE_PIN00 0x39`，无需扩表/注入。
4. **语义专一性**：th14 全 exe `push 0x39` 仅 8 处，其中播音 5 处全在收点结算；另 3 处
   （`0x47b78b` / `0x49f6c5` / `0x49fad4`）压 `0x39` 后跟对象构造（vtable 写 `0x47e470`/`0x47e3c0` 系）
   = 参数非播音 → **se_pin00 在 DDC 就是收点结算的钟声**。`0x2e` se_cardget（商店买符卡，
   05-shop-and-money §288）语义不符，Z16 处置作废。
5. **对齐核对**：原生 ×0（<20）无声 ←→ 本卡 mult==0 早 return 在其上 ✓；原生只有结算一声、
   碎片弹出无声 ←→ 本卡 spawn 帧无音 ✓（碎片收集时走原生 `0x25` se_item00 等自响）。
6. **处置**：collect2x.c 结算音 CE_SE_CARDGET(`0x2e`) → `CE_SE_PIN00(0x39)`；engine.h 增
   `CE_SE_PIN00 0x39`（注释引本条）。DLL 2026-09-08 19:35 重建已同步 mods/。
7. **TH15 三方交叉（2026-09-08 一手，材料 `local/_scratch/th15_vanilla.exe` = 零售 1.00b
   md5 同族）**：play 系两入口（`0x476360`/`0x476400`）prio 读点 `movsx word [0x4e46aa+id×0x14]`
   = 槽+0xa（槽基 **`0x4e46b4`**，stride `0x14`，行构与 TH14 同构）；槽 `0x39` @ `0x4e4b28`：
   `39 00 00 00 | 2e 00 00 00 | 38 ff 64 00 | …` = **id `0x39` / wav 下标 `0x2e` / −2dB(`0xff38`) /
   prio 100(`0x64`) / loop 0 / 游戏内 1** —— 与 TH14 行 `0x39`、TH18 行 `0x39` 逐项全同 → ZUN
   三作 SE 表按行直抄。用户听感参照的「魔改 TH15」d3d9 shell 亦以 `0x39` 为参调 TH15 播声
   （TH15BUT14-REPORT 🟡 `0x4408d0`），与 vanilla 行一致。
   🟡 **残留（TH15 名字腿）**：TH15 的 65 个 `se_*.wav` 串在 .rdata `0x4ce6d4` 起，但全 exe
   无任何指针/立即数引用（死串）→ TH15 运行时按序号解析（thbgm 句柄序），名字对不上一手，
   只能行级三作一致 + TH14 名表腿（pin00）收口。
   🟡 **残留**：th14/th18 两 thbgm.dat 的 wav 字节级 md5 未比对（本机 thtk 未编，
   tooling/thtk/build.sh 可补）——凭 同 id + 同文件名 + 同调校 三重一致 + 实跑听感收口。

| Z18 | 批闭门订正：**「池空即闭」→ TH14 原生 timer 语义**（最后一收后 8 游玩帧无新收才结）。池空门实跑三症状：结算音比 TH14 急 ~7 帧（池空 = 最后一收的下一帧即结）；NO BONUS 偶缺（急切把 8 帧窗口内的残余收点拆成 <5 微批静默关）；**躲弹幕时突然冒过期 ×0.5 弹字+钟声**（一枚残余点挂住批几十帧不落，离池才结） | 🔧 **timer 语义定案，结构被 Z20/Z21 承继**（idle 值 8→30→8+presence）；DLL 2026-09-08 19:54 首建已同步 mods/ —— 见下「Z18 证据」 |

| Z19 | 批集订正：**TH14「红点与蓝点」→ TH18 = 只计 P(1) 与钱(2)**（无蓝点，钱占其位）。原实现把 type 9–0xe 也收进批 —— 引擎击破转换（强制收集）专出的分数转换点，DDC 原义不入收点批 → 剔除：不入批、不计数、不触发 | ✅ **实跑确认**（run-7，用户「击破转换确实不触发 bonus 了」）；白名单 {1,2} 定案；单位小分量级（P=+10？钱=身价/10？）留 settle 日志对照 —— 见下「Z19 证据」 |
| Z20 | 批闭 idle 8→30 游玩帧（TH18 滴入物理适配，§Z18 结构不改）：TH14 原生 8 帧按 DDC 满火力收点线校准；TH18 **有 POC 线但过线只吸引、不批结算**，道具落进 70px 环后 ~5px/帧 滴入 → 波内吸收空洞 >8 帧常态 → 第六跑「一次收点被拆两次结算」（≥20+≥20、二次 settle 盖 pending）| 🔧 run-7 实跑：拆批不再现（30 兜住）但「体感慢了」（0.5 s 拖拍 = 副作用登记兑现）→ **已被 Z21 取代**（presence 续批 + idle 回 8）；16–20 兜底旋钮由 Z21 承继 —— 见下「Z20 证据」 |

| Z21 | 批闭 idle 30→8 + **presence 续批**（TH14 一手定案 Z20 遗留 #5：`0x43898e` 续期门在 item state-3 飞行 body、距离判定之前 —— 在飞批集道具每帧重载 timer → 批闭 = 收点流真停后 ~8 帧，与 DDC 同拍）。TH15/LoLK census：批记账块只写不读、timer 无消费 → 原生无批结算可对齐 | ✅ 已改、965 单测全过、DLL 22:04 重建同步 mods/；run-8 实跑 + 2026-09-09 回归通过：拆批/钟声未再报（presence 结构成立），run-8 新症状 = 无全屏事件也误弹灰 NO BONUS（Z21 证据残留 6 兑现：谓词 state∈{3,4} 过宽）→ **收窄落地 = Z22 成员门** —— 见下「Z21 证据」 |
| Z22 | **成员门 = 真空飞行体**（run-9）：批开/计数/续批只认**分发时 state==3** 的 {1,2} —— state 机普查：{1,2} 进 state 3 唯一入口 = V 门 `0x445b9a`；触收 1 / 环收 4 不入批；state-2 翻 3 只盖 9–0xe；引擎 `0x44699e`/`0x446c0a` 以 state==3 判「飞来」= 同谓词 | ✅ **整机回归实跑通过（2026-09-09）**：无 127 五局 collect2x 零日志；有 127 三笔 settle 全 ×2（263/103/82 items）。修复 = collect2x.c 成员门 + sdk.c presence 收窄 + engine.h 校正；965 单测全过 —— 见下「Z22 证据」尾注 |
`local/_scratch/th14_100b.exe` 全量 disasm（th14_full.asm）。原实现门 = `ce_item_pool_any_flying()`
池空才关批（collect2x.c），批计时来自 TH14 accessor 族：
1. **原生计时 = `[0x4f5884]`**：per-item 收点分支 `0x43898e` 系对 type≠9/0xa/0xb 重载 `edx=0x8`；
   accessor `0x439d60` = `mov [0x4f5884],0x8`（收点入批重载专用）；池循环帧尾 `0x438dad`
   `dec`、`0x438dc0` 归零 jne 落空即走结算（`0x438df1` 读 `[0x4f5880]` count 分档）→
   **批闭 = 最后一次收点后 ~7–8 游玩帧**。收点入批 = `0x439c90`（`inc [0x4f5880]` + 批分累加
   `[0x4f5878]`，与计时重载分开的两个 accessor）。
2. **原生不等池空**：计时归零无条件结算 —— 在飞残余点迟到 = 拆下一批单独计（<5 静默）。
   对照旧池空门：结算发生在「池内无批集道具」的下一帧 ≈ 最后一收 +1 帧 → 音急 ~7 帧；
   且任一残余点（掉出回收线才消失）可把批挂住数十帧 → 躲弹幕期突然冒出过期 ×0.5 + `0x39` 钟声
   （用户实跑：冒出那一刻完全没有收点动作）→ 两症状同根。
3. **NO BONUS 偶缺**：急切关批把本应并入同批（8 帧窗口内）的收点拆成 <5 微批 → 静默。
4. **处置**：idle 判据收入 core —— `c2x_collect_batch(+frame)` 记 `last_collect_at`；
   `c2x_maybe_settle(s, frame)` 批开且 `frame−last_collect_at ≥ C2X_BATCH_SETTLE_IDLE(8)` 才关
   （flying 参数删除——它编码的就是池空门）。接线层去掉 `ce_item_pool_any_flying()` 门。
   游玩帧 = on_item_frame 分派（§Z14 帧源），冻结/暂停帧不计，与原生池循环 tick 同口径。
   单测改 idle 语义 + 两新例（idle 窗口内收点并入同批；迟来残余拆新批静默关）。811 项全过。

**Z19 证据（2026-09-08，用户两连勘误后一手追 TH18 收点 type 语义）**：材料 =
`local/_scratch/th18_full.asm` + `th14_full.asm`。
1. **用户勘误链**：TH14 官方文本 =「一次性收点超过 20 个道具（**红点与蓝点总数**）即奖励一个碎片，
   只考虑红点和蓝点」→「TH18 没有蓝点，而是钱」→ 收点批集合必须是 **P 点与钱**，而非当时实现含
   转换点族的 {1,2,9–0xe}。原实现中 9–0xe 入批 → 用户实跑「击破自动小分收集（强制收集）触发 bonus」，
   与 DDC 行为不符（强制收集在 TH14 原生就不驱动收点批）。
2. **TH18 type 分派一手**（材料 th18_full.asm）：分派点 `0x44625f` 查映射表 `0x44685c`（19 bytes）
   再跳 `0x446830` 跳表（11 handlers）。type 1 → `0x4462cc`（P/power，经 `0x446870`）；type 2 →
   `0x4463ba` = collect_money_item `0x446b00`；**type 9–0xe 六型共指加分 handler `0x446547`**
   （`SCORE [0x4cccfc] += [0x4ccd34]/100`，cap 1e9）—— 只由击破转换（强制收集）产出，玩家正常收点
   吃不到；type `0x10`–`0x13` → `0x446531`（卡道具）。★ **尾段订正（写 Z19 当日再追一手推翻早先推断）**：
   P/钱例程**都**在分派窗内给 SCORE 加分 —— P（power 例程 `0x446870`）尾 `0x446ac0` 无条件
   `SCORE += edi/10`（cap 1e9−1；edi = `0x64`=100 → +10，或火力相关折算值，钳 ≥10）+ 前置弹窗
   `0x4645f0` 两次；钱（`0x446b00`）尾 `0x446d03` 无条件 `SCORE += esi/10`（esi = 道具身价，
   `0x446cf3 cmovle` 钳 ≥10）+ 弹窗 `0x446cfe`，并 inc `[0x4ccd20]/[0x4ccd30]/[0x4ccd34]` ——
   与 §O10（`0x446cf6` esi 身价同控弹窗与计分）互证。**P/钱道具各带小分** → stash delta = 该道具
   小分、批分 = 批内 Σ小分，非恒 0；「TH18 无 collect-score」推断作废（早先只看 handler stub 没读
   例程体，O10 已在案未回引）。
3. **TH14 侧收口**：计时重载门 `0x43898e` 只对 type≠9/0xa/0xb 生效 —— 强制收集系道具在原生
   语义里**不驱动收点批**（不拖计时、不被 20 连计数）；官方「红点与蓝点」与之一致。原 {1,2,9–0xe}
   集是臆测插值，违背这条一手。
4. **处置**：`type_in_batch` 收敛为 `type==1||type==2`（collect2x_core.c）。stash（collect BP 捕获
   SCORE 差）继续按分派窗差量入批 —— P/钱各带小分（上条尾段订正），批分 = Σ小分，TH14
   「batch-score × tier」的分数部分在 TH18 由此有真实来源；碎片与档位判定只吃 batch_count，不受
   影响。单测改写：批集白名单 {1,2}（碎片 4/6、卡 `0x10`–`0x13`、转换点 9/0xa/0xe 及其它 type、
   0/`0xffff` 全部不入不开批）、混收例（P+钱 = count 2；转换点贴着 P/钱被吃也不进批）。812 项全过。
   DLL 20:17 重建（128512 B，md5 aeba25920e61f924e620d04bdd92672d）已同步 mods/。
5. 🟡 **待验（run-7 部分确认，其余留第八跑）**：P/钱收点串如期触发碎片 + SE 时机正常（第八跑听
   `0x39` 钟声 = Z21 的 ~8 帧 latency）；**击破转换（转分点潮）不再触发 bonus = 实跑确认**（run-7，
   用户「哦对，击破转换确实不触发 bonus 了」）；结算弹窗值应 = 批内 Σ(P/钱小分)×档、**非零**
   （settle 日志 `batch score` 字段直接可读）——对照跑分确认每 P/钱的单位小分量级（P = +10？钱 =
   身价/10？）与弹窗/补差观感是否合理；若与 TH14 蓝点面值量级差距大，另行对齐。

**Z20 证据（2026-09-08 第六跑，用户：「批次判断错误，有时会连续触发两次收点」→ 澄清「不是同帧连续触发，
而是感觉上是一次收点的过程，被拆成两个结算」）**：场景（AskUserQuestion）= 攒了一波上线收点，同/下一波
小怪还在被打、道具持续生成；两次结算间隔 <0.5 s；两次弹字都完整、但基本只有一次生成碎片。
1. **症状与当时机制逐条吻合**：批闭 = 最后一收后 `C2X_BATCH_SETTLE_IDLE`（当时 8）游玩帧无新收（§Z18）。
   TH18 **没有 DDC 的「过线即全收」**（POC 线本身在：`player+0x47998`，Kanako 卡 22 还能抬到 224，但过线只是吸引）：道具落进自机旁 70px swoosh 环后以
   ~5px/帧 磁速滴入吸收点 → 一波道具的高度散布直接映射为到达时间散布，**波内天然有 8–25 帧吸收空洞**。
   攒的波被空洞腰斩：前半 ≥20 → 结算 1（弹字完整）；后半 ≥20 → 结算 2（<0.5 s = 30 帧内）。两结之间隔
   < 碎片延迟窗 20 帧 → 第二枚碎片登记覆盖第一枚 pending（接线层 `frag pending overwritten` 日志）→
   「基本只有一次生成碎片」——三项用户观察全对上。§Z18 的「NO BONUS 偶缺」是同一空洞的 <5 静默变体
   （急切切批）；当时归因池空门，timer 门保留 8 帧后（第六跑）症状仍在 → 根因修正为**窗口值对 TH18
   滴入物理太小**，与池空/timer 无关。
2. **TH14 原义不矛盾**：8 = 原生重载常量（`[0x4f5884]` @ th14 `0x43898e`/`0x439d60`，帧尾递减归零即结）
   —— 按 DDC 收集线物理校准：满火力线吸 = 道具一过线即收、到达流紧密，一波收点的吸收空洞 ≪8 帧，
   8 帧窗在 DDC 里就是「流停 ~0.13 s 即结」。TH18 复刻的批闭**结构**照搬原义（timer 语义、最后一收后
   idle 满才关、在飞残余不挡批、迟到拆下一批、结算不等在飞道具）；**窗口值是物理适配参数**：空洞 >8 帧
   拆批在 DDC 不存在、在 TH18 是波内常态。
3. **处置**：`C2X_BATCH_SETTLE_IDLE` 8 → 30 游玩帧（0.5 s）。语义零改：`c2x_collect_batch` 仍只记
   `last_collect_at`（批集 {1,2}，§Z19），`c2x_maybe_settle` 仍 idle 满才关批。音（`0x39`）/弹字/碎片登记
   都在 settle 帧 → 本值是「收点流停后多久发奖」的唯一时旋钮（弹字与钟声同步，不存在挪进
   `spawn_pending_frags` 的分歧，§Z18 注）。单测：新增「波内 20 帧空洞（>8 旧值、<30 新值）不拆批」
   （21+21 并入一批 = ×1.2 一结一碎片）；迟来残余拆批例改为批闭 +5 帧重开、静默自关（语义不变）。
   859 项全过。DLL 21:21 重建（128512 B，md5 `44d3f63f2b990489c45e536ddc3eb618`）已同步 D 盘 mods/
   （唯一加载源）。连带修正：dllverify 导出计数 18→22（Z 系列四个 `BP_ce_item_*` 加进 def 后 Makefile
   一直没同步，本次 `make dllverify` 才暴露 —— H8 之后加的导出漏了自检基线）。
4. **副作用登记**：结算（钟声+弹字+碎片登记）在收点流真停后最多 +30 帧（0.5 s）才发 —— 单次抓取 ≥20
   的体感由 ~0.13 s 变 ≤0.5 s 内响；若第七跑觉得钟声钝，把常量降到 16–20（越接近波内空洞上界 ~25 帧
   越有复拆风险，以实跑为准）。持续农场（空洞 <30 帧）会并入更大的批：流不停不结（DDC 线农正是这个
   体验），奖励按大档 + 碎片循环发放。
5. **遗留（下个杠杆，未定案不臆测）**：若 30 仍偶拆（用户视为同一次过程的空洞 >30 帧），下一个候选 =
   TH14 `0x43898e` 所在 per-item 路径若是对在飞批集道具**每帧续期**（presence reload 而非吸收时重载——
   重载点在距离判定之前、进入条件未追完），则正确修法是「批集道具仍在 swoosh 环内 = 续 idle」，等价
   「道具真正离场 ~8 帧才结」且不动 30 与钟声；先一手定案再动。

**Z21 证据（2026-09-08 run-7，用户：「体感慢了，你不能在各项上对齐 TH15（d3d9 壳内 vanilla 1.00b）
吗？还是机制变了」→ 本跑 = 上条遗留 #5 的定案与落地）**：材料 = th14_full.asm / th15_full.asm /
th18_full.asm（`local/_scratch`）。
1. **TH15/LoLK 一手 census（机制确实变了，但没有「更快的批节奏」可对齐）**：批记账块 {`0x4e7464`
   score-add、`0x4e746c` count-inc、`0x4e7474` qword、`0x4e747c`/`0x4e7480`} 只被 item manager 收点例程
   （`0x4402f9`–`0x440e64` 区间）写、全 exe 无任何读取；timer `0x4e7470` 全文件仅 3 处引用 = 续期门
   自读自写 `0x43fd12`/`0x43fd1d` + `mov [0x4e7470],0x8; ret 4` accessor `0x440ef0` —— **无 dec、无
   cmp/test、无消费端** → LoLK 把帧尾递减 → 五档比较 → 弹字/钟声 → 清零 的整条结算链删了。
   活着的 `0x4e7484`/88（有 cmp 有读）= 点道具总数/总分，`0x43ab79`/`0x43ac50` 只是结算屏「平均分」
   显示除法（divss），不是批链。→ 收点在 LoLK 根本没有批结算事件：TH15 手感的「快」= 批奖整个
   不存在、没有任何结算延迟可感知，不是存在一个更快的原生节奏 —— 「各项对齐 TH15」无锚点。
2. **TH15 续期门残骸与 TH14 同构 = presence 续批是 ZUN 跨作标准布局**：th15 `0x43fce3`-`0x43fcf3`
   （state-3 置位 [edi+`0xc64`]=3 + type 装载 [edi+`0xc68`]）→ `0x43fd12` `mov eax,[0x4e7470]` →
   type 豁免 {9,0xa,0xb,0xd,0xe} 跳过 + 0xf `cmovne` identity + 其余 `cmovne eax,edx` 重载
   （edx=8 @ `0x43f841`）→ `0x43fd42` 起距离判定 —— 与 TH14 `0x438974`（[edi+`0xbf0`]=3）→ `0x43898e`
   （豁免 {9,0xa}、0xb identity、其余重载）→ `0x43899e` 距离/atan2 逐位同构。结构在 DDC 活着、
   在 LoLK 是死代码 → 以 TH14 为对齐基准（非 TH15）成立。
3. **TH14 定案（关 Z20 #5）**：续期门在 state-3 飞行 body 内每帧执行（置 3 后无条件直落到重载、
   距离判定之前）→ **在飞批集道具每帧重载 timer** → 批闭只发生在「既无在飞批集道具、又无新收」
   的真停段后 ~8 帧（帧尾 `0x438dad` 递减、归零即结，Z18 证据 1）。重载常数 = 8 经 `mov [0x4f5884],0x8` accessor
   `0x439d60` 收口（门内 edx 出处为调用方，静态追不到，不臆测）。
4. **处置**：`C2X_BATCH_SETTLE_IDLE` 30→8；core 增 `c2x_batch_presence`（批开即续
   `last_event_at`）；接线 on_item_frame 在 settle 检查前问 `ce_item_pool_any_attracting()`
   （= TH18 的「在飞」：state {3,4}，flip 点 `0x445f6f`/`0x445f63` 置 3、`0x446647` 置 4；落地休止/
   抛物 state 1/2 不续 —— 与 TH14 原义同：未提交道具不属于当前收点流）→ 续期并早退。批闭
   latency 回原生 ~8 帧（0.13 s；Z20 副作用登记的 0.5 s 拖拍消除）。§Z20 的 8–25 帧波内空洞 =
   在飞散布（环内道具飞行期）→ presence 盖掉；环空的真空洞才消耗 idle。
5. **单测**：presence 族 —— 批开后续期不关批、40 帧大空洞被续期桥成单批、批未开 presence 不
   开批（首个收点才开）、批集道具落地休止（无 presence）照旧按 idle 关批、presence 不触发也不
   算计数；§Z20 空洞双向模型化（洞内有在飞 → 并单批 ×1.2 count 42；洞内无在飞 → +8 关批重开、
   ×0.5 各自静默）。常量全部由 `C2X_BATCH_SETTLE_IDLE` 驱动。965 全过。sdk 谓词改名
   `ce_item_pool_any_flying` → `ce_item_pool_any_attracting`（防旧语义误解）。DLL 22:04 重建
   （128512 B，md5 `ff84b1c7c9ba1793d779e483a443eacd`）已同步 D 盘 mods/（唯一加载源）；
   dllx87 0、dllverify 22 exports。
6. **残留（run-8 先验，不臆测）**：presence 只续「环内有在飞道具」的帧 —— 若第八跑仍见拆批 =
   连续 ≥8 帧环内无在飞且无新收的真空洞（细尾散布 >~22 帧到达间隙），兜底 = idle 16（Z20 #4
   预留旋钮），或谓词扩到近环抛物段（先一手追 TH18 state 机再动）。

**Z22 证据（2026-09-08 run-9，用户：「新的bug：我在没有进入全屏收点的模式的时候 no bonus 也触发了。
这个机制是我或者放b，或者上线，或者进入对话自动全屏收p+金钱的时候才会开始触发的」→ 中途补充：
「所以这里面会出现一个问题是：当我下全屏收点线的时候依旧会有p收过来，但是此时产生的p显然不会
飞过来，那么正确的行为自然是当飞过来的p收完之后就结算」）**：材料 = th18_full.asm
（`local/_scratch`；slot loop `0x445a80` + 出生函数 CE_FN_SPAWN_ITEM `0x446f40` 段）。
1. **症状机制链（与当时实现逐条吻合）**：批开 = 任意 type∈{1,2} 收集（含 30px 触收 state 1 /
   70px 环收 state 4）；presence 谓词 = 池扫 type∈{1,2} 且 state∈{3,4} → 普通游玩环收既开批又
   续批 → 小批 5–19 关 → 灰 NO BONUS 误弹；下收点线后新掉 P 经环入批拉长批（用户：「此时产生的
   p显然不会飞过来」）。
2. **state 写点普查**（每槽续点 `0x4467d2` `xorps xmm4,xmm4` → 各 case 的零比较干净）：写点全集 =
   `0x445b0f`(=2)/`0x445e38`(=0 删)/`0x445f6f`(=3)/`0x446146`(=1 自机 state 4 放回)/`0x446595`(=0 commit
   摘链)/`0x446647`(=4 环) + 出生 `0x446fac`(=1)/`0x4471d3`(=5)。state 5 = 出生 intro（+`0xc84` 倒计时，
   `0x445af3` 递减 → 到期置 2 + FX `0x407420`）——**state 5 唯一出生点 = `0x446f40` 的 type 9–0xe 分支
   `0x44713a`**（`0x446f62`–`0x446f8f` 六个 je；帧计数 [ecx+`0xe6bb20`] 按 `0x400`/`0x200`/`0x100` 分档取模
   散布 → 倒计时错峰）。state 2 = 释放下落体（`0x445ed6` 纯重力积分）→ vel_y ≥ 0（顶点）`0x445f4e`
   `comiss xmm0(=新 vel_y), xmm4(=0)` → 落穿 `0x445f59` → `0x445f5b` → `0x445f63` 置速 + `0x445f6f` 置 3
   —— 无条件翻 3，但只覆盖 9–0xe 族（P/钱 type 1/2 在该出生函数永远走 `0x446fa3` 常规 state-1 分支，
   不经过 state 2/5）。
3. **V 门 = type {1,2} 进 state 3 的唯一入口**（state-1 case：countdown ≤ 0 → 每帧跑 `0x445b9a`）：
   (a) 上线：player state ∉ {2,4}（`0x445ba3`/`0x445ba8` je `0x445bbf` 跳过线检）且线 `[player+0x47998]`
   > 自机 y `[player+0x624]`（`0x445bb2` comiss → ja `0x445f63`）；(b) 放 B：BombManager `[0x4cf2b8]`
   +`0x30` == 1 且 +`0x38` < `0x3c`（`0x445bc4`–`0x445bce`，R1 已确认 +`0x30` = 放弹标志）；(c) `[0x4cf2e0]`
   +`0x1b0` ≠ 0（对话自动收点；对象未命名 🟡）。无 V → `0x445be6` 起重力坠落体（state 保持 1）→ 逐帧
   过 `0x44620a` 共享尾（30px 触收闸 `0x44624a` / 70px 环 `0x4465da`）→ 出界 [`0x4b93a4`](y)/[`0x4b9340`](|x|)
   删 —— **普通游玩永不翻 3**。
4. **引擎原生谓词 = 分发时 slot state == 3**：state 3/4 跳过环入口（`0x4465da` 读 state；`0x4465e3`/
   `0x4465e8` je `0x446659`）直飞至 <30px 分发 `0x44625f`（分派前 state 未清）；P handler `0x446870` / 钱
   handler `0x446b00` 分发后读 `[slot+0xc74]`（`0x44699e`/`0x446c0a`）——state==3 与「自机在收点线上方」
   （线 `[player+0x47998]` ≥ 自机 y）汇到同一处理点（`0x446a21`/`0x446cb4`），线下方且 state≠3 走另
   一段（`0x4469a7` 起，分值分野语义未全追，不挡本门）—— TH18 用 state 3 认「飞过来的收点道具」。
5. **修复（三改 + 一校正）**：(a) `on_item_collect` 成员门 —— type∈{1,2} 之外还要求分发时 slot
   state == CE_ITEM_STATE_VACUUM(3)（engine.h 新常量）；触收/环收不开批、不计数、不 stash；
   (b) presence `ce_item_pool_any_attracting` 从 state∈{3,4} 收窄为 ==3（4 = 普通游玩 70px 环
   swoosh，不续批；TH14 原生 `0x43898e` 也只在 state-3 在飞体上重载，§Z21）；(c) 注释同步
   collect2x.c / collect2x_core.h / sdk.h / sdk.c；(d) engine.h CE_ITEM_STATE 注校正（旧注把 5 当
   「收集完成延迟段」、2 当「分派处理瞬态」、谓词 state∈{1,3,4} = §Z18 池空门时代模型，与普查
   矛盾）。批闭语义与用户补充完全对齐：下收点线后残余 state-3 在飞 P 继续计数、presence 续到
   收完 +8 帧即结；新掉落（无 V 不翻 3）永不开批不续批。Z21 证据尾部「谓词扩到近环抛物段」遗留
   （item 6 残留）随本行作废：谓词按 state==3 收窄而非扩集。
   🟡 **残留**：对话 V 门 `[0x4cf2e0]+0x1b0` 对象未命名（邻 BombManager `0x4cf2b8`）；player state
   2/4（`0x445ba3` 系跳过上线判定的两态）语义未追；state-1 坠落体 `0x445c85` `jl 0x445de6` 的横滚段
   （vel_x 清零/出界 [`0x4b9340`]）与 `0x44698d` 系「线下方分值分野」语义 —— 都不挡本门（本门只用
   state==3 谓词）。
**Z22 成员门实跑通过（2026-09-09 整机回归，log `th18_card_expand.log` 00:49 会话，0 条 `FAIL:`/`mitigation:`）**：
起手无 114 的五局（dev 起手 [76,71,70]，正常过关 / 进店 / boss 战）collect2x **零日志** —— 静默侧成立（图鉴 / 编成
24 张新卡全在册但机制不动）；局中拿到 114 后掉 P 成行（`P drop (power …, t=…) @(敌身位)`，落点贴合敌人、间隔随
火力节流）→ 三笔 settle 全 `x2.000`（263 / 103 / 82 items，`t=3837/4056/4278`，各带 frag 0x10 spawn 延迟 20 帧）。
用户验收：「有就起效、没有不起效」。遗留（未复验，想测再测）：钟声 / 弹字体感、下收点线瞬间残余 state-3 在飞
并入本批的专项（本段 1419 行的语义已对齐）、空卡组买倒吊人（§AJ 尾）。
## AJ. 豌豆射手（id 77）—— 第一张真走 SHT shooterset 的卡（子机连射 + 机枪爆发）

**没有手写机器码、没有新断点、不改任何 exe 字节**。两处内联汇编（`ce_allocate_option` / `ce_tick_shooters_for_card`）
早在 §U 就照零售调用点复刻好了，本卡是 `ce_tick_shooters_for_card` 的**第一个调用者**——所以这一节把那条链
从头到尾一手重走了一遍（2026-09-08，Ghidra：`Player__tick_shooters_for_ability_card` `0x40a9c0`、
`Player__shoot_one_bullet` `0x45e930`、`PlayerBullet__create` `0x45e320`、`Player__tick_shooting_state` `0x45ea00`）。
新写入面只有 `pl0X.sht` 偏移数组的两个空位 + 文件尾两段数据；`ability.anm` 加两个 entry / 四个脚本（本来就在重建）。
纯逻辑 `peashooter_core.c` 主机单测（等待区间、开大形状、周期、inflight、清零）。

| # | claim | 结论 |
| --- | --- | --- |
| AJ1 | 追加 shooterset 不动零售任何字节 | **CONFIRMED（工具每次回读校验）** —— `append_shooterset.py` 的三条不变式：头部 `+0x02` 仍是 40、shooterset 0 的四个 func 仍全 0（空位重复解析靠它幂等）、前 23 组逐字节相同。索引 `0x17` / `0x18` 落在零售留空的 17 项里 |
| AJ2 | `ce_tick_shooters_for_card` 的调用约定 | **CONFIRMED** —— `0x40a9c0` 反编译为 `(param_1 = option, param_2 = short, param_3 = long, param_4 = set)`，stdcall `ret 0x10`。`sdk.h` 的压栈序与之一致（§U 已复刻，本卡首次真调用）|
| AJ3 | **short_timer 可以由我们自己数** | **CONFIRMED（一手，本节的关键）** —— 见下「AG3 证据」：`param_2` 只进一处模运算，往下传的那份在 `PlayerBullet__create` 里从未被引用 ⇒ 传什么值都只影响「这一帧发不发」 |
| AJ4 | `+0x1c` 每帧只广播一次，且只在按住射击键时 | **CONFIRMED** —— 见下「AG4 证据」。⇒ 我们的计数器只在射击帧推进，「开大」不会在不开火时白烧；死亡 / 被读条状态（`player+0x476ac != 1`）也自动暂停 |
| AJ5 | `opt_slot` 必须 0 | **CONFIRMED** —— `0x45e320`：`if ((shooter[0x20] & 0xf) == 0) 位置 = *param_3`（= `0x40a9c0` 传进来的子机坐标）`else` 读 `player` 自己的第 N 个子机位置块。非 0 就会打到零售子机的位置上 |
| AJ6 | `fire_rate` 的取值范围 | **CONFIRMED** —— i8、符号位是 shooterset 终止符 ⇒ 1–127；`0` 在这条路径上是裸 `idiv`（`0x40a9c0` 没有零分支，主炮那条有）。我们用 60 / 5，工具也在 `pack_shooter` 里挡住越界 |
| AJ7 | 弹的贴图取自 `ability.anm` | **CONFIRMED** —— 见下「AG3 证据」的第 2 行与末行：`0x40a9c0` 开火期间换 `player+0x10`、结束还原；`0x45e320` 建 VM 用的就是它 ⇒ shooter 的 `+0x22` 填 ability 脚本号，四个 `pl0X.anm` 不动 |
| AJ8 | 音效字段 `< 0 = 不响` | **CONFIRMED** —— `0x45e320` 末尾 `if (-1 < *(short *)(shooter+0x24)) play_sound(...)`。平时那颗给 `0x20`（`se_gun00`，−15 dB / 优先级 10，1.42 s 一次）；开大那 7 × 20 颗给 **−1**（20 发/秒的音会炸耳）|
| AJ9 | 出膛偏移 `spawn_dx/dy` 会被减掉一帧速度 | **CONFIRMED（设计上正确）** —— `0x45e320`：`mode` 不是 4/5/6 时 `pos += (spawn_dx − vx, spawn_dy − vy)`。我们 `mode = 0`、角度 −π/2 ⇒ `vx = 0, vy = −speed`，所以 7 颗横向铺开 ±48 原样生效，纵向统一「先退一格、第一帧补回来」——这正是零售减法的用意 |
| AJ10 | 平时那颗 90 会被每敌每帧上限砍 | **已规避** —— 上限每帧从 `sht+0x28` 复位（Reimu 90 / Sakuya 60 / Sanae 120 / Marisa 160）：90 塞满 Reimu、超出 Sakuya，还要与自机弹抢配额。**豌豆在途的 40 帧**里 `ce_raise_damage_cap(180)`（Remilia `0x40f48e` 同款）；开大那排 49 不用抬 |
| AJ11 | 抬上限的副作用 | **设计（有意，已收窄）** —— 上限是自机弹与所有卡共用的，抬高对同帧的自机弹也放宽。收窄见下「AG11 说明」 |
| AJ12 | 时序 | **CONFIRMED** —— GameThread（0x10）复位上限 → AbilityManager（0x16）`on_tick_2` 抬 → Player（0x17）`on_tick_shooters` 出膛 → Enemy（0x1b）结算 ⇒ 同帧有效、不用还原。子机申请仍挂 `+0x18`（`0x45d5e0` 尾部广播）|
| AJ13 | replay 安全 | **CONFIRMED** —— 只有两处抽随机：开局第一个射击帧、每次开完大。两处都由 `on_tick_shooters` 驱动 ⇒ 调用序列完全由输入决定，replay 里逐次重放。`ce_rand` 用的就是游戏自己的 `REPLAY_SAFE_RNG`。节奏其余部分是整数计数，无浮点 |
| AJ14 | 子机槽认领 | **CONFIRMED（§U3 同款）** —— 12 个槽是公共池；用前查 `option+0x00 != 0 && option+0xd0 == card+0x08`。子机指针存 `ce_state()` 而不是 `card+0x54`（§U2）|
| AJ15 | 自机弹池装得下 | **CONFIRMED** —— `0x45e930` 的槽扫描上界是 **0x1ff = 512** 发。开大 140 颗摊在 100 帧里、每颗 448/18 ≈ 25 帧飞出场 ⇒ 同屏峰值约 **39** 颗，加主炮也远不到 512。满了 `shoot_one_bullet` 直接 `return 0`，不崩 |
| AJ16 | 与其他卡的交互 | **设计** —— `PlayerBullet__create` 末尾对每张卡广播 `on_bullet_created`（`vtable+0x28`），所以 K♠（×1.1）与古锭刀（终符 ×2）**也会作用在豌豆上**——同一条自机弹管线，一致而非例外。反过来豌豆不吃炸弹 / `ce_damage_rect` 那条路 |
| AJ17 | 数值 | **设计（用户定，2026-09-08 定稿）** —— 平时 **60 帧一颗 × 90 = 90 DPS**（PVZ 原版是 1.425 s ≈ 85 帧，用户改成整 1 秒：DPS 与 85/130 几乎相同、节奏更密）；开大 20 排 × 7 × 7 = **980**。周期 20–30 s ⇒ 综合约 **123 DPS**，与破损核心同档。代价：必须按住射击键 |
| AJ18 | `build_ability.py` 的排序改动 | **CONFIRMED 无副作用** —— 脚本文件原来按文件名字典序排，`100_…` 会排到 `68_…` 前面。改成按**编号数值**排；68–99 的相对顺序不变（两位数的字典序与数值序一致），`make anm` 重建后零售部分逐项自检仍通过 |

**AG3 证据**（`0x40a9c0` 反编译，逐行）：

```c
pcVar4 = *(char **)(*(int *)(PLAYER_PTR->field_47924 + 0x1c) + 0xe0 + ((param_4 << 8) >> 8) * 4);
*(void **)(PLAYER_PTR->field_0 + 0x10) = ABILITY_MANAGER_PTR->ability_anm;   /* ← AJ7 */
local_10 = *(int *)(param_1 + 0x5c) * (1/128.f);   local_c = *(int *)(param_1 + 0x60) * (1/128.f);
do {
    if (pcVar4[0x29] == 0) { iVar3 = param_2 % (int)pcVar4[-1]; cVar2 = *pcVar4;    }   /* short % fire_rate  == start_delay  */
    else                   { iVar3 = param_3 % (int)pcVar4[0x29]; cVar2 = pcVar4[0x2a]; }/* long  % fire_rate2 == start_delay2 */
    if (iVar3 == cVar2) Player__shoot_one_bullet(this, param_4 << 8 | uVar5, param_2, &local_10, param_1);
    ...
} while (-1 < *pcVar1);
*(undefined4 *)(this->field_0 + 0x10) = *(undefined4 *)(this->field_0 + 0xc);   /* 还原 */
```

`param_2` 在这里只出现两次：模运算、以及原样往下传。`Player__shoot_one_bullet` `0x45e930` 也只是
`PlayerBullet__create(this_00, param_1, param_2, param_3, param_4)` 转手；而 `PlayerBullet__create` `0x45e320`
的**整个函数体里 `param_2` 再没有被引用过**（Ghidra 的形参列表里有它，正文没有）。
⇒ 传我们自己的计数器，语义上等价于「这一帧发不发由我们说了算」，没有任何别的副作用。
顺带坐实了 [`engine/sht/th18/03`](../../../engine/sht/th18/03-func-tables-timers-and-dead-fields.md) §1 的推论：
引擎的短计时器只在 0..14 循环，**用它根本表达不出 85 帧的节奏**（`0x13` 那组 `fire_rate = 40` 实际就是每 15 帧一发）。

**AG4 证据**（`0x45ea00` 反汇编）：

```
0045EB35  cmp  [esi+0x476ac], 1        ; 玩家状态机 != 1（死亡 / 演出）→ 0045ED88 全跳过
0045EB42  cmp  [esi+0x476d4], 0        ; short < 0（松开射击键）→ 走 0045EB4B 的「是否按住」判定
0045EB4B  test byte [0x4ca428], 1      ;   没按住 → 0045ED1A，广播不发生
0045EBB1  cmp  eax, [ebx]              ; short.current == short.previous（计时器这帧没走）→ 0045ECCB 跳过
...                                    ; 主炮循环
0045ECA0  mov  eax, [0x4cf298]         ; AbilityManager
0045ECA5  mov  edi, [eax+0x1c]         ; 卡链表头
0045ECB2  mov  ecx, [edi]              ; card
0045ECB4  push esi                     ; long_timer
0045ECB5  push ebx                     ; short_timer
0045ECB8  call [eax+0x1c]              ; ← +0x1c 广播，每帧一次
```

基类 `+0x1c` 桩 `0x413080` = `33 C0 C2 08 00`（`xor eax,eax; ret 8`）⇒ thiscall + 两个栈参，
与 SDK 的 `int (ce_card_t *, uint32_t, uint32_t)` 一致（PE 直读）。

**AG11 说明**（为什么抬上限是安全的）：收窄成两条——① 只在豌豆可能命中的那 40 帧内抬（`pea_needs_cap`，
开火帧置 `PEA_INFLIGHT`、每帧递减；间隔 60 帧 > 40 ⇒ 两颗的窗口不重叠，单测有断言）；② 抬到固定 **180** 而不是「按当前值翻倍」，且 `ce_raise_damage_cap` 只抬不降
⇒ 与炎魔 / 青眼（都是 300）同帧时取更高者，不叠乘。要让这次放宽真的加强主炮，主炮得在同一帧对同一敌人打出
本机体上限那么多（Reimu 90 = 5400 DPS），零售四机体都做不到；实际它只在我们自己的豌豆落地那一帧起作用。

**实跑要看的**：见 [`NEXT.md`](NEXT.md) §1m。崩溃优先怀疑：`ce_tick_shooters_for_card` 的压栈序（AG2，第一次真调用）、
`ce_allocate_option`（AG14 / §U1，已随破损核心走过同一条路）。视觉怀疑：豌豆大小（script100 / 101 的 `scale`）、平时那颗的节奏（`PEA_INTERVAL`）、
子机高低（`PS_OPTION_OFFSET` 与 script102 的 `scale`）、开大那堵墙的宽度（`spawn_dx` 七项）。

## AA. 自机弹伤害倍率层（`ce_dmg_*` + `.bullet_damage_permille`）—— 2026-09-08 提炼

**没有新断点、没有新引擎调用、不改任何字节**：只是把「给自机弹加伤害」这个已经出现三次的动作
（K♠ 61、古锭刀 76、衔尾蛇 78）从各卡收进 SDK。纯逻辑 `ce_dmg_*` 在 `sdk_core.c`，主机单测。

| # | claim | 结论 |
| --- | --- | --- |
| AA1 | 该提炼 | **本仓约定** —— `SDK.md` 方案 3：「动作出现第二次就进 `sdk.h`」。这是第三次，且前两次的写法已经分叉（一张抬上限、一张没抬）|
| AA2 | 桩看到的是引擎刚写好的原始伤害 | **CONFIRMED（一手）** —— `PlayerBullet__create` `0x45e320` 先 `bullet+0x9c = shooter+0x02`、再遍历卡链表调 `+0x28`（同一份反编译见 §AJ3 证据）|
| AA3 | **就地乘**，不从 shooter 基值重算 | **CONFIRMED（对抗证伪）** —— 见下「AA3 证据」：零售 `CardMomoyo` 就是重算派，重算会把她的擦弹加成整个抹掉 |
| AA4 | 一颗弹只乘一次、只取整一次 | **设计** —— 聚合倍率由**落笔人**一张卡在它的 `+0x28` 桩里应用，其余参与者的桩只有一条 `id != applier` 的比较。旧写法逐张取整会漂：见下「AA4 证据」|
| AA5 | 落笔人的选法 | **设计** —— id 最小的在场参与者（`ce_dmg_applier`）。与卡在链表里的顺序无关 ⇒ 同一副牌组每次结果相同，单测钉住 |
| AA6 | 上限自动跟着抬 | **CONFIRMED（时序）** —— GameThread（优先级 0x10）每帧从 `sht+0x28` 复位 `player+0x47984` → AbilityManager（0x16）跑 `+0x2c`，落笔人在这里 `ce_raise_damage_cap(基值 × 聚合)` → 敌人结算（0x1b）。同帧有效、不用还原 |
| AA7 | 生命周期 | **设计** —— 绑定（`ce_card_bind`）登记声明的初值；`+0x50`（对象删除，含倒吊人摧毁）清 0 退出聚合；`+0x4c`（局末）回到声明初值；`ce_sdk_setup` 整体复位。全在 SDK 桩里，卡不用记 |
| AA8 | 边界 | **设计（写进名字）** —— `bullet_damage`：只管 `bullet+0x9c`。炸弹与 `ce_damage_rect` 的伤害源（青眼 / 炎魔 / 破损核心 / 真银圣剑）走另一条管线，**不**在这一层里 |
| AA9 | replay 安全 | **CONFIRMED** —— 全整数、无随机；聚合只在卡自己报值时变，而报值的时机由卡的确定性逻辑驱动 |
| AA10 | 迁移不改既有行为 | **一处例外，是修 bug** —— 古锭刀等价（×2 + 抬上限，仍走 `GD_MULT`）；**K♠ 从此会抬上限**了：它以前只翻弹不抬，贴脸打 boss 时那 +10% 有相当一部分被每敌每帧配额吃掉 |
| AA11 | 与零售 `CardMomoyo` 的顺序依赖 | **既有性质，非本层引入** —— 她重算覆写，谁在链表里靠后谁说了算。改之前 K♠ 也是同样的处境；本层没有让它更好也没有更坏 |
| AA13 | HUD 读数 | **设计** —— 聚合倍率不是 ×1 时，落笔人在 `+0x48 on_draw` 里把 `x1.60` 写在火力值左边（§AC）。没有加伤害的卡在场时一个字都不画 |
| AA12 | 桩的开销 | **可忽略** —— 每颗弹对每张卡多一条 `id != applier` 的比较（原本就要走一次桩）；`+0x2c` 同理 |
| AA14 | `+0x4c` / `+0x50` 的清账**不看声明值** | **2026-09-08 修**，见下「AA14 证据」 |

**AA14 证据**：这两个桩原本包在 `if (ce_hooks_##ID.bullet_damage_permille)` 里，看的是**声明**值。
一张声明 0、只在运行时调 `ce_set_bullet_damage_permille()` 参与的卡（接口允许，只是现有三张都声明了
非零初值，所以还没碰到）在局末与删对象时**不会退出聚合**，倍率跨局残留。守卫去掉，两处无条件调
`ce_dmg_set` —— 对没参与过的卡是幂等的（写 0 覆盖 0），AA7 那句「卡不用记」才真的成立。

**AA3 证据**：`CardMomoyo__on_bullet_init` `0x411010` 反编译：

```c
uVar1 = *(uint *)(param_1 + 0xac);                                   /* packed id */
iVar2 = *(int *)(*(int *)(PLAYER_PTR->field_47924 + 0x1c) + 0xe0 + ((int)uVar1 >> 8) * 4);
*(int *)(param_1 + 0x9c) = (int)(fVar4 * (float)(int)*(short *)((uVar1 & 0xff) * 0x5c + iVar2 + 2));
```

= `bullet+0x9c = min(1 + 擦弹/N, 上限) × shooter[+0x02]`：她从 packed id 回查 shooter 行、**从基值重算并覆写**。
我们要是也重算，同时带她时她的加成就没了。就地乘则与她相乘。

**AA4 证据**（为什么单次取整值得专门做）：三层 +20% 加法叠、基值 90。
逐张各自取整 `90 → 108 → 129 → 154`；一次算完 `90 × 1600 / 1000 = 144`。旧写法漂 10 点、且层数越多越漂。
单测 `test_sdk_core.c::test_dmg_layer` 把两个数都钉住了。

**实跑要看的**：K♠ / 古锭刀 / 衔尾蛇任意一张在场时日志有 `sdk: … bound (… .bullet_damage_permille = …)`；
古锭刀终符那两行不变；K♠ 打 boss 的血条掉速应比改之前**略快**（上限修好了）。崩溃不该有——这一层没有新的引擎入口。

## AB. 衔尾蛇（id 78）—— 每 miss X 次永久 +20％，X 递增

**没有新断点、没有新引擎调用、不改任何字节**：计数挂已经在用的 `+0x0c`，加伤害走 §AA 的层，
HUD 数字走 §X 已经跑通的那套。纯逻辑 `ouroboros_core.c` 主机单测。

| # | claim | 结论 |
| --- | --- | --- |
| AB1 | `+0x0c` 是「这一次是真的 miss」 | **CONFIRMED（§S 已用）** —— `on_death_after_deathbomb`，决死窗口已经过去。加倍（69）就在这个槽里多扣一条命 |
| AB2 | 返回 0 = 不救命，只记账 | **CONFIRMED（零售先例）** —— `CardTewi` 在同一个槽里只快照金钱、`return 0`（§3 表）|
| AB3 | 门槛序列 | **设计** —— X 初值 1、每次触发 +1 ⇒ 第 N 层在累计第 `N(N+1)/2` 次 miss 达成：1 / 3 / 6 / 10 / 15 …。单测逐次钉住前 9 层 |
| AB4 | 加法叠而非乘法 | **设计（用户定）** —— 1.2 / 1.4 / 1.6 …。残机有限 ⇒ 一局现实里 2–3 层、极限 4 层，自限 |
| AB5 | 「永久」的范围 | **设计** —— 一整局（run）之内。`+0x4c` 局末清零，倍率由 SDK 在同一个桩里回到声明的 ×1（§AA7）|
| AB6 | ctor 幂等 | **CONFIRMED（已知坑）** —— 每关开场引擎会对卡组每张卡再调一次 `+0x00`。这里只在 `need == 0`（= `ce_state` 刚清零）时铺初值，重调不会把层数抹掉 |
| AB7 | 零状态自愈 | **设计** —— `ce_state` 给的是清零内存，`need = 0` 是不可能的合法值；`ob_on_miss` 见到它先 `ob_reset`。单测覆盖（万一关卡中途买到卡、ctor 没赶上）|
| AB8 | HUD 数字 | **复用 §X** —— `+0x44` 拿本卡 HUD 图标根 VM，`ce_anm_spawn_ui` 起 script99、`ce_anm_set_sprite` 换 `TS_DIGIT_0..9`。图标重建时 `+0x44` 会再来一次；`+0x2c` 每帧对账，VM 没了就重建 |
| AB9 | 上限 | **归层** —— 叠到 ×1.6 不抬上限基本等于没加（Reimu 90）。这张卡一行都不用写，§AA6 自动做 |
| AB10 | replay 安全 | **CONFIRMED** —— 纯计数，无随机、无浮点。miss 的发生本身由输入决定 |
| AB11 | 与加倍（69）的交互 | **设计（反协同，有意）** —— 加倍让每次 miss 掉 2 命，衔尾蛇要靠 miss 成长：同时带这两张会更快 game over。属于卡池里正常的取舍 |
| AB12 | 层数上限 | **设计** —— `OB_MAX_STACKS = 9`（HUD 只有一位数字）。到顶后门槛也不再涨，避免计数溢出；现实里够不着（第 10 层要 55 次 miss）|

**实跑要看的**：见 [`NEXT.md`](NEXT.md) §1n。崩溃优先怀疑：`ce_anm_spawn_ui` 的 `ret 0x10` 第四个哑参（§X1，真银圣剑同款，尚未实跑）。

## AC. HUD 文字原语 `ce_hud_text`（`AsciiManager__drawf_debug`）—— 2026-09-08

用户要「把总的伤害倍率显示在火力值旁边」。**没有新断点、不改任何字节**：零售的
`CardMomoyo__on_draw` `0x4110a0` 就是「一张卡在 HUD 上画数字」的现成先例，本节把它的调用点逐指令复刻成一个原语。
第一个用户是伤害层的倍率读数（§AA13）。

| # | claim | 结论 |
| --- | --- | --- |
| AC1 | `AsciiManager__drawf_debug` `0x41a110` 的调用约定 | **CONFIRMED（一手）** —— 见下「AC1 证据」：`(mgr, pos[3], fmt, ...)`，**cdecl varargs**（调用方 `add esp, 0x14` 清 3 个 push + 一个 double）|
| AC2 | `ASCII_MANAGER_PTR = 0x4cf2ac` | **CONFIRMED（两处交叉）** —— Momoyo 的 `mov ecx,[0x4cf2ac]`；th-re-data 的 `statics.json` 同名同址 |
| AC3 | 字色在 `mgr+0x19228`，BGRA 一个 dword | **CONFIRMED** —— Momoyo `mov [ecx+0x1922b], al`（alpha 在 +3）与 `mov dword [eax+0x19228], 0xffffa000`（= B 00 / G A0 / R FF / A FF，橙红，与反编译的 `color[0..3]` 逐字节对上）。画完还原成 `0xffffffff`（`0x41128a`），我们照做 |
| AC4 | 只能在虚表 `+0x48 on_draw` 里调 | **CONFIRMED** —— `AbilityManager__on_draw` `0x408ab0` 每帧遍历卡链表调 `+0x48`；基类桩 `0x413160` = `33 C0 C3`（`xor eax,eax; ret`）⇒ thiscall 无栈参，与 SDK 的 `int (ce_card_t*)` 一致。**别在 `on_tick_2` 里调**：那是 tick 阶段 |
| AC5 | 不转发 varargs | **设计** —— 我们自己 `vsnprintf` 成一行，再用固定的 `"%s"` 交给引擎 ⇒ 调用形状永远是 4 个 push、清 0x10。躲开了 cdecl 变长栈与 `float → double` 提升（Momoyo 那一路要 `sub esp,8` + `movsd`）|
| AC6 | 坐标空间 | **CONFIRMED** —— Gui 的 `FUN_0043cd00` 里全是字面量：火力 `"%d."` (540, 182)、小数 (560, 189) scale 0.6、`"/%d."` (574, 182)、金钱右对齐 (620, 204)。Momoyo 把 VM 屏幕坐标 `÷ [0x56aca0]` 换算进同一个空间 ⇒ 直接给字面量即可，不用换算 |
| AC7 | 不设 font / group / scale / alignment | **照 Momoyo（有风险，已知）** —— 她只设字色，其余用环境值。所以我们的**位置与字号取决于同帧早些时候谁最后设过**，第一次实跑要把位置贴回来调（真银圣剑的 HUD 数字也是同一句话）|
| AC8 | 缓冲区 | **设计** —— 我们 128 字节、引擎那边 0x108 字节。倍率读数最长 `x99.99` |
| AC9 | 依赖面 | **CONFIRMED** —— `vsnprintf` 走 msvcrt，DLL 本来就依赖它；`make dllverify` 仍是 18 导出 / KERNEL32 + msvcrt 两个依赖 |

**AC1 证据**（`CardMomoyo__on_draw` `0x41122e`–`0x411286` 逐指令）：

```
0041122E  mov  ecx, [0x004cf2ac]          ; ASCII_MANAGER_PTR
00411234  sub  esp, 8                     ; 给 double 腾位置
00411245  mov  [ecx+0x1922b], al          ; color[3] = 图标当前 alpha
00411270  movsd [esp], xmm0               ; 第 4 参（%.3f 的值，float 提升成 double）
00411275  push 0x004b4c6c                 ; 第 3 参 fmt "%.3f"
0041127A  push eax                        ; 第 2 参 &pos（float[3]）
0041127B  push ecx                        ; 第 1 参 mgr
0041127C  call 0x0041a110
00411286  add  esp, 0x14                  ; 调用方清 0xc + 8 ⇒ cdecl
0041128A  mov  dword [eax+0x19228], 0xffffffff   ; 还原字色
```

**实跑要看的**：见 [`NEXT.md`](NEXT.md) §1n 的 HUD 那一段。崩溃优先怀疑：`ce_hud_text` 的 cdecl 声明（AC1）、
`ASCII_MANAGER_PTR` 在关卡外为空（已判空）。视觉怀疑：位置 / 字号（AC7：改 `CE_HUD_DMG_X` / `CE_HUD_POWER_Y`）。

## AD. 卡侧重复原语的提炼（`ce_option_*` / `ce_hud_digit_*` / `ce_refuse` / `ce_enemy_by_id` / `ce_player_x`）—— 2026-09-08

**没有新断点、没有新引擎调用、没有新的机器码，一个字节都没改。** 只是把在两三张卡里**逐字重复**的
C 代码原样搬进 `sdk.h` / `sdk.c`（`SDK.md` §5 的约定：出现第二次就提炼）。引擎侧的调用约定包装
（`ce_allocate_option`、`ce_anm_spawn_ui`、`ce_anm_set_sprite`、`ce_play_sound`）**没有被动过**，
它们的审计（§U1、§X、O22、O29c）继续有效。

| # | claim | 结论 |
| --- | --- | --- |
| AD1 | 子机三件套是逐字重复 | **CONFIRMED（机械比对）** —— `broken_core.c:36-67` 与 `peashooter.c:35-66` 归一化标识符后 diff，只剩注释差异 |
| AD2 | HUD 数字是逐字重复 | **CONFIRMED（机械比对）** —— `truesilver.c:16-38` 与 `ouroboros.c:28-50` 只差一个钳位表达式（`< 0 → 0` vs 无）与一行日志。合并后统一钳 `0..CE_HUD_DIGIT_MAX`，两张卡的取值域都在里面 |
| AD3 | 破损核心的行为等价 | **CONFIRMED（逐句对照）** —— `ce_option_get` = 「认领；失败则倒计时到 0 再申请一次」，与原 `option_of` + retry 分支同一条控制流；`ce_option_x/y` 是原来那两行定点转换 |
| AD4 | 豌豆射手的行为等价 | **CONFIRMED（刻意保留分叉）** —— 它原本在补申请的那一帧**无条件 `return 0`**（成功也不出膛），与破损核心「补上就立刻用」不同。所以它用的是 `ce_option_claim` + `ce_option_retry` 两步而不是 `ce_option_get`，节奏计数器 `pea_frame` 仍从下一帧起算 |
| AD5 | 真银圣剑的状态结构变了 | **无行为影响** —— HUD 三个字段从纯逻辑的 `ts_state_t` 搬进卡侧的 `ts_card_t{ ts_state_t st; ce_hud_digit_t hud; }`（与衔尾蛇的 `ob_card_t` 同构）。`truesilver_core.c` 与它的 12 条主机单测一个字没改、照过；私有状态块 `ce_state` 上限 256 字节，两张卡都远没到 |
| AD6 | 日志差异 | **只多不少** —— 衔尾蛇现在也会在图标消失时记一行 `ouroboros: hud icon gone, digit removed`（原本只有真银圣剑记）。其余日志文本逐字不变（卡名由参数传入）|
| AD7 | 拒绝发动的三份合并 | **CONFIRMED** —— 青眼 67 / 神之宣告 66 / 炎魔 72 的 `refuse()` 逐字相同（`CE_SE_INVALID` + `card: refused (why)` + `CE_ACTIVATE_REFUSED`）。合并后调用点写成 `ce_refuse("blue_eyes", "no lives")`，日志文本不变 |
| AD8 | 按 enemy_id 找敌人的两份合并 | **CONFIRMED** —— 古锭刀 76 与神之宣告 66 的 `find_enemy` 逐字相同（照 `get_boss_enemy_full` `0x4237f0` 的链表走法，`guard < 4096`）。`ce_enemy_by_id` 多一条 `em` 判空，调用点原本都已经判过 |
| AD9 | 编译门 | **通过** —— `make test-host`（全部主机单测）、`make dll`（零警告，`-Wall -Wextra`）、`make dllx87`（我们的目标文件里 x87 指令 0 条）、`make dllverify`（PE32/i386、18 导出、依赖仍只有 KERNEL32 + msvcrt）、`pytest`（8 passed） |
| AD11 | 敌人链表遍历的第三次 | **CONFIRMED（机械比对）+ 一处收紧**，见下「AD11 证据」 |
| AD12 | `flags &= ~2u` 的裸魔数 | **收进 `ce_option_drop`**，见下「AD12 证据」 |
| AD13 | float3 假设本来就在承重 | **这是收紧，不是扩面**，见下「AD13 证据」 |
| AD14 | 符卡状态的两处读法 | **CONFIRMED** —— 古锭刀 76 与神之宣告 66 各自 `*(uint32_t *)(spell + CE_SPELL_FLAGS)`。`ce_spell_flags()` 把「管理器为空」统一读作 0（= 不在符卡里），古锭刀原来的 `!spell` 判空分支因此可以去掉，语义等价 |
| AD10 | 实跑影响面 | **应当为零** —— 涉及的五张卡（71 / 77 / 75 / 78 / 66 / 67 / 72 / 76）本来就都在「待实跑」清单里，本次提炼不改变它们各自的验收标准；实跑时若哪一张的表现与 [`NEXT.md`](NEXT.md) 的描述不符，先怀疑 AD3–AD6 这几条等价性 |

**AD11 证据**：`firelord.c:75,81` 与 `broken_core.c:84` 三处手写同一条链表走法，且**都没有 guard**，
而 SDK 里 `ce_enemy_by_id` 那份有。合并成 `CE_ENEMY_FOREACH` 后三处一律带 `CE_ENEMY_LIST_GUARD`（4096）：
坏链表不再可能转死。过滤谓词 `ce_enemy_lockable`（非空 + 不带 `NO_LOCK`）是三处逐字相同的那一条。

**AD12 证据**：破损核心 71 与豌豆射手 77 都在 `on_run_reset` 里紧挨着 `ce_option_drop` 写 `flags &= ~2u`。
零售 `CardReimu1__method_4C` `0x40aad0` 本来就是「清 bit1 + 删子机 VM」一件事，折进去是照零售、不是我们的发明。
常量 `CE_FLAG_EQUIP_RESET_BIT1` **🟡 只描述「零售在收起子机时清的那一位」，不主张它的语义**。

**AD13 证据**：`CE_PLAYER_X` 的 engine.h 注释原本只写 `float px`，但青眼 / 豌豆射手 / 炎魔三张卡
已经在读 `[1]` `[2]` —— 假设散在三处、一处都没记。收进 `ce_player_pos()` 并把 float3 写进 engine.h
注释之后，它变成一条**可审计**的断言。`ce_enemy_pos` 同理（破损核心 / 炎魔两处）。

## AE. 旋风斩（id 79）—— 炸弹额外连放 ⌊火力⌋−1 发，代价同数的火力

**没有新断点、没有新引擎调用、不改任何字节**：用的全是已有的件 —— SDK 事件 `on_bomb_spent`（腐化 §T 开的断点
`0x4203bc`）、引擎自己的 `do_bomb`（黄昏 §R 用的 `ce_do_bomb`）、引擎自己的 `spend_power` + `repopulate`（炎魔 §V）。
纯逻辑 `ww_*` 在 `whirlwind_core.c`，主机单测。

| # | claim | 结论 |
| --- | --- | --- |
| AE1 | X = ⌊火力⌋ − 1 永远扣得实 | **CONFIRMED（枚举）** —— 见下「AE1 证据」 |
| AE2 | 不会无限连 | **CONFIRMED（两道）** —— 见下「AE2 证据」 |
| AE3 | 四发不可能同帧 | **CONFIRMED（一手）** —— `do_bomb` `0x420360` 开头查 `BOMB_MGR+0x30`「正在放」，非零直接返回 −1。所以只能一发接一发，这也是设计成「记账 + 接续」两段的原因，不是我们的选择 |
| AE4 | 算 X 的那一刻不会被自己的接续抢跑 | **CONFIRMED（时序，一手）** —— `do_bomb` **先**置 `+0x30` **再**调 `consume_bomb`，而我们的断点在 `consume_bomb` 刚返回处（`0x4203bc`）。所以 `on_bomb_spent` 执行时 `+0x30` 已是 1，同帧的 `on_tick_2` 里 `ww_tick(bombing=1)` 必然返回 0 |
| AE5 | 接出来的那几发不再算 X、不再花火力 | **设计 + 单测** —— `chaining` 位在调 `do_bomb` **之前**置好（`ww_tick` 返回 1 时就已置），而 `on_bomb_spent` 在 `do_bomb` 内部触发，所以它一定看得到。单测 `test_full_chain` 钉住这条顺序 |
| AE6 | 额外那几发不花炸弹数 | **设计（精确补回）** —— 见下「AE6 证据」 |
| AE7 | 引擎拒绝时不会卡死也不会无限重试 | **设计 + 单测** —— `do_bomb` 返回 −1 时欠账不减、下一帧再试；`waited` 攒满 `WW_GIVE_UP`（600 帧 = 10 s）就作废清零。单测 `test_refused_then_give_up` 走完这条路，`test_waited_resets_on_success` 钉住「成功一发就重置计时」（否则长链会被前面攒的帧数误杀）|
| AE8 | 欠账不跨关、不跨局 | **设计** —— `on_load` / `on_stage_start` / `on_run_reset` 都 `ww_clear`。带着「还欠 2 发」进下一关会在开场莫名其妙炸两下 |
| AE9 | replay 安全 | **CONFIRMED** —— 全整数、无随机、无浮点。X 只取决于按 B 那一刻的 `CURRENT_POWER`，而它由确定性的输入重放决定 |
| AE10 | 与腐化（70）的联动 | **设定内，非 bug** —— 腐化在 `on_bomb_spent` 里「退回当前数、扣上限」，接出来的每一发都会各触发一次 ⇒ 一次按 B 烧掉 X+1 点炸弹上限。我们的补回逻辑**不会与它打架**（只补真的少掉的那一下，见 AE6），但上限该扣的照扣 |
| AE11 | 与真银圣剑（75）/ 黄昏（68）的联动 | **设定内，非 bug** —— 真银圣剑对每一发各算一次挥砍（各 400 伤害 + 各一个残机碎片，受它自己每关 2 点耐久的限制）；黄昏在「最后一颗炸弹」条件下也会接一发，两张卡各自接各自的，可能多出一发。都不会崩、也不会无限 |
| AE12 | 决死炸也算 | **设计** —— 决死炸走的是同一个 `do_bomb`，所以同样触发。用户没有排除它，也没有理由排除 |
| AE13 | `repopulate` 的必要性 | **照零售** —— `CardTsukasa__c_press` `0x410e60` 扣完火力**无条件**调 `repopulate`（重建子机 + 广播 `on_power_level_change`）；炎魔 §V 已照做一次，这里第三次照做 |

**AE1 证据**：`spend_power` `0x457480` 的两条边界是「`CURRENT_POWER` ≤ 一档时拒绝、返回 0 且不扣」和「扣完钳到一档」。
X = ⌊P / per⌋ − 1 使得 X > 0 时 `P − X·per ∈ [per, 2·per)` —— 恒在一档以上、两档以下，两条边界都碰不到。
单测 `test_cost_never_refused` 把 `power ∈ [0, 400]` 全枚举，逐个断言「扣完 ≥ 一档」且「扣完 < 两档」（后者是在钉住「X 没算小」）。

**AE2 证据**：第一道是**数值自带的** —— 扣完火力落在 [1.00, 1.99]，⌊·⌋ − 1 = 0，接出来的那几发再算 X 就是 0。
第二道是 `chaining` 位（AE5）。两道任一成立就不会连成串；同时失效才可能出问题，而单测 `test_full_chain` 两条都验。

**AE6 证据**：`do_bomb` 内部的 `consume_bomb` `0x4574d0` 会扣炸弹数（自带钳 0）。这张卡的代价写在火力上，
所以每接一发之后把炸弹数补回 —— 但**只在它真的少了的时候补**（`CE_CURRENT_BOMBS() < before`）、补完钳 `MAX_BOMBS`。
这样与腐化共存不会双重补：腐化自己在 `on_bomb_spent` 里已经把当前数退回去了，我们看到「没少」就不动手（AE10）。

**实跑要看的**：见 [`NEXT.md`](NEXT.md) §1o。崩溃优先怀疑：`on_tick_2` 里 `PLAYER_PTR` 为空时调 `do_bomb`（已判空，黄昏踩过）、
`ce_spend_power` 的 thiscall + `ret 4`（AUDIT §V 已验一次）。行为怀疑：X 算错（看日志里的 `power %d - %d = %d`）、
炸弹数补多了或补少了（看 `bombs %d`）。

## AF. 股市（id 80）—— C 买 / Shift+C 卖，K 线面板（2026-09-08）

**没有新断点、不改任何字节。** 新引擎访问只有一处读：`INPUT_HELD` `0x4ca428`。新 SDK 面：状态块 256 → 512、`on_delete` 回调（`+0x50` 桩在 `ce_state_free` 之前调）。
纯逻辑 `stock_core.c` 主机单测（确定性 / 钳位 / 动量 / 波动率聚集 / K 线 / 买卖 / 来回损耗 / 几何）。
**行情模型 2026-09-08 换成隐藏参数 + 动量**（设计与平衡数据见 spec §3）：随机数改走自带 LCG、只在开局向引擎 RNG 播一次种，
于是带不带这张卡不再影响同一关的弹幕随机（原来每秒要从共用 RNG 取四次）。AF9 的 replay 论证不受影响：种子取自确定时点，之后纯函数推进。

| # | claim | 结论 |
| --- | --- | --- |
| AF1 | replay 安全 | **CONFIRMED（构造）** —— 输入只用 replay 重放的位；随机数 `ce_rand()` = `REPLAY_SAFE_RNG`；步进只在 `on_tick_2`。全整数，见下「AF1 证据」 |
| AF2 | `on_activate` 里读到的低速位与这次 C 键同帧 | **🟡 待实跑** —— `Input__compute_edges` `0x42abc0` 每帧先算好 `INPUT_HELD` / `INPUT_PRESSED`，Player tick（优先级 0x17）里 `c_press` 才被调，所以读的是同一帧的 held。风险只在「按 C 的同一帧才按下 Shift」这种人手做不到的情形 |
| AF3 | 写 `MONEY` 的安全 | **照先例** —— 黄金小丑（§W）/ 倒吊人直接写 `0x4ccd34`，HUD 每帧重读；没有钳位（引擎自己也没有发现上限，SM §1）。买时先查 `>= 50` 再减，卖只加：单测 `test_round_trip` 钉住「现金永不为负」 |
| AF4 | 33 个 UI 层 VM 起得出来 | **🟡 待实跑** —— `ce_anm_spawn_ui` 走 `0x409670`（真银圣剑 / 衔尾蛇各起 1 个已验）；VM 池是 fast 数组（`id & 0x7fff` 下标，`01-vm-instantiate.md`），容量未查。失败返回 0 → 整组删掉、日志一行、下一帧重试；不会半张图 |
| AF5 | `drawRect` + `scale` = 像素尺寸 | **照先例** —— 青眼血条 script87 `drawRect(1,1); scale(56, 4)` 画出 56×4 的条；`ce_anm_set_scale` 写 `vm+0x54`（O29k）。脚本里没有 `scaleTime`，插值器不会盖 |
| AF6 | 矩形锚点是中心 | **🟡 待实跑** —— ANM 默认 anchor 居中（thpages）；`stock_layout` 按中心算 cx/cy。若实跑发现是左上角锚，几何整体偏 (w/2, h/2)，改 `apply_rect` 一处 |
| AF7 | UI 层坐标空间是 2× | **推断（X5）** —— HUD 图标根 VM `pos(1056, 537)`、x 步进 80 对应 640 空间的 (528, 268) / 40 px 图标；`resolutionMode(2)` 同款。面板常量按 2× 写；`ce_hud_text` 是 640 字面量空间（AC6），两套分开 |
| AF8 | VM 生命周期不漏 | **CONFIRMED（构造）** —— 每帧 `vms_alive` 认领，任一没了整组重建；`on_run_reset` / `on_delete` 都 `kill_vms`。见下「AF8 证据」 |
| AF9 | ctor 幂等 | **CONFIRMED（构造）** —— `inited` 位；每关开场引擎再调 ctor 不会重开盘（SDK §9 ★）。不用 `owned[]`：mode 1 装载不调 ctor，状态本来就是零 |
| AF10 | 无敌人 / 对话中不能交易 | **引擎门控** —— `04-active-cards.md` §2 四重门；充能也停。零售主动卡共性，用户已接受 |
| AF11 | 整数不溢出 | **CONFIRMED（单测 + 2026-09-08 复核修正，M3）** —— 交易 / 市值用 int64 中间量；`test_value_pnl` 1 万股 × 9999.00；价格步进 `price × r` 在价格封顶时可达约 999900 × 2549 ≈ 2.55e9，**超过** 2^31，正因如此才必须走 int64。见下「AF11 证据」 |
| AF12 | 状态块 512 的副作用 | **CONFIRMED** —— `s_mem[256][512]` 静态 128 KB，`ce_state_alloc` 的上界检查用同一常量（单测）；`_Static_assert(sizeof(stock_card_t) <= 496)` |
| AF13 | 与其它主动卡共存 | **引擎语义** —— 只有选中的主动卡收到 `c_press`；D 键切卡由引擎管（`0x800`）。面板常驻不随选中变 |
| AF14 | `on_activate` 读到的低速位不是玩家瞬时按键，可能被录制器 OR 过 | **CONFIRMED（一手，2026-09-08 复审 I1）** —— 开着「射击时自动低速」时，按住射击 ＞9 帧后录制器把低速位强行 OR 进 `INPUT_HELD`，与是否真的按了低速无关；对玩家的后果与 replay 安全性见下「AF14 证据」 |

**AF1 证据**：输入只用 replay 重放的位——用卡 `0x400` 走引擎分派、低速 `0x008` 读 `INPUT_HELD`，回放时二者都由 replay 写入（`04-active-cards.md` §1）；
随机数 `ce_rand()` = `REPLAY_SAFE_RNG` `0x4cf288`（零售 Suwako / Yachie / Miko 同源）；股价步进只在 `on_tick_2`（管理器 tick，回放同样跑）。

**AF11 证据（2026-09-08 修正，M3）**：`stock_step` 的 `r` = 阶段漂移（幅度 ≤ 15）+ 阶段波动 × 噪声 / 1000（幅度 ≤ 60）+ 均值回归 `(STOCK_PRICE_OPEN − price) / 400`；
价格封顶 `STOCK_PRICE_MAX = 999900` 时均值回归项 = `(10000 − 999900) / 400 = −2474`（整数截断），三项同号叠加的最坏情形 `r ≈ −(15 + 60 + 2474) = −2549`。
此时 `price × r ≈ 999900 × 2549 ≈ 2.5487e9`，超过 2^31（2,147,483,648）约 4 亿，32 位有符号乘法会溢出。`stock_core.c:54` 把中间量提升成 `int64_t`
正是为了扛住这一步，不是因为乘积本来就塞得进 32 位——此前这里写的「≤ 999900 × 2500 < 2^31」漏算了均值回归项，算式本身是错的，结论（不溢出）不受影响。

**AF14 证据**：`0x462940`（replay 录制路径）里，若供体配置位 `0x200`（"射击时自动低速"）被置位、射击位在 `INPUT_HELD` 里、且射击按住计数器 ＞ 9 帧，
就把低速位 `0x008` 强行 OR 进即将写入的 `INPUT_HELD` 字——不管玩家有没有真的按住低速。后果：开着这个选项的玩家只要在按射击键（STG 里近乎全程都按着），
`on_activate`（`stock.c:121`）读到的低速位永远是 1，C 键会一直走卖出分支；想买入得先松开射击满 10 帧再按 C，体验上是反直觉的。
**Replay 不受影响**：录制侧的 OR 发生在把这一帧的输入字写进 replay 之前，回放路径只是把录制下来的字原样写回、不会再跑一次这段 OR 逻辑——
所以 AF1「输入只用 replay 重放的位」依然成立；这条只是人手操作的手感问题，不是同步性问题。实跑核对见 [`NEXT.md`](NEXT.md) §1p。

**AF8 证据**：底板先起（UI 列表尾插 = 后画在上），再 16 组蜡烛；任一没了整组重建，保证叠放顺序不乱。`on_delete` 是本次新加的 `+0x50` 回调：
先调卡（此时私有状态还活着，能拿到 `bg_id` / `body_id` / `wick_id`）再 `ce_state_free`，倒吊人摧毁（`ce_destroy_card` → `vt+0x50`）与局末 recount 都经过它。

**实跑要看的**：见 [`NEXT.md`](NEXT.md) §1p。

## AG. 卡组编成崩溃（2026-09-08 实跑）—— dev 权重把商店池顶爆 → 门 FAIL → 菜单读到没建出来的卡

**现象**：只挂 `_test` 时，主菜单进「卡组编成」看到空内容，按 Z 崩在 `0x414b7e`（`MOV ESI,[EAX+4]`，EAX = 0）。
只挂 `_255` 一切正常。日志逐字给出了前两环。

| # | claim | 结论 |
| --- | --- | --- |
| AG1 | 触发源是 dev 权重覆盖 | **CONFIRMED（日志）** —— 见下「AG1 证据」 |
| AG2 | 为什么现在才炸 | **CONFIRMED（算术）** —— 股市是第 23 张新卡。22 张时 22 × 25 = 550 ≤ 560，第 23 张顶到 575。与卡 80 的行为无关，它只是压垮容量的那一张 |
| AG3 | FAIL 之后门不再建菜单 | **CONFIRMED（一手）** —— `selfcheck.c` 是直线序列：`ce_cards_load` 失败即 `goto fail`，`ce_menu_setup` 根本不跑；失败处置只有 `restore_alloc_bound` |
| AG4 | 新卡 id 被禁 | **CONFIRMED（一手）** —— 见下「AG4 证据」 |
| AG5 | `_test` 把「空槽」变成「什么都没有」 | **CONFIRMED（一手）** —— 见下「AG5 证据」。**这就是「只有 `_test` 才崩」的原因** |
| AG6 | 空指针的确切来源 | **CONFIRMED（一手）** —— 见下「AG6 证据」 |
| AG7 | 顺序表尾界被撑到 255 是否有份 | **REFUTED** —— `emit_codecaves` 把 57..254 填成 id 57（BACK），`0x4149de` 查表读 `row+0x20`（`deck_visible`）为 0，在 `0x4149e1` 全部跳过。EDX 仍只是零售可见且已解锁的数量。尾界无害 |
| AG8 | 菜单搬迁有没有漏站点 | **CONFIRMED 无遗漏** —— 见下「AG8 证据」 |

**AG1 证据**：`cards_dev: … retail_weight=6 new_weight=20 (71 rows changed)` 后紧跟
`FAIL: cards: shop random pool 575 slots > 560`。`retail_weight 6` 把 55 张零售全踢出随机池，
池子只剩新卡：23 × (20 + 5) = 575 > `CE_SHOP_POOL_SLOTS` 560。

**AG4 证据**：`restore_alloc_bound` 把 `0x411479` 写回 `cmp ebx,0x38`，于是 id ≥ 57 在 `0x411482`
的 `jmp [0x412dac+ebx*4]` 落到 default，`allocate_new_card` 返回 `0xffffffff`：不建对象、不入链、
`mgr+0x28`（`num_total_cards`）不涨。

**AG5 证据**：`BP_ce_test_deck`（`0x407ee3`，在 `AbilityManager__reset_cards` 里）把**每个**空槽换成
`start_deck` 的下一个 id。不挂 `_test` 时空槽保持 id 56，走 `case 0x38` 分配出 `AbilityCardNullInf`，
`num_total_cards > 0`；挂上之后这些槽换成 71/70/68/72/80，全部分配失败 ⇒ `num_total_cards` 归零。

**AG6 证据**：菜单对象 `+0xfec` 是「玩家手上卡对象」的指针数组（256 槽），**只由** `0x415af0`
沿卡链表按 `mgr+0x28` 次填；`+0xe4` 是它的游标。而 `0x414b6d` 处 `TEST EDX,EDX` 的 EDX 是**另一张表**
（挑选列表 `__card_ids`，由顺序表遍历 `0x4149b0..0x414b59` 得出）的长度。两者由不同的 pass 产生、
互不校核，所以挑选列表非空、手牌数组却一格没填时，`[EDI+EAX*4+0xfec]` 读到 memset 留下的 0。

**AG8 证据**：全 `.text` 扫 imm32 `0x4b3600` / `0x4b36e4` 与 disp32 `+0x304`：6 处顺序表引用 +
尾界 `0x414b55` + 16 处 `__card_ids` 全部在册；另一处 `0x4b36e4` 在 `0x4337f7`，是无关常量。
抬到 255 的清理循环（`0x41495c`，ESI = this+`0xbec`）到 `+0xfe4` 为止，碰不到 `+0xfec`。

**两道修复**（2026-09-08）：

1. **让 dev 配置压不爆容量**（根因）：`ce_weight_override_fit`（`cards_def.c`，主机单测 `test_weight_override_fit`）把 `new_weight` 逐级下压到刚好过 `ce_shop_capacity_check` 为止，下限 1（0 会变必出资源卡、6 会退出随机池，都不是「少出现一点」），压不下去才原样返回让门照常 FAIL。日志 `cards_dev: new_weight 20 would overflow the shop pool (> 560 slots) — clamped to 19`。
   `cards_dev.js` 同时把 `new_weight` 从 20 调到 15（23 张 = 460/560，留出余量）。
2. **FAIL 之后别再塞注定失败的 id**（防线）：`selfcheck.c` 的 `fail:` 分支先调 `ce_dev_deck_disable()` 再 `restore_alloc_bound`。空槽于是恢复成 id 56 → `NullInf` 分配得出来 → `num_total_cards > 0`，菜单不再读到没填过的槽。

**遗留（不修，记着）**：`+0xfec` 与 EDX 由两个 pass 各自产生、彼此不校核，是零售自带的脆弱处；零售里 `num_total_cards` 不可能为 0（空槽总能分配 NullInf），所以踩不到。任何让手牌数为 0 的改动都会重新暴露它。

## AH. 商店随机池搬进 codecave（2026-09-08）—— 560 份的天花板

**为什么有 560 这个数**：`CardShop__pick_weighted_random_offer` `0x416f50` 开头 `sub esp, 0x8d4`，
其中 `0x8c0` 字节是「份数池」，基址 `ebp-0x8c4`，即 **560 个 dword**，每格放一张卡的表行指针。
填池的两处写（`0x417100` 的 `rep stosd` 压 weight 份、`0x41712f` 起五条 mov 压「没拿过」的 +5 份）
**全程没有越界检查**，计数器 `edi` 一路累加。而这块缓冲的末尾正好顶到 `ebp-0x4` 的栈金丝雀，
再往上是保存的 ebp 与返回地址 —— 所以超限不是「多的被丢掉」，是踩烂返回地址，`0x48dc60` 的
`__security_check_cookie` 把进程打死。

**为什么现在动它**：零售 362 份 + 23 张新卡（权重 2）161 份 = **523 / 560**，正常游玩下再加 5 张就撞顶。

| # | claim | 结论 |
| --- | --- | --- |
| AH1 | 560 = `0x8c0 / 4`，且缓冲紧贴金丝雀 | **CONFIRMED（一手）** —— 见上，`0x416f53` / `0x416f60` / `0x416f6e` 三条 |
| AH2 | 填池无越界检查 | **CONFIRMED（一手）** —— 整段只有 `cmp ecx, 0xd64`（卡 id 循环上界），没有任何针对 `edi` 的比较 |
| AH3 | 9 处站点就是全部 | **CONFIRMED（一手）** —— 位移 `0xfffff73c`（= −0x8c4）及其 +4/+8/+0xc/+0x10 变体在本函数内的全部出现：两处 `lea`、五条 `mov` 写、一条 `mov` 读，加 memset 的大小立即数 |
| AH4 | 改写全部原地等长 | **CONFIRMED（编码）** —— 见下「AH4 证据」 |
| AH5 | 栈帧一个字节不动 | **设计** —— `sub esp, 0x8d4` 与金丝雀保持原样；搬走之后那 0x8c0 只是没人用的栈空间。不动栈 = 不碰金丝雀、不碰任何其它局部的位移 |
| AH6 | 全有或全无 | **设计** —— `check_pool`（`selfcheck.c`）在门里逐条按 `sites_gen.h` 的模板重算应有字节再比对，任何一处不符即 FAIL。半搬半留比不搬更危险：读走 cave 而写还在栈上（或反之）都会读到空池 |
| AH7 | 池上限两处常量不许漂 | **CONFIRMED（编译期）** —— `cards.c` 里 `_Static_assert(CE_SHOP_POOL_SLOTS == CE_POOL_SLOTS)`，前者给主机单测用、后者由 `sites.py` 生成 |
| AH8 | 与容量检查的关系 | 上限从 560 提到 **8192**。`ce_shop_capacity_check` 与 `ce_weight_override_fit` 的单测改成按常量表达（`CE_SHOP_POOL_SLOTS / N − 5`），不再写死边界 |

**AH4 证据**：x86 的 SIB 编码里「mod 00 + base 101」= 纯 disp32、「mod 10 + base 101」= ebp + disp32，**两者同长**。
所以五条写 `89 94 BD disp32` → `89 14 BD disp32`、一条读 `8B 8C 95 disp32` → `8B 0C 95 disp32`，都是 7 → 7 字节，
只改 modrm 的 mod 两位。两处 `lea eax,[ebp-0x8c4]`（6 字节）→ `mov eax, imm32`（5）+ 一个 `nop`。
memset 的 `push 0x8c0` → `push 0x8000`，长度不变。**没有一条指令需要挪位置，也没有一条跨越指令边界。**

## AI. HUD 的坐标与绘制顺序（2026-09-08 一手）

反 `zGui__on_draw` `0x43cd00`（`0x43d710` 是它的一行包装）得到的字面量坐标表 —— 全在 640×480 空间，
不用做分辨率换算（同 AC6）。股市面板挪位置、持仓净值贴金钱都按这张表来。

| 元素 | 坐标 | 对齐 / 字号 | 颜色（B, G, R）|
| --- | --- | --- | --- |
| 总分 | `(620, 42)` | 右对齐，font 10 group 3 | 0x70,0x70,0x70 |
| 最高分 | `(620, 64)` | 同上 | 0x80,0x10,0x00 |
| 残机 / 炸弹行 | `(…, 120)` | scale 0.6 | 0,0,0 |
| 符卡计时 | `(224, 144/150/160/166)` | font 4 group 2 | 随 VM |
| `%3d` `/%d` | `(576, 158)` `(597, 158)` | scale 0.6 | — |
| 火力 | `(540, 182)` `.%2d (560, 189)` scale 0.6 `/%d. (574, 182)` `00 (606, 189)` | | 0,0,0x80 |
| **金钱** | `(620, 204)` | **右对齐** | **0,0x80,0x80**（= `CE_HUD_MONEY_COLOR` `0xff808000`）|

**★ 绘制顺序：HUD 文字永远在 UI 层图元之前。** `Gui__operator_new` `0x43bb4e` 把 `0x43d710`
注册在优先级 **`0x31`**；而 UI 列表的 ANM 层最早是层 24 的桩、优先级 **`0x3c`**
（`engine/anm/th18/02-render-stages.md` §1，UI 列表层 24–32 折叠到 37–45 那组桩）。
两者之间没有任何 UI 层能插进去，**所以「让面板垫在自己的读数底下」这件事做不到** ——
无论把脚本的 `layer()` 调到多少。唯一的解法是让文字与面板**不重叠**。
（世界列表的层 20–23 优先级 `0x2e`–`0x38` 确实更早，但它们走相机 1、视口是弹幕区，
画不到侧栏上，对 HUD 没用。）

**AsciiManager 的排版字段**（Ghidra 结构体 `zAsciiManager`，一手）：字色 `+0x19228`、副色 `+0x1922c`、
缩放 `+0x19230`、**字体 `+0x19244`**、**分组 `+0x19248`**、水平对齐 `+0x19250`、垂直对齐 `+0x19254`。
金钱那行用的是 **font 10 / group 3 / 右对齐**，比默认的 font 0 大一圈 —— 想让自己的数字看着和金钱是一套就照抄这三个
（`ce_hud_text_font`，画完逐项还原；不还原会把引擎后续的绘制带偏）。

**对齐模式 `+0x19250`**：**2 = 右对齐**（金钱 `(620, 204)` 与总分 `(620, 42)` 一手确认）；
**1 是引擎的常态值**（`FUN_0043cd00` 收尾时设的），实测表现为**左对齐**；于是 **0 只能是居中** —— 这一条是推的，实跑再确认。

**★ 想把数字画在自己的底板之上，只能用 ANM 图元。** HUD 文字的绘制优先级（Gui `0x31`、
AbilityManager `0x33`）永远早于任何 UI 层图元（≥ `0x3c`），所以 `ce_hud_text` 系列压不住自己的面板。
`ce_hud_number_*`（`sdk.c`）就是为这个加的：一位一个 VM，脚本 `HUD_NUMBER`（script105，`pos(0,0)`），
坐标走 UI 的 2× 空间，可右对齐。真银圣剑 / 衔尾蛇那个单位数字的 `ce_hud_digit_*` 是它的前身。

**股市面板的落位（2026-09-08 改）**：从侧栏下部（压着卡组图标）挪到**右上角**，
底板 640 空间 `(528, 50)` 尺寸 212×68 —— **顶边落在 y = 16，与弹幕区上边框对齐**（相机 0 视口 `(32, 16, 384, 448)`），盖掉总分与最高分——本 mod 里得分没有意义。
底板 alpha 从 160 提到 220，否则底下的分数会透出来。行情读数放在面板底（84）与残机行（120）
之间那条空隙（y = 96），持仓净值移到**金钱正下方**（`(620, 218)` 右对齐，与金钱同一个右边界、同款字体与字色 ——
原先贴在金钱左边会撞上「金」的图标，且默认字体偏小），
两者都不与面板重叠，于是不受上面那条顺序约束。

## AJ. 倒吊人删不掉「当前选中的主动卡」（2026-09-09 实跑修复）

**现象**：卡组是 { 旋风斩 79, 股市 80 }。第一次买倒吊人摧毁了 79，**第二次买什么也没发生** ——
日志 `hanged: no other card to destroy, nothing gained`，钱照扣。

**根因**：`ce_pick_random_card` 把 `mgr+0x38`（当前选中的主动卡）排除在候选之外，而 `ce_destroy_card`
也一律拒绝删它。删掉 79 之后卡组只剩股市，它是主动卡且正被选中 ⇒ 候选集为空。
两处保护都是为了同一件事：删掉之后 `mgr+0x38` 会变成悬垂指针。

**修法照零售自己的收尾**（`AbilityManager__recount_and_recategorize_cards` `0x4080e0` 末段）：
删之前把 `mgr+0x38` 摘空，删完调 `AbilityManager__set_selected_active_card` `0x408b00` 传 −1 重选。

| # | claim | 结论 |
| --- | --- | --- |
| AJ1 | `0x408b00` 能从「选中为空」恢复 | **CONFIRMED（一手）** —— 见下「AJ1 证据」 |
| AJ2 | 它也负责刷 HUD | **CONFIRMED（一手）** —— 命中时调 `update_selected_active_card_hud`，与 D 键切卡走的是同一条路（`0x45c0b0` 就是这么调的）|
| AJ3 | 没有主动卡时的行为 | **CONFIRMED** —— 返回 0、`mgr+0x38` 保持为空。与零售 recount 在同样情形下的结果一致 |
| AJ4 | 顺序不能反 | **设计** —— 必须在调虚表 `+0x50` 释放对象**之前**把 `mgr+0x38` 摘空；留到之后就是悬垂指针 |
| AJ5 | 不能改调 `recount` 了事 | **CONFIRMED（一手）** —— `0x4080e0` 的主循环会把所有 `flags & 1 == 0`（非存档装载）的卡**全部删掉**，那是局末清理，局中调等于清空卡组 |

**AJ1 证据**：`0x408b00` 在 `delta == -1` 且 `this+0x38 == 0` 时，从链表**尾部**起步（沿 `+0x1c` 走到底），
然后按 `prev` 回溯最多 `num_total` 次，找第一张 `flags & 8`（主动）的卡，写 `this+0x38` 并刷 HUD，返回 1。
`this+0x38` 非空时则从它的结点开始回溯 —— 正是 D 键「切到上一张」的语义。

## AK. 绀珠传（id 115）—— 章节系统 + 擦弹减速（2026-09-09）

**结论：零新断点、零字节改动。** 全卡只做四件事：读三个字段、写一个 float。

| # | 面 | 一手依据 | 结论 |
| --- | --- | --- | --- |
| AK1 | 读 `GRAZE` `0x4ccd0c` | `Player__do_graze` `0x45e160` 每擦弹 +1、封顶 99999999；`GameThread__thread_start` 每 run 归零 | ✅ 差分累计本波擦弹 |
| AK2 | 读符卡位 `SPELLCARD_PTR(0x4cf2c0)+0x78` bit0 | `FUN_00409b10` 就这一行；收卡 / 超时 / 中断都清位 | ✅ 下降沿 = 波边界 |
| AK3 | 读 `ENEMY_MGR_PTR(0x4cf2d0)+0x198` | ExpHP `enemy_count_real`；`C 键门控 0x45c07b` 也读它 | 🟡 击破率是近似（离场也算「消失」） |
| AK4 | 写 `ITEM_MANAGER_PTR(0x4cf2ec)+0xe6bb14` float = 0.3 | ExpHP `lolk_item_slowdown_factor`；物理 `0x445bf4`–`0x445c7d`；恢复 `0x446807`–`0x446821`（+0.1/帧） | ✅ 见下 |

### AK4b. 擦弹判定框（2026-09-09 二次定案）

| # | 面 | 一手依据 | 结论 |
| --- | --- | --- | --- |
| AK4b1a | **矩形**判定 = 弹中心 vs 自机判定框外扩 **24.0** | TH18 `0x45cc20`（`0x4b92a4`=24.0）；TH15 `th15:0x455be0`（`th15:0x4cffa0`=24.0） | 两作相同；半宽高 `player+0x479a4/0x479a8`（1.5/1.5） |
| AK4b1b | **圆形**判定 = `d² < (r + max(br/2.5, 40.0))² + br²` | TH18 `0x45cd90`（`0x4b921c`=2.5、`0x4b92c4`=40.0）；TH15 `th15:0x455d00`（`th15:0x4cff28`=2.5、`th15:0x4cffb8`=40.0） | **「额外圈」= 这个 40 px 边距**（圆弹有效擦弹半径 ≈ 43–50 px）；`r` = `player+0x4799c/0x479a0`（3.0/3.0） |
| AK4b1c | 子弹按 `flags` 选模式 | 分派器 `0x4248a0`：`& 0x2` 才碰、`& 0x10` 走圆、`& 0x40` 按 `bullet+0xf38` 缩放半径 | 本卡两种模式都照原样判 |
| AK4b2 | TH18 **每颗弹一生只擦一次** | `0x42496f test [esi+0x20],4` → 跳过；`0x424984 or [esi+0x20],4` 置位 | 「靠擦弹计数差分驱动减速」只在子弹**刚进框**那一帧生效 → 观感「只在擦弹那一刻」 |
| AK4b3 | 改成**每帧扫弹池判框** | 池 `BULLET_MGR(0x4cf2bc)+0xec`、2000 槽、stride `0xfa0`、状态 `+0xf68`（0 空 / 3 消弹中跳过）、位置 `+0x638` | 纯读 + 一个写；每帧 2000 次读 ≈ 可忽略；时机固定 → replay 安全 |

### AK4d. 擦弹圈（VM + 零售美术）

- **美术来源**：TH15 `effect.anm` 的 `eff_sloweffect.png` 右半（空心环，脚本 25/26/27 用它），
  `assets/ability/make_lolk_ring_art.py` 抽成 128×128 的 `ability/lolk/RING.png`
  （零售字节只在 `local/`，构建时取，不入库）。
- **脚本**：`assets/ability/scripts/106_lolk_ring.anm.txt` —— TH15 `script27` 1:1 复刻：
  `blendMode(2)` = **反向减算** `dst − src×alpha`（`th15:0x47d840` case 2：SRCBLEND=SRCALPHA、
  DESTBLEND=ONE、BLENDOP=REVSUBTRACT），30 帧淡入（0→96）+ 12 帧由大变小（4.3→2.0）+ 慢自转；
  被打断后 12 帧渐隐自删。**缩放取 TH15 原值的一半**（4.3→2.0 改成 2.15→1.0）：贴图 128×128 的亮环半径
  44 px，scale 1.0 才与判定圈（≈43 px）重合（实跑纠偏）。生成 `CE_ANM_ABILITY_SCRIPT_LOLK_RING` = 106、sprite 138。
- **接线**：`lolk.c` 的 `ring_sync()` —— 判定区有弹 → `ce_anm_spawn` + 每帧 `ce_anm_set_pos`；
  弹离开 → `ce_anm_interrupt(id, 1)`（渐隐交给脚本）；过关 / 局末 / `on_delete` 删 VM。
  **不加断点、不改字节。**

### AK4d2. 波次面板（2026-09-09 实跑补）

- **美术**：TH15 `front.anm` script241 的 `front02.png` **六条**（244/245/246/247/248 = sprite145/146/147/149/148
  在 y=0/50/90/130/170、中间三条 x 偏 −64；**242 = sprite150 = B 碎片标签**在 (0,240)）
  → `make_lolk_ring_art.py` **各切一条**，存 **256×32、内容居中 192**
  （贴图尺寸必须是 2 的幂；sprite 是中心锚，内容偏左 = 整条推左 32 px，2026-09-09 实机「偏左」的真因）
  → sprite 139..144、脚本 107..112（UI 列表 / layer 26 / resolutionMode 2）。
- **接线**：`panel_show()` 六条各起一个 UI VM，按 TH15 的绝对偏移摆
  （`(448,402) + (0,0)/(−64,50)/(−64,90)/(−64,130)/(−64,170)/(0,240)`）；碎片标签只在
  `frag_type != 0` 时起（TH15 里它被起出来但默认禁用，`bonus ≥ 1e6` 才留）。
- **文字**：五行右对齐（`align_h = 2`）、x = 300/**308**/300/300/300、y = 226/246/266/286/**296**、
  font 2 / group 2 —— 逐条照 `th15:0x436a6f`；格式 `"%d"` / `"%3.1f%%"` / `"%3d"` / `"%6d"` / `"  +%d"`，
  第 5 行色 `0xffff8080`（TH15 的 `0xff8080ff` 换序）。第 3 行 TH15 是 Pointdevice 重开数，本卡改印本波击破数。
- **实跑修的两个洞**（2026-09-09）：① 主菜单出现 `+0` —— `last_age` 初值 0 被当成「刚结算」，
  改成 `waves_total > 0` 才画；② 主菜单不该起面板 —— `on_draw` 开头加 `GAME_THREAD_PTR` 非 0 的门。

### AK4d3. 擦弹染色（进圈才黄、出圈还原）

- **判据**：本卡自己的擦弹判定圈（AK4b1a/b），**不是** TH18 的「已擦」位 —— 用户口径：
  弹进圈染黄、飞出圈还原原色（TH15 那边是 per-bullet 计时器 <45 帧内写色，见
  `engine/player/th15/01` §4d）。
- **颜色**：`0xffffd080`（BGRA：A=ff R=ff G=d0 B=80）= TH15 的 `th15:0x419d3d` 实际最亮档。
  TH15 的 G 是 `min((0x68−计时器)×2, 0xf0)` 且只在计时器 <0x2d(45) 时写 → **G 只落在 118..208**，
  那个 0xf0 上限够不着（用户「记忆里偏红」正确）。
- **落地**：`tint_restore_all()` + `tint_mark()`，**染色表放在卡外静态数组**，每帧开头还原上一帧染过的、
  再重染本帧在圈里的。2026-09-10 两处加固：表 **64 → 256** 项（密弹幕里圈内弹可能几十上百颗）；
  还原时**先核对颜色**（`*p == LOLK_GRAZED_COLOR` 才写回原色）—— 弹池是环形复用，槽被回收给新弹时
  无条件还原会把新弹染错（TH15 原生是 per-bullet 计时器，没有这个问题）。
- ⚠️ **教训（2026-09-09 实跑故障）**：染色表第一版塞进了 `ce_state` 私有状态 → 结构体涨到 ~1KB >
  上限 496 → `ce_state()` 返回 NULL → **每个回调第一行就退，卡看着 bound 了却什么也不做**。
  现已加 `_Static_assert(sizeof(lolk_card_t) <= 496)` 编译期拦。

### AK4c. 波次语义与击破率（2026-09-09 三次定案）

**波边界三类信号**（全部只读）：

| 信号 | 字段 | 用途 |
| --- | --- | --- |
| boss 出现 / 消失 | `ENEMY_MGR+0x48` boss_ids（`ce_enemy_by_id` 查活体）| 进 boss = 新波；消失 = 结算并关波 |
| **相位指纹** | `zEnemy+0x6374` 中断槽数组（stride 0x88）里**活动槽**的 `(index, hp_value, time, sub_life[0..3])` | boss 战内部：换相位 = 换波 |
| 章号 | `GLOBALS.inner+0x24` = `0x4ccce4`（ECL 524 写）| 道中切章 = 换波 |

指纹**只在有活动槽时更新**（槽被清空那帧 `hp_value = -1` 不算边界）→「清空 → 新槽」只报一次；
换波后 3 帧去抖（`LOLK_DEBOUNCE`，槽清空与符卡位翻转会连报两下）。

**击破率三来源**（用户定案）：

| 波种 | 率 | 判据（TH18 引擎自己的判定，见 `engine/chapter/th18/02`）|
| --- | --- | --- |
| 道中章 | 击破数 / 刷出总数（分母 0 → 100％）| `enemy_count_real` 差分 |
| boss 非符 | 击破 100％ / 超时 0％ | 本帧 `life.current(zEnemy+0x6220) <= 槽.hp_value`（击破）/ `槽.time <= time_in_ecl(zEnemy+0x14ec)`（超时）；引擎持续位 `zEnemy+0x635c` bit24 的上升沿也算超时 |
| boss 符卡 | 同上 | 另可读 `SPELLCARD+0x78` bit7（已超时）|
| **时符** | **恒 100％** | `SPELLCARD+0x78` bit3（ECL 542 → `0x42d650` 置）|

「本波见过」= 波开着期间采到一次即锁定；**边界帧也采**（击破/超时就发生在切换那一帧），
但符卡标记属于**新波**（否则非符波会被贴上符卡标签）。

实现：`lolk_core.c`（主机单测 91 组：三类边界 + 三条率口径 + 时符 + 空波 + 碎片 + 减速曲线）。

### AK5. 为什么这个写入面是安全的

- **字段是引擎自己造的、也自己在用**：`0x445c0d` / `0x445c6d` 两处读它乘位移与重力增量，
  `0x42ab27` 重置写 1.0、`0x446821` 每帧 +0.1 恢复。我们只是把 TH15 那个被删掉的写入补回来。
- **全 exe 没有任何别处写它**（`e6bb14` 共 6 处引用：1 写 1.0、3 读、恢复 1 读 1 写；
  字面量 `0x3e99999a` 全 exe 两处都不在 ItemManager 上）→ 不会与别的代码打架。
- **只影响飘落态**（state 1）：上抛 / 吸引中的道具不乘这个因子，不会把已收的道具卡住。
- **写指针前判空**（`ITEM_MANAGER_PTR` 为 0 时直接返回）。
- **只写 0.3、不写别的值**：恢复交给引擎，我们不去改 `0x4b90d8` / `0x4b9178` 那两个常量
  （改了是全局副作用，影响所有减速来源）。

### AK6. replay 确定性

写入时机 = 「擦弹计数上涨的那一帧」，完全由输入（移动/擦弹）决定；
道具物理本身不用 RNG（同函数里的随机只用于擦弹火花抖动，且走 `REPLAY_UNSAFE_RNG`）。
所以**录制与回放同步**。反面照旧：用本 mod 录的 replay 换回原版必然失同步
（道具轨迹变了），这是所有改玩法 mod 的通病，不是本卡特有。

### AK7. 已知偏差（写进 spec §3.2 / §6）

- 道中的分子分母用 `enemy_count_real` 差分近似：离场（未击破）也算「消失」
  ——TH15 的 `DESTROYED` 也是在敌人被移除时加的（`th15:0x428b70`），所以这一条与 TH15 同义；
  分母仍可能漏掉「一帧内刷出又死」的敌人；
- boss 相位的击破/超时靠**每帧采样**：如果本卡的 tick 恰好排在敌机更新之后，切换那一帧的条件读不到，
  退化到「引擎持续位上升沿」；**两个连续相位都超时**是唯一会漏的情形（下一个相位起点位已是 1，没有上升沿）；
- 相位指纹是本仓手法：若某个相位不重新登记中断槽（`ins_514` 没调），那一波不会切出来；
- TH15 的 PIV 增益、擦弹浮字、Pointdevice 快照都**不做**（TH18 没有对应物或语义不同）。

### AK8. 实跑要确认的四件事

1. 擦弹时道具真的变慢、松手约 0.12 s 恢复（若不是：`0xe6bb14` 偏移或 ITEM_MANAGER 指针错）；
2. **每一段攻击各结一次**（`boss nonspell` / `boss spell` 交替出现在日志里），道中按章各结一次；
3. **击破率**：打空 → 100％、超时 → 0％、时符 → 恒 100％（日志里的 `= R%`）；
4. 面板六条位置与五行文字对齐（坐标与锚点见 `NEXT.md` §1t 的「要调的地方」）。

## AL. 鬼形兽（id 117）—— TH17 动物灵 / 咆哮的卡侧复刻（2026-09-10，Stage 2）

一手语义：[`engine/animal/th17/01`](../../../engine/animal/th17/01-animal-spirits.md)（本次会话又补反编译了
`0x410af0` token tick / `0x40f980` 收集 / `0x410640` 鱼灵换道具 / `0x4491b0` 真死）。
TH18 无残骸：[`engine/animal/th18/01`](../../../engine/animal/th18/01-animal-residuals.md)。
**零新断点、零 binhack、零 codecave**；新增一个引擎地址 `th18:0x45d590`，资源侧纯追加 `ability.anm`：
13 个 token 单元（`beast1.png`）+ 24 个咆哮光环单元（`beastAURA.png`）+ 1 个消弹体（`beastot.png`）+
槽框（`beastf.png`）+ 脚本 137–149。

| # | claim | 结论 |
| --- | --- | --- |
| AL1 | 动物灵实体可以用 `ability.anm` 的 world VM 池自建 | **CONFIRMED** —— 40 只池（TH17 上限 0x28），`ce_anm_spawn` / `set_pos` / `set_sprite` / `interrupt` / `delete` 都是 SDK 已有原语；贴图走零售切图（`beast1.png` 13 个 32×32） |
| AL2 | `+0x10` 槽里调 `cancel_impending_death` 能救命 | **CONFIRMED（一手）** —— `Player__die` `0x45d3a0` 先置 state 4、**最后**才广播 `+0x10`，返回后不再写 state（3 个调用点 `call` 后直接 `ret`）⇒ 槽里调 `0x45d590`（state→1）不会被覆盖；返回值被忽略，救命靠副作用 |
| AL3 | 主动卡 `active_recharge=1` + `on_activate` 返回 0 的语义 | **CONFIRMED（SDK 契约）** —— 返回 0 = 瞬发、进 8 帧收尾；霊撃 40 帧由 `on_tick_2` 自己的 `hypering` 推进（中弹取消路径不走 C 键状态机） |
| AL4 | 鱼灵换道具用原生 `ce_spawn_item` | **CONFIRMED（照 TH17 `0x410640`）** —— 4→item6、5→item4、6→10×item1、7→10×item2；`ce_spawn_item` 的 8 参 ABI 已由辉针城 / 季节卡坐实 |
| AL5 | replay 安全 | **CONFIRMED** —— 出场角度 / 速度、速度下限重掷走 `ce_rand()`（replay 安全 RNG）；掉落节奏是确定性计数（每 3 杂兵 + boss 各 1 动物 + 1 鱼）；无自己的随机源 |
| AL6 | 不引入 x87 | **CONFIRMED** —— `make dllx87` 零 x87（`animal_hyper_radius` 用出参） |

### AL7. 自然掉落的 TH17 对齐（`BossItem` 全表，2026-09-10）

一手：`thecl -d 17 default.ecl` 的 `BossItem` / `BossItemPhase*` 子程序 + 全 stage ECL 的调用普查
（`grep '@BossItem' /tmp/th17ecl/*.txt`）。**杂兵只有 item 1/2/3，没有动物灵** —— 这是之前「每 3 个
杂兵掉一颗」被删掉的原因。

| TH17 调用 | 次数 | item | token | 卡侧对齐 |
| --- | --- | --- | --- | --- |
| `@BossItem(33,0,0)` | 39 | 33 → `rand%3+30` | 15..17（随机变色动物灵）| boss 相位 / 符卡边界，87% |
| `@BossItem(30/32,0,0)` | 10 | 30 / 32 | 15 / 17（指定狼 / 鹫）| 同上，合并成随机 |
| `@BossItem(22,0,0)` | 7 | 22 | 7（点道具鱼灵）| 同上，13% |
| `@BossItem(19,…)` / `(20,…)` | 21 / 15 | 19 / 20 | 4 / 5（炸弹 / 残机碎片灵）| boss / 中 boss 死亡，3:2 |
| `dropMain(23..29)` | 各 1 | 23..29 | 8..14（稀有动物灵）| **不做**（道中 boss 的条件分支，TH18 无对应信号）|
| `dropMain(16+$A)` | 10 | 16..18 | 1..3（**不变色**）| **不做**（只 st06 Keiki 的特定符卡；无法定位）|
| `dropMain(1/2/3)` | 52+ | 1/2/3 | — | 引擎自己撒，不关本卡 |

信号：boss 活动中断槽（`zEnemy+0x6374` 起 8 槽，stride 0x88）的 `phase_sig` 变化 —— 与绀珠传卡
（115）的 `phase_read` 同一条规则（`engine/chapter/th18/02-wave-signals.md`），槽名首字母 `'M'`
= 道中 boss（相位边界不掉）。

### AL8. Stage 3 边界（❓自定 / 待实装，不得当「复刻」宣称）

1. ~~暴走咆哮的射击强化是**标准版**（自机弹染色 + 狼/鹫 ×1.5 伤害）~~ → **已换成逐发复刻**，见 §AL11
   （自机弹染色仍在；`ce_set_bullet_damage_permille` 的替身已摘掉）；
2. 稀有动物灵（8..14）**不做**；「+480 凭依加成」**2026-09-10 二次定案已接**，见 §AL10；
3. 灵槽（x=281+36i / y=900，间距照抄）与光环几何是从 TH17 `pos(-360+36i, 872)` / `pos(0,-20)` 换算的，待实跑调；
   多张左下角卡暂时**允许重叠**，最终要竖直堆叠 / 右下角（用户口径，见 `SDK.md` §17）；
4. 就绪条（TH17 `beast.anm` script0x1e/0x1f）**已接**，见 §AL10 —— 之前这里记的「围着原版 15 子 VM 槽画」
   是错读：一手反汇编（`th17:0x40f0c6` 段）显示这两条 VM 的坐标每帧被写成**自机坐标 ×2**，是贴着自机的。
   霊撃三色光圈（`beastr.png` script0x21）已接。

### AL9. 实跑要确认的六件事

1. 杀敌掉动物灵 / 鱼灵，DVD 式弹跳、60 px 内减速、24 px 吸收；
2. 长寿命灵远离时变色（狼→獭→鹫）、靠近冻结；对话中不消失；
3. 满 5 → `hyper ready (type … frames …)`；3 同类 → `rush` + 840/1140/1500，混合 → 1140；
4. 按 C → 消弹 + 无敌 + 伤害；咆哮中收动物灵 → `hyper extended`；收鱼灵 → 立刻出道具；
5. 就绪时中弹 → `hyper save #N` 且不扣命；真 Miss → 2 个不变色灵；
6. 满槽不发动 → 超时后 `hyper end … (normal; … animals released)` + 2 个变色灵。

### AL10. 咆哮「就绪窗口条」（TH17 `beast.anm` script0x1e/0x1f = 30/31）—— 2026-09-10

**发现**：`AnmManager` 里两条 VM 的坐标每帧被写。**推测**：是就绪倒计时条。**验证**（一手反汇编 + 脚本原文）：

| 事实 | 证据 |
| --- | --- |
| 两条 VM 由 `AnimalManager__on_ready` `th17:0x40fb60` 建（`th17:0x40fc6a` → 脚本 `0x1e`、`th17:0x40fc74` → `0x1f`），存 `manager+0x28` / `+0x2c` | 反汇编 + `FUN_0040fb60` 段；建之前 `0x4b5aa8:=1140`、`0x4b5aac:=1140.0f`（= 本次就绪窗口帧数） |
| 每帧坐标 = **自机坐标 ×2**（定点取整），共 4 条 VM（`+0x28/+0x2c/+0x30/+0x38`）一起刷 | `th17:0x40ee70` 段：`esi=[0x4b77d0]`（Player），取 `[esi+0x610]`/`[esi+0x618]` × `0x4a3c18`（= **2.0f**）→ 写 `vm+0x5e4`/`vm+0x5ec` |
| 就绪窗口结束时**清掉** | `GoastManager::do_hyper_cancel` `th17:0x40f6d0` 起手清 0x1e/0x1f/0x20/type+0x28 |
| script30 = 空心外框：`drawRectBorder(1.0f, 10.0f)` + `color(192,192,192)` + alpha 0→255 淡入 20 帧 | `local/th17.v1.00b/anm/beast/beast.anm.txt` script30 原文 |
| script31 = 填充：`drawRectGrad(1.0f, 8.0f)` + `colorMode(1)`，`color`/`color2` 每 6 帧 **(224,0,0) ↔ (255,192,128)**，`jmp` 回环 20 次后定格 | 同上 script31 原文（`offset224:` … `jmp(offset224, 20)`） |
| 两条都 `anchor(0,2)`（锚 x 中心、y 底边）⇒ 长度缩短时**两端一起朝中间收** | script30/31 原文 |
| **长度**（每帧刷）：`FUN_00410180` `th17:0x410180` 把 `vm+0x70` 写成「时长 × 0.125 + 2」（外框）/「剩余 × 0.125」（填充），详见下面的「长度公式」段 | 一手反汇编 `th17:0x410180`–`0x4101f0` + 常量直读 |
| **初始时长随情况变**：多数类 == 自机凭依（`0x4b59f8`）→ `+0x1e0` = **+480 帧**（混合类那条路直接 1140 不加） | `th17:0x40fbe5` `cmp esi,[0x4b59f8]` / `th17:0x40fbed` `add eax,0x1e0` |

**长度公式（一手）**：`th17:0x410180` 每帧取 `manager+0x28`（外框）与 `+0x2c`（填充）两条 VM，
先 `vm+0x530 |= 8`，再写 `vm+0x70`：
`外框 = HYPER_DURATION(0x4b5ab8，i32) × 0.125(th17:0x4a3af4) + 2.0(th17:0x4a3c18)`；
`填充 = HYPER_TIME.current_f(0x4b5aac，f32) × 0.125`。
`+2.0` = 两侧各 1 的描边（填充满长 = 框长 − 2）⇒ **框长随初始时长变**；
初始时长本身随情况变：多数类 == 自机凭依（`th17:0x4b59f8`）时 `th17:0x40fbed` `+0x1e0` = +480 帧
（`th17:0x40fbe5` 的 `cmp esi,[0x4b59f8]`；混合类那条路 `th17:0x40fc40` 直接 1140，不加）。
对应本卡（表面 px）：时长 840 / 1140 / 1500 帧 → 框 **107 / 144.5 / 189.5 px**；
「多数类 == 凭依」时 1320 / 1620 / 1980 帧 → 框 **167 / 204.5 / 249.5 px**（多 60 px）。

**结论（卡侧落地，🟡 一处待实跑）**：新增 `ability.anm` 脚本 157（外框）/158（填充），由卡在 `on_ready` 起、
霊撃起手 / 就绪结束 / 卡卸载时清；每帧写位置 = `(640 + px×2, 32 + py×2 − ANIMAL_BAR_DY)`（表面 px，
换算见 `engine/anm/th18/01-vm-instantiate.md` §3）+ `scale = (ANIMAL_BAR_W, ANIMAL_BAR_H)`，
填充 `scale.x = (W−2) × time_left/duration`。
- 🟡 **TH17 用 `drawRectBorder(1,10)`，TH18 的 opcode 612 一用就崩**（实测 C0000005，`engine/anm/th18/01-vm-instantiate.md` §2b）
  ⇒ 改成「同尺寸实心 `drawRect(1,1)` 打底 + 填充盖住 1 px」的季节槽同款画法（脚本里 `sprite(-1)` 必须有）。
- ✅ **长度已回收**（2026-09-10 用户口径「框长度分情况」）：`帧数 × 0.125 (+2)` ——
  `ANIMAL_BAR_PER_FRAME = 0.125f`；外框 = `duration × 0.125 + 2`、填充 = `time_left × 0.125`
  ⇒ 840/1140/1500 帧 → 框 107/144.5/189.5 px（同类 == 凭依 +480 帧 → 167/204.5/249.5 px）。
  +480 的凭依口径：TH18 没有凭依选择 ⇒ 用**自机序号 % 3**（`CE_ADDR_CHARACTER % 3`，与光环颜色同一口径）。
- 🟡 **`ANIMAL_BAR_DY` 不是从 TH17 代码里回收的**（TH17 的偏移走 VM 位置，量纲可能是 640 空间 ×2）⇒ 实跑调。
- 一手脚本原文已入库：`local/th17.v1.00b/anm/beast/beast.anm.txt`（gitignored，构建/复现用）。

### AL11. 咆哮期间的 shot 组切换（TH17 `+10` 狼 / `+15` 鹫）—— 2026-09-10

**一手语义**：`Player__do_shooting` `th17:0x44a710`：平时 `idx = 火力档 (0..4) (+5 若聚焦)`；
咆哮中且兽灵 = 狼 → `idx = 火力档 + 10`、鹫 → `+15`（**聚焦与否一样**，獭 / 混合不换）。
TH17 的 `set 10..14` / `15..19` 就是那两组弹幕（`set10`/`set15` 是空组 = 火力档 0 没有子机就没有弹）。
文件布局 / 完整解码 / func 与 anm 映射见 [`engine/sht/th17/01`](../../../engine/sht/th17/01-file-layout-and-shooterset-index.md)、
[`engine/sht/th17/02`](../../../engine/sht/th17/02-hyper-shot-groups.md)（本次新开的两篇）。

| # | claim | 结论 |
| --- | --- | --- |
| AL11a | TH17 `pl0X?.sht` 能用现有工具解开 | **CONFIRMED（一手）** —— 解析器 `th17:0x4463f0`，见下 |
| AL11b | 那两组能追加进 TH18 的空位 | **CONFIRMED** —— `assets/sht/append_shooterset.py` 新增 10 组：`0x1e..0x22`（狼，`WOLF_0` = 空组）、`0x23..0x27`（鹫，`EAGLE_0` = 空组），正好填满 `0x17..0x27`；生成器回读自检（前 23 组逐字节不变 / set0 四个 func 仍为 0）|
| AL11c | 运行期怎么切 | **CONFIRMED（本卡实现）** —— 只改**数据指针**：咆哮中每帧把 `sht+0xe0` 的**主炮 0..9 槽**改指到 `基础槽 + i%5`（0..4 与 5..9 指同一组 ⇒ 聚焦不影响）；不动一行代码、不加 binhack |
| AL11d | 还原安全 | 存档用**文件级 static**（跨局重启后卡状态会重建，缓冲里可能还留着我们的指针）；写回前校验「还是同一块 sht 缓冲」，换角色重载后不往野指针上写；`hyper_end`（两条路径）与 `clear_card`（卸载 / 换局）都还原 |
| AL11e | func / anm 映射 | 咆哮组用到的 func 大部分有语义等价，`hit6→hit4` / `hit5→hit2` 是常数不同的最接近替代（🟡），弹贴图脚本见 [`engine/sht/th17/02`](../../../engine/sht/th17/02-hyper-shot-groups.md) §3 / §4 |

**AL11a 细节**：数据基址 `+0x130`、stride `0x58`、终止符 = fire_rate 的符号位；字段 `+0x00..+0x27` 与 TH18 同构，
TH17 从 `+0x28` 起整体比 TH18 早 4 字节（TH18 在 `+0x28..+0x2b` 多 2 字节填充 + `fire_rate2`/`start_delay2`）。

**AL11e 细节**：咆哮组只用到 init{0,1,4}、tick{0,1,3,5,6}、hit{0,1,5,6}；其中 init1/tick1/hit1、
init4→init2、tick3、tick5→tick2 有语义等价（✅）；灵梦 / 魔理沙的弹贴图脚本 `5/6`、`8/11` 两作逐字节相同（✅），
咲夜 / 早苗用 TH18 主炮弹替身（🟡）。

**实跑纠偏（2026-09-10 第三轮，两条都是我写漏的）**：
- **`anchor` 必须显式写 `anchor(0, 0)`（锚心）**：不写时默认是**左上角** ⇒ 条从自机位置往右长出去、
  左边一半不见了（用户：「进度条在自机的右侧、长度像两倍」）。TH17 script30/31 写的是 `anchor(0,2)`
  （锚心 + 底边），本卡要的是锚心 ⇒ 两个脚本都补 `anchor(0, 0)`（AUDIT §AL10 早期「不写 = 锚心」的记录是错的）。
- **填充脚本 158 漏了 `alphaTime`**：`alpha(0)` 之后没接 `alphaTime(20,0,255)` ⇒ 外框淡入了、填充恒 alpha 0
  **整条看不见**（用户：「看不见进度的条」）。现在与 157 对称：`+20: alphaTime(20,0,255); color(...)`。

**🟡 实跑要确认**：`pl02`（咲夜）/`pl03`（早苗）的弹贴图替身、`hit6→hit4` 的伤害源半径 / 速度差、
TH17 的 `mode=1/2` 与 TH18 是否逐位等价；特别是 `mode==2`（子机火力分配）在**主炮路径**下会不会因为
TH18 的子机槽为空而把弹从 (0,0) 发出 —— 若如此就把那几条改写 `mode=0`。
**咲夜 / 早苗的形状待定**（替身 → 按本人原生弹幕放大？见 [`NEXT.md`](NEXT.md) §1v-补 的 A/B/C 三案）。日志关键字
`wba_wc: shot set -> wolf|eagle (main 0..9 = slots N..M, sht 0x…)` / `shot set restored (main 0..9)`。

### AL12. 实跑第三轮的三处修正（2026-09-10，用户口径）

| # | 现象 | 一手依据 | 结论 |
| --- | --- | --- | --- |
**AL12d. 残碎 / B碎 不走椭圆（2026-09-10 用户提问后复核）**

`th17:0x410640` 的队列遍历（`edi = 0x4b5a64` = TOKENS[0]，`edi < 0x4b5a78` = 5 格）按 type−4 查跳表 `0x4108b8`：

| 队列里的鱼 | 撒什么 | 位置 |
| --- | --- | --- |
| 4（B鱼）| **1 颗** item 6（符卡碎）| `(自机.x, 自机.y − 80 − 30×同类已处理条数)`（`0x4106e1` 自己那本计数）|
| 5（残鱼）| **1 颗** item 4（残机碎）| 同上（`0x4106fd` 跳到 `0x4106d6`，之后同样 −30）|
| 6（P鱼）| 30 颗 item 1 | **椭圆公式**（`0x41071d` 段）|
| 7（点鱼）| 30 颗 item 2 | **椭圆公式**（`0x4107d8` 段）|

- 单颗分支的基座是 `[ebp-0x20]`，椭圆分支的基座是 `[ebp-0x4]`；**两条各降各的 30**
  （单颗分支 `jmp 0x41089a` 跳过 `0x410888` 那次减法）⇒ 队列里「两残碎 + 一 P鱼」时，
  残碎在 y−80 / y−110，P 鱼burst 在 y−80（不是 y−140）。
- 三处常量都取自 `th17:0x4a3ce4` = **30.0f**；每颗道具都是 **出膛角 −π/2、速度 2.2**；
  单颗 timer = 10，burst 的 timer = 10,12,…,68。

**AL12b 公式（一手 `th17:0x41071d`–`0x4107cd`）**：位置 = (自机.x, 自机.y−80) + `(cos(a)×i, sin(a)×i/2)`，
**i = 0,2,…,58**（每轮 +2.0，共 30 颗）、`a = Rng__rand_float(min=i, max=i×0.5) × π`
（`th17:0x402880` + `th17:0x4a3c5c` = π）；每颗**出膛角固定 −π/2、速度 2.2**、timer = 10,12,…,68。

**AL12c 一手**：[`engine/card/th18/05-shop-and-money.md`](../../../engine/card/th18/05-shop-and-money.md) §8
的表 —— type `0x10`→EXTEND2（残机碎片）、`0x11`→BOMB2（符卡碎片）、**`0x12`→PENDULUM（+50 金）**、
`0x13`→DANGO（+0.50 火力）；收集时 `allocate_new_card(卡 id)`。

| # | 现象 | 一手依据 | 结论 |
| --- | --- | --- | --- |
| AL12a | **飞在画面上的灵长得跟槽里一样** | 脚本 137–143 自己就有 `sprite(@WOLF_0/@FISH_*)`（= 零售 `beast1.png` 的 token 单元）；`token_make_vm` 里多写的 `ce_anm_set_sprite(…, sprite_for_type())` 把脚本贴图覆盖成了 `ICON_*`（= `beastf.png` 的**槽内图标**）| **CONFIRMED** —— 删掉那行：飞着的用 `beast1.png`、槽里的用 `beastf.png`，两套分开 |
| AL12b | **一次撒 30 颗道具的位置全叠在一个点** | TH17 `th17:0x41071d`–`0x4107cd` 的展开椭圆（公式见下）| **CONFIRMED** —— 现在按同一公式撒，范围随序号扩大 |
| AL12c | **金钱鱼掉的是「金钱点」不是「金钱卡」** | TH18 道具表 `th18:0x4b4020`：type 2 = 金钱（`MONEY += 1`，金色方点）；type `0x12` = 金钱**卡** | 先改成卡 ×1 → **2026-09-10 二次定案回退**：金钱鱼掉 **type 2 金钱道具 ×30**（TH17 的形状）|

### AL13. 出生点 / 无敌时间（2026-09-10 用户口径：写进 CARD-CHECKLIST 铁律 3）

| # | claim | 证据 | 结论 |
| --- | --- | --- | --- |
| AL13a | TH18 的**环收**按半径判，落在圈里就被吸 | `th18:0x4465da` 段（细节见下）| 本卡原基座 = `y−80`（照 TH17）只在默认 70 之外一点点 ⇒ **带道具回收范围卡 21 时就出生在圈里** ✅ 实跑复现 |
| AL13b | 两个引擎的「无敌时间」= spawn 第 6 参的 intro 倒计时 | TH18 `th18:0x445b78` / TH17 `th17:0x433520`（细节见下）| 本卡一直照抄这个 timer ✅（问题不在它）|
| AL13c | TH17 自己也会主动避开自机附近 | `th17:0x4347af`–`0x434838`（细节见下）| 用户的铁律与原版同源 ✅ |
| AL13d | 碎片 / 金钱该掉**一次性卡**而不是遗留道具槽 | `th18:0x4b4020` + `engine/card/th18/05` §8 | 改动：4(B鱼)→`0x11`、5(残鱼)→`0x10`、7(金钱鱼)→`0x12`；速度照辉针城实跑口径（碎片卡 1.2、P/钱 2.2）|

**AL13a 细节**：`th18:0x4465da`：`state ∈ {3,4}` 跳过；否则距离² 比 `player+0x47990`（聚焦）/
`+0x47994`（非聚焦）²（`0x4465f3`/`0x446606`）→ 进圈就把 `item+0xc74 = 4`、吸附速度
= `player+0x47988 / dist`（`0x446647`）。位置取自 ECL 坐标（`item+0xc2c` vs `player+0x620`）
⇒ 半径单位是 **ECL 单位**（默认 70；道具回收范围卡 21 抬到 110）。

**AL13b 细节**：TH18 `item+0xc84`（`th18:0x445b78`）：`> 0` 就 `--` 并 `jg 0x4467e5`
**跳过整段 state-1 处理**（不动、不判收），归零才调 `0x4472b0`；TH17 同构 `item+0xc68`
（`th17:0x433520` 段 `jg 0x434173`）。TH17 鱼灵换道具传 **10,12,…,68**（`th17:0x4106bb`/`0x4107b2`）。

**AL13c 细节**：`th17:0x4347af`–`0x434838`：卡类道具（type 0x10–0x20）先 `call 0x404a10`
取「自机 → 出生点」的方位，再**拒样最多 0x64 次**（`0x402880` RNG）挑一个安全位置。

**卡侧实现**（🟡 2026-09-10 当天改了两版，以第二版为准）：
`animal_pickup_ring()`（读聚焦 / 非聚焦环收半径）+ `animal_in_pickup_ring(pos)`（**纯检查**）。
第一版把每颗都沿方位推到圈外 ⇒ 用户实跑看到「**椭圆没有被填满、而且大了**」——30 颗全贴在圈边上 ✗；
第二版改回**只查不动**：照 TH17 的 `i = 0,2,…,58` 均匀铺满椭圆，出生点的圈内命中数写进日志
`inside_ring N/M`（铁律 3 的第①问要「查看」），靠 **intro 倒计时**（铁律 3 的第②问、TH17 的 10..68 帧）
保证刚出生那几帧不被吸。**要挪位置只能整组一起挪，不能逐颗推。**

### AL14. 彩蛋「没有动物灵时彩虹光环」（2026-09-10 实跑纠偏）

口径（用户 2026-09-10）：「当没有动物灵的时候（几乎不太可能），颜色是每一帧随机的」——
随机的是**颜色**，但动画帧要**照常往前走**（用户：「应当是 A 颜色第 1 帧、B 颜色第 2 帧、C 颜色第 3 帧」）。

| 版本 | 做法 | 结果 |
| --- | --- | --- |
| 第一版 | `aura_tick` 里每帧重掷颜色、**颜色变了就删掉重建**光环 VM（复用脚本 146–148）| ❌ 重建 = 脚本每次都从第一帧开始 ⇒ 「卡在不同颜色的首帧」+ 入场 scaleTime 反复重放 = 「抽动」 |
| 第二版（现行）| 新增**静态贴图版**脚本 **159**（`AURA_RAINBOW`，与 146–148 的设置逐条一致但没有 sprite 序列），C 每帧只做 `ce_anm_set_sprite(vm, AURA_<色>_<帧号>)`；帧号自己数（8 张 × 每张 3 帧 = 24 帧一轮），颜色每帧重掷 | ✅ 帧自然推进、只有颜色在跳 |

贴图号连续性（`native/anm_ids.h` 生成物）：`SPRITE_AURA_EAGLE_0` 217..224 / `WOLF_0` 225..232 /
`OTTER_0` 233..240 ⇒ `sprite_for_aura_frame(type, k) = 基址 + k`。

### AL15. 左下角槽位的「自机贴近渐隐」（2026-09-10 用户口径）

**不对齐 TH17，改用天空璋卡（TH16）那一套**，好让两张系统卡一致（用户原话：「为了统一，这个不对齐 th17，
采用 th16 卡渐隐的时间序列，位置暂时照抄，最终版本是要考虑 UI 此时的位置，因为多张卡得垒起来」）。

| 项 | 取值 | 来源 |
| --- | --- | --- |
| 淡出后的 alpha | **64** | TH16 `interruptLabel(5)` = `alphaTime(5,0,64)`（天空璋卡 `SEASON_HUD_DIM`）|
| 每帧步长 | **51** | 天空璋卡的近似（255→64 约 5 帧；原始是 `alphaTime(5,…)`）|
| 触发 / 迟滞 | `x < −64 且 y > 400` 淡出；`y < 384 或 x > −64` 淡回 | TH16 `th16:0x427f41` / `th16:0x427f6c`（天空璋卡 `SEASON_HUD_*`）|

- 覆盖方式：图标（静态贴图）每帧写 `A | 0x00ffffff`；灵魂 / 火焰脚本自带 `alphaTime` 淡入 ⇒
  **只在淡出期间**才覆盖它们的 alpha（淡回 255 那帧补写一次，标记 `hud_fade_touched`）。
- 🟡 **最终版**要按「UI 此刻的位置」判：多张左下角卡要竖直垒起来（[`SDK.md`](SDK.md) §17 /
  [`HUD-LAYOUT.md`](HUD-LAYOUT.md)），届时只改 `animal_hud_player_near()`（把 HUD 可见矩形算进去），
  触发逻辑本身不用动 —— 现在**暂时照抄**天空璋的阈值。
- 日志：`wba_wc: hud dim on (player near slot area)` / `hud dim off`。

### AL16. 道具灵的**槽内图标** = 「咆哮结束后拿到的东西」（2026-09-10 用户口径）

用户原话：「对于道具灵来说，它们在槽里的样式是你咆哮结束后会得到的东西。所以按照咱们目前的改法，
除了 P 点灵之外，剩下三个灵都得改成资源卡的贴图」。

- **一手依据**：卡道具（item type `0x10`–`0x13`）在场上**画的就是那张卡本身** ——
  `th18:0x447089` 段（`CE_FN_SPAWN_ITEM` 里 `type−0x10 ≤ 3` 的分支）不走 bullet 图集，而是
  `0x409310` 建一个 UI VM、再用卡 id 去取 `abcard.anm` 的卡面（普通道具才走 `0x41f920(bullet_anm, …)`）。
- **落地**：`assets/ability/make_animal_art.py` 新增 `CARD_ITEM` 三张（`BOMB2_min` / `EXTEND2_min` /
  `PENDULUM_min`，零售画布 64×128、卡面 64×80 ⇒ alpha bbox 裁掉空边后等比缩到 64×64 画布），
  ORDER 追加 `CARD_BOMB` / `CARD_LIFE` / `CARD_MONEY`；`sprite_for_type()` 里 4→`CARD_BOMB`、
  5→`CARD_LIFE`、7→`CARD_MONEY`（**6 = P 鱼保持原样**，用户口径「除了 P 点灵之外」）。
- ⚠️ **2026-09-10 二次定案（用户）：「槽里显示资源卡卡面」那版作废** —— `sprite_for_type()` 已回退成
  TH17 的鱼形槽位图（4/5/6/7 → `ICON_FISH_*`）。三张卡面贴图（`CARD_BOMB/CARD_LIFE/CARD_MONEY`）
  仍留在 ORDER 里备用、**不再被引用**；`make_animal_art.py` 的 `CARD_ITEM` 生成步骤也留着。
  金钱鱼自己的槽内图等用户 P 好金版后另加 `ICON_FISH_MONEY` 一行替换。
- **金钱鱼灵的两张手绘图**（2026-09-10，都在 `assets/ability/animal_custom/`，不被生成器覆盖）：
  `FISH_MONEY.png`（**飞行**贴图：脚本 143 的 `sprite(@FISH_MONEY)`，sprite 280）+
  `ICON_FISH_MONEY.png`（**槽内**图标：`sprite_for_type()` 的 default，sprite 281）。
- **顺带纠偏（2026-09-10）**：TH18 **道具图标**的来源一直是记错的 —— `th18:0x4b4020` 里那个号是
  `bullet.anm` 的**脚本号**（`script112..125`），脚本 112(P)/113(金钱)/115(命碎)/117(符卡碎) 画的是
  `bullet/item.png`（`bullet.anm` entry3）的 `sprite500/502/506/510`
  （P = 红色方点 16×16、金钱 = **金色方点** 16×16、命碎/符卡碎 = 32×32）。`make_animal_art.py` 的
  `ITEM_CELLS` 已改成从这个源切（之前切的是 `bullet1.png` 的弹图元，那 4 张 `ITEM_*` 一直没人用所以没暴露）。

### AL17. 资源灵撒一堆道具时的「播音」（2026-09-10 用户：现在没声音）

**一手**：TH18 的道具实体有一个**没被零售用过**的播音字段 ——
`item+0xc90`：`th18:0x447323`（intro 结束那一帧，`0x4472b0` 的尾段）
`eax = [edi+0xc90]; if (eax >= 0) call 0x476be0(eax, …)`，而 `0x476be0` = 12 通道的 SE 播放器。
零售 spawn 把它**恒写 -1**（`th18:0x447057`、`0x447295`）⇒ 这条音在零售里是死的；
**只有 type {1,2} 的 intro-end 走这条路**（`0x4472ee` 段），其他 type 在 `0x4472e4` 直接跳走。
TH17 侧同构：`+0xc74` = spawn 第 8 栈参，而所有调用点（含鱼灵换道具 `0x4106bb`）都传 `-1`。

**结论（卡侧）**：把 `+0xc90` 当「道具出现」的播音用 —— 撒 30 颗时**每颗自己**在自己的 intro 末尾
（timer = 10,12,…,68）响一声 ⇒ 一串错开的「啪啪」，正是用户记忆里的那个效果。
单颗（碎片卡 type `0x10/0x11`）走不到这条路（`0x4472e4`）⇒ 它们仍用一次性 `ce_play_sound`。
日志：`… se 0xd`。SE id 走 `ANIMAL_SE_FISH_ITEMS = 0x0d`（要换只改这一个常量）。

### AL18. 就绪条 / 左下角槽 的实跑几何（2026-09-10 用户按 **1280 分辨率截图**量）

| 元素 | 调整 | 落地 |
| --- | --- | --- |
| **就绪条整体** | 左移 **193 px**、下移 **94 px** | `ANIMAL_BAR_DX 193`、`ANIMAL_BAR_DY (−54)`（原 −40 ⇒ −40+94 = +54，条心跑到自机**下方**）|
| 就绪条**框** | 下边界再下 **1 px**、右边界再右 **1 px**（左/上不动）| `ANIMAL_BAR_GROW 1`：框 W/H 各 +1，框心跟着 (+0.5,+0.5) 保持左/上边界不动 |
| 就绪条的**填充** | 再下移 **1 px** | `ANIMAL_BAR_FILL_DY 1`（填充心 = `cy + 1`）|
| **左下角 5 格槽** | 左移 **193 px**、下移 **4 px** | `ANIMAL_HUD_X0 88`（281−193）、`ANIMAL_HUD_Y 904`（900+4）|

**框改成真的「空心框」**（用户：「进度条本身只有一个框体，内部不要填充任何东西」）：
`bar_edge[4]` = **4 条 1px 矩形**（上/下/左/右，各一支脚本 157 的 1×1 模板 + C 每帧写 scale/坐标）——
TH18 的 `drawRectBorder`（opcode 612）一用就崩，四条边是天空璋卡外框同款画法；
这样**内部全空**（不再有原来那块灰色底板），只有「表示进度的条」在里面按剩余比收缩（朝中点）。
淡入淡出仍由脚本 157/158 的 `alphaTime` 负责（C 不写这两支的颜色）。

### AL19c. 火焰的锚点 / 缩放中心 / alpha（2026-09-10 用户实机口径）

用户实测三件事：①**填了没满**时是 **1 倍 + alpha<0.3**；②1 倍时位置与 TH17 一致；③**放大到 1.5 倍后错位**，
TH17 像是「同时往上移」/缩放中心不同。根因与修法：

| 项 | TH17 | 本卡改法 |
| --- | --- | --- |
| 缩放中心 | 火焰放大是**往上长** ⇒ 等价于锚在**底边中点**（`anchor(0,2)`，与 TH17 就绪条 script30/31 同口径）| 火焰贴图在 64×64 画布里改**底对齐**（48 宽居中、底部贴边），脚本 156 加 `anchor(0, 2)` |
| 位置 | 1 倍时与我们的旧画面一致 | C 侧位置补 `(32, 56)`（`ANIMAL_FLAME_ANCHOR_DX/DY`）⇒ **1 倍时画面逐像素不变**，1.5 倍时改为往上长 |
| alpha | 填了没满 = **128**（script24 `interruptLabel(27)` 的原值）；就绪 / 咆哮中 = **255** | `lit = ready || hypering` ⇒ `255`，否则 `128`；再和「自机贴近淡出」取 min |

**alpha 的原值（一手，script24 的 interrupt 表）**：

| label | scale | alpha | color | 语义（推测）|
| --- | --- | --- | --- | --- |
| 28/29/30/31 | **1.5** | **255** | 粉 / 绿 / 蓝 / 白 | 该槽 token 类型的「闪烁」|
| **27** | **1.0** | **128** | 白 | 平时（填了但没就绪）⇒ **本卡取这一档** |
| 4 | — | 128 | 白 | 同上（另一入口）|
| 2 | — | 淡到 `$I0`（30 帧）| — | — |
| 3 | — | 淡到 `$I0/4` = **63**（10 帧）| — | 更暗的一档（若 128 偏亮可换）|
| 5 | — | 淡到 0（2 帧）| — | 熄灭 |

用户当时眼睛估的是「<0.3」；脚本里跟「1.0 倍」配对的原值是 **128（0.5）**，
看起来更暗多半是因为火焰贴图本身带半透明（VM alpha × 贴图 alpha）。

> **为什么是 (0, 24)**（2026-09-10 二次纠偏，详见 §AL20）：脚本 156 的贴图是 48×48 放在 64×64 画布下沿
> （x 8..56 / y 16..64），`anchor(0,2)` 把**画布底边中点**钉在 VM 位置上；而火焰在改锚点**之前**是
> 「**图心 = 槽心**」（精灵 VM 默认锚心）⇒ 要维持老画面，VM 位置 = 槽心 + (0, 24)（24 = 48/2）。
> ~~+32/+56~~ 是我按「默认左上角」算的，错了 —— 第一版实跑火焰右下偏了 ~32 px 就是这个原因。

### AL19b. 火焰的 1.5 倍（2026-09-10 用户实测）

用户量出「火焰特效的显示大小好像正好是 **1.5 倍**」—— 根因：TH17 script24（= 本卡脚本 156）的
`interruptLabel(27..31)` 里每条都是 `scale(1.5f, 1.5f)` 且**不还原**（`caseReturn()` 回原标签继续跑），
所以引擎发过一次那类中断之后火焰就**停在 1.5 倍**。本卡没接那几条中断（它们同时改 alpha/color，
是「槽位闪烁」用的），所以直接在 C 里 `ce_anm_set_scale(flame, 1.5f, 1.5f)` 钉住尺寸。

### AL19. 咆哮期间的**自机光环**（2026-09-10 用户：混合时自机没有那个贴图特效）

**用户口径（2026-09-10 两轮，以第二轮为准）**：两个特效要分开看 ——

| 特效 | 脚本 | 暴走咆哮（type 1–3）| 混合咆哮（type 4）| 一只动物灵都没有（彩蛋）|
| --- | --- | --- | --- | --- |
| **全屏渐变圆**（`self_aura`）| 155（TH17 script32）| ✅ | ✅ | ✅ |
| **类型光环**（`aura`）| 146–148（TH17 script41–43）| ✅ | **❌ 不出**（TH17 实机口径）| ✅ **彩虹版**（脚本 159）|

- 两者都**不在霊撃起手清掉**，从就绪窗口一直挂到 `hyper_end`（上一轮按用户要求的有意偏离，保留）；
- 混合咆哮**不出类型光环**（`aura_spawn` 里 `if (!aura_rainbow && hyper_type == 4) return;`）；
- 彩虹彩蛋：**颜色只在「精灵表换下一帧」的那一帧才换**（每 3 帧一次），不是逐帧乱换
  —— 用户：「逐帧换颜色太花了，应该是当它在精灵表上切换下一帧的时候切换到下一个颜色的下一帧」。

**一手复盘（我们原来是照 TH17 做的，是刻意偏离）**：
`do_hyper_cancel` `th17:0x40f6d0` 里只 `push 0x21`（重生霊撃光圈，`0x40f72d`→`0x407230`），
同时用 `0x476510` 把 `manager+0x28` 那几个槽**逐个清掉**（`0x40f7c6` 起）——
也就是 TH17 原版**咆哮期间确实没有类型光环**（type+0x28 只在 `on_ready` `0x40fc27` 建过）。
本卡按用户要求**有意偏离**：不清光环，`hyper_end` 才 kill。

### AL20. ★ anchor 的默认值分图元：**矩形 = 左上角，精灵 = 锚心**（2026-09-10 两次实跑踩出来）

| 图元 | 不写 `anchor` 时的默认 | 证据 |
| --- | --- | --- |
| `drawRect` / `drawRectGrad`（就绪条、季节槽外框）| **左上角** | 就绪条第一版没写 anchor，用户看到「条从自机往右长出去、左边一半不见了」；补 `anchor(0,0)` 后正常 |
| `sprite(...)`（火焰、槽内图标）| **锚心** | 火焰改锚点前「图心 = 槽心」（位置与 TH17 一致）；我按左上角补偿 (32,56) ⇒ 实跑火焰**右下偏 ~32 px**（用户：「往右下了似乎一个自身大小」）⇒ 改成 (0,24) 后归位 |

**教训**：给同一批 HUD 加 `anchor` 时，别把「矩形那套默认」套到精灵上 —— 两边差半个 sprite 尺寸。
（这条也解释了为什么槽内图标一直位置正确：它们从一开始就是「图心 = 槽心」。）

### AL21. Miss 掉落的坐标 / 出生宽限 + 道具出现的播音（2026-09-10 用户实跑）

**① Miss 的 2 只灵跑到复活点了**（顺带把「咆哮正常结束」的释放基座也统一到 `(自机.x, 自机.y−64)`，
原来我写的是 `y−40` 且带 ±12 横排）（用户：「撞弹生成的动物灵似乎是在角色 respawn 之后才生成，
我在特定位置撞弹会直接收取」）：`on_death_frame2` 是 **respawn 之后**才跑的钩子，
里面的 `ce_player_pos()` 已经是复活点；而且左右偏移只有 ±24 px = 正好等于收取半径 ⇒ 当场被收。
修法：

- `on_death_before_deathbomb`（中弹那一刻）先把自机坐标存进 `card` 状态（`death_x/death_y/death_valid`）；
- `on_death_frame2` 用**存下来的中弹点**掉落，位置 = **中弹点正前方 (x, y − 64)**
  （`ANIMAL_BURST_DY`；用户口径「Miss 的灵是中弹点正前方的一个位置掉落」，
  与 TH17 `0x40e920` 的爆散基座 `(自机.x, 自机.y − 64)` 同源）；
  两颗同一基座、靠各自随机初速散开（`token_spawn` 本来就是单位圆随机初速）；
- 另外给所有 token 加 **10 帧出生宽限**（`ANIMAL_TOKEN_GRACE`，`t->age`）—— 这就是「铁律 3」第②问
  在 token 上的落地，以后任何「在自机身边生成可收取物」的代码都要有这一条。

**② P 点生成音效听不见 —— 最终结论：走 C 侧 `ce_play_sound`，不用引擎那个槽。**

排查过程（都记下来，免得下次再走一遍）：

| 尝试 | 结果 |
| --- | --- |
| 引擎钩子 `item+0xc90` + SE `0x0d` | ❌ 听不到。查表（`th18:0x4c9b8a`，stride 20，首个 word 看着像音量）：0x0c/0x0d 是 10，本卡其它 SE 都是 100 |
| 同钩子 + SE `0x0e`（表里 100）| ❌ **依旧听不到** ⇒ 说明问题不在音色，而在**钩子这条路**（可能：本卡的 item 没走到 `0x4472b0` 的 intro-end 尾段，或 `0x476be0` 的 12 通道那时全占）|
| **C 侧 `ce_play_sound`（现方案）** | ✅ 生效。音效 id 最后**一手定位**到 TH17 自己的道具音（见下表）|

**★ TH17 的道具音到底是谁（一手定位）**：TH17 的声音播放器 = **`0x4654f0`**（`push <id>; call 0x4654f0`，
一手：`spawn_token` `0x41055d` 的 `push 0x3b`）。把 TH17 物品代码区（`0x430000`–`0x440000`）里所有
`call 0x4654f0` 捞出来，只有两条：

| 调用点 | SE | TH17 表 `0x4b2bfa` | TH18 表 `0x4c9b8a` |
| --- | --- | --- | --- |
| `th17:0x433b89` / `0x433c1f` / `0x433c96` / `0x433d1b` / `0x4343bc` | **`0x0d`** | vol **10**，snd 33 | vol **10**，snd 33（逐行相同）|
| `th17:0x433f90` | **`0x25`** | vol **100**，snd 16 | vol **100**，snd 16（逐行相同）|

⇒ 0x0d 就是原版那条**轻**的道具音（我最初猜的 0x0d 没错，只是音量 10 听不出来）；
本卡改取**同源、音量 100** 的 **0x25** 当播音（`ANIMAL_SE_FISH_ITEMS`）。
⚠️ 另外：TH17 的鱼灵换道具 `0x410640` 全程**没有** SE 调用（一手：那段区间没有 `call 0x4654f0`）。

⇒ `ANIMAL_SE_FISH_ITEMS = 0x3b`，在 `fish_to_items` 撒完那一批后直接播一次。
音色不满意改这一个常量即可（`0x3f` 收集 / `0x4d` 就绪 / `0x2c` 霊撃 都是 100 档、都已验证能出声）。
`CE_ITEM_SOUND_ID`（`item+0xc90`）的一手结论保留在 `engine.h`，但标注「本卡实测无效」。

### AL22. 咆哮**自然结束**的释放动画（`beastap.png`，2026-09-10 用户实机）

用户描述：「咆哮自然结束后会出一个 extra beast appeared 的贴图（`beastap.png`），好像在闪烁，
然后是这个消失之后才出 2 个动物灵」——**一手完全对上**：

| TH17 | 内容 |
| --- | --- |
| `0x40f494`（`HYPER_TIME <= 0` 且没霊撃的那条路）| 在 **2× 自机位置**建**脚本 `0x2c`(44)** 的 VM（存 `manager+0x3c`），并**复位 `release_timer`** |
| 脚本 44 | 父脚本：`layer(20)`、`pos(0,-128,0)`，`scriptNew(script46)` + `scriptNew(script45)` 后 `stop()` |
| 脚本 45 | `sprite95`（272×32）：`alphaTime(10,4,255)` 淡入 → `+10` 起 `alphaTime(60,1,0)` 淡出 → 70 帧停 |
| 脚本 46 | `sprite96`（272×32）：`+5`/`+5` 的 **`jmpDec` 10 次闪烁**（`color(224,224,192)` ↔ `(255,255,255)`）→ `scaleTime(15,0,0,0.5)` 压扁 |
| `release_timer == 120` | `sub_40e920` 才在 `(自机.x, 自机.y − 64)` 爆散动物灵 + 播 **se 0x3e** ⇒ 动画（~70 帧）先消失，2 只灵后到 |

**卡侧落地**：资源侧新增 `beastap.png`（entry12）的**两张 272×32 切图**（补成 512×32 画布、图居中，
这样「精灵 VM 默认锚心」时图心 = VM 位置，与原图一致）+ 脚本 **160（父）/161（子 A）/162（子 B）**
（父脚本去掉 `pos()`、`layer(20)→25`，位置由 C 每帧写 = 自机表面坐标 + `(0,-128)`）。
C 侧：`release_spawn()` 在 `hyper_end(by_hyper=0)` 建父 VM + `release_wait = ANIMAL_RELEASE_FRAMES(120)`；
`release_tick()` 每帧跟着自机、归零那帧才 `animal_end_burst`（2 只，位置 `(自机.x, 自机.y−64)`）+ se 0x3e，
并删掉父 VM（子树一起删）。**鱼灵换道具仍在超时那一刻立刻做**（TH17 第 2 步，不延迟）。

### AL23. 对话冻结 / 咆哮时长机制复查 / 撒道具音的最终定位（2026-09-10）

**① 对话中咆哮计时冻结**（用户口径）：`dialogue_open()` 为真时 ——

- 就绪窗口本来就不递减（`dec = paused ? 0 : …`）✅（原有）；
- **霊撃的 40 帧也停**（原来照走 ⇒ 修：`if (!paused) { hyper_frame(); animal_hyper_tick(); }`）；
- 释放动画的 120 帧等待 + 撒道具的「大量」播音同样冻结（都挪进 `if (!paused)`）。
- 动物灵**照常运动 / 变色 / 可被收**（这是用户早先定的口径，不动）。

**② 「停枪减缓 / 吃灵延长」复查**：两条**都已实现**（无需改）：

| 机制 | 实现 | 一手 |
| --- | --- | --- |
| 停枪减缓就绪窗口流失 | `dec = 1 + player_has_live_bullets()`（有存活子弹 −2，停枪 −1）| TH17 `0x40ef75` 段 |
| 收灵延长咆哮时间 | `animal_next_extend`（用户口径 90/60/42/28…）加到 `time_left`（就绪窗口）| TH17 代码是 `180 ×2/3` ⇒ 120/80/53 |
| 霊撃（40 帧）中收灵 | 进 `pending[5]`，霊撃结束回灌队列（可当场再次就绪 = 连咆哮）| TH17 §5.4 第 3–4 步 |

**③ 撒道具音的最终定位**：用户指名 `se_lgods4.wav`。
> ⚠️ **2026-09-11 订正**：这里原来推的 `0x3e` 是**错的**（当时把 SE 表的行基准当成 `0x4b2bfa`，
> 其实行基准是 `0x4b2bf0`/`0x4c9b80`，`0x4b2bfa` = 行内 `+0x0a` = **音量**）。正确版面见 §AL26：
> **`0x34` = se_lgods4.wav**，`0x3e` = se_tan03.wav。本卡已改成 `ANIMAL_SE_FISH_ITEMS = 0x34`。
播放方式：引擎钩子 `item+0xc90` 在本卡实跑不出声（0x0d/0x0e 都试过）⇒ 改成 C 侧
**每 2 帧一声、共播 n 声**（n = 这一批的道具数；原版是每颗各自在 intro 末尾响一声，听感「大量」）。

### AL24. ★ 咆哮是「满槽自动生效」——兽灵专属效果绑 bit1，不是等 C 键（2026-09-11 用户口径 + 一手）

**用户口径**：「暴走咆哮是收集到 ≥3 个同类动物灵那一档、**满槽就自动发动**的咆哮（有额外特殊效果），
不是按 C 的东西」+「th17 的暴走咆哮效果是**加强版的子机弹幕**、主机不发弹幕」。实跑症状：
满槽后主炮弹幕**完全没变**。

**一手复核（TH17 v1.00b）**：

- `FUN_0040fb60` `th17:0x40fb60`（满 5 格**就绪那一刻**）：算 `hyper_type` / 时长、`HYPER_FLAGS |= 2`
  （bit1 = **咆哮中**）、建就绪条与光环 VM。**没有任何输入位参与**。
- [`engine/sht/th17/01`](../../../engine/sht/th17/01-file-layout-and-shooterset-index.md) §4（一手）：
  `Player__do_shooting` `th17:0x44a710` 的索引公式 = 读 `th17:0x4b5abc`(hyper_type) 与
  `th17:0x4b5ac4`(HYPER_FLAGS **bit1**)：狼 → `火力档 + 10`、鹫 → `火力档 + 15`。
  ⇒ **主炮组在就绪那一刻就换了**（不需要任何按键）。
- 输入位 `0x2`（炸弹键）走的是**另一件事**：`sub_40ee70_beast_related` `0x40f37c` 里
  `if (input & 2) do_hyper_cancel(this, 0)` —— 把已在跑的咆哮**用掉**成 40 帧霊撃（§5.3）。

**原实现的问题**：把「就绪」当成**待按键的窗口**（`ready`），兽灵专属效果（狼/鹫换主炮组、獭 3 光球）
只挂在 `hypering`（= C 触发的 40 帧霊撃）上 ⇒ 满槽时玩家什么都看不到，正是「主炮弹幕没变」。

**改法（`animal.c`）**：

- 新增 `roaring(s) = s->core.ready || s->core.hypering`（= TH17 bit1 的口径）；
- `hyper_shot_apply` / `orb_ensure` 的门从 `hypering` 改成 `roaring`；
- `on_tick_2` 的 `else if (s->core.ready)` 分支每帧 `hyper_shot_apply + orb_ensure`（幂等）；
- `on_ready` 增日志 `roar auto-start (type …, … frames, beast effect = …)`；下一帧 `hyper_shot_apply`
  会打现有的 `shot set -> wolf/eagle`（首帧一次）；
- 霊撃入口 = **X（炸弹键，TH17 原位置）**：`on_tick_2` 读 `INPUT_PRESSED & 0x2`（门：`ready` 且非对话）
  → `hyper_begin(s,0)`；C（SDK `on_activate`）是同一动作的 TH18 主动卡版入口。用户 2026-09-11：
  「原版按 X 的效果 = 中弹」——即丢咆哮换 40 帧无敌/消弹，不扣命；中弹取消走同一段（`by_hit=1`）。
  TH17 在这两个状态下会屏蔽原版炸弹（`player_is_trying_to_bomb` `0x411d40` 要求 `HYPER_FLAGS & 6 == 0`）；
  本卡不劫持炸弹入口（TH18 侧正常没有炸弹库存）。

**★ 用户 2026-09-11 给的「咆哮模式」官方说明（照抄，本卡的验收口径）**：

> 收集动物灵道具后，灵会储存在画面左下的灵槽内，每集满 5 个灵（不论种类）**直接触发「咆哮模式」**。
> 如果五个灵中有三个同类基本动物灵将触发带有额外效果的「暴走咆哮模式」。
> 收集的五个灵中同类基本动物灵数量越多，「暴走咆哮模式」时间越长。
> 在「咆哮模式」下，停止射击可以减缓「咆哮模式」的结束时间。
> 在「咆哮模式」中取得动物灵道具可以稍微延长限时，取得道具鱼灵可以立刻获得相应道具。

逐条对到实现（§AL24 改完后）：

| 说明 | 实现 |
| --- | --- |
| 满 5 个（不论种类）**直接触发** | `animal_ready_compute` 满 5 → `on_ready`；`roaring() = ready \|\| hypering`，专属效果当帧起生效（无需按键）|
| 有 3 个同类 → **暴走**（带额外效果）| 多数类 ≥3 → `hyper_type = 1/2/3`；狼/鹫换主炮 shot 组、獭 3 光球 |
| 同类越多时间越长 | `animal_hyper_duration`：3 → 840 / 4 → 1140 / 5 → 1500（同类 == 自机凭依再 +480，RE 一手）|
| 停止射击减缓结束 | `dec = 1 + player_has_live_bullets()`（停枪 −1、有存活子弹 −2）|
| 咆哮中收灵**稍微延长**限时 | `animal_next_extend` = 90 / 60 / 42 / … 帧 |
| 咆哮中收**道具鱼灵立刻**拿道具 | `animal_collect` 返回 3 → `fish_to_items` 当场撒道具 |

（说明文本没提「用咆哮」，那一项是 RE 一手的炸弹键 `do_hyper_cancel`；本卡的入口 = **X**（TH17 原位置），C 是附加的 TH18 主动卡版入口。）

**顺带排除的两个方向**（留给下次别绕）：

- 换组数据本身没问题：部署的 `pl0X.sht` 里 `WOLF_1..4` 各 7 条（`hit_w=112`）、`EAGLE_1..4` 各 5 条；
  引擎每帧现读表（`th18:0x45ebcb`–`0x45ebfb`：`idx = CURRENT_POWER/POWER_PER_LEVEL (+5 聚焦)`，
  `row = [sht+0xe0+idx*4]`，首字节 `<0` = 终止符）。空组只在**火力档 0**，而 TH18 正常游玩到不了
  （`0x40d400` 那段 `cmovl eax, edx`(edx = `POWER_PER_LEVEL` = 100) 把下限钳在 1.00）——
  所以「目标组为空就不换」这条兜底不是病因（保留着无害）。
- 「C 键分派没到」是**误判**：同日早先那版加过输入边沿日志 + 兜底轮询并部署过一版，
  当日即全部撤回（DLL 回到 `cc39f6a0`）；本次真因是语义（自动 vs 按键）。

### AL25. ★ 主凭依灵：TH18 当「不存在」；带 25 / 30 的彩蛋（2026-09-11 用户定案）

**用户口径**（原话）：

> 首先第一点 th18 当不存在主凭依灵来做，也就是对应 th17 的主凭依灵非本凭依灵来做，但是彩蛋：
> 当持有弱肉强食之理或鬼杰组长的恐吓的时候，视为主凭依灵均存在，即触发的任何暴走咆哮
> = 主凭依灵 = 触发灵的暴走咆哮

**语义**：TH17 每个自机固定绑一个「主凭依灵」（霊夢A = オオカミ霊 / B = カワウソ霊 / C = オオワシ霊，
`engine/animal/th17/01` §3 表格 + `trophy.txt`）。`FUN_0040fb60` 里**只有**「队列多数类 == 主凭依灵」
那一条会 `+0x1e0`（= +480 帧，`th17:0x40fbe5` / `0x40fbed`）；别的地方（主炮组、光芒）都只按
`hyper_type` 走，不看主凭依灵。TH18 没有凭依选择 ⇒ **按「主凭依灵 ≠ 本凭依灵」做**，即恒不加 +480；
彩蛋：卡组里带着「弱肉强食之理」(id **25**，早鬼) 或「鬼杰组长的恐吓」(id **30**，袿姬) 时视为
主凭依灵**恒存在** ⇒ 任何暴走咆哮都算「本凭依灵的暴走咆哮」（吃 +480）。

**实现**：`animal_core.h` 的 `core.goast` 三态 —— `0..2` 指定 / `ANIMAL_GOAST_NONE`(-1) 不存在 /
`ANIMAL_GOAST_ANY`(-2) 恒存在；`animal_ready_compute` 的判定 `goast == ANY || best_type-1 == goast`。
卡侧 `goast_for_deck()`（`animal.c`）每帧读一遍**本局卡组链表**（`mgr+0x18` 哨兵 / `+0x1c` 首结点，
`CE_NODE_NEXT`/`CE_NODE_CARD`/`CE_CARD_ID`，走法照 `sdk.c` 的 `card_in_list`）：命中 25 或 30 → `ANY`，
否则 `NONE`。单测 `test_animal_core.c` 补 5 条（NONE 不加 / ANY 三类都加 / ANY 遇混合仍不加）。

> 🟡 **「持有」按「在本局卡组里」实现**（不是解锁表 `owned[]`）—— 与 25/30 自身效果的口径一致；
> 若用户要的是「解锁过就算」，改 `ce_owned(25)`/`ce_owned(30)` 一行的差别。

### AL26. ★ TH17/TH18 的 SE 表逐行解码（2026-09-11）—— id → wav 文件对照

**为什么要做**：用户两次指名「这条音应该是某个 wav 文件」（撒道具 = `se_lgods4.wav`、
暴走咆哮正常退出 = `se_pldead01.wav`），而卡里只能传 **SE id**。AL23 那次推错了一格，这次把版面钉死。

**版面（两作同构，0x00..0x53 逐行相同）**：

- 行基准 `th17:0x4b2bf0` / `th18:0x4c9b80`，stride **0x14**（20 字节）。
  一手：初始化循环 `th18:0x476460` / 播放器 `th18:0x476bea`（`movsx esi, word[eax*4+0x4c9b8a]`，
  `0x4c9b8a = 0x4c9b80 + 0x0a`）。
- 行内：`+0x00 u16 = wav 文件号`（0-based）、`+0x02 u16 = 计数器`、`+0x04 u32 = 装载偏移`、
  **`+0x0a u16 = 音量`**、`+0x0e u16 = 1`。
- wav 文件号 → 文件名：exe 里的指针数组（`th17:0xa0460` 69 项 / `th18:0xb33a0` 72 项），顺序与
  `thdat -l` 的 dat 顺序一致（TH18 多 `se_warpl/warpr/notice` 三只）。
- **自证**：低 id 与语义一一对上 —— `0x04 = se_pldead00`、`0x05 = se_invalid`、`0x08 = se_graze`、
  `0x0c = se_select00`、`0x0d = se_extend`、`0x0e = se_cardget`、`0x16 = se_pause`。

**本卡用到的对照**（两作相同）：

| id | wav | 用在哪 |
| --- | --- | --- |
| `0x2c` | se_item00 | （霊撃音；零售 `do_hyper_cancel` 推的就是它）|
| `0x34` | **se_lgods4** | 资源灵撒道具的「大量」播音（用户指名）|
| `0x38` | **se_pldead01** | **暴走咆哮正常退出**（用户指名，2026-09-11 接）|
| `0x3b` | se_fault | 动物灵出生（零售 `0x41055f`）|
| `0x3e` | se_tan03 | 咆哮超时爆散 2 只（零售 `0x40ec50`）|
| `0x3f` | se_wolf | 收集动物灵（零售 `0x410b73`）|
| `0x4d` / `0x4e` | （表里文件号 77/78，超出 69 个 wav 名字表；零售确实推这两个值）| 就绪音 / 变色音 |

`0x42`（se_item01）是零售在「咆哮结束」那一行推的值（`0x40f4fc`，走的是 `0x465450` 那支）；
用户听出来的那条是 `se_pldead01` ⇒ 本卡按用户口径放 **0x38**。若实跑觉得不对，改这一行即可。

### AL27. ★ 兽灵弹画错：TH17 shooter `anm` → `bullet.anm`，TH18 同字段 → `pl0X.anm`（2026-09-11）

**症状**：咆哮换 shot 组后，兽灵弹（狼的 7 发大弹 / 鹫的诱导）画成了 TH18 零售的纤细
64×16 普通自机弹，而不是 TH17 的 16×16 兽灵弹。

**根因**：

- TH17 shooter 记录 `+0x22` 的 `anm` 字段是在 **`bullet.anm`** 里取脚本：
  `pl00a` 狼 `anm=5`（`sprite337`）、鹫 `anm=6`（wrapper → `script5`）；
  `pl01a` 狼 `anm=8`（wrapper → `script7`）、鹫 `anm=11`（`sprite340`）。
  一手 dump：`local/vendor/thtk/build/thanm/thanm -l 17 local/th17.v1.00b/dat/bullet.anm`。
- TH18 播放同一字段时换成了自机的 **`pl0X.anm`**。零售 `pl00.anm` 的 script5 是
  64×16 普通自机弹（`sprite24..28`），不是 `sprite337`。
  一手 dump：`thanm -l 18 local/th18.v1.00a/dat/pl0X.anm`。
- 本卡原实现把 TH17 号原样填进追加 shooterset，且旧 `HYPER_ANM_MAP` 直接指向
  零售 `pl0X.anm` 的同号脚本，所以必错。

**修法（Option A）**：`assets/build_player_anm.py` 对四个 `pl0X.anm` 做纯追加：

- 追加 TH17 `bullet.anm` 的 **9 个 entry**（580 个 sprite），顺序、sprite 列表不变，
  每个 `spriteN` 改写为 `sprite(base+N)`；`base` = 该玩家原 entry 的 sprite 总数
  （`pl00/pl01/pl02/pl03 = 60/67/75/57`）。
- 追加 TH17 bullet 脚本。shooter 直接引用的四个号是 5/6/8/11；`script8` 里
  `scriptNew(script7)`，所以 `script7` 作为依赖一并复制。新脚本号按 TH17 原编号顺序
  连续排在原文件最后一个脚本之后；脚本内 `spriteN` 同样 `+base`，脚本号按旧→新映射：

| 目标文件 | 原最后 script | TH17 bullet 脚本 → 新脚本号 |
| --- | ---: | --- |
| `pl00.anm` | 40 | 5→41, 6→42, 7→43, 8→44, 11→45 |
| `pl01.anm` | 46 | 5→47, 6→48, 7→49, 8→50, 11→51 |
| `pl02.anm` | 29 | 5→30, 6→31, 7→32, 8→33, 11→34 |
| `pl03.anm` | 28 | 5→29, 6→30, 7→31, 8→32, 11→33 |

`append_shooterset.py` 的 `HYPER_ANM_MAP` 已改成指这些新号；`pl02/pl03` 没有 TH17
对应自机，继续借 `pl00a` / `pl01a` 记录，填各自 `pl0X.anm` 的本地新号。

**一手地址与对账**（任务给定地址；shooter 字段另由上述两作 dump 逐条读出）：

- `th17:0x4b2bf0` / `th18:0x4c9b80`。
- SHA-256：TH17 / TH18 九个同名贴图里 8 个一致；只有 `bullet/item.png` 不同
  （TH17 `06254532255ff0cd…` / TH18 `c69386db0b21a36a…`）。该 entry 不被
  5/6/7/8/11 使用（它们只用 `bullet/bullet1.png`，sha256 一致），追加 entry 仍用
  TH17 原图以保证对 TH17 `bullet.anm` 的逐项忠实。
- 构建后 `thanm -l 18 native/build/pl0X.anm` 可正常列出；新增 entry 为
  368/76/56/24/16/8/8/20/4 个 sprite，新增 script 为上述 5 个。

> ⚠ **本轮映射是错的**：TH17 `PlayerBullet__fire` 实际读 `anm+5`，所以这里把
> `anm` 同号脚本 5/6/7/8/11 当兽灵弹是错的（那些是普通主炮脚本）。修正过程、
> 实际脚本号与 TH18 原生绘制脚本见 §AL28；本节保留为第一轮失败记录。

### AL28. ★ 兽灵弹第二轮修正：TH17 `anm+5` + 脚本只画图、不带寿命（2026-09-11）

**症状（用户实跑）**：① 兽灵弹贴图仍错；② 射程/存在时间变短，弹会提前消失。

**根因 1（贴图）**：TH17 shooter 的 `+0x22` 不是直接索引 `bullet.anm`。
`PlayerBullet__fire` `th17:0x44a180` 调 `anm_init_copy_vm_from_loaded(..., anm + 5)`
（`th17:0x407230`，步长 `0x600`），所以 `pl00a` 狼/鹫 `anm=5/6` 实际播
`script10/11`（sprite339/340），`pl01a` 狼/鹫 `anm=8/11` 实际播
`script13/16`（sprite341/342）。上一轮按 `anm` 同号复制 `bullet.anm` script5/6/7/8/11，
实际画的是普通主炮 sprite337/338/340，所以必错。TH17 `hit5` 另硬编码 raw script8
（→ script7 → sprite338）当束端。

**根因 2（射程）**：上一轮原样复制 TH17 脚本，带 `posTime` 和 `+28: delete()`。
TH18 `Player__tick_bullets` `th18:0x45edb0` 每帧用 `bullet+8` 的 id
`AnmManager__get_vm_with_id` 查 VM；查不到就把整发弹的 active flag 清零，并关掉
`bullet+0xb0` 的伤害源。`delete()` 是删 VM、不是删弹实体，但这里两者绑定 ⇒
弹在 28 帧后一起消失，看起来就是射程变短。

**修法**：`assets/build_player_anm.py` 继续只追加 TH17 `bullet.anm` 的 9 个 entry
（sprite 号统一 `+base`），但追加脚本改成 TH18 原生最小脚本：slot5/6/8/11 分别
`layer(13)` + `type(1)` + `blendMode(1)` + `alpha(128)` + `scale(1.9f,1.9f)` +
`sprite(base+339/340/341/342)` + `stop()`；**不写** `posTime`/`delete`/其它位移。
slot7 是 hit5 的一次性束端特效：画 `sprite(base+338)`，`+28 delete()`。
`HYPER_ANM_MAP` 保持原有新号不变，只显式补上 slot7；`_map_hyper_row` 的 `hit_fx`
从旧 `anm_map[8]` 改指 slot7，避免把子弹本体脚本当束端。

**验证**：`python3 assets/build_player_anm.py` 重编四个 `pl0X.anm`，`--verify-only`
通过；`L.verify_rebuilt` 逐项确认零售 entry/script/贴图未变，追加脚本的 `stop()` 与
`sprite` 与计划一致，子弹槽无 `delete(`/`pos`/`posTime`/`move`。`thanm -l 18` dump
见 §AL27 的复现入口。**实跑验收**：灵梦狼/鹫、魔理沙狼/鹫看贴图是否为 16×16 兽灵弹，
弹能否飞到版面边缘自然消弹；买倒吊人/空白卡删掉后主炮槽是否还原。

### AL29. ★ 兽灵弹第三轮（源是 `pl0X.anm` 不是 `bullet.anm`）+ 咆哮正常退出音效 id（2026-09-11）

#### AL29-A 兽灵弹贴图：AL27/AL28 的源文件判断错误

**发现（一手反汇编）**：

- `th17:0x44a180` `PlayerBullet::fire` 在 `0x44a508` 取 `eax = [th17:0x4b77d0]`
  = `PLAYER_PTR`；`0x44a512` `mov ecx,[eax+0xc]` = 玩家 loaded anm 句柄。
- `Player__initialize` `th17:0x4463f0` 在 `0x4463fe`/`0x446409` 用
  `[0x4a214c + 自机*4]` 装载 `pl00.anm`/`pl01.anm` 到 `player+0xc`；
  因此句柄不是 `bullet.anm`。
- `0x44a535`–`0x44a574`：`movsx eax, word [shooter+0x22]; add eax,5;`
  `push eax; call anm_init_copy_vm_from_loaded`（`th17:0x407230`，脚本步长 `0x600`）。
  结论：TH17 实际播 **TH17 源 `pl0X.anm script(anm+5)`**；TH18 同字段直接索引
  目标 `pl0X.anm`。

**用户位置 → 源 sprite（TH17 `pl0X.anm` entry0、`player/pl0X/pl0X.png`）**：

| 案例 | 源脚本（`anm+5`） | 所选 sprite | 坐标 | 机械依据 |
| --- | ---: | ---: | --- | --- |
| 灵梦狼 | `pl00.anm` script10 | `sprite33` | `(128,192,126,64)` | 弹体类图里 `y+h` 最大（贴底 256），同级 `x+w` 最大（254）= 右下角 |
| 灵梦鹫 | `pl00.anm` script11 | `sprite38` | `(0,224,24,24)` | 底行最左；右侧 `sprite39/40/41` `(24/48/72,224,24,24)` 是 hit 动画，按用户口径排除 |
| 魔理沙狼 | `pl01.anm` script13 | `sprite42` + `sprite43` | `(128,192,64,32)` / `(192,192,64,32)` | 贴图 entry0 按唯一 `y` 升序分组，第 7 组 = `y=192`；组内最右两颗 |
| 魔理沙鹫 | `pl01.anm` script16 | `sprite29` | `(0,288,512,32)` | 仅两条 512 宽激光条（`sprite28 y=177` / `sprite29 y=288`），取 `y` 最大者 |
| 魔理沙鹫 hit5 束端 | `pl01.anm` script8（`th17:0x44c48d` 硬编码） | `sprite31` | `(80,192,32,32)` | 同一 `PLAYER_PTR+0xc` 源文件；TH18 slot7 `hit_fx` |

**修法**：`assets/build_player_anm.py` 不再追加 `bullet.anm`；改为把 TH17 源
`pl0X.anm` 的 **entry0** 纯追加到目标文件，`spriteN -> sprite(base+N)`。
四个目标 `base` = `60/67/75/57`；脚本号槽位顺序仍是 5/6/7/8/11，所以
`append_shooterset.py` 的 `HYPER_ANM_MAP` 不变。`pl02/pl03`（TH18 咲夜/早苗）
没有 TH17 对应自机，继续借 `pl00.anm`/`pl01.anm` 的源 sprite；因为追加 entry
用的是 TH17 原图，替身也拿到正确弹皮。

追加脚本的实际绘制 sprite（构建后 `thanm -l 18` 读取）：

| 目标 | slot5 | slot6 | slot7 hit | slot8 | slot11 |
| --- | ---: | ---: | ---: | ---: | ---: |
| `pl00.anm` | 93 (`33`) | 98 (`38`) | 91 (`31`) | — | — |
| `pl01.anm` | — | — | 98 (`31`) | 109/110 (`42/43`) | 96 (`29`) |
| `pl02.anm` | 108 (`33`) | 113 (`38`) | 106 (`31`) | — | — |
| `pl03.anm` | — | — | 88 (`31`) | 99/100 (`42/43`) | 86 (`29`) |

子弹脚本仍无 `delete()`/`pos*`/移动/`scriptNew()`/`jmp()`；slot7 保留一次性
`delete()`。`make check` 通过，`build_player_anm.py --verify-only` 回读确认
零售 entry/script/贴图逐项不变、追加贴图与 TH17 源 sha 一致。

#### AL29-B 咆哮正常退出音效：id 0x38 实际播 se_ch03

**发现（一手字节 + 反汇编）**：

- TH18 `SoundManager__play` 两个入口 `th18:0x476be0`/`0x476c70` 都是
  `movsx esi, word [base + id*0x14 + 0x0a]`（`0x4c9b8a`）并把该 word 写进
  slot+4（voice priority）。它不是 wav 选择器。
- wav 选择在 loader：`th18:0x4766b8` `edi=0x56c804`（slot 数组）、
  `esi=0x4c9b84`（cfg 行 `+0x04`）；`0x4766cf` 读 `[esi]` 并用
  `[eax*4+0x4b47a0]` 取 wav 文件名。即 **`id -> wav = u32[0x4c9b80 + id*0x14 + 0x04]`**。
- 行内 `+0x08` 的低 word 是音量（consumer `0x477611` `movsx eax, word [row+0x8]`），
  高 word = `+0x0a` = priority；旧代码 `se_volume_fix()` 写 `+0x0a` 只改 priority，不是音量。
> ⚠️ **2026-09-11 订正**：下面这三行按 `+0x04` 的结论是**错的**。语义自证（`0x05/0x08/0x0d/0x16`）表明
> **`+0x00` 才是 wav 号** ⇒ `0x38=se_pldead01`、`0x34=se_lgods4`、`0x43=se_release`、`0x3f=se_wolf`。
> 卡里已改回 `ANIMAL_SE_ROAR_END=0x38`、`ANIMAL_SE_FISH_ITEMS=0x34`（见 §AL35）。

- 逐行读表：`id 0x38`：`+0x00=55`、`+0x04=44` → **`se_ch03.wav`**；
  `id 0x43`：`+0x00=66`、`+0x04=55` → **`se_pldead01.wav`**；
  `id 0x3f`：`+0x04=51` → **`se_lgods4.wav`**。
  旧注释把 `+0x00` 当成 wav 号，所以把 `0x34/0x38` 当成 se_lgods4/se_pldead01，
  实际听到 se_don00/se_ch03。

**修法**：`native/cards/animal.c`
`ANIMAL_SE_ROAR_END` 改 `0x43`；`ANIMAL_SE_FISH_ITEMS` 一并改 `0x3f`（same
root cause，用户此前指名 se_lgods4）。`se_volume_fix()` 保留名字但注释/日志改为
priority fix；0x3f/0x43 的 priority 本来就是 100，正常情况下不再写不再打印。

**验证/部署**：

- `make check` 绿。
- `python3 tooling/check-docs.py` 绿。
- `make dist` + `MODKIT_DIR="/mnt/d/项目/幻想指南/18/th18_modkit" NO_GIT=1 make release`
  写入 6 处；部署目录里 `pl00..pl03.anm`、`pl00..pl03.sht`、`abcard.anm`、
  `ability.anm` 与 `native/build` 逐文件 `cmp` 相同。
- 部署 DLL md5 `57cb8930baeff6aaf5d0e3f4f0acea44`。
- `thanm -l 18 native/build/pl0X.anm` 与零售 dump 的前缀 entry/script 块逐字节一致；
  追加 entry 的 sprite 坐标见 AL29-A 表。



### AL30. ★ 鬼形兽第三轮修正：BossItem 掉落 / 符卡相位 / X 炸弹拦截 / 水獭光球（2026-09-11）

**问题**：id 117 的 boss 掉落与 TH17 `BossItem` 不等价；相位掉落分不清符卡 / 普通；咆哮中按 X 仍会放出原版炸弹；
水獭 3 光球只在屏幕中央重叠成一个。**范围**：`native/cards/animal.c`、`native/sdk.c`、`native/sdk/public.h`、
`native/sites.py`、`native/th18_card_expand.def`、`assets/ability/`（未改脚本，只重跑生成器核对）。

#### AL30-A BossItem 掉落：普通相位 2 资源灵 / 符卡相位 1 动物灵

**一手（TH17 v1.00b）**：

```bash
local/vendor/thtk/build/thecl/thecl -d 17 local/th17.v1.00b/dat/st01bs.ecl > /tmp/st01bs17.ecl
local/vendor/thtk/build/thecl/thecl -d 17 local/th17.v1.00b/dat/default.ecl > /tmp/default17.ecl
```

- `st01bs.ecl` `Boss2()`（普通 / 血条相位）结尾：
  `@BossItem(20, 30, 10);` + `@BossItem(19, 30, 10);`（dump 行 181–182）。
- `st01bs.ecl` `BossCard1()`（符卡相位）结尾：`@BossItem(33, 0, 0);`（dump 行 301）。
- `st01bs.ecl` `BossDead()` 结尾：`@BossItem(0, 40, 10);`（dump 行 705；主掉落 type 0，不发资源）。
- `default.ecl` `BossItem(A,B,C)`（dump 行 2–14）：`dropClear; dropExtra(1,$B); dropExtra(2,$C);
  dropArea; dropMain($A); dropItems` —— **一次调用一个主掉落**。
- 映射：item `20` → token 5（残机碎片灵）、`19` → token 4（炸弹碎片灵）、`33` → item 里 `rand%3+30` → token 1..3（随机动物灵）。
- 结论：普通相位 = `token 4 + token 5` 各 1；符卡相位 = 1 只随机动物灵。点鱼灵 `@BossItem(22)` 在 TH17 里是**另一个 site**，
  不与符卡结束共用，故本卡不再保留 87/13。

**TH18 接缝**：`BP_ce_enemy_drop` 挂 `th18:0x430510`（`Enemy__drop_items_and_notify_cards` 入口），每个 ECL `dropItems`
触发一次。当前 TH18 相位 ECL 的 `@BossItem` 事件（例如 `st01bs.ecl` `Boss2` 的 `@BossItem(17,10,10)`）对应 TH17 的一条相位
结算；`BossDead` 的 `@BossItem(16,10,10)` 对应真正死亡。实现：

- `on_enemy_drop_pre` 读 `zEnemy+0xc` 的 ECL 主上下文当前子程序名（`native/cards/lolk.c` 同款偏移
  `th18:0x0c` / `th18:0x1218` / `+0x8c`），名含 `"Dead"`（`BossDead` / `MBossDead`）→ **2 资源灵**；
- 其余相位事件按 `phase_is_spell` 锁存值分流：普通 → 2 资源灵，符卡 → 1 动物灵；
- 同一相位 / 同一死亡阶段用 `phase_drop_seen` / `death_dropped` 去重，TH18 多条 `BossItem` 不重发；
- `on_tick_2` 的 `phase_sig` 边界只在没有 drop 事件时补发；`st07bs BossDead` 这类无 `BossItem` 的死亡由
  「boss 消失 + 最后 ECL 子程序名含 Dead」兜底补 2 资源灵（`animal_phase_track`）。

证据日志：`wba_wc: boss death -> 2 resource spirits (4+5)`、`wba_wc: phase drop normal -> 2 resource spirits (4+5) (sig A -> B)`、
`wba_wc: phase drop spell -> 1 animal spirit (...)`。**待用户实跑**：一关内确认普通相位掉 4+5、符卡相位只掉 1 动物灵、
死亡不会双份。

#### AL30-B 符卡相位信号：`SPELLCARD+0x78` bit0 锁存

`CE_SPELL_FLAGS` = `th18:0x4cf2c0 + 0x78`；bit0 = 符卡进行中（`th18:0x42a320` 置 / `th18:0x42a780` 清，
见 `engine/chapter/th18/02-wave-signals.md` §1）。`animal_phase_track` 在当前 `phase_sig` 存续期间若 `ce_spell_active()`
为真就锁存 `phase_is_spell=1`；`phase_sig` 变化时用**旧相位**的锁存值决定掉落。边界帧不采新符卡位（新位属于新相位），
所以符卡结束 / 普通结束不会串。**待实跑**看 `phase drop spell/normal` 日志是否与 TH18 相位顺序一致（st01bs：Boss1 非符 →
BossCard1 符卡 → Boss2 非符 → BossCard2 符卡 → BossDead）。

#### AL30-C X 炸弹拦截：TH18 普通炸弹分支断点 `0x45c048`

**TH17 一手**：`player_is_trying_to_bomb` `th17:0x411d40` 在 `HYPER_FLAGS & 6 != 0` 时返回 0；
`engine/animal/th17/01` §5.2 已证咆哮就绪 / 霊撃期间 X 只走 `do_hyper_cancel`。
**TH18 一手** `objdump -d -Mintel --start-address=0x45be90 --stop-address=0x45caa0 local/th18.v1.00a/th18.exe`：
普通炸弹分支 `th18:0x45c03f` 调 `Bomb__can_bomb_and_deathbomb_check` `th18:0x420420`；
`th18:0x45c048` 原指令 = `f6 05 34 a4 4c 00 02`（`test BYTE PTR ds:0x4ca434,0x2`）；
`th18:0x45c051` = `call 0x420360`（`do_bomb`）；`th18:0x45c056` = 分支汇合点。

**修法**：新增 SDK 断点 `ce_bomb_gate` @ `th18:0x45c048`（7 字节，`sites.py` 生成、`BP_ce_bomb_gate` 导出为
`@23`），沿卡链表问 `on_bomb_gate()`。id 117 在 `ready || hypering` 时返回 1；handler 设
`regs->retaddr = 0x45c056` 并返回 0，**跳过 `call do_bomb`**。不咆哮时返回 0 / 无这张卡时放行原指令，
真实炸弹与无咆哮的死亡炸弹不受影响。首次拦截日志 `wba_wc: bomb suppressed while roaring`。

`th18_card_expand.def` 导出数 22 → 23；`native/Makefile` 的 `dllverify` 同步改为 23。**待实跑**：
咆哮中按 X 应只出 40 帧霊撃、B 数不减；非咆哮时 B 数正常减、炸弹正常放。

#### AL30-D 水獭 3 光球：位置每帧写、ready 也写

**症状**：只有一个水獭灵、屏幕中央、绕自身中心、不随自机。**根因**：`orb_ensure()` 只在 `hyper_frame()` 里调用，
而 `hyper_frame` 只在 `hypering` 且 `r > 1` 后运行；`ready` 期间三只 VM 都停在默认 `(0,0,0)` 且共用同一脚本 149，
自然重叠成一个，并被当成「绕自己转」。

**TH17 一手**：`FUN_00410030` `th17:0x410163` 把 3 个 orb 的 `pos` 每帧写成自机位置；三支脚本
`beast.anm` script `0x25`–`0x27`（37–39）初始 `rotate(0 / -2.0943952 / +2.0943952, 0, 同值)`、都
`angleVel(-0.06981317f, 0, -0.06981317f)`、`layer(13)`、`anchorOffset(48,64)`（靠自转形成绕自机轨道）。
**本作脚本 149** 已改成 `originMode(1)`（128×64 画布居中 80×64 图），因此 orbit 改由 C 位置写：
`player + polar(56, frame×-0.06981317f + i×120°)`，3 个 VM id 分开存 `s->orb[3]`。

**修法**：`orb_tick()` 在 `ready` 和 `hypering` 两态每帧调用；`orb_ensure()` 建 3 个不同 id 的 VM；
`orb_get_pos()` 统一算位置；`hyper_frame()` 的消弹判定复用同一公式。位置在 ECL 坐标（世界层 VM），
不是表面 px；脚本 `layer(13)` 与 TH17 一致。日志：`wba_wc: otter orbs spawn (vm a/b/c, radius 56, script 149, layer 13)`，
之后每 60 帧 `wba_wc: otter orbs pos (frame …, player x,y) vm a=(x,y) vm b=(x,y) vm c=(x,y)`。
**待实跑**：三只应绕自机、随自机移动；日志三 id 不同且三个坐标互差约 120°。

#### AL30-E 验证 / 部署

- `cd mods/th18.v1.00a/card-expand/native && make check`：主机单测 + `sites.py check` + `check-docs.py` 全绿。
- `python3 sites.py verify`：210 条 binhack 全部 `expected == exe 字节`。
- `MODKIT_DIR="/mnt/d/项目/幻想指南/18/th18_modkit" NO_GIT=1 make release`：成功；部署 DLL md5
  `f5e2978754f732b141b36fa9859bf709`（与 `native/th18_card_expand.dll` `cmp` 相同），
  `patch/th18.v1.00a.js` 与 `_255/th18.v1.00a.js` `cmp` 相同，`_255/files.js` 已重算。
- `native/build/ability.anm`、`abcard.anm`、`pl02/pl03.anm` 与部署目录 `cmp` 相同；本轮未改脚本 149 内容。


### AL31. ★ 大鹫灵命中后不消失 / 无碰撞动画：pl00 slot6 缺 `interruptLabel(1)`（2026-09-11）

**症状**：咆哮切到鹫组后，霊夢的 16×16 诱导弹打中敌人时不消失、没有命中动画；用户主观感受还包括
「似乎没伤害」。**范围**：只改 `assets/build_player_anm.py` 生成的目标 `pl0X.anm` 追加脚本；
`assets/sht/append_shooterset.py` 与 `native/build/sht/pl0X.sht` 的 EAGLE 字段不变。

#### 一手根因

- TH17 `pl00a.sht` EAGLE 行是 `func(init/tick/hit)=1/1/0`：命中时 `hit=0`，没有 shooter hit 函数，
  但这正是 TH17 原生行为 —— 命中收尾由引擎默认路径 + bullet VM 的 `interrupt(1)` 完成。
- TH18 的对应路径：
  - `th18:0x45f6a0` 是 damage source handler idx1。它先读 `bullet+0xf4`（shooter `+0x38` 的
    `func_on_hit` 指针）；为 0 时直接调 `th18:0x45f6f0`。所以 `hit0` 不是「无命中处理」。
  - `th18:0x45f6f0` 取 `bullet+8` 的 VM；若 `vm+0x5e0 != 0`，调用
    `table[0x4b4308 + vm[0x5e0]*4]`（即向 bullet 的 anm VM 发 interrupt）；随后写
    `bullet+0x8c=2`、`bullet+0x60 *= 0.125`、关掉 `bullet+0xb0` 的 damage source，
    并返回 `bullet+0x9c`。
  - `th18:0x45f0f0` 的命中结算把上面 handler 的返回值当作本次伤害
    （`fVar8 = 返回值`，再 `local_4c += fVar8`、`[ds+0x58] += [ds+0x54]`）。因此
    shooter `damage=20` → `bullet+0x9c=20` → 默认 handler 返回 20，数据层伤害不是 0。
- TH17 源 `pl00.anm` script11 在本体 sprite38 之后有：
  `interruptLabel(1); ... sprite(39); +10 sprite(40); +10 sprite(41); +10 delete();`
  （命中的 39/40/41 正是兽灵弹右侧三格 hit 动画）。TH18 零售同族脚本 `pl00.anm` script9
  也保留了同样的 `interruptLabel(1)` 结构。
- 本卡上一轮 `build_player_anm.py` 生成 pl00 slot6 时只画了本体 `base+38` 后 `stop()`，
  **把 interrupt(1) 整段丢了**。于是 `0x45f6f0` 发 interrupt 时 VM 没有任何 handler：
  不消失、不播 39/40/41、视觉上也没有碰撞反馈。

#### 修法

`assets/build_player_anm.py`：

- `SLOT_SOURCE_SPRITES["pl00"][6]` 从 `[38]` 改为 `[38,39,40,41]`，回读对账源 script11 的直接 sprite。
- slot6 的 TH18 原生脚本改为照 TH17 script11 重写：`-1:` 本体 sprite38 + `0:` 后
  `stop()`，随后 `interruptLabel(1)` 依次画 `base+39/40/41`，`+30` 帧 `delete()`。
- 校验器只允许 bullet slot 在 `interruptLabel(1)` 分支里出现 `delete()`；
  普通飞行路径仍禁止 `delete/pos/move/scriptNew/jmp`，避免再次缩短射程。

生成的 pl00 script42（pl02 借同一源，script31）样例：

```
script script42 {
-1:
    layer(13); ... sprite(sprite98); scale(1.5f, 1.5f);
0:
    nop(); stop();
    interruptLabel(1);
    ...
    sprite(sprite99);
+10: // 10
    sprite(sprite100);
+10: // 20
    sprite(sprite101);
+10: // 30
    delete();
}
```

#### 数据对账（未改 .sht）

`python3 assets/sht/append_shooterset.py` 重新生成后，EAGLE 槽 `0x24..0x27` 逐行仍是：

- `pl00/pl02`：`rate=3 dmg=20 hw/hh=16/16 mode=1 anm=42/31 func=1/1/0/0 hit_fx=0`；
- `pl01/pl03`：`rate=15 dmg=80/60/60/50/50 hh=34 mode=2 opt=0/16/32/48/64 anm=51/33
  func=2/2/0/2 hit_fx=49/31`。

即字段没有改：修复不是改 `mode`/`damage`/`hit_fx`，而是补回 TH17 原本由 `anm` 脚本承担的
命中 interrupt。若实跑后仍只有「伤害数字不动」而没有碰撞反馈，下一步才在 `th18:0x45f6a0`
入口和 `th18:0x45f0f0` 的 handler 返回值处加日志；当前一手路径显示 damage=20 会进入结算。

#### 验证 / 部署

- `python3 assets/build_player_anm.py` / `--verify-only`：四个 `pl0X.anm` 追加脚本与计划一致，
  零售 entry/script/贴图逐项不变，`pl00 script42` / `pl02 script31` 含 `interruptLabel(1)` + 三段 hit sprite + `delete()`。
- `python3 assets/sht/append_shooterset.py`：四文件 23 组不变，追加 17 组正常。
- `cd native && make check` 全绿。
- `MODKIT_DIR="/mnt/d/项目/幻想指南/18/th18_modkit" NO_GIT=1 make release`：
  `pl00.anm`/`pl02.anm` 与 `native/build` `cmp` 相同，四个 `pl0X.sht` / 另外两个 `pl0X.anm` 亦相同。
- 部署 DLL：`/mnt/d/项目/幻想指南/18/th18_modkit/mods/th18_card_expand.dll`
  md5 `f5e2978754f732b141b36fa9859bf709`（与 `native/th18_card_expand.dll` 相同）。

### AL32. ★ 水獭 3 光球初始角度错：`anchorOffset` 被上一轮的 C 侧摆圆方案丢掉（2026-09-11）

**症状**：用户实跑「三个水獭灵的转圈对，但初始角度不对；看 `beastot.png` 不旋转时，子机应在其右下角附近」。
上一轮把 3 个 VM 按 `player + polar(56, frame×-0.06981317f + i×120°)` 摆圆，同时脚本 149 把贴图
居中补到 128×64、删掉 TH17 原有的 `anchorOffset(48,64)`，并且三个 VM 都从 `rotate(0)` 起转
（C 没有写 0 / -120° / +120° 初相）；于是圆在走、贴图自己的转轴/初相不对。

#### 一手 ground truth（TH17 v1.00b）

- `beast.anm` entry10 = `bullet/beastot.png`；`thanm -l 17` 给出 sprite70 矩形
  `(x=0,y=0,w=80,h=64)`。TH17 零售 `beastot.png`、本机提取图
  `local/th17.v1.00b/anm/beast/bullet/beastot@beast@10.png`，以及用户
  `D:\SteamLibrary\...\th17\data\ANM\bullet\beastot.png` 像素一致。
- 贴图测量（TH17 零售图；本机 2026-09-11）：alpha bbox（含端点）**x=1..78、y=5..50**，
  alpha 质心 `(29.65,24.56)`；右下象限 `x≥40,y≥32` 是 **0 像素**。
  所以用户「不旋转时子机待在图片右下角」不是描述绿色图元质心，而是说
  **VM/转轴应落在图片右下角附近**：图挂左上、绕右下角自转才是 TH17 的原样。
- 如果硬把 alpha 质心当成「内容点」来算相位：相对原 pivot `(88,96)` 是
  `dx=-58.35, dy=-71.44`，`r=92.24`，引擎角 `atan2(dy,dx) = -129.24°`
  （标准数学 y-up 为 `+129.24°`），明显不是「右下角」；这说明旧的 C 侧 `φ0` 方案没有唯一解，
  本轮直接改用 retail 的 pivot 自转模型。
- TH17 script37–39（`0x25`–`0x27`）原文：

  ```text
  -1:
      layer(13);
      randMode(1);
      type(1);
  0:
      iset($I0, 0);
      sprite(sprite70);
      anchorOffset(48.0f, 64.0f);
      alpha(0);
      alphaTime(10, 0, 255);
      rotate(0.0f, 0.0f, 0.0f);   // 另两支为 -2.0943952 / +2.0943952
      angleVel(-0.06981317f, 0.0f, -0.06981317f);
      ...
  ```

- `anchorOffset` opcode 436 的 handler 把两个参数原样写进 `vm+0x80/+0x84`（TH17 `zAnmVmPrefix` 字段表；
  TH18 handler `th18:0x47a43a`、渲染 `th18:0x47ed50` 同样是 `corner = sprite_size × coeff − anchor_offset`）。
  取默认锚心，pivot 在源图的 `(w/2 + dx, h/2 + dy)`。
- `FUN_00410030` `th17:0x410030` 只负责生成 3 个 orb VM 并存 id（`0x410163` 处写 `manager+0x44+i*4`）；
  每帧位置由 `0x40f23c` 段写：`movq [vm+0x5e4], player.xy`，再写 z。即 **VM 在自机位置**，
  不是 C 摆圆。
- 同段 `0x40f24f` 读 `vm+0x40` = `rotation.z`，减 `0x4a3b1c`（`0.34906584f` = 20°）后，
  用 `0x42c00000`（`96.0f`）调 `0x4113c0`（`out = polar(angle, radius)`）得到水獭光球的
  **消弹/伤害判定点**：`player + polar(rotation.z − 0.34906584f, 96.0f)`。
- 80×64 原图 + `anchorOffset(48,64)` 的画布关系：pivot 在源图 `(40+48,32+64)=(88,96)`；
  图左上相对 VM 为 `(-88,-96)`，alpha bbox 相对 VM 约 x∈[-87,-10]、y∈[-91,-46]。
  本作 ORDER 把图居中补到 128×64（paste x=24）：原 pivot `(88,96)` → 画布 `(112,96)`，
  而 `(128/2+48,64/2+64)` 正好也是 `(112,96)` ⇒ **同一锚点数值仍然等价**。

#### 修法

- `assets/ability/scripts/149_otter_orb.anm.txt`：补回 `anchorOffset(48.0f, 64.0f);`；
  128×64 居中补边不改写锚点；注释里写明补边等价性。
- `native/cards/animal.c`：
  - VM 每帧 pos 写自机位置（不再由 C 摆 56 px 圆）；
  - `orb_ensure()` 生成时写 TH17 初相 `rotation.z = 0 / -120° / +120°`（`ce_anm_set_rotation`），
    之后脚本 `angleVel` 自己累加；
  - 消弹点改为读实际 VM 的 `rotation.z`（`vm+0x44`），用一手公式
    `player + polar(rotation.z − 0.34906584f, 96.0f)`；半径 `24 px` 不变；
  - 每 60 帧日志加 `rot=` 与 `hit=` 便于实跑核对，spawn 日志打印 `orbit r 96` 和初相。

#### 判定 / 备选

- **采用**：TH17 的「VM 在自机位置 + `anchorOffset(48,64)` 自转成轨 + 初相 0/∓120°」。
  这是 retail 脚本 + `0x40f23c` 段共同坐实的唯一写法。
- **没有采用**：保留 C 摆圆 r=56，只在 C 侧加一个由图片内容算出的 `φ0`。原因是实测
  右下象限为空，`φ0` 无法从绿色图元唯一决定；而且 TH17 的 damage 公式给的是 r=96、相位
  `rotation.z−20°`，并非图片质心轨道。
- 若用户下一轮实跑觉得视觉圆比原版大/小，再按眼调 art 缩放或 C 的轨道倍率；本轮先以
  一手 TH17 写法为准。

#### 验证 / 部署（2026-09-11）

- 未重跑 `assets/ability/make_animal_art.py`：本轮没有改零售切图/补边；脚本 149 是手写文件，
  直接由 `../assets/build_ability.py` 编入 `native/build/ability.anm`。
- `cd mods/th18.v1.00a/card-expand/native && make check` 全绿（主机单测 + `pytest` + sites + DLL ABI + check-docs）。
- `MODKIT_DIR="/mnt/d/项目/幻想指南/18/th18_modkit" NO_GIT=1 make release` 成功；
  `cmp`：`ability.anm`、`abcard.anm`、`pl00..03.anm` 与 `native/build` 全部相同。
- DLL：`native/th18_card_expand.dll` = `<modkit>/mods/th18_card_expand.dll`
  md5 **`06c88611892f9e104d2b6483e16ea0b3`**；`bash local/sync_modkit_dll.sh` 后
  `mods/`、`thcrap/bin/mod_*`、`thcrap/repos/.../th18_card_expand.dll` 四份同 md5（本轮未被游戏锁）。
- 未做 git commit / push。

### AL33. ★ 魔理沙大鹫灵闪退 + 狼灵无爆炸：兽灵脚本根本不用追加，零售 `pl0X.anm` 里逐字节就有（2026-09-11 四修）

> §AL32 已被并发会话的「水獭光球」占用，本节接在它后面。

**症状**（用户实跑，魔理沙 `pl01`）：① 大鹫灵咆哮直接闪退；② 狼灵咆哮命中无爆炸特效
（像灵梦大鹫灵修前）；③ 命中点画出魔理沙自机图。

#### 一手根因：AL27–AL31 的「TH18 零售 pl0X.anm 没有 TH17 兽灵脚本/贴图」前提是错的

- TH17 `PlayerBullet__fire` `th17:0x44a180` 在 `0x44a535`–`0x44a539` 做
  `movsx eax, word [shooter+0x22]; add eax, 5`，实际播 **`pl0X.anm script(anm+5)`**：
  `pl00` 狼 `anm5→script10`、鹫 `anm6→script11`；`pl01` 狼 `anm8→script13`、鹫 `anm11→script16`；
  `hit5` 硬编码同文件 `script8`（`th17:0x44c462`；TH18 改读 shooter `+0x26`）。TH18 同字段直接读
  `pl0X.anm script(anm)`（`th18:0x45e733`），`copy_vm` 见 `th18:0x407420`（步长 `0x60c`，脚本表 `+0x10c`）。
- **TH18 零售 `pl00.anm` / `pl01.anm` 的这几支脚本与 TH17 逐字节一致**（`thanm -l` 的 script 块
  sha256 前 16）：

  | 文件 | script8 (hit5) | script10/13 (狼) | script11/16 (鹫) |
  | --- | --- | --- | --- |
  | `pl00.anm` | `75f75428a7a09035` | `c6547c6dc156c475` | `a050f09e1b1502b9` |
  | `pl01.anm` | `73433aa9adba2f6e` | `cac1dc0c7a524788` | `076d52bd142f438d` |

  entry0 的 spec（`width/height/THTX/sprite` 矩形）也与 TH17 相同；`player/pl01/pl01.png`
  的 md5 在三处一致：`10ff96a027419498baceb70ca199dd23`（TH17 源 / TH18 零售 / 上一轮的追加副本）。
- 结论：**追加 entry0 只在文件里塞了一份重复贴图**；真正的伤害来自「为了满足『脚本不含
  `scriptNew`/`jmp`』的不变式，把 TH17 脚本抄简」：
  - `pl01` 鹫 TH17 `script16` 主路径 = `scriptNew(script7)` + `scrollX/stop`，命中/被挡靠
    `interruptLabel(1/2/3)`；上一轮生成的 `script51` 把 `scriptNew(script7)` 与**全部**
    `interruptLabel` 删掉。
  - `pl01` 狼 TH17 `script13` 主路径 = `offset76: sprite42; +4 sprite43; +4 jmp(offset76,0)`，
    命中 `interruptLabel(1)` 里 `scriptNewPos(script14,…)` 爆 3 个碎片（`script14` = 命中动画）；
    上一轮生成的 `script50` 把整段命中删掉 ⇒ 用户看到的「无爆炸」。
  - `interruptLabel` 缺失时，引擎命中 interrupt（`th18:0x4787ef` 起）在脚本里找不到标签，
    会把 pending 清掉并从当前指令继续；叠加被删掉的 `scriptNew` 子 VM，是「命中点画出奇怪图元 /
    自机图」的最合理解释。

#### 崩因 —— blocker policy：final file 里**没有**越界 sprite，两条最佳假设

- 旧 build（备份 dump）：`pl00/pl01/pl02/pl03.anm` = `106/120/121/110` sprites、
  `46/52/35/34` scripts。`pl01` 追加脚本是 `script50`（`sprite109/110`）与 `script51`（`sprite96`）；
  `sprite96/98/109/110` **全部 `< 120` 且都在 entry14 里**，sht 指的 `anm=50/51`、`hit_fx=49`
  也都存在 —— **final built file 里没有越界 sprite / 不存在的 script**。所以「越界 sprite」不是本
  build 的直接一手证据（逐脚本穷举见下）。
- **H1（主）**：真 `script16` 是 **mode=2 激光**：主路径 `scriptNew(script7)` 生「束根光斑」子 VM，
    同时带 `interruptLabel(1/2/3)`。抄简成 `script51` 后，激光引擎 `hit2`（`th18:0x460320`）
    每帧会 `interrupt_tree(vm,2)`（命中前）并在命中时建 `hit_fx` VM（`sh+0x26`）；VM 结构不完整
    ⇒ 闪退。零售同类激光（`pl01.anm script6` / `ability.anm script3`）都带 `scriptNew` + interrupt。
- **H2**：追加 entry0 把 `pl01` 的 sprite 表从 67 顶到 120、脚本从 47 顶到 52。loader 虽按所有
    entry 求和分配（`th18:0x48663c` 起做 `sum += [entry+4]/[entry+6]`，`0x4866b7` 起 malloc），
    但 96..119 这段高编号 sprite 只有这一份非零售 entry 在用，风险不必要。
- 本次修法把 H1、H2 **一起移除**（不追加、直接用真脚本），无论哪条成立都不会再崩。

#### 修法

- `assets/build_player_anm.py` 重写：**不再追加**。把零售
  `local/th18.v1.00a/dat/pl0X.anm` 原样拷到 `native/build/pl0X.anm`，并硬校验（任一失败非 0 退出、
  不产出文件）：
  - 目标脚本存在；每个脚本每个 `spriteN` < 该文件 sprite 总数且确实存在；
    `scriptNew(N)`/`scriptNewPos(N,…)` 的目标脚本也存在；
  - `pl00/pl01` 的兽灵脚本与 TH17 逐字节一致（不等就报错，绝不再抄简）；
  - 会飞的主弹脚本（`pl00` 10/11、`pl01` 13/16）主飞行路径（第一个 `interruptLabel` 之前）
    不得含 `delete()` —— 防止再出现 §AL28 的「射程缩短」；
  - 打印每文件 sprite/script 总数与用到的 sprite 号。
- `assets/sht/append_shooterset.py` 的 `HYPER_ANM_MAP`：
  - `pl00/pl01` → `{5:10, 6:11, 7:8, 8:13, 11:16}`（直接用零售兽灵脚本；`hit_fx=8`）。
  - `pl02`（咲夜）/`pl03`（早苗）没有 TH17 对应自机 ⇒ **安全回退 identity**
    `{5:5, 6:6, 7:7, 8:8, 11:11}`（各自零售普通自机弹；不崩、不来自机图；**视觉目标未达成**，
    明确登记，等以后要做时再按 pl00/pl01 的「真脚本」方式移植）。
- `assets/check_player_anm_bounds.py`（新增，独立复现）：`thanm -l 18` 回读四个 build；全脚本
  sprite 越界穷举 + `sht(0x1e..0x27)` 的 `anm`/`hit_fx` 必须落在本文件 + 会飞脚本主路径无 `delete`。

#### 验证（命令与输出）

`python3 assets/build_player_anm.py`：

```text
  pl00: 60 sprites, 41 scripts, 不改动；TH17 兽灵脚本 5->10 6->11 7->8 8->13 11->16；用到 sprite [31, 32, 33, 38, 39, 40, 41, 44]
  pl01: 67 sprites, 47 scripts, 不改动；TH17 兽灵脚本 5->10 6->11 7->8 8->13 11->16；用到 sprite [29, 31, 34, 35, 36, 37, 38, 39, 40, 41, 42, 43]
  pl02: 75 sprites, 30 scripts, 不改动；安全回退 5->5 6->6 7->7 8->8 11->11；用到 sprite [24, 26, 27, 28, 29, 30, 35, 36, 37, 38, 39, 40, 41, 42, 43, 44, 45]
  pl03: 57 sprites, 29 scripts, 不改动；安全回退 5->5 6->6 7->7 8->8 11->11；用到 sprite [24, 25, 26, 27, 28, 29, 30, 31, 32, 33, 34, 37, 40, 41, 42, 43]
verify: pl00/pl01 的兽灵脚本与 TH17 逐字节一致、直接用零售脚本；pl02/pl03 安全回退到零售普通自机弹；pl0X.anm 零改动，无越界 sprite / script。
```

`python3 assets/check_player_anm_bounds.py`（独立回读 `thanm -l 18`；旧 dump 里 pl01 是 120 sprites / 52 scripts，现在 67 / 47）：

```text
pl00.anm: sprites=60 (ids 0..59), scripts=41
  script10: sprite refs [33] -> OK (< 60); 主路径 delete=no
  script11: sprite refs [38, 39, 40, 41] -> OK (< 60); 主路径 delete=no
  script8: sprite refs [31, 32] -> OK (< 60); 主路径 delete=no
  sht(0x1e..0x27): 48 条 shooter 的 anm/hit_fx 全部落在 pl00.anm 的 script 表内
pl01.anm: sprites=67 (ids 0..66), scripts=47
  script13: sprite refs [42, 43] -> OK (< 67); 主路径 delete=no
  script16: sprite refs [29] -> OK (< 67); 主路径 delete=no
  script8: sprite refs [31] -> OK (< 67); 主路径 delete=yes
  sht(0x1e..0x27): 40 条 shooter 的 anm/hit_fx 全部落在 pl01.anm 的 script 表内
pl02.anm: sprites=75 (ids 0..74), scripts=30
  sht(0x1e..0x27): 48 条 shooter 的 anm/hit_fx 全部落在 pl02.anm 的 script 表内
pl03.anm: sprites=57 (ids 0..56), scripts=29
  sht(0x1e..0x27): 40 条 shooter 的 anm/hit_fx 全部落在 pl03.anm 的 script 表内
OK: sprite 全在界内；sht 的 anm/hit_fx 全部存在；兽灵脚本主路径无 delete()。
```

`python3 assets/sht/append_shooterset.py`：四文件 23 组逐字节不变、追加 17 组；
兽灵行现在是 `pl00.sht` 狼 `anm=10`/鹫 `anm=11`，`pl01.sht` 狼 `anm=13`/鹫 `anm=16`/`hit_fx=8`
（`pl01` 的 sht 行其它字段未改）。

- `cd native && make check` 全绿（主机单测 + `pytest` + `sites.py check` + DLL ABI + `check-docs.py`）。
- `MODKIT_DIR="/mnt/d/项目/幻想指南/18/th18_modkit" NO_GIT=1 make release` 成功；
  `cmp`：部署的 `pl00..03.anm`（= 零售文件）、`pl00..03.sht`、`abcard.anm`、`ability.anm` 与
  `native/build` 全部相同；`pl0X.anm` 与 `local/th18.v1.00a/dat/pl0X.anm` 也逐字节相同。
- DLL：`native/th18_card_expand.dll` = `<modkit>/mods/th18_card_expand.dll`，
  md5 **`06c88611892f9e104d2b6483e16ea0b3`**；`bash local/sync_modkit_dll.sh` 后四份 DLL 同 md5
  （未被游戏锁）。
- 未做 git commit / push。

**待实跑**：魔理沙（pl01）狼/鹫咆哮应：狼命中爆 3 个碎片（`script13→script14`）、鹫激光正常
出束不闪退、命中点不再出自机图；灵梦（pl00）同。咲夜（pl02）/早苗（pl03）本轮是**安全回退**，
画的是各自普通自机弹，不是兽灵专属弹幕。


### AL34. ★ 水獭 3 光球没有消弹：TH17 在 ready 段消，不在 40 帧霊撃里；半径是 28 不是 96（2026-09-11 五修）

**症状**：用户实跑「水獭灵咆哮没有消弹」。AL32 的视觉（VM 留自机位置 + `anchorOffset(48,64)` +
初相 0/∓120°）已经修对，但消弹没跑。

#### 一手根因

- C 侧原 `hyper_frame()` 的 orb cancel 只在 `s->core.hypering`（按 X/C 后的 40 帧霊撃）时运行；
  满槽**自动咆哮的就绪期**（`ready`）只 `orb_tick` 摆位置，从不发 cancel。
- TH17 `GoastManager::on_tick` `th17:0x40ee70`：`0x40f0b9 test DWORD PTR ds:0x4b5ac4,0x4` /
  `jne 0x40f394`，即 `HYPER_FLAGS & 4`（霊撃中）时**跳过** `0x40f0b9`–`0x40f378` 的 ready 段。
  3 光球循环在 ready 段 `0x40f200`–`0x40f372`，只在 `HYPER_FLAGS & 4 == 0` 时每帧跑。
- `do_hyper_cancel` `th17:0x40f6d0` 在 `0x40f7f7`–`0x40f827` 对 `hyper_type==2` 立刻删掉 3 个 VM；
  所以进入 40 帧霊撃后 3 光球不应再存在。
- 判定点：`0x40f24f movss xmm0,[ecx+0x40]`（TH17 `zAnmVmPrefix.rotation.z` 是 `+0x38..+0x40`；
  TH18 对应 `+0x3c..+0x44`），`0x40f254 subss xmm0,[0x4a3b1c]`（`0.34906584f` = 20°）；
  `0x40f26c mov DWORD PTR [esp+4],0x42c00000`（96.0f）后 `call 0x4113c0`（`polar=(cos,sin)*radius`）。
  ⇒ 判定点 = `player + polar(vm_rotation.z − 20°, 96)`，不是 VM 位置。
- **消弹半径**：`0x40f289 movss xmm2,[0x4a3ce0]` = **28.0f**；`0x40f291 push 1` 后
  `call 0x419df0`（BulletManager 半径消弹，`mode=1`）；紧接着 `0x40f31d` 起
  `push 5; push 2; push point; call 0x449c80`（`Player::create_damage_source`，life=5、dmg=2、
  `xmm2=28`、`xmm3=0`）。
- `th17:0x40f3a1` 是 `[0x4b5aa8] <= 0` 后的「咆哮超时释放」分支，不是 otter orb 消弹段。
  `engine/animal/th17/01` §5.3 旧文把这段误记成霊撃中；AUDIT §AL32 又把 `0x42c00000`（96，
  只是判定点离自机的距离）误记成消弹半径。二者本轮按反汇编订正。

#### 修法

- `native/cards/animal.c`：
  - 常量拆开：`ANIMAL_ORB_ORBIT_RADIUS=96.0f`、`ANIMAL_ORB_CANCEL_RADIUS=28.0f`
    （`th17:0x4a3ce0`）、`ANIMAL_ORB_CANCEL_MODE=1`、damage 2 / life 5。
  - `orb_ensure()` / `orb_tick()` 只在 `s->core.ready` 且未霊撃时存在；`hyper_begin()` 对
    `hyper_type==2` 调 `orb_kill(s,1)`（照 `th17:0x40f7f7`）。
  - 新增 `orb_cancel()`，在 `on_tick_2` 的 ready 分支 `if (!paused)` 后每帧跑：
    3 个 VM 各算 `hit=player+polar(rot−20°,96)`，然后 `ce_cancel_radius(hit,28,9999,1)` +
    `ce_damage_rect(hit,0,5,2,56,56)`（TH17 是圆形半径 28 的源；TH18 沿用本卡既有的矩形近似）。
  - 删除 `hyper_frame()` 里的 3 orb 块，避免在 TH17 已删 orb 的霊撃期继续跑。
  - 日志：`on_ready` 每次咆哮加
    `wba_wc: otter orb cancel armed/not armed (type …, 3 points, r=28, mode=1, state=ready, orbit=96)`；
    每 60 帧 `wba_wc: otter orbs pos` 增加 `cancel r/mode/orbit` 字段。
- 视觉模型不改：VM 每帧写自机位置，rotation.z 由脚本 `angleVel` 累加，C 只在 spawn 时写初相。

#### 判定 / 备选

- **采用**：TH17 ready 段每帧 3 点消弹 + 小伤害源；进霊撃删 orb。这是反汇编可逐句坐实的原版顺序。
- 若实跑日志已有 `otter orb cancel armed (r=28, orbit=96)` 但弹幕仍不死，下一步才查 TH18
  `CE_FN_BULLET_CANCEL_RADIUS_AS_BOMB` 的 mode 语义或 `ce_anm_set_rotation` 的初始化时序；
  在日志证明之前不再改视觉公式。

#### 验证 / 部署（2026-09-11）

- `cd native && make check`：主机单测 + `pytest` + sites + DLL ABI + `tooling/check-docs.py` 全绿。
- `MODKIT_DIR="/mnt/d/项目/幻想指南/18/th18_modkit" NO_GIT=1 make release`；`cmp` 部署的
  `ability.anm`/`abcard.anm`/`pl0X.anm`/`pl0X.sht`/`th18.v1.00a.js`/`cards.js` 与 `native/build`、
  `dist` 相同；部署 DLL md5 **`21a468667b96ac667fec4b14bdaef9cd`**。
- `bash local/sync_modkit_dll.sh`：四份 DLL（native、`mods/`、`thcrap/bin/`、
  `th18_card_expand_255/`）同 md5 `21a468667b96ac667fec4b14bdaef9cd`；本轮**未被游戏锁**。
- 未做 git commit / push。

### AL36. ★ 魔理沙大鹫灵 `mode=2`/`opt_slot=0` 主炮空指针闪退：pl01 兽灵组整组安全回退（2026-09-11 六修）

> 编号说明：当前文件里 `§AL35` 已在声音语义订正段落被引用（见 §AL29-B 末尾），
> 本节按下一个可用序号记为 `§AL36`；实际插在 §AL34 之后。用户实跑反馈：
> 「魔理沙大鹫灵激光依旧闪退；魔理沙狼灵子弹爆炸后会在原地产生一个魔理沙自己；
> 大鹫灵应该对了」。

#### 用户问题 (a)：大鹫灵激光闪退 —— 根因 ✅

- 源数据：`local/th17.v1.00b/dat/pl01a.sht` 的 EAGLE 组 = set `0x10`（set16）。set16 row0
  文件偏移 `0x1610`（组相对 `0x14e0`），字段一手解码：
  `+0x20 opt_slot=0`、`+0x21 mode=2`、`+0x22 anm=11`、`+0x14 angle=-π/2`、
  `+0x0c/+0x10 hit=(0,34)`、`+0x28/+0x2c/+0x34 func=(4,5,5)`。
  当前 `HYPER_FUNC_MAP` 把它翻成 TH18 `init2/tick2/hit2`（`th18:0x45fc20/0x45fc80/0x460320`）。
- **闪退点在 `tick2` 第一段，与 `hit_fx=8` 或脚本 16 无关**：
  - 主炮路径 `Player__tick_shooting_state` `th18:0x45ec41`–`0x45ec5f` 为每行算 option 指针：
    当 `opt_slot` 的低半字节为 0（本例 `+0x20=0`）时，`th18:0x45ec48 xor eax,eax` ⇒
    压入 **NULL** 作为 option。
  - `PlayerBullet__create` `th18:0x45e3ae` 把该指针存进 `bullet+0xe4`。
  - `tick2` `th18:0x45fcc8`–`0x45fcd3` 无条件：
    `mov eax,[edi+0xe4]; movd xmm1,[eax+0x5c]; movd xmm2,[eax+0x60]`。
    `eax=0` ⇒ 空指针解引用，必闪退。
  - TH17 的对应 `tick5` `th17:0x44bba0` 在 `th17:0x44bbd3` 有
    `test al,0xf; je 0x44bc3b`，低半字节 0 时用自机座标 `[player+0x610]`，
    **TH18 `tick2` 没有这条自机兜底分支**。
- 一手对照：TH18 零售主炮里的 `mode=2` 行**全部** `opt_slot` 低半字节 1..4：
  `local/th18.v1.00a/dat/pl01.sht` set `0x01` row2 文件 `0x2f4` opt=1，
  set `0x02` row2/3 文件 `0x40c/0x468` opt=1/2，
  set `0x03` row2..4 文件 `0x580/0x5dc/0x638` opt=1/2/3，
  set `0x04` row2..5 文件 `0x750/0x7ac/0x808/0x864` opt=1..4；
  它们都是 `anm=6`、`hit_h=16`、`hit_fx=8`、func `2/2/2`。唯一 `opt=0, mode=2` 的
  零售激光是 set `0x0b`（迷你八卦炉；`pl00..pl03` 同构，文件 `0x1068`，`anm=3`、
  `hit_fx=5`），但它只走 **装备卡子机路径** `Player__tick_shooters_for_ability_card`，
  该路径始终传入有效 option 指针；主炮路径不能直接搬这条。
- **正确 TH18 翻译**：主炮路径的 `mode=2` 只能从 option 槽 1..4 出束；TH17 的 row0
  “自机本体激光”在 TH18 主炮路径没有 data-only 等价物。要么改成 option 1..4（失去自机本体）、
  要么补一条 C 侧主炮自机激光分支（超出本次 data-only 安全修范围）。因此本次不实装，
  先做整组回退。

#### 用户问题 (b)：狼灵爆炸处“一个魔理沙自己” —— 证据与两个假设 🟡

- `thanm -l 18 -m local/vendor/eclmap/anmmap/v8.anmm local/th18.v1.00a/dat/pl01.anm` 一手：
  - `script13`（TH17 WOLF `anm=8` 实际播放的脚本，`th17:0x44a535 add eax,5`）主路径
    `sprite(sprite42); sprite(sprite43)`；`case(1)` 分支三次
    `scriptNewPos(script14, %F1, %F2)`（`case(1)` = `interruptLabel(1)`）。
  - `script14` 引用 `sprite44..sprite51`。
  - `script8`（TH17 `hit5` 束端，TH18 `hit_fx`）只引用 `sprite31`。
  - entry0 坐标：`sprite42=(128,192,64,32)`、`sprite43=(192,192,64,32)`；
    `sprite44..51` 全在 `y=257`、`30×30`、`x=1,33,65,97,129,161,193,225`；
    `sprite31=(80,192,32,32)`。
- 这些 sprite 都在 `pl01.anm` entry0（`player/pl01/pl01.png`）的表里，但**不是自机角色帧**：
  角色帧是 `sprite0..23`（`30×46`、`y=1..97`，黑/白/紫主体）；`sprite44..51` 是
  **橙白命中/爆散帧**（主色 `RGB(244,71,45)`+白），同一组 8 帧也由 `script12` 使用，
  即普通魔理沙 option 弹命中时的爆炸；`sprite42/43` 是那发大弹本体。
- 源码对拍：`local/th17.v1.00b/dat/pl01.anm` 与 TH18 零售 `pl01.anm` 的
  `script13/script14` 文本逐字节一致；两边 entry0 贴图 `player/pl01/pl01.png`
  md5 都是 `10ff96a027419498baceb70ca199dd23`。因此“TH17 有、TH18 没有”的替身解释不成立。
- `hit6→hit4` 本身不改变视觉：TH17 `hit6 th17:0x44d390` 在 `0x44d420` 调
  `AnmManager__interrupt_tree th17:0x476510`（id 1）；TH18 `hit4 th18:0x460cb0` 在
  `0x460d24` 调 `AnmManager__interrupt_tree th18:0x488be0`（id 1）。两者都会执行
  `script13` 的 `interruptLabel(1)` → `script14`。没有证据要求把爆炸改指
  `bullet.anm`/`effect.anm`。
- **H1（主）**：当前数据没有“自机角色帧”引用；用户看到的“魔理沙”很可能是
  `sprite44..51` 这组橙白爆散帧的外观被误认，或来自旧 build 的追加脚本缓存。
  **下一个实验**：在命中 VM 创建处断 `th18:0x407420`（或 `0x429ad0`），记录
  `vm+0xe0` 的 anm 基址、entry 号、script id 与当前 `spriteN`；与 `pl01.anm` entry0
  基址和 `sprite0..23/44..51` 对照。若记录到 `sprite0..23`，再查 `scriptNewPos` 解析。
- **H2**：若确实出现角色帧，最可疑的是命中 interrupt 被作用在**错误的 anm 上下文**：
  主炮 `PlayerBullet__create` 用的是 `player+0x10`；如果某条装备卡路径把
  `player+0x10` 留成 `ability.anm` 而未还原，`script13` 会在 `ability.anm` 里解析成
  另一支脚本。**下一个实验**：在 `th18:0x45e723`（`mov eax,[player+0x10]`）断下，
  记录该指针是否等于 `player+0xc`（`pl01.anm`）还是 `ABILITY_MANAGER+0xc`；
  同时核对 `hit4` 里 `[ebx+8]` 的 VM id 属于哪块 anm。若发现上下文串位，修法是
  在进入主炮发射前强制恢复 `player+0x10 = player+0xc`（而不是只在卡子机路径还原）。

#### 本次安全修法（已发布）

- `assets/sht/append_shooterset.py`：
  - `HYPER_ANM_MAP["pl01.sht"]` 改为 identity `{5:5,6:6,7:7,8:8,11:11}`（文档口径）；
  - 新增 `HYPER_FALLBACK["pl01.sht"] = WOLF/EAGLE -> retail set 5..9`，`_hyper_sets()`
    对 pl01 直接从 `local/th18.v1.00a/dat/pl01.sht` 拷贝 10 段零售主炮 row
    （含终止符）到追加槽 `0x1e..0x27`；pl00 完全不动。
  - 副本行全为零售 `mode=0` 的普通主炮弹（`anm=5` 本体、`anm=9` option），
    不含 `mode=2`、`opt_slot=0`、TH17 `init4/tick5/hit5`，不会再走到 `tick2` 空指针，
    也不会引用 TH17 狼/鹫脚本。
- 验证：`python3 assets/sht/append_shooterset.py --verify-only`、`check_player_anm_bounds.py`
  通过；`cd native && make check` 全绿（含 `check-docs.py`）。
- 部署：`MODKIT_DIR="/mnt/d/项目/幻想指南/18/th18_modkit" NO_GIT=1 make release`；
  部署目录的 `pl00..pl03.sht`、`pl00..pl03.anm`、`abcard.anm`、`ability.anm`
  与 `native/build/` 逐文件 `cmp` 相同。
- DLL 未改代码 ⇒ md5 仍为 `95321b4e4c2467532240fa8179579247`；
  `bash local/sync_modkit_dll.sh` 后四份（`native/`、`mods/`、`thcrap/bin/`、
  `th18_card_expand_255/`）同 md5，本轮未被游戏锁。
- 未做 git commit / push。

### AL37. ★ 魔理沙（pl01）兽灵组收尾：EAGLE 安全激光 + WOLF 根因复核（2026-09-11 七修）

#### A. EAGLE：data-only 安全激光（Task 1）

用户定案「安全改法」。`assets/sht/append_shooterset.py` 新增 `HYPER_TEMPLATE_PL01A_EAGLE_SAFE`
与 `HYPER_CUSTOM`：TH17 `pl01a.sht` EAGLE 五行的 `damage/hit_h/angle` 保留
（80/60/60/50/50、34、−90°±），其余字段换成 TH18 零售主炮 `mode=2` 激光形状：

| row | damage | angle | `opt_slot` raw | 低 4 位（option 指针）| 高 4 位（`mode=2` 状态槽）| `mode` | `anm` | `hit_w/hit_h` | `hit_fx` | func init/tick/draw/hit |
| ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: | --- | ---: | --- |
| 0 | 80 | −1.570796 | 0x11 | 1 | 1 | 2 | 6 | 0/34 | 8 | 2/2/0/2 |
| 1 | 60 | −1.308997 | 0x21 | 1 | 2 | 2 | 6 | 0/34 | 8 | 2/2/0/2 |
| 2 | 60 | −1.832596 | 0x32 | 2 | 3 | 2 | 6 | 0/34 | 8 | 2/2/0/2 |
| 3 | 50 | −1.047198 | 0x43 | 3 | 4 | 2 | 6 | 0/34 | 8 | 2/2/0/2 |
| 4 | 50 | −2.094395 | 0x54 | 4 | 5 | 2 | 6 | 0/34 | 8 | 2/2/0/2 |

- 低 4 位 = TH18 主炮路径真正用来算子机槽的 4 bit（`0x45ec4c` / `0x45e407`），必须恒 **1..4**；
  主炮不会再给 `opt_slot` 低 4 位为 0 的 `mode=2` 行压 NULL option。TH17 row0 的「自机本体激光」
  改挂 option1，row1..4 的 option1..4 语义不变。
- 高 4 位 = `mode==2` 时 `PlayerBullet__create` `0x45e48d` 与 `tick2` `0x46025c` 共用的
  `player+0x476fc` 状态槽。五行低 4 位有重复（1/1/2/3/4），若高 4 位也全 0，
  `Player__shoot_one_bullet` `0x45e96e` 会在第二行看到「该槽非 0」而**整行跳过**，五条只剩四条。
  故用 0x11/0x21/0x32/0x43/0x54 把状态槽错成 1..5；`Player__initialize` `0x45ad54` 已把
  `player+0x476fc` 起 0x78 字节清零，1..5 都在界内。
- 追加组索引保持 `CE_SHT_SET_EAGLE_0 = 0x23`（空组）+ `0x24..0x27`（同一份 5-row blob）；
  `CE_SHT_SET_WOLF_*` 等索引不变。
- 生成后逐字段回读 `native/build/sht/pl01.sht`：EAGLE 四组均为
  `rate=15`、`opt_slot=0x11/0x21/0x32/0x43/0x54`（低 4 位 1/1/2/3/4、高 4 位 1..5）、
  `mode=2`、`anm=6`、`hit_fx=8`、`func=(2,2,0,2)`，终止符位置正确；
  `check_player_anm_bounds.py` 确认 `anm=6` / `hit_fx=8` 都存在于 `pl01.anm` 的 47 支脚本中。

#### B. WOLF：Task 2 disassembly 结论（证伪 `ability.anm` 串位假说）

一手反汇编链如下：

- **弹本体的 anm 文件 = `player+0x10`**：`PlayerBullet__create` `0x45e320`
  - `0x45e723` `mov eax,[PLAYER_PTR+0x10]`；
  - `0x45e726` `mov [bullet+0xe0],eax`；
  - `0x45e733` `movsx ecx,word [shooter+0x22]`；`0x45e76c` `call 0x407420`
    按该 anm 文件 + script id 克隆脚本。
- **中断子 VM 继承父 VM 的 anm 文件，而不是重新读 `player+0x10`**：
  - `scriptNew` 在 `0x479ab6`、`scriptNewPos` 在 `0x479c95` 做
    `mov ecx,[ANM_MGR+edx*4+0x312072c]`，其中 `edx=[parent_vm+0x1c]`；
  - 紧接着 `call 0x4888f0`（child）或 `0x488a40`（orphan），内部 `call 0x407420` 克隆脚本；
  - `AnmManager__preload_anm` `0x486a3e` 把每个脚本模板的 `+0x1c` 写成
    `[AnmLoaded+0]`（该 anm 在 manager 取表里的槽号）。
- **`player+0x10` 写者普查**（capstone 按寄存器状态扫全 `.text`；除初始化外只认对
  `[player+0x10]` 的直接存储）：
  - `Player__initialize` `0x45a7b8` / `0x45a7c3`：`preload_anm(9, pl0X.anm)` 的返回值同时写
    `player+0xc` 与 `player+0x10`；
  - `Player__tick_shooters_for_ability_card` `0x40aa06` 写 ability.anm、`0x40aa9a` 还原
    `player+0x10 = player+0xc`；
  - `CardSumireko__on_tick_2` `0x410561` 写 ability.anm、`0x410685` 还原
    `player+0x10 = player+0xc`。
- 实跑卡组是 `[117,21,22]`（无装备卡），主炮路径 `Player__tick_shooting_state` `0x45ea00`
  在 card `+0x1c` 广播之前，且 AbilityManager（0x16）先于 Player tick（0x17）；所以主炮出弹时
  `player+0x10 == player+0xc == pl01.anm`。⇒ **本 build / 本卡组下，`ability.anm` 串位不成立**，
  不能解释机体本体。

**更合理解释**：AL33 已把 `pl01.anm` 改成零售原样（47 支脚本）；AL36 之前线上曾有一版
`pl01.sht` 把 WOLF `anm` 指到旧追加 script50/51，而部署的零售 `pl01.anm` 没有 50/51，
`0x407420` 的无边界脚本模板拷贝会读出垃圾/相邻堆数据。AL37 重新生成后，
`check_player_anm_bounds.py` 已把「sht 的 `anm`/`hit_fx` 必须在本文件 script 表内」变成构建门；
WOLF 直接走零售 `script13`。

#### C. 修法与额外安全网

- `HYPER_FALLBACK` 清空（保留空 dict 和 `_hyper_sets` 的 custom/fallback 两级安全网接口）；
  `HYPER_ANM_MAP["pl01.sht"]` 恢复 `{5:10,6:11,7:8,8:13,11:16}`；pl01 WOLF 重新使用
  TH17 五行（`mode=0/opt_slot=0`；主炮路径安全，因为 `mode!=2` 不读 option 指针）。
- `native/cards/animal.c` 的 `hyper_shot_apply()` 增加唯一一处「咆哮期间 anm 上下文」兜底：
  若 `*(player+0x10) != *(player+0xc)`，在 Player tick 之前写回 `player+0xc`，并只打一次
  `wba_wc: player anm context repaired …`。这是纯兜底（B 已证主因在数据），正常路径不触发、
  不写、不影响别的 ability 卡。
- `build_player_anm.py` / `check_player_anm_bounds.py` 不改；`pl0X.anm` 仍为零售原样。

#### D. 验证 / 待实跑

- `python3 assets/sht/append_shooterset.py` 重新生成；回读 `pl01.sht` `0x1e..0x27` 全部字段
  通过（WOLF `anm=13`；EAGLE `mode=2/anm=6/hit_fx=8` 五条）。
- `cd native && make check` 全绿（主机单测 + pytest + sites + DLL ABI + `check-docs.py`）。
- `MODKIT_DIR="/mnt/d/项目/幻想指南/18/th18_modkit" NO_GIT=1 make release` 成功；
  部署的 `pl00..03.sht`（`native/build/sht/`）、`pl00..03.anm` / `abcard.anm` / `ability.anm`
  （`native/build/`）与构建目录逐文件 `cmp` 相同。
- DLL：`native/th18_card_expand.dll` = **`51ead4e50ec9e27f13b0fff113a34473`**；
  `bash local/sync_modkit_dll.sh` 后四份（`native/`、`mods/`、`thcrap/bin/`、
  `th18_card_expand_255/`）同 md5，本轮未被游戏锁。
- **实跑验收**：
  1. 魔理沙（pl01）咆哮应：WOLF 命中爆 `script13 → script14` 的橙色 30×30 帧，**不再出现**
     魔理沙机体；EAGLE 出五条 `script6` 激光、命中 `script8` 光斑，**不再闪退**；
  2. 日志若出现 `player anm context repaired`，说明确有 anm 串位，请回传前后日志；
     若未出现且 WOLF 仍出自机，则问题不在 anm 上下文，下一步保留 WOLF 回退并改查
     `pl01.sht` 表指针/旧 patch；
  3. pl00（霊夢）不变；pl02/pl03 仍为安全回退。

### AL38. ★ 鬼形兽（id 117）八修：shot 组改为 fire-time 重定向 + EAGLE 身体激光；动物灵 pre-switch flicker 无条件化（2026-09-11）

用户实跑反馈（本轮）：

- 「魔理沙按着 Z 进入狼咆哮，原本发出的弹幕碰撞后会变成魔理沙贴图；不按 Z 进狼咆哮再按 Z 无事；大鹫在无子机时按 Z 什么都不出。」
- 「动物灵切换样式前会闪烁，但相当一部分不会；只有靠近过之后的那一次切换才会在切换前闪烁。」

#### A. 弹幕组：不覆盖零售 0..9，改 fire-time set index

**根因（已建立，本轮直接按约束实现）**：旧 `hyper_shot_apply()` 把 `pl0X.sht +0xe0` 的 **0..9 槽**
临时改指 beast 组；但 `PlayerBullet__create` 把 `(set<<8)|row` 存进 `bullet+0xac`（`th18:0x45e35c`），
命中/回读时 `row = rowidx*0x5c + sht[0xe0 + set*4]`。咆哮前射出的弹存的是零售 set 0..9，
命中时却读到 beast 组 ⇒ 变自机/特效贴图。TH17 不这样做：它是在**开火时**把 idx 选成
`火力档 + 10`（狼）/`+15`（鹫），零售 0..9 不动。

**实现**：新增 SDK 断点 `BP_ce_shot_set_index` @ `th18:0x45ebfb`（原指令
`mov edi,[edi+edx*4+0xe0]`，7 字节 `8b bc 97 e0 00 00 00`；`sites.py` 的 `SDK_SITES` 第 10 项）。

- `th18:0x45ebd1` 已算出 `eax = CURRENT_POWER / 100`（火力档 0..4）；聚焦时
  `th18:0x45ebf5` 把 `edx` 加成 +5 并写 `[ebp-8]`。
- handler 向在场卡问 `on_shot_set_index(card, eax)`。card 117 在 `ready || hypering` 且
  `hyper_type==1/3` 时返回 `CE_SHT_SET_WOLF_0/EAGLE_0 + power`（`0x1e/0x23 + 0..4`）。
- SDK 同时改 `regs->edx`（本帧取哪组 row）与 `[ebp-8]`（`th18:0x45ec69` 会把它打包进
  `bullet+0xac`，所以弹里也记 beast set）；随后放行原 `0x45ebfb`。零售 0..9 一个字节不写。
- handler 不咆哮返回 `-1`，原版主炮完全放行。

**日志**：card 每次进入 ready 后首次覆盖打
`wba_c: shot set -> wolf/eagle (fire-time index base 0x1e/0x23 + power, retail 0..9 untouched)`；
首次真正 fire 打
`wba_c: shot set index -> wolf/eagle base 0x1e/0x23 + pwr N = 0x…`。
一次咆哮只打一行，便于实跑对账。

**旧弹不会再被改**：已出弹存旧 set 0..9，命中回读到零售 row；新弹存 beast set，命中回读到
`0x1e..0x27`。`sht` 的 0..9 槽在内存里不再被本卡触碰。

#### B. EAGLE：选择 (a) 合成 option 指针，而不是矩形

用户反馈「无 option 时大鹫什么都不出」。TH17 `pl01a.sht` EAGLE 的 `mode=2` 激光是自机体激光
（`opt_slot=0`）；TH18 主炮路径会把 `opt_slot` 低 4 位解释成 option 槽，但没有真实子机时
`tick2`/`hit2` 读到的坐标是空槽，激光看不见。选择实现 (a)：

- `th18:0x45fcce` / `0x45fcd3`（`tick2`）与 `th18:0x460b9e` / `0x460ba3`（`hit2`）**只读**
  `option+0x5c`、`option+0x60` 两格（定点 ×1/128）。
- `animal.c` 定义只含这两个有效字段的 `animal_body_option_t s_body_opt`；`on_bullet_created`
  （`PlayerBullet__create` 末尾的卡槽 `th18:0x45e7f5`）对本卡 EAGLE set、`mode==2` 的弹
  写入 `bullet+0xe4 = &s_body_opt`（`th18:0x45e3ae` 原本从第 5 个栈参写 NULL/空槽）。
- 每个咆哮帧在 Player tick 之前把 `s_body_opt.x/y` 写成自机坐标 ×128（`ANIMAL_SUBPIXELS`），
  所以激光从身体出现并随身体移动；松开 Z 时引擎 `tick2` 原有的「无射击键 → 收束」路径停止。
- 这比矩形方案更接近零售 `mode=2` 语义：伤害/命中/hit_fx/消弹/收束全走引擎原路；不新增伤害源、
  不改 `ability.anm`，也不需要 `ce_cancel_radius` 之类近似。

日志：首次挂伪 option 打
`wba_c: eagle body laser active (set 0x…, synthetic option …)`；
咆哮结束/卸载打 `wba_c: eagle body laser off`。

**数据**：`assets/sht/append_shooterset.py` 仍把 PEA/SEASON/WOLF/EAGLE 追加在
`0x17..0x27`；WOLF 在 `0x1e..0x22`、EAGLE 在 `0x23..0x27`。本轮**不改数据**，
只改 C 侧绑定；`check_player_anm_bounds.py` 仍确认 `pl01.sht` EAGLE 的 `anm=6` / `hit_fx=8`
在 `pl01.anm` 的 47 支脚本里。

#### C. 动物灵 pre-switch flicker：补 TH17 的「寿命 == 60」无条件分支

**根因**：`animal.c` 原来只在「离开 60 px」时发 `interrupt 2`（闪烁）；没靠近过的 token
永远不发。TH17 `GoastManager::sub_410af0` `th17:0x410af0` 的做法不同：

- `th17:0x410c25`：离开 60 px 时，若剩余寿命 `<= 60` 发 `interrupt 2`，否则发 `3`（离开动画）；
- `th17:0x410c8c`–`0x410c99`：执行完近/远分支后，**无论**是否靠近过，只要
  `timer_life.current == 0x3c`（60）就 `push 2; call 0x476510` —— 所有 token 在切换前 60 帧都会闪一次。

**修法**：`ANIMAL_FLICKER_LEAD 60`；在近/远逻辑之后：

```c
if (t->life == ANIMAL_FLICKER_LEAD && t->id) {
    ce_anm_interrupt((uint32_t)t->id, 2);
    if (!t->flicker_logged) {
        t->flicker_logged = 1;
        ce_log("wba_wc: token flicker (was near=%d)", t->near);
    }
}
```

离开 60 px 的分支也改成 TH17 的条件 2/3；靠近冻结逻辑保留（`life < 60` 重置 90）。
`token_change_type()` 重置 `flicker_logged`，供下一轮 180 帧循环再次记录。

日志：每个 token 每轮变色前最多一行 `wba_wc: token flicker (was near=%d)`，可直接区分
「没靠近过」与「靠近过」两类的 `near` 值，用于证伪。

#### D. 验证 / 实跑验收

- `cd native && make check` 全绿；`python3 sites.py verify` 对账 210 条 binhack 全过。
- `python3 assets/check_player_anm_bounds.py`：四文件 sprite / script 边界、兽灵主路径 delete 全过。
- `MODKIT_DIR="/mnt/d/项目/幻想指南/18/th18_modkit" NO_GIT=1 make release` 成功；
  写入 `th18.v1.00a.js` / `mods/th18_card_expand.dll` / `files.js`。部署目标里的
  `th18/*.sht`、`th18/*.anm`、`patch/th18.v1.00a.js` 与 `native/build/` 逐文件 `cmp` 相同。
- 部署 DLL md5 = `1c6484c54d584336d1867320888397f6`；`bash local/sync_modkit_dll.sh`
  把同一 md5 同步到 `mods/`、`thcrap/bin/mod_th18_card_expand.dll`、
  `th18_card_expand_255/th18_card_expand.dll`，四份一致，本轮未被游戏锁。
- 实跑应出现的日志：
  - `wba_c: shot set -> wolf/eagle ...`
  - 首次开火：`wba_c: shot set index -> wolf/eagle base ... + pwr N = 0x…`
  - 大鹫：`wba_c: eagle body laser active (... synthetic option ...)`；结束：`... laser off`
  - 动物灵：`wba_wc: token flicker (was near=0)`（从没靠近）和 `was near=1`（靠近过）都该出现。
- 实跑不应出现：
  - 咆哮前弹命中变成自机/特效贴图；
  - `wba_c: player anm context repaired`（若出现，说明另有 anm 串位，保留日志继续查）；
  - 无 option 时大鹫按 Z 无激光。

### AL39. ★ 鬼形兽（id 117）九修：pl01 EAGLE 改卡侧矩形束 + `on_shot_set_index` 实跑探针 + 正常咆哮退出 SE 试听（2026-09-11）

用户实跑反馈（Item 1 / Item 2）：

- 「按住 Z 进鹫咆哮 = 发变成激光贴图的弹幕」；
- 「松开 Z 进鹫咆哮 = 什么都发不出来」（之后再按 Z 也无事）；
- 「正常咆哮退出的 SE 一直不对；不要继续 RE 猜。」

#### A. 结论：shooter-row 路径在主炮路径下做不出真 TH17 激光，改卡侧矩形束

TH18 主炮路径的 `PlayerBullet__create` 把 `mode=2` row 当普通弹体创建，而不是
`Player__tick_shooters_for_ability_card` 那条「挂在子机上的束」路径；`on_bullet_created`
补伪 option 只能补坐标，补不出 init2/tick2/hit2 的束长/伤害源语义。因此用户看到的是
**带 `pl01.anm` script16 / sprite29 贴图的移动弹**；松开 Z 时 `on_shot_set_index`
只在开火帧被问，更不会创建任何东西。

按用户已批准的 end state 改为**卡侧矩形束**：

- 数据：`assets/sht/append_shooterset.py` 的 `HYPER_CUSTOM[("pl01.sht", "EAGLE")] = []`。
  `native/build/sht/pl01.sht` 的 EAGLE 组 `0x23..0x27` 现在是合法空组（每条只有
  4 字节终止符 `ff ff ff ff`）。WOLF `0x1e..0x22` 与其它角色不动。
- fire-time：`BP_ce_shot_set_index` @ `th18:0x45ebfb`（现有第 24 个导出，未新增 BP/def/ABI）
  仍然在 `ready || hypering` 时返回 `EAGLE_0(0x23) + power`。引擎照常改 `edx` 与 `[ebp-8]`，
  但目标为空组 ⇒ 主炮不会创建 bullet，也就不会再有 laser sprite-bullet。
- 卡侧束：`native/cards/animal.c` 的 `eagle_beam_tick()` 在
  `CE_INPUT_HELD() & 1`（Z / 射击键）按住、`roaring()`、`hyper_type==3` 时：
  - 起 `CE_ANM_ABILITY_SCRIPT_EAGLE_BEAM`（`ability.anm` 新脚本 **163**）；
  - 每帧 `ce_anm_set_pos(vm, player)` + `scale=(1,1)`；脚本写
    `sprite(@EAGLE_BEAM)`（TH17 `pl01.anm` entry0 `sprite29` = `(0,288,512,32)`；
    抽取 PNG 与 TH18 同名贴图逐字节一致，md5 `10ff96a027419498baceb70ca199dd23`）；
    `anchor(1,0)` + `rotate(0,0,-1.5707964f)`，与青眼 script80 同款，从自机向上拉 512 px；
  - 伤害：`ce_damage_rect(center=(player.x, player.y-256), angle=0, life=2,
    dmg=20, w=34, h=512)`，并在同一帧 `ce_raise_damage_cap(300)`；
  - 消弹：引擎只有圆形 API，所以沿轴取 5 个重叠圆
    `ce_cancel_radius(r=20, max=9999, mode=1)` 近似 34 px 宽矩形柱；
  - 松开 Z / 咆哮结束 / 对话暂停：`eagle_beam_kill()`（`interrupt 1` 淡出 6 帧）。
- 不依赖火力档（伤害源不读 power），不依赖 option（没有真实子机也工作），满足用户
  (a) 咆哮前按住 Z、(b) 松开 Z 无 stray sprite-bullet、(c) 咆哮中再按 Z 三条。

**实跑探针**：`on_shot_set_index` 不再只打一次 `shot set index -> …`，而是在每次
`on_ready()` / `hyper_begin()` 后重置 `shot_index_req_logs`，对**前 5 次**真实请求逐条打：

```
wba_c: shot set index req: card=117 ready=1 hyper=0 power=N -> wolf (base 0x1e + pwr = 0x1f)
wba_c: shot set index req: card=117 ready=1 hyper=0 power=N -> eagle (main set empty; body beam) (base 0x23 + pwr = 0x24)
wba_c: shot set index req: card=117 ready=1 hyper=0 power=N -> unchanged
```

若 30 帧内一次都没被问（典型 = 松开 Z 进咆哮），`hyper_shot_apply()` 打：

```
wba_c: shot set index req: card=117 ready=1 hyper=0 none in first 30 frames (shoot_held=0)
```

**数据自检**（`native/build/sht/pl01.sht` 0x1e..0x27）：

```
0x1f..0x22 WOLF  = 5 条/组（TH17 WOLF 保留）
0x23..0x27 EAGLE = 0 条/组（空组，只有终止符）
```

#### B. 正常咆哮退出 SE：从「猜 id」改成 debug build 六候选试听

`ANIMAL_DEBUG_KEY` 仍为 1，所以 debug build 每个**正常** `hyper_end(!by_hyper)`：
抑制原 `ANIMAL_SE_ROAR_END`，设置 6 个候选（30 帧一声，首条在结束当帧）：

| # | id | resolved name（任务指定 id→name 表）|
| ---: | ---: | --- |
| 1 | `0x34` | `se_lgods4` |
| 2 | `0x38` | `se_pldead01` |
| 3 | `0x3e` | `se_tan03` |
| 4 | `0x43` | `se_release` |
| 5 | `0x42` | `se_item01` |
| 6 | `0x39` | `se_heal` |

日志（用户只需回报哪一号最像 TH17 的 roar-exit）：

```
wba_wc: SE probe active (6 candidates, 30 frames apart); normal roar-end SE 0x38 suppressed
wba_wc: SE probe 1/6 id 0x34 (se_lgods4)
wba_wc: SE probe 2/6 id 0x38 (se_pldead01)
wba_wc: SE probe 3/6 id 0x3e (se_tan03)
wba_wc: SE probe 4/6 id 0x43 (se_release)
wba_wc: SE probe 5/6 id 0x42 (se_item01)
wba_wc: SE probe 6/6 id 0x39 (se_heal)
```

对话暂停时试听冻结（`se_probe_tick(s, paused)` 提前 return），不会在暂停里偷播。

#### C. 门 / 部署（本轮）

- `make anm sht dll`、`make check` 全绿；`python3 sites.py verify` 210 条全对；
  `python3 assets/check_player_anm_bounds.py` 通过（pl01 sht 0x1e..0x27 = WOLF 20 + EAGLE 0）。
- 本轮**未新增 `BP_*` 导出**，`th18_card_expand.def` 保持 24 个；`dllverify` 仍报 24 exports。
- 部署 DLL 与 `native/build/` 文件逐字节 `cmp` 见最终报告；`AUDIT.md` 本节 / `NEXT.md` §1v-补3 是文档变更。


## G. OPEN —— 还没解决的

| # | 事项 | 说明 |
| --- | --- | --- |
| G1 | ~~游戏内未实跑~~ | **三步实跑通过**，见 §0 |
| G2 | ~~全有或全无的门还没做~~ **已做**（2026-09-02） | 见 §H。触发原因是用户复审指出的一种「日志一切正常」的静默失败 |
| G3 | 扩容还要动的东西 | B（§I）、C（§J，manager 部分）、D（§K）已做；`zAbilityMenu` 的 `__card_ids` 扩容与顺序表耦合，归 E；E 未做 |
| G5 | ~~新卡在卡组编成里「未获取」~~ | 战线 D 已做（§K，待实跑）。原因是 `unlocked_cards` 是 `uint8_t[57]`，`[58]` 落在未知区读出 0 |
| G6 | ~~新卡解锁后名字 / 说明是乱码~~ | **已做**（§L，待实跑）——三处读重定向到 DLL 缓冲，占位「测试卡牌 N」。原因：`zAbilityText` 只有 57 张，id 58 落在对象之外 |
| G4 | MSVC 换 build 后骨架是否仍然一致 | 未验证。`make check` 的锚点数会立刻暴露（不是 25 就停） |
| G7 | deck 槽 id 符号扩展墙的指令位 | §Z11：id ≥`0x80` 装槽被剔（现象实跑），写槽点 `0x41511c` 无校验 → 墙在显示/重建链；处置已绕道（id ≤127），墙位待钉死（`0x415af0` 起）后才知 127 以上能否解锁 |

## §AM. 敌人死亡 / 击破共享事件 + 止射闸（2026-09-12）

**没有手写机器码、没有新 cave**；新增**一个断点**（`ce_enemy_death`）与**一处 handler 扩展**
（`ce_shot_set_index` 里加 suppress 询问）。

| # | 断言 | 可信度 | 证据 |
| --- | --- | --- | --- |
| AM1 | 断点 `0x430240` = `FUN_00430240` 入口，`cavesize 6` / `expected 558bec83ec14` | ✅ 一手 | `make check` 的 sites 对账（exe 字节逐条比对）；`sites.py` 表内 |
| AM2 | `ecx` = zEnemy（唯一可信寄存器） | ✅ 一手 | 7 个调用点就地 `mov ecx,<enemy>`（`0x42ee85 [ebx+0x5588]` 回指 / `0x4307ec` 结点自指针）；见 spec §1.6 |
| AM3 | 死因 = `regs->retaddr`：`0x42f3b7` 打死主路、`0x42ee90` 炸弹秒杀、`0x430f59` ECL die、`0x4307f1`/`0x430a00`/`0x430c08` killAll | 🟡 待实跑直方图 | spec §1.3；🟡 唯一需要实跑定案的项 |
| AM4 | 事件**在撒主掉落之前**（⇒ 卡能改掉落表种类）| ✅ 一手 | `0x430469` 读表 → `0x430493 call 0x446f40` 撒 → `0x43049f call 0x430510` 通知卡 → `0x4304aa` 清表 |
| AM5 | 止射闸：`return 0` + `retaddr = 0x45eca0` 跳原生行循环、**保留卡广播** | ✅ 一手 | `0x45ec06 js 0x45eca0` 是引擎自己的空行落点；`0x45ebfb` 唯一入口 `0x45ebf0 je` |
| AM6 | 顺序：**先问 set index（117 咆哮），有覆盖本帧不抑制** | ✅ 实现即此 | `BP_ce_shot_set_index` 里先跑 set-index 循环、命中即 return |
| AM7 | 两个新 hook 对既有 27 张卡零影响 | ✅ | `CE_CARD` 用指定初始化器 ⇒ 未声明字段为 NULL；`make check` exit 0（含 27 张卡的 host 单测）|
| AM8 | 共享件：`ce_item_collect_all` / `ce_invuln_apply`（取最大）/ `ce_shoot_cancel_claim`（帧闩锁）| ✅ 编译进 DLL | `nm sdk.o` 有 `_BP_ce_enemy_death`；`make check` 全绿 |

**实跑清单**：① retaddr 直方图（打死 / 炸弹 / ECL die / 清场各打印一次）；② 只装 108 时低速按住 Z ⇒ 原生弹停、77/116 的卡子机照打；③ 与 117 同装 ⇒ 咆哮期间原生弹照发（不被抑制）。

### AM-补：两处「静默不生效」的坑（2026-09-12 收口时抓到并上了门）

1. **`CE_BP_*_RVA` 曾整体错位**：`sites.py` 生成 `sites_gen.h` 时用的是**下标** `SDK_SITES[10]`，
   插入 `ce_enemy_death` 后 `CE_BP_SHOT_SET_INDEX_RVA` 变成 `0x030240`（= 死亡断点地址）
   ⇒ 门里的「断点已挂」对账会看错字节。已改成**按名字**取址（`_sdk_rva(name)`），插条目不再错位。
2. **DLL 忘了导出 `BP_ce_enemy_death`**：patch 声明了断点、handler 也写了，但 `th18_card_expand.def`
   没有这条导出 ⇒ thcrap `GetProcAddress` 拿不到处理函数、**游戏里静默不生效**（不报错）。
   已补（`@25`，导出总数 24 → 25，`dllverify` 的上限同步改）。
   **新增永久门**：`sites.py check` 现在做「patch 声明的断点 ↔ `th18_card_expand.def` 的 `BP_*`」**双向对账**
   （含 `patch-test` 的断点；缺导出 / 僵尸导出都停），`make check` 会跑到。

## §AN. 红魔乡（id 106）—— PlayRank 动态难度（2026-09-12）

**没有手写机器码、没有新断点**；全部走既有事件（`on_item_collect` / `on_item_score` / `on_item_frame` /
`on_draw` / `on_stage_start`）。纯逻辑在 `cards/th06rank_core.c`（无引擎地址），卡侧 `cards/th06rank.c`。

| # | 断言 | 可信度 | 证据 |
| --- | --- | --- | --- |
| AN1 | rank 单位 = **千分之一**（PERMILLE），进位阈值 1/100 与 1/1000 都不丢精度（蓝点 +0.006 除不尽）| ✅ 实现即此 | `th06rank_core.h` 的 PERMILLE 常量表；`test_th06rank_core.c` 40168 断言 |
| AN2 | 12 条增减逐条对 spec §2.2（帧数 `2400−残机×240`、擦弹 +0.06、整命 +2.00、Bomb −2.00、Miss −16.00 非决死…）| ✅ 一手（th06 反编译 `GameManager.cpp` / `EnemyManager.cpp`）| spec 表逐行 |
| AN3 | 难度表 EASY 下限 **12**（不是 10）：EASY `16/12/20`、NORMAL/HARD/LUNATIC `16/10/32`、EXTRA `16/14/18` | ✅ 一手 | `th06rank_core.c` 常量 + 单测 |
| AN4 | 碎片会计三边界：碎片逐颗走 `on_item_collect`；整命/整炸走库存**正向增量**；同窗口碎片**净减**时抑制 | ✅ 实现即此 | 单测三例（3 碎 = 2.1；净 0 不抑制；别处来整命照加）|
| AN5 | `rank → 难度` **完全搁置**：未写 `0x4ccd08`、未 hook `0x42aa20`（用户 2026-09-12 拍板）| ✅ 实现即此 | `grep 0x4ccd08 cards/th06rank.c` 只有注释 |
| AN6 | 判定点隐藏 = `on_draw` 里 `player+0x476b0` 的 anm `color(0x00FFFFFF)`（alpha=0）、只在聚焦时写、带 `CE_ADDR_GAME_THREAD_PTR` 门 | 🟡 待实跑（anm 脚本可能每帧抢回颜色）| spec §2.1；实跑看指示点是否消失 |
| AN7 | 出屏 −0.03 的判据 = `\|x\| >= 200.0` **含等于**（`0x445e2b` `comiss` + `jb`）、`y > 472` | ✅ 一手 | 逐指令核到实现层（`0x445e20`/`0x445e2b`/`0x445e13`）|
| AN8 | 去重窗 = 定长 192 槽环形窗（不是 4696 槽位图：位图 587 B > `ce_state` 496 B 上限）| ✅ 编译期 | `sizeof(ce_state) <= 496` 断言，实际 452 B |
| AN9 | 全卡私有状态在 `on_load` / `on_stage_start` / `on_run_reset` / `on_delete` 都收干净（本卡无自建 VM / HUD）| ✅ | `th06rank.c` 生命周期段 |

**实跑清单**：① 连续吃道具看 rank 是否上升、中弹看 rank 是否 -16；② 判定点是否消失且**不影响判定**；
③ 蓝点非最大档的 +0.006 是否真的累进（吃 5 颗 = +0.03）；④ 每关补货（36/37 两张碎片卡）是否各 +0.70/+0.02。

## §AO. 花映塚（id 109）—— 爆风 / 蓄力 / 气槽（2026-09-12）

**新增一处 handler 的消费者**：`on_native_shot_suppress`（复用 `0x45ebfb`，见 §AM/AM5、AM6）。
**没有手写机器码、没有新断点、没有新 cave。** 纯逻辑在 `cards/pofv_core.c`，卡侧 `cards/pofv.c`。

| # | 断言 | 可信度 | 证据 |
| --- | --- | --- | --- |
| AO1 | 蓄力拦 Z **不改输入位**（`0x4ca428` bit `0x001` 一个字节都不写），只让 SDK 的 suppress 分支 `regs->retaddr = 0x45eca0` 跳过**本帧原生行循环**，卡广播（77/116）照常 | ✅ 一手 + 实现即此 | `0x45ec06 js 0x45eca0` = 引擎自己的空行落点；`grep` 卡侧无输入位写点 |
| AO2 | 拦 Z 条件 = 蓄力中（HOLD）**且** level ≥ 1 **且** gas ≥ 1 档 ⇒ **点射不拦**（不改变原版手感）| ✅ 实现即此 | `pofv_core.c` 的 `pofv_charge_suppress`；spec §2 |
| AO3 | 与 117 咆哮的优先序：**先问 set index，有覆写本帧不抑制** | ✅ 实现即此 | `BP_ce_shot_set_index` 先跑 set-index 循环（§AM/AM6）|
| AO4 | `on_enemy_death` **只做 O(1)**（连爆档 + 入队 + 掉一颗灵魂），消弹在 `on_tick_2` 帧末按预算做 ⇒ 清场帧不 O(n²) | ✅ 实现即此 | `pofv.c` §性能注释 + `POFV_BLASTS_PER_FRAME` |
| AO5 | 蓝圈**自扫池**逐弹 `0x428e90`（只吃白弹：半径 ≤ 4.0 且可消）；红圈用 `ce_cancel_radius` 并把**返回值**当颗数（引擎没有带 exclude 的半径消弹）| ✅ 一手（review §5.4 订正）| spec §10 订正条 |
| AO6 | `on_enemy_death` 里**不析构敌人、不缓存 `enemy` 指针**；`KILL_ALL` 被排除 | ✅ | `pofv.c` 回调体；DEATH §2.4 |
| AO7 | 无敌走 `ce_invuln_request` 声明层（每帧声明剩余帧数，层取 max）⇒ 不截短 107/117 的无敌 | ✅ 实现即此 | EVENTS-CONTRACT §4/§5.2；`pofv.c` |
| AO7b | ❌ **QCA 放弃**（用户 2026-09-12）：「B = 立即发动最高可发动等级」要拦 TH18 的炸弹键（`on_bomb_gate 0x45c048`），而那条路的「代价归属」一直未拍板、对玩家的收益也薄 ⇒ `pofv_qca_level()/pofv_qca_fire()` 已从 core **删除**（不留死代码），X 键完全交还原生炸弹；109 的 B 侧只剩长按 Z 蓄力 | ✅ 裁决 | `pofv_core.h` 文件内说明；spec §2/§6.2/§9 |
| AO8 | 长持续效果收尾：爆风队列 + 灵魂池 + 无敌声明，全在 `clear_card()`（`on_run_reset` / `on_delete` 共用）；当前无 ANM VM（视觉待资产）| ✅ | `pofv.c` `clear_card`；**加 VM 时必须补进这里** |
| AO9 | 16 个原版未逆到的量一律具名占位 + `❓ 自定` + `TODO(th09)`，未包装成「复刻」| ✅ | spec §5.2 逐条 |

**实跑清单**：① 按住 X 蓄力时原生弹停、松开恢复、点射不受影响；② 击破一只小怪看爆风消弹与得分；
③ 清场帧（boss 击破）帧耗是否可接受；④ 与 117 同装时咆哮期间原生弹照发。

## §AP. 风神录（id 110）+ 绿点共用件（2026-09-12）

**没有手写机器码、没有新断点**；绿点实体是**自建 VM 池**（TH18 没有绿点 item type），信仰记账走
**声明层**（`ce_score_declare` + `ce_score_applier`）。

| # | 断言 | 可信度 | 证据 |
| --- | --- | --- | --- |
| AP1 | 绿点是**一套实现 + 两个记账槽**：实体池 `faith.c` 共用（静态分区 大 64 / 小 256），记账 `faith_core.c` 按 `slot_mask` 分（110 只写 Δ_110、111 只写 Δ_111）| ✅ 实现即此 | `faith.h` 的 `faith_user_t` 回调 + `faith_register`；为 111 预留 SLOT_111 |
| AP2 | 灵击门槛 ≥ **2.00 档**、每次扣 **1 档**、下限 **1.05 档**（用户「サービス」）| ✅ 用户定案 | `faith_core.h` 的 `FAITH_POWER_MIN_LEVEL/COST/FLOOR`；110/111 共用同一组常数 |
| AP3 | 灵击的扣法是**自写**（写 `*(int32_t*)0x4ccd38` + `ce_repopulate_options()`），不走原生 `spend_power`（原生会拒绝低火力）| 🟡 待实跑 | `mof.c` 灵击段；实跑看 HUD 火力档是否掉 1 档 |
| AP4 | 消弹用 `ce_cancel_radius` 的**返回值**当颗数 + `ce_cancel_latch_claim` 帧闩锁（与 107/117 同帧时只有一家动手）| ✅ 实现即此 | `mof.c`；EVENTS-CONTRACT §5.2 |
| AP5 | PV→计分：`on_item_score` 里声明、**只在 `ce_score_applier() == 110` 时落笔**，读 `ce_score_aggregate()` 而不读别的卡状态 | ✅ 契约 | EVENTS-CONTRACT §3；`mof.c` |
| AP6 | 大信仰点判据 = `drop_main == 2` 或「同时掉钱与 P」，**boss 排除** | 🟡 boss 判据是占位 | `mof_core.c`；`❓ 自定 / TODO` |
| AP7 | 自建 VM 池在 `on_delete` / `on_run_reset` 按**自己那一槽**收干净（`faith_unregister(自己)` + `faith_clear_entities_mask(自己那一槽)`）—— 商店删卡时 tick 停摆，只有这两个回调跑得到 | ✅ | `cards/faith.h`；`mof.c` / `signal.c` 生命周期段 |
| AP7b | ⚠ `faith_module_reset()` **没有调用者、也谁都不该调**（它清**整池** + `memset` 整张注册表 ⇒ 110/111 同装时会把另一张卡的绿点与注册一起抹掉）；`faith_clear_entities()` 同理（保留为 `_mask(~0)` 特例）| ✅ | `cards/faith.h` 注释；`AUDIT.md` §AX |
| AP8 | HUD 坐标是 `❓ 自定`（非像素复刻，未抄到原版 MoF 位置），走 `ce_hud_slot_claim` 货架 | 🟡 待实跑 | `mof.c`；HUD-LAYOUT.md §2 |

## §AQ. 永夜抄（id 108）—— 妖机体 + 使魔（2026-09-12）

**没有手写机器码、没有新断点**；新增两处「共享件」的消费者：`on_native_shot_suppress`（见 §AM/AM5、AM6）
与 `ce_spawn_enemy()`（`0x42d7d0` 包装）。子机走**声明层**（`ce_optpose_*`，id 最小者落笔）。
纯逻辑在 `cards/youkai_core.c`，卡侧 `cards/youkai.c`。

| # | 断言 | 可信度 | 证据 |
| --- | --- | --- | --- |
| AQ1 | 止射闸判据 = **有自机 + 聚焦**；**绝不写 Z 输入位**（`0x4ca428` bit `0x001`），只让 SDK 跳原生行循环 | ✅ 实现即此 | `youkai.c`；`0x45ec06 js 0x45eca0` = 引擎自己的空行落点 |
| AQ2 | 与 117 咆哮优先序：先问 set index（有覆写不抑制）| ✅ 实现即此 | `BP_ce_shot_set_index` 的两段顺序（§AM/AM6）|
| AQ3 | 妖侧弹识别 = `bullet+0xac >> 8 == 0x16`（我们那组 shooterset）⇒ 只改自己的弹 | ✅ 实现即此 | `youkai.c`；`engine/sht/th18/02` |
| AQ4 | 子机：**只声明、不落笔**（落笔者才 `ce_option_alloc` + 写原生双偏移，`on_power_level_change` 重申请）| ✅ 契约 | EVENTS-CONTRACT §1（子机 id 最小者落笔）；`youkai.c` |
| AQ5 | 使魔 = 真敌人载体（`ce_spawn_enemy`），`spawn+0x14`（life）**钳 < 1000**（≥1000 会置 `flags_lo` 位 30 进锁定黑名单）| ✅ 一手 | `yk_clamp_spawn_life` + 单测；`0x42d8f6` |
| AQ6 | 伤害转嫁 1/2：`life.current` 逐帧差分 → **每帧新建** `ce_damage_rect(center,0,2,dmg,16,16)` 打在目标 + 同帧 `ce_raise_damage_cap` | 🟡 比例是 `❓ 自定` | 108 spec §3.7；实跑看 boss 掉血 |
| AQ7 | ❌ 已否决并写进注释：`enemy+0x6320` = `invuln_timer.current`（设正值 = 使魔无敌，与「使魔必须能被打」冲突）| ✅ 一手 | `youkai.c` 注释 |
| AQ8 | `on_enemy_death` 里只做 O(1)（记「该槽被打破」），不析构敌人、不缓存指针 | ✅ | `youkai.c`；DEATH §2.4 |
| AQ9 | 长持续效果收尾：使魔 / 子机声明 / 转嫁源都在 `on_delete` / `on_run_reset` 收干净 | ✅ | `youkai.c` 生命周期段 |
| AQ10 | ⚠ **实跑最高优先风险**：止射闸与妖侧 shooterset `0x16` 是**一对**，资产未装 ⇒ **低速一发弹都打不出来**（首次命中写诊断日志）| 🟡 待资产 | 108 spec §5；`YK_*_ASSET_READY` 门 |
| AQ11 | spec §3.4.1 勘误：`0x40a9c0` **不读** `rec[0x20]`、膛口恒取 `option+0x5c/0x60` ⇒「`opt_slot=0` ⇒ 从自机本体出膛」在卡路径上不成立 | ✅ 一手 | spec §3.5.1 / `youkai.c` 注释 |
| AQ12 | spec §3.5 的 `0x434019` **装不了断点**（1 字节 `push eax`，紧邻 `call rel32`）⇒ 走每帧状态差分替代 | ⏳ 低优先 | 108 spec §3.5.1 勘误 |

**实跑清单**：① 低速按住 Z ⇒ 原生弹停、妖侧弹出（需资产）；② 打使魔 ⇒ 掉血、转嫁到附近敌人；
③ 使魔全灭 ⇒ 转嫁归零、不重建；④ 与 117 同装时咆哮期间原生弹照发。

## §AR. 地灵殿（id 111）—— 信号槽 + 绿点信仰（2026-09-12）

**没有手写机器码、没有新断点**；绿点实体走 110 交付的**共用池**（`cards/faith.*`，按 `slot_mask` 分账），
记分走声明层。纯逻辑在 `cards/signal_core.c`，卡侧 `cards/signal.c`。

| # | 断言 | 可信度 | 证据 |
| --- | --- | --- | --- |
| AR1 | 绿点是**一套实现 + 两个记账槽**：111 只注册自己那一槽（`1 << FAITH_SLOT_111`），110 只写 Δ_110 ⇒ 不跨卡耦合 | ✅ 实现即此 | `signal.c` 的 `faith_user_t` 注册；`faith.h` |
| AR2 | 信号槽 0–130：线上 +50/f、满 100 起每帧 `ce_item_collect_all()`、线下 −0.3/f、擦弹 +8/graze、被弹/关卡归零 | 🟡 数值待实跑 | `signal_core.c` + 201 断言 |
| AR3 | 蓝点分 = 信仰 ×(擦弹倍率 + 信号强度)，**只落在金钱道具的分数上、不改 `MONEY`** | ✅ 用户定案 | `signal.c` 的 `on_item_score` 段（`*value = pv × 10`）|
| AR4 | **两卡同装时 111 与 110 算出同一个 PV**：`signal_core_pv_111_scaled()` == `mof_core_pv()`（逐字断言）| ✅ 实现即此 | `tests/test_signal_core.c` §12.6；共用件原来重复计入基础，已修（见 §AP/AP6 附注）|
| AR5 | B 双选与 110 同规则（库存 == 0 且 ≥2.00 档 ⇒ 扣 1 档、下限 1.05），常数取自 `faith_core.h` 的 `FAITH_POWER_*` | ✅ 用户定案 | `signal.c`；`EVENTS-CONTRACT` §5 |
| AR6 | 灵击消弹用 `ce_cancel_radius` 的**返回值**当颗数 + `ce_cancel_latch_claim` 帧闩锁（与 107/117 同帧只一家动手）| ✅ 实现即此 | `signal.c` |
| AR7 | 长持续效果收尾：绿点注册 / 无敌与计分声明 / 6 个 HUD VM / HUD 槽，全在 `on_delete` 与 `on_run_reset` 收干净 | ✅ | `signal.c` 生命周期段 |
| AR8 | HUD 走 `ce_hud_slot_claim` 货架（块 96×20、5 格），坐标/配色是 `❓ 自定`（TH11 信号槽是零售切图，我们用 `drawRect` 脚本 104 拼，**不冒充复刻**）| 🟡 待实跑 | `signal.c`；`HUD-LAYOUT.md` §2 |
| AR9 | 110 不再替 111 画信号（原 `mof.c` 的 `"sig 0"` 占位已删，连同那张 HUD 的 VM 与常量）| ✅ | `mof.c` 的 `on_draw` |

**实跑清单**：① 在回收线上方看信号槽涨、线下看衰减、被弹归零；② 满槽 ⇒ 全屏道具自动飞向自机；
③ 金钱道具得分是否随信仰/信号变化（`on_item_score` 的**第一个真用户**，review §7.8 要求按「首用」验）；
④ 与 110 同装 ⇒ 蓝点分与两卡各自的 HUD；⑤ 商店删卡后 HUD 与池是否干净。

## §AS. 神灵庙（id 113）—— 灵界 trance（2026-09-12）

**没有手写机器码、没有新断点**；挂**虚表 `+0x14`**（`on_death_frame2`，死后层），**不挂 `+0x10`**
（救援层 = `cancel_impending_death` `0x45d590`，113 不做救人）。纯逻辑在 `cards/trance_core.c`。

| # | 断言 | 可信度 | 证据 |
| --- | --- | --- | --- |
| AS1 | 落点 = **虚表 `+0x14`**（objdump 实证：`.rdata` 槽基址 `0x334` ⇒ `0x348` 是 `+0x14`），`+0x10` 本文件无函数体 | ✅ 一手 | `trance.c` + `nm`/objdump 对账 |
| AS2 | 「死后照死、只是先开灵界」：`+0x14` 里**不调** `cancel_impending_death`、不读自机坐标、不改生命与死亡流程（SPEC §7 三条禁令写进函数注释）| ✅ 实现即此 | `trance.c` |
| AS3 | 113 **不进救援层**：`trance_rescue_resolve(mask, 113, …)` 硬拒返回 0（分层做成可执行断言，不是注释）| ✅ 实现即此 | `trance_core.c` + 4 条单测 |
| AS4 | 救援层规则（由本卡提供契约、**执行需 107/117 接手**）：一次中弹**烧掉全部**救援者、顺序 **107 → 117**、全屏消弹只给第一家（闩锁）、无敌取 **max** | ⏳ 执行层未接 | `trance_core.c` 的 `trance_rescue_order()`；`EVENTS-CONTRACT` §5.2 |
| AS5 | 「中弹无伤」= 每帧 `ce_invuln_request(113, N)`（一手原语 `th13:0x444ca6` 在 TH18 无对应点）⇒ **本卡最大的近似**，`❓ 自定` | 🟡 待实跑 | `trance.c`；`TRANCE_INVULN_DECLARE 30` |
| AS6 | 击破 → 神灵 走 `on_enemy_death`（**排除 `KILL_ALL`**、要 `CE_EDEATH_F_REAL`），回调里只 O(1) 入队 | ✅ 实现即此 | `trance.c`；断点见 §AM（导出与 patch 已收口）|
| AS7 | 长持续效果收尾：先退声明层（无敌 / 计分）→ 收 HUD VM → 清私有状态，`on_delete` 与 `on_run_reset` 共用 | ✅ | `trance.c` 的 `trance_clear()` |
| AS8 | 5 个音效 id 与 HUD 坐标是 `❓ 自定`（TH13 的音效 id 在 TH18 无意义）；符卡碎片无 SDK helper ⇒ 只记日志不落笔 | 🟡 待实跑 | `trance.c` 常量表 |

**实跑清单**：① 中弹 ⇒ 不掉命、被拉进灵界；② 灵界期间无敌与得分提升是否生效；
③ 与 107/117 同装时救援层的烧毁顺序；④ 商店删卡（倒吊人 / 原版空白卡）后无残留 HUD。

## §AT. 星莲船（id 112）—— 飞碟 / 令牌（2026-09-12）

**没有手写机器码、没有新断点**；令牌与 UFO 本体都走 `ce_spawn_enemy()`（**占位 ECL 名 `"NoEnemyDebug"`
= 原版自带的惰性壳**，真 ECL 子程序待资产线）。纯逻辑在 `cards/ufo_core.c`（**零浮点**，见下）。

| # | 断言 | 可信度 | 证据 |
| --- | --- | --- | --- |
| AT1 | 令牌/UFO 用 `ce_spawn_enemy` 造；`spawn+0x14`（life）< 1000；**不在死亡栈里造** | ✅ 实现即此 | `ufo.c`；`engine.h` 的 `CE_SPAWN_*` |
| AT2 | 击破判据走 `on_enemy_death`：只认 `KILLED` / `BOMB_KILL`，**排除 `KILL_ALL`**（过场清场会 `memset(enemy+0x6240)` 并清主掉落，计进去会刷满计数）| ✅ 一手 | `ufo.c`；§AM；`DEATH` §1.3 |
| AT3 | 吸收链围栏三件：出屏（`\|x\| >= 200.0` 含等于、`y > 472`）/ 自机 30 px 收点圈（`player+0x4798c`）/ 跳过 state 3·4；**并清零 `vel`**（`item+0xc38/0xc3c/0xc40`）| ✅ 一手 | `0x445e1b`/`0x445e28`/`0x4b9340`/`0x4b93a4`；`0x445bf5`–`0x445c05` |
| AT4 | 「置空槽」与引擎自己的出界删除**同构**（`0x446595` vs `0x445e38` 逐指令同构，四步：`state=0` → 删 anm VM → anm id 清 0 → freelist 摘挂）；摘挂是 **LRU 移动、不是还回空闲表** ⇒ 槽要等整池轮一遍才复用 | ✅ 一手 | `ufo.c` 的 `ufo_item_relink` 与 `0x4465b4`–`0x4465d2` 逐寄存器对照 |
| AT4b | ⚠ **`ufo_item_relink` 是「直写引擎链表」的新用法**（无先例）：一手基址是 `*(head+4)` 而**不是** `*(item+4)`（审查抓到：写错 = 自赋值 = 空操作 = 链表损坏）。卡侧带两个 NULL 守卫（引擎那两条无守卫 ⇒ 我们只会「少崩」） | 🟡 待实跑 | impl 文档 §2.3；AUDIT 要求实跑确认不崩 |
| AT4c | 审查三轮累计抓到 **3 个「让整张卡不工作」的 BUG**（颜色恒 0 / 轮转表顺序使每次召唤都是彩虹 / `ufo_item_relink` 基址写错），另 10 条小 bug；每条都记进 impl 文档 §2.3–§2.5 | ✅ 实现即此 | `tests/test_ufo_core.c` 675 断言（含穷举不变量）|
| AT5 | 纯逻辑层**零浮点**：`fill` 存百万分比 `int32`，唯一出口 `ufo_fill_ratio()` 是 `static inline`（x87 硬门 + `float` 尾数不够，见 impl §2.1）| ✅ 实现即此 | `ufo_core.h`；`make dllx87` = 0 |
| AT6 | 凑组：用户只拍了「三同色 / 三色各一」⇒ **常规路径不丢最旧**；仅「槽满且不成组」时丢最旧**脱困**（否则 `{红,蓝,红}` 会死锁 ⇒ 本卡再也召唤不出 UFO）| ⏳ 待用户拍板 | `ufo_group_put()` + 单测；impl §2.2 |
| AT7 | 长持续效果收尾：令牌与本体都置 `flags_lo` 位 25「本帧删」**静默撤退**（不析构、UAF 安全）；`on_delete` 里同样静默撤退（撤不掉就说清）| ✅ 实现即此 | `ufo.c`；`0x42d200` + sweep `0x42dfc5` |
| AT8 | 未做（缺口）：真 ECL 子程序（令牌飘行 / 四色本体，一手名字表 `th12:0x4b33e0`）、`default.ecl` 的 thcrap 文件映射、8 件贴图、`ce_item_despawn()`、循环音停止 API | ⏳ 资产 / SDK | impl 文档 §4 |

**实跑清单**：① 打敌人看令牌是否落下；② 凑 3 枚同色是否召唤 UFO；③ UFO 是否吸走 type 1/2 道具并计数；
④ 击破 UFO 看奖励；⑤ **扫池实测帧耗与槽泄漏量**（三层早停已做，但无 exe ⇒ 帧耗未测）；
⑥ 与 110/114 同装时三张卡都在扫同一个道具池（成本叠加从未评估过）。

## §AU. 妖妖梦（id 107）—— 樱点 / 森罗结界（2026-09-12）

**没有手写机器码、没有新断点**；全部走既有事件 + 声明层（无敌/计分）+ HUD 槽。纯逻辑在 `cards/senro_core.c`
（整数定点、零浮点），卡侧 `cards/senro.c`。一手研究 = `local/_scratch/research-107-th07-remaining.md`。

| # | 断言 | 可信度 | 证据 |
| --- | --- | --- | --- |
| AU1 | 道具三档：小樱点 **樱点 +100**、`+C` **+30**、★（type 7）**+20**；★ **不涨**最大樱点 | ✅ 一手（订正 research）| `th07:0x43370b`/`0x433742`/`0x433958`；`0x42f69f` 逐指令只有一个写点 `[+0x961c]` |
| AU2 | 最大樱点的写点全 exe 只有难度表与 `th07:0x42f789` ⇒ 难度初值 200k/200k/250k/300k/400k | ✅ 一手 | `th07:0x42eb7a`–`0x42ec57`、`0x42f789` |
| AU3 | 结界 **7 s = 420 帧**（研究里 540 vs 420 的矛盾按用户口径取 420）| ✅ 用户定案 | `SENRO_*` 常量 |
| AU4 | 自然结束 ⇒ 樱点与最大樱点**各 +10000**（`push 0x2710` ×2）；进分数的是 `(樱点−基准)`（`imul ×10` 与 `idiv 10` 相消）| ✅ 一手 | `th07:0x441670`；research §2 |
| AU5 | 中弹 / 放炸弹 ⇒ 灵击崩结界：全屏消弹 + **40 帧无敌**（走 `ce_invuln_request` 声明层，层取 max ⇒ 不截短别人）+ 小樱点入账 | ✅ 一手 + 实现即此 | `th07:0x441bd0` / `0x441d8e`；`senro.c` |
| AU6 | 同帧多张卡都要消弹时只有一家动手（**107 拿账**）⇒ `ce_cancel_latch_claim(107)` | ✅ 契约 | `EVENTS-CONTRACT` §5.2 |
| AU7 | 擦弹加成：**高速 80 / 低速 30**（低速位 = `[player+0x240b]`，一手写点 `th07:0x43efa4`/`0x43f0ba`）| ✅ 一手 | `th07:0x43ed5f`–`0x43eda9` |
| AU8 | 全部状态在 `ce_state`（`_Static_assert ≤ 496`，实测 ~144 B）；`on_delete` / `on_run_reset` 收干净；**有世界 VM**（结界特效 ✅ 见 §BO-6；`senro_clear()` 在 `memset` 前 `senro_fx_clear()` 全删 ⇒ 删卡路也收得干净）| ✅ | `senro.c` |
| AU9 | ⏳ **低速命中通道被 SDK 挡掉**：`on_shot_damage` 的 handler 有聚焦 / bomb 门 ⇒ 命中通道今天只覆盖高速（低速那支代码在、走不到）| ⏳ 待用户拍板 | `sdk.c` 的 `BP_ce_shot_damage` 门 |
| AU10 | ⏳ 点道具分值基线用一手 `50000`（th07 蓝点满额）⇒ 会把金钱道具抬到 5–8 万；与 110/111 同装时按契约 Σ | ⏳ 待实跑 | spec §9 |

**实跑清单（spec §10 共 11 条）**：① 三档道具的数值与日志；② 满值开结界（SE + 震屏）与 7 s 时长；
③ 结界期间全屏自动收点；④ 中弹 ⇒ **同一帧内无残留弹幕**（消弹推迟一帧，约 16 ms）；⑤ 40 帧无敌；
⑥ 与 117/113 同装的救援层顺序；⑦ HUD 三行坐标校准；⑧ 商店删卡后无残留。

## §AV. `on_shot_damage_any`（2026-09-12，107 的低速命中通道）

**没有手写机器码、没有新断点**：与 `on_shot_damage` **共用** `0x45f2a2` 这一个站点，只是在 handler 里
**先**发一份不按聚焦过滤的、再走老事件的策略门。

| # | 断言 | 可信度 | 证据 |
| --- | --- | --- | --- |
| AV1 | 老事件（`on_shot_damage`）的门**一个字没改**：仍是「非聚焦 + 非 bomb + `param_6 == 0`」⇒ 114 的「非聚焦命中掉 P」语义零变化 | ✅ 实现即此 | `sdk.c` 的 `BP_ce_shot_damage` 两段顺序 |
| AV2 | 新事件发在**非 bomb 帧**的全部自机弹命中（含聚焦），`focused` 由 SDK 传；`damage` 已钳 ≥1、`pos` = 该敌世界坐标 | ✅ 一手（同断点的实参语义）| `0x45f2a2` 窗口；AUDIT §Z15 的 pos 源结论 |
| AV3 | 对既有卡**零影响**：`ce_hooks_t` 用指定初始化器 ⇒ 未声明 `on_shot_damage_any` 的卡该字段为 NULL，SDK 跳过 | ✅ | `make check` 全绿（35 张卡的行为卡对账不变）|
| AV4 | trace 槽 `0x88`：`CE_TRACE_SLOTS` 32 → 40（槽号 /4 当下标 ⇒ 必须覆盖到 `0x9c`）| ✅ | `sdk.c` 的 `ce_sdk_trace` 边界 |
| AV5 | 消费者：**107 森罗结界**（高速/低速两条命中通道，spec §9 ① 的缺口）| ⏳ 待接线 | 107 卡侧改为按 `focused` 分流 |

**实跑清单**：① 只装 107 ⇒ 高速命中与低速命中都记账（低速原来是死代码）；② 只装 114 ⇒ 掉 P 行为与改动前一致；
③ 107 + 114 同装 ⇒ 两者各按自己的门记账，不互相影响。

## §AW. 106 红魔乡的两条量纲错（2026-09-12 独立对抗审计抓到）

**背景**：106 的卡侧与单测都是绿的（40168 断言），但**尺子本身错了** —— `tests/test_th06rank_core.c` 的换算宏
自己写错 10 倍（`CENTI = p/1000` 而注释写「1 原值单位 = 1000 PERMILLE」），于是同一处把「60」与「+0.06 rank」
**同时断言成真**。这是「测了但测错」的典型：断言数量不等于覆盖质量。

| # | 断言 | 可信度 | 证据 |
| --- | --- | --- | --- |
| AW1 | 单位锚点 = **th06 原值 × 1000**（进位阈值 `100000` = 1.00 rank ⇒ ×1000 后 100 个原值单位）；`frame` / `life_full` / `bomb` / `miss` 四条一手值都落在同一锚点上 | ✅ 一手 | `th06rank_core.h` 的 `_Static_assert` 组（本次新增 14 条）|
| AW2 | `T06_INC_GRAZE` 原写 **60**，spec §3 要 **+0.06 rank** ⇒ **小 100 倍**（要擦 1666 次才进 1 档，应 16 次）| ✅ 已修 → **6000** | 同上静态断言 |
| AW3 | `T06_DEC_OFFSCREEN` 原写 **300**，spec 要 **−0.03** ⇒ **小 10 倍** | ✅ 已修 → **3000** | 同上 |
| AW4 | 两条 ÷5 通道（蓝点 / 小 P）的**最终值复核无误**（÷5 后 6000/600/200 = 0.06/0.006/0.002；装 110/111 时 ×5 回 0.01）| ✅ 一手 | 只改注释，未动数值 |
| AW5 | `d_life * T06_INC_LIFE_FULL` 的 int32 乘法在库存异常跳变时**有符号溢出 UB** ⇒ 加 ±100 档饱和钳（正常恒 ±1，可观察行为不变）| ✅ 已修 | `th06rank_core.c` |
| AW6 | 单测换算宏改成 `p / T06_UNIT_PERMILLE(1000)` 并**直接断言 spec 那一列**（`RANK_MILLI()`），尺子不再自证 | ✅ 已修 | `tests/test_th06rank_core.c`；40188 断言全绿 |
| AW7 | 审计报告的「`CE_PLAYER_FOCUSED()` 空指针会崩」**不成立**：宏是 `((CE_PLAYER() != 0) && *(int32_t *)(CE_PLAYER() + 0x476cc) != 0)`，`&&` 短路 ⇒ 指针为空时右侧不求值（`engine.h:275`）。主机复现的 SIGSEGV 来自读**假地址** `0x4cf410`，不是这个宏 | ✅ 一手 | `engine.h:275`；卡侧「先取一次指针」的写法仍然保留（更清晰）|

## §AX. 删卡收尾全量对抗审计（2026-09-12，35 张卡两批）

**方法**：从 `CE_CARD` 的实际展开读出钩子表 → 列出每张卡**跨帧留下**的东西（ANM VM / HUD VM / 子机槽 /
自建敌人 / 伤害源 / 池·队列·环 / 全局旋钮 / `ce_state`）→ 顺着 `on_delete`（虚表 `+0x50`）的**实际函数体**
（含它调的 helper）判定归属。判据 = `METHOD.md` 铁律：删卡都发生在**商店**里，那一刻卡的 tick 全停。
**58–80（23 张）与 106–113（8 张）分两批独立审计，各自修完并自验证。**

| 批次 | 结论 |
| --- | --- |
| 58–80 | 21 张干净；查出 **2 处真缺口 + 1 处加固**（70 腐化 / 58–62 皇家 static / 67·72 判据），逐条见下表 |
| 106–113 | 5 张干净、3 张补齐（110×111 共用池连坐 / 108 子机释放 guard / 107 HUD 坐标缓存），逐条见下表 |

**复核 2026-09-10 那次审计（NEXT.md 第 36 轮）**：主体成立（67/72 的 `stop()` 脚本、71/77 的子机槽、
75/78 的 HUD 数字、64/66 的自灭脚本、115 的 `panel_hide` 逐行核实无误），但**三处不准**：
① **漏了 70**（它写的是「跨关不重置的全局上限」，既非每帧重写也非按卡聚合）；
② **60 的理由错、结论对**（60 的四参只在 `on_load` 写、不是「每帧重写」；结论成立的真因是**删卡只可能发生在
商店/整段装卸时刻**，而每次关卡装配都会跑 `Player::reset` `0x45abb0` 把四参写回默认 —— 一手 `0x4441bf`/`0x444409`）；
③ 没提 58–62 的文件级 static。⚠ 另：`local/_scratch/audit-open-items-58-80.md` 对 60 的描述不成立，
且它建议的修法（`on_delete` 里写回 `{5,30,70,70}`）会**打掉 Nitori(21) 的 `{10,30,110,110}`** ⇒ **别照做**。

### AX-补：跨卡交互（只报告，未擅动）

| # | 事项 | 处置 |
| --- | --- | --- |
| AX1 | **116 / 117 不认领 HUD 槽**，与 107（认领 `(257,924)` 200×40）与 111（96×20）**今天就已经压在一起** | ⏳ 116 可平移到货架（它的常量正好落在第一格）；**117 被用户按截图整排左移到 x=88** ⇒ 要保位置就得给分配器加「自带锚点」这一支（HUD-LAYOUT §5 已列为待办）⇒ **报用户拍板** |
| AX2 | **117 的三处消弹没认领 `ce_cancel_latch_claim`** ⇒ §5.2 细则 2 只对 107/110/111 生效，117 靠「第一家清空池」侥幸不双计 | 🔧 本轮修（`animal.c`）|
| AX3 | 107 的中弹路**推迟一帧**才 claim，claim 失败就丢 pending（将来任何在 `+0x10` 就消弹的卡会让它的「全屏消弹转小樱点」静默丢失） | ⏳ 建议 107 改「下一帧重试」 |
| AX4 | `yk_withdraw_all()` 装不下时 `break` 却仍无条件 `yk_map_reset()` ⇒ 剩下的使魔变孤儿真敌人（今天唯一调用方传满容量，不可达） | ⏳ 低优先 |
| AX5 | 112×114 道具池：吸走走 `state=0`（跳过 collect/commit ⇒ 114 批集看不到、不双计），清尾四步与引擎逐指令同构 ⇒ **不会给 114 留坏链** | ✅ 复核通过 |
| AX6 | 113 的 `ce_score_declare(113, …)` 目前**没有落笔者消费**（spec §9 #5 载体未拍） | ⏳ 待用户拍板 |

## §AY. 敌人发射口闸（108 的「使魔死了 ⇒ 关掉那一槽」）—— 2026-09-12

**结论：能关，闸不是「写某个字段」而是「跳过发弹三步」。** 一手研究：
`local/_scratch/research-108-shot-slot-off.md`（371 行）。

| # | 断言 | 可信度 | 证据 |
| --- | --- | --- | --- |
| AY1 | 原定的 `0x434019` **装不了断点**（单条 1 字节 `50 push eax`，紧邻 `43401a e8 f1 37 ff ff call` 是 rel32）| ✅ 一手 | 108 spec §3.5.1；逐字节核过 |
| AY2 | 可行站点 = **`0x433ffa`**（`f30f108f44510000` = `movss xmm1,[edi+0x5144]`，**8 字节、窗内无相对寻址**）| ✅ 一手 | 新建 `ce_enemy_shot_slot`；patch 已重生成、`sites.py verify` 210 条对上 |
| AY3 | 寄存器语义：`edi` = **`&enemy->data`（`enemy+0x122c`）**（一手 `0x430d30 add ecx,0x122c`）、槽号在 `[ebp-0x56c]`（`0x433f3d` 写）、`esi = &data + i*0x488` | ✅ 一手 | handler 里带 `esi == data + slot*0x488` 自检，对不上就不动 |
| AY4 | 掐掉 = `retaddr = 0x43402d`（该 opcode 收尾 `jmp 0x436417`）⇒ 跳过 `et_protect_range` 的 set / `call 0x427810`(`__fire`) / 复位三步；**零内存写、不动敌人生命周期** | ✅ 一手 | `0x433ffa`–`0x43402c` 逐条 |
| AZ2 | `0x446cfc` **不能**当站点（那 7 字节里有 `call rel32`）⇒ 做法是**再加一个站点**，不是搬站点 | ✅ 一手 | 逐字节 |
| AY6 | 对既有卡**零影响**：`on_enemy_shot_slot` 未声明 ⇒ 字段 NULL ⇒ SDK 直接放行；导出数 25 → **26**（`BP_ce_enemy_shot_slot`），`dllverify` 上限同步改 | ✅ | `make dllverify` |
| AY7 | 可选 site B（`0x433de7`，`etNew` 边沿 = 「ECL 重新武装这一槽」通知）**本轮不做** | ⏳ | 研究 §5 |

## §AZ. `on_item_score` 只覆盖一半分支（2026-09-12 审计抓到并补）

**问题**：`collect_money_item`（`0x446b00`）算完身价后有**两条路**：
- **向上分支**：`0x446c01 comiss` + `0x446c04 jae 0x446cb4`（自机在回收线上方）**或** `0x446c0a cmp [edi+0xc74],3` + `0x446c11 je 0x446cb4`（道具真空飞行）
  ⇒ 落到 `0x446cf6 lea eax,[edi+0xc2c]`（= `on_item_score` 的原站点）。
- **线下分支**：两者都不成立 ⇒ `0x446cb2 jmp 0x446cfc` 直达收尾，**绕过 `0x446cf6`**。

⇒ 只挂 `0x446cf6` 时，**线下收的金钱完全不派发事件**（110/111 的 PV 注入对它无效），而卡内计数却按「线下系数」记了账 ⇒ 口径不一致。

**修法**：**加第二个站点**（`ce_item_score_below` @ `0x446caa`，8 字节 `8b45f8052c0c0000`：`mov eax,[ebp-8]` + `add eax,0xc2c`，无相对寻址），
handler = **与 `ce_item_score` 同一份实现**（`ce_item_score_common()`）。两支互斥 ⇒ 每次收集仍只派发一次事件。

| # | 断言 | 可信度 | 证据 |
| --- | --- | --- | --- |
| AZ1 | 线下分支确实绕过 `0x446cf6` | ✅ 一手 | `0x446cb2 eb 48 jmp 0x446cfc`；`0x446cf6` 只在向上分支里 |
| AZ2 | `0x446cfc` **不能**当站点（`56 50 e8 ed d8 01 00` 里有 `call rel32`），所以是「再加一个站点」而不是「把站点搬过去」 | ✅ 一手 | 逐字节 |
| AZ3 | 站点处 `esi` = 最终身价（线下分支 `0x446c91`–`0x446ca7` 已算好），与 `0x446cf6` 站点语义一致 | ✅ 一手 | 线下分支的 `esi` 就是收尾 `push esi` 用的那个 |
| AZ4 | 导出数 26 → **27**（`BP_ce_item_score_below`），`dllverify` 上限同步改；断点对账 26/26 | ✅ | `make dllverify` |

## §BA. 110×111 计分链路收口（2026-09-12，第二轮审计后）

| # | 断言 | 可信度 | 证据 |
| --- | --- | --- | --- |
| BA1 | **乘数附加通道真的被用上了**：111 在 `signal_declare_score()` 里 `ce_score_mul_declare(111, signal_core_strength(s->slot.signal))`，110 落笔读 `ce_score_mul_aggregate()` | ✅ | `signal.c` / `mof.c`；`tests/test_sdk_core.c` 的通道用例 |
| BA2 | ⚠ **量纲必须是千分点 0..1000**：`signal_core_strength()` 把 `s->slot.signal`（0..130000 的定点）换成 0..1000；直接声明裸值会让乘数大 ~100 倍（130000‰ = ×130）| ✅ | `sdk_core.h` 的通道注释 + `mof_core_pv()` 的 `× mult / 1000` |
| BA3 | 粘性语义：信号掉到 0 要**照样声明 0**；删卡由 `ce_layer_drop_card()` 一并清（`sdk.h`）| ✅ | `sdk_core.c` 的 `ce_score_mul_declare` |
| BA4 | 110 的**同一颗 P 点被记两次**已修：只保留 `on_item_collect` 那条（玩家真吃到才产生信仰）；`on_enemy_drop_pre` 不再撒点 | ✅ | `mof.c`；判据函数 `mof_core_pick_p_item()` 未动（111 共用）|
| BA5 | 110 的 `stat_small` 原来「撒点 ++ / 收到 ++」共用一个字段（一颗点最多记 3 次）⇒ 拆成「收到」与「撒出」两组，日志并排 | ✅ | `mof.c` 的日志格式 |
| BA6 | ⛔ **113 暂不参与计分层**：`trance_declare_score()` 不再写 `ce_score_declare(113, …)`（改本地记账 + 日志），因为 `spec §9 #5` 未拍板而它的 10000 会被 110/111 的落笔者当**信仰**算进 `aggregate` ⇒ 污染别人 | ✅ | `trance.c`；三条接法写在注释里 |

## §BB. TH10 一手定案 + 113 灵界接线（2026-09-12 收尾）

### BB-1 TH10（`local/_scratch/research-th10-faith-and-graze.md`，408 行）

| # | 断言 | 可信度 | 证据 |
| --- | --- | --- | --- |
| BB1 | **TH10 完全没有擦弹倍率**：`0.0001f` 全 exe 只在 `th10:0x470c58` 当 `atan2` 的 epsilon，**没有 `8.99f`**，13 个计分口无一读擦弹 ⇒ 110 的 `PV = 信仰`（无乘数）**本来就是对的**；`min(graze×0.0001, 8.99)` 是 **th11** 的说法且单位未核实 | ✅ 一手 | research §1/§2；`mof_core_graze_mult()` 已改标为「th11 来源、🟡」|
| BB2 | 信仰**显示 = 存储 × 10**（存 `0x474c4c`）：初值 `push 0xc350`→`set_faith 0x418b80` 内部 `/10`；回放 `0x4292f5 ×10` 再喂；弹窗值 = 显示增量 | ✅ 三重 | research §0 |
| BB3 | 信仰写点**穷举只有五类**：大 `5000/5000/8000/10000/10000`、小 +100、微 +10、miss `−(信仰−50000)/3`、非决死灵击 −3000、**Boss 阶段窗口到时 −30000** | ✅ 一手 | research §3 |
| BB4 | ❌ 本仓原写的「全避 −30000」**措辞错**：`th10:0x412932` 的唯一判据是「阶段计时器 ≥ 窗口结束值」，函数里没有被弹/灵击/全避读位 | ✅ 一手 | 已改 review/spec 措辞 |
| BB5 | ❌ 本仓原写的「连击槽空 −10800/s / −600/s」**在 TH10 不存在**（无逐帧衰减；`0x474c54` 只被充能、只被 HUD 读）| ✅ 一手 | 已删该规则（110 卡侧同步改 no-op）|
| BB6 | 连击帧数：大信仰点 **+120**（不是 130）；蓝点 +100、P/小信仰 +60、大 P +20、微 +3、1UP +0 | ✅ 一手 | research §3 |
| BB7 | 灵击：TH10 原版门槛 = 火力 **≥1.00**、消耗 **−1.00**、**无下限**；本卡按**用户裁决**用 **≥2.00 档 / 扣 1 档 / 下限 1.05**（两者在注释里分开写清） | ✅ 一手 + 裁决 | `mof_core.h` 的 `MOF_BOMB_POWER_*` |
| BB8 | 🟡 未取到：TH10 擦弹计数变量本身的地址；th11 侧「8.99」的一手真相（th11.exe 同样没有那两个字面量）⇒ **111 的倍率单位仍建立在 wiki 转抄上** | 🟡 | research §6（含已试路径）|

### BB-2 113 灵界的「槽怎么被填」（实装期补齐）

| # | 断言 | 可信度 | 证据 |
| --- | --- | --- | --- |
| BB9 | 修前**整张卡是空的**：`trance_collect()` 的唯一调用者没有调用者 ⇒ 槽恒 0、灵界永不开启、`+0x14` 恒 SKIPPED | ✅ 审计一手 | 已接线：`on_enemy_death`（O(1) 入队）→ `on_item_frame` 帧末 `trance_kill_take()` → `trance_on_spirit_collected()` → `trance_collect()` |
| BB10 | chain 阈值：判据由 `count > 10` 改 **`>= 10`**（一手 `th13:0x4391a0`：`+0x8838++ ; if (+0x8838 > 9)` ⇒ **第 10 颗**触发）| ✅ 一手 | `trance_core.c` + 单测（负控：改回 `>` ⇒ 4 条红）|
| BB11 | 第 8 条激活条件落地：TH18 的同一全局 = **`0x4cccc8`**（全 `.text` 唯一被 `test byte […],0x30` 读 9 次；与 TH13 `0x4be830` 逐位同形）；按 **uint8_t** 读（与一手同宽）| ✅ 一手 + 🟡 状态域语义未坐实 | research 之外的本次实证；exe 镜像里该字节 = 0x00 ⇒ 正常游玩必满足 |
| BB12 | 实体层（自建 VM vs ECL 真敌机）仍是 **spec §9 #3 未决** ⇒ 击破那份**帧末按「已收」入账**（`TRANCE_KILL_AUTOCOLLECT = 1`，❓自定），**场上看不见东西** | ⏳ 待用户拍板 | `trance.c` 常量表 |
| BB13 | 计分层**保持不写**（113 的 10000 会污染 110/111 的 PV 聚合）| ✅ | §BA6 |

## §BC. 109/110/111 收尾七条 + 三个残留未定（2026-09-12）

| # | 断言 | 可信度 | 证据 |
| --- | --- | --- | --- |
| BC1 | 110 的**蓝点高度系数**改成 spec 的**线性插值**（不再是 TOP/BOTTOM 二元死代码）：线下 1px = 997‰（原 540‰）、板底 540‰、超出夹取；`mof_core_pickup_coeff()` 成了生产入口 | ✅ 实现即 spec §2.4 | `mof_core.h/.c` + `mof.c`；单测含手算值 |
| BC2 | 小信仰点的连击改用 **+60f**（`FAITH_EV_SMALL_FAITH`），原误走绿点通道 +100f；信仰加值仍 +100 | ✅ 一手（`th10:0x41b61d push 0x3c`）| `mof_core_pickup_combo()` |
| BC3 | 大信仰点连击 **130 → 120**（130 是上限 `0x413035`，不是单次增量）；110 自带表 `MOF_COMBO_BIG`（共用件的 130 仍服务 111）| ✅ 一手（`0x41b5c5 push 0x78`）| `mof_core.h` |
| BC4 | ❌ **删掉「连击槽空 ⇒ 逐帧扣信仰」**这条规则：TH10 信仰写点已穷举、无逐帧衰减（`0x474c54` 只被充能、只被 HUD 读）⇒ `mof_core_combo_tick()` 空槽变为 **no-op** | ✅ 一手 | research §3；`mof_core.c` |
| BC5 | 111 的**关卡开场归零**补上（`on_stage_start`）——归零**只有信号槽**；Δ_111 / base / 上限 / 小绿点余数 / 连击**跨关保留** | ✅ spec §2.1 + 单测逐项 | `signal.c` |
| BC6 | 109 的 CA 无敌 **off-by-one 修掉**：改成「先递减再声明」⇒ 恰好 N 帧（C1 = 8），第 N+1 帧不再声明 | ✅ 一手/spec §5.1 | `pofv_core.{h,c}` + 循环断言 |
| BC7 | `mof_core_graze_mult()` 归属改写成「**th11 来源、🟡 单位未一手核实；TH10 无擦弹倍率、110 单装不得使用**」；灵击门槛/消耗的注释把「TH10 一手（≥1.00 / 无下限）」与「用户裁决的 TH18 口径（≥2.00 档 / 1.05 下限）」分开写 | ✅ 一手 + 裁决 | `mof_core.h` |

### 残留未定（已登记，**未擅改**）

| # | 事项 | 为什么要一手证据才能改 |
| --- | --- | --- |
| BC-a | **`ctor` 每关开场被引擎再调一次** ⇒ 110/111 的 ctor 里 `faith_slot_init_*` 每关把 Δ / 小绿点余数 / 连击 / 卡内奖残**清零** ⇒ spec 的「跨关保留」实跑不成立（奖残还会每关重来）。建议用 `ce_fresh_acquire()` 给**重置**加门控 + 把「换局回初值」搬到 `on_run_reset` | ⚠ 单纯按 `ce_fresh_acquire` 门控会让**第一关永远不初始化**（关卡开始那次 `owned[id]` 已经是 1）⇒ 必须先一手确认「卡对象是否跨关存活 / `+0x4c` 到底在何时跑」，再定「初值放哪、重置放哪」 |
| BC-b | **TH10 线下蓝点真身 ≠ 本仓 spec**：一手 = 线上 `10×信`、**线下过线即半价**（`10×信/2 − int((点y−144)×(1/304)×(0.5×信−5000))`，板底比值随信仰 45.5%–55%），判据用**道具自己的 y**；本仓 spec 写的是「线上 1.0 平滑降到板底 0.54」 | 卡侧现按 **spec** 实现（`mof_core_pickup_coeff()` 一个函数可换）；换回一手需要 `faith_pickup_fn` **带上被收点的位置与种类**（现在只带 slot_mask/tier/value）⇒ 要动共用件接口 |
| BC-c | 111 的中绿点（+10）走 `FAITH_EV_GREEN`(+100f)：§7.5 那张连击表属 Δ_110 小节、**111 无一手依据**；111 的大绿点仍读共用件的 130（TH10 一手是 120，SA 版未核） | 需要 th11 一手的绿点连击值（th11.exe 的那两个字面量都不在，研究 §6 列了已试路径）|

## §BD. HUD 的层与暂停模糊（2026-09-12 一手复核 + 修正）

**用户要求**：后续卡的 HUD 一律画在**会被暂停模糊罩盖住**的层上；TH14（114 辉针城）例外（ZUN 动过层，复用老代码，弹字仍清晰）。

**一手（`AnmManager__render_layer` `0x4882b8` 的 UI 列表段 `0x4882f1`，逐条）**：
`layer 24..32 ⇒ 绘制组 = layer + 13`（24→37、25→38、26→39 …）；`37..45` 原样；**其它任何层兜底折到组 `0x27`(=39)**。
暂停模糊罩 = `text.anm` script59 的 UI VM（**层 38 ⇒ 组 38**，`engine/anm/th18/03-ascii-text.md` §2f）；
UI 列表尾插、组内后生成者后画（§2b.4）。

| 我们的 layer | 组 | 暂停时 |
| --- | --- | --- |
| **24** | 37 | ✅ 被罩住 |
| **25** | 38 | ✅ 被罩住（罩子暂停那刻才生成 ⇒ 组内后画）|
| 26…32 | 39…45 | ❌ 不被罩住 |
| 20–23 及其它 | 39（兜底）| ❌ 不被罩住 |
| 世界层 3–19 | — | 在游玩区 surface 上会被 blur，但全分辨率 + 过相机 3 视口 ⇒ **不许画 HUD** |

**改掉的**：`assets/ability/scripts/` 的 **99 `hud_digit`**、**104 `stock_rect`**、**105 `hud_number`** 原来是 **layer 26**（= 组 39 ⇒ 暂停时清晰）
⇒ 全部改成 **layer 25**；`build/ability.anm` 已重建，`build_ability.py --verify-only` **exit 0**。
影响面：75/78 的 HUD 数字、80 股市面板的矩形、**111 信号槽的 5 格（用的就是 104）** —— 现在都随暂停一起被罩住。

**现状核对（各卡 HUD 的层）**：
`136 hud_glyph` = 25 ⇒ `ce_hud_vm_text_*` 的用户（107 / 110 / 111 / 113 / 112）✅；
117 的 144/145/154/156/157/158 = 25 ✅；116 的 123–127 = 24 ✅；
`117/118`（TH16 时代的季节字/条）= **layer 20 且全仓无引用**（历史遗留，别照抄）。

**仍不可能被罩住的（结构限制，已登记）**：ASCII 文字（`ce_hud_text*`）——
**116 的档位数字**、**80 股市面板的全部文字**、**114 的 bonus 弹字**（`ce_popup_mult` = 引擎自己的弹字 ⇒ 用户已豁免）。
要把它们也罩住，得改成贴图 VM（先例：117 的槽/数字、107/110/111/113 的文字），属后续可选优化。

### §BD.1 P 键截图看不到 HUD —— 机制**未坐实**，24→25 的批量改**挂起**（2026-09-12 复核）

**起因**：P 键截出的 BMP（`%ssnapshot\th18_%.3d.bmp`）里我们的 HUD 大多不在图上。

**一手事实**（完整过程与逐条地址见 [`engine/anm/th18/05-snapshot-and-surfaces.md`](../../../engine/anm/th18/05-snapshot-and-surfaces.md) §5/§8.1）：
帧内顺序 = tick 整趟 → draw 整趟 → 壳 `0x472b50`（等 vblank → **输入+截图 `0x472d18`** → Present `0x472d2d`），
截图读的是**本帧**后备缓冲；`0x3a`（`0x455cf0`）把 RT 切回后备缓冲并 `Clear(0xff000000)`；
`0x3b`（`0x455c90`）关混合后把 supervisor+`0x1c8` 那个 VM 贴回来 —— 而 **UI 列表 layer 24 画在 `0x39`、layer 25 画在 `0x3e`**，
正好被 `0x3a` 劈在两侧。

**为什么没改**：初版机制是「layer 24 画进离屏面，要等 `0x3b` 搬回来才可能进 BMP」，
但主 agent 独立复核（`objdump` 直读 exe）拿到**反向证据**：`0x3b` 用的 `text.anm` `script75/76/77` 里 `ins_400` 的目标尺寸是
`224x240`/`336x360`/`448x480`（随 640/960/1280 等比），**看着只有屏幕的 35%×50%、不是整面**，
且脚本自带 60 帧/段的动画与旋转。若合成只搬游玩区那类矩形 ⇒ 机制成立（layer 24 的 HUD 确实进不了 BMP）；
若它最终覆盖整面 ⇒ layer 24 也该能进，机制只解释一部分。**两种可能都缺决定性证据**，
而唯一能判决的游戏内「`layer(24)` vs `layer(25)` 双胞胎」实验，用户当前**不能开游戏**做。

**因此本轮不动**：写 `layer(24)` 的 13 个脚本全是**我们自己**的卡（`107`–`112`、`116`、`123`–`127`），
改层号会挪动叠放次序，属「UI 表现」范畴 ⇒ 归用户裁定的**统一 UI 处理**，不在这里顺手改。
**新写的 HUD 一律用 `layer(25)`**（组 38 ⇒ `0x3e` ⇒ 直画后备缓冲，不依赖 `0x3b` 合成；与模糊罩同组、罩子后生成 ⇒ 仍被罩住）——
已下发给本轮做 110/111 HUD 资产的执行者。

**将来要批量改的话**：`assets/ability/scripts/**` 是**共享件**（脚本的 layer 也算），
要重建 `build/ability.anm` + 逐脚本 `build_ability.py --verify-only` + 在本节留痕；改前按 `HUD-LAYOUT.md` §4.1 复核「被罩住」不受影响。

## §BE. ★★ 非系统卡改动清单（协作须知 —— 动过别人的卡就追加在这里）

> **背景（用户 2026-09-12 定的纪律）**：本仓多人协作，**系统卡 = id 106–117**，其余（58–80 那批）属别人。
> 每次**动到别人的卡**（含**共享件/共享脚本**导致的行为变化），都在这里逐条记：`文件 → 改了什么 → 为什么 → 谁改的 → 怎么验的`，
> 并主动请对方复核。规则原文见 [`CLAUDE.md`](../../../CLAUDE.md) 的「协作纪律」。

| 日期 | 别人的卡 | 文件 | 改了什么 | 为什么 | 验证 |
| --- | --- | --- | --- | --- | --- |
| 2026-09-12 | **69 加倍** | `cards/double.c`（并删除 `cards/double_core.{c,h}`、`tests/test_double_core.c`）| 「掉落 ×2」从「卡里直接改 `counts`」改成 **声明式** `on_load` 里一句 `ce_drop_mult_set(69, 2000)`；主掉落补偿、`ex_drop`/`timed_ex_drop` 两表都由 SDK 的掉落层做 | EVENTS-CONTRACT §2 的「先加后乘、乘作用在最终总数」（用户 2026-09-12 拍板）⇒ 旧写法**依赖卡链顺序**、且**别的卡加到的那颗不会被翻倍** | 掉落层纯逻辑单测并入 `tests/test_sdk_core.c`（`(1+1)×2=4` 算例）全绿；`make check` exit 0 |
| 2026-09-12 | **70 腐化** | `cards/corruption.c` | 新增 `on_delete`：把全局上限 `MAX_BOMBS`（`0x4ccd64`）**写回 7** + 钳当前数 + 刷 HUD；挂进 `CE_CARD` | **删卡收尾铁律的唯一漏网**：本卡每放一发把 MAX−1，而 MAX 一局内只被开局初始化写过 ⇒ 商店里被倒吊人/空白卡删掉后，扣掉的上限**永远不还**（复现：放 3 发 7→4、删卡后每关都是 `3/4`）| 交叉编译 0 警告、x87=0；`test_*` 全绿（见 §AX）|
| 2026-09-12 | **58–62 皇家同花顺** | `cards/royal.c`、`royal.h`、`cards/s10.c` `sj.c` `sq.c` `sk.c` `sa.c` | 新增共用 `ce_royal_dismiss()`（只收「属于自己被删那一张」的演出倒计时），五张黑桃各自 `.on_delete` 调它 | 文件级 static `s_fx_card/s_fx_countdown` 靠那张卡的 `on_tick_2` 推进 ⇒ 卡被删后永远挂着；`ce_destroy_card` 会清 `owned[id]` ⇒ 该卡**会被商店重新刷出**，再买到时 60 帧后在局中冒出一声没有演出配套的 trophy 音效 | 同上 |
| 2026-09-12 | **67 青眼白龙 / 72 炎魔之王** | `cards/blue_eyes.c`、`cards/firelord.c` | `dismiss()` 的清理判据补上 `bar_bg_id` | 血条背景 VM 会在删卡时漏删（当前不可复现，属加固）| 同上 |
| 2026-09-12 | **76 古锭刀** | `cards/guding.c` | 四个钩子加 `ce_state` **NULL 守卫** | `ce_state_user()` 在 256 槽耗尽时**静默返回 NULL**，同批其它卡都判了，这张没判 ⇒ 会崩 | 交叉编译 0 警告；`test_guding_core` 23 断言全绿 |
| 2026-09-12 | **75 真银圣剑 / 78 衔尾蛇 / 80 股市** | `assets/ability/scripts/99_hud_digit.anm.txt`（75/78 用）、`105_hud_number.anm.txt` + `104_stock_rect.anm.txt`（80 用）| **`layer(26)` → `layer(25)`**，重建 `build/ability.anm` | 用户要求「HUD 必须画在会被暂停模糊罩盖住的层上」；一手：`0x4882b8` 只把 layer 24–32 折到组 37–45、**其它层兜底折到 39**，而模糊罩是**层 38** ⇒ **layer 26 = 组 39 = 罩子之后画 = 暂停时清晰**（见 §BD）| `python3 assets/build_ability.py --verify-only` exit 0；`make test-host` 全绿 |

**共享件/共享资源改动对既有卡的影响面**（同样属于「动到别人的东西」，单列）：

| 改动 | 影响面 | 回归 |
| --- | --- | --- |
| `sdk/public.h` 追加事件钩子（`on_enemy_death` / `on_shot_damage_any` / `on_enemy_shot_slot` / `on_native_shot_suppress`）| **只影响声明了它的卡**（指定初始化器 ⇒ 未声明者为 NULL）；**18 张既有卡零影响** | `make check` 全绿（含 27 组主机单测 + 行为卡对账）|
| `sdk.h` 的 `CE_CARD`（`on_run_reset`/`opdelete` 现在多清几层声明 + HUD 槽）| **所有卡**：删卡/局末时多退几个「声明层」；没声明过的卡等于 no-op | 同上 |
| `sdk.c` 的 `BP_ce_item_frame` 每帧多做三件小事（无敌声明落笔、消弹闩锁 reset、HUD 整摞淡出推进）| **所有卡**的帧成本 +O(1) | 同上 |
| **新增第二个 `on_item_score` 站点**（`0x446caa`，线下收金钱）| 目前 `on_item_score` 的声明者只有 **110/111/107/113（全是系统卡）** ⇒ 既有卡不受影响；**但语义上这是个「事件覆盖面扩大」**，将来谁用要注意 | `sites.py verify` 210 条对得上；导出 27 |
| `engine.h` / `sites.py` / `Makefile` / `th18_card_expand.def` / `patch/th18.v1.00a.js` | 所有卡的构建与断点基础（新增 `BP_ce_enemy_death` / `BP_ce_enemy_shot_slot` / `BP_ce_item_score_below`；断点导出对账门）| `make check` exit 0 + `sites.py verify` + 26/26 导出对账 |

## §BF. 112 的用户裁决实装（2026-09-12：金=蓝 / 两同一异 / HUD 第一版 / 五边形钩子）

| # | 断言 | 可信度 | 证据 |
| --- | --- | --- | --- |
| BF1 | **金 UFO = 蓝 UFO**（用户：「到时候进行 P 图素材替换即可」）：`UFO_COLOR_GOLD`（编号 4，❓自定）在**凑组归一 / 奖励表 / 击破加成 / 吸收两计数 / HP 与飘行参数 / ECL 子程序**上**逐项走蓝的同一段代码**；贴图借 `UFO_ECL_BODY_BLUE` | ✅ 实现即此 | `ufo_core.c` 的 `ufo_color_norm()` / `ufo_color_art_index()`（一手 `th12:0x44a909`：idx=(c==−1)?3:c−1）|
| BF2 | 「**金算不算独立一色**」这一格留宏 `UFO_GOLD_COUNTS_AS_BLUE`（默认 1 = 归一成蓝）；两条分支**都过单测**（925 / 919）| ✅ | `tests/test_ufo_core.c` |
| BF3 | **两同一异 = 丢第一个 token**（用户确认）：第 3 枚放上不成组 ⇒ **当场丢最旧 + 重判**；六种排列统一 `dropped=1 / filled=2 / {原第 2 枚, 新一枚}` | ✅ 一手（`th12:0x422e80` 的 KEEP2 `0x422ef8`–`0x422f12` 逐字）| `ufo_group_put()` + 单测 |
| BF4 | ⚠ 一处**与字面一手的差异**：一手 KEEP2 的字面条件是 `slots[1]==color \|\| slots[0]==slots[1]` ⇒ `{A,B,A}`（首尾同色）**不走 KEEP2、会出彩虹**；用户口径「槽满不成组 ⇒ 丢最旧」是**有意偏离**这一格 ⇒ 已钉成断言 + `ufo_group_keep2_literal_th12()` 记录，⏳ 待实机复现一次 | 🟡 | `ufo_core.c`；spec §2 |
| BF5 | **五边形提示框**：纯逻辑判据 `ufo_pentagon_hint_for_color()`（∃c 使再加一枚成组；当前规则下等价 `filled==2`）+ 卡侧 `ufo_pentagon_hint_update()` **已真驱动**（脚本 **223**，建/跟随/删）；★ 层 = **`layer(12)`**（照抄零售，**不是** 25），见 §BO-7 | ✅ 一手 + 实跑未验 | `ufo.c`；spec §5.9 |
| BF6 | **HUD 第一版**（用户：「先写，我一点一点对」）：`ce_hud_slot_claim_card(112, 112×30, LEFT)`；3 格令牌槽（20×20、格距 4）+ 血条（112×6，填充按 HP‰）+ 贴图 VM 读数；**全在层 25** ⇒ 暂停时会被模糊罩盖住 | ✅ 实现即此 | `ufo.c`；`HUD-LAYOUT.md` §4.1 |
| BF7 | 收尾：`on_delete` / `on_run_reset` / 菜单门都调 `ufo_hud_kill()` + `ce_hud_slot_release(112)`；VM 句柄放**文件静态**（`on_ctor` 会 memset 私有状态）| ✅ | `ufo.c` |
| BF8 | ⚠ **脚本 104 的默认锚点两说**（未定，实跑一眼）：`§AL20` 的两次实跑说 `drawRect` 默认 = **左上角**；`signal.c` 旧注释引 season.c 说「图心」⇒ 112 与 111 各留一个整体翻的宏 | 🟡 待实跑 | §AL20 vs `signal.c` |

**未做（诚实清单）**：真 ECL 子程序（另开一条线在解 TH12）· 零售切图（横幅/闪光/光环/令牌 6 态/血条）· 五边形框美术 ·
扩圈消弹 · 击破得分公式（仍 0 分）· 循环音 · 整摞淡出（`ce_hud_near_corner()`，HUD-LAYOUT §4.5 形制未定）。
**一次都没进游戏**（HUD 落点/锚点、金令牌、丢最旧手感、血条读数都待首验）。

## §BG. 三条交付收口（2026-09-12：112 ECL 移植 / 116·117 迁货架 / 113 神灵实体池）

### BG-1 112 的真 ECL：**从 TH12 原样移植**（`assets/ecl/ufo.ecl.txt` + `assets/ecl/make_ufo_ecl.py` + `make ecl-ufo`）

| # | 断言 | 可信度 | 证据 |
| --- | --- | --- | --- |
| BG1 | TH12 的 UFO ECL 在 **`default.ecl`**（`th12.dat` 顶层，**不在** `st0X.ecl`）：`UFO_EtBreak` + 四色本体；名字表 `th12:0x4b33e0` 只有 **4 项** | ✅ 一手 | `thdat -x 12` + `thecl -d 12 -m th12.eclm`；TH12 往返字节一致（7900 B）|
| BG2 | **「令牌」没有可移植的源**：TH12 的令牌是**道具**（type 10–15），行为在 C 侧（`th12:0x4262f5`）⇒ 我们的令牌子程序只能**新写** | ✅ 一手 | 名字表 4 项 |
| BG3 | **「飞走」是引擎驱动**：`th12:0x44a3af` 在 `timer == 0x1a4`（= `720 − 300`）写 `enemy+0x127c`（`int_vars[0]`）= `+999`，ECL 只轮询；离场 = `moveLimitReset()` → `movePosTime(300, 1, …, −64.0f)` → `delete()`（自删、不发奖）| ✅ 一手 | 逐条 objdump |
| BG4 | ⚠ **420 帧是 UFO 本体的寿命**（不是令牌的）：`UFO_TOKEN_TTL_FRAMES`（现 2400）在 TH12 无源 ⇒ 仍是 `❓ 自定`，**别拿 420 去顶** | ✅ | 本条同时订正 spec 早前的措辞 |
| BG5 | 移植产物 9 子（4 原样 + 4 自计时 + `EtBreak`）；合并后 **零售 30 + 我们 9 = 39 子、无重名**；**编译 → 反编译 → 再编译字节一致（20028 B）**；A 组 4 个本体与 TH12 **38/38 指令相同** | ✅ 本地复现 | `make ecl-ufo` 输出 |
| BG6 | Δ1 删 `flagSet(1090)` 的理由：TH12 侧 `flags \|= 0x442` 全 exe 无消费者；TH18 的 `flags_lo & 0x21` = **伤害免疫** ⇒ 照搬会做出打不死的 UFO（spec §7 明令禁止）| ✅ 一手 | spec §7；`engine/ufo/th12/01-ufo-system.md:175` |
| BG7 | ⚠ **`anmSetSprite`（th12 op 259 / th18 op 303）的 arg1 是 anm 脚本号、不是 sprite 号**（TH12 `enemy.anm` 的 script135–138 才是四色飞碟）；反证 = `enemy.png` 的 sprite135–138 轮廓/尺寸都不同 | ✅ 量化 | 见 112 spec §3.9 |
| BG8 | ⏳ **卡点（未做）**：TH18 没有那 4 个 anm 脚本与 `ufo_anim.png` ⇒ 本体能飘/能飞走/能消弹但**没贴图**；发布还差 ① `release.py` 加一行（映射进 **patch-step3**，`abcard.anm` 同款 ⇒ PLAN §4.5 的 ❓ = **能**）② anm/贴图移植 | ⏳ 待资产 | `assets/ecl/ufo.ecl.txt` 文件尾 |

### BG-2 116 迁货架 + 117 「自带锚点」保位（用户裁决：116 可迁 / 117 位置正确）

| # | 断言 | 可信度 | 证据 |
| --- | --- | --- | --- |
| BG9 | 116（季节槽）`on_load` 里 `ce_hud_slot_claim_card(116, 104×36, LEFT)`；所有元素改成「锚点 + 原偏移」，锚点落在货架第一格 `(257,924)` 时**逐像素等于迁移前**（外框/两条/季节字/等级条/640 数字全对）| ✅ 逐项对账 | `cards/season.c`；`test_season_core` 120 全绿 |
| BG10 | 117（动物灵）用新的 **`ce_hud_slot_claim_pinned_card(117, 193×48, 88, 924)`** 登记：**5 格中心 `(88/124/160/196/232, 904) 迁移前后完全相同**、火焰偏移也相同；`clear_card` 里 release；消弹闩锁现状未动 | ✅ | `cards/animal.c`；`test_animal_core` 125 全绿 |
| BG11 | ⚠ **116 与 117 有一处几何冲突**（用户两条裁决的副产物）：116 的外框左边 `x 263..280` 压到 117 第 5 格 48×48 **画布右侧的透明边**（117 的**可见**框只到 `x 256`）⇒ 视觉上**不叠**；若要彻底消除，把 `SEASON_HUD_BLOCK_W` 104 → **112** 即可（货架会把 116 排到 x=369）| 🟡 待实跑看一眼 | 两卡的块/渲染区间对照见 116 的报告 |
| BG12 | 顺带确认：**107 与 117 的重叠是迁移前就有的**（107 认领矩形从 257 起，117 在 88..281）⇒ 本次**没有变坏**；107 是系统卡，后续若要整理再说 | ✅ | 同上 |

### BG-3 113 神灵实体池（用户裁决：不下落 / 必须跟过去吃 / 越缩越小到消失 / 不是敌人）

| # | 断言 | 可信度 | 证据 |
| --- | --- | --- | --- |
| BG13 | 实体 = **自建 VM 池**（文件级 static `s_spirit[128]`）：击破入队 → 帧末建 VM → **静止**（只写 `y+bob`）→ 每帧写 `scale`（越缩越小）→ 到期播消失段 → **玩家走到 24 px 内才入账** | ✅ 实现即用户语义 | `cards/trance.c`；断言数见 §BN |
| BG14 | `TRANCE_KILL_AUTOCOLLECT`（帧末自动入账那条兜底）**已整段删除**；全屏收点免疫是**结构性事实**（本文件对 `ce_item_collect_all` / `ce_cancel_all_bullets` / `ce_cancel_radius` 的**代码引用 = 0**）| ✅ | grep + 单测不变式（离得远 ⇒ 槽恒 0；走到跟前 ⇒ 恰好 +2；其余放到期 ⇒ 不入账）|
| BG15 | 载体借 `129_season_bullet_summer`（主路径 `0: stop()`、无 `scaleTime` ⇒ 卡侧 scale 不会被插值器盖掉）+ 零售 16×16 魂贴图 + 一手 TH13 `color()` 内芯染色；**一手照抄**：寿命 480 帧（`th13:0x438a61`）、缩小 1.5→0.4 / 200 帧（`astral.anm` script1）| ✅ 一手 + ❓载体 | `trance.c` |
| BG16 | **层 = 13（世界层）**，不是 HUD 的 24/25：神灵是**场上实体**（坐标来自 `ev->x/y`，与 `faith.c` 的 `FAITH_LAYER` 一致）；世界层 3–19 本来就在暂停模糊的游玩区 surface 上 ⇒ 同样会被模糊 ✓（HUD 才必须 24/25）| ✅ 语义正确 | `HUD-LAYOUT.md` §4.1 |
| BG17 | 收尾：`trance_clear()`（`on_delete`/`on_run_reset`/`on_stage_start` 共用）里 `trance_spirit_clear_all()` 放在 `if (!s) return;` **之前**（含正在播消失段的 `dying` 那批）；`ctor` 刻意不清池（每关会被重调）| ✅ | `trance.c` |

## §BH. 108 的孤儿敌机陷阱（2026-09-12 修）

**问题**（106–113 审计列的「建议但没动」之一）：`yk_withdraw_all(m, out, max)` 在缓冲区不够时 `break`，
却**无条件** `yk_map_reset(m)` ⇒ 还没撤的那几条被从表里抹掉，而它们的**真敌人还在场上**：

- 卡再也不认识它们（`fam_id` 归零）⇒ 既不钉位置、也不会被「静默撤退」收掉 ⇒ **孤儿敌机**；
- 止射闸的判据（`fam_seen`）也跟着失忆 ⇒ 那些槽的闸状态与场上不一致。

**复现条件**：只有「缓冲区 < 待撤条目数」时才触发；今天唯一调用方传的是 `YK_FAM_MAX`（= 表容量）⇒ **不可达**；
但这是**不变式级**的坑（表里有条目 ⇔ 场上那个载体还在），一次改调用点的参数就会踩。

**修法**：循环里被截断时置 `truncated`，**只有全部处理完才 `yk_map_reset()`**；被截断时表保持原样、下一帧接着撤。
单测补一条：表里 2 条 + 只给 1 格缓冲 ⇒ 撤 1、**表里还剩 1**（`youkai_core` 220 → **229** checks 全绿）。

| # | 断言 | 可信度 | 证据 |
| --- | --- | --- | --- |
| BH1 | 缓冲区不够时**不许丢条目**（截断就不清表）| ✅ 实现即此 | `cards/youkai_core.c` 的 `yk_withdraw_all()` |
| BH2 | 不变式保持：**表里有条目 ⇔ 场上那个真敌人还在**（`fam_id` 只在「已经发出 WITHDRAW 动作」时才清零）| ✅ | 上条 + 单测 |
| BH3 | 今天不可达（唯一调用方传满容量）⇒ 属**加固**，实装行为零变化 | ✅ | `cards/youkai.c` 的 `yk_drop_all()` |

## §BI. 本轮「官方素材」四件事（2026-09-12）—— 含**共享件**改动，协作须知

用户口径：素材一律用**官方零售图**（`local/` 里 th07–th19 的已解包资产），不许自己画；
点名四样：**信仰点（TH10 绿点）、飞碟／令牌（TH12）、UFO 本体（TH12）、樱点（TH07）＋时点（TH08）**。

### BI-1 共享件改动（★ 影响面 + 回归）

| 文件 | 改了什么 | 影响面 | 回归 |
| --- | --- | --- | --- |
| `assets/ability/entries/ORDER.txt` | **只追加 38 行**（UFO 16 / 令牌 12 / 神灵 10 / 绿点 1 / 樱点 2 / 时点 1 …），一字未删未改 | 既有卡的 entry/sprite 号**按行序算出**，追加在末尾 ⇒ **号不变** | `python3 ../assets/build_ability.py --verify-only` 过；`git diff` 只有 `+` 行 |
| `assets/ability/scripts/**` | 新增 26 个脚本文件（**164–188**） | 脚本号 = 零售 68 + 按号排序下标 ⇒ 末尾追加**不动** 68–163 的号 | 同上 |
| `native/anm_ids.h`（生成物） | 重新生成 | 167 → 313 行 | `git diff -U0` 的 `-` 行为 **0** ⇒ 既有宏一字未改 |
| `native/build/ability.anm`（生成物） | 重新生成 | 7,315,288 B；零售 entry 7 + 218、script 68 + 121 | `--verify-only` 过：**零售 spec / 贴图逐项一致** |
| **`assets/anmlib.py`** | `_TOKEN` 正则 `@([A-Za-z_]\w*)` → `@([A-Za-z_]\w*)@?` | 让 `@NAME@` 这种写法也能用（多吃一个收尾 `@`）。老写法 `@NAME` **零影响** | 全仓既有脚本（无一使用收尾 `@`）重建后 `--verify-only` 过 |
| **`assets/build_ability.py`** | `mapping` 追加 `SCRIPT_<ID>` → 脚本号（来自同一份 `plan()`，键与 `anm_ids.h` 的 `CE_ANM_ABILITY_SCRIPT_<ID>` 同名同号）；撞名则 `SystemExit` | 让脚本能写 `scriptNew(@SCRIPT_XXX@)` 而不是硬编码数字。既有脚本**一个都没用** `SCRIPT_` 前缀 ⇒ 对其它卡零影响 | 同上；撞名断言保证不会静默错指 |

**为什么要动后两个共享件**：同一天里同类脚本**重编号两次**（174→、200→186）。父脚本里 `scriptNew(175)`
这种字面量在重编号后会**静默指错**（子脚本不再生成，只在画面上少一块，不报错）⇒ 改成按名字引用。

### BI-2 一手结论（每条都进了 `engine/`）

| # | 断言 | 一手 | 落地 |
| --- | --- | --- | --- |
| BI1 | TH18 `anmSelect(N)` 的 `N`→文件、`K`/`S` 语义、以及 `anmSetSprite(K,-1)` = 原版「拆槽」—— 四条都钉死（映射表见文档 §1）| ✅ | [`engine/anm/th18/04-anm-slot-selection.md`](../../../engine/anm/th18/04-anm-slot-selection.md) |
| BI2 | **`ability.anm` 不在那张表里** ⇒ 112 的本体动画不可能由 ECL 直接画 | ✅ | 同上；ECL 改 `anmSetSprite(0,-1)` 当隐形载体 |
| BI3 | TH08 时点 = 道具 **type 7**（type 10 被改写成「type 7 + state 5」）；**必然进归航**（state 3 出生 vy=−2.0−rand(0.2)、+0.05/帧过零 ⇒ state 1）| ✅ | `engine/item/th08/01-time-orbs.md` §7.1 |
| BI4 | **击破 boss** 掉 100/700/1000 颗（`spellcard+0x10` 额度，每 8 帧 ≤7+6 喷）；**击破敌怪**掉 6..26（人単独档到 40）| ✅ | 同篇 §4/§5 |
| BI5 | **中弹 ⇒ 归航停住、恢复下落**：批量版 `th08:0x441530`（state 1→0、vy=−0.9）；即时版 `th08:0x4408e0`（`PLAYER+0∈{1,2}` ⇒ state 0、vy=−0.7）。★ **本文初版把 `th08:0x4413e0` 与它弄反了** —— `0x4413e0` 是「清屏后全屏收道具」| ✅ | 同篇 §7.3/§7.4（含订正段）|
| BI6 | TH07 **没有「樱点掉落指令」**：三档各走不同路径（★ = 每死敌 1 颗 + 每消弹 1 颗；小樱点 = 每消弹 1 颗、只由 boss 死亡序列那条 `mode≥3` 触发；大樱点 = 满火力时 type 0/2 全被改写成 type 7）| ✅ | `engine/item/th07/01-sakura-items.md` |
| BI7 | 107 spec §4.1 的 **TH07 type 号整体错位一格**（大樱点实为 type 7、★ 实为 type 6）—— 数值与地址都对，只是号错 | ✅ | spec 已加订正块 |

### BI-3 实装（112）

| # | 断言 | 可信度 | 证据 |
| --- | --- | --- | --- |
| BI8 | 本体+令牌可见 = **ECL 隐形载体**（`anmSetSprite(0,-1)`）+ **卡侧跟随 VM**（`ability.anm` script 164–167 / 168–173 挂到敌机 `abs_pos`）| ✅ 代码 + 门禁 | `cards/ufo.c`（+231 行）、`assets/ecl/ufo.ecl.txt` |
| BI9 | VM 收尾覆盖**全部四条路**（静默消失 / 击破 / 飞离 / **删卡只走 `on_delete`**）；`ufo_withdraw_all` 里**无条件按句柄**收本体 + 16 个令牌 | ✅ 实现即此 + ❓ 实跑未验 | `ufo.c` 的 `ufo_{tok,body}_vm_kill*` |
| BI10 | `ce_state` 预算：`sizeof(ufo_card_t)` 400 → **468 B** ≤ 496（余 28） | ✅ 实测 | `ufo.c` 的 `_Static_assert` |
| BI11 | **未验**：令牌层号 13（二手口径）、跟随可能晚一帧、死亡栈里删 anm VM 这条组合、彩虹令牌只取第一态（不换色）、金令牌与蓝同外观（用户裁决「等 P 图」）| ❓ | `ufo.c` 的文件尾清单 |

## §BJ. 跨卡耦合静态体检（2026-09-12，用户点名）

口径：全卡（58–80 + 106–117 共 35 张）一起装进一个 DLL，问题只在**共享面**上。
查了：钩子认领矩阵 / 声明层仲裁 / HUD 货架 / `ce_state` 上限 / 符号与宏冲突 / 引擎全局写点。

### BJ-1 真问题

| # | 问题 | 证据 | 后果 |
| --- | --- | --- | --- |
| BJ1 | ~~救援层没有仲裁~~ ⇒ **不是问题，是我的误读**：用户 2026-09-12 定案「**107 和 117 同时触发**」，两家各付各的代价正是设计意图（`EC` §5.2「一次中弹把救援者全部烧掉」）；`+0x10` 返回值恒 0 也不是缺陷（`Player__die` 本就忽略它）| 说明见上面附注 | **无**（行为与定案一致）|
| BJ2 | ~~113 的救援契约从没被调用 = 未接线~~ ⇒ **定位订正**：`trance_rescue_resolve()` 是**契约的可执行形式 + 回归断言**（9 条单测钉死顺序 107→117 / 全烧 / 消弹只一次 / 无敌取 max / 硬拒 113），**不是待接的 TODO** —— 详见下面 §BJ-1 附注 | `EC` §5.3（本次新增）| **真缺陷是文档**，已改 |
| BJ3 | **HUD 货架不检测重叠**：`hud_relayout()` 对 pinned 项直接 `continue`（`sdk_core.c:371`），`hud_register()` 只做**单块**越界检查、**不做块间重叠检测** | `sdk_core.c` 的 `hud_relayout` / `hud_register` | 自带锚点的卡（117）与任何货架卡可以**静默压在一起**；已知的「116 × 117 透明边」就是这类的第一个实例，但洞是通用的 |
| BJ4 | `s_hud[].reserved` 是**只写字段**：3 处写（`sdk_core.c:409/438/483`）、**0 处读**；注释写「局中 release 后保留占位（不参与重排）」，而 `hud_relayout` 只看 `used` | 同上 | 注释与实现不一致：删卡后占位**仍然参与重排**（这是设计），但字段让人以为相反 ⇒ 下一个改这里的人会被误导 |
| BJ9 | ★ **文档与定案不一致（本次误导的源头）**：`EC` 的 §1 / §4.2 / §4.3 / §5-2 四处仍写「先到先得、成功即止」+「一次中弹只消耗一个」，与同文件 **§5.2 的 ✅ 定案**自相矛盾 | 同一文件内互斥 | ✅ **已改**（四处对齐 + 新增 §5.3）|
       '四处仍写「救援型 OR（先到先得、成功即止）」+「一次中弹只消耗一个」，与 **§5.2 的 ✅ 定案**'
       '（全部烧掉、顺序 107→117）自相矛盾 | 同一文件内互斥 | ✅ **已改**：四处对齐 §5.2，并新增 §5.3 落地现状表 |


**§BJ-1 附注（BJ1/BJ2 的正面说明）**：救援层按 **用户 2026-09-12 定案** 是「**全部同时爆发**」——
107 与 117 **同时触发**、各付各的代价（`EC` §5.2）。四条细则**都不需要卡间互调**：
① 两家各自在自己的 `+0x10` 里做 + 调 `cancel_impending_death`；
② 消弹归属靠 `ce_cancel_latch_claim`（先认领者做、后到者记日志）；③ 无敌靠 `ce_invuln_request`（取 max）；
④ 决死 B 只在两家都没救时才有机会（`Player__die` 的广播位次）。

⚠ **本附注于 2026-09-12 晚订正（第二版定案）**：这里原写「**113 是「死亡已确定之后」的回光返照
（挂 `+0x14`，不在 `+0x10`）**」—— **该口径已被用户推翻**。现在的定案是：
**113 也在救援层里、且是最后一家**（用户原话「它一定在**救援层的最后**出发，**甚至比决死 B 还靠后**」），
语义是**延时**（取消死亡 → 进灵界 → 出灵界**再死一次**），实现落点 = `+0x10` **只登记** + **帧点**动手。
⇒ 逐条见下面 **§BN**（本文件）与 `EC` §5.4。

### BJ-2 靠巧合成立 / 潜在陷阱（现在不炸）

| # | 项 | 现状 |
| --- | --- | --- |
| BJ5 | `on_native_shot_suppress` 是**先到先得**（`sdk.c:784` 命中即 `return 0`，后面的卡不问）。108（聚焦止射）与 109（X 蓄力拦 Z）都会命中同一帧 | **当前无害**：两家都想要「抑制」（幂等），且 `pofv_charge_suppress()` 是纯谓词（`const`、不改状态）。但**谁往这个钩子里加副作用，生效者就由卡表顺序静默决定** —— 已在 `EVENTS-CONTRACT.md` 之外单列在此 |
| BJ6 | `ce_state` 上限被手抄进 11 个文件，**三种写法**：8 个写 `496`、`pofv.c` 写 `496 - 16`（= 480）、`stock.c`/`ufo.c`/`youkai.c` 写 `CE_STATE_BYTES - CE_STATE_RESERVED` | 常量一改就有 8 处变假断言；且 `pofv.c` 那条**比真实上限紧 16 字节** —— 109 的状态涨到 481..496 会**编译失败**，而运行时其实是合法的 |
| BJ7 | `ce_state_alloc()`（裸，上限 `CE_STATE_BYTES` = 512）与 `ce_state_user()`（上限 496）并存 | 卡若误用裸版，会连 SDK 保留的 16 字节块头（`ce_active_t`）一起吃掉 = 踩坏主动卡状态机。**现在只有 `sdk.c` 用裸版** ⇒ 无现行违规，属陷阱 |
| BJ8 | `ce_score_declare` 用 `value == 0` 表示「不参与」，落笔者 = **id 最小的非零声明者** | 契约成立的前提是「每个声明者都注册了 `on_item_score` 且非落笔者不落笔」—— 三个声明者（107/110/111）都满足；113 的声明**已停用**（`cards/trance.c:354`）⇒ 当前无洞 |

### BJ-3 查过、没问题的（记下来省得重查）

- **非 static 全局符号**：`cards/*.c` 里重复定义 = **0**（跨 TU 撞名会直接链接失败，本来也是硬门）。
- **跨头文件冲突宏**：同名不同值 = **0**。
- **声明层仲裁**：`ce_invuln_request` 取 **max**（`invuln_recompute`，不会被短请求截短）；`ce_score_*` / `ce_score_mul_*` 取 **sum**；`ce_optpose` / `ce_cancel_latch` 取 **id 最小者**，都是一手写明的规则，且各卡按规则自守。
- **引擎全局写点**：`CE_ADDR_CURRENT_POWER` / `CE_ITEM_STATE` / `CE_PLAYER_ITEM_ATTRACT_*` / `CE_SPELL_BONUS` 等热点在卡侧**只有读、没有写** ⇒ 没有「两张卡互相盖同一个字段」的情况。

## §BK. 113 / 116 改成「主动卡」（2026-09-12 用户定案）

用户口径原话：「**得改 th16 卡：它是主动卡，触发季节解放是通过 D 键切换到该卡，然后按 C**；
**灵界应该也是主动触发的，也按照该契约做**」。（th16 卡 = 116 天空璋 / 季节；灵界 = 113 神灵庙。）

### BK-1 契约（`SDK.md` §9，零售 12 张主动卡共用）

| 字段 | 语义 | 本批取值 |
| --- | --- | --- |
| `active_recharge` | 充能帧数（装填时 × `mgr+0xc58`）；**非 0 = 主动卡**（绑定时 SDK 改 flags 成主动写法、按 Tenshi 初始化两个 zTimer，引擎据此放进主动卡组 / HUD / C 键分派）| **`1`**（两张都是）|
| `on_activate(c)` | C 键发动（门：state 空闲且充能到底；SDK 已装填充能、置「释放中」）。`0` = 瞬发、`1` = 进持续态、`CE_ACTIVATE_REFUSED` = 条件不满足（**SDK 退回充能、不算发动**，卡自己放无效音）| 113 返回 0（瞬发）；116 返回 1（解放期间持续）|
| `on_active_tick(c, elapsed)` | 持续态每帧；返回 0 = 结束进收尾 | 116：`s->rel_frames > 0` |

**为什么 `active_recharge = 1` 而不是真冷却**：两张卡的**真实代价都不是时间** ——
113 的代价是**灵界槽**（600 满）、116 的代价是**季节点档位**（春/秋/冬清空槽、夏/土用扣一档）。
先例 = 117 咆哮（`cards/animal.c`：`active_recharge = 1`，代价是动物灵槽 ⇒ `!ready` 就 `ce_refuse`）。
⇒ 充能只作「SDK 主动卡机器的入场券」，**门槛全留在 `on_activate` 的 `ce_refuse` 里**。

### BK-2 实装

| 卡 | 改了什么 | 证据 |
| --- | --- | --- |
| **113** | `cards/trance.c`：新增 `on_activate`（`phase != IDLE` / `!trance_can_activate` ⇒ `ce_refuse`；否则 `trance_begin` + 入场音 + 日志）；**删掉 `on_item_frame` 里那条「满槽自动进」** | `cards/trance.c` |
| **116** | `cards/season.c`：新增 `on_activate`（`level <= 0` / `rel_frames > 0` ⇒ `ce_refuse`；否则 `release()`、返回 1 进持续态）+ `on_active_tick`；**删掉 A 键边沿**（连 `key_prev` 字段一起删）；**S 切副季节不动** | `cards/season.c` |
| 数据 | `patch/th18/cards.js`：113 / 116 加 `"category": 0`（`SDK.md` §自检的两条 FAIL 规则：主动卡必须 `category: 0`，反之亦然）；两条 `desc` 改成「D 切到本卡 + 按 C」口径 | `mkfiles.py --check` 35 张全合规；`patch/files.js` 已刷新 |

### BK-3 顺带查到、**本次没动**的一条（留给下一批）

`release()` 里**直接覆写** `CE_PLAYER_INVULN()` 的 zTimer（`cards/season.c` 的 `inv->cur = pr.invuln`），
**绕过了 `ce_invuln_request` 声明层** ⇒ 正是 `EVENTS-CONTRACT.md` §4.1 点名的那种写法：
**会把 107 / 117 / 113 给的长无敌截短**（层取 max 就是为了这个）。
（同类：`cards/sa.c`（62，**别人的卡**）也直接改无敌计时器，但它是「把引擎自己的复活无敌 280 → 420」，
不截短别人，属有意设计 ⇒ 只在 §BJ-2 记着，未动。）
⇒ 要把 116 的无敌改成声明式（`rel_invuln` 逐帧声明 + `ce_invuln_apply()` 当场补一笔），
**与本次的「触发方式」是两件事**，没混在一起做。

## §BL. 救援层顺序定案 + 「谁救了这次死亡」闩锁（2026-09-12 用户定案）

用户口径原话：「**113 这条机制能够确定说，它一定在救援层的最后出发，甚至比决死 B 还靠后**」。

⇒ **救援层顺序被钉死：107（森罗结界）→ 117（咆哮）→ 决死 B → 113（灵界）**。

### BL-1 为什么必须加一个闩锁

113 的语义是**延时**而不是免死（用户同日另一条定案：中弹 ⇒ **取消死亡** → 进灵界 → **灵界退出后重新死亡**）。
于是三家同装时会出现一个坏结果：**107/117 把死亡取消掉了（你活了）→ 113 也取消同一次死亡并开灵界
→ 灵界的窗口结束时 113 再把你杀掉** ⇒ 107/117 的资源白烧。

**修法（用户选 A）**：加一个「**这次死亡由谁救**」闩锁，**113 只在没人认领时才动手**
（不烧槽、不排重新死亡）。因为 113 被定为**最后一家**，它问的时候前面三家必然已经在同一帧报到过了
⇒ 顺序依赖性消失。

### BL-2 实装

| 件 | 内容 |
| --- | --- |
| **`sdk_core.{h,c}`** | 新增 `ce_rescue_note_reset()` / `ce_rescue_note_claim(id)` / `ce_rescue_note_owner()`（形状完全照 `ce_cancel_latch_*`）|
| **`sdk.c`** | 每帧初 reset：`ce_item_frame` 断点里紧挨 `ce_cancel_latch_reset()`；另在 `ce_sdk_setup` 里也 reset 一次 |
| **107 `cards/senro.c`** | `on_death_before_deathbomb` 里 `senro_do_libyou(..., by_hit=1)` **之前** `(void)ce_rescue_note_claim(SENRO_ID)` —— **无条件**（107 与 117 同时触发是已定案，不许因为没拿到账就不救）|
| **117 `cards/animal.c`** | 同上，`hyper_begin(s, 1)` 之前 `(void)ce_rescue_note_claim(ANIMAL_ID_NUM)` |
| **113 `cards/trance.c`** | 由并行的实装批次落地：动手前查 `ce_rescue_note_owner()`，`>= 0` 且不是自己 ⇒ 放弃 |
| 单测 | `native/tests/test_sdk_core.c` 补 10 条（541 → **551**）；覆盖「107 先报到 ⇒ 117/113 拿不到账」「reset 后 113 能拿到」「越界 id 不记」|

### BL-3 为什么「每帧闩锁」== 「每次死亡闩锁」

一次死亡横跨多帧（`Player__die` `0x45d3a0` 置 state 4 之后还有决死窗口 / 死亡动画），
**同帧不可能发生两次死亡** ⇒ 每帧重置足够。⚠️ 若将来出现「同帧两次死亡」（例如某种 ECL 双杀），
这条等式就不成立 ⇒ 那时要改成按死亡事件重置（需要 SDK 能在 `+0x10` 广播前拿到一次帧钩子，
而 `+0x10` 是**引擎自己**调的、不在我们的断点里 —— 记在这里备查）。

## §BM. 113 灵界 × 117 咆哮 / 107 结界的交互（2026-09-12 用户定案：**允许链式**）

用户问：「灵界状态中如果收集了动物灵开咆哮会怎么样？退出灵界状态的会被咆哮 cancel 掉？」
**先给代码事实**（一手，逐条落在文件上）：

| # | 事实 | 证据 |
| --- | --- | --- |
| BM1 | **咆哮不 cancel 灵界**：两套状态机**完全独立** —— 117 的 `hyper_begin()` 既不读也不写 `trance_state_t`（相位 / 槽），113 也不碰 117 的 hyper 状态 | `cards/animal.c` 的 `hyper_begin`；`cards/trance.c` |
| BM2 | 共享面只有三处，且**都已各就各位**：① **无敌**（117 是 `t->cur < frames` 才写的「只抬高」直接写、113 是 `ce_invuln_request` 声明层取 max ⇒ **互不截短**）；② **全屏消弹**（`ce_cancel_latch_claim` 同帧只一家做，另一家记日志让位）；③ **`cancel_impending_death`**（幂等） | `cards/animal.c` 的 `set_invuln` / `hyper_begin`；`sdk_core.c` 的闩锁 |
| BM3 | **灵界期间可以正常用咆哮**：117 的 `on_bomb_gate` 只在 `ready \|\| hypering` 时拦下原版炸弹；满槽只是「**就绪**」，真正「用掉」是 **X 键**（`CE_ADDR_INPUT_PRESSED & 0x002`）| `cards/animal.c` 的 `on_tick_2`（「咆哮模式满槽自动成立，X 把它用掉」那段）|
| BM4 | **但灵界退出的那次「重新死亡」是一次全新死亡** ⇒ 引擎会**再广播 `+0x10`** ⇒ 117（若那时 `core.ready`）/ 107（若结界还开着）**会照常救**，113 自己不会再救（槽被灵界抽干 + 它是最后一家）| §BL 的救援层顺序；§BK-2 的 113 |

### BM-1 定案：**允许链式**

用户 2026-09-12 选定：**允许** 107/117 把「113 的补死」再取消掉（灵界期攒到咆哮 / 结界开着 ⇒ 补死被救）。
⇒ **不加任何「禁止救援」的闸**；补死就走**普通死亡**那条路（`+0x10` 广播给全体）。
**推论（同一套规则，一并允许）**：113 **自己也可能链式** —— 补死那一刻若槽又攒到 ≥ 200
（灵界 10 秒里走过去吃神灵 = +2/槽，理论上 300 颗够重新满），它在**没人认领**时会再救一次
（再开灵界 ⇒ 再一次补死）。**天然上限 = `trance_begin` 那道 `槽 >= 200` 的门**（一手 `0x405a40`），
不另加限制。

> 为什么不照 TH13 的原意堵掉：TH13 里灵界期间 `Player::on_hit` 直接 return（根本不会触发第二次死亡），
> 所以原版**没有这个组合**（TH13 无咆哮、TH17 无灵界）⇒ 这条是**本仓的合成规则**，由用户定案，不是复刻。
> 用户选择「允许链式」（= 两张卡叠加时收益放大是**有意**的）。

## §BN. 113 的中弹救援（第二版：**延时**不是免死）+ 救援层顺序钉死（2026-09-12 用户定案）

用户口径原话两条（唯一权威）：
> 「**灵界有两个触发方式，一个是手动触发；一个是在确定中弹后，取消掉死亡，进入灵界状态；
> 灵界状态退出后，重新死亡。**」
> 「113 这条机制能够确定说，它一定在**救援层的最后**出发，**甚至比决死 B 还靠后**」
> 追问「补死要不要允许 107/117 再接一次」时的选择：「**就这样，允许链式**（灵界期间攒到咆哮 / 结界开着 ⇒ 补死被救）」

⇒ 层内顺序 **107（森罗结界）→ 117（咆哮）→ 决死 B → 113（灵界）**；
113 的语义 = **延时不是免死**（这一点与一手 `th13:0x405c3e` 逐字一致）。

### BN-1 一手证据（本次新反汇编，`local/th18.v1.00a/th18.exe`）

★ 本节**已按对抗复核订正**：第一版把落点放在 `on_item_frame` 的「第 9 帧」，那是**不可达**的
（见 BN7）。下面是订正后的完整证据链。

| # | 结论 | 证据（一手） | 可信度 |
| --- | --- | --- | --- |
| BN1 | `Player__die` `th18:0x45d3a0` = **thiscall(zPlayer\*)、无栈参、返回 void** | 全 exe 只有 3 处 `call`（`0x45cd27`/`0x45ce81`/`0x45d060`），全是 `mov ecx,<player>; call`；尾 `c3`。副作用见 BN1b | ✅ |
| BN1b | `Player__die` 干的四件事 | `+0x476ac = 4`（`0x45d4e4`）、死亡特效 VM（script `0x1d`）、`num_deathbomb_frames = 8`（`0x45d557`）、最后广播 `+0x10`（`0x45d570`）| ✅ |
| BN3 | 决死窗口 = **8 帧**，在 `Player__on_tick__body` case 4（入口 `0x45c0f4`）里收 | `0x45c0f4` 读 `+0x638`、`0x45c0fa` 比 `+0x47908`、`0x45c100` `jl 0x45c2a9` | ✅ |
| BN4 | `Player__cancel_impending_death` `0x45d590` 只写 6 个字段、**读零个计时器** | 70 B：`+0x644` bit0 / `+0x640` / `+0x638` / `+0x63c` / `+0x634` / **`+0x476ac = 1`**（`0x45d5cb`）| ✅ |
| BN5 | ★★ **`+0x0c` 的返回值决定扣不扣命，且 `commit` 与它在同一 tick** | `0x45c112` `xor edi,edi` → `0x45c114` 循链 `push edi; call [vtbl+0xc]; or edi,eax` → `0x45c12d` `jne 0x45c3d1` → `0x45c135` 否则当场 commit | ✅ |
| BN6 | ★ **113 排在决死 B 之后**（直接证据）| 决死 B 那条路在 `0x45c2a9`（`0x45c100` 的 `jl` 目标）：B 键 `[0x4ca434]` bit1 → `0x420420` can_bomb → `0x420360` do_bomb → `0x45c2d7 call 0x45d590`。它早于 `+0x0c` 的 `0x45c114` | ✅ |
| BN7 | ★★ **「帧点等 9 帧」不可达**（第一版的错）| 窗口关掉那一帧**没有**「已关但命还没扣」的中间态（BN5：同一条分支里要么被 `+0x0c` 挡住、要么当场 commit）；`+0x0c` 在 Player 线 `0x17` 广播，`on_item_frame`（断点 `0x446ee3`）在 ItemManager 线 `0x1e`（注册点 `0x4458a0`）⇒ 后者跑到时 `state` 已不是 4 | ✅ |
| BN8 | 零售自己的「取消这次死亡」= **`0x45d590` + `return 1`** | `CardEirin__…after_deathbomb` `0x40a4f0`（99 B）：`0x40a53f call 0x45d590` → **`0x40a544 mov eax,1`** → `0x40a54a ret 4`（stdcall，1 个 dword）；否则 `0x40a54d xor eax,eax` | ✅ |

### BN-2 改了哪些文件（本次）

| 文件 | 改什么 | 为什么 |
| --- | --- | --- |
| `native/cards/trance_core.h` | `trance_tick_t.re_die` + `trance_should_redie()` + `trance_state_init()`；**删掉**第一版那套排队常量 / 相位 / 字段 / 6 个函数 | 判断下沉到纯逻辑层；不可达的那套**不留死代码** |
| `native/cards/trance_core.c` | `begin` 的**手动窗升格**（一手 `0x405a4c`）；`end` 报 `re_die`；`trance_tick` 处理排队相位并集中填 `out`；`force_on_death` 支持从排队相位的升格、**删掉**第一版的「禁止再救」闸；新增排队 / 补死记账函数；selfcheck 补四条行为不变式 | 同上 |
| `native/cards/trance.c` | 新增 **`.on_death_after_deathbomb`（`+0x0c`：让位 → 槽门 → cancel → `return 1`）**；`on_death_frame2` 只记日志；`on_item_frame` 加「补死」；**删掉** `.on_death_before_deathbomb` | 落点订正到引擎自带的 `+0x0c`（BN7）|
| `native/tests/test_trance_core.c` | 新增 §M（10 组，订正版重写）+ selfcheck 相关改写；断言 **1038 → 1103** | 用户点名的四条 + 链式 + 升格 + NULL |
| `EVENTS-CONTRACT.md` | §4.1 卡表 113 行、§4.2 第 ②/③ 层、§4.3 三开表现、§5.2 细则 4 与标题、§5.3 表、§5.4 | 口径订正（113 在救援层里、顺序钉死、落点 = `+0x0c`）|
| `AUDIT.md` | 本 §BN；§BJ-1 附注加订正块 | 审计留痕 |
| `CARDS.md` | 113 行 | 卡片总表口径 |
| `NEXT.md` | 113 行 + 断言数 | 待办清单口径 |
| `docs/superpowers/specs/2026-09-12-ten-desires-113-design.md` | 末尾加**订正块** §12（不重写整篇）| 旧口径集中在 §2.1/§7/§9，订正块逐条指回 |

### BN-3 实跑要确认的四件事（❓ 未验）

1. **`+0x0c` 里真的救得住命**：日志应出现 `trance: +0x0c RESCUED ... return 1`，且**残机不减**
   （若残机还是掉了 ⇒ `return 1` 没传出去 / 引擎那一帧已经 `commit`，先查卡桩的签名）；
2. **113 确实排在决死 B 之后**：装 113 单独一张，中弹后在窗口里**按 B** ⇒ 应当是**决死 B 生效**
   （113 的日志里**不该**出现 `+0x0c RESCUED`）；不按 B ⇒ `+0x0c RESCUED`；
3. **107/117 同装时 113 让位**：日志应当出现 `trance: +0x0c rescue SKIPPED - death already claimed by card 107/117`；
4. **补死之后链式**：救下 → 灵界窗口走完 ⇒ `trance: re-die ... Player__die called` ⇒ 残机 −1
   （槽已 0 ⇒ 113 不会自己再救，符合一手「延时」）；若在窗口里又攒回 ≥200 ⇒ 允许再救一次。

## §BO. 本轮并发实装的收口与订正（2026-09-12）

### BO-1 108 妖侧 shooterset = **覆盖零售槽 `0x16`**（★ 共享件，协作须知）

**改了共享件**：`assets/sht/append_shooterset.py`（新增 `REPLACE` 表），生成物 `native/build/sht/*.sht` 各 +832 B、
`native/sht_ids.h` 多一行 `CE_SHT_SET_YOUKAI 0x16`。理由与口径：

- **不能追加**：sht 的 **40 格 `0x00–0x27` 全有主**（零售 `0x00–0x16`；`0x17–0x18` = 77 豌豆射手；
  `0x19–0x1d` = 116 季节；`0x1e–0x27` = 117 咆哮）。⇒ 108 **只能用 `0x16`**（零售唯一**无卡映射**的格）。
- **`0x16` 无卡的证据（一手，独立扫）**：全 exe `E8 rel32` 里调 `0x40a9c0` 共 **11 处**，
  压栈立即数 = `{0x0a,0x0b,0x0c|0x0d,0x0e,0x0f,0x10,0x11,0x12,0x13,0x14,0x15}`，**没有 `0x16`**；
  主炮路径 `0x45ea00` 的 `CURRENT_POWER/100 (+5 聚焦)` 也只给 `0x00–0x09`。
  ⇒ 原 `fire_rate = 0`（除零点 `0x40aa4f` 的 `idiv ecx` **没有零分支**）这格零售永远走不到，覆盖它零风险。
- **落地方式 = 构建期（不是运行期）**：新记录（9 行 832 B）接在文件尾，把偏移数组第 `0x16` 项指过去
  ⇒ 除 **`offset[0x16]` 一个 dword** 外文件逐字节不变，零售原 96 B 原地留作死数据。
  **不选运行期的三个理由**：① 那张表不是 .data，是 `Player__initialize` `0x45a7a0` 读进缓冲区后存在
  `player+0x47940` 的指针，且装载期已把偏移**就地改成绝对指针**（`0x45a830` 循环）；
  ② 零售那格只有 96 B = **1 行**，9 行形态在加载后的缓冲里没地方放（要 realloc）；③ 走运行期就得背
  「`on_delete` 还原」那条铁律 + 每张卡激活时写表，而构建期是零运行时风险。
- **影响面 = 0**：独立回读器逐字节比对 ⇒ `0x17`–`0x27`（77/116/117）的**偏移与字节零漂移**、
  零售 `0x16` 原 96 B 原地保留、模拟装载期解析全表**已无 `fire_rate == 0` 的行**。
  ⇒ **没有动别人的卡，不需要 §BE 登记**；但生成器是共享件，故记在这里。
- 参数源 = TH08 v1.00d **`ply03as.sht` 第 5 档（满 P）** 9 行（⚠ 订正：TH08 **没有 `ply0Xbs.sht`**，
  妖侧 = `plyNNas.sht`；人/妖两张名表在 `th08:0x4c7ce0` / `th08:0x4c7d10`）。
  选这一档是因为 **9 行的 `rec[+0x28]` 全为 0** = 默认发射路径 ⇒ 每个字段能 1:1 翻译，不用猜函数。
- ❓ 遗留（都写进 `cards/youkai_core.c` §3.4）：档位伤害表**故意不填**（TH08 单发伤害随 P **下降**、
  是配着路数设计的；照抄会「P 越高越弱」）· `hit_w/hit_h` 取零售 `(16,16)`（TH08 那两个字节全 exe 无读点）
  · anm 观感未验（TH08 原本 3 种弹贴图压成 1 种）· 2 行 `hdl=1`（TH08「搭档同时开火」）TH18 无对应 ⇒ 未翻译。

### BO-2 ★ 108 弹贴图的 anm **跟的是路径，不是 `opt_slot`**（规格 §3.4.1 那句要改）

规格 §3.4.1 写的「`opt_slot=0` 的弹贴图取 `pl0X.anm`、`opt_slot≠0` 取 `ability.anm`」**对卡路径不成立**。
一手：`Player__tick_shooters_for_ability_card` `0x40a9c0` 在**进行循环之前无条件**做
`player+0x10 = ABILITY_MANAGER->ability_anm`（`0x40aa03`，`0x4cf298` = `zAbilityManager*`），循环后才还原；
`PlayerBullet__create` `0x45e723` 把它存进 `bullet+0xe0` ⇒ **这条路上任何 `opt_slot` 的弹都查 `ability.anm`**。
与 [`engine/sht/th18/02-shooter-record.md`](../../../engine/sht/th18/02-shooter-record.md) §4 的标题
（「装备卡子机的弹，贴图来自 `ability.anm`」）以及仓内 `assets/check_player_anm_bounds.py:92` 的注释互证；
「117 走主炮路径所以指 `pl0X.anm`」正是另一条路径的对照。
⇒ 108 的记录 `+0x22` 全部 = **21**（零售 `ability.anm` script 21：16×16 自转弹 + 3 帧命中特效），四个机体同一支。
**规格 §3.4.1 那句待改**（该文件本轮被另一个代理占着写 §11，收口时一并订正）。

### BO-3 `enemy+0x6320` 结案（§AQ7 成立，理由精确化）+ 一条真副作用

一手见 [`engine/enemy/th18/06-enemy-inert-flag.md`](../../../engine/enemy/th18/06-enemy-inert-flag.md)：
该字段 = `zEnemyData.invuln_timer.current`，置位后命中**仍登记**（判重叠 / 吃一源一敌 tag / 计分 / 命中闪光 /
伤害进 `total_damage_including_ignored`）但 **`life` 不掉** ⇒ 它不是「可打但不计数」的杠杆，
反而会让 §3.7 的 **`life.current` 差分转嫁恒为 0**。且结构层面**不存在**「掉血 + 不计数」的位
（`ENM_CNT` 的排除闸与掉血闸覆盖同一组位），而 `ENM_CNT` 在零售 22 个 `.ecl` 里 **0 次出现**。
⇒ **§AQ7 的结论 ✅ 保留**，理由从「打不到」精确化为「**打不掉血 ⇒ 差分转嫁为 0**」；
spec §3.9 的「★ 替代杠杆」应作废（收口时与 §3.4.1 一并改）。
**★ 真副作用（要记）**：使魔既然是**真敌人**，就会被 `enemy_count_real` 照计（写点 `0x42d9ef` inc /
`0x42d2e8` dec）⇒ **炸弹门 `0x420453` / 决死门 `0x45c07b` 被放行（空场也能炸）**；
零售卡的内部门（`0x40de3f` 一族）与 ECL `enmCreate` 上限（`0x4369ec`）也会看到它；
GUI 的 per-boss 列表与 `ENM_CNT` 看不到它。⇒ 实跑清单里要专门看「空场能不能放炸弹」。

### BO-4 113 那道「让位门①」的口径订正（行为不变，注释理由错）

`+0x10`（`on_death_before_deathbomb`）在**中弹帧 N** 广播，`+0x0c`（`on_death_after_deathbomb`）
在**窗口关闭的 N+8 帧**才广播；而 `ce_rescue_note_*` 是**每帧 reset**（`sdk.c` 的 `BP_ce_item_frame`，
它在 ItemManager 线 `0x1e` 跑）。
⇒ 109/107/117 在帧 N 的 claim **到不了**帧 N+8 ⇒ `on_death_after_deathbomb` 里的
`ce_rescue_note_owner()` 让位门在**现行路线下不会触发**（`stat_rescue_lost` 那条分支同理）。
**行为仍然正确**：107/117 若真救了，死亡当场被取消、窗口根本不会开 ⇒ 引擎**根本不会叫 113**；
所以「113 是最后一家」是**引擎结构**保证的，不是靠闩锁。
那道门**保留**作「有人 claim 却没 cancel」的异常路径兜底，但**注释不能写成「同 tick」**（收口时改）。

### BO-5 108 剩下唯一的阻塞：低速子机（§3.3.1 声明层 + 数量）

`0x40a9c0` 的**膛口恒取子机** ⇒ 子机数为 0 时 108 的妖侧弹**一发都发不出来**
（日志已按原因分成「shooterset 没装」/「没有子机可出膛」，实跑一眼看得出卡在哪段）。
而 `YK_OPTION_COUNT_FOCUSED` 现在是**占位 0**（`cards/youkai_core.h`），
要落地就得实现 spec §3.3.1 那条**低速子机声明层**（`ce_option_pose_declare` + `on_option_pose_apply`，
108/111 同争一批子机对象 ⇒ id 最小者胜、由它落笔）。**该层机制用户已拍板，缺的是「数量取值」这一个数**
⇒ 属于要用户拍板的数值项，已登记 `NEXT.md`。

### BO-6 107 结界特效 VM 落地（A4 收口）+ 一条新的 TH07 规则

**做了什么**：TH07 里结界的视觉**不是 C 侧画的**，是 `EffectManager` 的两个 effect type
（type→脚本表 `th07:0x49efc0`，`{u16 id; u16 pad; void* update; void* draw}`，stride `0xc`）：

- **结界展开 / 灵击崩结界的大环 = type `0x1c` → id 731**：开结界 `th07:0x441aa4`、崩结界 `th07:0x441c19`
  都 `push 0x1c; push 自机坐标; push 4; push 1; push -1; ecx=EffectManager; call th07:0x41c610`；
  崩结界**先杀旧 VM**（`th07:0x441be5` 把 `[player+0xb7e6c]` 的 `+0x2cc` 置 0）再起新的。
- **灵击那 32 颗 = type `0x1d` → id 690**：`th07:0x441dd9`–`0x441e38` 循环（`cmp i,0x20`）；
  起始角 `0xC0490FDB` = **−π**、步长 `[0x498cfc]` = **2π/32**；`[vm+0x294]=fcos`、`[vm+0x298]=fsin`。
- 两个脚本**逐条照抄**成 `204_senro_field` / `205_senro_ring`（唯一差异 = `sprite(N)` → `@名字@`），
  由 `assets/ability/make_senro_art.py` 生成（源 = TH07 零售切图，`FIELD.png` 256×256 / `RING.png` 64×64）。
  **层 = `19`（世界层最高）❓自定**（零售脚本里没有 `layer()`，TH07 由 EffectManager 排；旁边口径 lolk 12 / ufo 13）。
- **收尾**：`senro_clear()` 在 `memset` **之前**调 `senro_fx_clear()`（1 + 32 个句柄全删、pending 清），
  `on_delete` / `on_run_reset` 走同一路，`on_stage_start` 也收一次；
  **建 VM 只在 `on_tick_2` 尾部**（中弹路在死亡栈上 ⇒ 只置 pending，下一帧兑现）。`sizeof(senro_card_t)` 144 → **316**（≤ 496 ✓）。

**★ 新查明的一条规则（TH07；值得单独记录）**：`AnmManager::loadAnm` `th07:0x44df90` 的 base
**每个 entry 前进「上一 entry 的 sprite 数」**（`th07:0x44e05b`）⇒ 由「effect type → 全局 id」反推
「哪一个 anm 文件的第几支脚本」就靠它，且有三处交叉印证：
`text.anm` 1792+36 = **1828** = `capture.anm` 的 base ✓、`eff01.anm` base **732** = 731+1 ✓、
「道具脚本 = `0x2c4 + type`」落在同一段 ✓。
⇒ 由此定出 **731 = `etama.anm` entry3（`etama4.png` 整张 256×256）的文件内脚本 0**、
**690 = entry1（`etama2.png`）的文件内脚本 10**（sprite28 = `128,112,64,64`）。
⚠ 这条规则**目前只写在本仓的生成器注释里**，还没进 `engine/anm/th07/` ⇒ 已登记 `NEXT.md` 待补文档。

**❓ 留给用户的决策 / 未验**：

1. ★ **结界「持续期」在原版没有任何视觉**（`th07:0x441330`–`0x441670` 逐条核过：那段每帧逻辑里
   一个 effect spawn 都没有）⇒ 甲是**一次性 33 帧闪光**。本轮**照抄、没有自行加长**；
   若要「结界全程挂环」是一行开关，但**那就不是复刻了** ⇒ 要不要做由用户拍。
2. 崩结界时 C 侧写进 VM 的宿主字段（`[vm+0xb4+8]`=剩余结界帧、`[vm+0xc8]`、
   `[vm+0x218]/[vm+0x21c]`=1.0、`[vm+0x220]/[vm+0x224]`=0.25、`[vm+0x14] ×= -1.0`）
   属于 TH07 effect 管理器的结构，TH18 无对应物 ⇒ **没搬**（`[vm+0x14] ×= -1` 的语义也没定死）。
3. 脚本 205 的两处 `fsetRand`/`isetRand` 是**零售原文**（同仓先例 115/89/95–97 同款，回放安全性同口径，未额外验证）。
4. 一次灵击 = **33 个 VM**（1 环 + 32 颗）；池满时只报日志，**帧耗与 VM 池上限待实跑**。

### BO-7 112 的 A7「五边形提示框」落地（★ 并把文档里一条错的层号订正过来）

**A7 到底指谁**：`TUNING.md` 原写「**113** 五边形框那类提示 UI（112 的五边形框）」—— 「113」是**串行笔误**：
113 的两份 spec 里 `grep 五边形` **零命中**，而本文件 §BF5、112 的 design spec §5.9、`TUNING.md` 的 112 行
三处都指向 **112**。⇒ 已订正为 112。

**判定 = 做（不需要用户拍板）**，三条理由：① 原版**确实有这东西、而且是脚本**；
② 触发条件是**纯引擎逻辑、一手可查**；③ **根本不需要贴图**（程序画的多边形）。
⇒ 既没有「零售切图」问题，也没有素材风格选择问题。

- **框本体**：TH12 v1.00b `bullet.anm` **script211**（扁平号 `0xd3`）= `sprite(-1); layer(12); blendMode(1); drawPolyBorder(1.0f, 5); angleVel(0,0,0.034906585f); color(255,255,255); scale(0,0); scaleTime(10,4,20,20); stop();` + 四段 `interruptLabel(7/8/9/10)`（红/蓝/绿/彩虹脉动）+ `alphaTime(4,0,0)→delete()`。**5 边 = 五边形**
- **触发**：`ItemManager__on_tick__body` `th12:0x425c20` 的令牌分支 `0x42658a`–`0x426843`：`0x42658a` 满 3 枚就不提示（并删已有框）；`0x4265aa` 只对令牌道具（类型 10..15）；`0x4265bc` 跳表 `0x426fc4` 六条 case（`10/13`·`11/14`·`12/15` 同 case）⇒ **框的颜色 = 收了这一枚会得到的那只 UFO 的颜色**
- **建/删**：建框六处 `0x4265f0` 一族（`push 0xd3; call 0x4615a0` 建 VM、层参 `0x17`=23 = 道具自己那层，`mov [edi+0x968],id`、`call 0x461970` 发标签）；**不重复建**（`0x4265d5 cmp [edi+0x968],0`）；删框 `0x461a70` + 句柄清 0
- **★ 层号订正**：零售脚本写的是 **`layer(12)`**（**不是** 25）。spec §5.9 与 `TUNING` 原写「必须 `layer(25)`」与一手冲突：框是**跟着令牌在游玩区里动的场上实体**，层 25 是「会被暂停模糊罩住」那一组，框进了 25 反而会被当 HUD 折到别组。**已按一手订正为 12**（相对关系与零售同向：零售「框 12 / 道具 VM 23」，我们「框 12 / 令牌贴图 VM 13」）
- **技术风险已查**：「建完立刻发中断」**不会跳掉脚本初始化**：`AnmLoaded__load_script_into_vm` `th12:0x454d10` 末尾 `0x454dba call 0x455630`（VM 自己先跑一帧）⇒ `drawPolyBorder`/`layer(12)` 已落地，下一帧 pending 中断才跳标签；TH18 侧同构（`0x405bf0`） ⇒ 卡侧照抄「spawn → 立刻 `ce_anm_interrupt`」（SDK 面**零新增**）

**落地**：脚本 **223**（`assets/ability/scripts/223_ufo_pentagon.anm.txt`，无 PNG / 无 ORDER 条目 ——
`@` 引用计数 = 0）+ `ufo.c` 真驱动。★ **句柄表放文件级静态区**：`sizeof(ufo_card_t)` 现在 **468 B**，
上限 496 ⇒ **只剩 28 B**，16 个槽各一个句柄（64 B）塞进 `ufo_tok_t` 当场被 `_Static_assert` 拦下（`532 <= 496`）
⇒ 改静态数组后 468 ✓。收尾链：`on_delete` → `ufo_withdraw_all` → `ufo_tok_vm_kill_all`（**只看句柄、不看 color**）
→ 贴图 VM 与框 VM 一起收；`on_run_reset`/`on_stage_start`/`on_ctor` 走同一条。

**❓ 两条要用户认一下的口径（都不是路线之争，一行可翻）**：

1. **彩虹令牌的框算哪一色**：零售按它**当前显示态**（type 13/14/15）算；我们的合成规则把彩虹当**通配**
   ⇒ 现在用既有的 `ufo_pentagon_hint_for_color()`（有单测），否则会出现「框不亮、收下去却成了组」。
   要改成零售字面口径 = 把 `ufo_pentagon_hint_update()` 里的 `t->color` 换成 `rb_state` 对应的色，**就一处**。
2. **框「我们跟随、零售不跟随」**：零售在 `create_vm` 那一刻把 pos 拷进 VM 之后就不动了；我们的令牌是敌机实体
   （真 ECL 到位后会飘）⇒ 不跟随就会「框留在原地、令牌飘走」。实体不动时两者等价 —— 属**有意偏离**，请认一下。

**📌 给后续做 112 的人**：卡私状态**只剩 28 B**（468/496）—— 再加任何跨帧字段前先算 `sizeof(ufo_card_t)`，
否则 `_Static_assert` 会拦。

### BO-8 108 低速子机声明层落地（★ 共享件：`sdk*` + `Makefile`）

**为什么要它**：108/111 的低速子机最终落到**同一批子机对象**上，两张卡各自每帧写 = 「谁后写谁赢」的竞态，
覆盖关系会**隐式依赖帧内 tick 顺序**。spec §3.3.1（用户 2026-09-12 拍板）的解法 = **单用途声明层**。

**新面**（纯逻辑在 `sdk_core`、分派在 `sdk.c`）：

- `ce_option_pose_t { int count; int anm_script; ce_option_cell_t cell[CE_OPTION_POSE_CELLS]; }`，
  `ce_option_cell_t { float fx, fy, ux, uy; }` —— **逐格**偏移（单位 = 像素）；`CE_OPTION_POSE_CELLS = 4`。
- `ce_optpose_declare/drop/frame_reset/winner/pose/frame`（`sdk_core.{c,h}`）；
  卡侧 `ce_option_pose_declare(ce_card_t*, const ce_option_pose_t*)` / `ce_option_pose_drop(...)`（`sdk.c`）。
- **选举 = 既有的「id 最小者胜」**（与 `ce_dmg_applier` 同形）⇒ 108(108) < 111(111) ⇒ 108 胜。
- **每帧清集合**在 `sdk.c` 的 `BP_ce_item_frame`（`0x446ee3`），与 `ce_cancel_latch_reset()` /
  `ce_rescue_note_reset()` / `ce_invuln_apply()` **同一处**：先 `ce_option_pose_frame_tick()`（落笔）→ 再清。
- **hooks 尾槽** `int (*on_option_pose_apply)(ce_card_t*)`（`sdk/public.h`；尾部追加 + designated initializer
  ⇒ 别的卡源码零改动）。稳态里只有被选举者被调；**退位通知**（上一帧落笔者这帧没赢）必须补叫一次 ——
  否则 108 松开 Shift 后没人叫它，`zPlayerOption` 会永远留在池里（= 高速还看得见妖侧子机）。
- **时序一手**：AbilityManager tick 优先级 `0x16`、ItemManager tick `0x1e` ⇒ 同帧内**卡先声明、帧界后落笔**。

**★ 形状缺口：为什么必须「逐格偏移」**（一手，不是取舍）：原生阵型只写**原生数组** `inner+0x50`（4 格，
`th18:0x45d8fc`/`0x45d902` → `+0x64..+0x70`），而**卡申请的装备卡子机在另一个数组** `inner+0x410`（12 格，
`th18:0x40a7a4 add edi,0xa30`）。两者共用一个每帧跟随 `FUN_0045bc90`（`0x45b8f5` push 4 / `[edi+0x670]`；
`0x45b905` push `0xc` / `[edi+0xa30]`），它对每格算 `pos = 自机位 + (聚焦 ? +0x6c/+0x70 : +0x64/+0x68)`
（`0x45bce0`–`0x45bd08`）⇒ **原生阵型永远不会落到卡池的格子上** ⇒「不写偏移、留给引擎原生阵型」**不成立**。
上限 `4` = 原生上限（`push 4` / `th18:0x45d81e mov edi,4`；卡池 12 是**池子**不是上限）；单位 = 像素
（`0x4b9310 = 128.0f`）。

**108 的数量表（用户口径「看不同机体在永夜抄和天空璋（文文）的数量」）**：
`YK_OPTION_BY_CHAR[4][5] = {{1,1,1,1,1}, {1,1,1,1,1}, {4,4,4,4,4}, {0,1,2,3,4}}`
（灵梦→紫 1 / 魔理沙→爱丽丝 1 / 咲夜→蕾米 4 是**常数**；早苗→文 `floor(P/100)`）。
**咲夜那 4 格的排布**取零售 `pl02.sht` **聚焦块** `k = T[3]+i = 6..9` 的原始 `(弧度, 半径)`，
按**引擎自己的换算**（`th18:0x45dd10`：θ = `player+0x4790c`（reset 置 −π/2，`th18:0x45ade6`）+
表值 → `th18:0x45e300` = `(r·cosθ, r·sinθ)` → ×`0x4b9464`（**−128.0f**）再减自机坐标）
**离线烘成像素**：`(±12.997, −35.708)`、`(±32.909, −19.000)` ⇒ 定点 `(±1663, −4570)`、`(±4212, −2431)`；
逐格验过 `trunc(整格像素×128)` == 引擎等价式（8 格全一致）。
**订正**：`youkai_core.h` 原写「TH08 妖侧是两只使魔」与一手不符 ⇒ 换成
**pl00 紫 1 / pl01 アリス 1 / pl02 レミリア 4、与 P 无关**（唯一 2 只是 `shottype 3` 妖侧 = 幽々子，本卡不用）。

**⚠ 一处与任务描述不符（重要）**：111 **没有可迁移的直写** —— 全仓 `CE_OPT_OFF_*` 的写点只有
`youkai.c`（108）与 `season.c`（116）；`signal.c` 里没有 `CE_OPT_OFF_*`、也没有任何 `ce_option_*` 引用
（`grep` 过）。它唯一的子机相关调用是 `signal_bomb()` 里的 `ce_repopulate_options()`（灵击扣 1 档火力后
池按新档重建的**原生要求**，不是阵型写入）。⇒ 111 只补了文件头的**接口接缝说明**，**代码零改动**；
真要把它 §9.1 的子机层做出来属于**新功能**（要先定 6 种组合各自的数量/位置/贴图 ⇒ 需要用户口径）。

**⚠⚠ 顺手修掉一个构建正确性坑（`Makefile`，共享件）**：`%.o` 的依赖表**没列全被 include 的头** ——
`ce_hooks_t` 定义在 `sdk/public.h` 里，改了它别的卡的 `.o` **不会重编**，而新的 `sdk.c` 会去读它们那截短的
旧结构（**越界读**）。已把 `sdk/public.h`（用 `$(wildcard sdk/*.h)` 兜住）与 `anm_ids.h`（`sdk.h` include 它、
`make anm` 会重写）加进依赖表，并留了一段注释说明踩坑经过。

**⚠ 另一个仓级陷阱（本次实际踩到）**：`make clean` 会 `rm -f $(PATCH)` = **删掉被跟踪的**
`patch/th18.v1.00a.js` ⇒ 之后 `make verify` 直接 `FileNotFoundError`；而 `make files` **不生成它**
（它由别的目标产），还会顺手把 `patch/files.js` 里那一项**删掉**。
⇒ 正确收拾法 = `git checkout -- patch/th18.v1.00a.js patch/files.js`（**别**用 `make files` 去「修」）。

**❓ 未定（要用户口径或实跑）**：

1. ★ **早苗在 P<100（档 0 = 0 个子机）**：`0x40a9c0` 的膛口**恒取子机** ⇒ 妖侧发不出弹，
   而原生弹被止射闸压掉 ⇒ **那一档一发都没有**。本轮**没改行为**（保持 spec §3.1 原文「有自机 + 聚焦」），
   只在 `on_native_shot_suppress` 的诊断日志里点明。要不要「`count == 0` 时不压原生弹」＝用户口径。
2. **位置口径 ❓自定**：数量照各自原作（TH08 / TH16），**位置照本作同机体同档的原生阵型**（一手）；
   TH08 每格子机的阵型烧在各机体脚本/表里，本轮**没有一手数据**。
3. 🟡 **咲夜那个环是静态的**（不随移动 lean 角转）：要转就得每帧三角函数 ⇒ 引 libm ⇒ 撞「`.c` 零 x87」硬门。
4. **12 格公共池刚好用满**：108 满配 4 + 116 最多 6 + 77 一颗 + 71 一颗 = 12；三张卡同时满配时后申请者失败并记日志。
5. `BP_ce_item_frame` **每帧恰好一次**的假设（与既有 `ce_cancel_latch_reset` 同款）；若一帧两次会出现一次多余的
   「退位释放 + 重申请」（子机闪一帧）。
6. **妖侧子机贴图仍是占位**（借豌豆射手脚本 102 —— `anm_ids.h` 里还没有 youkai 的条目）。
7. **整条链未实跑**（每帧声明 → 帧界落笔 → 引擎跟随 `0x45bc90` 摆位）：实跑可用
   「**咲夜 = 4 格不重叠**」与「**松开 Shift 后子机消失**」这两条直接验。

### BO-9 108 低速子机的零售切图（换掉豌豆射手占位）+ 一条「层由引擎定死」的一手

**一手结论：子机外观既不是共享图集的一格、也不是机体本体图 —— 四支机体各有一支子机脚本。**

| TH18 机体 | 零售子机脚本 | sprite（矩形从 anm 的 sprites 表读）|
| --- | --- | --- |
| 霊夢 → TH08 `pl00` 妖侧 八雲紫 | `player00.anm` **script 18**（`th08:0x44e3d7 push 0x12`）| 22–29 = 30×46 + **30 = 46×46** |
| 魔理沙 → `pl01` アリス | `player01.anm` **script 29**（`th08:0x44ea6f push 0x1d`）| 22–25 = 30×30 |
| 咲夜 → `pl02` レミリア | `player02.anm` **script 24**（`th08:0x44eba7 push 0x18`）| 20 = 30×30 |
| 早苗 → TH16 `pl03` 射命丸文 | `pl03.anm` **script 11**（`th16:0x492c00[CHARACTER]` = `{8,7,11,11}`）| 32–39 = 16×16（8 帧拍翅）|

- **自机子机 = `zPlayerOption`**（`player+0x40c` ×4、stride `0x2f4`、**每格头 `0x2a4` 就是一个 AnmVm**），
  三条装载点都在**进低速那一帧**（`th08:0x44b0e5`），经 `AnmLoaded::load_script_into_vm` `th08:0x4069f0`
  装进 `option+0x0`。**三支脚本与本体 script5..9 不共用任何 sprite**（本体 = 11..21）⇒ 两条 all-clear：
  不是「共享图集一格」（三队矩形/坐标/尺寸都不同）、不是本体图；也与**敌人使い魔**（ECL opcode 90/91/92）无关。
- ⚠ **TH16 的 `CHARACTER` 是「菜单序」不是文件号** `{0 霊夢, 1 チルノ, 2 文, 3 魔理沙}`
  （一手：`Bomb__operator_new` 的 switch + `.rdata` 的 anm 名表 `pl00/pl02/pl03/pl01.anm`）
  ⇒ **文 = `CHARACTER` 2 → `pl03.anm`**，与 spec §11.1 的文件号判定一致（两套编号别混）。
- ★ **层号不用我们管（一手）**：TH16 那支脚本**自带 `layer(13)`**（照抄）；TH08 那三支**零售一行 layer 都没有**，
  而 TH18 引擎在 `Player__allocate_option` 里 **`th18:0x40a8bc mov DWORD PTR [esi+0x18],0xe` 无条件把子机 VM 的层设成 14**
  （`zAnmVmPrefix+0x18` = layer）⇒ 不写 layer 即用 **14（世界层）**，**C 侧不需要补**。
- ⚠ **唯一抄不了的指令**：TH08 那三支各有一行 **`ins_83(2)`** —— TH08 v3 的**无名 opcode 83**
  （handler `th08:0x460314`，`vm+0x200 = 指令 dword`），`thanm -c 18` 实测报
  `opcode 83 is not known to exist in version 8` ⇒ **TH18 没有这个 opcode**。
  全 exe 只有两个读者（`th08:0x45e96c` 的 `draw_vm__player_bullet__state_2` 跳表、`th08:0x430efa` 敌人链），
  **都不在子机 VM 的绘制路径** ⇒ 省略对画面无影响（🟡 读码结论，未实跑）。
  它与 v8 `type(N)` **是否同义未证实**（v8 的 `type` handler `th18:0x47b12c` 是打包进 `vm+0x534` 的 5 位字段，
  机制不同）⇒ **没敢改写成 `type(2)`**。

**落地**：`make_youkai_option_art.py` + `assets/ability/youkai_option/` **22 张 PNG**（2 的幂、非空、
与 ORDER 声明逐张尺寸相等、留白左右对称；连跑 3 次产物 md5 恒定 `32ec39e1…`）+ 脚本 **224–227**
（`224/225/226/227_youkai_opt_{yukari,alice,remilia,aya}`）+ ORDER 片段 22 行（已并入主表）。
C 侧：占位 `CE_ANM_ABILITY_SCRIPT_PEASHOOTER_OPT`（卡 77 的豌豆射手子机）换成
`YK_OPT_SCRIPT_{YUKARI,ALICE,REMILIA,AYA}` + `yk_option_script(character)`；
写法用 `#ifdef CE_ANM_ABILITY_SCRIPT_YOUKAI_OPT_*` → 宏、`#else` → 字面量 ⇒ **`make anm` 后自动切宏、不用手改**
（本轮已重跑，`anm_ids.h` 现在有那四条）。数量表/阵型/落笔一行未动；`youkai_core.*` 没碰；
`on_delete` 路径不变（子机 VM 仍由 `ce_option_drop` 收）。

**❓ 两条要用户拍板 / 待实跑**：

1. ★ **中断标签口径不一致（照抄零售的必然后果）**：TH18 零售本体子机脚本（`pl00.anm` s15/16）与
   TH16 `pl03` s11 用的是 TH18 约定（**2 出场 / 3 进店收起 / 1 销毁**），而 TH08 那三支是 **TH08 约定**
   （s18：1/2 循环、3/4 起爆、5 散场；s29/s24 只有 1/2/5）。
   一手：TH18 清子机槽时发的是**标签 1**（`th18:0x45d310` 12 格卡池循环、`th18:0x45d2d0` 原生 4 格、
   `th18:0x45dbf0` repopulate 尾；零售装备卡同样，如 `CardTenshi::__on_tick_2` `0x40e9b8`）
   ⇒ TH08 系脚本收到 1 只是「切到另一段循环」（无 delete，随后 SDK 删 VM）＝无害；
   **但若引擎还会发 3**（本轮**没找到** 2/3 的一手发送点），s18 会切到 46×46 旋转的「隙间」形态并 `stop()`
   ⇒ 收起动画会丢。要不要把标签改写成 TH18 口径＝设计决定，本轮**没擅自改**。
2. **TH16 满火力另一形态没接**：`th16:0x492c10 = {9,8,12,13}`（文的那支 = `pl03.anm` **script 12**，
   8 帧同图但 `blendMode(1)` + `type(3)` + 不同帧序）。要接得**按火力档切脚本**（会触发「先收再要」的重申请）。
   一手数据已在上面，可随时补一支 228。
3. 子机外观的**观感未实跑**（三队子机 sprite 都是半透明：meanA pl00 120 / pl01 96 / pl02 81，本体 200+）。
