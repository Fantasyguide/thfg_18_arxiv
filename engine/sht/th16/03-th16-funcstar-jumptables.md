# 逆向工程发现 03:TH16 SHT func_* 跳转表破解(社区未解项,首次)
> **版本**：TH16 v1.00a（`th16.exe`，imagebase `0x400000`）。本文裸地址默认属该版本；引用其他版本须写成 `th18:0x…`。
>

> 方法:Ghidra(ghidra-re MCP)静态反汇编**用户自有 th16.exe**(32-bit PE,1927 函数) + 自写
> SHT 解析器交叉验证 pl00.sht/pl00sub.sht。日期 2026-06-08。
> 分级:✅高可信 / 🟡中可信(单遍推断,需对抗复核) / ❓未解。
> **意义**:`func_on_*` 的「索引→行为」表是社区公开未破解项(见 `02-*.md`)。本篇首次给出 **TH16
> 的机制(✅)+ 首遍行为表(idx0/1 ✅,idx2-6 🟡)**。仅适用 **TH16(天空璋)**,勿外推他作。
>
> **★ 2026-06-11 更新(见 §6)**:导入 ExpHP 符号后重做了一遍——解析器干净重译、**三表全 16 个表项
> 函数全部一手反编译/反汇编完毕(含两处裸码 init)**、补全表布局(null 填充 + hit 多一项)、常量全部
> 解出(±20°/±15°/加速 1.0·0.5/激光 18→512),并**修正 doc 内一处过度概括**("同一索引=一个行为包"
> 仅对 idx1/idx2 成立)。§2 的 idx2–6 由 🟡 升为 ✅(行为已一手坐实;"哪个角色用哪索引"仍依赖 §4)。

## 0. 一句话结论

TH16 把每个 shooter 的 `func_on_init/tick/_old_on_draw/hit`(文件里是小整数索引)在**加载时**用
**四张硬编码函数指针表**替换成真实函数地址;解析器、四张表地址、表项函数全部定位完毕。这正是
RUEEE 所说"load-time 替换成函数"模型的实证——**但作用在 func_* 上,不是 flags**。

## 1. SHT 解析器:`sht_parse_resolve_funcptrs` **`0x443790`** ✅

(原 `FUN_00443790`,已在 DB 重命名)由自机初始化 `player_shot_init` `0x440fb0` 对主/副 .sht 各调一次。
反编译要点(逐字段偏移与 `struct_16.js` **完全吻合**):

```c
base = load_file(".sht")            // FUN_00402440 读文件 → SHT 基址,存 *param_1
if (sht_off_cnt @0x02 != 0) {
  for (i = 0; i < sht_off_cnt; i++) {            // 遍历 shooterset
    off = base + 0x190 + i*4                     // sht_off 数组 @0x190 (=option_pos 0x40 + 0x150)
    *off += base + 0x1b8                          // 相对偏移重定位:shooter 数据区起点 0x1b8 = 0x190 + 0x0A*4
    shooter = *off
    while ((int8)shooter[0] >= 0) {               // fire_rate 字节;0xFF(负)= shooterset 终止符
      shooter.func_on_init @0x28 = TBL_init[ shooter.func_on_init ]   // ★ 索引→指针
      shooter.func_on_tick @0x2c = TBL_tick[ shooter.func_on_tick ]   // ★
      shooter._old_on_draw @0x30 = TBL_draw[ shooter._old_on_draw ]   // ★
      shooter.func_on_hit  @0x34 = TBL_hit [ shooter.func_on_hit  ]   // ★
      shooter += 0x58                             // per-shooter 步长 0x58 ✓
    }
  }
}
```

锚点全部实证:per-shooter 步长 **0x58**、func 字段偏移 **0x28/0x2c/0x30/0x34**、shooter 区起点
**0x1b8**(= `option_pos_len 0x150` + 头 0x40 → sht_off 0x190;+ `forced_shtoffarr_len 0x0A`×4 = 0x1b8)、
终止符 = fire_rate 字节高位置 1。**flags 段(@0x38,0x20 字节)在此循环中不被改写**——故 TH16 中"被
替换成函数指针"的是 func_*,与 flags 无关(修正 02 里对 RUEEE 模型的归属)。

