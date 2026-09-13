# tooling/steamless — 在 Linux 上脱 SteamStub 壳

本机（WSL/Linux）没有 Mono 也没有 Wine，而上游 [atom0s/Steamless](https://github.com/atom0s/Steamless)
是 **.NET Framework 4.5.2 + WinForms**。这里用一条最省事的路把它跑起来：

> **不修改上游任何文件** —— 只把它的**跨平台源码**（API + Variant 3.0/3.1 x86 解包器）用
> `Compile Include` 拉进一个 **.NET 8 控制台工程**，另写一个 `Driver.cs` 直接调插件
> （绕开上游 CLI 的 `Plugins/` 目录扫描）。

## 一次性准备

```bash
# 1) 上游源码（CC BY-NC-ND 4.0：只在本地跑，不转发、不发布）
git clone --depth 1 https://github.com/atom0s/Steamless.git local/vendor/Steamless

# 2) .NET 8 SDK 装到 ~/.dotnet（免 sudo）
curl -sSL https://dot.net/v1/dotnet-install.sh | bash -s -- --channel 8.0 --install-dir "$HOME/.dotnet"
```

## 用法

```bash
cd tooling/steamless
~/.dotnet/dotnet build -c Release
~/.dotnet/dotnet bin/Release/net8.0/steamless-linux.dll <包装版.exe> [--keepbind] [--realign]
# → 同目录生成 <包装版.exe>.unpacked.exe
```

⚠️ **别加 `--recalcchecksum`**：那一步走 `imagehlp.dll`（Windows 专有），在 Linux 上会抛
`DllNotFoundException` —— **脱壳文件在此之前已经落盘**，checksum 对静态分析也无意义。

## 判据（怎么确认脱壳成功）

| 指标 | 包装版 | 脱壳后 |
| --- | --- | --- |
| 文件大小 | 原版 + `.bind`（TH16: 812,400 = 683,520 + 128,880）| 原版大小（683,520）|
| `.text` 熵 | ≈ 8.000（加密）| ≈ 6.3–6.5（正常代码）|
| 入口点 | 落在 `.bind` 段内 | 回到 `.text` |
| 段表 | 多一个 `.bind` | 无 `.bind` |
| 符号对账 | — | 社区符号表（如 th-re-data）的函数地址逐一对上序言 |

**md5 与原生版不会相同**：Steamless 会重建 PE 容器（零化 DOS stub、重排段头、不重算 checksum），
**代码段字节才是原生的** —— 用「符号地址对账」判等，别用 md5 判等。

## 仅 Variant 3.0 / 3.1 x86

本工程只编了这两个插件（够用：Steam 上 ZUN 作品都是 3.1 x86）。要别的变体，
在 `steamless-linux.csproj` 里照样加 `Compile Include`，并在 `Driver.cs` 的插件列表里加一行。
