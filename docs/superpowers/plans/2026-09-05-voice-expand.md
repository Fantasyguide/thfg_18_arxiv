# TH18 音效表扩容（card-expand 语音）实施计划

> **版本**：TH18 v1.00a（`th18.exe`，imagebase `0x400000`）。本文裸地址默认属该版本；引用其他版本须写成 `th16:0x…`。

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 把 TH18 写死 84 槽的音效表整体搬到 thcrap codecave 并加长到 116 槽，让 card-expand 能注册 32 个自定义音效 id（`0x54`–`0x73`），第一批装角色语音。

**Architecture:** 四张原生表（cfg 配置表 / wav 名表 / slot 数组 / blob 指针数组）全部重定位到具名 RW codecave；51 处 `.text` 立即数由 `sites.py` 从**真 exe** 现扫现改，patch 里只有地址与源码。表的**内容**在 `th18_snd_patch_init`（`codecaves_apply` 末尾）从用户自己那份 exe `rep movsd` 拷过来 —— 仓库不留任何 ZUN 字节。DLL 在 `0x476410`（SoundManager 初始化）入口的断点里填语音 blob 与新行配置，然后放行让引擎按原生流程建 DirectSound buffer。

**Tech Stack:** Python 3（`sites.py` / `x86imm.py` 站点扫描与 binhack 生成）、thcrap binhack/codecave/breakpoint、i686 mingw C（`th18_card_expand.dll`）、主机 gcc 单测、pyghidra（只在 Task 1 取证）。

**Spec:** `docs/superpowers/specs/2026-09-05-voice-expand-design.md`

## Global Constraints

- **版本死绑 TH18 v1.00a**（`th18.exe`，imagebase `0x400000`）。所有裸地址属该版本。
- **patch 里不留任何 ZUN 字节。** cfg 表的 84 行内容、72 个 wav 名字符串一律运行时从用户的 exe 拷。
- **不认识就报错，绝不猜**（`x86imm.py` 的既定原则）：未覆盖的指令编码抛 `UnknownEncoding`，不许静默跳过。
- **全有或全无**：任何自检 `FAIL` 都必须还原到原生行为（新 id 不可用），不能半开。
- **地址写法** `` `0x476410` ``（反引号、无 `@`）；文档改完跑 `python tooling/check-docs.py`。
- **每一处写入点过 `mods/_template/AUDIT-checklist.md`**，追加到 `mods/th18.v1.00a/card-expand/AUDIT.md` 新的一节（§Q）。
- **两条行不变式**（违反即挂死，见 spec §3.4.1）：
  - **I1** 116 行的 `+0` 两两不同且恰好覆盖 `0..0x73`（循环 1 的扫描无上界，找不到就跑飞）。
  - **I2** 116 行的 `+4` 指向的 blob 槽都非 NULL（`0x4776f0` 遇 NULL 进 `Sleep(10)` 死等）。
- **不改 `0x4767d8`**（`CMP ESI,0x48`）：预加载线程仍只从 dat 读原生 72 个。
- 新 id 空间 `0x54`–`0x73`（`CE_SND_NEW_N = 32`）。

**工作目录**：除 Task 1 外，全部在 `mods/th18.v1.00a/card-expand/native/`。

---

### Task 1: 引擎一手订正 —— 重写 `th18-sound-table.md`

现有文档说表是 `0x52` 行、缺 id `0x52`/`0x53`、没有 slot 字段图。后面每个 Task 都要引用它，先修对。

**Files:**
- Modify: `engine/_shared/th18-sound-table.md`（整篇重写）

**Interfaces:**
- Produces: 下游 Task 2–8 引用的一手结论 —— 表 84 行、slot 元素字段图、两条链路、I1/I2 的成因。

- [ ] **Step 1: 用脚本重新生成 id → wav 全表（84 行），确认与现有 82 行只差 `0x52`/`0x53`**

```bash
cd /data/sunyunbo/www/renkolab
python3 - <<'EOF'
import struct
exe=open('local/th18.v1.00a/th18.exe','rb').read()
def v2f(v):
    for va,vs,ptr,rs in [(0x1000,0xab7ca,0x400,0xab800),(0xad000,0x15d58,0xabc00,0x15e00),(0xc3000,0xae4c4,0xc1a00,0xb800)]:
        o=v-0x400000
        if va<=o<va+max(vs,rs): return ptr+(o-va)
names=[]
for i in range(72):
    p=struct.unpack_from('<I',exe,v2f(0x4b47a0)+i*4)[0]
    f=v2f(p); names.append(exe[f:exe.index(b'\0',f)].decode())
rows=[struct.unpack_from('<5I',exe,v2f(0x4c9b80)+i*0x14) for i in range(0x54)]
perm={r[0]:(names[r[1]], r[2]>>16, r[2]&0xffff, r[4]) for r in rows}
assert sorted(perm)==list(range(0x54)), "I1 在原生就不成立？"
for i in range(0x54):
    w,vol,pan,f10=perm[i]
    print(f"| `0x{i:02x}` | `{w}` | {vol} | 0x{pan:04x} | {f10} |")
EOF
```

预期：84 行；`0x52` = `se_trophy.wav`、`0x53` = `se_notice.wav`；`assert` 不触发（原生就满足 I1）。

- [ ] **Step 2: 重写文档**

`engine/_shared/th18-sound-table.md` 的新目录（内容照 spec §1 搬，**加**日期与新证据）：

```markdown
# th18 音效 id 表（`play_sound(id)` → `se_*.wav`）

> **版本**：TH18 v1.00a（`th18.exe`，imagebase `0x400000`）。本文裸地址默认属该版本；引用其他版本须写成 `th16:0x…`。

## 0. 结论
## 1. SoundManager 是静态全局 `0x56ad7c`（字段表：设备 / 队列 / slot 数组 / blob 数组 / 音量）
## 2. 五张表（cfg / wav 名 / blob / slot / 队列）
## 3. slot 元素字段图（0x18 字节，六个字段各带证据地址）
## 4. id → wav 的映射是置换（82/82 比对证据）
## 5. 两条链路（播放 / 预加载）
## 6. 两条不变式 I1 / I2 及其成因
## 7. 全表（84 行：id / wav / 音量 / 声像 / `+0x10`）
## 8. 怎么再来一遍
```

订正点必须写明：**表是 `0x54` = 84 行不是 `0x52`**（界 `0x476472` `CMP EDX,0x54`、`0x4766ef` `CMP ESI,0x4ca214`），
**补 id `0x52` / `0x53`**（两者都有静态调用点 `0x4192c7` / `0x45653f`，不是空闲槽）。

- [ ] **Step 3: 跑文档检查**

Run: `python tooling/check-docs.py`
Expected: `✓ 全部通过`

- [ ] **Step 4: Commit**

```bash
git add engine/_shared/th18-sound-table.md docs/superpowers/specs/2026-09-05-voice-expand-design.md docs/superpowers/plans/2026-09-05-voice-expand.md
git commit -m "docs(sound): th18 音效表订正为 84 行（补 id 0x52/0x53）+ slot 字段图 + 两条不变式；语音扩表设计稿与实施计划"
```

---

### Task 2: `sound_sites.py` —— 52 处站点的扫描、分类与完整性审计

先只做**只读的扫描器**：把 52 处站点从真 exe 里认出来、归类、并证明没有漏网之鱼。不产出 patch。

**Files:**
- Create: `native/sound_sites.py`
- Test: `native/tests/test_sound_sites.py`
- Modify: `native/Makefile`（加 `snd-check` 目标）

**Interfaces:**
- Consumes: `native/x86imm.py` 的 `classify(text, off, text_va, ...)`（返回 `{"va","len","kind","text"}`，不认识抛 `UnknownEncoding`）。
- Produces:
  - `SND_REFS`：`tuple[(va:int, cave:str, off:int, title:str)]`，49 项
  - `SND_COUNTS`：`tuple[(va:int, old:int, new:int, title:str)]`，2 项
  - `SND_UNTOUCHED`：`tuple[(va:int, value:int, why:str)]`，1 项
  - `scan(text, text_va) -> list[dict]`：每项 `{"va","pre","old","cave","off","title","len"}`
  - `audit(text, text_va, covered) -> list[tuple]`：撞上四段地址区间但不在站点内的位置
  - 常量 `CFG_BASE/CFG_ROWS/NEW_N/CAVE_*`，供 Task 3–5 import

- [ ] **Step 1: 写站点表与常量（`native/sound_sites.py`）**

