// Drawing 组测试（opcode 600–613，14 条）。
// 依据：engine/anm/th16/09-drawing.md（下称「09」）。
//
// 本组是「置状态」组，没有可观察的算术输出，所以断言全部落在**状态位与字段值**上：
// 渲染模式写没写对、有没有碰不该碰的 flags_lo 位、参数落进了哪个槽、缓冲多大。
// 全部手写指令（零版权字节）——09 §6 已经证实 ZUN 自家 54 个 .anm 里本组只出现 8/14 条、
// 90 条指令，靠真实文件覆盖不到另外 6 条，也控制不了 paramMask 的各种组合。
import test from 'node:test';
import assert from 'node:assert/strict';
import { createVm, stepFrame, registerGroup } from '../src/vm/core.js';
import { drawingGroup } from '../src/vm/group-drawing.js';
import { mathJumpsGroup } from '../src/vm/group-math.js';
import { systemGroup } from '../src/vm/group-system.js';
import { settingsGroup } from '../src/vm/group-settings.js';
import { generalGroup } from '../src/vm/group-general.js';
import { childGroup } from '../src/vm/group-child.js';

function instr(opcode, args, { time = 0, paramMask = 0 } = {}) {
  return { time, opcode, paramMask, args };
}
function mkVm() {
  return createVm({ instructions: [instr(0, [])] });
}
function callOp(vm, opcode, args, opts) {
  const ins = instr(opcode, args, opts);
  return drawingGroup[opcode](vm, ins.args, ins);
}
function renderModeOf(vm) {
  return (vm.flagsLo >>> 25) & 0x1f;
}
// 浮点侧的变量号按 03-math-jumps.md §3 ④ 编成 float32 的位模式。
const f32b = new Float32Array(1);
const u32b = new Uint32Array(f32b.buffer);
function f32BitsOf(n) {
  f32b[0] = n;
  return u32b[0];
}
// 指令流里的浮点**字面量**同样是 f32 位模式（01-format.md §5：参数就是 4 字节原始值，
// resolveArg 按 kind 重解释）。所以本文件凡是喂 float 参数的地方都要过 fArg，不能直接写
// 十进制——直接写 640 会被当成 u32 位模式解成 8.97e-43。
const fArg = f32BitsOf;

// 09 §2 的 14 行：opcode → 渲染模式。这张表是本文件的主表，多条测试引用它。
const MODE_TABLE = [
  [600, 9], [601, 13], [602, 14], [603, 16], [604, 17], [605, 18], [606, 20],
  [607, 21], [608, 22], [609, 24], [610, 25], [611, 19], [612, 27], [613, 26],
];
// 每条 opcode 的一组「合法参数」，只为把 handler 跑通（各测试自己再断言细节）。
const SAMPLE_ARGS = {
  600: [4], 601: [4], 602: [4], 609: [4], 610: [4],
  603: [fArg(1), fArg(2)], 606: [fArg(1), fArg(2)], 607: [fArg(1), fArg(2)],
  608: [fArg(1), fArg(2)], 612: [fArg(1), fArg(2)], 613: [fArg(1), fArg(2)],
  604: [fArg(1), 5], 605: [fArg(1), 5],
  611: [fArg(1), fArg(2), 5],
};

test('14 条全注册：drawingGroup 的 key 集合正好是 600–613', () => {
  const keys = Object.keys(drawingGroup).map(Number).sort((a, b) => a - b);
  assert.deepEqual(keys, [600, 601, 602, 603, 604, 605, 606, 607, 608, 609, 610, 611, 612, 613]);
});

// ── 09 §2 主表：渲染模式 ─────────────────────────────────────────────────────────
test('09 §2：14 条各自写对渲染模式（flags_lo bit25–29）', () => {
  for (const [opcode, mode] of MODE_TABLE) {
    const vm = mkVm();
    callOp(vm, opcode, SAMPLE_ARGS[opcode]);
    assert.equal(renderModeOf(vm), mode, `opcode ${opcode} 应写模式 ${mode}`);
  }
});

