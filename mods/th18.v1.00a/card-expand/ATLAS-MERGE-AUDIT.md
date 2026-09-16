# ATLAS-MERGE-AUDIT —— `ability.anm` 图集合并审计（系统卡 106–117）

> **版本**：TH18 v1.00a（`th18.exe`，imagebase `0x400000`）。本文裸地址默认属该版本；引其它版本写成 `th16:0x…`。
> **范围**：**只**审计系统卡 106–117（红魔乡…鬼形兽）用到的 `ability.anm` 贴图与脚本。
> 别人的卡（id 58–80）只当**既成事实**算预算：不评价、不改、不重排它们的内容。
> **性质**：**只读审计** —— 本轮**没有改任何既有文件**，也没有跑 `make anm` / `make release`（生成物一律没动）。
> **前置**：`AUDIT.md` §CC（anm entry 序号只能到 255，一手 + 实跑合证）、§CD-1（一个 entry 多 sprite 引擎支持，限制是我们自己加的）。
> **一手地址**：`th18:0x477b00`（`AnmLoaded__set_sprite`）、`th18:0x481703`（`scale.x` 乘的是 sprite 矩形宽）、
> `th18:0x4865b0`（加载器一次性分配文件级 sprite 表）。

## 0. 摘要

> ⚠ **本文是 2026-09-13 的审计 + 提案**；**实装结果与逐项验证据 §9**（实装与本提案的两处偏差：
> 108 妖侧本体那 2 个图集按用户裁决不进图集、多出一个 1×1 的退役容器 ⇒ **entry 48 而不是 49**、
> sprite **415 而不是 469**）。下表的"合并后"一列是**提案值**，实装后的现行值见 §9.2。

| 指标 | 现在（合并前）| 合并后（本文提案）| 实装后（**现行**，§9.2）|
| --- | --- | --- | --- |
| `ability.anm` entry 总数 | **367** | **49** | **48** |
| 其中追加 entry | 360 | **42**（23 个图集 + 别人的卡 19 个单图） | **41**（21 个图集 + 19 单图 + 1 退役容器）|
| sprite 总数 | 469 | 469（**数量不变**，号全部重排一次） | **415**（少 55 张退役图 + 1 张 1×1 容器）|
| script 总数 | 262 | 262（**一个都不动**） | **262**（一个都没动 ✅）|
| entry ≥ 256（贴图必坏）| **111** | **0** | **0** ✅ |
| entry 余量（上限 256）| 负 111 | 206 | **208** |
| 系统卡 106–117 占用的贴图 | 317 张 | 317 张（**一张不删**）| 262 张进图集 + **55 张留在原地不进图集**（源文件一张没删）|
| 图集画布带来的体积 | — | +0.33 MB（9.47 MB → 约 9.8 MB）| 9.47 MB → **9.28 MB**（退役 55 张省下的比画布空白多）|

三条最要紧的发现：

1. **坏掉的是 111 个 entry，全部落在 107/108/110/111/112 五张卡上**：107 两张（`SENRO_FIELD`/`SENRO_RING`）、
   108 **九十四张**、110 十一张、111 三张、112 一张（`UFO_ARC_STRIP`）。
   113 已经被「往前挪」救过（24 个 entry 全部 < 256）✅，**但那违反「只追加」**，本次一并复原。
2. **合并只需要 23 个图集**（系统卡 21 + 共享 HUD 2），entry 从 367 压到 **49** —— 离 255 有 206 个 entry 的余量，
   而且**比原生最大的 `abcard.anm`（118 entry）还小** ⇒ 本方案对「上限到底是多少」这件事**不敏感**。
3. **成员名一个都不用改**：图集只把「一行 = 一个 entry」换成「一段 = 一个 entry、段内一行 = 一个成员」，
   于是 `anm_ids.h` 里 `CE_ANM_ABILITY_SPRITE_<名字>` 仍然逐名存在、脚本里的 `@NAME` 仍然能用
   ⇒ **卡侧 C 代码本次合并一条都不用改**（唯一「建议改」的是 `senro.c:153-154` 两条裸脚本号，见 §5.3）。

三条最不确定的事（详见 §7）：108 那 55 张「用不上」的妖侧本体图怎么处置；别的卡的未合并分支会不会撞重排；
以及我**没有实跑**（所有结论来自 `thanm -l` 回读 + 源码图）。
## 1. 现状对账（`thanm -l` 回读构建产物，不是看生成器日志）

```
$ local/vendor/thtk/build/thanm/thanm -l 18 mods/th18.v1.00a/card-expand/native/build/ability.anm \
      -m local/vendor/eclmap/anmmap/v8.anmm
entries 367 (0..366)   sprites 469 (0..468)   scripts 262 (0..261)
```

原生基线（`local/th18.v1.00a/anm/ability/ability.anm.txt`）：**7 entry / 109 sprite（0..108）/ 68 script**。
追加部分的编号规则（`assets/build_ability.py:48` 的 `plan()`）：

```
ORDER.txt 第 k 个数据行  →  entry = 7 + k     sprite = 109 + k
```

⇒ **entry ≥ 256 ⟺ 数据行 k ≥ 249 ⟺ sprite ≥ 358**。数据行第 250…360 行（共 **111** 行）现在贴图必错。
`AUDIT.md` §CC-2 已实跑证实过这条线（`TRANCE_GAUGE_FRAME_A` 从 entry359 → entry25 之后「吃灵指示正常了」）。

### 1.1 111 个坏 entry 落在谁头上

| 卡 | 该卡 ability 贴图 | entry ≥ 256 | 其中同时"无引用"的 |
| --- | --- | --- | --- |
| **107** `senro.c` | 4 | **2**（`SENRO_FIELD` entry304、`SENRO_RING` entry305）| 0 |
| **108** `youkai.c` | 109 | **94**（`YOUKAI_ALICE_S15` entry256 起，到 `YOUKAI_OPT_AYA_S39` entry365）| 40 |
| **110** `mof.c` | 12 | **11**（`MOF_FAITH_ICON` entry306 … `MOF_FAITH_DIGIT_9` entry316）| 0 |
| **111** `signal.c` | 3 | **3**（`SIGNAL_GAUGE_*` entry317–319）| 0 |
| **112** `ufo.c` | 29 | **1**（`UFO_ARC_STRIP` entry366）| 0 |
| **113** `trance.c` | 27 | 0 ✅（§CC-2 挪序救回）| 0 |
| 106 / 109 / 114 | 0 | 0 | 0 |
| **115** `lolk.c` | 8 | 0 | 0 |
| **116** `season.c` | 43 | 0 | 0 |
| **117** `animal.c` | 82 | 0 | 0 |
| 共享（`sdk.c`）| 14 | 0 | 0 |
| 别人的卡（58–80）| 29 | 0 | 0 |
| **合计** | **360** | **111** | 40 |

> 「无引用」的判据与死条目清单见 §3。注意 107 的 `SENRO_FIELD/RING` **不是**死图：
> `senro.c:153-154` 用**裸数字 204/205** 起它们（§5.2 #1）。

## 2. 系统卡逐卡清单（每张卡一节）

> 读法：名称后的 `☠` = 无引用（死条目，见 §3.2）；`≥256` 列 = 现在贴图必坏。
> 「引用（脚本 / C 行）」= 哪些 ability 脚本会画到这张图 + 卡代码里**直接**拿它当 sprite 的那一行。
> 106 / 109 / 114 三张卡在本层**什么都没有**，也一并列出以便对账。

### 2.1 卡 106 th06rank.c（0 张 ability 贴图）

本卡**不使用** `ability.anm` 的任何贴图或脚本（全库 grep `CE_ANM_ABILITY_` 在本文件里 0 命中）。

### 2.2 卡 107 senro.c（4 张 ability 贴图）

- 现在坏掉（entry ≥ 256）：**2 / 4**；无引用（死条目）：**2 / 4**。
- 卡代码直接起的脚本（宏 → 号）：无

| 贴图 | entry | sprite | 声明 WxH | 源 PNG 实测 | 引用（脚本 / C 行）| ≥256 |
| --- | --- | --- | --- | --- | --- | --- |
| `SAKURA_POINT` ☠ | 238 | 340 | 16x16 | 16×16 | script 186 | 否 |
| `SAKURA_SMALL` ☠ | 239 | 341 | 16x16 | 16×16 | script 187 | 否 |
| `SENRO_FIELD` | 304 | 406 | 256x256 | 256×256 | script 204 | **是** |
| `SENRO_RING` | 305 | 407 | 64x64 | 64×64 | script 205 | **是** |

### 2.3 卡 108 youkai.c（109 张 ability 贴图）

- 现在坏掉（entry ≥ 256）：**94 / 109**；无引用（死条目）：**55 / 109**。
- 卡代码直接起的脚本（宏 → 号）：`script206`（`youkai.c:320`）、`script207`（`youkai.c:321`）、`script208`（`youkai.c:322`）、`script209`（`youkai.c:323`）、`script224`（`youkai.c:106`）、`script225`（`youkai.c:111`）、`script226`（`youkai.c:116`）、`script227`（`youkai.c:121`）

| 贴图 | entry | sprite | 声明 WxH | 源 PNG 实测 | 引用（脚本 / C 行）| ≥256 |
| --- | --- | --- | --- | --- | --- | --- |
| `YOUKAI_YUKARI_S11` ☠ | 241 | 343 | 32x64 | 32×64 | script 189,191,193 | 否 |
| `YOUKAI_YUKARI_S12` ☠ | 242 | 344 | 32x64 | 32×64 | script 189,191,193 | 否 |
| `YOUKAI_YUKARI_S13` ☠ | 243 | 345 | 32x64 | 32×64 | script 189,191,193 | 否 |
| `YOUKAI_YUKARI_S14` ☠ | 244 | 346 | 32x64 | 32×64 | script 189,191,193 | 否 |
| `YOUKAI_YUKARI_S15` ☠ | 245 | 347 | 32x64 | 32×64 | script 190,191,192,193 | 否 |
| `YOUKAI_YUKARI_S16` ☠ | 246 | 348 | 32x64 | 32×64 | script 190,191,192,193 | 否 |
| `YOUKAI_YUKARI_S17` ☠ | 247 | 349 | 32x64 | 32×64 | script 190,191,192,193 | 否 |
| `YOUKAI_YUKARI_S18` ☠ | 248 | 350 | 32x64 | 32×64 | script 190,191,192,193 | 否 |
| `YOUKAI_YUKARI_S19` ☠ | 249 | 351 | 32x64 | 32×64 | script 190,192 | 否 |
| `YOUKAI_YUKARI_S20` ☠ | 250 | 352 | 32x64 | 32×64 | script 190,192 | 否 |
| `YOUKAI_YUKARI_S21` ☠ | 251 | 353 | 32x64 | 32×64 | script 190,192 | 否 |
| `YOUKAI_ALICE_S11` ☠ | 252 | 354 | 32x64 | 32×64 | script 194,196,198 | 否 |
| `YOUKAI_ALICE_S12` ☠ | 253 | 355 | 32x64 | 32×64 | script 194,196,198 | 否 |
| `YOUKAI_ALICE_S13` ☠ | 254 | 356 | 32x64 | 32×64 | script 194,196,198 | 否 |
| `YOUKAI_ALICE_S14` ☠ | 255 | 357 | 32x64 | 32×64 | script 194,196,198 | 否 |
| `YOUKAI_ALICE_S15` ☠ | 256 | 358 | 32x64 | 32×64 | script 195,196,197,198 | **是** |
| `YOUKAI_ALICE_S16` ☠ | 257 | 359 | 32x64 | 32×64 | script 195,196,197,198 | **是** |
| `YOUKAI_ALICE_S17` ☠ | 258 | 360 | 32x64 | 32×64 | script 195,196,197,198 | **是** |
| `YOUKAI_ALICE_S18` ☠ | 259 | 361 | 32x64 | 32×64 | script 195,196,197,198 | **是** |
| `YOUKAI_ALICE_S19` ☠ | 260 | 362 | 32x64 | 32×64 | script 195,197 | **是** |
| `YOUKAI_ALICE_S20` ☠ | 261 | 363 | 32x64 | 32×64 | script 195,197 | **是** |
| `YOUKAI_ALICE_S21` ☠ | 262 | 364 | 32x64 | 32×64 | script 195,197 | **是** |
| `YOUKAI_REMILIA_S11` ☠ | 263 | 365 | 64x64 | 64×64 | script 199,201,203 | **是** |
| `YOUKAI_REMILIA_S12` ☠ | 264 | 366 | 64x64 | 64×64 | script 199,201,203 | **是** |
| `YOUKAI_REMILIA_S13` ☠ | 265 | 367 | 64x64 | 64×64 | script 199,201,203 | **是** |
| `YOUKAI_REMILIA_S14` ☠ | 266 | 368 | 64x64 | 64×64 | script 199,201,203 | **是** |
| `YOUKAI_REMILIA_S15` ☠ | 267 | 369 | 64x64 | 64×64 | script 200,201,202,203 | **是** |
| `YOUKAI_REMILIA_S16` ☠ | 268 | 370 | 64x64 | 64×64 | script 200,201,202,203 | **是** |
| `YOUKAI_REMILIA_S17` ☠ | 269 | 371 | 64x64 | 64×64 | script 200,202 | **是** |
| `YOUKAI_REMILIA_S18` ☠ | 270 | 372 | 64x64 | 64×64 | script 200,202 | **是** |
| `YOUKAI_REMILIA_S19` ☠ | 271 | 373 | 64x64 | 64×64 | script 200,202 | **是** |
| `YOUKAI_FAM_S53_0` | 272 | 374 | 32x32 | 32×32 | script 206 | **是** |
| `YOUKAI_FAM_S53_1` | 273 | 375 | 32x32 | 32×32 | script 206 | **是** |
| `YOUKAI_FAM_S53_2` | 274 | 376 | 32x32 | 32×32 | script 206 | **是** |
| `YOUKAI_FAM_S53_3` | 275 | 377 | 32x32 | 32×32 | script 206 | **是** |
| `YOUKAI_FAM_S53_4` | 276 | 378 | 32x32 | 32×32 | script 206 | **是** |
| `YOUKAI_FAM_S53_5` | 277 | 379 | 32x32 | 32×32 | script 206 | **是** |
| `YOUKAI_FAM_S53_6` | 278 | 380 | 32x32 | 32×32 | script 206 | **是** |
| `YOUKAI_FAM_S53_7` | 279 | 381 | 32x32 | 32×32 | script 206 | **是** |
| `YOUKAI_FAM_S54_0` | 280 | 382 | 32x32 | 32×32 | script 207 | **是** |
| `YOUKAI_FAM_S54_1` | 281 | 383 | 32x32 | 32×32 | script 207 | **是** |
| `YOUKAI_FAM_S54_2` | 282 | 384 | 32x32 | 32×32 | script 207 | **是** |
| `YOUKAI_FAM_S54_3` | 283 | 385 | 32x32 | 32×32 | script 207 | **是** |
| `YOUKAI_FAM_S54_4` | 284 | 386 | 32x32 | 32×32 | script 207 | **是** |
| `YOUKAI_FAM_S54_5` | 285 | 387 | 32x32 | 32×32 | script 207 | **是** |
| `YOUKAI_FAM_S54_6` | 286 | 388 | 32x32 | 32×32 | script 207 | **是** |
| `YOUKAI_FAM_S54_7` | 287 | 389 | 32x32 | 32×32 | script 207 | **是** |
| `YOUKAI_FAM_S55_0` | 288 | 390 | 32x32 | 32×32 | script 208 | **是** |
| `YOUKAI_FAM_S55_1` | 289 | 391 | 32x32 | 32×32 | script 208 | **是** |
| `YOUKAI_FAM_S55_2` | 290 | 392 | 32x32 | 32×32 | script 208 | **是** |
| `YOUKAI_FAM_S55_3` | 291 | 393 | 32x32 | 32×32 | script 208 | **是** |
| `YOUKAI_FAM_S55_4` | 292 | 394 | 32x32 | 32×32 | script 208 | **是** |
| `YOUKAI_FAM_S55_5` | 293 | 395 | 32x32 | 32×32 | script 208 | **是** |
| `YOUKAI_FAM_S55_6` | 294 | 396 | 32x32 | 32×32 | script 208 | **是** |
| `YOUKAI_FAM_S55_7` | 295 | 397 | 32x32 | 32×32 | script 208 | **是** |
| `YOUKAI_FAM_S56_0` | 296 | 398 | 32x32 | 32×32 | script 209 | **是** |
| `YOUKAI_FAM_S56_1` | 297 | 399 | 32x32 | 32×32 | script 209 | **是** |
| `YOUKAI_FAM_S56_2` | 298 | 400 | 32x32 | 32×32 | script 209 | **是** |
| `YOUKAI_FAM_S56_3` | 299 | 401 | 32x32 | 32×32 | script 209 | **是** |
| `YOUKAI_FAM_S56_4` | 300 | 402 | 32x32 | 32×32 | script 209 | **是** |
| `YOUKAI_FAM_S56_5` | 301 | 403 | 32x32 | 32×32 | script 209 | **是** |
| `YOUKAI_FAM_S56_6` | 302 | 404 | 32x32 | 32×32 | script 209 | **是** |
| `YOUKAI_FAM_S56_7` | 303 | 405 | 32x32 | 32×32 | script 209 | **是** |
| `YOUKAI_AYA_S0` ☠ | 320 | 422 | 32x64 | 32×64 | script 218,220,222 | **是** |
| `YOUKAI_AYA_S1` ☠ | 321 | 423 | 32x64 | 32×64 | script 218,220,222 | **是** |
| `YOUKAI_AYA_S2` ☠ | 322 | 424 | 32x64 | 32×64 | script 218,220,222 | **是** |
| `YOUKAI_AYA_S3` ☠ | 323 | 425 | 32x64 | 32×64 | script 218,220,222 | **是** |
| `YOUKAI_AYA_S4` ☠ | 324 | 426 | 32x64 | 32×64 | script 218,220,222 | **是** |
| `YOUKAI_AYA_S5` ☠ | 325 | 427 | 32x64 | 32×64 | script 218,220,222 | **是** |
| `YOUKAI_AYA_S6` ☠ | 326 | 428 | 32x64 | 32×64 | script 218,220,222 | **是** |
| `YOUKAI_AYA_S7` ☠ | 327 | 429 | 32x64 | 32×64 | script 218,220,222 | **是** |
| `YOUKAI_AYA_S8` ☠ | 328 | 430 | 32x64 | 32×64 | script 219,220 | **是** |
| `YOUKAI_AYA_S9` ☠ | 329 | 431 | 32x64 | 32×64 | script 219,220 | **是** |
| `YOUKAI_AYA_S10` ☠ | 330 | 432 | 32x64 | 32×64 | script 219,220 | **是** |
| `YOUKAI_AYA_S11` ☠ | 331 | 433 | 32x64 | 32×64 | script 219,220 | **是** |
| `YOUKAI_AYA_S12` ☠ | 332 | 434 | 32x64 | 32×64 | script 219 | **是** |
| `YOUKAI_AYA_S13` ☠ | 333 | 435 | 32x64 | 32×64 | script 219 | **是** |
| `YOUKAI_AYA_S14` ☠ | 334 | 436 | 32x64 | 32×64 | script 219 | **是** |
| `YOUKAI_AYA_S15` ☠ | 335 | 437 | 32x64 | 32×64 | script 219 | **是** |
| `YOUKAI_AYA_S16` ☠ | 336 | 438 | 32x64 | 32×64 | script 221,222 | **是** |
| `YOUKAI_AYA_S17` ☠ | 337 | 439 | 32x64 | 32×64 | script 221,222 | **是** |
| `YOUKAI_AYA_S18` ☠ | 338 | 440 | 32x64 | 32×64 | script 221,222 | **是** |
| `YOUKAI_AYA_S19` ☠ | 339 | 441 | 32x64 | 32×64 | script 221,222 | **是** |
| `YOUKAI_AYA_S20` ☠ | 340 | 442 | 32x64 | 32×64 | script 221 | **是** |
| `YOUKAI_AYA_S21` ☠ | 341 | 443 | 32x64 | 32×64 | script 221 | **是** |
| `YOUKAI_AYA_S22` ☠ | 342 | 444 | 32x64 | 32×64 | script 221 | **是** |
| `YOUKAI_AYA_S23` ☠ | 343 | 445 | 32x64 | 32×64 | script 221 | **是** |
| `YOUKAI_OPT_YUKARI_S22` | 344 | 446 | 32x64 | 32×64 | script 224 | **是** |
| `YOUKAI_OPT_YUKARI_S23` | 345 | 447 | 32x64 | 32×64 | script 224 | **是** |
| `YOUKAI_OPT_YUKARI_S24` | 346 | 448 | 32x64 | 32×64 | script 224 | **是** |
| `YOUKAI_OPT_YUKARI_S25` | 347 | 449 | 32x64 | 32×64 | script 224 | **是** |
| `YOUKAI_OPT_YUKARI_S26` | 348 | 450 | 32x64 | 32×64 | script 224 | **是** |
| `YOUKAI_OPT_YUKARI_S27` | 349 | 451 | 32x64 | 32×64 | script 224 | **是** |
| `YOUKAI_OPT_YUKARI_S28` | 350 | 452 | 32x64 | 32×64 | script 224 | **是** |
| `YOUKAI_OPT_YUKARI_S29` | 351 | 453 | 32x64 | 32×64 | script 224 | **是** |
| `YOUKAI_OPT_YUKARI_S30` | 352 | 454 | 64x64 | 64×64 | script 224 | **是** |
| `YOUKAI_OPT_ALICE_S22` | 353 | 455 | 32x32 | 32×32 | script 225 | **是** |
| `YOUKAI_OPT_ALICE_S23` | 354 | 456 | 32x32 | 32×32 | script 225 | **是** |
| `YOUKAI_OPT_ALICE_S24` | 355 | 457 | 32x32 | 32×32 | script 225 | **是** |
| `YOUKAI_OPT_ALICE_S25` | 356 | 458 | 32x32 | 32×32 | script 225 | **是** |
| `YOUKAI_OPT_REMILIA_S20` | 357 | 459 | 32x32 | 32×32 | script 226 | **是** |
| `YOUKAI_OPT_AYA_S32` | 358 | 460 | 16x16 | 16×16 | script 227 | **是** |
| `YOUKAI_OPT_AYA_S33` | 359 | 461 | 16x16 | 16×16 | script 227 | **是** |
| `YOUKAI_OPT_AYA_S34` | 360 | 462 | 16x16 | 16×16 | script 227 | **是** |
| `YOUKAI_OPT_AYA_S35` | 361 | 463 | 16x16 | 16×16 | script 227 | **是** |
| `YOUKAI_OPT_AYA_S36` | 362 | 464 | 16x16 | 16×16 | script 227 | **是** |
| `YOUKAI_OPT_AYA_S37` | 363 | 465 | 16x16 | 16×16 | script 227 | **是** |
| `YOUKAI_OPT_AYA_S38` | 364 | 466 | 16x16 | 16×16 | script 227 | **是** |
| `YOUKAI_OPT_AYA_S39` | 365 | 467 | 16x16 | 16×16 | script 227 | **是** |

### 2.4 卡 109 pofv.c（0 张 ability 贴图）

本卡**不使用** `ability.anm` 的任何贴图或脚本（全库 grep `CE_ANM_ABILITY_` 在本文件里 0 命中）。

### 2.5 卡 110 mof.c（12 张 ability 贴图）

