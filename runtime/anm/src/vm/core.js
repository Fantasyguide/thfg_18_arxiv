// ANM v8 VM 骨架（零依赖）：取指 / 时间推进 / 跳转 / interrupt / 生死。
//
// 依据：engine/anm/th16/02-vm-core.md（下称「vm-core」）§2 的 `AnmVm_run` 伪代码是本文件
// 的直接实现依据，逐段注释指回它的小节（双向互指纪律，见 runtime/README.md）。跳转族
// （opcode 200–213）的“字面量、不可为变量”这条事实来自 engine/anm/th16/03-math-jumps.md
// （下称「math-jumps」）§2.1/§5，本文件的 jmp 支持只是通用的“handler 可以直接改写 pc”
// 机制，不认识任何具体 opcode 号。
//
// ★ 层次边界（runtime/README.md）：本文件不做任何具体 opcode 的语义判断——见下面
// “不建模”清单。100+/200+/300+/400+/500+/600+ 的语义全部通过 registerGroup 注入的
// `ops` 表间接调用，本文件本身只认 -1/0/1/2/3/4/5/6/7 这 9 个数字（1/2/3/4/6/7 是
// vm-core §2/§6 明确划给主循环自己处理的“生死/时间控制”特例；-1/0/5 分别是隐式脚本
// 结束、nop、interrupt 标签，全部走 default 分支）。
//
// ★ 本文件明确不建模的东西（都是范围决策，不是遗漏——原因见各自注释）：
//   - GAME_SPEED 全局寄存器与 slowdown（vm-core §2 ①、§2.1）：影响的是“位移/旋转怎么积分”
//     这类渲染表现，不影响生死/时间闸门/跳转/interrupt 本身；调用方可以通过
//     `createVm(script, { gameSpeed })` 传入一个固定的每帧步长模拟“没有减速”的情形。
//   - on_tick / on_wait 回调（vm-core §2 ②、§6 帧尾）：这是宿主（AnmManager/弹幕等）
//     的回调表，骨架层没有这些外部概念。
//   - flags_lo/flags_hi 与冻结闸门、暂停闸门（vm-core §3.5/§3.6、§2 ③）：`vm` 的契约里
//     没有 flags 字段，`3 stop`/`4 stopHide` 的差异也只是一个可见性 flag，两者在骨架里
//     退化成同一套时间冻结逻辑。
//   - 帧尾几何（apply_velocities / 全局滚动 / UV 重算 / 插值器，vm-core §2 ⑥、§2.1、§3.4）：
//     这些读写的字段（angular_velocity、uv_scroll_pos、插值器组……）不在 I2 的 vm 契约里，
//     且明确是 General(400+) 组的语义，留给那个任务。
//   - AnmLoaded/AnmManager 层的“模板预跑”“创建即 tick”（vm-core §8）：那是更高一层
//     （entry → 多个活 VM 实例）的职责，`createVm(script, opts)` 直接吃一个 script，
//     没有模板/克隆的概念。
//
// ★ 有一处是刻意“建模”成跟原版不一样的，不在上面“不建模”清单里，单独说：
//   - **分发循环的护栏**（vm-core §9 原话：“主循环无上限……自跳脚本会真的卡死。移植时
//     必须自己加护栏”——这句是文档点名要求的，不能算“正当省略”）。原版没有保护，
//     `200 jmp` 跳回同一时间刻度是已知会让 `0x45fba0` 死转的情形；照抄“无保护”搬到
//     JS 里后果比原版更严重——原版死循环好歹能靠任务管理器强杀进程，浏览器主线程真死
//     没有等价手段。所以 `stepFrame` 的分发循环有一个每帧最大指令数的硬顶
//     （`createVm(script, { maxInstructionsPerFrame })`，默认见下方常量），超过就抛错
//     （带 `vm.pc`/`vm.time`，方便定位是哪个脚本卡死），不会真的把 JS 主线程锁死。
//     代价：理论上单帧执行指令数极端多的合法脚本会被误伤——真实 54 个 th16 anm 里最长
//     单帧执行量远低于默认阈值，目前没有见过这种脚本。
//
// 接口：
//   createVm(script, opts?) -> vm
//   stepFrame(vm, ops) -> void
//   registerGroup(ops, groupTable) -> void
//   resolveArg(vm, raw, isVariable, kind) -> number   （变量右值读，供各 opcode 组用；
//                                                        36 个变量号全覆盖，见 04-variables.md）
//   writeIntVar(vm, varId, value) -> void              （整型变量左值写；只读/类型不匹配抛错）
//   writeFloatVar(vm, varId, value) -> void             （浮点变量左值写；只读/类型不匹配抛错）
//   isVarBit(paramMask, index) -> boolean              （resolveArg 的配套小工具）
//   triggerInterrupt(vm, n) -> void
//   PC_HANDLED                                          （handler 用它告诉骨架“我已经改过 pc”）

const INSTR_HEADER_SIZE = 8; // 01-format.md §5.1，与 anm-v8.js 的 INSTR_HEADER_SIZE 同一常量

// vm-core §9 点名要求的分发循环护栏（见文件头“刻意建模成跟原版不一样”那条）的默认值。
// 54 个真实 th16 anm 里最长单 script 才 43268 条*全脚本*指令（不是单帧执行量，单帧通常
// 只跑同一 time 标签下的几条），这里给了两个数量级的余量；调用方可以用
// `createVm(script, { maxInstructionsPerFrame })` 覆盖（测试里会调小它来快速触发护栏）。
const DEFAULT_MAX_INSTRUCTIONS_PER_FRAME = 200_000;

function instrSize(instr) {
  return INSTR_HEADER_SIZE + instr.args.length * 4;
}

// handler 的返回值用它表示“pc 已经被我自己改过，骨架不要再 += size”——对应 vm-core §1
// 表格里的 “MOV [EBX+0x28],…” 一族（跳转/中断落地/caseReturn/终止），与“ADD [EBX+0x28],size”
// 一族（绝大多数 handler，含默认分支）二选一。绝大多数 handler（比如 100/102 这类纯算术）
// 不需要关心这个符号，直接返回 undefined 即可，骨架会自动 += size。
export const PC_HANDLED = Symbol('anm-vm:pc-handled');

