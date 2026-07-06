#ifndef __WAVETABLE__
#define __WAVETABLE__
// Sample name: Celesta C5(L)
// Sample's base frequency: 523.2879013149699 Hz
// Sample's sample rate: 32000 Hz
#define WAVETABLE_LEN 21869
#define WAVETABLE_ATTACK_LEN 21260
#define WAVETABLE_LOOP_LEN 609
#define WAVETABLE_ACTUAL_LEN 21870

#include <stdint.h>
extern __code int8_t WaveTable[WAVETABLE_ACTUAL_LEN];
extern __code uint16_t WaveTable_Increment[];

#endif