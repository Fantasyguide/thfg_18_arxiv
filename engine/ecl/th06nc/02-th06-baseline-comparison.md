# TH06 ↔ TH06NC 的 ECL 指令集对照

> **版本**：跨版本。本文出现的地址一律带版本前缀（`th06:0x4074a0` / `th06nc:0x140022060`）。

状态：指令集差分 ✅ 一手（两边跳表逐项解出 + 字节级复核）。
本文**收口**了 [`01`](01-th06-baseline-and-nc-deltas.md) 里 opcode 200/201 那两条 🟡，
其中 201 的结论是**被证伪**——它不是新弹型。

一句话：**NC 的 ECL 指令集 = TH06 原封不动 + 一条真新指令（200）+ 一条借壳但模式不存在的（201）**。
1–135 范围内，两边「已实现 / 未实现」的集合**完全相同，两个方向都是空集**。

---

## 1. 分派结构

| 项 | TH06 | TH06NC |
| --- | --- | --- |
| VM | `th06:0x4074a0`（`EclManager__run_ecl`，13572 字节） | `th06nc:0x140022060`（`EclManager__RunEcl`，27928 字节） |
| 取 opcode | `MOVSX EAX, word ptr [EDX+0x4]` | `MOVSX EAX, word ptr [R14+0x4]` |
| 索引 | `opcode − 1` | `opcode − 1` |
| 上界 | `CMP …,0x86` / `JA`（134）→ **opcode 1–135** | `CMP EAX,0xc8` / `JA`（200）→ **opcode 1–201** |
| 跳表形式 | **直接跳表** `th06:0x40a9a4`，135 项 | **字节映射 + 跳表**：`th06nc:0x140028f5c` / `th06nc:0x140028d78` |
| 去重跳转目标（**含默认出口**） | **120** | **121** |
| 真 handler 数 | 119 | **120** |
| 越界落点 | `th06:0x40a008` | `th06nc:0x140025d7d` |

两种口径下都是**只多一个**，就是 opcode 200 的 handler（`th06nc:0x140026a41`）。
opcode 201 没有自己的 handler——它和 67–75 共用 slot 60。

## 2. 指令集差分：1–135 完全一致

把两边跳表逐项解出来对，结果是：

| 差分方向 | 结果 |
| --- | --- |
| 在 TH06 有实现、NC 落默认 | **空集** |
| 在 TH06 落默认、NC 有实现 | **空集** |

唯一落默认的旧 opcode 是 **127 `DEBUGWATCH`**，而且**两边都落默认**——
原版就没实现它，不是 NC 删的。

NC 新增的 opcode 号段里：**136–199 全部落默认**（65 个 opcode 落默认 = 127 + 136–199），
只有 **200 与 201** 有实际去处。所以「NC 把 opcode 上界抬到 201」不等于「NC 加了 66 条指令」，
真正加的只有两条。

## 3. ✅ 收口：opcode 201 不是新弹型

- **① 发现**：字节映射 + 跳表解出 `opcode 201 → slot 60 → th06nc:0x140025371`，
  与 opcode 67–75 **同一个 handler**。该 handler 开头：

  ```
  140025374  LEA   R12,[RDI + 0x26c]          ; enemy->bullet_props
  14002537b  MOV   word ptr [R12 + 0x2c],AX   ; .sprite
  140025385  MOVZX EAX,word ptr [R14 + 0x4]   ; instruction->opcode
  14002538a  SUB   AX,0x43                    ; − 67
  14002538e  MOV   word ptr [R12 + 0x30],AX   ; .aim_mode
  ```

  对 opcode 201 即 `aim_mode = 201 − 67 = 134`。
- **② 推测**：134 要么是 NC 新加的弹型编号，要么根本没有对应实现。
- **③ 验证**（三条独立）：
  1. **消费侧有界**：`th06nc:0x14000f870`（TH06 `BulletManager::SpawnSingleBullet` 的对应物）
     里 `switch((ushort)param_2[0xc])` —— `0xc*4 = 0x30`，正是 `bullet_props+0x30`。
     该 switch **只有 case 0–8，没有 default**。
  2. **对抗证伪**：全库搜「把某值与 134 / `0x86` 比较」，18 处命中**全部无关**
     （循环上界 `iVar < 0x86`、某 0x86 项数组的边界检查），
     **没有任何一处特判 aim_mode == 134**。
  3. **同构佐证**：TH06 侧 `BulletManager/SpawnSingleBullet.c` 的
     `switch(bulletProps->aim_mode)` 同样是 case 0–8 无 default，
     且 switch 前把角度初始化成 `0.0`；NC 的 `th06nc:0x14000f870` 在 switch 前
     也是 `fVar25 = 0.0` 加同一条速度插值，逐行同构。