- 现在坏掉（entry ≥ 256）：**11 / 12**；无引用（死条目）：**1 / 12**。
- 卡代码直接起的脚本（宏 → 号）：`script104`（`mof.c:756`）、`script210`（`mof.c:712`）、`script211`（`mof.c:729`）

| 贴图 | entry | sprite | 声明 WxH | 源 PNG 实测 | 引用（脚本 / C 行）| ≥256 |
| --- | --- | --- | --- | --- | --- | --- |
| `FAITH_DOT` ☠ | 237 | 339 | 16x16 | 16×16 | script 185 | 否 |
| `MOF_FAITH_ICON` | 306 | 408 | 16x16 | 16×16 | script 210 | **是** |
| `MOF_FAITH_DIGIT_0` | 307 | 409 | 8x16 | 8×16 | native/cards/mof.c:158/native/cards/mof.c:736 + script 211 | **是** |
| `MOF_FAITH_DIGIT_1` | 308 | 410 | 8x16 | 8×16 | native/cards/mof.c:158 + script 211 | **是** |
| `MOF_FAITH_DIGIT_2` | 309 | 411 | 8x16 | 8×16 | native/cards/mof.c:159 | **是** |
| `MOF_FAITH_DIGIT_3` | 310 | 412 | 8x16 | 8×16 | native/cards/mof.c:159 | **是** |
| `MOF_FAITH_DIGIT_4` | 311 | 413 | 8x16 | 8×16 | native/cards/mof.c:160 | **是** |
| `MOF_FAITH_DIGIT_5` | 312 | 414 | 8x16 | 8×16 | native/cards/mof.c:160 | **是** |
| `MOF_FAITH_DIGIT_6` | 313 | 415 | 8x16 | 8×16 | native/cards/mof.c:161 | **是** |
| `MOF_FAITH_DIGIT_7` | 314 | 416 | 8x16 | 8×16 | native/cards/mof.c:161 | **是** |
| `MOF_FAITH_DIGIT_8` | 315 | 417 | 8x16 | 8×16 | native/cards/mof.c:162 | **是** |
| `MOF_FAITH_DIGIT_9` | 316 | 418 | 8x16 | 8×16 | native/cards/mof.c:162 + script 211 | **是** |

### 2.6 卡 111 signal.c（3 张 ability 贴图）

- 现在坏掉（entry ≥ 256）：**3 / 3**；无引用（死条目）：**0 / 3**。
- 卡代码直接起的脚本（宏 → 号）：`script212`（`signal.c:578`）、`script213`（`signal.c:582`）、`script214`（`signal.c:586`）

| 贴图 | entry | sprite | 声明 WxH | 源 PNG 实测 | 引用（脚本 / C 行）| ≥256 |
| --- | --- | --- | --- | --- | --- | --- |
| `SIGNAL_GAUGE_BG` | 317 | 419 | 64x16 | 64×16 | script 212 | **是** |
| `SIGNAL_GAUGE_BAR` | 318 | 420 | 64x16 | 64×16 | script 213 | **是** |
| `SIGNAL_GAUGE_ICON` | 319 | 421 | 16x16 | 16×16 | script 214 | **是** |

### 2.7 卡 112 ufo.c（29 张 ability 贴图）

- 现在坏掉（entry ≥ 256）：**1 / 29**；无引用（死条目）：**0 / 29**。
- 卡代码直接起的脚本（宏 → 号）：`script104`（`ufo.c:1700`）、`script164`（`ufo.c:216`）、`script165`（`ufo.c:217`）、`script166`（`ufo.c:218`）、`script167`（`ufo.c:219`）、`script168`（`ufo.c:181`）、`script169`（`ufo.c:181`）、`script170`（`ufo.c:181`）、`script171`（`ufo.c:181`）、`script172`（`ufo.c:181`）、`script173`（`ufo.c:181`）、`script215`（`ufo.c:181`）、`script216`（`ufo.c:181`）、`script217`（`ufo.c:181`）、`script223`（`ufo.c:383`）、`script228`（`ufo.c:1126`）、`script231`（`ufo.c:1131`）

| 贴图 | entry | sprite | 声明 WxH | 源 PNG 实测 | 引用（脚本 / C 行）| ≥256 |
| --- | --- | --- | --- | --- | --- | --- |
| `UFO_RED_0` | 209 | 311 | 64x64 | 64×64 | script 164 | 否 |
| `UFO_RED_1` | 210 | 312 | 64x64 | 64×64 | script 164 | 否 |
| `UFO_RED_2` | 211 | 313 | 64x64 | 64×64 | script 164 | 否 |
| `UFO_RED_3` | 212 | 314 | 64x64 | 64×64 | script 164 | 否 |
| `UFO_BLUE_0` | 213 | 315 | 64x64 | 64×64 | script 165 | 否 |
| `UFO_BLUE_1` | 214 | 316 | 64x64 | 64×64 | script 165 | 否 |
| `UFO_BLUE_2` | 215 | 317 | 64x64 | 64×64 | script 165 | 否 |
| `UFO_BLUE_3` | 216 | 318 | 64x64 | 64×64 | script 165 | 否 |
| `UFO_GREEN_0` | 217 | 319 | 64x64 | 64×64 | script 166 | 否 |
| `UFO_GREEN_1` | 218 | 320 | 64x64 | 64×64 | script 166 | 否 |
| `UFO_GREEN_2` | 219 | 321 | 64x64 | 64×64 | script 166 | 否 |
| `UFO_GREEN_3` | 220 | 322 | 64x64 | 64×64 | script 166 | 否 |
| `UFO_RAINBOW_0` | 221 | 323 | 64x64 | 64×64 | script 167 | 否 |
| `UFO_RAINBOW_1` | 222 | 324 | 64x64 | 64×64 | script 167 | 否 |
| `UFO_RAINBOW_2` | 223 | 325 | 64x64 | 64×64 | script 167 | 否 |
| `UFO_RAINBOW_3` | 224 | 326 | 64x64 | 64×64 | script 167 | 否 |
| `TOKEN_RED_0` | 225 | 327 | 32x32 | 32×32 | script 168 | 否 |
| `TOKEN_RED_1` | 226 | 328 | 32x32 | 32×32 | script 168,215 | 否 |
| `TOKEN_BLUE_0` | 227 | 329 | 32x32 | 32×32 | script 169 | 否 |
| `TOKEN_BLUE_1` | 228 | 330 | 32x32 | 32×32 | script 169,216 | 否 |
| `TOKEN_GREEN_0` | 229 | 331 | 32x32 | 32×32 | script 170,217 | 否 |
| `TOKEN_GREEN_1` | 230 | 332 | 32x32 | 32×32 | script 170 | 否 |
| `TOKEN_RBRED_0` | 231 | 333 | 32x32 | 32×32 | script 171,215 | 否 |
| `TOKEN_RBRED_1` | 232 | 334 | 32x32 | 32×32 | script 171,215 | 否 |
| `TOKEN_RBBLUE_0` | 233 | 335 | 32x32 | 32×32 | script 172,216 | 否 |
| `TOKEN_RBBLUE_1` | 234 | 336 | 32x32 | 32×32 | script 172,216 | 否 |
| `TOKEN_RBGREEN_0` | 235 | 337 | 32x32 | 32×32 | script 173,217 | 否 |
| `TOKEN_RBGREEN_1` | 236 | 338 | 32x32 | 32×32 | script 173,217 | 否 |
| `UFO_ARC_STRIP` | 366 | 468 | 16x128 | 16×128 | script 228 | **是** |

### 2.8 卡 113 trance.c（27 张 ability 贴图）

- 现在坏掉（entry ≥ 256）：**0 / 27**；无引用（死条目）：**1 / 27**。
- 卡代码直接起的脚本（宏 → 号）：`script174`（`trance.c:790`）、`script175`（`trance.c:849`）、`script176`（`trance.c:850`）、`script177`（`trance.c:851`）、`script178`（`trance.c:852`）、`script179`（`trance.c:293`）、`script182`（`trance.c:286`）、`script233`（`trance.c:294`）、`script236`（`trance.c:287`）、`script239`（`trance.c:295`）、`script242`（`trance.c:288`）、`script245`（`trance.c:296`）、`script248`（`trance.c:289`）、`script251`（`trance.c:1706`）、`script252`（`trance.c:859`）、`script253`（`trance.c:860`）、`script254`（`trance.c:861`）、`script255`（`trance.c:897`）、`script256`（`trance.c:897`）、`script257`（`trance.c:899`）、`script258`（`trance.c:899`）、`script259`（`trance.c:898`）、`script260`（`trance.c:898`）、`script261`（`trance.c:1728`）

| 贴图 | entry | sprite | 声明 WxH | 源 PNG 实测 | 引用（脚本 / C 行）| ≥256 |
| --- | --- | --- | --- | --- | --- | --- |
| `SPIRIT_PLAIN_0` | 7 | 109 | 32x32 | 32×32 | script 180,234,240,246 | 否 |
| `SPIRIT_PLAIN_1` | 8 | 110 | 32x32 | 32×32 | script 181,235,241,247 | 否 |
| `SPIRIT_TRANCE_0` | 9 | 111 | 32x32 | 32×32 | script 183 | 否 |
| `SPIRIT_TRANCE_1` | 10 | 112 | 32x32 | 32×32 | script 184 | 否 |
| `SPIRIT_TRANCE_2` | 11 | 113 | 32x32 | 32×32 | script 237 | 否 |
| `SPIRIT_TRANCE_3` | 12 | 114 | 32x32 | 32×32 | script 238 | 否 |
| `TRANCE_GAUGE_TOP` | 13 | 115 | 64x64 | 64×64 | script 175 | 否 |
| `TRANCE_GAUGE_BAR` | 14 | 116 | 48x32 | 48×32 | script 176 | 否 |
| `TRANCE_GAUGE_BAR2` | 15 | 117 | 48x32 | 48×32 | script 177 | 否 |
| `TRANCE_GAUGE_MARK` | 16 | 118 | 16x32 | 16×32 | script 178 | 否 |
| `SPIRIT_TRANCE_4` | 17 | 119 | 32x32 | 32×32 | script 243 | 否 |
| `SPIRIT_TRANCE_5` | 18 | 120 | 32x32 | 32×32 | script 244 | 否 |
| `SPIRIT_TRANCE_6` | 19 | 121 | 32x32 | 32×32 | script 249 | 否 |
| `SPIRIT_TRANCE_7` | 20 | 122 | 32x32 | 32×32 | script 250 | 否 |
| `TRANCE_ASTRAL` | 21 | 123 | 512x512 | 512×512 | script 251 | 否 |
| `TRANCE_TIERUP_1` | 22 | 124 | 32x16 | 32×16 | script 252 | 否 |
| `TRANCE_TIERUP_2` | 23 | 125 | 32x16 | 32×16 | script 253 | 否 |
| `TRANCE_TIERUP_MAX` | 24 | 126 | 32x16 | 32×16 | script 254 | 否 |
| `TRANCE_GAUGE_FRAME_A` | 25 | 127 | 32x48 | 32×48 | native/cards/trance.c:2106 + script 175 | 否 |
| `TRANCE_GAUGE_FRAME_B` | 26 | 128 | 32x48 | 32×48 | native/cards/trance.c:2105 | 否 |
| `TRANCE_POPUP_LIFE1` | 27 | 129 | 24x8 | 24×8 | script 255 | 否 |
| `TRANCE_POPUP_LIFE2` | 28 | 130 | 24x8 | 24×8 | script 256 | 否 |
| `TRANCE_POPUP_SPELL1` | 29 | 131 | 32x8 | 24×8 | script 257 | 否 |
| `TRANCE_POPUP_SPELL2` | 30 | 132 | 32x8 | 24×8 | script 258 | 否 |
| `TRANCE_POPUP_POINT10` | 31 | 133 | 32x8 | 32×8 | script 259 | 否 |
| `TRANCE_POPUP_POINT100` | 32 | 134 | 32x8 | 32×8 | script 260 | 否 |
| `TPOINT` ☠ | 240 | 342 | 16x16 | 16×16 | script 188 | 否 |

### 2.9 卡 114 collect2x.c（0 张 ability 贴图）

本卡**不使用** `ability.anm` 的任何贴图或脚本（全库 grep `CE_ANM_ABILITY_` 在本文件里 0 命中）。

### 2.10 卡 115 lolk.c（8 张 ability 贴图）

- 现在坏掉（entry ≥ 256）：**0 / 8**；无引用（死条目）：**0 / 8**。
- 卡代码直接起的脚本（宏 → 号）：`script106`（`lolk.c:464`）、`script107`（`lolk.c:574`）、`script108`（`lolk.c:574`）、`script109`（`lolk.c:575`）、`script110`（`lolk.c:575`）、`script111`（`lolk.c:576`）、`script112`（`lolk.c:576`）、`script116`（`lolk.c:587`）

| 贴图 | entry | sprite | 声明 WxH | 源 PNG 实测 | 引用（脚本 / C 行）| ≥256 |
| --- | --- | --- | --- | --- | --- | --- |
| `LOLK_RING` | 62 | 164 | 128x128 | 128×128 | script 106 | 否 |
| `LOLK_PANEL_1` | 63 | 165 | 256x32 | 256×32 | script 107 | 否 |
| `LOLK_PANEL_2` | 64 | 166 | 256x32 | 256×32 | script 108 | 否 |
| `LOLK_PANEL_3` | 65 | 167 | 256x32 | 256×32 | script 109 | 否 |
| `LOLK_PANEL_4` | 66 | 168 | 256x32 | 256×32 | script 110 | 否 |
| `LOLK_PANEL_5` | 67 | 169 | 256x32 | 256×32 | script 111 | 否 |
| `LOLK_PANEL_6` | 68 | 170 | 256x32 | 256×32 | script 112 | 否 |
| `LOLK_PANEL_7` | 69 | 171 | 256x32 | 256×32 | script 116 | 否 |

### 2.11 卡 116 season.c（43 张 ability 贴图）

- 现在坏掉（entry ≥ 256）：**0 / 43**；无引用（死条目）：**1 / 43**。
- 卡代码直接起的脚本（宏 → 号）：`script113`（`season.c:313`）、`script115`（`season.c:422`）、`script120`（`season.c:886`）、`script121`（`season.c:889`）、`script123`（`season.c:1402`）、`script124`（`season.c:1413`）、`script125`（`season.c:1411`）、`script126`（`season.c:1415`）、`script127`（`season.c:1417`）、`script135`（`season.c:888`）

| 贴图 | entry | sprite | 声明 WxH | 源 PNG 实测 | 引用（脚本 / C 行）| ≥256 |
| --- | --- | --- | --- | --- | --- | --- |
| `SEASON_OPT` | 70 | 172 | 64x64 | 64×64 | script 113 | 否 |
| `SEASON_SEED` ☠ | 71 | 173 | 32x32 | 32×32 | script 114 | 否 |
| `PT_SPRING` | 72 | 174 | 16x16 | 16×16 | native/cards/faith.c:186/native/cards/faith.c:35/native/cards/faith.h:20/native/cards/season.c:354 + script 115 | 否 |
| `PT_SUMMER` | 73 | 175 | 16x16 | 16×16 | native/cards/faith.c:187/native/cards/faith.c:35/native/cards/faith.h:20/native/cards/season.c:355 | 否 |
| `PT_FALL` | 74 | 176 | 16x16 | 16×16 | native/cards/faith.c:185/native/cards/faith.c:35/native/cards/faith.h:20/native/cards/season.c:356 | 否 |
| `PT_WINTER` | 75 | 177 | 16x16 | 16×16 | native/cards/faith.c:35/native/cards/faith.h:20/native/cards/season.c:357 | 否 |
| `PT_DOYOU` | 76 | 178 | 16x16 | 16×16 | native/cards/faith.c:35/native/cards/faith.h:20/native/cards/season.c:358 | 否 |
| `GLYPH_SPRING` | 77 | 179 | 32x32 | 32×32 | native/cards/season.c:366 + script 117,126 | 否 |
| `GLYPH_SUMMER` | 78 | 180 | 32x32 | 32×32 | native/cards/season.c:367 | 否 |
| `GLYPH_FALL` | 79 | 181 | 32x32 | 32×32 | native/cards/season.c:368 | 否 |
| `GLYPH_WINTER` | 80 | 182 | 32x32 | 32×32 | native/cards/season.c:369 | 否 |
| `GLYPH_DOYOU` | 81 | 183 | 32x32 | 32×32 | native/cards/season.c:370 | 否 |
| `OPT_SPRING` | 82 | 184 | 16x16 | 16×16 | native/cards/season.c:342 | 否 |
| `OPT_SUMMER` | 83 | 185 | 16x16 | 16×16 | native/cards/season.c:343 | 否 |
| `OPT_FALL` | 84 | 186 | 16x16 | 16×16 | native/cards/season.c:344 | 否 |
| `OPT_WINTER` | 85 | 187 | 16x16 | 16×16 | native/cards/season.c:345 | 否 |
| `OPT_DOYOU` | 86 | 188 | 16x16 | 16×16 | native/cards/season.c:346 | 否 |
| `SEASON_LASER` | 87 | 189 | 256x32 | 256×32 | script 122,132 | 否 |
| `SEASON_STRIP` | 88 | 190 | 64x16 | 64×16 | script 127 | 否 |
| `BULLET_SPRING_A` | 89 | 191 | 32x32 | 32×32 | script 128 | 否 |
| `BULLET_SPRING_B` | 90 | 192 | 32x32 | 32×32 | script 128 | 否 |
| `HIT_SPRING_1` | 91 | 193 | 32x32 | 32×32 | script 128 | 否 |
| `HIT_SPRING_2` | 92 | 194 | 32x32 | 32×32 | script 128 | 否 |
| `HIT_SPRING_3` | 93 | 195 | 32x32 | 32×32 | script 128 | 否 |
| `HIT_SPRING_4` | 94 | 196 | 32x32 | 32×32 | script 128 | 否 |
| `HIT_SPRING_5` | 95 | 197 | 32x32 | 32×32 | script 128 | 否 |
| `HIT_SPRING_6` | 96 | 198 | 32x32 | 32×32 | script 128 | 否 |
| `HIT_SPRING_7` | 97 | 199 | 32x32 | 32×32 | script 128 | 否 |
| `HIT_SPRING_8` | 98 | 200 | 32x32 | 32×32 | script 128 | 否 |
| `BULLET_SUMMER` | 99 | 201 | 16x16 | 16×16 | script 129 | 否 |
| `HIT_SUMMER_1` | 100 | 202 | 16x16 | 16×16 | script 129 | 否 |
| `HIT_SUMMER_2` | 101 | 203 | 16x16 | 16×16 | script 129 | 否 |
| `HIT_SUMMER_3` | 102 | 204 | 16x16 | 16×16 | script 129 | 否 |
| `BULLET_FALL` | 103 | 205 | 64x64 | 64×64 | script 130 | 否 |
| `HIT_FALL_1` | 104 | 206 | 64x64 | 64×64 | script 130 | 否 |
| `HIT_FALL_2` | 105 | 207 | 64x64 | 64×64 | script 130 | 否 |
| `HIT_FALL_3` | 106 | 208 | 64x64 | 64×64 | script 130 | 否 |
| `BULLET_DOYOU` | 107 | 209 | 32x16 | 32×16 | script 131 | 否 |
| `HIT_DOYOU_1` | 108 | 210 | 32x16 | 32×16 | script 131 | 否 |
| `HIT_DOYOU_2` | 109 | 211 | 32x16 | 32×16 | script 131 | 否 |
| `HIT_DOYOU_3` | 110 | 212 | 32x16 | 32×16 | script 131 | 否 |
| `LASER_CHILD` | 111 | 213 | 32x32 | 32×32 | script 133 | 否 |
| `LASER_HIT` | 112 | 214 | 32x32 | 32×32 | script 134 | 否 |

### 2.12 卡 117 animal.c（82 张 ability 贴图）

- 现在坏掉（entry ≥ 256）：**0 / 82**；无引用（死条目）：**10 / 82**。
- 卡代码直接起的脚本（宏 → 号）：`script137`（`animal.c:593`）、`script138`（`animal.c:594`）、`script139`（`animal.c:595`）、`script140`（`animal.c:596`）、`script141`（`animal.c:597`）、`script142`（`animal.c:598`）、`script143`（`animal.c:599`）、`script145`（`animal.c:1951`）、`script146`（`animal.c:2031`）、`script147`（`animal.c:2032`）、`script148`（`animal.c:2033`）、`script149`（`animal.c:2809`）、`script150`（`animal.c:2951`）、`script154`（`animal.c:1967`）、`script155`（`animal.c:2069`）、`script156`（`animal.c:1934`）、`script157`（`animal.c:2147`）、`script158`（`animal.c:2148`）、`script159`（`animal.c:2025`）、`script160`（`animal.c:2987`）、`script163`（`animal.c:2467`）

