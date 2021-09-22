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

unitSz=10

pIncrement_int=0
pIncrement_frac=1
pWavetablePos_frac=2
pWavetablePos_int_h=4
pWavetablePos_int_l=3
pEnvelopeLevel=5
pEnvelopePos=6
pVal_l=7
pVal_h=8
pSampleVal=9

pMixOut_l=unitSz*POLY_NUM
pMixOut_h=unitSz*POLY_NUM+1
pLastSoundUnit=unitSz*POLY_NUM+2

pSynth = (0x10+0x0d)

ENVELOP_LEN=256
POLY_NUM=5
WAVETABLE_CELESTA_C5_ATTACK_LEN=1998
WAVETABLE_CELESTA_C5_LEN=2608
WAVETABLE_CELESTA_C5_LOOP_LEN=(WAVETABLE_CELESTA_C5_LEN - WAVETABLE_CELESTA_C5_ATTACK_LEN)


.area CSEG    (CODE)
_SynthAsm:
	; r4,r5,r6,r7 : temporary registers

.irp  Idx,0,1,2,3,4
	pSndUnit = pSynth+Idx*unitSz
	mov a, (pSndUnit+pEnvelopeLevel)
	mov b,a
	jz loopSynthEnd'Idx'$
	mov dpl, (pSndUnit+pWavetablePos_int_l)
	mov a,(pSndUnit+pWavetablePos_int_h)
	add a,#(_WaveTable_Celesta_C5>>8)
	mov dph,a
	mov a,#(_WaveTable_Celesta_C5)
	movc a,@a+dptr
	mov (pSndUnit+pSampleVal),a

	jnb a.7,signedMulBr1'Idx'$	; Do signed mutiple with unsigned MUL
	mul ab

	mov (pSndUnit+pVal_l),a
	mov (pSndUnit+pVal_h),b

	mov a,b			; Div with 0xFF
	clr b
	sjmp signedMulBr2End'Idx'$

signedMulBr1'Idx'$:
		cpl a				;Do signed mutiple with unsigned MUL
		inc a
		mul ab				; Mutiple envelopeLevel with sample

		mov (pSndUnit+pVal_l),a
		mov (pSndUnit+pVal_h),b

		mov a,b				; Div with 0xFF
		mov b,#0xFF
		cpl a
		inc a
signedMulBr2End'Idx'$:
		add a, (pSynth+pMixOut_l)
		mov (pSynth+pMixOut_l),a
		mov a,b
		addc a,(pSynth+pMixOut_h)
		mov (pSynth+pMixOut_h),a

		mov a,(pSndUnit+pIncrement_frac)
		addc a,(pSndUnit+pWavetablePos_frac)
		mov (pSndUnit+pWavetablePos_frac),a

		mov a,(pSndUnit+pIncrement_int)
		addc a,(pSndUnit+pWavetablePos_int_l)
		mov (pSndUnit+pWavetablePos_int_l),a

		mov a,#0
		addc a,(pSndUnit+pWavetablePos_int_h)
		mov (pSndUnit+pWavetablePos_int_h),a

	branch0_start'Idx'$:
		clr cy
		mov a,(pSndUnit+pWavetablePos_int_l)
		subb a,#WAVETABLE_CELESTA_C5_LEN
		mov a,(pSndUnit+pWavetablePos_int_h)
		subb a, #(WAVETABLE_CELESTA_C5_LEN>>8)
		jc branch0_end'Idx'$			; Jump if WAVETABLE_CELESTA_C5_LEN is great than x
		mov a,(pSndUnit+pWavetablePos_int_l)
		subb a,#WAVETABLE_CELESTA_C5_LOOP_LEN
		mov (pSndUnit+pWavetablePos_int_l),a
		mov a,(pSndUnit+pWavetablePos_int_h)
		subb a, #(WAVETABLE_CELESTA_C5_LOOP_LEN>>8)	
		mov (pSndUnit+pWavetablePos_int_h),a	
	branch0_end'Idx'$:
loopSynthEnd'Idx'$:
.endm 
		clr cy
mov a,(pSynth+pMixOut_l)
subb a,#253
mov a,(pSynth+pMixOut_h)
subb a,#(253>>8)
jc branch_lt_253$
mov r4,#253
mov r5,#(253>>8)
branch_lt_253$:
clr cy
mov a,#-255
subb a,(pSynth+pMixOut_l)
mov a,#(-255>>8)
subb a,(pSynth+pMixOut_h)
jc branch_lt_gt_end$
mov r4,#-255
mov r5,#(-255>>8)
branch_lt_gt_end$:

mov a,r5
mov cy,acc.7
rrc a
mov r5,a
mov a,r4
rrc a
mov r4,a
clr cy
subb a,#0x80




    ret