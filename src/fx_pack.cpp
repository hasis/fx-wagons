// One wagon of fx train (op1f_wagon.h) as a pack of its own, OS 1.7.3+0 with open-field: our old chain effects. Each
// pack's folder (tools/pack/<name>/) holds a two-line source that names its wagon and includes this file:
//   FX_ONE   the wagon (0 JUNEAUX, 1 TAPE, 2 GATE, 3 FREEZE: fx_wagons.h's enumeration)
//   FX_PACK  the pack's name, as its recipe gives it: the wagon is op1f_wagon_<name>, the card op1f_card_<name>
// The four:
//   op1f_wagon_juneaux  JUNEAUX the Juno-60 chorus (its own faceplate, juneaux.svg): MODE (I, II, I+II), MIX, DEPTH, TONE; stereo
//   op1f_wagon_tape     TAPE    a worn tape machine: WOW, FLUTTER, SATURATE, AGE
//   op1f_wagon_gate     GATE    a tempo-locked gate: DEPTH, PATTERN (1/4 .. TRANCE), LENGTH, SMOOTH
//   op1f_wagon_freeze   FREEZE  holds a moment as a pad: HOLD (up from 0 captures, 0 lets go), PITCH, FOLLOW, SMEAR
// The sound is the old code itself (fx_wagons.h): the same numbers the monolithic build ran on 1.7.3. No change of the
// firmware: the pack reads the tempo where the old build did (addrs.h), and CHERD's chord through op1f_cherd_pcs when
// cherd.pack is there.
//
// A tap on the wagon opens its pack's page (level 3 of the base, a picture of its own): the effect's name, and for
// each knob in encoder order its label, its value and a bar. The four knobs turn the wagon's four values while the page
// shows (the train keeps them in its file); T3 goes back to the train.
//
// Threads: the wagons' process on the audio thread; their page calls and fx_input on the key thread; fx_frame on the
// display thread. They share single words.
#include "build_m3_card.h"
#include "addrs.h"
#include "op1f_wagon.h"
#include "wagon_art.h"

typedef unsigned int u32;

extern "C" {   // libgcc's division, which the pack has no copy of (FREEZE and GATE divide by variables)
u32 fx_udiv(u32 a, u32 b) asm("___udivsi3");
u32 fx_udiv(u32 a, u32 b) { u32 q = 0, r = 0; for (int i = 31; i >= 0; --i) { r = (r << 1) | ((a >> i) & 1u); if (r >= b) { r -= b; q |= 1u << i; } } return q; }
u32 fx_umod(u32 a, u32 b) asm("___umodsi3");
u32 fx_umod(u32 a, u32 b) { return a - fx_udiv(a, b) * b; }
int fx_div(int a, int b) asm("___divsi3");
int fx_div(int a, int b) { const int neg = (a < 0) ^ (b < 0); const u32 q = fx_udiv(a < 0 ? -(u32)a : (u32)a, b < 0 ? -(u32)b : (u32)b); return neg ? -(int)q : (int)q; }
int fx_mod(int a, int b) asm("___modsi3");
int fx_mod(int a, int b) { return a - fx_div(a, b) * b; }
}

// GATE's clock advances once a block: the first GATE wagon of a block moves it (the wagons of one block run within a
// few hundred thousand cycles; a block is 580,500). ponytail: a cycle gap, a block counter of the train's if one is offered.
static u32 clock_cycles;
static int fx_new_block() {
    u32 c; asm volatile("%0 = CYCLES;" : "=d"(c));
    if (c - clock_cycles < 290000u) return 0;
    clock_cycles = c;
    return 1;
}

#include "fx_wagons.h"
#include "fx_face.h"

