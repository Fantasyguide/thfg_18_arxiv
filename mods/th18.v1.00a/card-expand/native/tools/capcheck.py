#!/usr/bin/env python3
"""capcheck.py —— 容量台账 `CAPACITY.md` 的机器对账（`make check` 的一环，**只读、不碰 exe**）。

    python3 native/tools/capcheck.py                        # 全表 + 判定（该红就 exit 1）
    python3 native/tools/capcheck.py --explain sht_groups   # 单行溯源
    python3 native/tools/capcheck.py --gen-appendix         # 重新生成 CAPACITY.md 的附录 A（原地改写）
    python3 native/tools/capcheck.py --gen-appendix --print # 只打印，不写文件

## 为什么要有它

本 mod 每一次「长大」都是 binhack：把引擎写死的界改掉，并把**每一个读者**指到新家。
两次事故（`AUDIT.md` §CC 的 256 entry、§AH 的 560 份商店池）都不是「数组满了」，而是
**有一个读者我们没改**——那种错不崩不报错，只在实跑里表现为「贴图错位 / 卡不进池」。

所以证据分两半：`CAPACITY.md` 回答「现在离墙多远、墙是谁砌的」，本脚本负责「这份台账**不许过期**」：

  ① 每条容量的 用量/上限/余量 都从**仓库源码**现算（`native/`、`assets/`、`patch/th18/*.js`）；
     不读 exe、不读 `native/build/`（那是 gitignored 的产物，新 clone 上没有）；
  ② 余量 < `MARGIN`（10%，与台账里写死的口径同一句）且没有登记豁免 ⇒ **红**；
     豁免必须指向 `NEXT.md` 里真实存在的计划小节，锚点消失就判红（豁免 = 有计划，不是放行）；
  ③ 出现台账里没有的**有界**容量消费者 ⇒ **红**：新 卡 id / 新 shooterset 组宏 / 新图集 entry
     （判据 = 该名字必须出现在 `CAPACITY.md` 里，卡 id 必须出现在附录 A1 的清单里）。
     ⚠ **口径**：`CE_ANM_ABILITY_SCRIPT_*` 与 sprite **不判红**——脚本号与 sprite 号都是 dword、
     没有上限（`engine/anm/th18/06-entry-limit-and-multi-sprite.md` §1.2），拦它们只会挡住正常加美术；
     它们的变动照样打进输出与附录 A 的差异提示（跑 `--gen-appendix` 刷新）；
  ④ 卡本地池如果在源码里找不到任何「满/丢」计数（= 没有运行时余量断言）⇒ **⚠ 列出来**
     （不判红：`cards/trance.c` 是别人的文件，加计数器要走协作流程，见 `AUDIT.md` §BE）。

## 硬编码（拿不到就写死，每处都给出一手出处；其余一律现算）

| 常量 | 值 | 出处 |
| --- | --- | --- |
| `ABCARD_RETAIL_ENTRIES` | 118 | `assets/build_abcard.py` 文件头「base = 原生 entry 数（th18 = 118）」|
| `ABILITY_RETAIL_ENTRIES` / `_SPRITES` | 7 / 109 | `engine/anm/th18/06-entry-limit-and-multi-sprite.md` §2.5 |
| `ANM_ENTRY_MAX` | 256 | `assets/build_ability.py` 的 `MAX_ENTRIES`（这里有同名兜底常量）|
| `SHT_SET_SLOTS_NOW` / `_TARGET` | 96 / 96 | 2026-09-14 扩容**已落地**（`AUDIT.md` §CQ）：数组原地长到 96 格、数据区基点 `0x180→0x260`；零售仍是 40 格 |
| `SHT_SET_RETAIL_USED` | 22 | 一手普查：4 份零售 `pl0X.sht` 的 `+0xe0` 各 22 格非零（`0x01`–`0x16`）|
| `RETAIL_SHOP_POOL` | 362 | `AUDIT.md` §AH（原生 55 张卡的 `Σ(weight+5)`）|
| `RETAIL_SHOP_GUARANTEED` | `None` | 🟡 未普查 ⇒ 那一行不进余量闸，只报新卡增量 |
"""
import argparse
import json
import re
import struct
import sys
import unicodedata
from pathlib import Path

HERE = Path(__file__).resolve().parent            # native/tools
NATIVE = HERE.parent                              # native/
MOD = NATIVE.parent                               # card-expand/
ASSETS = MOD / "assets"
CARDS = NATIVE / "cards"
PATCH_JS = MOD / "patch" / "th18.v1.00a.js"
CARDS_JS = MOD / "patch" / "th18" / "cards.js"
VOICE_JS = MOD / "patch" / "th18" / "voice.js"
DOC = MOD / "CAPACITY.md"
NEXT_MD = MOD / "NEXT.md"

sys.path.insert(0, str(ASSETS))
import anmlib                                                     # noqa: E402  —— 仓库自己的 ORDER.txt 解析器

# ── 口径（改这里 = 改台账里写的那句「余量 < 10% 判红」）─────────────────────
MARGIN = 0.10

# ── 硬编码常量（出处见文件头表）───────────────────────────────────────────
ABCARD_RETAIL_ENTRIES = 118
ABILITY_RETAIL_ENTRIES = 7
ABILITY_RETAIL_SPRITES = 109
SHT_DBASE_OLD = 0x180          # 零售数据区起点 = 重定位基址那处 LEA 的立即数
SHT_SET_SLOTS_NOW = 96        # 2026-09-14 扩容落地（AUDIT §CQ）；零售 40 = [0xe0,0x180)
SHT_SET_SLOTS_TARGET = 96
SHT_SET_RETAIL_USED = 22
RETAIL_SHOP_POOL = 362
RETAIL_SHOP_GUARANTEED = None
ANM_ENTRY_MAX = 256

