# Assets

| File | Source | Licence |
|---|---|---|
| `fonts/AtkinsonHyperlegible-Regular.ttf`, `-Bold.ttf` | https://github.com/googlefonts/atkinson-hyperlegible (`fonts/ttf/`) | SIL OFL 1.1, `fonts/OFL-AtkinsonHyperlegible.txt` |
| `fonts/BarlowSemiCondensed-Medium.ttf`, `-SemiBold.ttf` | https://github.com/jpt/barlow (`fonts/ttf/`) | SIL OFL 1.1, `fonts/OFL-Barlow.txt` |
| `fonts/PixelOperatorX50.ttf`, `-Bold.ttf` | Pixel Operator by Jayvee Enaguas (HarvettFox96), zip from https://www.dafont.com/pixel-operator.font (upstream https://notabug.org/HarvettFox96/ttf-pixeloperator), **modified**: the zero is redrawn as a dotted zero and the ampersand is redrawn as a classic pixel ampersand (upstream `&` read as `6`, slashed zero read as `8`); family renamed "Pixel Operator X50"; metrics and all other glyphs unchanged. Reproduce with `docs/design/mockup/patch_font.py`. Used for the LCD panels only | CC0 1.0, `fonts/CC0-PixelOperator.txt` |
| `fonts/CC0-PixelOperator.txt` | `LICENSE.txt` from the same zip | CC0 1.0 Universal legal text |
| `fonts/OFL-AtkinsonHyperlegible.txt` | https://raw.githubusercontent.com/googlefonts/atkinson-hyperlegible/main/OFL.txt | OFL 1.1 text |
| `fonts/OFL-Barlow.txt` | https://raw.githubusercontent.com/jpt/barlow/master/OFL.txt | OFL 1.1 text |
| `icons/app.svg` | hand-written for this project (docs/design/DESIGN.md section 9), 32 px and larger | project licence |
| `icons/app-small.svg` | hand-written, 2 x 2 variant used for the 16 and 24 px frames | project licence |
| `icons/app-256.png` | rendered from `app.svg` with Inkscape | project licence |
| `icons/app-512.png`, `icons/app-1024.png` | rendered from `app.svg` with Inkscape (`inkscape app.svg -w 1024 -h 1024 -o app-1024.png`); 1024 is the master for the macOS .icns | project licence |
| `icons/app.ico` | 16, 24, 32, 48, 64, 128, 256 px frames, assembled with ImageMagick | project licence |
| `theme/app.qss` | hand-written from DESIGN.md section 10 | project licence |
