// THTX 贴图解码测试（format → RGBA8）。
// 依据：engine/anm/th16/01-format.md（下称「01」）§6/§7。
//
// 全部用手工构造的像素与 THTX 块（零版权字节）——真实 `.anm` 在 local/（gitignored），
// 测试不能依赖它。真实文件的全量跑通另行在 OVERVIEW 里记数（417 entry / 89.7M 像素）。
import test from 'node:test';
import assert from 'node:assert/strict';
import { decodeThtx, formatToRgba, THTX_BPP, THTX_FORMAT } from '../src/format/thtx.js';

const px = (rgba, i) => [rgba[i * 4], rgba[i * 4 + 1], rgba[i * 4 + 2], rgba[i * 4 + 3]];

test('01 §7：9 项 bpp 表逐项与一手表一致', () => {
  // 直读 `th16:0x491b90` 的九个字节。这张表是 ✅ 一手，与「位布局」（🟡）是两回事。
  assert.deepEqual(THTX_BPP, [4, 4, 2, 2, 3, 2, 2, 1, 1]);
  // 实现了的四个 format，bpp 必须和大表对得上（防止两处各写一份而漂移）。
  for (const [f, info] of Object.entries(THTX_FORMAT)) {
    assert.equal(info.bpp, THTX_BPP[Number(f)], `format ${f} 的 bpp 两处要一致`);
  }
});

test('format 1（A8R8G8B8）：小端字节序是 B,G,R,A，解码只是 R/B 换位', () => {
  //                       B     G     R     A
  const src = Uint8Array.from([0x11, 0x22, 0x33, 0x44]);
  assert.deepEqual([...formatToRgba(src, 1, 1)], [0x33, 0x22, 0x11, 0x44]);
});

test('format 5（A4R4G4B4）：半字节扩成 n*17（0x3 -> 0x33），纯白到得了 255', () => {
  // u16 位段 A=15..12 R=11..8 G=7..4 B=3..0；小端 ⇒ byte0 = G<<4|B、byte1 = A<<4|R。
  // 造 A=0xf R=0x3 G=0x0 B=0xf ⇒ byte0 = 0x0f、byte1 = 0xf3
  const src = Uint8Array.from([0x0f, 0xf3]);
  assert.deepEqual([...formatToRgba(src, 1, 5)], [0x33, 0x00, 0xff, 0xff]);
  // 全 f ⇒ 四个通道都必须是 255（而不是 240）——这条就是在守 `*17` 不被写成 `<<4`
  assert.deepEqual([...formatToRgba(Uint8Array.from([0xff, 0xff]), 1, 5)], [255, 255, 255, 255]);
});

test('format 3（R5G6B5）：默认 GPU 规则，纯白 = (255,255,255)', () => {
  const white = Uint8Array.from([0xff, 0xff]); // u16 = 0xffff
  assert.deepEqual([...formatToRgba(white, 1, 3)], [255, 255, 255, 255]);
  // 单通道满量程也要到 255
  assert.deepEqual([...formatToRgba(Uint8Array.from([0x00, 0xf8]), 1, 3)], [255, 0, 0, 255], '纯红 0xf800');
  assert.deepEqual([...formatToRgba(Uint8Array.from([0xe0, 0x07]), 1, 3)], [0, 255, 0, 255], '纯绿 0x07e0');
  assert.deepEqual([...formatToRgba(Uint8Array.from([0x1f, 0x00]), 1, 3)], [0, 0, 255, 255], '纯蓝 0x001f');
});

test('format 3：thanm 模式复刻它蓝通道到不了 255 的 bug（工具链对拍用）', () => {
  // ★ 这条测试的存在本身就是结论：「抄 thanm 就能搞定」对 RGB565 不成立。
  // thanm image.c 的蓝通道三项都写成 `<< 16`，掩码却在 bit18/17/16 ⇒ 前两项恒为 0。
  const white = Uint8Array.from([0xff, 0xff]);
  assert.deepEqual([...formatToRgba(white, 1, 3, { rgb565: 'thanm' })], [255, 255, 249, 255]);
  assert.deepEqual([...formatToRgba(Uint8Array.from([0x1f, 0x00]), 1, 3, { rgb565: 'thanm' })], [0, 0, 249, 255]);
});

test('format 3：两种模式在 65536 个 u16 取值上的分歧计数 = 65280', () => {
  // 不是「大概不一样」——精确到个数。谁改了扩位规则，这个数会立刻动。
  const src = new Uint8Array(2);
  let diff = 0;
  for (let u = 0; u < 0x10000; u++) {
    src[0] = u & 0xff;
    src[1] = u >> 8;
    const a = formatToRgba(src, 1, 3);
    const b = formatToRgba(src, 1, 3, { rgb565: 'thanm' });
    if (a[0] !== b[0] || a[1] !== b[1] || a[2] !== b[2]) diff++;
  }
  assert.equal(diff, 65280, '99.6% 的取值上两者不同——所以这不是「差一点点」的问题');
});

