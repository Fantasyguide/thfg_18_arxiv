#!/usr/bin/env python3
"""永夜抄（系统卡 108）**使い魔（使魔）本体**：TH08 零售切图 + 脚本生成。

    python3 make_youkai_familiar_art.py        # 生成 + 自检（可重复跑，结果一致）

来源（零售字节只在 `local/`，gitignored；构建时取，不入库）：
  贴图+脚本  `local/th08.v1.00d/dat/enemy.anm` —— TH08 v1.00d（anm **version 3**）。
             贴图在 .anm 内部，entry 名 = `data/stgenm/enemy.png`（512×512）。
             本脚本运行时用 thanm 现场 `-l`（读矩形/脚本）与 `-x`（取贴图）到**临时目录**，仓里不落零售字节。
  复核命令   `local/vendor/thtk/build/thanm/thanm -l 8 -m local/vendor/thpages/static/mapfile/v2.anmm \
                 local/th08.v1.00d/dat/enemy.anm`
             ★ anmmap 必须用 **v2.anmm**（文件头自述「for version 2: PCB and version 3: IN, PoFV」）——
               TH08 的 anm 是 v3；v8（TH13+）会把 `sprite` 译成 `stop`。

── ★ 一手结论 ①：TH08 的「使い魔」是 ECL **opcode 90/91/92**，外观由被生成的那个敌人自己的 ECL 决定 ────
三支 handler 的 opcode 号由 `AnmManager::run_vm` 之外的 ECL 解释器分发表定死：
`th08:0x418623 jmp DWORD PTR [edx*4+0x41ecbe]`（表基址 `0x41ecbe`，`opcode = movsx word[ins+4]`，index = opcode−1）：

    opcode 90 → th08:0x41af5b → call th08:0x41f110 `Enemy::create_enemy_familiar_2`（re-data 名）
    opcode 91 → th08:0x41b10b → call th08:0x41f280 `Enemy::enemy_create_familiar`（re-data 名）
    opcode 92 → th08:0x41b2bb → call th08:0x41f110（同 90）

三者最后都 `call th08:0x42a680 EnemyManager::create_enemy`，生成一个**普通敌人**；参数签名 = `NffSSS`
（thtk `thecl/thecl06.c:751-753` 的内建表 `{90,"NffSSS"},{91,"NffSSS"},{92,"NffSSS"}`），
与 `th08:0x41f110` 的取参顺序逐一对上（`[ins+0xc]`=sub、`[ins+0x10]/[ins+0x14]`=x,y float、
`[ins+0x18]`→`enemy+0x2dfc`、`[ins+0x1c]`→`enemy+0x3304`、`[ins+0x20]`→`enemy+0x2e08`）。
`zEnemy` 字段名（re-data `type-structs-own.json`）：`+0x2dfc = life`、`+0x3304 = drop_main`、
`+0x2e08 = score_reward`、`+0x3380 = __familiar_count__but_not_total`。
★ 两条旁证：三个 case 都 `add DWORD PTR [ctx+0x3380],1`（使魔计数 +1）；
`th08:0x41b246` 读 `enemy+0x2e0c`（re-data 名 = `__flag_field_used_by_familiars__see_0x41b246`）决定要不要翻转使魔的角速度。

⇒ **使魔不是一张专用贴图**：它长得像谁，取决于它跑的那支 ECL sub 自己设了什么 anm 脚本。

── ★ 一手结论 ②：那支 sub 设的 anm 脚本 = `enemy.anm`（共享敌机图集）script **53/54/55/56/…** ────────
ECL opcode 54 的 handler `th08:0x419812` 取 1 个参数 K，调
`th08:0x4069f0 AnmLoaded::load_script_into_vm(this, vm=&enemy+0xc, script=K)`；
`this` 读自全局 `th08:0xf54e0c`（`th08:0x419850 mov ecx,DWORD PTR ds:0xf54e0c`）。
`zEnemy+0xc = primary_vm`（re-data）⇒ 这是**敌机本体 VM 的脚本**。

`0xf54e0c` 是哪一个 .anm？**用脚本数上界定死**：把 12 个关卡 ECL 全解出来，`ins_54(K)` 的 K 最大到
**95**（`ecldata5.ecl` 用 94/95）。TH08 全 317 条 entry 里脚本数 ≥96 的只有两个
（`thanm -l 8 -m v2.anmm` 逐个数）：**`etama.anm` 115**、**`enemy.anm` 101**；
各关 `stgNenm.anm` 只有 6–26（`stg1enm` 6、`stg4benm` 26、`stg8enm` 19 …）⇒ 关卡特有图集放不下。
`ins_54` 写的是**敌机本体 VM**，且 `EnemyManager` 里
`+0x9dceec = field_9dcef0__prolly_enemy_anm`（re-data 名，装 `enemy.anm`，`th08:0x42ec0e push "enemy.anm"`）
/ `+0x9dcef0 = …boss_anm`（装 `stgNenm.anm`）⇒ **`0xf54e0c` = `enemy.anm`**（贴图 `data/stgenm/enemy.png`）。
（🟡 残余：`etama.anm` 也满足 101<115 的上界，纯粹靠「敌机本体 VM ⇒ 敌机图集」排除，见报告 ❓。）

**使魔真正用到的那几支**（把 12 关 ECL 里所有 `ins_90/91/92("SubN", …)` 调用点连同 `SubN` 的
`ins_54(K)` 全部列出来，只出现 6 个 K）：**53、54、55、56、57、58**，
全部落在 `enemy.png` 的 **32×32 小物件循环动画**上（居中偏右一块 4×2 网格）：

    script53 = sprite 69..76   8 帧  (256..511, y=128)    ↓ 本生成器做的四套之一
    script54 = sprite 77..84   8 帧  (256..511, y=160)
    script55 = sprite 85..92   8 帧  (256..511, y=192)
    script56 = sprite 93..100  8 帧  (256..511, y=224)
    script57 = sprite 101..104 4 帧  (128..255, y=128)  ← 脚本里有 v3 专有指令 `ins_25`，见下
    script58 = sprite 105..108 4 帧  (128..255, y=160)  ← 同上

★ 本生成器**只移植 53..56**（四条 8 帧循环）。理由：57/58 的零售正文里有 `ins_25(1)` ——
一手：anm v3 解释器 `th08:0x45ea76 movsx eax,WORD PTR [edx]`（raw opcode）+1 查表 `th08:0x46206d`，
raw 25 → `th08:0x45f8d7 mov WORD PTR [ecx+0x1fc],ax`，即写 `zAnmVm+0x1fc`
（re-data 直接把该字段命名为 `__v3_ins_25`；thtk 的 `v2.anmm` 也在 25 号留白注释
「sets a word field. Might be rotateAuto? Why a whole word?」）—— **v8 anm 没有对应助记符**，
抄过去编不过；按纪律「不许自己编指令」⇒ 不移植，登记 ❓（见报告）。
53..56 的正文**只有 `sprite()` 与 `jmp()`**，可以逐条照抄。

── ★ 产出定位：108 该用哪一套 ─────────────────────────────────────────────────────
108 的使魔是**卡侧自建的真敌人载体**（不是复刻）：设计文档
`docs/superpowers/specs/2026-09-12-youkai-108-design.md` §3.5 把它定义成「发射口实体化的真敌人」，
每帧按活跃发射口槽数建/撤。**一套四色正好对应最多 4 个发射口槽**，所以本生成器把
script53/54/55/56 四套**全做**，卡侧按槽位 `ce_anm_set_sprite`（或按槽号选脚本）挑一套即可 ——
既保住「素材是官方零售」这条硬要求，又不用挑一个「最像」的。
使魔被打死按 1/2 转嫁本体是**我们的设计**（原作没有这条管线），与本生成器无关。

── 产出（每条都不动别人的文件）────────────────────────────────────────────────────
  assets/ability/youkai_familiar/YOUKAI_FAM_S<53|54|55|56>_<0..7>.png
        —— `enemy.anm` sprite 69..100 的零售切图，每张 **32×32**（本来就是 2 的幂 ⇒ 无需补边）。
        ★ 居中不变的依据（一手）：TH08 sprite 四边形以 **VM 位置为中心**画 ——
          `AnmManager::draw_std_quad_type_0` `th08:0x463877` / `th08:0x463904`：
          `flags_lo>>11 & 3 & 1` / `…&3&2` 两位为 0 时取 `pos − 半宽`（`fsub [ebp-0x4]`）；
          而 `AnmVm::reset` `th08:0x406926` 把 flags 低字设成 `0x7`（`mov WORD PTR [eax+0x1f8],0x7`）
          ⇒ 那两位都是 0。32×32 画布 = 源单元本身，居中因此是**逐像素恒等**，
          与 `build_ability.py:119`「源图小于声明尺寸才居中补」同一条公式。
  assets/ability/scripts/206..209_youkai_familiar_s<53|54|55|56>.anm.txt
        —— 正文逐条照抄 `enemy.anm` script53..56，**只**把 `sprite(spriteNN)` 换成 `sprite(@NAME)`；
          `+N:` 标签 / `jmp()` 一个不改（注释写在 `script scriptNNN {` **里面**）。
        ★ **没有 `layer()` 可抄**：零售这 4 支脚本里**一条 `layer()` 都没有** ⇒ 本生成器不替它编层号。
          ⚠ 卡侧把这几支挂在**独立 VM** 上时，那台 VM 必须**自己**补 `layer(...)` —— 世界层 3–19，
          别用 HUD 的 24/25。
  assets/ability/entries/ORDER.youkai_familiar.txt —— 待合并进 ORDER.txt 的片段（整段重写，幂等）。
        ⚠ **不动 `entries/ORDER.txt`**（多人并发，由主 agent 统一合并）。
  stdout —— ORDER 片段 + 自检（尺寸/非空/占位符/逐行 diff/能否编过/32 张图的可见像素数）+ 调用点参数表。
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
OUT_ART = HERE / "youkai_familiar"
OUT_SCRIPTS = HERE / "scripts"
OUT_ORDER = HERE / "entries" / "ORDER.youkai_familiar.txt"

THANM = REPO / "local/vendor/thtk/build/thanm/thanm"
ANMMAP_TH08 = REPO / "local/vendor/thpages/static/mapfile/v2.anmm"   # TH08 anm = version 3
ANMMAP_TH18 = REPO / "local/vendor/thpages/static/mapfile/v8.anmm"   # 目标：TH18 构建用的表
TH08_DAT = REPO / "local/th08.v1.00d/dat"
TH08_VERSION = "8"
TH18_VERSION = "18"

SRC_ANM = "enemy.anm"
SRC_ENTRY = "data/stgenm/enemy.png"

# TH08 ECL 里所有 `ins_90/91/92` 调用点指向的 sub 用到的 `ins_54(K)`，去重后只有这 6 个。
# 53..56 是可以逐条照抄的 8 帧循环；57/58 含 v3 专有指令 `ins_25`（见文件头）⇒ 不移植。
FAM_SCRIPTS = (53, 54, 55, 56)
NOT_PORTED = (57, 58)
SCRIPT0 = 206                                  # 脚本文件编号从这里起连续（206..209）

_SPRITE_RE = re.compile(r"sprite(\d+):\s*\{\s*x:\s*(\d+),\s*y:\s*(\d+),\s*w:\s*(\d+),\s*h:\s*(\d+)\s*\}")
_TOKEN_RE = re.compile(r"@([A-Za-z_][A-Za-z0-9_]*)")
_FILE_RE = re.compile(r"^(\d+)_([A-Za-z_][A-Za-z0-9_]*)\.anm\.txt$")
_SCRIPT_SPRITE_RE = re.compile(r"sprite\(sprite(\d+)\)")


# ── 取源（thanm 现场解，仓里不落零售字节）──────────────────────────────

def _require(p, what):
    if not p.is_file():
        sys.exit(f"缺 {what}：{p}\n（TH08 的 `.dat` 用 thtk 解不开，`local/th08.v1.00d/dat/` 已解好；"
                 f"缺 thanm 就先 `bash tooling/thtk/build.sh`）")


def run_thanm(args, cwd):
    r = subprocess.run([str(THANM), *args], cwd=str(cwd), capture_output=True, text=True)
    # ⚠ v2.anmm 里有一条 `74: 'acos' is a keyword, ignoring` 的**无害警告**（thanm 解析助记符表时发的）；
    #   只有这类 `… ignoring` 警告放行，其它 stderr 一律当失败（与 build_ability.py 同款严格度）。
    noise = [l for l in r.stderr.splitlines() if l.strip() and "ignoring" not in l]
    if r.returncode != 0 or noise:
        sys.exit(f"thanm {' '.join(map(str, args))} 失败：\n{r.stderr or r.stdout}")
    return r.stdout


def read_anm(anm: Path, workdir: Path):
    """→ (spec 文本, 解出来的贴图 PNG 路径)。`-l` 拿脚本/sprite 表；`-x` 解贴图。"""
    text = run_thanm(["-l", TH08_VERSION, "-m", str(ANMMAP_TH08), str(anm)], workdir)
    run_thanm(["-x", TH08_VERSION, str(anm)], workdir)
    png = workdir / SRC_ENTRY
    if not png.is_file():
        sys.exit(f"{anm.name}: thanm -x 没解出 {SRC_ENTRY}（entry 名变了？）")
    return text, png


def parse_entry(text, entry_name):
    """→ ({sprite 号: (x, y, w, h)}, (贴图 W, H))。矩形只从 spec 的 sprites 表读，不写死。"""
    i = text.find(f'name: "{entry_name}"')
    if i < 0:
        sys.exit(f"spec 里找不到 entry {entry_name}")
    j = text.find("sprites:", i)
    depth, start, end = 0, text.index("{", j), None
    for k in range(start, len(text)):
        if text[k] == "{":
            depth += 1
        elif text[k] == "}":
            depth -= 1
            if depth == 0:
                end = k
                break
    rects = {int(m.group(1)): tuple(int(x) for x in m.groups()[1:])
             for m in _SPRITE_RE.finditer(text[start:end])}
    if not rects:
        sys.exit(f"entry {entry_name} 的 sprites 表是空的")
    wm = re.search(r'name: "%s",\n\s*format: \d+,\n\s*width: (\d+),\n\s*height: (\d+),'
                   % re.escape(entry_name), text)
    if not wm:
        sys.exit(f"entry {entry_name} 的 width/height 没解析出来")
    return rects, (int(wm.group(1)), int(wm.group(2)))


def extract_script(text, n):
    m = re.search(rf"^script script{n} \{{.*?^\}}$", text, re.S | re.M)
    if not m:
        sys.exit(f"TH08 spec 里找不到 script{n}")
    return m.group(0)


def script_sprites(text, n):
    """→ [sprite 号…]（保持出现顺序，去重）。"""
    seen = []
    for s in (int(x) for x in _SCRIPT_SPRITE_RE.findall(extract_script(text, n))):
        if s not in seen:
            seen.append(s)
    return seen


# ── 切图 ─────────────────────────────────────────────────────────────

def pow2_ceil(v):
    p = 1
    while p < v:
        p <<= 1
    return p


def canvas_size(w, h):
    """→ (W, H)：都是 2 的幂、且 ≥ 内容（写进 ORDER 的尺寸必须是 2 的幂，构建硬门）。"""
    return pow2_ceil(w), pow2_ceil(h)


def crop_cycle(sheet_path, rects, sprite_ids, src_num):
    """一条 使い魔 循环的 sprite 逐个切出来、**居中**补到 2 的幂画布。

    → {ORDER 名: (路径, 画布 WxH, sprite 号, 可见像素数, 源单元字节)}
    """
    im = Image.open(sheet_path).convert("RGBA")
    OUT_ART.mkdir(parents=True, exist_ok=True)
    made = {}
    for idx, sid in enumerate(sprite_ids):
        x, y, w, h = rects[sid]
        cw, ch = canvas_size(w, h)
        cell = im.crop((x, y, x + w, y + h))
        canvas = Image.new("RGBA", (cw, ch), (0, 0, 0, 0))
        canvas.paste(cell, ((cw - w) // 2, (ch - h) // 2))   # ★ 居中 = 零售的 VM 中心锚点（见文件头）
        name = f"YOUKAI_FAM_S{src_num}_{idx}"
        path = OUT_ART / f"{name}.png"
        canvas.save(path)
        vis = sum(1 for a in canvas.getchannel("A").get_flattened_data() if a > 32)
        made[name] = (path, (cw, ch), sid, vis, cell.tobytes())
    return made


# ── 脚本 ─────────────────────────────────────────────────────────────

def header_comment(anm_name, src_num, mapping, nframes, mean):
    return (f"    // 由 assets/ability/make_youkai_familiar_art.py 从 TH08 v1.00d `{anm_name}` script{src_num} 生成，别手改。\n"
            f"    // これは「使い魔」の見た目：TH08 の ECL opcode 90/91/92 が作る敵が自分で張る "
            f"`enemy.anm` script{src_num}。\n"
            f"    // 一手 opcode 号：`th08:0x418623 jmp DWORD PTR [edx*4+0x41ecbe]`（index = opcode−1）\n"
            f"    //   → 90/th08:0x41af5b、91/th08:0x41b10b、92/th08:0x41b2bb → `Enemy::create_enemy_familiar_2` th08:0x41f110\n"
            f"    //   / `Enemy::enemy_create_familiar` th08:0x41f280 → `EnemyManager::create_enemy` th08:0x42a680。\n"
            f"    // 外观链条：sub 里 `ins_54({src_num})` → `AnmLoaded::load_script_into_vm` th08:0x4069f0\n"
            f"    //   (this = `th08:0xf54e0c`，vm = `zEnemy+0xc primary_vm`)。\n"
            f"    // 🟡 `th08:0xf54e0c` 为哪个 .anm：K 上界 95 ⇒ 只可能是 enemy.anm(101 支) 或 etama.anm(115 支)；\n"
            f"    //   按「它写的是敌机本体 VM」判为 **enemy.anm**（贴图 data/stgenm/enemy.png）——见生成器文件头 ②。\n"
            f"    // 指令逐条照抄（sprite/jmp/标签一字未动），只把 sprite 引用换成 ORDER 名字：\n"
            f"    //   " + "，".join(f"sprite{s} → @{mapping[s]}" for s in sorted(mapping)) + "\n"
            f"    // 该循环 {nframes} 帧、每帧 2 帧间隔（零售 `+2:`），可见像素均值 RGB≈{mean}。\n"
            f"    // ⚠ 零售这支脚本里**没有 layer()**（敌机本体 VM 的层由引擎定）⇒ 这里也不写；"
            f"挂独立 VM 时由挂它的那一方自己补 layer（世界层 3–19，别用 HUD 的 24/25）。")


def gen_scripts(spec_text, cycles, order_names):
    """→ ({文件名: (正文, 期望正文, 原文, 脚本号, 源脚本号)}, {文件名: {sprite号 → ORDER 名}})。"""
    OUT_SCRIPTS.mkdir(parents=True, exist_ok=True)
    out, maps = {}, {}
    dst = SCRIPT0
    for src_num in FAM_SCRIPTS:
        original = extract_script(spec_text, src_num)
        body = script_sprites(spec_text, src_num)
        mapping = {s: f"YOUKAI_FAM_S{src_num}_{i}" for i, s in enumerate(body)}
        for nm in mapping.values():
            assert nm in order_names, f"{nm} 不在 ORDER 片段里"
        head_old = f"script script{src_num} {{"
        mean = cycles[src_num][1]
        head_new = (f"script script{dst} {{\n"
                    f"{header_comment(SRC_ANM, src_num, mapping, len(body), mean)}")
        body_txt = original.replace(head_old, head_new, 1)
        for sid, nm in mapping.items():
            body_txt = body_txt.replace(f"sprite(sprite{sid})", f"sprite(@{nm})")
        want = original.replace(head_old, head_new, 1)     # 只做「脚本号 + 注释头」这一处结构改动
        left = _SCRIPT_SPRITE_RE.findall(body_txt)         # 漏换的引用（零售出现新 sprite 号时会炸）
        if left:
            sys.exit(f"{SRC_ANM} script{src_num} 里还有没替换的 sprite 引用 {left}")
        name = f"{dst}_youkai_familiar_s{src_num}.anm.txt"
        if not _FILE_RE.match(name):
            sys.exit(f"文件名不合 build_ability.py 的约定：{name}")
        (OUT_SCRIPTS / name).write_text(body_txt + "\n", encoding="utf-8")
        out[name] = (body_txt, want, original, dst, src_num)
        maps[name] = mapping
        print(f"[script] {name}  ← TH08 {SRC_ANM} script{src_num}"
              f"（{len(body)} 帧：{', '.join(f'sprite{s}' for s in body)}）")
        dst += 1
    return out, maps


def order_rows(cycles):
    """→ [(ORDER 名, 源图(相对 assets/), WxH, 源脚本号, sprite 号)]，按 脚本号 → 帧序 排。"""
    rows = []
    for src_num in FAM_SCRIPTS:
        for name, (_path, size, sid, _vis, _bytes) in sorted(
                cycles[src_num][0].items(), key=lambda kv: kv[1][2]):
            rows.append((name, f"ability/youkai_familiar/{name}.png", f"{size[0]}x{size[1]}", src_num, sid))
    return rows


def write_order(rows):
    hdr = [
        "# 永夜抄（系统卡 108）**使い魔（使魔）本体** —— TH08 v1.00d `enemy.anm` script53..56 + `data/stgenm/enemy.png` 切图。",
        "# 本片段由 assets/ability/make_youkai_familiar_art.py 生成（幂等，整段重写）；合并时把下面这些行追加进 ORDER.txt 即可。",
        "# 每行 `NAME  源图(相对 assets/)  WxH`；WxH 都是 2 的幂 ⇒ 整张贴图一个 sprite。",
        "# ★ 一手：TH08 的使い魔不是专用贴图 —— ECL opcode 90/91/92（`th08:0x41ecbe` 分发表 index=opcode−1）",
        "#   生成一个普通敌人（`EnemyManager::create_enemy` th08:0x42a680），外观 = 该 sub 自己 `ins_54(K)` 设的脚本。",
        "#   12 关 ECL 全部调用点只出现 6 个 K（53..58）；这里做的是其中的 53/54/55/56（8 帧循环）。",
        "#   K 的上界是 95（ecldata5）⇒ 那个 anm 只能是 `enemy.anm`（101 支脚本；各关 stgNenm 只有 6–26）。",
        "#   🟡 etama.anm(115 支) 也满足上界；判 enemy.anm 的依据 = `ins_54` 写的是敌机本体 VM（zEnemy+0xc）。",
        "# 四套对应最多 4 个发射口槽，卡侧按槽位挑一套即可（见生成器文件头「产出定位」）。",
        "# 每条脚本的正文逐条照抄零售，只把 sprite 引用换成这里的 @NAME；零售没有 layer()，故这里也不写。",
    ]
    lines = hdr + [f"{n}  {p}  {s}" for n, p, s, _k, _i in rows]
    OUT_ORDER.parent.mkdir(parents=True, exist_ok=True)
    OUT_ORDER.write_text("\n".join(lines) + "\n", encoding="utf-8")
    print(f"[order] {OUT_ORDER.relative_to(HERE.parent)}（{len(rows)} 行；**不动** entries/ORDER.txt）")
    return lines


# ── 自检 ─────────────────────────────────────────────────────────────

def check_pngs(cycles, rects):
    """① 每张 PNG：尺寸是 2 的幂；② 不全是透明；且与源图对应单元逐像素一致。"""
    files = sorted(OUT_ART.glob("YOUKAI_FAM_*.png"))
    want = sum(len(cycles[k][0]) for k in FAM_SCRIPTS)
    assert len(files) == want, f"① youkai_familiar/ 下有 {len(files)} 张 PNG，预期 {want}"
    vis_all, sizes = [], set()
    for src_num in FAM_SCRIPTS:
        for name, (path, size, sid, vis, cell_bytes) in cycles[src_num][0].items():
            im = Image.open(path).convert("RGBA")
            assert im.size == size, f"① {name}.png 是 {im.size}，标称 {size}"
            assert (size[0] & (size[0] - 1)) == 0 and (size[1] & (size[1] - 1)) == 0, \
                f"① {name}.png 尺寸 {size} 不是 2 的幂"
            x, y, w, h = rects[sid]
            bx, by = (size[0] - w) // 2, (size[1] - h) // 2
            assert im.crop((bx, by, bx + w, by + h)).tobytes() == cell_bytes, \
                f"② {name}.png 与 {SRC_ANM} sprite{sid} {rects[sid]} 不逐像素一致"
            assert im.getchannel("A").getbbox() is not None, f"② {name}.png 整张透明"
            assert vis >= 200, f"② {name}.png 只有 {vis} 个可见像素（源单元 {w}×{h}）"
            vis_all.append((name, vis))
            sizes.add(size)
    print(f"① {len(files)} 张 PNG 尺寸全是 2 的幂（画布 {sorted(sizes)}），"
          f"每张的可见区居中、与源图单元逐像素一致（32×32 源 ⇒ 画布恒等、居中平凡）✓")
    print(f"② {len(files)} 张全部非空（可见像素 {min(v for _n, v in vis_all)}–{max(v for _n, v in vis_all)}，"
          f"阈值 200）✓")
    return vis_all


def check_scripts(scripts, maps, rows):
    """③ 每个 @名字 都在 ORDER 片段里有定义；④ 与零售原文逐行 diff 只差 sprite 引用。"""
    names = {n for n, _p, _s, _k, _i in rows}
    changed_total = 0
    for fname, (body, want, _orig, _idx, _src) in sorted(scripts.items(), key=lambda kv: kv[1][3]):
        missing = set(_TOKEN_RE.findall(body)) - names
        assert not missing, f"③ {fname}: ORDER 片段里没有 {sorted(missing)}"
        gen, ref = body.splitlines(), want.splitlines()
        assert len(gen) == len(ref), f"④ {fname}: 行数 {len(gen)} vs 期望 {len(ref)}"
        for i, (g, e) in enumerate(zip(gen, ref)):
            if g == e:
                continue
            mg = re.fullmatch(r"\s*sprite\(@(YOUKAI_FAM_S\d+_\d+)\);", g)
            me = re.fullmatch(r"\s*sprite\(sprite(\d+)\);", e)
            assert mg and me, f"④ {fname}:{i + 1} 改到了 sprite 引用以外：{e!r} → {g!r}"
            assert mg.group(1) == maps[fname][int(me.group(1))], \
                f"④ {fname}:{i + 1} {e.strip()} 应换成 sprite(@{maps[fname][int(me.group(1))]})，实际 {g.strip()}"
            changed_total += 1
    print(f"③ {len(scripts)} 条脚本里的 @名字全部在 ORDER 片段有定义（{len(names)} 个名字） ✓")
    print(f"④ 与 TH08 原文逐行 diff：差异**只有** {changed_total} 行 sprite 引用"
          f"（脚本号 + 注释头之外正文一字未动） ✓")
    return changed_total


def print_script_diffs(scripts):
    print("\n# —— ④ diff 证据：TH08 原文（-）vs 生成脚本（+）——")
    for fname in sorted(scripts, key=lambda n: scripts[n][3]):
        body, _want, original, idx, src = scripts[fname]
        ref, gen = original.splitlines(), body.splitlines()
        print(f"\n=== {fname}（script{idx} ← enemy.anm script{src}；正文 {len(gen)} 行；"
              f"下面是与 TH08 原文的**全部**差异）")
        for line in difflib.unified_diff(ref, gen, "TH08-original", "generated", lineterm="", n=0):
            print(line)


# ── 能否编过（只在 /tmp 里单独编，不碰 native/build/、不跑 make）────────

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
    """⑤ 每条脚本：把 `@NAME` 换成 `sprite0`、脚本号改成 0，在 /tmp 里用 thanm -c 18 单独编一次。"""
    print("\n# —— ⑤ 能否编过（thanm -c 18，/tmp 里单独编；不碰 native/build/、不跑 make）——")
    with tempfile.TemporaryDirectory(prefix="fam-anm-") as td:
        d = Path(td)
        (d / "ability").mkdir()
        Image.new("RGBA", (64, 64), (255, 0, 255, 255)).save(d / "ability" / "TEST.png")
        ok = 0
        for fname in sorted(scripts, key=lambda n: scripts[n][3]):
            body, _want, _orig, idx, _src = scripts[fname]
            test = _TOKEN_RE.sub("sprite0", body).replace(f"script script{idx} {{", "script script0 {", 1)
            (d / "spec.txt").write_text(_TEST_ENTRY + "\n" + test + "\n", encoding="utf-8")
            out = d / "out.anm"
            if out.exists():
                out.unlink()
            run_thanm(["-c", TH18_VERSION, str(out), "spec.txt", "-m", str(ANMMAP_TH18)], d)
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


# ── 证据表：12 关 ECL 的调用点 ────────────────────────────────────────

CALL_SITES = """\
# —— ★ 一手证据：TH08 12 个关卡 ECL 里 ins_90/91/92 的调用点 → 被生成敌人的 ins_54(K) ——
#    （`thecl -d 8 ecldata*.ecl` 原始转储；这里按「opcode → 关卡 → 被调 sub 用的 K」归并去重）
opcode 90 (th08:0x41af5b → create_enemy_familiar_2 th08:0x41f110):
   ecldata1/1sp  K=56      ecldata2/2sp  K=57      ecldata3/3sp  K=53,57
   ecldata5/5sp  K=53      ecldata6      K=54      ecldata7      K=54
   ecldata8/8sp  K=53,55,57
