// ANM v8 VM ——Math 组（opcode 100–131，32 条）+ Jumps 组（opcode 200–213，14 条）。
//
// 依据：engine/anm/th16/03-math-jumps.md（下称「math-jumps」）§2 的 46 条指令表；每条
// handler 上方注释指回具体 opcode 行（双向互指纪律，见 runtime/README.md）。本文件不
// 重新论证任何语义结论——那是 engine/ 层的事，这里只做「把结论翻成 JS」的实现决策。
//
// 消费 I2 骨架（src/vm/core.js）导出的 registerGroup / resolveArg / isVarBit / PC_HANDLED。
// 变量解析（右值读）全部通过 core.js 的 resolveArg 完成——不在本文件重新实现一份读变量
// 的逻辑（那会制造两份变量表，正是分层要避免的事）。
//
// ⚠️ ── 已知接口缺口：变量表只覆盖 8 / 22 个可写号 ──────────────────────────────
// core.js 的 resolveArg 与本文件的 writeIntVar/writeFloatVar 目前都只认 I0–I3 / F0–F3
// 这 8 个槽（变量号 10000–10007）；02-vm-core.md §3.4 的变量表实际有 22 个可写号，
// 只读号更多。**本文件此处初版写的「其余变量号……这 46 条指令用不到」是事实错误**——
// 它被自己引用的 math-jumps §8 直接证伪：那张真实数据表统计的就是本组指令的参数，
// 里面白纸黑字写着 10008×164、10022×168、10030/10031 各 1。实测（54 个真实 .anm，
// 本组指令逐条调 handler）：
//
//     本组指令 2525 条 → 正常执行 1447，抛异常 1078（42.7%）
//       未覆盖读：10012×341 10022×168 10011×146 10008×81 10010×80 10026×14
//                 10016–10021 各 13–14  10035×7  10033/10034 各 6  10030/10031 各 1
//       未覆盖写：10008×83 10035×48 10028×10 10033×6
//     1929 个真实 script 里 338 个（17.5%）含至少一条会抛的指令
//
// 也就是说：**这 46 条的 opcode 语义是实现完了的，但这一组还不能拿去跑真实 .anm。**
// 补齐变量表要动 core.js 的变量解析、并把 pos/rotation 这些 VM 字段接进来，范围超出
// 本组，是一个独立任务。下面写变量时抛的错保留了这个事实（不静默丢弃、不悄悄写别处），
// 与「实现决策①」的除零处理同一套哲学。
//
// ── 层次边界：本文件是 runtime/ 实现，不是 engine/ 结论 ──────────────────────────
// 下面两处 math-jumps.md 本身没有给出唯一答案、需要本文件自己做取舍，各自在对应 handler
// 或本文件顶部注明依据和理由：
//   1. 整数除 0 / 取模 0 / INT_MIN÷-1 溢出（math-jumps §4.1：真机是裸 idiv，无保护，
//      #DE 崩溃）——JS 里没有等价的“崩溃”，见下面「实现决策①」。
//   2. opcode 122/123/131 用的 PRNG（math-jumps §7.4：三条走 Stream B 表现流，具体是
//      哪个生成器算法属于 engine/_shared/math-and-prng.md 的范围，不是本文件的事）——
//      约定成必填钩子 `vm.prng`，**没注入就抛**（不静默退化到 Math.random），
//      见下面「实现决策②」。

import { resolveArg, isVarBit, PC_HANDLED } from './core.js';

