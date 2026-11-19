// The two firmware places the wagons read, OS 1.7.3+0 of the OP-1 Field (addresses only, no bytes of the firmware).
#pragma once
#define A_BPM  0x8490c8fc   // the tempo, a float
#define A_BEAT 0x8490c908   // the beat counter (wraps 47 -> 0)
