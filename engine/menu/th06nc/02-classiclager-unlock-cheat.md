# TH06NC 结果页「全开档」秘籍：`classiclager` 与手柄序列

> **版本**：TH06NC（`th06nc.exe`，MD5 `b0cc28ec904e9efce2439ad857c938b5`，imagebase `0x140000000`）。
> 本文裸地址默认属该版本；引用其他版本须写成 `th16:0x…`。

状态：秘籍**存在**，代码级语义 ✅ 一手验证；端到端操作步骤 🟡（尚未实机跑过，见 §7）。
本文取代 [`01-result-screen-unlock-cheat-recon.md`](01-result-screen-unlock-cheat-recon.md) 的「未发现/可能移除」判断。

NC 没有保留 TH06 的 `HOME×5 → Q×2 → S×3`，而是换成**两条并行的新序列**，
挂在结果页的 **Lunatic** 成绩页（不是 TH06 的 Extra 页），共用同一段解锁代码。

| 条目 | 值 |
| --- | --- |
| 键盘序列 | 依次敲出 `classiclager`（12 次按下沿） |
| 手柄序列（XInput） | 按住 L3+R3 → 按 Y → 按 X → 按住 LB+RB |
| 触发位置 | 结果页 Score 菜单的第 4 项 `Lunatic — View Records` |
| 效果 | 4 个自机型 × 5 难度 × 2 档通关标志全部写 `99`，请求音效 28，置 `score.dat` 脏标志 |

---

## 1. 发现：字符串确实写进了 exe

社区流言（十六进制查看器里看到 `classiclager`）**属实**。

- **① 发现**：`.rdata` 的 `0x14030c560` 处有 13 字节 ASCII `"classiclager\0"`，
  文件偏移 `0x30b560`（`.rdata` 节 VA `0x1402c0000` / raw `0x2bf000`）。全 exe 仅此一处，
  UTF-16 无副本。Ghidra 记它有 2 处引用，实际引用函数只有 `0x14006e190` 一个。
- **② 推测**：它是某个逐字符比对的秘籍口令，而不是路径/日志字面量。
- **③ 验证**：见 §2——它前面紧邻一张 8 项解码表，且唯一引用者就是一台按下沿状态机。
- **④ 结论**：`"classiclager"` 是 TH06NC 的解锁口令 ✅。
- **⑤ 证据**：`0x14030c560`；`local/th06nc/state/all-strings.tsv` 第 1424 行。

它在 `.rdata` 里的邻居也说明了它属于结果页：紧挨着 `"Nanashi "`（成绩页默认名）
与 `"data/result00.anm"`–`"data/result03.anm"`。

## 2. 口令解码表：`0x14030c520`

`0x14030c520` 起是 8 项 × 8 字节的 `{ int ch; int dik; }`，正好在口令字符串之前收尾（`0x14030c560`）。

| bit | `ch` | `dik` | 扫描码常量 |
| --- | --- | --- | --- |
| 0 | `'c'` `0x63` | `0x2e` | `DIK_C` ✅ |
| 1 | `'l'` `0x6c` | `0x26` | `DIK_L` ✅ |
| 2 | `'a'` `0x61` | `0x1e` | `DIK_A` ✅ |
| 3 | `'s'` `0x73` | `0x1f` | `DIK_S` ✅ |
| 4 | `'i'` `0x69` | `0x17` | `DIK_I` ✅ |
| 5 | `'g'` `0x67` | `0x22` | `DIK_G` ✅ |
| 6 | `'e'` `0x65` | `0x12` | `DIK_E` ✅ |
| 7 | `'r'` `0x72` | `0x13` | `DIK_R` ✅ |

⚠️ 这些数值是 **PS/2 set-1 扫描码**；`DIK_*` 只是同值常量的通俗写法，**不代表 NC 用了 DirectInput**——exe 里根本没有 DirectInput（见 §10）。

- **① 发现**：8 项，第二个 dword 全部落在字母的 PS/2 set-1 扫描码上，且 8 个字母恰好是
  `classiclager` 去重后的全集（`c l a s i g e r`）。
- **② 推测**：这是「秘籍用到的键」表，bit 序即表序。
- **③ 验证**：`0x14006e190` 前 8 条 `MOVZX EAX,byte ptr [0x140c21e??]` 的地址减去
  `0x140c21e60` 依次得 `0x2e 0x26 0x1e 0x1f 0x17 0x22 0x12 0x13`——**与表中 `dik` 列逐项相同**。
  编译器把 `g_KeyRemap[kCheatKeys[i].dik]` 的 8 次迭代常量折叠展开了，位图循环被展开、
  字符查找循环没有，两者共用同一张表。
