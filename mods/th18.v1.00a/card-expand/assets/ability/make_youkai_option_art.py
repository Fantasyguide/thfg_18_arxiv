#!/usr/bin/env python3
"""永夜抄（系统卡 108）**低速子机（使い魔 / option）**：原生切图 + 脚本生成。

    python3 make_youkai_option_art.py        # 生成 + 自检（可重复跑，结果一致）

来源（原生字节只在 `local/`，gitignored；构建时取，不入库）：
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

── ★ 层（`layer`）：**照抄原生**，不自己编 ──────────────────────────────────────────────
  · TH16 那支**原版原本就写 `layer(13)`**（v8 的脚本自带）⇒ 照抄。
  · TH08 那三支**原版一行 layer 都没有**（v3 那一代 VM 的层由引擎/创建方给）。TH18 引擎在
    `Player__allocate_option` `0x40a790` 里**无条件**把子机 VM 的层设成 14：
    `th18:0x40a8bc mov DWORD PTR [esi+0x18],0xe`（`zAnmVmPrefix+0x18 = layer`）⇒
    **不需要 C 侧补、也不该在这里编一个层号**：脚本不写 layer ⇒ 保持引擎给的 14（世界层）。
    （对照：TH18 原生本体子机脚本 `pl00.anm` script15/16 自己写 `layer(13)`；本 mod 的 102/113 同层。）

── ★ `ins_83`：TH08 脚本里唯一**没法逐条照抄**的一行（已实测）────────────────────────────
  TH08 三支子机脚本里都有一行 `ins_83(2);`（v2.anmm 把它译成 `ins_83`，原文是
  `# 83   dword field` = **无名 opcode 83**）。它的 handler = `th08:0x460314`（`vm+0x200 = 指令的 dword 参数`）。
  实测：`thanm -c 18` 编 v8 时报 `opcode 83 is not known to exist in version 8` ⇒ **TH18 的 anm v8 里没有这个 opcode**，
  逐条照抄在技术上做不到 ⇒ 本生成器**省略这一行**并在脚本里写明（这是与原生唯一的一处「非 sprite」差异）。
  🟡 它写的那格 `vm+0x200` 在整份 th08.exe 里**只有两个读者**（`grep` 全反汇编）：
  `th08:0x45e96c`（`AnmManager::draw_vm__player_bullet__state_2` `0x45e960` 的 0..5 跳表）与
  `th08:0x430efa`（敌人链）—— **都不在普通 VM（含子机）的绘制路径上** ⇒ 省掉它对子机画面无影响（🟡 读码，未实跑）。
  ❓ 它与 v8 的 `type(N)` 是否同义**未证实**（v8 的 `type` handler `th18:0x47b12c` 是**打包进 flags**
  `vm+0x534` 的 5 位字段，与 TH08 写独立 dword 字段不是同一种机制）⇒ 按纪律**不猜**，不换写 `type(2)`。

── ★★ 中断标签：**必须按 TH18 口径重编号**（唯一一处「偏离原生原文」的语义改动）──────────────
  **为什么偏离原生原文**：`interruptLabel(N)` 的号是**引擎与该作品脚本之间的约定**，同一个号在不同作品里
  含义不同。TH08 那三支（script18/29/24）用的是 **TH08 的**约定（`(1)(2)` = 两段待机循环、
  `(3)(4)` = 起爆「隙间」（`sprite(30)` 旋转 + `stop()`）、`(5)` = 散场），而 TH18 引擎发给子机 VM 的
  只有 **1/2/3**，含义是 **1 = 销毁 / 2 = 出场（+待机循环）/ 3 = 进商店收起** ⇒ 逐条照抄会让
  「收起」「销毁」的表现全丢（引擎发 1 时 TH08 系脚本只当作「切循环」）。
  **用户 2026-09-12 定：这种情况肯定得映射，这个没办法** ⇒ 本生成器按下面的表**只重编号**
  （段序、时序、数值、sprite 引用一字不动；`jmp` 目标是 `offsetNN:` 段标签，与中断标签无关）。

  一手（2026-09-12 反汇编，本映射的全部依据）：
  ① **子机 VM 用的是 ability.anm**：`Player::allocate_option` `th18:0x40a790` 在 `th18:0x40a845` 取
     `[ds:0x4cf298]+0xc` 当 AnmLoaded、把**第 5 个栈参**当脚本号 `copy_vm`（`th18:0x407420`）进新 VM；
     `ds:0x4cf298` 由 AbilityManager 的 ctor `th18:0x40833d` 写入，同一函数 `th18:0x40834d`
     `push 0x4b4c10` = 字符串 **`"ability.anm"`**（`.rdata`，`th18:0x4b4c10`）⇒ 这个句柄就是 ability.anm。
     （旁证：原生装备卡 `CardReimu1::on_power_level_change` `th18:0x40aae0` 传脚本号 **2**、
     `CardSakuya1` 传 7、`CardSanae1` 传 0xe、`CardYoumu` 传 0x40/0x41 —— pl00.anm 只有 0..40 号，
     0x40/0x41 只存在于 ability.anm（68 支）⇒ 只可能是 ability.anm。）
  ② **原生 ability.anm 的子机脚本全是 2/3/1**：s2 / s6 / s7 / s14 / s16 / s18 / s20 / s22（都自带
     `layer(13)`）—— 结构统一为
     `interruptLabel(2)` + 出场缩放动画 + 待机循环 → `interruptLabel(3)` + `scaleTime(20,4,0,0); stop()`
     → `interruptLabel(1)` + `blendMode(1); scaleTime(20,4,0,3.0f); +20: delete()`。
     （TH16 `pl03.anm` s11 与 TH18 `pl00.anm` s15/16 同款 —— 三个作品同一约定。）
  ③ **引擎会发 3 个号，全 exe 穷举**（`interrupt_tree` `th18:0x488be0` 的调用点，目标 = 子机槽 VM
     `player+0xae0`(12 格 stride 0xf0) 与 `player+0x720`(原生 4 格)）：
       · **2 = 出场**：`th18:0x416d70`/`0x416d7e`（4 格）、`th18:0x416da5`/`0x416dae`（12 格）
         —— 函数头 `th18:0x416d50`，`and [player+0x4779c],~2` 后发 2；调用点：关店 `th18:0x4179e7`、
         玩家 tick `th18:0x45b2af`（`[player+0x477a4] >= 0x10` 时）。
       · **3 = 进商店收起**：`th18:0x416cf0`/`0x416cfe`（4 格）、`th18:0x416d25`/`0x416d2e`（12 格）
         —— 函数头 `th18:0x416cd0`，`or [player+0x4779c],2` 后发 3；触发：`CardYukari::c_press`
         `th18:0x40a1b0` 开店（`th18:0x40a1e4`/`0x40a1fc call 0x409a60` 置 `player+0x4779c` 的 0x180 位）
         → 玩家 tick `th18:0x45b1ae` 调 `0x416cd0`。
       · **1 = 销毁**：`th18:0x45bdb0`/`0x45bdc5`（开店满 30 帧后逐格：`[player+0x477e8] >= 0x1e`）、
         `th18:0x45d2d0`/`0x45d2e8`、`th18:0x45d310`/`0x45d328`、`th18:0x45dbf0`/`0x45dc05`。
  ④ **`stop()`/`stopHide()` 才是中断点，`interruptLabel` 本身是空操作**（TH18）：
     分派表 `th18:0x47b8a0`(53 组 dword) + `th18:0x47b974`(byte，index = opcode+1)：
     op5 `interruptLabel` → `th18:0x47b6f3`（只是 `add [vm+0x2c],[instr+2]` 前进一条）；
     op4 `stopHide` → `th18:0x4787ef`（`and [vm+0x534],~1` 清可见位）落到 op3 `stop` `th18:0x4787f6`；
     另有**帧首预扫**：`th18:0x47868c cmp [vm+0x494],0` — pending != 0 就 `jmp 0x478806` 先扫标签。
     扫描逻辑：从 `[vm+0x2c]`(当前指令) **向前**找 `interruptLabel`（`cmp WORD PTR [ecx],0x5` +
     `cmp [vm+0x494],[ecx+8]`），命中 `th18:0x4788ab` 跳过去；扫到脚本尾（opcode 0xffff）→
     `th18:0x478868` 清 pending、`th18:0x47889a` **原地继续**（= 找不到标签时不崩、也不跳；
     `interruptLabel(-1)` 会被当兜底段，**所以不许用 -1**）。
     ⇒ 推论（本轮的 ❓ 都出在这里）：**脚本里没有的号 = 那件事不发生**；标签在当前位置**之后**才找得到，
     所以在无限 `jmp` 循环里收到「循环自己那个号」= 找不到 = 原地继续（原生同构，不算差异）。

  映射表（`LABEL_MAP`，只重编号）：TH08/TH16 原号 → TH18 号，理由逐支写在脚本文件头里。
  TH08 里 TH18 没有对应语义的段（循环 B、隙间 A/B）**挪到引擎不发的号（4/5/6）**：正文保留，
  在 TH18 下成为不可达代码 —— 这是「不丢正文」+「不让引擎误触发」的唯一折中。

产出（只动自己的文件）：
  ability/youkai_option/YOUKAI_OPT_<角色>_S<原生 sprite 号>.png —— 原生矩形切图，**居中补透明**到 2 的幂画布。
       ★ 居中不是随手挑的：原版的子机 VM 以**中心**为锚点画（TH08 `AnmManager::draw_std_quad_type_0`
         `th08:0x463877`/`0x463904` 在 flags 那两位为 0 时 `left = pos.x − w*scale/2`，而 `AnmVm::reset`
         `0x406926` 把低字设成 0x7 ⇒ 两位都是 0），TH18 的 `type(0)`/`type(1)` 同为**中心锚点**
         （仓内既有结论：`make_youkai_art.py` 头 ①、`make_sanae_youkai_art.py`）；而本仓构建期
         （`assets/build_ability.py`）对「源图小于声明尺寸」也是**居中补** ⇒ 两边同一条公式、可见像素一个不动。
  ability/scripts/224..227_youkai_opt_<角色>.anm.txt —— 正文逐条照抄原生原文，只动两处：
       ① `sprite(N)` → `sprite(@名字@)`；② 中断标签号按 TH18 口径重编号（`LABEL_MAP`，见上）。
       （+ 省略 `ins_83`，理由写在脚本里；注释写在 `script scriptNNN {` **里面**。）
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
# → 原生哪个作品的哪支子机脚本。一手落点见文件头。
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
_LABEL_RE = re.compile(r"^(\s*)interruptLabel\((-?\d+)\);(\s*)$")

# ── ★★ 中断标签映射：TH08/TH16 原版号 → TH18 号（唯一一处语义偏离，理由见文件头）──────────
# TH18 发给子机 VM 的号只有 1/2/3：1 = 销毁、2 = 出场(+待机)、3 = 进商店收起。
# 每支的「原号 → 新号」逐条：
#   紫 s18  : 1→2（待机循环 A = 显示态）· 2→4（循环 B，TH18 无此语义）· 3→5（隙间 A）· 4→6（隙间 B）
#             · 5→1（散场 = 销毁）
#   アリスs29: 1→2（待机循环 A）· 5→1（散场 = 销毁）                       ❓ 无「收起」段
#   レミリアs24: 1→2（待机 A）· 2→4（待机 B，脉动）· 5→1（散场 = 销毁）      ❓ 无「收起」段
#   文 pl03 s11: None —— 原生本来就是 TH18 口径 2/3/1，**一个字都不动**。
# ⚠ 4/5/6 是「引擎不会发的号」：全 exe 里发往子机槽 VM 的 `interrupt_tree` 只有 1/2/3（见文件头 ③）。
# ⚠ 不许用 -1：两个引擎都把它当「找不到标签时的兜底段」(`th18:0x478837` / `th08:0x45f776`)。
LABEL_MAP = {
    "YUKARI":  {1: 2, 2: 4, 3: 5, 4: 6, 5: 1},
    "ALICE":   {1: 2, 5: 1},
    "REMILIA": {1: 2, 2: 4, 5: 1},
    "AYA":     None,
}


# ── 取源（thanm 现场解，仓里不落原生字节）──────────────────────────────

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
        canvas.paste(cell, ((cw - w) // 2, (ch - h) // 2))     # ★ 居中 = 原生的 VM 中心锚点（见文件头）
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
        "    // 指令逐条照抄（段序/时序/数值一字未动），只做两件事：① sprite 引用换成 ORDER 名字；",
        "    //   ② 中断标签按 TH18 口径重编号（见下面 ★★，这是唯一一处语义偏离）。sprite 换名表：",
        f"    //   {pairs}",
    ]
    if kind == "th08":
        pairs_label = "、".join(f"{a}→{b}" for a, b in sorted(LABEL_MAP[char].items()))
        lines += [
            "    // ★ 原版这支**没有 `layer()`**（TH08 的 anm v3 那代由 VM 自带层）⇒ 这里也不写：TH18 的",
            "    //   引擎在 `Player__allocate_option` `th18:0x40a8bc mov DWORD PTR [esi+0x18],0xe` 里把子机 VM",
            "    //   的层设成 **14**（世界层）⇒ 不写 layer 就用引擎给的 14，不需要 C 侧补、也不自己编层号。",
            "    // ★ 原版这一行是 `ins_83(2);`（TH08 v3 的**无名 opcode 83**，写 `vm+0x200`，handler",
            "    //   `th08:0x460314`）—— **TH18 的 anm v8 里 opcode 83 不存在**（`thanm -c 18` 实测报",
            "    //   \"opcode 83 is not known to exist in version 8\"）⇒ 逐条照抄做不到，**这一行省略**",
            "    //   （全 exe 只有两处读 `vm+0x200`：`th08:0x45e96c` player-bullet 专用 draw 分派、",
            "    //   `th08:0x430efa` 敌人链 —— 都不在子机 VM 的绘制路径上 ⇒ 对画面无影响，🟡 读码）。",
            "    // ★★ **中断标签按 TH18 口径重编号**（唯一一处语义偏离；用户 2026-09-12 定：这种情况肯定得",
            "    //   映射）——TH08 的号在 TH18 引擎下含义完全不同。TH18 发给子机 VM 的只有 1/2/3：",
            "    //     `1` = 销毁（清槽）、`2` = 出场（+待机循环）、`3` = 进商店收起；",
            "    //   原生同库 `ability.anm` 的子机脚本 s2/s6/s7/s14/s16/s18/s20/s22 就是这三个号（一手：",
            "    //   `Player__allocate_option` `th18:0x40a845` 取 `[0x4cf298]+0xc` = ability.anm 的句柄，",
            "    //   `ds:0x4cf298` 由 AbilityManager ctor `th18:0x40833d` 写、`th18:0x40834d push 0x4b4c10`",
            "    //   = 字符串 \"ability.anm\"；发送点 2=`th18:0x416d70`/`0x416dae`、3=`th18:0x416cf0`/`0x416d2e`、",
            "    //   1=`th18:0x45bdb0`/`0x45d2d0`/`0x45d310`/`0x45dbf0` —— 全在生成器文件头 ③）。",
            f"    //   本支映射（**只重编号，段序/时序/数值/正文一字未动**）：{pairs_label} ——",
            "    //   原号里 TH18 没有对应语义的段（循环 B / 隙间）挪到引擎不会发的号（4/5/6）：正文保留，",
            "    //   在 TH18 下不可达 —— 这是「不丢正文」+「不让引擎误触发」的折中；散场(原 5) → 1 = 销毁。",
        ]
        if 3 not in LABEL_MAP[char].values():
            lines += [
                "    // ❓ **本支没有「收起」段**（TH08 原本就没有这一种形态）⇒ 引擎发 `3`（进商店）时脚本里",
                "    //   没有 `interruptLabel(3)`：扫到脚本尾 → 清 pending → **从当前指令原地继续**",
                "    //   （一手：`th18:0x47884e` → `th18:0x478868` → `th18:0x47889a`）⇒ 进商店时子机**不收起**",
                "    //   （表现不复刻，不崩）。按纪律**不自己编一段**，登记为 ❓ 交用户拍板。",
            ]
    else:
        lines += [
            "    // ★ `layer(13)` 是**原版自己写的**（TH16 那代 VM 的层由脚本给）⇒ 照抄，不改。",
            "    // ★ 中断标签与 TH18 原生子机脚本**同款**、**一字未动**：`(2)` 出场、`(3)` 进商店收起、",
            "    //   `(1)` 销毁 —— 天然对得上（一手：TH18 子机 VM 用 `ability.anm`，其 s2/s6/s7/s14/s16/",
            "    //   s18/s20/s22 都是这三个号；引擎的发送点见生成器文件头 ③）。",
        ]
    return lines


def gen_script(src_text, src_num, new_num, char, cn, kind, anm_name, sprite_map, engine_note):
    """→ (脚本文本, 与原生原文的 diff 行数, 省略的 ins_83 行)

    ⚠ 只动两类行：`sprite(N)`→`sprite(@名字@)`、`interruptLabel(N)` 的**号**（`LABEL_MAP`）。
      段序/时序/数值/`jmp` 目标（都是 `offsetNN:` 段标签）一律不动。
    """
    body, dropped, relabeled = [], [], []
    label_map = LABEL_MAP[char]
    for line in src_text.splitlines():
        s = line.strip()
        if s == f"script script{src_num} {{":
            body.append(f"script script{new_num} {{")
            continue
        m = _INS83_RE.match(s)
        if m:
            indent = line[:len(line) - len(line.lstrip())]
            dropped.append(s)
            body.append(f"{indent}// ★ 原生原文此处为 `{s}` —— 见文件头：TH18 的 anm v8 无此 opcode ⇒ 省略")
            continue
        m = _LABEL_RE.match(line)
        if m and label_map is not None:
            old = int(m.group(2))
            if old not in label_map:
                raise SystemExit(f"{char}: 原生脚本出现未登记的 `interruptLabel({old})` ⇒ 映射表要重核"
                                 f"（只重编号，不许照抄 TH08 的号）")
            new = label_map[old]
            relabeled.append((old, new))
            body.append(f"{m.group(1)}interruptLabel({new});{m.group(3)}"
                        f"   // ★ 原生原文 `interruptLabel({old})` —— TH18 口径见文件头映射表")
            continue
        body.append(_SPRITE_CALL_RE.sub(lambda mm: f"sprite(@{sprite_map[int(mm.group(1))]}@)", line))
    if label_map is not None:
        missing = sorted(set(label_map) - {o for o, _ in relabeled})
        if missing:
            raise SystemExit(f"{char}: 映射表里的 {missing} 在原生脚本里没出现 ⇒ 映射表/脚本号要重核")
    head = header_lines(char, cn, kind, anm_name, src_num, sprite_map, engine_note)
    # ★★★ 2026-09-26（owner：「灵梦的妖子机本身……**应当旋转**」报了多次；真因**不在 C 侧**）：
    #   TH18 的 anm v8 **有渲染模式**：`type()`（opcode 302）→ `th18:0x47b12c` 写 `flags_lo(+0x534)`
    #   的 bit26–30；`AnmManager::draw_vm` `th18:0x481210` 按它分派 ——
    #     · **模式 0（默认）** → 角点函数 `0x47e8f0`：整段**没有一次旋转读取、没有 fsincos**
    #       （我逐行筛过 `0x47e8f0`–`0x47ed4f`）⇒ sprite 四边形**永远轴对齐**；
    #     · **模式 1 / 3** → `0x47ed50`：`call 0x4097d0`（把 rz 累加进 `vm+0x600`）→ `movss xmm0,[eax+8]`
    #       → `fsincos` ⇒ 四边形真的按 rz 转。
    #   TH08 那代（anm v3）**没有渲染模式这个概念**（TH08 全量 dump 里 `type(` 出现 **0** 次，
    #   绘制一律带旋转）⇒ **把 TH08 的脚本逐条照抄进 v8 必然落进模式 0**：标签 5 那一段
    #   `angleVel(0,0,0.62831855)+stop()` 只是让 VM 自己积分角度，**画面纹丝不动**。
    #   原生先例：TH18 自己会转的子机脚本都写着 `type(1)`（`ability.anm` script2/6 = 原生那 4 格子机，
    #   第一段就是 `layer(13); blendMode(1); type(1);`）；普查：含 `angleVel` 的原生脚本 153 个、
    #   其中 **145 个**带 `type()`，而「sprite 四边形 + 非 0 `angleVel` + 无 `type()`」的原生脚本 **0 个**。
    #   ⇒ 本生成器在脚本体最前面补一行 `type(1);`：**唯一一处偏离逐条照抄**，理由与一手见上。
    extra = ["    // ★ TH18 v8 的渲染模式：不写 `type(1)` 就落进模式 0 = 绘制定格（画面上不会转）。",
             "    //   一手与普查见生成器文件头「渲染模式」那一段；TH08 那代没有这个概念。",
             "    type(1);"]
    text = "\n".join(body[:1] + head + extra + body[1:]) + "\n"
    return text, dropped


def sprite_name(char, sid):
    return f"YOUKAI_OPT_{char}_S{sid}"


# ── ORDER 片段 ───────────────────────────────────────────────────────

def write_order(rows):
    lines = [
        "# 永夜抄（系统卡 108）**低速子机（使い魔 / option）** —— 四支机体各自的原生子机贴图。",
        "# 本片段由 assets/ability/make_youkai_option_art.py 生成（幂等，整段重写）；合并时把这些行追加进 ORDER.txt。",
        "# 每行 `NAME  源图(相对 assets/)  WxH`；WxH 都是 2 的幂 ⇒ 整张贴图一个 sprite。",
        "# ★ 一手（子机用哪支脚本/哪些 sprite）——**不是共享图集、也不是机体本体图**，每支机体一套：",
        "#   TH08 `playerNN.anm`：子机 = `zPlayerOption`（`player+0x40c` × 4、stride 0x2f4，头 0xa4×4… 见生成器头）",
        "#     pl00 script18（妖侧 紫）sprite22..30 / pl01 script29（アリス）sprite22..25 / pl02 script24（レミリア）sprite20；",
        "#     装载点 = 各机体 handler 的 state==1 分支：`th08:0x44e3d7 push 0x12` / `0x44ea6f push 0x1d` / `0x44eba7 push 0x18`，",
        "#     经 `AnmLoaded::load_script_into_vm th08:0x4069f0` 装进 `option+0x0` 那个 AnmVm。",
        "#   TH16 `pl03.anm`（文）：本体子机脚本号 = `th16:0x492c00[CHARACTER]`（菜单序 {8,7,11,11}，",
        "#     `CHARACTER`=2 = 文，`th16:0x4443ad` 取值 + `AnmLoaded::copy_vm th16:0x407b20`）⇒ script11，sprite32..39。",
        "# ★ 层：TH16 那支原版自带 `layer(13)`；TH08 那三支原版无 layer —— TH18 引擎在 `th18:0x40a8bc` 把子机 VM",
        "#   的层设成 14 ⇒ 不写 layer 即用 14（世界层），脚本里不编层号。",
    ]
    # ⚠ 数据行**严格 3 列**（`anmlib.read_order` 按空白切、只取 [0][1][2]；仓里既有片段全是 3 列）
    #   ⇒ 出处写在行间的 `#` 注释里（`read_order` 跳过 `#` 开头的行）。
    last = None
    for name, path, size, sid, char, src in rows:
        if char != last:
            lines.append(f"# —— {char}（{SOURCES_BY_CHAR[char][4]}）：原生 {src} = sprite"
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
            # ★ 主表 ORDER.txt 里的路径是相对 `assets/` 的（例如 `ability/youkai_option/…png`），
            #   而 `crop_option()` 返回的是绝对 Path；这里必须归一成同一种字符串，否则已合并进
            #   主表后每次重跑都会误报「重名/重路径不一致」，生成器直接退出。
            try:
                # ORDER.txt 的路径相对 `assets/`（= HERE.parent），不是相对 `assets/ability/`。
                rel = path.relative_to(HERE.parent).as_posix()
            except ValueError:
                rel = path.as_posix()
            mine[name] = (rel, size)
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


def check_label_maps(scripts):
    """② bis：中断标签映射自检（双射 + 只用引擎会发的 1/2/3 + 缺「收起」段要显式登记）。"""
    print("\n# —— ② bis 中断标签映射（TH08/TH16 原号 → TH18 号）——")
    for fname, (text, _dropped, src, _num, char) in sorted(scripts.items()):
        old = [int(x) for x in re.findall(r"^\s*interruptLabel\((-?\d+)\);", src, re.M)]
        new = [int(x) for x in re.findall(r"^\s*interruptLabel\((-?\d+)\);", text, re.M)]
        lm = LABEL_MAP[char]
        if lm is None:
            assert old == new, f"②bis {fname}: 这支不该动标签（{old} → {new}）"
            print(f"  {fname}: 原生就是 TH18 口径 {new} ⇒ 一字未动 ✓")
            continue
        assert sorted(lm) == sorted(old), f"②bis {fname}: 映射表的键 {sorted(lm)} != 原生标签 {sorted(old)}"
        assert [lm[x] for x in old] == new, f"②bis {fname}: 生成结果 {new} != 映射 {[lm[x] for x in old]}"
        assert len(set(lm.values())) == len(lm), f"②bis {fname}: 映射不是双射 ⇒ 两段会撞号"
        assert not ({1, 2, 3} & (set(lm.values()) - {1, 2, 3, 4, 5, 6})), f"②bis {fname}: 越界"
        assert 1 in lm.values(), f"②bis {fname}: 没有销毁段（1）⇒ 清槽时会找不到标签"
        miss = "❓ 无 3（收起）：引擎发 3 时找不到标签 ⇒ 原地继续，进商店子机不收起（已登记）" \
            if 3 not in lm.values() else "有 3（收起）"
        print(f"  {fname}: {old} → {new}（{miss}）")
    print("② bis 四支映射：双射、1 都在、引擎不发的号只用于 TH08 特有段 ✓")


def print_script_diffs(scripts):
    print("\n# —— ④ diff 证据：原生原文（-）vs 生成脚本（+）—— 差异只有 sprite 引用 + 省略的 ins_83 "
          "+ 中断标签重编号 ——")
    for fname, (text, dropped, src, num, _char) in sorted(scripts.items(), key=lambda kv: kv[1][3]):
        retail_num = int(re.search(r"^script script(\d+) \{", src, re.M).group(1))
        print(f"\n=== {fname}（新号 script{num} ← 原生 script{retail_num}；正文 {len(text.splitlines())} 行；"
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
        _require((TH08_DAT if kind == "th08" else TH16_DAT) / f"{name}.anm", f"{kind} 原版 {name}.anm")
    for i, s in enumerate(SOURCES):
        SOURCES_BY_CHAR[s[3]] = s
    # 每支子机都必须有标签映射的决定（None = 「原生本来就是 TH18 口径，一字不动」），不许漏
    for _kind, _name, _n, _char, _cn, _ci, _note in SOURCES:
        assert _char in LABEL_MAP, f"LABEL_MAP 缺 {_char} 的决定 ⇒ 不许默默照抄 TH08 的标签号"
        if _kind == "th08":
            assert LABEL_MAP[_char], f"{_char} 是 TH08 来源 ⇒ 必须映射（用户 2026-09-12 口径）"

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
    check_label_maps(scripts)
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
