#!/usr/bin/env python3
"""卡 108（TH08 永夜抄）**妖侧弹的弹体贴图** —— 把原作那一支 anm 脚本与它的 sprite 搬进 `ability.anm`。

owner 口径（2026-09-26 实跑）：「**妖机体的弹幕样式应该都不对，发的弹幕本身得搬过来**」。
此前所有妖侧行都填 `anm = 21`（TH18 原生 `ability.anm` 的通用弹），所以样式全是本作的、不是永夜抄的。

## 一手（怎么知道该搬哪一支）

TH08 的 `plyNNas.sht` 每一行有 `+0x24 anm 脚本基号`，引擎**用它 +10** 装进弹的 VM：
一手 `th08:0x44fd32 movsx edx,WORD PTR [ecx+0x24]` → `0x44fd36 add edx,0xa` → `push edx` → `call 0x4069f0`
（`AnmLoaded::SetAndExecuteScriptIdx`）⇒ **弹体脚本 = 行里的 `+0x24` + 10**（`engine/sht/th08/01` §3）。

逐档读出来（本机 `local/th08.v1.00d/dat/plyNNas.sht` 直读）→ 弹体脚本：

| 角色 | 主炮行 `+0x24` → 脚本 | 子机行 `+0x24` → 脚本 | 脚本用到哪几张 sprite |
| --- | --- | --- | --- |
| 霊夢・紫（pl00）| 4 → **14** | 6 → **16** | sprite35（64×16）/ sprite36（64×16）|
| 魔理沙・アリス（pl01）| 12 → **22** | 14/15/16/17 → **24/25/26/27** | sprite30（64×16）/ sprite32（448×32，逐档只差 `scale.y` 0.5/0.6/0.7/0.8）|
| 咲夜・レミリア（pl02）| 2 → **12** | 4 → **14** | sprite22（48×16）/ sprite23（16×32）|

早苗那一侧是 TH16：`pl03sub.sht` 的行 `+0x22 anm = 0` ⇒ 弹体脚本 = **`pl03sub.anm` script0**
（一手：`engine/season/th16/01` §5.6「副季节弹的贴图 = `pl0Xsub.anm` 里 `sht+0x22` 号脚本画的 sprite」），
它用 sprite0..3（每张 64×64）。

## 产物（都在 `assets/ability/` 下；**只追加**，不动既有 entry/sprite 号）

- `yk_bullet/<名字>.png` —— 源图里那一张 sprite 的 1:1 切图，**居中补到 2 的幂画布**
  （与 `make_youkai_option_art.py` 同一条公式：TH18 的绘制以 sprite 矩形中心为锚点）。
- `scripts/3NN_yk_bullet_*.anm.txt` —— 逐条照抄原脚本，只做三件事：
  ① `sprite(spriteN)` → `sprite(@名字@)`；
  ② 补 `type(1)`（**TH18 v8 的渲染模式**：不写就落进模式 0 = 绘制定格，见 `AUDIT.md` §BE #95）；
  ③ 省掉 v8 里**不存在**的助记符（`ins_83` / `ins_25` / `scrollX`），逐条在脚本注释里点名。
- `entries/ORDER.yk_bullet.txt` —— 待合并进 `ORDER.txt` 末尾的片段（整段重写、幂等）；
  本脚本**同时**把这段 append 进 `ORDER.txt`（它自己是共享件，按纪律登记 `AUDIT.md` §BE）。
- stdout —— ORDER 片段 + 自检（尺寸/非空/占位符/能否编过/重名/幂等 md5）。

用法：`python3 assets/ability/make_youkai_bullet_art.py`（在 `mods/th18.v1.00a/card-expand` 下任意路径）
"""
import hashlib
import re
import subprocess
import sys
import tempfile
from pathlib import Path

from PIL import Image

HERE = Path(__file__).resolve().parent          # assets/ability（本脚本仍住在这里，产物不放这里）
REPO = HERE.parents[4]                          # → renkolab
# ★★★ 2026-09-26 第十三批收尾：产物**整体挪出 `assets/ability/**`**。
#   原因（§BE #105）：妖侧行里的 `+0x22` 由**主炮路径**按 `pl0X.anm` 解析 ⇒ 这 7 支脚本的真正落地处是
#   四个 `pl0X.anm`（由 `assets/build_player_anm.py` 的 `bullet` 追加件读本目录的 spec 文本）。
#   留在 `assets/ability/scripts/` 会有两个坏处：① `build_ability.py` 会把它们编进 `ability.anm`
#   （**死资产**，还占 29 KB 图集）；② 每次重生成都会让 `ability.anm` 无缘无故变 md5（部署噪声）。
#   ⇒ 现在：`assets/yk_bullet/` = 成员 PNG + `scripts/` 里的 7 支 spec；**不再写任何 ability 侧文件**。
YKB_ROOT = HERE.parent / "yk_bullet"            # assets/yk_bullet
OUT_ART = YKB_ROOT
OUT_SCRIPTS = YKB_ROOT / "scripts"

THANM = REPO / "local/vendor/thtk/build/thanm/thanm"
TH08_DAT = REPO / "local/th08.v1.00d/dat"
TH16_DAT = REPO / "local/th16.v1.00a/dat"
TH08_ANM = REPO / "local/th08.v1.00d/anm"
TH16_ANM = REPO / "local/th16.v1.00a/anm"

SCRIPT0 = 364          # 脚本文件编号从这里起连续（现有序号最大 363 = ufo_summon_spark）
ATLAS = "ATLAS_YK_BULLET"

# 成员：ORDER 名 → (来源, 作品, anm 名, sprite 号, 是否裁一刀, 裁剪框)
MEMBERS = [
    # 霊夢・紫（TH08 player00 script14/16，sprite35/36 = 64×16）
    ("YKB_R_MAIN", "th08", "player00", 35, None),
    ("YKB_R_OPT",  "th08", "player00", 36, None),
    # 魔理沙・アリス（TH08 player01 script22/24..27，sprite30 = 64×16；sprite32 = 448×32）
    ("YKB_M_MAIN", "th08", "player01", 30, None),
    # ★ sprite32 是**一条 448×32 的滚动纹理**（原脚本 `scrollX(0.0625)` 让它横向流动；v8 没有 scrollX）
    #   ⇒ 只切**前 256×32**（那一张里非空像素就在 0..255，见生成器自检），静止显示。
    ("YKB_M_OPT",  "th08", "player01", 32, (0, 0, 256, 32)),
    # 咲夜・レミリア（TH08 player02 script12/14，sprite22 = 48×16、sprite23 = 16×32）
    ("YKB_S_MAIN", "th08", "player02", 22, None),
    ("YKB_S_OPT",  "th08", "player02", 23, None),
    # ★★★ 2026-09-27（owner：咲夜弹「打到敌人之后不消失、匀速慢速上升」）：**命中贴图**
    #   魔理沙主炮的命中脚本（`player01` script23）用的是**另一张**图（sprite31，64×16），
    #   其余角色的命中脚本复用弹体那张 ⇒ 只有这一张要新切。见下面 `SCRIPTS` 的 `hit` 字段。
    ("YKB_M_HIT",  "th08", "player01", 31, None),
    # ★★★ 2026-09-27（owner：「**文文子机子弹应当是红色的，并且没有分叉**」「**缺乏文文自机弹幕**」）：
    #   早苗那一侧整批换成 **TH16 `pl03.anm`**（= 文自己的主炮弹族），**不再用 `pl03sub.anm`**
    #   （那是季节弹：金黄 + 四帧循环，正是 owner 三条偏差里「不是红色」的那一族）。
    #   一手（`thanm -l 16 -m v8.anmm local/th16.v1.00a/dat/pl03.anm`，entry0，`yOffset` = 0）：
    #     · 本体弹 = **script5**：`sprite24`（32×16）+ 命中支 `sprite25/26/27`（32×16、`alpha 192`）。
    #     · 子机弹 = **script6/7/8/9**：`sprite28`（64×64）+ 命中支 `sprite29/30/31`；
    #       **`alpha(170)` + `color(255, 48, 48)` = 红**（对照 `script10` 同族写的是 `(224,192,64)`
    #       金黄）⇒ owner 说的「和秋季节子机一致但是**红色**」= **同一族贴图 + 红染色** ✓；
    #       四支逐档只差 `scale(1.0, 0.3/0.6/1.0/1.4)` ⇒ 与行里递增的判定盒 48×8/16/32/48 对上 ✓。
    #   ⚠ 命中支引用的就是 25..31 这几张 ⇒ 它们必须是**成员**（`sprite(@名字@)` 要替换得到），不能省。
    ("YKB_A_BODY",    "th16", "pl03", 24, None),
    ("YKB_A_BODY_H1", "th16", "pl03", 25, None),
    ("YKB_A_BODY_H2", "th16", "pl03", 26, None),
    ("YKB_A_BODY_H3", "th16", "pl03", 27, None),
    ("YKB_A_OPT",     "th16", "pl03", 28, None),
    ("YKB_A_OPT_H1",  "th16", "pl03", 29, None),
    ("YKB_A_OPT_H2",  "th16", "pl03", 30, None),
    ("YKB_A_OPT_H3",  "th16", "pl03", 31, None),
]

