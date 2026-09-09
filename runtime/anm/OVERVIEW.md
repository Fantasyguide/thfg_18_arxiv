# anm — ANM v8 播放器实现状态

> **层次**：本文档是 `runtime/` 层，追踪**实现进度**，不是引擎结论。引擎语义的断言 ×
> 版本矩阵在 [`engine/anm/OVERVIEW.md`](../../engine/anm/OVERVIEW.md)；本文任何一行「实现」
> 都必须能指回那边的一条断言，指不回去就在代码里标 🟡（见 [`../README.md`](../README.md) 的
> 双向互指纪律）。

这是一个 ANM v8（TH13+，参见 [`engine/anm/OVERVIEW.md`](../../engine/anm/OVERVIEW.md)）
精灵动画播放器，用「行为对不对得上 th16」反向证伪 `engine/anm/` 的断言。当前处于
**阶段 1：地基**——格式层（I1）、VM 骨架（I2）已实现；opcode 语义里 Math + Jumps 两组
（100–199/200–299，46 条，I3）已实现，其余组（System 8 条、Settings/General/Child
management/Drawing）仍「未开始」。

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
- **变量解析**：目前只实现 I0–I3 / F0–F3（变量号 10000–10007，
  [`engine/anm/th16/03-math-jumps.md`](../../engine/anm/th16/03-math-jumps.md) §3 专项①），
  **支持这 8 个槽内部的跨类型读**（整型指令读浮点槽走 `cvttss2si`，浮点指令读整型槽走
  `cvtdq2ps`——`03-math-jumps.md` §3：`get_int_var`/`get_float_var` 的定义域都是
  10000–10035，真实数据里 opcode 100/112 有 9 次这样跨类型读 F0，不是边角情况）。
  `02-vm-core.md` §3.4 变量表里其余变量号（10008 及以上）留给用到它们的 opcode 组按需扩展。
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
写 I0–I3/F0–F3 这 8 个变量槽（消费 `core.js` 的 `resolveArg` 做右值读；左值写回没有对应的
骨架导出函数，本文件补了一对镜像 `resolveArg` 的写函数，范围同样限定在 I0–I3/F0–F3），
14 条跳转（200–213）只改 `vm.pc`/`vm.time`（消费 `PC_HANDLED`）。

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
     `[0,1)`、`[-1,1)`）都在本文件内从这一个 raw 值推导。不提供 `vm.prng` 时退化到一个
     基于 `Math.random()` 的占位
     实现（非确定性，仅用于烟雾测试）；接 `engine/_shared/math-and-prng.md` 已解出的
     ZUN 算法是调用方的事，不在本组范围内。详见 `group-math.js` 文件头「实现决策②」。
- **未实现的路径**：非变量左值（math-jumps §9 坑表第一条"自修改字节码"）——ZUN 自家
  数据 0 处例外都是变量左值（§8），本组遇到非变量左值直接报错，不实现改写指令流字节。
- **🟡 标注**：124–128（sin/cos/tan/acos/atan）的签名（读 `arg1`、写 `arg0`）是 ✅，
  但"被调 CRT 函数究竟是哪一个"是 `03-math-jumps.md` §2.2 靠数值指纹认定、未经运行时
  验证的结论——本文件对应 5 条 handler 上方注释都标了这条不确定性，选用 JS 对应的
  `Math.sin`/`Math.cos`/`Math.tan`/`Math.acos`/`Math.atan`（+ `Math.fround`）实现。
- 测试：`tests/vm-math.test.js`，夹具 `tests/fixtures/math-ops.anm.txt`（自写、零版权
  字节；brief 给的 5 条测试之外自己加了除零/取模边界、`paramMask` 变量右值、PRNG 可
  注入、★ 跳转 dest/t 下标随 opcode 变的回归测试（200→arg0、201→arg1、202–213→arg2，
  math-jumps §2/§5 点名的坑）、129 角度归一边界值）。细节见
  `.superpowers/sdd/2026-09-09-runtime-anm-phase1/task-I3-report.md`。

## opcode 状态矩阵

