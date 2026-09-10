// 层 B · 最小宿主 —— 骨架**故意**没做的那一层（AnmManager）。
//
// core.js 只管一个 VM 跑一帧；真机的 `AnmManager` 还要管：VM 列表、layer 排序、
// 子 VM 的登记与回收、以及「script/sprite 的全局编号 → 具体是哪个 entry 的哪条记录」。
// 这个文件把这些补上，**只用已经一手确认的规则**，每条不确定的都显式标出来。
//
// 依据：01-format.md（下称「01」）§4/§5/§8、02-vm-core.md §3.5、08-child.md §5、
//       06-settings.md §2.1、09-drawing.md、10-sprite-geometry.md。
import { createVm, stepFrame, registerGroup, resolveArg, isVarBit } from '../vm/core.js';
import { mathJumpsGroup } from '../vm/group-math.js';
import { systemGroup } from '../vm/group-system.js';
import { settingsGroup } from '../vm/group-settings.js';
import { generalGroup, stepFrameTail, ensureGeneralState } from '../vm/group-general.js';
import { childGroup, ensureChildState, destroyTree, getRootVm } from '../vm/group-child.js';
import { drawingGroup } from '../vm/group-drawing.js';
import { buildGeometry, renderModeOf } from '../render/geometry.js';
import { decodeThtx } from '../format/thtx.js';

/** 六组合成一张 ops 表（层 A 的 135 条全在里面）。 */
export function createOps() {
  const ops = {};
  registerGroup(ops, mathJumpsGroup);
  registerGroup(ops, systemGroup);
  registerGroup(ops, settingsGroup);
  registerGroup(ops, generalGroup);
  registerGroup(ops, childGroup);
  registerGroup(ops, drawingGroup);
  return ops;
}

/**
 * 把 `parseAnm` 的输出摊平成宿主要用的形状。
 *
 * ★ **script 与 sprite 都是「跨 entry 的运行序号」**（01 §4/§5，一手 ✅）：
 * 加载器把它们按解析顺序塞进 `AnmLoaded+0x120` / `+0x11c` 两个全局数组，下标是累加的运行
 * 序号，**从不读记录里那个 `id` 字段**。sprite 的 `id` 在 ZUN 的文件里恰好与序号一致
 * （3101 个 0 处不符），但 script 的 `id` **会真的对不上**（e01–e08 的 id 是 −72…−1）。
 * 所以这里一律按位置摊平，`id` 只留着显示用。
 *
 * @param {Uint8Array} bytes 整个 `.anm` 文件
 * @param {{ entries: Array }} parsed `parseAnm(bytes)` 的结果
 */
export function flattenFile(bytes, parsed, opts = {}) {
  const entries = parsed.entries;
  const textures = entries.map((e) => (e.thtxOffset === null ? null : decodeThtx(bytes, e.thtxOffset, opts)));
  const sprites = [];
  const scripts = [];
  entries.forEach((entry, entryIndex) => {
    const tex = textures[entryIndex];
    for (const sp of entry.sprites) {
      // UV 四角，角序与顶点一致 = TL, TR, BL, BR（10 §2）。
      // 贴图尺寸用 **THTX 自己的 w/h**；entry header 里那对 width/height 是目标面的尺寸，
      // 两者在真实数据里可以不同（01 §3/§6）。没有贴图的 entry 退回 entry 的尺寸。
      const tw = tex ? tex.width : entry.width;
      const th = tex ? tex.height : entry.height;
      const u0 = sp.x / tw;
      const v0 = sp.y / th;
      const u1 = (sp.x + sp.w) / tw;
      const v1 = (sp.y + sp.h) / th;
      sprites.push({ ...sp, entryIndex, uv: [[u0, v0], [u1, v0], [u0, v1], [u1, v1]] });
    }
    entry.scripts.forEach((script, i) => {
      scripts.push({ script, entryIndex, indexInEntry: i, id: script.id });
    });
  });
  return { entries, textures, sprites, scripts };
}

// `flags_lo` bit0 = 可见位（05-system.md §8.1：`1 delete` 与 `4 stopHide` 都清它）。
const VISIBLE_BIT = 0x1;

/**
 * @param {ReturnType<typeof flattenFile>} file
 * @param {{ gameSpeed?: number, resolutionRatio?: number }} [opts]
 */
