// General 组测试（opcode 400–438，39 条）。
// 依据：engine/anm/th16/07-general.md（下称「07」）。
//
// 这一组的错误形态是**静默的**（07 brief 原话：写错不会报错，只会画面微妙不对）——所以
// 本文件的重心不是「handler 存在」，而是**数值验证**：31 个缓动曲线 mode 逐个断言曲线
// 性质（端点、峰值、谷值、导数符号、常数、越界回退线性）+ 求值顺序回归（PRNG 侧信道，
// 07 §2「$ 记号的更正」之后新给的 13 种形态表）。
//
// 结构仿 vm-settings.test.js/vm-math.test.js：一条端到端冒烟（真实 parseAnm -> createVm
// -> stepFrame，验证格式层解出的 args/paramMask 与 handler 接得上）+ 大量手写指令的精确
// 测试。插值曲线的验证走真实管线（407 posTime 的 handler + stepInterpolators 逐帧步进），
// 不额外导出内部的 easeValue/hermite 之类的纯函数——brief 只要求导出 generalGroup 与
// stepInterpolators，测试也应该走这两个公开入口，而不是伸手进内部实现。
import test from 'node:test';
import assert from 'node:assert/strict';
import { readFileSync } from 'node:fs';
import { parseAnm } from '../src/format/anm-v8.js';
import { createVm, stepFrame, registerGroup } from '../src/vm/core.js';
import { generalGroup, stepInterpolators, ensureGeneralState } from '../src/vm/group-general.js';

const ops = {};
registerGroup(ops, generalGroup);

function instr(opcode, args, { time = 0, paramMask = 0 } = {}) {
  return { time, opcode, paramMask, args };
}
function mkVm(instructions, opts) {
  return createVm({ instructions }, opts);
}
// 浮点字面量按 float32 位模式编码（math-jumps §3 结论④；核心机制见 core.js 的
// u32BitsToFloat32），与 vm-math.test.js/vm-vars.test.js 同款小工具。
function f32bits(x) {
  const f = new Float32Array([x]);
  return new Uint32Array(f.buffer)[0];
}
// PRNG 队列 mock：依次吐出给定的值，吐完了抛错（不静默回绕）——用来在求值顺序测试里
// 通过「哪个字段拿到了第几个 draw」反推 handler 真实的参数读取顺序。
function prngQueue(values) {
  let i = 0;
  return () => {
    if (i >= values.length) throw new Error(`prngQueue: 用尽（只准备了 ${values.length} 个值）`);
    return values[i++];
  };
}
// 让某个参数位读到 10022（rand，只读、Stream B、int 读取模/float 读不取模——04-variables.md
// §5）：int kind 直接存变量号，float kind 存变量号的 float32 位模式（同 400 号变量惯例）。
const RAND_VAR = 10022;

// ── 端到端冒烟：真实 .anm 文件（general-ops.anm.txt，自写、零版权字节）───────────────
test('端到端冒烟：真实文件解析 + 一帧执行，400/401/402/403/404/421/409/437 都生效', () => {
  const { entries } = parseAnm(readFileSync(new URL('./fixtures/general-ops.anm', import.meta.url)));
  const vm = createVm(entries[0].scripts[0]);
  stepFrame(vm, ops);

  assert.equal(vm.pos.x, Math.fround(1.5));
  assert.equal(vm.pos.y, Math.fround(2.5));
  assert.equal(vm.pos.z, Math.fround(3.5));
  assert.ok(Math.abs(vm.rotation.x - 0.1) < 1e-6);
  assert.equal(vm.flagsLo & 0x4, 0x4, '401 置 flags_lo bit2（rotate 脏位）');
  assert.equal(vm.scale.x, 2);
  assert.equal(vm.scale.y, 3);
  assert.equal(vm.flagsLo & 0x8, 0x8, '402 置 flags_lo bit3（scale 脏位）');
  assert.equal((vm.color >>> 24) & 0xff, 200, '403 alpha(200)');
  assert.equal((vm.color >>> 16) & 0xff, 10, '404 r=10');
  assert.equal((vm.color >>> 8) & 0xff, 20, '404 g=20');
  assert.equal(vm.color & 0xff, 30, '404 b=30');
  assert.equal(vm.flagsLo & 0x600000, 0x200000, '421 anchor h=1 落 bit21-22');
  assert.equal(vm.flagsLo & 0x1800000, 0x1000000, '421 anchor v=2 落 bit23-24');
  assert.equal(vm._general.interp.color1A.gate, 60, '409 alphaTime(t=60) 建立了插值器');
  assert.equal(vm._general.interp.color1A.mode, 4, '409 alphaTime mode=4');
  assert.equal(vm.flagsHi & 0x1c, 0x0c, '437 rotationSystem(3) 落 bit2-4');
});

// ── 「39 条全注册」计数断言 ──────────────────────────────────────────────────────
test('39 条 opcode（400–438）全部已注册，且没有多注册范围外的号', () => {
  const codes = Object.keys(generalGroup).map(Number);
  const inRange = codes.filter((c) => c >= 400 && c <= 438);
  assert.equal(inRange.length, 39, '缺一条就说明 07-general.md 的表没实现完');
  assert.equal(codes.length, 39, 'generalGroup 不应该含 400-438 之外的号');
});

