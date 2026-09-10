# TH16 引擎数学 / CRT 模块(逆向语义表)
> **版本**：TH16 v1.00a（`th16.exe`，imagebase `0x400000`）。本文裸地址默认属该版本；引用其他版本须写成 `th18:0x…`。
>

> **主题**:东方红楼梦…不,**TH16《鬼形兽》(th16.exe,imagebase `0x400000`)** 的数学子系统——
> 角度/向量几何、PRNG、以及游戏所链接的 MSVC CRT 浮点函数。
> 这是**跨主题的引擎知识**(不专属 SHT),按 `shared/` 录入规范登记:每条标 **出处 / 可信度 / 适用版本 / 地址·函数**。
>
> **可信度图例**:✅ 一手反汇编实证(指令级 + 多源/交叉印证) · 🟡 单源或结构推断、未过对抗证伪 · ❓ 存疑。
> **适用版本**:除非注明,所有结论**仅 TH16**(SHT/引擎逐版本差异大,勿外推到 TH15/17/18/19)。
>
> **方法与诚实声明**:本表由「Workflow 中立命名(不喂标签防 priming)→ 对抗证伪」流水线(20 函数 ×2 阶段,
> 共 ~40 子 agent)+ 主控**一手复核**产出。**对抗证伪阶段已完整跑完**,并**实打实抓出一处错误**:
> `0x488a00` 初判 `trunc`,对抗 agent 改判 `floor`,经我反汇编负数分支(`x<trunc 时 SUBSD 1.0`)证实=**floor**(见 §1.3)。
> 另有两 agent 对 sin/cos 对名**互相矛盾**,由我看旋转式 `X'=X·cos−Y·sin` 一手裁定(§1.4)。
> 浮点常量值与关键 x87 指令(FPREM/FPATAN)由**直接读取 th16.exe 的 PE 字节**确认(见 §1.6、§4),非反编译器推断。
> 纪律来源:`METHOD.md`、memory `re-overclaim-guard` / `re-agent-no-hypothesis-priming`。

---

## 0. 全局速览

| 子系统 | 关键函数 | 状态 |
| --- | --- | --- |
| CRT 浮点 | atan2=`0x487aaa`、fmod=`0x487aca`、sqrt=`0x488690`(`__libm_sse2_sqrt_precise`)、floor=`0x488a00`、sin=`0x405510`/cos=`0x4054f0`(核 `0x4884c0`/`0x488300`) | ✅ |
| 角度归一化 | `0x402d90`(纯归一)、`0x4052e0`(累加+归一)、`0x411410`(归一并写回 `*+0x1c`) | ✅ |
| 极坐标→直角 | `0x430df0`、`0x4054d0`(=速度=f(角,模)) | ✅ |
| atan2 浮点包装 | `0x4052a0`(`atan2(dy,dx)` 的 float 入/出包装) | ✅ |
| sin/cos 浮点包装 | `0x405510`=**sin**、`0x4054f0`=**cos**(旋转式一手裁定) | ✅ |
| PRNG | **16位满周期生成器**(单步 `0x449720`/双步 gameplay `0x402be0`)、播种 `0x43b520`、回放存还原 `0x449030`/`0x447760`、float 包装 `0x402cb0`/`70`/`f0`;周期 65536/gameplay 32768,模型见 `scripts/th16_prng_model.py` | ✅ |
| 运动积分器(用上述原语) | `0x402ff0`、`0x403110` | ✅(行为) |

**枢纽洞察**:本作**没有链接 1 参 `_CIxxx` CRT intrinsic**(2 参分派器 `__cintrindisp2` `0x488bb0` 只有 2 个 stub;1 参分派器 `__cintrindisp1`/`__ctrandisp1`/`__ctrandisp2` 的 `get_xrefs_to` **全为空**)。三角函数走**直接调用 `__libm_sse2_sin/cos` 核**(SSE2 多项式),不经 `_CI` x87 分派。这点把 CRT 面收得很窄。

---

## 1. CRT 浮点函数(标签深度)

