// System 组测试（opcode 0–7，8 条）。
// 依据：engine/anm/th16/05-system.md（下称 05）；控制流权威在
// engine/anm/th16/02-vm-core.md（下称 02）§6「终止/让出语义（五个分支）」。
//
// ★ 骨架 vs 组文件的切分（详见 src/vm/group-system.js 文件头）：opcode 1/2/3/4/6/7 的
// 语义完全在 core.js 的 stepFrame switch 里，ops 表里的 handler 正常运行下不会被调用；
// 只有 0（nop）与 5（case）真的会落进 ops 表查到的 handler，且它们本来就是 no-op。
// 所以下面的行为测试实际验证的是 **core.js 的 stepFrame + systemGroup 注册后的整体行为**，
// 而不是"systemGroup 的某个 handler 做了什么"——这正是 05/02 两份文档描述的语义应该有的
// 样子，也顺带验证了 systemGroup 里的 6 个 canary handler 从未被意外触发（一旦触发就会抛，
// 测试会直接变红，不会静默通过）。
import test from 'node:test';
import assert from 'node:assert/strict';
import { readFileSync } from 'node:fs';
import { parseAnm } from '../src/format/anm-v8.js';
import { createVm, stepFrame, registerGroup, triggerInterrupt } from '../src/vm/core.js';
import { systemGroup } from '../src/vm/group-system.js';

// 最小 op 表：systemGroup 本身 + 一个借来的 marker handler（opcode 100，"set"）。
// System 组的 8 条里没有任何一条会写整型/浮点变量槽，所有测试都要靠外部写一个标记值
// 才能观察"跑到哪儿了"——这里复用 group-math.js 的真实 opcode 100（set）没有意义
// （那会把 Math 组的正确性也牵扯进来），跟 vm-core.test.js 一样现场注册一个最小 set。
const ops = {};
registerGroup(ops, systemGroup);
registerGroup(ops, {
  100: (vm, [dst, v]) => { vm.i[dst] = v; }, // set：只用来当"跑到这里了"的标记
});

function loadScript(index) {
  const { entries } = parseAnm(readFileSync(new URL('./fixtures/vm-system.anm', import.meta.url)));
  return entries[0].scripts[index];
}

// ── 「8 条全注册」计数断言（照 vm-math.test.js 的同类断言）───────────────────────
test('0–7 共 8 个 opcode 全部注册进 systemGroup', () => {
  const codes = Object.keys(systemGroup).map(Number).sort((a, b) => a - b);
  assert.deepEqual(codes, [0, 1, 2, 3, 4, 5, 6, 7], '05-system.md §2 的 8 条表——少一条就是漏实现');
});

// ── 05-system.md #1 delete / 隐式脚本结尾：alive 变 false ─────────────────────────
// 02-vm-core.md §6.1：opcode -1（隐式结尾）与 opcode 1（delete）是同一段代码，
// 两条测试分别覆盖显式 delete 与隐式结尾，断言的形状必须一致（alive: true -> false）。

test('05-system.md #1 delete：pc=-1 且 alive=false', () => {
  const vm = createVm(loadScript(0)); // script -1 deleteFlow
  stepFrame(vm, ops);
  assert.equal(vm.i[0], 11, 'delete 之前那条 marker 应该先跑完');
  assert.equal(vm.pc, -1);
  assert.equal(vm.alive, false);
});

test('隐式脚本结尾（无任何终止指令）与 #1 delete 同一段代码：alive 同样变 false', () => {
  const vm = createVm(loadScript(2)); // script -3 implicitEnd
  stepFrame(vm, ops);
  assert.equal(vm.i[0], 33);
  assert.equal(vm.pc, -1);
  assert.equal(vm.alive, false);
});

// ── 05-system.md #2 static：冻结但 alive 不变（与 delete 的核心差异，02 §6.1）───────
test('05-system.md #2 static：pc=-1 但 alive 不变 —— 与 delete 的关键差异', () => {
  const vm = createVm(loadScript(1)); // script -2 staticFlow
  stepFrame(vm, ops);
  assert.equal(vm.i[0], 22, 'static 之前那条 marker 应该先跑完');
  assert.equal(vm.pc, -1);
  assert.equal(vm.alive, true, 'static 只冻结脚本，vm 本身仍然"活着"，与 delete 相反');
});

// ── 05-system.md #3 stop：自锁但可被 triggerInterrupt 唤醒（02 §6.1/§4）───────────
test('05-system.md #3 stop：自锁（pc 不前进），triggerInterrupt 命中后从标签处唤醒', () => {
  const vm = createVm(loadScript(3)); // script -4 stopWake
  stepFrame(vm, ops); // 跑到 marker(I0=1) 再撞上第一条 stop，自锁
  assert.equal(vm.i[0], 1);
  const lockedPc = vm.pc;
  assert.equal(vm.alive, true, 'stop 只是暂停，不是销毁');

  // 再跑几帧，验证真的锁住了：pc 一直停在同一条 stop 上，vm.time 每帧净变化 0
  // （decrement 再被帧尾 increment 抵消，02-vm-core.md §6.1 结论 4）。
  for (let n = 0; n < 4; n++) stepFrame(vm, ops);
  assert.equal(vm.pc, lockedPc, 'stop 自锁：指令指针不该动');
  assert.equal(vm.i[0], 1, '没有 interrupt 命中，标签后的指令不该被执行');

  // 唤醒：triggerInterrupt(42) 命中 `5 case 42` 标签，标签之后的指令在同一次
  // stepFrame 调用里就执行了（05 §5：interrupt 命中直接把指针带到标签之后）。
  triggerInterrupt(vm, 42);
  stepFrame(vm, ops);
  assert.equal(vm.i[0], 999, 'stop 应该被 interrupt 唤醒，跑到标签后面的 marker');
  assert.equal(vm.alive, true, '唤醒后落进另一条 stop，继续自锁，不是被销毁');
  assert.notEqual(vm.pc, lockedPc, '唤醒后指令指针应该已经离开原来的 stop');
});

