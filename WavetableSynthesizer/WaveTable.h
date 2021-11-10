#ifndef __WAVETABLE__
#define __WAVETABLE__
// Sample name: CP-80 EP C4
// Sample's base frequency: 262.24329160743173 Hz
// Sample's sample rate: 32000 Hz
#define WAVETABLE_LEN 8338
#define WAVETABLE_ATTACK_LEN 8216
#define WAVETABLE_LOOP_LEN 122
#define WAVETABLE_ACTUAL_LEN 8339

#include <stdint.h>
extern __code int8_t WaveTable[WAVETABLE_ACTUAL_LEN];
extern __code uint16_t WaveTable_Increment[];

#endif