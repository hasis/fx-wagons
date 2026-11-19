#!/usr/bin/env python3
"""Our wagons' drawings for fx train (op1f_wagon.h, version 2), and the Mac preview of the train with them.

  python3 -I tools/pack/wagon_art.py header                    writes tools/pack/wagon_art.h (the C strings)
  python3 -I tools/pack/wagon_art.py preview OUTDIR [TRAIN]    writes train_*.png: TRAIN is Faust's checkout with the
                                                               registry (default ~/dev/open-field-wagons)

The drawings follow the train's own characters (pictures/train/fxtrain.svg.make): a wagon's frame at x 0, the art
between x 4 and 66 standing on y 144, stroke #A2A4A6 at 1.3 (1 for the fine lines), the near-black #000005 as fill,
#CDD0C2 for the small light parts and a knob-colour dot or two. The base adds each four times lit and four times dim
(every colour #5B6670, the near-black fills black); the preview does the same, then places the wagons as
build_h_train.cpp's dress() does: chassis, character, label, lamp, links, siding, the parameter card.
"""
import math, re, subprocess, sys, tempfile
from pathlib import Path

HERE = Path(__file__).resolve().parent
G, BG, LT, DIM = '#A2A4A6', '#000005', '#CDD0C2', '#5B6670'
BLUE, OCHRE, RED = '#3C6087', '#AD8959', '#CC1F00'


def ln(x1, y1, x2, y2, c=G, w=1.3):
    return '<line fill="none" stroke="%s" stroke-width="%g" stroke-linecap="round" x1="%g" y1="%g" x2="%g" y2="%g"/>' % (c, w, x1, y1, x2, y2)


def circ(cx, cy, r, c=G, fill=BG, w=1.3):
    return '<circle fill="%s" stroke="%s" stroke-width="%g" cx="%g" cy="%g" r="%g"/>' % (fill, c, w, cx, cy, r)


def dot(cx, cy, r, c):
    return '<circle fill="%s" cx="%g" cy="%g" r="%g"/>' % (c, cx, cy, r)


def rect(x, y, w, h, rx=2, c=G, fill=BG, sw=1.3):
    return '<rect fill="%s" stroke="%s" stroke-width="%g" x="%g" y="%g" width="%g" height="%g" rx="%g"/>' % (fill, c, sw, x, y, w, h, rx)


def path(d, c=G, w=1.3, fill='none'):
    assert not re.search(r'[Aa]', d), 'no arcs: the unit hangs at the start on one (open-field docs/base.md)'
    return '<path fill="%s" stroke="%s" stroke-width="%g" stroke-linecap="round" stroke-linejoin="round" d="%s"/>' % (fill, c, w, d)


def wave(x0, x1, y, amp, cycles, phase=0.0, n=24):
    pts = ['%s%.1f,%.1f' % ('M' if i == 0 else 'L', x0 + (x1 - x0) * i / n, y + amp * math.sin(phase + 2 * math.pi * cycles * i / n)) for i in range(n + 1)]
    return ''.join(pts)


def tune():      # a tuner: the meter's arc and needle, beside a microphone on its stand
    s = rect(6, 112, 38, 32)
    s += path('M12,136C12,128.82 17.82,123 25,123C32.18,123 38,128.82 38,136')   # the meter's half circle as two curves: the unit's picture reader has no arcs (an arc hangs it at the start)
    for a in (-60, -30, 0, 30, 60):
        t = math.radians(a - 90); s += ln(25 + 13 * math.cos(t), 136 + 13 * math.sin(t), 25 + 10 * math.cos(t), 136 + 10 * math.sin(t), LT, 1)
    s += ln(25, 136, 32, 125, LT, 1.3) + dot(25, 136, 1.6, RED) + dot(25, 123, 0.9, BLUE)
    s += rect(51, 100, 10, 16, 5) + ln(53, 105, 59, 105, DIM, 1) + ln(53, 109, 59, 109, DIM, 1)
    s += path('M48,111C48,121 64,121 64,111') + ln(56, 119, 56, 144) + ln(49, 143.5, 63, 143.5)
    return s


