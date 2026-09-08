# `runtime/` — ANM v8 播放器（第一阶段）设计

> **版本**：跨版本。本文出现的地址一律带版本前缀（如 `th16:0x442560`）。
>
> **状态**：设计定稿 2026-09-09。决策见 §2，落地计划另出（`writing-plans`）。

## 0. 一句话结论

在 renkolab 下新开第四层 `runtime/`，放**引擎知识的可执行形式**。第一阶段做一个
**ANM v8 播放器**（JS + WebGL）：先把 135 条 opcode 的语义在 **th16** 上一手验完写进
`engine/anm/th16/`，再按组实现。th18 只差 2 条指令，几乎白送。

## 1. 为什么是 ANM，为什么是现在

`engine/anm/OVERVIEW.md` 开头写着「⚠️ 本子系统尚未系统开工」——272 行，是九个子系统里
最薄的一个（对比 sht 2468 行、card 2232 行）。而同一份文档自己说「东方里几乎所有
『显示』都挂在 `.anm` 上」。**最大的洞正好压在最要紧的接缝上。**

选它当第一阶段还有三个操作性理由：

1. **自成产品**——能播 anm 立刻对 modding 有用（现在改卡图还得靠 thanm 往返）。
2. **可独立验证**——不依赖 ECL 和游戏逻辑，跑真实 `.anm` 和原版比就能证伪。
3. **校准器**——做完才知道「逐 opcode 一手验证」的真实速度，用来修正后面所有估计。

反向论证：先做 ECL 执行会立刻撞墙。`ecl_enm_create` 一条指令就牵敌机管理器、anm、
碰撞、掉落——每个游戏 opcode 背后都要一整个子系统才能跑。

## 2. 决策记录

| # | 决策 | 理由 | 被否的选项 |
| --- | --- | --- | --- |
| D1 | 保真度先 A（能跑、看着像），B 作为知识工作留后 | B 的真门槛是按位复刻 MSVC CRT 的 `sinf`/`cosf`/`atan2f`，与语言无关 | 一上来追 replay 位级兼容 |
| D2 | JS + WebGL | 渲染与检查 UI 是 JS 主场；ANM 的 D3D9 语义到 WebGL 近乎直译 | Rust（研究型原型上过重）；Python 核心 + JS 壳（两套代码两道接口） |
| D3 | 知识先行（doc → code），按 opcode 组交替 | thpages 有 117 条完整描述当路标，不是盲读 | 实现即验证（验证强度靠人眼）；trace 驱动（前期投入过大，见 §7） |
| D4 | 逆向主攻 th16，th18 补差 | 见 §3 的数据：社区命名深四倍，指令集 98.5% 重合 | th18 主攻；两作并行对账 |
| D5 | 目录名 `runtime/` | 准确描述「执行 th18/th16 资产的运行时」，且点明与 `engine/` 的关系 | `remake/`（过度承诺）、`engine-js/`（与 `engine/` 撞名） |

D1 的一处更正记录在案：早先判断「Rust 是唯一能通往 B 的语言」**不成立**。f32 语义在
Python（`numpy.float32`）和 JS（`Math.fround`，asm.js 即靠此保证位级语义）里都拿得到，
而 Rust 的 `f32::sin` 走系统 libm，同样对不上 MSVC。语言这一项的承重比原判断小得多。

## 3. 为什么主攻 th16（数据）

ExpHP 的 thpages 把 v8 指令集按游戏建表，逐作继承。读源码可直接得到：

```
'15'  = '143'
'16'  = '15'  + 612 drawRectBorder, 613 drawLine
'165' = '16'  + 439 vd-imaginary-439
'17'  = '165' then delete(439)          → '17' ≡ '16'
'18'  = '17'  + 439 fadeNearCamera, 614 drawArc
```

| 对比项 | th16 | th18 |
| --- | :---: | :---: |
| v8 opcode 条数 | 135 | 137 |
| `th-re-data` 里含 `Anm` 的函数命名 | **165** | 43 |
| Ghidra 库里 anm opcode case labels | 137 | 136 |

**`th18 − th16` = `{439 fadeNearCamera, 614 drawArc}`；`th16 − th18` = 空；同号异义 = 零。**

即 98.5% 重合。而 ExpHP 在 th16 上给出 `AnmFastVm::` / `AnmBehemoth::` / `AnmLoaded::create_*`
整族命名（th16 独有 146 个，th18 独有仅 24 个）。**在 th16 上做，等于白拿四倍的命名底图。**

