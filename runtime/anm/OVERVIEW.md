# anm — ANM v8 播放器实现状态

> **版本**：TH16 v1.00a（`th16.exe`，imagebase `0x400000`）——本文裸地址默认属该版本，
> 引用其他版本须写成 `th18:0x…`。实现以 th16 主攻、th18 补差（见设计文档 D5）。
>
> **层次**：本文档是 `runtime/` 层，追踪**实现进度**，不是引擎结论。引擎语义的断言 ×
> 版本矩阵在 [`engine/anm/OVERVIEW.md`](../../engine/anm/OVERVIEW.md)；本文任何一行「实现」
> 都必须能指回那边的一条断言，指不回去就在代码里标 🟡（见 [`../README.md`](../README.md) 的
> 双向互指纪律）。

这是一个 ANM v8（TH13+，参见 [`engine/anm/OVERVIEW.md`](../../engine/anm/OVERVIEW.md)）
精灵动画播放器，用「行为对不对得上 th16」反向证伪 `engine/anm/` 的断言。当前处于
**阶段 1：地基**——格式层（I1）、VM 骨架（I2）已实现；opcode 语义 **135 / 135 条全部已实现**：
Math + Jumps 46（I3）· System 8（I4）· Settings 18（I5）· General 39（I6）· Child management 10（I7）·
Drawing 14（I8）。**层 A（纯状态机）到此收口**——设计文档 §10.1 的层 A / 层 B 切分里，
剩下的是层 B：帧尾几何填充（`th16:0x4632f0`）+ `draw_vm`（`th16:0x468490`）的 WebGL 后端。

> **「135 条全实现」不等于「画面对得上」**：Drawing 组 14 条自身不发 draw call，只置渲染模式 +
> 写 `sprite_size`/`I0`/顶点缓冲（[`09-drawing.md`](../../engine/anm/th16/09-drawing.md) §0）。
> 真正把这些状态变成像素的是层 B，尚未开始。另外 09 §6 证实 `601`/`607`/`609`/`610`/`611`/`613`
> 这 6 条 ZUN 自家 54 个 `.anm` 一次都没用过——语义 ✅，但**没有 ground truth 可对画面**。

## 格式层：已实现

`src/format/anm-v8.js` 的 `parseAnm(bytes: Uint8Array)` 按
[`engine/anm/th16/01-format.md`](../../engine/anm/th16/01-format.md) §2–§6 的字段表解析
entry 链表 / sprite 表 / script 表 / 指令流，只做结构解析，不含任何 opcode 语义
（VM 语义仍是下面矩阵的事）。

- 差分测试：`tests/format.test.js`，以 `thanm -l` 的输出为 oracle（夹具
  `tests/fixtures/minimal.anm.txt`，自写、零版权字节，`tests/build-fixtures.sh` 用
  `thanm -c 8` 编译）。
- 真实文件冒烟：`local/th16.v1.00a/dat/` 下全部 54 个 `.anm` 解析零异常，entry/script/
  sprite 计数（423 / 1929 / 3101）与 `01-format.md` §10 的一手审计逐字吻合；指令数
  41339 + 每 script 一条被本解析器排除在 `instructions` 之外的 `opcode=-1` 结束标记
  （1929）= 43268，同样吻合该文档的总指令数；其中 `paramMask !== 0` 的指令有
  **2572** 条（下游做变量引用解析的 sanity check 基数）。细节见
  `.superpowers/sdd/2026-09-09-runtime-anm-phase1/task-I1-report.md`。
- **输出契约**：`instructions[i]` = `{ time, opcode, paramMask, args }`。`paramMask`
  是 01-format.md §5.1 的原始 u16，格式层只**透出**、不解释——按位判断某个 `args[i]`
  是不是变量引用，再去查 VM 变量表，是下一步 VM 骨架的事，不是这层的事。可信度分层
  见 `01-format.md` §5.2：bit 0–8 ✅ 一手验证过，bit 9–15 ❓ 从没被测试到（不代表
  保留位，只是没证据）。
- **契约（下游 VM 骨架要遵守）**：`sprite.id`/`script.id` 是文件里存的原始值，不是
  位置序号（01-format.md §4/§5：加载器不读这两个字段）。要跨 entry 累加的运行时索引，
  用 `entries.flatMap(e => e.scripts)`/`entries.flatMap(e => e.sprites)` 的**数组下标**，
  不要用 `id`。详见 `anm-v8.js` 顶部注释。

## 贴图解码（THTX → RGBA8）：已实现

`src/format/thtx.js`（B1）。`parseAnm` 只把 THTX 块的绝对偏移放进 `entry.thtxOffset`，
像素解码归这里；实现了 `thtx.format` 1 / 3 / 5 / 7 四个值，其余**显式抛**。

