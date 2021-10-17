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
    mov a,_currentTick
    add a,#1
    mov _currentTick,a
    mov a,(_currentTick+1)
    addc a,#0
    mov (_currentTick+1),a
    mov a,(_currentTick+2)
    addc a,#0
    mov (_currentTick+2),a
    mov a,(_currentTick+3)
    addc a,#0
    mov (_currentTick+3),a

    mov a,#DECAY_TIME_FACTOR
    clr cy
    subb a,_decayGenTick
    jc updateDecayGenTickEnd$
    inc _decayGenTick

updateDecayGenTickEnd$:
