#!/usr/bin/env python3
"""The fx wagons' faceplates (each wagon pack's page), and the Mac preview.

  python3 -I tools/pack/fx/face.py make            writes juneaux/tape/gate/freeze.svg.make and fx_face.h
  python3 -I tools/pack/fx/face.py preview [dir]   writes page_*.png (default tools/pack/fx/build), a few animation states

The page a tap on a JUNEAUX, TAPE, GATE or FREEZE wagon opens: the effect's name, a drawing of the effect that moves
(prebuilt frame groups, one shown at a time by fx_pack.cpp) with each knob's part in that knob's colour, and the knobs
in encoder order as rows: colour, label, value in words, a bar of segments. The texts are the firmware's letters through
op1f_text (placeholders here). Line art in the train's colours; path commands M L H V C only, no display attribute.
"""
import os, math, subprocess, sys, tempfile
from pathlib import Path

HERE = Path(__file__).resolve().parent
PREVIEW_SCALE = int(os.environ.get('PREVIEW_SCALE', '2'))   # the PNGs: 480 x 220 points of the unit's screen, this many pixels a point
KNOB = ['#3C6087', '#8A653C', '#A2A4A6', '#CC1F00']   # the train's knob colours
G, LT, DIM = '#A2A4A6', '#CDD0C2', '#5B6670'
ROW_Y0, ROW_DY, DOT_X, LABEL_X, VALUE_X, BAR_X, BAR_W, SEGMENTS = 58, 40, 30, 46, 170, 270, 180, 16


def holder(gid, x, y, colour):   # a text's place: its top-left and its colour (as autotune's page)
    return '<g id="%s"><path fill="none" stroke="%s" stroke-width="1.3" stroke-linecap="square" d="M%d,%dV%d"/></g>' % (gid, colour, x, y + 15, y)


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


# ---- the faceplates of TAPE, GATE and FREEZE: the header, a drawing on the left (each knob's part of it in that knob's
# colour) and the four knobs as compact rows on the right (label, value, bar). Animation as JUNEAUX's: the pack shows one
# of N prebuilt groups a display frame (fx_pack.cpp's dress_tape, dress_gate, dress_freeze).
RW_Y0, RW_DY, RW_DOT_X, RW_LABEL_X, RW_VALUE_X, RW_BAR_X, RW_BAR_W = 46, 42, 254, 268, 410, 254, 196   # value at 410: the firmware letters of SATURATE reach ~395
PANEL = '<path fill="%s" stroke="%s" stroke-width="1.3" stroke-linejoin="round" d="M24,200L24,64L40,48L228,48L228,200Z"/>' % (BG, G)
TP_SPOKES, TP_WOW, TP_FLUT, TP_SPECKS = 8, 8, 4, 12   # reel frames, wow frames, flutter frames, AGE's specks
GT_HEADS, GT_SLOTS, GT_DEPTHS = 64, 28, 3            # playhead places a bar, door places (16 steps + 12 triplets), swing steps
FZ_SIZES, FZ_TURNS, FZ_SMEAR, FZ_PITCHES = 4, 8, 4, 25


def f1(v): return ('%.1f' % v).rstrip('0').rstrip('.')
def pt(x, y): return '%s,%s' % (f1(x), f1(y))
def stroke(c, w=1.3, cap='round'): return 'fill="none" stroke="%s" stroke-width="%s" stroke-linecap="%s" stroke-linejoin="round"' % (c, w, cap)
def grp(gid, inner): return '<g id="%s">%s</g>' % (gid, inner)
def pth(d, c, w=1.3): return '<path %s d="%s"/>' % (stroke(c, w), d)


def header():
    return [holder('fx_title', 24, 10, LT), '<line fill="none" stroke="%s" stroke-width="1.3" x1="24" y1="34" x2="456" y2="34"/>' % DIM]


