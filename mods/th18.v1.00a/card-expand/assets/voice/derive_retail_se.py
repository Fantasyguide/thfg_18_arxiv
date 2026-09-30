#!/usr/bin/env python3
"""derive_retail_se.py —— 把**别作的 `se_*.wav`** 现取到 `assets/voice/`，供音效表扩容那条通道播放。

## 为什么需要它

TH18 的 84 行音效表（71 枚 wav）里**没有**某些别作独有的 SE。要「听到原作那一枚」有两条路：
① 借一枚「像的」TH18 原生音（= ❓ 自定，玩家听得出不对）；② **把那一枚随 mod 发**。
本脚本走 ②：走本仓**已有的音效表扩容通道**（`native/sound_sites.py` 的 `id 0x54..0x73` 给 32 个新 id，
`assets/voice/ORDER.txt` 一行 = 一个新 id），加一行就得到新 SE，卡侧照旧 `ce_play_sound(id, x)` /
`ce_play_voice(NAME, x)`，**不需要**自己写播放器。

先例 = `ASTRALUP`（TH13 的 `se_astralup.wav`，灵界槽升格音）：本脚本就是它那支
`derive_astralup.py` 的表驱动版（那个文件现在只是转发到本文件，`make voice` 调的名字没变）。

## ⚠ 版权口径（本仓铁律：不许把版权字节提交进仓库）

`_src/` 与 `make_melodies.py` 产出的 wav 是**我们自己做的**，入库；
但**别作的 `se_*.wav` 是 ZUN 的原版音频** ⇒ **不能入库**。
所以：源留在 `local/`（gitignored，用户自己那份游戏），由本脚本在**构建期逐字节拷过来**，
产物写进根 `.gitignore`。
★ **逐字节拷，不重采样/不重编码** —— 音色必须与原作完全一致（`ASTRALUP` 那条的口径）。
★ 各作自己的 `dat/se_*.wav` 格式五花八门（22050/44100 Hz × 8/16 bit × 单/双声道），
  引擎的 RIFF 解析不挑格式（`engine/_shared/th18-sound-table.md` §5.2），只做「没压缩」这一条检查。

## 跑

    python3 assets/voice/derive_retail_se.py          # 逐条现取（幂等：已是最新就跳过）
    python3 assets/voice/derive_retail_se.py NAME …   # 只取其中几条

`make voice` 会自动先跑它（`make voice` → `derive_astralup.py` → 本文件 → `build_voice.py`）。
缺哪一作的样本就直接报错停下（**不要下载素材**，见 `local/README.md`）——缺的会一次全列出来。
"""
import shutil
import subprocess
import sys
import tempfile
import wave
from pathlib import Path

HERE = Path(__file__).resolve().parent
REPO = HERE.parents[4]          # renkolab/（assets/voice → assets → card-expand → th18.v1.00a → mods → renkolab）

# NAME, 版本目录, dat 内文件名, 一手出处（给读日志的人：这一枚为什么在这里）
TABLE = (
    ("ASTRALUP", "th13.v1.00c", "se_astralup.wav",
     "th13:0x43d574 `MOV EDX,0x44`（灵界槽跨格）；TH18 84 行里没有它，也无波形替身"),
    ("UFOALERT", "th12.v1.00b", "se_ufoalert.wav",
     "th12:0x44a9fe `mov eax,0x36` + 0x44aa06 `call play_sound_at_position`（UFO 出场，循环音）"),
    ("CHARGE00", "th09.v1.50a", "se_charge00.wav",
     "th09:0x41fa5e `push 0x2b` + 0x41fa65 `call 0x43e2f0`（蓄力 gauge == 0 那一帧才响 = 起手）"),
    ("CHARGEUP", "th09.v1.50a", "se_chargeup.wav",
     "th09:0x41fb31 `push 0x32` + 0x41fb38（`(int)charge/100` 变大那一帧 = **每升一级**）"),
    ("MOFPOWERUP", "th10.v1.00a", "se_powerup.wav",
     "th10:0x41b446（小 P）/ 0x41b68c（大 P）`mov ebx,0x1d; call 0x43dd10` —— 满 P 那一帧的上涨音；"
     "TH10 的 `0x1d`→`se_powerup.wav`，与 TH18 的**同名不同录音**（77278 B vs 367106 B）"),
    ("YOUKAI_FAM_SPAWN", "th08.v1.00d", "se_option.wav",
     "th08:0x41b2aa `push 0x24` + 0x41b2b1 `call 0x45d660` = **建使魔**（opcode 91 `familiarCreate`）"
     "那条尾巴的 SE（op90 `0x41b0fa`/`0x41b101`、op92 `0x41b4cb`/`0x41b4d2` **三处同形** ⇒ "
     "每建一台使魔各响一次）。SE 表 `th08:0x4c8040[0x24]` = {wav 28, vol **−800**, prio 100}；"
     "名表 `0x24` 项 = `se_option.wav`。★ 卡 108 只在**惰性绕转使魔**建壳成功时播（映射使魔那条路 "
     "不是 `familiarCreate` 的语义）"),
    ("YOUKAI_FAM_SHIFT", "th08.v1.00d", "se_ophide.wav",
     "th08:0x42c489 `push 0x28` + 0x42c490 `call 0x45d550` = `FUN_0042c420` 的「**人→妖**」分支"
     "（`flags` bit11 == 0 且玩家已是妖；同支还有 `0x42c477 push 0x2` = 环换妖侧、"
     "尾 `0x42c498 mov [enemy+0x332f],0` = enemyType 妖）。"
     "★ **2026-09-26 表基准订正**：TH08 声音表 = **`0x4c8040 + id*8`**（`+0x00` wav 号 "
     "`0x45d4ce mov ecx,[eax*8+0x4c8040]`；`+0x04` 音量；`+0x06` 第三格 = 播放入口读的那一格 "
     "`0x45d55c`）⇒ **id 0x28 → wav 32 → 名表 `0x4c81b0[32]` = se_ophide.wav**"
     "（旧注写的 `0x4c8046+id*8` 是**错位一格**的假基准，已作废）。"
     "TH18 没有它（同名文件/波形都没有）⇒ 现取；反方向（妖→人，id **0x27**）→ wav 31 = **se_opshow.wav**"
     "（TH18 同样没有 ⇒ 卡侧按 owner 口径**不播**，见 `youkai.c` 的 SE 段）"),
    ("PCB_BORDER", "th07.v1.00b", "se_border.wav",
     "th07:0x441b91 `push 0x20`（结界展开支，函数 `th07:0x441960`）→ TH07 `0x20` = `se_border.wav`；"
     "TH07 独有（TH18 71 枚里没有同名也没有同 md5 的波形）"),
    ("UFO_TOKEN_SPAWN", "th12.v1.00b", "se_piyo.wav",
     "th12:0x42741b `mov $0x37,%eax` + 0x427423 `call 0x453e20`（令牌生成那一声）；TH12 SE 表 0x4ae5a4 第 0x37 行"
     "（VA 0x4ae9f0）`+0x00` = 名表下标 44 → 名表 0x4aea50 → `se_piyo.wav`。TH18 没有这一枚 ⇒ 现取"),
    ("UFO", "th12.v1.00b", "se_ufo.wav",
     "★ owner 2026-09-23（112 #8「UFO 召唤…得有音效 se_ufo.wav」）指定；一手定位 = TH12 SE 表 `th12:0x4ae5a4` "
     "第 **0x33** 行 `+0x00` = 名表下标 **40** → 名表 `th12:0x4aea50` → `se_ufo.wav`，行属性 vol **−400** / "
     "prio 100 / loop 0（逐字段读出）。⚠ TH12 里这一枚的播放点是 0x407059/0x4077c9/0x408161/0x408a09（居中）"
     "与 0x46a836（带声像）—— **不在** `Ufo::spawn`（那里只有 0x34 + 0x36）⇒ 本槽属 **owner 口径映射**。"
     "TH18 84 行里没有这一枚 ⇒ 现取"),
)


