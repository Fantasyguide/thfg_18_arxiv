// 层 B · 几何生成测试。
// 依据：engine/anm/th16/10-sprite-geometry.md（「10」）、09-drawing.md §3.2–§3.5（「09」）。
//
// 这个模块的错误是**静默**的——顶点算错不会抛，只会画歪。所以这里一律断具体数值
// （角点坐标、UV、颜色、顶点个数），不断「函数存在」。
import test from 'node:test';
import assert from 'node:assert/strict';
import { createVm } from '../src/vm/core.js';
import { ensureGeneralState } from '../src/vm/group-general.js';
import { ensureChildState } from '../src/vm/group-child.js';
import {
  ANCHOR_X, ANCHOR_Y, build3d, buildArc, buildGeometry, buildRing,
  buildSpriteQuad, effectiveRotZ, finalPos, renderModeOf,
} from '../src/render/geometry.js';

const NOP = { time: 0, opcode: 0, paramMask: 0, args: [] };
// 单位 UV 四角（TL,TR,BL,BR），方便看清 uv_scale 的收缩方向
const UV = [[0, 0], [1, 0], [0, 1], [1, 1]];
const ctx = { spriteUv: UV };

function mkVm(mode = 0) {
  const vm = createVm({ instructions: [NOP] });
  vm.flagsLo = ((vm.flagsLo & ~0x3e000000) | ((mode << 25) >>> 0)) >>> 0;
  ensureGeneralState(vm);
  return vm;
}
const close = (a, b, eps = 1e-4) => Math.abs(a - b) < eps;
const xy = (v) => [v.x, v.y];

test('10 §2：两张锚点表与直读 PE 的值逐项一致', () => {
  assert.deepEqual(ANCHOR_X, [
    [-0.5, 0.5, -0.5, 0.5], [0.0, 1.0, 0.0, 1.0], [-1.0, 0.0, -1.0, 0.0], [-0.5, -0.5, 0.5, 0.5],
  ]);
  assert.deepEqual(ANCHOR_Y, [
    [-0.5, -0.5, 0.5, 0.5], [0.0, 0.0, 1.0, 1.0], [-1.0, -1.0, 0.0, 0.0], [0.0, 0.0, 0.0, 0.0],
  ]);
});

// ── sprite 四边形 ───────────────────────────────────────────────────────────────
test('sprite：中心锚点 + sprite_size(100,50) ⇒ 角序 TL/TR/BL/BR 在 (±50, ±25)', () => {
  const vm = mkVm(0);
  vm.spriteSize = { x: 100, y: 50 };
  const q = buildSpriteQuad(vm, ctx);
  assert.equal(q.vertices.length, 4);
  assert.deepEqual(xy(q.vertices[0]), [-50, -25], 'TL');
  assert.deepEqual(xy(q.vertices[1]), [50, -25], 'TR');
  assert.deepEqual(xy(q.vertices[2]), [-50, 25], 'BL');
  assert.deepEqual(xy(q.vertices[3]), [50, 25], 'BR');
});

test('sprite：锚点 h=左 / v=顶 ⇒ 四边形整体落在第四象限（0..w, 0..h）', () => {
  const vm = mkVm(0);
  vm.spriteSize = { x: 100, y: 50 };
  vm.flagsLo = ((vm.flagsLo | (1 << 21) | (1 << 23)) >>> 0);
  const q = buildSpriteQuad(vm, ctx);
  assert.deepEqual(xy(q.vertices[0]), [0, 0], 'TL 落在原点');
  assert.deepEqual(xy(q.vertices[3]), [100, 50], 'BR 落在 (w,h)');
});

test('sprite：锚点 h=右 / v=底 ⇒ 落在第二象限（-w..0, -h..0）', () => {
  const vm = mkVm(0);
  vm.spriteSize = { x: 100, y: 50 };
  vm.flagsLo = ((vm.flagsLo | (2 << 21) | (2 << 23)) >>> 0);
  const q = buildSpriteQuad(vm, ctx);
  assert.deepEqual(xy(q.vertices[0]), [-100, -50]);
  assert.deepEqual(xy(q.vertices[3]), [0, 0]);
});

test('sprite：anchor_offset 是**减**（10 §0 ②），不是加', () => {
  const vm = mkVm(0);
  vm.spriteSize = { x: 100, y: 50 };
  ensureGeneralState(vm).anchorOffset = { x: 10, y: 4 };
  const q = buildSpriteQuad(vm, ctx);
  assert.deepEqual(xy(q.vertices[0]), [-60, -29], '-50-10, -25-4');
});