> 本作 CRT = **MSVC VS2015+**,**混合实现**:
> - **2 参 intrinsic(atan2/fmod)走 x87 thunk**——经 `__cintrindisp2`/`__trandisp2` 分派到含 `FPATAN`/`FPREM`
>   的 x87 例程(PE 字节实测:fmod 的 thunk `0x487ad4` 内 `FPREM=D9 F8`;atan2 簇内有 `FPTAN/FPATAN`)。
> - **1 参(sin/cos/sqrt/floor)走 SSE2 `__libm_sse2_*` 多项式**(无 x87 超越指令)。
>
> ⚠️ 早前「全簇无 x87 超越指令」的说法**错了**——当时只反了 SSE2 核(`0x4884c0` 等),没反 2 参 intrinsic 的 x87 thunk。识别靠**x87 指令特征(fmod)+ 调用约定 + call-site 行为(atan2)**。

### 1.1 `0x487aaa` = `atan2` ✅
- **出处/证据**:① stub 形态 `MOV EDX,0x494e50; JMP 0x488bb0(__cintrindisp2)` = MSVC `_CIatan2/_CIpow/_CIfmod` 家族的 2 参 x87 intrinsic 规范壳;`__cintrindisp2` 对 ST0/ST1 双 FXAM(确为 2 参)。② **独立交叉印证**:一个**不知情(无标签)的子 agent** 反编译浮点包装 `0x4052a0` 时,仅凭「两参=坐标差 (dy,dx)、返回当角度用」就独立判定为 atan2(见 §1.5)。③ 17 个 call-site,homing(`playershot_tick_homing_idx1` `0x445ee0`)中 `find_nearest_enemy→atan2(dy,dx)→拧角` 教科书式。
- ④ **零值护栏**:包装 `FUN_00443840` 在 `dy==0 && dx==0` 时直接返回 `0x494534=π/2`(`atan2(0,0)` 的约定缺省)——这是 atan2 特有模式,pow/fmod 不会有。⑤ 对抗证伪 agent 枚举全 17 call-site:**无一处把返回值当模长/距离/幂用**(那会暴露 pow/fmod 误判),证伪不掉。
- **结论**:TH16 的 2 参反正切 CRT intrinsic = `atan2(ST1=dy, ST0=dx)`(`_CIatan2`),走 x87 thunk。同走 `0x488bb0` 的另一个 2 参 stub = `0x487aca`=fmod(§1.6)。
- 旧记录 `crt_atan2_likely`(`apply_th16_sht_names.py`)→ 升级 `crt_atan2` ✅。

### 1.2 `0x488690` = `sqrt`(`__libm_sse2_sqrt_precise`)✅
- **出处**:Ghidra 库签名(FidDb)直接命名;18 个 call-site(距离/模长计算)。无需再证。

### 1.3 `0x488a00` = `floor`(向 −∞ 取整)✅ ★对抗证伪纠错
- **★教训**:初判 `trunc`(向零),**被对抗 agent 改判 `floor`,我反汇编坐实=floor**。
- **决定性证据**(负数分支 `0x488a98`):`PSRLQ/PSLLQ` 先得「向零截断」的尾数 → `CMPLTPD x<截断? → ANDPD 1.0(0x494e00) → SUBSD 截断−1.0`。即 **x 为负且有小数时减 1** = 向 −∞ = floor(`trunc(-2.3)=-2`,但本函数给 `-3`)。常量 `0x494e00=1.0`(PE 实测,double)。
- 其余:`PSRLQ XMM0,0x34` 取指数 → 移位量;阈值 `CMP EAX,0x3ff`(|x|<1)、`0x432`(|x|≥2^53→原样)。call-site(`FUN_00417bc0`/`0x437ee0`/`0x438370`)用作 `floor(elapsed/period)` 帧/计分分箱(小数部 `x−floor(x)∈[0,1)` 恒非负,正合分箱语义)。
- **可信**:✅(指令级负数分支 + 常量 PE 实测)。

### 1.4 `0x405510`=`sinf`(核 `0x4884c0`)/ `0x4054f0`=`cosf`(核 `0x488300`)✅ ★对抗裁定
- **对名一手裁定**:旋转点 `FUN_0040e490` `0x40e527`/0x40e537:`r1=0x405510(angle); r2=0x4054f0(angle)`,
  再 `X'=X·r2 − Y·r1`。标准旋转 `X'=X·cos − Y·sin` ⇒ **r2=cos、r1=sin** ⇒ **`0x405510`=sin、`0x4054f0`=cos**。
  (两个子 agent 对此**互相打架**,一个说反了;以此旋转式为准。)
