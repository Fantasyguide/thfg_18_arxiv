// ANM v8 VM ——Child management 组（opcode 500–509，10 条）。
//
// 依据：engine/anm/th16/08-child.md（下称「08」）§2 的 10 条指令表 + §3/§4/§5 的坐标继承/
// ignoreParent-copyParentVars/world-UI 列表专项。**父子创建的权威是
// engine/anm/th16/02-vm-core.md（下称「02」）§8/§8.1**——08 §3 只是把 02 的字段表按
// opcode 视角重排了一遍，02 §8.1 有一条重排时丢掉的时序细节（★2，见下）。冲突处以 02 为准。
// 每条 handler 上方注释指回具体 opcode 行（双向互指纪律，见 runtime/README.md）。本文件
// 不重新论证任何语义结论——那是 engine/ 层的事，这里只做「把结论翻成 JS」的实现决策。
//
// 消费 core.js 导出的 resolveArg / isVarBit / createVm / stepFrame / registerGroup。
//
// ══════════════════════════════════════════════════════════════════════════════════
// ★★★ 本文件的结构核心：core.js 没有 VM 树，本组要现建一棵 ★★★
// ══════════════════════════════════════════════════════════════════════════════════
// core.js 的 `computeRotZEffective`（10026 有效 z 旋转）那段注释写得很直白：本骨架此前
// *每一个* vm 都等价于 04-variables.md §6.2 递归伪代码里 `root == null` 的分支，且明确
// 声明这「不是漏了一部分逻辑的近似值，是该分支下 100% 精确的实现」——因为在本组之前，
// 骨架里根本不存在「有父」的 vm。本组一旦把 parent/root 接起来，这个前提就不再成立了：
// 有父的 child vm 上，10026 应该递归累加祖先链的 rotation.z，但 core.js 那段实现完全不知道
// `vm._child.root` 的存在，会继续走 `root == null` 分支，安静地漏掉祖先旋转。
// **本文件不改 core.js、也不改 10026 的实现**（brief 硬约束）——这里只把「谁该改、怎么改」
// 讲清楚，写进任务报告，交给持有 core.js 的人去接。
//
// VM 树的落点：**不进 core.js 的顶层 vm 契约**，理由与 group-settings.js 的 flagsLo/flagsHi
// 先例、group-general.js 的 `vm._general` 先例一致——core.js 这一轮明确不让动，新公共契约
// 字段该长什么样是持有 core.js 的人的裁决权，本组不越权。选型：
//   - `parent`/`root`/`children`/`layer`/文件级脚本表/tick 用的 ops 表/508 写的回调下标——
//     这些是本组*新增*的概念，没有任何既有字段可以复用，全部收进惰性建的 `vm._child`
//     （`ensureChildState`，见下）。
//   - `pos_2`（+0x4e0）/`entity_pos`（+0x5e0）——**不是新概念**，是 group-general.js 已经在
//     管理的物理字段（`vm._general.pos2`/`vm._general.entityPos`）。本组直接 import 它导出的
//     `ensureGeneralState`，读写同一份存储，不建第二份影子状态（见 group-general.js 头部
//     2026-09-09 那条注释）。
//   - `flags_hi`（含本组要动的 3 个位：0x400 托管 / 0x2000000 继承位 / 0x10000 ignoreParent）
//     ——core.js 已经把它做成顶层字段（`vm.flagsHi`，wipe 默认 0x4000），本组直接读写，
//     不新建任何字段（与 group-settings.js 的 getFlagsHi/setFlagsHi 用法一致）。
//
// ══════════════════════════════════════════════════════════════════════════════════
// ★★★ ★2：`flags_hi | 0x400` 的写入时序，在本骨架里已经不可能被复现出「有区别」 ★★★
// ══════════════════════════════════════════════════════════════════════════════════
// 02 §8.1：root 把 `flags_hi | 0x400` 写在 `copy_vm` **之后**（`th16:0x46eedf`），child
// 写在 `copy_vm` **之前**（`th16:0x46eda8`）；02 §8.1 的三条易错细节额外指出：child 路径
// 写 `entity_pos = 0` 也在 copy_vm 之前，之所以没被 copy_vm 内部的 `wipe_suffix` 抹掉，是
// 因为 `wipe_suffix`（`th16:0x407a50`）特意保留了 `+0x5e0..0x5eb`（以及 `+0x18`/`+0x53c`）
// 不清零；`+0x534`（flags_hi）落在 `copy_vm` 拷贝的「前 0x538 字节 prefix」范围内，所以
// 「写在 copy_vm 之前」这件事本身是有风险的（prefix 拷贝会用模板的 flags_hi 整体覆盖掉
// 之前的写入）——child 路径没有出这个问题，是因为 0x400 这个具体位在模板里恰好也会是
// 该有的值（或者该位不落在被覆盖的争议区间——08 原文没有进一步展开这一步，本文件也不替它
// 补）。**这一整套「prefix / suffix / wipe_suffix 特例保留」的机制，本骨架完全没有**：
// `createVm(script)` 不经过任何模板/克隆步骤，新 vm 从一份写死的默认值直接开始，根本不存在
// 「后面一步会不会把前面写的值覆盖掉」这回事。所以 ★2 在本骨架里**没有可以被测试观察到的
// 行为差异**——两条路径不管先写后写，最终 `flagsHi & 0x400` 都是「置位」，没有第二次覆盖
// 把它冲掉。
//
// 本文件的处理方式：**没有假装这件事不存在**——下面 `createManagedChild`/`createManagedRoot`
// 两个函数里，`flagsHi |= 0x400` 这一行在代码里的相对位置特意分别摆在「函数体最前面」
// （对应 child「copy_vm 之前」）和「其余字段写完之后」（对应 root「copy_vm 之后」），照抄
// 02 §8.1 的顺序，而不是把两条路径写成看起来一样。但这只是「让读者看代码时能对上文档」的
// 记录手段，不代表这个顺序在本骨架里有任何功能后果——测试只能断言两条路径最终都置位
// （见 vm-child.test.js），断不出「顺序错了会怎样」，因为本骨架里顺序真的不影响结果。
// **这条留给报告：如果以后 core.js/某个更高层加入了「模板预跑 + copy_vm」机制**（比如某个
// AnmLoaded 层要求「新实例先克隆模板 prefix，再叠加 create_managed_* 的专属写入」），
// 上面这条关于「0x400 写入时序为什么重要」的推理才会重新变得可观察，到时候这两处的
// 相对位置就不只是摆设了。
import {
  createVm,
  stepFrame,
  resolveArg,
  isVarBit,
  registerGroup,
} from './core.js';
import { ensureGeneralState } from './group-general.js';

