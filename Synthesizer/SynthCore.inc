;====================================================================
; SynthCore.inc — Synthesizer 结构体字段偏移与常量定义
;
; 本文件定义所有汇编模块共享的常量：
;   - 波表参数 (通过 .include "WaveTable.inc" 引入)
;   - POLY_NUM
;   - SoundUnit 结构体的字段偏移 (10 字节/声道)
;   - Synthesizer 结构体的字段偏移
;   - SynthAbsAddr: Synthesizer 在 DATA 内存的绝对地址
;   - SynthTotalSize: Synthesizer 结构体总字节数
;
; 内存布局 (DATA 段)：
;   0x00-0x07: 寄存器 Bank 0 (C 代码使用)
;   0x08-0x0F: 寄存器 Bank 1 (ISR 使用)
;   0x10-0x1A: C 编译器自动分配的 DSEG 变量 (sysMsPre + sysMs, 5 字节)
;   0x1B-0x1F: 未使用 (5 字节)
;   0x20:      位寄存器
;   0x21-0x73: Synthesizer 结构体 (83 字节 = 8×10 + 3)
;   0x74-0xFF: C 栈 (140 字节)
;
;   Player 结构体 (~34 字节) 由 SDCC 分配到 XRAM, 不占用 DATA
;   VoiceState (midiNote+velocity, 8×2=16 字节) 由 SDCC 分配到 XRAM
;====================================================================

	; ---- 引入波表常量 ----
	.include "WaveTable.inc"

	; ---- 调试/功能开关 ----
	DEBUG_SYNTH=0
	USE_LINEAR_INTEROP=1

	; ---- 包络参数 ----
	ENVELOP_LEN=256
	POLY_NUM=8
	RELEASE_STEP=6

;====================================================================
; SoundUnit 结构体字段偏移 (每个声道 10 字节)
;
; 对应 C 结构体 SoundUnitSplit:
;   typedef struct _SoundUnitSplit {
;       uint8_t  increment_frac;      // 偏移 0: 相位增量小数部分
;       uint8_t  increment_int;       // 偏移 1: 相位增量整数部分
;       uint8_t  wavetablePos_frac;   // 偏移 2: 波形采样位置小数部分
;       uint16_t wavetablePos_int;    // 偏移 3-4: 波形采样位置整数部分
;       uint8_t  envelopeLevel;       // 偏移 5: 包络幅度 0-255
;       uint8_t  envelopePos;         // 偏移 6: 包络表索引 0-255
;       int16_t  val;                 // 偏移 7-8: 当前输出值
;       int8_t   sampleVal;           // 偏移 9: 波形采样值 (调试用)
;   } SoundUnitSplit;
;
; 注意：C 代码通过 union 的 combine 视图访问 increment 和 wavetablePos_int
; 作为 uint16_t；ASM 通过 split 视图访问各字节。
; 但由于 8051 是大端序，combine.increment (uint16_t) 的低字节在低地址
; (increment_frac=偏移0)，高字节在高地址 (increment_int=偏移1)。
; 这一点与直观预期可能相反——SDCC 对 8051 使用大端存储。
;
; VoiceState 结构体 (XRAM, 每声道 2 字节):
;   偏移 0: midiNote (uint8_t) — MIDI 音符编号, NoteOff 查找用
;   偏移 1: velocity  (uint8_t) — velocityCurve[vel] 缩放因子 0-255
;====================================================================

	unitSz=10                          ; 每个声道的字节数

	; ---- SoundUnitSplit 字段偏移 ----
	pIncrement_frac=0                  ; 相位增量小数部分 (uint8_t)
	pIncrement_int=1                   ; 相位增量整数部分 (uint8_t)
	pWavetablePos_frac=2               ; 采样位置小数部分 (uint8_t)
	pWavetablePos_int_h=4              ; 采样位置整数高字节 (uint8_t)
	pWavetablePos_int_l=3              ; 采样位置整数低字节 (uint8_t)
	pEnvelopeLevel=5                   ; 包络幅度 0-255 (uint8_t)
	pEnvelopePos=6                     ; 包络表索引 0-255 (uint8_t)
	pVal_l=7                           ; 当前输出值低字节 (int16_t)
	pVal_h=8                           ; 当前输出值高字节 (int16_t)
	pSampleVal=9                       ; 波形采样值 (int8_t, 调试用)

;====================================================================
; Synthesizer 结构体字段偏移 (83 字节)
;
;   typedef struct _Synthesizer {
;       SoundUnitUnion SoundUnitUnionList[POLY_NUM];  // 偏移 0-79 (8×10 字节)
;       int16_t  mixOut;                              // 偏移 80-81: 混合输出
;       uint8_t  lastSoundUnit;                       // 偏移 82: 下一个声道编号
;   } Synthesizer;
;====================================================================

	pMixOut_l=unitSz*POLY_NUM          ; 混合输出低字节 (80 = 8×10)
	pMixOut_h=unitSz*POLY_NUM+1       ; 混合输出高字节 (81)
	pLastSoundUnit=unitSz*POLY_NUM+2  ; 下一声道编号 (82)

; ---- Synthesizer 在 DATA 内存的绝对地址 ----
;     Player 已移至 XRAM (SDCC 自动分配), 所以此处不再依赖 PlayrAbsAddr
;     0x21 与原版 master 一致, 紧接在 DSEG 变量之后
	SynthAbsAddr = 0x21
	SynthTotalSize = unitSz*POLY_NUM+3 ; 83
