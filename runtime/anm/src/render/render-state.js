// 层 B · 渲染状态映射 —— D3D9 render state → WebGL。**纯数据 + 纯函数，不碰 gl 对象**，
// 所以能在 Node 里逐项断言。webgl.js 只负责把这里算出来的东西喂给 GL。
//
// 依据：engine/anm/th16/09-drawing.md（下称「09」）§4，全部 ✅ 一手：
//   §4.1 全局重置 `th16:0x45ba80` 的 8 条 render state
//   §4.2 per-VM `setup_render_state_for_vm` `th16:0x464f10`：blend 10 模式 + 采样器过滤/寻址
//   §4.3 mode 函数里的 texture-stage state
//   §4.4 顶点格式与 draw call
//
// **GL 枚举一律用字符串符号名**（`'SRC_ALPHA'` 而不是 0x0302）：测试不需要 GL 上下文，
// 文档也读得懂；webgl.js 里再查表换成 `gl.XXX`。
//
// ⚠️ 09 §4.2 点名的易踩错：`0x114` 是 **SetSamplerState** 不是 SetTextureStageState。
// 采样器过滤/寻址（(b)(c)(d)）只影响**怎么取纹素**，不产生任何 color/alpha op。

// ── §4.1 全局重置（设备初始化时设一次） ────────────────────────────────────────────
// D3D 枚举值是一手 ✅（直读），括号里的名字是按 D3D9 SDK 翻的 🟡。
export const GLOBAL_DEFAULTS = {
  alphaBlendEnable: true, // 0x1b = 1 —— 全局开混合，所以各 blend mode 才生效
  alphaTestEnable: true, // 0x0f = 1
  alphaRef: 1, // 0x18 = 1  ⇒ 参考值 1/255 ≈ 0.004
  alphaFunc: 'GREATEREQUAL', // 0x19 = 7
  srcBlend: 'SRC_ALPHA', // 0x13 = 5
  dstBlend: 'ONE_MINUS_SRC_ALPHA', // 0x14 = 6
  blendOp: 'FUNC_ADD', // 0xab = 1
  zFunc: 'ALWAYS', // 0x17 = 8 —— 深度测试恒过
};

// alpha test 的落地：`ALPHAREF=1` + `GREATEREQUAL` ⇒ **丢弃 alpha < 1/255 的像素**。
// 不是 no-op（09 §4.1）。WebGL 没有固定管线的 alpha test，只能在 fragment shader 里 discard。
export const ALPHA_TEST_THRESHOLD = 1 / 255;