// ── mode（500–503 共用的第 4 参立即数，08 §2.1）：bit2 判列表（0=world/1=UI），
//    bit1 判位置（0=尾/1=头），bit0 不用。──────────────────────────────────────────
const MODE_WORLD_TAIL = 0; // 500 createChild
const MODE_WORLD_HEAD = 2; // 502 prependChild
const MODE_UI_TAIL = 4; // 501 createChildUi
const MODE_UI_HEAD = 6; // 503 prependChildUi

// flags_hi 的三个相关位（02 §3.5/§3.6、08 §0/§3/§4.1）。
const FLAG_MANAGED = 0x400; // 「由管理器托管」（create_managed_* 置）
const FLAG_INHERITED_UNKNOWN = 0x2000000; // 「创建时从父/源继承的一位」，语义未定（02 §3.5 🟡）
const FLAG_IGNORE_PARENT = 0x10000; // 507 ignoreParent 设的位；也是 10026 递归的继承闸门（04 §6.2）

// 08 §6.1：effect 表 th16:0x4a2250，8-dword 项 {s16 anm槽, s16 脚本, callback, 6 个回调下标}。
// 本组只考证过 kind=1（真实数据 508 ×42 全部是它，04-variables.md §8 更正块③给出完整行：
// {anmSlot:0, script:0, callback: th16:0x405670, 六回调下标: {2,2,2,2,1,1}}）。
// ★3：`508` 要如实把下标写进 vm 字段，但**本阶段不实现**——
//   - `callback`（th16:0x405670，安装期同步调用）：在当前 VM 上建顶点缓冲，渲染层，跳过；
//   - `on_tick` 回调下标 2 → `ANM_ON_TICK_CALLABLES[2]` = th16:0x405700（02-vm-core.md
//     §3.5/04-variables.md §8）：每帧读变量 10035 加 `rotation.z` 算极坐标，渲染几何层，
//     跳过。**将来谁要实现 on_tick callable 派发（大概率是 core.js 的 frameTail 或某个
//     system 层），从这里的 `onTickIdx` 接。**
// ❓（08 §9）：`slotC8`/`slotCC`/`slotD4`/`slotD8` 四个下标对应哪几张回调表，02-vm-core.md
//   §3.5 没登记（只登记了 `+0x5c0`/`+0x5c4`/`+0x5d0`/`+0x5dc`）——如实写进字段，语义不下判断。
const EFFECT_TABLE = {
  1: {
    indices: { onTickIdx: 2, slotC8: 2, slotCC: 2, onInterruptIdx: 2, slotD4: 1, slotD8: 1 },
  },
};