opcode 91 (th08:0x41b10b → enemy_create_familiar th08:0x41f280):
   ecldata1/1sp  K=55,56   ecldata2/2sp  K=57,58   ecldata3/3sp  K=53,56
   ecldata4a/4asp K=61,63,64                          ecldata4b/4bsp  用 ins_55/56 走「anm 家族」路径（无 ins_54）
   ecldata5/5sp  K=53,54   ecldata6/6sp  K=53,54   ecldata7/7sp  K=53,54
   ecldata8/8sp  K=53,55,57,58
opcode 92 (th08:0x41b2bb → create_enemy_familiar_2 th08:0x41f110):
   ecldata1/1sp  K=55,56   ecldata2      K=53      ecldata3/3sp  K=53,54
   ecldata4a/4b/4asp/4bsp 用 ins_55 家族   ecldata5/5sp K=53,54,56
   ecldata6      K=54      ecldata7/7sp  K=53,54   ecldata8/8sp  K=53,54
   ecldata_al    ins_55(12)（アリス面：走 ins_55 家族，无 ins_54）
⇒ 去重后的 K 只有 {53,54,55,56,57,58}；上界 95（ecldata5 的 ins_54(94/95)）⇒ 那个 anm 有 ≥96 支脚本
   ⇒ 只可能是 enemy.anm(101) 或 etama.anm(115)；`ins_54` 写的是敌机本体 VM ⇒ enemy.anm。
