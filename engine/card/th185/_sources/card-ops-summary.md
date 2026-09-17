# th185 卡方法**操作摘要**（机械抽取，主要看 `+0x2c` 每帧体）—— 素材

> **版本**：TH18.5（th185 v1.00a，Steam 脱壳镜像）。

**来源**：`+0x2c`（管理器每帧，§18.1 锚定）的真实方法体，最多反汇编 70 条；
只列**写绝对全局**（带操作与立即数）、**写 `[reg+位移]` 字段**（带立即数）、**call 立即数**。

| id | 内部名 | 方法体 | 写全局 | 写字段 | 立即数 call |
| ---: | --- | --- | --- | --- | --- |
| 0 | `NITORI` | `th185:0x40a600` | mov `th185:0x4d108c` 0x3e7; mov `th185:0x4d108c` 0x0; mov `th185:0x4d108c` | and [edi+0x54] `-3`; mov [esi+0x0] `th185:0x4b8998` | — |
| 1 | `PENDULUM2` | （走基类） | — | — | — |
| 2 | `DANGO` | `th185:0x414e40` | add `th185:0x4d1088` 0x64; dec `th185:0x4d1088`; mov `th185:0x4d7af8` 0x0 | and [ecx+0x54] `-3`; mov [esi+0x0] `th185:0x4b8998`; mov [esi+0x10] 0x0; mov [esi+0x14] 0x0 | `th185:0x48f711` |
| 3 | `SPEEDQUEEN` | `th185:0x4136a0` | mov `th185:0x4d10b0` 0x2 | and [ecx+0x54] `-3`; mov [esi+0x0] `th185:0x4b8998`; mov [esi+0x10] 0x0; mov [esi+0x14] 0x0; mov [ecx+0x38] 0x0; | `th185:0x48f711` |
| 4 | `NARUMI` | `th185:0x414d50` | mov `th185:0x4d10c8` 0x7; mov `th185:0x4d10c0`; mov `th185:0x4d10bc`; mov `th185:0x4d10bc` 0x7; mov `th185:0x4d10c0` 0x0; inc `th185:0x4d10c4` | and [edx+0x54] `-3`; mov [esi+0x0] `th185:0x4b8998`; mov [esi+0x10] 0x0; mov [esi+0x14] 0x0 | `th185:0x48f711` |
| 5 | `TAKANE` | （走基类） | — | — | — |
| 6 | `REIMU_OP2` | （走基类） | — | — | — |
| 7 | `ALICE_OP` | （走基类） | — | — | — |
| 8 | `MARISA_OP2` | `th185:0x411b10` | — | or [esi+0x54] 0x20 | `th185:0x48a4a0` |
| 9 | `SAKUYA_OP` | `th185:0x4116e0` | — | or [edi+0x54] 0x20; mov [edi+0x58] 0x0; mov [edi+0x24] 0x0; mov [edi+0x2c] 0x0; mov [edi+0x28] 0x0; mov [edi+0 | `th185:0x42fd90`, `th185:0x44e6a0`, `th185:0x48a650`, `th185:0x409d80` |
| 10 | `SAKUYA_OP2` | `th185:0x4113d0` | — | and [esi+0x54] `-33`; and [esp+0x24] `-2`; mov [esp+0x8] 0x0; mov [esp+0xc] `-1040187392`; mov [esp+0x | `th185:0x409d80`, `th185:0x491360` |
| 11 | `SANAE_OP` | `th185:0x410e90` | — | or [esi+0x54] 0x20; mov [esi+0x1c] 0x0; mov [esi+0x58] 0x2; mov [esi+0x24] 0x0; mov [esi+0x2c] 0x0; mov [esi+0 | `th185:0x48a650`, `th185:0x40a1f0`, `th185:0x48a540`, `th185:0x40a0f0`, `th185:0x48a4a0`, `th185:0x42fd90`,  |
| 12 | `SANAE_OP2` | `th185:0x410880` | — | or [esi+0x54] 0x20; mov [edi+`295556`] 0x12c; mov [esp+0x0] 0x0; mov [ebp+-0x1c] `1107296256` | `th185:0x48a4a0`, `th185:0x463a90` |
| 13 | `OKINA_OP` | `th185:0x410380` | — | and [edi+0x54] `-33`; or [edi+0x54] 0x20; mov [edi+0x58] 0x2; mov [edi+0x58] 0x3 | `th185:0x409140`, `th185:0x48a4a0`, `th185:0x405ad0`, `th185:0x4792b0` |
| 14 | `YOUMU_OP2` | （走基类） | — | — | — |
| 15 | `MARISA_OP` | （走基类） | — | — | — |
| 16 | `STAR` | `th185:0x412f30` | inc `th185:0x529ff1`; dec `th185:0x529ff1` | or [esi+0x538] 0x1000; mov [ebp+-0xc] 0x0; mov [esi+0x44] 0x0; mov [esi+0x520] 0x0 | `th185:0x409d80`, `th185:0x48ac50`, `th185:0x407080`, `th185:0x47ac70`, `th185:0x489e30` |
| 17 | `TORAMARU` | `th185:0x40fd40` | — | and [esi+0x54] `-33`; mov [esi+0x1c] 0x0 | `th185:0x48a650` |
| 18 | `SEKIBANKI` | `th185:0x40f0b0` | — | or [edi+0x54] 0x20; mov [edi+0x58] 0x0 | `th185:0x4063e0` |
| 19 | `YUYUKO` | （走基类） | — | — | — |
| 20 | `ITEM_CATCH` | （走基类） | — | — | — |
| 21 | `MINORIKO` | （走基类） | — | — | — |
| 22 | `MANEKI` | （走基类） | — | — | — |
| 23 | `MAINSHOT_SP` | （走基类） | — | — | — |
| 24 | `FLAN` | `th185:0x414270` | mov `th185:0x4d10b4` 0x0 | or [esi+0x54] 0x20; mov [esi+0x24] 0x0; mov [esi+0x28] 0x0; and [esi+0x54] `-33`; mov [esi+0x2c] 0x0 | `th185:0x409d80` |
| 25 | `ITEM_LINE` | （走基类） | — | — | — |
| 26 | `POWERMAX` | `th185:0x40ab40` | — | mov [esi+0x0] `th185:0x4b8998`; mov [esi+0x10] 0x0; mov [esi+0x14] 0x0 | `th185:0x40a180` |
| 27 | `CIRNO_OP` | （走基类） | — | — | — |
| 28 | `WARP` | `th185:0x40e860` | mov `th185:0x4d1078` | mov [eax+0x164] 0x1; mov [ecx+0x58] 0x64; mov [esi+0x0] `th185:0x4b8998`; mov [esi+0x10] 0x0; mov [esi+0x14] 0x0 | `th185:0x48f711` |
| 29 | `MONEY` | （走基类） | — | — | — |
| 30 | `ROKUMON` | `th185:0x40aab0` | mov `th185:0x4d1078` | and [ecx+0x54] `-3`; mov [esi+0x0] `th185:0x4b8998`; mov [esi+0x10] 0x0; mov [esi+0x14] 0x0 | `th185:0x48f711` |
| 31 | `URUMI` | （走基类） | — | — | — |
| 32 | `YAMAWARO` | （走基类） | — | — | — |
| 33 | `JOON` | （走基类） | — | — | — |
| 34 | `PACHE` | `th185:0x414c20` | mov `th185:0x4d10b8` 0x64; mov `th185:0x4d10b8` 0x0; mov `th185:0x4d10b8` | and [edi+0x54] `-3`; mov [esi+0x0] `th185:0x4b8998`; mov [esi+0x10] 0x0; mov [esi+0x14] 0x0 | `th185:0x48f711` |
| 35 | `MAGICSCROLL` | `th185:0x414660` | — | and [eax+0x0] `-2`; mov [esi+0x24] 0x0; mov [esi+0x28] 0x0; mov [esi+0x20] `-1`; and [esi+0x54] 0x | `th185:0x48a4a0`, `th185:0x42fd90`, `th185:0x44e6a0` |
| 36 | `KOISHI` | （走基类） | — | — | — |
| 37 | `KISERU` | （走基类） | — | — | — |
| 38 | `MUKADE` | （走基类） | — | — | — |
| 39 | `SEIJA` | `th185:0x40e7c0` | mov `th185:0x4d1078` | and [ecx+0x54] `-3`; mov [esi+0x0] `th185:0x4b8998`; mov [esi+0x10] 0x0; mov [esi+0x14] 0x0 | `th185:0x48f711` |
| 40 | `BASSDRUM` | `th185:0x40de70` | mov `th185:0x4d1090` 0x3e7; mov `th185:0x4d1090` 0x0; mov `th185:0x4d1090` | and [edi+0x54] `-3`; mov [esi+0x0] `th185:0x4b8998` | — |
| 41 | `MOON` | （走基类） | — | — | — |
| 42 | `MIKOFLASH` | （走基类） | — | — | — |
| 43 | `VAMPIRE` | （走基类） | — | — | — |
| 44 | `RICEBALL` | （走基类） | — | — | — |
| 45 | `TEWI` | （走基类） | — | — | — |
| 46 | `KOZUCHI` | （走基类） | — | — | — |
| 47 | `PSYCO` | `th185:0x40dd50` | mov `th185:0x4d10ac` 0x3e7; mov `th185:0x4d10ac` 0x0; mov `th185:0x4d10ac` | and [edi+0x54] `-3`; mov [esi+0x0] `th185:0x4b8998` | — |
| 48 | `WAKASAGI` | （走基类） | — | — | — |
| 49 | `OPTION_BR` | `th185:0x40e940` | — | mov [eax+`8148340`] 0x1; mov [esi+0x0] `th185:0x4b8998`; mov [esi+0x10] 0x0; mov [esi+0x14] 0x0; and [ecx+0x54] `1048575` | `th185:0x48f711` |
| 50 | `KANAME` | （走基类） | — | — | — |
| 51 | `SUN` | （走基类） | — | — | — |
| 52 | `AUTOBOMB` | （走基类） | — | — | — |
| 53 | `NUE_OP` | `th185:0x40f810` | — | or [esi+0x54] 0x20; mov [esi+0x58] 0x0; mov [esi+0x1c] 0x0 | `th185:0x48a4a0`, `th185:0x48a650`, `th185:0x40a060` |
| 54 | `LUNA` | `th185:0x412e30` | mov `th185:0x4d10c8` 0x7; mov `th185:0x4d10c0`; mov `th185:0x4d10bc`; mov `th185:0x4d10bc` 0x7; mov `th185:0x4d10c0` 0x0; inc `th185:0x4d10c4` | and [edx+0x54] `-3`; mov [esi+0x0] `th185:0x4b8998`; mov [esi+0x10] 0x0; mov [esi+0x14] 0x0; and [ecx+0x54] 0xff | `th185:0x48f711` |
| 55 | `SUNNY` | （走基类） | — | — | — |
| 56 | `YUREIMU` | （走基类） | — | — | — |
| 57 | `FUTO` | （走基类） | — | — | — |
| 58 | `AUN` | `th185:0x413a70` | — | mov [ebp+-0xc] `1142947840`; mov [ebp+-0x8] `1107296256`; mov [ebp+-0x4] 0x0 | `th185:0x409d80`, `th185:0x48a540`, `th185:0x4792b0`, `th185:0x42fed0` |
| 59 | `EXTEND` | `th185:0x413760` | — | mov [ebp+-0xc] `1107296256`; mov [ebp+-0x8] `1142947840`; mov [ebp+-0x4] 0x0 | `th185:0x409d80`, `th185:0x48a540`, `th185:0x4792b0`, `th185:0x42fed0` |
| 60 | `PEACE` | （走基类） | — | — | — |
| 61 | `KEIKI` | `th185:0x40a420` | mov `th185:0x4d10c8` 0x7; mov `th185:0x4d10c0`; mov `th185:0x4d10c0` 0x0; mov `th185:0x4d10bc`; mov `th185:0x4d10bc` 0x7; mov `th185:0x4d10c4` | and [ecx+0x54] `-3`; mov [esi+0x0] `th185:0x4b8998`; mov [esi+0x10] 0x0 | — |
| 62 | `DREMII` | （走基类） | — | — | — |
| 63 | `NEMUNO` | `th185:0x4129a0` | mov `th185:0x4d1094` 0x64; mov `th185:0x4d1094` 0x0; mov `th185:0x4d1094` | and [edi+0x54] `-3`; mov [esi+0x0] `th185:0x4b8998`; mov [esi+0x10] 0x0; mov [esi+0x14] 0x0 | `th185:0x48f711` |
| 64 | `JYUNKA` | （走基类） | — | — | — |
| 65 | `NULL` | （走基类） | — | — | — |
| 66 | `SHION` | `th185:0x40a550` | add `th185:0x4d1088` 0xf | and [ecx+0x54] `-3`; mov [esi+0x0] `th185:0x4b8998`; mov [esi+0x10] 0x0; mov [esi+0x14] 0x0; or [ecx+0x54] 0x2 | `th185:0x48f711` |
| 67 | `MAINSHOT_PU` | `th185:0x40b0e0` | mov `th185:0x4d10a0` 0x3e7; mov `th185:0x4d10a0` 0x0; mov `th185:0x4d10a0` | and [edi+0x54] `-3`; mov [esi+0x0] `th185:0x4b8998` | `th185:0x48a4a0`, `th185:0x417940`, `th185:0x48a540` |
| 68 | `SAKI` | （走基类） | — | — | — |
| 69 | `SEIRAN` | `th185:0x412ab0` | mov `th185:0x4d1038` 0x7 | and [ecx+0x54] `-3`; mov [esi+0x0] `th185:0x4b8998`; mov [esi+0x10] 0x0; mov [esi+0x14] 0x0; or [ecx+0x54] 0x2 | `th185:0x48f711` |
| 70 | `YOUMU_OP` | `th185:0x4128e0` | inc `th185:0x4d1088`; mov `th185:0x4d7afc` 0x0 | mov [esi+0x0] `th185:0x4b8998`; mov [esi+0x10] 0x0; mov [esi+0x14] 0x0; or [ecx+0x54] 0x2 | `th185:0x48f711` |
| 71 | `KUTAKA` | （走基类） | — | — | — |
| 72 | `EIKA` | （走基类） | — | — | — |
| 73 | `DEAD_SPELL` | （走基类） | — | — | — |
| 74 | `KOMACHI` | （走基类） | — | — | — |
| 75 | `KANAME2` | （走基类） | — | — | — |
| 76 | `MAGATAMA` | （走基类） | — | — | — |
| 77 | `REIMU_OP` | `th185:0x4120f0` | — | and [esi+0x54] `-33` | `th185:0x409d80`, `th185:0x48a4a0` |
| 78 | `LILY` | `th185:0x40e720` | — | mov [eax+`8148348`] 0x1; and [ecx+0x54] `-3`; mov [esi+0x0] `th185:0x4b8998`; mov [esi+0x10] 0x0; mov [esi+0x14]  | `th185:0x48f711` |
| 79 | `CYLINDER` | （走基类） | — | — | — |
| 80 | `SUIKA` | `th185:0x40a820` | mov `th185:0x4d109c` 0x64; mov `th185:0x4d109c` 0x0; mov `th185:0x4d109c` | and [edi+0x54] `-3`; mov [esi+0x0] `th185:0x4b8998`; mov [esi+0x10] 0x0; mov [esi+0x14] 0x0 | `th185:0x48f711` |
| 81 | `MOKOU` | （走基类） | — | — | — |
| 82 | `BLANK` | `th185:0x40a710` | mov `th185:0x4d1098` 0x3e7; mov `th185:0x4d1098` 0x0; mov `th185:0x4d1098` | and [edi+0x54] `-3`; mov [esi+0x0] `th185:0x4b8998` | — |
| 83 | `YUMARISA` | `th185:0x412c10` | add `th185:0x4d1070`; add `th185:0x4d1074`; mov `th185:0x4d1068` 0x0; mov `th185:0x4d106c` 0x0 | mov [esi+0x494] 0x2; and [edi+0x54] `-3`; mov [esi+0x0] `th185:0x4b8998`; mov [esi+0x10] 0x0; mov [esi+0x14] 0x0 | `th185:0x48f711` |
| 84 | `LARVA` | `th185:0x412b60` | mov `th185:0x4d1038` 0x0 | and [ecx+0x54] `-3`; mov [esi+0x0] `th185:0x4b8998`; mov [esi+0x10] 0x0; mov [esi+0x14] 0x0; or [ecx+0x54] 0x2 | `th185:0x48f711` |
| 85 | `BACK` | （走基类） | — | — | — |
| 86 | `?` | （走基类） | — | — | — |
