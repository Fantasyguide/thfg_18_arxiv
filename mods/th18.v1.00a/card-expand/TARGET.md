# TARGET —— card-expand 死绑登记

> **版本**：TH18 v1.00a（`th18.exe`，imagebase `0x400000`）。本文裸地址默认属该版本；引用其他版本须写成 `th16:0x…`。

**这份文件回答一个问题：换一个 exe build，我要重取哪些量？**

对账脚本：`native/sites.py`（`make check` / `make verify`）。**换 build 后先过这一关。**

## 目标二进制

| 项 | 值 |
| --- | --- |
| 游戏 / 版本 | th18 v1.00a |
| exe md5 | `9969cac756098c1da05a81de45437a70` |
| imagebase | `0x400000`（无 DYNAMICBASE） |

⚠️ codecave 里取原生表地址用的是 thcrap 的 **`Rx` 记法**（`<Rxc53c0>`，
`expression.h:310`：相对模块基址的十六进制），**不写死 `0x400000`**。

## 死绑量（只有五个数）

| 量 | 值 | 出处 |
| --- | --- | --- |
| 注册表基址 | `0x4c53c0` | [`engine/card/th18/07-registry.md`](../../../engine/card/th18/07-registry.md) |
| 行长 stride | `0x34` | 同上 |
| 原生行数 | 58 | 同上 |
| 回退行号 | 56（`NULL`） | [`engine/card/th18/11-sentinels-56-57.md`](../../../engine/card/th18/11-sentinels-56-57.md) §2.1 |
| 表的 RVA | `0xc53c0` | = 基址 − imagebase |

**其余 100 个地址全部是算出来的，不是抄来的。**这就是为什么这个 mod 的
`TARGET.md` 比 [`../mouse-control/TARGET.md`](../mouse-control/TARGET.md) 短得多——
写入点由扫描器从二进制里现找。

## 写入点：100 处，全部同长

扫描器保证每一处都只替换 4 字节常量，**opcode 一个字节都不动**（`make verify` 第 ③ 项）。

| 类 | 处数 | 原形态 | 改成 |
| --- | --- | --- | --- |
| `start`（内联查表起点） | 24 | `mov <r>, 表基+f` | `mov <r>, 新表+f` |
| `end`（尾界） | 25 | `cmp <r>, 表尾+f` | `cmp <r>, 新表+行数*0x34+f` |
| `fallback`（查不到的回退臂） | 25 | `mov <r>, 回退行+g` | `mov <r>, 新表+56*0x34+g` |
| `hit`（命中臂） | 25 | `add`/`lea <r>, 表基+g` | `add`/`lea <r>, 新表+g` |
| `start`（表遍历，按计数收尾） | 1 | `mov edx, 表基+4` @ `0x418e0e` | 同 start |

`start` 是 24 而不是 25：`0x414412` 的 `mov eax, 0x4c53c4` 被编译器提到分支之前，
**由 `0x414420` 和 `0x414494` 两个循环共用**。扫描器显式支持这种共享。

## 写入点：战线 B（仅 `ROWS > 58` 时生成）

| 地址 | 原字节 | 改成 | 长度 |
| --- | --- | --- | --- |
| `0x411479` | `83 fb 38` | `83 fb <rows-1>` | 3 |
| `0x411482` | `ff 24 9d ac 2d 41 00` | `ff 24 9d <codecave:th18_card_jumptable>` | 7 |

跳转表源 `0x412dac`（RVA `0x12dac`）57 项；case 56 函数体 `0x411489`（RVA `0x11489`）。

## 写入点：战线 C（仅 `ROWS > 58`，12 处，全部同长）

| 地址 | 原 | 改 |
| --- | --- | --- |
| `0x4082d6` `0x4082ec` `0x40860a` | `push 0xd70` | `push 0xd70+rows*4` |
| `0x407eb0` | `lea edi,[ebx+0xc84]` | `+0xd70` |
| `0x407eb6` | `mov ecx,0x38` | `rows` |
| `0x412d42` | `mov dword [edi+eax*4+0xc84],1` | `+0xd70` |
| `0x416f8f` `0x41744a` `0x417535` | `mov r,0xc84` | `0xd70` |
| `0x41716b` `0x417527` `0x4175e7` | `cmp r,0xd64` | `0xe50`（**只跟到 56**，见 README）|

