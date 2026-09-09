// Child management 组测试（opcode 500–509，10 条）。
// 依据：engine/anm/th16/08-child.md（下称「08」），父子创建的权威是
// engine/anm/th16/02-vm-core.md（下称「02」）§8/§8.1。
//
// 本文件全部走「直接调用 childGroup[opcode](vm, args, instr)」而不是
// `stepFrame(vm, ops)`——因为大部分用例要拿到「刚创建出来的那个 vm」做断言，而
// stepFrame 的分发循环不会把 handler 的返回值交回调用方（core.js：只在乎是不是
// PC_HANDLED）。500–506 的 handler 因此都设计成「返回新建的 vm」，直接调用即可拿到，
// 详见 group-child.js 文件尾的说明。只有「父死→整棵子树销毁」那条测试需要走真实
// stepFrame（要验证 core.js 自身的 `1 delete` 语义与本组的 destroyTree 怎么衔接），
// 那条测试里会显式用 registerGroup 建一次 ops 表。
//
// 夹具：全部手写指令对象（零版权字节），不依赖 parseAnm/真实 .anm 二进制——本组的
// 复杂度集中在「vm 树结构」而不是「格式层怎么解出 args/paramMask」，后者已经被
// vm-general.test.js/vm-settings.test.js 的端到端冒烟覆盖过，这里没有必要重复搭一份
// thanm 编译管线（500–509 的 formats_v8 签名在 local/vendor/thtk 里，但本地没有现成的
// 编译好的 thanm 可执行文件，权衡后选择跳过）。
import test from 'node:test';
import assert from 'node:assert/strict';
import { createVm, stepFrame, registerGroup } from '../src/vm/core.js';
import { childGroup, ensureChildState, destroyTree } from '../src/vm/group-child.js';
import { ensureGeneralState } from '../src/vm/group-general.js';

function mkVm(script) {
  return createVm(script);
}
function instr(opcode, args, { time = 0, paramMask = 0 } = {}) {
  return { time, opcode, paramMask, args };
}
function callOp(vm, opcode, args, opts) {
  const ins = instr(opcode, args, opts);
  return childGroup[opcode](vm, ins.args, ins);
}
// 浮点字面量按 float32 位模式编码（math-jumps §3 结论④），与其余组测试同款小工具。
function f32bits(x) {
  const f = new Float32Array([x]);
  return new Uint32Array(f.buffer)[0];
}

// 「2 static」单指令脚本：创建即 tick 时执行一次就冻结（pc=-1，alive 不变），既不会
// 立刻把新 vm 判死（不像空脚本/纯 nop 跑完会触发隐式 -1 = delete），也不会产生任何
// 副作用——绝大多数测试拿它当「占位子脚本」用。
const staticScript = { instructions: [{ time: 0, opcode: 2, paramMask: 0, args: [] }] };

test('10 条全注册：childGroup 的 key 集合正好是 500–509', () => {
  const keys = Object.keys(childGroup).map(Number).sort((a, b) => a - b);
  assert.deepEqual(keys, [500, 501, 502, 503, 504, 505, 506, 507, 508, 509]);
});

test('vm._child.scripts 未挂载时直接抛错，不静默——08 §0.1/§1.1「只能在当前 VM 所属的 .anm 文件里选脚本」', () => {
  const vm = mkVm(staticScript);
  assert.throws(() => callOp(vm, 500, [0]), /vm\._child\.scripts/);
});

test('脚本运行序号越界也抛错', () => {
  const vm = mkVm(staticScript);
  ensureChildState(vm).scripts = [staticScript];
  assert.throws(() => callOp(vm, 500, [7]), /脚本运行序号 7/);
});