**真实数据全量跑通**（测试覆盖不到，记在这里）：54 个 th16 `.anm` 的 **417 个带贴图 entry
全部解码成功、89.7 M 像素、零失败**；`thtx.format` 实测分布 `1`×190 · `5`×136 · `3`×52 · `7`×39。

**两条与 thanm 的有意分歧**（`src/format/thtx.js` 文件头有完整论证）：

1. **RGB565 不抄 thanm**。thanm `image.c` 蓝通道三项都写成 `<< 16`、掩码却在 bit18/17/16，
   前两项恒为 0 ⇒ **蓝通道永远到不了 255**（纯白解出 `(255,255,249)`）；它整体还用「复制最低位」
   而非「高位补低位」，**65536 个取值里 65280 个（99.6%）与 GPU 不同**。真机把 RGB565 交给
   D3D9 纹理、**扩位是 GPU 做的**，所以本实现走标准高位复制。`{ rgb565: 'thanm' }` 可切回去对拍。
2. **format 7 默认按 `D3DFMT_A8`**（`rgb = 0`、`a = 源字节`）而不是 thanm 的 `GRAY8`（亮度+不透明）。
   `01-format.md` §7 一手证明 D3DFORMAT 值是 28 = A8。⚠️ 这条分歧在 TH16 **不影响任何真实画面**——
   39 个 format-7 entry 全部是同一个 `face/dummy.png` 占位图。`{ a8: 'luminance' }` 可切。

**一条有意偏离真机**（A 保真度下的取舍）：游戏加载时会把源像素**转成 `entry.format` 的目标格式**
（`th16:0x46cc45`），如 A8R8G8B8 → A4R4G4B4 会掉到 4 位/通道。本实现**只用源精度、不做这步降位**，
因此比真机**更准**。真实数据里两个 format 不同的只有 1 个 entry（`enemy.anm` 的
`enemy/enemy_aura.png`：源 1、目标 6）。

## VM 骨架：已实现

`src/vm/core.js` 实现取指 / 时间推进 / 跳转 / interrupt / 生死，依据
[`engine/anm/th16/02-vm-core.md`](../../engine/anm/th16/02-vm-core.md) §2 的 `AnmVm_run`
伪代码（逐段注释指回对应小节）。**不含任何具体 opcode 语义**——100+/200+/300+/400+/500+/600+
全部通过 `registerGroup(ops, groupTable)` 注入的 `ops` 表间接调用，`core.js` 本身只认
`-1/0/1/2/3/4/5/6/7` 这几个数字（生死/时间控制的骨架特例）。

- **接口**：`createVm(script, opts?) -> vm`、`stepFrame(vm, ops) -> void`、
  `registerGroup(ops, groupTable) -> void`、变量解析 `resolveArg(vm, raw, isVariable, kind)`
  （配套小工具 `isVarBit(paramMask, index)`）、`triggerInterrupt(vm, n) -> void`、
  handler 用来声明「我已经自己改过 pc」的哨兵 `PC_HANDLED`。
