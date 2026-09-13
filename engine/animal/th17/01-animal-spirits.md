# th17 动物灵 / 咆哮（HYPER）系统 —— 一手全链：掉落 → 5 格队列 → 霊撃 → 中弹不扣命

> **版本**：TH17 v1.00b（`th17.exe`，md5 `7a5b5bc1c231792098a28d8367928aaf`，747,520 B，imagebase `0x400000`）。
> 本文裸地址默认属该版本；引用其他版本写成 `th18:0x…`。
> **方法**：Ghidra 12.1.3 一手反编译（工程 `local/th17.v1.00b/ghidra_projects/th17.exe.gpr`，1967 函数 / 1299 有名）
> + `objdump -d -M intel` 全 exe 反汇编 + `thdat`/`thanm`/`thecl` 解包 `th17.dat`（`beast.anm` / `st*.ecl` / `trophy.txt`）。
> **可信度**：结构体形状、字段偏移、常量、调用链 ✅ 一手（附地址）；「某个值是给谁用的」这类设计意图 🟡；
> 未定位的写入者 ❓ 一律列进 §10。
>
> 本文把 [`animal/OVERVIEW`](../OVERVIEW.md) 的 th17 列与 [`animal/th18/01`](../th18/01-animal-residuals.md) §1 的
> ExpHP 断言逐条升级为一手；对账结果在 §9。

## 0. 一句话模型

```
ECL dropMain(33) / dropMain(30..32)  ──►  item type 30..33
        (boss/中boss 死亡、稀有动物条件)         │ ItemManager::spawn_item `0x434740`
                                              ▼  (type 16..32 → token)
                        GoastManager::spawn_token `0x410380`  (zToken 0x70 B，上限 40)
                                              │ 场上飘：beast.anm 脚本 0..13
                       24 px 内收集 ──► FUN_0040f980 `0x40f980`
                                              │ TOKENS[HYPER_FILL] = type; HYPER_FILL++
                       满 5 格 ──► FUN_0040fb60 `0x40fb60`（选咆哮类型 + 时长）
                                              │ HYPER_FLAGS |= 2
                       按炸弹键（input bit 1）──► do_hyper_cancel `0x40f6d0`(this,0)
                                              │ 40 帧霊撃 + 伤害源 + 60 帧无敌
                       中弹 ──► Player::die `0x449420`（state 4）
                                              │ 8 帧内若咆哮在 → do_hyper_cancel(this,1)
                                              ▼ + Player::cancel_impending_death `0x4495e0`
                                        回到 state 1：**不扣命**
```

> ⚠️ **「按 C」是误传**：th17 里咆哮与**炸弹同一个键**（输入掩码 bit `0x2`）。槽满/咆哮中炸弹被
> `player_is_trying_to_bomb` `0x411d40` 主动屏蔽（它要求 `HYPER_FLAGS & 6 == 0`），所以同一按键
> 在「咆哮就绪」时变成霊撃。见 §5.2。

## 1. 样本与地址对齐（本文的证据前提）

| 项 | 值 |
| --- | --- |
| 脱壳版 | `local/th17.v1.00b/th17.exe`，747,520 B，md5 `7a5b5bc1c231792098a28d8367928aaf`，PE 时间戳 2019-08-10 17:36:44 UTC |
| 原包装版留档 | `local/th17.v1.00b/th17.exe.steamstub`，888,976 B，md5 `69a3bedc29172ba8d33cf0d73afc3f9d` |
| 工程 | `local/th17.v1.00b/ghidra_projects/th17.exe.gpr`（函数 dump `local/th17.v1.00b/th17-funcs.json`）|

- ① 发现：`objdump -h` 给出 `.text` VMA `0x401000`（0x98eba B）、`.rdata` `0x49a000`、`.data` `0x4ad000`，与 imagebase `0x400000` 的 32 位 PE 一致。
- ② 推测：脱壳只重建 PE 容器，代码段字节应仍是原版的。
- ③ 验证：把 ExpHP `th-re-data` v1.00b 的 **781 个函数地址**与脱壳版 Ghidra 库的函数表逐一对齐 ——
  **780/781 地址精确落在同名函数入口**（唯一例外 `0x421120 EnemyData::ecl_run_over_300`，Ghidra 切分不同）；
  再对动物灵 13 个函数手工抽检序言：`spawn_token` `0x410380` = `55 8B EC 83 EC 0C 53 8B D9`（标准帧），
  `operator new` `0x40e420` = `55 8B EC 83 EC 08 56 57 6A 54`（`push 0x54` = 管理器 0x54 B），
  `TokenList::insert_at_back` `0x4111a0` = `55 8B EC 8B 41 04 85 C0`，13/13 都是合法入口。
- ④ 结论：**脱壳版与原版版代码段地址对齐** ✅（md5 不同只因容器重建，见 [`games/th17.v1.00b/INDEX.md`](../../../games/th17.v1.00b/INDEX.md)）。
- ⑤ 证据：`objdump -h/-d`、`local/th17.v1.00b/th17-funcs.json`、`local/vendor/th-re-data/data/th17.v1.00b/funcs.json`。

## 2. 对象模型（一手字段表）

### 2.1 全局与管理器

