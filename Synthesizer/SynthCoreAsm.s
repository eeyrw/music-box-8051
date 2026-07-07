;====================================================================
; SynthCoreAsm.s — 合成器数据段 (绝对地址 0x21)
;
; NoteOnAsm / NoteOffAsm / GenDecayEnvlopeAsm / SynthReleaseAllAsm → SynthCore.c
; SynthAsm (ISR 热路径) → Synth.inc
;====================================================================

	.module SYNTH_CORE_ASM
	.include "SynthCore.inc"
	.globl _synthForAsm
	.globl _voiceState

	.area IABS    (ABS,DATA)
	.org SynthAbsAddr
_synthForAsm::
	.ds SynthTotalSize              ; 77 字节
