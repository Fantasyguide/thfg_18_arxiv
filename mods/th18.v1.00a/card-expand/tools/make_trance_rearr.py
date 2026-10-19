#!/usr/bin/env python3
"""make_trance_rearr.py —— 用第三方同人 MIDI 生成**灵界版重编曲**素材（主旋律压低、伴奏抬高）。

    python3 tools/make_trance_rearr.py --stem th18_03 --stem th18_04     # 先做 1 面那两首
    python3 tools/make_trance_rearr.py                                  # 全部（读 MANIFEST 里有的）
    python3 tools/make_trance_rearr.py --render=naive --stem th18_03    # 老合成器（兜底 / A-B 对照）
    python3 tools/make_trance_rearr.py --stem th18_03 --prog-gain 56=-4.4 --low-db 2.3
                                                                        # 临时按音色调（dB）

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
   ★ **调音按音色（2026-10-19 起，owner 口径「从音色是什么冲调 arrange」）**：文件头两张表全是 **dB** ——
   `GLOBAL`（17 首通用，目前只有 `drums_db = +2.5`）与 `PER_TRACK`（单曲例外，键 = 内部曲名）。
   每音的增益按优先级取**一条**：**鼓（ch9 / bank 128）> melody 轨 > `prog_db`（GM program 表）
   > 倍斯 program 32–39 > 低音区乐器 > 1.0**。CLI 可临时覆盖：`--drums-gain` / `--prog-gain 56=-4.4`
   （支持 `56-63=-3` 区间）/ `--melody-db` / `--bass-db` / `--low-db` / `--track-config <json>`；
   旧的线性口径 `--melody-gain 0.35` / `--bass-gain 1.6` 仍可用（内部换算成 dB）。
   ★ **换音色 / 换 kit 时冻结母带与时间轴（2026-10-19 A/B 口径）**：`--master-peaks <json>`
   给每首一个**改版前的 pre-norm 峰值**（增益 = `--peak 0.92` / 那个峰值），`--starts <json>`
   给每首**改版前的起点**（不再跑 `fine_align`）⇒ 除这次改的音色外，其它乐器**逐采样不变**、
   A/B 可以在同一秒直接切换。`--peak-report` / `--start-report` 把本次的值写出来，供下一轮用。
   两项都不给 ⇒ 照旧：逐文件峰值归一 + 每次重新 fine_align。
   ★★ **母带按响度匹配（2026-10-19 owner 口径）**：峰值归一不是响度 —— 同一峰值下两首的
   积分响度能差 6 LU。`--loudness-match original` = **逐曲**把 `_rearr` 的积分响度
   （LUFS，ITU-R BS.1770-4 / EBU R128）顶到**该曲原曲循环体**的响度；`lufs:<值>` /
   `peak:<dBFS>` 是另外两种目标。超过 `--peak-ceiling`（默认 **−0.3 dBFS**）时走
   **lookahead 软限幅**（`soft_limit`：±5 ms 窗取所需衰减、dB 域释放 60 dB/s、**不加延时**），
   不是简单缩放。量测用同目录 `measure_loudness.py`（自带 BS.1770，不依赖 ffmpeg）。
   ★★ **母带 EQ（2026-10-20 owner 第三轮口径）**：`--eq master` 才生效（**默认 off ⇒ 既有 17 首
   逐采样不变**）。表在文件头 `EQ_GLOBAL` / `PER_TRACK_EQ`：全曲**低架 +4 dB @100 Hz**（补 SF2
   重编曲链的下潜不足）；`th18_12` 低架 **−4 dB**（它 60–120 过量 +10.8 dB）；
   `th18_13` **高架 −3 dB @4 kHz + 20 Hz 高通**（压高频 + 治直流偏移）。处理顺序
   **高通 → 低架 → 高架 → 响度匹配**（EQ 在限幅器前 = 正常母带链；因果滤波 ⇒ 不加预响、
   不改帧数/时间轴）。临时试：`--low-shelf -4@100` / `--high-shelf -3@4000` / `--highpass 20`
   / `--eq-json '{"th18_13": {"high_shelf_db": -5}}'`。
   ★★ **2026-10-20 全量 17 首定稿**（owner「就这样吧，可以结了」）：`th18_13` **保持 −3 dB @4 kHz
   + 20 Hz 高通**；17/17 出盘帧数与现役逐首相等（Δ=0），响度对原曲 **|ΔLUFS| ≤ 0.02**。
   出盘命令 = `--eq master --loudness-match original`（逐曲结果见 `TUNING.md` §2.0h-3）。
3. **渲染**（`--render`）：
   - **`sf2`（默认）** = **GM SoundFont 真采样渲染**（2026-10-19 起）：把调音表写进 MIDI 的
     **note-on velocity**（每音按"鼓 / melody / program / 倍斯 / 低音区"取一条增益，
     超 127 时**整体等比缩**保相对关系）⇒ 存临时 `.mid` ⇒
     `fluidsynth -F out.wav -r 44100 -g 1.0 <sf2> <mid>` ⇒ 读回 PCM ⇒ 44100→22050（FIR 半带抽取）
     ⇒ 22050 Hz 立体声 16 bit —— 与引擎里其它素材同格式（`trance_bgm_core` 的 fmt 记录口径）。
     **为什么改 MIDI 而不是在合成器里分轨**：`pick_melody` 判出来的是**轨**，而 fluidsynth 只吃
     MIDI；把增益写进 velocity = 每个音各算各的，比"按通道分几次渲染再相加"简单且不会漏掉
     同通道不同角色的音。velocity→振幅实测**近似线性**（见 `_sf2_velocity_gain`），
     所以线性增益 g 直接当 velocity 因子用。
     ★ **鼓组 bank 修补（2026-10-19 owner：ch15 那层「假钢琴鼓」必须改真鼓）**：
       本批 5 首（内部 `th18_13/14/16/17/18`）把**第二层鼓**写在 **ch15**、而 MIDI 里
       **一个 bank select 都没有** ⇒ fluidsynth 按 prog 0 把它渲成**钢琴**。⇒ 渲染前在
       **临时 MIDI**（不动源 MIDI）里给这些通道补 `CC0=1 + CC32=0`（MMA 14-bit 口径 =
       **bank 128**）+ fluidsynth 加 `-o synth.midi-bank-select=mma`（默认 `gs` 会**直接
       忽略** CC0，实测见 `drum_bank_patch`）。**只对"确认是鼓"的通道**下手：通道 ≠ 9、
       该轨的音色/轨道名命中鼓组关键字（`Drum Kit` / `GA5` / `Elementics`）、且**全部音高
       落在 GM 打击乐键域 27–87**。
       ⚠ **实测：这 5 首的 ch15 在源 MIDI 里是 `CC7=0`（整层静音）** ⇒ 补 bank 对成品
       `_rearr` **零听感变化**（补/不补逐采样差 −48 dB）；真能听到"假钢琴鼓"的是 **naive**
       老变体（`*_both.wav` / `*_se.wav`：naive 只认 ch9 为鼓、也不认 CC7）。要让它真的响
       得用 `--drum-bank-unmute 100`（默认关，等 owner 拍板）。
   - **`naive`（兜底，2026-09-14 那版）** = 自写小型合成器（GM program 家族 → 锯齿/脉冲/正弦 + ADSR，
     鼓用噪声包络）。**没有 fluidsynth / 没有 sf2 的机器**靠它仍能出全套素材。
   ★ **鼓 kit / ZUN 号覆盖表（2026-10-19 owner 定：`KIT_PROGRAM` / `TRUMPET_REMAP`）**：
     渲染前在**临时 MIDI**（不动源 MIDI）里把 program change 的**值字节原地改掉**：
     · **鼓通道（ch9，以及工具补过 bank128 / 源 MIDI 里就是 bank128 的鼓层，含 ch15）一律
       `KIT_PROGRAM = 25`** —— thfont 里 bank128 prog25 = `TR-909 [SC-55]`（808/909 血统的电鼓），
       替换掉原来的 GM 默认 `STANDARD 1`(prog0) / `POWER`(prog16)。ch15 那层在源 MIDI 里是
       `CC7=0` 静音（见 `drum_bank_patch`），照样设 25 —— 对成品零听感变化，只为"万一放开"时同套 kit。
     · **轨名命中 `Romantic Tp`（ZUN 号）的轨**：按 `TRUMPET_REMAP` 把 `61`（Brass Section）/
       `58`（Tuba）改成 `56`（Trumpet；thfont bank0 prog56 的 preset 名就叫 `Romantic Tp`）。
       **轨名没有依据的轨一律不动**（本批 61 全部带 `Romantic Tp` 轨名，故 61→56 的命中面 = 这些轨）。
     · `--no-program-override` 关掉这张表（A/B 对照、或出"改版前 pre-norm 峰值"用）。
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
import json
import math
import os
import re
import shutil
import struct
import subprocess
import sys
import tempfile
import wave

import numpy as np

HERE = os.path.dirname(os.path.abspath(__file__))
if HERE not in sys.path:                          # 响度模式要 import 同目录的 measure_loudness
    sys.path.insert(0, HERE)
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

# ---------------------------------------------------------------- 调音表（**按音色**，dB）
# ★ 方法论（owner 2026-10-19 第二轮原话）：**「从音色是什么冲调 arrange」** ——
#   不要按 "melody / bass" 这种粗分轨调，要按「**这条轨用的是什么音色**」（GM program / 鼓通道）给增益。
#   所以下面两张表的单位全是 **dB**（velocity→振幅实测近似线性，见 `_sf2_velocity_gain`；
#   g_lin = 10^(dB/20)）。优先级（每音只命中一条，不叠乘）：
#       **鼓（ch9 / bank 128）> melody 轨 > 指定 program > 倍斯 program 32–39 > 低音区乐器 > 1.0**
#
# `GLOBAL` = 17 首全部生效；`PER_TRACK` = 单曲覆盖（键 = **内部曲名**，不是 wav 名）。
# ⚠ 单曲例外**不要**摊到全量：owner 只点了 03/07/08 三首。

GLOBAL = {
    # ★ owner：「整体灵界可以把鼓的声音放大」⇒ 17 首全加。
    #   只认 **GM 鼓**：ch9（GM 规定的鼓通道）或 **bank 128** 的鼓组。
    #   ★ 2026-10-19 owner：「ch15 那层假钢琴鼓**肯定得改成真鼓**」——实测本批 5 首
    #     （内部 `th18_13/14/16/17/18`）把**第二层鼓**写在 **ch15**（音色名 `GA5 (Elementics)` /
    #     `Triebwerk (Drum Kit 6)`）且 MIDI 里**没有任何 bank select** ⇒ 若放开就是 prog 0 钢琴。
    #     现在 `drum_bank_patch()` 给这些通道补 bank select（MMA 口径 CC0=1+CC32=0 = bank 128）
    #     ⇒ 走**真鼓组**，并因此吃这条 +2.5 dB。
    #   ⚠ 但这 5 首的 ch15 在**源 MIDI 里是 `CC7=0`（整层静音）** ⇒ 补 bank 对成品零听感变化
    #     （补/不补逐采样差 −48 dB）。"假钢琴鼓"只在 **naive** 老变体（`*_both.wav` / `*_se.wav`）里听得到。
    "drums_db": +2.5,
}

PER_TRACK = {
    # ── 03 一面道中 ── owner 第一轮：「zun 号可以再压，低音的其他乐器可以再加」
    #   · zun 号 = ZUNpet = SD-90 `Romantic Tp`：本曲 MIDI（`th18_02.mid`）track7 / ch4 / **GM 56**
    #     （它**不在** melody 集里 —— melody 是 track5「Mantra Melody」ch2 GM 80）⇒ 原来 1.0。
    #     56–58 一起压（小号/长号/大号 = 铜管低音区也在内），取 **−4.4 dB**（= 第一轮验证过的 ×0.6）。
    #   · 低音区其它乐器：**按乐器**（轨+通道）判「平均音高 < 60 (C4)」⇒ 整件 +2.3 dB（= ×1.3）。
    #     为什么不是「平均音高 < 48」：实测本曲 <48 的音符**全在**两个倍斯通道（ch5/ch6，已吃 bass），
    #     照 48 等于零改动；真正被漏掉的是 ch1「Model D Close」钢琴（均 56.8，占全曲能量 33.6%）。
    #     它和 ch0 是同一台琴的八度叠奏 ⇒ **按乐器抬**（逐音抬会把八度关系弄歪）。
    #   · melody 0.35 → 0.30（−10.5 dB，owner 第一轮「再压」）。
    "th18_03": dict(melody_db=-10.5, prog_db={56: -4.4, 57: -4.4, 58: -4.4},
                    low_db=+2.3, low_mean_max=60),
    # ── 07 三面道中 ── owner 第二轮：「07b 的主旋律弦乐可以压」
    #   ⚠ **本曲 MIDI 里没有 40–47 弦乐**（Karl Zuñiga 这版把弦乐声部换成了
    #     prog 1 钢琴 6% / prog 25 十二弦钢吉他 34.2% / prog 66 次中音萨克斯 7.6% / prog 78 哨音 7.2%）。
    #     ⇒ 按「**主旋律**再压 3 dB」落地（melody 轨 = track4/ch1/prog 1）；40–47 那张表照样写着
    #     （命中 0 个音），是为了口径可追溯 —— 真按"弦乐"点名要等 owner 指定是哪个音色。
    "th18_07": dict(melody_db=-12.1,
                    prog_db={p: -3.0 for p in range(40, 48)}),
    # ── 08 三面 boss ── owner 第二轮：「08b 的号也得压」
    #   本曲"号"= track10/ch5/**GM 61 (`BRASS`)**（16.6% 能量，源轨名 `Romantic Tp`）
    #   ⇒ 铜管族 56–63 一起 −3 dB（本曲实际命中 61）。melody 是 SawLead(81)，owner 没点，不动。
    "th18_08": dict(prog_db={p: -3.0 for p in range(56, 64)}),
}

# 默认 melody / 倍斯（历史口径的线性增益换算成 dB：0.35 / 1.6）
DB_MELODY = 20 * math.log10(0.35)      # −9.1 dB
DB_BASS = 20 * math.log10(1.6)         # +4.1 dB


# ---------------------------------------------------------------- 母带 EQ 表（逐曲，2026-10-20 owner 第三轮口径）
# ★ owner 2026-10-20 拍板（原话「响度 4db，th18_12 可以小修 −4db，th18_13 得压高频」）：
#   · **全曲统一低架 +4 dB @100 Hz** —— 试点实测把 th18_14 的 sub<60 与原曲之差
#     −5.0 → −1.5 dB（60–120 只多 1 dB）；这是对整条 SF2 重编曲链下潜不足的补偿。
#   · **th18_12（5 面 boss）低架 −4 dB** —— 它 60–120 在同响度下**过量 +10.8 dB**
#     （低频被压扁，crest 8.8 vs 原曲 12.7）⇒ 覆盖全局 +4，低架取绝对 −4。
#   · **th18_13（6 面道中）高架 −3 dB @4 kHz + 20 Hz 高通** —— owner「压高频」；
#     20 Hz 高通顺手治它实测的**直流偏移**（均值 −0.0272 / <20 Hz −16.4 dB，其余三首 −28…−35）。
# 口径：
#   · **默认关**（`--eq off`）⇒ 不给开关时 17 首的既有参数与产物**逐采样不变**（向后兼容）。
#   · 只有 `--eq master` 才套 EQ_GLOBAL + PER_TRACK_EQ；`--eq-json` / `--low-shelf` /
#     `--high-shelf` / `--highpass` 可临时覆盖（A/B 用）。
#   · 处理顺序 **高通 → 低架 → 高架**，在**响度匹配之前**（正常母带链：EQ 在限幅器前）。
#   · 键：low_shelf_db / low_shelf_hz / high_shelf_db / high_shelf_hz / highpass_hz。
EQ_GLOBAL = {"low_shelf_db": +4.0, "low_shelf_hz": 100.0}

PER_TRACK_EQ = {
    # 5 面 boss：低频过量（60–120 +10.8 dB）⇒ 低架**绝对 −4 dB**（不是 +4 再 −4）。
    "th18_12": {"low_shelf_db": -4.0},
    # 6 面道中：压高频（高架 −3 dB @4 kHz，f0 处 ≈ −1.5 dB、>8 kHz ≈ −3 dB）
    # + 20 Hz 高通（治直流偏移，不吃音乐频段）。
    "th18_13": {"high_shelf_db": -3.0, "high_shelf_hz": 4000.0, "highpass_hz": 20.0},
}

EQ_KEYS = ("highpass_hz", "low_shelf_db", "low_shelf_hz", "high_shelf_db", "high_shelf_hz")


# ---------------------------------------------------------------- 鼓 kit / ZUN 号覆盖表
# ★★ 2026-10-19 owner 拍板（依据 `local/th18.v1.00a/midi/TIMBRE-AUDIT.md` 的音色审计 +
#   `灵界BGM试听/鼓组A-B` 的听感 A/B）：
#   · **鼓 = kit 25**（TR-808 / SD Tekno 系）：17 首的 **ch9 program 一律 25**；ch15 那层
#     （5 首的第二套鼓、源 MIDI 里 `CC7=0` 静音）**也设 25**（对成品零听感变化，口径统一）。
#     thfont.sf2 的 bank128 prog25 preset 名 = `TR-909 [SC-55]`（audit 按一般表叫 TR-808；
#     两者都是 808/909 血统，与默认 STANDARD 1 / POWER 明显不同族）。
#   · **ZUN 号 = 56**：旋律轨里 `61`（Brass Section）/ `58`（Tuba）→ `56`（Trumpet），
#     **只改轨名命中 `Romantic Tp` 的轨**（audit：本批多数 61、少数本来就是 56、1 首 58 ——
#     所以不能盲改 61，必须按轨名判）；**轨名没有依据的轨一律不动**。
#   做法 = 渲染前把**临时 MIDI** 的 program change 值字节原地改（不增删事件 ⇒ 时间轴不动）。
KIT_PROGRAM = 25
TRUMPET_PROGRAM = 56
# 61 = Brass Section（本批 "ZUN 号" 误用的 GM 音色）；58 = Tuba（`th18_04` 的 Romantic Tp 轨）。
# 56 本身已经是目标 ⇒ 不在表里，命中轨原样保留。
TRUMPET_REMAP = {61: TRUMPET_PROGRAM, 58: TRUMPET_PROGRAM}
# `SD-90 (Romantic Tp)` / `Romantic Tp` / `ZUNpet` 都算；别把 `Bari.Sax` 之类捎进来。
TRUMPET_TRACK_RE = re.compile(r"romantic\s*tp|trumpet|zun\s*pet", re.I)



# ---------------------------------------------------------------- SMF 解析
def _vlq(b, i):
    v = 0
    while True:
        c = b[i]; i += 1
        v = (v << 7) | (c & 0x7F)
        if not (c & 0x80):
            return v, i


def parse_smf(path, velout=None, info=None, ccout=None, pcout=None):
    """→ (tracks, ppq, tempos)。tracks[i] = [(tick, 'on'/'off'/'prog', ch, a, b)]（按 tick 升序）。

    `velout`（list，可选）：把每条 note-on 的 **velocity 字节在文件里的绝对偏移**追加进去，
    形如 `(轨号, tick, 通道, 音高, velocity, 偏移)` —— sf2 路径靠它**原地改 velocity**
    （比"重新生成一个 SMF"安全：所有 meta/sysex/CC 原样保留）。

    `info`（list，可选）：逐轨追加 `{"names": [轨道名/音色名...], "notes": {ch: [音高...]}}`
    —— 给 `drum_bank_patch` 判"这条轨是不是鼓"用（音色名 + 音域），**不改**事件流。

    `ccout`（list，可选）：把 CC7（通道音量）的**值字节绝对偏移**追加进去，
    形如 `(轨号, tick, 通道, 值, 偏移)` —— `--drum-bank-unmute` 靠它把 `CC7=0` 原地抬高。

    `pcout`（list，可选）：把 program change（0xC0）的**值字节绝对偏移**追加进去，
    形如 `(轨号, tick, 通道, program, 偏移)` —— 鼓 kit / ZUN 号覆盖表（`program_override`）
    靠它**原地改值**（与 `velout`/`ccout` 一样，只改字节、不增删事件 ⇒ 时间轴不动）。
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
        names, note_ch = [], {}
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
                elif typ in (0x03, 0x04):                  # 轨道名 / 乐器名（鼓组识别的依据）
                    names.append(data.decode("cp932", "replace").strip())
            elif st in (0xF0, 0xF7):
                ln2, i = _vlq(body, i); i += ln2
            elif hi in (0x80, 0x90, 0xA0, 0xB0, 0xE0):
                a, b = body[i], body[i + 1]
                vel_at = body_off + i + 1            # ← b 的绝对偏移（在 i 前进之前算）
                i += 2
                if hi == 0x90 and b > 0:
                    evs.append((tick, "on", ch, a, b))
                    note_ch.setdefault(ch, []).append(a)
                    if velout is not None:
                        velout.append((ti, tick, ch, a, b, vel_at))
                elif hi == 0x80 or (hi == 0x90 and b == 0):
                    evs.append((tick, "off", ch, a, 0))
                elif hi == 0xB0 and a in (0, 7, 32):
                    # 只留 bank select（CC0/CC32）+ 通道音量（CC7）：判鼓组（bank 128）与
                    # 「这层是不是被 CC7=0 静音了」都要用；其余 CC 不装
                    evs.append((tick, "cc", ch, a, b))
                    if ccout is not None and a == 7:
                        # ⚠ 这里 `i` **已经走过了两个数据字节**（见上面 `i += 2`）⇒ 值在 `i-1`
                        ccout.append((ti, tick, ch, b, body_off + i - 1))
            elif hi in (0xC0, 0xD0):
                # ⚠ program change 的值字节（0xC0 的 a）在这里、**i 还没前进** ⇒ 绝对偏移 = body_off+i
                val_at = body_off + i
                a = body[i]; i += 1
                if hi == 0xC0:
                    evs.append((tick, "prog", ch, a, 0))
                    if pcout is not None:
                        pcout.append((ti, tick, ch, a, val_at))
            else:
                break
        tracks.append(evs)
        if info is not None:
            info.append({"names": names, "notes": note_ch})
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


