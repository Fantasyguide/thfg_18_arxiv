// ANM v8 VM ——General 组（opcode 400–438，39 条）。
//
// 依据：engine/anm/th16/07-general.md（下称「07」）§2 的 39 条指令表 + §3/§4/§5 的插值器
// 架构、缓动曲线编号与公式、mode 7/8/17 特判。每条 handler 上方注释指回具体 opcode 行
// （双向互指纪律，见 runtime/README.md）。本文件不重新论证任何语义结论——那是 engine/
// 层的事，这里只做「把结论翻成 JS」的实现决策。
//
// 消费 core.js 导出的 resolveArg / isVarBit（本组没有跳转，不需要 PC_HANDLED）。
//
// ══════════════════════════════════════════════════════════════════════════════════
// ★★★ 本文件的结构核心：插值器状态挂哪里 ★★★
// ══════════════════════════════════════════════════════════════════════════════════
// 07 §3：12 组帧尾插值器（`AnmVm::step_interpolators` th16:0x463b30）大半 `*Time` 指令
// 干的事就是往里填 (起点/终点/切线/闸门/mode)。这些结构（以及它们目标写回的字段：
// pos_2、color_2、scale_2、uv_scale、uv_scroll_vel、angular_velocity、scale_growth、
// anchor_offset、entity_pos）**全部不在 core.js 的 vm 契约里**——02-vm-core.md §3.3 记的
// `zAnmVm` 完整字段图比 I2 骨架大得多，core.js 只吃了 100/200 系需要的最小子集。
//
// 选型（可参考先例：flagsLo/flagsHi 最初由 group-settings.js 惰性建在 vm 顶层，后来
// 06 任务收口进了 createVm——见 group-settings.js 文件头的完整推演）：
//   (a) 比照 flagsLo/flagsHi 的先例，把 pos2/color2/scale2/… 这十来个新字段也直接惰性建
//       在 vm 顶层（`vm.pos2 = …`）。
//   (b) 把本组新增的一切状态（这十来个非契约字段 + 12 组插值器）收进**一个惰性建的命名
//       空间 `vm._general`**，只留一个入口函数 `ensureGeneralState(vm)`。
// 采用 **(b)**，理由与 flagsLo/flagsHi 先例的选择方向一致（都是「不碰 core.js，惰性建」），
// 但具体形状不同，原因是数量级不一样：flagsLo/flagsHi 只有 2 个标量字段，直接放 vm 顶层
// 不会弄乱契约；本组一次性要挂 9 个非插值字段 + 12 组插值器结构（每组 5–6 个子字段），
// 如果都摊平到 vm 顶层，会让「哪些字段是 core.js 契约、哪些是 General 组自己的记账」这条
// 边界彻底看不出来——`vm._general` 这一个前缀 `_`（core.js 自己的记账字段也用这个前缀，
// 语义是「内部细节，不保证跨版本稳定」）就把这条边界画清楚了，且未来 core.js 收口时，
// 是「挑几个字段升格进契约」还是「把 vm._general 整个提升成公开命名空间」是一个更容易
// 单独判断的问题（不需要先把十几个散落的顶层字段挑出来）。
//
// **收口建议（写给持有 core.js 的人）**：等这批字段的契约稳定下来，可以参考 flagsLo/
// flagsHi 的路径把 `vm._general` 拆开：pos2/color2/scale2/uvScale/uvScrollVel/
// angularVelocity/scaleGrowth/anchorOffset/entityPos 这 9 个直接升格进 `createVm`
// 的顶层字段（各自默认值见下方 `ensureGeneralState`，都已经标好对应的 zAnmVm 偏移和
// AnmVm::wipe 的初值依据）；12 组插值器结构本组建议保持嵌套（`vm._interp` 之类），
// 因为它们不是「哪个 opcode 组都可能要读」的通用状态，只有 stepInterpolators 自己用。
//
// ══════════════════════════════════════════════════════════════════════════════════
// ★★★ frameTail 接线需求（不改 core.js，写在这里）★★★
// ══════════════════════════════════════════════════════════════════════════════════
// core.js 的 `frameTail(vm)` 目前只做 `timerAdjust(vm, vm._gameSpeed)`。07 §3/§10 坑表
// 「t 受减速影响」：12 组插值器的内嵌 zTimer 也以 GAME_SPEED 为步长，且是**帧尾**才 step
// （`AnmVm::step_interpolators` 是 `AnmVm_run` 帧尾几何的一部分，02-vm-core.md §2 ⑥）。
// 本文件按 brief 要求导出 `stepInterpolators(vm)`，但**不会自己挂到 frameTail**——
// 需要 core.js 的 frameTail 在 `timerAdjust(vm, vm._gameSpeed)` **之后**（或之前——12 组
// 插值器彼此独立、也不读 vm.time，与脚本时间推进的先后顺序不影响插值结果本身；但为了
// 跟 02-vm-core.md §2 的帧尾顺序保持字面一致，建议放在 `timerAdjust` 之后）额外调用一次
// `stepInterpolators(vm)`。调用方需要自己 `import { stepInterpolators } from
// './group-general.js'`（或把它并入更高层的组合入口）。
import { resolveArg, isVarBit } from './core.js';

// ── 变量读写：转发 core.js（不重新实现变量表）──────────────────────────────────────
function readArg(vm, instr, index, kind) {
  return resolveArg(vm, instr.args[index], isVarBit(instr.paramMask, index), kind);
}
// 6 条「$ 记号用错了」的指令（07 §2「$ 记号的更正」）：arg0 根本不查 param_mask，是裸
// movzx——只能是字面量。这里不走 resolveArg（那会在 mask 置位时误当成变量号去查表），
// 直接读 args[0] 的低位。
function literalByte(instr, index = 0) {
  return instr.args[index] & 0xff;
}
function literalWord(instr, index = 0) {
  return instr.args[index] & 0xffff;
}