下表由 [`tools/gen-opcode-matrix.py`](tools/gen-opcode-matrix.py) 从 thpages 的
`local/vendor/thpages/js/tables/reference/anm.ts` 生成（th16 继承链 `'16'` = `'15'` + 2 条，
135 个 opcode）。**重生成**：`python3 runtime/anm/tools/gen-opcode-matrix.py`。

thpages / th-re-data 是社区单源——「文档」列指向 `engine/anm/th16/` 里一手复核过的小节才算数，
纯抄 thpages 的助记名不算；「可信度」列默认 `—`（未看），community 自己标 `wip: 2`
（Global Constraints 里说的"社区标 wip:2 的 6 条"）的先标 🟡 提醒需要过 `METHOD.md` 的额外闸门。

<!-- gen-opcode-matrix:begin -->

| opcode | 组 | thpages 名 | 文档 | 实现 | 可信度 |
| ---: | --- | --- | --- | --- | --- |
| 0 | System | `nop` | 未开始 | 未开始 | — |
| 1 | System | `delete` | 未开始 | 未开始 | — |
| 2 | System | `static` | 未开始 | 未开始 | — |
| 3 | System | `stop` | 未开始 | 未开始 | — |
| 4 | System | `stop2` | 未开始 | 未开始 | 🟡（社区 wip:2，需过额外闸门） |
| 5 | System | `case` | 未开始 | 未开始 | — |
| 6 | System | `wait` | 未开始 | 未开始 | — |
| 7 | System | `caseReturn` | 未开始 | 未开始 | — |
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
| 300 | Settings | `sprite` | 未开始 | 未开始 | — |
| 301 | Settings | `spriteRand` | 未开始 | 未开始 | — |
| 302 | Settings | `renderMode` | 未开始 | 未开始 | — |
| 303 | Settings | `blendMode` | 未开始 | 未开始 | — |
| 304 | Settings | `layer` | 未开始 | 未开始 | — |
| 305 | Settings | `noZBuffer` | 未开始 | 未开始 | — |
| 306 | Settings | `v8-306` | 未开始 | 未开始 | 🟡（社区 wip:2，需过额外闸门） |
| 307 | Settings | `v8-randMode` | 未开始 | 未开始 | — |
| 308 | Settings | `flipX` | 未开始 | 未开始 | — |
| 309 | Settings | `flipY` | 未开始 | 未开始 | — |
| 310 | Settings | `visible` | 未开始 | 未开始 | — |
| 311 | Settings | `resampleMode` | 未开始 | 未开始 | — |
| 312 | Settings | `scrollMode` | 未开始 | 未开始 | — |
| 313 | Settings | `resolutionMode` | 未开始 | 未开始 | — |
| 314 | Settings | `attached` | 未开始 | 未开始 | — |
| 315 | Settings | `colorizeChildren` | 未开始 | 未开始 | — |
| 316 | Settings | `v8-flag-316` | 未开始 | 未开始 | 🟡（社区 wip:2，需过额外闸门） |
| 317 | Settings | `v8-flag-317` | 未开始 | 未开始 | 🟡（社区 wip:2，需过额外闸门） |
| 400 | General | `pos` | 未开始 | 未开始 | — |
| 401 | General | `rotate` | 未开始 | 未开始 | — |
| 402 | General | `scale` | 未开始 | 未开始 | — |
| 403 | General | `alpha` | 未开始 | 未开始 | — |
| 404 | General | `rgb` | 未开始 | 未开始 | — |
| 405 | General | `alpha2` | 未开始 | 未开始 | — |
| 406 | General | `rgb2` | 未开始 | 未开始 | — |
| 407 | General | `posTime` | 未开始 | 未开始 | — |
| 408 | General | `rgbTime` | 未开始 | 未开始 | — |
| 409 | General | `alphaTime` | 未开始 | 未开始 | — |
| 410 | General | `rotateTime` | 未开始 | 未开始 | — |
| 411 | General | `rotateTime2D` | 未开始 | 未开始 | — |
| 412 | General | `scaleTime` | 未开始 | 未开始 | — |
| 413 | General | `rgb2Time` | 未开始 | 未开始 | — |
| 414 | General | `alpha2Time` | 未开始 | 未开始 | — |
| 415 | General | `angleVel` | 未开始 | 未开始 | — |
| 416 | General | `scaleGrowth` | 未开始 | 未开始 | — |
| 417 | General | `alphaTimeLinear` | 未开始 | 未开始 | — |
| 418 | General | `v8-418` | 未开始 | 未开始 | — |
| 419 | General | `v8-flag-419` | 未开始 | 未开始 | — |
| 420 | General | `posBezier` | 未开始 | 未开始 | — |
| 421 | General | `anchor` | 未开始 | 未开始 | — |
| 422 | General | `posAdopt` | 未开始 | 未开始 | 🟡（社区 wip:2，需过额外闸门） |
| 423 | General | `colorMode` | 未开始 | 未开始 | — |
| 424 | General | `rotateAuto` | 未开始 | 未开始 | — |
| 425 | General | `uVel` | 未开始 | 未开始 | — |
| 426 | General | `vVel` | 未开始 | 未开始 | — |
| 427 | General | `uVelTime` | 未开始 | 未开始 | — |
| 428 | General | `vVelTime` | 未开始 | 未开始 | — |
| 429 | General | `uvScale` | 未开始 | 未开始 | — |
| 430 | General | `uvScaleTime` | 未开始 | 未开始 | — |
| 431 | General | `v8-flag-431` | 未开始 | 未开始 | 🟡（社区 wip:2，需过额外闸门） |
| 432 | General | `slowdown-immune` | 未开始 | 未开始 | — |
| 433 | General | `posTime2D` | 未开始 | 未开始 | — |
| 434 | General | `scale2` | 未开始 | 未开始 | — |
| 435 | General | `scale2Time` | 未开始 | 未开始 | — |
| 436 | General | `anchorOffset` | 未开始 | 未开始 | — |
| 437 | General | `rotationSystem` | 未开始 | 未开始 | — |
| 438 | General | `originMode` | 未开始 | 未开始 | — |
| 500 | Child management | `createChild` | 未开始 | 未开始 | — |
| 501 | Child management | `createChildUi` | 未开始 | 未开始 | — |
| 502 | Child management | `prependChild` | 未开始 | 未开始 | — |
| 503 | Child management | `prependChildUi` | 未开始 | 未开始 | — |
| 504 | Child management | `createRoot` | 未开始 | 未开始 | — |
| 505 | Child management | `createChildPos` | 未开始 | 未开始 | — |
| 506 | Child management | `createRootPos` | 未开始 | 未开始 | — |
| 507 | Child management | `ignoreParent` | 未开始 | 未开始 | — |
| 508 | Child management | `createEffect` | 未开始 | 未开始 | — |
| 509 | Child management | `copyParentVars` | 未开始 | 未开始 | — |
| 600 | Drawing | `textureCircle` | 未开始 | 未开始 | — |
| 601 | Drawing | `textureArcEven` | 未开始 | 未开始 | — |
| 602 | Drawing | `textureArc` | 未开始 | 未开始 | — |
| 603 | Drawing | `drawRect` | 未开始 | 未开始 | — |
| 604 | Drawing | `drawPoly` | 未开始 | 未开始 | — |
| 605 | Drawing | `drawPolyBorder` | 未开始 | 未开始 | — |
| 606 | Drawing | `drawRectGrad` | 未开始 | 未开始 | — |
| 607 | Drawing | `drawRectShadow` | 未开始 | 未开始 | — |
| 608 | Drawing | `drawRectShadowGrad` | 未开始 | 未开始 | — |
| 609 | Drawing | `textureCylinder3D` | 未开始 | 未开始 | — |
| 610 | Drawing | `textureRing3D` | 未开始 | 未开始 | — |
| 611 | Drawing | `drawRing` | 未开始 | 未开始 | — |
| 612 | Drawing | `drawRectBorder` | 未开始 | 未开始 | — |
| 613 | Drawing | `drawLine` | 未开始 | 未开始 | — |

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
    gen-opcode-matrix.py   重生成上面那张矩阵（只重生成「未开始」骨架，已实现的行要手改）
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

阶段 1 的后续任务（波 D 及以后）在 `.superpowers/sdd/2026-09-09-runtime-anm-phase1/` 下的
计划里逐条排；这里不重复列——按 T0 brief 的说法，本文档不该跟踪其他任务的范围。
