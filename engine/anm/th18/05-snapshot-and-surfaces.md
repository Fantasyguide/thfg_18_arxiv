# anm/th18 §5 — P 键截图（BMP）× 帧内顺序 × 两张离屏面 × 层/组/优先级/RT 对照

> **版本**：TH18 v1.00a（`th18.exe`，imagebase `0x400000`）。本文所有地址都写全 `th18:0x…` 前缀。
> **方法**：一手平坦反汇编（`local/_scratch/th18_full.asm`，关键点与 `objdump -d -M intel` 直读 exe 逐字节对齐）
> + 直读 exe `.data/.rdata` 初值 + ExpHP `local/vendor/th-re-data/data/th18.v1.00a/`（funcs 交叉对名）。
> **缘起（用户 2026-09-xx）**：「按 P 截图，对**大部分我们加上去的 HUD 似乎不生效**」——即 `mods/th18.v1.00a/card-expand`
> 加的那些 HUD 大多没进 `snapshot\th18_###.bmp`。我不能跑游戏 ⇒ 本文只做「为什么」，并给出可验证的修法。
> **可信度**：§1 §2 §3 §4 §5 全部 ✅（每条都有写入点 + 读取点两端一手）；§6 的机制判到「层/组 → RT 边界」✅，
> 唯一没验的一环（0x3b 合成四边形的覆盖范围）标 🟡；§8 列出没做到的。

## 0. 结论速查

| 问题 | 结论 | 依据 |
| --- | --- | --- |
| 截图取的是本帧还是上一帧残留 | **本帧**：tick 列表全跑完 → draw 列表全跑完 → EndScene → 截图 → Present | `th18:0x471b53`/`th18:0x471bcb`/`th18:0x471bf5`/`th18:0x471c07`、`th18:0x472d18` |
| SwapEffect | **`D3DSWAPEFFECT_DISCARD`(1)**（Present 后后备缓冲内容未定义，但截图在 Present **之前** ⇒ 与本症状无关）| `th18:0x473f24` |
| BackBufferCount | **0**（默认 1 张）；且 `Flags = 1 = D3DPRESENTFLAG_LOCKABLE_BACKBUFFER` ⇒ 后备缓冲可 Lock | `th18:0x473fba`（静态初值 0）|
| 我们家 HUD 的两层落在哪 | **layer 24（UI 列表）→ 组 37 → 桩 `th18:0x487ce0` → on_draw 优先级 `0x39` → 画进「离屏面 B」**；**layer 25 → 组 38 → 桩 `th18:0x4878e0` → 优先级 `0x3e` → 直接画进后备缓冲** | `th18:0x4882f1`（折法）、`th18:0x487d99`（push 0x25）、`th18:0x4878e0`（push 0x26）|
| 两者之间是什么 | `0x3a` 阶段：SetRenderTarget(0, 后备缓冲) + **Clear 整面成 `0xff000000`**；`0x3b` 阶段：画 text.anm 的「屏幕四边形」把离屏面搬回后备缓冲 | `th18:0x455d00`–`th18:0x455d2e`、`th18:0x455cab` |
| 钉死的机制 | 我们的 HUD **在 0x3a 这次切 RT 的两侧被劈成两半**：layer 24 那批写进离屏面（`0x4ccfa0`，**不是** BMP 读的那张面），layer 25 那批写进后备缓冲（`0x4ccfa4`，**就是** BMP 读的那张面）| §5 §6 |
| 推荐修法 | **HUD 一律 layer 25**（UI 列表）。代价见 §7：不破坏暂停模糊（24/25 都在罩子之下），只改「相对合成/世界层 24–25 的叠放次序」| §7 |

## 1. 一帧的顺序：tick 全跑完 → draw 全跑完 → EndScene → 输入(内含截图) → Present

帧块在 **WinMain**（`th18:0x471270`–`th18:0x47226f`）里，一手：

```asm
471b43  mov  ecx,ds:0x4cf294        ; = supervisor（注册表对象）
471b49  mov  ds:0x4cd5e0,0x2
471b53  call 0x4012e0               ; ★ run_all_on_tick —— tick 列表整趟跑完
471b58  test eax,eax ; je 0x471f41  ; tick 返回 0 ⇒ 本帧不画也不 Present
471b60  cmp  eax,-1  ; je 0x471f2b
471b78  movzx eax,BYTE PTR ds:0x4cd00c     ; 「每 N 帧画一次」配置
471b81  cmp  eax,ecx ; jge 0x471c0c        ; 跳过帧：不画、不 Present、不截图
471b8f  call DWORD PTR [ecx+0xa4]   ; BeginScene
471bcb  call 0x401420               ; ★ run_all_on_draw —— draw 列表整趟跑完
471bf5  call DWORD PTR [ecx+0xa8]   ; EndScene
471c07  call 0x472b50               ; ↓
```

`th18:0x472b50`（帧率限制 + 截图 + Present 的壳）：

