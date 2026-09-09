// Math + Jumps 组测试（opcode 100–199 / 200–299，46 条）。
// 依据：engine/anm/th16/03-math-jumps.md（下称 math-jumps）。
//
// 前五条测试逐字来自 task-I3-brief.md 的「步骤 2」，只按任务说明裁定的一处必要修正改了
// `.buffer`（brief 原写法对小文件是 bug：Node 的 Buffer 来自共享内存池，`.buffer` 拿到的
// 是整个池，见 tests/format.test.js / tests/vm-core.test.js 顶部同样的说明）。
// 后面几条是本任务自己加的：除零/取模边界、paramMask 变量参数、PRNG 可注入、
// ★ 跳转下标随 opcode 变的回归测试（math-jumps §2/§5 点名的坑：200→arg0、201→arg1、
// 202–213→arg2，本组之前吃过一次「全部取 args[0]」的亏），以及 129 角度归一的边界值。
import test from 'node:test';
import assert from 'node:assert/strict';
import { readFileSync } from 'node:fs';
import { parseAnm } from '../src/format/anm-v8.js';
import { createVm, stepFrame, registerGroup } from '../src/vm/core.js';
import { mathJumpsGroup, placeholderPrng } from '../src/vm/group-math.js';

const ops = {};
registerGroup(ops, mathJumpsGroup);

function run(fixture, frames) {
  // brief 原文是 `readFileSync(...).buffer`——对小文件是 bug（见文件头说明），改成直接传
  // Buffer；parseAnm 内部用 bytes.buffer/byteOffset/byteLength 三件套处理，Buffer 天生兼容。
  const { entries } = parseAnm(readFileSync(new URL(fixture, import.meta.url)));
  const vm = createVm(entries[0].scripts[0]);
  for (let n = 0; n < frames; n++) stepFrame(vm, ops);
  return vm;
}

// ── brief 步骤 2：现成的五条测试 ──────────────────────────────────────────────

test('整型四则：set/add/sub/mul/div/mod', () => {
  const vm = run('./fixtures/math-ops.anm', 1);
  assert.equal(vm.i[0], 7); // set 3; add 4
  assert.equal(vm.i[1], 6); // set 2; mul 3
  assert.equal(vm.i[2], 1); // set 7; mod 3
});

test('三元式 add3：x = a + b，不改 a/b', () => {
  const vm = run('./fixtures/math-ops.anm', 1);
  assert.equal(vm.i[3], 9);
});

test('浮点槽与整型槽相互独立', () => {
  const vm = run('./fixtures/math-ops.anm', 1);
  assert.ok(Math.abs(vm.f[0] - 1.5) < 1e-6);
  assert.equal(vm.i[0], 7, '写 f0 不该动 i0');
});

test('jmpLt 条件成立时跳转并按表设定 time', () => {
  const vm = run('./fixtures/math-ops.anm', 3);
  assert.equal(vm.i[3], 9, '循环体按 03-math-jumps.md 的跳转语义执行');
  // ★ 上面那句断言不了跳转：i3 在 time 0 段就已经是 9，循环体只碰 I1。
  //   （收口审阅实测：把 14 条跳转 handler 全删掉，只留这一句时测试仍然绿。）
  //   下面这句才是有区分力的部分——夹具 math-ops.anm.txt 的 time 1 段是
  //   `206 jmpLt I1<9 -> loopBody` + `102 add I1,1` + `200 jmp loopHead`：
  assert.equal(
    vm.i[1],
    9,
    '循环必须真的跑完：I1 从 6 每轮 +1 直到 206 jmpLt 的 I1<9 不再成立 ⇒ 终值恰为 9。' +
      '206/200 任何一条不生效，I1 会停在 6（不跳=直接 ins_3 停机）或 7（跳一次就丢了回边）',
  );
  // 「按表设定 time」那一半在这个夹具上测不出来（206/200 的 t 都是 1，与帧尾推进后的
  // 时间同值，删掉 handler 也还是 1）——单独用手工指令测，见下面那条 `t 覆盖 script_time`。
});

test('所有 46 条 opcode 都已注册', () => {
  const codes = Object.keys(mathJumpsGroup).map(Number);
  const inRange = codes.filter((c) => (c >= 100 && c < 200) || (c >= 200 && c < 300));
  assert.equal(inRange.length, 46, '缺一条就说明 03-math-jumps.md 的表没实现完');
});

