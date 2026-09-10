# engine/ — 知识层

**子系统为主，版本为辅。** 每个子系统一个目录：

```
engine/<子系统>/
  OVERVIEW.md      门面:跨版本模型 + 断言×版本矩阵(每格链回一手文档)
  <版本>/          一手证据:地址、偏移、反编译依据、可信度分级
```

`_shared/` 是例外：装真正跨子系统的引擎知识（主循环、数学/PRNG、归档格式、社区来源）。

## 为什么是这个轴

版本为主会让共通部分在各作之间重复，长出互相打架的说法；
子系统为主则强迫你在落笔时回答「这条是共通的还是本作特有的」——
矩阵就是这个回答的载体。**新作的特有机制也是子系统**（TH18 卡牌 → `card/`），
矩阵里其他版本填 ❌ 即可，不要再开版本目录。

## 现有子系统

| 子系统 | 已覆盖版本 | 状态 |
| --- | --- | --- |
| [`player/`](player/OVERVIEW.md) | th06nc、th16、th18 | th06nc 主循环与碰撞/擦弹已定位；th16 三大子系统已反 |
| [`bullet/`](bullet/OVERVIEW.md) | th16 | 核心引擎 + 运动 VM + 激光 |
| [`ecl/`](ecl/OVERVIEW.md) | th06nc、th16、th18 | th06nc 已完成 TH06 基线与新增 opcode 勘探；th16 VM 核心已基本反完 |
| [`sht/`](sht/OVERVIEW.md) | th16 | 运行时语义已攻下（社区此前无公开破解） |
| [`msg/`](msg/OVERVIEW.md) | th16 | 两套指令集全反，已告一段落 |
| [`menu/`](menu/OVERVIEW.md) | th16 | 状态机 + 选择链 |
| [`card/`](card/OVERVIEW.md) | th18 | 已反完：对象模型 / 生命周期 / 21 槽钩子 / C 键释放 / 商店与金钱 |
| [`anm/`](anm/OVERVIEW.md) | th06nc、th16 | th06nc VM 已定位、新增 opcode 32–35 已反完；th16 未系统开工 |