- 包装体:`CVTSS2SD → CALL 核 → CVTSD2SS`;两核同形(`STMXCSR` 检精度 → `PEXTRW` 指数范围归约)= `__libm_sse2_sin`/`__libm_sse2_cos`。
- **可信**:✅(旋转式一手裁定)。注:`0x430df0`/`0x4054d0` 另用 x87 `FSINCOS`/`fcos·fsin` 内联,不经这俩包装。

### 1.5 `0x4052a0` = `atan2f`(float 入/出包装)✅
- **证据**(指令级):`CVTSS2SD ×2` 把两个 float 升 double 压 x87 栈 → `CALL 0x487aaa` → `CVTPD2PS` 收窄回 float。所有 call-site 两参 = `(src.y−dst.y, src.x−dst.x)=(dy,dx)`,返回当角度(送旋转或 `CVTTSS2SI` 取整方向)。→ `atan2(dy,dx)`。
- 参数序 `atan2(dy,dx)` vs `atan2(dx,dy)` 依赖 `_CIatan2(ST1,ST0)` 约定;call-site「朝目标的航向角」用法与 `atan2(dy,dx)` 一致。

### 1.6 `0x487aca` = `fmod`(_CIfmod)✅
- **决定性证据**(指令级,PE 字节实测):stub `MOV EDX,0x494940; JMP __cintrindisp2`;其 x87 thunk 在 `0x487ad4`:
  `FXCH; FPREM(0x487ad6 = D9 F8); FNSTSW…` = 教科书 x87 **fmod**(`FPREM` 循环求浮点余数)。**`FPREM` 排除 pow**(pow 用 `FYL2X/F2XM1`),`atan2` 已是 `0x487aaa`。
- **用法佐证**:`FUN_00417bc0` 中 `fmod` + `floor`(§1.3)= `floor(elapsed/period)` 式子帧/计分,常量 `0.00835≈½帧 / 0.0167≈1帧 @60fps`(§4 实测)。
- **可信**:✅(`FPREM` 一手 + 用法)。对抗证伪通过。

---

## 2. 角度 / 向量几何原语(算法深度)

> 公用常量(§4 实测):`f_PI=0x494588=π`、`f_2PI=0x4945b8=2π`、`f_negPI=0x494734=−π`。
> 归一化区间 **`[-π, π]`（两端都闭）**,迭代法(非 fmod),每个循环 **上限 34 次**作 runaway/NaN 护栏。
> ★ 2026-09-10 更正：原写「(−π, π]」「≤0x21≈33 次」两处都不对。守卫 `cmp eax,0x20; jg` 比的是**自增前**
> 的值,所以算术发生在 `old = 0..33` 共 **34** 步;出口判据是 `x <= π`(`comiss xmm0,xmm2; jbe`)与
> `-π <= x`(`comiss xmm3,xmm0; jbe`),**两端都闭**。另:两个循环共用计数器 `k`,不重置。
> 泄漏阈值 `|θ| ≲ 216.77`(= 69π),超过就原样漏出去、不报错。
> 完整推导与 float32 逐步模拟表见 [`../anm/th16/03-math-jumps.md`](../anm/th16/03-math-jumps.md) §7.1。

### 2.1 `0x402d90` = `normalize_angle(x) → (−π,π]` ✅(传值)
- `while(x>π) x−=2π;  while(x<−π) x+=2π;  return x`(XMM0 传入传出)。34 个 call-site,最常用的纯归一。指令级确认(`COMISS f_PI / SUBSS f_2PI` …)。

### 2.2 `0x4052e0` = `*out = normalize_angle(*this + Δ)` ✅
- `this`=当前角指针,Δ 由 XMM2 隐式传入(转向增量),结果写 `*out`。2 个 call-site(均 homing):atan2 目标角 − 当前角 → 最短弧 Δ → 本函数推进并归一 → `0x411410` 据新角重建速度向量。指令级确认。

### 2.3 `0x411410` = `*(this+0x1c) = normalize_angle(*(this+0x1c))` ✅
- 同款双 do/while 归一,**结果写回结构体 +0x1c(航向字段)**。8 call-site(homing/多种 actor 更新),`FUN_00410550` 内有两处同体内联 → 本函数是该归一的 out-of-line 共用版。指令级确认。