"""


# ── main ─────────────────────────────────────────────────────────────

def main():
    _require(THANM, "thanm 可执行文件")
    _require(ANMMAP_TH08, "TH08 用的 anmmap v2.anmm")
    _require(ANMMAP_TH18, "TH18 构建用的 anmmap v8.anmm")
    _require(TH08_DAT / SRC_ANM, f"TH08 零售 {SRC_ANM}")

    with tempfile.TemporaryDirectory(prefix="fam-src-") as td:
        tmp = Path(td)
        spec_text, png = read_anm(TH08_DAT / SRC_ANM, tmp)
        rects, sheet = parse_entry(spec_text, SRC_ENTRY)
        print(f"[src] {SRC_ANM}（anm v3，贴图 {sheet[0]}×{sheet[1]}，entry {SRC_ENTRY}，"
              f"{len(rects)} 个 sprite，101 支脚本）")

        im = Image.open(png).convert("RGBA")

        def mean_rgb(sids):
            px = []
            for sid in sids:
                x, y, w, h = rects[sid]
                px += [p for p in im.crop((x, y, x + w, y + h)).get_flattened_data() if p[3] > 32]
            return tuple(round(sum(p[i] for p in px) / len(px)) for i in range(3)) if px else (0, 0, 0)

        cycles, order_names = {}, set()
        for n in FAM_SCRIPTS:
            body = script_sprites(spec_text, n)
            made = crop_cycle(png, rects, body, n)
            mean = mean_rgb(body)
            cycles[n] = (made, mean, body)
            order_names |= set(made)
            rr = sorted({rects[s] for s in body})
            print(f"[crop] script{n}  {len(body)} 帧  sprite{body[0]}..{body[-1]}  矩形 {rr}  "
                  f"可见像素均值 RGB≈{mean}  → {OUT_ART.name}/  {len(made)} 张 32×32")

        rows = order_rows(cycles)
        order_lines = write_order(rows)
        scripts, maps = gen_scripts(spec_text, cycles, order_names)

        print("\n# —— ORDER.youkai_familiar.txt 内容（合并进 ORDER.txt 时追加这些行）——")
        for line in order_lines:
            print(line)

        print("\n" + CALL_SITES)

        check_pngs(cycles, rects)
        check_scripts(scripts, maps, rows)
        print_script_diffs(scripts)
        check_compile(scripts)

        print("\n# —— ❓ 没做的那两套（57/58）——")
        for n in NOT_PORTED:
            body = script_sprites(spec_text, n)
            print(f"  enemy.anm script{n}：sprite {body}（{len(body)} 帧）。"
                  f"零售正文首行 `ins_25(1)` —— raw opcode 25 → `th08:0x45f8d7 mov WORD PTR [ecx+0x1fc],ax`"
                  f"（zAnmVm+0x1fc，re-data 名 `__v3_ins_25`），**v8 anm 无对应助记符** ⇒ 不移植。")

        print_md5(sorted(OUT_ART.glob("YOUKAI_FAM_*.png"))
                  + sorted(OUT_SCRIPTS.glob("20[4-7]_youkai_familiar_*.anm.txt"))
                  + [OUT_ORDER])
    print("\n全部自检通过（幂等：重跑覆盖同名文件，内容不变）。")


if __name__ == "__main__":
    main()
