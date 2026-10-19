#!/usr/bin/env python3
"""measure_band_energy.py —— 分频段能量 / 低频包络：**原曲 vs 我们的 `_rearr` vs 响度匹配版**。

    python3 tools/measure_band_energy.py                       # 4 首对照（12/13/14/18）+ th18_14 三方
    python3 tools/measure_band_energy.py --stems th18_14 --md /tmp/x.md --csv /tmp/x.csv
    python3 tools/measure_band_energy.py --norm-dir /tmp/lowfreq/norm   # 另存归一到 −14 LUFS 的 wav

## 为什么要「按同一 LUFS 归一」再比频谱
响度不同时，频段绝对电平天然差好几个 dB，直接比表格没有意义。本工具做两件事：

1. **相对量**（与响度无关）：每个频段能量占总能量的比例（dB）——回答"这首曲子的低频**占比**是不是本来就低"；
2. **绝对量**：把三方各自乘一个增益，使 **积分响度（BS.1770-4 / EBU R128，`measure_loudness.integrated_lufs`）
   都等于 `--target-lufs`（默认 −14.0）**，再报每段的 dBFS —— 回答"同样响的时候，谁的低频更少"。

## 频段
`sub<60 / 60–120 / 120–250 / 250–500 / 500–2k / 2k–8k / >8k`，用 **Welch PSD**（不是滤波器组）：
44.1 kHz 取 `nperseg=32768`（Δf 1.35 Hz）、22.05 kHz 取 `16384`（同 Δf）⇒ **sub<60 有 ~44 个频点**，
不会被 FFT 分辨率坑。能量 = ∫PSD df（单边、density 口径），总能量 = 全带积分。

★ **带宽口径**：我们的素材是 22050 Hz（Nyquist 11 kHz），原曲是 44100 Hz。
所以**所有**分频段统计都先统一到 **11 kHz 以内**（原曲用 8 阶 Butterworth 低通到 10.8 kHz 再算；
`>8k` 段 = 8–11 kHz）。原曲 11–22 kHz 的能量单独报一行，只作参考、不进对比表。

## 低频包络（kick 有没有被压）
对 `<120 Hz` 低通后的信号算：峰值、RMS、**crest = 峰−RMS**、包络的 99.9 百分位与中位数之差、
"重击占空比"（包络 ≥ p99.9−10 dB 的时间占比）、**每秒重击数**（find_peaks，间距 ≥0.1 s）。
crest 变小 / 占空比变大 ⇒ 低频被压扁（限幅/压缩），听感就是"kick 没劲"。

## 输入
- 原曲：`local/th18.v1.00a/thbgm.dat` 的**循环体**（`measure_loudness.source_loop`，一手 fmt 口径）；
- 我们的：`local/th18.v1.00a/trance_bgm/<stem>b_rearr.wav`（22050 Hz）；
- 响度匹配版：`--pilot-dir`（默认 `/tmp/rearr_loud`）里的同名 wav；缺就用本工具的
  `match_lufs()`（与上一轮试点**同一套**：软限幅 lookahead 5 ms / release 60 dB·s⁻¹ / ceiling −0.3 dBFS，
  迭代把积分响度顶到原曲的值）现算一份。
"""
import argparse
import csv
import json
import os
import sys

import numpy as np

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import measure_loudness as M                      # noqa: E402

try:
    from scipy.ndimage import maximum_filter1d, uniform_filter1d
    from scipy.signal import butter, find_peaks, sosfiltfilt, welch
except Exception:                                  # pragma: no cover
    raise SystemExit("需要 scipy（pip install scipy）")

BANDS = [("sub<60", 0.0, 60.0), ("60-120", 60.0, 120.0), ("120-250", 120.0, 250.0),
         ("250-500", 250.0, 500.0), ("500-2k", 500.0, 2000.0),
         ("2k-8k", 2000.0, 8000.0), (">8k", 8000.0, 11000.0)]
CAP = 10800.0                                      # 统一带宽上限（留 200 Hz 给低通过渡带）


# ---------------------------------------------------------------- 工具
def db(x):
    return 10.0 * np.log10(x) if x > 0 else -np.inf


def to_11k(x, sr):
    """原曲（44.1k）低通到 11 kHz 以内；已经是 22.05k 的只做轻微去齿（≤10.8k）。"""
    hi = min(CAP, sr / 2.0 * 0.98)
    sos = butter(8, hi, btype="low", fs=sr, output="sos")
    return sosfiltfilt(sos, x.astype(np.float64), axis=0).astype(np.float32)


