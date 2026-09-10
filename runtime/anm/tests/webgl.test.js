// 层 B · WebGL 后端测试。用**记录调用的 mock gl**——这个模块的价值在于「设了哪些状态、
// 设了几次、顶点怎么摆」，这些用 mock 断言比真开一个 GL 上下文更精确，也能在 Node 里跑。
import test from 'node:test';
import assert from 'node:assert/strict';
import { createRenderer, unpackD3dColor, uploadTexture, VERTEX_SHADER, FRAGMENT_SHADER } from '../src/render/webgl.js';

// ── mock gl ────────────────────────────────────────────────────────────────────
const ENUM_NAMES = [
  'BLEND', 'DEPTH_TEST', 'CULL_FACE', 'TEXTURE_2D', 'TEXTURE_MAG_FILTER', 'TEXTURE_MIN_FILTER',
  'TEXTURE_WRAP_S', 'TEXTURE_WRAP_T', 'ARRAY_BUFFER', 'DYNAMIC_DRAW', 'FLOAT', 'TRIANGLES',
  'VERTEX_SHADER', 'FRAGMENT_SHADER', 'LINK_STATUS', 'COMPILE_STATUS', 'RGBA', 'UNSIGNED_BYTE',
  'SRC_ALPHA', 'ONE_MINUS_SRC_ALPHA', 'ONE', 'ZERO', 'FUNC_ADD', 'FUNC_REVERSE_SUBTRACT',
  'ONE_MINUS_DST_COLOR', 'ONE_MINUS_SRC_COLOR', 'DST_COLOR', 'DST_ALPHA', 'ONE_MINUS_DST_ALPHA',
  'MIN', 'MAX', 'NEAREST', 'LINEAR', 'REPEAT', 'CLAMP_TO_EDGE', 'MIRRORED_REPEAT',
];

function mockGl({ minMax = true } = {}) {
  const calls = [];
  const gl = { _calls: calls };
  ENUM_NAMES.forEach((n, i) => { gl[n] = 0x1000 + i; });
  const rec = (name) => (...args) => { calls.push([name, ...args]); };
  for (const m of ['useProgram', 'enable', 'disable', 'uniform1i', 'uniform2f', 'uniform1f',
    'uniformMatrix4fv', 'bindTexture', 'texParameteri', 'bindBuffer', 'bufferData',
    'enableVertexAttribArray', 'vertexAttribPointer', 'drawArrays', 'deleteBuffer',
    'deleteProgram', 'attachShader', 'linkProgram', 'shaderSource', 'compileShader',
    'blendFunc', 'blendEquation', 'texImage2D']) gl[m] = rec(m);
  gl.createBuffer = () => ({ tag: 'buf' });
  gl.createTexture = () => ({ tag: 'tex' });
  gl.createShader = () => ({ tag: 'shader' });
  gl.createProgram = () => ({ tag: 'program' });
  gl.getAttribLocation = (p, n) => ENUM_NAMES.length + n.length;
  gl.getUniformLocation = (p, n) => ({ uniform: n });
  gl.getProgramParameter = () => true;
  gl.getShaderParameter = () => true;
  gl.getExtension = (n) => (n === 'EXT_blend_minmax' ? (minMax ? {} : null) : null);
  return gl;
}
const of = (gl, name) => gl._calls.filter((c) => c[0] === name);
const nameOf = (gl, v) => ENUM_NAMES.find((n) => gl[n] === v) ?? v;

const vm = (lo = 0, hi = 0) => ({ flagsLo: lo >>> 0, flagsHi: hi >>> 0 });
const quad = (space = 'screen') => ({
  space, primitive: 'quad',
  vertices: [0, 1, 2, 3].map((i) => ({ x: i, y: i * 2, z: 0, color: 0xffffffff, u: i, v: i })),
});

test('D3DCOLOR 字节序：02 §3.3 的 B,G,R,A 小端读成 u32 = 0xAARRGGBB', () => {
  assert.deepEqual(unpackD3dColor(0xff000000), [0, 0, 0, 1], '只有 A');
  assert.deepEqual(unpackD3dColor(0x00ff0000), [1, 0, 0, 0], '只有 R');
  assert.deepEqual(unpackD3dColor(0x0000ff00), [0, 1, 0, 0], '只有 G');
  assert.deepEqual(unpackD3dColor(0x000000ff), [0, 0, 1, 0], '只有 B');
});

test('createRenderer 做 09 §4.1 的全局重置：开 BLEND、关深度、关剔除', () => {
  const gl = mockGl();
  createRenderer(gl);
  assert.deepEqual(of(gl, 'enable').map((c) => nameOf(gl, c[1])), ['BLEND'], 'ALPHABLENDENABLE=1');
  const dis = of(gl, 'disable').map((c) => nameOf(gl, c[1]));
  assert.ok(dis.includes('DEPTH_TEST'), 'ZFUNC=ALWAYS ⇒ 等价于关深度测试');
  assert.ok(dis.includes('CULL_FACE'), 'CULLMODE 未建模，关掉是安全侧');
});

