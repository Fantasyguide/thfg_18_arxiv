# TH18 自机本体染色（`player+0x538 / +0x53c`）与三个「移速」字段

> **版本**：TH18 v1.00a（`th18.exe`，imagebase `0x400000`）。本文裸地址默认属该版本。
> **可信度**：字段/常量/写点**全部逐指令反汇编 ✅**；"闪黄"的**观感**由 owner 实跑确认 ✅。
> **动机**：2026-10-24 实跑报「黑桃J（卡 59）拿走之后自机本体闪黄，黑桃10 不闪」。
> 结论是**引擎自带**的"加速中"视觉，不是任何一张卡在染色。

## 0. 一句话

**TH18 用 `player+0x53c`（`zPlayer.vm` 的 `color_2`）表达自机状态**：决死窗口蓝、危险红、
**加速中黄（`0xffffff00`，每 8 帧亮 4 帧 = 闪）**。触发条件是 **`player+0x477ec > 阈值`** ——
**谁把"移速倍率"抬起来，谁就会让引擎自己把自机闪成黄色**。

## 1. 两个颜色字段

`zPlayer.vm` = `player+0x14`（`engine.h` 的 `CE_PLAYER_VM`）⇒ 颜色字段绝对地址：

| 字段 | 绝对地址 | 说明 |
| --- | --- | --- |
| `color_1` | **`player+0x538`** | 主色（平时 `0xffffffff`）|
| `color_2` | **`player+0x53c`** | **状态色**：引擎按状态覆盖这一格 |

## 2. 引擎的状态染色（一手：`0x45c620`–`0x45c702`，`Player::on_tick` 尾部）

```
45c63a:  mov  0x638(%edi),%eax        ; 帧计数
45c640:  cmp  0x634(%edi),%eax
45c646:  je   0x45c674                ; 不等于 ⇒ 跳过
45c649:  idiv %esi
45c64b:  test %edx,%edx
45c64d:  jne  0x45c674
45c65a:  movl $0xff0000ff,0x53c(%edi) ; ★ 决死窗口 = 蓝
...
45c690:  testb $0x20,0x4779c(%edi)    ; 某个状态位
45c69d:  je   0x45c6c2
45c6b6:  movl $0xffff0000,0x53c(%edi) ; ★ 危险 = 红
...
45c6c2:  movss 0x477ec(%edi),%xmm0    ; ★★ 读「移速倍率」
45c6ca:  comiss %xmm3,%xmm0           ; 与阈值比
45c6cd:  jbe  0x45c702                ; 不超过 ⇒ 不上色
45c6cf:  mov  0x638(%edi),%eax        ; 帧计数
45c6d5:  and  $0x80000007,%eax        ; 帧 % 8
45c6e1:  cmp  $0x4,%eax
45c6e4:  jge  0x45c702                ; 后 4 帧不上色
45c6e6:  movl $0xffffff00,0x53c(%edi) ; ★★ 加速中 = 黄（每 8 帧亮 4 帧 = 闪）
...
45c702:  movl $0x3f800000,0x477ec(%edi) ; ★ 同一函数末尾把倍率复位 1.0
```

⇒ **"闪"不是谁在反复写颜色**，是**引擎自己按 `帧 % 8 < 4` 上/不上黄**。

## 3. 三个「移速」字段（别混）

| 字段 | 名字 | 谁写 | 会不会触发黄闪 |
| --- | --- | --- | --- |
| **`player+0x477b4`** | 直线**步长**（1/128 px） | **原生卡 29 疾风木屐**：`0x40ca81 movl $0x400`（= 1024/128 = 8.00 px/帧）| ★ **不会** ✓ |
| **`player+0x477bc`** | 斜向**步长** | 同上 `0x40ca8b movl $0x2d4`（= 5.66 px/帧）| ★ **不会** ✓ |
| **`player+0x477ec`** | **移速倍率**（`Player::move` 最后一步 `mulss`，`0x45b5b6`/`0x45b5db`）| 卡 59 黑桃J（`= 1.1`）；引擎 `0x45c702` 每帧复位 1.0 | ★★ **会** ✗ |
| `player+0x477f0` | 基础速度 | 引擎每帧从 `0x5217dc` 复制（`0x45c717`）| 不会 |

⇒ **原生卡 29「疾风木屐」是安全的** —— 它写的是**步长**（`+0x477b4/+0x477bc`），
**从来没碰过倍率 `+0x477ec`**（一手：`objdump -d --start-address=0x40ca70 --stop-address=0x40cab0`）。

## 4. 后果与修法

- ★ **任何把 `+0x477ec` 抬到阈值以上的卡，都会让引擎把自机闪成黄色**（不是 bug，是引擎的视觉）。
- ★ 想保留加速又想不闪：在 **`on_item_frame`（优先级 `0x1e`，晚于 `Player::on_tick` 的 `0x17`）**
  里把 `color_2` 从 `0xffffff00` **顶回 `0xffffffff`**，**只顶这一种**（蓝/红是正常提示，别碰）。
- ★ 卡 59 `cards/sj.c` 已按此实现（`on_item_frame` + 条件 `== 0xffffff00`）。

## 5. 复现命令

```bash
# 状态染色整段
objdump -d --start-address=0x45c620 --stop-address=0x45c710 local/th18.v1.00a/th18.exe
# 疾风木屐写步长（不是倍率）
objdump -d --start-address=0x40ca70 --stop-address=0x40cab0 local/th18.v1.00a/th18.exe
# 倍率被读进位移
objdump -d --start-address=0x45b5a0 --stop-address=0x45b5f0 local/th18.v1.00a/th18.exe
```
