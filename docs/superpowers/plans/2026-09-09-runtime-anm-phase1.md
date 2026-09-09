# ANM v8 播放器（第一阶段）实施计划

> **For agentic workers:** 本计划的任务按「**一个 agent 一次独立会话**」切分，不是抄代码级步骤。
> R 系列（逆向）给目标 / 一手源 / 判据 / 落点，**不给代码**——结论还没逆出来，预写代码是假的。
> I 系列（实现）给真实测试代码，走 TDD。派发前先读 §「执行模型」，尤其 **Ghidra 锁**那条。

**Goal**：做出一个 ANM v8 播放器（JS + WebGL），并在过程中把 `engine/anm/` 从 272 行推到覆盖 VM 语义。

**Architecture**：两层。层 A（VM，121 条 opcode）纯算术、与渲染 API 无关；层 B（渲染后端，Drawing 组 14 条）
是 D3D9 固定管线到 WebGL 的翻译。知识产出落 `engine/anm/th16/`，代码落 `runtime/anm/`，两者双向互指。

**Tech Stack**：JS（ES modules）+ WebGL2 + `node:test`（Node v22.22.1，零依赖）；
逆向走 ghidra-re MCP / HTML 导出；夹具用 `local/vendor/thtk/build/thanm` 编译。

> **版本**：跨版本。本文出现的地址一律带版本前缀（如 `th16:0x45fce8`）。

**Spec**：[`../specs/2026-09-09-runtime-anm-design.md`](../specs/2026-09-09-runtime-anm-design.md)

## 执行状态（2026-09-09 收工）

**波 A–C 已完成并推送**：分支 `runtime/anm-phase1`，19 提交，未合并 `main`。
`npm test` 44/44 · `check-docs.py` 164 文件四项全绿 · 54 个真实 `.anm` 解析 54/54。

| 任务 | 状态 | 提交 | 产出 |
| --- | --- | --- | --- |
| T0 | ✅ | `9912c65`→`f021690` | `runtime/` 骨架、th16 HTML 快照（`local/th16.v1.00a/state/`）、opcode 矩阵生成器 |
| R1 | ✅ | `4e1503b`→`a4ddd7a` | `engine/anm/th16/01-format.md`（25 ✅ / 4 🟡） |
| I1 | ✅ | `7b5bd62`→`e6943a8` | `runtime/anm/src/format/anm-v8.js` |
| R2 | ✅ | `640aaf2`→`2b8484e` | `engine/anm/th16/02-vm-core.md`（62 ✅ / 9 🟡） |
| I2 | ✅ | `0da2ebd`→`2192e80` | `runtime/anm/src/vm/core.js` |
| R3 | ✅ | `c12f499`→`037755c` | `engine/anm/th16/03-math-jumps.md`（41 ✅ / 5 🟡） |
| I3 | ✅ | `4373548` | `runtime/anm/src/vm/group-math.js`（46 条全注册） |
| 收口 | ✅ | `5cd4918`/`7b44583`/`916b4a8` | 消两处虚标 ✅、断言回流、矩阵生成器防抹平 |
| **RV1** | ⬜ **下一个** | — | `engine/anm/th16/04-variables.md`（36 行表） |
| **RV2** | ⬜ | — | 同上，补 `10033–10035` 专项（可与波 D 并行） |
| **IV** | ⬜ | — | `resolveArg` 8 槽 → 22 槽 |
| 波 D–F | ⬜ | — | 见下 |

**下次开工从 RV1 起**，brief 已生成在 `.superpowers/sdd/2026-09-09-runtime-anm-phase1/task-RV1-brief.md`
（RV2 的 brief 同目录；RV1 完成后才派 RV2。该目录 gitignored，含 19 份 brief + 各任务 report
+ 完整执行账本 `progress.md`，里面有全部 11 条控制器裁定与每轮审阅发现——**下次审阅时读它**）。

### 已知遗留（不在本分支范围，波 D 前建议处理）

1. **`resolveArg` 只覆盖 8/22 个可写变量号** —— 实测 Math+Jumps 组 2525 条真实指令里
   **1078 条（42.7%）会抛**，1929 个 script 里 338 个（17.5%）受影响。这正是 RV1/IV 要解决的。
2. **「33 次 / `(-π, π]`」在仓里还有 4 处未改**（`_shared/math-and-prng.md` §2/§2.1、
   `bullet/th16/03-lasers.md`、`sht/th16/06`、`sht/th18/02`），其中一处写着「跟数学模块同套」——
   现在指向一个已知错误的数字（一手为 **34 次 / `[-π, π]`**，见 `03-math-jumps.md` §7.1）。
   与 anm 侧构成跨子系统互相矛盾的 ✅。
3. **`124–128` 的 CRT 函数身份**靠常量指纹判定、无运行时验证。升 ✅ 不需要 32 位环境：
   这五个是纯叶子函数无导入，用 `unicorn` 映射 `.text`/`.rdata` 直接调（约 30 行）与 libm 对拍即可。
4. **`naming-batch.md` 待落库**（T10）：`102__iadd`→`102__add` 与 `112__add`→`112__add3`
   **必须同一 MCP 批次原子应用**，否则中途库里会出现两个 `112__add`。
5. **Ghidra MCP 的 `th16` 注册项已泄漏**（开报 DuplicateId、关报 NotFound），需重启 MCP 服务端才能清；
   本轮四个逆向任务全部改走静态快照 + PE 直解完成，质量未打折。
   另：`local/th16.v1.00a/ghidra_projects/th16.exe.gpr` 是 **0 字节**，工程注册有历史损伤，可能是根因。

---

## Global Constraints

- **仓库不留任何版权字节。** 真 `.anm`、`.exe`、贴图只在 `local/`（gitignored）。入库夹具一律
  我们自写源码 + `thanm` 编译。
- **逆向主攻 th16**（`local/th16.v1.00a/th16.exe`，md5 `cb9caf54ce5738f70086e783ec88fd2a`，
  imagebase `0x400000`，MCP `database_id` = `th16`）。th18 在阶段 3 补差。
- **地址格式**：`` `th16:0x45fce8` ``，反引号、无 `@`。文档开头声明默认版本；跨版本文档每个地址带
  `th16:` / `th18:` 前缀。写完跑 `python3 tooling/check-docs.py`。
- **可信度**：thpages / th-re-data 是**社区单源**。只有在 th16.exe 上一手复核过才能标 ✅，
  否则 🟡。社区标 `wip:2` 的 6 条若得出结论，要过 `METHOD.md` 的额外闸门，复核前一律 🟡。
