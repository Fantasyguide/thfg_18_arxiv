# LESSONS —— 用 thcrap + DLL 改造东方引擎时踩过的坑（card-expand 一线总结）

> **版本**：TH18 v1.00a（`th18.exe`，imagebase `0x400000`）。本文裸地址默认属该版本；引用其他版本须写成 `th16:0x…`。

> 版本无关的方法论；例子全来自 TH18 v1.00a 的 card-expand（2026-09-01 → 09-04，A–E 五条战线 + 行为 SDK）。
> 每条给「现象 → 根因 → 以后怎么做」，一手证据在 [`th18.v1.00a/card-expand/AUDIT.md`](th18.v1.00a/card-expand/AUDIT.md) 的对应小节。
> 动手前的检查单在 [`_template/AUDIT-checklist.md`](_template/AUDIT-checklist.md)；这里是它背后的故事。

## 0. 三句话

1. **引擎不会告诉你它的约定，反汇编会**——每一次「看着像」都要用 `ret N`、调用点的 `push`、优先级注册点去核。
2. **一切假设都要有运行时守卫**——填表后回读、基类虚表比对、JSON 类型检查、全有或全无。静默的部分成功比崩溃更糟。
3. **实跑前一律 🟡**——静态审计过了六条战线，实跑照样抓出三次根因（post_init、`%`、`ret 4`）。

## 1. thcrap 平台

| 现象 | 根因 | 以后 |
| --- | --- | --- |
| DLL 导出的 `*_mod_post_init` 从不被调，日志无一字 | `plugin.cpp` 用 `unordered_map::merge` 并钩子表，thcrap.dll 自己先占了 `post_init` 这个 key，后来者被静默丢弃（2024-11-06 stable 与 master 同）| 要「全部 patch 应用完」的时点，**用自己 patch 里声明的断点**（能触发即证明最后一个 init stage 已应用）。§H′ |
| 日志写进了 `thcrap/bin/` | 注入期间 CWD 是 thcrap/bin | 日志、side-car 一律绝对路径。H14、K13 |
| 断点能盖几条指令？ | `cavesize` 是被搬进 cave 的原字节数，可以跨多条指令，但**不能含相对寻址**（rel8/rel32、RIP 类）；返回 0 = 跳过原指令从 `addr+cavesize` 继续 | 选断点位置时先看后面那条指令是不是 `jmp/call rel`。K6、O1 |
| 想让别的 patch 也能加内容 | `stack_game_json_resolve("x.js")` 深合并栈里每个 patch 的 `th18/x.js` | 数据走 thcrap 栈 JSON；`thcrap.dll` / `jansson.dll` 的函数 `GetProcAddress` 拿，不链接导入库（少一类版本漂移）。`json_t` 只读 `->type`，根类型不对直接 FAIL |
| 两个不同行数的 patch 同时进栈 | 搬表 binhack 只有先到的生效，但分配器上界两边都打上 → 跳转表越界 | 任何 FAIL 都还原分配器上界（`restore_alloc_bound`），全有或全无 |

## 2. ABI：每一个假设都要用字节核

