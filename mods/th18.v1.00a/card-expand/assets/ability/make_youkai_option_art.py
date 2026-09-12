#!/usr/bin/env python3
"""永夜抄（系统卡 108）**低速子机（使い魔 / option）**：零售切图 + 脚本生成。

    python3 make_youkai_option_art.py        # 生成 + 自检（可重复跑，结果一致）

来源（零售字节只在 `local/`，gitignored；构建时取，不入库）：
  TH08 三队  `local/th08.v1.00d/dat/playerNN.anm`（anm **version 3**；贴图在 .anm 内部）
  TH16 文    `local/th16.v1.00a/dat/pl03.anm`（anm **version 8**）
  复核命令   `local/vendor/thtk/build/thanm/thanm -l 8  -m .../v2.anmm local/th08.v1.00d/dat/player00.anm`
             `local/vendor/thtk/build/thanm/thanm -l 16 -m .../v8.anmm local/th16.v1.00a/dat/pl03.anm`
             ★ anmmap 按**文件头自述的 anm 版本**选：TH08 = v3 ⇒ v2.anmm；TH16 = v8 ⇒ v8.anmm。

── ★★ 一手落点：子机（option）用**哪支脚本 / 哪些 sprite**（2026-09-12，反汇编，本生成器的根据）──
**不是「一个共享图集里的一格」，也不是「机体本体图」** —— 每支机体各有自己的一支子机脚本：

  TH08（`local/_scratch/th08_full.asm` + ExpHP `funcs.json`）：
    · 子机 = `zPlayerOption` 数组（`player+0x40c` × 4、stride `0x2f4`、每格**头 0x2a4 字节就是一个 AnmVm**）
      —— 建数组：`th08:0x449d71 push 0x4 / push 0x2f4 / base = player+0x40c`（ctor = `PlayerOption::constructor`
      `th08:0x449e50`；它 reset 的正是 option+0x0 的 AnmVm）。
    · 每帧 tick：`th08:0x44bd3c cmp [ebp-0x2c],0x4` 循环，取 `option+0x2ec` 当函数指针调
      （`th08:0x44bd79 call DWORD PTR [esi+eax*1+0x6f8]`，`0x6f8 = 0x40c+0x2ec`），
      调用约定 = `fn(player /*ecx*/, option /*edx*/)`。
    · **进低速那一帧**（`th08:0x44b0e5`）把 `option+0x2c8`（state）置 1 = 出场 ⇒ 各机体的 handler 在
      state==1 里把子机脚本装进 `option+0x0` 那个 VM：
        `th08:0x44e3d7 push 0x12`（**pl00 = 18**，`0x44e3a0` = shottype 0 的 handler）
        `th08:0x44ea6f push 0x1d`（**pl01 = 29**，`0x44ea40` = shottype 1）
        `th08:0x44eba7 push 0x18`（**pl02 = 24**，`0x44eb70` = shottype 2）
      装载调用 = `AnmLoaded::load_script_into_vm` `th08:0x4069f0`（`ecx = [player+0xc]` = 本机体的 anm 句柄、
      `arg1 = option`（= 那个 AnmVm）、`arg2 = 脚本号`；它写 `vm+0x204`/`vm+0x21a`）。
      ★ 这三支脚本与本体 script5..9 **不同**，引用的 sprite 也不与本体重合（本体 = sprite11..21）。
    · 哪支属于哪队：`th08:0x4c7d40[shottype][4]`（spec §12.1）行 0/1/2 = 霊夢·紫 / 魔理沙·アリス / 咲夜·レミリア。
    · **TH08 的 v3 脚本没有 `layer()` 指令** ⇒ 这三支里一行 layer 都没有（见下面的「层」一节）。

  TH16（`local/_scratch/th16.dis`）：
    · 本体子机由 `PlayerInner::repopulate_options` `th16:0x4440e0` 重建（数量 = `floor(P/100)`）。
    · 每格：`th16:0x4443ad mov eax,[eax*4+0x492c00]`（`eax` = `CHARACTER`）→ 脚本号，
      再 `th16:0x4443d6 push` 它 + `AnmLoaded::copy_vm` `th16:0x407b20`（`imul esi,[ebp+0xc],0x5fc`+`rep movs`，
      即「把 `AnmLoaded+0x10c` 里第 N 个模板 VM 抄进子机 VM」）。
    · 直读 `th16.exe` 的 `.data`：**`th16:0x492c00` = {8, 7, 11, 11}**（`CHARACTER` = **菜单序**，
      不是文件号：`th16:0x492c10` 旁边那张 = {9,8,12,13} = 满火力形态）；
      菜单序 → anm 文件的表在 `.rdata`：`0x491ccc` 起 `pl00.anm / pl02.anm / pl03.anm / pl01.anm`
      （`CHARACTER` = 0 霊夢 / 1 チルノ / 2 **文** / 3 魔理沙，一手：`Bomb__operator_new` 的 switch，
      `engine/player/th16/02` §1）⇒ **CHARACTER=2（文）→ pl03.anm → script 11**。
    · pl03 script11 自带 `layer(13); type(0); resampleMode(1);`，8 格 16×16 = sprite32..39（一只拍翅的鴉）。

── ★ 层（`layer`）：**照抄零售**，不自己编 ──────────────────────────────────────────────
  · TH16 那支**零售原本就写 `layer(13)`**（v8 的脚本自带）⇒ 照抄。
  · TH08 那三支**零售一行 layer 都没有**（v3 那一代 VM 的层由引擎/创建方给）。TH18 引擎在
    `Player__allocate_option` `0x40a790` 里**无条件**把子机 VM 的层设成 14：
    `th18:0x40a8bc mov DWORD PTR [esi+0x18],0xe`（`zAnmVmPrefix+0x18 = layer`）⇒
    **不需要 C 侧补、也不该在这里编一个层号**：脚本不写 layer ⇒ 保持引擎给的 14（世界层）。
    （对照：TH18 零售本体子机脚本 `pl00.anm` script15/16 自己写 `layer(13)`；本 mod 的 102/113 同层。）

── ★ `ins_83`：TH08 脚本里唯一**没法逐条照抄**的一行（已实测）────────────────────────────
  TH08 三支子机脚本里都有一行 `ins_83(2);`（v2.anmm 把它译成 `ins_83`，原文是
  `# 83   dword field` = **无名 opcode 83**）。它的 handler = `th08:0x460314`（`vm+0x200 = 指令的 dword 参数`）。
  实测：`thanm -c 18` 编 v8 时报 `opcode 83 is not known to exist in version 8` ⇒ **TH18 的 anm v8 里没有这个 opcode**，
  逐条照抄在技术上做不到 ⇒ 本生成器**省略这一行**并在脚本里写明（这是与零售唯一的一处「非 sprite」差异）。
  🟡 它写的那格 `vm+0x200` 在整份 th08.exe 里**只有两个读者**（`grep` 全反汇编）：
  `th08:0x45e96c`（`AnmManager::draw_vm__player_bullet__state_2` `0x45e960` 的 0..5 跳表）与
  `th08:0x430efa`（敌人链）—— **都不在普通 VM（含子机）的绘制路径上** ⇒ 省掉它对子机画面无影响（🟡 读码，未实跑）。
  ❓ 它与 v8 的 `type(N)` 是否同义**未证实**（v8 的 `type` handler `th18:0x47b12c` 是**打包进 flags**
  `vm+0x534` 的 5 位字段，与 TH08 写独立 dword 字段不是同一种机制）⇒ 按纪律**不猜**，不换写 `type(2)`。

── ★ 中断标签：TH08 那三支用的是 **TH08 的**约定，不是 TH18 的（照抄的必然后果，见报告 ❓）──────
  TH18 零售本体子机脚本（`pl00.anm` script15/16）用 `interruptLabel(2)` = 出场、`(3)` = 进店收起、`(1)` = 销毁；
  TH16 pl03 script11 **也是这三个标签**（天然对得上）。TH08 script18/29 用的是 `(1)(2)` = 两段循环、
  `(3)(4)` = 起爆（`sprite(…)` 旋转 + `stop()`）、`(5)` = 散场 —— 引擎在
  `th18:0x45dbf0` / `0x45d2d0` / `0x45d310` 清槽时发的是**标签 1**（TH08 那三支里 = 切循环，无 delete）。
  ⇒ 逐条照抄 = 保留 TH08 的标签语义；本生成器**不改标签**（改了就不是「照抄零售」了）。

产出（只动自己的文件）：
  ability/youkai_option/YOUKAI_OPT_<角色>_S<零售 sprite 号>.png —— 零售矩形切图，**居中补透明**到 2 的幂画布。
       ★ 居中不是随手挑的：零售的子机 VM 以**中心**为锚点画（TH08 `AnmManager::draw_std_quad_type_0`
         `th08:0x463877`/`0x463904` 在 flags 那两位为 0 时 `left = pos.x − w*scale/2`，而 `AnmVm::reset`
         `0x406926` 把低字设成 0x7 ⇒ 两位都是 0），TH18 的 `type(0)`/`type(1)` 同为**中心锚点**
         （仓内既有结论：`make_youkai_art.py` 头 ①、`make_sanae_youkai_art.py`）；而本仓构建期
         （`assets/build_ability.py`）对「源图小于声明尺寸」也是**居中补** ⇒ 两边同一条公式、可见像素一个不动。
  ability/scripts/224..227_youkai_opt_<角色>.anm.txt —— 正文逐条照抄零售原文，只把 `sprite(N)` 换成
       `sprite(@名字@)`（+ 省略 `ins_83`，理由写在脚本里；注释写在 `script scriptNNN {` **里面**）。
  ability/entries/ORDER.youkai_option.txt —— 待合并进 ORDER.txt 的片段（整段重写，幂等）。
       ⚠ **不动 `entries/ORDER.txt`**（多人并发，由主 agent 统一合并）。
  stdout —— ORDER 片段 + 自检（尺寸/非空/占位符/逐行 diff/能否编过/重名检查）+ 幂等 md5。
"""
import difflib
import hashlib
import re
import subprocess
import sys
import tempfile
from pathlib import Path