// ── 手工构造指令：不经 thanm，直接摆 { time, opcode, paramMask, args } ──────────────
// 下面几组边界/回归测试要对指令的字节偏移、paramMask 位做精确控制，用 thanm 文本再算
// 偏移容易手滑；createVm 只要求 `script.instructions` 是这个形状的数组（见 core.js
// 顶部 JSDoc），不需要真的过一遍文件格式层。

function instr(opcode, args, { time = 0, paramMask = 0 } = {}) {
  return { time, opcode, paramMask, args };
}

function mkVm(instructions, opts) {
  return createVm({ instructions }, opts);
}

// 迷你两遍汇编：第一遍算每条指令的字节偏移（label -> offset），第二遍把 `{ ref: label }`
// 占位符替换成算出来的偏移——手写跳转目标最容易在这一步算错，交给代码算比较可靠。
function buildScript(items) {
  const offsets = {};
  let cursor = 0;
  for (const it of items) {
    if (it.label) offsets[it.label] = cursor;
    cursor += 8 + it.args.length * 4;
  }
  const instructions = items.map((it) => ({
    time: it.time ?? 0,
    opcode: it.opcode,
    paramMask: it.paramMask ?? 0,
    args: it.args.map((a) => (a !== null && typeof a === 'object' && 'ref' in a ? offsets[a.ref] : a)),
  }));
  return { instructions };
}

function f32bits(x) {
  const f = new Float32Array([x]);
  return new Uint32Array(f.buffer)[0];
}

// ── 除零 / 取模边界（math-jumps §4.1：裸 idiv，无保护，本组选择抛异常，见
// group-math.js 顶部「实现决策①」）─────────────────────────────────────────────

test('整数除 0：抛异常而不是静默返回哨兵', () => {
  const vm = mkVm([
    instr(100, [10000, 5], { paramMask: 0b01 }), // I0 = 5
    instr(108, [10000, 0], { paramMask: 0b01 }), // I0 /= 0
  ]);
  assert.throws(() => stepFrame(vm, ops), /整数除 0 \/ 取模 0/);
});

test('整数取模 0：抛异常而不是静默返回哨兵', () => {
  const vm = mkVm([
    instr(100, [10000, 5], { paramMask: 0b01 }), // I0 = 5
    instr(110, [10000, 0], { paramMask: 0b01 }), // I0 %= 0
  ]);
  assert.throws(() => stepFrame(vm, ops), /整数除 0 \/ 取模 0/);
});

test('INT_MIN / -1：商溢出同样抛异常（比除 0 更隐蔽的一条）', () => {
  const vm = mkVm([
    instr(100, [10000, -0x80000000], { paramMask: 0b01 }), // I0 = INT_MIN
    instr(108, [10000, -1], { paramMask: 0b01 }), // I0 /= -1
  ]);
  assert.throws(() => stepFrame(vm, ops), /INT_MIN \/ -1/);
});

test('浮点除 0 与取模：IEEE 语义，不抛异常', () => {
  const vm = mkVm([
    instr(101, [f32bits(10004), f32bits(1)], { paramMask: 0b01 }), // F0 = 1.0
    instr(109, [f32bits(10004), f32bits(0)], { paramMask: 0b01 }), // F0 /= 0 -> +Infinity
  ]);
  stepFrame(vm, ops);
  assert.equal(vm.f[0], Infinity);
});

// ── paramMask 变量参数：右值也可以是变量，不只是字面量 ────────────────────────────

test('paramMask 置位的右值参数按变量号解析（add 的加数取自 I1，不是字面量）', () => {
  const vm = mkVm([
    instr(100, [10000, 5], { paramMask: 0b01 }), // I0 = 5
    instr(100, [10001, 3], { paramMask: 0b01 }), // I1 = 3
    instr(102, [10000, 10001], { paramMask: 0b11 }), // I0 += I1（两个参数都是变量）
  ]);
  stepFrame(vm, ops);
  assert.equal(vm.i[0], 8, '若第二个参数被当字面量 10001 读，结果会是 10004 而不是 8');
});

// ── PRNG 可注入（group-math.js「实现决策②」）─────────────────────────────────────

test('122 rand 的 PRNG 可通过 vm.prng 注入，不是硬编码 Math.random', () => {
  const vm = mkVm([instr(122, [10000, 7], { paramMask: 0b01 })]); // x=I0, n=7
  vm.prng = () => 23; // 固定一个确定的 raw draw
  stepFrame(vm, ops);
  assert.equal(vm.i[0], 23 % 7, '结果应完全由注入的 prng 决定（23 % 7 = 2）');
});