test('sprite：有效缩放 = scale × scale_2（两者都参与，不是只用 scale）', () => {
  const vm = mkVm(0);
  vm.spriteSize = { x: 100, y: 50 };
  vm.scale = { x: 2, y: 3 };
  ensureGeneralState(vm).scale2 = { x: 0.5, y: 2 };
  const q = buildSpriteQuad(vm, ctx);
  assert.deepEqual(xy(q.vertices[3]), [50 * 2 * 0.5, 25 * 3 * 2], '(50,150)');
});

test('sprite：mode 0 与 mode 2 顶点位置完全相同（10 §1 ④）', () => {
  const mk = (m) => {
    const vm = mkVm(m);
    vm.spriteSize = { x: 100, y: 50 };
    vm.rotation.z = 1.0; // 就算给了旋转，这两个 mode 也不该转
    return buildSpriteQuad(vm, ctx).vertices.map(xy);
  };
  assert.deepEqual(mk(0), mk(2));
  assert.deepEqual(mk(0)[0], [-50, -25], '而且确实没转');
});

test('sprite：mode 1/3 绕有效 z 旋转 —— 转 90° 后 TL 从 (-50,-25) 到 (25,-50)', () => {
  const vm = mkVm(1);
  vm.spriteSize = { x: 100, y: 50 };
  vm.rotation.z = Math.PI / 2;
  const q = buildSpriteQuad(vm, ctx);
  // x' = x·cos − y·sin = 0 − (−25)(1) = 25 ; y' = x·sin + y·cos = −50 + 0 = −50
  assert.ok(close(q.vertices[0].x, 25), `TL.x 应 ≈ 25，实得 ${q.vertices[0].x}`);
  assert.ok(close(q.vertices[0].y, -50), `TL.y 应 ≈ -50，实得 ${q.vertices[0].y}`);
  const q3 = buildSpriteQuad(mkVm(3), ctx); // mode 3 与 mode 1 同 handler
  assert.equal(q3.mode, 3);
});

test('sprite：z 是四角共用的 pos.z + pos_2.z + entity_pos.z', () => {
  const vm = mkVm(0);
  vm.pos.z = 1;
  const g = ensureGeneralState(vm);
  g.pos2.z = 2;
  g.entityPos.z = 4;
  const q = buildSpriteQuad(vm, ctx);
  assert.deepEqual(q.vertices.map((v) => v.z), [7, 7, 7, 7]);
});

test('sprite 的 z **不是** final_pos.z —— 挂根时不含根的贡献（10 §3）', () => {
  // `th16:0x465fbd` 那段确实往 z 上加过 final_pos.z，但 `th16:0x466070` 随后用
  // `entity_pos.z + pos.z + pos_2.z` **整个覆盖**——死写。挂根时两者才分得开：
  // final_pos.z 会递归加上根的 z，而顶点 z 不会。
  const root = mkVm(0);
  root.pos.z = 1000;
  const vm = mkVm(0);
  vm.pos.z = 1;
  ensureGeneralState(vm).pos2.z = 2;
  ensureGeneralState(vm).entityPos.z = 4;
  ensureChildState(vm).root = root;
  assert.equal(finalPos(vm, ctx).z, 1007, 'final_pos.z 含根的 1000');
  const q = buildSpriteQuad(vm, ctx);
  assert.deepEqual(q.vertices.map((v) => v.z), [7, 7, 7, 7], '顶点 z 只有自己的三项，不含根');
});

test('10 §5：423 colorMode 的四种四角配色', () => {
  const c1 = 0x11223344;
  const c2 = 0x55667788;
  const run = (m) => {
    const vm = mkVm(0);
    vm.color = c1;
    ensureGeneralState(vm).color2 = c2;
    vm.flagsLo = ((vm.flagsLo & ~0x60000) | ((m << 17) >>> 0)) >>> 0;
    return buildSpriteQuad(vm, ctx).vertices.map((v) => v.color >>> 0);
  };
  assert.deepEqual(run(0), [c1, c1, c1, c1], '0 = 平涂 color_1');
  assert.deepEqual(run(1), [c2, c2, c2, c2], '1 = 平涂 color_2');
  assert.deepEqual(run(2), [c1, c2, c1, c2], '2 = 水平渐变（左 c1 → 右 c2）');
  assert.deepEqual(run(3), [c1, c1, c2, c2], '3 = 垂直渐变（上 c1 → 下 c2）');
});

