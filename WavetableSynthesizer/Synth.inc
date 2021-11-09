.area CSEG    (CODE)
_SynthAsm:
	pSynth = SynthAbsAddr
	mov (pSynth+pMixOut_l),#0
	mov (pSynth+pMixOut_h),#0	

	; r4,r5,r6,r7 : temporary registers

.irp  Idx,0,1,2,3,4,5,6,7

	pSndUnit = pSynth+Idx*unitSz
	mov a, (pSndUnit+pEnvelopeLevel)
	mov r4,a
	jz soundUnitProcessEnd'Idx'$

	mov dpl, (pSndUnit+pWavetablePos_int_l)
	mov a,(pSndUnit+pWavetablePos_int_h)
	add a,#(_WaveTable>>8)
	mov dph,a
	mov a,#(_WaveTable)
	movc a,@a+dptr

.ifne USE_LINEAR_INTEROP
	mov r0,a
	mov a,#(_WaveTable+1)
	movc a,@a+dptr	
	clr c
	subb a,r0
	mov b, (pSndUnit+pWavetablePos_frac)

	jb a.7,signedMulNegBranch0'Idx'$	; Do signed mutiple with unsigned MUL
	mul ab
	mov a,b			; Div with 0xFF
	sjmp signedMulEnd0'Idx'$
	signedMulNegBranch0'Idx'$:
	cpl a				;Do signed mutiple with unsigned MUL
	inc a
	mul ab				; Mutiple envelopeLevel with sample
	cpl  a
	add  a,#1
	mov a,b
	cpl a
	addc a,#0
	signedMulEnd0'Idx'$:

	add a,r0
.endif
	mov b,r4
.ifne DEBUG_SYNTH
	;mov (pSndUnit+pSampleVal),a
.endif

	jb a.7,signedMulNegBranch'Idx'$	; Do signed mutiple with unsigned MUL
	mul ab
.ifne DEBUG_SYNTH
	mov (pSndUnit+pVal_l),a
	mov (pSndUnit+pVal_h),b
.endif
	mov a,b			; Div with 0xFF
	mov b,#0
	sjmp signedMulEnd'Idx'$
	signedMulNegBranch'Idx'$:
	cpl a				;Do signed mutiple with unsigned MUL
	inc a
	mul ab				; Mutiple envelopeLevel with sample

	cpl  a
	add  a,#1
.ifne DEBUG_SYNTH
	mov (pSndUnit+pVal_l),a
.endif
	mov a,b
	cpl a
	addc a,#0
.ifne DEBUG_SYNTH
	mov (pSndUnit+pVal_h),a	
.endif
	mov b,#0xFF ; Div with 0xFF	
	signedMulEnd'Idx'$:

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


	; if (waveTablePosInt >= WAVETABLE_LEN)
	clr c
	mov a,(pSndUnit+pWavetablePos_int_l)
	subb a,#(WAVETABLE_LEN)
	mov a,(pSndUnit+pWavetablePos_int_h) 
	subb a,#((WAVETABLE_LEN)>>8)
	jc waveTablePosAdjustEnd'Idx'$
	;	waveTablePosInt -= WAVETABLE_LOOP_LEN;
	clr c			
	mov a,(pSndUnit+pWavetablePos_int_l)
	subb a,#WAVETABLE_LOOP_LEN
	mov (pSndUnit+pWavetablePos_int_l),a
	mov a,(pSndUnit+pWavetablePos_int_h)
	subb a, #(WAVETABLE_LOOP_LEN>>8)	
	mov (pSndUnit+pWavetablePos_int_h),a	
	waveTablePosAdjustEnd'Idx'$:
	soundUnitProcessEnd'Idx'$:

.endm

	mov r4,(pSynth+pMixOut_l)
	mov r5,(pSynth+pMixOut_h)

	mixOut = mixOut>>1
	mov a,r5
	mov c,a.7
	rrc a
	mov r5,a
	mov a,r4
	rrc a
	mov r4,a


CLIP_UPPER=127
CLIP_LOWER=-128

;	main.c:24: if (synthForAsm.mixOut > CLIP_UPPER)
	clr	c
	mov	a,#(CLIP_UPPER&0xFF)
	subb	a,r4
	mov	a,#((CLIP_UPPER>>8) ^ 0x80)
	mov	b,r5
	xrl	b,#0x80
	subb	a,b
	jnc	00104$
;	main.c:26: synthForAsm.mixOut = CLIP_UPPER;
	mov	r4,#(CLIP_UPPER&0xFF)
	mov	r5,#(CLIP_UPPER>>8)
	sjmp	00107$
00104$:
;	main.c:28: else if (synthForAsm.mixOut < CLIP_LOWER)
	clr	c
	mov	a,r4
	subb	a,#(CLIP_LOWER&0xFF)
	mov	a,r5
	xrl	a,#0x80
	subb	a,#((CLIP_LOWER>>8)&0x7F)
	jnc	00107$
;	main.c:30: synthForAsm.mixOut = CLIP_LOWER;
	mov	r4,#(CLIP_LOWER&0xFF)
	mov	r5,#(CLIP_LOWER>>8)
00107$:

	; a = a - (-CLIP_LOWER)
	mov a,r4
	clr c
	subb a,#(CLIP_LOWER&0xFF)
	mov r4,a
	mov a,r5
	subb a,#((CLIP_LOWER>>8)&0xFF)
	mov r5,a


	mov	dptr,#REG_PWMA_CCR2H
	mov a,r5
	movx	@dptr,a
	mov	dptr,#REG_PWMA_CCR2L
	mov a,r4
	movx	@dptr,a


	;mov REG_CCAP0H,a
	;cpl a
	;mov REG_CCAP1H,a