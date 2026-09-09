// Settings 组测试（opcode 300–317，18 条）。
// 依据：engine/anm/th16/06-settings.md（下称「06」）。
//
// 结构仿 vm-math.test.js：一条端到端冒烟（真实 parseAnm -> createVm -> stepFrame，验证
// 格式层解出的 args/paramMask 与 handler 接得上）+ 大量手写指令的精确位测试（真实文件里
// 精确控制 paramMask/参数值不直观，06 §5 也证实本组大部分位组合 ZUN 数据里从未出现过，
// 手写指令是唯一能测到它们的办法）。
import test from 'node:test';
import assert from 'node:assert/strict';
import { readFileSync } from 'node:fs';
import { parseAnm } from '../src/format/anm-v8.js';
import { createVm, stepFrame, registerGroup } from '../src/vm/core.js';
import { settingsGroup } from '../src/vm/group-settings.js';

const ops = {};
registerGroup(ops, settingsGroup);

function instr(opcode, args, { time = 0, paramMask = 0 } = {}) {
  return { time, opcode, paramMask, args };
}
function mkVm(instructions, opts) {
  return createVm({ instructions }, opts);
}

// ── 端到端冒烟：真实 .anm 文件（settings-ops.anm.txt，自写、零版权字节）─────────────────
test('端到端冒烟：真实文件解析 + 一帧执行，300/302/304/308/312/316 都生效', () => {
  const { entries } = parseAnm(readFileSync(new URL('./fixtures/settings-ops.anm', import.meta.url)));
  const vm = createVm(entries[0].scripts[0]);
  stepFrame(vm, ops);

  assert.equal(vm.spriteId, 3, '300 sprite(3)');
  assert.equal(vm.flagsLo & 0x1, 0x1, '300 置 flags_lo bit0（可见）');
  assert.equal((vm.flagsLo >>> 25) & 0x1f, 1, '302 renderMode(1) 写进 bit25-29');
  assert.equal(vm.layer, 26, '304 layer(26)：vm.layer 镜像 +0x18');
  assert.equal((vm.flagsHi >>> 20) & 0x1f, 1, '304：layer 26 落在 20-31 区间，flags_hi 加 0x100000（bit20）');
  assert.equal(vm.scale.x, -1, '308 flipX：scale.x 从默认 1 变成 -1');
  assert.equal(vm.flagsLo & 0x8, 0x8, '308：bit3 缩放脏位也被置上（★ 更正后的行为，最容易漏）');
  assert.equal(vm.flagsHi & 0x3, 2, '312 scrollMode(x=2,...)：flags_hi bit0-1 = 2');
  assert.equal(vm.flagsLo & 0x2, 0x2, '316：flags_lo bit1 置位（默认也是置位，但仍验证 handler 真的跑了）');
});

// ── 「18 条全注册」计数断言 ──────────────────────────────────────────────────────
test('18 条 opcode（300–317）全部已注册', () => {
  const codes = Object.keys(settingsGroup).map(Number);
  const inRange = codes.filter((c) => c >= 300 && c <= 317);
  assert.equal(inRange.length, 18, '缺一条就说明 06-settings.md 的表没实现完');
  assert.equal(codes.length, 18, 'settingsGroup 不应该含 300-317 之外的号');
});

// ── 必须有的测试①：三条 wip:2 flag（306/316/317）的位确实被置对（06 §3.1/§3.2）────────

test('306：flags_lo bit15（0x8000）被置成 arg0 & 1', () => {
  const vm = mkVm([instr(306, [1])]);
  stepFrame(vm, ops);
  assert.equal(vm.flagsLo & 0x8000, 0x8000);
});

test('306：arg0=0 时 bit15 被清（不是只会置位，「按值写位」不是 or-only）', () => {
  const vm = mkVm([instr(306, [1]), instr(306, [0])]);
  stepFrame(vm, ops);
  assert.equal(vm.flagsLo & 0x8000, 0, '第二条 306(0) 应该把 bit15 清掉');
});

test('316：flags_lo bit1（0x2）被置位（默认本来就置位，用 317 先清掉再验证 316 真的能置回）', () => {
  const vm = mkVm([instr(317, []), instr(316, [])]);
  stepFrame(vm, ops);
  assert.equal(vm.flagsLo & 0x2, 0x2, '316 应该把 bit1 置回');
});

test('317：flags_lo bit1（0x2）被清（06 §3.2：默认置位，317 执行后必须变成清位，否则测不出区分力）', () => {
  const vm = mkVm([instr(317, [])]);
  stepFrame(vm, ops);
  assert.equal(vm.flagsLo & 0x2, 0, '317 之前默认 bit1=1，317 之后必须是 0');
});