- 全局指针 `GOAST_MANAGER_PTR` = `0x4b7684` ✅（`0x4b7660`–`0x4b768c` 一串管理器指针，它在 `STAGE_PTR` `0x4b7680` 与 `BOMB_PTR` `0x4b7688` 之间）。
- 构造 = `GoastManager::operator new` `0x40e420`（**函数名是 ExpHP 的，实际是构造函数**）：
  `operator new(0x54)` → `memset(this,0,0x54)` → `AnmManager::preload_anm("beast.anm", 0x1e)` `0x474750` 存进 `+0xc`
  → 注册两个 `zUpdateFunc`：`on_tick__stub` `0x40f960`（优先级 `0x1f`）、`on_draw` `0x40f970`（优先级 `0x22`）
  → 复位 `HYPER_FILL`/`TOKENS[5]`/两个计时器/`HYPER_FLAGS`。失败路径打印 `0x4a13b0`（SJIS「ファイルが見つかりません」）。
- 调用者：`GameThread::thread_start`（`0x4306c0` 处 `call 0x40e420`）；销毁 `GameThread::thread_start__stub` `0x4310b3` / `Supervisor::on_registration` `0x44171f` 调 `operator delete` `0x40e640`。
- `GoastManager::on_draw` `0x40f970` = `mov eax,1; ret` —— **纯 stub，管理器自己不画东西**；token 的图形是全局 `AnmManager` 池里的 VM（`AnmManager::get_vm_with_id` `0x476470`）。
- `GoastManager::on_tick__stub` `0x40f960` = `push ecx; call 0x40e750; pop ecx; ret` —— 把 ecx 压栈当 `this` 传给 `on_tick`。

| 偏移 | 名字（本文） | 类型 / 语义 | 证据 |
| --- | --- | --- | --- |
| `+0x0` | `header` | 指针 → `{flags, on_tick UpdateFunc*, on_draw UpdateFunc*}` 🟡 | `0x40e487` `or [edi],2`；`0x40e4fe` `[edi+4]=uf` |
| `+0x4` | `on_tick_uf` | `zUpdateFunc*`（注册 `0x40f960`，优先级 0x1f） | `0x40e500` |
| `+0x8` | `on_draw_uf` | `zUpdateFunc*`（注册 `0x40f970`，优先级 0x22） | `0x40e578` |
| `+0xc` | `anm_file` | `zAnmLoaded*` = `beast.anm` | `0x40e4bb`；ExpHP 同名 |
| `+0x10` | `token_count` | 场上 token 数，上限 `0x28` = **40** | `0x410390` `cmp [ebx+0x10],0x28` |
| `+0x14` | `list_head` | `zTokenList`（entry/next/prev/seldom，0x10 B） | `0x40e493`–`0x40e4af` |
| `+0x18` | `list_head.next` | 链表首元素（遍历起点） | `0x40e790` `mov edi,[esi+0x18]` |
| `+0x24` | `gauge_vm` | 5 格咆哮槽 VM id（beast.anm 脚本 `0x1d`） | `0x40e568`；`FUN_0040dec0` `0x40df00` 区段 |
| `+0x28` | `ready_vm_a` | 咆哮就绪光效 A（脚本 `0x1e`） | `0x40fc6a` |
| `+0x2c` | `ready_vm_b` | 咆哮就绪光效 B（脚本 `0x1f`） | `0x40fc74` |
| `+0x30` | `aura_vm` | 自机光环（脚本 `0x20`） | `0x40fdc1` |
| `+0x34` | `strike_vm` | 霊撃 VM（脚本 `0x21`），`do_hyper_cancel` 建 | `0x40f895` |
| `+0x38` | `type_aura_vm` | 按咆哮类型的专属光环（脚本 `type+0x28`） | `0x40fc27` |
| `+0x3c` | `release_vm` | 咆哮超时释放动画（脚本 `0x2c`） | `0x40f494` |
| `+0x40` | `strike_damage_source` | 霊撃伤害源下标（`Player::create_damage_source` 返回值） | `0x40f8a4` |
| `+0x44`/`+0x48`/`+0x4c` | `otter_orb_vm[3]` | カワウソ模式 3 颗光球（脚本 `0x25`–`0x27`） | `FUN_00410030` `0x410163` |
| `+0x50` | `unknown_50` | 每帧自减；「自机有存活子弹」判定里一起读 ❓ | `0x40f0b6` 自减 / `0x40ef63` 读 |

### 2.2 `zToken`（0x70 B ✅）

`spawn_token` `0x410380` 开头 `operator new(0x70)` + `memset(_,0,0x70)`，`TokenList::insert_at_back` `0x4111a0` 挂到 `manager+0x14`。

| 偏移 | 名字 | 语义 | 证据 |
| --- | --- | --- | --- |
| `+0x0` | `node.entry` | 自指（`node.entry = &node`） | `0x4103ce` |
| `+0x4`/`+0x8`/`+0xc` | `node.next/prev/seldom` | 链表 | `0x4103d3`–`0x4103e1` |
| `+0x10` | `anm_id` | 全局 AnmManager 池里的 VM id | `0x41046f` |
| `+0x14` | `token_type` | **1..17**（见 §3.2 全表） | `0x410475` |
| `+0x18` | `pos` | `zFloat3`，spawn 时 = 传入位置 | `0x4104e5` |
| `+0x24` | `vel` | `zFloat3`，初始 = 单位圆 × 角度（§4.1） | `0x410570` |
| `+0x30` | `timer_age` | zTimer，出生起的帧数 | `0x410411` 起 |
| `+0x44` | `timer_offscreen` | zTimer，离屏帧数 | `0x410445` |
| `+0x58` | `timer_life` | zTimer，**寿命**（240 帧起，见 §4.3） | `0x41049f`–`0x4104ad` |
| `+0x6c` | `flags` | bit0 已被收集 / bit1 有寿命 / bit2 靠近自机 | `0x41047b` |

## 3. 动物灵从哪来

### 3.1 只有两条 spawn 通道 ✅

