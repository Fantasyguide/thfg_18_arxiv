#!/usr/bin/env python3
"""make_trance_rearr.py —— 用第三方同人 MIDI 生成**灵界版重编曲**素材（主旋律压低、伴奏抬高）。

    python3 tools/make_trance_rearr.py --stem th18_03 --stem th18_04     # 先做 1 面那两首
    python3 tools/make_trance_rearr.py                                  # 全部（读 MANIFEST 里有的）
    python3 tools/make_trance_rearr.py --render=naive --stem th18_03    # 老合成器（兜底 / A-B 对照）

## 用户口径（2026-09-14）
「只要有 th18 的 midi，**放大其非主旋律、减小其主旋律**即可」——听感上要像"**另一首曲子**"，
而不是同一首换了 EQ（我们之前那条 naive 低音放大已被用户否掉）。

## 为什么这条路是"复刻"而不是"自定"
一手实测（`local/th13.v1.00c/dat/thbgm.fmt` + 用户机上的 `thbgm.dat`，2026-09-14）：
TH13 的 `NN`/`NNb` 对**时长严格相等、时间轴零延迟对齐**（能量包络最佳对齐 = +0.00 s），
但**相关度分两档**：`06` vs `06b` r = **0.911**（同一演奏的另一种混音）、
`03` vs `03b` r = **0.407**（真的换了编曲/织体）。⇒ 原版的"灵界版"**至少有一半是重编曲**，
这正是用户听成"另一首曲子"的那部分。⇒ 我们用现成 MIDI 重配器 + 压主旋律，方向与一手一致。

## 管线（四步）
1. **解析 SMF**（`parse_smf`，自写，支持 format 0/1、running status、tempo 变化、program change）；
2. **判主旋律**（`pick_melody`）：按"音域最高 + 单音为主 + 音符密度"打分，选出 1–2 条主旋律轨；
   主旋律 × `--melody-gain`（默认 0.35 ≈ −9 dB）、伴奏 × 1.0，贝斯通道再 × `--bass-gain`（默认 1.6）；
3. **渲染**（`--render`）：
   - **`sf2`（默认）** = **GM SoundFont 真采样渲染**（2026-10-19 起）：把增益写进 MIDI 的
     **note-on velocity**（主旋律 × `--melody-gain`、贝斯通道 × `--bass-gain`、其余 ×1.0，
     超 127 时**整体等比缩**保相对关系）⇒ 存临时 `.mid` ⇒
     `fluidsynth -F out.wav -r 44100 -g 1.0 <sf2> <mid>` ⇒ 读回 PCM ⇒ 44100→22050（FIR 半带抽取）
     ⇒ 22050 Hz 立体声 16 bit —— 与引擎里其它素材同格式（`trance_bgm_core` 的 fmt 记录口径）。
     **为什么改 MIDI 而不是在合成器里分轨**：`pick_melody` 判出来的是**轨**，而 fluidsynth 只吃
     MIDI；把增益写进 velocity = 每个音各算各的，比"按通道分几次渲染再相加"简单且不会漏掉
     同通道不同角色的音。velocity→振幅实测**近似线性**（见 `_sf2_velocity_gain`），
     所以线性增益 g 直接当 velocity 因子用。
   - **`naive`（兜底，2026-09-14 那版）** = 自写小型合成器（GM program 家族 → 锯齿/脉冲/正弦 + ADSR，
     鼓用噪声包络）。**没有 fluidsynth / 没有 sf2 的机器**靠它仍能出全套素材。
4. **对齐**（`fine_align`）：把渲染结果与**原曲循环体**的 chroma 时间序列做相关，取最佳延迟 ⇒
   以那个偏移为起点、截取**与原曲循环体严格等长**的一段（`--xfade` 加末尾 500 ms 交叉淡化）
   ⇒ 时长/循环点与原曲一致（我们刚修好的"同时间点 seek"继续有效）。
   ★ 两条渲染路径共用同一个 `fine_align`/截断/归一化尾巴 —— 换音色**不动时间轴**。

## 授权与存放
MIDI 是**第三方同人作品**（来源、编曲者署名、逐文件 md5 见 `local/th18.v1.00a/midi/MANIFEST.tsv`；
Usage Policy = 「可修改、**必须署名编曲者**；**ZUN 本家的 MIDI 不得用于 fangame**」——本目录 17 首
全部是同人编曲（Karl Zuñiga），无 ZUN 本家 MIDI）。
SoundFont 是**第三方音色库**（来源 URL + sha256 见 `local/th18.v1.00a/sf2/SOURCE.txt` 与
`mods/…/NOTICE.md` 的「音色库」一节）。
生成物、MIDI、sf2 **都只落在 `local/`（gitignored，不入库）**；本工具本身不含任何素材。
"""
import argparse
import glob
import hashlib
import math
import os
import shutil
import struct
import subprocess
import sys
import tempfile
import wave

import numpy as np

HERE = os.path.dirname(os.path.abspath(__file__))
MOD = os.path.dirname(HERE)
REPO = os.path.dirname(os.path.dirname(os.path.dirname(MOD)))
MIDI_DIR = os.path.join(REPO, "local", "th18.v1.00a", "midi")
SF2_DIR = os.path.join(REPO, "local", "th18.v1.00a", "sf2")
# 无 sudo 时的 fluidsynth：`apt-get download` + `dpkg-deb -x` 解到 local/vendor/（**不动系统**）
FS_VENDOR_GLOB = os.path.join(REPO, "local", "vendor", "fluidsynth-*")
RENDER_SR = 44100                   # sf2 路径的渲染采样率（最后降到 SR=22050 出盘）

