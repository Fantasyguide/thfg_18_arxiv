# 东方 modding 社区:工具 / 人物 / 权威来源速查
> **版本**：TH16 v1.00a（`th16.exe`，imagebase `0x400000`）。本文裸地址默认属该版本；引用其他版本须写成 `th18:0x…`。


新会话需要查某个东方格式或运行时行为时,先看这里"哪里查最准"。结论来自我们两轮 deep research
(都做了多源对抗验证)+ 源码核对。可信度:✅确证 / 🟡单源或推断 / ❓社区未解。

## 工具链全景:谁覆盖哪些格式

| 格式 | 官方/主流工具 | SHT 支持? |
| --- | --- | --- |
| ECL / ANM / MSG / STD / DAT | **thtk**(thpatch):thecl/thanm/thmsg/thstd/thdat | — |
| STD / ANM / MSG(现代重写) | **truth**(ExpHp):trustd/truanm/trumsg | — |
| **SHT** | **没有官方工具** | 见下 |

本地：`local/vendor/thtk`（**已编好** `build/thanm` `build/thdat`，`tooling/thtk/build.sh`）；
`local/vendor/truth`（Apache-2.0，只借 `map/*.anmm` 当 anmmap 备份源；主用 thpages 的 `static/mapfile/v8.anmm`）。
ECL 指令名：`local/vendor/eclmap`（Priw8/eclmap，`eclmap/th18.eclm`）。**ANM 编译只用标准 thanm**——产物要和社区生态互通，truanm 不用。

✅ **关键事实:thtk 和 ExpHp 的 truth 都不解析 SHT**(thtk 只能用 thdat 从 .dat 里解出 `.sht`
blob,不解析内部)。**不存在 `thsht` 这种官方工具**。网上"thsht 存符卡名"是错的(符卡名属 MSG)。

## ↗ 我们自己发布的:`Renko6626/th06nc-re-data`

<https://github.com/Renko6626/th06nc-re-data>(**CC0**)—— TH06NC 的符号数据与 mapfile 叠加层,
本仓 `games/th06nc/symbols.json` 的**单向下游**。TH06NC 此前社区零覆盖
(truth 的 `v0.anmm` 只到 ANM opcode 31、`th-re-data` 无 NC 条目、`th06-re` 是原版),这是第一份。

- **生成**:`python3 tooling/publish_th06nc_re_data.py local/vendor/th06nc-re-data`
  (幂等;`--check` 只比对不写,可当 CI)。
- **单向**:符号的唯一源是 `games/th06nc/symbols.json`,公开仓库只接收。别反过来编辑它。
- **发布安全**:`tooling/ghidra/tests/test_publish_th06nc_re_data.py` 守三条——
  不含游戏字节 / 不转发上游名字(mapfile 只准出现 32-35)/ 不漏本机路径与非 ASCII。
### TH06NC 的库不导上游名（2026-09-11 定）—— 但理由不是「侵权」

> ⚠️ **本节曾把理由写成「被许可证挡死」，那是错的**，且与本文第 108 行早已记下的
> 「用户确认可研究/移植（社区公开、作者允许），正式发布前建议注明出处」直接冲突。
> 更正如下。

**先把两件事分开：**

| | 是什么 | 风险 |
| --- | --- | --- |
| **用一个名字** | `EnemyManager__SpawnEnemy` 这种短的事实性标签 | 基本没有。它不构成可保护的表达 |
| **转发整份数据集** | th06-re 的 1267 条 `address ↔ name` 映射 | 这才是「不入库、不转发」针对的东西（汇编/数据库形态） |

**而迁名到 TH06NC 根本不涉及第二件事**：TH06 的地址一个都不会搬过去，
共享的只有词汇，地址与验证都是我们自己在 NC 上重做的。

所以**不导入 TH06NC 的库，是一个政策选择，不是法律必然**。成立的理由有两条：

1. **认识论**（独立成立，与许可证无关）：那批名字若靠**纯结构迁移**搬过来，
   按 [`../../METHOD.md`](../../METHOD.md) 的「迁名的两类」只能当线索——
   实测 ground truth 命中 0/25，见
   [`th06nc-name-transfer.md`](th06nc-name-transfer.md) §1。
2. **保持来源可分**：现在 th06nc 的 `symbols.json` 里带上游标记的条目是 **0 条**，
   混进去之后要区分「哪些是我们读出来的、哪些是借的」成本极高。
   这是工程卫生，不是合规要求。