def rows(ks=(0, 1, 2, 3)):
    b, sw = [], RW_BAR_W / SEGMENTS
    for r, k in enumerate(ks):
        y = RW_Y0 + RW_DY * r
        b.append('<circle fill="%s" cx="%d" cy="%d" r="5"/>' % (KNOB[k], RW_DOT_X, y + 8))
        b.append(holder('fx_lab%d' % k, RW_LABEL_X, y, G))
        b.append(holder('fx_val%d' % k, RW_VALUE_X, y, LT))
        b.append('<rect fill="none" stroke="%s" stroke-width="1" x="%g" y="%d" width="%g" height="12"/>' % (DIM, RW_BAR_X - 2, y + 20, RW_BAR_W + 2))
        for s in range(SEGMENTS):
            b.append('<g id="fx_b%ds%d"><rect fill="%s" x="%g" y="%d" width="%g" height="8"/></g>' % (k, s, KNOB[k], RW_BAR_X + s * sw, y + 22, sw - 2))
    return b


# TAPE: two reels turning (WOW sways their speed and the tape's side spans, blue), the tape under the head trembling
# with FLUTTER (ochre), the head glowing with SATURATE (grey), specks of wear on the tape with AGE (red).
TP_L, TP_R, TP_Y, TP_GL, TP_GR, TP_GY, TP_BY = (78, 96), (174, 96), 96, (50, 168), (202, 168), 168, 173


def tape_body():
    b = header() + [PANEL]
    for (cx, cy), pack in ((TP_L, 25), (TP_R, 17)):
        b.append('<circle %s cx="%d" cy="%d" r="31"/>' % (stroke(G), cx, cy))            # the reel's flange
        b.append('<circle %s cx="%d" cy="%d" r="%d"/>' % (stroke(DIM, 1), cx, cy, pack))  # the tape wound on it
        b.append('<circle %s cx="%d" cy="%d" r="5"/>' % (stroke(LT), cx, cy))            # the hub
    for f in range(TP_SPOKES):                       # the spokes, both reels, clockwise 15 degrees a frame (3 spokes: 120)
        d = ''
        for cx, cy in (TP_L, TP_R):
            for s in range(3):
                t = math.radians(f * 120 / TP_SPOKES + s * 120 - 90)
                d += 'M%sL%s' % (pt(cx + 7 * math.cos(t), cy + 7 * math.sin(t)), pt(cx + 28 * math.cos(t), cy + 28 * math.sin(t)))
        b.append(grp('tp_r%d' % f, pth(d, LT)))
    rng = __import__('random').Random(7)
    for i in range(TP_SPECKS):                       # AGE: specks of wear on the wound tape and the tape running
        if i < 8:
            cx, cy = (TP_L, TP_R)[i & 1]; rr = rng.uniform(9, (25, 17)[i & 1] - 2); t = rng.uniform(0, 2 * math.pi)
            x, y = cx + rr * math.cos(t), cy + rr * math.sin(t)
        else: x, y = (62, 92, 160, 190)[i - 8], TP_BY
        b.append(grp('tp_a%d' % i, '<line %s x1="%s" y1="%s" x2="%s" y2="%s"/>' % (stroke(KNOB[3], 2.4), f1(x - 1.2), f1(y), f1(x + 1.2), f1(y))))
    for w in range(4):                               # WOW: the side spans sway (4 depths x 8 frames)
        for f in range(TP_WOW):
            bow = (0, 3, 6, 9)[w] * math.sin(2 * math.pi * f / TP_WOW)
            d = ''
            for (x0, y0), (x1, y1), sg in (((TP_L[0] - 25, TP_Y), (TP_GL[0] - 5, TP_GY), 1), ((TP_R[0] + 17, TP_Y), (TP_GR[0] + 5, TP_GY), -1)):
                dx = sg * bow
                d += 'M%sC%s %s %s' % (pt(x0, y0), pt(x0 + (x1 - x0) / 3 + dx, y0 + (y1 - y0) / 3), pt(x0 + 2 * (x1 - x0) / 3 + dx, y0 + 2 * (y1 - y0) / 3), pt(x1, y1))
            b.append(grp('tp_w%d_%d' % (w, f), pth(d, KNOB[0])))
    for a in range(4):                               # FLUTTER: the tape across the head trembles (4 depths x 4 frames)
        for f in range(TP_FLUT):
            r2 = __import__('random').Random(100 + f)
            d = 'M%s' % pt(TP_GL[0], TP_BY)
            for x in range(TP_GL[0] + 4, TP_GR[0], 4):
                y = TP_BY if a == 0 or 118 <= x <= 134 else TP_BY + r2.choice((-1, 1)) * (0.6, 1.3, 2.2, 3.2)[a] * r2.uniform(0.5, 1)
                d += 'L%s' % pt(x, y)
            d += 'L%s' % pt(TP_GR[0], TP_BY)
            b.append(grp('tp_f%d_%d' % (a, f), pth(d, KNOB[1])))
    for x, y in (TP_GL, TP_GR):                      # the guides the tape wraps round
        b.append('<circle %s cx="%d" cy="%d" r="5"/>' % (stroke(G), x, y))
    for h in range(4):                               # SATURATE: the head, warmer and glowing (4 steps)
        s = '<rect fill="%s" stroke="%s" stroke-width="%s" x="116" y="175" width="20" height="12"/>' % (BG, LT, (1.3, 1.6, 2, 2.4)[h])
        for k in range(h):
            o = 3 + 2.5 * k
            s += '<polyline %s points="%s %s %s %s"/>' % (stroke(KNOB[2], 1), pt(116 - o, 178), pt(116 - o, 187 + o), pt(136 + o, 187 + o), pt(136 + o, 178))
        b.append(grp('tp_h%d' % h, s))
    return '\n'.join(b + rows())