# ★★ 2026-09-15（用户："道中播放的是关底的曲子"）——**内部曲名 ≠ 音乐室编号**，必须查一手表：
#   内部曲名（`th18_01, 03, 04, …`）里**没有 `th18_02`**（ZUN 的编号有洞），
#   而音乐室/同人 MIDI 用的是**连续编号**（"N. 标题"），两者在 `th18_14` 之后就错开了：
#       th18_17 = No.14（Ex 道中）、th18_18 = No.15（Ex boss）、
#       th18_15 = No.16（结局）、  th18_16 = No.17（Staff）—— 不是简单的 +1 平移。
#   ⇒ **一手来源：`dat/musiccmt.txt`**（游戏自带的音乐室注释文件，ZUN 亲手写的对照：
#      `@bgm/th18_NN` 块里第一行就是 `No. N  标题`）。这里直接解析它，不硬编码、不猜。
#      （旁证：关卡表 `th18:0x4c9410` stride `0xd4`，`+0x3c`/`+0x40` = 道中/boss 两个曲名，
#        stage1 = (`th18_03`, `th18_04`)；`0x443121`/`0x443130` 把这两个名字注册进 BGM 槽 0/1。
#        musiccmt 里 `th18_03` = No.2 = 1 面道中，完全闭环。）
MUSICCMT = os.path.join(REPO, "local", "th18.v1.00a", "dat", "musiccmt.txt")


def music_room_numbers():
    """→ {内部曲名: 音乐室编号}，直接读 `dat/musiccmt.txt`（authoritative）。"""
    txt = open(MUSICCMT, "rb").read().decode("cp932")
    cur, out = None, {}
    for line in txt.splitlines():
        line = line.strip()
        if line.startswith("@bgm/"):
            cur = line[len("@bgm/"):].strip()
        elif cur and line.startswith("No."):
            rest = line[len("No."):].strip()
            num = rest.split()[0] if rest else ""
            if num.isdigit():
                out[cur] = int(num)
            cur = None
    return out


def midi_index_for(internal_name):
    """内部曲名（如 th18_03）→ 同人 MIDI 的槽位号（如 2）。查 `musiccmt.txt` 的一手表。"""
    return music_room_numbers().get(internal_name)
OUT_DIR = os.path.join(REPO, "local", "th18.v1.00a", "trance_bgm")
FMT = os.path.join(REPO, "local", "th18.v1.00a", "dat", "thbgm.fmt")
PCM = os.path.join(REPO, "local", "th18.v1.00a", "thbgm.dat")

SR = 22050
BLOCK = 2 * 2                       # 16 bit 立体声
FMT_RECORD = 0x34


# ---------------------------------------------------------------- SMF 解析
def _vlq(b, i):
    v = 0
    while True:
        c = b[i]; i += 1
        v = (v << 7) | (c & 0x7F)
        if not (c & 0x80):
            return v, i


def parse_smf(path, velout=None):
    """→ (tracks, ppq, tempos)。tracks[i] = [(tick, 'on'/'off'/'prog', ch, a, b)]（按 tick 升序）。

    `velout`（list，可选）：把每条 note-on 的 **velocity 字节在文件里的绝对偏移**追加进去，
    形如 `(轨号, tick, 通道, 音高, velocity, 偏移)` —— sf2 路径靠它**原地改 velocity**
    （比"重新生成一个 SMF"安全：所有 meta/sysex/CC 原样保留）。
    """
    d = open(path, "rb").read()
    if d[:4] != b"MThd":
        raise SystemExit(f"{path}: 不是 SMF")
    _, fmt, ntrk, ppq = struct.unpack_from(">IHHH", d, 4)
    if fmt not in (0, 1):
        raise SystemExit(f"{path}: format {fmt} 不支持（只支持 0/1）")
    off = 14
    tracks, tempos = [], []                    # 只装**文件里真实存在**的 tempo 事件（见 tempo_at 的注释）
    while off + 8 <= len(d):
        sig, ln = d[off:off + 4], struct.unpack_from(">I", d, off + 4)[0]
        body = d[off + 8:off + 8 + ln]
        body_off = off + 8
        off += 8 + ln
        if sig != b"MTrk":
            continue
        ti = len(tracks)
        evs, i, tick, run = [], 0, 0, None
        while i < len(body):
            dt, i = _vlq(body, i)
            tick += dt
            st = body[i]
            if st & 0x80:
                run = st; i += 1
            else:
                st = run
            if st is None:
                break
            hi, ch = st & 0xF0, st & 0x0F
            if st == 0xFF:
                typ = body[i]; i += 1
                ln2, i = _vlq(body, i)
                data = body[i:i + ln2]; i += ln2
                if typ == 0x51 and ln2 == 3:
                    tempos.append((tick, (data[0] << 16) | (data[1] << 8) | data[2]))
                elif typ == 0x2F:
                    break
            elif st in (0xF0, 0xF7):
                ln2, i = _vlq(body, i); i += ln2
            elif hi in (0x80, 0x90, 0xA0, 0xB0, 0xE0):
                a, b = body[i], body[i + 1]
                vel_at = body_off + i + 1            # ← b 的绝对偏移（在 i 前进之前算）
                i += 2
                if hi == 0x90 and b > 0:
                    evs.append((tick, "on", ch, a, b))
                    if velout is not None:
                        velout.append((ti, tick, ch, a, b, vel_at))
                elif hi == 0x80 or (hi == 0x90 and b == 0):
                    evs.append((tick, "off", ch, a, 0))
            elif hi in (0xC0, 0xD0):
                a = body[i]; i += 1
                if hi == 0xC0:
                    evs.append((tick, "prog", ch, a, 0))
            else:
                break
        tracks.append(evs)
    if not tempos:
        tempos = [(0, 500000)]                 # 文件一个 tempo 事件都没有 ⇒ 用 SMF 默认 120 BPM
    tempos.sort(key=lambda tu: tu[0])          # ⚠ 只按 tick 排（按 (tick, µs) 排会把同一 tick 的顺序搞乱）
    return tracks, ppq, tempos


def ticks_to_sec(tempos, ppq):
    """→ f(tick)（分段线性，按 tempo 段累加）。"""
    segs, t, last, us = [], 0.0, 0, tempos[0][1]
    for tick, u in tempos:
        if tick > last:
            segs.append((last, tick, t, us))
            t += (tick - last) * us / 1e6 / ppq
            last = tick
        us = u
    segs.append((last, 1 << 62, t, us))

    def f(tick):
        for a, b, ta, u in segs:
            if a <= tick < b:
                return ta + (tick - a) * u / 1e6 / ppq
        return t
    return f


