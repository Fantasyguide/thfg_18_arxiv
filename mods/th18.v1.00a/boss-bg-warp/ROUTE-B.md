# ⛔ 状态：暂停/放弃（2026-10-22，例会前）

本路线（自绘"boss 式局部背景扭曲"）**已全部关闭**，不再对游戏产生任何效果：

| 层 | 关法 | 现状 |
| --- | --- | --- |
| 启动器 | `mods/enabled.json` 的 `dlls` 里**移除** `th18_bgwarp.dll` | ✅ 不会加载 |
| 部署 | 删掉 `thcrap/bin/mod_th18_bgwarp.dll` | ✅ |
| ini | `th18_routeb.ini`：`enable = 0`、`mode = 0`；`th18_bgwarp.ini`：`b_test = 0`、`mode = 0`、`retire = 0`、`touch_enemy_mesh = 0` | ✅ 即使被加载也无效 |

**恢复**：把 `th18_bgwarp.dll` 加回 `enabled.json` 的 `dlls`，并把上面几个值改回去即可 ✓

---

# 路线 B —— 「我们自己画」的自包含实现（`native/route_b.c`）

> **版本**：TH18 v1.00a（`th18.exe`，imagebase `0x400000`）。本文裸地址默认属该版。

> 单独一个文件的原因：主文件 `native/th18_bgwarp.c` 由**另一个并行 agent** 维护；
> 路线 B 放在自己的文件里（自装桩、自己的日志/ini），只在 `Makefile` 的 `SRC` 里加一行，**不改对方代码**。

## 文件布局（2026-10-21 拆分：为并进卡侧做准备）

| 文件 | 角色 | 说明 |
| --- | --- | --- |
| **`native/bgwarp_core.c` / `.h`** | ★ **可移植核心** | 自绘扭曲的全部实现（装桩/绘制/场/解贴图/防递归/重叠分域）。**零 ini、零日志文件、零热键** |
| `native/route_b.c` | 试验台 | 只负责 ini + 日志 + 热键 + 调 core（方便迭代） |
| `native/th18_bgwarp.c` | 别人的 | 另一个 agent 维护的顶点验证 mod（不碰） |

### 公开接口（`bgwarp_core.h`）

```c
int  bgwarp_install(bgwarp_log_fn logf);      /* 装层 6 桩，一次；失败即保持关闭（不影响游戏） */
void bgwarp_set(int on, float radius, float power, float alpha);   /* on=0 ⇒ 立刻停 */
void bgwarp_tune(int grid_n, float fade, int overlap, float blend);
void bgwarp_set_mode(int mode);  int bgwarp_mode(void);
void bgwarp_set_test(int surf, int two, float shift, float two_off, int radius_auto);
void bgwarp_set_tick(void (*fn)(void));       /* 宿主每帧回调（可选） */
void bgwarp_restore(void);                    /* 还原层 6 桩 */
```

### 并进卡侧（109 C4）只要三行

```c
bgwarp_install(card_log);                     /* DLL 加载后一次，或首次激活时 */
bgwarp_set(1, 160.0f, 1.0f, 255.0f);          /* C4 发动 */
bgwarp_set(0, 0, 0, 0);                       /* C4 结束 / on_delete（本仓铁律） */
```

### 生命周期（一手依据）

一手 boss 扭曲的消失方式是**硬停**：效果结束时引擎把
`stage+0x3410`（那份 aura 网格的指针）**直接清零**（`0x41da03`：`mov dword [edi+0x3410], 0`），
同段还把 `+0x3414=112.0f`、`+0x3418=192.0f`、相位 `+0x3434/+0x3438=0` 恢复默认；
每帧 tick 开头 `test ecx,ecx / je` ⇒ 指针为空就整段跳过 ⇒ **瞬间消失，不淡出** ✓
⇒ 所以 `bgwarp_set(0,…)` 就是**立刻停**（与一手一致）。要做淡出属于**自定**行为，须登记。

## 它做什么

| 环节 | 做法（一手见 [`AUDIT.md`](AUDIT.md) §O） |
| --- | --- |
| 插入点 | **层 6 绘制桩 `0x487780`**（叶子函数 `6A 06 E8 rel32 C3`，被调 `render_layer=0x488260`）——背景已合成、精灵层未画 |
| 装桩 | **替换型桩**：整个桩被替换，桩尾用 `mov eax,imm32; call eax` **绝对调用**复刻原动作 ⇒ 不搬任何相对寻址字节 |
| 绘制 | 复刻引擎：`SetTexture[+0x104]`、`SetFVF(0x144)[+0x164]`、`[+0x10c](0,6,0)`、`DrawPrimitiveUP[+0x14c]`；顶点 28B `{x,y,z=0,rhw=1,color,u,v}` |
| 状态 | 画前关 `ZENABLE/ZWRITEENABLE`、**开 `ALPHABLEND` 并把 COLOROP/ALPHAOP 钉在 TEXTURE/DIFFUSE**（先存后设、画完恢复）；画后把引擎的"当前贴图/FVF"缓存置成不可能值 |
| 采样源 | 离屏面 `[0x4ccfa0]`（默认）或 `[0x4ccf9c]`；**若它就是当前 RT 则自动换另一张**（否则采到自己 ⇒ 递归） |
| 坐标 | 世界 x∈[−192,192]、y∈[0,448]；**实测视口** `GetViewport(+0xC0)` ⇒ playfield 中心 = 视口中心 |
| 敌方圆心/半径 | 网格段 VM 的 `+0x5c8` 缓冲里就是它这一帧的顶点 ⇒ 扫一遍取包围盒（不碰引擎内部结构） |
| 光晕纹理 | 精灵序号 `[vm+0x20]`/`[vm+0x24]` → 条目表（步长 68，`+8`=handle）→ `handle>>8`/`&0xff` → 纹理表 `+0x18*i` |