from PIL import Image

HERE = Path(__file__).resolve().parent          # assets/ability
REPO = HERE.parents[4]                          # assets/ability → card-expand → th18.v1.00a → mods → renkolab
OUT_ART = HERE / "youkai_option"
OUT_SCRIPTS = HERE / "scripts"
OUT_ORDER = HERE / "entries" / "ORDER.youkai_option.txt"
MAIN_ORDER = HERE / "entries" / "ORDER.txt"     # 只读（查重名）

THANM = REPO / "local/vendor/thtk/build/thanm/thanm"
ANMMAP_V2 = REPO / "local/vendor/thpages/static/mapfile/v2.anmm"   # TH08 anm = version 3
ANMMAP_V8 = REPO / "local/vendor/thpages/static/mapfile/v8.anmm"   # TH16 anm = version 8 / 目标 TH18
TH08_DAT = REPO / "local/th08.v1.00d/dat"
TH16_DAT = REPO / "local/th16.v1.00a/dat"

SCRIPT0 = 224          # 脚本文件编号从这里起连续（`make anm` 之前 ORDER 已有的最大脚本号 = 223：UFO_PENTAGON）

# TH18 机体号（= `CE_ADDR_CHARACTER` 0..3；`youkai_core.h` 的 `YK_OPTION_BY_CHAR` 同一顺序）
# → 零售哪个作品的哪支子机脚本。一手落点见文件头。
SOURCES = [
    # (kind, anm 名, 脚本号, 角色短名, 中文名, TH18 机体号, 说明)
    ("th08", "player00", 18, "YUKARI",  "八雲紫",     0, "TH08 pl00 = 霊夢／八雲紫 队的妖侧"),
    ("th08", "player01", 29, "ALICE",   "アリス",     1, "TH08 pl01 = 魔理沙／アリス 队的妖侧"),
    ("th08", "player02", 24, "REMILIA", "レミリア",   2, "TH08 pl02 = 咲夜／レミリア 队的妖侧"),
    ("th16", "pl03",     11, "AYA",     "射命丸文",   3, "TH16 pl03 = 文（`CHARACTER` = 2，菜单序）"),
]

