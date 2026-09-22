# 02 · TH08 `enemy.anm` script 57/58（使魔的另外两套外观）：逐帧抄写 + `ins_25` 的一手判定

> **版本**：TH08 v1.00d（样本 `local/th08.v1.00d/dat/enemy.anm`，anm **version 3**；反编译 `local/vendor/th08_dec`）。
> 本文裸地址默认属该版本；引用其他版本须写成 `th18:0x…`。
> **方法**：一手 —— ① `local/vendor/thtk/build/thanm/thanm -l 8 -m local/vendor/thpages/static/mapfile/v2.anmm
> local/th08.v1.00d/dat/enemy.anm`（anmmap 必须 **v2**：文件头自述「v2: PCB / v3: IN, PoFV」）；
> ② `objdump -d` 逐指令读 `th08.exe`；③ `grep -nE '\[[a-z0-9]+\+0x1fc\]' local/_scratch/th08_full.asm` 全库普查 +
> `local/vendor/th08_dec/config/reccmp-functions.csv` 把每条命中归到函数名；④ ExpHP 结构体
> `local/vendor/th-re-data/data/{th08.v1.00d,th18.v1.00a}/type-structs-own.json`；
> ⑤ TH18 侧助记符表 `local/vendor/thpages/static/mapfile/v8.anmm`。
> **可信度**：脚本正文 / sprite 矩形 / handler 语义 ✅；「`vm+0x1fc` 在 anm 管线里只写不读」🟡（**否定结论**，
> 手段是字节扫描 + 函数归名，见 §6 的边界）；「TH18 无等价指令」✅（助记符表全量 + 布局差异）。
> **用途**：回答 owner 6⑥「缺少敌人的使魔的动画本身」里「另外两套外观能不能补」。前作见
> [`01-familiars-and-anm.md`](01-familiars-and-anm.md)（使魔是什么 / 每波怎么重建 / 53..56 的既有移植）。

## 0. 一句话结论

1. **57/58 是「4 帧一套」的那一族**：`sprite(101..104)` / `sprite(105..108)`，每张 **32×32**，
   帧间隔 **4 帧**（`+4:`），16 帧一轮（`jmp(offset12, 0)`）；两套都比 53..56 少一半帧数、但每帧停得久。
   同族还有 **59/60**（`sprite(109..112)` / `113..116`），**但只有 57/58 被使魔用过**（见 §2.3）。
2. **`ins_25(1)` 是「把参数原样写进 `zAnmVm+0x1fc`（int16）」**，一手 handler `th08:0x45f8d7` 三条指令；
   该字段在 ExpHP 结构体里就叫 **`__v3_ins_25`**（vendor 也不知道它干什么）。
3. **在 TH08 自己的 anm 管线里，这个字段只写不读**（🟡 否定结论）：全 exe `[reg+0x1fc]` 命中 35 处，
   归名后**只有一处落进 `AnmManager::ExecuteScript`**（= 那个写点本身），其余 34 处全在别的子系统
   （敌人 / 背景 / 弹 / 激光 / 自机）—— 同偏移不同结构体。
4. **TH18 的 v8 anm 没有对应指令、也没有对应槽位**（`v8.anmm` 全量助记符里没有任何「通用写 word 字段」
   的一支；TH18 的 `zAnmVmPrefix+0x1f8` 是 `scale_i`，布局已经不同）。
   ⇒ **可移植，做法 = 照抄 sprite/jmp 部分 + 把 `ins_25(1)` 删掉**（丢弃一次没人读的写）。
   ⚠ **不要**把它猜成 TH18 的 `rotateAuto(1)`（v8 确有 `424 rotateAuto`）——那是**加**一个我们无法证成
   的可见行为，与「照抄」相反。

## 1. 逐帧抄写（原生正文，一字未改）

`thanm -l` 原文（`/tmp/th08enemy.txt`，本机可复算）：

```
script script57 {              script script58 {
    ins_25(1);                     ins_25(1);
offset12:                      offset12:
    sprite(sprite101);             sprite(sprite105);
+4: // 4                           +4: // 4
    sprite(sprite102);             sprite(sprite106);
+4: // 8                           +4: // 8
    sprite(sprite103);             sprite(sprite107);
+4: // 12                          +4: // 12
    sprite(sprite104);             sprite(sprite108);
+4: // 16                          +4: // 16
    jmp(offset12, 0);              jmp(offset12, 0);
}                              }
```

| 项 | script57 | script58 |
| --- | --- | --- |
| 帧数 | 4 | 4 |
| sprite 号 | 101 / 102 / 103 / 104 | 105 / 106 / 107 / 108 |
| 每帧停 | **4 帧**（`+4:`）| **4 帧** |
| 一轮 | **16 帧**后回到 `offset12` | 16 帧 |
| 循环起点 | **`offset12`（= 第一条 `sprite`）**，⇒ `ins_25` **只在第一次进入时执行一次** | 同 |
| 可见性 / alpha / 混合 | 脚本内**一条都没有**（只有 `ins_25` + `sprite` + `jmp`）| 同 |

