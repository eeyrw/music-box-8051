;====================================================================
; SynthCore.inc — Synthesizer 结构体字段偏移与常量定义
;
; 本文件定义所有汇编模块共享的常量。
;
; 内存布局 (DATA 段)：
;   0x00-0x07: 寄存器 Bank 0 (C 代码使用)
;   0x08-0x0F: 寄存器 Bank 1 (ISR 使用)
;   0x10-0x1A: DSEG 变量 (sysMsPre + sysMs, 5 字节)
;   0x1B-0x1F: 未使用
;   0x20:      位寄存器
;   0x21-0x6D: Synthesizer (77 字节 = 8×9 + 5)
;   0x6E-0xFF: C 栈 (146 字节)
;
;   Player (~34 字节) → XRAM
;   VoiceState (5×8=40 字节) → XRAM
;====================================================================

	.include "WaveTable.inc"

	DEBUG_SYNTH=0
	USE_LINEAR_INTEROP=1
	USE_DITHERING=1

	; ---- ADSR 时钟 ----
	ADSR_TICK_MS=5

	; ---- ADSR 步进值 (同步于 SynthCore.h) ----
	; ATTACK: step=22 → 6 ticks × 5ms = 30ms
	; DECAY:  step=3  → ceil(28/3)=10 ticks × 5ms = 50ms (delta=28)
	; RELEASE: step=1  → 100 ticks × 5ms = 500ms (from SUSTAIN_THR=100)
	ADSR_ENV_MAX=128
	ADSR_ATTACK_RATE=22
	ADSR_DECAY_RATE=3
	ADSR_SUSTAIN_THRESHOLD=100
	ADSR_SUSTAIN_DECAY_RATE=0
	ADSR_RELEASE_RATE=1

	; ---- 包络状态 ----
	ENV_STATE_SILENT=0
	ENV_STATE_ATTACK=1
	ENV_STATE_DECAY=2
	ENV_STATE_SUSTAIN=3
	ENV_STATE_RELEASE=4

	ENVELOP_LEN=256
	POLY_NUM=8

	unitSz=9

	; ---- SoundUnitSplit 字段偏移 ----
	pIncrement_frac=0
	pIncrement_int=1
	pWavetablePos_frac=2
	pWavetablePos_int_h=4
	pWavetablePos_int_l=3
	pEnvelopeLevel=5
	pVal_l=6
	pVal_h=7
	pSampleVal=8

; ---- VoiceState XRAM 字段偏移 (5 字节/声道) ----
;   0: midiNote
;   1: velocity  (MIDI_vel << 1, 0-254)
;   2: envelopeState
;   3: envelopePhase  (att/dec/sus/rel: 当前 env 值 0-128)
;   4: reserved
	vState_midiNote=0
	vState_velocity=1
	vState_envState=2
	vState_envPhase=3
	vState_sz=5

; ---- Synthesizer 字段偏移 ----
	pMixOut_l=unitSz*POLY_NUM          ; 72
	pMixOut_h=unitSz*POLY_NUM+1       ; 73
	pLastSoundUnit=unitSz*POLY_NUM+2  ; 74
	pLFSR_l=unitSz*POLY_NUM+3         ; 75
	pLFSR_h=unitSz*POLY_NUM+4         ; 76

	SynthAbsAddr = 0x21
	SynthTotalSize = unitSz*POLY_NUM+5 ; 77