# ---------------------------------------------------------------- 鼓组 bank 修补（ch15 假鼓 → 真鼓）
# ★★ 2026-10-19 owner：「ch15 那层**假钢琴鼓**肯定得改成真鼓」。
#   现象：内部 `th18_13/14/16/17/18` 五首里有一层鼓写在 **ch15**（音色名 `GA5 (Elementics)` /
#   `Triebwerk (Drum Kit 6)`，prog 0）且整份 MIDI **一个 bank select 都没有** ⇒ fluidsynth 按
#   prog 0 把它渲成**钢琴**（音高 33–53 的低音区钢琴 = 听起来像"假鼓"）。
#   做法：渲染前给这些通道补 **bank select = bank 128（鼓组）**，只写进**临时 MIDI**，源 MIDI 不动。
#
# ⚠ **bank 128 的正确写法**（实测 2026-10-19，fluidsynth 2.2.5 + thfont.sf2）：
#   · CC 的值域是 7 bit（0–127），**`CC0=128` 根本发不出来**（0x80 会被当成 status 字节）。
#     SF2 的 bank 号是 14 bit：`bank = CC0(MSB) × 128 + CC32(LSB)` ⇒ **bank 128 = CC0=1 + CC32=0**。
#   · 但 fluidsynth 默认 `synth.midi-bank-select = gs`，**GS 模式直接忽略 CC0**（实测 CC0=1 与
#     不发的渲染**逐采样相同**，rms/质心都一样）；`gm`/`xg` 同样不认。只有 `mma` 按 14 bit 收
#     ⇒ 所以渲染时加 `-o synth.midi-bank-select=mma`。
#   · 实测判据：同一串鼓音符（36/38/42/49）渲在 ch9（GM 鼓）vs ch15+CC0=1+mma ⇒ 波形相关 **+1.000**
#     （centroid 5766 Hz = 打击乐）；不发 CC0 / 发 CC0=0 / gs+CC0=1 ⇒ 相关 −0.04（centroid 959 Hz = 钢琴）。
#   · 本批 17 份 MIDI 除本次补的以外**没有任何 CC0/CC32**（逐文件扫过）⇒ 切 mma 对其余 12 首零影响。
#
# ★★ **但这层在源 MIDI 里是静音的**（2026-10-19 实测，比 bank 更要紧）：
#   这 5 首的 ch15 轨道都有一条 `CC7=0`（通道音量 0）@tick 0 ⇒ **整层发不出声**（fluidsynth 实测：
#   CC7=0 → −75 dB）。⇒ 补 bank **不会改变成品听感**（本工具补/不补的 `_rearr` 逐采样差 −48 dB）；
#   真正能听到"假钢琴鼓"的是 **naive** 老变体（`*_both.wav` / `*_se.wav`）：naive 合成器只认 ch9
#   为鼓、也不认 CC7 ⇒ 那层在 naive 里就是低音区钢琴（音高 33–53）。
#   ⇒ 要不要让这层**真的响**是 mixing 决定，不是 bug 修：`--drum-bank-unmute CC7` 可抬回来试听，
#     **默认不动**（等 owner 拍板；实测 th18_16 抬到 CC7=100：sub<60 +4.7 dB / 60–250 +5.5 dB）。
DRUM_NAME_RE = re.compile(r"drum\s*kit|drumkit|\bGA5\b|elementics", re.I)
GM_DRUM_LO, GM_DRUM_HI = 27, 87      # GM 打击乐键域（B1–D#6 一带有意义的鼓键；超出就不是鼓）