| 贴图 | entry | sprite | 声明 WxH | 源 PNG 实测 | 引用（脚本 / C 行）| ≥256 |
| --- | --- | --- | --- | --- | --- | --- |
| `WOLF_0` | 127 | 229 | 32x32 | 32×32 | script 137,145 | 否 |
| `WOLF_1` | 128 | 230 | 32x32 | 32×32 | script 137 | 否 |
| `WOLF_2` | 129 | 231 | 32x32 | 32×32 | script 137 | 否 |
| `OTTER_0` | 130 | 232 | 32x32 | 32×32 | script 138 | 否 |
| `OTTER_1` | 131 | 233 | 32x32 | 32×32 | script 138 | 否 |
| `OTTER_2` | 132 | 234 | 32x32 | 32×32 | script 138 | 否 |
| `EAGLE_0` | 133 | 235 | 32x32 | 32×32 | script 139 | 否 |
| `EAGLE_1` | 134 | 236 | 32x32 | 32×32 | script 139 | 否 |
| `EAGLE_2` | 135 | 237 | 32x32 | 32×32 | script 139 | 否 |
| `FISH_BOMB` | 136 | 238 | 32x32 | 32×32 | script 140 | 否 |
| `FISH_LIFE` | 137 | 239 | 32x32 | 32×32 | script 141 | 否 |
| `FISH_POWER` | 138 | 240 | 32x32 | 32×32 | script 142 | 否 |
| `FISH_POINT` ☠ | 139 | 241 | 32x32 | 32×32 | **无** | 否 |
| `GAUGE_FRAME` ☠ | 140 | 242 | 64x64 | 64×64 | script 144 | 否 |
| `AURA_EAGLE_0` | 141 | 243 | 64x64 | 64×64 | native/cards/animal.c:2011 + script 148 | 否 |
| `AURA_EAGLE_1` | 142 | 244 | 64x64 | 64×64 | script 148 | 否 |
| `AURA_EAGLE_2` | 143 | 245 | 64x64 | 64×64 | script 148 | 否 |
| `AURA_EAGLE_3` | 144 | 246 | 64x64 | 64×64 | script 148 | 否 |
| `AURA_EAGLE_4` | 145 | 247 | 64x64 | 64×64 | script 148 | 否 |
| `AURA_EAGLE_5` | 146 | 248 | 64x64 | 64×64 | script 148 | 否 |
| `AURA_EAGLE_6` | 147 | 249 | 64x64 | 64×64 | script 148 | 否 |
| `AURA_EAGLE_7` | 148 | 250 | 64x64 | 64×64 | script 148 | 否 |
| `AURA_WOLF_0` | 149 | 251 | 64x64 | 64×64 | native/cards/animal.c:2009 + script 146,159 | 否 |
| `AURA_WOLF_1` | 150 | 252 | 64x64 | 64×64 | script 146 | 否 |
| `AURA_WOLF_2` | 151 | 253 | 64x64 | 64×64 | script 146 | 否 |
| `AURA_WOLF_3` | 152 | 254 | 64x64 | 64×64 | script 146 | 否 |
| `AURA_WOLF_4` | 153 | 255 | 64x64 | 64×64 | script 146 | 否 |
| `AURA_WOLF_5` | 154 | 256 | 64x64 | 64×64 | script 146 | 否 |
| `AURA_WOLF_6` | 155 | 257 | 64x64 | 64×64 | script 146 | 否 |
| `AURA_WOLF_7` | 156 | 258 | 64x64 | 64×64 | script 146 | 否 |
| `AURA_OTTER_0` | 157 | 259 | 64x64 | 64×64 | native/cards/animal.c:2010 + script 147 | 否 |
| `AURA_OTTER_1` | 158 | 260 | 64x64 | 64×64 | script 147 | 否 |
| `AURA_OTTER_2` | 159 | 261 | 64x64 | 64×64 | script 147 | 否 |
| `AURA_OTTER_3` | 160 | 262 | 64x64 | 64×64 | script 147 | 否 |
| `AURA_OTTER_4` | 161 | 263 | 64x64 | 64×64 | script 147 | 否 |
| `AURA_OTTER_5` | 162 | 264 | 64x64 | 64×64 | script 147 | 否 |
| `AURA_OTTER_6` | 163 | 265 | 64x64 | 64×64 | script 147 | 否 |
| `AURA_OTTER_7` | 164 | 266 | 64x64 | 64×64 | script 147 | 否 |
| `OTTER_ORB` | 165 | 267 | 128x64 | 128×64 | script 149 | 否 |
| `STRIKE_RING` | 166 | 268 | 128x128 | 128×128 | script 151,152,153 | 否 |
| `SOUL_0` | 167 | 269 | 16x16 | 16×16 | script 154 | 否 |
| `SOUL_1` | 168 | 270 | 16x16 | 16×16 | script 154 | 否 |
| `SOUL_2` | 169 | 271 | 16x16 | 16×16 | script 154 | 否 |
| `SOUL_3` | 170 | 272 | 16x16 | 16×16 | script 154 | 否 |
| `SOUL_4` | 171 | 273 | 16x16 | 16×16 | script 154 | 否 |
| `SOUL_5` | 172 | 274 | 16x16 | 16×16 | script 154 | 否 |
| `SOUL_6` | 173 | 275 | 16x16 | 16×16 | script 154 | 否 |
| `SOUL_7` | 174 | 276 | 16x16 | 16×16 | script 154 | 否 |
| `SOUL_8` | 175 | 277 | 16x16 | 16×16 | script 154 | 否 |
| `SOUL_9` | 176 | 278 | 16x16 | 16×16 | script 154 | 否 |
| `SOUL_10` | 177 | 279 | 16x16 | 16×16 | script 154 | 否 |
| `ITEM_P` ☠ | 178 | 280 | 16x16 | 16×16 | **无** | 否 |
| `ITEM_MONEY` ☠ | 179 | 281 | 16x16 | 16×16 | **无** | 否 |
| `ITEM_LIFE` ☠ | 180 | 282 | 16x16 | 16×16 | **无** | 否 |
| `ITEM_BOMB` ☠ | 181 | 283 | 16x16 | 16×16 | **无** | 否 |
| `FLAME_0` | 182 | 284 | 64x64 | 64×64 | script 156 | 否 |
| `FLAME_1` | 183 | 285 | 64x64 | 64×64 | script 156 | 否 |
| `FLAME_2` | 184 | 286 | 64x64 | 64×64 | script 156 | 否 |
| `FLAME_3` | 185 | 287 | 64x64 | 64×64 | script 156 | 否 |
| `FLAME_4` | 186 | 288 | 64x64 | 64×64 | script 156 | 否 |
| `FLAME_5` | 187 | 289 | 64x64 | 64×64 | script 156 | 否 |
| `FLAME_6` | 188 | 290 | 64x64 | 64×64 | script 156 | 否 |
| `FLAME_7` | 189 | 291 | 64x64 | 64×64 | script 156 | 否 |
| `FLAME_8` | 190 | 292 | 64x64 | 64×64 | script 156 | 否 |
| `FLAME_9` | 191 | 293 | 64x64 | 64×64 | script 156 | 否 |
| `FLAME_10` | 192 | 294 | 64x64 | 64×64 | script 156 | 否 |
| `FLAME_11` | 193 | 295 | 64x64 | 64×64 | script 156 | 否 |
| `ICON_WOLF` | 194 | 296 | 64x64 | 64×64 | native/cards/animal.c:611 | 否 |
| `ICON_OTTER` | 195 | 297 | 64x64 | 64×64 | native/cards/animal.c:612 | 否 |
| `ICON_EAGLE` | 196 | 298 | 64x64 | 64×64 | native/cards/animal.c:613 | 否 |
| `ICON_FISH_BOMB` | 197 | 299 | 32x32 | 32×32 | native/cards/animal.c:617 | 否 |
| `ICON_FISH_LIFE` | 198 | 300 | 32x32 | 32×32 | native/cards/animal.c:618 | 否 |
| `ICON_FISH_POWER` | 199 | 301 | 32x32 | 32×32 | native/cards/animal.c:619 | 否 |
| `ICON_FISH_POINT` ☠ | 200 | 302 | 32x32 | 32×32 | **无** | 否 |
| `CARD_BOMB` ☠ | 201 | 303 | 64x64 | 64×64 | **无** | 否 |
| `CARD_LIFE` ☠ | 202 | 304 | 64x64 | 64×64 | **无** | 否 |
| `CARD_MONEY` ☠ | 203 | 305 | 64x64 | 64×64 | **无** | 否 |
| `FISH_MONEY` | 204 | 306 | 32x32 | 32×32 | script 143 | 否 |
| `ICON_FISH_MONEY` | 205 | 307 | 32x32 | 32×32 | native/cards/animal.c:620 | 否 |
| `BEASTAP_A` | 206 | 308 | 512x32 | 512×32 | script 161 | 否 |
| `BEASTAP_B` | 207 | 309 | 512x32 | 512×32 | script 162 | 否 |
| `EAGLE_BEAM` | 208 | 310 | 512x32 | 512×32 | script 163 | 否 |

## 3. 总数对账：这 360 张谁在用

判据（全部可复现，见 §8）：

1. **根** = `native/**/*.c|h` 里出现过的 `CE_ANM_ABILITY_SCRIPT_*` 宏 + `assets/sht/append_shooterset.py` 里的同名宏
   + `native/cards/senro.c:153-154` 两条**裸数字**（204/205，没有 `#ifdef` 兜底 ⇒ 必须当根）；
2. **可达脚本** = 根 ∪ 所有 `scriptNew(@SCRIPT_X@)` 与 `scriptNew(scriptN)`（字面量，8 个文件里共 30 处）的闭包；
3. **活贴图** = 被可达脚本 `@NAME` 引用的 ∪ 被 C 宏直接用的 ∪ **算术族**（`TS_DIGIT_0+v`、`FONT2_0+(ch-'0')`、
   `AURA_{WOLF,OTTER,EAGLE}_0+k` ⇒ 整族视为活）；其余 = **无引用**。

| 指标 | 数 |
| --- | --- |
| 追加贴图总数 | 360 |
| 被至少一个**可达**脚本引用的名称 | 240 |
| 被 C 宏**逐名**引用的名称 | 43 |
| 只靠**算术推导**才活的名字（不出现在任何 C 宏/脚本里）| 18（`TS_DIGIT_1..9`、`FONT2_1..9`）|
| **活贴图合计**（三者并集）| **290** |
| **无引用（死）贴图** | **70** |
| 追加脚本总数 | 194 |
| **无引用（死）脚本** | **30** |

### 3.1 死脚本（30 个）—— 没有任何 C 代码起它们，也没有父脚本 `scriptNew` 它们

| 脚本 | 归属卡 | 它引用的贴图（同样变死）|
| --- | --- | --- |
| `script114` `season_seed` | 116 | `SEASON_SEED` |
| `script117` `season_glyph` | 116 | `GLYPH_SPRING` |
| `script118` `season_bar_frame` | 116 | —（只画多边形/无贴图） |
| `script119` `season_bar_fill` | 116 | —（只画多边形/无贴图） |
| `script122` `season_laser` | 116 | `SEASON_LASER` |
| `script144` `gauge_frame` | 117 | `GAUGE_FRAME` |
| `script185` `faith_dot` | 110 | `FAITH_DOT` |
| `script186` `sakura_point` | 107 | `SAKURA_POINT` |
| `script187` `sakura_small` | 107 | `SAKURA_SMALL` |
| `script188` `tpoint` | 113 | `TPOINT` |
| `script189` `youkai_yukari_s5` | 108 | `YOUKAI_YUKARI_S11`、`YOUKAI_YUKARI_S12`、`YOUKAI_YUKARI_S13`、`YOUKAI_YUKARI_S14` |
| `script190` `youkai_yukari_s6` | 108 | `YOUKAI_YUKARI_S15`、`YOUKAI_YUKARI_S16`、`YOUKAI_YUKARI_S17`、`YOUKAI_YUKARI_S18`、`YOUKAI_YUKARI_S19`、`YOUKAI_YUKARI_S20`、`YOUKAI_YUKARI_S21` |
| `script191` `youkai_yukari_s7` | 108 | `YOUKAI_YUKARI_S11`、`YOUKAI_YUKARI_S12`、`YOUKAI_YUKARI_S13`、`YOUKAI_YUKARI_S14`、`YOUKAI_YUKARI_S15`、`YOUKAI_YUKARI_S16`、`YOUKAI_YUKARI_S17`、`YOUKAI_YUKARI_S18` |
| `script192` `youkai_yukari_s8` | 108 | `YOUKAI_YUKARI_S15`、`YOUKAI_YUKARI_S16`、`YOUKAI_YUKARI_S17`、`YOUKAI_YUKARI_S18`、`YOUKAI_YUKARI_S19`、`YOUKAI_YUKARI_S20`、`YOUKAI_YUKARI_S21` |
| `script193` `youkai_yukari_s9` | 108 | `YOUKAI_YUKARI_S11`、`YOUKAI_YUKARI_S12`、`YOUKAI_YUKARI_S13`、`YOUKAI_YUKARI_S14`、`YOUKAI_YUKARI_S15`、`YOUKAI_YUKARI_S16`、`YOUKAI_YUKARI_S17`、`YOUKAI_YUKARI_S18` |
| `script194` `youkai_alice_s5` | 108 | `YOUKAI_ALICE_S11`、`YOUKAI_ALICE_S12`、`YOUKAI_ALICE_S13`、`YOUKAI_ALICE_S14` |
| `script195` `youkai_alice_s6` | 108 | `YOUKAI_ALICE_S15`、`YOUKAI_ALICE_S16`、`YOUKAI_ALICE_S17`、`YOUKAI_ALICE_S18`、`YOUKAI_ALICE_S19`、`YOUKAI_ALICE_S20`、`YOUKAI_ALICE_S21` |
| `script196` `youkai_alice_s7` | 108 | `YOUKAI_ALICE_S11`、`YOUKAI_ALICE_S12`、`YOUKAI_ALICE_S13`、`YOUKAI_ALICE_S14`、`YOUKAI_ALICE_S15`、`YOUKAI_ALICE_S16`、`YOUKAI_ALICE_S17`、`YOUKAI_ALICE_S18` |
| `script197` `youkai_alice_s8` | 108 | `YOUKAI_ALICE_S15`、`YOUKAI_ALICE_S16`、`YOUKAI_ALICE_S17`、`YOUKAI_ALICE_S18`、`YOUKAI_ALICE_S19`、`YOUKAI_ALICE_S20`、`YOUKAI_ALICE_S21` |
| `script198` `youkai_alice_s9` | 108 | `YOUKAI_ALICE_S11`、`YOUKAI_ALICE_S12`、`YOUKAI_ALICE_S13`、`YOUKAI_ALICE_S14`、`YOUKAI_ALICE_S15`、`YOUKAI_ALICE_S16`、`YOUKAI_ALICE_S17`、`YOUKAI_ALICE_S18` |
| `script199` `youkai_remilia_s5` | 108 | `YOUKAI_REMILIA_S11`、`YOUKAI_REMILIA_S12`、`YOUKAI_REMILIA_S13`、`YOUKAI_REMILIA_S14` |
| `script200` `youkai_remilia_s6` | 108 | `YOUKAI_REMILIA_S15`、`YOUKAI_REMILIA_S16`、`YOUKAI_REMILIA_S17`、`YOUKAI_REMILIA_S18`、`YOUKAI_REMILIA_S19` |
| `script201` `youkai_remilia_s7` | 108 | `YOUKAI_REMILIA_S11`、`YOUKAI_REMILIA_S12`、`YOUKAI_REMILIA_S13`、`YOUKAI_REMILIA_S14`、`YOUKAI_REMILIA_S15`、`YOUKAI_REMILIA_S16` |
| `script202` `youkai_remilia_s8` | 108 | `YOUKAI_REMILIA_S15`、`YOUKAI_REMILIA_S16`、`YOUKAI_REMILIA_S17`、`YOUKAI_REMILIA_S18`、`YOUKAI_REMILIA_S19` |
| `script203` `youkai_remilia_s9` | 108 | `YOUKAI_REMILIA_S11`、`YOUKAI_REMILIA_S12`、`YOUKAI_REMILIA_S13`、`YOUKAI_REMILIA_S14`、`YOUKAI_REMILIA_S15`、`YOUKAI_REMILIA_S16` |
| `script218` `youkai_aya_s0` | 108 | `YOUKAI_AYA_S0`、`YOUKAI_AYA_S1`、`YOUKAI_AYA_S2`、`YOUKAI_AYA_S3`、`YOUKAI_AYA_S4`、`YOUKAI_AYA_S5`、`YOUKAI_AYA_S6`、`YOUKAI_AYA_S7` |
| `script219` `youkai_aya_s1` | 108 | `YOUKAI_AYA_S10`、`YOUKAI_AYA_S11`、`YOUKAI_AYA_S12`、`YOUKAI_AYA_S13`、`YOUKAI_AYA_S14`、`YOUKAI_AYA_S15`、`YOUKAI_AYA_S8`、`YOUKAI_AYA_S9` |
| `script220` `youkai_aya_s2` | 108 | `YOUKAI_AYA_S0`、`YOUKAI_AYA_S1`、`YOUKAI_AYA_S10`、`YOUKAI_AYA_S11`、`YOUKAI_AYA_S2`、`YOUKAI_AYA_S3`、`YOUKAI_AYA_S4`、`YOUKAI_AYA_S5`、`YOUKAI_AYA_S6`、`YOUKAI_AYA_S7`、`YOUKAI_AYA_S8`、`YOUKAI_AYA_S9` |
| `script221` `youkai_aya_s3` | 108 | `YOUKAI_AYA_S16`、`YOUKAI_AYA_S17`、`YOUKAI_AYA_S18`、`YOUKAI_AYA_S19`、`YOUKAI_AYA_S20`、`YOUKAI_AYA_S21`、`YOUKAI_AYA_S22`、`YOUKAI_AYA_S23` |
| `script222` `youkai_aya_s4` | 108 | `YOUKAI_AYA_S0`、`YOUKAI_AYA_S1`、`YOUKAI_AYA_S16`、`YOUKAI_AYA_S17`、`YOUKAI_AYA_S18`、`YOUKAI_AYA_S19`、`YOUKAI_AYA_S2`、`YOUKAI_AYA_S3`、`YOUKAI_AYA_S4`、`YOUKAI_AYA_S5`、`YOUKAI_AYA_S6`、`YOUKAI_AYA_S7` |

### 3.2 死贴图（70 张）—— 合并时的「删 or 留」清单

| 组 | 张数 | 名称 | 备注 |
| --- | --- | --- | --- |
| 108 妖侧本体（紫/爱丽丝/文） | 46 | YOUKAI_YUKARI_S11..S21、YOUKAI_ALICE_S11..S21、YOUKAI_AYA_S0..S23 | `youkai.c` 一条都没引用；且 §7-① 那条路径要的是 `pl0X.anm` 的 sprite 号 ⇒ 留在这里也用不上 |
| 108 妖侧本体（蕾米莉亚） | 9 | YOUKAI_REMILIA_S11..S19 | 同上 |
| 116 季节点 / 季节点脚本族 | 1 | SEASON_SEED | `script114`（season_seed）死 ⇒ 这张 32×32 无人引用（shooterset 的四季各走自己的 `SEASON_BULLET_*` / `SEASON_LASER_BEAM`，都不经过 114） |
| 113 神灵点备用 | 1 | TPOINT | `script188` 死 |
| 110 信仰点备用 | 1 | FAITH_DOT | `script185` 死（110/111 现在借 116 的 `PT_*`） |
| 107 樱点备用 | 2 | SAKURA_POINT、SAKURA_SMALL | `script186`/`script187` 死（107 的结界走 `SENRO_*`） |
| 117 鱼灵/道具备用图 | 6 | FISH_POINT、ICON_FISH_POINT、ITEM_P、ITEM_MONEY、ITEM_LIFE、ITEM_BOMB | `FISH_POINT`/`ICON_FISH_POINT` 已被用户手绘的 `FISH_MONEY`/`ICON_FISH_MONEY` 取代；`ITEM_*` 是「鱼灵换道具」的备用图 |
| 117 卡面备用 | 3 | CARD_BOMB、CARD_LIFE、CARD_MONEY | `animal.c:615` 注释：「已回退作废、贴图留在 ORDER 里备用」 |
| 117 槽框备用 | 1 | GAUGE_FRAME | `script144` 死（实际用的是 `script145` + `script154`/`script156`） |
| **合计** | **70** | | |

**建议：一张都不删**。理由：① 合并后 entry 只要 49 个（余量 206），删它们省不到 entry（它们本来就并进已有图集）；
② 别人的卡随时可能新引用；③ 真要删就单独走一次 PR，别和重排混在一起。**唯一例外**是 108 那 55 张——
见 §7-①（它们不是「没人用」，是「用不上」，属于放错文件）。

## 4. 合并方案

### 4.1 六条设计规则（每条都对应一个会踩的坑）

1. **每个成员的矩形 = 成员自己那张图的像素尺寸**（不补边、不放大、不缩小）。
   这样 `th18:0x481703`（`vm+0x74 × vm+0x54` = 矩形宽 × `scale.x`）算出来的宽度与现在逐像素相同 ⇒
   脚本里的 `scale()` / C 侧的 `ce_anm_set_scale` 行为**一个字节都不变**。
   ⇒ 这同时把「48×32 被补进 64×64」那类坑（§CD 第 4 条）从**结构上**消掉了：图集里没有「补边」这回事。
2. **算术族必须连续升序**（合并后最容易被忽略的一条）：`TS_DIGIT_0..9`、`FONT2_0..9`、
   `AURA_WOLF_0..7`、`AURA_OTTER_0..7`、`AURA_EAGLE_0..7` —— 见 §5.1。方案里都排成单图集内连续。
3. **成员名一个都不改** ⇒ `anm_ids.h` 里仍然逐名生成 `CE_ANM_ABILITY_SPRITE_<NAME>`（值 = 图集内的 sprite 号），
   脚本正文仍然写 `@NAME` ⇒ **卡侧与脚本零改动**（唯一例外见 §5.2 的 107）。
4. 画布尺寸 = 货架式打包的结果，**允许非 2 的幂**（`build_ability.py:60` 已放开），上限仍是硬门里的 4096。
5. **不与「别人的卡」混编**：他们的 19 张单图 entry 原样保留（他们的美术还在长，混进来以后动一次就要动两家）。
6. **`FONT2_*` 是唯一例外**：成员矩形取 **16×32**（= 现在的画布，不是源图的 16×20），
   因为 `native/sdk.c:690` 的 `+5.0f * sy * dat` 是按「32 高的矩形里 20 行字形居中」标定的；
   保持 16×32 等价于**零行为变化**。若改成 16×20，必须同时把那个 `+5.0f` 改成 `-1.0f * sy * dat`（§5.1）。

### 4.2 图集总表（23 个图集 = 系统卡 21 + 共享 HUD 2）

> ⚠ **实装时少排了 2 个**：`ATLAS_YK_BODY`（46 帧）与 `ATLAS_YK_REMILIA`（9 帧）—— 108 妖侧本体，
> 用户 2026-09-13 裁决「连接暂时删除、其本身不要删除」⇒ **55 张源 PNG 一个没删**，只是不进图集；
> 它们引用的 55 个名字由 `ORDER.txt` 的 `REF` 段指向 1×1 全透明的 `RETIRED_BLANK`（**不占美术**）。
> ⇒ 实装是 **21 个图集**，且该表里 entry38 之后的号**整体前移 2**。现行号见 §9.2（或直接看 
> `native/anm_ids.h`）。

| 图集 | 卡 | 画布 WxH | 成员 | 新 entry | 新 sprite 号范围 | 说明 |
| --- | --- | --- | --- | --- | --- | --- |
| `ATLAS_HUD_DIGIT` | 共享 | 128×96 | 10 | entry7 | sprite109…118 | ce_hud_digit/ce_hud_number 的 10 个数字（sdk.c 用 +v 算术 ⇒ 必须连续升序） |
| `ATLAS_FONT2` | 共享 | 128×64 | 14 | entry8 | sprite119…132 | ce_hud_vm_text 的字形（sdk.c 用 FONT2_0 + (ch-'0') 算术 ⇒ 0..9 必须连续升序） |
| `ATLAS_LOLK` | 115 | 256×352 | 8 | entry28 | sprite152…159 | 绀珠传面板 7 态 + 环 |
| `ATLAS_SEASON_BULLET` | 116 | 192×208 | 23 | entry29 | sprite160…182 | 四季弹与命中特效 + 季节点种子 |
| `ATLAS_SEASON_UI` | 116 | 256×160 | 20 | entry30 | sprite183…202 | 季节子机 / 季节点 / 档位字 / 量表 / 激光 |
| `ATLAS_ANIMAL_TOKEN` | 117 | 128×160 | 19 | entry31 | sprite203…221 | 动物灵 9 帧 + 鱼灵 5 张 + 槽内鱼形图标 5 张（32×32 一族） |
| `ATLAS_ANIMAL_64` | 117 | 320×576 | 43 | entry32 | sprite222…264 | 类型光环 24 帧（狼/獭/鹫 各 8，必须各自连续）+ 咆哮火焰 12 + 槽图标 + 备用卡面 |
| `ATLAS_ANIMAL_SMALL` | 117 | 128×32 | 15 | entry33 | sprite265…279 | 灵槽空槽（灵魂 11 帧）+ 4 张道具灵备用图（16×16） |
| `ATLAS_ANIMAL_FX` | 117 | 128×192 | 2 | entry34 | sprite280…281 | 霊撃环 + 水獭光球 |
| `ATLAS_ANIMAL_BEAM` | 117 | 512×96 | 3 | entry35 | sprite282…284 | 终符光束 / 鹫光束 / 灵击长条（512×32 ×3） |
| `ATLAS_SENRO` | 107 | 256×320 | 4 | entry36 | sprite285…288 | 樱点/结界（FIELD 256×256 + RING 64×64 + 两个 16×16） |
| `ATLAS_YK_FAM` | 108 | 128×256 | 32 | entry37 | sprite289…320 | 四色使魔 4 套 × 8 帧 |
| `ATLAS_YK_BODY` | 108 | 256×384 | 46 | entry38 | sprite321…366 | 妖侧本体 24 帧 ×3（紫/爱丽丝/文）+ 蕾米莉亚 9 帧（全 dead，先留着） |
| `ATLAS_YK_REMILIA` | 108 | 192×192 | 9 | entry39 | sprite367…375 | 蕾米莉亚妖侧本体 9 帧（dead） |
| `ATLAS_YK_OPT` | 108 | 160×176 | 22 | entry40 | sprite376…397 | 子机（紫 9 + 爱丽丝 4 + 蕾米 1 + 文 8） |
| `ATLAS_MOF_FAITH` | 110 | 128×16 | 12 | entry41 | sprite398…409 | 信仰点图标 + 8×16 数字（mof.c 用逐名数组 ⇒ 无连续性要求） |
| `ATLAS_SIGNAL` | 111 | 128×32 | 3 | entry42 | sprite410…412 | 信号槽底板 / 刻度杆 / 图标 |
| `ATLAS_UFO_BODY` | 112 | 256×256 | 16 | entry43 | sprite413…428 | 四色飞碟本体各 4 帧 |
| `ATLAS_UFO_TOKEN` | 112 | 128×224 | 13 | entry44 | sprite429…441 | 令牌 6 组 ×2 帧 + 量表弧底 |
| `ATLAS_TRANCE_SPIRIT` | 113 | 128×96 | 11 | entry45 | sprite442…452 | 神灵四型的 普通/灵界 各一对（+ TPOINT 备用） |
| `ATLAS_TRANCE_GAUGE` | 113 | 128×128 | 9 | entry46 | sprite453…461 | 左下角指示槽：底/条/刻度/框两态/三档提示/标记 |
| `ATLAS_TRANCE_POPUP` | 113 | 128×16 | 6 | entry47 | sprite462…467 | 吃到神灵的 6 张数字弹窗（24×8 与 32×8 混排） |
| `ATLAS_TRANCE_ASTRAL` | 113 | 512×512 | 1 | entry48 | sprite468…468 | 灵界背景（512×512；卡侧默认关，`TRANCE_BG_ENABLE=0`） |