- **④ 结论**：`0x14030c520` = 秘籍按键表，bit *i* ↔ 第 *i* 项字母 ✅。
- **⑤ 证据**：`0x14030c520`、`0x14006e190`（`0x14006e194`–`0x14006e246` 的 8 组 `MOVZX`/`BTS`）。

### 为什么是「字母」而不是「键位」

`0x140c21e60` 是一张 256 字节的**布局感知扫描码重映射表**：`0x140012be0` 用
`GetKeyboardLayout(0)` 取当前 HKL，先把表填成恒等映射，再对 `0x1403081c0` 里的 0x25 个扫描码
跑 `MapVirtualKeyExW(vsc, 3, hkl)` 反查回当前布局的扫描码写回表内。HKL 变了会重建。
秘籍读的是 `g_KeyState[g_KeyRemap[DIK_C]]`（`0x140c21f70` 是 256 字节键按下状态数组），
**所以在 AZERTY / QWERTZ 上也是敲字母 `classiclager`，不是敲 QWERTY 的物理位置** ✅。

## 3. 键盘状态机 `0x14006e190`

结果页对象里两个字段：`+0x4430` = 当前步号（int），`+0x4434` = 上一帧的 8 位字母位图（uint）。

```
cur  = 8 个字母的按下位图
edge = ~prev & cur ;  prev = cur
if (edge == 0) return 0                     // 非秘籍键完全不影响，不会打断
want = table.index_of("classiclager"[step]) // 0..7
if (edge & (1 << want)) {
    if (++step >= 12) { 解锁(); step = 0; return 1 }
} else {
    step = (edge & (1 << index_of('c'))) ? 1 : 0   // 敲错就归零，敲 'c' 则重新起头
}
```

- 长度闸门是 `CMP EAX,0xc` / `JL`，即需要 **12 次按下沿**，与 `"classiclager"` 长度一致 ✅。
- `step` 3、4 两位都是 `'s'`，必须**松开再按**第二次（逐位按下沿检测）✅。
- 同一帧同时按下多个字母不影响推进，只要期望字母在其中 ✅。
- **不需要按住任何修饰键** ❌与 TH06 不同：TH06 要求全程按住 `FOCUS` 或 `SKIP`。
- 秘籍字母以外的按键既不推进也不重置（`edge == 0` 直接返回）✅。

## 4. 手柄状态机（`0x14006e020` 内联）

⚠️ **`0x140c21e58` 不是键盘位图，是手柄按钮位图**，只有 6 个 bit。
它由 `0x1400127d0` 从 `0x1400a9fd0(1, &pad)` 的结果拼出。

`0x1400a9fd0` 是 XInput 封装：exe 在运行时 `LoadLibrary` 依次探测
`xinput1_4.dll` / `xinput1_3.dll` / `xinput9_1_0.dll` 并取 `XInputGetState` / `XInputSetState`
（**不在静态导入表里**，详见 §8），从每手柄 `0x530` 字节缓存里拷 26 字节
= **16 个按钮位展开 + 2 扳机 + 4 摇杆轴**。

按钮索引 = XInput `wButtons` 的位序，三条独立佐证：

| 佐证 | 内容 |
| --- | --- |
| D-pad | `0x1400127d0` 把 `pad[0..3]` 映射到上/下/左/右 = `DPAD_UP/DOWN/LEFT/RIGHT`（bit 0–3）✅ |
| 确定/取消 | 用 `pad[12]`/`pad[13]` 且带交换标志 `0x140c21dba`（日式 A/B 互换）= `A`/`B`（bit 12/13）✅ |
| 扳机阈值 | `0x1d < trigger` 即 `>= 30` = `XINPUT_GAMEPAD_TRIGGER_THRESHOLD` ✅ |

于是 `0x140c21e58` 的 6 个 bit：

| bit | `pad[]` | XInput |
| --- | --- | --- |
| 0 | `pad[6]` | `LEFT_THUMB`（L3） |
| 1 | `pad[7]` | `RIGHT_THUMB`（R3） |
| 2 | `pad[14]` | `X` |
| 3 | `pad[15]` | `Y` |
| 4 | `pad[8]` | `LEFT_SHOULDER`（LB） |
| 5 | `pad[9]` | `RIGHT_SHOULDER`（RB） |

