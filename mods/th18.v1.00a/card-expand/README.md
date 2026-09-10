# card-expand —— TH18 卡表搬迁 / 扩容的基础设施

> **版本**：TH18 v1.00a（`th18.exe`，imagebase `0x400000`）。本文裸地址默认属该版本；引用其他版本须写成 `th16:0x…`。

把 `zTableCardData[]` 从 `.data` 搬进 codecave，为「加新卡」腾出行数。
**「加一张卡到底改了什么」的追溯表在 [`MAP.md`](MAP.md)**——150 条 binhack、7 个断点、5 个 codecave 各自的出处。
**想加一张卡：读 [`DATA.md`](DATA.md)**（登记，JSON）和 **[`SDK.md`](SDK.md)**（行为，C）；已有的卡在 [`CARDS.md`](CARDS.md)。
踩过的坑与方法论总结在 [`../../LESSONS.md`](../../LESSONS.md)。
**★ 铁律：UI / 动画一律先从原代码找** —— 动手写任何 UI、动画、特效前，先在原游戏的 anm 脚本 +
引擎函数里找到它**原本怎么做**（脚本号 / 地址 / 数值 / 触发条件）并照抄；贴图走零售切图。
完整版与反面教材见 [`../../../METHOD.md`](../../../METHOD.md)。
方案全貌见 [`../card-rework/PLAN-255-ids.md`](../card-rework/PLAN-255-ids.md)，
边界依据见 [`engine/card/th18/11-sentinels-56-57.md`](../../../engine/card/th18/11-sentinels-56-57.md)。

**做到哪了**（下一步交接见 [`NEXT.md`](NEXT.md)）：

| 步 | 内容 | 状态 |
| --- | --- | --- |
| 1 | 只搬表，58 行，行为零变化 | ✅ **实跑通过**（2026-09-02）；2026-09-07 退役，不再进 dist / modkit（复现：`make ROWS=58 gen verify dll files`）|
| 2 | 255 行，仍零变化 | ✅ **实跑通过** |
| 3 / 战线 B | 分配器搬迁 + 验证钩子（`make step3`） | ✅ **实跑通过**：`allocate_new_card(id=58)` 真的发到手上 |
| C | `zAbilityManager` 扩容，`owned[]` 255 项（并入 `_255`）| ✅ **实跑通过** |
| D | 存档影子数组 + side-car（并入 `_255`）| ✅ **实跑通过**：id 58 获得 → 重启仍解锁。见下「战线 D」 |
| E | 图鉴 / 顺序表 / 文案 / 图 / 商店 / 数据 | 文案 ✅、图鉴 + 编成 ✅ **实跑通过**；**商店 + JSON 数据装载**（[`DATA.md`](DATA.md)）静态审计通过 **待实跑**；图由写卡的人改 ANM |
| **9 行为** | 行为 SDK + 黑桃五张 | 断点换虚表 + `thiscall` 桩（[`SDK.md`](SDK.md)），静态审计通过（AUDIT §O）**待实跑** |

第 1 步的价值不在功能，而在用「和香草没差别」这个最容易判定的标准，
一次性验证 100 处搬迁 + 生成器 + 对账器 + 开机自检。

## 这里没有手写的 patch

`patch/th18.v1.00a.js` 是 **`native/sites.py` 从真 exe 生成的**，不要手改。

```bash
cd native
make quick          # 主机 C 单测 + pytest
make check          # 资源、站点、文档与 DLL ABI 完整检查
make package        # 生成完整 dist 发布包
make list           # 25 个查表实例的全表
make gen            # 生成 patch（ROWS=58；ROWS=255 出扩容版）
make verify         # 把生成好的 patch 拿回真 exe 逐条对账
make files          # 刷新 files.js 的 crc32
```

理由：100 处 `expected` 手写等于必然出错，而 thcrap 对 `expected` 不匹配的处理是
**记一行日志然后跳过**（`binhack.cpp:1420`）——对整表搬迁来说，部分应用就是灾难。

死绑量（表基址 / stride / 行数 / 回退行 / RVA）与换 build 要重取的清单在 [`TARGET.md`](TARGET.md)；
对抗审计与实跑记录在 [`AUDIT.md`](AUDIT.md)。

## 三个文件各干什么