TH08_VERSION = "8"          # thanm 的 VERSION 参数 = 作品号（TH08 是 8）
TH16_VERSION = "16"
TH18_VERSION = "18"

_SPRITE_DEF_RE = re.compile(r"sprite(\d+):\s*\{\s*x:\s*(\d+),\s*y:\s*(\d+),\s*w:\s*(\d+),\s*h:\s*(\d+)\s*\}")
_SPRITE_CALL_RE = re.compile(r"sprite\(sprite(\d+)\)")
_NAME_RE = re.compile(r"@([A-Za-z_][A-Za-z0-9_]*)@")
_INS83_RE = re.compile(r"^ins_83\((\d+)\);\s*$")


# ── 取源（thanm 现场解，仓里不落零售字节）──────────────────────────────

def _require(p, what):
    if not p.is_file():
        raise SystemExit(f"缺 {what}：{p}\n（TH08 的 `.dat` 用 thtk 解不开，`local/th08.v1.00d/dat/` 已解好；"
                         f"缺 thanm 就先 `bash tooling/thtk/build.sh`）")


def run_thanm(args, cwd):
    r = subprocess.run([str(THANM), *args], cwd=str(cwd), capture_output=True, text=True)
    # ⚠ v2.anmm 里有一条 `74: 'acos' is a keyword, ignoring` 的**无害警告**；只有这类 `… ignoring` 放行。
    noise = [l for l in r.stderr.splitlines() if l.strip() and "ignoring" not in l]
    if r.returncode != 0 or noise:
        raise SystemExit(f"thanm {' '.join(map(str, args))} 失败：\n{r.stderr or r.stdout}")
    return r.stdout