```asm
472b5a  call 0x473390               ; 取高精度计时（QPC）
472cc7  call DWORD PTR [ecx+0x4c]   ; GetRasterStatus —— 等 vblank（返回非 0 就跳出）
472d18  call 0x4731b0               ; ★ 输入处理；P 键截图就在这个函数里（§2）
472d2d  call DWORD PTR [ecx+0x44]   ; ★ Present(NULL,NULL,NULL,NULL)
```

**⇒ tick 全跑完才跑 draw；Present 在两个列表之后；截图夹在「draw 之后、Present 之前」。**
所以「截图读到的是上一帧 Present 完的残留」**不成立** ✅。

两个列表的遍历器与注册器（`0x4cf294` = supervisor，是列表哨兵所在的对象）：

| 项 | tick | draw |
| --- | --- | --- |
| 注册 | `th18:0x401180`（`ret 8`）| `th18:0x401230`（`ret 8`，同构）|
| 链表哨兵 | supervisor+0x14，头 = supervisor+**0x18** | supervisor+0x3c，头 = supervisor+**0x40** |
| 遍历 | `run_all_on_tick` `th18:0x4012e0`（读 `[ecx+0x18]`）| `run_all_on_draw` `th18:0x401420`（读 `[supervisor+0x40]`）|
| 顺序 | 注册时**按优先级升序**插链（`th18:0x4011d5`–`th18:0x4011e2`），遍历从表头到表尾 | 同左 |

节点结构（`UpdateFunc::operator new` `th18:0x401530`，对象 0x28 字节）一手：`+0x00` 优先级、`+0x04` flags、
`+0x08` **每帧回调**（= `operator new` 的唯一参数）、`+0x0c` **注册期一次性钩子**（`th18:0x401193`–`th18:0x4011a1`
调用一次后清零；返回值非 0 ⇒ 注册失败，调用方 `test eax,eax ; jne 错误路径`）、`+0x24` 回调的 `this`、`+0x14` 链节。

⚠️ **订正任务书里的一处**：`th18:0x453640` **不是**截图任务。它是 ExpHP 命名的 `Supervisor::on_registration`，
在 `th18:0x453ded` 是以 **`+0x0c` 一次性钩子**注册的（`th18:0x453de6`）；那一条 on_tick 优先级 1 的**每帧回调是
`th18:0x453460`**（`operator new` 的参数，`th18:0x453db0` 压栈，ExpHP：`Supervisor::on_tick`）。
截图函数是另一个地址：**`th18:0x453f40`**。

## 2. P 键 → BMP 的完整路径（一手）

触发点：输入处理函数 **`th18:0x4731b0`**（区间 `th18:0x4731b0`–`th18:0x473389`）：

```asm
4732fa  test DWORD PTR ds:0x4ca21c,0x40000   ; ← 配置里的截图键（P）；不按下就整段跳过
473304  je 0x473379
473317  push 0x4b7d54 ("snapshot") ; call 0x49bb2f   ; 建目录
473330  loop: push idx ; push dir ; push 0x4b7d64 ("%ssnapshot\th18_%.3d.bmp") ; call 0x41b310 (sprintf)
47334c        call 0x402220                          ; 该文件名已存在？（存在就 idx++ 重试）
473356        cmp esi,0x2710 ; jl loop               ; 最多试 10000 个
473367  call 0x453f40                                ; ★ 真正截图
```

截图函数 **`th18:0x453f40`**（区间 `th18:0x453f40`–`th18:0x45418d`）：

```asm
453f67  push filename ; push 0x4b6a50 ("SnapShot! %s\n") ; call 0x4562a0   ; 记日志
453f79  mov  eax,ds:0x4ccdf8          ; IDirect3DDevice9*
453f86  push &out ; push 0 ; push 0 ; push 0 ; push eax
453f8e  call DWORD PTR [ecx+0x48]     ; ★ GetBackBuffer(iSwapChain=0, BackBuffer=0, MONO, &out)
453fe2  mov  eax,ds:0x4cceec          ; = D3DPRESENT_PARAMETERS+0x08 = BackBufferFormat
453fe7  cmp  eax,0x15 ; jl  错误
453ff0  cmp  eax,0x16 ; jle 继续      ; A8R8G8B8 / X8R8G8B8 可以
453ff5  cmp  eax,0x17 ; jne 错误      ; R5G6B5 ⇒ push 0x4b6a60 ("16bit は取り込めない")
45409c  mov  DWORD PTR [eax],0x28     ; BITMAPINFOHEADER.biSize
454093  mov  WORD PTR [eax+0xe],0x18  ; biBitCount = 24
4540d8  push surface ; call DWORD PTR [ecx+0x34]  ; ★ 锁定（填出 pitch/pBits）
454107  call 0x4914c0                 ; memcpy（整张面对拷成一份自留地）
45411d  call DWORD PTR [ecx+0x38]     ; 解锁
454129  push 0x454190 ; call 0x402bb0 ; 把 24bpp 转换 + 写文件丢给线程
454141  push 0x4b6a78 ("snapShotScreen : " …) / 45416c push 0x4b6a98 ("error : snapShotScreen")
```