# 脚本：ORDER 名 → (作品, anm 名, 源脚本号, [sprite 号…], 说明, 命中=(命中脚本号, [命中 sprite 号…]) 或 None)
#
# ★★★ 2026-09-27（owner 实跑：「**咲夜妖机体的所有弹幕打到敌人之后不消失，匀速慢速上升**」，
#   并指出这是「改攻击方式的老 bug」）：**TH08 的每一支弹都配一支「命中脚本」**——
#   行里的 `+0x24` 是**基号**：命中 = 基号 **+11**（弹体 = +10，见 `engine/sht/th08/01` §3
#   与 `th08:0x44fd32`/`0x44fd36`）。TH18 没有「换脚本」这条路（AL31 已证），
#   对位 = **同一个 VM 的 `interruptLabel(1)` 支**：引擎默认命中路径
#   （`th18:0x45f6a0` → 取 `bullet+8` 的 VM → 若 `vm+0x5e0 != 0` 就 `table[0x4b4308+…]` 发 interrupt
#   → `0x45f6f0`）只想得动「VM 的 interrupt 支」；**没有那一支**时它照样写
#   `bullet+0x8c = 2` 与 `bullet+0x60 *= 0.125`（= 速度变成 1/8）——
#   于是弹**不消失**、继续以 **1/8 的速度**往上飘 ⇒ 正是 owner 看到的
#   「不消失 + 匀速慢速上升」（咲夜主炮 20 ⇒ 2.5、子机 12 ⇒ 1.5，最显眼）。
#   ⇒ 现在把 TH08 的命中脚本（`+0x24+11`）逐条翻进 `interruptLabel(1)` 支，末尾 `delete()`。
#   `None` = 源里那一支**自带** `interruptLabel(1)`（TH08 player01 script24/25、TH16 pl03sub script0），
#   已经在正文里，不重复加。
SCRIPTS = [
    ("YKB_R_MAIN", "th08", "player00", 14, [35],        "霊夢・紫 妖侧**主炮**弹（`ply00as` 主炮行 `+0x24`=4 ⇒ +10）",        (15, [35])),
    ("YKB_R_OPT",  "th08", "player00", 16, [36],        "霊夢・紫 妖侧**子机**弹（`ply00as` 子机行 `+0x24`=6 ⇒ +10）",        (17, [36])),
    ("YKB_M_MAIN", "th08", "player01", 22, [30],        "魔理沙・アリス 妖侧主炮弹（`+0x24`=12 ⇒ +10）",                    (23, [31])),
    ("YKB_M_OPT",  "th08", "player01", 24, [32],        "魔理沙・アリス 妖侧子机弹（`+0x24`=14 ⇒ +10；逐档 25/26/27 只差 `scale.y`）", None),
    ("YKB_S_MAIN", "th08", "player02", 12, [22],        "咲夜・レミリア 妖侧主炮弹（`+0x24`=2 ⇒ +10）",                     (13, [22])),
    ("YKB_S_OPT",  "th08", "player02", 14, [23],        "咲夜・レミリア 妖侧子机弹（`+0x24`=4 ⇒ +10）",                     (15, [23])),
    # ★★★ 2026-09-27（owner 三条口径的收口）：早苗 = **TH16 的文**，取它 **`pl03.anm` 主炮那一族**
    #   （`pl03sub.anm` 是季节弹，owner 明确说「只取其低速部分复刻」而低速那半的子机行
    #   `anm = 1..4` 按 **+5** 落在 `pl03.anm` script6..9 ⇒ 见 `engine/sht/th16/01` §5.7c）。
    #   五支都**自带** `stop(); interruptLabel(1); … delete()` ⇒ `hit=None`（不重复加）。
    ("YKB_A_BODY", "th16", "pl03", 5, [24, 25, 26, 27], "文（TH16 pl03）妖侧**本体**弹（script5，两列共用；命中支 25/26/27 自带）", None),
    ("YKB_A_OPT1", "th16", "pl03", 6, [28, 29, 30, 31], "文 妖侧**子机**弹 档1（script6：红 `color(255,48,48)`、`scale.y` 0.3）", None),
    ("YKB_A_OPT2", "th16", "pl03", 7, [28, 29, 30, 31], "文 妖侧**子机**弹 档2（script7：同上、`scale.y` 0.6）", None),
    ("YKB_A_OPT3", "th16", "pl03", 8, [28, 29, 30, 31], "文 妖侧**子机**弹 档3（script8：同上、`scale.y` 1.0）", None),
    ("YKB_A_OPT4", "th16", "pl03", 9, [28, 29, 30, 31], "文 妖侧**子机**弹 档4（script9：同上、`scale.y` 1.4）", None),
]

_SPRITE_DEF_RE = re.compile(r"sprite(\d+):\s*\{\s*x:\s*(\d+),\s*y:\s*(\d+),\s*w:\s*(\d+),\s*h:\s*(\d+)\s*\}")
_SPRITE_CALL_RE = re.compile(r"sprite\(sprite(\d+)\)")
_NAME_RE = re.compile(r"@([A-Za-z_][A-Za-z0-9_]*)@")
# v8 里没有的助记符（逐条在产物脚本里点名；不要静默丢）
# ★★★ 2026-09-26 第十二批：**`ins_25(N)` 不再丢** —— 它是 TH08 的「按 VM 的旋转画」开关，
#   在 TH18 里的对位就是 **`rotateAuto(N)`**。一手（本轮新读，订正 `engine/enemy/th08/02` §3.2 的
#   「dead store」结论）：`Player::UpdateBulletVms 0x4512f0` 在循环里读 **`bullets[i]+0x1fc`**
#   （`0x451343 movsx eax,WORD PTR [edx+0x1fc]` / `test eax,eax / je`），非 0 时
#   `0x45134e mov edx,[ecx+0x450]`（= 那颗弹 VM 的 rotation）→ `0x45135b call 0x40ec00`
#   ⇒ **`+0x1fc` 正是 `ins_25` 写的那一格，而且自机弹的更新路径真的读它**
#   （`PlayerBulletVm` 的结构是 `AnmVm(0x2a4) + …` ⇒ 该偏移就是 VM 自己的字段）。
#   ⚠ 上一版把 `ins_25(1)` 当死存丢掉 ⇒ 弹的贴图**不跟着飞行方向转**：我们切来的弹体贴图是
#   **横向**的（64×16，尖头朝 +x），而妖侧行是垂直向上飞（angle = −π/2）⇒ 观感就是「样式不对」
#   （owner 报的那条，在贴图搬对之后仍然存在的原因之一）。TH18 的原生弹脚本同样写 `rotateAuto(1)`
#   （`pl00.anm` script5/6 逐条），⇒ 翻译过来即与原生同构。
#   `ins_83(N)` 仍然**只登记不翻译**（v2 的「dword field」，TH18 无对应；见 §BE #105 的未决项）。
_DROP_RE = re.compile(r"^\s*(ins_83\(\d+\);|scrollX\([^;]*\);)\s*$")
_INS25_RE = re.compile(r"^\s*ins_25\((\d+)\);\s*$")