```python
"""sound_sites.py —— TH18 音效表扩容的站点表与扫描器。

四张原生表要整体搬进 codecave，`.text` 里 52 处引用它们。本模块只回答两件事：
  ① 这 52 处每一处在 exe 里长什么样、该换成什么（scan）
  ② 除了这 52 处，还有没有别的地方撞上这些地址（audit）

★ 不认识就报错。x86imm.classify 抛异常时**不许 except 掉**——
  静默跳过一个站点 = 表搬了一半，且不崩不报错。
"""
import struct
from x86imm import classify, UnknownEncoding, Ambiguous     # noqa: F401

IMAGEBASE = 0x400000

# ---- 原生形状（一手：engine/_shared/th18-sound-table.md）--------------------
CFG_BASE   = 0x4c9b80; CFG_ROW = 0x14; CFG_ROWS = 84
CFG_END    = CFG_BASE + CFG_ROWS * CFG_ROW            # 0x4ca214
NAMES_BASE = 0x4b47a0; NAMES_N = 72                   # 72 项 + NULL 终止
SLOT_BASE  = 0x56c804; SLOT_SIZE = 0x18; SLOT_N = 84
SLOT_END   = SLOT_BASE + SLOT_N * SLOT_SIZE           # 0x56cfe4
BLOB_BASE  = 0x56cfe4; BLOB_N = 72
BLOB_END   = BLOB_BASE + BLOB_N * 4                   # 0x56d104

# ---- 扩容后 ---------------------------------------------------------------
NEW_N      = 32                                       # 新 id 0x54..0x73
CFG_ROWS_N = CFG_ROWS + NEW_N                         # 116
NAMES_N_N  = NAMES_N + NEW_N                          # 104
CAVE_CFG   = "th18_snd_cfg"                           # 116 * 0x14 = 0x910
CAVE_NAMES = "th18_snd_names"                         # (104+1) * 4 = 0x1a4
CAVE_SLOTS = "th18_snd_slots"                         # 116 * 0x18 = 0xae0
CAVE_BLOBS = "th18_snd_blobs"                         # 104 * 4    = 0x1a0
CAVE_INIT  = "th18_snd_patch_init"
CFG_RVA    = CFG_BASE   - IMAGEBASE                   # 0xc9b80
NAMES_RVA  = NAMES_BASE - IMAGEBASE                   # 0xb47a0

CAVE_SIZE = {
    CAVE_CFG:   CFG_ROWS_N * CFG_ROW,
    CAVE_NAMES: (NAMES_N_N + 1) * 4,
    CAVE_SLOTS: CFG_ROWS_N * SLOT_SIZE,
    CAVE_BLOBS: NAMES_N_N * 4,
}

# ---- 49 处「换成 <codecave:X+off>」的引用 ---------------------------------
# off = 新表内的字节偏移。尾界站点的 off = 新表的（有效）长度。
SND_REFS = (
    # cfg 表（6 引用 + 1 尾界）
    (0x40111a, CAVE_CFG, 0x0, "pre-main 扫描起点"),
    (0x476457, CAVE_CFG, 0x0, "init 循环1 扫描起点"),
    (0x4766bd, CAVE_CFG, 0x4, "init 循环2 游标（行+4）"),
    (0x476716, CAVE_CFG, 0x4, "init 错误路径取 wav 名下标"),
    (0x476bea, CAVE_CFG, 0xa, "play_sound 两参版取音量"),
    (0x476c8d, CAVE_CFG, 0xa, "play_sound 三参版取音量"),
    (0x4766ef, CAVE_CFG, CFG_ROWS_N * CFG_ROW, "init 循环2 尾界"),
    # slot 数组（25 引用 + 3 尾界）
    (0x401110, CAVE_SLOTS, 0x4,  "pre-main 写 +4 = -1"),
    (0x401129, CAVE_SLOTS, 0xc,  "pre-main 写 +0xc = 槽号"),
    (0x401130, CAVE_SLOTS, 0x8,  "pre-main 写 +8 = &cfg 行"),
    (0x444d8f, CAVE_SLOTS, 0x0,  "stop-all 起点"),
    (0x45a4a1, CAVE_SLOTS, 0x8,  "设备恢复重播循环起点"),
    (0x45ff38, CAVE_SLOTS, 20 * SLOT_SIZE, "★ 硬编码槽 20（se_lazer02 常驻激光音）"),
    (0x471393, CAVE_SLOTS, 0x0,  "WinMain 释放起点"),
    (0x4766b8, CAVE_SLOTS, 0x0,  "init 循环2 起点"),
    (0x476c60, CAVE_SLOTS, 0x4,  "play_sound 两参版写音量"),
    (0x476d06, CAVE_SLOTS, 0x4,  "play_sound 三参版写音量"),
    (0x477533, CAVE_SLOTS, 0x0,  "消费者读 buffer"),
    (0x477562, CAVE_SLOTS, 0x0,  "消费者读 buffer"),
    (0x4775aa, CAVE_SLOTS, 0x0,  "消费者读 buffer"),
    (0x4775bf, CAVE_SLOTS, 0x0,  "消费者读 buffer"),
    (0x4775ce, CAVE_SLOTS, 0x0,  "消费者读 buffer"),
    (0x4775fa, CAVE_SLOTS, 0x0,  "消费者读 buffer"),
    (0x477652, CAVE_SLOTS, 0x0,  "消费者读 buffer"),
    (0x47766b, CAVE_SLOTS, 0x0,  "消费者读 buffer"),
    (0x4777c0, CAVE_SLOTS, 0x0,  "DuplicateSoundBuffer 源"),
    (0x4775f3, CAVE_SLOTS, 0x8,  "消费者读 &cfg 行"),
    (0x477664, CAVE_SLOTS, 0x8,  "消费者读 &cfg 行"),
    (0x477736, CAVE_SLOTS, 0x8,  "0x4776f0 去重扫描起点"),
    (0x4775dc, CAVE_SLOTS, 0x10, "消费者写播放态"),
    (0x47753a, CAVE_SLOTS, 0x14, "消费者写播放位"),
    (0x47755b, CAVE_SLOTS, 0x14, "消费者写播放位"),
    (0x444dbf, CAVE_SLOTS, CFG_ROWS_N * SLOT_SIZE,       "stop-all 尾界"),
    (0x4713ad, CAVE_SLOTS, CFG_ROWS_N * SLOT_SIZE,       "WinMain 释放尾界"),
    (0x45a4c5, CAVE_SLOTS, CFG_ROWS_N * SLOT_SIZE + 0x8, "重播循环尾界（+8 变体）"),
    # blob 数组（9 引用 + 1 尾界）
    (0x4713b5, CAVE_BLOBS, 0x0, "WinMain 释放起点"),
    (0x4767cc, CAVE_BLOBS, 0x0, "预加载线程写 blob"),
    (0x477758, CAVE_BLOBS, 0x0, "0x4776f0 判空"),
    (0x47777b, CAVE_BLOBS, 0x0, "0x4776f0 判空（等待中）"),
    (0x477788, CAVE_BLOBS, 0x0, "0x4776f0 取 blob"),
    (0x477905, CAVE_BLOBS, 0x0, "0x4776f0 取 blob"),
    (0x47791f, CAVE_BLOBS, 0x0, "0x4776f0 清 blob"),
    (0x477956, CAVE_BLOBS, 0x0, "0x4776f0 取 blob"),
    (0x477970, CAVE_BLOBS, 0x0, "0x4776f0 清 blob"),
    # ★ 释放尾界刻意只到原生 72：语音 blob 来自 thcrap 的堆，交给游戏的 free 会崩。
    #   进程此刻正在退出，不释放到此为止。见 spec §3.3 E。
    (0x4713d8, CAVE_BLOBS, BLOB_N * 4, "WinMain 释放尾界（★ 只到原生 72）"),
    # wav 名表（4 引用，无尾界——预加载的界是计数，见 SND_UNTOUCHED）
    (0x4766d3, CAVE_NAMES, 0x0, "init 加载取名"),
    (0x47671d, CAVE_NAMES, 0x0, "init 错误路径取名"),
    (0x4767bc, CAVE_NAMES, 0x0, "预加载取名"),
    (0x476803, CAVE_NAMES, 0x0, "预加载错误路径取名"),
)

# ---- 2 处「换立即数」的计数界 ---------------------------------------------
SND_COUNTS = (
    (0x401139, SLOT_N * SLOT_SIZE, CFG_ROWS_N * SLOT_SIZE, "pre-main slot 初始化字节界"),
    (0x476472, SLOT_N,             CFG_ROWS_N,             "init 循环1 槽数界"),
)

# ---- 1 处刻意不改 ---------------------------------------------------------
SND_UNTOUCHED = (
    (0x4767d8, NAMES_N, "预加载 wav 数：仍只从 dat 读原生 72 个，语音 blob 由 DLL 填"),
)

TABLE_RANGES = {          # 完整性审计要扫的四段原生地址
    CAVE_CFG:   (CFG_BASE,   CFG_END + 4),
    CAVE_NAMES: (NAMES_BASE, NAMES_BASE + (NAMES_N + 1) * 4),
    CAVE_SLOTS: (SLOT_BASE,  SLOT_END + 0xc),
    CAVE_BLOBS: (BLOB_BASE,  BLOB_END + 4),
}


class ShapeError(Exception):
    pass
```

- [ ] **Step 2: 写 `scan()` 与 `audit()`**

追加到 `native/sound_sites.py`：