- **结论五段链**：发现 → 推测 → 验证 → 结论（可信度 + 版本）→ 证据（地址 / 出处）。
- **层次归属**：引擎语义只进 `engine/`；实现取舍只进 `runtime/`。`runtime/` 里每条实现要能指回
  `engine/` 的一条断言，指不回去就在代码里标 🟡。

---

## 执行模型

### ★ Ghidra 工程锁是工程级独占——这条决定了并行怎么排

`docs/SETUP.md` §锁：MCP 开着库 / GUI 开着工程 / driver 在跑，三者**同一时刻只能有一个**。
**N 个 agent 同时用 MCP 逆同一个库会直接撞锁。**

解法（仓库已有）：

1. **T0 里做一次性 HTML 导出**——`tooling/ghidra/export_html.py th16` 导出时占一次锁（几分钟），
   之后随便翻都不占。导出内容对齐 MCP 只读工具（反编译 C、反汇编、双向 xref、调用图、
   局部变量、原始字节、结构体、字符串、函数表），**逆向所需的只读能力全在里面**。
2. **并行的 R 任务一律读 HTML，不碰 MCP。** 这是硬规则，写进每个 R 任务的判据。
3. **需要写库的动作（重命名函数、建结构体、加 label）必须串行化**：每个 R agent 把想要的命名
   写成一份 `naming-batch.md` 交回，由 T10 统一用 MCP 落库，再 `tooling/ghidra/symbols.py export th16`。
   ⚠️ 不导出就等于没有。

### 派发波次

```
波 A（串行）  T0 ──> R1 ──> I1
波 B（串行）  R2 ──> I2                      VM 骨架，后面全部依赖它
波 C（串行）  R3 ──> I3                      Math+Jumps：用来跑通整条流水线并校准方法
波 C′（串行） RV1 ──> IV                     ★ 变量表：波 D 的前置依赖，有界任务
              └──> RV2                     mystery-angle 三条专项：开放式，RV1 完成后与波 D 并行，不挡 IV
波 D（并行）  R4 R5 R6 R7 R8                 ★ 五个 agent 同时派发，各读 HTML
波 E（串行）  I4 ─> I5 ─> I6 ─> I7 ─> I8     按 spec §6 的组顺序实现
波 F（串行）  I9 ──> T10 ──> R11
```

**为什么波 C′ 必须在波 D 之前**：`resolveArg` 现在只覆盖 8 个变量槽（`I0–I3`/`F0–F3`），
而 th16 实际有 **36 个变量号、其中 22 个可写**。实测：Math+Jumps 组的 2525 条真实指令里
**1078 条（42.7%）会抛**，1929 个 script 里 338 个（17.5%）受影响。而波 D 的 Settings(300+)
与 General(400+) 两组的主战场正是 `10013–10015`（pos）与 `10023–10025`（rot）。
不先补，五个组会各自重演一遍「语义实现完了但跑不了真实数据」，且各自长一份 `writeIntVar`——
正是分层要避免的事。

**为什么 R3 不并进波 D**：spec §6 定了 Math/Jumps 先落地跑通流水线。多一条理由——R3 产出的
`03-*.md` 是波 D 五个 agent 的**范例文档**。没有范例就并行，等于把一个未校准的方法乘以五。

---

## 文件结构

| 文件 | 职责 | 由谁产出 |
| --- | --- | --- |
| `runtime/README.md` | 层门面：这一层是什么、与 `engine/` 的关系、双向互指纪律、怎么跑 | T0 |
| `runtime/anm/OVERVIEW.md` | 子系统门面：135 条 opcode 的「已实现 / 已验证 / 待复核」状态矩阵 | T0，各 I 任务更新 |
| `runtime/anm/src/format/anm-v8.js` | `.anm` v8 解析：entry 表、sprite 表、script 表 → 纯数据对象 | I1 |
| `runtime/anm/src/vm/core.js` | VM 骨架：取指、时间标签推进、跳转、interrupt、vm 状态对象 | I2 |
| `runtime/anm/src/vm/group-math.js` | opcode 100–199 + 200–299（Math + Jumps，46 条） | I3 |
| `runtime/anm/src/vm/group-system.js` | opcode 0–99（System，8 条） | I4 |
| `runtime/anm/src/vm/group-settings.js` | opcode 300–399（Settings，18 条） | I5 |
| `runtime/anm/src/vm/group-general.js` | opcode 400–499（General，39 条） | I6 |
| `runtime/anm/src/vm/group-child.js` | opcode 500–599（Child management，10 条） | I7 |
| `runtime/anm/src/vm/group-draw.js` | opcode 600–699（Drawing，14 条）——只置状态，不发 draw call | I8 |
| `runtime/anm/src/render/webgl.js` | WebGL2 后端：四相机、渲染阶段、混合模式、精灵四边形 | I8 |
| `runtime/anm/src/render/D3D9-MAPPING.md` | 逐条「D3D9 语义 / WebGL 对应 / 差异 / 处置」（spec §10.4） | I8 |
| `runtime/anm/tests/fixtures/*.anm.txt` | 夹具**源码**（我们自写，入库） | I1 起各任务追加 |
| `runtime/anm/tests/build-fixtures.sh` | 用 `thanm` 把源码编成 `.anm`（产物 gitignored） | I1 |
| `runtime/anm/web/index.html` | 查看器：时间轴、逐帧 vm 状态、加载本地 `.anm` | I9 |
| `tooling/pe_read.py` | 按 VA 直读 exe 字节（手解 PE 节表）——解跳表 / 常量池，**不占工程锁** | RV1（前置） |
| `engine/anm/th16/01-format.md` … `09-*.md` | 一手结论（九篇） | R1–R3、**RV1**（+ RV2）、R4–R8 |
| `engine/anm/th16/04-variables.md` | **变量表**：36 个变量号的语义 / 可写性 / PRNG 流归属 | **RV1**（+ RV2 补 §mystery） |
| `engine/anm/OVERVIEW.md` | 断言矩阵扩到覆盖 VM 语义 | T10 |

`vm/` 按 opcode 组切文件而非按功能切，是为了让**代码结构与 opcode 表一一对应**——
`engine/anm/th16/` 的表格行直接映到文件，漏实现的 opcode 一眼看得出。

---

## 波 A — 地基

### T0：环境地基 + `runtime/` 骨架

**Files:**
- Create: `runtime/README.md`, `runtime/anm/OVERVIEW.md`, `runtime/anm/package.json`
- Create: `runtime/anm/tests/smoke.test.js`
- Modify: `README.md`（顶层目录树加 `runtime/` 一行）, `.gitignore`

**Produces:** 一份可并行只读的 th16 HTML 导出；`node --test` 可跑；`runtime/` 四层定位落地。