### 2.4 `0x430df0` = `set_velocity_from(angle, speed)`(极坐标→直角)✅
- **指令级铁证**:`FLD angle; FSINCOS; FMUL speed; FSTP this[0]; FMUL speed; FSTP this[4]` → `this[0]=cos(θ)·r, this[1]=sin(θ)·r`(`__thiscall`,只写 +0/+4 两个 float,不读全局)。5 call-site:道具/弹的「角+速→速度向量」,其中 state4 的 angle 正是 atan2 朝目标结果(= atan2 的逆运算),量纲自洽。
- **同族** `0x4054d0`:同义但用 x87 `fcos/fsin` 内联(`this[0]=cos·r, this[1]=sin·r`)。

### 2.5 运动积分器 `0x402ff0` / `0x403110` ✅(行为)
- 按 `mode = *(obj+0x40)&0xf` 分派的**每帧运动更新器**(非纯数学原语,但密集使用上述原语):
  - `0x402ff0`:case0 据角+速置速度(`0x4054d0`);case2/3 位置积分 + 角度归一;case4 另一套。
  - `0x403110`:更全(含 3 分量位置积分;case3 用 `0x405510/0x4054f0` 做 2D **旋转矩阵** `(x·c−y·s, x·s+y·c)`)。
- `DAT_004a5788` = 每帧 dt(=1.0f,§4),在调用方乘到输出上做时间步进。

---

## 3. PRNG(ZUN 16 位生成器)★ 已算法级完整解出

> 这是东方招牌的**确定性回放**核心:**整局随机性由一个 16 位种子决定**。模型脚本 `tooling/ghidra/scripts/th16_prng_model.py`(可复现,已跑通)。

### 3.0 状态结构 / 两条流
- **状态只有 16 位**(`XOR AX, word[ESI]` 等全是 16 位操作;高位 `MOVZX AX` 丢弃)。状态对象 = `{ +0: u16 state, +4: u32 draw_counter }`。
- **两条独立流,同算法异状态**:
  - **Stream A** = `DAT_004a6d88` / 计数器 `DAT_004a6d8c` —— **gameplay,回放关键**(float 包装全取它)。
  - **Stream B** = `DAT_004a6d80` / 计数器 `DAT_004a6d84` —— init/其他(`0x458db0` 启动时预热 256 步)。

### 3.1 核心递推(单步)✅ 指令级
一手反汇编 `0x449720`(`0x449782`–`0x4497a7`),**单步**:
```
t  = ((s ^ 0x9630) - 0x6553) & 0xFFFF     ; 非线性混合(XOR 常量 + 减常量)
s' = ((t << 2) | (t >> 14)) & 0xFFFF      ; = ROL16(t, 2)   状态前进
```
每单步 `counter += 1`。可选 `EnterCriticalSection(0x4c1048)`(`DAT_004c10b6` 置位时,线程安全)。

★**发射值因调用者而异**(bit-exact 关键):
- `0x449720`(缓冲填充,0x1ee 个)发射**新状态 `s'`**;`0x458db0`(Stream B)同样以状态为输出。
- `0x402be0`(**gameplay draw**)每次调用做**两单步**,counter+=2,**返回 32 位 = `(t1<<16)|t2`**(用两个**旋转前**的 `t` 值拼接),状态存 `r2=ROL16(t2,2)`。

### 3.2 算法性质(模型实算,✅)
- **`step()` 是 0..65535 上的满周期置换**:全部 65536 个状态构成**单一长度 65536 的环**,**无不动点、无短环、无坏种子**(若递推读错,环结构会退化——得到干净满周期反过来印证递推无误)。
- **gameplay 每 draw 前进 2 步** ⇒ 双步映射分裂成 **2 个长度 32768 的环** ⇒ **gameplay 序列周期 = 32768 draws**。
- **32 位输出 `(t1<<16)|t2` 是两次连续输出拼的**,真实熵只有 16 位、高低半字相关——作"32 位随机"质量弱,但对"固定种子可复现弹幕"而言**确定性 > 质量**,是有意为之。