- **变量表**：`resolveArg`（读）+ `writeIntVar`/`writeFloatVar`（写）覆盖
  [`engine/anm/th16/04-variables.md`](../../engine/anm/th16/04-variables.md)（下称「04」）
  §2 的全 **36 个变量号**（10000–10035），22 个可写（7 整型 + 15 浮点，04 §3 穷举）/
  14 个只读，逐号落在真实 `vm` 字段（`vm.i`/`vm.f`/`vm.pos`/`vm.rotation`/`vm.i4`/`vm.i5`/
  `vm.randParamOne`/`vm.randParamPi`/`vm.randParamInt`/`vm.mysteryVars`），不是各存一份影子
  状态；只读号写入会抛带变量名的错，类型不匹配的写（比如用整型指令写一个只浮点可写的号）
  也抛错（04 §3：真机会自修改字节码，本骨架不实现这条路径）。
  - **跨类型读支持全 36 号、且是非对称的**（04 §4 一手穷举）：7 个整型存储号
    int→float 走 `cvtdq2ps`，9 个号 float→int 走 `cvttss2si`（截零）；`10022` 单独一条
    公式；**其余 19 个只有浮点语义的号，整型读不算字段值，直接返回变量号本身**——
    这条对“不建模”的号是个好消息：camera/lookat 与 `rot-z-effective` 的整型读根本不需要
    真的碰相机/祖先链，直接落这条默认分支。
  - **随机族两条独立 PRNG 流**（04 §5，承 `03-math-jumps.md` §7.4）：`10010–10012`/`10022`
    走 Stream B，复用 `group-math.js` 122/123/131 已经约定的 `vm.prng` 钩子（同一条物理流，
    不新开名字）；`10030–10032` 走 Stream A，另开 `vm.prngReplay`，与 `vm.prng` 严格独立。
    两个钩子都是必填——没注入时对应变量号直接抛，不静默退化。
  - **`10026`（rot-z-effective）**：04 §6.2 的算法要递归累加祖先旋转，但本骨架没有 VM
    树（`500+` Child management 组还没实现）——每个 `vm` 都等价于该算法 `root == null`
    的分支，所以这里给出的是该分支下**精确**的值（`wrapAngle34Step(vm.rotation.z)`，
    与 129 mathReduceAngle 同一算法），不是近似；等父子关系接进来后要补递归那条分支。
  - **`10016–10021`（camera-x/y/z、lookat-x/y/z）浮点读**：04 §6.1 证实这是舞台相机
    位置/朝向镜像块的全局态，不落在 `vm` 字段里——本骨架显式列进「不建模」清单并抛错，
    不静默返回 0（画面悄悄错位会没人发现，与「PRNG 缺失直接抛」「除零宁可抛」同一哲学）。
  - **`10033–10035`（mystery-angle-x/y/z）**：04 §8（RV2）定论为**通用可写浮点暂存，
    无固定引擎语义**（ZUN 数据里 x/y 常存 2D 偏移、z 常存角度，是用法不是约束）——
    落在 `vm.mysteryVars`，字段名沿用社区叫法，代码注释指回 04 §8 说明这条限定。
  - 细节、逐号映射表、两处故意改坏验证见
    `.superpowers/sdd/2026-09-09-runtime-anm-phase1/task-IV-report.md`。
- **跳转**：骨架只提供「handler 可以直接改写 `vm.pc`（字节偏移），骨架据此不再 `+= size`」
  这套通用机制（`PC_HANDLED`），不认识任何具体跳转 opcode。**这个 VM 里不存在变量跳转
  目标**——`03-math-jumps.md` §2.1/§5 一手证实跳转族（200/201/202–213）的 `dest`/`t` 恒为
  字面量，不查 `param_mask`。
- **分发循环护栏**：`02-vm-core.md` §9 点名要求移植方自己加保护（原版无迭代上限，自跳
  脚本会真死循环，JS 主线程死锁比原版更致命）——`stepFrame` 每帧执行指令数有一个可配置
  上限（`createVm(script, { maxInstructionsPerFrame })`，默认 20 万），超限抛错而不是
  真的锁死。这是骨架里唯一一处**刻意**跟原版行为不同的地方，理由见 `core.js` 文件头。
- **明确不建模**（范围决策，理由见 `core.js` 文件头注释）：GAME_SPEED 全局寄存器与
  slowdown、`on_tick`/`on_wait` 回调、`flags_lo`/`flags_hi`（含 `stop`/`stopHide` 的可见性
  差异）、帧尾几何（速度积分/全局滚动/UV 重算/插值器）、`AnmLoaded`/`AnmManager` 层的模板
  预跑与「创建即 tick」。
- 测试：`tests/vm-core.test.js`，夹具 `tests/fixtures/vm-timing.anm.txt`（自写、零版权
  字节）。细节见 `.superpowers/sdd/2026-09-09-runtime-anm-phase1/task-I2-report.md`。

## Math + Jumps 组（opcode 100–199 / 200–299，46 条）：已实现

`src/vm/group-math.js` 导出 `mathJumpsGroup`（`{ [opcode]: handler }`，供 `registerGroup`
并入总 `ops` 表），依据 [`engine/anm/th16/03-math-jumps.md`](../../engine/anm/th16/03-math-jumps.md)
§2 的 46 条指令表，每条 handler 上方注释指回具体 opcode 行。32 条算术/数学（100–131）
读写变量槽（消费 `core.js` 的 `resolveArg` 做右值读、`writeIntVar`/`writeFloatVar` 做左值写——
两者都覆盖上面「变量表」小节说的全 36 个变量号，不再本地维护一份镜像），14 条跳转
（200–213）只改 `vm.pc`/`vm.time`（消费 `PC_HANDLED`）。

