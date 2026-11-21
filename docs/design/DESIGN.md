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
| `strip` | `#B2BBCA` | panel-strip nod: thin rules; scrollbar hover |
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
| `lcd-field` | `#E6E8E1` | browse display strip: backlit LCD field (pale, slightly warm grey) |
| `lcd-field-dim` | `#BFC3BA` | live top bar readout panel: the same LCD, dimmed so it does not glow on a dark stage |
| `lcd-ink` | `#111411` | LCD text, inverse (reversed) segment fill, LCD LED and borders |
| `lcd-bezel` | `#3A413B` | the thin dark bezel round both LCD panels (2px browse, 2u live) |
| `lcd-warn` | `#8A1414` | reserved for red text on a pale LCD field; currently unused (connection status moved outside the LCD panels) |
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
| `lcd-ink` `#111411` | `lcd-field-dim` `#BFC3BA` | 10.4:1 | live top bar readout text (address, name) |
| `lcd-field-dim` | `lcd-ink` | 10.4:1 | mode text in the inverse segment |
| `lcd-bezel` `#3A413B` | `bar` `#12171B` | 1.7:1 | panel edge (decoration; the field edge against `bar` is the boundary) |
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
`field` 6.2; `text` on `sel-row` 8.8; `text-dim` on `sel-row` 4.8; `lcd-ink` on
`lcd-field` 15.0; `lcd-field` on `lcd-ink` 15.0; `lcd-warn` on `lcd-field` 7.8; `on-cyan` on `cyan` 7.7; `green` on `panel` 9.1; `amber` on `panel` 9.0;
`red` on `panel` 6.8; `cyan` on `panel` 6.7. `disabled-text` is intentionally lower
(3.6:1 on `panel`).

Rule: never put `text-dim` on `sel-row` in anything smaller than 14px; use `text`.

---

## 2. Fonts

Three families, six files, committed under `assets/fonts/` with their licence files:
Atkinson and Barlow are OFL 1.1, Pixel Operator is CC0 1.0 (§2.2). Justification: Pixel
Operator X50 Bold sets the big names on the live buttons (owner choice at G2, 2026-11-21:
dot-matrix, in the spirit of the X50's LCD; its dotted zero and distinct `I`, `l`, `1`
keep names readable); Atkinson Hyperlegible keeps I/l/1 and O/0 distinct in the live top
bar set name, the small sound-name line on buttons, browse and settings text; Barlow Semi
Condensed gives the tracked, uppercase, hardware-silkscreen feel for labels, addresses
and numerals in a narrow width.

| File | Family (Qt) | Weight | Source |
|---|---|---|---|
| `AtkinsonHyperlegible-Regular.ttf` | Atkinson Hyperlegible | 400 | https://raw.githubusercontent.com/googlefonts/atkinson-hyperlegible/main/fonts/ttf/AtkinsonHyperlegible-Regular.ttf |
| `AtkinsonHyperlegible-Bold.ttf` | Atkinson Hyperlegible | 700 | https://raw.githubusercontent.com/googlefonts/atkinson-hyperlegible/main/fonts/ttf/AtkinsonHyperlegible-Bold.ttf |
| `BarlowSemiCondensed-Medium.ttf` | Barlow Semi Condensed | 500 | https://raw.githubusercontent.com/jpt/barlow/master/fonts/ttf/BarlowSemiCondensed-Medium.ttf |
| `BarlowSemiCondensed-SemiBold.ttf` | Barlow Semi Condensed | 600 | https://raw.githubusercontent.com/jpt/barlow/master/fonts/ttf/BarlowSemiCondensed-SemiBold.ttf |

| `PixelOperatorX50.ttf` | Pixel Operator X50 | 400 | patched Pixel Operator (CC0): upstream zip https://www.dafont.com/pixel-operator.font (`https://dl.dafont.com/dl/?f=pixel_operator`; upstream repo https://notabug.org/HarvettFox96/ttf-pixeloperator, not reachable from the design machine); patched by `docs/design/mockup/patch_font.py` (§2.2) |
| `PixelOperatorX50-Bold.ttf` | Pixel Operator X50 | 700 | same, patched |

Licences (T2.3 fetches, saves beside the fonts):

