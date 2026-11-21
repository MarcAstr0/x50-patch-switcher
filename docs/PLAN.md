# Plan

Milestones in order. Each task names its agent (see `.claude/agents/`), its acceptance
criteria, and whether it ends at a **gate** where the owner (Mario) has to act. The
foreman (`docs/FOREMAN.md`) dispatches tasks, verifies, commits and ticks the boxes.
Nothing is done until the foreman ticks it after running the checks.

Legend: ☐ not started · ◐ in progress · ☑ done · ⛔ blocked (say why in the Decisions log).

Agents: **builder** (Sonnet) writes all code. **designer** (Sonnet) does everything
visual. **reviewer** (Sonnet) reviews, read-only. **clerk** (Haiku) does bounded,
mechanical, checkable work: proofreading data, drafting docs from a template, capturing
screenshots. The clerk never writes application code. Codex is the independent second
reviewer (`scripts/codex-review.sh`).

**The owner does not have an X50.** Hardware gates go through the keyboard player, who
has the unit and a Windows laptop. Each one must be a single download plus a
double-click, with results she can send back as a folder.

## M0 · Scaffold ☑ (2026-11-21, Opus, interactive with the owner)

- ☑ Repository, docs: `SPEC.md`, `MIDI.md`, `DESIGN-BRIEF.md`, this plan,
  `FOREMAN.md`, `CLAUDE.md`, `AGENTS.md`, agent files, `scripts/codex-review.sh`,
  `docs/claude-settings.example.json`.
- ☑ Korg MIDI Implementation located and filed in `~/Documentos/Manuales/Korg/`;
  `docs/MIDI.md` transcribed from it with line references.

## Gate G0 (owner, before T1.9) · questions and a remote

The owner asks the keyboard player, and creates the GitHub repository. M1 starts
without waiting; only T1.9 (CI) needs the remote.

1. Does she play **programs, combinations or both** live? (If one only, the Mode Change
   path still ships but G1 can test it less.)
2. Has she **overwritten factory sounds**? (Decides how much the read-names path matters
   at G1.)
3. Windows version, and does she have, or want, a **page-turner pedal**? Touch screen?
4. **UI language**: English (like the X50 itself) or Spanish? Default: English UI,
   Spanish one-page guide.
5. GitHub repo **public or private** (the D-05 app is public, MIT). Create it and add it
   as `origin`.

## M1 · Core, protocol and command line (builder; T1.2 clerk)

No GUI beyond the scaffold window. Everything testable headless.

- ☑ **T1.0 Build scaffold.** `CMakeLists.txt` with RtMidi 6.0.0 via FetchContent;
  static libs `x50core`, `x50midi`, `x50ui`; the executable; `tests/` with an
  `x50_add_test()` helper; an empty main window; `--list-ports`. Build and `ctest` green
  on the Linux box.
- ☑ **T1.1 Factory catalogue.** `scripts/extract-voice-list.py` (reads VNL by path with
  `pdftotext -layout`, writes `data/x50-factory.tsv` per SPEC §3.2). Mind the two-column
  program pages, the three-column GM page, two-line combination rows, and names with
  internal spaces. Test `tests/test_factory_table.cpp`: counts 384 / 512 / 128 / 9;
  spot rows `COMBI A000` = StereophonicKeys (Keyboard), `PROG A000` = Portamento Pad
  (SlowSynth), `PROG A065` = M1 Piano, `PROG G001` = Acoustic Piano; no name over 16
  characters; no duplicate address.
- ☑ **T1.2 Proofread the catalogue** (clerk). Compare every row of the TSV with VNL, page
  by page. Write `docs/proofread-T1.2.md` listing each mismatch (address, TSV value,
  VNL value, page). Edit nothing else. The builder fixes the script from that list;
  the clerk re-checks only the listed rows.