- **两处本文件自己做的实现决策**（math-jumps.md 没有给出唯一答案，属于 `runtime/` 层的
  取舍，不是 engine/ 结论）：
  1. **整数除 0 / 取模 0 / `INT_MIN÷-1`**（108/110/118/120）：真机是裸 `idiv`，无保护，
     触发 x86 `#DE` 崩溃（`03-math-jumps.md` §4.1）。JS 没有等价的"崩溃"——本文件选择
     **抛异常**（不是返回哨兵、不是套用 IEEE 的 `Infinity`/`NaN`），保留"这个位置在真机
     上会崩"这个事实，让调用方显式处理，而不是让一个查不出处的数字悄悄传播下去。
     理由的完整权衡见 `group-math.js` 文件头「实现决策①」。
  2. **PRNG（122/rand、123/randF、131/mathCirclePosRand，`03-math-jumps.md` §7.4 钉死
     这三条走 Stream B 表现流）**：本文件不硬编码 `Math.random()`，而是约定一个可选钩子
     `vm.prng`（`() => number`，每次返回 `[0, 2^32)` 的无符号 32 位整数，对应真机 raw draw
     的返回值，地址见 `03-math-jumps.md` §7.4）——三条指令的三种派生分布（整数取模、
     `[0,1)`、`[-1,1)`）都在本文件内从这一个 raw 值推导。**`vm.prng` 是必填的：没注入时
     122/123/131 直接抛，不静默退化**——初版会悄悄退化到 `Math.random()`，把一个非确定、
     不可复现、与真机无关的数字当成正常结果返回，这与同文件「除零宁可抛」的取舍自相矛盾，
     且退化路径零测试覆盖。只想跑通不在乎数值时显式用本模块导出的 `placeholderPrng`。
     接 `engine/_shared/math-and-prng.md` 已解出的 ZUN 算法是调用方的事，不在本组范围内。
     详见 `group-math.js` 文件头「实现决策②」。
- **未实现的路径**：非变量左值（math-jumps §9 坑表第一条"自修改字节码"）——ZUN 自家
  数据 0 处例外都是变量左值（§8），本组遇到非变量左值直接报错，不实现改写指令流字节。
- **🟡 标注**：124–128（sin/cos/tan/acos/atan）的签名（读 `arg1`、写 `arg0`）是 ✅，
  但"被调 CRT 函数究竟是哪一个"是 `03-math-jumps.md` §2.2 靠数值指纹认定、未经运行时
  验证的结论——本文件对应 5 条 handler 上方注释都标了这条不确定性，选用 JS 对应的
  `Math.sin`/`Math.cos`/`Math.tan`/`Math.acos`/`Math.atan`（+ `Math.fround`）实现。
- ✅ **变量表缺口已由 IV 任务补齐——残余抛出率 3.2%，且全部是刻意的「不建模」抛错，
  不是变量表还没写的号。** I3 交付时的实测是变量表只覆盖 8/22 个可写号，真实 th16
  数据里本组 42.7% 的指令会抛（该数字与失败清单仍见本文件 git 历史/`task-I3-report.md`）。
  IV 任务把 `core.js` 的变量表扩到 `04-variables.md` 的全 36 号后，用同一套方法重跑
  （本仓解析器 + 骨架 + 本组表，对 `local/th16.v1.00a/dat/` 全部 54 个 `.anm` 的本组
  指令逐条调 handler，固定注入 `vm.prng`/`vm.prngReplay`）：

  ```
  本组指令 2525 条 → 正常执行 2445，抛异常 80（3.2%）
    未覆盖读：10016–10021（camera-x/y/z、lookat-x/y/z）各 13–14 次，共 80 次
  1929 个真实 script 里 14 个（0.7%）含至少一条会抛的指令
  ```

  **残余的 80 次抛出全部落在 `10016–10021`**——舞台相机位置/朝向镜像块，04 §6.1 证实
  这是播放器不建模的引擎全局态（不落在 `vm` 字段里，本骨架没有 Stage 全局状态可读），
  是刻意选择抛错、不是变量表遗漏。**这一组现在已经可以拿去跑真实 `.anm`**，除了脚本
  确实用到相机/朝向读数的极少数指令（54 个文件里 14/1929 个 script）。
  细节见 `.superpowers/sdd/2026-09-09-runtime-anm-phase1/task-IV-report.md`。

- 测试：`tests/vm-math.test.js`，夹具 `tests/fixtures/math-ops.anm.txt`（自写、零版权
  字节；brief 给的 5 条测试之外自己加了除零/取模边界、`paramMask` 变量右值、PRNG 可
  注入、★ 跳转 dest/t 下标随 opcode 变的回归测试（200→arg0、201→arg1、202–213→arg2，
  math-jumps §2/§5 点名的坑）、129 角度归一边界值）。细节见
  `.superpowers/sdd/2026-09-09-runtime-anm-phase1/task-I3-report.md`。

## opcode 状态矩阵

