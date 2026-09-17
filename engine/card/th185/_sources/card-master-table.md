# th185 **卡主表**（一卡一行，汇总全部已抽取的一手数据）—— 素材

> **版本**：TH18.5（th185 v1.00a，Steam 脱壳镜像）。

**列的含义**：`类`=卡表 `+0x0c`（价格类别，§9 权重表下标）；`限`=卡表 `+0x18`（购买上限）；
`族`=覆盖槽签名归族（§38）；`状态格`/`效果号` 来自 [`card-params.md`](card-params.md)；
`动效`=该效果号在 `ability.anm` 的脚本（§47/§48，已验证 23/23 命中）；`卡图`= [`card-art-entries.md`](card-art-entries.md) 的 entry 基名。
**深度**：🟢＝已读方法体（§30/§42/§43/§48 批），🟡＝表推（按族套用即可）。

| id | 内部名 | 显示名 | 类 | 限 | 族 | 状态格 / 效果号 | 动效(anm 脚本) | 卡图 entry | 深度 |
| ---: | --- | --- | ---: | ---: | --- | --- | --- | --- | --- |
| 1 | `PENDULUM2` | 勝手に天下の回り物 | 0 | 999 | 零逻辑(引擎侧) | — | （§43 未找到实现） | `PENDULUM2` | 🟡spec |
| 2 | `DANGO` | 鈴瑚印の団子 | 0 | 999 | 状态格(全) | `th185:0x4d1088`; `th185:0x4d7af8` | — | `DANGO` | 🟢 |
| 4 | `NARUMI` | 肉体強化地蔵 | 1 | 9 | 小族 | `th185:0x4d10c8`; `th185:0x4d10c0`; `th185:0x4d10bc`; `th185:0x4d10c4` | — | `NARUMI` | 🟢 |
| 34 | `PACHE` | 魔法使いの基礎勉強 | 1 | 9 | 状态格(简) | `th185:0x4d10b8` | — | `PACHE` | 🟡 |
| 35 | `MAGICSCROLL` | 法力経典 | 1 | 9 | 每帧/子机 | — | — | `MAGICSCROLL` | 🟡 |
| 24 | `FLAN` | 破壊の美学 | 1 | 3 | 每帧/子机 | `th185:0x4d10b4` | — | `FLAN` | 🟡 |
| 57 | `FUTO` | 龍の通り道 | 4 | 3 | 小族 | — | — | `FUTO` | 🟢 |
| 58 | `AUN` | けしかける狛犬 | 1 | 3 | 每帧/子机 | — | — | `AUN` | 🟢 |
| 59 | `EXTEND` | 命のカード | 1 | 3 | 每帧/子机 | — | — | `EXTEND` | 🟢 |
| 3 | `SPEEDQUEEN` | 疾風の下駄 | 3 | 5 | 状态格(简) | `th185:0x4d10b0` | — | `SPEEDQUEEN` | 🟢 |
| 27 | `CIRNO_OP` | アイスフェアリー | 1 | 5 | 小族 | — | — | `CIRNO_OP` | 🟡 |
| 16 | `STAR` | 静かなる輝き | 2 | 1 | 每帧/子机 | — | — | `STAR` | 🟢 |
| 54 | `LUNA` | 狂おしい静寂 | 2 | 1 | 状态格(简) | `th185:0x4d10c8`; `th185:0x4d10c0`; `th185:0x4d10bc`; `th185:0x4d10c4` | — | `LUNA` | 🟡 |
| 55 | `SUNNY` | 賑やかな無力 | 2 | 1 | 零逻辑(引擎侧) | — | （§43 未找到实现） | `SUNNY` | 🟡spec |
| 56 | `YUREIMU` | ゆのみ霊夢 | 2 | 1 | 小族 | — | — | `YUKKURI_REIMU` | 🟡 |
| 83 | `YUMARISA` | ゆのみ魔理沙 | 5 | 1 | 状态格(全) | `th185:0x4d1070`; `th185:0x4d1074`; `th185:0x4d1068`; `th185:0x4d106c` | — | `YUKKURI_MARISA` | 🟡 |
| 84 | `LARVA` | 刺激的な鱗粉 | 5 | 1 | 状态格(全) | `th185:0x4d1038` | — | `LARVA` | 🟡 |
| 69 | `SEIRAN` | 舞い降りた兎 | 2 | 1 | 状态格(全) | `th185:0x4d1038` | — | `SEIRAN` | 🟡 |
| 63 | `NEMUNO` | 山姥の包丁 | 5 | 1 | 状态格(全) | `th185:0x4d1094` | — | `NEMUNO` | 🟡 |
| 70 | `YOUMU_OP` | 半霊の半分 | 5 | 1 | 小族 | `th185:0x4d1088`; `th185:0x4d7afc` | — | `YOUMU_OP` | 🟡 |
| 14 | `YOUMU_OP2` | 半霊のスペア | 5 | 1 | 小族 | — | — | `YOUMU_OP2` | 🟡 |
| 77 | `REIMU_OP` | 陰陽玉 | 5 | 1 | 每帧/子机 | — | — | `REIMU_OP` | 🟢 |
| 6 | `REIMU_OP2` | 陰陽玉（針） | 5 | 1 | 零逻辑(引擎侧) | — | （§43 未找到实现） | `REIMU_OP2` | 🟡spec |
| 7 | `ALICE_OP` | 上海人形 | 6 | 1 | 零逻辑(引擎侧) | — | （§43 未找到实现） | `ALICE_OP` | 🟡spec |
| 15 | `MARISA_OP` | ミニ八卦炉 | 6 | 1 | 零逻辑(引擎侧) | — | （§43 未找到实现） | `MARISA_OP` | 🟡spec |
| 8 | `MARISA_OP2` | ミニ八卦炉（ミサイル） | 5 | 1 | 每帧/子机 | — | — | `MARISA_OP2` | 🟢 |
| 9 | `SAKUYA_OP` | メイドナイフ | 6 | 1 | 每帧/子机 | — | — | `SAKUYA_OP` | 🟡 |
| 10 | `SAKUYA_OP2` | メイドナイフ（跳弾） | 5 | 1 | 小族 | — | — | `SAKUYA_OP2` | 🟢 |
| 11 | `SANAE_OP` | 無事かえるお守り | 6 | 1 | 小族 | — | — | `SANAE_OP` | 🟢 |
| 12 | `SANAE_OP2` | 蛇の抜け殻入りお守り | 5 | 1 | 小族 | — | — | `SANAE_OP2` | 🟢 |
| 13 | `OKINA_OP` | 背中の扉 | 6 | 1 | 小族 | — | — | `OKINA_OP` | 🟢 |
| 17 | `TORAMARU` | 攻撃的な宝塔 | 6 | 1 | 每帧/子机 | — | — | `TORAMARU` | 🟡 |
| 53 | `NUE_OP` | 鬱陶しいＵＦＯ | 6 | 1 | 每帧/子机 | — | — | `NUE_OP` | 🟡 |
| 18 | `SEKIBANKI` | 自由気ままな生首 | 4 | 1 | 每帧/子机 | — | — | `SEKIBANKI` | 🟡 |
| 72 | `EIKA` | 気ままな無脊椎動物 | 4 | 1 | 小族 | — | — | `EIKA` | 🟡 |
| 76 | `MAGATAMA` | 太古の勾玉 | 4 | 1 | 小族 | — | — | `boss4` | 🟡 |
| 49 | `OPTION_BR` | 偶像防衛隊 | 5 | 1 | 小族 | — | — | `OPTION_BR` | 🟡 |
| 28 | `WARP` | 画面の境界 | 8 | 1 | 小族 | `th185:0x4d1078` | — | `WARP` | 🟢 |
| 39 | `SEIJA` | ひらり布 | 2 | 1 | 状态格(简) | `th185:0x4d1078` | — | `SEIJA` | 🟡 |
| 78 | `LILY` | アイテムの季節 | 2 | 1 | 状态格(全) | — | — | `LILY` | 🟡 |
| 46 | `KOZUCHI` | 打ち出の小槌 | 2 | 1 | 小族 | — | — | `KOZUCHI` | 🟡 |
| 40 | `BASSDRUM` | 重低音バスドラム | 5 | 1 | 状态格(全) | `th185:0x4d1090` | — | `BASSDRUM` | 🟡 |
| 47 | `PSYCO` | サイコキネシス | 5 | 1 | 状态格(全) | `th185:0x4d10ac` | — | `PSYCO` | 🟡 |
| 48 | `WAKASAGI` | わかさぎの鱗 | 6 | 1 | 小族 | — | — | `WAKASAGI` | 🟡 |
| 71 | `KUTAKA` | 正直すぎる道しるべ | 5 | 1 | 小族 | — | — | `KUTAKA` | 🟡 |
| 74 | `KOMACHI` | 急がば三途の回り道 | 5 | 1 | 小族 | — | — | `KOMACHI` | 🟡 |
| 75 | `KANAME2` | 要石ミサイル | 5 | 1 | 小族 | — | — | `KANAME2` | 🟡 |
| 79 | `CYLINDER` | 霊力の標本瓶 | 5 | 1 | 小族 | — | — | `boss5b` | 🟡 |
| 50 | `KANAME` | 忍耐の要石 | 5 | 1 | 小族 | — | — | `KANAME` | 🟡 |
| 41 | `MOON` | 狂気の月 | 12 | 1 | 效果对 | 建 `script75` / 驱动 `script29` | ↑（`ability.anm` 脚本号） | `MOON` | 🟡 |
| 42 | `MIKOFLASH` | やんごとなき威光 | 8 | 1 | 效果对 | 建 `script80` / 驱动 `script31` | ↑（`ability.anm` 脚本号） | `MIKOFLASH` | 🟡 |
| 43 | `VAMPIRE` | ヴァンパイアファング | 12 | 1 | 效果对 | 建 `script78` / 驱动 `script30` | ↑（`ability.anm` 脚本号） | `VAMPIRE` | 🟡 |
| 44 | `RICEBALL` | 大天狗の麦飯 | 13 | 1 | 效果对 | 建 `script73` / 驱动 `script28` | ↑（`ability.anm` 脚本号） | `boss5` | 🟡 |
| 51 | `SUN` | 地底の太陽 | 9 | 1 | 效果对 | 建 `script71` / 驱动 `script26` | ↑（`ability.anm` 脚本号） | `SUN` | 🟡 |
| 45 | `TEWI` | 詐欺うさぎの足 | 13 | 1 | 效果对 | 建 `script69` / 驱动 `script24` | ↑（`ability.anm` 脚本号） | `TEWI` | 🟡 |
| 22 | `MANEKI` | 商売上手な招き猫 | 2 | 1 | 效果对 | 建 `script68` / 驱动 `script23` | ↑（`ability.anm` 脚本号） | `MANEKI_NEKO` | 🟡 |
| 36 | `KOISHI` | 小石ころ帽子 | 3 | 1 | 效果对 | 建 `script20` / 驱动 `script16` | ↑（`ability.anm` 脚本号） | `KOISHI` | 🟡 |
| 25 | `ITEM_LINE` | 神山への供物 | 1 | 1 | 小族 | — | — | `ITEM_LINE` | 🟡 |
| 20 | `ITEM_CATCH` | 守銭奴の教訓 | 3 | 1 | 效果对 | 建 `script16` / 驱动 — | ↑（`ability.anm` 脚本号） | `ITEM_CATCH` | 🟢 |
| 19 | `YUYUKO` | 弾幕の亡霊 | 3 | 1 | 效果对 | 建 `script23` / 驱动 `script17` | ↑（`ability.anm` 脚本号） | `YUYUKO` | 🟢 |
| 31 | `URUMI` | 濡れた大蛇 | 5 | 1 | 效果对 | 建 `script23` / 驱动 `script17` | ↑（`ability.anm` 脚本号） | `URUMI` | 🟡 |
| 73 | `DEAD_SPELL` | かぐや姫の隠し箱 | 3 | 1 | 效果对 | 建 `script14` / 驱动 `script21` | ↑（`ability.anm` 脚本号） | `DEAD_SPELL` | 🟢 |
| 29 | `MONEY` | 鬼傑組長の脅嚇 | 8 | 1 | 效果对 | 建 `script14` / 驱动 `script14` | ↑（`ability.anm` 脚本号） | `MONEY` | 🟢 |
| 32 | `YAMAWARO` | 山童的買い物術 | 2 | 1 | 效果对 | 建 `script7` / 驱动 `script20` | ↑（`ability.anm` 脚本号） | `boss2` | 🟡 |
| 37 | `KISERU` | ドラゴンキセル | 5 | 1 | 效果对 | 建 `script7` / 驱动 — | ↑（`ability.anm` 脚本号） | `boss3` | 🟡 |
| 38 | `MUKADE` | 暴食のムカデ | 5 | 1 | 效果对 | 建 `script6` / 驱动 `script33` | ↑（`ability.anm` 脚本号） | `boss7` | 🟡 |
| 52 | `AUTOBOMB` | 生命爆発の薬 | 5 | 1 | 效果对 | 建 `script6` / 驱动 `script19` | ↑（`ability.anm` 脚本号） | `AUTOBOMB` | 🟢 |
| 21 | `MINORIKO` | 生命の豊穣 | 4 | 1 | 效果对 | 建 `script6` / 驱动 `script11` | ↑（`ability.anm` 脚本号） | `MINORIKO` | 🟡 |
| 68 | `SAKI` | 不屈の脳筋 | 3 | 1 | 效果对 | 建 `script2` / 驱动 `script32` | ↑（`ability.anm` 脚本号） | `SAKI` | 🟡 |
| 81 | `MOKOU` | 不死鳥の尾 | 9 | 1 | 效果对 | 建 `script2` / 驱动 `script18` | ↑（`ability.anm` 脚本号） | `MOKOU` | 🟡 |
| 5 | `TAKANE` | 百回目のブラックマーケット | 13 | 1 | 效果对 | 建 `script2` / 驱动 `script10` | ↑（`ability.anm` 脚本号） | `TAKANE` | 🟡 |
| 67 | `MAINSHOT_PU` | 弱肉強食の理 | 2 | 1 | 状态格(全) | `th185:0x4d10a0` | — | `MAINSHOT_PU` | 🟢 |
| 23 | `MAINSHOT_SP` | はじける赤蛙 | 8 | 1 | 小族 | — | — | `MAINSHOT_SP` | 🟡 |
| 26 | `POWERMAX` | 頼りになる弟子狸 | 8 | 1 | 小族 | — | — | `POWERMAX` | 🟢 |
| 30 | `ROKUMON` | 地獄の沙汰も金次第 | 11 | 1 | 状态格(简) | `th185:0x4d1078` | — | `ROKUMON` | 🟢 |
| 33 | `JOON` | 抗いがたきジュリ扇 | 6 | 1 | 小族 | — | — | `JOON` | 🟡 |
| 60 | `PEACE` | 命を焚く松明 | 4 | 1 | 小族 | — | — | `PEACE` | 🟡 |
| 80 | `SUIKA` | 酔狂の伊吹瓢 | 7 | 1 | 状态格(简) | `th185:0x4d109c` | — | `SUIKA` | 🟡 |
| 82 | `BLANK` | 空白のカード | 7 | 1 | 状态格(简) | `th185:0x4d1098` | — | `BLANK` | 🟡 |
| 0 | `NITORI` | 資本主義のジレンマ | 5 | 1 | 状态格(全) | `th185:0x4d108c` | — | `NITORI` | 🟢 |
| 66 | `SHION` | 煤けた団扇 | 8 | 1 | 状态格(全) | `th185:0x4d1088` | — | `SHION` | 🟡 |
| 61 | `KEIKI` | クリエイターなら容易な事 | 4 | 1 | 状态格(简) | `th185:0x4d10c8`; `th185:0x4d10c0`; `th185:0x4d10bc`; `th185:0x4d10c4` | — | `KEIKI` | 🟡 |
| 62 | `DREMII` | 数えたくなる羊 | 1 | 1 | 小族 | — | — | `DREMII` | 🟡 |
| 64 | `JYUNKA` | 純粋な自己肯定感 | 5 | 1 | 零逻辑(引擎侧) | — | （§43 未找到实现） | `JYUNKA` | 🟡spec |
| 65 | `NULL` | 装備可能です | 8 | 1 | 零逻辑(引擎侧) | — | （§43 未找到实现） | `NULL` | 🟡spec |
| 85 | `BACK` |  | 0 | 1 | 零逻辑(引擎侧) | — | （§43 未找到实现） | `BACK` | 🟡spec |
| 86 | `?` |  | 0 | 1 | 零逻辑(引擎侧) | — | （§43 未找到实现） | `?` | 🟡spec |