def _require(p, what):
    if not p.is_file():
        raise SystemExit(f"缺 {what}：{p}（样本由用户放进 `local/`，别去下载；缺 thanm 先 bash tooling/thtk/build.sh）")


ANMMAP_V2 = REPO / "local/vendor/thpages/static/mapfile/v2.anmm"   # TH08（anm v3 那一代）
ANMMAP_V8 = REPO / "local/vendor/thpages/static/mapfile/v8.anmm"   # TH16 / TH18


def read_anm(kind: str, name: str, workdir: Path) -> str:
    """解出该 anm 的文本（thanm 现场解、仓里不落原生字节）。

    ★ **必须带 `-m <anmmap>`**：不带的话 thanm 打的是 `ins_300(...)` 这种**裸 opcode**，
    我们的 `sprite(spriteN)` 替换就一条都匹配不上（第一次跑就踩了：早苗那支整段没换名）。
    """
    if kind == "th08":
        _require(TH08_DAT / f"{name}.anm", "TH08 anm")
        ver, path, mp = "8", TH08_DAT / f"{name}.anm", ANMMAP_V2
    else:
        _require(TH16_DAT / f"{name}.anm", "TH16 anm")
        ver, path, mp = "16", TH16_DAT / f"{name}.anm", ANMMAP_V8
    _require(mp, "anmmap")
    r = subprocess.run([str(THANM), "-l", ver, "-m", str(mp), str(path)], capture_output=True, text=True)
    noise = [l for l in r.stderr.splitlines() if l.strip() and "ignoring" not in l]
    if r.returncode != 0 or noise or not r.stdout.strip():
        raise SystemExit(f"thanm 解 {path} 失败：\n{r.stderr[:400] or r.stdout[:400]}")
    return r.stdout


def sheet_path(kind: str, name: str) -> Path:
    """源图（TH08 = `anm/<anm名>/data/player00/<anm名>.png`；TH16 = `anm/pl03/player/pl03.png`）。"""
    if kind == "th08":
        return TH08_ANM / name / "data" / "player00" / f"{name}.png"
    # TH16：`pl03sub.anm` 的贴图在 `anm/pl03sub/player/pl03/pl03.png`（**与 pl03.anm 各一份**，
    # 别混用 —— 两边的 sprite 号空间不同）
    return TH16_ANM / name / "player" / "pl03" / "pl03.png"


def entry_rects(text: str, name: str):
    """该 anm 里 **entry0** 的 sprite 矩形 + **`yOffset`**（子弹都在 entry0）。

    ★★★ 2026-09-26（owner：「妖机体的弹幕样式应该都不对」→ 复核发现**早苗那四张是白方块**）：
    **`yOffset` 必须加进裁图原点**。一手（`thanm -l 16 -m v8.anmm th16.v1.00a/dat/pl03sub.anm`）：
    ```
    entry entry0 { name: "player/pl03/pl03.png", width: 256, height: 64, yOffset: 176, THTXHeight: 64 }
    entry entry1 { name: "player/pl03/pl03.png", width: 128, height: 16, yOffset: 144, THTXHeight: 16 }
    ```
    两个 entry **共用同一个贴图名**，各自只是那张大盘里的一段 ⇒ 引擎按 `yOffset` 定位，
    而 `thanm -x` 解出来的是**整段大盘**（`player/pl03/pl03.png` = 256×240）：
      · entry0 的 256×64 在 **y = 176..240**（= 那四个「月牙」= script0 的 sprite0..3 真身）；
      · 大盘 y = 0..176 是**空白**。
    旧版按 `y = 矩形.y`（= 0）裁 ⇒ 裁到空白 ⇒ 四张全白、`vis` 还是 4096（不透明白）
    ⇒ 旧自检「`vis == 0` 才算空图」抓不到（见下面新增的「白图」判据）。
    **其它源的 `yOffset` 全是 0**（2026-09-26 实测：TH08 `player00/01/02`、`ascii`、`etama`、
    TH16 `pl03`、TH18 `pl0X` 全表都没有非零 `yOffset`）⇒ 只有 `pl03sub` 吃这一条。"""
    m = re.search(r"entry entry0 \{(.*?)^\}", text, re.S | re.M)
    if not m:
        raise SystemExit(f"{name}: 找不到 entry0")
    body = m.group(1)
    yo = re.search(r"yOffset:\s*(-?\d+)", body)
    y_off = int(yo.group(1)) if yo else 0
    rects = {int(x.group(1)): (int(x.group(2)), int(x.group(3)) + y_off, int(x.group(4)), int(x.group(5)))
             for x in _SPRITE_DEF_RE.finditer(body)}
    return rects, y_off


def extract_script(text: str, n: int):
    m = re.search(rf"^script script{n} \{{.*?^\}}$", text, re.S | re.M)
    if not m:
        raise SystemExit(f"anm 里找不到 script{n}")
    return m.group(0)


def pow2_ceil(v):
    p = 1
    while p < v:
        p <<= 1
    return p


def canvas_size(w, h):
    return pow2_ceil(w), pow2_ceil(h)


# ★★★ 2026-09-27（owner：「**先搞滚动的**」）：**可无缝平铺**的成员。
#   一手 TH08 的束脚本有 `scrollX(0.0625f)`（每帧滚 1/16 宽），而那张条状纹理是**一次性**的
#   （左右边缘接不上）⇒ 滚到接缝会在束上留一条移动的缝（owner 实跑「一节一节消失」）。
#   这里把指定成员改成**镜像平铺**：`[内容 | 内容镜像]` ⇒ 新图**左右边缘像素相同**
#   （右端 = 镜像段的末尾 = 内容的第 0 列；左端 = 内容的第 0 列）⇒ UV 绕回时**无缝** ✓。
#   代价：贴图宽翻倍 ⇒ 卡侧的 `YK_M_LASER_BEAM_SPRITE_W` 要跟着改（`448/512 = 0.875` 的拉伸），
#   成员在 entry 里也占更宽的位置（`assets/build_player_anm.py` 的 `YK_BULLET_CANVAS`）。
MIRROR_MEMBERS = {
    "YKB_M_OPT",      # 魔理沙妖侧子机的束（唯一一条需要滚动的贴图）
}


def mirror_tile(canvas):
    """把画布变成「内容 + 自身镜像」⇒ 左右边缘像素相同、可无缝滚动。"""
    from PIL import Image as _I
    w, h = canvas.size
    out = _I.new("RGBA", (w * 2, h), (0, 0, 0, 0))
    out.paste(canvas, (0, 0))
    out.paste(canvas.transpose(_I.FLIP_LEFT_RIGHT), (w, 0))
    return out