- **④ 结论**：opcode 201 **不是新弹型** ✅。它借用 67–75 的整套参数解析
  （sprite、count1/count2、rank 缩放、位置、两组角度与速度），
  但选中的 `aim_mode = 134` 没有任何分支实现，
  于是角度停在 switch 前的 `0.0`，速度取插值结果。
- **⑤ 证据**：跳表 `th06nc:0x140028f5c` / `th06nc:0x140028d78`；
  handler `th06nc:0x140025371`；消费者 `th06nc:0x14000f870`；
  `local/vendor/th06-re/src/th06_102h.exe/BulletManager/SpawnSingleBullet.c`。

> ⚠️ **还没做的**：上面全是静态推理。「实际打出来是一排朝右平飞的弹」这一步
> **没有实机验证**。样本里 201 出现 13 次，一条明显退化的指令被用了 13 次是可疑的——
> 也可能这一路根本不会走到 spawn（例如被上游的 count/flags 提前挡掉）。
> 在实机看到之前，**不要把「NC 有一条坏指令」当成定论**；已确证的只有
> 「aim_mode 134 在 spawn 的 switch 里无分支」这一条。

## 4. ✅ 收口：opcode 200 是真新指令（极坐标 shootOffset）

`th06nc:0x140026a41`，独立 handler：

```
        XMM7 = GetVarFloat(args + 4)     ; 半径
        XMM6 = GetVarFloat(args + 0)     ; 角度
140026a63  CALL cosf ; MULSS XMM0,XMM7 ; MOVSS [RDI+0xc5c],XMM0   ; shoot_offset.x
140026a77  CALL sinf ; MULSS XMM0,XMM7 ; MOVSS [RDI+0xc60],XMM0   ; .y
140026a98  MOV  dword ptr [RDI+0xc64],R12D                        ; .z = 0
```

`R12D` 由函数序言 `th06nc:0x14002213f` 的 `XOR R12D,R12D` 置零 ✅，所以 z 确实写 0。

对照 TH06 的 `SHOOTOFFSET`（opcode 81，`case 0x51`）：它把三个变量**直接**读进
`shoot_offset.x/y/z`。**NC 的 200 是它的极坐标版本**，把 (角度, 半径) 换算成直角坐标，
并强制 z = 0。这是一条货真价实的新指令 ✅。

暂定名 `SHOOTOFFSETPOLAR` 🟡 —— 数学已验，名字仍是我们的命名选择。

## 5. 顺带确认的字段偏移

| 字段 | TH06NC | 依据 |
| --- | --- | --- |
| `enemy->bullet_props` | `enemy + 0x26c` | `LEA R12,[RDI+0x26c]` |
| `bullet_props.sprite` | `+ 0x2c`（u16） | 同上 |
| `bullet_props.aim_mode` | `+ 0x30`（u16） | 写侧 `MOV [R12+0x30],AX`；读侧 `param_2[0xc]` |
| `enemy->shoot_offset` | `enemy + 0xc5c`（3×f32） | opcode 200 handler |

TH06 侧对应的社区结构体是 `BulletProps`（`types.h:2484` 起，`aim_mode` 是 `ushort`）。

## 6. 下一步

1. 🟡 **实机验证 opcode 201**：录一次含 201 的关卡（样本里在 `ecldata6/7`），
   看是不是真打出朝右平飞的弹。这条是 §3 那段警告的唯一解法。
2. ❓ **ECL loader 边界与子表结构**仍未展开（[`01`](01-th06-baseline-and-nc-deltas.md) 的旧待办）。
3. ❓ **enemy context 字段级结构**：本文只挖到 `bullet_props` / `shoot_offset` 两处，
   TH06 社区的 `Enemy` 结构体有几百个字段，值得按同样手法逐段对。
4. — 1–135 的**具体语义**本文没有逐条复核，只证明了「实现与否」的集合一致。
   别把「跳表对上了」当成「每条语义都验过了」。

## 6.5 这份对照的直接用途

既然指令集逐项一致，opcode 号就成了两个二进制之间的**共同坐标系**：
比对「每个 opcode 的 handler 调用了什么」，就能把 TH06 已知的子系统函数定位到 NC。
已按此定位并命名 16 个函数（`BulletManager` / `ItemManager` / `EffectManager` /
`EnemyEclInstr` 各族），方法、自证与已知缺陷见 [`03-opcode-crossmatch.md`](03-opcode-crossmatch.md)。

## 7. 自己看

```sh
tooling/ghidra/serve.sh th06.v1.02h 6094   # EclManager__run_ecl @0x4074a0
tooling/ghidra/serve.sh th06nc     6092    # EclManager__RunEcl  @0x140022060
```
