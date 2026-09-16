#!/usr/bin/env python3
"""make_trance_marker.py —— 给灵界 BGM 造「诊断素材」：把指定音效粗暴循环，单独放 / 叠在音乐上。

    python3 tools/make_trance_marker.py --se "<se_pldead00.wav 的路径>"            # 造 stage1 那两条
    python3 tools/make_trance_marker.py --se <路径> --stem th18_03 --stem th18_04   # 指定曲目

产物（与音乐素材同目录、同采样率/声道）：

| 文件 | 内容 | 用途 |
| --- | --- | --- |
| `<stem>b_se.wav`   | **只有**那个音效，从头循环铺满音乐的长度 | 一眼确认"第二路到底有没有响" |
| `<stem>b_both.wav` | 现有的加强低音音乐 **+** 那个音效循环叠在上面 | 同时听音乐差异（用户 2026-09-14 的口径）|

**怎么切**：在 `<modkit>\mods\bgm\`（或 `thcrap\bin\bgm\`、`game\bgm\`）放一个 `mode.txt`，
内容写 `se` 或 `both`（一行）；删掉它 = 回到 `music`（默认）。DLL 侧读它，见
`native/cards/trance_bgm.c` 的 `read_material_mode()`。⇒ **不用重新发布就能 A/B**。

为什么不去改 `make_trance_bgm.py`：那个工具是从 `thbgm.dat` 走 DSP 生成主素材的**唯一出处**；
本工具只做"拿现成素材 + 一个音效 → 诊断变体"，改它会把两件事搅在一起。
"""
import argparse
import hashlib
import os
import struct
import sys
import wave

HERE = os.path.dirname(os.path.abspath(__file__))
MOD = os.path.dirname(HERE)                     # card-expand/
REPO = os.path.dirname(os.path.dirname(os.path.dirname(MOD)))
DEFAULT_OUT = os.path.join(REPO, "local", "th18.v1.00a", "trance_bgm")
DEFAULT_SE = ("/mnt/d/117/桌面/迫真车万/[th13] 东方神灵庙 (汉化版+日文版)/se_pldead00.wav")


def read_wav(path):
    """→ (pcm_bytes, rate, channels, bits)：8-bit 一律**升成 16-bit 有符号**（引擎只吃 16-bit PCM，
    一手：构造成器把 fmt 交给 DirectSound，我们的 fmt 记录写的是 16 bit）。"""
    with wave.open(path, "rb") as w:
        rate, ch, sw, n = w.getframerate(), w.getnchannels(), w.getsampwidth(), w.getnframes()
        raw = w.readframes(n)
    if sw == 1:                                  # 8-bit 无符号 → 16-bit 有符号
        out = bytearray(n * ch * 2)
        for i, b in enumerate(raw):
            v = (b - 128) << 8
            struct.pack_into("<h", out, i * 2, max(-32768, min(32767, v)))
        return bytes(out), rate, ch, 16
    if sw == 2:
        return raw, rate, ch, 16
    raise SystemExit(f"{path}: 只支持 8/16-bit PCM，实测 {sw * 8}-bit")


def resample_lin(pcm, ch, src_rate, dst_rate):
    """线性重采样（诊断素材够用；主素材走 make_trance_bgm.py 的那条链）。"""
    if src_rate == dst_rate:
        return pcm
    n = len(pcm) // (2 * ch)
    src = struct.unpack(f"<{n * ch}h", pcm[:n * ch * 2])
    out_n = int(n * dst_rate / src_rate)
    out = bytearray(out_n * ch * 2)
    for i in range(out_n):
        pos = i * src_rate / dst_rate
        i0 = int(pos)
        i1 = min(i0 + 1, n - 1)
        fr = pos - i0
        for c in range(ch):
            v = src[i0 * ch + c] * (1 - fr) + src[i1 * ch + c] * fr
            struct.pack_into("<h", out, (i * ch + c) * 2, int(max(-32768, min(32767, round(v)))))
    return bytes(out)


def write_wav(path, pcm, rate, channels):
    """写 16 bit PCM WAV（与 `make_trance_bgm.py` 同一条路：stdlib `wave`，不写额外 chunk）。

    ★★ 2026-09-14 踩过：第一版这里直接 `open(path,'wb').write(pcm)` —— **忘了写 RIFF 头**，
    产物是裸 PCM：用户播放器判"损坏"，DLL 的 WAV 解析器也直接拒绝
    （日志 `trance-bgm: REFUSED - 素材 "…_both.wav" 不可用 rc=-6`）⇒ 第二路根本不起。
    写完必须回读校验（见下面 `verify_wav()`），这是唯一能挡住这类错的一步。"""
    with wave.open(path, "wb") as w:
        w.setnchannels(channels)
        w.setsampwidth(2)
        w.setframerate(rate)
        w.writeframes(pcm)