## 写入点：战线 D（仅 `ROWS > 58`；9 处改 ModRM，短 1 字节 nop 补齐）

| 地址 | 原 | 改（`S` = `<codecave:th18_card_unlocked>`）| 存档指针在 |
| --- | --- | --- | --- |
| `0x41440b` | `cmp [eax+edx+0x5f588],cl` | `cmp [edx+S],cl` | eax |
| `0x4149ec` | `cmp byte [esi+eax+0x5f588],0` | `cmp byte [esi+S],0` | **eax（index）** |
| `0x416590` | `cmp byte [eax+ebx+0x5f588],0` | `cmp byte [ebx+S],0` | eax |
| `0x41694e` | `cmp byte [eax+edx+0x5f588],0` | `cmp byte [edx+S],0` | eax |
| `0x416e3d` | `cmp [edx+ecx+0x5f588],al` | `cmp [edx+S],al` | **ecx（index）** |
| `0x417125` | `cmp byte [eax+esi+0x5f588],0` | `cmp byte [esi+S],0` | eax |
| `0x417ea3` | `cmp byte [ecx+eax+0x5f588],0` | `cmp byte [ecx+S],0` | **eax（index）** |
| `0x418df6` | `cmp byte [esi+edi+0x5f588],0` | `cmp byte [edi+S],0` | esi |
| `0x418e15` | `mov al,[esi+eax+0x5f588]` | `mov al,[eax+S]` | esi |

「存档指针在哪个寄存器」由生成器从站点前 48 字节内最近一条 `mov r32,[0x4cf41c]` 取，
**不按 SIB 槽位推**（3 处存档指针在 index，见 AUDIT §K2）。

战线 D 的死绑量：

