#!/usr/bin/env bash
# Checks the bytes `x50-patch-switcher --send` really puts on a MIDI port, using the ALSA
# "Midi Through" port and aseqdump. No hardware needed. Linux only.
#
# Nothing answers on Midi Through, so the app cannot confirm an X50: it works in manual
# mode (channel from --channel, Bank Map GM) and the mode is unknown, so the Mode Change
# SysEx is always sent first and its ACK wait times out after 100 ms (docs/MIDI.md 3).
# The expected bytes below are typed by hand from docs/MIDI.md:
#   section 6: Device Inquiry request    F0 7E 7F 06 01 F7   (sent by the probe first)
#   section 4: Mode Change               F0 42 3g 7A 4E 0m F7  (m = 0 COMBI PLAY, 2 PROG PLAY)
#   section 3: GM Bank Map table         MSB 3F (banks A-D), 79 (G), 78 (g(d)); LSB 00-03
#
# Usage: scripts/midi-smoke.sh [path-to-exe]     (default build/x50-patch-switcher or $X50_EXE)
# Exit: 0 all match, 1 a mismatch or a failed send, 77 skipped (missing tool or port).
set -u

root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
exe="${1:-${X50_EXE:-$root/build/x50-patch-switcher}}"

skip() { echo "SKIP: $1"; exit 77; }
command -v aseqdump >/dev/null 2>&1 || skip "aseqdump not found (package alsa-utils)"
[ -x "$exe" ] || skip "executable not found: $exe (build it first, or pass its path)"
aseqdump -l 2>/dev/null | grep -q "Midi Through" || skip "no ALSA 'Midi Through' port (modprobe snd-seq-dummy)"

tmp="$(mktemp -d)"
dump_pid=""
cleanup() { [ -n "$dump_pid" ] && kill "$dump_pid" 2>/dev/null; rm -rf "$tmp"; }
trap cleanup EXIT

# Turns aseqdump lines into plain hex bytes, one message per line. aseqdump prints
# channels 0-based and values in decimal.
canon() {
  awk '
    /System exclusive/ { sub(/^.*System exclusive[ ]+/, ""); print; next }
    /Control change/   { gsub(/[,]/, ""); printf "B%X %02X %02X\n", $4, $6, $8; next }
    /Program change/   { gsub(/[,]/, ""); printf "C%X %02X\n", $4, $6; next }
  '
}

failures=0
# check <name> <address> <channel> <expected bytes, one message per line>
check() {
  local name="$1" addr="$2" ch="$3" expected="$4" out="$tmp/dump.txt" rc
  : > "$out"
  aseqdump -p "Midi Through" > "$out" 2>&1 &
  dump_pid=$!
  sleep 0.5
  "$exe" --send "$addr" --port "Midi Through" --channel "$ch" >"$tmp/send.txt" 2>&1
  rc=$?
  sleep 0.3
  kill "$dump_pid" 2>/dev/null; wait "$dump_pid" 2>/dev/null; dump_pid=""
  local got
  got="$(canon < "$out")"
  if [ "$rc" -ne 0 ]; then
    echo "FAIL $name: --send exited $rc"; sed 's/^/    /' "$tmp/send.txt"; failures=$((failures + 1)); return
  fi
  if [ "$got" == "$expected" ]; then
    echo "ok   $name"
  else
    echo "FAIL $name: bytes differ"
    echo "  expected:"; echo "$expected" | sed 's/^/    /'
    echo "  got:";      echo "$got" | sed 's/^/    /'
    failures=$((failures + 1))
  fi
}

check "PROG A000, channel 1" "PROG A000" 1 "F0 7E 7F 06 01 F7
F0 42 30 7A 4E 02 F7
B0 00 3F
B0 20 00
C0 00"

check "COMBI C127, channel 1" "COMBI C127" 1 "F0 7E 7F 06 01 F7
F0 42 30 7A 4E 00 F7
B0 00 3F
B0 20 02
C0 7F"

check "PROG G001, channel 16" "PROG G001" 16 "F0 7E 7F 06 01 F7
F0 42 3F 7A 4E 02 F7
BF 00 79
BF 20 00
CF 00"

check "PROG g(d)128, channel 2" "PROG g(d)128" 2 "F0 7E 7F 06 01 F7
F0 42 31 7A 4E 02 F7
B1 00 78
B1 20 00
C1 7F"

if [ "$failures" -ne 0 ]; then
  echo "$failures check(s) failed"
  exit 1
fi
echo "all checks passed"
