// A wagon of the effect train offered by another pack: the interface between the pack `fx train` and a pack that
// brings an effect of its own. A pack that offers one includes this header and nothing else of fx train's.
//
// The pack offers an Op1fWagon under the name op1f_wagon_<name> for each effect it brings, and fx train wants each
// name as an optional one (module/build_h_chain.cpp, OFFERED: one line a wagon). A wagon whose pack is away, or whose table is not of this
// version, is not offered, and fx train runs as it always did.
//
// What fx train does with it:
//   - it is one more effect at the end of every knob's list on the train, picked by a turn like any other;
//   - its label on the train is its name, written in the firmware's letters; its picture there is its art (version 2),
//     else the empty wagon's tumbleweed;
//   - each wagon that carries it has an instance of its own: the bytes of state it asks for, cleared, handed to init()
//     and then to every process() of that instance. One instance for each knob of each host at most (eight), built and
//     thrown away as the knob's pick and the sound slot change; the state is the train's memory, never the pack's;
//   - a tap on its wagon calls page(), and nothing when it has none;
//   - on that page the pack's input function leaves the 1st knob's click (the key 54, plain and with shift) alone:
//     the train takes it and closes the page, as T3 does on a sound and the mixer key on a tape track (fx train 12,
//     tape train 6). The other clicks and the presses are the page's;
//   - it never stands under the lamp: the firmware's own slot cannot carry it, so the lamp's knob does not offer it,
//     and its values have no page of the firmware's;
//   - the chain file and the presets keep it by its name: its first four characters stand where an effect's number
//     stands. An fx train that does not know it reads an empty wagon there, and so does this one when its pack is away.
//
// Values: eight words, value << 16, each value 0..32767, as the firmware's effects read theirs. The train keeps them
// in its file; the knobs 1 to 4 of page() write the first four.
//
// Threads: process() runs on the audio thread: no log, no file, nothing that waits, nothing of the firmware's. init()
// runs on the display thread, before the instance's first process(). page() runs on the thread that handles keys,
// as a card's input function does (docs/base.md, "The threads").
#ifndef OP1F_WAGON_H
#define OP1F_WAGON_H

enum { OP1F_WAGON_MAGIC = 0x4e474157 /* "WAGN" */, OP1F_WAGON_VERSION = 2, OP1F_WAGON_STATE_MAX = 512 * 1024, OP1F_WAGON_ART_MAX = 4096 };

struct Op1fWagon {
    unsigned magic;              // OP1F_WAGON_MAGIC
    unsigned version;            // OP1F_WAGON_VERSION; a table of version 1 ends before art
    const char* name;            // its label and what the files keep: small letters and digits, eight at most; the first four tell it from every other wagon
    unsigned bytes;              // the state of one instance, at most OP1F_WAGON_STATE_MAX
    unsigned cost;               // cycles one process() takes at most: the train holds a chain under its ceiling of 200,000
    short defaults[8];           // a new wagon's values, 0..32767
    void (*init)(void* state);   // the state cleared, before the first process(); may be 0
    // one block, in place: left and right are 64 samples each, Q31; params the instance's eight words
    void (*process)(void* state, int* left, int* right, const volatile unsigned* params);
    // a tap on the wagon: params are its eight words, which the pack's page may write (value << 16) while it shows; may be 0
    void (*page)(volatile unsigned* params);
    // version 2: the wagon's picture on the train, or 0: SVG elements, no ids, at most OP1F_WAGON_ART_MAX bytes, drawn
    // as the train's own characters are (module/build_h_train.cpp): in the frame of a wagon at x 0, between x 4 and 66,
    // standing on y 144, line art in stroke 1.3. The base adds it to the train's picture four times as it is and four
    // times dim, for a parked wagon (every colour the dim grey, the near-black fills black); art the picture's reader
    // refuses leaves the train as it was, with the tumbleweed for this wagon
    const char* art;
};

#endif