test('122 rand：n === 0 短路成 0，不崩（本组唯一有零守卫的指令，§4.3）', () => {
  const vm = mkVm([instr(122, [10000, 0], { paramMask: 0b01 })]);
  vm.prng = () => 0xdeadbeef; // 随便给个非零 raw，验证被短路而不是拿去取模
  stepFrame(vm, ops);
  assert.equal(vm.i[0], 0);
});

test('123 randF 的 PRNG 同样可注入：x = randf01() * r', () => {
  const vm = mkVm([instr(123, [f32bits(10004), f32bits(10)], { paramMask: 0b01 })]); // x=F0, r=10
  vm.prng = () => 0x80000000; // raw / 2^32 = 0.5
  stepFrame(vm, ops);
  assert.ok(Math.abs(vm.f[0] - 5) < 1e-6);
});

// ── ★ 跳转 dest/t 下标随 opcode 变——本组最容易踩的坑的回归测试 ───────────────────
// math-jumps §2/§5：200 的 dest 在 arg0、201 在 arg1、202–213 在 arg2。三个测试都设了
// 一条「毒药」指令（跳转若走错下标就会执行到它，把标记变量写成 999）和一条「落地」指令
// （跳转若走对下标才会执行到它，把标记变量写成 42）——错下标要么落到毒药、要么落到一个
// 不存在的字节偏移（脚本直接终止，标记变量维持初值 0），两种情况断言都会红。

test('跳转下标回归①：200 jmp 的 dest 在 arg0', () => {
  const { instructions } = buildScript([
    { opcode: 200, args: [{ ref: 'landing' }, 0] }, // dest=landing, t=0
    { opcode: 100, paramMask: 0b01, args: [10000, 999] }, // 毒药：跳转走错下标才会执行到
    { label: 'landing', opcode: 100, paramMask: 0b01, args: [10000, 42] },
    { opcode: 3, args: [] },
  ]);
  const vm = mkVm(instructions);
  stepFrame(vm, ops);
  assert.equal(vm.i[0], 42);
});

test('跳转下标回归②：201 jmpDec 的 dest 在 arg1（不是 arg0——arg0 是被减的变量 x）', () => {
  const { instructions } = buildScript([
    { opcode: 100, paramMask: 0b01, args: [10000, 2] }, // x = I0 = 2
    { opcode: 201, paramMask: 0b001, args: [10000, { ref: 'landing' }, 0] }, // --x；x>0 才跳
    { opcode: 100, paramMask: 0b01, args: [10001, 999] }, // 毒药
    { label: 'landing', opcode: 100, paramMask: 0b01, args: [10001, 42] },
    { opcode: 3, args: [] },
  ]);
  const vm = mkVm(instructions);
  stepFrame(vm, ops);
  assert.equal(vm.i[0], 1, '--x：2 减到 1');
  assert.equal(vm.i[1], 42, 'x=1>0，应该跳转（如果错读 arg0=10000 当 dest，pc 会落到一个不存在的偏移，脚本直接终止，i1 会停在初值 0）');
});

test('跳转下标回归③：202–213 一族的 dest 在 arg2（代表：206 jmpLt）', () => {
  const { instructions } = buildScript([
    { opcode: 206, args: [1, 2, { ref: 'landing' }, 0] }, // a=1, b=2, a<b 恒真
    { opcode: 100, paramMask: 0b01, args: [10000, 999] }, // 毒药
    { label: 'landing', opcode: 100, paramMask: 0b01, args: [10000, 42] },
    { opcode: 3, args: [] },
  ]);
  const vm = mkVm(instructions);
  stepFrame(vm, ops);
  assert.equal(vm.i[0], 42, '如果错读 arg0=1 当 dest，pc=1 不落在任何指令边界，脚本直接终止，i0 会停在初值 0');
});

test('跳转不成立：202–213 一族条件不满足时顺序执行，不跳转', () => {
  const { instructions } = buildScript([
    { opcode: 206, args: [2, 1, { ref: 'landing' }, 0] }, // a=2, b=1, a<b 恒假
    { opcode: 100, paramMask: 0b01, args: [10000, 999] }, // 顺序执行应该跑到这条
    { opcode: 3, args: [] },
    { label: 'landing', opcode: 100, paramMask: 0b01, args: [10000, 42] },
  ]);
  const vm = mkVm(instructions);
  stepFrame(vm, ops);
  assert.equal(vm.i[0], 999);
});