# ---------------------------------------------------------------- 主旋律判定
def pick_melody(tracks, t2s):
    """→ (melody_track_set, 报表)。判据：音域高、单音为主（复音重叠少）、音符密度高。"""
    stats = []
    for i, evs in enumerate(tracks):
        notes = [(t2s(t), a, b) for t, k, ch, a, b in evs if k == "on"]
        if len(notes) < 8:
            stats.append(None); continue
        pitches = [n[1] for n in notes]
        notes_sorted = sorted(notes)
        overlap, busy = 0, 0.0
        end = -1.0
        for t, p, v in notes_sorted:
            if t < end: overlap += 1
            else: busy += 1
            end = max(end, t + 0.05)
        dur = max(1e-6, notes_sorted[-1][0] - notes_sorted[0][0])
        stats.append(dict(n=len(notes), pitch=float(np.mean(pitches)),
                          hi=float(np.percentile(pitches, 90)),
                          mono=1.0 - overlap / len(notes),
                          dens=len(notes) / dur))
    ok = [(i, s) for i, s in enumerate(stats) if s]
    if not ok:
        return set(), stats
    hi_ref = max(s["hi"] for _, s in ok)
    ranked = sorted(ok, key=lambda kv: (kv[1]["hi"] - 0.15 * hi_ref * (1 - kv[1]["mono"])
                                        + 6.0 * kv[1]["mono"] + 0.001 * kv[1]["dens"]), reverse=True)
    return {ranked[0][0]}, stats


# ---------------------------------------------------------------- 合成
def adsr(n, a, d, s, r, sr=SR):
    e = np.zeros(n, dtype=np.float32)
    ai = min(int(a * sr), n); di = min(int(d * sr), max(0, n - ai))
    si = max(0, n - ai - di)
    if ai: e[:ai] = np.linspace(0, 1, ai, dtype=np.float32)
    if di: e[ai:ai + di] = np.linspace(1, s, di, dtype=np.float32)
    if si: e[ai + di:] = s
    tail = min(int(r * sr), n)
    if tail: e[n - tail:] *= np.linspace(1, 0, tail, dtype=np.float32)
    return e


def osc(kind, freq, n, sr=SR):
    t = np.arange(n, dtype=np.float32) / sr
    ph = 2 * np.pi * freq * t
    if kind == "saw":
        x = 2.0 * ((ph / (2 * np.pi)) % 1.0) - 1.0
    elif kind == "pulse":
        x = np.where((ph / (2 * np.pi)) % 1.0 < 0.35, 1.0, -1.0).astype(np.float32)
    elif kind == "tri":
        x = 2.0 * np.abs(2.0 * ((ph / (2 * np.pi)) % 1.0) - 1.0) - 1.0
    elif kind == "sine":
        x = np.sin(ph)
    else:                                   # soft（正弦+少量三次谐波，给 pad）
        x = np.sin(ph) + 0.25 * np.sin(3 * ph)
    return x.astype(np.float32)


def voice_for(prog):
    """GM program → (波形, ADSR, 八度偏移, 增益)。不求像真乐器，只求"层次分明"。
    低音要厚、pad 要稳、主音要亮 —— 这样"压主旋律、抬伴奏"的差别才听得出来。"""
    if prog is None:      return ("saw",   (0.005, 0.08, 0.7, 0.05), 0, 1.0)
    if 32 <= prog <= 39:  return ("sine",  (0.005, 0.06, 0.9, 0.05), -1, 1.5)   # bass
    if 0 <= prog <= 7:    return ("tri",   (0.002, 0.15, 0.5, 0.08), 0, 0.9)    # piano
    if 16 <= prog <= 23:  return ("pulse", (0.004, 0.10, 0.6, 0.06), 0, 1.0)    # organ
    if 24 <= prog <= 31:  return ("saw",   (0.004, 0.12, 0.7, 0.08), 0, 0.9)    # guitar
    if 40 <= prog <= 47:  return ("pulse", (0.006, 0.12, 0.8, 0.10), 0, 1.0)    # strings
    if 56 <= prog <= 63:  return ("pulse", (0.010, 0.10, 0.85, 0.12), 0, 1.1)   # brass
    if 80 <= prog <= 95:  return ("saw",   (0.020, 0.30, 0.9, 0.25), 1, 0.7)    # lead(亮)
    if 96 <= prog <= 103: return ("soft",  (0.050, 0.30, 0.9, 0.40), 0, 0.7)    # pad
    return ("saw", (0.006, 0.12, 0.7, 0.10), 0, 0.9)


def drum(kind, n, rng):
    t = np.arange(n, dtype=np.float32) / SR
    if kind == "kick":
        f = 110 * np.exp(-t * 28) + 45
        x = np.sin(2 * np.pi * np.cumsum(f) / SR) * np.exp(-t * 9)
    elif kind == "snare":
        x = (rng.standard_normal(n) * 0.6 + np.sin(2 * np.pi * 190 * t) * 0.4) * np.exp(-t * 18)
    elif kind == "hat":
        x = rng.standard_normal(n) * np.exp(-t * 60) * 0.35
    else:                                   # tom / 其它：给个低沉噪声
        x = rng.standard_normal(n) * np.exp(-t * 12) * 0.5
    return x.astype(np.float32)


def render_naive(notes, prog_by_ch, secs, melody_ch, mel_gain, bass_gain, seed=1234):
    """`--render=naive`：自写合成器的老路径（兜底用，2026-09-14 那版原样保留）。
    notes: [(t0, dur, midi, vel, ch)]；→ 立体声 float32 (n,2)。"""
    n_total = int(secs * SR) + SR
    left = np.zeros(n_total, dtype=np.float32)
    right = np.zeros(n_total, dtype=np.float32)
    rng = np.random.default_rng(seed)
    for t0, dur, midi, vel, ch in notes:
        i0 = int(t0 * SR)
        if i0 >= n_total or dur <= 0:
            continue
        if ch == 9:                                            # 鼓通道（GM channel 10）
            n = min(int(0.18 * SR), n_total - i0)
            k = "kick" if midi in (35, 36) else "snare" if midi in (38, 40) else "hat"
            x = drum(k, n, rng) * (vel / 127.0) * 0.9
            pan_l = pan_r = 0.5
        else:
            kind, (a, d, s, r), octshift, gain = voice_for(prog_by_ch.get(ch))
            if ch in melody_ch:                                # 主旋律：压低
                gain *= mel_gain
            else:
                gain *= bass_gain if (prog_by_ch.get(ch) or 0) in range(32, 40) else 1.0
            f = 440.0 * 2 ** ((midi - 69 + 12 * octshift) / 12.0)
            n = min(int((dur + r) * SR), n_total - i0)
            if n <= 4:
                continue
            env = adsr(n, a, d, s, r)[:n]
            x = osc(kind, f, n) * env * (vel / 127.0) * gain * 0.35
            pan_l = pan_r = 0.5
        l = int((1.0 - pan_l) * 0.0)
        left[i0:i0 + len(x)] += x * (1.0 - l)
        right[i0:i0 + len(x)] += x * (1.0 - l)
    return np.stack([left, right], axis=1)


