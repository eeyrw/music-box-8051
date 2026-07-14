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
	.globl _MulU8High

	.area IABS    (ABS,DATA)
	.org SynthAbsAddr
_synthForAsm::
	.ds SynthTotalSize              ; 82 字节

	.area CSEG    (CODE)
_MulU8High:
	mov	a,dpl
	mov	b,a
	mov	a,sp
	add	a,#0xfe
	mov	r0,a
	mov	a,@r0
	mul	ab
	mov	dpl,b
	ret
