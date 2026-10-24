# thfg_18_arxiv —— TH18「幻想指南」mod 的**只读冻结快照**

> **状态：冻结（frozen）** —— 本仓库是快照归档，不再接受功能改动。任何后续开发在
> `Nipporita/renkolab` 的对应分支上进行（去屎/deshit 相关改动**不在**本快照里）。

## 快照来源

| 项 | 值 |
| --- | --- |
| 上游 | `Nipporita/renkolab` |
| 提交 | `aa8d61f8`（冻结版：92/111/HUD/商店/禁用手柄 全部修复 + Renko 平衡改动）|
| 性质 | 非 deshit 的开发态 / 发版基线（SDK 仍是单文件 `sdk.c` + `sdk.h`，未拆模块）|
| 口径 | 与上游该提交**内容一致**，仅追加「构建必须件」（见下）|

## 与上游的差异（**只多了构建必须件**）

1. `mods/th18.v1.00a/card-expand/assets/voice/*.wav`
   上游 `.gitignore` 只收了其中 8 个；这里补齐 **16 个**（`build_voice.py` 会按 `assets/voice/ORDER.txt` 逐个要）
   —— 否则 `git clone` 后 `make dll` 直接停在 `FAIL: 缺文件：CHARGE00.wav`。
2. `assets/bgm/*.wav` + `mode.txt`（**36 首**）
   发版包运行时放在 `<modkit>/thcrap/bin/bgm/`；这里按"归档"位置存放，部署时复制过去即可。
3. `local/vendor/thtk/build/{thanm,thdat,thmsg}/`（含 `COPYING*`）
   `make anm / sht / ecl` 需要 thtk 的 `thanm/thdat`。上游要求本机自己 `bash tooling/thtk/build.sh`；
   这里直接放**预编译件**（BSD 许可，见同目录 `COPYING`），让归档副本不需要额外工具链。
   它们的查找路径就是 `local/vendor/thtk/build/<tool>/<tool>`（见 `tooling/thtk/unpack.py` 的 `find_tools()`）。

其余文件与上游该提交**逐字节相同**（`git diff --stat` 只应看到以上三类新增）。

## 构建

```bash
# 1) 只编 DLL（不需要游戏本体）
cd mods/th18.v1.00a/card-expand/native
make dll                 # → th18_card_expand.dll；需要 i686-w64-mingw32-gcc
make dll CE_DEV_DUMP=0   # 发布档：所有 ce_log 编译为空，不写任何日志

# 2) 提交前完整只读门（主机单测 + 资源 + 站点 + 文档 + DLL ABI）
#    ⚠ 里面 `anm-verify` / `sht-verify` 要校验 `build/ability.anm`、`build/sht/*.sht`：
#       这些是**由游戏原生贴图切出来再打包**的构建产物，本仓不收录（版权）
#       ⇒ 裸克隆时请先放好你自己的游戏资源、跑一次 `make anm sht`，再 `make check`。
#       只想验证代码能不能编，用上面的 `make dll` 即可（不需要任何游戏文件）。
make check

# 3) 资源再生成（anm / sht / ecl）
make anm sht             # 需要本仓 local/vendor/thtk/build 下的 thanm/thdat（已附）
```

### 需要你自己放的东西（版权件，**不随本仓分发**）

- `local/th18.v1.00a/th18.exe`、`th18.dat`、`thbgm.dat`（运行游戏与 `make dist` 需要）
- `assets/ability/**.png` 里由**游戏原生贴图**切出来的那批（`make_*_art.py` 从 `local/` 现切；
  本仓不收录游戏美术字节）
- Ghidra 工程、`local/vendor/th-re-data` 等逆向环境（见上游 `docs/SETUP.md`）

## 音频的版权说明（重要）

- `assets/bgm/*.wav`：**改自《东方天空璋》原曲的同人编曲**，用社区的 **thfont** 音源渲染。
  上游作者认为在社区惯例下可以随 mod 分发，故一并归档；**原始作曲版权属于 ZUN / 上海アリス幻樂団**，
  编曲/音源版权属于各自作者。**如原作者要求，可随时删除对应文件**。
- `mods/th18.v1.00a/card-expand/assets/voice/*.wav`：本项目自己合成/录制的语音与音效。
- `th18/th18.exe`、`.dat`、`.anm` 派生的贴图：**不含**在本仓。

## 目录提示

```
mods/th18.v1.00a/card-expand/    mod 本体：patch/（thcrap patch）+ native/（DLL 源码）+ assets/（资源源）
engine/ games/ tooling/ docs/    逆向工作台（与 mod 共用同一套引擎知识）
assets/bgm/                      ★ 本快照新增：运行时要装到 thcrap/bin/bgm/ 的编曲
```

## 许可

代码与文档按上游仓库的许可；`tooling/thtk` 的预编译件按其自身 `COPYING`（BSD）；音频见上。

## 变更记录

- **2026-10-24 首次冻结**：`8b3d2beb`（当日修复 + Renko 平衡）。
- **2026-10-24 重冻结（当前）**：`aa8d61f8` ——
  92 伟大之作（清扫代数修正 / 威光脚本号 400→394 / 冷却 6·180s / 换面不清零 / 召唤物掉金钱卡）、
  111 全屏吸收接自建池（季节道具·108 刻符·109 灵力道具，排除 117/112）、
  HUD 压暗统一接 SDK（`ce_hud_dim_alpha` / `ce_hud_dim_apply(_a)`）并修 108 槽条洗白 / 117 末帧 / 112 闪光 / 116 季节字与 releaseable、
  天顶剑音效每 4 把一次、商店招财猫常驻（随机位恒 6）、**禁用手柄** binhack 并入卡包、118 十二合一关回。
