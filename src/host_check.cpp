// Host check for fx.pack: each wagon (fx_wagons.h, the code the unit runs) called as fx train calls a wagon
// (op1f_wagon.h: the instance table, the state in the train's region, the eight words a block), against the old code
// called as the old chains' fx_run called it (FxSlot p[] = the knobs, the slot's 32 KiB, clock_block once a block before
// GATE). The two must agree bit for bit. TAPE's reference is fx_tape_wagon.h, the reworked TAPE (the old fx_tape.h is
// no longer the wagon's sound: the ladder below sets the two side by side). Also checks that fx_juno.h is mod_cherd.cpp's
// JUNO case unchanged, and writes dry and wet WAVs for the ear.
//   c++ -std=c++17 -O2 -I tools/patch -I tools/pack/fx tools/pack/fx/host_check.cpp -o /tmp/fx && /tmp/fx OUTDIR [LADDER_DIR]
// With LADDER_DIR: TAPE old and new at 25, 50, 75 and 100 % of all four knobs and at the default, on the test pad and line
// and on every 16-bit WAV in LADDER_DIR/src, written as LADDER_DIR/<source>_<level>_<old|new>.wav.
#include <cassert>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <algorithm>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

static volatile uint32_t host_bpm = 0x42f00000u;   // 120.0f, as tempo.cpp keeps it
static volatile int host_beat;                      // tempo.cpp's counter: 48 a beat, wrapping 47 -> 0
#define A_BPM ((uintptr_t)&host_bpm)
#define A_BEAT ((uintptr_t)&host_beat)
static int fx_new_block() { return 1; }             // one GATE a block here
#include "fx_wagons.h"
#include "op1f_wagon.h"

static const int SR = 44100, FR = 64;

// ---------------------------------------------------------------- fx train's side (module/build_h_chain.cpp)
typedef void (*ProcessFn)(void*, int*, int*, void*);
typedef void (*InitFn)(void*, int);
static void plug_process(void* obj, int* L, int* R, void* ctx) { auto o = (uintptr_t*)obj; ((const Op1fWagon*)o[1])->process((void*)o[2], L, R, (const volatile unsigned*)ctx); }
static void plug_init(void* obj, int) { auto o = (uintptr_t*)obj; const Op1fWagon* w = (const Op1fWagon*)o[1]; if (w->init) w->init((void*)o[2]); }
static const struct { void* none[2]; ProcessFn process; InitFn init; } PLUG_TABLE = { { 0, 0 }, plug_process, plug_init };

// the pack's wagons (fx_pack.cpp), without CHERD here
template <int K> static void w_process(void* s, int* L, int* R, const volatile unsigned* p) { fx_chord(0); fx_wagon_block(K, s, L, R, p); }
#define W(NAME, K, DEF) { OP1F_WAGON_MAGIC, OP1F_WAGON_VERSION, NAME, sizeof(FxState), 0, { DEF, 0, 0, 0, 0 }, 0, w_process<K>, 0, 0 }
static const Op1fWagon WAGONS[FX_WAGONS] = { W("juneaux", FX_W_JUNO, FX_DEF_JUNO), W("tape", FX_W_TAPE, FX_DEF_TAPE), W("gate", FX_W_GATE, FX_DEF_GATE), W("freeze", FX_W_FREEZE, FX_DEF_FREEZE) };

// ---------------------------------------------------------------- material
static double mfreq(double n) { return 440 * std::pow(2.0, (n - 69) / 12); }
struct Note { double at, len, midi; };
static void synth(std::vector<int>& L, std::vector<int>& R, double seconds, const std::vector<Note>& notes) {
    const size_t n = size_t(seconds * SR) / FR * FR;
    std::vector<double> l(n, 0), r(n, 0);
    for (const Note& no : notes) {
        double ph = 0; const double f = mfreq(no.midi);
        for (size_t i = size_t(no.at * SR); i < n && i < size_t((no.at + no.len) * SR); ++i) {
            const double t = (i - no.at * SR) / SR, e = std::fmin(1, t / 0.01) * std::fmin(1, (no.len - t) / 0.05);
            ph += f / SR; double v = 0; for (int h = 1; h <= 12; ++h) v += std::sin(2 * M_PI * h * ph) / h;
            l[i] += 0.18 * e * v; r[i] += 0.18 * e * v;
        }
    }
    L.resize(n); R.resize(n);
    for (size_t i = 0; i < n; ++i) { L[i] = int(std::fmax(-1, std::fmin(1, l[i])) * 2147483647.0); R[i] = int(std::fmax(-1, std::fmin(1, r[i])) * 2147483647.0); }
}
static void wav(const std::string& fn, const std::vector<int>& L, const std::vector<int>& R) {
    FILE* f = std::fopen(fn.c_str(), "wb"); assert(f);
    const uint32_t n = uint32_t(L.size()), d = n * 4, c = 36 + d, sr = SR, br = SR * 4, fmt = 16; const uint16_t pcm = 1, ch = 2, al = 4, bits = 16;
    std::fwrite("RIFF", 1, 4, f); std::fwrite(&c, 4, 1, f); std::fwrite("WAVEfmt ", 1, 8, f); std::fwrite(&fmt, 4, 1, f); std::fwrite(&pcm, 2, 1, f);
    std::fwrite(&ch, 2, 1, f); std::fwrite(&sr, 4, 1, f); std::fwrite(&br, 4, 1, f); std::fwrite(&al, 2, 1, f); std::fwrite(&bits, 2, 1, f);
    std::fwrite("data", 1, 4, f); std::fwrite(&d, 4, 1, f);
    for (uint32_t i = 0; i < n; ++i) { int16_t a = int16_t(L[i] >> 16), b = int16_t(R[i] >> 16); std::fwrite(&a, 2, 1, f); std::fwrite(&b, 2, 1, f); }
    std::fclose(f);
}