| 量 | 值 | 出处 |
| --- | --- | --- |
| `unlocked_cards` 偏移 | `0x5f588`（`uint8_t[0x39]`）| ExpHP `type-structs-own.json`；`engine/card/th18/11` §1 |
| `SCOREFILE_PTR` | `0x4cf41c` | `ScoreFile__load` `0x463984` 写入 |
| 存档目录缓冲 | `0x568c61`（RVA `0x168c61`）| `Window__sub_4726a0`，`%APPDATA%\ShanghaiAlice\th18\` |
| `mark_obtained` | `0x418de0`（测试钩子调它）| `CardCollection__mark_obtained_and_notify` |

## hook 点：战线 D 的三个断点（`_255` patch 里）

| 名 | 地址 | 原字节 | 长 | 干什么 |
| --- | --- | --- | --- | --- |
| `ce_unlock_write` | `0x418e04` | `c6 84 3e 88 f5 05 00 01` | 8 | 影子[edi]=1；id<57 放行原指令，否则写 side-car 并跳过 |
| `ce_save_loaded` | `0x46398a` | `8d b3 b8 f4 05 00` | 6 | 影子 ← 原生存档（ebx）+ side-car |
| `ce_unlock_all` | `0x4648fe` | `8d 83 88 f5 05 00` | 6 | 影子[0..55]=1 |

## 写入点：战线 E 第二块（图鉴 / 编成，仅 `ROWS > 58`，27 处全同长）

| 地址 | 原 | 改 |
| --- | --- | --- |
| `0x414401` `0x4145f8` `0x414639` `0x415681` `0x4156b6` | `[r*4+0x4b3600]` | `[r*4+<codecave:th18_card_order>]` |
| `0x41499f` | `mov eax,0x4b3600` | cave |
| `0x414b54` | `cmp eax,0x4b36e4` | `cave+0x3fc`（255 项）|
| `0x413817` `0x413831` `0x413abb` | `push 0x13fc` | `push 0x17f8` |
| `0x41495c` | `mov edi,0x38` | `0xff` |
| `0x4145d2` / `0x414b3f` | `[esi-0x4e8]` / `[eax-0x8e8]` | `[esi+0xc10]` / `[eax+0x810]` |
| 14 处 | `[…+0x304]` | `[…+0x13fc]` |

⚠️ `0x4337f7 mov eax,[eax*4+0x4b36e4]` 用的是**紧邻的另一张表**，值相同，不许改（生成器显式排除）。

**运行时由 DLL 写的立即数**（patch 不动，门里按 56 + 已注册新卡数现写）：
`0x4137bb` `0x414394` `0x41439e`（imm32）、`0x4145e2` `0x4157cb`（**imm8**，上限 127）、`0x41570d` `0x415817`（imm32）。

死绑量：顺序表 `0x4b3600` 57 项（RVA `0xb3600`）；`zAbilityMenu` `0x13fc`；`__card_ids` `+0x304`；列数全局 `0x5704bc`。

## hook 点：战线 E 第一块（文案重定向，`_255` patch 里）

| 名 | 地址 | 原字节 | 长 | 干什么 |
| --- | --- | --- | --- | --- |
| `ce_text_name` | `0x416694` | `69 cb c0 01 00 00` | 6 | `imul ecx,ebx,0x1c0` → ecx = 重定向偏移 |
| `ce_text_desc` | `0x416779` | `69 45 0c c0 01 00 00` | 7 | `imul eax,[ebp+0xc],0x1c0` → eax |
| `ce_text_notify` | `0x41926a` | `69 c3 c0 01 00 00` | 6 | `imul eax,ebx,0x1c0` → eax |

死绑量：`ABILITY_TXT_PTR` `0x4cf29c`；条目 `0x1c0`；原生 57 张。

## 写入点：patch-test（只在验证时进栈）

| 地址 | 原字节 | 改成 | 说明 |
| --- | --- | --- | --- |
| `0x407ee3` | `0f b6 84 30 08 f6 05 00` | `e8 [cave] 90 90 90` | 8 字节；cave 里先执行原 `movzx`，再把 56 改 58 |
| `0x411469` | `81 7f 28 00 01 00 00` | thcrap 断点，`cavesize` 7 | `BP_ce_trace_alloc`，读 `[ebp+8]`/`[ebp+0xc]`；新 id 第一次出现时调 `mark_obtained(id,1)` |

## hook 点小结

第 1 步 patch **没有 hook 点**：申请 codecave，改常量，完（自检门 `ce_gate` 除外）。
`_255` 多三个断点（战线 D，上表）；patch-test 再多一个追踪断点。

DLL 只有一个被 thcrap 调用的入口 `th18_card_expand_mod_post_init`
（`init.cpp:420` `mod_func_run_all("post_init")`），跑在 thcrap 初始化线程上、
游戏代码开始之前；它**不注入任何游戏函数**。

## codecave

| 名字 | 大小 | 权限 | 内容 |
| --- | --- | --- | --- |
| `th18_card_table` | 行数 × `0x34` | RW | 开机时从原生表拷贝而来 |
| `th18_card_jumptable`（战线 B） | 行数 × 4 | RW | 0–56 原样拷，其余 → case 56 |
| `th18_card_table_patch_init` | 20–66 字节 | RX + export | 保险用的拷贝代码；权威是 DLL 的 post_init |
| `th18_ce_test_deck58`（patch-test） | 19 字节 | RX | 原 `movzx` + 空槽改 58 |

`*_patch_init` 由 `patch_func_init` 调用（`binhack.cpp:1724` → `plugin.cpp:304`），
签名 `void (TH_CDECL *)(void *param)`（`plugin.h:71`）。

⚠️ **它跑在 `codecaves_apply` 末尾，早于 `binhacks_apply`**（`runconfig.cpp:655-656`），
且是否被调用取决于 thcrap 版本。所以它只是保险；**填表与验证的权威是 DLL 的
`post_init`**，见 [`README.md`](README.md)。DLL 从 `func_get("codecave:th18_card_table")`
（`plugin.h:24`，`THCRAP_API`）取地址，从 `GetModuleHandleA(NULL) + 0xc53c0` 读原生表。

## 换版本时必须重取

- [ ] 注册表基址 / stride / 行数（`sites.py` 顶部的五个常量）
- [ ] 表的 RVA
- [ ] 确认查表骨架没变（MSVC 换版本可能生成不同形状）——`make check` 的锚点数会立刻告诉你
- [ ] 确认那两条不变式仍然成立

可以借的只有：方法论、骨架的**形状**、两条不变式的**定义**。
