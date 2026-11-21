#!/usr/bin/env python3
"""Extract the X50 factory sound list from Korg's VNL PDF into a TSV.

Usage: extract-voice-list.py [VNL.pdf] [out.tsv]

Needs `pdftotext` (poppler). Columns: kind, bank, number, name, category, osc.
kind is COMBI or PROG; bank is A-D, G or g(d); osc is s/d/D or blank.
The PDF is read by path and never copied into the repository.
"""
import os
import re
import subprocess
import sys

DEFAULT_PDF = os.path.expanduser("~/Documentos/Manuales/Korg/X50_VNL_EFGJ1.pdf")
HERE = os.path.dirname(os.path.abspath(__file__))
DEFAULT_OUT = os.path.join(HERE, "..", "data", "x50-factory.tsv")

COMBI_PAGES = range(3, 15)   # banks A, B, C
PROG_PAGES = range(15, 19)   # banks A, B, C, D
GM_PAGE = 19                 # bank G and g(d)
EXPECTED = {"COMBI": 384, "PROG-ABCD": 512, "PROG-G": 128, "PROG-g(d)": 9}
NAME_LEN = 16
KNOWN_CATEGORIES = frozenset([
    "Bass", "BassSplits", "Bell/Mallet", "Bell/Mallet/Perc", "Brass", "BrassReed",
    "Complex & SE", "Drums", "Ds/Hits", "FastSynth", "Guitar/Plucked", "Hit/Arpg",
    "Keyboard", "LeadSplits", "LeadSynth", "MotionSynth", "Orchestral", "Organ",
    "Pads", "SE", "SlowSynth", "Strings", "Synth", "User", "Vocal/Airy",
    "Woodwind/Reed", "World",
])
# (kind, bank) -> exact set of numbers the bank must contain.
EXPECTED_NUMBERS = {}
for _b in "ABC":
    EXPECTED_NUMBERS[("COMBI", _b)] = set(range(0, 128))
for _b in "ABCD":
    EXPECTED_NUMBERS[("PROG", _b)] = set(range(0, 128))
EXPECTED_NUMBERS[("PROG", "G")] = set(range(1, 129))
EXPECTED_NUMBERS[("PROG", "g(d)")] = {1, 9, 17, 25, 26, 33, 41, 49, 57}


def fail(msg):
    sys.stderr.write("extract-voice-list: error: %s\n" % msg)
    sys.exit(1)