- BMP 尺寸 = **表面尺寸**，不是窗口尺寸：`biWidth = ds:0x56ac80`、`biHeight = ds:0x56ac84`（`th18:0x4540a8`/`th18:0x4540b6`）。
- 像素转换线程 `th18:0x454190`：从下往上逐行（`th18:0x4541be` 起 `dec edi`，`th18:0x4541d4` 行首 = `pitch × y`），
  每个源像素读一个 dword、只写 3 字节 ⇒ **alpha 被丢掉**，得到 24bpp BGR；文件写入用 `WriteFile`（`th18:0x454234`）、
  句柄在 `th18:0x4ca96c`。
- 结论：**BMP 的内容 = 调用 `GetBackBuffer(0,0,…)` 那一刻后备缓冲 0 的像素**，逐字节对拷，没有任何后处理 ✅。

## 3. `D3DPRESENT_PARAMETERS`：SwapEffect / BackBufferCount（一手）

全局结构体在 `0x4ccee4`（`.data` 初值**全 0**，靠 `th18:0x473df0` 那个函数在**本地**建好后整块拷进去：
`th18:0x473fa8`/`th18:0x473fcb`/`th18:0x473fd6`/`th18:0x473fe2`），随即 `th18:0x473fea call 0x473b20`
（= 建/重置设备；`CreateDevice` 调用点 `th18:0x473c19`–`th18:0x473c35`，设备指针写进 `0x4ccdf8`，
`th18:0x473d59` 把运行库可能改写过的结构体拷回 `0x4ccee4`）。

| 字段 | 偏移 | 值 | 一手 |
| --- | --- | --- | --- |
| BackBufferWidth / Height | +0x00 / +0x04 | 0 / 0（= 用窗口客户区尺寸）| `th18:0x473e25` 清零 |
| **BackBufferFormat** | +0x08 | 0x15(A8R8G8B8) 或 0x17(R5G6B5) | `th18:0x473ed2`/`th18:0x473f02`/`th18:0x473f60` |
| **BackBufferCount** | +0x0c | **0**（全 exe 无任何写入 ⇒ 静态初值 0）| `th18:0x473e2a` 清零 |
| MultiSampleType / Quality | +0x10 / +0x14 | 0 / 0 | `th18:0x473e2f` 清零 |
| **SwapEffect** | +0x18 | **1 = `D3DSWAPEFFECT_DISCARD`** | `th18:0x473f24`、`th18:0x473f34`、`th18:0x473f47` |
| hDeviceWindow | +0x1c | 0（用 `CreateDevice` 的 hFocusWindow）| `th18:0x473e34` 清零 |
| Windowed | +0x20 | 1（窗口）/ 0（全屏）| `th18:0x473f8d` / 静态 0 |
| EnableAutoDepthStencil / Format | +0x24 / +0x28 | 1 / 0x50(`D3DFMT_D24S8`) | `th18:0x473fa1` / `th18:0x473faf` |
| **Flags** | +0x2c | **1 = `D3DPRESENTFLAG_LOCKABLE_BACKBUFFER`** | `th18:0x473fba` |
| RefreshRate / PresentationInterval | +0x30 / +0x34 | 0 / 1(`INTERVAL_ONE`) 或 `0x80000000` | `th18:0x473f14`/`th18:0x473f1d`/`th18:0x473f3b` |

⚠️ **订正任务书的两处偏移**：`SwapEffect` 在 **+0x18**（不是 +0x8），`BackBufferCount` 在 **+0x0c**（不是 +0x4）。
`+0x08` 是 **BackBufferFormat** —— 这也解释了截图里那句格式检查读的正是 `0x4cceec`（= 结构体 +0x08，`th18:0x453fe2`）。

**这两条对本症状的含义**（别写反）：
- `D3DSWAPEFFECT_DISCARD` ⇒ **Present 之后**后备缓冲内容未定义；但截图在 Present **之前**（§1），
  所以「截图拿到被 DISCARD 清掉的面」**不成立**。
- `LOCKABLE_BACKBUFFER` ⇒ 对后备缓冲 `LockRect` 合法，所以 `th18:0x4540de` 那次锁定不会失败。
- `BackBufferCount = 0` + 全局只缓存一次后备缓冲面（`th18:0x453f21`）⇒ 全游戏**只有一个后备缓冲**，
  「画进 A 面、Preset 的是 B 面」这种双缓冲错位**不可能**发生 ✅。

## 4. 两张离屏面 + 后备缓冲：谁是谁（一手，含一处重要订正）

全 exe 只有 **5 处** `SetRenderTarget`（设备 vtable `+0x94`，2 参 + this）：

| 调用点 | 所属阶段（on_draw 优先级）| 目标面 |
| --- | --- | --- |
| `th18:0x455400` | `0x4553b0`，优先级 **1** | `ds:0x4ccf9c` |
| `th18:0x455934` | `0x455610`，优先级 **0xe** | `ds:0x4ccfa0` |
| `th18:0x455aa4` | `0x455a70`，优先级 **0x19** | `ds:0x4ccf9c` |
| `th18:0x455bf8` | `0x455bc0`，优先级 **0x2c** | `ds:0x4ccfa0` |
| `th18:0x455d0b` | `0x455cf0`，优先级 **0x3a** | `ds:0x4ccfa4` |