- ☑ **T1.3 `SoundAddress`.** Parse and format per SPEC §3.1; `bankSelect(bankMap)` per
  MIDI.md §3 table; ordering for prev/next. Tests: round trip of all 1 033 addresses;
  every grammar form in SPEC §3.1; rejects `A128`, `COMBI D000`,
  `G000`, `G129`, `E000`; bank-select table for both maps, every row.
- ☑ **T1.4 `Catalogue` and `SearchQuery`.** Factory names, unit-name overlay, *edited*
  flag, categories per kind. Search per SPEC §4.1: at least 25 table cases including
  accents, multi-token AND, address tokens, `cat:`, bare address matching both kinds.
- ☑ **T1.5 `Library` and `SetModel`.** JSON per SPEC §3.3, `QSaveFile`, 500 ms debounce,
  broken-file sidelining, `--config-dir`, portable-mode rule (SPEC §7.2). Set
  operations: create, rename, duplicate, delete, resize 6↔8 (resize to 6 keeps slots
  1-6 and refuses if 7-8 are filled, saying so), move slot, set/clear slot and label.
  Tests in a temp dir for all of it.
- ☑ **T1.6 `X50Protocol`.** Pure functions for every message in MIDI.md §3-§7: builders
  with exact-byte tests; parsers that validate markers and lengths and return errors
  carrying the raw bytes; the 7/8 codec with round-trip tests over every tail length
  1-7 and random data; record slicing for 444- and 544-byte records with name and
  category extraction.
- ☑ **T1.7 `X50Link`, backends and `FakeX50`.** Connect by name (substring), 2 s poll
  while disconnected, probe (inquiry → mode and state) with 1 s timeouts, send with Mode
  Change + ACK-or-100 ms wait, follow mode with 300 ms echo suppression, read names with
  progress, cancel, 10 s per-bank timeout, NAK handling, "port busy" on open failure.
  `FakeX50` answers everything per MIDI.md and can be configured to: be a microX, have
  Bank Map KORG, have each enable flag off, NAK, drop replies, send malformed dumps,
  change mode from its "panel". Tests on the fake for each of those.
- ☑ **T1.8 Command line.** Every flag in SPEC §6 with its exit codes. `--diagnose`
  writes `report.txt` (probe checklist, port list, versions, timings) and one `.syx` per
  bank read. `packaging/windows/Diagnose X50.bat` runs it into
  `X50-diagnose-<date-time>` on the real Desktop (`GetFolderPath`, fallback `%USERPROFILE%\Desktop`) and opens that folder.
  `scripts/midi-smoke.sh` checks `--send` bytes against `Midi Through` + `aseqdump`.
- ☑ **T1.9 CI.** GitHub Actions on `windows-latest`, `ubuntu-24.04`, `macos-14`: build,
  test, and on Windows `windeployqt` into a portable zip containing the exe, the Qt
  DLLs, the `.bat` and a short `LEEME.txt`/`README.txt`. Artifact downloadable from the
  run. Proof the exe starts: `--list-ports` and `--probe --fake-midi` on the runner.
- ☑ **R1 Review** (reviewer + Codex, reconciled by the foreman per `docs/FOREMAN.md`).

**Gate G1 (owner → keyboard player) · first contact with a real X50.** The foreman
prepares the CI zip and a WhatsApp-ready message in Spanish (in
`docs/gates/G1-message.md`): unzip, plug in the X50, double-click `Diagnose X50.bat`,
send back the folder. When it returns, the foreman: records the WinMM port name and the
probe results in `docs/MIDI.md`, adds the `.syx` captures to `tests/data/captures/`,
and has the builder (task **T1.10**) add tests that decode those captures and match VNL
names for unedited slots. Any mismatch with MIDI.md is fixed in MIDI.md first, then in
code. M3's read-names UI (T3.5) waits for G1; nothing else does.

## M2 · Design (designer) · in parallel with M1

Touches only `docs/design/` and `assets/`. No C++.

