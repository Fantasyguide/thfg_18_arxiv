# ability/peashooter/_src —— 豌豆射手（id 77）的原始素材与出处

| 文件 | 是什么 | 来源 |
| --- | --- | --- |
| `PeaShooter.reanim` | PvZ1 原版豌豆射手的动画数据（`anim_full_idle` 段 25 帧） | [ruslan831/PlantsVsZombies-decompilation](https://github.com/ruslan831/PlantsVsZombies-decompilation) 的 `SexyAppFramework/reanim/`（2026-09-12 下载） |
| `parts/*.png` | 它引用的 19 张零件图 | 同上目录 |
| `PEASHOOTER_face.png` | 卡图画面（512×640），**生成物** | `../make_peashooter_art.py` |

- **版权**：PopCap / EA 的 *Plants vs. Zombies* 原版美术与动画数据。只留原件与出处，用于非商业同人 mod。
- **2026-09-12 换素材**（用户要求）：这张卡原来的两张图是**程序生成**的 PVZ 味仿画，现在改成原版素材：
  - 子机本体 `../PEASHOOTER.png`（64×64）= 原版待机第一帧**去掉茎和叶**（`stalk_*` / `backleaf*` / `frontleaf*`
    三组轨道不画）再**逆时针转 90°** —— 原版是侧面朝右，转完炮口朝上，与我们的子机「向正上方吐豌豆」对上；
  - 卡图 = PvZ 草坪条纹 + 5 倍渲染的**整株**（与向日葵 / 金盏花同一片草坪，三张 PvZ 卡是一套）。
  - `../PEA.png`（豌豆弹）**仍是程序生成**：reanim 里没有豌豆那张图（它是单独的贴图资源）。
- 渲染走 `../reanim.py`。