test('10 §4：uv_scale 的收缩恒以 TL 为锚（ku 只动 TR/BR、kv 只动 BL/BR）', () => {
  const vm = mkVm(0);
  ensureGeneralState(vm).uvScale = { x: 0.5, y: 0.25 };
  const q = buildSpriteQuad(vm, ctx);
  const u = q.vertices.map((v) => v.u);
  const v = q.vertices.map((x) => x.v);
  assert.deepEqual(u, [0, 0.5, 0, 0.5], 'TL/BL 的 u 不动，TR/BR 收到一半');
  assert.deepEqual(v, [0, 0, 0.25, 0.25], 'TL/TR 的 v 不动，BL/BR 收到四分之一');
  // 若实现成「以中心缩」，TL 的 u 会变成 0.25——这条就是守它
  assert.equal(u[0], 0, 'TL 是锚，永远不动');
});

test('10 §4：uv_scroll_pos 是四角统一平移', () => {
  const vm = mkVm(0);
  ensureGeneralState(vm).uvScrollPos = { x: 0.125, y: -0.5 };
  const q = buildSpriteQuad(vm, ctx);
  assert.deepEqual(q.vertices.map((v) => v.u), [0.125, 1.125, 0.125, 1.125]);
  assert.deepEqual(q.vertices.map((v) => v.v), [-0.5, -0.5, 0.5, 0.5]);
});

test('sprite：缺 ctx.spriteUv 时显式抛（宿主必须提供，骨架没有贴图表）', () => {
  assert.throws(() => buildSpriteQuad(mkVm(0), {}), /ctx\.spriteUv/);
  assert.throws(() => buildSpriteQuad(mkVm(0), { spriteUv: [[0, 0]] }), /ctx\.spriteUv/);
});

// ── final_pos（`th16:0x406c40` + `th16:0x406a70`）─────────────────────────────────
test('final_pos = pos + pos_2 + entity_pos（三项，不是两项）', () => {
  const vm = mkVm(0);
  vm.pos = { x: 1, y: 2, z: 3 };
  const g = ensureGeneralState(vm);
  g.pos2 = { x: 10, y: 20, z: 30 };
  g.entityPos = { x: 100, y: 200, z: 300 };
  assert.deepEqual(finalPos(vm, ctx), { x: 111, y: 222, z: 333 });
});

test('final_pos：挂根时递归加上根的 final_pos', () => {
  const root = mkVm(0);
  root.pos = { x: 1000, y: 2000, z: 0 };
  const child = mkVm(0);
  child.pos = { x: 1, y: 2, z: 0 };
  ensureChildState(child).root = root;
  assert.deepEqual(finalPos(child, ctx), { x: 1001, y: 2002, z: 0 });
});

test('final_pos：314 attached（flags_hi bit23）时先绕根的 rotation.z 转 xy', () => {
  const root = mkVm(0);
  root.rotation.z = Math.PI / 2;
  const child = mkVm(0);
  child.pos = { x: 10, y: 0, z: 0 };
  ensureChildState(child).root = root;
  const without = finalPos(child, ctx);
  assert.ok(close(without.x, 10) && close(without.y, 0), '没置 bit23 就不转');
  child.flagsHi = (child.flagsHi | 0x800000) >>> 0;
  const withRot = finalPos(child, ctx);
  assert.ok(close(withRot.x, 0), `转 90° 后 x 应 ≈ 0，实得 ${withRot.x}`);
  assert.ok(close(withRot.y, 10), `转 90° 后 y 应 ≈ 10，实得 ${withRot.y}`);
});

test('final_pos：ignoreParent（flags_hi bit16）关掉整条根继承', () => {
  const root = mkVm(0);
  root.pos = { x: 1000, y: 0, z: 0 };
  const child = mkVm(0);
  child.pos = { x: 1, y: 0, z: 0 };
  ensureChildState(child).root = root;
  child.flagsHi = (child.flagsHi | 0x10000) >>> 0;
  assert.equal(finalPos(child, ctx).x, 1);
});