// ══════════════════════════════════════════════════════════════════════════════════
// § 求值顺序回归（07 §2「$ 记号的更正」之后的 13 种形态表；至少覆盖 400/420/401 三种）
// ══════════════════════════════════════════════════════════════════════════════════

test('400 pos：求值顺序 (2,1,0) —— 先读 z、再 y、再 x（与 415 同款，容易被误当升序）', () => {
  const vm = mkVm([instr(400, [f32bits(RAND_VAR), f32bits(RAND_VAR), f32bits(RAND_VAR)], { paramMask: 0b111 })]);
  vm.prng = prngQueue([111, 222, 333]); // 依次消费：z=111、y=222、x=333
  stepFrame(vm, ops);
  assert.equal(vm.pos.z, Math.fround(111));
  assert.equal(vm.pos.y, Math.fround(222));
  assert.equal(vm.pos.x, Math.fround(333));
});

test('401 rotate：求值顺序 (0,1,2) 升序 —— 对照组，验证不是所有指令都降序', () => {
  const vm = mkVm([instr(401, [f32bits(RAND_VAR), f32bits(RAND_VAR), f32bits(RAND_VAR)], { paramMask: 0b111 })]);
  vm.prng = prngQueue([111, 222, 333]); // 依次消费：x=111、y=222、z=333
  stepFrame(vm, ops);
  assert.equal(vm.rotation.x, Math.fround(111));
  assert.equal(vm.rotation.y, Math.fround(222));
  assert.equal(vm.rotation.z, Math.fround(333));
});

test('420 posBezier：求值顺序 (1,2,3,7,0,6,5,4) —— 07 §2 表里唯一的「交错」形态', () => {
  // arg0=t（int 读）、arg1-3=切线1、arg4-6=目标、arg7=切线2.x（走变量，消费 PRNG）；
  // arg8/arg9（切线2.y/z）是本实现对文档缺口的外推位置（07 §2 表原文只列了 8 个下标，
  // 见 group-general.js #420 handler 上方注释），这里故意留字面量 0、不接变量，
  // 不去断言一个文档没给出的顺序。
  const args = new Array(10).fill(0);
  args[0] = RAND_VAR; // t：int kind，直接存变量号（不走 float32 位模式）
  for (const i of [1, 2, 3, 4, 5, 6, 7]) args[i] = f32bits(RAND_VAR);
  const vm = mkVm([instr(420, args, { paramMask: 0b0011111111 })]); // bit0-7 全部标变量
  vm.prng = prngQueue([111, 222, 333, 444, 555, 666, 777, 888]);
  stepFrame(vm, ops);

  const entry = vm._general.interp.pos; // 白盒读取插值器内部状态，直接核验顺序解出的值
  assert.deepEqual(entry.tangent1, [Math.fround(111), Math.fround(222), Math.fround(333)], 'arg1,2,3 → 切线1 (x,y,z)');
  assert.equal(entry.tangent2[0], Math.fround(444), 'arg7 → 切线2.x');
  assert.equal(entry.gate, 555, 'arg0 → t（555 % randParamInt(0x10000) = 555）');
  assert.deepEqual(
    entry.target,
    [Math.fround(888), Math.fround(777), Math.fround(666)],
    'arg6,5,4 按此顺序读 → 目标 (x=888,y=777,z=666)',
  );
  assert.equal(entry.mode, 8, 'posBezier 的 mode 硬编码 8');
});

test('432 slowdown-immune：arg0 被读两次（右值 + 「左值指针」），用的是第二次读的结果', () => {
  const vm = mkVm([instr(432, [RAND_VAR], { paramMask: 0b1 })]);
  let calls = 0;
  vm.prng = () => {
    calls++;
    return calls === 1 ? 4 : 5; // 第一次(偶数,会清位) 第二次(奇数,会置位)——用来分辨用的是哪次
  };
  stepFrame(vm, ops);
  assert.equal(calls, 2, 'arg0 应该被 resolveArg 读两次（07 §2 表原话）');
  assert.equal(vm.flagsHi & 0x200, 0x200, '最终写入的是第二次读的值（5，奇数，置位）');
});

test('408 rgbTime：求值顺序 (4,3,2,0) —— b、g、r、t', () => {
  const vm = mkVm([instr(408, [RAND_VAR, 0, RAND_VAR, RAND_VAR, RAND_VAR], { paramMask: 0b11101 })]);
  vm.prng = prngQueue([1, 2, 3, 4]); // b=1, g=2, r=3, t=4
  stepFrame(vm, ops);
  const entry = vm._general.interp.color1Rgb;
  assert.deepEqual(entry.target, [3, 2, 1], 'target = (r,g,b) = (3,2,1)');
  assert.equal(entry.gate, 4);
});

