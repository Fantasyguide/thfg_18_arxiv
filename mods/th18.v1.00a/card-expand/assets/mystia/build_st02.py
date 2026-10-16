#!/usr/bin/env python3
"""米斯蒂亚那场「使魔拖尾火花」（纯 anm，不依赖 108 卡 / DLL）—— **spec 编辑逻辑的可复用原型**。

★★ 定位（owner 2026-09-30 改口径之后，**别再当产物生成器用**）
  owner 定案「**都开强强联合，只开一个也能用**」⇒ 我们**不能**把 `th18/st02enm.anm` 装进自己的 patch：
  那会和第三方 **leveledit 补丁同名**（thcrap 同名文件**后加载者赢** ⇒ 一方坏掉，「只开一个也能用」就不成立）。
  ⇒ 原方案 **B（我们 patch 出整文件产物）作废**。要做的三处编辑**搬到 leveledit 的源**（上游分支）上去。
  本脚本现在的用途只有两个：
    ① 把「**该改哪三处、怎么改**」写成可执行、可回归的**规格**（下面 ① ② ③）；
    ② 出一份**预览 anm**（默认落 `local/_scratch/…`，gitignored）跑往返核对，证明这套编辑逻辑自洽。
  ⚠ **不要**把输出指到 `mods/th18.v1.00a/card-expand/patch/th18/` —— 那等于把作废的 B 又装回去。

── 要搬到 leveledit 源上的三处编辑 ────────────────────────────────────────────
  ① **新 entry**：16×16 火花贴图 = TH08 `etama.anm` **sprite219 `(0,96,16,16)`** 的同格切图；
     entry 号 = 现有 entry 数；sprite 号 = **`max_sprite_id(source) + 1`**。
     ⚠ TH18 的 sprite 表是**全文件扁平**的（`engine/anm/th18/06-entry-limit-and-multi-sprite.md` §2）
     ⇒ 图省事写 `sprite0` 会和 entry0 的 sprite0（512×512 敌机图集第 0 格）**撞号**、静默改坏原生 entry0。
  ② **新脚本**：TH08 `etama.anm` **script71** 的零翻译配方 —— 加法、`scale = 1.1 + rand(1.0)` →
     `scaleTime(20,0,0,0)`、`alpha = 48 + rand(64)`、`color = 128 + rand(64)`×3、`+20: delete()`；
     `layer(7)/type(0)` 写在**子**脚本里（照原生 `enemy.anm` script89/91 的先例）。
  ③ **触发**：使魔本体那两支循环（本份 = script28/29）里，每个 `+4:` ⇒
     `+2: scriptNewRoot(新脚本, 0, 0); +2: scriptNewRoot(新脚本, 0, 0)` —— 理由见下。

★★ 2026-10-15 订正（leveledit 侧一手 ✓）：上面 ③ 原稿写的是 **`scriptNewPos`(505)** ✗ ——
   上游 `Fantasyguide/th18-leveledit` 的 commit `fc67cee` 明写「**505 会把火花画到世界原点**」✗
   ⇒ 正确 opcode 是 **`scriptNewRoot`(504)** ✓（本文件只做**规格留痕** ✓；产物由 leveledit 那条线自己出 ✓）。

── 一手依据（TH08 侧；地址族见 `engine/enemy/th08/04-mystia-boss-familiars-leveledit-port.md` §9/§10）──
  TH08 的使魔拖尾 = 引擎**每 2 帧**在使魔位置撒 `SpawnEffect(0x26)`（`th08:0x42c4b1` 的常数 2 +
  消费者 `Enemy::FUN_0042c420`），模板正是 `etama.anm` script71。
  TH18 侧的落法：子 VM 由 `scriptNewPos`(505) 创建 —— `AnmVm__run th18:0x479b30`–`0x479b5d` 只写子 VM 的
  `__pos_2`，绘制 = `pos + entity_pos + __pos_2` ⇒ **只在创建那一刻相对父定位、之后自由飘**（火花留在撒点）。

★ 为什么把 `+4:` 拆成「两段 `+2:` + 各一颗」（**不要**只发 2 次）
  使魔循环是 **`+4:` 步进**（4 tick 一步、16 tick 一轮）。若按「4 帧循环里挑 2 帧」去发，相邻两次间隔是
  **4 tick**（同段）或 **8 tick**（隔段）——都不是一手的 **2 帧**。拆成两段 `+2:` 后回读实证：
  `scriptNewPos` 落在 tick **2/4/6/8/10/12/14/16** = 8 颗 / 16 tick ⇒ **每 2 tick 一颗**；
  而 `sprite()` 仍压在**原来的 tick**（0/4/8/12）⇒ **sprite 动画还是 4 tick 一轮**（速度不变）。
  稳态在飞 = 20/2 = **10 颗/台**。

── ⚠ 工具坑（上一个 agent 卡死在第一条，别再踩）─────────────────────────────
  1. **`thanm -c` 按 entry 的 `name:` 相对 `cwd` 找贴图**（本份 = `stgenm/stage02/enm2.png` 一族）。
     抽取目录必须**就是**编译 cwd：`thanm -x` 解到 `tex/`、却拿 `td/`（上一级）去 `-c`
     ⇒ 报 `stgenm/stage02/enm2.png: could not open for reading`。
  2. **不要自己拼 subprocess**：走本仓 `assets/anmlib.py` ——
     `L.thanm([...], cwd, tools)`（`-l` / `-x`）、`L.compile_anm(spec, tex_dir, out, tools, anmmap)`
     （内部 = 「spec 写进 tex_dir + cwd=tex_dir 跑 `-c`」，天然避开坑 1）、`L.parse_entries/parse_scripts`、
     `L.max_sprite_id`、`L.insert_entries/append_scripts/substitute`；工具路径用 `L.find_tools()` 探测。
     （`anmlib.link_retail_textures` 是给「unpack 解出来的原生贴图目录」用的；本份来源是 leveledit 的
     **anm 产物** ⇒ 直接 `-x` 抽进编译 cwd，语义等价，不必再软链一遍。）
  3. **anmmap 按 anm 头 version 选**（TH18 的头 = 8 ⇒ `v8.anmm`），**不是**按作品号；选错不报错、
     只会把助记符译成垃圾。
  4. `thanm -l` 会给指令补 `// tick` 注释、丢掉我们写的注释 ⇒ 「spec ↔ 回读」逐字比会全是噪声；
     要比就比**去注释后的语义**（本脚本的 `_norm`），并且**按 entry/script 块**比，不要做整文件行集合差。

    python3 assets/mystia/build_st02.py [预览输出路径]
"""
import difflib
import re
import shutil
import sys
import tempfile
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))   # assets/ → anmlib
import anmlib as L  # noqa: E402

