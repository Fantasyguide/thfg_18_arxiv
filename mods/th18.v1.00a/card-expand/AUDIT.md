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
| L5 | 零售 57 张 = `0x63c0`，第 57 张的位置就是尾部字段 | **CONFIRMED** —— `0x4165ae mov eax,[edi+0x63c0]`、`0x41676b lea esi,[edi+0x63c4]`；57 × 0x1c0 = 0x63c0。阈值取 57：id 57（BACK）也重定向，零售从不为它渲染文案，无观察差异 |
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

**M6 `__card_ids` 访问点**：AbilityMenu 全部函数（`0x413470..0x4160b0`）objdump 过滤位移 ∈ [0x304,0x3e4) 与负位移：
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
| O4 | 桩不用的槽透传基类实现是安全的 | **CONFIRMED** —— +0x08/+0x38/+0x3c/+0x40 直接填基类地址；门里 `ce_sdk_setup` 把 `0x4b4c78` 的 0/2/14/15/16/20 槽与 `engine.h` 常量比对，不符 FAIL（布局守卫）|
| O5 | +0x50 桩的尾跳 | **CONFIRMED** —— `ce_state_free(this)` 后 `jmp 0x411410`，ecx 重新装 this，栈上的 flag 参数原样留给基类的 `ret 4`（objdump：`mov ecx,esi; pop esi; jmp eax`）|
| O6 | 私有状态不会泄漏 | **CONFIRMED** —— 所有销毁路径都走 +0x50 槽（`reset_cards` / `recount` / 即时卡当场删 `0x412d1d`），SDK 在那释放；256 槽 = 一局卡数上限（`0x411469`）|
| O7 | 移速倍率在 `on_tick_2` 里写才生效 | **CONFIRMED** —— 见下「O7 证据」 |
| O8 | `player+0x47774` 是复活无敌计时器，`{0x117,0x118}` 只在刚置好那一帧出现 | **CONFIRMED（推断链）** —— 见下「O8 证据」，实跑看 A♠ |
| O9 | `bullet+0x9c` 在 +0x28 槽被调时已是最终基础伤害 | **CONFIRMED** —— `PlayerBullet__create` `0x45e396` 写、`0x45e7f5` 调槽、`0x45e837` 取用；`CardMomoyo__on_bullet_init` `0x411083` 同一字段覆写 |
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

**O3 证据**：读 `0x4b4c78` 的 21 个指针逐个反汇编，带栈参的只有 +0x0c(`ret 4`)、+0x1c(`ret 8`)、+0x28(`ret 4`)、
+0x30(`ret 8`)、+0x44(`ret 4`)，其余裸 `ret`。桩用 `__attribute__((thiscall))`，objdump 确认 this 在 ecx、
一参桩 `ret 4`、二参 `ret 8`（checklist 的 ABI 项）。

**O7 证据**：`player+0x477ec` 由移动函数 `0x45b5b6` 读、Player tick 末尾 `0x45c702` 复位 1.0。UpdateFunc 优先级
AbilityManager 0x16 < Player 0x17（注册点 `0x40847f` / `0x45a8b4`，同一注册函数 `0x401180`）→ 管理器 tick 里的写
在同一帧被移动读到；`on_tick`（+0x24，`0x45c0e5`）在复位之前，白写。BombMarisa（0x19）写 0.5 是同一字段。

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

**O20 证据**：C 键分派 `0x45c084`：`INPUT_PRESSED & 0x400` → `mgr+0x38` 的槽 +0x08。Tenshi `c_press` `0x40ebf9`：门 `+0x54 == 0 && +0x38 <= 0`；
装填 `+0x38 = (int)(+0x48 × mgr+0xc58)`、`+0x3c = 同 float`、`+0x34 = int − 1`；经过帧 `+0x20` 组清零；`flags |= 0x20`。
`__on_tick_2` `0x40e8c0`：state 0 清 0x20、门控（`GUI_PTR->msg(+0x1b0) == 0 && ENEMY_MANAGER->enemy_count_real(+0x198) != 0`，ExpHP 名）下 `+0x38 > 0` 才 `Timer__decrement(+0x34)`；
state 2 清 0x20、`+0x24 > 8` 回 0；末尾门控下 `Timer__increment(+0x20)`。`__on_load__2` 只清状态与经过帧，`method_4C` 再清充能。
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
`CardTenshi__c_press` `0x40ebf0` 内联的就是这套（层 13、脚本 0x1c、坐标改玩家位），证明从 c_press 时点调它安全。
C 侧用 `__attribute__((thiscall))` 函数指针，4 个栈参；`reverse.o` objdump：`mov ecx,[0x4cf298+0x10]`、`push out_vm; push 0x10; push 0x44; push out_id; call`，
call 后直接 `mov eax,[esp]` 读 id、**无 `add esp`**——与被调方清栈一致。`ce_anm_spawn` 对 `anm == NULL` 早退返回 0。
备查未用：`AnmLoaded__set_sprite` `0x477b00` thiscall ret 8（两出口）、`0x488cf0` stdcall(id) ret 4。
ANM 侧：`ability.anm` 追加 entry7（`REVERSE.png`，字段抄 abcard `BLANK_max`）/ `sprite109`、`script68`（`assets/ability/scripts/68_reverse_flash.anm.txt`），
`build_ability.py` 校验零售 7 entry / 68 脚本 / 贴图逐项不变，`thanm -l` 往返一致。脚本自 `delete()`，卡对象不记 id。
~~🟡 世界层投影是否透视未验~~ → 实跑显示不对；根因：模式 8 走 WORLD 矩阵不加相机 2 的区域原点偏移（`AnmManager+0xd0/+0xd8`），放层 16 会整体偏位（`engine/anm/th18/01-vm-instantiate.md` §3b）。改为零售配方 `layer(20); resolutionMode(1); type(8);`，`ce_anm_spawn` 的层参数同步 20（脚本内 `layer()` 会覆盖，纯注释意义）。复跑仍全空白 → 见 O24′。