// the knobs over time: (from second, the four values)
struct Knobs { double at; unsigned short p[4]; };
static const unsigned short* knobs_at(const std::vector<Knobs>& k, double t) { const unsigned short* p = k[0].p; for (auto& e : k) if (t >= e.at) p = e.p; return p; }
static void beat_at(size_t sample) { host_beat = int((sample * 48ull * 120 / 60 / SR) % 48); }   // the OS's counter at 120 bpm

static std::vector<unsigned char> region(sizeof(FxState) + 64);   // one place for both runs: FREEZE seeds its grains from the slot's address

// The old way: fx_run's case on an FxSlot, the knobs in p[], clock_block before GATE.
static void run_old(int kind, std::vector<int> L, std::vector<int> R, const std::vector<Knobs>& k, std::vector<int>& oL, std::vector<int>& oR) {
    std::memset(region.data(), 0, region.size()); g_clk_phase = g_clk_beat = g_clk_inc = 0; g_clk_last = -1;
    FxState& s = *(FxState*)region.data();
    for (size_t b = 0; b < L.size() / FR; ++b) {
        beat_at(b * FR);
        const unsigned short* p = knobs_at(k, double(b * FR) / SR); for (int i = 0; i < 4; ++i) s.f.p[i] = p[i];
        int* l = &L[b * FR]; int* r = &R[b * FR];
        switch (kind) {
        case FX_W_JUNO: fx_juno(s.f, s.mem, l, r); break;
        case FX_W_TAPE: fx_tape_wagon(s.f, s.mem, l, r); break;
        case FX_W_GATE: clock_block(); fx_gate(s.f, l, r); break;
        case FX_W_FREEZE: fx_freeze(s.f, s.mem, l, r); break;
        }
    }
    oL = L; oR = R;
}

// The train's way: the instance built as build_live builds it, the region cleared over the wagon's bytes, the eight
// words value << 16, [2](obj, L, R, ctx) once a block.
static void run_wagon(int kind, std::vector<int> L, std::vector<int> R, const std::vector<Knobs>& k, std::vector<int>& oL, std::vector<int>& oR) {
    const Op1fWagon& w = WAGONS[kind];
    assert(w.magic == OP1F_WAGON_MAGIC && w.bytes <= OP1F_WAGON_STATE_MAX && w.bytes <= region.size());
    std::memset(region.data(), 0, w.bytes); g_clk_phase = g_clk_beat = g_clk_inc = 0; g_clk_last = -1;
    uintptr_t obj[4] = { (uintptr_t)&PLUG_TABLE, (uintptr_t)&w, (uintptr_t)region.data(), 0 };
    ((InitFn)((void**)obj[0])[3])(obj, 0);
    unsigned ctx[8];
    for (size_t b = 0; b < L.size() / FR; ++b) {
        beat_at(b * FR);
        const unsigned short* p = knobs_at(k, double(b * FR) / SR); for (int i = 0; i < 8; ++i) ctx[i] = i < 4 ? unsigned(p[i]) << 16 : 0;
        ((ProcessFn)((void**)obj[0])[2])(obj, &L[b * FR], &R[b * FR], ctx);
    }
    oL = L; oR = R;
}

