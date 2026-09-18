# assets/ — 卡图（`abcard.anm` 追加 sprite）

> **版本**：TH18 v1.00a（`th18.exe`，imagebase `0x400000`）。本文裸地址默认属该版本；引用其他版本须写成 `th16:0x…`。

面向**写卡的人**：一张新卡要有自己的图，只需要放两张 PNG、在 `ORDER.txt` 加一行、把算出来的
两个数填进 JSON。DLL 不碰 ANM；构建脚本把图追加进原生 `abcard.anm` 重编，随 `_255` patch
整文件替换分发（thcrap 的 `th18/abcard.anm`）。

## 放什么

| 文件 | 尺寸 | 说明 |
| --- | --- | --- |
| `cards/<NAME>_max.png` | **256×320** RGBA | 大图：编成 / 商店 / 图鉴用 |
| `cards/<NAME>_min.png` | **64×80** RGBA | 小图：HUD 卡组图标 |
| `cards/ORDER.txt` | 一行一个 `NAME` | **只追加**。行号决定 sprite 号 |
| `cards/_art/<NAME>.png` | 256×320 RGBA | 无框画面（`fit_card.py` 自动留档）：`_min` 与重出框的源 |

`NAME` 建议用卡的 `internal_name`（如 `SPADE_10`）。尺寸不对构建直接报错。
两种出图方式：
- **现成整图**：`fit_card.py NAME src.png`，去白边、上下微修、等比缩放居中到 256×320（`--bg` 可给透明）。黑桃五张（英式牌面 SVG 渲染后）和反转牌（UNO 图）都是它。
- **简洁占位**：`gen_placeholder.py`，白底黑图，`--suit ♠ --rank 10` 画花色 + 点数，`--image x.png` 居中放一张图标——还没找到图时先顶上。

第三方 / 原始素材放 `cards/_src/`（原件 + 出处 README）。

**边框**：原生 117 张卡共用同一个框（外 3 px 黑 → 13 px 深色斜面、边中高光 → 内 2 px 黑，画面区 220×284）。
`cardframe.py` 按量出来的参数程序合成它（仓库里不放原生像素），`fit_card.py` 默认套上（`--no-frame` 关）；
**原生的 `_min`（HUD 图标）不带框**，所以无框画面留在 `cards/_art/NAME.png`，`_min` 从它缩出；`python3 cardframe.py` 给 `_max` 补框、从 `_art` 重出 `_min`（PNG 里记 `renkolab-frame=1`，重复跑不会套两层）。
画面按等比覆盖 220×284 居中裁（宽裁 ~1.4%），不拉伸。`ability.anm` 里的卡图副本引用同一批 PNG，一起带框。

### 系统卡（106–118）卡面：白底 + 各作 exe 图标（❓自定）

106–118 这批系统卡在原版里**没有卡面**（都复用原生空白 sprite 116/117 = `ability/empty_max.png` + `ability/dummy.png`），
所以**没有一手配方可复刻**；画面本身是 ❓ 自定（用户 2026-09-18 口径：「卡图就用这个吧」= 白底 + 图标最大居中，
**框仍走原生同款** `cardframe.py`，这一半是复刻）。三个工具串起来：

| 步骤 | 工具 | 说明 |
| --- | --- | --- |
| ① 导出待改的现有卡面 | `tools/export_cardart.py` | 全卡面（含非系统卡）导到一个可翻的目录 + `index.html`，用来对风格 |
| ② 抠各作 exe 图标 | `tools/extract_exe_icons.py` | PE `RT_GROUP_ICON`/`RT_ICON` → `.ico` → 最大帧 PNG（32×32）；`--exe tag=路径` 指样本，样本放 `local/<tag>.v*/` |
| ③ 拼卡面草图 | `tools/make_system_cardart.py` | `--icons <②的输出> --out <目标>`，出 `_art`（无框）+ `_max`（套 `cardframe.apply_frame`）+ `_min`（无框 64×80）+ `index.html` |

- 106–117 = **一作一图**：`--scale nearest`（默认）整数倍最近邻 ⇒ 图标 **192×192**（32×6），画面区内居中，**不糊**；
  `--scale fill` 会铺到 220×220 但有插值。
