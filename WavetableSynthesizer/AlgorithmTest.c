#ifdef RUN_TEST
#include "SynthCore.h"
#include "Player.h"
#include <stdint.h>
#include <stdio.h>
#include "PeriodTimer.h"

#define TEST_LOOP_NUN 10000

extern void UpdateTick(void);

void PrintMem(uint8_t *src, uint8_t len)
{
    for (uint8_t i = 0; i < len; i++)
    {
        printf("%02X ", src[i]);
    }
    printf("\n");
}
void TestInit(void)
{
    SynthInit(&synthForC);
    SynthInit(&synthForAsm);
}

int16_t abs_u16(int16_t num)
{
    return num > 0 ? num : -num;
}

void PrintParameters(Synthesizer *synth)
{
    SoundUnitUnion *soundUnionList;
    printf("MixOut:%d\n", synth->mixOut);
    soundUnionList = &(synth->SoundUnitUnionList[0]);

    printf("%16s", "Chn Val");
    for (uint8_t k = 0; k < POLY_NUM; k++)
    {
        printf("%6d ", soundUnionList[k].combine.val);
    }
    printf("\n");

    printf("%16s", "Chn Sample");
    for (uint8_t k = 0; k < POLY_NUM; k++)
    {
        printf("%6d ", soundUnionList[k].combine.sampleVal);
    }
    printf("\n");

    printf("%16s", "Chn EnvLevel");
    for (uint8_t k = 0; k < POLY_NUM; k++)
    {
        printf("%6d ", soundUnionList[k].combine.envelopeLevel);
    }
    printf("\n");

    printf("%16s", "Chn WavePosInt");
    for (uint8_t k = 0; k < POLY_NUM; k++)
    {
        printf("%6d ", soundUnionList[k].combine.wavetablePos_int);
    }
    printf("\n");

    printf("%16s", "Chn WavePosFrac");
    for (uint8_t k = 0; k < POLY_NUM; k++)
    {
        printf("%6d ", soundUnionList[k].combine.wavetablePos_frac);
    }
    printf("\n");

    printf("%16s", "Chn EnvlPos");
    for (uint8_t k = 0; k < POLY_NUM; k++)
    {
        printf("%6d ", soundUnionList[k].combine.envelopePos);
    }
    printf("\n");

    printf("%16s", "Chn NoteIncrInt");
    for (uint8_t k = 0; k < POLY_NUM; k++)
    {
        printf("%6d ", soundUnionList[k].split.increment_int);
    }
    printf("\n");

    printf("%16s", "Chn NoteIncrFrac");
    for (uint8_t k = 0; k < POLY_NUM; k++)
    {
        printf("%6d ", soundUnionList[k].split.increment_frac);
    }
    printf("\n");
}

void TestUpdateTickFunc(void)
{
    uint32_t i;
    currentTick = 0;

    printf("~~~~~~~Start testing updateTickFunc.~~~~~~~\n");
    for (i = 0; i < 0xffff; i++)
    {
        if (i != currentTick)
        {
            printf("UpdateTickFunc get wrong in %ld loop. want:%ld, result:%ld\n", i, i, currentTick);
            break;
        }
        UpdateTick();
    }
    if (i == currentTick)
        printf("UpdateTickFunc passed the test.\n");
}