def verify_wav(path, rate, channels, nbytes, what):
    """回读：RIFF 头在不在、参数对不对、data 长度对不对。不一致直接退出（不静默）。"""
    with wave.open(path, "rb") as w:
        got = (w.getframerate(), w.getnchannels(), w.getsampwidth(), w.getnframes() * w.getnchannels() * 2)
    if got[0] != rate or got[1] != channels or got[2] != 2 or got[3] != nbytes:
        raise SystemExit(f"{what}: 回读不一致 —— 想要 {rate}Hz/{channels}ch/16bit/{nbytes}B，"
                         f"实测 {got[0]}Hz/{got[1]}ch/{got[2] * 8}bit/{got[3]}B")
    with open(path, "rb") as f:
        if f.read(4) != b"RIFF":
            raise SystemExit(f"{what}: 没有 RIFF 头（引擎与播放器都会拒绝）")
    print(f"    ✓ {os.path.basename(path)} 回读：{got[0]}Hz/{got[1]}ch/16bit，data {got[3]} B")


def mix(base, over, ch, gain):
    """`base` 里叠上循环铺满的 `over`（gain ∈ [0,1]），返回 (pcm, clipped)。"""
    nb, no = len(base) // 2, len(over) // 2
    if no == 0:
        raise SystemExit("音效为空")
    b = list(struct.unpack(f"<{nb}h", base[:nb * 2]))
    o = struct.unpack(f"<{no}h", over[:no * 2])
    clipped = 0
    for i in range(nb):
        v = b[i] + int(o[i % no] * gain)
        if v > 32767:
            v, clipped = 32767, clipped + 1
        elif v < -32768:
            v, clipped = -32768, clipped + 1
        b[i] = v
    return struct.pack(f"<{nb}h", *b), clipped


def main(argv=None):
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    ap.add_argument("--se", default=DEFAULT_SE, help="诊断音效（默认 TH13 的 se_pldead00.wav）")
    ap.add_argument("--out", default=DEFAULT_OUT, help="素材目录（默认 local/th18.v1.00a/trance_bgm/）")
    ap.add_argument("--stem", action="append", default=None,
                    help="曲目 stem（可多次；默认 th18_03 与 th18_04 = 1 面道中/boss）")
    ap.add_argument("--se-gain", type=float, default=0.45,
                    help="both 模式下音效的音量比例（默认 0.45；1.0 = 与音乐等幅，会很吵）")
    a = ap.parse_args(argv)

    stems = a.stem or ["th18_03", "th18_04"]
    if not os.path.isfile(a.se):
        raise SystemExit(f"找不到诊断音效：{a.se}\n（用 --se <路径> 指过去）")
    se_pcm, se_rate, se_ch, se_bits = read_wav(a.se)
    print(f"诊断音效 {os.path.basename(a.se)}：{se_rate} Hz {se_ch}ch {se_bits}bit "
          f"{len(se_pcm) // (2 * se_ch) / se_rate:.3f} s")

    wrote = 0
    for stem in stems:
        src = os.path.join(a.out, f"{stem}b.wav")
        if not os.path.isfile(src):
            print(f"  ★ 跳过 {stem}：没有主素材 {os.path.relpath(src, REPO)}"
                  f"（先跑 tools/make_trance_bgm.py）")
            continue
        music, rate, ch, bits = read_wav(src)
        if bits != 16 or rate != 22050:
            print(f"  ⚠ {stem}b.wav 是 {rate} Hz/{bits}bit —— 按它的参数走")
        se2 = resample_lin(se_pcm, se_ch, se_rate, rate)
        if se_ch == 1 and ch == 2:                 # 单声道音效 → 复制到两声道
            n = len(se2) // 2
            s = struct.unpack(f"<{n}h", se2)
            se2 = struct.pack(f"<{n * 2}h", *[v for x in s for v in (x, x)])
        no = len(se2) // (2 * ch)
        fresh = 0
        loops = len(music) // len(se2)
        body = se2 * loops
        while len(body) < len(music):
            body += se2[:min(len(se2), len(music) - len(body))]
        body = body[:len(music)]
        # 峰值归一（音效原是 8-bit，直接铺会偏小；归一到 0.9 满刻度好辨认）
        n = len(body) // 2
        sv = struct.unpack(f"<{n}h", body)
        peak = max(1, max(abs(v) for v in sv))
        k = (0.9 * 32767) / peak
        body = struct.pack(f"<{n}h", *[max(-32768, min(32767, int(v * k))) for v in sv])
        p_se = os.path.join(a.out, f"{stem}b_se.wav")
        write_wav(p_se, body, rate, ch)
        print(f"  {stem}b_se.wav：音效循环铺满 {len(body) // (2 * ch) / rate:.2f} s")
        verify_wav(p_se, rate, ch, len(body), f"{stem}b_se.wav")
        fresh += len(body)

        mixed, clipped = mix(music, se2, ch, a.se_gain)
        p_both = os.path.join(a.out, f"{stem}b_both.wav")
        write_wav(p_both, mixed, rate, ch)
        print(f"  {stem}b_both.wav：音乐 + 音效（gain {a.se_gain:g}），削顶 {clipped} 个样本")
        verify_wav(p_both, rate, ch, len(mixed), f"{stem}b_both.wav")
        wrote += 2
    if not wrote:
        return 2
    print(f"\n写盘 {wrote} 个文件 → {os.path.relpath(a.out, REPO)}")
    print("切模式：在 <modkit>\\mods\\bgm\\mode.txt 里写 `se` 或 `both`（删掉 = 默认 music）；"
          "改完重进灵界即可。")
    print("同步到游戏：bash local/sync_modkit_dll.sh")
    return 0


if __name__ == "__main__":
    sys.exit(main())