test('blend mode 0：blendFunc(SRC_ALPHA, ONE_MINUS_SRC_ALPHA) + FUNC_ADD + alphaRef 1/255', () => {
  const gl = mockGl();
  const r = createRenderer(gl);
  r.draw(quad(), { vm: vm(0), texture: 'T' });
  const bf = of(gl, 'blendFunc').at(-1);
  assert.deepEqual([nameOf(gl, bf[1]), nameOf(gl, bf[2])], ['SRC_ALPHA', 'ONE_MINUS_SRC_ALPHA']);
  assert.equal(nameOf(gl, of(gl, 'blendEquation').at(-1)[1]), 'FUNC_ADD');
  const refs = of(gl, 'uniform1f').map((c) => c[2]);
  assert.equal(refs.at(-1), 1 / 255, 'alpha test 开着');
});

test('★ blend mode 3 是唯一关 alpha test 的 ⇒ alphaRef 设成 -1（shader 里不 discard）', () => {
  const gl = mockGl();
  const r = createRenderer(gl);
  r.draw(quad(), { vm: vm(3 << 5), texture: 'T' });
  assert.equal(of(gl, 'uniform1f').at(-1)[2], -1);
  const bf = of(gl, 'blendFunc').at(-1);
  assert.deepEqual([nameOf(gl, bf[1]), nameOf(gl, bf[2])], ['ONE', 'ZERO'], 'Replace');
});

test('blend mode 8/9 用 MIN/MAX 方程；缺 EXT_blend_minmax 时显式抛不降级', () => {
  const ok = mockGl({ minMax: true });
  const r1 = createRenderer(ok);
  r1.draw(quad(), { vm: vm(8 << 5), texture: 'T' });
  assert.equal(nameOf(ok, of(ok, 'blendEquation').at(-1)[1]), 'MIN');

  const bad = mockGl({ minMax: false });
  const r2 = createRenderer(bad);
  assert.throws(() => r2.draw(quad(), { vm: vm(9 << 5), texture: 'T' }), /EXT_blend_minmax/);
  // 但 mode 0 在同一个上下文里照样能画——不是整体退化
  assert.doesNotThrow(() => r2.draw(quad(), { vm: vm(0), texture: 'T' }));
});

test('09 §4.2 的状态缓存：同一状态连画两次不重复设 blendFunc/texParameteri', () => {
  const gl = mockGl();
  const r = createRenderer(gl);
  r.draw(quad(), { vm: vm(0), texture: 'T' });
  const after1 = { bf: of(gl, 'blendFunc').length, tp: of(gl, 'texParameteri').length };
  r.draw(quad(), { vm: vm(0), texture: 'T' });
  assert.equal(of(gl, 'blendFunc').length, after1.bf, '真机也带这份缓存（mgr+0x184fbb4 等）');
  assert.equal(of(gl, 'texParameteri').length, after1.tp);
  r.draw(quad(), { vm: vm(1 << 5), texture: 'T' });
  assert.equal(of(gl, 'blendFunc').length, after1.bf + 1, '换了 blend mode 才重设');
});

test('采样器：filter/wrapS/wrapT 分别来自 flags_hi bit11 / flags_hi bit0-1 / flags_lo bit30-31', () => {
  const gl = mockGl();
  const r = createRenderer(gl);
  r.draw(quad(), { vm: vm((2 << 30) >>> 0, (1 << 11) | 1), texture: 'T' });
  const tp = of(gl, 'texParameteri').map((c) => [nameOf(gl, c[2]), nameOf(gl, c[3])]);
  assert.ok(tp.some(([k, v]) => k === 'TEXTURE_MAG_FILTER' && v === 'NEAREST'), 'bit11=1 ⇒ 最近邻');
  assert.ok(tp.some(([k, v]) => k === 'TEXTURE_WRAP_S' && v === 'CLAMP_TO_EDGE'), 'U 位 =1 ⇒ CLAMP');
  assert.ok(tp.some(([k, v]) => k === 'TEXTURE_WRAP_T' && v === 'MIRRORED_REPEAT'), 'V 位 =2 ⇒ MIRROR');
});

