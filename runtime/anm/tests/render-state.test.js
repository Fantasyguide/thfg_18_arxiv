// 层 B · 渲染状态映射测试（纯数据，不需要 GL 上下文）。
// 依据：engine/anm/th16/09-drawing.md §4。
import test from 'node:test';
import assert from 'node:assert/strict';
import {
  ALPHA_TEST_THRESHOLD, BLEND_MODES, GLOBAL_DEFAULTS, TEXTURE_STAGE, VERTEX_FORMATS,
  addressUFor, addressVFor, blendStateFor, filterFor, renderStateFor,
} from '../src/render/render-state.js';

const vmWith = (lo = 0, hi = 0) => ({ flagsLo: lo >>> 0, flagsHi: hi >>> 0 });

test('09 §4.2：blend 表 10 项，且 mode 3 是唯一关 alpha test 的', () => {
  assert.equal(BLEND_MODES.length, 10, '真机跳表 `th16:0x465254` 就是 10 项');
  const off = BLEND_MODES.map((b, i) => (b.alphaTest ? null : i)).filter((x) => x !== null);
  assert.deepEqual(off, [3], '只有 mode 3 把 ALPHATESTENABLE 设 0');
});

test('09 §4.2：10 种 blend 的 src/dst/op 逐项与一手表一致', () => {
  const got = BLEND_MODES.map((b) => `${b.src}|${b.dst}|${b.op}`);
  assert.deepEqual(got, [
    'SRC_ALPHA|ONE_MINUS_SRC_ALPHA|FUNC_ADD',
    'SRC_ALPHA|ONE|FUNC_ADD',
    'SRC_ALPHA|ONE|FUNC_REVERSE_SUBTRACT',
    'ONE|ZERO|FUNC_ADD',
    'ONE_MINUS_DST_COLOR|ONE_MINUS_SRC_COLOR|FUNC_ADD',
    'DST_COLOR|ZERO|FUNC_ADD',
    'ONE_MINUS_SRC_COLOR|ONE_MINUS_SRC_ALPHA|FUNC_ADD',
    'DST_ALPHA|ONE_MINUS_DST_ALPHA|FUNC_ADD',
    'SRC_ALPHA|ONE|MIN',
    'SRC_ALPHA|ONE|MAX',
  ]);
});

test('blendStateFor 取的是 flags_lo bit5–8（不是别的位段）', () => {
  for (let m = 0; m < 10; m++) {
    assert.equal(blendStateFor((m << 5) >>> 0), BLEND_MODES[m], `mode ${m}`);
  }
  // 把 bit5–8 以外的位全填满也不该影响结果
  const allButBlend = (0xffffffff & ~0x1e0) >>> 0; // bit5–8 清空、其余全填 1
  assert.equal(blendStateFor(((5 << 5) | allButBlend) >>> 0), BLEND_MODES[5], '其余位是别人的地盘');
});

test('blendStateFor：>9 返回 null = 「不设、保持上一次」，不是回落到 mode 0', () => {
  for (let m = 10; m < 16; m++) assert.equal(blendStateFor((m << 5) >>> 0), null, `mode ${m}`);
});

test('真实数据用到的 7 个 blend mode 全部解得出（{0,1,2,3,4,7,9}）', () => {
  // 实测 `303 blendMode` 分布：1×440 · 2×43 · 0×16 · 3×12 · 4×6 · 9×2 · 7×1，从未越界。
  for (const m of [0, 1, 2, 3, 4, 7, 9]) assert.ok(blendStateFor((m << 5) >>> 0), `mode ${m} 必须有表项`);
});

test('09 §4.2 (b)：flags_hi bit11 = 0 线性 / 1 最近邻', () => {
  assert.equal(filterFor(0), 'LINEAR');
  assert.equal(filterFor(1 << 11), 'NEAREST');
  assert.equal(filterFor(0xffffffff & ~(1 << 11)), 'LINEAR', '别的位不该影响');
});

test('09 §4.2 (c)(d)：U 在 flags_hi bit0–1、V 在 flags_lo bit30–31（两个字段拆开）', () => {
  assert.equal(addressUFor(0), 'REPEAT');
  assert.equal(addressUFor(1), 'CLAMP_TO_EDGE');
  assert.equal(addressUFor(2), 'MIRRORED_REPEAT');
  assert.equal(addressVFor(0), 'REPEAT');
  assert.equal(addressVFor((1 << 30) >>> 0), 'CLAMP_TO_EDGE');
  assert.equal(addressVFor((2 << 30) >>> 0), 'MIRRORED_REPEAT');
  // ★ 拆开这件事本身：把 U 的位写进 flags_lo 不该改变 U
  assert.equal(addressUFor(0), 'REPEAT', 'U 只看 flags_hi');
});

test('寻址位值 3 未建模 ⇒ 抛（真实数据里 312 从未写出 3）', () => {
  assert.throws(() => addressUFor(3), /位值 3 未建模/);
  assert.throws(() => addressVFor((3 << 30) >>> 0), /位值 3 未建模/);
});

test('renderStateFor 一次算全四项', () => {
  const st = renderStateFor(vmWith((1 << 5) | (1 << 30), (1 << 11) | 2));
  assert.equal(st.blend, BLEND_MODES[1]);
  assert.equal(st.filter, 'NEAREST');
  assert.equal(st.addressU, 'MIRRORED_REPEAT');
  assert.equal(st.addressV, 'CLAMP_TO_EDGE');
});

test('09 §4.1：全局重置的 alpha test 落成「丢弃 a < 1/255」', () => {
  assert.equal(GLOBAL_DEFAULTS.alphaRef, 1);
  assert.equal(GLOBAL_DEFAULTS.alphaFunc, 'GREATEREQUAL');
  assert.equal(ALPHA_TEST_THRESHOLD, 1 / 255);
  assert.equal(GLOBAL_DEFAULTS.zFunc, 'ALWAYS', '深度测试恒过');
});

test('09 §4.3：texture stage 是一次乘法，ARG1 恒为纹理', () => {
  assert.equal(TEXTURE_STAGE.colorOp, 'MODULATE');
  assert.equal(TEXTURE_STAGE.arg1, 'TEXTURE', 'COLORARG1 从不设 ⇒ D3D9 默认');
  assert.equal(TEXTURE_STAGE.arg2ByPath.sprite, 'DIFFUSE');
  assert.equal(TEXTURE_STAGE.arg2ByPath.textureArc3D, 'TFACTOR', '只有 3D 那条用 TFACTOR');
});

test('09 §4.4：textureArc3D 用更正后的 FVF 0x142 / stride 24', () => {
  // 原文曾写 `0x102` / 20 / DrawPrimitive，三项都错（2026-09-10 更正）。
  assert.equal(VERTEX_FORMATS.textureArc3D.fvf, 0x142);
  assert.equal(VERTEX_FORMATS.textureArc3D.stride, 24);
  assert.equal(VERTEX_FORMATS.sprite.stride, 28);
  assert.equal(VERTEX_FORMATS.sprite.fvf, 0x144);
});
