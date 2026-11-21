# Changelog

All notable changes to this project are documented in this file. The format follows
[Keep a Changelog](https://keepachangelog.com/en/1.1.0/).

## [Unreleased]

## [0.9.0] - 2026-11-21 (beta)

### Added

- Factory sound catalogue of the KORG X50: 384 combinations, 512 programs, 128 GM sounds
  and 9 GM drum kits, proofread against the X50 documentation.
- Sound addresses in the X50's own form (`COMBI A000`, `PROG A012`, `PROG G001`), with
  the bank select values for both Bank Map settings.
- Search by name, category or address: several words at once, accent and case
  insensitive, `cat:` for categories.
- Browse view with kind toggles (Combis, Programs, GM), a category list, a result list
  and keyboard shortcuts; highlighting never sends, Enter, double-click or Play does.
- Favourites, recent sounds and an "edited" marker for sounds whose name was changed on
  the X50.
- Sets of 6 or 8 sounds with a set editor: slots with labels, drag to reorder,
  Ctrl+1 to Ctrl+8 to fill a slot.
- Full-screen live view with 6 or 8 large buttons, switched by tap, click, keys 1 to 8,
  arrow or page keys (for foot pedals), and Ctrl+Left/Right for sets; leaving needs a
  held or double Esc.
- Connection to the X50 over USB-MIDI: port picker, probe with a checklist that says
  what to change on the X50, and following the X50's panel changes.
- Command line with `--list-ports`, `--send`, `--probe`, `--diagnose` (a report folder
  for troubleshooting), `--screenshot` and a simulated X50 (`--fake-midi`).
- Windows portable build (icon, version information, portable mode with a `portable`
  file), plus Linux AppImage and macOS dmg builds from GitHub Actions.
- Display-sleep prevention on Windows while live view is open.
- Small footprint: about 45 MB of memory and idle CPU use of 0.1 % or less on the Linux
  test build.
