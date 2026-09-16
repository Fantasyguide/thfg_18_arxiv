# engine/ — 知识层

**子系统为主，版本为辅。** 每个子系统一个目录：

```
engine/<子系统>/
  OVERVIEW.md      门面:跨版本模型 + 断言×版本矩阵(每格链回一手文档)
  <版本>/          一手证据:地址、偏移、反编译依据、可信度分级
```

`_shared/` 是例外：装真正跨子系统的引擎知识（主循环、数学/PRNG、归档格式、社区来源）。

> **★ 铁律**：写 UI / 动画 / 特效前，**先在本层找到原版怎么做**（anm 脚本号 + 引擎地址 + 数值 +
> 触发条件），照抄后再动手；找不到就标 `❓ 自定`。见 [`../METHOD.md`](../METHOD.md)。

## 为什么是这个轴

版本为主会让共通部分在各作之间重复，长出互相打架的说法；
子系统为主则强迫你在落笔时回答「这条是共通的还是本作特有的」——
矩阵就是这个回答的载体。**新作的特有机制也是子系统**（TH18 卡牌 → `card/`），
矩阵里其他版本填 ❌ 即可，不要再开版本目录。

## 现有子系统

| 子系统 | 已覆盖版本 | 状态 |
| --- | --- | --- |
| [`player/`](player/OVERVIEW.md) | th06nc、th10、th11、th15、th16、th18 | th06nc 主循环与碰撞/擦弹已定位；th16 三大子系统已反；th18 已定位自机位置与状态计时器；th15 擦弹已反；**th10 灵击全链（伤害/半径/震屏否定）与 th11 灵击+命碎、六个支援特技已反** |
| [`bullet/`](bullet/OVERVIEW.md) | th06nc、th16、th18 | th06nc 两个池与 Bullet/Laser 结构体已定位；th16 核心引擎 + 运动 VM + 激光；**th18 消弹→撒道具全链（五个入口 + mode 真值表）已反** |
| [`enemy/`](enemy/OVERVIEW.md) | th06nc | 池与生成路径已定位；发现 NC 独有的 ECL 变量继承 |
| [`ecl/`](ecl/OVERVIEW.md) | th06nc、th16、th18 | th06nc 已完成 TH06 基线与新增 opcode 勘探；th16 VM 核心已基本反完 |
| [`std/`](std/OVERVIEW.md) | th06nc | 解释器与六条 opcode 已反完；**发现 NC 独有的背景循环 opcode**（op3 的 -1/-2） |
| [`sht/`](sht/OVERVIEW.md) | th16 | 运行时语义已攻下（社区此前无公开破解） |
| [`msg/`](msg/OVERVIEW.md) | th16 | 两套指令集全反，已告一段落 |
| [`menu/`](menu/OVERVIEW.md) | th16 | 状态机 + 选择链 |
| [`card/`](card/OVERVIEW.md) | th18 | 已反完：对象模型 / 生命周期 / 21 槽钩子 / C 键释放 / 商店与金钱 |
| [`chapter/`](chapter/OVERVIEW.md) | th15 | 章节（波次）系统 + 章节奖：边界/计数/结算/UI 全链 |
| [`season/`](season/OVERVIEW.md) | th16 | 季节系统（季节点/季节槽/季节子机/升级/季节解放）；**th18 侧残骸审计 = 五样全无** |
| [`animal/`](animal/OVERVIEW.md) | th17 | 动物灵 / 咆哮（HYPER）；**th18 侧残骸审计 = 六样全无（只剩一串无名死槽）** |
| [`ufo/`](ufo/OVERVIEW.md) | th12 | 飞碟（UFO）：令牌槽 → 召唤 → 吸收道具 → 击破奖励全链一手 |
| [`spirit/`](spirit/OVERVIEW.md) | th13 | 神灵 / 灵界（Trance）：六条生成通道 + 槽 + 灵界两道「中弹不死」闸门 |
| [`input/`](input/OVERVIEW.md) | th10、th11、th18 | 输入装配线：键盘配置表 / 掩码位 / 哪些键空着（A、S 空着，D 被切卡占用）；th10·th11 的位掩码与「按键 N 帧 → 派生位」已反（**th10 无双击方向键**）|
| [`faith/`](faith/OVERVIEW.md) | th10、th11 | 信仰 / 最大得点及其倍率：th10 的信仰量纲与蓝点分、th11 的绿点三档 + (交信度 + 擦弹倍率)；**TH10 无擦弹倍率、无擦弹计数器的否定结论** |
| [`anm/`](anm/OVERVIEW.md) | th06nc、th16、th18 | th06nc VM 已定位、新增 opcode 32–35 已反完；th18 已反 VM 实例化 / 渲染阶段 / **ASCII 文字系统**；th16 仍只有旁敲锚点 |
| [`item/`](item/OVERVIEW.md) | th07、th08、th11 | 道具池 + `spawn_item` + 掉落 opcode；th07 樱点三档的来源与数量、th08 时点；**th11 的 11 项类型跳表、命碎（5 片 = 1 命）与收集 `+N` 弹窗** |
