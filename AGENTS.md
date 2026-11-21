# AGENTS.md

Instructions for OpenAI Codex (and any other agent that reads this file). Claude Code
agents read `CLAUDE.md`; the two say the same things.

## What this is

`x50-patch-switcher`: a C++17 / Qt 6 Widgets desktop app that finds and selects
programs and combinations on a KORG X50 over USB-MIDI and SysEx, with a full-screen
live view of 6-8 large buttons. Windows first, Linux and macOS too. Offline. Never
writes to the X50.

## Read first, in this order

1. `docs/SPEC.md`: behaviour and architecture.
2. `docs/MIDI.md`: every byte, with its source in Korg's MIDI Implementation.
3. `docs/PLAN.md`: tasks with acceptance criteria. A change is judged against the
   criteria of the task it implements.

## Hardware facts that are not negotiable

- Global channel for everything; SysEx header `F0 42 3g 7A`.
- Program and combination banks share bank select values; the X50's mode decides. Mode
  Change `F0 42 3g 7A 4E 0m F7` (m 0 = COMBI PLAY, 2 = PROG PLAY) precedes the bank
  select when the target's kind differs from the believed mode.
- Bank select MSB `00` (Bank Map KORG) or `3F` (GM) for banks A-D, LSB = bank index;
  `79,00` for G and `78,00` for g(d) in both maps. Order: CC0, CC32, PC.
- Mode and State reply: 27 bytes, layout in `docs/MIDI.md` §5; Bank Map is byte 7 bit 4;
  enable Program / Bank / Combination change are byte 8 bits 0 / 1 / 2.
- Dump data uses Korg's 7-in-8 encoding (`docs/MIDI.md` §7); program records are 444
  bytes, combination records 544, names in bytes 0-15.
- Never send parameter changes, write requests or dumps to the X50.

## When you are asked to review

You are the independent second reviewer next to a Claude-based one; you never see its
findings and it never sees yours. Report defects, not preferences. For each finding:
severity (blocker, major, minor), file and line, what is wrong, a concrete input or
sequence that triggers it, and the fix. Look hardest at:

- wrong bytes, ordering or channel; Mode Change sent or omitted wrongly;
- parsers that accept malformed or truncated SysEx instead of rejecting it with the raw
  bytes; the 7/8 codec at group boundaries and short tails;
- threading: RtMidi callbacks reach Qt only through a queued signal; nothing blocks the
  GUI thread on MIDI;
- live view input: accidental exit, key auto-repeat, empty slots, re-send of current;
- error paths: port busy (Windows WinMM is exclusive), port vanishes, no SysEx reply,
  NAK, timeouts, malformed `library.json`, missing config directory;
- persistence: atomic writes, debounce, recents cap of 50;
- footprint from `docs/SPEC.md` §5;
- tests that prove something easier than the acceptance criteria.

Say "no defects found" for an area you checked and found clean. End with one line: ship
or do not ship, and why. No praise, no restating the diff.

## Conventions, if you are ever asked to change code

C++17, Qt 6.4 API floor, Qt Widgets only, no QML, no new dependencies, no network.
One class per `.h/.cpp` pair under `src/<module>/`. `QStringLiteral` for literals;
`QT_NO_CAST_FROM_ASCII` is on. Every `core/` and `midi/` behaviour has a Qt Test.

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
ctest --test-dir build --output-on-failure
```

No KORG trademarks under `assets/`; `assets/local/` is never committed. No em dashes in
user-facing strings or docs.