// ── 08 §3：child 不拷父的变换字段，只继承 layer / root / flags_hi bit 0x2000000 ──────
test('createChild: 不拷父的变换字段（pos/rotation/scale/color/entity_pos），只继承 layer / root / flags_hi bit 0x2000000', () => {
  const grandparent = mkVm(staticScript);
  ensureChildState(grandparent).scripts = [staticScript];
  ensureChildState(grandparent).layer = 12;
  grandparent.flagsHi = (grandparent.flagsHi | 0x2000000) >>> 0;

  // 父自己也是从 grandparent 建出来的子——用来验证「root = 父的 root ?: 父」这条链式规则：
  // parent 不是根，它的 root 应该是 grandparent；child 的 root 应该跟着等于 grandparent，
  // 而不是 parent 自己。
  const parent = callOp(grandparent, 500, [0]);
  assert.equal(ensureChildState(parent).root, grandparent);

  // 给 parent 一套明显偏离默认值的变换，child 如果拷了就能测出来
  Object.assign(parent.pos, { x: 100, y: 200, z: 300 });
  Object.assign(parent.rotation, { x: 1, y: 2, z: 3 });
  parent.scale.x = 5;
  parent.color = 0x11223344;
  ensureGeneralState(parent).entityPos = { x: 42, y: 43, z: 44 };

  const child = callOp(parent, 500, [0]);

  // 不拷变换
  assert.deepEqual(child.pos, { x: 0, y: 0, z: 0 });
  assert.deepEqual(child.rotation, { x: 0, y: 0, z: 0 });
  assert.deepEqual(child.scale, { x: 1, y: 1 });
  assert.equal(child.color, 0xffffffff);
  assert.deepEqual(ensureGeneralState(child).entityPos, { x: 0, y: 0, z: 0 }, 'entity_pos 清 0，不拷父的');

  // 继承
  assert.equal(ensureChildState(child).layer, 12);
  assert.equal(ensureChildState(child).root, grandparent, 'root = 父的 root ?: 父');
  assert.equal(ensureChildState(child).parent, parent);
  assert.notEqual(child.flagsHi & 0x2000000, 0);
});

// ── 08 §3 / 02 §8.1：createRoot 是「续接」——entity_pos/rotation 整块拷，
//    pos_2 只拷源的 pos.x/.y（非对称落点 + 不写 .z）──────────────────────────────────
test('createRoot: entity_pos / rotation 整块拷源；pos_2 ← 源的 pos（非对称落点，只写 x/y）', () => {
  const source = mkVm(staticScript);
  ensureChildState(source).scripts = [staticScript];
  Object.assign(source.pos, { x: 1.5, y: -2.5, z: 9 });
  Object.assign(source.rotation, { x: 0.1, y: 0.2, z: 0.3 });
  ensureGeneralState(source).entityPos = { x: 7, y: 8, z: 9 };

  const root = callOp(source, 504, [0]);
  const rootGeneral = ensureGeneralState(root);

  // ★ 最容易写错的一条：落点是 pos_2（+0x4e0），不是 pos（+0x2c）；且只搬 x/y，z 不写
  assert.equal(rootGeneral.pos2.x, 1.5);
  assert.equal(rootGeneral.pos2.y, -2.5);
  assert.equal(rootGeneral.pos2.z, 0, 'pos_2.z 不写，保留默认值 0');
  assert.deepEqual(root.pos, { x: 0, y: 0, z: 0 }, 'root.pos 不被源的 pos 覆盖——那是 pos_2 的活');

  // entity_pos / rotation 是整块拷（x/y/z 都拷）
  assert.deepEqual(rootGeneral.entityPos, { x: 7, y: 8, z: 9 });
  assert.deepEqual(root.rotation, { x: 0.1, y: 0.2, z: 0.3 });

  // root 无父
  assert.equal(ensureChildState(root).parent, null);
  assert.equal(ensureChildState(root).root, null);
});

// ── ★2：flags_hi | 0x400 的写入时序在 root/child 路径上相反（02 §8.1），但本骨架没有
//    copy_vm/模板机制，两条路径在这里没有可观察的顺序差异——只能断言两条路径最终都
//    置位。详细论证见 group-child.js 文件头的 ★2 大段注释。 ────────────────────────
test('★2: flags_hi | 0x400 —— root/child 两条路径写入顺序相反（见 group-child.js ★2），但最终都置位「已托管」', () => {
  const src = mkVm(staticScript);
  ensureChildState(src).scripts = [staticScript];
  const child = callOp(src, 500, [0]);
  const root = callOp(src, 504, [0]);
  assert.notEqual(child.flagsHi & 0x400, 0, 'child 路径（代码里写在最前面，对应 copy_vm 之前）');
  assert.notEqual(root.flagsHi & 0x400, 0, 'root 路径（代码里写在最后面，对应 copy_vm 之后）');
});

