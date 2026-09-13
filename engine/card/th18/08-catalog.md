# TH18 卡牌功能目录(代码优先 + 社区交叉)
> **版本**：TH18 v1.00a（`th18.exe`）。本文裸地址默认属该版本；引用其他版本须写成 `th16:0x…`。
>

> 适用:TH18 v1.00a,`th18.exe`(`th18`)。**代码=一手 ground truth**,社区(THBWiki/wikiwiki.jp/bilibili)印证。数值 @60fps。
> 价格表 `0x4b35c4`:t0=0/t1=50/t2=80/t3=100/t4=100/t5=140/t6=180/t7=200/t8=240/t9=280/t10=300/t11=350/t12=400/t13=450/t14=500。
> **card_id↔类映射经权威核验**:`AbilityCard<X>Inf` vtable 槽 +0x50(operator_delete)→ `Card<角色>__operator_delete`(各角色唯一),
> 叠加 allocate switch 的 card_id↔vtable。下表 id/角色为此法钉死;子 agent 波次的 effect 已按此**校正贴回正确 id**(见 §订正)。

## ★ boss 卡 ↔ 角色(代码铁证,回答之前的开放问题)
dmode 1-5 的"本关专属卡"= 各关 boss 的角色卡,经 vtable 链证实(非 lore 推断):
| 关 | dmode | card_id | 内部名 | 角色类 | boss |
| --- | --- | --- | --- | --- | --- |
| 1 | 1 | 38 | MANEKI | CardMike | 三毛猫/Mike Goutokuji |
| 2 | 2 | 39 | YAMAWARO | CardTakane | Takane Yamashiro |
| 3 | 3 | 40 | KISERU | CardSannyo | Sannyo Komakusa |
| 4 | 4 | 51 | MAGATAMA | CardMisumaru | Misumaru Tamatsukuri |
| 5 | 5 | 52 | CYLINDER | CardTsukasa | Tsukasa Kudamaki |
| 5 | 5 | 53 | RICEBALL | CardMegumu | Megumu(同 5 面段) |
| EX | — | 54 | MUKADE | CardMomoyo | Momoyo Himemushi(暴食のムカデ) |

## A. 使用类(主动卡,按 C 发动;充能=帧/秒)

| id | 内部名 | 角色 | 效果 | 充能 | 激活 | 价 | 可信 |
| --- | --- | --- | --- | --- | --- | --- | --- |
| 43 | KANAME 要石 | Tenshi | 锁位生成要石,**全屏清弹+激光**(随玩家漂),清满250弹或到时止 | 3600f/60s | 1800f/30s | 400 | ✅ |
| 45 | MIKOFLASH | Miko | 伤害源120;40–119帧每8帧**随机点名消弹**(10次) | 1800f/30s | 130f/2.2s | 400 | ✅ |
| 46 | VAMPIRE | Remilia | 30f蓄→60f **矩形局部清弹+伤害脉冲**(宽32) | 1200f/20s | 90f | 240 | ✅ |
| 47 | SUN | Utusho | **每帧全屏清弹+激光**,每6帧耗1power(~1.0P) | 18000f/300s | 600f/10s | 450 | ✅ |
| 44 | MOON | Clownpiece | 锁定位放**膨胀/收缩清弹球**(不跟随),半径随ANM长 | 2700f/45s | 80+40f | 240 | ✅ |
| 49 | BASSDRUM | Raiko | 锁位30f内**每帧全屏清弹+激光** | 600f/10s | 30f/0.5s | 140 | ✅ |
| 50 | PSYCO | Sumireko | 120f内对玩家附近(半径`弹半径×0.5+64`)type1/2弹**拦截发自机弹并消** | 1500f/25s | 120f/2s | 180 | ✅ |
| 42 | KOZUCHI 小槌 | Shinmyoumaru | 生成小槌跟随,20f后"挥击"半径4清弹/激光~10f | 2400f/40s | ~40f | 180 | ✅ |
| 48 | LILY | LilyWhite | 召唤**搬运道具妖精敌人**(每3次召强化版);参数随次数递增 | 7200f/120s | — | 300 | ✅ |
| 41 | WARP | Yukari | 按移动方向切换**瞬移方向位**(player+0x4779c) | (非帧倒计时🟡) | — | 300 | ✅机制 |
| 52 | CYLINDER | **Tsukasa**(5面boss) | **c_press 直接放炸弹**(走BOMB begin),耗1.00P,需≥2.00P,可吃spell奖励 | (init未设🟡) | — | 140 | ✅效果 |
| 53 | RICEBALL | **Megumu**(5面) | **整个充能周期每帧全屏清弹+激光**;按power分支(充碎片/三连按) | 5400f/90s | 持续 | 280 | 🟡power分支 |

