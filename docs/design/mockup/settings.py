"""Settings dialog mockup (DESIGN.md section 8). 520 px wide client area, height by content
(minimum 560), 100% scale, no OS title bar."""
from lib import *
from browse import chevron_down, chevron

DW = 520
MARGIN = 16       # dialog margin around the card (s4)
PAD = 16          # card padding


def glyph(s, kind, gx, gy):
    """24x24 glyph box at (gx, gy); shapes differ per state (DESIGN 8.1)."""
    cx, cy = gx + 12, gy + 12
    if kind == 'pass':
        s.circle(cx, cy, 10, fill=C['green'])
        s.poly([(cx - 4.5, cy + 0.5), (cx - 1.5, cy + 3.5), (cx + 4.5, cy - 3.5)], stroke=C['on_cyan'], sw=2,
               close=False, round_=True)
    elif kind == 'fail':
        s.rect(cx - 10, cy - 10, 20, 20, fill=C['red'], rx=4)
        s.line(cx - 4, cy - 4, cx + 4, cy + 4, C['on_red'], 2, 'round')
        s.line(cx + 4, cy - 4, cx - 4, cy + 4, C['on_red'], 2, 'round')
    elif kind == 'unknown':
        d = 10.5
        s.poly([(cx, cy - d), (cx + d, cy), (cx, cy + d), (cx - d, cy)], stroke=C['amber'], sw=2, round_=True)
        s.line(cx - 3.5, cy, cx + 3.5, cy, C['amber'], 2, 'round')
    else:  # running
        s.circle(cx, cy, 9, stroke=C['cyan_text'], sw=2)
        for dx in (-4.5, 0, 4.5):
            s.circle(cx + dx, cy, 1.5, fill=C['cyan_text'])


def wrap(text_segments, wbox, size):
    """text_segments: list of (string, bold). Greedy word wrap -> lines of [(word, bold)]."""
    words = []
    for t, b in text_segments:
        for w in t.split(' '):
            if w:
                words.append((w, b))
    lines, cur, curw = [], [], 0.0
    sp = adv(' ', 'ar', size)
    for w, b in words:
        ww = adv(w, 'ab' if b else 'ar', size)
        if cur and curw + sp + ww > wbox:
            lines.append(cur)
            cur, curw = [], 0.0
        curw += (sp if cur else 0) + ww
        cur.append((w, b))
    lines.append(cur)
    return lines


def field(s, x, y, w, text, chev=True):
    s.rect(x, y, w, 40, fill=C['field'], stroke=C['border'], sw=1, rx=6)
    s.text(x + 12, y + 20, text, 'ar', 15, C['text'])
    if chev:
        chevron_down(s, x + w - 20, y + 20)