### 3.3 种子与回放(用户关注点)✅
- **新局**:`0x43b520` 置 `state = (u16)timeGetTime()` ⇒ **新局只有 65536 种可能种子**。
- **回放**:`0x449030`/`0x447760` 从回放文件存的种子字**还原 state**,counter 清 0 ⇒ **整局逐帧确定性重放**。
- **计数器** `d8c/d84` = draw 次数,随回放存档/还原(本组函数里只见 INC/reset,未见现场 CMP;典型用于回放同步/desync 侦测,确切比对点未定位 🟡)。

### 3.4 播种 `0x43b520` ✅
- `t=timeGetTime()`(winmm 导入 `[0x48b24c]`);`DAT_004a6d88 = DAT_004a6d80 = (u16)t`(低 16 位)。亦置 `DAT_004a5788=1.0f`(帧 dt)。注册为 startup init 回调(`FUN_0043ba40` 把其地址塞进任务槽)。

### 3.5 存档/回放 `0x449030`(及同款 `0x447760`)✅(结构)/🟡(记录布局)
- mode0(新局):`DAT_004a6d80 := (u16)DAT_004a6d88`,清计数器,快照种子入 0x294 字节记录。mode1(载入回放):从存档 word 同时写 `DAT_004a6d88` 与 `DAT_004a6d80`,清计数器,`REP MOVSD 0x8a`(552 字节)参数块还原到 `0x4a5790`。
- **🟡**:0x294 记录与 552 字节参数块的**逐字段语义未全解**(只钉了种子 +2、角色 idx +0、回放标志位 +0x290)。回放=确定性重放,故 PRNG 必须可存档——与「seed=timeGetTime」自洽。

### 3.6 float 包装(取一个 32 位 raw 再线性映射)✅
> 三者都 `raw = 0x402be0(this)`(`this` = ECX 里传进来的 PRNG 对象指针,**哪条流由调用方决定**,见下面的调用约定),再无符号化(`u = (double)(uint32)raw`,经查表 `0x494850={0, 2³²}` 修正符号位)后线性映射。常量见 §4。

| 地址 | 公式 | 值域 | 用途 |
| --- | --- | --- | --- |
| `0x402cb0` | `u·2⁻³¹ − 1.0` | **[−1, 1)** | 对称单位随机(45 caller,最常用;乘幅度→角抖/位置/速度) |
| `0x402c70` | `u·2⁻³²` | **[0, 1)** | 单位随机(21 caller) |
| `0x402cf0` | `u/(2³¹/π) − π` | **[−π, π)** | 随机角度/方向(2 caller) |

- 调用约定:三个包装都是 **thiscall 单参**——`ECX` = PRNG 对象指针,函数体自己**不写 ECX**,原样转发给 `0x402be0`
  (`0x402c77` / `0x402cb7` / `0x402cf7`)。所以「是哪条流」**取决于调用点**,不是包装本身的属性。
  ⚠️ **本条初版写的「全部 `f(&DAT_004a6d88)` 单参(Stream A)」是错的**,已被本仓 ANM 侧的一手证据证伪:
  `AnmVm::get_float_var` 的 `0x45f3ef` 是 `mov ecx, 0x4a6d80` 紧跟 `0x45f3f9  call 0x402cb0`——
  **同一个包装被 Stream B(表现流)调用**。正确的表述是:**gameplay 侧**的调用点传 `&DAT_004a6d88`(Stream A);
  ANM 子系统的 16 个调用点里 **13 个走 Stream B `0x4a6d80`、3 个走 Stream A**(逐点穷举清单 + negative set 见
  [`../anm/th16/03-math-jumps.md`](../anm/th16/03-math-jumps.md) §7.4)。本节只改这一条宣称,其余未复核。
- `FUN_00424110` 是 ECL「random 系列 opcode」的 switch 派发(把这几个包装暴露给脚本层)。

---

## 4. 数学常量(★ 直接读 th16.exe PE 字节实测,✅)

> 读法:解析 PE 节表(`.rdata` VA=0x8b000/Ptr=0x8a200、`.data` VA=0x9d000/Ptr=0x9c000),VA−imagebase→文件偏移,`struct.unpack('<f'/'<d')`。脚本见 `tooling/ghidra/scripts/`(可复现)。