- [ ] **步骤 1：解包 th16 的 anm**（th18 的 56 个已解，th16 还没有）

```bash
python3 tooling/thtk/unpack.py th16.v1.00a
ls local/th16.v1.00a/anm/ | wc -l      # 期望 > 0；产物在 local/，不入库
```

- [ ] **步骤 2：导出 HTML（占一次锁，之后波 D 的并行全靠它）**

```bash
source tooling/env.sh
"$JAVA_HOME/bin/python" tooling/ghidra/export_html.py th16
tooling/ghidra/serve.sh th16           # 只绑 127.0.0.1
```
把导出目录路径记进 `runtime/README.md`，波 D 的 agent 直接读。

- [ ] **步骤 3：建 `runtime/` 骨架**

`runtime/README.md` 必须写清四层分工与**双向互指纪律**（spec §4 原文）：

> `engine/` 引擎怎么工作 · `runtime/` 同一份知识的可执行形式 · `mods/` 我改了什么 · `tooling/` 怎么再来一遍
>
> - `runtime/` 里每条实现必须能指回 `engine/` 的一条断言。指不回去 = 你在猜，代码里标 🟡。
> - `runtime/` 跑出来的差异回流 `engine/` 当新问题。播放器是证伪装置。
> - `runtime/` **不放引擎结论**。

`runtime/anm/OVERVIEW.md` 建一张 135 行的状态矩阵骨架（opcode / 组 / thpages 名 / 文档 / 实现 / 可信度），
初始全部「未开始」。

- [ ] **步骤 4：`node:test` 跑通**

```js
// runtime/anm/tests/smoke.test.js
import test from 'node:test';
import assert from 'node:assert/strict';

test('node:test 可用，ES module 导入正常', () => {
  assert.equal(1 + 1, 2);
});
```

```json
// runtime/anm/package.json
{ "name": "th-anm-runtime", "type": "module", "private": true,
  "scripts": { "test": "node --test tests/" } }
```

- [ ] **步骤 5：`.gitignore` 加夹具产物**

```
runtime/anm/tests/fixtures/*.anm
runtime/anm/node_modules/
```

- [ ] **步骤 6：跑通并提交**

```bash
cd runtime/anm && npm test          # 期望 1 passed
cd ../.. && python3 tooling/check-docs.py
git add runtime .gitignore README.md && git commit -m "feat(runtime): 新开 runtime/ 层与 anm 骨架"
```

**完成判据**：`npm test` 绿；`check-docs.py` 绿；HTML 导出可访问；`local/th16.v1.00a/anm/` 非空。

---

### R1：ANM v8 文件格式一手确认

**这是一次独立逆向会话。** 读 HTML 导出，不碰 MCP。

**目标**：把 `.anm` v8 的文件布局逐字段定死——header、entry 表、sprite 表、script 表、
贴图引用、entry 之间的链接方式——每条带地址证据。

**一手源**：th16 的 anm 加载路径。入口找法：`local/vendor/th-re-data/data/th16.v1.00a/funcs.json`
里搜 `Anm`（165 个命名），重点看 `AnmManager::` 的 preload / load 族。

**社区源（对账，非结论）**：
- `local/vendor/thtk/` 的 `thanm` 源码——**格式权威**，分歧时以它和一手 loader 为准
- `local/th16.v1.00a/anm/<名>/<名>.anm.txt`（T0 解出的 thanm `-l` 文本）当实据

**方法**：三方对账——thanm 源码怎么读 × loader 反编译怎么读 × 真实文件字节对不对得上。
挑至少 3 个真实 `.anm`（一个小的、一个带多 entry 的、一个带多 script 的）验证。

**产出**：`engine/anm/th16/01-format.md`。必须包含一张**字段偏移表**（偏移 / 类型 / 含义 / 可信度）
和「v8 与更早版本的差异」一节（若 loader 里有版本分支）。

**完成判据**：
1. 字段表能让人不看 thanm 源码就写出解析器
2. 每条结论有地址或「thanm 源码 + 真实文件双证」
3. `check-docs.py` 绿
4. 想要的 Ghidra 重命名写进 `engine/anm/th16/naming-batch.md`（T10 统一落库），**不要自己开 MCP**

---

### I1：v8 解析器

**Files:**
- Create: `runtime/anm/src/format/anm-v8.js`, `runtime/anm/tests/format.test.js`
- Create: `runtime/anm/tests/fixtures/minimal.anm.txt`, `runtime/anm/tests/build-fixtures.sh`

**Interfaces:**
- Consumes: `engine/anm/th16/01-format.md` 的字段表
- Produces: `parseAnm(arrayBuffer) -> { entries: [{ name, width, height, format, sprites, scripts }] }`，
  其中 `sprites: [{ id, x, y, w, h }]`，`scripts: [{ id, instructions: [{ time, opcode, args }] }]`

- [ ] **步骤 1：写夹具源码与构建脚本**

```
# runtime/anm/tests/fixtures/minimal.anm.txt —— 自写，零版权字节
# 具体语法以 R1 确认的 thanm 版本为准；至少包含：
#   一个 entry、一张 8x8 纯色贴图引用、两个 sprite、一个 script（set/add/jmp/stop）
```

```bash
#!/usr/bin/env bash
# runtime/anm/tests/build-fixtures.sh
set -euo pipefail
THANM="$(command -v thanm || echo "$(git rev-parse --show-toplevel)/local/vendor/thtk/build/thanm")"
cd "$(dirname "$0")/fixtures"
for f in *.anm.txt; do "$THANM" -c "${f%.txt}" "$f"; done
```

- [ ] **步骤 2：写失败测试——用 thanm 当 oracle**

不硬编码字节偏移（那会把 R1 的结论抄两遍）。改成**差分测试**：我们的解析结果必须与
`thanm -l` 的输出一致。

```js
// runtime/anm/tests/format.test.js
import test from 'node:test';
import assert from 'node:assert/strict';
import { execFileSync } from 'node:child_process';
import { readFileSync } from 'node:fs';
import { parseAnm } from '../src/format/anm-v8.js';

const THANM = process.env.THANM ?? '../../local/vendor/thtk/build/thanm';
const FIX = new URL('./fixtures/minimal.anm', import.meta.url);

test('entry / sprite / script 计数与 thanm -l 一致', () => {
  const parsed = parseAnm(readFileSync(FIX).buffer);
  const dump = execFileSync(THANM, ['-l', FIX.pathname], { encoding: 'utf8' });
  const nEntry = (dump.match(/^entry\b/gm) ?? []).length;
  const nScript = (dump.match(/^script\b/gm) ?? []).length;
  assert.equal(parsed.entries.length, nEntry);
  assert.equal(parsed.entries.flatMap(e => e.scripts).length, nScript);
});

test('指令流的 time 与 opcode 单调可读', () => {
  const parsed = parseAnm(readFileSync(FIX).buffer);
  const ins = parsed.entries[0].scripts[0].instructions;
  assert.ok(ins.length > 0, '至少解出一条指令');
  for (const i of ins) {
    assert.equal(typeof i.time, 'number');
    assert.equal(typeof i.opcode, 'number');
    assert.ok(Array.isArray(i.args));
  }
});
```

