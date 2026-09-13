# ANM 卡图管线 —— 设计（A 阶段）

> **版本**：TH18 v1.00a（`th18.exe`，imagebase `0x400000`）。本文裸地址默认属该版本；引用其他版本须写成 `th16:0x…`。
> 日期：2026-09-04。状态：待用户审阅。

## 0. 一句话

给 card-expand 的第 8 段（卡图）配上工具：写卡的人放两张 PNG，`make` 出一个追加了新 sprite 的
`th18/abcard.anm`，随 `_255` patch 整文件替换分发；同时把「解 dat / 解 anm / 重建 anm」固化成
版本无关的 `tooling/thtk/`，协作者 clone 后能复现今天的 `local/th18.v1.00a/anm/` 目录。

**不在本次范围**（B / C 阶段，另开 spec）：卡牌场上特效（`ability.anm` 68 个脚本 + SDK
`ce_spawn_effect`）、引擎侧 ANM 装载器 / VM opcode 一手逆向（`engine/anm/th18/`）。

## 1. 已确认的事实（2026-09-04，thanm release 12 一手解包）

| 事实 | 依据 |
| --- | --- |
| `abcard.anm` 118 个 entry，**一 entry 一 sprite，sprite 号 = entry 号**，0..117 | `thanm -l 18` 输出 |
| entry 0/1 = 卡框 `abframe` / 道具 `abitem`；2..117 = 卡图 `<NAME>_max` / `<NAME>_min` 成对 | 同上 |
| `_max`：贴图 256×512（THTX 256×320），sprite 256×320；`_min`：贴图 64×128（THTX 64×128），sprite 64×80；format 1（BGRA8888） | 同上 |
| 116/117 = `empty_max` / `dummy`，与引擎表行 56 的 sprite 对一致；57 用 2/3 = `BACK_max` / `dummy` | `engine/card/th18/11-sentinels-56-57.md` + 本次解包互证 |
| `thanm -l` → `thanm -c` 重建**字节一致** | `cmp` 通过 |
| `ability.anm` = 7 贴图 / 68 脚本（场上特效）；`abmenu.anm` = 3 贴图 / 19 脚本（编成 / 图鉴 UI） | `thanm -l 18` |
| th18 的 anmmap 用 thpages `v8.anmm`（`any.anmm` 明确 18 → v8；含 th18 新加 439 `fadeNearCamera` / 614 `drawArc`） | `local/vendor/thpages/static/mapfile/` |
| 格式层面 sprite 数无上限（header 里是计数）；**运行时**是否有上限未查 → 🟡，留给 C 阶段 | — |

## 2. 分层与落点

| 东西 | 落点 | 寿命 |
| --- | --- | --- |
| thtk 工具链：编译配方、解包脚本、环境探测、doctor 项 | `tooling/thtk/` + `tooling/env.sh` + `tooling/doctor.py` + `docs/SETUP.md` | 版本无关 |
| 卡图源文件 + 重建脚本 + 顺序清单 | `mods/th18.v1.00a/card-expand/assets/` | 死绑 th18 + 本 mod |
| 解出的 dat / anm、重建产物 | `local/th18.v1.00a/{dat,anm}/`、`native/build/`、`dist/` | 不入库（版权字节） |
| 三个卡牌 ANM 装什么、余量结论 | `engine/card/th18/10-extensibility-limits.md` §5、`DATA.md` | 引擎 / mod 文档 |

## 3. `tooling/thtk/`

```
tooling/thtk/
  README.md     做什么、怎么装、怎么用、坑
  build.sh      编 thanm + thdat 到 local/vendor/thtk/build/
  unpack.py     <版本>：dat → local/<版本>/dat/ → local/<版本>/anm/<名>/
```

- **`build.sh`**：`git submodule update --init --depth 1`（libpng / zlib-ng / thtypes）→ 若 PATH 里没有
  bison / flex / m4，用 `apt-get download bison flex m4 libfl2 libfl-dev` + `dpkg -x` 解到
  `local/vendor/bisonflex/`（免 sudo；bison 需要 `BISON_PKGDATADIR` 指向搬家后的 `share/bison`）→
  cmake（`-DBUILD_SHARED_LIBS=OFF`，`FL_LIBRARY` 指向解出的 `libfl.a`）→ `make thanm thdat`。
  非 apt 系统给提示、不硬做。幂等。
- **`env.sh`** 新增探测：`THANM` / `THDAT`（PATH 优先，其次 `local/vendor/thtk/build/`），
  `THTK_ANMMAP_DIR`（`local/vendor/thpages/static/mapfile`，退而 `local/vendor/truth/map`）。
- **`doctor.py`** 新增一组：thanm / thdat 可执行、anmmap 目录、样本 `local/<版本>/<版本>.dat`。
- **`unpack.py <版本>`**：`thdat -x <N> local/<版本>/thXX.dat` → `dat/`；对每个 `.anm`：
  `anm/<名>/<名>.anm.txt`（`thanm -l -m <该作 anmmap>`）+ `thanm -x`（贴图按 entry 路径落地）。
  版本号 → thtk 版本参数 / anmmap 文件的映射表写在脚本里（16 → v8，18 → v8，…）。
  结束打印一张「名 / entries / scripts」表。幂等，可重跑。
