# 2026-09-24 崩溃取证：启动期 `0xC0000005`（崩在 ntdll 堆管理里）

> **版本**：TH18 v1.00a（`th18.exe`，imagebase `0x400000`）。
> **谁写的**：110/111 这一条线。**为什么写**：用户 2026-09-24 报「根本连主菜单都没进去就崩溃了」，
> dump = `<modkit>/crashdumps/th18.exe.48448.dmp`。**结论**：签名与「我们 DLL 的代码」无关
> （栈上没有本 DLL 一帧），最可能是**部署脚本正在重写 1.1 GB 素材时启动游戏**读到了半截文件；
> 处置见 §4（同步脚本已改成「未变就一个字节都不写」+ 游戏在跑直接拒绝）。

## 1. 签名（`tooling/crashdump/dmp.py` 原样读数）

```
异常   : code=0xc0000005 addr=0x77ae0812 params=['0x0', '0x291a40']
寄存器 : eip=77ae0812 esp=07defc2c ebp=07defd54 eax=18223170 ebx=18221250
         ecx=00291a40 edx=0d2dceb0 esi=18223168 edi=00800000
EIP 在 : C:\WINDOWS\System32\ntdll.dll+50812
访问   : 读 @ 0x291a40
崩点页 : 0x77ae0792..0x77ae0892（已被 dump 捕获）
```

- 崩点那几字节（从 `fault-32` 起）＝
  `… 8b 4e 0c 89 4d ac | **8b 09** | 8b 52 04 3b ca …`
  ⇒ `mov ecx,[esi+0xc]` → `mov [ebp-0x54],ecx` → **`mov ecx,[ecx]`**（`8b 09`）而 `ecx = 0x291a40`
  ⇒ **坏指针来自 `[esi+0xc]`（`esi = 0x18223168`）**，读它就 `0xC0000005`。
- `edi = 0x00800000`（8 MB）像**堆基址**，`[edi+0x74]` 之类随后被当堆字段用 ⇒ 崩点在**堆管理内部**
  （`RtlpAllocate/Free` 那一族在走自己的链表/树）。
- **ebp 链全是异常派发链**（`ntdll+0x92e12` / `ntdll+0x3ecca` / `th18.exe+0x9d08a` /
  `+0x91a57`(`_UnwindNestedFrames`) / `+0x6e476`(`__endthreadex`) / `+0x92d6a`(`FindHandler`) /
  `kernel32+0x15d49`）⇒ **它给出的是 SEH 在展开时的帧，不是原始调用链**，别拿它当调用栈读。
- **栈扫描：整个栈上没有一条落进 `mod_th18_card_expand.dll`（base `0x5ded0000`，size `0xf5000`）的返回地址**
  （命中 0）⇒ 崩的那一刻我们的代码不在调用链上。

## 2. 与旧 dump 的对比（同一台机、同一 mod）

| dump | 签名 | 判读 |
| --- | --- | --- |
| `th18.exe.104552.dmp`（09-19 22:03）| `0xC0000005` **写 0x1**，`eip=0x3c205fa`（不在任何模块）| **已知**：thcrap 跳板池未写区（`tooling/crashdump/README.md` 记的那一条）|
| `th18.exe.108972.dmp`（09-18 01:06）| `0xC0000005` 读 0x0，`eip=0x4013ba`（exe 内）| 另一类（exe 内空指针）|
| **`th18.exe.48448.dmp`（09-24 01:57）** | `0xC0000005` **读 `0x291a40`**，`eip=ntdll+0x50812` | **新签名**：堆元数据被写坏（不是跳板池、不是我们 DLL 的代码）|

## 3. 时间线（关键证据：启动落在部署脚本的写盘窗口里）

| 时刻 | 事件 |
| --- | --- |
| 01:55:13 | `local/sync_modkit_dll.sh` 写完 `<modkit>/mods/th18_card_expand.dll`，**随后开始无脑重写 3×383 MB 的 bgm**（旧版没有任何「同 md5 就跳过」）|
| 01:57:31 | 日志重开（用户**新起了一次游戏**）|
| 01:57:42 | 崩溃 dump 落盘 |

⇒ 崩溃发生在「部署脚本还在往同一个 modkit 目录狂写 1.1 GB」的过程中。游戏/thcrap 从这些目录里
**边写边读**（DLL、anm、素材），读到半截文件就能解释「堆被写坏」这种签名。

## 4. 处置（已做）

1. **同步脚本三处加固**（`local/sync_modkit_dll.sh`，本机文件）：
   - **游戏在跑直接拒绝**（`tasklist.exe` 里见 `th18.exe` ⇒ `exit 1`，除非 `SKIP_RUNNING_CHECK=1`）；
   - DLL 逐份**同 md5 就跳过**；
   - bgm 判据从「逐文件 md5 全量比对（读 2×1.1 GB，DrvFs 上要几分钟）」换成**尺寸比对**，
     未变则**一个字节都不写**并打印一行说明。
   ⇒ 实测：一次 `sync_modkit_dll.sh` 从**几十秒~几分钟的重写**降到 **5.5 s 且零写盘**（全部一致时）。
2. **对照实跑**：同步收尾后，用 `autotest/run_autotest.bat --scenario z-intercept --launch`
   （Windows 侧 runner，见 `mods/.../native/tools/autoplay_win.py`）启动游戏 —— **正常进主菜单**，
   无新 dump。（该 runner 自身有个小 bug：菜单序章里键名写成 `"Z"` 而 `VK` 表里是 `"SHOT"`，
   走到注入那一步会 `KeyError`；不影响「能不能启动」，本轮没改它。）
3. 旧 dump（`104552` / `108972`）**保持不动**，本次 dump 也留着（同名签名再出现时用来对比偏移）。

## 5. 还剩下的不确定（别当已结案）

- 这一条**不是**「已定位到某个我们的 bug」：没有复现步骤、崩点在我们代码之外。
  如果再次出现（尤其**没在同步/发布的同时**启动），请把新 dump 一起给我 ——
  判据同上：看 `eip` 在不在 ntdll、坏指针是不是又一次很小的地址（`0x29xxxx` 这种）。
- 启动期真正跑我们代码的只有：SDK 的表格搬迁 / 站点校验 / 卡 ctor；
  若要彻底排除，下一步可以拿**不带本批改动**的 DLL（`git` 上一个发布批的 `dist/bin/th18_card_expand.dll`）
  连跑几次做对照。