- [ ] **步骤 3：跑测试确认失败**

```bash
cd runtime/anm && bash tests/build-fixtures.sh && npm test
# 期望 FAIL: Cannot find module '../src/format/anm-v8.js'
```

- [ ] **步骤 4：按 `01-format.md` 的字段表实现 `parseAnm`**

只做解析，不做语义。每个偏移常量旁边用注释指回 `01-format.md` 的对应行（双向互指纪律）。

- [ ] **步骤 5：跑测试确认通过，并对真实文件冒烟**

```bash
npm test        # 期望全绿

# 真实文件冒烟：把 local/ 里所有 th16 的 .anm 全跑一遍，任何一个抛异常都算没做完
node --input-type=module -e "
import { readFileSync, readdirSync } from 'node:fs';
const { parseAnm } = await import('./src/format/anm-v8.js');
const dir = '../../local/th16.v1.00a/dat';
let ok = 0;
for (const f of readdirSync(dir).filter(f => f.endsWith('.anm'))) {
  const { entries } = parseAnm(readFileSync(\`\${dir}/\${f}\`).buffer);
  console.log(f, entries.length, 'entries',
              entries.flatMap(e => e.scripts).length, 'scripts');
  ok++;
}
console.log('OK:', ok, 'files');
"
```

- [ ] **步骤 6：更新状态矩阵并提交**

在 `runtime/anm/OVERVIEW.md` 把「格式层」标为已实现。

```bash
git add runtime/anm engine/anm/th16 && git commit -m "feat(runtime/anm): v8 解析器 + thanm 差分测试"
```

**完成判据**：`npm test` 绿；能解析 `local/th16.v1.00a/dat/` 里**任意**真实 `.anm` 不抛异常。

---

## 波 B — VM 骨架

### R2：VM 骨架一手（结构 + 驱动 + 时间 + interrupt）

**独立逆向会话，读 HTML。**

**目标**：搞清 VM 怎么跑，而不是单条 opcode 干什么：

1. `zAnmVm` 的字段图（至少：当前指令指针、时间、pos/scale/rotation/color/uv、flag 位场、父子指针）
2. 每帧驱动：谁 tick、一帧内推进多少、时间标签（`time`）与指令执行的关系
3. **interrupt 机制**：怎么触发、怎么找到目标标签、触发后时间怎么变
4. 指令流的终止语义（v8 的 `1 delete` / `2 static` / `3 stop` / 跑到末尾；
   ⚠️ `0` 在 v8 是 `nop`，v8 无 opcode 15 —— 初稿误把 TH06(v0) 的编号搬进 v8 语境，R2 一手纠正）

**一手源**：`labels.json` 的 `anm` 键给了 137 条 case label 带地址，其中
`th16:0x45fb9a end_loop_iter` / `th16:0x45fba0 begin_loop_iter` 直接框出了**解释器主循环**——从那里往外扩。
`funcs.json` 里 `AnmFastVm::` / `AnmLoaded::` / `AnmManager::` 三族命名。

**社区源**：thpages 的 `#anm/concepts` 概念页（time label、interrupt 的说明）；
`local/vendor/thpages/static/mapfile/v8.anmm`。

**产出**：`engine/anm/th16/02-vm-core.md`。

**完成判据**：
1. 字段图能让人写出 vm 状态对象
2. 「一帧发生什么」有明确的伪代码，且与 `begin_loop_iter` / `end_loop_iter` 的反编译对得上
3. interrupt 的查找与跳转有地址证据
4. 命名写进 `naming-batch.md`，不开 MCP
5. `check-docs.py` 绿

---

### I2：VM 骨架实现

**Files:**
- Create: `runtime/anm/src/vm/core.js`, `runtime/anm/tests/vm-core.test.js`
- Create: `runtime/anm/tests/fixtures/vm-timing.anm.txt`

**Interfaces:**
- Consumes: `parseAnm` (I1)；`engine/anm/th16/02-vm-core.md`
- Produces:
  - `createVm(script, opts) -> vm`，`vm` 至少有 `{ i: [0,0,0,0], f: [0,0,0,0], pos, scale, rotation, color, time, pc, alive }`
  - `stepFrame(vm, ops) -> void`（推进一帧；`ops` 是 opcode 号 → handler 的表）
  - `registerGroup(ops, groupTable) -> void`（各组把自己的 handler 注册进来）

- [ ] **步骤 1：写夹具**——一个只用 `set` / `add` / `jmp` / `stop` 的脚本，配上明确的时间标签，
  用来断言「第 N 帧时 i0 应为多少」。这几条 opcode 的语义是确定的（thpages 生成器表），
  可以现在就写死期望值。

- [ ] **步骤 2：写失败测试**

```js
// runtime/anm/tests/vm-core.test.js
import test from 'node:test';
import assert from 'node:assert/strict';
import { readFileSync } from 'node:fs';
import { parseAnm } from '../src/format/anm-v8.js';
import { createVm, stepFrame, registerGroup } from '../src/vm/core.js';

// 最小 op 表：只实现 set/add，用来验骨架而非验组
const ops = {};
registerGroup(ops, {
  100: (vm, [dst, v]) => { vm.i[dst] = v; },        // set
  112: (vm, [dst, v]) => { vm.i[dst] += v; },       // add
});

function loadFirstScript(path) {
  const { entries } = parseAnm(readFileSync(new URL(path, import.meta.url)).buffer);
  return entries[0].scripts[0];
}

test('同一时间标签上的指令在同一帧内全部执行', () => {
  const vm = createVm(loadFirstScript('./fixtures/vm-timing.anm'));
  stepFrame(vm, ops);
  assert.equal(vm.i[0], 7, 'set(i0,3) 与 add(i0,4) 同在 t=0，应在第一帧跑完');
});

test('时间未到的指令不执行', () => {
  const vm = createVm(loadFirstScript('./fixtures/vm-timing.anm'));
  stepFrame(vm, ops);
  assert.equal(vm.time, 1, '一帧推进 1');
  assert.notEqual(vm.i[1], 99, 't=10 的指令不该在第 1 帧执行');
});

test('stop 之后 pc 不再前进，但 vm 仍存活', () => {
  const vm = createVm(loadFirstScript('./fixtures/vm-timing.anm'));
  for (let n = 0; n < 60; n++) stepFrame(vm, ops);
  const pc = vm.pc;
  stepFrame(vm, ops);
  assert.equal(vm.pc, pc);
  assert.equal(vm.alive, true);
});
```