def band_powers(x, sr):
    """→ ({band: 功率}, 总功率)。Welch PSD 积分；11 kHz 以上不计入总功率。"""
    mono = x.mean(axis=1).astype(np.float64)
    nper = 32768 if sr >= 40000 else 16384
    nper = min(nper, len(mono))
    f, P = welch(mono, fs=sr, nperseg=nper, noverlap=nper // 2,
                 detrend=False, scaling="density")
    df = float(f[1] - f[0])
    out, tot = {}, 0.0
    for name, lo, hi in BANDS:
        hi = min(hi, sr / 2.0)
        m = (f >= lo) & (f < hi)
        v = float(np.sum(P[m]) * df)
        out[name] = v
        tot += v
    return out, tot


def lf_metrics(x, sr, fc=120.0):
    """<fc Hz 的峰值 / RMS / crest / 包络统计（mono）。"""
    sos = butter(4, min(fc, sr / 2 * 0.9), btype="low", fs=sr, output="sos")
    y = sosfiltfilt(sos, x.mean(axis=1).astype(np.float64))
    rms = float(np.sqrt(np.mean(y ** 2)))
    peak = float(np.abs(y).max())
    env = uniform_filter1d(np.abs(y), max(1, int(0.005 * sr)))
    p999 = float(np.percentile(env, 99.9))
    med = float(np.percentile(env, 50))
    duty = float(np.mean(env >= p999 * 10 ** (-10 / 20.0))) if p999 > 0 else 0.0
    pk, _ = find_peaks(env, height=p999 * 10 ** (-8 / 20.0), distance=max(1, int(0.1 * sr)))
    return {
        "peak_db": 20 * np.log10(peak) if peak > 0 else -np.inf,
        "rms_db": 20 * np.log10(rms) if rms > 0 else -np.inf,
        "crest_db": (20 * np.log10(peak / rms)) if (peak > 0 and rms > 0) else np.nan,
        "env_p999_db": 20 * np.log10(p999) if p999 > 0 else -np.inf,
        "env_med_db": 20 * np.log10(med) if med > 0 else -np.inf,
        "duty": duty,
        "hits_per_s": len(pk) / (len(y) / sr) if len(y) else np.nan,
    }


# ---------------------------------------------------------------- 响度匹配（= 上一轮试点同一套）
def soft_limit(x, sr, ceiling_db=-0.3, lookahead_ms=5.0, release_db_per_sec=60.0):
    C = 10 ** (ceiling_db / 20.0)
    p = np.abs(x).max(axis=1)
    a = np.where(p > C, 20 * np.log10(np.maximum(p, 1e-12) / C), 0.0)
    La = max(1, int(round(lookahead_ms * sr / 1000.0)))
    m = maximum_filter1d(a, size=2 * La + 1, mode="nearest")
    step = release_db_per_sec / sr
    n = len(m)
    ar = np.maximum.accumulate(m + step * np.arange(n)) - step * np.arange(n)
    g = 10 ** (-ar / 20.0)
    y = (x * g[:, None]).astype(np.float32)
    return y, {"max_gr": float(ar.max()), "mean_gr": float(ar.mean()),
               "frac": float((ar > 0.1).mean())}


def match_lufs(x, sr, target, ceiling=-0.3, iters=6):
    """增益 + 软限幅，迭代到积分响度 = target（与 /tmp/rearr_loud 试点同口径）。"""
    g = target - M.integrated_lufs(x, sr)
    y, st, l = x, {}, M.integrated_lufs(x, sr)
    for _ in range(iters):
        y = x * (10 ** (g / 20.0))
        y, st = soft_limit(y, sr, ceiling)
        l = M.integrated_lufs(y, sr)
        err = target - l
        if abs(err) < 0.02:
            break
        g += err
    st.update(gain_db=g, lufs=l, peak_db=M.peak_dbfs(y))
    return y, st


