# Design brief

For the `designer` agent. Read `docs/SPEC.md` §4 first.

## Who and where

- **User:** the support keyboard player of Chroma Noir, not the owner. She did not ask
  for this app, so it has to be obvious with no training: one-page guide at most.
- **Where:** her Windows laptop, on a keyboard stand or a table beside the X50, on a dark
  stage, viewed at arm's length or further, glanced at between bars while playing.
  Browse view is used at home or at rehearsal, seated, with time.
- **Input:** trackpad, laptop keyboard, possibly a touch screen, possibly a USB
  page-turner pedal (it sends arrow or page keys).

## Priorities, in order

1. **Live view legibility.** The current sound and the next one must be readable in one
   glance from about 1 m. Large type, very high contrast, lit state impossible to miss.
   Assume a 13-15" 1080p screen at 100 % or 125 % scaling.
2. **No accidents on stage.** Big targets with gaps between them, no hover-only
   affordances, nothing that pops up.
3. **Browse view clarity.** Dense is fine; it is a finder. Must not look like a
   spreadsheet.
4. **Character.** A nod to the X50: its dark grey panel, the backlit LCD of the period,
   the KORG-era sans-serif labelling. A nod, not a replica. Live view outranks
   character: if a reference detail costs legibility, drop it.

## Constraints

- Qt 6 Widgets, styled with QSS and QPainter. No QML. Must look right at 1×, 1.25×, 1.5×
  and 2× device pixel ratio.
- Fonts: OFL or equivalent, committed under `assets/fonts/` with their licences. Name
  the exact files.
- **No KORG trademarks**: no KORG logo or logotype, no X50 logotype as drawn on the
  panel. "KORG X50" may appear as plain text, the way a manual prints it.
- One dark theme. No light theme in v1.
- Colour must never be the only signal: the lit button also differs in shape, border or
  weight, so it reads under coloured stage light and for colour-blind users.
- Repaint discipline: designs must allow the live view to repaint only on state change
  (no animated idle states, no pulsing).

## Deliverables

1. `docs/design/REFERENCES.md`: what the X50 panel and LCD actually look like, with
   sources (KORG's product page, Vintage Synth Explorer, review photos). Write down what
   you observed; do not assert a detail you did not verify.
2. `docs/design/DESIGN.md`: every value the builder needs. Colours as hex, fonts and
   sizes, spacing, component states (normal, lit, empty, disabled, pressed, focused),
   the live grid at 6 and 8 slots and how it scales with window size (text must shrink
   to fit a 16-character name, never truncate silently), browse layout and resize
   rules, the display strip, the settings dialog with the probe checklist, the
   Esc-to-leave hint. If the builder has to guess, the document is not finished.
3. `assets/theme/app.qss` with tokens listed as comments at the top; `assets/icons/`
   app icon as SVG + 256 px PNG + `.ico` (Windows needs it first).
4. Mockups as PNG at 1×: `docs/design/mockup-live-8.png`, `mockup-live-6.png`,
   `mockup-browse.png`, `mockup-settings.png`. Built from HTML or SVG sources kept in
   `docs/design/mockup/`, rendered headless (Playwright MCP or Inkscape), never with
   the owner's own Chrome.