// ── 实现决策① 整数除 0 / 取模 0 / INT_MIN÷-1：抛异常，不返回哨兵 ──────────────────
// math-jumps §4.1 结论④：真机的 108/110/118/120 四条是裸 `cdq;idiv`，前面一条比较、
// 一条条件跳转都没有——`b==0` 与 `INT_MIN/-1`（商溢出）都会触发 x86 #DE，直接让进程
// 异常退出。JS 没有整数除法陷阱，`7/0` 是 `Infinity`、`(-2**31)/-1` 是有限数——如果照抄
// 这个「不崩」的行为，播放器会在真机会崩溃的地方悄悄给出一个错误但看似合理的数字，
// 后续所有依赖这个数的计算全部静默跑偏，且没有任何信号提示哪里出了问题。
// 权衡过三个选项：
//   (a) 静默返回某个哨兵（0？NaN？）——法信息量最少，且没有任何一手证据支持"真机
//       这种情况下的值是什么"（真机是直接崩，没有"这种情况下的值"这回事）。
//   (b) 复刻成 JS 的 `x/0` 语义（Infinity/NaN）——这是浮点除法（109/119）该做的事
//       （math-jumps §4.2：那两条真的是 IEEE，不崩），但套在整数除上是编造了一个真机
//       不存在的行为，会让"这个位置在真机上会崩"这个事实彻底消失。
//   (c) 抛 JS 异常——选这个。理由：这是唯一一种"不会让错误悄悄传播下去"的选择——
//       调用方（未来的播放器主循环）能在这一帧显式捕获，决定是中止这个 VM 实例、
//       跳过这条指令、还是往上抛给宿主；无论选哪种，"这个位置在真机上是崩溃"这个
//       事实都被保留了下来，而不是被一个查不出处的数字悄悄抹掉。
// 代价：mod 作者写出真会踩这个坑的脚本时，JS 播放器的报错行为和真机的进程崩溃行为
// 不是同一种"崩"——但这本来就不可能对齐（JS 异常总能被 try/catch 接住，x86 #DE 不能），
// 选哪个都不是"和真机一样"，(c) 只是"最不会被误解成正常返回值"的那个。
function assertIntDivGuard(divisor, dividend, mnemonic) {
  if (divisor === 0) {
    throw new Error(
      `group-math ${mnemonic}: 整数除 0 / 取模 0 —— 03-math-jumps.md §4.1：真机是裸 idiv，` +
        '没有任何保护，会触发 x86 #DE 崩溃；本实现选择抛异常而不是返回哨兵（见本文件顶部' +
        '「实现决策①」），调用方需要显式处理这种情况。',
    );
  }
  if (dividend === INT32_MIN && divisor === -1) {
    throw new Error(
      `group-math ${mnemonic}: INT_MIN / -1 —— 03-math-jumps.md §4.1：idiv 商溢出同样触发 ` +
        'x86 #DE（这条比除 0 更隐蔽，很容易被漏掉），本实现同上抛异常。',
    );
  }
}
const INT32_MIN = -0x80000000;

// ── 实现决策② PRNG：通过 vm.prng() 可注入，不硬编码 Math.random ──────────────────
// math-jumps §7.4（本文档内部纠错过一次的一节）钉死本组只有 122/rand、123/randF、
// 131/mathCirclePosRand 三条走 Stream B（表现流，`0x4a6d80`）；具体这个流是什么算法，
// engine/_shared/math-and-prng.md 已经算法级解出（ZUN 的 16 位生成器），但那是 engine/
// 层的知识，不是本文件该重新实现的东西——本文件只需要一个"能连续吐 32 位无符号整数"
// 的抽象接口，真正接哪个算法是调用方（未来把这一层接到真 PRNG 或接到确定性测试值）的事。
//
// 约定：`vm.prng`（**必填**，函数，`() => number`）每次调用返回一个 [0, 2^32) 的无符号
// 32 位整数——对应真机 `0x402be0` raw draw 的返回值。122/123/131 用到的三种派生分布
// （[0,2^32) 整数取模、[0,1) 浮点、[-1,1) 浮点）全部在本文件内从这一个 raw 值推导，
// 公式来自 math-jumps §2.2/§7.2 记录的三个 wrapper（`0x402c70`/`0x402cb0`）。
//
// **没注入 `vm.prng` 时直接抛，不静默退化。** 初版是"退化到一个基于 Math.random() 的
// 占位实现"，但**没有任何信号**——一个非确定、不可复现、与真机无关的数字被当成正常结果
// 返回给调用方，之后所有依赖它的计算全部静默跑偏。这与本文件「实现决策①」的除零处理
// 自相矛盾（那里明确拒绝"让一个查不出处的数字悄悄传播下去"），而且退化路径在测试里
// 零覆盖。改成抛异常之后，"忘了接 PRNG"这件事在第一条 122/123/131 上就炸出来。
// 真的只想跑通、不在乎数值的场景（烟雾测试）仍然一行就能满足，但必须**显式**写出来：
//     vm.prng = placeholderPrng;   // 见下面的导出，非确定性、不是 ZUN 的算法
function prngRaw32(vm) {
  if (typeof vm.prng !== 'function') {
    throw new Error(
      'group-math: opcode 122/123/131 需要 PRNG，但 vm.prng 没有注入。' +
        '本组不提供隐式退化（那会把一个与真机无关的随机数当成正常结果悄悄传下去，' +
        '与本文件「实现决策①」的除零处理自相矛盾）。请显式注入：' +
        'vm.prng = () => <[0, 2^32) 的无符号整数>；只想跑通不在乎数值时用本文件导出的 ' +
        'placeholderPrng（非确定性，不是 engine/_shared/math-and-prng.md 里 ZUN 的算法）。',
    );
  }
  return vm.prng(vm) >>> 0;
}

