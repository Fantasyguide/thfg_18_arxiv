# th185（100th Black Market）的卡牌 / 货币系统 —— 侦察起点

> **版本**：跨版本（本文主写 TH18.5 = th185 v1.00a，兼与 TH18 对照）。跨版本文档：**每个地址都带版本前缀**
> （`th185:` / `th18:`）。

**背景**：用户 2026-09-18 把 TH18.5 的 Steam 安装目录指过来，提出三件事：
① 能不能把 th185 的**全部卡**加进本仓的 TH18 卡牌 mod（`mods/th18.v1.00a/card-expand/`）；
② **货币（弹货）系统**能不能一并复刻；③ 为承载这些卡，卡表要扩到 **id > 128**（用户实测 >128 有问题）。
本文只做**第一手清点**：先把这个游戏里"卡/货币"相关的系统、全局和结构大小钉死，作为后续深入的路标。

## 1. 样本与一手来源（🟢）

- 样本：`local/th185.v1.00a/th185.exe`，md5 `234b1e6eb1cc1b8b0c6e490487cf191c`
  （原始位置 `D:\SteamLibrary\steamapps\common\th185`，只复制了 exe；`th185.dat` 等留在原处）。
- 版本：`readme.txt` 标 2022/08/14；exe 内含字符串 `1.00a` ⇒ 目录名按 `th185.v1.00a` 登记。
- 命名来源：`local/vendor/th-re-data/data/th185.v1.00a/`（ExpHP 金矿）。
  **金矿在这个版本上明显变薄**：`funcs.json` 58 条、`statics.json` 41 条、
  `type-structs-own.json` 36 个类型、`type-structs-ext.json` 89 个类型，
  且**没有** `zTableCardData` / `zCardBaseClass` / `zVTableCard` / `zAbilityMenu` 这些
  th18 侧已有的卡系统类型名 ⇒ **th185 的卡层没有被社区命名**，要自己认。

## 2. 三个系统的锚点（符号名来自金矿，🟢）

| 系统 | 全局（静态指针） | 结构大小 | 入口 |
| --- | --- | --- | --- |
| 能力/卡管理器 | `zAbilityManager*` @ `th185:0x4d7ab8` | `0x117c` | `th185:0x408690`（operator new）、`on_tick` `th185:0x408a40`、`on_draw` `th185:0x408fc0` |
| 黑市（弹货商店） | `zBlackMarket*` @ `th185:0x4d7ac4` | `0x1254` | `initialize` `th185:0x41d430`、`on_tick` `th185:0x41e150`、`on_draw` `th185:0x41f4a0` |
| **永久卡商店**（货币消费端） | `zPermanentCardShop*` @ `th185:0x4d7ac8` | `0xe4c` | `initialize` `th185:0x41fb50`、`on_tick` `th185:0x420150`、`on_draw` `th185:0x421130` |

- ★ **结构大小对比（一手，来自两份金矿的结构表末项）**：th185 的 `zAbilityManager` 是 `0x117c`，
  th18 的同名结构是 `0xd70` ⇒ **差了 `0x40c`，两者不是同一份结构**。
  推论：卡系统同源但**已经改过**，任何"把 TH18 的 hook 直接搬过去"的想法都必须先按字段核对（🟡 推测）。
- 顺带登记：`th185:0x421580`（金矿名 `sub_421580_trophy_related`，成就/奖杯相关）；
  商店两张 UI 的 tick 就是上表的 `th185:0x41e150` 与 `th185:0x420150`。

## 3. 其它可用锚点（金矿直接给出，🟢）

- 全局：`GAME_THREAD_PTR` `th185:0x4d7b0c`、`STAGE_PTR` `th185:0x4d7ad8`、`GUI_PTR` `th185:0x4d7b08`、
  `CURRENT_STAGE_DATA` `th185:0x4d7c54`、`STAGE_DATA_TABLE`（`zTableStageData[0xa]`）`th185:0x4cba90`、
  `ANM_MANAGER_PTR` `th185:0x529e88`、`UPDATE_FUNC_REGISTRY` `th185:0x4d7ab4`。