状态字段：`+0x4448` = 步号，`+0x444c` = 上一帧位图。四步（`edge = ~prev & cur`）：

| 步 | 推进条件 | 打断条件 |
| --- | --- | --- |
| 0 → 1 | `cur & 0x03 == 0x03` 且 `edge & 0x03`：**同时按住 L3+R3** | — |
| 1 → 2 | `edge & 0x08`：按 **Y** | `edge & 0x34`（X / LB / RB） |
| 2 → 3 | `edge & 0x04`：按 **X** | `edge & 0x38`（Y / LB / RB） |
| 3 → 触发 | `cur & 0x30 == 0x30` 且 `edge & 0x30`：**同时按住 LB+RB** | `edge & 0x0c`（X / Y） |

步号取到非 0–3 的值时直接归零。没接手柄时 `0x1400a9fd0` 返回 `0xffffffff`，
`0x140c21e58` 不更新，该序列静默失效 ✅。

## 5. 触发条件：Lunatic 成绩页，不是 Extra

- **① 发现**：`0x14006e3a0`（结果页每帧 tick）在 `0x14006e3d8` 读 `[RCX + 0x9e94]` 作 switch 索引，
  上界 `CMP EDX,0x11`，跳表在 `0x140072824`（18 条 RVA）。解出的目标：

  | state | 目标 | 含义 |
  | --- | --- | --- |
  | 3 / 4 / 5 / 7 | `0x14006fac3` | 共享成绩页处理 |
  | **6** | `0x14006fab4` | `MOV RCX,RSI` / `CALL 0x14006e020` → **再落入共享处理** |

- **② 推测**：state = 结果页菜单光标 + 3；若菜单顺序为 Easy…Extra，则 state 6 = Lunatic。
- **③ 验证**（三重）：
  1. **不信反编译器的 case 标签**，直接按 `0x140072824` 的 RVA 表复算——只有 state 6 带 `CALL`。
  2. `0x14006e3a0` 的菜单分支里，光标 0–4 一律执行 `state = cursor + 3`
     （另有第 6 项 → state 8、第 7 项 → state 2），与 TH06 `ResultScreen/OnUpdate.c:237/250/262` 同构。
  3. **贴图实证**：`result00.anm` 的 script1–script7 用 sprite1–sprite7，y 依次
     101/146/191/236/281/326/371（自上而下等距 45px）。把 sprite1–5 从
     `data/result/result00.png` 切出来读，正是 `Easy` / `Normal` / `Hard` / `Lunatic` / `Extra`
     的 `— View Records` 条目。
- **④ 结论**：state 3=Easy、4=Normal、5=Hard、**6=Lunatic**、7=Extra；
  秘籍只在 **Lunatic 成绩页**响应 ✅。
- **⑤ 证据**：`0x14006e3a0`、跳表 `0x140072824`、`0x14006fab4`、`0x14006fac3`；
  `local/th06nc/decoded/anm/th06TL/result00.anm.txt`；
  `local/th06nc/decoded/textures/png/th06TL/result00_en.png` 的 sprite1–5 区域。

> 这条正是 [`01`](01-result-screen-unlock-cheat-recon.md) 没找到秘籍的原因：
> 那份笔记按 TH06 的先验只盯 case 7（Extra），而 NC 把它挪到了 case 6。

## 6. 解锁效果

两条秘籍走同一段代码（`0x14006e190` 与 `0x14006e020` 各有一份等价内联）：

1. **写通关表**：以 `0x1404ff100` 为起点、步长 `0x18`、共 4 轮，每轮写 10 字节 `99`
   （`0x1404ff100` / `0x1404ff118` / `0x1404ff130` / `0x1404ff148`）。
2. **请求音效 28**：若 `0x140509694 == 0`，在 `0x140509670` 的 3 项 int 队列里找空位（值为负）
   写入 `0x1c`；已有 `0x1c` 则跳过。
3. **清零步号**，并置结果页 `+0x9e99 = 1`。

- **① 发现**：`0x140074470`（`score.dat` 写盘）里 `0x1404ff0f4` / `0x1404ff10c` / `0x1404ff124` /
  `0x1404ff13c` 各被写入 `0x44524c43` = `'CLRD'`，随后 `0x180018`、版本 `0x15`。