def read_anm(anm: Path, version: str, workdir: Path):
    """→ spec 文本（`-l`）+ 解出来的贴图根目录（`-x`，entry 名自带相对路径）。"""
    text = run_thanm(["-l", version, "-m", str(ANMMAP_V2 if version == TH08_VERSION else ANMMAP_V8),
                      str(anm)], workdir)
    run_thanm(["-x", version, str(anm)], workdir)
    return text


def parse_entries(text):
    """→ [(entry 名, {sprite 号: (x, y, w, h)}), …]，按文件里的出现顺序。矩形只从 sprites 表读，不写死。"""
    out = []
    for m in re.finditer(r"name:\s*\"([^\"]+)\",(.*?)\n\}", text, re.S):
        name, body = m.group(1), m.group(2)
        j = body.find("sprites:")
        if j < 0:
            continue
        start, depth, end = body.index("{", j), 0, None
        for k in range(start, len(body)):
            if body[k] == "{":
                depth += 1
            elif body[k] == "}":
                depth -= 1
                if depth == 0:
                    end = k
                    break
        rects = {int(g[0]): tuple(int(x) for x in g[1:])
                 for g in _SPRITE_DEF_RE.findall(body[start:end])}
        out.append((name, rects))
    if not out:
        raise SystemExit("spec 里一个 entry 都没解析出来")
    return out


def extract_script(text, n):
    m = re.search(rf"^script script{n} \{{.*?^\}}$", text, re.S | re.M)
    if not m:
        raise SystemExit(f"spec 里找不到 script{n}")
    return m.group(0)


# ── 切图 ─────────────────────────────────────────────────────────────

def pow2_ceil(v):
    p = 1
    while p < v:
        p <<= 1
    return p


def canvas_size(w, h):
    """→ (W, H)：都是 2 的幂、且 ≥ 内容（写进 ORDER 的尺寸必须是 2 的幂，构建硬门）。"""
    return pow2_ceil(w), pow2_ceil(h)