# 卡本地池：(行 key, 上限宏, 定义文件, 扫「满/丢」计数的文件, 该出现的计数标识(None = 没有), 满了怎么办)
#   ★ 这张表就是「有没有运行时余量断言」的判据；标识为 None ⇒ 该池满时**静默**（只有 LRU/退让）。
#   ★ 收谁不收谁的口径（`CAPACITY.md` §5-③ 有同样一份）：只收**装活状态、满了会改变行为**的定长数组。
#     标量阈值（`*_VFALL_MAX`）、钳位常量（`SEASON_DROP_CAP`）、诊断普查表（`ANIMAL_WAVE_CENSUS_MAX`）
#     一律不算池 —— 它们满不满不影响玩家看得到的东西。
#   ★ 2026-09-15（113 收尾，AUDIT §CX）：原 `pool_ledger`（`TRANCE_SHT_LEDGER`）**已随永久槽落地删掉** ——
#     灵界主炮组不再借原生格，也就没有"账本满 ⇒ 宁可不借"这条退让路径（少了**一个**运行时池）。
POOLS = [
    ("pool_spirit", "TRANCE_SPIRIT_POOL", "trance_core.h", "trance.c",
     "s_stat_spirit_dropped", "满 → 丢尾并计数（汇总日志 `pool dropped`）"),
    ("pool_popup", "TRANCE_POPUP_MAX", "trance.c", "trance.c",
     "s_popup_overflow", "满 → 顶掉最老的一支并计数（`popup overflow`）"),
    ("pool_killq", "TRANCE_KILL_QUEUE_MAX", "trance_core.h", "trance.c",
     "trance_kill_queue_dropped", "满 → 丢尾并计数（`queue dropped`）"),
    ("pool_boss", "TRANCE_BOSS_SLOTS", "trance.c", "trance.c",
     None, "满 → LRU 淘汰最久没受伤的那个（**无计数** ⇒ 满了看不出来）"),
    ("pool_phaseq", "TRANCE_PHASE_QUEUE_MAX", "trance.c", "trance.c",
     "s_stat_phase_full", "满 → 丢掉这一批「相位结束」事件并计数（汇总日志 `phase` 那一行）"),
    ("pool_antok", "ANIMAL_TOKEN_MAX", "animal.c", "animal.c",
     "token pool full", "满 → `token_spawn_heading` 返回 0（不生成令牌）；大怪掉落被拦时才记一行"),
    ("pool_anbigdrop", "ANIMAL_BIG_DROP_QUEUE", "animal.c", "animal.c",
     "s_big_drop_lost", "满 → 丢尾并计数（首次记一行 `big-enemy drop queue full`）"),
    ("pool_ufotok", "UFO_TOKEN_CAP", "ufo_core.h", "ufo.c",
     "s_tokpool.dropped", "满 → 丢新令牌并计数（汇总行 `token pool dropped`）"),
    ("pool_pofvsp", "POFV_SPIRIT_MAX", "pofv_core.h", "pofv.c",
     "spirit pool full", "满 → 丢掉这支神灵并记一行（`cap_logged` 只记一次）"),
    ("pool_pofvbq", "POFV_BLAST_QUEUE_MAX", "pofv_core.h", "pofv.c",
     "cap_logged", "满 → 丢爆风并计数（`blast queue full` 只记一行）"),
    ("pool_seasonpt", "SEASON_POINT_POOL", "season.c", "season.c",
     None, "满 → `spawn_point_at` 返回 0（调用方丢）—— **无计数** ⇒ 满了一片点到不了手"),
    ("pool_lolktint", "LOLK_TINT_MAX", "lolk.c", "lolk.c",
     None, "满 → 直接不染（注释口径「只影响观感」）—— **无计数**"),
    ("pool_faithuser", "FAITH_MAX_USERS", "faith.h", "faith.c",
     None, "满 → `faith_register` 返回 0（这张卡不计分）—— **无计数**"),
]

# ── 豁免：必须有 NEXT.md 里真实存在的计划，锚点消失就判红 ────────────────────
#   ⚠ 豁免 ≠ 放行：这一行照旧排在**最前面**、照旧打 ⚠。
WAIVERS = {
    "sht_groups": "SHT 容量扩容计划",
}

BEGIN = "<!-- capcheck:appendix:begin"
END = "<!-- capcheck:appendix:end -->"
SPRITE_NOTE = "A5 是**计数棘轮**而不是逐名清单"


# ══════════════════════════════════════════════════════════════════════════
# 解析（全部只读仓库文件）
# ══════════════════════════════════════════════════════════════════════════
def read_text(p: Path) -> str:
    return p.read_text(encoding="utf-8")


def gen_const(path: Path, name: str) -> int:
    """从**生成器源码**里读 `NAME = <数字>` —— 上限只有一个来源。

    anm 的 256 是**引擎的界**，但真正执行它的是构建期硬门（`build_abcard.py` / `build_ability.py`
    的 `MAX_ENTRIES`）。台账要是自己再写一个 256，两边就会各自漂移：门加严了台账还报旧余量、
    门被删了台账照样绿。所以这里跟生成器源码取**同一个数** —— 取不到就炸，不许兜底。
    """
    m = re.search(rf"^{name}\s*=\s*(\d+)\s*(?:#|$)", read_text(path), re.M)
    if not m:
        raise SystemExit(f"capcheck: `{path.name}` 里找不到 `{name} = <数字>` —— "
                         f"生成器的上限常量没了 / 改名了 / 被删了。\n"
                         f"  台账的「界」必须与执行它的那道门同一个数，先修脚本（或把门加回来）。")
    return int(m.group(1))


def gen_anm_caps():
    """→ (abcard 的 MAX_ENTRIES, ability 的 MAX_ENTRIES)，并校验都没越过引擎硬顶。"""
    caps = {n: gen_const(ASSETS / f"build_{n}.py", "MAX_ENTRIES") for n in ("abcard", "ability")}
    for n, v in caps.items():
        if v > ANM_ENTRY_MAX:
            raise SystemExit(
                f"capcheck: `build_{n}.py` 的 `MAX_ENTRIES` = {v} > 引擎硬顶 {ANM_ENTRY_MAX} —— "
                f"这道门放行了会产出**绑错贴图**的 anm（贴图键低 8 位，`0x486d38` / `0x47e1a4`）。")
    return caps["abcard"], caps["ability"]


def defines(path: Path):
    """`#define NAME <十进制|0x十六进制|(A + B)>` → {NAME: int}（函数式宏忽略）。

    二遍解 `(A + B)`：主池里唯一用到的是 `ufo_core.h` 的
    `#define UFO_TOKEN_CAP (UFO_TOKEN_MAX + UFO_TOKEN_POOL_SLACK)`。
    **只认加法**——别的算式（`sizeof(...)`、乘法）在有界数组上没出现过，
    放进来等于把上限口径藏进表达式，`CAPACITY.md` 就没法一眼看懂。
    """
    txt = read_text(path)
    out = {}
    for m in re.finditer(r"^#define\s+(\w+)\s+(0[xX][0-9a-fA-F]+|\d+)\s*(?:/\*|$)", txt, re.M):
        out[m.group(1)] = int(m.group(2), 0)
    for _ in range(4):                                   # 定点迭代：够 4 层嵌套
        grew = False
        for m in re.finditer(r"^#define\s+(\w+)\s*\(\s*(\w+)\s*\+\s*(\w+)\s*\)\s*(?:/\*|$)",
                             txt, re.M):
            name, a, b = m.groups()
            if name not in out and a in out and b in out:
                out[name] = out[a] + out[b]
                grew = True
        if not grew:
            break
    return out


def need(d, name, where):
    if name not in d:
        raise SystemExit(f"capcheck: {where} 里找不到 `#define {name}` —— 台账的上限来源没了，先修脚本")
    return d[name]


def ce_card_ids():
    """`native/cards/*.c` 里**行首**的 `CE_CARD(<id>,` → [(id, 文件, 行号)]（注释里提到的不算）。"""
    out = []
    for p in sorted(CARDS.glob("*.c")):
        for no, line in enumerate(read_text(p).splitlines(), 1):
            m = re.match(r"CE_CARD\(\s*(\d+)\s*,", line)
            if m:
                out.append((int(m.group(1)), p.name, no))
    out.sort()
    return out


def sht_sets():
    """`native/sht_ids.h`（生成物）的 `CE_SHT_SET_*` → {宏: 组号}。"""
    out = {}
    for m in re.finditer(r"^#define\s+(CE_SHT_SET_\w+)\s+(0[xX][0-9a-fA-F]+)\s",
                         read_text(NATIVE / "sht_ids.h"), re.M):
        out[m.group(1)] = int(m.group(2), 0)
    return out


