// ANM v8 VM —— Drawing 组（opcode 600–613，14 条）。
//
// 依据：engine/anm/th16/09-drawing.md（下称「09」）。每条 handler 上方标注它对应 09 的哪一节
// 与哪个 case 地址；地址一律 th16 v1.00a。
//
// ── 这个组只做一件事：置状态 ────────────────────────────────────────────────────
// 09 §0/§8 第一条坑：**Drawing 组 14 条自身不发 draw call**。每条只做两件事：
//   ① 把 `flags_lo` bit 25–29（渲染模式）写成一个特定值；
//   ② 写一个 vm 字段（`sprite_size` / `I0` / 顶点缓冲）。
// 真正的绘制分两步、都在帧尾之后：`th16:0x4632f0` 按模式填顶点，`draw_vm th16:0x468490`
// 按模式分发到各 mode 函数发 `DrawPrimitive`。**那两步属于层 B（渲染后端），不在本文件**——
// 本组与渲染后端的分界线就是这里：本组写状态，后端读状态。
//
// ── 只改 5 位，其余 flags_lo 位一位不动 ──────────────────────────────────────────
// 09 §1.1 给了可机器复核的判据：每条 case 体开头恒是
//   `mov eax,[ebx+0x530]` / `and eax,<mask>` / `or eax,<val>` / `mov [ebx+0x530],eax`，
// 且 `and` 清掉的位与该 mode 五位二进制里「应为 0」的位精确互补、`or` 置的位与「应为 1」的
// 位精确一致，两者并集恒为 `0x3e000000`。所以净效果 = 只写模式字段。**本组不碰 blend mode
// （bit 5–8，`303` 的地盘）、也不碰采样地址模式（bit 30–31，`312` 的地盘）**——09 §8 专门
// 列了这条坑：别把「设 render mode」和「设 blend」混成一个动作。
//
// ── 求值次序 ────────────────────────────────────────────────────────────────────
// 09 §2.1（承 03-math-jumps.md §9.1 的判据）：本组每条都是**下标升序**读参数，且**左值解析
// （写字段）恒在右值读取之后**。本文件照抄这个次序——先把该读的参数全读出来，再写字段。
// 对 `604/605/611` 这不是可有可无的讲究：它们的末位参数写的是 `I0`（变量号 10000），万一
// 某个参数自己就是 `$I0`，「先读后写」和「边读边写」会给出不同结果。
//
// ── 本组不建模的东西 ────────────────────────────────────────────────────────────
// 顶点内容：本组只把缓冲「分配」出来（09 §2.2 的 `th16:0x4064e0`），**不填**——填顶点是帧尾
// `th16:0x4632f0` 的事，需要 09 §3.1 那张字段读表 + 尚标 🟡 的几何公式，归层 B。
import { resolveArg, isVarBit } from './core.js';

// ── flags_lo bit 25–29 = 渲染模式（02-vm-core.md §3.6） ─────────────────────────
// 与 group-settings.js 的 `302 renderMode` 写的是同一个字段、同一套位运算。这里刻意**不**从
// group-settings.js import：既有分层是「组依赖骨架、骨架不依赖组」，组与组之间同样不该互相
// 依赖（I7 为了复用 ensureGeneralState 破一次例是因为那是**同一份物理状态**，而位运算只是
// 三行纯函数，复制比耦合便宜）。两处若有分歧，以 02 §3.6 的字段定义为准。
const RENDER_MODE_MASK = 0x3e000000;
const RENDER_MODE_SHIFT = 25;

function setRenderMode(vm, mode) {
  const cleared = (vm.flagsLo & ~RENDER_MODE_MASK) >>> 0;
  const bits = ((mode << RENDER_MODE_SHIFT) & RENDER_MODE_MASK) >>> 0;
  vm.flagsLo = (cleared | bits) >>> 0;
}

