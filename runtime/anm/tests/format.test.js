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

test('两个 entry 都被正确解析（覆盖 next_offset 链表遍历路径）', () => {
  // minimal.anm.txt 现在有 entry0（2 sprite / 1 script / format 1）和
  // entry1（1 sprite / 1 script / format 3，故意用不同 format 验证「按 entry 各读
  // 各的，不是缓存串号」）。这条测试之前只靠 54 个真实文件冒烟间接盖到，而冒烟
  // 只验「不抛异常」、不验内容——01-format.md §2 的头号断言（entry 链表）本身没有
  // 单元测试直接盯着看，这里补上。
  const parsed = parseAnm(readFileSync(FIX));
  assert.equal(parsed.entries.length, 2);

  const [entry0, entry1] = parsed.entries;
  assert.equal(entry0.name, 'minimal.png');
  assert.equal(entry0.format, 1);
  assert.equal(entry0.sprites.length, 2);
  assert.equal(entry0.scripts.length, 1);

  assert.equal(entry1.name, 'minimal.png');
  assert.equal(entry1.format, 3);
  assert.equal(entry1.sprites.length, 1);
  assert.equal(entry1.scripts.length, 1);
});

test('sprite 字段（含 id）与夹具源码吻合', () => {
  const parsed = parseAnm(readFileSync(FIX));
  const [entry0, entry1] = parsed.entries;

  // id 是文件里存的原始值，不是数组下标——entry0.sprite1 故意在夹具源码里显式给了
  // id:42（与它的位置下标 1 不符），entry1 的自动分配 id 从 42 之后接着走到 43。
  // 见 anm-v8.js 顶部「消费方契约」注释：下游要用位置下标，不要用这个 id。
  assert.deepEqual(
    entry0.sprites.map(s => [s.id, s.x, s.y, s.w, s.h]),
    [[0, 0, 0, 8, 8], [42, 0, 0, 4, 4]],
  );
  assert.deepEqual(
    entry1.sprites.map(s => [s.id, s.x, s.y, s.w, s.h]),
    [[43, 0, 0, 8, 8]],
  );
});

test('script.id 与夹具源码吻合（含负数，验证 s32 而非 u32 读法）', () => {
  // 夹具源码里显式给了 -72 / -1（呼应 01-format.md §5：真实文件 e01–e08 的 script id
  // 就是这个范围）。用负数而不是顺手的 0/1，是因为 0/1 在 getInt32/getUint32 下读出
  // 来的值完全一样，测不出「id 该按有符号读」这类签名 bug；-72/-1 能测出来。
  const parsed = parseAnm(readFileSync(FIX));
  const [entry0, entry1] = parsed.entries;
  assert.equal(entry0.scripts[0].id, -72);
  assert.equal(entry1.scripts[0].id, -1);
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
