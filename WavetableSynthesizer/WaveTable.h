#ifndef __WAVETABLE__
#define __WAVETABLE__
// Sample name: Harp D#5
// Sample's base frequency: 619.461615976171 Hz
// Sample's sample rate: 32000 Hz
#define WAVETABLE_LEN 2302
#define WAVETABLE_ATTACK_LEN 2251
#define WAVETABLE_LOOP_LEN 51
#define WAVETABLE_ACTUAL_LEN 2303

#include <stdint.h>
extern __code int8_t WaveTable[WAVETABLE_ACTUAL_LEN];
extern __code uint16_t WaveTable_Increment[];

#endif