#!/usr/bin/env python3
"""fx.pack's page: one picture for its four wagons, and the Mac preview.

  python3 -I tools/pack/fx/face.py make            writes fxwagon.svg.make and fx_face.h
  python3 -I tools/pack/fx/face.py preview [dir]   writes page_*.png (default tools/pack/fx/build)

The page a tap on a JUNO, TAPE, GATE or FREEZE wagon opens: the effect's name, and for each of the four knobs, in
encoder order, the knob's colour, its label, its value in words and a bar of segments. The texts are the firmware's
letters through op1f_text (placeholders here); the pack shows and hides the segments. Line art in the train's colours.
"""
import math, subprocess, sys, tempfile
from pathlib import Path

HERE = Path(__file__).resolve().parent
KNOB = ['#3C6087', '#8A653C', '#A2A4A6', '#CC1F00']   # the train's knob colours
G, LT, DIM = '#A2A4A6', '#CDD0C2', '#5B6670'
ROW_Y0, ROW_DY, DOT_X, LABEL_X, VALUE_X, BAR_X, BAR_W, SEGMENTS = 58, 40, 30, 46, 170, 270, 180, 16


def holder(gid, x, y, colour):   # a text's place: its top-left and its colour (as autotune's page)
    return '<g id="%s"><path fill="none" stroke="%s" stroke-width="1.3" stroke-linecap="square" d="M%d,%dV%d"/></g>' % (gid, colour, x, y + 15, y)


def body():
    b = [holder('fx_title', 24, 10, LT), '<line fill="none" stroke="%s" stroke-width="1.3" x1="24" y1="34" x2="456" y2="34"/>' % DIM]
    sw = BAR_W / SEGMENTS
    for k in range(4):
        y = ROW_Y0 + ROW_DY * k
        b.append('<circle fill="%s" cx="%d" cy="%d" r="5"/>' % (KNOB[k], DOT_X, y + 8))
        b.append(holder('fx_lab%d' % k, LABEL_X, y, G))
        b.append(holder('fx_val%d' % k, VALUE_X, y, LT))
        b.append('<rect fill="none" stroke="%s" stroke-width="1" x="%g" y="%d" width="%g" height="12" rx="1"/>' % (DIM, BAR_X - 2, y + 2, BAR_W + 2))
        for s in range(SEGMENTS):
            b.append('<g id="fx_b%ds%d"><rect fill="%s" x="%g" y="%d" width="%g" height="8"/></g>' % (k, s, KNOB[k], BAR_X + s * sw, y + 4, sw - 2))
    return '\n'.join(b)


# ---- JUNEAUX's own faceplate (juneaux.svg): the chorus panel on the left, its two buttons I and II with a light over
# each (1st knob, MODE), and the two voices of the chorus weaving over it, wider with DEPTH and moving at the speed of
# the mode; MIX, DEPTH and TONE as rows on the right. The pack shows one wave group (depth step, phase frame) at a time.
BG = '#000005'
JX_DEPTHS, JX_FRAMES = 5, 8
JX_ROW_Y0, JX_ROW_DY, JX_DOT_X, JX_LABEL_X, JX_VALUE_X, JX_BAR_X, JX_BAR_W = 58, 50, 254, 268, 380, 254, 196


def jx_wave(depth, frame, voice):
    amp = 2 + depth * 5.5
    ph = 2 * math.pi * frame / JX_FRAMES + (math.pi if voice else 0)
    n = 30
    pts = ['%s%.1f,%.1f' % ('M' if i == 0 else 'L', 40 + 172 * i / n, 108 + amp * math.sin(ph + 2 * math.pi * 1.5 * i / n)) for i in range(n + 1)]
    return ''.join(pts)


def juneaux_body():
    b = [holder('fx_title', 24, 10, LT), '<line fill="none" stroke="%s" stroke-width="1.3" x1="24" y1="34" x2="456" y2="34"/>' % DIM]
    b.append('<path fill="%s" stroke="%s" stroke-width="1.3" stroke-linejoin="round" d="M24,200L24,64L40,48L228,48L228,200Z"/>' % (BG, G))
    for i in range(7):                                       # the panel's slider slots, under the voices
        b.append('<line fill="none" stroke="%s" stroke-width="1" x1="%d" y1="186" x2="%d" y2="194"/>' % (DIM, 44 + 28 * i, 44 + 28 * i))
    for d in range(JX_DEPTHS):
        for f in range(JX_FRAMES):
            b.append('<g id="jx_w%d_%d"><path fill="none" stroke="%s" stroke-width="1.3" stroke-linecap="round" stroke-linejoin="round" d="%s"/>'
                     '<path fill="none" stroke="%s" stroke-width="1.3" stroke-linecap="round" stroke-linejoin="round" d="%s"/></g>'
                     % (d, f, KNOB[0], jx_wave(d, f, 0), KNOB[1], jx_wave(d, f, 1)))
    for k, cx in enumerate((86, 166)):                       # the buttons I and II, a light over each
        b.append('<circle fill="%s" stroke="%s" stroke-width="1" cx="%d" cy="144" r="3.5"/>' % (BG, DIM, cx))
        b.append('<g id="jx_led%d"><circle fill="%s" cx="%d" cy="144" r="3.5"/></g>' % (k + 1, KNOB[0], cx))
        b.append('<rect fill="%s" stroke="%s" stroke-width="1.3" x="%d" y="152" width="52" height="26" rx="2"/>' % (BG, G, cx - 26))
        for j in range(k + 1):
            x = cx + (j * 8 - 4 if k else 0)
            b.append('<line fill="none" stroke="%s" stroke-width="1.3" stroke-linecap="round" x1="%d" y1="159" x2="%d" y2="171"/>' % (LT, x, x))
    sw = JX_BAR_W / SEGMENTS
    for r, k in enumerate((1, 2, 3)):                        # MIX, DEPTH, TONE
        y = JX_ROW_Y0 + JX_ROW_DY * r
        b.append('<circle fill="%s" cx="%d" cy="%d" r="5"/>' % (KNOB[k], JX_DOT_X, y + 8))
        b.append(holder('fx_lab%d' % k, JX_LABEL_X, y, G))
        b.append(holder('fx_val%d' % k, JX_VALUE_X, y, LT))
        b.append('<rect fill="none" stroke="%s" stroke-width="1" x="%g" y="%d" width="%g" height="12" rx="1"/>' % (DIM, JX_BAR_X - 2, y + 22, JX_BAR_W + 2))
        for s in range(SEGMENTS):
            b.append('<g id="fx_b%ds%d"><rect fill="%s" x="%g" y="%d" width="%g" height="8"/></g>' % (k, s, KNOB[k], JX_BAR_X + s * sw, y + 24, sw - 2))
    return '\n'.join(b)


