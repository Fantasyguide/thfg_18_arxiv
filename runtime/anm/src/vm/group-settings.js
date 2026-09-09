// ANM v8 VM ——Settings 组（opcode 300–317，18 条）。
//
// 依据：engine/anm/th16/06-settings.md（下称「06」）§2 的 18 条指令表 + §2.1/§2.3 的两处
// 「审阅更正」（300 的负数分支是热路径、308/309 还置 bit3 缩放脏位）。每条 handler 上方
// 注释指回具体 opcode 行（双向互指纪律，见 runtime/README.md）。本文件不重新论证任何语义
// 结论——那是 engine/ 层的事，这里只做「把结论翻成 JS」的实现决策。
//
// 消费 core.js 导出的 resolveArg / isVarBit（本组没有跳转，不需要 PC_HANDLED）。
//
// ── ★ 层次归属：flags_lo/flags_hi 不在 core.js 的 vm 契约里，本组怎么处理 ──────────────
// 06 §0/§1.1：本组 18 条里有 13 条只改 `flags_lo`（`+0x530`）/ `flags_hi`（`+0x534`）
// 这两个 32 位字段的若干位；核心问题是 core.js 的 `createVm` 目前没有这两个字段（见
// core.js 文件头「本文件明确不建模的东西」第三条：flags_lo/flags_hi 与冻结闸门被 I2 明确
// 划到 400+/General 组或渲染器层，没有进 vm 契约）。brief 给了两个选项：
//   (a) 在 group-settings.js 之外新开一个共享的 vm 扩展点（比如一个 `extendVmFlags(vm)`
//       之类的初始化函数，调用方在 createVm 之后自己再调一次）——这会制造一个新的、
//       不受 core.js 管的「第二次初始化」步骤，调用方必须记得按顺序调用两次，且这个
//       扩展点本身该放哪个文件、算不算「新的骨架」都没有定论，本身就是要 core.js owner
//       裁决的架构问题。
//   (b) handler 直接在 `vm` 上惰性建 `flagsLo`/`flagsHi` 字段（首次访问才赋默认值），
//       注明「骨架收口时应把它移进 createVm」——不新开文件/新接口，代价是「字段何时
//       出现在 vm 上」这件事变得不那么显式（要读这个文件的注释才知道）。
// 采用 **(b)**：理由同 brief 给出的倾向——core.js 这一轮明确不让动（brief 硬约束），
// 加一个新的公共扩展点等于在没有 core.js owner 参与的情况下往骨架的公共契约里加东西，
// 风险比「惰性建字段 + 显眼 TODO」更大；且 (b) 与本仓库已有先例一致（04-variables.md
// 的 IV 任务把 `i4`/`i5`/`randParamOne` 等字段加进了 createVm 本身，说明「哪些字段该在
// 契约里」历来是由持有 core.js 的任务决定的，本组不该越权替它决定）。
// **TODO(骨架收口)**：等有人重新审视 core.js 的 vm 契约时，应该把 `flagsLo`/`flagsHi`
// 两个字段（含下面 `getFlagsLo`/`getFlagsHi` 里写的默认值）搬进 `createVm` 本体，删掉这里
// 的惰性初始化——本组这套 getter/setter 只是「core.js 没给字段之前的过渡」，不是最终形态。
//
import { resolveArg, isVarBit } from './core.js';

// 默认值：06 §3.2 证实 `AnmVm::wipe`（`th16:0x409128`/`th16:0x409220`）把 `flags_lo` 低
// 16 位默认置成 7（bit0=可见、bit1=316/317 的绘制闸门、bit2 未在本文档范围内追），所以
// `flagsLo` 的惰性默认值取 0x7，不是 0——316/317 两条测试都依赖这个默认值（317 清位要
// 从「默认已置位」清成「清位」才有区分力，见 vm-settings.test.js）。`flags_hi` 没有一手
// 证据支持任何非零默认值，惰性默认值取 0。
function getFlagsLo(vm) {
  if (vm.flagsLo === undefined) {
    vm.flagsLo = 0x7; // 06-settings.md §3.2：AnmVm::wipe 默认低 16 位 = 7（bit0/1/2）
  }
  return vm.flagsLo;
}
function setFlagsLo(vm, value) {
  vm.flagsLo = value >>> 0;
}
function getFlagsHi(vm) {
  if (vm.flagsHi === undefined) {
    vm.flagsHi = 0; // 06 没有给出 flags_hi 的非零默认值证据
  }
  return vm.flagsHi;
}
function setFlagsHi(vm, value) {
  vm.flagsHi = value >>> 0;
}