// ══════════════════════════════════════════════════════════════════════════════════
// § 惰性 vm 扩展：vm._child
// ══════════════════════════════════════════════════════════════════════════════════
// 导出是刻意的——parent/root/children 这几个字段没有别的公开读法（不像 pos/rotation 那样
// 有 vm 顶层字段可读），调用方（含测试）要断言「继承了谁的 root」只能直接读这里，跟
// core.js 文件头「内部簿记不对调用方隐藏，只是不保证跨版本稳定」的态度一致。
export function ensureChildState(vm) {
  if (vm._child) return vm._child;
  vm._child = {
    parent: null, // +0x5b0，02 §3.3；root 不写（= null，自己是根）
    root: null, // +0x5ac，02 §3.3；child = 父的 root ?: 父（08 §3）
    children: [], // +0x588 子链头的 JS 版；08 §5：create_managed_child 尾部无条件 prepend
    layer: 0, // +0x18；本组是骨架里唯一持有这个字段的地方（渲染管理器按它分桶，
    // 但本骨架没有 AnmManager，只如实继承/记录，不做分桶）
    insertMode: null, // 500–503 的 mode（0/2/4/6，见 08 §2.1）；🟡（08 §9：+0x51c 存的这个
    // mode 下游被谁读没穷举）——只记录，供未来 AnmManager 层的 world/ui 渲染链表使用，
    // 本组不建那两张链表（08 §5：那是与子链无关的另一套机制）
    effectCallbackIndices: null, // 508 写；见 EFFECT_TABLE 注释
    scripts: null, // 「当前 VM 所属 .anm 文件」的脚本数组，按运行序号索引（08 §0.1/§1.1：
    // create_managed_* 只能在这个数组里选脚本）。本骨架没有 AnmLoaded/文件概念，这里是
    // 调用方唯一的接入点——顶层 vm 建好后设置一次 `ensureChildState(vm).scripts = [...]`，
    // 会随每次创建自动传播给新 vm（因为新 vm 与创建者永远同属一个「文件」）
    ops: null, // 「创建即 tick」要用的完整 ops 表（含本组自身 + 100+/300+/400+ 等）。未设置时
    // 回退到 DEFAULT_TICK_OPS（只含本组自己，见文件尾）——与 group-general.js 的
    // `_stepInterpolators` 钩子同一个精神：没装就是没接，不静默假装跑对了全部语义，
    // 但也不因为「没接」就直接抛（这是组合期的接线缺口，不是本组自己的语义缺口）
  };
  return vm._child;
}

function getFileScripts(creatorVm) {
  const state = ensureChildState(creatorVm);
  if (!state.scripts) {
    throw new Error(
      'group-child: vm._child.scripts 未挂载——08-child.md §0.1/§1.1：createChild/createRoot ' +
        '只能在“当前 VM 所属的 .anm 文件”里按脚本运行序号选脚本；本骨架没有 AnmLoaded/文件的 ' +
        '概念，请调用方在顶层 vm 上先 `ensureChildState(vm).scripts = [...]`（会随创建自动 ' +
        '传播给子/根 vm）。',
    );
  }
  return state.scripts;
}

