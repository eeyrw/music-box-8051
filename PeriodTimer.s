;.include "SynthCore.inc"
;.include "STM8.inc"
.module PERIOD_TIMER
; ======== Global function ========
.globl _timer_isr

.area CODE
_timer_isr:
;	TIM4_SR &= ~(1 << TIM4_SR_UIF);
;	bres	21316, #0
;	MEASURE_S;
;	bset	20485, #5
;.include "Synth.inc"
;.include "UpdateTick.inc"
;	MEASURE_E;
;	bres	20485, #5
reti
	