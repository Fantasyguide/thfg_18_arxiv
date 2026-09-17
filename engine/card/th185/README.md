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

## 8. ★★ 卡表正身：`th185.dat` 里有一份**纯文本卡表** `ability.txt`（一手，本轮最大收获）

**发现**：把资源档解开（`tooling/thtk/build/thdat/thdat -x d th185.dat`，**284 个文件**）后，里面有
**`ability.txt`**（23125 字节，cp932/CRLF）—— **ZUN 随游戏发行的纯文本卡表**：
每张卡一段，段首是 `@内部名`，下一非空行是卡面显示名，其余是该卡说明文。

⇒ **"th185 有哪些卡"这个问题不需要任何反汇编**：`ability.txt` 里 **86 段**，末段是 `@NULL`
⇒ **85 张实体卡**（内部名 + 显示名 + 说明文全在）。全表索引（源码素材）见
[`_sources/ability-txt-index.md`](_sources/ability-txt-index.md)；说明文正文属版权文本，留在 `local/` 不入库。

### 8.1 与 §6 的虚表算术**闭合**（两条独立证据互相印证）

| 版本 | `ability.txt` 段数 | 虚表条目（§6 实测） | 闭合式 |
| --- | --- | --- | --- |
| th18 | **56**（首 `EXTEND`，末 `MAGATAMA2`，**无 NULL 段**） | **58** | 56 实体卡 + id 56 NULL + id 57 BASE = 58 ✅ |
| th185 | **86**（首 `EXTEND`，**末段 `NULL`**） | **87** | 85 实体卡 + id 85 NULL（有段）+ id 86 BASE = 87 ✅ |

⇒ 两作都是"**实体卡 + NULL + BASE = 虚表条目**"，且 **th185 比 th18 正好多 29 张**（85 − 56）
—— 与 §6 里"虚表条目 87 − 58 = 29"**完全对上**（🟢 双证）。

### 8.2 对「把 th185 的全部卡搬进来」的直接含义

- **卡的内容（名称/说明/内部名）已经到手**，无需解密、无需逆向：直接读 `ability.txt`。
- **还缺的是"每张卡的效果怎么实现"** —— 那在代码里（th185 的卡类），而 th185 金矿**没有**卡类命名
  （§1），所以"效果语义"仍要逐张反汇编；但**卡的"清单与归属"这一层已经不用猜了**。
- 内部名与 th18 侧高度同形（都从 `EXTEND` / `WARP` / `ITEM_CATCH` / `MAINSHOT_PU` 这一族起），
  说明两作卡表**同一套命名约定** ⇒ 做"逐张对照表"（th18 id ↔ th185 内部名）是纯文本工作，成本很低。
- **价格**不在这份文本里（`ability.txt` 只有名与说明）；th18 的价格表是 `th18:0x4b35c4` 的 15 档
  （见 [`../th18/08-catalog.md`](../th18/08-catalog.md) 抬头），th185 侧的价格要另找（§7 已知运行期价目表在
  `[shop+0xe44]`，但**填表来源**未定位）—— 这是下一轮的价格线。

### 8.3 顺带确认的两件事

- **`abcard.anm` 在 th185 里也在**（与 th18 同名）⇒ 复刻"卡图"时两作可以照同一条路走；
  但 §8.8（[`../th18/10-extensibility-limits.md`](../th18/10-extensibility-limits.md)）那条
  "entry 号 8 位"的容量墙仍在，**85 张卡要多少 entry 必须先算**。
- th185 的 anm 查表惯用法（本轮一手）：`movzx eax, dl / imul r, 0x5c / sar esi,8 / [base + esi*4 + 0xe0]`
  —— **低字节 = entry 索引（步长 `0x5c`）、高字节 = 槽/文件索引**，与 th18 的 `th18:0x486d38` 一族同形
  ⇒ **anm entry 的空间结论可以两作通用**。

### 8.4 th18 ↔ th185 卡名对照（纯文本工作，已算完）

用两作的 `ability.txt` 做**内部名集合差**（一手，`local/` 下的两份文本）：

| 项 | 数 | 说明 |
| --- | ---: | --- |
| th185 卡名总数（含 NULL） | 86 | 实体卡 **85** |
| 两作**同名** | **51** | 同内部名 ⇒ 效果大概率同源（th18 侧已有复刻） |
| **th185 独有** | **34** | = 真正需要新写的部分 |
| th18 独有 | 5 | `BOMB` / `BOMB2` / `DBOMBEXTEND` / `EXTEND2` / `MAGATAMA2` |

**th185 独有的 34 个内部名**（全部）：

`TEWI`、`YOUMU_OP2`、`PENDULUM2`、`TORAMARU`、`STAR`、`LUNA`、`SUNNY`、`FLAN`、`FUTO`、`AUN`、
`JOON`、`SHION`、`KEIKI`、`SEIRAN`、`DREMII`、`JYUNKA`、`NITORI`、`TAKANE`、`MINORIKO`、`LARVA`、
`NEMUNO`、`WAKASAGI`、`URUMI`、`SEKIBANKI`、`KUTAKA`、`KOMACHI`、`EIKA`、`SEIJA`、`KANAME2`、`PEACE`、
`SAKI`、`SUIKA`、`YUREIMU`、`YUMARISA`