/**
 * @param {{ instructions: Array<{ time: number, opcode: number, paramMask: number, args: number[] }> }} script
 * @param {{ gameSpeed?: number }} [opts]
 */
// `createVm` 认识的 opts 键。**未知键直接抛**，不静默忽略——
// 2026-09-09 实测踩过：写 `createVm(s, { prng: fn })` 注入 PRNG 钩子毫无效果、
// 也不报错，统计里 PRNG 变量全抛，差点误判实现有问题。钩子要直接挂 `vm.prng`。
// 与本模块「宁可抛不要静默」的既有取舍一致（除零、PRNG 缺失、只读变量写入都抛）。
const KNOWN_VM_OPTS = new Set(['gameSpeed', 'maxInstructionsPerFrame', 'stepInterpolators']);

export function createVm(script, opts = {}) {
  for (const key of Object.keys(opts)) {
    if (!KNOWN_VM_OPTS.has(key)) {
      throw new Error(
        `createVm: 不认识的 opts 键 ${JSON.stringify(key)}。` +
          `已知键：${[...KNOWN_VM_OPTS].join(' / ')}。` +
          'PRNG 钩子不走 opts——直接挂 vm.prng / vm.prngReplay（见 group-math.js「实现决策②」）。',
      );
    }
  }
  const instructions = script.instructions;

  // 字节偏移表：vm-core §5（跳转）与 §1“取脚本基址”一节明确 instr_offset 是相对脚本起点
  // 的字节偏移，不是指令下标——所以取指必须能用任意字节偏移直接定位指令，而不是顺序数组
  // 下标。这里预先把每条指令的字节偏移算出来，配一张 offset -> 数组下标 的表，取指时 O(1)。
  const offsets = new Array(instructions.length);
  const offsetIndex = new Map();
  let cursor = 0;
  for (let i = 0; i < instructions.length; i++) {
    offsets[i] = cursor;
    offsetIndex.set(cursor, i);
    cursor += instrSize(instructions[i]);
  }

  return {
    // ── 对外契约（brief 的 interfaces 段） ──────────────────────────────
    i: [0, 0, 0, 0], // I0–I3，vm-core §3.4 / math-jumps §3：变量号 10000–10003
    f: [0, 0, 0, 0], // F0–F3，vm-core §3.4 / math-jumps §3：变量号 10004–10007
    pos: { x: 0, y: 0, z: 0 }, // vm-core §3.3 +0x2c；wipe 默认全零
    scale: { x: 1, y: 1 }, // vm-core §3.3 +0x50；wipe 默认 (1,1)
    rotation: { x: 0, y: 0, z: 0 }, // vm-core §3.3 +0x38；wipe 默认全零
    color: 0xffffffff, // vm-core §3.3 +0x520（color_1）；wipe 默认 0xffffffff

    // ── IV：04-variables.md §2 补的其余变量号落点（不是各存一份影子状态，直接是
    //   36 号变量表里那几个不在 i[]/f[]/pos/rotation 里的物理字段）──────────────
    i4: 0, // 04-variables.md §2 行 10008，`+0x4cc`；I4，除“整型寄存器”外无其他已知语义（🟡）
    i5: 0, // 04-variables.md §2 行 10009，`+0x4d0`；I5，同上（🟡，社区称仅拍照小游戏用，未复核）
    randParamOne: 1.0, // 04-variables.md §2 行 10027，`+0x4d4`；10011/10012 的刻度，wipe 默认 1.0
    randParamPi: Math.fround(Math.PI), // 04-variables.md §2 行 10028，`+0x4d8`；10010 的刻度，wipe 默认 π
    randParamInt: 0x10000, // 04-variables.md §2 行 10029，`+0x4dc`；10022 整读的模数，wipe 默认 0x10000
    mysteryVars: { x: 0, y: 0, z: 0 }, // 04-variables.md §2 行 10033–10035，`+0x4c0/4c4/4c8`；
    // RV2 定论（04 §8）：三个无固定引擎语义的通用可写浮点暂存，字段名沿用社区叫法
    // “mystery”但不代表真是角度——ZUN 数据里 x/y 常存 2D 偏移、z 常存角度，这是用法
    // 不是约束（04 §8.5：“别按角度做归一”）。wipe 只 memset 0，默认 0.0（04 §8.1）。
    flagsLo: 0x7, // vm-core §3.3 `+0x530`；`AnmVm::wipe` 把低 16 位写成 7（bit0/1/2）。
    // bit0 = 可见位（`1 delete` 与 `4 stopHide` 都清它，见 05-system.md §8.1）；
    // bit3 = 缩放脏位（`308/309` 置，见 06-settings.md §2.3）
    flagsHi: 0x4000, // vm-core §3.3 `+0x534`；wipe = `(flags_hi & ~0x8000) | 0x4000`，
    // 新建 VM 的前值为 0 ⇒ 0x4000。**这个默认值有行为意义**：vm-core §2 的
    // `(flags_hi & 0xc000) == 0x4000` 是「可暂停」判据（`th16:0x45fa91`）
    time: 0, // vm-core §3.2 zTimer.current；wipe 后 Timer::set_value(0)
    pc: 0, // vm-core §3.1 instr_offset；新建 VM 从脚本开头跑
    alive: true, // 骨架自己的记账字段：真实引擎用 AnmVm_run 的返回值（1=销毁）表示这件事，
    // 骨架把它落成一个持久字段，好让调用方在 stepFrame 之外也能查

    // ── 内部簿记（不在对外契约里，但没有理由对调用方隐藏——前缀 _ 只是提醒“这是实现细节，
    //   不保证跨版本稳定”）──────────────────────────────────────────────
    _instructions: instructions,
    _offsets: offsets,
    _offsetIndex: offsetIndex,
    _timeF: 0, // vm-core §3.2 zTimer.current_f
    _timePrev: -999999, // vm-core §3.3：wipe 把 previous 设成哨兵 0xfff0bdc1(=-999999)，不是 0
    _pendingInterrupt: 0, // vm-core §3.1 pending_interrupt；0 = 无（interrupt 号 0 无法投递，§5 结论5）
    _savedTime: 0, // vm-core §3.1 saved_time（配合 caseReturn，见 §3.2 “Timer::copy 的坑”）
    _savedPc: 0, // vm-core §3.1 saved_instr_offset
    _gameSpeed: opts.gameSpeed ?? 1, // 见文件头“不建模”清单：真实 GAME_SPEED 是全局态，
    // 骨架把它简化成每个 vm 自己的固定步长
    _stepInterpolators: opts.stepInterpolators ?? null, // 07-general.md §3 的帧尾插值器钩子：
    // 由调用方在装 generalGroup 时一并注入（`createVm(script, { stepInterpolators })`）。
    // 见 frameTail：骨架只负责调用时序，不认识插值器结构本身。
    _maxInstructionsPerFrame: opts.maxInstructionsPerFrame ?? DEFAULT_MAX_INSTRUCTIONS_PER_FRAME,
    // 见文件头“分发循环的护栏”一条：vm-core §9 点名要求移植方自己加保护
  };
}

