#!/usr/bin/env python3
"""把 _src/ 里的压缩语音（ogg / mp3 / …）转成引擎要的 PCM wav，并把响度拉到语音基准。

    python3 convert_voice.py NAME [NAME …]        # _src/NAME.ogg → NAME.wav
    python3 convert_voice.py --all                # _src/ 里每个 .ogg 都转

  输出：44.1 kHz、16-bit、单声道（声像只对单声道有意义），data 之外不带别的 chunk。
  响度：先整体 +GAIN_DB，再软限幅到 LIMIT（峰值 ≈ -0.5 dBFS）。原生 wav 一律 peak 归一化、
  cfg 行只能衰减不能增益，所以「够响」只能靠 wav 自己（README「响度」一节）；用户给的语音素材
  峰值已近满幅但 RMS 只有 -15 dBFS，直接转会比基准 -10 轻 5 dB —— 增益 + 限幅把波峰因数压下去。
  参数写死、ffmpeg 决定性输出 ⇒ 同一份源图重跑得到同一个 wav。

依赖：ffmpeg（PATH 里有就行；这台机器是 /usr/bin/ffmpeg）。
转完跑 `make voice`：它会打印每条的 peak / rms，偏离 -10 ± 4 dB 会提醒。
"""
import argparse
import shutil
import subprocess
import sys
from pathlib import Path

HERE = Path(__file__).resolve().parent
SRC = HERE / "_src"
GAIN_DB = 6.0           # 2026-09-06 实测：FIRELORD_* 两条 +6 dB → RMS -10.7 / -11.2（基准 -10 ± 4）
LIMIT = 0.94            # alimiter 上限（线性，≈ -0.5 dBFS）
EXTS = (".ogg", ".mp3", ".flac", ".m4a", ".wav")


def convert(name: str) -> Path:
    src = next((SRC / f"{name}{e}" for e in EXTS if (SRC / f"{name}{e}").exists()), None)
    if src is None:
        raise SystemExit(f"{name}: _src/ 里没有 {name}.ogg（或 .mp3/.flac/.m4a/.wav）")
    out = HERE / f"{name}.wav"
    af = f"volume={GAIN_DB}dB,alimiter=limit={LIMIT}:attack=3:release=60:level=false"
    cmd = ["ffmpeg", "-y", "-v", "error", "-i", str(src), "-af", af,
           "-ar", "44100", "-ac", "1", "-sample_fmt", "s16",
           "-map_metadata", "-1", "-fflags", "+bitexact", "-flags:a", "+bitexact",
           str(out)]
    subprocess.run(cmd, check=True)
    print(f"{name}: {src.name} → {out.name} ({out.stat().st_size} bytes, +{GAIN_DB:g} dB, limit {LIMIT})")
    return out


def main():
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    ap.add_argument("names", nargs="*")
    ap.add_argument("--all", action="store_true", help="转 _src/ 里所有 .ogg")
    a = ap.parse_args()
    if shutil.which("ffmpeg") is None:
        raise SystemExit("需要 ffmpeg（PATH 里没找到）")
    names = a.names or ([p.stem for p in sorted(SRC.glob("*.ogg"))] if a.all else [])
    if not names:
        ap.error("给 NAME 或 --all")
    for n in names:
        convert(n)
    return 0


if __name__ == "__main__":
    sys.exit(main())
