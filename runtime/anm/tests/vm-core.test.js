// VM 骨架测试：取指 / 时间推进 / 跳转 / interrupt / 生死。
// 依据：engine/anm/th16/02-vm-core.md（下称 vm-core）§2 的 AnmVm_run 伪代码；
// engine/anm/th16/03-math-jumps.md（下称 math-jumps）§2.1/§3/§5（跳转字面量 + 变量号编码）。
//
// 前三条测试逐字来自 task-I2-brief.md 的「步骤 2」，只按任务书裁定的一处必要修正改了
// `.buffer`（brief 原写法对小文件是 bug：Node 的 Buffer 来自共享内存池，`.buffer` 拿到的
// 是整个池，见 tests/format.test.js 顶部同样的说明）。后面几条是本任务自己加的：
// 变量解析 + 跳转（字面目标）+ interrupt。
import test from 'node:test';
import assert from 'node:assert/strict';
import { readFileSync } from 'node:fs';
import { parseAnm } from '../src/format/anm-v8.js';
import { createVm, stepFrame, registerGroup, resolveArg, isVarBit, PC_HANDLED, triggerInterrupt } from '../src/vm/core.js';

// resolveArg 的浮点侧变量号解码要求“10000 的 float32 位模式”，不是十进制 10000
// （math-jumps §3 结论④：浮点指令把变量号也编成 float32）——测试里独立算一遍位模式，
// 不复用 core.js 内部的 u32BitsToFloat32（那是私有实现细节，测试不应该依赖它）。
function f32BitsOf(value) {
  const f = new Float32Array([value]);
  return new Uint32Array(f.buffer)[0];
}

// 最小 op 表：只实现 set/add，用来验骨架而非验组
const ops = {};
registerGroup(ops, {
  100: (vm, [dst, v]) => { vm.i[dst] = v; }, // set
  112: (vm, [dst, v]) => { vm.i[dst] += v; }, // add（借用真实 opcode 112 的“加法”这个名字，
  // 但只喂了 2 个参数——夹具里 `ins_112(0, 4)` 就是故意只给 2 个参数，见 fixtures/
  // vm-timing.anm.txt 顶部注释：thanm 对“少给参数”只警告不报错。这不是在断言真实 th16
  // opcode 112（add3，3 参数）的语义，纯粹是骨架测试借用的号）
});

function loadScript(path, index = 0) {
  const { entries } = parseAnm(readFileSync(new URL(path, import.meta.url)));
  return entries[0].scripts[index];
}

test('同一时间标签上的指令在同一帧内全部执行', () => {
  const vm = createVm(loadScript('./fixtures/vm-timing.anm'));
  stepFrame(vm, ops);
  assert.equal(vm.i[0], 7, 'set(i0,3) 与 add(i0,4) 同在 t=0，应在第一帧跑完');
});

test('时间未到的指令不执行', () => {
  const vm = createVm(loadScript('./fixtures/vm-timing.anm'));
  stepFrame(vm, ops);
  assert.equal(vm.time, 1, '一帧推进 1');
  assert.notEqual(vm.i[1], 99, 't=10 的指令不该在第 1 帧执行');
});

test('stop 之后 pc 不再前进，但 vm 仍存活', () => {
  const vm = createVm(loadScript('./fixtures/vm-timing.anm'));
  for (let n = 0; n < 60; n++) stepFrame(vm, ops);
  const pc = vm.pc;
  stepFrame(vm, ops);
  assert.equal(vm.pc, pc);
  assert.equal(vm.alive, true);
});

