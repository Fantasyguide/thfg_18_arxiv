// 变量表测试：36 个变量号（10000–10035）的读/写，依据 engine/anm/th16/04-variables.md
// （下称「04」）§2 的一手结论。core.js 的 resolveArg（读）与 writeIntVar/writeFloatVar
// （写）是本文件测的对象——不测任何具体 opcode（那是 group-math.js 等 opcode 组的事，
// 见 tests/vm-math.test.js），只测变量表本身：22 个可写号真的接到了 vm 字段（不是影子
// 状态）、14 个只读号写入会带变量名报错、camera/lookat 的“不建模”会显式抛而不是静默
// 返回 0、跨类型读的非对称规则、两条 PRNG 流的划界。
import test from 'node:test';
import assert from 'node:assert/strict';
import { createVm, resolveArg, writeIntVar, writeFloatVar } from '../src/vm/core.js';

// 变量号按 float32 位模式编码（math-jumps §3 结论④）——resolveArg 的 kind='float' 分支
// 需要这个，直接复用 vm-core.test.js 同款小工具（不导入 core.js 内部私有的同名函数）。
function f32BitsOf(value) {
  const f = new Float32Array([value]);
  return new Uint32Array(f.buffer)[0];
}

function bareVm(opts) {
  return createVm({ instructions: [] }, opts);
}

// resolveArg 的 kind='int' 分支：raw 就是十进制变量号，不需要位编码。
function readInt(vm, varId) {
  return resolveArg(vm, varId, true, 'int');
}
function readFloat(vm, varId) {
  return resolveArg(vm, f32BitsOf(varId), true, 'float');
}

// ── 22 个可写号：读/写都落在真实 vm 字段上（04 §2「vm 字段」列），不是各存一份影子状态 ──

test('10008/10009（I4/I5）：写后从 vm.i4/vm.i5 读回，读侧同一份字段', () => {
  const vm = bareVm();
  writeIntVar(vm, 10008, 42);
  writeIntVar(vm, 10009, -7);
  assert.equal(vm.i4, 42, '落点是 vm.i4（04 §2 行 10008，`+0x4cc`），不是另存一份');
  assert.equal(vm.i5, -7, '落点是 vm.i5（04 §2 行 10009，`+0x4d0`）');
  assert.equal(readInt(vm, 10008), 42);
  assert.equal(readInt(vm, 10009), -7);
});

test('10013–10015（pos-x/y/z）：写后 vm.pos 跟着变（04 §2：落点是 `+0x2c`）', () => {
  const vm = bareVm();
  writeFloatVar(vm, 10013, 1.5);
  writeFloatVar(vm, 10014, -2.5);
  writeFloatVar(vm, 10015, 3.5);
  assert.equal(vm.pos.x, 1.5);
  assert.equal(vm.pos.y, -2.5);
  assert.equal(vm.pos.z, 3.5);
  assert.equal(readFloat(vm, 10013), 1.5);
});

test('10023–10025（rot-x/y/z）：写后 vm.rotation 跟着变（04 §2：落点是 `+0x38`）', () => {
  const vm = bareVm();
  writeFloatVar(vm, 10023, 0.1);
  writeFloatVar(vm, 10024, 0.2);
  writeFloatVar(vm, 10025, 0.3);
  assert.equal(Math.fround(vm.rotation.x), Math.fround(0.1));
  assert.equal(Math.fround(vm.rotation.y), Math.fround(0.2));
  assert.equal(Math.fround(vm.rotation.z), Math.fround(0.3));
});

test('10027–10029（三个随机参数）：写后 vm.randParamOne/Pi/Int 跟着变，且真的影响随机公式', () => {
  const vm = bareVm({});
  writeFloatVar(vm, 10027, 4); // rand-param-one
  assert.equal(vm.randParamOne, 4);
  vm.prng = () => 0x80000000; // randf01() = 0x80000000/2^32 = 0.5
  assert.equal(readFloat(vm, 10011), 2, '10011 = rand-param-one * randf01()，注入的 raw 让 randf01=0.5');
  writeFloatVar(vm, 10028, 8); // rand-param-pi
  assert.equal(vm.randParamPi, 8);
  writeIntVar(vm, 10029, 10); // rand-param-int
  assert.equal(vm.randParamInt, 10);
  vm.prng = () => 20; // raw % 10 == 0，验证 10022 整读真的用了 vm.randParamInt 当模数
  assert.equal(readInt(vm, 10022), 0);
});

test('10033–10035（mystery-angle-x/y/z）：语义待定（04 §8 RV2：通用浮点暂存），但读写落在真实字段', () => {
  const vm = bareVm();
  writeFloatVar(vm, 10033, 11);
  writeFloatVar(vm, 10034, 22);
  writeFloatVar(vm, 10035, 33);
  assert.equal(vm.mysteryVars.x, 11);
  assert.equal(vm.mysteryVars.y, 22);
  assert.equal(vm.mysteryVars.z, 33);
  assert.equal(readFloat(vm, 10035), 33);
});

