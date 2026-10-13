# DOC-CONTRADICTIONS —— 注释与文档矛盾台账（P13 / W1）

> 工单：**P13**（见 [`WORK-PACKAGES.md`](WORK-PACKAGES.md)）；检查器：[`native/tools/doc_claims.py`](native/tools/doc_claims.py) ✓
> 纪律（R1）：**本台账只记录，不改代码**；每条都要有「一手依据」与「处置」；处置需 owner 裁决的标注清楚 ✓。
> 状态：**首轮**（2026-10-14）。对照物 = 当前已提交状态。

## 0. 开工清单（worktree 起手式）—— 本轮实测补全 ✓

在 worktree 里跑 `make check` 要绿，必须补齐下列**不在版本库里的东西**（否则基线就红 ✗）：
1. `ln -s <主树>/local <worktree>/local` ✓ —— ⚠ 陷阱：若先跑了 build，worktree 里会**生成真的 `local/` 目录** ✗
   ⇒ 符号链接会变成 `local/local` ✓；必须 `rm -rf <worktree>/local` 后重建链接 ✓。
2. **被 gitignore 的生成资产**（`assets/ability` 76 · `patch/th18` 16 · `assets/voice` 10 · `native/voice_ids.h` 等，共 109 个 / 4.5MB）✓
   ⇒ `git ls-files --others --ignored --exclude-standard mods/th18.v1.00a/card-expand` 后 `cp -n` ✓。
3. **其他会话尚未提交、但被 tracked 文档引用的文件**（当前：`engine/faith/th12,15,16,17/*.md` ✓、
   `engine/card/th09/04-player-shot-sequences.md` ✓）⇒ 只拷进 worktree、**不进 PR** ✓。
4. `make anm`（6 个产物）✓ + `make sht`（4 个产物）✓ —— 否则 `anm-verify` / `sht-verify` 直接红 ✗。
5. `make check` ⇒ **rc=0** ✓（451 个文档）。

## 1. 首轮发现

| # | 位置 | 冲突内容 | 一手依据 | 归属 | 处置 |
| :-: | --- | --- | --- | --- | --- |
| C1 | `engine/faith/OVERVIEW.md` + `engine/_shared/rosters/th1X.md` | 链到 `engine/faith/th12/15/16/17/01-point-item-value.md`，而这四个文件**在已提交状态里不存在** ✗ ⇒ `check-docs` 断链 20 处 | `git ls-files engine/faith/` = 9 个文件；主树里这四个目录是 `??`（**另一会话在写、尚未提交**）| 非系统卡（引擎文档）| **不改** ✓：等那位提交后自然消解；本轮只在 worktree 里补齐以取得绿色基线 ✓ |
| C2 | `HANDOFF-109-C1-C4.md` | 链到 `engine/card/th09/04-player-shot-sequences.md`（主树里是**已暂存未提交** ✗）| 同上 | 109 | 同上 ✓（109 排除 ✗）|
| C3 | W1a（"注释里写的常量值 ≠ 实际定义"）| **首轮 0 处** ✓ —— 收紧到"明确的赋值式声明"后，全仓 4656 个常量定义**没有**注释声明与定义打架 ✓ | `python3 native/tools/doc_claims.py report --only W1a` ✓ | — | 无需处置 ✓；**保留为检查** ✓ |
| C4 | W1b（偏移 ↔ 命名金矿）| **暂不启用** ✗：首轮 155 处里绝大多数是假阳性（`zItem` 是道具结构体、`CE_ITEM_POOL_OFF` 却是**管理器**偏移；`zPlayerBullet` 在金矿里几乎为空）| `report --only W1b` 输出 ✓ | — | **后续子项**：先建「常量 → 它真正索引的结构」映射表，再启用 ✓ |
| C5 | W1d（跨文档数字不变量）| 首轮 0 处 ✓（HUD 右沿/锚点、sht 格数两处登记、容量文档均已一致 ✓）| `report --only W1d` ✓ | — | 保留为检查 ✓（后续把「state 表」「−192 口径」也纳入 ✓）|

## 2. 检查器现状（可复现）

```
python3 native/tools/doc_claims.py report            # 默认集合 W1a/W1c/W1d（只打印）
python3 native/tools/doc_claims.py report --only W1b # 未收敛的那条，手动跑
python3 native/tools/doc_claims.py check             # 有发现则 rc=1（**尚未**挂进 make check ✓）
```
⚠ 按 P13 纪律：**误报未收敛前不转闸** ✗ ⇒ 目前 `make check` 里没有它 ✓；等 owner 看过首轮结论再决定是否 `check` ✓。

## 1b. P16（额外最大得点数值核查）带来的新条目（2026-10-14）