def sht_expansion_check():
    """pl0X.sht 组数扩容的三方对账：`sht_ids.h`（生成物）↔ patch 里的 binhack ↔ 结构性恒等式。

    三方任一漂移都是**静默错位**（数组与数据区错 0xE0 字节 ⇒ 每条零售 shooterset 都指到别人家），
    所以这里判红而不是只打印：
      ① `sht_ids.h`：`CE_SHT_SLOTS` = 96（宣布可用）、`CE_SHT_SLOTS_RESERVED` = 256（数组区预留）、
     `CE_SHT_ARRAY_OFF` = 0xe0、`CE_SHT_DATA_BASE` = 0x4e0；
      ② patch 的 `sht_dbase_*`：`expected` = `8d82` + 老基点、`code` = `8d82` + 新基点（只换立即数）；
      ③ 恒等式：`CE_SHT_ARRAY_OFF + CE_SHT_SLOTS_RESERVED*4 == CE_SHT_DATA_BASE`
         （**预留**数组区必须**正好**顶到数据区 —— 少一格读者读出去、多一格压住数据区第一项）；
      ④ `CE_SHT_SLOTS <= CE_SHT_SLOTS_RESERVED`（宣布可用不能超过预留）。
    → (slots, slots_reserved, data_base)
    """
    txt = read_text(NATIVE / "sht_ids.h")
    def one(macro, default=None):
        m = re.search(r"^#define\s+%s\s+(\S+)" % macro, txt, re.M)
        if not m:
            if default is not None:
                return default
            raise SystemExit(f"capcheck: `sht_ids.h` 里没有 `{macro}` —— 生成器没跟上扩容（跑 `make sht`）")
        return int(m.group(1), 0)
    slots = one("CE_SHT_SLOTS")
    reserved = one("CE_SHT_SLOTS_RESERVED")
    arr = one("CE_SHT_ARRAY_OFF")
    base = one("CE_SHT_DATA_BASE")
    if slots != SHT_SET_SLOTS_NOW:
        raise SystemExit(f"capcheck: `sht_ids.h` 的 CE_SHT_SLOTS = {slots}，"
                         f"本脚本的 SHT_SET_SLOTS_NOW = {SHT_SET_SLOTS_NOW}（两边必须一起改）")
    if not 40 <= slots <= reserved:
        raise SystemExit(f"capcheck: 格数关系不成立：零售 40 ≤ 宣布 {slots} ≤ 预留 {reserved}")
    if arr + reserved * 4 != base:
        raise SystemExit(f"capcheck: shooterset 扩容恒等式破了：预留数组末尾 {arr + reserved * 4:#x} "
                         f"!= 数据区起点 {base:#x}（读者会读到数据区 / 数据区第一项会被压住）")
    if base != SHT_DBASE_OLD + (reserved - 40) * 4:
        raise SystemExit(f"capcheck: 数据区起点 {base:#x} != 0x180 + ({reserved}-40)*4 "
                         f"= {SHT_DBASE_OLD + (reserved - 40) * 4:#x}")
    # patch 那一侧：只认「唯一一条 sht_* binhack，且它换的就是这个立即数」
    doc = json.loads(read_text(PATCH_JS))
    sht_bh = {k: v for k, v in doc.get("binhacks", {}).items() if k.startswith("sht_")}
    if len(sht_bh) != 1:
        raise SystemExit(f"capcheck: patch 里 `sht_*` binhack 有 {len(sht_bh)} 条，应为 1 条"
                         f"（数组扩容那处数据区基点）")
    name, bh = next(iter(sht_bh.items()))
    exp, code = bytes.fromhex(bh["expected"]), bytes.fromhex(bh["code"])
    if exp[:2] != b"\x8d\x82" or len(exp) != 6 or len(code) != 6:
        raise SystemExit(f"capcheck: {name} 的形态不对（应 6 字节 `8d 82 imm32`）")
    if struct.unpack("<I", exp[2:])[0] != SHT_DBASE_OLD or struct.unpack("<I", code[2:])[0] != base:
        raise SystemExit(f"capcheck: {name} 换的不是数据区基点：expected imm = "
                         f"{struct.unpack('<I', exp[2:])[0]:#x}（应 {SHT_DBASE_OLD:#x}）、"
                         f"code imm = {struct.unpack('<I', code[2:])[0]:#x}（应 {base:#x}）")
    return slots, reserved, base


def anm_macros():
    """`native/anm_ids.h`（生成物）→ (图集宏, 脚本宏, sprite 宏数, sprite 号段)。"""
    txt = read_text(NATIVE / "anm_ids.h")
    atlas = sorted(set(re.findall(r"^#define\s+(CE_ANM_ABILITY_ATLAS_\w+)", txt, re.M)))
    script = sorted(set(re.findall(r"^#define\s+(CE_ANM_ABILITY_SCRIPT_\w+)", txt, re.M)))
    spr = [int(v) for v in re.findall(r"^#define\s+CE_ANM_ABILITY_SPRITE_\w+\s+(\d+)", txt, re.M)]
    if not spr:
        raise SystemExit("capcheck: anm_ids.h 里一个 sprite 宏都没有 —— 生成物坏了？")
    return atlas, script, len(spr), (min(spr), max(spr))


def ability_order():
    """`assets/ability/entries/ORDER.txt` → (追加 entry 数, 追加 sprite 数, REF 行数, 数据行数)。"""
    items, lines = anmlib.read_order(ASSETS / "ability" / "entries" / "ORDER.txt")
    n_entry = sum(1 for it in items if it["kind"] != "ref")
    n_sprite = sum(len(it["members"]) if it["kind"] == "atlas" else 1
                   for it in items if it["kind"] != "ref")
    return n_entry, n_sprite, sum(1 for it in items if it["kind"] == "ref"), len(lines)


def abcard_order():
    return anmlib.read_order_names(ASSETS / "cards" / "ORDER.txt")


def pl_plan():
    """`assets/build_player_anm.py` → ({pl0X: entry 总数}, {pl0X: 追加脚本数})。

    解析规则（对着那份源码的结构写的；那边改结构这里要跟着改）：
      · `dict(idx=N` 是该文件里**唯一**带 `idx=` 的写法 ⇒ N 只出现在 entry 计划里，
        且归属**最近一个前面出现过的** `"plNN":` 键（两个表都是这个布局）；
      · entry 总数 = max(idx) + 1 —— `_append_spec()` 硬校验「idx = 原生 entry 数 + 序号」；
      · `new_sid=N` 只出现在 `APPEND_PLAN` 的脚本计划里 ⇒ 按同一个「最近键」归属后计数。
    """
    cur = None
    max_idx, n_scripts = {}, {}
    for line in read_text(ASSETS / "build_player_anm.py").splitlines():
        m = re.search(r'"(pl\d\d)"\s*:', line)
        if m:
            cur = m.group(1)
        if cur is None:
            continue
        m = re.search(r"dict\(idx=(\d+)", line)
        if m:
            max_idx[cur] = max(max_idx.get(cur, -1), int(m.group(1)))
        if re.search(r"new_sid=\d+", line):
            n_scripts[cur] = n_scripts.get(cur, 0) + 1
    if set(max_idx) != {"pl00", "pl01", "pl02", "pl03"}:
        raise SystemExit(f"capcheck: 从 build_player_anm.py 只认出 {sorted(max_idx)} 的 entry 计划"
                         f"（期望 pl00..pl03）—— 解析规则过期了，先修脚本")
    return {k: v + 1 for k, v in max_idx.items()}, n_scripts


