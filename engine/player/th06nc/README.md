# TH06NC Player

Steam New Classic 的玩家子系统。定位手法与 ECL 不同（主循环是 chain 回调、没有 xref），
见 [主循环与碰撞/擦弹判定](01-main-loop-and-collision.md)。

⚠️ **本作把被弹判定从 TH06 的轴对齐盒换成了圆形**（半径 1.25，与 TH06 半宽同值，所以圆内接于方、四角被削掉）。擦弹逻辑被内联进弹幕主循环，没有独立函数。

| 入口 | 地址 | 当前命名 |
| --- | --- | --- |
| 玩家每帧 tick | `0x140068820` | `Player__OnUpdate` |
| 弹幕每帧 tick | `0x140010870` | `BulletManager__OnUpdate` |
| 玩家初始化 | `0x140068160` | `Player__AddedCallback` |
| 被弹（死亡）判定 | `0x14006a980` | `Player__CalcKillBoxCollision` |
| 炸弹弹幕裁剪 | `0x14006a8c0` | `Player__BombProjectileClipsBox` |
| 激光判定 | `0x14006aba0` | `Player__CalcLaserHitbox` |

Ghidra 应用脚本为 [`tooling/ghidra/apply_th06nc_player.py`](../../../tooling/ghidra/apply_th06nc_player.py)。