## B. 能力类——死亡/决死钩子

| id | 内部名 | 角色 | 效果 | 消耗 | 真救命? | 价 | 可信 |
| --- | --- | --- | --- | --- | --- | --- | --- |
| 23 | AUTOBOMB | Eirin | 决死结算:有ROKUMON且金≥200则让位;否则花炸弹清屏复活(返回1) | **2 炸弹**(剩1则1) | ✅取消死亡 | 350 | ✅ |
| 35 | ROKUMON 六文 | ShikiEiki | 金≥200则**扣200金**清屏复活(返回1) | **200 金** | ✅取消死亡 | 140 | ✅ |
| 24 | DBOMBEXTEND | Tewi | `before_deathbomb`把**决死窗口延到15帧**;死亡时**保住金钱** | 无 | ❌延窗+保金 | 100 | ✅ |
| 27 | KOISHI | Koishi | **不撞敌机本体**(`__on_tick_2`置 ENEMY_MGR+0x164=1)+ 死亡时少扣power(−50,下限100) | 无 | ❌减损失/免撞机体 | 100 | ✅(社区解明) |
| 31 | DEAD_SPELL | Kaguya | 死亡frame2 喷 **3个符卡(炸弹)道具**(type-7=符卡道具,非命碎片)——安慰奖 | 无 | ❌死亡掉落 | 100 | ✅(社区订正) |
| 32 | POWERMAX | Mamizou | **获得时火力+1.0**;死亡时**保power**(封顶3×min=3.00,不跌破) | 无 | ❌保power | 350 | ✅ |

## C. 能力类(永续被动)/ 即时类(获得即生效)