// ── 圆环族 ──────────────────────────────────────────────────────────────────────
test('mode 9：顶点数 = 2·I0，末尾那对是第 0/1 对的位置副本（环闭合）', () => {
  const vm = mkVm(9);
  vm.i[0] = 5;
  vm.i[1] = 1;
  vm.scale = { x: 4, y: 20 }; // 环宽 4、中心半径 20
  const g = buildRing(vm, ctx);
  assert.equal(g.vertices.length, 10, '2 × I0');
  assert.equal(g.primitive, 'trianglestrip');
  assert.ok(close(g.vertices[8].x, g.vertices[0].x) && close(g.vertices[8].y, g.vertices[0].y), '闭合顶点与第 0 个同位置');
  assert.ok(close(g.vertices[9].x, g.vertices[1].x), '同上，内圈那个');
  assert.notEqual(g.vertices[8].v, g.vertices[0].v, '但 v 被改写成累计终值');
  assert.ok(close(g.vertices[8].v, 1), 'v 走满 I1 = 1');
});

test('09 §3.2：scale.y = 中心半径、scale.x = 环宽 ⇒ 外 22 / 内 18', () => {
  const vm = mkVm(9);
  vm.i[0] = 5;
  vm.i[1] = 1;
  vm.scale = { x: 4, y: 20 };
  vm.rotation.z = 0; // 起点在 +x 轴
  const g = buildRing(vm, ctx);
  assert.ok(close(g.vertices[0].x, 22), `外半径应 20+2=22，实得 ${g.vertices[0].x}`);
  assert.ok(close(g.vertices[1].x, 18), `内半径应 20-2=18，实得 ${g.vertices[1].x}`);
});

test('mode 9：内外可用不同颜色（flags_lo & 0x60000 时内圈取 color_2）', () => {
  const vm = mkVm(9);
  vm.i[0] = 3; vm.i[1] = 1; vm.color = 0xaaaaaaaa;
  ensureGeneralState(vm).color2 = 0xbbbbbbbb;
  let g = buildRing(vm, ctx);
  assert.equal(g.vertices[1].color >>> 0, 0xaaaaaaaa, '没置位时内圈也是 color_1');
  vm.flagsLo = (vm.flagsLo | 0x20000) >>> 0;
  g = buildRing(vm, ctx);
  assert.equal(g.vertices[0].color >>> 0, 0xaaaaaaaa, '外圈恒 color_1');
  assert.equal(g.vertices[1].color >>> 0, 0xbbbbbbbb, '内圈换 color_2');
});

test('09 §3.2：601（mode 13）以基准角居中、602（mode 14）从基准角起', () => {
  const mk = (mode) => {
    const vm = mkVm(mode);
    vm.i[0] = 3; vm.i[1] = 1;
    vm.scale = { x: 0, y: 10 }; // 环宽 0 ⇒ 内外同半径，只看角度
    vm.rotation.x = Math.PI / 2; // 总弧角 90°
    vm.rotation.z = 0; // 基准角 0
    return buildArc(vm, ctx);
  };
  const a602 = mk(14);
  // 602：起始角 = 0 ⇒ 第一个点在 (10, 0)
  assert.ok(close(a602.vertices[0].x, 10) && close(a602.vertices[0].y, 0), '602 从基准角起');
  const a601 = mk(13);
  // 601：起始角 = 0 − 90°/2 = −45° ⇒ 第一个点在 (10cos45, −10sin45)
  assert.ok(close(a601.vertices[0].x, 10 * Math.SQRT1_2), `601 起始角应 -45°，实得 x=${a601.vertices[0].x}`);
  assert.ok(close(a601.vertices[0].y, -10 * Math.SQRT1_2));
});

test('09 §3.2：弧走满 rotation.x，末对正好落在「起始角 + 总弧角」', () => {
  const vm = mkVm(14);
  vm.i[0] = 5; vm.i[1] = 1;
  vm.scale = { x: 0, y: 10 };
  vm.rotation.x = Math.PI / 2;
  vm.rotation.z = 0;
  const g = buildArc(vm, ctx);
  assert.equal(g.vertices.length, 10, '2·I0，且**不补**闭合顶点');
  assert.ok(close(g.vertices[8].x, 0, 1e-3), `末对应在 90°，实得 x=${g.vertices[8].x}`);
  assert.ok(close(g.vertices[8].y, 10, 1e-3));
});

test('09 §3.2：弧族内外两个顶点同色（与 mode 9 不同）', () => {
  const vm = mkVm(13);
  vm.i[0] = 3; vm.i[1] = 1; vm.color = 0xaaaaaaaa;
  ensureGeneralState(vm).color2 = 0xbbbbbbbb;
  vm.flagsLo = (vm.flagsLo | 0x20000) >>> 0;
  const g = buildArc(vm, ctx);
  assert.equal(g.vertices[0].color >>> 0, 0xbbbbbbbb);
  assert.equal(g.vertices[1].color >>> 0, 0xbbbbbbbb, '内外同色 —— 本族没有径向渐变');
});