/**
 * 供各 opcode 组把自己的 handler 表并入总的 ops 表。
 * @param {Record<number, Function>} ops
 * @param {Record<number, Function>} groupTable
 */
export function registerGroup(ops, groupTable) {
  for (const key of Object.keys(groupTable)) {
    if (Object.prototype.hasOwnProperty.call(ops, key)) {
      // 两个组认领了同一个 opcode 号，几乎总是 bug（要么抄错了号，要么两个组的范围重叠）——
      // 静默覆盖会让后注册的组悄悄吃掉前一个组的 handler，排查起来会很痛苦，宁可现在就炸。
      throw new Error(`registerGroup: opcode ${key} 已经被注册过，拒绝覆盖（多半是两个组认领了同一个号）`);
    }
    ops[key] = groupTable[key];
  }
}

/**
 * 触发 interrupt：对应 vm-core §5 的 `AnmManager::interrupt_tree` 那步
 * `vm.pending_interrupt = n`（`0x46f0e3`）——骨架不实现“找到 VM / 遍历子树”那部分
 * （那是 AnmManager 的职责，骨架里只有一个裸 VM），只留这个最小接口：
 * 调用方（未来的管理器层，或直接测试）负责先找到 vm，再喊它。
 */
export function triggerInterrupt(vm, n) {
  vm._pendingInterrupt = n;
}

function timerAdjust(vm, delta) {
  // vm-core §3.2 Timer::increment/decrement 与 §2 case 6 的公共骨架：previous = 旧 current，
  // current_f 累加，current 截断向零取整。三处调用点（帧尾 +speed、stop 的 -speed、
  // wait 的 -n）全部走这一条路径。
  vm._timePrev = vm.time;
  vm._timeF += delta;
  vm.time = Math.trunc(vm._timeF); // vm-core §9：CVTTSS2SI 是截零，不是 floor
}

function timerSetValue(vm, v) {
  // vm-core §3.2 Timer::set_value：cur = v；prev = v - 1；cur_f = (float)v。
  // 跳转成功（math-jumps §6）与 interrupt 命中（vm-core §5）都调这个，参数来源不同。
  vm.time = v;
  vm._timePrev = v - 1;
  vm._timeF = v;
}

function terminateDeleted(vm) {
  // vm-core §6.1：opcode -1（隐式脚本结尾）与 opcode 1（delete）共用同一段代码——
  // instr_offset 写 -1，vm 被标记为“请销毁”。真实引擎里“请销毁”是 AnmVm_run 的
  // 返回值 1，交给外层管理器处理；骨架没有管理器，直接把这件事记在 vm.alive 上。
  vm.pc = -1;
  vm.alive = false;
}

function fetchInstruction(vm) {
  const idx = vm._offsetIndex.get(vm.pc);
  if (idx === undefined) {
    // 01-format.md §5.1：脚本末尾那条 opcode=-1 的结束标记不进 instructions 数组，
    // 但仍然占 8 字节，主循环“到达它”这件事本身就等价于取到一条 opcode -1 的指令。
    // 这里没有单独存一份“结束标记”，而是直接把“取不到指令”统一处理成同一件事：
    // 要么是正常跑到了脚本末尾，要么是 pc 被算错跳到了非指令边界——vm-core §9 明确
    // 主循环没有任何越界保护，这里骨架选择把两种情况都当“脚本已终止”处理，而不是
    // 抛异常读到别的指令的字节：宁可让脚本提前结束，也不要曲解垃圾数据当合法指令跑。
    return null;
  }
  const instr = vm._instructions[idx];
  return { instr, size: instrSize(instr), idx };
}

// ── interrupt：vm-core §5 的 DO_INTERRUPT 伪代码 ──────────────────────────────
function doInterrupt(vm) {
  const want = vm._pendingInterrupt;
  vm._pendingInterrupt = 0; // 0x45fb21：无论命中与否都清空 pending

  // §5：从脚本开头线性扫，找 `ins 5 <want>`；顺带记住 `ins 5 -1`（default 分支）。
  let matchIdx = -1;
  let fallbackIdx = -1;
  for (let i = 0; i < vm._instructions.length; i++) {
    const ins = vm._instructions[i];
    if (ins.opcode !== 5) continue;
    if (ins.args[0] === want) {
      matchIdx = i;
      break; // 同号标签只有第一个生效（§5 结论2）
    }
    if (ins.args[0] === -1 && fallbackIdx < 0) fallbackIdx = i;
  }

  const idx = matchIdx >= 0 ? matchIdx : fallbackIdx;
  if (idx < 0) {
    // §5 伪代码：扫到 -1 都没命中、也没有 default 标签 -> Timer_decrement + 直接帧尾。
    timerAdjust(vm, -vm._gameSpeed);
    frameTail(vm);
    return 'frame-done';
  }

  // §5 / §3.2「Timer::copy 的坑」：快照只保留 .current（截掉小数位与 previous），
  // 所以这里不需要额外记 _timeF/_timePrev——vm.time 本身就是已经截断过的整数。
  vm._savedTime = vm.time;
  vm._savedPc = vm.pc;

  const label = vm._instructions[idx];
  timerSetValue(vm, label.time); // ★ 时间设成标签自己的 time，不是任何参数（§5 结论3）
  vm.pc = vm._offsets[idx] + instrSize(label); // 跳过 case 指令本身（§5 `off += ins.size`）
  return 'continue';
}

