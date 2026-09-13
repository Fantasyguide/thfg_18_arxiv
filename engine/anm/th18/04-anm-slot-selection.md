# anm/th18 §4 — `anmSelect(N)` 的 `N`、`anmSetSprite(K,S)` 的 `K`/`S` 到底是什么

> **版本**：TH18 v1.00a（`th18.exe`，imagebase `0x400000`）。本文所有地址都写全 `th18:0x…` 前缀
> （等价于同目录 [01](01-vm-instantiate.md)–[03](03-ascii-text.md) 的裸地址写法，只是显式标注）。
> **方法**：一手平坦反汇编（`local/_scratch/th18_full.asm`）+ 直读 exe/`.dat` 常量表 + 7 关 ECL 反编译穷举
> （`thecl -d 18`）+ ExpHP `local/vendor/th-re-data/data/th18.v1.00a/`（funcs/statics/结构体）交叉对名。
> **可信度**：`N → 文件` 四条分支全 ✅（每条都有写入点 + 读取点两端一手）；`K`/`S` ✅；
> 少数附带推断标 🟡/❓（见 §7）。

## 0. 结论速查

| 参数 | 是什么 | 一手依据 |
| --- | --- | --- |
| `N`（`anmSelect(N)`） | **anm 源选择器**：4 类文件（2 个硬编码 + 当前关 ECL 的 `anim{}` 列表 + `st0X.std` 里的 wl anm）。写进 `zEnemyData+0x264` | `th18:0x430dfd`；解析器 `th18:0x438d40` |
| `K`（`anmSetSprite(K,S)` 第 1 参） | **本敌机自己的 anm 槽号**：`zEnemyData.anm_vm_ids[0x10]`（`zEnemyData+0x124 + K*4`）。合法 `0..15` | `th18:0x436be0`/`th18:0x436be8`（写槽）、`th18:0x436d50`（兄弟分派器读槽）；上限检查 `th18:0x436d47` |
| `S`（第 2 参） | **该 anm 文件「扁平脚本表」的脚本号**（整个文件所有 entry 的脚本连续编号） | 载体 `th18:0x407476`/`th18:0x4074fc`；ECL 用到 251 > 单 entry 的 89 |
| `S` 的范围校验 | **没有校验**（只按 `S*0x60c` 取模板）。唯一显式分支：`anmSetSprite` 里 `S < 0` = 拆掉该槽 | `th18:0x436bf6`；原生 14 处 `anmSetSprite(K,-1)` |

**一句话可执行结论**：要让一条 **ECL 敌机**画出自定义动画 —— 把脚本**追加在 `enemy.anm` 末尾**，
ECL 写 `anmSelect(2); anmSetSprite(0, 261);`（`261` = TH18 v1.00a `enemy.anm` 现有扁平脚本数，
新脚本就是第 261 号）。见 §6 的三条路线与硬约束。

## 1. 发现：一个选择器，N 个副本

`anmSelect` 只是把 ECL 参数存起来，真正解析发生在**消费方**。

```
ECL 302 anmSelect(N)  →  th18:0x430dfd:
    ecl_get_int_arg(ctx,0) → eax
    mov [edi+0x264], eax            ; edi = zEnemyData*，+0x264 = ExpHP「selected_anm_index」
```

消费 `+0x264` 的 opcode（一手读跳转表：`movzx eax,byte[op-300+th18:0x4366f4]` → `jmp [eax*4+th18:0x436438]`，
用 522/524 校准过：`522→th18:0x434eba`、`524→th18:0x435b23`，与 `local/_scratch/th18-seams.md` 的独立解码一致）：