```python
def scan(text, text_va):
    """把 SND_REFS / SND_COUNTS 逐条对回 exe：确认那里确实是一条以该 4 字节收尾的指令。

    返回 [{"va","pre","old","cave","off","title","len","kind"}]，cave=None 表示纯立即数界。
    """
    out = []
    for va, cave, off, title in SND_REFS:
        p = va - text_va
        info = classify(text, p, text_va, 0)          # 不认识 → 抛，别接
        ins_va, n = info["va"], info["len"]
        raw = text[ins_va - text_va:ins_va - text_va + n]
        if len(raw) < 4:
            raise ShapeError("0x%06x：指令只有 %d 字节" % (va, n))
        old = struct.unpack("<I", raw[-4:])[0]
        lo, hi = TABLE_RANGES[cave]
        if not (lo <= old < hi):
            raise ShapeError("0x%06x：末 4 字节 0x%x 不在 %s 的区间内" % (va, old, cave))
        out.append({"va": ins_va, "pre": raw[:-4], "old": old, "cave": cave,
                    "off": off, "title": title, "len": n, "kind": info["kind"]})
    for va, old, new, title in SND_COUNTS:
        p = va - text_va
        info = classify(text, p, text_va, 0)
        ins_va, n = info["va"], info["len"]
        raw = text[ins_va - text_va:ins_va - text_va + n]
        got = struct.unpack("<I", raw[-4:])[0]
        if got != old:
            raise ShapeError("0x%06x：计数界是 0x%x，表里写的是 0x%x" % (va, got, old))
        out.append({"va": ins_va, "pre": raw[:-4], "old": old, "cave": None,
                    "off": new, "title": title, "len": n, "kind": info["kind"]})
    for va, val, why in SND_UNTOUCHED:
        info = classify(text, va - text_va, text_va, 0)
        raw = text[info["va"] - text_va:info["va"] - text_va + info["len"]]
        got = struct.unpack("<I", raw[-4:])[0] if info["len"] >= 4 else None
        if got != val:
            raise ShapeError("0x%06x（刻意不改）：值是 %s，应为 0x%x —— %s" % (va, got, val, why))
    return out


def audit(text, text_va, covered):
    """扫四段原生地址区间里所有 4 字节出现处，列出不在已覆盖站点内的。

    输出必须由人逐条确认「漏掉的都是该漏的」：同值的热全局（如 0x4ca214 本身就是
    一个被 0x401e0e / 0x474b45 读写的普通 int 全局）不该改。
    """
    out = []
    for cave, (lo, hi) in TABLE_RANGES.items():
        for v in range(lo, hi, 4):
            needle = struct.pack("<I", v)
            p = text.find(needle)
            while p >= 0:
                if (text_va + p) not in covered:
                    try:
                        info = classify(text, p, text_va, 0)
                        desc, kind = info["text"], info["kind"]
                    except (UnknownEncoding, Ambiguous):
                        desc, kind = "(无法归类)", "?"
                    out.append((text_va + p, v, cave, kind, desc))
                p = text.find(needle, p + 1)
    out.sort()
    return out
```

- [ ] **Step 3: 写自检测试（`native/tests/test_sound_sites.py`）**

```python
"""test_sound_sites.py —— 拿真 exe 验站点表。跑：python3 tests/test_sound_sites.py"""
import os, sys
sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), ".."))
import sites, sound_sites as S

fail = []
def check(cond, msg):
    if not cond: fail.append(msg)

text, text_va, _ = sites.load_text_section(sites.EXE)   # 返回 3 元组

# ① 站点数：49 引用 + 2 计数 = 51 改写；另 1 处刻意不改
check(len(S.SND_REFS) == 49, "SND_REFS 应 49 项，实际 %d" % len(S.SND_REFS))
check(len(S.SND_COUNTS) == 2, "SND_COUNTS 应 2 项")
check(len(S.SND_UNTOUCHED) == 1, "SND_UNTOUCHED 应 1 项")

# ② 每一处都能在 exe 里对上（scan 内部会抛）
rec = S.scan(text, text_va)
check(len(rec) == 51, "scan 应返回 51 条，实际 %d" % len(rec))

# ③ 地址不重复
vas = [r["va"] for r in rec]
check(len(set(vas)) == len(vas), "站点地址有重复")

# ④ 尺寸算术
check(S.CAVE_SIZE[S.CAVE_CFG]   == 0x910, "cfg cave 尺寸")
check(S.CAVE_SIZE[S.CAVE_SLOTS] == 0xae0, "slot cave 尺寸")
check(S.CAVE_SIZE[S.CAVE_NAMES] == 0x1a4, "names cave 尺寸")
check(S.CAVE_SIZE[S.CAVE_BLOBS] == 0x1a0, "blob cave 尺寸")

# ⑤ 完整性审计：未覆盖的必须全在白名单里
covered = set()
for r in rec:
    for k in range(r["len"]): covered.add(r["va"] + k)
WHITELIST = {                       # 同值不同物，不许改
    0x401e0e: "0x4ca214 是普通 int 全局（写）",
    0x474b45: "0x4ca214 是普通 int 全局（写）",
}
un = S.audit(text, text_va, covered)
for va, v, cave, kind, desc in un:
    check(va in WHITELIST, "未覆盖站点 0x%06x（值 0x%x，%s）：%s" % (va, v, cave, desc))
check(set(WHITELIST) <= {u[0] for u in un}, "白名单里有条目没被扫到，说明形状变了")

print("sound_sites: %d passed, %d failed" % (5 + len(un) - len(fail), len(fail)))
for m in fail: print("  FAIL", m)
sys.exit(1 if fail else 0)
```

- [ ] **Step 4: 跑测试，确认失败点是「白名单不完整」而不是崩溃**

Run: `cd native && python3 tests/test_sound_sites.py`
Expected: 第一次多半会列出若干「未覆盖站点」。**逐条读 `desc`**：
- `mov [0x4ca214], ecx` 之类 = 同值热全局 → 加进 `WHITELIST` 并注明理由；
- `mov esi, 0x56c804` 之类落在表内的**真引用** → 是站点表漏了，补进 `SND_REFS`；
- `(无法归类)` → 补 `x86imm.py` 的编码规则，**不许 except 掉**。

反复直到通过。**每加一条白名单都要在注释里写清「为什么它不是站点」。**

- [ ] **Step 5: 加 Makefile 目标**

在 `native/Makefile` 的 `.PHONY` 行加 `snd-check`，并追加：

```makefile
## snd-check —— 音效表 51 处站点的扫描 + 完整性审计（只读，不产出）
snd-check:
	python3 tests/test_sound_sites.py
```

并把它挂进 `check`：`check: anm-verify snd-check`

- [ ] **Step 6: 跑通并 Commit**

Run: `cd native && make snd-check`
Expected: `sound_sites: N passed, 0 failed`，退出码 0

```bash
git add native/sound_sites.py native/tests/test_sound_sites.py native/Makefile
git commit -m "feat(sound): 音效表 51 处站点的站点表与完整性审计（sound_sites.py + make snd-check）"
```

---

### Task 3: codecave 声明与 `th18_snd_patch_init` 拷贝代码

四块 RW 内存 + 一段在 `codecaves_apply` 末尾运行的初始化代码：从用户的 exe 拷原生内容，给 32 个新行写结构骨架（满足 I1/I2）。

**Files:**
- Create: `native/sound_emit.py`
- Test: `native/tests/test_sound_emit.py`

**Interfaces:**
- Consumes: `sound_sites` 的 `CAVE_*` / `CAVE_SIZE` / `CFG_RVA` / `NAMES_RVA` / `CFG_ROWS` / `NEW_N` / `CFG_ROW`
- Produces: `emit_sound_codecaves() -> dict`，键是 cave 名，值是 thcrap codecave 对象；供 Task 4 合并进 `sites.py` 的 `doc["codecaves"]`

- [ ] **Step 1: 写 codecave 发射器（`native/sound_emit.py`）**