// 「按值写位」惯用法（06 §1.1：`dst ^= (src^dst)&mask`，等价于「把 mask 位设成 value 的
// 同位值」，不是 toggle）。`mask`/`shift` 描述位区间，`value` 是原始整型（可能带无关高位，
// 由 mask 天然截断，不需要调用方先手动 `& (mask>>>shift)`）。
function setBits(current, mask, shift, value) {
  const m = mask >>> 0;
  return ((current & ~m) | ((value << shift) & m)) >>> 0;
}

// 两族「literal-only」参数的读法（06 §4：这几条即使 paramMask 置位也不查变量表——
// handler 压根不读 paramMask）：
//   - 32 位字面量族（302/303/305/306/310/311）：直接 `args[i]|0`。
//   - 低 8 位字面量族（304/307/315）：`movzx byte`，只取低 8 位。
function literalInt(args, i) {
  return args[i] | 0;
}
function literalByte(args, i) {
  return (args[i] | 0) & 0xff;
}

// 「可为变量」族（300/301/312/313/314）的读法：转发给 core.js 的 resolveArg（不重新
// 实现变量表）。06 §4：真实数据里这 5 条也恒为字面量，但机器码层支持变量，播放器要按
// `get_int_var` 语义实现。
function readArg(vm, instr, index) {
  return resolveArg(vm, instr.args[index], isVarBit(instr.paramMask, index), 'int');
}

// ── 301 spriteRand 的 PRNG（Stream B，`0x4a6d80`）─────────────────────────────────
// 06 §2/§2.3：301 走 Stream B（表现流）——与 group-math.js 122/123/131、core.js 内部
// 10010–10012/10022 的读取，用的是**同一条物理流**，约定成同一个钩子名 `vm.prng`
// （group-math.js 文件头「实现决策②」已经定下这个约定，本文件复用，不新开名字，否则
// 同一条流会被两个不同的钩子名各自要求注入，调用方稍不注意就会喂出两条不同步的序列）。
// `vm.prng` **必填**：没注入直接抛，不静默退化——同 group-math.js/core.js 已立住的哲学。
// 与 core.js 内部 `requirePrngRaw32`、group-math.js 的 `prngRaw32` 重复几行是刻意的
// （两个文件都有同款「复制优于新增公共依赖」的先例，见各自文件头）。
function prngRaw32StreamB(vm, opcode) {
  if (typeof vm.prng !== 'function') {
    throw new Error(
      `group-settings: opcode ${opcode} spriteRand 需要 PRNG（06-settings.md §2/§2.3：` +
        'Stream B，`0x4a6d80`），但 vm.prng 没有注入。本组复用 group-math.js 122/123/131 ' +
        '与 core.js 变量表已经约定的 vm.prng 钩子（同一条物理流），不提供隐式退化。',
    );
  }
  return vm.prng(vm) >>> 0;
}