⇒ 移植面**极窄**：既不需要 alpha 曲线，也不需要 `interruptLabel`（对比 53..56 的 8 帧套同理）。
唯一「不可照抄」的只有第一行那条 `ins_25`。

## 2. sprite 与图集（一手）

### 2.1 图集

`enemy.anm` **只有一个 entry**：`entry0`，`name: "data/stgenm/enemy.png"`，`version: 3`，
`format: 5`（THTX），**512×512**，`colorKey: 0x00000000`，脚本总数 **101 支** ⇒ sprite 号是**该图集内的绝对号**。

### 2.2 这 8 张矩形（`thanm -l` 原文读数）

| sprite | x | y | w × h |
| --- | --- | --- | --- |
| 101 / 102 / 103 / 104 | 128 / 160 / 192 / 224 | **128** | 32 × 32 |
| 105 / 106 / 107 / 108 | 128 / 160 / 192 / 224 | **160** | 32 × 32 |

（对照：已移植的 53..56 用的是 **`y = 128/160/192/224`、`x = 256..480`** 那一列 8 张一套；
57/58 是**左半张图（`x = 128..255`）同一 y 行**的 4 张一套 —— 即「同一批贴图、另一半排布」。）

### 2.3 同族与使用率（一手：24 份 `ecldata*.ecl`）

- `ins_25` 的全库使用处 = **4 支脚本：57 / 58 / 59 / 60**（`grep -c 'ins_25('` = 4）。
  59 = `sprite(109..112)`（`y = 192`）、60 = `sprite(113..116)`（`y = 224`），形状与 57/58 逐条相同。
- 但**使魔只用到 57 与 58**：把每个 `familiarCreate*("SubN", …)` 追到 `SubN` 自己的 `anmSet(K)`，
  按调用点加权得 **53×347 / 57×165 / 54×162 / 56×48 / 58×46 / 55×22**（其余 K 是统计噪声）
  ⇒ **57/58 合计 211 处（≈21% 的使魔生成）**，而 59/60 **一次都没被使魔用过**。
  ⇒ 「补另外两套外观」= 补 **57/58** 这两支就够，不必做 59/60。

## 3. `ins_25` 到底是什么（一手）

### 3.1 handler（逐指令）

opcode 号由 anm v3 解释器分派：`th08:0x45ea76 movsx eax,WORD PTR [edx]`（取 raw opcode）→ `+1` 查表
`th08:0x46206d`；raw **25** → handler **`th08:0x45f8d7`**：

```
45f8d7  mov  ecx,[ebp+0x8]        ; ecx = zAnmVm*
45f8da  mov  edx,[ebp-0x4]        ; edx = 当前指令指针
45f8dd  mov  ax,[edx+0x8]         ; ax = 指令的第 1 个 16 位参数
45f8e1  mov  [ecx+0x1fc],ax       ; zAnmVm+0x1fc = 参数       ← 全部动作
45f8e8  jmp  0x461785             ; 收尾（同函数内，推进指令指针）
```

⇒ **语义 = 「把参数写进 VM 的一个 int16 字段」**，没有任何其它副作用（不建 VM、不播声音、不碰 flags）。
`zAnmVm+0x1fc` 的 vendor 名字正是 **`__v3_ins_25`**（`int16_t`；邻居是 `+0x1f8 flags`、
`+0x1fe pending_interrupt`）—— ExpHP 也只能按「写它的那条指令」给它命名。
thtk 的 `v2.anmm` 里 **raw 25 是空白行**（上一条 24 `posMode`、下一条 26 `scrollNowX`），
注明「sets a word field. Might be rotateAuto? Why a whole word?」—— 与我读到的指令**逐字吻合**。

### 3.2 ★ 谁读它？—— 全库普查：**只有那个写点落在 anm 管线里**（🟡 否定结论）

手段：`grep -nE '\[[a-z0-9]+\+0x1fc\]' local/_scratch/th08_full.asm` ⇒ **35 处**，逐条按
`config/reccmp-functions.csv` 归函数名：

| 命中处归属函数 | 处数 |
| --- | --- |
| `FUN_0042f5f0`（`EnemyData::step_game_logic` 一带）| 6 |
| `FUN_00424c40`（`EclExIns` 一带）| 5 |
| `th08::EnemyManager::OnDrawImpl` | 5 |
| `th08::EclExIns::ReisenFreezeBullets` / `Background::RenderObjects` / `EnemyManager::OnUpdate` / `AllocShotSlot` | 各 2 |
| 其余 9 个函数（背景 / ECL / 自机 / 弹 / 激光，各 1 处）| 各 1 —— 见下方清单 |
| **`th08::AnmManager::ExecuteScript`** | **1 —— 就是 §3.1 那个写点** |

「其余 9 个各 1 处」逐个点名（都是别的子系统里的同偏移字段）：
`th08::Background::UpdateStageTint`、`th08::EclManager::RunEcl`、`th08::RunLaserScript`、
`th08::EnemyManager::SpawnEnemy1`、`th08::EnemyManager::SpawnEnemy2`、
`th08::BulletManager::SetupLaserMove`、`th08::BulletManager::DrawSingleBullet`、
`th08::Player::UpdateBulletVms`、`th08::Player::DrawBulletVms`。