## 2. 四张函数指针表(TH16,.rdata)✅地址 / 🟡部分行为

| 字段 | 表基址 | 有效索引 | 备注 |
| --- | --- | --- | --- |
| `func_on_init` | **`0x4919c0`** | 0–5 | idx0 = null |
| `func_on_tick` | **`0x4919a0`** | 0–5 | idx0 = null |
| `_old_on_draw` | `0x4a6f04` | (见注) | **TH16 不派发 draw** ✅(证据:解析器实测**所有 .sht 的 draw 索引=0**,只会读 entry0)。⚠️审计:`0x4a6f04` 在**可写 .data**、与无关全局重叠,**勿当"干净全 null 表"** |
| `func_on_hit`  | **`0x491980`** | 0–6 | idx0 = null;hit 多一项 idx6 |

**索引 0 = 空指针 = 无该回调**(默认/直线弹)。同一索引在三表里的函数地址相邻 → **索引选的是一个
"射击行为包"(init/tick/hit 成组)**。

### 索引→行为表(TH16)—— 角色/季节级 ground truth(解包全部 pl0X.sht + wiki 互证)

> ⚠️ **两次修正记录(2026-06-08,同日)**:
> 1. 第一版由并行 agent 产出,我在 prompt 里把"idx1=homing"当参考词汇、homing 列为候选首位,造成
>    **锚定偏置**,agent 把 idx2/3/5 误标 homing。改用硬判据(**tick 是否调 `find_nearest_enemy`/
>    读敌人 +0x1250**)自行重判修正。教训见 memory `re-agent-no-hypothesis-priming`。
> 2. 角色顺序我一度搞反。**正确**:pl00=灵梦、pl01=魔理沙、pl02=琪露诺、pl03=射命丸文。
>
> **TH16 是季节系统**:每角色绑定一季节,主弹=该季节弹,副弹(plXsub)=所选季节弱化版(故 sub 随
> **季节**而非角色)。解包 9 个 .sht 实际用到的 tick 索引 = {0,1,2,5}(3、4 在原版 pl 文件里无人用)。

| tick idx | 季节(角色 / 文件) | 反汇编行为(客观) | wiki 设定 | 读敌人 | 可信 |
| --- | --- | --- | --- | --- | --- |
| **0** | 夏(琪露诺 pl02 全部)+ 各主弹基底 | 空指针无回调,纯直线;**散射来自 SHT 几何字段(angle/count),非回调** | 冰晶大范围散射 | — | ✅ |
| **1** | 春(灵梦 pl00) | `0x445ee0`:`find_nearest_enemy`→atan2→**拧角 +0x64 朝敌**,同时**调速 +0x60**(对准加速/转向减速),59帧后停 | 追踪樱花瓣(focus/unfocus 近似) | ✅连续 | ✅✅ 解包+wiki |
| **2** | 冬(魔理沙 pl01) | `0x446260`:**激光**——option 位置定位 + 蓄力 **+0xa0** 拉长 + 端点几何,见下"激光变量" | 激光,随 power 散开/focus 收直 | ❌ | ✅✅ 解包+wiki+机制 |
| **5** | 秋(射命丸文 pl03) | `0x447480`:`if(state==1) **speed@+0x60 += 常量0x49449c**` —— **匀加速弹**(非曲率,见下★修正) | 穿刺疾风(加速前冲),两侧前向弹幕 | ❌ | ✅ 加速贴合"疾风";穿刺/barrage 靠几何 |
| 3 (未用) | — | `0x446e00`:`if(state==1) **speed@+0x60 += 常量0x4944d8**` **匀加速**,同 5 仅常量不同 | — | ❌ | 🟡 原版无人用 |
| 4 (未用) | — | `0x4470f0`:邻近锁定——敌入框则一次锁存敌坐标+加速 14.0f 冲刺 | — | ✅一次性 | 🟡 原版无人用(可能跨季节组合/dev) |

