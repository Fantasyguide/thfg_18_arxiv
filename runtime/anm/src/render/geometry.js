// 层 B · 几何生成 —— 「vm 状态 → 顶点数组」的**纯计算**。不碰 WebGL，可以纯 Node 测。
//
// 依据：
//   engine/anm/th16/10-sprite-geometry.md（下称「10」）—— sprite 四边形（mode 0/1/2/3）
//   engine/anm/th16/09-drawing.md（下称「09」）§3.2–§3.5 —— 帧尾几何三族（mode 9 / 13/14 / 24/25）
//   engine/anm/th16/02-vm-core.md（下称「02」）§3.3 —— 每个偏移对应哪个字段
//
// ── 宿主必须提供、VM 不建模的两样东西（ctx） ─────────────────────────────────────
//   `spriteUv`  —— sprite 的 UV 四角（`+0x3a8..+0x3c4`）。真机由 `set_sprite` 从贴图表算、
//                  帧尾 `recompute_uv_quad`（`flags_hi & 0x2000`）重算；骨架没有贴图表，
//                  所以由宿主从 `entry.sprites[id]` 与贴图尺寸算好传进来。
//                  形状：`[[u,v] × 4]`，角序与顶点一致 = TL, TR, BL, BR。
//   `resolutionRatio` —— `th16:0x4d9d34` 那个运行时分辨率比例因子（BSS，PE 里是 0）。默认 1。
//
// ── 顶点表示 ────────────────────────────────────────────────────────────────────
// `{ x, y, z, color, u, v }`，`color` 是 D3DCOLOR（u32，字节序 B,G,R,A —— 02 §3.3）。
// 返回 `{ mode, space, primitive, vertices }`：
//   `space: 'screen'` —— 顶点已是最终屏幕坐标（真机 FVF 带 XYZRHW）
//   `space: 'model'`  —— 模型坐标，要走 WORLD 矩阵（只有 mode 24/25，见 09 §3.4）
import { getRootVm, getParentVm } from '../vm/group-child.js';
import { ensureGeneralState } from '../vm/group-general.js';

const TWO_PI32 = Math.fround(2 * Math.PI);
const PI32 = Math.fround(Math.PI);

// 10 §2 / 09：`flags_lo` bit25–29。
export function renderModeOf(vm) {
  return (vm.flagsLo >>> 25) & 0x1f;
}

// ── 10 §2 的两张锚点表（直读 `th16:0x4a3048` / `th16:0x4a3078`，各 4 组 × 4 float）──
// 逐角系数，角序 TL, TR, BL, BR。
// 第 4 项（下标 3）**两张都不是有效锚点**：x 表那一项是 y 型图案（疑似复制粘贴残留）、
// y 表那一项全 0（四边形塌成一条线）。`421 anchor` 写的是 `n & 3` 所以值 3 可达——
// 照抄即可，不要为它另立分支（10 §2 ④）。
export const ANCHOR_X = [
  [-0.5, 0.5, -0.5, 0.5], // 0 = 中
  [0.0, 1.0, 0.0, 1.0], // 1 = 左
  [-1.0, 0.0, -1.0, 0.0], // 2 = 右
  [-0.5, -0.5, 0.5, 0.5], // 3 = 未定义
];
export const ANCHOR_Y = [
  [-0.5, -0.5, 0.5, 0.5], // 0 = 中
  [0.0, 0.0, 1.0, 1.0], // 1 = 顶
  [-1.0, -1.0, 0.0, 0.0], // 2 = 底
  [0.0, 0.0, 0.0, 0.0], // 3 = 未定义（退化）
];

// ── 角度归一：02 §2.1 更正块 / 03 §7.1。上限 34 步、两端都闭 `[-π, π]` ──────────────
// 三族的循环里 `θ += dθ` 之后都内联了这一套，省掉它在大 I0 × 大弧角时会与真机分岔。
function wrapAngle(x) {
  let v = Math.fround(x);
  let k = 0;
  if (v > PI32) {
    do {
      const old = k;
      v = Math.fround(v - TWO_PI32);
      k++;
      if (old > 0x20) break;
    } while (v > PI32);
  }
  if (-PI32 > v) {
    do {
      const old = k;
      v = Math.fround(v + TWO_PI32);
      k++;
      if (old > 0x20) break;
    } while (-PI32 > v);
  }
  return v;
}