def cut_sprites(workdir: Path):
    """把每个成员 1:1 切出来、居中补到 2 的幂画布。→ {(名字): (路径, WxH, 可见像素, 源字节)}"""
    OUT_ART.mkdir(parents=True, exist_ok=True)
    made = {}
    for name, kind, anm, sid, crop in MEMBERS:
        text = read_anm(kind, anm, workdir)
        rects, y_off = entry_rects(text, anm)
        if sid not in rects:
            raise SystemExit(f"{anm}: 没有 sprite{sid}（源 ank 变了？）")
        x, y, w, h = rects[sid]
        if crop:                                        # ★ 只对「滚动纹理」那一张裁（见 MEMBERS 的注）
            cx, cy, cw_, ch_ = crop
            if cx + cw_ > w or cy + ch_ > h:
                raise SystemExit(f"{anm} sprite{sid}: 裁剪框 {crop} 超出 {w}x{h}")
            x, y, w, h = x + cx, y + cy, cw_, ch_
        im = Image.open(sheet_path(kind, anm)).convert("RGBA")
        cell = im.crop((x, y, x + w, y + h))
        cw, ch = canvas_size(w, h)
        canvas = Image.new("RGBA", (cw, ch), (0, 0, 0, 0))
        canvas.paste(cell, ((cw - w) // 2, (ch - h) // 2))     # 居中 = 原生的中心锚点
        if name in MIRROR_MEMBERS:                      # ★ 可无缝平铺（见 MIRROR_MEMBERS 的注）
            canvas = mirror_tile(canvas)
            cw, ch = canvas.size
        path = OUT_ART / f"{name}.png"
        canvas.save(path)
        # 用**逐通道**展平（`get_flattened_data` 不回元组、也不触发 Pillow 的弃用告警）。
        a_ch = list(canvas.getchannel("A").get_flattened_data())
        rgb_ch = [list(canvas.getchannel(c).get_flattened_data()) for c in ("R", "G", "B")]
        vis = sum(1 for a in a_ch if a > 32)
        # ★★★ 2026-09-26（`pl03sub` 那个坑的**防复发判据**）：裁到「整块不透明白」= 源矩形落在了
        #   大盘的空白段（`yOffset` 没加、或加错）—— 旧的 `vis == 0` 判据对它无效（白图 vis 满格）。
        ink = sum(1 for i, a in enumerate(a_ch)
                  if a > 32 and (rgb_ch[0][i], rgb_ch[1][i], rgb_ch[2][i]) != (255, 255, 255))
        if vis > 0 and ink == 0:
            raise SystemExit(f"{name}: 切出来是**整块白**（{cw}x{ch}，{vis} 个不透明像素、0 个非白）"
                             f" —— 源矩形 ({x},{y},{w},{h}) 大概落在源图空白段："
                             f"{anm} 的 entry0 yOffset={y_off}（本函数已把它加进 y）")
        made[name] = (path, (cw, ch), vis, cell.tobytes(), ink)
    return made


def _translate(src_lines, sprite_map, drop_log):
    """v2/v16 → v8 的逐行翻译（三个动作，见 `gen_script` 的注）。命中支与弹体支共用。"""
    out = []
    for line in src_lines:
        s = line.strip()
        m25 = _INS25_RE.match(s)
        if m25:                                        # ★ TH08 `ins_25(N)` ≡ TH18 `rotateAuto(N)`
            out.append(f"    rotateAuto({m25.group(1)});")
            continue
        m = _DROP_RE.match(s)
        if m:
            indent = line[:len(line) - len(line.lstrip())]
            drop_log.append(s)
            out.append(f"{indent}// ★ 原生原文此处为 `{s}` —— TH18 的 anm v8 没有这个助记符 ⇒ 省略")
            continue
        out.append(_SPRITE_CALL_RE.sub(lambda mm: f"sprite(@{sprite_map[int(mm.group(1))]}@)", line))
    return out


def gen_script(name: str, kind: str, anm: str, src_num: int, sids, note: str, sprite_map: dict,
               hit=None, hit_map=None, workdir=None) -> str:
    """逐条照抄 TH08/TH16 那一支；只做三件事（换 sprite 名 / 补 type(1) / 省掉 v8 没有的助记符）。

    ★ `hit = (命中脚本号, [命中 sprite…])`：TH08 的命中脚本（行 `+0x24` + **11**）翻成
      TH18 的 `interruptLabel(1)` 支（`None` = 源脚本自带那一支，不重复加）。见 `SCRIPTS` 的注。"""
    workdir = workdir or Path(tempfile.mkdtemp(prefix="ykbullet."))
    anm_text = read_anm(kind, anm, workdir)
    src = extract_script(anm_text, src_num)
    out, dropped = [], []
    for line in src.splitlines():
        s = line.strip()
        if s.startswith("script script"):
            out.append(f"script script{SCRIPT0 + [x[0] for x in SCRIPTS].index(name)} {{")
            out += [
                f"    // 由 assets/ability/make_youkai_bullet_art.py 从 "
                f"{'TH08 v1.00d' if kind == 'th08' else 'TH16 v1.00a'} `{anm}` script{src_num} 生成，别手改。",
                f"    // {note}。",
                "    // 指令逐条照抄（段序/时序/数值一字未动），只做四件事：",
                "    //   ① `sprite(N)` → `sprite(@名字@)`；② 补 `type(1)`（TH18 v8 的**渲染模式**：不写就落进",
                "    //   模式 0 = 绘制定格 ⇒ `AUDIT.md` §BE #95）；③ `ins_25(N)` → `rotateAuto(N)`",
                "    //   （TH08 的「按 VM 旋转画」开关 ⇔ TH18 的 `rotateAuto`，一手见 `_DROP_RE` 上面那段）；",
                "    //   ④ 省掉 v8 里确实没有的助记符（下面逐条点名）。",
                "    // ★ 为什么补 `layer(13)`：原脚本没有（TH08 那代由 VM 自带层），TH18 的原生弹脚本都显式写",
                "    //   （`ability.anm` script21 起手 `layer(13)`）⇒ 照原生那一支写。",
                "    layer(13);",
                "    type(1);",
            ]
            continue
        if s == "}":
            # ★ 命中支插在收尾 `}` **之前**（形状照 TH18 原生弹脚本：冻结 → interruptLabel(1) → delete）。
            if hit is not None:
                hit_num, hit_sids = hit
                out += [
                    "    // ★★★ 2026-09-27：**命中支** = TH08 的命中脚本（行 `+0x24` + 11）逐条照抄。",
                    "    //   为什么必须有（owner：「弹打到敌人之后不消失、匀速慢速上升」）：TH18 的默认命中",
                    "    //   路径（`th18:0x45f6a0` → `0x45f6f0`）只做「给弹的 VM 发 interrupt(1)」＋",
                    "    //   `bullet+0x8c = 2` ＋ `bullet+0x60 *= 0.125`；**VM 里没有 interrupt 支时**",
                    "    //   弹既不消失、又只剩 1/8 速度地继续飞（`AUDIT.md` §AL31 是同一类；",
                    "    //   形状照 TH18 原生弹脚本 `pl00.anm` script5..11）。",
                    "    stop();",
                    "    interruptLabel(1);",
                    "    layer(15);",
                    "    type(1);",
                ]
                hbody = extract_script(anm_text, hit_num).splitlines()
                # ⚠ **标签注释要改成绝对帧**：命中支接在弹体支之后，而 `thanm` 解回来的文本里
                #   标签注释是**绝对帧**（`+10: // 10010`）⇒ 直接抄源脚本的 `// 10` 会被
                #   `build_player_anm.py` 的「编出来 ↔ 计划」逐字符闸当场拦下（实测踩过一次）。
                #   基准 = 弹体支最后一个标签的绝对帧（我们这几支都是 `+10000`）。
                _base = 0
                for _l in out:
                    _m = re.match(r"^\+(\d+):\s*//\s*(\d+)\s*$", _l.strip())
                    if _m: _base = int(_m.group(2))
                for _l in _translate(hbody[1:-1], hit_map or sprite_map, dropped):
                    _m = re.match(r"^(\s*)\+?(\d+):\s*//\s*(\d+)\s*$", _l)
                    if _m:
                        out.append(f"{_m.group(1)}+{_m.group(2)}: // {_base + int(_m.group(3))}")
                    else:
                        out.append(_l)
                for hs in hit_sids or ():
                    if not any(f"@{sprite_map[hs]}@" in l for l in out):
                        raise SystemExit(f"{name}: 命中支里没有引用 sprite{hs}")
            out.append(line)
            continue
        out += _translate([line], sprite_map, dropped)
    for sid in sids:
        if not any(f"@{sprite_map[sid]}@" in l for l in out):
            raise SystemExit(f"{name}: 产物里没有引用 sprite{sid}（sprite 表或源脚本对不上）")
    return "\n".join(out) + "\n"


def pack_atlas(made):
    """把成员按「货架」摆进一张 2 的幂画布里 → {名字: (x, y, w, h)}, (W, H)。

    ⚠ 成员画布高度不一（TH08 的 16/32、TH16 的 64）⇒ **必须多行**，而且每行按高度分组
    （第一版写成单行 512x64，生成器自己在 `write_png_row` 里报错拦下了 —— 那道自检留着）。
    """
    rows, maxw = [], 512
    cur, curw, curh = [], 0, 0
    for name, _k, _a, _s, _c in MEMBERS:
        w, h = made[name][1]
        if cur and (curw + w > maxw or h > curh):
            rows.append((cur, curw, curh))
            cur, curw, curh = [], 0, 0
        cur.append(name)
        curw += w
        curh = max(curh, h)
    if cur:
        rows.append((cur, curw, curh))
    place, y = {}, 0
    for names, _w, h in rows:
        x = 0
        for n in names:
            w, hh = made[n][1]
            place[n] = (x, y + (h - hh) // 2, w, hh)      # 行内垂直居中
            x += w
        y += h
    W = pow2_ceil(maxw if maxw else 1)
    H = pow2_ceil(y)
    return place, (W, H)


def write_layout(made):
    """把图集布局写成 `assets/yk_bullet/LAYOUT.txt`（**纯文档**：`build_ability.py` 不再读它）。

    旧版这里产出的是要 append 进 `assets/ability/entries/ORDER.txt` 的片段 —— 2026-09-26 第十三批
    把那 10 个成员从 ability 图集里撤掉了（真正需要它们的是四个 `pl0X.anm` 的新 entry，
    由 `build_player_anm.py` 的 `YK_BULLET_MEMBERS` / `YK_BULLET_CANVAS` 重新摆放）。
    """
    place, (W, H) = pack_atlas(made)
    lines = [
        "# 卡 108 妖侧弹的成员图（`assets/yk_bullet/*.png`）+ 它们的来源与摆放。",
        "# 由 assets/ability/make_youkai_bullet_art.py 生成（幂等，整段重写）；**纯文档**。",
        "# 一手：弹体脚本 = 行里 `+0x22` 的 anm 脚本，按 **pl0X.anm** 解析（`+0x24`/`+10` 那条是 TH08 的算法）；",
        "#   霊夢 s14/s16（sprite35/36）· 魔理沙 s22/s24（sprite30/sprite32）· 咲夜 s12/s14（sprite22/23）·",
        "#   早苗 = TH16 `pl03sub.anm` script0（sprite0..3）。",
        f"# 本文件里那张 512x128 的排布只是「切图时用过的画布」；真正落进 pl0X.anm 的摆放见",
        f"#   assets/build_player_anm.py 的 YK_BULLET_CANVAS（每机体一张小画布）。",
        f"YkBULLET_LAYOUT {W}x{H}",
    ]
    for name, _kind, _anm, _sid, _crop in MEMBERS:
        x, y, w, h = place[name]
        lines.append(f"{name:<17} {name}.png   @{x},{y},{w}x{h}")
    text = "\n".join(lines) + "\n"
    (OUT_ART / "LAYOUT.txt").write_text(text, encoding="utf-8")
    return text, (W, H)


def check_compile(scripts):
    """逐支用 thanm 编一遍（v8）——保证产物不是「写出来编不过」。"""
    with tempfile.TemporaryDirectory(prefix="ykbulletchk.") as d:
        d = Path(d)
        Image.new("RGBA", (32, 32), (255, 255, 255, 255)).save(d / "x.png")
        entry = (f"entry entry0 {{\n    version: 8,\n    name: \"x.png\",\n    format: 1,\n"
                 f"    width: 32,\n    height: 32,\n    sprites: {{ sprite0: {{ x: 0, y: 0, w: 16, h: 16 }} }}\n}}\n")
        ok = 0
        for name, text in scripts.items():
            body = _NAME_RE.sub("sprite0", text)
            (d / "spec.txt").write_text(entry + "\n" + body, encoding="utf-8")
            # ⚠ thanm 的 `-c` 用法与 `-l` 不同（先给 out、再给 in，且要 `-m`）——
            #   照 `make_youkai_option_art.py` 的 check_compile 那条实测过的写法。
            r = subprocess.run([str(THANM), "-c", "18", "out.anm", "spec.txt", "-m", str(ANMMAP_V8)],
                               cwd=str(d), capture_output=True, text=True)
            if r.returncode != 0:
                raise SystemExit(f"{name} 编不过：{r.stderr[:300]}")
            ok += 1
        return ok


# ★★★ 2026-09-27（owner ⑥：「**魔理沙妖子机的激光是真激光，无限贯通，会随着 P 的增加而变宽**」）：
#   魔理沙（TH08 `ply01as` = アリス）那一侧的**子机行**每档只有 1 行、`type = 5`，四个回调
#   = init `th08:0x44fe20` / tick `th08:0x450840` / draw `th08:0x450ad0` / hit `th08:0x450ee0`
#   ⇒ **不是一颗弹，是一条长驻的激光束 VM**（钉在子机上、每帧重画 + 32 点历史轨迹做伤害盒）。
#   一手（子 agent 逐指令）：`type = 5` 在碰撞 pass 里被**显式跳过销毁**（`th08:0x4518b3` 一族）
#   ⇒ **无限贯通、每 2 帧结算一次**；宽度**逐档靠不同号的 anm 脚本**（`player01.anm` script24..28 的
#   `scale(1.0, 0.5/0.6/0.7/0.8/1.0)`，贴图 sprite32 = 448×32）⇒ 屏上宽度 16/19.2/22.4/25.6/32 px；
#   长度恒 **448**、判定盒 24×448、伤害 7（`row+0x1c`）。
#
# **TH18 侧**（第二十六批**改写**）：**引擎**那一行不再发任何东西（实跑定案：`mode=2` 在主炮路径下
#   一出生就被判完成；117 在 TH17 上有同一结论 —— 见 `cards/animal.c:283`-`300`），
#   束改由**卡侧**画（`cards/youkai.c` 的 `yk_beam_tick`）+ 卡侧结算（`ce_damage_rect`）。
#   ⇒ 本卡把 5 支**束脚本**追加进 `pl01.anm`（卡侧从 `yk_player_anm()` = `pl01.anm` 起脚本，
#     贴图 `YKB_M_OPT` 就长在那个文件的图集里），形状见 `gen_beam` 的 docstring：
#     ★ 第三十一批起**正文逐行照抄 117 的 `script163`**（渲染壳、`rotate` 初值、`scrollX` 的
#     `fset` 随机相位 + −1/21 全照它）；只有「宽度逐档」与「层号 14」是本卡自己的口径。
#   ⚠ 逐档宽度仍然逐档一支脚本（下列表），但**宽度值由卡侧写**（`YK_M_LASER_BEAM_W_PX100`）；
#     脚本之间只差 `scale(1.0f, <逐档>)` 那一行，五支都留着是为了「行里的 `+0x22` 逐档映射」与卡侧下标一一对应。
BEAMS = [
    # (名字, 屏上目标宽度 px, scale.y = 目标宽度 / 贴图高 32 —— 一手值，现由卡侧表执行)
    ("YKB_M_BEAM0", 16.0, 0.5),
    ("YKB_M_BEAM1", 16.0, 0.5),
    ("YKB_M_BEAM2", 19.2, 0.6),
    ("YKB_M_BEAM3", 22.4, 0.7),
    ("YKB_M_BEAM4", 25.6, 0.8),
]


def gen_beam(name, spr_member, width_px, scale_y, src_num):
    """生成一支**束脚本**（v8）—— ★★★ 2026-09-27 第三十一批：**正文逐行照抄 117 的束脚本**
    `assets/ability/scripts/163_eagle_beam.anm.txt`（它自己抄的是 TH17 `pl01.anm` script16）。

    **为什么改成「照抄」**：owner 的口径是「**问题依旧是，117 的激光也是向外流动的，你是怎么弄成这个样子的**」
    ⇒ 他要的就是 117 那个形状。此前两批我把「从外向内」分别归给**锚点**（`TUNING.md` §2.1ccc-9：把
    `anchor(1, 0)` 改成 `anchor(0, 0)`）与**滚动符号**（§2.1ccc-10），两次都没对。与 163 逐行比对后
    看到的**客观差别**只有一条：**163 把 `rotate(0.0f, 0.0f, -1.5707964f)` 烘进了脚本，我们一个角度都没写**
    （`scrollX` 的形态也不同：163 是「`fset` 随机起始相位 + `+1:` 起每帧 `-1/21`」）。
    本批不再推理成因，只把**正文对齐**成 163 的同形；能不能让 owner 满意由实跑判。

    逐行对照（左 = 163，右 = 本支）：

    | 行 | 163 | 本支 | 说明 |
    | --- | --- | --- | --- |
    | `layer(...)` | 13 | **14** | 层号是各卡自己的口径（本卡 owner 定序：自机弹 13 < 束 14 < 妖侧子机 15，§2.1ccc-7）|
    | `sprite` / `type(1)` | 同 | 同 | 逐字 |
    | `anchor(1, 0)` | 同 | 同 | **左端锚**（`engine/anm/th16/10` §2：`h=1` ⇒ 四角 x = `0,+1,0,+1`）⇒ 图左端贴 VM、本地 +x 往外伸 |
    | `blendMode(1)` / `alpha(176)` | 同 | 同 | alpha 也照 163 取 **176**（不再用我们自算的 160）|
    | **`rotate(0.0f, 0.0f, -1.5707964f)`** | 有 | **第三十一批补上** | ★ 这就是那两批缺的那一行 |
    | `scale(1.0f, X)` | `4.0` | **逐档** | 本卡宽度逐档（一手 TH08 `player01.anm` script24..28 的 0.5/0.6/0.7/0.8）；x 由卡侧每帧写（`2048/512 = 4.0` × 展开系数）|
    | `fset(%F0, %RANDF); scrollX(%F0);` | 有 | ★ **第三十二批删掉** | **差异⑤之一**：一手的 `rate` 份副本是**同一支 VM**（`th08:0x450ad0`）⇒ 共享相位；我们的副本各是一支 VM，随机相位会互相平均掉 ⇒ 纹样被抹平（见 `cards/youkai_core.h` 的「每条残影的 UV 相位必须相同」）|
    | `+1: scrollX(-0.04761905f);` | 同 | **`+1: scrollX(-0.02083333f);`** | **差异⑤之二**：163 的 −1/21 是**按 163 自己 512 宽的图集**归一化的；我们的 entry 是 **768** 宽 ⇒ 照字面搬会快 1.5 倍、且比一手快 2.29 倍（1.75 帧/纹样周期 = 超奈奎斯特的频闪）。本支取回**一手的 16 texel/帧**（换算链见 `youkai_core.h` 的同名长注）|
    | `stop(); interruptLabel(1); scaleTime(…); +N: delete();` | 6 帧 | **8 帧** | 收束段 = `YK_M_LASER_BEAM_GROW_FRAMES`（「如收回般展开」的倒放，§2.1ccc-4 ❓）|

    ⚠ **仍不写 `rotateAuto`**：那是「朝**运动方向**」的绘制位（`flags_hi` bit7）—— 我们这支 VM 是
      **没有速度的世界 VM**，方向由卡侧每帧 `ce_anm_set_rotation` 写（117 的 `eagle_beam_tick` 同款：
      bake 一个 −90° 当初值、每帧再覆盖）。"""
    return f"""script script{{N}} {{
    // 由 assets/ability/make_youkai_bullet_art.py 生成（{name}），别手改。
    // 魔理沙（TH08 `ply01as` = アリス）妖侧**子机激光的束脚本**（owner ⑥；第二十六批改成卡侧 VM 专用，
    // 第二十七批起同一帧摆 `rate` 条残影 —— 见 `cards/youkai.c` 的 `yk_beam_tick`）。
    // 几何 = TH08 `player01.anm` script{src_num}（`scale(1.0f, {scale_y}f)` ⇒ 屏上宽度 ≈ {width_px} px，
    //   贴图高 32）；**其余每一句照抄 117 的 `163_eagle_beam.anm.txt`**（第三十一批起，见下）。
    // ⚠ 不写 `rotateAuto`（那是给有速度的弹用的「朝运动方向」位）；长度/宽度每帧由卡侧写
    //   （`cards/youkai.c` 的 `yk_beam_tick`）。
    // ★★★ 2026-09-27 第三十一批：**正文逐行照抄 117 的 `163_eagle_beam.anm.txt`**
    //   （owner：「117 的激光也是向外流动的，你是怎么弄成这个样子的」）——
    //   `anchor` / `rotate` 初值 / `alpha` 全照它。三处是本卡自己的口径：
    //   ① `layer(14)`（层序见下）；② `scale(1.0f, {scale_y}f)` 的**逐档宽度**（一手 TH08）
    //   —— 117 那里两轴共用它自己的 SCALE 4.0；③ ★ **第三十二批**：`scrollX` 的**随机相位删掉**、
    //   流速换成一手那一档（**差异⑤**，理由见下与 `cards/youkai_core.h` 的同名长注）。
    // ★ 2026-09-27 第二十七批定的层序 / 第二十八批订正：`layer(13)` → **`layer(14)`**。
    //   owner 的最终口径（`TUNING.md` §2.1ccc-7）：**自机弹 13 < 束 14 < 妖侧子机 15** ——
    //   我们那三支妖侧子机脚本显式写 `layer(15)` 把 14 让给束。一手：自机弹 VM 层 = 13
    //   （零售 `pl01.anm` 弹体脚本逐条 `layer(13)`）、子机 VM 层 = 14（`th18:0x40a8bc`）。
    //   ⚠ **第二十七批那次改漏了本文件**（`youkai.c` 的 `YK_BEAM_ANM_LAYER` 改成 14、这里仍是 13）
    //   ⇒ 构建期闸 `check_beam_script_constants` 当场报红，但**那个生成器不在 `make check` 的路径上**
    //   ⇒ 实际部署出去的 `pl01.anm` 里还是 13（束与自机弹同层）。现在两道闸：
    //   本文件的 `check_beam_script_constants()` + 宿主机单测
    //   `native/tests/test_youkai_core.c` 的 `check_beam_spec_files()`（后者跑在 `make check` 里）。
    layer(14);
    sprite(@{spr_member}@);
    type(1);
    // ★★★ 2026-09-27 第三十一批：**照抄 163**。`anchor(1, 0)` = **左端锚**
    //   （`engine/anm/th16/10` §2 的 x 系数表：`h=1` ⇒ 四角 x = `0,+1,0,+1` ⇒ 原点在图的**左端**、
    //   图朝本地 +x 铺）⇒ 配合卡侧的 −π/2 覆盖，VM 落在束的**近端**、远端朝屏幕上方。
    //   ⚠ 命名订正：本文件第二十九/三十批把这两个值叫反了（`anchor(1,0)` 写成「右端锚」、
    //     `anchor(0,0)` 写成「左端锚」）。按 §2 的表 `h=0` 是**居中**（四角 x = `−0.5,+0.5,−0.5,+0.5`）。
    //     行为一个字都没改（照抄 163 就对了），这里只把名字对齐一手。
    anchor(1, 0);
    blendMode(1);
    // alpha 也照 163 取 **176**（此前是我们自算的 160）。卡侧 `YK_M_LASER_BEAM_ALPHA` 同步改 176；
    //   「越旧越淡」的逐条 alpha 仍由卡侧按一手 `0x450ad0` 算（`yk_m_laser_beam_bar_alpha`）。
    alpha(176);
    // ★★★ **这一行就是本卡前两批缺的那一行**：163 把 −90° 烘进脚本当初值，我们此前一个角度都不写
    //   （注释里还写着「角度也不 bake」）⇒ 束的初值与 117 不同。现在逐字照抄；
    //   卡侧每帧的 `ce_anm_set_rotation`（5 条扇形 + 残影）仍然覆盖它 —— 117 的 `eagle_beam_tick`
    //   也是「bake 初值 + 每帧覆盖」这个形状。
    rotate(0.0f, 0.0f, -1.5707964f);
    // 宽度初值 = 逐档（一手 TH08 `player01.anm` script{src_num} 的 `scale.y`）；
    //   x 由卡侧每帧写（`YK_LASER_LEN 2048 / 512 = 4.0` × 展开系数 —— 与 163 的 `4.0` 同数）。
    scale(1.0f, {scale_y}f);
    // ★★★ 贴图**往外流**：`+1:` 起每帧一个**定值**流速（v8 opcode 425 `uVel` ⇒ `uv_scroll_vel.x`，
    //   单位 = 一整幅图集宽；一手的 opcode 80 同理 —— 换算链见 `cards/youkai_core.h` 的
    //   `YK_M_LASER_BEAM_SCROLL` 长注）。
    //   ★ 第三十二批：**删掉 163 的 `fset(%F0, %RANDF); scrollX(%F0);`（随机起始相位）** ——
    //   一手 draw 把**同一支 VM** 画 `rate` 次（`th08:0x450ad0` 的循环里只改位置与 alpha、
    //   **UV 从不重算**）⇒ 一条束的 `rate` 份副本**共享同一个相位**、彼此只差位置；
    //   而我们的副本是**各自一支 VM**，12 个随机相位会互相**平均**掉 ⇒ 纹样（64 texel 的菱形格）
    //   被抹平，看上去是一条光滑亮条、看不出贴图在流（owner 第三十二批原话：
    //   「至于贴图本身，我实际看不太清它到底是不是往外发射」）。
    //   163 那句是给 117 的 **5 条独立束**去相关的；我们这 12 份是**叠在同一条线上**的副本。
    //   ⚠ `+8: // 9` 的那个 **9 是 thanm 反解出来的绝对时间**（`+1:` 起的时间轴 ⇒ 1 + 8）；
    //   `build_player_anm.py` 的 `_verify_appended` 把「计划文本 vs 编出来再反解的文本」**逐字符**比
    //   （只归一化 `offsetNN`）⇒ 这个注释必须与 thanm 打印的一模一样，否则 `make anm` 当场红。
+1: // 1
    scrollX(-0.02083333f);
    stop();
    interruptLabel(1);
    scaleTime(8, 1, 1.0f, 0.0f);
+8: // 9
    delete();
}}
"""


# ★★★ 2026-09-27 第二十七批：**束脚本的壳常量必须与卡侧对账**（构建期闸）。
#   为什么加它：owner 报的「图依旧在一节一节长出」的**根因**正是「脚本里的 `scrollX` 与贴图宽度脱钩」——
#   上一批把成员从 256 镜像平铺成 512，却没把步长减半 ⇒ 屏上流速翻倍、变成每帧半个钻石周期的**频闪**。
#   这类「两个文件各写一个数」的漂移构建期看不出来（脚本能编过、anm 校验也过）⇒ 这里现读卡侧的
#   `YK_M_LASER_BEAM_SCROLL`（`youkai_core.h`）与 `YK_BEAM_ANM_LAYER`（`youkai.c`）逐字比对。
#   ★ 第三十一批扩容：连「照抄 163 的 6 句」（`blendMode` / `type` / `rotate` / `+1:` …）一起钉 ——
#   `rotate(0.0f, 0.0f, -1.5707964f)` 从来没进过脚本，而前两批**没有任何闸看得见它**。
#   ★★ 第三十二批：**`fset(%F0, %RANDF); scrollX(%F0);` 从「必须有」翻成「必须没有」** ——
#   一手的 `rate` 份副本是同一支 VM（共享 UV 相位），我们的副本各是一支 VM ⇒ 随机相位会把
#   12 份纹样平均掉（看不出贴图在流）。同时流速按**一手的 16 texel/帧**重算（entry 宽 768）。
YOUKAI_CORE_H = REPO / "mods/th18.v1.00a/card-expand/native/cards/youkai_core.h"
YOUKAI_C = REPO / "mods/th18.v1.00a/card-expand/native/cards/youkai.c"


def _read_define(path: Path, name: str) -> str:
    m = re.search(rf"^#define\s+{re.escape(name)}\s+([^\s/]+)", path.read_text(encoding="utf-8"), re.M)
    if not m:
        raise SystemExit(f"{path.name}: 找不到 `#define {name} …` —— 它与束脚本必须成对（改了名字两边一起改）")
    return m.group(1)


def _live_body(body: str) -> str:
    """脚本正文的**非注释行**（`//` 起头的行不算）。

    ★ 第三十二批：反向闸（「不许出现 `%RANDF`」）必须只看正文 —— 我们的注释里**会写到**它
    （正是为了讲清「为什么不写它」），拿 `strstr` 扫全文会把自己的说明当成违规
    （本批实测：第一版闸就是这么红掉的）。正反两向都只认正文，闸才真的钉在指令上。"""
    return "\n".join(ln for ln in body.splitlines() if not ln.lstrip().startswith("//"))


def check_beam_script_constants(scripts: dict):
    scroll = _read_define(YOUKAI_CORE_H, "YK_M_LASER_BEAM_SCROLL")
    layer = _read_define(YOUKAI_C, "YK_BEAM_ANM_LAYER")
    alpha = _read_define(YOUKAI_CORE_H, "YK_M_LASER_BEAM_ALPHA")
    # ★ 第三十一批：除了三个卡侧常量，再把「照抄 163」的那几句也钉住 —— 上一批漏的就是这一类的行
    #   （`rotate(…)` 从来没进过脚本，两批都没人看得见）。
    # ★★ 第三十二批：**去掉** `fset(%F0, %RANDF);` / `scrollX(%F0);` 两句，并改成**反向闸**
    #   （脚本正文里出现 `%RANDF` 就是回归到了「12 个随机相位互相平均掉」那个形态）。
    want = (f"scrollX(-{scroll});", f"layer({layer});", f"alpha({alpha});", "anchor(1, 0);",
            "blendMode(1);", "type(1);", "rotate(0.0f, 0.0f, -1.5707964f);", "+1: // 1")
    for k, (_bname, _w, _y) in enumerate(BEAMS):
        num = SCRIPT0 + len(SCRIPTS) + k
        name = f"YKB_M_BEAM{k}"
        body = _live_body(scripts[name].replace("{N}", str(num)))
        for line in want + (f"scale(1.0f, {_y}f);",):
            if line not in body:
                raise SystemExit(f"{name}: 脚本正文里没有 `{line}` —— 与 `youkai_core.h`/`youkai.c` 的"
                                 f"同名常量脱钩了，或没照 163 的同形写（这正是「一节一节」那个 bug 的形状）")
        for bad in ("%RANDF", "scrollX(%F0)"):
            if bad in body:
                raise SystemExit(f"{name}: 脚本正文里还有 `{bad}` —— 一手的 `rate` 份残影是**同一支 VM**"
                                 f"（共享 UV 相位），我们的副本各是一支 VM ⇒ 随机相位会把 12 份纹样"
                                 f"平均掉（owner 报的「看不出贴图往外发射」）。见 `TUNING.md` §2.1ccc-12")
    print(f"== 对账：束脚本的壳常量 == 卡侧（`scrollX({scroll})` / `layer({layer})` / `alpha({alpha})`）"
          f"+ 照抄 163 的形状；**无随机相位**（第三十二批）✓")


def main():
    workdir = Path(tempfile.mkdtemp(prefix="ykbullet."))
    made = cut_sprites(workdir)
    print("== 切图（1:1，居中补到 2 的幂）")
    for name, (path, (w, h), vis, _cell, ink) in made.items():
        if vis == 0:
            raise SystemExit(f"{name}: 切出来是空图（源矩形错了？）")
        print(f"   {name:<12} {w}x{h}  可见像素 {vis:<6} 非白 {ink:<6} ← {path.relative_to(HERE.parent)}")

    # sprite → ORDER 成员名（按 **(作品, anm 名, sprite 号)** 查；早苗那支一个脚本用 4 张图，
    #   四张各有自己的成员名 ⇒ 不能用「脚本名 + 序号」现编 —— 名字必须与 ORDER 片段逐字一致，
    #   否则 `make anm` 会在占位符替换那一步报「引用了不存在的成员」。）
    member_of = {(k, a, sid): nm for nm, k, a, sid, _c in MEMBERS}
    scripts = {}
    for name, kind, anm, src_num, sids, note, hit in SCRIPTS:
        smap = {}
        for sid in list(sids) + (list(hit[1]) if hit else []):
            key = (kind, anm, sid)
            if key not in member_of:
                raise SystemExit(f"{name}: sprite{sid} 没有对应的 ORDER 成员（MEMBERS 要补一行）")
            smap[sid] = member_of[key]
        scripts[name] = gen_script(name, kind, anm, src_num, sids, note, smap, hit=hit, hit_map=smap)

    OUT_SCRIPTS.mkdir(parents=True, exist_ok=True)
    # ★ 2026-09-27（owner ⑥）：5 支**束脚本**（魔理沙的激光），编号接在 SCRIPTS 之后。
    for k, (bname, bwidth, by) in enumerate(BEAMS):
        num = SCRIPT0 + len(SCRIPTS) + k
        # 前两档在 TH08 里共用 script24 ⇒ 源脚本号 = 24 + max(k-1, 0)
        scripts[bname] = gen_beam(bname, "YKB_M_OPT", bwidth, by, 24 + max(k - 1, 0))
        scripts[bname] = scripts[bname].replace("{N}", str(num))
        (OUT_SCRIPTS / f"{num}_yk_bullet_{bname.lower()}.anm.txt").write_text(
            scripts[bname].replace("{N}", str(num)), encoding="utf-8")
    want = {f"{SCRIPT0 + i}_yk_bullet_{name.lower()}.anm.txt" for i, (name, *_r) in enumerate(SCRIPTS)}
    want |= {f"{SCRIPT0 + len(SCRIPTS) + k}_yk_bullet_{b.lower()}.anm.txt"
             for k, (b, _w, _y) in enumerate(BEAMS)}
    for i, (name, *_rest) in enumerate(SCRIPTS):
        fname = f"{SCRIPT0 + i}_yk_bullet_{name.lower()}.anm.txt"
        (OUT_SCRIPTS / fname).write_text(scripts[name], encoding="utf-8")
    # ★★★ 2026-09-27：**清掉上一批留下的 spec** —— 文件名带序号（`370_yk_bullet_<名字>.anm.txt`），
    #   而 `build_player_anm.py` 是按**序号** glob 的（`{spec_file}_yk_bullet_*.anm.txt` 必须唯一）。
    #   本批把 `YKB_A_0` 换成 `YKB_A_BODY` 时实测踩到：旧的 `370_…_a_0.anm.txt` 留在盘上 ⇒
    #   同号两个文件 ⇒ `_yk_bullet_block()` 报「找不到唯一的弹体 spec」。成员 PNG 同理。
    stale = sorted(p for p in OUT_SCRIPTS.glob(f"{SCRIPT0}_yk_bullet_*.anm.txt") if p.name not in want)
    stale += sorted(p for p in OUT_SCRIPTS.glob("*_yk_bullet_*.anm.txt")
                    if p.name not in want and p not in stale)
    stale += sorted(p for p in OUT_ART.glob("YKB_*.png") if p.name not in {f"{n}.png" for n, *_ in MEMBERS})
    for p in stale:
        p.unlink()
    if stale:
        print(f"== 清掉上一批的 {len(stale)} 个过期产物：" + ", ".join(p.name for p in stale))
    print(f"== 脚本 {len(SCRIPTS)} 支 + 束 {len(BEAMS)} 支 → {OUT_SCRIPTS}"
          f"（编号 {SCRIPT0}..{SCRIPT0 + len(SCRIPTS) + len(BEAMS) - 1}）")

    order, (AW, AH) = write_layout(made)
    used = sum(made[n][1][0] * made[n][1][1] for n, *_ in MEMBERS)
    print(f"== 布局说明 → {(OUT_ART / 'LAYOUT.txt').relative_to(HERE.parent)}（切图画布 {AW}x{AH}；成员总面积 {used} ✓）")
    print(f"   ⚠ 这批 spec 的真正落地处 = 四个 pl0X.anm（`build_player_anm.py` 的 bullet 追加件读 {OUT_SCRIPTS.name}/）")

    # ★ 2026-09-26 第十三批：**不再有「与 ability 主表撞名」这一步** —— 成员图与 spec 都挪出了
    #   ability 管线（见文件头与 `LAYOUT.txt` 的注），撞名与否已无关。
    ok = check_compile({k: v.replace("{N}", str(SCRIPT0)) for k, v in scripts.items()})
    print(f"== 自检：{ok}/{len(scripts)} 支脚本能编过（thanm -c 18）；名字无重复；空图 0")
    check_beam_script_constants(scripts)
    print(f"== md5（幂等判据）：{hashlib.md5((order + ''.join(scripts.values())).encode()).hexdigest()}")
    print("== 下一步：`make -C native anm`（ability.anm 不再含这批；pl0X.anm 会带上它们）")


if __name__ == "__main__":
    main()