- anm 一线（复刻"按脚本号起 VM"这种机制的必要入口）：
  `AnmManager::get_vm_with_id` `th185:0x48a4a0`、`AnmVm::run` `th185:0x47ac70`、
  `AnmManager::preload_anm` `th185:0x488360`、`AnmManager::on_tick_23` `th185:0x4899d0`。
- 别的子系统：`BulletManager::operator new` `th185:0x429ec0`、`Gui::on_tick` `th185:0x442650`、
  `StageInner::run_std` `th185:0x425e00`、`WinMain` `th185:0x473950`。
  （注意：th185 的 `statics.json` 里**没有** th18 那一批 `BULLET_MANAGER_PTR` / `ENEMY_MANAGER_PTR`
  之类的指针名，弹池/敌机池的全局地址要自己找。）

## 4. 三个待解问题（**下一轮直接从这三条切**）

1. **th185 有多少张能力卡？卡定义表在哪？**
   th18 侧是 `zTableCardData[]`，每项 `0x34` 字节、共 58 项（见
   [`../th18/07-registry.md`](../th18/07-registry.md)）。th185 侧对应结构**没有被命名**，
   要从 `AbilityManager` 的 tick/initialize 里反推表基址与项大小，**先拿到张数** —— 这个数直接决定
   "全部卡搬进来"的规模，也决定第 5 节那个问题的答案。
2. **货币字段在哪？**
   `zPermanentCardShop`（`0xe4c`）与 `zBlackMarket`（`0x1254`）的成员表在金矿里是**空白的**，
   只能反汇编认：钱是哪个偏移、什么类型（int32？）、扣款/加款的唯一入口是哪个函数、
   **跨局持久化**写进哪个存档结构（这是"复刻货币"的真正难点，不是画个 UI 就完）。
3. **卡 id > 128 的坏点**（与 TH18 mod 共通的题目）：
   若 th185 自己的卡数 **>127 且引擎跑得动**，那就**直接证明限制出在 mod 侧而不是引擎侧**；
   反之若 th185 的卡 id 也被压进 ≤7 位，就要找到那个位域的确切位置。**这一条是第 1 条的直接副产品**，
   所以顺序是：先数卡 → 再看 id 位宽。

## 5. 与 TH18 卡牌 mod 的关系（先记判断，别当结论）

- 同源是**事实**（同名 `AbilityManager`、同名 `zAnmVm`/`zGui`/`zUpdateFuncRegistry` 一族）。
- 但 `zAbilityManager` 大小不同 ⇒ **卡基类/虚表槽位必须逐字段核对**后才能谈移植（🟡）。
- "把卡搬过来"的实际工作量取决于**卡的效果是数据驱动还是代码驱动**：th18 侧已经证明是
  「`zTableCardData` 静态表 + 每卡一个 C++ 子类（21 槽虚表）」（见 [`../th18/01-object-model.md`](../th18/01-object-model.md)），
  若 th185 同构，则"加卡"= **搬数据 + 搬行为代码**，不是改几个数字。这条在下一轮用 th185 的
  `AbilityManager::on_tick`（`th185:0x408a40`）确认。

## 5. ★ 样本是 **SteamStub 包装版**，已脱壳（一手，**做 th185 任何反汇编前必须先做这一步**）

**发现**：`local/th185.v1.00a/th185.exe` 的入口点 `th185:0x57d310` **落在 `.bind` 节**（该节 `0x29900` 字节、
带 `CODE|EXECUTE` 标志），`.text` 字节熵 ≈ 8.0，**任何地址都反汇编不出合法指令** ⇒ 这是被
**SteamStub 包装**过的镜像（本仓另外 14 份样本的 EP 全部落在 `.text` 里，只有它一个例外）。

**处置**：用**本仓已有的** [`tooling/steamless`](../../../tooling/steamless/README.md)（Linux 上跑 Steamless）：

```bash
~/.dotnet/dotnet tooling/steamless/bin/Release/net8.0/steamless-linux.dll local/th185.v1.00a/th185.exe
# → local/th185.v1.00a/th185.exe.unpacked.exe（local/ 是 gitignored）
```

**判据（全部通过，逐条实测）**：

