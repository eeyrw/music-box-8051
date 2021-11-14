#include "SynthCore.h"
#include <stdint.h>
#include <stdio.h>
#include "WaveTable.h"

#ifdef RUN_TEST
Synthesizer synthForC;
#endif

void SynthInit(Synthesizer *synth)
{
	SoundUnitUnion *soundUnionList = &(synth->SoundUnitUnionList[0]);
	for (uint8_t i = 0; i < POLY_NUM; i++)
	{
		soundUnionList[i].combine.increment = 0;
		soundUnionList[i].combine.wavetablePos_frac = 0;
		soundUnionList[i].combine.wavetablePos_int = 0;
		soundUnionList[i].combine.envelopeLevel = 0;
		soundUnionList[i].combine.envelopePos = 0;
		soundUnionList[i].combine.val = 0;
		soundUnionList[i].combine.tick = 0;
	}
	synth->lastSoundUnit = 0;
	synth->mixOut = 0;
	synth->tempTick = 0;
}
#ifdef RUN_TEST
void NoteOnAsmP(uint8_t note)
{
	uint8_t lastSoundUnit = synthForAsm.lastSoundUnit;

	// disable_interrupts();
	synthForAsm.SoundUnitUnionList[lastSoundUnit].combine.increment = WaveTable_Increment[note & 0x7F];
	synthForAsm.SoundUnitUnionList[lastSoundUnit].combine.wavetablePos_frac = 0;
	synthForAsm.SoundUnitUnionList[lastSoundUnit].combine.wavetablePos_int = 0;
	synthForAsm.SoundUnitUnionList[lastSoundUnit].combine.envelopePos = 0;
	synthForAsm.SoundUnitUnionList[lastSoundUnit].combine.envelopeLevel = 255;
	synthForAsm.SoundUnitUnionList[lastSoundUnit].combine.combine.tick = 0;
	// enable_interrupts();

	lastSoundUnit++;

	if (lastSoundUnit == POLY_NUM)
		lastSoundUnit = 0;

	synthForAsm.lastSoundUnit = lastSoundUnit;
}

void GenDecayEnvlopeAsmP(void)
{
	__data SoundUnitUnion *soundUnionList = &(synthForAsm.SoundUnitUnionList[0]);
	for (uint8_t i = 0; i < POLY_NUM; i++)
	{
		if (soundUnionList[i].split.wavetablePos_int >= WAVETABLE_ATTACK_LEN &&
			(soundUnionList[i].split.envelopePos < (sizeof(EnvelopeTable) - 1)))
		{
			uint8_t p = soundUnionList[i].split.envelopePos;
			soundUnionList[i].split.envelopeLevel = EnvelopeTable[p];
			soundUnionList[i].split.envelopePos += 1;
		}
	}
}
void NoteOnC(uint8_t note)
{
	uint8_t lastSoundUnit = synthForC.lastSoundUnit;

	// disable_interrupts();
	synthForC.SoundUnitUnionList[lastSoundUnit].combine.increment = WaveTable_Increment[note & 0x7F];
	synthForC.SoundUnitUnionList[lastSoundUnit].combine.wavetablePos_frac = 0;
	synthForC.SoundUnitUnionList[lastSoundUnit].combine.wavetablePos_int = 0;
	synthForC.SoundUnitUnionList[lastSoundUnit].combine.envelopePos = 0;
	synthForC.SoundUnitUnionList[lastSoundUnit].combine.envelopeLevel = 255;
	synthForC.SoundUnitUnionList[lastSoundUnit].combine.combine.tick = 0;
	// enable_interrupts();

	lastSoundUnit++;

	if (lastSoundUnit == POLY_NUM)
		lastSoundUnit = 0;

	synthForC.lastSoundUnit = lastSoundUnit;
}

void SynthC(void)
{
	synthForC.mixOut = 0;
	SoundUnitUnion *soundUnionList = &(synthForC.SoundUnitUnionList[0]);
	for (uint8_t i = 0; i < POLY_NUM; i++)
	{
		if (soundUnionList[i].combine.envelopeLevel == 0)
			continue;
		soundUnionList[i].combine.sampleVal = WaveTable[soundUnionList[i].combine.wavetablePos_int];
		soundUnionList[i].combine.val = (int16_t)soundUnionList[i].combine.envelopeLevel * soundUnionList[i].combine.sampleVal;

		uint32_t waveTablePos = soundUnionList[i].combine.increment +
								soundUnionList[i].combine.wavetablePos_frac +
								((uint32_t)soundUnionList[i].combine.wavetablePos_int << 8);

		uint16_t waveTablePosInt = waveTablePos >> 8;
		if (waveTablePosInt >= WAVETABLE_LEN)
			waveTablePosInt -= WAVETABLE_LOOP_LEN;
		soundUnionList[i].combine.wavetablePos_int = waveTablePosInt;
		soundUnionList[i].combine.wavetablePos_frac = 0xFF & waveTablePos;
		synthForC.mixOut += (soundUnionList[i].combine.val >> 8);
	}
}

void GenDecayEnvlopeC(void)
{
	__xdata SoundUnitUnion *soundUnionList = &(synthForC.SoundUnitUnionList[0]);
	for (uint8_t i = 0; i < POLY_NUM; i++)
	{
		if (soundUnionList[i].split.wavetablePos_int >= WAVETABLE_ATTACK_LEN &&
			(soundUnionList[i].split.envelopePos < (sizeof(EnvelopeTable) - 1)))
		{
			uint8_t p = soundUnionList[i].split.envelopePos;
			soundUnionList[i].split.envelopeLevel = EnvelopeTable[p];
			soundUnionList[i].split.envelopePos += 1;
		}
	}
}
#endif