下表由 [`tools/gen-opcode-matrix.py`](tools/gen-opcode-matrix.py) 从 thpages 的
`local/vendor/thpages/js/tables/reference/anm.ts` 生成（th16 继承链 `'16'` = `'15'` + 2 条，
135 个 opcode）。

**重生成**：`python3 runtime/anm/tools/gen-opcode-matrix.py`。**默认保留已填行**——
逐 opcode 与本次生成的骨架行比对，只要现有行被人改过（文档/实现/可信度任一列）就
**逐字节原样保留**，只补 thpages 新出现的 opcode、只重写仍是「未开始」的骨架行；
脚本自带的 `preserve_check()` 每次运行都复核这条不变量，保不住就当场退出且不写文件。
要把全表推平成「未开始」必须显式 `--force`（会丢掉所有手填状态）。
`--check` 只解析 + 自检 + 保留检查，不写文件（CI 用）。

thpages / th-re-data 是社区单源——「文档」列指向 `engine/anm/th16/` 里一手复核过的小节才算数，
纯抄 thpages 的助记名不算；「可信度」列默认 `—`（未看），community 自己标 `wip: 2`
（Global Constraints 里说的"社区标 wip:2 的 6 条"）的先标 🟡 提醒需要过 `METHOD.md` 的额外闸门。

<!-- gen-opcode-matrix:begin -->

