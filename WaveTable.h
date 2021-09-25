#ifndef __WAVETABLE__
#define __WAVETABLE__
// Sample name: Celesta C5 Mini
// Sample's base frequency: 524.9773498620826 Hz
// Sample's sample rate: 32000 Hz

#include <stdint.h>

#define WAVETABLE_LEN 2608
#define WAVETABLE_ATTACK_LEN 1507
#define WAVETABLE_LOOP_LEN 1101

extern __code int8_t WaveTable[];
extern __code uint16_t WaveTable_Increment[];

#endif