// ── 02 §8：创建即 tick ────────────────────────────────────────────────────────────
test('创建即 tick：子 VM 在插进子链之前，已经跑完自己脚本 time<=0 的那一段', () => {
  const parent = mkVm(staticScript);
  // 脚本 1：time=-3 处调用本组自己的 507 ignoreParent(1)——选一个 childGroup 自己认领的
  // opcode，不需要依赖任何别的组的 ops 就能在「创建即 tick」阶段生效（tickOnce 未接到
  // 外部完整 ops 表时，回退到只含本组自己的 DEFAULT_TICK_OPS，见 group-child.js）。
  const tickScript = { instructions: [{ time: -3, opcode: 507, paramMask: 0, args: [1] }] };
  ensureChildState(parent).scripts = [staticScript, tickScript];

  const child = callOp(parent, 500, [1]); // 选脚本序号 1 = tickScript

  // 断言发生在 callOp 返回之后——我们自己一次 stepFrame 都没手动调用过，如果
  // ignoreParent(1) 已经生效，只能是 createManagedChild 内部的 tickOnce 干的。
  assert.notEqual(child.flagsHi & 0x10000, 0, '507 ignoreParent(1) 已经在“创建即 tick”阶段跑过');
  assert.equal(ensureChildState(parent).children[0], child, '子链 prepend 发生在 tick 之后');
});

// ── 08 §5：500–503 的子链插入无条件 prepend，与 mode 无关；mode 只记录供未来渲染层用 ──
test('500–503: mode 各不相同，但子链一律无条件 prepend', () => {
  const parent = mkVm(staticScript);
  ensureChildState(parent).scripts = [staticScript];
  const c500 = callOp(parent, 500, [0]);
  const c501 = callOp(parent, 501, [0]);
  const c502 = callOp(parent, 502, [0]);
  const c503 = callOp(parent, 503, [0]);

  assert.equal(ensureChildState(c500).insertMode, 0);
  assert.equal(ensureChildState(c501).insertMode, 4);
  assert.equal(ensureChildState(c502).insertMode, 2);
  assert.equal(ensureChildState(c503).insertMode, 6);

  // 无条件 prepend：最后建的排在最前面，跟 mode 无关（502/503 的“prepend”指的是渲染
  // 列表里的位置，不是子链——08 §5 结论2）
  assert.deepEqual(ensureChildState(parent).children, [c503, c502, c501, c500]);
});

// ── 08 §2.2 / §5：505/506 的求值次序 (0,1,2)，只写 pos_2.x/y ──────────────────────────
test('505 createChildPos: 500 + pos_2.x/y（可以是变量），.z 不写', () => {
  const parent = mkVm(staticScript);
  ensureChildState(parent).scripts = [staticScript];
  parent.f = [1.5, -2.5, 0, 0]; // F0/F1

  const child = callOp(parent, 505, [0, f32bits(10004), f32bits(10005)], { paramMask: 0b110 });
  const general = ensureGeneralState(child);
  assert.equal(general.pos2.x, 1.5, 'x 来自变量 F0');
  assert.equal(general.pos2.y, -2.5, 'y 来自变量 F1');
  assert.equal(general.pos2.z, 0);
  assert.equal(ensureChildState(parent).children[0], child, '505 也进子链（是 500 的超集）');
});

test('506 createRootPos: 504 + pos_2.x/y 写成字面量', () => {
  const source = mkVm(staticScript);
  ensureChildState(source).scripts = [staticScript];

  const root = callOp(source, 506, [0, f32bits(3.25), f32bits(-4.5)]);
  const general = ensureGeneralState(root);
  assert.equal(general.pos2.x, 3.25);
  assert.equal(general.pos2.y, -4.5);
  assert.equal(ensureChildState(root).parent, null, '506 建的仍然是根');
});

// ── 08 §4.1：507 是设值，不是翻转 ──────────────────────────────────────────────────
test('507 ignoreParent: 设值不是翻转（连续两次 ignoreParent(1) 仍是 1）', () => {
  const vm = mkVm(staticScript);
  callOp(vm, 507, [1]);
  assert.notEqual(vm.flagsHi & 0x10000, 0);
  callOp(vm, 507, [1]);
  assert.notEqual(vm.flagsHi & 0x10000, 0, '不是 toggle，仍然是 1');
  callOp(vm, 507, [0]);
  assert.equal(vm.flagsHi & 0x10000, 0);
});

