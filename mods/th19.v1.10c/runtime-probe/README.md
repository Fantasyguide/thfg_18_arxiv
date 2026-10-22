# runtime-probe —— TH19 v1.10c 只读运行时探针（`th19_probe2`）

> **版本**：TH19 v1.10c（`th19.exe`，imagebase `0x400000`，样本 md5 见 `games/th19.v1.10c/INDEX.md`）。
> 本文裸地址默认属该版本；引用其他版本须写成 `th18:0x…`。

一个 thcrap 插件 DLL，**只读**：每 100ms 读一次"可配置的锚点"并把几块内存打出来，写进
游戏目录的 `th19_probe2.log`。**不写游戏内存、不装 binhack、不挂钩子**。

## 它是用来回答什么的

`engine/bullet/th18/01-laser-subsystem.md` §26 已经把 **`609` 的 8 个参数落点**全部定死（一手）：

```
609/610/611/612 共用 handler = 0x5027a3
段表挂在 [ctx+0x5ac] → 元素 → +0x28
  r → 段表 +0x00（0x502999）    s → 段表 +0x04（0x5029d4）
  +0x08/+0x0c 只在 610/612 才有
```

**但静态读不下去了**：`[reg+0x5ac]` 的 75 处、段表访问器的 77 处，**100% 落在 `0x502xxx`–`0x503xxx`（handler 区）**
⇒ **运动侧不存在"读段表"的静态路径**。

所以这个探针要看的是：**运行时那块段表里 `r`/`s` 还是不是 ECL 给的值（0 / 0.1 / −0.4 / 0.05），
以及同一帧里哪几个"每帧字段"跟着变** —— 后者就是消费者与单位。

## 装法

```
<thcrap 安装目录>/bin/th19_probe2.dll     ← 插件只从 bin 加载
<游戏目录>/th19_probe2.ini                ← 可选：配锚点（见下）
```

没有 patch 目录、没有 JSON。

## 配置（`th19_probe2.ini`，与 `th19.exe` 同目录）

```ini
anchor_rva=0x4cf2e0
```

* `anchor_rva` = **相对 imagebase 的 RVA**。探针每轮读 `base+anchor_rva` 处的 dword，
  然后**三种解释都打**：
  1. `at-anchor`：锚点处本体（16 个 dword，带浮点视图）；
  2. `ctx+0x5ac` / `ctx+0x4e2c`：把该 dword 当 **ECL 上下文指针**；
  3. `enemy+0x1230+0x5ac`：把该 dword 当 **敌机指针**（一手：vtable 桩 `0x5058b0` 就是 `add ecx,0x1230`）。
* 不配（或配 0）⇒ 只打心跳，用来确认插件被加载。

## 怎么算成功

1. **thcrap 日志**里应出现 `th19_probe2.dll: initialized and active`；
2. **`th19_probe2.log`** 里应出现 `=== th19_probe2 起 … ===` 与
   `[guard] 609 handler 签名匹配，确认 th19.v1.10c。`（签名 = `0x5027a3` 前 5 字节 `6a 00 8b cf e8`）；
3. 配好 `anchor_rva` 后，每轮会打出 `[anchor] rva=… va=… *va=…` 与三组 dump。

## 判读（要点）

* **先在早苗 C3 打出来的时候**看 `ctx+0x5ac` 那 16 个 dword：应能看到**小整数**（弹型 id / 颜色）与**接近 0 的浮点**；
* `ctx+0x4e2c` 的 16 个 dword 是变换序号表；打出 C3 后应**非全 0**；
* 段表（`+0x5ac` 元素 `+0x28`）里的 `r`/`s` 若**保持** ECL 的值（`0`/`0.1`/`-0.4`/`0.05`）⇒ 说明它们只是暂存；
  若**被改写**⇒ 那一步就是消费者。

## 构建

```bash
cd mods/th19.v1.10c/runtime-probe/native
make            # 需要 llvm-mingw（i686）+ .def（导出无装饰的 thcrap_plugin_init）
```

* 产物必须是 **32 位**（thcrap 会拒绝位数不符）；
* `.def` 里 `thcrap_plugin_init` **绝不能带装饰**（`@0` 之类）—— thcrap 在 LoadLibrary 之前就扫 PE 导出表。

## ★ 装到 modkit（owner 用的是「启动TH19mod.bat」那套 modkit）

★ 那种启动器**不是直接跑 thcrap 命令行**，所以别去猜目录名 —— ★ **以 `thcrap.dll` 为准**：
★ **`th19_probe2.dll` 必须和 `thcrap.dll` 在同一个文件夹**（这就是 `<thcrap>/bin`）。

★★ **关键：这个 modkit 的启动器是"勾选式"的** —— 一手依据 [`mods/README.md`](../../../README.md)：
「`mods/*.dll` **勾选式装进 `bin/`**」、以及 `card-expand/README.md` 的「**启动器里 DLL 必勾**」。
⇒ ★ 所以**勾选框读的是 `<modkit>\mods\*.dll`**，★ 把 DLL 放 `bin\` 只是"绕过勾选"的兜底。

★ 所以给了一个安装器：★ **`install/install_probe.bat`**（纯 ASCII，cmd 的 OEM 代码页不会乱码）

```bat
install_probe.bat "D:\项目\幻想指南\18\th19_modkit"
```

★ 它只做三件事，★ **不删任何文件、不改游戏 exe**：
1. ★ 在 modkit 目录下 `dir /s /b thcrap.dll` ⇒ ★ 定位 modkit 根与 `bin`；
2. ★ **把 `th19_probe2.dll` 拷进 `<modkit>\mods\`**（★ 这样启动器的勾选列表里会出现 **`th19_probe2`**）；
   ★ 若没有 `mods\` 目录，★ 退回直接拷 `bin\`（★ 能用，但没有勾选框）；
3. ★ `dir /s /b th19.exe` ⇒ ★ 在游戏目录写 `th19_probe2.ini`（默认 `anchor_rva=0x0`，只打心跳）。

★ 装完后：★ **启动器里把 `th19_probe2` 勾上** ⇒ ★ 它会自己把 DLL 装进 `bin\`。

★ **如果它报 "thcrap.dll not found"** ⇒ ★ 那个 modkit **不是 thcrap 加载**（可能是别的注入器），
★ 那这个探针就用不上 —— ★ 告诉我，我改成"独立 DLL 注入"或换别的路子。
