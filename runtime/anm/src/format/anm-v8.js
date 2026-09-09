// ANM v8 文件格式解析器（零依赖，纯结构解析，不含任何运行时/VM 语义）。
//
// 依据：engine/anm/th16/01-format.md（TH16 一手确认的 v8 字段表）。本文件每个偏移常量
// 都在注释里指回该文档的对应小节（双向互指纪律，见 runtime/README.md）。本文件本身
// 不重新论证格式为什么是这样——那是 engine/ 层的事。
//
// 接口：parseAnm(bytes: Uint8Array) -> { entries: [{ name, width, height, format, sprites, scripts }] }
//   sprites: [{ id, x, y, w, h }]
//   scripts: [{ id, instructions: [{ time, opcode, args }] }]
//
// 参数必须是 Uint8Array（Node 的 Buffer 是其子类），不是 ArrayBuffer——用
// `readFileSync(path)` 直接传，不要 `.buffer`：Node 对小文件返回的 Buffer 来自共享
// 内存池，`.buffer` 拿到的是整个池（还带 byteOffset），不是文件字节。

const ENTRY_HEADER_SIZE = 0x40; // 01-format.md §3：header 固定 0x40 字节
const SCRIPT_TABLE_ENTRY_SIZE = 8; // 01-format.md §5：script 表每项 8 字节
const INSTR_HEADER_SIZE = 8; // 01-format.md §5.1：指令头 8 字节
const OPCODE_END = -1; // 01-format.md §5.1：opcode = -1（0xffff）= 脚本结束

// ★ 消费方契约（I2 VM 骨架直接依赖）：sprite.id / script.id 是文件里存的原始值，
// 不是位置序号（01-format.md §4/§5：加载器不读这两个字段，真实文件里可以和位置序号
// 对不上，script id 甚至可以是负数）。想要 VM 用的跨 entry 累加运行时索引，用
// `entries.flatMap(e => e.scripts)`/`entries.flatMap(e => e.sprites)` 的**数组下标**，
// 数组顺序就是文件顺序，天然等于游戏 VM 用的序号——不要用 `id`，也不要用 entry 内的
// 局部下标。

/**
 * @param {Uint8Array} bytes
 * @returns {{ entries: Array<{ name: string, width: number, height: number,
 *   format: number, sprites: Array<{ id: number, x: number, y: number, w: number, h: number }>,
 *   scripts: Array<{ id: number, instructions: Array<{ time: number, opcode: number, args: number[] }> }>
 * }> }}
 */
export function parseAnm(bytes) {
  // 别直接 `new DataView(bytes.buffer)`：那会忽略 byteOffset/byteLength，
  // 在 bytes 是某个更大 ArrayBuffer 的切片（例如 Node Buffer 池）时读到邻居的数据。
  const view = new DataView(bytes.buffer, bytes.byteOffset, bytes.byteLength);

  const entries = [];
  let entryBase = 0;

  // 01-format.md §2：文件 = entry 单向链表，next_offset 相对*本 entry 起始*，0 结束。
  for (;;) {
    let entry;
    try {
      entry = parseEntry(view, bytes, entryBase);
    } catch (err) {
      // 给冒出来的错误（不管是本文件里显式 throw 的还是 DataView 原生 RangeError）
      // 补上「这是文件里哪个 entry 出的事」，不然只有一串没有文件坐标的报错。
      throw new Error(`anm-v8: failed to parse entry at byte offset ${entryBase}: ${err.message}`);
    }
    entries.push(entry.value);

    const nextOffset = entry.nextOffset;
    if (nextOffset === 0) break;
    entryBase += nextOffset;
  }

  return { entries };
}

