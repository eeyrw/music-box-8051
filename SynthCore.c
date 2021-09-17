#include "SynthCore.h"
#include <stdint.h>
#include <stdio.h>
#include "WaveTable_Celesta_C5.h"
#include "EnvelopeTable.h"
#define RUN_TEST

void SynthInit(Synthesizer* synth)
{
    SoundUnitUnion* soundUnionList=&(synth->SoundUnitUnionList[0]);
	for (uint8_t i = 0; i < POLY_NUM; i++)
	{
		soundUnionList[i].combine.increment = 0;
		soundUnionList[i].combine.wavetablePos_frac = 0;
		soundUnionList[i].combine.wavetablePos_int = 0;
		soundUnionList[i].combine.envelopeLevel = 255;
		soundUnionList[i].combine.envelopePos = 0;
        soundUnionList[i].combine.val = 0;
	}
    synth->lastSoundUnit=0;
}


void NoteOnAsm(Synthesizer* synth,uint8_t note)
{

}
void GenDecayEnvlopeAsm(Synthesizer* synth)
{
	
}
void SynthAsm(Synthesizer* synth)
{
	
}
#ifdef RUN_TEST
void NoteOnC(Synthesizer* synth,uint8_t note)
{
	uint8_t lastSoundUnit = synth->lastSoundUnit;

	// disable_interrupts();
	synth->SoundUnitUnionList[lastSoundUnit].combine.increment = WaveTable_Celesta_C5_Increment[note&0x7F];
	synth->SoundUnitUnionList[lastSoundUnit].combine.wavetablePos_frac = 0;
	synth->SoundUnitUnionList[lastSoundUnit].combine.wavetablePos_int = 0;
	synth->SoundUnitUnionList[lastSoundUnit].combine.envelopePos = 0;
	synth->SoundUnitUnionList[lastSoundUnit].combine.envelopeLevel = 255;
	// enable_interrupts();

	lastSoundUnit++;

	if (lastSoundUnit== POLY_NUM)
		lastSoundUnit = 0;

    synth->lastSoundUnit=lastSoundUnit;
}

void SynthC(Synthesizer* synth)
{
    synth->mixOut=0;
    SoundUnitUnion* soundUnionList=&(synth->SoundUnitUnionList[0]);
    for(uint8_t i=0;i<POLY_NUM;i++)
    {
        soundUnionList[i].combine.val=soundUnionList[i].combine.envelopeLevel*WaveTable_Celesta_C5[soundUnionList[i].combine.wavetablePos_int]/255;
        soundUnionList[i].combine.sampleVal=WaveTable_Celesta_C5[soundUnionList[i].combine.wavetablePos_int];
		uint32_t waveTablePos=soundUnionList[i].combine.increment+
                             soundUnionList[i].combine.wavetablePos_frac+
                             ((uint32_t)soundUnionList[i].combine.wavetablePos_int<<8); 

        uint16_t waveTablePosInt= waveTablePos>>8;
        if(waveTablePosInt>WAVETABLE_CELESTA_C5_LEN)
           waveTablePosInt-=WAVETABLE_CELESTA_C5_LOOP_LEN;
        soundUnionList[i].combine.wavetablePos_int= waveTablePosInt;
        soundUnionList[i].combine.wavetablePos_frac=0xFF&waveTablePos;
        synth->mixOut+=soundUnionList[i].combine.val;
    }
}

void GenDecayEnvlopeC(Synthesizer* synth)
{
    SoundUnitUnion* soundUnionList=&(synth->SoundUnitUnionList[0]);
	for (uint8_t i = 0; i < POLY_NUM; i++)
	{
		if(soundUnionList[i].combine.wavetablePos_int >= WAVETABLE_CELESTA_C5_ATTACK_LEN &&
				soundUnionList[i].combine.envelopePos <sizeof(EnvelopeTable)-1)
		{
			soundUnionList[i].combine.envelopeLevel = EnvelopeTable[soundUnionList[i].combine.envelopePos];
			soundUnionList[i].combine.envelopePos += 1;
		}
	}
}
#endif