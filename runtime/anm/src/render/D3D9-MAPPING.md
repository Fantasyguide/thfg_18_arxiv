# D3D9 → WebGL 映射表

> **版本**：TH16 v1.00a（`th16.exe`，imagebase `0x400000`）。本文裸地址默认属该版本；
> 引用其他版本须写成 `th18:0x…`。
>
> **层次**：本文属 `runtime/` 层，是**实现侧的对照表**，不是引擎结论。每一行都指回
> [`engine/anm/th16/09-drawing.md`](../../../../engine/anm/th16/09-drawing.md)（下称「09」）§4 的一手扫描；
> 指不回去的行会显式标出来。代码在 `render-state.js`（纯映射）与 `webgl.js`（喂给 GL）。

## 1. 全局重置（09 §4.1，`th16:0x45ba80`）

| D3D9 render state | 值 | WebGL 对应 | 在哪 |
| --- | :---: | --- | --- |
| `ALPHABLENDENABLE` `0x1b` | 1 | `gl.enable(BLEND)` | `webgl.js` createRenderer |
| `ZFUNC` `0x17` | 8 = ALWAYS | `gl.disable(DEPTH_TEST)` | 同上 |
| `ALPHATESTENABLE` `0x0f` | 1 | shader `discard` 的开关（`uAlphaRef >= 0`） | `webgl.js` applyAlphaRef |
| `ALPHAREF` `0x18` | 1 | `uAlphaRef = 1/255` | `render-state.js` `ALPHA_TEST_THRESHOLD` |
| `ALPHAFUNC` `0x19` | 7 = GREATEREQUAL | `if (c.a < uAlphaRef) discard;` | FRAGMENT_SHADER |
| `SRCBLEND` / `DESTBLEND` / `BLENDOP` | 5 / 6 / 1 | 见 §2（每 VM 会覆盖） | — |

> **`ALPHAFUNC = GREATEREQUAL` + `ALPHAREF = 1` 不是 no-op**：它丢弃 `alpha < 1/255` 的像素。
> WebGL 没有固定管线 alpha test，只能在 fragment shader 里 `discard`。

## 2. 混合模式（09 §4.2 (a)，`flags_lo` bit 5–8，跳表 `th16:0x465254`）

| mode | D3D SRCBLEND / DESTBLEND / BLENDOP | WebGL `blendFunc` / `blendEquation` | alpha test |
| :---: | --- | --- | :---: |
| 0 | 5 / 6 / 1 | `SRC_ALPHA, ONE_MINUS_SRC_ALPHA` · `FUNC_ADD` | 开 |
| 1 | 5 / 2 / 1 | `SRC_ALPHA, ONE` · `FUNC_ADD` | 开 |
| 2 | 5 / 2 / 3 | `SRC_ALPHA, ONE` · `FUNC_REVERSE_SUBTRACT` | 开 |
| 3 | 2 / 1 / 1 | `ONE, ZERO` · `FUNC_ADD` | **关**（唯一一个） |
| 4 | 10 / 4 / 1 | `ONE_MINUS_DST_COLOR, ONE_MINUS_SRC_COLOR` · `FUNC_ADD` | 开 |
| 5 | 9 / 1 / 1 | `DST_COLOR, ZERO` · `FUNC_ADD` | 开 |
| 6 | 4 / 6 / 1 | `ONE_MINUS_SRC_COLOR, ONE_MINUS_SRC_ALPHA` · `FUNC_ADD` | 开 |
| 7 | 7 / 8 / 1 | `DST_ALPHA, ONE_MINUS_DST_ALPHA` · `FUNC_ADD` | 开 |
| 8 | 5 / 2 / 4 | `SRC_ALPHA, ONE` · `MIN` | 开 |
| 9 | 5 / 2 / 5 | `SRC_ALPHA, ONE` · `MAX` | 开 |
| >9 | — | **不设**（保持上一次的状态） | 不变 |