| id | 内部名 | 角色 | 类别 | 效果 | 数值 | 价 | 可信 |
| --- | --- | --- | --- | --- | --- | --- | --- |
| 1 | EXTEND | Life | 即时 | +1 残机 | — | 50 | ✅ |
| 2 | BOMB | Bomb | 即时 | +1 炸弹 | — | 0 | ✅ |
| 3 | EXTEND2 | LifeFragment | 即时 | +1 残机碎片 | — | 0 | ✅ |
| 4 | BOMB2 | BombFragment | 即时 | +1 炸弹碎片 | — | 0 | ✅ |
| 5 | PENDULUM | **Nazrin** | 即时 | 获得即 **+50 金**(MONEY+_DAT_004ccd30 同加) | +50金 | 0 | ✅ |
| 6 | DANGO | Ringo | 即时 | **+50 power碎片(=+0.50火力)**,跨档则重建option | +0.50P | 0(掉落) | ✅ |
| 7 | MOKOU | Mokou | 即时 | **+3 残机**,残机上限抬到7 | +3命 | 450 | ✅ |
| 33 | YUYUKO | Yuyuko | 能力 | **擦弹时有时消去弹幕**(graze-erase;`__on_tick_2`置 BULLET_MGR+`0x7a41f0`=1 使能)。社区"×1.8"❌证伪(那是MUKADE) | — | 240 | ✅(社区解明) |
| 34 | MONEY | Yachie | 能力 | **敌人掉落额外金钱**(recharge 散射**金钱道具** type2;`FUN_00446b00` 证 type2 收集=`MONEY+1`)| — | 140 | ✅(社区订正:金钱非火力)（卡名 = **鬼杰组长的恐吓**、归属 = 吉吊八千慧 —— **2026-09-13 用户看卡图确认**（`ability/MONEY_max.png`）） |
| 36 | NARUMI | Narumi | 能力 | 获得时 **+1命,且每过一关 +1命碎片**(on_load 分支 DAT_004cd5f4;析构−1命) | — | 140 | ✅(社区解明) |
| 37 | PACHE | Patchouli | 能力 | 获得时 **+1炸弹,且每过一关 +1符卡**(钳到MAX_BOMBS) | +1炸/关 | 100 | ✅(社区补全) |
| 21 | ITEM_CATCH | **Nitori** | 能力/装备 | 设玩家 +0x47988~94 = {10,30,110,110}(道具吸收/范围参数) | — | 100 | ✅效果/🟡语义 |
| 22 | ITEM_LINE | **Kanako** | 能力 | 设玩家 +0x47998=224.0（**回收线下移**：默认 128，y 越大越靠屏幕下方）+ GUI flag `gui+0x194` bit 0x2000 —— 只负责把那条线画出来（`FUN_0043a8b0` 起 script `0x39`，y = `poc_height×2 − 80`）。★ 卡里的季节点每帧读这个字段 ⇒ 跟着变 | 224.0 | 100 | ✅ |
| 25 | MAINSHOT_PU | **Saki** | 装备/能力 | **弱肉強食の理**:对 shot type7 的弹 **伤害×1.4、尺寸×1.5、速参×1.5**(代码确认 ×1.4!) | ×1.4伤/×1.5尺 | 180 | ✅✅ |
| 26 | MAGICSCROLL | **Byakuren** | 能力 | **魔法卷物**:设 mgr+0xc58=0.8 = **所有主动卡充能−20%**;另置 BOMB+0xa4=1(炸弹无敌延长) | 充能×0.8 | 300 | ✅ |
| 28 | MAINSHOT_SP | **Suwako** | 装备/能力 | 对 shot type7 的弹 **1/8 概率**改命中回调为**爆炸**(伤害源 type6 power20) | 1/8爆炸,伤20 | 140 | ✅ |
| 29 | SPEEDQUEEN | **Aya** | 能力 | **停射时超高速移动**;移动中**判定极小**,起步瞬间**无敌**(代码 speed/分数域=此机制)| — | 100 | ✅(社区解明) |
| 30 | OPTION_BR | **Keiki** | 装备 | 4 个固定 option 槽,每 **360帧(6s)** 在 option 位**全屏式清弹**一次 | 4槽×6s清弹 | 350 | ✅（效果一手；卡名 = **偶像亲卫队**、归属 = **袿姬** —— 2026-09-13 用户看卡图确认（`ability/OPTION_BR_max.png`）。⚠ 卡扩展 §AL25 的彩蛋**不**用这张，它比 25 或 **34**） |
| 19 | OKINA_OP | Okina | 装备 | option(+36px)每帧 **20×6 矩形清弹** | 矩形20×6 | 280 | ✅ |
| 20 | NUE_OP | Nue | 装备 | option(−60px,带轨道角)每帧 **半径清弹**(cancel_radius_as_bomb) | 半径∞ | 200 | ✅ |
| 51 | MAGATAMA 勾玉 | **Misumaru**(4面boss) | 装备 | 生成**两个 option(±60px)**,每帧各在 option 位**半径清弹** | 双option半径清 | 240 | ✅ |
| 54 | MUKADE 暴食蜈蚣 | **Momoyo**(EX boss) | 能力 | **暴食のムカデ**:on_bullet_init 把自机弹伤害 ×`min(1.8, 击杀/20000 +1.0)`(渐增至**×1.8**);on_draw 画当前倍率 | ×1.0→1.8(2万杀满) | 300 | ✅✅ |
| 55 | MAGATAMA2 | Magatama(Misumaru EX) | 能力 | **EX 特有"天蓝色勾玉"**:**必须装备方可进 EXTRA 关·非EX关不可装**(入关自动携带,不占初始槽) | — | 140 | ✅(社区解明) |
| 0 | BLANK | Chimata | 即时 | 空白卡:获得即**弃掉当前所有卡**,下个商店**全财产换稀有卡**(引擎侧 `card_exists(0)` @ `FUN_00430d30`,非 vtable)| — | 0 | ✅(社区+引擎核实) |