function parseEntry(view, bytes, base) {
  // 01-format.md §3：entry header（0x40 字节）字段表。
  const version = view.getUint32(base + 0x00, true);
  if (version !== 8) {
    // 01-format.md §3 / §0：TH16 的加载器只接受 version === 8，没有旧版本分支。
    // 这是纯结构解析器，不代表游戏行为——遇到非 8 直接报错比静默按 v8 布局瞎读更安全。
    throw new Error(`anm-v8: unsupported entry version ${version} at byte offset ${base} (expected 8)`);
  }
  const nSprites = view.getUint16(base + 0x04, true);
  const nScripts = view.getUint16(base + 0x06, true);
  // +0x08 u16：加载路径不读，🟡（见 01-format.md §3.1），跳过不解析。
  const width = view.getUint16(base + 0x0a, true);
  const height = view.getUint16(base + 0x0c, true);
  const format = view.getUint16(base + 0x0e, true);
  const nameOffset = view.getUint32(base + 0x10, true);
  // +0x14/+0x16 x_offset/y_offset、+0x18 memory_priority：不影响格式层输出字段，跳过。
  const thtxOffset = view.getUint32(base + 0x1c, true);
  const hasData = view.getUint16(base + 0x20, true);
  // +0x22 low_res_scale、+0x23 保留、+0x28..0x40 保留：同上，跳过。
  const nextOffset = view.getUint32(base + 0x24, true);

  const name = readCString(bytes, base + nameOffset);

  const sprites = parseSprites(view, base, nSprites);
  const scripts = parseScripts(view, base, nSprites, nScripts, view.byteLength);

  // THTX 块本身（贴图像素）不在 parseAnm 的输出契约里（interfaces 只要 name/width/
  // height/format/sprites/scripts），这里只按 01-format.md §6 校验偏移在场时形状合法，
  // 不吐出像素数据。hasData === 0 时 thtxOffset 恒为 0，不校验。
  if (hasData !== 0) {
    const magic = readFixedString(bytes, base + thtxOffset, 4);
    if (magic !== 'THTX') {
      // 01-format.md §6：magic 是 🟡（加载器不校验），但真实文件 417/417 都是 "THTX"；
      // 不吻合大概率是我们哪个偏移算错了，值得报错而不是悄悄吞掉。
      throw new Error(`anm-v8: expected THTX magic at entry+0x${thtxOffset.toString(16)}, got ${JSON.stringify(magic)}`);
    }
  }

  return {
    nextOffset,
    value: { name, width, height, format, sprites, scripts },
  };
}

function parseSprites(view, entryBase, nSprites) {
  // 01-format.md §4：偏移表在 entry+0x40 起，n_sprites 个 u32，每个是 sprite 记录
  // 相对 entry 的偏移（不是记录本身连续排列——要老实按偏移表跳转）。
  const sprites = [];
  for (let i = 0; i < nSprites; i++) {
    const offsetTableEntry = entryBase + ENTRY_HEADER_SIZE + i * 4;
    const spriteOffset = view.getUint32(offsetTableEntry, true);
    const recordBase = entryBase + spriteOffset;

    // 01-format.md §4：sprite 记录 20 字节 {u32 id; f32 x; f32 y; f32 w; f32 h}。
    // id 是装饰性字段（加载器不读，真实文件里与位置序号未必一致）——这里如实吐出
    // 记录里存的原始 id，不替换成位置序号：这是「只解析不做语义」的选择。
    const id = view.getUint32(recordBase + 0x00, true);
    const x = view.getFloat32(recordBase + 0x04, true);
    const y = view.getFloat32(recordBase + 0x08, true);
    const w = view.getFloat32(recordBase + 0x0c, true);
    const h = view.getFloat32(recordBase + 0x10, true);
    sprites.push({ id, x, y, w, h });
  }
  return sprites;
}

function parseScripts(view, entryBase, nSprites, nScripts, bufferEnd) {
  // 01-format.md §5：script 表紧接 sprite 偏移表之后，每项 8 字节 {s32 id; u32 offset}。
  const scriptTableBase = entryBase + ENTRY_HEADER_SIZE + nSprites * 4;
  const scripts = [];
  for (let i = 0; i < nScripts; i++) {
    const tableEntry = scriptTableBase + i * SCRIPT_TABLE_ENTRY_SIZE;
    const id = view.getInt32(tableEntry + 0x00, true);
    const scriptOffset = view.getUint32(tableEntry + 0x04, true);
    const instructions = parseInstructions(view, entryBase + scriptOffset, bufferEnd);
    scripts.push({ id, instructions });
  }
  return scripts;
}

