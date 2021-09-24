.module UPDATETICK_TESTBENCH

.globl _UpdateTick

.include "SynthCore.inc"
.include "Player.inc"

.area DATA

.area CODE
.include "UpdateTick.inc"
ret