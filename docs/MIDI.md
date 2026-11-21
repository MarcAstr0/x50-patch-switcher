# X50 MIDI facts

Everything the app sends to or reads from a KORG X50, with the source of each fact.
Where this file and an agent's memory disagree, this file wins. Where this file and the
Korg documents disagree, the Korg documents win and this file gets fixed.

## Sources (on the owner's machine, not in the repository)

| Short name | File | What it is |
|---|---|---|
| **OG** | `~/Documentos/Manuales/Korg/X50_OpGuide_E1.pdf` | Operation Guide, 132 pp. Bank select table PDF p. 46-47, MIDI Implementation Chart PDF p. 128. Page numbers in these docs are PDF pages, not printed ones (printed = PDF - 6) |
| **IMP** | `~/Documentos/Manuales/Korg/X50_microX_MIDIimp.txt` | MIDI Implementation v1.0 (9 Mar 2006), the byte-level authority |
| **VNL** | `~/Documentos/Manuales/Korg/X50_VNL_EFGJ1.pdf` | Voice Name List: factory combinations, programs, GM banks, drum kits |
| **ED** | `~/Documentos/Manuales/Korg/microX50Editor_OM_E2.pdf` | KORG's own editor; useful for what is possible over USB |

The Korg documents are copyrighted and stay out of git. Agents read them by path. Line
numbers below refer to IMP after `tr -d '\r'`.

## 1. Connection

