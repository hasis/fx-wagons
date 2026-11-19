// ---------- TAPE: a worn tape machine (mellotron / cassette character) ----------
// #included by fx_special.cpp; also compiled alone by tools/tape/render.cpp (host renders), so it only needs FxSlot's
// p[4] / s[8] / pos / lfo, u16 / i32 / u32 and mulq15. Signal: i32 with the audio in the top 16 bits (as every chain).
//   1st WOW      slow pitch drift (0.6 Hz swing + a random wander), up to about +-25 cents
//   2nd FLUTTER  fast pitch shake (~8 Hz, with a 13 Hz scrape), up to about +-8 cents
//   3rd SATURATE drive into a soft clip with a little level make-up (tape compression)
//   4th AGE      the top rolls off (open .. ~3 kHz) and hiss comes up
// Wow and flutter move one read head over both channels (one tape transport), through a 2048-sample line per channel.
// State: s[0..3] the two lowpass poles per channel, s[4] wander value, s[5] wander target, s[6] flutter phase,
// s[7] noise seed; obj[0] scrape phase; lfo = wow phase; pos = write index.
static const u32 TAPE_N = 2048, TAPE_M = TAPE_N - 1;
static inline i32 tape_sin(u32 ph){                                     // phase 2^32 a turn -> Q15 sine (parabola, < 0.1 % THD)
    const i32 x = (i32)(ph >> 16) - 32768;                              // -32768..32767 = -pi..pi
    const i32 y = (x * (32768 - (x < 0 ? -x : x))) >> 13;               // 4x(1-|x|), Q15, sign flipped below
    return -(y + ((((y * (y < 0 ? -y : y)) >> 15) - y) * 7209 >> 15));  // + 0.22 (y|y| - y): the classic refinement
}
static inline i32 tape_rd(const short* b, u32 pos){                     // pos Q16 samples; linear interpolation, Q15 out
    const u32 i = pos >> 16; const i32 fr = (i32)((pos >> 1) & 0x7fff), a0 = b[i & TAPE_M], a1 = b[(i + 1) & TAPE_M];
    return a0 + (((a1 - a0) * fr) >> 15);
}
static void fx_tape(FxSlot& f, short* mem, i32* L, i32* R){
    short* bl = mem, * br = mem + TAPE_N;
    const i32 wow = f.p[0], flut = f.p[1], sat = f.p[2], age = f.p[3];
    u32& seed = (u32&)f.s[7]; if (!seed) seed = 0x2545F491u;
    // wander: a new random target every block, eased toward (a slow, irregular drift on top of the swing)
    seed = seed * 1103515245u + 12345u; if ((seed >> 24) < 6) f.s[5] = (i32)((seed >> 8) & 0xffff) - 32768;
    f.s[4] += (f.s[5] - f.s[4]) >> 6;
    const i32 aw = wow * 170 >> 15, af = flut * 5 >> 15;                // depth in samples (Q0): 170 -> +-25 c at 0.6 Hz
    const int g = 16 + (sat * 112 >> 15);                               // drive x16 (1..8 in Q4)
    const i32 mk = 32768 * 2 * 16 / (16 + g);                           // make-up 2 / (1 + drive), Q15
    const i32 k = age < 512 ? 32767 : 32767 - (age * 21300 >> 15);      // lowpass: open .. ~3 kHz, 2 poles
    const i32 hiss = age * age >> 23;                                   // up to ~128 in Q15 (~ -48 dBFS), filtered below
    for (int i = 0; i < 64; ++i) {
        bl[f.pos & TAPE_M] = (short)(L[i] >> 16); br[f.pos & TAPE_M] = (short)(R[i] >> 16);
        f.lfo += 58436u;                                                // 0.6 Hz
        f.s[6] += 779000; f.obj[0] += 1266000u;                        // ~8 Hz flutter, ~13 Hz scrape
        const i32 w = (tape_sin(f.lfo) * 3 + (f.s[4] >> 0)) >> 2;       // swing + wander, Q15
        const i32 fl = (tape_sin((u32)f.s[6]) * 3 + tape_sin(f.obj[0])) >> 2;
        const i32 d = (300 << 16) + w * aw * 2 + fl * af * 2;           // Q16 samples behind the write head (>= ~130)
        const u32 rp = (f.pos << 16) - (u32)d;
        ++f.pos;
        for (int c = 0; c < 2; ++c) {
            i32 v = tape_rd(c ? br : bl, rp);                           // Q15
            if (sat > 256) {
                v = v * g >> 4; if (v > 32767) v = 32767; if (v < -32767) v = -32767;
                v = v - (((v * v >> 15) * v >> 15) / 3);                // x - x^3/3
                v = v * mk >> 15;
            }
            if (k < 32767) {
                seed = seed * 1664525u + 1013904223u; v += (((i32)(seed >> 16) - 32768) * hiss) >> 15;   // hiss, then the roll-off darkens it
                i32& y1 = f.s[c * 2], & y2 = f.s[c * 2 + 1];
                y1 += ((v - y1) * k) >> 15; y2 += ((y1 - y2) * k) >> 15; v = y2;
            }
            if (v > 32767) v = 32767; if (v < -32767) v = -32767;
            (c ? R : L)[i] = v << 16;
        }
    }
}