- [ ] **步骤 3：跑测试确认失败** — `npm test`，期望模块找不到。
- [ ] **步骤 4：按 `02-vm-core.md` 实现 `core.js`**。骨架只管取指 / 时间 / 跳转 / interrupt / 生死，
  **不含任何具体 opcode**——那是各组的事。
- [ ] **步骤 5：跑测试确认通过。**
- [ ] **步骤 6：更新状态矩阵，提交。**

**完成判据**：三个测试全绿；`core.js` 里没有任何 opcode 号常量（除时间/终止语义必需的）。

---

## 波 C — Math + Jumps（跑通流水线并校准方法）

### R3：Math + Jumps 组（46 条）

**独立逆向会话，读 HTML。这一篇是波 D 五个 agent 的范例，写法要立住。**

**目标**：opcode 100–199（32 条）与 200–299（14 条）逐条确认签名与语义。

**一手源**：`labels.json` 的 `anm` 键直接给了每条的 case 地址，例如
`th16:0x45fce8 100__set`、`th16:0x45fd68 112__add`、`th16:0x45fbfc 200__jmp`、`th16:0x45fc12 201__jmpIfPos`。
逐个反编译 case 体。

**社区源**：thpages 的 `OPERATOR_DATA` / `OPERATOR_3_DATA` / `JUMP_DATA` 三张生成器表
（`local/vendor/thpages/js/tables/reference/anm.ts` 约 604–646 行）——它们把这 46 条描述成
`a += b` / `a = b + c` / `jump if a < b` 这类；**这是待验假设，不是结论**。

**方法**：逐条比对「生成器表说的运算」与「case 体反编译出的运算」。重点看四件容易错的事：
① 整型槽与浮点槽（`i0-3` / `f0-3`）的编号与存储位置；② 除零 / 取模的边界；
③ 跳转目标是**字节偏移**还是指令下标；④ 跳转后 `time` 怎么设。

**产出**：`engine/anm/th16/03-math-jumps.md`——一张 46 行表（opcode / 助记 / 签名 / 语义 / 地址 / 可信度）
外加上述四点各开一节。

**完成判据**：
1. 46 条**全部**有地址与可信度标记；与社区说法不一致的单独开节记「我们错 / 社区错」的裁决
2. 表格单元格 ≤ 200 字符（超了就拆小节，`check-docs.py` 会报）
3. `check-docs.py` 绿
4. 命名写进 `naming-batch.md`，不开 MCP

---

### I3：Math + Jumps 实现

**Files:**
- Create: `runtime/anm/src/vm/group-math.js`, `runtime/anm/tests/vm-math.test.js`
- Create: `runtime/anm/tests/fixtures/math-ops.anm.txt`

**Interfaces:**
- Consumes: `registerGroup` / `createVm` / `stepFrame` (I2)；`engine/anm/th16/03-math-jumps.md`
- Produces: `mathJumpsGroup` —— 一个 `{ [opcode]: (vm, args) => void }` 表，覆盖 100–199 与 200–299

- [ ] **步骤 1：夹具**——写一个把每种运算都用一遍的脚本，末尾 `stop`；期望值按 `03-math-jumps.md` 算。
- [ ] **步骤 2：写失败测试**

```js
// runtime/anm/tests/vm-math.test.js
import test from 'node:test';
import assert from 'node:assert/strict';
import { readFileSync } from 'node:fs';
import { parseAnm } from '../src/format/anm-v8.js';
import { createVm, stepFrame, registerGroup } from '../src/vm/core.js';
import { mathJumpsGroup } from '../src/vm/group-math.js';

const ops = {};
registerGroup(ops, mathJumpsGroup);

function run(fixture, frames) {
  const { entries } = parseAnm(readFileSync(new URL(fixture, import.meta.url)).buffer);
  const vm = createVm(entries[0].scripts[0]);
  for (let n = 0; n < frames; n++) stepFrame(vm, ops);
  return vm;
}

test('整型四则：set/add/sub/mul/div/mod', () => {
  const vm = run('./fixtures/math-ops.anm', 1);
  assert.equal(vm.i[0], 7);      // set 3; add 4
  assert.equal(vm.i[1], 6);      // set 2; mul 3
  assert.equal(vm.i[2], 1);      // set 7; mod 3
});

test('三元式 add3：x = a + b，不改 a/b', () => {
  const vm = run('./fixtures/math-ops.anm', 1);
  assert.equal(vm.i[3], 9);
});

test('浮点槽与整型槽相互独立', () => {
  const vm = run('./fixtures/math-ops.anm', 1);
  assert.ok(Math.abs(vm.f[0] - 1.5) < 1e-6);
  assert.equal(vm.i[0], 7, '写 f0 不该动 i0');
});

test('jmpLt 条件成立时跳转并按表设定 time', () => {
  const vm = run('./fixtures/math-ops.anm', 3);
  assert.equal(vm.i[3], 9, '循环体按 03-math-jumps.md 的跳转语义执行');
});

test('所有 46 条 opcode 都已注册', () => {
  const codes = Object.keys(mathJumpsGroup).map(Number);
  const inRange = codes.filter(c => (c >= 100 && c < 200) || (c >= 200 && c < 300));
  assert.equal(inRange.length, 46, '缺一条就说明 03-math-jumps.md 的表没实现完');
});
```

- [ ] **步骤 3：跑测试确认失败。**
- [ ] **步骤 4：实现 `group-math.js`。** 每条 handler 上方注释写 `// 03-math-jumps.md #<opcode>`。
- [ ] **步骤 5：跑测试确认通过。**
- [ ] **步骤 6：更新状态矩阵；把这一轮的真实耗时记进 `runtime/anm/OVERVIEW.md`，**
      **用来重估波 D 的五组**（spec §11 风险表第一行的处置）。提交。

**完成判据**：五个测试绿，尤其最后那条「46 条全注册」；耗时已记录。

---

## 波 C′ — 变量表专项（波 D 的前置依赖）

### RV1：th16 ANM 变量表一手确认（36 个变量号）

**独立逆向会话。** 静态快照 + PE 直解，不碰 Ghidra MCP。

**目标**：把 `10000–10035` 共 36 个变量号逐条定死——语义、类型、**可写性**、
背后落在 vm 的哪个字段、以及（对随机族）走哪条 PRNG 流。