def cave_sizes():
    """`patch/th18.v1.00a.js` 的 `codecaves` → {名字: 字节数}（生成物，但**入库**）。"""
    out = {}
    for name, v in json.loads(read_text(PATCH_JS)).get("codecaves", {}).items():
        s = v.get("size")
        out[name] = int(s, 0) if isinstance(s, str) and s.startswith("0x") else None
    return out


def _json_loose(p: Path):
    return json.loads(re.sub(r"^\s*//.*$", "", read_text(p), flags=re.M))


def cards_js():
    return {int(k): v for k, v in _json_loose(CARDS_JS).items()}


def voice_js():
    return _json_loose(VOICE_JS)


def shop_pool(cards):
    """复算 `ce_shop_capacity_check`（`native/cards_def.c`）→ (随机池份数, 保证位下限)。

    排除规则与 `+0x14 == 6` 的语义**照抄那个函数**，别在这里另立一套。
    """
    pool, guaranteed = RETAIL_SHOP_POOL, 6                      # 6 = CE_SHOP_RANDOM_MAX
    for c in cards.values():
        w, dm = c.get("weight"), c.get("dmode", 0)
        if w == 0:
            guaranteed += 1
        elif w != 6:
            pool += w + 5
        if isinstance(dm, int) and 1 <= dm <= 5:
            guaranteed += 1
    return pool, guaranteed


# ══════════════════════════════════════════════════════════════════════════
# 台账行
# ══════════════════════════════════════════════════════════════════════════
class Row:
    def __init__(self, key, title, used, limit, bound, grow, readers, plan, risk,
                 evidence, gate=True, note=""):
        self.key, self.title = key, title
        self.used, self.limit = used, limit
        self.bound, self.grow, self.readers, self.plan = bound, grow, readers, plan
        self.risk, self.evidence, self.note = risk, evidence, note
        self.gate = bool(gate) and isinstance(used, int) and isinstance(limit, int) and limit > 0

    @property
    def headroom(self):
        return (self.limit - self.used) if self.gate else None

    @property
    def pct(self):
        return (self.headroom / self.limit * 100.0) if self.gate else None