对 `spawn_token` `0x410380` 全 exe 调用点普查：**只有 2 个调用者**。

| 调用者 | 地址 | 说明 |
| --- | --- | --- |
| `ItemManager::spawn_item` | `0x4348b2` | item type **16..32** 被转成 token（`type-0xf`），item type 33 先随机成 30..32 |
| `GoastManager::sub_40e920` | `0x40eca9` 起 14 处 | 咆哮「超时释放」的爆散（§5.4） |

`ItemManager::spawn_item` `0x434740` 的三段分派（一手反编译）：
`param_1 == 0x21`（33）→ `rand%3 + 0x1e`；`9..14` → 特殊道具路径；`16..32` → **token 路径**；
其余（`<16` 或 `>32`）→ 普通道具路径（`param_1 == 0xf` 时改写为 `6`）。

→ **结论**：本作里动物灵是**道具的一种**（item type 16..33），不是「敌人死亡时引擎自动掉」。
敌人 ECL 全表普查（§3.3）里普通妖精只掉 item 1/2/3（火力/点），**没有** item 16..32。✅

### 3.2 item type ↔ token type 全表（一手推导 + 游戏内文本佐证）

token type = item type − 15。anm 脚本号 = `spawn_token` 内的分派结果。

| token | 名字 | beast.anm 脚本 | sprite 来源 | 依据 |
| --- | --- | --- | --- | --- |
| 1 / 15 | オオカミ霊（狼） | 0 | `beast1.png` sprite0-2 | `0x410472`；`trophy.txt`「霊夢A＝オオカミ霊」 |
| 2 / 16 | カワウソ霊（獺） | 1 | `beast1.png` sprite3-5 | 同上；`trophy.txt`「霊夢B＝カワウソ霊」 |
| 3 / 17 | オオワシ霊（鷲） | 2 | `beast1.png` sprite6-8 | 同上；`trophy.txt`「霊夢C＝オオワシ霊」 |
| 4 | 道具霊（炸弹碎片） | 3 | `beast1.png` sprite9 | 收集 → item 6（`0x410912`） |
| 5 | 道具霊（残机碎片） | 4 | `beast1.png` sprite10 | 收集 → item 4（`0x410940`） |
| 6 | 道具霊（火力） | 5 | `beast1.png` sprite11 | 收集 → 10× item 1（`0x4109db`） |
| 7 | 道具霊（点） | 6 | `beast1.png` sprite12 | 收集 → 10× item 2（`0x410a9b`） |
| 8 | 稀有：クラゲ（水母） | 7 | `beast2.png` | `trophy.txt` @33；`award_trophy(type+0x19)` `0x410525` |
| 9 | 稀有：牛 | 8 | `beast2b.png` | `trophy.txt` @34 |
| 10 | 稀有：ひよこ | 9 | `beast2c.png` | `trophy.txt` @35 |
| 11 | 稀有：亀 | 10 | `beast2d.png` | `trophy.txt` @36 |
| 12 | 稀有：埴輪 | 11 | `beast2e.png` | `trophy.txt` @37 |
| 13 | 稀有：埴輪（馬） | 12 | `beast2f.png` | `trophy.txt` @38 |
| 14 | 稀有：三匹のひよこ | 13 | `beast2g.png` | `trophy.txt` @39 |

> 「1/15」同一脚本：`spawn_token` 对 `param_2 = 15..17` 走「长寿命」分支（`token_type = param-0xe` = 1..3、
> 置 flags bit1、寿命 240 帧），对 `param_2 = 1..14` 走另一分支（`token_type = param`、无寿命）。
> **15..17 与 1..3 是同一只动物灵的两个变体** ✅（`0x41045e` 的 `ja` 分叉）。

### 3.3 ECL 掉落点（一手：`thecl -d 17` 全 stage ECL + `default.ecl` 宏展开）

ECL 掉落指令 = `dropMain`(510) / `dropExtra`(507) / `dropItems`(509) / `dropArea`(508) / `dropClear`(506)；
`dropExtra(type,count)` 写 `EnemyDrop.count[type]`（`0x41d840`），`dropItems` 一次性弹出并清空（`0x41d880`）。

| 掉落 | 出现在 | 说明 |
| --- | --- | --- |
| `dropMain(33)` | 各面 boss 的血条相位/符卡结束（st01bs..st07bs，共 39 处） | **随机一只 オオカミ/カワウソ/オオワシ** |
| `dropMain(30)` / `(32)` | st03bs / st04bs / st05bs / st06bs 的 `BossCard2`/`BossCard3` | 指定 狼 / 鷲 |
| `dropMain(16+$A)`（$A=rand%3） | st06bs 的 `BossCard6/7`（Keiki） | 随机 狼/獺/鷲 |
| `dropMain(19)` / `(20)` | 每面 boss 与中 boss 死亡（`BossItem(19/20,...)`、st01mbs 等） | token 4（炸弹碎片灵）/ token 5（残机碎片灵） |
| `dropMain(22)` | 多面 `BossCard2` | token 7（10×点道具） |
| `dropMain(23..29)` | **各面中 boss 的稀有动物条件分支** | token 8..14，逐面一只 |
| `dropMain(1)/(2)/(3)`、`dropExtra(1..3,n)` | 普通妖精 | 火力/点/大火力道具，**不是动物灵** |

稀有动物条件（一手 = 中 boss 死亡子程序里的 `dropMain(23..29)` + 游戏内 trophy 提示）：