### 4.3 成员明细（矩形 = 成员原图的像素尺寸，坐标是画布左上角起）

#### `ATLAS_HUD_DIGIT` — 画布 128×96，entry7，sprite109…118

| 成员 | 原图 | 矩形 x,y,w,h | 新 sprite | 旧 sprite |
| --- | --- | --- | --- | --- |
| `TS_DIGIT_0` | `ability/truesilver/DIGIT_0.png` | 0,0,32,32 | 109 | 152 |
| `TS_DIGIT_1` | `ability/truesilver/DIGIT_1.png` | 32,0,32,32 | 110 | 153 |
| `TS_DIGIT_2` | `ability/truesilver/DIGIT_2.png` | 64,0,32,32 | 111 | 154 |
| `TS_DIGIT_3` | `ability/truesilver/DIGIT_3.png` | 96,0,32,32 | 112 | 155 |
| `TS_DIGIT_4` | `ability/truesilver/DIGIT_4.png` | 0,32,32,32 | 113 | 156 |
| `TS_DIGIT_5` | `ability/truesilver/DIGIT_5.png` | 32,32,32,32 | 114 | 157 |
| `TS_DIGIT_6` | `ability/truesilver/DIGIT_6.png` | 64,32,32,32 | 115 | 158 |
| `TS_DIGIT_7` | `ability/truesilver/DIGIT_7.png` | 96,32,32,32 | 116 | 159 |
| `TS_DIGIT_8` | `ability/truesilver/DIGIT_8.png` | 0,64,32,32 | 117 | 160 |
| `TS_DIGIT_9` | `ability/truesilver/DIGIT_9.png` | 32,64,32,32 | 118 | 161 |

#### `ATLAS_FONT2` — 画布 128×64，entry8，sprite119…132

| 成员 | 原图 | 矩形 x,y,w,h | 新 sprite | 旧 sprite |
| --- | --- | --- | --- | --- |
| `FONT2_0` | `ability/font2/FONT2_0.png` | 0,0,16,32 | 119 | 215 |
| `FONT2_1` | `ability/font2/FONT2_1.png` | 16,0,16,32 | 120 | 216 |
| `FONT2_2` | `ability/font2/FONT2_2.png` | 32,0,16,32 | 121 | 217 |
| `FONT2_3` | `ability/font2/FONT2_3.png` | 48,0,16,32 | 122 | 218 |
| `FONT2_4` | `ability/font2/FONT2_4.png` | 64,0,16,32 | 123 | 219 |
| `FONT2_5` | `ability/font2/FONT2_5.png` | 80,0,16,32 | 124 | 220 |
| `FONT2_6` | `ability/font2/FONT2_6.png` | 96,0,16,32 | 125 | 221 |
| `FONT2_7` | `ability/font2/FONT2_7.png` | 112,0,16,32 | 126 | 222 |
| `FONT2_8` | `ability/font2/FONT2_8.png` | 0,32,16,32 | 127 | 223 |
| `FONT2_9` | `ability/font2/FONT2_9.png` | 16,32,16,32 | 128 | 224 |
| `FONT2_MINUS` | `ability/font2/FONT2_MINUS.png` | 32,32,16,32 | 129 | 225 |
| `FONT2_PERCENT` | `ability/font2/FONT2_PERCENT.png` | 48,32,16,32 | 130 | 226 |
| `FONT2_DOT` | `ability/font2/FONT2_DOT.png` | 64,32,16,32 | 131 | 227 |
| `FONT2_PLUS` | `ability/font2/FONT2_PLUS.png` | 80,32,16,32 | 132 | 228 |

#### `ATLAS_LOLK` — 画布 256×352，entry28，sprite152…159

| 成员 | 原图 | 矩形 x,y,w,h | 新 sprite | 旧 sprite |
| --- | --- | --- | --- | --- |
| `LOLK_RING` | `ability/lolk/RING.png` | 0,0,128,128 | 152 | 164 |
| `LOLK_PANEL_1` | `ability/lolk/PANEL_1.png` | 0,128,256,32 | 153 | 165 |
| `LOLK_PANEL_2` | `ability/lolk/PANEL_2.png` | 0,160,256,32 | 154 | 166 |
| `LOLK_PANEL_3` | `ability/lolk/PANEL_3.png` | 0,192,256,32 | 155 | 167 |
| `LOLK_PANEL_4` | `ability/lolk/PANEL_4.png` | 0,224,256,32 | 156 | 168 |
| `LOLK_PANEL_5` | `ability/lolk/PANEL_5.png` | 0,256,256,32 | 157 | 169 |
| `LOLK_PANEL_6` | `ability/lolk/PANEL_6.png` | 0,288,256,32 | 158 | 170 |
| `LOLK_PANEL_7` | `ability/lolk/PANEL_7.png` | 0,320,256,32 | 159 | 171 |

#### `ATLAS_SEASON_BULLET` — 画布 192×208，entry29，sprite160…182

| 成员 | 原图 | 矩形 x,y,w,h | 新 sprite | 旧 sprite |
| --- | --- | --- | --- | --- |
| `SEASON_SEED` | `ability/season/SEASON_SEED.png` | 0,0,32,32 | 160 | 173 |
| `BULLET_SPRING_A` | `ability/season/BULLET_SPRING_A.png` | 32,0,32,32 | 161 | 191 |
| `BULLET_SPRING_B` | `ability/season/BULLET_SPRING_B.png` | 64,0,32,32 | 162 | 192 |
| `HIT_SPRING_1` | `ability/season/HIT_SPRING_1.png` | 96,0,32,32 | 163 | 193 |
| `HIT_SPRING_2` | `ability/season/HIT_SPRING_2.png` | 128,0,32,32 | 164 | 194 |
| `HIT_SPRING_3` | `ability/season/HIT_SPRING_3.png` | 160,0,32,32 | 165 | 195 |
| `HIT_SPRING_4` | `ability/season/HIT_SPRING_4.png` | 0,32,32,32 | 166 | 196 |
| `HIT_SPRING_5` | `ability/season/HIT_SPRING_5.png` | 32,32,32,32 | 167 | 197 |
| `HIT_SPRING_6` | `ability/season/HIT_SPRING_6.png` | 64,32,32,32 | 168 | 198 |
| `HIT_SPRING_7` | `ability/season/HIT_SPRING_7.png` | 96,32,32,32 | 169 | 199 |
| `HIT_SPRING_8` | `ability/season/HIT_SPRING_8.png` | 128,32,32,32 | 170 | 200 |
| `BULLET_SUMMER` | `ability/season/BULLET_SUMMER.png` | 160,32,16,16 | 171 | 201 |
| `HIT_SUMMER_1` | `ability/season/HIT_SUMMER_1.png` | 176,32,16,16 | 172 | 202 |
| `HIT_SUMMER_2` | `ability/season/HIT_SUMMER_2.png` | 0,64,16,16 | 173 | 203 |
| `HIT_SUMMER_3` | `ability/season/HIT_SUMMER_3.png` | 16,64,16,16 | 174 | 204 |
| `BULLET_FALL` | `ability/season/BULLET_FALL.png` | 32,64,64,64 | 175 | 205 |
| `HIT_FALL_1` | `ability/season/HIT_FALL_1.png` | 96,64,64,64 | 176 | 206 |
| `HIT_FALL_2` | `ability/season/HIT_FALL_2.png` | 0,128,64,64 | 177 | 207 |
| `HIT_FALL_3` | `ability/season/HIT_FALL_3.png` | 64,128,64,64 | 178 | 208 |
| `BULLET_DOYOU` | `ability/season/BULLET_DOYOU.png` | 128,128,32,16 | 179 | 209 |
| `HIT_DOYOU_1` | `ability/season/HIT_DOYOU_1.png` | 160,128,32,16 | 180 | 210 |
| `HIT_DOYOU_2` | `ability/season/HIT_DOYOU_2.png` | 0,192,32,16 | 181 | 211 |
| `HIT_DOYOU_3` | `ability/season/HIT_DOYOU_3.png` | 32,192,32,16 | 182 | 212 |

#### `ATLAS_SEASON_UI` — 画布 256×160，entry30，sprite183…202

| 成员 | 原图 | 矩形 x,y,w,h | 新 sprite | 旧 sprite |
| --- | --- | --- | --- | --- |
| `SEASON_OPT` | `ability/season/SEASON_OPT.png` | 0,0,64,64 | 183 | 172 |
| `PT_SPRING` | `ability/season/PT_SPRING.png` | 64,0,16,16 | 184 | 174 |
| `PT_SUMMER` | `ability/season/PT_SUMMER.png` | 80,0,16,16 | 185 | 175 |
| `PT_FALL` | `ability/season/PT_FALL.png` | 96,0,16,16 | 186 | 176 |
| `PT_WINTER` | `ability/season/PT_WINTER.png` | 112,0,16,16 | 187 | 177 |
| `PT_DOYOU` | `ability/season/PT_DOYOU.png` | 128,0,16,16 | 188 | 178 |
| `GLYPH_SPRING` | `ability/season/GLYPH_SPRING.png` | 144,0,32,32 | 189 | 179 |
| `GLYPH_SUMMER` | `ability/season/GLYPH_SUMMER.png` | 176,0,32,32 | 190 | 180 |
| `GLYPH_FALL` | `ability/season/GLYPH_FALL.png` | 208,0,32,32 | 191 | 181 |
| `GLYPH_WINTER` | `ability/season/GLYPH_WINTER.png` | 0,64,32,32 | 192 | 182 |
| `GLYPH_DOYOU` | `ability/season/GLYPH_DOYOU.png` | 32,64,32,32 | 193 | 183 |
| `OPT_SPRING` | `ability/season/OPT_SPRING.png` | 64,64,16,16 | 194 | 184 |
| `OPT_SUMMER` | `ability/season/OPT_SUMMER.png` | 80,64,16,16 | 195 | 185 |
| `OPT_FALL` | `ability/season/OPT_FALL.png` | 96,64,16,16 | 196 | 186 |
| `OPT_WINTER` | `ability/season/OPT_WINTER.png` | 112,64,16,16 | 197 | 187 |
| `OPT_DOYOU` | `ability/season/OPT_DOYOU.png` | 128,64,16,16 | 198 | 188 |
| `SEASON_LASER` | `ability/season/SEASON_LASER.png` | 0,96,256,32 | 199 | 189 |
| `SEASON_STRIP` | `ability/season/SEASON_STRIP.png` | 0,128,64,16 | 200 | 190 |
| `LASER_CHILD` | `ability/season/LASER_CHILD.png` | 64,128,32,32 | 201 | 213 |
| `LASER_HIT` | `ability/season/LASER_HIT.png` | 96,128,32,32 | 202 | 214 |

#### `ATLAS_ANIMAL_TOKEN` — 画布 128×160，entry31，sprite203…221

| 成员 | 原图 | 矩形 x,y,w,h | 新 sprite | 旧 sprite |
| --- | --- | --- | --- | --- |
| `WOLF_0` | `ability/animal/WOLF_0.png` | 0,0,32,32 | 203 | 229 |
| `WOLF_1` | `ability/animal/WOLF_1.png` | 32,0,32,32 | 204 | 230 |
| `WOLF_2` | `ability/animal/WOLF_2.png` | 64,0,32,32 | 205 | 231 |
| `OTTER_0` | `ability/animal/OTTER_0.png` | 96,0,32,32 | 206 | 232 |
| `OTTER_1` | `ability/animal/OTTER_1.png` | 0,32,32,32 | 207 | 233 |
| `OTTER_2` | `ability/animal/OTTER_2.png` | 32,32,32,32 | 208 | 234 |
| `EAGLE_0` | `ability/animal/EAGLE_0.png` | 64,32,32,32 | 209 | 235 |
| `EAGLE_1` | `ability/animal/EAGLE_1.png` | 96,32,32,32 | 210 | 236 |
| `EAGLE_2` | `ability/animal/EAGLE_2.png` | 0,64,32,32 | 211 | 237 |
| `FISH_BOMB` | `ability/animal/FISH_BOMB.png` | 32,64,32,32 | 212 | 238 |
| `FISH_LIFE` | `ability/animal/FISH_LIFE.png` | 64,64,32,32 | 213 | 239 |
| `FISH_POWER` | `ability/animal/FISH_POWER.png` | 96,64,32,32 | 214 | 240 |
| `FISH_POINT` | `ability/animal/FISH_POINT.png` | 0,96,32,32 | 215 | 241 |
| `FISH_MONEY` | `ability/animal_custom/FISH_MONEY.png` | 32,96,32,32 | 216 | 306 |
| `ICON_FISH_BOMB` | `ability/animal/ICON_FISH_BOMB.png` | 64,96,32,32 | 217 | 299 |
| `ICON_FISH_LIFE` | `ability/animal/ICON_FISH_LIFE.png` | 96,96,32,32 | 218 | 300 |
| `ICON_FISH_POWER` | `ability/animal/ICON_FISH_POWER.png` | 0,128,32,32 | 219 | 301 |
| `ICON_FISH_POINT` | `ability/animal/ICON_FISH_POINT.png` | 32,128,32,32 | 220 | 302 |
| `ICON_FISH_MONEY` | `ability/animal_custom/ICON_FISH_MONEY.png` | 64,128,32,32 | 221 | 307 |

#### `ATLAS_ANIMAL_64` — 画布 320×576，entry32，sprite222…264

| 成员 | 原图 | 矩形 x,y,w,h | 新 sprite | 旧 sprite |
| --- | --- | --- | --- | --- |
| `AURA_WOLF_0` | `ability/animal/AURA_WOLF_0.png` | 0,0,64,64 | 222 | 251 |
| `AURA_WOLF_1` | `ability/animal/AURA_WOLF_1.png` | 64,0,64,64 | 223 | 252 |
| `AURA_WOLF_2` | `ability/animal/AURA_WOLF_2.png` | 128,0,64,64 | 224 | 253 |
| `AURA_WOLF_3` | `ability/animal/AURA_WOLF_3.png` | 192,0,64,64 | 225 | 254 |
| `AURA_WOLF_4` | `ability/animal/AURA_WOLF_4.png` | 256,0,64,64 | 226 | 255 |
| `AURA_WOLF_5` | `ability/animal/AURA_WOLF_5.png` | 0,64,64,64 | 227 | 256 |
| `AURA_WOLF_6` | `ability/animal/AURA_WOLF_6.png` | 64,64,64,64 | 228 | 257 |
| `AURA_WOLF_7` | `ability/animal/AURA_WOLF_7.png` | 128,64,64,64 | 229 | 258 |
| `AURA_OTTER_0` | `ability/animal/AURA_OTTER_0.png` | 192,64,64,64 | 230 | 259 |
| `AURA_OTTER_1` | `ability/animal/AURA_OTTER_1.png` | 256,64,64,64 | 231 | 260 |
| `AURA_OTTER_2` | `ability/animal/AURA_OTTER_2.png` | 0,128,64,64 | 232 | 261 |
| `AURA_OTTER_3` | `ability/animal/AURA_OTTER_3.png` | 64,128,64,64 | 233 | 262 |
| `AURA_OTTER_4` | `ability/animal/AURA_OTTER_4.png` | 128,128,64,64 | 234 | 263 |
| `AURA_OTTER_5` | `ability/animal/AURA_OTTER_5.png` | 192,128,64,64 | 235 | 264 |
| `AURA_OTTER_6` | `ability/animal/AURA_OTTER_6.png` | 256,128,64,64 | 236 | 265 |
| `AURA_OTTER_7` | `ability/animal/AURA_OTTER_7.png` | 0,192,64,64 | 237 | 266 |
| `AURA_EAGLE_0` | `ability/animal/AURA_EAGLE_0.png` | 64,192,64,64 | 238 | 243 |
| `AURA_EAGLE_1` | `ability/animal/AURA_EAGLE_1.png` | 128,192,64,64 | 239 | 244 |
| `AURA_EAGLE_2` | `ability/animal/AURA_EAGLE_2.png` | 192,192,64,64 | 240 | 245 |
| `AURA_EAGLE_3` | `ability/animal/AURA_EAGLE_3.png` | 256,192,64,64 | 241 | 246 |
| `AURA_EAGLE_4` | `ability/animal/AURA_EAGLE_4.png` | 0,256,64,64 | 242 | 247 |
| `AURA_EAGLE_5` | `ability/animal/AURA_EAGLE_5.png` | 64,256,64,64 | 243 | 248 |
| `AURA_EAGLE_6` | `ability/animal/AURA_EAGLE_6.png` | 128,256,64,64 | 244 | 249 |
| `AURA_EAGLE_7` | `ability/animal/AURA_EAGLE_7.png` | 192,256,64,64 | 245 | 250 |
| `FLAME_0` | `ability/animal/FLAME_0.png` | 256,256,64,64 | 246 | 284 |
| `FLAME_1` | `ability/animal/FLAME_1.png` | 0,320,64,64 | 247 | 285 |
| `FLAME_2` | `ability/animal/FLAME_2.png` | 64,320,64,64 | 248 | 286 |
| `FLAME_3` | `ability/animal/FLAME_3.png` | 128,320,64,64 | 249 | 287 |
| `FLAME_4` | `ability/animal/FLAME_4.png` | 192,320,64,64 | 250 | 288 |
| `FLAME_5` | `ability/animal/FLAME_5.png` | 256,320,64,64 | 251 | 289 |
| `FLAME_6` | `ability/animal/FLAME_6.png` | 0,384,64,64 | 252 | 290 |
| `FLAME_7` | `ability/animal/FLAME_7.png` | 64,384,64,64 | 253 | 291 |
| `FLAME_8` | `ability/animal/FLAME_8.png` | 128,384,64,64 | 254 | 292 |
| `FLAME_9` | `ability/animal/FLAME_9.png` | 192,384,64,64 | 255 | 293 |
| `FLAME_10` | `ability/animal/FLAME_10.png` | 256,384,64,64 | 256 | 294 |
| `FLAME_11` | `ability/animal/FLAME_11.png` | 0,448,64,64 | 257 | 295 |
| `ICON_WOLF` | `ability/animal/ICON_WOLF.png` | 64,448,64,64 | 258 | 296 |
| `ICON_OTTER` | `ability/animal/ICON_OTTER.png` | 128,448,64,64 | 259 | 297 |
| `ICON_EAGLE` | `ability/animal/ICON_EAGLE.png` | 192,448,64,64 | 260 | 298 |
| `GAUGE_FRAME` | `ability/animal/GAUGE_FRAME.png` | 256,448,64,64 | 261 | 242 |
| `CARD_BOMB` | `ability/animal/CARD_BOMB.png` | 0,512,64,64 | 262 | 303 |
| `CARD_LIFE` | `ability/animal/CARD_LIFE.png` | 64,512,64,64 | 263 | 304 |
| `CARD_MONEY` | `ability/animal/CARD_MONEY.png` | 128,512,64,64 | 264 | 305 |

#### `ATLAS_ANIMAL_SMALL` — 画布 128×32，entry33，sprite265…279

| 成员 | 原图 | 矩形 x,y,w,h | 新 sprite | 旧 sprite |
| --- | --- | --- | --- | --- |
| `SOUL_0` | `ability/animal/SOUL_0.png` | 0,0,16,16 | 265 | 269 |
| `SOUL_1` | `ability/animal/SOUL_1.png` | 16,0,16,16 | 266 | 270 |
| `SOUL_2` | `ability/animal/SOUL_2.png` | 32,0,16,16 | 267 | 271 |
| `SOUL_3` | `ability/animal/SOUL_3.png` | 48,0,16,16 | 268 | 272 |
| `SOUL_4` | `ability/animal/SOUL_4.png` | 64,0,16,16 | 269 | 273 |
| `SOUL_5` | `ability/animal/SOUL_5.png` | 80,0,16,16 | 270 | 274 |
| `SOUL_6` | `ability/animal/SOUL_6.png` | 96,0,16,16 | 271 | 275 |
| `SOUL_7` | `ability/animal/SOUL_7.png` | 112,0,16,16 | 272 | 276 |
| `SOUL_8` | `ability/animal/SOUL_8.png` | 0,16,16,16 | 273 | 277 |
| `SOUL_9` | `ability/animal/SOUL_9.png` | 16,16,16,16 | 274 | 278 |
| `SOUL_10` | `ability/animal/SOUL_10.png` | 32,16,16,16 | 275 | 279 |
| `ITEM_P` | `ability/animal/ITEM_P.png` | 48,16,16,16 | 276 | 280 |
| `ITEM_MONEY` | `ability/animal/ITEM_MONEY.png` | 64,16,16,16 | 277 | 281 |
| `ITEM_LIFE` | `ability/animal/ITEM_LIFE.png` | 80,16,16,16 | 278 | 282 |
| `ITEM_BOMB` | `ability/animal/ITEM_BOMB.png` | 96,16,16,16 | 279 | 283 |

#### `ATLAS_ANIMAL_FX` — 画布 128×192，entry34，sprite280…281

| 成员 | 原图 | 矩形 x,y,w,h | 新 sprite | 旧 sprite |
| --- | --- | --- | --- | --- |
| `STRIKE_RING` | `ability/animal/STRIKE_RING.png` | 0,0,128,128 | 280 | 268 |
| `OTTER_ORB` | `ability/animal/OTTER_ORB.png` | 0,128,128,64 | 281 | 267 |