```python
"""sound_emit.py —— 音效表扩容的 codecave 与 binhack 发射器。

★ 版权红线：cfg 表的 84 行内容与 72 个 wav 名字符串是 ZUN 的数据，
  patch 里一个字节都不留 —— 一律在 patch_init 里从**用户自己那份 exe** rep movsd 拷。

★ 时机：`*_patch_init` 由 patch_func_init 调用，在 codecaves_apply 末尾
  （binhack.cpp:1724），**早于** binhacks_apply（runconfig.cpp:655-656）。
  「先把表填好、再让改过的代码去读它」正好落在这个缝里。
  ⚠️ 反过来它**不能**用来验证 binhack —— 它跑的时候一个 binhack 都还没打。

调用约定：`typedef void (TH_CDECL *mod_call_type)(void *param)`（plugin.h:71）
—— cdecl、一参、调用方清栈，所以结尾是裸 `ret`。pushad/popad 覆盖被调方要保留的寄存器。
"""
import struct
import sound_sites as S


def emit_sound_codecaves():
    assert S.CFG_ROWS * S.CFG_ROW % 4 == 0
    cfg_dwords   = S.CFG_ROWS * S.CFG_ROW // 4          # 84 * 0x14 / 4 = 420
    names_dwords = S.NAMES_N + 1                        # 72 项 + NULL 终止

    # 新行骨架：+0 = 84+k（满足 I1），+4/+8/+0xc/+0x10 = 0（+4 = 0 → 指向原生 wav 0，满足 I2）
    #   L: ab           stosd            ; [edi] = eax(槽号); edi += 4
    #      31 d2        xor edx, edx
    #      89 17        mov [edi], edx        ; +4
    #      89 57 04     mov [edi+4], edx      ; +8
    #      89 57 08     mov [edi+8], edx      ; +0xc
    #      89 57 0c     mov [edi+0xc], edx    ; +0x10
    #      83 c7 10     add edi, 0x10
    #      40           inc eax
    #      4b           dec ebx
    #      75 xx        jnz L
    body = "ab" "31d2" "8917" "895704" "895708" "89570c" "83c710" "40" "4b"
    body_len = len(body) // 2
    loop = body + "75%02x" % ((256 - (body_len + 2)) & 0xff)

    code = "".join([
        "fc",                                                  # cld
        "60",                                                  # pushad
        # ① 原生 84 行 cfg → cave
        "bf<codecave:%s>" % S.CAVE_CFG,                        # mov edi, cfg cave
        "be<Rx%x>" % S.CFG_RVA,                                # mov esi, 原生 cfg 表
        "b9%s" % struct.pack("<I", cfg_dwords).hex(),          # mov ecx, 420
        "f3a5",                                                # rep movsd
        # ② 32 个新行的结构骨架（edi 已指向 cave + 84*0x14）
        "b8%s" % struct.pack("<I", S.CFG_ROWS).hex(),          # mov eax, 84
        "bb%s" % struct.pack("<I", S.NEW_N).hex(),             # mov ebx, 32
        loop,
        # ③ 原生 72 个 wav 名指针 + NULL → cave
        "bf<codecave:%s>" % S.CAVE_NAMES,                      # mov edi, names cave
        "be<Rx%x>" % S.NAMES_RVA,                              # mov esi, 原生名表
        "b9%s" % struct.pack("<I", names_dwords).hex(),        # mov ecx, 73
        "f3a5",                                                # rep movsd
        "61",                                                  # popad
        "c3",                                                  # ret（cdecl，调用方清栈）
    ])

    caves = {name: {"size": "0x%x" % size} for name, size in S.CAVE_SIZE.items()}
    caves[S.CAVE_INIT] = {"code": code, "export": True,
                          "title": "音效表：从用户的 exe 拷原生 84 行 cfg + 72 个 wav 名，"
                                   "并给 32 个新行写 +0 = 槽号 的骨架（I1/I2）"}
    return caves
```

- [ ] **Step 2: 写发射器的单测（`native/tests/test_sound_emit.py`）**

```python
"""test_sound_emit.py —— codecave 发射器的形状检查。跑：python3 tests/test_sound_emit.py"""
import os, re, sys
sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), ".."))
import sound_sites as S
from sound_emit import emit_sound_codecaves

fail = []
def check(cond, msg):
    if not cond: fail.append(msg)

c = emit_sound_codecaves()
check(set(c) == set(S.CAVE_SIZE) | {S.CAVE_INIT}, "codecave 名集合不对：%s" % sorted(c))
check(c[S.CAVE_INIT]["export"] is True, "patch_init 必须 export")
check(S.CAVE_INIT.endswith("_patch_init"), "名字必须以 _patch_init 结尾才会被 thcrap 调用")

code = c[S.CAVE_INIT]["code"]
# ★ 版权红线：patch 里不许出现原生数据。允许的只有：hex 字节、<codecave:…>、<Rx…>
check("<Rx%x>" % S.CFG_RVA in code,   "cfg 必须用 Rx 相对模块基址取，不能写死绝对地址")
check("<Rx%x>" % S.NAMES_RVA in code, "wav 名表同上")
check(code.endswith("61c3"), "结尾必须是 popad; ret（cdecl）")
check(code.startswith("fc60"), "开头必须是 cld; pushad")

# jnz 的回跳距离
m = re.search(r"ab31d2891789570489570889570c83c710404b75([0-9a-f]{2})", code)
check(m is not None, "找不到新行骨架循环")
if m:
    body_len = len("ab31d2891789570489570889570c83c710404b") // 2
    check(int(m.group(1), 16) == (256 - (body_len + 2)) & 0xff, "jnz 回跳距离算错")

# 拷贝长度
import struct
check(struct.pack("<I", S.CFG_ROWS * S.CFG_ROW // 4).hex() in code, "cfg 拷贝 dword 数不对")
check(struct.pack("<I", S.NAMES_N + 1).hex() in code, "wav 名拷贝 dword 数不对（要含 NULL 终止）")

print("sound_emit: %d checks, %d failed" % (9, len(fail)))
for m_ in fail: print("  FAIL", m_)
sys.exit(1 if fail else 0)
```

- [ ] **Step 3: 跑测试确认失败（还没有 `sound_emit.py` 时）**

Run: `cd native && python3 tests/test_sound_emit.py`
Expected: `ModuleNotFoundError: No module named 'sound_emit'`（Step 1 已写就跳过本步）

- [ ] **Step 4: 跑测试确认通过**

Run: `cd native && python3 tests/test_sound_emit.py`
Expected: `sound_emit: 9 checks, 0 failed`

- [ ] **Step 5: 用 keystone 反汇编回来，人眼核对骨架循环**

```bash
cd native && python3 - <<'EOF'
from sound_emit import emit_sound_codecaves
import sound_sites as S, re
code = emit_sound_codecaves()[S.CAVE_INIT]["code"]
# 把 <…> 表达式替换成 4 个占位字节，再反汇编看形状
flat = re.sub(r"<[^>]+>", "deadbeef", code)
print(flat)
EOF
```

把 `flat` 贴进任一 x86-32 反汇编器（或 `objdump -D -b binary -m i386 --start-address=0`），
确认序列是：`cld / pushad / mov edi,imm / mov esi,imm / mov ecx,0x1a4 / rep movsd /
mov eax,0x54 / mov ebx,0x20 / (stosd; xor edx,edx; 四个 mov; add edi,0x10; inc eax; dec ebx; jnz) /
mov edi,imm / mov esi,imm / mov ecx,0x49 / rep movsd / popad / ret`。

- [ ] **Step 6: Commit**

```bash
git add native/sound_emit.py native/tests/test_sound_emit.py
git commit -m "feat(sound): 四块 codecave + th18_snd_patch_init（从用户 exe 拷原生表 + 新行 I1/I2 骨架）"
```

---

### Task 4: 51 处 binhack 生成与 `verify` 对账

**Files:**
- Modify: `native/sound_emit.py`（加 `emit_sound_binhacks`）
- Modify: `native/sites.py`（`main()` 里合并音效表的 codecaves/binhacks；`verify` 认这批）
- Modify: `native/tests/test_sound_emit.py`（加 binhack 断言）

**Interfaces:**
- Consumes: `sound_sites.scan(text, text_va)`
- Produces: `emit_sound_binhacks(text, text_va) -> dict`，键形如 `snd_401110`，值 `{"addr","code","expected","title"}`

- [ ] **Step 1: 写 binhack 发射器（追加到 `native/sound_emit.py`）**

```python
def emit_sound_binhacks(text, text_va):
    """51 处：把指令末 4 字节换掉，前缀原样保留。长度必须不变。

    · cave 引用 → <codecave:名+偏移>（**绝对**表达式用尖括号；方括号是相对偏移，
      写错会把相对量当指针用 —— mods/thcrap-platform.md §3.4）
    · 计数界   → 新立即数
    """
    B = {}
    for r in S.scan(text, text_va):
        if r["cave"] is None:
            new_tail = struct.pack("<I", r["off"]).hex()
        else:
            suffix = ("+%x" % r["off"]) if r["off"] else ""
            new_tail = "<codecave:%s%s>" % (r["cave"], suffix)
        expected = (r["pre"] + struct.pack("<I", r["old"])).hex()
        B["snd_%06x" % r["va"]] = {
            "addr": "0x%06x" % r["va"],
            "code": r["pre"].hex() + new_tail,
            "expected": expected,
            "title": "音效表扩容：%s" % r["title"],
        }
    assert len(B) == 51, "应生成 51 条 binhack，实际 %d" % len(B)
    return B
```

- [ ] **Step 2: 加断言到 `native/tests/test_sound_emit.py`**

在文件末尾（`print` 之前）插入：

```python
import sites
text, text_va, _ = sites.load_text_section(sites.EXE)   # 返回 3 元组
from sound_emit import emit_sound_binhacks
bh = emit_sound_binhacks(text, text_va)
check(len(bh) == 51, "应 51 条 binhack，实际 %d" % len(bh))
for name, b in bh.items():
    exp = bytes.fromhex(b["expected"])
    va  = int(b["addr"], 16)
    check(text[va - text_va:va - text_va + len(exp)] == exp, "%s：expected 与 exe 不符" % name)
    # 长度不变：<…> 表达式渲染成 4 字节
    code_len = len(re.sub(r"<[^>]+>", "xxxxxxxx", b["code"])) // 2
    check(code_len == len(exp), "%s：改写后长度变了（%d vs %d）" % (name, code_len, len(exp)))
    # ★ 绝对表达式必须是尖括号
    check("[codecave:" not in b["code"], "%s：用了方括号（相对），必须用尖括号" % name)
# 尾界特判：blob 释放界只到原生 72
check(bh["snd_%06x" % 0x4713d8]["code"].endswith("<codecave:%s+%x>" % (S.CAVE_BLOBS, S.BLOB_N * 4)),
      "0x4713d8 的释放尾界必须只到原生 72（跨堆释放会崩）")
# 硬编码槽 20
check(bh["snd_%06x" % 0x45ff38]["code"].endswith("<codecave:%s+%x>" % (S.CAVE_SLOTS, 20 * S.SLOT_SIZE)),
      "0x45ff38 必须指向槽 20")
```