function getScript(creatorVm, scriptId) {
  const scripts = getFileScripts(creatorVm);
  const script = scripts[scriptId];
  if (!script) {
    throw new Error(
      `group-child: 脚本运行序号 ${scriptId} 在 vm._child.scripts（长度 ${scripts.length}）里找不到` +
        '（08-child.md §8 坑表：arg0 是脚本运行序号，不是文件里的 script id）。',
    );
  }
  return script;
}

// createVm 的 opts：继承创建者的模拟参数（gameSpeed/maxInstructionsPerFrame/
// stepInterpolators）——brief 没有专门论证这条，是本文件的范围决策：同一棵树的 vm 共享
// 同一套模拟步长/插值钩子，比每个子 vm 各用默认值更贴近「同一帧、同一个世界」的直觉。
function buildChildOpts(creatorVm) {
  const opts = {};
  if (typeof creatorVm._gameSpeed === 'number') opts.gameSpeed = creatorVm._gameSpeed;
  if (typeof creatorVm._maxInstructionsPerFrame === 'number') {
    opts.maxInstructionsPerFrame = creatorVm._maxInstructionsPerFrame;
  }
  if (typeof creatorVm._stepInterpolators === 'function') {
    opts.stepInterpolators = creatorVm._stepInterpolators;
  }
  return opts;
}

// 02 §8：「两个 helper 都在插列表之前各调一次 AnmVm::run」——创建即 tick。本骨架没有
// AnmManager 的世界/UI 列表，这里的「插列表」specifically 指 08 §5 的子链 prepend（对
// createRoot 没有子链可插，但 tick 仍然要跑，语义上等价于「插进 world 列表之前」）。
function tickOnce(vm) {
  const state = ensureChildState(vm);
  const ops = state.ops ?? DEFAULT_TICK_OPS;
  stepFrame(vm, ops);
}

function inheritFileContext(creatorVm, newVm) {
  const creatorState = ensureChildState(creatorVm);
  const newState = ensureChildState(newVm);
  newState.scripts = creatorState.scripts;
  newState.ops = creatorState.ops;
}

// ── 08-child.md #500/#501/#502/#503/#505 共用：create_managed_child（02 §8.1「child」列）──
function createManagedChild(creatorVm, scriptId, mode) {
  const creatorState = ensureChildState(creatorVm);
  const script = getScript(creatorVm, scriptId);
  const child = createVm(script, buildChildOpts(creatorVm));
  inheritFileContext(creatorVm, child);
  const childState = ensureChildState(child);

  // ★2：flags_hi | 0x400 写在最前面，照抄 02 §8.1「child 路径在 copy_vm 之前」的相对顺序
  // （本骨架没有 copy_vm，这个顺序在这里没有可观察的功能后果，见文件头 ★2 大段说明）。
  child.flagsHi = (child.flagsHi | FLAG_MANAGED) >>> 0;

  // layer ← 父的 layer（02 §8.1 表；08 §3 表同一行，创建者 = 父）
  childState.layer = creatorState.layer;

  // entity_pos 清 0（02 §8.1「child」列：`0x46edba`/`0x46edc3`，写在 copy_vm 之前，靠
  // wipe_suffix 保留 +0x5e0..0x5eb 才活下来——本骨架没有 wipe_suffix，ensureGeneralState
  // 的默认值本来就是 {0,0,0}，这里显式赋值只是让「清 0」这件事在代码里可见，不依赖
  // 「默认值恰好是 0」这个容易被后人改掉的隐藏假设）。
  ensureGeneralState(child).entityPos = { x: 0, y: 0, z: 0 };

  // rotation / pos_2：child 不写，保留 createVm 的默认值（08 §3 结论1：「child 不继承
  // 任何变换值」）。

  // flags_hi bit 0x2000000：从父同位复制（02 §8.1；语义未定，🟡，见常量注释）。
  child.flagsHi = ((child.flagsHi & ~FLAG_INHERITED_UNKNOWN) | (creatorVm.flagsHi & FLAG_INHERITED_UNKNOWN)) >>> 0;

  // parent ← 当前 VM；root ← 父的 root ?: 父（08 §3）
  childState.parent = creatorVm;
  childState.root = creatorState.root ?? creatorVm;

  // 创建即 tick（02 §8）。
  tickOnce(child);

  // 子链：08 §5 结论2——无条件 prepend，与 mode（世界/UI、头/尾）无关，create 与 prepend
  // 写法相同（那条差异只影响下面记录的 insertMode，供未来渲染列表用）。
  creatorState.children.unshift(child);
  childState.insertMode = mode;

  return child;
}