// 「按值写位」惯用法，与 group-settings.js 的同名工具逐字节相同——06-settings.md §1.1
// 记过的「`dst ^= (src^dst)&mask`，等价于把 mask 位设成 value 的同位值」不是 toggle。
// 复制而不是从 group-settings.js 导入是刻意的（两个组本来就没有互相依赖关系，本仓已有
// 「复制优于新增公共依赖」的先例，见 group-math.js/group-settings.js 各自文件头）。
function setBits(current, mask, shift, value) {
  const m = mask >>> 0;
  return ((current & ~m) | ((value << shift) & m)) >>> 0;
}

// ── D3DCOLOR 字节布局（02-vm-core.md §3.3：`+0` = B、`+1` = G、`+2` = R、`+3` = A，
// 小端 u32 里就是 `(A<<24)|(R<<16)|(G<<8)|B`）───────────────────────────────────────
function colorGetByte(color, channel) {
  switch (channel) {
    case 'B': return color & 0xff;
    case 'G': return (color >>> 8) & 0xff;
    case 'R': return (color >>> 16) & 0xff;
    case 'A': return (color >>> 24) & 0xff;
    default: throw new Error(`group-general: 未知的颜色通道 ${channel}`);
  }
}
function colorSetByte(color, channel, value) {
  const v = value & 0xff; // 07 §10 坑表「颜色是 8 位截断」：写入前先截到一个字节
  switch (channel) {
    case 'B': return ((color & ~0xff) | v) >>> 0;
    case 'G': return ((color & ~0xff00) | (v << 8)) >>> 0;
    case 'R': return ((color & ~0xff0000) | (v << 16)) >>> 0;
    case 'A': return ((color & ~0xff000000) | (v << 24)) >>> 0;
    default: throw new Error(`group-general: 未知的颜色通道 ${channel}`);
  }
}
function colorSetRgb(color, r, g, b) {
  let c = colorSetByte(color, 'R', r);
  c = colorSetByte(c, 'G', g);
  c = colorSetByte(c, 'B', b);
  return c;
}

// ── 角度最短路径折叠：与 core.js 的 wrapAngle34Step / group-math.js 的 reduceAngle129
// 是同一段 math-jumps §7.1 算法的第三份拷贝（本仓已有的先例：这段“34 步循环折叠进
// [-π,π]”的小工具三个文件各自维护一份，而不是从 core.js 导出——core.js 那份是私有的
// 内部实现细节，不是它对外的契约）。InterpAngle::step（07 §5.2）默认模式用它算
// target-start 的最短路径角度差；mode 7/17 也用它折叠累加后的角度，避免无界增长。
const PI32 = Math.fround(Math.PI);
const TWO_PI32 = Math.fround(2 * Math.PI);
function wrapAngle32(x) {
  let v = Math.fround(x);
  let k = 0;
  if (v > PI32) {
    do {
      const old = k;
      v = Math.fround(v - TWO_PI32);
      k++;
      if (old > 32) break;
    } while (v > PI32);
  }
  if (-PI32 > v) {
    do {
      const old = k;
      v = Math.fround(v + TWO_PI32);
      k++;
      if (old > 32) break;
    } while (-PI32 > v);
  }
  return v;
}

// ══════════════════════════════════════════════════════════════════════════════════
// § 惰性 vm 扩展：vm._general
// ══════════════════════════════════════════════════════════════════════════════════
// 默认值依据 02-vm-core.md §3.3「默认值来自 AnmVm::wipe」：整块 memset 0，只有
// scale/scale_2/uv_scale 覆盖成 (1,1)、color_1 覆盖成 0xffffffff；**color_2 没有被覆盖，
// 默认是 0x00000000**（很容易想当然写成 0xffffffff，与 color_1 搞混）。entity_pos
// `+0x5e0` 是 wipe **不清**的一块——但那是「wipe 时保留旧值」，本骨架里 vm 是全新创建的，
// 没有「旧值」这回事，取 0 是唯一合理的初值。
function ensureGeneralState(vm) {
  if (vm._general) return vm._general;
  vm._general = {
    pos2: { x: 0, y: 0, z: 0 }, // +0x4e0，`flags_lo & 0x400` 时 400/pos 写这里而不是 vm.pos
    color2: 0x00000000, // +0x524（color_2），★ 默认 0，不是 0xffffffff（见上）
    scale2: { x: 1, y: 1 }, // +0x58，wipe 覆盖成 (1,1)（op 434 的目标）
    uvScale: { x: 1, y: 1 }, // +0x68，wipe 覆盖成 (1,1)
    uvScrollVel: { x: 0, y: 0 }, // +0x3c8
    angularVelocity: { x: 0, y: 0, z: 0 }, // +0x44（415 写）
    scaleGrowth: { x: 0, y: 0 }, // +0x60（416 写）
    anchorOffset: { x: 0, y: 0 }, // +0x80（436 写）
    entityPos: { x: 0, y: 0, z: 0 }, // +0x5e0（422 posAdopt 读/清）
    interp: createInterpolators(),
  };
  return vm._general;
}

// ══════════════════════════════════════════════════════════════════════════════════
// § 12 组插值器：结构 + 通用 step
// ══════════════════════════════════════════════════════════════════════════════════
// 07 §3 的结构表：start/target/tangent1/tangent2/output 都是「每分量一份」的定长数组，
// 外加标量 currentF/current（内嵌 zTimer）、gate（剩余帧闸门）、mode。
function zeros(n) {
  return new Array(n).fill(0);
}
function makeInterpEntry(dims) {
  return {
    start: zeros(dims),
    target: zeros(dims),
    tangent1: zeros(dims),
    tangent2: zeros(dims),
    output: zeros(dims),
    currentF: 0,
    current: 0,
    gate: 0,
    mode: 0,
  };
}
// 12 组，键名任意取（不是 vm 契约的一部分），dims/kind 决定 stepGeneric 怎么算、
// apply 决定算完的 output 写回哪个真实字段。kind：'float' 普通浮点插值；
// 'int' 8 位色彩通道（写回前 &0xff 截断，07 §10「颜色是 8 位截断」）；
// 'angle' 走最短路径折叠（07 §5.2，仅 rotation2d 这一组）。
function createInterpolators() {
  return {
    pos: makeInterpEntry(3),
    color1Rgb: makeInterpEntry(3),
    color1A: makeInterpEntry(1),
    rotation: makeInterpEntry(3),
    rotation2d: makeInterpEntry(1),
    scale: makeInterpEntry(2),
    scale2: makeInterpEntry(2),
    uvScale: makeInterpEntry(2),
    color2Rgb: makeInterpEntry(3),
    color2A: makeInterpEntry(1),
    uvScrollVelX: makeInterpEntry(1),
    uvScrollVelY: makeInterpEntry(1),
  };
}

