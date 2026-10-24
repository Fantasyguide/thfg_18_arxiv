#!/usr/bin/env python3
"""make_lowfreq_pilot.py —— 给一首 `_rearr` 出「响度匹配 + 低架 EQ」试点（**只写试点目录，不动素材**）。

    python3 tools/make_lowfreq_pilot.py --stem th18_14 \
        --out "/tmp/rl-clean/灵界BGM试听/低频-试点" --shelf-db 2 4 --shelf-hz 100

## 为什么是"响度匹配之后再架 EQ、再匹配一次"
① 先把 `_rearr` 的**积分响度**顶到**该曲原曲循环体**的 LUFS（与 `make_trance_rearr.py --loudness-match original`
   同一套：增益 + 软限幅 lookahead 5 ms / release 60 dB·s⁻¹ / ceiling −0.3 dBFS）—— 这样 A/B 时
   音量不跳、耳朵只比音色；
② 再叠一个**低架（low shelf）**：RBJ 双二阶，`f0=100 Hz`、`S=1`、增益 `+2 / +4 dB`；
   DC 端 = +G，100 Hz 处 ≈ +G/2，250 Hz 以上 ≈ 0（实测见报告表）。
③ 架完响度会涨 ⇒ **再匹配回同一个 LUFS**（否则"低音多"里混着"整体更响"，听感不可信）。
★ 全程不重采样、不改帧数 ⇒ 与 `_rearr` **逐帧同长、循环点不变**（试点可以原地替换试听）。

## 命名
`<stem>b_rearr_loud.wav`（只有响度匹配）/ `…_loud_shelf+2dB.wav` / `…_loud_shelf+4dB.wav`，
名字里的 `+2dB` 就是**架的量**，`shelf hz` 写进同名 `.json`（连同 LUFS、峰、限幅统计、实测频段差）。
"""
import argparse
import json
import os
import sys

import numpy as np

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import measure_loudness as M                       # noqa: E402
from measure_band_energy import write_wav, to_11k, band_powers, db, BANDS  # noqa: E402
# ★ 响度匹配**直接用生成器的实现**（`make_trance_rearr.match_loudness`）：它比"增益+限幅"多一条
#   `loop=True`（前后各接 1 s 卷回副本再限幅）——循环点两侧增益包络连续。自己再写一遍会出现
#   0.1 LU 级的口径差，试点就不再与 `--loudness-match original` 的产物逐字节相同了。
from make_trance_rearr import match_loudness as match_lufs  # noqa: E402


def low_shelf(x, sr, f0, gain_db, S=1.0):
    """RBJ audio-EQ-cookbook low shelf（双二阶、因果）。DC 增益 = +gain_db，f0 处 ≈ 一半，2·f0 以上 ≈ 0。"""
    from scipy.signal import sosfilt
    A = 10.0 ** (gain_db / 40.0)
    w0 = 2.0 * np.pi * f0 / sr
    cw, sw = np.cos(w0), np.sin(w0)
    alpha = sw / 2.0 * np.sqrt((A + 1.0 / A) * (1.0 / S - 1.0) + 2.0)
    sA = np.sqrt(A)
    b0 = A * ((A + 1) - (A - 1) * cw + 2 * sA * alpha)
    b1 = 2 * A * ((A - 1) - (A + 1) * cw)
    b2 = A * ((A + 1) - (A - 1) * cw - 2 * sA * alpha)
    a0 = (A + 1) + (A - 1) * cw + 2 * sA * alpha
    a1 = -2 * ((A - 1) + (A + 1) * cw)
    a2 = (A + 1) + (A - 1) * cw - 2 * sA * alpha
    sos = np.array([[b0 / a0, b1 / a0, b2 / a0, 1.0, a1 / a0, a2 / a0]])
    return sosfilt(sos, x.astype(np.float64), axis=0).astype(np.float32)


def band_delta(y, sr, ref, sr_ref):
    """→ {band: ΔdB}：两边都归一到 −14 LUFS 后逐段比（与 measure_band_energy 同口径）。"""
    bp_y, _ = band_powers(y, sr)
    bp_r, _ = band_powers(ref, sr_ref)
    gy = -14.0 - M.integrated_lufs(y, sr)
    gr = -14.0 - M.integrated_lufs(ref, sr_ref)
    return {b: (db(bp_y[b]) + gy) - (db(bp_r[b]) + gr) for b, _, _ in BANDS}