| opcode | 组 | thpages 名 | 文档 | 实现 | 可信度 |
| ---: | --- | --- | --- | --- | --- |
| 0 | System | `nop` | 已实现 | 已实现 | — |
| 1 | System | `delete` | 已实现 | 已实现 | — |
| 2 | System | `static` | 已实现 | 已实现 | — |
| 3 | System | `stop` | 已实现 | 已实现 | — |
| 4 | System | `stop2` | 已实现 | 已实现 | 🟡（社区 wip:2，需过额外闸门） |
| 5 | System | `case` | 已实现 | 已实现 | — |
| 6 | System | `wait` | 已实现 | 已实现 | — |
| 7 | System | `caseReturn` | 已实现 | 已实现 | — |
| 100 | Math | `set` | ✅ `03-math-jumps.md` §2 | 已实现 `group-math.js` | ✅ |
| 101 | Math | `setF` | ✅ `03-math-jumps.md` §2 | 已实现 `group-math.js` | ✅ |
| 102 | Math | `add` | ✅ `03-math-jumps.md` §2 | 已实现 `group-math.js` | ✅ |
| 103 | Math | `addF` | ✅ `03-math-jumps.md` §2 | 已实现 `group-math.js` | ✅ |
| 104 | Math | `sub` | ✅ `03-math-jumps.md` §2 | 已实现 `group-math.js` | ✅ |
| 105 | Math | `subF` | ✅ `03-math-jumps.md` §2 | 已实现 `group-math.js` | ✅ |
| 106 | Math | `mul` | ✅ `03-math-jumps.md` §2 | 已实现 `group-math.js` | ✅ |
| 107 | Math | `mulF` | ✅ `03-math-jumps.md` §2 | 已实现 `group-math.js` | ✅ |
| 108 | Math | `div` | ✅ `03-math-jumps.md` §2 | 已实现 `group-math.js` | ✅ |
| 109 | Math | `divF` | ✅ `03-math-jumps.md` §2 | 已实现 `group-math.js` | ✅ |
| 110 | Math | `mod` | ✅ `03-math-jumps.md` §2 | 已实现 `group-math.js` | ✅ |
| 111 | Math | `modF` | ✅ `03-math-jumps.md` §2 | 已实现 `group-math.js` | ✅ |
| 112 | Math | `add3` | ✅ `03-math-jumps.md` §2 | 已实现 `group-math.js` | ✅ |
| 113 | Math | `addF3` | ✅ `03-math-jumps.md` §2 | 已实现 `group-math.js` | ✅ |
| 114 | Math | `sub3` | ✅ `03-math-jumps.md` §2 | 已实现 `group-math.js` | ✅ |
| 115 | Math | `subF3` | ✅ `03-math-jumps.md` §2 | 已实现 `group-math.js` | ✅ |
| 116 | Math | `mul3` | ✅ `03-math-jumps.md` §2 | 已实现 `group-math.js` | ✅ |
| 117 | Math | `mulF3` | ✅ `03-math-jumps.md` §2 | 已实现 `group-math.js` | ✅ |
| 118 | Math | `div3` | ✅ `03-math-jumps.md` §2 | 已实现 `group-math.js` | ✅ |
| 119 | Math | `divF3` | ✅ `03-math-jumps.md` §2 | 已实现 `group-math.js` | ✅ |
| 120 | Math | `mod3` | ✅ `03-math-jumps.md` §2 | 已实现 `group-math.js` | ✅ |
| 121 | Math | `modF3` | ✅ `03-math-jumps.md` §2 | 已实现 `group-math.js` | ✅ |
| 122 | Math | `rand` | ✅ `03-math-jumps.md` §2 | 已实现 `group-math.js` | ✅ |
| 123 | Math | `randF` | ✅ `03-math-jumps.md` §2 | 已实现 `group-math.js` | ✅ |
| 124 | Math | `mathSin` | ✅ `03-math-jumps.md` §2 | 已实现 `group-math.js` | 🟡（签名 ✅ / 身份未经运行时验证，`03-math-jumps.md` §2.2） |
| 125 | Math | `mathCos` | ✅ `03-math-jumps.md` §2 | 已实现 `group-math.js` | 🟡（签名 ✅ / 身份未经运行时验证，`03-math-jumps.md` §2.2） |
| 126 | Math | `mathTan` | ✅ `03-math-jumps.md` §2 | 已实现 `group-math.js` | 🟡（签名 ✅ / 身份未经运行时验证，`03-math-jumps.md` §2.2） |
| 127 | Math | `mathAcos` | ✅ `03-math-jumps.md` §2 | 已实现 `group-math.js` | 🟡（签名 ✅ / 身份未经运行时验证，`03-math-jumps.md` §2.2） |
| 128 | Math | `mathAtan` | ✅ `03-math-jumps.md` §2 | 已实现 `group-math.js` | 🟡（签名 ✅ / 身份未经运行时验证，`03-math-jumps.md` §2.2） |
| 129 | Math | `mathReduceAngle` | ✅ `03-math-jumps.md` §2 | 已实现 `group-math.js` | ✅ |
| 130 | Math | `mathCirclePos` | ✅ `03-math-jumps.md` §2 | 已实现 `group-math.js` | ✅ |
| 131 | Math | `mathCirclePosRand` | ✅ `03-math-jumps.md` §2 | 已实现 `group-math.js` | ✅ |
| 200 | Jumps | `jmp` | ✅ `03-math-jumps.md` §2 | 已实现 `group-math.js` | ✅ |
| 201 | Jumps | `jmpDec` | ✅ `03-math-jumps.md` §2 | 已实现 `group-math.js` | ✅ |
| 202 | Jumps | `jmpEq` | ✅ `03-math-jumps.md` §2 | 已实现 `group-math.js` | ✅ |
| 203 | Jumps | `jmpEqF` | ✅ `03-math-jumps.md` §2 | 已实现 `group-math.js` | ✅ |
| 204 | Jumps | `jmpNe` | ✅ `03-math-jumps.md` §2 | 已实现 `group-math.js` | ✅ |
| 205 | Jumps | `jmpNeF` | ✅ `03-math-jumps.md` §2 | 已实现 `group-math.js` | ✅ |
| 206 | Jumps | `jmpLt` | ✅ `03-math-jumps.md` §2 | 已实现 `group-math.js` | ✅ |
| 207 | Jumps | `jmpLtF` | ✅ `03-math-jumps.md` §2 | 已实现 `group-math.js` | ✅ |
| 208 | Jumps | `jmpLe` | ✅ `03-math-jumps.md` §2 | 已实现 `group-math.js` | ✅ |
| 209 | Jumps | `jmpLeF` | ✅ `03-math-jumps.md` §2 | 已实现 `group-math.js` | ✅ |
| 210 | Jumps | `jmpGt` | ✅ `03-math-jumps.md` §2 | 已实现 `group-math.js` | ✅ |
| 211 | Jumps | `jmpGtF` | ✅ `03-math-jumps.md` §2 | 已实现 `group-math.js` | ✅ |
| 212 | Jumps | `jmpGe` | ✅ `03-math-jumps.md` §2 | 已实现 `group-math.js` | ✅ |
| 213 | Jumps | `jmpGeF` | ✅ `03-math-jumps.md` §2 | 已实现 `group-math.js` | ✅ |
| 300 | Settings | `sprite` | 已实现 | 已实现 | — |
| 301 | Settings | `spriteRand` | 已实现 | 已实现 | — |
| 302 | Settings | `renderMode` | 已实现 | 已实现 | — |
| 303 | Settings | `blendMode` | 已实现 | 已实现 | — |
| 304 | Settings | `layer` | 已实现 | 已实现 | — |
| 305 | Settings | `noZBuffer` | 已实现 | 已实现 | — |
| 306 | Settings | `v8-306` | 已实现 | 已实现 | 🟡（社区 wip:2，需过额外闸门） |
| 307 | Settings | `v8-randMode` | 已实现 | 已实现 | — |
| 308 | Settings | `flipX` | 已实现 | 已实现 | — |
| 309 | Settings | `flipY` | 已实现 | 已实现 | — |
| 310 | Settings | `visible` | 已实现 | 已实现 | — |
| 311 | Settings | `resampleMode` | 已实现 | 已实现 | — |
| 312 | Settings | `scrollMode` | 已实现 | 已实现 | — |
| 313 | Settings | `resolutionMode` | 已实现 | 已实现 | — |
| 314 | Settings | `attached` | 已实现 | 已实现 | — |
| 315 | Settings | `colorizeChildren` | 已实现 | 已实现 | — |
| 316 | Settings | `v8-flag-316` | 已实现 | 已实现 | 🟡（社区 wip:2，需过额外闸门） |
| 317 | Settings | `v8-flag-317` | 已实现 | 已实现 | 🟡（社区 wip:2，需过额外闸门） |
| 400 | General | `pos` | 已实现 | 已实现 | — |
| 401 | General | `rotate` | 已实现 | 已实现 | — |
| 402 | General | `scale` | 已实现 | 已实现 | — |
| 403 | General | `alpha` | 已实现 | 已实现 | — |
| 404 | General | `rgb` | 已实现 | 已实现 | — |
| 405 | General | `alpha2` | 已实现 | 已实现 | — |
| 406 | General | `rgb2` | 已实现 | 已实现 | — |
| 407 | General | `posTime` | 已实现 | 已实现 | — |
| 408 | General | `rgbTime` | 已实现 | 已实现 | — |
| 409 | General | `alphaTime` | 已实现 | 已实现 | — |
| 410 | General | `rotateTime` | 已实现 | 已实现 | — |
| 411 | General | `rotateTime2D` | 已实现 | 已实现 | — |
| 412 | General | `scaleTime` | 已实现 | 已实现 | — |
| 413 | General | `rgb2Time` | 已实现 | 已实现 | — |
| 414 | General | `alpha2Time` | 已实现 | 已实现 | — |
| 415 | General | `angleVel` | 已实现 | 已实现 | — |
| 416 | General | `scaleGrowth` | 已实现 | 已实现 | — |
| 417 | General | `alphaTimeLinear` | 已实现 | 已实现 | — |
| 418 | General | `v8-418` | 已实现 | 已实现 | — |
| 419 | General | `v8-flag-419` | 已实现 | 已实现 | — |
| 420 | General | `posBezier` | 已实现 | 已实现 | — |
| 421 | General | `anchor` | 已实现 | 已实现 | — |
| 422 | General | `posAdopt` | 已实现 | 已实现 | 🟡（社区 wip:2，需过额外闸门） |
| 423 | General | `colorMode` | 已实现 | 已实现 | — |
| 424 | General | `rotateAuto` | 已实现 | 已实现 | — |
| 425 | General | `uVel` | 已实现 | 已实现 | — |
| 426 | General | `vVel` | 已实现 | 已实现 | — |
| 427 | General | `uVelTime` | 已实现 | 已实现 | — |
| 428 | General | `vVelTime` | 已实现 | 已实现 | — |
| 429 | General | `uvScale` | 已实现 | 已实现 | — |
| 430 | General | `uvScaleTime` | 已实现 | 已实现 | — |
| 431 | General | `v8-flag-431` | 已实现 | 已实现 | 🟡（社区 wip:2，需过额外闸门） |
| 432 | General | `slowdown-immune` | 已实现 | 已实现 | — |
| 433 | General | `posTime2D` | 已实现 | 已实现 | — |
| 434 | General | `scale2` | 已实现 | 已实现 | — |
| 435 | General | `scale2Time` | 已实现 | 已实现 | — |
| 436 | General | `anchorOffset` | 已实现 | 已实现 | — |
| 437 | General | `rotationSystem` | 已实现 | 已实现 | — |
| 438 | General | `originMode` | 已实现 | 已实现 | — |
| 500 | Child management | `createChild` | 已实现 | 已实现 | — |
| 501 | Child management | `createChildUi` | 已实现 | 已实现 | — |
| 502 | Child management | `prependChild` | 已实现 | 已实现 | — |
| 503 | Child management | `prependChildUi` | 已实现 | 已实现 | — |
| 504 | Child management | `createRoot` | 已实现 | 已实现 | — |
| 505 | Child management | `createChildPos` | 已实现 | 已实现 | — |
| 506 | Child management | `createRootPos` | 已实现 | 已实现 | — |
| 507 | Child management | `ignoreParent` | 已实现 | 已实现 | — |
| 508 | Child management | `createEffect` | 已实现 | 已实现 | 🟡（effect 表项 1 一手，其余表项未逐条追） |
| 509 | Child management | `copyParentVars` | 已实现 | 已实现 | — |
| 600 | Drawing | `textureCircle` | 已实现 | 已实现 | — |
| 601 | Drawing | `textureArcEven` | 已实现 | 已实现 | ZUN 数据 0 次；🟡 弧角展开公式（09 §9，归层 B） |
| 602 | Drawing | `textureArc` | 已实现 | 已实现 | 🟡 弧角展开公式（09 §9，归层 B） |
| 603 | Drawing | `drawRect` | 已实现 | 已实现 | — |
| 604 | Drawing | `drawPoly` | 已实现 | 已实现 | — |
| 605 | Drawing | `drawPolyBorder` | 已实现 | 已实现 | — |
| 606 | Drawing | `drawRectGrad` | 已实现 | 已实现 | — |
| 607 | Drawing | `drawRectShadow` | 已实现 | 已实现 | ZUN 数据 0 次 |
| 608 | Drawing | `drawRectShadowGrad` | 已实现 | 已实现 | — |
| 609 | Drawing | `textureCylinder3D` | 已实现 | 已实现 | ZUN 数据 0 次；🟡 3D 几何公式（09 §9，归层 B） |
| 610 | Drawing | `textureRing3D` | 已实现 | 已实现 | ZUN 数据 0 次；🟡 3D 几何公式（09 §9，归层 B） |
| 611 | Drawing | `drawRing` | 已实现 | 已实现 | ZUN 数据 0 次 |
| 612 | Drawing | `drawRectBorder` | 已实现 | 已实现 | — |
| 613 | Drawing | `drawLine` | 已实现 | 已实现 | ZUN 数据 0 次 |