- [ ] **Step 3: 跑测试**

Run: `cd native && python3 tests/test_sound_emit.py`
Expected: `0 failed`

- [ ] **Step 4: 接进 `sites.py` 的 `main()`**

在 `sites.py` 顶部 import 附近加：

```python
from sound_emit import emit_sound_codecaves, emit_sound_binhacks
```

在 `doc = {...}` 组装之后（`sites.py` 约 1095 行，`doc["binhacks"].update(...)` 那一串的**末尾**）追加：

```python
        doc["codecaves"].update(emit_sound_codecaves())
        doc["binhacks"].update(emit_sound_binhacks(text, text_va))
```

- [ ] **Step 5: 生成并数一数**

Run: `cd native && make ROWS=255 gen`
Expected: 输出的 binhack 数比改动前多 **51**，codecave 数多 **5**。用下面这条对账：

```bash
cd native && python3 -c "
import json; d=json.load(open('../patch/th18.v1.00a.js'))
snd=[k for k in d['binhacks'] if k.startswith('snd_')]
print('snd binhacks =', len(snd))
print('codecaves    =', [k for k in d['codecaves'] if 'snd' in k])
assert len(snd)==51, len(snd)
"
```

- [ ] **Step 6: 拿生成的 patch 回真 exe 对账**

Run: `cd native && make verify`
Expected: 全部通过，无 `expected 不符`。若 `verify` 不认识 `snd_` 前缀，按 `sites.py` 里
`verify` 对 `order_` / `menu_` 的处理方式补一个分支：只校验 `expected` 与 exe 一致、
且 `code` 与 `expected` 等长。

- [ ] **Step 7: 与别的 patch 求冲突**

Run: `cd native && make conflicts OTHERS="<thcrap>/repos/nmlgc/base_tsa/th18.v1.00a.js"`
Expected: 51 处新站点与 base_tsa 无交集（有交集必须停下来讨论，不能直接盖）。

- [ ] **Step 8: Commit**

```bash
git add native/sound_emit.py native/sites.py native/tests/test_sound_emit.py ../patch/th18.v1.00a.js
git commit -m "feat(sound): 51 处 binhack 生成 + verify 对账（音效表四张全搬进 codecave）"
```

---

### Task 5: DLL 侧 —— `ce_snd_gate` 断点、填 blob、五条自检

**Files:**
- Create: `native/sound.c`
- Create: `native/sound.h`
- Modify: `native/sites.py`（`emit_header` 里导出音效常量到 `sites_gen.h`；`main()` 里加 `ce_snd_gate` 断点）
- Modify: `native/Makefile`（`SRCS` 加 `sound.c`）
- Modify: `native/th18_card_expand.def`（导出 `BP_ce_snd_gate`）

**Interfaces:**
- Consumes: `sites_gen.h` 的 `CE_SND_CFG_ROWS`(84) / `CE_SND_NEW_N`(32) / `CE_SND_CFG_ROW`(0x14) / `CE_SND_SLOT_SIZE`(0x18) / `CE_SND_NAMES_N`(72) / `CE_SND_CAVE_*`（cave 名字符串）；`card_expand.h` 的 `ce_log` / `ce_verdict` / `ce_func_get`；`thcrap_bp.h` 的 `x86_reg_t` / `BP_EXEC_ORIGINAL`
- Produces: `int ce_sound_init(void)`（1 = 成功，0 = 失败已还原）；`int ce_sound_voice_count(void)`；`BP_ce_snd_gate`

- [ ] **Step 1: 在 `sites.py` 的 `emit_header()` 里导出常量**

在 `emit_header` 生成的 `sites_gen.h` 文本末尾追加：

```python
    import sound_sites as S
    lines += [
        "",
        "/* ---- 音效表扩容（sound_sites.py）---- */",
        "#define CE_SND_CFG_ROWS   %d" % S.CFG_ROWS,
        "#define CE_SND_NEW_N      %d" % S.NEW_N,
        "#define CE_SND_ROWS_TOTAL %d" % S.CFG_ROWS_N,
        "#define CE_SND_CFG_ROW    0x%x" % S.CFG_ROW,
        "#define CE_SND_SLOT_SIZE  0x%x" % S.SLOT_SIZE,
        "#define CE_SND_NAMES_N    %d" % S.NAMES_N,
        "#define CE_SND_FIRST_ID   0x%x" % S.CFG_ROWS,
        '#define CE_SND_CAVE_CFG   "codecave:%s"' % S.CAVE_CFG,
        '#define CE_SND_CAVE_NAMES "codecave:%s"' % S.CAVE_NAMES,
        '#define CE_SND_CAVE_SLOTS "codecave:%s"' % S.CAVE_SLOTS,
        '#define CE_SND_CAVE_BLOBS "codecave:%s"' % S.CAVE_BLOBS,
    ]
```

（`lines` 就是 `emit_header` 里已有的行列表变量，`sites.py:885` 起。）

- [ ] **Step 2: 在 `sites.py` 的 `main()` 里声明断点**

在 `doc["breakpoints"]` 组装处追加（与 `ce_gate` 同款形状，`0x476410` 入口是
`55 8b ec 6a ff` = `push ebp; mov ebp,esp; push -1`，5 字节且无相对寻址）：

```python
        doc["breakpoints"]["ce_snd_gate"] = {
            "addr": "0x476410", "cavesize": 5, "expected": "558bec6aff",
            "title": "SoundManager::init 入口：填语音 blob 与新行配置，然后放行让引擎建 buffer",
        }
```

并在 `gen` 里加一条对账（防止 exe 不是预期版本）：

```python
        if text[0x476410 - text_va:0x476410 - text_va + 5].hex() != "558bec6aff":
            raise ShapeError("0x476410 入口不是 55 8b ec 6a ff")
```

- [ ] **Step 3: 写 `native/sound.h`**

```c
/* sound.h —— 音效表扩容的 DLL 侧接口。 */
#ifndef CE_SOUND_H
#define CE_SOUND_H

/* 在 SoundManager::init 入口调一次。1 = 新 id 可用；0 = 已还原成原生行为。 */
int ce_sound_init(void);

/* 已登记的语音条数（自检与日志用）。 */
int ce_sound_voice_count(void);

#endif
```

- [ ] **Step 4: 写 `native/sound.c`**

