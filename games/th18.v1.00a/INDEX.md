# th18/ — TH18(東方虹龍洞 / Unconnected Marketeers)逆向工作区

> **版本**：TH18 v1.00a（`th18.exe`，imagebase `0x400000`）。本文裸地址默认属该版本；引用其他版本须写成 `th16:0x…`。

> **独立于 th16 的工作区**,避免结论混淆。本目录放 **TH18 自己一手验证过的结论**;
> TH16 的成果只作**方法论 + 认知地图 + 待验假设**来源,**地址/偏移一律按 th18.exe 重取**(见 [`port-plan.md`](port-plan.md))。
> 新会话先读本 README,再读 [`port-plan.md`](port-plan.md)。纪律:`METHOD.md`。

## 当前状态:✅ 基础建设与卡牌主线已完成，符号往返已跑通(2026-09-01)

> 新会话**无需重做导入**。要重建或补齐，一条命令：`tooling/ghidra/bootstrap.py th18`(幂等)。
> 若目标是协作制作运行时卡牌改造，先读 [ROADMAP](../../mods/th18.v1.00a/card-rework/ROADMAP.md)。
> 注入链路本身已不再是未知数——见 [`mods/th18.v1.00a/mouse-control`](../../mods/th18.v1.00a/mouse-control/README.md)
> (首个实跑通过的 TH18 注入产物)与 [`mods/thcrap-platform.md`](../../mods/thcrap-platform.md)。

- ✅ **th18.exe 已全量分析**(Ghidra headless,函数 **2333** 个,已命名 **1366**)。
- ✅ **ExpHP 名字已套满**:`skipped=874 missing=1`。原先漏掉的 63 个只经 vtable 进入的回调
  (含 `Player::on_tick` `0x45caa0`、`BulletManager::on_tick` `0x424e70`)已由
  `create_missing_funcs.py` 补建函数后套上,只剩 1 个 CRT 初始化器(落在别的函数体内,无价值)。
- ✅ **157 个结构体 + 2 枚举 + 2 位域已建**,`statics` 类型已套 **151** 项(`failed=0`)。
- ✅ **492 条 labels 已套**:VM 指令分发函数体内的 opcode case 标签
  (ecl 242 / anm 136 / msg 36 / std 21 / card 57),详见下节。
- ✅ **卡牌主线已一手反出**:核心架构/调用接缝、资源经济、58 项注册表、商店规则与卡牌目录见
  `engine/card/th18/` 的 01–09 共九篇（对象模型 / 生命周期 / 钩子全表 / C 键释放 / 商店与金钱 /
  资源经济 / 注册表 / 逐卡目录 / 社区对账）。
- ✅ **22 条 Shift-JIS 串已认出并落库**（Ghidra 的字符串分析器认不出日文，原先一条没有）：
  标题、字体名（ＭＳ ゴシック / メイリオ / ＭＳ 明朝）、卡牌与实绩文案模板
  （`を手に入れた！` / `の実績を手に入れた`）、音乐室剧透警告。
- ✅ **符号往返已跑通**:我们自己那层 **504** 条存进 [`symbols.json`](symbols.json)(含 268 条函数原型),
  全量重建后 0 漂移。见 [`tooling/ghidra/README.md`](../../tooling/ghidra/README.md) 的「两层符号」。
- ✅ **首个实跑通过的 TH18 注入产物**:[`mods/th18.v1.00a/mouse-control`](../../mods/th18.v1.00a/mouse-control/README.md)
- ✅ **卡表搬进 codecave、分配出非原生 id**（2026-09-02）:[`mods/th18.v1.00a/card-expand`](../../mods/th18.v1.00a/card-expand/README.md)，
  100 处 binhack + 开机自检 DLL；`allocate_new_card(id=58)` 实跑成功。下一步存档层，见 [`NEXT.md`](../../mods/th18.v1.00a/card-expand/NEXT.md)
  (thcrap 断点 + 自建 DLL,鼠标操自机 + 左/右/中键映射,2026-09-01 实跑)。
  它同时验通了 Linux 交叉编译 → Windows 的交付链路,以及 `engine/player/th18/01` 的坐标结论在活进程上成立。