def drum_bank_patch(raw, info):
    """→ (新 MIDI 字节, hits)。`hits[ch] = [(轨号, 音色名, 最低音, 最高音, 音符数)]`。

    **只对确认是鼓的通道**补 bank select（写法见上面的注释），判据三条全中：
      ① 通道 ≠ 9（ch9 按 GM 本来就是鼓组，不需要补）；
      ② 该轨的**轨道名/音色名**命中 `DRUM_NAME_RE`（`Drum Kit` / `GA5` / `Elementics`）；
      ③ 该通道**全部音高**落在 GM 打击乐键域 `27–87`、且至少 8 个音（排除"名字撞车但其实是旋律"的轨）。
    插入点 = 该轨 body 的最前面（delta 0，tick 0）：`CC0=1` + `CC32=0`（= bank 128）。
    同一通道若在多条轨出现，则每条含它的轨都插一次（幂等，tick 0 重复设 bank 无害）。
    """
    hits = {}
    for ti, meta in enumerate(info):
        name = " / ".join(meta["names"])
        if not name or not DRUM_NAME_RE.search(name):
            continue
        for ch, pitches in sorted(meta["notes"].items()):
            if ch == 9 or len(pitches) < 8:
                continue
            lo, hi = min(pitches), max(pitches)
            if lo < GM_DRUM_LO or hi > GM_DRUM_HI:
                continue
            hits.setdefault(ch, []).append((ti, name, lo, hi, len(pitches)))
    if not hits:
        return raw, hits
    # ⚠ MThd 头块固定 14 字节（`MThd` + len=6 + 6 字节 body）：从 **14** 开始走块，前 14 字节原样保留。
    out, pos, ti = bytearray(raw[:14]), 14, 0
    while pos + 8 <= len(raw):
        sig = raw[pos:pos + 4]
        ln = struct.unpack_from(">I", raw, pos + 4)[0]
        body = raw[pos + 8:pos + 8 + ln]
        if sig == b"MTrk":
            inject = b""
            for ch in sorted(hits):
                if ch in info[ti]["notes"]:
                    inject += bytes([0, 0xB0 | ch, 0, 1, 0, 0xB0 | ch, 32, 0])
            if inject:
                body = inject + body
            out += b"MTrk" + struct.pack(">I", len(body)) + body
            ti += 1
        else:
            out += sig + struct.pack(">I", ln) + body
        pos += 8 + ln
    if pos < len(raw):
        out += raw[pos:]
    return bytes(out), hits


