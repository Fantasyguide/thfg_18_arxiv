# ability/sunflower/_src —— 向日葵（id 84）的原始素材与出处

| 文件 | 是什么 | 来源 | 版权 |
| --- | --- | --- | --- |
| `sunflower.gif` | PvZ1 原版向日葵**待机动画**，74×73，54 帧 × 50 ms = 2.7 s | 取自 PvZ 复刻项目 [BhavyaC16/Plants-Vs-Zombies](https://github.com/BhavyaC16/Plants-Vs-Zombies) 的 `Assets/images/Plants/sunflower.gif`（2026-09-12 下载） | 素材本身是 PopCap / EA 的 *Plants vs. Zombies* 原版美术 |
| `sunflower_seed_packet.png` | 同项目的向日葵**卡包**图（105×67） | 同上，`Assets/images/Cards/sunflowerCard.png` | 同上 |
| `SUNFLOWER_face.png` | 卡图画面（512×640），**生成物** | `../make_sunflower_art.py` | — |

- **版权口径**：向日葵的形象与这套动画版权归 PopCap / EA。本仓只留原件与本条出处，用于非商业同人 mod；
  要撤换，删掉这两个原件并重跑 `make anm` 即可（生成物 `SF_*.png` 与卡图会一起消失）。
- **为什么留 gif 而不是留切好的帧**：帧是 `make_sunflower_art.py` 现取的（每 2 帧一张、居中贴进 128×128
  透明画布、**一个像素都不重采样**）。留原件 + 生成脚本，换取帧策略不用重新下载素材。
- `sunflower_seed_packet.png` 当前**没有用在任何地方**：卡图用的是草坪底 + 放大的植株（卡包只有 105×67，
  拉到 256×320 会糊）。留着是因为它是「PvZ 自己的卡」，以后要做卡背 / 图鉴小图时是第一顺位素材。
