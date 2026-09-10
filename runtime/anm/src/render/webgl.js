// 层 B · WebGL 后端。把 render-state.js 算出来的状态喂给 GL，并把 geometry.js 吐的顶点
// 批量提交。**只负责画，不认识 VM 的语义**——它只读 `vm.flagsLo/flagsHi`（为了 §4.2 的
// 四项状态），其余一律由调用方通过 geometry 传进来。
//
// 依据：engine/anm/th16/09-drawing.md §4（下称「09」）。
//
// ── 为什么 gl 是参数而不是内部 new 出来的 ────────────────────────────────────────
// 为了能在 Node 里测。`createRenderer(gl, …)` 接受任何实现了所需方法的对象，
// 测试里传一个记录调用的 mock 就能逐条断言「设了哪些状态、设了几次」。
//
// ── 三个必须知道的坑 ────────────────────────────────────────────────────────────
// 1. **canvas 必须 `premultipliedAlpha: false`**。09 §4.2 的 10 种 blend 都是按
//    「非预乘」写的（`SRC_ALPHA` × 源色）；WebGL 的 canvas 默认预乘，合成时会再乘一次
//    alpha，半透明处整体偏暗。`getContext('webgl2', { premultipliedAlpha: false })`。
// 2. **`MIN`/`MAX` 混合方程**（blend mode 8/9）在 **WebGL 1 需要 `EXT_blend_minmax`**，
//    WebGL 2 原生有。缺扩展时本模块显式抛，不静默退回 `FUNC_ADD`——那会把「只取亮部」
//    画成「加色」，是肉眼可见的错。
// 3. **alpha test 只能在 shader 里 discard**：`ALPHAREF=1` + `GREATEREQUAL` ⇒
//    丢弃 `a < 1/255`，且**只在 blend mode ≠ 3 时**（09 §4.1/§4.2）。
// 4. **canvas 还要 `alpha: false`**（不只是 `premultipliedAlpha: false`）。09 §4.2.1 ② ——
//    D3D9 没开 `SEPARATEALPHABLENDENABLE`，同一对因子也作用在 alpha 上，于是
//    **blend mode 2 / 4 / 6 在源不透明时把目标 alpha 算成 0**。真机的后备缓冲不透明、
//    alpha 不参与显示，所以没人在意；网页上画到带 alpha 的 canvas 里就是**打洞**，
//    背景会透出来。真实数据里 mode 2 用了 43 次、mode 4 用了 6 次，不是理论问题。
//    ⇒ `getContext('webgl2', { alpha: false, premultipliedAlpha: false })`。
import {
  ALPHA_TEST_THRESHOLD, GLOBAL_DEFAULTS, renderStateFor, TEXTURE_STAGE,
} from './render-state.js';

// ── shader ──────────────────────────────────────────────────────────────────────
// 顶点位置有两种空间（09 §3.4 / §4.4）：
//   screen —— 已是最终屏幕像素（真机 FVF 带 XYZRHW），用 uViewport 做正交变换；
//   model  —— 模型坐标（只有 mode 24/25），走调用方给的 uMatrix。
// 用同一对 shader、靠 uMatrix 是否为单位阵区分太隐晦，所以显式用一个 uniform 开关。
export const VERTEX_SHADER = `
attribute vec3 aPos;
attribute vec4 aColor;
attribute vec2 aUv;
uniform vec2 uViewport;   // 屏幕空间用：像素 → NDC
uniform mat4 uMatrix;     // 模型空间用：调用方给的 world×view×proj
uniform bool uModelSpace;
varying vec4 vColor;
varying vec2 vUv;
void main() {
  if (uModelSpace) {
    gl_Position = uMatrix * vec4(aPos, 1.0);
  } else {
    // D3D 屏幕坐标：原点左上、y 向下。NDC 原点居中、y 向上。
    float x = aPos.x / uViewport.x * 2.0 - 1.0;
    float y = 1.0 - aPos.y / uViewport.y * 2.0;
    gl_Position = vec4(x, y, 0.0, 1.0);
  }
  vColor = aColor;
  vUv = aUv;
}`;