- ☑ **T2.1 References** per the brief.
- ☑ **T2.2 `DESIGN.md`** per the brief, complete enough that the builder never guesses.
- ☑ **T2.3 Assets**: fonts with licences, icon set including `.ico`, `app.qss`.
- ☑ **T2.4 Mockups**: live 8, live 6, browse, settings.

**Gate G2 (owner).** Mockups published as an artifact page (or paths given). One round
of changes budgeted. M3 waits for G2. If the owner wants the keyboard player's opinion
on the live mockups, that is the moment to send them to her.

## M3 · Browse view (builder; designer reviews) · after G2

- ☑ **T3.1 Theme, DisplayStrip, screenshot harness.** `--screenshot` with
  `--fake-midi` and a canned library; states `browse`, `browse-search`, `settings`,
  `settings-errors`, `live-8`, `live-6`, `live-disconnected`.
- ☑ **T3.2 BrowseView.** Search, kind toggles, categories, result list and delegate,
  send on Enter / double-click / Play, prev/next, every shortcut in SPEC §4.1.
- ☐ **T3.3 Favourites, recents, edited markers.**
- ☑ **T3.4 Settings and probe checklist.** Ports, Test, Check X50, the checklist with
  per-line fix hints, manual channel and Bank Map when the probe fails.
- ☐ **T3.5 Read names** UI with progress and cancel (after G1 / T1.10).
- ☑ **T3.6 SetEditor.** Set picker and operations, slot grid, drag reorder, labels,
  `Ctrl+1…8`.
- ☐ **D3 Design review** (designer): screenshots of every state at 1×, 1.25× and 2×
  against DESIGN.md; numbered deviations; one fix round, a second at most.
- ☐ **R3 Review** (reviewer + Codex).

## M4 · Live view (builder; designer reviews)

- ☐ **T4.1 LiveView** painting per DESIGN.md: 6 and 8 grids, lit, empty, label + sound
  name, fit-to-width text, top bar.
- ☐ **T4.2 Live input**: click/touch, `1`-`8`, next/previous keys with empty-slot
  skipping and no wrap, re-send on current, set switching, Esc hold / double-Esc,
  no dialogs in live view. Tests drive it with `QTest::keyClick`.
- ☐ **T4.3 Display-sleep inhibit** on Windows while live (SPEC §4.2), no-op elsewhere;
  CI-built, behaviour confirmed at G3.
- ☐ **T4.4 Footprint.** `scripts/measure-footprint.sh` (Linux) and the CI Windows job
  printing working-set size after start; within SPEC §5.
- ☐ **D4 Design review** of live view at 1×, 1.25×, 1.5×, 2×.
- ☐ **R4 Review** (reviewer + Codex), emphasis on input handling and repaint discipline.