### 2.1 把因子代进方程之后（09 §4.2.1）

| mode | 闭式 | 备注 |
| :---: | --- | --- |
| 0 | `S·Sa + D(1−Sa)` | Normal |
| 1 | `S·Sa + D` | Add |
| 2 | `D − S·Sa` | Subtract · **打洞** |
| 3 | `S` | Replace |
| 4 | `S + D − 2SD` | **不是 Screen** · **打洞** |
| 5 | `S·D` | Multiply |
| 6 | `S(1−S) + D(1−Sa)` | **打洞** |
| 7 | `S·Da + D(1−Da)` | src 画在 dst 后 |
| 8 / 9 | `min(S,D)` / `max(S,D)` | 两个因子在 GL 下**空转**（规范规定 MIN/MAX 不带权重） |

> **mode 4 不是 Screen**：thpages 记作「Screen? (S + D − SD)」，实际是 `S + D − **2**SD`。
> 白叠白（`S = D = 1`）本式给 **0（黑）**、Screen 给 1（白），差满量程。别照 thpages 实现。

**实测用量**（54 个真实 `.anm` 里 `303 blendMode` 的分布）：`1`×440 · `2`×43 · `0`×16 ·
`3`×12 · `4`×6 · `9`×2 · `7`×1；**5 / 6 / 8 从未使用，也从未越界**。所以「>9 不设」这条
分支没有 ground truth——代码返回 `null` 让调用方什么都不做，**不回落到 mode 0**。

## 3. 采样器（09 §4.2 (b)(c)(d)）

⚠️ 这三项走的是 **`SetSamplerState`（vtable 69 = `0x114`）**，不是 `SetTextureStageState`
（67 = `0x10c`）。它们只影响**怎么取纹素**，不产生任何 color/alpha op——09 §4.2 专门提醒过这个坑。

| 项 | 位 | 值 → WebGL |
| --- | --- | --- |
| 过滤（`311 resampleMode`） | `flags_hi` bit 11 | 0 → `LINEAR`（MAG 与 MIN 同值）；1 → `NEAREST` |
| U 寻址（`312 scrollMode` 的 x） | `flags_hi` bit 0–1 | 0 → `REPEAT`；1 → `CLAMP_TO_EDGE`；2 → `MIRRORED_REPEAT` |
| V 寻址（`312` 的 y） | **`flags_lo` bit 30–31** | 同上 |

**U 和 V 在两个不同的字段里**——`312` 一条指令写两处。位值 **3** 在 09 §4.2 里没有记载，
真实数据里 `312` 只出现 `(2,0)` 与 `(0,1)`、**从未写出 3**，所以代码对它**显式抛**而不是猜一档。

## 4. Texture stage（09 §4.3）→ 一次乘法

`COLOROP`/`ALPHAOP` = `MODULATE`，`COLORARG2`/`ALPHAARG2` = `DIFFUSE`（sprite / textureCircle）
或 `TFACTOR`（textureArc3D），**`COLORARG1`/`ALPHAARG1` 从不设 ⇒ 恒为 D3D9 默认 `D3DTA_TEXTURE`**。

```glsl
vec4 c = texture2D(uTex, vUv) * vColor;   // 整条 texture stage 就是这一行
```

别按「D3D9 固定管线默认」再多写一层 color/alpha op（09 §8 的坑表列了这条）。

## 5. 顶点格式（09 §4.4）

| 路径 | FVF | stride | 图元 |
| --- | --- | :---: | --- |
| sprite 批量（`flush_sprites` `th16:0x465a80`） | `0x144` = XYZRHW\|DIFFUSE\|TEX1 | 28 | `TRIANGLELIST` |
| textureCircle（mode 9/13/14） | `0x144` | 28 | `TRIANGLESTRIP` |
| textureArc3D（mode 24/25） | `0x142` = XYZ\|DIFFUSE\|TEX1 | 24 | `TRIANGLESTRIP`（`DrawPrimitiveUP`） |

