// ANM v8 VM ——System 组（opcode 0–7，8 条）：生死 + 让出 + 标签控制层。
//
// 依据：engine/anm/th16/05-system.md（下称「05」）§2 的 8 条指令表；每条 handler 上方
// 注释指回具体 opcode 行（双向互指纪律，见 runtime/README.md）。控制流的权威在
// engine/anm/th16/02-vm-core.md（下称「02」）§6「终止/让出语义（五个分支）」——05 §4/§8.2
// 与它重复的部分已声明以 02 为准，本文件同样以 02 为准，不重新论证任何语义结论。
//
// ★★★ 本组与骨架（src/vm/core.js）高度重叠，切分依据写在这里 ★★★
//
// 读一遍 core.js 的 `stepFrame` 就会发现：它的分发循环对 opcode **1/2/3/4/6/7** 是硬编码
// 的 `switch` 分支，在触达 `ops[instr.opcode]` 查表之前就已经 `return`/`continue` 掉了
// （见 core.js 的 `switch (instr.opcode) { case 1: ...; case 2: ...; case 3: case 4: {...};
// case 6: {...}; case 7: {...}; default: { const handler = ops[instr.opcode]; ... } }`）。
// 这意味着：**这 6 个 opcode 号无论在 ops 表里挂什么 handler，stepFrame 正常运行下都不会
// 调用它**——不是"骨架已经做了大部分，组文件补一层薄封装做剩下的事再转发调用"，是骨架
// 做完之后**这 6 个 handler 在正常路径下永远够不到**。
//
// 只有 opcode **0（nop）** 与 **5（case）** 会真正落进 switch 的 `default` 分支、经
// `ops[instr.opcode]` 被调用——而这两条的语义本来就是"什么都不做，只推进指针"
// （05 §2 表：0 nop 只 `instr_offset += size`；5 case 运行时是 no-op，语义全在 interrupt
// 扫描里，那段扫描——core.js 的 `doInterrupt`——直接遍历 `vm._instructions`，根本不经过
// ops 表，见 02 §5/05 §5）。`default` 分支查不到 handler 时本来就是 `vm.pc += size`，
// 所以**就算完全不注册 0/5，行为也已经正确**。
//
// 权衡下来的取舍：**8 个 opcode 全部注册进 systemGroup**，但性质分两种：
//
//   - **0/5：真 no-op handler**（返回 undefined，等价于 core.js 默认分支本来就会做的事）。
//     注册它不新增任何行为，纯粹是让"这两个号被 System 组认领"在代码里显式可查、可 grep。
//   - **1/2/3/4/6/7：canary handler**——调用即抛错。它们不是"语义在 core.js，本 handler
//     只做 X"那种会被调用的薄封装，而是**按设计永远不会被调用**的哨兵：如果它们真的被
//     调用了，说明 core.js 的 switch 被改动过（不再拦截这个 opcode），或者有代码绕开
//     `stepFrame` 直接调了 `ops[opcode]`——两种情况都值得在第一时间炸出来，而不是让一个
//     "看起来实现了、其实从没跑过"的 handler 安静地维持一个假象。
//
// 为什么不干脆只注册 0/5、完全不提 1/2/3/4/6/7？两个理由：
//   1. `registerGroup`（core.js）靠"同一个 opcode 号被两个组认领"来查重——System 组不
//      声明 1/2/3/4/6/7 的话，将来某个新组若手滑把这几个号也实现了一遍（例如把"终止"
//      误判成自己范围内的事），不会有任何报错提示"这其实是骨架的活"，一份影子实现会
//      静默混进另一个组的表里。canary 挂在这里，这条防线才完整。
//   2. `05-system.md` §2 的表格是 8 条、连号 0–7；本文件的 `systemGroup` 也应该是 8 个
//      key、连号 0–7——读者对着文档表核对"这条实现在哪"时，不需要先猜一遍"是不是被骨架
//      吃掉了、组文件里干脆没写这一项"。
//
// 不需要 `resolveArg`/`writeIntVar`/`isVarBit`/`PC_HANDLED` 这些 core.js 的导出——8 条里
// 唯一带参数语义的是 `6 wait`，而它的参数解析（含 param_mask 变量判定）已经在 core.js 的
// `case 6` 里做完了，本文件不重复。

/**
 * 生成一个"canary" handler：调用即抛错，用来标注"这个 opcode 的语义在 core.js 的
 * stepFrame switch 里，正常运行路径下不会走到这里"。
 * @param {number} opcode
 * @param {string} mnemonic
 * @param {string} coreRef 指回 core.js 具体实现位置 + engine 文档小节
 */
