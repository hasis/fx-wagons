// ---------- Chord-aware FX: GATE, FREEZE, CLOUD, CANON (#included by mod_cherd.cpp just before fx_run) ----------
// Effects a stock Field can't have: they follow CHERD's chord (g_root / g_window / g_lowc) and the tempo clock.
// Memory per slot is the usual 32 KiB = one 16384-sample mono buffer (0.37 s; CANON stores at half rate = 0.74 s).
// Grain / tap state lives in FxSlot.obj (160 bytes: 8 grains of 20). s[7] is each effect's random seed.
// Knob 1 is the one the chain page turns: GATE depth, FREEZE hold, CLOUD mix, CANON mix.
// ponytail: 0.37 s buffers cap grain reach and echo time; a bigger FX arena (heap shrink) is the upgrade path.

static const u32 kSemi[49] = {16384, 17358, 18390, 19484, 20643, 21870, 23170, 24548, 26008, 27554, 29193, 30929, 32768, 34716,
    36781, 38968, 41285, 43740, 46341, 49097, 52016, 55109, 58386, 61858, 65536, 69433, 73562, 77936, 82570, 87480, 92682, 98193,
    104032, 110218, 116772, 123715, 131072, 138866, 147123, 155872, 165140, 174960, 185364, 196386, 208064, 220436, 233544, 247431, 262144};   // 2^(n/12), n = -24..24, Q16
static inline u32 semi_inc(int s){ s = s < -24 ? -24 : s > 24 ? 24 : s; return kSemi[s + 24]; }
static const u32 FXN = 16384, FXM = FXN - 1;

// The chord CHERD shows, as semitones above its root (ascending, the root first). Nothing shown yet: a fifth.
static int chord_tones(int* out){
    u32 m = 1;
    if (g_shown) { const u32 w = g_window, lo = g_lowc, r = g_root % 12; for (u32 b = 0; b < 32; ++b) if (w >> b & 1) m |= 1u << ((lo + b + 12 - r) % 12); }
    else m |= 1u << 7;
    int n = 0; for (int i = 0; i < 12; ++i) if (m >> i & 1) out[n++] = i;
    return n;
}

// Tempo clock for the rhythmic effects, at block rate (called once per 64-frame block from the synth chain hook). Free-runs
// on the BPM and snaps to the OS beat counter A_BEAT (wraps 47 -> 0 once a beat; a jump to 0 from elsewhere = transport start).
static volatile u32 g_clk_phase = 0, g_clk_beat = 0, g_clk_inc = 0; static int g_clk_last = -1;
static inline int beat_now(){ return *(volatile int*)A_BEAT; }
static void clock_block(){
    const int b = beat_now();
    g_clk_inc = bpm_now() * 103884u;                                    // 2^32 per beat: bpm / 60 / 44100 * 64 frames
    if (b == 0 && g_clk_last > 0 && g_clk_last != 47) { g_clk_phase = 0; g_clk_beat = 0; }                       // transport start: bar 1
    else if (b == 0 && g_clk_last == 47) { if (g_clk_phase > 0x80000000u) ++g_clk_beat; g_clk_phase = 0; }        // the OS beat: snap
    else { const u32 np = g_clk_phase + g_clk_inc; if (np < g_clk_phase) ++g_clk_beat; g_clk_phase = np; }
    g_clk_last = b;
}
// Position in the bar of frame i of this block, in 384ths (96 a beat, as the comp patterns).
static inline u32 bar_pos(int i){
    const u32 ph = g_clk_phase + (u32)i * (g_clk_inc >> 6);
    return (g_clk_beat & 3) * 96u + (((ph >> 16) * 96u) >> 16);
}

static inline u32 frnd(FxSlot& f){ u32& r = (u32&)f.s[7]; if (!r) r = 0x9e3779b9u ^ (u32)(unsigned long)&f; r = r * 1103515245u + 12345u; return r >> 8; }
static inline i32 tap(const short* b, u32 pos){            // pos Q16 samples; linear interpolation, Q15 out
    const u32 i = pos >> 16; const i32 fr = (i32)((pos >> 1) & 0x7fff), a0 = b[i & FXM], a1 = b[(i + 1) & FXM];
    return a0 + (((a1 - a0) * fr) >> 15);
}
static inline short clip15(i32 v){ return (short)(v > 32767 ? 32767 : v < -32767 ? -32767 : v); }