- ✅ **SHT 行为函数表已一手验完**（2026-09-06）:四张表 18 项、15/120 帧计时器、头部判定字段无人读、flags `+0x4c` 读取点。见 [`engine/sht/th18/03`](../../engine/sht/th18/03-func-tables-timers-and-dead-fields.md)，wiki 稿 [`engine/sht/WIKI-DRAFT.md`](../../engine/sht/WIKI-DRAFT.md)。
- ✅ **MSG 两套指令集已一手验完**（2026-09-06）:关卡对话 0–36 逐 case、结局/staff 0–17、ECL 518/519 握手、
  跳过机制、立绘 ANM 中断协议、结局编号公式；44 个真实 `.msg` 实测。见 [`engine/msg/th18/`](../../engine/msg/th18/README.md)，
  面向 THBWiki 的成稿在 [`engine/msg/WIKI-DRAFT.md`](../../engine/msg/WIKI-DRAFT.md)。
- ⏳ **仍未做**:装备卡 shooter 数据存储与少数标 🟡 的字段仍待验证。
  类型绑定**卡牌那 268 个已做**,`Player__*` / 各 Manager / 全局仍未绑,见下节。

## ★ 让反编译变可读:类型绑定(卡牌那 268 个已绑)

ExpHP 给的是**名字 + 结构体布局**,不给**绑定**(哪个函数的哪个参数是哪个类型)。没绑之前:

```c
undefined4 __fastcall CardLife__destructor(int param_1)
    if ((GAME_THREAD_PTR != 0) && ((*(byte *)(param_1 + 0x50) & 2) != 0)) {
```

绑上之后:

```c
bool __thiscall CardLife__destructor(zCardBaseClass *self)
    if ((GAME_THREAD_PTR != 0) && ((self->flags & 2) != 0)) {
```

**已做**(2026-09-01,[`tooling/ghidra/bind_types.py`](../../tooling/ghidra/bind_types.py)):
`Card*__<vtable 槽>` **268 个函数**已按 ExpHP 的 `zVTableCard` 逐槽 C 签名绑定,
并给 31 个大于基类的卡类建了带填充的子类结构体(`zCardTenshi` 等),
免得子类字段被渲染成 `self[1].card_id` 那种误导。规则是数据:
[`tooling/ghidra/bindings/th18.v1.00a.json`](../../tooling/ghidra/bindings/th18.v1.00a.json)。

**还没做**:`Player__*`(this 有时是 `zPlayer*` 有时是 `zPlayerInner*`,故意不一刀切)、
各 Manager(tier 2,`--tier 2` 可开)、以及全局变量的类型绑定。

产出属于「我们那层」,已在 [`symbols.json`](symbols.json)(504 条,其中 268 条带原型)。
回退靠 [`bindings.json`](bindings.json) + `bind_types.py th18 --revert`。

## ★ labels:492 条现成的 opcode 表

`labels.json` 本仓在 2026-09-01 前从没读过。每条 = 一个 **opcode → 处理分支地址**:

| 组 | 条数 | 宿主函数 |
| --- | --- | --- |
| `ecl` | 242 | `0x430d30` 175 条 + `EclRunContext__ecl_run` 67 条 |
| `anm` | 136 | `AnmVm__run` |
| `msg` | 36 | `GuiMsgVm__run` |
| `std` | 21 | `StageInner__run_std` |
| `card` | 57 | `AbilityManager__allocate_new_card` |

两点值得注意：`0x430d30` 我们库里**还没命名**,但它体内有 175 个 ECL opcode case——身份就此确定;
`card` 那 57 条与 `engine/card/th18/07-registry.md` 的 58 项注册表是两边独立得出的——
**交叉对名已完成**(2026-09-01):57 条标签 = `allocate_new_card` `0x412dac` 那张 57 项跳转表,
与注册表 58 项减去两个菜单哨兵吻合。见
[`engine/card/th18/10-extensibility-limits.md`](../../engine/card/th18/10-extensibility-limits.md) §2。

## 这是什么 / 目标