# GATE: the bar as a lane of openings (PATTERN's hits, each open for LENGTH of the gap to the next, its edges by SMOOTH,
# square or rounded; ochre), the level it shuts to (DEPTH, blue), a row of doors (one at each hit) that swing open as the playhead passes.
GT_X0, GT_W, GT_TOP, GT_BOT, GT_DT, GT_DB = 36, 180, 58, 102, 122, 166
GT_MASK = [0x1111, 0x5555, 0xFFFF, 0, 0x4444, 0x0041, 0x0449, 0xADAD]   # fx_special.cpp kGateMask; 1/8t (3): 12 a bar


def gt_slots(pat):   # door places of a pattern: 0..15 the 16ths, 16..27 the triplet 8ths
    return [16 + i for i in range(12)] if pat == 3 else [s for s in range(16) if GT_MASK[pat] >> s & 1]


def gt_slot_x(slot): return (GT_X0 + slot * GT_W / 16, GT_W / 16) if slot < 16 else (GT_X0 + (slot - 16) * GT_W / 12, GT_W / 12)


def gt_lane(pat, lq, sm):
    ln = 3277 + (lq * 32767 // 3) * 29490 // 32768           # fx_gate's length, Q15 of the gap
    if pat == 3: opens = [(i * 15, 15 * ln / 32768) for i in range(12)]
    else:
        hits = gt_slots(pat); st = GT_W / 16
        opens = []
        for j, h in enumerate(hits):
            span = (hits[(j + 1) % len(hits)] - h) % 16 or 16
            opens.append((h * st, span * st * ln / 32768))
    iv = []
    for x, w in opens:                                      # an opening past the bar's end goes on at its start
        if x + w > GT_W: iv += [(x, GT_W - x), (0, x + w - GT_W)]
        else: iv.append((x, w))
    iv.sort()
    e = (0, 4)[sm]
    d = 'M%s' % pt(GT_X0, GT_BOT)
    for x, w in iv:
        x0, x1 = GT_X0 + x, GT_X0 + x + w; ee = min(e, w / 2.5)
        if sm and ee: d += 'L%sC%s %s %sL%sC%s %s %s' % (pt(x0, GT_BOT), pt(x0 + ee, GT_BOT), pt(x0, GT_TOP), pt(x0 + ee, GT_TOP), pt(x1 - ee, GT_TOP),
                                                         pt(x1, GT_TOP), pt(x1 - ee, GT_BOT), pt(x1, GT_BOT))
        else: d += 'H%sV%dH%sV%d' % (f1(x0), GT_TOP, f1(x1), GT_BOT)
    return d + 'H%d' % (GT_X0 + GT_W)


def gate_body():
    b = header() + [PANEL]
    b.append('<line %s x1="%d" y1="%d" x2="%d" y2="%d"/>' % (stroke(DIM, 1), GT_X0, GT_BOT, GT_X0 + GT_W, GT_BOT))
    for q in range(5):                                       # the beats under the lane and under the doors
        x = GT_X0 + q * GT_W / 4
        b.append('<line %s x1="%s" y1="176" x2="%s" y2="%d"/>' % (stroke(G if q in (0, 4) else DIM, 1), f1(x), f1(x), 186 if q in (0, 4) else 182))
    for dq in range(4):                                      # DEPTH: the level the gate shuts to (dashes)
        y = GT_TOP + (GT_BOT - GT_TOP) * dq / 3
        d = ''.join('M%sH%s' % (pt(GT_X0 + x, y), f1(GT_X0 + x + 5)) for x in range(0, GT_W, 9))
        b.append(grp('gt_f%d' % dq, pth(d, KNOB[0])))
    for p in range(8):                                       # PATTERN x LENGTH x SMOOTH: the openings of the bar
        for lq in range(4):
            for sm in range(2):
                b.append(grp('gt_l%d_%d' % (p * 4 + lq, sm), pth(gt_lane(p, lq, sm), KNOB[1])))
    for p in range(8):                                       # the doorways of a pattern
        s = ''
        for slot in gt_slots(p):
            x, w = gt_slot_x(slot)
            s += '<rect fill="none" stroke="%s" stroke-width="1" x="%s" y="%d" width="%s" height="%d"/>' % (DIM, f1(x + 1), GT_DT, f1(w - 2), GT_DB - GT_DT)
        b.append(grp('gt_d%d' % p, s))
    for slot in range(GT_SLOTS):                             # each door shut, and swung open by DEPTH's steps
        x, w = gt_slot_x(slot); x0, x1 = x + 2.2, x + w - 2.2
        b.append(grp('gt_c%d' % slot, '<rect fill="%s" stroke="%s" stroke-width="1.3" x="%s" y="%d" width="%s" height="%d"/>'
                     % (BG, LT, f1(x0), GT_DT + 1.5, f1(x1 - x0), GT_DB - GT_DT - 3)))
        for dq in range(GT_DEPTHS):
            xe = x0 + (x1 - x0) * (0.7, 0.45, 0.24)[dq]
            b.append(grp('gt_o%d_%d' % (slot, dq), '<path fill="%s" stroke="%s" stroke-width="1.3" stroke-linejoin="round" d="M%sL%sL%sL%sZ"/>'
                         % (BG, KNOB[0], pt(x0, GT_DT + 1.5), pt(xe, GT_DT - 2 - 2 * dq), pt(xe, GT_DB + 2 + 2 * dq), pt(x0, GT_DB - 1.5))))
    for i in range(GT_HEADS):                                # the playhead, 64 places a bar
        x = GT_X0 + GT_W * i / GT_HEADS
        b.append(grp('gt_p%d' % i, '<path fill="none" stroke="%s" stroke-width="1" d="M%s,52V172"/>' % (LT, f1(x))))
    return '\n'.join(b + rows())


# FREEZE: a snowflake over a block of ice. HOLD grows it and sets it turning slowly while it holds (blue branches); off,
# a small dim one. PITCH a marker on a ruler of semitones (ochre); FOLLOW a chain link, closed when on, beside a chord
# (grey); SMEAR streaks drifting from the flake (red).
FZ_C, FZ_RUL, FZ_RT, FZ_RB = (108, 100), 214, 58, 178


def flake(r, turn, arm, branch, wa=1.3, wb=1):
    cx, cy = FZ_C; da = db = ''
    for k in range(6):
        t = math.radians(60 * k - 90 + turn)
        da += 'M%sL%s' % (pt(cx + 0.12 * r * math.cos(t), cy + 0.12 * r * math.sin(t)), pt(cx + r * math.cos(t), cy + r * math.sin(t)))
        for at, ln in ((0.45, 0.3), (0.72, 0.2)):
            bx, by = cx + at * r * math.cos(t), cy + at * r * math.sin(t)
            for side in (-1, 1):
                u = t + side * math.radians(40); db += 'M%sL%s' % (pt(bx, by), pt(bx + ln * r * math.cos(u), by + ln * r * math.sin(u)))
    return pth(da, arm, wa) + pth(db, branch, wb)


def freeze_body():
    b = header() + [PANEL]
    b.append('<rect %s x="38" y="156" width="160" height="36"/>' % stroke(G))          # the block of ice
    b.append('<path %s d="M178,162L190,162M184,167L190,167"/>' % stroke(LT, 1))
    for l in range(1, 4):                                    # SMEAR: streaks (3 steps x 4 frames), behind the flake
        for f in range(FZ_SMEAR):
            r3 = __import__('random').Random(l * 10 + 1); d = ''
            for i in range(2 * l + 1):
                y = FZ_C[1] - 30 + 60 * (i + 0.5) / (2 * l + 1) + r3.uniform(-3, 3)
                ln = (14, 24, 34)[l - 1] * r3.uniform(0.6, 1); off = (f * 4 + r3.uniform(0, 8)) % 16
                d += 'M%sH%s' % (pt(FZ_C[0] - 50 - off, y), f1(FZ_C[0] - 50 - off + ln))
                d += 'M%sH%s' % (pt(FZ_C[0] + 50 + off - ln, y), f1(FZ_C[0] + 50 + off))
            b.append(grp('fz_s%d_%d' % (l, f), pth(d, KNOB[3])))
    b.append(grp('fz_off', flake(12, 0, DIM, DIM, 1, 1)))
    for s in range(FZ_SIZES):                                # HOLD: the flake's size x its turn (8 frames: 60 degrees)
        for t in range(FZ_TURNS):
            b.append(grp('fz_h%d_%d' % (s, t), flake((20, 29, 38, 46)[s], t * 60 / FZ_TURNS, LT, KNOB[0])))
    d = 'M%d,%dV%d' % (FZ_RUL, FZ_RT, FZ_RB)                  # PITCH: the ruler, -12 at the bottom, +12 at the top
    for i in range(FZ_PITCHES):
        y = FZ_RB - i * (FZ_RB - FZ_RT) / (FZ_PITCHES - 1); big = i in (0, 12, 24)
        d += 'M%sH%d' % (pt(FZ_RUL, y), FZ_RUL + (8 if big else 4))
    b.append(pth(d, G, 1))
    for i in range(FZ_PITCHES):
        y = FZ_RB - i * (FZ_RB - FZ_RT) / (FZ_PITCHES - 1)
        b.append(grp('fz_p%d' % i, '<polygon fill="%s" points="%s %s %s"/>' % (KNOB[1], pt(FZ_RUL - 2, y), pt(FZ_RUL - 10, y - 4), pt(FZ_RUL - 10, y + 4))))
    for i, y in enumerate((182, 176, 170)):                  # the chord FOLLOW follows: three notes on a stem
        b.append('<ellipse fill="%s" cx="56" cy="%d" rx="3.6" ry="2.6"/>' % (KNOB[2], y))
    b.append('<line %s x1="63.6" y1="182" x2="63.6" y2="164"/>' % stroke(KNOB[2], 1))
    for on in (0, 1):                                        # FOLLOW: a link, apart (off) or joined (on)
        dx = 0 if on else 5
        c = KNOB[2] if on else DIM
        b.append(grp('fz_l%d' % on, '<ellipse %s cx="%d" cy="174" rx="8" ry="5"/><ellipse %s cx="%d" cy="174" rx="8" ry="5"/>'
                     % (stroke(c), 82 - dx, stroke(c), 94 + dx)))
    return '\n'.join(b + rows())


def wrap(name, b):
    n = len(b)
    (HERE / name).write_bytes(b'op1f picture 2\nsize %d\nfrom tempo.svg 43996 eb050950\nc 0 0 371\na %d\n%s\nc 0 43989 7\n' % (371 + n + 7, n, b))
    print('%s: %d bytes of ours' % (name, n))


BODIES = {'juneaux': juneaux_body, 'tape': tape_body, 'gate': gate_body, 'freeze': freeze_body}


def make():
    for name, body in BODIES.items(): wrap('%s.svg.make' % name, body().encode())
    (HERE / 'fx_face.h').write_text('// Made by face.py: do not edit.\n#pragma once\nenum { FX_SEGMENTS = %d, JX_DEPTHS = %d, JX_FRAMES = %d,\n'
        '       TP_SPOKES = %d, TP_WOW = %d, TP_FLUT = %d, TP_SPECKS = %d, GT_HEADS = %d, GT_SLOTS = %d, GT_DEPTHS = %d,\n'
        '       FZ_SIZES = %d, FZ_TURNS = %d, FZ_SMEAR = %d, FZ_PITCHES = %d };\n'
        % (SEGMENTS, JX_DEPTHS, JX_FRAMES, TP_SPOKES, TP_WOW, TP_FLUT, TP_SPECKS, GT_HEADS, GT_SLOTS, GT_DEPTHS, FZ_SIZES, FZ_TURNS, FZ_SMEAR, FZ_PITCHES))


def lvl(v, n): return (v * n + 16384) >> 15      # as the pack quantizes a knob: 0..n


def shown(name, v, t):
    """The animated groups the pack shows for knob values v (0..32767 each) at display frame t, as fx_pack.cpp does."""
    if name == 'juneaux': return {'jx_w%d_%d' % (lvl(v[2], JX_DEPTHS - 1), t % JX_FRAMES)} | ({'jx_led1'} if lvl(v[0], 2) != 1 else set()) | ({'jx_led2'} if lvl(v[0], 2) else set())
    if name == 'tape':
        return ({'tp_r%d' % (t % TP_SPOKES), 'tp_w%d_%d' % (lvl(v[0], 3), t % TP_WOW), 'tp_f%d_%d' % (lvl(v[1], 3), t % TP_FLUT), 'tp_h%d' % lvl(v[2], 3)}
                | {'tp_a%d' % i for i in range(lvl(v[3], TP_SPECKS))})
    if name == 'gate':
        pat, lq, sm, dq = v[1] * 8 >> 15, lvl(v[2], 3), lvl(v[3], 1), lvl(v[0], 3)
        g = {'gt_l%d_%d' % (pat * 4 + lq, sm), 'gt_d%d' % pat, 'gt_f%d' % dq, 'gt_p%d' % (t % GT_HEADS)}
        ln = 3277 + (v[2] * 29490 >> 15); pos = (t % GT_HEADS) * 4; cur = -1
        if pat == 3: cur = 16 + (pos * 384 // 256 >> 5) if (pos * 384 // 256 & 31) < (32 * ln >> 15) else -1
        else:
            hits = gt_slots(pat); h = max([x for x in hits if x * 16 <= pos] or [hits[-1]]); nx = hits[(hits.index(h) + 1) % len(hits)]
            span = ((nx - h) % 16 or 16) * 16
            if ((pos - h * 16) & 255) < (span * ln >> 15): cur = h
        for slot in gt_slots(pat): g.add('gt_o%d_%d' % (slot, max(dq - 1, 0)) if slot == cur and dq else 'gt_c%d' % slot)
        return g
    if name == 'freeze':
        g = {'fz_p%d' % ((v[1] * 24 + 16383) >> 15), 'fz_l%d' % (v[2] > 16384)}
        g.add('fz_h%d_%d' % (min(v[0] * FZ_SIZES >> 15, FZ_SIZES - 1), t % FZ_TURNS) if v[0] > 1000 else 'fz_off')
        if lvl(v[3], 3): g.add('fz_s%d_%d' % (lvl(v[3], 3), t % FZ_SMEAR))
        return g


ANIMATED = ('jx_w', 'jx_led', 'tp_', 'gt_', 'fz_')


def preview_svg(title, rows, b, show):
    """rows: (label, value text, 0..32767) per knob, as the pack's frame sets them; show: the animated groups shown."""
    import re
    for gid in re.findall(r'<g id="([a-z0-9_]+)">', b):
        if gid.startswith(ANIMATED) and gid not in show: b = b.replace('<g id="%s">' % gid, '<g id="%s" display="none">' % gid)
    for k, (label, value, v) in enumerate(rows):
        for s in range(lvl(v, SEGMENTS), SEGMENTS):
            b = b.replace('<g id="fx_b%ds%d">' % (k, s), '<g id="fx_b%ds%d" display="none">' % (k, s))
    texts = {'fx_title': (title, LT)}
    for k, (label, value, v) in enumerate(rows):
        if '"fx_lab%d"' % k in b: texts['fx_lab%d' % k] = (label, G); texts['fx_val%d' % k] = (value, LT)
    for gid, (s, c) in texts.items():   # the firmware's letters, drawn here in a plain font (shapes differ)
        i = b.index('<g id="%s">' % gid); d = b.index(' d="M', i); x, y = b[d + 5:b.index('V', d)].split(',')
        b = b[:i] + '<g id="%s">' % gid + b[b.index('</g>', i):]   # the placeholder becomes the label on the unit
        b += '<text x="%s" y="%d" fill="%s" font-family="Helvetica" font-size="18" letter-spacing="4">%s</text>' % (x, int(y) + 1, c, s.upper())   # about as wide as the firmware letters (SATURATE ~127 px on the unit)
    return ('<svg xmlns="http://www.w3.org/2000/svg" width="480" height="480" viewBox="0 0 480 480"><rect width="480" height="480" fill="#000005"/>'
            '<g transform="translate(0 130)"><rect width="480" height="220" fill="#000005"/>' + b + '</g></svg>')


# name -> (picture, title, rows, display frame); the first of each picture is page_<picture>.png, the others animation states
STATES = {
    'juneaux':  ('juneaux', [('mode', 'i', 0), ('mix', '100%', 32767), ('depth', '50%', 16384), ('tone', '50%', 16384)], 1),
    'tape':     ('tape', [('wow', '20%', 6500), ('flutter', '12%', 4000), ('saturate', '15%', 5000), ('age', '27%', 9000)], 0),
    'tape_b':   ('tape', [('wow', '75%', 24576), ('flutter', '62%', 20480), ('saturate', '100%', 32767), ('age', '87%', 28672)], 3),
    'gate':     ('gate', [('depth', '79%', 26000), ('pattern', '1/16', 10240), ('length', '50%', 16384), ('smooth', '25%', 8192)], 9),
    'gate_b':   ('gate', [('depth', '100%', 32767), ('pattern', 'trnc', 32767), ('length', '75%', 24576), ('smooth', '100%', 32767)], 40),
    'gate_c':   ('gate', [('depth', '50%', 16384), ('pattern', 'sync', 26000), ('length', '100%', 32767), ('smooth', '0%', 0)], 1),
    'gate_d':   ('gate', [('depth', '100%', 32767), ('pattern', '1/8t', 14000), ('length', '50%', 16384), ('smooth', '50%', 16384)], 22),
    'freeze':   ('freeze', [('hold', 'off', 0), ('pitch', '0', 16384), ('follow', 'on', 24576), ('smear', '37%', 12000)], 0),
    'freeze_b': ('freeze', [('hold', '75%', 24576), ('pitch', '+5', 23210), ('follow', 'on', 32767), ('smear', '62%', 20480)], 2),
    'freeze_c': ('freeze', [('hold', '100%', 32767), ('pitch', '-12', 0), ('follow', 'off', 0), ('smear', '100%', 32767)], 5),
}


def preview(out):
    out = Path(out); out.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory() as tmp:
        for name, (pic, rows, t) in STATES.items():
            f = Path(tmp) / ('page_%s.svg' % name)
            f.write_text(preview_svg(pic, rows, BODIES[pic](), shown(pic, [r[2] for r in rows], t)))
            subprocess.run(['qlmanage', '-t', '-s', str(480 * PREVIEW_SCALE), '-o', tmp, str(f)], check=True, capture_output=True)
            png = out / ('page_%s.png' % name)
            Path(tmp, f.name + '.png').replace(png)
            subprocess.run(['sips', '-c', str(220 * PREVIEW_SCALE), str(480 * PREVIEW_SCALE), str(png)], check=True, capture_output=True)
            print(png)


if __name__ == '__main__':
    if sys.argv[1:2] == ['make']: make()
    elif sys.argv[1:2] == ['preview']: preview(sys.argv[2] if len(sys.argv) > 2 else HERE / 'build')
    else: sys.exit(__doc__)