def page_text(pdf, n):
    try:
        out = subprocess.run(
            ["pdftotext", "-layout", "-f", str(n), "-l", str(n), pdf, "-"],
            check=True, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
    except (OSError, subprocess.CalledProcessError) as e:
        fail("pdftotext failed on page %d: %s" % (n, e))
    return out.stdout.decode("utf-8", "replace").replace("\f", "").split("\n")


def first_field(s):
    """Text up to the first run of two or more spaces."""
    return re.split(r"\s{2,}", s.strip(), maxsplit=1)[0]


def split_name_category(seg, where):
    """seg: text after the flag. Name is a fixed 16-char field, then category."""
    # The PDF font is proportional, so the category may start right after a
    # 15-char name plus one space, or after a 16-char name plus a space.
    cut = NAME_LEN if seg[NAME_LEN:NAME_LEN + 1].isspace() or len(seg) <= NAME_LEN else NAME_LEN - 1
    if cut == NAME_LEN - 1 and not seg[cut:cut + 1].isspace():
        fail("name field not followed by a space at %s: %r" % (where, seg))
    return seg[:cut].strip(), first_field(seg[cut:])


def parse_combis(pdf):
    rows = []
    row_re = re.compile(r"^\s*([A-C])(\d{3})\s+")
    for p in COMBI_PAGES:
        lines = page_text(pdf, p)
        for i, line in enumerate(lines):
            m = row_re.match(line)
            if not m:
                continue
            if i + 1 >= len(lines):
                fail("combination row without category line, page %d" % p)
            name, _ = split_name_category(line[m.end():].rstrip(), "page %d" % p)
            cat = first_field(lines[i + 1])
            if not cat:
                fail("empty category, page %d: %r" % (p, line))
            rows.append(("COMBI", m.group(1), int(m.group(2)), name, cat, ""))
    return rows


def parse_programs(pdf):
    rows = []
    # Two columns per line; flag column sits at a fixed offset from the code.
    code_re = re.compile(r"(?:(?<=\s)|^)([A-D])(\d{3})(?: +([sdD])(?= ))? +(?=\S)")
    for p in PROG_PAGES:
        for line in page_text(pdf, p):
            ms = list(code_re.finditer(line))
            for k, m in enumerate(ms):
                end = ms[k + 1].start() if k + 1 < len(ms) else len(line)
                name, cat = split_name_category(line[m.end():end].rstrip(), "page %d" % p)
                rows.append(("PROG", m.group(1), int(m.group(2)), name, cat,
                             m.group(3) or ""))
    return rows


def parse_gm(pdf):
    rows = []
    gm_re = re.compile(r"(?:(?<=\s)|^)G(\d{3})\s+([sd]) ")
    kit_re = re.compile(r"(?:(?<=\s)|^)g\(d\)(\d{3})\s+(?=\S)")
    anchor = re.compile(gm_re.pattern + "|" + kit_re.pattern)
    for line in page_text(pdf, GM_PAGE):
        ms = list(anchor.finditer(line))
        for k, m in enumerate(ms):
            end = ms[k + 1].start() if k + 1 < len(ms) else len(line)
            seg = line[m.end():end].rstrip()
            if m.group(1) is not None:  # G row
                name, cat = split_name_category(seg, "GM page")
                rows.append(("PROG", "G", int(m.group(1)), name, cat, m.group(2)))
            else:
                name, cat = split_name_category(seg, "GM page")
                rows.append(("PROG", "g(d)", int(m.group(3)), name, cat, ""))
    return rows


def main():
    pdf = sys.argv[1] if len(sys.argv) > 1 else DEFAULT_PDF
    out = sys.argv[2] if len(sys.argv) > 2 else DEFAULT_OUT
    if not os.path.isfile(pdf):
        fail("VNL PDF not found: %s" % pdf)

    combis, progs, gm = parse_combis(pdf), parse_programs(pdf), parse_gm(pdf)
    gm_prog = [r for r in gm if r[1] == "G"]
    gm_drum = [r for r in gm if r[1] == "g(d)"]
    got = {"COMBI": len(combis), "PROG-ABCD": len(progs),
           "PROG-G": len(gm_prog), "PROG-g(d)": len(gm_drum)}
    if got != EXPECTED:
        fail("unexpected counts %r, expected %r; nothing written" % (got, EXPECTED))

    rows = combis + progs + gm_prog + gm_drum
    seen = set()
    for r in rows:
        key = (r[0], r[1], r[2])
        if key in seen:
            fail("duplicate address %r" % (key,))
        seen.add(key)
        if not r[3] or not r[4] or len(r[3]) > NAME_LEN or re.match(r"[sdD] ", r[3]):
            fail("bad name/category in row %r" % (r,))

    numbers = {}
    for r in rows:
        numbers.setdefault((r[0], r[1]), set()).add(r[2])
        if r[4] not in KNOWN_CATEGORIES:
            fail("unknown category %r in row %r" % (r[4], r))
    if numbers != EXPECTED_NUMBERS:
        for key in sorted(set(numbers) | set(EXPECTED_NUMBERS)):
            got, want = numbers.get(key, set()), EXPECTED_NUMBERS.get(key, set())
            if got != want:
                fail("bank %r: missing %r, unexpected %r; nothing written"
                     % (key, sorted(want - got), sorted(got - want)))

    os.makedirs(os.path.dirname(os.path.abspath(out)), exist_ok=True)
    with open(out, "w", encoding="utf-8", newline="\n") as f:
        for r in rows:
            f.write("\t".join((r[0], r[1], "%03d" % r[2], r[3], r[4], r[5])) + "\n")
    print("wrote %d rows to %s" % (len(rows), out))


if __name__ == "__main__":
    main()