| 面 | 稀有动物 | ECL 落点 | trophy 提示（游戏内原文） |
| --- | --- | --- | --- |
| 1 | クラゲ | `st01mbs` `ins_510(23)` | 「中ボスの出す幽霊を一つも倒さずに、撃破しよう」 |
| 2 | 牛 | `st02mbs` `@BossItemPhase(30,10,24,48)` | 「ある条件下で中ボスを倒すと出るぞ」 |
| 3 | ひよこ | `st03mbs` `@BossItemPhase(30,20,25,48)` | 「ロアリングのカワウソモード発動中に倒そう」 |
| 4 | 亀 | `st04mbs` `@BossItemPhase(30,20,26,48)` | 「動物霊を取らずに、画面内に溜めた状態で倒そう！」 |
| 5 | 埴輪 | `st05mbs` `@BossItemPhase(30,20,27,48)` | 「ヒント：動物霊を従えて……」 |
| 6 | 埴輪（馬） | `st06mbs` `@BossItemPhase(30,20,28,48)` | 「霊撃を使って倒そう！」 |
| Extra | 三匹のひよこ | `st07mbs` `@BossItemPhase(50,50,29,48)` | 「ある条件下で中ボスを倒すと出るぞ」 |

> 稀有动物 trophy 在 **spawn 时**发放（`0x410525` `TrophyNotice::award_trophy(type+0x19)`，type 8..14 → trophy 33..39），
> 与 trophy 文案「発見しよう」一致 ✅。

## 4. 运动、收集与寿命（一手数值）

### 4.1 出场散射 `GoastManager::sub_410570` `0x410570`

`cartesian_from_polar_4113c0(token+0x24, angle, 1.0)` 把初速设为单位圆；随后遍历全部 token，
若与某 token 的距离² < **576.0**（24 px）且两者速度点积 ≥ **0.96**（同向），就把角度 +**3.883222 rad**（≈222.5°）重试，
最多 10 次。→ 作用 = **避免同向重叠** ✅。

### 4.2 每帧 `GoastManager::sub_410af0` `0x410af0`（返回 1 = 该 token 出列）

| 数值 | 值 | 常量地址 / 写点 |
| --- | --- | --- |
| 收集半径 | **24 px**（距离² < 576.0） | `0x4a3db0`；`0x410b64` 比较 |
| 减速半径 | **60 px**（距离² ≤ 3600.0 时速度 ×0.5） | `0x4a3dd4` / `0x4a3b48`；`0x410bec` |
| 吸收速度 | 每帧 `vel = (player − token) × 0.25` | `0x4a3b0c`；`0x4110bd` |
| 吸收后存活 | `timer_age.current > 9` 即出列（约 10 帧） | `0x410af0` 吸收分支 |
| 反弹边界 | `x ≤ −180` / `x ≥ 180` / `y ≤ 128` / `y ≥ 384` 翻符号 | `0x4a3e8c` / `0x4a3d48` / `0x4a3d38` / `0x4a3d88` |
| 速度下限 | `|vel| < 0.2` 时重掷 `±(0.5 + rand×0.3)` | `0x4a3b04` / `0x4a3b48` |
| 离屏判定 | `|x| > 208` 或 `|y − 224| > 240` | `0x4a3d58` / `0x4a3d64` |
| 离屏出列 | 离屏帧数 > **119** 或 年龄 > **129** 帧 | `0x410d36` 区段 |
| 年龄上限 | `timer_age.current < 8400` 才继续反弹 | `0x410af0` 后段 |
| 年龄 7800 帧 | `AnmManager::interrupt(vm, 9)`（变暗） | `0x476590` |

### 4.3 寿命与「变形」✅

`timer_life`（`+0x58`）在 spawn 时 = **240 帧**（prev 239 / cur 240 / cur_f 240.0）。

- 离自机 > 60 px 时每帧递减；
- 进入 60 px 内时**冻结**，且若剩余 < 60 帧则重置为 **90 帧**，并置 flags bit2；
- 归零时：**token_type = `type % 3 + 1`**（1→2→3→1，即 狼→獺→鷲 循环），删掉旧 VM、用新脚本重建，
  寿命重置为 **180 帧**，播 se `0x4e`。

→ **场上动物灵会自己换种类**，靠近时冻结 —— 与「稀有动物提示：取らずに溜める」互为印证 🟡（意图）。

### 4.4 没有「PoC 自动收集」✅（负结论）

- 全 exe 里「收进队列」的函数 `FUN_0040f980` `0x40f980` 只有 2 个调用点：token 的 24 px 判定
  （`0x410b81`）与咆哮结束时的溢出队列回灌（`0x40f5b9`）。
- token 的 `flags`（`+0x6c`）只有 spawn 与 `sub_410af0` 自己写（`0x410481` / `0x410b86` / `0x410c35` / `0x410c88`），
  没有任何外部系统能把 token 标记为「被收」。
- → **TH17 的动物灵没有 item 那种「到版顶自动全收」的通道**，只有 24 px 吸收 ✅（与 item 的 PoC 是两套）。

## 5. 咆哮（ロアリング / HYPER）

### 5.1 字段（`zGlobalsInner`，inner 基址 `0x4b59dc`）✅