| 文件 | 角色 |
| --- | --- |
| `native/shape.py` | **权威站点来源**。整体匹配内联查表的骨架，每次命中自带「四条臂配套」的保证 |
| `native/x86imm.py` | **完整性审计**。扫出所有撞上这些值的地方，凡不在骨架里的都列出来人工过目 |
| `native/sites.py` | 串起来 + 两条不变式校验 + 生成 patch + 对账 |

### 为什么不是「找到 4 字节值就改」

因为那件事**天然有歧义**：`0x4c5f8c` 既是卡表的尾界立即数（`cmp eax, 0x4c5f8c`），
又是一个被 9 个函数读写的**热全局**的地址（`mov eax, [0x4c5f8c]`）。
更糟的是 `83 3D <K> 00`（`cmp dword [K], 0`）的第二个字节 `3D` 自己就是
`cmp eax, imm32` 的 opcode——两种读法都「自洽」。

**第一版就是这么写的，结果把 39 个 END 当成 25 个。**
改成匹配整段骨架之后，锚点（`add r32,0x34` 紧跟 `inc r32`）在整个 `.text` 里
**正好 25 处**，与 25 个内联点一一对应，没有歧义可言。

## 两条不变式 —— 安全性全压在这上面

`make check` 对 25 个实例逐个验：

1. **`END - 表尾 == LOOP_START - 表基`** —— 循环比较的字段和终止条件必须是同一个。
2. **`FALLBACK - 回退行 == HIT_ARM - 表基`** —— 两条臂必须取同一个字段。

★ 违反 ② 正是整表搬迁**唯一会静默算错**的失败模式：LOOP_START 指了新表、
HIT_ARM 还指旧表时，`下标 * 0x34 + 旧基址` 会返回**另一张卡的行**——不崩、不报错。
所以必须按实例校验，不能按站点。

## 「干净」的四个具体做法

**① 仓库不留一个版权字节。** 新表的内容不写进 patch（那是 ZUN 的数据），
改成开机时从**用户自己那份 exe** 里 `memcpy` 过来。patch 里只有地址、原字节和源码。

**② 原表不删不覆盖。** 新表是拷贝。任何漏改的站点仍然看到一张合法的（只是旧的）表，
失败模式从「内存腐败」降级为「新卡不出现」。第 1 步行数不变，两张表内容逐字节相同，
**连漏改都不会有可观察的后果**——这正是它适合当第一步的原因。

**③ 生成器与对账器分开。** `gen` 产出，`verify` 把产物拿回真 exe 重新核对
（`expected` == exe 字节、`code` 与 `expected` 等长、只换常量不换 opcode）。
只信生成器等于没查。

**④ 完整性审计留档。** `make check` 会列出所有撞上这些值但不在骨架里的位置
（当前 15 处，全部是那两个热全局），让人确认「漏掉的都是该漏的」。
`CardCollection__mark_obtained_and_notify` 的表遍历就是被这条路径捞回来的——
它按计数收尾（`cmp ecx, 0x38`）而不是按地址，骨架不同，差点漏掉。

## 战线 B —— 分配器（`make step3`）

让 `allocate_new_card(id ≥ 57)` 合法地产出一张卡。**没有手写一个字节的构造器**：

| 改什么 | 怎么改 |
| --- | --- |
| 跳转表 `0x412dac`（57 项） | 搬进 codecave `th18_card_jumptable`（255 项）：0–56 原样拷，**57–254 全指向 case 56 的函数体 `0x411489`** |
| `0x411479` `cmp ebx, 0x38` | → `cmp ebx, 0xfe`（3 字节，同长）|
| `0x411482` `jmp [0x412dac+ebx*4]` | → `jmp [新表+ebx*4]`（7 字节，同长）|

为什么指向 case 56 就够：它的函数体做 `new(0x54)` → memset → 挂基类虚表 `0x4b4c78`
（22 槽全是 `xor eax,eax; ret` 之类的空函数，无空槽）→ `jmp 0x412cd5`；
公共尾段再把 **`card->id` 写成 `ebx` 里的真实 id**（`0x412cec`）。
于是任何未注册的 id 都得到一张挂着**自己的 id**的无行为卡——「克隆现有卡」最干净的形式，
而 [`AUDIT.md`](AUDIT.md) §A 里那条 `0x412cd5` 的栈契约根本不用碰，因为我们复用的是原装的 case。

## 战线 C —— `zAbilityManager` 扩容（并入 `_255`）