#if !defined(FX_ONE) || !defined(FX_PACK)
#error "a wagon pack's source defines FX_ONE and FX_PACK before it includes fx_pack.cpp"
#endif
#define FX_CAT_(a, b) a##b
#define FX_CAT(a, b) FX_CAT_(a, b)
#define FX_STR_(a) #a
#define FX_STR(a) FX_STR_(a)
#define FX_CARD FX_CAT(op1f_card_, FX_PACK)
#define FX_SELF FX_CAT(op1f_wagon_, FX_PACK)
extern "C" const Op1fCard FX_CARD OP1F_NAMED(FX_STR(FX_CARD));
extern "C" unsigned __attribute__((weak)) op1f_cherd_pcs() asm("op1f_cherd_pcs");   // cherd.pack: chord bits 16..27

// ---------------------------------------------------------------- the wagons
template <int KIND> static void wagon_process(void* state, int* left, int* right, const volatile unsigned* params) {
    fx_chord(&op1f_cherd_pcs ? op1f_cherd_pcs() : 0u);
    fx_wagon_block(KIND, state, left, right, params);
}

static void* volatile document;                     // the wagon's faceplate (face.py), once the frame made it
static volatile unsigned* volatile page_params;     // the tapped wagon's values while its page shows
static volatile int page_kind;
static volatile u32 changes;

template <int KIND> static void wagon_page(volatile unsigned* params) {
    void* d = document;
    if (d == 0) return;
    page_kind = KIND;
    page_params = params;
    op1f_page_open(&FX_CARD, d);
}

#define FX_WAGON(NAME, KIND, DEFAULTS, ART, COST) { OP1F_WAGON_MAGIC, OP1F_WAGON_VERSION, NAME, sizeof(FxState), COST, { DEFAULTS, 0, 0, 0, 0 }, \
    0, wagon_process<KIND>, wagon_page<KIND>, ART }   // constants only: a pack runs no constructors
// the costs: estimates from the code's work a block (the train's log has the measured cycles of each wagon)
// per wagon: its defaults, its drawing (wagon_art.h) and its cost (an estimate; the train's log has the measured cycles)
#define FX_DEFS_0 FX_DEF_JUNO
#define FX_DEFS_1 FX_DEF_TAPE
#define FX_DEFS_2 FX_DEF_GATE
#define FX_DEFS_3 FX_DEF_FREEZE
#define FX_ART_0 WAGON_ART_JUNEAUX
#define FX_ART_1 WAGON_ART_TAPE
#define FX_ART_2 WAGON_ART_GATE
#define FX_ART_3 WAGON_ART_FREEZE
#define FX_COST_0 20000
#define FX_COST_1 25000
#define FX_COST_2 30000
#define FX_COST_3 30000
extern "C" const Op1fWagon FX_SELF OP1F_NAMED(FX_STR(FX_SELF)) =
    FX_WAGON(FX_STR(FX_PACK), FX_ONE, FX_CAT(FX_DEFS_, FX_ONE), FX_CAT(FX_ART_, FX_ONE), FX_CAT(FX_COST_, FX_ONE));

// ---------------------------------------------------------------- the key thread
enum { INPUT_LEVEL_2 = 36, KEY_CLICK_1 = 54, KEY_CLICK_4 = 57, KEY_PRESS_1 = 61, KEY_PRESS_4 = 64 };

extern "C" int fx_input(const Op1fInput* e) OP1F_NAMED("fx_input");
extern "C" int fx_input(const Op1fInput* e) {
    if (e->bytes < (u32)INPUT_LEVEL_2 || !e->mine) return 0;
    if (e->what == (u32)OP1F_UP) return 1;           // the up of a key whose down this pack took
    volatile unsigned* p = page_params;
    if (e->what == (u32)OP1F_TURN && e->key < 4 && p) {
        const int by = e->turn >> 10;                // 1024 a detent
        if (by) { p[e->key] = (u32)fx_step(page_kind, (int)e->key, (u16)(p[e->key] >> 16), by) << 16; ++changes; }
        return 1;
    }
    if ((e->what == (u32)OP1F_DOWN || e->what == (u32)OP1F_DOWN_SHIFT) &&
        ((e->key > (u32)KEY_CLICK_1 && e->key <= (u32)KEY_CLICK_4) || (e->key >= (u32)KEY_PRESS_1 && e->key <= (u32)KEY_PRESS_4))) return 1;   // the knobs are the page's
    return 0;                                        // but the 1st knob's click: the train's, which closes the page (fx train 12, tape train 6)
}