| 偏移 | 绝对地址 | 本文名字 | 语义 |
| --- | --- | --- | --- |
| `+0x1c` | `0x4b59f8` | `GOAST` | 选择的自机兽灵 0..2（狼/獺/鷲）；`FUN_0045b9b0` `0x45bc17` 由「9 选 1」索引算出 |
| `+0x84` | `0x4b5a60` | `HYPER_FILL` | **队列已收数 0..5**（不是充能槽百分比） |
| `+0x88` | `0x4b5a64` | `TOKENS[5]` | **5 格队列：存收集到的 token_type**（不是 5 类计数） |
| `+0x9c` | `0x4b5a78` | `pending_count` | 霊撃中收集的溢出队列计数（0..5） |
| `+0xa0` | `0x4b5a7c` | `pending[5]` | 溢出队列本体 |
| `+0xb4` | `0x4b5a90` | `release_timer` | 释放爆散延时（`>= 0x78` = 120 帧才触发，`0x40ea85`） |
| `+0xc8` | `0x4b5aa4` | `HYPER_TIME` | 咆哮剩余帧（zTimer） |
| `+0xdc` | `0x4b5ab8` | `hyper_duration` | 本次咆哮总时长（帧） |
| `+0xe0` | `0x4b5abc` | `hyper_type` | 咆哮类型 1/2/3 = 狼/獺/鷲，4 = 混合 |
| `+0xe4` | `0x4b5ac0` | `hyper_extend` | 每多收一颗 +180 帧（`0x40fc99` 写 0xb4） |
| `+0xe8` | `0x4b5ac4` | `HYPER_FLAGS` | 状态位，见下表 |

`HYPER_FLAGS` 位表（一手：全部写/读点已普查）：

| 位 | 值 | 含义 | 证据 |
| --- | --- | --- | --- |
| 0 | `0x001` | HUD 槽可见闩锁（自机在 `x < −64 && y > 400` 时置位，`y < 384` 或 `x > −64` 复位） | `0x40e85f`–`0x40e8e0` |
| 1 | `0x002` | **咆哮就绪**（满 5 格） | `0x40fc8a` `or 2` |
| 2 | `0x004` | **霊撃进行中** | `0x40f844` `or 4`；ECL 全局 100 读它 |
| 3 | `0x008` | 本次霊撃是「中弹取消」触发（`param_1=1`） | `0x40f835` `(param_1<<3)^…&8` |
| 4..7 | `0x0f0` | **释放等级**（见 §5.4） | `0x40ff5e` 清、`0x40ff64` 起置 |
| 8 | `0x100` | 暂停（对话中） | `0x40f09e` |
| 9 | `0x200` | token 全清（`destroy_all` `0x40e3f6`） | `0x40eea2` |
| 10 | `0x400` | 自机有存活子弹（每帧重算） | `0x40ef75` |

### 5.2 充能与激活

- **收集** `FUN_0040f980` `0x40f980`（token 进 24 px 时由 `0x410b81` 调）：
  `TOKENS[HYPER_FILL] = type; HYPER_FILL++`，更新 HUD 第 `i` 格（`FUN_00410200`）；到 5 立刻调 `FUN_0040fb60`。
- **就绪** `FUN_0040fb60` `0x40fb60`：统计队列里 type 1/2/3 的出现次数。
  某类 ≥3 → `hyper_type = 该类`、`duration = 表[0x4a13c4 + count*4]`（**count 3→840、4→1140、5→1500** 帧），
  若该类 == `GOAST` 再 **+480** 帧；都不足 3 → `hyper_type = 4`（混合）、`duration = 1140`。
  然后 `HYPER_FLAGS |= 2`、建就绪/光环 VM（脚本 `0x1e`/`0x1f`/`0x20`/`type+0x28`）、播 se `0x4d`，
  并把释放等级写进 `HYPER_FLAGS` 高半字节（取队列里最后一个 type 8..14 的 `type−7`）。
- **激活**：`sub_40ee70_beast_related` `0x40ee70` 里 `if (INPUT_STRUCT.input & 2) do_hyper_cancel(this,0)`
  （`0x40f37c`–`0x40f389`）。**输入掩码 bit 1 = 炸弹键**（`Player::on_tick` `0x447b4a` 用它的上升沿调 `do_bomb` `0x411c30`）；
  槽满时 `player_is_trying_to_bomb` `0x411d40` 因 `HYPER_FLAGS & 6 != 0` 返回 0，于是**同一按键变成霊撃** ✅。
- **计时**：就绪期间 `HYPER_TIME` 每帧 −1（对话中暂停）；若自机有存活子弹则**再 −1**（`0x40ef75`）；收集新 token 时 **+180**（上限回 clamp 到 duration）。
- ★ **「咆哮模式」是满 5 格的就绪那一刻就自动成立的**（用户 2026-09-11 给的官方说明口径，与本文件
  §4 的索引公式一致）：同一刻 `HYPER_FLAGS` bit1 置位，**兽灵专属效果当场生效** —— 狼 / 鹫的
  `Player__do_shooting` 索引公式（§4）读 bit1 ⇒ **主炮组立刻换成 set10..14 / 15..19**（set10/15 空档
  只对应火力档 0，正常游玩取不到）、獭的 3 颗光球起 VM；就绪条（`0x1e/0x1f`）显示的就是咆哮的剩余时间。
  输入位 `0x2`（炸弹键）是**另一件事**：把已经在跑的咆哮**用掉**成 40 帧霊撃（`do_hyper_cancel`）。
  ——别把 bit1 / bit2 读成「先就绪、按键才开始咆哮」。

### 5.3 霊撃 `GoastManager::do_hyper_cancel` `0x40f6d0`（一手）

