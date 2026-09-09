// 本机（Linux/.NET 8）驱动 Steamless 插件的最小 driver —— 自己写的胶水，不含上游代码。
// 用法: dotnet run -- <packed.exe> [--keepbind] [--realign] [--recalcchecksum]
using System;
using System.Collections.Generic;
using Steamless.API.Events;
using Steamless.API.Model;
using Steamless.API.Services;

namespace SteamlessLinuxDriver
{
    internal static class Driver
    {
        private static int Main(string[] args)
        {
            if (args.Length == 0)
            {
                Console.Error.WriteLine("usage: steamless-linux <packed.exe> [--keepbind] [--realign] [--recalcchecksum] [--quiet]");
                return 2;
            }

            var file = args[0];
            var opts = new SteamlessOptions();
            for (var i = 1; i < args.Length; ++i)
            {
                switch (args[i].ToLowerInvariant())
                {
                    case "--keepbind": opts.KeepBindSection = true; break;
                    case "--keepstub": opts.ZeroDosStubData = false; break;
                    case "--dumppayload": opts.DumpPayloadToDisk = true; break;
                    case "--dumpdrmp": opts.DumpSteamDrmpToDisk = true; break;
                    case "--realign": opts.DontRealignSections = false; break;
                    case "--recalcchecksum": opts.RecalculateFileChecksum = true; break;
                    case "--exp": opts.UseExperimentalFeatures = true; break;
                    case "--quiet": opts.VerboseOutput = false; break;
                }
            }

            var log = new LoggingService();
            log.AddLogMessage += (sender, e) =>
            {
                if (e.MessageType == LogMessageType.Debug && !opts.VerboseOutput)
                    return;
                Console.WriteLine($"[{e.MessageType}] {e.Message}");
            };

            var plugins = new List<SteamlessPlugin>
            {
                new Steamless.Unpacker.Variant31.x86.Main(),
                new Steamless.Unpacker.Variant30.x86.Main(),
            };

            foreach (var p in plugins)
            {
                if (!p.Initialize(log))
                {
                    Console.Error.WriteLine($"initialize failed: {p.Name}");
                    continue;
                }

                Console.WriteLine($"plugin: {p.Name} (v.{p.Version})");
                if (!p.CanProcessFile(file))
                {
                    Console.WriteLine("  -> cannot process this file");
                    continue;
                }

                var ok = p.ProcessFile(file, opts);
                Console.WriteLine(ok ? "RESULT: OK" : "RESULT: FAILED");
                return ok ? 0 : 1;
            }

            Console.Error.WriteLine("no plugin could process the file");
            return 3;
        }
    }
}