12 处，全部同长：3 处 `push 0xd70`（`operator_new` / 它的 memset / sized delete）→ `0x116c`；
`owned[]` 从 `+0xc84` 搬到 `+0xd70`（`reset_cards` 的 `lea` + `rep stosd` 项数 56→255、
`allocate_new_card` 尾段的 `owned[id]=1`、商店三处循环起点）。

★ **商店三处循环的上界只跟到 56**（`+0xe50`），不跟到 255。原因是筛选链：
第一轮要的是 `owned==0` → `is_available_at_stage==1` → **`+0x14==0`**（固定商品），
而 NULL 行 `+0x14=0`、`unlocked[56]=1`——**它能过**；id 58–254 查表落到回退行（也是 NULL）→ 全过
→ 约 198 个候选写进 57 槽的栈数组 `[ebp-0xe4]`。所以新卡进商店池不是把上界抬高就行，
要在 E 里加「查表命中才算」（`entry->id == esi`）的 codecave 筛选，那不是同长改写。

DLL 的 `check_grow` 按运行时 rows 现算这 12 处的改后字节逐一核对。

## 战线 D —— 存档：影子数组 + side-car（并入 `_255`）

`unlocked_cards` 是 `zScoreFile` 里的 `uint8_t[57]`，后面没余量，又在存档里。所以：

- **读**（9 处）改成读 codecave `th18_card_unlocked`（256 字节，下标 = id）。改的是 ModRM（去掉 SIB），
  每处短 1 字节 nop 补齐。⚠️ `[base+index]` 里存档指针放哪一格是随机的（3/9 在 index），
  生成器从站点前面那条 `mov r32,[SCOREFILE_PTR]` 决定丢哪个寄存器，对账器独立再算一遍（AUDIT §K2）。
- **写**（`mark_obtained` `0x418e04`）改成断点 `BP_ce_unlock_write`：影子[id]=1；**id<57 放行原指令**
  （零售数组照常写，`scoreth18.dat` 逐字节与香草一致）；id≥57 跳过原指令、立刻写 side-car。
- **初始化**：断点 `BP_ce_save_loaded`（`ScoreFile__load` 尾段）把零售 57 项拷进影子，再从 side-car 读 57..254。
- **作弊解锁** `unlock_all` 多一个断点镜像到影子（否则读写分家到重启）。

side-car：`%APPDATA%\ShanghaiAlice\th18\th18_card_expand.sav`（路径取自游戏自己的存档目录缓冲，
取不到退到 exe 目录）。271 字节：`"TH18CEXP"` + 版本 + 255 + 255 个字节。**只有 id≥57 那段被读回**。
每次解锁新 id 立刻写盘（先 `.tmp` 再原子替换），所以不用管 `scoreth18.dat` 何时落盘。

**没有 DLL 时**影子全零 ⇒ 所有卡「未获取」。`_255` 本来就要 DLL（B 的兜底也在里面），启动器里 DLL 必勾。

**验收**（叠 `patch-test`，DLL 会在第一次分配到新 id 时替你调 `mark_obtained(58,1)`）：

1. 第一次启动：日志有 `unlocked: shadow @ …, 57 retail (N set) + side-car (0 new ids set)`；
   开一局（第一格留空）→ 弹「获得卡牌」通知 → 日志 `unlock: id=58 (NEW; shadow + side-car saved)`；
   存档目录出现 `th18_card_expand.sav`。
2. **退出再进**：日志 `side-car (1 new ids set)`；进局后卡列表里那张不再是「まだ手に入れてない」，
   名字是「**测试卡牌 58**」，说明栏三行占位文案（获得通知里也是这个名字）。
3. `scoreth18.dat` 的 md5 与打补丁前**相同**（除非这局解锁了零售卡）。
4. 结论行变成 `OK: … allocator relocated, manager grown, unlocked shadowed, 100/100 sites verified`，
   前面多一行 `unlocked: shadow @ …, 9 read sites + 3 breakpoints verified; side-car = <路径>`。

## 战线 E 第一块 —— id ≥ 57 的文案（并入 `_255`）