- **118 十二合一 = 3×4 拼图**（用户 2026-09-18：「十二合一就拼一个 3*4 的，然后最大居中」）：
  12 个图标按**卡号 106→117**（= TH06→TH17，正好 12 作）逐行左→右、上→下摆成 3 列 4 行，格子正方形、整墙居中。
  格子边长 `c = min((220−2·PAD)/3, (284−2·PAD)/4)`（PAD=6 ⇒ 上限 68），`nearest` 再取整到 32 的整数倍：
  - 默认 `nearest`：`c=64` ⇒ 墙 **192×256**，四边留白各 14 px（逐像素校验过 = 各作图标 NEAREST ×2 合成到白底）；
  - 备选 `--scale fill`：`c=68` ⇒ 墙 204×272（留白 L/R 8、T/B 6），更大但有插值（导出的 `118_alt_fill_*` 就是这个）。
  小卡面（64×80）同样拼 3×4，`c = min(64/3, 80/4) = 20` ⇒ 墙 60×80（缩小用 LANCZOS，最近邻缩小会烂成一团）。
- ⚠ 样本路径只出现在命令行，不进仓库；`local/` 里没有对应 exe 就抽不出图标（脚本会画占位卡并写明原因）。
- **已落地（2026-09-18）**：13 张都进了 `cards/`（`ORDER.txt` 末尾追加 13 行 ⇒ sprite **164/165 … 188/189**），
  `_max` 256×320 带框、`_min` 64×80 无框、`_art/NAME.png` 256×320 无框。
  `_art` 用的是 `cardframe.py` 的**「画面区原尺寸」口径**（PNG 记 `renkolab-art=area`）：加框 **1:1 贴**、`_min` 按**内容外框**缩 ——
  因为这批画面是**整数倍最近邻**放大的 exe 图标，走老的「等比覆盖裁切」会被缩成 0.8875 倍、像素必糊（见 `cardframe.py` 头注）。
  ★ **用户 2026-09-18 口径：这就是最终版**（「118 的 12 合一足够恶搞了，这个基本可认为是最终最终版了」）
  ⇒ 卡面这一项收工，不再改（`--scale fill` 那版只当作留档的备选，不出图）。

## 索引怎么来

原生 `abcard.anm` 有 118 个 entry（0..117），**一个 entry 一个 sprite，sprite 号 = entry 号**，
新卡从 118 起两两追加：

```
ORDER 第 k 行（0 起）  →  sprite_large = 118 + 2k,  sprite_small = 119 + 2k
```

`make anm` 会打印每张卡的那一对，照抄进 `patch/th18/cards.js` 的 `sprite_large` / `sprite_small`。
构建脚本**校验** JSON 和 ORDER 一致（不一致报错退出，不会悄悄改 JSON）；原生索引（≤ 117）照旧放行。

## 跑什么

```bash
cd ../native
make anm          # → native/build/abcard.anm，打印索引表；JSON 不一致会报错
make anm-verify   # 重建文件自检：entry 数、原 117 张贴图逐张一致、新 entry 字段 == BLANK 模板
make dist         # 把 build/abcard.anm 放进 dist/patch-step3/th18/，files.js 收它的 crc
```

前置：`bash tooling/thtk/build.sh` + `python3 tooling/thtk/unpack.py th18.v1.00a`
（脚本从 `local/th18.v1.00a/anm/abcard/` 取原生 spec 与贴图）。

## 边界

- 一次重建 = modkit 历史多 20 MB。**只在真的加了新图时**才重建 / 发布，日常改 JSON、DLL 不碰它。
- 运行时 sprite 数上限**未验**（格式层无上限）；目前加到 147（15 张），C 阶段在 `AnmManager__preload_anm` 坐实。
- `ecl/`：开发辅助（独立 patch `th18_card_expand_devstage`，启动器里单独勾）。`st01`–`st06.ecl.txt` 是入库的空壳关卡源（logo → 对话 → boss）；`make_dev_ecl.py` 编它们并现场把原生 boss 血量 ÷100、死时多掉 300 金。
- `ability.anm`（场上特效）的追加在 [`ability/`](ability/README.md)：卡图副本 entry + 特效脚本，`make anm` 一起重建。`abmenu.anm` 不管。
- `sht/`：**装备卡子机的弹幕（连射）**。`append_shooterset.py` 往四个原生 `pl0X.sht` 的偏移数组空位里追加 shooterset
  （`make sht` → `native/build/sht/*.sht` + `native/sht_ids.h`），随 `_255` 整文件替换。**当前 `APPEND` 为空 = 不产出**
  （破损核心第一版用过，改成定点伤害源了）。
  格式与不变式见 [`engine/sht/th18/`](../../../../engine/sht/th18/README.md)，用法见 [`SDK.md`](../SDK.md) §12。