#### `ATLAS_ANIMAL_BEAM` — 画布 512×96，entry35，sprite282…284

| 成员 | 原图 | 矩形 x,y,w,h | 新 sprite | 旧 sprite |
| --- | --- | --- | --- | --- |
| `BEASTAP_A` | `ability/animal/BEASTAP_A.png` | 0,0,512,32 | 282 | 308 |
| `BEASTAP_B` | `ability/animal/BEASTAP_B.png` | 0,32,512,32 | 283 | 309 |
| `EAGLE_BEAM` | `ability/animal/EAGLE_BEAM.png` | 0,64,512,32 | 284 | 310 |

#### `ATLAS_SENRO` — 画布 256×320，entry36，sprite285…288

| 成员 | 原图 | 矩形 x,y,w,h | 新 sprite | 旧 sprite |
| --- | --- | --- | --- | --- |
| `SENRO_FIELD` | `ability/senro/FIELD.png` | 0,0,256,256 | 285 | 406 |
| `SENRO_RING` | `ability/senro/RING.png` | 0,256,64,64 | 286 | 407 |
| `SAKURA_POINT` | `ability/sakura/POINT.png` | 64,256,16,16 | 287 | 340 |
| `SAKURA_SMALL` | `ability/sakura/SMALL.png` | 80,256,16,16 | 288 | 341 |

#### `ATLAS_YK_FAM` — 画布 128×256，entry37，sprite289…320

| 成员 | 原图 | 矩形 x,y,w,h | 新 sprite | 旧 sprite |
| --- | --- | --- | --- | --- |
| `YOUKAI_FAM_S53_0` | `ability/youkai_familiar/YOUKAI_FAM_S53_0.png` | 0,0,32,32 | 289 | 374 |
| `YOUKAI_FAM_S53_1` | `ability/youkai_familiar/YOUKAI_FAM_S53_1.png` | 32,0,32,32 | 290 | 375 |
| `YOUKAI_FAM_S53_2` | `ability/youkai_familiar/YOUKAI_FAM_S53_2.png` | 64,0,32,32 | 291 | 376 |
| `YOUKAI_FAM_S53_3` | `ability/youkai_familiar/YOUKAI_FAM_S53_3.png` | 96,0,32,32 | 292 | 377 |
| `YOUKAI_FAM_S53_4` | `ability/youkai_familiar/YOUKAI_FAM_S53_4.png` | 0,32,32,32 | 293 | 378 |
| `YOUKAI_FAM_S53_5` | `ability/youkai_familiar/YOUKAI_FAM_S53_5.png` | 32,32,32,32 | 294 | 379 |
| `YOUKAI_FAM_S53_6` | `ability/youkai_familiar/YOUKAI_FAM_S53_6.png` | 64,32,32,32 | 295 | 380 |
| `YOUKAI_FAM_S53_7` | `ability/youkai_familiar/YOUKAI_FAM_S53_7.png` | 96,32,32,32 | 296 | 381 |
| `YOUKAI_FAM_S54_0` | `ability/youkai_familiar/YOUKAI_FAM_S54_0.png` | 0,64,32,32 | 297 | 382 |
| `YOUKAI_FAM_S54_1` | `ability/youkai_familiar/YOUKAI_FAM_S54_1.png` | 32,64,32,32 | 298 | 383 |
| `YOUKAI_FAM_S54_2` | `ability/youkai_familiar/YOUKAI_FAM_S54_2.png` | 64,64,32,32 | 299 | 384 |
| `YOUKAI_FAM_S54_3` | `ability/youkai_familiar/YOUKAI_FAM_S54_3.png` | 96,64,32,32 | 300 | 385 |
| `YOUKAI_FAM_S54_4` | `ability/youkai_familiar/YOUKAI_FAM_S54_4.png` | 0,96,32,32 | 301 | 386 |
| `YOUKAI_FAM_S54_5` | `ability/youkai_familiar/YOUKAI_FAM_S54_5.png` | 32,96,32,32 | 302 | 387 |
| `YOUKAI_FAM_S54_6` | `ability/youkai_familiar/YOUKAI_FAM_S54_6.png` | 64,96,32,32 | 303 | 388 |
| `YOUKAI_FAM_S54_7` | `ability/youkai_familiar/YOUKAI_FAM_S54_7.png` | 96,96,32,32 | 304 | 389 |
| `YOUKAI_FAM_S55_0` | `ability/youkai_familiar/YOUKAI_FAM_S55_0.png` | 0,128,32,32 | 305 | 390 |
| `YOUKAI_FAM_S55_1` | `ability/youkai_familiar/YOUKAI_FAM_S55_1.png` | 32,128,32,32 | 306 | 391 |
| `YOUKAI_FAM_S55_2` | `ability/youkai_familiar/YOUKAI_FAM_S55_2.png` | 64,128,32,32 | 307 | 392 |
| `YOUKAI_FAM_S55_3` | `ability/youkai_familiar/YOUKAI_FAM_S55_3.png` | 96,128,32,32 | 308 | 393 |
| `YOUKAI_FAM_S55_4` | `ability/youkai_familiar/YOUKAI_FAM_S55_4.png` | 0,160,32,32 | 309 | 394 |
| `YOUKAI_FAM_S55_5` | `ability/youkai_familiar/YOUKAI_FAM_S55_5.png` | 32,160,32,32 | 310 | 395 |
| `YOUKAI_FAM_S55_6` | `ability/youkai_familiar/YOUKAI_FAM_S55_6.png` | 64,160,32,32 | 311 | 396 |
| `YOUKAI_FAM_S55_7` | `ability/youkai_familiar/YOUKAI_FAM_S55_7.png` | 96,160,32,32 | 312 | 397 |
| `YOUKAI_FAM_S56_0` | `ability/youkai_familiar/YOUKAI_FAM_S56_0.png` | 0,192,32,32 | 313 | 398 |
| `YOUKAI_FAM_S56_1` | `ability/youkai_familiar/YOUKAI_FAM_S56_1.png` | 32,192,32,32 | 314 | 399 |
| `YOUKAI_FAM_S56_2` | `ability/youkai_familiar/YOUKAI_FAM_S56_2.png` | 64,192,32,32 | 315 | 400 |
| `YOUKAI_FAM_S56_3` | `ability/youkai_familiar/YOUKAI_FAM_S56_3.png` | 96,192,32,32 | 316 | 401 |
| `YOUKAI_FAM_S56_4` | `ability/youkai_familiar/YOUKAI_FAM_S56_4.png` | 0,224,32,32 | 317 | 402 |
| `YOUKAI_FAM_S56_5` | `ability/youkai_familiar/YOUKAI_FAM_S56_5.png` | 32,224,32,32 | 318 | 403 |
| `YOUKAI_FAM_S56_6` | `ability/youkai_familiar/YOUKAI_FAM_S56_6.png` | 64,224,32,32 | 319 | 404 |
| `YOUKAI_FAM_S56_7` | `ability/youkai_familiar/YOUKAI_FAM_S56_7.png` | 96,224,32,32 | 320 | 405 |

#### `ATLAS_YK_BODY` — 画布 256×384，entry38，sprite321…366

| 成员 | 原图 | 矩形 x,y,w,h | 新 sprite | 旧 sprite |
| --- | --- | --- | --- | --- |
| `YOUKAI_YUKARI_S11` | `ability/youkai/YOUKAI_YUKARI_S11.png` | 0,0,32,64 | 321 | 343 |
| `YOUKAI_YUKARI_S12` | `ability/youkai/YOUKAI_YUKARI_S12.png` | 32,0,32,64 | 322 | 344 |
| `YOUKAI_YUKARI_S13` | `ability/youkai/YOUKAI_YUKARI_S13.png` | 64,0,32,64 | 323 | 345 |
| `YOUKAI_YUKARI_S14` | `ability/youkai/YOUKAI_YUKARI_S14.png` | 96,0,32,64 | 324 | 346 |
| `YOUKAI_YUKARI_S15` | `ability/youkai/YOUKAI_YUKARI_S15.png` | 128,0,32,64 | 325 | 347 |
| `YOUKAI_YUKARI_S16` | `ability/youkai/YOUKAI_YUKARI_S16.png` | 160,0,32,64 | 326 | 348 |
| `YOUKAI_YUKARI_S17` | `ability/youkai/YOUKAI_YUKARI_S17.png` | 192,0,32,64 | 327 | 349 |
| `YOUKAI_YUKARI_S18` | `ability/youkai/YOUKAI_YUKARI_S18.png` | 224,0,32,64 | 328 | 350 |
| `YOUKAI_YUKARI_S19` | `ability/youkai/YOUKAI_YUKARI_S19.png` | 0,64,32,64 | 329 | 351 |
| `YOUKAI_YUKARI_S20` | `ability/youkai/YOUKAI_YUKARI_S20.png` | 32,64,32,64 | 330 | 352 |
| `YOUKAI_YUKARI_S21` | `ability/youkai/YOUKAI_YUKARI_S21.png` | 64,64,32,64 | 331 | 353 |
| `YOUKAI_ALICE_S11` | `ability/youkai/YOUKAI_ALICE_S11.png` | 96,64,32,64 | 332 | 354 |
| `YOUKAI_ALICE_S12` | `ability/youkai/YOUKAI_ALICE_S12.png` | 128,64,32,64 | 333 | 355 |
| `YOUKAI_ALICE_S13` | `ability/youkai/YOUKAI_ALICE_S13.png` | 160,64,32,64 | 334 | 356 |
| `YOUKAI_ALICE_S14` | `ability/youkai/YOUKAI_ALICE_S14.png` | 192,64,32,64 | 335 | 357 |
| `YOUKAI_ALICE_S15` | `ability/youkai/YOUKAI_ALICE_S15.png` | 224,64,32,64 | 336 | 358 |
| `YOUKAI_ALICE_S16` | `ability/youkai/YOUKAI_ALICE_S16.png` | 0,128,32,64 | 337 | 359 |
| `YOUKAI_ALICE_S17` | `ability/youkai/YOUKAI_ALICE_S17.png` | 32,128,32,64 | 338 | 360 |
| `YOUKAI_ALICE_S18` | `ability/youkai/YOUKAI_ALICE_S18.png` | 64,128,32,64 | 339 | 361 |
| `YOUKAI_ALICE_S19` | `ability/youkai/YOUKAI_ALICE_S19.png` | 96,128,32,64 | 340 | 362 |
| `YOUKAI_ALICE_S20` | `ability/youkai/YOUKAI_ALICE_S20.png` | 128,128,32,64 | 341 | 363 |
| `YOUKAI_ALICE_S21` | `ability/youkai/YOUKAI_ALICE_S21.png` | 160,128,32,64 | 342 | 364 |
| `YOUKAI_AYA_S0` | `ability/sanae_youkai/YOUKAI_AYA_S0.png` | 192,128,32,64 | 343 | 422 |
| `YOUKAI_AYA_S1` | `ability/sanae_youkai/YOUKAI_AYA_S1.png` | 224,128,32,64 | 344 | 423 |
| `YOUKAI_AYA_S2` | `ability/sanae_youkai/YOUKAI_AYA_S2.png` | 0,192,32,64 | 345 | 424 |
| `YOUKAI_AYA_S3` | `ability/sanae_youkai/YOUKAI_AYA_S3.png` | 32,192,32,64 | 346 | 425 |
| `YOUKAI_AYA_S4` | `ability/sanae_youkai/YOUKAI_AYA_S4.png` | 64,192,32,64 | 347 | 426 |
| `YOUKAI_AYA_S5` | `ability/sanae_youkai/YOUKAI_AYA_S5.png` | 96,192,32,64 | 348 | 427 |
| `YOUKAI_AYA_S6` | `ability/sanae_youkai/YOUKAI_AYA_S6.png` | 128,192,32,64 | 349 | 428 |
| `YOUKAI_AYA_S7` | `ability/sanae_youkai/YOUKAI_AYA_S7.png` | 160,192,32,64 | 350 | 429 |
| `YOUKAI_AYA_S8` | `ability/sanae_youkai/YOUKAI_AYA_S8.png` | 192,192,32,64 | 351 | 430 |
| `YOUKAI_AYA_S9` | `ability/sanae_youkai/YOUKAI_AYA_S9.png` | 224,192,32,64 | 352 | 431 |
| `YOUKAI_AYA_S10` | `ability/sanae_youkai/YOUKAI_AYA_S10.png` | 0,256,32,64 | 353 | 432 |
| `YOUKAI_AYA_S11` | `ability/sanae_youkai/YOUKAI_AYA_S11.png` | 32,256,32,64 | 354 | 433 |
| `YOUKAI_AYA_S12` | `ability/sanae_youkai/YOUKAI_AYA_S12.png` | 64,256,32,64 | 355 | 434 |
| `YOUKAI_AYA_S13` | `ability/sanae_youkai/YOUKAI_AYA_S13.png` | 96,256,32,64 | 356 | 435 |
| `YOUKAI_AYA_S14` | `ability/sanae_youkai/YOUKAI_AYA_S14.png` | 128,256,32,64 | 357 | 436 |
| `YOUKAI_AYA_S15` | `ability/sanae_youkai/YOUKAI_AYA_S15.png` | 160,256,32,64 | 358 | 437 |
| `YOUKAI_AYA_S16` | `ability/sanae_youkai/YOUKAI_AYA_S16.png` | 192,256,32,64 | 359 | 438 |
| `YOUKAI_AYA_S17` | `ability/sanae_youkai/YOUKAI_AYA_S17.png` | 224,256,32,64 | 360 | 439 |
| `YOUKAI_AYA_S18` | `ability/sanae_youkai/YOUKAI_AYA_S18.png` | 0,320,32,64 | 361 | 440 |
| `YOUKAI_AYA_S19` | `ability/sanae_youkai/YOUKAI_AYA_S19.png` | 32,320,32,64 | 362 | 441 |
| `YOUKAI_AYA_S20` | `ability/sanae_youkai/YOUKAI_AYA_S20.png` | 64,320,32,64 | 363 | 442 |
| `YOUKAI_AYA_S21` | `ability/sanae_youkai/YOUKAI_AYA_S21.png` | 96,320,32,64 | 364 | 443 |
| `YOUKAI_AYA_S22` | `ability/sanae_youkai/YOUKAI_AYA_S22.png` | 128,320,32,64 | 365 | 444 |
| `YOUKAI_AYA_S23` | `ability/sanae_youkai/YOUKAI_AYA_S23.png` | 160,320,32,64 | 366 | 445 |

#### `ATLAS_YK_REMILIA` — 画布 192×192，entry39，sprite367…375

| 成员 | 原图 | 矩形 x,y,w,h | 新 sprite | 旧 sprite |
| --- | --- | --- | --- | --- |
| `YOUKAI_REMILIA_S11` | `ability/youkai/YOUKAI_REMILIA_S11.png` | 0,0,64,64 | 367 | 365 |
| `YOUKAI_REMILIA_S12` | `ability/youkai/YOUKAI_REMILIA_S12.png` | 64,0,64,64 | 368 | 366 |
| `YOUKAI_REMILIA_S13` | `ability/youkai/YOUKAI_REMILIA_S13.png` | 128,0,64,64 | 369 | 367 |
| `YOUKAI_REMILIA_S14` | `ability/youkai/YOUKAI_REMILIA_S14.png` | 0,64,64,64 | 370 | 368 |
| `YOUKAI_REMILIA_S15` | `ability/youkai/YOUKAI_REMILIA_S15.png` | 64,64,64,64 | 371 | 369 |
| `YOUKAI_REMILIA_S16` | `ability/youkai/YOUKAI_REMILIA_S16.png` | 128,64,64,64 | 372 | 370 |
| `YOUKAI_REMILIA_S17` | `ability/youkai/YOUKAI_REMILIA_S17.png` | 0,128,64,64 | 373 | 371 |
| `YOUKAI_REMILIA_S18` | `ability/youkai/YOUKAI_REMILIA_S18.png` | 64,128,64,64 | 374 | 372 |
| `YOUKAI_REMILIA_S19` | `ability/youkai/YOUKAI_REMILIA_S19.png` | 128,128,64,64 | 375 | 373 |

#### `ATLAS_YK_OPT` — 画布 160×176，entry40，sprite376…397

| 成员 | 原图 | 矩形 x,y,w,h | 新 sprite | 旧 sprite |
| --- | --- | --- | --- | --- |
| `YOUKAI_OPT_YUKARI_S22` | `ability/youkai_option/YOUKAI_OPT_YUKARI_S22.png` | 0,0,32,64 | 376 | 446 |
| `YOUKAI_OPT_YUKARI_S23` | `ability/youkai_option/YOUKAI_OPT_YUKARI_S23.png` | 32,0,32,64 | 377 | 447 |
| `YOUKAI_OPT_YUKARI_S24` | `ability/youkai_option/YOUKAI_OPT_YUKARI_S24.png` | 64,0,32,64 | 378 | 448 |
| `YOUKAI_OPT_YUKARI_S25` | `ability/youkai_option/YOUKAI_OPT_YUKARI_S25.png` | 96,0,32,64 | 379 | 449 |
| `YOUKAI_OPT_YUKARI_S26` | `ability/youkai_option/YOUKAI_OPT_YUKARI_S26.png` | 128,0,32,64 | 380 | 450 |
| `YOUKAI_OPT_YUKARI_S27` | `ability/youkai_option/YOUKAI_OPT_YUKARI_S27.png` | 0,64,32,64 | 381 | 451 |
| `YOUKAI_OPT_YUKARI_S28` | `ability/youkai_option/YOUKAI_OPT_YUKARI_S28.png` | 32,64,32,64 | 382 | 452 |
| `YOUKAI_OPT_YUKARI_S29` | `ability/youkai_option/YOUKAI_OPT_YUKARI_S29.png` | 64,64,32,64 | 383 | 453 |
| `YOUKAI_OPT_YUKARI_S30` | `ability/youkai_option/YOUKAI_OPT_YUKARI_S30.png` | 96,64,64,64 | 384 | 454 |
| `YOUKAI_OPT_ALICE_S22` | `ability/youkai_option/YOUKAI_OPT_ALICE_S22.png` | 0,128,32,32 | 385 | 455 |
| `YOUKAI_OPT_ALICE_S23` | `ability/youkai_option/YOUKAI_OPT_ALICE_S23.png` | 32,128,32,32 | 386 | 456 |
| `YOUKAI_OPT_ALICE_S24` | `ability/youkai_option/YOUKAI_OPT_ALICE_S24.png` | 64,128,32,32 | 387 | 457 |
| `YOUKAI_OPT_ALICE_S25` | `ability/youkai_option/YOUKAI_OPT_ALICE_S25.png` | 96,128,32,32 | 388 | 458 |
| `YOUKAI_OPT_REMILIA_S20` | `ability/youkai_option/YOUKAI_OPT_REMILIA_S20.png` | 128,128,32,32 | 389 | 459 |
| `YOUKAI_OPT_AYA_S32` | `ability/youkai_option/YOUKAI_OPT_AYA_S32.png` | 0,160,16,16 | 390 | 460 |
| `YOUKAI_OPT_AYA_S33` | `ability/youkai_option/YOUKAI_OPT_AYA_S33.png` | 16,160,16,16 | 391 | 461 |
| `YOUKAI_OPT_AYA_S34` | `ability/youkai_option/YOUKAI_OPT_AYA_S34.png` | 32,160,16,16 | 392 | 462 |
| `YOUKAI_OPT_AYA_S35` | `ability/youkai_option/YOUKAI_OPT_AYA_S35.png` | 48,160,16,16 | 393 | 463 |
| `YOUKAI_OPT_AYA_S36` | `ability/youkai_option/YOUKAI_OPT_AYA_S36.png` | 64,160,16,16 | 394 | 464 |
| `YOUKAI_OPT_AYA_S37` | `ability/youkai_option/YOUKAI_OPT_AYA_S37.png` | 80,160,16,16 | 395 | 465 |
| `YOUKAI_OPT_AYA_S38` | `ability/youkai_option/YOUKAI_OPT_AYA_S38.png` | 96,160,16,16 | 396 | 466 |
| `YOUKAI_OPT_AYA_S39` | `ability/youkai_option/YOUKAI_OPT_AYA_S39.png` | 112,160,16,16 | 397 | 467 |

#### `ATLAS_MOF_FAITH` — 画布 128×16，entry41，sprite398…409

| 成员 | 原图 | 矩形 x,y,w,h | 新 sprite | 旧 sprite |
| --- | --- | --- | --- | --- |
| `MOF_FAITH_ICON` | `ability/mof/FAITH_ICON.png` | 0,0,16,16 | 398 | 408 |
| `FAITH_DOT` | `ability/mof/FAITH_DOT.png` | 16,0,16,16 | 399 | 339 |
| `MOF_FAITH_DIGIT_0` | `ability/mof/FAITH_DIGIT_0.png` | 32,0,8,16 | 400 | 409 |
| `MOF_FAITH_DIGIT_1` | `ability/mof/FAITH_DIGIT_1.png` | 40,0,8,16 | 401 | 410 |
| `MOF_FAITH_DIGIT_2` | `ability/mof/FAITH_DIGIT_2.png` | 48,0,8,16 | 402 | 411 |
| `MOF_FAITH_DIGIT_3` | `ability/mof/FAITH_DIGIT_3.png` | 56,0,8,16 | 403 | 412 |
| `MOF_FAITH_DIGIT_4` | `ability/mof/FAITH_DIGIT_4.png` | 64,0,8,16 | 404 | 413 |
| `MOF_FAITH_DIGIT_5` | `ability/mof/FAITH_DIGIT_5.png` | 72,0,8,16 | 405 | 414 |
| `MOF_FAITH_DIGIT_6` | `ability/mof/FAITH_DIGIT_6.png` | 80,0,8,16 | 406 | 415 |
| `MOF_FAITH_DIGIT_7` | `ability/mof/FAITH_DIGIT_7.png` | 88,0,8,16 | 407 | 416 |
| `MOF_FAITH_DIGIT_8` | `ability/mof/FAITH_DIGIT_8.png` | 96,0,8,16 | 408 | 417 |
| `MOF_FAITH_DIGIT_9` | `ability/mof/FAITH_DIGIT_9.png` | 104,0,8,16 | 409 | 418 |

#### `ATLAS_SIGNAL` — 画布 128×32，entry42，sprite410…412

| 成员 | 原图 | 矩形 x,y,w,h | 新 sprite | 旧 sprite |
| --- | --- | --- | --- | --- |
| `SIGNAL_GAUGE_BG` | `ability/signal/SIGNAL_GAUGE_BG.png` | 0,0,64,16 | 410 | 419 |
| `SIGNAL_GAUGE_BAR` | `ability/signal/SIGNAL_GAUGE_BAR.png` | 64,0,64,16 | 411 | 420 |
| `SIGNAL_GAUGE_ICON` | `ability/signal/SIGNAL_GAUGE_ICON.png` | 0,16,16,16 | 412 | 421 |

#### `ATLAS_UFO_BODY` — 画布 256×256，entry43，sprite413…428

