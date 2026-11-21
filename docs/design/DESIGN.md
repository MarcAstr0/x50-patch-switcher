# Design

Authority for everything visual. Behaviour is in `docs/SPEC.md` §4; research is in
`docs/design/REFERENCES.md`. Where SPEC is silent on something visual, this document
decides, and the decision is marked **[decided]**. All colours were chosen here, not
measured from hardware; the owner approves them on the mockups (REFERENCES §4.10).

Conventions

- `px` means device-independent (logical) pixels as Qt reports them. Qt maps them to
  device pixels at the screen's scale factor. Never multiply by the scale factor
  yourself except in the border-rounding rule (§3.5).
- `u` is the **live unit** (§5.1). `24u` means 24 design pixels times `u`.
- Hex colours are sRGB. Contrast ratios are WCAG 2.x relative-luminance ratios,
  computed for this document with the formula, not estimated.
- No em dashes anywhere in user-facing strings. Ellipsis character `…` is allowed.

---

## 1. Colour tokens

One dark theme. Tokens are the single source: `assets/theme/app.qss` lists them in a
comment block; `Theme` (C++) holds the same hex values as constants for QPainter code.
Same names in both.

### 1.1 Palette

| Token | Hex | Use |
|---|---|---|
| `bg-live` | `#0B0D0F` | live view window background |
| `bar` | `#12171B` | live top bar, bottom hint strip background |
| `surface` | `#1A1F24` | live button body |
| `surface-pressed` | `#2A333A` | pressed live button |
| `line` | `#2A3238` | separators, bar bottom border, empty-slot dashes |
| `line-strong` | `#5F6C76` | button borders (normal), 3.6:1 against `bg-live` |
| `text` | `#F2F5F7` | primary text |
| `text-dim` | `#AEB8C2` | secondary text, addresses, hints |
| `text-empty` | `#7E8A94` | empty-slot text only (not actionable) |
| `strip` | `#B2BBCA` | panel-strip nod: thin rules and the NEXT border only |
| `cyan` | `#5DB0D0` | lit fill, primary button fill, LED connected (LCD backlight nod) |
| `cyan-text` | `#7CC8E4` | cyan used as text or thin lines on dark; pressed-lit fill |
| `on-cyan` | `#04141B` | text and glyphs on `cyan` / `cyan-text` fills |
| `red` | `#FF8080` | error text, disconnected ring, fail glyph. Means "something is wrong", nothing else |
| `on-red` | `#04141B` | text on a `red` fill |
| `amber` | `#F0B65A` | favourite star (filled), "unknown" probe state. Browse and settings only |
| `green` | `#7BD39A` | "pass" probe state. Settings only |
| `bg-browse` | `#121518` | browse and settings window background |
| `panel` | `#1B2024` | browse panels, dialog body |
| `panel-raised` | `#232A30` | secondary buttons, toggles (off), set-editor cells |
| `field` | `#101417` | text inputs, list backgrounds |
| `sel-row` | `#1E4A5C` | selected result row, selected category |
| `border` | `#46525A` | browse control borders (1px) |
| `disabled-text` | `#6B7780` | disabled controls (exempt from contrast rules) |
| `lcd-bg` | `#0F2B36` | display strip background (LCD echo) |
| `lcd-text` | `#7CC8E4` | display strip text (same hex as `cyan-text`) |
| `placeholder` | `#8A97A2` | search placeholder text |

### 1.2 Contrast table (live view, target 7:1 for text)

| Foreground | Background | Ratio | Where |
|---|---|---|---|
| `text` `#F2F5F7` | `surface` `#1A1F24` | 15.2:1 | button name, slot number |
| `text-dim` `#AEB8C2` | `surface` | 8.3:1 | address, sound name (normal button) |
| `on-cyan` `#04141B` | `cyan` `#5DB0D0` | 7.7:1 | all text on the lit button |
| `text` | `#04141B` badge on lit | >17:1 | slot number on lit |
| `on-cyan` | `cyan-text` `#7CC8E4` | 10.0:1 | lit button when pressed |
| `text` | `surface-pressed` `#2A333A` | 11.7:1 | pressed normal button |
| `cyan-text` | `bg-live` | 10.4:1 | NEXT tag text |
| `strip` `#B2BBCA` | `bg-live` | 10.1:1 | NEXT border (also non-text) |
| `text` | `bar` `#12171B` | 16.5:1 | top bar text |
| `text-dim` | `bar` | 9.0:1 | top bar small text, set label |
| `cyan` | `bar` | 7.4:1 | connected LED, "NOW" marks in bar |
| `red` `#FF8080` | `bar` | 7.4:1 | NO X50 text, ring, errors |
| `on-red` `#04141B` | `red` | 7.7:1 | flashed (failed send) status chip |
| `text-dim` | `bg-live` | 9.7:1 | Esc hint |
| `text` | `bg-live` | 17.8:1 | Esc hint, armed state |
| `text-empty` `#7E8A94` | `bg-live` | 5.5:1 | empty slot (deliberately dim; not read in a glance) |
| `line-strong` | `bg-live` | 3.6:1 | button border (non-text, 3:1 required) |
| `line-strong` | `surface` | 3.1:1 | idem on the button body |

Browse and settings (target 4.5:1, most exceed 7:1): `text` on `panel` 15.0; `text-dim`
on `panel` 8.2; `text` on `field` 16.9; `text-dim` on `field` 9.2; `placeholder` on
`field` 6.2; `text` on `sel-row` 8.8; `text-dim` on `sel-row` 4.8; `lcd-text` on
`lcd-bg` 7.9; `on-cyan` on `cyan` 7.7; `green` on `panel` 9.1; `amber` on `panel` 9.0;
`red` on `panel` 6.8; `cyan` on `panel` 6.7. `disabled-text` is intentionally lower
(3.6:1 on `panel`).

Rule: never put `text-dim` on `sel-row` in anything smaller than 14px; use `text`.

---

## 2. Fonts

Two families, four files, all OFL 1.1, committed under `assets/fonts/` with their
licence files. Justification: Atkinson Hyperlegible keeps I/l/1 and O/0 distinct in
sound names read at a distance; Barlow Semi Condensed gives the tracked, uppercase,
hardware-silkscreen feel for labels, addresses and numerals in a narrow width.