// ── 08-child.md #504/#506 共用：create_managed_root（02 §8.1「root」列）─────────────
function createManagedRoot(sourceVm, scriptId) {
  const sourceState = ensureChildState(sourceVm);
  const script = getScript(sourceVm, scriptId);
  const root = createVm(script, buildChildOpts(sourceVm));
  inheritFileContext(sourceVm, root);
  const rootState = ensureChildState(root);
  const rootGeneral = ensureGeneralState(root);
  const sourceGeneral = ensureGeneralState(sourceVm);

  // layer ← 源的 layer（02 §8.1「root」列：写在 copy_vm 之后；本骨架无顺序敏感性）
  rootState.layer = sourceState.layer;

  // flags_hi bit 0x2000000：从源同位复制
  root.flagsHi = ((root.flagsHi & ~FLAG_INHERITED_UNKNOWN) | (sourceVm.flagsHi & FLAG_INHERITED_UNKNOWN)) >>> 0;

  // entity_pos ← 源同偏移整块拷（x/y/z 全拷，02 §8.1「root」列 `0x46ef0a`–`0x46ef20` 四条
  // 独立标量搬运，与下面 pos_2 的「只搬 x/y」不同，别搞混）。
  const srcEntity = sourceGeneral.entityPos;
  rootGeneral.entityPos = { x: srcEntity.x, y: srcEntity.y, z: srcEntity.z };

  // rotation ← 源同偏移整块拷（x/y/z）
  root.rotation = { x: sourceVm.rotation.x, y: sourceVm.rotation.y, z: sourceVm.rotation.z };

  // ★ pos_2 ← 源的 pos，且**只搬 x/y，不搬 z**——02 §8.1「root」列 `0x46ef38`–`0x46ef48`：
  // `MOVQ XMM0,[EDX+0x2c]; MOVQ [ESI+0x4e0],XMM0` 是一条 8 字节（两个 float32）的搬运，
  // 只覆盖 pos_2.x/.y；pos_2.z 保留 createVm 的默认值 0（08 §2.2 一手同款结论用在
  // 505/506 身上，这里是 504 自己不带参数版的同一条搬运指令，同样只搬两个分量）。
  // ⚠️ 落点是新 VM 的 `pos_2`（+0x4e0），**不是** `pos`（+0x2c，那是模板/创建期默认值，
  // 这里完全不碰）——这是 08 §3 点名「最容易写成同偏移」的那处非对称，改坏验证见报告。
  rootGeneral.pos2.x = sourceVm.pos.x;
  rootGeneral.pos2.y = sourceVm.pos.y;

  // flags_hi | 0x400：写在其余字段之后，照抄 02 §8.1「root 路径在 copy_vm 之后」的相对
  // 顺序（同 ★2，本骨架里没有可观察的功能后果）。
  root.flagsHi = (root.flagsHi | FLAG_MANAGED) >>> 0;

  // parent/root 不写（= null，自己是根）——ensureChildState 的默认值已经是 null，这里
  // 显式再写一次只是让「root 无父」这件事在代码里可读。
  rootState.parent = null;
  rootState.root = null;

  // 创建即 tick（02 §8）。
  tickOnce(root);

  // root 没有子链可插；08 §2 行 504 说它进 world 列表尾部，本组不建那张列表，只记录 mode
  // 供未来 AnmManager 层用（与 500–503 的 insertMode 同一个字段，含义一致：0=world 尾）。
  rootState.insertMode = MODE_WORLD_TAIL;

  return root;
}