def build_rows():
    rows = []
    cards = cards_js()
    ids = ce_card_ids()
    id_set = [i for i, _, _ in ids]
    sg = defines(NATIVE / "sites_gen.h")
    cd = defines(NATIVE / "cards_def.h")
    sk = defines(NATIVE / "sdk_core.h")
    en = defines(NATIVE / "engine.h")

    max_rows = need(sg, "CE_MAX_ROWS", "sites_gen.h")
    id_max = need(cd, "CE_CARD_ID_MAX", "cards_def.h")
    max_new = need(cd, "CE_CARD_MAX_NEW", "cards_def.h")
    shop_slots = need(cd, "CE_SHOP_POOL_SLOTS", "cards_def.h")
    offer_slots = need(cd, "CE_SHOP_OFFER_SLOTS", "cards_def.h")
    text_entries = need(sg, "CE_TEXT_ENTRIES", "sites_gen.h")
    snd_rows = need(sg, "CE_SND_CFG_ROWS", "sites_gen.h")
    snd_new = need(sg, "CE_SND_NEW_N", "sites_gen.h")
    snd_names = need(sg, "CE_SND_NAMES_N", "sites_gen.h")
    state_slots = need(sk, "CE_STATE_SLOTS", "sdk_core.h")
    state_bytes = need(sk, "CE_STATE_BYTES", "sdk_core.h")
    state_res = need(sk, "CE_STATE_RESERVED", "sdk_core.h")
    caves = cave_sizes()

    # ── A1 卡 id 空间 ────────────────────────────────────────────────────
    rows.append(Row(
        "card_ids", "卡 id 空间（表行 / 分配器 / 商店循环）", len(id_set), id_max,
        "原生 58 行 + `allocate_new_card` 的 `cmp ebx,0x38`（`0x411479`）⇒ 上界 `CE_CARD_ID_MAX`",
        "`CE_MAX_ROWS` = 255：整表搬 codecave `th18_card_table` + 跳转表搬 `th18_card_jumptable`",
        "`python3 sites.py check`（99 处站点）+ `audit_interior`（整区间再扫一遍）",
        "抬 `CE_CARD_ID_MAX` 要同时动 3 条商店循环上界与 255 项跳转表 —— **先处理 abcard entry 那条墙**",
        "✅", "`native/sites.py` L46-80、`native/sites_gen.h` L4-15；patch `allocate_new_card: cmp ebx, 0x38 → 0xfe`",
    ))

    # ── A1b zAbilityManager.owned[] ──────────────────────────────────────
    rows.append(Row(
        "owned", "`zAbilityManager.owned[]`（每局「拥有这张卡」位图）",
        max(id_set) + 1, max_rows,
        "原生 `int[56]`（`+0xc84`..`+0xd64`，整个对象止于 `+0xd70`）⇒ id ≥ 56 会写到自己对象之外",
        "12 条等长 binhack：owned[] 搬到对象尾 `+0xd70`、对象 `0xd70 → 0xd70 + rows*4`（`sites.py` 战线 C）",
        "`selfcheck.c` 的 `check_grow()` 按**运行时** rows 重算那 12 处的字节（`grow:` 自报行）+ 商店三处循环上界",
        "owned[] 跟着 `CE_MAX_ROWS` 走，12 处全部由 rows 现算 ⇒ 抬表行数不用手改界",
        "✅", "`native/sites.py` L53-56 / L416-445、`native/sites_gen.h` L124-125、`native/selfcheck.c` L145-170",
        note=f"用量 = 最高在用 id + 1（现在 {max(id_set) + 1}）；剩下的是**幻影 id** —— 表行已开、"
             "没有卡注册，装载器把 56/57 两行的 `+0x14` 权重写成 6，三条商店筛选都过不了",
    ))

    # ── A2 图鉴条目数 ────────────────────────────────────────────────────
    rows.append(Row(
        "menu_rows", "图鉴 / 编成条目数（`num_total_cards`）", len(id_set), max_new,
        "原生条目 56 写死在 7 处立即数；其中两处是**循环上界** `cmp r,imm8`（符号扩展）⇒ 旧上界 127",
        "① patch 把那两处**原地换成**「无符号字节比较 + `jb`」（`sites.py` 的 `MENU_COUNT_BCMP`，上界 → 255）；"
        "② 其余 5 处立即数 patch 不动，由 DLL 在自检门里按「56 + 已注册数」现写",
        "`sites_gen.h` 的 `CE_MENU_COUNT`（7 处）+ `CE_MENU_COUNT_OP`（2 处编码改写的回读）+ `CE_MENU`（`__card_ids` 20 处）",
        "条目数 > 127 需要那两条编码改写；> 255 就得再动表行（`CE_MAX_ROWS`）",
        "✅", "`native/sites.py` 的 `MENU_COUNT_SITES` / `MENU_COUNT_BCMP`、`sites_gen.h` 的 `CE_MENU_COUNT*`",
    ))

    # ── A3 文案缓冲 ──────────────────────────────────────────────────────
    slots_text = max_rows - text_entries
    rows.append(Row(
        "text_buffer", "卡文案缓冲（`zAbilityText` 重定向）", len(id_set), slots_text,
        "原生 `zAbilityText` 只有 57 张 × `0x1c0`（末张位置已是尾部字段，原地扩不了）",
        "不扩对象：`text.c` 自带 `s_ext[198][0x1c0]`，3 处断点算 `(ext+(id-57)*0x1c0) - ABILITY_TXT_PTR`",
        "`native/text.c` 的 `ce_text_off()` + patch 的 3 条 `文案重定向 → BP_ce_text_*`",
        "把 `CE_MAX_ROWS` 抬上去即可（缓冲按 `CE_MAX_ROWS - 57` 自动长）；上限就是 id 空间",
        "✅", "`native/text.c` L3-30、`sites_gen.h` L143-147、`native/sites.py` L118-121",
    ))

    # ── A4 存档影子数组 ──────────────────────────────────────────────────
    shadow = caves.get("th18_card_unlocked") or (ANM_ENTRY_MAX)
    rows.append(Row(
        "unlocked_shadow", "存档影子数组（`unlocked_cards`）", max(id_set) + 1, shadow,
        "原生 `zScoreFile.unlocked_cards` = `uint8_t[57]`（`+0x5f588`），其后是未知区",
        "codecave `th18_card_unlocked`（下标 = card id）；6 处读改指影子 + 3 个断点同步 side-car",
        "patch 的 6 条 `unlocked_cards 读 → 影子数组` + `BP_ce_unlock_write` / `_save_loaded` / `_unlock_all`",
        "影子大小 = id 空间；`CE_MAX_ROWS` 一抬就要同步 cave size 与 `sites.py` 里的 `0x100`",
        "✅", "`native/sites.py` L65-76、L375；`sites_gen.h` L134-141",
    ))

    # ── A5 显示顺序表 ────────────────────────────────────────────────────
    order_cave = caves.get("th18_card_order") or (max_rows * 4)
    rows.append(Row(
        "order_table", "图鉴显示顺序表", len(id_set), order_cave // 4,
        "原生 57 dword（`0x4b3600`），尾界 `0x4b36e4` 只被 `0x414b54` 的 cmp 用",
        "整表搬 codecave `th18_card_order`（6 处只换 imm32），DLL 重排 `[原生 0..55, 新 id…, 56, 57]`",
        "patch 的 6 条 `显示顺序表 → codecave（引用）`；`ORDER_EXCLUDE` 里 `0x4337f7` 是同值不同表，**不许改**",
        "cave 大小 = `CE_MAX_ROWS` 项；与 A1 同步抬",
        "✅", "`native/sites.py` L76-81、L620-632",
    ))

    # ── A6 商店池 ────────────────────────────────────────────────────────
    pool, guaranteed = shop_pool(cards)
    rows.append(Row(
        "shop_pool", "商店随机池（`pick_weighted_random_offer`）", pool, shop_slots,
        "原生是 `ebp-0x8c4` 的 `0x8c0` 字节栈缓冲 = **560 个 dword**，末尾紧贴栈金丝雀且填池无越界检查",
        "整段搬 codecave `th18_card_pool`（9 处等长改写），`sub esp` 一个字节不动",
        "`ce_shop_capacity_check`（每次装载跑）+ 9 条 `商店随机池 → codecave` binhack",
        "真撞顶先调权重；再抬 cave（`cards_def.h` 与 `sites.py` 两个常量，`_Static_assert` 钉着）",
        "✅", "`AUDIT.md` §AH、`native/cards_def.c` L105-129、`native/cards_def.h` L23",
        note=f"零售部分写死 {RETAIL_SHOP_POOL}（出处 `AUDIT.md` §AH）；新卡部分是 "
             f"`patch/th18/cards.js` 现算的 Σ(weight+5)",
    ))
    rows.append(Row(
        "shop_offers", "商店保证位（必出资源卡 + 关卡卡）",
        RETAIL_SHOP_GUARANTEED, offer_slots,
        "`AbilityShop__initialize` 的 `[ebp-0xe4]` = `CE_SHOP_OFFER_SLOTS` = 57",
        "不长大（这是「每次进店摆几个」的名额，不是池子）",
        "`ce_shop_capacity_check` 的第二个出口（`guaranteed > 57` ⇒ 装载 FAIL）",
        "新卡里 `weight == 0`（必出）或 `dmode ∈ 1..5`（关卡卡）一多就撞；改卡数据，不改界",
        "🟡", "`native/cards_def.c` L105-129",
        gate=False,
        note=f"新卡侧现算的可疑名额 = {guaranteed - 6}（保证位下限 {guaranteed}）；"
             "零售那一半没普查 ⇒ 不判余量",
    ))

    # ── A7 音效表 ────────────────────────────────────────────────────────
    n_voice = len(voice_js())
    rows.append(Row(
        "sound_rows", "音效 cfg 表 / slot 数组", snd_rows + n_voice, snd_rows + snd_new,
        "原生 84 行（`0x4c9b80`，stride `0x14`）+ 84 个 slot（`0x56c804`，stride `0x18`）",
        "四张表整搬 codecave；新 id 从 `0x54` 起预留 32 行（`sound_sites.py` 的 `NEW_N`）",
        "`native/sound.c` 的初始化自检（不变式 I1/I2）+ 51 处 `音效表扩容：…` 改写（第 52 处引用刻意不改）+ `make snd-check`",
        "**加行**：抬 `NEW_N`（4 个 cave 的尺寸都由它算）；**加 id 不加行**不用动界",
        "✅", "`native/sound_sites.py` L26-60、`sites_gen.h` L255-268、`patch/th18/voice.js`",
        note=f"用量 = 84 + `voice.js` 里登记的语音 id 数（现在 {n_voice} 个）",
    ))
    rows.append(Row(
        "sound_names", "wav 名表 / blob 指针数组", snd_names + n_voice, snd_names + snd_new,
        "原生 72 项（`NAMES_BASE` `0x4b47a0` / `BLOB_BASE` `0x56cfe4`）",
        "同上（cave `th18_snd_names` / `th18_snd_blobs`）",
        "`sound.c` 的 `ce_snd_selfcheck()` + `make snd-check`",
        "抬 `NEW_N` 即可；⚠ 释放尾界**刻意只到原生 72**（语音 blob 来自 thcrap 堆，交给游戏 free 会崩）",
        "✅", "`native/sound_sites.py` L38-59、L122",
    ))

    # ── A8 卡对象 / 私状态 ───────────────────────────────────────────────
    rows.append(Row(
        "state_slots", "每局同时持有的卡对象数（`ce_state` 槽）", None, state_slots,
        "引擎 `cmp [edi+0x28],0x100`（`0x411469`）⇒ 一局最多 256 个卡对象",
        "不长大：`sdk_core.h` 的静态槽表按 256 开（256 × 512 B = 128 KiB）",
        "`ce_state_alloc()` 满时返回 NULL（调用方必须按「这次拿不到状态」处理）",
        "比 id 空间宽（256 > 255），现实中不会先撞；真撞了要改引擎立即数",
        "✅", "`native/sdk_core.h` L12、`native/sites.py` L61",
        gate=False, note="用量 = 运行期同时存在的卡对象数，静态推不出来",
    ))
    rows.append(Row(
        "state_bytes", "单卡私有状态字节数（`ce_state_user`）",
        None, state_bytes - state_res,
        f"`CE_STATE_BYTES` {state_bytes} − `CE_STATE_RESERVED` {state_res}（块头留给 SDK）",
        "2026-09-08 已从 256 抬到 512（股市 id 80 要 430+ 字节）",
        "**每张卡文件尾的 `_Static_assert(sizeof(x_card_t) <= 496)`** —— 编译期闸，比运行期硬",
        "抬 `CE_STATE_BYTES`（静态表 = 256 × 字节数，注意 DLL 体积）",
        "✅", "`native/sdk_core.h` L13/L53；`grep -rho '_Static_assert(sizeof([a-z_]*_card_t'` = 11 处",
        gate=False, note="每张卡各占多少，没有单一数字；判据是那些 `_Static_assert` 全绿",
    ))

    # ── B. anm 侧 ───────────────────────────────────────────────────────
    # 上限从**生成器源码**取（`build_*.py` 的 `MAX_ENTRIES`）——那道门才是真正执行界的东西。
    ab_cap, ability_cap = gen_anm_caps()
    ab = abcard_order()
    ab_used = ABCARD_RETAIL_ENTRIES + 2 * len(ab)
    rows.append(Row(
        "abcard_entry", "`abcard.anm` entry（卡图：大图 + 小图各一条）", ab_used, ab_cap,
        f"**引擎硬顶 {ANM_ENTRY_MAX}**：贴图键 = `(槽号 << 8) | entry 下标`，绑贴图只取低 8 位"
        f"（`0x486d38` / `0x47e1a4`）；构建期硬门 = `build_abcard.py` 的 `MAX_ENTRIES` = {ab_cap}",
        "只追加（`build_abcard.py`）：每张新卡 2 条 entry（`sprite_large` / `sprite_small`）",
        f"`build_abcard.py` 的 `MAX_ENTRIES` 硬门（构建期 `SystemExit` + 印余量）+ `make anm-verify`",
        "① `_max`/`_min` 并进**一个** entry（一个 entry 两个 sprite）⇒ 69 → 138 张；② 拆到别的空闲 anm 槽（槽号 < `0x21`）",
        "✅", f"`engine/anm/th18/06-entry-limit-and-multi-sprite.md` §1.1/§2.5；`assets/build_abcard.py`"
              f"（`MAX_ENTRIES` = {ab_cap}，2026-09-13 补的硬门）",
        note=f"原生 {ABCARD_RETAIL_ENTRIES} 条 + 已追加 {2 * len(ab)} 条（{len(ab)} 张卡）"
             f"⇒ **还能进 {(ab_cap - ab_used) // 2} 张**（比 `CE_CARD_MAX_NEW` 的 {max_new} 更紧 ——"
             f" 卡侧**最先**撞的就是这道墙）",
    ))

    n_entry, n_sprite, n_ref, _ = ability_order()
    rows.append(Row(
        "ability_entry", "`ability.anm` entry（卡特效 / HUD 图集）",
        ABILITY_RETAIL_ENTRIES + n_entry, ability_cap,
        f"同 abcard（引擎硬顶 {ANM_ENTRY_MAX}）；构建期硬门 = `build_ability.py` 的 "
        f"`MAX_ENTRIES` = {ability_cap}",
        "`ORDER.txt` 的图集段：多张小图并进一个 entry（**只许追加**，`ORDER.frozen.txt` 是硬门）",
        f"`build_ability.py` 的 `MAX_ENTRIES` 硬门（构建期 `SystemExit` + 印余量）+ `make anm-verify`",
        "继续并图集：把相关的十几张再收进一个画布（`ATLAS-MERGE-AUDIT.md`）",
        "✅", f"`assets/build_ability.py`（`MAX_ENTRIES` = {ability_cap}）、`assets/ability/entries/ORDER.txt`",
        note=f"追加 {n_entry} 条 entry（单图 + 图集）、sprite 成员 {n_sprite} 个、REF {n_ref} 行（退役名不占 entry）",
    ))
    atlas, scripts, n_spr_macros, spr_range = anm_macros()
    rows.append(Row(
        "ability_sprite", "`ability.anm` sprite 表", ABILITY_RETAIL_SPRITES + n_sprite, None,
        "**无上限**：sprite 号是 dword（`vm+0x24`），解析器 `0x477b00` 按 `idx*0x44` 取",
        "不适用（把它当成 255 上限是**旧的误读**）",
        "`check_ability_anm_bounds.py`（脚本引用的 sprite 必须存在）",
        "不适用",
        "✅", "`engine/anm/th18/06-entry-limit-and-multi-sprite.md` §1.2、`assets/check_ability_anm_bounds.py`",
        gate=False, note=f"原生 {ABILITY_RETAIL_SPRITES} + 追加 {n_sprite}",
    ))
    rows.append(Row(
        "ability_script", "`ability.anm` 脚本表", len(scripts), None,
        "**无上限**：脚本号是 dword（`anm+0x120` 指针表 / `anm+0x10c` 脚本体）",
        "不适用",
        "`anm-verify`（脚本号必须连续、脚本里的 `@NAME@` 必须都能解析）",
        "不适用",
        "✅", "`assets/anmlib.py`、`native/anm_ids.h`",
        gate=False,
        note=f"`anm_ids.h`：{len(scripts)} 个追加脚本宏、{len(atlas)} 个图集宏、"
             f"{n_spr_macros} 个 sprite 宏（号 {spr_range[0]}..{spr_range[1]}）",
    ))

    pl_e, pl_s = pl_plan()
    rows.append(Row(
        "pl_anm_entry", "`pl0X.anm` entry（自机弹贴图）", max(pl_e.values()), ANM_ENTRY_MAX,
        "同 B1（256 entry / 文件）",
        "`build_player_anm.py` 只追加（原生部分逐字节不变，`--verify-only` 回读校验）",
        "`check_player_anm_bounds.py`（sprite 越界 + sht 的 anm/hit_fx 必须存在）",
        "同一份 anm 里继续并 entry（一个 entry 多 sprite，117 的兽灵已经这么做）",
        "✅", "`assets/build_player_anm.py`（`PLAYER_ART_ENTRIES` / `TH13_TRANCE_ENTRIES`）、`native/pl_anm_ids.h`",
        note="四份用量：" + "、".join(f"{k} {v}" for k, v in sorted(pl_e.items())) +
             "；追加脚本 " + "、".join(f"{k} {v}" for k, v in sorted(pl_s.items())) + " 个",
    ))

    # ── C. shooterset ───────────────────────────────────────────────────
    sets = sht_sets()
    slots, reserved, dbase = sht_expansion_check()   # 生成物 ↔ patch ↔ 结构性恒等式 三方对账
    extra = sum(1 for v in sets.values() if v >= 0x17)      # 本 mod 的 APPEND 新占格（0x17–0x27）
    used = SHT_SET_RETAIL_USED + 1 + extra                  # 零售 22（0x01–0x16）+ 槽 0 + APPEND
    if used > slots:
        raise SystemExit(f"capcheck: shooterset 用量 {used} > {slots} 格 —— sht_ids.h 与扩容常量对不上")
    rows.append(Row(
        "sht_groups", "shooterset 组数组（`pl0X.sht` 影像 `+0xe0`）",
        used, slots,
        f"**{slots} 格宣布可用**（数组区 `[+0xe0, +{dbase:#x})` 预留 {reserved} 格，末尾正好顶到数据区）；"
        f"计数 = 影像头 `+0x02` 的 u16",
        "**2026-09-14 已扩容**（`AUDIT.md` §CQ / `NEXT.md` §1w）：数组区原地长到 256 格（宣布可用 96）、"
        "数据区基点 `0x180 → 0x4e0`（exe 侧 **1 条** 6 字节等长 binhack）；14 个读者的位移 `+0xe0` 一个字节没动",
        "`sht-verify`（计数 96 / 数组正好顶到数据区 / 前 23 组的记录与偏移项逐字节不变）+ 本脚本的三方常量对账"
        "（`sht_ids.h` ↔ patch 的 `sht_dbase_*` code ↔ 恒等式）+ 读者普查（`CAPACITY.md` §4：14 处引用）",
        f"再要更多格：**只改文件**（抬 `CE_SHT_SLOTS` 到目标槽之上 + 写槽位）—— 预留到 0x{reserved - 1:x}，"
        f"不用再动 exe；计数内 `0x28` 起还剩 {slots - used} 格直接可用",
        "✅", "`assets/sht/append_shooterset.py`、`native/sht_ids.h`、`native/sites.py`、`NEXT.md` §1w",
        note=f"零售占 {SHT_SET_RETAIL_USED} 格（`0x01`–`0x16`）+ 槽 0；本 mod 的 `APPEND` 再占 {extra} 格"
             f"（`0x17`–`0x27`）⇒ 余量 {slots - used} 格（{100.0 * (slots - used) / slots:.1f}%）；"
             f"其中 113 的收尾（换永久槽 `0x28`）与 111 的 31 组（23 子机 + 8 主炮）已登记在 `sht_ids.h` 的分配表里",
    ))

    # ── D. 卡本地池 ─────────────────────────────────────────────────────
    for key, macro, def_file, scan_file, diag, plan in POOLS:
        d = defines(CARDS / def_file)
        val = d.get(macro)
        if val is None:
            raise SystemExit(f"capcheck: `{macro}` 不在 `native/cards/{def_file}` 里了 —— 池搬走了，先修脚本")
        has = (diag is not None) and (diag in read_text(CARDS / scan_file))
        rows.append(Row(
            key, f"卡本地池 `{macro}`", None, val,
            f"我们自己定的（`{def_file}` 的 `#define {macro} {val}`）",
            "改常量即可 —— 但 `sizeof(x_card_t) <= 496` 的 `_Static_assert` 会拦住静态区超限",
            f"运行时计数 `{diag}`（`cards/{scan_file}`）" if has else "**无运行时余量断言**（静态检查也没有）",
            plan, "✅" if has else "⚠",
            f"`native/cards/{def_file}` 的 `#define {macro}` + `cards/{scan_file}` 的满/丢处理",
            gate=False,
            note="" if has else "★ 满了只会静默退让/淘汰 ⇒ 实跑里看不出来；建议加一个计数（先走 `AUDIT.md` §BE）",
        ))

    # ── E. 我们不长的界 ─────────────────────────────────────────────────
    rows.append(Row(
        "item_pool", "道具池（`ItemManager`，**我们不长大**）",
        None, need(en, "CE_ITEM_SLOTS", "engine.h"),
        "原生：构造器把 `mgr+0x14` 起 4696 个槽（stride `0xc94`）挂成双向空链表；循环 `cmp eax,0x1258`（`0x4467fc`）",
        "**不长大**（改它会连带几十处链表/遍历假设）；卡侧只**扫**它",
        "`CE_ITEM_SLOTS` 的读者：`sdk.c` L1500、`sdk.h` L739、`cards/ufo.c`、`cards/th06rank.c`",
        "撞顶的表现是**静默 spawn 失败**（空链表返回 0）：跨帧分批撒 / 排队补撒，**别去抬这个界**",
        "🟡", "`native/engine.h` L491-494、`AUDIT.md`（钱鱼灵 150）第 2 条",
        gate=False, note="用量 = 运行期在场道具数，静态推不出来",
    ))
    return rows