```c
/* sound.c —— 音效表扩容的 DLL 侧：填语音 blob、写新行配置、五条自检。
 *
 * 时序（spec §3.4）：
 *   codecaves_apply 末尾  th18_snd_patch_init  拷原生 84 行 cfg + 72 个 wav 名 + 新行骨架
 *   binhacks_apply        51 处站点改写
 *   BP_ce_snd_gate        ← 本文件。0x476410 入口，引擎还没读表
 *   放行后               循环1 初始化 116 槽、循环2 逐行建 buffer
 *
 * ★ blob 的字节直接用 thcrap stack_game_file_resolve 的返回，不拷贝、不释放：
 *   WinMain 的释放循环尾界刻意只到原生 72（binhack 0x4713d8），跨堆 free 会崩。
 */
#include <stdio.h>
#include <string.h>
#include "card_expand.h"
#include "sites_gen.h"
#include "sound.h"

typedef struct json_t { int type; size_t refcount; } json_t;

static json_t     *(*p_stack_game_json_resolve)(const char *, size_t *);
static void       *(*p_stack_game_file_resolve)(const char *, size_t *);
static json_t     *(*p_json_decref_safe)(json_t *);
static void       *(*p_json_object_iter)(json_t *);
static const char *(*p_json_object_iter_key)(void *);
static json_t     *(*p_json_object_iter_value)(void *);
static void       *(*p_json_object_iter_next)(json_t *, void *);
static json_t     *(*p_json_object_get)(const json_t *, const char *);
static long long   (*p_json_integer_value)(const json_t *);
static const char *(*p_json_string_value)(const json_t *);

static int resolve_imports(void)
{
    static const struct { const char *dll, *sym; void **slot; } tab[] = {
        { "thcrap.dll",  "stack_game_json_resolve", (void **)&p_stack_game_json_resolve },
        { "thcrap.dll",  "stack_game_file_resolve", (void **)&p_stack_game_file_resolve },
        { "thcrap.dll",  "json_decref_safe",        (void **)&p_json_decref_safe },
        { "jansson.dll", "json_object_iter",        (void **)&p_json_object_iter },
        { "jansson.dll", "json_object_iter_key",    (void **)&p_json_object_iter_key },
        { "jansson.dll", "json_object_iter_value",  (void **)&p_json_object_iter_value },
        { "jansson.dll", "json_object_iter_next",   (void **)&p_json_object_iter_next },
        { "jansson.dll", "json_object_get",         (void **)&p_json_object_get },
        { "jansson.dll", "json_integer_value",      (void **)&p_json_integer_value },
        { "jansson.dll", "json_string_value",       (void **)&p_json_string_value },
    };
    for (unsigned i = 0; i < sizeof tab / sizeof tab[0]; ++i) {
        HMODULE m = GetModuleHandleA(tab[i].dll);
        *tab[i].slot = m ? (void *)GetProcAddress(m, tab[i].sym) : NULL;
        if (!*tab[i].slot) { ce_verdict("snd: FAIL missing %s!%s", tab[i].dll, tab[i].sym); return 0; }
    }
    return 1;
}

/* wav 名表要指向长期有效的字符串，放 DLL 自己的静态区。 */
static char  s_names[CE_SND_NEW_N][64];
static int   s_voice_count;

int ce_sound_voice_count(void) { return s_voice_count; }

static uint8_t *cave(const char *name)
{
    return ce_func_get ? (uint8_t *)ce_func_get(name) : NULL;
}

/* 五条自检。任何一条不过 → 把新行退回骨架（+4 = 0、+0x10 = 0），原生行为不变。 */
static int selfcheck(uint8_t *cfg, uint8_t *slots, uint8_t **blobs, char **names)
{
    (void)names;
    /* R8：pre-main（0x401100，界 0x401139）已经跑过，新槽的 +4 必须是 -1「空闲」。
     * 不是 -1 = 那处字节界没改对，play_sound 一进来就把新 id 当忙的处理。 */
    for (int k = CE_SND_CFG_ROWS; k < CE_SND_ROWS_TOTAL; ++k) {
        uint32_t v = *(uint32_t *)(slots + k * CE_SND_SLOT_SIZE + 4);
        if (v != 0xFFFFFFFFu) {
            ce_verdict("snd: FAIL R8 slot %d .+4 = 0x%08x, expected -1 (0x401139 bound?)", k, v);
            return 0;
        }
    }
    int seen[CE_SND_ROWS_TOTAL];
    memset(seen, 0, sizeof seen);
    for (int j = 0; j < CE_SND_ROWS_TOTAL; ++j) {
        uint32_t slot = *(uint32_t *)(cfg + j * CE_SND_CFG_ROW);
        uint32_t wav  = *(uint32_t *)(cfg + j * CE_SND_CFG_ROW + 4);
        if (slot >= (uint32_t)CE_SND_ROWS_TOTAL || seen[slot]) {
            ce_verdict("snd: FAIL I1 row %d has slot %u (dup or out of range)", j, slot);
            return 0;
        }
        seen[slot] = 1;
        if (wav >= (uint32_t)(CE_SND_NAMES_N + CE_SND_NEW_N)) {
            ce_verdict("snd: FAIL row %d wav index %u out of range", j, wav);
            return 0;
        }
        if (!blobs[wav]) {
            ce_verdict("snd: FAIL I2 row %d -> blob[%u] is NULL (would hang in 0x4776f0)", j, wav);
            return 0;
        }
    }
    /* 槽 20 必须仍是 se_lazer02（wav 下标 0x26）——0x45ff38 那处硬编码引用的对象 */
    for (int j = 0; j < CE_SND_ROWS_TOTAL; ++j)
        if (*(uint32_t *)(cfg + j * CE_SND_CFG_ROW) == 20) {
            uint32_t wav = *(uint32_t *)(cfg + j * CE_SND_CFG_ROW + 4);
            if (wav != 0x26) { ce_verdict("snd: FAIL slot 20 -> wav %u, expected 0x26", wav); return 0; }
            break;
        }
    return 1;
}

static void rollback(uint8_t *cfg)
{
    for (int k = 0; k < CE_SND_NEW_N; ++k) {
        uint8_t *row = cfg + (CE_SND_CFG_ROWS + k) * CE_SND_CFG_ROW;
        *(uint32_t *)(row + 4)   = 0;      /* 回到原生 wav 0，I2 仍成立 */
        *(uint32_t *)(row + 8)   = 0;
        *(uint32_t *)(row + 0x10) = 0;
    }
    s_voice_count = 0;
}

int ce_sound_init(void)
{
    uint8_t  *cfg   = cave(CE_SND_CAVE_CFG);
    uint8_t  *slots = cave(CE_SND_CAVE_SLOTS);
    uint8_t **blobs = (uint8_t **)cave(CE_SND_CAVE_BLOBS);
    char    **names = (char **)cave(CE_SND_CAVE_NAMES);
    if (!cfg || !slots || !blobs || !names) {
        ce_verdict("snd: FAIL codecave lookup (cfg=%p slots=%p blobs=%p names=%p)",
                   (void *)cfg, (void *)slots, (void *)blobs, (void *)names);
        return 0;
    }
    ce_log("snd: caves cfg=%p names=%p slots=%p blobs=%p", (void *)cfg, (void *)names,
           (void *)slots, (void *)blobs);
    if (!resolve_imports()) return 0;

    size_t sz = 0;
    json_t *root = p_stack_game_json_resolve("voice.js", &sz);
    if (root) {
        void *it = p_json_object_iter(root);
        for (int k = 0; it && k < CE_SND_NEW_N; it = p_json_object_iter_next(root, it)) {
            const char *key = p_json_object_iter_key(it);
            json_t *obj = p_json_object_iter_value(it);
            json_t *jw = p_json_object_get(obj, "wav");
            const char *wav = jw ? p_json_string_value(jw) : NULL;
            if (!wav) { ce_log("snd: skip \"%s\": no \"wav\"", key ? key : "?"); continue; }

            char path[128];
            snprintf(path, sizeof path, "voice/%s.wav", wav);
            size_t bytes = 0;
            void *buf = p_stack_game_file_resolve(path, &bytes);
            if (!buf || bytes < 44) { ce_log("snd: skip \"%s\": %s not found", key, path); continue; }
            if (memcmp(buf, "RIFF", 4) != 0 || memcmp((char *)buf + 8, "WAVE", 4) != 0) {
                ce_log("snd: skip \"%s\": %s is not RIFF/WAVE", key, path); continue;
            }

            json_t *jv = p_json_object_get(obj, "volume");
            json_t *jp = p_json_object_get(obj, "pan");
            long long vol = jv ? p_json_integer_value(jv) : 100;
            long long pan = jp ? p_json_integer_value(jp) : 0;
            if (vol < 0) vol = 0; else if (vol > 100) vol = 100;

            snprintf(s_names[k], sizeof s_names[k], "%s", path);
            blobs[CE_SND_NAMES_N + k] = (uint8_t *)buf;
            names[CE_SND_NAMES_N + k] = s_names[k];

            uint8_t *row = cfg + (CE_SND_CFG_ROWS + k) * CE_SND_CFG_ROW;
            *(uint32_t *)(row + 4)    = (uint32_t)(CE_SND_NAMES_N + k);
            *(uint32_t *)(row + 8)    = ((uint32_t)(vol & 0xffff) << 16) | (uint32_t)(pan & 0xffff);
            *(uint32_t *)(row + 0x10) = 1;
            ce_log("snd: voice %d id 0x%02x \"%s\" -> %s (%u bytes, vol %lld pan %lld)",
                   k, CE_SND_FIRST_ID + k, key, path, (unsigned)bytes, vol, pan);
            ++k; ++s_voice_count;
        }
        p_json_decref_safe(root);
    } else {
        ce_log("snd: no voice.js in the patch stack (0 voices)");
    }

    if (!selfcheck(cfg, slots, blobs, names)) { rollback(cfg); return 0; }
    ce_verdict("snd: OK %d voices, %d rows, I1/I2 hold", s_voice_count, CE_SND_ROWS_TOTAL);
    return 1;
}
```

- [ ] **Step 5: 挂断点（追加到 `native/dll_main.c` 末尾）**

```c
/* 音效表扩容的门：0x476410 = SoundManager::init 入口。
 * 语音 blob 必须在引擎建 buffer 之前就位，这里正好在循环之前。 */
#include "sound.h"
int __cdecl BP_ce_snd_gate(x86_reg_t *regs, void *bp_info)
{
    (void)regs; (void)bp_info;
    static int done;
    if (!done) { done = 1; ce_sound_init(); }
    return BP_EXEC_ORIGINAL;
}
```

- [ ] **Step 6: 加进构建与导出表**

`native/Makefile`：`SRCS` 那一行加 `sound.c`。
`native/th18_card_expand.def`：`EXPORTS` 下加一行 `BP_ce_snd_gate`。

- [ ] **Step 7: 编译 + x87 自检 + 导出表核对**

```bash
cd native && make ROWS=255 gen && make dll && make dllx87 && make dllverify
```
Expected: 编译无 warning（`-Wall -Wextra`）；`x87 in our objects: 0`；导出表里能看到 `BP_ce_snd_gate`。

- [ ] **Step 8: Commit**

```bash
git add native/sound.c native/sound.h native/dll_main.c native/Makefile native/th18_card_expand.def native/sites.py ../patch/th18.v1.00a.js
git commit -m "feat(sound): ce_snd_gate 断点 —— 填语音 blob 与新行配置 + I1/I2 自检（失败即还原）"
```