// ── §4.2 (a) blend mode = `flags_lo` bit 5–8，10 项跳表 `th16:0x465254` ────────────
// 每行：[alphaTest, SRCBLEND, DESTBLEND, BLENDOP]，D3D 值 → WebGL 符号名。
// **唯独 mode 3（Replace）把 ALPHATESTENABLE 设 0**，其余 9 种都开。
// `formula` 是把因子代进混合方程得到的闭式（09 §4.2.1）；`killsDestAlpha` = 源不透明时
// 会把目标 alpha 打成 0 —— **网页画布上就是「打洞」**，见文件尾的说明与 D3D9-MAPPING.md §6。
export const BLEND_MODES = [
  { alphaTest: true, src: 'SRC_ALPHA', dst: 'ONE_MINUS_SRC_ALPHA', op: 'FUNC_ADD', formula: 'S·Sa + D(1−Sa)', killsDestAlpha: false, note: 'Normal 透明' },
  { alphaTest: true, src: 'SRC_ALPHA', dst: 'ONE', op: 'FUNC_ADD', formula: 'S·Sa + D', killsDestAlpha: false, note: 'Add 加色' },
  { alphaTest: true, src: 'SRC_ALPHA', dst: 'ONE', op: 'FUNC_REVERSE_SUBTRACT', formula: 'D − S·Sa', killsDestAlpha: true, note: 'Subtract 减色' },
  { alphaTest: false, src: 'ONE', dst: 'ZERO', op: 'FUNC_ADD', formula: 'S', killsDestAlpha: false, note: 'Replace 不透明（唯一关 alpha test 的）' },
  // ★ 不是 Screen：thpages 记作「Screen? (S + D − SD)」，实际是 S + D − **2**SD。
  // 白叠白 (S=D=1) 本式给 0（黑）、Screen 给 1（白），差满量程。09 §4.2.1 ①。
  { alphaTest: true, src: 'ONE_MINUS_DST_COLOR', dst: 'ONE_MINUS_SRC_COLOR', op: 'FUNC_ADD', formula: 'S + D − 2SD', killsDestAlpha: true, note: '对称差型；**不是 Screen**' },
  { alphaTest: true, src: 'DST_COLOR', dst: 'ZERO', op: 'FUNC_ADD', formula: 'S·D', killsDestAlpha: false, note: 'Multiply 正片叠底' },
  { alphaTest: true, src: 'ONE_MINUS_SRC_COLOR', dst: 'ONE_MINUS_SRC_ALPHA', op: 'FUNC_ADD', formula: 'S(1−S) + D(1−Sa)', killsDestAlpha: true, note: '—' },
  { alphaTest: true, src: 'DST_ALPHA', dst: 'ONE_MINUS_DST_ALPHA', op: 'FUNC_ADD', formula: 'S·Da + D(1−Da)', killsDestAlpha: false, note: '前置（画在 dst 后）' },
  // 8/9：GL 规范明文规定 MIN/MAX **不带权重**，所以这两个因子在 GL 端是空转的（09 §4.2.1 ③）。
  { alphaTest: true, src: 'SRC_ALPHA', dst: 'ONE', op: 'MIN', formula: 'min(S, D)（因子空转）', killsDestAlpha: false, note: '只取暗部' },
  { alphaTest: true, src: 'SRC_ALPHA', dst: 'ONE', op: 'MAX', formula: 'max(S, D)（因子空转）', killsDestAlpha: false, note: '只取亮部' },
];

/**
 * 会把目标 alpha 打掉的那几种模式（09 §4.2.1 ②）。D3D9 没开 `SEPARATEALPHABLENDENABLE`，
 * 所以同一对因子也作用在 alpha 上；源不透明时 mode 2/4/6 的结果 alpha 都是 0。
 * 真机无所谓（后备缓冲不透明），**网页画布会被打洞** —— 真实数据里 mode 2 用了 43 次、
 * mode 4 用了 6 次。解法见 D3D9-MAPPING.md §6：把 canvas 建成 `{ alpha: false }`。
 */
export const ALPHA_DESTROYING_MODES = BLEND_MODES
  .map((b, i) => (b.killsDestAlpha ? i : -1))
  .filter((i) => i >= 0);

/**
 * `flags_lo` bit 5–8 → blend 状态。
 * @returns {object|null} `null` = 09 §4.2「>9 走 default **不设**」——即**保持上一次的状态**。
 *   真机那张跳表只有 10 项，越界直接跳过整段设置，所以这是「继承」不是「用默认」。
 *   真实数据里 `303 blendMode` 的取值是 {0,1,2,3,4,7,9}，**从未越界**，所以这条分支没有
 *   ground truth；调用方拿到 null 应当「什么都不做」，不要擅自回落到 mode 0。
 */
export function blendStateFor(flagsLo) {
  const mode = (flagsLo >>> 5) & 0xf;
  return BLEND_MODES[mode] ?? null;
}

// ── §4.2 (b) 纹理过滤 = `flags_hi` bit 11（`311 resampleMode` 的落点）──────────────
// v = 2(LINEAR) 当 bit11 = 0、v = 1(POINT) 当 bit11 = 1。MAG 与 MIN 设同一个值。
export function filterFor(flagsHi) {
  const nearest = (flagsHi >>> 11) & 1;
  return nearest ? 'NEAREST' : 'LINEAR';
}

