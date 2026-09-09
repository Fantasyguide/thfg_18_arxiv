// v8 解析器差分测试：以 thanm -l 的输出为 oracle，不硬编码字节偏移（那会把
// engine/anm/th16/01-format.md 的结论抄第二遍）。
//
// ⚠️ 对 brief 步骤 2 的两处必要修正（task-I1-brief.md 例子代码有 bug，任务书已裁定）：
//   1. `parseAnm` 签名是 `(bytes: Uint8Array)`，不是 `(arrayBuffer: ArrayBuffer)`——
//      所以这里传 `readFileSync(FIX)` 本身（Buffer 是 Uint8Array 子类），不取 `.buffer`。
//      brief 原写法 `readFileSync(p).buffer` 对小文件是 bug：Node 的 Buffer 来自共享
//      内存池，`.buffer` 拿到的是整个池（还带 byteOffset），不是文件字节。
//   2. thanm 的 CLI 是 `thanm -l VERSION ARCHIVE`，比 brief 例子多一个位置参数 VERSION；
//      本仓固定用 8（v8）。
import test from 'node:test';
import assert from 'node:assert/strict';
import { execFileSync } from 'node:child_process';
import { readFileSync } from 'node:fs';
import { parseAnm } from '../src/format/anm-v8.js';

const THANM = process.env.THANM ?? '../../local/vendor/thtk/build/thanm/thanm';
const FIX = new URL('./fixtures/minimal.anm', import.meta.url);

test('entry / sprite / script 计数与 thanm -l 一致', () => {
  const parsed = parseAnm(readFileSync(FIX));
  const dump = execFileSync(THANM, ['-l', '8', FIX.pathname], { encoding: 'utf8' });
  const nEntry = (dump.match(/^entry\b/gm) ?? []).length;
  const nScript = (dump.match(/^script\b/gm) ?? []).length;
  assert.equal(parsed.entries.length, nEntry);
  assert.equal(parsed.entries.flatMap(e => e.scripts).length, nScript);
});

test('指令流的 time 与 opcode 单调可读', () => {
  const parsed = parseAnm(readFileSync(FIX));
  const ins = parsed.entries[0].scripts[0].instructions;
  assert.ok(ins.length > 0, '至少解出一条指令');
  for (const i of ins) {
    assert.equal(typeof i.time, 'number');
    assert.equal(typeof i.opcode, 'number');
    assert.ok(Array.isArray(i.args));
  }
});

test('sprite 字段与夹具源码吻合', () => {
  const parsed = parseAnm(readFileSync(FIX));
  const sprites = parsed.entries[0].sprites;
  assert.equal(sprites.length, 2);
  assert.deepEqual(
    sprites.map(s => [s.x, s.y, s.w, s.h]),
    [[0, 0, 8, 8], [0, 0, 4, 4]],
  );
});

test('指令内容与夹具源码（set/add/jmp/stop）吻合', () => {
  const parsed = parseAnm(readFileSync(FIX));
  const ins = parsed.entries[0].scripts[0].instructions;
  // minimal.anm.txt：ins_100(0,1); ins_102(0,2); ins_200(finish,0); ins_3();
  // 结束标记 opcode=-1 不计入 instructions（见 anm-v8.js parseInstructions 的注释）。
  assert.deepEqual(ins.map(i => i.opcode), [100, 102, 200, 3]);
  assert.deepEqual(ins.map(i => i.time), [0, 0, 1, 2]);
  assert.deepEqual(ins[0].args, [0, 1]);
  assert.deepEqual(ins[1].args, [0, 2]);
  assert.equal(ins[2].args.length, 2);
  assert.deepEqual(ins[3].args, []);
});

test('entry 元信息（name/width/height/format）与夹具源码吻合', () => {
  const parsed = parseAnm(readFileSync(FIX));
  const entry = parsed.entries[0];
  assert.equal(entry.name, 'minimal.png');
  assert.equal(entry.width, 8);
  assert.equal(entry.height, 8);
  assert.equal(entry.format, 1);
});