| opcode | 名字（eclmap） | handler | 用它做什么 |
| --- | --- | --- | --- |
| 303 | `anmSetSprite` | `th18:0x436bb0`（内联副本 `th18:0x436c16`–`th18:0x436c4e`） | 槽 `K` 起脚本 `S`；`S<0` 拆槽 |
| 306 | `anmSetMain` | `th18:0x431592`（调共享解析器 `th18:0x438d40`） | 同上，并记 `+0x270`/`+0x26c`（「主 anm」） |
| 313 | `anmSelectedPlay` | `th18:0x4316f5` | 槽 `arg0` 起 `[+0x270]+5` |
| 316 | `anm316` | `th18:0x4317a1` | 槽 `arg0`、脚本 `arg1(+5)` |
| 318 | `anmReset` | `th18:0x431680` | 用记下的 `+0x264`/`+0x270` 重建槽 0 |

**⚠️ 不消费 `anmSelect` 的三个**：`307 anmPlay` `th18:0x4311df`、`308 anmPlayAbs` `th18:0x431345`、
`314 anmPlayHigh` `th18:0x431292` —— 它们**把 anm 索引当自己的第 1 参**（`arg0`），产出的 VM 挂到
**EffectManager**（`[0x4cf2c8+0x1c+idx*4]`，`th18:0x431289`）。配对统计时把它们算进来就会出现假"越界"（§5）。

同一套四分支配色（逐字节相同的 3 段比较体）在 exe 里**至少 14 处**（数法：把「`N=7`/`N=0`/`N=1` 三段分支体
在 40 条指令窗口内共现」记作一次拷贝）：共享函数 `th18:0x438d40`、`anmSetSprite` 内联版 `th18:0x436c1b`、
`anmPlay`/`anmPlayHigh`/`anmPlayAbs`（`th18:0x431204`/`th18:0x4312b7`/`th18:0x431366`）、
派发器里另 3 处（`th18:0x43102d`/`th18:0x43114d`/`th18:0x4313e9`）、`th18:0x42a69a`/`th18:0x42a70f`、
`th18:0x4302ea`、MSG/GUI 侧 3 处（`th18:0x43f857`/`th18:0x43f965`/`th18:0x43fa72`，即
[`../../msg/th18/01-stage-msg.md`](../../msg/th18/01-stage-msg.md) 的 op2 选择器）。
十四处编码完全一致 ⇒ 这是引擎级的固定约定，不是某个函数的巧合 ✅✅。

## 2. `N → 文件` 映射表（`th18:0x438d40` 逐分支）

```
mov  ecx,[ebp+8]                      ; N
cmp  ecx,0x7 ; je  → [0x4cf2b4]+0x3464      ; N=7
test ecx,ecx ; je  → [0x4cf2bc]+0x7a41ec    ; N=0
cmp  ecx,0x1 ; je  → [0x4cf2c8]+0xc         ; N=1
else               → [0x4cf2d0]+0x168+ecx*4 ; N≥2（N≠7）
```

| N | 文件 | 取值链 | 可信度 |
| --- | --- | --- | --- |
| 0 | **`bullet.anm`** | `[BulletManager]+0x7a41ec` ← `preload_anm(7,"bullet.anm")`（`th18:0x423b7a`–`th18:0x423b86`，ctor `th18:0x423af0`） | ✅ |
| 1 | **`effect.anm`** | `[EffectManager]+0xc` ← `preload_anm(8,"effect.anm")`（`th18:0x42aceb`–`th18:0x42acf7`，ctor `th18:0x42ac70`） | ✅ |
| 2 | **`enemy.anm`**（7 关全部 `anim{}` 的第 0 条） | `[ENEMY_MANAGER]+0x170` ← ANIM 解析器 `th18:0x42da90` 写 `+0x170+i*4` | ✅ |
| 3 | **当前关的 `st0Xenm.anm`**（`anim{}` 第 1 条） | `[ENEMY_MANAGER]+0x174` | ✅ |
| 4 | `anim{}` 第 2 条：st05/st06/st07 = `st05enm2.anm`；st01–04 无（= 0） | `[ENEMY_MANAGER]+0x178` | ✅ |
| 5 | `anim{}` 第 3 条：仅 st07 = `st04enm.anm` | `[ENEMY_MANAGER]+0x17c` | ✅ |
| 6 | 无原生关用到（最多 4 条 ANIM） | `+0x180` | ✅ |
| **7** | **当前关的 `st0Xwl.anm`** | `[Stage]+0x3464` ← `preload_anm(3 或 4, <st0X.std+0x10 的字符串>)`（`th18:0x41b902`） | ✅ |
| ≥8（除 7） | `anim{}` 第 `N-2` 条；**超出条数就是 0** | `+0x168+N*4` | ✅ |