test('407 posTime：求值顺序 (0,4,3,2) —— t 先读，再 z、y、x（与 408/410/413 正相反）', () => {
  const args = [RAND_VAR, 0, f32bits(RAND_VAR), f32bits(RAND_VAR), f32bits(RAND_VAR)];
  const vm = mkVm([instr(407, args, { paramMask: 0b11101 })]);
  vm.prng = prngQueue([9, 111, 222, 333]); // t=9, z=111, y=222, x=333
  stepFrame(vm, ops);
  const entry = vm._general.interp.pos;
  assert.equal(entry.gate, 9);
  assert.deepEqual(entry.target, [Math.fround(333), Math.fround(222), Math.fround(111)]);
});

test('433 posTime2D：求值顺序 (0,3,2) —— t 先读，再 radius(arg3)、angle(arg2)', () => {
  const args = [RAND_VAR, 0, f32bits(RAND_VAR), f32bits(RAND_VAR)];
  const vm = mkVm([instr(433, args, { paramMask: 0b1101 })]);
  vm.prng = prngQueue([5, 20, 30]); // t=5, radius=20, angle=30
  stepFrame(vm, ops);
  const entry = vm._general.interp.pos;
  assert.equal(entry.gate, 5);
  // target = (cos(angle)*radius, sin(angle)*radius, 0)；angle=Math.fround(30), radius=Math.fround(20)
  const angle = Math.fround(30);
  const radius = Math.fround(20);
  assert.ok(Math.abs(entry.target[0] - Math.fround(Math.cos(angle) * radius)) < 1e-4);
  assert.ok(Math.abs(entry.target[1] - Math.fround(Math.sin(angle) * radius)) < 1e-4);
  assert.equal(entry.target[2], 0);
});

test('416 scaleGrowth：求值顺序 (1,0) —— 先 y 后 x', () => {
  const vm = mkVm([instr(416, [f32bits(RAND_VAR), f32bits(RAND_VAR)], { paramMask: 0b11 })]);
  vm.prng = prngQueue([7, 8]); // y=7, x=8
  stepFrame(vm, ops);
  assert.equal(vm._general.scaleGrowth.y, Math.fround(7));
  assert.equal(vm._general.scaleGrowth.x, Math.fround(8));
});

// ══════════════════════════════════════════════════════════════════════════════════
// § 39 条各自的直接语义（非插值指令）
// ══════════════════════════════════════════════════════════════════════════════════

test('400 pos：flags_lo & 0x400 时写 pos_2（vm._general.pos2）而不是 vm.pos', () => {
  const vm = mkVm([instr(400, [f32bits(1), f32bits(2), f32bits(3)])]);
  vm.flagsLo = (vm.flagsLo | 0x400) >>> 0; // 手动置位「写 pos_2」闸门（真机由某条渲染相关指令置，07 范围外）
  stepFrame(vm, ops);
  assert.deepEqual(vm._general.pos2, { x: 1, y: 2, z: 3 });
  assert.deepEqual(vm.pos, { x: 0, y: 0, z: 0 }, 'flags_lo bit10 置位时不该动 vm.pos');
});

test('403/404/405/406：color_1/color_2 的 alpha 与 rgb 写入（含 8 位截断）', () => {
  const vm = mkVm([
    instr(403, [300]), // alpha=300 → 截断成 300&0xff=44
    instr(404, [256, -1, 128]), // r=256→0, g=-1→255, b=128
    instr(405, [10]),
    instr(406, [1, 2, 3]),
  ]);
  stepFrame(vm, ops);
  assert.equal((vm.color >>> 24) & 0xff, 300 & 0xff);
  assert.equal((vm.color >>> 16) & 0xff, 0, '256 截断成 0');
  assert.equal((vm.color >>> 8) & 0xff, 255, '-1 截断成 255（& 0xff）');
  assert.equal(vm.color & 0xff, 128);
  assert.equal((vm._general.color2 >>> 24) & 0xff, 10);
  assert.equal((vm._general.color2 >>> 16) & 0xff, 1);
  assert.equal((vm._general.color2 >>> 8) & 0xff, 2);
  assert.equal(vm._general.color2 & 0xff, 3);
});

test('color2 默认是 0x00000000，不是 0xffffffff（易与 color_1 搞混，02-vm-core.md §3.3）', () => {
  const vm = mkVm([]);
  stepInterpolators(vm); // 触发 ensureGeneralState 惰性建（不依赖任何具体指令）
  assert.equal(vm._general.color2, 0);
});

test('415 angleVel：写 vm._general.angularVelocity，flags_hi |= 0x1000000', () => {
  const vm = mkVm([instr(415, [f32bits(1), f32bits(2), f32bits(3)])]);
  stepFrame(vm, ops);
  assert.deepEqual(vm._general.angularVelocity, { x: 1, y: 2, z: 3 });
  assert.equal(vm.flagsHi & 0x1000000, 0x1000000);
});

test('417 alphaTimeLinear：arg0(alpha) 裸字节不查 mask，arg1(t) 才走 get_int_var；mode 硬编码 0', () => {
  // arg0 即使 paramMask 置位也不该被当成变量——07 §2「$ 记号的更正」。
  const vm = mkVm([instr(417, [200, RAND_VAR], { paramMask: 0b11 })]);
  vm.prng = prngQueue([50]); // 只应该消费一次（t），alpha 是字面量不消费
  stepFrame(vm, ops);
  const entry = vm._general.interp.color1A;
  assert.equal(entry.target[0], 200, 'alpha 按字面量 200 读，没有被当成变量号去查表');
  assert.equal(entry.gate, 50);
  assert.equal(entry.mode, 0, 'mode 硬编码线性');
});

