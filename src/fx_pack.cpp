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

static void* volatile document;                     // the page, once the frame made it
static volatile unsigned* volatile page_params;     // the tapped wagon's values while its page shows
static volatile int page_kind;
static void* jx_document;                       // JUNEAUX's own faceplate (face.py), 0 when it did not load: the shared page then
static volatile u32 changes;

template <int KIND> static void wagon_page(volatile unsigned* params) {
    void* d = KIND == FX_W_JUNO ? jx_document : document;
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
        ((e->key >= (u32)KEY_CLICK_1 && e->key <= (u32)KEY_CLICK_4) || (e->key >= (u32)KEY_PRESS_1 && e->key <= (u32)KEY_PRESS_4))) return 1;   // the knobs are the page's
    return 0;
}

// ---------------------------------------------------------------- the display thread
static char* put(char* p, const char* s) { while (*s) *p++ = *s++; *p = 0; return p; }
static char* put_num(char* p, u32 v) { char d[10]; int n = 0; do { d[n++] = (char)('0' + v % 10); v /= 10; } while (v); while (n) *p++ = d[--n]; *p = 0; return p; }

static struct { u32 asked; void* seg[4][FX_SEGMENTS]; int lit[4]; u32 opened, logged;
                void* jseg[4][FX_SEGMENTS]; int jlit[4]; void* led[2]; void* wave[JX_DEPTHS][JX_FRAMES]; int shown_wave, shown_mode; u32 ticks; } f;

static void make_page() {
    void* d = (FX_ONE == FX_W_JUNO ? 0 : op1f_picture(&FX_CARD, "fxwagon.svg"));
    if (d) {
        char name[12];
        for (int k = 0; k < 4; ++k) for (int s = 0; s < FX_SEGMENTS; ++s) { char* q = put_num(put(name, "fx_b"), (u32)k); q = put(q, "s"); put_num(q, (u32)s); f.seg[k][s] = op1f_node(d, name); }
    }
    void* j = (FX_ONE == FX_W_JUNO ? op1f_picture(&FX_CARD, "juneaux.svg") : 0);
    if (j) {
        char name[12];
        for (int k = 1; k < 4; ++k) for (int s = 0; s < FX_SEGMENTS; ++s) { char* q = put_num(put(name, "fx_b"), (u32)k); q = put(q, "s"); put_num(q, (u32)s); f.jseg[k][s] = op1f_node(j, name); }
        f.led[0] = op1f_node(j, "jx_led1"); f.led[1] = op1f_node(j, "jx_led2");
        for (int a = 0; a < JX_DEPTHS; ++a) for (int b = 0; b < JX_FRAMES; ++b) {
            char* q = put_num(put(name, "jx_w"), (u32)a); q = put(q, "_"); put_num(q, (u32)b);
            f.wave[a][b] = op1f_node(j, name); op1f_show(f.wave[a][b], 0);
        }
    }
    for (int k = 0; k < 4; ++k) f.lit[k] = f.jlit[k] = -1;
    f.shown_wave = f.shown_mode = -1;
    document = d; jx_document = j;
    op1f_log(3, "op1f/" FX_STR(FX_PACK) ".cpp", 1, FX_STR(FX_PACK) ": page %s; wagon offered; cherd %s", (FX_ONE == FX_W_JUNO ? j : d) ? "made" : "NOT made",
             &op1f_cherd_pcs ? "there" : "away");
}

// JUNEAUX's faceplate: the lights over I and II (MODE), one wave group: its depth step by DEPTH, its phase frame moving
// at the speed of the mode (I slow, II faster, I+II fastest, as the chorus's own LFO)
static void dress_juneaux(void* d, volatile unsigned* p) {
    const u16 mode = (u16)(p[0] >> 16), depth = (u16)(p[2] >> 16);
    const int m = mode < 10923 ? 0 : mode < 21845 ? 1 : 2;
    if (m != f.shown_mode) { op1f_show(f.led[0], m != 1); op1f_show(f.led[1], m != 0); f.shown_mode = m; }
    ++f.ticks;
    const u32 every = m == 0 ? 6u : m == 1 ? 3u : 1u;
    const int w = (int)((depth * (JX_DEPTHS - 1) + 16383) / 32767) * JX_FRAMES + (int)(f.ticks / every % JX_FRAMES);
    if (w != f.shown_wave) {
        if (f.shown_wave >= 0) op1f_show(f.wave[f.shown_wave / JX_FRAMES][f.shown_wave % JX_FRAMES], 0);
        op1f_show(f.wave[w / JX_FRAMES][w % JX_FRAMES], 1); f.shown_wave = w;
    }
}

static void dress(void* d) {
    const int kind = page_kind;
    volatile unsigned* p = page_params;
    const FxSpec& sp = FX_SPEC[kind];
    const bool jx = d == jx_document;
    op1f_text(d, "fx_title", sp.name, 200);
    if (jx) dress_juneaux(d, p);
    for (int k = jx ? 1 : 0; k < 4; ++k) {
        const u16 v = (u16)(p[k] >> 16);
        char t[16];
        const char* w = fx_words(kind, k, v, t);
        if (!w) { put(put_num(t, (v * 100u + 16383u) / 32767u), "%"); w = t; }
        op1f_text(d, k == 0 ? "fx_lab0" : k == 1 ? "fx_lab1" : k == 2 ? "fx_lab2" : "fx_lab3", sp.label[k], 120);
        op1f_text(d, k == 0 ? "fx_val0" : k == 1 ? "fx_val1" : k == 2 ? "fx_val2" : "fx_val3", w, 96);
        const int lit = (v * FX_SEGMENTS + 16383) / 32767;
        void** seg = jx ? f.jseg[k] : f.seg[k]; int& was = jx ? f.jlit[k] : f.lit[k];
        if (lit != was) { for (int s = 0; s < FX_SEGMENTS; ++s) if (seg[s]) op1f_show(seg[s], s < lit); was = lit; }
    }
}

extern "C" void fx_frame(const Op1fFrame* now) OP1F_NAMED("fx_frame");
extern "C" void fx_frame(const Op1fFrame* now) {
    if (now->bytes < 36u) return;
    if (!f.asked) { f.asked = 1; make_page(); }
    void* d = page_kind == FX_W_JUNO && jx_document ? jx_document : document;
    if (d && now->page == &FX_CARD && page_params) { dress(d); ++f.opened; }
    if (f.opened && changes != f.logged && (f.opened & 255) == 0) { f.logged = changes; op1f_log(3, "op1f/" FX_STR(FX_PACK) ".cpp", 2, FX_STR(FX_PACK) ": %u knob turns on the page", changes); }
}