// 用 07 §2 表里明确写出的「起点 = 当前值」初始化一组插值器：start 从**当前**目标字段读、
// tangent1/tangent2 默认 (0,…,0)（07 §5.1：标准 `*Time` 家族的切线恒读全局 `0x4d9dc4`，
// 该全局是 BSS 零段全文无一处写 ⇒ 恒 0；只有 420 posBezier 显式传入非零切线，见下）。
function beginInterp(entry, currentValue, target, gate, mode, tangent1, tangent2) {
  entry.start = currentValue.slice();
  entry.target = target.slice();
  entry.tangent1 = tangent1 ? tangent1.slice() : zeros(currentValue.length);
  entry.tangent2 = tangent2 ? tangent2.slice() : zeros(currentValue.length);
  entry.output = currentValue.slice();
  entry.currentF = 0;
  entry.current = 0;
  entry.gate = gate | 0;
  entry.mode = mode | 0;
}

// ── § 4：31 个缓动曲线（mode 1–31；mode 0/负数/>31 走线性）────────────────────────
// 07 §4.1 表 + §4.2 高斯族闭合式（★ 2026-09-09 审阅更正：自变量是 `1-u` 不是 `u`，
// 见 07 §4.2 的一手反汇编证据）。GAUSS_C 五个常量的顺序对应 mode 22–26（下凹）与
// 27–31（上凸，同一组 c，公式套 `1 - g_c(1-u)`）。
const HALF_PI32 = Math.fround(Math.PI / 2);
const GAUSS_C = [0.25, 0.3, 0.35, 0.38, 0.4];
function gaussC(u, c) {
  const w = (1 - c) * (1 - c);
  const s = (c * c) / w;
  const n = 1 - s;
  return ((u - c) * (u - c)) / w / n - s / n;
}
function easeValue(mode, u) {
  switch (mode) {
    case 0: return u;
    case 1: return u * u;
    case 2: return u * u * u;
    case 3: return u * u * u * u;
    case 4: return 1 - (1 - u) ** 2;
    case 5: return 1 - (1 - u) ** 3;
    case 6: return 1 - (1 - u) ** 4;
    case 9: return u < 0.5 ? 2 * u * u : 1 - 2 * (1 - u) * (1 - u);
    case 10: return u < 0.5 ? 4 * u ** 3 : 1 - 4 * (1 - u) ** 3;
    case 11: return u < 0.5 ? 8 * u ** 4 : 1 - 8 * (1 - u) ** 4;
    case 12: return u < 0.5 ? 0.5 - 0.5 * (1 - 2 * u) ** 2 : 0.5 + 0.5 * (2 * u - 1) ** 2;
    case 13: return u < 0.5 ? 0.5 - 0.5 * (1 - 2 * u) ** 3 : 0.5 + 0.5 * (2 * u - 1) ** 3;
    case 14: return u < 0.5 ? 0.5 - 0.5 * (1 - 2 * u) ** 4 : 0.5 + 0.5 * (2 * u - 1) ** 4;
    case 15: return 0;
    case 16: return 1;
    case 18: return Math.fround(Math.sin(HALF_PI32 * u)); // ease-out：f'(0)=π/2
    case 19: return Math.fround(1 - Math.cos(HALF_PI32 * u)); // ease-in：f'(0)=0
    case 20: return u < 0.5 ? Math.fround(0.5 * Math.sin(Math.PI * u)) : Math.fround(1 - 0.5 * Math.sin(Math.PI * u));
    case 21:
      return u < 0.5
        ? Math.fround(0.5 * (1 - Math.cos(Math.PI * u)))
        : Math.fround(0.5 + 0.5 * Math.sin((2 * u - 1) * (Math.PI / 2)));
    default:
      if (mode >= 22 && mode <= 26) return gaussC(u, GAUSS_C[mode - 22]);
      // ★ 2026-09-09 审阅更正：自变量是 `1-u`，不是 `u`——写成 `1-gaussC(u,c)` 会让
      // 整段动画倒放（f(0)=1,f(1)=0）且没有过冲峰，见 07 §4.2 的一手反汇编证据。
      if (mode >= 27 && mode <= 31) return 1 - gaussC(1 - u, GAUSS_C[mode - 27]);
      return u; // mode 0 / 负数 / >31：线性（07 §4.1 表最后一行）
  }
}

// ── § 5：Hermite（mode 8 对标准 *Time = 切线恒 0 的 smoothstep；420 posBezier 显式传
// 非零切线才是真正的三次 Hermite）──────────────────────────────────────────────────
// 07 §5.1：`output = (2u³−3u²+1)·start + (u³−2u²+u)·tangent1 + (u³−u²)·tangent2 +
// (3u²−2u³)·target`。
function hermite(u, start, target, t1, t2) {
  const u2 = u * u;
  const u3 = u2 * u;
  const h00 = 2 * u3 - 3 * u2 + 1;
  const h10 = u3 - 2 * u2 + u;
  const h01 = u3 - u2;
  const h11 = 3 * u2 - 2 * u3;
  return h00 * start + h10 * t1 + h01 * t2 + h11 * target;
}