// ---------------------------------------------------------------- TAPE: old and new, a ladder
static bool read_wav(const std::string& fn, std::vector<int>& L, std::vector<int>& R) {
    FILE* f = std::fopen(fn.c_str(), "rb"); if (!f) return false;
    std::vector<unsigned char> b; unsigned char t[4096]; size_t n; while ((n = std::fread(t, 1, sizeof t, f)) > 0) b.insert(b.end(), t, t + n); std::fclose(f);
    size_t p = 12; int ch = 0, bits = 0;
    while (p + 8 <= b.size()) {
        const uint32_t sz = b[p + 4] | b[p + 5] << 8 | b[p + 6] << 16 | (uint32_t)b[p + 7] << 24;
        if (!std::memcmp(&b[p], "fmt ", 4)) { ch = b[p + 10]; bits = b[p + 22]; }
        if (!std::memcmp(&b[p], "data", 4) && bits == 16 && ch >= 1) {
            const size_t frames = std::min<size_t>(sz, b.size() - p - 8) / (2 * ch) / FR * FR; L.resize(frames); R.resize(frames);
            for (size_t i = 0; i < frames; ++i) { const unsigned char* q = &b[p + 8 + i * 2 * ch];
                L[i] = int(int16_t(q[0] | q[1] << 8)) << 16; R[i] = ch > 1 ? int(int16_t(q[2] | q[3] << 8)) << 16 : L[i]; }
            return true;
        }
        p += 8 + sz + (sz & 1);
    }
    return false;
}
static double rms_db(const std::vector<int>& x) { double a = 0; for (int v : x) a += double(v) * v; return 10 * std::log10(a / x.size() / 4.6116860184273879e18 + 1e-20); }
static void tape_ladder(const std::string& dir, const std::string& name, const std::vector<int>& L, const std::vector<int>& R) {
    wav(dir + "/" + name + "_dry.wav", L, R);
    struct Level { const char* tag; unsigned short p[4]; } levels[] = {
        { "25", { 8192, 8192, 8192, 8192 } }, { "50", { 16384, 16384, 16384, 16384 } }, { "75", { 24576, 24576, 24576, 24576 } },
        { "100", { 32767, 32767, 32767, 32767 } }, { "default", { FX_DEF_TAPE } } };
    std::printf("tape   %-16s dry %6.1f dB rms;", name.c_str(), rms_db(L));
    for (auto& lv : levels) {
        std::vector<int> o[2] = { L, R }, w[2] = { L, R };
        for (int which = 0; which < 2; ++which) {
            std::memset(region.data(), 0, region.size());
            FxState& s = *(FxState*)region.data(); for (int k = 0; k < 4; ++k) s.f.p[k] = lv.p[k];
            std::vector<int>* x = which ? w : o;
            for (size_t b = 0; b < L.size() / FR; ++b) (which ? fx_tape_wagon : fx_tape)(s.f, s.mem, &x[0][b * FR], &x[1][b * FR]);
            wav(dir + "/" + name + "_" + lv.tag + (which ? "_new.wav" : "_old.wav"), x[0], x[1]);
        }
        std::printf("  %s: old %+.1f new %+.1f", lv.tag, rms_db(o[0]) - rms_db(L), rms_db(w[0]) - rms_db(L));
    }
    std::printf(" (dB against dry)\n");
}

static std::string squash(std::string s) { std::string o; for (char c : s) if (c != ' ' && c != '\n' && c != '\t') o += c; return o; }

