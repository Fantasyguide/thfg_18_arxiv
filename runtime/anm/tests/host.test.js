// 层 B · 最小宿主测试。
// 依据：01-format.md §4/§5（全局序号）、02-vm-core.md §3.5、08-child.md §5、06-settings.md §2.1。
import test from 'node:test';
import assert from 'node:assert/strict';
import { createManager, createOps, flattenFile } from '../src/host/manager.js';
import { ensureChildState } from '../src/vm/group-child.js';

const ins = (opcode, args = [], { time = 0, paramMask = 0 } = {}) => ({ time, opcode, paramMask, args });
const f32b = (n) => { const a = new Float32Array([n]); return new Uint32Array(a.buffer)[0]; };

// 手搭一个「已摊平」的 file，省掉造二进制的麻烦（flattenFile 单独测）。
function mkFile(scripts, { spriteCount = 2, texSize = 64 } = {}) {
  const sprites = [];
  for (let i = 0; i < spriteCount; i++) {
    const u0 = (i * 8) / texSize, v0 = 0, u1 = (i * 8 + 8) / texSize, v1 = 16 / texSize;
    sprites.push({ id: i, x: i * 8, y: 0, w: 8, h: 16, entryIndex: 0, uv: [[u0, v0], [u1, v0], [u0, v1], [u1, v1]] });
  }
  return {
    entries: [{ name: 'test', width: texSize, height: texSize, sprites, scripts }],
    textures: [{ width: texSize, height: texSize, rgba: new Uint8Array(texSize * texSize * 4), format: 1, bpp: 4, size: 0 }],
    sprites,
    scripts: scripts.map((script, i) => ({ script, entryIndex: 0, indexInEntry: i, id: i })),
  };
}

test('createOps：六组合起来正好 135 条，且互不冲突', () => {
  assert.equal(Object.keys(createOps()).length, 135);
});

// ── 01 §4/§5：全局序号 ──────────────────────────────────────────────────────────
test('flattenFile：sprite 与 script 都按「跨 entry 的运行序号」摊平，不看记录里的 id', () => {
  const parsed = {
    entries: [
      { name: 'a', width: 32, height: 32, thtxOffset: null, sprites: [{ id: 0, x: 0, y: 0, w: 8, h: 8 }, { id: 1, x: 8, y: 0, w: 8, h: 8 }], scripts: [{ id: -72, instructions: [] }] },
      { name: 'b', width: 16, height: 16, thtxOffset: null, sprites: [{ id: 999, x: 0, y: 0, w: 4, h: 4 }], scripts: [{ id: -71, instructions: [] }, { id: -70, instructions: [] }] },
    ],
  };
  const file = flattenFile(new Uint8Array(0), parsed);
  assert.equal(file.sprites.length, 3, '两个 entry 的 sprite 接在一起');
  assert.equal(file.sprites[2].entryIndex, 1, '第 3 个 sprite 属于 entry 1');
  assert.equal(file.sprites[2].id, 999, 'id 如实保留（只作显示用）');
  assert.equal(file.scripts.length, 3);
  // ★ script 的 id 会真的和序号对不上（e01–e08 的 id 是 −72…−1）——取脚本一律用序号
  assert.deepEqual(file.scripts.map((s) => s.id), [-72, -71, -70]);
  assert.deepEqual(file.scripts.map((s) => s.entryIndex), [0, 1, 1]);
});

test('flattenFile：UV 用 entry 各自的贴图尺寸算，角序 TL/TR/BL/BR', () => {
  const parsed = {
    entries: [{ name: 'a', width: 64, height: 32, thtxOffset: null, sprites: [{ id: 0, x: 16, y: 8, w: 32, h: 16 }], scripts: [] }],
  };
  const { sprites } = flattenFile(new Uint8Array(0), parsed);
  assert.deepEqual(sprites[0].uv, [[0.25, 0.25], [0.75, 0.25], [0.25, 0.75], [0.75, 0.75]]);
});