// ── 单组插值器的一帧 step（07 §3 公共骨架 + §5 mode 7/8/17 特判）──────────────────
// 返回 true 表示这一步真的动了（gate 原本 > 0），调用方据此决定要不要把 output 写回
// 真实字段——gate<=0（从未激活，或已经跑完）时什么都不做，字段维持上一次的值。
function stepGeneric(vm, entry, kind) {
  if (entry.gate <= 0) return false;
  entry.currentF += vm._gameSpeed; // 速度源 = GAME_SPEED（02-vm-core.md §3.2：内嵌 zTimer 恒以它为步长）
  entry.current = Math.trunc(entry.currentF); // CVTTSS2SI 截零，不是 floor（同 core.js timerAdjust）
  const dims = entry.start.length;
  const mode = entry.mode;

  if (entry.current < entry.gate) {
    // ── 中间帧 ──────────────────────────────────────────────────────────────
    if (mode === 7) {
      // 07 §5「mode 7 = 逐帧相对位移」：start += target；output = start（累计后的值）。
      for (let d = 0; d < dims; d++) entry.start[d] += entry.target[d];
      if (kind === 'angle') entry.start[0] = wrapAngle32(entry.start[0]); // §5.2：mode 7/17 先对角度 wrap
      entry.output = entry.start.slice();
    } else if (mode === 17) {
      // 07 §5「mode 17 = 逐帧加速」：start += tangent2；tangent2 += target；output = start。
      for (let d = 0; d < dims; d++) {
        entry.start[d] += entry.tangent2[d];
        entry.tangent2[d] += entry.target[d];
      }
      if (kind === 'angle') entry.start[0] = wrapAngle32(entry.start[0]);
      entry.output = entry.start.slice();
    } else {
      const u = entry.currentF / entry.gate; // 07 §4「u = current_f / gate」
      if (mode === 8) {
        // 07 §5.1：标准 *Time 家族切线恒 0 ⇒ smoothstep；420 posBezier 显式传非零切线
        // 才是真 Hermite。InterpAngle 的 mode 8 用另外两个未逐条解码的角度 helper
        // （07 §5.2，🟡）——本实现对 angle 也套同一条 Hermite 公式（切线仍为 0，退化成
        // smoothstep），**不**对 target-start 做最短路径折叠（那是 default 模式的行为，
        // 07 §5.2 没有证据说 mode 8 也走同一条折叠路径）——这是本文件在 🟡 结论上的
        // 显式取舍，见任务报告。
        for (let d = 0; d < dims; d++) {
          entry.output[d] = hermite(u, entry.start[d], entry.target[d], entry.tangent1[d], entry.tangent2[d]);
        }
      } else {
        const e = easeValue(mode, u);
        for (let d = 0; d < dims; d++) {
          if (kind === 'angle') {
            // 07 §5.2：default 模式走「最短路径角度差」，target-start 先折进 [-π,π]。
            const delta = wrapAngle32(entry.target[d] - entry.start[d]);
            entry.output[d] = entry.start[d] + delta * e;
          } else {
            entry.output[d] = entry.start[d] + (entry.target[d] - entry.start[d]) * e;
          }
        }
      }
    }
  } else {
    // ── 结束帧（07 §3「否则闸门=0，output=target（mode 7/17 时=累计后的 start）」）──
    entry.gate = 0;
    if (mode === 7 || mode === 17) {
      entry.output = entry.start.slice();
    } else {
      entry.output = entry.target.slice();
    }
  }

  if (kind === 'int') {
    for (let d = 0; d < dims; d++) entry.output[d] = entry.output[d] & 0xff; // 颜色通道 8 位截断
  }
  return true;
}

// ── 12 组的「写回真实字段」——07 §3 表的目标字段 + 02-vm-core.md §3.4 的「step 后置的
// 脏位」（rotation/rotation2d → flags_lo|=4；scale/scale_2 → flags_lo|=8；
// uv_scale → flags_lo|=0x10；其余组没有脏位）。★ 已知的简化：pos 组恒写 vm.pos，
// **不**复刻 400/pos 那样按 `flags_lo & 0x400` 改写 pos_2——07 §2 表只对「400 pos」这一
// 条直接写指令记了这条分流，没有一手证据说 407/420/433 这三条把 pos 写进插值器时也会在
// **每次 step** 时重新检查这个位（该位在 07 文档范围内也没有任何指令能在插值过程中翻转
// 它）；按「偏最小实现」处理成恒写 vm.pos，在报告里标注为一个待确认点。
function applyPos(vm, general, output) {
  vm.pos.x = output[0];
  vm.pos.y = output[1];
  vm.pos.z = output[2];
}
function applyColor1Rgb(vm, general, output) {
  vm.color = colorSetRgb(vm.color, output[0], output[1], output[2]);
}
function applyColor1A(vm, general, output) {
  vm.color = colorSetByte(vm.color, 'A', output[0]);
}
function applyRotation(vm, general, output) {
  vm.rotation.x = output[0];
  vm.rotation.y = output[1];
  vm.rotation.z = output[2];
  vm.flagsLo = (vm.flagsLo | 4) >>> 0;
}
function applyRotation2d(vm, general, output) {
  vm.rotation.z = output[0];
  vm.flagsLo = (vm.flagsLo | 4) >>> 0;
}
function applyScale(vm, general, output) {
  vm.scale.x = output[0];
  vm.scale.y = output[1];
  vm.flagsLo = (vm.flagsLo | 8) >>> 0;
}
function applyScale2(vm, general, output) {
  general.scale2.x = output[0];
  general.scale2.y = output[1];
  vm.flagsLo = (vm.flagsLo | 8) >>> 0;
}
function applyUvScale(vm, general, output) {
  general.uvScale.x = output[0];
  general.uvScale.y = output[1];
  vm.flagsLo = (vm.flagsLo | 0x10) >>> 0;
}
function applyColor2Rgb(vm, general, output) {
  general.color2 = colorSetRgb(general.color2, output[0], output[1], output[2]);
}
function applyColor2A(vm, general, output) {
  general.color2 = colorSetByte(general.color2, 'A', output[0]);
}
function applyUvScrollVelX(vm, general, output) {
  general.uvScrollVel.x = output[0];
}
function applyUvScrollVelY(vm, general, output) {
  general.uvScrollVel.y = output[0];
}