/**
 * 占位 PRNG：非确定性、**不是** ZUN 的算法（真算法见 `engine/_shared/math-and-prng.md`
 * §3），只服务"跑通就行、不在乎具体数值"的场景。必须由调用方显式赋给 `vm.prng`——
 * 本模块不会自己用它兜底（理由见上）。
 */
export function placeholderPrng() {
  return (Math.random() * 0x100000000) >>> 0;
}
function prngUnit(vm) {
  // 0x402c70：u · 2⁻³² ∈ [0, 1)（math-jumps §2.2）。
  return prngRaw32(vm) / 0x100000000;
}
function prngSigned(vm) {
  // 0x402cb0：u · 2⁻³¹ − 1.0 ∈ [-1, 1)（math-jumps §7.2）。
  return prngRaw32(vm) / 0x80000000 - 1;
}

// ── 变量读写的公共小工具 ──────────────────────────────────────────────────────
// 读：直接转发给 core.js 的 resolveArg（不重新实现）。
function readArg(vm, instr, index, kind) {
  return resolveArg(vm, instr.args[index], isVarBit(instr.paramMask, index), kind);
}

// 写：core.js 没有导出对应的"左值"函数（它的 resolveArg 只管右值读），但本组 32 条
// 算术/数学指令里 30 条要写回一个变量槽，所以这里补一对镜像 resolveArg 的写函数。
// 范围与 core.js 的读范围保持同一份依据（02-vm-core.md §3.4 / math-jumps §3），不是
// 本组自创的新范围（也不是完整范围——见顶部「已知接口缺口」）——
// I0–I3 只能被"整型左值指针"写、F0–F3 只能被"浮点左值指针"写，
// math-jumps §3 的两张索引表本身就是分开的（get_int_var_ptr 与 get_float_var_ptr 的
// 可写变量号集合不相交），所以写侧**不需要**读侧那种跨类型转换——这不是遗漏。
function writeIntVar(vm, varId, value) {
  if (varId >= 10000 && varId <= 10003) {
    vm.i[varId - 10000] = value | 0;
    return;
  }
  throw new Error(
    `group-math: 整型变量号 ${varId} 落在尚未实现的变量表范围（本实现只覆盖 I0–I3）` +
      '——02-vm-core.md §3.4 的变量表共 22 个可写号；真实 th16 数据里本组确实会写 10008 等号' +
      '（见本文件顶部「已知接口缺口」的实测分布），不是边角情况。',
  );
}
function writeFloatVar(vm, varId, value) {
  if (varId >= 10004 && varId <= 10007) {
    vm.f[varId - 10004] = Math.fround(value);
    return;
  }
  throw new Error(
    `group-math: 浮点变量号 ${varId} 落在尚未实现的变量表范围（本实现只覆盖 F0–F3）` +
      '——02-vm-core.md §3.4 的变量表共 22 个可写号（含 pos/rotation 分量）；真实 th16 数据里' +
      '本组确实会写 10035/10033 等号（见本文件顶部「已知接口缺口」的实测分布），不是边角情况。',
  );
}

// u32 位模式 -> float32，用来解出"这个左值参数指的是哪个变量号"（浮点指令把变量号也
// 编成 float32——math-jumps §3 结论④）。与 core.js 内部的同名小工具重复 4 行，是刻意
// 的：core.js 没有导出它，本文件也只用它做左值下标解析（读侧已经通过 resolveArg 转发，
// 不会有两份"读变量值"的逻辑并存——这里复制的只是"u32 位重解释成 float32"这个纯数学
// 工具函数，不含任何变量表知识）。
const f32Scratch = new Float32Array(1);
const u32Scratch = new Uint32Array(f32Scratch.buffer);
function u32BitsToFloat32(u32) {
  u32Scratch[0] = u32 >>> 0;
  return f32Scratch[0];
}