// ---------------------------------------------------------------- the display thread
// Each wagon's faceplate (face.py: juneaux.svg, tape.svg, gate.svg, freeze.svg): a drawing whose animation is one of a
// set of prebuilt groups shown a display frame (op1f_show), and the four knobs as rows (label, value, bar of segments).
// Nothing here divides: steps by multiply and shift, digits by subtraction, frames by counters and masks.
static char* put(char* p, const char* s) { while (*s) *p++ = *s++; *p = 0; return p; }
static char* put_num(char* p, u32 v) {
    static const u32 TEN[] = { 1000000000u, 100000000u, 10000000u, 1000000u, 100000u, 10000u, 1000u, 100u, 10u, 1u };
    int started = 0;
    for (int i = 0; i < 10; ++i) { char c = '0'; while (v >= TEN[i]) { v -= TEN[i]; ++c; } if (c != '0' || started || i == 9) { *p++ = c; started = 1; } }
    *p = 0; return p;
}
static void* node(void* d, const char* pre, int a, const char* sep = 0, int b = 0) {   // "pre<a>" or "pre<a><sep><b>"
    char n[24]; char* q = put_num(put(n, pre), (u32)a);
    if (sep) put_num(put(q, sep), (u32)b);
    return op1f_node(d, n);
}
static void* hidden(void* d, const char* pre, int a, const char* sep = 0, int b = 0) { void* g = node(d, pre, a, sep, b); if (g) op1f_show(g, 0); return g; }
static void show(void* g, int on) { if (g) op1f_show(g, on); }
static void swap(void* const* g, int& was, int now) {   // one group of a set shown (-1: none)
    if (now == was) return;
    if (was >= 0) show(g[was], 0);
    if (now >= 0) show(g[now], 1);
    was = now;
}
static inline int lvl(u32 v, u32 n) { return (int)((v * n + 16384u) >> 15); }   // a knob's 0..32767 as a step 0..n

static_assert(JX_FRAMES == 8 && TP_SPOKES == 8 && TP_WOW == 8 && TP_FLUT == 4 && GT_HEADS == 64 && GT_SLOTS == 28 &&
              FZ_TURNS == 8 && FZ_SMEAR == 4 && FZ_PITCHES == 25 && FZ_SIZES == 4, "face.py and the masks below agree");

static struct { u32 asked, opened, logged, t; void* seg[4][FX_SEGMENTS]; int lit[4];
#if FX_ONE == 0
    void* led[2]; void* wave[JX_DEPTHS * JX_FRAMES]; int wave_was, mode_was; u32 ticks, phase;
#elif FX_ONE == 1
    void* spoke[TP_SPOKES]; void* wow[4 * TP_WOW]; void* flut[4 * TP_FLUT]; void* head[4]; void* speck[TP_SPECKS];
    int spoke_was, wow_was, flut_was, head_was, specks_was; u32 ph, rnd;
#elif FX_ONE == 2
    void* lane[8 * 4 * 2]; void* doors[8]; void* shut[GT_SLOTS]; void* open[GT_SLOTS * GT_DEPTHS]; void* head[GT_HEADS]; void* floor[4];
    int lane_was, doors_was, head_was, floor_was, pat_was, open_was, cur_was;
#else
    void* off; void* flake[FZ_SIZES * FZ_TURNS]; void* smear[3 * FZ_SMEAR]; void* pitch[FZ_PITCHES]; void* link[2];
    int off_was, flake_was, smear_was, pitch_was, link_was; u32 ticks, turn;
#endif
} f;

#define FX_PIC_0 "juneaux.svg"
#define FX_PIC_1 "tape.svg"
#define FX_PIC_2 "gate.svg"
#define FX_PIC_3 "freeze.svg"