<!-- gen-opcode-matrix:end -->

> **图例**：✅ 在 th16.exe 上一手验过并已实现 · 🟡 待验/单源或社区自认不确定 ·
> ❌ 已知不同/不支持 · — 未开始

## 目录

```
runtime/anm/
  OVERVIEW.md         本文件
  package.json         零依赖，node --test 跑测试
  src/
    format/
      anm-v8.js         v8 格式解析器（parseAnm），指回 01-format.md §2–§6
    vm/
      core.js           VM 骨架（取指/时间/跳转/interrupt/生死），指回 02-vm-core.md §2
      group-math.js     Math + Jumps 组（opcode 100–199/200–299），指回 03-math-jumps.md §2
  tools/
    gen-opcode-matrix.py   重生成上面那张矩阵（默认保留已填行；--force 才推平）
  tests/
    smoke.test.js       node:test 能跑起来的最小验证
    format.test.js       v8 解析器差分测试，以 thanm -l 为 oracle
    vm-core.test.js      VM 骨架测试
    vm-math.test.js      Math + Jumps 组测试
    build-fixtures.sh    用 thanm -c 8 把 fixtures/*.anm.txt 编译成 .anm
    fixtures/
      minimal.anm.txt      自写夹具源码（入库）
      vm-timing.anm.txt    自写夹具源码（入库），VM 骨架测试用
      math-ops.anm.txt     自写夹具源码（入库），Math + Jumps 组测试用
      *.anm/*.png          build-fixtures.sh 编译产物（gitignored，不入库）
```

