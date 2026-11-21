#!/usr/bin/env bash
# Builds "Guia rapida.pdf" from docs/GUIDE.md: pandoc (Markdown to HTML) then headless Chrome
# (HTML to PDF, A4). Usage: build-guide-pdf.sh OUTPUT.pdf [CHROME]
# Needs pandoc, a Chrome/Chromium, and DejaVu Sans (fallback for U+2717). No LaTeX.
set -euo pipefail
out="$(realpath -m "$1")"
root="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
chrome="${2:-}"
if [ -z "$chrome" ]; then
  for c in google-chrome google-chrome-stable chromium chromium-browser; do
    if command -v "$c" >/dev/null; then chrome="$c"; break; fi
  done
fi
test -n "$chrome" || { echo "no Chrome/Chromium found" >&2; exit 1; }
dejavu="$(fc-match -f '%{file}' 'DejaVu Sans:style=Book')"
case "$dejavu" in *DejaVuSans.ttf) ;; *) echo "DejaVu Sans not found (got: $dejavu)" >&2; exit 1;; esac

work="$(mktemp -d)"
trap 'rm -rf "$work"' EXIT
sed -e "s|@FONTDIR@|file://$root/assets/fonts|g" -e "s|@DEJAVU@|file://$dejavu|g" \
  "$root/packaging/guide/guide.css" > "$work/guide.css"
pandoc "$root/docs/GUIDE.md" -f markdown -t html5 -s --metadata lang=es \
  --metadata title="Guía rápida" --css "$work/guide.css" -o "$work/guide.html"
# pandoc adds the title block from metadata; the Markdown already has its own H1.
sed -i '/<header id="title-block-header">/,/<\/header>/d' "$work/guide.html"
"$chrome" --headless --no-sandbox --disable-gpu --no-pdf-header-footer \
  --allow-file-access-from-files --print-to-pdf="$out" "file://$work/guide.html" 2>/dev/null
test -s "$out"
echo "wrote $out"