// ── 变量解析 ──────────────────────────────────────────────────────────────
// script 1（waitVar）：`ins_6([10000])`——6 wait 的参数是变量引用（I0 = 10000），
// 不需要任何 opcode 组：wait 是 vm-core.md §2 case 6 明确划给骨架自己处理的，
// 用来验证 core.js 内部真的接上了 resolveArg，不是把 param_mask 当空气。
test('paramMask 置位的参数被当变量号解析（wait 的参数取自 I0，不是字面量）', () => {
  const vm = createVm(loadScript('./fixtures/vm-timing.anm', 1));
  vm.i[0] = 5;
  stepFrame(vm, {});
  // vm-core §2 case 6：cur_f -= n。n 若真从 vm.i[0] 读出来是 5，vm.time 该是 -5；
  // 若 resolveArg 没生效、把裸值 10000 当字面量减，会是 -10000——两个数量级差出的结果
  // 足以直接证伪“没解析变量”这个假设，不需要更细的断言。
  assert.equal(vm.time, -5);
});

test('resolveArg 对未注册的变量号报错（不是静默按字面量放行）', () => {
  const vm = createVm(loadScript('./fixtures/vm-timing.anm', 1));
  // 10010 不在 I2 骨架实现的 I0–I3/F0–F3 范围内（02-vm-core.md §3.4 还有别的变量号，
  // 但那些留给用到它们的 opcode 组扩展）——resolveArg 应该报错，而不是返回某个乱数。
  assert.throws(() => resolveArg(vm, 10010, true, 'int'), /I2 骨架尚未覆盖/);
});

// 跨类型变量读：math-jumps §3 明确 get_int_var/get_float_var 的定义域都是 10000–10035，
// 不是各自只认自己类型那 4 个槽——真实数据里 827 个整型变量参数有 9 次是 opcode 100/112
// （整型指令）去读 F0。这不是边角情况，I0–I3/F0–F3 这 8 个槽必须支持跨类型读。
test('跨类型变量读：整型指令读浮点槽 F0（cvttss2si，截零）', () => {
  const vm = createVm(loadScript('./fixtures/vm-timing.anm', 1));
  vm.f[0] = 3.75;
  // kind='int' 时变量号本身按 int32 解码（math-jumps §3 结论④：编码取决于指令类型，
  // 不取决于被引用的变量），所以这里直接传十进制 10004，不用转 float32 位模式。
  assert.equal(resolveArg(vm, 10004, true, 'int'), 3, 'cvttss2si 截零：3.75 -> 3（不是 4，不是 floor）');
});

test('跨类型变量读：浮点指令读整型槽 I0（cvtdq2ps）', () => {
  const vm = createVm(loadScript('./fixtures/vm-timing.anm', 1));
  vm.i[0] = 7;
  // kind='float' 时变量号按 float32 位模式编码——传 f32BitsOf(10000)，不是十进制 10000
  // （直接传十进制会被 u32BitsToFloat32 按位重解释成一个跟 10000 毫不相干的极小浮点数，
  // Math.trunc 之后大概率是 0，命中 I0 反而说明测试写错了，不是巧合过关）。
  assert.equal(resolveArg(vm, f32BitsOf(10000), true, 'float'), 7);
});

// ── 跳转（字面目标）──────────────────────────────────────────────────────
// script 2（jmpLit）：`ins_200(landing, 0)`。math-jumps.md §2.1/§5 一手证实 th16 的跳转族
// dest/t 永远按字面量读，从不查 param_mask——这个 VM 里不存在“变量跳转目标”，所以这里
// 只测字面目标这一条路径（早先任务说明提过要另测一条“变量目标”，那是控制器给错的前提，
// 已经撤回，没有据此写过代码）。
// opcode 200 本身不是骨架的职责（骨架不认识任何具体 opcode，见 core.js 文件头），这里
// 现场注册一个最小 handler，只用来证明骨架的“handler 自己改 pc、骨架不再 += size”
// 这套通用机制（PC_HANDLED）配合字节偏移取指是对的。
test('跳转：handler 直接改写 pc 后，骨架按字节偏移取指，不再自动前进', () => {
  const jmpOps = {};
  registerGroup(jmpOps, {
    100: ops[100], // 复用上面的 set，landing 前后各一条 ins_100 都要能跑
    200: (vm, args) => {
      // math-jumps §2.1：200 的 dest = args[0]，恒为字面量。
      vm.pc = args[0];
      return PC_HANDLED;
    },
  });
  const vm = createVm(loadScript('./fixtures/vm-timing.anm', 2));
  stepFrame(vm, jmpOps);
  assert.equal(vm.i[0], 222, '跳转应跳过 ins_100(0,111)，落到 landing 执行 ins_100(0,222)');
});