> ★ 最后一行 09 §4.4 原先三项都写错（`0x102` / 20 / `DrawPrimitive`），2026-09-10 已按
> `th16:0x468133` `push 0x142` 与 `th16:0x4681cc` `push 0x18` 更正。

**本实现的顶点是 9 个 float**（xyz + rgba + uv，stride 36 字节），与真机的打包方式不同——
真机把颜色打成一个 `D3DCOLOR` dword、位置带 `rhw`。这不影响像素，只是提交方式；
`XYZRHW` 的「已经是屏幕坐标」这层语义由 shader 的 `uModelSpace = false` 分支承担。

## 6. 三个必须知道的坑

1. **canvas 必须 `premultipliedAlpha: false`**。09 §4.2 的 10 种混合都是按「非预乘」写的；
   WebGL canvas 默认预乘，合成时会再乘一次 alpha，半透明处整体偏暗。
2. **`MIN`/`MAX`（blend mode 8/9）在 WebGL 1 需要 `EXT_blend_minmax`**，WebGL 2 原生有。
   缺扩展时本实现**抛**，不静默退回 `FUNC_ADD`——那会把「只取亮部」画成「加色」。
3. **D3D 屏幕坐标原点在左上、y 向下**，NDC 原点居中、y 向上。顶点着色器里
   `y_ndc = 1 - y/h*2`，少这一步整个画面上下颠倒。
4. **canvas 还要 `alpha: false`**（不只是 `premultipliedAlpha: false`）。D3D9 没开
   `SEPARATEALPHABLENDENABLE`（`th16:0x45ba80` 的扫描里没有它），所以**同一对因子也作用在
   alpha 通道上**，于是 **blend mode 2 / 4 / 6 在源不透明时把目标 alpha 算成 0**：

   | mode | alpha 闭式 | `Sa = Da = 1` |
   | :---: | --- | :---: |
   | 2 | `Da − Sa²` | 0 |
   | 4 | `Sa + Da − 2·Sa·Da` | 0 |
   | 6 | `(1−Sa)(Sa + Da)` | 0 |

   真机的后备缓冲不透明、alpha 不参与显示，所以没人在意；**网页上画到带 alpha 的 canvas
   里就是「打洞」**，页面背景会从图里透出来。真实数据里 mode 2 用了 **43 次**、mode 4 用了
   **6 次**，不是理论问题。thpages 只对 mode 2 提过这件事，4 与 6 同样如此。

   ⇒ `getContext('webgl2', { alpha: false, premultipliedAlpha: false })`。
   代码里 `ALPHA_DESTROYING_MODES` 导出了这三个号，测试逐条断言。

## 7. 没有一手依据的地方（如实标注）

| 项 | 状态 |
| --- | :---: |
| `CULLMODE` —— 09 §4.1 的扫描里没有它。本实现 `gl.disable(CULL_FACE)`（安全侧，不会误剔除） | ❓ |
| mode 24/25 的 **WORLD 矩阵**：真机在 `th16:0x467d00` 里构造（读 rotation/scale/scale_2/uv_scale），那段还没逆 ⇒ 本实现要求调用方自己给 `ctx.matrix`，不编一个 | ❓ 归 B6 |
| rect 族关 `ZWRITEENABLE`（`th16:0x4694a8`）——本实现整体关了深度测试，等价覆盖，但没有单独建模 | 🟡 |

## 关联

- 一手扫描：[`engine/anm/th16/09-drawing.md`](../../../../engine/anm/th16/09-drawing.md) §4
- 顶点怎么算出来的：[`geometry.js`](geometry.js)、[`10-sprite-geometry.md`](../../../../engine/anm/th16/10-sprite-geometry.md)
- 层 B 的任务分解：[`docs/superpowers/plans/2026-09-10-runtime-anm-layerb.md`](../../../../docs/superpowers/plans/2026-09-10-runtime-anm-layerb.md)
