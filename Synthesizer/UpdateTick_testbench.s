.module UPDATETICK_TESTBENCH

.globl _UpdateTick
.globl _timer_isr

.include "Player.inc"

.area CSEG    (CODE)
.include "UpdateTick.inc"
ret

_timer_isr:
reti