function parseInstructions(view, scriptBase, bufferEnd) {
  // 01-format.md §5.1：指令头 8 字节 {s16 opcode; u16 size; s16 time; u16 param_mask}，
  // args = (size - 8) / 4 个 u32。opcode === -1 是脚本结束标记；它的 size 字段实测恒为 0，
  // 但在文件里仍占 8 字节——步进游标必须当 8 字节处理，否则会死循环（这里不把它计入
  // instructions 数组：它是结构性终止符，不是一条可执行指令）。
  //
  // 两条显式防线（都是代码审查后补的，不是走运躲过的隐式行为）：
  //   1. size < INSTR_HEADER_SIZE（含 size === 0）直接报错。不这样做的话，游标步进
  //      量可以是 0（死循环）；现在唯一挡住它的是 `new Array(负数)` 抛 RangeError——
  //      这是意外副作用，不是意图，改天有人把 `new Array(argCount)` 重构成
  //      `[]` + push 循环，`argCount` 为负时循环体一次都不进也不抛，死循环就复活了。
  //   2. cursor（连同这条指令的整个 size）不能超出 bufferEnd。没有这条的话，损坏文件
  //      缺失 -1 结束标记时，解析器会把文件里其它结构（name 字符串、别的 script、
  //      sprite 表）当成指令继续解析——要么最终越界抛 RangeError（没有位置信息），
  //      要么更糟：巧合撞上 0xFFFF 提前「成功」结束，静默吐出错误数据。
  const instructions = [];
  let cursor = scriptBase;
  for (;;) {
    if (cursor + INSTR_HEADER_SIZE > bufferEnd) {
      throw new Error(
        `anm-v8: instruction stream at byte offset ${cursor} (script started at ${scriptBase}) ` +
        `runs past end of buffer (length ${bufferEnd}) without a -1 end marker`,
      );
    }

    const opcode = view.getInt16(cursor + 0x00, true);
    if (opcode === OPCODE_END) break;

    const size = view.getUint16(cursor + 0x02, true);
    if (size < INSTR_HEADER_SIZE) {
      throw new Error(
        `anm-v8: instruction at byte offset ${cursor} (opcode ${opcode}) has size ${size}, ` +
        `smaller than the 8-byte instruction header`,
      );
    }
    if (cursor + size > bufferEnd) {
      throw new Error(
        `anm-v8: instruction at byte offset ${cursor} (opcode ${opcode}, size ${size}) ` +
        `runs past end of buffer (length ${bufferEnd})`,
      );
    }

    const time = view.getInt16(cursor + 0x04, true);
    // param_mask（cursor + 0x06）标出哪些 args 是变量 id，属 VM 语义，格式层不解释，
    // 保留在 01-format.md §5.1 里，不在这个输出契约（{time, opcode, args}）里吐出。

    const argCount = (size - INSTR_HEADER_SIZE) / 4;
    const args = new Array(argCount);
    for (let i = 0; i < argCount; i++) {
      args[i] = view.getUint32(cursor + INSTR_HEADER_SIZE + i * 4, true);
    }
    instructions.push({ time, opcode, args });

    cursor += size;
  }
  return instructions;
}

function readCString(bytes, offset) {
  let end = offset;
  while (end < bytes.length && bytes[end] !== 0) end++;
  if (end >= bytes.length) {
    // 与 version / THTX magic 两处一致：宁可带位置信息地报错，也不要静默吞掉——
    // 截断字符串会被当成合法 name 用，问题会在很远的下游才暴露。
    throw new Error(`anm-v8: name string starting at byte offset ${offset} has no NUL terminator before end of buffer`);
  }
  return bytesToString(bytes, offset, end);
}

function readFixedString(bytes, offset, length) {
  return bytesToString(bytes, offset, offset + length);
}

function bytesToString(bytes, start, end) {
  // 按字节原样映射成字符串（不做 Shift-JIS/UTF-8 语义解码）：足以承载 ASCII 路径名
  // （"player/pl02/pl02.png"、"@"、"@R..."），也不会在非 ASCII 字节上抛异常或用
  // U+FFFD 吞掉信息——这是「只做解析不做语义」在字符编码上的落地。
  let s = '';
  for (let i = start; i < end; i++) s += String.fromCharCode(bytes[i]);
  return s;
}
