.include "WaveTable.inc"

DEBUG_SYNTH=0
USE_LINEAR_INTEROP=1

ENVELOP_LEN=256
POLY_NUM=7


DECAY_TIME_FACTOR=250


;typedef struct _SoundUnit
;{
;	uint8_t increment_frac;
;	uint8_t increment_int;
;	uint8_t  wavetablePos_frac;
;	uint16_t  wavetablePos_int;
;	uint8_t envelopeLevel;
;	uint8_t envelopePos;
;	int16_t val;
;	int8_t sampleVal;
;   uint16_t tick;
;} SoundUnit;


;typedef struct _Synthesizer
;{
;    SoundUnitUnion SoundUnitUnionList[POLY_NUM];
;	 int16_t mixOut;
;    uint8_t lastSoundUnit;
;    uint8_t tempTick;
;}Synthesizer;



pIncrement_int=1
pIncrement_frac=0
pWavetablePos_frac=2
pWavetablePos_int_h=4
pWavetablePos_int_l=3
pEnvelopeLevel=5
pEnvelopePos=6
pVal_l=7
pVal_h=8
pSampleVal=9
pTick_l=10
pTick_h=11
unitSz=(pTick_h+1)

pMixOut_l=unitSz*POLY_NUM
pMixOut_h=unitSz*POLY_NUM+1
pLastSoundUnit=unitSz*POLY_NUM+2
pTempTick=unitSz*POLY_NUM+3

SynthAbsAddr = 0x10+11+7
SynthTotalSize = unitSz*POLY_NUM+4