// ── `th16:0x406a70` 的分辨率档 ────────────────────────────────────────────────────
// ★ 注意它比角计算里那份**多两个 case**：角那边（`th16:0x4633e2`）只认 `0x100000` 与
// `0x200000`，这里 `0x300000` 也走 ×ratio、`0x400000` 也走 ×ratio×0.5。如实照抄两份。
function positionResolutionFactor(vm, ratio) {
  const sel = vm.flagsHi & 0x700000;
  if (sel === 0x100000 || sel === 0x300000) return ratio;
  if (sel === 0x200000 || sel === 0x400000) return Math.fround(ratio * 0.5);
  return 1;
}
function cornerResolutionFactor(vm, ratio) {
  const sel = vm.flagsHi & 0x700000;
  if (sel === 0x100000) return ratio;
  if (sel === 0x200000) return Math.fround(ratio * 0.5);
  return 1;
}

/**
 * `th16:0x406c40` + `th16:0x406a70` ——「最终位置」，**递归**。
 *
 *   out = pos + pos_2 + entity_pos                       `0x406c50`–`0x406c93`
 *   out *= 分辨率因子                                     `0x406a88`–`0x406aec`
 *   若 root 非空且 !(flags_hi & 0x10000)：                 `0x406af1`–`0x406b0a`
 *       若 flags_hi & 0x800000（`314 attached`）：         `0x406b10`
 *           out.xy 绕 root.rotation.z 旋转                 `0x406b17`–`0x406b5d`
 *       out += final_pos(root)   ← 递归                    `0x406b62`–`0x406b97`
 *
 * 递归实际只有一层深（root 自己的 `+0x5ac == 0`，见 04 §6.2 ④′），但照抄成递归——
 * 万一将来发现某条路径上 `+0x5ac` 会指中间父，这里不用改。
 */
export function finalPos(vm, ctx = {}) {
  const ratio = ctx.resolutionRatio ?? 1;
  const g = ensureGeneralState(vm);
  const f = positionResolutionFactor(vm, ratio);
  let x = Math.fround((vm.pos.x + g.pos2.x + g.entityPos.x) * f);
  let y = Math.fround((vm.pos.y + g.pos2.y + g.entityPos.y) * f);
  let z = Math.fround((vm.pos.z + g.pos2.z + g.entityPos.z) * f);

  const root = getRootVm(vm);
  if (root && (vm.flagsHi & 0x10000) === 0) {
    if ((vm.flagsHi & 0x800000) !== 0) {
      // `314 attached`：绕**根的原始 rotation.z**（不是有效旋转）转 xy。
      // `th16:0x4054f0` = cos、`th16:0x405510` = sin（由 `0x406b30`–`0x406b5d` 的
      // `x·cos − y·sin` / `y·cos + x·sin` 配对反推）。
      const c = Math.cos(root.rotation.z);
      const s = Math.sin(root.rotation.z);
      const nx = Math.fround(x * c - y * s);
      const ny = Math.fround(y * c + x * s);
      x = nx;
      y = ny;
    }
    const rp = finalPos(root, ctx);
    x = Math.fround(x + rp.x);
    y = Math.fround(y + rp.y);
    z = Math.fround(z + rp.z);
  }
  return { x, y, z };
}

// ── 有效缩放：10 §3「× (scale × scale_2)，挂父再乘父的同一乘积」──────────────────
// 读的是**父**（`+0x5b0`）不是根。`th16:0x465f06`–`0x465f48` / `th16:0x4661db`–`0x466221`。
function effectiveScale(vm) {
  const g = ensureGeneralState(vm);
  let sx = Math.fround(vm.scale.x * g.scale2.x);
  let sy = Math.fround(vm.scale.y * g.scale2.y);
  const p = getParentVm(vm);
  if (p && (vm.flagsHi & 0x10000) === 0) {
    const pg = ensureGeneralState(p);
    sx = Math.fround(sx * Math.fround(p.scale.x * pg.scale2.x));
    sy = Math.fround(sy * Math.fround(p.scale.y * pg.scale2.y));
  }
  return { x: sx, y: sy };
}