# ══════════════════════════════════════════════════════════════════════════
# 附录 A（`CAPACITY.md` 里的名单；判「有没有新的容量消费者」就靠它）
# ══════════════════════════════════════════════════════════════════════════
def gen_appendix() -> str:
    ids = ce_card_ids()
    sg = sht_sets()
    atlas, scripts, n_spr, rng = anm_macros()
    L = []
    A = L.append
    A(BEGIN + "（本段由 `native/tools/capcheck.py --gen-appendix` 生成，**别手改**）-->")
    A("")
    A(f"**A1. 卡 id（`CE_CARD(` 的落点，{len(ids)} 张）** —— 出处 `native/cards/*.c`")
    A("")
    A("```")
    A(" ".join(str(i) for i, _, _ in ids))
    A("```")
    A("")
    A(f"**A2. shooterset 组（`native/sht_ids.h`，{len(sg)} 个宏）**")
    A("")
    A("```")
    for k in sorted(sg, key=lambda x: (sg[x], x)):
        A(f"{k} {sg[k]:#06x}")
    A("```")
    A("")
    A(f"**A3. 图集 entry（`CE_ANM_ABILITY_ATLAS_*`，{len(atlas)} 个）** —— 每个 = `ability.anm` 一个 entry")
    A("")
    A("```")
    L.extend(atlas)
    A("```")
    A("")
    A(f"**A4. 追加脚本（`CE_ANM_ABILITY_SCRIPT_*`，{len(scripts)} 个）**")
    A("")
    A("```")
    L.extend(scripts)
    A("```")
    A("")
    A("**A5. sprite 汇总（`CE_ANM_ABILITY_SPRITE_*`）** —— sprite 号是 dword、**不是容量**，"
      "所以这里只登记总数与号段（加一张美术这两行就变 ⇒ 台账必须跟着刷新）")
    A("")
    A("```")
    A(f"CE_ANM_ABILITY_SPRITE 宏 {n_spr} 个，号 {rng[0]}..{rng[1]}")
    A("```")
    A("")
    A(END)
    return "\n".join(L) + "\n"


