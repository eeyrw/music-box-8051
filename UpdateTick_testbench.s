.module UPDATETICK_TESTBENCH

.globl _UpdateTick

.include "Player.inc"

.area CSEG    (CODE)
.include "UpdateTick.inc"
ret