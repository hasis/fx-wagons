// The Juno-60 chorus of the old chains, as a function: the case FX_JUNO of fx_run in tools/patch/mod_cherd.cpp, moved
// here unchanged (fx.pack and its host check use it; the monolithic build keeps its own copy). Needs FxSlot, mulq15 and
// sat_add32 as mod_cherd.cpp has them.
static void fx_juno(FxSlot& f, short* mem, i32* L, i32* R){
    // Juno-60-style chorus (our own code). One BBD-like line, two taps on opposite LFO phases (L / R). Numbers from pendragon-andyh/Juno60 (Sonic Visualiser + jpcima): modes I
    // 0.513 Hz and II 0.863 Hz, triangle, sweep 1.66..5.35 ms; I+II a 9.75 Hz sine over 3.3..3.7 ms, MONO; dry
    // summed 1.62 dB under the wet (schematic resistors); ~9 kHz anti-alias low-pass before the line and a 2-pole
    // ~7.5 kHz one after it (emulation values, unmeasured). Knobs: mode (thirds), mix (full = the original balance),
    // depth (x0..x2 around the mode), tone (post low-pass, centre ~7.5 kHz).
    const int mode = f.p[0] < 10923 ? 0 : f.p[0] < 21845 ? 1 : 2;
    static const u32 kInc[3] = {49962u, 84049u, 949617u};               // LFO Hz (pendragon-andyh/Juno60): I 0.513, II 0.863, I+II 9.75; step = 2^32/44100 * Hz
    static const int kLo[3] = {4797628, 4797628, 9537454}, kHi[3] = {15462236, 15462236, 10693509};   // delay Q16 samples: I/II 1.66..5.35 ms, I+II 3.3..3.7 ms
    const int centre = (kLo[mode] >> 1) + (kHi[mode] >> 1), half0 = (kHi[mode] >> 1) - (kLo[mode] >> 1);
    const int half = (half0 >> 7) * (f.p[2] >> 7);                      // depth knob: x0..x2 (16384 = x1), Q16 samples
    const int mix = f.p[1], kLp = 12000 + (f.p[3] * 19000 >> 15);      // post low-pass coefficient, Q15 (~7.5 kHz at centre)
    short* line = mem;
    for (int i = 0; i < 64; ++i) {
        i32& pre = f.s[4]; pre += mulq15(((L[i] >> 2) + (R[i] >> 2)) - pre, 23678);   // ~9 kHz anti-alias, mono, Q30
        line[f.pos & 511] = (short)(pre >> 15);                         // into the line, Q15
        f.lfo += kInc[mode];
        const int ph = (int)(f.lfo >> 16), tri = ph < 32768 ? ph * 2 : (65535 - ph) * 2;   // 0..65534
        int m = tri - 32767;                                            // -32767..32767
        if (mode == 2) m = (m * (98304 - ((m * m) >> 15))) >> 16;       // I+II: triangle shaped to a sine, x (3 - x^2) / 2
        for (int c = 0; c < 2; ++c) {
            const int mm = (c && mode != 2) ? -m : m;                   // I/II: right tap opposite phase (stereo); I+II: same tap both = mono (vibrato/Leslie, per pendragon)
            const int d = centre + (((half >> 8) * (mm >> 3)) >> 4);    // Q16 samples: centre +- half
            const int rp = (int)((f.pos << 16) - (u32)d);
            const int i0 = (rp >> 16) & 511, fr = rp & 0xffff;
            const int a0 = line[i0], a1 = line[(i0 + 1) & 511];
            const int y = a0 + (((a1 - a0) * (fr >> 2)) >> 14);          // linear interpolation
            i32& p1 = f.s[c * 2], & p2 = f.s[c * 2 + 1];
            p1 += mulq15((y << 15) - p1, kLp); p2 += mulq15(p1 - p2, kLp);   // 2-pole post low-pass, Q30
            i32* x = c ? &R[i] : &L[i];
            *x = sat_add32(mulq15(*x, 19035), mulq15(mulq15(p2 << 1, mix), 22938));   // dry 0.58, wet 0.7 (-1.62 dB)
        }
        ++f.pos;
    }
    return;
}