## 下一步

层 A 已收口（135/135）。下一步是**层 B：渲染后端**——两块，边界在
[`09-drawing.md`](../../engine/anm/th16/09-drawing.md) §0 划得很清楚：

1. **帧尾几何填充**（`th16:0x4632f0`）：按渲染模式把顶点写进 `vm.vertexBuffer`。
   ★ **2026-09-10：三族的几何公式已全部一手解完并升 ✅**（09 §3.1–§3.5）——mode 9 整圆环、
   mode 13/14 圆环弧（601 居中 / 602 从基准角起，分野是同一段代码里的 `cmp eax,0x1c000000`）、
   mode 24/25 圆柱与平面圆环（共用一段，靠 `F1` 当高还是当宽分形状）。**层 B 的第一块不再有 🟡**。
2. **绘制分发**（`draw_vm` `th16:0x468490`）：mode → handler 的两级跳表 09 §3 已逐字解出；
   render state（blend / alpha test / texture stage / FVF）09 §4 全部 ✅，可直接照着写 WebGL。

其余任务（T10 命名落库、R11 的 th18 差分）在 `.superpowers/sdd/2026-09-09-runtime-anm-phase1/`
下的计划里逐条排；这里不重复列——按 T0 brief 的说法，本文档不该跟踪其他任务的范围。
