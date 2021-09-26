

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

.irp  Idx,0,1,2,3,4,5
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


;	if (synthForAsm.mixOut > 253)
	clr	c
	mov	a,#0xfd
	subb	a,r4
	mov	a,#(0x00 ^ 0x80)
	mov	b,r5
	xrl	b,#0x80
	subb	a,b
	jnc	00104$
;	synthForAsm.mixOut = 253;
	mov	r4,#0xfd
	mov	r5,#0x00
	sjmp	00107$
00104$:
;	else if (synthForAsm.mixOut < -255)
	clr	c
	mov	a,r4
	subb	a,#0x01
	mov	a,r5
	xrl	a,#0x80
	subb	a,#0x7f
	jnc	00107$
;	synthForAsm.mixOut = -255;
	mov	r4,#0x01
	mov	r5,#0xff
00107$:

; mixOut = mixOut>>1
mov a,r5
mov c,a.0
mov a,r4
rrc a
mov r4,a
clr c
subb a,#0x80

mov REG_CCAP0H,a
cpl a
mov REG_CCAP1H,a


;pop a
;pop b
;pop dph
;pop dpl