## D. 装备类——各角色自机射击子机(on_power→Player__allocate_option;on_shoot→tick_shooters,逐卡 SHT 索引)
> 子弹实际数据在 SHT shooter 表——**存储已解决(2026-09-05)**:`pl0X.sht` 的 `+0xe0` 偏移数组第 `0x0a`–`0x16` 项,
> 见 [`../../sht/th18/01-file-layout-and-shooterset-index.md`](../../sht/th18/01-file-layout-and-shooterset-index.md)。
> 此处只给 SHT 索引 + 子机位 + 价。

| id | 内部名 | 角色 | SHT索引 | 子机位偏移 | 特殊 | 价 |
| --- | --- | --- | --- | --- | --- | --- |
| 8 | REIMU_OP | Reimu1 | 0xa | +0x30 | — | 240 |
| 9 | REIMU_OP2 | Reimu2 | 0x12 | −0x18 | — | 280 |
| 10 | MARISA_OP | Marisa1 | 0xb | +0x10 | — | 240 |
| 11 | MARISA_OP2 | Marisa2 | 0x13 | +0x20 | — | 280 |
| 12 | SAKUYA_OP | Sakuya1 | **0xc/0xd 按聚焦** | +0x10 | 聚焦换表 | 240 |
| 13 | SAKUYA_OP2 | Sakuya2 | 0x14 | +0x20 | — | 280 |
| 14 | SANAE_OP | Sanae1 | 0xe | +0x1c | — | 240 |
| 15 | SANAE_OP2 | Sanae2 | 0x15 | −0x14 | — | 280 |
| 16 | YOUMU_OP | Youmu | 0x11 | +0x1c | 自定义tick(绕行) | 240 |
| 17 | ALICE_OP | Alice | 0xf | +0x1c | **追踪锁敌**:无锁定/出界则不发 | 280 |
| 18 | CIRNO_OP | Cirno | 0x10 | −0x3c | — | 200 |

## 订正(波次 id 错配 → 已校正)
- 子 agent 把若干 `Card<角色>` 函数贴到**错误 card_id**(effect 反的是对的函数,只是 id/价错)。经权威 vtable 链全表重映后已改正:
  Nazrin→5(非34)、Nitori→21、Kanako→22、Saki→25(弱肉強食)、Byakuren→26(魔法卷物)、Suwako→28、Aya→29、Keiki→30、
  Misumaru→51、Tsukasa→52、Momoyo→54。
- **推翻 salvage 一条**:"弱肉強食の理 ×1.4 为社区杜撰、代码无" → **错**;×1.4 在 `CardSaki`(MAINSHOT_PU/id25)**代码确认**。
- **解决 Mukade 之争**:×1.8 渐增 = MUKADE(54)=Momoyo(EX);id52 CYLINDER=Tsukasa 是另一张(炸弹型主动)。

## 待核 / 开放
- 🟡 WARP/CYLINDER 充能 init 机制 · ITEM_CATCH/LINE 玩家字段精确尺度。
- ✅ **社区分歧已全部一手核实**(见 `09-community-crosscheck.md` §5):Yachie=金钱(已订正)· Shinmyoumaru 清弹→金钱(一致)· Chimata 空白卡 & Sannyo 收符卡→命碎片 = 引擎侧 `card_exists(id)` @ `FUN_00430d30`(stub 卡的效果不在 vtable,在子系统按 id 触发)。
> 📌 **stub 卡规律**:`Card<角色>` 仅 operator_delete 的卡(Chimata/Sannyo/Mike/Takane 等),效果在引擎子系统按 `card_exists(card_id)` 触发,不在 vtable。
- ✅ 装备子机的实际弹幕数值 → 表在 `pl0X.sht`,字段图见 [`../../sht/th18/02-shooter-record.md`](../../sht/th18/02-shooter-record.md)(2026-09-05 解决)。
- 资源/即时卡的库存数值见 `06-resource-economy.md`;商店机制见 `05-shop-and-money.md`;注册表见 `07-registry.md`。