test('09 §2：模式号对 opcode 不单调——611=19、612=27、613=26，别「排整齐」', () => {
  // 这条不是重复上一条：它守的是「有人觉得表乱了顺手改成递增」这件事。09 §3 的 draw_vm
  // 分发表依赖这个不单调映射——611 靠 mode 19 归进 poly 族（17/18/19），612/613 靠
  // mode 27/26 归进 rect 族（16/20/21/22/26/27）。改成递增会把它们分派到错误的绘制函数。
  const modeOf = (op) => {
    const vm = mkVm();
    callOp(vm, op, SAMPLE_ARGS[op]);
    return renderModeOf(vm);
  };
  assert.equal(modeOf(611), 19, '611 drawRing 归 poly 族（17/18/19）');
  assert.ok(modeOf(611) < modeOf(610), '611 的模式号比 610 的小');
  assert.equal(modeOf(612), 27, '612 drawRectBorder');
  assert.equal(modeOf(613), 26, '613 drawLine');
  assert.ok(modeOf(612) > modeOf(613), '612 的模式号比 613 的大（opcode 却是 612 < 613）');
});

// ── 09 §1.1 的可机器复核判据 ─────────────────────────────────────────────────────
test('09 §1.1：14 条只改 bit25–29，其余 flags_lo 位一位不动', () => {
  // §1.1 的判据是「and 清的位与 or 置的位并集恒为 0x3e000000」。等价的黑盒测法：把
  // flags_lo 全置 1 跑一遍，结果必须精确等于「只把 5 位模式字段换成该 mode」。
  for (const [opcode, mode] of MODE_TABLE) {
    const vm = mkVm();
    vm.flagsLo = 0xffffffff >>> 0;
    callOp(vm, opcode, SAMPLE_ARGS[opcode]);
    const expected = (((0xffffffff & ~0x3e000000) >>> 0) | ((mode << 25) >>> 0)) >>> 0;
    assert.equal(vm.flagsLo >>> 0, expected, `opcode ${opcode}：只有 bit25–29 该变`);
  }
});

test('09 §8：本组不碰 blend mode（bit5–8）与采样地址模式（bit30–31）', () => {
  // 挑 flags_lo 里属于别的组的两块地盘单独盯一遍——上一条用全 1 图案，这条用真实值图案，
  // 万一有人把 mask 写成 0xfe000000（多吃 bit25–31）之类，这条会红得更直白。
  for (const [opcode] of MODE_TABLE) {
    const vm = mkVm();
    vm.flagsLo = ((5 << 5) | (2 << 30)) >>> 0; // blend mode 5（Multiply）+ V 寻址 2（MIRROR）
    callOp(vm, opcode, SAMPLE_ARGS[opcode]);
    assert.equal((vm.flagsLo >>> 5) & 0xf, 5, `opcode ${opcode} 不该动 blend mode（303 的地盘）`);
    assert.equal((vm.flagsLo >>> 30) & 0x3, 2, `opcode ${opcode} 不该动 V 寻址（312 的地盘）`);
  }
});

// ── 09 §5(2) 落点 A：sprite_size ─────────────────────────────────────────────────
test('09 §5(2)：603/606/607/608/612/613 把 arg0/arg1 写进 sprite_size.x/y', () => {
  for (const opcode of [603, 606, 607, 608, 612, 613]) {
    const vm = mkVm();
    callOp(vm, opcode, [fArg(640), fArg(480)]);
    assert.equal(vm.spriteSize.x, 640, `opcode ${opcode}：arg0 → +0x70`);
    assert.equal(vm.spriteSize.y, 480, `opcode ${opcode}：arg1 → +0x74`);
  }
});

test('09 §7 裁决 3：613 drawLine 的 arg1「unused」是渲染层的说法，VM 层照写 +0x74', () => {
  // thpages 写 args:['len','unused']；一手是「case 体与 603 共用尾巴 th16:0x462618，确实写
  // +0x74」，只有 mode 26 的绘制 handler 不读它。把它当 unused 优化掉，VM 状态就与真机不符。
  const vm = mkVm();
  callOp(vm, 613, [fArg(100), fArg(7.5)]);
  assert.equal(vm.spriteSize.x, 100, 'len → sprite_size.x');
  assert.equal(vm.spriteSize.y, 7.5, '★ arg1 照写 sprite_size.y，不是丢弃');
});