**O24′（根因，2026-09-04 二轮）**：`engine.h` 把 `CE_MGR_ABILITY_ANM` 写成 `0x10`——那是 **abcard_anm**。ExpHP `zAbilityManager`：`+0x0c ability_anm / +0x10 abcard_anm / +0x14 abmenu_anm`；`CardTenshi__c_press` `0x40ebf0` 取 `[ABILITY_MANAGER_PTR+0xc]` 起 script 0x1c 与之一致。错拿 abcard.anm（18 个脚本）后 `AnmManager__sub_407420` **无边界检查**地把脚本表第 68 项（表外内存）拷成 VM → 垃圾 VM，什么都画不出，两版（层 16 / 层 20）全空白。来源是 `10-extensibility-limits.md` §5 旧文一句写反的偏移，没有对着结构体 / 反汇编重取——**又一次 K′/M6/O1 教训：偏移必须从上下文重取，不能抄文档**。修正为 0x0c，2D 零售配方脚本先跑基线（实跑 ✅ 显示在上边框中点 = ECL 原点），再切回 type(8) 层 20（实跑 ✅ 有透视；锚点顶边对齐，pos y 上移 96 补偿）。
附带订正：O24 里「层 16 会偏位」的推断经 `02-render-stages.md` 一手对表后不成立（层 12–19 在相机 3 下，偏移为 0）。

**O25（2026-09-04）方片 2 `d2.c`**：只读数据，无新引擎调用——价格表 `CARD_PRICE_BY_TIER` `0x4b35c4`（int[15]，SM 一手 dump）按表行 `+0x10` 的 tier 取价，越界返回 0；
写 `MONEY` `0x4ccd34` / `MONEY_TOTAL` `0x4ccd30`（SM §1）。时序依据：成交 `0x4185c7` 先 `allocate_new_card(mode 2)`（ctor）后扣款，故 ctor 写 `2·M − price`。
M < price 的火力补差价路径不动 MONEY（游戏随后清零）。**未实跑。**

**O26（2026-09-04）皇家同花顺 `cards/royal.c`**（黑桃五张共用 ctor）。触发不开新断点：成交 `allocate_new_card(mode 2)` 调 ctor（`0x412d11`），`owned[id]=1` 在其后（`0x412d42`），ctor 里查其余四张 `owned[]`（`mgr+0xd70`，本 mod 搬迁后的数组）齐了即第五张，天然只触发一次；纯函数 `ce_royal_flush_ready` 有单测。
两个新引擎调用：**`0x4575f0` 加命**——`mov esi,ecx`（this = GlobalsInner `0x4cccdc`），不读栈参，唯一出口裸 `ret`；`[esi+0x6c]` CURRENT_LIVES 钳 `[esi+0x78]` LIVES_MAX，内部再调 `0x405bf0` 起特效、`0x476be0` 放音效 0x11。**`0x457690` 加 bomb**——this = ecx，`[ecx+0x7c]` CURRENT_BOMBS 钳 `[ecx+0x88]` MAX_BOMBS，音效 0x2e，**唯一出口 `ret 4`** 但函数体不读任何栈参（O23 同款），C 侧签名带一个 dummy int。`royal.o` objdump：加命 `mov ecx,0x4cccdc; call`（无 push）；加 bomb `mov ecx; push 0; call`，call 后无 `add esp` —— 与两函数的出口一致。上限照 `CardLife__destructor` `0x409b80` / `CardBomb__destructor` `0x409c20`：先 `+1` 钳 7 再调加法。门：`GAME_THREAD_PTR` `0x4cf2e4` 非 0（零售即时卡 dtor 的门）。ctor 返回 0 保留卡。
实跑（2026-09-04）：购买第五张触发 ✅，但**之后每关开始又触发一次**（共 6 次）——引擎在每关开始对卡组里每张卡再调一次 +0x00（trace：初始携带的 64 `on_stage_start` 后紧跟 `ctor`；02-lifecycle 只记了 mode 0/2 调 ctor，漏了这条）。修：ctor 开头 `ce_fresh_acquire(c)`（= `owned[自己] == 0`；获得路径里 owned 在 ctor 之后 `0x412d42` 才置 1，关卡开始那次早已是 1）。复跑 ✅ 只触发一次。
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

**O28a 证据**：C 键在 Player tick（0x17）、判定在 EnemyManager tick（0x1b，Enemy tick 体 `0x42ff80` 里比较先于 `Timer__increment`）：**同一帧**收场；ECL VM `0x430d30` 只在宣言 case `0x434fc2` 碰 `+0x2bc`，`wait` 用别的计时器，改它不会提前唤醒脚本； `0x42ED40` 每帧取第一个 `hp_value > -1 && time > 0` 的槽，`slot.time <= cur` 即走超时段（血量钉到阈值、`sub_timeout`、Spellcard 置 0x80 / 清奖励）；比较是 `<=`，写成等值下一帧必触发；`prev` 一并写，`cur_f` 同步（zTimer 三件套，O23 教训）