这三个全局是 **supervisor（`0x4ccdf0`）的字段**，用绝对地址写法在别处见过它们，所以早先按绝对地址 grep
「搜不到写入」是假象：

| 全局 | = supervisor+ | 是什么 | 一手（写入点）|
| --- | --- | --- | --- |
| `0x4ccf9c` | `+0x1ac` | **离屏面 A** = `text.anm` 第 12 张贴图的 level-0 面 | `th18:0x455ef6`–`th18:0x455f0b`（`GetSurfaceLevel(0,&[esi])`）|
| `0x4ccfa0` | `+0x1b0` | **离屏面 B** = `text.anm` 第 18 张贴图的 level-0 面 | `th18:0x455f0e`–`th18:0x455f29` |
| `0x4ccfa4` | `+0x1b4` | **后备缓冲面** | `th18:0x453f21`（初始化）与 `th18:0x455ef3`（`GetBackBuffer`）|
| `0x4cd61c` | `+0x82c` | `text.anm` 的 `AnmLoaded`（`th18:0x452d32 preload_anm("text.anm")`，串在 `0x4b6988`）| — |

⇒ **§2 里 `02-render-stages.md` 说的「surface 0 / surface 1」= 这两张 `text.anm` 贴图的 level-0 面**（同窗口大小），
都不是后备缓冲；而「回后备缓冲」= `0x3a` 那一次 ✅。这一步以前写着「🟡 surface 0/1 布局与窗口不同」，现在可以收紧。

## 5. 层 → 组 → 桩 → 优先级 → RT → Clear（完整对照）

`AnmManager::render_layer` **`th18:0x488260`** 两趟，逐指令：

```asm
; 第一趟：[esi+0x6f0] 那条链 = 世界列表（插入函数 th18:0x488350 写 +0x6f0/+0x6f4）
mov  edx,[edi+0x18]        ; VM 的 layer 字段
lea  eax,[edx-0x25] ; cmp eax,8 ; lea ecx,[edx-0xd] ; cmova ecx,edx
                           ; ⇒ layer 37..45 折成 layer-13；其余原样
cmp  ecx,[ebp+8] ; jne 跳过 ; call 0x481210（真画）
; 第二趟：[esi+0x6f8] = UI 列表（插入函数 th18:0x488490 写 +0x6f8/+0x6fc）
mov  ecx,[edx+0x18]
lea  eax,[ecx-0x18] ; cmp eax,8 ; jbe ⇒ ecx += 0xd      ; 24..32 ⇒ +13
                    ; 否则 37..45 原样；其余 ⇒ ecx = 0x27(39)   ; ★ UI 列表的兜底组 = 39
cmp  ecx,[ebp+8] ; jne 跳过 ; call 0x481210
```
两处 `test DWORD PTR [vm+0x538],0x180 ; jne 跳过`（`th18:0x488295`、`th18:0x4882e5`）—— **bit 7/8 置位的 VM 一律不画**。

我们走哪条链：`ce_anm_spawn_ui` → `AnmLoaded::instantiate_vm_to_ui_list_back` **`th18:0x409670`**
→ `th18:0x409712 call 0x488490` ⇒ **UI 列表** ✅（`ce_anm_spawn` 走 `th18:0x43a030`，是世界列表）。

### 5.1 主表（只列到我们关心的层）

| VM 所在列表 | VM `layer(n)` | 折到的组 | 绘制桩 | on_draw 优先级 | 那一刻的 RT |
| --- | --- | --- | --- | --- | --- |
| UI | **24** | **37** | `th18:0x487ce0`（`th18:0x487d99 push 0x25`）| **0x39** | **离屏面 B**（`0x2c` 切的，中间没人切回来）|
| UI | **25** | **38** | `th18:0x4878e0`（`th18:0x4878e0 push 0x26`）| **0x3e** | **后备缓冲**（`0x3a` 切回来的）|
| UI | 26 | 39 | `th18:0x4878f0`（push 0x27）| 0x41 | 后备缓冲 |
| UI | 27 | 40 | `th18:0x487900`（push 0x28）| 0x43 | 后备缓冲 |
| UI | 30 / 31 / 32 | 43 / 44 / 45 | `0x487910` / `0x487920` / `0x487930` | 0x55 / 0x57 / 0x5a | 后备缓冲 |
| 世界 | 24 | 24 | `th18:0x487ae0`（`th18:0x487ba0 push 0x18`）| **0x3c** | 后备缓冲 |
| 世界 | 25 | 25 | `th18:0x4878d0`（push 0x19）| 0x3d | 后备缓冲 |
| 世界 | 12–19 | 12–19 | 见 §1/§2 表 | 0x1b–0x2b | 离屏面 A 或 B（取决于所在段）|