def drum_bank_names(hits):
    """→ 一行摘要（"ch15 → bank128（GA5 (Elementics)，4481 音）"）。"""
    parts = []
    for ch, items in sorted(hits.items()):
        name = items[0][1]
        if len(items) > 1:
            name += f" 等 {len(items)} 轨"
        parts.append(f"ch{ch} → bank128（{name}，{items[0][4]} 音）")
    return " / ".join(parts)


# ---------------------------------------------------------------- 鼓 kit / ZUN 号（program 覆盖）
def program_override(raw, info, pc_out, drum_ch):
    """→ (改过的 MIDI 字节, hits, seen)。

    · **`KIT_PROGRAM`**：`drum_ch` 里**每一条** program change → 25（ch9 / 补过 bank128 的 ch15）；
    · **`TRUMPET_REMAP`**：**只对轨名命中 `TRUMPET_TRACK_RE` 的轨**（61 Brass Section / 58 Tuba → 56）。
      轨名没有依据的轨**一个字节不动**（`seen` 只装轨名命中的那些，供报表逐曲列出）。
    只改 PC 的**值字节**（不增删事件 ⇒ 所有后续偏移仍有效；`pc_out` 来自补 bank 之后的那次解析）。
    """
    raw = bytearray(raw)
    names = [" / ".join(m["names"]) for m in info]
    hits, seen = [], []
    for ti, tick, ch, prog, at in pc_out:
        assert raw[at] == prog, f"PC 偏移不自洽：track {ti} tick {tick} ch{ch} @{at}"
        name = names[ti] if ti < len(names) else ""
        if ch in drum_ch:
            if prog != KIT_PROGRAM:
                raw[at] = KIT_PROGRAM
                hits.append(dict(track=ti, name=name, tick=tick, ch=ch,
                                 old=prog, new=KIT_PROGRAM, why="kit"))
        elif name and TRUMPET_TRACK_RE.search(name):
            new = TRUMPET_REMAP.get(prog, prog)
            seen.append(dict(track=ti, name=name, tick=tick, ch=ch, old=prog, new=new))
            if new != prog:
                raw[at] = new
                hits.append(dict(track=ti, name=name, tick=tick, ch=ch,
                                 old=prog, new=new, why="trumpet"))
    return raw, hits, seen


def program_override_summary(hits, seen):
    """→ 一行摘要（"kit25 ch9（1 个 PC：16→25）；ZUN 号 tr6/ch4 61→56「Romantic Tp」"）。"""
    parts = []
    kit = {}
    for h in hits:
        if h["why"] == "kit":
            kit.setdefault(h["ch"], []).append(h["old"])
    if kit:
        parts.append("kit25 " + ", ".join(
            f"ch{ch}（{len(v)} 个 PC：{','.join(map(str, sorted(set(v))))}→{KIT_PROGRAM}）"
            for ch, v in sorted(kit.items())))
    if seen:
        # 同一轨常有多条同值 PC（本批每轨 5 条，DAW 按段重发）⇒ 按 (轨/通道/旧→新) 去重再报
        uniq = []
        for s in seen:
            k = (s["track"], s["ch"], s["old"], s["new"], s["name"])
            if k not in [(u["track"], u["ch"], u["old"], u["new"], u["name"]) for u in uniq]:
                uniq.append(s)
        parts.append("ZUN 号 " + ", ".join(
            (f"tr{s['track']}/ch{s['ch']} {s['old']}→{s['new']}「{s['name']}」"
             if s["old"] != s["new"] else
             f"tr{s['track']}/ch{s['ch']} 已是 {s['new']}「{s['name']}」（不动）")
            for s in uniq))
    return "；".join(parts)


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


def _db2lin(db):
    """dB → 线性振幅倍数（velocity 因子）。"""
    return 10.0 ** (float(db) / 20.0)


