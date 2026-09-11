# archive —— 已退役的策略

不进构建、不进 `make test`。留着是因为它们各自代表一条路线，改新策略时可以拿来对拍。

| 文件 | 路线 | 退役原因 |
| --- | --- | --- |
| `policy_sampling.c` + `test_policy_sampling.c` | DWA 型短视界采样：18 个候选 × 24 帧外推取最小净距 | 候选少、视界短，弹密时候选全负只剩炸弹；被势场 + VO 掩码取代（2026-09-11，见 `../policy.c` 头部的决策记录） |

要单独编它：`gcc -std=c99 -I.. -o /tmp/tps archive/test_policy_sampling.c archive/policy_sampling.c -lm`
（`policy_sampling.c` 里的符号名与现行 `policy.h` 相同，不能和 `policy.c` 一起链接）。