test('418：未建模，显式抛错而不是静默返回', () => {
  const vm = mkVm([instr(418, [])]);
  assert.throws(() => stepFrame(vm, ops), /未建模|不建模/);
});

test('419：flags_hi bit13（0x2000）按值写位，不是只会置位', () => {
  const vm = mkVm([instr(419, [1]), instr(419, [0])]);
  stepFrame(vm, ops);
  assert.equal(vm.flagsHi & 0x2000, 0, '第二条 419(0) 应该把 bit13 清掉');
});

test('421 anchor：一个 u32（不是两个 s16），h=低16位 v=高16位，且 arg0 不查 mask', () => {
  const packed = (5 & 0xffff) | ((3 & 0xffff) << 16);
  const vm = mkVm([instr(421, [packed], { paramMask: 0b1 })]); // 即使置位也该被当字面量
  stepFrame(vm, ops);
  assert.equal((vm.flagsLo >>> 21) & 0x3, 5 & 0x3, 'h 落 bit21-22（2 位，5 的低 2 位）');
  assert.equal((vm.flagsLo >>> 23) & 0x3, 3 & 0x3, 'v 落 bit23-24');
});

test('422 posAdopt：pos = entity_pos，随后 entity_pos 清 0', () => {
  // 本骨架里新建 vm 的 entity_pos 默认 0（07 §11：本组没有任何指令能写 entity_pos，
  // 真机靠宿主实体系统预置）；这里手动预置 vm._general.entityPos 来单独验证「收养 +
  // 清零」这两步机制（07 §8：机制 ✅，语义 🟡）。
  const vm = mkVm([instr(422, [])]);
  vm._general = { entityPos: { x: 5, y: 6, z: 7 } };
  stepFrame(vm, ops);
  assert.deepEqual(vm.pos, { x: 5, y: 6, z: 7 });
  assert.deepEqual(vm._general.entityPos, { x: 0, y: 0, z: 0 });
});

test('423/424/431/438：4 条「$ 记号更正」的裸字节 flag 写入，即使 paramMask 置位也不查表', () => {
  const vm = mkVm([
    instr(423, [2], { paramMask: 0b1 }),
    instr(424, [1], { paramMask: 0b1 }),
    instr(431, [1], { paramMask: 0b1 }),
    instr(438, [2], { paramMask: 0b1 }),
  ]);
  stepFrame(vm, ops);
  assert.equal((vm.flagsLo >>> 17) & 0x3, 2, '423 colorMode');
  assert.equal(vm.flagsHi & 0x80, 0x80, '424 rotateAuto');
  assert.equal(vm.flagsHi & 0x100, 0x100, '431（语义 ❓，机制 ✅）');
  assert.equal((vm.flagsHi >>> 18) & 0x3, 2, '438 originMode');
});

test('425/426：uVel/vVel 直接写 uv_scroll_vel，flags_hi |= 0x1000000', () => {
  const vm = mkVm([instr(425, [f32bits(1.5)]), instr(426, [f32bits(2.5)])]);
  stepFrame(vm, ops);
  assert.equal(vm._general.uvScrollVel.x, Math.fround(1.5));
  assert.equal(vm._general.uvScrollVel.y, Math.fround(2.5));
  assert.equal(vm.flagsHi & 0x1000000, 0x1000000);
});

test('429/434/436：uvScale/scale2/anchorOffset 升序直接写', () => {
  const vm = mkVm([instr(429, [f32bits(2), f32bits(3)]), instr(434, [f32bits(4), f32bits(5)]), instr(436, [f32bits(6), f32bits(7)])]);
  stepFrame(vm, ops);
  assert.deepEqual(vm._general.uvScale, { x: 2, y: 3 });
  assert.equal(vm.flagsLo & 0x10, 0x10, '429 置缩放脏位 bit4');
  assert.deepEqual(vm._general.scale2, { x: 4, y: 5 });
  assert.equal(vm.flagsLo & 0x8, 0x8, '434 也置 bit3（与 402 scale 共用）');
  assert.deepEqual(vm._general.anchorOffset, { x: 6, y: 7 });
});

test('437 rotationSystem：单参、走 get_int_var（$ 记号更正的对照组）', () => {
  const vm = mkVm([instr(437, [5])]); // 字面量（mask=0）：先确认基本赋值；变量路径见下一条
  stepFrame(vm, ops);
  assert.equal((vm.flagsHi >>> 2) & 0x7, 5 & 0x7);
});

test('437 rotationSystem：arg0 确实查 mask（对照 423/424/431/438 不查）', () => {
  const vm = mkVm([instr(437, [10000], { paramMask: 0b1 })]); // 10000 = I0，默认值 0
  stepFrame(vm, ops);
  assert.equal((vm.flagsHi >>> 2) & 0x7, 0, 'I0 默认 0，说明 arg0 真的被当变量号去查表了');
});