## 模式（游戏里按 `8` 循环；`9` 开关第二份）

| 模式 | 内容 |
| --- | --- |
| 0 | 关（在舞台里也什么都不碰 D3D） |
| 1 | M1：整屏平移采样（只覆盖平移后那块） |
| 2 | **M2：N×N 扭曲网格**（N 3..65；圆心跟自机；圆外裁掉 + 外圈羽化）——**用户确认"融合得自然"** |
| 3 | 光晕纹理 + 屏幕 UV |
| 4 | **平面网格（位移=0）**：与原画面逐像素重合 ⇒ **"看不出来"就是对齐成功** |
| 5 | 光晕纹理 + 整图 UV（回答"这张纹理是什么"） |

## 闪退账（截至 2026-10-21）

| # | dump | 归因 | 处理 |
| --- | --- | --- | --- |
| 1 | `24472` | **我们**：认领 VM ⇒ 引擎回收 ⇒ 写 `[空vm+0x534]` | 放弃"自建网格"路线 |
| 2 | `105936` | 覆盖层签名（`Eip=0x1480968b`、栈全是 nvspcap） | 无法归因于我们（同签名出现 2 次） |
| 3 | `30508` | 同上（更早，桩还没装） | — |
| 4 | `84012` | 同 #2 签名 | — |
| 5 | `51608`/`37324` | **我们**：设备指针过期 ⇒ `call vt[0x3a]` 跳堆地址（`Eip=0x90a66dc`） | `rb_code()` 校验 + `mode` 默认 0 |
| 6 | `84840` | **我们**：**面的** vtable 没过校验 ⇒ `GetContainer` 跳堆（`Eip=0x8e43efc`） | 面也过 `rb_code()` + 设备换指针清缓存 |

⇒ 我们自己的真 bug 是 **3 个**（#1 VM 认领、#5 过期设备、#6 过期面）；后两个是同一类：
**"拿游戏给的指针直接 call"**。

## ★ 铁律（写进代码，不再靠记性）

**本 DLL 里所有间接调用，必须先过 `rb_code()`（`MEM_IMAGE` + 可执行页）**；
设备/面的指针一旦变化（`Reset`/重建），**立刻清空所有纹理与面缓存**。
目前 `rb_code()` 共 11 处（设备 9 个槽 + 面 2 个槽 + `GetRenderTarget`/`GetViewport`），
剩下唯一不校验的调用是静态导入的 `GetAsyncKeyState`（不可能指向堆）。

## ★ D3D 调用参数个数对照表（血泪：**缺参**会让 d3d9 崩在内部）

| 槽 | 方法 | 参数（含 this） | 曾错 |
| --- | --- | --- | --- |
| +0x0C | `TestCooperativeLevel()` | 1 | |
| +0x98 | `GetRenderTarget(RenderTargetIndex, ppSurface)` | **3** | ★ **曾经只传 2 个** ⇒ d3d9 写栈垃圾 ⇒ 崩（三次 dump 同 Eip `d3d9+0x11c00c`） |
| +0xC0 | `GetViewport(pViewport)` | 2 | |
| +0xE4 / +0xE8 | `SetRenderState(state,value)` / `GetRenderState(state,pValue)` | 3 | |
| +0x104 | `SetTexture(stage,pTexture)` | 3 | |
| +0x108 / +0x10C | `GetTextureStageState(stage,type,pValue)` / `SetTextureStageState(stage,type,value)` | **4** | ★ Get 曾经只传 3 |
| +0x14C | `DrawPrimitiveUP(pt,count,data,stride)` | 5 | |
| +0x164 | `SetFVF(fvf)` | 2 | |
| 面 +0x2C / +0x30 | `GetContainer(riid,ppContainer)` / `GetDesc(pDesc)` | 3 / 2 | |

### 纹理 / 设备侧（2026-10-22 又踩一次）