仓库自身的重心也在 th16——`engine/bullet/`、`engine/ecl/`、`engine/sht/` 的 ✅ 主力都在那作，
交叉引用顺手。th18 侧的成果（card / msg / player）不受影响：播放器是 v8 通用的，直接吃 th18 的 `.anm`。

## 4. `runtime/` 的定位与纪律

第四层，与现有三层这样分工：

> `engine/` 引擎怎么工作 · `runtime/` **同一份知识的可执行形式** · `mods/` 我改了什么 · `tooling/` 怎么再来一遍

四层的寿命不同，这是分层的唯一理由（同 `README.md` 的原话）：引擎知识跨版本可蒸馏；
mod 产物死绑 exe build；工具链版本无关；而 **`runtime/` 的寿命跟着格式版本走**——
一个 v8 播放器覆盖 th13–th185，比 mod 长、比 engine 短。

### 双向互指（写进 `runtime/README.md` 当纪律）

- **`runtime/` 里每条实现必须能指回 `engine/` 的一条断言。** 指不回去 = 你在猜，代码里标 🟡。
- **`runtime/` 跑出来的差异回流 `engine/` 当新问题。** 播放器是证伪装置，这是它对知识层的回报。

`runtime/` **不放引擎结论**——那是 `engine/` 的活。它只放代码，加一份「哪段代码依据哪条断言」的索引。
这条是为了防住 `CLAUDE.md` 点名的头号错误：在错误的层里写引擎语义。

## 5. 目录结构

照仓库惯例——**顶层目录用 `README.md` 当层门面，子系统用 `OVERVIEW.md` 挂状态矩阵**
（`engine/README.md` 对 `engine/<子系统>/OVERVIEW.md`，`mods/`、`games/` 同构）：

```
runtime/
  README.md            层门面：这一层是什么、与 engine/ 的关系、双向互指纪律、怎么跑
  anm/
    OVERVIEW.md        子系统门面：状态矩阵（哪条 opcode 已实现 / 已验证 / 待复核）
    src/
      format/          .anm v8 二进制解析（entry 表、sprite、script）
      vm/              opcode 执行；一组一个文件，对应 thpages 的七组
      render/          WebGL 薄壳：四相机、渲染阶段、混合模式
                       + D3D9-MAPPING.md（§10.4）
    tests/             最小 .anm 夹具 + 单测
    web/               查看器页面（时间轴、逐帧 VM 状态）
```

`vm/` 按 thpages 的分组切文件而不是按功能切，是为了让**代码结构和 opcode 表一一对应**——
`engine/anm/th16/` 里的表格行能直接映到文件，反过来漏实现的 opcode 一眼看得出。

## 6. 阶段划分

第一批产出落在 `engine/anm/th16/`，**不在 `runtime/`**——D3 是知识先行，`runtime/` 先只有
`README.md` 和计划，代码随文档长出来。

| 阶段 | 产出 | 落点 |
| --- | --- | --- |
| 0 · 地基 | v8 文件格式一手确认（thtk 权威 + th16 loader 复核） | `engine/anm/th16/01-format.md` + `runtime/README.md` |
| 1 · 语义 | 135 条 opcode 按七组分批对账 | `engine/anm/th16/02..08-*.md` |
| 2 · 实现 | 每组文档落地后立刻实现该组并跑通；Drawing 组含 render state 一手扫描（§10.4） | `runtime/anm/` |
| 3 · th18 | 补 439 / 614 两条 + 地址对账 | `engine/anm/th18/` |

### 为什么按组交替，而不是 135 条写完再动手

D3 的风险是「写完 135 条才知道对不对」。按组交替仍是 doc-before-code，但每组两三天
就有一次实跑反馈。**组的顺序不是随便排的**：System / Math / Jumps 是 VM 骨架，不落地
后面全组都验不了；Drawing 放最后，因为它最依赖渲染壳成熟。

### 七组的真实规模（th16，来自 thpages 源码统计）

| 组 | opcode 范围 | 条数 | 有完整描述 | `wip:1` 描述不全 | `wip:2` 社区也没搞懂 |
| --- | --- | ---: | ---: | ---: | ---: |
| System | 0–99 | 8 | 7 | 0 | 1 |
| Math | 100–199 | 32 | 32 | 0 | 0 |
| Jumps | 200–299 | 14 | 14 | 0 | 0 |
| Settings | 300–399 | 18 | 11 | 4 | 3 |
| General | 400–499 | 39 | 35 | 2 | 2 |
| Child management | 500–599 | 10 | 4 | 6 | 0 |
| Drawing | 600–699 | 14 | 14 | 0 | 0 |
| **合计** | | **135** | **117** | **12** | **6** |