**Gate G3 (owner → keyboard player) · beta.** Pre-release `v0.9.0` zip. The foreman
writes `docs/gates/G3-checklist.md` (Spanish, numbered, each step with what she should
see on the X50's display) and the message to send. She runs it at home or rehearsal;
fixes go back to the builder as T5.x tasks.

## M5 · Packaging, guide, v1.0.0

- ☐ **T5.1 Windows polish** (builder): icon and version resources in the exe, portable
  mode, `Diagnose X50.bat` still included, SmartScreen note in the readme.
- ☐ **T5.2 Linux AppImage and macOS dmg** (builder), as in the D-05 project.
- ☐ **T5.3 One-page guide for the keyboard player** (clerk drafts from SPEC §4 and the
  screenshots, in the G0 language; foreman checks every instruction against the built
  app). `docs/GUIDE.md` plus a PDF in the zip.
- ☐ **T5.4 README, CHANGELOG, screenshots at 2×** (clerk, foreman checks).
- ☐ **T5.5** Everything reported at G3.
- ☐ **R5 Final review** (reviewer + Codex).
- ☐ **T5.6** Tag `v1.0.0`, release green.

**Gate G4 (owner + keyboard player) · acceptance** per SPEC §11.

## Decisions log

- 2026-11-21 · Separate app from the Boutique Patch Switcher (different vendor, protocol
  and user). Same stack and workflow. (owner)
- 2026-11-21 · Desktop app, not web: venues have poor connectivity. (owner)
- 2026-11-21 · Live mode of 6-8 big favourite buttons is the core feature. (owner)
- 2026-11-21 · Workers on Sonnet and Haiku only (owner). Designer moves from Opus (D-05
  project) to Sonnet; the brief compensates by being explicit about every deliverable.
- 2026-11-21 · Windows is the primary target and has no local test machine; every
  hardware fact goes through `--diagnose` and the keyboard player.
- 2026-11-21 · G0: GitHub repository `MarcAstr0/x50-patch-switcher` created **public**
  (MIT) and added as `origin`. (owner)
- 2026-11-21 · G0: keyboard player's answers not in yet; proceeding on defaults: both
  programs and combinations, English UI, Spanish one-page guide, no pedal or touch
  screen assumed (arrow and page keys still work). Revisit when the answers arrive.
  (owner)
- 2026-11-21 · Default catalogue order (`SoundAddress::operator<`): COMBI before PROG,
  then bank A, B, C, D, G, g(d), then number. Prev/next steps through the current result
  list (SPEC §4.1), which uses this order when unsorted. (foreman, detail)
- 2026-11-21 · CMake floor raised from 3.22 to 3.24: RtMidi 6.0.0's own CMakeLists
  requires 3.24 (R-T1.0, both reviewers). SPEC §2 updated. (foreman, detail)
- 2026-11-21 · `--list-ports` exits 1 if port enumeration itself fails (SPEC §6 said
  only "exit 0"); an empty list is still 0. (foreman, detail)
- 2026-11-21 · Windows subsystem: the exe is a console app for now so CLI output works.
  Before G3, T1.8/T5.1 must make a double-clicked GUI launch open no console window
  (GUI subsystem plus `AttachConsole` for CLI flags, or a second target). (foreman)
- 2026-11-21 · Search details where SPEC §4.1 is silent (T1.4): NFD fold then lower case;
  free text matches displayed name or category; edited slots search the unit name only;
  `cat:` matches a category prefix **or** the prefix of any `/`-separated part
  (`cat:mallet` finds Bell/Mallet), quoted values allowed; adjacent tokens joined (up to
  3) when they parse as an address; an out-of-range address token matches nothing.
  (foreman, detail)
- 2026-11-21 · R-T1.4 (reviewer + Codex) fixes queued after T1.5: reject split addresses
  atomically (`C D000` must not become text "C" + PROG D000); join tokens only when the
  first is a kind prefix or bank letter; quoted tokens are always text; drop empty
  tokens; `Catalogue` takes the unit's 16 + 16 category names from the global dump
  (MIDI.md §7, TABLE 4) and `categories()` returns them once read; help shows the
  program name. (foreman)
- 2026-11-21 · Library details where SPEC §3.3 is silent (T1.5): unknown JSON fields
  ignored; no version field; ISO 8601 UTC timestamps; `bankMap` "korg"/"gm"; an address
  that no longer parses drops that entry with a load warning, a structural error
  sidelines the file; `moveSlot` onto an occupied slot swaps; `duplicateSet` names the
  copy "<name> copy". (foreman, detail)
- 2026-11-21 · X50Protocol strictness where MIDI.md is silent (T1.6), each to be checked
  against the G1 captures: global dump accepted if decoded length >= 840 (IMP TABLE 4
  runs to 849); a 7/8 final group of one lone header byte, or with header bits set for
  absent bytes, is rejected; program dump byte 5 must be 00; dump number byte exposed,
  not constrained; non-printable name bytes kept raw and flagged; incoming `4E 7F`
  (DEMO) rejected, Mode and State accepts 7F. (foreman)
- 2026-11-21 · G2 (owner): mockups approved with changes: no NEXT state in live view;
  long names break onto two larger lines before shrinking on one; browse results in
  catalogue order (COMBI A-C, PROG A-D, G, g(d), by number). (owner)
- 2026-11-21 · G2 (owner): the display strip keeps the X50's black-on-white LCD look (as
  the D-05 app kept its green-on-black). Browse strip is a full LCD panel; in live view
  only the current-sound readout is a dimmed LCD panel inside the dark top bar, so it
  does not glow on stage. Lit-button cyan unchanged. (owner; placement by foreman)