def main(argv=None):
    ap = argparse.ArgumentParser(description="响度匹配 + 低架 EQ 试点")
    ap.add_argument("--stem", default="th18_14")
    ap.add_argument("--src", default=None, help="默认 local/th18.v1.00a/trance_bgm/<stem>b_rearr.wav")
    ap.add_argument("--out", required=True)
    ap.add_argument("--shelf-db", nargs="+", type=float, default=[2.0, 4.0])
    ap.add_argument("--shelf-hz", type=float, default=100.0)
    ap.add_argument("--no-base", action="store_true", help="只出架 EQ 版，不重写纯响度匹配版")
    a = ap.parse_args(argv)

    src = a.src or os.path.join(M.OUT_DIR, f"{a.stem}b_rearr.wav")
    sr, x = M.read_wav(src)
    sr_o, xo = M.source_loop(a.stem)
    ref = to_11k(xo, sr_o)                          # 与原曲@11k 同口径（只用于频段差）
    # ★ 目标响度用**原曲全带宽**（游戏里听到的那一份）：与 `--loudness-match original` 同口径。
    #   注：输入是**已峰值归一**的成品（生成器当时是对原始渲染做匹配的）⇒ 与上一轮试点
    #   同 LUFS / 同 peak / 同样频段差，**样本差 ≤4 LSB（−78 dBFS，听不出）**，不是逐字节同一文件。
    target = M.integrated_lufs(xo, sr_o)
    os.makedirs(a.out, exist_ok=True)

    base, st = match_lufs(x, sr, target)
    print(f"{a.stem}: {os.path.basename(src)}  LUFS {M.integrated_lufs(x, sr):.2f} → "
          f"{st['lufs']:.2f}（增益 {st['gain_db']:+.2f} dB，限幅最大 {st['max_gr']:.2f} dB）")

    made = []
    if not a.no_base:
        p = os.path.join(a.out, f"{a.stem}b_rearr_loud.wav")
        write_wav(p, sr, base)
        made.append((p, None, base, {}))

    for gdb in a.shelf_db:
        y = low_shelf(base, sr, a.shelf_hz, gdb)
        y, st2 = match_lufs(y, sr, target)          # 架完再匹配回同一响度
        tag = f"{gdb:+g}dB".replace("+", "p")
        p = os.path.join(a.out, f"{a.stem}b_rearr_loud_shelf{tag}.wav")
        write_wav(p, sr, y)
        made.append((p, gdb, y, st2))
        print(f"  shelf {gdb:+.1f} dB @ {a.shelf_hz:g} Hz → 再匹配增益 {st2['gain_db']:+.2f} dB，"
              f"限幅最大 {st2['max_gr']:.2f} dB，结果 {st2['lufs']:.2f} LUFS")

    # 报表：帧数 + 频段差
    rep = {"stem": a.stem, "src": src, "target_lufs": target, "shelf_hz": a.shelf_hz,
           "frames": len(base), "files": {}}
    for p, gdb, y, st2 in made:
        assert len(y) == len(base), "帧数变了！"
        d = band_delta(y, sr, ref, sr_o)
        rep["files"][os.path.basename(p)] = {
            "shelf_db": gdb, "lufs": float(M.integrated_lufs(y, sr)),
            "peak_db": float(M.peak_dbfs(y)), "frames": len(y),
            "limit": {k: float(v) for k, v in (st2 or {}).items() if isinstance(v, (int, float))},
            "band_delta_vs_orig_db": {k: float(v) for k, v in d.items()},
        }
        print("   " + os.path.basename(p) + "  " +
              "  ".join(f"{k}{v:+.2f}" for k, v in d.items() if k in ("sub<60", "60-120", "120-250")))
    jp = os.path.join(a.out, f"{a.stem}_lowfreq_pilot.json")
    with open(jp, "w", encoding="utf-8") as f:
        json.dump(rep, f, ensure_ascii=False, indent=1)
    print(f"→ {a.out}（{len(made)} 个 wav + {os.path.basename(jp)}）")
    return 0


if __name__ == "__main__":
    sys.exit(main())