**on_init / on_hit(按角色实际用到)**:灵梦 init1(`0x445ed0`:清 homing 目标槽 +0x90)、魔理沙 init2
(`0x446200` 裸码,激光初始化)、射命丸文 init4(`0x447450` objdump:置池对象 +0xd110=2)。命中:魔理沙
hit2(`0x446870` 激光命中)、射命丸文 hit5(`0x447270`:RNG 角度扰动+经 `0x445e20` 发弹)。idx0 用户命中=0(无)。

### ★ 你要的"落实":追踪 / 激光 关键变量

- **追踪(homing,春/灵梦/idx1)** ✅:
  - 移动角 = **+0x64**(被逐帧拧向目标,FUN_004052e0);**速度 = +0x60**(对准加速/转向减速);
    homing 目标句柄槽 = **+0x90**(init 清零,tick 填);
  - 取目标:`find_nearest_enemy`(`0x425240`,半径内最近可击中敌,返回敌 +0x5740 句柄);
  - 求角:`atan2`(`0x487aaa`,用敌 +0x1250/+0x1254 − 自机坐标);转速上限常量 DAT_004945b8 / DAT_00494458;
  - 计时:`+0x10` 存活帧 > 0x3b(59)后停止追踪转匀速漂移。
- **激光(laser,冬/魔理沙/idx2)** ✅:
  - **长度/蓄力 = +0xa0**(每帧 += DAT_004945e4 直到上限 DAT_004946c0)→ 决定激光伸长;
  - **朝向角 = +0x64**(经大量归一化平滑);渲染束的延展量写到子对象 **+0x68** = `+0xa0 * _DAT_004943e8`;
  - **远端点** = `FUN_004476b0(out, 角@+0x64, 长 = +0xa0 * DAT_0049449c)` —— 激光束几何;
  - **束起点** = option 位置表 `DAT_004a6ef8 + 0x6bc`(stride 0xe4,按 SHT `option` 字节索引);
  - 持久束对象经 `FUN_0046efa0` 创建/更新,置标志位 |8(+0x70=蓄力)、|0x10(+0x68=延展)。

### 裸码缺口现状

- ✅ 已用 **objdump(pei-i386,按 VA 反汇编)** 补出:`0x447480`(tick5,文)、`0x447450`(init4,文)。
- ⏳ 仍未读:`0x446200`(init2,魔理沙激光初始化)——objdump 可补,优先级低(tick2 已足够定性激光)。
- MCP 的 decompile/disassemble 对"仅被指针表 DATA 引用、未建函数"的裸码会失败;**objdump 按地址直读
  字节是有效绕过**(无需 pyghidra 重分析)。

## 3. 辅助函数 / 字段偏移词汇表(本轮实证,复用价值高)✅/🟡

> 后续反编译任何自机/子弹相关函数都用得上。已在 DB 重命名的标注 ✓。
> 📌 **SHT shooter 结构(.sht 文件)完整字段图 0x00–0x57 + flags 段判定见 `05-th16-flags-no-runtime-read.md`**
> (内含修正:`+0x14`=角度种子 seed、`+0x20`=option/副弹 id;flags 0x38–0x57 经全程序穷举**运行时无人读**)。
> 下方词汇表是**运行时槽**(param_1+偏移),与文件 shooter 结构是两回事,勿混。