test('09 §5(2)：sprite_size 是 f32 槽（`movss` 落 +0x70/+0x74），写入按 float32 截断', () => {
  // ★ 这条必须走**变量**路径才有区分力：字面量参数本来就是 f32 位模式，resolveArg 解出来
  // 已经是 f32，那条路上 Math.fround 是 no-op（第一版测试写成喂字面量 + notEqual，故意
  // 拆掉 fround 时 0 条红——假验证，已改）。而 vm.f[] 是 JS number，别的组完全可能往里
  // 存一个非 f32 可表示的 double，此时「写进 f32 槽要截断」才真正发生。
  //
  // 三个写 f32 槽的地方各覆盖一次（09 §2.1 的落点 A、落点 B，以及 611 的自写路径），
  // 少覆盖哪个，拆掉那处的 fround 就会 0 条红。
  const D = 0.1; // 双精度 0.1，f32 里表示不出来
  const cases = [
    ['落点 A / 603 的 x', 603, [fArg(10004), fArg(1)], 0x1, 'x'],
    ['落点 A / 603 的 y', 603, [fArg(1), fArg(10004)], 0x2, 'y'],
    ['落点 B / 604 的 x', 604, [fArg(10004), 5], 0x1, 'x'],
    ['611 自写的 x', 611, [fArg(10004), fArg(1), 5], 0x1, 'x'],
    ['611 自写的 y', 611, [fArg(1), fArg(10004), 5], 0x2, 'y'],
  ];
  for (const [label, opcode, args, paramMask, axis] of cases) {
    const vm = mkVm();
    vm.f[0] = D; // 变量号 10004 = F0
    callOp(vm, opcode, args, { paramMask });
    assert.equal(vm.spriteSize[axis], Math.fround(D), `${label}：经 movss 落 f32 槽`);
    assert.notEqual(vm.spriteSize[axis], D, `${label}：拆掉 fround 这条就会红`);
  }
});

test('09 §8：sprite_size 只有一份——300 与 603 写的是同一对槽', () => {
  // 09 §8 第五条坑。骨架里 sprite_size 挂在 core.js 的 vm 契约上（不是 group-drawing 的
  // 私有影子状态），所以「先 drawRect 再被别人覆盖」这种序列能如实反映出来。
  const vm = mkVm();
  const slot = vm.spriteSize;
  callOp(vm, 603, [fArg(8), fArg(8)]);
  callOp(vm, 606, [fArg(640), fArg(480)]);
  assert.equal(vm.spriteSize, slot, '两次写都落在同一个对象上，没有被换成新对象');
  assert.deepEqual({ x: vm.spriteSize.x, y: vm.spriteSize.y }, { x: 640, y: 480 }, '后写的赢');
});

// ── 09 §5(3) 落点 B：radius + I0 ─────────────────────────────────────────────────
test('09 §5(3)：604/605 的 arg0(f)→sprite_size.x、arg1(int)→I0', () => {
  for (const opcode of [604, 605]) {
    const vm = mkVm();
    callOp(vm, opcode, [fArg(64), 12]);
    assert.equal(vm.spriteSize.x, 64, `opcode ${opcode}：radius → +0x70`);
    assert.equal(vm.i[0], 12, `opcode ${opcode}：nInit → +0x4a0（I0）`);
    assert.equal(vm.spriteSize.y, 0, `opcode ${opcode} 不写 +0x74（默认仍是 0）`);
  }
});

test('09 §2/§5(3)：611 drawRing 三参数全自写（r→x、t→y、n→I0）', () => {
  // 09 §2.1：611 不复用任何别的尾巴，是本组唯一的三参数指令。
  const vm = mkVm();
  callOp(vm, 611, [fArg(64), fArg(4), 32]);
  assert.equal(vm.spriteSize.x, 64, 'radius → +0x70');
  assert.equal(vm.spriteSize.y, 4, 'thickness → +0x74');
  assert.equal(vm.i[0], 32, 'nInit → +0x4a0（I0）');
});