- **② 推测**：`0x1404ff0f4` 是 4 条 `CLRD` 记录的数组，步长 `0x18`；秘籍写的是每条 `+0x0c` 的数据区。
- **③ 验证**：记录头 12 字节（magic 4 + `0x180018` 4 + 版本 4）之后正是 `+0x0c`；
  `0x1404ff0f4 + 0x0c = 0x1404ff100`，与秘籍的 4 个写入基址逐一吻合，10 字节 = 5 难度
  「带续关」+ 5 难度「不续关」。TH06 社区源码 `ResultScreen/OnUpdate.c:153-158` 是
  `local_10 * 0x18 + 0x69ccdc + local_14` 与 `+ 0x69cce1 + local_14`，双重循环 4×5——**结构完全一致**。
- **④ 结论**：`0x1404ff0f4` = `CLRD` 记录数组（4 条 × `0x18`），`+0x0c` 起 10 字节为通关标志；
  秘籍把 40 字节全写 `99` ✅。
- **⑤ 证据**：`0x140074470`、`0x1404ff0f4`、`0x14006e190`（`0x14006e2dc`–`0x14006e30c` 的填充循环）；
  `local/vendor/th06-re/src/th06_102h.exe/ResultScreen/OnUpdate.c`。

**音效 id 沿用 TH06 编号** ✅：同一个 `0x14006e3a0` 里菜单确认用 `10`、返回用 `0xb`、秘籍用 `0x1c`，
对上社区 `types.h:2408/2409/2426` 的 `SOUND_SELECT=10` / `SOUND_BACK=11` / `SOUND_1UP=28`。
三个 id 同时吻合，也就同时坐实了 `0x140509670` 是音效请求队列。

**会落盘** ✅：`0x140074470` 的整段写盘逻辑被 `if (*(char *)(param_1 + 0x9e99) != '\0')` 包住，
而秘籍成功后恰好置该字节为 1（`0x140079c70` 在正常结算时也置它）。所以解锁写进 `score.dat`，重启后仍在。

## 7. 与 TH06 的差异，以及还没验的部分

| 项 | TH06 1.02h | TH06NC |
| --- | --- | --- |
| 触发页 | Extra 成绩页（state 7） | **Lunatic 成绩页（state 6）** |
| 序列 | `HOME×5 → Q×2 → S×3`，全程按住 `FOCUS` 或 `SKIP` | 敲 `classiclager`；或手柄 L3+R3 → Y → X → LB+RB |
| 修饰键 | 必须按住 | 不需要 |
| 手柄 | 无 | 有独立序列（XInput） |
| 效果 | 4×5×2 通关标志写 99 + `SOUND_1UP` | 相同 |

TH06 原序列在 NC 里**不可用** ✅：NC 的逻辑按钮位图（`0x1400127d0` 生成）根本没有 `HOME` 位，
且 `Q` 落在 bit 11、`S` 落在 bit 12、`R` 落在 bit 14，与 TH06 `Controller.hpp` 的
`Q=1<<9` / `S=1<<10` / `HOME=1<<11` 完全不同。

**还没做的**（不要把下面任何一条写成已验证）：

1. 🟡 **实机验证**：本文全部结论来自静态一手反汇编，尚未在游戏里跑过。
   验收标准三者同时成立：Lunatic 成绩页敲完 12 个字母后听到 1UP 音效、
   `CLRD` 40 字节变 `99`、退出重进后 `score.dat` 里仍然是 `99`。先备份 `score.dat`。
2. ❓ **`0x140509694` 的确切含义**：当前只知道它非 0 时跳过音效入队，推测是静音/音效不可用标志。
3. ❓ **是否还有第三条入口**：本文只覆盖结果页。全局的「写 `99` / 写 `CLRD` / 请求音效 28」
   锚点尚未做穷尽搜索，不能断言 NC 只有这两条。
4. ❓ **`0x1403081c0` 的 0x25 个扫描码**未逐项列出；重映射表对非拉丁布局的实际行为未测。

## 8. 输入后端（决定这条秘籍在什么设备上有效）

一手取自 PE 导入表与字符串表：

| 事项 | 证据 |
| --- | --- |
| 手柄**只有 XInput** | 字符串 `XInputGetState` / `XInputSetState` + 宽字符串 `xinput1_4.dll` / `xinput1_3.dll` / `xinput9_1_0.dll`；**不在静态导入表里**，是运行时 `LoadLibrary` 依次探测 ✅ |
| **没有 DirectInput** | 导入表与字符串表均无 `dinput8` / `IDirectInput`（唯一的 `dinput` 疑似命中是 `AttachThreadInput`）✅ |
| 键盘走 Win32 | `GetAsyncKeyState` / `GetKeyboardState` + `GetKeyboardLayout` / `MapVirtualKeyExW` ✅ |
| 无手柄名数据库 | 字符串表里没有 DualSense / DualShock / Switch Pro 等型号名，也没有 SDL 映射库那类条目 ✅ |
| 静态导入的第三方 DLL | 只有 `steam_api64.dll`（其余全是 CRT / KERNEL32 / USER32 / ole32）✅ |

