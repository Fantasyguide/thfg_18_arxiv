# ANM 卡图管线 —— 实施计划（A 阶段）

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

> **版本**：TH18 v1.00a（`th18.exe`，imagebase `0x400000`）。本文裸地址默认属该版本；引用其他版本须写成 `th16:0x…`。

**Goal:** 写卡的人放两张 PNG + 一行 ORDER，`make dist` 产出追加了新 sprite 的 `th18/abcard.anm` 随 `_255` 分发；thtk 工具链固化进 `tooling/thtk/`。

**Architecture:** 版本无关的 thtk 编译 / 解包脚本放 `tooling/thtk/`；卡图源与重建脚本放 `mods/th18.v1.00a/card-expand/assets/`；重建产物只落 gitignored 的 `native/build/` 与 `dist/`。DLL 零改动。

**Tech Stack:** thtk release 12（标准 thanm / thdat，本地编译）、python3 + PIL、thpages `v8.anmm`、GNU make。

**Spec:** `docs/superpowers/specs/2026-09-04-anm-card-art-pipeline-design.md`

## Global Constraints

- 本仓库不留版权字节：解包贴图、重建 `.anm` 只在 `local/`、`native/build/`、`dist/`（后两者 gitignore）。
- 只用标准 thanm（不用 truanm）；列出与编译用同一份 anmmap（`local/vendor/thpages/static/mapfile/v8.anmm`）。
- 索引规则：`ORDER.txt` 第 k 行（0 起）→ `sprite_large = 118 + 2k`，`sprite_small = 119 + 2k`；ORDER 只追加不重排。
- 源图尺寸：`_max` 256×320、`_min` 64×80（脚本把 `_min` 垫到 64×128 透明底）；entry 字段照抄 `BLANK_max` / `BLANK_min`。
- 不写死机器路径：脚本一律从仓库根相对定位；外部工具靠 PATH / `local/vendor/` 探测。
- 文档改完跑 `python3 tooling/check-docs.py`。

---

### Task 1: `tooling/thtk/build.sh` —— 一键编 thanm / thdat

**Files:**
- Create: `tooling/thtk/build.sh`, `tooling/thtk/README.md`
- Modify: `local/README.md`（vendor 列表加 `truth/`、`bisonflex/`；th18 目录加 `th18.dat` / `dat/` / `anm/`）、`docs/SETUP.md`（加「thtk」一节）、`engine/_shared/community-sources.md`（加 truth 一行：anmmap 备份源，Apache-2.0）

**Produces:** `local/vendor/thtk/build/thanm/thanm`、`.../thdat/thdat`。

- [ ] 写 `build.sh`：`set -eu`；`ROOT` 由脚本位置推；若 `local/vendor/thtk` 不存在则 `git clone https://github.com/thpatch/thtk`；`git submodule update --init --depth 1`；若 `command -v bison flex m4` 有缺 → `apt-get download bison flex m4 libfl2 libfl-dev` 到 `local/vendor/bisonflex/debs/`，`dpkg -x` 到 `local/vendor/bisonflex/`，导出 `PATH`、`BISON_PKGDATADIR=$BF/usr/share/bison`、`M4=$BF/usr/bin/m4`；非 apt 系统打印「请自行安装 bison flex m4」退出 2；cmake `-DCMAKE_BUILD_TYPE=Release -DBUILD_SHARED_LIBS=OFF -DFL_LIBRARY=$BF/usr/lib/x86_64-linux-gnu/libfl.a -DFL_INCLUDE_DIR=$BF/usr/include`（仅当用了 bisonflex）；`make -j thanm thdat`；末尾打印 `thanm -V`。
- [ ] 验证：先 `rm -rf local/vendor/thtk/build`，跑 `bash tooling/thtk/build.sh`，末行出 `Touhou Toolkit release 12`；再跑一次确认幂等（秒回）。
- [ ] 写 README（做什么 / 怎么装 / 坑：Ghidra 无关；bison 搬家要 `BISON_PKGDATADIR`；`thanm -c` 与 `-l` 必须同一份 anmmap；release 页只有 Windows exe）。
- [ ] 改 `local/README.md`、`docs/SETUP.md`、`community-sources.md`；`python3 tooling/check-docs.py` 过。
- [ ] Commit：`tooling(thtk): build.sh 一键编标准 thanm/thdat（bison/flex 免 sudo 走 apt-get download）`

### Task 2: `tooling/thtk/unpack.py` + env / doctor 探测

**Files:**
- Create: `tooling/thtk/unpack.py`
- Modify: `tooling/env.sh`（末尾加 thtk 探测块）、`tooling/doctor.py`（加 `check_thtk()`，接进 `main`）

**Produces:** `local/<版本>/dat/*`、`local/<版本>/anm/<名>/<名>.anm.txt` + 贴图；`env.sh` 导出 `THANM` `THDAT` `THTK_ANMMAP_DIR`；
可 import 的 `find_tools() -> tuple[Path thanm, Path thdat, Path anmmap_dir]`（Task 4 复用，找不到抛 `SystemExit` 带 fix 提示）。

