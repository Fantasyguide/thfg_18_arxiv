# th18 动物灵 / 咆哮残骸审计 —— 四个部件，一个都没剩（但死槽一串）

> **版本**：TH18 v1.00a（`th18.exe`，md5 `9969cac756098c1da05a81de45437a70`，imagebase `0x400000`）。
> 本文裸地址默认属该版本；引用其他版本写成 `th17:0x…`。
> **方法**：ExpHP `th-re-data` 逐版本结构体/符号对账 + 一手穷举（TH18 的 funcs 729 / statics 154 全表、
> `zGlobalsInner` `0x4cccd0`–`0x4ccde0` 的逐地址引用普查、item type 分派、`Player::die` 路径）。
> **可信度**：TH18 侧的「有/无」✅ 一手；**「某东西不存在」给 🟡** —— 零符号 = 看不见，不等于没有槽。

## 0. 一句话

TH17 的动物灵系统在 TH18 **没有可复活的残骸**：管理器对象、token 链表、5 类计数、充能槽、
咆哮计时/标志、取消路径**六样全无**。
TH18 的第三条资源轴是**装备卡**（[`card/th18`](../../card/OVERVIEW.md)），死亡保护换成
**deathbomb + 卡钩子**（`0x45d590 Player::cancel_impending_death`）。
唯一像残骸的是 `zGlobalsInner` 里一串**只被初始化、无读者**的死槽（§2b），但**没有语义证据**认领。

## 1. 动物灵对象（`zGoastManager` / `zToken`）

TH17 一手（th-re-data 结构体 + 函数名）：

| 东西 | 形状 | 地址 / 证据 |
| --- | --- | --- |
| 管理器全局 | `GOAST_MANAGER_PTR` | `th17:0x4b7684` |
| `zGoastManager` | `anm_file(+0xc)` / `token_count(+0x10)` / `list_head(+0x14)`，0x54 字节 | th-re-data |
| `zToken` | `node(+0)` / `anm_id(+0x10)` / `token_type(+0x14)` / 两个 vec3 / 三个 timer / `flags(+0x6c)`，0x70 字节 | th-re-data |
| 行为 | `spawn_token`(`th17:0x410380`) / `on_tick`(`0x40e750`) / `on_draw`(`0x40f970`) / `do_hyper_cancel`(`0x40f6d0`) / `destroy_all`(`0x40e330`) / `TokenList::insert_at_back`(`0x4111a0`) | ExpHP 函数名 |

TH18 侧普查：

- `funcs.json`（729 条）里**没有**任何 `Goast*` / `Token*` / `Animal*` / `Hyper*` / `Roar*` 命名的函数；
- `statics.json`（154 条）里**没有** `GOAST_MANAGER_PTR` 这类全局指针（TH18 的管理器只有
  Ability / Bullet / Item / Enemy / Laser / Popup / Gui / Spellcard …）；
- item type 分派（`ItemManager__on_tick__body` `0x445a80`）里**没有**动物灵类型的 case
  （TH16/TH17 的「动物灵当道具」这条路在 TH18 也没有）。

→ **动物灵对象不存在** 🟡（「没找到」；要证死需要按结构体形状扫全 exe，见 §4 的待办）。

## 2. 收集槽与咆哮状态

### 2a. TH17 的字段（同一块 `zGlobalsInner`）

| 偏移 | 名字 | 用途 |
| --- | --- | --- |
| `+0x1c` | `GOAST` | 当前选的动物灵类型 |
| `+0x84` | `HYPER_FILL` | 咆哮充能槽 |
| `+0x88` | `TOKENS[5]` | 五类动物灵各自的收集数 |
| `+0xc8` | `HYPER_TIME` | 咆哮剩余时间（zTimer）|
| `+0xe8` | `HYPER_FLAGS` | 咆哮标志位 |

### 2b. TH18 的同一块（`inner` 基址 `0x4cccdc`，一手逐地址普查）

| 偏移 | 绝对地址 | TH18 现状 |
| --- | --- | --- |
| `+0x84` | `0x4ccd60` | **活**：`thread_start` 写 3（`0x442785`）、`Player__commit_death_and_enter_state2` 读（`0x45d1cb`）|
| `+0x88` | `0x4ccd64` | **活**：`MAX_BOMBS`（多处读写）|
| `+0x90` | `0x4ccd6c` | 死：只有两处 `add`（`0x42f033` / `0x42f435`），无读者（[`chapter/th18/01`](../../chapter/th18/01-chapter-residuals.md) §2b）|
| `+0x94` | `0x4ccd70` | 死：只有 `thread_start` 写 0 |
| `+0x98` | `0x4ccd74` | 零引用 |
| `+0xc8` | `0x4ccda4` | **死**：只有 `thread_start` 写 0（`0x44280f`）|
| `+0xe8` | `0x4ccdc4` | **死**：`thread_start` 写 0 后自己 `TEST` 一次（初始化门），无外部读者 |