// ── 08 §6.1：508 不建子 VM，只重配当前 VM 的回调下标 ────────────────────────────────
test('508 createEffect: kind=1 把 6 个回调下标如实写进 vm._child.effectCallbackIndices', () => {
  const vm = mkVm(staticScript);
  const before = ensureChildState(vm).children.length;
  callOp(vm, 508, [1]);
  assert.deepEqual(ensureChildState(vm).effectCallbackIndices, {
    onTickIdx: 2, slotC8: 2, slotCC: 2, onInterruptIdx: 2, slotD4: 1, slotD8: 1,
  });
  assert.equal(ensureChildState(vm).children.length, before, '508 不建子 VM（08 §6.1 裁决社区“造子图”的说法不对）');
});

test('508 createEffect: 未建模的 kind 直接抛错，不编造下标', () => {
  const vm = mkVm(staticScript);
  assert.throws(() => callOp(vm, 508, [2]), /未建模/);
});

// ── 08 §4.2：509 只拷 16 个变量，不拷 pos/rotation；父为空是 no-op ──────────────────
test('509 copyParentVars: 拷 16 个变量（i0-3/f0-3/i4/i5/randParam*/mysteryVars），不拷 pos/rotation', () => {
  const parent = mkVm(staticScript);
  parent.i = [1, 2, 3, 4];
  parent.f = [5, 6, 7, 8];
  parent.i4 = 9;
  parent.i5 = 10;
  parent.mysteryVars = { x: 11, y: 12, z: 13 };
  parent.randParamOne = 14;
  parent.randParamPi = 15;
  parent.randParamInt = 16;
  Object.assign(parent.pos, { x: 999, y: 999, z: 999 });
  Object.assign(parent.rotation, { x: 888, y: 888, z: 888 });
  ensureChildState(parent).scripts = [staticScript];

  const child = callOp(parent, 500, [0]);
  callOp(child, 509, []);

  assert.deepEqual(child.i, [1, 2, 3, 4]);
  assert.deepEqual(child.f, [5, 6, 7, 8]);
  assert.equal(child.i4, 9);
  assert.equal(child.i5, 10);
  assert.deepEqual(child.mysteryVars, { x: 11, y: 12, z: 13 });
  assert.equal(child.randParamOne, 14);
  assert.equal(child.randParamPi, 15);
  assert.equal(child.randParamInt, 16);
  assert.notDeepEqual(child.pos, parent.pos, '509 不拷 pos');
  assert.notDeepEqual(child.rotation, parent.rotation, '509 不拷 rotation');
});

test('509 copyParentVars: 父为空（根 VM 上调用）是 no-op，不抛错', () => {
  const root = mkVm(staticScript);
  const before = { i: [...root.i], f: [...root.f], i4: root.i4 };
  callOp(root, 509, []);
  assert.deepEqual(root.i, before.i);
  assert.deepEqual(root.f, before.f);
  assert.equal(root.i4, before.i4);
});

// ── 02 §6：父死 → 整棵子树销毁 ──────────────────────────────────────────────────
test('父死 → 整棵子树销毁（02-vm-core.md §6：flags_hi |= 0x40 递归打标 + alive=false）', () => {
  const deleteScript = { instructions: [{ time: 0, opcode: 1, paramMask: 0, args: [] }] };
  const root = mkVm(deleteScript);
  ensureChildState(root).scripts = [deleteScript, staticScript];
  const childA = callOp(root, 500, [1]);
  const grandchild = callOp(childA, 500, [1]);

  assert.equal(root.alive, true);
  assert.equal(childA.alive, true);

  const ops = {};
  registerGroup(ops, childGroup);
  stepFrame(root, ops); // 跑到 `1 delete`——core.js 自己的语义，不需要本组配合

  assert.equal(root.alive, false, 'root 自己先死，这是 core.js 的既有行为');
  assert.equal(childA.alive, true, '子树还没被级联销毁——core.js 不知道树的存在，级联需要骨架主动调用 destroyTree（见任务报告“骨架接线需求”）');

  destroyTree(root);

  assert.equal(childA.alive, false);
  assert.notEqual(childA.flagsHi & 0x40, 0);
  assert.equal(grandchild.alive, false);
  assert.notEqual(grandchild.flagsHi & 0x40, 0);
  assert.equal(ensureChildState(root).children.length, 0, '销毁后清空子链');
});