def juno():      # a Juno: the slanted panel with sliders and keys, two voices weaving over it
    s = path('M6,144L6,130L14,122L64,122L64,144Z', fill=BG)
    for i in range(6): s += ln(18 + 7 * i, 125, 18 + 7 * i, 129, DIM, 1) + ln(16.5 + 7 * i, 127 - (i % 3), 19.5 + 7 * i, 127 - (i % 3), LT, 1)
    s += ln(8, 134, 62, 134)
    for i in range(9): s += ln(11 + 6 * i, 134, 11 + 6 * i, 143, G, 1)
    s += path(wave(8, 62, 108, 6, 1.5), LT, 1.3) + path(wave(8, 62, 108, 6, 1.5, math.pi), DIM, 1.3)
    s += dot(8, 108, 1.6, BLUE) + dot(62, 108, 1.6, OCHRE)
    return s


def tape():      # a tape machine: two reels with their spokes, the tape over the head, the deck
    s = rect(6, 130, 58, 14)
    for cx in (20, 50):
        s += circ(cx, 116, 11) + circ(cx, 116, 3.2, LT, BG, 1)
        for a in (90, 210, 330):
            t = math.radians(a); s += ln(cx + 3.2 * math.cos(t), 116 + 3.2 * math.sin(t), cx + 10 * math.cos(t), 116 + 10 * math.sin(t), G, 1)
    s += path('M20,127L30,134L40,134L50,127', LT, 1) + rect(31, 133, 8, 5, 1, G, BG, 1)
    s += dot(12, 139, 1.4, BLUE) + dot(58, 139, 1.4, RED)
    return s


def gate():      # a row of doors, two shut and two swung open: the rhythm
    s = ''
    for i in range(4):
        x = 7 + 15 * i
        s += rect(x, 112, 12, 32, 0.5, G, 'none', 1)
        if i % 2 == 0: s += rect(x + 1.5, 113.5, 9, 29, 0.5, G, BG, 1.3) + dot(x + 8, 129, 1, LT)
        else: s += path('M%g,113L%g,116L%g,140L%g,143Z' % (x + 1.5, x + 6, x + 6, x + 1.5), G, 1.3, BG)
    s += ln(5, 109, 66, 109, LT, 1) + dot(12.5, 105, 1.6, BLUE) + dot(42.5, 105, 1.6, BLUE)
    return s


def freeze():    # a snowflake held over a block of ice
    s = rect(13, 128, 44, 16, 1.5) + ln(17, 132, 23, 132, LT, 1) + ln(17, 135, 20, 135, LT, 1)
    cx, cy, r = 35, 108, 15
    for k in range(6):
        t = math.radians(60 * k - 90); ex, ey = cx + r * math.cos(t), cy + r * math.sin(t)
        s += ln(cx, cy, ex, ey, G, 1.3)
        bx, by = cx + 0.62 * r * math.cos(t), cy + 0.62 * r * math.sin(t)
        for side in (-1, 1):
            u = t + side * math.radians(35); s += ln(bx, by, bx + 5 * math.cos(u), by + 5 * math.sin(u), LT, 1)
    s += dot(cx, cy, 1.8, BLUE)
    return s


ARTS = {'tune': tune(), 'juneaux': juno(), 'tape': tape(), 'gate': gate(), 'freeze': freeze()}


def header():
    out = ['// Made by tools/pack/wagon_art.py: our wagons\' drawings for fx train (op1f_wagon.h, art). Do not edit.', '#pragma once']
    for name, art in ARTS.items():
        assert len(art) <= 4096 and ' id=' not in art, name
        out.append('static const char WAGON_ART_%s[] = "%s";' % (name.upper(), art.replace('"', '\\"')))
    (HERE / 'wagon_art.h').write_text('\n'.join(out) + '\n')
    print('wagon_art.h: ' + ', '.join('%s %d bytes' % (n, len(a)) for n, a in ARTS.items()))