⚠️ **订正 `02-render-stages.md` 的两处**（本文 §6 的结论完全依赖它们）：
1. 优先级 **`0x39`** 的桩（`0x487ce0`）画的是 **UI 列表的 layer 24**（组 37）——**不是**「世界列表 layer 37」；
   而优先级 **`0x3c`** 的桩（`0x487ae0`）画的是 **世界列表的 layer 24**（组 24），**不是 UI layer 24**。
   （`0x25 = 37` 与 `0x18 = 24` 都是脚本/数据结构里的十进制层号，桩压的就是它。）
2. 截图的症状与「哪一层被暂停模糊罩住」无关，而与 **`0x39` / `0x3e` 之间夹着的 `0x3a` 切 RT + 整面清屏** 有关。

### 5.2 那一小段 on_draw 时间线（一手，按优先级升序）

```asm
0x38  th18:0x487a10   世界层 23                                    RT = 离屏面 B
0x39  th18:0x487ce0   组 37 ← UI layer 24   ★我们的一大半 HUD      RT = 离屏面 B
0x3a  th18:0x455cf0   切 RT → 后备缓冲 + Clear(0,NULL,3,0xff000000,1.0f,0)
                      th18:0x455d00 push [0x4ccfa4] ; th18:0x455d0b call [ecx+0x94]
                      th18:0x455d22 push 0xff000000 ; th18:0x455d27 push 3 ; th18:0x455d2e call [ecx+0xac]
0x3b  th18:0x455c90   画「屏幕四边形」[0x4ccfb8]  ⇒ 把离屏面搬回后备缓冲
                      th18:0x455cab push [0x4ccfb8] ; th18:0x455cb7 call 0x481210
0x3c  th18:0x487ae0   世界层 24（顺带设 ZWRITEENABLE=0 / ZFUNC=ALWAYS / 相机 2）  RT = 后备缓冲
0x3d  th18:0x4878d0   世界层 25
0x3e  th18:0x4878e0   组 38 ← UI layer 25   ★我们的另一大半 HUD（= 暂停模糊罩同组）
0x3f  th18:0x419b10   AsciiManager on_draw_3f
```

- `0x3a` 那次 Clear 的语义：`Count=0, pRects=NULL, Flags=3(TARGET|ZBUFFER), Color=0xff000000, Z=1.0`
  ⇒ **把整张后备缓冲抹成不透明黑**（`th18:0x455d16` 起那一串压栈，Z 用 `th18:0x455d1b` 覆写）
  —— 这也是为什么「上一层画的、这一层之后不重画的东西」在**屏幕上也看不见**。
- `0x3b` 画的是 `text.anm` 的哪个脚本：由窗口宽度选 `th18:0x455f3f`（宽 0x280 → `0x42/0x48/0x45/0x4b`）、
  `th18:0x455fdd`（0x3c0 → `0x43/0x49/0x46/0x4c`）、`th18:0x456076`（0x500 → `0x44/0x4a/0x47/0x4d`），
  四个 VM 是 supervisor+0x1bc/0x1c0/0x1c4/0x1c8 = `0x4ccfac`/`0x4ccfb0`/`0x4ccfb4`/`0x4ccfb8`，
  分别画在优先级 `0xf`/`0x1a`/`0x2d`/`0x3b` ⇒ **`0x4ccfb8` 就是 `0x3b` 那次「把画面搬回后备缓冲」的四边形** 🟡（见 §8）。

## 6. 结论：为什么我们加的 HUD 大多不在 BMP 里

**机制一句话**：**我们的 HUD 被 `0x3a` 那次「切回后备缓冲 + 整面清黑」劈成了两半 —— `layer(24)` 的那批在
`0x39` 就画进了离屏面 B（`0x4ccfa0`，不是 BMP 读的那张面），只有等 `0x3b` 的合成四边形把它搬回来才可能进 BMP；
`layer(25)` 的那批在 `0x3e` 直接画进后备缓冲（`0x4ccfa4`，正是 `GetBackBuffer(0,0)` 读的那张面）。**

支撑地址：折法 `th18:0x4882f1`（UI 24→37）；桩 `th18:0x487ce0`+`th18:0x487d99`（组 37，优先级 `0x39`，
注册点 `th18:0x484a68`）；切 RT `th18:0x455d0b`（→ `0x4ccfa4`）；整面清黑 `th18:0x455d2e`（`Flags=3`、`0xff000000`）；
合成 `th18:0x455cb7`（`[0x4ccfb8]`）；桩 `th18:0x4878e0`（组 38，优先级 `0x3e`，注册点 `th18:0x484ad0`）；
截图读的面 `th18:0x453f8e`（`GetBackBuffer(0,0,…)`）。

一次**可以直接跑**的判别实验（把「是层的锅」与「是别的原因」分开，1 张图就够）：
在同一位置放两个一模一样的贴图 HUD VM，一个脚本写 `layer(24)`、一个写 `layer(25)`，
按 P 后看 BMP：**只有 25 的那个在图上** ⇒ 机制即上面这句；两个都在 ⇒ 我们的机制判错，得往 §8 的 ❓ 里找。