export function createManager(file, opts = {}) {
  const baseOps = createOps();
  const ops = { ...baseOps };
  const vms = [];
  const skipped = [];

  // ── `set_sprite` 的宿主侧落地 ──────────────────────────────────────────────────
  // 真机 `300 sprite` 会调 `set_sprite th16:0x45edb0`，**当场**把贴图的像素尺寸写进
  // `sprite_size`（`+0x70/+0x74`，`th16:0x45ee36`）并算好 UV 四角（`+0x3a8..`）。
  // group-settings.js 只记了 `vm.spriteId`——它没有贴图表，记不了别的。这里补上。
  //
  // **必须在 opcode 里做、不能挪到帧尾**：同一帧里 `300 sprite` 之后可能跟着
  // `603 drawRect` 改写同一对 `sprite_size` 槽（09 §5(2)：两者共用），帧尾统一刷会把它冲掉。
  function applySprite(vm, id) {
    if (id < 0) {
      // 06 §2.1（★ 已更正为热路径）：真机换成 AsciiManager 的 `ascii.anm` sprite `0x102`。
      // 独立播放器没有加载 ascii.anm，**没有可换的贴图** ⇒ 本宿主按 thpages 的说法
      // 「不使用图像」处理：记下来、这一帧不画它。真实数据里 4683 条 `300` 有 80 条是 -1
      // （全部恰好 -1），不是理论分支。这是一处**有意偏离真机**，显式记在 vm 上。
      vm._host = vm._host ?? {};
      vm._host.spriteRef = null;
      vm._host.systemSprite = true;
      return;
    }
    const sp = file.sprites[id];
    vm._host = vm._host ?? {};
    vm._host.systemSprite = false;
    if (!sp) {
      // 越界：真机会读到 `+0x11c` 数组外的垃圾。播放器记下来并跳过，不编一个。
      vm._host.spriteRef = null;
      vm._host.badSprite = id;
      return;
    }
    vm._host.spriteRef = sp;
    vm.spriteSize.x = sp.w; // `th16:0x45ee36`
    vm.spriteSize.y = sp.h;
  }

  ops[300] = (vm, args, instr) => {
    const id = resolveArg(vm, instr.args[0], isVarBit(instr.paramMask, 0), 'int');
    if (id < 0) {
      // 不走 settingsGroup[300]——它对负数是显式抛（那是「组不建模系统贴图」的诚实表态）。
      // 宿主有自己的策略（见 applySprite），所以在这里拦下。可见位仍要照置（06 §2.1）。
      vm.flagsLo = (vm.flagsLo | VISIBLE_BIT) >>> 0;
      vm.spriteId = id;
      vm.spriteSelectTime = vm.time;
      applySprite(vm, id);
      return;
    }
    const r = baseOps[300](vm, args, instr);
    applySprite(vm, vm.spriteId);
    return r;
  };
  ops[301] = (vm, args, instr) => {
    const r = baseOps[301](vm, args, instr);
    applySprite(vm, vm.spriteId);
    return r;
  };

  function register(vm) {
    if (vms.includes(vm)) return vm;
    vms.push(vm);
    return vm;
  }

  /** 建一个顶层 VM 跑指定的全局 script 序号。 */
  function spawn(scriptIndex) {
    const rec = file.scripts[scriptIndex];
    if (!rec) throw new Error(`manager.spawn: 没有全局序号 ${scriptIndex} 的 script（共 ${file.scripts.length} 条）`);
    const vm = createVm(rec.script, {
      gameSpeed: opts.gameSpeed ?? 1,
      frameTail: stepFrameTail,
      getRootVm,
    });
    ensureGeneralState(vm);
    const cs = ensureChildState(vm);
    // 08 §5：`500–506` 要靠这两张表才能建子——脚本表是文件级的，ops 表是「创建即 tick」用的。
    cs.scripts = file.scripts.map((s) => s.script);
    cs.ops = ops;
    vm._host = { spriteRef: null, systemSprite: false, scriptIndex };
    return register(vm);
  }

  /** 推进一帧：逐 VM `stepFrame` → 收编新生的子 VM → 回收死掉的。 */
  function step() {
    // 快照：这一帧新建的子 VM 下一帧才参与（真机也是插进列表后由下一轮遍历带上，
    // 且它们在创建时已经被 `create_managed_*` 预跑过一帧——08 §2「创建即 tick」）。
    const snapshot = vms.slice();
    for (const vm of snapshot) {
      if (!vm.alive) continue;
      stepFrame(vm, ops);
    }
    // 收编：`500–506` 的 handler 会把新 VM 挂进父的子链，但 stepFrame 不把返回值交出来，
    // 所以走子链发现它们（08 §5 的 prepend）。
    for (const vm of vms.slice()) collectChildren(vm);
    // 回收：`th16:0x46e660` 是「子先于己」的递归销毁。destroyTree 已经复刻了那套。
    for (const vm of vms.slice()) {
      if (!vm.alive) destroyTree(vm);
    }
    for (let i = vms.length - 1; i >= 0; i--) {
      if (!vms[i].alive) vms.splice(i, 1);
    }
  }

  function collectChildren(vm) {
    const st = vm._child;
    if (!st) return;
    for (const child of st.children) {
      if (!vms.includes(child)) {
        child._host = child._host ?? { spriteRef: null, systemSprite: false };
        vms.push(child);
      }
      collectChildren(child);
    }
  }

  /**
   * 这一帧要画什么。**按 layer 升序稳定排序**（`304 layer` 写 `vm.layer`，02 §3.5）。
   * 未实现的渲染模式**不抛**，收进 `skipped` 让调用方显示——播放器要能放真实文件，
   * 一条未实现的 mode 不该炸掉整帧；但也不能静默，所以逐条记原因。
   */
  function drawList() {
    skipped.length = 0;
    const out = [];
    const ordered = vms
      .map((vm, i) => ({ vm, i }))
      .sort((a, b) => (a.vm.layer ?? 0) - (b.vm.layer ?? 0) || a.i - b.i)
      .map((x) => x.vm);
    for (const vm of ordered) {
      if (!vm.alive) continue;
      if ((vm.flagsLo & VISIBLE_BIT) === 0) continue; // 可见位没置，真机也不画
      const host = vm._host ?? {};
      if (!host.spriteRef) {
        skipped.push({ vm, reason: host.systemSprite ? 'sprite(-1)：真机换系统贴图，本宿主不加载 ascii.anm' : '还没选过 sprite' });
        continue;
      }
      let geo;
      try {
        geo = buildGeometry(vm, { spriteUv: host.spriteRef.uv, resolutionRatio: opts.resolutionRatio ?? 1 });
      } catch (err) {
        skipped.push({ vm, mode: renderModeOf(vm), reason: err.message });
        continue;
      }
      out.push({ vm, geometry: geo, entryIndex: host.spriteRef.entryIndex, texture: file.textures[host.spriteRef.entryIndex] });
    }
    return out;
  }

  return {
    ops,
    vms,
    skipped,
    spawn,
    step,
    drawList,
    get aliveCount() {
      return vms.filter((v) => v.alive).length;
    },
  };
}