```c
do_hyper_cancel(this, int by_hit) {
    // 在自机位置建脚本 0x21 的 VM → this+0x34；播 se 0x2c
    // 清就绪/光环 VM（脚本 0x1e/0x1f/0x20/0x28/0x38；hyper_type==2 时清 3 颗光球）
    HYPER_FLAGS = (by_hit<<3 ^ flags) & 8 ^ flags | 4;
    HYPER_TIME = 40 帧 (prev 39 / cur 40 / cur_f 40.0);
    this+0x40 = Player::create_damage_source(&player->inner, 0x28 /*伤害 40*/, 5);
    hyper_type == 3 → player->flags |= 0x40;
    // 符卡：by_hit==0 且符卡中 → 按剩余时间 60 帧门槛决定是否清 bonus
    player->inner.iframes = 60 帧;
    ENEMY_MANAGER->bomb_count++（+0x??）; can_still_capture_spell = 0;
}
```

霊撃期间（`HYPER_FLAGS & 4`）每帧（`0x40f61a` 起）：

- `f = HYPER_TIME.current_f / 40`，伤害源半径字段 `+0x4 = (1 − f⁴) × 200.0` → **0 → 200 px 扩张**；
- `BulletManager::cancel_radius_as_bomb(伤害源+0x1c, ~(HYPER_FLAGS>>3)&1)` + `LaserManager::cancel_in_radius(...)`
  → 半径内子弹/激光被取消（取消会掉 item 9，见 `gen_items_from_et_cancel` `0x419ce0`）；
- `hyper_type == 2`（カワウソ）时，3 颗光球各建一个伤害 2 的伤害源并做同样的取消（`0x40f3a1` 区段）。

### 5.4 结束与「超时释放」

`HYPER_TIME.current <= 0` 时（`0x40f394` `cmp/jg`）：

1. 若**霊撃没发生**（bit2 = 0）→ 在 2× 自机位置建脚本 `0x2c` 的 VM 存进 `this+0x3c`，复位 `release_timer`；
2. `FUN_00410640` `0x410640`：把队列里残留的 token 4..7 换成道具（4→item 6、5→item 4、6→10×item 1、7→10×item 2）；
3. `HYPER_FLAGS &= ~6`、`HYPER_FILL = 0`、`TOKENS[0..4] = 0`、逐格刷 HUD；
4. 把霊撃期间攒的 `pending[5]` 逐颗重新塞回队列。

`release_timer` 到 **120 帧**（`0x78`）时 `sub_40e920` `0x40e920` 在 **(自机.x, 自机.y − 64)** 爆散动物灵，播 se `0x3e`：

| 释放等级（FLAGS>>4&0xf） | 生成 |
| --- | --- |
| 0 | 2 颗（随机 15..17） |
| 2 | 5 颗：2×随机 + type 5 + type 4 + 1×随机 |
| 3 | 5 颗：type 6 + type 7 + type 5 + type 4 + type 4 |
| 其他（1,4..7） | 5 颗：2×随机 + type 5 + 2×随机 |

> 游戏内 trophy 原文：「効果は、ロアリング終了時に動物霊を５つ放出」（水母）。
> 一手代码里这条只在**咆哮超时未用霊撃**时成立（bit2=0 才建 `0x3c`）——两者按字面有出入 🟡。

## 6. 「中弹只退咆哮，不扣命」✅（完整调用链）

1. 子弹/敌人命中 → `Player::die` `0x449420`：播死亡音、建 effect 脚本 `0x1d`、**`inner.state = 4`**（`0x449564`）、复位 `time_in_state`。
2. `Player::on_tick` 的 state 4 分支 `0x447b64`：`if (time_in_state < 8)` →
   `if ((HYPER_FLAGS & 2) && !(HYPER_FLAGS & 4)) { do_hyper_cancel(GOAST_MANAGER_PTR, 1); Player::cancel_impending_death(); }`（`0x447bf8`）。
3. `Player::cancel_impending_death` `0x4495e0`：`time_in_state = 0x3c`、**`state = 1`**、复位无敌计时器。
4. 若 8 帧内没有咆哮 → 走 `FUN_004491b0` `0x4491b0`：`CURRENT_LIVES--`、`state = 2`、`miss_count++`；
   它还会在「咆哮就绪」时把 `HYPER_TIME` 打成 1（**死一次咆哮槽清空**）。

→ 「用咆哮换一次不扣命」是**引擎内建**，且 `HYPER_FLAGS` bit3 记下「这次霊撃是被中弹取消的」（§5.3）。
与 TH18 侧只有 `Player::cancel_impending_death` + 卡钩子的现状对照见 §9。

## 7. HUD / UI（一手：`beast.anm` 脚本号 + 引擎写点）

| 元素 | anm | 脚本 | 引擎写点 |
| --- | --- | --- | --- |
| 5 格咆哮槽（主 VM） | `beast.anm` entry8 `beastf.png` | `0x1d`(29) | `FUN_0040dec0` `0x40df00` 建，存 `manager+0x24` |
| 槽位子 VM（5 个空框层） | 同上 | `0xe`–`0x12`(14–18) | `FUN_00410200` `0x410200` 写 `vm+0x490` = 4/5 |
| 槽位子 VM（5 个图标层） | 同上 | `0x18`–`0x1c`(24–28) | 同上；有货时 `vm+0x490 = TOKENS[slot]+6` |
| 就绪光效 A/B | 同上 | `0x1e`(30) / `0x1f`(31) | `FUN_0040fb60` `0x40fc6a` / `0x40fc74` |
| 自机光环 | 同上 | `0x20`(32) | `0x40fdc1` |
| 霊撃特效 | entry9 `beastr.png` | `0x21`(33) | `do_hyper_cancel` `0x40f895` |
| カワウソ 3 光球 | entry10 `beastot.png` | `0x25`–`0x27`(37–39) | `FUN_00410030` `0x410163` |
| 类型专属光环 | entry11 `beastAURA.png` | `type+0x28` = `0x29`/`0x2a`/`0x2b` | `0x40fc27` |
| 超时释放动画 | entry12 `beastap.png` | `0x2c`(44) | `0x40f494` |