| File | Family (Qt) | Weight | Source |
|---|---|---|---|
| `AtkinsonHyperlegible-Regular.ttf` | Atkinson Hyperlegible | 400 | https://raw.githubusercontent.com/googlefonts/atkinson-hyperlegible/main/fonts/ttf/AtkinsonHyperlegible-Regular.ttf |
| `AtkinsonHyperlegible-Bold.ttf` | Atkinson Hyperlegible | 700 | https://raw.githubusercontent.com/googlefonts/atkinson-hyperlegible/main/fonts/ttf/AtkinsonHyperlegible-Bold.ttf |
| `BarlowSemiCondensed-Medium.ttf` | Barlow Semi Condensed | 500 | https://raw.githubusercontent.com/jpt/barlow/master/fonts/ttf/BarlowSemiCondensed-Medium.ttf |
| `BarlowSemiCondensed-SemiBold.ttf` | Barlow Semi Condensed | 600 | https://raw.githubusercontent.com/jpt/barlow/master/fonts/ttf/BarlowSemiCondensed-SemiBold.ttf |

Licences (T2.3 fetches, saves beside the fonts):

- `assets/fonts/OFL-AtkinsonHyperlegible.txt` from https://raw.githubusercontent.com/googlefonts/atkinson-hyperlegible/main/OFL.txt
- `assets/fonts/OFL-Barlow.txt` from https://raw.githubusercontent.com/jpt/barlow/master/OFL.txt

I verified on 2026-11-21 that these four TTFs and both OFL.txt files exist at those
URLs (GitHub tree listing) and that the four files download (sizes 54 KB, 55 KB,
164 KB, 169 KB). The Windows-in-Qt family-name behaviour (below) is not verified.

Loading: `QFontDatabase::addApplicationFont` from Qt resources, once at start. After
loading, the builder logs `applicationFontFamilies()` for each. If the Barlow files come
back as family `Barlow Semi Condensed SemiBold` (a name-ID-1 style family) instead of
`Barlow Semi Condensed` with weights, `Theme` must use the returned family string with
weight Normal. `Theme` is the only place that names a family. Fallback if loading fails:
`Segoe UI`, then the platform sans; the layout rules (§5.3) measure with whatever font
is active, so nothing breaks, it only looks different.

### 2.1 Type roles

`px` below are design pixels in browse/settings; in live view they are multiplied by
`u` (§5.1). Tracking is letter spacing in px (`QFont::setLetterSpacing(AbsoluteSpacing)`).
Line height is a multiple of the pixel size.

| Role | Family, weight | Size | Tracking | Line height | Case |
|---|---|---|---|---|---|
| `live-name` | Atkinson Bold | fitted 28u to 52u (8 slots) or 60u (6 slots), §5.3 | 0 | 1.1 | as typed |
| `live-badge` | Barlow SemiBold | 28u | 0 | 1.0 | digit |
| `live-tag` | Barlow SemiBold | 22u | 2u | 1.0 | upper |
| `live-addr` | Barlow SemiBold | 22u | 1u | 1.0 | as typed (`COMBI A012`) |
| `live-sound` | Atkinson Regular | fitted 20u down to 14u | 0 | 1.1 | as typed |
| `bar-set` | Atkinson Bold | 24u (shrinks to 18u, §6.2) | 0 | 1.1 | as typed |
| `bar-status` | Barlow SemiBold | 22u | 1u | 1.0 | upper |
| `bar-now` | Atkinson Bold | 24u | 0 | 1.1 | as typed |
| `hint` | Barlow Medium | 18u | 0.5u | 1.2 | sentence |
| `body` | Atkinson Regular | 15px | 0 | 1.3 | as typed |
| `body-strong` | Atkinson Bold | 15px | 0 | 1.3 | as typed |
| `small` | Atkinson Regular | 13px | 0 | 1.3 | as typed |
| `label` | Barlow SemiBold | 12px | 1.5px | 1.2 | upper |
| `address` | Barlow SemiBold | 15px | 0.5px | 1.2 | as typed |
| `strip-main` | Atkinson Bold | 24px | 0 | 1.1 | as typed |
| `title` | Atkinson Bold | 20px | 0 | 1.2 | as typed |

Floor: any live text other than `live-name` never renders under 11 logical px (so
`max(11, round(size*u))`). `live-name` has no floor, it shrinks by the §5.3 rule.

Text rendering: `QFont::PreferDefaultHinting`, antialiasing on for text and shapes.
Sound names are never uppercased.

---

## 3. Spacing, radii, borders

### 3.1 Spacing scale (px, browse and settings)

`4, 8, 12, 16, 24, 32`. Names: `s1=4, s2=8, s3=12, s4=16, s5=24, s6=32`. Use nothing else.

### 3.2 Live spacing (multiplied by `u`)

Margin 24u, gap 20u, inner padding 16u, top bar 56u, hint strip 32u, badge 44u.

### 3.3 Radii

| Item | Radius |
|---|---|
| Live button | 14u |
| Slot badge | 8u |
| Live tag/chip, mode chip | 6u |
| Browse controls (buttons, inputs, toggles) | 6px |
| Panels, dialog groups | 8px |
| Pills (EDITED marker) | 4px |
| Window, list rows | 0 |

### 3.4 Border widths

| Item | Width |
|---|---|
| Live button normal | 2u |
| Live button lit | 6u, colour `text` |
| Live button next-up | 3u, colour `strip` |
| Live button empty | 2u dashed (dash 8u, gap 6u), colour `line` |
| Live button pressed | 4u, colour `cyan-text` (normal) |
| Bar bottom rule | 2u, colour `line` |
| Browse controls | 1px `border`; focused 2px `cyan` |
| Selected-row left accent | 4px `cyan` |

### 3.5 Pixel snapping rule

Every stroke width and rect edge is snapped to whole device pixels: stroke device
width = `max(1, round(w * dpr))`; logical width = device width / dpr. Borders are drawn
**inside** the rect (inset by half the stroke) so a thicker border never moves content.
Content layout uses the unsnapped values; only the painted edges snap. This is what
makes 1.25x and 1.5x crisp.