function frameTail(vm) {
  // vm-core §2 ⑥：本骨架只保留“脚本时间每帧 += GAME_SPEED”这一条（§4 结论）；
  // 几何/回调部分见文件头“不建模”清单。
  timerAdjust(vm, vm._gameSpeed);
  // 07-general.md §3：12 组帧尾插值器（`AnmVm::step_interpolators` th16:0x463b30）——
  // `*Time` 指令往插值器里填「起点/终点/切线/剩余帧闸门/mode」，真正的推进在帧尾。
  // 状态与步进逻辑都在 group-general.js（骨架不认识那 12 组的结构），这里只负责按
  // 真机的时序调用它：**在 timerAdjust 之后**。
  // 用运行时注入而不是静态 import：core.js 是骨架，不该反向依赖任何 opcode 组
  // （层次归属：组可以依赖骨架，骨架不依赖组）。没装就是没实现该组，不报错。
  if (typeof vm._stepInterpolators === 'function') vm._stepInterpolators(vm);
}

/**
 * 推进一帧。对应 vm-core §2 的 `AnmVm_run`（裁剪掉文件头“不建模”清单里那些部分）。
 * @param {ReturnType<typeof createVm>} vm
 * @param {Record<number, (vm: object, args: number[], instr: object) => (typeof PC_HANDLED | void)>} ops
 */
export function stepFrame(vm, ops) {
  // §2 ③ 第一道早退闸门：`vm.instr_offset < 0` = 已终止（delete/static 留下的 -1）。
  if (vm.pc < 0) return;

  // §2 ④：取指前先处理挂起的 interrupt。
  if (vm._pendingInterrupt !== 0) {
    const outcome = doInterrupt(vm);
    if (outcome === 'frame-done') return;
    // 'continue'：pc/time 已经被 doInterrupt 设到标签落点，落进下面的取指循环继续跑，
    // 和真实引擎的 `goto DISPATCH` 一致。
  }

  // §2 ⑤：分发循环——把这一帧该跑的指令一口气跑完。
  let guard = 0;
  for (;;) {
    // 见文件头“分发循环的护栏”：vm-core §9 点名要求的保护，原版没有、这里刻意加了。
    if (++guard > vm._maxInstructionsPerFrame) {
      throw new Error(
        `stepFrame: 单帧执行的指令数超过 ${vm._maxInstructionsPerFrame} 条上限，多半是脚本` +
          `发生了死循环（vm-core.md §9：原版对此没有任何保护，自跳脚本会真的卡死；` +
          `骨架加了这道护栏，代价见 core.js 文件头）。当前 vm.pc=${vm.pc}, vm.time=${vm.time}`,
      );
    }
    const fetch = fetchInstruction(vm);
    if (fetch === null) {
      terminateDeleted(vm);
      return; // §6.1：opcode -1 不跑帧尾
    }
    const { instr, size } = fetch;

    // §4 结论 / `0x45fbc2`：时间闸门，`<=` 才放行；没到点就整条循环退出去跑帧尾。
    if (instr.time > vm.time) break;

    switch (instr.opcode) {
      case 1: // delete（§6.1：-1 与 1 共用同一段代码，-1 在上面 fetch===null 分支处理过了）
        vm.flagsLo = (vm.flagsLo & ~1) >>> 0; // 05-system.md §2 表：delete 也清可见位
        terminateDeleted(vm);
        return; // 不跑帧尾
      case 2: // static
        vm.pc = -1; // alive 不变——vm 仍然“活着”，只是脚本冻结（§6.1）
        return; // 不跑帧尾
      case 4: // stopHide：先清可见位，再原样落进 `3 stop`
        // 05-system.md §8.1 一手（`th16:0x4629dc` `AND [EBX+0x530],0xFFFFFFFE`）：
        // 清 flags_lo bit0 后**直落** stop 主体。这是社区标 wip:2 的那条，已一手坐实。
        vm.flagsLo = (vm.flagsLo & ~1) >>> 0;
      // falls through
      case 3: { // stop
        // §2 case 3：进自锁前再查一次 pending_interrupt——被 stop 的 VM 仍可被 interrupt 唤醒。
        if (vm._pendingInterrupt !== 0) {
          const outcome = doInterrupt(vm);
          if (outcome === 'frame-done') return;
          continue; // 命中：接着在同一个分发循环里跑下去
        }
        // Timer_decrement(&vm.timer) 再走帧尾（帧尾会再 +speed，一减一加净变化 0，
        // pc 不动 -> 下一帧原地再执行同一条 stop，稳定自锁），见 vm-core §6.1。
        timerAdjust(vm, -vm._gameSpeed);
        frameTail(vm);
        return;
      }
      case 6: {
        // wait n：不是“设定目标”，是把时间往回拨 n（§2 case 6 / §4 结论）。
        // n 走 param_mask bit0：可以是字面量，也可以是变量（这条本身就是 resolveArg
        // 在骨架里的一个真实用例，不需要靠任何 opcode 组）。
        const isVar = isVarBit(instr.paramMask, 0);
        const n = resolveArg(vm, instr.args[0], isVar, 'int');
        timerAdjust(vm, -n);
        vm.pc += size;
        continue;
      }
      case 7: {
        // caseReturn：Timer::copy(vm.timer, saved_time) + instr_offset = saved_instr_offset
        // （§2 case 7 / §3.2「Timer::copy 的坑」——saved_time 在 doInterrupt 里存的时候
        // 就已经是“只留 .current”的截断值，这里直接当 Timer::set_value 的参数用）。
        timerSetValue(vm, vm._savedTime);
        vm.pc = vm._savedPc;
        continue;
      }
      default: {
        // opcode 0（nop）、5（interrupt 标签，正常执行时只是路过）、以及任何没有
        // 在 ops 里注册的号，统统落这条默认分支：只推进指针（§6.1 “0 nop”/§7 “洞”）。
        // 注册过的号（100+/200+/300+/…）交给对应组的 handler。
        const handler = ops[instr.opcode];
        if (handler) {
          const result = handler(vm, instr.args, instr);
          // handler 返回 PC_HANDLED 表示它已经自己改过 pc（跳转族），骨架不再 += size
          // ——对应 vm-core §1 “ADD size” vs “MOV 算出来的值” 那张表的两条路。
          if (result !== PC_HANDLED) vm.pc += size;
        } else {
          vm.pc += size;
        }
        continue;
      }
    }
  }

  frameTail(vm);
}