def render_sf2(mid_path, sf2, fs, fs_libdir, melody_tracks, prog_by_ch, notes,
               mel_gain, bass_gain, log=print, tune=None, drum_bank=True, unmute_cc7=None,
               program_override_on=True):
    """SoundFont 渲染：改 velocity 的临时 MIDI ⇒ fluidsynth 渲 44100 Hz wav ⇒ 读回 ⇒ 降到 22050。

    返回 (n,2) float32 @ SR —— 与 `render_naive` 同接口，后面的 fine_align / 截断 / 归一化
    **一个字都没改**（换的只是音源，时间轴不动）。

    `tune` = 调音表（见文件头 `GLOBAL` / `PER_TRACK`，**单位 dB**）：
      · `drums_db` —— **鼓**（ch9，或 bank select 指到 **bank 128** 的鼓组通道）；
      · `melody_db` —— `pick_melody` 选的 melody 轨；
      · `prog_db` = {GM program: dB} —— 按 **program change** 命中（"从音色是什么调"的关键旋钮）；
      · `bass_db` —— 倍斯 program 32–39；
      · `low_db` / `low_mean_max` —— **按乐器**（轨+通道）判低音区：平均音高 < `low_mean_max`
        就整件 +`low_db`。★ 不按单音抬：伴奏常是**八度叠奏**，逐音只抬低八度会把八度关系弄歪。
    优先级（每音只命中一条，不叠乘）：鼓 > melody 轨 > `prog_db` > 倍斯 > 低音区乐器 > 1.0。

    `drum_bank`（默认开）：渲染前跑 `drum_bank_patch()`，把"确认是鼓但不在 ch9"的通道
    补 bank select 到 **bank 128**（ch15 假钢琴鼓修复；`--no-drum-bank` 关掉，仅供 A/B）。
    ⚠ 本批这 5 首的 ch15 在**源 MIDI 里就是 `CC7=0`（整层静音）** ⇒ 补 bank 只保证"万一有人
    把这层放开，它是真鼓"，对成品**零听感变化**（实测与 `--no-drum-bank` 逐采样相同）。
    `unmute_cc7`（默认 None）：把这层静音的 `CC7=0` 抬到给定值（`--drum-bank-unmute`），
    用来试听"真鼓放出来是什么样"；**默认不动**（包内素材保持现状，等 owner 拍板）。
    `program_override_on`（默认开）：跑 `program_override()` —— 鼓通道 → **kit 25**、
    `Romantic Tp` 轨 61/58 → **56**（2026-10-19 owner 拍板，见文件头 `KIT_PROGRAM` / `TRUMPET_REMAP`）。
    `--no-program-override` 关掉（A/B 对照 / 出改版前峰值）。
    """
    tune = tune or {}
    drums_gain = _db2lin(tune.get("drums_db") or 0.0)
    mel_gain = _db2lin(tune["melody_db"]) if "melody_db" in tune else mel_gain
    bass_gain = _db2lin(tune["bass_db"]) if "bass_db" in tune else bass_gain
    prog_gain = {int(k): _db2lin(v) for k, v in (tune.get("prog_db") or {}).items()}
    low_gain = _db2lin(tune.get("low_db") or 0.0)
    low_mean_max = float(tune.get("low_mean_max") or 60)
    drum_hit = pg_hit = low_hit = 0
    tmp = tempfile.mkdtemp(prefix="rearr-sf2-")
    try:
        # ① 鼓组 bank 修补（ch15 假鼓）：只改**临时 MIDI**，源 MIDI 一个字节不动
        mid_src = mid_path
        hits = {}
        if drum_bank:
            info = []
            parse_smf(mid_path, info=info)
            patched, hits = drum_bank_patch(open(mid_path, "rb").read(), info)
            if hits:
                mid_src = os.path.join(tmp, "patched.mid")
                open(mid_src, "wb").write(patched)
                log(f"    [drum-bank] {os.path.basename(mid_path)}: {drum_bank_names(hits)}")
        vol, cc_out, pc_out = [], [], []
        # 只为拿 velocity / CC7 / PC 的字节偏移（**补过 bank 的文件重新解析**，偏移才对）
        # `info2` 同时装轨道名：program_override 要按轨名判 "ZUN 号"（Romantic Tp）。
        info2 = []
        tracks, _, _ = parse_smf(mid_src, velout=vol, ccout=cc_out, pcout=pc_out, info=info2)
        # 鼓通道：ch9（GM 规定）+ 本次补过 bank 的通道
        #   + 任何 CC0=1（MMA 14-bit 口径 = bank 128）的通道
        drum_ch = ({9} | set(hits)
                   | {ch for evs in tracks for tick, k, ch, a, b in evs
                      if k == "cc" and a == 0 and b == 1})
        # 每件乐器（轨+通道）的平均音高 —— 判它是不是"低音乐器"
        agg = {}
        for ti, tick, ch, note, vel, at in vol:
            if ch not in drum_ch:
                s, n = agg.get((ti, ch), (0, 0))
                agg[(ti, ch)] = (s + note, n + 1)
        mean_pitch = {k: s / n for k, (s, n) in agg.items()}
        low_inst = {k for k, mp in mean_pitch.items()
                    if mp < low_mean_max
                    and (prog_by_ch.get(k[1]) or 0) not in range(32, 40)   # 倍斯另有 bass_db
                    and k[0] not in melody_tracks}                          # melody 轨另有 melody_db
        raw = bytearray(open(mid_src, "rb").read())
        # ★ 鼓 kit 25 / ZUN 号 56（2026-10-19 owner 定）：只改临时 MIDI 的 program change 值字节
        if program_override_on:
            raw, prog_hits, prog_seen = program_override(raw, info2, pc_out, drum_ch)
            # 调音表按 **program** 命中：改过的通道要让增益表看到**新** program
            # （本批 08 的 56–63 表早已覆盖 61/56；这里只是让语义自洽，未来加别的表不会错位）
            for h in prog_hits:
                prog_by_ch[h["ch"]] = h["new"]
            if prog_hits or prog_seen:
                log(f"    [prog] {program_override_summary(prog_hits, prog_seen)}")
        # ★ 被 **CC7=0** 静音的通道：本批 5 首的 ch15 就是这种（见 drum_bank_patch 的注释）。
        #   它们**听不见**，但它们的 velocity 会参与下面的"全局等比缩"⇒ 静音层能悄悄把整首曲子
        #   压低（实测 th18_18：补鼓后全局缩 0.825→0.751，全曲 RMS 掉 1.5 dB，而听感上什么都没变）。
        #   ⇒ 静音通道的音符**不参与** `top`，只做越界保护（≤127，反正发不出声）。
        cc7 = {}
        for evs in tracks:
            for tick, k, ch, a, b in evs:
                if k == "cc" and a == 7:
                    cc7.setdefault(ch, []).append(b)
        muted_ch = {ch for ch, vals in cc7.items() if vals and max(vals) == 0}
        unmuted = set()
        if unmute_cc7 is not None:                 # ★ 可选：把静音的鼓层抬回来（A/B 试听用）
            for ti, tick, ch, v, at in cc_out:
                if ch in hits and v == 0:
                    raw[at] = max(1, min(127, int(unmute_cc7)))
                    unmuted.add(ch)
            muted_ch -= unmuted
        # ② 每音的线性增益（优先级见 docstring）
        scaled, audible = [], []
        for ti, tick, ch, note, vel, at in vol:
            g = 1.0
            prog = prog_by_ch.get(ch) or 0
            if ch in drum_ch:
                g = drums_gain
                drum_hit += 1
            elif ti in melody_tracks:
                g = mel_gain
            elif prog in prog_gain:
                g = prog_gain[prog]
                pg_hit += 1
            elif prog in range(32, 40):
                g = bass_gain
            elif (ti, ch) in low_inst:
                g = low_gain
                low_hit += 1
            v = _sf2_velocity_gain(vel, g)
            scaled.append((v, vel, at))
            if ch not in muted_ch:
                audible.append(v)
        # ③ 超过 127 就**整体等比缩**（不是逐音 clip）：相对关系不变，绝对电平交给末尾的峰值归一。
        top = max(audible) if audible else 127
        shrink = 1.0
        if top > 127:
            shrink = 127.0 / top
        scaled = [(max(1, min(127, int(round(v * shrink)))), o, at) for v, o, at in scaled]
        chg = sum(1 for v, o, _ in scaled if v != o)
        for v, _, at in scaled:
            raw[at] = v
        mid2 = os.path.join(tmp, "scaled.mid")
        wav = os.path.join(tmp, "out.wav")
        open(mid2, "wb").write(bytes(raw))
        env = dict(os.environ)
        if fs_libdir:
            env["LD_LIBRARY_PATH"] = fs_libdir
        # ★ `synth.midi-bank-select=mma`：默认 `gs` 会**忽略 CC0** ⇒ bank 128 选不上（见 drum_bank_patch）。
        #   MMA 按 14-bit 收（CC0=MSB×128 + CC32）；本批 MIDI 除本次补的以外没有 CC0/CC32 ⇒ 对其余曲零影响。
        cmd = [fs, "-F", wav, "-r", str(RENDER_SR), "-g", "1.0",
               "-o", "synth.midi-bank-select=mma", "-i", sf2, mid2]
        r = subprocess.run(cmd, env=env, capture_output=True, text=True)
        if r.returncode != 0 or not os.path.exists(wav):
            raise SystemExit(f"fluidsynth 失败（rc={r.returncode}）：{' '.join(cmd)}\n"
                             f"{(r.stderr or r.stdout)[-2000:]}")
        pcm, sr = read_wav_pcm(wav)
    finally:
        shutil.rmtree(tmp, ignore_errors=True)
    out = downsample_2x(pcm, sr)
    extra = ""
    if hits:
        extra += f"，鼓组 bank 修补 {drum_bank_names(hits)}"
        if muted_ch:
            extra += (f"，其中 {','.join('ch%d' % c for c in sorted(muted_ch))} 的 CC7=0"
                      f"（源 MIDI 本来就是静音层，不计入全局缩）")
        if unmuted:
            extra += (f"，已把 {','.join('ch%d' % c for c in sorted(unmuted))} 抬回 "
                      f"CC7={unmute_cc7}（试听用，非默认）")
    if drums_gain != 1.0:
        extra += f"，鼓 {tune['drums_db']:+.1f} dB（命中 {drum_hit} 个音）"
    if prog_gain:
        extra += ("，program " + ", ".join(f"{p}:{tune['prog_db'][p]:+.1f}dB"
                                           for p in sorted(prog_gain))
                  + f"（命中 {pg_hit} 个音）")
    if low_gain != 1.0:
        names = ", ".join(f"ch{ch}" for ti, ch in sorted(low_inst))
        extra += (f"，低音区乐器（平均音高 <{low_mean_max:g}：{names or '无'}）"
                  f"{tune['low_db']:+.1f} dB（命中 {low_hit} 个音）")
    log(f"    [sf2] {os.path.basename(sf2)}: velocity 改 {chg}/{len(scaled)} 个音"
        f"（全局缩 {shrink:.3f}{extra}），{len(pcm)/sr:.1f}s@{sr} → {len(out)/SR:.1f}s@{SR}")
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


