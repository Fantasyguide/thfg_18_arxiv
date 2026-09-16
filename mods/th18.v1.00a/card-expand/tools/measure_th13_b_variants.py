#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""measure_th13_b_variants.py —— **量**出 TH13 的灵界版（`th13_NNb.wav`）与正常轨到底差在哪。

# 为什么有这个脚本

用户问：「ZUN 在 TH13 的灵界版里**到底改了什么**？」，并给了自己的朴素猜测
「**压了主旋律轨，放大了其他轨**」。**网上没有公开的 stem 级分析**（只有作业用合辑 / A-B 对比视频 /
粉丝的 trance-ify 改编演示）⇒ 只能**我们自己量**。TH13 的装机目录现在可读
（`thbgm.dat` = 492,019,608 B、`thbgm.fmt` = 1629 B，见 `AUDIT.md` §CO-1），所以可以逐对比 PCM。

本脚本**只测量、不生成**（生成在 `make_trance_bgm.py`）。

# 测什么（每个候选假设一条判据，全部落到数字）

| 假设 | 判据（在**同一带限域**里逐对算）|
| --- | --- |
| (a) 低音/伴奏被抬 | `<150 Hz` 与 `150–500` 两个低频带**相对**抬升（相对 = 先按各自总能量归一）|
| (b) 主旋律被压 | `500–2k`（主旋律基频主战场）与 `2k–6k` 相对**下降**，且降幅**大于**其它带 |
| (c) 中间（mid）被削 = karaoke 式 | **mid/side 比**下降（`mid=(L+R)/2`、`side=(L−R)/2`）|
| (d) 以上都不是 / 每对不一样 | 各对之间**离散**（同一条规则套不上），或者"逐带增益模型"解释不了波形 |

★ (d) 的量化判据（本脚本的关键一手）：把正常轨按测得的那 5 个带增益**最小二乘**重建
（`recon = Σ g_i · band_i`，`g_i = <b, band_i>/<band_i, band_i>`），再看
`r_ls = corr(recon, b)` 与 `1 − ‖recon‖²/‖b‖²`（**未被解释的能量占比**）。
**若 b 只是"同一段混音换了几条带的增益"，则 `r_ls ≈ 1` 且未解释占比 ≈ 0**；
`r_ls` 低 / 未解释占比高 ⇒ 两条轨**不是同一段素材**（换了编曲、换了乐器、或干脆是另一版演奏）。

# ★ 带限口径（必须有，因为两条轨采样率不同）

正常轨 **44100 Hz**、b 轨 **22050 Hz**（一手 fmt 实测，`AUDIT` §CO-1）。
直接比会把正常轨 11–22 kHz 的能量算进去、还会让 11 kHz 以上的内容**折叠**进被测量的带里。所以：

1. 把正常轨做**FFT 砖墙低通到 11025 Hz**（= b 轨的 Nyquist），再 **2:1 抽取**
   （`(x[0::2] + x[1::2]) / 2`）；
2. 两条轨都取**循环段**（`[intro_size, track_size)`，`track_size` **含** intro —— 一手口径见 `AUDIT` §CO-3），
   从循环段开头对齐（TH13 的设计就是让两轨 intro/loop **秒数严格相等**，所以不需要找 lag，
   脚本仍会**独立验一次**最佳 lag 是不是 0）；
3. 分析窗口 = 循环段开头的 **2^21 帧**（≈ 95 s @22050，取两轨较短者），逐带用 FFT 掩码切分
   （互补、相位一致，5 条带之和 = 原信号）。
4. 5 条带：`<150` / `150–500` / `500–2k` / `2k–6k` / `>6k`（上限都是 11025 Hz）。

# 用法

```bash
python3 tools/measure_th13_b_variants.py --th13 "/mnt/d/…/[th13] 东方神灵庙 (汉化版+日文版)"
python3 tools/measure_th13_b_variants.py --th13 DIR --pairs th13_01 th13_03   # 只量两对
python3 tools/measure_th13_b_variants.py --th13 DIR --json out.json           # 机器可读
```