# ---------------------------------------------------------------- 合成 / SoundFont 渲染
def _is_sf2(p):
    try:
        with open(p, "rb") as f:
            h = f.read(12)
        return h[:4] == b"RIFF" and h[8:12] == b"sfbk"
    except OSError:
        return False


def find_fluidsynth(explicit=None):
    """→ (可执行文件, 追加的 LD_LIBRARY_PATH 或 None)。

    查找顺序：`--fluidsynth` → `$RL_FLUIDSYNTH` → `PATH` → `local/vendor/fluidsynth-*/usr/bin/fluidsynth`。
    最后一条是**无 sudo 密码**时的装法（本机现状）：`apt-get download fluidsynth libfluidsynth3 …`
    + `dpkg-deb -x` 解到 `local/vendor/fluidsynth-<ver>/`（**完全不动系统**、删目录即卸载），
    跑的时候把它的 `usr/lib/x86_64-linux-gnu[+ /pulseaudio]` 塞进子进程的 `LD_LIBRARY_PATH`。
    """
    cands = []
    if explicit:
        cands.append(explicit)
    if os.environ.get("RL_FLUIDSYNTH"):
        cands.append(os.environ["RL_FLUIDSYNTH"])
    w = shutil.which("fluidsynth")
    if w:
        cands.append(w)
    for d in sorted(glob.glob(FS_VENDOR_GLOB)):
        cands.append(os.path.join(d, "usr", "bin", "fluidsynth"))
    for c in cands:
        if not (os.path.isfile(c) and os.access(c, os.X_OK)):
            continue
        # local/vendor/fluidsynth-X/usr/bin/fluidsynth → local/vendor/fluidsynth-X/usr/lib/x86_64-linux-gnu
        lib = os.path.join(os.path.dirname(os.path.dirname(c)), "lib", "x86_64-linux-gnu")
        extra = None
        if os.path.isdir(lib):
            extra = ":".join(p for p in (lib, os.path.join(lib, "pulseaudio"),
                                         os.environ.get("LD_LIBRARY_PATH", "")) if p)
        return c, extra
    return None, None


def find_sf2(explicit=None):
    """→ .sf2 路径（或 None）。顺序：`--sf2` → `$RL_SF2` → `local/th18.v1.00a/sf2/*.sf2`。

    同目录多份时**优先名字里带 thfont 的**（owner 口径：ZUN 用的是 Roland SD-90，
    THfont 是社区替代），否则按文件名取第一份（本机 = `GeneralUser-GS.sf2`）。
    来源/许可/sha256 登记在 `local/th18.v1.00a/sf2/SOURCE.txt`，正文抄进 `NOTICE.md` 的「音色库」。
    """
    cands = []
    if explicit:
        cands.append(explicit)
    if os.environ.get("RL_SF2"):
        cands.append(os.environ["RL_SF2"])
    # ★ 2026-10-19：docstring 一直写"优先 thfont"，但代码只按文件名排序（`GeneralUser-…` < `thfont…`
    #   ⇒ 真把 thfont 丢进去反而不会被选中）。这里按 owner 口径落地：**名字里带 thfont 的排最前**，
    #   其余按文件名（大小写不敏感）。只改选择顺序，不动渲染。
    cands += sorted(glob.glob(os.path.join(SF2_DIR, "*.sf2")),
                    key=lambda p: (0 if "thfont" in os.path.basename(p).lower() else 1,
                                   os.path.basename(p).lower()))
    for c in cands:
        if c and _is_sf2(c):
            return c
    if explicit or os.environ.get("RL_SF2"):
        raise SystemExit(f"指定的 SoundFont 不是合法 .sf2（缺 RIFF/sfbk 头）：{cands[0]}")
    return None


def _sf2_velocity_gain(v, gain):
    """线性增益 → velocity 因子。

    ★ 实测（2026-10-19，`fluidsynth 2.2.5` + GeneralUser-GS，同一音符 vel 127 vs 60）：
      振幅比 **0.449**，而 60/127 = 0.472 ⇒ 指数 **1.07 ≈ 1**，即 fluidsynth 的
      velocity→振幅**基本是线性的**（不是 GM 文档里那个平方律）⇒ 直接用 `v * gain`。
      所以 `--melody-gain 0.35` 在 sf2 路径上同样是 **−9 dB**，与 naive 路径口径一致。
    """
    return int(round(v * gain))


def _fir_lowpass(taps, cutoff, sr):
    """窗函数 sinc 低通（线性相位，DC 增益 1）。用于 44100→22050 抽取前的抗混叠。"""
    n = np.arange(taps) - (taps - 1) / 2.0
    h = 2.0 * cutoff / sr * np.sinc(2.0 * cutoff / sr * n) * np.blackman(taps)
    return (h / h.sum()).astype(np.float32)