| 现象 | 根因 | 以后 |
| --- | --- | --- |
| **主动卡上场即崩**，`C0000005 … could not be executed at 堆地址`，返回地址在 UpdateFunc 分派器 | `Timer__decrement/increment` 尾是 `ret 4`：thiscall + 一个从不读的栈参（原生调它前 `push ecx`）。按无参调 → 每次多弹 4 字节 → 调用方栈上移 → 下一个 UpdateFunc 的函数指针取成堆地址 | **调引擎函数前看它全部 `ret N` 出口**，再看原生怎么调它（调用点前的 `push`、后面有没有 `add esp`）。O16 的四个看了，O23 这两个只看了头 |
| **按 C 发动即崩**，`could not be executed at 堆地址`，PC = 卡对象地址、`[ESP+8]` = 返回 `ce_sdk_c_press` | `BulletManager__cancel_all` `0x4297a0` 尾是 `ret 4`（thiscall + 一个从不读的栈参，原生 `mov ecx,[mgr]; push 0; call`）；Ghidra 反编译显示 `void(void)`。按无参调 → `on_activate` 的 `ret` 弹到卡对象指针 | **同 O23 第二次栽**：反编译签名不算数，`disassemble_function` 看全部 `ret N`，再看原生调用点前的 `push`。AUDIT O28h′ |
| `pick_weighted_random_offer` 的参数 | fastcall：ecx=out、edx=lo，栈上 hi / exclude / n，`ret 0xc` | 反编译器的原型不可信，看寄存器的首次使用 + `ret N` |
| `play_sound(id)` 有个隐藏参数 | 声像 x 走 xmm2，不在栈上 | 调用点前的 `movss xmm2, …` 就是参数；C 里只能内联汇编（SDK 唯一一处，O22）|
| 战线 D 改 9 处 `[base+idx+K]` 为 `[idx+SHADOW]`，3/9 寄存器留错 | SIB 里「哪个寄存器是存档指针」由编译器随手排，`base` 和 `index` 会互换 | **凡是「编译器把什么放在哪一格」都从上下文重取**，生成器从 `mov r32,[SCOREFILE_PTR]` 反推。K′ |
| `cmp r, imm8` 填 `0xff` 想表示 255 | imm8 符号扩展 = −1，`jl` 永不成立 | 上界进 imm8 就只能 ≤ 127；要更大就换 cave。M3 |
| `code` 与 `expected` 不等长 | thcrap 会记一行日志然后**跳过校验**直接写 | 生成器保证等长、只换常量；对账器把生成物拿回 exe 逐字节比。B3 |

## 3. 对象模型：不要假设布局

| 现象 | 根因 | 以后 |
| --- | --- | --- |
| 原生主动卡对象 0x74 字节、`state` 在 `+0x54`；我们的对象 0x54 | 每个 case 自己 `new` 自己的大小；基类只有 0x54 | 引擎读的字段（flags bit3/bit5、`+0x34` 充能、`+0x48`）都在基类内才敢复用；卡自己的状态放 DLL 侧按对象指针索引的表，销毁槽里释放。O19、§4 |
| ExpHP 的两个 zTimer 名字反了；`GAME_THREAD_PTR` 其实是 `GUI_PTR` | 社区符号是起点不是结论 | 每个用到的名字都用一手读写点核一次（谁递增、谁递减、谁在什么条件下读）。OM §3、O20 |
| `zAbilityText` 第 58 张在对象外；`unlocked_cards[57]` 落在未知区 | 数组尺寸散在十几处硬编码 | 「加一项」不成立时用**重定向 / 影子数组 + side-car**，对象一个字节不扩。§K、§L |
| HUD 往 ANM VM 写 `+0x58/+0x70`，看着像写卡对象 | 偏移撞车：`+0x534` 说明那是 zAnmVm | 判断一个写入的对象类型，看同段代码里的其他偏移（`+0x534` 只可能是 VM）|

## 4. 数据与 id

| 现象 | 根因 | 以后 |
| --- | --- | --- |
| 商店上界抬到 255 会被 ~198 个幻影灌爆 57 槽 | `TableCardData__get` 线性查 `+0x04`，未命中回落 NULL 行；NULL 副本永远不会被返回 | 幻影 = NULL 行本身。把 NULL/BACK 两行 `+0x14` 写成 6，三个循环自然排除，**零机器码**——前提是穷举 `+0x14` 的读者（N1）。而且要放在 `fill_table` 里，任何后续 FAIL 都不影响它（N3）|
| `ability.txt` 解析器可能往新 id 写文案 → 越界 | 它按 `+0x00 internal_name` 全表比对 | 存的名字带 `'\n'` 前缀，永不命中。N4 |
| 装载器拒了自己写的示范卡 | 文案里有 ASCII `%`，会被当 printf 格式串；校验是对的，内容是错的 | 用全角 `％`；错误信息把出错的那一行打出来 |
| 文档说「多数敌人不掉蓝色得点」 | TH18 根本没有得点道具类型：金钱道具同时给钱给分；小分来自弹消 | 设计效果前先枚举**类型分派表**（`ItemManager__on_tick__body` 的 switch），不按别作的常识 |
| 「10% 概率 +2」 | 自带随机会让 replay 失同步 | 确定性计数（每第 10 个）或游戏自己的 RNG；替玩家抽卡直接调商店的 `pick`。O15 |

## 5. 时序：什么时候写才有效

