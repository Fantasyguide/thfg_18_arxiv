东方红魔乡 New Classic —— 机器代打插件
==========================================

★ 关注更新 / 反馈问题 ★
  项目主页：https://github.com/Fantasyguide
  作者 GitHub：https://github.com/Renko6626
  作者 QQ：1471850534
后续版本、游戏更新后的适配都会发在上面的 GitHub。用得上的话请点个 Star / Follow，
出问题或有想法直接在 GitHub 提 issue 或 QQ 联系。

这是什么
--------
一个小插件，放进游戏目录即可用。按 C 键进入代打模式，让电脑替你躲弹，也可以用鼠标进行微操。
插件不改游戏本体，状态通过读取内存获得，输出是模拟按键，故用它代打的replay有效。

（但在道德上，请不要用它伪装自己实现了lnn，这种算法脚本很不像人，更像人的部分请关注我的后续开发）


安装
----------------
1. 找到游戏目录（Steam 里右键游戏 → 管理 → 浏览本地文件），里面有 th06nc.exe。
2. 把压缩包里的 VCRUNTIME140_1.dll 复制到这个目录，和 th06nc.exe 放在一起。
3. 启动游戏。进入关卡后，如果画面左上角会出现一行字：MANUAL MODE。证明它装好了。


怎么用
------
进入关卡后按 C 键，在三种模式之间轮换（左上角的字会跟着变）：

  MANUAL MODE  什么都不做，原版相同手动打。
  AUTO MODE    电脑代打，但按住鼠标左键时，自机会朝鼠标的位置走。
  BYPASS MODE  电脑完全接管。

在 AUTO 和 BYPASS 模式下，你的操作会被覆盖掉不起作用。


卸载
----
把游戏目录里的 VCRUNTIME140_1.dll 删掉即可。
（th06nc_autoplay.ini 和 th06nc_autoplay.log 是它生成的配置和日志，一并删掉也没关系。）


常见问题
--------
Q: 进游戏后左上角没有字？
A: 检查文件名是不是正好叫 VCRUNTIME140_1.dll，是不是和 th06nc.exe 在同一层。
   如果游戏目录里有一个 th06nc_autoplay.log，打开看最后几行。

Q: 游戏一启动就报错 0xc0000142？
A: 你的系统里缺 Visual C++ 运行库。安装微软官方的 "Visual C++ Redistributable"
   （x64 版）后再试。

Q: 左上角没有字，日志 th06nc_autoplay.log 里写着"序言与预期不符，永久停用"？
A: 游戏版本和这个插件不匹配（通常是游戏更新了）。等插件更新，或者删掉 DLL。

Q: 它会不会死？
A: 会。它只是个躲弹算法，不是无敌外挂。密集弹幕下它会放炸弹自保，炸弹用完就可能死。

Q: 想调参数？
A: 把压缩包里的 th06nc_autoplay.ini 也复制到游戏目录，用记事本打开，每一行都有说明。
   不放这个文件将采用默认配置。


版本
----
适用：th06nc.exe（Steam 版，MD5 b0cc28ec904e9efce2439ad857c938b5）
项目：https://github.com/Fantasyguide   作者：https://github.com/Renko6626   QQ 1471850534