TH18 = **東方虹龍洞**(Unconnected Marketeers),特有机制是**卡牌/能力(card/ability)系统**(取代了 TH16 的季节系统/炸弹)。
目标同 TH16 路线:**搞懂引擎运行时语义**(自机/弹幕/敌人/道具…),为 IDE 的多作支持铺路。分两步:
1. **验证共通引擎**:TH16 已反的引擎子系统(自机状态机/碰撞/开火/option/伤害管线/资源经济/SHT 格式)在 TH18 大概率**结构同形**——快速**验证**(不是假设成立)。
2. **新反特有机制**:卡牌/能力系统(`zCard*`/`zAbility*`)是 TH18 独有,**从零(但有 ExpHP 结构锚点)反**,像 TH16 反季节那样直接进语义层。

## 与 th16 的关系(★复用什么 / 不复用什么)

| 复用 ✅ | 不复用 ❌(必须按 th18 重取)|
| --- | --- |
| 方法论(锚点常量、func 表、ExpHP 命名、证据链纪律)| **所有函数/全局地址**(th18.exe 重新编译,全变)|
| 工具(`tooling/ghidra/import_th_re_data*.py` 直接指向 `local/vendor/th-re-data/data/th18.v1.00a`)| **结构体字段偏移**(th18 结构体长大了,例:`zPlayer` th16=0x2c828 / **th18=0x479d4**)|
| 认知地图(知道该找哪些子系统/字段语义)| **特有机制**(TH16 季节/炸弹 ≠ TH18 卡牌/能力,代码不同)|
| 字段的**语义含义**(state/iframes/flags/hitbox… 是什么)| **func_* 跳转表内容、玩法数值**(逐作不同)|

→ 一句话:**搬"概念和语义",查 ExpHP th18 结构体拿"新偏移",到 th18.exe 验"新地址/新行为"。**

### ★ 强烈鼓励:先读 TH16 的结论和 Ghidra 工程(但只"参考逻辑",不照抄)

开工前**务必先翻 TH16 的成果当地图**——它们告诉你"该找什么、长什么样、坑在哪",能省掉大量探索:
- **TH16 findings**:`engine/player/th16/01-06`(自机生命/季节释放/火力/option/字段图/资源经济)、`engine/sht/th16/03,05,07,08`
  (SHT func 表/字段/shooterset/伤害管线)、`engine/_shared/`(引擎数学/主循环/归档)。
- **TH16 Ghidra 工程**:可同时 `open_database local/th16.v1.00a/th16.exe`(database_id `th16`)与 th18 **并排对照**——
  对同名函数(ExpHP 命名一致)看 TH16 已反清楚的版本,理解逻辑后**再去 th18.exe 重新定位/验证**。
  (MCP 支持多库;`decompile_function` 指定不同 `database` 即可左右对照。)

**但只能"参考逻辑",不得直接搬运**:
- ✅ 可借:控制流/状态机形状、字段的**含义**、"该有哪些步骤"、命名、踩过的坑(见各 finding 的 🟡/❓/纠错段)。
- ❌ 不可抄:任何**地址、偏移、func 表内容、数值、帧数**——这些是 TH16 的,th18 必须**自己一手取/验**。
- 落笔规矩:从 TH16 借来的判断在 th18 文档里写成"**(TH16 如此,th18 待验)**",**在 th18.exe 验证通过后**才去掉"待验"。
- 一句话:**拿 TH16 当"已解出的参考答案"对照思路,但每一步都要在 th18 上重新算一遍、对得上才算数。**

## ExpHP th18 盘点(实测 `local/vendor/th-re-data/data/th18.v1.00a/`,2026-06-13)

ExpHP 对 TH18 积累与 TH16 同量级,引擎结构体填得很满,**卡牌系统也已结构化映射**:

- 命名函数 **729**、statics 154、结构体(own)**157**。
- **引擎结构体(共通,可直接套)**:`zEnemyData 71/71`、`zEclVm 9/9`、`zBullet 48/56`、`zPlayer 18/22`、
  `zPlayerInner 9/14`、`zAnmVm`、`zSupervisor 34/46`、**`zShtShooter 16/19`(同 th16,size 0x58)**、`zItem`、`zGui`。