**推论 🟡**：游戏本体只会说 XInput，所以 PlayStation 手柄是靠 **Steam Input 翻译成 XInput** 才能用的——
这与 Steam 商店给它打的分类一致（`18` Partial Controller Support、`55/56` DualShock 有线/蓝牙、
`57/58` DualSense 有线/蓝牙、`75` Keyboard Only Option）。脱离 Steam 用非 XInput 手柄需要外部包装器。
标 🟡 是因为「靠 Steam Input」这一步没有在本机实测过。

**对秘籍的实际影响** ✅：`0x1400127d0` 里那 6 个 bit 用的是**写死的按钮下标**（6/7/8/9/14/15），
不像射击/炸弹/低速/跳过那样过 `0x140c21da4` 一族的可配置下标。
**所以游戏内 Key Config 改键不会影响手柄秘籍，按的永远是物理按键位。**

## 9. 相关地址一览

| 地址 | 含义 |
| --- | --- |
| `0x14030c560` | `"classiclager"` 口令字符串 |
| `0x14030c520` | 秘籍按键表，8 × `{ int ch; int dik; }` |
| `0x14006e190` | 键盘秘籍状态机 |
| `0x14006e020` | 手柄秘籍状态机 + 键盘状态机的唯一调用者 |
| `0x14006e3a0` | 结果页每帧 tick，state switch |
| `0x140072824` | 结果页 state 跳表（18 条 RVA） |
| `0x14006fab4` | state 6 入口（带 `CALL 0x14006e020`） |
| `0x14006fac3` | state 3/4/5/6/7 共享成绩页处理 |
| `0x140012be0` | 每帧输入采集；重建扫描码重映射表 |
| `0x1400127d0` | 合成逻辑按钮位图；写 `0x140c21e58` |
| `0x1400a9fd0` | XInput 手柄状态取用封装（26 字节归一化结构） |
| `0x140074470` | `score.dat` 写盘，受 `+0x9e99` 脏标志控制 |
| `0x140079c70` | 结果页 chain 驱动；正常结算时置 `+0x9e99` |
| `0x140c21e60` | 256 字节布局感知扫描码重映射表 |
| `0x140c21f70` | 256 字节键按下状态数组 |
| `0x140c21e58` | 手柄按钮位图（6 bit） |
| `0x140c21dba` | 手柄确定/取消交换标志 |
| `0x1403081c0` | 参与重映射的 0x25 个扫描码 |
| `0x1404ff0f4` | `CLRD` 记录数组基址（4 × `0x18`） |
| `0x1404ff100` | `CLRD[0]` 数据区（10 字节通关标志） |
| `0x140509670` | 音效请求队列（3 × int，负值为空） |
| `0x140509694` | 非 0 时跳过音效入队 |

结果页对象字段（相对 `0x14006e3a0` 的 `param_1`）：

| 偏移 | 含义 |
| --- | --- |
| `+0x00a8` | 进入成绩页时保存的光标（TH06 的 `diffPlayed`） |
| `+0x4430` | `classiclager` 步号 |
| `+0x4434` | 上一帧字母位图 |
| `+0x4448` | 手柄秘籍步号 |
| `+0x444c` | 上一帧手柄位图 |
| `+0x9e7c` | 菜单光标 |
| `+0x9e80` | 保存的光标 |
| `+0x9e94` | 结果页 state（switch 索引） |
| `+0x9e98` | 每 state 一次的初始化标志 |
| `+0x9e99` | `score.dat` 脏标志 |

## 10. 复现

```sh
source tooling/env.sh
python tooling/ghidra/apply_th06nc_menu.py --dry-run
python tooling/ghidra/export_html.py th06nc
```

字符串与解码表可以完全脱离 Ghidra 复核：

```sh
python3 - <<'EOF'
import struct
d = open('local/th06nc/th06nc.exe', 'rb').read()
off = 0x30b520                      # .rdata: VA 0x14030c520
for i in range(8):
    ch, dik = struct.unpack_from('<ii', d, off + i * 8)
    print(i, chr(ch), hex(dik))
print(d[off + 0x40:off + 0x4d])     # b'classiclager\x00'
EOF
```