- 2026-11-21 · R-T1.6: Codex asked to validate Mode and State bytes 19-21 / 23-25 and other
  reserved bits. Declined: MIDI.md §5 says they are ignored, and the layout is unverified,
  so extra strictness risks refusing a real unit's reply; revisit with G1 captures. Dump
  kind now fixes the record count (kind 1 = 128, kind 2 = 1) and X50 program dumps cap
  the bank at D; both done inside T1.7. (foreman)
- 2026-11-21 · G2 typography (owner asked for the X50 LCD look, manual PDF p. 10): LCD
  panels use a dot-matrix face (Pixel Operator, CC0, patched as "Pixel Operator X50" for
  `&` and zero legibility) and the LCD's own layout ("PROG 0:Play", "Bank C",
  "Category 00:Keyboard", inverse "096: Piano &Strings"). Live button names stay
  Atkinson unless the owner picks the pixel-font variant. Category numbers are
  zero-based as on the unit. Open for M3: the factory TSV has category names but not
  their index; the index order must come from the unit's global dump or from VNL, never
  alphabetical. (owner; foreman)
- 2026-11-21 · X50Link details (T1.7): version shown as "major.minor(2 digits)"; sends
  during a probe or a names read are refused, not queued; with the mode unknown, panel
  CC/PC are ignored until a 4E or a probe tells the mode; unplug is detected only on a
  failed send or a backend error (no polling while connected). G1 must confirm that a
  whole bank dump (~65-80 KB) arrives as one SysEx through WinMM. (foreman)
- 2026-11-21 · **G2 passed** (owner): design approved. Live-view button names use the
  dot-matrix face (Pixel Operator X50) too, "option B", chosen by the owner over the
  brief's legibility-first default; the designer folds it into DESIGN.md as the spec.
  (owner)
- 2026-11-21 · R-T1.7 (reviewer, verified in RtMidi 6.0.0 source): WinMM hands each
  filled SysEx buffer to the callback as a separate chunk (no reassembly; ALSA does
  reassemble). Bank dumps of 65-80 KB would arrive in 1 KB pieces. Fix: larger
  `setBufferSize()` before `openPort()` plus F0..F7 reassembly in the backend, tested
  with a fragmenting fake. (foreman)
- 2026-11-21 · CLI exit codes (T1.8), extending SPEC §6: 0 ok; 1 probe check failed /
  diagnose incomplete / port enumeration failed; 2 bad or ambiguous address, bad option;
  3 no matching port; 4 send failed (busy, NAK, lost) or diagnose could not write;
  5 microX answered. A bare `A000` is ambiguous for `--send` (exit 2). `--diagnose`
  enforces a send allow-list (inquiry, Mode and State, dump requests only). (foreman)
- 2026-11-21 · **R-T1.7 fix round (next builder task, blocks G1):** (1) WinMM SysEx
  fragments: `setBufferSize()` before `openPort()` and F0..F7 reassembly in
  RtMidiBackend, test with a fragmenting fake; (2) clear learned mode, Bank Map, channel,
  bank-select bytes and current sound on disconnect/stop/new probe, keep manual fallbacks
  separate; (3) serialise a send that arrives during a Mode Change wait and re-evaluate the
  target mode before CC0/CC32/PC; (4) echo suppression consumes only the expected
  CC0/CC32/PC tuple; (5) on a request timeout, check the port still exists, else go
  Disconnected; (6) ignore a well-formed dump for another bank/kind (late dump after
  cancel or timeout); (7) emit `sendFailed` when a Mode Change write fails or the port
  dies during the wait; (8) connection generation counter drops stale queued messages.
  Reviewer + Codex, reconciled by the foreman. (foreman)