// ── 08-child.md §6：父死 → 整棵子树销毁（权威在 02-vm-core.md §6：th16:0x46e340 判返回值
//    → th16:0x46e660 递归把整棵子树标 flags_hi |= 0x40 再销毁）。08 正确地把这条转引给
//    02、没有重复论证，本文件同样以 02 为准。────────────────────────────────────────────
// core.js 的 stepFrame 只会把「本 vm 该不该继续存在」记在 `vm.alive`/`vm.pc`（-1）上，
// 完全不知道 vm 树的存在（core.js 文件头「不建模」清单里也没提过树）——所以「父死子树
// 一并销毁」不可能是 stepFrame 自动做的事，必须由知道树结构的这一层（也就是这里）在
// 检测到某个 vm 死亡之后主动调用。谁来触发这次检测，是下面「骨架接线需求」里要报告的事。
export function destroyTree(vm) {
  vm.flagsHi = (vm.flagsHi | 0x40) >>> 0; // 02 §6.1：flags_hi |= 0x40（请求销毁/已销毁标记）
  vm.alive = false;
  vm.pc = -1;
  const state = ensureChildState(vm);
  for (const child of state.children) {
    destroyTree(child);
  }
  state.children.length = 0; // 已经整棵销毁，清空子链，避免悬挂引用被误当「还活着的子」
  return vm;
}

// ── 参数读取小工具 ──────────────────────────────────────────────────────────────
function readIntArg(vm, instr, index) {
  return resolveArg(vm, instr.args[index], isVarBit(instr.paramMask, index), 'int');
}
function readFloatArg(vm, instr, index) {
  return resolveArg(vm, instr.args[index], isVarBit(instr.paramMask, index), 'float');
}