---

### Task 6: 语音资源管线 —— `assets/voice/` 与 `voice.js`

**Files:**
- Create: `assets/voice/README.md`
- Create: `assets/voice/ORDER.txt`
- Create: `assets/build_voice.py`
- Create: `patch/th18/voice.js`
- Test: `native/tests/test_build_voice.py`
- Modify: `native/Makefile`（加 `voice` 目标）

**Interfaces:**
- Consumes: `sound_sites.NEW_N`（上限 32）
- Produces:
  - `assets/build_voice.py` 的 `validate(order, wav_dir) -> list[dict]`：每项 `{"name","index","id","path","bytes","rate","bits","channels"}`
  - `patch/th18/voice.js`：`{"<KEY>": {"wav": "<NAME>", "volume": 0-100, "pan": int}}`
  - `native/voice_ids.h`：`#define CE_VOICE_<NAME> 0x??`

- [ ] **Step 1: 写 `assets/build_voice.py`**

```python
#!/usr/bin/env python3
"""build_voice.py —— 语音资源的校验与索引生成。

  assets/voice/ORDER.txt   一行一个 NAME，**只追加**。行号 k → 音效 id 0x54+k、wav 下标 72+k
  assets/voice/<NAME>.wav  PCM（tag 1）。原生 71 个 se 就是 44.1k/22.05k × 8/16 bit × 单/双声道混用，
                           引擎的 RIFF 解析不挑格式；建议 16-bit 单声道（单声道声像才有意义）。

跑：python3 assets/build_voice.py          校验 + 拷进 patch/th18/voice/ + 生成 native/voice_ids.h
    python3 assets/build_voice.py --check  只校验
"""
import argparse, os, shutil, struct, sys

HERE = os.path.dirname(os.path.abspath(__file__))
VOICE_DIR = os.path.join(HERE, "voice")
PATCH_DIR = os.path.join(HERE, "..", "patch", "th18", "voice")
IDS_H     = os.path.join(HERE, "..", "native", "voice_ids.h")
MAX_N     = 32            # = sound_sites.NEW_N
FIRST_ID  = 0x54
FIRST_WAV = 72


class VoiceError(Exception):
    pass


def parse_fmt(path):
    b = open(path, "rb").read()
    if len(b) < 44 or b[:4] != b"RIFF" or b[8:12] != b"WAVE":
        raise VoiceError("%s：不是 RIFF/WAVE" % os.path.basename(path))
    i = 12
    while i + 8 <= len(b):
        cid, sz = b[i:i + 4], struct.unpack_from("<I", b, i + 4)[0]
        if cid == b"fmt ":
            tag, ch, rate, _bps, _al, bits = struct.unpack_from("<HHIIHH", b, i + 8)
            if tag != 1:
                raise VoiceError("%s：format tag %d，只支持 PCM(1)" % (os.path.basename(path), tag))
            return {"bytes": len(b), "rate": rate, "bits": bits, "channels": ch}
        i += 8 + sz + (sz & 1)
    raise VoiceError("%s：找不到 fmt 块" % os.path.basename(path))


def validate(order_lines, wav_dir):
    names = [l.strip() for l in order_lines if l.strip() and not l.strip().startswith("#")]
    if len(names) > MAX_N:
        raise VoiceError("ORDER.txt 有 %d 行，上限 %d（= sound_sites.NEW_N）" % (len(names), MAX_N))
    if len(set(names)) != len(names):
        raise VoiceError("ORDER.txt 有重复的 NAME")
    out = []
    for k, n in enumerate(names):
        if not n.replace("_", "").isalnum():
            raise VoiceError("NAME %r 只能是字母数字下划线" % n)
        p = os.path.join(wav_dir, n + ".wav")
        if not os.path.exists(p):
            raise VoiceError("缺文件：%s.wav" % n)
        info = parse_fmt(p)
        info.update({"name": n, "index": k, "id": FIRST_ID + k,
                     "wav_index": FIRST_WAV + k, "path": p})
        out.append(info)
    return out


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--check", action="store_true")
    a = ap.parse_args()
    order = open(os.path.join(VOICE_DIR, "ORDER.txt"), encoding="utf-8").read().splitlines()
    v = validate(order, VOICE_DIR)
    total = sum(x["bytes"] for x in v)
    print("语音 %d 条，共 %.2f MB" % (len(v), total / 1048576.0))
    print("%-24s %-6s %-6s %s" % ("NAME", "id", "wav", "格式"))
    for x in v:
        print("%-24s 0x%02x   %-6d %d Hz %d-bit %dch  %d B"
              % (x["name"], x["id"], x["wav_index"], x["rate"], x["bits"], x["channels"], x["bytes"]))
    if a.check:
        return
    os.makedirs(PATCH_DIR, exist_ok=True)
    for f in os.listdir(PATCH_DIR):
        if f.endswith(".wav"):
            os.remove(os.path.join(PATCH_DIR, f))
    for x in v:
        shutil.copy2(x["path"], os.path.join(PATCH_DIR, x["name"] + ".wav"))
    with open(IDS_H, "w", encoding="utf-8") as f:
        f.write("/* voice_ids.h —— 由 assets/build_voice.py 生成，别手改。 */\n")
        f.write("#ifndef CE_VOICE_IDS_H\n#define CE_VOICE_IDS_H\n\n")
        for x in v:
            f.write("#define CE_VOICE_%-24s 0x%02x\n" % (x["name"], x["id"]))
        f.write("\n#define CE_VOICE_REGISTERED %d\n\n#endif\n" % len(v))
    print("→ %s（%d 个文件）\n→ %s" % (PATCH_DIR, len(v), IDS_H))


if __name__ == "__main__":
    try:
        main()
    except VoiceError as e:
        print("FAIL:", e); sys.exit(1)
```

- [ ] **Step 2: 写单测（`native/tests/test_build_voice.py`）**

```python
"""test_build_voice.py —— build_voice.validate 的边界。跑：python3 tests/test_build_voice.py"""
import os, struct, sys, tempfile
sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "..", "assets"))
from build_voice import validate, VoiceError, MAX_N

fail = []
def check(cond, msg):
    if not cond: fail.append(msg)

def wav(path, tag=1, ch=1, rate=44100, bits=16, data=b"\0" * 100):
    fmt = struct.pack("<HHIIHH", tag, ch, rate, rate * ch * bits // 8, ch * bits // 8, bits)
    body = b"WAVE" + b"fmt " + struct.pack("<I", len(fmt)) + fmt + b"data" + struct.pack("<I", len(data)) + data
    open(path, "wb").write(b"RIFF" + struct.pack("<I", len(body)) + body)

d = tempfile.mkdtemp()
wav(os.path.join(d, "A.wav")); wav(os.path.join(d, "B.wav"))
wav(os.path.join(d, "BAD.wav"), tag=3)                      # IEEE float，不是 PCM

v = validate(["A", "B"], d)
check(len(v) == 2, "两条")
check(v[0]["id"] == 0x54 and v[1]["id"] == 0x55, "id 从 0x54 递增")
check(v[0]["wav_index"] == 72 and v[1]["wav_index"] == 73, "wav 下标从 72 递增")
check(v[0]["rate"] == 44100 and v[0]["bits"] == 16 and v[0]["channels"] == 1, "fmt 解析")

for lines, why in ((["A", "A"], "重复 NAME"), (["A"] * (MAX_N + 1), "超上限"),
                   (["MISSING"], "缺文件"), (["BAD"], "非 PCM"), (["A-B"], "非法字符")):
    try:
        validate(lines, d); fail.append("应该报错但没报：%s" % why)
    except VoiceError:
        pass

# 注释与空行会被跳过
check(len(validate(["# c", "", "A"], d)) == 1, "注释/空行")

print("build_voice: %d failed" % len(fail))
for m in fail: print("  FAIL", m)
sys.exit(1 if fail else 0)
```

- [ ] **Step 3: 跑测试确认失败**

Run: `cd native && python3 tests/test_build_voice.py`
Expected: `ModuleNotFoundError: No module named 'build_voice'`（Step 1 已写就直接进 Step 4）

- [ ] **Step 4: 跑测试确认通过**

Run: `cd native && python3 tests/test_build_voice.py`
Expected: `build_voice: 0 failed`

- [ ] **Step 5: 放一条真语音跑通链路**

先用**原生 wav 的字节**验证格式那一环（spec R6）：

```bash
cd /data/sunyunbo/www/renkolab
mkdir -p mods/th18.v1.00a/card-expand/assets/voice
cp local/th18.v1.00a/dat/se_release.wav mods/th18.v1.00a/card-expand/assets/voice/TEST_VOICE.wav
printf 'TEST_VOICE\n' > mods/th18.v1.00a/card-expand/assets/voice/ORDER.txt
cd mods/th18.v1.00a/card-expand && python3 assets/build_voice.py
```
Expected: 打印 `语音 1 条`、`TEST_VOICE 0x54 72 44100 Hz ...`，生成 `patch/th18/voice/TEST_VOICE.wav` 与 `native/voice_ids.h`。