static void make_page() {
    void* d = op1f_picture(&FX_CARD, FX_CAT(FX_PIC_, FX_ONE));
    if (d) {
        for (int k = 0; k < 4; ++k) for (int s = 0; s < FX_SEGMENTS; ++s) f.seg[k][s] = node(d, "fx_b", k, "s", s);   // JUNEAUX: rows 1..3 (0 answers 0)
#if FX_ONE == 0
        f.led[0] = node(d, "jx_led", 1); f.led[1] = node(d, "jx_led", 2);
        for (int a = 0; a < JX_DEPTHS; ++a) for (int b = 0; b < JX_FRAMES; ++b) f.wave[a * JX_FRAMES + b] = hidden(d, "jx_w", a, "_", b);
#elif FX_ONE == 1
        for (int i = 0; i < TP_SPOKES; ++i) f.spoke[i] = hidden(d, "tp_r", i);
        for (int a = 0; a < 4; ++a) {
            for (int b = 0; b < TP_WOW; ++b) f.wow[a * TP_WOW + b] = hidden(d, "tp_w", a, "_", b);
            for (int b = 0; b < TP_FLUT; ++b) f.flut[a * TP_FLUT + b] = hidden(d, "tp_f", a, "_", b);
            f.head[a] = hidden(d, "tp_h", a);
        }
        for (int i = 0; i < TP_SPECKS; ++i) f.speck[i] = hidden(d, "tp_a", i);
#elif FX_ONE == 2
        for (int p = 0; p < 8; ++p) {
            for (int l = 0; l < 4; ++l) for (int s = 0; s < 2; ++s) f.lane[(p * 4 + l) * 2 + s] = hidden(d, "gt_l", p * 4 + l, "_", s);   // pattern x length, smooth
            f.doors[p] = hidden(d, "gt_d", p);
        }
        for (int i = 0; i < GT_SLOTS; ++i) { f.shut[i] = hidden(d, "gt_c", i); for (int q = 0; q < GT_DEPTHS; ++q) f.open[i * GT_DEPTHS + q] = hidden(d, "gt_o", i, "_", q); }
        for (int i = 0; i < GT_HEADS; ++i) f.head[i] = hidden(d, "gt_p", i);
        for (int i = 0; i < 4; ++i) f.floor[i] = hidden(d, "gt_f", i);
#else
        f.off = op1f_node(d, "fz_off"); show(f.off, 0);
        for (int a = 0; a < FZ_SIZES; ++a) for (int b = 0; b < FZ_TURNS; ++b) f.flake[a * FZ_TURNS + b] = hidden(d, "fz_h", a, "_", b);
        for (int a = 0; a < 3; ++a) for (int b = 0; b < FZ_SMEAR; ++b) f.smear[a * FZ_SMEAR + b] = hidden(d, "fz_s", a + 1, "_", b);
        for (int i = 0; i < FZ_PITCHES; ++i) f.pitch[i] = hidden(d, "fz_p", i);
        f.link[0] = hidden(d, "fz_l", 0); f.link[1] = hidden(d, "fz_l", 1);
#endif
    }
    for (int k = 0; k < 4; ++k) f.lit[k] = -1;
#if FX_ONE == 0
    f.wave_was = f.mode_was = -1;
#elif FX_ONE == 1
    f.spoke_was = f.wow_was = f.flut_was = f.head_was = f.specks_was = -1;
#elif FX_ONE == 2
    f.lane_was = f.doors_was = f.head_was = f.floor_was = f.pat_was = f.open_was = f.cur_was = -1;
#else
    f.off_was = f.flake_was = f.smear_was = f.pitch_was = f.link_was = -1;
#endif
    document = d;
    op1f_log(3, "op1f/" FX_STR(FX_PACK) ".cpp", 1, FX_STR(FX_PACK) ": page %s; wagon offered; cherd %s", d ? "made" : "NOT made",
             &op1f_cherd_pcs ? "there" : "away");
}