// 09 §4.3：整条 texture stage 就是 `纹理 × (DIFFUSE 或 TFACTOR)` 一次乘法，
// `COLORARG1` 恒为纹理。别多写一层 op。
export const FRAGMENT_SHADER = `
precision mediump float;
uniform sampler2D uTex;
uniform float uAlphaRef;  // < 0 = 关掉 alpha test（blend mode 3）
varying vec4 vColor;
varying vec2 vUv;
void main() {
  vec4 c = texture2D(uTex, vUv) * vColor;
  if (c.a < uAlphaRef) discard;
  gl_FragColor = c;
}`;

const FLOATS_PER_VERTEX = 9; // xyz(3) + rgba(4) + uv(2)

// D3DCOLOR：02 §3.3 记的字节序是 `+0x520`=B `+0x521`=G `+0x522`=R `+0x523`=A，
// 小端读成 u32 就是 **0xAARRGGBB**（标准 D3DCOLOR_ARGB）。
export function unpackD3dColor(c) {
  const u = c >>> 0;
  return [((u >>> 16) & 0xff) / 255, ((u >>> 8) & 0xff) / 255, (u & 0xff) / 255, ((u >>> 24) & 0xff) / 255];
}

function glEnum(gl, name) {
  const v = gl[name];
  if (v === undefined) throw new Error(`webgl: 上下文没有枚举 ${name}`);
  return v;
}

/**
 * @param {WebGLRenderingContext} gl
 * @param {{ viewport?: {width:number, height:number}, hasBlendMinMax?: boolean }} [opts]
 */
