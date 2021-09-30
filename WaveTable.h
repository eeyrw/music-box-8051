#ifndef __WAVETABLE__
#define __WAVETABLE__
// Sample name: CP-80 EP C4
// Sample's base frequency: 262.2347666207412 Hz
// Sample's sample rate: 32000 Hz
#define WAVETABLE_LEN 8461
#define WAVETABLE_ATTACK_LEN 8340
#define WAVETABLE_LOOP_LEN 121

#include <stdint.h>
extern __code int8_t WaveTable[WAVETABLE_LEN];
extern __code uint16_t WaveTable_Increment[];

#endif