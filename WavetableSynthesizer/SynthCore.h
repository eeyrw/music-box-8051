#ifndef __SYNTH_CORE_H__
#define __SYNTH_CORE_H__

#include <stdint.h>
#include "EnvelopeTable.h"

#define POLY_NUM 7
#define DECAY_TIME_FACTOR 200

typedef struct _SoundUnit
{
	uint16_t increment;
	uint8_t wavetablePos_frac;
	uint16_t wavetablePos_int;
	uint8_t envelopeLevel;
	uint8_t envelopePos;
	int16_t val;
	int8_t sampleVal;
	uint16_t tick;
} SoundUnit;

typedef struct _SoundUnitSplit
{
	uint8_t increment_frac;
	uint8_t increment_int;
	uint8_t wavetablePos_frac;
	uint16_t wavetablePos_int;
	uint8_t envelopeLevel;
	uint8_t envelopePos;
	int16_t val;
	int8_t sampleVal;
	uint16_t tick;
} SoundUnitSplit;

typedef union _SoundUnitUnion
{
	SoundUnit combine;
	SoundUnitSplit split;
} SoundUnitUnion;

typedef struct _Synthesizer
{
	SoundUnitUnion SoundUnitUnionList[POLY_NUM];
	int16_t mixOut;
	uint8_t lastSoundUnit;
	uint8_t tempTick;
} Synthesizer;

extern void SynthInit(Synthesizer *synth);

#ifdef RUN_TEST
extern void NoteOnC(uint8_t note);
extern void SynthC(void);
extern void GenDecayEnvlopeC(void);
#endif

extern void NoteOnAsm(uint8_t note);
extern void GenDecayEnvlopeAsm(void);
extern void SynthAsm(void);

#ifdef RUN_TEST
extern Synthesizer synthForC;
#endif

extern __data Synthesizer synthForAsm;

#endif