所以「其余 = `ENEMY_MANAGER+0x168+idx*4`，即 ECL 载入的 anm 槽」这句旧笔记要收紧成
**`N = anim{} 下标 + 2`**：数组本身从 N=0 起算（`+0x168`），但 0/1 被硬编码分支抢占，
ANIM 解析器实际从 `+0x170`（= N=2）开始写。`zEnemyManager` 整块 0x1a0 字节
（ctor `th18:0x42dc20`：`push 0x1a0` + `memset`），`+0x168..0x19c` = 14 个 `zAnmLoaded*` ⇒ 结构上 N 最多到 13 ✅。
ANIM 条数再多也会撞上 preload 槽上限（`th18:0x4867e0` 的 `cmp edi,0x21`：`i+10 < 33`）。

**七关 `anim{}` 实测**（`thecl -d 18` 反编译的头部原样，也可用 `strings -t x st0X.ecl` 复核）：

| 关 | ANIM 列表（= N=2 起） |
| --- | --- |
| st01 | `enemy.anm`, `st01enm.anm` |
| st02 | `enemy.anm`, `st02enm.anm` |
| st03 | `enemy.anm`, `st03enm.anm` |
| st04 | `enemy.anm`, `st04enm.anm` |
| st05 | `enemy.anm`, `st05enm.anm`, `st05enm2.anm` |
| st06 | `enemy.anm`, `st06enm.anm`, `st05enm2.anm` |
| st07 | `enemy.anm`, `st07enm.anm`, `st05enm2.anm`, `st04enm.anm` |

### 2.1 各分支的写入点（一手）

- **N=0**：`th18:0x423af0`（= ExpHP `BulletManager::operator new`，对象 `0x7a41f8` 字节）里
  `mov ds:0x4cf2bc,ebx`（`th18:0x423b6d`），随后 `push 0x4b636c`(="bullet.anm") / `push 7` / `call th18:0x486880`
  → 结果存 `[ebx+0x7a41ec]`（`th18:0x423b86`）。`0x4cf2bc` = ExpHP `BULLET_MANAGER_PTR` ✅。
- **N=1**：`th18:0x42ac70`（= ExpHP `EffectManager::operator new`）里先 `mov ds:0x4cf2c8,ebx`，
  再 `preload_anm(7,"bullet.anm")→[ebx+0x10]`、`preload_anm(8,"effect.anm")→[ebx+0xc]`。
  独立旁证：特效生成器 `th18:0x42af70` 取 `[EffectManager + word[th18:0x4ccbf8+type*0x20]*4 + 0xc]`
  当 anm 走 `th18:0x405bf0` 起 VM ⇒ `+0xc` 是 EffectManager 的第 0 个 anm，用它的只有 effect 类特效 ✅。
- **N=7**：`th18:0x41b850`（= ExpHP `Stage::initialize`，对象 0x34a0 字节，ctor `th18:0x41bf90`）
  `mov ds:0x4cf2b4,ebx`（`th18:0x41b860`）→ 用 `[[CUR_STAGE_DATA]+4]`（= `STAGE_DATA_TABLE[stage]+4`，
  实测 8 张表里就是 `"st01.std"`…`"st07.std"`）打开 `.std`（`th18:0x402060`）→ `malloc(len)` + `memcpy` →
  `preload_anm(stage&1 ? 4 : 3, 副本+0x10)` → `[ebx+0x3464]`（`th18:0x41b907`）。
  **`st0X.std` 的 0x10 处就是 anm 文件名**：`xxd -s 0x10` ⇒ `st01wl.anm` … `st07wl.anm`（7 个文件全查）✅。
  旁证：`.std` 自己的解释器（`th18:0x41d93d` 一段：从 `.std` 记录里取脚本号 → `th18:0x407420`，
  `ecx = [zStage+0x3464]`，VM 落在 `zStage+0x38c + i*0x60c`）也把这个 anm 当「世界动画」源用 ✅。