// ── 环半径：09 §3.2「scale.y = 中心半径、scale.x = 环宽」────────────────────────────
// ⚠️ 父缩放这里把 r_out 乘父 `scale.x`、r_in 乘父 `scale.y`（09 §3.5 第 1 条，标 ❓）——
// 父 scale 非等比时环会畸变。两处（mode 9 与 13/14）写法一致，照抄，不"修"。
function ringRadii(vm, ctx) {
  const ratio = ctx.resolutionRatio ?? 1;
  const half = Math.fround(vm.scale.x * 0.5);
  let outer = Math.fround(vm.scale.y + half);
  let inner = Math.fround(vm.scale.y - half);
  const p = getParentVm(vm);
  if (p && (vm.flagsHi & 0x10000) === 0) {
    outer = Math.fround(outer * p.scale.x); // ← 是 .x，不是笔误
    inner = Math.fround(inner * p.scale.y); // ← 是 .y
  }
  const f = cornerResolutionFactor(vm, ratio);
  if (f !== 1) {
    outer = Math.fround(outer * f);
    inner = Math.fround(inner * f);
  }
  return { outer, inner };
}

function requireSpriteUv(ctx, who) {
  const uv = ctx.spriteUv;
  if (!Array.isArray(uv) || uv.length !== 4 || uv.some((p) => !Array.isArray(p) || p.length !== 2)) {
    throw new Error(
      `${who}: ctx.spriteUv 必须是 4 个 [u,v] —— 它对应 zAnmVm 的 \`+0x3a8..+0x3c4\`（sprite 的 UV 四角）。` +
        '真机由 set_sprite 从贴图表算、帧尾 recompute_uv_quad 重算；骨架没有贴图表，必须由宿主提供。',
    );
  }
  return uv;
}

// ══════════════════════════════════════════════════════════════════════════════════
// § sprite 四边形（mode 0/1/2/3）—— 10 §0 的完整公式
// ══════════════════════════════════════════════════════════════════════════════════
/**
 * @param {object} vm
 * @param {{ spriteUv: number[][], resolutionRatio?: number }} ctx
 */