**O28b 证据**：超时段对 `flags & 8` 的槽走 `(flags & 9) == 9` = 收下；我们在写计时的同一帧清 bit1、`bonus = 0`、`can_still_capture_spell = 0`，结束 `0x42A780` 看 bit1 走失败演出，ECL 523 的 Sannyo 碎片也看 bit1。**限制**：耐久符卡的 `sub_timeout` 脚本本身是「守住」分支，脚本里写的掉落照旧；`0x42ed40` 耐久路径累加的 `0x4ccd6c` 无人读

**O28c 证据**：一手反汇编：`mov edx,ecx`（this = gui），读 `[ebp+8]` 命数、`[ebp+0xc]` 碎片、`[ebp+0x10]` 上限，`ret 0xc`；零售在 `0x417a10..0x417a28`（`AbilityShop__sub_417880`）与 `0x45d1aa..0x45d1bd`（`Player__commit_death_and_enter_state2`）都以 `push [0x4ccd54]; push [0x4ccd4c]; push lives` 调它

**O28h**（订正）：第一版只看了 `0x4297a0` 的序言与 Ghidra 的 `void(void)` 签名，断言「无参裸 ret」——**错**。整函数反汇编到尾：`0x429a0e ret 4`；零售调用现场 `0x434d48 mov ecx,[0x4cf2bc]; push 0; call`、`0x434e01` 同款、MSG 构造 `0x43e2e2 push ecx; call`——thiscall + 一个从不读取的栈参。按无参调多弹 4 字节，`on_activate` 的 `ret` 把栈上的卡对象指针（`0x17d2e090`）当返回地址跳飞：崩溃报告 `[ESP+8]` = 返回 `ce_sdk_c_press` 的地址、PC = 卡对象地址，与 4 字节偏差完全吻合。修正：`thiscall(BULLET_MANAGER; 0)`。**与 O23 同一个坑：调引擎函数前必须看全部 `ret N` 出口，反编译签名不算数。**`LaserManager__cancel_all` `0x449090`（这条第一版就对）：`mov ecx,[0x4cf3f4]` 自取 this，读 `[ebp+8]` 当 mode 传给每条激光的 `vtable+0x28(mode, 0)`，`ret 8`（两个栈参，`0x4490c4`）；ECL 两处 `push 0; push 1/0; call`，玩家死亡 `0x45c3c9 push ecx; push 1; call`（第二参从不读）。我们照 ECL 写 `(1, 0)`，声明为 stdcall 两参。调用时机在写完中断槽计时之后、同一 `on_activate` 内。

**O28g 证据**：非耐久符卡：超时段置 bit7（`0x42a320` 每张新符卡清），卡里 `flags & 0x80` → 拒绝。耐久符卡自然超时不置 bit7、`enemy+0x635c` 的超时位也被清，只靠「活动槽已清（hp_value = -1）→ 找不到槽 → 拒绝」兜底；ECL 预装多个带超时的槽时会误伤下一段。实跑清单：符卡刚超时那几帧连按 C 看是否 refuse


**O29（2026-09-04）青眼白龙 `cards/blue_eyes.c`**（主动卡：献祭 1 残机召唤跟随龙，2500 挡弹，每 300 帧一道矩形伤害光束）。设计 `docs/superpowers/specs/2026-09-04-blue-eyes-design.md`。

**O29a** `BulletManager__cancel_radius_as_bomb` `0x429370` = stdcall 四栈参 + XMM2 半径，ret 0x10；计数器 `mgr+0x7a41e8`。**证据**：反汇编四个出口都 `ret 0x10`；序言 `movaps xmm0,xmm2` 存 `[ebp-4]` 当半径；Tenshi `0x40eb13` `movss xmm2,[0x4b9290]`(18.0) 后 `push 0; push 0x1869f; push 0; push ebx; call`；`0x40eb46..0x40eb6a` 调前 `mov [ecx+0x7a41e8],0` 调后读。🟡 静态一致，待实跑。

**O29b** `FUN_0045dfa0` = stdcall(center*, angle, life, dmg) + XMM2 宽 / XMM3 高，ret 0x10，返回 1-based 槽号。**证据**：反汇编 `movss [ebp-8],xmm3; movss [ebp-4],xmm2`、两个出口 `ret 0x10`、`+0x14/+0x18` 收 XMM、`[ebp+0xc]` 角度归一到 (−π, π]；Remilia `0x40f478 movaps xmm3,xmm2`(高) `0x40f480 movss xmm2,[0x4b92b4]`(宽 32) `push 0xc8; push 2; push 0(角度); push eax(center); call`。🟡

**O29c** `AnmManager__get_vm_with_id` `0x488b40` thiscall(mgr; id) ret 4；`interrupt_tree` `0x488be0` stdcall(id, n) ret 8；`0x488cf0` stdcall(id) ret 4。**证据**：`0x488b47 mov esi,ecx`、三个出口 `ret 4`；`0x488be3 mov ecx,[0x51f65c]`（自取 this）、`0x488c4e ret 8`；三张零售卡都 `push [esi+0x1c]; mov ecx,[0x51f65c]; call 0x488b40` 后写 `[eax+0x5f0]`。🟡