// 左值参数在指令流里恒为变量（math-jumps §8：左值参数 0 处例外都是变量）——但那是
// ZUN 自家数据的统计事实，不是架构保证。真机对"非变量左值"的行为是把指令流里那 4 个
// 字节的地址原样退回，写入会改掉脚本本身（math-jumps §9 坑表第一条"自修改字节码"）。
// 本组不实现这条路径（偏最小实现；ZUN 数据 0 处例外，见 §8）——遇到非变量左值直接
// 报错，而不是悄悄改写 instr.args 或悄悄丢弃这次写。
function requireVariableLvalueId(instr, index, kind) {
  if (!isVarBit(instr.paramMask, index)) {
    throw new Error(
      `group-math: opcode ${instr.opcode} 的 arg${index} 是非变量左值（自修改字节码）——` +
        '03-math-jumps.md §9 坑表第一条；ZUN 自家数据 0 处例外（§8），本组未实现这条路径。',
    );
  }
  const raw = instr.args[index];
  return kind === 'float' ? Math.trunc(u32BitsToFloat32(raw)) : raw | 0;
}
function writeIntLvalue(vm, instr, index, value) {
  writeIntVar(vm, requireVariableLvalueId(instr, index, 'int'), value);
}
function writeFloatLvalue(vm, instr, index, value) {
  writeFloatVar(vm, requireVariableLvalueId(instr, index, 'float'), value);
}

// ── 跳转落地：math-jumps §6 的 Timer::set_value 复刻 ──────────────────────────────
// 三处跳转成功路径（200/201 的成立分支/202–213 共用的 0x460b88）都调同一个
// `Timer::set_value(t)`：cur=t；prev=t-1；cur_f=t（不是"保持"也不是"清零"）。
// core.js 内部有一份一模一样的 `timerSetValue`，但它是私有函数、没有导出——I3 的范围
// 是消费 I2 的公开接口，不改 core.js（那是 I2 的既定骨架）。core.js 文件头明确说
// `_timeF`/`_timePrev` 这些前缀 `_` 字段"没有理由对调用方隐藏，只是不保证跨版本稳定"，
// 所以这里直接读写它们，复刻同样的三行赋值，而不是只改 `vm.time`（那样会漏掉
// "跳转会改 previous"这条 math-jumps §9 点名的坑——依赖 previous 的逻辑会在跳转后
// 看到一个人造的 1 帧步长）。
function setScriptTime(vm, t) {
  vm.time = t;
  vm._timePrev = t - 1;
  vm._timeF = t;
}

// ── 12 条条件跳转的公共实现：math-jumps §2.1 已证实它们共用同一个"跳转成功"落点
// （0x460b88），dest/t 的下标（arg2/arg3）与 NaN 行为都是同一套；用工厂函数生成，
// 避免把同一段"读 a、读 b、比较、跳"抄 12 遍。★ dest/t 的下标是本组最容易踩的坑
// （03-math-jumps.md §2/§5 点名）：这十二条都在 arg2/arg3，不是 arg0——跟 200
// （arg0）、201（arg1）都不一样，下标错了会把 149 条不存在的"变量跳转目标"凭空造出来。
function makeCompareJump(kind, cmp) {
  return (vm, args, instr) => {
    const a = readArg(vm, instr, 0, kind);
    const b = readArg(vm, instr, 1, kind);
    if (cmp(a, b)) {
      vm.pc = args[2] | 0; // dest：arg2，02-vm-core.md 用的字节偏移坐标系，直接赋值
      setScriptTime(vm, args[3] | 0); // t：arg3，恒为字面量
      return PC_HANDLED;
    }
    // 不满足条件：不返回 PC_HANDLED，骨架按 size 顺序前进（math-jumps §2 表格「否则顺序执行」）。
  };
}

// ── 129 mathReduceAngle 用到的 float32 常量 ────────────────────────────────────
// math-jumps §7.1：真机常量 `0x494588`=3.14159274（float32 π）、`0x4945b8`=6.28318548
// （float32 2π）。`Math.fround` 给出的是对应双精度数学常量的正确舍入 float32 值，
// 与这两个十进制读数一致（可用 `Math.fround(Math.PI)` 复核=3.1415927410125732，
// 与 exe 常量同一个 float32 位模式）——不是巧合，是"IEEE-754 float32 舍入 π"本来
// 就只有一个正确答案，任何编译器发出的字面量常量都是这同一个值。
const PI32 = Math.fround(Math.PI);
const TWO_PI32 = Math.fround(2 * Math.PI);

