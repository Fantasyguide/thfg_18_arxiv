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
//   resolveArg(vm, raw, isVariable, kind) -> number   （变量解析，供各 opcode 组用）
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
export function createVm(script, opts = {}) {
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
        terminateDeleted(vm);
        return; // 不跑帧尾
      case 2: // static
        vm.pc = -1; // alive 不变——vm 仍然“活着”，只是脚本冻结（§6.1）
        return; // 不跑帧尾
      case 3: // stop
      case 4: {
        // stopHide：骨架不建模可见性 flag，与 3 走同一套逻辑（见文件头“不建模”清单）。
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

// ── 变量解析：math-jumps §3 结论④ ────────────────────────────────────────────
// I0–I3 = 变量号 10000–10003（vm.i[0..3]），F0–F3 = 10004–10007（vm.f[0..3]）。
// vm-core §3.4 的完整变量表还有 10008/10009、pos/rotation 分量、10022 随机、
// 10027–10029 常量……本骨架只实现 I0–I3/F0–F3 这 8 个（brief 明确只要这些）——
// 其余变量号留给要用到它们的 opcode 组（比如要读 pos.x 的 General 组）自己按需扩展
// 这张表，不在 I2 骨架范围内。
//
// ★ 跨类型读（读取值，不是解析哪个变量号——见下面 resolveArg 的两段式说明）：
// math-jumps §3 明确 `get_int_var`/`get_float_var` 的定义域**都是 10000–10035**，
// 不是各自只认自己那 4 个槽——`get_int_var(10004..10007)` 走 `cvttss2si`（读浮点槽转
// 整型，截零），`get_float_var(10000..10003)` 走 `cvtdq2ps`（读整型槽转浮点）。真实数据
// 里这不是理论边界：827 个整型变量参数里有 9 次是 opcode 100/112 用整型指令去读 F0
// （math-jumps §3 结论④：“变量号的编码取决于指令的类型，不取决于变量本身”——即使引用的
// 是 F0，只要宿主指令是整型指令，那 4 个字节就按 int32 编码）。所以 I0–I3/F0–F3 这 8 个
// 槽内部要能跨类型读，这不是“未实现的变量号”（10008/10022 那类才是），下面两个函数
// 分别处理各自的两种情况：本类型直接读，另一类型转换后读。10008 及以上仍然抛，但
// 措辞改成“I2 骨架尚未覆盖”而不是笼统的“越界”——这两者不是一回事。

function readIntVar(vm, varId) {
  if (varId >= 10000 && varId <= 10003) return vm.i[varId - 10000];
  if (varId >= 10004 && varId <= 10007) return Math.trunc(vm.f[varId - 10004]); // cvttss2si：截零，不是 floor
  throw new Error(
    `resolveArg: 变量号 ${varId} 是 I2 骨架尚未覆盖的变量号` +
      '（02-vm-core.md §3.4 还有更多变量号——10008/10009、pos/rotation 分量、随机数……' +
      '留给需要它们的 opcode 组扩展；I0–I3/F0–F3 这 8 个槽本身已经支持跨类型读）',
  );
}

function readFloatVar(vm, varId) {
  if (varId >= 10004 && varId <= 10007) return vm.f[varId - 10004];
  if (varId >= 10000 && varId <= 10003) return Math.fround(vm.i[varId - 10000]); // cvtdq2ps：整型转 float32 精度
  throw new Error(
    `resolveArg: 变量号 ${varId} 是 I2 骨架尚未覆盖的变量号` +
      '（02-vm-core.md §3.4 还有更多变量号——10008/10009、pos/rotation 分量、随机数……' +
      '留给需要它们的 opcode 组扩展；I0–I3/F0–F3 这 8 个槽本身已经支持跨类型读）',
  );
}

// u32 位模式 <-> float32，用来在“浮点指令把变量号编成 float32”（math-jumps §3 结论④）
// 这条规则下正确解码：算术运算 `raw | 0` 只对整数编码安全，浮点编码必须按位重解释。
const f32Scratch = new Float32Array(1);
const u32Scratch = new Uint32Array(f32Scratch.buffer);
function u32BitsToFloat32(u32) {
  u32Scratch[0] = u32 >>> 0;
  return f32Scratch[0];
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