// ── 05-system.md #6 wait：把时间往回拨，不是设一个未来的唤醒点（05 §6 结论）─────────
test('05-system.md #6 wait：vm.time 变负（往回拨），后续同标签指令被时间闸门挡住，靠逐帧 +1 爬回来', () => {
  const vm = createVm(loadScript(5)); // script -6 waitLiteral
  stepFrame(vm, ops); // marker(I0=1) -> wait(5)：vm.time -= 5
  assert.equal(vm.i[0], 1);
  assert.equal(vm.time, -4, 'wait 5 把 timeF 从 0 减到 -5，帧尾再 +1（GAME_SPEED）净得 -4');
  assert.ok(vm.time < 0, '★ 关键区分：这是"把时间拨到过去"，不是"设一个未来的唤醒点"（后者应该是正数）');
  assert.equal(vm.alive, true, 'wait 之后 vm 还活着，只是被自己拨的时间挡住了');

  // 后续第三条 marker（I0=2）与 wait 同在 time=0 的块里，此刻 vm.time 是负的，时间闸门
  // 挡住它——要靠帧尾的 `+= GAME_SPEED` 一帧一帧爬回 0 才会执行，正是"往回拨"而不是
  // "设唤醒点"的直接后果（若是设唤醒点，语义会是"提前触发"而不是"需要时间追上来"）。
  for (let n = 0; n < 4; n++) {
    stepFrame(vm, ops);
    assert.equal(vm.i[0], 1, `第 ${n + 2} 帧：vm.time 还没追回 0，marker 不该执行`);
  }
  // 第 6 次 stepFrame：vm.time 终于追回 0，marker 执行，脚本随即跑到末尾（隐式结尾=delete）。
  stepFrame(vm, ops);
  assert.equal(vm.i[0], 2, 'vm.time 追回 0 后，被挡住的 marker 终于执行');
  assert.equal(vm.alive, false, '脚本执行完毕，隐式结尾等价于 delete（02 §6.1）');
});

// ── 05-system.md #5 case / #7 caseReturn：偏移与时间的保存 / 恢复 ───────────────────
test('05-system.md #5/#7：caseReturn 恢复 interrupt 前的字节偏移与脚本时间，且同帧继续', () => {
  const vm = createVm(loadScript(4)); // script -5 caseReturnFlow
  stepFrame(vm, ops); // marker(I0=100)，随后撞上 time=9 的 case 标签，时间未到，让出
  assert.equal(vm.i[0], 100);
  const pcBeforeInterrupt = vm.pc; // 恰好停在 `5 case 42` 标签自己的偏移上
  const timeBeforeInterrupt = vm.time; // 1（frameTail 的 +1）

  triggerInterrupt(vm, 42);
  stepFrame(vm, ops);
  // 这一次 stepFrame 调用内部发生的事（全部在同一帧里）：
  //   doInterrupt 命中标签 -> 存快照(savedPc=pcBeforeInterrupt, savedTime=timeBeforeInterrupt)
  //   -> 时间设成标签自己的 9、指针跳过标签 -> 执行标签后的 marker(I0=999)
  //   -> 执行 caseReturn：时间恢复成 1、指针恢复成 pcBeforeInterrupt -> 回 DISPATCH（不是帧尾）
  //   -> 恢复后指针正好落回 case 标签本身，但此刻 vm.time=1 < 标签的 time=9，被时间闸门挡住
  //   -> 时间闸门挡住后才走帧尾，+1 变成 2。
  assert.equal(vm.i[0], 999, 'case 块里的 marker 必须在同一次 stepFrame 调用内跑完' +
    '（若 caseReturn 错误地跳去帧尾而不是 DISPATCH，这条断言会在下一次 stepFrame 才变真，这里就会失败）');
  assert.equal(vm.pc, pcBeforeInterrupt, 'caseReturn 必须把指针恢复到 interrupt 命中前的那个字节偏移');
  assert.equal(vm.time, timeBeforeInterrupt + 1, 'caseReturn 恢复的是 interrupt 前的脚本时间（1），' +
    '不是标签自己的时间（9）；恢复后被闸门挡住，仍然照常走一次帧尾 +1');
  assert.equal(vm.alive, true, '还没跑到脚本末尾');
});

// ── 05-system.md #5 case：没有 interrupt 命中时是纯 no-op ──────────────────────────
test('05-system.md #5 case：没有 interrupt 命中时是纯 no-op，标签前后的指令连续执行', () => {
  const vm = createVm(loadScript(6)); // script -7 caseNoopPassthrough
  stepFrame(vm, ops);
  assert.equal(vm.i[0], 7, '标签之前的 marker');
  assert.equal(vm.i[1], 77, '标签之后的 marker——case 标签本身不该产生任何可观察副作用');
  assert.equal(vm.alive, false, '脚本正常跑完（隐式结尾）');
});