**可以做、也应该做的**：把 TH06 的库当**参考**——开着对照读、拿它的名字帮自己认函数，
结论在 NC 自己的字节上重新取证。本轮 `EnemyManager__OnUpdate`、
`GameManager__AddedCallback` 就是这么来的。**若将来公开发布，按第 108 行注明出处。**

> ⚠️ **这条约束直接决定了 TH06NC 的迁名策略**：TH06 工程里那 1267 个已命名函数
> 绝大多数来自无 LICENSE 的 `th06-re`，**不能搬进 `games/th06nc/symbols.json`**。
> 能安全转发的只有 CC0 推出来的名字。三种批量手法的实测见
> [`th06nc-name-transfer.md`](th06nc-name-transfer.md)。

- **有 LICENSE**:选 CC0 就是因为 ExpHP 那边没有 LICENSE 给我们造成过麻烦,不想重复。

## ★★ TH06 专用金矿:`th06-re` 的 Ghidra XML

✅ **`local/vendor/th06-re-xml/th06_102h.exe.xml`**(15 MB)—— th06-re 社区工程的 Ghidra XML 导出:
**1520 个函数名**、718 条签名注释、1230 条注释、2310 个结构体定义。TH06 这作它比 ExpHP
(`th-re-data` 只给 274 个函数)丰富得多,是这作**第一个该去翻的**。

- **不能直接 import 成程序**:XML 引用外部 `th06_102h.exe.bytes`,我们没有。它只能当**符号/注释来源**,
  套在用真 exe 建好的库上——这正是 [`tooling/ghidra/import_th06_re_xml.py`](../../tooling/ghidra/import_th06_re_xml.py) 干的事。
- **上游无 LICENSE**,和 `th-re-data` 一样**不入库、不转发**;由脚本重放,
  `symbols.py` 的归属判定已把它并进基线。
- **有对齐闸门**:导入前先数 XML 函数入口在库里的命中率,低于 70% 直接中止。
  我们手上这份 exe 命中 **91.6%**,哈希与 th06-decomp README 记的不同但代码布局一致。
- 配套还有 `local/vendor/th06-re/src/th06_102h.exe/`(逐函数 C 源码)与 `local/vendor/th06-decomp/`。

## ★★ TH08(永夜抄)专用:`Patchouli-CN/th08_dec`(**MIT** 反编译源码 + 符号表)

✅ **<https://github.com/Patchouli-CN/th08_dec>**（用户 2026-09-16 提供；本地克隆 `local/vendor/th08_dec`，
**gitignored**、可重克隆）。本仓卡 **108 = TH08(永夜抄)系统卡**(妖机体 / 使魔)，而本作此前
**没有任何源码级来源**（`th-re-data` 对 TH08 的覆盖远不如 TH06/TH10+）⇒ **TH08 侧第一个该去翻的**。

| 项 | 值（本轮实测）|
| --- | --- |
| 目标 exe | TH08 **v1.00d**，上游记的 sha256 `330fbdbf58a710829d65277b4f312cfbb38d5448b3df523e79350b879213d924` |
| **我们的样本** | `local/th08.v1.00d/th08.exe`，sha256 **`9d669602…`** ⇒ **哈希不同**（另一个 1.00d 构建）|
| **布局是否对得上** | ✅ 本轮抽样核过：vtable 项 **6/6** 落在 `.text` 内、按 CSV 抽 **400/400** 个函数入口都是 `push ebp; mov ebp, esp` ⇒ **同布局**（与 TH06 那边「哈希不同但布局一致」同一情形）|
| 内容 | `src/*.cpp|hpp`（**MIT**，2030 个函数里约 93% 已实现、平均匹配率 91.2%）、`config/reccmp-functions.csv`（**2221 条 名字↔地址**）、`reccmp-globals.csv`（含 vftable）、`reccmp-floats.csv` / `reccmp-strings.csv`、`objdiff.json` |
| 许可证 | **MIT**（Copyright (c) 2025 KSS）⇒ 与 `th-re-data`（无 LICENSE）/`th06-re`（无 LICENSE）**不同**：**注明出处即可引用**，比那两家宽松 |
| 可信度 | 🟡 **单源**（社区反编译，本仓尚未逐条对账）——引用它的任何断言都要落到 `th08:0x…` + 我们自己的样本上，**别直接写成「一手」**；本体是 `-Od` 反推、stub/低匹配仍多（字节大小率 14.8%）|

**怎么用**（照 `th-re-data` 那一套）：① 开着源码对照读（`src/` 逐函数）；
② 把 `reccmp-functions.csv` 的 `名字↔地址` 当**命名层**（地址空间与我们样本一致，可直接套）；
③ 保持来源可分：导进 Ghidra / `symbols.json` 时**标明来自 th08_dec（MIT）**，
别与「我们自己读出来的名字」混成一份无出处的清单（理由见本文「TH06NC 的库不导上游名」那一节）。

