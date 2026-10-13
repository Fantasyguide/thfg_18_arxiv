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