// A grain: read position and speed (Q16), window phase x (Q30, done at 1 << 30) and its step, pan (-16384..16384).
struct Grain { u32 pos, inc, x, winc; i32 pan; };
static inline Grain* grains(FxSlot& f){ return (Grain*)f.obj; }
static inline i32 grain_win(u32 x){ const i32 x15 = (i32)(x >> 15); return (x15 * (32768 - x15)) >> 13; }   // 4x(1-x), Q15
// Sum the live grains of a slot into Q15 left / right.
static inline void grains_sum(Grain* g, int n, const short* buf, i32* gl, i32* gr){
    i32 l = 0, r = 0;
    for (int k = 0; k < n; ++k) { Grain& q = g[k]; if (q.x >= (1u << 30)) continue;
        const i32 v = (tap(buf, q.pos) * grain_win(q.x)) >> 15;
        l += (v * (16384 - q.pan)) >> 15; r += (v * (16384 + q.pan)) >> 15;
        q.pos += q.inc; q.x += q.winc; }
    *gl = l; *gr = r;
}

// GATE: chops the sound on a rhythm from CHERD's comp patterns (with the feel page's swing), locked to the tempo clock.
// Knobs: depth (how far it closes), pattern, length (of each opening, part of the gap to the next hit), smooth (edges).
static const u16 kGateMask[8] = {0x1111, 0x5555, 0xFFFF, 0, 0x4444, 0x0041, 0x0449, 0xADAD};   // bit = 16th; 1/8T (3) is special
static const char* const gate_names[8] = {"1/4", "1/8", "1/16", "1/8T", "STAB", "CHRL", "SYNC", "TRANCE"};
static inline int gate_pattern(u16 p){ return p * 8 >> 15; }
static void fx_gate(FxSlot& f, i32* L, i32* R){
    const int pat = gate_pattern(f.p[1]); const u16 mask = kGateMask[pat];
    const i32 shut = 32767 - f.p[0], len = 3277 + (f.p[2] * 29490 >> 15);     // length 10..100 % of the gap
    const i32 k = 4000 >> (f.p[3] * 6 >> 15);                                // edge: ~0.2 ms .. ~12 ms one-pole
    static const u8 amount[3] = {0, 8, 16}; const u32 sw = amount[g_swing % 3];
    i32& env = f.s[0]; if (!f.s[1]) { f.s[1] = 1; env = 32767; }
    for (int i = 0; i < 64; ++i) {
        const u32 pos = bar_pos(i); bool open;
        if (pat == 3) open = (i32)(pos % 32u) < (32 * len >> 15);              // triplet 8ths: 12 a bar
        else {
            const u32 s = pos / 24u; u32 since = 0, span = 384; open = false;
            for (u32 back = 0; back < 16; ++back) { const u32 st = (s + 16 - back) & 15; if (!(mask >> st & 1)) continue;
                const u32 at = st * 24 + (st % 4 == 2 ? sw : st % 2 == 1 ? sw / 2 : 0);
                if (back == 0 && at > pos) continue;                         // swung late: not yet
                since = (pos + 384 - at) % 384u;
                for (u32 fw = 1; fw <= 16; ++fw) if (mask >> ((st + fw) & 15) & 1) { span = fw * 24; break; }
                open = (i32)since < (i32)(span * (u32)len >> 15); break; }
        }
        env += mulq15((open ? 32767 : shut) - env, k);
        L[i] = mulq15(L[i], env); R[i] = mulq15(R[i], env);
    }
}

