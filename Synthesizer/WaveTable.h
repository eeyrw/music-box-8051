#ifndef __WAVETABLE__
#define __WAVETABLE__
// Sample name: Square Wave C4
// Sample's base frequency: 262.22755246353034 Hz
// Sample's sample rate: 32000 Hz
#define WAVETABLE_LEN 5427
#define WAVETABLE_ATTACK_LEN 5306
#define WAVETABLE_LOOP_LEN 121
#define WAVETABLE_ACTUAL_LEN 5428

#include <stdint.h>
extern __code int8_t WaveTable[WAVETABLE_ACTUAL_LEN];
extern __code uint16_t WaveTable_Increment[];

#endif