**O29d** 献祭：`CURRENT_LIVES < 1` 拒绝；−1 后 `ce_gui_update_lives`——同神之宣告 O28c 的路径。✅ 复用。

**O29e** ~~不写 `player+0x47984`~~ → **O29e′（2026-09-06）**：光束帧 `ce_raise_damage_cap(300)`，与炎魔 V20 同一条证据链（自机弹共用配额、Remilia 先例、GameThread 每帧复位）。用户改定。✅

**O29f** 私有状态与 SDK 主动卡状态机同槽 → 块头预留 16 字节（`ce_state_user`），主机单测 `test_sdk_core` 的块头用例。✅ 单测。

**O29g** `bullet+0x24 != 0` 的弹穿过龙（`0x429370` 的命中条件；零售炸弹同款）。✅ 设计。

**O29i**（教训，静态发现）内联汇编里**先 push 再引用 `"m"` 操作数会错位**：第一版 `ce_damage_rect` 的 `pushl %[ang]` 用了 `"m"(angle)`，编译成 `pushl 0x8(%esp)`——前面已经压了两个值，esp 相对偏移指向别的槽，压进去的是垃圾。objdump 核对调用现场时抓到；改成先 `memcpy` 成整数位、`"r"` 约束再压。**规则：内联汇编里压栈的参数一律走寄存器；`"m"` 只用在任何 push 之前的 `movss`。** `ce_cancel_radius` 与 `ce_play_sound` 本来就符合。

**O29j**（🟡 视觉，实跑看）光束 = 父 VM（C 每帧钉在龙头）+ `scriptNew` 子 VM：子的原点是父位置（thpages `ignoreParent` 说明反推子默认继承父的位置 / 旋转 / 缩放）。为不押注旋转继承，父脚本不转、每个子脚本自己 `rotate(0,0,−π/2)`、彩光沿 −y 飞（不用 `attached`/ins 314）：继承与否结果都朝上。贴图是零售 `pl01b`/`pl01b2`（ORDER 里 `local/` 源，构建时取，仓库不含字节）。

**O29k** 血条 = 两个根 VM（script86 底槽 / 87 填充，`sprite(-1); drawRect(1,1)`），C 每帧写 `vm+0x54/+0x58`（scale x/y）与位置、颜色。**证据**：ExpHP `zAnmVmPrefix` scale = +0x54 zFloat2（同结构 color_1 = +0x524 已被 Tenshi 一手印证）；零售 HUD 充能条 `draw_active_card_hud_entry` `0x408a53`/`0x408a70` 把 fill 比例写进 `vm+0x58`（同时 `+0x70` uv_scale、`+0x80` uv 滚动、`flags |= 0x18`）——「代码写 scale」是零售做法，thpages `drawRect` 也这么建议。脚本里没有 scaleTime 在跑，写入不被插值器覆盖。颜色 D3DCOLOR `0xAARRGGBB`（Tenshi 的 `0xff0080ff` = 蓝 (0,128,255)，第一版文档写成橙是读反了字节序）。🟡 drawRect 是否吃 scale / 层 13 上的表现实跑看。

**O29h** 龙 VM 被引擎删掉（关卡切换 / 消弹演出）时卡不悬空：`on_active_tick` 每帧先 `ce_anm_get_vm`，为 0 就结束持续态；`on_stage_start`/`on_run_reset` 主动删。🟡 实跑。

## P. 商店走两遍（每关进店 2 次）—— 两个放行断点

引擎链一手见 [`engine/card/th18/05-shop-and-money.md`](../../../engine/card/th18/05-shop-and-money.md) §3.5。
**没有手写机器码、不改控制流**：两个断点都返回 1 放行原指令；唯一的「改动」是给 `eax` 加一位，
让零售自己的建店代码（`0x443b10..0x443c17`）再跑一遍。状态机在 `shop_core.c`（主机单测 `test_shop_core.c`）。

| # | claim | 结论 |
| --- | --- | --- |
| P1 | `0x4183ea` 盖住的 12 字节是四条完整指令且无相对寻址 | **CONFIRMED** —— `6a 06` / `6a 00` / `6a 06` / `8d 8f 28 02 00 00`（push 6; push 0; push 6; lea ecx,[edi+0x228]）；`sites.py` 生成时与 exe 逐字节对账，门里再核 `e8` + 7 个 `90` |
| P2 | `0x4183ea` 只在「玩家选是、真的成交」时经过 | **CONFIRMED** —— 它在 `case 6/7` 的 `param_1[3]==0` 分支里，前一条是 `0x4183e5 call FUN_00416c80(this,5)`；空白卡路线（state 3 → `0x418584` 置 4）、取消（`0x418551` 置 2）、买不起（state 8）都不经过 |
| P3 | `0x443b05` 处 `eax` = `GameThread+0xb0`、`esi` = this；改 `eax` 后原 `test` 算出的 flags 正是零售语义 | **CONFIRMED** —— 见下「P3 证据」 |
| P4 | 重开与关店在同一帧，敌人 / MSG 看不到指针为 0 | **CONFIRMED** —— 商店 on_tick 0xc（`0x4171de`）在自己 tick 里 `0x417857` 清指针；GameThread 0x10（`0x442a97`）本帧随后建店 `0x443bed`；EnemyManager 0x1b（`0x42dcd8`）、Gui 0x21（`0x43bac5`）在后 |
| P4′ | GameThread 在 `0x443b05` 之前的提前返回不会造成空档 | **PARTIAL** —— 见下「P4′」：两条提前返回若恰在关店那一帧命中，晚一帧重开（敌人 / MSG 跑一帧）；日志标 `late` 实跑核对 |
| P5 | 第二家店与第一家共用零售代码，无新对象布局假设 | **CONFIRMED** —— 建店 / `AbilityShop__initialize` / on_tick / 析构全是零售路径；offer 重抽自动排除已拥有（`mgr+0xd70` owned，`pick` 与两个保证循环同一门控，SM §4） |
| P6 | 练习 / replay 回放 / 空白卡 / 中途退出都不会多开店 | **CONFIRMED** —— 见下「P6 证据」 |
| P7 | replay 一致性 | **CONFIRMED** —— 见下「P7 证据」 |
| P8 | 暂停菜单期间不会出错 | **CONFIRMED** —— 见下「P8 证据」 |

