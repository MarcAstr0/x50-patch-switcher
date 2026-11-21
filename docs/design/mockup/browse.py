"""Browse view mockup (DESIGN.md section 7). Window client area 1920x1080, 100% scale,
no OS title bar. Search 'piano', Combis + Programs on, GM off."""
import csv
from lib import *

W, H = 1920, 1080
M = 16

PROG_CATS = ['Keyboard', 'Organ', 'Bell/Mallet', 'Strings', 'Brass', 'Woodwind/Reed', 'Guitar/Plucked',
             'Bass', 'SlowSynth', 'FastSynth', 'LeadSynth', 'MotionSynth', 'Vocal/Airy', 'Hit/Arpg',
             'Drums', 'SE']

FAV = {('COMBI', 'B064'), ('PROG', 'B001'), ('PROG', 'D104')}
USED = {('PROG', 'D104'): 12, ('COMBI', 'B064'): 3, ('PROG', 'B001'): 7, ('PROG', 'A081'): 2,
        ('COMBI', 'C051'): 5}
EDITED = {('PROG', 'D113'): 'Classic Piano LR'}
CURRENT = ('PROG', 'D104')
SELECTED = ('COMBI', 'B064')


def load_rows():
    rows = []
    with open(ROOT + '/data/x50-factory.tsv', encoding='utf-8') as f:
        for ln in f:
            p = ln.rstrip('\n').split('\t')
            kind, bank, num, name, cat = p[0], p[1], p[2], p[3], p[4]
            if bank in ('G', 'g(d)'):
                continue  # GM toggle is off
            if 'piano' not in name.lower():
                continue
            addr = bank + num
            nm = EDITED.get((kind, addr), name)
            rows.append(dict(kind=kind, addr=addr, name=nm, factory=name, cat=cat))
    rows.sort(key=lambda r_: r_['name'].lower())
    return rows


def button(s, x, y, w, h, label, primary=False, enabled=True, bold=True):
    if primary:
        s.rect(x, y, w, h, fill=C['cyan'], rx=6)
    else:
        s.rect(x, y, w, h, fill=C['panel_raised'], stroke=C['border'], sw=1, rx=6)


def chevron(s, cx, cy, left, col, sw=2):
    d = 4 if left else -4
    s.poly([(cx + d, cy - 6), (cx - d, cy), (cx + d, cy + 6)], stroke=col, sw=sw, close=False, round_=True)


def chk(s, x, y, col):
    s.poly([(x + 1, y + 6.5), (x + 4.5, y + 10), (x + 11, y + 2.5)], stroke=col, sw=2, close=False, round_=True)