test('四边形展开成两个三角形，索引 (0,1,2)+(1,3,2)，角序 TL/TR/BL/BR', () => {
  const gl = mockGl();
  const r = createRenderer(gl);
  r.draw(quad(), { vm: vm(0), texture: 'T' });
  assert.equal(r.pendingVertices, 6, '2 个三角形');
  r.flush();
  const data = of(gl, 'bufferData').at(-1)[2];
  assert.equal(data.length, 6 * 9, '每顶点 9 个 float：xyz + rgba + uv');
  // 每个顶点的 x 就是它的角下标（夹具这么造的），据此读出索引序列
  const idx = [];
  for (let i = 0; i < 6; i++) idx.push(data[i * 9]);
  assert.deepEqual(idx, [0, 1, 2, 1, 3, 2]);
  assert.equal(r.pendingVertices, 0, 'flush 后清空');
});

test('trianglestrip 展开成 n-2 个三角形，且奇数号三角形调换前两个顶点（保持绕向）', () => {
  const gl = mockGl();
  const r = createRenderer(gl);
  const strip = {
    space: 'screen', primitive: 'trianglestrip',
    vertices: [0, 1, 2, 3, 4].map((i) => ({ x: i, y: 0, z: 0, color: 0xffffffff, u: 0, v: 0 })),
  };
  r.draw(strip, { vm: vm(0), texture: 'T' });
  assert.equal(r.pendingVertices, 3 * 3, '5 个顶点 ⇒ 3 个三角形');
  r.flush();
  const data = of(gl, 'bufferData').at(-1)[2];
  const idx = [];
  for (let i = 0; i < 9; i++) idx.push(data[i * 9]);
  assert.deepEqual(idx, [0, 1, 2, 2, 1, 3, 2, 3, 4]);
});

test('顶点布局：stride 36 字节，位置 0 / 颜色 12 / UV 28', () => {
  const gl = mockGl();
  const r = createRenderer(gl);
  r.draw(quad(), { vm: vm(0), texture: 'T' });
  r.flush();
  const ptrs = of(gl, 'vertexAttribPointer').map((c) => [c[1], c[2], c[4], c[5], c[6]]);
  const strides = ptrs.map((p) => p[3]);
  assert.deepEqual(strides, [36, 36, 36]);
  assert.deepEqual(ptrs.map((p) => p[4]), [0, 12, 28], '三个属性的偏移');
  assert.deepEqual(ptrs.map((p) => p[1]), [3, 4, 2], '分量数：xyz / rgba / uv');
});

test('换贴图会先 flush 上一批（不同贴图不能混进一次 drawArrays）', () => {
  const gl = mockGl();
  const r = createRenderer(gl);
  r.draw(quad(), { vm: vm(0), texture: 'A' });
  assert.equal(of(gl, 'drawArrays').length, 0);
  r.draw(quad(), { vm: vm(0), texture: 'B' });
  assert.equal(of(gl, 'drawArrays').length, 1, '换贴图触发 flush');
  assert.equal(r.pendingVertices, 6, 'B 的那批还在攒');
});

test('model 空间（mode 24/25）没给矩阵就抛——WORLD 矩阵那段还没逆，不编一个', () => {
  const gl = mockGl();
  const r = createRenderer(gl);
  assert.throws(() => r.draw(quad('model'), { vm: vm(0), texture: 'T' }), /ctx\.matrix/);
  const m = new Float32Array(16);
  assert.doesNotThrow(() => r.draw(quad('model'), { vm: vm(0), texture: 'T', matrix: m }));
  assert.equal(of(gl, 'uniformMatrix4fv').length, 1);
});

test('空 geometry 不产生任何调用', () => {
  const gl = mockGl();
  const r = createRenderer(gl);
  const before = gl._calls.length;
  r.draw({ space: 'screen', primitive: 'quad', vertices: [] }, { vm: vm(0), texture: 'T' });
  assert.equal(gl._calls.length, before);
  r.flush();
  assert.equal(of(gl, 'drawArrays').length, 0, '没顶点就不发 draw call');
});

test('shader 源码里带着 09 §4.3 的「一次乘法」与 alpha test discard', () => {
  assert.match(FRAGMENT_SHADER, /texture2D\(uTex, vUv\) \* vColor/, 'ARG1=纹理 × ARG2=diffuse，只此一乘');
  assert.match(FRAGMENT_SHADER, /if \(c\.a < uAlphaRef\) discard;/);
  assert.match(VERTEX_SHADER, /1\.0 - aPos\.y \/ uViewport\.y \* 2\.0/, 'D3D 屏幕 y 向下');
});

test('uploadTexture 传 RGBA8', () => {
  const gl = mockGl();
  uploadTexture(gl, { width: 2, height: 1, rgba: new Uint8Array(8) });
  const c = of(gl, 'texImage2D').at(-1);
  assert.equal(nameOf(gl, c[3]), 'RGBA');
  assert.equal(nameOf(gl, c[7]), 'RGBA');
  assert.equal(nameOf(gl, c[8]), 'UNSIGNED_BYTE');
  assert.deepEqual([c[4], c[5]], [2, 1]);
});