#if FX_ONE == 0
// JUNEAUX: the lights over I and II (MODE), one wave group: its depth step by DEPTH, its phase frame moving at the speed
// of the mode (I slow, II faster, I+II fastest, as the chorus's own LFO)
static void dress_wagon(const u32* v) {
    const int m = v[0] < 10923 ? 0 : v[0] < 21845 ? 1 : 2;
    if (m != f.mode_was) { show(f.led[0], m != 1); show(f.led[1], m != 0); f.mode_was = m; }
    if (++f.ticks >= (m == 0 ? 6u : m == 1 ? 3u : 1u)) { f.ticks = 0; f.phase = (f.phase + 1) & (JX_FRAMES - 1); }
    swap(f.wave, f.wave_was, lvl(v[2], JX_DEPTHS - 1) * JX_FRAMES + (int)f.phase);
}
#elif FX_ONE == 1
// TAPE: the reels turn, their speed swaying with WOW (a slow triangle) and trembling with FLUTTER (a random step); the
// side spans sway in step with the reels (WOW's depth), the tape across the head trembles (FLUTTER's depth), the head
// glows (SATURATE's step), specks of wear (AGE: more of them)
static void dress_wagon(const u32* v) {
    const int tw = (int)(f.t & 255), tri = tw < 128 ? tw - 64 : 191 - tw;             // -64..63, 256 frames a sway
    f.rnd = f.rnd * 1103515245u + 12345u;
    int speed = 48 + ((tri * (int)(v[0] >> 9)) >> 7) + ((((int)(f.rnd >> 24) - 128) * (int)(v[1] >> 10)) >> 8);
    if (speed < 4) speed = 4;
    f.ph += (u32)speed;
    swap(f.spoke, f.spoke_was, (int)((f.ph >> 7) & (TP_SPOKES - 1)));
    swap(f.wow, f.wow_was, lvl(v[0], 3) * TP_WOW + (int)((f.t >> 5) & (TP_WOW - 1)));
    swap(f.flut, f.flut_was, lvl(v[1], 3) * TP_FLUT + (int)((f.t >> 1) & (TP_FLUT - 1)));
    swap(f.head, f.head_was, lvl(v[2], 3));
    const int n = lvl(v[3], TP_SPECKS);
    if (n != f.specks_was) { for (int i = 0; i < TP_SPECKS; ++i) show(f.speck[i], i < n); f.specks_was = n; }
}
#elif FX_ONE == 2
// GATE: the bar's openings (PATTERN x LENGTH x SMOOTH), the level it shuts to (DEPTH), the pattern's doors, the playhead
// from GATE's own clock (g_clk_beat, g_clk_phase: the wagon's clock_block on the audio thread), and the door of the hit
// the gate is open for now swung by DEPTH (at no depth the gate never shuts: no door moves). As fx_gate, without swing.
static void dress_wagon(const u32* v) {
    const int pat = gate_pattern((u16)v[1]), dq = lvl(v[0], 3);
    swap(f.lane, f.lane_was, (pat * 4 + lvl(v[2], 3)) * 2 + lvl(v[3], 1));
    swap(f.floor, f.floor_was, dq);
    const u32 ph = g_clk_phase, bt = g_clk_beat & 3, len = 3277u + (v[2] * 29490u >> 15);
    swap(f.head, f.head_was, (int)((bt << 4) | (ph >> 28)));
    int cur = -1;
    if (pat == 3) { const u32 pos = bt * 96u + (((ph >> 16) * 96u) >> 16); if ((pos & 31u) < (32u * len >> 15)) cur = 16 + (int)(pos >> 5); }   // 12 a bar
    else {
        const u32 mask = kGateMask[pat], pos = (bt << 6) | (ph >> 26), step = pos >> 4;                  // 256ths of the bar
        for (u32 back = 0; back < 16; ++back) {
            const u32 st = (step + 16 - back) & 15; if (!(mask >> st & 1)) continue;
            u32 span = 16; for (u32 fw = 1; fw <= 16; ++fw) if (mask >> ((st + fw) & 15) & 1) { span = fw; break; }
            if (((pos - (st << 4)) & 255u) < ((span << 4) * len >> 15)) cur = (int)st;
            break;
        }
    }
    if (pat != f.pat_was) {
        if (f.open_was >= 0) show(f.open[f.open_was], 0);
        f.open_was = f.cur_was = -1;
        swap(f.doors, f.doors_was, pat);
        for (int s = 0; s < GT_SLOTS; ++s) show(f.shut[s], pat == 3 ? s >= 16 : s < 16 && (kGateMask[pat] >> s & 1));
        f.pat_was = pat;
    }
    const int o = cur < 0 || dq == 0 ? -1 : cur * GT_DEPTHS + dq - 1;
    if (o != f.open_was) {
        if (f.open_was >= 0) { show(f.open[f.open_was], 0); show(f.shut[f.cur_was], 1); }
        if (o >= 0) { show(f.open[o], 1); show(f.shut[cur], 0); }
        f.open_was = o; f.cur_was = cur;
    }
}
#else
// FREEZE: off, a small dim flake; holding, a flake as big as HOLD turning slowly; PITCH's marker on the ruler, FOLLOW's
// link joined or apart, SMEAR's streaks drifting
static void dress_wagon(const u32* v) {
    int fl = -1;
    if (v[0] > 1000) {
        if (++f.ticks >= 6) { f.ticks = 0; f.turn = (f.turn + 1) & (FZ_TURNS - 1); }
        const int size = (int)(v[0] * FZ_SIZES >> 15);
        fl = (size > FZ_SIZES - 1 ? FZ_SIZES - 1 : size) * FZ_TURNS + (int)f.turn;
    }
    if ((fl < 0) != f.off_was) { show(f.off, fl < 0); f.off_was = fl < 0; }
    swap(f.flake, f.flake_was, fl);
    swap(f.pitch, f.pitch_was, (int)((v[1] * 24u + 16383u) >> 15));                   // freeze_semis + 12
    swap(f.link, f.link_was, v[2] > 16384);
    const int l = lvl(v[3], 3);
    swap(f.smear, f.smear_was, l ? (l - 1) * FZ_SMEAR + (int)((f.t >> 2) & (FZ_SMEAR - 1)) : -1);
}
#endif