### 引擎辅助函数
| 地址 | 语义 | 可信 |
| --- | --- | --- |
| `find_nearest_enemy` ✓ **`0x425240`** | `(out, &refpos)`:遍历敌人链表,返回半径(XMM3 传入)内最近**可击中**敌人的句柄(敌+0x5740),无则 0 | ✅ |
| `atan2` `0x487aaa` ✓ | **已坐实 atan2**:stub→`__cintrindisp2`(VS2015 2 参 CRT 分派,内部 `__trandisp2` 对 ST0/ST1 双 FXAM=双参);调用方 homing `0x445fbe` 先压 dy 再压 dx、结果直接当朝敌角存 slot+0x64 → 双参反正切=atan2 | ✅ |
| `FUN_0041a980(handle)` | 校验敌人句柄是否仍存活(返回 0=已消失) | 🟡 |
| `FUN_0041b540(&handle)` | 句柄 → 敌人对象指针 | 🟡 |
| `FUN_00445e20` | **共享"发射/落地子弹"收尾**:设精灵、scale +0x50、拷坐标、角度缩放 +0x60、置发射标志 +0x8c=2 | 🟡 |
| `FUN_0046efa0 / FUN_0046f0b0` | 生成效果/粒子对象(带 type、scale) | 🟡 |
| `FUN_0040e5c0` | 注册渲染条目 | 🟡 |
| `FUN_0045e1f0(id) / FUN_0045e2a0(id)` | 按 id 播放音效/效果(如 hit SFX id=0x41) | 🟡 |
| `FUN_00402cb0(&DAT_...)` | 读游戏 PRNG | 🟡 |
| `FUN_00402440(name,…)` | 从 .dat 读取文件入内存(SHT 加载用) | ✅ |

### 自机射击/子弹结构字段(param_1 + 偏移)
`+0x10` 存活帧;`+0x12/0x13` 命中框;`+0x18` 速度;`+0x48/+0x4c` x/y 坐标;`+0x50` scale;
`+0x60` **速度(speed,=shooter+0x18)**;`+0x64` **移动/瞄准角(angle,=shooter+0x14)**;`+0x8c` **行为状态**(1=活动/发射,2=完成/空闲);
> ★ **修正(2026-06-08,经 sht-webedit 字段名 + homing/idx3/idx5 一手复核)**:先前把 `+0x60` 标成"移动角"
> 是**标反了**——`+0x60`=速度、`+0x64`=角。连带:idx3/idx5 的 `+0x60 += 常量` 实为**匀加速**(非"定曲率");
> Ghidra DB 里相关 plate 注释(`0x445ee0`/0x446e00/0x447480)同样需改。详见 `05-*.md` §2/§2b。
`+0x90` **homing 目标句柄槽**;`+0xa0` 蓄力/到达量;`+0xb0` 子弹/option 链接。

### 敌人结构字段(enemy + 偏移)
`+0x1250` **x**;`+0x1254` **y**;`+0x1258` z(疑);`+0x526c` **标志位**(掩码 `0xc000021` = 死亡/
无敌/不可锁定);`+0x5740` **句柄/id**。

### 关键全局
`DAT_004a6dc0` = 敌人管理器(`+0x180` 敌人链表头,`+0x15c` 另一链表);
`DAT_004a6ef8` = 子弹/效果对象池基址(`+0xd080` 子池,步长 0x94)。

## 4. 交叉验证(全部 9 个 pl0X.sht + wiki)✅

自写 TH16 SHT 解析器(`local/th16.v1.00a/` 本地跑,版权不入库)解出每文件实际用到的索引,与季节设定逐一吻合:

| 文件 | 角色/季节 | 用到的(init,tick,hit) | 对照 |
| --- | --- | --- | --- |
| pl00 / pl00sub | 灵梦 / 春 | (0,0,0)+(1,1,0) / (1,1,0) | idx1 homing ✅ |
| pl01 / pl01sub | 魔理沙 / 冬 | (0,0,0)+(2,2,2) / (2,2,2) | idx2 激光 ✅ |
| pl02 / pl02sub | 琪露诺 / 夏 | **全 (0,0,0)** | 纯直线,散射靠几何 ✅ |
| pl03 / pl03sub | 射命丸文 / 秋 | (0,0,5)+(0,5,0)+(4,5,0) / (0,5,0) | idx5 微曲 + init4 + hit5 🟡 |
| pl04sub | Extra 专用 | (0,0,0) | 直线 |

- 头部/偏移/终止符/步长全部与解析器和 `struct_16.js` 自洽;每角色用到的索引与其季节弹种一致 → **布局
  + 机制 + 语义三方互证**。
- 注:`_old_on_draw` 在全部文件均为 0(空),再证 TH16 不用 draw 回调。
- ⚠️ tick 索引 3、4 在原版文件中无人使用(可能为 dev 残留或未在基础文件出现的跨季节组合)。

## 5. 下一步

1. ✅ **补裸码缺口**(2026-06-11,§6):objdump 已补 `0x446200`(init2)/0x447450(init4);`0x447480`(tick5)
   已 Ghidra 反编译。三表全 16 项全部读完。
2. ✅ **对抗复核 idx2-6**(2026-06-11,§6):全表项一手反编译,行为坐实,idx2–6 升 ✅;命名已落 DB。
3. ✅ **解析 pl0X.sht**:见 §4(已做)。
4. ⏳ 稳定后:把"TH16 索引→行为表 + 字段偏移"以版本元数据形式回填 `docs/`,作 IDE 的 SHT 语义层。
5. 🟡 验证 TH16↔TH18/19 是否共用编号(若是,一次反汇编覆盖多作)——**未做,下一刀**。

## 来源 / 复核线索

- 一手:th16.exe 反汇编(用户自有);函数地址见上,可在 Ghidra DB `th16` 复核(已重命名若干)。
- 交叉:`struct_16.js`(布局)、`02-*.md`(社区模型)、本地 SHT 解析器(pl00/pl00sub)。

---

## 6. 更新(2026-06-11):导入 ExpHP 符号后重做 + 全表项一手坐实

> 触发:把 ExpHP th-re-data 的函数/静态名导入 Ghidra(`tooling/ghidra/import_th_re_data.py`,safe 模式
> applied=385)。解析器与调用方因此带上社区命名,语义一目了然;遂把三表 **全部 16 个表项函数**一手
> 反编译/反汇编一遍。本节是对 §1–§4 的**补全 + 一处修正**,可信度:行为体=✅(一手),角色归属=继承 §4。

### 6.1 解析器重译(带 ExpHP 名,更干净)✅

`sht_parse_resolve_funcptrs @ 0x443790` 重译,机制与 §1 完全一致,只是名字清楚了:

```c
puVar4 = reads_file_into_new_allocation_402440(sht_filename, 0, 0);  // 读 .sht 入内存
*param_1 = (int)puVar4;
if (puVar4 == 0) return 0xffffffff;
if (*(short*)(puVar4 + 2) != 0) {                 // sht_off_cnt @0x02
  iVar5 = 400;                                     // 0x190 = sht_off 数组
  do {
    piVar1 = (int*)(iVar5 + *param_1);
    *piVar1 += *param_1 + 0x1b8;                   // 相对→绝对 重定位
    pcVar3 = *(char**)(iVar5 + *param_1);
    while (-1 < *pcVar3) {                         // fire_rate 字节 >=0(0xFF=终止)
      pcVar3[0x28] = sht_func_init_table       [pcVar3[0x28]];   // ★ 索引→指针
      pcVar3[0x2c] = sht_func_tick_table       [pcVar3[0x2c]];
      pcVar3[0x30] = sht_func_draw_table_UNUSED[pcVar3[0x30]];
      pcVar3[0x34] = sht_func_hit_table        [pcVar3[0x34]];
      pcVar3 += 0x58;                              // per-shooter 步长
    }
    iVar5 += 4;
  } while (++i < sht_off_cnt);
}
```

**无越界检查**:解析器直接拿字段整数当下标乘 4。表的"有效项数"= ZUN 实际填了几项(见 6.2)。

**调用方 `player_shot_init @ 0x440fb0`(ExpHP 名坐实选择链)✅**:每次自机初始化对主/副 .sht 各调
解析器一次,文件名来自——
- **主 .sht** = `PLAYER_SHT_FILENAMES[SUBSHOT__ZERO_IN_TH16 + CHARACTER]` → **按角色**选(`SUBSHOT__ZERO_IN_TH16`
  在 TH16 恒 0,ExpHP 名已言明);
