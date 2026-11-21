# References

Research for the visual direction. Every observation says where it came from. "Observed"
means I looked at the photograph or page myself; "reported" means a page's text said so;
"unverified" means I could not confirm it. Nothing here is copied from a manual. Looked
up 2026-11-21.

No photograph is stored in the repository. The images were viewed from KORG's CDN and
Vintage Synth Explorer into a scratch folder only.

## 1. The KORG X50 panel and LCD

Sources used:

- [S1] KORG Germany, X50 features page: https://www.korg.com/de/products/synthesizers/x50/page_1.php (carries the product photographs; the page says the product is discontinued)
- [S2] KORG Germany, X50 specifications: https://www.korg.com/de/products/synthesizers/x50/page_2.php
- [S3] KORG Germany, X50 overview (hero photograph): https://www.korg.com/de/products/synthesizers/x50/
- [S4] Vintage Synth Explorer: https://www.vintagesynth.com/korg/x50 (photo: https://www.vintagesynth.com/sites/default/files/2019-07/KorgX50.jpg)
- [S5] Audiofanzine technical sheet and user reviews: https://en.audiofanzine.com/digital-synth/korg/X50/

Observations:

1. **Body colour.** Near-black charcoal plastic chassis with rounded end cheeks; seen from
   the front in the hero photo. Sampled in the close-up photo, the dark panel area is
   about `#2E3337` to `#414C4E` (JPEG, lit studio photo, so approximate). [S3] [S1] [S4]
2. **End pieces.** Two silver or light grey tubular handles run along the left and right
   ends, standing out from the black body. [S3] [S4]
3. **Panel layout and finish.** The panel is dark with three zones: left (wheels, volume
   slider, four knobs), centre (LCD), right (data wheel, joystick-like cursor pad,
   buttons). The zones are separated by lighter blue-grey metallic-looking strips that
   carry silkscreen text. Sampled strip colour about `#B2BBCA` in the close-up. The
   finish reads as satin or matte, not glossy; I could not judge texture from a
   photograph. [S1] [S3]
4. **Labelling.** White (and some light grey) printed text on the black panel, small,
   uppercase for most section titles ("REALTIME CONTROLS", "VOLUME", "CATEGORY",
   "PROG", "COMBI", "GLOBAL", "MULTI"); dark text on the lighter strips. The face looks
   like a clean, neutral, medium-weight sans of the Helvetica/Univers family, wide
   letter spacing on titles. I cannot identify the exact typeface: unverified. The "KORG"
   and "X50" panel logotypes are visible and must not be reproduced. [S1] [S3]
5. **Buttons.** Small, soft-edged, rounded-rectangle buttons in pale blue-grey (about
   `#9AAFCC` sampled), set into the black panel; the COMBI button is a pink/red
   translucent button. A larger red/pink button also sits at the left in the hero photo.
   Some small red LEDs (e.g. beside the knob bank selector) are visible. [S1] [S3] [S4]
6. **Knobs.** Four round silver/aluminium-look knobs with a ribbed grip and a dark base
   ring, on the left. The data wheel on the right is a larger black disc. [S1]
7. **Keys.** White keys appear pale grey-white, black keys glossy black. Not relevant to
   the app beyond mood. [S1]
8. **LCD technology.** Graphical LCD, 240 x 64 pixels, backlit (reported by KORG as
   "Display (240 x 64 Pixel), hintergrundbeleuchtet"). One search listing gave 160 x 64;
   KORG's own page is taken as authoritative. [S2]
9. **LCD look.** Observed: a wide, short rectangle, sky-blue/cyan backlight (sampled about
   `#5DB0D0` to `#61AECA`) with dark blue-grey text and reversed (filled dark) bars for
   selected fields. It is a graphical display with proportional-looking small text, not
   a fixed character grid; the sound name line (a reversed bar with address and name,
   e.g. a "000:" style address then the name) is the largest text. Bank letter and
   category shown on the line above. Text is too small in the photographs to read
   exactly, so exact font and character count are unverified. [S1]
10. **Accent colours.** Red/pink for COMBI and for LEDs, blue-grey for the neutral
    buttons, LCD cyan. No orange or green seen on the panel. [S1] [S3]
11. **Size and weight.** 61 keys; 975 x 257 x 80 mm (S2 reported); weight reported as
    4.3 kg [S2] and 4.5 kg [S5]. Reviewers call it light and plasticky. [S4] [S5]
12. **Editor software look.** KORG's X50 editor screenshot (on S1) is black and dark red
    with red controls and a red-on-black top bar; it shows the sound as "000 Big Sweep
    Stab" in a white, large-ish sans on a black field. This is software, not the panel,
    but is a second period reference. [S1]

Could not verify: the exact panel typeface; the LCD font and character count; exact
colours of the real unit under stage light (all colour values above come from marketing
photographs); whether the panel finish is textured; anything about KORG's own brand
colours beyond what is visible in the photos.

## 2. Stage-legibility references

Sources:

- [L1] OnSong manual, Low Light Mode: https://onsongapp.com/docs/low-light-mode
- [L2] Apple MainStage User Guide (Perform mode, full screen, screen saver option): https://support.apple.com/en-il/guide/mainstage/mstgad6d1ffe/mac
- [L3] Camelot Pro, App Store listing: https://apps.apple.com/us/app/camelot-pro/id1326331127

Findings:

