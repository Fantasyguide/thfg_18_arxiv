bin/th18_card_expand.dll   一份，配下面任何一套 patch。放 <thcrap>/bin。

patch-step3/   255 行 + 分配器搬迁 + 存档影子数组 + 文案 / 图鉴 / 编成 / 商店。
               通过 = th18_card_expand.log 末尾 "OK: table filled (255 rows …), 100/100 sites verified"。
               自带本 mod 的卡池 th18/cards.js（id 58–76，各有行为，开局即解锁）。
               日志里应有 "cards: 19 registered from cards.js"；图鉴 / 编成 / 商店里都能看到，能买、能拿。
patch-test/    开发环境，叠在 step3 之上：th18/cards_dev.js 指定起手卡组并追踪每个钩子第一次被调。
               钩子部分：卡组编成里把前几格清空，开一局 →
               "trace: allocate_new_card(id=…, mode=1)  <- NEW ID"。
patch-devstage/ 开发用空壳关卡：st01–st06 道中全删、boss 血量 ÷100、boss 死时多掉 300 金。
               只想快速刷商店 / 试卡时勾；想打正常弹幕就别勾。不依赖别的 patch。