def current_appendix():
    if not DOC.is_file():
        raise SystemExit(f"capcheck: 没有 {DOC} —— 台账还没写？")
    txt = read_text(DOC)
    i, j = txt.find(BEGIN), txt.find(END)
    if i < 0 or j < 0:
        raise SystemExit(f"capcheck: {DOC.name} 里找不到附录 A 的标记（`{BEGIN}…` / `{END}`）")
    return txt[i:j + len(END)] + "\n", txt[:i], txt[j + len(END):]


def appendix_ids(block: str):
    """附录 A1 段里登记过的卡 id（空格分隔的十进制 token）。"""
    m = re.search(r"\*\*A1\..*?```\n(.*?)```", block, re.S)
    if not m:
        raise SystemExit("capcheck: 附录 A 里找不到 A1（卡 id 清单）段")
    return {int(x) for x in re.findall(r"\b(\d+)\b", m.group(1))}


def name_gate(doc_text, block, rows):
    """③ 有界消费者：新 卡 id / 新 shooterset 组 / 新图集 entry 必须已经在台账里出现过。"""
    bad, soft = [], []
    ids_now = {i for i, _, _ in ce_card_ids()}
    ids_doc = appendix_ids(block)
    if ids_now - ids_doc:
        bad.append(f"新卡 id {sorted(ids_now - ids_doc)} 不在 `CAPACITY.md` 附录 A1 里 —— "
                   f"跑 `python3 native/tools/capcheck.py --gen-appendix` 重新登记")
    for label, names in (("shooterset 组", set(sht_sets())),
                         ("图集 entry", set(anm_macros()[0]))):
        miss = sorted(n for n in names if n not in doc_text)
        if miss:
            bad.append(f"新的{label}宏没登记进 `CAPACITY.md`：{miss[:8]}"
                       f"{' …' if len(miss) > 8 else ''}（共 {len(miss)} 个）—— 台账必须说清它占的是哪一格")

    cur = block
    new = gen_appendix()
    if cur != new:
        soft.append("附录 A 与仓库现状有差异（**不判红**：脚本 / sprite 无上限，只是可见性登记）。\n"
                    "   想刷新：`python3 native/tools/capcheck.py --gen-appendix`")
    return bad, soft


