#!/usr/bin/env bash
# Footprint check against docs/SPEC.md section 5 (Linux, headless, no root).
# Starts the built app (offscreen, --fake-midi, temp config dir) in browse view and in live view
# (X50_START_LIVE=1 test hook), then reports RSS, CPU over an idle window, threads and wakeups.
# Usage: scripts/measure-footprint.sh [path-to-exe]   (env: IDLE_SECS=10 SETTLE_SECS=3)
# Limits: RSS < 60 MB (SPEC 5), idle CPU 0 % (allowed here: <= 1 % of one core over the window,
# i.e. 10 ticks of 10 ms in 10 s, to absorb the one-off startup tail), startup to first-ready
# reported only (SPEC 5 states no startup limit).
set -u
export LC_ALL=C
root="$(cd "$(dirname "$0")/.." && pwd)"
exe="${1:-$root/build/x50-patch-switcher}"
idle="${IDLE_SECS:-10}"
settle="${SETTLE_SECS:-3}"
rss_limit_kb=$((60 * 1024))
cpu_limit_ticks=$((idle * 100 / 100))   # 1 % of one core
[ -x "$exe" ] || { echo "not executable: $exe" >&2; exit 2; }
hz=$(getconf CLK_TCK)
export QT_QPA_PLATFORM=offscreen
fail=0
printf '%-8s %9s %9s %8s %8s %9s %s\n' view "RSS(MB)" "HWM(MB)" "CPU(ms)" threads "wakeups" "startup(ms)"

measure() {
  local view="$1" cfg pid t0 t1 ready=-1 s0 s1 c0 c1 rss hwm thr ctx0 ctx1
  cfg="$(mktemp -d)"
  [ "$view" = live ] && export X50_START_LIVE=1 || unset X50_START_LIVE
  t0=$(date +%s%N)
  "$exe" --fake-midi --config-dir "$cfg" >/dev/null 2>&1 &
  pid=$!
  # Startup: first moment the process has threads and VmRSS and stays alive (window shown).
  while kill -0 "$pid" 2>/dev/null; do
    if grep -q '^VmRSS' "/proc/$pid/status" 2>/dev/null; then
      sleep 0.2; ready=$(( ($(date +%s%N) - t0) / 1000000 )); break
    fi
    sleep 0.02
  done
  if ! kill -0 "$pid" 2>/dev/null; then echo "$view: app exited early" >&2; rm -rf "$cfg"; fail=1; return; fi
  sleep "$settle"
  cpu() { awk '{print $14+$15}' "/proc/$pid/stat"; }
  ctx() { awk '/ctxt_switches/{s+=$2} END{print s+0}' /proc/$pid/task/*/status; }
  c0=$(cpu); ctx0=$(ctx)
  sleep "$idle"
  c1=$(cpu); ctx1=$(ctx)
  rss=$(awk '/^VmRSS/{print $2}' "/proc/$pid/status")
  hwm=$(awk '/^VmHWM/{print $2}' "/proc/$pid/status")
  thr=$(awk '/^Threads/{print $2}' "/proc/$pid/status")
  kill "$pid" 2>/dev/null; wait "$pid" 2>/dev/null; rm -rf "$cfg"
  local cpu_ms=$(( (c1 - c0) * 1000 / hz ))
  printf '%-8s %9.1f %9.1f %8d %8d %9s %s\n' "$view" \
    "$(awk "BEGIN{print $rss/1024}")" "$(awk "BEGIN{print $hwm/1024}")" \
    "$cpu_ms" "$thr" "$(( (ctx1 - ctx0) / idle ))/s" "$ready"
  if [ "$rss" -ge "$rss_limit_kb" ]; then echo "FAIL $view: RSS ${rss} kB >= ${rss_limit_kb} kB" >&2; fail=1; fi
  if [ $((c1 - c0)) -gt "$cpu_limit_ticks" ]; then echo "FAIL $view: idle CPU $((c1 - c0)) ticks over ${idle}s" >&2; fail=1; fi
}
measure browse
measure live
echo "limits: RSS < $((rss_limit_kb / 1024)) MB, idle CPU <= 1 % of one core over ${idle}s; wakeups = context switches per second (all threads); no startup limit in SPEC 5"
[ "$fail" -eq 0 ] && echo "footprint: OK" || echo "footprint: OVER LIMIT"
exit "$fail"
