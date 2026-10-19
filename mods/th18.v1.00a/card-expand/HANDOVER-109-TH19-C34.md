# HANDOVER-109-TH19-C34.md — 卡 109 的 C3–C4（TH19 ECL）交接

> **版本**：TH19 v1.10c（`th19.exe`，imagebase `0x400000`，样本 md5 `dd0e1ff8c02327bf6408f763b8beedc9`，已脱壳）。
> 本文裸地址默认属该版；引别作写 `th18:0x…`。
> **一手**：`local/th19.v1.10c/_ext/ecl/pl0{1,2}.txt`（`thecl -d 19 -x -j`；TH19 无 eclmap 时即 `ins_NNN`）。

## 0. 一句话状态

★ **读完了、也对上了**：魔理沙/早苗 的 C3–C4 共 **451 行 ECL、51 种 opcode、233 次调用**，
51/51 有行（✅30／🟡20／❓1）；★ **owner 四条口述逐句对账完毕**（含两处裁定）；
★ 沉淀成 eclmap（32 条命名，43/43 归一化复核）＋ 4 篇 engine 文档；
★ **卡侧一行未改**（等 owner 授权才动 `native/`）。

## 1. 交付物地图（按"要查什么"读）

| 想查 | 读哪个 |
| --- | --- |
| 为什么这些代码是 C3–C4（归属） | [`HANDOFF-109-TH19-C34-ECL.md`](HANDOFF-109-TH19-C34-ECL.md) §1 |
| owner 原话 + 逐句对账 | 同上 §2.0／§2.1–§2.4 |
| 派发机制 + handler 表 + 每 opcode 结论 | 同上 §3.0／§3.0.1／§4（§4.5 是 20 条 🟡 的逐条状态） |
| 逐行注释的 ECL 原文 | [`HANDOFF-109-TH19-C34-ECL-LISTING.md`](HANDOFF-109-TH19-C34-ECL-LISTING.md)（566 行） |
| ★ **卡侧该怎么改** | [`HANDOFF-109-TH19-C34-IMPL.md`](HANDOFF-109-TH19-C34-IMPL.md)（四张卡各四块 ＋ §5 总账） |
| 弹型表（`602` 的 type 0–49） | [`engine/ecl/th19/02-bullet-types.md`](../../../engine/ecl/th19/02-bullet-types.md) |
| 第四世代对照表的比对 | [`engine/ecl/th19/03-thbwiki-4thgen-compare.md`](../../../engine/ecl/th19/03-thbwiki-4thgen-compare.md) |
| 旧交接（已被上面取代） | [`HANDOFF-109-TH19-ECL-REVERSE.md`](HANDOFF-109-TH19-ECL-REVERSE.md) §9 |

## 2. 已定案（一手）

| 项 | 结论 | 出处 |
| --- | --- | --- |
| 派发 | 解释器 `0x4fecc0`；`handler(op)=dword[0x50524c+4×byte[0x505560+(op−300)]]`，覆盖 300..1042；98..299 no-op；`this`=`enemy+0x1230` | `00-opcode-table.md` §1 |
| 取指层 | `EclRunContext::ecl_run` = `0x5687d0`；系统码 ≤`0x61` 走 `0x56b0d8`＋`0x56afbc`（71 项） | 同上 §1.1 |
| 三个结构体 | 弹型记录 `this+0x5ac`、偏移 `this+0x4e6c`、基准位置 `this+0x4f2c`（y 向下为正，`.z=(x>−990)` 哨兵） | `01-et-family-args.md` §1 |
| `602` | `(num, type, color)`；type 0–49，**38 = 激光段**、0 = 点弹 | `02-bullet-types.md` |
| `601` | 发射；出生点 = 基准 ＋ 偏移（`0x404010` = vec3 相加） | `01-…` §2 |
| `618`／`603` | 偏移向量（极坐标／笛卡尔，同字段） | 同上 |
| `604`／`605`／`606`／`607`／`608` | 角度(dir/dif)／速度上下限／way+layer／style／音效 | 同上 ＋ `03-…` §2 |
| `609`／`611` | 弹幕**变换**序列（mode 8 = 切向加速度；通道 1 = 并行；611 段号内部递增） | `03-…` §5 |
| `700`／`701`／`703` | **直线激光**：变激光(a/length/b/width)／预警线 a、变实 b、激光 c、消失 d／发预警线激光 | `03-…` §2 |
| `711` | **曲线激光**发弹（把段表搬进 spawn 描述符） | `01-…` §5 ＋ owner 裁定 |
| `569` | 与 **288×448、中心 (0,224)** 的活动区求交，两 float 写回输出 | `01-…` §2 |

## 3. ★ owner 的两条裁定（2026-10-18，最高优先）