// ── 参数读取（转发给骨架的 resolveArg，param_mask 位由 isVarBit 判） ───────────
// 09 §5：`S nmax` 走 `get_int_var th16:0x45f610`；`f w,h` / `f r` 走
// `get_float_var th16:0x45f2d0`。本组没有「读变量可能推进 PRNG」的随机参数，所以读多读少
// 不会有副作用差异——但次序仍按 §2.1 照抄，理由见文件头。
function readIntArg(vm, instr, index) {
  return resolveArg(vm, instr.args[index], isVarBit(instr.paramMask, index), 'int');
}
function readFloatArg(vm, instr, index) {
  return resolveArg(vm, instr.args[index], isVarBit(instr.paramMask, index), 'float');
}

// ── 09 §2.2 顶点缓冲分配：`th16:0x4064e0`（__thiscall ECX=vm，栈参 1 个字节数） ──
//   vm[+0x5bc] = size;  vm[+0x5b8] = operator_new(size);
// 逐条照抄，包括**先写大小再写指针**这个次序，以及**不释放旧缓冲**——`0x4064e0` 的函数体里
// 没有 free/delete，重复调用就是直接覆盖 `+0x5b8`。真机上这是泄漏；本骨架跑在 GC 语言里，
// 旧 ArrayBuffer 失去引用后自动回收，行为等价而不需要额外记账。
//
// stride 的来历（09 §2.2）：56 = 28×2、48 = 24×2 —— textureCircle 的顶点 stride 是 28 字节
// （FVF `0x144` = XYZRHW|DIFFUSE|TEX1），textureArc3D 是 20 字节（FVF `0x102` = XYZ|TEX1）；
// `nmax` 是**点数上限**，每个点摊 2 个顶点，所以倍数是 2×stride 而不是 stride 本身。
// （09 §4.4 的 stride 表与这里的 56/48 口径一致，24 与 20 的差是对齐，文档已标明。）
function allocVertexBuffer(vm, nmax, bytesPerPoint, opcode) {
  if (nmax < 0) {
    // 真机是 `operator new((size_t)(nmax * 56))`：负数按 size_t 解释成天文数字 ⇒ bad_alloc ⇒
    // 崩。骨架不复刻崩溃，但也拒绝静默把它当 0——那会让「脚本把 I0 算成了负数」这个真实
    // bug 在播放器里消失得无影无踪。与 group-math 的除零、group-settings 301 的 b==0 同策。
    throw new Error(
      `group-drawing ${opcode}: nmax=${nmax} < 0 —— 09-drawing.md §2.2：真机 ` +
        '`th16:0x4064e0` 直接把它交给 `operator new`（按 size_t 解释成天文数字，bad_alloc）。' +
        '本实现抛异常，不静默当 0 处理。',
    );
  }
  const size = nmax * bytesPerPoint;
  vm.vertexBufferSize = size; // `+0x5bc`，先写（照抄 0x4064e0 的次序）
  vm.vertexBuffer = new ArrayBuffer(size); // `+0x5b8`
}

// ── 09 §2.1 的三个共用落点，照它的复用关系抽成三个 helper ────────────────────────
// 文档把「谁跳到谁的尾巴」穷举清楚了（扫全部 14 个 case 的最后一条 jmp 目标），这里的三个
// helper 就是那三个落点，复用关系与 §2.1 的表一一对应。

// 落点 A（`th16:0x462618`，603 的尾巴）：`arg0(f)→+0x70`、`arg1(f)→+0x74`。
// 用户：603（自己）、606、607、608、612、613。
function writeSpriteSize(vm, instr) {
  const w = readFloatArg(vm, instr, 0);
  const h = readFloatArg(vm, instr, 1);
  vm.spriteSize.x = Math.fround(w);
  vm.spriteSize.y = Math.fround(h);
}