1. OnSong offers a low light mode with black background and white text "intended for dark
   stages to reduce eye strain as well as minimize ambient light"; its manual notes that
   screen light can make a player's face glow white. [L1] **Borrow:** a near-black
   background; no large bright areas. **Avoid:** large white or very bright fills in live
   view; one lit button should be the brightest thing on screen, not a white wall.
2. OnSong's low light mode cannot recolour PDF or image content, so some content stays
   bright. [L1] **Borrow:** the app draws everything itself, so nothing bright sneaks in.
3. MainStage's Perform mode can run full screen and has a setting to stop the screen saver
   during a show. [L2] **Borrow:** full screen live view and keeping the display awake
   (already in SPEC §4.2). **Avoid:** screen-control layouts that mix many small
   controls; MainStage selectors rely on the user building a layout.
4. MainStage lets you relabel controls so they are easier to identify. [L2] **Borrow:**
   the set's own labels as the large text (already in SPEC), sound name small.
5. Camelot Pro lets scenes be changed remotely by MIDI or Bluetooth pedals. [L3]
   **Borrow:** foot-pedal navigation (already in SPEC). I found no source describing
   Camelot's on-stage button size or contrast: unverified, do not cite it for those.

Gap: I found no published measurements for text size at 1 m. DESIGN.md must therefore
state its own sizes with the reasoning (distance, angle) and the owner should test on the
real laptop.

## 3. Candidate fonts

All licences read from the project's own repository (OFL 1.1 header seen in each file).
Nothing downloaded to the project yet; T2.3 does that.

KORG-era sans for labels (neutral grotesque, uppercase-friendly, similar spirit to the
panel silkscreen):

1. **Barlow** (and Barlow Semi Condensed). OFL 1.1. https://github.com/jpt/barlow
   (licence: https://raw.githubusercontent.com/jpt/barlow/master/OFL.txt). Slightly
   squared, industrial-signage feel, many weights, and a semi condensed cut that helps
   fit long names. Closest in mood to hardware labelling.
2. **Source Sans 3.** OFL 1.1. https://github.com/adobe-fonts/source-sans
   (licence: https://raw.githubusercontent.com/adobe-fonts/source-sans/release/LICENSE.md).
   Neutral humanist-grotesque, very good hinting on Windows, wide weight range; the safe
   choice for browse view body text.
3. **Inter.** OFL 1.1. https://github.com/rsms/inter
   (licence: https://raw.githubusercontent.com/rsms/inter/master/LICENSE.txt). Closest
   to Helvetica/Univers-style neutrality; tall x-height, tabular numerals, for addresses
   such as "A012". Larger files; may ship a subset or the static weights only.

Highly legible face for live-view names:

4. **Atkinson Hyperlegible.** OFL 1.1, by the Braille Institute, designed for distinct
   letterforms (I, l, 1, O, 0 differ). https://github.com/googlefonts/atkinson-hyperlegible
   (licence: https://raw.githubusercontent.com/googlefonts/atkinson-hyperlegible/main/OFL.txt;
   background: https://brailleinstitute.org/freefont). Chosen for names where "I/l/1"
   confusion matters. Check at T2.3 whether the repository holds usable static TTFs for
   Regular and Bold, or whether "Atkinson Hyperlegible Next" is available instead:
   unverified.

I also considered Public Sans (US Web Design System) and rejected it for now: its
repository's licence file describes a fork of Libre Franklin and I did not read it to the
end, so its terms are unverified.

## 4. Implications for DESIGN.md

1. Background near-black, not pure black and not X50 charcoal: use a very dark neutral
   (about `#0B0D0F`) in live view so the screen emits little light; the panel charcoal
   `#2E3337` family is for browse view surfaces and button bodies only.
2. Lit button must not be a bright fill field. Use the LCD cyan (`#5DB0D0` family) as the
   lit accent, with dark text on it, plus a thick border, a heavier weight and a marker
   glyph (for example a "NOW" tag or filled triangle), so it works without colour.
3. Use the blue-grey strip colour (`#B2BBCA` family) only for thin separators and small
   labels; it is the nod to the panel, not a large surface.
4. Use red/pink (the COMBI button and LEDs) sparingly: connection-lost LED and error
   text only, so red keeps meaning "something is wrong".
5. Section titles and small labels in uppercase with wide tracking, medium weight, light
   grey on dark, copying panel silkscreen style; never in live-view button names.
6. The top bar "display strip" can echo the LCD: a rectangular cyan-tinted field with
   dark text and a reversed bar for the selected line. A nod only, drawn with QPainter
   rectangles, with no logotype and no "KORG" drawing; "KORG X50" appears as plain text.
7. Live-view button names in Atkinson Hyperlegible Bold, sized to fit a 16-character
   name by shrinking, with a stated minimum size and wrap rule; labels and browse UI in
   Barlow or Source Sans 3. Decide at T2.3 whether two families are worth the bytes.
   (Superseded at G2 approved, 2026-11-21: the owner chose Pixel Operator X50 Bold for
   the big button names; see DESIGN.md §5.3.)
8. Round shapes: soft-edged rounded rectangles (small radius) echo the X50 buttons;
   do not imitate the ribbed knobs or the metallic gradient strips.
9. No animation anywhere at idle, no glow effects that need repaint, no gradients that
   change on hover in live view.
10. Because all colours here are sampled from marketing photos, DESIGN.md must set its
    own hex values and have the owner approve them on the mockups, not claim they match
    the hardware.