test('跳转的 t 覆盖 script_time（math-jumps §6：Timer::set_value(t)，且 previous = t - 1）', () => {
  const { instructions } = buildScript([
    { opcode: 200, args: [{ ref: 'landing' }, 5] }, // time 0：跳 landing，t = 5
    { opcode: 100, paramMask: 0b01, args: [10000, 999] }, // 毒药（顺序执行才会碰到）
    { label: 'landing', time: 5, opcode: 100, paramMask: 0b01, args: [10000, 42] },
    { time: 5, opcode: 3, args: [] },
  ]);
  const vm = mkVm(instructions);
  stepFrame(vm, ops);
  assert.equal(vm.time, 5, 't=5 被直接写进 script_time —— 不是保持 0、也不是取目标指令的 time 标签');
  assert.equal(vm._timePrev, 4, 'Timer::set_value 同时把 previous 设成 t-1（math-jumps §6 结论 3）');
  assert.equal(
    vm.i[0],
    42,
    '时间被拨到 5 之后 landing（time=5）当帧就过闸门执行；跳转不生效的话会先吃到毒药 999、' +
      '而 landing 的 time=5 > 0 会被闸门挡住',
  );
});

// ── 129 mathReduceAngle：上限 34 步 / 泄漏阈值 ±216.77（math-jumps §7.1，刚被修正过
// 的数字——之前的版本写的是 33 步 / ±207.3，本组照修正后的实现，用文档给出的模拟表验）───

test('129 mathReduceAngle：216.7（34 步以内）归约进 [-π, π]', () => {
  const varId = f32bits(10004);
  const vm = mkVm([
    instr(101, [varId, f32bits(216.7)], { paramMask: 0b01 }), // F0 = 216.7
    instr(129, [varId], { paramMask: 0b01 }), // 原地归约
  ]);
  stepFrame(vm, ops);
  assert.ok(Math.abs(vm.f[0] - 3.0716386) < 1e-3, `got ${vm.f[0]}`);
  assert.ok(vm.f[0] >= -Math.PI && vm.f[0] <= Math.PI, '34 步以内应该落进 [-π, π]');
});

test('129 mathReduceAngle：216.77（超过泄漏阈值）归约后仍越界，不报错——原样漏出去', () => {
  const varId = f32bits(10004);
  const vm = mkVm([
    instr(101, [varId, f32bits(216.77)], { paramMask: 0b01 }), // F0 = 216.77
    instr(129, [varId], { paramMask: 0b01 }),
  ]);
  stepFrame(vm, ops);
  assert.ok(Math.abs(vm.f[0] - 3.1416459) < 1e-3, `got ${vm.f[0]}`);
  assert.ok(vm.f[0] > Math.PI, '这是文档 §7.1 表格里明确「刚好越过 π」的那一行，不是 bug');
});

// ── A7 收口修复：defaultPrng 静默退化已改成抛异常（group-math.js「实现决策②」）───────
// 之前没注入 vm.prng 时会悄悄退化到 Math.random()——非确定、不可复现、与真机无关的数字
// 被当成正常结果返回，且这条退化路径零测试覆盖（4 条既有 PRNG 测试全都注入了 vm.prng）。

test('122/123/131 没注入 vm.prng 时抛异常，不静默退化到 Math.random', () => {
  for (const [opcode, args, paramMask] of [
    [122, [10000, 7], 0b01],
    [123, [f32bits(10004), f32bits(1)], 0b01],
    [131, [f32bits(10004), f32bits(10005), f32bits(1), f32bits(2)], 0b0011],
  ]) {
    const vm = mkVm([instr(opcode, args, { paramMask })]);
    assert.throws(
      () => stepFrame(vm, ops),
      /vm\.prng/,
      `opcode ${opcode} 未注入 PRNG 时必须抛，且错误信息要指出缺的是 vm.prng`,
    );
  }
});

test('placeholderPrng 是显式导出的占位实现，注入后 122 不再抛', () => {
  const vm = mkVm([instr(122, [10000, 7], { paramMask: 0b01 })]);
  vm.prng = placeholderPrng;
  stepFrame(vm, ops); // 不抛即通过
  assert.ok(Number.isInteger(vm.i[0]) && vm.i[0] >= 0 && vm.i[0] < 7, `got ${vm.i[0]}`);
});