同族死槽（都是 `thread_start` 里初始化、之后没人读）：
`0x4ccda0` / `0x4ccda8` / `0x4ccdac` / `0x4ccdb0` / `0x4ccdb4` / `0x4ccdb8` / `0x4ccdc0` /
`0x4ccdc4` / `0x4ccdc8` / `0x4ccdcc` / `0x4ccdd0` / `0x4ccdd4`。
其中 `0x4ccdbc` 是**活**的（`CardEirin::on_tick` `0x40a59d` / `CardShikiEiki::on_tick` `0x40dabd` 读），
`0x4ccdd8` 由 `MainMenu::on_tick` 读写。

> ⚠️ **别把偏移巧合当血缘**：TH17 的 `HYPER_TIME` 在 `+0xc8`、TH18 的 `+0xc8` 也是死槽 ——
> 但 TH18 的 `inner` 布局是 **TH15/TH16 血统**（`CURRENT_LIVES` 在 `+0x6c`、`MAX_BOMBS` 在 `+0x88`、
> `+0x90/+0x94/+0x98` 是 TH15 的每章计数器），而 TH17 的 `inner` 因为 `GOAST` 插在 `+0x1c`
> 整体位移过（`CURRENT_LIVES` 在 `+0x64`）。**两边的 `+0xc8` 不是同一个字段** 🟡。

## 3. 咆哮的「中弹只退出咆哮」

TH17：命中路径检查咆哮状态 → 走 `GoastManager::do_hyper_cancel`（`th17:0x40f6d0`），不扣命。

TH18：`Player::die` `0x45d3a0` 里**没有**这种分支（一手反编译：它直接播死亡音、
起死亡特效 VM（script 0x1d）、清符卡位、进死亡状态、`num_deathbomb_frames = 8`，
再沿卡链表调虚表 `+0x10`）。
但**通用机制活着**：

| 接缝 | 地址 | 说明 |
| --- | --- | --- |
| `Player::cancel_impending_death` | `0x45d590` | 把玩家拉回 state 1 + 60 帧无敌（`+0x638 = 0x3c`）—— 死亡取消的唯一入口 |
| 卡虚表 `+0x10` / `+0x0c` / `+0x14` | `on_death_before_deathbomb` / `on_death_after_deathbomb` / `on_death_frame2` | 卡在死亡各阶段介入 |
| deathbomb 本体 | `0x420360 do_bomb` + `+0x30` 门 | 引擎自己的「消耗炸弹换不死」 |

→ 「中弹只退出咆哮」可以**用卡复刻**（`on_death_before_deathbomb` 里检查咆哮槽 →
`cancel_impending_death` + 清咆哮），但它是**卡侧行为**，不是引擎残骸 ✅。

## 4. 想复刻的话 / 还没做完的证伪

| 事项 | 状态 |
| --- | --- |
| 按 `zGoastManager` 的形状（`+0xc` anm 指针 / `+0x10` 计数 / `+0x14` 链表头）扫 TH18 全 `.data` 找同形对象 | ❓ 没做 —— 这是把 §1 的 🟡 变 ✅/❌ 的唯一办法 |
| 按 `zToken` 的 stride 0x70 找 TH18 里的同形池 | ❓ 没做 |
| TH18 死槽（§2b）逐个找「曾经的名字」 | ❓ 没做 —— 需要跨版本字段对齐（TH16→TH17→TH18 的 `inner` 位移图）|
| 卡侧实现（动物灵掉落 + 收集槽 + 咆哮 + 死亡取消） | ✅ **Stage 2 已落地**（2026-09-10，id 117「鬼形兽」）|

**卡侧 Stage 2 详情**（2026-09-10）：`mods/th18.v1.00a/card-expand/native/cards/animal.c` + `animal_core.c`
（纯逻辑单测 116 断言）；动物灵实体 = 自建 VM 池 + TH17 原版切图（`beast1.png` 13 单元 / `beastf.png` 槽框 /
`beast.anm` script0–6 生成的 137–143），DVD 式弹跳 / 24 px 吸收 / 60 px 减速 / 变色 / 版边消失；
审计 `mods/th18.v1.00a/card-expand/AUDIT.md` §AL；实跑清单 `NEXT.md` §1v；
设计 `docs/superpowers/specs/2026-09-10-season-and-animal-design.md` §2。
**仍待实装**：暴走咆哮的射击强化、水獭消弹体视觉、稀有动物灵（用户定案不做）——见 `NEXT.md` §1v Stage 3。