// ── 变量解析：engine/anm/th16/04-variables.md（下称「04」）§2 的 36 条变量表 ─────────
// 04 是本节的一手依据：36 个变量号（10000–10035）、22 个可写（7 整型 + 15 浮点）、
// 14 个只读，逐条对到 case 地址与 vm 字段落点（04 §1/§2）。math-jumps §3 只给了这套
// 变量机制的公共骨架（get_var 的 mask 分派、变量号按指令类型编码）；具体每个号是什么、
// 能不能写、落在哪，以 04 为准。

// 7 个可写整型号（走 get_int_var_ptr，04 §3 穷举）：I0–I3、I4、I5、rand-param-int。
const INT_WRITABLE = new Set([10000, 10001, 10002, 10003, 10008, 10009, 10029]);
// 15 个可写浮点号（走 get_float_var_ptr，04 §3 穷举）：F0–F3、pos、rot、两个随机刻度、
// 三个 mystery。
const FLOAT_WRITABLE = new Set([
  10004, 10005, 10006, 10007,
  10013, 10014, 10015,
  10023, 10024, 10025,
  10027, 10028,
  10033, 10034, 10035,
]);
// 14 个只读号（04 §3 negative set，穷举 + 与社区 mut 字段对账一致）。
const READONLY = new Set([
  10010, 10011, 10012,
  10016, 10017, 10018, 10019, 10020, 10021,
  10022, 10026,
  10030, 10031, 10032,
]);
// float→int 支持 cvttss2si（截零）转换的 9 个号（04 §4③穷举）——正是 7 个可写整型号
// 之外“有整型可读意义”的浮点号：F0–F3、两个随机刻度、三个 mystery。10022 单独走一条
// u32→模数的路（04 §5），不在这条转换规则里，虽然它也是整型号。
const CVTTSS2SI_IDS = new Set([10004, 10005, 10006, 10007, 10027, 10028, 10033, 10034, 10035]);
// int→float 支持 cvtdq2ps 转换的 7 个号（04 §4①）：恰好就是 7 个可写整型号——
// 10022 的 int→float 方向（浮读）单独走 u32→float 的路，不经过这条 cvtdq2ps 规则。
const CVTDQ2PS_IDS = INT_WRITABLE;
// 播放器不建模的引擎全局态：舞台相机位置/朝向镜像块（04 §6.1，🟡 结构 ✅ / 语义身份 🟡）。
// 只影响“浮点读”——整型读这 6 个号走的是下面“19 个非对称号”那条默认分支，根本不碰
// 相机全局，见 04 §4：get_int_var 对它们的 case 全部指向默认分支，返回变量号本身。
const UNMODELED_CAMERA_IDS = new Set([10016, 10017, 10018, 10019, 10020, 10021]);

// 36 个变量号的社区名（04 §2 表第 2 列），只用于报错信息更好认，不含任何判定逻辑。
const VAR_NAMES = {
  10000: 'i0', 10001: 'i1', 10002: 'i2', 10003: 'i3',
  10004: 'f0', 10005: 'f1', 10006: 'f2', 10007: 'f3',
  10008: 'i4', 10009: 'i5',
  10010: 'randrad', 10011: 'randf-01', 10012: 'randf-11',
  10013: 'pos-x', 10014: 'pos-y', 10015: 'pos-z',
  10016: 'camera-x', 10017: 'camera-y', 10018: 'camera-z',
  10019: 'lookat-x', 10020: 'lookat-y', 10021: 'lookat-z',
  10022: 'rand',
  10023: 'rot-x', 10024: 'rot-y', 10025: 'rot-z',
  10026: 'rot-z-effective',
  10027: 'rand-param-one', 10028: 'rand-param-pi', 10029: 'rand-param-int',
  10030: 'randrad-replay', 10031: 'randf-01-replay', 10032: 'randf-11-replay',
  10033: 'mystery-angle-x', 10034: 'mystery-angle-y', 10035: 'mystery-angle-z',
};
function varLabel(varId) {
  const name = VAR_NAMES[varId];
  return name ? `${varId}（${name}）` : `${varId}`;
}

function assertKnownVarId(caller, varId) {
  if (!Number.isInteger(varId) || varId < 10000 || varId > 10035) {
    throw new Error(
      `${caller}: 变量号 ${varId} 不是合法的 ANM 变量号（04-variables.md §2：定义域是 ` +
        '10000–10035 共 36 个号，越界之外没有变量）',
    );
  }
}