**P7 证据**：商店抽卡 `0x41717b mov ecx,0x4cf280`（REPLAY_UNSAFE_RNG：AnmVm / ItemManager / StageInner 用；同步 RNG 在 `0x4cf288`，Bullet / ECL / ReplayManager 用），第二家店多抽一次不碰同步 RNG；此外 录制 / 回放在商店期间都停（`0x4629a2` / `0x462aa5` 看指针）；`sub_417880` 每次关店都覆写本关卡组快照与 GlobalsInner（含金钱），回放时第一家店 30 帧内复原最终快照后退出、不成交、不重开 → 与录制时两家店买完的结果一致。实跑核对


**P3 证据**：`0x443aff mov eax,[esi+0xb0]`；`0x443874 mov esi,ecx`（fastcall this）；esi 在 `0x443a8e mov esi,1` 被借用后于 `0x443a96` 从 `[esp+8]`（`0x443877` 存的 this）重载，`0x443a75` 的跳过支路不碰它——两条路到 `0x443b05` 都是 this；`test eax,0x20000` 由 cave 里执行，
`0x443b0a jz` 看它的 ZF；创建后 `0x443c17 and [esi+0xb0],~0x20000`、`0x443c21` 重读 —— 内存位我们不动，不会残留。

**P4′**：`0x44387b`（`[0x4cd624] & 0x180` → return 0，线程注销）与 `0x4439ed`（`this+0xb0 & 4` → 置 0x80 后 return 1）都在 `0x443b05` 之前。
前者是关闭 / 切模式，商店时段不会命中；后者的位 4 语义未钉死。真命中那一帧本断点不跑，下一帧才重开，中间敌人 0x1b / Gui 0x21 各跑一帧。
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
广播后 `memset(&enemy+0x04, 0, 0x50)` = 0x50 字节 = **20 个 int32**（type 1..0x13）。
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
——所以杂兵 = 主掉落一颗，之前的实现对它一个字节都没改。附加循环只撒 type 1..0x13（`0x12 < i` 退出），第 20 格永远不撒（`double_core.h` `DBL_DROP_MAX 19`）。
修法不引入新调用：断点时主掉落已飞出但字段未清，读 `ce_drop_main(counts)` 得类型，`ex_drop[type-1] += 1`，引擎的附加循环撒第二颗（散射位置按 `dropArea`，方向同样朝上）。
纯逻辑 `double_core.c` 主机单测 46 例。翻倍前后合计每种 = 2 ×（主 + 附加）。

**实跑要看的**：`_test` 起手卡组已带 69。打杂兵 → 掉落**明显翻倍**（火力、点数、金钱都是）；
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
| U4 | 子机的贴图来自 `ability.anm` | **CONFIRMED** —— `allocate_option` 的最后一参是脚本号，取 `AbilityManager->ability_anm`（Reimu 2、Alice 0x10）。我们给 script88 |
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
| V17 | 震屏工厂 `0x476060` 的调用约定 | **CONFIRMED** —— 见下「V17 证据」：fastcall(ecx = type, edx = time; start, end, 0, 0x5b) `ret 0x10`；type 1 的 on_tick 用 UI RNG，不进 replay 流 |
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
tick 顺序 GameThread 0x10 → AbilityManager 0x16（我们的 `on_active_tick` 在这里）→ Enemy 0x1b ⇒ 写入只影响当帧的敌人结算，下一帧自动复位，不用还原、不留残值。
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
type 1 → 只注册 on_tick `0x475c70`（`register__on_tick(…, 0x14)`），不注册 on_draw（所以 0x5b 用不到）；写 `+0x18 = time`、`+0x1c = start`、`+0x20 = end`。
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
`[ebp+8]` = out_id（`mov [edi], 0` 后 `mov [edi], ecx` 写回 id）；`[ebp+0x14]` 全程不读；唯一出口 `0x409746` **`ret 0x10`**。⇒ thiscall(anm; int* out_id, int script, float* pos, int unused) ret 0x10。
`sdk.h` 的 `ce_anm_spawn_ui` 压四个参。层不是参数（世界版 `0x405bf0` 写 `vm+0x18`，这里没有）—— 靠脚本里的 `layer(26)`。