export function buildSpriteQuad(vm, ctx = {}) {
  const uv = requireSpriteUv(ctx, 'buildSpriteQuad');
  const mode = renderModeOf(vm);
  const g = ensureGeneralState(vm);
  const ratio = ctx.resolutionRatio ?? 1;

  // ① 锚点 → ② × sprite_size − anchor_offset
  const h = (vm.flagsLo >>> 21) & 3;
  const v = (vm.flagsLo >>> 23) & 3;
  const xs = new Array(4);
  const ys = new Array(4);
  for (let i = 0; i < 4; i++) {
    xs[i] = Math.fround(Math.fround(ANCHOR_X[h][i] * vm.spriteSize.x) - g.anchorOffset.x);
    ys[i] = Math.fround(Math.fround(ANCHOR_Y[v][i] * vm.spriteSize.y) - g.anchorOffset.y);
  }
  // ③ 分辨率档（角这份只认 0x100000 / 0x200000，见上）
  const rf = cornerResolutionFactor(vm, ratio);
  if (rf !== 1) {
    for (let i = 0; i < 4; i++) {
      xs[i] = Math.fround(xs[i] * rf);
      ys[i] = Math.fround(ys[i] * rf);
    }
  }
  // ④ 有效缩放
  const sc = effectiveScale(vm);
  for (let i = 0; i < 4; i++) {
    xs[i] = Math.fround(xs[i] * sc.x);
    ys[i] = Math.fround(ys[i] * sc.y);
  }
  // ⑤ 旋转 —— **只有 mode 1/3**（10 §1 表：mode 0/2 走不旋转的 `th16:0x465c40`）
  if (mode === 1 || mode === 3) {
    // 角 = 有效 z 旋转（`th16:0x464dd0`），骨架已实现在 core.js 的 10026 读路径上；
    // 这里不重复递归，直接用同一条规则（自身 + 根，受 `flags_hi & 0x10000` 闸门）。
    const theta = effectiveRotZ(vm);
    const c = Math.cos(theta);
    const s = Math.sin(theta);
    for (let i = 0; i < 4; i++) {
      const nx = Math.fround(xs[i] * c - ys[i] * s);
      const ny = Math.fround(xs[i] * s + ys[i] * c);
      xs[i] = nx;
      ys[i] = ny;
    }
  }
  // ⑥ 平移；z 四角共用一个标量（`th16:0x466070`：entity_pos.z + pos.z + pos_2.z，
  //    **不经 0x406a70 的分辨率与根变换**——那一步写的 z 会被这里整个覆盖，10 §3）
  const fp = finalPos(vm, ctx);
  const z = Math.fround(g.entityPos.z + vm.pos.z + g.pos2.z);

  // 颜色：10 §5 的四种 colorMode（`flags_lo` bits17–18）
  const c1 = vm.color >>> 0;
  const c2 = g.color2 >>> 0;
  const colorMode = (vm.flagsLo >>> 17) & 3;
  let colors;
  if (colorMode === 0) colors = [c1, c1, c1, c1];
  else if (colorMode === 1) colors = [c2, c2, c2, c2];
  else if (colorMode === 2) colors = [c1, c2, c1, c2]; // 水平渐变
  else colors = [c1, c1, c2, c2]; // 垂直渐变

  // UV：10 §4。ku 只作用在 1、3，kv 只作用在 2、3 ⇒ **收缩恒以 TL 为锚**。
  const su = g.uvScrollPos.x;
  const sv = g.uvScrollPos.y;
  const ku = g.uvScale.x;
  const kv = g.uvScale.y;
  const us = [
    Math.fround(su + uv[0][0]),
    Math.fround(su + uv[0][0] + Math.fround((uv[1][0] - uv[0][0]) * ku)),
    Math.fround(su + uv[2][0]),
    Math.fround(su + uv[2][0] + Math.fround((uv[3][0] - uv[2][0]) * ku)),
  ];
  const vs = [
    Math.fround(sv + uv[0][1]),
    Math.fround(sv + uv[1][1]),
    Math.fround(sv + uv[0][1] + Math.fround((uv[2][1] - uv[0][1]) * kv)),
    Math.fround(sv + uv[1][1] + Math.fround((uv[3][1] - uv[1][1]) * kv)),
  ];

  const vertices = new Array(4);
  for (let i = 0; i < 4; i++) {
    vertices[i] = {
      x: Math.fround(xs[i] + fp.x),
      y: Math.fround(ys[i] + fp.y),
      z,
      color: colors[i],
      u: us[i],
      v: vs[i],
    };
  }
  return { mode, space: 'screen', primitive: 'quad', vertices };
}

// 10026 的规则（04 §6.2 ④′）：自身 rotation.z 累加根的，受 `flags_hi & 0x10000` 闸门，
// 结果 wrap 进 [-π, π]。与 core.js 的 computeRotZEffective 同式——那边是内部函数，
// 这里按同一条一手规则重写一份而不是把骨架的私有函数导出去。
export function effectiveRotZ(vm) {
  let z = vm.rotation.z;
  const root = getRootVm(vm);
  if (root && (vm.flagsHi & 0x10000) === 0) z = Math.fround(z + effectiveRotZ(root));
  return wrapAngle(z);
}

// ══════════════════════════════════════════════════════════════════════════════════
// § 圆环族（mode 9 / 13 / 14）—— 09 §3.2 与 §3.3
// ══════════════════════════════════════════════════════════════════════════════════
function ringVertex(x, y, z, color, u, v) {
  return { x, y, z, color, u, v };
}