// ── set_sprite 的宿主侧落地 ────────────────────────────────────────────────────
test('300 sprite 会当场把贴图像素尺寸写进 sprite_size（`th16:0x45ee36`）', () => {
  const file = mkFile([{ instructions: [ins(300, [1]), ins(0, [], { time: 99 })] }]);
  const m = createManager(file);
  const vm = m.spawn(0);
  m.step();
  assert.equal(vm.spriteId, 1);
  assert.equal(vm.spriteSize.x, 8, 'sprite 的 w');
  assert.equal(vm.spriteSize.y, 16, 'sprite 的 h');
  assert.equal(vm._host.spriteRef, file.sprites[1]);
});

test('★ 同帧里 603 drawRect 改写 sprite_size 不会被宿主冲掉（两者共用同一对槽）', () => {
  // 09 §5(2)：`sprite_size` 是「一对槽两个写者」。宿主若把 sprite 解析挪到帧尾统一刷，
  // 就会把 603 写的 w/h 冲掉——所以它必须在 opcode 里做。这条守的就是那个顺序。
  const file = mkFile([{ instructions: [ins(300, [1]), ins(603, [f32b(640), f32b(480)]), ins(0, [], { time: 99 })] }]);
  const m = createManager(file);
  const vm = m.spawn(0);
  m.step();
  assert.equal(vm.spriteSize.x, 640, '603 后写的赢');
  assert.equal(vm.spriteSize.y, 480);
});

test('sprite(-1)：不抛，标成系统贴图，drawList 里跳过并给出原因', () => {
  // 真实数据 4683 条 300 里有 80 条是 -1（全部恰好 -1）。settingsGroup[300] 对负数是显式抛
  // （「组不建模系统贴图」的诚实表态），宿主必须自己拦下来，否则真实文件一放就炸。
  const file = mkFile([{ instructions: [ins(300, [-1]), ins(0, [], { time: 99 })] }]);
  const m = createManager(file);
  const vm = m.spawn(0);
  // ★ 先把可见位清掉：createVm 的默认 flags_lo = 0x7 本来就带 bit0，不清的话
  // 下面那条断言恒真、对「handler 有没有置位」零区分力（第一版就是这样，故意破坏红 0 条）。
  vm.flagsLo = (vm.flagsLo & ~1) >>> 0;
  assert.doesNotThrow(() => m.step());
  assert.equal(vm.spriteId, -1);
  assert.notEqual(vm.flagsLo & 1, 0, '可见位照置（06 §2.1）');
  assert.equal(m.drawList().length, 0);
  assert.match(m.skipped[0].reason, /sprite\(-1\)/);
});

test('sprite 号越界：记下来并跳过，不编一个', () => {
  const file = mkFile([{ instructions: [ins(300, [999]), ins(0, [], { time: 99 })] }]);
  const m = createManager(file);
  const vm = m.spawn(0);
  m.step();
  assert.equal(vm._host.badSprite, 999);
  assert.equal(m.drawList().length, 0);
});

// ── 帧循环 ─────────────────────────────────────────────────────────────────────
test('step：跑到脚本末尾的 VM 被回收出列表', () => {
  const file = mkFile([{ instructions: [ins(300, [0]), ins(1, [])] }]); // 1 = delete
  const m = createManager(file);
  const vm = m.spawn(0);
  assert.equal(m.aliveCount, 1);
  m.step();
  assert.equal(vm.alive, false);
  assert.equal(m.vms.length, 0, '死掉的从列表里摘掉');
});