static void dress(void* d) {
    const int kind = FX_ONE;                          // the pack's one wagon (page_kind): its words fold at compile time
    volatile unsigned* p = page_params;
    const FxSpec& sp = FX_SPEC[kind];
    u32 v[4];
    for (int k = 0; k < 4; ++k) v[k] = p[k] >> 16;
    op1f_text(d, "fx_title", sp.name, 200);
    ++f.t;
    dress_wagon(v);
    static const char* const LAB[4] = { "fx_lab0", "fx_lab1", "fx_lab2", "fx_lab3" }, * const VAL[4] = { "fx_val0", "fx_val1", "fx_val2", "fx_val3" };
    for (int k = FX_ONE == FX_W_JUNO ? 1 : 0; k < 4; ++k) {                         // JUNEAUX's MODE is its two lights
        char t[16];
        const char* w = fx_words(kind, k, (u16)v[k], t);
        if (!w) { put(put_num(t, (v[k] * 100u + 16384u) >> 15), "%"); w = t; }
        op1f_text(d, LAB[k], sp.label[k], 120);
        op1f_text(d, VAL[k], w, 96);
        const int lit = lvl(v[k], FX_SEGMENTS);
        if (lit != f.lit[k]) { for (int s = 0; s < FX_SEGMENTS; ++s) show(f.seg[k][s], s < lit); f.lit[k] = lit; }
    }
}

extern "C" void fx_frame(const Op1fFrame* now) OP1F_NAMED("fx_frame");
extern "C" void fx_frame(const Op1fFrame* now) {
    if (now->bytes < 36u) return;
    if (!f.asked) { f.asked = 1; make_page(); }
    void* d = document;
    if (d && now->page == &FX_CARD && page_params) { dress(d); ++f.opened; }
    if (f.opened && changes != f.logged && (f.opened & 255) == 0) { f.logged = changes; op1f_log(3, "op1f/" FX_STR(FX_PACK) ".cpp", 2, FX_STR(FX_PACK) ": %u knob turns on the page", changes); }
}