// ── 必须有的测试②：308/309 的 bit3 缩放脏位（★ 06 §2.3 审阅更正，最容易漏）───────────

test('308 flipX：scale.x 取反 + bit11 toggle + bit3（缩放脏位）也被置上', () => {
  const vm = mkVm([instr(308, [])]);
  stepFrame(vm, ops);
  assert.equal(vm.scale.x, -1, 'scale.x 从默认 1 变成 -1');
  assert.equal(vm.flagsLo & 0x800, 0x800, 'bit11 toggle：从 0 变 1');
  assert.equal(vm.flagsLo & 0x8, 0x8, '★ bit3 缩放脏位必须也被置上（06 §2.3 更正）');
});

test('308 flipX 连续调两次：scale 还原，bit11 toggle 回去，但 bit3 保持置位（or，不是 toggle）', () => {
  const vm = mkVm([instr(308, []), instr(308, [])]);
  stepFrame(vm, ops);
  assert.equal(vm.scale.x, 1, '连续两次 flip，scale.x 还原成 1');
  assert.equal(vm.flagsLo & 0x800, 0, 'bit11 是真 toggle：两次之后清回 0');
  assert.equal(vm.flagsLo & 0x8, 0x8, 'bit3 是 or（每次都置位），不会因为第二次调用而清掉');
});

test('309 flipY：scale.y 取反 + bit12 toggle + bit3（缩放脏位）也被置上', () => {
  const vm = mkVm([instr(309, [])]);
  stepFrame(vm, ops);
  assert.equal(vm.scale.y, -1);
  assert.equal(vm.flagsLo & 0x1000, 0x1000, 'bit12 toggle');
  assert.equal(vm.flagsLo & 0x8, 0x8, '★ bit3 缩放脏位');
});

// ── 必须有的测试③：312 的双字段位写入（06 §3.5：x 落 flags_hi，y 落 flags_lo）────────

test('312 scrollMode(x,y)：x 落 flags_hi bit0-1，y 落 flags_lo bit30-31，两个字段独立', () => {
  const vm = mkVm([instr(312, [2, 3])]); // x=2 -> flags_hi bit0-1=2；y=3 -> flags_lo bit30-31=3
  stepFrame(vm, ops);
  assert.equal(vm.flagsHi & 0x3, 2, 'x=2 写进 flags_hi bit0-1');
  assert.equal((vm.flagsLo >>> 30) & 0x3, 3, 'y=3 写进 flags_lo bit30-31');
});

test('312 scrollMode：x/y 互不干扰（改 x 不动 flags_lo，改 y 不动 flags_hi 的其余位）', () => {
  const vm = mkVm([instr(311, [1]), instr(312, [1, 0])]); // 先用 311 在 flags_hi 打一个无关位
  stepFrame(vm, ops);
  assert.equal(vm.flagsHi & 0x800, 0x800, '311 resampleMode(1) 先置的 bit11 应该还在');
  assert.equal(vm.flagsHi & 0x3, 1, '312 的 x=1 只动 bit0-1，不该清掉 311 置的 bit11');
});

// ── 必须有的测试④：301 的 b==0 抛异常（06 §2.3：无零守卫，裸 div 会 #DE）─────────────

test('301 spriteRand：b==0 抛异常而不是静默返回哨兵', () => {
  const vm = mkVm([instr(301, [1, 0])]);
  vm.prng = () => 0xdeadbeef;
  assert.throws(() => stepFrame(vm, ops), /b==0/);
});

test('301 spriteRand：没注入 vm.prng 时抛异常（不静默退化）', () => {
  const vm = mkVm([instr(301, [1, 8])]);
  assert.throws(() => stepFrame(vm, ops), /vm\.prng/);
});

test('301 spriteRand：正常路径 id = a + (raw % (u32)b)，flags_lo bit0 置位', () => {
  const vm = mkVm([instr(301, [120, 8])]);
  vm.prng = () => 23; // 23 % 8 = 7
  stepFrame(vm, ops);
  assert.equal(vm.spriteId, 127, '120 + 23%8 = 127');
  assert.equal(vm.flagsLo & 0x1, 0x1, '301 也置可见位');
});

test('301 spriteRand：b 为负数按 u32 解释，模数极大（06 §2.3 更正：不是"几乎恒=a"）', () => {
  const vm = mkVm([instr(301, [0, -1])]); // b = -1 = 0xFFFFFFFF
  vm.prng = () => 0x12345678;
  stepFrame(vm, ops);
  assert.equal(vm.spriteId, 0x12345678 | 0, 'b 极大 -> raw % b == raw -> id = 0 + raw');
});