def _extract_one(version: str, fname: str, cache_ok: bool = True) -> Path:
    """源 wav 的定位：① `local/<版本>/dat/<文件>`（已解过）；② 从 `local/<版本>/*.dat` 里**只抽这一个**
    （`thdat -x`，产物落系统临时目录 —— 不会往 `local/` 写东西）。
    ★ ② 是给「样本有 .dat、但没跑过 `tooling/thtk/unpack.py`」的作准备的（TH09/TH11 就是）——
    实现在 `tooling/thtk/unpack.py` 的 `find_tools()`（thdat 的探测口径与它一致，不另造一套）。
    """
    pre = REPO / "local" / version / "dat" / fname
    if pre.is_file():
        return pre
    vdir = REPO / "local" / version
    dats = sorted(vdir.glob("*.dat"))
    if not dats:
        raise SystemExit(f"缺样本：{vdir} 下既没有 dat/{fname}，也没有任何 .dat\n"
                         f"（用户放进 local/，见 local/README.md —— 别去下载）")
    sys.path.insert(0, str(REPO / "tooling" / "thtk"))
    import unpack as _unpack                                    # noqa: E402（要用它的 find_tools）
    _thanm, thdat, _mapdir = _unpack.find_tools()
    out = Path(tempfile.mkdtemp(prefix="derive_retail_se."))
    r = subprocess.run([str(thdat), "-C", str(out), "-x", "d", str(dats[0]), fname],
                       capture_output=True, text=True)
    got = out / fname
    if r.returncode != 0 or not got.is_file():
        raise SystemExit(f"{dats[0].name} 里抽不出 {fname}（thdat rc={r.returncode}）\n{r.stdout}{r.stderr}")
    return got


def derive(name: str, version: str, fname: str, note: str) -> tuple[bool, str]:
    """逐字节拷一份。返回 (有没有改动, 一行结果)。"""
    src = _extract_one(version, fname)
    dst = HERE / f"{name}.wav"
    with wave.open(str(src), "rb") as w:
        ch, sw, fr, n = w.getnchannels(), w.getsampwidth(), w.getframerate(), w.getnframes()
        comp = w.getcomptype()
    if comp != "NONE" or sw not in (1, 2):
        raise SystemExit(f"{src.name}: 不是未压缩的 8/16-bit PCM（comptype={comp}, 采样宽={sw}）"
                         f" —— 引擎的 RIFF 解析吃这个，别转成别的")
    if dst.is_file() and dst.read_bytes() == src.read_bytes():
        return False, f"{name}: {dst.name} 已是最新（与 {version} 的样本逐字节相同）"
    shutil.copyfile(src, dst)
    return True, (f"{name}: 现取 {version}/{src.name} -> {dst.name}"
                  f"（{ch} ch / {sw * 8} bit / {fr} Hz / {n} 帧，逐字节拷）")


def main() -> int:
    want = set(sys.argv[1:])
    todo = [r for r in TABLE if not want or r[0] in want]
    if not todo:
        raise SystemExit("给的 NAME 一个都不在表里：" + ", ".join(r[0] for r in TABLE))
    changed = 0
    for name, version, fname, note in todo:
        did, line = derive(name, version, fname, note)
        changed += did
        print(line + ("" if did else "（跳过）"))
        print(f"    一手：{note}")
    print(f"derive_retail_se: {len(todo)} 条，更新 {changed} 条")
    return 0


if __name__ == "__main__":
    sys.exit(main())