uint8_t SynthParamterCompare(Synthesizer *synthA, Synthesizer *synthB)
{
    uint8_t error = 0;
    SoundUnitUnion *sa = &(synthA->SoundUnitUnionList[0]);
    SoundUnitUnion *sb = &(synthB->SoundUnitUnionList[0]);

    if (abs_u16(synthA->mixOut - synthB->mixOut) > POLY_NUM)
        error++;
    for (uint8_t k = 0; k < POLY_NUM; k++)
    {
        if (abs_u16(sa[k].combine.val - sb[k].combine.val) > 2)
        {
            printf("SND ID:%d  Wrong chn value\n", k);
            error++;
            printf("SND ID:%d   C: ", k);
            PrintMem((__xdata uint8_t *)&sa[k], sizeof(SoundUnitUnion));
            printf("SND ID:%d ASM: ", k);
            PrintMem((uint8_t *)&sb[k], sizeof(SoundUnitUnion));
        }
        if (sa[k].combine.sampleVal != sb[k].combine.sampleVal)
        {
            printf("SND ID:%d  Wrong sample value\n", k);
            error++;
            printf("SND ID:%d   C: ", k);
            PrintMem((__xdata uint8_t *)&sa[k], sizeof(SoundUnitUnion));
            printf("SND ID:%d ASM: ", k);
            PrintMem((uint8_t *)&sb[k], sizeof(SoundUnitUnion));
        }
        if (sa[k].combine.envelopeLevel != sb[k].combine.envelopeLevel)
        {
            printf("SND ID:%d  Wrong envelopeLevel\n", k);
            error++;
            printf("SND ID:%d   C: ", k);
            PrintMem((__xdata uint8_t *)&sa[k], sizeof(SoundUnitUnion));
            printf("SND ID:%d ASM: ", k);
            PrintMem((uint8_t *)&sb[k], sizeof(SoundUnitUnion));
        }
        if (sa[k].combine.envelopePos != sb[k].combine.envelopePos)
        {
            printf("SND ID:%d  Wrong envelopePos\n", k);
            error++;
            printf("SND ID:%d   C: ", k);
            PrintMem((__xdata uint8_t *)&sa[k], sizeof(SoundUnitUnion));
            printf("SND ID:%d ASM: ", k);
            PrintMem((uint8_t *)&sb[k], sizeof(SoundUnitUnion));
        }
        if (sa[k].combine.wavetablePos_frac != sb[k].combine.wavetablePos_frac)
        {
            printf("SND ID:%d  Wrong wavetablePos_frac\n", k);
            error++;
            printf("SND ID:%d   C: ", k);
            PrintMem((__xdata uint8_t *)&sa[k], sizeof(SoundUnitUnion));
            printf("SND ID:%d ASM: ", k);
            PrintMem((uint8_t *)&sb[k], sizeof(SoundUnitUnion));
        }
        if (sa[k].combine.wavetablePos_int != sb[k].combine.wavetablePos_int)
        {
            printf("SND ID:%d  Wrong wavetablePos_int\n", k);
            error++;
            printf("SND ID:%d   C: ", k);
            PrintMem((__xdata uint8_t *)&sa[k], sizeof(SoundUnitUnion));
            printf("SND ID:%d ASM: ", k);
            PrintMem((uint8_t *)&sb[k], sizeof(SoundUnitUnion));
        }
        if (sa[k].combine.increment != sb[k].combine.increment)
        {
            printf("SND ID:%d  Wrong increment\n", k);
            error++;
            printf("SND ID:%d   C: ", k);
            PrintMem((__xdata uint8_t *)&sa[k], sizeof(SoundUnitUnion));
            printf("SND ID:%d ASM: ", k);
            PrintMem((uint8_t *)&sb[k], sizeof(SoundUnitUnion));
        }
    }
    if (error > 0)
    {
        printf("%d error(s) found:\n", error);
        printf("Synth C:\n");
        PrintParameters(synthA);
        printf("Synth ASM:\n");
        PrintParameters(synthB);
    }
    else
    {
        printf("Passed.\n");
    }
    return error;
}
extern void NoteOnAsmP(uint8_t n);
extern void GenDecayEnvlopeAsmP(void);

void TestSynth(void)
{
    uint8_t tryNum = 3;
    printf("~~~~~~~Start testing synthesizer.~~~~~~~\n");
    for (uint8_t i = 0; i < POLY_NUM+1; i++)
    {
        NoteOnC(i * 2 + 80);
        NoteOnAsm(i * 2 + 80);
    }
    for (uint16_t i = 0; i < TEST_LOOP_NUN; i++)
    {
        printf("ENV==========%d==============\n", i);
        GenDecayEnvlopeAsm();
        GenDecayEnvlopeC();
        if ((SynthParamterCompare(&synthForC, &synthForAsm) > 0) && (tryNum--) == 0)
            break;
        for (uint8_t i = 0; i < 20; i++)
        {
            SynthAsm();
            SynthC();
        }
        printf("SYNTH========%d==============\n", i);
        if ((SynthParamterCompare(&synthForC, &synthForAsm) > 0) && (tryNum--) == 0)
            break;
    }
}

void TestProcess(void)
{
    TestInit();
    TestUpdateTickFunc();
    TestSynth();
}
#endif