/** mode 9 —— `600 textureCircle`：整圆环，循环 `I0-1` 次 + 末尾补一对闭合顶点。 */
export function buildRing(vm, ctx = {}) {
  const uv = requireSpriteUv(ctx, 'buildRing');
  const g = ensureGeneralState(vm);
  const n = (vm.i[0] | 0) - 1; // `th16:0x46333e` DEC
  const { outer, inner } = ringRadii(vm, ctx);
  const fp = finalPos(vm, ctx);
  const vertices = [];
  if (n <= 0) return { mode: 9, space: 'screen', primitive: 'trianglestrip', vertices };

  const dTheta = Math.fround(TWO_PI32 / n);
  const dv = Math.fround((vm.i[1] | 0) / n);
  // 外顶点恒 color_1；内顶点在 `flags_lo & 0x60000` 非 0 时用 color_2 ⇒ **本 mode 支持内外渐变**
  const cOuter = vm.color >>> 0;
  const cInner = (vm.flagsLo & 0x60000) !== 0 ? g.color2 >>> 0 : vm.color >>> 0;
  const uOuter = Math.fround(g.uvScrollPos.x + uv[0][0]);
  const uInner = Math.fround(uv[1][0] + g.uvScrollPos.x);

  let theta = vm.rotation.z; // 起始角 = rotation.z，不减半、不自判
  let vAcc = 0;
  for (let k = 0; k < n; k++) {
    const vv = Math.fround(g.uvScrollPos.y + vAcc);
    vertices.push(ringVertex(Math.fround(Math.cos(theta) * outer + fp.x), Math.fround(Math.sin(theta) * outer + fp.y), fp.z, cOuter, uOuter, vv));
    vertices.push(ringVertex(Math.fround(Math.cos(theta) * inner + fp.x), Math.fround(Math.sin(theta) * inner + fp.y), fp.z, cInner, uInner, vv));
    vAcc = Math.fround(vAcc + dv);
    theta = wrapAngle(Math.fround(theta + dTheta));
  }
  // `th16:0x46358b`–`th16:0x4635df`：把第 0/1 个顶点整块拷到末尾，只把 v 改写成累计终值。
  const vEnd = Math.fround(g.uvScrollPos.y + vAcc);
  vertices.push({ ...vertices[0], v: vEnd });
  vertices.push({ ...vertices[1], v: vEnd });
  return { mode: 9, space: 'screen', primitive: 'trianglestrip', vertices };
}

/** mode 13 / 14 —— `601 textureArcEven` / `602 textureArc`：圆环弧，循环 `I0` 次、不补闭合。 */
export function buildArc(vm, ctx = {}) {
  const uv = requireSpriteUv(ctx, 'buildArc');
  const mode = renderModeOf(vm);
  const g = ensureGeneralState(vm);
  const count = vm.i[0] | 0;
  const { outer, inner } = ringRadii(vm, ctx);
  const fp = finalPos(vm, ctx);
  const vertices = [];
  if (count <= 0) return { mode, space: 'screen', primitive: 'trianglestrip', vertices };

  const arc = vm.rotation.x; // 总弧角
  const denom = count - 1;
  const dTheta = Math.fround(arc / denom);
  const dv = Math.fround((vm.i[1] | 0) / denom);
  // ★ 601 与 602 的唯一分野（`th16:0x4636b1`/`th16:0x4636b6`）
  let theta =
    mode === 14
      ? wrapAngle(vm.rotation.z) // 602：从基准角起
      : wrapAngle(Math.fround(vm.rotation.z - Math.fround(arc * 0.5))); // 601：以基准角居中
  // 本族内外两个顶点**同一个颜色**（与 mode 9 不同）
  const color = (vm.flagsLo & 0x60000) !== 0 ? g.color2 >>> 0 : vm.color >>> 0;
  const uOuter = Math.fround(g.uvScrollPos.x + uv[0][0]);
  const uInner = Math.fround(uv[1][0] + g.uvScrollPos.x);

  let vAcc = 0;
  for (let k = 0; k < count; k++) {
    const vv = Math.fround(g.uvScrollPos.y + vAcc);
    vertices.push(ringVertex(Math.fround(Math.cos(theta) * outer + fp.x), Math.fround(Math.sin(theta) * outer + fp.y), fp.z, color, uOuter, vv));
    vertices.push(ringVertex(Math.fround(Math.cos(theta) * inner + fp.x), Math.fround(Math.sin(theta) * inner + fp.y), fp.z, color, uInner, vv));
    vAcc = Math.fround(vAcc + dv);
    theta = wrapAngle(Math.fround(theta + dTheta));
  }
  return { mode, space: 'screen', primitive: 'trianglestrip', vertices };
}