| 判据 | 包装版 | 脱壳后 |
| --- | --- | --- |
| 文件大小 | `1,053,440` | `883,200`（正好差 `0x29900` = `.bind`） |
| 入口点 | `th185:0x57d310`（`.bind` 内） | `th185:0x49003b`（`.text` 内）—— **与金矿 `_start` = `th185:0x49003b` 完全一致** |
| `.text` 熵 | ≈ 8.0 | `6.53` |
| 节表 | 含 `.bind` | `.text` / `.rdata` / `.data` / `.rsrc` |
| 符号对账 | — | 6 个金矿函数地址**逐个对上合法序言**（见下） |

符号对账明细（金矿地址 → 脱壳镜像里的序言）：`AbilityManager::operator new` `th185:0x408690`、
`AbilityManager::on_tick` `th185:0x408a40`、`BlackMarket::on_tick` `th185:0x41e150`、
`PermanentCardShop::on_tick` `th185:0x420150`、`AnmManager::get_vm_with_id` `th185:0x48a4a0`、
`WinMain` `th185:0x473950` —— 全部是 `push ebp / mov ebp,esp` 或 `push ebx / mov ebx,esp` 一类的
正常序言，**没有一处是垃圾字节** ⇒ **金矿的 th185 地址与脱壳镜像同源**，可以放心拿金矿当地图。

⚠ **不要用 md5 判等**：Steamless 会重建 PE 容器（零化 DOS stub、重排节头），代码段字节才是原生的。

## 6. ★ 卡表结构：**th185 有 87 个卡槽**，虚表步长 `0x5c`（一手，独立双证）

**手法**（可复用）：卡类构造器都会把自己的**虚表地址**写进对象首字段（`mov dword ptr [对象], imm32`），
所以**把"这类 immediate 的取值集合"统计出来**，就得到卡类虚表的地址集合，进而得到张数与步长。

| 版本 | 虚表地址集合 | 个数 | 步长 | 整除性 |
| --- | --- | --- | --- | --- |
| th18（对照） | `[th18:0x4b4c78, th18:0x4b6010]` | **58** | `0x58` | ✅ 整除 |
| th185 | `[th185:0x4b6ab0, th185:0x4b8998]` | **87** | **`0x5c`** | ✅ 整除 |

- **th18 侧自校准成立**：算法给出 58 个、基址 `th18:0x4b4c78`、步长 `0x58`，与本文 §1 已登记的
  「金矿 `VTABLE_CARD_*` 共 58 项、`VTABLE_CARD_NULL` 在 `th18:0x4b4c78`」**逐项吻合** ⇒ 方法可信。
- **th185 的第二条独立证据**：`.rdata` 里从 `th185:0x4b6ab0` 起，有 **87 行**「22 个 dword 全是 `.text`
  代码指针」的段，**行距正好 `0x5c`**，末行正好是 `th185:0x4b8998`
  （= th18 侧「基类虚表放在数组最后」的那条约定，`VTABLE_CARD_BASE_CLASS` 同形）✓
- **推论一**：th185 的卡槽是 **0–86**，比 TH18 的 0–57 多 **29** 个。
- **推论二**：卡基类虚表**多了 1 个槽**（`0x58 → 0x5c` = 22 → 23 slot）⇒ 想把 th185 的卡**行为**搬过来，
  必须按 th185 的槽位表重核（🟡 多出来的那一槽是什么，未读）。
- ★ **对 §4 问题 3 的直接含义**：**th185 自己的卡 id 只到 86，根本没碰到 127/128 边界**
  ⇒ 它**证明不了** >128 可行，>128 的断点仍然只能在 mod 侧（或 TH18 引擎侧）找。这条把问题 3 的
  调查范围**钉死在 mod 侧**，省掉一整条弯路。

## 7. ★ **货币（弹货）找到了**：`MONEY` = `th185:0x4d1074`（一手，🟢）

**手法**（可复用）：先扫三个商店函数的**绝对全局引用**（`BlackMarket::on_tick` `th185:0x41e150`、
`PermanentCardShop::initialize` `th185:0x41fb50`、`PermanentCardShop::on_tick` `th185:0x420150`），
再只在"**被加减、且和某个返回值比较**"的候选里挑 —— 钱一定同时有收、支、判定三种用法。