`zAbilityText` 只有 57 张 × 0x1c0（名字 + 6 行说明，每行 0x40），第 58 张已在对象之外。
不扩对象（尾部 7 个 vm id 字段的访问点没数全），改成**重定向**：三处 `imul r, id, 0x1c0`
（`0x416694` 卡名、`0x416779` 说明、`0x41926a` 获得通知）挂断点，id≥57 时把 r 改成
「加上基址后落进 DLL 缓冲」的偏移。缓冲现在是占位文案「测试卡牌 N」（UTF-8，thcrap 的 textdisp
先按 UTF-8 解），战线 E 的真数据落地时就用这块缓冲。⚠️ 名字会被当 printf 格式串，文案里不能有 `%`。

## 战线 E 第二块 —— 图鉴 / 编成里出现新卡（并入 `_255`）

两个菜单都按**显示顺序表**（`0x4b3600`，57 个 id）走；图鉴的条目数是写死的 56，编成按
「可见且已解锁」动态收集，收进 `zAbilityMenu.__card_ids[56]`。所以：

- 顺序表搬进 codecave `th18_card_order`（255 项）；DLL 在门里重排成
  `[零售 0..55, 已注册新 id…, 56(空槽), 其余 57(BACK，两处都不可见)]`。
- `zAbilityMenu` 从 `0x13fc` 扩到 `0x17f8`，`__card_ids` 搬到 `+0x13fc`（16 处，含两处游标相对寻址）。
- 图鉴条目数 7 处立即数由 DLL **按 56 + N 现写**（N = 注册表 `cards.c` 里的新卡数），
  两处是 `cmp r,imm8` → 条目数 ≤ 127。PLAN 里「→0xff」是错的（符号扩展成 −1）。

**验收**：图鉴最后多出一格（锁着显示卡背；解锁后是 NULL 图标 + 「测试卡牌 58」）；
解锁后编成列表里空槽前多一张，可以直接选进卡组——从此不再需要 `_test` 来把它塞进空槽。
日志：`menu: order table @ … rebuilt (56 retail + 1 new + NULL, rest BACK); encyclopedia entries = 57`，
结论行多 `menu extended`。

## 战线 E 第 7 + 10 段 —— 商店里出现新卡、新卡从 JSON 来（并入 `_255`）

**数据**：DLL 在门里 `stack_game_json_resolve("cards.js")`，thcrap 把栈里每个 patch 的 `th18/cards.js`
深合并；每个键 = id（58–254），值 = 表行字段 + 文案 + sprite 索引。逐张校验后写 codecave 第 id 行、
覆盖文案缓冲、登记进注册表（图鉴 / 顺序表由它驱动）。任何一条错 → 整包 FAIL。格式与上限见 [`DATA.md`](DATA.md)。

**商店**：三处循环上界从 56 抬到 rows（`grow_41716b/417527/4175e7`）。幻影 id 查表回落到 NULL 行 56，
装载器把 cave 里 56 / 57 两行的 `+0x14`（权重）写成 6，三条筛选（`≠0&&≠6` / `==0` / dmode 1–5）就都过不了——
**没有新的机器码**。前提「`+0x14` 只有商店读」的穷举在 [`AUDIT.md`](AUDIT.md) §N1；随机池 560 份 / offer 57 槽的
容量由装载器现算（§N2）。

**验收**：`patch/th18/cards.js`（`_255` 自带的卡池：黑桃五张，开局即解锁）出现在图鉴 / 编成 / 商店（5–9 档价，权重 2），
能买、能拿、重启仍解锁。日志：`cards: 5 registered from cards.js; shop pool …`，`grow: … shop loops 255 ids`，
结论行多 `cards loaded`。

### 验证钩子 `patch-test/`（只在测试时进栈）

| 钩子 | 干什么 |
| --- | --- |
| `0x407ee3` binhack → `th18_ce_test_deck58` | `reset_cards` 读初始卡组时，**空槽(56) 改发 id 58**。不写存档、不改文件 |
| `0x411469` 断点 → `BP_ce_trace_alloc` | 每次 `allocate_new_card(id, mode)` 记进日志；**新 id 第一次出现时调 `mark_obtained(id,1)`**（战线 D 的验收，否则没有路能解锁新卡）|

**流程**：`make step3` → `patch/` 进栈（DLL 不用换），再把 `patch-test/` 叠在上面（它还带示范卡的 `th18/cards.js`）→
卡组编成里把**第一格清空**（选那个空白项）→ 开一局。日志里应出现：