# ---------------------------------------------------------------- 汇总
def analyse(stem, pilot_dir, target_lufs, want_norm_dir=None):
    sr_o, xo = M.source_loop(stem)
    src = {"原曲": (sr_o, xo)}
    o11 = to_11k(xo, sr_o)                          # 统一带宽口径
    src["原曲@11k"] = (sr_o, o11)

    ours_path = os.path.join(M.OUT_DIR, f"{stem}b_rearr.wav")
    if not os.path.exists(ours_path):
        raise SystemExit(f"缺 {ours_path}")
    sr_u, xu = M.read_wav(ours_path)
    src["现在"] = (sr_u, xu)

    pilot_path = os.path.join(pilot_dir, f"{stem}b_rearr.wav") if pilot_dir else None
    if pilot_path and os.path.exists(pilot_path):
        sr_p, xp = M.read_wav(pilot_path)
        src["响度匹配"] = (sr_p, xp)
        st_p = {"source": "pilot-file", "path": pilot_path,
                "lufs": M.integrated_lufs(xp, sr_p), "peak_db": M.peak_dbfs(xp)}
    else:
        xp, st_p = match_lufs(xu, sr_u, M.integrated_lufs(o11, sr_o))
        src["响度匹配"] = (sr_u, xp)
        st_p["source"] = "computed"

    out = {"stem": stem, "sources": {}, "pilot_stats": st_p}
    for name, (sr, x) in src.items():
        bp, tot = band_powers(x, sr)
        out["sources"][name] = {
            "sr": sr, "frames": len(x), "secs": len(x) / sr,
            "lufs": M.integrated_lufs(x, sr),
            "peak_db": M.peak_dbfs(x),
            "rms_db": M.rms_dbfs(x),
            "bands": {k: db(v) for k, v in bp.items()},
            "share": {k: (db(v) - db(tot)) for k, v in bp.items()},
            "tot_db": db(tot),
            "lf": lf_metrics(x, sr),
        }
        # 归一到同一 LUFS 后各段绝对电平
        g = target_lufs - out["sources"][name]["lufs"]
        out["sources"][name]["gain_to_target"] = g
        out["sources"][name]["bands_at_target"] = {
            k: out["sources"][name]["bands"][k] + g for k in out["sources"][name]["bands"]}
        out["sources"][name]["lf_at_target"] = {
            k: (v + g if isinstance(v, float) and v == v and abs(v) != float("inf") else v)
            for k, v in out["sources"][name]["lf"].items()}

    # 原曲 11–22k 参考（只有 44.1k 源有）
    f, P = welch(xo.mean(axis=1).astype(np.float64), fs=sr_o, nperseg=32768,
                 noverlap=16384, detrend=False, scaling="density")
    m = (f >= 11000) & (f < 22000)
    out["orig_11_22k_db"] = db(float(np.sum(P[m]) * (f[1] - f[0])))

    if want_norm_dir:
        os.makedirs(want_norm_dir, exist_ok=True)
        for name, (sr, x) in src.items():
            if name == "原曲":                      # 44.1k 那份不需要（@11k 已覆盖）
                continue
            g = target_lufs - out["sources"][name]["lufs"]
            y = np.clip(x * (10 ** (g / 20.0)), -1.0, 1.0)
            tag = {"原曲@11k": "orig11k", "现在": "now", "响度匹配": "loud"}[name]
            write_wav(os.path.join(want_norm_dir, f"{stem}_{tag}.wav"), sr, y)
    return out


def write_wav(path, sr, x):
    import wave
    a = np.clip(x, -1.0, 1.0)
    pcm = (a * 32767.0).astype("<i2")
    with wave.open(path, "wb") as w:
        w.setnchannels(x.shape[1])
        w.setsampwidth(2)
        w.setframerate(sr)
        w.writeframes(pcm.tobytes())


# ---------------------------------------------------------------- 表
def fmt(x, w=8, p=2, sign=False):
    if x is None or (isinstance(x, float) and (x != x)):
        return f"{'—':>{w}}"
    return f"{x:>{w}.{p}f}" if not sign else f"{x:>+{w}.{p}f}"


def table_three(o, target_lufs, label=""):
    """th18_14 三方：原始 / 归一到 target 后的绝对电平 / 相对占比。"""
    L = []
    L.append(f"### {label}（stem `{o['stem']}`）\n")
    L.append("| 源 | LUFS | peak dBFS | RMS dBFS |")
    L.append("|---|---|---|---|")
    for n in ("原曲", "原曲@11k", "现在", "响度匹配"):
        s = o["sources"][n]
        L.append(f"| {n} | {s['lufs']:.2f} | {s['peak_db']:.2f} | {s['rms_db']:.2f} |")
    L.append(f"\n原曲 11–22 kHz 能量（参考，**不进对比**）：{o['orig_11_22k_db']:.2f} dBFS\n")

    names = ("原曲@11k", "现在", "响度匹配")
    L.append(f"**A. 归一到 {target_lufs:+.1f} LUFS 后的绝对电平（dBFS）**\n")
    L.append("| 频段 | " + " | ".join(names) + " | 现−原 | 匹配−原 |")
    L.append("|---" * (len(names) + 3) + "|")
    ref_a = o["sources"]["原曲@11k"]["bands_at_target"]
    for b, lo, hi in BANDS:
        row = [f"{b}"] + [f"{o['sources'][n]['bands_at_target'][b]:.2f}" for n in names]
        now = o["sources"]["现在"]["bands_at_target"][b]
        mat = o["sources"]["响度匹配"]["bands_at_target"][b]
        row += [f"{now - ref_a[b]:+.2f}", f"{mat - ref_a[b]:+.2f}"]
        L.append("| " + " | ".join(row) + " |")

    L.append(f"\n**B. 频段占比（相对总量，dB；与响度无关）**\n")
    L.append("| 频段 | " + " | ".join(names) + " | 现−原 | 匹配−原 |")
    L.append("|---" * (len(names) + 3) + "|")
    ref_b = o["sources"]["原曲@11k"]["share"]
    for b, lo, hi in BANDS:
        row = [f"{b}"] + [f"{o['sources'][n]['share'][b]:.2f}" for n in names]
        now = o["sources"]["现在"]["share"][b]
        mat = o["sources"]["响度匹配"]["share"][b]
        row += [f"{now - ref_b[b]:+.2f}", f"{mat - ref_b[b]:+.2f}"]
        L.append("| " + " | ".join(row) + " |")

    L.append("\n**C. 低频包络（<120 Hz）**\n")
    L.append("| 源 | peak | RMS | crest | 包络 p99.9 | 包络中位 | 占空比 | 重击/秒 |")
    L.append("|---|---|---|---|---|---|---|---|")
    for n in names:
        lf = o["sources"][n]["lf"]
        L.append(f"| {n} | {lf['peak_db']:.2f} | {lf['rms_db']:.2f} | {lf['crest_db']:.2f} "
                 f"| {lf['env_p999_db']:.2f} | {lf['env_med_db']:.2f} "
                 f"| {lf['duty']*100:.1f}% | {lf['hits_per_s']:.2f} |")
    return "\n".join(L)