export function createRenderer(gl, opts = {}) {
  const viewport = opts.viewport ?? { width: 640, height: 480 };
  // WebGL2 原生有 MIN/MAX；WebGL1 要 EXT_blend_minmax。调用方可以显式告知，
  // 否则按「有 getExtension 就问一次」探测。
  const hasMinMax =
    opts.hasBlendMinMax ?? (typeof gl.getExtension === 'function' ? !!gl.getExtension('EXT_blend_minmax') : true);

  const program = buildProgram(gl);
  const buffer = gl.createBuffer();
  const loc = {
    aPos: gl.getAttribLocation(program, 'aPos'),
    aColor: gl.getAttribLocation(program, 'aColor'),
    aUv: gl.getAttribLocation(program, 'aUv'),
    uViewport: gl.getUniformLocation(program, 'uViewport'),
    uMatrix: gl.getUniformLocation(program, 'uMatrix'),
    uModelSpace: gl.getUniformLocation(program, 'uModelSpace'),
    uTex: gl.getUniformLocation(program, 'uTex'),
    uAlphaRef: gl.getUniformLocation(program, 'uAlphaRef'),
  };

  // 09 §4.1 的全局重置。**只做一次**，与真机 `th16:0x45ba80` 的时机一致。
  gl.useProgram(program);
  gl.enable(glEnum(gl, 'BLEND')); // ALPHABLENDENABLE = 1
  gl.disable(glEnum(gl, 'DEPTH_TEST')); // ZFUNC = ALWAYS ⇒ 深度测试恒过，等价于关掉
  gl.disable(glEnum(gl, 'CULL_FACE')); // ❓ 09 §4.1 的扫描里没有 CULLMODE，未建模；
  // 关掉是安全侧（不会误剔除），若将来证明真机在剔除，这里要改。
  gl.uniform1i(loc.uTex, 0);
  gl.uniform2f(loc.uViewport, viewport.width, viewport.height);

  // §4.2 的四项状态在真机各带一份「与上次不同才 flush + 重设」的缓存
  // （`mgr+0x184fbb4/bba/bbc/bbd`）。这里照抄这个缓存——既是保真，也省掉每批一堆冗余调用。
  const cache = { blend: null, filter: null, addressU: null, addressV: null, texture: null, alphaRef: null, modelSpace: null };

  let batch = [];
  let batchKey = null;
  let batchPrimitive = null;

  function applyBlend(bs) {
    if (bs === null) return; // 09 §4.2：跳表越界 ⇒ **不设**，保持上一次的状态
    if (cache.blend && cache.blend.src === bs.src && cache.blend.dst === bs.dst && cache.blend.op === bs.op) return;
    if ((bs.op === 'MIN' || bs.op === 'MAX') && !hasMinMax) {
      throw new Error(
        `webgl: blend 方程 ${bs.op}（09 §4.2 的 blend mode 8/9）需要 WebGL2 或 EXT_blend_minmax。` +
          '静默退回 FUNC_ADD 会把「只取亮/暗部」画成「加色」，所以这里抛而不是降级。',
      );
    }
    gl.blendFunc(glEnum(gl, bs.src), glEnum(gl, bs.dst));
    gl.blendEquation(glEnum(gl, bs.op));
    cache.blend = bs;
  }
  function applySampler(st) {
    const T2D = glEnum(gl, 'TEXTURE_2D');
    if (cache.filter !== st.filter) {
      const f = glEnum(gl, st.filter);
      gl.texParameteri(T2D, glEnum(gl, 'TEXTURE_MAG_FILTER'), f);
      gl.texParameteri(T2D, glEnum(gl, 'TEXTURE_MIN_FILTER'), f);
      cache.filter = st.filter;
    }
    if (cache.addressU !== st.addressU) {
      gl.texParameteri(T2D, glEnum(gl, 'TEXTURE_WRAP_S'), glEnum(gl, st.addressU));
      cache.addressU = st.addressU;
    }
    if (cache.addressV !== st.addressV) {
      gl.texParameteri(T2D, glEnum(gl, 'TEXTURE_WRAP_T'), glEnum(gl, st.addressV));
      cache.addressV = st.addressV;
    }
  }
  function applyAlphaRef(bs) {
    // blend mode 3 是唯一把 ALPHATESTENABLE 设 0 的（09 §4.2）。
    const ref = bs === null ? cache.alphaRef : bs.alphaTest ? ALPHA_TEST_THRESHOLD : -1;
    if (ref === cache.alphaRef) return;
    gl.uniform1f(loc.uAlphaRef, ref);
    cache.alphaRef = ref;
  }

  function pushVertex(out, v) {
    const [r, g, b, a] = unpackD3dColor(v.color);
    out.push(v.x, v.y, v.z, r, g, b, a, v.u, v.v);
  }

  /**
   * 提交一份 geometry.js 的输出。
   * @param {{space:string, primitive:string, vertices:object[]}} geo
   * @param {{ vm: object, texture: WebGLTexture, matrix?: Float32Array }} ctx
   */
  function draw(geo, ctx) {
    if (!geo.vertices.length) return;
    const st = renderStateFor(ctx.vm);
    const modelSpace = geo.space === 'model';
    if (modelSpace && !ctx.matrix) {
      throw new Error(
        'webgl: model 空间的几何（mode 24/25）需要 ctx.matrix —— 真机的 WORLD 矩阵在 ' +
          '`th16:0x467d00` 里构造（读 rotation/scale/scale_2/uv_scale），那段还没逆（计划 B6）。' +
          '在逆出来之前由调用方自己给一个矩阵，而不是这里编一个。',
      );
    }
    const key = JSON.stringify([st, ctx.texture === undefined ? null : 'tex', geo.primitive, modelSpace]);
    if (batchKey !== null && (key !== batchKey || ctx.texture !== cache.texture)) flush();
    batchKey = key;
    batchPrimitive = geo.primitive;
    if (ctx.texture !== cache.texture) {
      gl.bindTexture(glEnum(gl, 'TEXTURE_2D'), ctx.texture);
      cache.texture = ctx.texture;
    }
    applyBlend(st.blend);
    applyAlphaRef(st.blend);
    applySampler(st);
    if (cache.modelSpace !== modelSpace) {
      gl.uniform1i(loc.uModelSpace, modelSpace ? 1 : 0);
      cache.modelSpace = modelSpace;
    }
    if (modelSpace) gl.uniformMatrix4fv(loc.uMatrix, false, ctx.matrix);

    // 四边形 → 两个三角形。角序 TL(0) TR(1) BL(2) BR(3)（10 §2）⇒ (0,1,2) + (1,3,2)。
    // 真机走的是 sprite 批量缓冲 + `TRIANGLELIST`（09 §4.4），这里等价展开。
    if (geo.primitive === 'quad') {
      const v = geo.vertices;
      for (const i of [0, 1, 2, 1, 3, 2]) pushVertex(batch, v[i]);
    } else if (geo.primitive === 'trianglestrip') {
      // strip 展开成 list，好和 quad 共用一个批次（真机 strip 是单独一次 DrawPrimitiveUP，
      // 展开只影响提交方式、不影响像素）。
      const v = geo.vertices;
      for (let i = 0; i + 2 < v.length; i++) {
        if (i % 2 === 0) { pushVertex(batch, v[i]); pushVertex(batch, v[i + 1]); pushVertex(batch, v[i + 2]); }
        else { pushVertex(batch, v[i + 1]); pushVertex(batch, v[i]); pushVertex(batch, v[i + 2]); }
      }
    } else {
      throw new Error(`webgl: 不认识的 primitive ${JSON.stringify(geo.primitive)}`);
    }
  }

  function flush() {
    if (!batch.length) return;
    const data = new Float32Array(batch);
    gl.bindBuffer(glEnum(gl, 'ARRAY_BUFFER'), buffer);
    gl.bufferData(glEnum(gl, 'ARRAY_BUFFER'), data, glEnum(gl, 'DYNAMIC_DRAW'));
    const stride = FLOATS_PER_VERTEX * 4;
    const FLOAT = glEnum(gl, 'FLOAT');
    gl.enableVertexAttribArray(loc.aPos);
    gl.vertexAttribPointer(loc.aPos, 3, FLOAT, false, stride, 0);
    gl.enableVertexAttribArray(loc.aColor);
    gl.vertexAttribPointer(loc.aColor, 4, FLOAT, false, stride, 12);
    gl.enableVertexAttribArray(loc.aUv);
    gl.vertexAttribPointer(loc.aUv, 2, FLOAT, false, stride, 28);
    gl.drawArrays(glEnum(gl, 'TRIANGLES'), 0, data.length / FLOATS_PER_VERTEX);
    batch = [];
    batchKey = null;
    batchPrimitive = null;
  }

  return {
    draw,
    flush,
    program,
    get pendingVertices() {
      return batch.length / FLOATS_PER_VERTEX;
    },
    get lastPrimitive() {
      return batchPrimitive;
    },
    setViewport(width, height) {
      viewport.width = width;
      viewport.height = height;
      gl.uniform2f(loc.uViewport, width, height);
    },
    dispose() {
      gl.deleteBuffer(buffer);
      gl.deleteProgram(program);
    },
  };
}