- [ ] `unpack.py <版本>`（如 `th18.v1.00a`）：表 `GAMES = {"th16": ("16", "v8.anmm"), "th18": ("18", "v8.anmm")}`（键取版本目录前缀）；找工具：PATH → `local/vendor/thtk/build/`；找 anmmap：`local/vendor/thpages/static/mapfile/` → `local/vendor/truth/map/`；`thdat -x N thXX.dat` 到 `dat/`（已存在且非空则跳过，`--force` 重解）；每个 `.anm`：`mkdir anm/<名>`，在其中 `thanm -l N ../../dat/<名>.anm -m <map> > <名>.anm.txt`，`thanm -x N ../../dat/<名>.anm`；`.err` 非空即报错；末尾打印 `名 / entries / scripts` 表（`^entry` / `^script` 计数）。
- [ ] `env.sh`：加一段 bash 探测（不进 `_driver.py`，thtk 与 Ghidra 无关）：`THANM`/`THDAT` = `command -v` 或 `local/vendor/thtk/build/...`；`THTK_ANMMAP_DIR` 同上顺序；存在才 export 并回显。
- [ ] `doctor.py` `check_thtk()`：thanm / thdat 可执行（fix 提示 `bash tooling/thtk/build.sh`）、anmmap 目录（fix 提示 clone thpages）、每个 `local/th*/` 下是否有 `.dat`（soft）。
- [ ] 验证：`mv local/th18.v1.00a/anm local/th18.v1.00a/anm.bak && python3 tooling/thtk/unpack.py th18.v1.00a`，然后 `diff -r anm anm.bak` 为空 → `rm -rf anm.bak`；`source tooling/env.sh` 回显三个变量；`python3 tooling/doctor.py` 新项全绿。
- [ ] Commit：`tooling(thtk): unpack.py 解 dat → 每个 anm 一目录（spec + 贴图）；env.sh/doctor 探测 thanm`

### Task 3: `assets/` 骨架 + 占位卡图

**Files:**
- Create: `mods/th18.v1.00a/card-expand/assets/README.md`、`assets/cards/ORDER.txt`、`assets/gen_placeholder.py`、`assets/cards/SPADE_{10,J,Q,K,A}_{max,min}.png`
- Delete: `assets/.gitkeep`（若有）

**Produces:** 5 对 PNG（256×320 / 64×80，RGBA），ORDER 五行 `SPADE_10 SPADE_J SPADE_Q SPADE_K SPADE_A`。

- [ ] `gen_placeholder.py NAME "♠" "10" [--out assets/cards]`：PIL 画 256×320 圆角深色底 + 中央大花色 + 左上/右下点数，字体 `NotoSerifCJK-Bold.ttc`（`fc-match` 找不到就退回 PIL 默认字体）；`_min` 用同一构图直接缩到 64×80。批量：无参数时按内置表画黑桃五张。
- [ ] 跑它，`file assets/cards/*.png` 确认尺寸与 RGBA。
- [ ] README：命名规则、尺寸、ORDER 追加规则与索引公式、`make anm` / `make anm-verify`、「JSON 里填 build 脚本打印的那对数」。
- [ ] Commit：`feat(card-expand): assets/ 卡图源目录 + 黑桃五张占位图 + ORDER`

### Task 4: `build_abcard.py` + `make anm` / `make anm-verify`

**Files:**
- Create: `assets/build_abcard.py`、`native/tests/test_build_abcard.py`
- Modify: `native/Makefile`（`anm`、`anm-verify` 目标；`check` 依赖 `anm-verify`）、根 `.gitignore`（加 `mods/**/native/build/`）

**Interfaces:**
- `build_abcard.py [--verify-only]`：读 `local/th18.v1.00a/anm/abcard/abcard.anm.txt`、`assets/cards/ORDER.txt`、`patch/th18/cards.js`；写 `native/build/abcard.anm`；stdout 打印 `NAME  large  small` 表；任何校验失败 exit 1。
- 纯函数（供测试）：`parse_entries(spec_text) -> list[dict]`、`make_entry(template: dict, idx: int, name: str, png_rel: str) -> str`、`expected_sprites(order: list[str]) -> dict[str, tuple[int,int]]`、`check_cards_js(cards: dict, expected: dict, n_entries: int) -> list[str]`（返回错误行）。

