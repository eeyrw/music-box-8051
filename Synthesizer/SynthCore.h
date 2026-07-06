#ifndef __SYNTH_CORE_H__
#define __SYNTH_CORE_H__

#include <stdint.h>
#include "EnvelopeTable.h"

#define POLY_NUM 8

typedef struct _SoundUnit
{
	uint16_t increment;
	uint8_t wavetablePos_frac;
	uint16_t wavetablePos_int;
	uint8_t envelopeLevel;
	uint8_t envelopePos;
	int16_t val;
	int8_t sampleVal;
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
} Synthesizer;

typedef struct _VoiceState
{
	uint8_t midiNote;
	uint8_t velocity;
} VoiceState;

extern __xdata VoiceState voiceState[POLY_NUM];

extern void SynthInit(Synthesizer *synth);

#ifdef RUN_TEST
extern void NoteOnC(uint8_t note);
extern void NoteOffC(uint8_t note);
extern void SynthC(void);
extern void GenDecayEnvlopeC(void);
#endif

extern void NoteOnAsm(uint8_t note, uint8_t velocity);
extern void NoteOffAsm(uint8_t note);
extern void GenDecayEnvlopeAsm(void);
extern void SynthReleaseAllAsm(void);
extern void SynthAsm(void);

#ifdef RUN_TEST
extern Synthesizer synthForC;
#endif

extern __data Synthesizer synthForAsm;

#endif