**一手源**：
- 读侧：`AnmVm::get_int_var` `th16:0x45f610`、`AnmVm::get_float_var` `th16:0x45f2d0`
  （后者的 `add eax,-10000; cmp eax,0x23; jmp [eax*4+0x45f578]` 是一张 **36 项**跳表，
  与社区表条数精确对应）
- ★ **那张跳表已解出，直接用**（2026-09-09 实测，`tooling/pe_read.py`）：
  `th16:0x45f578` 36 项 → case 地址，例 `10000 → th16:0x45f2ed`、`10010 → th16:0x45f3e7`、
  `10035 → th16:0x45f371`。⚠️ **表项顺序 ≠ case 代码顺序**（`10010` 的 case 体排在 `10011` 之后），
  照反编译 switch 顺序猜会错——自己重跑一遍核对：
  `python3 tooling/pe_read.py local/th16.v1.00a/th16.exe 0x45f578 --dwords 36 --base 10000`
- 读侧**字段落点**：`local/th16.v1.00a/state/f/0045f2d0.html` 反汇编里每个 case 的
  `MOVD XMM0,[ECX+0x…]` 就是落点（`10013–10015 → +0x2c/0x30/0x34`、
  `10023–10025 → +0x38/0x3c/0x40`、`10033–10035 → +0x4c0/0x4c4/0x4c8`；
  `10016–10021` 与 `10026` **没有直接字段读**，走 helper，单独交代）。
  ⚠️ **别用 `state/data/functions.jsonl` 的反编译文本**——Ghidra 丢了 XMM 返回值，
  36 个 case 全被糊成裸 `return;`（实测）。**看反汇编。**
- 写侧：两张 lvalue 表（`03-math-jumps.md` §9 已解出并逐条列举，可直接接用）

**社区源（对账，非结论）**：`local/vendor/thpages/js/tables/reference/anmvar.ts`
—— **覆盖完整**：th16 的 36 个变量号全部有名，49 条 byRefId 里 **32 条已定稿 / 14 条 `wip:1` /
3 条 `wip:2`**，且带 `mut` 字段（可写性）与 `type`（`$` 整型 / `%` 浮点）。
按它统计：**可写 22 个、只读 14 个**——这个数字与我们独立逆向出的一致。

**已有的一手结论（别重复论证，直接接）**：
- `03-math-jumps.md` §3：`I0–I3`=10000–10003、`F0–F3`=10004–10007，跨类型是**转换**非重解释位
- `03-math-jumps.md` §7.4：随机族的 PRNG 流划界（10010–10012 走 Stream B，10030–10032 走 Stream A）
- `02-vm-core.md` §3.4：已在收口轮修正过 `10026/10027/10028`

**`10033–10035`（`mystery-angle-x/y/z`）不归你** —— 它们是 **RV2** 的专项。
你在表里给出类型、可写性、case 地址、落在 vm 的哪个字段（这些都是有界的一手工作），
**语义一栏标 ❓ 并指向 RV2**，不要在本任务里去追它们的含义。这条边界是刻意的：
本任务必须是**有界**的，判据可验证。

**产出**：`engine/anm/th16/04-variables.md`。**单版本文档**（默认 th16），开头 20 行内声明版本。
一张 36 行表（变量号 / 社区名 / 类型 / **可写性** / 语义 / 落在 vm 的哪个字段 / case 地址 / 可信度）
+ 若干专项小节。**格式照 `03-math-jumps.md`**（它是范例，且已按审阅意见改进过）。

**完成判据**：
1. 36 条**全部**有 case 地址与可信度；可写性逐条有一手依据（不是照抄 `mut` 字段）
2. `10033–10035` 的类型 / 可写性 / case 地址 / 字段落点已给，语义栏标 ❓ 并指向 RV2；
   **不去追它们的含义**（那是 RV2 的事，越界即判据不达标）
3. **每个统计写明分母与所算字段**；「全部/一律」类宣称附穷举 + **negative set**
4. 与 `02-vm-core.md` §3.4、`03-math-jumps.md` §3/§7.4/§9 逐条衔接，冲突处以本篇一手为准并写明
5. 断言**回流** `engine/anm/OVERVIEW.md`（这条是波 D 模板的必备项，本篇先做示范）；
   顺带把该文件开头「⚠️ 本子系统尚未系统开工」的过期状态改掉
6. `python3 tooling/check-docs.py` 四项全绿；`04-variables.md` 加进 `engine/anm/th16/README.md`
   的「计划产出」清单（**`--orphans` 查不出这类遗漏**，见该 README 的警告）；命名追加进
   `naming-batch.md`（**别覆盖**）

---

### RV2：`mystery-angle-x/y/z`（`10033–10035`）专项

**开放式研究任务。可与波 D 并行 —— 它不产出波 D 需要的东西。**

**为什么单开**：这三条是 thpages 标 `wip:2` 的，ExpHP 原话
"Is somehow related to rotation? Little is known."。要下结论就得过 `METHOD.md` 的
**额外闸门**（一手到底 / 对抗证伪 / 量纲常识 / 交叉对名），**没有收敛保证**。
把它和 RV1 的 36 行表放在一起，会让有界的那半被无界的这半劫持。

**你手上有 ExpHP 没有的东西**：真实用法数据。`10035` 有 **7 次读、48 次写**，
`10033`/`10034` 各 **6 次读**（全库 54 个 `.anm`）。现成工具：
`runtime/anm/src/format/anm-v8.js` 的 `parseAnm(bytes)`，指令对象 `{time, opcode, paramMask, args}`。
`03-math-jumps.md` 反复证明了「拿真实数据跑统计去**证伪**竞争假设」比纯读反编译强得多 ——
它就是这么推翻「跳转 `t` 取自目标标签」的。

**方法建议**（不是硬性步骤）：
① 从 RV1 已确认的 case 地址出发，看这三个号读/写的是 vm 的哪个字段，再用 xref 找谁还碰那个字段；
② 拿 48 次写 + 19 次读的**上下文**做统计——写它们的是哪些 opcode、紧邻指令是什么、
   同一 script 里和 `rot-x/y/z`（`10023–10025`）、`rot-z-effective`（`10026`）如何共现；
③ 列出竞争假设并逐个证伪（例：「是 rot 的另一份拷贝」「是父级累积角」「是渲染用的欧拉角缓存」），
   **把证伪过程和 negative set 都写出来**。

**产出**：追加进 `engine/anm/th16/04-variables.md` 的一个专项小节（不新开文件），
并把表里那三行的语义与可信度就地更新。