// ── interrupt ────────────────────────────────────────────────────────────
// script 3（interrupt）：`ins_5(7)` 是 interrupt 号 7 的标签，后面跟一条正常指令。
test('interrupt：命中标签后从标签之后的指令继续跑', () => {
  const vm = createVm(loadScript('./fixtures/vm-timing.anm', 3));
  triggerInterrupt(vm, 7);
  stepFrame(vm, ops); // 需要 opcode 100（set）的 handler 才能跑标签后面那条指令
  assert.equal(vm.i[0], 777, 'interrupt 命中 7 号标签，跳过标签本身，执行紧跟着的 ins_100(0,777)');
});

// ── 终止（五个分支里“死”的那一半，前一轮完全没覆盖）──────────────────────────
// vm-core.md §6.1：-1/1（delete）不跑帧尾、直接销毁；2（static）不跑帧尾但只冻结，
// alive 不变；这是两条容易混淆的路径，必须分别断言。
test('终止：opcode 1 delete 后 pc=-1 且 alive=false', () => {
  const vm = createVm(loadScript('./fixtures/vm-timing.anm', 4));
  stepFrame(vm, ops);
  assert.equal(vm.i[0], 11, 'delete 之前那条 ins_100 应该先跑完');
  assert.equal(vm.pc, -1);
  assert.equal(vm.alive, false);
});

test('终止：opcode 2 static 冻结（pc=-1）但 alive 不变——这是它与 delete 的关键差异', () => {
  const vm = createVm(loadScript('./fixtures/vm-timing.anm', 5));
  stepFrame(vm, ops);
  assert.equal(vm.i[0], 22, 'static 之前那条 ins_100 应该先跑完');
  assert.equal(vm.pc, -1);
  assert.equal(vm.alive, true, 'static 只冻结脚本，vm 本身仍然“活着”，不同于 delete');
});

test('终止：脚本隐式结束（没有任何终止指令，fetchInstruction 返回 null）等价于 delete', () => {
  const vm = createVm(loadScript('./fixtures/vm-timing.anm', 6));
  stepFrame(vm, ops);
  assert.equal(vm.i[0], 33);
  assert.equal(vm.pc, -1);
  assert.equal(vm.alive, false, '01-format.md §5.1：脚本末尾的 opcode=-1 结束标记不进 instructions 数组，' +
    '但跑到那里就是 delete 语义');
});

// ── 分发循环护栏（vm-core.md §9 点名要求，见 core.js 文件头）────────────────────
test('分发循环护栏：自跳脚本被护栏截住，不会真的死循环下去', () => {
  const jmpOps = {};
  registerGroup(jmpOps, {
    200: (vm, args) => {
      vm.pc = args[0];
      return PC_HANDLED;
    },
  });
  // 覆盖成很小的上限，好让测试快速触发，不用真的空转 20 万次。
  const vm = createVm(loadScript('./fixtures/vm-timing.anm', 7), { maxInstructionsPerFrame: 5 });
  assert.throws(() => stepFrame(vm, jmpOps), /单帧执行的指令数超过 5 条上限/);
});

// isVarBit 是给各 opcode 组用的小工具，骨架自己（case 6 wait）也在用它，这里顺带验证
// 一下它本身的行为：bit 0 置位 / 不置位两种情况都要对。
test('isVarBit：按位判断 param_mask 的第 index 位', () => {
  assert.equal(isVarBit(0b1, 0), true);
  assert.equal(isVarBit(0b10, 0), false);
  assert.equal(isVarBit(0b10, 1), true);
});