def write_appendix():
    cur, head, tail = current_appendix()
    new = gen_appendix()
    if cur == new:
        print("附录 A 已是最新（无需改写）")
        return 0
    DOC.write_text(head + new + tail, encoding="utf-8")
    print(f"已改写 {DOC.relative_to(MOD)} 的附录 A")
    return 0


# ══════════════════════════════════════════════════════════════════════════
# 输出
# ══════════════════════════════════════════════════════════════════════════
def dwidth(s: str) -> int:
    return sum(2 if unicodedata.east_asian_width(c) in "WF" else 1 for c in s)


def pad(s, n):
    return s + " " * max(n - dwidth(s), 1)


def explain(rows, key):
    for r in rows:
        if r.key != key:
            continue
        print(f"【{r.key}】{r.title}")
        print(f"  界是谁定的      : {r.bound}")
        print(f"  我们怎么长大    : {r.grow}")
        if r.gate:
            print(f"  用量 / 上限     : {r.used} / {r.limit}（余量 {r.headroom}，{r.pct:.1f}%）")
        elif isinstance(r.limit, int):
            print(f"  用量 / 上限     : {'—' if r.used is None else r.used} / {r.limit}（不进余量闸）")
        else:
            print(f"  用量 / 上限     : {'—' if r.used is None else r.used} / 无上限")
        print(f"  谁在读它        : {r.readers}")
        print(f"  加满时怎么办    : {r.plan}")
        print(f"  风险等级        : {r.risk}")
        print(f"  出处            : {r.evidence}")
        if r.note:
            print(f"  注              : {r.note}")
        return 0
    raise SystemExit(f"capcheck: 没有这条容量：{key}\n  可用：{'、'.join(r.key for r in rows)}")


def show(rows):
    w = max(dwidth(r.title) for r in rows)
    head = pad("容量", w + 2) + "用量 / 上限" + " " * 6 + pad("余量", 7) + pad("余量%", 8) + "风险"
    print("card-expand 容量台账（`CAPACITY.md`）对账 —— 口径：余量 < %.0f%% 且无豁免 ⇒ 红\n"
          % (MARGIN * 100))
    print(head)
    print("-" * (w + 36))
    for r in rows:
        if r.gate:
            pct = f"{r.pct:.1f}%"
            print(pad(r.title, w + 2) + pad(f"{r.used} / {r.limit}", 14)
                  + pad(str(r.headroom), 7) + pad(pct, 8) + r.risk)
        else:
            used = "—" if r.used is None else str(r.used)
            lim = str(r.limit) if isinstance(r.limit, int) else "无上限"
            print(pad(r.title, w + 2) + pad(f"{used} / {lim}", 14) + pad("—", 7) + pad("—", 8) + r.risk)
    print()
    for r in rows:
        if r.note:
            print(f"  {r.risk} {r.title.split('（')[0]}：{r.note}")
    print()
    print("说明：`—` = 不进余量闸（用量只有运行期才知道，或上限本身是我们自己设的口径）；"
          f"⚠ = 缺运行时余量断言。{SPRITE_NOTE}：sprite 号不是容量，只登记总数与号段。")


def main():
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    ap.add_argument("--explain", metavar="NAME", default=None, help="打印该行的全部溯源")
    ap.add_argument("--gen-appendix", action="store_true", help="重新生成 CAPACITY.md 的附录 A")
    ap.add_argument("--print", dest="only_print", action="store_true", help="配 --gen-appendix：只打印不写文件")
    a = ap.parse_args()

    if a.gen_appendix:
        if a.only_print:
            sys.stdout.write(gen_appendix())
            return 0
        return write_appendix()

    rows = build_rows()
    if a.explain:
        return explain(rows, a.explain)

    show(rows)
    bad = []

    # ① 余量闸
    low = [r for r in rows if r.gate and r.pct < MARGIN * 100]
    for r in low:
        anchor = WAIVERS.get(r.key)
        if not anchor:
            bad.append(f"【{r.key}】{r.title}：余量 {r.pct:.1f}%（{r.used}/{r.limit}）"
                       f"低于口径 {MARGIN * 100:.0f}%，且没有登记豁免")
        elif anchor not in read_text(NEXT_MD):
            bad.append(f"【{r.key}】登记了豁免「{anchor}」，但 `NEXT.md` 里找不到这段 —— "
                       f"计划没了，豁免作废（要么补计划，要么修 `CAPACITY.md`）")
        else:
            print(f"⚠⚠ 余量 {r.pct:.1f}% < {MARGIN * 100:.0f}%：{r.title}\n"
                  f"     {r.used} / {r.limit} —— 已登记豁免（`NEXT.md` §「{anchor}」）；再满就没有出路了")

    # ②③ 名单闸（有界消费者判红，脚本/sprite 只提示）
    block, _, _ = current_appendix()
    doc_text = read_text(DOC)
    nb, soft = name_gate(doc_text, block, rows)
    bad += nb
    for s in soft:
        print(f"⚠ {s}")

    # ③ 缺运行时断言的池（只警告，不判红）
    warn = [r for r in rows if r.risk == "⚠"]
    if warn:
        print()
        print(f"⚠ {len(warn)} 条容量没有运行时余量断言（满了静默退让，实跑里看不出来）：")
        for r in warn:
            print(f"   · {r.title}：{r.plan}")
        print("   —— 建议在卡里加一个计数（动别人的文件前先按 `AUDIT.md` §BE 登记）。")

    print()
    if bad:
        print("capcheck：**红** —— 以下 %d 条不通过：" % len(bad))
        for i, b in enumerate(bad, 1):
            print(f"  {i}. {b}")
        return 1
    print("capcheck：全绿 ✅ —— 台账与仓库现状一致，没有容量低于余量口径。")
    return 0


if __name__ == "__main__":
    sys.exit(main())