def table_cross(outs, target_lufs):
    L = [f"### 4 首对照（都归一到 {target_lufs:+.1f} LUFS）\n",
         "| 曲 | 原 sub<60 | 现 sub<60 | Δ | 原 60–120 | 现 60–120 | Δ "
         "| 原 120–250 | 现 120–250 | Δ | 原<120 占比 | 现<120 占比 | Δ |",
         "|---" * 13 + "|"]
    for o in outs:
        a = o["sources"]["原曲@11k"]
        b = o["sources"]["现在"]
        r = [o["stem"]]
        for band in ("sub<60", "60-120", "120-250"):
            r += [f"{a['bands_at_target'][band]:.2f}", f"{b['bands_at_target'][band]:.2f}",
                  f"{b['bands_at_target'][band] - a['bands_at_target'][band]:+.2f}"]
        s_o = -10 * np.log10(10 ** (-a["share"]["sub<60"] / 10) + 10 ** (-a["share"]["60-120"] / 10))
        s_n = -10 * np.log10(10 ** (-b["share"]["sub<60"] / 10) + 10 ** (-b["share"]["60-120"] / 10))
        r += [f"{s_o:.2f}", f"{s_n:.2f}", f"{s_n - s_o:+.2f}"]
        L.append("| " + " | ".join(r) + " |")
    return "\n".join(L)


def main(argv=None):
    ap = argparse.ArgumentParser(description="分频段能量 + 低频包络：原曲 / 我们 / 响度匹配")
    ap.add_argument("--stems", nargs="+", default=["th18_14", "th18_12", "th18_13", "th18_18"])
    ap.add_argument("--pilot-dir", default="/tmp/rearr_loud")
    ap.add_argument("--target-lufs", type=float, default=-14.0)
    ap.add_argument("--norm-dir", default=None, help="把归一到 target 的三方 wav 另存到这里")
    ap.add_argument("--md", default=None)
    ap.add_argument("--csv", default=None)
    a = ap.parse_args(argv)

    outs = [analyse(s, a.pilot_dir, a.target_lufs, a.norm_dir) for s in a.stems]
    md = []
    for o in outs:
        md.append(table_three(o, a.target_lufs,
                              label="三方对照" if o["stem"] == "th18_14" else "同曲型对照"))
        md.append("")
    md.append(table_cross(outs, a.target_lufs))
    text = "\n".join(md)
    print(text)
    if a.md:
        open(a.md, "w", encoding="utf-8").write(text + "\n")
        print(f"\nMD → {a.md}")
    if a.csv:
        with open(a.csv, "w", newline="", encoding="utf-8") as f:
            w = csv.writer(f)
            w.writerow(["stem", "source", "lufs", "peak", "rms", "band", "db", "share_db",
                        "db_at_target"])
            for o in outs:
                for n, s in o["sources"].items():
                    for b, _, _ in BANDS:
                        w.writerow([o["stem"], n, f"{s['lufs']:.3f}", f"{s['peak_db']:.3f}",
                                    f"{s['rms_db']:.3f}", b, f"{s['bands'][b]:.3f}",
                                    f"{s['share'][b]:.3f}", f"{s['bands_at_target'][b]:.3f}"])
        print(f"CSV → {a.csv}")
    if a.norm_dir:
        print(f"NORM → {a.norm_dir}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