test('vm 初值：三个随机参数默认 1.0/π/0x10000，mystery 默认 0（04 wipe 默认）', () => {
  const vm = bareVm();
  assert.equal(vm.randParamOne, 1.0);
  assert.equal(vm.randParamPi, Math.fround(Math.PI));
  assert.equal(vm.randParamInt, 0x10000);
  assert.equal(vm.mysteryVars.x, 0);
  assert.equal(vm.i4, 0);
  assert.equal(vm.i5, 0);
});

// ── 14 个只读号：写入必须抛错，且错误信息带变量名（brief 步骤 1 明确要求）───────────

test('只读变量写入抛错，且错误信息带变量名：10010（randrad，只读）', () => {
  const vm = bareVm();
  assert.throws(() => writeFloatVar(vm, 10010, 1), /10010.*randrad|randrad.*10010/);
});

test('只读变量写入抛错：10022（rand，整型只读）', () => {
  const vm = bareVm();
  assert.throws(() => writeIntVar(vm, 10022, 1), /只读/);
});

test('只读变量写入抛错：10026（rot-z-effective，只读）', () => {
  const vm = bareVm();
  assert.throws(() => writeFloatVar(vm, 10026, 1), /只读/);
});

test('只读变量写入抛错：camera-x（10016）与 lookat-z（10021）', () => {
  const vm = bareVm();
  assert.throws(() => writeFloatVar(vm, 10016, 1), /只读/);
  assert.throws(() => writeFloatVar(vm, 10021, 1), /只读/);
});

test('14 个只读号穷举：writeIntVar/writeFloatVar 全部抛错', () => {
  const READONLY = [
    10010, 10011, 10012,
    10016, 10017, 10018, 10019, 10020, 10021,
    10022, 10026,
    10030, 10031, 10032,
  ];
  assert.equal(READONLY.length, 14);
  const vm = bareVm();
  for (const varId of READONLY) {
    assert.throws(() => writeIntVar(vm, varId, 1), `writeIntVar(${varId}) 应该抛`);
    assert.throws(() => writeFloatVar(vm, varId, 1), `writeFloatVar(${varId}) 应该抛`);
  }
});

// ── 类型不匹配写：22 个可写号里，整型号只能整型写、浮点号只能浮点写（04 §3：两张
//   lvalue 表的可写集合不相交，用错类型的指令写会在真机上自修改字节码，本骨架拒绝执行） ──

test('类型不匹配写：用 writeFloatVar 写一个只整型可写的号（10000/i0）要抛，不能静默落地', () => {
  const vm = bareVm();
  assert.throws(() => writeFloatVar(vm, 10000, 1.5), /浮点左值写入|自修改字节码/);
  assert.equal(vm.i[0], 0, '拒绝执行，vm.i[0] 不应该被动过');
});

test('类型不匹配写：用 writeIntVar 写一个只浮点可写的号（10013/pos-x）要抛', () => {
  const vm = bareVm();
  assert.throws(() => writeIntVar(vm, 10013, 5), /整型左值写入|自修改字节码/);
  assert.equal(vm.pos.x, 0);
});

// ── camera/lookat（10016–10021）：播放器不建模的引擎全局态，浮点读显式抛，不静默返 0 ──

test('camera-x（10016）浮点读：不建模，显式抛错（不是静默返回 0）', () => {
  const vm = bareVm();
  assert.throws(() => readFloat(vm, 10016), /不建模|Stage/);
});

test('lookat-z（10021）浮点读：同上', () => {
  const vm = bareVm();
  assert.throws(() => readFloat(vm, 10021), /不建模|Stage/);
});

// ── 跨类型读的非对称规则（04 §4，穷举结论）───────────────────────────────────────

test('整型读只有浮点语义的号：返回变量号本身，不是字段值（04 §4③，19 个号之一：pos-x）', () => {
  const vm = bareVm();
  writeFloatVar(vm, 10013, 99.5);
  assert.equal(readInt(vm, 10013), 10013, '不是 99（Math.trunc(99.5)），是变量号本身');
});

test('整型读只有浮点语义的号：camera-x（10016）不需要真的碰相机，直接返回 10016', () => {
  const vm = bareVm();
  // 不注入任何相机数据，也不应该抛——整型读走的是「返回变量号本身」这条默认分支，
  // 04 §4：get_int_var 的索引表把这 19 个号全指向默认分支，压根不会执行浮点 case 的代码。
  assert.equal(readInt(vm, 10016), 10016);
});

test('浮点读 7 个整型存储号：cvtdq2ps 转换（04 §4①），既有的 8 槽行为不变', () => {
  const vm = bareVm();
  writeIntVar(vm, 10029, 5); // rand-param-int
  assert.equal(readFloat(vm, 10029), 5);
});

