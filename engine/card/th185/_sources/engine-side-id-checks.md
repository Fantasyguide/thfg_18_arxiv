# th185 引擎侧「按卡 id 直接比较」的点（原始字节扫描）—— 素材

> **版本**：TH18.5（th185 v1.00a，Steam 脱壳镜像）。

**扫法**：原始字节找 `cmp dword ptr [reg+4], imm8`（`83 78..7f 04 imm`）—— **不经线性反汇编，避开 desync**（§39.4）。
卡名由卡表 `th185:0x4ca374` 的 id→内部名给出；「最近命名函数」取金矿 `funcs.json`（可能相距较远，仅作定位参考）。

| 站点 | id | 卡（id→名） | 最近命名函数 |
| --- | ---: | --- | --- |
| `th185:0x408415` | 85 | `BACK` | UpdateFunc::operator new |
| `th185:0x41b2e2` | 37 | `KISERU` | AbilityManager::on_draw |
| `th185:0x41d199` | 37 | `KISERU` | AbilityManager::on_draw |
| `th185:0x41d3b7` | 37 | `KISERU` | AbilityManager::on_draw |
| `th185:0x41d6a6` | 36 | `KOISHI` | BlackMarket::initialize |
| `th185:0x41da17` | 66 | `SHION` | BlackMarket::initialize |
| `th185:0x41da62` | 2 | `DANGO` | BlackMarket::initialize |
| `th185:0x41da92` | 0 | `NITORI` | BlackMarket::initialize |
| `th185:0x41e3d1` | 2 | `DANGO` | BlackMarket::on_tick |
| `th185:0x41edb8` | 2 | `DANGO` | BlackMarket::on_tick |
| `th185:0x41ee45` | 64 | `JYUNKA` | BlackMarket::on_tick |
| `th185:0x426f11` | 16 | `STAR` | StageInner::run_std |
| `th185:0x432bc1` | 0 | `NITORI` | BulletManager::operator new |
| `th185:0x4346ad` | 0 | `NITORI` | BulletManager::operator new |
| `th185:0x434f2e` | 0 | `NITORI` | BulletManager::operator new |
| `th185:0x43d112` | 0 | `NITORI` | BulletManager::operator new |
| `th185:0x43d142` | 66 | `SHION` | BulletManager::operator new |
| `th185:0x445ef5` | 0 | `NITORI` | GuiMsgVm::run |
| `th185:0x468585` | 6 | `REIMU_OP2` | decrypt_msg_string_arg |
| `th185:0x46b694` | 67 | `MAINSHOT_PU` | decrypt_msg_string_arg |
| `th185:0x48aa72` | 0 | `NITORI` | AnmManager::interrupt_tree |
| `th185:0x48f47a` | 1 | `PENDULUM2` | AnmManager::interrupt_tree |
| `th185:0x4924d8` | 0 | `NITORI` | _start |
| `th185:0x495b4b` | 1 | `PENDULUM2` | _start |
| `th185:0x49c98e` | 50 | `KANAME` | _start |
| `th185:0x49c9a9` | 52 | `AUTOBOMB` | _start |
| `th185:0x49db57` | 8 | `MARISA_OP2` | _start |
| `th185:0x49f29c` | 1 | `PENDULUM2` | _start |
| `th185:0x4a13d7` | 1 | `PENDULUM2` | _start |
| `th185:0x4a16c3` | 1 | `PENDULUM2` | _start |