- 2026-11-21 · **R-T1.8 fixes (before G1):** Diagnose X50.bat checks the exe exists
  (message: unzip the whole folder first), branches its final message on
  `report.txt` existing, and uses `[Environment]::GetFolderPath('Desktop')` (OneDrive,
  localised desktop) with `%USERPROFILE%\Desktop` as fallback; DiagnoseRunner writes
  report.txt first and records per-file write errors; Windows arguments via
  `CommandLineToArgvW` (non-ANSI user names); tests for diagnose exit codes 1, 3, 4;
  explicit restrict flag in LoggingBackend. Reviewer's "no diagnose test" claim was wrong
  (`diagnoseFake` exists). (foreman)
- 2026-11-21 · R-T1.8 additions from Codex: `--in-port` alone must be honoured by
  diagnose; port-enumeration failure exits 1, not 3; a microX answer exits 5 for
  `--probe` and `--diagnose` too; check report.txt write/flush and exit 4 on a short
  write; diagnose folder name collision-safe (seconds plus a -N suffix if it exists).
  (foreman)
- 2026-11-21 · T1.9: CI green on windows-latest, ubuntu-24.04 and macos-15 (macos-14 is
  retired on GitHub) in run 44932729199; Windows zip artifact built and its staged exe
  smoke-tested without Qt on PATH. Runners have no ALSA sequencer, so real-MIDI tests
  skip or accept the documented exit-1 path there. MSVC shows 11 Qt 6.8 deprecation /
  truncation warnings (qAsConst, QDateTime ctor, a cast in a test): fix in R1. (foreman)
- 2026-11-21 · Codex on the T1.7/T1.8 fix rounds: "actions @v7 unpublished" rejected
  (run 44932729199 resolved and ran them); "restore macos-14" rejected (image retired;
  macos-15 recorded above). Queued for R1 fixes: after a NAK of a superseded Mode Change,
  re-evaluate the newest send against the unchanged mode instead of failing it; keep all
  unexpired echo tuples (overlapping sends); parse a dump of an unexpected kind and fail
  the step with raw bytes if malformed (ignore only well-formed late dumps); remember the
  exact port names opened and check those on timeout, not the substring. (foreman)
- 2026-11-21 · R1 (reviewer, both high findings verified by the foreman): "do not ship to
  G1". The Windows zip lacked the MSVC runtime (windeployqt: VCINSTALLDIR not set; CI
  stayed green because the runner has the redistributable) and diagnose port pairing
  failed on WinMM names (RtMidi appends " <index>"). Fix round also: refused inquiry
  replies still lead to read-only dump requests, inquiry retried once, CI runs the
  extracted zip and the .bat itself, the queued Codex items and the MSVC warnings. Codex
  R1 hit the account usage limit; one retry after the fix round. (foreman)
- 2026-11-21 · **R1 closed** on the reviewer agent alone: Codex hit the account usage
  limit on the first run and on the one allowed retry (FOREMAN rule). Second fix round:
  MSVC runtime copied from the toolset's own redist (14.52, asserted against
  VCToolsVersion), no vc_redist/dxcompiler/dxil in the zip, exact-first port selection,
  clearer refused-inquiry result. CI green, run 44935392424, artifact
  X50-Patch-Switcher-windows-966817a.zip (13 MB). Ready for G1. (foreman)
- 2026-11-21 · For T5.1: windeployqt bundles Qt6Network.dll with the tls and
  networkinformation plugins although the app links no network module (CLAUDE.md: no
  network code). Find what pulls it in (likely Qt6Svg or the icon engine) and exclude it,
  checking the staged smoke still passes. Not a G1 blocker. (foreman)
