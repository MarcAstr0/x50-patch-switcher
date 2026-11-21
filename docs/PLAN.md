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
- ☐ **T1.1 Factory catalogue.** `scripts/extract-voice-list.py` (reads VNL by path with
  `pdftotext -layout`, writes `data/x50-factory.tsv` per SPEC §3.2). Mind the two-column
  program pages, the three-column GM page, two-line combination rows, and names with
  internal spaces. Test `tests/test_factory_table.cpp`: counts 384 / 512 / 128 / 9;
  spot rows `COMBI A000` = StereophonicKeys (Keyboard), `PROG A000` = Portamento Pad
  (SlowSynth), `PROG A065` = M1 Piano, `PROG G001` = Acoustic Piano; no name over 16
  characters; no duplicate address.
- ☐ **T1.2 Proofread the catalogue** (clerk). Compare every row of the TSV with VNL, page
  by page. Write `docs/proofread-T1.2.md` listing each mismatch (address, TSV value,
  VNL value, page). Edit nothing else. The builder fixes the script from that list;
  the clerk re-checks only the listed rows.
- ☐ **T1.3 `SoundAddress`.** Parse and format per SPEC §3.1; `bankSelect(bankMap)` per
  MIDI.md §3 table; ordering for prev/next. Tests: round trip of all 1 033 addresses;
  every grammar form in SPEC §3.1; rejects `A128`, `COMBI D000`,
  `G000`, `G129`, `E000`; bank-select table for both maps, every row.
- ☐ **T1.4 `Catalogue` and `SearchQuery`.** Factory names, unit-name overlay, *edited*
  flag, categories per kind. Search per SPEC §4.1: at least 25 table cases including
  accents, multi-token AND, address tokens, `cat:`, bare address matching both kinds.
- ☐ **T1.5 `Library` and `SetModel`.** JSON per SPEC §3.3, `QSaveFile`, 500 ms debounce,
  broken-file sidelining, `--config-dir`, portable-mode rule (SPEC §7.2). Set
  operations: create, rename, duplicate, delete, resize 6↔8 (resize to 6 keeps slots
  1-6 and refuses if 7-8 are filled, saying so), move slot, set/clear slot and label.
  Tests in a temp dir for all of it.
- ☐ **T1.6 `X50Protocol`.** Pure functions for every message in MIDI.md §3-§7: builders
  with exact-byte tests; parsers that validate markers and lengths and return errors
  carrying the raw bytes; the 7/8 codec with round-trip tests over every tail length
  1-7 and random data; record slicing for 444- and 544-byte records with name and
  category extraction.
- ☐ **T1.7 `X50Link`, backends and `FakeX50`.** Connect by name (substring), 2 s poll
  while disconnected, probe (inquiry → mode and state) with 1 s timeouts, send with Mode
  Change + ACK-or-100 ms wait, follow mode with 300 ms echo suppression, read names with
  progress, cancel, 10 s per-bank timeout, NAK handling, "port busy" on open failure.
  `FakeX50` answers everything per MIDI.md and can be configured to: be a microX, have
  Bank Map KORG, have each enable flag off, NAK, drop replies, send malformed dumps,
  change mode from its "panel". Tests on the fake for each of those.
- ☐ **T1.8 Command line.** Every flag in SPEC §6 with its exit codes. `--diagnose`
  writes `report.txt` (probe checklist, port list, versions, timings) and one `.syx` per
  bank read. `packaging/windows/Diagnose X50.bat` runs it into
  `%USERPROFILE%\Desktop\X50-diagnose-<date>` and opens that folder.
  `scripts/midi-smoke.sh` checks `--send` bytes against `Midi Through` + `aseqdump`.
- ☐ **T1.9 CI.** GitHub Actions on `windows-latest`, `ubuntu-24.04`, `macos-14`: build,
  test, and on Windows `windeployqt` into a portable zip containing the exe, the Qt
  DLLs, the `.bat` and a short `LEEME.txt`/`README.txt`. Artifact downloadable from the
  run. Proof the exe starts: `--list-ports` and `--probe --fake-midi` on the runner.
- ☐ **R1 Review** (reviewer + Codex, reconciled by the foreman per `docs/FOREMAN.md`).

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
- ☐ **T2.2 `DESIGN.md`** per the brief, complete enough that the builder never guesses.
- ☐ **T2.3 Assets**: fonts with licences, icon set including `.ico`, `app.qss`.
- ☐ **T2.4 Mockups**: live 8, live 6, browse, settings.

**Gate G2 (owner).** Mockups published as an artifact page (or paths given). One round
of changes budgeted. M3 waits for G2. If the owner wants the keyboard player's opinion
on the live mockups, that is the moment to send them to her.

## M3 · Browse view (builder; designer reviews) · after G2

- ☐ **T3.1 Theme, DisplayStrip, screenshot harness.** `--screenshot` with
  `--fake-midi` and a canned library; states `browse`, `browse-search`, `settings`,
  `settings-errors`, `live-8`, `live-6`, `live-disconnected`.
- ☐ **T3.2 BrowseView.** Search, kind toggles, categories, result list and delegate,
  send on Enter / double-click / Play, prev/next, every shortcut in SPEC §4.1.
- ☐ **T3.3 Favourites, recents, edited markers.**
- ☐ **T3.4 Settings and probe checklist.** Ports, Test, Check X50, the checklist with
  per-line fix hints, manual channel and Bank Map when the probe fails.
- ☐ **T3.5 Read names** UI with progress and cancel (after G1 / T1.10).
- ☐ **T3.6 SetEditor.** Set picker and operations, slot grid, drag reorder, labels,
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