export const settingsGroup = {
  // ── 300 sprite：$S id ── 06-settings.md #300 ───────────────────────────────────
  // 06 §2.1：三件事，且顺序固定——① flags_lo |= 1（可见）；② 读 id（可为变量）；
  // ③ id<0 走「系统贴图」分支（★ 2026-09-09 审阅更正：这是热路径，4683 条真实 300 里
  // 80 条 arg0==-1，不是理论分支）。本骨架没有 AnmManager/AsciiManager/ascii.anm 可换，
  // 拒绝把「系统贴图」静默处理成 0 或忽略——那会让画面在真机会显式换成 ascii.anm sprite
  // 0x102 的地方悄悄变成别的东西，没有任何信号。id>=0 时把 id 记进 `vm.spriteId`（本骨架
  // 没有 AnmLoaded/贴图表可以真的「换贴图」，只记录「VM 认为自己当前选中的 sprite id」这
  // 一层状态）+ `vm.spriteSelectTime` 镜像 `+0x494`（06 §2.1 结论1 的「记录上次 texSelect
  // 时间」）。`on_sprite_set` 回调（`+0x5dc`）★ 不建模——06 §2.1/§8 自己也把回调内部语义
  // 列为「归 ECL/渲染文档」🟡，本骨架没有 `+0x5dc` 这个字段、也没有回调表可调，直接跳过
  // 这一步（真实数据里 4683 条 300 全部字面量，`+0x5dc` 由弹/激光侧另外写，不是本组自己
  // 的状态，见 06 §2.1 证据）。
  300: (vm, args, instr) => {
    setFlagsLo(vm, getFlagsLo(vm) | 0x1);
    const id = readArg(vm, instr, 0);
    if (id < 0) {
      throw new Error(
        `group-settings 300/sprite: id=${id} < 0 —— 06-settings.md §2.1（★ 已更正为热路径，` +
          '非理论分支）：真机换成 AsciiManager（`0x4a6d98`）`+0x19240` 归档里的 sprite ' +
          '`0x102`（「系统贴图」）。本骨架没有 AsciiManager/ascii.anm 可换，拒绝静默当 0 或' +
          '忽略处理——那会让一个真机会显式换图的分支在播放器里悄悄消失。',
      );
    }
    vm.spriteId = id;
    vm.spriteSelectTime = vm.time; // 06 §2.1：`vm[+0x494] = vm.timer.current`
  },

  // ── 301 spriteRand：$S a, $S b ── 06-settings.md #301 ──────────────────────────
  // 06 §2.3：`id = a + (raw % (u32)b)`；求值顺序 arg0→arg1（升序），且**两个参数都读完
  // 之后**才抽随机（若 a/b 是会推进 PRNG 的变量号，序列位置由此确定——本骨架的
  // Stream B 变量号 10010–10012/10022 正是这种情况，顺序错了会让随机序列跟游戏错位）。
  // `b` 按 u32 解释、**没有零守卫**——`b==0` 真机是裸 `div` 直接 `#DE` 崩，本组同
  // group-math.js「实现决策①」的除零哲学：抛异常，不返回哨兵。
  // flags_lo |= 1（可见，与 300 同）；id 记进 `vm.spriteId`（06 没有给出 301 是否也调
  // `set_sprite`/记 `+0x494` 的一手结论，只写了「flags_lo|=1」——本实现按「spriteRand
  // 也是选中一个 sprite」的最小合理假设记 spriteId，不记 `spriteSelectTime`，因为后者
  // 06 §2.1 只对 300 一手证过；这条假设已经在报告的「疑问」一节里请求复核）。
  301: (vm, args, instr) => {
    const a = readArg(vm, instr, 0);
    const bRaw = readArg(vm, instr, 1);
    const b = bRaw >>> 0; // 06 §2.3：b 按 u32 解释
    if (b === 0) {
      throw new Error(
        'group-settings 301/spriteRand: b==0 —— 06-settings.md §2.3：真机是裸 `div`，' +
          '没有零守卫，会触发 x86 #DE 崩溃（与 122 rand 不同——122 有零守卫，301 没有，' +
          '文档原文曾把这条边界写反，§2.3 已更正）。本实现抛异常，不返回哨兵。',
      );
    }
    const raw = prngRaw32StreamB(vm, 301);
    const id = (a + (raw % b)) | 0;
    setFlagsLo(vm, getFlagsLo(vm) | 0x1);
    vm.spriteId = id;
  },

  // ── 302 renderMode：S mode（32 位字面量，不可为变量）── 06-settings.md #302 ────────
  // flags_lo bit25–29 = mode & 0x1f。mode==10（掩码后）触发 `call 0x469e20` 写
  // `on_tick` 回调下标 4（06 §3.6）——`on_tick`/`on_wait` 回调是 core.js 文件头明确列的
  // 「不建模」清单（宿主 AnmManager 的回调表，骨架没有这个概念），本组不重新引入，遇到
  // 就显式抛（不静默丢弃这次写）。真实数据里 mode 10 出现 0 次（06 §5），所以这条分支
  // 目前没有 ground truth 可对，但仍按「宁可抛不要静默」处理。
  302: (vm, args) => {
    const mode = literalInt(args, 0);
    setFlagsLo(vm, setBits(getFlagsLo(vm), 0x3e000000, 25, mode));
    if ((mode & 0x1f) === 10) {
      throw new Error(
        'group-settings 302/renderMode: mode 10 —— 06-settings.md §3.6：真机触发 ' +
          '`call 0x469e20` 写 on_tick 回调表下标 4，这是 core.js 文件头明确列的「不建模」' +
          '范围（宿主 AnmManager 的回调表，骨架没有这个概念）。flags_lo 的 5 位模式字段已经' +
          '按正常路径写入，只有回调注册这一步被拒绝静默跳过。',
      );
    }
  },

  // ── 303 blendMode：S mode（32 位字面量）── 06-settings.md #303 ─────────────────────
  // flags_lo bit5–8 = mode & 0xf。
  303: (vm, args) => {
    const mode = literalInt(args, 0);
    setFlagsLo(vm, setBits(getFlagsLo(vm), 0x1e0, 5, mode));
  },

  // ── 304 layer：b n（低 8 位字面量）── 06-settings.md #304 ──────────────────────────
  // 06 §2.2：`set_layer` 一次写四处——`+0x18`（layer 字段，本骨架落 `vm.layer`）+
  // flags_hi 的三段派生位（层区间 3–19→0x40000、20–23→0x80000、其余清 0xc0000；
  // 20–31 或 36–42→额外置 0x100000，这条没有对应的「否则清位」分支——逐字照抄 06 §2.2
  // 给出的 C 伪代码结构，第二个 if 没有 else）。
  304: (vm, args) => {
    const layer = literalByte(args, 0);
    vm.layer = layer; // 镜像 `+0x18`

    let hi = getFlagsHi(vm);
    if (layer >= 3 && layer <= 19) {
      hi = ((hi & ~0x80000) | 0x40000) >>> 0;
    } else if (layer >= 20 && layer <= 23) {
      hi = ((hi & ~0x40000) | 0x80000) >>> 0;
    } else {
      hi = (hi & ~0xc0000) >>> 0;
    }
    if ((layer >= 20 && layer <= 31) || (layer >= 36 && layer <= 42)) {
      hi = ((hi & ~0x600000) | 0x100000) >>> 0;
    }
    setFlagsHi(vm, hi);
  },

  // ── 305 noZBuffer：S disable（32 位字面量）── 06-settings.md #305 ───────────────────
  // flags_lo bit13 = disable & 1。⚠️ 06 §3.4 点名：别与 flags_hi bit13（`0x2000`，
  // 「帧尾重算 UV 四角」）搞混——同名位、不同字。本组用独立的 `flagsLo`/`flagsHi` 字段，
  // 天然不会混。
  305: (vm, args) => {
    const disable = literalInt(args, 0);
    setFlagsLo(vm, setBits(getFlagsLo(vm), 0x2000, 13, disable));
  },

  // ── 306 v8-306：S enable（32 位字面量）── 06-settings.md #306 ──────────────────────
  // flags_lo bit15（`0x8000`）= enable & 1。06 §3.1（一手到底，社区 wip:2 收敛）：
  // 读者是帧尾 `entity_pos += 全局滚动`，帧尾几何不在本骨架范围（core.js 文件头「不建模」
  // 清单），本组只负责置位，不复刻读者。
  306: (vm, args) => {
    const enable = literalInt(args, 0);
    setFlagsLo(vm, setBits(getFlagsLo(vm), 0x8000, 15, enable));
  },

  // ── 307 v8-randMode：b mode（低 8 位字面量）── 06-settings.md #307 ─────────────────
  // flags_hi bit10（`0x400`）= mode & 1。06 §3.3（一手穷举 + negative set）：TH16 全 exe
  // 无任何读者，是死位——PRNG 流选择是硬编码的（03-math-jumps.md §7.4）。本组仍然照写
  // 这个位（置位事实成立，✅），不因为「没人读」就跳过写入——保留「死位」这个可观察事实，
  // 而不是替引擎决定「反正没用就不用实现」。
  307: (vm, args) => {
    const mode = literalByte(args, 0);
    setFlagsHi(vm, setBits(getFlagsHi(vm), 0x400, 10, mode));
  },

  // ── 308 flipX：— ── 06-settings.md #308 ────────────────────────────────────────
  // ★ 2026-09-09 审阅更正（06 §2.3）：不是内存 `xor`，是寄存器上两步——
  // `flags_lo ^= 0x800`（bit11 toggle）**且 `flags_lo |= 8`**（bit3「缩放脏位」，
  // 02-vm-core.md 已 ✅：翻转 scale 后必须让下游重算变换）。原文档曾漏记 bit3 这一步，
  // 是本组测试里明确要求覆盖到的一条（brief 点名「最容易漏」）。
  308: (vm) => {
    vm.scale.x = -vm.scale.x;
    let lo = getFlagsLo(vm);
    lo = (lo ^ 0x800) >>> 0; // toggle bit11
    lo = (lo | 0x8) >>> 0; // 置位 bit3（缩放脏位，★ 更正后新增的一步）
    setFlagsLo(vm, lo);
  },

  // ── 309 flipY：— ── 06-settings.md #309 ────────────────────────────────────────
  // 与 308 同款更正，toggle 的是 bit12（`0x1000`），同样加 bit3。
  309: (vm) => {
    vm.scale.y = -vm.scale.y;
    let lo = getFlagsLo(vm);
    lo = (lo ^ 0x1000) >>> 0; // toggle bit12
    lo = (lo | 0x8) >>> 0; // 置位 bit3（缩放脏位）
    setFlagsLo(vm, lo);
  },

  // ── 310 visible：S visible（32 位字面量）── 06-settings.md #310 ────────────────────
  // flags_lo bit0 = visible & 1。★ 与 300/301 写的是同一个位，但置位方不同（06 §6
  // 裁决 #4：社区记的签名是 'b'，一手是 32 位字面量，宽度记错了，语义一致）。
  310: (vm, args) => {
    const visible = literalInt(args, 0);
    setFlagsLo(vm, setBits(getFlagsLo(vm), 0x1, 0, visible));
  },

  // ── 311 resampleMode：S n（32 位字面量）── 06-settings.md #311 ─────────────────────
  // flags_hi bit11（`0x800`）= n & 1。
  311: (vm, args) => {
    const n = literalInt(args, 0);
    setFlagsHi(vm, setBits(getFlagsHi(vm), 0x800, 11, n));
  },

  // ── 312 scrollMode：$S x, $S y ── 06-settings.md #312 ──────────────────────────────
  // flags_hi bit0–1 = x & 3；flags_lo bit30–31 = y & 3。求值顺序 arg0→arg1（06 §2.3，
  // 无随机，顺序对结果不可观察，但仍按文档顺序读，理由同 group-math.js #121 的同款注释：
  // 现在不可观察不代表将来接上会推进 PRNG 的变量号时也不可观察）。
  312: (vm, args, instr) => {
    const x = readArg(vm, instr, 0);
    const y = readArg(vm, instr, 1);
    setFlagsHi(vm, setBits(getFlagsHi(vm), 0x3, 0, x));
    setFlagsLo(vm, setBits(getFlagsLo(vm), 0xc0000000, 30, y));
  },

  // ── 313 resolutionMode：$S n ── 06-settings.md #313 ────────────────────────────────
  // flags_hi bit20–22（`0x700000`）= n & 7。
  313: (vm, args, instr) => {
    const n = readArg(vm, instr, 0);
    setFlagsHi(vm, setBits(getFlagsHi(vm), 0x700000, 20, n));
  },

  // ── 314 attached：$S enable ── 06-settings.md #314 ─────────────────────────────────
  // flags_hi bit23（`0x800000`）= enable & 1。读者（06 §3.7）依赖父子 VM 树（`+0x5ac`
  // 递归链），Child management（500+）还没实现——本组只负责置位，不复刻读者（与 306 同
  // 一套边界：置位是本组的事，读者在渲染器/父子树层）。
  314: (vm, args, instr) => {
    const enable = readArg(vm, instr, 0);
    setFlagsHi(vm, setBits(getFlagsHi(vm), 0x800000, 23, enable));
  },

  // ── 315 colorizeChildren：b enable（低 8 位字面量）── 06-settings.md #315 ──────────
  // flags_hi bit25（`0x2000000`）= enable & 1。
  315: (vm, args) => {
    const enable = literalByte(args, 0);
    setFlagsHi(vm, setBits(getFlagsHi(vm), 0x2000000, 25, enable));
  },

  // ── 316 v8-flag-316：— ── 06-settings.md #316 ──────────────────────────────────────
  // flags_lo |= 2（置位 bit1）。06 §3.2（一手到底，社区 wip:2 收敛）：`draw_vm` 里的
  // 第二道绘制闸门，默认置位（wipe 低 16 位=7 已含 bit1，见本文件 `getFlagsLo` 默认值）。
  316: (vm) => {
    setFlagsLo(vm, getFlagsLo(vm) | 0x2);
  },

  // ── 317 v8-flag-317：— ── 06-settings.md #317 ──────────────────────────────────────
  // flags_lo &= ~2（清位 bit1）。与 316 是同一道闸门的一对开关。
  317: (vm) => {
    setFlagsLo(vm, getFlagsLo(vm) & ~0x2);
  },
};