---

## 4. Window and scale handling (all views)

- Qt 6 default high-DPI scale policy (fractional, pass-through). Do not set
  `AA_EnableHighDpiScaling` or a rounding override. Verified requirement, not a
  verified default: T2.3/M3 screenshot tests at 1.25 and 1.5 must show crisp 2u borders.
- Pixmaps and icons are SVG or painted. Where a raster is needed, build it at
  `size * devicePixelRatio` and `setDevicePixelRatio(dpr)`.
- Handle `QEvent::ScreenChangeInternal`/`devicePixelRatioChanged` by recomputing the
  live fit cache (§5.3). Nothing else caches DPR.
- Live view is `showFullScreen()`. Browse is a normal window, default 1180 x 760,
  minimum 1000 x 700, remembers its geometry.

---

## 5. Live view

### 5.1 The unit `u`

All live sizes derive from the window (the live view's rect) size, not from the scale
factor:

```
u = clamp( min(W / 1280, H / 720), 0.4, 3.0 )      W, H in logical px
```

1280 x 720 is the reference where `u = 1`. Recompute on resize only. Round `u` to 3
decimals. Every live length is `round(value_in_design_px * u)` unless it is a font
size (use fractional `QFont::setPixelSize` with the rounded integer) or a stroke
(§3.5).

### 5.2 Structure and geometry (design px at `u = 1`; multiply by `u`)

Vertical stack, full window width:

1. Top bar: y 0, height 56. Background `bar`, 2 bottom rule `line` (inside the 56).
2. Gap 20.
3. Grid area: height `H - 56 - 20 - 20 - 32` = `H - 128` (at u = 1: 592).
4. Gap 20.
5. Hint strip: height 32, background `bg-live` (no fill, same as window), text centred.

Horizontal: margin 24 left and right of the grid and of the hint text.

Grid: **2 rows**, **4 columns (8 slots)** or **3 columns (6 slots)**, row-major, slot
1 top-left, slot 4 (or 3) top-right, next row continues. Gap 20 both directions.

```
col width (8) = (W - 48 - 3*20) / 4 = (W - 108u) / 4
col width (6) = (W - 48 - 2*20) / 3 = (W - 88u) / 3
row height    = (H - 128u - 20u) / 2
```

Buttons fill their cells; leftover pixels from rounding go to the last column and last
row. At the reference size: 8-slot button 293 x 286, 6-slot 397 x 286. Because `u` is the
minimum of the two ratios and the bars scale with `u`, the row height is always at
least `286u`, and the column width at least `293u` (8) or `397u` (6). The anatomy below
relies on those minimums.

### 5.3 Button anatomy

Inner box = button rect inset by 16 on all sides (the border sits inside that inset;
6u lit border does not shrink the inner box).

Top to bottom inside the inner box:

1. **Top row**, height 44. Left: slot badge, a 44 x 44 rounded square (radius 8),
   containing the slot digit (`live-badge`, centred). Right-aligned: state tag
   (`live-tag`, vertically centred on the row) when the slot is NOW or NEXT.
2. Gap 8.
3. **Name box**: height = button height - 16 - 44 - 8 - 8 - 48 - 16 = **button height
   - 140** (146 at reference). Width = inner width (button width - 32). Text centred
   horizontally and vertically, alignment centre for wrapped lines too.
4. Gap 8.
5. **Bottom block**, height 48, bottom-aligned: line 1 address (`live-addr`), line 2
   sound name (`live-sound`), left-aligned. Line 2 only when the slot has a label (the
   big text is then the label). With no label, the big text is the sound name and the
   bottom block shows only the address, bottom-aligned (line 1 sits at the bottom).

What is shown in the big text: the slot's `label` if set, else the sound's current
name (unit name if read, else factory name).

**Fit-to-width algorithm for the big text.** Runs when the label/name, the button size
or `u` changes; the result (pixel size, line breaks) is cached on the button. Paint
never measures. Measure with `QFontMetricsF` at the candidate pixel size.
`W_box` = name box width, `H_box` = name box height.

Stage A, one line. For `s` from `max1` down to `36u` in steps of `2u` (round each
candidate to whole px): if the one-line advance of the text is <= `W_box`, use `s`,
one line. `max1` = 52u (8 slots) or 60u (6 slots).

Stage B, two lines. For `s` from `max2` down to `28u` in steps of `2u`: break at
spaces (greedy, first line as long as fits); if the text fits in 2 lines (each line <=
`W_box`, height `2 * 1.1 * s` <= `H_box`), use it. `max2` = 44u (8) or 52u (6).
Text with no space may not fit by word; for the 28u step only, allow breaking inside
the word (`QTextOption::WrapAnywhere`) into 2 lines.

Stage C, last resort. For `s` from `26u` down to `14u` in steps of `2u`, allow break
anywhere into 2 lines, use the first that fits. Ellipsis is never used on the big text.
A 16-character name always fits by Stage C; the worst case measured (16 x "W", 14.1 em
in Atkinson Bold) fits in 2 lines at 28u in the narrowest 8-slot box (261u wide).
Stage C is a safety net for fonts that fail to load, not a normal path.

Resulting behaviour for common names (Atkinson Bold advance measured): "Lounge
Orchestra" (8.35 em) is two lines at 44u in the 8-slot grid and one line at 42u in the
6-slot grid; a 7-character name sets at 52u in one line.

**Bottom block fit.** `live-sound`: from 20u down to 14u in steps of 2u until the
advance <= inner width; at 14u a 16-character name always fits, so there is no ellipsis.
`live-addr` never needs fitting (longest is `PROG g(d)001`, 5.2 em at 22u = 114u).

Text colours per state are in §5.4. The big text is always `text` on normal buttons
and `on-cyan` on the lit button.

### 5.4 States of a live button

State is derived, never stored on the widget by hand: (slot empty?, lit?, next?,
pressed?). Repaint a button only when one of those changes, or its content changes.

| State | Fill | Border | Big text | Address / sound | Badge | Tag |
|---|---|---|---|---|---|---|
| **Normal** | `surface` | 2u `line-strong` | `text` | `text-dim` | outline 2u `line-strong`, no fill, digit `text` | none |
| **Lit (current)** | `cyan` | **6u `text`** | `on-cyan` | `on-cyan` | fill `on-cyan`, digit `text` | **filled right-pointing triangle 14u wide x 16u high + "NOW"**, colour `on-cyan` |
| **Next-up [decided]** | `surface` | 3u `strip` | `text` | `text-dim` | as normal | outlined chevron `>` (stroke 3u, 10u wide x 16u high) + "NEXT", colour `cyan-text` |
| **Empty** | none (shows `bg-live`) | 2u dashed `line` | "EMPTY" in `live-tag` style, `text-empty`, centred in the whole button (no name box) | none | outline 2u dashed `line`, digit `text-empty` | none |
| **Pressed** (mouse or touch down on a non-empty button) | `surface-pressed` | 4u `cyan-text` | `text` | `text-dim` | as normal | as before |
| **Pressed, lit** | `cyan-text` | 6u `text` | `on-cyan` | `on-cyan` | as lit | as lit |
| **Focused** | none in live view [decided] | none | none | none | none | none |
| **Disconnected** | buttons draw exactly as above; see §6.3 | | | | | |

Notes.

- **Non-colour signals for lit.** (1) The only button with a 6u bright border; (2)
  luminance inversion (light fill with dark text, readable in greyscale and under
  coloured light); (3) the "NOW" tag with a solid triangle, which no other state has;
  (4) the badge is solid dark instead of outlined. Any one of these is enough.
- The lit button is the brightest object on screen, a fill of mid-luminance cyan
  (not white). Nothing else in live view is filled with a bright colour.
- **Next-up** is SPEC-silent. The brief says the next sound must be readable in one
  glance, and the pedal keys step to it. Rule: the next non-empty slot after the lit
  slot, no wrap. None if nothing is lit or the lit slot is the last non-empty one.
  Owner may veto; removing it only deletes one row of this table and the tag.
- **Empty** never changes on press; click and keys do nothing, no pressed state.
- **Focused.** Live buttons are `Qt::NoFocus`; the live view widget owns the keyboard
  and never moves focus. So no focus ring exists in live view. (Browse focus is §7.)
- **Hover.** No hover styling in live view at all (no hover-only affordances).
  `WA_Hover` off for live buttons.
- Press state: set on mouse/touch press, cleared on release or if the pointer leaves.
  The send occurs on release inside the button (not on press), so a stray touch that
  slides away sends nothing. [decided]

### 5.5 Top bar contents (height 56u, padding 24u left and right)

Three zones in one row, vertically centred:

1. **Left, set name.** `bar-set` (Atkinson Bold 24u), `text`. Max width 34% of W. If
   wider, shrink 24u down to 18u in steps of 2u; at 18u elide with `…`. Eliding is
   acceptable here because the set name is the player's own text, shown in full in
   browse. [decided]
2. **Centre, now readout.** Starts after the set name zone with 24u gap; fills the
   remaining width less the right zone. Contents, left to right, gap 12u:
   - mode chip: `COMBI`, `PROG` or `--` (mode unknown), `bar-status` text, 2u
     `text-dim` border, radius 6u, height 32u, padding 10u horizontal, text `text-dim`.
   - address `live-addr` in `text-dim`, then the sound name `bar-now` in `text`,
     for example `COMBI A012  Lounge Orchestra`. Shrinks 24u to 16u in steps of 2u to
     fit, then wraps nothing and elides with `…` (it is a duplicate of the button text,
     which never truncates).
   - When the X50 is on a sound **not in the set**: after the name, a tag `NOT IN SET`
     (`live-tag` style at 18u, `cyan-text` text, 2u `cyan-text` border, radius 6u).
   - When the current sound is unknown (nothing sent, nothing followed): the readout
     is the single word `--`, `text-dim`.
3. **Right, connection.** Right-aligned. LED 16u diameter, 12u gap, then `bar-status`
   text. States:
   - `X50 CONNECTED`: LED filled disc `cyan`; text `text`.
   - `NO X50`: LED is a **hollow ring** 3u stroke `red` with a diagonal bar inside
     (the "no" sign), text `red`.
   - Error (SPEC: errors show in the top bar only), for example `PORT BUSY (ANOTHER
     PROGRAM IS USING THE X50)`: replaces the status text; LED as `NO X50`; text `red`
     in `bar-status` style at 18u, up to 2 lines, right zone allowed up to 45% of W
     (then the centre zone shrinks). Message words come from the app, not this file.
   - Failed send flash (SPEC §4.3 "flash the status red") [decided]: the whole right
     zone gets a `red` filled rounded chip (radius 6u, padding 8u 12u), text `on-red`,
     for 2 s then back. It is a state change, not an animation: set once, cleared once
     by a single-shot 2 s timer.

Bottom rule: 2u `line` along the bottom of the bar.

### 5.6 Esc-to-leave hint

- Text (exact): `Hold Esc for 1 second, or press Esc twice, to leave live view` in `hint`
  style, `text-dim`, centred in the hint strip.
- Shown: always while the live view is visible, never hidden, never moves.
- "Esc" is drawn as a key cap: 2px `text-dim` rounded rect (radius 4u), padding 6u
  horizontal, the letters `Esc` inside (`hint` style, `text`). The cap is part of the
  one-line layout, inline at the word "Esc" (first occurrence only, i.e. "Hold [Esc]
  for 1 second, or press Esc twice,"; the second "Esc" is plain text).
- Armed state (first Esc pressed, or Esc held): the text becomes `Esc pressed: press Esc
  again, or keep holding, to leave` in `text` colour, `hint` style, with `text` colour on
  the key cap border. It reverts to the normal text when the 1 s window ends (the
  existing Esc-hold timer) or when Esc is released and no second press follows within 1
  s. No progress bar or fill (nothing animates).
- Width at the narrowest case: 62 characters at 18u in Barlow Medium is under 560u, so
  it fits one line at every `u` since W >= 1280u.

### 5.7 Behaviour at each resolution

Full-screen windows. Native screen size, scale factor, logical window, `u`, button
size (8 slots and 6 slots), top bar height, and physical pixel size of the largest and
smallest big-text step (max1 for 8 slots, 28u):

| Screen | Scale | Logical W x H | `u` | 8-slot button | 6-slot button | Top bar | Name max 8 / 6 (physical px) | Name min (physical px) |
|---|---|---|---|---|---|---|---|---|
| 1920x1080 | 100% | 1920x1080 | 1.500 | 440x429 | 596x429 | 84 | 78 / 90 | 42 |
| 1920x1080 | 125% | 1536x864 | 1.200 | 352x343 | 477x343 | 67 | 78 / 90 | 42 |
| 1920x1080 | 150% | 1280x720 | 1.000 | 293x286 | 397x286 | 56 | 78 / 90 | 42 |
| 1920x1080 | 200% | 960x540 | 0.750 | 220x214 | 298x214 | 42 | 78 / 90 | 42 |
| 1366x768 | 100% | 1366x768 | 1.067 | 313x305 | 424x305 | 60 | 55 / 64 | 30 |
| 1366x768 | 125% | 1092x614 | 0.853 | 250x244 | 339x244 | 48 | 55 / 64 | 30 |
| 1366x768 | 150% | 910x512 | 0.711 | 208x203 | 282x203 | 40 | 55 / 64 | 30 |
| 1366x768 | 200% | 683x384 | 0.533 | 156x153 | 212x153 | 30 | 55 / 64 | 30 |

(Name max is 52u (8 slots) or 60u (6 slots) times `u` times the scale factor; name min
is 28u times the same. Check any cell with §5.1 to §5.3; the screenshot tests use these
cells.)

Because `u` follows the window, a 1920x1080 screen looks the same at every scale; what
changes is physical sharpness. Smaller physical screens at the same logical size (1366)
get smaller type automatically.

Legibility reasoning (stated because no published figure was found, REFERENCES §2):
15.6 in 1080p laptop, pixel pitch about 0.18 mm, 100% scale gives `u = 1.5`; a 52u
name is 78 px, about 14 mm em height, about 10 mm cap height, which at 1 m subtends
about 34 arcminutes. The fitted two-line 44u names are 66 px, about 8.5 mm cap height,
about 29 arcminutes at 1 m. Owner should confirm on the real laptop at stage distance.

Resize while live (window not full screen, only in testing and screenshots): recompute
`u`, geometry and fit caches, repaint everything once. No resize animation.

Aspect ratios other than 16:9: the grid fills the area, so buttons get taller (4:3) or
wider (21:9); type size follows `u` and the fit uses the real name box.

### 5.8 Six versus eight slots

Same anatomy. Only differences: 3 columns instead of 4, `max1` 60u (not 52u), `max2`
52u (not 44u). The set editor in browse uses the same column counts (§7.8).

---

## 6. Live view system states

### 6.1 Normal

As §5.

### 6.2 No button lit

When the unit is on a sound not in the set, or nothing is known, no button is lit and
no button is next-up. Top bar readout per §5.5.

### 6.3 Disconnected

- Top bar right zone: `NO X50` as §5.5. The buttons are not dimmed or disabled (manual
  sending is allowed by SPEC §4.5), and the **lit button stays as last known** [decided]
  with no extra marking, because the top bar already states the condition and a second
  state on the buttons would be one more thing to read.
- A send that cannot happen flashes the right zone red (§5.5).

### 6.4 Empty set (all slots empty)

All slots draw as Empty. Hint strip unchanged. The top bar left zone shows the set
name. No extra message (the set editor is where that is fixed). [decided]

### 6.5 Repaint discipline

Repaint triggers, and only these: lit slot change (repaint the old and new button, plus
the next-up pair and the bar readout), connection state change (bar only), press and
release (that button), Esc armed state (hint strip only), error text change (bar),
resize, DPR change, set or label change. Use `update(rect)` for the affected button or
zone, never `update()` on the whole view for a single change. No animation, no timer
for painting. Paint code does no allocation: pens, brushes, fonts, `QStaticText` or
precomputed layouts are built in the fit/geometry pass.

---

## 7. Browse view

### 7.1 Window and resize

Default 1180 x 760. **Minimum 1000 x 700.** `bg-browse` background. Outer margin 16
(`s4`), gap between regions 12 (`s3`).

Vertical stack, top to bottom:

| Region | Height | Stretches |
|---|---|---|
| Display strip | 64 | width |
| Toolbar row (search, kind toggles, buttons) | 40 | width |
| Main area (category list left; result list over set editor right) | rest | both |
| Status line | 24 | width |

Main area columns: **category list 200** (min 160, max 260, user-resizable by a splitter
handle 6 wide, `line` colour), remaining width is the right column. The right column,
top to bottom: **result list** (stretches), **transport bar** (56 fixed), **set editor
panel** (fixed 200). Everything fixed keeps its height; the result list takes all
slack. At the minimum window (700 high): 16 + 64 + 12 + 40 + 12 + [main] + 12 + 24 + 16 =
196 + main, so main = 504; the result list = 504 - 56 - 200 - 2*12 = 224: a 28 header plus 5 rows of 36 (196). That is the floor; below it the window cannot shrink.

### 7.2 Display strip (what the X50 is on, and the connection)

Height 64, full width, radius 8, background `lcd-bg`, 1px border `#1D4C5E`
[decided]. Padding 8 vertical, 12 horizontal. An echo of the backlit LCD (REFERENCES §1
item 9), drawn with QPainter rectangles only; no logo.

Left to right:

1. **Reversed bar** (the LCD selected-line look): rounded rect radius 4, fill `cyan`,
   height 48, padding 16 horizontal, auto width, max 62% of strip width. Contents,
   text `on-cyan`: mode chip text `COMBI` / `PROG` / `--` in `label` style (Barlow
   SemiBold 12px upper, tracked 1.5px), 12 gap, address (`address` 15px), 12 gap,
   sound name (`strip-main`, Atkinson Bold 24px, shrinks to 16px then elides `…`).
2. Gap 12. If the sound is unknown (nothing sent or followed) the reversed bar is not
   filled: it is drawn with `lcd-bg` fill and a 1px `lcd-text` border, and shows `--` in
   `lcd-text`.
3. Right aligned: connection LED and text as live bar (§5.5) but at 15px/`label`
   style, same LED rules (16px diameter, hollow ring with bar when disconnected),
   text `lcd-text` when connected, `red` when not.

### 7.3 Toolbar row (height 40)

Left to right, gap 8 between items:

| Item | Width | Notes |
|---|---|---|
| Search field | stretch, min 260 | 40 high, §7.4 |
| Kind toggles | 3 x 88, gap 4 | `Combis`, `Programs`, `GM`; §7.5 |
| `Read names` button | 120 | secondary; disabled with tooltip text when not connected |
| `Settings` button | 100 | secondary |
| `Live  F5` button | 100 | **primary** (cyan fill); text `Live` plus `F5` in `label` style |

Total at the minimum: 1000 - 32 = 968 available; fixed items 272 (3 x 88 + 2 x 4) + 120
+ 100 + 100 = 592, plus 4 gaps of 8 = 624, so the search field gets 344.

### 7.4 Search field

Height 40, radius 6, background `field`, 1px `border`, text `body` 15px `text`, placeholder
`Search name, address (A012), cat:text` in `placeholder`. Left padding 36 with a painted
magnifier icon (16 x 16, 2px stroke `text-dim`) at x 12. Focus: 2px `cyan` border
(width change absorbed by painting the border inside; no layout shift). A clear `x`
button appears inside the right edge (20 x 20, `text-dim`) only when text is present.
Focused on start (SPEC §4.1).

### 7.5 Kind toggles

Three checkable buttons, 88 x 40, radius 6, text `body-strong` 15px. On: fill
`sel-row`, 2px `cyan` border, text `text`, **plus a painted check mark 12 x 12 left of
the text** (not colour only). Off: fill `panel-raised`, 1px `border`, text `text-dim`, no
check. Hover: fill lightens to `#2C353C`. Pressed: fill `surface-pressed`. Focus: 2px
`cyan` outline offset 2. Disabled: text `disabled-text`, fill `panel`. All three on by
default.

### 7.6 Category list

Left column, background `panel`, radius 8, padding 8. Title row `CATEGORY` (`label` style,
`text-dim`, height 24). Then a list: first row `All`, then the 16 categories of the
kind shown (combination or program categories; when kinds mixed, the program list is
shown and the title reads `CATEGORY (PROG)`; combos-only shows `CATEGORY (COMBI)`)
[decided]. Row height 32, padding 12 horizontal, text `body` 15px `text`. Selected: fill
`sel-row`, 4px `cyan` left bar, text `text` bold. Hover: fill `panel-raised`. Focus:
2px `cyan` outline, inset. A count in `small` `text-dim` right-aligned on each row (the
number of matches) is **not** shown, to keep the list calm.

### 7.7 Result list and row delegate

Background `field`, radius 8, 1px `border`, no alternating rows. Header row 28 high
with `label` style column titles in `text-dim` on `panel`: no title over the star
column, `ADDRESS`, `NAME`, no title over the edited column, `CATEGORY`, `USED` (right
aligned). Rows are **36 high** (touch friendly; fits 5 rows at the
minimum). Row separator: 1px `line`, none under the selected row.

Columns left to right (widths in px; row padding 12 left and right):

| Column | Width | Content |
|---|---|---|
| Star | 36 | painted star 18 x 18, centred. Favourite: filled `amber`. Not: 1.5px outline `text-dim`. Click toggles. |
| Address | 112 | `address` style, `text-dim` (`text` in selected). Canonical form `COMBI A012`, `PROG G001`, `PROG g(d)001` |
| Name | stretch | `body` (Atkinson Regular 15px), `text`. Edited names show the unit's name. Elide with `…` at the right edge only when the column is too narrow (browse is a finder; the full name is in the tooltip and in the display strip once sent) |
| Edited | 60 | pill `EDITED`, 20 high, radius 4, 1px `amber` border, text `label` style 11px `amber`, shown only when the unit name differs from the factory name; tooltip shows the factory name |
| Category | 140 | `small` 13px `text-dim`, elide `…` |
| Used | 44 | `small`, `text-dim`, right aligned, `x12` form, blank for 0 |

Selected row: fill `sel-row`, 4px `cyan` left accent, text `text`, address `text`.
Hover: fill `panel-raised`. Focused list: 2px `cyan` outline inset around the list.
The row that matches the X50's current sound gets a painted solid right-pointing
triangle 8 x 10 `cyan` in the 8 px left of the star column [decided] (the browse echo of
NOW). Highlight never sends. Empty results: centred text `No sounds match.` in `body`
`text-dim`.

Double-click and Enter send. Context menu (native style, themed by QSS in §10) offers
Add to set > slot 1..N.

### 7.8 Transport bar (below results, height 56)

Left to right, gap 8, buttons 40 high:

| Control | Width | Notes |
|---|---|---|
| `Prev` | 88 | secondary, painted left chevron + text, tooltip `Ctrl+Left` |
| `Next` | 88 | secondary, text + right chevron, tooltip `Ctrl+Right` |
| `Play` | 120 | **primary** (cyan fill, `on-cyan` text bold), enabled only with a highlighted row; sends it |
| spacer | stretch | |
| `Star` toggle | 88 | secondary, painted star + `Star`, `Ctrl+D`; filled amber star when the highlighted sound is a favourite |

Primary state set: normal fill `cyan`; hover fill `#6FBFDE`; pressed fill `cyan-text`;
focus 2px `text` outline offset 2; disabled fill `panel-raised`, text `disabled-text`.
Secondary: normal fill `panel-raised` + 1px `border`, hover `#2C353C`, pressed
`surface-pressed`, focus 2px `cyan` outline offset 2, disabled text `disabled-text`.

### 7.9 Set editor panel (height 200, below the transport bar)

Background `panel`, radius 8, padding 12. Two rows:

1. **Header row**, height 36, gap 8: set picker (combo, stretch, min 160, 36 high),
   buttons `New`, `Rename`, `Duplicate`, `Delete` (secondary, 36 high, 84 wide each; Delete
   text `red`; it asks for confirmation as SPEC says), and a size toggle `6 | 8` (two
   segmented 36 x 36 toggles styled like kind toggles).
2. Gap 8. **Slot grid**, remaining height (200 - 24 - 36 - 8 = 132): 2 rows by 4 columns
   (8 slots) or 3 columns (6 slots), the same shape as the live layout, gap 8. Cell
   height (132 - 8) / 2 = 62, width = (panel inner width - gaps) / columns (about 170 at
   the minimum window for 8 slots).

Slot cell: radius 6, `panel-raised`, 1px `border`; padding 8. Left: slot digit in a
24 x 24 rounded-square (radius 4, 1px `text-dim` outline, `address` style). Right of it
two lines: line 1 label or sound name (`body-strong` 15px `text`, elide `…`), line 2
address (`small` 13px `text-dim`) and, if a label is set, the sound name instead of
nothing on line 2 as `address - name`. Empty: dashed 1px `border`, text `EMPTY` `label`
style `disabled-text`. Selected (clicked): 2px `cyan` border. Drag target while
dragging: fill `sel-row`, 2px `cyan` dashed. Lit-equivalent (the slot that is current on
the X50): a 6 x 8 solid triangle at the cell's top-right, `cyan`. Focus: 2px `cyan`
outline offset 2. Double-click edits the label in place (an inline field in `field`
style). `Del` empties the slot (no confirmation).

### 7.10 Status line (24 high)

`small` text `text-dim`, left. Shows read-names progress (with a 6 px high progress bar
200 wide, track `field`, fill `cyan`, and a `Cancel` secondary button 72 x 24 in the same
line while reading), short messages and the "library was broken" once-only notice.
Never modal.

---

## 8. Settings dialog

Modal, 520 wide, minimum height 560, fixed width, height grows with content to at most
80% of the screen (the checklist scrolls inside if needed). Background `bg-browse`,
content on a `panel` card (radius 8, padding 16). Title bar is the OS title bar, text
`Settings`. Closes with `Close` (primary, 120 x 40, bottom right) or Esc.

Sections, vertical gap 16, each starts with a `label` style title in `text-dim`:

1. **`MIDI PORTS`**: two rows, label column 96 wide (`body`), combo stretch, 40 high:
   `Output` combo, `Input` combo, plus `Test` (secondary, 80 x 40) at the end of the
   Output row. Combo style: like inputs; the drop-down arrow is a painted chevron.
2. **`IF THE X50 DOES NOT ANSWER`** (manual settings): `Channel` spin box (1 to 16,
   default 1, 40 high, 96 wide) and `Bank Map` combo (`GM`, `KORG`, default `GM`, 40 high,
   160 wide) on one row, with a `small` `text-dim` note beneath: `Used only when the X50
   gives no answer.`
3. **`X50 CHECK`**: `Check X50` button (primary, 140 x 40) and, to the right, a status
   word `small` `text-dim` (`Last check: 14:02` or `Not checked yet`). Below, the
   **probe checklist** (§8.1).

### 8.1 Probe checklist

A list of lines, each: **glyph (24 x 24 box, painted)**, then text. Row height 32, min;
gap 4 between rows. Text `body` 15px `text` (the line's message, for example `X50 found
(system 1.05, global channel 1)`). The glyphs are painted shapes, never font characters,
so they render the same on every platform; the CLI's text `✓`/`✗` do not appear in the GUI.

| State | Glyph (24 x 24 box, 2px strokes) | Glyph colour | Message | Fix hint |
|---|---|---|---|---|
| **pass** | filled disc (20 diameter) `green` with a check mark in `on-cyan` | `green` | `text` | none |
| **fail** | filled **square** with radius 4 (20 x 20) `red` with a cross in `on-red` | `red` | `text` | shown, below |
| **unknown** | outlined **diamond** (20 x 20 rotated square) `amber` with a short dash inside | `amber` | `text` | optional |
| **running** | hollow circle 20 diameter `cyan-text` with three 3 px dots inside (static, no spinner) | `cyan-text` | `text-dim` | none |

Shape differs for every state (disc, square, diamond, hollow circle), so colour is not
the only signal. Rows are drawn in the order of SPEC §4.5 (`X50 found`, `Program Change`,
`Bank Change`, `Combination Change`, `Bank Map`, `SysEx replies`). While the probe runs
every row shows `running` and its text is the check's name; a row updates when its reply
arrives (repaint that row only). The Device Inquiry failure shows a single `fail` row
whose message is the "No reply..." text of SPEC §4.5; the other rows are not shown.

**Fix hint style.** A `fail` or `unknown` row followed by its hint on its own wrapped
block: indent 36 (aligned with message text), `small` 13px Atkinson Regular `text-dim`,
width = row width - 36, line height 1.35, padding 4 top, 8 bottom, and a **2px left
rule** in `red` (fail) or `amber` (unknown) at x 24 (the glyph centre line, drawn from
the bottom of the glyph to the end of the hint). The hint text uses the X50's own words
and quotes, for example `GLOBAL 1: MIDI, tick "Bank"`, with the quoted words in
`body-strong` weight. The "No reply" hint is the only multi-sentence hint and wraps in
the same style.

Disabled-until-first-run state: the list area shows `small` `text-dim` text `Run Check X50
to see whether the X50 is set up for this app.` centred in a `field` box 96 high.

Dialog controls states: as §7.8 (primary / secondary), inputs as §7.4, combos and spin
boxes follow input states (focus 2px `cyan`, disabled `disabled-text` on `panel`).

---

## 9. Icon brief (for T2.3)

Deliverables: `assets/icons/app.svg` (source), `app-256.png`, `app.ico`
(containing 16, 24, 32, 48, 64, 128, 256 px frames; **Windows first, the `.ico` is what
the exe embeds**). Square, no text, no KORG mark, no keyboard or panel drawing.

Concept: the live view in miniature. A rounded-square tile (radius 22% of the width)
filled `#1B2024` with a 1px inner rim `#5F6C76`. On it a grid of 2 rows by 4 columns of
rounded rectangles (radius 12% of cell), each `#2A333A` with a `#5F6C76` 2 unit rim, gaps
equal to 40% of the cell, margin 14% of the tile. **One cell lit** (second from left, top
row): fill `#5DB0D0`, rim `#F2F5F7` heavy, a small solid dark triangle `#04141B` in it.
Nothing else. At 24 px and below use a simplified 2 x 2 grid with the same lit cell
(top-left), no rim. No gradient, no glow. Palette strictly the §1.1 tokens.
The tile must read on both a dark and a light taskbar: the light rim `#B2BBCA` 1px
outside edge at 256 px (scaled down proportionally) provides that.

---

## 10. QSS mapping

Live view is custom painted (QPainter) from `Theme` constants and does not use QSS.
Browse and settings use QSS from `assets/theme/app.qss` loaded once with
`qApp->setStyleSheet`. Rules the builder must follow so the QSS can target things:

| Object name / class / property | Target | Notes |
|---|---|---|
| `QMainWindow`, `QDialog`, `QWidget#browseRoot` | `bg-browse` | |
| `#displayStrip` | `DisplayStrip` | painted; QSS gives only border and radius via `Theme`, so DisplayStrip paints itself |
| `#searchField` (QLineEdit) | search input | 40 high via QSS `min-height`/`max-height` |
| `QToolButton#kindToggle` + `:checked`, `:hover`, `:pressed`, `:disabled`, `:focus` | the 3 toggles | setCheckable(true) |
| `QPushButton[role="primary"]` | Live, Play, Check X50, Close | `setProperty("role", "primary")` |
| `QPushButton[role="secondary"]` | all other buttons | default role if unset |
| `QPushButton[role="danger"]` | Delete text colour | `red` text only |
| `#categoryList` (QListView) | category list | `::item` height 32 |
| `#resultList` (QListView/QTableView) | result list | delegate paints rows; QSS gives frame, background, focus |
| `#resultList QHeaderView::section` | header row | 28 high, `label` style |
| `#transportBar` | transport container | |
| `#setEditor`, `#setPanel` | set editor panel | background `panel`, radius 8 |
| `QFrame#slotCell` with property `state` = `normal|empty|selected|drop` | slot cells | also `[current="true"]` |
| `#statusLine` (QLabel), `QProgressBar#readProgress` | status | 6 high bar |
| `#settingsDialog`, `#settingsCard` | dialog and card | |
| `QLabel[role="sectionTitle"]` | `label` style titles | uppercase text set in code |
| `QLabel[role="small"]`, `QLabel[role="hint"]` | small and dim text | |
| `#probeChecklist` (custom widget) | checklist | paints glyphs and hints itself from `Theme`; QSS sets only `background` and `font` |
| `QComboBox`, `QSpinBox` | inputs | 40 high |
| `QToolTip` | tooltips | bg `panel-raised`, text `text`, 1px `border`, padding 6 |
| `QMenu`, `QMenu::item:selected` | context menus | bg `panel`, selected `sel-row`, item height 32 |
| `QScrollBar:vertical` | scrollbars | 12 wide, handle `border` with radius 6, hover `strip`, no arrows |
| `QSplitter::handle` | splitter | 6 wide, `line` |

Rule for everything painted by QPainter (live buttons, top bar, display strip,
checklist, delegate): colours and sizes come from `Theme`, which mirrors the tokens in
§1 and §3; never hard-code hex in a paint method.

---

## 11. Builder checklist

Every value the builder needs and where it is. Nothing below is left to choose.

- [x] Palette hex, names, ratios: §1.
- [x] Four font files, sources, licences, load and fallback: §2. Type roles with sizes,
      tracking, case, line height: §2.1.
- [x] Spacing scale, live spacing, radii, border widths, pixel snapping: §3.
- [x] High-DPI policy, minimum window size, default size: §4.
- [x] Live unit formula and clamp: §5.1. Live stack, grid, gaps, margins, column and row
      maths: §5.2. Button anatomy with every vertical measurement: §5.3.
- [x] Fit algorithm (max, min, step, wrap, last resort, no ellipsis), bottom-block fit,
      cache rule: §5.3.
- [x] Every live state with fill, border, text, badge, tag; non-colour lit signals;
      next-up, empty, pressed, focused, disconnected: §5.4, §6.
- [x] Top bar zones, widths, shrink rules, error and flash behaviour: §5.5.
- [x] Esc hint text, placement, armed text, when shown: §5.6.
- [x] 1x, 1.25x, 1.5x, 2x on 1920x1080 and 1366x768, with button sizes: §5.7.
- [x] 6 versus 8 slot differences: §5.8.
- [x] Repaint discipline and what triggers a repaint: §6.5.
- [x] Browse regions, sizes, resize rules, minimum size: §7.1. Display strip: §7.2.
      Toolbar, search, toggles: §7.3 to §7.5. Category list: §7.6. Result row delegate
      with exact row height and columns, edited and favourite markers: §7.7. Transport,
      Play: §7.8. Set editor grid and cell: §7.9. Status line: §7.10.
- [x] Settings dialog, probe checklist states with glyph shapes, fix hint style: §8.
- [x] Icon brief: §9. QSS object names and properties: §10.

Values the builder must still decide (none visual): the `Theme` class's API and the
exact names of C++ constants (mirror the token names with `camelCase`). The mockups
(T2.3) are authoritative for any visual question this text cannot answer; if the
mockup and this text disagree, this text wins and the mockup is a bug.

### Re-read pass: values I originally left implicit, now filled

1. Name box height formula and what happens to leftover height: §5.3 (button height - 140).
2. The inset rule for borders so thick borders do not shift content: §3.5.
3. Lit button badge and tag colours: §5.4.
4. Which key cap is drawn in the hint and when it changes: §5.6.
5. What the disconnected state does to the buttons (nothing), flash duration (2 s): §5.5, §6.3.
6. Where the current X50 sound shows in browse lists (triangle) and in the set editor: §7.7, §7.9.
7. Fallback fonts and the family-name check for the Barlow files: §2.
8. Scale behaviour on non-16:9 windows: §5.7.

### Not fixable from here (needs the owner or hardware)

- Whether Barlow Semi Condensed registers under the family name `Barlow Semi Condensed`
  on Windows Qt: check at first run (§2); not verified.
- The 1 m legibility claim is computed, not measured (§5.7). Owner to test on the real laptop.
- The `next-up` state and the "lit button stays when disconnected" rule are my decisions
  where SPEC is silent (§5.4, §6.3).