- **`docs/SETUP.md`**：加一节「thtk」；`local/README.md`：登记 `dat/` `anm/` 布局、`vendor/truth`（新克隆，anmmap 备份源）；
  `engine/_shared/community-sources.md`：补 truth 一行。

## 4. `card-expand/assets/`

```
assets/
  README.md            写卡的人看：放什么、命名、跑什么、索引怎么来
  cards/
    ORDER.txt          一行一个 NAME，只追加不重排 —— 行号决定 sprite 号
    SPADE_10_max.png   256×320
    SPADE_10_min.png   64×80
    ...
  build_abcard.py      原 spec + 追加 entry → thanm -c → native/build/abcard.anm；校验；打印索引表
  gen_placeholder.py   用 Noto Serif CJK 画占位卡图（花色 + 点数），给还没画图的卡用
```

- **索引规则**：`ORDER.txt` 第 k 行（从 0 数）的卡 → `sprite_large = 118 + 2k`，`sprite_small = 119 + 2k`。
  写卡的人把这对填进 `patch/th18/cards.js`。`build_abcard.py` 读 `cards.js` **校验**：每张卡的
  sprite 对要么是原生的（≤ 117），要么正好等于 ORDER 推出的值；不一致就报错退出，不悄悄改 JSON。
- **entry 模板**：复制 `BLANK_max` / `BLANK_min` 两个 entry 的字段（format / width / height / THTX*），
  只改 `name`（`ability/<NAME>_max.png`）和贴图路径；`_min` 源图 64×80 由脚本垫成 64×128 透明底
  （THTX 高度 128），`_max` 256×320 直接用。尺寸不对直接报错。
- **产物**：`native/build/abcard.anm`（gitignore 加 `mods/**/native/build/`）。**不进 `patch/`**（20 MB 版权字节，`patch/` 是入库目录）。
- **接进构建**：
  - `Makefile` 加 `anm`（调 `build_abcard.py`）、`anm-verify`（见 §6）；`step3` / `dist` 依赖 `anm`。
  - `dist`：把 `build/abcard.anm` 拷到 `dist/patch-step3/th18/abcard.anm`，然后**对 dist 目录重算 `files.js`**。
    `mkfiles.py` 改成接受目录参数，且把 `.anm` 纳入 crc 清单（现在只收 `.js`）。
  - `release.py` 的 `MAP` 加 `patch-step3/th18/abcard.anm → …/th18_card_expand_255/th18/abcard.anm`。
  - 只进 `_255`：`_test` 叠在 `_255` 上，卡池 JSON 也在 `_255`，abcard 跟卡池走。DLL 零改动。

## 5. 文档回填

- `DATA.md`：§「卡图」把「索引余量 ⏳ 未查」换成 §1 的事实；加「`assets/` 工作流」一段；原生 sprite 表
  加一列 entry 名（`SPADE_10 → 118/119` 这类新卡行由 build 脚本打印，手抄进去）。
- `engine/card/th18/10-extensibility-limits.md` §5 与 `11-sentinels-56-57.md` 的遗留问题：
  三个 ANM 各装什么、sprite = entry、118 起可追加；运行时上限 🟡 待 C 阶段。
- `MAP.md` 第 8 段：「手工」→「`assets/` 工具」；`NEXT.md` 更新；`CARDS.md` 黑桃五张加图标列。
- `engine/anm/OVERVIEW.md`：加一行「thtk 工具链就位、th18 已全解包」的导航（不写引擎结论）。

## 6. 验证

**主机侧（`make anm-verify`，进 `make check`）**：
1. 重建后 `thanm -l` 能列出、entry 数 = 118 + 2 × |ORDER|；
2. 从重建文件 `thanm -x` 出的 **117 张原图与原始解包逐张 `cmp` 一致**（原 entry 未被动过）；
3. 新 entry 的 spec 字段 == BLANK 模板（除 name）；
4. `cards.js` 里每个 sprite 索引 < entry 数，且 ORDER 一致性通过。

**实跑（Windows，用户）**：`_255` + `_test` 起手黑桃五张 → 卡组编成 / 商店 / HUD 三处看到各自的（占位）卡图，
不再是 `empty_max` 的空白。日志无 `FAIL:`（DLL 未改，`100/100 sites verified` 照旧）。

## 7. 风险与取舍

- **thanm 是唯一编译器**（用户决定：生态兼容优先，不用 truanm）。spec 的助记符依赖 anmmap，编译和列出必须用同一份。
- 发布 anm 内含 ZUN 原贴图：用户已表态直接发（memory `anm-copyright-stance`）；本仓库仍不留版权字节。
- 运行时 sprite 上限未验：若引擎对 `abcard_anm` 的 sprite 数有硬上限，实跑会以崩溃或黑图暴露；
  先只加 5 对（到 127），C 阶段在 `AnmManager__preload_anm` / `AnmLoaded` 结构里坐实。