Math 与 Jumps 的描述由 thpages 的 `OPERATOR_DATA` / `JUMP_DATA` / `OPERATOR_3_DATA`
三张生成器表批量产出（`a += b` 这一类），因此**风险最低、最适合当第一组落地**，
用来跑通「文档 → 实现 → 验证」的整条流水线。

### 风险名单（社区标 `wip:2` 的 6 条）

`4 stop2` · `306 v8-306` · `316 v8-flag-316` · `317 v8-flag-317` · `422 posAdopt` · `431 v8-flag-431`

这 6 条社区没搞懂，是**最可能产出「超过社区」结论的地方**，也因此要过 `METHOD.md`
的额外闸门（一手到底 / 对抗证伪 / 量纲常识 / 交叉对名），复核前一律 🟡。

`wip:1` 的 12 条（`305 noZBuffer` `307 v8-randMode` `313 resolutionMode` `314 attached`
`433 posTime2D` `438 originMode` 与 `502`–`508` 的 child 族）是描述不完整，补全即可。

## 7. 明确不做（YAGNI 边界）

第一阶段**只做**：ANM v8 的解析 + VM + 渲染，播放单个 `.anm` 里的单个 script。

**不做**：ECL、STD、背景、音效、菜单、存档、replay。

**trace harness 进 backlog。** 它指的是「用 thcrap 断点在原版进程里 dump anm VM 每帧状态
当 golden trace，让播放器自动回归」——这是本仓库独有的杠杆（card-expand 已有 22 导出 /
9 断点的成熟基础设施，别人做复刻只能靠眼睛比）。但它是一笔前期投入，且要过
`mods/_template/AUDIT-checklist.md` 的手写机器码审计。**触发条件**：阶段 2 撞上第一个
「肉眼看不出差异在哪」的 opcode 时再启用，不提前建。

## 8. 测试与版权

真 `.anm` 在 `local/`（gitignored，版权字节），**测试不能带**。

夹具用**我们自己写源码、用标准 thanm 编译出的最小 `.anm`**——零版权字节，可入库，
还能精确构造边界用例（空 script、interrupt、child vm、时间标签跳转）。真文件只在本地做回归。

这条同时满足 `README.md` 的头号纪律（仓库不留任何版权字节）与「测试必须可重跑」。

## 9. 技术栈

- **JS + WebGL**，无框架。
- **解析层自己写**，不依赖 truth / thtk：v8 格式简单，且必须能在浏览器里跑。
  thtk 仍是格式权威——出分歧时以它和一手 loader 为准。
- **f32 语义按需下沉**：默认用 JS number（f64）；确认某处需要单精度时，
  该处用 `Math.fround` 包成专用子模块并注明理由。不全局铺开（D1）。

## 10. 层 A / 层 B：D3D9 → WebGL 的映射

### 10.1 两层切分（第一阶段的结构前提）

ANM 系统分两层，**可移植性相反**：

| | 干什么 | 与 Direct3D 的关系 | opcode 数 |
| --- | --- | --- | ---: |
| **层 A · VM** | 算每个 vm 的 pos / scale / rotation / color / uv / flags / interrupt 跳转 | **零关系，纯算术** | 121 |
| **层 B · 渲染后端** | 把 vm 状态变成 draw call | D3D9 固定管线 | 14（Drawing 组） |

两个后果，都影响计划怎么排：

- **知识工作的主体在层 A**，与渲染 API 的选择无关——换 Rust / Canvas2D / Vulkan 都不改变
  §6 那 135 条的产出。D2（选 JS + WebGL）因此是**低风险决策**，选错了也只废掉层 B。
- **层 B 会掩盖层 A 的错误**：VM 算错 `pos`，画面歪了，分不清是算术错还是渲染没对齐。
  这是 trace harness 的正当性来源——它抓的是**层 A 的输出**，绕开渲染直接验算术（§7）。

### 10.2 已确认的 D3D9 用法（一手）

`engine/anm/th18/02-render-stages.md` 已给出：

```
D3DXMatrixLookAtLH(view, eye, at, up = (0, −1, 0))          // y 朝下
D3DXMatrixPerspectiveFovLH(proj, fov, W/H, near 1.0, far 10000.0)
SetRenderState(ZWRITEENABLE, 0) + SetRenderState(ZFUNC, ALWAYS)   // 层 20–23
CreateVertexBuffer(0x2d0, FVF 0x102)                        // XYZ|TEX1，每顶点 0x14 = xyz + uv
```