// ── 09 §2.2 落点 C：顶点缓冲分配 ─────────────────────────────────────────────────
test('09 §2.2：600/601/602 分配 56×nmax，609/610 分配 48×nmax', () => {
  for (const [opcode, per] of [[600, 56], [601, 56], [602, 56], [609, 48], [610, 48]]) {
    const vm = mkVm();
    assert.equal(vm.vertexBuffer, null, '分配前 +0x5b8 是空的');
    callOp(vm, opcode, [10]);
    assert.equal(vm.vertexBufferSize, 10 * per, `opcode ${opcode}：+0x5bc = ${per}×nmax`);
    assert.equal(vm.vertexBuffer.byteLength, 10 * per, `opcode ${opcode}：缓冲真的按这个大小建出来`);
  }
});

test('09 §2.2：重复分配直接覆盖 +0x5b8，不复用旧缓冲（0x4064e0 没有 free）', () => {
  const vm = mkVm();
  callOp(vm, 600, [10]);
  const first = vm.vertexBuffer;
  callOp(vm, 600, [20]);
  assert.notEqual(vm.vertexBuffer, first, '是新缓冲，不是原地改大小');
  assert.equal(vm.vertexBufferSize, 20 * 56);
});

test('09 §6：600 的 nmax 真实数据 12/12 是变量 $I0，param_mask 路径必须通', () => {
  // 09 §6 证伪过「600 的 nmax 是字面量」这个假设：ZUN 数据里 12/12 全是被 mask 标成的
  // 变量 10000。若 handler 不走 resolveArg 而是直接取字面量，这里会分配 10000×56 字节。
  const vm = mkVm();
  vm.i[0] = 7;
  callOp(vm, 600, [10000], { paramMask: 0x1 });
  assert.equal(vm.vertexBufferSize, 7 * 56, 'nmax 取的是 I0 的值 7，不是字面量 10000');
});

test('09 §2.2：nmax 为负直接抛，不静默当 0（真机是 operator new 天文数字）', () => {
  const vm = mkVm();
  assert.throws(() => callOp(vm, 600, [-1]), /nmax=-1 < 0/);
  assert.equal(vm.vertexBuffer, null, '抛出后不该留下半个缓冲');
  assert.equal(renderModeOf(vm), 9, '★ 但渲染模式已经写进去了——09 §1.1：置 flags_lo 在解析参数之前');
});

test('09 §2.2：nmax=0 是合法的（operator new(0) 返回有效指针），不抛', () => {
  const vm = mkVm();
  callOp(vm, 600, [0]);
  assert.equal(vm.vertexBufferSize, 0);
  assert.equal(vm.vertexBuffer.byteLength, 0);
});

// ── 09 §2.1：求值次序 ────────────────────────────────────────────────────────────
test('09 §2.1：604 先读完 arg1 再写 I0——arg1 自己是 $I0 时不能读到刚写的值', () => {
  // 左值解析恒在右值读取之后。若实现写成「先把 I0 清掉再读」或「边读边写」，这条会红。
  const vm = mkVm();
  vm.i[0] = 33;
  callOp(vm, 604, [fArg(64), 10000], { paramMask: 0x2 }); // arg1 是变量 $I0
  assert.equal(vm.i[0], 33, '读到的是写之前的 33，回写后仍是 33');
});

test('09 §2.1：611 三个参数按下标升序读，arg2 写 I0 不影响 arg0/arg1 的读', () => {
  const vm = mkVm();
  vm.i[0] = 6;
  vm.f[0] = 12.5; // F0 = 变量号 10004
  callOp(vm, 611, [fArg(10004), fArg(4), 10000], { paramMask: 0x1 | 0x4 });
  assert.equal(vm.spriteSize.x, 12.5, 'arg0 是变量 $F0 → 12.5');
  assert.equal(vm.spriteSize.y, 4, 'arg1 字面量');
  assert.equal(vm.i[0], 6, 'arg2 是变量 $I0，读到写前的 6');
});