- **副 .sht** = `(&PTR_s_pl00sub_sht_00492cb8)[SUBSEASON]` → **按季节**选(印证 §4"sub 随季节而非角色")。
- **快路**:`CACHED_PLAYER_SHT_FILE` 非 0 时复用上次解析结果、跳过重读(中途换装/续关用)。
- 紧接着用 `CHARACTER_HITBOX_SIZE_TABLE / GRAZEBOX / ATTRACTBOX_*` 按 `CHARACTER` 装判定框 →
  这些 ExpHP 静态名与我们 Phase 3 的独立结论一致(强交叉验证)。

### 6.2 三表完整布局(.rdata,PE 字节直读)✅

三表在 .rdata **连续排布、各自 null 起头并 null 尾填对齐**;`func_on_hit` 比另两张多一项(idx6):

```
hit   @0x491980 : [0]=NULL [1]=4460c0 [2]=446870 [3]=446e20 [4]=446f80 [5]=447270 [6]=447320   (7 项)
tick  @0x4919a0 : [0]=NULL [1]=445ee0 [2]=446260 [3]=446e00 [4]=4470f0 [5]=447480 ..pad..        (6 项)
init  @0x4919c0 : [0]=NULL [1]=445ed0 [2]=446200 [3]=4470e0 [4]=447450 [5]=4474d0 ..pad..         (6 项)
draw  @0x4a6f04 : 不派发(所有 .sht 的 draw 索引=0;且该地址在可写 .data,勿当干净表)
```

**idx0 恒 = 空指针 = 无该回调。**

### 6.3 ★ 修正:索引**不是**"成组的行为包"(只 idx1/idx2 碰巧成组)

§2 曾说"同一索引在三表里函数地址相邻 → 索引选一个 init/tick/hit 成组的行为包"。把 16 个函数**按地址
排序**后这只对 idx1/idx2 成立:

```
445ed0 init1 · 445ee0 tick1 · 4460c0 hit1     ← idx1 三件挨在一起(春/灵梦:寻的包)
446200 init2 · 446260 tick2 · 446870 hit2     ← idx2 三件挨在一起(冬/魔理沙:激光包)
446e00 tick3 · 446e20 hit3 · 446f80 hit4 · 4470e0 init3 · 4470f0 tick4 · 447270 hit5 · 447320 hit6 · 447450 init4 · 447480 tick5 · 4474d0 init5
                                              ← idx3/4/5 的 init/tick/hit 地址**不挨着**
```

**根因**:.sht 里每个 shooter 的 init/tick/hit 三个索引**互相独立**,可任意组合。§4 实测 pl03(文/秋)
就用了 `(init4, tick5, hit0)`、`(init0, tick5, hit0)`、`(init0, tick0, hit5)` 等**跨"包"组合**。所以
正确说法是:**`func_on_init/tick/hit` 是三套独立的行为选择器,各有自己的索引空间**;只有最简单的角色
(灵梦全用 idx1、魔理沙全用 idx2)才表现得像"成组"。

### 6.4 全 16 表项行为(全部一手反编译/反汇编)

> **可信度分层(本节自审,2026-06-11)**:
> - **行为体**(函数做了什么)= ✅ **一手反编译/反汇编**(机械事实)。
> - **"原版用"列** = **继承 §4 的 .sht 枚举,本次未重跑解析器**(init∈{0,1,2,4}、tick∈{0,1,2,5}、hit∈{0,2,5});§4 错则此列错。
> - **字段名**(`+0x60=speed`、`+0x02=dmg`、`+0xa0=charge`、`+0xb0=link` 等)= 沿用 `05-*.md`/§3 **已坐实词汇,本次未逐个重验**。其中 `+0x02=dmg` 见 `05-*.md`(✅,且 05 亲自反编译 `0x446e20`/f80 确认仅读 +0x02 当伤害)。
> - 名 = 已落 Ghidra DB 的描述性名(只描述代码动作,不夹推测用途)。