| 成员 | 原图 | 矩形 x,y,w,h | 新 sprite | 旧 sprite |
| --- | --- | --- | --- | --- |
| `UFO_RED_0` | `ability/ufo/UFO_RED_0.png` | 0,0,64,64 | 413 | 311 |
| `UFO_RED_1` | `ability/ufo/UFO_RED_1.png` | 64,0,64,64 | 414 | 312 |
| `UFO_RED_2` | `ability/ufo/UFO_RED_2.png` | 128,0,64,64 | 415 | 313 |
| `UFO_RED_3` | `ability/ufo/UFO_RED_3.png` | 192,0,64,64 | 416 | 314 |
| `UFO_BLUE_0` | `ability/ufo/UFO_BLUE_0.png` | 0,64,64,64 | 417 | 315 |
| `UFO_BLUE_1` | `ability/ufo/UFO_BLUE_1.png` | 64,64,64,64 | 418 | 316 |
| `UFO_BLUE_2` | `ability/ufo/UFO_BLUE_2.png` | 128,64,64,64 | 419 | 317 |
| `UFO_BLUE_3` | `ability/ufo/UFO_BLUE_3.png` | 192,64,64,64 | 420 | 318 |
| `UFO_GREEN_0` | `ability/ufo/UFO_GREEN_0.png` | 0,128,64,64 | 421 | 319 |
| `UFO_GREEN_1` | `ability/ufo/UFO_GREEN_1.png` | 64,128,64,64 | 422 | 320 |
| `UFO_GREEN_2` | `ability/ufo/UFO_GREEN_2.png` | 128,128,64,64 | 423 | 321 |
| `UFO_GREEN_3` | `ability/ufo/UFO_GREEN_3.png` | 192,128,64,64 | 424 | 322 |
| `UFO_RAINBOW_0` | `ability/ufo/UFO_RAINBOW_0.png` | 0,192,64,64 | 425 | 323 |
| `UFO_RAINBOW_1` | `ability/ufo/UFO_RAINBOW_1.png` | 64,192,64,64 | 426 | 324 |
| `UFO_RAINBOW_2` | `ability/ufo/UFO_RAINBOW_2.png` | 128,192,64,64 | 427 | 325 |
| `UFO_RAINBOW_3` | `ability/ufo/UFO_RAINBOW_3.png` | 192,192,64,64 | 428 | 326 |

#### `ATLAS_UFO_TOKEN` — 画布 128×224，entry44，sprite429…441

| 成员 | 原图 | 矩形 x,y,w,h | 新 sprite | 旧 sprite |
| --- | --- | --- | --- | --- |
| `TOKEN_RED_0` | `ability/ufo/TOKEN_RED_0.png` | 0,0,32,32 | 429 | 327 |
| `TOKEN_RED_1` | `ability/ufo/TOKEN_RED_1.png` | 32,0,32,32 | 430 | 328 |
| `TOKEN_BLUE_0` | `ability/ufo/TOKEN_BLUE_0.png` | 64,0,32,32 | 431 | 329 |
| `TOKEN_BLUE_1` | `ability/ufo/TOKEN_BLUE_1.png` | 96,0,32,32 | 432 | 330 |
| `TOKEN_GREEN_0` | `ability/ufo/TOKEN_GREEN_0.png` | 0,32,32,32 | 433 | 331 |
| `TOKEN_GREEN_1` | `ability/ufo/TOKEN_GREEN_1.png` | 32,32,32,32 | 434 | 332 |
| `TOKEN_RBRED_0` | `ability/ufo/TOKEN_RBRED_0.png` | 64,32,32,32 | 435 | 333 |
| `TOKEN_RBRED_1` | `ability/ufo/TOKEN_RBRED_1.png` | 96,32,32,32 | 436 | 334 |
| `TOKEN_RBBLUE_0` | `ability/ufo/TOKEN_RBBLUE_0.png` | 0,64,32,32 | 437 | 335 |
| `TOKEN_RBBLUE_1` | `ability/ufo/TOKEN_RBBLUE_1.png` | 32,64,32,32 | 438 | 336 |
| `TOKEN_RBGREEN_0` | `ability/ufo/TOKEN_RBGREEN_0.png` | 64,64,32,32 | 439 | 337 |
| `TOKEN_RBGREEN_1` | `ability/ufo/TOKEN_RBGREEN_1.png` | 96,64,32,32 | 440 | 338 |
| `UFO_ARC_STRIP` | `ability/ufo_gauge/UFO_ARC_STRIP.png` | 0,96,16,128 | 441 | 468 |

#### `ATLAS_TRANCE_SPIRIT` — 画布 128×96，entry45，sprite442…452

| 成员 | 原图 | 矩形 x,y,w,h | 新 sprite | 旧 sprite |
| --- | --- | --- | --- | --- |
| `SPIRIT_PLAIN_0` | `ability/trance/SPIRIT_PLAIN_CELL_C0R0.png` | 0,0,32,32 | 442 | 109 |
| `SPIRIT_PLAIN_1` | `ability/trance/SPIRIT_PLAIN_CELL_C0R1.png` | 32,0,32,32 | 443 | 110 |
| `SPIRIT_TRANCE_0` | `ability/trance/SPIRIT_TRANCE_CELL_C0R0.png` | 64,0,32,32 | 444 | 111 |
| `SPIRIT_TRANCE_1` | `ability/trance/SPIRIT_TRANCE_CELL_C0R1.png` | 96,0,32,32 | 445 | 112 |
| `SPIRIT_TRANCE_2` | `ability/trance/SPIRIT_TRANCE_CELL_C1R0.png` | 0,32,32,32 | 446 | 113 |
| `SPIRIT_TRANCE_3` | `ability/trance/SPIRIT_TRANCE_CELL_C1R1.png` | 32,32,32,32 | 447 | 114 |
| `SPIRIT_TRANCE_4` | `ability/trance/SPIRIT_TRANCE_CELL_C2R0.png` | 64,32,32,32 | 448 | 119 |
| `SPIRIT_TRANCE_5` | `ability/trance/SPIRIT_TRANCE_CELL_C2R1.png` | 96,32,32,32 | 449 | 120 |
| `SPIRIT_TRANCE_6` | `ability/trance/SPIRIT_TRANCE_CELL_C3R0.png` | 0,64,32,32 | 450 | 121 |
| `SPIRIT_TRANCE_7` | `ability/trance/SPIRIT_TRANCE_CELL_C3R1.png` | 32,64,32,32 | 451 | 122 |
| `TPOINT` | `ability/tpoint/TPOINT.png` | 64,64,16,16 | 452 | 342 |

#### `ATLAS_TRANCE_GAUGE` — 画布 128×128，entry46，sprite453…461

| 成员 | 原图 | 矩形 x,y,w,h | 新 sprite | 旧 sprite |
| --- | --- | --- | --- | --- |
| `TRANCE_GAUGE_TOP` | `ability/trance/GAUGE_AC.png` | 0,0,64,64 | 453 | 115 |
| `TRANCE_GAUGE_BAR` | `ability/trance/GAUGE_BAR.png` | 64,0,48,32 | 454 | 116 |
| `TRANCE_GAUGE_BAR2` | `ability/trance/GAUGE_BAR2.png` | 0,64,48,32 | 455 | 117 |
| `TRANCE_GAUGE_MARK` | `ability/trance/GAUGE_MARK.png` | 48,64,16,32 | 456 | 118 |
| `TRANCE_GAUGE_FRAME_A` | `ability/trance/GAUGE_FRAME_A.png` | 64,64,32,48 | 457 | 127 |
| `TRANCE_GAUGE_FRAME_B` | `ability/trance/GAUGE_FRAME_B.png` | 96,64,32,48 | 458 | 128 |
| `TRANCE_TIERUP_1` | `ability/trance/TIERUP_1.png` | 0,112,32,16 | 459 | 124 |
| `TRANCE_TIERUP_2` | `ability/trance/TIERUP_2.png` | 32,112,32,16 | 460 | 125 |
| `TRANCE_TIERUP_MAX` | `ability/trance/TIERUP_MAX.png` | 64,112,32,16 | 461 | 126 |

#### `ATLAS_TRANCE_POPUP` — 画布 128×16，entry47，sprite462…467

| 成员 | 原图 | 矩形 x,y,w,h | 新 sprite | 旧 sprite |
| --- | --- | --- | --- | --- |
| `TRANCE_POPUP_LIFE1` | `ability/trance/POPUP_LIFE1.png` | 0,0,24,8 | 462 | 129 |
| `TRANCE_POPUP_LIFE2` | `ability/trance/POPUP_LIFE2.png` | 24,0,24,8 | 463 | 130 |
| `TRANCE_POPUP_SPELL1` | `ability/trance/POPUP_SPELL1.png` | 48,0,24,8 | 464 | 131 |
| `TRANCE_POPUP_SPELL2` | `ability/trance/POPUP_SPELL2.png` | 72,0,24,8 | 465 | 132 |
| `TRANCE_POPUP_POINT10` | `ability/trance/POPUP_POINT10.png` | 96,0,32,8 | 466 | 133 |
| `TRANCE_POPUP_POINT100` | `ability/trance/POPUP_POINT100.png` | 0,8,32,8 | 467 | 134 |

#### `ATLAS_TRANCE_ASTRAL` — 画布 512×512，entry48，sprite468…468

| 成员 | 原图 | 矩形 x,y,w,h | 新 sprite | 旧 sprite |
| --- | --- | --- | --- | --- |
| `TRANCE_ASTRAL` | `ability/trance/ASTRAL.png` | 0,0,512,512 | 468 | 123 |

### 4.4 合并前 / 合并后 entry 数对照

| 归属 | 合并前 entry | 合并后 entry | 合并前 ≥256 | 合并后 ≥256 |
| --- | --- | --- | --- | --- |
| 107 senro | 4 | 1 | 2 | 0 |
| 108 youkai | 109 | 4 | 94 | 0 |
| 110 mof | 12 | 1 | 11 | 0 |
| 111 signal | 3 | 1 | 3 | 0 |
| 112 ufo | 29 | 2 | 1 | 0 |
| 113 trance | 27 | 4 | 0 | 0 |
| 115 lolk | 8 | 1 | 0 | 0 |
| 116 season | 43 | 2 | 0 | 0 |
| 117 animal | 82 | 5 | 0 | 0 |
| 共享 HUD（`sdk.c`） | 14 | 2 | 0 | 0 |
| 别人的卡（58–80） | 29 | 19 | 0 | 0 |
| **追加合计** | **360** | **42** | **111** | **0** |
| **含原生 7 entry 的总数** | **367** | **49** | — | — |

### 4.5 新 `ORDER.txt` 的排布，以及「只追加」怎么恢复

**排布（一次性的最后一次重排，之后永不再动）**：

| 段 | 内容 | entry | sprite |
| --- | --- | --- | --- |
| A 共享 HUD（`sdk.c`） | 2 个 entry | entry7…entry8 | sprite109…132 |
| B 别人的卡（19 行**原样搬**，一个字不改） | 19 个 entry | entry9…entry27 | sprite133…151 |
| C 系统卡**已定稿** 114–117 | 8 个 entry | entry28…entry35 | sprite152…284 |
| D 系统卡**待定** 106–113（❓ 还会改的都在这段） | 13 个 entry | entry36…entry48 | sprite285…468 |

**「只追加」的恢复动作**：

1. 这次重排是**一次性**的：113 的灵界块从文件头回到它该在的地方（卡 id 序），`ORDER.txt` 头部那 40 行
   「搬序说明」注释整段删掉，换成一段「本文件此后只追加」的声明 + 本报告链接。
2. 之后**任何**新增（我们的新卡 / 别人的新卡 / 别人的新美术）一律 `>> ORDER.txt` 追加到**文件末尾**；
   现有行**一行都不许挪、不许插**。
3. 美术要换 → **不动老行**，在末尾追加新的图集/entry，老行留在原地（只损失 entry 号，不损失正确性）；
   这也是「待定段 D 放最后」的原因：D 里任何一张卡要换图，追加点紧挨着它们。
4. `build_ability.py` 里把这条约定写成**硬门**：记下 `ORDER.txt` 的行数与每行名字的 sha256，
   若检测到「既有行被移动/删除」就要求 `--allow-reorder`（避免下一个人无意重排又踩 255）。

### 4.6 builder 要扩什么（`assets/build_ability.py` + `assets/anmlib.py`）

现状（`assets/build_ability.py:34-73`）：`plan()` 一行一个 entry、`template_for()` 一张图一个 sprite、
`make_entry()` 只吐一行 `spriteN`、`write_ids_h()` 一行一个 sprite 宏。

**提案：`ORDER.txt` 加一种「图集段」语法（成员名保持顶层、不改成 `NAME_k`）**

```
# 图集段：`ATLAS <画布名> <WxH>` 开一段，段内一行一个成员 `成员名  源图  WxH  @x,y`
#   ⇒ 整段编译成 **一个 entry**（name = ability/atlas/<画布名>.png），
#     成员按段内顺序拿**连续** sprite 号；
#   ⇒ anm_ids.h 仍然为**每个成员名**生成 CE_ANM_ABILITY_SPRITE_<成员名>（值 = 段内成员号），
#     额外生成 CE_ANM_ABILITY_ATLAS_<画布名> = 该 entry 号（调试/日志用）。
ATLAS ATLAS_SEASON_UI 256x144
SEASON_OPT      ability/season/SEASON_OPT.png   64x64  @0,0
PT_SPRING       ability/season/PT_SPRING.png    16x16  @64,0
…
# 单图段（现在这种写法）照旧：一行 = 一个 entry = 一个 sprite
PEA             ability/peashooter/PEA.png      32x32
```

各处的具体改法：

| 位置 | 现在 | 要改成 |
| --- | --- | --- |
| `anmlib.read_order()` | 一行 2–3 列 | 认 `ATLAS <名> <WxH>` 段头与成员行（`@x,y` 第四列）|
| `anmlib.make_entry()` | 只写一行 `spriteN` | 加 `sprites=[(sid,x,y,w,h), …]` 参数（`build_player_anm.py:180` 的 `_entry_block()` 已是现成模板）|
| `build_ability.py.plan()` | `entries=[(name, src, eidx, sidx, sz)]` | 加一层：`entries` 里带 `members=[(name, src, x, y, w, h)]`，sprite 号按成员顺序发 |
| `template_for()` | 只有一个 `sprite` 字段 | 图集时按画布建字段（`width/height/THTXSize/THTXWidth/THTXHeight` = 画布），sprite 表由成员给 |
| 贴图合成 | `Image.open(src)` → 居中补边 → 存单图 | 图集：开一张 `W×H` 的透明画布，把每个成员 `paste` 到它的矩形（**尺寸必须相等**，不等直接 `raise`）|
| `write_ids_h()` | 一行一个名字 | 图集段内逐成员出一行（宏名不变）|
| `verify_rebuilt` 循环 | `e["sprite"] != t["sprite"]` | 改成逐成员核对矩形（`parse_entries()` 已在多 sprite 时返回 `sprite=None`、`n_sprites=N`，可直接用）|
| `assets/check_ability_anm_bounds.py` | 检查②③：宏名后缀 == entry 名 | 改成：sprite 必须落在**含该成员的图集 entry** 里（`_read_order()` 要重写成会读段）|

> ⚠ `assets/check_ability_anm_bounds.py:112-118` 现在会把「图集」判成错误（它要求
> `CE_ANM_ABILITY_SPRITE_ICON_WOLF` 落在名为 `ability/ICON_WOLF.png` 的 entry 里）。**这是合并 PR 必须先改的校验器**，
> 否则 `make check` 一定红。

## 5. ★ 合并后各系统卡的 C 侧代码怎么改

**一句话结论：系统卡 106–117 的 C 代码本次合并**一条都不用改**（唯一「建议改」的是 `senro.c` 两条裸数字）。
理由是方案把三个不变量都保住了：① 成员名不变 ⇒ 宏名不变；② 算术族在图集内连续升序；③ 脚本号一个都不动。
下面逐条给证据。

### 5.1 依赖「sprite 号连续 / 可用算术推导」的代码 —— 全库只有 5 处

`grep -rnE "CE_ANM_ABILITY_(SPRITE|SCRIPT)_[A-Z0-9_]+ *[+\-]" native/` 的全部命中（5 处 + 1 处注释）：

| 位置 | 代码 | 依赖什么 | 合并方案 | 结论 |
| --- | --- | --- | --- | --- |
| `native/sdk.c:581` | `CE_ANM_ABILITY_SPRITE_TS_DIGIT_0 + v`（`v ∈ 0..9`，`CE_HUD_DIGIT_MAX` = `sdk.h:515`）| `TS_DIGIT_0..9` **连续升序** | 全在 `ATLAS_HUD_DIGIT` 内、按 0..9 排 | ✅ 不改 |
| `native/sdk.c:617` | `CE_ANM_ABILITY_SPRITE_TS_DIGIT_0 + d[i]`（`ce_hud_number_set`）| 同上 | 同上 | ✅ 不改 |
| `native/sdk.c:629` | `CE_ANM_ABILITY_SPRITE_FONT2_0 + (uint32_t)(ch - '0')` | `FONT2_0..9` **连续升序** | 全在 `ATLAS_FONT2` 内、按 0..9 排（`MINUS/PERCENT/DOT/PLUS` 排在其后）| ✅ 不改 |
| `native/cards/animal.c:2009` | `CE_ANM_ABILITY_SPRITE_AURA_WOLF_0 + k`（`k ∈ 0..7`）| `AURA_WOLF_0..7` 连续升序 | `ATLAS_ANIMAL_64` 内按 WOLF→OTTER→EAGLE 各 0..7 排 | ✅ 不改 |
| `native/cards/animal.c:2010` / `:2011` | `AURA_OTTER_0 + k` / `AURA_EAGLE_0 + k` | 同上 | 同上 | ✅ 不改 |
| `native/cards/youkai_core.h:197`（**只是注释**）| 写着「`CE_ANM_ABILITY_SCRIPT_YOUKAI_FAMILIAR_S53 + 变体`」| — | 代码实际在 `youkai.c:317-325` **逐条 `switch`** | ✅ 不改（建议把注释改对）|

其余「看起来像靠号」的地方，实际都是**逐名查表**（合并天然免疫），逐个点名：

| 位置 | 形态 | 合并后 |
| --- | --- | --- |
| `native/cards/mof.c:157-163` `s_hud_digit_tbl[10]` | 十个 `MOF_FAITH_DIGIT_n` **逐名列出** | ✅ 不改（注释里明写「不假设号连续」）|
| `native/cards/mof.c:736` | `s_hud_digit_tbl[…]` / `…DIGIT_0` 兜底 | ✅ 不改 |
| `native/cards/season.c:339-348` `season_opt_sprite()` | `switch` 逐名（`OPT_*`）| ✅ 不改 |
| `native/cards/season.c:351-362` `season_sprite()` | `switch` 逐名（`PT_*`）| ✅ 不改 |
| `native/cards/season.c:365-374` `season_glyph_sprite()` | `switch` 逐名（`GLYPH_*`）| ✅ 不改 |
| `native/cards/faith.c:182-189` `faith_sprite_for()` | `switch` 逐名（借 116 的 `PT_*`）| ✅ 不改 |
| `native/cards/animal.c:608-622` `sprite_for_type()` | `switch` 逐名（`ICON_*`）| ✅ 不改 |
| `native/cards/trance.c:2104-2106` | 三元表达式逐名切 `TRANCE_GAUGE_FRAME_A/B` | ✅ 不改 |
| `native/cards/ufo.c:213-235`、`youkai.c:317-325`、`animal.c:592-600` | 全是**脚本**宏、逐条列 | ✅ 不改（脚本号不动）|
| `native/cards/season.c:1002` `CE_ANM_VM_SPRITE_ID` | 读**自机**（`pl0X.anm`）当前 sprite 号再写回同文件的残影 VM | ✅ 不涉及 `ability.anm` 的号 |
| `native/cards/youkai.c:797` `CE_FN_ANM_SET_SPRITE` | 写 `player+0xc`（`pl0X.anm` 那一页）| ⚠ 与本合并无关，但见 §7-① |

### 5.2 写死的 sprite 号 / 脚本号

| # | 位置 | 内容 | 本次合并要不要改 | 说明 |
| --- | --- | --- | --- | --- |
| 1 | `native/cards/senro.c:153` | `#define SENRO_ANM_SCRIPT_FIELD 204`（**没有 `#ifdef` 兜底**）| **不必改**（脚本号不变），**建议改**成 `CE_ANM_ABILITY_SCRIPT_SENRO_FIELD` | 这是全库唯一一处「活的裸脚本号」；注释还写着「anm_ids.h 由主 agent 统一压号后合并」——重排之后这句话会误导下一个人 |
| 2 | `native/cards/senro.c:154` | `#define SENRO_ANM_SCRIPT_RING 205` | 同上 | 同上 |
| 3 | `native/cards/ufo.c:1129` / `:1134` | `#define UFO_GAUGE_SCRIPT_ARC 228` / `BAR 231`（在 `#else` 分支里）| 不用改 | 上面已有 `#ifdef CE_ANM_ABILITY_SCRIPT_UFO_GAUGE_*` ⇒ 当前**走的是宏**；`#else` 是死代码 |
| 4 | `native/cards/youkai.c:109/114/119/124` | `YK_OPT_SCRIPT_{YUKARI,ALICE,REMILIA,AYA} = 224..227`（`#else` 分支）| 不用改 | 同上（宏已存在）|
| 5 | 硬编码 **sprite** 号 | `grep -rnE "ce_anm_set_sprite\([^)]*, *[0-9]+ *\)" native/` = **0 处** | — | 与 §CC-2 的普查一致 |
| 6 | `assets/ability/scripts/` 里的裸 sprite 号 | `grep -rnE "sprite\( *[0-9]+" assets/ability/scripts/` = **0 处**（只有 `210_mof_faith_icon.anm.txt` 的注释里有一句）| — | 脚本一律走 `@NAME` |
| 7 | `assets/ability/scripts/` 里的 `scriptNew(scriptN)` **字面量** | 30 处、8 个文件（`70`,`79`,`132`,`150`,`160`,`179`,`182`,`232`）| 不用改 | 本次**不动脚本号**。⚠ 以后若跑 `renumber_scripts.py`，这 30 处会断（工具会点名报出来）|

### 5.3 逐卡结论（106–117）