**完成判据（刻意放松）**：
1. **给出结论并过额外闸门，或诚实标 ❓ 并写清「排除了哪些假设、依据是什么」** —— 后者同样算完成；
   **结论若与 thpages `wip:2` 冲突，必须列出冲突点 + 一手证据**（这是额外闸门的落点）
2. 统计写明分母与所算字段；穷举类宣称附 negative set
3. `check-docs.py` 四项全绿

**做不出来不算失败。** 诚实的 ❓ + 一份「已排除假设」清单，对下一个人的价值高于硬凑的结论。

---

### IV：变量表实现（把 `resolveArg` 从 8 槽扩到 22 槽）

**Files:**
- Modify: `runtime/anm/src/vm/core.js`（`resolveArg` / `writeIntVar` / `writeFloatVar`）
- Modify: `runtime/anm/tests/vm-core.test.js`、`runtime/anm/OVERVIEW.md`
- Test: 新增 `runtime/anm/tests/vm-vars.test.js`

**Interfaces:**
- Consumes: `engine/anm/th16/04-variables.md` 的 36 行表（**RV1 产出即可，不必等 RV2**）
- Produces: `resolveArg` / `isVarBit` 签名**不变**，只扩定义域；只读变量写入时抛错

- [ ] **步骤 1：按 04 的表扩读侧到 36 个号、写侧到 22 个可写号**，只读的 14 个在写入时抛带变量名的错。
      **落点要真接上 vm 字段**（`10013–10015` → `vm.pos`、`10023–10025` → `vm.rotation` 等），
      不是各存一份影子状态。**camera/lookat 族（10016–10021）若属播放器不建模的引擎全局态，
      显式列进 `core.js` 的「不建模」清单并抛，不要静默返 0。**
- [ ] **步骤 2：回归验证**——重跑收口审阅那个实验（I1 解析器 + 骨架 + Math 组表跑 54 个真实文件、
      逐条调 handler），**把 42.7% 的抛出率降下来并把新数字写进报告与 OVERVIEW**。
      这是本任务唯一有意义的验收指标。
- [ ] **步骤 3：故意改坏验证**——至少两处（一个只读变量的写保护、一个新增槽的读落点），
      确认对应测试变红，实际输出进报告。
- [ ] **步骤 4**：`npm test` 全绿、`check-docs.py` 绿、更新 `runtime/anm/OVERVIEW.md`
      的「已知接口缺口」块（缺口缩小后要如实改写，别留旧数字）、按路径 `git add` 提交。

---

## 波 D — 五组并行（★ 同时派发五个 agent）

以下五个任务**互不依赖**，全部读 HTML 导出，**任何一个都不要开 MCP**。
每个都以 R3 的 `03-math-jumps.md` 为**格式范例**。

统一的完成判据（五个都适用）：

1. 本组每条 opcode 有：opcode 号 / thpages 助记 / 签名 / 一手语义 / case 地址 / 可信度
2. 与 thpages 不一致处单独开节裁决；社区标 `wip:1` 的补全、`wip:2` 的按 `METHOD.md` 额外闸门处理，
   **复核前一律 🟡**
3. 表格单元格 ≤ 200 字符；`check-docs.py` 绿
4. 想要的 Ghidra 命名写进 `engine/anm/th16/naming-batch.md`（**追加，不要覆盖别人的**）

### R4：System 组（0–99，8 条）→ `engine/anm/th16/05-system.md`

含社区 `wip:2` 的 `4 stop2`。重点：与 `1 delete` / `2 static` / `3 stop` 的生死语义如何区分
（⚠️ `0` 是 `nop`，v8 无 opcode 15；R2 已在 `02-vm-core.md` §6.2 给出一手，本组据此复核）——
这条直接决定 I2 的骨架对不对，若与 `02-vm-core.md` 冲突，**以本任务的一手为准并回改 02**。

### R5：Settings 组（300–399，18 条）→ `engine/anm/th16/06-settings.md`

社区完成度最低的一组：11 定稿 / 4 `wip:1` / 3 `wip:2`。
`wip:2` = `306 v8-306`、`316 v8-flag-316`、`317 v8-flag-317`；
`wip:1` = `305 noZBuffer`、`307 v8-randMode`、`313 resolutionMode`、`314 attached`。
三个 flag 类的重点是**它们置的是 vm 的哪个位、那个位被谁读**——用 xref 反查读取点，比盲读 case 体有效。
⚠️ `307 v8-randMode` 涉及 RNG，注意 `engine/_shared/math-and-prng.md` 已解出的双流语义（replay-safe / unsafe）。

### R6：General 组（400–499，39 条）→ `engine/anm/th16/07-general.md`

条数最多。含 `wip:2` 的 `422 posAdopt`、`431 v8-flag-431`，`wip:1` 的 `433 posTime2D`、`438 originMode`。
重点：插值类指令（`posTime` 家族）的**缓动曲线编号与公式**——这是画面对不对的主因，
且与 `engine/_shared/math-and-prng.md` 的运动积分器可能同源，交叉引用。

### R7：Child management 组（500–599，10 条）→ `engine/anm/th16/08-child.md`

6/10 是 `wip:1`（`502 prependChild`、`503 prependChildUi`、`504 createRoot`、
`505 createChildPos`、`506 createRootPos`、`508 createEffect`）——社区完成度按比例最低的一组。
重点：父子 vm 的**坐标继承规则**与**生命周期**（父死子怎么办），以及 world 列表与 UI 列表的区别
（`engine/anm/th18/02-render-stages.md` 记了 `th18:0x488260` 的层号折叠，th16 应同构但**需自证**）。

### R8：Drawing 组（600–699，14 条）+ render state 一手扫描 → `engine/anm/th16/09-drawing.md`

两件事，一个 agent 做完：

1. **14 条 opcode**（thpages 全部定稿，风险最低的一半）
2. **render state 扫描**（spec §10.4 的第二样交付物）：把 ANM 绘制路径上的
   `SetRenderState` / `SetTextureStageState` 调用点扫一遍，回答「ANM 到底设了哪些状态」。
   这是**引擎结论**，进 `engine/`；据此做的 WebGL 取舍是实现决策，由 I8 写进 `runtime/`。
   目标是把 spec §10.3 映射表里 alpha test / texture stage states / blend modes 三行的 🟡 降掉。

---

## 波 E — 按 spec §6 的组顺序实现

I4–I8 结构与 I3 完全同构，逐个来。每个任务：

