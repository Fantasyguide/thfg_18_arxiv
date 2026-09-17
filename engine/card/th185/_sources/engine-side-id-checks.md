# th185 引擎侧「按卡 id 直接比较」的点（**经函数头重同步验证**）—— 素材

> **版本**：TH18.5（th185 v1.00a，Steam 脱壳镜像）。

**方法**：原始字节找 `83 78..7f 04 imm8`，然后①排除前一字节是 `66/67/f2/f3` 前缀的假命中；
②从**最近的已知函数头**（金矿 `funcs.json`）重同步反汇编，要求该地址确实是一条 `cmp dword ptr [reg+4], imm8`、长度 4。
⇒ 未通过②的候选**一律丢弃**（它们可能是数据或前缀指令的一部分）。

| 站点 | id | 卡（id→名） | 最近命名函数 |
| --- | ---: | --- | --- |
| `th185:0x41d6a6` | 36 | `KOISHI` | BlackMarket::initialize |
| `th185:0x41da17` | 66 | `SHION` | BlackMarket::initialize |
| `th185:0x41da62` | 2 | `DANGO` | BlackMarket::initialize |
| `th185:0x41da92` | 0 | `NITORI` | BlackMarket::initialize |
| `th185:0x41e3d1` | 2 | `DANGO` | BlackMarket::on_tick |
| `th185:0x41edb8` | 2 | `DANGO` | BlackMarket::on_tick |
| `th185:0x41ee45` | 64 | `JYUNKA` | BlackMarket::on_tick |
| `th185:0x445ef5` | 0 | `NITORI` | GuiMsgVm::run |
| `th185:0x48aa72` | 0 | `NITORI` | AnmManager::interrupt_tree |
