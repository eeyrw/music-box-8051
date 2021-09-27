#ifndef __WAVETABLE__
#define __WAVETABLE__
// Sample name: Harp D#5
// Sample's base frequency: 621.8548762513416 Hz
// Sample's sample rate: 32000 Hz
#define WAVETABLE_LEN 13187
#define WAVETABLE_ATTACK_LEN 12621
#define WAVETABLE_LOOP_LEN 566

#include <stdint.h>
extern __code int8_t WaveTable[WAVETABLE_LEN];
extern __code uint16_t WaveTable_Increment[];

#endif