| 槽 | 方法 | 参数（含 this） | 曾错 |
| --- | --- | --- | --- |
| 设备 +0x5C | `CreateTexture(w,h,levels,usage,fmt,pool,ppTex,pShared)` | **9** | ✓ |
| 纹理 **15** | `GetSurfaceLevel(level, ppSurface)` | **3** | ★ 曾错写成 **13**（= `GetLevelCount()`，**不收参数**）却按 2 参数调 ⇒ stdcall 栈不平衡 ⇒ 返回地址被踩 ⇒ 崩 |
| 纹理 14 | `GetLevelDesc(pDesc)` | 2 | — |
| 纹理 13 | `GetLevelCount()` | **1** | — |
| 任意 COM 2 | `Release()` | **1** | — |

⇒ **改任何一处 D3D 调用，先照这张表数参数**（本类错误已犯 2 次：GetTextureStageState、GetRenderTarget）。

## 四条血泪教训（都踩过）

1. **用游戏给的东西之前先验证"它是我想的那种东西"** —— `rb_code()` 要求函数指针属于 `MEM_IMAGE` 且可执行；
   否则"过期设备指针"会让 `call vt[0x3a]` 跳进堆地址（dump `th18.exe.37324.dmp`：`Eip=0x90a66dc` 堆区、栈上是层 6 桩）✓
2. **`STUB_MAX` 要按桩的实际长度给**（64 是旧"跳板"尺寸 ⇒ 替换型桩 ≈110B ⇒ 会**默默失败**，现在所有失败都写日志）；
3. **采样源不能是当前渲染目标** —— 否则采到自己画的东西 ⇒ 递归（自动换面 + 日志 `[递归] …`）；
4. ★ **按键轮询不能在 `mode==0` 的提前返回之后** —— 否则 mode=0 永远收不到键 ⇒ 整个 mod 看起来"完全没效果"
   （2026-10-21 就是这么"消失"的）。现在的顺序：**在不在舞台 → 轮询按键 → 按 mode 决定画不画**。

## 2026-10-21 丢失事件

`native/route_b.c` 与本文档当时**未入库**（untracked），被某次清理从工作区删掉 ⇒ `make` 还能跑，
但重编出来的是**没有路线 B 的 DLL**（源码已重写回来；源码副本也放到了 modkit 的 `mods/th18_bgwarp-route_b.c`）。
**教训：新文件不入库就等于没有。**

## 结项小结（暂停时的一手结论，供日后恢复）

**已证实（一手）**
- 敌人的扭曲 = **ECL opcode 629** 建的 17×m 网格；屏幕空间顶点、`u=base/屏幕尺寸`、圆内 alpha `0xff`/圆外 `0x00`、
  位移 `32·t·n̂ + 8·sin φ`（φ **逐顶点**推进 x:+π/32 y:−π/64，**逐帧**推进 x:+π/16 y:+π/32）；
  消失 = `stage+0x3410 = 0` 硬停。**实跑反证**：顶点 alpha 置 0 ⇒ boss 扭曲当场消失 ⇒ 它确实在被画 ✓
- 那层**颜色（光环）是独立的 `et` 特效**：`BossCard6_et` / `BossCard6_et2`，由 `0x42d7d0(名字, 参数)` 创建，
  与 aura mesh **无关** ✓（我此前"光环=扭出来的画面"的判断是错的 ✗）
- 同一个敌人连发 629 ⇒ **销毁旧网格再建新的**（每敌只有一个槽位 `enemy+0x558c` ✓）

**已跑通但放弃的原因**
1. 自绘与引擎那份**两次采样叠加** ⇒ 无论怎么调都出"基于透明度的叠影" ✗（唯一干净解 = 退休引擎那份 + 单次绘制融合 N 个透镜）
2. 性能：`n=33/two=1` 实测 **25–37 ms/帧** ✗（采的是一个"当前渲染目标"⇒ D3D9 隐式整屏拷贝；且 `DrawPrimitiveUP` 大缓冲）
3. D3D 对象侧 API（`CreateTexture`/`GetSurfaceLevel`）槽位/参数**还没核到家** ⇒ 试"屏幕副本"直接崩了两次 ✗
   （已加静态检查 `check_d3d.py` 进 `make check`，覆盖 `__stdcall`/arity/实参个数/未知槽位 ✓）

**若恢复，按这个顺序做（一次一项）**
1. 场的公式**逐字节复刻**（逐顶点相位 + 二进制 alpha + 去羽化）⇒ 观感先对齐原版
2. 退休全部引擎网格（**只清 alpha**，保留其位移场）⇒ 读它们的 `mesh+0x14 − mesh+0x18` ⇒ 单次绘制**逐顶点融合 N 个透镜**
3. 性能：**不采当前 RT**（走引擎的屏幕 sprite，或核清纹理 API 后做一次帧末拷贝）+ 静态顶点缓冲 + 无 boss 只画一份
4. 颜色层：复用 `0x42d7d0("BossCard6_et")`（待验证能否脱离符卡状态）或自绘彩色层