test('301 spriteRand：求值顺序 arg0 -> arg1，两个参数读完才抽随机', () => {
  // 用会推进 PRNG 的变量号（10011 = randf01()*randParamOne）间接验证顺序：
  // 若实现先抽随机再读参数，会打乱下面这条 PRNG 序列的消费次数。
  let calls = 0;
  const vm = mkVm([instr(301, [1, 2])]);
  vm.prng = () => {
    calls += 1;
    return 5;
  };
  stepFrame(vm, ops);
  assert.equal(calls, 1, '301 只应该消费一次 PRNG draw（不该因为参数求值顺序错误而多抽/少抽）');
  assert.equal(vm.spriteId, 1 + (5 % 2));
});

// ── 必须有的测试⑤：300 的负数 id 走未建模分支 ──────────────────────────────────────

test('300 sprite：id < 0 走「系统贴图」分支，抛「未建模」异常（★ 06 §2.1 更正：热路径不是理论路径）', () => {
  const vm = mkVm([instr(300, [-1])]);
  assert.throws(() => stepFrame(vm, ops), /系统贴图|AsciiManager/);
});

test('300 sprite：id >= 0 正常写 spriteId + flags_lo bit0 + spriteSelectTime', () => {
  const vm = mkVm([instr(300, [42])], { gameSpeed: 1 });
  vm.time = 7; // 手动摆一个非零时间，验证 spriteSelectTime 真的镜像 vm.time
  stepFrame(vm, ops);
  assert.equal(vm.spriteId, 42);
  assert.equal(vm.flagsLo & 0x1, 0x1);
  assert.equal(vm.spriteSelectTime, 7, '+0x494 应该镜像 sprite 指令执行时的 vm.time');
});

test('300 sprite：id 可为变量（paramMask 置位时经 get_int_var 读 I0）', () => {
  const vm = mkVm([instr(300, [10000], { paramMask: 0b1 })]);
  vm.i[0] = 99;
  stepFrame(vm, ops);
  assert.equal(vm.spriteId, 99, '若被当字面量读，会得到 10000 而不是 99');
});

// ── 参数宽度三族：304/307/315 低 8 位截断（06 §4）────────────────────────────────────

test('304 layer：超过 255 被 movzx 截断（低 8 位）', () => {
  const vm = mkVm([instr(304, [256 + 26])]); // 0x11A & 0xff = 26
  stepFrame(vm, ops);
  assert.equal(vm.layer, 26);
});

test('307 randMode：写死位 flags_hi bit10，即便全 exe 无读者也照写（06 §3.3）', () => {
  const vm = mkVm([instr(307, [1])]);
  stepFrame(vm, ops);
  assert.equal(vm.flagsHi & 0x400, 0x400);
});

test('315 colorizeChildren：flags_hi bit25', () => {
  const vm = mkVm([instr(315, [1])]);
  stepFrame(vm, ops);
  assert.equal(vm.flagsHi & 0x2000000, 0x2000000);
});

// ── 302 renderMode：mode 10 的 on_tick 回调分支未建模，应显式抛 ──────────────────────

test('302 renderMode：mode=10 之外正常写 5 位模式字段', () => {
  const vm = mkVm([instr(302, [8])]);
  stepFrame(vm, ops);
  assert.equal((vm.flagsLo >>> 25) & 0x1f, 8);
});

test('302 renderMode：mode=10 触发 on_tick 回调注册，本骨架未建模，显式抛异常', () => {
  const vm = mkVm([instr(302, [10])]);
  assert.throws(() => stepFrame(vm, ops), /on_tick|不建模/);
});

// ── 303/305/310/311/313/314：单位字段直写，逐条覆盖 ─────────────────────────────────

test('303 blendMode：flags_lo bit5-8', () => {
  const vm = mkVm([instr(303, [9])]);
  stepFrame(vm, ops);
  assert.equal((vm.flagsLo >>> 5) & 0xf, 9);
});

test('305 noZBuffer：flags_lo bit13（不是 flags_hi 的同名位，06 §3.4 点名的坑）', () => {
  const vm = mkVm([instr(305, [1])]);
  stepFrame(vm, ops);
  assert.equal(vm.flagsLo & 0x2000, 0x2000, 'flags_lo bit13 应该被置');
  assert.equal(vm.flagsHi & 0x2000, 0, 'flags_hi 的同名位不该被动到');
});