- **N≥2**：`th18:0x42da90` 遍历 `"ANIM"` 列表（`cmp [eax],0x4d494e41`）：对第 i 个名字先查
  `[ANM_MANAGER + 0x3120754 + i*4]`（= preload 槽 `i+10`）是否已装载，未装载就
  `th18:0x4867e0(ANM_MANAGER, i+10, 名字)` 现场载入，然后把 `zAnmLoaded*` 写进
  `[EBX + ENEMY_MANAGER - 0x31205e4]` = **`ENEMY_MANAGER + 0x170 + i*4`**（`th18:0x42daed` 命中已装载路径 / `th18:0x42db51` 新载入路径）。
  `ANIM` 之后紧跟 `"ECLI"`（`th18:0x42dba3`）—— 与 [`../../ecl/format-reference.md`](../../ecl/format-reference.md) 的 SCPT/ANIM/ECLI 布局一致 ✅。

## 3. `K` = 本敌机的 anm 槽号（0..15）

`anmSetSprite`（`th18:0x436bb0`）一手：

```
push 0 ; call ecl_get_int_arg   → K
push 1 ; call ecl_get_int_arg   → S
lea  ebx,[edi + 0x124 + K*4]    ; 槽（ExpHP: zEnemyData.anm_vm_ids[0x10]）
push [ebx] ; call th18:0x488cf0 ; 先把旧 VM 标删
mov  [ebx],0
test esi,esi ; js → 收工        ; S<0 ⇒ 只清槽，不起 VM
...解析 N → anm（内联副本）...
push layer[= zEnemyData+0x27c + 7] / script=S / &out_id ; ecx=anm ; call th18:0x43a030
mov  [ebx],<新 vm id>           ; 新 id 写回槽
K==0 ⇒ zEnemyData+0x26c = S、+0x268 = N，并把 final_sprite_size(+0x4fd8/+0x4fdc)
        = vm.sprite_size × vm.scale（th18:0x43a250/0x43a260）
flags_lo & 0x20 ⇒ th18:0x488eb0(槽)
```

- **槽表是「每条敌机各自一份」**，不是全局 VM 数组：`zEnemyData.anm_vm_ids`（`+0x124`，16 个 `zAnmId`）+
  `zEnemyData.anm_pos`（`+0x164`，16 个 `zFloat3`，`anmMove` 写它）。两个数组正好头顶脚：`0x124+16*4 = 0x164`、
  `0x164+16*12 = 0x224` ✅。
- **上限 `K ≤ 15` 由兄弟分派器校验**：`th18:0x436cf0`（槽内子指令 319/325–337 = `anmRotate`/`anmColor`/`anmMove`/… 的公共入口）
  里 `cmp ebx,0xf ; ja 默认` （`th18:0x436d47`）。
  **`anmSetSprite`/`anmSetMain` 自己不做边界检查** ⇒ `K ≥ 16` 会写穿到相邻字段（`+0x164` 区），是真实的越界口子 ✅。
- 槽 0 特殊：除记录 `+0x268/+0x26c` 外，它的 VM 尺寸会变成敌机的 `final_sprite_size`（判定的依据）——
  所以「主体」放槽 0 是原生约定（153/153 次 `anmSetMain` 都用 K=0；`anmSetSprite` 用 K=0..3）。
- `anmSetMain`（`th18:0x431592`）与 `anmSetSprite` 几乎同构，差别只有：不处理 `S<0`；
  K==0 时额外写 `+0x270`（ExpHP `anm_main`）并置 `flags_lo |= 0x100000`。
  `+0x270` 就是 `anmReset`/`anmSelectedPlay`/`anm316` 回放用的「主脚本」记忆。

## 4. `S` = 该 .anm 的扁平脚本号；**没人校验**