| id | 卡 | 文件 | 用到的 ability 贴图 | 本次要改的 C 代码 | 理由 |
| --- | --- | --- | --- | --- | --- |
| 106 | 红魔乡 | `native/cards/th06rank.c` | 0 | **无** | 全文件 0 处 `CE_ANM_ABILITY_`（只在 `:409` 用 `ce_anm_set_color` 改一个原生 VM 的颜色） |
| 107 | 妖妖梦 | `native/cards/senro.c` | 4 | **无（建议改 2 行）** | 贴图全由脚本 204/205 画，卡侧不碰 sprite 号；脚本号本次不变 ⇒ 功能不受影响。建议把 `:153-154` 的裸数字换成宏（§5.2 #1） |
| 108 | 永夜抄 | `native/cards/youkai.c` | 109 | **无** | 脚本宏（206–209、224–227）+ 无 sprite 宏；`yk_fam_anm_script()` 逐条 `switch`。⚠ 妖侧本体 55 张与 `YK_BODY_SPRITE_FOCUSED = -1` 见 §7-① |
| 109 | 花映塚 | `native/cards/pofv.c` | 0 | **无** | 0 处 `CE_ANM_ABILITY_` |
| 110 | 风神录 | `native/cards/mof.c` | 12 | **无** | `s_hud_digit_tbl[10]` 逐名 + 脚本宏（210/211）；`FAITH_DOT` 是死图 |
| 111 | 地灵殿 | `native/cards/signal.c` | 3 | **无** | 三个 sprite 全在图集里、脚本宏 212–214；`signal.c` 自己一行 sprite 号都没写 |
| 112 | 星莲船 | `native/cards/ufo.c` | 29 | **无** | 全走脚本宏；`TOKEN_*`/`UFO_*` 的贴图由脚本 164–173 画。顺手可更正 `:297` 注释里过期的「脚本 219/220/221」 |
| 113 | 神灵庙 | `native/cards/trance.c` | 27 | **无** | `TRANCE_GAUGE_FRAME_A/B` 走宏；`:820` / `:2112-2131` 的 `TRANCE_HUD_ART_SCALE` 作用在**矩形=原尺寸**的图上 ⇒ 行为不变 |
| 114 | 辉针城 | `native/cards/collect2x.c` | 0 | **无** | 0 处 `CE_ANM_ABILITY_`；卡图是 `abcard.anm` 的占位（116/117），与本事无关 |
| 115 | 绀珠传 | `native/cards/lolk.c` | 8 | **无** | `LOLK_RING` / `LOLK_PANEL_1..7` 全走宏；面板 7 态由 7 条脚本各画一张 |
| 116 | 天空璋 | `native/cards/season.c` | 43 | **无** | 三个 `switch` 逐名；`:1002` 的残影抄的是 `pl0X.anm` 的号，不受影响 |
| 117 | 鬼形兽 | `native/cards/animal.c` | 82 | **无** | `AURA_*_0 + k` 靠连续 ⇒ 方案里 WOLF/OTTER/EAGLE 各 8 张在 `ATLAS_ANIMAL_64` 内**连续升序**；其余逐名 |

### 5.4 其它链条会不会被牵连（逐条查过）

| 链条 | 结论 | 证据 |
| --- | --- | --- |
| `abcard.anm` / `assets/build_abcard.py` / `patch/th18/cards.js` | **不受影响** | `build_abcard.py:21` 用自己的 `assets/cards/ORDER.txt`、`base = len(retail_entries)`（原生 abcard = 118）且 sprite 号 = entry 号，与 `ability.anm` 完全两套计数 |
| `pl0X.anm` / `assets/build_player_anm.py` / `native/pl_anm_ids.h` / `assets/check_player_anm_bounds.py` | **不受影响** | 独立的文件、独立的 entry/sprite 计数；`pl02 entry12 = 128×16 ⇒ 2 sprite` 正是「多 sprite entry」的现成先例（`build_player_anm.py:73-96`）|
| `pl0X.sht` / `assets/sht/append_shooterset.py` / `native/sht_ids.h` | **不受影响，且产物逐字节不变** | shooterset 的 `anm`(`+0x22`) / `hit_fx`(`+0x26`) 存的是**脚本号**（`append_shooterset.py:72-113` 用 `CE_ANM_ABILITY_SCRIPT_*`），本次不动脚本号 ⇒ `make sht-verify` 的「前 23 组逐字节不变」仍成立 |
| `native/engine.h` / `sites.py` / `patch/**` | **不受影响** | 与 anm 编号无关 |
| `native/anm_ids.h` | 自动重生成 | `make anm` 的产物 |
| `native/build/ability.anm` | 自动重生成 | 同上 |

### 5.5 合并 PR 的改动清单（可直接拿来判断「要不要通知别人」）

| # | 文件 | 谁负责 | 风险 | 回归怎么跑 |
| --- | --- | --- | --- | --- |
| 1 | `assets/ability/entries/ORDER.txt` | **我们**（共享件）| ★★★ 所有卡的 sprite 号平移 | `make anm` → `make check` → 全卡实跑 |
| 2 | `assets/build_ability.py` + `assets/anmlib.py` | **我们**（共享生成器）| ★★ 生成器改错 = 全库错 | `make anm-verify`（含原生贴图逐张比对）|
| 3 | `assets/check_ability_anm_bounds.py` | **我们**（共享校验器）| ★ 不改就一定红 | `make check` |
| 4 | `native/cards/senro.c:153-154` | **我们**（107）| ☆ 只换宏名 | `make check`（107 的回归）|
| 5 | `native/cards/ufo.c:297`、`youkai_core.h:197`、`animal.c:2003-2004`、`make_trance_art.py:279` 的**注释** | **我们** | ☆ 纯文字 | — |
| 6 | `native/anm_ids.h`、`native/build/*.anm` | 生成物 | — | `make anm` |
| 7 | **别人的卡（58–80）的源文件** | **不用改**（全走宏，§5.1/§5.2 查过）| 但**必须重编 DLL** | 需在 `AUDIT.md` §BE 登记一行：「ORDER 重排 ⇒ 所有卡的 sprite 号平移，行为不变；旧 DLL + 新 ability.anm 会全错位」|
| 8 | `native/th18_card_expand.dll` + `native/build/ability.anm` | 发布件 | ★★★ 必须**同时**发布 | `local/DLL-SYNC.md` 的四处 md5 对账 |

> **要不要通知别人？** 结论：**不需要逐个问，但 §BE 必须登记**（`CLAUDE.md` 的口径：作者自己查这份清单）。
> 他们的卡**源文件一行都不用改**，只是号变了；真正的硬约束是「DLL 与 `ability.anm` 必须同一次发布」。

## 6. 两个坑，逐条落进方案

### 6.1 坑①：VM 的 `scale.x` 是按 sprite 的**矩形宽度**缩放的

机制（一手）：`th18:0x481703` 算的是 `[vm+0x74] × [vm+0x54]`（矩形宽 × `scale.x`），
而 `vm+0x74` 是 `AnmLoaded__set_sprite th18:0x477b00` 从 sprite 结构写进 VM 的。
⇒ 只要**每个 sprite 的矩形还是原图那么宽**，脚本里的 `scale()` / `scaleTime()` 与 C 侧的 `ce_anm_set_scale`
**行为逐像素不变**。本方案 4.1 第 1 条就是照这条定的。

**系统性检查**（范围 = 106–117 可达的全部 ability 脚本，判据见 §8）：

| 检查项 | 结果 |
| --- | --- |
| 106–117 可达的 ability 脚本 | **119** 条 |
| 其中带 `scale(` / `scaleTime(` 的 | **75** 条 |
| 这 75 条里画到「画布 > 内容」（= 补过边）成员的 | **只有 2 条**：`script257` / `script258`（符卡碎弹窗 `TRANCE_POPUP_SPELL1/2`）|
| 全库补过边的 entry | 17 个：`TRANCE_POPUP_SPELL1/2`（我们的 113）、`FONT2_*` ×14（共享）、`BLUE_EYES_FLARE`（别人的卡）|

那 2 条**为什么现在也没出事**：补边是**居中**的（`assets/build_ability.py:120`：
`pad.paste(im, ((want[0]-im.width)//2, (want[1]-im.height)//2))`），而这两条脚本写的是 `anchor(0, 2)`，
一手语义 = **水平居中 / 垂直底**（`assets/ability/scripts/210_mof_faith_icon.anm.txt:39` 原文：
「0 中 / 1 左 / 2 右；0 中 / 1 顶 / 2 底」）⇒ 居中补边**不产生位移**。

★ 顺手订正一处**误读**（不是 bug，但会误导下一个人）：`assets/ability/make_trance_art.py:277-280` 写着
「脚本是 `anchor(0, 2)`（左-下），居中补边会让图形整体右移 4 px、弹窗不再从神灵的 x 起画」——
`0` 是**中**不是左，所以这句话的前件不成立。（`anchor(0,2)` 的「居中」锚 + 居中补边 = 零位移。）
⇒ 方案里把 `TRANCE_POPUP_SPELL1/2` 的**声明尺寸从 32×8 改成 24×8**（= 源图真实尺寸，与 `TRANCE_POPUP_LIFE1/2` 一致）：
视觉零差异、少 8 px 画布，并顺手把上面那句注释改对。

另两处「画布 > 内容」都不在系统卡范围内，方案不动它们：

| 位置 | 现状 | 方案 |
| --- | --- | --- |
| `FONT2_*`（共享 HUD 字形，`sdk.c` 用）| 源图 16×20，声明画布 16×32（居中补 6 行上下）| **保留 16×32 矩形**（§4.1 第 6 条）；`sdk.c:690` 的 `+5.0f * sy * dat` 正是为它标定的 |
| `BLUE_EYES_FLARE`（别人的卡，青眼白龙）| 原生 `pl01b2.png` 96×256，声明 128×256（居中补 32 px）| 不动（属于 58–80 那批）；若他们愿意，按原生尺寸改成 96×256 即可，但**不在本次合并范围** |

**合并后这类坑在结构上消失**：图集成员的矩形 = 原图尺寸，`build_ability.py` 的「补边」分支
（`if im.size != want: pad`）从此只服务于单图 entry。

### 6.2 坑②：迁移与回归步骤

```bash
cd mods/th18.v1.00a/card-expand

# ① 重建 anm + 重新生成 anm_ids.h（**唯一会改号的步骤**）
cd native && make anm                  # build_abcard.py + build_ability.py + build_player_anm.py
# ② .sht 的 anm/hit_fx 字段引用 anm_ids.h 的**脚本**号 ⇒ 必须在 make anm 之后重建
make sht                               # 会交叉核对 pl_anm_ids.h
# ③ 完整只读门（含 anm-verify → check_ability_anm_bounds.py、sht-verify、sites check、check-docs）
make check
# ④ DLL
make dll
```

| 步 | 命令 | 看什么 | 失败了说明什么 |
| --- | --- | --- | --- |
| 1 | `make anm` | 末尾打印的 `原生 entry 7 + N`；N 应为 **42**（本方案）| 生成的 entry/sprite 表与图集不符 |
| 2 | `make anm-verify` | `build_ability.py --verify-only` 的逐项自检 + `check_ability_anm_bounds.py` | §4.6 里那两个校验器的改动漏了 |
| 3 | `make sht-verify` | 「前 23 组逐字节不变」 | 说明脚本号被动过（本次**不该**发生）|
| 4 | `make check` | `check-docs.py` + `sites.py check` | 文档/站点受牵连 |
| 5 | `make release` + `local/sync_modkit_dll.sh` | 四处 DLL 同 md5（`local/DLL-SYNC.md`）| 版本不一致 ⇒ 会误判「改了没生效」 |

**实跑回归清单**（按风险从高到低）：

| # | 回归对象 | 看什么 | 依据 |
| --- | --- | --- | --- |
| 1 | **113 判定表** | `AUDIT.md` §CA / §CB / §CC / §CD 的四张表逐条；尤其 §CC-2 的四个对照号（`TRANCE_GAUGE_FRAME_A` 等）**会再变一次**，要把表里的号更新成 §4.3 的新号 | §CC-2、§CD-4 |
| 2 | **117 的图** | 五格灵槽 / 灵魂 / 火焰 / 兽灵光环 / 霊撃环 / 光束（`AUDIT.md` §AL 的图与断言）| §AL |
| 3 | **110 / 111 的 HUD** | 信仰点绿点、信号槽（这两张卡现在**就是坏的**：11 + 3 个坏 entry）| 本报告 §1.1 |
| 4 | **107 的结界** | `SENRO_FIELD`（256×256 全屏闪光）/ `SENRO_RING`（32 颗樱点）——现在也是坏的（entry304/305）| 本报告 §1.1 |
| 5 | **112 的令牌与量表** | `UFO_ARC_STRIP`（entry366，现在坏）；令牌 6 态与四色本体 | 本报告 §1.1 |
| 6 | **共享 HUD 字形**（`TS_DIGIT_*` / `FONT2_*`）| 卡 75 真银圣剑、78 衔尾蛇、112 星莲船（`ce_hud_digit`）、80 股市（`ce_hud_number`）；卡 107/110/115/116（`ce_hud_vm_text` 的 `FONT2_*` 字形）| §5.1 |

**DLL 与 `ability.anm` 必须同时发布**（这条不是建议，是硬约束）：

- DLL 里把 `anm_ids.h` 的宏值**烘进了机器码**（`sdk.c:581/617` 的 `TS_DIGIT_0 + v`、
  `animal.c:2009-2011` 的 `AURA_*_0 + k`、以及所有 `ce_anm_spawn(…, scriptNN, …)` 的立即数）；
- 而 `ability.anm` 是**号 → 贴图**的那一页。**旧 DLL + 新 anm（或反过来）= 全部贴图错位**，
  比现在只坏 111 个 entry 更糟（会全坏）。
- `make release` 本来就同时发两者；真正会出错的是**手工拷贝**：`local/DLL-SYNC.md` 规定 4 处 DLL 必须同 md5，
  再加一条：`th18/ability.anm` 也要跟 DLL 一起换（modkit 的 `mods/th18/` 目录）。

## 7. 风险与不确定项

| # | 事项 | 把握 | 需要谁拍板 | 建议 |
| --- | --- | --- | --- | --- |
| 1 | **108 的妖侧本体 55 张**（`YOUKAI_{YUKARI,ALICE,REMILIA,AYA}_S*`）：没有任何脚本/C 引用，而 `youkai.c:797` 那条路径把 sprite 号写给 **`player+0xc` 那一页（`pl0X.anm`）** ⇒ 即使接上，取的也是 `pl0X.anm` 的贴图表 ⇒ **画出来必错** | 🟡 逻辑上一手（`youkai.c:764-784` 的注释自己就写明「传的 anm 必须是 VM 那一页」）| 用户 / 108 作者 | 三选一：①**删**这 55 行（省 2 个 entry + 55 张图）②留着当「以后要用」③改走 `build_player_anm.py` 追加进 `pl0X.anm`（**推荐**，那才是能用的路）|
| 2 | 别的卡的**未合并分支**：若作者本地已经往 `ORDER.txt` 末尾追加过美术，本次重排会冲突 | 🟡 我只看得到本仓工作区 | 各卡作者 | 合并前在群里/§BE 说明「要重排了，先把本地追加落库」|
| 3 | 「死条目」是不是真的没人用（可能有别人的代码在别处引用）| 🟢 全库 grep 过 `native/**` + `assets/sht/**`；但**删**就有风险 | 用户 | **一张都不删**（合并后 entry 只要 49 个，删了省不到东西）|
| 4 | 「entry 上限是 255」的证据强度：§CC 的量级普查（原生最大 `abcard.anm` 118 entry）| 🟡 没有一手反汇编证明「8 位」 | — | **本方案天然不敏感**：合并后 49 entry 比原生最大的 118 还小；即使真实上限是 128 也安全 |
| 5 | 图集体积：画布空白会让 `ability.anm` 从 9.47 MB 涨到约 **9.8 MB**（+0.33 MB；估算 = 追加部分的 `W×H×4` 求和：现值 8.04 → 图集 4.53 + 别人的卡 3.84 MB）| 🟢 算术 | 用户（只在意外包体/显存时）| 若在意：把 512×512 的 `TRANCE_ASTRAL`（卡侧默认关）与几个大画布改小 |
| 6 | 图集**几何**是我按货架式打包算的，不是唯一解；美术以后改尺寸就要改矩形 | 🟢 | 美术作者 | 矩形集中写在 `ORDER.txt` 的 `@x,y` 列，改起来是一处 |
| 7 | `native/cards/senro.c:153-154` 的裸脚本号 204/205 | 🟢 已定位 | 107 作者（我们自己）| 本次顺手换成宏 |
| 8 | `assets/check_ability_anm_bounds.py:44` 的 `CARD117_SCRIPTS = list(range(137, 164))` 是**写死的脚本段** | 🟢 | 117 作者 | 117 以后加脚本（>163）这段就漏检；建议改成从 `anm_ids.h` 的 `SCRIPT_ANIMAL_*` 推 |
| 9 | 我**没有实跑**，也**没有跑 `make anm`**（审计纪律）：所有「坏/好」的结论来自 `thanm -l` 回读 + 源码图，不是画面 | 🟢 已声明 | — | 合并后必须按 §6.2 的清单实跑一次 |

## 8. 复现命令与证据索引

本文所有数字都由下面这些**只读**命令得到（没有一条写过仓库里的文件）：

```bash
MOD=mods/th18.v1.00a/card-expand

# ① 回读构建产物：entry / sprite / script 数
local/vendor/thtk/build/thanm/thanm -l 18 $MOD/native/build/ability.anm \
    -m local/vendor/eclmap/anmmap/v8.anmm | head

# ② 原生基线（7 entry / 109 sprite / 68 script）
python3 - <<'PY'
import re,pathlib
t=pathlib.Path('local/th18.v1.00a/anm/ability/ability.anm.txt').read_text()
print(len(re.findall(r'^entry entry(\d+)',t,re.M)),
      max(int(x) for x in re.findall(r'^\s+sprite(\d+):',t,re.M))+1,
      len(re.findall(r'^script script(\d+)',t,re.M)))
PY

# ③ 某个 ORDER 名被哪些脚本引用（注意 token 是 `@NAME`，收尾 @ 可选 —— `anmlib.py:34`）
grep -rn '@WOLF_0' $MOD/assets/ability/scripts/

# ④ 卡侧对 sprite / 脚本宏的引用（含 file:line）
grep -rn 'CE_ANM_ABILITY_' $MOD/native --include=*.c --include=*.h | grep -v anm_ids.h

# ⑤ 算术依赖（§5.1 的全部命中）
grep -rnE 'CE_ANM_ABILITY_(SPRITE|SCRIPT)_[A-Z0-9_]+ *[+\-]' $MOD/native --include=*.c --include=*.h

# ⑥ 写死的号（§5.2）
grep -rnE 'ce_anm_set_sprite\([^)]*, *[0-9]+ *\)' $MOD/native
grep -rnE 'sprite\( *[0-9]+' $MOD/assets/ability/scripts/
grep -rnE '#define +[A-Z0-9_]*ANM_SCRIPT[A-Z0-9_]* +[0-9]+' $MOD/native --include=*.c --include=*.h
```

| 结论 | 证据 |
| --- | --- |
| 追加部分 `entry = 7+k` / `sprite = 109+k` | `assets/build_ability.py:48`（`plan()`） |
| 367 entry / 469 sprite / 262 script | `thanm -l 18 native/build/ability.anm`（§8 ①） |
| 原生 7 entry / 109 sprite / 68 script | `local/th18.v1.00a/anm/ability/ability.anm.txt` |
| token 语法是 `@NAME`（收尾 `@` 可选） | `assets/anmlib.py:34` `_TOKEN = re.compile(r"@([A-Za-z_][A-Za-z0-9_]*)@?")` |
| 一个 entry 多 sprite 引擎支持 | `AUDIT.md` §CD-1（`th18:0x4865b0` 分配文件级 sprite 表、`th18:0x477b00` 用 `idx*0x44` 取） |
| `scale.x` 乘矩形宽 | `th18:0x481703`；`vm+0x74` 由 `th18:0x477b00` 写入（`assets/build_ability.py:52-58` 的注释同源） |
| `anchor(h,v)` = 0 中 / 1 左·顶 / 2 右·底 | `assets/ability/scripts/210_mof_faith_icon.anm.txt:39` |
| 多 sprite entry 的现成写法 | `assets/build_player_anm.py:186-210`（`_entry_block()`）+ `:73-96`（`pl02 entry12 = 128×16 ⇒ 2 sprite`） |
| 补边是居中 | `assets/build_ability.py:120` `pad.paste(im, ((want[0]-im.width)//2, (want[1]-im.height)//2))` |
| `sdk.c` 的算术依赖 | `native/sdk.c:581`、`:617`、`:629`；`native/sdk.h:515` `CE_HUD_DIGIT_MAX 9` |
| `animal.c` 的光环算术依赖 | `native/cards/animal.c:2006-2013` `sprite_for_aura_frame()` |
| 107 的裸脚本号 | `native/cards/senro.c:153-154`，用在 `:307` / `:343` |
| abcard 是另一套计数 | `assets/build_abcard.py:7-8`、`:81-87`（自己的 ORDER、`base = 原生 abcard entry 数`） |
| `.sht` 只吃脚本号 | `assets/sht/append_shooterset.py:519-534`（`anm`/`hit_fx` 用宏名解析）、`:674-678`（读 `anm_ids.h`） |
| DLL 必须与 anm 同时发布 | `AUDIT.md` §CC-2 尾注 + `local/DLL-SYNC.md` |

## 9. 实装记录（2026-09-13 做完）

> §0–§8 是**只读审计**（写的时候一个文件都没动）；本节记**实装**：改了什么、新的数字、怎么验的、
> 以及**没做成什么**。所有数字都是 `thanm -l 18` **回读构建产物**得到的，不是生成器日志。

### 9.1 改了什么（逐文件）

| # | 文件 | 改了什么 |
| --- | --- | --- |
| 1 | `assets/ability/entries/ORDER.txt` | 换成 **v2 语法**并重排成 A/B/C/D 四段 + 退役段（**最后一次重排**）：A 共享 HUD → B 别人的卡 19 行**原样搬** → C 114–117 → D 106–113 → REF 退役段。语法写在文件头注释里 |
| 2 | `assets/ability/entries/ORDER.frozen.txt` | **新增**：A/B/C 段 186 个数据行的逐行 `sha256` + **结构指纹**（= 重排检测的依据）|
| 3 | `assets/anmlib.py` | `read_order()` 认 v2 语法（`ATLAS` 段 / 网格 `@列x行` / 显式 `@x,y,WxH` / `REF`）；`make_entry(sprites=[…])` 支持**一个 entry 多个 sprite**；新增 `parse_sprite_rects()` / `order_signature()` / `read_order_names()`（abcard 的一列名字 ORDER）；`verify_rebuilt(on_extract=…)` 钩子 |
| 4 | `assets/build_ability.py` | 图集**合成画布**（成员按矩形贴，**不许补边**）、逐成员 sprite 号、`write_ids_h()` 逐名 + 图集别名 + 退役名、**图集逐像素自检**（从 anm 回读的画布裁矩形比对源图）、**重排检测硬门**、**256 entry 硬门 + 余量打印**、单图老路径一字未改 |
| 5 | `assets/check_ability_anm_bounds.py` | 把「宏名后缀 == entry 名」放宽成「**宏名 → ORDER 给的 entry + 矩形**」；脚本侧的 sprite 引用也加比矩形 |
| 6 | `assets/ability/make_atlas.py` | **新增**：独立对账工具 —— 解析审计 §4.3 的表 vs `ORDER.txt` 的图集段（逐格比），自己画一遍画布 + `tobytes()` 逐像素自检，并写预览图到 `native/build/atlas-preview/` |
| 7 | `assets/ability/make_font2_art.py` | 16×20 的字形**在生成器里**居中补成 16×32（原来靠构建期补）⇒ 图集规则①没有例外，贴图逐字节不变 |
| 8 | `assets/ability/retired/BLANK_1x1.png` | **新增**：1×1 全透明，退役名的落点 |
| 9 | `native/cards/senro.c:153-154` | 裸脚本号 `204/205` → `CE_ANM_ABILITY_SCRIPT_SENRO_{FIELD,RING}` 宏（值不变；§5.2 #1 的"建议改"）|
| 10 | `assets/ability/make_*.py`（12 个）+ `assets/ability/README.md` | **注释/说明统一**：把所有「因为构建期要求 2 的幂 / 因为一个 entry 只能一个 sprite，所以补边 / 拼图」的**过期理由**改成真实理由（并指向本节）。**产物逐字节不变**（见 §9.5）；顺手删掉 `make_sakura/senro/tpoint/sanae_youkai` 里"尺寸要是 2 的幂"的 `assert`（那条规则已作废，留着会挡原生尺寸的合法切图）|
| 11 | `assets/ability/make_trance_spirit_scripts.py` | **修一个真踩到的坑**：它按「当前最大号 + 1」发号，**重跑时认不出自己上次生成的 18 个文件** ⇒ 又从 262 起生成一批同名副本（`SCRIPT_SPIRIT_PLAIN1` 与 ORDER 撞名、整次构建直接红）。改成**按文件名认领**自己上次的号（幂等：覆盖而不是另开一批）|
| 12 | `native/Makefile` | 新增 `make atlas-check`（图集独立对账，**故意不进 `check`** —— 它读一份文档，且更硬的像素门已在 `anm-verify` 里）|
| 13 | `native/anm_ids.h`、`native/build/ability.anm` | 生成物（`make anm`）|

