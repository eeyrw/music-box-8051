.include "SynthCore.inc"
.include "8051.inc"
.module PERIOD_TIMER
; ======== Global function ========
.globl _timer_isr
.area REG_BANK_1	(REL,OVR,DATA)
.ds 8
.area CSEG    (CODE)
_timer_isr:
	ar7 = 0x0f
	ar6 = 0x0e
	ar5 = 0x0d
	ar4 = 0x0c
	ar3 = 0x0b
	ar2 = 0x0a
	ar1 = 0x09
	ar0 = 0x08
	push	acc
	push	b
	push	dpl
	push	dph
	push	psw
	mov	psw,#0x08
	setb P55
.include "Synth.inc"
.include "UpdateTick.inc"
	clr P55
	pop	psw
	pop	dph
	pop	dpl
	pop	b
	pop	acc
reti
	