⚠ `assets/voice/TEST_VOICE.wav` 是原生字节，**只用于本地验证，不许入库**。
加到 `.gitignore`：`mods/th18.v1.00a/card-expand/assets/voice/TEST_VOICE.wav`
和 `mods/th18.v1.00a/card-expand/patch/th18/voice/*.wav`（真语音入库时再逐个 `git add -f`）。

- [ ] **Step 6: 写 `patch/th18/voice.js`**

```json
{
  "TEST_VOICE": { "wav": "TEST_VOICE", "volume": 100, "pan": 0 }
}
```

- [ ] **Step 7: 写 `assets/voice/README.md`**

照 `assets/README.md` 的形状（放什么 / 索引怎么来 / 跑什么 / 边界），要写明：
`ORDER.txt` 只追加、行号 k → id `0x54+k`、上限 32、建议 16-bit 单声道、
以及「`voice.js` 的 key 是给写卡的人看的名字，`wav` 字段才是 `ORDER.txt` 里的 NAME」。

- [ ] **Step 8: 加 Makefile 目标并 Commit**

`.PHONY` 加 `voice`，追加：

```makefile
## voice  —— 语音资源校验 + 拷进 patch/th18/voice/ + 生成 native/voice_ids.h
voice:
	python3 ../assets/build_voice.py
```
并把校验挂进 `check`：`check: anm-verify snd-check` → 末尾加 `&& python3 ../assets/build_voice.py --check`（若 ORDER.txt 不存在则跳过）。

```bash
git add assets/build_voice.py assets/voice/README.md assets/voice/ORDER.txt \
        patch/th18/voice.js native/tests/test_build_voice.py native/Makefile ../../../.gitignore
git commit -m "feat(sound): 语音资源管线（assets/voice + build_voice.py + voice.js + voice_ids.h）"
```

---

### Task 7: 接到一张卡上 —— SDK 与一次真实发声

**Files:**
- Modify: `native/sdk.h`（`ce_play_voice` 便捷宏 + include `voice_ids.h`）
- Modify: `native/cards/reverse.c`（反转牌 `c_press` 加一句语音）
- Modify: `SDK.md`（新增「语音」一节）

**Interfaces:**
- Consumes: `voice_ids.h` 的 `CE_VOICE_<NAME>`；`sdk.h` 已有的 `ce_play_sound(uint32_t id, float x)`
- Produces: `ce_play_voice(name_suffix, x)` 宏

- [ ] **Step 1: 在 `native/sdk.h` 的音效一节后追加**

```c
/* 语音：与 SE 完全同构（可叠加、不做独占通道），只是 id 落在扩展区 0x54..0x73。
 * NAME 来自 assets/voice/ORDER.txt，由 build_voice.py 生成 voice_ids.h。 */
#include "voice_ids.h"
#define ce_play_voice(NAME, x)  ce_play_sound(CE_VOICE_##NAME, (x))
```

⚠ `voice_ids.h` 是生成物：`native/Makefile` 的 `%.o` 依赖行要加上它，
且 `make dll` 之前必须先 `make voice`（在 `all` / `step3` 里把 `voice` 排在 `dll` 之前）。

- [ ] **Step 2: 让反转牌发一次语音（`native/cards/reverse.c`）**

把 `c_press` 里那行 `ce_play_sound(0x4d, …)` 之后加一行：

```c
    ce_play_voice(TEST_VOICE, p ? *(float *)(p + CE_PLAYER_X) : 0.0f);
```

- [ ] **Step 3: 全量构建**

```bash
cd native && make voice && make ROWS=255 gen verify test-host snd-check dll dllx87 files
```
Expected: 全绿；`patch/files.js` 里出现 `th18/voice/TEST_VOICE.wav` 与 `th18/voice.js` 的 crc。

- [ ] **Step 4: Windows 实跑（用户侧）**

叠 `_255` + `_test`，进关卡按 C。日志 `th18_card_expand.log` 应有：

```
snd: caves cfg=<addr> names=<addr> slots=<addr> blobs=<addr>
snd: voice 0 id 0x54 "TEST_VOICE" -> voice/TEST_VOICE.wav (353756 bytes, vol 100 pan 0)
snd: OK 1 voices, 116 rows, I1/I2 hold
```

体感：按 C 时**同时**听到 Tenshi 发动音（`0x4d`）和语音 —— 两条一起响就证明「可叠加」这条设计成立。
Alt-Tab 切出切回再按 C，语音仍在（R10）。退出游戏不崩（blob 不释放，R5 的堆问题已绕开）。

**任何 `FAIL:` 都是回归**，按 spec §5 的次序怀疑：
`snd: FAIL codecave lookup` → R2（cave 名或 `func_get` 时机）；
`snd: FAIL I1` → R4（`patch_init` 的骨架循环没跑或跑错）；
`snd: FAIL I2` → R5；
**没有 `snd:` 任何一行** → 断点没触发，查 `0x476410` 的 `expected` 与 cavesize；
**游戏卡在黑屏** → I2 被违反且自检没拦住，`0x4776f0` 在 `Sleep(10)` 死循环；
**玩家激光没声音** → R7，`0x45ff38` 的槽 20 指错了。

- [ ] **Step 5: 更新 `SDK.md`**

加「语音」一节：`ORDER.txt` → `voice.js` → `ce_play_voice(NAME, x)` 三步，
并写明**语音就是 SE**：可叠加、跟随 SE 音量、不打断、replay 无影响。

- [ ] **Step 6: Commit**

```bash
git add native/sdk.h native/cards/reverse.c native/Makefile SDK.md ../patch/files.js
git commit -m "feat(sound): ce_play_voice + 反转牌接一条测试语音（扩展 id 0x54 实跑）"
```

---

### Task 8: 审计、追溯与发布

**Files:**
- Modify: `AUDIT.md`（新增 §Q）
- Modify: `MAP.md`（第 11 段）
- Modify: `NEXT.md`（现状表 + 实跑清单）
- Modify: `CARDS.md` / `DATA.md`（若语音进了卡的登记字段）
- Modify: `README.md`（`assets/voice/` 的入口）

- [ ] **Step 1: 写 `AUDIT.md` §Q**

每一处写入点过 `mods/_template/AUDIT-checklist.md`。按类分组，不必 51 条各写一段，
但**必须逐条列出地址**，并对下面五处单独展开（它们是这批里唯一有独立失败模式的）：

| 条目 | 内容 |
| --- | --- |
| Q1 | `th18_snd_patch_init` 的调用时机与 ABI（cdecl / pushad 覆盖被调方保留寄存器 / `rep movsd` 前 `cld`） |
| Q2 | I1：新行骨架循环写的 `+0 = 84+k`；违反 = 循环 1 的无界扫描跑飞 |
| Q3 | I2：新行 `+4 = 0`；违反 = `0x4776f0` 在 `0x477768` 的 `Sleep(10)` 死等 |
| Q4 | `0x4713d8` 释放尾界刻意只到原生 72：跨堆 free 的规避，代价是进程退出时不回收 32 个 blob |
| Q5 | `0x45ff38` 硬编码槽 20（`se_lazer02`）：改写后必须仍指槽 20，自检里有断言 |

顶部照惯例留一行「实跑通过（日期）」的位置。

- [ ] **Step 2: `MAP.md` 加第 11 段**

行：**11 音效表扩容 / 语音** —— 站点 `native/sound_sites.py`、发射 `native/sound_emit.py`、
DLL `native/sound.c`、资源 `assets/voice/`、引擎一手 `engine/_shared/th18-sound-table.md`、审计 §Q。

- [ ] **Step 3: `NEXT.md` 更新**

现状表加一行「11 音效表扩容 🔧 待实跑」；实跑清单加 Task 7 Step 4 的日志与体感（原文照搬，
下一个会话不该回来读这份计划才能实跑）。

- [ ] **Step 4: 全量校验**

```bash
cd native && make check && make ROWS=255 gen verify test-host dll dllx87 dllverify files
cd /data/sunyunbo/www/renkolab && python tooling/check-docs.py
```
Expected: 全绿。

- [ ] **Step 5: 出包**

```bash
cd native && make dist
```
Expected: `dist/` 里 `patch-step3/th18/voice/` 有 wav、`voice.js` 在、`files.js` 收了它们的 crc。

- [ ] **Step 6: Commit**

```bash
git add AUDIT.md MAP.md NEXT.md README.md SDK.md
git commit -m "docs(sound): AUDIT §Q（51 处站点 + I1/I2 + 两处特例）、MAP 第 11 段、NEXT 实跑清单"
```

- [ ] **Step 7: 实跑通过后再发布**

`make release PUSH=1`。**实跑没过之前不要 release** —— modkit 是给别人装的。

---

## 附：这份计划刻意没做的事

- **不做语音独占通道 / 打断 / 队列**（用户 2026-09-05 明确否决）。
- **不做按需加载**：32 条 × 2 秒 ≈ 5.6 MB，与原生 5.8 MB 同量级。
- **不改 `0x4767d8`**：语音不走 dat，预加载线程仍只读原生 72 个。
- **不扩 blob 释放尾界**：见 Task 4 / AUDIT Q4。
- **不动 `engine/card/`**：这是音频子系统，结论归 `engine/_shared/th18-sound-table.md`。