# ---------------------------------------------------------------- the preview: the train as the base dresses it
def dim(art):     # build_m3_art.cpp put_group(dim)
    return re.sub(r'#([0-9A-Fa-f]{6})', lambda m: '#000005' if m.group(1)[0] == '0' and m.group(1)[1] in '0Ee' else '#5B6670', art)


def train_svg(train_dir):
    with tempfile.TemporaryDirectory() as tmp:
        out = Path(tmp) / 'ft.svg'
        subprocess.run([sys.executable, '-I', 'tools/picture_make.py', 'make', '--instructions', 'pictures/train/fxtrain.svg.make',
                        '--display', str(HERE.parents[1] / 'local' / 'content-173-stock' / 'display'), '--out', str(out)],
                       cwd=train_dir, check=True, capture_output=True)
        s = out.read_text()
    order = list(ARTS)                                      # the types 11 and up, in OFFERED's order
    groups = ''.join(''.join('<g id="fxpcar%d_%d">%s</g>' % (i, c, ARTS[n]) for c in range(4)) +
                     ''.join('<g id="fxpdim%d_%d">%s</g>' % (i, c, dim(ARTS[n])) for c in range(4)) for i, n in enumerate(order))
    return s.replace('<g id="fxlamp"', groups + '<g id="fxlamp"', 1), order


FIRST_X, PITCH, LOCO_RIGHT, FLOOR_Y, SIDING_DY = 118, 80, 110, 144, 11
KNOB = ['#3C6087', '#8A653C', '#A2A4A6', '#CC1F00']
KNOB_DARK = ['#2B4261', '#5E554B', '#5B6670', '#BF0020']
HIDE = ('fxsiding', 'fxsag', 'fxlink', 'fxchassis', 'fxcar_', 'fxdim_', 'fxlabel_', 'fxlamp', 'ufo', 'fxhint', 'fxcard', 'fxneedlered',
        'fxfull', 'fxplug', 'fxpcar', 'fxpdim', 'fxpuff', 'fxbeam')


def scene(svg, order, wagons, lamp, card=None, cost=40):
    """wagons: per position (knob, type name or None, coupled); card: the knob whose card shows (a pick)."""
    show, move, extra, used = set(), {}, [], {}
    prev_r, prev_y, links = LOCO_RIGHT + 1.5, FLOOR_Y + 2.5, 0
    for p, (k, t, on) in enumerate(wagons):
        x, y = FIRST_X + PITCH * p, 0 if on or t is None else SIDING_DY
        off = t is not None and not on
        ch = 'fxchassisdim%d' % (k + 1) if off else 'fxchassis%d' % (k + 1)
        show.add(ch); move[ch] = (x, y)
        if off: show.add('fxsiding%d' % (p + 1))
        c = used.get(t, 0); used[t] = c + 1
        if t is None:
            cid = 'fxcar_none' + ('' if c == 0 else '_%d' % (c + 1)); lab = 'fxlabel_none_dim'
        elif t in order:
            cid = 'fxp%s%d_%d' % ('dim' if off else 'car', order.index(t), c); lab = None
            extra.append('<text x="%g" y="%g" fill="%s" font-family="Helvetica" font-size="14" letter-spacing="0.8">%s</text>'
                         % (x + 10, 90, (KNOB_DARK if off else KNOB)[k], t.upper()))
        else:
            cid = 'fx%s_%s' % ('dim' if off else 'car', t) + ('' if c == 0 else '_%d' % (c + 1)); lab = 'fxlabel_%s_%s%d' % (t, 'd' if off else '', k + 1)
        show.add(cid); move[cid] = (x, y)
        if lab: show.add(lab); move[lab] = (x, 0)   # labels move with x and the hop only (dress)
        if k == lamp: show.add('fxlamp'); move['fxlamp'] = (x, y)
        if not off:
            ty = FLOOR_Y + 2.5 + y
            lid = 'fxlink%d' % (links + 1); links += 1; show.add(lid)
            move[lid] = 'matrix(%g %g 0 1 %g %g)' % ((x - 1.5) - prev_r, ty - prev_y, prev_r, prev_y)
            prev_r, prev_y = x + 70 + 1.5, ty
    if card is not None:
        show |= {'fxcard', 'fxcardbg%d' % (card + 1), 'fxcardknob%d' % (card + 1), 'fxcardlabel'}

    def fix(m):
        tag, gid = m.group(1), m.group(2)
        if gid in show or not gid.startswith(HIDE):
            if gid in move:
                v = move[gid]
                return '<%s id="%s" transform="%s"' % (tag, gid, v if isinstance(v, str) else 'translate(%g %g)' % v)
            return m.group(0)
        return '<%s id="%s" display="none"' % (tag, gid)
    s = re.sub(r'<(\w+) id="([^"]+)"', fix, svg)
    deg = 180 - cost * 1.8
    s = s.replace('<g id="fxneedle"', '<g id="fxneedle" transform="translate(70 107) rotate(%g)"' % -deg, 1)
    s = s.replace('</svg>', ''.join(extra) + '</svg>')
    return re.sub(r'fill="\{[^}]*\}"', 'fill="#000005"', s)   # the picture's own fill words (a dict printed by Faust's tool) read as black here


