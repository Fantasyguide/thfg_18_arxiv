# tooling/crashdump —— 崩溃取证工具（版本无关）
> **版本**：跨版本（工具本身与版本无关；示例用 TH18 v1.00a）。

2026-09-17 定位 TH18「跳进 thcrap 跳板池未写区」那条崩溃时造的工具，**TH08/TH09/TH12 可直接复用**。

```bash
python3 tooling/crashdump/dmp.py <file.dmp> [--frames 24] [--symbols <funcs.json>]
```

它一次给出：异常码/访问地址/寄存器、`Eip` 落在哪个模块、**崩点那一页有没有被 dump 捕获（捕获了就直接打字节）**、
模块表、以及 **ebp 链**（往上两级就能看出是从哪个引擎循环里跳出去的）。解析配方（各流的偏移）写在文件头注释里，
改之前先读它。

## 判读心法（这套是那天真正定责用的）

1. **`Eip` 不在任何模块里** ⇒ 不是正常代码，多半是「跳到 thcrap 给断点站点分配的跳板池」或「已释放的代码页」；
2. **崩点那页全 0** ⇒ 执行的是 `00 00` = `add byte ptr [eax], al`；若此时 `Eax = 1`，就正好解释
   「`0xC0000005` **写** `0x1`」这种看起来莫名其妙的签名（那天五次 dump 全是这一个签名）；
3. **ebp 链** ⇒ 从哪个循环跳出去的（我们那次就靠它把范围夹到 `ItemManager::on_tick` 内部）；
4. **偏移是不是恒定的**：换配置后偏移**按 0x10/条**平移 ⇒ 说明坏指针是「池布局相对」的（那天由此确认是站点预算问题）。

## 配套的两件在 DLL 里的取证（源码即文档，改前读注释）

- **站点/节点轨迹**：`mods/th18.v1.00a/card-expand/native/sdk.c` 的 `ce_site_trail()` / `ce_chain_node_guard()`
  —— 每个 handler 入口与每个更新链节点各打一行（「与上次不同才打」），而 `ce_log` **逐行 flush**
  ⇒ **崩前最后一行就是现场**。⚠ 新加的 handler 别忘了打 `ce_site_trail()`，否则它不出现在轨迹里
  （2026-09-17 就因为同僚新加的 `BP_ce_enemy_damage` 没打，差点把它排除掉）；
- **跳板池页 dump**：`ce_mem_map_dump()` —— 把 `0x03c00000..0x03e00000` 里 `PAGE_EXECUTE` 的页整段打 hex
  ⇒ 离线反汇编就能看出「池里哪些格写过、零尾巴从哪开始」。

## 另外两个好用的

- `python3 tooling/xref.py callers 0x401530` —— 扫 `e8 rel32` 找调用者并标出所在函数（那天用它把
  「更新链上到底有谁」89 处注册者全列了出来）；`callees` / `name` 同源。
- `engine/_shared/frame-loop.md` §4 —— TH18 更新链（`UpdateFuncRegistry`）的一手机制：节点布局、返回码表、注册者清单。