def build(outdir):
    log = []
    s = Svg(W, H, C['bg_browse'])
    rows = load_rows()
    log.append('browse: %d matching rows for "piano" (GM excluded), 16 visible' % len(rows))

    # display strip ------------------------------------------------------
    s.rect(M, M, W - 2 * M, 64, fill=C['lcd_bg'], stroke=C['lcd_border'], sw=1, rx=8)
    bx, by, bh = M + 12, M + 8, 48
    mode, addr, name = 'PROG', 'D104', 'Classic E.Piano'
    lab_w = adv(mode, 'bs', 12, 1.5) - 1.5
    addr_w = adv(addr, 'bs', 15, 0.5) - 0.5
    name_w = adv(name, 'ab', 24)
    bw = 16 + lab_w + 12 + addr_w + 12 + name_w + 16
    s.rect(bx, by, bw, bh, fill=C['cyan'], rx=4)
    cy = by + bh / 2.0
    x = bx + 16
    s.text(x, cy, mode, 'bs', 12, C['on_cyan'], trk=1.5)
    x += lab_w + 12
    s.text(x, cy, addr, 'bs', 15, C['on_cyan'], trk=0.5)
    x += addr_w + 12
    s.text(x, cy, name, 'ab', 24, C['on_cyan'])
    label = 'X50 CONNECTED'
    right = W - M - 12
    s.text(right, M + 32, label, 'bs', 15, C['lcd_text'], anchor='end', trk=1.5)
    tw = adv(label, 'bs', 15, 1.5) - 1.5
    led(s, right - tw - 12 - 8, M + 32, 16, True, 2, C['cyan'], C['red'])

    # toolbar ------------------------------------------------------------
    ty = M + 64 + 12
    sw_ = W - 2 * M - (308 + 120 + 100 + 100) - 4 * 8
    s.rect(M, ty, sw_, 40, fill=C['field'], stroke=C['cyan'], sw=2, rx=6)
    # magnifier
    mx, my = M + 12 + 6.5, ty + 20 - 1.5
    s.circle(mx, my, 5, stroke=C['text_dim'], sw=2)
    s.line(mx + 3.6, my + 3.6, mx + 8.5, my + 8.5, C['text_dim'], 2, 'round')
    s.text(M + 36, ty + 20, 'piano', 'ar', 15, C['text'])
    cx = M + 36 + adv('piano', 'ar', 15) + 1
    s.line(cx, ty + 11, cx, ty + 29, C['text'], 1.5)
    # clear x
    qx, qy = M + sw_ - 12 - 20, ty + 10
    s.line(qx + 6, qy + 6, qx + 14, qy + 14, C['text_dim'], 1.5, 'round')
    s.line(qx + 14, qy + 6, qx + 6, qy + 14, C['text_dim'], 1.5, 'round')

    x = M + sw_ + 8
    for lab, on in (('Combis', True), ('Programs', True), ('GM', False)):
        if on:
            s.rect(x, ty, 100, 40, fill=C['sel_row'], stroke=C['cyan'], sw=2, rx=6)
            tw = adv(lab, 'ab', 15)
            tot = 12 + 6 + tw
            x0 = x + (100 - tot) / 2.0
            chk(s, x0, ty + 14, C['text'])
            s.text(x0 + 18, ty + 20, lab, 'ab', 15, C['text'])
        else:
            s.rect(x, ty, 100, 40, fill=C['panel_raised'], stroke=C['border'], sw=1, rx=6)
            s.text(x + 50, ty + 20, lab, 'ab', 15, C['text_dim'], anchor='middle')
        x += 104
    x = M + sw_ + 8 + 308 + 8
    for lab, wd in (('Read names', 120), ('Settings', 100)):
        s.rect(x, ty, wd, 40, fill=C['panel_raised'], stroke=C['border'], sw=1, rx=6)
        s.text(x + wd / 2.0, ty + 20, lab, 'ab', 15, C['text'], anchor='middle')
        x += wd + 8
    s.rect(x, ty, 100, 40, fill=C['cyan'], rx=6)
    lw = adv('Live', 'ab', 15)
    fw = adv('F5', 'bs', 12, 1.5) - 1.5
    x0 = x + (100 - lw - 8 - fw) / 2.0
    s.text(x0, ty + 20, 'Live', 'ab', 15, C['on_cyan'])
    s.text(x0 + lw + 8, ty + 20, 'F5', 'bs', 12, C['on_cyan'], trk=1.5)

    # main area ----------------------------------------------------------
    my0 = ty + 40 + 12            # 144
    mh = H - M - 24 - 12 - my0     # 884
    # category list
    s.rect(M, my0, 200, mh, fill=C['panel'], rx=8)
    s.text(M + 8 + 12, my0 + 8 + 12, 'CATEGORY (PROG)', 'bs', 12, C['text_dim'], trk=1.5)
    cats = ['All'] + PROG_CATS
    ry = my0 + 8 + 24
    for i, c in enumerate(cats):
        if i == 0:
            s.rect(M + 8, ry, 184, 32, fill=C['sel_row'])
            s.rect(M + 8, ry, 4, 32, fill=C['cyan'])
            s.text(M + 8 + 12, ry + 16, c, 'ab', 15, C['text'])
        else:
            s.text(M + 8 + 12, ry + 16, c, 'ar', 15, C['text'])
        ry += 32
    # splitter handle
    s.rect(M + 200, my0, 6, mh, fill=C['line'])

    rx0 = M + 206
    rw = W - M - rx0              # 1682
    lh = mh - 56 - 200 - 24       # 604
    # result list
    s.rect(rx0, my0, rw, lh, fill=C['field'], rx=8)
    s.clip_start(rx0, my0, rw, lh, 8)
    s.rect(rx0, my0, rw, 28, fill=C['panel'])
    inner = rw - 2 - 12
    x0 = rx0 + 1
    hy = my0 + 14
    s.text(x0 + 12 + 36 + 12 - 12 + 0, hy, 'ADDRESS', 'bs', 12, C['text_dim'], trk=1.5)
    s.text(x0 + 12 + 36 + 112, hy, 'NAME', 'bs', 12, C['text_dim'], trk=1.5)
    namew = inner - 24 - (36 + 112 + 60 + 140 + 44)
    cat_x = x0 + 12 + 36 + 112 + namew + 60
    s.text(cat_x, hy, 'CATEGORY', 'bs', 12, C['text_dim'], trk=1.5)
    s.text(cat_x + 140 + 44, hy, 'USED', 'bs', 12, C['text_dim'], anchor='end', trk=1.5)
    y = my0 + 28
    for i in range(16):
        row = rows[i]
        key = (row['kind'], row['addr'])
        sel = key == SELECTED
        if sel:
            s.rect(x0, y, inner, 36, fill=C['sel_row'])
            s.rect(x0, y, 4, 36, fill=C['cyan'])
        elif i < 15:
            s.rect(x0, y + 35, inner, 1, fill=C['line'])
        cy = y + 18
        if key == CURRENT:
            s.poly([(x0 + 4, cy - 5), (x0 + 12, cy), (x0 + 4, cy + 5)], fill=C['cyan'])
        star(s, x0 + 12 + 18, cy, 9, fill=C['amber'] if key in FAV else None,
             stroke=None if key in FAV else C['text_dim'], sw=1.5)
        ax = x0 + 12 + 36
        s.text(ax, cy, '%s %s' % (row['kind'], row['addr']), 'bs', 15, C['text'] if sel else C['text_dim'], trk=0.5)
        nx = ax + 112
        s.text(nx, cy, row['name'], 'ar', 15, C['text'])
        if row['factory'] != row['name']:
            px = x0 + 12 + 36 + 112 + namew
            pw = adv('EDITED', 'bs', 11, 1.5) - 1.5 + 12
            s.rect(px, cy - 10, pw, 20, stroke=C['amber'], sw=1, rx=4)
            s.text(px + 6, cy, 'EDITED', 'bs', 11, C['amber'], trk=1.5)
        s.text(cat_x, cy, row['cat'], 'ar', 13, C['text_dim'])
        u = USED.get(key)
        if u:
            s.text(cat_x + 140 + 44, cy, 'x%d' % u, 'ar', 13, C['text_dim'], anchor='end')
        y += 36
    # scrollbar handle: 16 of N rows
    track = lh - 28
    hh = track * 16.0 / len(rows)
    s.rect(rx0 + rw - 1 - 12, my0 + 28, 12, hh, fill=C['border'], rx=6)
    s.clip_end()
    s.rect(rx0, my0, rw, lh, stroke=C['border'], sw=1, rx=8)

    # transport bar ------------------------------------------------------
    tby = my0 + lh + 12
    by = tby + 8
    s.rect(rx0, by, 88, 40, fill=C['panel_raised'], stroke=C['border'], sw=1, rx=6)
    chevron(s, rx0 + 22, by + 20, True, C['text'])
    s.text(rx0 + 38, by + 20, 'Prev', 'ab', 15, C['text'])
    nx = rx0 + 96
    s.rect(nx, by, 88, 40, fill=C['panel_raised'], stroke=C['border'], sw=1, rx=6)
    s.text(nx + 18, by + 20, 'Next', 'ab', 15, C['text'])
    chevron(s, nx + 88 - 22, by + 20, False, C['text'])
    px = nx + 96
    s.rect(px, by, 120, 40, fill=C['cyan'], rx=6)
    s.text(px + 60, by + 20, 'Play', 'ab', 15, C['on_cyan'], anchor='middle')
    sx = rx0 + rw - 88
    s.rect(sx, by, 88, 40, fill=C['panel_raised'], stroke=C['border'], sw=1, rx=6)
    star(s, sx + 24, by + 20, 8, fill=C['amber'])
    s.text(sx + 40, by + 20, 'Star', 'ab', 15, C['text'])

    # set editor ---------------------------------------------------------
    sey = tby + 56 + 12
    s.rect(rx0, sey, rw, 200, fill=C['panel'], rx=8)
    ix, iy, iw = rx0 + 12, sey + 12, rw - 24
    btn_w = [84, 84, 84, 84]
    pick_w = iw - sum(btn_w) - 76 - 5 * 8
    s.rect(ix, iy, pick_w, 36, fill=C['field'], stroke=C['border'], sw=1, rx=6)
    s.text(ix + 12, iy + 18, 'Friday Set 2', 'ar', 15, C['text'])
    chevron_down(s, ix + pick_w - 18, iy + 18)
    x = ix + pick_w + 8
    for lab in ('New', 'Rename', 'Duplicate', 'Delete'):
        s.rect(x, iy, 84, 36, fill=C['panel_raised'], stroke=C['border'], sw=1, rx=6)
        s.text(x + 42, iy + 18, lab, 'ab', 15, C['red'] if lab == 'Delete' else C['text'], anchor='middle')
        x += 92
    # size toggle 6 | 8 (8 on)
    s.rect(x, iy, 36, 36, fill=C['panel_raised'], stroke=C['border'], sw=1, rx=6)
    s.text(x + 18, iy + 18, '6', 'ab', 15, C['text_dim'], anchor='middle')
    s.rect(x + 40, iy, 36, 36, fill=C['sel_row'], stroke=C['cyan'], sw=2, rx=6)
    s.text(x + 58, iy + 18, '8', 'ab', 15, C['text'], anchor='middle')

    gy = iy + 36 + 8
    cw = (iw - 3 * 8) / 4.0
    from live import SET8
    for i, sl in enumerate(SET8):
        c, rr = i % 4, i // 4
        cx_ = ix + c * (cw + 8)
        cy_ = gy + rr * (62 + 8)
        if not sl:
            s.rect(cx_, cy_, cw, 62, stroke=C['border'], sw=1, rx=6, dash=(4, 3))
            s.rect(cx_ + 8, cy_ + 19, 24, 24, stroke=C['border'], sw=1, rx=4, dash=(3, 2))
            s.text(cx_ + 20, cy_ + 31, str(i + 1), 'bs', 15, C['disabled_text'], anchor='middle', trk=0.5)
            s.text(cx_ + 40, cy_ + 31, 'EMPTY', 'bs', 12, C['disabled_text'], trk=1.5)
            continue
        selc = (i == 2)
        s.rect(cx_, cy_, cw, 62, fill=C['panel_raised'], rx=6)
        if selc:
            s.rect(cx_, cy_, cw, 62, stroke=C['cyan'], sw=2, rx=6)
        else:
            s.rect(cx_, cy_, cw, 62, stroke=C['border'], sw=1, rx=6)
        s.rect(cx_ + 8, cy_ + 19, 24, 24, stroke=C['text_dim'], sw=1, rx=4)
        s.text(cx_ + 20, cy_ + 31, str(i + 1), 'bs', 15, C['text'], anchor='middle', trk=0.5)
        l1 = sl['label'] or sl['name']
        addr = '%s %s' % (sl['kind'], sl['addr'])
        l2 = addr + ' - ' + sl['name'] if sl['label'] else addr
        s.text(cx_ + 40, cy_ + 21, l1, 'ab', 15, C['text'])
        s.text(cx_ + 40, cy_ + 42, l2, 'ar', 13, C['text_dim'])
        if (sl['kind'], sl['addr']) == CURRENT:
            s.poly([(cx_ + cw - 6 - 6, cy_ + 6), (cx_ + cw - 6, cy_ + 10), (cx_ + cw - 12, cy_ + 14)], fill=C['cyan'])

    # status line --------------------------------------------------------
    s.text(M, H - M - 12, 'Sent PROG D104 Classic E.Piano', 'ar', 13, C['text_dim'])
    s.save(outdir + '/browse.svg')
    return log


def chevron_down(s, cx, cy):
    s.poly([(cx - 5, cy - 2.5), (cx, cy + 2.5), (cx + 5, cy - 2.5)], stroke=C['text_dim'], sw=2,
           close=False, round_=True)
