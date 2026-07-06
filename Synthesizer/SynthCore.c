#include "SynthCore.h"
#include <stdint.h>
#include <stdio.h>
#include "WaveTable.h"

__xdata VoiceState voiceState[POLY_NUM];

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
		soundUnionList[i].combine.envelopePos = 255;
		soundUnionList[i].combine.val = 0;
		voiceState[i].midiNote = 0;
		voiceState[i].velocity = 0;
	}
	synth->lastSoundUnit = 0;
	synth->mixOut = 0;
}
#ifdef RUN_TEST
static uint8_t selectVoice(Synthesizer *synth)
{
	uint8_t i;
	for (i = 0; i < POLY_NUM; i++) {
		if (synth->SoundUnitUnionList[i].split.envelopeLevel == 0)
			return i;
	}
	return synth->lastSoundUnit;
}

void NoteOnAsmP(uint8_t note)
{
	uint8_t idx = selectVoice(&synthForAsm);

	// disable_interrupts();
	synthForAsm.SoundUnitUnionList[idx].combine.increment = WaveTable_Increment[note & 0x7F];
	synthForAsm.SoundUnitUnionList[idx].combine.wavetablePos_frac = 0;
	synthForAsm.SoundUnitUnionList[idx].combine.wavetablePos_int = 0;
	synthForAsm.SoundUnitUnionList[idx].combine.envelopePos = 255;
	synthForAsm.SoundUnitUnionList[idx].combine.envelopeLevel = 255;
	// enable_interrupts();

	voiceState[idx].midiNote = note;
	voiceState[idx].velocity = 255;

	idx++;
	if (idx == POLY_NUM)
		idx = 0;
	synthForAsm.lastSoundUnit = idx;
}

void GenDecayEnvlopeAsmP(void)
{
	__data SoundUnitUnion *soundUnionList = &(synthForAsm.SoundUnitUnionList[0]);
	for (uint8_t i = 0; i < POLY_NUM; i++)
	{
		if (soundUnionList[i].split.envelopePos < (sizeof(EnvelopeTable) - 1))
		{
			uint8_t p = soundUnionList[i].split.envelopePos;
			soundUnionList[i].split.envelopeLevel = (uint8_t)(((uint16_t)EnvelopeTable[p] * voiceState[i].velocity) >> 8);
			uint16_t newPos = (uint16_t)p + 6;
			if (newPos >= 255)
				newPos = 255;
			soundUnionList[i].split.envelopePos = (uint8_t)newPos;
		}
	}
}

void NoteOffAsmP(uint8_t note)
{
	for (uint8_t i = 0; i < POLY_NUM; i++)
	{
		if (synthForAsm.SoundUnitUnionList[i].combine.envelopeLevel > 0 &&
			voiceState[i].midiNote == note)
		{
			uint8_t p = 0;
			while (p < 255 && EnvelopeTable[p] >= synthForAsm.SoundUnitUnionList[i].combine.envelopeLevel)
				p += 6;
			synthForAsm.SoundUnitUnionList[i].combine.envelopePos = p;
		}
	}
}

void NoteOnC(uint8_t note)
{
	uint8_t idx = selectVoice(&synthForC);

	// disable_interrupts();
	synthForC.SoundUnitUnionList[idx].combine.increment = WaveTable_Increment[note & 0x7F];
	synthForC.SoundUnitUnionList[idx].combine.wavetablePos_frac = 0;
	synthForC.SoundUnitUnionList[idx].combine.wavetablePos_int = 0;
	synthForC.SoundUnitUnionList[idx].combine.envelopePos = 255;
	synthForC.SoundUnitUnionList[idx].combine.envelopeLevel = 255;
	// enable_interrupts();

	idx++;
	if (idx == POLY_NUM)
		idx = 0;
	synthForC.lastSoundUnit = idx;
}

void NoteOffC(uint8_t note)
{
	for (uint8_t i = 0; i < POLY_NUM; i++)
	{
		if (synthForC.SoundUnitUnionList[i].combine.envelopeLevel > 0 &&
			voiceState[i].midiNote == note)
		{
			uint8_t p = 0;
			while (p < 255 && EnvelopeTable[p] >= synthForC.SoundUnitUnionList[i].combine.envelopeLevel)
				p += 6;
			synthForC.SoundUnitUnionList[i].combine.envelopePos = p;
		}
	}
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
		if (soundUnionList[i].split.envelopePos < (sizeof(EnvelopeTable) - 1))
		{
			uint8_t p = soundUnionList[i].split.envelopePos;
			soundUnionList[i].split.envelopeLevel = (uint8_t)(((uint16_t)EnvelopeTable[p] * voiceState[i].velocity) >> 8);
			uint16_t newPos = (uint16_t)p + 6;
			if (newPos >= 255)
				newPos = 255;
			soundUnionList[i].split.envelopePos = (uint8_t)newPos;
		}
	}
}
#endif