| 现象 | 根因 | 以后 |
| --- | --- | --- |
| 在 `on_tick`（+0x24）里写移速倍率没效果 | Player tick 末尾复位 1.0；`on_tick` 在复位之前 | UpdateFunc **优先级**决定同帧顺序：AbilityManager 0x16 < Player 0x17 < Bomb 0x19 < Bullet 0x1d…；每帧写的量放在「被读之前、被复位之后」的那个 tick 里（`on_tick_2`）。注册点全在 `0x401180` 的调用者里。O7 |
| 想在死亡槽 `+0x14` 里放大复活无敌 | 复活置计时器在那之后 | 找「刚置好」的签名（`{prev 279, cur 280}` 只出现一帧）在下一帧识别。O8 |
| 初始携带的卡 ctor 没被调 | mode 1（存档）/ 3（replay）不调 ctor/dtor；mode 2（购买）先 ctor 再 dtor | 「获得时生效一次」放 `on_load`；即时卡 `deck_visible: 0` 免得进初始卡组变死卡。lifecycle §2–3 |
| 主动卡的 C 键与充能都停了 | 原生门控：不在对话 **且场上有敌人**（`enemy_count_real != 0`）| 这是原生规则，设计时知道就行 |
| 在 ctor 里再调 `allocate_new_card` | 外层还没动 mgr 状态；返回非 0 → 删自己 → 取当前计数返回 | 可以重入；ebx/esi/edi 靠 thiscall 桩按 ABI 保住。O17 |

## 6. 自检与失败策略

- **门里做三件事**：填 → 回读全部写入点 → 一行结论。100 处 binhack 每处都回读，不信 thcrap 的「OK」。
- **FAIL 之后游戏必须仍可玩**：还原分配器上界、商店哨兵权重在填表时就写好、影子数组缺失就放行原指令。
- **布局假设有运行时守卫**：基类虚表 6 个槽与常量比对、JSON 根类型、`derive_rows` 从 patch 反推行数。
- **日志分两档**：`ce_log` 进自己的文件、`ce_verdict` 同时镜像到 thcrap 日志，只镜像结论不刷屏。
- **trace 第一次命中**：每张卡每个槽只记第一次——崩溃日志停在哪一行，就知道最后一个进入的钩子。这次就是靠它定位到 `on_tick_2`。

## 7. 开发流程

- **纯逻辑拆成主机可测的 C**（`cards_def.c`、`sdk_core.c`），Linux 上 `make test-host`；平台侧只能 Windows 实跑。
- **桩与调用约定用 objdump 核**：`this` 在 ecx、`ret 4/8`、fastcall 的 ecx/edx、被调方清栈时调用后没有 `add esp`。
- **内联汇编里压栈的参数一律走寄存器**：`push` 之后再引用 `"m"` 操作数，esp 相对偏移已经错位（青眼 `ce_damage_rect` 第一版的 angle 就压成了垃圾，objdump 抓到；AUDIT O29i）。`"m"` 只用于任何 push 之前的 `movss`。
- **开发配置进 `_test`**：起手卡组直接拿到要测的卡、`trace: true`。别让「测一张卡」依赖商店运气。
- **Makefile 的收尾状态**：`make dist` 曾以 ROWS=58 收尾，把工作区打回 step1——生成物入库的仓库里，构建脚本的最终状态就是提交状态。
- **文档三件套每改必补**：`MAP.md`（这一改动碰了游戏的哪一段）、`AUDIT.md`（claim → CONFIRMED/REFUTED + 证据）、`CARDS.md`（加了哪张卡）。`NEXT.md` 写给没有上下文的下一个会话。
- **表格单元格 ≤ 200 字**（`check-docs.py`）：证据长了就拆成小节，表里只留结论。

## 8. 读崩溃日志的顺序

1. 异常类型：`could not be executed` + 地址在堆 = 跳到了数据（函数指针被改 / 栈返回地址错）；`could not be read/written` = 越界访问。
2. 第一个 `RETURN to` 在游戏 exe 里的地址 = 谁调了坏指针；它前面那条 `Indirect call` 说明是表驱动分派（UpdateFunc / 虚表）。
3. 我们的日志最后一行 trace = 最后进入的钩子；崩溃在它返回之后 ⇒ 优先怀疑**栈平衡**（`ret N`）而不是写越界。
4. 再去看那个钩子里调过的每个引擎函数的 `ret N`。