> ⚠️ **不成立的候选**（都已一手排除，别再往这两个方向修）：
> 1. 「画进了不被截图的表面」——**部分成立**：离屏面 A/B 确实不被直接截图（§4），但它们是同一帧里被
>    `0x3b` 搬回后备缓冲的；所以「丢的是哪些」只能由 `0x3a` 的两侧决定，不是「随便一张离屏面」。
> 2. 「在截图取像之后才画」——**不成立**：`run_all_on_draw`（`th18:0x471bcb`）整趟跑完才 `th18:0x472d18` 截图（§1）。
> 3. 「被 `0x3a` 的 Clear 抹掉」——**对屏幕是事实、对截图不是**：`0x3b` 之后会重画；真正没被重画的部分
>    **屏幕上也看不见**（这正好是 `0x39` 与 `0x3c` 两个桩夹在 Clear 两侧的意义）。

## 7. 修法（可验证 + 代价）

### 7.1 首选：把我们自己的 HUD 从 `layer(24)` 改成 `layer(25)`（UI 列表）

- 为什么能修：`25 → 组 38 → th18:0x4878e0 → 优先级 0x3e`，那时 RT 已经是**后备缓冲**（`0x3a` 切的），
  像素直接落在 `GetBackBuffer(0,0)` 会读到的那张面上 ⇒ 进 BMP 不再依赖 `0x3b` 的合成。
- 代价 1（暂停模糊）**不付**：按 `HUD-LAYOUT.md` §4.1 的硬约束表，**层 24(组 37) 与层 25(组 38) 都在模糊罩之下**
  （罩子 = `text.anm` script59 的 UI VM，`th18:0x458480` 起 `push 0x3b` 现场起，组 38，
  暂停那一刻才生成 ⇒ 同组内后画 ⇒ 盖在我们上面）。24→25 **不改变「被罩住」这个性质** ✅。
- 代价 2（叠放次序，必须自己复核）：HUD 从「合成之前」挪到「合成之后」，于是它会盖在
  `0x3b` 的合成四边形和 `0x3c`/`0x3d` 两个桩（世界列表 layer 24/25）之上。我们与世界层 24/25 无交集 ⇒
  实害应为零；但**如果将来有人把 HUD 放世界列表 layer 24/25，层号就要按世界列表的折法重算**
  （世界列表 24/25 不折，落在 `0x3c`/`0x3d`，本来就在后备缓冲里）。
- 代价 3：改 `assets/ability/scripts/**` 的 `layer()` 等于动共享件（CLAUDE.md 协作纪律第 2 条）——
  要重建 `build/ability.anm`、逐脚本 `--verify-only`，并在 `AUDIT.md` 登记；现有脚本里 **13 个写 `layer(24)`**
  （`107`–`112`、`116`、`123`–`127`）与 **11 个写 `layer(25)`**，这是一次性批量改。

### 7.2 备选 A：留在 24，但让组 37 在 `0x3a` 之后再画一遍

需要新增一个 on_draw 阶段（code cave）+ 往 draw 链注册（`th18:0x401230`，哨兵 supervisor+0x3c），
或把 `0x39` 那个桩的优先级改到 `> 0x3e`。
- 代价：**直接违反暂停模糊硬约束**——组 37 挪到罩子（组 38，`0x3e`）之后就**不再被罩住**，
  暂停时 HUD 会清晰地浮在模糊层上；且要改 UpdateFunc 链表（写堆、破坏 replay 无关性）。
- 只在「必须保留 layer 24 的既有叠放次序」时才考虑，不推荐。

### 7.3 备选 B：走 ASCII 文字路（`ce_hud_text*` / `ce_hud_vm_text_*` 中的文字部分）

AsciiManager 的 `0x3f`/`0x48`/`0x58` 三趟都在 `0x3a` 之后 ⇒ 直接进后备缓冲 ⇒ 一定进 BMP。
- 代价：**结构上不可能被暂停模糊罩住**（`03-ascii-text.md` §2f 一手：唯一早于罩子的那趟是 `0x37`，
  而它把相机切成相机 0、落在离屏面里，实跑看不见）⇒ 只适合「不需要被罩」的文字/数字。

### 7.4 若要「HUD 既被罩又进截图」的正解

就是 7.1：**贴图 VM + `layer(25)`**（`HUD-LAYOUT.md` §4.1 已经这么规定；本文只是补上「为什么 24 不行」——
24 会被 `0x3a` 挡在截图之外）。

## 8. 未答 / 待验（❓）

