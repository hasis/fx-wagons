// TAPE as fx.pack's wagon: tools/patch/fx_tape.h (the old chains' TAPE, which the monolithic build keeps) with its upper
// range made tape rather than fuzz. Uses that file's tape_sin, tape_rd and TAPE_N (fx_special.cpp includes it).
//
// What turned into distortion in the old one, and what this one does instead:
//   SATURATE  old: drive x1..x8, a hard clip at full scale, then x - x^3/3 and a make-up of 2 / (1 + drive): at the top
//             every peak was clipped flat, which is fuzz. Here: drive x1..x1.75, the cubic only up to its round top
//             (the input held to full scale where the curve is already flat, so no corner), level-matched (1 / drive:
//             small signals pass at unity, peaks are squeezed) and blended in by the knob: warmth and compression.
//   AGE       old: the roll-off to ~3 kHz and hiss up to ~-48 dBFS. Here: the same roll-off, the hiss at most ~-54 dBFS,
//             and over its upper half short dropouts (the level dips and recovers in some tens of ms), as worn tape does.
//   WOW, FLUTTER  unchanged: the old numbers, so they are what one hears grow.
// State as the old one's, plus obj[1] the dropout level and obj[2] its target (Q15, from 0 = none: written as 32767 - x).
static void fx_tape_wagon(FxSlot& f, short* mem, i32* L, i32* R){
    short* bl = mem, * br = mem + TAPE_N;
    const i32 wow = f.p[0], flut = f.p[1], sat = f.p[2], age = f.p[3];
    u32& seed = (u32&)f.s[7]; if (!seed) seed = 0x2545F491u;
    seed = seed * 1103515245u + 12345u; if ((seed >> 24) < 6) f.s[5] = (i32)((seed >> 8) & 0xffff) - 32768;
    f.s[4] += (f.s[5] - f.s[4]) >> 6;
    const i32 aw = wow * 170 >> 15, af = flut * 5 >> 15;
    const i32 g = 16 + (sat * 12 >> 15);                                // drive x16: 1 .. 1.75 (Q4)
    const i32 mk = 32768 * 16 / g;                                      // 1 / drive, Q15: unity for small signals
    const i32 k = age < 512 ? 32767 : 32767 - (age * 21300 >> 15);      // lowpass: open .. ~3 kHz, 2 poles (as before)
    const i32 hiss = age * age >> 24;                                   // up to ~64 in Q15 (~ -54 dBFS)
    // a dropout: over AGE's upper half, now and then the level dips (deeper and more often with AGE) and comes back
    i32& drop = (i32&)f.obj[1]; i32& dropto = (i32&)f.obj[2];
    if (age > 16384) {
        seed = seed * 1664525u + 1013904223u;
        if ((i32)(seed >> 22) < ((age - 16384) >> 12) && dropto == 0) dropto = 4000 + (i32)((seed >> 8) % (u32)((age - 16384) >> 1));   // up to ~ -6 dB
    }
    for (int i = 0; i < 64; ++i) {
        bl[f.pos & TAPE_M] = (short)(L[i] >> 16); br[f.pos & TAPE_M] = (short)(R[i] >> 16);
        f.lfo += 58436u;
        f.s[6] += 779000; f.obj[0] += 1266000u;
        const i32 w = (tape_sin(f.lfo) * 3 + (f.s[4] >> 0)) >> 2;
        const i32 fl = (tape_sin((u32)f.s[6]) * 3 + tape_sin(f.obj[0])) >> 2;
        const i32 d = (300 << 16) + w * aw * 2 + fl * af * 2;
        const u32 rp = (f.pos << 16) - (u32)d;
        ++f.pos;
        drop += (dropto - drop) >> 9;                                   // into the dip over ~10 ms ...
        if (dropto && drop >= dropto - 64) dropto = 0;                  // ... and out of it again
        for (int c = 0; c < 2; ++c) {
            i32 v = tape_rd(c ? br : bl, rp);                           // Q15
            if (sat > 256) {
                i32 x = v * g >> 4; if (x > 32767) x = 32767; if (x < -32767) x = -32767;   // held where the curve is flat
                x = x - (((x * x >> 15) * x >> 15) / 3);                // x - x^3/3: round, no corner
                x = x * mk >> 15;                                       // level-matched: small signals at unity, peaks squeezed
                v += (x - v) * sat >> 15;                               // blended in by the knob
            }
            if (k < 32767) {
                seed = seed * 1664525u + 1013904223u; v += (((i32)(seed >> 16) - 32768) * hiss) >> 15;
                i32& y1 = f.s[c * 2], & y2 = f.s[c * 2 + 1];
                y1 += ((v - y1) * k) >> 15; y2 += ((y1 - y2) * k) >> 15; v = y2;
            }
            if (drop) v = v * (32767 - drop) >> 15;
            if (v > 32767) v = 32767; if (v < -32767) v = -32767;
            (c ? R : L)[i] = v << 16;
        }
    }
}
