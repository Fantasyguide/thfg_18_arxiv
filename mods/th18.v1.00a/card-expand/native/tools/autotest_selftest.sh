#!/usr/bin/env bash
# 自动测试框架自身的离线自检（不需要游戏；一条命令跑全套）
#
# 用途：CI / 接手时先跑这个 —— 它验证"框架自己的逻辑"没坏：
#   ① 场景编排（10 个场景无"没按下就松开/结束时还按着"）
#   ② WSL→Windows 桥（uv 启 Windows Python，拿回环境表）
#   ③ 判读分类（判据齐 / 有前置无判据=失效 / 无迹象=未触发）
#   ④ 报告链（明细 Markdown + 汇总 Markdown 都能落盘）
# 退出码 0 = 全绿；非 0 = 有红项（逐项打印）。
#
# 用法：
#   bash mods/th18.v1.00a/card-expand/native/tools/autotest_selftest.sh [MODKIT_DIR]
# MODKIT_DIR 缺省取环境变量；给 /mnt/d/... 形式即可（脚本会转成 Windows 路径）。

set -u
HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
MK="${1:-${MODKIT_DIR:-}}"
TMP="$(mktemp -d)"
trap 'rm -rf "$TMP"' EXIT
FAIL=0
ok()   { printf '  ✅ %s\n' "$1"; }
bad()  { printf '  ❌ %s\n' "$1"; FAIL=1; }
head_() { printf '\n=== %s ===\n' "$1"; }

head_ "① 场景编排自检（离线）"
if python3 "$HERE/autoplay.py" --selftest | tee "$TMP/selftest.txt" | tail -1 | grep -q "问题 0 条"; then
  ok "10 个场景编排合法"
else
  bad "场景编排有问题：$(tail -1 "$TMP/selftest.txt")"
fi

head_ "② WSL→Windows 桥（uv 启动 Windows 侧 runner）"
if [ -n "$MK" ]; then
  if python3 "$HERE/autoplay.py" --win-smoke --modkit "$MK" > "$TMP/bridge.txt" 2>&1 && grep -q '"ok": true' "$TMP/bridge.txt"; then
    ok "桥通了（$(grep -o '"python": "[^"]*"' "$TMP/bridge.txt" | head -1)）"
  else
    bad "桥自检失败（给对 MODKIT_DIR 了吗？）：$(tail -2 "$TMP/bridge.txt")"
  fi
else
  printf '  ⚠ 没给 MODKIT_DIR ⇒ 跳过桥自检（其余项照跑）\n'
fi

head_ "③ 判读分类（失效 vs 未触发）"
printf '=== th18_card_expand test ===\nyoukai: run done - 3 familiars spawned, 1 killed, 0 withdrew\n' > "$TMP/pass.log"
printf '=== th18_card_expand test ===\nyoukai: native option slot 1 claimed\n' > "$TMP/fail.log"
printf '=== th18_card_expand test ===\nsnd: OK 4 voices\n' > "$TMP/none.log"
python3 "$HERE/verify.py" familiar --check --log "$TMP/pass.log" > "$TMP/o1.txt" 2>&1
python3 "$HERE/verify.py" familiar --check --log "$TMP/fail.log" > "$TMP/o2.txt" 2>&1
python3 "$HERE/verify.py" familiar --check --log "$TMP/none.log" > "$TMP/o3.txt" 2>&1
grep -q "必需判据齐" "$TMP/o1.txt" && ok "判据齐 ⇒ 通过" || bad "判据齐的情形判错了"
grep -q "分类：\*\*失效" "$TMP/o2.txt" && ok "有前置、判据没中 ⇒ 失效" || bad "该判「失效」却判成别的"
grep -q "分类：\*\*未触发" "$TMP/o3.txt" && ok "无迹象 ⇒ 未触发" || bad "该判「未触发」却判成别的"

head_ "④ 报告链（明细 + 汇总）"
python3 "$HERE/autoplay.py" --all --check-only --log "$TMP/fail.log" > "$TMP/all.txt" 2>&1
if [ -f "$TMP/report_familiar.md" ] || ls "$TMP"/report_*.md >/dev/null 2>&1; then
  ok "明细报告已落盘（$(ls "$TMP"/report_*.md 2>/dev/null | wc -l) 份）"
else
  bad "明细报告没写出来"
fi
if [ -f "$TMP/summary.md" ]; then
  ok "汇总报告已落盘（$(grep -c '^| [0-9]' "$TMP/summary.md" 2>/dev/null) 行场景）"
else
  bad "汇总报告没写出来"
fi

head_ "结论"
if [ "$FAIL" -eq 0 ]; then
  printf '✅ 框架离线自检全绿（真跑仍需一次人工确认注入是否生效）\n'
else
  printf '❌ 有红项，见上（不要拿它去跑真跑）\n'
fi
exit "$FAIL"