// ── §4.2 (c)(d) 寻址 = U 在 `flags_hi` bit0–1、V 在 `flags_lo` bit30–31 ──────────────
// **两个字段是拆开的**（`312 scrollMode` 一条指令写两处）。
// `v ∈ {1=WRAP, 3=CLAMP, 2=MIRROR}` 对应位值 {0,1,2}。
const ADDRESS_MODES = ['REPEAT', 'CLAMP_TO_EDGE', 'MIRRORED_REPEAT'];

function addressFor(bits, which) {
  const m = ADDRESS_MODES[bits];
  if (m === undefined) {
    // 位值 3：`312 scrollMode` 写的是 `n & 3` 所以可达，但 09 §4.2 只记了 {0,1,2} 三档，
    // 真实数据里 `312` 的取值只有 (2,0) 与 (0,1)，**3 一次都没出现** ⇒ 没有 ground truth。
    // 宁可抛也不要挑一个看着合理的档：猜错的话画面会在某条 mod 脚本上悄悄不对。
    throw new Error(
      `render-state: ${which} 寻址位值 3 未建模 —— 09-drawing.md §4.2 只记了 {0,1,2} → ` +
        '{WRAP, CLAMP, MIRROR}，真实 th16 数据里 `312 scrollMode` 从未写出 3，没有可对的 ground truth。',
    );
  }
  return m;
}
export function addressUFor(flagsHi) {
  return addressFor(flagsHi & 3, 'U');
}
export function addressVFor(flagsLo) {
  return addressFor((flagsLo >>> 30) & 3, 'V');
}

/** 把一个 vm 的四项采样器/混合状态一次算全。 */
export function renderStateFor(vm) {
  return {
    blend: blendStateFor(vm.flagsLo),
    filter: filterFor(vm.flagsHi),
    addressU: addressUFor(vm.flagsHi),
    addressV: addressVFor(vm.flagsLo),
  };
}

// ── §4.3 texture stage：固定管线那一层就是**一次乘法** ────────────────────────────
// `COLOROP/ALPHAOP = MODULATE`、`COLORARG2/ALPHAARG2 = DIFFUSE`（sprite / textureCircle）或
// `TFACTOR`（textureArc3D）；`COLORARG1/ALPHAARG1` **从不设** ⇒ 恒为 D3D9 默认 `D3DTA_TEXTURE`。
// ⇒ WebGL 一条 `texture2D(...) * color` 就复刻了整条 texture stage。别照「固定管线默认」
// 多写一层 color/alpha op（09 §8 的坑表专门列了这条）。
export const TEXTURE_STAGE = {
  colorOp: 'MODULATE',
  alphaOp: 'MODULATE',
  arg1: 'TEXTURE', // 从不设 ⇒ D3D9 默认
  // arg2 由绘制路径决定：
  arg2ByPath: {
    sprite: 'DIFFUSE', // flush_sprites `th16:0x465a80`
    textureCircle: 'DIFFUSE', // `th16:0x4681f0`（mode 9/13/14）
    textureArc3D: 'TFACTOR', // `th16:0x467d00`（mode 24/25）+ SetRenderState(TEXTUREFACTOR)
  },
};

// ── §4.4 顶点格式（FVF → stride） ────────────────────────────────────────────────
// ★ textureArc3D 那行 09 §4.4 原先写错过三项（FVF `0x102`/stride 20/`DrawPrimitive`），
// 2026-09-10 已更正为 `0x142` / 24 / `DrawPrimitiveUP`——这里用更正后的值。
export const VERTEX_FORMATS = {
  sprite: { fvf: 0x144, stride: 28, primitive: 'TRIANGLES', note: 'XYZRHW|DIFFUSE|TEX1' },
  textureCircle: { fvf: 0x144, stride: 28, primitive: 'TRIANGLE_STRIP', note: '同 sprite' },
  textureArc3D: { fvf: 0x142, stride: 24, primitive: 'TRIANGLE_STRIP', note: 'XYZ|DIFFUSE|TEX1' },
};
