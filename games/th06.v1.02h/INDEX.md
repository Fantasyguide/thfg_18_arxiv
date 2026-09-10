# TH06 v1.02h — 东方红魔郷（原版）

本页是版本入口；语义结论放在 `engine/`。这作的主要用途是**给 TH06NC 当对照基线**——
NC 是它的重制，两边的子系统一一对应，但地址与结构一律要在各自 exe 上重取。

| 项目 | 值 |
| --- | --- |
| PE | x86-32，imagebase `0x400000`，时间戳 2002-12-01 |
| SHA-256 | `327c4e923d7057d80d3057aed1e60389f4b560fabcec73c9dbb956ea68ec52ab` |
| MD5 | `99920041bc37e3ad4dc0201af84244c1` |
| exe | `local/th06.v1.02h/th06.exe` |
| Ghidra 工程 | `local/th06.v1.02h/ghidra_projects/th06.exe.gpr` |
| MCP `database_id` | `th06.v1.02h` |
| exe 内函数总数 | 1396 |
| 已命名 | 1267（90.8%） |
| 🔬 真·待挖 | 129 |

> ⚠️ **哈希与 th06-decomp README 记的不同**（那边写 `9f76483c…`），但**代码布局一致**：
> 社区 XML 的 1520 个函数入口有 **91.6%** 落在本库已识别的函数上，且
> `ResultScreen::OnUpdate` 区间内能找到社区源码写明的 `0x69ccdc` / `0x69cce1` 立即数对。
> 差异应在发行包装（资源/图标）而非代码。**用别的发行版之前请自己重跑对齐检查。**

## 符号来源：三层

| 层 | 来源 | 规模 | 方向 |
| --- | --- | --- | --- |
| ExpHP | `local/vendor/th-re-data/data/th06.v1.02h/` | 376 名字 + 76 类型 + 165 labels + 117 statics | 只进不出 |
| th06-re XML | `local/vendor/th06-re-xml/th06_102h.exe.xml` | **897 名字** + 596 签名注释 + 528 注释 | 只进不出 |
| 我们那层 | `games/th06.v1.02h/symbols.json` | 尚无（还没在本作做原创逆向） | 双向 |

两层上游都**不入库**（上游无 LICENSE，不擅自转发），由脚本重放；
`symbols.py` 的归属判定已把两层都算进基线，所以 `export` 不会把社区名字当成我们的活倒出来。

## HTML 视图（不抢库锁）

```sh
tooling/ghidra/export_html.py th06.v1.02h        # 导出（占一次锁，约 1 分钟）
tooling/ghidra/serve.sh       th06.v1.02h 6094   # 端出来
```

⚠️ 本机 6090/6091 常年被 th18/th16 占着，**换端口前先确认**——
`serve.sh` 不会检测端口冲突，`http.server` 绑失败只会在日志里报，页面照样是旧库的。

## 复现入口

```sh
source tooling/env.sh
python3 tooling/ghidra/bootstrap.py th06.v1.02h        # 建库 + ExpHP 层（幂等）
python3 tooling/ghidra/import_th06_re_xml.py --dry-run # 社区 XML 层，先看统计
python3 tooling/ghidra/import_th06_re_xml.py
python3 tooling/ghidra/symbols.py status th06.v1.02h
```

⚠️ 版本号要**写全**：`local/th06*` 会同时命中 `th06nc` 和 `th06.v1.02h`，
`_driver.resolve` 现在遇到歧义会直接报错而不是静默选一个。

## 与 TH06NC 的对照

| 子系统 | TH06 | TH06NC |
| --- | --- | --- |
| ECL | `EclManager__run_ecl`（ExpHP 命名） | [engine/ecl/th06nc](../../engine/ecl/th06nc/README.md) |
| ANM | `AnmManager__run_vm` `0x00433960`，跳表 `0x00434a63`（32 项） | [对照](../../engine/anm/th06nc/02-th06-baseline-comparison.md)：同一台机器，x64 重编译 + 4 条新 opcode |
| 结果页解锁 | `HOME×5 → Q×2 → S×3`（Extra 成绩页） | [engine/menu/th06nc](../../engine/menu/th06nc/02-classiclager-unlock-cheat.md)，`classiclager`（Lunatic 页） |

不要把 exe、DAT 或导出的 HTML 提交到仓库；它们都是本地研究输入。
