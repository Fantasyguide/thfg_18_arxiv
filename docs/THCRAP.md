# THCRAP 可玩包 —— 开发机准备（新机器 checklist）

> **版本**：跨版本。本文只讲构建机上怎么备齐灵界版 BGM 要用的外部素材，不含游戏地址。
> **适用**：`mods/th18.v1.00a/card-expand/tools/make_trance_rearr.py`。
> ★ ★ **这些素材都在 `local/`（gitignored），换机器要重新准备** —— clone 下来只有工具，
> 没有 sf2 / MIDI / thbgm.dat；发布包里带的是渲染好的 wav，也不带这些源料。

## 开发机准备

新机器上 `make_trance_rearr.py` 会卡在三样东西上：**fluidsynth、sf2、MIDI**（外加 `thbgm.dat`）。
逐条备齐即可；缺哪样，工具的报错里都会直接说怎么补。

### 1. ★ fluidsynth：无 root 装法（2026-10-19 实测可行）

系统能装就走系统（`sudo apt-get install -y fluidsynth`）。没有 root 时，把发行版 .deb
解到 `local/vendor/` —— **完全不动系统、删目录即卸载**：

```bash
REPO=$(git rev-parse --show-toplevel)          # 在仓库根目录执行
mkdir -p "$REPO/local/vendor/fluidsynth-2.2.5" && cd /tmp
apt-get download fluidsynth libfluidsynth3 libsndfile1 libinstpatch-1.0-2 \
  libglib2.0-0 libjack-jackd2-0 libsdl2-2.0-0 libpulse0 libflac8 \
  libvorbis0a libvorbisenc2 libopus0 libogg0 libdecor-0-0 libasyncns0
for d in *.deb; do dpkg-deb -x "$d" "$REPO/local/vendor/fluidsynth-2.2.5/"; done
```

- 一共 1 个主包 + **14 个依赖**（本机 Ubuntu 22.04，`fluidsynth 2.2.5`）。
- ★ **工具会自动找到它**：`find_fluidsynth()` 的顺序是
  `--fluidsynth` → `$RL_FLUIDSYNTH` → `PATH` → `local/vendor/fluidsynth-*/usr/bin/fluidsynth`；
  命中 vendor 那份时，把 `usr/lib/x86_64-linux-gnu[:/pulseaudio]` 加进**子进程**的
  `LD_LIBRARY_PATH`（不动当前 shell）。
- ★ 换机器也可以**整个 `local/vendor/fluidsynth-2.2.5/` 目录拷过去**（同发行版/同架构），不用重下。
- 实在拿不到：`--render=naive` 退回机器内置的老合成器（能出全套素材，音色差一档，可作 A-B 对照）。

### 2. ★ sf2 位置

`local/th18.v1.00a/sf2/thfont.sf2` = **THfont**（SD-90 采样系，owner 提供；
261,192,820 B / 249.1 MiB；sha256 `c05372f76164aac916f9913550fab9d30d650cf205f000da27513ab98054b419`；
186 预设，bank 128 prog 127 = `SD90`；**许可未声明 ⇒ 只本机自用、发布包不带 sf2**）。
来源/许可的正文见 `mods/th18.v1.00a/card-expand/NOTICE.md` 的「音色库」一节。

- `find_sf2()` 顺序：`--sf2` → `$RL_SF2` → 该目录下的 `*.sf2`；
  ★ **多份时优先挑文件名带 `thfont` 的**，否则按文件名排序取第一份。
- 目标音色是 **Roland SD-90**（ZUN 原曲用的硬件）；THfont 就是 SD-90 采样系 ——
  2026-10-19 owner 3 首试点后拍板**定为正式音源**，全量 17 首已换。
- **备选**：同目录的 `GeneralUser-GS.sf2`（GeneralUser GS v2.0.3，32,319,396 B，sha256
  `9575028c7a1f589f5770fccc8cff2734566af40cd26ed836944e9a5152688cfe`，许可明确）——
  听感差一档，仍留着作 A/B 对照；删掉 `thfont.sf2` 会自动回落它（或 `--sf2` 显式指定）。
- 换库后**必须**重跑试点重新确认听感。

### 3. ★ MIDI 位置

`local/th18.v1.00a/midi/` —— **17 个 `th18_NN.mid` + `MANIFEST.tsv`**：

- 来源：`https://github.com/AyHa1810/touhou-midi-collection`（commit `5a11a0a`，
  路径 `1 - Shooter Games/18 - Unconnected Marketeers/`）；
- 17 首**全部是 Karl Zuñiga 编曲**（同曲多版本时选他那版 —— 唯一覆盖全集、织体最接近原曲；
  Usage Policy：可修改、**必须署名编曲者**、ZUN 本家 MIDI 不得用于 fangame）；
- `MANIFEST.tsv` 记了逐曲 md5 / 曲目号 / 编曲者 / 内部曲名 / musiccmt 标题 / 源文件；
  编号以 `dat/musiccmt.txt` 一手表为准（内部曲名有洞，音乐室编号连续，两者不对齐）。

- ★ 还要 `local/th18.v1.00a/thbgm.dat`（430,093,548 B ≈ 430 MB）才能跑 `fine_align` ——
  它要拿**原曲循环体**的 chroma 序列做相关对齐；`--no-align` 可跳过，但循环点会对不上。

## ★ 一句话

**这些素材都在 `local/`（gitignored），换机器要重新准备。** 仓库里只有工具与登记，
没有 sf2 / MIDI / thbgm.dat；发布包带的是渲染好的 wav。