- 槽位图标层用 `AnmManager::interrupt(vm, id)`（`FUN_00476590` `0x476590`）切到 `TOKENS[slot]+6` 号脚本，
  **有货 = 4 + type+6，空 = 5**（`0x410200` 全函数）。
- 槽的**缩放**驱动量：`vm+0x70 = hyper_duration×0.125 + 2.0`、`vm+0x70 = HYPER_TIME.current_f×0.125`
  （`FUN_00410180` `0x410180`，每帧刷）。
- HUD 可见性由 §5.1 的 bit0 闩锁控制：`AnmManager::interrupt_tree(manager+0x24, 3)` 显示 / `2` 隐藏（`0x40e894`/`0x40e8ce`）。
- token 自身动画（`beast.anm` 脚本 0..13 的 `interruptLabel`）与引擎调用对应：
  `1` = 被吸收（scaleTime + delete）、`2`/`3` = 待机/离开、`7` = 定格帧、`8` = 靠近自机、`9` = 7800 帧后变暗。

## 8. 跨系统接口

| 接口 | 位置 | 说明 |
| --- | --- | --- |
| ECL 全局 99（`0x63`） | `0x427b0e` | 读 `GoastManager+0x10` = **场上 token 数** |
| ECL 全局 100（`0x64`） | `0x427b1c` | `(HYPER_FLAGS>>2)&1` = **霊撃是否进行中** |
| item type 30/31/32 | `0x4348ad` | 指定 狼/獺/鷲 |
| item type 33 | `0x43474e` | `rand%3+30` = 随机兽灵 |
| `player_is_trying_to_bomb` | `0x411d40` | 咆哮就绪/进行中时**禁止炸弹** |
| `GoastManager::destroy_all` | `0x40e330` | 清空 token、复位链表、`HYPER_FLAGS |= 0x200` |

## 9. 对账：社区/既有文档断言 → 一手判定

| 既有断言（出处） | 判定 | 一手证据 |
| --- | --- | --- |
| 管理器全局 `GOAST_MANAGER_PTR` `0x4b7684`（[OVERVIEW](../OVERVIEW.md)、[th18/01](../th18/01-animal-residuals.md) §1） | ✅ | 构造 `0x40e48d` 写入；15 处引用全普查 |
| `zGoastManager` 0x54 B、`anm_file+0xc`/`token_count+0x10`/`list_head+0x14`（同上） | ✅ | `0x40e428` `push 0x54`；`0x40e4bb`/`0x40e4fe`/`0x40e493` |
| `zToken` 0x70 B、`node+0`/`anm_id+0x10`/`token_type+0x14`/2×vec3/3×timer/`flags+0x6c`（同上） | ✅ | `0x4103a2` `push 0x70`；§2.2 表 |
| `spawn_token` `0x410380` / `on_tick` `0x40e750` / `do_hyper_cancel` `0x40f6d0` / `destroy_all` `0x40e330` / `insert_at_back` `0x4111a0` | ✅ | 序言与语义逐一对上 |
| `on_draw` `0x40f970`（ExpHP 名） | ✅ 但**是 stub** | `mov eax,1; ret`，管理器不绘制 |
| 「有 5 类动物灵计数 `TOKENS[5]`」（OVERVIEW 第 3 行） | ❌ **证伪** | `0x40f9ab` `TOKENS[HYPER_FILL] = type`：是 5 格**队列** |
| 「有咆哮充能槽 `HYPER_FILL`」（OVERVIEW） | ✅ 但语义修正 | `+0x84` = 队列已收数 0..5，不是 0..100 的槽 |
| `HYPER_TIME` `+0xc8` / `HYPER_FLAGS` `+0xe8`（th18/01 §2a） | ✅ | 构造复位 `0x40e60c`/`0x40e62a`；§5.1 位表 |
| `GOAST` `+0x1c` = 当前兽灵（th18/01 §2a） | ✅ | `FUN_0040fb60` `0x40fbe5` 比它加 480 帧 |
| 「中弹只退出咆哮（不扣命）」`do_hyper_cancel` `0x40f6d0`（OVERVIEW） | ✅ | §6 的 `0x447bf8` → `0x4495e0` 全链 |
| 「咆哮 = 按 C」（任务书/社区口径） | ❌ 证伪 | 触发位是输入掩码 bit `0x2`＝炸弹键（`0x40f37c`），炸弹被 `0x411d40` 屏蔽 |
| TH17 `inner` 因 `GOAST` 插在 `+0x1c` 而整体位移（th18/01 §2b 注） | ✅ | `CURRENT_LIVES` 在 `+0x64`、`CURRENT_BOMBS` 在 `+0x70`（ExpHP 结构体 + 本文件引用点） |

## 10. 未答 ❓

> 动手清单（每条配「下一步建议」+ 复现命令）在 [`games/th17.v1.00b/HANDOVER.md`](../../../games/th17.v1.00b/HANDOVER.md) §4；本表是原始记录。

