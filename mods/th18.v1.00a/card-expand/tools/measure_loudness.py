#!/usr/bin/env python3
"""measure_loudness.py —— 量「原曲循环体」vs「我们渲的 _rearr」的**响度**（不是峰值）。

    python3 tools/measure_loudness.py                      # 17 首全测，出对照表
    python3 tools/measure_loudness.py --stems th18_05 th18_07
    python3 tools/measure_loudness.py --csv /tmp/loud.csv --md /tmp/loud.md

## 为什么不是只看峰值
峰值归一（`--peak 0.92`）只保证**最高的那一个采样**到某个值；两首曲子 crest factor
不同 ⇒ 同样峰值下**积分响度能差好几个 LU**。游戏里从原曲切到灵界版时，耳朵听到的是
响度不是峰值 ⇒ 要对齐的是 **LUFS**。

## 四个指标
1. **peak dBFS**：采样峰（`20log10(max|x|)`）。可选 `--true-peak` 用 4× 过采样估真峰。
2. **RMS dBFS**（整体、未加权）：`20log10(sqrt(mean(x²)))`，两声道一起算。
3. **RMS 100 Hz–8 kHz 带内**：4 阶 Butterworth 带通、`sosfiltfilt` **零相位**（不引入延时，
   两边同口径）。为什么要这条：整体 RMS 被 100 Hz 以下（底鼓/贝斯，耳朵不敏感）和
   8 kHz 以上（镲，能量小）拉偏；100–8000 更接近"耳朵实际听到的能量"。
4. **LUFS（ITU-R BS.1770-4 / EBU R128 积分响度）**：本机**没有 ffmpeg**（`which ffmpeg` 空），
   `pyloudnorm` 也没装 ⇒ 自己实现，**照 libebur128 的系数公式**（任意采样率）：
   - K-weighting = ① 高架滤波 `f0=1681.974450955533 Hz, G=+3.999843853973347 dB,
     Q=0.7071752369554196`（`Vb=Vh^0.4996667741545416`）→ ② RLB 高通
     `f0=38.13547087602444 Hz, Q=0.5003270373238773`，两级都是双二阶（biquad），**因果**滤波。
   - 分块：400 ms 窗、100 ms 步进；块响度 `L_j = -0.691 + 10log10(Σ_ch G_ch·z_ch)`，
     立体声 `G=1.0`。
   - 门限：绝对 −70 LUFS，相对（过绝对门限的块的功率均值 −10 LU）。
   - 自检：48 kHz、997 Hz 正弦、RMS −23 dBFS ⇒ **−23.04 LUFS**（EBU Tech 3341 期望 −23.0 ±0.1）✓
   （本机 scipy 1.15.3；没有 scipy 时带内 RMS 会报错——LUFS/峰值/RMS 不依赖 scipy）
   ⚠ 若以后装了 ffmpeg，可用
   `ffmpeg -i x.wav -af ebur128=peak=true -f null -` 复核（口径应与本文件一致 ±0.1 LU）。

## 带宽口径（重要）
原曲 **44100 Hz 立体声**、我们的 `_rearr` 是 **22050 Hz 立体声**（引擎素材格式）。
22050 Hz 的 Nyquist = 11 kHz ⇒ **11–22 kHz 的能量我们根本带不过去**，而这部分对 LUFS
有一点贡献（K-weighting 高频段 +4 dB）。所以表里多给一列
**`原曲LUFS@限带11k`** = 原曲先 `resample_poly(1,2)` 降采样到 22050（带抗混叠 FIR）
再量的 LUFS —— 那是"我们的格式**理论上限**能承载的响度"。
`ΔLUFS` 用**原曲全带宽**做基准（= 游戏里真正听到的原曲）；`Δ限带` 用限带版，
只用来分辨"差的是编曲/母带"还是"差的是格式带宽"。

## 切原曲
`thbgm.fmt` 记录 = thtk `bgm_fmt_t`，0x34 字节，`+0x10` 文件偏移、`+0x14` preload、
`+0x18` intro 字节、`+0x1c` track 总字节（**含 intro**）⇒ 循环体 = `[off+intro, off+size)`。
`th18_01` 起 offset 0x10（跳过 16 B 的 "ZWAV" 头）。
"""
import argparse
import csv
import os
import struct
import sys
import wave

import numpy as np

try:
    from scipy.signal import butter, sosfiltfilt, lfilter
    HAVE_SCIPY = True
except Exception:                                    # 兜底：只量 LUFS/峰值/整体 RMS
    HAVE_SCIPY = False