// ── 15 个浮点存储号 / 7 个整型存储号的直接字段落点（04 §2「vm 字段」列）──────────────
function getStoredFloat(vm, varId) {
  switch (varId) {
    case 10004: return vm.f[0];
    case 10005: return vm.f[1];
    case 10006: return vm.f[2];
    case 10007: return vm.f[3];
    case 10013: return vm.pos.x; // 04 §2 行 10013：`+0x2c`
    case 10014: return vm.pos.y;
    case 10015: return vm.pos.z;
    case 10023: return vm.rotation.x; // 04 §2 行 10023：`+0x38`
    case 10024: return vm.rotation.y;
    case 10025: return vm.rotation.z;
    case 10027: return vm.randParamOne;
    case 10028: return vm.randParamPi;
    case 10033: return vm.mysteryVars.x;
    case 10034: return vm.mysteryVars.y;
    case 10035: return vm.mysteryVars.z;
    default:
      throw new Error(`core.js 内部错误：getStoredFloat 收到不在 FLOAT_WRITABLE 里的 ${varId}`);
  }
}
function setStoredFloat(vm, varId, value) {
  const v = Math.fround(value);
  switch (varId) {
    case 10004: vm.f[0] = v; return;
    case 10005: vm.f[1] = v; return;
    case 10006: vm.f[2] = v; return;
    case 10007: vm.f[3] = v; return;
    case 10013: vm.pos.x = v; return; // ⚠️ 04 §3 备注：直接改字段，绕过 400 pos 的插值器
    case 10014: vm.pos.y = v; return;
    case 10015: vm.pos.z = v; return;
    case 10023: vm.rotation.x = v; return;
    case 10024: vm.rotation.y = v; return;
    case 10025: vm.rotation.z = v; return;
    case 10027: vm.randParamOne = v; return;
    case 10028: vm.randParamPi = v; return;
    case 10033: vm.mysteryVars.x = v; return;
    case 10034: vm.mysteryVars.y = v; return;
    case 10035: vm.mysteryVars.z = v; return;
    default:
      throw new Error(`core.js 内部错误：setStoredFloat 收到不在 FLOAT_WRITABLE 里的 ${varId}`);
  }
}
function getStoredInt(vm, varId) {
  switch (varId) {
    case 10000: return vm.i[0];
    case 10001: return vm.i[1];
    case 10002: return vm.i[2];
    case 10003: return vm.i[3];
    case 10008: return vm.i4;
    case 10009: return vm.i5;
    case 10029: return vm.randParamInt;
    default:
      throw new Error(`core.js 内部错误：getStoredInt 收到不在 INT_WRITABLE 里的 ${varId}`);
  }
}
function setStoredInt(vm, varId, value) {
  const v = value | 0;
  switch (varId) {
    case 10000: vm.i[0] = v; return;
    case 10001: vm.i[1] = v; return;
    case 10002: vm.i[2] = v; return;
    case 10003: vm.i[3] = v; return;
    case 10008: vm.i4 = v; return;
    case 10009: vm.i5 = v; return;
    case 10029: vm.randParamInt = v; return;
    default:
      throw new Error(`core.js 内部错误：setStoredInt 收到不在 INT_WRITABLE 里的 ${varId}`);
  }
}

// ── PRNG：两条独立流（04 §5，承 math-jumps §7.4）───────────────────────────────
// Stream B（`0x4a6d80`，表现流）：10010–10012、10022。group-math.js 的 122/123/131
// 已经在用 `vm.prng` 这个钩子名表示 Stream B（同一份「实现决策②」的约定，见该文件
// 头部）——这里复用同一个钩子，不新开一个名字，否则同一条物理 PRNG 流会被两个不同的
// 钩子名各自要求注入，调用方稍不注意就会喂出两条不同步的序列。
// Stream A（`0x4a6d88`，回放流）：10030–10032。这条流本组骨架/math-jumps 都没有任何
// opcode 用到过，是 04 §5 第一次要求播放器接的钩子，另开一个名字 `vm.prngReplay`，
// 与 `vm.prng` 严格不共用状态（04 §5：“两组是逐字节同公式的镜像”，但底层是两个独立
// 计数器/种子，混用会让两条本该独立的序列相互污染）。
// 两个钩子都是**必填**：没注入时对应变量号直接抛，不静默退化——与 group-math.js
// 「实现决策②」、本文件除零/相机的取舍同一套哲学（除零宁可抛、PRNG 缺失直接抛）。
function requirePrngRaw32(vm, hookName, varId) {
  const fn = vm[hookName];
  if (typeof fn !== 'function') {
    const streamLabel = hookName === 'prng' ? 'Stream B（0x4a6d80，表现流）' : 'Stream A（0x4a6d88，回放流）';
    throw new Error(
      `resolveArg: 变量 ${varLabel(varId)} 需要 PRNG，但 vm.${hookName} 没有注入（04-variables.md §5：` +
        `${streamLabel}）。两条流是独立状态，不能共用同一个钩子——Stream B 复用 group-math.js ` +
        '122/123/131 已经约定的 vm.prng（同一条物理流）；Stream A 是 10030–10032 专属的 ' +
        'vm.prngReplay，本模块不提供隐式退化。',
    );
  }
  return fn(vm) >>> 0;
}
// 04 §5 公式（承 math-jumps §2.2/§7.2）：raw·2⁻³² ∈ [0,1)；raw·2⁻³¹−1.0 ∈ [-1,1)。
// 与 group-math.js 的 prngUnit/prngSigned 是同一对纯数学公式——那边没有导出，这里
// 复制 2 行是刻意的（同本文件已有的 u32BitsToFloat32 复制先例），不是没看到那份实现。
function randF01(raw) {
  return raw / 0x100000000;
}
function randFpm1(raw) {
  return raw / 0x80000000 - 1;
}

// ── 04 §8.5 / 129 mathReduceAngle 同款角度归约：34 步上限，10026 专用 ───────────────
// 与 group-math.js 的 reduceAngle129（129 mathReduceAngle 的实现）是同一个算法
// （04 §6.2 原话：“wrap 进 [-π,π]…同 129 mathReduceAngle 的 34 步上限”）——group-math.js
// 是 core.js 的消费者，core.js 不能反过来 import 它，所以这里独立复制一份（本文件已有
// u32BitsToFloat32 的复制先例；这次复制的是“04 与 math-jumps 两篇文档都描述的同一个
// 引擎算法”，不是本文件自创的近似）。
const PI32 = Math.fround(Math.PI);
const TWO_PI32 = Math.fround(2 * Math.PI);
function wrapAngle34Step(x) {
  let k = 0;
  if (x > PI32) {
    do {
      const old = k;
      x = Math.fround(x - TWO_PI32);
      k++;
      if (old > 32) break;
    } while (x > PI32);
  }
  if (-PI32 > x) {
    do {
      const old = k;
      x = Math.fround(x + TWO_PI32);
      k++;
      if (old > 32) break;
    } while (-PI32 > x);
  }
  return x;
}