function buildProgram(gl) {
  const vs = compile(gl, glEnum(gl, 'VERTEX_SHADER'), VERTEX_SHADER);
  const fs = compile(gl, glEnum(gl, 'FRAGMENT_SHADER'), FRAGMENT_SHADER);
  const p = gl.createProgram();
  gl.attachShader(p, vs);
  gl.attachShader(p, fs);
  gl.linkProgram(p);
  if (typeof gl.getProgramParameter === 'function' && !gl.getProgramParameter(p, glEnum(gl, 'LINK_STATUS'))) {
    throw new Error(`webgl: program 链接失败：${gl.getProgramInfoLog?.(p)}`);
  }
  return p;
}
function compile(gl, type, src) {
  const s = gl.createShader(type);
  gl.shaderSource(s, src);
  gl.compileShader(s);
  if (typeof gl.getShaderParameter === 'function' && !gl.getShaderParameter(s, glEnum(gl, 'COMPILE_STATUS'))) {
    throw new Error(`webgl: shader 编译失败：${gl.getShaderInfoLog?.(s)}`);
  }
  return s;
}

// 便利：把 THTX 解出来的 RGBA8 传成一张贴图。过滤/寻址由 draw() 每批设置，这里不设。
export function uploadTexture(gl, { width, height, rgba }) {
  const tex = gl.createTexture();
  gl.bindTexture(glEnum(gl, 'TEXTURE_2D'), tex);
  gl.texImage2D(
    glEnum(gl, 'TEXTURE_2D'), 0, glEnum(gl, 'RGBA'), width, height, 0,
    glEnum(gl, 'RGBA'), glEnum(gl, 'UNSIGNED_BYTE'), rgba,
  );
  return tex;
}
