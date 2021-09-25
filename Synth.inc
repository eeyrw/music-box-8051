

.area CSEG    (CODE)
_SynthAsm:
	;push dpl
	;push dph
	;push b
	;push a
	pSynth = SynthAbsAddr
	mov (pSynth+pMixOut_l),#0
	mov (pSynth+pMixOut_h),#0	

	; r4,r5,r6,r7 : temporary registers

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

	jb a.7,signedMulBr1'Idx'$	; Do signed mutiple with unsigned MUL
	mul ab
	mov (pSndUnit+pVal_l),a
	mov (pSndUnit+pVal_h),b
	mov a,b			; Div with 0xFF
	mov b,#0
	sjmp signedMulBr2End'Idx'$

signedMulBr1'Idx'$:
		cpl a				;Do signed mutiple with unsigned MUL
		inc a
		mul ab				; Mutiple envelopeLevel with sample

		cpl  a
		add  a,#1
		mov (pSndUnit+pVal_l),a
		mov a,b
		cpl a
		addc a,#0
		mov (pSndUnit+pVal_h),a	
		mov b,#0xFF ; Div with 0xFF
	
signedMulBr2End'Idx'$:
		add a, (pSynth+pMixOut_l)
		mov (pSynth+pMixOut_l),a
		mov a,b
		addc a,(pSynth+pMixOut_h)
		mov (pSynth+pMixOut_h),a

		mov a,(pSndUnit+pIncrement_frac)
		add a,(pSndUnit+pWavetablePos_frac)
		mov (pSndUnit+pWavetablePos_frac),a

		mov a,(pSndUnit+pIncrement_int)
		addc a,(pSndUnit+pWavetablePos_int_l)
		mov (pSndUnit+pWavetablePos_int_l),a

		mov a,#0
		addc a,(pSndUnit+pWavetablePos_int_h)
		mov (pSndUnit+pWavetablePos_int_h),a

	branch0_start'Idx'$:
		;		if (waveTablePosInt >= WAVETABLE_LEN)
		;	waveTablePosInt -= WAVETABLE_LOOP_LEN;
		clr c
		mov a,(pSndUnit+pWavetablePos_int_l)
		subb a,#(WAVETABLE_LEN)
		mov a,(pSndUnit+pWavetablePos_int_h) 
		subb a,#((WAVETABLE_LEN)>>8)
		jc branch0_end'Idx'$
		clr c			
		mov a,(pSndUnit+pWavetablePos_int_l)
		subb a,#WAVETABLE_LOOP_LEN
		mov (pSndUnit+pWavetablePos_int_l),a
		mov a,(pSndUnit+pWavetablePos_int_h)
		subb a, #(WAVETABLE_LOOP_LEN>>8)	
		mov (pSndUnit+pWavetablePos_int_h),a	
	branch0_end'Idx'$:
loopSynthEnd'Idx'$:
.endm 

mov r4,(pSynth+pMixOut_l)
mov r5,(pSynth+pMixOut_h)
sjmp branch_lt_gt_end$

clr c
mov a,(pSynth+pMixOut_l)
subb a,#253
mov a,(pSynth+pMixOut_h)
subb a,#(253>>8)
jc branch_lt_253$
mov r4,#253
mov r5,#(253>>8)
branch_lt_253$:
clr c
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
mov c,a.0
mov a,r4
rrc a
mov r4,a
clr c
subb a,#0x80


;mov REG_CCAP0L,a
mov REG_CCAP0H,a

cpl a


;mov REG_CCAP1L,a
mov REG_CCAP1H,a

;pop a
;pop b
;pop dph
;pop dpl