- **扁平**：`th18:0x43a030`（`AnmLoaded::instantiate_vm_to_world_list_front`，`ret 0x10`，第 4 个参数不被读）
  → `th18:0x407420`：`esi = S*0x60c + [AnmLoaded+0x10c]`（`th18:0x407476`/`th18:0x4074fc`）→
  `rep movsd` 拷 0x151 dword 进 VM 模板。**从头到尾没有和脚本数比较** ⇒ 越界 = 读脚本表之后的字节当指令（UB）。
- **为什么能确定是"整个文件"的编号**：`enemy.anm` 第 0 个 entry 只有 **89** 个脚本，而 ECL 用到 `S = 251`
  （st04 `anmSelect(2)`）⇒ 编号必然跨 entry 连续 ✅。用 `nextoffset`（entry 头 +0x24）链起来求和即可复现。
- TH18 v1.00a 关心的几个文件的扁平脚本数（自算，与 `thanm -l` 的条目顺序一致；也与人先前实测
  `enemy.anm 261 / bullet.anm 328 / effect.anm 195` 完全吻合 ✅）：

| 文件 | 脚本数 | 文件 | 脚本数 |
| --- | --- | --- | --- |
| `bullet.anm` | 328 | `st01enm.anm` | 19 |
| `effect.anm` | 195 | `st02enm.anm` | 19 |
| `enemy.anm` | **261** | `st03enm.anm` | 18 |
| `st04enm.anm` | 20 | `st05enm.anm` | 19 |
| `st05enm2.anm` | 13 | `st06enm.anm` | 23 |
| `st07enm.anm` | 20 | `st01wl.anm` | 6 |
| `st02wl.anm` | 4 | `st03wl.anm` | 4 |
| `st04wl.anm` | 6 | `st05wl.anm` | 7 |
| `st06wl.anm` | 7 | `st07wl.anm` | 8 |

- **`S < 0` 是唯一的显式分支**（`th18:0x436bf6`，`anmSetSprite` 专有）：拆掉槽 K 的 VM、不建新的。
  原生真的这么用 —— 14 处，全在 boss 死亡 / 符卡收尾：`st02mbs:179 anmSetSprite(1,-1)`、
  `st07bs:2390 anmSetSprite(2,-1)`、`st07mbs:407/408 anmSetSprite(1,-1); anmSetSprite(2,-1);` 等 ✅。
- 顺带：`N` 也不校验 —— 若 `anim{}` 没有第 `N-2` 条，槽里是 0，而 `th18:0x43a030` 第一句就是
  `inc [anm+0x134]`（引用计数），**没有 NULL 检查** ⇒ **写超范围的 N 直接崩**（`0x134` 处写入 = 访问违例）✅。

## 5. 交叉验证：7 关 ECL 穷举

方法：`thecl -d 18` 反编译 22 个 `.ecl`（7 关主档 + bs/mbs + `default.ecl`），
**只在「消费 `anmSelect` 的 opcode」上配对**（303/306/313/316/318；排除自带索引的 307/308/314），
遇到 label / `return` / `goto` / 复合 `if` 就丢掉当前 N（跨基本块不配对）。

| 关 | N | 推定文件 | 调用数 | K 范围 | S 范围 | 该文件脚本数 | 判定 |
| --- | --- | --- | --- | --- | --- | --- | --- |
| st01 | 1 | effect.anm | 3 | 1–2 | 105–114 | 195 | ✅ |
| st01 | 2 | enemy.anm | 35 | 0–1 | 0–111 | 261 | ✅ |
| st01 | 3 | st01enm.anm | 10 | 0 | 0–6 | 19 | ✅ |
| st02 | 2 | enemy.anm | 34 | 0–1 | 0–**224** | 261 | ✅ |
| st03 | 2 | enemy.anm | 31 | 0–1 | 20–214 | 261 | ✅ |
| st04 | 2 | enemy.anm | 12 | 0–1 | 40–**251** | 261 | ✅ |
| st05 | 3 / 4 | st05enm / st05enm2 | 3 / 2 | 0 | 0 | 19 / 13 | ✅ |
| st06 | 3 / 4 | st06enm / st05enm2 | 8 / 2 | 0–3 | 0–22 | 23 / 13 | ✅ |
| st07 | 3 / 4 / 5 | st07enm / st05enm2 / st04enm | 10 / 3 / 1 | 0 | 0 | 20 / 13 / 20 | ✅ |
| default | 2 | enemy.anm | 2 | 0 | 229–235 | 261 | ✅ |