- **Files**：`runtime/anm/src/vm/group-<组>.js` + `runtime/anm/tests/vm-<组>.test.js` + 夹具
- **Interfaces**：Consumes `registerGroup`/`createVm`/`stepFrame` 与对应的 `engine/anm/th16/0N-*.md`；
  Produces 该组的 handler 表，导出名逐一写死如下（不要自创）：
  `mathJumpsGroup`（I3）· `systemGroup`（I4）· `settingsGroup`（I5）· `generalGroup`（I6）·
  `childGroup`（I7）· `drawGroup`（I8）
- **步骤**：夹具 → 失败测试 → 确认失败 → 按文档实现 → 确认通过 → 更新状态矩阵 → 提交
- **必须有的那条测试**：「本组 opcode 全部注册」计数断言（同 I3 步骤 2 最后一例），
  数字取自对应文档的表行数

| 任务 | 组 | 条数 | 依据文档 | 额外要求 |
| --- | --- | ---: | --- | --- |
| I4 | System | 8 | `05-system.md` | 若 R4 的生死语义与 `02-vm-core.md` 冲突，**先回改 `core.js` 与 02**，再实现本组 |
| I5 | Settings | 18 | `06-settings.md` | flag 类只置位；「谁读这个位」的行为留给读取方，本组不越界实现 |
| I6 | General | 39 | `07-general.md` | 插值家族要有**逐帧数值**测试，不能只测终值 |
| I7 | Child mgmt | 10 | `08-child.md` | 需要 `core.js` 支持父子 vm 列表——若 I2 没做，本任务补，并回填 `02-vm-core.md` |
| I8 | Drawing + 渲染后端 | 14 | `09-drawing.md` | 见下 |

**I8 额外产出**（这个任务比其他四个大，但不拆——渲染后端与 Drawing 组的状态互为验证）：

- `runtime/anm/src/render/webgl.js`：WebGL2 后端。四相机（`LookAtLH` + `PerspectiveFovLH` 照抄公式）、
  渲染阶段、混合模式、精灵四边形。
- `runtime/anm/src/render/D3D9-MAPPING.md`：逐条「D3D9 语义 / WebGL 对应 / 差异 / 处置」，
  以 spec §10.3 的六行为起点，用 R8 的扫描结果把三行 🟡 落实。
- **`group-draw.js` 只置状态，不发 draw call**——保持层 A / 层 B 分离（spec §10.1）。
  这样 Drawing 组的 opcode 可以在 Node 里无 GL 上下文测试。

---

## 波 F — 收尾

### I9：查看器页面

**Files:** Create `runtime/anm/web/index.html`（+ 必要的 `web/*.js`）

功能：加载本地 `.anm`（`<input type=file>`，不打包任何版权数据）、选 entry / script、
播放 / 暂停 / 逐帧 / 拖时间轴、**右栏实时显示当前 vm 的全部字段**。

最后一条是重点：它把「层 B 掩盖层 A 错误」这个风险（spec §11）压下去一大半——
肉眼看画面看不出的偏差，看 `pos` / `scale` 的数值能看出。

**完成判据**：能加载 `local/th16.v1.00a/dat/` 里的真实 `.anm` 并播放；vm 字段面板随帧更新。

### T10：命名落库 + 断言矩阵 + 验收

- [ ] **步骤 1：把 `naming-batch.md` 落进 Ghidra**（此时才开 MCP，独占无妨）

```bash
# 确认没有别的东西占锁，然后用 MCP 逐条 rename_function / create_structure / rename_address
tooling/ghidra/symbols.py status th16
tooling/ghidra/symbols.py export th16          # ⚠️ 不导出就等于没有
git add games/th16.v1.00a/symbols.json
```

- [ ] **步骤 2：扩 `engine/anm/OVERVIEW.md` 的断言矩阵**——从现在的 6 行扩到覆盖 VM 语义
      （格式、驱动、时间、interrupt、父子、各组语义、渲染阶段），th16 列不再全 🟡。

- [ ] **步骤 3：逐条核对 spec §12 验收标准**

```bash
cd runtime/anm && npm test                     # 全绿
cd ../.. && python3 tooling/check-docs.py      # 四项全绿
```

1. 播放真实 th16 `.anm` 的**非平凡 script**（含时间标签跳转、interrupt、child vm 三者），
   与原版同帧截图比对无可见差异
2. 135 条 opcode 在 `engine/anm/th16/` 有表且带可信度；**统计 ✅ 占比并写进 OVERVIEW**
3. `engine/anm/OVERVIEW.md` 矩阵已扩
4. `check-docs.py` 绿；夹具可重跑且不含版权字节
5. th18 列——由 R11 点亮

- [ ] **步骤 4：提交。**

### R11：th18 补差

**目标**：把 v8 结论从 th16 移到 th18，点亮矩阵的 th18 列。

**两件事**：
1. 新增两条 opcode 的一手：`439 fadeNearCamera`、`614 drawArc`（社区对 `439` 只有 th185 的
   `vd-imaginary-439` 记录，**th18 的 439 是不同的指令**，别对错号）
2. **地址对账**：th16 的 case 地址搬到 th18（th18 库里有 136 条 anm label 可用）；
   逐条确认语义未变——spec §3 说同号异义为零，但那是**社区表**的结论，属社区单源，
   需抽样一手复核后才能把 th18 列标 ✅

**产出**：`engine/anm/th18/03-opcode-delta.md` + `engine/anm/OVERVIEW.md` 的 th18 列
**完成判据**：两条新 opcode 有地址与语义；抽样复核的样本量与结论写明；`check-docs.py` 绿

---

## 自查

**Spec 覆盖**：§4 纪律→T0 步骤 3；§5 目录→文件结构表；§6 阶段与七组→R1/R2/R3/R4–R8 与波 E；
§7 不做→本计划无 ECL/STD/音效任务，trace harness 未列（backlog）；§8 测试与版权→I1 步骤 1、
Global Constraints；§9 技术栈→Tech Stack；§10.1 两层→I8「只置状态不发 draw call」；
§10.3/§10.4 →R8 与 I8；§10.5 →T10 步骤 3 第 1 条用「无可见差异」；§11 风险→I3 步骤 6 记耗时、
I9 的 vm 字段面板、R8 降 🟡、T0 的层次纪律；§12 验收→T10 步骤 3。

**已知缺口（有意留白，非遗漏）**：trace harness 按 spec §7 是 backlog，触发条件在波 E；
真到那时另开计划，不在本计划内预写。

**类型一致性**：`parseAnm` / `createVm` / `stepFrame` / `registerGroup` 四个签名在 I1、I2 定义，
I3–I8 全部按同一签名消费；各组导出名在波 E 表前逐一写死（`mathJumpsGroup` / `systemGroup` / `settingsGroup` /
`generalGroup` / `childGroup` / `drawGroup`），避免 I3 叫一个名、I7 叫另一个。