// 12 组的静态描述（key 对到 createInterpolators 的字段名；顺序照抄 07 §3/02-vm-core.md
// §3.4 表的行序，纯为了可读性，彼此独立、顺序本身不影响结果——除了 rotation 与
// rotation2d 理论上可以同一帧都激活、都写 vm.rotation.z，此时后处理的组「获胜」，这与
// 真实引擎里两个独立结构谁在 step_interpolators 里排后面一致，不是本文件引入的分歧）。
const INTERP_GROUPS = [
  { key: 'pos', kind: 'float', apply: applyPos },
  { key: 'color1Rgb', kind: 'int', apply: applyColor1Rgb },
  { key: 'color1A', kind: 'int', apply: applyColor1A },
  { key: 'rotation', kind: 'float', apply: applyRotation },
  { key: 'rotation2d', kind: 'angle', apply: applyRotation2d },
  { key: 'scale', kind: 'float', apply: applyScale },
  { key: 'scale2', kind: 'float', apply: applyScale2 },
  { key: 'uvScale', kind: 'float', apply: applyUvScale },
  { key: 'color2Rgb', kind: 'int', apply: applyColor2Rgb },
  { key: 'color2A', kind: 'int', apply: applyColor2A },
  { key: 'uvScrollVelX', kind: 'float', apply: applyUvScrollVelX },
  { key: 'uvScrollVelY', kind: 'float', apply: applyUvScrollVelY },
];

/**
 * 帧尾插值器步进——07 §3「AnmVm::step_interpolators」。★ 不在本文件里自动挂到任何帧尾
 * 钩子（brief 硬约束：不改 core.js）；调用方（core.js 的 frameTail，或更高层的组合入口）
 * 需要在每帧的帧尾显式调用一次。见本文件顶部「frameTail 接线需求」。
 * @param {ReturnType<import('./core.js').createVm>} vm
 */
export function stepInterpolators(vm) {
  const general = ensureGeneralState(vm);
  for (const group of INTERP_GROUPS) {
    const entry = general.interp[group.key];
    const changed = stepGeneric(vm, entry, group.kind);
    if (changed) group.apply(vm, general, entry.output);
  }
}

