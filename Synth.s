.module SYNTH
.globl _SynthAsm

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
;} SoundUnit;


;typedef struct _Synthesizer
;{
;    SoundUnitUnion SoundUnitUnionList[POLY_NUM];
;	int16_t mixOut;
;    uint8_t lastSoundUnit;
;}Synthesizer;

SoundUnitSize=10

pIncrement_int=0
pIncrement_frac=1
pWavetablePos_frac=2
pWavetablePos_int_h=4
pWavetablePos_int_l=3
pEnvelopeLevel=5
pEnvelopePos=6
pVal=7
pSampleVal=9

pMixOut=SoundUnitSize*POLY_NUM
pLastSoundUnit=SoundUnitSize*POLY_NUM+2


ENVELOP_LEN=256
POLY_NUM=5
WAVETABLE_CELESTA_C5_ATTACK_LEN=1998
WAVETABLE_CELESTA_C5_LEN=2608
WAVETABLE_CELESTA_C5_LOOP_LEN=(WAVETABLE_CELESTA_C5_LEN - WAVETABLE_CELESTA_C5_ATTACK_LEN)


.area CSEG    (CODE)
_SynthAsm:
	ar7 = 0x07
	ar6 = 0x06
	ar5 = 0x05
	ar4 = 0x04
	ar3 = 0x03
	ar2 = 0x02
	ar1 = 0x01
	ar0 = 0x00
    mov r6,dpl
    ret