function unreachableCoreHandled(opcode, mnemonic, coreRef) {
  return () => {
    throw new Error(
      `group-system: opcode ${opcode}（${mnemonic}）的语义已经在 core.js 的 stepFrame switch 里` +
        `完整实现（${coreRef}），正常路径下这个 handler 不会被调用。如果你看到这个错误，说明 ` +
        'core.js 的分发逻辑被改动过（不再拦截这个 opcode），或者有代码绕开 stepFrame 直接调用了 ' +
        'ops[opcode]——回头核实 core.js 的 switch 是否还在处理这个 opcode。',
    );
  };
}

export const systemGroup = {
  // 05-system.md #0 —— nop：真 no-op。core.js 默认分支查不到 handler 时本来就是
  // `vm.pc += size`，这里注册一个空函数只是让"System 组认领了 opcode 0"显式可查，
  // 不改变任何行为。
  0: () => {},

  // 05-system.md #1 —— delete：语义在 core.js `stepFrame` 的 `case 1`
  // （`terminateDeleted`：`pc = -1`、`alive = false`），02-vm-core.md §6.1。
  1: unreachableCoreHandled(1, 'delete', 'core.js stepFrame case 1（terminateDeleted）/ 02-vm-core.md §6.1'),

  // 05-system.md #2 —— static：语义在 core.js `stepFrame` 的 `case 2`
  // （`pc = -1`，`alive` **不**变——这是它与 delete 的关键差异），02-vm-core.md §6.1。
  2: unreachableCoreHandled(2, 'static', 'core.js stepFrame case 2 / 02-vm-core.md §6.1'),

  // 05-system.md #3 —— stop：语义在 core.js `stepFrame` 的 `case 3`/`case 4` 共用块
  // （进自锁前再查一次 `_pendingInterrupt`，命中就唤醒；否则 `Timer::decrement` 后跑
  // 完整帧尾，净时间变化为 0，`pc` 不动——下一帧原地重执行同一条 stop），
  // 02-vm-core.md §6.1/§4。
  3: unreachableCoreHandled(3, 'stop', 'core.js stepFrame case 3/4 / 02-vm-core.md §6.1 结论 3/4、§4'),

  // 05-system.md #4 —— stopHide：一手结论是"清可见位 + 落进 3 stop"（05 §8.1），但
  // core.js 文件头「不建模」清单明确写了 flags_lo/flags_hi（含可见位）不在骨架的 `vm`
  // 契约里，所以 `case 3`/`case 4` 在 core.js 里是**同一个 switch 分支**——本骨架里
  // stopHide 与 stop 行为完全相同（差的那个可见位骨架压根没有字段可清）。
  4: unreachableCoreHandled(4, 'stopHide', 'core.js stepFrame case 3/4（与 stop 共用，可见位不建模）/ 02-vm-core.md §6.1'),

  // 05-system.md #5 —— case：运行时是纯 no-op（`instr_offset += size`），语义全在
  // interrupt 扫描里（02-vm-core.md §5 的 `doInterrupt`：直接线性扫 `vm._instructions`
  // 找 `opcode === 5 && args[0] === want`，全程不经过 `ops` 表）。这里注册的 no-op 只
  // 覆盖"正常顺序执行路过一条 case 标签"这一条路径——与 opcode 0 一样，注册与否行为
  // 一致，只是显式认领。
  5: () => {},

  // 05-system.md #6 —— wait：语义在 core.js `stepFrame` 的 `case 6`
  // （`timerAdjust(vm, -n)`；`n` 走 `isVarBit` + `resolveArg('int')`，可为字面量也可为
  // 变量），05 §6 / 02-vm-core.md §2 case 6。
  6: unreachableCoreHandled(6, 'wait', 'core.js stepFrame case 6（timerAdjust(vm, -n)）/ 05-system.md §6'),

  // 05-system.md #7 —— caseReturn：语义在 core.js `stepFrame` 的 `case 7`
  // （`timerSetValue(vm, vm._savedTime)` + `pc = vm._savedPc`，`continue` 回到分发循环
  // 顶部——同帧继续，不是让给帧尾），05 §7 / 02-vm-core.md §6。
  7: unreachableCoreHandled(7, 'caseReturn', 'core.js stepFrame case 7（timerSetValue + pc=_savedPc, continue）/ 05-system.md §7'),
};