**X3 证据**：`0x457570`：`mov esi, ecx`（this = GlobalsInner `0x4cccdc`）；`CURRENT_LIVES(+0x6c) >= LIVES_MAX(+0x78)` → `+0x70 = 0; ret 4`；否则 `+0x70`（LIFE_FRAGMENTS）+1，
按难度表（`0x4b40c8` / extra `0x4b3ff4`，下标 `+0x74`）取每命碎片数，`while (frag >= n) { frag -= n; 0x4575f0(加整命) }`，最后 `0x441f10(GUI; lives, frag, max)` 刷 HUD；出口 `0x4575e2` **`ret 4`**。
调用方 `CardLifeFragment__constructor` `0x409cc0`（先 `LIVES_MAX` +1 钳 7 再调它——那是碎片**卡**的额外效果，我们不要）、`CardNarumi` 每关、道具 type 0x10。⇒ thiscall(GlobalsInner; 哑参) ret 4，`ce_add_life_fragment` 与 `ce_add_bomb` 同形。

**实跑要看的**：见 [`NEXT.md`](NEXT.md) §1k。崩溃优先怀疑 X1 / X3 的哑参、X2 的 thiscall；视觉看 X5。

## Y. 古锭刀（id 76）—— 终符期间自机弹伤害 ×2

**没有手写机器码、没有新断点、不调任何新引擎函数、不改任何字节**。读的都是已在用的结构（神之宣告的 boss / 中断槽链、K♠ 的 `bullet+0x9c`、
炎魔的 `ce_raise_damage_cap`），新读一个 `sht+0x28`。纯逻辑 `guding_core.c` 主机单测（26 例：活动槽选择、`Dead` 匹配含 0x40 满长无终止符、状态沿、倍率）。

| # | claim | 结论 |
| --- | --- | --- |
| Y1 | 「终符」= 活动中断槽子程序名含 `Dead` | **CONFIRMED（ECL 一手，全覆盖）** —— 见下「Y1 证据」：七关 boss + Extra 道中最后一张符卡的槽名全是 `BossDead` / `MBossDead`，其余 `Boss2`…`Boss10`；主线道中无符卡 |
| Y2 | 活动槽 = 第一个 `hp_value > -1 && time > 0` | **CONFIRMED** —— 与引擎超时判定同一条规则（`0x42edf5`，§O28）；神之宣告 `expire_current_attack` 已实跑用它。Extra 道中槽 1 常驻 `MBossDead`（整体计时）但槽 0 在前，Card1 / Card2 期间取到的是 `MBoss2` / `MBoss3` ⇒ 不误判（单测覆盖） |
| Y3 | 4 参 `setInterrupt` 把名字写进 `sub_life` 与 `sub_timeout` 哪一个 | **推理（领域）** —— 击破走 `sub_life`（`+8`）、超时走 `sub_timeout`（`+0x48`）（§2 ③ / ①），而终符无论击破还是超时都进 `BossDead` ⇒ 两个都是它。代码两个都查、任一含 `Dead` 即算（`gd_slot_is_final`），不押注哪个 |
| Y4 | 为什么不用 `stars`（ECL 540） | **CONFIRMED 反例** —— st07mbs `MBoss2` / `MBoss3` 都写 `stars(0)`，Card2 会被当终符；`MBoss1` 根本不写。主线 boss 倒是严格（`stars(N)` = 本符之后还剩几张） |
| Y5 | 翻倍两处一起 | **CONFIRMED（V20 同一事实）** —— 自机弹与卡的源共用每敌每帧上限，贴脸时已吃满，只翻弹不抬等于白翻。抬到 **sht 基值** ×2 而非当前值 ×2，见下「Y5 证据」 |
| Y6 | 时序 | **CONFIRMED** —— GameThread（0x10）复位上限 → AbilityManager tick（0x16）`on_tick_2` 判定 + 抬 → Player tick（0x17）建弹调 `+0x28` 读同帧判定 → Enemy（0x1b）结算。`on_bullet_created` 只读私有状态，不再走链表 |
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

## Z. 豌豆射手（id 77）—— 第一张真走 SHT shooterset 的卡（子机连射 + 机枪爆发）

**没有手写机器码、没有新断点、不改任何 exe 字节**。两处内联汇编（`ce_allocate_option` / `ce_tick_shooters_for_card`）
早在 §U 就照零售调用点复刻好了，本卡是 `ce_tick_shooters_for_card` 的**第一个调用者**——所以这一节把那条链
从头到尾一手重走了一遍（2026-09-08，Ghidra：`Player__tick_shooters_for_ability_card` `0x40a9c0`、
`Player__shoot_one_bullet` `0x45e930`、`PlayerBullet__create` `0x45e320`、`Player__tick_shooting_state` `0x45ea00`）。
新写入面只有 `pl0X.sht` 偏移数组的两个空位 + 文件尾两段数据；`ability.anm` 加两个 entry / 四个脚本（本来就在重建）。
纯逻辑 `peashooter_core.c` 主机单测（等待区间、开大形状、周期、inflight、清零）。