// 落点 B（`th16:0x4626fb`，604 的尾巴）：`arg0(f)→+0x70`、`arg1(int)→+0x4a0`（I0）。
// 用户：604（自己）、605。
function writeRadiusAndPointCount(vm, instr) {
  const radius = readFloatArg(vm, instr, 0);
  const n = readIntArg(vm, instr, 1);
  vm.spriteSize.x = Math.fround(radius);
  vm.i[0] = n | 0; // `+0x4a0` = I0；09 §3 要点 3：绘制时按 `2*I0` 取顶点数
}

// 落点 C（`th16:0x46247c` / `th16:0x4624f5`）：读 int `nmax` → ×56 / ×48 → 分配。
// 56 的用户：600（自己）、601、602；48 的用户：609（自己）、610。
function allocFromNmax(vm, instr, bytesPerPoint, opcode) {
  const nmax = readIntArg(vm, instr, 0);
  allocVertexBuffer(vm, nmax, bytesPerPoint, opcode);
}

/**
 * Drawing 组 handler 表。09 §2 的 14 行逐条落地。
 *
 * 「渲染模式」一列（写进 flags_lo bit 25–29 的值）**不是按 opcode 单调递增的**——
 * 611 是 19、612 是 27、613 是 26。这不是笔误：09 §2 的表就是这样，§3 的 `draw_vm`
 * 分发表也据此把 611 归进 poly 族（17/18/19）、612/613 归进 rect 族（16/20/21/22/26/27）。
 * 测试里有一条专门守这个「不单调」的映射，防止有人「顺手排整齐」。
 */