// FREEZE: turn HOLD up from zero and the last moment is captured and held as a pad (grains over the frozen buffer);
// back to zero lets it go. PITCH transposes it (-12..+12), FOLLOW moves it with CHERD's chord root, SMEAR shortens the
// grains (grainier, more blurred). s[0] frozen, s[1] root at capture, s[3] wet level.
static inline int freeze_semis(u16 p){ return (int)((p * 24 + 16383) >> 15) - 12; }
static void freeze_spawn(FxSlot& f, Grain& q, u32 inc, u32 x0){
    u32 len = 8000 - (u32)(f.p[3] * 6500 >> 15);                            // ~180 .. 34 ms
    const u32 reach = (u32)(((len >> 4) * (inc >> 4)) >> 8) + 256;          // samples the grain travels, plus a guard
    if (reach > FXN - 512) len = (FXN - 768) * 65536u / inc;
    const u32 span = FXN - 256 - (u32)(((len >> 4) * (inc >> 4)) >> 8);     // where it may start without crossing the seam
    const u32 back = 256 + (u32)(((len >> 4) * (inc >> 4)) >> 8) + (span > 512 ? frnd(f) % (span - 512) : 0);
    q.pos = ((f.pos - back) & FXM) << 16; q.inc = inc; q.winc = (1u << 30) / len; q.x = x0; q.pan = (i32)(frnd(f) % 20001u) - 10000;
}
static void fx_freeze(FxSlot& f, short* buf, i32* L, i32* R){
    Grain* g = grains(f);
    const bool hold = f.p[0] > 1000;
    if (hold && !f.s[0]) { f.s[0] = 1; f.s[1] = (i32)(g_root % 12); for (int k = 0; k < 4; ++k) g[k].x = 1u << 30; f.s[2] = 1; }   // capture
    if (!hold) f.s[0] = 0;
    int semis = freeze_semis(f.p[1]);
    if (f.p[2] > 16384 && g_shown) { int d = ((int)(g_root % 12) - f.s[1] + 12) % 12; if (d > 6) d -= 12; semis += d; }
    const u32 inc = semi_inc(semis);
    if (f.s[2]) { f.s[2] = 0; for (int k = 0; k < 4; ++k) freeze_spawn(f, g[k], inc, (u32)k << 28); }   // four grains, a quarter apart
    for (int k = 0; k < 4; ++k) g[k].inc = inc;                             // live pitch (FOLLOW)
    i32& wet = f.s[3]; const i32 want = f.s[0] ? f.p[0] : 0;
    for (int i = 0; i < 64; ++i) {
        if (!f.s[0]) { buf[f.pos] = clip15((L[i] >> 17) + (R[i] >> 17)); f.pos = (f.pos + 1) & FXM; }
        wet += (want - wet) >> 9;
        if (!wet && !f.s[0]) continue;
        for (int k = 0; k < 4; ++k) if (g[k].x >= (1u << 30)) freeze_spawn(f, g[k], inc, 0);
        i32 gl, gr; grains_sum(g, 4, buf, &gl, &gr);
        L[i] = sat_add32(L[i], mulq15((i32)clip15(gl * 12000 >> 15) << 16, wet)); R[i] = sat_add32(R[i], mulq15((i32)clip15(gr * 12000 >> 15) << 16, wet));
    }
}