| # | 位置 | 冲突内容 | 一手依据 | 归属 | 处置 |
| :-: | --- | --- | --- | --- | --- |
| C6 | `engine/faith/th15/01-point-item-value.md` §4.4（`:336`/`:344`/`:350-351`/`:557`/`:560`）| 旧文仍写「miss 撒 **type 9** ⇒ 每颗 +2」✗，而权威 §4.6（`:719-736`）+ `:627` 已订正「**miss 不涨 PIV**（撒 **type 1 ×7**）」✓ ⇒ **同一份文档两个口径** | th15 子代理一手穷举（`thecl -d 15 -x` 全解 23 个 `.ecl`）| 引擎文档（非系统卡）| **不改**（他人文档 ✓）；读者**只看 §4.6** ✓ |
| C7 | `engine/faith/OVERVIEW.md:163` | 写「★ **读值时夹**（`min(池, cap_i)`）」✗，实现却是**写侧闸**（`sdk_core.c:849` 按 `cap_i − init_i` 累计封顶 ✓，`ce_piv_reading()` 不夹 ✓）；`:170`/`:180` 措辞又不同 ⇒ 读闸 vs 写闸是**两种机制** ✗ | 代码 + th15 子代理 | 引擎文档 / 共享件 | 待 owner 定**权威口径** ✓ |
| C8 | `native/sdk_core.c:861` 引的 4 份出处 | `engine/faith/{th12,th15,th16,th17}/` 全是 **untracked** ✗（只有 th13/th14 已入库 ✓）⇒ **干净 clone 按注释找不到** ✓ | `git status --porcelain`（th15/th16/th17 三份子代理各自独立确认 ✓）| 另一会话在途 | 等那几份提交后自然消解 ✓；在此之前任何引用都应标「未入库」 ✗ |
| C9 | `native/sdk_core.c:967` | 注释写「一手（**th12–th17 四作同形**）：每 +1 擦弹 ⇒ 最大得点 +1」✗；一手只有 **th12–th15** ✓（th16/th17 **两作独立证实擦弹不写 PIV** ✓；声明擦弹族的只有 112/113/114/115 ✓）| th16/th17 子代理 + `OVERVIEW.md:216/233` | 共享件（SDK）| **P14a** 批次改注释 + 按 R2 登记 `AUDIT.md` §BE ✓（本文只记录，不改 ✗ R4）|

> 数值本身**全部一致**：六作（th12–th17）的 `init`/`cap` 与代码表**逐项 0 差** ✓ —— 详见 [`MAXPOINT-AUDIT.md`](MAXPOINT-AUDIT.md) ✓。

### 0.2 开工清单的**第 4 个陷阱**（2026-10-14 实测，P20 批次踩到）

- ⚠ **在 `local/` 下做 `git checkout` / `git restore`（例如 `git checkout -- local/README.md`）会把符号链接变成实体目录** ✗
  ⇒ `sites.py check` 立刻报「**找不到样本**：<worktree>/local/th18.v1.00a/th18.exe」✗（`make release` 的发布闸 ① 变红 ✗）。
  **修法**：`rm -rf local && ln -s <主树>/local local` ✓（`rm -rf` 不会跟随符号链接 ✓）。
- **期望状态**：`local` 是**符号链接** ✓ + `git status` 里长期有一条 `D local/README.md` —— 因为 `local/README.md` 是 `local/` 下**唯一被跟踪**的例外 ✓；
  它是**假删除** ✗（文件通过链接可见 ✓，只是 git 不跟随目录符号链接 ✓）⇒ **永不 stage 它** ✓（`git add` 只点名自己的文件 ✓）。
- 另注：主树的 `native/th18_card_expand.dll` **会被别的会话重建** ✗ ⇒ `make release` 之后若 `local/sync_modkit_dll.sh` 读到的 md5 与发布闸记录的不同 ✗，
  说明**别人刚在主树构建过** ✓ ⇒ 必须从**自己 worktree 的构建产物**显式 `cp` 到各处并逐一 `md5sum` 核对 ✓（本轮即如此：闸记 `3e23c44a…`，主树已变成 `ae55d165…` ✗）。

## 1c. 流程失误留痕

| **C10b** | **同型失误第三次，但这次是"闸的盲区"**（2026-10-14，P18 批次）：`CARD-BEHAVIOR-AUDIT.md` 里有 **131 处裸地址**（`0x……` 未加反引号）✗，合并后共享线的 `[addr]` 闸变红 ✗ | **根因：`check-docs` 跳过未跟踪文件** ✗ —— 新写的文档在 `git add` **之前**跑 `check-docs` 永远是绿的 ✓，`git add` 之后才会被查 ✗ | 已在 P24 批次定点修好 ✓（131 → 0 ✓）。**规矩（写死）**：**新增文件必须先 `git add` 再跑 `check-docs`** ✓；大段由 agent 生成的文本（含 `thNN:0x…`）**必须过一遍"裸地址加反引号"规范化** ✓（本批的定点修法可作为工具：按检查器输出的 `文件:行号 + 地址` 修补 ✓）|
（2026-10-14，P16 的版本声明漏提交）

| # | 现象 | 根因 | 处置 |
| :-: | --- | --- | --- |
| **C10a** | **同型失误第二次**（2026-10-14，P20 批次）：§BE 那一行我先 `git add`（长版）、**之后**才把它拆成「短行 + 小节」（闸由红转绿 ✓），但第二次提交只写了 `git add RELEASE-STATE.json` ✗ ⇒ **拆分留在工作区、没进提交** ✗ ⇒ 合并进共享线的仍是超长那版 ⇒ **红闸再犯** ✗ | 根因：**先暂存、后编辑、提交时没重新暂存** ✗ | 已在 P21 批次重做拆分 ✓。**规矩（写死）**：编辑 → `check-docs` → `git add` → `git diff --cached` 核对 → commit ✓；**`git add` 永远放在最后一次编辑之后** ✓ |
| C10 | **共享线 `check-docs` 曾为红** ✗：`MAXPOINT-AUDIT.md` 引了 13 个地址但**没有版本声明**（`[version]` 闸）| P16 批次里我在 worktree 补了这行、跑绿 ✓，但**没 commit** ✗ 就直奔开 PR ⇒ PR 里的版本仍是旧的 ✗（`0ccee42f` 里只有正文那 2 处「版本」字样）| 已在 **P20/P21 批次**补回并重拆 ✓；**教训**：*闸绿的是「工作区」，提交的是「索引」* —— 每次 `check-docs` 绿之后必须**再看一眼 `git status` 有没有未提交的修改** ✓ |