export const drawingGroup = {
  // ── 600 textureCircle：`$ nmax` → 模式 9，分配 56×nmax ── 09 §2 / `th16:0x46246c` ──
  600: (vm, args, instr) => {
    setRenderMode(vm, 9);
    allocFromNmax(vm, instr, 56, 600);
  },

  // ── 601 textureArcEven：`$ nmax` → 模式 13 ── 09 §2 / `th16:0x4624c1` ──────────────
  // 弧角「居中于 0」。绘制层与 600/602 完全同函数（`th16:0x468683`），差别在帧尾几何
  // `th16:0x4632f0` 的分支（09 §3 要点 1）——本骨架不含几何，所以这三条在本文件里
  // 只有模式号不同，这是**对的**，不是漏实现。
  // ⚠️ ZUN 自家 54 个 .anm 一次没用过（09 §6），语义 ✅ 但画面无 ground truth。
  601: (vm, args, instr) => {
    setRenderMode(vm, 13);
    allocFromNmax(vm, instr, 56, 601);
  },

  // ── 602 textureArc：`$ nmax` → 模式 14 ── 09 §2 / `th16:0x4624d3` ─────────────────
  // 弧角「从 0 起」。同上，与 600/601 共用绘制函数。
  602: (vm, args, instr) => {
    setRenderMode(vm, 14);
    allocFromNmax(vm, instr, 56, 602);
  },

  // ── 603 drawRect：`f w, f h` → 模式 16，写 sprite_size ── 09 §2 / `th16:0x462608` ──
  // 09 §8 第二条坑：`sprite_size` 是**基础尺寸**，绘制时还要 `×scale`
  // （`th16:0x468722` 的 `mulss xmm0,[esi+0x50]`）。`drawRect(1,1)` + `scale=100` 才是 100px。
  603: (vm, args, instr) => {
    setRenderMode(vm, 16);
    writeSpriteSize(vm, instr);
  },

  // ── 604 drawPoly：`f r, $ n` → 模式 17，写 sprite_size.x + I0 ── 09 §2 / `0x4626eb` ─
  // 真实数据里出现 42 次，是本组最常用的一条（09 §6）：radius 46/46 全字面量、
  // n ∈ {3,12,32,36,40,50}（正多边形边数）。
  604: (vm, args, instr) => {
    setRenderMode(vm, 17);
    writeRadiusAndPointCount(vm, instr);
  },

  // ── 605 drawPolyBorder：`f r, $ n` → 模式 18 ── 09 §2 / `th16:0x462766` ────────────
  605: (vm, args, instr) => {
    setRenderMode(vm, 18);
    writeRadiusAndPointCount(vm, instr);
  },

  // ── 606 drawRectGrad：`f w, f h` → 模式 20 ── 09 §2 / `th16:0x462685` ──────────────
  606: (vm, args, instr) => {
    setRenderMode(vm, 20);
    writeSpriteSize(vm, instr);
  },

  // ── 607 drawRectShadow：`f w, f h` → 模式 21 ── 09 §2 / `th16:0x462697` ────────────
  // ⚠️ ZUN 自家一次没用过（09 §6）。
  607: (vm, args, instr) => {
    setRenderMode(vm, 21);
    writeSpriteSize(vm, instr);
  },

  // ── 608 drawRectShadowGrad：`f w, f h` → 模式 22 ── 09 §2 / `th16:0x4626ac` ────────
  608: (vm, args, instr) => {
    setRenderMode(vm, 22);
    writeSpriteSize(vm, instr);
  },

  // ── 609 textureCylinder3D：`$ nmax` → 模式 24，分配 48×nmax ── 09 §2 / `0x4624e5` ──
  // 几何参数（`F0–F3` = 总角/高/半径/中心角、`I0`/`I1` = 点数/重复次数）由帧尾
  // `th16:0x4632f0` 读（09 §3.1），**不是本条指令的参数**——本条只给缓冲。
  // ⚠️ ZUN 自家一次没用过；精确 3D 几何公式在 09 §9 仍标 🟡。
  609: (vm, args, instr) => {
    setRenderMode(vm, 24);
    allocFromNmax(vm, instr, 48, 609);
  },

  // ── 610 textureRing3D：`$ nmax` → 模式 25 ── 09 §2 / `th16:0x462552` ──────────────
  // 与 609 共用绘制函数 `th16:0x467d00`（09 §3 要点 1）。⚠️ 同样无 ground truth。
  610: (vm, args, instr) => {
    setRenderMode(vm, 25);
    allocFromNmax(vm, instr, 48, 610);
  },

  // ── 611 drawRing：`f r, f t, $ n` → 模式 19 ── 09 §2 / `th16:0x462778` ─────────────
  // 09 §2.1：611 三条参数**全自写**，不复用任何别的尾巴——所以这里也不走上面的 helper。
  // 落点：r→`+0x70`、t（thickness）→`+0x74`、n→`+0x4a0`（I0）。
  // ⚠️ ZUN 自家一次没用过。
  611: (vm, args, instr) => {
    setRenderMode(vm, 19);
    const radius = readFloatArg(vm, instr, 0);
    const thickness = readFloatArg(vm, instr, 1);
    const n = readIntArg(vm, instr, 2);
    vm.spriteSize.x = Math.fround(radius);
    vm.spriteSize.y = Math.fround(thickness);
    vm.i[0] = n | 0;
  },

  // ── 612 drawRectBorder：`f w, f h` → 模式 27 ── 09 §2 / `th16:0x4626d6` ────────────
  612: (vm, args, instr) => {
    setRenderMode(vm, 27);
    writeSpriteSize(vm, instr);
  },

  // ── 613 drawLine：`f len, f _` → 模式 26 ── 09 §2 / `th16:0x4626c1` ────────────────
  // ★ 09 §5(3) / §7 裁决 3：thpages 写 `args:['len','unused']`，但「unused」是**渲染层**的
  // 说法——mode 26 的绘制 handler（`th16:0x46882a`→`th16:0x469330`）不读 `+0x74`（线恒 1px
  // 厚、只取 x 长）。**VM 层照写不误**（与 603 共用尾巴 `th16:0x462618`）。所以这里必须
  // 写 `spriteSize.y`，把它「优化掉」就与真机的 VM 状态对不上了。
  // ⚠️ ZUN 自家一次没用过。
  613: (vm, args, instr) => {
    setRenderMode(vm, 26);
    writeSpriteSize(vm, instr);
  },
};