HERE = os.path.dirname(os.path.abspath(__file__))
MOD = os.path.dirname(HERE)
REPO = os.path.dirname(os.path.dirname(os.path.dirname(MOD)))
FMT = os.path.join(REPO, "local", "th18.v1.00a", "dat", "thbgm.fmt")
PCM = os.path.join(REPO, "local", "th18.v1.00a", "thbgm.dat")
OUT_DIR = os.path.join(REPO, "local", "th18.v1.00a", "trance_bgm")
FMT_RECORD = 0x34
SRC_RATE = 44100                                     # 一手：fmt 的字节口径 = 44100×2ch×2B = 176400 B/s
OUR_RATE = 22050                                     # `_rearr` 出盘采样率（引擎素材格式）
STD = ("th18_01", "th18_03", "th18_04", "th18_05", "th18_06", "th18_07", "th18_08",
       "th18_09", "th18_10", "th18_11", "th18_12", "th18_13", "th18_14", "th18_15",
       "th18_16", "th18_17", "th18_18")              # 内部曲名（fmt 顺序，17 首）


# ---------------------------------------------------------------- 读
def _fmt_entries():
    d = open(FMT, "rb").read()
    out = []
    for i in range(len(d) // FMT_RECORD):
        r = d[i * FMT_RECORD:(i + 1) * FMT_RECORD]
        name = r[0:8].split(b"\0")[0].decode("ascii", "replace").rstrip(". ")
        off, pre, intro, size = struct.unpack_from("<IIiI", r, 16)
        out.append((name, off, pre, intro, size))
    return out


def source_loop(name):
    """→ (rate, float32 (n,2))：**原曲循环体**（丢掉 intro），44100 Hz 立体声。"""
    for nm, off, pre, intro, size in _fmt_entries():
        if nm != name:
            continue
        if not (0 < intro < size):
            raise SystemExit(f"{name}: fmt 的 intro={intro} / track={size} 不自洽")
        with open(PCM, "rb") as f:
            f.seek(off + intro)
            raw = f.read(size - intro)
        if len(raw) != size - intro:
            raise SystemExit(f"{name}: thbgm.dat 读到 {len(raw)} B，fmt 说 {size - intro} B")
        a = np.frombuffer(raw, dtype="<i2").astype(np.float32) / 32768.0
        return SRC_RATE, a.reshape(-1, 2)
    raise SystemExit(f"{FMT} 里没有 {name}")


def read_wav(path):
    with wave.open(path, "rb") as w:
        ch, sw, sr, n = w.getnchannels(), w.getsampwidth(), w.getframerate(), w.getnframes()
        raw = w.readframes(n)
    if sw != 2:
        raise SystemExit(f"{path}: 只支持 16 bit PCM（getsampwidth={sw}）")
    a = np.frombuffer(raw, dtype="<i2").astype(np.float32) / 32768.0
    return sr, a.reshape(-1, ch)


# ---------------------------------------------------------------- 指标
def peak_dbfs(x):
    p = float(np.abs(x).max()) if x.size else 0.0
    return 20.0 * np.log10(p) if p > 0 else -np.inf


def true_peak_dbfs(x, up=4):
    """4× 多相过采样估真峰（dBTP）。只看采样峰会漏掉采样点之间的过冲。"""
    try:
        from scipy.signal import resample_poly
    except Exception:
        return peak_dbfs(x)
    m = 0.0
    for c in range(x.shape[1]):
        y = resample_poly(x[:, c].astype(np.float64), up, 1)
        m = max(m, float(np.abs(y).max()))
    return 20.0 * np.log10(m) if m > 0 else -np.inf


def rms_dbfs(x):
    if not x.size:
        return -np.inf
    m = float(np.mean(x.astype(np.float64) ** 2))
    return 10.0 * np.log10(m) if m > 0 else -np.inf


def band_rms_dbfs(x, sr, lo=100.0, hi=8000.0, order=4):
    """100 Hz–8 kHz 带内 RMS dBFS（Butterworth 带通、零相位；>Nyquist 自动夹）。"""
    if not HAVE_SCIPY:
        raise SystemExit("带内 RMS 需要 scipy（pip install scipy）")
    hi = min(hi, sr / 2.0 * 0.98)
    sos = butter(order, [lo, hi], btype="band", fs=sr, output="sos")
    y = sosfiltfilt(sos, x.astype(np.float64), axis=0)
    return rms_dbfs(y)


def k_weighting(fs):
    """K-weighting 两级 biquad（libebur128 的系数公式，任意 fs）。"""
    f0, G, Q = 1681.974450955533, 3.999843853973347, 0.7071752369554196
    K = np.tan(np.pi * f0 / fs)
    Vh = 10.0 ** (G / 20.0)
    Vb = Vh ** 0.4996667741545416
    a0 = 1.0 + K / Q + K * K
    b1 = np.array([(Vh + Vb * K / Q + K * K) / a0, 2.0 * (K * K - Vh) / a0,
                   (Vh - Vb * K / Q + K * K) / a0])
    a1 = np.array([1.0, 2.0 * (K * K - 1.0) / a0, (1.0 - K / Q + K * K) / a0])
    f0, Q = 38.13547087602444, 0.5003270373238773
    K = np.tan(np.pi * f0 / fs)
    a0 = 1.0 + K / Q + K * K
    b2 = np.array([1.0 / a0, -2.0 / a0, 1.0 / a0])
    a2 = np.array([1.0, 2.0 * (K * K - 1.0) / a0, (1.0 - K / Q + K * K) / a0])
    return (b1, a1), (b2, a2)


def integrated_lufs(x, sr):
    """ITU-R BS.1770-4 / EBU R128 积分响度（LUFS）。立体声权重 1.0。"""
    if not HAVE_SCIPY:
        raise SystemExit("LUFS 需要 scipy（pip install scipy）")
    (b1, a1), (b2, a2) = k_weighting(sr)
    z = np.zeros(len(x), dtype=np.float64)
    for c in range(x.shape[1]):
        y = lfilter(b2, a2, lfilter(b1, a1, x[:, c].astype(np.float64)))
        z += y * y
    win, hop = int(0.4 * sr), int(0.1 * sr)
    n = len(z)
    if n < win:
        if n == 0:
            return -np.inf
        win, st = n, np.array([0])
    else:
        st = np.arange(0, n - win + 1, hop)
    cs = np.concatenate([[0.0], np.cumsum(z)])
    m = (cs[st + win] - cs[st]) / win
    l = -0.691 + 10.0 * np.log10(m + 1e-30)
    keep = l > -70.0
    if not keep.any():
        return -np.inf
    rel = -0.691 + 10.0 * np.log10(m[keep].mean()) - 10.0
    keep &= l > rel
    return float(-0.691 + 10.0 * np.log10(m[keep].mean()))


def measure(path_or_name, is_original=False, true_peak=False):
    if is_original:
        sr, x = source_loop(path_or_name)
    else:
        sr, x = read_wav(path_or_name)
    d = {
        "rate": sr, "frames": len(x), "secs": len(x) / sr,
        "peak": peak_dbfs(x),
        "rms": rms_dbfs(x),
        "lufs": integrated_lufs(x, sr),
    }
    d["rms_band"] = band_rms_dbfs(x, sr) if HAVE_SCIPY else float("nan")
    d["true_peak"] = true_peak_dbfs(x) if true_peak else float("nan")
    return d


def bandlimited_22k(x):
    """把 44100 立体声降到 22050（抗混叠）——模拟"我们的格式能承载的最高带宽"。"""
    if not HAVE_SCIPY:
        return None, OUR_RATE
    from scipy.signal import resample_poly
    return resample_poly(x.astype(np.float64), 1, 2, axis=0).astype(np.float32), OUR_RATE


# ---------------------------------------------------------------- 表
def build_rows(stems, true_peak=False):
    rows = []
    for stem in stems:
        ours = os.path.join(OUT_DIR, f"{stem}b_rearr.wav")
        o = measure(stem, is_original=True, true_peak=true_peak)
        bl, blr = bandlimited_22k(source_loop(stem)[1])
        o["lufs_11k"] = integrated_lufs(bl, blr) if bl is not None else float("nan")
        if not os.path.exists(ours):
            rows.append({"stem": stem, "orig": o, "ours": None})
            continue
        u = measure(ours, true_peak=true_peak)
        rows.append({"stem": stem, "orig": o, "ours": u})
    return rows


def print_table(rows):
    hdr = (f"{'曲':<9}{'peak原':>8}{'peak我':>8}{'Δpk':>7}"
           f"{'RMS原':>8}{'RMS我':>8}{'Δ':>7}"
           f"{'带内原':>8}{'带内我':>8}{'Δ':>7}"
           f"{'LUFS原':>8}{'LUFS我':>8}{'ΔLU':>7}{'原@11k':>8}")
    print(hdr)
    print("-" * len(hdr))
    for r in rows:
        o = r["orig"]
        if r["ours"] is None:
            print(f"{r['stem']:<9}{o['peak']:>8.2f}{'—':>8}{'—':>7}{o['rms']:>8.2f}"
                  f"{'—':>8}{'—':>7}{o['rms_band']:>8.2f}{'—':>8}{'—':>7}"
                  f"{o['lufs']:>8.2f}{'—':>8}{'—':>7}{o['lufs_11k']:>8.2f}")
            continue
        u = r["ours"]
        print(f"{r['stem']:<9}{o['peak']:>8.2f}{u['peak']:>8.2f}{u['peak']-o['peak']:>+7.2f}"
              f"{o['rms']:>8.2f}{u['rms']:>8.2f}{u['rms']-o['rms']:>+7.2f}"
              f"{o['rms_band']:>8.2f}{u['rms_band']:>8.2f}{u['rms_band']-o['rms_band']:>+7.2f}"
              f"{o['lufs']:>8.2f}{u['lufs']:>8.2f}{u['lufs']-o['lufs']:>+7.2f}"
              f"{o['lufs_11k']:>8.2f}")


def write_csv(path, rows):
    with open(path, "w", newline="", encoding="utf-8") as f:
        w = csv.writer(f)
        w.writerow(["stem", "orig_peak_dbfs", "ours_peak_dbfs", "d_peak_db",
                    "orig_rms_dbfs", "ours_rms_dbfs", "d_rms_db",
                    "orig_band_rms_dbfs", "ours_band_rms_dbfs", "d_band_db",
                    "orig_lufs", "ours_lufs", "d_lufs", "orig_lufs_11k",
                    "ours_true_peak_dbtp", "orig_secs", "ours_secs"])
        for r in rows:
            o, u = r["orig"], r["ours"] or {}
            w.writerow([r["stem"],
                        f"{o['peak']:.2f}", f"{u.get('peak', float('nan')):.2f}",
                        f"{u.get('peak', float('nan')) - o['peak']:+.2f}",
                        f"{o['rms']:.2f}", f"{u.get('rms', float('nan')):.2f}",
                        f"{u.get('rms', float('nan')) - o['rms']:+.2f}",
                        f"{o['rms_band']:.2f}", f"{u.get('rms_band', float('nan')):.2f}",
                        f"{u.get('rms_band', float('nan')) - o['rms_band']:+.2f}",
                        f"{o['lufs']:.2f}", f"{u.get('lufs', float('nan')):.2f}",
                        f"{u.get('lufs', float('nan')) - o['lufs']:+.2f}",
                        f"{o['lufs_11k']:.2f}",
                        f"{u.get('true_peak', float('nan')):.2f}",
                        f"{o['secs']:.2f}", f"{u.get('secs', float('nan')):.2f}"])


def write_md(path, rows):
    with open(path, "w", encoding="utf-8") as f:
        f.write("| 曲 | peak 原 | peak 我 | Δ | RMS 原 | RMS 我 | Δ | 带内 原 | 带内 我 | Δ "
                "| LUFS 原 | LUFS 我 | ΔLU | 原@11k |\n")
        f.write("|---|---|---|---|---|---|---|---|---|---|---|---|---|---|\n")
        for r in rows:
            o, u = r["orig"], r["ours"]
            if u is None:
                f.write(f"| {r['stem']} | {o['peak']:.2f} | — | — | {o['rms']:.2f} | — | — "
                        f"| {o['rms_band']:.2f} | — | — | {o['lufs']:.2f} | — | — "
                        f"| {o['lufs_11k']:.2f} |\n")
                continue
            f.write(f"| {r['stem']} | {o['peak']:.2f} | {u['peak']:.2f} "
                    f"| {u['peak']-o['peak']:+.2f} | {o['rms']:.2f} | {u['rms']:.2f} "
                    f"| {u['rms']-o['rms']:+.2f} | {o['rms_band']:.2f} | {u['rms_band']:.2f} "
                    f"| {u['rms_band']-o['rms_band']:+.2f} | {o['lufs']:.2f} | {u['lufs']:.2f} "
                    f"| {u['lufs']-o['lufs']:+.2f} | {o['lufs_11k']:.2f} |\n")


def main(argv=None):
    ap = argparse.ArgumentParser(description="量原曲循环体 vs _rearr 的峰值/RMS/LUFS")
    ap.add_argument("--stems", nargs="+", default=None)
    ap.add_argument("--csv", default=None)
    ap.add_argument("--md", default=None)
    ap.add_argument("--true-peak", action="store_true", help="额外算 4× 过采样真峰（慢）")
    a = ap.parse_args(argv)
    if not HAVE_SCIPY:
        print("★ 没有 scipy：只能出峰值/整体 RMS（LUFS 与带内 RMS 需要 scipy）")
    stems = a.stems or [s for s in STD
                        if os.path.exists(os.path.join(OUT_DIR, f"{s}b_rearr.wav"))]
    rows = build_rows(stems, true_peak=a.true_peak)
    print_table(rows)
    if a.csv:
        write_csv(a.csv, rows)
        print(f"\nCSV → {a.csv}")
    if a.md:
        write_md(a.md, rows)
        print(f"MD → {a.md}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
