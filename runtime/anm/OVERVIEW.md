# anm — ANM v8 播放器实现状态

> **层次**：本文档是 `runtime/` 层，追踪**实现进度**，不是引擎结论。引擎语义的断言 ×
> 版本矩阵在 [`engine/anm/OVERVIEW.md`](../../engine/anm/OVERVIEW.md)；本文任何一行「实现」
> 都必须能指回那边的一条断言，指不回去就在代码里标 🟡（见 [`../README.md`](../README.md) 的
> 双向互指纪律）。

这是一个 ANM v8（TH13+，参见 [`engine/anm/OVERVIEW.md`](../../engine/anm/OVERVIEW.md)）
精灵动画播放器，用「行为对不对得上 th16」反向证伪 `engine/anm/` 的断言。当前处于
**阶段 1：地基**——格式层（I1）已实现，opcode 状态矩阵（指令语义）仍全部「未开始」。

## 格式层：已实现

`src/format/anm-v8.js` 的 `parseAnm(bytes: Uint8Array)` 按
[`engine/anm/th16/01-format.md`](../../engine/anm/th16/01-format.md) §2–§6 的字段表解析
entry 链表 / sprite 表 / script 表 / 指令流，只做结构解析，不含任何 opcode 语义
（VM 语义仍是下面矩阵的事）。

- 差分测试：`tests/format.test.js`，以 `thanm -l` 的输出为 oracle（夹具
  `tests/fixtures/minimal.anm.txt`，自写、零版权字节，`tests/build-fixtures.sh` 用
  `thanm -c 8` 编译）。
- 真实文件冒烟：`local/th16.v1.00a/dat/` 下全部 54 个 `.anm` 解析零异常，entry/script/
  sprite 计数（423 / 1929 / 3101）与 `01-format.md` §10 的一手审计逐字吻合；指令数
  41339 + 每 script 一条被本解析器排除在 `instructions` 之外的 `opcode=-1` 结束标记
  （1929）= 43268，同样吻合该文档的总指令数；其中 `paramMask !== 0` 的指令有
  **2572** 条（下游做变量引用解析的 sanity check 基数）。细节见
  `.superpowers/sdd/2026-09-09-runtime-anm-phase1/task-I1-report.md`。
- **输出契约**：`instructions[i]` = `{ time, opcode, paramMask, args }`。`paramMask`
  是 01-format.md §5.1 的原始 u16，格式层只**透出**、不解释——按位判断某个 `args[i]`
  是不是变量引用，再去查 VM 变量表，是下一步 VM 骨架的事，不是这层的事。可信度分层
  见 `01-format.md` §5.2：bit 0–8 ✅ 一手验证过，bit 9–15 ❓ 从没被测试到（不代表
  保留位，只是没证据）。
- **契约（下游 VM 骨架要遵守）**：`sprite.id`/`script.id` 是文件里存的原始值，不是
  位置序号（01-format.md §4/§5：加载器不读这两个字段）。要跨 entry 累加的运行时索引，
  用 `entries.flatMap(e => e.scripts)`/`entries.flatMap(e => e.sprites)` 的**数组下标**，
  不要用 `id`。详见 `anm-v8.js` 顶部注释。

## opcode 状态矩阵

下表由 [`tools/gen-opcode-matrix.py`](tools/gen-opcode-matrix.py) 从 thpages 的
`local/vendor/thpages/js/tables/reference/anm.ts` 生成（th16 继承链 `'16'` = `'15'` + 2 条，
135 个 opcode）。**重生成**：`python3 runtime/anm/tools/gen-opcode-matrix.py`。

thpages / th-re-data 是社区单源——「文档」列指向 `engine/anm/th16/` 里一手复核过的小节才算数，
纯抄 thpages 的助记名不算；「可信度」列默认 `—`（未看），community 自己标 `wip: 2`
（Global Constraints 里说的"社区标 wip:2 的 6 条"）的先标 🟡 提醒需要过 `METHOD.md` 的额外闸门。

<!-- gen-opcode-matrix:begin -->