CARD = L.MOD
REPO = L.REPO
TOOLS = L.find_tools()                     # (thanm, thdat, anmmap_dir)：本仓统一的工具探测
ANMMAP = TOOLS[2] / "v8.anmm"              # ⚠ 按 anm 头 version 选（TH18 = 8），不是按作品号
VERSION = L.VERSION                        # "18"

# ══════════════════ ★ 来源路径（唯一一处；搬到 leveledit 源时就改这一行） ══════════════════
# 现状 = leveledit 补丁产物的**本机草稿副本**。补丁正本在 modkit 的
#   `thcrap/repos/leveledit/th18_leveledit_stage02_mystia/` 一类的目录；本机**没有**那个 clone。
# 搬到 leveledit 源（上游分支）时：把这一行指向那边仓库里的 `st02enm.anm`，把下面三处编辑写进去。
SOURCE = REPO / "local/_scratch/c108/patch/st02enm.anm"
# ═══════════════════════════════════════════════════════════════════════════════════════

TRAIL_PNG = CARD / "assets/ability/youkai_familiar/YOUKAI_FAM_TRAIL.png"  # = TH08 sprite219 同格（16×16）

# ★ 预览产物只落 `local/` 草稿（gitignored）。
#   ⚠ **不要**改到 `patch/th18/st02enm.anm` —— 方案 B 已作废（与 leveledit 补丁同名冲突，见文件头）。
PREVIEW_ANM = REPO / "local/_scratch/c108/build/st02enm.trail.anm"
OUT_ANM = Path(sys.argv[1]) if len(sys.argv) > 1 else PREVIEW_ANM