test('整型读 9 个浮点存储号：cvttss2si 截零（04 §4②），扩展到新槽：rand-param-one（10027）', () => {
  const vm = bareVm();
  writeFloatVar(vm, 10027, 3.75);
  assert.equal(readInt(vm, 10027), 3, 'cvttss2si 截零：3.75 -> 3');
});

test('既有的 I0–I3/F0–F3 跨类型读不受影响（回归）', () => {
  const vm = bareVm();
  vm.f[0] = 3.75;
  assert.equal(readInt(vm, 10004), 3);
  vm.i[0] = 7;
  assert.equal(readFloat(vm, 10000), 7);
});

// ── PRNG 两条流：Stream B（vm.prng，10010–10012/10022）与 Stream A（vm.prngReplay，
//   10030–10032）互相独立，缺一个不能拿另一个顶替 ────────────────────────────────

test('10010–10012（Stream B）没注入 vm.prng 时抛错，不静默退化', () => {
  const vm = bareVm();
  assert.throws(() => readFloat(vm, 10010), /vm\.prng\b/);
  assert.throws(() => readFloat(vm, 10011), /vm\.prng\b/);
  assert.throws(() => readFloat(vm, 10012), /vm\.prng\b/);
});

test('10030–10032（Stream A）没注入 vm.prngReplay 时抛错，即使 vm.prng 已经注入', () => {
  const vm = bareVm();
  vm.prng = () => 0; // Stream B 有了，Stream A 还是没有——不能互相顶替
  assert.throws(() => readFloat(vm, 10030), /vm\.prngReplay\b/);
});

test('10010（randrad）与 10030（randrad-replay）用各自的流，互不干扰', () => {
  const vm = bareVm();
  let bCalls = 0;
  let aCalls = 0;
  vm.prng = () => { bCalls++; return 0xc0000000; }; // randFpm1 = 0xc0000000/0x80000000-1 = 0.5
  vm.prngReplay = () => { aCalls++; return 0x40000000; }; // randFpm1 = -0.5
  const b = readFloat(vm, 10010);
  const a = readFloat(vm, 10030);
  assert.equal(bCalls, 1);
  assert.equal(aCalls, 1);
  assert.equal(b, Math.fround(vm.randParamPi * 0.5));
  assert.equal(a, Math.fround(vm.randParamPi * -0.5));
});

test('10022（rand）整读：raw % randParamInt，模数 0 短路成 0，不崩', () => {
  const vm = bareVm();
  vm.randParamInt = 0;
  vm.prng = () => { throw new Error('模数为 0 应该短路，不应该真的去 draw'); };
  assert.equal(readInt(vm, 10022), 0);
});

test('10022（rand）浮读：raw 当无符号整数转 float，不取模（04 §5「10022 的两面」）', () => {
  const vm = bareVm();
  vm.randParamInt = 3; // 故意设一个会让「取模」路径产生截然不同结果的模数
  vm.prng = () => 1000000;
  assert.equal(readFloat(vm, 10022), Math.fround(1000000), '浮读不取模，直接是 raw 本身（转 float）');
});

test('10022（rand）整读消耗 PRNG draw：连续两次读取不同（用计数器验证真的调用了 vm.prng）', () => {
  const vm = bareVm();
  vm.randParamInt = 100;
  let n = 0;
  vm.prng = () => (n++, n * 17);
  assert.equal(readInt(vm, 10022), 17);
  assert.equal(readInt(vm, 10022), 34);
});

// ── 10026（rot-z-effective）：本骨架无 VM 树，等价于「root==null」分支——wrap(自身 z) ──

test('10026（rot-z-effective）：无父级时 = wrap(vm.rotation.z)（04 §6.2）', () => {
  const vm = bareVm();
  vm.rotation.z = 1.0;
  assert.equal(readFloat(vm, 10026), Math.fround(1.0), '未超过 [-π,π]，wrap 是 no-op');
});

test('10026（rot-z-effective）：超出 [-π,π] 会被 wrap 回来（同 129 mathReduceAngle 算法）', () => {
  const vm = bareVm();
  vm.rotation.z = Math.fround(4); // > π，需要减一次 2π
  const result = readFloat(vm, 10026);
  assert.ok(result <= Math.fround(Math.PI) && result >= -Math.fround(Math.PI));
});

// ── 越界变量号：36 号定义域之外直接抛，不当成任何一种已知情形处理 ─────────────────

test('越界变量号（10036/9999）读写全部抛错', () => {
  const vm = bareVm();
  assert.throws(() => readInt(vm, 10036), /不是合法的 ANM 变量号/);
  assert.throws(() => readFloat(vm, 9999), /不是合法的 ANM 变量号/);
  assert.throws(() => writeIntVar(vm, 10036, 1), /不是合法的 ANM 变量号/);
  assert.throws(() => writeFloatVar(vm, 9999, 1), /不是合法的 ANM 变量号/);
});
