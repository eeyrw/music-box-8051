#ifndef __WAVETABLE__
#define __WAVETABLE__
// Sample name: Square Wave C5
// Sample's base frequency: 524.4774968507006 Hz
// Sample's sample rate: 32000 Hz
#define WAVETABLE_LEN 1389
#define WAVETABLE_ATTACK_LEN 1328
#define WAVETABLE_LOOP_LEN 61
#define WAVETABLE_ACTUAL_LEN 1390

#include <stdint.h>
extern __code int8_t WaveTable[WAVETABLE_ACTUAL_LEN];
extern __code uint16_t WaveTable_Increment[];

#endif