- 2026-11-21 · **G1 on hold** (owner): zip X50-Patch-Switcher-windows-966817a.zip and
  the message are ready in docs/gates/; not sent yet. M3 starts meanwhile. (owner)
- 2026-11-21 · Category numbers: neither VNL nor the Operation Guide lists the factory
  category order (only two data points from the LCD figures: COMBI 00 = Keyboard,
  PROG 09 = SlowSynth). Until the unit's global dump has been read (or G1's global.syx
  becomes a fixture), the LCD shows "Category:Keyboard" without a number, and the browse
  category list uses the order of first appearance in the factory catalogue per kind.
  After a read, the unit's 16 + 16 names and their order are used. (foreman)
- 2026-11-21 · T3.1 details: --screenshot sizes are logical (1920x1080 window, PNG is
  1920s x 1080s device px at scale s); screenshot states set the strip state directly
  (deterministic) rather than probing the fake; GM or unlisted categories show
  "Category --:<name>" once unit categories are known. assets/theme/app.qss header still
  lists the retired lcd-bg/lcd-text tokens: designer to fix at D3. (foreman)
- 2026-11-21 · Codex unavailable (ChatGPT usage limit) until 18:18 local. M3 tasks get the
  reviewer agent now; Codex reviews owed for: T3.1 (f60fae9), and every M3 commit after
  it until Codex is back. Run them then, before R3. (foreman)
- 2026-11-21 · R-T3.1 (reviewer: ship; Codex owed). Fix round after T3.2: connection
  message must wrap/elide inside its 40% zone even for one unbroken token (and a test
  that checks the zone); --screenshot always forces offscreen; disconnected ring and LED
  snapped to device pixels (DESIGN §3.5); no QPen construction or relayout() inside
  paintEvent (queue syncScale); test_display_strip at 1.25 and 1.5 plus an "only LCD
  colours inside the panel" check; distinct exit code for a failed screenshot write.
  (foreman)
- 2026-11-21 · Browse details (T3.2), where SPEC §4.1 is silent: Prev/Next do not wrap
  (status line says so); key auto-repeat never sends (no scanning by holding a key);
  when the query changes, the first result is highlighted (highlight never sends; Enter
  sends) so type-then-Enter works; with both kinds on, the category list shows program
  categories; window geometry to be remembered (next fix round). (foreman)
- 2026-11-21 · For D3 (designer), DESIGN gaps found by the builder in R-T3.1/T3.2 fixes:
  toolbar button widths/padding ("Read names" needs 126, DESIGN says 120); app.qss
  min-height/min-width are content-box (+2 px border), kind toggle rule still 88 (DESIGN
  100); result header "28 high" means border + 27; cell padding only on outer edges and
  EDITED pill left-aligned in its column (not written); pixel-font rule 2 should say
  glyphs are placed per character at integer advances (Qt 6.4 FreeType drifts);
  Prev/Next chevron spacing unspecified; Programs toggle check-mark inset. Window layout
  is stored in <configDir>/window.ini. (foreman)
- 2026-11-21 · Settings details (T3.4): Test = apply the chosen ports, reconnect and run
  the read-only probe (SPEC §4.5 "a Test button that sends a probe"); port choices and
  manual channel/Bank Map in <configDir>/settings.ini; the checklist shows all rows as
  running, then the results (no per-stage signal; the probe is two short requests).
  Queued for the T3.4 fix round: shorten the "No reply" message to "No reply from the
  X50" (the hint carries the Exclusive instruction); "Last check" time from Library
  unit.lastSeen. (foreman)
- 2026-11-21 · SPEC change (owner): browse gets "Favourites" and "Recent" entries at the
  top of the category list (SPEC §4.1 updated); they combine with search and the kind
  toggles; Recent is most recent first. Designer adds them to DESIGN §7.6 (T3.3b), then
  the builder implements them. (owner)