// CLOUD: a stream of short grains from the last fraction of a second, each pitched to a tone of CHERD's chord and panned
// at random. MIX, DENSITY (1..8 overlapping), SIZE (20..200 ms), PITCH: unison (a little detune), chord, chord + octaves.
static const char* const cloud_names[3] = {"UNISON", "CHORD", "WIDE"};
static void fx_cloud(FxSlot& f, short* buf, i32* L, i32* R){
    Grain* g = grains(f);
    const int over = 1 + (f.p[1] * 7 >> 15), zone = f.p[3] * 3 >> 15;
    static const i32 kOlap[8] = {32767, 23170, 18918, 16384, 14654, 13377, 12385, 11585};   // 1/sqrt(overlap)
    const u32 len0 = 880 + (u32)(f.p[2] * 7940 >> 15);
    int tones[12]; const int nt = chord_tones(tones);
    if (!f.s[1]) { f.s[1] = 1; for (int k = 0; k < 8; ++k) g[k].x = 1u << 30; }   // fresh slot: every grain free
    i32& timer = f.s[0]; const i32 mix = f.p[0];
    for (int i = 0; i < 64; ++i) {
        buf[f.pos] = clip15((L[i] >> 17) + (R[i] >> 17)); f.pos = (f.pos + 1) & FXM;
        if (--timer <= 0) {
            int k = 0; while (k < 8 && g[k].x < (1u << 30)) ++k;
            if (k < 8) { Grain& q = g[k];
                u32 inc = 65536;
                if (zone == 0) inc = 65536 - 300 + frnd(f) % 600;              // unison, ~ +-8 cents
                else { int s = tones[frnd(f) % (u32)nt]; if (zone == 2) s += 12 * ((int)(frnd(f) % 3) - 1); inc = semi_inc(s); }
                u32 len = len0; const u32 cap = 6000u * 65536u / inc; if (len > cap) len = cap;
                const u32 travel = (u32)(((len >> 4) * (inc >> 4)) >> 8), ahead = travel > len ? travel - len : 0;
                const u32 room = FXN - 512 - travel - len, back = 64 + ahead + (room > 64 ? frnd(f) % room : 0);
                q.pos = ((f.pos - back) & FXM) << 16; q.inc = inc; q.winc = (1u << 30) / len; q.x = 0; q.pan = (i32)(frnd(f) % 28001u) - 14000; }
            timer = (i32)(len0 / (u32)over);
        }
        i32 gl, gr; grains_sum(g, 8, buf, &gl, &gr);
        gl = clip15((gl * kOlap[over - 1]) >> 15); gr = clip15((gr * kOlap[over - 1]) >> 15);
        L[i] = sat_add32(mulq15(L[i], 32767 - (mix >> 1)), mulq15(gl << 16, mix));
        R[i] = sat_add32(mulq15(R[i], 32767 - (mix >> 1)), mulq15(gr << 16, mix));
    }
}

// CANON: up to three echoes, each shifted to the next tone of CHERD's chord (UP, DOWN), or by fifths or octaves, so one note
// comes back as an arpeggio; FEEDBACK sends the last echo round again, shifted again. Runs at half rate (darker repeats,
// 0.74 s of line). MIX, TIME (1/16, 1/8T, 1/8, 1/4 of the tempo; echoes that don't fit the line are dropped), MODE, FEEDBACK.
// obj: three tap phases (Q16, 0..W). s[0] half-rate toggle, s[1] previous input, s[2..3] held output, s[4..5] smoothing, s[6] last tap.
static const char* const canon_time_names[4] = {"1/16", "1/8T", "1/8", "1/4"};
static const char* const canon_mode_names[4] = {"UP", "DOWN", "FIFTHS", "OCTAVES"};
static const u32 CANON_W = 1024;                                             // pitch-shift window, half-rate samples (~46 ms)
static void fx_canon(FxSlot& f, short* buf, i32* L, i32* R){
    u32* ph = (u32*)f.obj;
    const int ti = f.p[1] * 4 >> 15, mode = f.p[2] * 4 >> 15;
    static const u8 den[4] = {4, 3, 2, 1};
    u32 T = 1323000u / bpm_now() / den[ti]; if (T < CANON_W) T = CANON_W;
    int taps = (int)((FXN - CANON_W - 8) / T); if (taps > 3) taps = 3;
    int semis[3];
    if (mode >= 2) { static const signed char kFix[2][3] = {{7, 12, 19}, {12, -12, 24}}; for (int n = 0; n < 3; ++n) semis[n] = kFix[mode - 2][n]; }
    else { int t[12]; const int nt = chord_tones(t);
        for (int n = 0; n < 3; ++n) semis[n] = mode == 0 ? t[(n + 1) % nt] + 12 * ((n + 1) / nt) : t[(nt - 1 - n % nt + nt) % nt] - 12 * (1 + n / nt); }
    const i32 fb = f.p[3] * 24576 >> 15, mix = f.p[0];
    for (int i = 0; i < 64; ++i) {
        const i32 m = (L[i] >> 17) + (R[i] >> 17);
        if ((f.s[0] ^= 1)) { f.s[1] = m; }
        else if (taps > 0) {
            buf[f.pos] = clip15(((f.s[1] + m) >> 1) + mulq15(f.s[6], fb));
            i32 out[3] = {0, 0, 0};
            for (int n = 0; n < taps; ++n) {
                const u32 r = semi_inc(semis[n]); ph[n] = (ph[n] + 65536u - r) % (CANON_W << 16);   // the head drifts at 1 - ratio
                i32 acc = 0;
                for (int h = 0; h < 2; ++h) {
                    const u32 phh = (ph[n] + (h ? (CANON_W << 15) : 0)) % (CANON_W << 16);
                    const u32 d = (u32)(n + 1) * T * 65536u + phh;                    // Q16 delay
                    const i32 v = tap(buf, (f.pos << 16) - d);
                    const u32 x = phh / CANON_W;                                       // 0..65535 across the window
                    const i32 w = (i32)(x < 32768 ? x : 65535 - x);                    // triangle, 0..32767
                    acc += (v * w) >> 14;
                }
                out[n] = acc >> 1;
            }
            f.s[6] = out[taps - 1];
            f.s[2] = clip15((out[0] * 26000 + out[2] * 16000) >> 15); f.s[3] = clip15((out[1] * 26000 + out[2] * 16000) >> 15);
            f.pos = (f.pos + 1) & FXM;
        }
        f.s[4] += mulq15((f.s[2] << 16) - f.s[4], 20000); f.s[5] += mulq15((f.s[3] << 16) - f.s[5], 20000);   // smooth the half-rate steps
        L[i] = sat_add32(L[i], mulq15(f.s[4], mix)); R[i] = sat_add32(R[i], mulq15(f.s[5], mix));
    }
}