| # | claim | 结论 |
| --- | --- | --- |
| Z1 | 追加 shooterset 不动零售任何字节 | **CONFIRMED（工具每次回读校验）** —— `append_shooterset.py` 的三条不变式：头部 `+0x02` 仍是 40、shooterset 0 的四个 func 仍全 0（空位重复解析靠它幂等）、前 23 组逐字节相同。索引 `0x17` / `0x18` 落在零售留空的 17 项里 |
| Z2 | `ce_tick_shooters_for_card` 的调用约定 | **CONFIRMED** —— `0x40a9c0` 反编译为 `(param_1 = option, param_2 = short, param_3 = long, param_4 = set)`，stdcall `ret 0x10`。`sdk.h` 的压栈序与之一致（§U 已复刻，本卡首次真调用）|
| Z3 | **short_timer 可以由我们自己数** | **CONFIRMED（一手，本节的关键）** —— 见下「Z3 证据」：`param_2` 只进一处模运算，往下传的那份在 `PlayerBullet__create` 里从未被引用 ⇒ 传什么值都只影响「这一帧发不发」 |
| Z4 | `+0x1c` 每帧只广播一次，且只在按住射击键时 | **CONFIRMED** —— 见下「Z4 证据」。⇒ 我们的计数器只在射击帧推进，「开大」不会在不开火时白烧；死亡 / 被读条状态（`player+0x476ac != 1`）也自动暂停 |
| Z5 | `opt_slot` 必须 0 | **CONFIRMED** —— `0x45e320`：`if ((shooter[0x20] & 0xf) == 0) 位置 = *param_3`（= `0x40a9c0` 传进来的子机坐标）`else` 读 `player` 自己的第 N 个子机位置块。非 0 就会打到零售子机的位置上 |
| Z6 | `fire_rate` 的取值范围 | **CONFIRMED** —— i8、符号位是 shooterset 终止符 ⇒ 1–127；`0` 在这条路径上是裸 `idiv`（`0x40a9c0` 没有零分支，主炮那条有）。我们用 60 / 5，工具也在 `pack_shooter` 里挡住越界 |
| Z7 | 弹的贴图取自 `ability.anm` | **CONFIRMED** —— 见下「Z3 证据」的第 2 行与末行：`0x40a9c0` 开火期间换 `player+0x10`、结束还原；`0x45e320` 建 VM 用的就是它 ⇒ shooter 的 `+0x22` 填 ability 脚本号，四个 `pl0X.anm` 不动 |
| Z8 | 音效字段 `< 0 = 不响` | **CONFIRMED** —— `0x45e320` 末尾 `if (-1 < *(short *)(shooter+0x24)) play_sound(...)`。平时那颗给 `0x20`（`se_gun00`，−15 dB / 优先级 10，1.42 s 一次）；开大那 7 × 20 颗给 **−1**（20 发/秒的音会炸耳）|
| Z9 | 出膛偏移 `spawn_dx/dy` 会被减掉一帧速度 | **CONFIRMED（设计上正确）** —— `0x45e320`：`mode` 不是 4/5/6 时 `pos += (spawn_dx − vx, spawn_dy − vy)`。我们 `mode = 0`、角度 −π/2 ⇒ `vx = 0, vy = −speed`，所以 7 颗横向铺开 ±48 原样生效，纵向统一「先退一格、第一帧补回来」——这正是零售减法的用意 |
| Z10 | 平时那颗 90 会被每敌每帧上限砍 | **已规避** —— 上限每帧从 `sht+0x28` 复位（Reimu 90 / Sakuya 60 / Sanae 120 / Marisa 160）：90 塞满 Reimu、超出 Sakuya，还要与自机弹抢配额。**豌豆在途的 40 帧**里 `ce_raise_damage_cap(180)`（Remilia `0x40f48e` 同款）；开大那排 49 不用抬 |
| Z11 | 抬上限的副作用 | **设计（有意，已收窄）** —— 上限是自机弹与所有卡共用的，抬高对同帧的自机弹也放宽。收窄见下「Z11 说明」 |
| Z12 | 时序 | **CONFIRMED** —— GameThread（0x10）复位上限 → AbilityManager（0x16）`on_tick_2` 抬 → Player（0x17）`on_tick_shooters` 出膛 → Enemy（0x1b）结算 ⇒ 同帧有效、不用还原。子机申请仍挂 `+0x18`（`0x45d5e0` 尾部广播）|
| Z13 | replay 安全 | **CONFIRMED** —— 只有两处抽随机：开局第一个射击帧、每次开完大。两处都由 `on_tick_shooters` 驱动 ⇒ 调用序列完全由输入决定，replay 里逐次重放。`ce_rand` 用的就是游戏自己的 `REPLAY_SAFE_RNG`。节奏其余部分是整数计数，无浮点 |
| Z14 | 子机槽认领 | **CONFIRMED（§U3 同款）** —— 12 个槽是公共池；用前查 `option+0x00 != 0 && option+0xd0 == card+0x08`。子机指针存 `ce_state()` 而不是 `card+0x54`（§U2）|
| Z15 | 自机弹池装得下 | **CONFIRMED** —— `0x45e930` 的槽扫描上界是 **0x1ff = 512** 发。开大 140 颗摊在 100 帧里、每颗 448/18 ≈ 25 帧飞出场 ⇒ 同屏峰值约 **39** 颗，加主炮也远不到 512。满了 `shoot_one_bullet` 直接 `return 0`，不崩 |
| Z16 | 与其他卡的交互 | **设计** —— `PlayerBullet__create` 末尾对每张卡广播 `on_bullet_created`（`vtable+0x28`），所以 K♠（×1.1）与古锭刀（终符 ×2）**也会作用在豌豆上**——同一条自机弹管线，一致而非例外。反过来豌豆不吃炸弹 / `ce_damage_rect` 那条路 |
| Z17 | 数值 | **设计（用户定，2026-09-08 定稿）** —— 平时 **60 帧一颗 × 90 = 90 DPS**（PVZ 原版是 1.425 s ≈ 85 帧，用户改成整 1 秒：DPS 与 85/130 几乎相同、节奏更密）；开大 20 排 × 7 × 7 = **980**。周期 20–30 s ⇒ 综合约 **123 DPS**，与破损核心同档。代价：必须按住射击键 |
| Z18 | `build_ability.py` 的排序改动 | **CONFIRMED 无副作用** —— 脚本文件原来按文件名字典序排，`100_…` 会排到 `68_…` 前面。改成按**编号数值**排；68–99 的相对顺序不变（两位数的字典序与数值序一致），`make anm` 重建后零售部分逐项自检仍通过 |