def build(outdir):
    log = []
    ops = []   # deferred drawing because height depends on content

    cx0 = MARGIN + PAD       # content left
    cw = DW - 2 * (MARGIN + PAD)   # 456
    y = MARGIN + PAD

    def title(t):
        nonlocal y
        ops.append(('title', t, y))
        y += 16 + 8

    # 1 MIDI ports
    title('MIDI PORTS')
    ops.append(('row_port', 'Output', y, True)); y += 40 + 8
    ops.append(('row_port', 'Input', y, False)); y += 40
    y += 16
    # 2 manual
    title('IF THE X50 DOES NOT ANSWER')
    ops.append(('manual', y)); y += 40 + 4
    ops.append(('note', y)); y += round(13 * 1.3)
    y += 16
    # 3 check
    title('X50 CHECK')
    ops.append(('checkbtn', y)); y += 40 + 8
    rows = [
        ('pass', 'X50 found (system 1.05, global channel 1)', None),
        ('pass', 'Program Change enabled', None),
        ('fail', 'Bank Change disabled', [('GLOBAL 1: MIDI, tick ', False), ('"Bank"', True)]),
        ('pass', 'Combination Change enabled', None),
        ('unknown', 'Bank Map: not reported', [('The X50 did not say which Bank Map it uses. Sending as GM; change '
                                              'Bank Map above if banks come out wrong.', False)]),
        ('running', 'SysEx replies', None),
    ]
    for k, msg, hint in rows:
        ops.append(('check', k, msg, y)); y += 32
        if hint:
            lines = wrap(hint, cw - 36, 13)
            lh = 13 * 1.35
            hh = 4 + len(lines) * lh + 8
            # the rule starts at the glyph bottom (row top + 28) and runs to the hint end
            ops.append(('hint', k, lines, y, hh, lh))
            log.append('hint %s: %d line(s)' % (k, len(lines)))
            y += hh
        y += 4
    y -= 4
    y += 16
    ops.append(('close', y)); y += 40
    y += PAD
    card_bottom = y
    total = max(560, card_bottom + MARGIN)
    card_bottom = total - MARGIN
    log.append('settings: content height %d, canvas %dx%d' % (y + MARGIN, DW, total))

    s = Svg(DW, total, C['bg_browse'])
    s.rect(MARGIN, MARGIN, DW - 2 * MARGIN, card_bottom - MARGIN, fill=C['panel'], rx=8)
    for op in ops:
        t = op[0]
        if t == 'title':
            s.text(cx0, op[2] + 8, op[1], 'bs', 12, C['text_dim'], trk=1.5)
        elif t == 'row_port':
            yy = op[2]
            s.text(cx0, yy + 20, op[1], 'ar', 15, C['text'])
            fw = cw - 96 - 8 - (88 if op[3] else 0)
            field(s, cx0 + 96, yy, fw, 'X50 MIDI 1')
            if op[3]:
                bx = cx0 + cw - 80
                s.rect(bx, yy, 80, 40, fill=C['panel_raised'], stroke=C['border'], sw=1, rx=6)
                s.text(bx + 40, yy + 20, 'Test', 'ab', 15, C['text'], anchor='middle')
        elif t == 'manual':
            yy = op[1]
            s.text(cx0, yy + 20, 'Channel', 'ar', 15, C['text'])
            sx = cx0 + 64 + 8
            s.rect(sx, yy, 96, 40, fill=C['field'], stroke=C['border'], sw=1, rx=6)
            s.text(sx + 12, yy + 20, '1', 'ar', 15, C['text'])
            ax = sx + 96 - 22
            s.poly([(ax - 4, yy + 16), (ax, yy + 12), (ax + 4, yy + 16)], stroke=C['text_dim'], sw=2,
                   close=False, round_=True)
            s.poly([(ax - 4, yy + 24), (ax, yy + 28), (ax + 4, yy + 24)], stroke=C['text_dim'], sw=2,
                   close=False, round_=True)
            bx = sx + 96 + 24
            s.text(bx, yy + 20, 'Bank Map', 'ar', 15, C['text'])
            cbx = cx0 + cw - 160
            field(s, cbx, yy, 160, 'GM')
        elif t == 'note':
            s.text(cx0, op[1] + 4 + 6.5, 'Used only when the X50 gives no answer.', 'ar', 13, C['text_dim'])
        elif t == 'checkbtn':
            yy = op[1]
            s.rect(cx0, yy, 140, 40, fill=C['cyan'], rx=6)
            s.text(cx0 + 70, yy + 20, 'Check X50', 'ab', 15, C['on_cyan'], anchor='middle')
            s.text(cx0 + 140 + 16, yy + 20, 'Checking' + '…', 'ar', 13, C['text_dim'])
        elif t == 'check':
            k, msg, yy = op[1], op[2], op[3]
            glyph(s, k, cx0, yy + 4)
            s.text(cx0 + 36, yy + 16, msg, 'ar', 15, C['text_dim'] if k == 'running' else C['text'])
        elif t == 'hint':
            k, lines, yy, hh, lh = op[1], op[2], op[3], op[4], op[5]
            col = C['red'] if k == 'fail' else C['amber']
            # rule from the glyph bottom (4 px above this block's top) to the end of the hint
            s.rect(cx0 + 11, yy - 4, 2, hh + 4, fill=col)
            ty = yy + 4
            for ln in lines:
                x = cx0 + 36
                for w, b in ln:
                    f = 'ab' if b else 'ar'
                    s.text(x, ty + lh / 2.0, w, f, 13, C['text_dim'])
                    x += adv(w + ' ', f, 13) if False else adv(w, f, 13) + adv(' ', 'ar', 13)
                ty += lh
        elif t == 'close':
            yy = op[1]
            s.rect(cx0 + cw - 120, yy, 120, 40, fill=C['cyan'], rx=6)
            s.text(cx0 + cw - 60, yy + 20, 'Close', 'ab', 15, C['on_cyan'], anchor='middle')
    s.save(outdir + '/settings.svg')
    return log