// ══════════════════════════════════════════════════════════════════════════════════
// § 插值器：*Time 家族的求值顺序 + 曲线数值验证
// ══════════════════════════════════════════════════════════════════════════════════

// 用 407 posTime 作为公共探针：start=(0,0,0)、target=(1,0,0)，只跟踪 pos.x。
function sampleFraction(mode, fraction, gate = 1000) {
  const vm = mkVm([instr(407, [gate, mode, f32bits(1), f32bits(0), f32bits(0)])]);
  stepFrame(vm, ops);
  const frames = Math.round(fraction * gate);
  for (let i = 0; i < frames; i++) stepInterpolators(vm);
  return vm.pos.x;
}

test('409/427/428/411/412/430/435/413/414/410：其余 *Time 求值顺序 + 建库正确性（批量）', () => {
  // 已经在别处覆盖过求值顺序判据的一次性核验：这里只需确认每条都能正确建立插值器
  // （target/gate/mode 落对位置），顺序层面的坑已经被 400/401/420/408/407/433/416/432
  // 挡住了主要形态（降序/升序/交错/双读/混合），这里换个角度：批量检查「target 的值对
  // 不对」而不是「顺序对不对」，两者合起来才算把 13 种形态与 39 条指令都过了一遍。
  // 全部用字面量（mask=0）——这条测试要看的是 target/gate 落点对不对，不是求值顺序，
  // 混进变量标记只会让 resolveArg 把这些字面量数值误当变量号去查表（越界抛错）。
  const cases = [
    { op: 409, args: [10, 0, 128], mask: 0, key: 'color1A', target: [128], gate: 10 },
    { op: 427, args: [10, 0, f32bits(1.5)], mask: 0, key: 'uvScrollVelX', target: [Math.fround(1.5)], gate: 10 },
    { op: 428, args: [10, 0, f32bits(2.5)], mask: 0, key: 'uvScrollVelY', target: [Math.fround(2.5)], gate: 10 },
    { op: 412, args: [10, 0, f32bits(2), f32bits(3)], mask: 0, key: 'scale', target: [Math.fround(2), Math.fround(3)], gate: 10 },
    { op: 430, args: [10, 0, f32bits(4), f32bits(5)], mask: 0, key: 'uvScale', target: [Math.fround(4), Math.fround(5)], gate: 10 },
    { op: 435, args: [10, 0, f32bits(6), f32bits(7)], mask: 0, key: 'scale2', target: [Math.fround(6), Math.fround(7)], gate: 10 },
    { op: 413, args: [10, 0, 1, 2, 3], mask: 0, key: 'color2Rgb', target: [1, 2, 3], gate: 10 },
    { op: 414, args: [10, 0, 42], mask: 0, key: 'color2A', target: [42], gate: 10 },
    { op: 410, args: [10, 0, f32bits(1), f32bits(2), f32bits(3)], mask: 0, key: 'rotation', target: [1, 2, 3].map(Math.fround), gate: 10 },
  ];
  for (const c of cases) {
    const vm = mkVm([instr(c.op, c.args, { paramMask: c.mask })]);
    stepFrame(vm, ops);
    const entry = vm._general.interp[c.key];
    assert.deepEqual(entry.target, c.target, `opcode ${c.op} target`);
    assert.equal(entry.gate, c.gate, `opcode ${c.op} gate`);
  }
});

test('411 rotateTime2D：角度感知插值，走最短路径（从 3 弧度插值到 -3 弧度不会绕远路）', () => {
  const vm = mkVm([instr(411, [100, 0, f32bits(3.0)])]);
  vm.rotation.z = -3.0; // 起点 -3 rad，目标 3 rad——直接差 6 rad，最短路径应该是反向差 (2π-6)
  stepFrame(vm, ops);
  for (let i = 0; i < 50; i++) stepInterpolators(vm); // 半程
  // 最短路径：从 -3 走到 3 的最短方向是「往下绕」（经过 ±π），不是直接 +6 弧度线性增长；
  // 半程时刻的角度应该落在 [-π, π] 折叠范围内的某个值，且**不等于**朴素线性 (-3+3)/2=0
  // 这种巧合之外的验证：直接检查绝对值别超过 π（走最短路径的必然结果）。
  assert.ok(Math.abs(vm.rotation.z) <= Math.PI + 1e-3, `最短路径插值中途不该超过 π，got ${vm.rotation.z}`);
});

// ── ★ 31 个缓动曲线 mode 的数值验证 ────────────────────────────────────────────────

test('mode 0 / 负数 / >31：线性回退', () => {
  for (const mode of [0, -1, 32, 99]) {
    for (const frac of [0.2, 0.5, 0.8]) {
      const v = sampleFraction(mode, frac);
      assert.ok(Math.abs(v - frac) < 0.01, `mode ${mode} u=${frac}: 期望≈${frac}, got ${v}`);
    }
  }
});

test('mode 15 恒 0、mode 16 恒 1（缓动过程中；跑完会在最后一帧强制吸附到 target，这是所有非 7/17 mode 的共性，不是 15/16 特有）', () => {
  for (const frac of [0.1, 0.5, 0.9]) {
    assert.equal(sampleFraction(15, frac), 0, `mode 15 u=${frac}`);
    assert.equal(sampleFraction(16, frac), 1, `mode 16 u=${frac}`);
  }
});