| opcode | 组 | thpages 名 | 文档 | 实现 | 可信度 |
| ---: | --- | --- | --- | --- | --- |
| 0 | System | `nop` | 未开始 | 未开始 | — |
| 1 | System | `delete` | 未开始 | 未开始 | — |
| 2 | System | `static` | 未开始 | 未开始 | — |
| 3 | System | `stop` | 未开始 | 未开始 | — |
| 4 | System | `stop2` | 未开始 | 未开始 | 🟡（社区 wip:2，需过额外闸门） |
| 5 | System | `case` | 未开始 | 未开始 | — |
| 6 | System | `wait` | 未开始 | 未开始 | — |
| 7 | System | `caseReturn` | 未开始 | 未开始 | — |
| 100 | Math | `set` | 未开始 | 未开始 | — |
| 101 | Math | `setF` | 未开始 | 未开始 | — |
| 102 | Math | `add` | 未开始 | 未开始 | — |
| 103 | Math | `addF` | 未开始 | 未开始 | — |
| 104 | Math | `sub` | 未开始 | 未开始 | — |
| 105 | Math | `subF` | 未开始 | 未开始 | — |
| 106 | Math | `mul` | 未开始 | 未开始 | — |
| 107 | Math | `mulF` | 未开始 | 未开始 | — |
| 108 | Math | `div` | 未开始 | 未开始 | — |
| 109 | Math | `divF` | 未开始 | 未开始 | — |
| 110 | Math | `mod` | 未开始 | 未开始 | — |
| 111 | Math | `modF` | 未开始 | 未开始 | — |
| 112 | Math | `add3` | 未开始 | 未开始 | — |
| 113 | Math | `addF3` | 未开始 | 未开始 | — |
| 114 | Math | `sub3` | 未开始 | 未开始 | — |
| 115 | Math | `subF3` | 未开始 | 未开始 | — |
| 116 | Math | `mul3` | 未开始 | 未开始 | — |
| 117 | Math | `mulF3` | 未开始 | 未开始 | — |
| 118 | Math | `div3` | 未开始 | 未开始 | — |
| 119 | Math | `divF3` | 未开始 | 未开始 | — |
| 120 | Math | `mod3` | 未开始 | 未开始 | — |
| 121 | Math | `modF3` | 未开始 | 未开始 | — |
| 122 | Math | `rand` | 未开始 | 未开始 | — |
| 123 | Math | `randF` | 未开始 | 未开始 | — |
| 124 | Math | `mathSin` | 未开始 | 未开始 | — |
| 125 | Math | `mathCos` | 未开始 | 未开始 | — |
| 126 | Math | `mathTan` | 未开始 | 未开始 | — |
| 127 | Math | `mathAcos` | 未开始 | 未开始 | — |
| 128 | Math | `mathAtan` | 未开始 | 未开始 | — |
| 129 | Math | `mathReduceAngle` | 未开始 | 未开始 | — |
| 130 | Math | `mathCirclePos` | 未开始 | 未开始 | — |
| 131 | Math | `mathCirclePosRand` | 未开始 | 未开始 | — |
| 200 | Jumps | `jmp` | 未开始 | 未开始 | — |
| 201 | Jumps | `jmpDec` | 未开始 | 未开始 | — |
| 202 | Jumps | `jmpEq` | 未开始 | 未开始 | — |
| 203 | Jumps | `jmpEqF` | 未开始 | 未开始 | — |
| 204 | Jumps | `jmpNe` | 未开始 | 未开始 | — |
| 205 | Jumps | `jmpNeF` | 未开始 | 未开始 | — |
| 206 | Jumps | `jmpLt` | 未开始 | 未开始 | — |
| 207 | Jumps | `jmpLtF` | 未开始 | 未开始 | — |
| 208 | Jumps | `jmpLe` | 未开始 | 未开始 | — |
| 209 | Jumps | `jmpLeF` | 未开始 | 未开始 | — |
| 210 | Jumps | `jmpGt` | 未开始 | 未开始 | — |
| 211 | Jumps | `jmpGtF` | 未开始 | 未开始 | — |
| 212 | Jumps | `jmpGe` | 未开始 | 未开始 | — |
| 213 | Jumps | `jmpGeF` | 未开始 | 未开始 | — |
| 300 | Settings | `sprite` | 未开始 | 未开始 | — |
| 301 | Settings | `spriteRand` | 未开始 | 未开始 | — |
| 302 | Settings | `renderMode` | 未开始 | 未开始 | — |
| 303 | Settings | `blendMode` | 未开始 | 未开始 | — |
| 304 | Settings | `layer` | 未开始 | 未开始 | — |
| 305 | Settings | `noZBuffer` | 未开始 | 未开始 | — |
| 306 | Settings | `v8-306` | 未开始 | 未开始 | 🟡（社区 wip:2，需过额外闸门） |
| 307 | Settings | `v8-randMode` | 未开始 | 未开始 | — |
| 308 | Settings | `flipX` | 未开始 | 未开始 | — |
| 309 | Settings | `flipY` | 未开始 | 未开始 | — |
| 310 | Settings | `visible` | 未开始 | 未开始 | — |
| 311 | Settings | `resampleMode` | 未开始 | 未开始 | — |
| 312 | Settings | `scrollMode` | 未开始 | 未开始 | — |
| 313 | Settings | `resolutionMode` | 未开始 | 未开始 | — |
| 314 | Settings | `attached` | 未开始 | 未开始 | — |
| 315 | Settings | `colorizeChildren` | 未开始 | 未开始 | — |
| 316 | Settings | `v8-flag-316` | 未开始 | 未开始 | 🟡（社区 wip:2，需过额外闸门） |
| 317 | Settings | `v8-flag-317` | 未开始 | 未开始 | 🟡（社区 wip:2，需过额外闸门） |
| 400 | General | `pos` | 未开始 | 未开始 | — |
| 401 | General | `rotate` | 未开始 | 未开始 | — |
| 402 | General | `scale` | 未开始 | 未开始 | — |
| 403 | General | `alpha` | 未开始 | 未开始 | — |
| 404 | General | `rgb` | 未开始 | 未开始 | — |
| 405 | General | `alpha2` | 未开始 | 未开始 | — |
| 406 | General | `rgb2` | 未开始 | 未开始 | — |
| 407 | General | `posTime` | 未开始 | 未开始 | — |
| 408 | General | `rgbTime` | 未开始 | 未开始 | — |
| 409 | General | `alphaTime` | 未开始 | 未开始 | — |
| 410 | General | `rotateTime` | 未开始 | 未开始 | — |
| 411 | General | `rotateTime2D` | 未开始 | 未开始 | — |
| 412 | General | `scaleTime` | 未开始 | 未开始 | — |
| 413 | General | `rgb2Time` | 未开始 | 未开始 | — |
| 414 | General | `alpha2Time` | 未开始 | 未开始 | — |
| 415 | General | `angleVel` | 未开始 | 未开始 | — |
| 416 | General | `scaleGrowth` | 未开始 | 未开始 | — |
| 417 | General | `alphaTimeLinear` | 未开始 | 未开始 | — |
| 418 | General | `v8-418` | 未开始 | 未开始 | — |
| 419 | General | `v8-flag-419` | 未开始 | 未开始 | — |
| 420 | General | `posBezier` | 未开始 | 未开始 | — |
| 421 | General | `anchor` | 未开始 | 未开始 | — |
| 422 | General | `posAdopt` | 未开始 | 未开始 | 🟡（社区 wip:2，需过额外闸门） |
| 423 | General | `colorMode` | 未开始 | 未开始 | — |
| 424 | General | `rotateAuto` | 未开始 | 未开始 | — |
| 425 | General | `uVel` | 未开始 | 未开始 | — |
| 426 | General | `vVel` | 未开始 | 未开始 | — |
| 427 | General | `uVelTime` | 未开始 | 未开始 | — |
| 428 | General | `vVelTime` | 未开始 | 未开始 | — |
| 429 | General | `uvScale` | 未开始 | 未开始 | — |
| 430 | General | `uvScaleTime` | 未开始 | 未开始 | — |
| 431 | General | `v8-flag-431` | 未开始 | 未开始 | 🟡（社区 wip:2，需过额外闸门） |
| 432 | General | `slowdown-immune` | 未开始 | 未开始 | — |
| 433 | General | `posTime2D` | 未开始 | 未开始 | — |
| 434 | General | `scale2` | 未开始 | 未开始 | — |
| 435 | General | `scale2Time` | 未开始 | 未开始 | — |
| 436 | General | `anchorOffset` | 未开始 | 未开始 | — |
| 437 | General | `rotationSystem` | 未开始 | 未开始 | — |
| 438 | General | `originMode` | 未开始 | 未开始 | — |
| 500 | Child management | `createChild` | 未开始 | 未开始 | — |
| 501 | Child management | `createChildUi` | 未开始 | 未开始 | — |
| 502 | Child management | `prependChild` | 未开始 | 未开始 | — |
| 503 | Child management | `prependChildUi` | 未开始 | 未开始 | — |
| 504 | Child management | `createRoot` | 未开始 | 未开始 | — |
| 505 | Child management | `createChildPos` | 未开始 | 未开始 | — |
| 506 | Child management | `createRootPos` | 未开始 | 未开始 | — |
| 507 | Child management | `ignoreParent` | 未开始 | 未开始 | — |
| 508 | Child management | `createEffect` | 未开始 | 未开始 | — |
| 509 | Child management | `copyParentVars` | 未开始 | 未开始 | — |
| 600 | Drawing | `textureCircle` | 未开始 | 未开始 | — |
| 601 | Drawing | `textureArcEven` | 未开始 | 未开始 | — |
| 602 | Drawing | `textureArc` | 未开始 | 未开始 | — |
| 603 | Drawing | `drawRect` | 未开始 | 未开始 | — |
| 604 | Drawing | `drawPoly` | 未开始 | 未开始 | — |
| 605 | Drawing | `drawPolyBorder` | 未开始 | 未开始 | — |
| 606 | Drawing | `drawRectGrad` | 未开始 | 未开始 | — |
| 607 | Drawing | `drawRectShadow` | 未开始 | 未开始 | — |
| 608 | Drawing | `drawRectShadowGrad` | 未开始 | 未开始 | — |
| 609 | Drawing | `textureCylinder3D` | 未开始 | 未开始 | — |
| 610 | Drawing | `textureRing3D` | 未开始 | 未开始 | — |
| 611 | Drawing | `drawRing` | 未开始 | 未开始 | — |
| 612 | Drawing | `drawRectBorder` | 未开始 | 未开始 | — |
| 613 | Drawing | `drawLine` | 未开始 | 未开始 | — |

<!-- gen-opcode-matrix:end -->

> **图例**：✅ 在 th16.exe 上一手验过并已实现 · 🟡 待验/单源或社区自认不确定 ·
> ❌ 已知不同/不支持 · — 未开始

## 目录

```
runtime/anm/
  OVERVIEW.md         本文件
  package.json         零依赖，node --test 跑测试
  src/
    format/
      anm-v8.js         v8 格式解析器（parseAnm），指回 01-format.md §2–§6
  tools/
    gen-opcode-matrix.py   重生成上面那张矩阵
  tests/
    smoke.test.js       node:test 能跑起来的最小验证
    format.test.js       v8 解析器差分测试，以 thanm -l 为 oracle
    build-fixtures.sh    用 thanm -c 8 把 fixtures/*.anm.txt 编译成 .anm
    fixtures/
      minimal.anm.txt     自写夹具源码（入库）
      minimal.anm/.png    build-fixtures.sh 编译产物（gitignored，不入库）
```

## 下一步

阶段 1 的后续任务（波 D 及以后）在 `.superpowers/sdd/2026-09-09-runtime-anm-phase1/` 下的
计划里逐条排；这里不重复列——按 T0 brief 的说法，本文档不该跟踪其他任务的范围。
