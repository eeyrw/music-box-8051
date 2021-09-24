.globl _synthForAsm
.area DSEG    (DATA)
_synthForAsm::
	.ds unitSz*POLY_NUM+3
.area CSEG    (CODE)
_SynthAsm:
	; r4,r5,r6,r7 : temporary registers
	pSynth = _synthForAsm
.irp  Idx,0,1,2,3,4
	pSndUnit = pSynth+Idx*unitSz
	mov a, (pSndUnit+pEnvelopeLevel)
	mov b,a
	jz loopSynthEnd'Idx'$
	mov dpl, (pSndUnit+pWavetablePos_int_l)
	mov a,(pSndUnit+pWavetablePos_int_h)
	add a,#(_WaveTable>>8)
	mov dph,a
	mov a,#(_WaveTable)
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
		subb a,#WAVETABLE_LEN
		mov a,(pSndUnit+pWavetablePos_int_h)
		subb a, #(WAVETABLE_LEN>>8)
		jc branch0_end'Idx'$			; Jump if WAVETABLE_LEN is great than x
		mov a,(pSndUnit+pWavetablePos_int_l)
		subb a,#WAVETABLE_LOOP_LEN
		mov (pSndUnit+pWavetablePos_int_l),a
		mov a,(pSndUnit+pWavetablePos_int_h)
		subb a, #(WAVETABLE_LOOP_LEN>>8)	
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

; mixOut = mixOut>>1
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