- `assets/fonts/OFL-AtkinsonHyperlegible.txt` from https://raw.githubusercontent.com/googlefonts/atkinson-hyperlegible/main/OFL.txt
- `assets/fonts/OFL-Barlow.txt` from https://raw.githubusercontent.com/jpt/barlow/master/OFL.txt
- `assets/fonts/CC0-PixelOperator.txt`: the `LICENSE.txt` shipped in the Pixel Operator zip,
  Creative Commons CC0 1.0 Universal (also stated in the font's name table, ID 13). CC0 is a
  public-domain dedication, so it meets the "OFL or equivalent" rule.

I verified on 2026-11-21 that these four TTFs and both OFL.txt files exist at those
URLs (GitHub tree listing) and that the four files download (sizes 54 KB, 55 KB,
164 KB, 169 KB). The Windows-in-Qt family-name behaviour (below) is not verified.

Verified with `fc-scan` on 2026-11-21 (T2.3): the Atkinson files report family
`Atkinson Hyperlegible`, styles `Regular` and `Bold`. Each Barlow file reports two
families: `Barlow Semi Condensed` (typographic family, name ID 16) and `Barlow Semi
Condensed Medium` / `Barlow Semi Condensed SemiBold` (name ID 1), with style `Medium` or
`SemiBold`. Qt on Windows commonly registers the ID 1 name, so `Theme` must take the
string returned by `applicationFontFamilies()` and not hard-code one. Files as committed:
`assets/fonts/` (4 TTF plus 2 OFL texts), `assets/icons/` (`app.svg`, `app-small.svg` for
the 16 and 24 px frames, `app-256.png`, `app.ico`), `assets/theme/app.qss`.

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
| `live-name` | Pixel Operator X50 Bold | `16k` device px, `k` whole, fitted by §5.3 (start `round(4.5 * U)`, floor 2, last resort 1) | 0 | cell grid, pitch `14k` (§5.3) | as typed |
| `live-badge` | Barlow SemiBold | 28u | 0 | 1.0 | digit |
| `live-tag` | Barlow SemiBold | 22u | 2u | 1.0 | upper |
| `live-addr` | Barlow SemiBold | 22u | 1u | 1.0 | as typed (`COMBI A012`) |
| `live-sound` | Atkinson Regular (kept, §5.3 note) | fitted 20u down to 14u | 0 | 1.1 | as typed |
| `bar-set` | Atkinson Bold | 24u (shrinks to 18u, §6.2) | 0 | 1.1 | as typed |
| `bar-status` | Barlow SemiBold | 22u | 1u | 1.0 | upper |
| `bar-now` | superseded by the `lcd-*` roles below (Pixel Operator) | | | | |
| `hint` | Barlow Medium | 18u | 0.5u | 1.2 | sentence |
| `body` | Atkinson Regular | 15px | 0 | 1.3 | as typed |
| `body-strong` | Atkinson Bold | 15px | 0 | 1.3 | as typed |
| `small` | Atkinson Regular | 13px | 0 | 1.3 | as typed |
| `label` | Barlow SemiBold | 12px | 1.5px | 1.2 | upper |
| `address` | Barlow SemiBold | 15px | 0.5px | 1.2 | as typed |
| `strip-main` | superseded by the `lcd-*` roles below (Pixel Operator) | | | | |
| `lcd-row` | Pixel Operator X50 Regular (Bold where §7.2 says) | `16k` device px | 0 | cell grid, §2.2 | as typed |
| `lcd-bar-browse` | Pixel Operator X50 Regular | `32k` device px | 0 | cell grid | as typed |
| `lcd-live-bar` | Pixel Operator X50 Bold | `16*kb` device px | 0 | cell grid | as typed |
| `lcd-live-mode` | Pixel Operator X50 Bold | `16*km` device px | 0 | cell grid | as typed |
| `title` | Atkinson Bold | 20px | 0 | 1.2 | as typed |

`k`, `kb`, `km` are whole numbers of device pixels per font pixel (§2.2).

Floor: any live text other than `live-name` never renders under 11 logical px (so
`max(11, round(size*u))`). `live-name` has no px floor, it is fitted by the §5.3 rule in
whole `k` (floor `k` = 2, then 1 only as a last resort).

Text rendering: `QFont::PreferDefaultHinting`, antialiasing on for text and shapes, for
every face except Pixel Operator (§2.2 has its own rules).
Sound names are never uppercased.

### 2.2 Pixel Operator (LCD panels only)

Used in exactly three places: the browse display strip (§7.2), the live top-bar readout
(§5.5) and the big name on each live button (§5.3, Bold only, owner choice at G2). It is
never used for lists, labels, dialogs or the connection status.

Why this face (research, 2026-11-21, against the Operation Guide screenshots, "PROG
0:Play" printed p.2 and "COMBI 0:Play" p.4): the X50 LCD has a chunky proportional bitmap face in
two weights, bold for the header and the bank, regular for the category line and the big
sound bar, with a slashed or dotted zero. Pixel Operator is a proportional 16-pixel
face with a Regular and a Bold (upstream slashed zero, replaced by a dotted zero, below), 9-pixel capitals and 7-pixel lowercase,
and it covers every character in `data/x50-factory.tsv` (checked with fontTools against
every character of every name and category in the file, including `& ' ( ) / . : + !`) and `…` (U+2026). It is not the KORG
face and carries no trademark. Two upstream glyphs failed at readout sizes (`&` read as `6`, "D104" read as "D184"), so
the committed files are **patched copies**, family `Pixel Operator X50`, made by
`docs/design/mockup/patch_font.py` from the upstream TTFs (CC0 allows it). Only two
glyph outlines change; metrics, grid, advances and every other glyph are identical:

- `0`: dotted zero. The slash is gone. Regular: oval as upstream (5 wide, rows 0 to 8)
  with a 1 x 3 pixel centre mark (column 3, rows 3 to 5), leaving a 1 px gap each side.
  Bold: oval as upstream (6 wide, 2 px strokes) with a 2 x 3 centre mark (columns 3 and 4,
  rows 3 to 5); at those three rows the side strokes thin to 1 px so a 1 px gap shows each
  side. Reads as a zero with something inside, never as the waisted `8`, nor as `O` or `D`.
- `&`: classic pixel ampersand, a top loop, a crossing diagonal, an arm at the upper right
  of the lower half and a lower loop. Regular 5 px wide (columns 1 to 5), cap height 9,
  inside the upstream advance (7 px). **Bold (redrawn at D4, 2026-11-21)** is 7 px wide
  (columns 1 to 7, advance 8, so no right side bearing; the next letter's own 1 px bearing
  leaves a 1 px gap) with the right side open: a 2 px arm at the upper right of the lower
  half (rows 4 to 5) and a 2 px tail at the bottom right (row 8), separated from the body by
  gaps. The first Bold version (6 px wide, closed lower loop, 1 px counters) read as `8`
  with a notch at `k` = 6 in `Piano &Strings` (D4). Checked on
  `docs/design/mockup/glyph-test.png` and `glyph-test-amp.png` (old and new at `k` = 2 to 7). The Operation Guide's LCD figures show
  no factory name with an ampersand, so the shape is not copied from the X50.

The family name in the name table is `Pixel Operator X50` for both files (Bold has style
`Bold`), so it is never confused with upstream. Source of the upstream files: §2.

Grid. `unitsPerEm` is 1600 and every outline coordinate is a multiple of 100, so one font
pixel is `size / 16` px. The face is crisp only when the pixel size is `16 * k` with `k` a
whole number of **device** pixels (16, 32, 48, 64 ... px). Metrics in font pixels: ascent
13, descent 3, cap height 9, x-height 7, so ink runs from 9 above the baseline to 3
below it (12 rows). Advances are whole multiples of 100 units (space 4, `i` 5 regular or
6 bold, digits 7 regular or 8 bold, `M` and `w` 9), so text widths are whole multiples of
`k`.

Rendering rules for the builder:

1. Paint the LCD panels in device-pixel space: take `QTransform tr = p.transform()`, map
   the panel's logical origin with `tr` and round it to whole device pixels, then
   `p.save(); p.resetTransform();` and paint with integer device coordinates only.
   Every panel dimension below is a whole number of device pixels. This is what keeps
   1.25x and 1.5x on the grid. `p.restore()` afterwards.
2. `QFont f(family); f.setPixelSize(16 * k)` with `k` a whole number (never a fractional
   point size); `f.setHintingPreference(QFont::PreferNoHinting)`;
   `f.setStyleStrategy(QFont::StyleStrategy(QFont::PreferAntialias | QFont::NoSubpixelAntialias))`;
   `f.setKerning(false)`; `f.setLetterSpacing(QFont::AbsoluteSpacing, 0)`. Draw with
   `drawText(QPoint(x, baseline), text)` where `x` and `baseline` are integers, **one call per
   character** (D3, 2026-11-21: Qt 6.4 with FreeType drifts by a device pixel inside a
   run even with hinting off). For each character `c` of the text: `adv(c) = k * round(
   QFontMetricsF(f).horizontalAdvance(c) / k)` (whole font pixels), `x_0` the integer
   start, `x_(i+1) = x_i + adv(c_i)`, no kerning, no letter spacing. Line width for
   centring and fitting is the sum of `adv`. Cache the per-character offsets in the same
   pass as the fit (no allocation in paint). Subpixel
   (ClearType) antialiasing is off so no coloured fringes appear on the inverse bars.
3. Do not measure with `QFontMetricsF` in logical pixels. Use `QFontMetrics` on the same
   device-space font; with no hinting the advance is exactly `k` times the font-pixel
   advance.
4. Baseline inside a box of height `h` device px, for ink centred (cap top to descender
   bottom): `baseline = top + round((h - 12k) / 2) + 9k`. §7.2 gives the browse
   positions as fixed offsets that this formula reproduces.
5. Family registration like Barlow: log `applicationFontFamilies()`; the Bold file reports
   family `Pixel Operator X50` style `Bold` (name ID 1 and 2), so one family string with
   weight Bold works. `Theme` holds the string. If loading fails the panels fall back to
   Barlow SemiBold at `16*k` px and the same boxes (they only look different).

Sizes in use (device px per font pixel `k`; the font px size is `16k`):

| Place | `k` | Font px | Cap height (px) | Rule |
|---|---|---|---|---|
| Browse strip, header, bank, category rows | `k = max(1, round(scale))` | `16k` | `9k` | `scale` is the screen's device pixel ratio |
| Browse strip, sound bar | `2k` | `32k` | `18k` | |
| Live readout, sound bar | `kb = max(1, round(1.8 * U))` | `16*kb` | `9*kb` | `U = u * devicePixelRatio` |
| Live readout, mode segment | `km = min(kb, max(1, round(1.2 * U)))` | `16*km` | `9*km` | |
| Live button name (Bold) | `k`, fitted per name by §5.3, from `round(4.5 * U)` down to 2 (1 last resort) | `16k` | `9k` | name box in device px |

At 1x, 1.25x, 1.5x, 2x on a 1080p screen: browse `k` = 1, 1, 2, 2 (font 16 and 32, 16 and
32, 32 and 64, 32 and 64 px); the live `U` is 1.5 in every row of §5.7 for 1920x1080, so
`kb = 3` (48 px, cap 27 px), `km = 2` (32 px). On a 1366x768 panel `U` = 1.067, so `kb = 2`,
`km = 1`. The size changes only when `u` or the screen changes (§6.5), never per paint.

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

Scope (D3, 2026-11-21): the rule is **strict** for everything painted by `QPainter`
(live view, the two LCD panels, glyphs, the checklist, the list delegates). Controls
drawn by QSS in browse and settings (buttons, inputs, toggles, slot cells) are drawn by
Qt with its own border geometry; at 1.25x their 1 px borders land on 1.25 device px and
show one blended row. That is accepted there (measured, D3 review item 4) and is not
a reason to replace QSS controls by custom painting. At 1x and 2x they are exact.

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

Buttons fill their cells; each column width is rounded, and the last column and last row
take whatever is left (at 1920x1080, 8 slots: 440, 440, 440, 438). At the reference size: 8-slot button 293 x 286, 6-slot 397 x 286. Because `u` is the
minimum of the two ratios and the bars scale with `u`, the row height is always at
least `286u`, and the column width at least `293u` (8) or `397u` (6). The anatomy below
relies on those minimums.

### 5.3 Button anatomy

Inner box = button rect inset by 16 on all sides (the border sits inside that inset;
6u lit border does not shrink the inner box).

Top to bottom inside the inner box:

1. **Top row**, height 44. Left: slot badge, a 44 x 44 rounded square (radius 8),
   containing the slot digit (`live-badge`, centred). Right-aligned: state tag
   (`live-tag`, vertically centred on the row) when the slot is NOW; the tag's
   triangle sits 8u left of the tag text, the tag's right edge at the inner
   box's right edge.
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

**Font of the big text.** `Pixel Operator X50` **Bold** (`PixelOperatorX50-Bold.ttf`),
always, on normal and lit buttons alike. Size is `16k` device px with `k` a whole number
of device pixels per font pixel (§2.2), never a fractional or `u`-multiplied size.
Metrics in font pixels: cap height 9, x-height 7, ink 12 rows (9 above the baseline, 3
below), advances whole multiples of `k` (§2.2). **Line pitch is `14k`** (12 ink rows plus
2 rows of air). Case as typed, no tracking, kerning off.

**Fit-to-width algorithm for the big text.** Runs when the label/name, the button size,
`u` or the screen's device pixel ratio changes; the result (`k`, lines, line pitch,
origin) is cached on the button. Paint never measures. All numbers are device pixels:
`W_box = round(logical name box width * dpr)`, `H_box = round(logical name box height *
dpr)` (logical box from this section, `dpr` = `devicePixelRatio`). Measure with
`QFontMetrics` on the device-space font (§2.2 rule 3), so a width is exactly `k` times
the font-pixel advance. `U = u * dpr`.

Constants. `kcap = max(2, round(4.5 * U))`, the largest `k`, same for 6 and 8 slots (so
the larger 6-slot buttons gain by fitting more on one line, not by bigger type).
`kfloor = 2` (32 px font, cap height 18 px). One line needs `12k <= H_box`; two lines need
`26k <= H_box` (12k ink plus one pitch of 14k).

1. **One-line `s1`.** The largest `k` from `kcap` down to `kfloor` such that the whole
   text on one line has advance <= `W_box` (and `12k <= H_box`); none if no `k` fits.
2. **Balanced two lines `s2` (text with at least one space only).** For each candidate
   `k` from `kcap` down to `kfloor`, split at the space that gives the smallest widest
   line (measure both halves at `k`; ties go to the earlier space). One break only, never
   three lines, so "Piano &Strings" breaks after "Piano" and "Lounge Orchestra" after
   "Lounge". `s2` is the largest `k` for which both halves have advance <= `W_box` and
   `26k <= H_box`; none if no `k` fits.
3. **Choose.** If `s2` exists and (`s1` is none or `s2 > s1`): two lines at `s2`. Else if
   `s1` exists: one line at `s1`. A spaced name therefore breaks onto two larger lines
   before it is shrunk onto one line, and on a tie (`s2 == s1`) the single line wins.
4. **No fit at `kfloor` (a long word, or a tiny window).** Never an ellipsis, never
   mid-word breaks while `kfloor` fits. Otherwise break anywhere
   (`QTextOption::WrapAnywhere`, as many lines as needed) at `k = 2` if the lines fit
   `H_box` (n lines need `12k + 14k * (n - 1)`), then at `k = 1` with whatever line count
   results (clip at the box if it overflows). This is a safety net: the worst 16-character
   name ("WWWWWWWWWWWWWWWW") needs it only at the smallest windows (table below).
5. **No space (`StereophonicKeys`, `Waiting4Sunrise`).** Step 2 is skipped, so the result
   is step 1 (one line, largest `k` that fits) or step 4.

**Placement (device px, integers only).** Let `n` = number of lines, `h = 12k + 14k *
(n - 1)`. Paint in device-pixel space like the LCD panels (§2.2 rule 1: map the name box
origin with the painter transform, round, `resetTransform`). `top = box_top +
round((H_box - h) / 2)`. Line `i` (0-based) has `baseline = top + 9k + 14k * i` and
`x = box_left + round((W_box - line_advance) / 2)`, both integers (lines are centred
individually). Font settings exactly as §2.2 rule 2 (`PreferNoHinting`, no subpixel
antialiasing, kerning off). Colour per §5.4 (`text`, or `on-cyan` when lit).

Resulting sizes at 1920x1080 (all four scale factors, `U = 1.5`, `kcap = 7`, box 392 x
219 in the first 8-slot column, 548 x 219 for 6 slots) and 1366x768 (`U = 1.067`,
`kcap = 5`, box about 279 x 155 for 8 slots, 390 x 155 for 6; the box differs by 1 to 5
device px between scale factors because logical sizes round first). Cells show `k`
(font px = `16k`), with `/2` for two lines:

| Screen, scale | Slots | `StereophonicKeys` | `Waiting4Sunrise` | `Piano &Strings` | `Solo lead` | `Layered A.Piano` | 16 x `W` |
|---|---|---|---|---|---|---|---|
| 1920x1080 at 1x, 1.25x, 1.5x, 2x | 8 | 3 (48 px) | 3 (48 px) | 6/2 (96 px) | 7/2 (112 px) | 7/2 (112 px) | 2 (32 px) |
| 1920x1080 at 1x, 1.25x, 1.5x, 2x | 6 | 4 (64 px) | 4 (64 px) | 7/2 (112 px) | 7 (112 px, one line, ties go to one line) | 7/2 (112 px) | 3 (48 px) |
| 1366x768 at 1x, 1.25x, 2x | 8 | 2 (32 px) | 2 (32 px) | 4/2 (64 px) | 5/2 (80 px) | 4/2 (64 px) | 2/2 lines (32 px) |
| 1366x768 at 1.5x | 8 | 2 (32 px) | 2 (32 px) | 4/2 (64 px) | 5/2 (80 px) | 5/2 (80 px) | 2/2 lines (32 px) |
| 1366x768 at 1x, 1.25x, 1.5x, 2x | 6 | 3 (48 px) | 3 (48 px) | 5/2 (80 px) | 5 (80 px, one line) | 5/2 (80 px) | 2 (32 px) |

(Computed with `docs/design/mockup/fit_table.py`, which runs this algorithm on the real
font file. At 1920x1080 the last 8-slot column is 2 px narrower than the first; no listed
name changes. `Layered A.Piano` at 1366x768, 1.5x differs from the other scales because
`Layered` is 280 px wide at `k` = 5 and the box is 276 to 281 px: only the 281 px box fits.
A 1 px difference in the box can move `k` by 1, which is correct behaviour, not a bug.) The mockups are 1920x1080 at 1x and show
`Verse EP`, `Intro pad`, `Bridge organ` and `Solo lead` on two lines at `k` = 7, `Piano
&Strings` on two lines at `k` = 6, `StereophonicKeys` and `Waiting4Sunrise` on one line at
`k` = 3. In the 6-slot mockup `Warm pad` and `Harp intro` are one line at `k` = 7 (tie).

Choice for the owner, not implemented: at 1366x768 the 16-character no-space names fall
to `k` = 2 (cap height 18 px). Breaking them in the middle of the word onto two lines
would give `k` = 4 but reads worse, so the rule above refuses mid-word breaks while
`k` = 2 fits.

Mixed faces on a button: the address line is Barlow SemiBold (as before) and the sound
name line below a label stays Atkinson Regular (as before, not Barlow: sound names keep
their distinct `I`, `l`, `1`, and a smaller proportional text under the dot-matrix name
separates "what it is called" from "what it is"). I looked at both mockups for a clash
and found none (the pixel face is much larger and the small lines are clearly
secondary), so both are unchanged.

**Bottom block fit.** `live-sound`: from 20u down to 14u in steps of 2u until the
advance <= inner width; at 14u a 16-character name always fits, so there is no ellipsis.
`live-addr` never needs fitting (longest is `PROG g(d)001`, 5.2 em at 22u = 114u).

Text colours per state are in §5.4. The big text is always `text` on normal buttons
and `on-cyan` on the lit button.

### 5.4 States of a live button

State is derived, never stored on the widget by hand: (slot empty?, lit?,
pressed?). Repaint a button only when one of those changes, or its content changes.

| State | Fill | Border | Big text | Address / sound | Badge | Tag |
|---|---|---|---|---|---|---|
| **Normal** | `surface` | 2u `line-strong` | `text` | `text-dim` | outline 2u `line-strong`, no fill, digit `text` | none |
| **Lit (current)** | `cyan` | **6u `text`** | `on-cyan` | `on-cyan` | fill `on-cyan`, digit `text` | **filled right-pointing triangle 14u wide x 16u high + "NOW"**, colour `on-cyan` |
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
- The lit button is the brightest object on screen by its 6u `text` border, with a fill
  of mid-luminance cyan (not white; relative luminance 0.38). The one other large pale
  area is the top-bar LCD readout field (`lcd-field-dim`, relative luminance 0.54, 685 x
  54 device px at 1080p); it is brighter than the cyan fill but small, in the bar, and
  accepted (D4). Nothing else in live view is filled with a bright colour.
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
2. **Centre, now readout: an LCD panel in Pixel Operator (§2.2).** The top bar stays
   dark; only this readout is an LCD, echoing the unit's black-on-pale display, whose
   header row reads `PROG 0:Play` in an inverse block with the sound as a big inverse bar
   (owner direction 2026-11-21; Operation Guide printed pages 2 and 4). One compact line: an inverse
   **mode segment** then an inverse **sound bar**. Starts after the set name zone with a
   24u gap. Painted with rectangles and device-space text only (§2.2 rule 1); no
   gradient, no dot grid. `U = u * devicePixelRatio`, `kb`, `km` from §2.2.
   - Panel: height 40u, vertically centred in the bar content (above the 2u rule); outer
     rounded rect radius 6u fill `lcd-bezel` (`#3A413B`, the 2u bezel), inside it a rect
     inset 2u, radius 4u, fill `lcd-field-dim` (`#BFC3BA`, deliberately dimmer than the
     browse strip so it does not glow on a dark stage). Inner padding 4u on all sides
     between the field and the two segments. Width = 2u + 4u + mode segment + 6u gap +
     sound bar + 4u + 2u, as wide as its content.
   - Mode segment: plain rectangle (radius 0), height 32u, fill `lcd-ink`, horizontal
     padding 8u each side, text Pixel Operator Bold `16*km` px in `lcd-field-dim`. Text
     is exactly `PROG 0:Play` or `COMBI 0:Play` (the X50 writes the page as `0:Play`;
     it is a constant).
   - Gap between segment and bar: 6u of `lcd-field-dim` showing through.
   - Sound bar: plain rectangle, height 32u, fill `lcd-ink`, horizontal padding 10u each
     side, text Pixel Operator **Bold** `16*kb` px in `lcd-field-dim` (Bold in live view
     for legibility; the unit's own bar is regular). Text is exactly
     `<bank letter><number>: <name>`: bank letter `A` to `D`, `G` or `g(d)`, number as
     three digits, a colon, one space, the sound's name as typed. Examples: `D104: Classic
     E.Piano`, `C096: Piano &Strings`, `A000: StereophonicKeys`, `G001: Acoustic Piano`
     (the manual gives GM numbers as 001 to 128, Operation Guide, specifications: "Program banks G:
     001-128, g(d): 001-128", so `001`; the bank letter is kept here because live view has no separate
     bank row). If the name is unknown (an address read from the unit before names were
     read), the name part is `---`.
   - Vertical text position in both segments: the §2.2 rule 4 baseline formula with
     `h = 32u` (rounded to whole device px); ink height `12k` is at most 22u at `kb`,
     so it always fits.
   - Fit: the panel's available width is the bar width minus left padding, set zone, 24u
     gaps, the right zone and (when shown) the `NOT IN SET` tag and its 16u gap. If the
     panel is wider, lower `kb` by 1 (and `km = min(km, kb)`) until it fits or `kb = 1`;
     if still too wide, drop characters from the end of the name and append `…`
     (U+2026, present in the font). Never lower the mode segment text on its own.
   - Contrast: `lcd-field-dim` on `lcd-ink` 10.4:1 (both texts, >= 7:1).
   - When the X50 is on a sound **not in the set**: after the panel (16u gap, so outside
     it), a tag `NOT IN SET` (`live-tag` style at 18u, `cyan-text` text, 2u `cyan-text`
     border, radius 6u, height 32u, padding 10u horizontal) on the dark bar.
   - When the current sound is unknown (nothing sent, nothing followed): mode segment
     text `--`; the bar is not filled but drawn as a `max(1, round(2u))` device-px
     `lcd-ink` outline on the field, with the text `--` in Pixel Operator Regular
     `16*kb` px, `lcd-ink`, padding as above.
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
- "Esc" is drawn as a key cap: 2u `text-dim` rounded rect (radius 4u), height 24u, padding
  6u horizontal, one normal space either side, the letters `Esc` inside (`hint` style, `text`). The cap is part of the
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
smallest big-text step (the Pixel Operator name sizes are in §5.3, not here):

| Screen | Scale | Logical W x H | `u` | 8-slot button | 6-slot button | Top bar | `U = u * scale` | Name `kcap` (largest font px) / floor 2 (32 px) |
|---|---|---|---|---|---|---|---|---|
| 1920x1080 | 100% | 1920x1080 | 1.500 | 440x429 | 596x429 | 84 | 1.500 | 7 (112 px) |
| 1920x1080 | 125% | 1536x864 | 1.200 | 352x343 | 477x343 | 67 | 1.500 | 7 (112 px) |
| 1920x1080 | 150% | 1280x720 | 1.000 | 293x286 | 397x286 | 56 | 1.500 | 7 (112 px) |
| 1920x1080 | 200% | 960x540 | 0.750 | 220x214 | 298x214 | 42 | 1.500 | 7 (112 px) |
| 1366x768 | 100% | 1366x768 | 1.067 | 313x305 | 424x305 | 60 | 1.067 | 5 (80 px) |
| 1366x768 | 125% | 1092x614 | 0.853 | 250x244 | 339x244 | 48 | 1.066 | 5 (80 px) |
| 1366x768 | 150% | 910x512 | 0.711 | 208x203 | 282x203 | 40 | 1.067 | 5 (80 px) |
| 1366x768 | 200% | 683x384 | 0.533 | 156x153 | 212x153 | 30 | 1.066 | 5 (80 px) |

(Name `kcap = max(2, round(4.5 * U))`, §5.3; the sizes actually chosen per name are in the
§5.3 table. Check any cell with §5.1 to §5.3; the screenshot tests use these cells.)

LCD readout pixel sizes (§2.2, §5.5) on these cells: 1920x1080 at every scale has
`U = u * scale = 1.5`, so `kb = 3` (48 px font, 27 px caps) and `km = 2` (32 px); the
1366x768 cells all have `U = 1.067`, so `kb = 2` (32 px) and `km = 1` (16 px). Always whole
device pixels.

Because `u` follows the window, a 1920x1080 screen looks the same at every scale; what
changes is physical sharpness. Smaller physical screens at the same logical size (1366)
get smaller type automatically.

Legibility reasoning (stated because no published figure was found, REFERENCES §2):
15.6 in 1080p laptop, pixel pitch about 0.18 mm, 100% scale gives `u = 1.5`; a 52u
name is 78 px, about 14 mm em height, about 10 mm cap height, which at 1 m subtends
about 34 arcminutes for Atkinson at that size (superseded face). The dot-matrix names have
cap height `9k` px: at `k` = 7 (112 px font) 63 px, about 11 mm, about 38 arcminutes at 1
m; at `k` = 3 (the 16-character names) 27 px, about 4.9 mm, about 17 arcminutes. Owner should confirm on the real laptop at stage distance.

Resize while live (window not full screen, only in testing and screenshots): recompute
`u`, geometry and fit caches, repaint everything once. No resize animation.

Aspect ratios other than 16:9: the grid fills the area, so buttons get taller (4:3) or
wider (21:9); type size follows `u` and the fit uses the real name box.

### 5.8 Six versus eight slots

Same anatomy. Only difference: 3 columns instead of 4, so the name box is wider (548 vs
392 device px at 1920x1080) and more names fit on one line or at a larger `k` (§5.3
table). `kcap` is the same. The set editor in browse uses the same column counts (§7.8).

---

## 6. Live view system states

### 6.1 Normal

As §5.

### 6.2 No button lit

When the unit is on a sound not in the set, or nothing is known, no button is lit and
Top bar readout per §5.5.

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

Repaint triggers, and only these: lit slot change (repaint the old and new button and the bar readout), connection state change (bar only), press and
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
| Display strip | `74k` device px = `74k / scale` px (74 at 1x), `k` from §2.2 | width |
| Toolbar row (search, kind toggles, buttons) | 40 | width |
| Main area (category list left; result list over set editor right) | rest | both |
| Status line | 24 | width |

Main area columns: **category list 200** (min 160, max 260, user-resizable by a splitter
handle 6 wide, `line` colour), remaining width is the right column. The right column,
top to bottom: **result list** (stretches), **transport bar** (56 fixed), **set editor
panel** (fixed 200). Everything fixed keeps its height; the result list takes all
slack. At the minimum window (700 high): 16 + 64 + 12 + 40 + 12 + [main] + 12 + 24 + 16 =
196 + main with a 64 strip; with the 74 strip it is 206 + main, so main = 494 and the result list = 494 - 56 - 200 - 2*12 = 214: a 28 header plus 5 rows of 36 (208). That is the floor. At scale 1.5 and 2 (`k` = 2) the strip is 148 device px = 98.7 px at 1.5x and 74 px at 2x; the window minimum height must grow by `strip - 74` so the 5-row floor still holds (setMinimumHeight is recomputed when `k` changes). Below it the window cannot shrink.

### 7.2 Display strip (what the X50 is on, and the connection)

Height `74k` device px (74 at 1x), full width row. Left: an LCD panel in Pixel Operator
(§2.2) that echoes the X50's display (Operation Guide printed pages 2 and 4: inverse header row,
"Bank" and "Category" line, then the sound as a large inverse bar). Right, **outside the
panel**, on the dark `bg-browse`: the connection LED and status. Painted with QPainter
rectangles and device-space text only (§2.2 rule 1), no gradient, no dot grid, no logo.
All numbers below are device pixels with `k = max(1, round(scale))`; at 1x `k = 1`, so
they are also logical px.

Panel box: `480k` wide, `74k` high (clipped to the available width on a narrow window),
left aligned at the window margin. Bezel: outer rounded rect radius `4k`, fill
`lcd-bezel` (`#3A413B`), and the field inside it inset `2k`, radius `2k`, fill
`lcd-field` (`#E6E8E1`). Field size `476k x 70k`. Coordinates below are inside the
field (origin top-left). Segments and the bar have radius 0.

| Element | Box (x, y, w, h) | Text | Colours |
|---|---|---|---|
| Mode segment (left) | `4k, 3k, textW + 8k, 16k` | `PROG 0:Play` or `COMBI 0:Play`, Bold `16k`, x = 8k, baseline y = 14k | fill `lcd-ink`, text `lcd-field` |
| Page segment (right) | right edge at `472k` (4k from the field edge), `y 3k, w textW + 8k, h 16k` | `Program` Bold + `:Program Select` Regular (PROG), or `Combination` Bold + `:Combi Select` Regular (COMBI), both `16k`, x = segment left + 4k, baseline y = 14k | fill `lcd-ink`, text `lcd-field` |
| Bank | text only | `Bank A` Bold `16k`, x = 8k, baseline y = 32k | `lcd-ink` on field |
| Category | text only | `Category 00:Keyboard` Regular `16k`, x = 160k, baseline y = 32k | `lcd-ink` on field |
| Sound bar | `4k, 37k, 468k, 30k` | `096: Piano &Strings` Regular `32k` (`2k` per font pixel), x = 10k, baseline y = 37k + 21k = 58k | fill `lcd-ink`, text `lcd-field` |

`textW` is the measured advance of the segment text (a whole multiple of `k`); the field
bottom padding is 3k below the bar.

Exact strings:

- Mode segment: `PROG 0:Play` in Program mode, `COMBI 0:Play` in Combination mode. The
  page `0:Play` is constant.
- Page segment: `Program:Program Select` (word Bold) or `Combination:Combi Select`.
- Bank: `Bank ` plus the bank as the app writes it: `A`, `B`, `C`, `D`, `G`, `g(d)`
  (Combination banks `A` to `C`). Example `Bank C`.
- Category: `Category ` + two-digit zero-based category index + `:` + category name, for
  example `Category 00:Keyboard`. The index is the category's position (zero-based) in the
  unit's category table. The manual proves two points only: `Keyboard` is `00` in both
  modes (printed p.3 "00: Keyboard"; p.4 `Category 00:Keyboard`) and PROG `SlowSynth` is `09`
  (p.2 `Category 09:SlowSynth`). The rest of the order is **not verified here**: take it from
  the unit's category-name table once read, or from the VNL page order, and never
  from an alphabetical sort. If the index is not known show `Category --:Keyboard`.
  If the category is unknown show `Category --:--`.
- Sound bar: three-digit number (`000` to `127`, GM `001` to `128`), a colon, one space,
  the name as typed: `096: Piano &Strings`, `001: Acoustic Piano`. The bank is not
  repeated (it is in the Bank row). Unknown name: `096: ---`.
- Names never shrink here: the bar is 468k wide, and the widest factory name
  (`OrchestraMaximus`, bar text `000: OrchestraMaximus`) measures `286k` at the `2k`
  size against `456k` available (measured with the committed font). If a name still
  does not fit, clip with `…` at the end.
- Unknown sound (nothing sent, nothing followed): mode segment `--` (padding as above),
  no page segment, no Bank or Category text, and the sound bar is an outline: `k` thick
  `lcd-ink` stroke inside the box, no fill, text `--` Regular `32k` in `lcd-ink`.

Connection zone (right aligned, vertically centred on the strip, 16 from the right
margin of the window, on `bg-browse`): same LED and text as the live bar (§5.5, right
zone), at browse size: LED 16px diameter, 12px gap, text `label` style (Barlow SemiBold
15px tracked 1.5px, upper). Connected: filled disc `cyan`, text `text`. Disconnected:
hollow ring 3px `red` with a diagonal bar, text `NO X50` in `red`. Errors replace the
text with the app's message in `red`, up to two lines, right zone at most 40% of the
window width (the panel is clipped first).

Contrast: `lcd-ink` on `lcd-field` 15.0:1; `lcd-field` on `lcd-ink` 15.0:1.
The token `lcd-warn` is no longer used in the strip (its use moved to `red` on the dark
background); keep the token.

### 7.3 Toolbar row (height 40)

Left to right, gap 8 between items:

| Item | Width | Notes |
|---|---|---|
| Search field | stretch, min 260 | 40 high, §7.4 |
| Kind toggles | 3 x 112, gap 4 | `Combis`, `Programs`, `GM`; §7.5 |
| `Read names` button | 126 | secondary; disabled with tooltip text when not connected |
| `Settings` button | 100 | secondary |
| `Live  F5` button | 100 | **primary** (cyan fill); text `Live` plus `F5` in `label` style |

Button rule (D3): padding 16 left and right (border included), so a button is
`max(listed width, text advance + 32)`; the listed width is a floor and a wider text grows
the button, never clips it. Measured advances (Atkinson Bold 15): `Read names` 94 (so 126),
`Settings` 62, `Live  F5` 70 (listed 100 holds). Toolbar and the three buttons are 40 high.

Total at the minimum: 1000 - 32 = 968 available; fixed items 348 (3 x 112 + 2 x 4) + 126
+ 100 + 100 = 674, plus 4 gaps of 8 = 706, so the search field gets 262 (min 260).
(Toggles were 88, then 100; `Programs` with its check mark sat 5 px from the border at
100, so they are 112.)

### 7.4 Search field

Height 40, radius 6, background `field`, 1px `border`, text `body` 15px `text`, placeholder
`Search name, address (A012), cat:text` in `placeholder`. Left padding 36 with a painted
magnifier icon (16 x 16, 2px stroke `text-dim`) at x 12. Focus: 2px `cyan` border
(width change absorbed by painting the border inside; no layout shift). A clear `x`
button appears inside the right edge (20 x 20, `text-dim`) only when text is present.
Focused on start (SPEC §4.1).

### 7.5 Kind toggles

Three checkable buttons, 112 x 40 (fixed), radius 6, text `body-strong` 15px. On: fill
`sel-row`, 2px `cyan` border, text `text`, **plus a painted check mark 12 x 12 left of
the text** (not colour only). Layout inside (D3): the left padding is a constant 20 in
every state (12 for the check mark plus 8 gap, reserved even when off, so the text never
moves when toggled); the text is centred in the width that remains inside the border.
The check mark's left edge is the text's left edge minus 20 (so 8 gap to the text); its
vertical centre is the button's. At `Programs` (text 71 wide) the check sits 10 px from
the outer edge and the text ends 11 px from the other edge; `Combis` and `GM` have more
air, same rule. Off: fill `panel-raised`, 1px `border`, text `text-dim`, no
check. Hover: fill lightens to `#2C353C`. Pressed: fill `surface-pressed`. Focus: 2px
`cyan` outline offset 2. Disabled: text `disabled-text`, fill `panel`. All three on by
default.

### 7.6 Category list

Left column, background `panel`, radius 8, padding 8. Title row `CATEGORY` (`label` style,
`text-dim`, height 24). Then a list, top to bottom [decided]:

1. `Favourites` (row 32, star glyph).
2. `Recent` (row 32, clock glyph).
3. A separator row, 9 high, not selectable, not focusable: a 1px `line` rule across the
   row (inset 12 each side) with 4px empty above and 4px below.
4. `All`.
5. The 16 categories of the kind shown (combination or program categories; when kinds
   mixed, the program list is shown and the title reads `CATEGORY (PROG)`; combos-only
   shows `CATEGORY (COMBI)`).

The title never changes when Favourites or Recent is selected (it names the category
list, which is unchanged); the result list has no title of its own. Initial selection on
start stays `All`.

Row height 32, padding 12 horizontal, text `body` 15px `text`, for every row including
Favourites and Recent (no other text style). Favourites and Recent carry a painted glyph
14 x 14 at x = 12, vertically centred, with the text starting at x = 34 (12 + 14 + 8);
`All` and the categories keep the text at x = 12, so the two special rows are visibly
set apart by the glyph as well as by the separator:

- Star (Favourites): the same five-point star shape as the result list's, filled `amber`
  in every state (unselected, hover, selected). It is the shape, not the colour, that
  identifies the row.
- Clock (Recent): a circle of 14 px outer diameter, 1.5px stroke `text-dim` (`text` when
  selected), no fill, plus two 1.5px hands from the centre: one up 4px, one right 3px
  (round caps). Painted with antialiasing; no animation.

Selected: fill `sel-row`, 4px `cyan` left bar, text `text` bold (glyphs as above). Hover:
fill `panel-raised`. Focus: 2px `cyan` outline, inset. Up/Down skip the separator. A
count in `small` `text-dim` right-aligned on each row (the number of matches) is **not**
shown, to keep the list calm.

### 7.7 Result list and row delegate

Background `field`, radius 8, 1px `border`, no alternating rows. Header row **28 high
measured from the outer top edge of the list**: the 1px `border` plus a 27 px header
section (`QHeaderView::section` height 27, and the header widget `setFixedHeight(27)`)
with `label` style column titles in `text-dim` on `panel`: no title over the star
column, `ADDRESS`, `NAME`, no title over the edited column, `CATEGORY`, `USED` (right
aligned). Rows are **36 high** (touch friendly; fits 5 rows at the
minimum). Row separator: 1px `line`, none under the selected row.

Columns left to right (widths in px). Row padding 12 applies **only to the outer edges**:
12 before the star column and 12 after the `Used` column (so the star column starts 12
px inside the list's inner edge and `Used` text ends 12 px before the scrollbar or inner
right edge). Columns abut with no inner cell padding; text is drawn at the column's left
edge (right edge for `Used`), the gutter is in the column widths. Header titles use the
same x as their column. Name text is elided at `Edited` column left minus 8.

| Column | Width | Content |
|---|---|---|
| Star | 36 | painted star 18 x 18, centred. Favourite: filled `amber`. Not: 1.5px outline `text-dim`. Click toggles. |
| Address | 112 | `address` style, `text-dim` (`text` in selected). Canonical form `COMBI A012`, `PROG G001`, `PROG g(d)001` |
| Name | stretch | `body` (Atkinson Regular 15px), `text`. Edited names show the unit's name. Elide with `…` at the right edge only when the column is too narrow (browse is a finder; the full name is in the tooltip and in the display strip once sent) |
| Edited | 60 | pill `EDITED`, 20 high (vertically centred in the 36 row), radius 4, 1px `amber` border, text `label` style 11px `amber`, **left-aligned in its column** (pill x = column left, width = text advance + 12, 52 at the committed font), shown only when the unit name differs from the factory name; tooltip shows the factory name. Measured: x 1635 to 1686, the Category column starts 8 px later |
| Category | 140 | `small` 13px `text-dim`, elide `…` |
| Used | 44 | `small`, `text-dim`, right aligned, `x12` form, blank for 0 |

Selected row: fill `sel-row`, 4px `cyan` left accent, text `text`, address `text`.
Hover: fill `panel-raised`. Focused list: 2px `cyan` outline inset around the list.
The row that matches the X50's current sound gets a painted solid right-pointing
triangle 8 x 10 `cyan` in the 8 px left of the star column [decided] (the browse echo of
NOW). Highlight never sends. Result order is catalogue order, never by name: COMBI A, B, C, then PROG A, B, C, D, then G, then g(d) (only when GM is on), each bank by number. Empty results: centred text in `body`
`text-dim`, wrapped to the list width minus 48, see "Favourites and Recent views" below
for the text.

**Favourites and Recent views** (category rows `Favourites`, `Recent`, §7.6). They are a
filter over the same list and the same row delegate; they combine (AND) with the search
text and the Combis / Programs / GM toggles exactly like a category does.

- Order: `Favourites` in catalogue order (the §7.7 order above, never by name or by when
  starred). `Recent` most recent first (the order of the recents list, newest at the top),
  **not** catalogue order. Prev/next and Enter step through the list in the order shown.
- Columns unchanged: Star column, Address, Name, Edited, Category, and `USED` (`x12`
  form, blank for 0; the count is the same whichever view the sound is shown in). In
  Favourites every star is filled, in Recent a star is filled only when the sound is a
  favourite. The current-sound triangle and the selected-row styling are unchanged.
- Clicking a star in Favourites only toggles the star (outline, `text-dim`): the row
  stays, keeping its position and the selection, so a second click restores it. Rows of
  sounds that are no longer favourites drop out the next time the view is rebuilt
  (another category or view selected, search text or kind toggles changed). The same
  star click in Recent only toggles the star. (Foreman, 2026-11-21: no instant removal,
  so an accidental click is undone in place.)
- Empty text, centred horizontally and vertically in the list area, `body` 15px
  `text-dim`. The "nothing at all" texts apply when the view has no entries before search
  and kinds are applied; otherwise (search or kinds filtered everything out) the usual
  text applies:

| Case | Exact text |
|---|---|
| `Favourites`, no favourites exist | `No favourites yet. Star a sound with Ctrl+D or the star.` |
| `Recent`, nothing sent yet | `Nothing played yet. Sounds you send appear here.` |
| `Favourites` or `Recent` has entries but search or kinds hide them all | `No sounds match.` |
| Any other category, no match | `No sounds match.` |


Double-click and Enter send. Context menu (native style, themed by QSS in §10) offers
Add to set > slot 1..N.

### 7.8 Transport bar (below results, height 56)

Left to right, gap 8, buttons 40 high:

| Control | Width | Notes |
|---|---|---|
| `Prev` | 90 | secondary, painted left chevron + text, tooltip `Ctrl+Left` |
| `Next` | 90 | secondary, text + right chevron, tooltip `Ctrl+Right` |
| `Play` | 120 | **primary** (cyan fill, `on-cyan` text bold), enabled only with a highlighted row; sends it |
| spacer | stretch | |
| `Star` toggle | 88 | secondary, painted star + `Star`, `Ctrl+D`; filled amber star when the highlighted sound is a favourite |

Prev / Next content (D3): a chevron 7 wide x 12 high, 2px `text` stroke, round caps,
vertically centred; gap **8** between chevron and text; chevron and text together are
centred in the button (padding at least 16). `Prev`: chevron then text; `Next`: text then
chevron. (The build had 5 and 3 px gaps.) Play 120 and Star 88 as listed; the same
`max(listed, advance + 32)` rule as §7.3 applies (Star's painted star 14 + gap 8 count as
advance). Accepted at the second look: the build uses a 16 px icon with Qt's own icon-to-text
gap (ink 12.5 wide, about 5 px to the text, button still 88); the star is a secondary control
and the difference is not visible at arm's length.

Primary state set: normal fill `cyan`; hover fill `#6FBFDE`; pressed fill `cyan-text`;
focus 2px `text` outline offset 2; disabled fill `panel-raised`, text `disabled-text`.
Secondary: normal fill `panel-raised` + 1px `border`, hover `#2C353C`, pressed
`surface-pressed`, focus 2px `cyan` outline offset 2, disabled text `disabled-text`.

### 7.9 Set editor panel (height 200, below the transport bar)

Background `panel`, radius 8, padding 12. Two rows:

1. **Header row**, height 36, gap 8: set picker (combo, stretch, min 160, 36 high),
   buttons `New` 84, `Rename` 96, `Duplicate` 106, `Delete` 84 (secondary, 36 high; the
   `max(84, advance + 32)` rule of §7.3, which gives these; Delete text `red`; it asks for
   confirmation as SPEC says), and a size toggle `6 | 8` (two segmented 36 x 36 toggles
   styled like kind toggles, gap 8, no check mark: the digit is the label and the on
   state's cyan border and `sel-row` fill carry the state; the on state is also bold).
2. Gap 8. **Slot grid**, remaining height (200 - 24 - 36 - 8 = 132): the grid's outer
   edges are the panel's content edges, i.e. flush with the header row (left edge of the
   set picker, right edge of the `8` toggle). No extra inset: the slot focus ring is drawn **inside**
   the cell (see Focus below), because the cells are flush with the panel. Cell top = header bottom + 8, last row bottom =
   panel bottom - 12. 2 rows by 4 columns
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
the X50): a 6 x 8 solid triangle at the cell's top-right, `cyan`. Focus: 2px `cyan` ring drawn
inside the cell, its outer edge 2 px inside the cell's outer edge (so it sits within the
8 px cell padding and never overflows). Double-click edits the label in place (an inline field in `field`
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
content on a `panel` card (radius 8, padding 16, inset 16 from the dialog edge). Title bar
is the OS title bar, text `Settings`. Closes with `Close` (primary, 120 x 40) or Esc.
**Close sits below the card, outside it** (D3, matches the build; the mockup drew it
inside): 16 below the card, its right edge on the card's right edge, 16 above the dialog
bottom.

Sections, vertical gap 16, each starts with a `label` style title in `text-dim`:

1. **`MIDI PORTS`**: two rows, label column 96 wide (`body`), combo stretch, 40 high:
   `Output` combo, `Input` combo, plus `Test` (secondary, 80 x 40) at the end of the
   Output row. Combo style: like inputs; the drop-down arrow is a painted chevron.
2. **`IF THE X50 DOES NOT ANSWER`** (manual settings): `Channel` spin box (1 to 16,
   default 1, 40 high, 96 wide) and `Bank Map` combo (`GM`, `KORG`, default `GM`, 40 high,
   160 wide) on one row, with a `small` `text-dim` note beneath: `Used only when the X50
   gives no answer.` Enabled state (D3, was unspecified): both are **disabled** (text
   `disabled-text`, fill `panel`) while the last check found the X50 (they are then not
   used), and enabled in every other case (never checked, X50 not found, check failed).
   Owner to confirm.
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
rule** in `red` (fail) or `amber` (unknown) at x 12 from the row's left edge (the glyph centre line, drawn from
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

Box model (D3): QSS `min-/max-height` and `min-/max-width` are **content-box**; the border
is added outside. A 40 px control with a 1px border writes `min-height: 38px` in the QSS
(toggle: content width = total 112 - `padding-left` 20 - both borders, so `min-width: 90px`
unchecked (1px border) and `min-width: 88px; min-height: 36px` in `:checked` (2px border);
padding is added too, which the first D3 wording missed and made the toggle 132 wide).
`app.qss` now states both, so the widget-level override in `KindToggle` and `SetEditor`
(the `6 | 8` toggles use `padding-left: 0`, so there content = total - borders: 34 and 32)
can be deleted. The `QHeaderView::section`
height is 27 (the list's 1px border is the 28th). `app.qss` is already written this way;
its header comment lists the same tokens as §1.1 (the old `lcd-bg`, `lcd-text` are gone).

Rule for everything painted by QPainter (live buttons, top bar, display strip,
checklist, delegate): colours and sizes come from `Theme`, which mirrors the tokens in
§1 and §3; never hard-code hex in a paint method.

---

## 11. Builder checklist

Every value the builder needs and where it is. Nothing below is left to choose.

- [x] Palette hex, names, ratios: §1.
- [x] Six font files (Pixel Operator X50 included, which now also sets the live button
      names), sources, licences, load and fallback: §2. Type roles with sizes,
      tracking, case, line height: §2.1.
- [x] Spacing scale, live spacing, radii, border widths, pixel snapping: §3.
- [x] High-DPI policy, minimum window size, default size: §4.
- [x] Live unit formula and clamp: §5.1. Live stack, grid, gaps, margins, column and row
      maths: §5.2. Button anatomy with every vertical measurement: §5.3.
- [x] Fit algorithm for the dot-matrix names (whole `k`, `kcap`, floor, two-line rule,
      long-word fallback, placement, per-scale table, no ellipsis), bottom-block fit,
      cache rule: §5.3.
- [x] Every live state with fill, border, text, badge, tag; non-colour lit signals;
      empty, pressed, focused, disconnected: §5.4, §6.
- [x] Top bar zones, widths, shrink rules, error and flash behaviour: §5.5.
- [x] Esc hint text, placement, armed text, when shown: §5.6.
- [x] 1x, 1.25x, 1.5x, 2x on 1920x1080 and 1366x768, with button sizes: §5.7.
- [x] 6 versus 8 slot differences: §5.8.
- [x] Repaint discipline and what triggers a repaint: §6.5.
- [x] Browse regions, sizes, resize rules, minimum size: §7.1. Display strip: §7.2.
      Toolbar, search, toggles: §7.3 to §7.5. Category list: §7.6. Result row delegate
      with exact row height and columns, edited and favourite markers: §7.7. Transport,
      Play: §7.8. Set editor grid and cell: §7.9. Status line: §7.10.
- [x] Live button names in Pixel Operator X50 Bold: §2.1, §2.2, §5.3.
- [x] LCD font (Pixel Operator), grid sizes, Qt rendering settings, baseline formula:
      §2.2. LCD readout in live top bar, exact strings, colours, padding, fit: §5.5.
      Browse display strip with exact boxes, offsets, strings, unknown state: §7.2.
- [x] Settings dialog, probe checklist states with glyph shapes, fix hint style: §8.
- [x] Icon brief: §9. QSS object names and properties: §10.

Values the builder must still decide (none visual): the `Theme` class's API and the
exact names of C++ constants (mirror the token names with `camelCase`). The mockups
(T2.3) are authoritative for any visual question this text cannot answer; if the
mockup and this text disagree, this text wins and the mockup is a bug.

### Re-read pass: values I originally left implicit, now filled

0. (G2 typography) Pixel size at each scale and how it snaps (§2.2); the category index
   rule and its unverified order (§7.2); GM number format (§5.5, §7.2); unknown-sound
   look of both panels; what is outside the panels (connection, NOT IN SET).

0b. (G2 approved) Live button names use Pixel Operator X50 Bold; sizes in whole `k`,
   fit by §5.3, painted in device-pixel space like the LCD panels; address stays Barlow,
   sound-name line stays Atkinson Regular. Atkinson Hyperlegible is still used by
   `bar-set`, `live-sound`, `body`, `body-strong`, `small`, `title`, so it is not removable.

1. Name box height formula and what happens to leftover height: §5.3 (button height - 140).
2. The inset rule for borders so thick borders do not shift content: §3.5.
3. Lit button badge and tag colours: §5.4.
4. Which key cap is drawn in the hint and when it changes: §5.6.
5. What the disconnected state does to the buttons (nothing), flash duration (2 s): §5.5, §6.3.
6. Where the current X50 sound shows in browse lists (triangle) and in the set editor: §7.7, §7.9.
7. Fallback fonts and the family-name check for the Barlow files: §2.
8. Scale behaviour on non-16:9 windows: §5.7.

### Not fixable from here (needs the owner or hardware)

- Category index order (§7.2): only `Keyboard` = 00 (both modes) and PROG `SlowSynth` =
  09 are proven by the manual. The mockup's program category list (§7.6) puts SlowSynth
  at position 8, so its order is wrong somewhere; it must come from the unit or the VNL.
- (Resolved in change round G2c: `&` and `0` redrawn in the patched font, §2.2.)

- Whether Barlow Semi Condensed registers under the family name `Barlow Semi Condensed`
  on Windows Qt: check at first run (§2); not verified.
- The 1 m legibility claim is computed, not measured (§5.7). Owner to test on the real laptop.
- The "lit button stays when disconnected" rule is my decision where SPEC is silent (§6.3).

---

## 12. Mockups

Sources are generated SVG in `docs/design/mockup/` (`lib.py`, `live.py`, `browse.py`,
`settings.py`; `python3 build.py` rewrites the SVGs and renders the PNGs with Inkscape
using a throwaway fontconfig that points at `assets/fonts/`, so Atkinson, Barlow and Pixel
Operator X50 are the committed files). `fit_table.py` prints the §5.3 size table. Text is measured with the same font files, so the §5.3 fit rule in
`live-8` and `live-6` is the real algorithm's output. All at 100% scale, 1x, no OS title bar.

| File | Canvas | Shows |
|---|---|---|
| `docs/design/mockup/live-8.svg`, `docs/design/mockup-live-8.png` | 1920x1080, `u` = 1.5 | 8 slots; slot 2 lit (NOW), button names in Pixel Operator X50 Bold: slot 3 `Piano &Strings` on two lines at `k` = 6 (96 px, balanced break), slots 1, 2, 5, 7 two lines at `k` = 7 (112 px), slot 6 empty, 16-character `StereophonicKeys` and `Waiting4Sunrise` one line at `k` = 3 (48 px), connected top bar with the LCD readout panel |
| `docs/design/mockup/live-6.svg`, `docs/design/mockup-live-6.png` | 1920x1080, `u` = 1.5 | 6 slots, names in Pixel Operator X50 Bold (`k` = 7, 112 px, one or two lines); X50 on a sound not in the set: `NOT IN SET` tag beside the LCD readout panel, no lit button |
| `docs/design/mockup/browse.svg`, `docs/design/mockup-browse.png` | 1920x1080 client area (maximised window) | search `piano`, Combis and Programs on, GM off, program category list, 16 of 32 result rows, favourite stars, `EDITED` pill, selected row, current-sound triangle, display strip, transport bar, 8-slot set editor |
| `docs/design/mockup/settings.svg`, `docs/design/mockup-settings.png` | 520x709 (content height; minimum is 560) | ports, manual settings, check running: pass, fail with hint, pass, unknown with hint, running |

Values changed in this document while drawing (the text above already carries them):

1. §7.3, §7.5: kind toggles 88 -> **100** wide (`Programs` plus its check mark did not fit
   in 88). Search field at the minimum window 344 -> 308.
2. §5.6: key cap border `2px` -> **2u**; cap height **24u** added.
3. §8.1: hint rule x `24` -> **12** (the doc said glyph centre line, which is 12).

Values that were implicit and are now fixed (also written into the sections above):

4. §5.2: grid column widths are rounded, the last column takes the remainder (440, 440, 440, 438).
5. §5.3: a long single word (`StereophonicKeys`) sets on one line at `k` = 3 (superseded
   by the G2 approved change: the Atkinson 30u figure no longer applies).
6. §5.3: tag triangle or chevron sits 8u left of the tag text.
7. §5.5: address in the top bar and the display strip is bank and number only (`D104`);
   `NOT IN SET` tag 16u after the name, 32u high, 10u padding.

Drawing choices the doc did not specify, used in the mockups (builder: treat as decided
unless the owner objects):

- Browse at 1920 wide: category column 200, then the 6 px splitter, then the right column
  (1682). The result list's vertical scrollbar takes 12 of its width; rows are 1668 wide
  inside the 1 px border, and the `Name` column is 1252.
- Live bottom block (72 px at `u` 1.5): the lower line's box is bottom-aligned, the address
  line sits directly above it, any slack is at the top. Without a label the address line
  alone is bottom-aligned.
- Empty set-editor cell keeps its digit box (dashed, `disabled-text` digit) beside `EMPTY`.
- Settings: card inset 16 from the dialog edge, padding 16, section title row 16 high with 8
  below it, spin box arrows are two painted chevrons on the right, status word shows
  `Checking…` while the probe runs, placeholder port name `X50 MIDI 1` (the real Windows
  name is unknown, MIDI.md).
- Settings dialog at this content is 709 high, under the 80% of 1080 cap (864).

- 2026-11-21: browse category list gains `Favourites` and `Recent` above `All` (owner
  decision, SPEC §4.1): §7.6 order, separator, star and clock glyphs, text indent; §7.7
  ordering (Favourites catalogue order, Recent newest first), column behaviour, three
  empty-state texts. Static mockup not updated.

- 2026-11-21 (D3 review, `docs/design/review-M3/D3.md`): toolbar widths (toggles 112,
  `Read names` 126, min-width rule `max(listed, advance + 32)`), kind toggle inset and
  check gap, result header 28 = border + 27, cell padding on outer edges only, `EDITED`
  pill left-aligned in its column, Prev/Next 90 with 8 gap chevrons, set editor header
  buttons 84/96/106/84 and slot grid flush with the header, Settings Close below the
  card, manual fields disabled after a passing check, pixel font placed per character
  (§2.2 rule 2), QSS content-box rule (§10), strict pixel snapping limited to painted
  items (§3.5). `app.qss` header and min sizes aligned.

Change round G2 (2026-11-21, owner-approved changes):

- 2026-11-21: NEXT state removed from live view (§5.3 tag, §5.4 table and note, §1.1 and
  §1.2 `strip` uses, §3.4, §6.2, §6.5, checklist) and from `live.py`. Only the lit slot
  stands out.
- 2026-11-21: §5.3 fit rewritten: a name with a space breaks at the best-balanced space
  onto two lines whenever its one-line size would be below the two-line start size
  (44u at 8 slots, 52u at 6). `Piano &Strings` is two lines at 44u (66 px) in `live-8`
  (slot 3), and would be two lines at 52u (78 px) in the 6-slot grid.
- 2026-11-21: browse results are in catalogue order (§7.7). The mockup rows were
  re-selected from `data/x50-factory.tsv`: 13 COMBI rows then PROG A001, A065, A081 are
  the 16 visible of 32. To keep every row feature visible, the example's current sound
  is now `COMBI C096 Piano &Strings` (triangle, set-editor slot 3 marker), the edited
  example is `PROG A065` (`M1 Piano Soft`), favourites B064, C096 and B001.
- 2026-11-21: LCD look (owner description wins over REFERENCES §1, no note there):
  browse display strip is black-on-pale with a reversed segment (§7.2, tokens
  `lcd-field`, `lcd-ink`, `lcd-bezel`, `lcd-warn`); in live view only the top bar's
  current-sound readout is an LCD panel with a dimmed field `lcd-field-dim` (§5.5). Tokens
  `lcd-bg` and `lcd-text` are gone. Readout fonts are Barlow Semi Condensed. The lit
  button cyan is unchanged. Regenerated: `mockup-live-8.png`, `mockup-live-6.png`,
  `mockup-browse.png`.

Change round G2b (2026-11-21, owner follow-up on typography):

- New font Pixel Operator (CC0), Regular and Bold, for the two LCD panels only (§2.2);
  files and licence in `assets/fonts/`, listed in `assets/README.md`. Live buttons stay
  Atkinson Hyperlegible (superseded at G2 approved, below).
- §7.2 rewritten: the strip is an LCD panel `480k x 74k` echoing the X50 display (inverse
  mode and page segments, `Bank` and `Category` row, inverse sound bar). Strip height 64 ->
  `74k` (§7.1 arithmetic updated: result list floor 214, 5 rows). Connection status moved
  outside the panel onto `bg-browse` (cyan LED, `text`).
- §5.5 centre zone rewritten: one-line readout, inverse mode segment `PROG 0:Play` plus
  inverse sound bar `D104: Classic E.Piano`, both Pixel Operator Bold; panel field
  `lcd-field-dim` unchanged; `kb`, `km` formula; fit rule lowers `kb` then elides.
- §2.1: roles `bar-now` and `strip-main` superseded by `lcd-*` roles; token `lcd-warn`
  unused. The display strip shows `Category 00:Keyboard` using the manual's zero-based
  index (the brief said 01; the manual says 00).
- `mockup-live-8-pixelfont.png` added for the owner to compare names in Pixel Operator
  (since merged into `mockup-live-8.png` and deleted, see G2 approved below).
  Regenerated: `mockup-live-8.png`, `mockup-live-6.png`, `mockup-browse.png`.

Change round G2c (2026-11-21, glyph defects):

- `&` read as `6` and the slashed zero as `8`. `PixelOperator.ttf` and `-Bold.ttf` are
  replaced by `PixelOperatorX50.ttf` and `-Bold.ttf` (dotted zero, redrawn ampersand,
  family `Pixel Operator X50`; §2, §2.2). Unpatched files deleted; CC0 licence kept.
  `assets/README.md` records the modification. Reproduce with
  `docs/design/mockup/patch_font.py`; test strip `docs/design/mockup/glyph-test.png`
  (`glyph_test.py`). Builder: load the new file names and use family `Pixel Operator X50`.
  Regenerated all mockup PNGs.

G2 approved (2026-11-21). The owner approved round G2 with these choices:

- No NEXT state in live view (only the lit slot stands out).
- Two-line names: a name with a space breaks onto two larger lines before shrinking.
- Browse results in catalogue order.
- LCD look with Pixel Operator X50 (browse strip and live top-bar readout).
- Dot-matrix button names (option B): live-view big names use Pixel Operator X50 Bold,
  not Atkinson Hyperlegible. §2.1, §2.2, §5.3 (algorithm rewritten for whole-`k` sizes,
  table per screen and scale), §5.7 and §5.8 updated. The small address line (Barlow
  SemiBold) and the label's sound-name line (Atkinson Regular) are unchanged.
  `mockup-live-8.png` and `mockup-live-6.png` regenerated with it;
  `mockup-live-8-pixelfont.png` and its SVG deleted; `build.py` no longer builds them;
  `mockup/fit_table.py` added. Atkinson Hyperlegible is still used by `bar-set`,
  `live-sound`, `body`, `body-strong`, `small` and `title`, so no font file is removable.

Open question for the owner, not changed: at wide browse windows the `EDITED` pill sits at
the right edge of the very wide `Name` column, far from the name it describes (§7.7 puts
it in its own 60 px column). Moving it to follow the name text would read better; this is
left as specified until the owner says.

Change round D4 (2026-11-21, live view review, `docs/design/review-M4/D4.md`):

- Bold `&` redrawn (§2.2): 7 px wide, open on the right, so it no longer reads as `8` at
  `k` = 6 or 3. `PixelOperatorX50-Bold.ttf` regenerated by `patch_font.py` (only this glyph
  differs from the previous file; every advance and metric is unchanged, so the §5.3 fit
  table and widths hold). `mockup-live-8.png` regenerated. Builder: replace the font file,
  nothing else.
- §5.4 note corrected: the readout field is acknowledged as brighter than the lit fill
  (wording only, no value changed). §3.5 is restated by D4 item 2: stroke device width
  `max(1, round(w_design * u * dpr))`, never rounded to logical px first.