- The X50's USB-B port is a USB-MIDI device. On Windows the OS's standard USB-MIDI
  driver loads automatically; the KORG USB-MIDI driver is optional (OG PDF p. 35, "Installing the
  KORG USB-MIDI Driver", step 7). Linux and macOS: class compliant, expected to need no
  driver. **Not yet verified on hardware.**
- The USB port carries MIDI only, no audio (OG PDF p. 20).
- The owner does not have the unit. The support keyboard player has it, with a Windows
  laptop. Every hardware fact marked *unverified* below is settled at gate G1.

## 2. The global channel

Program and Combination mode receive program changes and bank selects on the **global
MIDI channel** (OG PDF p. 46, GLOBAL 1: MIDI "MIDI Channel", factory default 1). All Korg
SysEx carries it too: header `F0 42 3g 7A`, where `g` = global channel − 1 (IMP 3.).

The app can **discover** the global channel without asking: the Device Inquiry reply's
third byte is `0g` (IMP 1-4-1), and every Korg SysEx reply carries `3g`.

## 3. Selecting a sound

Send, on the global channel `n`, in this order:

1. If the target's mode differs from the mode the app believes the X50 is in: **Mode
   Change** SysEx (§4), then wait for the ACK (`F0 42 3g 7A 23 F7`) or 100 ms, whichever
   comes first.
2. `Bn 00 mm` Bank Select MSB
3. `Bn 20 bb` Bank Select LSB
4. `Cn pp` Program Change

The X50 switches when it receives the program change after the bank select (OG PDF p. 46).

### Bank select values (IMP lines 297-310 and *2 at line 152; OG PDF p. 47)

The values depend on the X50's **Bank Map** setting (GLOBAL, "Bank Map": KORG or GM).
The factory default is not stated in the documents; OG PDF p. 47 shows the GM table. The
app reads the actual setting (§5) and never assumes.

| Target | Bank Map = KORG `mm, bb` | Bank Map = GM `mm, bb` | Program change |
|---|---|---|---|
| Program bank A | `00, 00` | `3F, 00` | 0-127 |
| Program bank B | `00, 01` | `3F, 01` | 0-127 |
| Program bank C | `00, 02` | `3F, 02` | 0-127 |
| Program bank D | `00, 03` | `3F, 03` | 0-127 |
| Program bank G (GM) | `79, 00` | `79, 00` | 0-127 (= G001-G128) |
| Program bank g(d) (GM drums) | `78, 00` | `78, 00` | 0-127 (= g(d)001-128) |
| Combination bank A | `00, 00` | `3F, 00` | 0-127 |
| Combination bank B | `00, 01` | `3F, 01` | 0-127 |
| Combination bank C | `00, 02` | `3F, 02` | 0-127 |

Program and combination banks A-C use the **same** bank select values. Which one is
selected is decided by the X50's current mode, never by the bytes. That is why §4 exists.

Bank E (`00/3F, 04`) exists only on the microX. The GM variation banks (MSB 121, LSB
1-9, OG PDF p. 47) are out of scope for v1.

### Gating settings on the X50 (GLOBAL 1: MIDI page)

- MIDI Filter **Program Change** checked, or nothing switches.
- MIDI Filter **Bank** checked, or bank select is ignored and only the program number
  changes within the current bank (OG PDF p. 47, note under the table).
- **Combi (Combination Change)** checked, or in Combination mode a program change on the
  global channel changes the program of a timbre instead of the combination (OG PDF p. 64-65).
- MIDI Filter **Exclusive** checked, or every SysEx in §4-§6 is ignored (OG PDF p. 38;
  chart note *E). Factory default not confirmed. No reply to any SysEx is the symptom, so the app reports "no SysEx reply: check Exclusive" rather than guessing.

## 4. Mode change (IMP (27), line 734)

```
F0 42 3g 7A 4E 0m F7
```

| `m` | Mode |
|---|---|
| 0 | COMBI PLAY |
| 1 | COMBI EDIT |
| 2 | PROG PLAY |
| 3 | PROG EDIT |
| 4 | MULTI |
| 6 | GLOBAL |
| 7F | DEMO (only ever reported, never requested) |

The app sends only `m = 0` and `m = 2`. The X50 answers with ACK `F0 42 3g 7A 23 F7` or
NAK `F0 42 3g 7A 24 cc F7` (cc per IMP *22). **It also sends `4E` by itself when the
player changes mode on the panel**: the app listens for it (follow mode).

## 5. Mode and state (IMP (1) and (32), lines 480 and 794)

Request: `F0 42 3g 7A 12 F7` (the app may send it on channel 0 before it knows `g`; the
X50 answers only on its own `g`, so the app first finds `g` by Device Inquiry, §6).

Reply, 27 bytes. Index 0 is `F0`:

| Index | Value | Meaning |
|---|---|---|
| 0-3 | `F0 42 3g 7A` | header |
| 4 | `42` | function |
| 5 | `mm` | current mode, table in §4 |
| 6 | `06` | marker: global |
| 7 | setting data 1 | bit 0 Local Control; bits 1-2 MIDI Clock; **bit 4 Bank Map (0 KORG, 1 GM)**; bit 5 Power On Mode; bit 6 Ext. Realtime |
| 8 | setting data 2 | **bit 0 Enable Program Change; bit 1 Enable Bank Change; bit 2 Enable Combination Change**; bit 3 Aftertouch; bit 4 Control Change |
| 9 | setting data 3 | memory protect bits |
| 10 | `02` | marker: program mode |
| 11 | program bank | 0-3 = A-D (4 = E, microX) |
| 12 | program number | 0-127 |
| 13 | `00` | reserved |
| 14 | `00` | marker: combination mode |
| 15 | combination bank | 0-2 = A-C |
| 16 | combination number | 0-127 |
| 17 | `00` | reserved |
| 18 | `04` | marker: multi mode |
| 19-21 | | multi (ignored) |
| 22 | `21` | marker: ext. controller |
| 23-25 | | ext. controller (ignored) |
| 26 | `F7` | end |

This one reply gives the app everything the settings checklist needs: mode, Bank Map,
the three enable flags, and the current program and combination. **The layout is
transcribed from IMP and unverified on hardware**: the parser must validate every marker
byte and refuse the reply (reporting the raw bytes) if one is off.

Note that bank G / g(d) cannot be reported in byte 11 (the field is 0-4). After the
player picks a GM program on the panel, the app learns it from the CC0/CC32/PC the X50
transmits, not from this reply.

## 6. Device inquiry (IMP 1-4-1 and 2-4-1)

Request: `F0 7E 7F 06 01 F7` (7F = any channel).

Reply: `F0 7E 0g 06 02 42 7A 00 mm 00 vv nn rr bb F7`

- `42` KORG, `7A` X50/microX family. `mm` member code: **`01` = X50**, `09` = microX.
- `vv.nn.rr` major, minor, release; `bb` build. The X50's latest system is 1.05
  (korg.com X50 downloads, System Updater v1.05).
- `g` = global channel − 1.

A microX answers the same way. v1 identifies it and says "microX is not supported".

## 7. Reading names from the unit (IMP (3), (5), (17), (19), (22), *5)

Factory names come from VNL (`data/x50-factory.tsv`). If the player overwrote factory
slots, only the unit knows the new names. The app reads them on request, never live.

### Requests

| What | Bytes |
|---|---|
| One program bank (128 programs) | `F0 42 3g 7A 1C 1b 00 00 F7`, `b` = 0-3 for A-D (kind 1 = one bank) |
| One combination bank | `F0 42 3g 7A 1D 1b 00 00 F7`, `b` = 0-2 for A-C |
| Global data (category names) | `F0 42 3g 7A 0E 00 F7` |

The byte after the function is `00kk 00bb`: kind in bits 4-5, bank in bits 0-1. The X50
answers with the dump below or NAK `24 cc`.

### Replies

| Reply | Layout after `F0 42 3g 7A` |
|---|---|
| Program dump `4C` | `4C`, `0X` (0 = X50), `kind/bank`, `program no.`, data…, `F7` |
| Combination dump `4D` | `4D`, `00`, `kind/bank`, `combination no.`, data…, `F7` |
| Global dump `51` | `51`, `00`, data…, `F7` |

### Data encoding (IMP *5)

Every 7 internal bytes travel as 8 MIDI bytes. The first MIDI byte of each group carries
the top bits: bit 0 = top bit of internal byte 1, bit 1 = of byte 2, and so on to bit 6.
The next 7 MIDI bytes carry the low 7 bits of internal bytes 1-7. A final short group of
`k` internal bytes is `1 + k` MIDI bytes. Decode with that rule; test it with a
round-trip over random data and over every group length 1-7.

### Record layouts (IMP TABLE 1, 2, 4)

| Record | Size (internal bytes) | Name | Category |
|---|---|---|---|
| Program (TABLE 1) | **444** (offsets 0-443) | bytes 0-15, ASCII 0x20-0x7F, space padded | byte 110, 0-15 |
| Combination (TABLE 2) | **544** (offsets 0-543) | bytes 0-15 | byte 118, bits 0-3 |
| Global (TABLE 4) | n/a | | program category names at 328-583 (16 × 16 bytes); combination category names at 584-839 (16 × 16) |

A bank dump is 128 records back to back, so a program bank is 56 832 internal bytes
(~65 000 MIDI bytes) and a combination bank 69 632 (~79 600). At USB-MIDI speed that is
a few seconds; over a 31 250-baud DIN cable it would be ~20 s per bank.

**Record sizes and offsets are transcribed from IMP and unverified.** The first hardware
gate captures one real program bank and one real combination bank as `.syx` files; the
decoder must reproduce, from those captures, the names VNL lists for the same slots. If
a dump's decoded length is not a multiple of the record size, the app must refuse it and
report, not guess.

## 8. What the X50 transmits on its own (follow mode)

When the player changes things on the panel, on the global channel (IMP 1-1, *2):

- Program or combination change: `Bn 00 mm`, `Bn 20 bb`, `Cn pp`, with the bank values
  of the table in §3 for the current Bank Map.
- Mode change on the panel: `F0 42 3g 7A 4E 0m F7`.
- It may also send `40` / `49` current-buffer dumps when edit pages are entered (IMP
  (16), (18)); the app ignores them.

## 9. What the app never sends

Parameter changes (`41`), write requests (`11`, `1A`, `17`), any dump **to** the X50
(`4C`, `4D`, `50`, `51`, …), Global mode changes, GM System On. The app selects; it
never writes the X50's memory.

## 10. Test recipe without hardware

- A `FakeX50` test double implements §3-§7: tracks mode, Bank Map, enable flags and
  global channel, answers Device Inquiry, Mode and State, Mode Change (ACK/NAK) and bank
  dump requests from a synthetic memory image built with the §7 layouts. Every core and
  MIDI test runs against it.
- Linux: the ALSA `Midi Through` port plus `aseqdump` shows the bytes a real send
  produces (`scripts/midi-smoke.sh`).
- Hardware captures from gate G1 go under `tests/data/captures/` and become fixtures.