// math-jumps §7.1：守卫在算术之后、比的是自增前的值，上限 34 步（不是 33），第一段
// 用满后第二段的 k 不重置（最多再借 1 步）。逐字照抄 §7.1 给出的 C 伪代码结构，
// 包括"先无条件 do 一次、再检查 old>32 是否 break"这个顺序——这正是"34 步"这个数字
// 从哪来的（old 从 0 数到 33，第 34 次循环体做完、old=33>32 才跳出）。
function reduceAngle129(x) {
  let k = 0;
  if (x > PI32) {
    do {
      const old = k;
      x = Math.fround(x - TWO_PI32);
      k++;
      if (old > 32) break;
    } while (x > PI32);
  }
  if (-PI32 > x) {
    do {
      const old = k;
      x = Math.fround(x + TWO_PI32);
      k++;
      if (old > 32) break;
    } while (-PI32 > x);
  }
  return x;
}

export const mathJumpsGroup = {
  // ── 100–111：a op= b（math-jumps §2 表，12 条）────────────────────────────
  // 03-math-jumps.md #100
  100: (vm, args, instr) => {
    writeIntLvalue(vm, instr, 0, readArg(vm, instr, 1, 'int'));
  },
  // 03-math-jumps.md #101
  101: (vm, args, instr) => {
    writeFloatLvalue(vm, instr, 0, readArg(vm, instr, 1, 'float'));
  },
  // 03-math-jumps.md #102
  102: (vm, args, instr) => {
    const a = readArg(vm, instr, 0, 'int');
    const b = readArg(vm, instr, 1, 'int');
    writeIntLvalue(vm, instr, 0, (a + b) | 0);
  },
  // 03-math-jumps.md #103
  103: (vm, args, instr) => {
    const a = readArg(vm, instr, 0, 'float');
    const b = readArg(vm, instr, 1, 'float');
    writeFloatLvalue(vm, instr, 0, a + b);
  },
  // 03-math-jumps.md #104
  104: (vm, args, instr) => {
    const a = readArg(vm, instr, 0, 'int');
    const b = readArg(vm, instr, 1, 'int');
    writeIntLvalue(vm, instr, 0, (a - b) | 0);
  },
  // 03-math-jumps.md #105
  105: (vm, args, instr) => {
    const a = readArg(vm, instr, 0, 'float');
    const b = readArg(vm, instr, 1, 'float');
    writeFloatLvalue(vm, instr, 0, a - b);
  },
  // 03-math-jumps.md #106 —— `imul`，溢出按 32 位截断：用 Math.imul，普通 `*` 在两个
  // int32 相乘时会超出 2^53 精度（最坏 ~4.6e18），必须走 32 位截断乘法。
  106: (vm, args, instr) => {
    const a = readArg(vm, instr, 0, 'int');
    const b = readArg(vm, instr, 1, 'int');
    writeIntLvalue(vm, instr, 0, Math.imul(a, b));
  },
  // 03-math-jumps.md #107
  107: (vm, args, instr) => {
    const a = readArg(vm, instr, 0, 'float');
    const b = readArg(vm, instr, 1, 'float');
    writeFloatLvalue(vm, instr, 0, a * b);
  },
  // 03-math-jumps.md #108 —— `idiv` 截零：`Math.trunc`，不能直接用 `/`（§4.1）。
  // 除 0 / INT_MIN÷-1 见本文件「实现决策①」。
  108: (vm, args, instr) => {
    const a = readArg(vm, instr, 0, 'int');
    const b = readArg(vm, instr, 1, 'int');
    assertIntDivGuard(b, a, '108/div');
    writeIntLvalue(vm, instr, 0, Math.trunc(a / b));
  },
  // 03-math-jumps.md #109 —— `divss`，IEEE，不崩（§4.2）。
  109: (vm, args, instr) => {
    const a = readArg(vm, instr, 0, 'float');
    const b = readArg(vm, instr, 1, 'float');
    writeFloatLvalue(vm, instr, 0, a / b);
  },
  // 03-math-jumps.md #110 —— 余数符号随被除数：JS 的 `%` 与 idiv 语义一致，可以直接用（§4.1）。
  110: (vm, args, instr) => {
    const a = readArg(vm, instr, 0, 'int');
    const b = readArg(vm, instr, 1, 'int');
    assertIntDivGuard(b, a, '110/mod');
    writeIntLvalue(vm, instr, 0, a % b);
  },
  // 03-math-jumps.md #111 —— fmod，双精度算完再回 float（§4.2）；JS 的 `%` 对浮点数的
  // 定义与 C `fmod` 一致（余数符号随被除数），不需要额外转换。
  111: (vm, args, instr) => {
    const a = readArg(vm, instr, 0, 'float');
    const b = readArg(vm, instr, 1, 'float');
    writeFloatLvalue(vm, instr, 0, a % b);
  },

  // ── 112–121：x = a op b（三元式，10 条）───────────────────────────────────
  // 03-math-jumps.md #112
  112: (vm, args, instr) => {
    const a = readArg(vm, instr, 1, 'int');
    const b = readArg(vm, instr, 2, 'int');
    writeIntLvalue(vm, instr, 0, (a + b) | 0);
  },
  // 03-math-jumps.md #113
  113: (vm, args, instr) => {
    const a = readArg(vm, instr, 1, 'float');
    const b = readArg(vm, instr, 2, 'float');
    writeFloatLvalue(vm, instr, 0, a + b);
  },
  // 03-math-jumps.md #114
  114: (vm, args, instr) => {
    const a = readArg(vm, instr, 1, 'int');
    const b = readArg(vm, instr, 2, 'int');
    writeIntLvalue(vm, instr, 0, (a - b) | 0);
  },
  // 03-math-jumps.md #115
  115: (vm, args, instr) => {
    const a = readArg(vm, instr, 1, 'float');
    const b = readArg(vm, instr, 2, 'float');
    writeFloatLvalue(vm, instr, 0, a - b);
  },
  // 03-math-jumps.md #116
  116: (vm, args, instr) => {
    const a = readArg(vm, instr, 1, 'int');
    const b = readArg(vm, instr, 2, 'int');
    writeIntLvalue(vm, instr, 0, Math.imul(a, b));
  },
  // 03-math-jumps.md #117
  117: (vm, args, instr) => {
    const a = readArg(vm, instr, 1, 'float');
    const b = readArg(vm, instr, 2, 'float');
    writeFloatLvalue(vm, instr, 0, a * b);
  },
  // 03-math-jumps.md #118
  118: (vm, args, instr) => {
    const a = readArg(vm, instr, 1, 'int');
    const b = readArg(vm, instr, 2, 'int');
    assertIntDivGuard(b, a, '118/div3');
    writeIntLvalue(vm, instr, 0, Math.trunc(a / b));
  },
  // 03-math-jumps.md #119
  119: (vm, args, instr) => {
    const a = readArg(vm, instr, 1, 'float');
    const b = readArg(vm, instr, 2, 'float');
    writeFloatLvalue(vm, instr, 0, a / b);
  },
  // 03-math-jumps.md #120
  120: (vm, args, instr) => {
    const a = readArg(vm, instr, 1, 'int');
    const b = readArg(vm, instr, 2, 'int');
    assertIntDivGuard(b, a, '120/mod3');
    writeIntLvalue(vm, instr, 0, a % b);
  },
  // 03-math-jumps.md #121 —— ★ 求值顺序是反的：先读 arg2（b）再读 arg1（a）（§9.1）。
  // 本组变量范围（I0–I3/F0–F3）里没有带副作用的 PRNG 变量，顺序在当前测试下不可观察，
  // 但严格照抄文档顺序——将来变量范围扩到 10010–10012/10030–10032 这类"读一次就推进
  // 一次 PRNG"的变量号时，顺序错了会让随机序列跟游戏错位，现在照抄比将来改更省事。
  121: (vm, args, instr) => {
    const b = readArg(vm, instr, 2, 'float');
    const a = readArg(vm, instr, 1, 'float');
    writeFloatLvalue(vm, instr, 0, a % b);
  },

  // ── 122–131：随机 / CRT 数学 / 角度归一 / 极坐标（10 条，社区 label 是裸数字）───
  // 03-math-jumps.md #122 —— `x = (u32)prng() % (u32)n`；n==0 时 x=0（本组唯一一条
  // 对零除做了防护的指令，§4.3）；n<0 按 u32 重解释成一个 ≥2^31 的大数。
  // PRNG 见本文件「实现决策②」。
  122: (vm, args, instr) => {
    const n = readArg(vm, instr, 1, 'int') >>> 0; // (u32)n，§4.3
    const raw = prngRaw32(vm);
    const result = n === 0 ? 0 : raw % n;
    writeIntLvalue(vm, instr, 0, result | 0); // 存回 s32 字段：>2^31-1 的余数按硬件语义环绕成负数
  },
  // 03-math-jumps.md #123 —— `x = randf01() * r`，randf01 ∈ [0,1)。
  123: (vm, args, instr) => {
    const r = readArg(vm, instr, 1, 'float');
    writeFloatLvalue(vm, instr, 0, prngUnit(vm) * r);
  },
  // 03-math-jumps.md #124 —— 签名 ✅ / 身份 🟡（math-jumps §2.2：被调 CRT 函数是靠数值
  // 指纹认定是 sin，没有运行时验证）。cvtss2sd -> 双精度 libm -> cvtsd2ss，JS 用
  // Math.sin（双精度）+ Math.fround 等价（§4.2）。
  124: (vm, args, instr) => {
    const theta = readArg(vm, instr, 1, 'float');
    writeFloatLvalue(vm, instr, 0, Math.fround(Math.sin(theta)));
  },
  // 03-math-jumps.md #125 —— 身份 🟡（同上，指纹认定是 cos）。
  125: (vm, args, instr) => {
    const theta = readArg(vm, instr, 1, 'float');
    writeFloatLvalue(vm, instr, 0, Math.fround(Math.cos(theta)));
  },
  // 03-math-jumps.md #126 —— 身份 🟡（同上，指纹认定是 tan）。
  126: (vm, args, instr) => {
    const theta = readArg(vm, instr, 1, 'float');
    writeFloatLvalue(vm, instr, 0, Math.fround(Math.tan(theta)));
  },
  // 03-math-jumps.md #127 —— 身份 🟡（同上，指纹认定是 acos）；`abs(v)>1 → NaN` 是
  // JS Math.acos 的原生行为，不需要特判。
  127: (vm, args, instr) => {
    const v = readArg(vm, instr, 1, 'float');
    writeFloatLvalue(vm, instr, 0, Math.fround(Math.acos(v)));
  },
  // 03-math-jumps.md #128 —— 身份 🟡（同上，指纹认定是 atan）；`±inf → ±π/2` 是
  // JS Math.atan 的原生行为，不需要特判。
  128: (vm, args, instr) => {
    const v = readArg(vm, instr, 1, 'float');
    writeFloatLvalue(vm, instr, 0, Math.fround(Math.atan(v)));
  },
  // 03-math-jumps.md #129 —— 单参数、原地改写（读 = 写同一个变量）；上限 34 步、
  // 泄漏阈值 ±216.77（= 69π），§7.1（本文档纠正过一次的数字，34 不是 33）。
  129: (vm, args, instr) => {
    const x = readArg(vm, instr, 0, 'float');
    writeFloatLvalue(vm, instr, 0, reduceAngle129(x));
  },
  // 03-math-jumps.md #130 —— ★ 求值顺序是反的：先读 arg3（r）再读 arg2（θ）（§9.1）。
  // `x=cos(θ)*r; y=sin(θ)*r`；真机一次 `fsincos` 出两个值，这里分开调用 Math.cos/
  // Math.sin——只影响性能不影响结果（doc 原话：不影响语义）。
  130: (vm, args, instr) => {
    const r = readArg(vm, instr, 3, 'float');
    const theta = readArg(vm, instr, 2, 'float');
    writeFloatLvalue(vm, instr, 0, Math.fround(Math.cos(theta) * r));
    writeFloatLvalue(vm, instr, 1, Math.fround(Math.sin(theta) * r));
  },
  // 03-math-jumps.md #131 —— 半径区间不是 `[lo,hi]`（社区错，§7.2 裁决）：
  // `r = lo + s2·(hi-lo)`，`s2 ∈ [-1,1)` ⇒ `r ∈ (2·lo-hi, hi)`。角先随机、半径后随机
  // （asm 顺序：0x460738 draw s1 算 ang，0x46074c draw s2 算 r，§2.2 表）。
  // PRNG 见本文件「实现决策②」。
  131: (vm, args, instr) => {
    const ang = prngSigned(vm) * PI32;
    const lo = readArg(vm, instr, 2, 'float');
    const hi = readArg(vm, instr, 3, 'float');
    const r = lo + prngSigned(vm) * (hi - lo);
    writeFloatLvalue(vm, instr, 0, Math.fround(Math.cos(ang) * r));
    writeFloatLvalue(vm, instr, 1, Math.fround(Math.sin(ang) * r));
  },

  // ── 200–213：跳转（14 条）── ★ dest/t 的下标逐族不同，是本组最容易踩的坑 ──────
  // （03-math-jumps.md §2/§5：200 在 arg0、201 在 arg1、202–213 在 arg2；对全部跳转
  // 一律取 args[0] 会凭空多出 149 条不存在的"变量目标"，本文档 §5 专门记录了这次事故）。
  // dest/t 永远是字面量，不查 param_mask（§2.1）——下面 14 条 handler 一律直接
  // `args[i] | 0`，不经过 readArg/resolveArg。

  // 03-math-jumps.md #200 —— dest=arg0，t=arg1。
  200: (vm, args) => {
    vm.pc = args[0] | 0;
    setScriptTime(vm, args[1] | 0);
    return PC_HANDLED;
  },
  // 03-math-jumps.md #201 —— dest=arg1（★ 不是 arg0），t=arg2；`--x`，减完 `x>0` 才跳，
  // x 是唯一一个变量位（arg0，真机始终变量，§8：86/86 恒为 1；本组仍走 requireVariableLvalueId，
  // 遇到非变量左值会显式报错而不是静默按字面量当变量号读）。
  201: (vm, args, instr) => {
    const cur = readArg(vm, instr, 0, 'int') - 1;
    writeIntLvalue(vm, instr, 0, cur);
    if (cur > 0) {
      vm.pc = args[1] | 0;
      setScriptTime(vm, args[2] | 0);
      return PC_HANDLED;
    }
  },
  // 03-math-jumps.md #202 —— jmpEq，int，dest=arg2。
  202: makeCompareJump('int', (a, b) => a === b),
  // 03-math-jumps.md #203 —— jmpEqF，float，dest=arg2；有 NaN → 不跳，JS `===` 对 NaN
  // 天然为 false，不需要特判。
  203: makeCompareJump('float', (a, b) => a === b),
  // 03-math-jumps.md #204 —— jmpNe，int，dest=arg2。
  204: makeCompareJump('int', (a, b) => a !== b),
  // 03-math-jumps.md #205 —— jmpNeF，float，dest=arg2；有 NaN → 跳，JS `!==` 对 NaN
  // 天然为 true（`NaN !== x` 恒真，含 `x` 本身也是 NaN 的情形），不需要特判。
  205: makeCompareJump('float', (a, b) => a !== b),
  // 03-math-jumps.md #206 —— jmpLt，int，dest=arg2。
  206: makeCompareJump('int', (a, b) => a < b),
  // 03-math-jumps.md #207 —— jmpLtF，float，dest=arg2；有 NaN → 不跳，JS `<` 对 NaN
  // 天然为 false，不需要特判。
  207: makeCompareJump('float', (a, b) => a < b),
  // 03-math-jumps.md #208 —— jmpLe，int，dest=arg2。
  208: makeCompareJump('int', (a, b) => a <= b),
  // 03-math-jumps.md #209 —— jmpLeF，float，dest=arg2；有 NaN → 不跳。
  209: makeCompareJump('float', (a, b) => a <= b),
  // 03-math-jumps.md #210 —— jmpGt，int，dest=arg2。
  210: makeCompareJump('int', (a, b) => a > b),
  // 03-math-jumps.md #211 —— jmpGtF，float，dest=arg2；有 NaN → 不跳。
  211: makeCompareJump('float', (a, b) => a > b),
  // 03-math-jumps.md #212 —— jmpGe，int，dest=arg2。
  212: makeCompareJump('int', (a, b) => a >= b),
  // 03-math-jumps.md #213 —— jmpGeF，float，dest=arg2；有 NaN → 不跳。
  213: makeCompareJump('float', (a, b) => a >= b),
};