1. ★ ★ **魔理沙 C3/C4 = 直线激光 ＋ 预警线**：「有预警线 40 帧 → 变实 这个是对的」
   ⇒ `701(40,10,30,10,0)` 就是 **预警 40 → 变实 10 → 实体 30 → 消失 10**。
2. ★ ★ **早苗 C3 = 曲线激光**：「按照表现来看，早苗C3确实是曲线激光」
   ⇒ `609` 的变换序列 = **曲线激光的路径/弯曲参数**（`mode 8` 的 `r` 弯曲率、`s` 自转速率），`711` 发弹。
   ★ 贴图是**横条**：`laser1.png` 第 5 行（256×16）。

## 4. 仍未定（交接重点）

| 项 | 状态 | 下一步 |
| --- | --- | :--- |
| 20 条 🟡（`302 306 307 313 316 401 404 405 412 500 502 504 505 513 516 523 604 616`＋`21`） | 写点全一手，**消费者未读** | 读 6 个消费者：`0x4cb3d0`／`0x4cb960`／`0x4f3910`／`0x4ee880`／`0x4f6730`／`0x4bb8a0`（详见 C34-ECL §4.5） |
| `602(0,0,4)` 的 `type 0`（点弹）与"激光条"的关系 | 🟡 | 若曲线激光的条另有来源 ⇒ 查曲线激光的绘制路径 |
| `bounced < 4` 的「4」 | 🟡 | 第四世代表里 `mode 64` = 触壁反弹（`a`=次数、`b`=6 位墙掩码）；魔理沙 C4 一手**没有** mode 64 ⇒ 需再核三组 `611` |
| ★ **早苗 C3/C4 的 y 方向** | ⚠ 待 owner 拍板 | 一手"从顶往下"（y≈0／y=96），卡侧两处做了**镜像**（`FIELD_H − …`）⇒ 与 owner 原话相反 |

## 5. 卡侧下一步（要 owner 点头才动 `native/`）

| 卡 | 改什么（细节见 IMPL） |
| --- | --- |
| 魔理沙 C3 | ① 档位改由 `pofv_c4_rank()` 驱动四张表（现固定取 1）；② 补「预警线 40 → 变实 10 → 30 → 消失 10」两段过渡 |
| 魔理沙 C4 | ① 每条激光在自己那台的时刻**现取**自机角（现在 `base` 只在激活时算一次）；② **改成 3 条激光**（owner 原话已定，`TUNING` 的 ❓ 可关） |
| 早苗 C3 | ① x 起点 **130→140**、x 步长改 **`%F`**（`130/$E` 归"段起始时间"）；② y 去掉 `+140`；③ **改用曲线激光 + 横条贴图** |
| 早苗 C4 | ① x 不跟自机（改"基准 0 ＋ 极坐标偏移 (192,θ)"）；② theta 改单一变量 ±1°/±2° 漂移；③ 加"不反弹"标记 |

## 6. 操作须知（会咬人的地方）

* ★ 工作目录：**`/home/nipporita/rl-109-sakuya`**（分支 `feat/p109-c34-refactor`）。主检出 `/home/nipporita/renkolab` 停在 main，别混。
* ★ **绝不** `git checkout/switch/restore/stash`（旧 ref 里 `local` 是 `120000`，一次 checkout 清一次 `local/`）。
* ★ 闸：`python3 tooling/check-docs.py`（=0 处）＋ `cd mods/th18.v1.00a/card-expand/native && make check`（rc 0）。两者都实测通过（506 文件）。
* ★ eclmap 自检：用 `tooling/eclmap/maps/th19.v1.10c.eclm` 重出 43 份转储，与 `_ext/ecl/` **归一化（名字→`FN`）后逐字相同**。
* ★ 动 Ghidra 先 `source tooling/env.sh`；工程在 `local/th19.v1.10c/ghidra_projects`。★ 但本轮 90% 的 handler 都是 **`objdump`** 读的，不需要 Ghidra。
* ★ 有用的中间产物（gitignored）：`local/th19-ecl-dispatch.txt`（196 个 handler 的 op→地址）、`local/thbwiki-4thgen-ref.tsv`（174 条 opcode 签名）。

## 7. 本会话提交（分支上，共 22 笔）

`87cc930b` 51 行收口表 ／ `ebeb70e9` 合并子代理 RE ／ `71b61c8a` eclmap ／ `6da29036` eclmap 扩到 32＋engine §4/§5 ／
`772ae3b5` 结构归属 ／ `82f93615` dif 分布 ／ `58b9da23` 矩阵 th19 列 ／ `47b25c01` A 组细节 ／
`4a690882`–`c079c585` IMPL 四张卡 ／ `bc37b3bf` §4.5 ／ `1bc30acb`＋`e6262021` 弹型表（602 定案）／
`db62193c` THBWiki 比对 ／ `0c1f332f`＋`9603fea8` 曲线激光两轮裁定。

★ 每条都注明「待 rebase 到 origin/main 后重跑闸」。