- ❓ **`0x3b` 合成四边形（`0x4ccfb8`，`text.anm` script `0x4b`/`0x4c`/`0x4d`）的覆盖范围与 UV 没验**：
  它是整面同尺寸采样，还是只搬「游玩区」那块矩形？这决定「layer 24 的 HUD 是否 100% 进不了 BMP」
  （若它整面搬，则 layer 24 应该也能进 BMP，我的机制就只解释了一部分）。
  **→ 主 agent 已复核一半，见 §8.1：脚本号与画的是哪个 VM 已坐实，但「覆盖范围」拿到了反向证据（🟡 未决）。**
  验法：解 `text.anm` 这三个脚本（脚本号随窗口宽度变，见 `th18:0x455f3f`/`th18:0x455fdd`/`th18:0x456076`）
  的 `set_sprite`/`scale`/`pos`；或按 §6 那个「24 vs 25 双胞胎实验」直接从 BMP 反推。
- ❓ `ds:0x4ca21c & 0x40000` 与物理键的对应没坐实：`0x4ca210`（当帧键位 dword）/`0x4ca214`（上一帧）
  由 `th18:0x401e00` 那张包装（`th18:0x401650` 取新值 → 交换 → `th18:0x404e90`）维护，
  而 `0x4ca21c` 的位语义我只按「用户按 P 出图」这一条现象反推。
- ❓ 设备与 surface 的 vtable 槽位号：本 build 里「取后备缓冲」在设备 `+0x48`（4 参 + this，`th18:0x453f8e`）、
  「锁定/解锁」在面对象 `+0x34`/`+0x38`（`th18:0x4540de`/`th18:0x45411d`，同族用法 `th18:0x487602`/`th18:0x48765a`）；
  同一个设备对象上 `SetRenderState` 在 `+0xe4`（按 `D3DRS_ZFUNC=0x17`、值 `8=D3DCMP_ALWAYS`、
  以及 `0xe=ZWRITEENABLE` 置 0 判定）。这些**槽位号与某版 DX SDK 头文件的行序对不上**（差 4/8 字节），
  原因未查（🟡 可能是 SDK 版本或 ZUN 自己的头）。本文一律按「语义 + 参数形态」认方法，不按头文件行序。
- ❓ `[vm+0x538] & 0x180`（`th18:0x488295`）这两个 bit 是什么（谁置位）没查；不排除「某些 HUD 在特定状态被它整帧跳过」。
- ❓ 未验：我们的 HUD 里究竟哪几个是 `ce_anm_spawn_ui` + `layer(24)`、哪几个是被 `0x3a` 之后才画的
  （这需要把每个 HUD 脚本的层与 spawn 路径对一张表；本文只给出「13 个脚本写 24、11 个写 25」这个计数）。

### 8.1 主 agent 独立复核（2026-09-12，`objdump -d` 直接读 exe）

**结论先给：`0x3a` 那段 ✅ 复现；`0x3b` 画的是哪个 VM ✅ 坐实；但「合成是否覆盖整面」拿到了反证 ⇒ §4 的机制强度从 ✅ 下调为 🟡。**

1. **`0x3b` 阶段 `th18:0x455c90`**（逐条）：
   - `cmpl $0x0,0x4ccf9c` 守门（离屏面 A 为空就整段跳过）；
   - `SetRenderState(0xf = `D3DRS_ALPHABLENDENABLE`, 0)` —— **关混合**（`+0xe4`）；
   - `mov 0x51f65c,%ecx`（AnmManager）+ `push 0x4ccfb8` → `call th18:0x481210`（按 `AnmVM**` 槽画一个 VM）；
   - `mov 0x4ccfb8,%eax` → `movl $0xffffffff,0x524(%eax)`；再 `SetRenderState(0xf, 1)` 恢复。
   ⇒ `0x3b` 画的正是 **supervisor+0x1c8（全局 `0x4ccfb8`）那个 VM**，且**关混合直贴**，语义确实是「把离屏面搬回来」。
2. **`0x3a` 阶段 `th18:0x455cf0`**：`call th18:0x47e730`（AnmManager）→ `SetRenderTarget(0, 0x4ccfa4)`（`+0x94`）
   → `Clear(0, NULL, 3, 0xff000000, 1.0f, 0)`（`+0xac`，Z 值用 `movl $0x3f800000,(%esp)` 现场改写栈顶）
   ✅ 与 §5 一致。
3. **屏幕四边形脚本的装载（补一手，`th18:0x455ec0` 起）**：按全局宽度 `0x56ac80` 三选一（`0x280`=640 / `0x3c0`=960 / `0x500`=1280）：
   三张面的 VM（supervisor+`0x1bc`/`0x1c0`/`0x1c4`）分别拿 `0x42`/`0x43`/`0x44`、`0x48`/`0x49`/`0x4a`、`0x45`/`0x46`/`0x47`；
   **第 4 个 VM（supervisor+`0x1c8` = `0x4ccfb8`，就是 `0x3b` 画的那个）拿 `0x4b`/`0x4c`/`0x4d`**
   ⇒ 本文 §8 与 §5「按宽度选 `0x4b`/`0x4c`/`0x4d`」**成立**（地址 `th18:0x455f3f`/`th18:0x455fdd`/`th18:0x456076` 三处 `cmp` + 尾部 `th18:0x455fd6`/`th18:0x45606f`/`th18:0x456108` 的 `push $0x4b/0x4c/0x4d`）。