// ── 10010–10012/10030–10032：随机族浮点值（04 §5，只读，每次读都会消费一次 PRNG draw）──
function computeRandVarFloat(vm, varId) {
  switch (varId) {
    case 10010: return Math.fround(vm.randParamPi * randFpm1(requirePrngRaw32(vm, 'prng', varId)));
    case 10011: return Math.fround(vm.randParamOne * randF01(requirePrngRaw32(vm, 'prng', varId)));
    case 10012: return Math.fround(vm.randParamOne * randFpm1(requirePrngRaw32(vm, 'prng', varId)));
    case 10030: return Math.fround(vm.randParamPi * randFpm1(requirePrngRaw32(vm, 'prngReplay', varId)));
    case 10031: return Math.fround(vm.randParamOne * randF01(requirePrngRaw32(vm, 'prngReplay', varId)));
    case 10032: return Math.fround(vm.randParamOne * randFpm1(requirePrngRaw32(vm, 'prngReplay', varId)));
    default:
      throw new Error(`core.js 内部错误：computeRandVarFloat 收到不认识的 ${varId}`);
  }
}

// ── 10022（rand）：整读取模、浮读不取模——两条完全不同的公式（04 §5「10022 的两面」）──
function computeRand22Int(vm) {
  const mod = vm.randParamInt >>> 0;
  if (mod === 0) return 0; // 04 §5：模数为 0 短路成 0，与 group-math.js 122 的零守卫一致
  const raw = requirePrngRaw32(vm, 'prng', 10022);
  return (raw % mod) | 0; // 结果落回 s32 字段，可能环绕成负数（同 group-math.js 122）
}
function computeRand22Float(vm) {
  const raw = requirePrngRaw32(vm, 'prng', 10022); // 浮读同样消费一次 draw，不取模
  return Math.fround(raw); // raw 已经是 [0, 2^32) 的无符号数值，Math.fround = 04 §5 描述的
  // CVTDQ2PD→（按符号位加 2³²，此处 raw 恒非负所以这步等价于 no-op）→CVTPD2PS 全链路结果
}

// ── 10026（rot-z-effective）：04 §6.2，`0x464dd0` 递归累积祖先旋转再 wrap ─────────────
// 本骨架没有 VM 树（parent/root 指针）——Child management（opcode 500+）还没实现，见
// 本文件与 OVERVIEW.md 的“不建模”/“下一步”——所以无法构造出“有父”的 vm 实例：本骨架
// 里*每一个* vm 都等价于 04 §6.2 递归伪代码里 `root == null` 的那个分支。这不是“漏了一部分
// 逻辑的近似值”，是该分支下 100% 精确的实现——`root == null` 时递归直接跳过，只把自己
// 三个旋转分量 wrap 进 [-π,π]，取 z 分量返回，正是下面这一行做的事。等 500+ 组把父子关系
// 接进来之后，这里要补上“若 vm 存在父级则递归累加”那条分支，不在本任务范围。
function computeRotZEffective(vm) {
  return wrapAngle34Step(vm.rotation.z);
}

// u32 位模式 <-> float32，用来在“浮点指令把变量号编成 float32”（math-jumps §3 结论④）
// 这条规则下正确解码：算术运算 `raw | 0` 只对整数编码安全，浮点编码必须按位重解释。
const f32Scratch = new Float32Array(1);
const u32Scratch = new Uint32Array(f32Scratch.buffer);
function u32BitsToFloat32(u32) {
  u32Scratch[0] = u32 >>> 0;
  return f32Scratch[0];
}

// ── 读侧总分派：36 个号全覆盖（04 §1：get_int_var/get_float_var 的跳表都是 36 项）───
// ★ 跨类型读是非对称的（04 §4，一手穷举）：
//   - 7 个整型存储号（INT_WRITABLE）int→float 走 cvtdq2ps；10022 的 int→float 另有公式。
//   - 9 个号（CVTTSS2SI_IDS）float→int 走 cvttss2si（截零）。
//   - 其余 19 个“只有浮点语义”的号（10010–10021、10023–10026、10030–10032），整型读
//     不会真的去算字段值——get_int_var 的索引表把它们全指向默认分支，原样退回变量号
//     本身（04 §4③穷举）。这不是“没实现”，是真机的真实行为：用整型指令读 pos.x（10013）
//     拿到的不是坐标，是 10013 这个数。★ 这条也是本节的一个好消息：camera/lookat
//     （10016–10021）与 rot-z-effective（10026）的“不建模”只影响*浮点*读——整型读走
//     这条默认分支，根本不需要真的碰相机全局或祖先链。

function readIntVar(vm, varId) {
  assertKnownVarId('resolveArg', varId);
  if (INT_WRITABLE.has(varId)) return getStoredInt(vm, varId);
  if (varId === 10022) return computeRand22Int(vm);
  if (CVTTSS2SI_IDS.has(varId)) return Math.trunc(getStoredFloat(vm, varId)); // cvttss2si：截零
  return varId; // 19 个非对称号：默认分支原样退回变量号本身（04 §4）
}

function readFloatVar(vm, varId) {
  assertKnownVarId('resolveArg', varId);
  if (FLOAT_WRITABLE.has(varId)) return getStoredFloat(vm, varId);
  if (CVTDQ2PS_IDS.has(varId)) return Math.fround(getStoredInt(vm, varId)); // cvtdq2ps
  if (varId === 10022) return computeRand22Float(vm);
  if (varId === 10010 || varId === 10011 || varId === 10012 || varId === 10030 || varId === 10031 || varId === 10032) {
    return computeRandVarFloat(vm, varId);
  }
  if (varId === 10026) return computeRotZEffective(vm);
  if (UNMODELED_CAMERA_IDS.has(varId)) {
    throw new Error(
      `resolveArg: 变量 ${varLabel(varId)} 读的是播放器不建模的引擎全局态` +
        '（舞台相机位置/朝向镜像块，04-variables.md §6.1：只读、读全局，不落在 vm 字段里）——' +
        '本骨架没有 Stage 全局状态可读，故意不静默返回 0：返回错的坐标会让画面悄悄错位而无人' +
        '察觉，与本项目“除零宁可抛”“PRNG 缺失直接抛”同一哲学。想要真实相机数据，调用方需要' +
        '在骨架之外自己接一份 Stage 状态、拦截这几个变量号。',
    );
  }
  throw new Error(`core.js 内部错误：readFloatVar 的分支表没盖住 ${varId}（04 §2 应有 36 个号，逻辑有遗漏）`);
}