| 表/idx | 地址 | DB 名 | 行为(一手) | 原版用 |
| --- | --- | --- | --- | --- |
| init1 | `0x445ed0` | (§2) | 清寻的目标槽 +0x90 | 灵梦/春 |
| init2 | `0x446200` | 裸码 | **激光 init**:清蓄力 +0xa0、调 `0x45e1f0(id=0x14)`(🟡 doc03 记为"按 id 播音效/效果",未确证是 SFX)、初始化链接池对象(+0xb0)、清 flag bit0 | 魔理沙/冬 |
| init3 | `0x4470e0` | `playershot_init_clearflags_idx3` | `*this &= ~0x3c`(清 flag bit2–5)+ 清 +0x90 | — |
| init4 | `0x447450` | 裸码 | 置链接池对象 `+0xd110 = 2`(经 +0xb0 索引,DAT_4a6ef8 池基址) | 文/秋 |
| init5 | `0x4474d0` | `playershot_init_anglejitter_idx5` | 发射角 +0x64 **±15°** 抖动(`0.01745*15`,**REPLAY_SAFE_RNG**) | —(base 未用) |
| tick1 | `0x445ee0` | (§2) | **寻的**:find_nearest_enemy→atan2→拧角 +0x64 + 调速 +0x60,59 帧后停 | 灵梦/春 |
| tick2 | `0x446260` | (§2) | **激光**:蓄力 +0xa0 每帧 +18 至上限 512、端点几何 | 魔理沙/冬 |
| tick3 | `0x446e00` | `playershot_tick_accel_idx3` | `if(state==1) speed+0x60 += 1.0`(匀加速,常量 DAT_4944d8=**1.0**) | — |
| tick4 | `0x4470f0` | `playershot_tick_enemylock_dash_idx4` | **锁敌→水平冲刺**(反汇编+游戏内双验,2026-06-11):扫 ENEMY_MANAGER 链,锁定**与子弹同高(±16px)、横向≥16px** 的敌人 → flag→4、速度→0、存敌坐标 +0xb4、重置 timer(+0x20)→ **锁后第 4 帧**(timer.cur +0x24==4):把移动角 +0x64 **掰成纯水平**(敌在右=0,敌在左=−π 常量0x494734)+ 速度 **14.0** 冲刺,flag→8。净效果=升到敌人那一行→横切平推 | —(锁定弹,base 未用;配 init3) |
| tick5 | `0x447480` | 裸码 | `if(state==1) speed+0x60 += 0.5`(匀加速,常量 DAT_49449c=**0.5**) | 文/秋(疾风渐加速) |
| hit1 | `0x4460c0` | `playershot_hit_spread_spawn_idx1` | 命中:角 ±**20°**(0.349rad)抖 + 生成效果 type 0x98 + 随机 4 色字节(+0x520..523)+ launch_shared | —(base 未用) |
| hit2 | `0x446870` | (§2) | 激光命中 | 魔理沙/冬 |
| hit3 | `0x446e20` | `playershot_hit_dmgsrc_child_idx3` | 命中:**读该 shooter 的 dmg(short@+2,经自机已载 .sht 的 sht_off[+0x190];+0x02=dmg 见 `05-*.md` ✅)** → `Player__create_damage_source`(ExpHP 名,arg=dmg) → 生成池子弹(+0xd080,步长 0x94,子速 0.3)+ 拷运动学 + SFX 0x41(`SoundManager__play_sound`,ExpHP 名) | — |
| hit4 | `0x446f80` | `playershot_hit_dmgsrc_child_idx4` | 同 hit3 家族,子速 +0x60=**2.0**、子 +0x80=4 | — |
| hit5 | `0x447270` | `playershot_hit_spread_spawn_idx5` | 命中:角 ±20° 抖 + 生成效果 type 0x98 + launch_shared(idx1 的精简版,无染色) | 文/秋 |
| hit6 | `0x447320` | `playershot_hit_spread_spawn_idx6` | 同 hit5 + AnmVm scale-time 3.0→1.0/0x14 帧 + 随机 3 色字节 | — |

