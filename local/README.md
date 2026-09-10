# local/ — 本地样本与工程（**不入库**）

**纪律：本仓库不留任何版权字节。** 游戏 exe、解包资产、Ghidra 工程、第三方仓库克隆
一律只存在于这个目录，根 `.gitignore` 把 `local/*` 全部忽略，只保留本文件。

新会话/新机器上这里是空的——照下表自己放回去。

## 布局

```
local/
├── th16.v1.00a/          TH16《鬼形兽》
│   ├── th16.exe          md5 cb9caf54ce5738f70086e783ec88fd2a  (683,520 B)
│   ├── th16.dat          原始归档(77 MB)
│   ├── pl0*.sht          从 .dat 解出的自机配置 + 我们改造过的实验档
│   ├── e0*.msg staff*.msg 结局/staff roll 样本
│   ├── ghidra_projects/  ★ 主工程 th16.exe.{gpr,rep}
│   └── th16.exe_ghidra/  早期工程(已弃用,留档)
├── th18.v1.00a/          TH18《虹龍洞》
│   ├── th18.exe          md5 9969cac756098c1da05a81de45437a70  (847,360 B)
│   ├── th18.dat          原始归档(112 MB)
│   ├── dat/              thdat -x 解出的 225 个文件(tooling/thtk/unpack.py)
│   ├── anm/<名>/         每个 anm 一目录:<名>.anm.txt(thanm spec)+ 贴图;56 个
│   └── ghidra_projects/  ★ 主工程 th18.exe.{gpr,rep}
├── vendor/               第三方仓库克隆(只记来源，不转发源码)
│   ├── th-re-data/       ★★ ExpHP 逐版本符号金矿(funcs/statics/structs)
│   ├── thcrap/           ★ thcrap 一手源码(插件/breakpoint/binhack 机制的权威)
│   ├── thcrap-patches/   ★ ExpHP 的 17 个玩法补丁(零 DLL 的真实范例 + keystone 构建链)
│   ├── thpages/          ★ ExpHP 指令参考站源码(ANM 渲染管线/坐标系/MSG 指令表)
│   ├── thtk/             thecl/thanm/thmsg/thstd/thdat;build/ 是本机编的 thanm+thdat(tooling/thtk/build.sh)
│   ├── bisonflex/        免 sudo 解出来的 bison/flex/m4(.deb → dpkg -x),只给编 thanm 用
│   ├── eclmap/           Priw8 的 ECL 指令名表（th18.eclm 等），thecl -m 用
│   ├── truth/            ExpHP 现代重写(truanm 等,Apache-2.0);只借它的 map/ 当 anmmap 备份源
│   ├── sht-webedit/      SHT 字节布局的社区事实标准
│   ├── shmupcc-sht/      另一个 SHT 参考实现
│   └── th16.eclm th16.eclmap
└── scratch/              smoke_test 工程等一次性产物
```

## 怎么放回去

> ✅ **TH16 样本已可用（2026-09-09）**：本机两份 `th16.exe`（Steam 安装
> `F:/D/SteamLibrary/steamapps/common/th16/`、桌面副本）都是 **Steam 包装版**
> （md5 `701a603c2afc686776ffddacbed4ee4e`，812,400 B，`.text` 熵 8.000 = SteamStub Variant 3.1 加密）。
> 已用 **Steamless** 在 Linux 上脱壳（见下），得到 `local/th16.v1.00a/th16.exe`
> （683,520 B，md5 `70d3429444e1e6c18a72e7200a4d7ce8`，`.text` 熵 6.504）；它与仓库记录的零售 md5
> `cb9caf54ce5738f70086e783ec88fd2a` 不同，**只因 Steamless 重建了 PE 容器**——代码段字节是零售的
> （`th-re-data` 的 v1.00a 函数地址逐一对上序言，抽检 40/40）。原始包装版留档为 `th16.exe.steamstub`。
>
> **脱壳配方**（本机无 Mono/Wine，用 .NET 8 直接编上游源码，不修改上游文件）——完整说明见
> [`../tooling/steamless/README.md`](../tooling/steamless/README.md)：
>
> ```bash
> git clone --depth 1 https://github.com/atom0s/Steamless.git local/vendor/Steamless   # CC BY-NC-ND：只本地跑
> curl -sSL https://dot.net/v1/dotnet-install.sh | bash -s -- --channel 8.0 --install-dir "$HOME/.dotnet"
> cd tooling/steamless && ~/.dotnet/dotnet build -c Release
> ~/.dotnet/dotnet bin/Release/net8.0/steamless-linux.dll <包装版.exe>
> ```
>
> ⚠️ 结尾别加 `--recalcchecksum`（走 `imagehlp.dll`，Linux 上必抛异常）——**脱壳文件在此之前就已落盘**。

1. **游戏 exe / .dat**：用你自己合法持有的副本，按上表哈希核对后放进对应版本目录。
   **不要去下载。**
2. **vendor 克隆**：各仓库上游 commit 记录在
   [`../engine/_shared/community-sources.md`](../engine/_shared/community-sources.md)，按记录重新 clone。
   两个 SHT 参考仓库**均无 LICENSE 文件**，正式移植/发布前需确认授权，故不直接提交其源码。
   ExpHP `th-re-data` 上游同样无 LICENSE——它的函数名可用于本地逆向，但不擅自转发。
3. **Ghidra 工程**：不必找回，用
   [`../tooling/ghidra/bootstrap.py`](../tooling/ghidra/bootstrap.py) 从 exe 重建
   （建库 → 分析 → 套 ExpHP 名 → 套结构体 → dump → 落盘）。

## Ghidra 工程与 MCP

`ghidra-re` MCP 的 `open_database` 直接指工程里的 exe：

| 版本 | `file_path` | `database_id` |
| --- | --- | --- |
| TH16 | `local/th16.v1.00a/th16.exe` | `th16` |
| TH18 | `local/th18.v1.00a/th18.exe` | `th18` |
| TH06 | `local/th06.v1.02h/th06.exe` | `th06.v1.02h` |
| TH06NC | `local/th06nc/th06nc.exe` | `th06nc` |

⚠️ 版本号别只写 `th06`——`local/th06*` 同时命中 `th06nc` 与 `th06.v1.02h`，
`_driver.resolve` 遇到这种歧义会直接报错，要求写全。

MCP 支持多库并存，两个 `database` 可以**并排对照**同名函数（ExpHP 命名一致），
这是把 TH16 已反清楚的逻辑映射到 TH18 的主要手法——
但**只能参考逻辑，地址/偏移必须在目标 exe 上重取**
（[`../games/th18.v1.00a/port-plan.md`](../games/th18.v1.00a/port-plan.md)）。

## `vendor/th18_modkit`（发布仓库）

`Renko6626/th18_modkit` —— **mod 包发布仓库**：自带 thcrap（2024-11-06 stable）+ 勾选式启动器，
朋友 clone 下来放好 `th18.exe` 就能一键启动。renkolab 是开发仓库，产物往那边推：
patch 进 `thcrap/repos/Renko_1055/<id>/`，DLL 进 `mods/`（带 `.json` 侧车）。
它的 `thcrap/repos/nmlgc/base_tsa/th18*.js` 就是 `sites.py conflicts` 要对照的那份。