#include "fx_tape.h"   // TAPE: wow, flutter, saturation, age (also built alone by tools/tape/render.cpp)

// DUCK: a sidechain. A trigger pulls the level down, then it recovers. TRIGGER: ALL = the drums' audio (any hit, any
// sequencer, live keys), one drum key 1..24 (pattern steps and ratchets: g_duck_cnt; key 1 is the kick on most kits), or TRACK 1..4 (that tape track's audio
// crossing ~ -20 dBFS: a recorded kick pumps the other tracks). DEPTH how far (to silence at full), RELEASE 40..800 ms,
// SHAPE linear .. a late, pumping recovery. obj: the drum counters last seen (24 bytes); s[0] envelope (Q23, 1 = just
// hit), s[1] counters synced, s[2] attacking, s[3] track trigger re-armed (fell below half the threshold).
static inline int duck_key(u16 p){ return (int)(p * 29 >> 15) - 1; }       // -1 ALL, 0..23 drum keys, 24..27 tape tracks
static void fx_duck(FxSlot& f, i32* L, i32* R){
    u8* seen = (u8*)f.obj; const int key = duck_key(f.p[2]); bool hit = false;
    for (int k = 0; k < 24; ++k) { const u8 c = g_duck_cnt[k]; if (seen[k] != c) { if (f.s[1] && key == k) hit = true; seen[k] = c; } }
    { const u32 a = g_duck_any; if (f.obj[6] != a) { if (f.s[1] && key < 0) hit = true; f.obj[6] = a; } }   // ALL: any counted drum hit...
    f.s[1] = 1;
    if (key < 0 || key >= 24) { const i32 lv = key < 0 ? g_drum_lvl : g_trk_lvl[key - 24];   // ...or the drum bus / a tape track: rising edge over the threshold
        if (lv > 3300 && f.s[3]) { hit = true; f.s[3] = 0; } else if (lv < 1650) f.s[3] = 1; }
    if (hit) f.s[2] = 1;
    const i32 ONE = 1 << 23, rel = 1764 + (f.p[1] * 33516 >> 15), step = ONE / rel, depth = f.p[0], shape = f.p[3];
    i32& env = f.s[0];
    for (int i = 0; i < 64; ++i) {
        if (f.s[2]) { env += (ONE - env) >> 6; if (env > ONE - (ONE >> 6)) { env = ONE; f.s[2] = 0; } }   // ~1.5 ms attack: no click
        else if (env > 0) { env -= step; if (env < 0) env = 0; }
        const i32 e = env >> 8, u = 32767 - e, late = 32767 - ((((u * u) >> 15) * u) >> 15);   // e and 1 - (1 - e)^3, Q15
        const i32 c = e + (((late - e) * shape) >> 15), g = 32767 - ((depth * c) >> 15);
        L[i] = mulq15(L[i], g); R[i] = mulq15(R[i], g);
    }
}
