# 108（永夜抄）/ 112（星莲船）交接件 —— 谁接手都从这里读

> **版本**：TH18 v1.00a（`th18.exe`，imagebase `0x400000`）。本文裸地址默认属该版本；
> 引用其他版本一律写前缀（例 `th12:0x44ab00`、`th08:0x44e8d0`）。
> 写这份的原因：2026-09-23 那一轮里 108/112 的排查跨了很多批，线索散在 `SINGLE-CARD-TEST.md`
> 的几十行表格与 `AUDIT.md` §BE 里；这里只留**当前状态 + 下一步的两条分支**，细则仍指回原文件。

## 0. 当前部署面（单一来源 = `RELEASE-STATE.json`）

| 项 | 值 |
| --- | --- |
| DLL | 见 `RELEASE-STATE.json` 的 `last_release.dll_md5`（modkit 四处同 md5，用 `local/sync_modkit_dll.sh` 同步）|
| patch / sht / anm | 同文件快照；112 的 `ability.anm` 在 2026-09-23 因 **#17 毛笔横幅** 变过一次 |

## 1. 112 星莲船：已落地 / 已实跑验过

| 条 | 状态 | 判据（日志关键字）|
| --- | --- | --- |
| #21 槽带位置 | ✅ 已发版 + 实跑已验 | `ufo: HUD up … block 132x32 pinned at surface (248, 928) = the PLAYFIELD bottom-left corner, aligned to card 113's block` |
| #17 槽底毛笔横幅 | ✅ 已发版 + 实跑已验（VM 起来了）| `ufo: brush banner under the slots up (ability.anm script 315 <= front.anm script79, sprite67 …)` |
| #18 token 生成音效 | ✅ 已发版 | `ufo/se tally …` 里应有 `ufo-token-spawn` 计数 |
| #19 召唤清槽 | ✅ 不重现（同局无 `ufo: slots cleared`，而 tally 有 `ufo-fill-full 2 / ufo-break 2`）| — |
| #14 召唤紫环 + 雷电 | ❌ 未实现（一手方向已排除两处）| 见 §3 |
| #20 击破 ×N | ❌ 未实现（**一手规格已全部到手**）| 见 §2 |

## 2. #20 ×N 的实现规格（一手，照做即可）

- **创建点**：`th12:0x44ab00`–`0x44ab5f` —— `push $0x86` → `call 0x4615a0` → 存 `+0x40`（兄弟 VM = 脚本 134）；
  `push $0x88` → `call 0x4615a0` → 存 `+0x44`（**★ ×N = 脚本 136**）；`movl $0x2d0,0x28(%esi)` = **寿命 720 帧**。
- **脚本原文**（`local/th12.v1.00b/anm/front/front.anm.txt`）：
  `script136 { scriptNew(137); sprite(-1); drawRect(32,4); layer(10); color(255,96,96); pos(0,-28); stop(); 中断1=delete }`
  `script137 { sprite(-1); drawRect(32,4); layer(9);  color(0,0,0);     pos(1,1);   stop(); 中断1=delete }`
  ⇒ 亮红 + 黑、错开 = **描边式文字**（不是贴图）。
- **每帧更新**：`th12:0x44a80b`–`0x44a83b`（取 `[ebx+0x44]` → `call 0x461920` 查 VM，返回 0 就清句柄 →
  `orl $0x8,0x47c(%eax)` → `fstps 0x40(%eax)`）⇒ 文字内容与坐标由 C 每帧写。
- **我们的实现**：用 112 现有 HUD 字形通道拼「×」+ 数字（**不新造贴图**）、颜色 `(255,96,96)` + 黑描边错开 `(1,1)`、
  寿命 720 帧、生成时机 = 击破/结算那一刻、位置每帧写。

## 3. #14 紫环 + 雷电：已排除的两处 + 下一步的方法

- **已排除①**：召唤的 C 路径（`th12:0x44a8xx`–`0x44a97x`）里**只建一支 VM** = `push $0x52`（脚本 82 = 大横幅）。
- **已排除②**：不是靠 `color` 染的 —— 扫全部 TH12 anm dump，紫色系只有 `bullet.anm` 的纯品红 `(255,0,255)`
  ⇒ 紫环是**贴图**，得从 `sprite` 号切出来认。
- **2026-09-23 追加**：UFO **本体**贴图来自 TH12 **`enemy.anm` script135**（见 `assets/ability/scripts/164_ufo_red.anm.txt`
  的文件头：`make_ufo_art.py` 从 `enemy.anm` script135 生成）⇒ 下一轮找环/闪电时**别只看 `front.anm`**：
  优先查 `enemy.anm`（本体同族）与 `stgenm06.anm`；按 `rotateTime` 形状扫过一遍，命中的文件 = `bullet` / `enemy` / `front` / `pl01` / `stgenm06`（未定论，别据此下结论）。