// ══════════════════════════════════════════════════════════════════════════════════
// § 3D 族（mode 24 / 25）—— 09 §3.4
// ══════════════════════════════════════════════════════════════════════════════════
/**
 * `609 textureCylinder3D`（mode 24）/ `610 textureRing3D`（mode 25）。
 * 共用一段代码，靠 `F1` 的**用法**分形状：24 两圈同半径、高 `±F1/2`（圆柱侧壁）；
 * 25 两圈同在 y=0、半径 `F2∓F1/2`（平面圆环）。圆在 **XZ 平面**、高沿 **Y**。
 * 顶点是**模型坐标**——本族不加 final_pos。
 */
export function build3d(vm, ctx = {}) {
  const uv = requireSpriteUv(ctx, 'build3d');
  const mode = renderModeOf(vm);
  const g = ensureGeneralState(vm);
  const count = vm.i[0] | 0;
  const vertices = [];
  if (count <= 0) return { mode, space: 'model', primitive: 'trianglestrip', vertices };

  const [F0, F1, F2, F3] = [vm.f[0], vm.f[1], vm.f[2], vm.f[3]];
  const denom = count - 1;
  const dTheta = Math.fround(F0 / denom);
  const dv = Math.fround((vm.i[1] | 0) / denom);
  let theta = wrapAngle(Math.fround(F3 - Math.fround(F0 * 0.5))); // 恒以 F3 居中

  let h = Math.fround(F1 * 0.5);
  let rA = F2;
  let rB = F2;
  if (mode === 25) {
    rA = Math.fround(F2 - Math.fround(F1 * 0.5));
    h = 0;
    rB = Math.fround(F2 + Math.fround(F1 * 0.5));
  }
  const color = (vm.flagsLo & 0x60000) !== 0 ? g.color2 >>> 0 : vm.color >>> 0;
  const uA = Math.fround(g.uvScrollPos.x + uv[0][0]);
  const uB = Math.fround(uv[1][0] + g.uvScrollPos.x);

  let vAcc = 0;
  for (let k = 0; k < count; k++) {
    const vv = Math.fround(g.uvScrollPos.y + vAcc);
    const c = Math.cos(theta);
    const s = Math.sin(theta);
    vertices.push(ringVertex(Math.fround(c * rA), h, Math.fround(s * rA), color, uA, vv));
    vertices.push(ringVertex(Math.fround(c * rB), Math.fround(-h), Math.fround(s * rB), color, uB, vv));
    vAcc = Math.fround(vAcc + dv);
    theta = wrapAngle(Math.fround(theta + dTheta));
  }
  return { mode, space: 'model', primitive: 'trianglestrip', vertices };
}

/** 按渲染模式分发。未覆盖的 mode 显式抛，不静默返回空。 */
export function buildGeometry(vm, ctx = {}) {
  const mode = renderModeOf(vm);
  if (mode === 0 || mode === 1 || mode === 2 || mode === 3) return buildSpriteQuad(vm, ctx);
  if (mode === 9) return buildRing(vm, ctx);
  if (mode === 13 || mode === 14) return buildArc(vm, ctx);
  if (mode === 24 || mode === 25) return build3d(vm, ctx);
  throw new Error(
    `buildGeometry: 渲染模式 ${mode} 未实现 —— 已覆盖 0/1/2/3（10-sprite-geometry.md）与 ` +
      '9/13/14/24/25（09-drawing.md §3）。mode 4/8/15 走 `th16:0x467410`、rect 族（16/20/21/22/26/27）走 ' +
      '`th16:0x468722`、poly 族（17/18/19）走 `th16:0x4689b3`，这三条路径还没逆（计划里的 B6）。',
  );
}