// ── 09 §0/§8 第一条坑：本组不发 draw call ────────────────────────────────────────
test('09 §0：本组只置状态——不碰 pos/scale/rotation/color，也不填顶点', () => {
  // 守「有人顺手在 drawRect 里把 scale 乘进去」这类越权：09 §8 第二条坑明确 ×scale 发生在
  // 绘制层（th16:0x468722），不在本组。
  for (const [opcode] of MODE_TABLE) {
    const vm = mkVm();
    callOp(vm, opcode, SAMPLE_ARGS[opcode]);
    assert.deepEqual(vm.scale, { x: 1, y: 1 }, `opcode ${opcode} 不该动 scale`);
    assert.deepEqual(vm.pos, { x: 0, y: 0, z: 0 }, `opcode ${opcode} 不该动 pos`);
    assert.deepEqual(vm.rotation, { x: 0, y: 0, z: 0 }, `opcode ${opcode} 不该动 rotation`);
    assert.equal(vm.color, 0xffffffff, `opcode ${opcode} 不该动 color`);
    assert.equal(vm.time, 0, `opcode ${opcode} 不该动 time`);
  }
});

test('09 §3 要点 1：600/601/602 在本层只有模式号之差（几何分支在帧尾，不在本组）', () => {
  const results = [600, 601, 602].map((op) => {
    const vm = mkVm();
    callOp(vm, op, [10]);
    return { mode: renderModeOf(vm), size: vm.vertexBufferSize };
  });
  assert.deepEqual(results.map((r) => r.mode), [9, 13, 14], '三个模式号各不相同');
  assert.deepEqual(results.map((r) => r.size), [560, 560, 560], '缓冲大小完全一致（同族同 stride）');
});

// ── 与骨架分发循环的集成 ─────────────────────────────────────────────────────────
test('集成：走 stepFrame 真实分发，handler 返回 undefined ⇒ 骨架按 size 推进 pc', () => {
  // 上面所有用例都直接调 handler（要拿返回值/精确控参）；这条补上「经 core.js 的分发循环
  // 跑」这一层，防止 handler 签名与骨架约定脱节（比如误返回 PC_HANDLED 导致 pc 不推进）。
  const ops = {};
  registerGroup(ops, drawingGroup);
  const vm = createVm({
    instructions: [
      instr(603, [fArg(640), fArg(480)]),
      instr(604, [fArg(64), 12]),
      instr(600, [10000], { paramMask: 0x1 }), // nmax = $I0，此时 I0 已被 604 写成 12
      instr(0, [], { time: 1 }), // 留一条未来指令，免得跑到脚本尾隐式 delete
    ],
  });
  stepFrame(vm, ops);

  assert.equal(vm.spriteSize.x, 64, '604 后写，赢过 603');
  assert.equal(vm.i[0], 12, '604 写的 I0');
  assert.equal(renderModeOf(vm), 9, '最后执行的是 600 → 模式 9');
  assert.equal(vm.vertexBufferSize, 12 * 56, '600 的 nmax 读到 604 刚写的 I0 = 12');
  assert.ok(vm.alive, '三条都是普通指令，不该终止脚本');
});

test('层 A 收口：六个组一起注册正好 135 个 opcode，且互不冲突', () => {
  // registerGroup 遇到重复的 opcode 号会抛，所以这条同时证明了「六个组的号段互不重叠」。
  // 135 = Math+Jumps 46 · System 8 · Settings 18 · General 39 · Child 10 · Drawing 14。
  const ops = {};
  registerGroup(ops, mathJumpsGroup);
  registerGroup(ops, systemGroup);
  registerGroup(ops, settingsGroup);
  registerGroup(ops, generalGroup);
  registerGroup(ops, childGroup);
  registerGroup(ops, drawingGroup);
  assert.equal(Object.keys(ops).length, 135, '层 A 的 135 条全部有 handler');
  const counts = [
    [mathJumpsGroup, 46], [systemGroup, 8], [settingsGroup, 18],
    [generalGroup, 39], [childGroup, 10], [drawingGroup, 14],
  ];
  assert.equal(counts.reduce((n, [g]) => n + Object.keys(g).length, 0), 135);
  for (const [g, n] of counts) assert.equal(Object.keys(g).length, n);
});