def crop_option(sheet_path, rects, sprite_ids, char):
    """该机体的子机 sprite 逐个切出来、**居中**补到 2 的幂画布。

    → {ORDER 名: (路径, 画布 WxH, sprite 号, 可见像素数, 源单元字节)}
    """
    im = Image.open(sheet_path).convert("RGBA")
    OUT_ART.mkdir(parents=True, exist_ok=True)
    made = {}
    for sid in sorted(sprite_ids):
        x, y, w, h = rects[sid]
        cw, ch = canvas_size(w, h)
        cell = im.crop((x, y, x + w, y + h))
        canvas = Image.new("RGBA", (cw, ch), (0, 0, 0, 0))
        canvas.paste(cell, ((cw - w) // 2, (ch - h) // 2))     # ★ 居中 = 零售的 VM 中心锚点（见文件头）
        name = f"YOUKAI_OPT_{char}_S{sid}"
        path = OUT_ART / f"{name}.png"
        canvas.save(path)
        vis = sum(1 for a in canvas.getchannel("A").get_flattened_data() if a > 32)
        made[name] = (path, (cw, ch), sid, vis, cell.tobytes())
    return made


# ── 脚本 ─────────────────────────────────────────────────────────────

def header_lines(char, cn, kind, anm_name, src_num, sprite_map, engine_note):
    pairs = "，".join(f"sprite{k} → @{v}@" for k, v in sorted(sprite_map.items()))
    lines = [
        f"    // 由 assets/ability/make_youkai_option_art.py 从 "
        f"{'TH08 v1.00d' if kind == 'th08' else 'TH16 v1.00a'} `{anm_name}` script{src_num} 生成，别手改。",
        f"    // 本支子机 = {cn}（{char}，TH18 机体 {SOURCES_BY_CHAR[char][5]} 那一支）；{engine_note}",
        "    // 指令逐条照抄（标签/时序/数值一字未动），只把 sprite 引用换成 ORDER 名字：",
        f"    //   {pairs}",
    ]
    if kind == "th08":
        lines += [
            "    // ★ 零售这支**没有 `layer()`**（TH08 的 anm v3 那代由 VM 自带层）⇒ 这里也不写：TH18 的",
            "    //   引擎在 `Player__allocate_option` `th18:0x40a8bc mov DWORD PTR [esi+0x18],0xe` 里把子机 VM",
            "    //   的层设成 **14**（世界层）⇒ 不写 layer 就用引擎给的 14，不需要 C 侧补、也不自己编层号。",
            "    // ★ 零售这一行是 `ins_83(2);`（TH08 v3 的**无名 opcode 83**，写 `vm+0x200`，handler",
            "    //   `th08:0x460314`）—— **TH18 的 anm v8 里 opcode 83 不存在**（`thanm -c 18` 实测报",
            "    //   \"opcode 83 is not known to exist in version 8\"）⇒ 逐条照抄做不到，**这一行省略**",
            "    //   （全 exe 只有两处读 `vm+0x200`：`th08:0x45e96c` player-bullet 专用 draw 分派、",
            "    //   `th08:0x430efa` 敌人链 —— 都不在子机 VM 的绘制路径上 ⇒ 对画面无影响，🟡 读码）。",
            "    // ★ 中断标签是**TH08 的约定**：`(1)(2)` = 两段循环、`(3)(4)` = 起爆（旋转 + `stop()`）、",
            "    //   `(5)` = 散场；与 TH18 零售本体子机脚本的 2 出场 / 3 收起 / 1 销毁**不同**（照抄的必然结果）。",
        ]
    else:
        lines += [
            "    // ★ `layer(13)` 是**零售自己写的**（TH16 那代 VM 的层由脚本给）⇒ 照抄，不改。",
            "    // ★ 中断标签与 TH18 零售本体子机脚本**同款**：`(2)` 出场、`(3)` 进店收起、`(1)` 销毁 —— 天然对得上。",
        ]
    return lines


def gen_script(src_text, src_num, new_num, char, cn, kind, anm_name, sprite_map, engine_note):
    """→ (脚本文本, 与零售原文的 diff 行数, 省略的 ins_83 行)"""
    body, dropped = [], []
    for line in src_text.splitlines():
        s = line.strip()
        if s == f"script script{src_num} {{":
            body.append(f"script script{new_num} {{")
            continue
        m = _INS83_RE.match(s)
        if m:
            indent = line[:len(line) - len(line.lstrip())]
            dropped.append(s)
            body.append(f"{indent}// ★ 零售原文此处为 `{s}` —— 见文件头：TH18 的 anm v8 无此 opcode ⇒ 省略")
            continue
        body.append(_SPRITE_CALL_RE.sub(lambda mm: f"sprite(@{sprite_map[int(mm.group(1))]}@)", line))
    head = header_lines(char, cn, kind, anm_name, src_num, sprite_map, engine_note)
    text = "\n".join(body[:1] + head + body[1:]) + "\n"
    return text, dropped


def sprite_name(char, sid):
    return f"YOUKAI_OPT_{char}_S{sid}"


# ── ORDER 片段 ───────────────────────────────────────────────────────

def write_order(rows):
    lines = [
        "# 永夜抄（系统卡 108）**低速子机（使い魔 / option）** —— 四支机体各自的零售子机贴图。",
        "# 本片段由 assets/ability/make_youkai_option_art.py 生成（幂等，整段重写）；合并时把这些行追加进 ORDER.txt。",
        "# 每行 `NAME  源图(相对 assets/)  WxH`；WxH 都是 2 的幂 ⇒ 整张贴图一个 sprite。",
        "# ★ 一手（子机用哪支脚本/哪些 sprite）——**不是共享图集、也不是机体本体图**，每支机体一套：",
        "#   TH08 `playerNN.anm`：子机 = `zPlayerOption`（`player+0x40c` × 4、stride 0x2f4，头 0xa4×4… 见生成器头）",
        "#     pl00 script18（妖侧 紫）sprite22..30 / pl01 script29（アリス）sprite22..25 / pl02 script24（レミリア）sprite20；",
        "#     装载点 = 各机体 handler 的 state==1 分支：`th08:0x44e3d7 push 0x12` / `0x44ea6f push 0x1d` / `0x44eba7 push 0x18`，",
        "#     经 `AnmLoaded::load_script_into_vm th08:0x4069f0` 装进 `option+0x0` 那个 AnmVm。",
        "#   TH16 `pl03.anm`（文）：本体子机脚本号 = `th16:0x492c00[CHARACTER]`（菜单序 {8,7,11,11}，",
        "#     `CHARACTER`=2 = 文，`th16:0x4443ad` 取值 + `AnmLoaded::copy_vm th16:0x407b20`）⇒ script11，sprite32..39。",
        "# ★ 层：TH16 那支零售自带 `layer(13)`；TH08 那三支零售无 layer —— TH18 引擎在 `th18:0x40a8bc` 把子机 VM",
        "#   的层设成 14 ⇒ 不写 layer 即用 14（世界层），脚本里不编层号。",
    ]
    # ⚠ 数据行**严格 3 列**（`anmlib.read_order` 按空白切、只取 [0][1][2]；仓里既有片段全是 3 列）
    #   ⇒ 出处写在行间的 `#` 注释里（`read_order` 跳过 `#` 开头的行）。
    last = None
    for name, path, size, sid, char, src in rows:
        if char != last:
            lines.append(f"# —— {char}（{SOURCES_BY_CHAR[char][4]}）：零售 {src} = sprite"
                         f"{[r[3] for r in rows if r[4] == char]}（画布 "
                         f"{sorted({r[2] for r in rows if r[4] == char})}）——")
            last = char
        lines.append(f"{name}  {path}  {size}")
    OUT_ORDER.write_text("\n".join(lines) + "\n", encoding="utf-8")
    return lines


# ── 自检 ─────────────────────────────────────────────────────────────

def check_pngs(all_made):
    print("\n# —— ① 贴图：尺寸是 2 的幂、且不全是透明 ——")
    total = 0
    for char, made in all_made.items():
        for name, (path, (cw, ch), sid, vis, _cell) in sorted(made.items()):
            assert cw & (cw - 1) == 0 and ch & (ch - 1) == 0, f"① {name} 画布 {cw}x{ch} 不是 2 的幂"
            assert vis > 0, f"① {name} 全透明"
            assert path.stat().st_size > 0
            total += 1
        vis_all = sorted(v[3] for v in made.values())
        print(f"  {char}: {len(made)} 张，画布 {sorted({v[1] for v in made.values()})}，"
              f"可见像素 {vis_all[0]}..{vis_all[-1]}")
    print(f"① {total} 张全部：2 的幂画布 + 非空 ✓")


def check_names(all_made):
    """ORDER 名字不许与主表已有重名（幂等：主表里若已有**我们自己的**那一行，按同路径放行）。"""
    print("\n# —— ② 重名检查（只读 entries/ORDER.txt）——")
    mine = {}
    for made in all_made.values():
        for name, (path, size, _sid, _vis, _cell) in made.items():
            mine[name] = (path, size)
    if not MAIN_ORDER.is_file():
        print("  ⚠ 找不到 entries/ORDER.txt（主表），跳过重名检查")
        return
    clash = []
    for line in MAIN_ORDER.read_text(encoding="utf-8").splitlines():
        s = line.strip()
        if not s or s.startswith("#"):
            continue
        parts = s.split()
        if len(parts) < 2:
            continue
        n, p = parts[0], parts[1]
        if n in mine and p != mine[n][0]:
            clash.append((n, p, mine[n][0]))
    assert not clash, f"② 与主表重名/重路径不一致：{clash}"
    already = sum(1 for line in MAIN_ORDER.read_text(encoding="utf-8").splitlines()
                  if line.split() and line.split()[0] in mine)
    print(f"② 本片段的 {len(mine)} 个名字与主表 {already} 行核对：无冲突 ✓"
          f"（主表里已出现的 {already} 行路径与本片段逐字一致）")


def check_scripts(scripts, rows):
    print("\n# —— ③ 脚本里每个 @名字@ 都在 ORDER 片段里有定义 ——")
    names = {r[0] for r in rows}
    n_tok = 0
    for fname, (text, _dropped, _src, _num, _char) in sorted(scripts.items()):
        used = set(_NAME_RE.findall(text))
        missing = used - names
        assert not missing, f"③ {fname}: {sorted(missing)} 不在 ORDER 片段里"
        n_tok += len(used)
        print(f"  {fname}: {len(used)} 个 @名字@ 全部有定义（{', '.join(sorted(used))}）")
    print(f"③ 4/4 条脚本、共 {n_tok} 处引用全部落在 ORDER 片段里 ✓")


def print_script_diffs(scripts):
    print("\n# —— ④ diff 证据：零售原文（-）vs 生成脚本（+）—— 差异只有 sprite 引用 + 省略的 ins_83 ——")
    for fname, (text, dropped, src, num, _char) in sorted(scripts.items(), key=lambda kv: kv[1][3]):
        retail_num = int(re.search(r"^script script(\d+) \{", src, re.M).group(1))
        print(f"\n=== {fname}（新号 script{num} ← 零售 script{retail_num}；正文 {len(text.splitlines())} 行；"
              f"省略 {len(dropped)} 行：{dropped or '—'}）")
        for line in difflib.unified_diff(src.splitlines(), text.splitlines(),
                                         "retail-original", "generated", lineterm="", n=0):
            print(line)


_TEST_ENTRY = """entry entry0 {
    version: 8,
    name: "ability/TEST.png",
    format: 1,
    width: 64,
    height: 64,
    memoryPriority: 0,
    lowResScale: 0,
    hasData: 1,
    THTXSize: 16384,
    THTXFormat: 1,
    THTXWidth: 64,
    THTXHeight: 64,
    THTXZero: 0,
    sprites: {
        sprite0: { x: 0, y: 0, w: 64, h: 64 }
    }
}
"""


def check_compile(scripts):
    """⑤ 每条脚本：`@NAME@` 临时换 `sprite0`、脚本号改 0，在 /tmp 里用 thanm -c 18 单编一次。"""
    print("\n# —— ⑤ 能否编过（thanm -c 18，/tmp 里单独编；不碰 native/build/、不跑 make）——")
    with tempfile.TemporaryDirectory(prefix="youkai-opt-anm-") as td:
        d = Path(td)
        (d / "ability").mkdir()
        Image.new("RGBA", (64, 64), (255, 0, 255, 255)).save(d / "ability" / "TEST.png")
        ok = 0
        for fname, (text, _dropped, _src, num, _char) in sorted(scripts.items(), key=lambda kv: kv[1][3]):
            test = _NAME_RE.sub("sprite0", text).replace(f"script script{num} {{", "script script0 {", 1)
            (d / "spec.txt").write_text(_TEST_ENTRY + "\n" + test + "\n", encoding="utf-8")
            out = d / "out.anm"
            if out.exists():
                out.unlink()
            run_thanm(["-c", TH18_VERSION, str(out), "spec.txt", "-m", str(ANMMAP_V8)], d)
            assert out.is_file() and out.stat().st_size > 0, f"⑤ {fname} 编出来是空的"
            ok += 1
            print(f"  {fname} → spec script0（@名字临时代 sprite0）：thanm -c 18 OK（{out.stat().st_size} B）")
        print(f"⑤ {ok}/{len(scripts)} 条脚本都能编过（临时目录已删，native/build 一行没动） ✓")


def print_md5(paths):
    h = hashlib.md5()
    for p in sorted(paths):
        h.update(p.name.encode())
        h.update(p.read_bytes())
    print(f"\n[md5] 本次全部产物（{len(paths)} 个文件的「名字+内容」汇总）= {h.hexdigest()}"
          f"   ← 连跑两次应完全一致（幂等判据）")


SOURCES_BY_CHAR = {}


# ── main ─────────────────────────────────────────────────────────────

def main():
    _require(THANM, "thanm 可执行文件")
    _require(ANMMAP_V2, "TH08 用的 anmmap v2.anmm")
    _require(ANMMAP_V8, "TH16 / TH18 用的 anmmap v8.anmm")
    for kind, name, _n, _c, _cn, _ci, _note in SOURCES:
        _require((TH08_DAT if kind == "th08" else TH16_DAT) / f"{name}.anm", f"{kind} 零售 {name}.anm")
    for i, s in enumerate(SOURCES):
        SOURCES_BY_CHAR[s[3]] = s

    all_made, rows, scripts = {}, [], {}
    with tempfile.TemporaryDirectory(prefix="youkai-opt-src-") as td:
        tmp = Path(td)
        for k, (kind, anm_name, src_num, char, cn, char_idx, engine_note) in enumerate(SOURCES):
            anm = (TH08_DAT if kind == "th08" else TH16_DAT) / f"{anm_name}.anm"
            version = TH08_VERSION if kind == "th08" else TH16_VERSION
            work = tmp / f"{kind}-{anm_name}"
            work.mkdir(parents=True)
            text = read_anm(anm, version, work)
            src_text = extract_script(text, src_num)
            ids = sorted({int(x) for x in _SPRITE_CALL_RE.findall(src_text)})
            assert ids, f"{anm_name} script{src_num} 一个 sprite 都没引用"
            # sprite 号是**文件全局**的：找出「含全部这些 sprite 的那个 entry」，不写死 entry 名
            owner = [(nm, rx) for nm, rx in parse_entries(text) if all(i in rx for i in ids)]
            if len(owner) != 1:
                raise SystemExit(f"{anm_name} script{src_num} 的 sprite {ids} 落在 {len(owner)} 个 entry 里 ⇒ 要复核")
            entry_name, rects = owner[0]
            sheet = work / entry_name
            if not sheet.is_file():
                raise SystemExit(f"{anm_name}: thanm -x 没解出 {entry_name}（entry 名变了？）")
            made = crop_option(sheet, rects, ids, char)
            all_made[char] = made
            smap = {sid: sprite_name(char, sid) for sid in ids}
            new_num = SCRIPT0 + k
            script, dropped = gen_script(src_text, src_num, new_num, char, cn, kind, anm_name, smap, engine_note)
            fname = f"{new_num}_youkai_opt_{char.lower()}.anm.txt"
            # 幂等前提：这个号只能是我们自己的文件（别的生成器占了就报错，不覆盖）
            for other in OUT_SCRIPTS.glob(f"{new_num}_*.anm.txt"):
                assert other.name == fname, f"⑤ 脚本号 {new_num} 已被 {other.name} 占用 ⇒ 换号"
            (OUT_SCRIPTS / fname).write_text(script, encoding="utf-8")
            scripts[fname] = (script, dropped, src_text, new_num, char)
            for sid in ids:
                nm, (path, size, _s, _v, _c) = smap[sid], made[smap[sid]]
                rows.append((nm, f"ability/youkai_option/{nm}.png", f"{size[0]}x{size[1]}", sid, char,
                             f"{kind} {anm_name}.anm script{src_num}"))
            im = Image.open(sheet)
            print(f"[src] {kind} {anm_name}.anm script{src_num}（{cn} {char}）：entry {entry_name}"
                  f"（贴图 {im.width}×{im.height}）→ sprite {ids}")
            print(f"[crop] {char}：{len(made)} 张 → ability/youkai_option/（画布 "
                  f"{sorted({v[1] for v in made.values()})}）")

    order_lines = write_order(rows)
    print("\n# —— ORDER.youkai_option.txt 内容（合并时把这些行追加进 ORDER.txt；本脚本没碰 ORDER.txt）——")
    for line in order_lines:
        print(line)

    check_pngs(all_made)
    check_names(all_made)
    check_scripts(scripts, rows)
    print_script_diffs(scripts)
    check_compile(scripts)

    outs = [v[0] for made in all_made.values() for v in made.values()]
    outs += [OUT_SCRIPTS / f for f in scripts]
    outs.append(OUT_ORDER)
    print_md5(outs)
    print(f"\n[done] 4 支机体（"
          + "、".join(f"{s[4]}→{s[3]} script{s[2]}（{len(all_made[s[3]])} 张）" for s in SOURCES)
          + f"）；脚本号 {SCRIPT0}..{SCRIPT0 + len(SOURCES) - 1}；`make anm` 之后 anm_ids.h 会给出"
            f" `CE_ANM_ABILITY_SCRIPT_YOUKAI_OPT_*` 宏（本次 C 侧先用字面量）。")


if __name__ == "__main__":
    main()
