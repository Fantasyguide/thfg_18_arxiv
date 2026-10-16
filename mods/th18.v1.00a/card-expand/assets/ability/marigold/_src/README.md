# ability/marigold/_src —— 金盏花（id 85）的原始素材与出处

| 文件 | 是什么 | 来源 |
| --- | --- | --- |
| `Marigold.reanim` | PvZ1 原版金盏花的动画数据（33 fps，30 帧；`anim_idle` 段 25 帧） | [ruslan831/PlantsVsZombies-decompilation](https://github.com/ruslan831/PlantsVsZombies-decompilation) 的 `SexyAppFramework/reanim/`（2026-09-12 下载） |
| `parts/*.png` | 它引用的 15 张零件图 | 同上目录 |
| `MARIGOLD_face.png` | 卡图画面（512×640），**生成物** | `../make_marigold_art.py` |

- **版权**：这些是 PopCap / EA 的 *Plants vs. Zombies* 原版美术与动画数据。本仓只留原件与本条出处，
  用于非商业同人 mod；要撤换，删掉 `Marigold.reanim` 与 `parts/` 再重跑 `make anm` 即可。
- **零件为什么有豌豆射手的**：**原版就是这么拼的** —— 金盏花只有花头 / 花瓣 / 五官是自己的，
  茎和叶直接复用 `PEASHOOTER_STALK_*` / `PEASHOOTER_*LEAF*`（见 `Marigold.reanim` 里的 `<i>`）。
- **为什么要上色**：`Marigold_head.png` 与 `Marigold_petals.png` 在原件里是**白的** ——
  原版金盏花有多种颜色，是运行时着色出来的。`make_marigold_art.py` 乘上金黄 `(255,176,48)`，
  正好对上「金盏花」这个名字；茎叶与五官不动。
- 渲染走 `../reanim.py`（本仓自写的最小 reanim 渲染器，按原版逐帧变换拼，不重画）。