TRAIL_ENTRY_NAME = "mystia/fam_trail.png"    # 贴图路径：相对编译 cwd（thanm -x / -c 的约定）
# 使魔本体那两支 4 帧循环。搬到别的源上时**按形状核对**（见 `_patch_loop_script` 的守卫），
# 本份的判据 = 含 4 个 `+4:` + `jmp(offset…, 0)` + sprite 83..90 的 16 tick 循环。
LOOP_SCRIPTS = (28, 29)

TRAIL_BODY = """script script@TRAIL_SCRIPT@ {
    // ★★ 米斯蒂亚那场的**人侧基础层**拖尾火花（纯 anm）—— TH08 `etama.anm` script71 参数照一手：
    //   每 2 tick 一颗（由父脚本的 `+2:` 步进决定）、20 tick 寿命、加法、随机 alpha/color、
    //   `scale = 1.1 + rand(1.0)` → 0、`+20: delete()`。
    //   子 VM 自己带 layer/type（照原生 `enemy.anm` script89/91 的先例：子 VM 不继承父的那两样）。
    layer(7);
    type(0);
    sprite(sprite@TRAIL_SPRITE@);
    blendMode(1);
    fsetRand(%F0, 1.0f);
    fsetAdd(%F0, %F0, 1.1f);
    scale(%F0, %F0);
    scaleTime(20, 0, 0.0f, 0.0f);
    isetRand($I0, 64);
    isetAdd($I0, $I0, 48);
    alpha($I0);
    isetRand($I0, 64);
    isetAdd($I0, $I0, 128);
    isetRand($I1, 64);
    isetAdd($I1, $I1, 128);
    isetRand($I2, 64);
    isetAdd($I2, $I2, 128);
    color($I0, $I1, $I2);
+20: // 20
    delete();
}
"""


def _entry_block(idx: int, sprite_idx: int, png_rel: str, w: int = 16, h: int = 16) -> str:
    """编辑 ①：一个新 entry（16×16、单 sprite）—— 字段逐字对齐同文件既有 entry 的 `thanm -l` 写法。"""
    return (f"entry entry{idx} {{\n"
            f"    version: 8,\n"
            f'    name: "{png_rel}",\n'
            f"    format: 1,\n"
            f"    width: {w},\n"
            f"    height: {h},\n"
            f"    memoryPriority: 0,\n"
            f"    lowResScale: 0,\n"
            f"    hasData: 1,\n"
            f"    THTXSize: {w * h * 4},\n"
            f"    THTXFormat: 1,\n"
            f"    THTXWidth: {w},\n"
            f"    THTXHeight: {h},\n"
            f"    THTXZero: 0,\n"
            f"    sprites: {{\n"
            f"        sprite{sprite_idx}: {{ x: 0, y: 0, w: {w}, h: {h} }}\n"
            f"    }}\n"
            f"}}\n")


def _patch_loop_script(block: str, script_no: int) -> str:
    """编辑 ③：每个 `+4:` ⇒ `+2: scriptNewPos; +2: scriptNewPos`（理由见文件头）。

    守卫：只认「含 `jmp(` + 至少一个 `+4:`」的循环块 —— 搬源时脚本号如果漂了，宁可直接报错，
    也不要把 `+2:` 打进一支不相干的脚本。
    """
    if "jmp(" not in block:
        sys.exit(f"[FAIL] script{script_no} 不含 `jmp(` —— 不像使魔循环，先人工核对（脚本号漂了？）")
    out, n = re.subn(r"^\+4:.*$",
                     f"+2:\n    scriptNewPos(script{TRAIL_SCRIPT}, 0.0f, 0.0f);\n"
                     f"+2:\n    scriptNewPos(script{TRAIL_SCRIPT}, 0.0f, 0.0f);",
                     block, flags=re.M)
    if n == 0:
        sys.exit(f"[FAIL] script{script_no} 里一个 `+4:` 都没有（源换过？）")
    return out


