#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""make_trance_bgm.py —— 生成 **卡 113「灵界版 BGM」** 的 WAV 素材（TH18 自己的曲子 + 低音放大）。

# 这是什么

用户 2026-09-13 的口径（原话）：「**用纯 naive 的方式，把 th18 现有 6 面 + Ex 面的曲子，把低音放大，
生成那么多 wav，然后塞进去当灵界**」。

对应 TH13 原版的做法：TH13 的 `thbgm.fmt` 里每首道中曲都另有一条 **`th13_NNb.wav`** 变体
（22050 Hz、intro/loop 秒数与正常轨**逐一严格相等**，见
`engine/_shared/th13-th18-trance-bgm-research.md` §1.5-C / §2.2）；灵界进出就是
「把当前曲名的扩展名前一位改成 `b`，再入队 `cmd 9` 换轨」（`th13:0x405b1d`）。

TH18 的数据里**没有** b 变体（fmt 实测 18 条全是 `th18_NN` / `th128_08`，没有 `NNb`），
且 TH18 的 `cmd 9` 是死代码 ⇒ 本仓走**路线 A**：不改 `thbgm.dat` / `th18.dat`，
在 DLL 侧用引擎自己的内存流构造成器（`th18:0x4898f0`）另开**第二路 `CStreamingSound`**，
与主 BGM 交叉淡化（接线在 `native/cards/trance_bgm.c`）。
所以「塞进去」= **把生成的 WAV 放到 DLL 旁边的 `bgm/` 目录**（本地步骤，见本文末 §部署）。

# 一手依据（全部本机可复核）

## 1. 曲目表：stage → 用哪条 fmt 记录

`thbgm.fmt` 记录 = thtk `bgm_fmt_t`，**0x34 字节**（`local/vendor/thtk/extlib/thtypes/bgm_types.h`）：

```
+0x00 fn[16]              曲名（NUL 终止；fn[0]==0 = 表尾哨兵）
+0x10 track_offset        文件内偏移（thbgm.dat；第一条 0x10 = 跳过 16 B 的 "ZWAV" 头）
+0x14 preload_size        ★ 实测 18 条**逐条等于 track_size**
+0x18 intro_size          ★ **循环点**（= intro 段的字节数）
+0x1c track_size          ★ **读取窗口长度 = 整条轨的总字节数（含 intro）**
+0x20 WAVEFORMATEX(18B) + pad[2]
```

★ 关于 `track_size` 的**口径订正**（一手实测，与本工具直接相关）：
`offset[N+1] - offset[N] == track_size[N]` **逐条成立**（例：记录 0 `track_offset=16`、
`track_size=19403296`，记录 1 的 `track_offset=19403312` = 16 + 19403296）
⇒ **`track_size` 含 `intro_size`**，循环段字节数 = `track_size - intro_size`。
（引擎侧的同一个口径：读取窗口 `[base, base+track_size)`、回卷点 `base+intro_size` ——
`th18:0x48acc0` 的 preload 分支，逐条见 `trance_bgm_core.h` 的文件头。）

**stage → 曲名**的映射有**两条互相独立的一手来源**，本工具**都读、并断言它们一致**：

1. **exe 的关卡表**（`th18.exe`，imagebase `0x400000`）：表基址 `0x4c9410`、步长 `0xd4`、
   索引 = 关卡号；`CUR_STAGE_DATA` 指针在 `0x4cf428`。条目布局（逐字段本机实测）：

   | 偏移 | 含义 | 证据 |
   | --- | --- | --- |
   | `+0x00` | 关卡号（`0` 是残留条目、`7` = Extra）| 用它的指令：`imul eax,eax,0xd4; add eax,0x4c9410`（`th18:0x444d40`、`0x455242`、`0x46501d` 等 9 处）|
   | `+0x04` | `st0N.std` 名 | 表里指向 `st01.std`…`st07.std` 的字符串 |
   | `+0x08` | `st0N.ecl` 名 | 同上 |
   | `+0x0c` | **道中曲名** | `th18:0x443121` `push [eax+0xc]` → `0x454620(slot 0)` |
   | `+0x10` | **boss 曲名** | `th18:0x443130` `push [eax+0x10]` → `0x454620(slot 1)` |

   即「关卡开始时引擎自己就把这两条名字塞进 BGM 槽」——这是 TH18 版 `stage → bgm` 的**活代码**证据。
2. **`musiccmt.txt`（音乐室作者注释，游戏自己的数据）**：每条 `@bgm/th18_NN` 的正文写着
   「１面のテーマです。」「２面のテーマです。」…「最終面のテーマです。」「エキストラステージのテーマです。」
   —— 与 (1) 的表**逐条相符**。

## 2. 生成什么

对每条目标轨：从 `thbgm.dat` 切出**循环段**（`[intro_size, track_size)`），做**纯 naive 的低音放大**，
（默认）降到 22050 Hz 写成 16 bit 立体声 WAV，文件名 `th18_NNb.wav`（**TH13 的原命名法**）。

### DSP（naive，逐条写清，全部 ❓ 自定 —— 见 `AUDIT.md` §CO / `TUNING.md`）

1. **循环段**：丢掉 intro、只留循环体。理由：第二路从字节 0 起播（引擎内存流 `cur = base + intro_size`
   才是回卷点，而不是起点），只留循环体 ⇒ 「文件里第 0 字节 = 循环起点 = 回卷点」，两头一致，
   **不需要去改引擎流对象的内部读指针**。代价：灵界版**没有**那段 intro（听感 = 直接从主歌起）。
2. **低音提取**：两段滑动平均级联（= 三角窗 FIR，`--taps` 默认 **132** 抽头/段 ⇒ 实测 −3 dB ≈ **107 Hz**；
   用 `cumsum` 实现，O(n)）。`bass = box(box(x, N), N)`。
   （抽头数的实测对照见 `AUDIT.md` §CO：66 抽头 ⇒ 低频只 +3.8 dB、中高 −3.6 dB；
   132 ⇒ +6.0 / −1.7；200 ⇒ +8.2 / −0.2 —— 抽头越长，放大越集中在"真低音"上、
   越不必靠压中高频来换余量。）
3. **放大**：`y = x + g·bass`，`g = 10^(--gain-db/20)`（默认 **+9 dB**）。
4. **整轨峰值归一**：若 `peak(y) > headroom`（默认 0.97 FS）⇒ 全轨乘 `headroom/peak`。
   **不做**压缩/限幅器 —— 增益越大，整轨（含中高频）被压得越多，听感是「低频更厚」而不是「整体更响」。
5. **降采样**：2 帧一平均再抽取（2 抽头 box = 轻微抗混叠，**不是**窗 sinc）。默认 22050 Hz = TH13 的 b 轨采样率。
6. int16 量化（四舍五入 + 钳位保险，正常路径不该触发）。

工具会把**实测的滤波器 −3 dB 频点**、原/新峰值与 RMS、低频占比、实际放大倍数**打印成表**，
—— 这些数字就是 `AUDIT.md` §CO 表里那些值的出处。

# ★★ 版权纪律