- [ ] 测试先写（不依赖 `local/`）：`expected_sprites(["A","B"]) == {"A": (118,119), "B": (120,121)}`；`check_cards_js` 对原生索引 ≤117 放行、对 ORDER 卡索引错值报错、对 ≥ n_entries 报错；`make_entry` 以内嵌的 BLANK 模板字符串生成 entry，断言 `name` 与 `sprite<idx>` 正确、其余字段原样。`python3 -m pytest native/tests/test_build_abcard.py -q` 先红。
- [ ] 实现：复制 spec 文本（原 118 entry 与 18 脚本不动），在最后一个 `entry` 之后、第一个 `script` 之前插入新 entry；`_min` 用 PIL 垫到 64×128 写到 `native/build/tex/ability/<NAME>_min.png`，`_max` 校验 256×320 后拷到 `native/build/tex/ability/<NAME>_max.png`；把原 117 张贴图**软链**进 `native/build/tex/ability/`（thanm -c 在 `native/build/tex/` 里跑，spec 路径 `ability/...` 相对 cwd）；`thanm -c 18 ../abcard.anm abcard.spec -m <map>`；工具与 anmmap 沿用 Task 2 的探测逻辑（import `tooling/thtk/unpack.py` 里的 `find_tools()`）。
- [ ] `--verify-only` / 构建后自检：`thanm -l` 重建文件 → entry 数 == 118 + 2·|ORDER|；`thanm -x` 到临时目录后原 117 张与 `local/.../anm/abcard/ability/*.png` 逐张 `filecmp`；新 entry 字段（除 name / sprite 名）== BLANK 模板；`check_cards_js` 零错误。
- [ ] Makefile：`anm: ; python3 ../assets/build_abcard.py`；`anm-verify: ; python3 ../assets/build_abcard.py --verify-only`；`check: anm-verify`（原 `sites.py check` 保留）。`.gitignore` 加 `mods/**/native/build/`。
- [ ] 验证：测试绿；`make anm` 打印五行 `SPADE_10 118 119 …`；此时 `cards.js` 还是 116/117 → **应当报错退出**（证明校验有效）。
- [ ] Commit：`feat(card-expand): build_abcard.py 追加 sprite 重建 abcard.anm + anm-verify（原贴图逐张 cmp）`

### Task 5: 接进 dist / release，JSON 换真索引

**Files:**
- Modify: `patch/th18/cards.js`（58–62 的 sprite 对 → 118/119 … 126/127）、`native/mkfiles.py`（接受目录参数；纳入 `.anm`）、`native/Makefile`（`step3`/`dist` 依赖 `anm`；`dist` 拷 `build/abcard.anm` 到 `$(DIST)/patch-step3/th18/` 后对该目录重算 files.js）、`native/release.py`（`MAP` 加 `patch-step3/th18/abcard.anm → thcrap/repos/Renko_1055/th18_card_expand_255/th18/abcard.anm`）

- [ ] `mkfiles.py`：`refresh(dir)` 收 `.js` 与 `.anm`（仍排除 `files.js`）；无参数保持原行为，有参数只刷给定目录。
- [ ] Makefile 与 release.py 按上表改；`cards.js` 五张换索引（63 强欲之壶、64 反转牌不动）。
- [ ] 验证：`make anm` 通过（校验不再报错）；`make check`；`make dist` 后 `dist/patch-step3/th18/abcard.anm` 存在、`dist/patch-step3/files.js` 含其 crc；`python3 -c` 用 `zlib.crc32` 核对一致；`make dllverify` 照旧（DLL 未变）。
- [ ] Commit：`feat(card-expand): abcard.anm 进 dist/_255 + files.js 收 .anm；黑桃五张换 118–127 索引`

### Task 6: 文档回填

**Files:**
- Modify: `card-expand/DATA.md`（卡图一节：118 entry / sprite=entry / 118 起追加 / `assets/` 工作流；原生 sprite 表加 entry 名列，可由 `awk` 从 spec 生成后贴入）、`engine/card/th18/10-extensibility-limits.md` §5、`engine/card/th18/11-sentinels-56-57.md` 遗留问题段、`card-expand/MAP.md` 第 8 段、`card-expand/NEXT.md`、`card-expand/CARDS.md`（加「卡图」列）、`engine/anm/OVERVIEW.md`（加导航：thtk 工具链 / th18 全解包 / 三个卡牌 ANM 规模；不写引擎结论）、`mods/LESSONS.md`（开头补版本声明，清掉 check-docs 既有违规）

- [ ] 逐份改；引擎侧「运行时 sprite 上限」标 🟡 留 C 阶段。
- [ ] `python3 tooling/check-docs.py` 零违规。
- [ ] Commit：`docs(card-expand/anm): 第 8 段卡图有工具；abcard/ability/abmenu 三 ANM 规模与索引结论回填`

### Task 7: 发布 + 实跑

- [ ] `make release PUSH=1`（modkit 里 `_255/th18/abcard.anm` 新增，约 20 MB）。
- [ ] 用户 Windows `git pull` 实跑：编成 / 商店 / HUD 三处黑桃五张显示占位卡图；日志 `OK … 100/100 sites verified` 照旧、无 `FAIL:`。
- [ ] 通过后：MAP 第 8 段 → ✅，NEXT.md 记「实跑通过」；不通过则优先怀疑运行时 sprite 上限（C 阶段入口 `AnmManager__preload_anm`）。