def _norm(text: str):
    """去掉 thanm 自打的 `// tick` 注释与空行 —— 「语义往返」比对用（别做逐字比，见文件头坑 4）。"""
    return [c for c in (l.split("//")[0].rstrip() for l in text.splitlines()) if c.strip()]


TRAIL_ENTRY = TRAIL_SCRIPT = None   # main() 里按源现算（= 现有 entry/script 数）


def main():
    global TRAIL_ENTRY, TRAIL_SCRIPT
    for p in (SOURCE, TRAIL_PNG):
        if not p.exists():
            hint = "（本机草稿副本；搬到 leveledit 源时改 SOURCE）" if p == SOURCE else ""
            sys.exit(f"[FAIL] 缺文件：{p}{hint}")

    with tempfile.TemporaryDirectory() as td:
        # ★ 抽取目录 == 编译 cwd（坑 1：`thanm -c` 按 entry 的 name 相对 cwd 找贴图）
        tex = Path(td)
        src_spec = L.thanm(["-l", VERSION, "-m", ANMMAP, SOURCE], tex, TOOLS)
        L.thanm(["-x", VERSION, SOURCE], tex, TOOLS)
        dst = tex / TRAIL_ENTRY_NAME
        dst.parent.mkdir(parents=True, exist_ok=True)
        shutil.copyfile(TRAIL_PNG, dst)

        src_e, src_s = L.parse_entries(src_spec), L.parse_scripts(src_spec)
        TRAIL_ENTRY, TRAIL_SCRIPT = len(src_e), len(src_s)
        sprite_id = L.max_sprite_id(src_spec) + 1        # ★ 全文件扁平 ⇒ 必须 max+1（编辑 ①）
        print(f"[src] {SOURCE.name}：entry {len(src_e)} / script {len(src_s)}、"
              f"max sprite = {sprite_id - 1} ⇒ 新 sprite = {sprite_id}")

        # ── 三处编辑（一律走 anmlib 的助手；脚本号/entry 号按源现算，方便搬源）──
        s = src_spec.rstrip("\n") + "\n"
        s = L.insert_entries(s, [_entry_block(TRAIL_ENTRY, sprite_id, TRAIL_ENTRY_NAME)])
        body = L.substitute(TRAIL_BODY, {"TRAIL_SCRIPT": TRAIL_SCRIPT, "TRAIL_SPRITE": sprite_id})
        s = L.append_scripts(s, [body])
        want_spawns = 0
        for n in LOOP_SCRIPTS:
            m = re.search(rf"^script script{n} \{{.*?^\}}$", s, re.S | re.M)
            if not m:
                sys.exit(f"[FAIL] 找不到 script{n}")
            want_spawns += 2 * len(re.findall(r"^\+4:", m.group(0), flags=re.M))
            s = s.replace(m.group(0), _patch_loop_script(m.group(0), n), 1)
        got_spawns = s.count(f"scriptNewPos(script{TRAIL_SCRIPT}")
        if got_spawns != want_spawns:
            sys.exit(f"[FAIL] scriptNewPos 数 {got_spawns} ≠ 期望 {want_spawns}")

        # ── 编预览 + 往返核对 ──
        OUT_ANM.parent.mkdir(parents=True, exist_ok=True)
        L.compile_anm(s, tex, OUT_ANM, TOOLS, ANMMAP)
        back = L.thanm(["-l", VERSION, "-m", ANMMAP, OUT_ANM], tex, TOOLS)
        out_e, out_s = L.parse_entries(back), L.parse_scripts(back)

        bad = []

        if len(out_e) != TRAIL_ENTRY + 1 or len(out_s) != TRAIL_SCRIPT + 1:
            bad.append(f"entry {len(out_e)} / script {len(out_s)}（期望 {TRAIL_ENTRY + 1} / {TRAIL_SCRIPT + 1}）")
        for i, (a, b) in enumerate(zip(src_e, out_e)):      # 原生 entry（含 sprite 矩形）逐块一致
            if a["block"] != b["block"]:
                bad.append(f"原生 entry{i} 的块变了")
        for i in range(min(LOOP_SCRIPTS)):                  # 原生 script（除被打补丁那两支）逐块一致
            if src_s[i]["block"] != out_s[i]["block"]:
                bad.append(f"原生 script{i} 的块变了")
        for n in LOOP_SCRIPTS:                              # 两支循环：只允许那一处 `+4:` 改写
            if _norm(out_s[n]["block"]) != _norm(_patch_loop_script(src_s[n]["block"], n)):
                bad.append(f"script{n} 不是「+4: ⇒ 两颗」那一处改写")
        if _norm(out_e[TRAIL_ENTRY]["block"]) != _norm(_entry_block(TRAIL_ENTRY, sprite_id, TRAIL_ENTRY_NAME)):
            bad.append(f"新 entry{TRAIL_ENTRY} 与写下的不一致")
        if _norm(out_s[TRAIL_SCRIPT]["block"]) != _norm(body):
            bad.append(f"新 script{TRAIL_SCRIPT} 与写下的不一致")

        # 逐行归属：每个「增/删/替换」区段都必须落在允许的块里
        line_span = {}
        for e in out_e[TRAIL_ENTRY:]:
            line_span[(back.count("\n", 0, e["span"][0]), back.count("\n", 0, e["span"][1]))] = f"entry{e['idx']}"
        for x in out_s[TRAIL_SCRIPT:]:
            line_span[(back.count("\n", 0, x["span"][0]), back.count("\n", 0, x["span"][1]))] = f"script{x['idx']}"
        for n in LOOP_SCRIPTS:
            line_span[(back.count("\n", 0, out_s[n]["span"][0]),
                       back.count("\n", 0, out_s[n]["span"][1]))] = f"script{n}"
        raw_a, raw_b = src_spec.splitlines(), back.splitlines()
        sm = difflib.SequenceMatcher(None, raw_a, raw_b, autojunk=False)
        ins = dele = 0
        per_block, stray = {}, []
        for tag, i1, i2, j1, j2 in sm.get_opcodes():
            if tag != "equal":
                ins += j2 - j1
                dele += i2 - i1
            for ln in range(j1, j2):
                owner = next((v for (lo, hi), v in line_span.items() if lo <= ln < hi), None)
                if tag == "equal":
                    continue
                if owner is None:
                    if raw_b[ln].strip():                   # 块间空行不算差异
                        stray.append(raw_b[ln])
                else:
                    per_block[owner] = per_block.get(owner, 0) + 1
        if stray:
            bad.append(f"有 {len(stray)} 行差异落在允许的块之外：{stray[:3]}")

        print(f"[ok] 预览产物 {OUT_ANM}（{OUT_ANM.stat().st_size} B）；"
              f"thanm -l：entry {len(out_e)} / script {len(out_s)}")
        print(f"[ok] 往返（对原件 listing）：原生 entry0..{len(src_e) - 1} 逐块一致、"
              f"script0..{min(LOOP_SCRIPTS) - 1} 逐块一致；"
              f"script{LOOP_SCRIPTS[0]}/{LOOP_SCRIPTS[1]} = 仅 `+4:` ⇒ 两段 `+2:`×2 颗；"
              f"entry{TRAIL_ENTRY}/script{TRAIL_SCRIPT} = 新增")
        print(f"[ok] 贴图 name: 标签 = \"{TRAIL_ENTRY_NAME}\"；新 sprite 号 = {sprite_id}（全文件唯一）")
        print(f"[ok] 往返差异行 = {ins + dele}（新增 {ins} / 删除 {dele}）；逐块归属 = "
              f"{ {k: per_block[k] for k in sorted(per_block)} }；块外 = {len(stray)} 行")
        print("⚠ 预览产物**不是**发布物：方案 B 已作废，这三处编辑要搬到 leveledit 的源上（见文件头定位）。")
        if bad:
            for m in bad:
                print("   ✗ " + m)
            sys.exit(f"[FAIL] 往返核对没过（{len(bad)} 条）")
        print("[ok] 往返核对全过（差异只在允许的块里）")


if __name__ == "__main__":
    main()