⇒ **anm 的更新 / 绘制路径里没有任何一处读 `zAnmVm+0x1fc`**；其余 34 处是**别的结构体**在同一偏移上的字段
（背景对象 / 敌人 / 弹 / 激光 / 自机 VM）。**推论**：`ins_25(N)` 对这些脚本是一次**死存（dead store）**，
删掉它在画面上等价。⚠ 这是**否定结论**，边界见 §6。

## 4. TH18 侧：有没有等价物（一手）

1. **助记符表层面 —— 没有**。`v8.anmm` 全量（`awk` 排序）里三组指令：
   `0..7`（`nop`/`delete`/`static`/`stop`/`stopHide`/`interruptLabel`/`wait`/`caseReturn`）、
   `100..131`（`iset`…`circlePosRand`）、`200..213`（`jmp`…`fjge`）、`300..315`（`sprite`…`colorizeChildren`）、
   `400..440`（`pos`/`rotate`/`scale`/`alpha`/`color`/…`rotateAuto`/`anchor`/`rotationMode`/…）、
   `500..509`、`600..613`（`drawRect` 一族）、`10000+`（变量与随机）。**没有任何一条是「把参数写进 VM 的某个通用字段」**。
2. **槽位层面 —— 也没有**。TH18 的 `zAnmVmPrefix+0x1f8` 是 `scale_i`（`struct zInterpFloat2`），
   与 TH08 的 `+0x1f8 flags` / `+0x1fc __v3_ins_25` 不是一回事 ⇒ **TH18 里不存在这个字段**，
   「照抄一条写它的指令」在目标版本里没有落点。
3. **疑似后继：`424 rotateAuto`**。thtk 当年就猜过 ins_25「Might be rotateAuto?」。
   🟡 但把 `ins_25(1)` 翻成 `rotateAuto(1)` 是**加一个行为**（自动旋转），而我们能证明的只有
   「TH08 里这个字段没人读」⇒ **不做这个替换**；要做得先单独查 TH18 `rotateAuto` 写哪一格、谁读
   （属于另一条一手工单）。

## 5. 结论：**能补**（可移植），代价如下

| 项 | 内容 | 依据 |
| --- | --- | --- |
| 脚本 | 新增 2 支 `ability.anm` 脚本（照 `206..209` 的范式：`assets/ability/scripts/` + `ORDER` 片段 + `make anm`），正文 = **§1 的 sprite/jmp 逐条 + 删掉 `ins_25(1)`** | §1 / §3.2 |
| 贴图 | 从 TH08 `enemy.anm` 切 **8 张 32×32**（sprite 101..108，矩形见 §2.2），走既有 `make_youkai_familiar_art.py` 的同一套流程（原生切图，不自己画）| §2.2 |
| 生成物 / 台账 | `ability.anm` 追加 ⇒ `anm_ids.h` 重生成、`CAPACITY.md` 的 entry 余量记账、`ORDER` 追加（都是既有流程，非新机制）| 既有工程 |
| 站点 | **0 条**（跟随 VM 是卡侧 VM，不经 ECL / 不经断点）| 既有实现 |
| 代码 | 若只是「多做两套素材」= 0 行代码；若要真的用上（例如给槽 0/1 各配一套），只需在 `yk_fam_anm_script()` 的 switch 里加两个分支（**这条要等实跑结论**：现在 53..56 是否看得见/位置对不对还没验）| 卡侧 |

**建议顺序**：先让 owner 实跑现在的 4 套（53..56 跟随 VM）→ 若「看不见 / 位置不对」先修层号或位置
（那是卡侧 1 行的事，比补素材重要得多）→ 确认播放链路 OK 之后，再决定要不要补 57/58 这两套**外观**。

## 6. 缺口（🟡 在哪、还缺什么证据）

1. **「只写不读」是否定结论**，手段是「按 `[reg+0x1fc]` 的字节模式全库扫 + 按函数归名」。
   它**覆盖不到**间接基址的读法（例如先把 `vm+0x1f0` 装进寄存器再读 `[reg+0xc]`）。
   要把它升到 ✅，需要：在 Ghidra 里给 `zAnmVm` 结构体做一次类型化 xref
   （或对 `0x1fc` 的所有 16 位访问逐条看**基址来源**）。**在此之前按「可删」处理是安全的**
   （删一条没人读的写 = 证明不了有影响；保留它则根本没有落点）。
2. 未查：TH18 `rotateAuto`（`424`）到底写哪一格、谁读 —— 只有做完这条，才能说
   「ins_25 与 rotateAuto 是不是同一个东西」（本文只说**不建议替换**）。
3. 本文只覆盖 `enemy.anm`（v1.00d）。TH08 的社区反编译记录的 sha256 与本机样本不同（另一个 1.00d 构建）
   ⇒ 若别的构建里 57/58 正文不同，须以本机样本为准（本文全部读自本机样本）。