全部 314 个配对**零违例**；反向证据更硬：`N=2` 用到 `S=251`，任何「`N=2` = `st0Xenm.anm`（18–23 脚本）」
的假设当场证伪 ✅✅。

**反面教材（我自己踩的）**：把 `anmPlay(1,96)` 当"跟着 `anmSelect(3)` 走"就会得到
「st01enm.anm 有 19 个脚本却有 S=96」的假越界。真相是 `anmPlay` 的 `arg0` 才是 anm 索引：
`anmPlay(1,96)` = `effect.anm` 脚本 96（195 个，合法）。全原生 ECL 里 `anmPlay*(K,S)` 只出现三类索引：
`K=0` → S=28（bullet.anm）、`K=1` → S=32..104（effect.anm）、`K=7` → S=3..4
（`st01wl.anm` 只有 6 个脚本 ⇒ 合法，顺带第三次印证 `N=7 = st0Xwl.anm` ✅）。

## 6. 可执行结论

**要把自定义动画画到一条 ECL 敌机上，脚本要落在「当前关能看见的 anm」里。**
`ability.anm`/`abcard.anm`（卡牌那套）**不在**这个集合里 —— ECL 只有 §2 那四种取值路径。

| 路线 | 脚本追加进 | ECL 写 | 代价 |
| --- | --- | --- | --- |
| **A. 追加到 `enemy.anm`**（推荐，零 ECL 头改动） | `enemy.anm` 末尾（新脚本 = 第 **261** 号） | `anmSelect(2); anmSetSprite(0, 261);` | patch 里多一个被替换的原生 `.anm`；**所有关通用** |
| B. 追加到 `effect.anm` | `effect.anm` 末尾（新脚本 = 第 **195** 号起） | `anmSelect(1); anmSetSprite(0, 195);` | 同上；`effect.anm` 常驻（EffectManager 开局预载槽 8） |
| C. 给本关 ECL 加一条 ANIM | 新 `.anm`（自己切图），写进 SCPT 头 `anim { "enemy.anm"; "mymod.anm"; }` | `anmSelect(3); anmSetSprite(0, S);`（`S` = 该新文件里的脚本号） | 要改 ECL 头并让该 anm 进 `.dat`；只有这一关能用 |

**硬约束（照抄即可，违反 = 崩或乱画）**：

1. `N` 必须在有效范围且对应槽非 0（§2 表：0/1/2/3/4/5/7 或 `anim{}` 里的下标+2）。越界 ⇒ `0x43a030` 对 NULL 解引用。
2. `K ∈ 0..15`（同一条敌机的槽位；`K=0` 是"主体"，决定 `final_sprite_size` 判定框）。
3. `S` 必须 `< 该文件的扁平脚本数`；**引擎不查**，写错了就是拿后面的字节当 anm 指令。
   `anmSetSprite(K, -1)` 是合法用法（拆槽）。
4. 追加脚本必须**追加在文件末尾**（新的 entry 或最后一个 entry 的末尾），否则会顶掉后面所有脚本的编号。

> 这条直接回答 `mods/th18.v1.00a/card-expand/TUNING.md` §3.1「112 飞碟的 anm 落点」——
> 那里的路线 1「把脚本追加进 `effect.anm`」对应的 N 就是 **1**（🟡 → ✅），
> 断言「`ability.anm` 里的脚本 ECL 指不到」也成立 ✅。

## 7. 与已有笔记的差异 / 未答

