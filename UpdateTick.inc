; ======== Global Variable ========
.globl _decayGenTick
.globl _currentTick

.include "SynthCore.inc"

.area DSEG    (DATA)
_decayGenTick::
	.ds 1
_currentTick::
	.ds 4
.area CSEG    (CODE)	
_UpdateTick:
;     currentTick++;
;     if(decayGenTick<200)
;         decayGenTick+=1;
; }
    ;ldw x,_currentTick+2
    ;addw x,#1
    ;ldw _currentTick+2,x
    ;ld a,_currentTick+1
    ;adc a,#0
    ;ld _currentTick+1,a
    ;ld a,_currentTick+0
    ;adc a,#0
    ;ld _currentTick+0,a

    ;ld a,_decayGenTick
    ;cp a,#DECAY_TIME_FACTOR
    ;jrnc updateDecayGenTickEnd$
    ;inc _decayGenTick

;updateDecayGenTickEnd$:
