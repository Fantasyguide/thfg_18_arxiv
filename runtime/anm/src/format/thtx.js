// THTX 贴图块解码 —— format → RGBA8。
//
// 依据：engine/anm/th16/01-format.md（下称「01」）§6（THTX 头）与 §7（format 枚举一手表）。
// 参考实现：local/vendor/thtk/thanm/image.c 的 `format_to_rgba()`（**不是逐条照抄，见下**）。
//
// ── 为什么不能直接抄 thanm ────────────────────────────────────────────────────────
// thanm 是**编辑工具**，它要的是「解出来能存 PNG、改完再塞回去」；播放器要的是「像游戏」。
// 两者在 RGB565 上给出不同结果，而且差别肉眼可见：
//
//   thanm 的 `FORMAT_RGB565` 分支里，蓝通道那三项写成
//       ((u16 & 0x0001) << 16 & 0x040000) | ((u16 & 0x0001) << 16 & 0x020000) | (… & 0x010000)
//   三项都是 `<< 16`，但掩码分别在 bit18/17/16 —— 前两项恒为 0（`(x&1)<<16` 只可能点亮 bit16）。
//   ⇒ **蓝通道永远到不了 255**：纯白 `0xffff` 解出来是 RGB(255, 255, 249)。
//   再者它整体用「把最低位复制到补位」而不是「把高位复制到低位」，
//   实测 65536 个 u16 取值里 **65280 个（99.6%）与 GPU 的结果不同**。
//
// 真机把 RGB565 交给 D3D9 纹理，**扩位是 GPU 干的**，规则是标准的高位复制
// （`v8 = (v5 << 3) | (v5 >> 2)`）。所以本文件默认走 GPU 规则；
// 想要与 thanm 逐字节对齐（做工具链对拍时）传 `{ rgb565: 'thanm' }`。
//
// ARGB4444 与 BGRA8888 两条则**与 thanm 完全一致**：前者的半字节复制 `n -> n*17`
// （`0x3 -> 0x33`）正是 GPU 对 4 位通道做的事，后者只是 R/B 交换。
//
// ── 只实现真实数据里出现过的 4 个 format ──────────────────────────────────────────
// 01 §7 的一手表有 9 项，但 bpp 与 D3DFORMAT 之外**没有一手的位布局证据**——
// 「A1R5G5B5」「A8R3G3B2」这些名字是按 D3D9 SDK 的枚举值翻的（01 §7 自己标 🟡）。
// 照名字猜位布局 = 编证据。实测 `thtx.format`（**注意不是 `entry.format`**）在 54 个
// 真实 th16 文件、423 个 entry 里只出现 1 / 3 / 5 / 7 四个值，所以这里只实现这四个，
// 其余显式抛。
export const THTX_FORMAT = {
  1: { bpp: 4, d3d: 21, name: 'A8R8G8B8' },
  3: { bpp: 2, d3d: 23, name: 'R5G6B5' },
  5: { bpp: 2, d3d: 26, name: 'A4R4G4B4' },
  7: { bpp: 1, d3d: 28, name: 'A8' },
};

// 01 §7 完整的 9 项 bpp 表（直读 `th16:0x491b90`）。解码不用它，但算行距/校验 size 要用，
// 而且「某个 format 的 bpp 是多少」是一手 ✅ 的，与「位布局」是两回事。
export const THTX_BPP = [4, 4, 2, 2, 3, 2, 2, 1, 1];

function bppOf(format) {
  if (format >= 0 && format < THTX_BPP.length) return THTX_BPP[format];
  throw new Error(`thtx: format ${format} 超出 01-format.md §7 的 9 项表（0–8）`);
}

/**
 * 把一块源像素解成 RGBA8。
 * @param {Uint8Array} src 源像素，长度必须 >= pixels * bpp
 * @param {number} pixels 像素个数
 * @param {number} format `thtx.format`（01 §7）
 * @param {{ rgb565?: 'gpu'|'thanm', a8?: 'alpha'|'luminance' }} [opts]
 * @returns {Uint8Array} 长度 pixels*4，字节序 R,G,B,A
 */
export function formatToRgba(src, pixels, format, opts = {}) {
  const rgb565Mode = opts.rgb565 ?? 'gpu';
  const a8Mode = opts.a8 ?? 'alpha';
  if (rgb565Mode !== 'gpu' && rgb565Mode !== 'thanm') {
    throw new Error(`thtx: 不认识的 rgb565 模式 ${JSON.stringify(rgb565Mode)}（只接受 'gpu' / 'thanm'）`);
  }
  if (a8Mode !== 'alpha' && a8Mode !== 'luminance') {
    throw new Error(`thtx: 不认识的 a8 模式 ${JSON.stringify(a8Mode)}（只接受 'alpha' / 'luminance'）`);
  }
  const info = THTX_FORMAT[format];
  if (!info) {
    throw new Error(
      `thtx: format ${format} 未实现 —— 01-format.md §7 的 9 项表里只有 bpp 与 D3DFORMAT 是一手 ✅，` +
        '位布局是按 SDK 名字翻的 🟡；真实 th16 数据的 thtx.format 只出现 1/3/5/7，' +
        '其余照名字猜位布局等于编证据，所以这里拒绝解码。',
    );
  }
  const need = pixels * info.bpp;
  if (src.length < need) {
    throw new Error(`thtx: 源像素不足，format ${format}（bpp ${info.bpp}）× ${pixels} 像素需要 ${need} 字节，只有 ${src.length}`);
  }
  const out = new Uint8Array(pixels * 4);

  if (format === 1) {
    // A8R8G8B8：小端 u32 的字节序是 B,G,R,A。只需把 R 与 B 换位（同 thanm 的 BGRA8888 分支）。
    for (let i = 0; i < pixels; i++) {
      const s = i * 4;
      const d = i * 4;
      out[d + 0] = src[s + 2]; // R
      out[d + 1] = src[s + 1]; // G
      out[d + 2] = src[s + 0]; // B
      out[d + 3] = src[s + 3]; // A
    }
    return out;
  }

  if (format === 5) {
    // A4R4G4B4：u16 位段 A=15..12 R=11..8 G=7..4 B=3..0。
    // 小端 ⇒ byte0 = G<<4 | B、byte1 = A<<4 | R。
    // 每个 4 位通道扩成 8 位用 `n * 17`（= `n<<4 | n`，即 0x3 -> 0x33），与 thanm 一致，
    // 也正是 GPU 对 4 位通道做的事。
    for (let i = 0; i < pixels; i++) {
      const b0 = src[i * 2 + 0];
      const b1 = src[i * 2 + 1];
      const d = i * 4;
      out[d + 0] = (b1 & 0x0f) * 17; // R
      out[d + 1] = ((b0 >> 4) & 0x0f) * 17; // G
      out[d + 2] = (b0 & 0x0f) * 17; // B
      out[d + 3] = ((b1 >> 4) & 0x0f) * 17; // A
    }
    return out;
  }

  if (format === 3) {
    // R5G6B5：u16 位段 R=15..11 G=10..5 B=4..0，无 alpha（恒不透明）。
    for (let i = 0; i < pixels; i++) {
      const u = src[i * 2] | (src[i * 2 + 1] << 8);
      const r5 = (u >> 11) & 0x1f;
      const g6 = (u >> 5) & 0x3f;
      const b5 = u & 0x1f;
      const d = i * 4;
      if (rgb565Mode === 'gpu') {
        // 高位复制到低位——GPU 采样 D3DFMT_R5G6B5 时做的事，纯白能到 255。
        out[d + 0] = (r5 << 3) | (r5 >> 2);
        out[d + 1] = (g6 << 2) | (g6 >> 4);
        out[d + 2] = (b5 << 3) | (b5 >> 2);
      } else {
        // 逐位复刻 thanm image.c 的结果（含它蓝通道那个到不了 255 的 bug），只为工具链对拍。
        out[d + 0] = (r5 << 3) | (r5 & 1 ? 7 : 0);
        out[d + 1] = (g6 << 2) | (g6 & 1 ? 3 : 0);
        out[d + 2] = (b5 << 3) | (b5 & 1 ? 1 : 0);
      }
      out[d + 3] = 0xff;
    }
    return out;
  }

  // format 7：01 §7 一手证明 D3DFORMAT 值是 28 = `D3DFMT_A8`（**只有 alpha**），
  // 而 thanm 把它记作 `GRAY8` 并解成「亮度 + 不透明」。两者不等价，本文件默认按一手走：
  //   'alpha'（默认）    rgb = 0、a = 源字节 —— D3D9 规范下 A8 的 RGB 采样为 0（这一步是
  //                      规范推的 🟡，不是从 exe 里读出来的）
  //   'luminance'        rgb = a = 源字节 —— thanm 的口径，做 PNG 对拍时用
  // ⚠️ 这条分歧在 TH16 **不影响任何真实画面**：39 个 format-7 entry 全部是同一个
  //    `face/dummy.png` 占位图（实测 54 文件 423 entry）。所以选哪个都不会画错东西，
  //    但仍然按「一手 > 社区」把默认放在 A8 这边。
  for (let i = 0; i < pixels; i++) {
    const v = src[i];
    const d = i * 4;
    const rgb = a8Mode === 'luminance' ? v : 0;
    out[d + 0] = rgb;
    out[d + 1] = rgb;
    out[d + 2] = rgb;
    out[d + 3] = v;
  }
  return out;
}

/**
 * 解一个 THTX 块（01 §6：16 字节头 + 紧密像素）。
 * @param {Uint8Array} bytes 整个文件
 * @param {number} offset THTX 块在文件里的绝对偏移
 * @param {object} [opts] 透传给 formatToRgba
 */
export function decodeThtx(bytes, offset, opts = {}) {
  const view = new DataView(bytes.buffer, bytes.byteOffset, bytes.byteLength);
  // magic 在 01 §6 标 🟡（加载器不校验），但真实文件 417/417 都是 "THTX"——
  // 对不上多半是偏移算错了，报错比往下瞎解强。parseAnm 已经校验过一次，这里不重复。
  const format = view.getInt16(offset + 0x06, true); // MOVSX，01 §3.4：有符号
  const width = view.getInt16(offset + 0x08, true);
  const height = view.getInt16(offset + 0x0a, true);
  const size = view.getUint32(offset + 0x0c, true);
  if (width < 0 || height < 0) {
    throw new Error(`thtx: 负的尺寸 ${width}x${height} @ 0x${offset.toString(16)}（01 §6：字段是 s16，但真实数据全非负）`);
  }
  const pixels = width * height;
  const bpp = bppOf(format);
  const pixelBase = offset + 0x10;
  // 行距 = width * bpp，紧密无 padding（01 §6，`th16:0x46ccdb`/`th16:0x46cccb`）。
  const src = bytes.subarray(pixelBase, pixelBase + pixels * bpp);
  const rgba = formatToRgba(src, pixels, format, opts);
  return { format, width, height, size, bpp, rgba };
}