## ★★ 逆向符号金矿:ExpHP `th-re-data`(逆向时第一个去翻的)

✅ **`exphp-share/th-re-data`** —— ExpHP 从 binja 导出的**逐游戏符号数据库**,是**当前最硬的 TH16(及他作)
一手运行时源**。逆向 exe 时**先翻它**,能省掉大量从零命名。
- **内容**(`data/th16.v1.00a/` 等逐版本目录):`funcs.json`(TH16 约 932/1930 函数命名)、`statics.json`
  (全局/静态符号 + 类型)、`type-structs-*.json`(结构体布局,如 `zEclVm`/`zEclRunContext`/`AnmManager`…)。
  覆盖重 **ANM / laser / ECL / bullet**;**SHT 几乎为零**(SHT 运行时语义是我们的原创产出)。
- **粒度**:**只给"叫什么 / 字段在哪",不给"干什么"**(`comment` 0 条)。**当命名层用,语义层自己做。**
- **位置**:本地克隆 `local/vendor/th-re-data`(**gitignored**,可重克隆);上游 `exphp-share/th-re-data`;
  我们的 fork `Renko6626/th-re-data`(已提 **PR #7** = 15 个 MainMenu funcs、**issue #8** = 导入脚本,ExpHP 已邀 PR)。
- **怎么用**:**`tooling/ghidra/import_th_re_data.py`** —— 把 funcs/statics 名 + 注释一键套进 Ghidra
  (safe 默认不覆盖已有名;`--overwrite` / `--dry-run`)。建新工程或重建时跑它白得命名上下文。
  (当前 `th16` Ghidra 工程**已套用并落盘**,重开即见。)
- 配套权威:Priw8 `eclmap`(ECL 指令名)、`thtk`(格式权威)。⚠️ 上游不太活跃;我们的 ghidra-re MCP 也已 fork 自用
  (见 `ghidra-mcp-tools.md`)。

## SHT 的事实标准:Priw8

✅ **Priw8**(thpatch 成员、thtk 贡献者)是 SHT 工具的主要作者,也是新作 SHT 的事实权威。

- **sht-webedit** <https://github.com/Priw8/sht-webedit>(部署站 <https://priw8.github.io/sht-webedit/>)
  - 浏览器内编辑 .sht;**逐版本二进制 schema** 在 `js/struct/struct_NN.js`,通用 reader/writer 在
    `js/import.js`/`js/export.js`。是目前最完整的逆向文档。
  - ✅ 部署站版本选择器覆盖 **TH07–TH19(獣王園/UDoALG)**;GitHub README 过时(只到 TH18)——
    **以部署站和源码为准**。
  - 本地克隆见 `local/local/vendor/sht-webedit`(@98b8cca)。
- **shmupcc-sht** <https://github.com/Priw8/shmupcc-sht>
  - quickjs CLI 编译器:`-c` JSON→二进制、`-d` 二进制→JSON(类似 ECL 的 .decl↔.ecl)。
  - 架构干净(声明式 `Struct` + 校验 + JSON 互转 + 版本注册表),值得借鉴。
  - 🟡 但当前只注册了 `"shmupcc"` 一个**归一化格式**(✅ 已本地核对:**大端 + float64**),**不含
    真实东方逐版本 reader**;真实 schema 仍取自 sht-webedit。本地克隆 `vendor/shmupcc-sht`(@dcf1f91)。
- ⚠️ 两个仓库**都无 LICENSE 文件**;用户确认可研究/移植(社区公开、作者允许),正式发布前建议注明出处。

## 其他权威来源

- **pytouhou** <https://pytouhou.linkmauve.fr/doc/> + 源码
  - 东方引擎的 Python 重写;有 **Gen-1(TH06/07/08)** 的 SHT 结构文档和发弹逻辑实现。
  - 🟡 仅适用旧引擎,**不能外推到 TH15-19** 的 func_*/shooterset 模型。但发弹判定
    `(fire_time + delay) % interval == 0`、type 选择器(3=激光/2=加速/1=寻的)等可参考思路。
- **Mddass 东方文件格式规范**(touhouwiki User 命名空间)
  <https://en.touhouwiki.net/wiki/User:Mddass/Touhou_File_Format_Specification/SHT>
  - 🟡 二手草稿,**只覆盖 TH07/TH12/TH13**,无 TH14-19。自动抓取常被 Cloudflare 418 挡。
  - ❌ 其"PCB 0x28 枚举是 func_* 前身"的说法被对抗验证否决(0-3),PCB 枚举只是 PCB 专有。
- **thpatch / thcrap** — 翻译补丁生态,东方逆向社区主阵地(论坛、贡献者网络)。
- **THBWiki「游戏攻略/东方天空璋/系统」**（用户 2026-09-09 贴来）
  <https://thwiki.cc/游戏攻略/东方天空璋/系统>
  - 🟡 攻略体感数据（作者自承「斜体所写的数据笔者不能确定」），但**与 TH16 一手反编译逐条吻合**：
    季节子机 1~6 级累计 100/230/390/590/840/1140 个季节点、三条季节点来源（打倒敌人 / 擦弹 / 消弹）、
    解放后槽清零（夏・土用只扣 1 副机）——见
    [`season/th16/01`](../season/th16/01-season-data-and-tables.md) §8 的逐条对账表。
  - ✅ 可用作「无敌帧 9/30/60、秋加速、冬加伤、绿分数道具」这类**攻略才有的一手体感**的交叉源；
    ⚠️ 数值一律先与反编译对账再采用，别单源采信。

## 运行时关键结论(✅ 已确证,sht-webedit README)

- 发弹计时器逐版本:PCB=60 帧;IN–DDC(除 MoF)=15 帧;MoF=16 帧。
- **LoLK 起**:新增 `fire_rate2`/`start_delay2` 走 **120 帧**计时器,设置后覆盖原字段;该计时器
  "有著名偶发 bug 导致停止发射"——❓ 代码级根因社区未给出。

## func_* 调用点语义(✅ ExpHP,th07–17,已并入 PR #8;非「索引→行为」表)

社区**没有**索引→行为表(见下「未解」),但**调用点已知**,是反汇编的下手锚点:
`func_on_init`=对象创建时;`func_on_tick`=每帧 `Player::on_tick`;`_old_on_draw`=`Player::on_draw`
(MoF/TH16 起废弃不调);`func_on_hit`=命中敌人时(音效 / UFO 早苗B 溅射)。`rate2/delay2`= 上面
120 帧计时器,**TH16 副季节 sub-shot 的计时即走此**(非特殊 func/flag)。
🟡 flags 段疑为 **load-time 替换成 thiscall 函数指针的派发槽**(RUEEE 单源,TH15 枚举基数 5/4/2/6;
与 sht-webedit `flags_len` 字节数对不上,**以反汇编为准**)。详见 `engine/sht/th16/02-*.md`。

## ❓ 社区公开未解(= 我们反汇编的目标)

- `func_on_init/tick/draw/hit`(TH19 变 4× `func_?`)逐游戏的"索引→行为"映射:**社区无任何已发布表**,
  作者自述 "documentation is yet to be made"。需对各 exe 反汇编。
  - ✅ **TH16 已由我们破解(首次)**:加载时四张硬编码函数指针表把索引换成函数地址。解析器
    `sht_parse_resolve_funcptrs` `0x443790`;表址 init=`0x4919c0`/tick=`0x4919a0`/draw=`0x4a6f04`(全 null,
    TH16 不用)/hit=`0x491980`。**TH16=季节系统**,tick 索引经解包全部 pl0X.sht + wiki 互证:
    **idx0=直线(夏/琪露诺,散射靠几何)、idx1=追踪(春/灵梦)、idx2=激光(冬/魔理沙)、idx5=匀加速(秋/文)**。
    关键变量(✅一手):**slot+0x64=移动角、slot+0x60=速度**(易标反)、+0x90 目标句柄、+0xa0 激光长度、
    find_nearest_enemy`0x425240`、atan2`0x487aaa`(已坐实)。idx3/5 = `speed+0x60 += 常量`(匀加速,非曲率;
    游戏内实测 Aya 加速)。详见 `engine/sht/th16/03-*.md`。**仅 TH16,勿外推。**
- `flags` 段(TH14-18 = 0x20 字节,TH19 = 0x3c)位含义:作者自述 "who knows"(repo Issue #6)。
- main 头部 `unknown_2..unknown_10`、shooter 的 `unknown_sht_float/byte`:无文档。

> 解开以上任意一项后,把"结论 + 游戏/地址/函数 + 可信度"提炼到 `shared/sht-field-semantics.md`。

## 自机判定点 / 擦弹半径:逐版本 + 「TH16 起全角色统一写死」(✅ wiki + 我们 RE 互证)

- **历史**(Touhou Wiki *Hitbox*,🟡 搜索引擎摘录,直连被反爬挡):TH13 神灵庙起判定由方→圆;
  **TH13 神灵庙 → TH15 绀珠传:灵梦判定半径 2.0px、其余角色 3.0px**(灵梦更小=传统优势)。
  擦弹范围自 UFO(TH12)起**与判定点大小挂钩**(灵梦小则擦弹小、魔理沙大则擦弹大)。
- ✅ **TH16 天空璋 = 统一起点**:wiki 注明 TH16 起有一组判定尺寸(2.0/2.4/2.7px)**"完全未使用"**。
  **我们逐字节反汇编证实**:TH16 `player_shot_init@0x440fb0` 把 .sht 的判定字段用 exe 内**按角色表**
  (`DAT_00492c98/0c78/0c88[char]`)覆写,实测 4 角色**值完全相同**:
  - **hitbox = 3.0**(真判定半径,`0x4439e0` 碰撞用);grazebox 字段 = 5.0(实被挪作释放弹速度);
    itembox = 60.0(收集半径,半宽30);另有 exe 常量 `DAT_00492c68`=100.0(道具吸附半径,半宽50)。
  - 擦弹半径 = hitbox(3.0) + 弹尺寸余量,无角色差异。
  → **结论:灵梦"判定点更小"保留到 TH15,TH16 取消、全角色统一 3.0**;判定/擦弹半径**不可经 .sht 编辑**
    (硬编码于 exe)。可信度:统一值=✅一手;TH13–15 逐作数值=🟡(wiki 单源,绝对确认需各作反汇编)。
  - 出处:Touhou Wiki [Hitbox](https://en.touhouwiki.net/wiki/Hitbox) / [Graze](https://en.touhouwiki.net/wiki/Graze);
    一手见 `engine/sht/th16/05-*.md` §2b/§4b。

## ★★ TH07 源码重建工程（`D:\项目\幻想指南\18\th07`，本机路径；社区，2026-09-16 探明）

**是什么**：TH07 v1.00b 的**源码重建**（GensokyoClub th06/th08 同一路线）——目标 = 原版日文 1.00b
（`size 650752` / `sha256 35467eaf…` / `md5 0126afce…`，见其 `config/target.toml`），验收 = **逐函数字节比对**
（`config/functions.csv`，1527 行：`address,size,span_end,current_name,proposed_name,module,status,match_percent,source_file,evidence`）；
`src/` 里的 `.cpp` 只在"100% 匹配"后才落地（其 `AGENTS.md` 的规矩），所以它是**可读且等价于原版**的一手级参考 ——
但仍按本仓 `METHOD.md` 的分级当**社区证据**用，关键结论用我们自己的 `local/_scratch/th07.asm` 复核。

**★ 与我们的样本的关系（本机实测，2026-09-16）**：我们的 `local/th07.v1.00b/th07.exe`
（= 用户那份 `[th07] 东方妖妖梦 (汉化版)\th07.exe`，两者 **md5 全等** `d08774b90b821b6a2764280bb146a400`）
与工程目标**尺寸相同、哈希不同**（汉化补丁就地改过），但**代码区逐字节相同** ——
用账本里带 `SHA <16hex>` 的 69 条（排除 `third_party`/`runtime` 库行）对账：**60 条逐字节相同**，
不同的 9 条**全在 `pbg`（LZSS 编/解码那一族）** = 汉化版重打包了 PBG 档案 ⇒ 只动了编解码器。
⇒ **我们的地址/反汇编对两边都有效**；差异只影响"读档案"，与玩法逻辑无关。

**怎么用**（给后续做 107 的人）：
· 查"某个地址属于哪个函数"：用 `span_end` 列做区间查询（例：`0x441530`/`0x441553`/`0x44162a` 都在
  **`0x441330`** 里 = 结界/樱点管理器；`0x433742`/`0x43370b`/`0x4330db` 都在 **`0x432990`** 里 = 道具/分值）；
  ⚠ 这两个函数**尚未重建**（`status` 空）⇒ 这一段只能读我们自己的 `th07.asm`。
· 已重建且与 107 相关的：`src/Chain.cpp`（`Chain_cut` / `Chain_add_to_calc_chain` / `Chain_run_calc_chain` … 13 个）、
  `src/Player*.cpp`（27 个，含 `0x441960 Player_finish_deathbomb` = 我们记的"环的缩放插值 `0x441b56`"所在函数）、
  `config/known-symbols.csv` / `known-globals.csv`（结构体与全局命名）。
· 自检口径（想复核"某函数是否两边一致"）：取账本那一行的 `address`..`span_end` 字节，算
  `sha256(...)[:16]`，与该行 `evidence` 里的 16 位十六进制比 —— 相等即逐字节相同（上面那条结论就是这么来的）。