// ── 3D 族 ──────────────────────────────────────────────────────────────────────
test('09 §3.4：mode 24 = 圆柱（两圈同半径 F2、高 ±F1/2）', () => {
  const vm = mkVm(24);
  vm.i[0] = 4; vm.i[1] = 1;
  vm.f = [Math.PI * 2, 8, 10, 0]; // F0 总角、F1 高、F2 半径、F3 中心角
  const g = build3d(vm, ctx);
  assert.equal(g.space, 'model', '模型坐标，不加 final_pos');
  assert.equal(g.vertices.length, 8);
  const [a, b] = g.vertices;
  assert.ok(close(Math.hypot(a.x, a.z), 10), `上圈半径应 = F2 = 10，实得 ${Math.hypot(a.x, a.z)}`);
  assert.ok(close(Math.hypot(b.x, b.z), 10), '下圈半径也是 F2');
  assert.equal(a.y, 4, '上圈 y = +F1/2');
  assert.equal(b.y, -4, '下圈 y = -F1/2');
});

test('09 §3.4：mode 25 = 平面圆环（两圈同在 y=0、半径 F2∓F1/2）', () => {
  const vm = mkVm(25);
  vm.i[0] = 4; vm.i[1] = 1;
  vm.f = [Math.PI * 2, 8, 10, 0];
  const g = build3d(vm, ctx);
  const [a, b] = g.vertices;
  assert.equal(a.y, 0, '压平');
  // ★ 下圈的 y 是 `-0`，不是 `0`——真机 `th16:0x4639eb` 用 `XORPS xmm0, [0x494890]`
  // （四个 0x80000000）翻符号位，对 `0.0f` 得到的正是 `-0.0f`。JS 的 Math.fround(-0) 同样
  // 保留负零，assert/strict 用 Object.is 会把两者判成不等。数值上等价，这里显式接受 -0
  // 而不是把实现改成 `h === 0 ? 0 : -h`——那才是偏离真机。
  assert.ok(Object.is(b.y, -0) || b.y === 0, `下圈 y 应为 0 或 -0，实得 ${b.y}`);
  assert.ok(close(Math.hypot(a.x, a.z), 6), `内半径应 10-4=6，实得 ${Math.hypot(a.x, a.z)}`);
  assert.ok(close(Math.hypot(b.x, b.z), 14), `外半径应 10+4=14，实得 ${Math.hypot(b.x, b.z)}`);
});

test('09 §3.4：圆在 XZ 平面 —— 起点 F3=0 落在 +x，走 90° 落在 +z', () => {
  const vm = mkVm(24);
  vm.i[0] = 2; vm.i[1] = 1;
  vm.f = [Math.PI / 2, 0, 10, Math.PI / 4]; // 总角 90°、中心角 45° ⇒ 起始角 0
  const g = build3d(vm, ctx);
  assert.ok(close(g.vertices[0].x, 10, 1e-3) && close(g.vertices[0].z, 0, 1e-3), '起点在 +x');
  assert.ok(close(g.vertices[2].x, 0, 1e-3) && close(g.vertices[2].z, 10, 1e-3), '90° 后在 +z');
});

// ── 分发 ────────────────────────────────────────────────────────────────────────
test('buildGeometry：按 render mode 分发，未覆盖的显式抛', () => {
  const q = buildGeometry(mkVm(0), ctx);
  assert.equal(q.primitive, 'quad');
  assert.equal(buildGeometry(mkVm(9), Object.assign({}, ctx)).mode, 9);
  for (const m of [4, 8, 15, 16, 17, 20]) {
    assert.throws(() => buildGeometry(mkVm(m), ctx), /未实现/, `mode ${m} 应抛`);
  }
});

test('renderModeOf / effectiveRotZ 的基本契约', () => {
  assert.equal(renderModeOf(mkVm(25)), 25);
  const root = mkVm(0);
  root.rotation.z = 0.5;
  const child = mkVm(0);
  child.rotation.z = 0.25;
  ensureChildState(child).root = root;
  assert.ok(close(effectiveRotZ(child), 0.75), '自身 + 根');
  child.flagsHi = (child.flagsHi | 0x10000) >>> 0;
  assert.ok(close(effectiveRotZ(child), 0.25), 'ignoreParent 关闭累加');
});