def wrap(name, b):
    n = len(b)
    (HERE / name).write_bytes(b'op1f picture 2\nsize %d\nfrom tempo.svg 43996 eb050950\nc 0 0 371\na %d\n%s\nc 0 43989 7\n' % (371 + n + 7, n, b))
    print('%s: %d bytes of ours' % (name, n))


def make():
    wrap('juneaux.svg.make', juneaux_body().encode())
    b = body().encode()
    n = len(b)
    (HERE / 'fxwagon.svg.make').write_bytes(b'op1f picture 2\nsize %d\nfrom tempo.svg 43996 eb050950\nc 0 0 371\na %d\n%s\nc 0 43989 7\n' % (371 + n + 7, n, b))
    (HERE / 'fx_face.h').write_text('// Made by face.py: do not edit.\n#pragma once\nenum { FX_SEGMENTS = %d, JX_DEPTHS = %d, JX_FRAMES = %d };\n' % (SEGMENTS, JX_DEPTHS, JX_FRAMES))
    print('fxwagon.svg.make: %d bytes of ours' % n)


def preview_svg(title, rows, b=None, hide=()):
    """rows: (label, value text, 0..32767) per knob, as the pack's frame sets them."""
    b = b or body()
    for gid in hide: b = b.replace('<g id="%s">' % gid, '<g id="%s" display="none">' % gid)
    for k, (label, value, v) in enumerate(rows):
        lit = (v * SEGMENTS + 16383) // 32767
        for s in range(lit, SEGMENTS):
            b = b.replace('<g id="fx_b%ds%d">' % (k, s), '<g id="fx_b%ds%d" display="none">' % (k, s))
    texts = {'fx_title': (title, LT)}
    for k, (label, value, v) in enumerate(rows):
        if '"fx_lab%d"' % k in b: texts['fx_lab%d' % k] = (label, G); texts['fx_val%d' % k] = (value, LT)
    for gid, (s, c) in texts.items():   # the firmware's letters, drawn here in a plain font (shapes differ)
        i = b.index('<g id="%s">' % gid); d = b.index(' d="M', i); x, y = b[d + 5:b.index('V', d)].split(',')
        b = b[:i] + '<g id="%s">' % gid + b[b.index('</g>', i):]   # the placeholder becomes the label on the unit
        b += '<text x="%s" y="%d" fill="%s" font-family="Helvetica" font-size="15" letter-spacing="0.8">%s</text>' % (x, int(y) - 2, c, s.upper())
    return ('<svg xmlns="http://www.w3.org/2000/svg" width="480" height="480" viewBox="0 0 480 480"><rect width="480" height="480" fill="#000005"/>'
            '<g transform="translate(0 130)"><rect width="480" height="220" fill="#000005"/>' + b + '</g></svg>')


STATES = {
    'juneaux': ('juneaux', [('mode', 'i', 0), ('mix', '100%', 32767), ('depth', '50%', 16384), ('tone', '50%', 16384)]),
    'tape':   ('tape', [('wow', '20%', 6500), ('flutter', '12%', 4000), ('saturate', '15%', 5000), ('age', '27%', 9000)]),
    'gate':   ('gate', [('depth', '79%', 26000), ('pattern', '1/16', 9362), ('length', '50%', 16384), ('smooth', '25%', 8192)]),
    'freeze': ('freeze', [('hold', '75%', 24576), ('pitch', '+5', 20479), ('follow', 'on', 32767), ('smear', '37%', 12000)]),
}


def preview(out):
    out = Path(out); out.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory() as tmp:
        for name, (title, rows) in STATES.items():
            f = Path(tmp) / ('page_%s.svg' % name)
            if name == 'juneaux':                       # its own faceplate: depth step 2, frame 1, mode I (light I on)
                hide = ['jx_w%d_%d' % (d, fr) for d in range(JX_DEPTHS) for fr in range(JX_FRAMES) if (d, fr) != (2, 1)] + ['jx_led2']
                f.write_text(preview_svg(title, rows, juneaux_body(), hide))
            else:
                f.write_text(preview_svg(title, rows))
            subprocess.run(['qlmanage', '-t', '-s', '960', '-o', tmp, str(f)], check=True, capture_output=True)
            png = out / ('page_%s.png' % name)
            Path(tmp, f.name + '.png').replace(png)
            subprocess.run(['sips', '-c', '440', '960', str(png)], check=True, capture_output=True)
            print(png)


if __name__ == '__main__':
    if sys.argv[1:2] == ['make']: make()
    elif sys.argv[1:2] == ['preview']: preview(sys.argv[2] if len(sys.argv) > 2 else HERE / 'build')
    else: sys.exit(__doc__)