4. **⚠️ 反证（覆盖范围）**：`thanm -l 18 local/th18.v1.00a/dat/text.anm` 读 `script75/76/77`（= `0x4b/0x4c/0x4d`）：
   它们用的是 `sprite93`/`sprite94`/`sprite95`，矩形 `(128,16,384,448)` / `(192,24,576,672)` / `(256,32,768,896)`，
   而脚本里 `ins_400` 给的目标尺寸是 **`224x240` / `336x360` / `448x480`**（三者按 1.5 倍递增，与 640/960/1280 同步）。
   - 若 `ins_400` 是「目标尺寸」，则 640 宽下这个四边形只画 **224x240 = 屏幕的 35% × 50%**，**不是整面**；
   - 且这三个脚本自带 **每段 60 帧的动画**（`ins_5` 跳转 + `+60:` 等帧 + `ins_412(60,9,…)` 插值 + `th18` 的旋转/翻转），
     不像「静态整面同尺寸搬运」。
   ⇒ **「layer 24 的 HUD 一定进不了 BMP」这条机制只解释了「搬回来的那块矩形之外的东西会丢」**：
   若合成只搬游玩区那类矩形，layer 24 的 HUD 确实进不了 BMP；若它最终覆盖整面，则 layer 24 也应该能进，机制就只解释一部分。
   **两种可能都还没有决定性证据。**
5. **⚠️ 我这个反证本身也可能读错**：`ins_400` 的语义我是从「脚本上下文 + 数值随窗口等比」推的，**没有对着
   `AnmManager` 的指令分发表认 handler**（这是本仓「不许把推断写成结论」的边界内 —— 但我把两边都摆出来，别只信一边）。
   坐实只有两条路：(a) 认 `ins_400` 的 handler 地址与参数语义；(b) 游戏内「同位置放 `layer(24)` 与 `layer(25)` 双胞胎」
   实验（用户已明确当前**不能开游戏** ⇒ 挂起）。
   在坐实之前，**不要把「HUD 一律改 `layer(25)`」当成已验证的修法**（AUDIT §BD 已按此降级登记）。

## 证据指针

截图触发/输入 `th18:0x4731b0`（键判定 `th18:0x4732fa`、起名循环 `th18:0x473330`、调用 `th18:0x473367`）·
截图本体 `th18:0x453f40`（GetBackBuffer `th18:0x453f8e`、格式检查 `th18:0x453fe2`、锁定 `th18:0x4540de`、
memcpy `th18:0x454107`、解锁 `th18:0x45411d`、BMP 线程 `th18:0x454190`）·
帧内顺序 `th18:0x471b53`/`th18:0x471bcb`/`th18:0x471bf5`/`th18:0x471c07` · 壳 `th18:0x472b50`（截图 `th18:0x472d18`、Present `th18:0x472d2d`）·
列表 `th18:0x401180`/`th18:0x401230`/`th18:0x4012e0`/`th18:0x401420`/`th18:0x401530` ·
`D3DPRESENT_PARAMETERS` 全局 `0x4ccee4`（构造 `th18:0x473df0`、建/重置设备 `th18:0x473b20`、CreateDevice `th18:0x473c35`）·
离屏面与后备缓冲 `0x4ccf9c`/`0x4ccfa0`/`0x4ccfa4`（取面 `th18:0x455ef3`/`th18:0x455f0b`/`th18:0x455f29`，初始化 `th18:0x453f21`）·
五处 SetRenderTarget `th18:0x455400`/`0x455934`/`0x455aa4`/`0x455bf8`/`0x455d0b` ·
`render_layer` `th18:0x488260`（世界折法 `th18:0x4882a4`、UI 折法 `th18:0x4882f1`、兜底组 `th18:0x48830b`）·
插入器 `th18:0x488350`（世界）/`th18:0x488490`（UI）· 我们走 `th18:0x409670` ·
桩 `th18:0x487ce0`（组 37，注册 `th18:0x484a68`）/`th18:0x487ae0`（组 24，注册 `th18:0x484930`）/
`th18:0x4878d0`（组 25，注册 `th18:0x4848c8`）/`th18:0x4878e0`（组 38，注册 `th18:0x484ad0`）·
`0x3a` 阶段 `th18:0x455cf0` · `0x3b` 阶段 `th18:0x455c90`（合成四边形 `th18:0x455cb7`，画 VM 的调用 `th18:0x481210`）·
屏幕四边形脚本装载 `th18:0x455ec0`（宽度全局 `0x56ac80`，三选一 `th18:0x455f3f`/`th18:0x455fdd`/`th18:0x456076`；
第 4 个 VM 取脚本 `th18:0x455fd6`/`th18:0x45606f`/`th18:0x456108`）· 暂停模糊罩 `th18:0x458480` · 字符串 `0x4b7d64`（BMP 名）/`0x4b6a50`（SnapShot!）/`0x4b6a60`（16bit）/`0x4b6988`（text.anm）。