export const childGroup = {
  // 08-child.md #500 createChild —— 建子 VM，进 world 尾部，父 = 当前
  500: (vm, args, instr) => {
    const scriptId = readIntArg(vm, instr, 0);
    return createManagedChild(vm, scriptId, MODE_WORLD_TAIL);
  },

  // 08-child.md #501 createChildUi —— 建子 VM，进 UI 尾部，父 = 当前
  501: (vm, args, instr) => {
    const scriptId = readIntArg(vm, instr, 0);
    return createManagedChild(vm, scriptId, MODE_UI_TAIL);
  },

  // 08-child.md #502 prependChild —— 建子 VM，进 world 头部，父 = 当前
  // thpages wip:1（社区自己也标了「插到列表头」这条不完全确定）；一手 08 §2.1 已坐实
  // mode=2，本组按一手实现，thpages 的 wip 只是提醒「社区侧信心不满」，不改变实现。
  502: (vm, args, instr) => {
    const scriptId = readIntArg(vm, instr, 0);
    return createManagedChild(vm, scriptId, MODE_WORLD_HEAD);
  },

  // 08-child.md #503 prependChildUi —— 建子 VM，进 UI 头部，父 = 当前（thpages wip:1，同 502）
  503: (vm, args, instr) => {
    const scriptId = readIntArg(vm, instr, 0);
    return createManagedChild(vm, scriptId, MODE_UI_HEAD);
  },

  // 08-child.md #504 createRoot —— 建根 VM（无父），进 world 尾部，拷源 transform
  // thpages wip:1；02 §8.1 已一手逐条坐实，见 createManagedRoot。
  504: (vm, args, instr) => {
    const scriptId = readIntArg(vm, instr, 0);
    return createManagedRoot(vm, scriptId);
  },

  // 08-child.md #505 createChildPos —— 500 + 把子 pos_2.x/y 写成 x/y
  // 求值次序（08 末尾「操作数求值次序」一节）：(0,1,2) 升序，且次序被数据依赖强制——
  // script id 是 create 调用的入参，必须先解出来才能调用 createManagedChild；x/y 是
  // 创建（含创建即 tick）*之后*另外写回的字段，所以这里的读取顺序特意写成
  // 「先 scriptId → 再 create（含 tick，可能消费 PRNG）→ 再 x → 再 y」，跟字面顺序
  // 一致，不要为了「看起来整洁」把三次 resolveArg 挪到一起提前算掉。thpages wip:1。
  505: (vm, args, instr) => {
    const scriptId = readIntArg(vm, instr, 0);
    const child = createManagedChild(vm, scriptId, MODE_WORLD_TAIL);
    const x = readFloatArg(vm, instr, 1);
    const y = readFloatArg(vm, instr, 2);
    const general = ensureGeneralState(child);
    // 只写 .x/.y，.z 不写（08 §2.2 一手：MOVQ 只覆盖两个 dword）。
    general.pos2.x = x;
    general.pos2.y = y;
    return child;
  },

  // 08-child.md #506 createRootPos —— 504 + 把新根 pos_2.x/y 写成 x/y
  // 求值次序同 505（08 末尾一节：506 自身 case 体只读 arg0，随即 jmp 进与 505 共用的尾巴
  // 读 x/y，属同族，同样是 (0,1,2) 升序、同样被数据依赖强制）。thpages wip:1。
  506: (vm, args, instr) => {
    const scriptId = readIntArg(vm, instr, 0);
    const root = createManagedRoot(vm, scriptId);
    const x = readFloatArg(vm, instr, 1);
    const y = readFloatArg(vm, instr, 2);
    const general = ensureGeneralState(root);
    general.pos2.x = x;
    general.pos2.y = y;
    return root;
  },

  // 08-child.md #507 ignoreParent —— flags_hi bit 0x10000 = arg0 & 1（08 §4.1：设值，
  // 不是翻转；与 group-settings.js 的「按值写位」惯用法同一手法，这里就地内联，不新增
  // 公共工具——本组只有这一处用到，犯不着为一次调用抽公共函数，group-general.js 文件头
  // 也是同样的取舍理由）。
  507: (vm, args, instr) => {
    const n = readIntArg(vm, instr, 0);
    const bit = (n & 1) << 16; // 0x10000 = 1<<16
    vm.flagsHi = ((vm.flagsHi & ~FLAG_IGNORE_PARENT) | bit) >>> 0;
  },

  // 08-child.md #508 createEffect —— 按 0x4a2250 effect 表重配当前 VM（不建子 VM！
  // 08 §6.1 已裁决社区「造子图」的说法不对，一手是「重配自己」）。
  // ★3：只如实写 6 个回调下标，setup callback（th16:0x405670）与 on_tick callable
  // （th16:0x405700）都**不实现**，理由见 EFFECT_TABLE 上方注释。thpages wip:1。
  508: (vm, args, instr) => {
    const kind = readIntArg(vm, instr, 0);
    const entry = EFFECT_TABLE[kind];
    if (!entry) {
      throw new Error(
        `group-child 508 createEffect: effect kind ${kind} 未建模——08-child.md §6.1/§9：` +
          '一手只核过 kind=1（真实数据 42 次全部是它），其余 kind 的表项内容没有 ground truth，' +
          '宁可抛也不要编一份假下标出来。',
      );
    }
    const state = ensureChildState(vm);
    state.effectCallbackIndices = { ...entry.indices };
  },

  // 08-child.md #509 copyParentVars —— 从父拷 +0x4a0..0x4df 64 字节变量块（08 §4.2：
  // 父为空则整条是 no-op；不拷 pos/rotation）。无参，∅。
  509: (vm) => {
    const state = ensureChildState(vm);
    const parent = state.parent;
    if (!parent) return; // 08 §4.2：父为空 = no-op
    vm.i = [...parent.i]; // 10000–10003
    vm.f = [...parent.f]; // 10004–10007
    vm.i4 = parent.i4; // 10008
    vm.i5 = parent.i5; // 10009
    vm.mysteryVars = { ...parent.mysteryVars }; // 10033–10035
    vm.randParamOne = parent.randParamOne; // 10027
    vm.randParamPi = parent.randParamPi; // 10028
    vm.randParamInt = parent.randParamInt; // 10029
  },
};

// tickOnce 未接到调用方的 ops 时的兜底：至少让本组自己的 10 个 opcode（含嵌套的
// 500+ 创建）在新 vm 的 time<=0 段里生效，不是完全空表——但仍然覆盖不到 100+/300+/400+
// 等其他组的语义，见 ensureChildState 里 `ops` 字段的注释与任务报告「骨架接线需求」。
const DEFAULT_TICK_OPS = {};
registerGroup(DEFAULT_TICK_OPS, childGroup);