- **订正 1**：旧笔记把 `th18:0x4315d1`–`0x4315e4` 那一处（push `[edi+0x264]` → `th18:0x438d40`）
  当成 `anmSetSprite` 的实现。它是 **306 `anmSetMain`**（`th18:0x431592`）。
  303 `anmSetSprite` 是独立函数 `th18:0x436bb0`（解析器内联在 `th18:0x436c16`–`th18:0x436c4e`）—— 两者结论一致，但地址要对上。
- **订正 2**：`th18:0x406636`（`mov [ecx+0x168],eax`）**不是** anm 槽表。那是 `AnmVm` 的
  `alpha1_i` / `rgb1_i` 插值设置器（`ecx` = `AnmVm*`，同族字段 `+0x140/+0x144/+0x148/+0x14c/+0x160/+0x164/+0x16c`），
  被 328 `anmAlphaTime` 一类的槽内指令调用。槽表在 `zEnemyData+0x124`（每条敌机一份，见 §3）。
- 🟡 `anmSetSprite` 与 `anmSetMain` 的语义分工（"辅槽/主槽"）是从字段写法推的：
  `anmSetMain` 额外记 `+0x270`（只有它能被 `anmReset`/`anmSelectedPlay`/`anm316` 复用），
  原生用法上 `anmSetMain` 恒 `K=0`；但"主/辅"这套命名不是原生代码里的字符串，属于命名而非机制。
- ❓ `zEnemyData+0x27c`（ExpHP `anm_layers`）由哪个 opcode 写、`layer = +0x27c + 7` 里的 `+7` 是什么分组，未追。
- ❓ `th18:0x488eb0`（`flags_lo & 0x20` 时代替删除调用的收尾）内部语义未细看（清 `vm+0x534` 一位 + 遍历 `vm+0x598`）。
- ❓ `zEnemyData+0x130/+0x134`、`AnmVm+0x160/+0x164` 那套「`test al,1` / `or eax,1` / 清伴随字段」的惯用法
  （MSVC 生成的两字段惰性标记）未逐点确认归属。
- ❓ ANIM 列表若多于 12 条会写穿 `zEnemyManager`（`+0x170+11*4 = 0x19c` 是最后一项），
  解析器本身无条数检查；原生最多 4 条，所以看不到后果。

## 证据指针

`anmSelect` case `th18:0x430dfd` · 选择器 `th18:0x438d40` · `anmSetSprite` `th18:0x436bb0`（内联选择器 `th18:0x436c16`）·
`anmSetMain` `th18:0x431592` · `anmPlay` `th18:0x4311df` · `anmSelectedPlay` `th18:0x4316f5` · `anm316` `th18:0x4317a1` ·
槽内指令分派器（K≤15 检查）`th18:0x436cf0`（检查点 `th18:0x436d47`）·
`AnmManager::preload_anm` `th18:0x486880` / 内层 `th18:0x4867e0`（索引上限 `0x21`）·
ANIM 解析器 `th18:0x42da90`（写入 `th18:0x42daed` / `th18:0x42db51`）·
`Stage::initialize` `th18:0x41b850`（`st0X.std` → `st0Xwl.anm`，`th18:0x41b902`）·
`EffectManager` ctor `th18:0x42ac70`（`th18:0x42aceb`）· `BulletManager` ctor `th18:0x423af0`（`th18:0x423b7a`）·
`AnmLoaded::instantiate_vm_to_world_list_front` `th18:0x43a030` → 模板拷贝 `th18:0x407420`（`th18:0x407476`/`th18:0x4074fc`）·
全局 `0x4cf2b4`(Stage) `0x4cf2bc`(BulletManager) `0x4cf2c8`(EffectManager) `0x4cf2d0`(EnemyManager) `0x4cf428`/`0x4c9410`(关卡表) `0x51f65c`(AnmManager)（后五个名字取自 ExpHP `th-re-data` th18）·
结构 `zEnemyManager` / `zEnemyData` / `zStage` / `zAnmLoaded` / `zAnmVm`（同上）·
数据 `local/th18.v1.00a/dat/st0X.ecl` 的 `anim{}` 头、`st0X.std+0x10`。