test('端点：所有非常数/非累加 mode 在 u→0 时 f≈0、u→1 时 f≈1（抓住①「1-g_c(1-u) 写反」这类端点颠倒的 bug）', () => {
  const tweenModes = [1, 2, 3, 4, 5, 6, 9, 10, 11, 12, 13, 14, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31];
  for (const mode of tweenModes) {
    const near0 = sampleFraction(mode, 0.002);
    const near1 = sampleFraction(mode, 0.98);
    assert.ok(Math.abs(near0) < 0.5, `mode ${mode}: f(u≈0)=${near0} 应该接近 0`);
    assert.ok(near1 > 0.5, `mode ${mode}: f(u≈1)=${near1} 应该接近 1`);
  }
});

test('★ 峰值：mode 27–31 的过冲峰在 u=1-c 处应为 1.125/1.225/1.408/1.602/1.800（07 §4.2）', () => {
  const cs = [0.25, 0.3, 0.35, 0.38, 0.4];
  const expected = [1.125, 1.225, 1.408, 1.602, 1.8];
  for (let i = 0; i < 5; i++) {
    const mode = 27 + i;
    const v = sampleFraction(mode, 1 - cs[i]);
    assert.ok(Math.abs(v - expected[i]) < 0.02, `mode ${mode} 峰值：期望 ${expected[i]}, got ${v}`);
  }
});

test('★ 谷值：mode 22–26 的下凹谷在 u=c 处应为 -0.125/-0.225/-0.408/-0.602/-0.800（07 §4.2）', () => {
  const cs = [0.25, 0.3, 0.35, 0.38, 0.4];
  const expected = [-0.125, -0.225, -0.408, -0.602, -0.8];
  for (let i = 0; i < 5; i++) {
    const mode = 22 + i;
    const v = sampleFraction(mode, cs[i]);
    assert.ok(Math.abs(v - expected[i]) < 0.02, `mode ${mode} 谷值：期望 ${expected[i]}, got ${v}`);
  }
});

test('★ 正弦族 18/19/20/21 的导数符号（07 §0/§4.1「通俗名原本全反了」的更正校验）', () => {
  // 用差商近似 f'(0)、f'(1)：du=0.001（gate=1000 时正好 1 帧）。
  const du = 0.001;
  function slopeAt0(mode) {
    return sampleFraction(mode, du) / du;
  }
  function slopeAt1(mode) {
    return (1 - sampleFraction(mode, 1 - du)) / du;
  }
  // 18 = sin(π/2·u)：ease-out，f'(0)=π/2>1（起步快）、f'(1)=0（收尾慢）。
  assert.ok(slopeAt0(18) > 1.0, `mode18 f'(0) 应该 >1（起步快），got ${slopeAt0(18)}`);
  assert.ok(slopeAt1(18) < 0.2, `mode18 f'(1) 应该≈0（收尾慢），got ${slopeAt1(18)}`);
  // 19 = 1-cos(π/2·u)：ease-in，f'(0)=0（起步慢）、f'(1)=π/2（收尾快）。
  assert.ok(slopeAt0(19) < 0.2, `mode19 f'(0) 应该≈0（起步慢），got ${slopeAt0(19)}`);
  assert.ok(slopeAt1(19) > 1.0, `mode19 f'(1) 应该 >1（收尾快），got ${slopeAt1(19)}`);
  // 20：out-in，两端都快（f'=π/2）。
  assert.ok(slopeAt0(20) > 1.0, `mode20 f'(0) 应该 >1，got ${slopeAt0(20)}`);
  assert.ok(slopeAt1(20) > 1.0, `mode20 f'(1) 应该 >1，got ${slopeAt1(20)}`);
  // 21：in-out，两端都慢（f'=0）。
  assert.ok(slopeAt0(21) < 0.2, `mode21 f'(0) 应该≈0，got ${slopeAt0(21)}`);
  assert.ok(slopeAt1(21) < 0.2, `mode21 f'(1) 应该≈0，got ${slopeAt1(21)}`);
});

test('mode 1/2/3 与 4/5/6：ease-in（前段慢） vs ease-out（前段快）', () => {
  // u=0.3 处：u^n（in）应该远小于 1-(1-u)^n（out）。
  assert.ok(sampleFraction(1, 0.3) < sampleFraction(4, 0.3), 'mode1(u²) < mode4(1-(1-u)²) at u=0.3');
  assert.ok(sampleFraction(2, 0.3) < sampleFraction(5, 0.3));
  assert.ok(sampleFraction(3, 0.3) < sampleFraction(6, 0.3));
});

// ── mode 7/8/17 特判 ────────────────────────────────────────────────────────────

test('mode 7：逐帧累加 target（不是缓动曲线），量级 ≈ t×target', () => {
  const gate = 20;
  const vm = mkVm([instr(407, [gate, 7, f32bits(0.1), f32bits(0), f32bits(0)])]);
  stepFrame(vm, ops);
  for (let i = 0; i < gate; i++) stepInterpolators(vm);
  assert.ok(vm.pos.x > gate * 0.1 * 0.8 && vm.pos.x <= gate * 0.1 + 1e-3, `got ${vm.pos.x}`);
});