**判定链（三条互相独立，都指向同一个地址）**：

1. **收**：`th185:0x40a9eb`（`add dword [0x4d1074], edx`）、`th185:0x412c2e`（`add …, eax`）；
   另有 `th185:0x40ed69`（`mov dword [0x4d1074], 0`）一次性清零。
2. **支**：`th185:0x41ee2d`（**`sub dword [0x4d1074], eax`**）—— 在 `BlackMarket::on_tick` 的购买分支里。
3. **判定**：`th185:0x41e298` / `th185:0x41e316` 都是 `cmp dword [0x4d1074], eax` 后接
   `jge`（买得起才走购买/换货分支）。这个 `eax` 来自 **`th185:0x41f860`** —— 见下。
4. 另有两处阈值比较 `cmp dword [0x4d1074], 0x12c`（=300）：`th185:0x40aea5`、`th185:0x40ee20`。

### 7.1 **价格表**：`[shop + 0xe44]` 起的 dword 数组，项数 `[shop + 0xa40]`

`th185:0x41f860` = **算价函数**（一手反汇编）：

```asm
th185:0x41f860  mov  edi, ecx            ; edi = 商店对象（this）
th185:0x41f864  call th185:0x41f8a0     ; 先把货架刷新一遍
th185:0x41f869  mov  esi, [edi + 0xa40]  ; 项数
th185:0x41f86f  mov  eax, 0x1869f        ; 99999 = 哨兵初值
th185:0x41f878  lea  ecx, [edi + 0xe44]  ; 价格表基址
th185:0x41f880  mov  edx, [ecx]          ; 逐项取价
th185:0x41f882  cmp  eax, edx / jle …    ; 取最小值
th185:0x41f886  test edx, edx / cmovne eax, edx   ; **0 = 该槽无货，跳过**
```

⇒ 价格表 = **`[shop+0xe44]` 起、`[shop+0xa40]` 个 dword**，**0 表示该槽没货**（`test edx,edx; cmovne`
是"零价不算候选"的写法），函数返回**当前最便宜的非零价**（初值 `0x1869f` = 99999 哨兵）。
⇒ 所以"**买不起**"的判定 = `MONEY < 最便宜价`，这就是 §7 判定链第 3 条那个比较。

### 7.2 顺带排除的两个候选（🟡 未定，**不是钱**）

| 地址 | 引用 | 关键用法 | 判断 |
| --- | --- | --- | --- |
| `th185:0x4d1078` | 16 处 | `cmp dword [0x4d1078], 0x64`（=100）`th185:0x4126e6` | 疑为"已持有/进度计数"，与 `th185:0x4d107c` 成对比较 |
| `th185:0x4d107c` | 7 处 | 与 `th185:0x4d1078` 成对（`th185:0x41e3ef`、`th185:0x41d950`） | 疑为该计数的上限 |

两者都**没有**"被价格减"的用法 ⇒ 不是货币。0x100 的差值（100）与"100th"这个标题呼应，
但这是**推测**，未证。

### 7.3 对「复刻弹货货币」的含义

- 结构**极简**：一个 `.data` 全局（钱）+ 一张 dword 价目表（含"0=无货"约定）+ 一个扣款点。
  TH18 侧本来就有 `MONEY`（`th18:0x4ccd34`）与整套商店/金钱系统（见
  [`../th18/05-shop-and-money.md`](../th18/05-shop-and-money.md)）⇒ **复刻货币不是新机制**，
  而是"换一套价目表与入口"。
- **但语义不同，必须分开**：TH18 的 `MONEY` **同时是计分乘数**（`../th18/05 §2`），
  而 th185 这只 `MONEY` 只服务于市场（与价格比较、扣款），**没有看到计分/火力换算**。
  若两者共用同一个全局就会互相污染 ⇒ 实现上应当**另开一个货币字段**，不要复用 `th18:0x4ccd34`。
- 未做：**跨局持久化**（钱/已买断的永久卡是否写进存档、写在哪）—— 这是下一步第一件事。