| 事项 | 状态 |
| --- | --- |
| 8..14 号稀有动物「被收进队列后到底影响什么」的完整设计意图 | 🟡 代码面已解（改释放等级），但「为何 2/3 档特殊、其余 1/4..7 同档」无文档佐证 |
| `goast+0x50` 的写入者 | ❓ 只找到自减（`0x40f0b6`）与读取（`0x40ef63`）；全 exe 按位移扫 `+0x50` 只命中 token 侧的 offscreen 计时器（`0x410d79`/`0x410da2`，基址是 `zToken`）|
| `Player::create_damage_source` 第三参（伤害源的 `+0xf188`）语义 | ❓ 霊撃与カワウソ光球都传 5 |
| 伤害源 `+0x4`（逐帧写半径）字段名 | ❓ ExpHP 的 `zPlayerDamageSource` 无字段名 |
| 稀有动物掉落条件里的 ECL 变量 `[-9926]` 具体含义 | ❓ 只见 `unless ([-9926]==0) goto skip` |
| 输入掩码 bit→虚拟键的完整表 | 🟡 已坐实 bit0=射击、bit1=炸弹/霊撃、bit3=低速（用法侧）；`read_keyboard_input` `0x401ab0` 的装配链含 `0x50/0x60/0x90` 复合位，未逐位解完 |
| `hyper_type==2`（カワウソ）3 光球的命中数值语义 | 🟡 伤害 2、每帧取消子弹；半径来源未定 |
| 咆哮超时释放 vs trophy 文案「ロアリング終了時」的措辞差异 | 🟡 见 §5.4 注 |
| `beast.anm` 13 个 entry 与 7 只稀有动物的**逐图**对名 | 🟡 靠 `trophy.txt` 顺序 + `beast2*.png` 7 张一一对应，未做人眼比对（本机模型不支持读图） |

## 11. 跨作对照：TH13 神灵 / 灵界（只引用，未重跑 TH13）

[`spirit/OVERVIEW`](../../spirit/OVERVIEW.md) 与 [`spirit/th13/01`](../../spirit/th13/01-spirit-and-trance.md)
把 TH13 神灵 / 灵界写成一手的「第三条资源轴」。两作**机制同构、代码不同源**：

| 维度 | TH13 神灵 / 灵界（该文一手） | TH17 动物灵 / 咆哮（本文一手） |
| --- | --- | --- |
| 管理器 | `SpiritItemManager` `th13:0x4383a0`（256 槽池，stride `0x88`） | `GoastManager` `0x40e420`（token 链表，上限 40） |
| 收集物 | 4 种神灵（奖励不同） | 3 兽灵 + 4 道具灵 + 7 稀有动物 |
| 充能 | `CURRENT_SPIRIT_GAUGE` `th13:0x4be808`（0..600，3 格） | `HYPER_FILL` inner+`0x84`（0..5 格） |
| 收集半径 | 距离 ≤ 20 px 吸收 | 距离 ≤ 24 px 吸收（速度 ×0.25，约 10 帧） |
| 进模式 | 专用灵界键 `th13:0x800`，要求满槽 | **炸弹键**（掩码 bit `0x2`），要求队列满 5 |
| 中弹保护 | 灵界中免伤 + 死亡前自动免费灵界 | `do_hyper_cancel(…,1)` + `cancel_impending_death` |
| 同源？ | — | **不同源**：类名/结构体/字段/地址全不同 |

> 本节的 TH13 列全部引自上两篇，**未在本次会话重跑 TH13 样本**。
> 顺带订正 [`spirit/OVERVIEW`](../../spirit/OVERVIEW.md) 矩阵里 th17 的两个 ❓（该文件由另一位 agent 维护，本文只记结论）：
> 「手动进入要求槽满」→ ✅（满 5 格才 `HYPER_FLAGS |= 2`，之后才读按键：`0x40fc8a` / `0x40f37c`）；
> 「模式期间收集物换外观 / 奖励翻倍」→ 未找到换外观或翻倍的证据，TH17 侧是「咆哮改变自机射击」
> （`hyper_type` 被 `Player::do_shooting` 读）❌/🟡。

## 12. 证据与复现

```bash
# 1) 反汇编全量
objdump -d -M intel local/th17.v1.00b/th17.exe > th17.asm
# 2) Ghidra 反编译（工程已就绪）
export PYTHONPATH=local/pyghidra-site GHIDRA_INSTALL_DIR=$HOME/opt/ghidra_12.1.3_PUBLIC \
       JAVA_HOME=/usr/lib/jvm/java-21-openjdk-amd64
python3 tooling/ghidra/scripts/decompile_funcs.py th17.v1.00b \
        0x410380 0x40e750 0x40ee70 0x40f6d0 0x40f980 0x40fb60 0x410af0 0x434740
# 3) 资产
source tooling/env.sh
"$THDAT" -x 17 local/th17.v1.00b/th17.dat out/
"$THANM" -l 17 -m local/vendor/thpages/static/mapfile/v8.anmm out/beast.anm
"$THECL" -d 17 out/st01.ecl out/default.ecl
```

- 关键函数：`0x40e420`（构造）/`0x40e750`（on_tick）/`0x40e920`（爆散）/`0x40ee70`（咆哮主循环）/
  `0x40f6d0`（霊撃）/`0x40f980`（收集）/`0x40fb60`（就绪）/`0x410200`（HUD 槽）/`0x410380`（spawn）/
  `0x410570`（散射）/`0x410af0`（token 帧）/`0x4111a0`（链表）/`0x434740`（道具→token）/
  `0x449420`（Player::die）/`0x4495e0`（cancel_impending_death）/`0x4491b0`（真死）。
- 常量：咆哮时长表 `0x4a13cc`–`0x4a13d8`（1140/840/1140/1500）、`beast.anm` 名字串 `0x4a13a4`。
- 相关：[`animal/OVERVIEW`](../OVERVIEW.md)、[`animal/th18/01`](../th18/01-animal-residuals.md)、
  [`games/th17.v1.00b/INDEX.md`](../../../games/th17.v1.00b/INDEX.md)。