def preview(out, train_dir):
    out = Path(out); out.mkdir(parents=True, exist_ok=True)
    svg, order = train_svg(train_dir)
    scenes = {   # (knob, type, coupled) by position; the lamp's knob
        'tune_normal': ([(0, 'delay', 1), (1, 'tune', 1), (2, 'phone', 1), (3, None, 1)], 0, None),
        'tune_picked': ([(0, 'delay', 1), (1, 'tune', 1), (2, 'phone', 1), (3, None, 1)], 0, 1),
        'tune_parked': ([(0, 'delay', 1), (1, 'tune', 0), (2, 'phone', 1), (3, None, 1)], 0, None),
        'ours_all':    ([(0, 'juneaux', 1), (1, 'tape', 1), (2, 'gate', 1), (3, 'freeze', 1)], -1, None),
        'ours_parked': ([(0, 'nitro', 1), (1, 'juneaux', 0), (2, 'tape', 1), (3, 'freeze', 0)], 0, None),
    }
    with tempfile.TemporaryDirectory() as tmp:
        for name, (wagons, lamp, card) in scenes.items():
            f = Path(tmp) / ('train_%s.svg' % name)
            body = scene(svg, order, wagons, lamp, card)
            body = body[body.index('<svg'):]
            inner = body[body.index('>') + 1:body.rindex('</svg>')]
            f.write_text('<svg xmlns="http://www.w3.org/2000/svg" xmlns:xlink="http://www.w3.org/1999/xlink" width="480" height="480" viewBox="0 0 480 480">'
                         '<rect width="480" height="480" fill="#000005"/><g transform="translate(0 130)">' + inner + '</g></svg>')   # square and centred: sips crops about the middle
            subprocess.run(['qlmanage', '-t', '-s', '960', '-o', tmp, str(f)], check=True, capture_output=True)
            png = out / ('train_%s.png' % name)
            Path(tmp, f.name + '.png').replace(png)
            subprocess.run(['sips', '-c', '440', '960', str(png)], check=True, capture_output=True)
            print(png)


if __name__ == '__main__':
    if sys.argv[1:2] == ['header']: header()
    elif sys.argv[1:2] == ['preview']: preview(sys.argv[2], sys.argv[3] if len(sys.argv) > 3 else str(Path.home() / 'dev' / 'open-field-wagons'))
    else: sys.exit(__doc__)
