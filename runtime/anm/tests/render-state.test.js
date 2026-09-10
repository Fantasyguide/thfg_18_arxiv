// 层 B · 渲染状态映射测试（纯数据，不需要 GL 上下文）。
// 依据：engine/anm/th16/09-drawing.md §4。
import test from 'node:test';
import assert from 'node:assert/strict';
import {
  ALPHA_DESTROYING_MODES, ALPHA_TEST_THRESHOLD, BLEND_MODES, GLOBAL_DEFAULTS, TEXTURE_STAGE, VERTEX_FORMATS,
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

// ══════════════════════════════════════════════════════════════════════════════════
// § 09 §4.2.1：把因子代进混合方程 —— 闭式、对 thpages 的两处更正、alpha 打洞
// ══════════════════════════════════════════════════════════════════════════════════
// 下面这个求值器**只依赖因子名**（那是一手 ✅ 的部分）与 D3D9/GL 共用的混合方程定义。
// 所以谁要是改了 BLEND_MODES 里的因子，闭式断言会立刻红——这几条测试就是那些闭式的
// 「可执行版本」，防止文档与代码各自漂移。
const FACTOR = {
  ONE: () => 1,
  ZERO: () => 0,
  SRC_ALPHA: ({ Sa }) => Sa,
  ONE_MINUS_SRC_ALPHA: ({ Sa }) => 1 - Sa,
  DST_COLOR: ({ D }) => D,
  ONE_MINUS_DST_COLOR: ({ D }) => 1 - D,
  ONE_MINUS_SRC_COLOR: ({ S }) => 1 - S,
  DST_ALPHA: ({ Da }) => Da,
  ONE_MINUS_DST_ALPHA: ({ Da }) => 1 - Da,
};
const clamp01 = (v) => Math.max(0, Math.min(1, v));
// 颜色通道求值。alpha 通道同式，只是把 S/D 换成 Sa/Da（因子本身也按 alpha 取）。
function evalBlend(mode, S, D, Sa = 1, Da = 1) {
  const b = BLEND_MODES[mode];
  if (b.op === 'MIN') return Math.min(S, D); // GL 规范：MIN/MAX 不带权重
  if (b.op === 'MAX') return Math.max(S, D);
  const env = { S, D, Sa, Da };
  const s = S * FACTOR[b.src](env);
  const d = D * FACTOR[b.dst](env);
  return clamp01(b.op === 'FUNC_REVERSE_SUBTRACT' ? d - s : s + d);
}
function evalBlendAlpha(mode, Sa, Da) {
  const b = BLEND_MODES[mode];
  if (b.op === 'MIN') return Math.min(Sa, Da);
  if (b.op === 'MAX') return Math.max(Sa, Da);
  // 对 alpha 通道，"COLOR" 类因子取的也是 alpha 分量
  const env = { S: Sa, D: Da, Sa, Da };
  const s = Sa * FACTOR[b.src](env);
  const d = Da * FACTOR[b.dst](env);
  return clamp01(b.op === 'FUNC_REVERSE_SUBTRACT' ? d - s : s + d);
}

test('09 §4.2.1：每种模式的闭式与因子代入的结果一致（抽 5 组值）', () => {
  const closed = {
    0: (S, D, Sa) => clamp01(S * Sa + D * (1 - Sa)),
    1: (S, D, Sa) => clamp01(S * Sa + D),
    2: (S, D, Sa) => clamp01(D - S * Sa),
    3: (S) => clamp01(S),
    4: (S, D) => clamp01(S + D - 2 * S * D),
    5: (S, D) => clamp01(S * D),
    6: (S, D, Sa) => clamp01(S * (1 - S) + D * (1 - Sa)),
    7: (S, D, Sa, Da) => clamp01(S * Da + D * (1 - Da)),
    8: (S, D) => Math.min(S, D),
    9: (S, D) => Math.max(S, D),
  };
  const samples = [[1, 1, 1, 1], [0.5, 0.5, 1, 1], [0.8, 0.2, 0.5, 1], [0.25, 0.75, 1, 0.5], [0, 1, 0.25, 0.75]];
  for (let m = 0; m < 10; m++) {
    for (const [S, D, Sa, Da] of samples) {
      assert.ok(
        Math.abs(evalBlend(m, S, D, Sa, Da) - closed[m](S, D, Sa, Da)) < 1e-9,
        `mode ${m} @ S=${S} D=${D} Sa=${Sa} Da=${Da}：因子代入 ${evalBlend(m, S, D, Sa, Da)} ≠ 闭式 ${closed[m](S, D, Sa, Da)}`,
      );
    }
  }
});

test('★ 09 §4.2.1 ①：mode 4 不是 Screen —— 白叠白给黑，差满量程', () => {
  // thpages 记作「Screen? (S + D − SD)」。取分离最大的点做对抗证伪。
  const screen = (S, D) => clamp01(S + D - S * D);
  assert.equal(evalBlend(4, 1, 1), 0, '本式：1 + 1 − 2 = 0 ⇒ 黑');
  assert.equal(screen(1, 1), 1, 'Screen：1 + 1 − 1 = 1 ⇒ 白');
  assert.ok(Math.abs(evalBlend(4, 0.8, 0.8) - 0.32) < 1e-9);
  assert.ok(Math.abs(screen(0.8, 0.8) - 0.96) < 1e-9);
  // 而且不是「只在极端点不同」：两者在整个定义域上几乎处处不同
  let same = 0, total = 0;
  for (let i = 0; i <= 10; i++) for (let j = 0; j <= 10; j++) {
    total++;
    if (Math.abs(evalBlend(4, i / 10, j / 10) - screen(i / 10, j / 10)) < 1e-9) same++;
  }
  assert.ok(same < total * 0.3, `121 个采样点里只有 ${same} 个相同 —— 不是舍入差异`);
  assert.match(BLEND_MODES[4].note, /不是 Screen/, '表里要写明这件事');
  // ★ 把 `formula` 字段也钉住：它是这条更正在代码里的落点，不钉的话改回
  // 「S + D − SD」不会有任何测试红（第一版故意破坏验证时就是这样，红 0 条）。
  assert.match(BLEND_MODES[4].formula, /2SD/, 'mode 4 的闭式必须带那个 2');
  // 顺手把另外几个有名字的闭式也钉一下，防止整列 formula 悄悄腐烂
  assert.equal(BLEND_MODES[3].formula, 'S');
  assert.equal(BLEND_MODES[5].formula, 'S·D');
});

test('★ 09 §4.2.1 ②：mode 2/4/6 在源不透明时把目标 alpha 打成 0', () => {
  assert.deepEqual(ALPHA_DESTROYING_MODES, [2, 4, 6]);
  for (let m = 0; m < 10; m++) {
    const a = evalBlendAlpha(m, 1, 1); // 不透明源画到不透明底
    const kills = a < 1 - 1e-9;
    assert.equal(kills, BLEND_MODES[m].killsDestAlpha, `mode ${m}：结果 alpha=${a}，表里标的是 ${BLEND_MODES[m].killsDestAlpha}`);
  }
  // thpages 只提了 mode 2；4 与 6 同样如此，而 4 在真实数据里用了 6 次。
  assert.equal(evalBlendAlpha(4, 1, 1), 0);
  assert.equal(evalBlendAlpha(6, 1, 1), 0);
});

test('09 §4.2.1 ③：mode 8/9 的两个因子是空转的（GL 的 MIN/MAX 不带权重）', () => {
  // 把 SRCALPHA 换成任何值都不该改变结果 —— 这里直接验「结果 = min/max(S,D)」。
  for (const [S, D] of [[0.2, 0.8], [1, 0], [0.5, 0.5]]) {
    assert.equal(evalBlend(8, S, D, 0.3), Math.min(S, D), 'Sa 不参与');
    assert.equal(evalBlend(9, S, D, 0.3), Math.max(S, D));
  }
  assert.match(BLEND_MODES[8].formula, /因子空转/);
});