生成物是**对 ZUN 版权音频的派生作品**（直接切片 + 滤波）⇒ **只能待在 `local/`（gitignored）**，
**永远不许入库**，也不进 `dist/`。工具在写盘前用 `git check-ignore` 自查，写进被跟踪的路径就**拒绝**。

# 用法

**默认生成 14 首**（7 道中 + 7 关底，用户 2026-09-14 口径「BGM 得包括 7 首道中和 7 首关底」）；
只想要道中曲用 `--set stage`。**任何一条超过 DLL 的 `TRANCE_BGM_MAX_PCM` 都会让整批生成失败**
（生成期红，不留到运行期静默降级）；容量不够时的两条出路见 `--help` 与本文件末。

```bash
# 0) 先解出 thbgm.fmt（若 local/th18.v1.00a/dat/ 里还没有）
M=/mnt/d/项目/幻想指南/18/th18_modkit
local/vendor/thtk/build/thdat/thdat -x 18 "$M/game/th18.dat" thbgm.fmt

# 1) 生成（默认 = 14 首：6 面 + Ex 的**道中曲 + 关底曲**）
python3 tools/make_trance_bgm.py                 # 从仓库根跑也行：加 --root
python3 tools/make_trance_bgm.py --list          # 只打印映射表
python3 tools/make_trance_bgm.py --set stage     # 只要 7 首道中曲
python3 tools/make_trance_bgm.py --verify-only   # 只校验已生成的文件（尺寸/格式/md5）
python3 tools/make_trance_bgm.py --gain-db 12    # 更狠的低音
python3 tools/make_trance_bgm.py --measure       # 生成 + 独立回读量带内能量
```

默认路径（可用 `--root` 换仓库根）：

| 输入 | 默认 |
| --- | --- |
| fmt | `local/th18.v1.00a/dat/thbgm.fmt` |
| PCM | `local/th18.v1.00a/thbgm.dat` |
| exe（映射对账用，可缺）| `local/th18.v1.00a/th18.exe` |
| 输出 | `local/th18.v1.00a/trance_bgm/` |