**Z3 证据**（`0x40a9c0` 反编译，逐行）：

```c
pcVar4 = *(char **)(*(int *)(PLAYER_PTR->field_47924 + 0x1c) + 0xe0 + ((param_4 << 8) >> 8) * 4);
*(void **)(PLAYER_PTR->field_0 + 0x10) = ABILITY_MANAGER_PTR->ability_anm;   /* ← Z7 */
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

**Z4 证据**（`0x45ea00` 反汇编）：

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

**Z11 说明**（为什么抬上限是安全的）：收窄成两条——① 只在豌豆可能命中的那 40 帧内抬（`pea_needs_cap`，
开火帧置 `PEA_INFLIGHT`、每帧递减；间隔 60 帧 > 40 ⇒ 两颗的窗口不重叠，单测有断言）；② 抬到固定 **180** 而不是「按当前值翻倍」，且 `ce_raise_damage_cap` 只抬不降
⇒ 与炎魔 / 青眼（都是 300）同帧时取更高者，不叠乘。要让这次放宽真的加强主炮，主炮得在同一帧对同一敌人打出
本机体上限那么多（Reimu 90 = 5400 DPS），零售四机体都做不到；实际它只在我们自己的豌豆落地那一帧起作用。

**实跑要看的**：见 [`NEXT.md`](NEXT.md) §1m。崩溃优先怀疑：`ce_tick_shooters_for_card` 的压栈序（Z2，第一次真调用）、
`ce_allocate_option`（Z14 / §U1，已随破损核心走过同一条路）。视觉怀疑：豌豆大小（script100 / 101 的 `scale`）、平时那颗的节奏（`PEA_INTERVAL`）、
子机高低（`PS_OPTION_OFFSET` 与 script102 的 `scale`）、开大那堵墙的宽度（`spawn_dx` 七项）。

## AA. 自机弹伤害倍率层（`ce_dmg_*` + `.bullet_damage_permille`）—— 2026-09-08 提炼

**没有新断点、没有新引擎调用、不改任何字节**：只是把「给自机弹加伤害」这个已经出现三次的动作
（K♠ 61、古锭刀 76、衔尾蛇 78）从各卡收进 SDK。纯逻辑 `ce_dmg_*` 在 `sdk_core.c`，主机单测。

| # | claim | 结论 |
| --- | --- | --- |
| AA1 | 该提炼 | **本仓约定** —— `SDK.md` 方案 3：「动作出现第二次就进 `sdk.h`」。这是第三次，且前两次的写法已经分叉（一张抬上限、一张没抬）|
| AA2 | 桩看到的是引擎刚写好的原始伤害 | **CONFIRMED（一手）** —— `PlayerBullet__create` `0x45e320` 先 `bullet+0x9c = shooter+0x02`、再遍历卡链表调 `+0x28`（同一份反编译见 §Z3 证据）|
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

**股市面板的落位（2026-09-08 改）**：从侧栏下部（压着卡组图标）挪到**右上角**，
底板 640 空间 `(528, 50)` 尺寸 212×68 —— **顶边落在 y = 16，与弹幕区上边框对齐**（相机 0 视口 `(32, 16, 384, 448)`），盖掉总分与最高分——本 mod 里得分没有意义。
底板 alpha 从 160 提到 220，否则底下的分数会透出来。行情读数放在面板底（84）与残机行（120）
之间那条空隙（y = 96），持仓净值移到**金钱正下方**（`(620, 218)` 右对齐，与金钱同一个右边界、同款字体与字色 ——
原先贴在金钱左边会撞上「金」的图标，且默认字体偏小），
两者都不与面板重叠，于是不受上面那条顺序约束。

## G. OPEN —— 还没解决的

| # | 事项 | 说明 |
| --- | --- | --- |
| G1 | ~~游戏内未实跑~~ | **三步实跑通过**，见 §0 |
| G2 | ~~全有或全无的门还没做~~ **已做**（2026-09-02） | 见 §H。触发原因是用户复审指出的一种「日志一切正常」的静默失败 |
| G3 | 扩容还要动的东西 | B（§I）、C（§J，manager 部分）、D（§K）已做；`zAbilityMenu` 的 `__card_ids` 扩容与顺序表耦合，归 E；E 未做 |
| G5 | ~~新卡在卡组编成里「未获取」~~ | 战线 D 已做（§K，待实跑）。原因是 `unlocked_cards` 是 `uint8_t[57]`，`[58]` 落在未知区读出 0 |
| G6 | ~~新卡解锁后名字 / 说明是乱码~~ | **已做**（§L，待实跑）——三处读重定向到 DLL 缓冲，占位「测试卡牌 N」。原因：`zAbilityText` 只有 57 张，id 58 落在对象之外 |
| G4 | MSVC 换 build 后骨架是否仍然一致 | 未验证。`make check` 的锚点数会立刻暴露（不是 25 就停） |