⇒ 于是"把 th185 的全部卡搬进来"这句话，**工程量可以第一次算清楚**：
**51 张走"同源对照+移植"**（还能拿 th18 的一手实现当参照），**34 张要从零**（每张仍要读 th185 的卡类代码，
因为金矿没有卡类命名）。卡图那侧另有 §8.8 的 entry 容量墙要一起算。

## 9. ★★ 弹货的**定价公式**（渐进定价）与货架结构 —— 一手读全（轮 19）

§7 只知道"钱和价目表在哪"。本轮把**价格怎么算出来的**读完了：`PermanentCardShop` 的刷架函数
`th185:0x41f8a0`（`th185:0x41f860` 进去第一件事就是调它）。

### 9.1 逐指令还原（一手）

```asm
th185:0x41f8be  lea  ebx, [eax + 0xe44]      ; ebx → 价格数组
loop i < [shop+0xa40]:
th185:0x41f8d0  mov  edi, [ebx - 0x400]      ; ★ 货架数组在 shop+0xa44，与价格数组相距 0x400
th185:0x41f8d9  mov  edx, [th185:0x4d7ab8]   ; edx = AbilityManager
th185:0x41f8df  mov  eax, [edi + 4]          ; 卡的 id
th185:0x41f8e2  mov  ecx, [edx + eax*4 + 0xec8]   ; n = 该 id 的**已购/持有计数**
th185:0x41f8e9  mov  eax, [edi + 0x10]       ; 卡的**类别**
th185:0x41f8ec  mov  esi, [eax*4 + th185:0x4b59c0] ; w = 类别权重（静态表）
th185:0x41f8f6  imul esi, [edx + 0x2c]       ; w *= k（AbilityManager+0x2c 的全局倍率）
th185:0x41f8fd  mov  ecx, [edi + 0x40]       ; base = 卡的基准价
th185:0x41f900  sub  ecx, esi                ; base - w·k
th185:0x41f8f3  lea  eax, [ecx+1] / imul eax,ecx / sar eax,1 / inc eax
th185:0x41f908  imul eax, ecx                ; T · (base - w·k)
th185:0x41f912  add  eax, esi                ; + w·k
th185:0x41f914  mov  [ebx - 4], eax          ; ⇒ 写回价格数组
```

⇒ **定价公式**（`n` = 该卡的已购计数，`w` = 类别权重，`k` = `[AbilityManager+0x2c]`，`base` = 卡自带基准价）：

```
T(n+1) = ((n+1)·n)/2 + 1        （n = 0,1,2,3… ⇒ 1, 2, 4, 7, 11, …）
price  = T(n+1) · (base − w·k) + w·k
```

★ **自洽校验（这条最有力）**：`n = 0` 时 `price = base` —— **第一次买正好是基准价**，
与"基准价"这个名字完全吻合 ⇒ 公式读对了（🟢）。买第二张同卡就翻倍（`2·base − w·k`），
第三张 ≈ 4 倍 …**这就是 th185 的"渐进定价"**，也是它和 TH18 商店（固定几档价、`th18:0x4b35c4`）
**最本质的区别**。

### 9.2 类别权重表（一手数据）

`th185:0x4b59c0` 起 16 个 dword：`0, 5×9, 10×5, 0`
⇒ 类别 **0 = 无**、**1–9 权重 5**、**10–14 权重 10**（15 个有效类别）。价格差就来自这两档权重。

### 9.3 货架与"我有哪些卡"的结构（一手）

| 项 | 位置 | 说明 |
| --- | --- | --- |
| 价格数组 | `shop + 0xe44`，项数 `[shop+0xa40]` | 0 = 该槽无货（§7） |
| 货架卡指针数组 | `shop + 0xa44`（= 价格 − `0x400`） | 每项指向**卡对象**；`+4` = id、`+0x10` = 类别、`+0x40` = 基准价 |
| 已购计数 | `AbilityManager + 0xec8 + id*4` | 渐进定价的 `n` |
| 定价倍率 | `AbilityManager + 0x2c` | 公式里的 `k` |
| 持有卡链表 | `AbilityManager + 0x1c` | 每节点：`[node]`→卡对象、`[node+4]`→next |
| 当前选中卡 | `AbilityManager + 0x7c` | `th185:0x41f930` 返回它的 id（空则 −1） |

顺带一条**独立印证 255** 的一手事实：`th185:0x41f930` 把持有卡列成一张 **−1 结尾的 id 表，
上限 `cmp ecx, 0xff`（255）** ⇒ 连"列卡"这种外围函数都按 255 设计（与 §6 的 87 槽是两回事：
槽是**类**，这里是**运行时实例**）。

### 9.4 对「复刻弹货货币」的结论（更新 §7.3）

- **货币本身=一个全局**（`th185:0x4d1074`，§7）+ **收/支三点**（§7）+ **渐进定价公式**（本节）
  ⇒ 复刻路径已经完整：钱、价目、买断计数、类别权重、全局倍率，五件东西都有确切出处。
- **要照抄的关键语义是"渐进"而不是"价目表"**：TH18 的价格是静态 15 档；th185 是按
  **每张卡的已购次数**做三角数增长。若只搬价目表，手感会完全不对。
- **仍缺**：`k = [AbilityManager+0x2c]` 的量纲（谁在改它、范围多少）与
  `base = [card+0x40]` 的**取值来源**（卡数据表本身还没定位 —— th185 的卡对象不是 th18 那种
  `0x34` 行；这是下一轮第一件事，拿到它就能把 85 张卡的价格一次性列全）。