test('step：500 建出来的子 VM 会被收编进列表（走子链发现，08 §5 的 prepend）', () => {
  const file = mkFile([
    { instructions: [ins(500, [1]), ins(0, [], { time: 99 })] }, // 父：建子跑 script 1
    { instructions: [ins(300, [0]), ins(0, [], { time: 99 })] }, // 子
  ]);
  const m = createManager(file);
  const parent = m.spawn(0);
  m.step();
  assert.equal(m.vms.length, 2, '子被收编');
  const child = ensureChildState(parent).children[0];
  assert.ok(m.vms.includes(child));
  // 「创建即 tick」：子在被建出来时已经跑过一帧，所以它的 300 已经生效
  assert.equal(child.spriteId, 0);
  assert.equal(child.spriteSize.x, 8, '子的 sprite 也被宿主解析了');
});

test('step：父死时整棵子树一起销毁（`th16:0x46e660` 子先于己）', () => {
  const file = mkFile([
    { instructions: [ins(500, [1]), ins(1, [])] }, // 父：建子后立刻 delete
    { instructions: [ins(0, [], { time: 99 })] },
  ]);
  const m = createManager(file);
  const parent = m.spawn(0);
  m.step();
  assert.equal(parent.alive, false);
  assert.equal(m.vms.length, 0, '父和子都被摘掉');
});

// ── 画什么 ─────────────────────────────────────────────────────────────────────
test('drawList：按 layer 升序稳定排序（304 layer 写 vm.layer）', () => {
  const file = mkFile([
    { instructions: [ins(300, [0]), ins(304, [20]), ins(0, [], { time: 99 })] },
    { instructions: [ins(300, [0]), ins(304, [3]), ins(0, [], { time: 99 })] },
    { instructions: [ins(300, [0]), ins(304, [20]), ins(0, [], { time: 99 })] },
  ]);
  const m = createManager(file);
  const a = m.spawn(0), b = m.spawn(1), c = m.spawn(2);
  m.step();
  const order = m.drawList().map((d) => d.vm);
  assert.deepEqual(order, [b, a, c], 'layer 3 在前；同 layer 保持登记顺序');
});

test('drawList：可见位没置就不画（1 delete / 4 stopHide 都会清它）', () => {
  const file = mkFile([{ instructions: [ins(300, [0]), ins(0, [], { time: 99 })] }]);
  const m = createManager(file);
  const vm = m.spawn(0);
  m.step();
  assert.equal(m.drawList().length, 1);
  vm.flagsLo = (vm.flagsLo & ~1) >>> 0;
  assert.equal(m.drawList().length, 0);
});

test('drawList：未实现的渲染模式收进 skipped 而不是抛（播放器要放得动真实文件）', () => {
  // 302 renderMode(8) —— mode 8 走 `th16:0x467410`，还没逆（B6）。真实数据里用了 196 次。
  const file = mkFile([{ instructions: [ins(300, [0]), ins(302, [8]), ins(0, [], { time: 99 })] }]);
  const m = createManager(file);
  m.spawn(0);
  m.step();
  const list = m.drawList();
  assert.equal(list.length, 0);
  assert.equal(m.skipped.length, 1);
  assert.equal(m.skipped[0].mode, 8);
  assert.match(m.skipped[0].reason, /未实现/);
});

test('drawList：正常 sprite 出一份 quad 几何 + 对应 entry 的贴图', () => {
  const file = mkFile([{ instructions: [ins(300, [1]), ins(0, [], { time: 99 })] }]);
  const m = createManager(file);
  m.step.call(null); // 无副作用调用，确保 spawn 前 step 不炸
  const vm = m.spawn(0);
  m.step();
  const [d] = m.drawList();
  assert.equal(d.vm, vm);
  assert.equal(d.geometry.primitive, 'quad');
  assert.equal(d.geometry.vertices.length, 4);
  assert.equal(d.texture, file.textures[0]);
  // 中心锚点 + sprite 8×16 ⇒ 角在 (±4, ±8)
  assert.deepEqual([d.geometry.vertices[0].x, d.geometry.vertices[0].y], [-4, -8]);
  // UV 取的是该 sprite 的四角
  assert.deepEqual([d.geometry.vertices[0].u, d.geometry.vertices[0].v], file.sprites[1].uv[0]);
});