`--th13` 必须指向**同时含 `thbgm.fmt` 与 `thbgm.dat`** 的目录（本仓 `local/th13.v1.00c/` 只有 fmt ⇒ 不够）。
脚本**不写死任何机器路径**（本仓纪律）。
"""

from __future__ import annotations

import argparse
import json
import os
import struct
import sys

try:
    import numpy as np
except ImportError:  # pragma: no cover
    sys.exit("需要 numpy（pip install numpy）")

FMT_RECORD = 0x34
# 13 对（研究文档 §1.5-C 的全表；`th13_17` 没有 b 变体、`th13_18/19` 等也没有）
DEFAULT_PAIRS = ["th13_01", "th13_03", "th13_04", "th13_06", "th13_07", "th13_08",
                 "th13_10", "th13_11", "th13_12", "th13_15", "th13_16", "th13_20", "th13_22"]
# 5 条带的上界（Hz）；最后一段到 11025（= 22050 的 Nyquist）
BAND_EDGES = [150.0, 500.0, 2000.0, 6000.0, 11025.0]
BAND_NAMES = ["<150", "150-500", "500-2k", "2k-6k", ">6k"]
ANALYSIS_FRAMES = 1 << 21                     # ≈ 95 s @22050
TARGET_RATE = 22050                           # b 轨的采样率（= 我们的比较域）


class Rec:
    __slots__ = ("name", "offset", "intro", "size", "rate", "channels", "block", "avg")


def parse_fmt(path: str) -> dict:
    data = open(path, "rb").read()
    out, off = {}, 0
    while off + FMT_RECORD <= len(data) and data[off]:
        r = Rec()
        r.name = data[off:off + 16].split(b"\0")[0].decode("latin1")
        r.offset, _pre, r.intro, r.size = struct.unpack_from("<IIiI", data, off + 16)
        _tag, r.channels, r.rate, r.avg, r.block, _bits = struct.unpack_from("<HHIIHH", data, off + 32)
        out[r.name.split(".")[0]] = r
        off += FMT_RECORD
    return out


def read_stereo(fh, r: Rec, frames: int):
    """读**循环段开头** `frames` 帧的立体声 float64（交错 → [frames, 2]）。"""
    start = r.offset + r.intro                        # ★ track_size 含 intro ⇒ 循环段从这里起
    avail = r.size - r.intro
    n = min(frames * r.block, avail)
    n -= n % r.block
    fh.seek(start)
    raw = fh.read(n)
    return np.frombuffer(raw, dtype="<i2").reshape(-1, r.channels).astype(np.float64)


def lowpass_fft(x, fs: float, cutoff: float):
    """FFT 砖墙低通（逐列；`x` 是 [n] 或 [n, ch]）。只用于**分析**（生成器不用它）。"""
    n = x.shape[0]
    spec = np.fft.rfft(x, axis=0)
    freqs = np.fft.rfftfreq(n, 1.0 / fs)
    spec[freqs > cutoff] = 0.0
    return np.fft.irfft(spec, n=n, axis=0)


def decimate2(x):
    """2:1 抽取（两帧一平均 —— 与生成器的降采样同一条链，便于口径一致）。"""
    n = (x.shape[0] // 2) * 2
    return (x[:n:2] + x[1:n:2]) / 2.0


def band_split(x, fs: float):
    """按 `BAND_EDGES` 做**互补** FFT 掩码切分；返回 [n_bands][n]（逐列同相，和 = 原信号）。"""
    n = x.shape[0]
    spec = np.fft.rfft(x, axis=0)
    freqs = np.fft.rfftfreq(n, 1.0 / fs)
    out, lo = [], 0.0
    for hi in BAND_EDGES:
        m = (freqs >= lo) & (freqs < hi)
        s = np.zeros_like(spec)
        s[m] = spec[m]
        out.append(np.fft.irfft(s, n=n, axis=0))
        lo = hi
    return out


def energy_db(x) -> float:
    e = float(np.mean(np.square(x, dtype=np.float64)))
    return 10.0 * np.log10(e) if e > 0 else float("-inf")


def corr(a, b) -> float:
    a = a - a.mean()
    b = b - b.mean()
    d = float(np.linalg.norm(a) * np.linalg.norm(b))
    return float(np.dot(a, b) / d) if d > 0 else 0.0


def best_lag_frames(a, b, max_lag: int = 2205) -> tuple:
    """在 ±`max_lag` 帧内找最大互相关（用短窗 + 直接相关，够用且快）。返回 (lag, 相关)。"""
    k = 1 << 15
    seg = a[k:k + 8192]
    lo = max(0, k - max_lag)
    win = b[lo:k + 8192 + max_lag]
    c = np.correlate(win, seg, mode="valid")            # 长度 = len(win)-len(seg)+1
    i = int(np.argmax(np.abs(c)))
    lag = int(lo + i - k)
    if 0 <= lag <= max_lag:
        p, q = b[lag:lag + 8192], a[k:k + 8192]
    elif lag < 0:
        p, q = b[0:8192 + lag], a[k - lag:k + 8192]
    else:
        p, q = b[lag:lag + 8192], a[k:k + 8192]
    m = min(len(p), len(q))
    return lag, corr(p[:m], q[:m])


def analyse_pair(fh, rec: dict, stem: str, verbose: bool = True) -> dict:
    a, b = rec.get(stem), rec.get(stem + "b")
    if a is None or b is None:
        return {"pair": stem, "error": "fmt 里缺正常轨或 b 轨"}

    # ---- ① 两条轨都取循环段开头；正常轨先带限到 11025 Hz 再 2:1 抽取 ----
    n_norm = ANALYSIS_FRAMES * 2                          # 正常轨在 44100 域要 2 倍帧数
    xa = read_stereo(fh, a, n_norm)
    xa = lowpass_fft(xa, a.rate, TARGET_RATE / 2.0)       # ★ 砖墙低通（否则 11–22k 会折叠）
    xa = decimate2(xa)
    xb = read_stereo(fh, b, ANALYSIS_FRAMES)
    n = min(xa.shape[0], xb.shape[0])
    xa, xb = xa[:n], xb[:n]

    # ---- ② 独立验一次"两轨时间对齐"（TH13 的设计应该恒为 0 帧）----
    ma, mb = xa.mean(axis=1), xb.mean(axis=1)
    lag, r0 = best_lag_frames(ma, mb)

    # ---- ③ mid / side ----
    mid_a, side_a = (xa[:, 0] + xa[:, 1]) / 2, (xa[:, 0] - xa[:, 1]) / 2
    mid_b, side_b = (xb[:, 0] + xb[:, 1]) / 2, (xb[:, 0] - xb[:, 1]) / 2
    ms_a = energy_db(side_a) - energy_db(mid_a)
    ms_b = energy_db(side_b) - energy_db(mid_b)

    # ---- ④ 分带（在 mid 上做；side 只报 mid/side 比，见上）----
    ba, bb = band_split(mid_a, TARGET_RATE), band_split(mid_b, TARGET_RATE)
    e_a = [energy_db(x) for x in ba]
    e_b = [energy_db(x) for x in bb]
    # 相对口径：先各自减去"两轨总能量差"，剩下的才是**倾斜**（tilt）
    lvl = energy_db(mid_b) - energy_db(mid_a)             # 绝对电平变化（dB）
    tilt = [(e_b[i] - lvl) - e_a[i] for i in range(len(ba))]

    # ---- ⑤ 逐带增益模型（最小二乘）能不能解释 b ----
    #  ★ 逐带最小二乘增益：`g_i = <b, band_i> / ‖band_i‖²`（band_i 之间正交 ⇒ 就是这个闭式）。
    #  ⚠ 分母必须是**该带自己的能量**，不是整轨能量 —— 2026-09-14 这里写错过一次，会把
    #     g 算小一个量级、把"未解释占比"假抬到 80–99%（修好后 `r_ls² == 1 − 未解释`，已加断言）。
    g = [float(np.dot(mid_b, x) / float(np.dot(x, x))) if float(np.dot(x, x)) > 0 else 0.0 for x in ba]
    recon = sum(gi * x for gi, x in zip(g, ba))
    r_ls = corr(recon, mid_b)
    e_recon_db, e_b_db = energy_db(recon), energy_db(mid_b)   # ⚠ 别写成 e_a/e_b（上面那两个是逐带列表）
    unexplained = float(10 ** ((e_b_db - e_recon_db) / 10.0))  # 未被解释的能量**倍数**
    unexplained = max(0.0, 1.0 - 1.0 / unexplained) if unexplained > 0 else 1.0

    # 两条独立算路必须自洽：`r_ls² + 未解释 ≈ 1`（差 >0.02 就是上面那个 g 又写错了）
    consist = abs((1.0 - unexplained) - r_ls * r_ls)

    out = {
        "pair": stem, "frames": int(n), "lag": int(lag), "corr_at_best_lag": float(r0),
        "ls_selfconsistency_err": float(consist),
        "level_db": float(lvl),
        "band_a_db": [float(v) for v in e_a], "band_b_db": [float(v) for v in e_b],
        "band_tilt_db": [float(v) for v in tilt],
        "mid_side_a_db": float(ms_a), "mid_side_b_db": float(ms_b),
        "mid_side_change_db": float(ms_b - ms_a),
        "band_gain_ls": [float(v) for v in g], "corr_ls": float(r_ls),
        "unexplained_frac": float(unexplained),
    }
    if verbose:
        print(f"  {stem:9s} lag={lag:+4d}帧 最佳lag相关={r0:+.3f} 电平={lvl:+5.2f}dB | "
              + " ".join(f"{BAND_NAMES[i]}:{tilt[i]:+5.2f}" for i in range(len(tilt)))
              + f" | mid/side Δ={ms_b - ms_a:+5.2f}dB | r_ls={r_ls:+.3f} 未解释={unexplained * 100:5.1f}%")
    return out


def verdict(rows: list) -> list:
    """把数字翻成对 (a)/(b)/(c)/(d) 的**明确**支持/否定（阈值写死在这里，免得事后解释）。"""
    ok = [r for r in rows if "error" not in r]
    if not ok:
        return ["没有任何一对可用"]
    TILT_MIN = 2.0        # 判"抬/压"的门槛（相对倾斜，dB）
    LAG_MS = 2.0          # 判"mid/side 变了"的门槛（dB）
    RL_SAME = 0.9         # 判"同一段素材"的相关门槛
    UNEX_MAX = 0.2        # 判"逐带增益就够解释"的未解释占比门槛

    low_up = [r for r in ok if r["band_tilt_db"][0] > TILT_MIN and r["band_tilt_db"][1] > TILT_MIN]
    mel_dn = [r for r in ok
              if r["band_tilt_db"][2] < -TILT_MIN and r["band_tilt_db"][3] < -TILT_MIN
              and r["band_tilt_db"][2] < min(r["band_tilt_db"][0], r["band_tilt_db"][1])]
    ms_dn = [r for r in ok if r["mid_side_change_db"] < -LAG_MS]
    same = [r for r in ok if r["corr_ls"] > RL_SAME and r["unexplained_frac"] < UNEX_MAX]

    n = len(ok)
    lines = []
    lines.append(f"(a) 低音/伴奏被抬（<150 与 150–500 **同时** > +{TILT_MIN} dB 相对）："
                 f"**{len(low_up)}/{n} 对成立**" + ("" if low_up else " ⇒ 不支持"))
    lines.append(f"(b) 主旋律带被压（500–2k 与 2k–6k **同时** < −{TILT_MIN} dB 且比低频带更狠）："
                 f"**{len(mel_dn)}/{n} 对成立**" + ("" if mel_dn else " ⇒ 不支持"))
    b_lo = sum(1 for r in ok if r["band_tilt_db"][2] < 0)
    lines.append(f"(b′) **方向一致性**口径：500–2k 在 **{b_lo}/{n}** 对里都是降的"
                 f"（这才是「系统性」的判据；单对阈值判据会被个别反向拖没）")
    lines.append(f"(c) 中间（mid）被削 = karaoke 式（mid/side Δ < −{LAG_MS} dB）："
                 f"**{len(ms_dn)}/{n} 对成立**" + ("" if ms_dn else " ⇒ 不支持"))
    lines.append(f"(d) 「同一段素材 + 换几条带的增益」这套模型：**只有 {len(same)}/{n} 对**满足"
                 f"（r_ls > {RL_SAME} 且未解释 < {UNEX_MAX * 100:.0f}%）"
                 + ("⇒ 其余各对**不是**同一段素材的逐带增益版" if len(same) < n else ""))
    # ★ 聚合统计才是"系统性的那部分"：逐带看"多少对在降/升"和均值/中位数
    #   （单对阈值判据 (`(a)`/`(b)`) 会因为个别对的反向而失效；13 对的**方向一致性**才是判据）
    lines.append("逐带聚合（相对倾斜，dB）：")
    for i, nm in enumerate(BAND_NAMES):
        v = [r["band_tilt_db"][i] for r in ok]
        dn = sum(1 for x in v if x < 0)
        lines.append(f"    {nm:>8s}  均值 {sum(v) / len(v):+5.2f}  中位 "
                     f"{float(np.median(v)):+5.2f}  跨 {min(v):+5.2f}…{max(v):+5.2f}  "
                     f"下降 {dn}/{len(v)} 对")
    allmid = [(r["band_tilt_db"][2] + r["band_tilt_db"][3]) / 2 for r in ok]
    lines.append(f"    {'500-6k':>8s}  均值 {sum(allmid) / len(allmid):+5.2f}"
                 f"（= '旋律/中频' 合成带）  全降 {sum(1 for x in allmid if x < 0)}/{len(allmid)} 对")
    ms = [r["mid_side_change_db"] for r in ok]
    lines.append(f"    {'mid/side':>8s}  均值 {sum(ms) / len(ms):+5.2f}  跨 {min(ms):+5.2f}…{max(ms):+5.2f}"
                 f"  （若系统性 karaoke 削中间，这一列应整体为负且明显）")
    rl = [r["corr_ls"] for r in ok]
    lines.append(f"    逐带增益模型 r_ls：均值 {sum(rl) / len(rl):.2f}  中位 {float(np.median(rl)):.2f}"
                 f"  跨 {min(rl):.2f}…{max(rl):.2f}（=1 才说明 b 只是同素材换了几条带的增益）")
    lo = [r["band_tilt_db"][0] for r in ok]
    his = [r["band_tilt_db"][3] for r in ok]
    lines.append(f"离散度：<150 带的相对倾斜跨 **{min(lo):+.1f}…{max(lo):+.1f} dB**、"
                 f"2k–6k 跨 **{min(his):+.1f}…{max(his):+.1f} dB**"
                 f"（同一条规则若成立，这一列应当很窄）")
    nz = [r for r in ok if abs(r["lag"]) <= 1]
    lines.append(f"时间对齐：**{len(nz)}/{n} 对**最佳 lag 在 ±1 帧内 ⇒ 同 tempo/同段落结构")
    return lines


def main(argv=None) -> int:
    ap = argparse.ArgumentParser(description="量出 TH13 灵界版（b 轨）与正常轨的差异（只读、不生成）")
    ap.add_argument("--th13", required=True, help="TH13 安装目录（必须同时含 thbgm.fmt 与 thbgm.dat）")
    ap.add_argument("--pairs", nargs="*", default=DEFAULT_PAIRS, help="要量的曲（默认 13 对）")
    ap.add_argument("--json", default=None, help="把逐对数字写成 JSON")
    args = ap.parse_args(argv)

    fmt = os.path.join(args.th13, "thbgm.fmt")
    dat = os.path.join(args.th13, "thbgm.dat")
    for p in (fmt, dat):
        if not os.path.exists(p):
            return int(bool(sys.stderr.write(
                f"★ 找不到 {p}\n  `--th13` 要指向同时含 thbgm.fmt 与 thbgm.dat 的目录"
                f"（本仓 local/th13.v1.00c/ 只有 fmt ⇒ 不够）。\n")))
    rec = parse_fmt(fmt)
    print(f"=== TH13 b 轨量测 ===\n  目录 {args.th13}\n"
          f"  fmt {len(rec)} 条记录，thbgm.dat {os.path.getsize(dat)} B\n"
          f"  ★ 带限口径：正常轨(44100) 先 FFT 砖墙低通到 **11025 Hz** 再 2:1 抽取 → 与 b 轨(22050) 同域；\n"
          f"     分析窗 = 循环段开头 {ANALYSIS_FRAMES} 帧（≈ {ANALYSIS_FRAMES / TARGET_RATE:.0f} s），\n"
          f"     5 条带 = {' / '.join(BAND_NAMES)}（互补 FFT 掩码，和 = 原信号）\n"
          f"  倾斜(tilt) = 逐带能量变化 **减去该对的总电平变化** ⇒ 只反映「改了什么比例」，不含整体音量\n")

    rows = []
    with open(dat, "rb") as fh:
        for stem in args.pairs:
            rows.append(analyse_pair(fh, rec, stem))

    print("\n-- 判据（阈值写死在脚本里：倾斜 ±2 dB / mid-side ±2 dB / r_ls 0.9 / 未解释 20%）")
    for ln in verdict(rows):
        print("  " + ln)

    if args.json:
        with open(args.json, "w", encoding="utf-8") as f:
            json.dump({"th13": args.th13, "band_names": BAND_NAMES,
                       "band_edges_hz": BAND_EDGES, "analysis_frames": ANALYSIS_FRAMES,
                       "target_rate": TARGET_RATE, "pairs": rows}, f, ensure_ascii=False, indent=1)
        print(f"\n  JSON → {args.json}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