**家族归纳**:hit 表其实是两族——**{1,5,6}=命中处散射生成一发偏角子弹**(launch_shared,差异在染色/缩放),
**{3,4}=命中处按 shooter 的 dmg 造伤害源 + 从池生成派生子弹**(差异在子弹速度/标志)。init/tick 的
{3,4,5} 是匀加速 / 锁敌冲刺 / 角度抖动等独立小行为。

### 6.5 关键常量(PE 字节坐实)

| 符号 | 值 | 用途 |
| --- | --- | --- |
| `float_0_34906584` | 0.349066 rad = **20°** | hit1/5/6 散射偏角幅度(±) |
| `0.017453292 × 15` | **15°** | init5 角度抖动幅度(±) |
| `DAT_004944d8` | **1.0** | tick3 每帧加速度 |
| `DAT_0049449c` | **0.5** | tick5 每帧加速度(文/秋,比 tick3 缓) |
| `DAT_004945e4` / `DAT_004946c0` | **18 / 512** | tick2 激光蓄力增量 / 上限 |

### 6.6 落盘 / 复核

- DB:9 个函数已重命名 + 加 [TH16] plate 注释(其中 `0x446e00` **修正了一处过时错名** `..curve..`→`..accel..`,
  旧注释还把 +0x60 当"角",实为速度,见 `05-*.md`)。两处裸码 init(`0x446200`/0x447450)Ghidra 未建函数,
  无法 rename,语义见上表 + 本节。
- 复核入口:Ghidra DB `th16`,地址见 6.2;常量可用 `local/th16.v1.00a/` 本地 PE 字节复核(脚本式 struct.unpack)。

### 6.7 激光(idx2)一手复核(2026-06-11)—— §2 旧 🟡 升 ✅ + 补充

`playershot_tick_laser_idx2 @ 0x446260` + 命中 `0x446870` 一手反编译(ExpHP 名导入后)。§2"激光变量"
的要点(蓄力 +0xa0 每帧 +18 封顶 512、束长=蓄力×0.5、起点 option 表 stride 0xe4、束端点 `cartesian_from_polar_4476b0`)
**全部一手坐实,升 ✅**。新增/澄清:

1. **起点锚定子机**:每帧把激光 pos `+0x48/+0x4c` 覆写为 **option 位置表**(`PLAYER+0x6bc`,stride `0xe4`,
   按 shooter `option`(+0x20)字节 −1 索引;≥100 走另一段表)。→ 激光根部跟随子机移动,非"发射即脱手"。
2. **方向 +0x64 平滑摆向目标角**(带死区 + `×0.1`):**非聚焦 → 目标 = shooter 的 `.sht` angle(+0x14)= 散开**;
   **聚焦(`PLAYER+0x165c8≠0`)→ 目标 = −0.5π(正上)= 收拢竖直**。**全程不读敌人 → 方向不追踪**(纯几何)。
   → 这给 `07` 的 🟡"聚焦标志=`+0x165c8`"添了**独立第二证**(激光聚焦收拢竖直)。
3. **anm 束体**:蓄力归一化(×1/512)写 `+0x68`、原值写 `+0x70`,置 vm flag `+0x530 |8|0x10` 驱动视觉伸长。
4. **命中 hit2(`0x446870`)= 激光↔敌人几何求交**:把长度 `+0xa0` **截断到接触点**(`dist − 偏移`),
   激光视觉停在所打敌人身上 + 接触点喷火花(`AnmLoaded__create_40e5c0`)。retail:魔理沙/冬(pl01)用。
5. **激活逻辑**:尾部一大段按 `CURRENT_POWER/POWER_PER_LEVEL` + 状态 + per-option 表(`+0x597e`)决定该激光
   开/关;关闭时 `+0x8c=2` + anm interrupt(1) + 效果 `0x45e2a0(0x14)`。
- 落盘:`0x446260` 已重命名 `playershot_tick_laser_idx2` + plate 注释;`0x4476b0` → `cartesian_from_polar_4476b0`。