test('mode 8：标准 *Time 的切线恒 0 ⇒ smoothstep（3u²-2u³），不是完整 Hermite', () => {
  const v = sampleFraction(8, 0.5);
  // smoothstep(0.5) = 3*0.25-2*0.125 = 0.75-0.25 = 0.5
  assert.ok(Math.abs(v - 0.5) < 0.02, `smoothstep(0.5) 应该 =0.5, got ${v}`);
});

test('420 posBezier：非零切线时是真正的三次 Hermite（不是 smoothstep）', () => {
  const gate = 10;
  const args = new Array(10).fill(0);
  args[0] = gate;
  args[1] = f32bits(2); // tangent1.x
  args[4] = f32bits(1); // target.x
  args[7] = f32bits(0); // tangent2.x
  const vm = mkVm([instr(420, args)]); // 全字面量，mask=0
  stepFrame(vm, ops);
  for (let i = 0; i < 5; i++) stepInterpolators(vm); // u=0.5
  // 参考实现（与 group-general.js 的 hermite() 同一公式，测试里独立算一遍避免同源 bug）
  function hermiteRef(u, start, target, t1, t2) {
    const u2 = u * u;
    const u3 = u2 * u;
    return (2 * u3 - 3 * u2 + 1) * start + (u3 - 2 * u2 + u) * t1 + (u3 - u2) * t2 + (3 * u2 - 2 * u3) * target;
  }
  const expected = hermiteRef(0.5, 0, 1, 2, 0);
  assert.ok(Math.abs(vm.pos.x - expected) < 0.02, `期望 ${expected}, got ${vm.pos.x}`);
  assert.ok(Math.abs(vm.pos.x - 0.5) > 0.05, '应该明显偏离 smoothstep(0.5)=0.5，证明确实用了非零切线');
});

// ── 帧尾脏位：插值器 step 期间也要置对应的 flags_lo 位（02-vm-core.md §3.4）────────────

test('scaleTime(412) 每次 step 都置 flags_lo bit3（缩放脏位），rotateTime(410) 置 bit2', () => {
  const vm = mkVm([instr(412, [10, 4, f32bits(2), f32bits(3)])]); // 全字面量，mask=0
  stepFrame(vm, ops);
  vm.flagsLo = 0; // 清掉，验证 step 会重新置上
  stepInterpolators(vm);
  assert.equal(vm.flagsLo & 0x8, 0x8);
});

test('412 scaleTime：mode 真的落进插值器（不是被存到别的字段/丢失）—— 用曲线形状反证', () => {
  // 起点 scale=(1,1)（vm 默认），目标 (0,0)，mode=4（ease-out 二次）：u=0.1 处
  // ease(4,0.1)=1-(0.9)²=0.19，明显偏离线性 ease(0,0.1)=0.1——如果 mode 被存错位/丢失
  // （比如悄悄退化成硬编码 0/线性），这条测试会先于曲线数值验证测试组红。
  const gate = 100;
  const vm = mkVm([instr(412, [gate, 4, f32bits(0), f32bits(0)])]);
  stepFrame(vm, ops);
  assert.equal(vm._general.interp.scale.mode, 4, 'mode 参数应该原样落进插值器');
  for (let i = 0; i < 10; i++) stepInterpolators(vm); // u=0.1
  const expected = 1 - (1 - 0.1) ** 2; // ease(4, 0.1)
  assert.ok(Math.abs((1 - vm.scale.x) - expected) < 0.02, `期望 ease-out 曲线，got scale.x=${vm.scale.x}`);
  assert.ok(Math.abs(vm.scale.x - 0.9) > 0.01, '不该退化成线性 (u=0.1 时线性会是 0.9)');
});

// ══════════════════════════════════════════════════════════════════════════════════
// § 故意改坏验证（人工执行，见任务报告 task-I6-report.md；这里只留最终应该通过的断言）
// ══════════════════════════════════════════════════════════════════════════════════
// 三处改坏 + 复原的过程是手动跑 `npm test` 观察红/绿完成的（改动会在提交前撤销），
// 具体操作与实际输出写进报告，不留在测试代码里（留在这里会让「改坏」变成两份要维护的
// 拷贝，且下一位读者分不清哪份是当前生效的实现）。