### 9.2 新的数字

| 指标 | 合并前 | **现在** | 说明 |
| --- | --- | --- | --- |
| entry（总 / 追加）| 367 / 360 | **48 / 41** | 上限 **256**（贴图键低 8 位：`th18:0x486d38` / `th18:0x47e1a4`）|
| **entry 余量** | 负 111 | **208** | 给以后的新卡美术（用户口径：要留余量）|
| sprite | 469 | **415** | 262 张进图集 + 19 单图 + 1 张 1×1 容器；**sprite 号不受限** |
| script | 262 | **262** | 一个都没动（脚本号来自文件名）|
| 追加 entry 里 ≥256 的 | 111 | **0** | 107×2 / 108×94 / 110×11 / 111×3 / 112×1 全部修好 |
| `ability.anm` 体积 | 9,472,576 B | **9,276,276 B** | −196 KB（退役 55 张省的比画布空白多）|
| 图集 | — | **21 个** | + 别人的卡 19 张单图 + 1 个退役容器 |

#### 现行 entry / sprite 号（`thanm -l` + `anm_ids.h` 回读）

| 图集 / 单图 | 类型 | 画布 | 成员 | entry | sprite |
| --- | --- | --- | --- | --- | --- |
| `ATLAS_HUD_DIGIT` | 图集 | 128×96@4×3 | 10 | entry7 | sprite109…118 |
| `ATLAS_FONT2` | 图集 | 128×64@8×2 | 14 | entry8 | sprite119…132 |
| `REVERSE` | 单图 | — | 1 | entry9 | sprite133 |
| `ROYAL_FLUSH` | 单图 | — | 1 | entry10 | sprite134 |
| `SPADE_10_FX` | 单图 | — | 1 | entry11 | sprite135 |
| `SPADE_J_FX` | 单图 | — | 1 | entry12 | sprite136 |
| `SPADE_Q_FX` | 单图 | — | 1 | entry13 | sprite137 |
| `SPADE_K_FX` | 单图 | — | 1 | entry14 | sprite138 |
| `SPADE_A_FX` | 单图 | — | 1 | entry15 | sprite139 |
| `GOLD_888` | 单图 | — | 1 | entry16 | sprite140 |
| `JUDGMENT_FX` | 单图 | — | 1 | entry17 | sprite141 |
| `BLUE_EYES_DRAGON` | 单图 | — | 1 | entry18 | sprite142 |
| `BLUE_EYES_BEAM` | 单图 | — | 1 | entry19 | sprite143 |
| `BLUE_EYES_FLARE` | 单图 | — | 1 | entry20 | sprite144 |
| `BROKEN_CORE_ORB` | 单图 | — | 1 | entry21 | sprite145 |
| `BROKEN_CORE_BOLT` | 单图 | — | 1 | entry22 | sprite146 |
| `FIRELORD_BODY` | 单图 | — | 1 | entry23 | sprite147 |
| `FIRELORD_FIREBALL` | 单图 | — | 1 | entry24 | sprite148 |
| `FIRELORD_BLAST` | 单图 | — | 1 | entry25 | sprite149 |
| `PEA` | 单图 | — | 1 | entry26 | sprite150 |
| `PEASHOOTER_OPT` | 单图 | — | 1 | entry27 | sprite151 |
| `ATLAS_LOLK` | 图集 | 256×352 | 8 | entry28 | sprite152…159 |
| `ATLAS_SEASON_BULLET` | 图集 | 192×208 | 23 | entry29 | sprite160…182 |
| `ATLAS_SEASON_UI` | 图集 | 256×160 | 20 | entry30 | sprite183…202 |
| `ATLAS_ANIMAL_TOKEN` | 图集 | 128×160@4×5 | 19 | entry31 | sprite203…221 |
| `ATLAS_ANIMAL_64` | 图集 | 320×576@5×9 | 43 | entry32 | sprite222…264 |
| `ATLAS_ANIMAL_SMALL` | 图集 | 128×32@8×2 | 15 | entry33 | sprite265…279 |
| `ATLAS_ANIMAL_FX` | 图集 | 128×192 | 2 | entry34 | sprite280…281 |
| `ATLAS_ANIMAL_BEAM` | 图集 | 512×96@1×3 | 3 | entry35 | sprite282…284 |
| `ATLAS_SENRO` | 图集 | 256×320 | 4 | entry36 | sprite285…288 |
| `ATLAS_YK_FAM` | 图集 | 128×256@4×8 | 32 | entry37 | sprite289…320 |
| `ATLAS_YK_OPT` | 图集 | 160×176 | 22 | entry38 | sprite321…342 |
| `ATLAS_MOF_FAITH` | 图集 | 128×16 | 12 | entry39 | sprite343…354 |
| `ATLAS_SIGNAL` | 图集 | 128×32 | 3 | entry40 | sprite355…357 |
| `ATLAS_UFO_BODY` | 图集 | 256×256@4×4 | 16 | entry41 | sprite358…373 |
| `ATLAS_UFO_TOKEN` | 图集 | 128×224 | 13 | entry42 | sprite374…386 |
| `ATLAS_TRANCE_SPIRIT` | 图集 | 128×96 | 11 | entry43 | sprite387…397 |
| `ATLAS_TRANCE_GAUGE` | 图集 | 128×128 | 9 | entry44 | sprite398…406 |
| `ATLAS_TRANCE_POPUP` | 图集 | 128×16 | 6 | entry45 | sprite407…412 |
| `ATLAS_TRANCE_ASTRAL` | 图集 | 512×512@1×1 | 1 | entry46 | sprite413…413 |
| `ATLAS_RETIRED` | 图集 | 1×1@1×1 | 1 | entry47 | sprite414…414 |
| （REF）`YOUKAI_YUKARI_S11` | 退役 | — | 0 | — | sprite414（= RETIRED_BLANK） |
| （REF）`YOUKAI_YUKARI_S12` | 退役 | — | 0 | — | sprite414（= RETIRED_BLANK） |
| （REF）`YOUKAI_YUKARI_S13` | 退役 | — | 0 | — | sprite414（= RETIRED_BLANK） |
| （REF）`YOUKAI_YUKARI_S14` | 退役 | — | 0 | — | sprite414（= RETIRED_BLANK） |
| （REF）`YOUKAI_YUKARI_S15` | 退役 | — | 0 | — | sprite414（= RETIRED_BLANK） |
| （REF）`YOUKAI_YUKARI_S16` | 退役 | — | 0 | — | sprite414（= RETIRED_BLANK） |
| （REF）`YOUKAI_YUKARI_S17` | 退役 | — | 0 | — | sprite414（= RETIRED_BLANK） |
| （REF）`YOUKAI_YUKARI_S18` | 退役 | — | 0 | — | sprite414（= RETIRED_BLANK） |
| （REF）`YOUKAI_YUKARI_S19` | 退役 | — | 0 | — | sprite414（= RETIRED_BLANK） |
| （REF）`YOUKAI_YUKARI_S20` | 退役 | — | 0 | — | sprite414（= RETIRED_BLANK） |
| （REF）`YOUKAI_YUKARI_S21` | 退役 | — | 0 | — | sprite414（= RETIRED_BLANK） |
| （REF）`YOUKAI_ALICE_S11` | 退役 | — | 0 | — | sprite414（= RETIRED_BLANK） |
| （REF）`YOUKAI_ALICE_S12` | 退役 | — | 0 | — | sprite414（= RETIRED_BLANK） |
| （REF）`YOUKAI_ALICE_S13` | 退役 | — | 0 | — | sprite414（= RETIRED_BLANK） |
| （REF）`YOUKAI_ALICE_S14` | 退役 | — | 0 | — | sprite414（= RETIRED_BLANK） |
| （REF）`YOUKAI_ALICE_S15` | 退役 | — | 0 | — | sprite414（= RETIRED_BLANK） |
| （REF）`YOUKAI_ALICE_S16` | 退役 | — | 0 | — | sprite414（= RETIRED_BLANK） |
| （REF）`YOUKAI_ALICE_S17` | 退役 | — | 0 | — | sprite414（= RETIRED_BLANK） |
| （REF）`YOUKAI_ALICE_S18` | 退役 | — | 0 | — | sprite414（= RETIRED_BLANK） |
| （REF）`YOUKAI_ALICE_S19` | 退役 | — | 0 | — | sprite414（= RETIRED_BLANK） |
| （REF）`YOUKAI_ALICE_S20` | 退役 | — | 0 | — | sprite414（= RETIRED_BLANK） |
| （REF）`YOUKAI_ALICE_S21` | 退役 | — | 0 | — | sprite414（= RETIRED_BLANK） |
| （REF）`YOUKAI_AYA_S0` | 退役 | — | 0 | — | sprite414（= RETIRED_BLANK） |
| （REF）`YOUKAI_AYA_S1` | 退役 | — | 0 | — | sprite414（= RETIRED_BLANK） |
| （REF）`YOUKAI_AYA_S2` | 退役 | — | 0 | — | sprite414（= RETIRED_BLANK） |
| （REF）`YOUKAI_AYA_S3` | 退役 | — | 0 | — | sprite414（= RETIRED_BLANK） |
| （REF）`YOUKAI_AYA_S4` | 退役 | — | 0 | — | sprite414（= RETIRED_BLANK） |
| （REF）`YOUKAI_AYA_S5` | 退役 | — | 0 | — | sprite414（= RETIRED_BLANK） |
| （REF）`YOUKAI_AYA_S6` | 退役 | — | 0 | — | sprite414（= RETIRED_BLANK） |
| （REF）`YOUKAI_AYA_S7` | 退役 | — | 0 | — | sprite414（= RETIRED_BLANK） |
| （REF）`YOUKAI_AYA_S8` | 退役 | — | 0 | — | sprite414（= RETIRED_BLANK） |
| （REF）`YOUKAI_AYA_S9` | 退役 | — | 0 | — | sprite414（= RETIRED_BLANK） |
| （REF）`YOUKAI_AYA_S10` | 退役 | — | 0 | — | sprite414（= RETIRED_BLANK） |
| （REF）`YOUKAI_AYA_S11` | 退役 | — | 0 | — | sprite414（= RETIRED_BLANK） |
| （REF）`YOUKAI_AYA_S12` | 退役 | — | 0 | — | sprite414（= RETIRED_BLANK） |
| （REF）`YOUKAI_AYA_S13` | 退役 | — | 0 | — | sprite414（= RETIRED_BLANK） |
| （REF）`YOUKAI_AYA_S14` | 退役 | — | 0 | — | sprite414（= RETIRED_BLANK） |
| （REF）`YOUKAI_AYA_S15` | 退役 | — | 0 | — | sprite414（= RETIRED_BLANK） |
| （REF）`YOUKAI_AYA_S16` | 退役 | — | 0 | — | sprite414（= RETIRED_BLANK） |
| （REF）`YOUKAI_AYA_S17` | 退役 | — | 0 | — | sprite414（= RETIRED_BLANK） |
| （REF）`YOUKAI_AYA_S18` | 退役 | — | 0 | — | sprite414（= RETIRED_BLANK） |
| （REF）`YOUKAI_AYA_S19` | 退役 | — | 0 | — | sprite414（= RETIRED_BLANK） |
| （REF）`YOUKAI_AYA_S20` | 退役 | — | 0 | — | sprite414（= RETIRED_BLANK） |
| （REF）`YOUKAI_AYA_S21` | 退役 | — | 0 | — | sprite414（= RETIRED_BLANK） |
| （REF）`YOUKAI_AYA_S22` | 退役 | — | 0 | — | sprite414（= RETIRED_BLANK） |
| （REF）`YOUKAI_AYA_S23` | 退役 | — | 0 | — | sprite414（= RETIRED_BLANK） |
| （REF）`YOUKAI_REMILIA_S11` | 退役 | — | 0 | — | sprite414（= RETIRED_BLANK） |
| （REF）`YOUKAI_REMILIA_S12` | 退役 | — | 0 | — | sprite414（= RETIRED_BLANK） |
| （REF）`YOUKAI_REMILIA_S13` | 退役 | — | 0 | — | sprite414（= RETIRED_BLANK） |
| （REF）`YOUKAI_REMILIA_S14` | 退役 | — | 0 | — | sprite414（= RETIRED_BLANK） |
| （REF）`YOUKAI_REMILIA_S15` | 退役 | — | 0 | — | sprite414（= RETIRED_BLANK） |
| （REF）`YOUKAI_REMILIA_S16` | 退役 | — | 0 | — | sprite414（= RETIRED_BLANK） |
| （REF）`YOUKAI_REMILIA_S17` | 退役 | — | 0 | — | sprite414（= RETIRED_BLANK） |
| （REF）`YOUKAI_REMILIA_S18` | 退役 | — | 0 | — | sprite414（= RETIRED_BLANK） |
| （REF）`YOUKAI_REMILIA_S19` | 退役 | — | 0 | — | sprite414（= RETIRED_BLANK） |

#### 算术族（**必须连续升序**，逐条从 `anm_ids.h` 读回来核过）

| 族 | sprite 号 | 结果 |
| --- | --- | --- |
| `TS_DIGIT_0..9` | 109…118 | ✅ 连续升序（`sdk.c:581/617` 用 `TS_DIGIT_0 + v`）|
| `FONT2_0..9` | 119…128 | ✅ 连续升序（`sdk.c:629` 用 `FONT2_0 + (ch-'0')`）|
| `AURA_WOLF_0..7` | 222…229 | ✅ 连续升序（`animal.c:2009` 用 `AURA_WOLF_0 + k`）|
| `AURA_OTTER_0..7` | 230…237 | ✅ 同上（`animal.c:2010`）|
| `AURA_EAGLE_0..7` | 238…245 | ✅ 同上（`animal.c:2011`）|

### 9.3 与提案的四处偏差（都说清为什么）

1. **108 妖侧本体那 2 个图集没排**（`ATLAS_YK_BODY` 46 帧 + `ATLAS_YK_REMILIA` 9 帧）—— 用户 2026-09-13 裁决
   「妖侧本体的『连接』暂时删除，但是其本身不要删除，没人引用是因为 108 还没开始真做」。
   ⇒ **源 PNG 一个都没删**（`assets/ability/youkai/`、`assets/ability/sanae_youkai/`），只是不进图集：
   **−2 entry、−55 sprite**，entry38 之后的号整体前移 2。
   ⚠ 这 55 个名字**不能直接删名**：脚本 189–203 / 218–222（"死脚本"：没有任何 C 代码起它们）
   里仍写着 `@YOUKAI_YUKARI_S11` 这类占位，名字消失会让整个构建抛「脚本里有未定义的占位 @…」。
   ⇒ 全部走 `REF … RETIRED_BLANK`（1×1 全透明）：死脚本照旧编得进去、画不出东西。
   **108 真做时**：把这批源图排成图集**追加到 ORDER.txt 末尾**（老行一行不动），再把 REF 段删掉。
   ✅ 已核：引用这 55 个名字的脚本**恰好 20 个**（`189–203` + `218–222`，正是 §3.1 的死脚本清单），
   并逐条查过 `native/**` 与 `assets/sht/append_shooterset.py`：**没有任何 C / sht 引用它们的脚本宏**
   ⇒ 退役指向 1×1 透明**不会**影响任何会被调用的画面。反过来：哪天 108 真接上这几条脚本，
   必须先把真图排进图集（否则画出来的是一片 1×1 透明 = 看不见东西）。
2. **多一个 `ATLAS_RETIRED`**（1 entry / 1 sprite，1×1 全透明）= 上面那 55 个退役名的容器（不占美术）。
   §3.2 里**其余的死图**（`SEASON_SEED` / `FAITH_DOT` / `SAKURA_*` / `TPOINT` / `GAUGE_FRAME` /
   `ITEM_*` / `CARD_*` / `FISH_POINT` …）**照样进了图集**：它们只是图集里的一个格子，**不额外占 entry**，
   而且留着它们才让 `script114/144/185/186/187/188` 那些死脚本仍能编译 —— 与 §3.2「一张都不删」的建议一致。
3. **`FONT2` 的 16×32 例外（§4.1⑥）换了实现位置**：补边从「构建期由 `build_ability.py` 居中补」搬到
   `make_font2_art.py`（PNG 现在就是 16×32）⇒ **规则①在图集里没有任何例外**，
   而贴图与合并前**逐字节相同**（已核：14 张全部相同）。`sdk.c:690` 的 `+5.0f * sy * dat` 不受影响。
4. 与提案**一致**的一处要点名：`TRANCE_POPUP_SPELL1/2` 的矩形按 §6.1 从 32×8 改成 **24×8**
   （旧 32×8 两侧各 4 px 是**补边**）—— 这是本次唯一"会看见"的尺寸变化，内容像素未动。

### 9.4 builder / 校验器上新增的硬门（这次新加的能力，都做成了可执行检查）

| 门 | 在哪 | 不满足会怎样 |
| --- | --- | --- |
| **256 entry 上限** | `build_ability.py` 的 `MAX_ENTRIES` | 直接报错退出，并给出"并进图集 / 拆槽"两条路；**每次构建还会打印余量** |
| **成员矩形 = 原图尺寸** | `build_ability.py` 的 `render_entry()` | 报错并指路（要补边请改美术生成器），不再"默默补边" |
| **图集逐像素自检** | `build_ability.py` 的 `check_entry_pixels()`（`--verify-only` 也跑）| 从 `thanm -x` 回读的画布上按矩形裁出来与源 PNG `tobytes()` 比，不一致就报错退出 |
| **重排检测** | `ORDER.frozen.txt` + `build_ability.py` 的 `check_frozen()` | 已定稿的前 186 行**逐行 sha256** 比对 + **结构指纹**；挪动/删改就报错退出，消息里写清"谁被挪了、怎么办、真要紧排怎么走" |
| **宏名 → entry + 矩形** | `check_ability_anm_bounds.py` | 卡侧 sprite 宏、脚本 `@NAME` 落错 entry 或落错**矩形**都报错 |
| **审计对账**（人工/CI 可选）| `assets/ability/make_atlas.py` | `ORDER.txt` 的图集段与本文 §4.3 **逐格**比；不一致就红 |

### 9.5 验收命令与结果

```bash
cd mods/th18.v1.00a/card-expand/native
make anm     # ✅ entry 48（原生 7 + 41）、script 262；图集逐像素自检 22 个图集 / 287 个成员全过
make sht     # ✅ 产物逐字节不变（.sht 只吃脚本号）—— 连跑两次 md5 相同
make check   # ✅ rc=0（含 anm-verify / check_ability_anm_bounds.py / sht-verify / dllx87 / dllverify）
python3 ../../../../tooling/check-docs.py   # ✅ 269 个文件全过
thanm -l 18 build/ability.anm -m …/v8.anmm  # entry 48 / sprite 415 / script 262
```

**这次迁移的核心正确性证据（逐像素）**：

| 检查 | 谁做的 | 结果 |
| --- | --- | --- |
| 图集成员：**从编译好的 anm 回读**的画布上按矩形裁出来 vs 源 PNG | `build_ability --verify-only`（`make anm-verify` ⊂ `make check`）| **22 个图集 / 287 个成员，全部逐字节相同** ✅ |
| 图集成员：自己画一遍画布再裁（独立代码路径）vs 源 PNG | `assets/ability/make_atlas.py` | **21 个图集 / 286 个成员全部相同** ✅ |
| **ORDER 的矩形 vs 审计 §4.3 的表** | `assets/ability/make_atlas.py` | **逐格一致**（23 个图集里 21 个进 ORDER，2 个按裁决不进）✅ |
| **逐名像素回归**：每个名字的新矩形内容 vs **合并前那一次构建**的贴图 | 一次性比对（本文记录）| **303 个名字逐字节相同**；2 个不同（`TRANCE_POPUP_SPELL1/2`，就是 §6.1 那条去补边）；1 个是新增的 `RETIRED_BLANK` ✅ |
| FONT2 的 16×20→16×32 搬家是否等價 | 同上 | 14 张与旧构建产物**逐字节相同** ✅ |
| 脚本号 | `make sht` + `thanm -l` | **262 个一个没变** ✅ |
| 原生部分（7 entry / 68 script / 原生贴图）| `verify_rebuilt()`（每次构建都跑）| 逐块、逐张一致 ✅ |
| **别人的卡那 19 行**（B 段）| 与 `git show HEAD:…/ORDER.txt` 逐字节比 | **19/19 完全相同** ✅（一个字都没改）|
| **旧 ORDER 的 344 个名字有没有丢** | 逐个查新 `ORDER.txt` 的定义 | **一个没丢** ✅（55 个退役名以 `REF` 形式仍在）|
| 构建**幂等** | `build_ability.py` 连跑两次比 md5 | `ability.anm` / `anm_ids.h` 逐字节相同 ✅ |
| 生成器**幂等 + 产物不变** | 26 个 `make_*.py` / `gen_banner.py` 全跑一遍再比 md5 | 366 张 PNG + 194 个脚本**逐字节不变** ✅（§9.1 #10/#11 的改动都是注释级）|

### 9.6 没做 / 待办（诚实清单）

1. **没实跑**。本文的所有"好了"都指**结构 + 像素**层面的证据，不是画面。部署后按 §6.2 的清单看一眼：
   113 的指示槽与弹窗、117 的五格灵槽/火焰/光环、**110/111 的 HUD（这两张卡此前就是坏的）**、
   107 的结界全屏闪光、112 的令牌与量表弧底（`UFO_ARC_STRIP` 此前是坏的）、共享 HUD 字形（`TS_DIGIT_*`/`FONT2_*`）。
2. **没跑 `make release`**（用户自己做部署）⇒ ⚠ **DLL 与 `ability.anm` 必须同时发布**：
   DLL 里烘着 `anm_ids.h` 的宏值，旧 DLL + 新 anm = **全部贴图错位**（比原来坏 111 个更糟）。
   `make check` 会把 DLL 重编好，但**发布件（`th18/ability.anm`）要跟着一起换**。
3. **108 真做时的两件事**：把那 55 张排成图集**追加到 ORDER.txt 末尾**；删掉 REF 段。
   （§7-① 建议的另一条路 —— 改走 `build_player_anm.py` 追加进 `pl0X.anm` —— 仍未做，那需要 108 的作者拍板。）
4. **别人的卡若在本地往 ORDER 追加过美术**：本次是**全量重排**，请以仓库里的 ORDER 为准（§7-②）；
   之后加美术**只能 append 到文件末尾**（重排检测会拦）。
5. `ORDER.frozen.txt` 目前冻的是 **A/B/C 三段 186 行**（共享 HUD + 别人的卡 + 114–117）。
   D 段（106–113）**故意不冻** —— 那几张卡还会改；改 D 段内部不影响任何已定稿的号。
