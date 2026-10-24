# native —— card-expand 的构建 / 发布入口

> **版本**：TH18 v1.00a。本文不写裸地址；构建纪律以 [`../GATES.md`](../GATES.md) 与
> [`../AUDIT.md`](../AUDIT.md) 为准。

**以后 owner 只跑这一条：**

```bash
cd mods/th18.v1.00a/card-expand/native
make dev MODKIT_DIR="/mnt/d/项目/幻想指南/18/th18_modkit"
```

等价的直接调用（不经过 make，路径由你传，脚本不写死任何机器上的目录）：

```bash
python3 tools/devbuild.py --modkit "/mnt/d/项目/幻想指南/18/th18_modkit"
```

## 它做什么

`tools/devbuild.py` 把「校验 → 构建 → 发布 → 回读 → 汇总」串成**一条链**，
**任何一步失败立即退出**，并打印：

- **哪一步**失败（步骤号 + 名字）；
- **为什么**这一步存在（对应今天翻过的哪次车）；
- **怎么修**（可执行的下一步）。

最后打印一页 PASS/FAIL 表和本次要发的东西（DLL md5 / cards.js md5 / 指南页数）。

### 闸的顺序

| 阶段 | 闸 | 拦的是什么 |
| --- | --- | --- |
| A 源码前置 | `make cards-check` | 109 desc 超 6 行 ⇒ 整批新卡停用（owner 看到「所有 mod 卡消失」） |
| A 源码前置 | 调试/测试卡不可见 | `DEBUG_DEL` / `DEBUG_*` / `TEST_*` 必须 `initial_unlocked=0`、`deck_visible=0`、`weight=6`（6 = 不进随机池；`weight=0` 反而是「必上架」哨兵 ✗）；否则 201 又会进商店/图鉴/指南 |
| A 源码前置 | `sites.py check` | 站点不变式、声明↔def 对账、跳板池预算 |
| A 源码前置 | `gen_def.py --check` | patch 声明了但 DLL 没导出 ⇒ 游戏里静默不生效 |
| A 源码前置 | `gen_drift.py` | 手改生成物 ⇒ 二分/发布拿到假状态 |
| A 源码前置 | `contract_check.py` | 有行为没登记 ⇒ DLL 禁用全部新卡 |
| A 源码前置 | `make test-host` | 纯逻辑回归 |
| A 源码前置 | git 分支 / 冲突 / 未提交源文件 | 不许在 `main` 上构建；不许带 `UU`；源文件先 commit（或 `--allow-dirty`） |
| A 源码前置 | `check-docs.py` | 文档规范 |
| A 源码前置 | 冲突标记扫描 | `mods/` 下文本文件不许有连续 7 个 `<` 或 `>`（今天指南踩过） |
| A 源码前置 | `make_guide.py` + 调试卡扫描 | 指南里出现「调试・定向删卡」或任何 `DEBUG_*`/`TEST_*` ⇒ 停 |
| A 源码前置 | xelatex 两遍 | 指南要真能编译；打印 PDF 页数 |
| B 构建 | `make dll`（除非 `--skip-build`） | 先删旧 DLL 强制重链，避免 sites_gen.h / .def 变了而 DLL 没重编 |
| B 发布 | `deploy.py --skip-build --force` | 唯一被认可的发布路径（内部还会跑来源校验、发布锁、cards-check、资产同步） |
| B 回读 | 部署件 md5 | 部署件 `cards.js` / `files.js` / DLL 三处都必须与源同 md5 |
| B 回读 | 部署件关键项 | **从部署件读**：109 desc = 6 行、201 `init=0 / deck=0 / weight=6`、所有调试卡不可见 |
| B 回读 | 部署件 `files.js` crc | 每个条目的 crc 与该文件现算一致（crc 不对 ⇒ thcrap 认为没变） |
| C 可选 | 发布 zip 同批 | `--full`：抽 3 个 wav + `cards.js` + DLL，比 zip 内 md5 vs modkit；不一致只报，不自动重打 |

## 开关

| 开关 | 作用 |
| --- | --- |
| `--modkit <dir>`（或 `MODKIT_DIR`） | 发布目标 modkit 根目录；**必须传** |
| `--skip-build` | 跳过 `make dll`，只校验 + 发布已构建产物 |
| `--allow-dirty` | 明确放行未提交的源文件（默认拦下并提示先 commit） |
| `--full` / `--with-zip` | 额外核对发布 zip 与 modkit 同批 |
| `--zip <path>` | 指定发布 zip（默认 `dist/th18_card_expand_bundle.zip`） |
| `--skip-guide-pdf` | 只生成 guide.tex 并查调试卡，不跑 xelatex（无 TeX 的机器） |

## 产物

- DLL：`native/th18_card_expand.dll`；
- 指南：`native/build/guide/guide.tex` 与 `guide.pdf`（gitignored 的构建目录）；
- modkit：patch + DLL + 资产由 `tools/deploy.py` 发布，回读校验通过才算完成。

## 出问题先看哪里

- 卡表 / 调试卡：`patch/th18/cards.js`，改完跑 `python3 mkfiles.py` 刷新 `patch/files.js`；
- 站点 / 生成物：`sites.py`、`tools/gen_def.py`、`tools/gen_drift.py`；
- 指南：`tools/make_guide.py` 的 `skip` 与 `PLAYER-GUIDE.md`；
- 发布：`tools/deploy.py` 的来源校验（落后 `origin/main` 会拒绝）、发布锁（`<modkit>/.deploy.lock`）、
  `tools/sync_assets.py` 的资产回读。

**不要在 `main` 上跑，不要 push，不要绕过 `deploy.py` 手拷文件。**
