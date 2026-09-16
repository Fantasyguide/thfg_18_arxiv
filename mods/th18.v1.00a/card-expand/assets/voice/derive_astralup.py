#!/usr/bin/env python3
"""derive_astralup.py —— 把 **TH13 的 `se_astralup.wav`** 取到语音素材目录，供音效表扩容那条通道播放。

## 为什么需要它（用户 2026-09-16n 定案：方案②「把这枚 wav 随 mod 发」）

TH13 的 59 枚 SE 与 TH18 的 71 枚里，**同名 58 枚逐字节完全相同**，而 **TH13 唯一多出来的那枚
正是 `se_astralup.wav`**（灵界槽跨格的升格音，`th13:0x43d574 MOV EDX,0x44`）——TH18 的 84 行音效表里
**没有**它，也无波形替身（相关 < 0.1）。所以「升格音」一直是 ❓自定替身（`se_powerup`）。

用户选了「**随 mod 发这一枚**」⇒ 走本仓**已有的音效表扩容通道**（`AUDIT` §Q：四张表搬进 codecave、
`id 0x54..0x73` 给 32 个语音），**不需要**自己写播放器 —— 加一行 `ORDER.txt` 就得到一个新 SE id，
卡侧照旧 `ce_play_sound(id, x)` ✓。

## ⚠ 版权口径（本仓铁律：不许把版权资产提交进仓库）

`assets/voice/*.wav` 是**入库**的（因为那是我们自己合成的），但这枚是**ZUN 的原版音频** ⇒
**不能入库**。所以：源留在 `local/`（gitignored），由本脚本在构建期**拷过来**，
产物 `assets/voice/ASTRALUP.wav` 单独写进 `.gitignore`。
（同一形状的先例：113 的咲夜灵界形态图从 `local/th18.v1.00a/trance_form/` 现取。）

跑：`python3 assets/voice/derive_astralup.py`（`make voice` 会自动先跑它）
"""
import shutil
import struct
import sys
import wave
from pathlib import Path

HERE = Path(__file__).resolve().parent
REPO = HERE.parents[4]                       # renkolab/（assets/voice → assets → card-expand → th18.v1.00a → mods → renkolab）
SRC = REPO / "local" / "th13.v1.00c" / "dat" / "se_astralup.wav"
DST = HERE / "ASTRALUP.wav"


def main() -> int:
    if not SRC.is_file():
        raise SystemExit(f"缺 TH13 样本 {SRC}\n（用户放进 local/，见 local/README.md）")
    with wave.open(str(SRC), "rb") as w:
        ch, sw, fr, n = w.getnchannels(), w.getsampwidth(), w.getframerate(), w.getnframes()
        comp = w.getcomptype()
    if comp != "NONE" or sw not in (1, 2):
        raise SystemExit(f"{SRC.name}: 不是未压缩的 8/16-bit PCM（comptype={comp}, 采样宽={sw}）"
                         f"—— 引擎的 RIFF 解析吃这个，别转成别的")
    # 逐字节拷（不做任何重采样/重编码：音色必须与原版完全一致）
    if not DST.is_file() or DST.read_bytes() != SRC.read_bytes():
        shutil.copyfile(SRC, DST)
        print(f"voice: 现取 {SRC.name} -> {DST.name}（{ch} ch / {sw * 8} bit / {fr} Hz / {n} 帧，逐字节拷）")
    else:
        print(f"voice: {DST.name} 已是最新（与 TH13 样本逐字节相同）")
    return 0


if __name__ == "__main__":
    sys.exit(main())