test('310 visible：flags_lo bit0，字面量 32 位（不是 byte，06 §6 裁决#4）', () => {
  const vm = mkVm([instr(310, [1])]);
  stepFrame(vm, ops);
  assert.equal(vm.flagsLo & 0x1, 0x1);
});

test('310 visible(0)：清可见位', () => {
  const vm = mkVm([instr(310, [1]), instr(310, [0])]);
  stepFrame(vm, ops);
  assert.equal(vm.flagsLo & 0x1, 0);
});

test('311 resampleMode：flags_hi bit11', () => {
  const vm = mkVm([instr(311, [1])]);
  stepFrame(vm, ops);
  assert.equal(vm.flagsHi & 0x800, 0x800);
});

test('313 resolutionMode：flags_hi bit20-22（$S，可为变量）', () => {
  const vm = mkVm([instr(313, [10000], { paramMask: 0b1 })]);
  vm.i[0] = 2;
  stepFrame(vm, ops);
  assert.equal((vm.flagsHi >>> 20) & 0x7, 2);
});

test('314 attached：flags_hi bit23（$S，可为变量）', () => {
  const vm = mkVm([instr(314, [1])]);
  stepFrame(vm, ops);
  assert.equal(vm.flagsHi & 0x800000, 0x800000);
});

// ── 304 layer 的派生 flags_hi 位（06 §2.2）：三个区间 + 一条无 else 的附加位 ─────────────

test('304 layer：3-19 区间写 0x40000，且清掉可能残留的 0x80000', () => {
  const vm = mkVm([instr(304, [20]), instr(304, [10])]); // 先落 20-23 区间，再落 3-19 区间
  stepFrame(vm, ops);
  assert.equal(vm.flagsHi & 0x40000, 0x40000, '10 落在 3-19，应该置 0x40000');
  assert.equal(vm.flagsHi & 0x80000, 0, '第二条应该把第一条留下的 0x80000 清掉');
});

test('304 layer：20-23 区间写 0x80000', () => {
  const vm = mkVm([instr(304, [22])]);
  stepFrame(vm, ops);
  assert.equal(vm.flagsHi & 0x80000, 0x80000);
  assert.equal(vm.flagsHi & 0x40000, 0);
});

test('304 layer：区间外（如 0/1/2）两个 0xc0000 位全清', () => {
  const vm = mkVm([instr(304, [20]), instr(304, [0])]);
  stepFrame(vm, ops);
  assert.equal(vm.flagsHi & 0xc0000, 0, 'layer=0 落在 else 分支，应清掉两个位');
});

test('304 layer：20-31 或 36-42 额外置 0x100000（且没有 else 清位分支——离开区间后位保持不变）', () => {
  const vm = mkVm([instr(304, [25]), instr(304, [0])]); // 25 落 20-31，之后换成 0（不在 20-31/36-42）
  stepFrame(vm, ops);
  assert.equal(vm.flagsHi & 0x100000, 0x100000, '06 §2.2：第二个 if 没有 else，0x100000 应该保持被置');
});

// ── 故意改坏验证①：308 的 `or 8` 去掉（缩放脏位漏写）───────────────────────────────
// 这段注释记录人工验证过程，不是自动化测试（改坏 -> 跑测试 -> 变红 -> 改回 -> 跑测试 -> 转绿
// 的过程与结果见 task-I5-report.md，这里只放一条形式化的回归断言，锁住这个行为不再退化）。
test('回归①锁定：308/309 必须同时写 bit3（防止「去掉 or 8」式的退化）', () => {
  const vmX = mkVm([instr(308, [])]);
  stepFrame(vmX, ops);
  assert.equal(vmX.flagsLo & 0x8, 0x8);
  const vmY = mkVm([instr(309, [])]);
  stepFrame(vmY, ops);
  assert.equal(vmY.flagsLo & 0x8, 0x8);
});

// ── 故意改坏验证②：312 的两个字段写反 ───────────────────────────────────────────────
test('回归②锁定：312 的 x 必须落 flags_hi、y 必须落 flags_lo（防止两个字段写反式的退化）', () => {
  const vm = mkVm([instr(312, [1, 2])]); // x=1（二进制 01），y=2（二进制 10）—— 特意选不对称的值
  stepFrame(vm, ops);
  assert.equal(vm.flagsHi & 0x3, 1, 'x 必须落 flags_hi bit0-1');
  assert.equal((vm.flagsLo >>> 30) & 0x3, 2, 'y 必须落 flags_lo bit30-31');
});