# ---------------------------------------------------------------- 母带：响度匹配（2026-10-19）
# 口径：**峰值归一 ≠ 响度**。`--peak 0.92` 只钉住"最高的一个采样"，crest factor 不同的两首
# 在同一峰值下积分响度能差 6 LU（本批实测 th18_14 −6.28 LU / th18_05 −6.14 / th18_06 −6.13）。
# 游戏里原曲（44.1 kHz 全带宽）↔ 灵界版（22.05 kHz）切换时耳朵听到的是**响度**，所以要对齐
# **LUFS**（ITU-R BS.1770-4 / EBU R128），且**逐曲**对齐各自原曲（原曲之间本来就不一样，
# 从 −13.0 到 −9.9 LUFS）。
def soft_limit(x, sr, ceiling_db=-0.3, lookahead_ms=5.0, release_db_per_sec=60.0, loop=True):
    """离线**软限幅器**（lookahead 峰值限幅）→ (y, stats)。

    - 需要的衰减 `a[i] = 20log10(|x[i]|/C)`（C = 10^(ceiling_db/20)）；
    - 在 **±lookahead** 窗内取**最大值**（离线 ⇒ 可以同时看未来，**不引入延时**，
      时间轴与原曲/其它 A/B 版本逐采样对齐）；
    - 释放用 dB 域线性回收 `ar[i] = max(m[i], ar[i-1] − step)`，可向量化成
      `ar = max.accumulate(m + step·i) − step·i`（等价，无 Python 循环）；
    - 因为 `ar[i] ≥ m[i] ≥ a[i]`，**保证** `|y[i]| ≤ C`（峰值上限是硬的，不需要再 clip）。
    - `loop=True`（默认）：素材是**循环体** ⇒ 前后各接 1 s 的"卷回"副本再限幅、
      取中段 ⇒ 循环点两侧的增益包络/释放状态连续（否则文件头尾各算各的，
      卷回时会有一次电平跳变）。
    """
    from scipy.ndimage import maximum_filter1d
    n = len(x)
    pad = min(max(1, n // 4), int(1.0 * sr)) if loop else 0
    xp = np.concatenate([x[-pad:], x, x[:pad]]) if pad else x
    C = 10.0 ** (ceiling_db / 20.0)
    p = np.abs(xp).max(axis=1)
    a = np.where(p > C, 20.0 * np.log10(np.maximum(p, 1e-12) / C), 0.0)
    La = max(1, int(round(lookahead_ms * sr / 1000.0)))
    m = maximum_filter1d(a, size=2 * La + 1, mode="nearest")
    step = release_db_per_sec / sr
    idx = np.arange(len(m), dtype=np.float64)
    ar = np.maximum.accumulate(m + step * idx) - step * idx
    y = (xp * (10.0 ** (-ar / 20.0))[:, None]).astype(np.float32)[pad:pad + n]
    ar = ar[pad:pad + n]
    return y, {"max_gr": float(ar.max()), "mean_gr": float(ar.mean()),
               "frac": float((ar > 0.1).mean()), "lookahead_ms": lookahead_ms,
               "release_db_s": release_db_per_sec}


def match_loudness(x, sr, target_lufs, ceiling_db=-0.3, max_iter=6, tol=0.02, **kw):
    """把 `x` 的**积分响度**顶到 `target_lufs`，峰值不超过 `ceiling_db`。→ (y, stats)

    线性增益 + 限幅器；限幅会吃掉一点响度 ⇒ 迭代（`g += target − LUFS(y)`），
    通常 1–4 次收敛到 ±0.02 LU。返回的 stats 里 `gain_db` 是**相对输入**的总增益。
    """
    import measure_loudness as ML                      # 同目录；只在响度模式下才需要 scipy
    g = target_lufs - ML.integrated_lufs(x, sr)
    st = {}
    for it in range(max_iter):
        y, st = soft_limit(x * (10.0 ** (g / 20.0)), sr, ceiling_db, **kw)
        l = ML.integrated_lufs(y, sr)
        err = target_lufs - l
        if abs(err) <= tol:
            break
        g += err
    st.update(gain_db=g, lufs=l, peak_db=20.0 * np.log10(max(float(np.abs(y).max()), 1e-12)),
              iters=it + 1, target=target_lufs)
    return y, st


def resolve_loudness_target(spec, stem, loop, ssr, ml):
    """`--loudness-match` → (kind, value)。kind ∈ {lufs, peak}。

    - `original`      ：逐曲目标 = **原曲循环体**的积分响度（44100 Hz 全带宽，游戏里真正听到的）
    - `lufs:-11.0`    ：统一目标
    - `peak:-0.3`     ：按**采样峰**（dBFS；>0 且 ≤1 视作线性 FS，兼容老的 `--peak 0.92` 口径）
    """
    s = (spec or "").strip()
    if s == "original":
        return "lufs", ml.integrated_lufs(loop, ssr)
    if s.startswith("lufs:"):
        return "lufs", float(s.split(":", 1)[1])
    if s.startswith("peak:"):
        v = float(s.split(":", 1)[1])
        return "peak", (10.0 ** (v / 20.0) if v <= 0 else v)
    raise SystemExit(f"--loudness-match 只认 original / lufs:<值> / peak:<dBFS|0-1>，收到 {spec!r}")


# ---------------------------------------------------------------- 母带 EQ：RBJ 双二阶（因果，不改时间轴）
# 全部用 audio-EQ-cookbook（RBJ）的**双二阶**：一阶高通/架式在 22.05 kHz 上做，系数稳定。
# 素材是**循环体** ⇒ 前后各接 0.25 s 的"卷回"副本再滤、取中段（与 `soft_limit(loop=True)` 同一手法）：
# 循环点两侧滤波器状态连续，不会在文件头尾各起一次瞬态。**因果滤波 ⇒ 不加预响、不引入延时**，
# 输出与输入**逐帧同长**（试点可以原地替换、同一秒 A/B）。
def _sos_apply(x, sos, sr, loop=True):
    """单级双二阶（sos 一行）；循环体边界用卷回副本保温。"""
    from scipy.signal import sosfilt
    n = len(x)
    pad = min(max(1, n // 8), int(0.25 * sr)) if loop else 0
    xp = np.concatenate([x[-pad:], x, x[:pad]]) if pad else x
    y = sosfilt(sos, xp.astype(np.float64), axis=0)[pad:pad + n]
    return y.astype(np.float32)


def _low_shelf_sos(sr, f0, gain_db, S=1.0):
    """RBJ low shelf：DC 处 +gain_db，f0 处 ≈ 一半，2·f0 以上 ≈ 0（S=1）。"""
    A = 10.0 ** (gain_db / 40.0)
    w0 = 2.0 * np.pi * min(f0, sr * 0.49) / sr
    cw, sw = np.cos(w0), np.sin(w0)
    alpha = sw / 2.0 * np.sqrt((A + 1.0 / A) * (1.0 / S - 1.0) + 2.0)
    sA = np.sqrt(A)
    b0 = A * ((A + 1) - (A - 1) * cw + 2 * sA * alpha)
    b1 = 2 * A * ((A - 1) - (A + 1) * cw)
    b2 = A * ((A + 1) - (A - 1) * cw - 2 * sA * alpha)
    a0 = (A + 1) + (A - 1) * cw + 2 * sA * alpha
    a1 = -2 * ((A - 1) + (A + 1) * cw)
    a2 = (A + 1) + (A - 1) * cw - 2 * sA * alpha
    return np.array([[b0 / a0, b1 / a0, b2 / a0, 1.0, a1 / a0, a2 / a0]])


def _high_shelf_sos(sr, f0, gain_db, S=1.0):
    """RBJ high shelf：高频处 +gain_db，f0 处 ≈ 一半，f0/2 以下 ≈ 0（S=1）。"""
    A = 10.0 ** (gain_db / 40.0)
    w0 = 2.0 * np.pi * min(f0, sr * 0.49) / sr
    cw, sw = np.cos(w0), np.sin(w0)
    alpha = sw / 2.0 * np.sqrt((A + 1.0 / A) * (1.0 / S - 1.0) + 2.0)
    sA = np.sqrt(A)
    b0 = A * ((A + 1) + (A - 1) * cw + 2 * sA * alpha)
    b1 = -2 * A * ((A - 1) + (A + 1) * cw)
    b2 = A * ((A + 1) + (A - 1) * cw - 2 * sA * alpha)
    a0 = (A + 1) - (A - 1) * cw + 2 * sA * alpha
    a1 = 2 * ((A - 1) - (A + 1) * cw)
    a2 = (A + 1) - (A - 1) * cw - 2 * sA * alpha
    return np.array([[b0 / a0, b1 / a0, b2 / a0, 1.0, a1 / a0, a2 / a0]])


def _highpass_sos(sr, f0, Q=0.7071067811865476):
    """RBJ 二阶高通（巴特沃斯 Q=1/√2）：20 Hz 用来去直流/次声，不碰音乐频段。"""
    w0 = 2.0 * np.pi * min(f0, sr * 0.49) / sr
    cw, sw = np.cos(w0), np.sin(w0)
    alpha = sw / (2.0 * Q)
    b0, b1, b2 = (1.0 + cw) / 2.0, -(1.0 + cw), (1.0 + cw) / 2.0
    a0, a1, a2 = 1.0 + alpha, -2.0 * cw, 1.0 - alpha
    return np.array([[b0 / a0, b1 / a0, b2 / a0, 1.0, a1 / a0, a2 / a0]])


def apply_eq(x, sr, eq, loop=True):
    """按表套母带 EQ → float32（**同帧**）。顺序 高通 → 低架 → 高架。

    线性/因果滤波器，级联顺序对稳态传递函数没有影响；这样排只是让直流先被拿掉，
    架式滤波器不必去处理 DC 偏置。
    """
    if not eq:
        return x
    y = x
    if float(eq.get("highpass_hz") or 0.0) > 0:
        y = _sos_apply(y, _highpass_sos(sr, float(eq["highpass_hz"])), sr, loop)
    if float(eq.get("low_shelf_db") or 0.0) != 0.0:
        y = _sos_apply(y, _low_shelf_sos(sr, float(eq.get("low_shelf_hz") or 100.0),
                                         float(eq["low_shelf_db"])), sr, loop)
    if float(eq.get("high_shelf_db") or 0.0) != 0.0:
        y = _sos_apply(y, _high_shelf_sos(sr, float(eq.get("high_shelf_hz") or 4000.0),
                                          float(eq["high_shelf_db"])), sr, loop)
    return y


def resolve_eq(stem, a):
    """`--eq` / `--eq-json` / `--low-shelf` / `--high-shelf` / `--highpass` → EQ 字典。

    优先级：`--eq master` 的表（EQ_GLOBAL → PER_TRACK_EQ）→ `--eq-json`（按内部曲名）→ CLI。
    **默认（`--eq off`）返回 `{}`** ⇒ 一个采样都不动。
    """
    eq = {}
    if a.eq == "master":
        eq.update(EQ_GLOBAL)
        eq.update(PER_TRACK_EQ.get(stem) or {})
    if a.eq_json:
        try:
            with open(a.eq_json, encoding="utf-8") as f:
                cfg = json.load(f)
        except (OSError, json.JSONDecodeError) as e:
            raise SystemExit(f"--eq-json 读不了：{e}")
        cfg = cfg.get(stem, cfg) if isinstance(cfg, dict) else {}
        for k, v in cfg.items():
            if k not in EQ_KEYS:
                raise SystemExit(f"--eq-json 的键只认 {EQ_KEYS}，收到 {k!r}")
            eq[k] = float(v)
    for spec, key, hzkey, hzdef in (
            (a.low_shelf, "low_shelf_db", "low_shelf_hz", 100.0),
            (a.high_shelf, "high_shelf_db", "high_shelf_hz", 4000.0)):
        if spec is not None:
            db, hz = _parse_db_hz(spec, key)
            eq[key] = db
            if hz is not None:
                eq[hzkey] = hz
            elif hzkey not in eq:
                eq[hzkey] = hzdef
    if a.highpass is not None:
        eq["highpass_hz"] = float(a.highpass)
    # 全零 = 等价于关；顺手清掉，避免打印 "EQ 0 dB"
    return {k: v for k, v in eq.items() if float(v or 0.0) != 0.0}


def _parse_db_hz(spec, name):
    """`+4` / `-4@100` / `-3@4000` → (dB, hz|None)。"""
    s = str(spec).strip()
    db_s, sep, hz_s = s.partition("@")
    try:
        db = float(db_s)
        hz = float(hz_s) if sep else None
    except ValueError:
        raise SystemExit(f"--{name.replace('_db', '').replace('_', '-')} 要写成 DB 或 DB@HZ"
                         f"（例 -4@100），收到：{spec!r}")
    if hz is not None and hz <= 0:
        raise SystemExit(f"频率必须 > 0，收到：{spec!r}")
    return db, hz


def eq_knobs(eq):
    """一行 EQ 摘要（印在逐曲报表/JSON 里，便于事后对账）。"""
    if not eq:
        return "EQ off"
    parts = []
    if float(eq.get("highpass_hz") or 0.0) > 0:
        parts.append(f"HP {float(eq['highpass_hz']):g} Hz")
    if float(eq.get("low_shelf_db") or 0.0) != 0.0:
        parts.append(f"低架 {float(eq['low_shelf_db']):+.1f} dB @{float(eq.get('low_shelf_hz') or 100):g} Hz")
    if float(eq.get("high_shelf_db") or 0.0) != 0.0:
        parts.append(f"高架 {float(eq['high_shelf_db']):+.1f} dB @{float(eq.get('high_shelf_hz') or 4000):g} Hz")
    return " / ".join(parts)


def _parse_prog_spec(spec):
    """`56=-4.4` / `56-58=-3` / `40-47=-3` → {56: -4.4} / {56,57,58: -3}。"""
    k, sep, v = spec.partition("=")
    if not sep:
        raise SystemExit(f"--prog-gain 要写成 PROG[-HI]=dB（例 56=-4.4 / 56-63=-3），收到：{spec!r}")
    try:
        db = float(v)
        if "-" in k.strip()[1:]:
            lo, _, hi = k.strip().partition("-")
            lo, hi = int(lo), int(hi)
            if hi < lo:
                raise ValueError
            return {p: db for p in range(lo, hi + 1)}
        return {int(k): db}
    except ValueError:
        raise SystemExit(f"--prog-gain 的 PROG/range 不合法：{spec!r}")


def resolve_tune(stem, a):
    """→ (tune, mel_gain, bass_gain)。顺序：全局 GLOBAL → 逐曲 PER_TRACK → `--track-config` → CLI。

    优先级后者覆盖前者；`prog_db` 做**并集**（CLI 可以只加一个 program 而不清掉表里的）。
    """
    tune = {"drums_db": GLOBAL["drums_db"], "melody_db": DB_MELODY, "bass_db": DB_BASS,
            "prog_db": {}, "low_db": 0.0, "low_mean_max": 60}
    tune.update({k: (dict(v) if isinstance(v, dict) else v)
                 for k, v in (PER_TRACK.get(stem) or {}).items()})
    tune["prog_db"] = {int(k): float(v) for k, v in (tune.get("prog_db") or {}).items()}
    # --track-config：JSON，按**内部曲名**键（也可直接给一份不带曲名的对象）
    if a.track_config:
        try:
            cfg = json.loads(a.track_config)
        except json.JSONDecodeError as e:
            raise SystemExit(f"--track-config 不是合法 JSON：{e}")
        cfg = cfg.get(stem, cfg) if isinstance(cfg, dict) else {}
        for k, v in cfg.items():
            if k == "prog_db":
                tune["prog_db"].update({int(p): float(db) for p, db in v.items()})
            else:
                tune[k] = v
    # CLI（最高优先级）
    if a.prog_gain:
        for spec in a.prog_gain:
            tune["prog_db"].update(_parse_prog_spec(spec))
    if a.drums_gain is not None:
        tune["drums_db"] = a.drums_gain
    if a.low_db is not None:
        tune["low_db"] = a.low_db
    if a.low_mean_max is not None:
        tune["low_mean_max"] = a.low_mean_max
    if a.melody_db is not None:
        tune["melody_db"] = a.melody_db
    if a.bass_db is not None:
        tune["bass_db"] = a.bass_db
    if a.melody_gain is not None:                    # 旧线性口径（兼容）
        tune["melody_db"] = 20 * math.log10(a.melody_gain)
    if a.bass_gain is not None:
        tune["bass_db"] = 20 * math.log10(a.bass_gain)
    return tune, _db2lin(tune["melody_db"]), _db2lin(tune["bass_db"])


def tune_knobs(tune):
    """→ 一行旋钮摘要（印在逐曲报表里，便于事后对账"这首用了什么参数"）。"""
    s = f"drums {tune['drums_db']:+.1f}dB / mel {tune['melody_db']:+.1f}dB / bass {tune['bass_db']:+.1f}dB"
    if tune.get("prog_db"):
        s += " / prog " + ",".join(f"{p}:{db:+.1f}" for p, db in sorted(tune["prog_db"].items()))
    if float(tune.get("low_db") or 0.0) != 0.0:
        s += f" / low<{float(tune.get('low_mean_max') or 60):g}:{float(tune['low_db']):+.1f}dB"
    return s


def main(argv=None):
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    ap.add_argument("--stem", action="append", default=None,
                    help="曲目 stem（可多次，例 th18_03）；默认 = MANIFEST 里全部")
    ap.add_argument("--melody-gain", type=float, default=None,
                    help="主旋律**线性**增益（旧口径，默认 0.35 ≈ −9.1 dB；等价于 --melody-db -9.1）")
    ap.add_argument("--bass-gain", type=float, default=None,
                    help="倍斯（GM program 32–39）**线性**增益（旧口径，默认 1.6 ≈ +4.1 dB）")
    ap.add_argument("--melody-db", type=float, default=None, help="主旋律增益（dB）")
    ap.add_argument("--bass-db", type=float, default=None, help="倍斯增益（dB）")
    ap.add_argument("--drums-gain", type=float, default=None, metavar="DB",
                    help="鼓（ch9 / bank 128 鼓组）增益，dB（全局默认 +2.5）")
    ap.add_argument("--prog-gain", action="append", default=None, metavar="PROG[-HI]=DB",
                    help="按 GM program（或 program 区间）调增益，dB，可重复"
                         "（例 --prog-gain 56=-4.4 --prog-gain 56-63=-3）")
    ap.add_argument("--low-db", type=float, default=None,
                    help="低音区乐器增益（dB）：按**乐器**（轨+通道）平均音高 < --low-mean-max 判")
    ap.add_argument("--low-mean-max", type=float, default=None,
                    help="低音区乐器的平均音高上限（默认 60 = C4）")
    ap.add_argument("--track-config", default=None, metavar="JSON",
                    help='逐曲覆盖（JSON，按内部曲名键）：\'{"th18_07": {"prog_db": {"25": -1.5}}}\'')
    ap.add_argument("--out", default=OUT_DIR)
    ap.add_argument("--peak", type=float, default=0.92, help="峰值归一目标（老的线性口径；响度模式下不用）")
    ap.add_argument("--loudness-match", default=None, metavar="original|lufs:<值>|peak:<值>",
                    help="★ 母带按**响度**匹配（替代 --peak 峰值归一）："
                         "`original` = 逐曲匹配**原曲循环体**的积分响度（LUFS，BS.1770-4）；"
                         "`lufs:-11.0` = 统一目标；`peak:-0.3` = 按采样峰（dBFS）。"
                         "需要 scipy。超峰值上限时走 lookahead 软限幅（不引入延时）。")
    ap.add_argument("--peak-ceiling", type=float, default=-0.3, metavar="DBFS",
                    help="响度匹配后的**硬**峰值上限（dBFS，默认 -0.3；只有响度模式用）")
    ap.add_argument("--limit-release", type=float, default=60.0, metavar="DB/S",
                    help="限幅器释放速率（dB/秒，默认 60 = 6 dB/100 ms；越小越平滑、越吃瞬态）")
    ap.add_argument("--loudness-report", default=None, metavar="JSON",
                    help="把每首的响度匹配结果（目标/增益/限幅量/最终 LUFS+peak）写进 JSON")
    ap.add_argument("--eq", choices=("off", "master"), default="off",
                    help="★ 母带 EQ（默认 **off** = 17 首既有产物逐采样不变）："
                         "`master` = 套 EQ_GLOBAL（全曲低架 +4 dB @100 Hz）+ PER_TRACK_EQ"
                         "（th18_12 低架 −4 / th18_13 高架 −3 @4 kHz + 20 Hz 高通）；"
                         "在响度匹配**之前**、限幅器之前处理，不改帧数/时间轴")
    ap.add_argument("--eq-json", default=None, metavar="JSON",
                    help='逐曲 EQ 覆盖（JSON，按内部曲名键，可只给一首）：'
                         '\'{"th18_13": {"high_shelf_db": -5}}\'；键 = '
                         'highpass_hz/low_shelf_db/low_shelf_hz/high_shelf_db/high_shelf_hz')
    ap.add_argument("--low-shelf", default=None, metavar="DB[@HZ]",
                    help="临时低架（例 `--low-shelf=-4@100`；只给 dB 用 100 Hz）——负值要带 `=`，试 A/B 用")
    ap.add_argument("--high-shelf", default=None, metavar="DB[@HZ]",
                    help="临时高架（例 `--high-shelf=-3@4000`；只给 dB 用 4 kHz）——负值要带 `=`")
    ap.add_argument("--highpass", type=float, default=None, metavar="HZ",
                    help="临时高通（例 `20`）")
    ap.add_argument("--dry-run", action="store_true", help="只解析 + 报表，不渲染")
    ap.add_argument("--no-align", action="store_true",
                    help="不做细对齐（默认**做**：小节线 ±1 小节三条锚点各搜 ±0.8 s，取 chroma 相关最高者）")
    ap.add_argument("--no-drum-bank", action="store_true",
                    help="**不**给非 ch9 的鼓通道补 bank 128（关掉 ch15 真鼓修复；仅供 A/B 对照）")
    ap.add_argument("--no-program-override", action="store_true",
                    help="**不**改 program（鼓 kit 25 / ZUN 号 61,58→56）——A/B 对照或出改版前峰值用")
    ap.add_argument("--master-peaks", default=None, metavar="JSON",
                    help="母带增益锚定：JSON {内部曲名: 改版前 pre-norm 峰值}（A/B 口径 —— 保证换 kit / "
                         "换号以外**其它乐器绝对电平不变**，也让 07b 与 kit25 A/B 逐字节相同）")
    ap.add_argument("--peak-report", default=None, metavar="JSON",
                    help="把每首**归一化前**的峰值写进这个 JSON（给下一次 --master-peaks 用）")
    ap.add_argument("--starts", default=None, metavar="JSON",
                    help="起点锚定：JSON {内部曲名: 起点秒} —— 换音色/换 kit 时沿用改版前起点，"
                         "不跑 fine_align（A/B 口径；07b 与 kit25 A/B 逐字节相同就靠这条）")
    ap.add_argument("--start-report", default=None, metavar="JSON",
                    help="把每首实际采用的起点（秒）写进这个 JSON（给下一次 --starts 用）")
    ap.add_argument("--drum-bank-unmute", type=int, default=None, metavar="CC7",
                    help="给补过鼓 bank 的通道把源 MIDI 的 `CC7=0` 抬到 CC7（0–127）"
                         "——本批 5 首的 ch15 是整层静音的，这条只为试听；**默认不动**")
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
    # 母带锚定（A/B 口径）：给 `--master-peaks` 的 JSON 里有的曲目用**固定增益**
    # = a.peak / 改版前 pre-norm 峰值 ⇒ 换 kit / 换号之外其它乐器电平不动；
    # 没给的曲目照旧逐文件峰值归一。`--peak-report` 把这次归一化前的峰值存下来供下次用。
    master_peaks = {}
    if a.master_peaks:
        with open(a.master_peaks, encoding="utf-8") as f:
            master_peaks = json.load(f)
        print(f"母带锚定：{len(master_peaks)} 首用 --master-peaks 的改版前峰值（其余照旧归一）")
        if a.loudness_match:
            print("  ★ --master-peaks 与 --loudness-match 同时给：**响度模式优先**，"
                  "固定增益不参与（响度模式自己算增益）")
    # 起点锚定（A/B 口径）：`--starts` 里有的曲目直接用改版前的起点（不再跑 fine_align）
    forced_starts = {}
    if a.starts:
        with open(a.starts, encoding="utf-8") as f:
            forced_starts = json.load(f)
        print(f"起点锚定：{len(forced_starts)} 首用 --starts 的改版前起点（换音色不动时间轴）")
    peak_report = {}
    start_report = {}
    loud_report = {}
    env_cache = {}
    print(f"{'曲':<9}{'轨':>4}{'音符':>7}{'主旋律轨':>9}{'MIDI 时长':>10}{'循环体':>9}{'起点':>9}{'r':>7}  文件")
    wrote = 0
    for stem in stems:
        # stem = **内部曲名**（th18_03…）；MIDI 文件按**音乐室编号**命名（th18_02…）
        mi = midi_index_for(stem)
        if mi is None:
            print(f"  ★ 跳过 {stem}：dat/musiccmt.txt 里没有这个内部曲名（不是本作曲目）")
            continue
        tune, mel_gain, bass_gain = resolve_tune(stem, a)
        eq = resolve_eq(stem, a)                     # 先解析：坏 JSON 在渲染前就停
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
                                  mel_gain, bass_gain, tune=tune,
                                  drum_bank=not a.no_drum_bank,
                                  unmute_cc7=a.drum_bank_unmute,
                                  program_override_on=not a.no_program_override)
        else:
            rendered = render_naive(notes, prog, midi_secs + 0.5, mel_ch, mel_gain,
                                    bass_gain, seed=hash(stem) & 0xFFFF)
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
        # ★ `--starts`（A/B 口径）：换音色 / 换 kit **不动时间轴** —— 直接用改版前那次的起点，
        #   不再跑 fine_align。这样同一首的多个音色版本逐采样对齐，A/B 可以直接切换比对。
        if stem in forced_starts:
            start_sec = float(forced_starts[stem])
            r_corr = float("nan")
            print(f"    [start] {stem}: 锚定改版前起点 {start_sec:+.6f}s（换音色不动时间轴）")
        elif a.no_align:
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
        start_report[stem] = start_sec
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
        peak_report[stem] = peak
        # ★ 母带 EQ（2026-10-20 owner 第三轮）：先 EQ、后响度匹配（限幅器最后）。
        #   默认 `--eq off` ⇒ eq 为空 ⇒ 一个采样都不动（17 首既有产物向后兼容）。
        #   放在 peak_report 之后：`--peak-report` / `--master-peaks` 的口径仍然是
        #   **EQ 前的渲染峰值**（A/B 锚定语义不变）。
        if eq:
            seg = apply_eq(seg, SR, eq)
            print(f"    [eq] {stem}: {eq_knobs(eq)}（响度匹配前；同帧）")
        if a.loudness_match:
            # ★ 响度匹配（2026-10-19）：目标逐曲 = 原曲循环体的 LUFS；峰值上限 -0.3 dBFS；
            #   超上限走 lookahead 软限幅。`--master-peaks` 的固定增益在这里**不参与**
            #   （响度模式自己算增益），两者同时给会提示。
            import measure_loudness as ML
            kind, val = resolve_loudness_target(a.loudness_match, stem, loop, ssr, ML)
            if kind == "peak":
                seg *= (val / peak)
                loud_report[stem] = {"mode": "peak", "target": val,
                                     "peak_out": peak * (val / peak), "gain_db": 20 * np.log10(val / peak)}
                print(f"    [loud] {stem}: 峰值目标 {val:.6f}（{20*np.log10(val):.2f} dBFS）"
                      f" → 增益 {20*np.log10(val/peak):+.2f} dB")
            else:
                seg, st = match_loudness(seg, SR, val, a.peak_ceiling,
                                         release_db_per_sec=a.limit_release)
                st.update(mode=("original" if a.loudness_match == "original" else "lufs"),
                          pre_norm_peak=peak, eq=(dict(eq) if eq else None))
                loud_report[stem] = st
                tag = "原曲" if a.loudness_match == "original" else "目标"
                lim = (f"限幅 −{st['max_gr']:.2f} dB（{st['frac']*100:.1f}% 采样）"
                       if st["max_gr"] > 0.01 else "未触发限幅")
                print(f"    [loud] {stem}: {tag} {st['target']:.2f} LUFS → 增益 {st['gain_db']:+.2f} dB"
                      f"；{lim}；结果 {st['lufs']:.2f} LUFS / peak {st['peak_db']:.2f} dBFS"
                      f"（{st['iters']} 次迭代）")
        elif stem in master_peaks:
            gain = a.peak / float(master_peaks[stem])
            seg *= gain
            print(f"    [master] {stem}: 改版前 pre-norm peak {float(master_peaks[stem]):.6f} "
                  f"→ 固定增益 {gain:.6f}（新峰值 {peak * gain:.6f}）")
        else:
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
              f"   [{a.render} | {tune_knobs(tune)} | BPM "
              f"{midi_bpm(tempos, tick_at_intro):.1f} 主旋律能量占比 "
              f"{share(mel_ch)*100:.0f}%→"
              f"{share(mel_ch)*100*mel_gain**2/(1-share(mel_ch)+share(mel_ch)*mel_gain**2):.0f}%"
              f"{'' if not eq else ' | ' + eq_knobs(eq)}]")
    if a.peak_report and peak_report:
        with open(a.peak_report, "w", encoding="utf-8") as f:
            json.dump(peak_report, f, ensure_ascii=False, indent=1, sort_keys=True)
        print(f"pre-norm 峰值 → {a.peak_report}（{len(peak_report)} 首；下次给 --master-peaks 用）")
    if a.start_report and start_report:
        with open(a.start_report, "w", encoding="utf-8") as f:
            json.dump(start_report, f, ensure_ascii=False, indent=1, sort_keys=True)
        print(f"起点 → {a.start_report}（{len(start_report)} 首；下次给 --starts 用）")
    if a.loudness_report and loud_report:
        with open(a.loudness_report, "w", encoding="utf-8") as f:
            json.dump(loud_report, f, ensure_ascii=False, indent=1, sort_keys=True)
        print(f"响度匹配结果 → {a.loudness_report}（{len(loud_report)} 首）")
    print(f"\n写出 {wrote} 个文件 → {os.path.relpath(a.out, REPO)}"
          f"（切模式：mode.txt 写 `rearr`；同步：bash local/sync_modkit_dll.sh）")
    return 0 if wrote or a.dry_run else 2


if __name__ == "__main__":
    sys.exit(main())