依赖：`numpy`（本仓 `assets/` 下已有多个脚本用它）。
"""

from __future__ import annotations

import argparse
import hashlib
import os
import re
import struct
import subprocess
import sys
import wave

try:
    import numpy as np
except ImportError:  # pragma: no cover - 环境问题，直接说清怎么补
    sys.exit("需要 numpy（pip install numpy）——本仓 assets/ 下的美术脚本同样依赖它")

# ---------------------------------------------------------------------------
# 常量
# ---------------------------------------------------------------------------

FMT_RECORD = 0x34            # thbgm.fmt 单条记录（thtk bgm_fmt_t）

# ★ stage → 曲名（道中曲）。**索引 = exe 关卡表 0x4c9410 + N*0xd4 的 +0x0c 槽**，逐条见文件头 §1。
#   这不是手抄：`check_exe_table()` 会去读 exe 的同一张表，对不上就**拒绝生成**。
STAGE_THEME = {
    1: "th18_03", 2: "th18_05", 3: "th18_07", 4: "th18_09",
    5: "th18_11", 6: "th18_13", 7: "th18_17",          # 7 = Extra（st07）
}
# ★ boss 曲 = 同一张表的 +0x10 槽（`setBoss` 之后引擎自己切过去）。默认**不生成**（用户口径只要道中曲）。
STAGE_BOSS = {
    1: "th18_04", 2: "th18_06", 3: "th18_08", 4: "th18_10",
    5: "th18_12", 6: "th18_14", 7: "th18_18",
}
# `--set all` 时额外补的非关卡曲：标题 / 结局 / 制作名单 / 音乐室那条 128 曲。
EXTRA_THEMES = ["th18_01", "th18_15", "th18_16", "th128_08"]

SETS = {
    "stage": list(STAGE_THEME.values()),
    "stage+boss": list(STAGE_THEME.values()) + list(STAGE_BOSS.values()),
    "all": list(STAGE_THEME.values()) + list(STAGE_BOSS.values()) + EXTRA_THEMES,
}
# ★ 默认 = `stage+boss`（**14 首**：7 道中 + 7 关底）。用户 2026-09-14 口径：
#   「**BGM 得包括 7 首道中和 7 首关底**」—— 关底战期间引擎把 BGM 切到 boss 曲（一手 `th18:0x443130`），
#   只生道中曲的话 boss 战里进灵界会走"素材不在 ⇒ 不改 BGM"（首日实跑就是这么被看出来的）。
DEFAULT_SET = "stage+boss"

EXE_TABLE_BASE = 0x4c9410     # ★ 关卡表基址（一手：9 处 `imul eax,eax,0xd4; add eax,0x4c9410`）
EXE_TABLE_STRIDE = 0xd4
EXE_TABLE_MAX = 8             # 0..7（0 = 残留条目，7 = Extra）

# DLL 侧单个 PCM 缓冲的上限：`native/cards/trance_bgm_core.h` 的 `TRANCE_BGM_MAX_PCM`。
# ★ 这份只是**头文件读不到时**的兜底值；正常情况下 `check_dll_cap()` 会去读头文件并以它为准
#   （两份不一致时打印提示并采用头文件的值 —— 单一真相在头文件）。
DLL_MAX_PCM = 24 * 1024 * 1024

DEFAULT_RATE = 22050          # TH13 的 b 轨采样率（一手 fmt 实测）
DEFAULT_GAIN_DB = 9.0
DEFAULT_TAPS = 132            # 每段滑动平均的抽头数（级联两段 ⇒ 三角窗；−3 dB ≈ 107 Hz @44100）

# ★★ 第二条链：**照量出来的 TH13 倾斜做**（`--dsp th13-tilt`；默认仍是上面那条"低音货架"）。
#    数字出处 = `tools/measure_th13_b_variants.py` 对 TH13 13 对的实测**聚合**（相对倾斜，dB）：
#      <150 Hz **+1.33**（0/13 对在降 ⇒ 唯一一个"全都抬"的带）、
#      500–2k **−3.04**（**13/13 对都在降** ⇒ 实测里唯一系统性成立的规则）、
#      2k–6k **−1.76**（10/13 在降）、150–500 −0.62（10/13 在降）、>6k +1.02（只有 4/13 在降 ⇒ 太杂，不抄）。
#    只抄**方向一致**的三条带（+1.33 / −3.04 / −1.76）；`mid/side` 平均只 −0.88 dB（13 对里只有 3 对 < −2 dB）
#    ⇒ **不抄 karaoke 式的"削中间"**（实测不支持）。完整判据与反面证据见 `AUDIT.md` §CO-10。
#   形制：(种类, 频带下界 Hz, 频带上界 Hz, 相对增益 dB)
#   ★ 只抄**方向一致**的两条带 ⇒ 两条判据写在这里，免得日后有人凭感觉加带：
#     · `<150 Hz`：13 对里 **0 对在降**（+0.20…+2.73）⇒ 抄 +1.33（均值）
#     · `500–2k`：13 对里 **13 对都在降**（−0.73…−5.99）⇒ 抄 −3.04（均值）
#   ⚠ **没有抄** `2k–6k`（均值 −1.76，但只有 10/13 在降、跨 −4.7…+2.3）与 `150–500`（10/13、−0.62）：
#     方向不够一致（"每对都这样"才叫规则），而且滑平均搭出来的"带"边沿很糊、根本落不到那个频段上
#     （实测：硬加 −1.76 dB 的 2k–6k 后，回读只到 −1.2…+0.8 dB ⇒ 是**假的精确**，不抄）。
#   `>6k`（+1.02，只有 4/13 在降）同理不抄。
TH13_TILT = [("lowpass",    0.0,  150.0, +1.33),   # <150 Hz 抬（13/13 一致）
             ("band",     500.0, 2000.0, -3.04)]   # 500–2k 压（13/13 一致 —— 实测里唯一系统性的规则）


# ---------------------------------------------------------------------------
# fmt / PCM
# ---------------------------------------------------------------------------

class Track:
    __slots__ = ("index", "name", "offset", "preload", "intro", "size",
                 "rate", "channels", "bits", "block", "avg", "tag")

    @property
    def loop_bytes(self) -> int:
        """循环段字节数 = track_size - intro_size（★ track_size **含** intro，见文件头 §1）。"""
        return self.size - self.intro

    @property
    def seconds(self) -> float:
        return self.size / self.avg if self.avg else 0.0

    @property
    def loop_seconds(self) -> float:
        return self.loop_bytes / self.avg if self.avg else 0.0


def parse_fmt(path: str) -> list[Track]:
    data = open(path, "rb").read()
    tracks, off, i = [], 0, 0
    while off + FMT_RECORD <= len(data) and data[off]:
        name = data[off:off + 16].split(b"\0")[0].decode("latin1")
        to, pre, intro, size = struct.unpack_from("<IIiI", data, off + 16)
        tag, ch, rate, avg, ba, bits = struct.unpack_from("<HHIIHH", data, off + 32)
        t = Track()
        t.index, t.name, t.offset, t.preload, t.intro, t.size = i, name, to, pre, intro, size
        t.tag, t.channels, t.rate, t.avg, t.block, t.bits = tag, ch, rate, avg, ba, bits
        tracks.append(t)
        off += FMT_RECORD
        i += 1
    return tracks


def track_by_stem(tracks: list[Track], stem: str) -> Track:
    for t in tracks:
        if t.name.split(".")[0] == stem:
            return t
    raise KeyError(stem)


def slice_loop(buf_mm, path: str, t: Track) -> bytes:
    """切出**循环段**（`[intro, size)`）—— 见文件头 §2 第 1 条。"""
    if t.intro < 0 or t.size <= 0 or t.intro >= t.size:
        raise ValueError(f"{t.name}: intro={t.intro} track={t.size} 不自洽")
    if (t.loop_bytes % t.block) != 0:
        raise ValueError(f"{t.name}: 循环段 {t.loop_bytes} 不是 block({t.block}) 的整数倍")
    with open(path, "rb") as f:
        f.seek(t.offset + t.intro)
        raw = f.read(t.loop_bytes)
    if len(raw) != t.loop_bytes:
        raise ValueError(f"{t.name}: 只读到 {len(raw)} / {t.loop_bytes} 字节（{path} 被截短了？）")
    return raw


# ---------------------------------------------------------------------------
# exec 关卡表对账（把「映射」变成机器可查，而不是手抄）
# ---------------------------------------------------------------------------

def _pe_sections(exe: bytes):
    pe = struct.unpack_from("<I", exe, 0x3C)[0]
    if exe[pe:pe + 4] != b"PE\0\0":
        raise ValueError("不是 PE")
    nsec = struct.unpack_from("<H", exe, pe + 6)[0]
    optsz = struct.unpack_from("<H", exe, pe + 20)[0]
    imgbase = struct.unpack_from("<I", exe, pe + 24 + 28)[0]
    secs = []
    for i in range(nsec):
        o = pe + 24 + optsz + i * 40
        vsz, va, rsz, raw = struct.unpack_from("<IIII", exe, o + 8)
        secs.append((imgbase + va, max(vsz, rsz), raw))
    return imgbase, secs


def check_exe_table(exe_path: str, verbose: bool = True) -> dict[int, tuple[str, str]]:
    """读 exe 的关卡表，返回 {stage: (道中曲, boss 曲)}；文件不在就返回 {}（不致命）。"""
    if not os.path.exists(exe_path):
        print(f"  ! 关卡表对账跳过：没有 {exe_path}")
        return {}
    exe = open(exe_path, "rb").read()
    imgbase, secs = _pe_sections(exe)

    def v2f(va: int):
        for base, size, raw in secs:
            if base <= va < base + size:
                return raw + (va - base)
        return None

    def cstr(va: int) -> str:
        fo = v2f(va)
        if fo is None:
            return "<越界>"
        end = exe.find(b"\0", fo, fo + 64)
        return exe[fo:end].decode("latin1")

    out = {}
    for stage in range(EXE_TABLE_MAX):
        fo = v2f(EXE_TABLE_BASE + stage * EXE_TABLE_STRIDE)   # ★ 表基址是**绝对 VA**（0x4c9410）
        if fo is None:
            raise ValueError("关卡表不在 .data 里（版本变了？）")
        num, p_std, p_ecl, p_bgm0, p_bgm1 = struct.unpack_from("<5I", exe, fo)
        out[num] = (cstr(p_bgm0).split(".")[0], cstr(p_bgm1).split(".")[0])
        if verbose:
            print(f"  exe 表[索引 {stage}] stage={num:2d} {cstr(p_std):10s} {cstr(p_ecl):10s} "
                  f"道中={cstr(p_bgm0):10s} boss={cstr(p_bgm1)}")
    return out


def assert_mapping(table: dict[int, tuple[str, str]]) -> None:
    """把本文件的硬编码映射与 exe 的表逐条比 —— 对不上就报错退出（绝不静默生成错的曲）。"""
    if not table:
        return
    bad = []
    for stage, want in STAGE_THEME.items():
        got = table.get(stage, ("?", "?"))[0]
        if got != want:
            bad.append(f"stage {stage}: 表里是 {got}，本文件写的是 {want}")
    for stage, want in STAGE_BOSS.items():
        got = table.get(stage, ("?", "?"))[1]
        if got != want:
            bad.append(f"stage {stage} boss: 表里是 {got}，本文件写的是 {want}")
    if bad:
        raise SystemExit("★ 映射对账失败（exe 的关卡表 ≠ 本文件的表）：\n  " + "\n  ".join(bad))
    print(f"  OK 映射对账：{len(STAGE_THEME)} 条道中曲 + {len(STAGE_BOSS)} 条 boss 曲与 exe 表逐条一致")


def check_dll_cap(header_path: str, verbose: bool = True) -> int:
    """读 DLL 侧头文件里的 `TRANCE_BGM_MAX_PCM`，与本文件的 DLL_MAX_PCM 对账。"""
    if not os.path.exists(header_path):
        print(f"  ! DLL 上限对账跳过：没有 {header_path}")
        return DLL_MAX_PCM
    m = re.search(r"#define\s+TRANCE_BGM_MAX_PCM\s+([^\n/]+)", open(header_path, encoding="utf-8").read())
    nums = re.findall(r"(\d+)\s*u?\b", m.group(1)) if m else []
    if not m or not nums:
        print(f"  ! DLL 上限对账跳过：{header_path} 里找不到 TRANCE_BGM_MAX_PCM")
        return DLL_MAX_PCM
    val = 1
    for v in nums:                       # `(16u * 1024u * 1024u)` ⇒ 逐个数相乘
        val *= int(v)
    if val != DLL_MAX_PCM:
        print(f"  ★ 注意：DLL 侧上限 = {val} B，本文件写的是 {DLL_MAX_PCM} B ⇒ 以 DLL 为准")
    elif verbose:
        print(f"  OK DLL 上限对账：TRANCE_BGM_MAX_PCM = {val} B")
    return val


# ---------------------------------------------------------------------------
# DSP（naive；全部 ❓ 自定，理由逐条在文件头 §2）
# ---------------------------------------------------------------------------

def box_filter(x, n: int):
    """因果滑动平均（n 抽头，沿 axis 0 = 帧轴），O(n)：前 n-1 帧用**部分窗**（免得开头塌一块）。

    `x` 可以是 1-D（单声道）或 2-D `[frames, ch]`（立体声）—— 逐声道独立滤波。"""
    if n <= 1 or x.shape[0] == 0:
        return x
    frames = x.shape[0]
    tail = x.shape[1:]
    zero = np.zeros((1,) + tail)
    c = np.concatenate((zero, np.cumsum(x, axis=0, dtype=np.float64)), axis=0)
    out = np.empty_like(c[:-1])
    k = min(n - 1, frames)
    scale = np.arange(1, k + 1).astype(np.float64).reshape((-1,) + (1,) * len(tail))
    out[:k] = (c[1:k + 1] - c[0]) / scale
    if frames > k:
        out[k:] = (c[n:] - c[:frames - k]) / n
    return out


def box_response_db(taps: int, fs: int, freqs, stages: int = 2):
    """`stages` 段 box 级联的幅频响应（dB）—— 用来**实测**报告 −3 dB 频点，不是估计。

    `stages=2`（三角窗）是默认链 `--dsp bass` 用的；`--dsp th13-tilt` 用 `stages=3`
    （滚降更陡 ⇒ 搭出来的"带"漏得少，见那份 DSP 的注释）。"""
    h = np.ones(taps) / taps
    hh = h
    for _ in range(max(1, stages) - 1):
        hh = np.convolve(hh, h)
    resp = np.abs(np.fft.rfft(hh, 16384))
    f = np.fft.rfftfreq(16384, 1.0 / fs)
    mag = np.interp(freqs, f, resp)
    return 20.0 * np.log10(np.maximum(mag, 1e-12))


def measure_cutoff_hz(taps: int, fs: int, stages: int = 2) -> float:
    """级联 box 的 −3 dB 频点（数值求，取报告用）。"""
    f = np.linspace(1.0, fs / 2.0, 20000)
    db = box_response_db(taps, fs, f, stages)
    below = np.where(db <= -3.0)[0]
    return float(f[below[0]]) if below.size else float("nan")


_TAPS_CACHE: dict = {}


def taps_for_cutoff(fs: int, hz: float, stages: int = 2, lo: int = 2, hi: int = 4096) -> int:
    """反解"两段 box 级联"的抽头数，使其 −3 dB 频点 ≈ `hz`（数值搜索；结果缓存）。

    用实测响应（`measure_cutoff_hz`）而不是套公式 —— 与生成时真正跑的滤波器是同一份实现。"""
    key = (fs, round(hz, 3), stages)
    if key in _TAPS_CACHE:
        return _TAPS_CACHE[key]
    while lo < hi:                                   # 抽头越多 ⇒ 截止越低（单调）
        mid = (lo + hi) // 2
        if measure_cutoff_hz(mid, fs, stages) > hz:
            lo = mid + 1
        else:
            hi = mid
    _TAPS_CACHE[key] = lo
    return lo


def boost_naive(x, fs: int, gain_db: float, taps: int):
    """`y = x + g * box(box(x))`，返回 (y, bass)。x 是 float64 的 [frames] 或 [frames, ch]。"""
    bass = box_filter(box_filter(x, taps), taps)
    g = 10.0 ** (gain_db / 20.0)
    return x + g * bass, bass


# ---- RBJ 双二阶（"照实测倾斜"这条链用它，因为滑平均搭的带**实现不了干净的带选择性**）----
#   ★ 为什么不用滑平均级联：第一版就是那么写的，回读显示"减 3 dB 的 500–2k 带"只到 −0.4…−2.7 dB，
#     还把 150–500 一起拖低 −4…−6.5 dB（三角窗的边沿太糊、相邻带互相漏）⇒ **假的精确**，不写进产物。
#   RBJ 的公式是公开教科书级的东西（Audio EQ Cookbook），这里照抄系数式，用**截断冲激响应 + FFT 卷积**
#   落地（离线工具，没有实时约束；纯 Python 逐样本递归 7M 样本太慢，所以走卷积）。
def _rbj_peaking(fs: float, f0: float, q: float, db: float):
    """RBJ peaking EQ 的 (b, a)。"""
    A = 10.0 ** (db / 40.0)
    w0 = 2.0 * np.pi * f0 / fs
    alpha = np.sin(w0) / (2.0 * q)
    cw = np.cos(w0)
    b = [1 + alpha * A, -2 * cw, 1 - alpha * A]
    a = [1 + alpha / A, -2 * cw, 1 - alpha / A]
    return [v / a[0] for v in b], [v / a[0] for v in a]


def _rbj_lowshelf(fs: float, f0: float, db: float, slope: float = 1.0):
    """RBJ low-shelf 的 (b, a)。"""
    A = 10.0 ** (db / 40.0)
    w0 = 2.0 * np.pi * f0 / fs
    cw, sw = np.cos(w0), np.sin(w0)
    alpha = sw / 2.0 * np.sqrt((A + 1 / A) * (1 / slope - 1) + 2)
    tsa = 2 * np.sqrt(A) * alpha
    b = [A * ((A + 1) - (A - 1) * cw + tsa), 2 * A * ((A - 1) - (A + 1) * cw),
         A * ((A + 1) - (A - 1) * cw - tsa)]
    a = [(A + 1) + (A - 1) * cw + tsa, -2 * ((A - 1) + (A + 1) * cw),
         (A + 1) + (A - 1) * cw - tsa]
    return [v / a[0] for v in b], [v / a[0] for v in a]


def _fir_from_iir(b, a, n: int = 8192):
    """把 (b, a) 的**冲激响应**前 `n` 个样本算出来（对 δ 逐样本递推 `n` 次，一次性、可忽略）。

    ⚠ 别把 `b1/b2` 乘到**输出** `h` 上 —— 2026-09-14 第一版就是这么写错的：那样 b≈a 的窄带滤波器
    会退化成近似全通（残差只剩 1e−5 量级），而"没效果"在回读里长得像"倾斜本来就是 0"，
    是**最容易被误读成"实测不支持"的那种 bug**。自检：`h.sum()` 必须等于该滤波器的**直流增益**。"""
    x = np.zeros(n)
    x[0] = 1.0
    h = np.zeros(n)
    for i in range(n):
        v = b[0] * x[i]
        if i >= 1:
            v += b[1] * x[i - 1] - a[1] * h[i - 1]
        if i >= 2:
            v += b[2] * x[i - 2] - a[2] * h[i - 2]
        h[i] = v
    return h


def _apply_fir(x, h):
    """用 FFT 做线性卷积（逐列），只取前 `len(x)` 个样本（`mode='same'` 的因果版）。"""
    n = x.shape[0] + h.shape[0] - 1
    N = 1 << int(np.ceil(np.log2(n)))
    H = np.fft.rfft(h, N)
    X = np.fft.rfft(x, N, axis=0)
    y = np.fft.irfft(X * (H[:, None] if x.ndim > 1 else H), n=N, axis=0)
    return y[:x.shape[0]]


def tilt_th13(x, fs: int, spec=None, stages: int = 3):
    """`--dsp th13-tilt`：照 TH13 实测的三条带做**朴素**倾斜（滑平均级联搭的带通，O(n)）。

        low(<150 Hz)    = box(box(x, N150))
        b1(500–2k)      = box(box(x, N2k)) − box(box(x, N500))
        b2(2k–6k)       = box(box(x, N6k)) − box(box(x, N2k))
        y = x + g_low·low + g_b1·b1 + g_b2·b2        （g = 10^(dB/20) − 1）

    返回 (y, 实际三条带的 −3 dB 频点 dict)。"""
    y = x.copy()
    used = {}
    for kind, lo, hi, db in (spec or TH13_TILT):
        if kind == "lowpass":                          # 低架（"低频抬"那一类）
            b, a = _rbj_lowshelf(fs, hi, db)
            used[f"shelf {hi:g}Hz"] = db
        else:                                          # 峰值滤波（"中频压"那一类；中心 = 几何中点）
            b, a = _rbj_peaking(fs, float(np.sqrt(max(lo, 1.0) * hi)), 0.7, db)
            used[f"peak {lo:g}-{hi:g}Hz"] = db
        y = _apply_fir(y, _fir_from_iir(b, a))
    return y, used


MEASURE_EDGES = [150.0, 500.0, 2000.0, 6000.0]
MEASURE_NAMES = ["<150", "150-500", "500-2k", "2k-6k", ">6k"]


def measure_band_index(lo: float, hi: float) -> int:
    """把 [lo, hi) 这条"我们做的带"映射到 `MEASURE_NAMES` 的那一条（按下界中点）。"""
    mid = (lo + hi) / 2.0 if lo > 0 else hi / 2.0
    for i, edge in enumerate(MEASURE_EDGES):
        if mid < edge:
            return i
    return len(MEASURE_EDGES)


def band_energies_db(x, fs: float):
    """把 `x` 按 `MEASURE_EDGES` 切成 5 条**互补**带（FFT 掩码，和 = 原信号），返回各带能量 dB。

    ★ 这 5 条带与 `tools/measure_th13_b_variants.py` 用的是**同一套** ⇒ 我们的产物与 TH13 的实测倾斜可直接对表。"""
    spec = np.fft.rfft(x)
    fr = np.fft.rfftfreq(x.shape[0], 1.0 / fs)
    out, lo = [], 0.0
    for hi in MEASURE_EDGES + [fs / 2.0]:
        m = (fr >= lo) & (fr < hi)
        sp = np.zeros_like(spec)
        sp[m] = spec[m]
        out.append(10.0 * np.log10(max(1e-12, float(np.mean(np.fft.irfft(sp, n=x.shape[0]) ** 2)))))
        lo = hi
    return out


def rms_db(x) -> float:
    if x.size == 0:
        return float("-inf")
    r = float(np.sqrt(np.mean(np.square(x, dtype=np.float64))))
    return 20.0 * np.log10(r / 32768.0) if r > 0 else float("-inf")


def peak_db(x) -> float:
    p = float(np.max(np.abs(x))) if x.size else 0.0
    return 20.0 * np.log10(p / 32768.0) if p > 0 else float("-inf")


# ---------------------------------------------------------------------------
# 一首曲子的转换
# ---------------------------------------------------------------------------

def convert(raw: bytes, t: Track, rate_out: int, gain_db: float, taps: int, headroom: float,
            norm: str = "peak", dsp: str = "bass"):
    """返回 (pcm_int16_bytes, rate_out, channels, stats dict)。"""
    if t.bits != 16 or t.tag != 1:
        raise ValueError(f"{t.name}: 只支持 16 bit PCM（实测 TH18 全表都是），本轨 tag={t.tag} bits={t.bits}")
    ch = t.channels
    frames = np.frombuffer(raw, dtype="<i2").reshape(-1, ch).astype(np.float64)
    src_peak, src_rms = peak_db(frames), rms_db(frames)

    calib = None
    if dsp == "th13-tilt":
        # ★ 照实测倾斜（不乘 --gain-db：那三条带的值就是实测值）。
        #   ⚠ 但**峰值归一**会把一部分倾斜吃掉（它给全轨加一个公共缩放）⇒ 单跑一次到不了目标，
        #   所以这里做 3 轮"量误差 → 改带增益"的小校准（`--dsp bass` 那条链没有这个问题）。
        spec = [list(t) for t in TH13_TILT]
        base_e = band_energies_db(frames.mean(axis=1), t.rate)
        # 把每条"我们做的带"映射到 5 带口径里的那一条（按下界中点，别写死下标 ⇒ 加减带都对）
        idx = [measure_band_index(lo, hi) if kind == "band" else measure_band_index(0.0, hi)
               for kind, lo, hi, _db in spec]
        target = [0.0] * len(MEASURE_NAMES)
        for j, bi in enumerate(idx):
            target[bi] = spec[j][3]
        for _ in range(4):
            y, _cuts = tilt_th13(frames, t.rate, spec)
            ypk = float(np.max(np.abs(y))) if y.size else 0.0
            sc = min(1.0, (headroom * 32767.0) / ypk) if (norm != "none" and ypk > 0) else 1.0
            ym = (y * sc).mean(axis=1)
            e = band_energies_db(ym, t.rate)
            lvl = 20.0 * np.log10(float(np.sqrt(np.mean(ym ** 2))) / max(1e-12, float(np.sqrt(np.mean(frames.mean(axis=1) ** 2)))))
            got = [(e[i] - lvl) - base_e[i] for i in range(5)]
            calib = got
            for j, bi in enumerate(idx):
                spec[j][3] += (target[bi] - got[bi])                      # 线性误差回灌（4 轮足够收敛）
        y, _cuts = tilt_th13(frames, t.rate, spec)
        bass = box_filter(box_filter(frames, taps), taps)   # 只为统计"低音占比"用
    else:
        y, bass = boost_naive(frames, t.rate, gain_db, taps)
    # ★ 这里统计的是"放大后、归一前"的 RMS 增益（归一与降采样都还没发生）——
    #   归一那一步单独记 `norm_db`，两者相加 = 整轨净响度变化。
    rms_x = float(np.sqrt(np.mean(np.square(frames, dtype=np.float64))))
    rms_y = float(np.sqrt(np.mean(np.square(y, dtype=np.float64))))
    boost_db = 20.0 * np.log10(rms_y / max(1e-12, rms_x))

    scale = 1.0
    ypeak = float(np.max(np.abs(y))) if y.size else 0.0
    if norm == "none":
        pass                                    # 故意不归一（试听用；削顶数会在表里报出来）
    elif ypeak > headroom * 32767.0:
        scale = headroom * 32767.0 / ypeak
        y = y * scale
    norm_db = 20.0 * np.log10(scale) if scale > 0 else float("-inf")

    # 降采样：factor 帧一平均再抽取（2 抽头 box = 轻微抗混叠，不是窗 sinc）
    factor = max(1, int(round(t.rate / rate_out))) if rate_out else 1
    if factor > 1:
        keep = (y.shape[0] // factor) * factor
        y = y[:keep].reshape(-1, factor, ch).mean(axis=1)
    rate = t.rate // factor

    q = np.rint(y)
    clipped = int(np.count_nonzero((q > 32767.0) | (q < -32768.0)))
    q = np.clip(q, -32768.0, 32767.0).astype("<i2")

    low_ratio_src = float(np.sqrt(np.mean(np.square(bass, dtype=np.float64)))) / \
        max(1e-12, float(np.sqrt(np.mean(np.square(frames, dtype=np.float64)))))
    stats = dict(rate=rate, rate_src=t.rate, factor=factor, frames_in=frames.shape[0],
                 calib=calib,
                 frames_out=q.shape[0], src_peak_db=src_peak, src_rms_db=src_rms,
                 norm_db=norm_db, boost_db=boost_db, low_share=low_ratio_src, clipped=clipped,
                 secs_in=t.loop_bytes / t.avg, secs_out=q.shape[0] / rate)
    return q.tobytes(), rate, ch, stats


def predicted_out_bytes(t: Track, rate_out: int) -> int:
    """不跑 DSP 也能算准输出字节数：`factor` 帧一平均再抽取，帧数向下取整到 factor 的整数倍。"""
    factor = max(1, int(round(t.rate / rate_out))) if rate_out else 1
    frames = t.loop_bytes // t.block
    return (frames // factor) * t.block


def write_wav(path: str, pcm: bytes, rate: int, channels: int) -> None:
    """写 16 bit PCM WAV（stdlib `wave`；不写任何额外 chunk —— 引擎只认 fmt/data）。"""
    with wave.open(path, "wb") as w:
        w.setnchannels(channels)
        w.setsampwidth(2)
        w.setframerate(rate)
        w.writeframes(pcm)


# ---------------------------------------------------------------------------
# 版权 / 路径纪律
# ---------------------------------------------------------------------------

def assert_ignored(path: str) -> None:
    """★ 派生自版权音频的生成物**永远不许入库**：写盘前用 git 自查一次。"""
    d = os.path.abspath(path)
    while not os.path.exists(d):
        nd = os.path.dirname(d)
        if nd == d:
            break
        d = nd
    try:
        r = subprocess.run(["git", "check-ignore", "-q", d], capture_output=True)
    except FileNotFoundError:
        print("  ! 没有 git，跳过 check-ignore 自查（请自行确认输出目录在 .gitignore 里）")
        return
    if r.returncode != 0:
        raise SystemExit(f"★ 拒绝写盘：{d} **不在 .gitignore 覆盖范围内** —— "
                         f"生成物派生自版权音频，必须落在 local/ 下（见 .gitignore 第 5 行）")


def rel(root: str, p: str) -> str:
    try:
        return os.path.relpath(p, root)
    except ValueError:
        return p


# ---------------------------------------------------------------------------
# main
# ---------------------------------------------------------------------------

def main(argv=None) -> int:
    here = os.path.dirname(os.path.abspath(__file__))
    default_root = os.path.abspath(os.path.join(here, "..", "..", "..", ".."))

    ap = argparse.ArgumentParser(description="生成卡 113 的灵界版 BGM（TH18 曲 + 低音放大 + TH13 的 b 命名）")
    ap.add_argument("--root", default=default_root, help="仓库根（默认自动定位）")
    ap.add_argument("--fmt", default=None, help="thbgm.fmt（默认 local/th18.v1.00a/dat/thbgm.fmt）")
    ap.add_argument("--dat", default=None, help="thbgm.dat（默认 local/th18.v1.00a/thbgm.dat）")
    ap.add_argument("--exe", default=None, help="th18.exe（映射对账用；缺则跳过）")
    ap.add_argument("--out", default=None, help="输出目录（默认 local/th18.v1.00a/trance_bgm/）")
    ap.add_argument("--set", dest="which", default=DEFAULT_SET, choices=sorted(SETS),
                    help="生成哪一批（默认 stage+boss = 6 面 + Ex 的**道中曲 + 关底曲**，共 14 首）")
    ap.add_argument("--gain-db", type=float, default=DEFAULT_GAIN_DB, help="低音增益 dB（默认 +9）")
    ap.add_argument("--taps", type=int, default=DEFAULT_TAPS, help="每段滑动平均抽头（默认 66）")
    ap.add_argument("--rate", type=int, default=DEFAULT_RATE, help="输出采样率（0 = 保持源采样率）")
    ap.add_argument("--headroom", type=float, default=0.97, help="峰值归一目标（相对满刻度）")
    ap.add_argument("--dsp", default="bass", choices=("bass", "th13-tilt"),
                    help="DSP 链：bass（默认，低音货架 y=x+g·lp，+9 dB）/ "
                         "th13-tilt（照 TH13 b 轨**实测**的三条带倾斜：<150 +1.33 / 500–2k −3.04 / 2k–6k −1.76 dB）")
    ap.add_argument("--norm", default="peak", choices=("peak", "none"),
                    help="归一方式：peak = 整轨峰值归一到 --headroom（默认，不削顶）；"
                         "none = 不做归一（会削顶 —— 只用来试听'纯放大'是什么样）")
    ap.add_argument("--list", action="store_true", help="只打印映射表，不生成")
    ap.add_argument("--verify-only", action="store_true", help="只校验已生成的文件（不写盘）")
    ap.add_argument("--measure", action="store_true",
                    help="生成后**独立回读**已生成的 WAV，量出低频带/中高频带相对源素材的实际变化（dB）")
    args = ap.parse_args(argv)

    root = os.path.abspath(args.root)
    fmt_path = args.fmt or os.path.join(root, "local/th18.v1.00a/dat/thbgm.fmt")
    dat_path = args.dat or os.path.join(root, "local/th18.v1.00a/thbgm.dat")
    exe_path = args.exe or os.path.join(root, "local/th18.v1.00a/th18.exe")
    out_dir = args.out or os.path.join(root, "local/th18.v1.00a/trance_bgm")
    dll_hdr = os.path.join(here, "..", "native", "cards", "trance_bgm_core.h")

    print("=== make_trance_bgm ===")
    print(f"  root {root}")
    if not os.path.exists(fmt_path):
        sys.exit(f"★ 没有 {fmt_path}\n  先解包：local/vendor/thtk/build/thdat/thdat -x 18 "
                 f"\"<modkit>/game/th18.dat\" thbgm.fmt")
    tracks = parse_fmt(fmt_path)
    print(f"  fmt {rel(root, fmt_path)}：{len(tracks)} 条记录")

    # ---- 映射：读 exe 的表并对账（一手来源 ① vs 本文件的表）----
    print("-- stage → 曲名 对账（exe 关卡表 0x4c9410，步长 0xd4）")
    table = check_exe_table(exe_path)
    if table:
        assert_mapping(table)
    else:
        print("  ! 没有 exe ⇒ 只靠本文件的表（musiccmt.txt 的口径）；请自行核对")

    # musiccmt 的第二条独立来源：只是提醒（正文是日文注释，机器判据太脆，不做硬断言）
    cmt = os.path.join(os.path.dirname(fmt_path), "musiccmt.txt")
    if os.path.exists(cmt):
        try:
            txt = open(cmt, "rb").read().decode("cp932", errors="replace")
            want = {"1": "１面", "2": "２面", "3": "３面", "4": "４面", "5": "５面", "6": "最終面", "7": "エキストラ"}
            ok = 0
            for stage, stem in STAGE_THEME.items():
                m = re.search(r"@bgm/" + stem + r"\b(.*?)(?=@bgm/|\Z)", txt, re.S)
                if m and want[str(stage)] in m.group(1):
                    ok += 1
            print(f"  musiccmt.txt 对账：{ok}/{len(STAGE_THEME)} 条道中曲的作者注释与关卡号相符")
            if ok != len(STAGE_THEME):
                print("  ! musiccmt 对账没全过 —— 以 exe 表为准（它才是活代码）")
        except Exception as e:  # 任何解析意外都不该挡住生成
            print(f"  ! musiccmt.txt 对账跳过：{e}")

    cap = check_dll_cap(dll_hdr)

    cut = measure_cutoff_hz(args.taps, 44100)
    if args.dsp == "th13-tilt":
        print("-- DSP：**th13-tilt**（照 TH13 b 轨实测的三条带倾斜做朴素滑平均带通）—— "
              + "、".join(f"{lo:g}–{hi:g} Hz {db:+.2f} dB" if kind == "band" else f"<{hi:g} Hz {db:+.2f} dB"
                          for kind, lo, hi, db in TH13_TILT))
        print("   ★ 这三条带的值**不是听感拍的**：来自 `tools/measure_th13_b_variants.py` 对 TH13 13 对的聚合"
              "（<150 Hz 0/13 在降、500–2k **13/13 在降**、2k–6k 10/13 在降）——见 AUDIT §CO-10。"
              "`--gain-db/--taps` 在这条链下**不生效**（带就定义在频点上）")
    else:
        print(f"-- DSP：**bass**（默认）：两段 {args.taps} 抽头滑动平均级联（三角窗）⇒ −3 dB ≈ {cut:.0f} Hz @44100；"
              f"y = x + {10 ** (args.gain_db / 20):.2f}·bass（{args.gain_db:+.1f} dB）")
    print(f"   峰值归一 {args.headroom:.2f} FS（{args.norm}）；降到 {args.rate or '源'} Hz；取循环段（丢掉 intro）")

    wanted = SETS[args.which]
    plan = []
    for stem in wanted:
        try:
            plan.append((track_by_stem(tracks, stem), stem))
        except KeyError:
            sys.exit(f"★ fmt 里没有 {stem}（fmt 换了？）")

    if args.list:
        print(f"\n-- 计划（--set {args.which}，{len(plan)} 首）")
        for t, stem in plan:
            print(f"  [{t.index:2d}] {stem:10s} {t.seconds:7.2f}s 循环 {t.loop_seconds:7.2f}s "
                  f"→ {stem}b.wav")
        return 0

    # ---- ★ 预检：任何一条超过 DLL 上限 ⇒ **整批拒绝**（生成期就红，不留到运行期静默降级）----
    over = [(t, stem, predicted_out_bytes(t, args.rate)) for t, stem in plan]
    over = [(t, stem, n) for t, stem, n in over if n > cap]
    if over:
        print(f"\n★ 拒绝生成：下面 {len(over)} 条**超过 DLL 的素材上限** {cap} B "
              f"（= TRANCE_BGM_MAX_PCM，读自 {os.path.relpath(dll_hdr, root)}）：")
        for t, stem, n in over:
            print(f"    {stem}b.wav  {n} B（{n / 1048576:.2f} MiB）> {cap} B（{cap / 1048576:.0f} MiB）")
        print("  两条出路（都改了再重跑）：\n"
              "    ① 抬上限：native/cards/trance_bgm_core.h 的 `TRANCE_BGM_MAX_PCM`（本工具自动跟着抬）——\n"
              "       它只是 `VirtualAlloc` 的天花板，抬高的代价是灵界期间多 commit 那点内存，\n"
              "       而 DLL 体积/常驻**不受影响**（按需分配、`finish_release()` 立刻释放）；\n"
              "    ② 单曲降采样率：`--rate <更小>`（整批一起降；想只降某几首就分两次跑不同 --set/--out）。")
        return 2

    # ---- 生成 / 校验 ----
    os.makedirs(out_dir, exist_ok=True)
    assert_ignored(out_dir)

    if args.verify_only:
        return verify_only(out_dir, plan, args)

    hdr = (f"{'#':>2} {'stage':>5} {'源记录':10s} {'源字节':>10} {'循环时长':>7} "
           f"{'出字节':>10} {'出时长':>7} {'原峰':>6} {'原RMS':>6} {'放大':>7} {'归一':>8} "
           f"{'净响度':>7} {'低音占比':>8} {'文件'}")
    print("\n" + hdr)
    lines, worst = [], 0
    for t, stem in plan:
        out_name = f"{stem}b.wav"
        out_path = os.path.join(out_dir, out_name)
        predicted = predicted_out_bytes(t, args.rate)
        raw = slice_loop(None, dat_path, t)
        pcm, rate, ch, st = convert(raw, t, args.rate, args.gain_db, args.taps, args.headroom,
                                    args.norm, args.dsp)
        if len(pcm) > cap:                    # 预检已挡（预测式与实测式两条独立算路）
            print(f"  ★ {out_name} = {len(pcm)} B > DLL 上限 {cap} B —— 预测值算错了？拒绝写盘")
            return 2
        if len(pcm) != predicted:
            print(f"  ★ {out_name}: 实测 {len(pcm)} B != 预测 {predicted} B（尺寸预测与 DSP 脱钩了）")
            return 2
        write_wav(out_path, pcm, rate, ch)
        worst = max(worst, len(pcm))
        md5 = hashlib.md5(pcm).hexdigest()
        lines.append(f"{out_name}\t{t.index}\t{t.name}\t{t.loop_bytes}\t{len(pcm)}\t{rate}\t{ch}\t{md5}")
        stage = next((k for k, v in STAGE_THEME.items() if v == stem), None)
        stage = stage if stage is not None else next((k for k, v in STAGE_BOSS.items() if v == stem), 0)
        print(f"{t.index:2d} {stage:>5} {t.name:10s} {t.loop_bytes:>10} {st['secs_in']:>6.1f}s "
              f"{len(pcm):>10} {st['secs_out']:>6.1f}s "
              f"{st['src_peak_db']:>+5.1f} {st['src_rms_db']:>+5.1f} {st['boost_db']:>+6.1f}dB "
              f"{st['norm_db']:>+7.1f}dB {st['boost_db'] + st['norm_db']:>+6.1f}dB {st['low_share']:>7.2f}x "
              f"{rel(root, out_path)}")
        if st.get("calib"):
            print(f"     ↳ th13-tilt 校准后实测倾斜（5 带，减整轨）："
                  + " ".join(f"{MEASURE_NAMES[i]}:{st['calib'][i]:+.2f}" for i in range(5))
                  + "   （目标 " + " / ".join(
                      (f"<{hi:g}" if kind == "lowpass" else f"{lo:g}-{hi:g}") + f":{db:+.2f}"
                      for kind, lo, hi, db in TH13_TILT) + "）")
        if st["clipped"]:
            print(f"     ★ 削顶 {st['clipped']} 个样本"
                  + ("（`--norm none` 的预期结果）" if args.norm == "none" else "（不该发生）"))

    with open(os.path.join(out_dir, "MANIFEST.tsv"), "w", encoding="utf-8") as f:
        f.write("# make_trance_bgm.py 生成清单（派生自版权音频 ⇒ 永久留在 local/，不入库）\n")
        f.write(f"# set={args.which} gain_db={args.gain_db} taps={args.taps} rate={args.rate} "
                f"headroom={args.headroom} cutoff_hz={cut:.0f} dll_max_pcm={cap}\n")
        f.write("file\tfmt_index\tsrc_name\tsrc_loop_bytes\tout_bytes\trate\tchannels\tmd5_pcm\n")
        f.write("\n".join(lines) + "\n")

    if args.measure:
        # 5 条带与 `measure_th13_b_variants.py` **同一套**（<150 / 150-500 / 500-2k / 2k-6k / >6k）
        # ⇒ 我们的产物与 TH13 的实测倾斜可以直接对表（A/B 用）。
        names = MEASURE_NAMES

        def band_e(x, fs):
            return band_energies_db(x, fs)

        print("\n-- 独立回读（解码生成物，与源素材的**同一个循环段**比带内能量；+ = 变响）")
        print(f"  {'源记录':10s} " + " ".join(f"{n:>9s}" for n in names) + f" {'整轨':>8s}   倾斜相对(减整轨)")
        for t, stem in plan:
            src = np.frombuffer(slice_loop(None, dat_path, t), dtype="<i2") \
                    .reshape(-1, t.channels).astype(np.float64).mean(axis=1)
            with wave.open(os.path.join(out_dir, f"{stem}b.wav"), "rb") as w:
                ro = w.getframerate()
                got = np.frombuffer(w.readframes(w.getnframes()), dtype="<i2") \
                        .reshape(-1, w.getnchannels()).astype(np.float64).mean(axis=1)
            f = max(1, t.rate // ro)
            src_s = src[:got.shape[0] * f]
            if f > 1:
                src_s = src_s[:(src_s.shape[0] // f) * f].reshape(-1, f).mean(axis=1)
            m = min(src_s.shape[0], got.shape[0])
            src_s, got_s = src_s[:m], got[:m]
            ea, eb = band_e(src_s, ro), band_e(got_s, ro)
            lvl = 10.0 * np.log10(float(np.mean(got_s ** 2)) / float(np.mean(src_s ** 2)))
            tilt = [(eb[i] - lvl) - ea[i] for i in range(len(names))]
            print(f"  {t.name:10s} " + " ".join(f"{eb[i] - ea[i]:>+8.2f}" for i in range(len(names)))
                  + f" {lvl:>+7.2f}dB   " + " ".join(f"{v:>+5.2f}" for v in tilt))

    total = sum(int(l.split("\t")[4]) for l in lines)
    print(f"\n合计 {len(lines)} 个文件、{total / 1048576:.1f} MiB，最大单个 {worst / 1048576:.1f} MiB"
          f"（DLL 上限 {cap / 1048576:.0f} MiB）")
    print(f"清单：{rel(root, os.path.join(out_dir, 'MANIFEST.tsv'))}")

    # ---- 部署提示（本地步骤；DLL 从自己的目录旁边的 bgm/ 找素材）----
    print("\n-- 部署（本地步骤，见 AUDIT.md §CO-6 / local/DLL-SYNC.md）\n"
          f"  M=\"/mnt/d/项目/幻想指南/18/th18_modkit\"\n"
          "  for d in mods thcrap/bin game; do mkdir -p \"$M/$d/bgm\" && "
          f"cp -f {rel(root, out_dir)}/*.wav \"$M/$d/bgm/\"; done\n"
          "  ★ 三处都要放：DLL 的候选目录 = ①<DLL 自己所在目录>\\bgm ②它上两级的 mods\\bgm ③<exe 目录>\\bgm；\n"
          "    实跑证明**真正被加载的是 thcrap/bin 那一份**（启动器在开始游戏那一刻从 mods/ 复制过去的），\n"
          "    所以只放 mods/bgm 会一直报 `素材不在`。`bash local/sync_modkit_dll.sh` 已经一次同步三处。")
    return 0


def verify_only(out_dir: str, plan, args) -> int:
    """只读校验：MANIFEST 在不在、文件在不在、尺寸/采样率/信道/md5 对不对。"""
    man = os.path.join(out_dir, "MANIFEST.tsv")
    if not os.path.exists(man):
        print(f"★ 没有 {man} —— 先跑一次生成")
        return 1
    rows = {}
    for ln in open(man, encoding="utf-8"):
        if ln.startswith("#") or ln.startswith("file\t"):
            continue
        p = ln.rstrip("\n").split("\t")
        if len(p) >= 8:
            rows[p[0]] = p
    bad = 0
    for t, stem in plan:
        name = f"{stem}b.wav"
        path = os.path.join(out_dir, name)
        row = rows.get(name)
        if not row:
            print(f"  FAIL {name}: 不在清单里")
            bad += 1
            continue
        if not os.path.exists(path):
            print(f"  FAIL {name}: 文件不在")
            bad += 1
            continue
        with wave.open(path, "rb") as w:
            got = (w.getnchannels(), w.getsampwidth(), w.getframerate(), w.getnframes() * w.getnchannels() * 2)
        want = (int(row[6]), 2, int(row[5]), int(row[4]))
        md5 = hashlib.md5(open(path, "rb").read()[44:]).hexdigest()  # 跳过 44 B 的 canonical WAV 头
        if got != want:
            print(f"  FAIL {name}: (ch,width,rate,bytes) = {got}，清单是 {want}")
            bad += 1
        elif md5 != row[7]:
            print(f"  FAIL {name}: md5 {md5} != 清单 {row[7]}")
            bad += 1
        else:
            print(f"  OK   {name}: {want[3]} B, {want[2]} Hz, {want[0]}ch (md5 {md5[:8]}…)")
    print(f"\n{len(plan)} 个文件，{bad} 个失败")
    return 1 if bad else 0


if __name__ == "__main__":
    sys.exit(main())