矩阵在 `th18:0x454950`（四台相机共用），模式 8 的共享顶点缓冲在 `th18:0x485110`，
层派发在 `th18:0x488260`。四台相机全透视，exe 里没有 Ortho。

### 10.3 映射表（首批）

| D3D9 | WebGL 对应 | 难度 | 可信度 |
| --- | --- | --- | :---: |
| 行向量 `v*M`、左手系、NDC z ∈ [0,1] | 列向量 `M*v`、NDC z ∈ [−1,1] | 低——`LookAtLH` / `PerspectiveFovLH` 公式公开，照抄 | ✅ |
| 像素中心在整数处（D3D9 的 −0.5 偏移） | 像素中心在 0.5 | **中——2D 精灵对齐肉眼可见** | ✅ |
| 多渲染目标 surface 0 / 1 + 视口切换 | FBO | 低 | ✅ |
| `D3DRS_ALPHATESTENABLE` / `ALPHAREF` | 无对应，fragment shader 里 `discard` | 低 | 🟡 |
| 固定管线 texture stage states | 无对应，写 shader 复现 | 中 | 🟡 |
| `D3DBLEND_*` | `gl.blendFunc`，基本 1:1 | 低 | 🟡 |

**后三行标 🟡 是因为仓库还没有一手证据说 ANM 具体设了哪些**——已确认的只有矩阵与
`ZWRITEENABLE` / `ZFUNC` 两条渲染状态。它们是按 D3D9 固定管线的一般情况推的，
按 `METHOD.md` 属「推断」，不是结论。

### 10.4 两样交付物

| 交付物 | 落点 | 为什么在那 |
| --- | --- | --- |
| `D3D9-MAPPING.md`：逐条「D3D9 语义 / WebGL 对应 / 差异 / 处置」 | `runtime/anm/render/` | 是我们的**实现决策**，不是引擎结论 |
| **render state 一手扫描**：ANM 绘制路径到底设了哪些 `SetRenderState` / `SetTextureStageState` | `engine/anm/th16/` | 是**引擎结论**，排进 Drawing 组 |

这两样分开放，正是 §4 双向互指纪律的第一个实例：扫描结论进 `engine/`，
照它做的取舍进 `runtime/`，后者引用前者。

### 10.5 为什么「逐像素一致」是错的标准

**原版自己跨 GPU / 驱动就不逐像素相同**——光栅化规则、纹理过滤精度、mipmap 选择
都有实现自由度。拿它当标准会把无穷的时间花在不存在的偏差上。

一个现成旁证：**dgVoodoo2**（一个 D3D9 → D3D12/Vulkan 的**通用**翻译层，不认识东方）
能跑 TH18 且画面正常。这说明**这游戏的渲染不依赖 D3D9 的冷门行为**，
能被按语义重写的后端正确重现——难度上限有参照。

所以 §12 的验收标准写的是「无可见差异」，不是「逐像素相同」。

## 11. 风险

| 风险 | 影响 | 处置 |
| --- | --- | --- |
| 135 条全靠一手复核，工作量被低估 | 阶段 1 拖长 | Math/Jumps 先跑通流水线，用真实速度重估后面五组 |
| 层 B 的偏差掩盖层 A 的算术错误 | 假阳性「实现正确」 | trace harness 抓层 A 输出、绕开渲染（§7 / §10.1） |
| WebGL 与 D3D9 语义不完全对应 | 画面细节偏差 | §10 的映射表 + render state 一手扫描；差异记进 `runtime/`，不污染 `engine/` |
| 在 `runtime/` 里写引擎结论 | 层次归属复辟 | §4 的双向互指纪律 + code review 时按 `CLAUDE.md` 查 |

## 12. 验收标准（第一阶段）

1. `runtime/anm/` 能加载真实 th16 `.anm`，播放**至少一个非平凡 script**（含时间标签跳转、
   interrupt、child vm 三者）；与原版同帧截图比对无可见差异。「非平凡」是硬要求——
   只播一个静态精灵证明不了 VM 是对的。
2. 135 条 opcode 在 `engine/anm/th16/` 有表，每条带可信度标记；**✅ 的比例是这一阶段的真实产出指标**。
3. `engine/anm/OVERVIEW.md` 的断言矩阵从当前的 6 行扩到覆盖 VM 语义，th16 列不再全 🟡。
4. `python3 tooling/check-docs.py` 全绿；夹具单测可重跑且不含版权字节。
5. th18 列通过补 `439` / `614` 两条 + 地址对账点亮。
