#ifndef __WAVETABLE__
#define __WAVETABLE__
// Sample name: True Vibe C5
// Sample's base frequency: 522.9857411008662 Hz
// Sample's sample rate: 32000 Hz
#define WAVETABLE_LEN 40057
#define WAVETABLE_ATTACK_LEN 39997
#define WAVETABLE_LOOP_LEN 60
#define WAVETABLE_ACTUAL_LEN 40058

#include <stdint.h>
extern __code int8_t WaveTable[WAVETABLE_ACTUAL_LEN];
extern __code uint16_t WaveTable_Increment[];

#endif