// ── 写侧总分派：22 个可写号，只读/类型不匹配一律带变量名报错 ──────────────────────
// 04 §3 穷举：INT_WRITABLE（7）与 FLOAT_WRITABLE（15）互不相交——写一个只在另一类型
// lvalue 表里的号，真机会落进默认分支，把入参指针原样退回（写进指令流本身，见
// math-jumps §9 坑表第一条“自修改字节码”）。本骨架不实现这条路径（ZUN 数据 0 处例外，
// math-jumps §8），遇到就抛“类型不匹配”错误，不静默改写 instr.args、也不静默丢弃。

/**
 * 写一个整型变量槽。只有 7 个整型可写号（04 §3）会真的落地；只读号与浮点专属号都抛错
 * （错误信息带变量名，不静默忽略——brief 步骤 1 明确要求）。
 * @param {ReturnType<typeof createVm>} vm
 * @param {number} varId
 * @param {number} value
 */
export function writeIntVar(vm, varId, value) {
  assertKnownVarId('writeIntVar', varId);
  if (INT_WRITABLE.has(varId)) {
    setStoredInt(vm, varId, value);
    return;
  }
  if (READONLY.has(varId)) {
    throw new Error(
      `writeIntVar: 变量 ${varLabel(varId)} 是只读变量，不能写入（04-variables.md §3：` +
        '22 可写 / 14 只读的 negative set 之一）',
    );
  }
  // 剩下的必然是 15 个浮点可写号之一：真机没有整型 lvalue，写会自修改字节码。
  throw new Error(
    `writeIntVar: 变量 ${varLabel(varId)} 只能通过浮点左值写入（04-variables.md §3：` +
      'get_int_var_ptr 的表里没有它），用整型指令写在真机上会把入参指针原样退回、改写指令流字节' +
      '（自修改字节码，math-jumps §9 坑表第一条），本骨架未实现这条路径，拒绝静默执行。',
  );
}

/**
 * 写一个浮点变量槽。只有 15 个浮点可写号（04 §3）会真的落地；只读号与整型专属号都抛错。
 * @param {ReturnType<typeof createVm>} vm
 * @param {number} varId
 * @param {number} value
 */
export function writeFloatVar(vm, varId, value) {
  assertKnownVarId('writeFloatVar', varId);
  if (FLOAT_WRITABLE.has(varId)) {
    setStoredFloat(vm, varId, value);
    return;
  }
  if (READONLY.has(varId)) {
    throw new Error(
      `writeFloatVar: 变量 ${varLabel(varId)} 是只读变量，不能写入（04-variables.md §3：` +
        '22 可写 / 14 只读的 negative set 之一）',
    );
  }
  // 剩下的必然是 7 个整型可写号之一：真机没有浮点 lvalue，写会自修改字节码。
  throw new Error(
    `writeFloatVar: 变量 ${varLabel(varId)} 只能通过整型左值写入（04-variables.md §3：` +
      'get_float_var_ptr 的表里没有它），用浮点指令写在真机上会把入参指针原样退回、改写指令流字节' +
      '（自修改字节码，math-jumps §9 坑表第一条），本骨架未实现这条路径，拒绝静默执行。',
  );
}

/**
 * 变量解析：解析一个可能带 param_mask 的指令参数。
 *
 * 依据 math-jumps.md §1.1 的公共骨架（`v = mask位 ? get_var(vm, raw) : raw`）与
 * §3 结论④（变量号的编码宽度跟指令类型走：整型指令把变量号编成 int32，浮点指令编成
 * float32——即使变量号本身一样，两组指令里代表它的 4 个字节完全不同）。
 *
 * @param {ReturnType<typeof createVm>} vm
 * @param {number} raw    01-format.md §5.1 的原始 u32 参数值（未经解释的那 4 个字节）
 * @param {boolean} isVariable   这个参数位在 param_mask 里的对应 bit 是否置位（通常来自
 *   isVarBit——它对 bit 9–15 同样能算，但 01-format.md §5.2 说这些高位从没被验证过，
 *   调用方引用 9 个以上参数的 opcode 时留意这条）
 * @param {'int' | 'float'} kind   调用方（某条具体 opcode 的语义）知道这个参数该按整型
 *   还是浮点读——骨架本身不含这个知识，所以要求调用方明说
 * @returns {number}
 */
export function resolveArg(vm, raw, isVariable, kind) {
  if (kind === 'float') {
    if (!isVariable) return u32BitsToFloat32(raw);
    // helper 第一件事是 cvttss2si（截零），math-jumps §3：即使调用方直接传入十进制的
    // 变量号（比如测试里写 10004）而不是“10004 的 float32 位模式”，Math.trunc 在这里
    // 也是安全的 no-op——但正确用法应该总是传真实的 float32 位模式，见下面 isVarBit
    // 旁边的调用惯例说明。
    const varId = Math.trunc(u32BitsToFloat32(raw));
    return readFloatVar(vm, varId);
  }
  if (kind !== 'int') {
    throw new Error(`resolveArg: 未知的 kind ${JSON.stringify(kind)}，只接受 'int' 或 'float'`);
  }
  if (!isVariable) return raw | 0; // u32 位模式当 s32 重解释
  return readIntVar(vm, raw | 0);
}

/**
 * resolveArg 的配套小工具：判断 param_mask 的第 index 位是否置位。
 * 01-format.md §5.1：bit i = 第 i 个参数是变量 id。纯位运算，对任意 index 都对；
 * 但 01-format.md §5.2 的可信度分层要带过来：真实数据只验证过 bit 0–8（54 个 th16
 * anm 里最高只用到 bit 8），**bit 9–15 ❓ 从没被验证过**——不代表它们是保留位，只是
 * 没有证据；调用方（尤其是要写 9 个以上参数的 opcode，如果真的存在）读高位时心里要有
 * 这根弦，这条 isVarBit 本身不会替你做这个判断。
 * @param {number} paramMask
 * @param {number} index
 */
export function isVarBit(paramMask, index) {
  return (paramMask & (1 << index)) !== 0;
}
