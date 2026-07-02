;====================================================================
; SynthCore.inc — Synthesizer 结构体字段偏移与常量定义
;
; 本文件定义所有汇编模块共享的常量：
;   - 波表参数 (通过 .include "WaveTable.inc" 引入)
;   - POLY_NUM / DECAY_TIME_FACTOR
;   - SoundUnit 结构体的字段偏移 (10 字节/声道)
;   - Synthesizer 结构体的字段偏移
;   - SynthAbsAddr: Synthesizer 在 DATA 内存的绝对地址
;   - SynthTotalSize: Synthesizer 结构体总字节数
;
; 内存布局 (DATA 段)：
;   0x00-0x07: 寄存器 Bank 0 (C 代码使用)
;   0x08-0x0F: 寄存器 Bank 1 (ISR 使用)
;   0x10-0x1D: Player 结构体 (14 字节, Player.inc 定义)
;   0x1E-0x20: 保留 (3 字节对齐)
;   0x21-0x73: Synthesizer 结构体 (83 字节 = 8×10 + 3)
;
;   总占用: 0x10-0x73 = 100 字节，剩余 0x74-0xFF = 140 字节用于 C 栈
;====================================================================

	; ---- 引入波表常量 ----
	.include "WaveTable.inc"

	; ---- 调试/功能开关 ----
	DEBUG_SYNTH=0                      ; 1=启用调试输出 (将中间值写入 SoundUnit.val/sampleVal)
	USE_LINEAR_INTEROP=1               ; 1=用线性插值, 0=不用插值 (直接取最近采样点)

	; ---- 包络参数 ----
	ENVELOP_LEN=256                    ; EnvelopeTable 数组长度
	POLY_NUM=8                         ; 复音数 (同时发声数)

	; ---- 衰减时间因子 ----
	;     decayGenTick 每 tick 增 1, 到 100 后触发 GenDecayEnvlopeAsm
	;     即每 100/32000 ≈ 3.125ms 推进一格包络
	DECAY_TIME_FACTOR=100

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
;   typedef union _SoundUnitUnion {
;       SoundUnit combine;            // 组合视图 (uint16_t increment, uint16_t wavetablePos_int)
;       SoundUnitSplit split;         // 拆分视图 (上面各字段)
;   } SoundUnitUnion;
;
; 注意：C 代码通过 union 的 combine 视图访问 increment 和 wavetablePos_int
; 作为 uint16_t；ASM 通过 split 视图访问各字节。
; 但由于 8051 是大端序，combine.increment (uint16_t) 的低字节在低地址
; (increment_frac=偏移0)，高字节在高地址 (increment_int=偏移1)。
; 这一点与直观预期可能相反——SDCC 对 8051 使用大端存储。
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
;     PlayrAbsAddr (0x10) + PlayerTotalSize (14) + 额外的 3 字节保留
;     = 0x10 + 0x11 + 0x00 = 0x21
	SynthAbsAddr = 0x10+11+6
	SynthTotalSize = unitSz*POLY_NUM+3 ; 83