test('format 7（A8）：默认按一手的 D3DFMT_A8 解（rgb=0、a=源字节）', () => {
  const src = Uint8Array.from([0x00, 0x80, 0xff]);
  const out = formatToRgba(src, 3, 7);
  assert.deepEqual(px(out, 0), [0, 0, 0, 0x00]);
  assert.deepEqual(px(out, 1), [0, 0, 0, 0x80]);
  assert.deepEqual(px(out, 2), [0, 0, 0, 0xff]);
});

test('format 7：luminance 模式给 thanm 口径（rgb = a = 源字节）', () => {
  const out = formatToRgba(Uint8Array.from([0x80]), 1, 7, { a8: 'luminance' });
  assert.deepEqual([...out], [0x80, 0x80, 0x80, 0x80]);
});

test('未实现的 format 显式抛，不静默当成某个已知格式', () => {
  // 01 §7 的 9 项表里 0/2/4/6/8 只有 bpp 与 D3DFORMAT 是一手，位布局是按 SDK 名字翻的 🟡。
  for (const f of [0, 2, 4, 6, 8]) {
    assert.throws(() => formatToRgba(new Uint8Array(16), 1, f), /未实现/, `format ${f}`);
  }
  // 表外的 format 走的也是「未实现」这条（formatToRgba 先查 THTX_FORMAT，查不到就抛，
  // 轮不到 bppOf 的表范围检查）——那条 /超出/ 只在 decodeThtx 里才够得着，见下一条。
  assert.throws(() => formatToRgba(new Uint8Array(16), 1, 9), /未实现/, '表外的 format');
});

test('decodeThtx：format 超出 01 §7 的 9 项表时报「超出」而不是「未实现」', () => {
  const blk = mkThtx({ format: 1, width: 1, height: 1, pixels: new Uint8Array(4) });
  new DataView(blk.buffer).setInt16(0x06, 9, true);
  assert.throws(() => decodeThtx(blk, 0), /超出.*9 项表/);
});

test('源像素不足时抛，不越界读出垃圾', () => {
  assert.throws(() => formatToRgba(new Uint8Array(3), 1, 1), /源像素不足/);
});

test('不认识的 opts 值显式抛（同 createVm 的 opts 守卫）', () => {
  assert.throws(() => formatToRgba(new Uint8Array(4), 1, 1, { rgb565: 'standard' }), /不认识的 rgb565/);
  assert.throws(() => formatToRgba(new Uint8Array(4), 1, 7, { a8: 'gray' }), /不认识的 a8/);
});

// ── decodeThtx：01 §6 的 16 字节头 ──────────────────────────────────────────────
function mkThtx({ format, width, height, pixels }) {
  const head = new Uint8Array(0x10);
  const dv = new DataView(head.buffer);
  head[0] = 0x54; head[1] = 0x48; head[2] = 0x54; head[3] = 0x58; // "THTX"
  dv.setUint16(0x04, 0, true);
  dv.setInt16(0x06, format, true);
  dv.setInt16(0x08, width, true);
  dv.setInt16(0x0a, height, true);
  dv.setUint32(0x0c, pixels.length, true);
  const out = new Uint8Array(0x10 + pixels.length);
  out.set(head, 0);
  out.set(pixels, 0x10);
  return out;
}

test('decodeThtx：头字段 + 像素解码，行距紧密无 padding', () => {
  // 2x2 的 A8R8G8B8，四个像素各不相同（B,G,R,A 顺序写入）
  const pixels = Uint8Array.from([
    1, 2, 3, 4, 5, 6, 7, 8,
    9, 10, 11, 12, 13, 14, 15, 16,
  ]);
  const blk = mkThtx({ format: 1, width: 2, height: 2, pixels });
  const t = decodeThtx(blk, 0);
  assert.equal(t.format, 1);
  assert.equal(t.width, 2);
  assert.equal(t.height, 2);
  assert.equal(t.bpp, 4);
  assert.equal(t.size, 16, '01 §6：size 实测恒 = w*h*bpp（加载器不读它）');
  assert.equal(t.rgba.length, 16);
  assert.deepEqual(px(t.rgba, 0), [3, 2, 1, 4], '第 0 像素 R/B 换位');
  assert.deepEqual(px(t.rgba, 3), [15, 14, 13, 16], '最后一个像素——行距算错这条就红');
});

test('decodeThtx：块可以在文件中间（偏移不是 0）', () => {
  const inner = mkThtx({ format: 7, width: 2, height: 1, pixels: Uint8Array.from([0x10, 0x20]) });
  const file = new Uint8Array(64 + inner.length);
  file.set(inner, 64);
  const t = decodeThtx(file, 64);
  assert.deepEqual(px(t.rgba, 0), [0, 0, 0, 0x10]);
  assert.deepEqual(px(t.rgba, 1), [0, 0, 0, 0x20]);
});

test('decodeThtx：负尺寸报错（01 §6 字段是 s16，真实数据全非负）', () => {
  const blk = mkThtx({ format: 1, width: 2, height: 2, pixels: new Uint8Array(16) });
  new DataView(blk.buffer).setInt16(0x08, -1, true);
  assert.throws(() => decodeThtx(blk, 0), /负的尺寸/);
});