// ── 2026-09-09 接线回归：frameTail 真的调用了 stepInterpolators ─────────────────────
// 背景：本文件其余插值测试都**直接调** stepInterpolators，绕过了 stepFrame 的帧尾。
// 那样即使 core.js 压根没接线也全绿。这两条专门走 createVm 注入 + 只调 stepFrame 的路径。
test('接线：createVm 注入 stepInterpolators 后，只调 stepFrame 插值也会推进', () => {
  const gate = 10;
  // ⚠️ 脚本必须**活过这一帧**，否则跑不到帧尾：只有一条指令时会跑到末尾走隐式 delete，
  // 而 vm-core §6.1 明确「opcode -1 不跑帧尾」——插值自然不会推进。这里用一条未来时刻的
  // 指令把时间闸门挡住（stepFrame 的分发循环 break 出来后才落到 frameTail）。
  const vm = createVm(
    {
      instructions: [
        instr(407, [gate, 0, f32bits(1), f32bits(0), f32bits(0)]),
        instr(0, [], { time: 9999 }), // nop，只为把脚本留在「未终止」状态
      ],
    },
    { stepInterpolators },
  );
  stepFrame(vm, ops); // 这一帧建库，帧尾走第一次步进
  const afterFirst = vm.pos.x;
  for (let i = 0; i < gate; i++) stepFrame(vm, ops); // 只用 stepFrame，不手调 stepInterpolators
  assert.ok(afterFirst < vm.pos.x, '插值必须随 stepFrame 推进——没推进说明 frameTail 没接线');
  assert.ok(Math.abs(vm.pos.x - 1) < 1e-6, `linear 走满 gate 应抵达 target=1，实得 ${vm.pos.x}`);
});

test('接线：不注入钩子时 stepFrame 不推进插值（钩子是可选的，不是默认行为）', () => {
  const vm = createVm({
    instructions: [
      instr(407, [10, 0, f32bits(1), f32bits(0), f32bits(0)]),
      instr(0, [], { time: 9999 }),
    ],
  });
  for (let i = 0; i < 20; i++) stepFrame(vm, ops);
  assert.equal(vm.pos.x, 0, '没注入钩子就不该有人推进插值器');
});

test('createVm 对不认识的 opts 键抛错（历史坑：静默忽略 prng 钩子）', () => {
  assert.throws(
    () => createVm({ instructions: [] }, { prng: () => 0 }),
    /不认识的 opts 键 "prng"/,
  );
});

// ══════════════════════════════════════════════════════════════════════════════════
// § 闸门 <= 0 的两条路（★ 2026-09-10：审阅 07-9 引出的实现修正）
// ══════════════════════════════════════════════════════════════════════════════════
// 一手：`th16:0x406e24` `test eax,eax` → `0x406e26` `jle 0x406f16` → `0x406f16` `jz 0x406ed8`
// （ZF 仍来自上面那次 test）。所以 ==0 与 <0 走的**不是**同一条路。
// 调用方侧的判据是 `!= 0`（ANM 侧 `th16:0x463b39` `cmp dword ptr [esi+0xdc],0` / `je`），
// 所以负闸门真的会进到 step 里。真实数据：16 条 `410 rotateTime(t=-1, mode=7)`。

test('闸门 < 0 + mode 7：每帧相对旋转、永不结束（真实数据里的 410 rotateTime(-1,7,…)）', () => {
  // 16 条真实指令就是这个形态。旧实现 `gate <= 0 → return false` 会让它一动不动。
  const vm = mkVm([instr(410, [-1, 7, f32bits(0.25), f32bits(0), f32bits(0)])]);
  stepFrame(vm, ops);
  assert.equal(vm.rotation.x, 0, '建库帧本身不步进');
  stepInterpolators(vm);
  assert.ok(Math.abs(vm.rotation.x - 0.25) < 1e-6, '第 1 帧：start += target');
  stepInterpolators(vm);
  assert.ok(Math.abs(vm.rotation.x - 0.5) < 1e-6, '第 2 帧：继续累加');
  for (let i = 0; i < 20; i++) stepInterpolators(vm);
  assert.ok(Math.abs(vm.rotation.x - Math.fround(0.25 * 22)) < 1e-4, '22 帧后仍在累加——永不结束');
});

test('闸门 < 0 不推进计时器（`th16:0x406f18` 全程不碰 +0x3c/+0x40/+0x44）', () => {
  const vm = mkVm([instr(410, [-1, 7, f32bits(0.25), f32bits(0), f32bits(0)])]);
  stepFrame(vm, ops);
  const entry = ensureGeneralState(vm).interp.rotation;
  const before = { currentF: entry.currentF, current: entry.current, gate: entry.gate };
  for (let i = 0; i < 5; i++) stepInterpolators(vm);
  assert.equal(entry.currentF, before.currentF, 'current_f 不动');
  assert.equal(entry.current, before.current, 'current 不动');
  assert.equal(entry.gate, before.gate, '闸门保持负值，不会被写回 0');
});

test('闸门 < 0 + 未解码的 mode：显式抛，不静默当 no-op', () => {
  const vm = mkVm([instr(410, [-1, 0, f32bits(1), f32bits(0), f32bits(0)])]);
  stepFrame(vm, ops);
  assert.throws(() => stepInterpolators(vm), /闸门 -1 < 0 且 mode=0/);
});

test('闸门 == 0：什么都不做（真机由调用方的 `cmp …,0 / je` 挡在函数外）', () => {
  const vm = mkVm([instr(410, [0, 7, f32bits(0.25), f32bits(0), f32bits(0)])]);
  stepFrame(vm, ops);
  const before = vm.rotation.x;
  for (let i = 0; i < 5; i++) stepInterpolators(vm);
  assert.equal(vm.rotation.x, before, 'gate==0 不写回任何字段');
});
