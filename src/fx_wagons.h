// fx.pack's wagons, the same on the unit and on the Mac (host_check.cpp): our old chain effects JUNO, TAPE, GATE and
// FREEZE as fx train wagons (op1f_wagon.h). The effects are the old code itself, included as it is:
//   tools/patch/fx_special.cpp   GATE, FREEZE (and CLOUD, CANON, DUCK and the old TAPE, unused here)
//   fx_juno.h                    the JUNO case of mod_cherd.cpp's fx_run, moved into a function unchanged
//   fx_tape_wagon.h              TAPE: the old fx_tape.h with its top made tape rather than fuzz (not the old sound)
// This header gives them what mod_cherd.cpp gave them: FxSlot, mulq15, sat_add32, the tempo (bpm_now, A_BEAT through
// clock_block) and CHERD's chord. The includer defines A_BPM and A_BEAT (addrs.h on the unit) and fx_new_block().
//
// A wagon's state is the old slot: an FxSlot and its 32 KiB line, cleared by the train as fx_set_type cleared them.
// Its first four values are the slot's four knobs (value << 16 = p[k]).
#pragma once

typedef unsigned char u8;
typedef unsigned short u16;
typedef int i32;
typedef unsigned u32;

struct FxSlot { volatile u8 type; volatile u16 p[4]; i32 s[8]; u32 pos, lfo; u32 obj[40]; };   // mod_cherd.cpp's
static inline i32 sat_add32(i32 a, i32 b){ const i32 s = (i32)((u32)a + (u32)b); return ((a ^ s) & (b ^ s)) < 0 ? (a < 0 ? (i32)0x80000000 : 0x7fffffff) : s; }
static inline i32 mulq15(i32 x, i32 q){ return (x >> 15) * q + (((x & 0x7fff) * q) >> 15); }

// The tempo, as mod_cherd.cpp read it: tempo.cpp's BPM float, converted without float math.
static u32 bpm_now(){
    const u32 f = *(volatile u32*)A_BPM, e = (f >> 23) & 0xff, m = (f & 0x7fffff) | 0x800000;
    const u32 v = e >= 127 && e < 150 ? m >> (150 - e) : 120;
    return v >= 20 && v <= 300 ? v : 120;
}

// CHERD's chord, in the words the old effects read (g_root, g_window, g_lowc, g_shown), made from op1f_cherd_pcs's chord
// mask: the window is the mask over pitch classes from C, the root its lowest pitch class (CHERD's own root is not
// offered: FREEZE's FOLLOW moves by the change of that lowest note). The feel page's swing is CHERD's: 0 here.
static u32 g_root, g_window, g_lowc;
static int g_shown, g_swing;
static inline void fx_chord(u32 pcs){
    const u32 chord = (pcs >> 16) & 0xfffu;
    g_shown = chord != 0; g_window = chord; g_lowc = 0;
    u32 r = 0; while (chord && !(chord >> r & 1u)) ++r;
    g_root = r;
}

static u8 g_duck_cnt[24]; static u32 g_duck_any; static i32 g_drum_lvl, g_trk_lvl[4];   // DUCK's sources: DUCK is not a wagon here

#include "fx_special.cpp"
#include "fx_juno.h"
#include "fx_tape_wagon.h"

enum { FX_W_JUNO, FX_W_TAPE, FX_W_GATE, FX_W_FREEZE, FX_WAGONS };
struct FxState { FxSlot f; short mem[16384]; };   // the old slot: its state and its 32 KiB

// One block of a wagon on the train's split bus.
static inline void fx_wagon_block(int kind, void* state, int* L, int* R, const volatile unsigned* params){
    FxState& s = *(FxState*)state;
    for (int k = 0; k < 4; ++k) s.f.p[k] = (u16)(params[k] >> 16);
    switch (kind) {
    case FX_W_JUNO: fx_juno(s.f, s.mem, L, R); break;
    case FX_W_TAPE: fx_tape_wagon(s.f, s.mem, L, R); break;
    case FX_W_GATE: if (fx_new_block()) clock_block(); fx_gate(s.f, L, R); break;
    case FX_W_FREEZE: fx_freeze(s.f, s.mem, L, R); break;
    }
}

// What the page shows: the old chain page's labels (mod_cherd.cpp fx_knob_names), defaults (its pick) and words (fx_ui.cpp
// fx_value). steps: a knob of so many settings (its value moves a setting a detent), 0 for an amount (32 detents).
struct FxSpec { const char* name; const char* label[4]; short defaults[4]; u8 steps[4]; };
#define FX_DEF_JUNO 0, 32767, 16384, 16384           // mode I, the original mix
#define FX_DEF_TAPE 6500, 4000, 5000, 9000          // a gentle tape: half the old chains' wobble and wear (13000 9000 9000 16000)
#define FX_DEF_GATE 26000, 10240, 16384, 8192        // deep, 1/16, half length, soft edges
#define FX_DEF_FREEZE 0, 16384, 24576, 12000         // not holding, no transpose, follow on
static const FxSpec FX_SPEC[FX_WAGONS] = {
    { "juneaux",   { "mode", "mix", "depth", "tone" },          { FX_DEF_JUNO },   { 3, 0, 0, 0 } },
    { "tape",   { "wow", "flutter", "saturate", "age" },     { FX_DEF_TAPE },   { 0, 0, 0, 0 } },
    { "gate",   { "depth", "pattern", "length", "smooth" },  { FX_DEF_GATE },   { 0, 8, 0, 0 } },
    { "freeze", { "hold", "pitch", "follow", "smear" },      { FX_DEF_FREEZE }, { 0, 25, 2, 0 } },
};

// A knob's value in words (fx_ui.cpp fx_value, in the font's small letters), else 0 for an amount (a percentage).
static inline const char* fx_words(int kind, int k, u16 p, char* val){
    if (kind == FX_W_GATE && k == 1) { static const char* const n[8] = {"1/4", "1/8", "1/16", "1/8t", "stab", "chrl", "sync", "trnc"}; return n[gate_pattern(p) & 7]; }   // 4 letters at most: the page's value column holds 4 of the firmware's
    if (kind == FX_W_FREEZE && k == 0 && p <= 1000) return "off";
    if (kind == FX_W_FREEZE && k == 1) { const int st = freeze_semis(p); int j = 0; if (st) val[j++] = st < 0 ? '-' : '+';
        const int a = st < 0 ? -st : st; if (a >= 10) val[j++] = '1'; val[j++] = (char)('0' + (a >= 10 ? a - 10 : a)); val[j] = 0; return val; }
    if (kind == FX_W_FREEZE && k == 2) return p > 16384 ? "on" : "off";
    if (kind == FX_W_JUNO && k == 0) { static const char* const n[3] = {"i", "ii", "i+ii"}; return n[p < 10923 ? 0 : p < 21845 ? 1 : 2]; }
    return 0;
}

// A detent of knob k: the value one setting (or a 32nd) further, held to 0..32767.
static inline u16 fx_step(int kind, int k, u16 p, int by){
    const int n = FX_SPEC[kind].steps[k];
    if (!n) { int v = (int)p + by * 1024; return (u16)(v < 0 ? 0 : v > 32767 ? 32767 : v); }
    int level = ((int)p * (n - 1) + 16383) / 32767 + by;
    level = level < 0 ? 0 : level > n - 1 ? n - 1 : level;
    return (u16)(level * 32767 / (n - 1));
}