int main(int argc, char** argv) {
    const std::string out = argc > 1 ? argv[1] : "tools/pack/fx/build";
    {   // fx_juno.h is the old case: the code between its first and last line, spaces aside
        std::ifstream a("tools/patch/mod_cherd.cpp"), b("tools/pack/fx/fx_juno.h"); std::stringstream sa, sb; sa << a.rdbuf(); sb << b.rdbuf();
        std::string m = sa.str(), j = sb.str();
        const size_t s0 = m.find("const int mode = f.p[0] < 10923"), s1 = m.find("++f.pos;\n        }\n        return; }", s0);
        assert(s0 != std::string::npos && s1 != std::string::npos);
        const std::string old_code = squash(m.substr(s0, s1 - s0)), j0 = squash(j);
        assert(j0.find(old_code) != std::string::npos);
        std::printf("juno        fx_juno.h holds mod_cherd.cpp's JUNO case unchanged\n");
    }
    for (int i = 0; i < FX_WAGONS; ++i) for (int k = 0; k < 4; ++k) assert(WAGONS[i].defaults[k] == FX_SPEC[i].defaults[k]);
    for (int i = 0; i < FX_WAGONS; ++i) for (int k = 0; k < 4; ++k) if (FX_SPEC[i].steps[k]) for (int l = 0; l < FX_SPEC[i].steps[k]; ++l) {   // a setting survives a detent's round trip
        const unsigned short v = (unsigned short)(l * 32767 / (FX_SPEC[i].steps[k] - 1));
        assert(fx_step(i, k, fx_step(i, k, v, 1), -1) == v || l == FX_SPEC[i].steps[k] - 1);
    }

    std::vector<Note> line, pad;
    const double mel[] = { 57, 60, 64, 67, 69, 67, 64, 60 };
    for (int i = 0; i < 16; ++i) line.push_back({ 0.25 * i, 0.22, mel[i % 8] });
    for (double m : { 57.0, 60.0, 64.0 }) pad.push_back({ 0.0, 4.0, m });
    struct Case { int kind; const char* dir; const std::vector<Note>* m; std::vector<Knobs> k; const char* what; };
    const unsigned short S = 32767;
    std::vector<Case> cases = {
        { FX_W_JUNO, "juneaux", &line, { { 0, { 0, S, 16384, 16384 } } }, "mode_i" },
        { FX_W_JUNO, "juneaux", &line, { { 0, { 16384, S, 16384, 16384 } } }, "mode_ii" },
        { FX_W_JUNO, "juneaux", &pad, { { 0, { S, S, 16384, 16384 } } }, "mode_i+ii_pad" },
        { FX_W_TAPE, "tape", &pad, { { 0, { 4000, 2500, 3000, 6000 } } }, "1_subtle_pad" },      // default candidates (the old chains' was 13000 9000 9000 16000)
        { FX_W_TAPE, "tape", &line, { { 0, { 4000, 2500, 3000, 6000 } } }, "1_subtle_line" },
        { FX_W_TAPE, "tape", &pad, { { 0, { 6500, 4000, 5000, 9000 } } }, "2_medium_pad" },                 // the default (FX_DEF_TAPE)
        { FX_W_TAPE, "tape", &line, { { 0, { 6500, 4000, 5000, 9000 } } }, "2_medium_line" },
        { FX_W_TAPE, "tape", &pad, { { 0, { 9000, 6000, 7000, 12000 } } }, "3_more_pad" },
        { FX_W_TAPE, "tape", &line, { { 0, { 9000, 6000, 7000, 12000 } } }, "3_more_line" },
        { FX_W_TAPE, "tape", &pad, { { 0, { 13000, 9000, 9000, 16000 } } }, "4_old_default_gnarly_pad" },
        { FX_W_TAPE, "tape", &line, { { 0, { 30000, 20000, 20000, 28000 } } }, "worn_out" },
        { FX_W_GATE, "gate", &pad, { { 0, { FX_DEF_GATE } } }, "1-16_120bpm" },
        { FX_W_GATE, "gate", &pad, { { 0, { 30000, 32767, 16384, 4000 } } }, "trance_120bpm" },
        { FX_W_FREEZE, "freeze", &line, { { 0, { 0, 16384, 24576, 12000 } }, { 1.2, { 24576, 16384, 24576, 12000 } }, { 3.2, { 0, 16384, 24576, 12000 } } }, "hold_1.2s_to_3.2s" },
        { FX_W_FREEZE, "freeze", &line, { { 0, { 0, 27306, 24576, 20000 } }, { 1.0, { 30000, 27306, 24576, 20000 } } }, "hold_up_a_fifth_smeared" },
    };
    for (auto& c : cases) {
        std::vector<int> L, R, oL, oR, wL, wR;
        synth(L, R, 4.5, *c.m);
        run_old(c.kind, L, R, c.k, oL, oR);
        run_wagon(c.kind, L, R, c.k, wL, wR);
        const bool same = oL == wL && oR == wR, moved = oL != L || oR != R;
        std::string d = out + "/" + c.dir;
        wav(d + "/" + std::string(c.what) + "_dry.wav", L, R);
        wav(d + "/" + std::string(c.what) + "_" + c.dir + ".wav", wL, wR);
        size_t stereo = 0; for (size_t i = 0; i < wL.size(); ++i) stereo += wL[i] != wR[i];
        std::printf("%-6s %-24s wagon = reference bit for bit: %s; changes the sound: %s; L != R in %zu of %zu frames\n",
                    c.dir, c.what, same ? "yes" : "NO", moved ? "yes" : "no", stereo, wL.size());
        assert(same && moved);
    }
    if (argc > 2) {
        const std::string ld = argv[2];
        std::vector<int> L, R; synth(L, R, 4.5, pad); tape_ladder(ld, "testsynth_pad", L, R);
        synth(L, R, 4.5, line); tape_ladder(ld, "testsynth_line", L, R);
        for (const char* src : { "oberon_pwmpad", "lilith_softpad", "clarinet_line" })
            if (read_wav(ld + "/src/" + src + ".wav", L, R)) tape_ladder(ld, src, L, R);
    }
    std::printf("ok: WAVs in %s/{juneaux,tape,gate,freeze}/\n", out.c_str());
    return 0;
}