def downsample_2x(x, sr=RENDER_SR, cutoff=10000.0):
    """(n,2) float32 @ sr → (n/2,2) @ sr/2。31 tap FIR + 抽取（不平均，保留高频）。"""
    h = _fir_lowpass(31, cutoff, sr)
    y = np.empty((len(x) // 2, x.shape[1]), dtype=np.float32)
    for c in range(x.shape[1]):
        # 'same' 让群延迟居中；抽到 sr/2 后正好抵消
        y[:, c] = np.convolve(x[:, c], h, mode="same")[::2][:len(y)]
    return y


def read_wav_pcm(path, want_sr=RENDER_SR):
    """读 fluidsynth 写出的 wav → (float32 (n,2), sr)。只吃 16 bit PCM（fluidsynth 的默认输出）。"""
    with wave.open(path, "rb") as w:
        ch, sw, sr, n = w.getnchannels(), w.getsampwidth(), w.getframerate(), w.getnframes()
        raw = w.readframes(n)
    if sw != 2:
        raise SystemExit(f"{path}: 只支持 16 bit PCM（实际 {sw*8} bit）")
    a = np.frombuffer(raw, dtype="<i2").astype(np.float32).reshape(-1, ch) / 32768.0
    if ch == 1:
        a = np.repeat(a, 2, axis=1)
    elif ch > 2:
        a = a[:, :2]
    if sr != want_sr:
        raise SystemExit(f"{path}: 采样率 {sr} ≠ {want_sr}（-r 参数没生效？）")
    return a, sr


def render_sf2(mid_path, sf2, fs, fs_libdir, melody_tracks, prog_by_ch, notes,
               mel_gain, bass_gain, log=print):
    """SoundFont 渲染：改 velocity 的临时 MIDI ⇒ fluidsynth 渲 44100 Hz wav ⇒ 读回 ⇒ 降到 22050。

    返回 (n,2) float32 @ SR —— 与 `render_naive` 同接口，后面的 fine_align / 截断 / 归一化
    **一个字都没改**（换的只是音源，时间轴不动）。
    """
    vol = []
    parse_smf(mid_path, velout=vol)            # 只为拿 velocity 字节偏移（几何与主流程那份一致）
    raw = bytearray(open(mid_path, "rb").read())
    # ① 每音的线性增益：主旋律（按**轨**判，`pick_melody` 的结论原样用）→ mel_gain；
    #    贝斯（该通道的 GM program ∈ 32–39）→ bass_gain；鼓（ch 9）/其余 → 1.0。
    scaled = []
    for ti, tick, ch, note, vel, at in vol:
        g = 1.0
        if ch != 9:
            if ti in melody_tracks:
                g = mel_gain
            elif (prog_by_ch.get(ch) or 0) in range(32, 40):
                g = bass_gain
        scaled.append((_sf2_velocity_gain(vel, g), vel, at))
    # ② 超过 127 就**整体等比缩**（不是逐音 clip）：相对关系不变，绝对电平交给末尾的峰值归一。
    top = max(v for v, _, _ in scaled)
    shrink = 1.0
    if top > 127:
        shrink = 127.0 / top
        scaled = [(max(1, int(round(v * shrink))), o, at) for v, o, at in scaled]
    chg = sum(1 for v, o, _ in scaled if v != o)
    for v, _, at in scaled:
        raw[at] = v
    tmp = tempfile.mkdtemp(prefix="rearr-sf2-")
    try:
        mid2 = os.path.join(tmp, "scaled.mid")
        wav = os.path.join(tmp, "out.wav")
        open(mid2, "wb").write(bytes(raw))
        env = dict(os.environ)
        if fs_libdir:
            env["LD_LIBRARY_PATH"] = fs_libdir
        cmd = [fs, "-F", wav, "-r", str(RENDER_SR), "-g", "1.0", "-i", sf2, mid2]
        r = subprocess.run(cmd, env=env, capture_output=True, text=True)
        if r.returncode != 0 or not os.path.exists(wav):
            raise SystemExit(f"fluidsynth 失败（rc={r.returncode}）：{' '.join(cmd)}\n"
                             f"{(r.stderr or r.stdout)[-2000:]}")
        pcm, sr = read_wav_pcm(wav)
    finally:
        shutil.rmtree(tmp, ignore_errors=True)
    out = downsample_2x(pcm, sr)
    log(f"    [sf2] {os.path.basename(sf2)}: velocity 改 {chg}/{len(scaled)} 个音"
        f"（全局缩 {shrink:.3f}），{len(pcm)/sr:.1f}s@{sr} → {len(out)/SR:.1f}s@{SR}")
    return out


def note_list(tracks, t2s, melody_tracks):
    """把事件流展开成 (t0, dur, midi, vel, ch)（同通道同音高的 on/off 配对）。"""
    out = []
    for ti, evs in enumerate(tracks):
        on = {}
        for tick, kind, ch, a, b in evs:
            if kind == "on":
                on.setdefault((ch, a), []).append((t2s(tick), b))
            elif kind == "off":
                q = on.get((ch, a))
                if q:
                    t0, vel = q.pop(0)
                    out.append((t0, max(0.03, t2s(tick) - t0), a, vel, ch))
        for (ch, a), q in on.items():                          # 没有 off 的（少见）：给 0.5 s
            for t0, vel in q:
                out.append((t0, 0.5, a, vel, ch))
    return sorted(out)


# ---------------------------------------------------------------- 原曲循环体
def tempo_at(tempos, tick):
    """→ 该 tick 处的 tempo（µs/四分音符）。**不能取第一条**：这些同人 MIDI 开头常有
    一条显式 120 BPM 的初始化事件，真正的曲子 tempo 在后面（例 th18_03 主段 = 170.1 BPM）。"""
    us = tempos[0][1]
    for t, u in tempos:
        if t <= tick:
            us = u
        else:
            break
    return us


def midi_bpm(tempos, tick=0):
    return 60e6 / tempo_at(tempos, tick)


def source_loop(name):
    """→ (intro_bytes, track_bytes, 采样率, 声道数, 循环体 PCM 的 numpy 单声道视图)。"""
    for p, what in ((FMT, "thbgm.fmt"), (PCM, "thbgm.dat")):
        if not os.path.exists(p):
            raise SystemExit(f"缺 {what}（{os.path.relpath(p, REPO)}）—— fine_align 要拿**原曲循环体**"
                             f"做和声对齐，没有它就出不了素材。从游戏本体拷一份进 local/。")
    d = open(FMT, "rb").read()
    for i in range(len(d) // FMT_RECORD):
        r = d[i * FMT_RECORD:(i + 1) * FMT_RECORD]
        fn = r[0:8].split(b"\0")[0].decode("ascii", "replace").rstrip(". ")
        if fn != name:
            continue
        off, pre, intro, size = struct.unpack_from("<IIiI", r, 16)
        if not (0 < intro < size):
            raise SystemExit(f"{name}: fmt 的 intro/track 不自洽")
        f = open(PCM, "rb")
        f.seek(off + intro)
        raw = f.read(size - intro)
        a = np.frombuffer(raw, dtype="<i2").astype(np.float32) / 32768.0
        return intro, size, 44100, 2, a.reshape(-1, 2)
    raise SystemExit(f"{FMT} 里没有 {name}")


def env10ms(x, sr=22050):
    hop = int(sr * 0.01); win = int(sr * 0.03)
    n = (len(x) - win) // hop
    if n < 8:
        return np.zeros(8, dtype=np.float32)
    m = np.lib.stride_tricks.sliding_window_view(x[:n * hop + win], win)[::hop]
    e = np.sqrt((m.astype(np.float64) ** 2).mean(axis=1))
    return 20 * np.log10(e + 1e-9)


def align_offset(rendered, src_loop, max_lag_s=8.0):
    """把渲染结果与原曲循环体的能量包络互相关 → 最佳延迟（秒，正值 = 渲染要往后挪）。"""
    r = env10ms(rendered.mean(axis=1))
    s = env10ms(src_loop[::2, 0])                    # 原曲 44100 → 22050
    n = min(len(r), len(s))
    if n < 50:
        return 0.0
    r = (r[:n] - r[:n].mean()) / (r[:n].std() + 1e-9)
    s = (s[:n] - s[:n].mean()) / (s[:n].std() + 1e-9)
    L = int(max_lag_s * 100); best = (-2.0, 0)
    for lag in range(-L, L + 1):
        x, y = (r[:n - lag], s[lag:]) if lag >= 0 else (r[-lag:], s[:n + lag])
        if len(x) < 40:
            continue
        c = float((x * y).mean())
        if c > best[0]:
            best = (c, lag)
    return best[1] * 0.01, best[0]


def _chroma_frames(x, rate=SR, hop_s=0.05, win_s=0.2, lo=60.0, hi=2000.0):
    """→ (起始秒数组, chroma 矩阵 n×12，逐帧 L2 归一)。`x` = 单声道 float。"""
    N = int(win_s * rate)
    if N < 64 or len(x) < N + 64:
        return None, None
    hop = int(hop_s * rate)
    win = np.hanning(N).astype(np.float32)
    fr = np.fft.rfftfreq(N, 1.0 / rate)
    sel = (fr >= lo) & (fr <= hi)
    idx = np.arange(len(fr))[sel]
    # 每个频点落到哪个半音（MIDI 音级 % 12）
    mid = np.round(69 + 12 * np.log2(np.maximum(fr[sel], 1e-9) / 440.0)).astype(int) % 12
    t0 = 0.0
    n = 1 + (len(x) - N) // hop
    C = np.zeros((n, 12), np.float32)
    for i in range(n):
        seg = x[i * hop:i * hop + N] * win
        F = np.abs(np.fft.rfft(seg))[sel]
        for b in range(12):
            C[i, b] = F[mid == b].sum()
    C /= (np.linalg.norm(C, axis=1, keepdims=True) + 1e-9)
    return C


def fine_align(rendered, src_loop, start_est_s, max_lag_s=0.8):
    """在 `start_est_s`（= 按原曲 intro 长度取的小节线）附近滑动，找与**原曲循环体**最贴的起点。

    返回 (lag 秒, 相关系数)。判据 = **chroma（和声）时间序列**的逐帧余弦相似度均值。

    ★ 2026-09-15（用户："配对没问题，还是时间对不上"）：
      素材是**同一首曲子、同一条时间轴**上的重编曲（MIDI 速度实测 = 原曲），
      所以相位**对得齐** —— DLL 侧据此开「同时间点 seek」（TH13 原版那套）。
      而对齐精度决定 seek 之后两路是否真的同相：小节线只能给到 ±半小节
      （170 BPM 下 ≈ ±0.7 s），必须再用相关微调。

    ★ 为什么判据用 **chroma 而不是能量包络**：重编曲把音色/配器全换了（还压了主旋律、抬了贝斯），
      包络相关只有 0.24–0.73（对比度低、易被鼓组带偏）；而**和声进行没变** ——
      AUDIT §CU-4 实测 chroma 时间序列相关 0.928、最佳 lag ≈ 0。两者都要过"同一小节线附近"这一关，
      chroma 的峰更尖。

    ⚠ 本函数取代了旧的 `align_offset(rendered, loop, max_lag_s=bar*2)`：那个写法拿
      **渲染结果的开头**去和循环体相关，而正确延迟 ≈ +intro 秒（几十秒），远超 ±2 小节的搜索范围，
      永远搜不到（所以旧 `--corr-align` 一直没敢开）。现在把搜索**锚在估计起点上**，范围 ±0.8 s（< 半小节：和弦循环音乐每隔几小节就重复一次，窗口一放宽就会挑到重复峰）。
    """
    # 只算"估计起点 ±(搜索范围 + 判据窗口)"这一段的 chroma —— 全曲算太慢（458 s 的曲子要几万次 FFT）
    # ★★ 单位必须自洽：chroma 帧的 hop 是 **50 ms**（`_chroma_frames(hop_s=0.05)`）。
    #   上一版按 10 ms 算（`*100`）⇒ 锚点偏到 2 倍、搜索窗变 ±7.5 s，正好撞上"4 小节一循环"
    #   的假峰（实测 `th18_03` 的起点因此偏了 5.7 s ≈ 4 小节）。见 AUDIT §CW。
    hop = int(0.05 * SR)
    i0 = int(round(start_est_s / 0.05))
    L = int(round(max_lag_s / 0.05))
    lo_f = max(0, i0 - L - 4)
    hi_f = i0 + L + 600 + 4                            # 判据窗口 30 s（600 帧）
    a = lo_f * hop
    b = min(len(rendered), hi_f * hop + int(0.2 * SR))
    if b - a < int(0.4 * SR):
        return 0.0, float("nan")
    Cr = _chroma_frames(rendered[a:b].mean(axis=1))
    Cs = _chroma_frames(src_loop[::2, 0])              # 原曲 44100 → 22050
    if Cr is None or Cs is None:
        return 0.0, float("nan")
    n_use = min(600, len(Cs), len(Cr))                 # 30 s 判据窗口
    Cs = Cs[:n_use]
    base = i0 - lo_f                                   # 渲染帧数组里"估计起点"的下标
    best_c, best_lag = -2.0, 0
    for lag in range(-L, L + 1):
        k = base + lag
        if k < 0 or k + n_use > len(Cr):
            continue
        c = float((Cr[k:k + n_use] * Cs).sum(axis=1).mean())
        if c > best_c:
            best_c, best_lag = c, lag
    return best_lag * 0.05, best_c


def write_wav(path, pcm_bytes, rate=SR, channels=2):
    with wave.open(path, "wb") as w:
        w.setnchannels(channels); w.setsampwidth(2); w.setframerate(rate)
        w.writeframes(pcm_bytes)


def main(argv=None):
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    ap.add_argument("--stem", action="append", default=None,
                    help="曲目 stem（可多次，例 th18_03）；默认 = MANIFEST 里全部")
    ap.add_argument("--melody-gain", type=float, default=0.35, help="主旋律增益（默认 0.35 ≈ −9 dB）")
    ap.add_argument("--bass-gain", type=float, default=1.6, help="贝斯增益（默认 1.6）")
    ap.add_argument("--out", default=OUT_DIR)
    ap.add_argument("--peak", type=float, default=0.92, help="峰值归一目标")
    ap.add_argument("--dry-run", action="store_true", help="只解析 + 报表，不渲染")
    ap.add_argument("--no-align", action="store_true",
                    help="不做细对齐（默认**做**：小节线 ±1 小节三条锚点各搜 ±0.8 s，取 chroma 相关最高者）")
    ap.add_argument("--xfade", action="store_true", help="接缝加 500 ms 交叉淡化（默认不加）")
    ap.add_argument("--render", choices=("sf2", "naive"), default="sf2",
                    help="音源：sf2 = GM SoundFont 渲染（默认，需 fluidsynth + .sf2）；"
                         "naive = 老的自写合成器（兜底）")
    ap.add_argument("--sf2", default=None,
                    help="SoundFont 路径（默认自动找 local/th18.v1.00a/sf2/*.sf2，优先 thfont）")
    ap.add_argument("--fluidsynth", default=None,
                    help="fluidsynth 可执行文件（默认 PATH → local/vendor/fluidsynth-*/usr/bin/）")
    a = ap.parse_args(argv)

    # ── 音源（sf2）先解析好：缺 fluidsynth / 缺 sf2 就在**开始渲染前**停，别跑到一半
    fs = fs_lib = sf2 = None
    if a.render == "sf2" and not a.dry_run:
        fs, fs_lib = find_fluidsynth(a.fluidsynth)
        if not fs:
            raise SystemExit(
                "找不到 fluidsynth。三条路：\n"
                "  ① sudo apt-get install -y fluidsynth\n"
                "  ② 无 sudo：apt-get download fluidsynth libfluidsynth3 libsndfile1 libinstpatch-1.0-2 "
                "libglib2.0-0 libjack-jackd2-0 libsdl2-2.0-0 libpulse0 libflac8 libvorbis0a libvorbisenc2 "
                "libopus0 libogg0 libdecor-0-0 libasyncns0 && for d in *.deb; do dpkg-deb -x $d "
                "local/vendor/fluidsynth-2.2.5; done\n"
                "  ③ 退回老路径：--render=naive")
        sf2 = find_sf2(a.sf2)
        if not sf2:
            raise SystemExit(
                f"找不到 .sf2（{os.path.relpath(SF2_DIR, REPO)}/ 下没有 RIFF/sfbk 文件）。\n"
                "  本机用的是 GeneralUser GS v2.0.3（来源 URL + sha256 见同目录 SOURCE.txt）")
        print(f"音源：{os.path.basename(sf2)}（{os.path.getsize(sf2)/1e6:.1f} MB，"
              f"sha256 {(hashlib.sha256(open(sf2,'rb').read()).hexdigest())[:16]}…） + {fs}")

    # 默认曲目表 = **内部曲名**全集（取自 musiccmt.txt 的一手表，按音乐室 No. 排序）。
    # ★ 不要用 MIDI 目录里的文件名当默认列表：那批文件按**音乐室编号**命名，
    #   既有 `th18_02`（无对应内部曲名）又缺 `th18_18`（Ex boss）—— 见 AUDIT §CU。
    tbl = music_room_numbers()
    if not tbl:
        raise SystemExit(f"读不出 {MUSICCMT}（需要游戏自带的音乐室注释文件）")
    stems = a.stem or [k for k, _ in sorted(tbl.items(), key=lambda kv: kv[1])]
    os.makedirs(a.out, exist_ok=True)
    env_cache = {}
    print(f"{'曲':<9}{'轨':>4}{'音符':>7}{'主旋律轨':>9}{'MIDI 时长':>10}{'循环体':>9}{'起点':>9}{'r':>7}  文件")
    wrote = 0
    for stem in stems:
        # stem = **内部曲名**（th18_03…）；MIDI 文件按**音乐室编号**命名（th18_02…）
        mi = midi_index_for(stem)
        if mi is None:
            print(f"  ★ 跳过 {stem}：dat/musiccmt.txt 里没有这个内部曲名（不是本作曲目）")
            continue
        mid = os.path.join(MIDI_DIR, f"th18_{mi:02d}.mid")
        if not os.path.exists(mid):
            print(f"  ★ 跳过 {stem}：没有 {os.path.relpath(mid, REPO)}（音乐室 No.{mi}）")
            continue
        tracks, ppq, tempos = parse_smf(mid)
        t2s = ticks_to_sec(tempos, ppq)
        mel, stats = pick_melody(tracks, t2s)
        prog = {}
        for evs in tracks:
            for tick, kind, ch, val, _ in evs:
                if kind == "prog":
                    prog[ch] = val
        notes = note_list(tracks, t2s, mel)
        if not notes:
            print(f"  ★ {stem}: 一个音符都没解析出来")
            continue
        midi_secs = max(n[0] + n[1] for n in notes)
        src_name = None
        # MIDI 文件名 th18_03 → fmt 里叫 "th18_03"
        try:
            intro, tsize, ssr, sch, loop = source_loop(stem)
        except SystemExit as e:
            print(f"  ★ {stem}: {e}")
            continue
        loop_secs = len(loop) / ssr
        if a.dry_run:
            print(f"{stem:<9}{len(tracks):>4}{len(notes):>7}{str(sorted(mel)):>9}{midi_secs:>9.2f}s"
                  f"{loop_secs:>8.2f}s{'—':>9}{'—':>7}  (dry-run)")
            continue
        # 渲染时把主旋律轨单独标记（按通道号集合；naive 路径用它，sf2 路径按**轨**判）
        mel_ch = set()
        for ti in mel:
            for tick, kind, ch, val, _ in tracks[ti]:
                if kind in ("on", "off"):
                    mel_ch.add(ch)
        if a.render == "sf2":
            rendered = render_sf2(mid, sf2, fs, fs_lib, mel, prog, notes,
                                  a.melody_gain, a.bass_gain)
        else:
            rendered = render_naive(notes, prog, midi_secs + 0.5, mel_ch, a.melody_gain,
                                    a.bass_gain, seed=hash(stem) & 0xFFFF)
        # ── 起点：**照原曲的 intro 长度**（一手：fmt 的 intro_size；同人 MIDI 用的是原曲速度，
        #    实测 MIDI 主 tempo 170.1 BPM ↔ 原曲能量自相关 0.360 s ⇒ 两边时间轴同构），
        #    再按拍/小节对齐（4/4）⇒ 起点落在小节线上。
        intro_secs = intro / (ssr * 4)                # intro 是**字节**（44100 Hz × 2ch × 2B = 176400 B/s）
        # ★ 小节对齐要用**该处**的 tempo：tick ≈ intro_secs 处的那个
        tick_at_intro = int(intro_secs * ppq * 1e6 / tempo_at(tempos, 0)) if tempos else 0
        beat = 60.0 / midi_bpm(tempos, tick_at_intro)
        bar = beat * 4
        start_sec = round(intro_secs / bar) * bar
        # ★ 默认做**细对齐**（±0.8 s）：小节线估计 ± chroma 微调 ⇒ 素材 t=0 = 原曲循环点。
        #   `--no-align` 关掉（只按小节线，退回"只靠生成器推断"）。
        # ★★ 2026-10-19（3 首试点时发现）：**小节线估计本身可能整整差一小节**——
        #   `round(intro_secs/bar)` 在 intro 落在"半小节"附近时会翻到另一侧，而 MIDI 的 intro
        #   普遍比游戏里的短 ~0.7 s（三首实测 −0.72/−0.75/−0.73 s），两者叠加就能把真答案推到
        #   ±0.8 s 搜索窗**外面**：th18_06 因此被截在 +8.03 s（真值 ≈ +5.76 s），出盘后与原曲
        #   差 **2.27 s** ⇒ "同时间点 seek" 时会串到另一小节上（实测 chroma@0 = 0.78，修正后 0.93）。
        #   ⇒ 修法：把 **±1 小节**那三条小节线都当锚点，各跑一遍 `fine_align`，取 chroma 相关
        #     最高的那条（判据不变，只是候选从 1 条变 3 条；±0.8 s 的窗口宽度也不变）。
        if a.no_align:
            lag, r_corr = 0.0, float("nan")
        else:
            cands = []
            for k in (-1, 0, 1):
                est = max(0.0, round(intro_secs / bar) * bar + k * bar)
                lg, rr = fine_align(rendered, loop, est)
                cands.append((est, lg, rr if rr == rr else -9.0))
            start_est, lag, r_corr = max(cands, key=lambda c: c[2])
            if r_corr < -8.5:                                # 全 NaN ⇒ 按原估计
                r_corr = float("nan")
            start_sec = start_est + lag
            if start_est != cands[1][0]:
                print(f"    [align] 小节线估计 {cands[1][0]:+.2f}s → 换用 "
                      f"{start_est:+.2f}s（r={r_corr:.3f}，±1 小节重挑）")
        start = max(0, int(start_sec * SR))
        want = int(round(loop_secs * SR))                        # ★ 严格 = 原曲循环体长度
        if start < 0:                                            # 渲染比原曲早 ⇒ 前面补静音
            rendered = np.concatenate([np.zeros((-start, 2), dtype=np.float32), rendered])
            start = 0
        if len(rendered) < start + want:                         # 不够就首尾相接补满
            reps = int(math.ceil((start + want) / max(1, len(rendered) - start))) + 1
            rendered = np.concatenate([rendered] * max(2, reps))
        seg = rendered[start:start + want].copy()
        # 接缝：默认**不动**（同人 MIDI 的循环段本来就接得上；交叉淡化会把尾巴混进开头）
        xf = 0 if not a.xfade else min(int(0.5 * SR), len(seg) // 4)
        if xf > 0:
            w = np.linspace(0, 1, xf, dtype=np.float32)[:, None]
            seg[:xf] = seg[:xf] * w + seg[-xf:] * (1 - w)
        peak = float(np.abs(seg).max()) or 1.0
        seg *= (a.peak / peak)
        pcm = (np.clip(seg, -1, 1) * 32767.0).astype("<i2").tobytes()
        name = f"{stem}b_rearr.wav"
        path = os.path.join(a.out, name)
        write_wav(path, pcm)
        wrote += 1
        # 主旋律占比（按"音符能量"= vel²×时长）

        def share(chset):
            tot = sum((v / 127.0) ** 2 * d for t, d, m, v, ch in notes)
            mel_e = sum((v / 127.0) ** 2 * d for t, d, m, v, ch in notes if ch in chset)
            return mel_e / max(tot, 1e-9)
        print(f"{stem:<9}{len(tracks):>4}{len(notes):>7}{str(sorted(mel)):>9}{midi_secs:>9.2f}s"
              f"{loop_secs:>8.2f}s{start_sec:>+8.2f}s{r_corr:>7.3f}  {os.path.relpath(path, REPO)}"
              f"   [{a.render} | BPM {midi_bpm(tempos, tick_at_intro):.1f} 主旋律能量占比 "
              f"{share(mel_ch)*100:.0f}%→"
              f"{share(mel_ch)*100*a.melody_gain**2/(1-share(mel_ch)+share(mel_ch)*a.melody_gain**2):.0f}%]")
    print(f"\n写出 {wrote} 个文件 → {os.path.relpath(a.out, REPO)}"
          f"（切模式：mode.txt 写 `rearr`；同步：bash local/sync_modkit_dll.sh）")
    return 0 if wrote or a.dry_run else 2


if __name__ == "__main__":
    sys.exit(main())
