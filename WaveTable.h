#ifndef __WAVETABLE__
#define __WAVETABLE__
// Sample name: Square Wave C5
// Sample's base frequency: 524.5767916408392 Hz
// Sample's sample rate: 32000 Hz
#define WAVETABLE_LEN 4557
#define WAVETABLE_ATTACK_LEN 4069
#define WAVETABLE_LOOP_LEN 488

#include <stdint.h>
extern __code int8_t WaveTable[WAVETABLE_LEN];
extern __code uint16_t WaveTable_Increment[];

#endif