- **TH18 特有(卡牌/能力,新反锚点)**:`zCardBaseClass`、`zCardEquipmentSingle`、`zCardList`、`zCardMomoyo`、
  `zVTableCard`、`zTableCardData`、`zAbilityManager`、`zAbilityMenu`、`zAbilityText`。
- **注意缺失**:`zBomb` **MISSING**(TH18 无炸弹对象,卡牌/能力取代)、`zShtRawFile` 未命名(`zShtShooter` 在,
  SHT 格式应仍在,只是 ExpHP 没标完整文件结构)。

## 环境(工具与 th16 共用,但**工程/文件独立**,无 sudo)

- Ghidra 12.1.2 + conda env `ghidra`(同 `tooling/ghidra/README.md`)。MCP `ghidra-re` 同一套工具。
- **★ TH18 是独立的一套文件 + Ghidra 工程,不与 th16 共用**(不同 exe → 不同 project)。位置对照:

  | 作品 | exe | MCP `database_id` | headless project-dir / project / program |
  | --- | --- | --- | --- |
  | TH16 | `local/th16.v1.00a/th16.exe` | `th16` | `local/th16.v1.00a/ghidra_projects` / `th16.exe` / `/th16.exe` |
  | **TH18** | **`local/th18.v1.00a/th18.exe`** | **`th18`** | **`local/th18.v1.00a/ghidra_projects`** / `th18.exe` / `/th18.exe` |

  详见 `local/th18.v1.00a/README.md`。**可同时开 `th16` 与 `th18` 两库并排对照**(`decompile_function` 指定 `database`),
  结论各写各的。
- **样本**:`th18.exe`(32 位 PE)由用户放进 **`local/th18.v1.00a/`**(gitignored),**没有就先问用户、不要下载**。
- **重建/补齐整个库**(开工第一件事,幂等;先在 MCP 里 `close_database` 释放工程锁):
  ```bash
  source tooling/env.sh
  "$JAVA_HOME/bin/python" tooling/ghidra/bootstrap.py th18
  ```
  之后把 `zPlayer*/zEnemyData*/zBullet*` 套到对应全局 → 反编译即具名字段(同 th16 做法,见 `engine/player/th16/05` §0.5)。

## 怎么开干(给新会话)

1. 确认有 `local/th18.v1.00a/th18.exe`;没有先问用户。
2. `open_database`(`file_path=local/th18.v1.00a/th18.exe`,`database_id=th18`)→ 跑上面两个 import 脚本 → 得 729 名 + 157 结构体。
3. 读 [`port-plan.md`](port-plan.md):挑一个**共通子系统**(如自机生命/碰撞)→ 用 ExpHP th18 `zPlayerInner` 偏移定位 → **到 th18.exe 一手验证**行为是否与 TH16 同形 → 写进 `engine/<子系统>/th18/`。
4. 或挑**特有的卡牌/能力系统**(`zCard*`/`zAbility*`)从锚点新反。
5. **每条结论**都按证据链纪律落到 **th18 自己的地址 + 读写点 + 可信度**;**严禁把 TH16 地址/偏移写成 TH18 事实**。

## 纪律(本工作区红线)

- **TH16 的一切结论在这里都是"假设",未在 th18.exe 一手验证前不得当事实**(CLAUDE.md 明写"别外推到 TH15-19")。
- **地址/偏移一律重取**;引用 TH16 时显式标 "(TH16)" 以免混入。
- 主仓库不留版权字节:`samples/ ghidra projects/ exports/` gitignore。

## 目录

```
games/th18.v1.00a/
├── INDEX.md       # ← 你在这
├── port-plan.md   # ★ TH16 结论→TH18 待验假设 + 锚点 + "会变"警示
└── symbols.json   # ★ 我们自己那层符号(入库);往返见 tooling/ghidra/symbols.py

engine/card/th18/  # TH18 一手结论(卡牌 01~09 + OPEN-questions)
local/th18.v1.00a/ # th18.exe + 它自己的 Ghidra 工程(gitignored)
```