- **方法（下一轮照走）**：`objdump -d local/th12.v1.00b/th12.exe > /tmp/th12.asm`，
  认 `call 0x4615a0`（anm 实例化）并取它前面最近的 `push $0xNN` ⇒ 全 exe **142 处**创建点。
  UFO/GUI 家族邻近候选：`0x421371→0x55`、`0x421619→0x51`、`0x4314e3/0x431593→0x51/0x52`、
  `0x436622→0x4d`、`0x438429→0x50`、`0x448ab1→0x63`、`0x448adf→0x75`。
  ⇒ 从 `0x42xxxx` 那两处开始，先认**调用者上下文**（「收令牌」还是「召唤」），再切它们的 `sprite` 认形状。

## 4. 108 永夜抄：子机那条（**当前唯一的大坑**）

**现象（owner）**：妖侧「本体只发一列（正常两列）、子机不发」；人侧「本体消失、定在那发弹」；
最新一轮又报「**依旧子机滞留**」。

**已证**（日志）：`ce: shot-set-index first claim … edi=0x… [player+0x47940]=0x…` ⇒ 我们认领的 set 号
**落在对的行表上**（前几轮怀疑的「set 进了别的表」排除）。

**机制（代码侧一手）**：主炮行循环按行里 `+0x20` 选膛口 —— `slot 0` = 自机本体，`slot ≥ 1` =
`PLAYER+0x580+slot*0xf0` = **引擎自己那 4 格原生子机**（`th18:0x45ec4c`–`0x45ec5d`；`engine.h` 的
`CE_NATIVE_OPT_*`），**不是**卡申请的池（`inner+0x410`）。而我们的 TH08 行按 TH08 的子机编号写（0..4）
⇒ `slot ≥ 1` 的行要的正是原生那 4 格 —— 可我们低速把它们藏了（`ce_native_option_hide_declare` +
每帧 `sweep` + 隐藏断点把 `lvl` 压 0）。

**两条互斥的修法（等一次实跑分流，别乱改）**：

| 分支 | 触发判据（日志）| 修法 |
| --- | --- | --- |
| A. **我们的旧 VM 没死** | `youkai: option release - N slot(s) held, VMs to delete: …` 里同一格 ptr 反复出现而 VM id 每次不同 | 修 `yk_option_release` / `ce_option_drop` 链（per-slot VM id 的删除与激光收尾）|
| B. **原生那 4 格回不来/不动** | `youkai: native option slots at HIGH speed - 0/4 live` 或四行 `muzzle` 恒定 | 修上批那个「隐藏→可见边沿补 `repopulate`」；或改成**低速不藏原生记录、只杀显示** |

**顺带要判的还有**（同一次实跑就能给）：`youkai: native option slots at low speed - N/4 live`
（低速那 4 格到底在不在）。

## 5. 108 其它：已实跑验过 / 只等眼判

| 条 | 状态 |
| --- | --- |
| #30 人妖槽看不见 | 资产侧已验（产物 `script49` = `layer(25) + pos(0,0,0)`）；owner 最新一轮报「**在画面上方被挡**」⇒ 这版加了 **HUD 起来时必打一次** 逐 VM 实况（`youkai/hud.vm … draw = pos+pos2+ent = 表面 (…)`），判「数值没生效」还是「画到别的 pass」|
| #26 决死弹幕变红 | 链已验（`youkai: deathbomb window - tinted N …`）；颜色已按 owner 口径改**纯红** `0xffff0000`，够不够红待眼判 |
| #29 / #32 shift 光环持续触发 | ✅ 已验（`body Shift aura ON` 每进一次妖侧恰好一行）|
| #33 / #38 松 Shift 后人侧子机消失/定住 | 已发版（边沿补 `repopulate` + 20 帧冷却）⇒ 若仍在，看 §4 的分支 B |
| #42 永点看不见 / 太大 | 已发版两次：先补尺寸、后按 owner「大了 3 倍」改回**不注入** `scale`（世界层本来就 ×2）⇒ 判据 `youkai/orb.vm … scale` 应打 **1.00** |
| #34 / #35 灵梦妖侧不发弹 + 子机不转 | 资产侧已验（`script5` = `layer(13)` + 真弹贴图；旋转 = #9 的 `th08:0x44e8d0` 弹簧 + 旋转标签 5，已改待实跑）⇒ 行为待 §4 |

## 6. 纪律（别忘）

每批：`make check` rc=0 + `python3 tooling/check-docs.py` 全绿 + `make dllx87` 0 条；
**不加新断点站点**（预算 31）；行为改动必须发版（DLL 四路径同 md5 + `RELEASE-STATE.json` 记账）并在
`SINGLE-CARD-TEST.md` 写**实跑验收关键字**；自定值进 `TUNING.md`，共享件按 `AUDIT.md` §BE 登记。