```
jumptable: 255 entries at 0x… (57 retail + 198 -> case56 @ 0x00411489); allocator bound = 254
OK: table filled (255 rows @ 0x…), allocator relocated, 100/100 sites verified
trace: allocate_new_card(id=58, mode=1)  <- NEW ID
```

第三行是定论：香草会在这里分配 id 56 然后被 `FUN_00407f10` 按 `id==56` 摘掉；
我们的 58 不会被摘，会以 NULL 的图标留在卡列表里（sprite 116 长什么样还没看过）。

战线 C 并入 `_255` 后 59–254 都合法（`owned[]` 已扩到 255 项）。

## 发布 —— 一条命令，Windows 只负责拉

renkolab 是**开发仓库**，[`Renko6626/th18_modkit`](https://github.com/Renko6626/th18_modkit)
是**发布仓库**（自带 thcrap 2024-11-06 + 勾选式启动器，朋友 clone 下来放好 exe 就能一键启动）。
它的克隆在 `local/vendor/th18_modkit`。

```bash
cd native
make release          # 构建 dist（DLL 可复现,无时间戳）→ 同步进 modkit → 在那边提交
make release PUSH=1   # 再 push
```

`release.py` 只覆盖**生成物**（三个 patch 的 `th18.v1.00a.js`、`files.js`、DLL）；
`patch.js`、侧车 `.json`、README 是 modkit 里手工维护的文案，**不碰**。
发布仓库不干净或落后于远端时它会拒绝/先快进。幂等：没变化就不提交。

modkit 里的对应关系：

| renkolab `dist/` | modkit |
| --- | --- |
| `patch-step3/` | `thcrap/repos/Renko_1055/th18_card_expand_255/` |
| `patch-test/`  | `thcrap/repos/Renko_1055/th18_card_expand_test/` |
| `patch-devstage/` | `thcrap/repos/Renko_1055/th18_card_expand_devstage/` |
| `bin/th18_card_expand.dll` | `mods/th18_card_expand.dll` |

启动器里：**`th18_card_expand_255` + DLL 必勾**；`_test` 只在验证战线 B 时叠上，
`_devstage` 只在要快速刷商店 / 试卡时勾。
2026-09-07 起步骤 1（`th18_card_expand`）与空壳补丁 `renko` 已从 modkit 移除：
前者对玩家零效果、只在启动器里制造「二选一」误选，后者是从没填过东西的空目录。

## 装法（不用 modkit 时）

| 放哪 | 什么 |
| --- | --- |
| patch 栈里加上 `patch/` 这个目录 | codecave 声明 + 100 条 binhack |

不需要 DLL。本地测试见 [`../../thcrap-platform.md`](../../thcrap-platform.md) §6.2。

## 日志

DLL 写**自己的**日志：`<游戏 exe 所在目录>/th18_card_expand.log`（写不了退到 `%TEMP%`），
每次启动新开一份，第一行是时间戳。路径从 `GetModuleFileNameA(NULL)` 拼**绝对**路径——
⚠️ 不能用相对路径：thcrap 注入时先 `SetCurrentDirectory(thcrap/bin)`，跑完整个 init
（含 `plugin_init` 与 `post_init`）才恢复 CWD（`inject.cpp:355-390`），
相对路径会把日志写进 `thcrap/bin/`。第一版就是这么丢的。thcrap 自己的日志里只镜像
**结论那一行**（`[th18_card_expand] OK/FAIL …`），不刷屏。

## 怎么算通过

`th18_card_expand.log` 末尾（或 thcrap 日志里）找这一行：

```
OK: table filled (255 rows @ 0x…), allocator relocated, … , 100/100 sites verified
```

**没有这一行就是没通过**，不管前面的 binhack 日志多整齐。可能的红字：

| 日志 | 意思 |
| --- | --- |
| `FAIL: func_get unavailable` | thcrap 太老，拿不到 codecave 地址；表**没填** |
| `FAIL: codecave:th18_card_table not found` | patch 没进栈，只放了 DLL |
| `FAIL: table sanity …` | 零售表地址错了（换 build？）|
| `FAIL: N/100 sites verified … partial application, DO NOT PLAY` | 有 binhack 没打上（`expected` 不匹配被跳过），第一处的实际字节已打出来 |

第 1 步（`ROWS=58`，已退役）的判据是同一行的 58 行版，外加进游戏后**商店、图鉴、卡组编成、
局内用卡全部与香草无差别**——那一步的全部意义就是让「搬迁不完整」变得可观测。

## 目录

```
card-expand/
├── README.md          # 你在这
├── DATA.md            # ★ 怎么用 th18/cards.js 登记一张新卡
├── SDK.md             # ★ 怎么给一张卡写行为（C）
├── CARDS.md           # ★ 已实装的卡：id / 效果 / 实现 / 状态
├── NEXT.md            # ★ 下一个会话从这里开始
├── MAP.md             # ★ 追溯表:一张卡要经过的 10 段路,每条 binhack/断点/codecave 的出处
├── TARGET.md          # ★ 死绑登记
├── AUDIT.md           # 对抗审计
├── native/
│   ├── shape.py       ★ 查表骨架匹配器(权威站点来源)
│   ├── sound_sites.py ★ 音效表扩容:52 处站点表 + 完整性审计
│   ├── sound_emit.py  ★ 音效表扩容:codecave / patch_init / 51 处 binhack 发射器
│   ├── x86imm.py      x86 常量定位器(完整性审计用)
│   ├── sites.py       ★ 扫描 / 校验 / 生成 / 对账
│   ├── mkfiles.py     刷新 patch/ 与 patch-test/ 的 files.js（递归）
│   ├── release.py     dist → 发布仓库 th18_modkit
│   ├── sites_gen.h    生成物:DLL 用的站点表
│   ├── card_expand.h  DLL 内部共用声明
│   ├── dll_main.c     入口 / 日志 / thcrap API / 自检①(零售表签名)
│   ├── selfcheck.c    ★ 自检②:BP_ce_gate 填表(+跳转表) + 回读站点
│   ├── unlocked.c     ★ 战线 D:影子数组 + side-car + 三个断点
│   ├── text.c         战线 E 第一块:id≥57 文案重定向(三个断点)
│   ├── cards_def.h/.c ★ 一张卡的定义:校验 / 表行编码 / 商店容量(纯逻辑)
│   ├── tests/         cards_def 的主机单测(make test-host)
│   ├── cards.c        ★ 战线 E 第 10 段:装 th18/cards.js → 表行 + 文案 + 注册表;cards_dev.js
│   ├── sdk.h engine.h ★ 行为 SDK:CE_CARD 宏 / 桩 / 引擎地址
│   ├── sdk_core.h/.c  注册表 / 对账 / 状态槽(主机单测)
│   ├── sdk.c          ★ 断点 ce_card_bind / ce_item_score、门里守卫与对账
│   ├── cards/         每张行为卡一个 .c(黑桃 s10 sj sq sk sa、pot 强欲之壶)
│   ├── menu.c         战线 E 第二块:顺序表重排 + 图鉴条目数 + 站点核对
│   ├── sound.h/.c     ★ 第 11 段:BP_ce_snd_gate 填语音 blob + 新行配置 + I1/I2 自检
│   ├── voice_ids.h    生成物(gitignored):CE_VOICE_<NAME>,由 assets/build_voice.py 出
│   ├── bp_trace.c     测试断点:记录 allocate_new_card(id, mode);新 id 顺手 mark_obtained
│   ├── thcrap_bp.h    断点 ABI(与 mouse-control 同一份)
│   ├── th18_card_expand.def
│   └── Makefile
├── assets/
│   ├── cards/ ability/ ecl/   卡图 / 场上特效 / 开发关卡
│   ├── build_voice.py         ★ 语音校验 + 索引(与 ORDER.txt 对账)
│   └── voice/                 ★ ORDER.txt + <NAME>.wav(wav gitignored)
├── patch/
│   ├── patch.js
│   ├── files.js
│   ├── th18/cards.js   ★ 本 mod 的卡池(黑桃五张,initial_unlocked)
│   ├── th18/voice.js   ★ 语音登记(wav / id / volume / pan)
│   ├── th18/voice/     语音 wav(gitignored;随 make release 进 modkit)
│   └── th18.v1.00a.js  ★ 生成物,不要手改(仓库里是 ROWS=255 = step3)
└── patch-test/         只在测试时叠上
    ├── patch.js        依赖 th18_card_expand
    ├── files.js
    ├── th18/cards_dev.js 起手卡组 + trace
    └── th18.v1.00a.js  ★ 生成物:两个测试断点(起手卡组 / 分配追踪)
```