| 符号(建议名) | VA | 实测值 | = |
| --- | --- | --- | --- |
| `f_PI` | `0x494588` | 3.14159274 | **π** |
| `f_2PI` | `0x4945b8` | 6.28318548 | **2π** |
| `f_negPI` | `0x494734` | −3.14159274 | **−π** |
| `f_PI_2` | `0x494534` | 1.57079637 | **π/2** |
| `f_PI_4` | `0x4944c0` | 0.785398185 | **π/4** |
| `f_PI_12` | `0x494464` | 0.261799395 | **π/12**(15°) |
| `f_inv512` | `0x4943e8` | 0.001953125 | **1/512** |
| `f_inv128` | `0x4943f4` | 0.0078125 | **1/128** |
| `f_128` | `0x494644` | 128 | 屏幕坐标尺度 |
| `f_rng_2pow_m31` | `0x4943d4` | 4.65661e-10 | **2⁻³¹**(RNG [−1,1) 尺度) |
| `f_rng_2pow_m32` | `0x4943d0` | 2.32831e-10 | **2⁻³²**(RNG [0,1) 尺度) |
| `f_rng_2pow31_div_pi` | `0x494700` | 683565248 | **≈2³¹/π**(RNG 角度尺度) |
| `f_one` | `0x4944d8` | 1.0 | RNG −1.0 偏置 |
| `d_ufix[0]/[1]` | `0x494850` | 0.0 / 4294967296.0 | **{0, 2³²}** 无符号修正表(double) |
| `d_halfframe` | `0x4944e0` | 0.00835(double) | ≈½ 帧 @60fps(`0x487aca` 用) |
| `d_frame` | `0x494500` | 0.0167(double) | ≈1 帧 @60fps |
| `d_100` | `0x494598` | 100.0(double) | |

> ⚠️ 早期 `engine/sht/th16/06` §9 的常量表(标 0x494xxx)逐一与本实测一致(已抽查 π/12·π/4·π/2·1/512·1/128·128 全中),可信。

---

## 5. 开放问题 / 待办

> 对抗证伪已跑完;原 🟡 的 fmod、sin/cos 对名、floor 均已收敛为 ✅(见 §1)。剩余开放:

1. **PRNG**:算法已**完整解出**(单步递推、满周期 65536、gameplay 周期 32768、两条流、种子/回放机制),bit-exact 模型 `th16_prng_model.py` 已写并跑通。**唯一剩**:在**真机**抓一段 `draw32()` 输出对模型做 ground-truth 比对(Linux headless 跑不了 th16.exe → 留待 Windows/真机),以及 counter 的**现场 desync 比对点**定位 🟡。
2. **回放记录布局** 🟡:`0x449030`/`0x447760` 的 0x294/0x31c 字节记录 + 552 字节参数块逐字段语义未全解(只钉了种子 +2 / 角色 idx / "t16r" 魔数 / 回放标志位)。
3. **`0x458db0`** 是**一次性 init 例程**(字体枚举 + 把 Stream B 预热 256 步),非纯 stepper(已命名 `prng_init_warm_stream_b`,取其 RNG 侧面)。
4. **跨版本**:以上全部仅 TH16;迁 TH18/19 时地址/常量布局**必变**,但「迭代角度归一化、ROL16-PRNG、x87-2参/SSE2-1参 的 CRT 混合、极坐标速度」这些**模式**大概率沿用,可作锚点。

---

## 关联
- 入口锚点:`engine/sht/th16/06-th16-engine-incisions.md` §9(本表是其「数学起步包」的兑现 + 纠错)。
- 一手 SHT 上下文(homing/atan2 调用方):`engine/sht/th16/03-th16-funcstar-jumptables.md`。
- 落盘脚本:`tooling/ghidra/scripts/apply_th16_math_names.py`(函数名+数据符号名+证据注释,headless 可复现,**唯一能给数据符号真改名**的途径);`tooling/ghidra/scripts/th16_prng_model.py`(PRNG 参考模型 + 周期证明,跑通)。
- **Ghidra 工程状态**:24 个函数名 + 48 条证据注释已通过(已修复的)`ghidra-re` MCP **落盘**(跨 close/重开验证存活);数据全局因 MCP 无改名工具,DB 内为注释、符号名仍 `DAT_xxxx`(真改名走上面脚本)。
- 纪律:`METHOD.md`;memory `re-overclaim-guard` / `re-evidence-chain-discipline` / `ghidra-mcp-save-broken`。