// ══════════════════════════════════════════════════════════════════════════════════
// § 39 条 handler
// ══════════════════════════════════════════════════════════════════════════════════
export const generalGroup = {
  // 07-general.md #400 —— ★ 求值顺序 (2,1,0)：先读 z、再 y、再 x（07 §2「$ 记号的更正」
  // 之后的求值顺序表：400 与 415 同为 (2,1,0) 降序，原文曾只点名 415，容易让人以为 400
  // 是升序——这正是本组「求值次序」判据要防的默认假设）。`flags_lo & 0x400` 时写
  // `pos_2` 而不是 `pos`（07 §2 表；`vm._general.pos2` 见文件头字段说明）。
  400: (vm, args, instr) => {
    const general = ensureGeneralState(vm);
    const z = readArg(vm, instr, 2, 'float');
    const y = readArg(vm, instr, 1, 'float');
    const x = readArg(vm, instr, 0, 'float');
    if ((vm.flagsLo & 0x400) !== 0) {
      general.pos2.x = x;
      general.pos2.y = y;
      general.pos2.z = z;
    } else {
      vm.pos.x = x;
      vm.pos.y = y;
      vm.pos.z = z;
    }
  },
  // 07-general.md #401 —— 求值顺序 (0,1,2) 升序。
  401: (vm, args, instr) => {
    const x = readArg(vm, instr, 0, 'float');
    const y = readArg(vm, instr, 1, 'float');
    const z = readArg(vm, instr, 2, 'float');
    vm.rotation.x = x;
    vm.rotation.y = y;
    vm.rotation.z = z;
    vm.flagsLo = (vm.flagsLo | 4) >>> 0;
  },
  // 07-general.md #402 —— 求值顺序 (0,1) 升序。
  402: (vm, args, instr) => {
    vm.scale.x = readArg(vm, instr, 0, 'float');
    vm.scale.y = readArg(vm, instr, 1, 'float');
    vm.flagsLo = (vm.flagsLo | 8) >>> 0;
  },
  // 07-general.md #403 —— 单参 (0)。alpha 是字节（写入前 colorSetByte 自己会截断）。
  403: (vm, args, instr) => {
    const a = readArg(vm, instr, 0, 'int');
    vm.color = colorSetByte(vm.color, 'A', a);
  },
  // 07-general.md #404 —— 求值顺序 (0,1,2) 升序。
  404: (vm, args, instr) => {
    const r = readArg(vm, instr, 0, 'int');
    const g = readArg(vm, instr, 1, 'int');
    const b = readArg(vm, instr, 2, 'int');
    vm.color = colorSetRgb(vm.color, r, g, b);
  },
  // 07-general.md #405 —— 单参 (0)。
  405: (vm, args, instr) => {
    const general = ensureGeneralState(vm);
    const a = readArg(vm, instr, 0, 'int');
    general.color2 = colorSetByte(general.color2, 'A', a);
  },
  // 07-general.md #406 —— 求值顺序 (0,1,2) 升序。
  406: (vm, args, instr) => {
    const general = ensureGeneralState(vm);
    const r = readArg(vm, instr, 0, 'int');
    const g = readArg(vm, instr, 1, 'int');
    const b = readArg(vm, instr, 2, 'int');
    general.color2 = colorSetRgb(general.color2, r, g, b);
  },
  // 07-general.md #407 posTime —— ★ 求值顺序 (0,4,3,2)：t 先读，再 z、y、x（07 §2 表里
  // 唯一「t 在最前」的 *Time：与 408/410/413 的 (4,3,2,0) 正好相反，是「混合」形态）。
  // 插值目标 = pos 组；切线恒 0（07 §5.1：全局 0x4d9dc4 恒零）。
  407: (vm, args, instr) => {
    const general = ensureGeneralState(vm);
    const t = readArg(vm, instr, 0, 'int');
    const mode = args[1] | 0; // mode 恒字面量，07 §1.1/§2
    const z = readArg(vm, instr, 4, 'float');
    const y = readArg(vm, instr, 3, 'float');
    const x = readArg(vm, instr, 2, 'float');
    beginInterp(general.interp.pos, [vm.pos.x, vm.pos.y, vm.pos.z], [x, y, z], t, mode);
  },
  // 07-general.md #408 rgbTime —— 求值顺序 (4,3,2,0)：b、g、r、t。
  408: (vm, args, instr) => {
    const general = ensureGeneralState(vm);
    const b = readArg(vm, instr, 4, 'int');
    const g = readArg(vm, instr, 3, 'int');
    const r = readArg(vm, instr, 2, 'int');
    const t = readArg(vm, instr, 0, 'int');
    const mode = args[1] | 0;
    beginInterp(
      general.interp.color1Rgb,
      [colorGetByte(vm.color, 'R'), colorGetByte(vm.color, 'G'), colorGetByte(vm.color, 'B')],
      [r, g, b],
      t,
      mode,
    );
  },
  // 07-general.md #409 alphaTime —— 求值顺序 (2,0)：alpha、t。
  409: (vm, args, instr) => {
    const general = ensureGeneralState(vm);
    const a = readArg(vm, instr, 2, 'int');
    const t = readArg(vm, instr, 0, 'int');
    const mode = args[1] | 0;
    beginInterp(general.interp.color1A, [colorGetByte(vm.color, 'A')], [a], t, mode);
  },
  // 07-general.md #410 rotateTime —— 求值顺序 (4,3,2,0)：z、y、x、t。切线恒 0（同 407）。
  410: (vm, args, instr) => {
    const general = ensureGeneralState(vm);
    const z = readArg(vm, instr, 4, 'float');
    const y = readArg(vm, instr, 3, 'float');
    const x = readArg(vm, instr, 2, 'float');
    const t = readArg(vm, instr, 0, 'int');
    const mode = args[1] | 0;
    beginInterp(general.interp.rotation, [vm.rotation.x, vm.rotation.y, vm.rotation.z], [x, y, z], t, mode);
  },
  // 07-general.md #411 rotateTime2D —— 求值顺序 (2,0)：angle、t。角度感知（最短路径），
  // 见 stepGeneric 的 kind==='angle' 分支与 07 §5.2。
  411: (vm, args, instr) => {
    const general = ensureGeneralState(vm);
    const angle = readArg(vm, instr, 2, 'float');
    const t = readArg(vm, instr, 0, 'int');
    const mode = args[1] | 0;
    beginInterp(general.interp.rotation2d, [vm.rotation.z], [angle], t, mode);
  },
  // 07-general.md #412 scaleTime —— 求值顺序 (3,2,0)：y、x、t。
  412: (vm, args, instr) => {
    const general = ensureGeneralState(vm);
    const y = readArg(vm, instr, 3, 'float');
    const x = readArg(vm, instr, 2, 'float');
    const t = readArg(vm, instr, 0, 'int');
    const mode = args[1] | 0;
    beginInterp(general.interp.scale, [vm.scale.x, vm.scale.y], [x, y], t, mode);
  },
  // 07-general.md #413 rgb2Time —— 求值顺序 (4,3,2,0)：b、g、r、t。
  413: (vm, args, instr) => {
    const general = ensureGeneralState(vm);
    const b = readArg(vm, instr, 4, 'int');
    const g = readArg(vm, instr, 3, 'int');
    const r = readArg(vm, instr, 2, 'int');
    const t = readArg(vm, instr, 0, 'int');
    const mode = args[1] | 0;
    beginInterp(
      general.interp.color2Rgb,
      [colorGetByte(general.color2, 'R'), colorGetByte(general.color2, 'G'), colorGetByte(general.color2, 'B')],
      [r, g, b],
      t,
      mode,
    );
  },
  // 07-general.md #414 alpha2Time —— 求值顺序 (2,0)：alpha、t。
  414: (vm, args, instr) => {
    const general = ensureGeneralState(vm);
    const a = readArg(vm, instr, 2, 'int');
    const t = readArg(vm, instr, 0, 'int');
    const mode = args[1] | 0;
    beginInterp(general.interp.color2A, [colorGetByte(general.color2, 'A')], [a], t, mode);
  },
  // 07-general.md #415 —— ★ 求值顺序 (2,1,0)，与 400 同款（07 §2「$ 记号的更正」表）。
  415: (vm, args, instr) => {
    const general = ensureGeneralState(vm);
    const z = readArg(vm, instr, 2, 'float');
    const y = readArg(vm, instr, 1, 'float');
    const x = readArg(vm, instr, 0, 'float');
    general.angularVelocity.x = x;
    general.angularVelocity.y = y;
    general.angularVelocity.z = z;
    vm.flagsHi = (vm.flagsHi | 0x1000000) >>> 0;
  },
  // 07-general.md #416 —— 求值顺序 (1,0)：先 y 后 x。
  416: (vm, args, instr) => {
    const general = ensureGeneralState(vm);
    const y = readArg(vm, instr, 1, 'float');
    const x = readArg(vm, instr, 0, 'float');
    general.scaleGrowth.x = x;
    general.scaleGrowth.y = y;
    vm.flagsHi = (vm.flagsHi | 0x1000000) >>> 0;
  },
  // 07-general.md #417 alphaTimeLinear —— ★ 「$ 记号的更正」之一：arg0（alpha）是裸
  // movzx byte，从不查 mask；arg1（t）才走 get_int_var（07 §2 表求值顺序 `(1)`：只有
  // arg1 会经过 resolveArg）。mode 硬编码 0（线性）。alpha 字节截断（07 §2 表原话）。
  417: (vm, args, instr) => {
    const general = ensureGeneralState(vm);
    const alpha = literalByte(instr, 0);
    const t = readArg(vm, instr, 1, 'int');
    beginInterp(general.interp.color1A, [colorGetByte(vm.color, 'A')], [alpha], t, 0);
  },
  // 07-general.md #418 —— 重算 sprite UV 四角（07 §6.3：`recompute_uv_quad` 0x468c00
  // 按 flags_lo bit25-29 的渲染模式算四角，再各过一遍 0x45f940 归一化写 +0x3a8..0x3c4）。
  // ★ 本骨架不建模：这是渲染几何（依赖 sprite 尺寸、渲染模式分支的具体数学，07 §11 自己
  // 也把「418 的高层效果」列为「归渲染文档」），不是 VM 状态机能独立算出来的东西——同
  // core.js 对 camera-*/lookat-* 的态度（04 §6.1）：宁可显式抛错，也不要悄悄返回一个
  // 看似合理、实际与真机无关的角点坐标。
  418: () => {
    throw new Error(
      'group-general 418: 重算 sprite UV 四角未建模——07-general.md §6.3：这是渲染几何' +
        '（recompute_uv_quad 0x468c00 依赖 sprite 尺寸与 flags_lo bit25-29 的渲染模式分支，' +
        '归渲染文档范围），不是 VM 状态机能独立算出来的东西。本骨架不提供隐式退化。',
    );
  },
  // 07-general.md #419 —— flags_hi bit13（0x2000，帧尾自动做 418 同一件事）。★「$ 记号
  // 的更正」表确认 419 的 arg0 确实走 get_int_var（对照组，不在 6 条例外之列）。
  419: (vm, args, instr) => {
    const v = readArg(vm, instr, 0, 'int');
    vm.flagsHi = setBits(vm.flagsHi, 0x2000, 13, v);
  },
  // 07-general.md #420 posBezier —— ★ 求值顺序 (1,2,3,7,0,6,5,4)：切线1(x,y,z)→切线2.x
  // →t→目标(z,y,x)。07 原文的这张序表只列了 8 个下标（0,1,2,3,4,5,6,7），漏掉了
  // arg8/arg9（切线2 的 y/z 分量）——这是文档本身的缺口，不是本文件漏抄（07 §9 的真实
  // 数据对账也只提到 `param_mask` 在 arg1/2/7/8 上出现过变量，没提 arg9）。本实现把
  // arg8/arg9 紧跟在 arg7 之后读（即切线2 的 x/y/z 三个分量连续读完，与切线1 的 x/y/z
  // 连续读的结构对称）——这是本文件在文档缺口上做的最小外推，不是一手结论，标 🟡，
  // 见任务报告；不影响本文件的求值顺序回归测试（那条测试只断言文档给出的 8 个下标）。
  // 语义（07 §6.4，✅）：终点=(x2,y2,z2)=arg4-6；初速切线=(x1,y1,z1)=arg1-3；
  // 末速切线=(x3,y3,z3)=arg7-9；mode 硬编码 8。
  420: (vm, args, instr) => {
    const general = ensureGeneralState(vm);
    const t1x = readArg(vm, instr, 1, 'float');
    const t1y = readArg(vm, instr, 2, 'float');
    const t1z = readArg(vm, instr, 3, 'float');
    const t2x = readArg(vm, instr, 7, 'float');
    const t2y = readArg(vm, instr, 8, 'float'); // 🟡 见上：文档序表缺这一项，外推位置
    const t2z = readArg(vm, instr, 9, 'float'); // 🟡 同上
    const t = readArg(vm, instr, 0, 'int');
    const targetZ = readArg(vm, instr, 6, 'float');
    const targetY = readArg(vm, instr, 5, 'float');
    const targetX = readArg(vm, instr, 4, 'float');
    beginInterp(
      general.interp.pos,
      [vm.pos.x, vm.pos.y, vm.pos.z],
      [targetX, targetY, targetZ],
      t,
      8,
      [t1x, t1y, t1z],
      [t2x, t2y, t2z],
    );
  },
  // 07-general.md #421 anchor —— ★「$ 记号的更正」+「一个 u32 不是两个 s16」（07
  // §6.1）：arg0 从不查 mask，h = 低 16 位、v = 高 16 位。写 flags_lo bit21-22（h）与
  // bit23-24（v）。
  421: (vm, args, instr) => {
    const h = literalWord(instr, 0);
    const v = (instr.args[0] >>> 16) & 0xffff;
    let flags = setBits(vm.flagsLo, 0x600000, 21, h);
    flags = setBits(flags, 0x1800000, 23, v);
    vm.flagsLo = flags;
  },
  // 07-general.md #422 posAdopt —— 机制 ✅ / 语义 🟡（07 §8）：`pos = entity_pos`，
  // 再把 `entity_pos` 清 0。无参数。
  422: (vm) => {
    const general = ensureGeneralState(vm);
    vm.pos.x = general.entityPos.x;
    vm.pos.y = general.entityPos.y;
    vm.pos.z = general.entityPos.z;
    general.entityPos.x = 0;
    general.entityPos.y = 0;
    general.entityPos.z = 0;
  },
  // 07-general.md #423 colorMode —— ★「$ 记号的更正」：arg0 裸字节，从不查 mask。
  // flags_lo bit17-18（2 位）。
  423: (vm, args, instr) => {
    const v = literalByte(instr, 0);
    vm.flagsLo = setBits(vm.flagsLo, 0x60000, 17, v);
  },
  // 07-general.md #424 rotateAuto —— ★「$ 记号的更正」：arg0 裸字节。flags_hi bit7（0x80）。
  424: (vm, args, instr) => {
    const v = literalByte(instr, 0);
    vm.flagsHi = setBits(vm.flagsHi, 0x80, 7, v);
  },
  // 07-general.md #425 uVel —— 单参 (0)。
  425: (vm, args, instr) => {
    const general = ensureGeneralState(vm);
    general.uvScrollVel.x = readArg(vm, instr, 0, 'float');
    vm.flagsHi = (vm.flagsHi | 0x1000000) >>> 0;
  },
  // 07-general.md #426 vVel —— 单参 (0)。
  426: (vm, args, instr) => {
    const general = ensureGeneralState(vm);
    general.uvScrollVel.y = readArg(vm, instr, 0, 'float');
    vm.flagsHi = (vm.flagsHi | 0x1000000) >>> 0;
  },
  // 07-general.md #427 uVelTime —— 求值顺序 (2,0)：vel、t。
  427: (vm, args, instr) => {
    const general = ensureGeneralState(vm);
    const vel = readArg(vm, instr, 2, 'float');
    const t = readArg(vm, instr, 0, 'int');
    const mode = args[1] | 0;
    beginInterp(general.interp.uvScrollVelX, [general.uvScrollVel.x], [vel], t, mode);
  },
  // 07-general.md #428 vVelTime —— 求值顺序 (2,0)：vel、t。
  428: (vm, args, instr) => {
    const general = ensureGeneralState(vm);
    const vel = readArg(vm, instr, 2, 'float');
    const t = readArg(vm, instr, 0, 'int');
    const mode = args[1] | 0;
    beginInterp(general.interp.uvScrollVelY, [general.uvScrollVel.y], [vel], t, mode);
  },
  // 07-general.md #429 uvScale —— 求值顺序 (0,1) 升序。
  429: (vm, args, instr) => {
    const general = ensureGeneralState(vm);
    general.uvScale.x = readArg(vm, instr, 0, 'float');
    general.uvScale.y = readArg(vm, instr, 1, 'float');
    vm.flagsLo = (vm.flagsLo | 0x10) >>> 0;
  },
  // 07-general.md #430 uvScaleTime —— 求值顺序 (3,2,0)：y、x、t。
  430: (vm, args, instr) => {
    const general = ensureGeneralState(vm);
    const y = readArg(vm, instr, 3, 'float');
    const x = readArg(vm, instr, 2, 'float');
    const t = readArg(vm, instr, 0, 'int');
    const mode = args[1] | 0;
    beginInterp(general.interp.uvScale, [general.uvScale.x, general.uvScale.y], [x, y], t, mode);
  },
  // 07-general.md #431 —— ★「$ 记号的更正」：arg0 裸字节。flags_hi bit8（0x100，语义 ❓，
  // 07 §8：499 次真实使用，无交叉测试点，维持未知）。
  431: (vm, args, instr) => {
    const v = literalByte(instr, 0);
    vm.flagsHi = setBits(vm.flagsHi, 0x100, 8, v);
  },
  // 07-general.md #432 slowdown-immune —— ★ 求值顺序 (0,0)：arg0 读两次（07 §2 表原话
  // 「右值 + 左值指针」）——如果 arg0 是一个每次读都会推进 PRNG 的变量（比如 10022
  // rand），这条指令本身就会消费两次 draw。字面上复刻这个「读两次」，不只是图省事把
  // resolveArg 调用两遍：这正是本组「求值次序」判据要求的东西——次序错了不报错，只会让
  // 随机序列悄悄跟游戏错位。flags_hi bit9（0x200，= 02 §3.6「强制 GAME_SPEED=1.0」）。
  432: (vm, args, instr) => {
    readArg(vm, instr, 0, 'int'); // 第一次读（右值）——按文档字面复刻，结果本身不使用
    const v = readArg(vm, instr, 0, 'int'); // 第二次读（“左值指针”那一次）——取这次的值
    vm.flagsHi = setBits(vm.flagsHi, 0x200, 9, v);
  },
  // 07-general.md #433 posTime2D —— ★ 求值顺序 (0,3,2)：t 先读，然后 radius(arg3)、
  // angle(arg2)。极坐标目标（07 §6.2，不是「2D 版 posTime」）：
  // target = (cos(angle)·radius, sin(angle)·radius, 0)，z 分量一路插值到 0。
  433: (vm, args, instr) => {
    const general = ensureGeneralState(vm);
    const t = readArg(vm, instr, 0, 'int');
    const mode = args[1] | 0;
    const radius = readArg(vm, instr, 3, 'float');
    const angle = readArg(vm, instr, 2, 'float');
    const targetX = Math.fround(Math.cos(angle) * radius);
    const targetY = Math.fround(Math.sin(angle) * radius);
    beginInterp(general.interp.pos, [vm.pos.x, vm.pos.y, vm.pos.z], [targetX, targetY, 0], t, mode);
  },
  // 07-general.md #434 scale2 —— 求值顺序 (0,1) 升序。
  434: (vm, args, instr) => {
    const general = ensureGeneralState(vm);
    general.scale2.x = readArg(vm, instr, 0, 'float');
    general.scale2.y = readArg(vm, instr, 1, 'float');
    vm.flagsLo = (vm.flagsLo | 8) >>> 0;
  },
  // 07-general.md #435 scale2Time —— 求值顺序 (3,2,0)：y、x、t。
  435: (vm, args, instr) => {
    const general = ensureGeneralState(vm);
    const y = readArg(vm, instr, 3, 'float');
    const x = readArg(vm, instr, 2, 'float');
    const t = readArg(vm, instr, 0, 'int');
    const mode = args[1] | 0;
    beginInterp(general.interp.scale2, [general.scale2.x, general.scale2.y], [x, y], t, mode);
  },
  // 07-general.md #436 anchorOffset —— 求值顺序 (0,1) 升序。没有 flag 位（07 §2 表）。
  436: (vm, args, instr) => {
    const general = ensureGeneralState(vm);
    general.anchorOffset.x = readArg(vm, instr, 0, 'float');
    general.anchorOffset.y = readArg(vm, instr, 1, 'float');
  },
  // 07-general.md #437 rotationSystem —— 单参 (0)。★「$ 记号的更正」的对照组：确实走
  // get_int_var。flags_hi bit2-4（3 位）。
  437: (vm, args, instr) => {
    const v = readArg(vm, instr, 0, 'int');
    vm.flagsHi = setBits(vm.flagsHi, 0x1c, 2, v);
  },
  // 07-general.md #438 originMode —— ★「$ 记号的更正」：arg0 裸字节。flags_hi bit18-19（2 位）。
  438: (vm, args, instr) => {
    const v = literalByte(instr, 0);
    vm.flagsHi = setBits(vm.flagsHi, 0xc0000, 18, v);
  },
};
