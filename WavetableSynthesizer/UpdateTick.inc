;====================================================================
; UpdateTick.inc — 32-bit 节拍计数器
;
; 本文件由 PeriodTimer.s 通过 .include 引入到 Timer0 ISR 中。
; 定义 _UpdateTick 标签，完成两个定时任务：
;
;   1. currentTick++     — 32-bit 无符号整数，每 32kHz tick 递增
;   2. decayGenTick++    — 8-bit 衰减发生器滴答计数器
;      当 decayGenTick < DECAY_TIME_FACTOR (100) 时递增，
;      达到100后停止，由 PlayerProcess 读取后清零
;
; currentTick 的高 24 位 (currentTick>>8) 被 PlayerProcess 用于
; 与 lastScoreTick 比较以确定乐谱节拍
;
; 地址: _decayGenTick (1字节) + _currentTick (4字节)
;       均分配在 DATA 段 (内部 RAM, 直接寻址)
;====================================================================

	.globl _decayGenTick
	.globl _currentTick

	.include "SynthCore.inc"

	; ---- 在 DATA 段分配全局变量 ----
	.area DSEG    (DATA)
_decayGenTick::
	.ds 1                             ; 衰减滴答计数器 (uint8_t)
_currentTick::
	.ds 4                             ; 32-bit 主节拍计数器 (uint32_t)

	; ---- 代码段 ----
	.area CSEG    (CODE)
_UpdateTick:

; ---- C 代码等效逻辑 ----
;   void UpdateTick(void) {
;       currentTick++;
;       if (decayGenTick < DECAY_TIME_FACTOR)
;           decayGenTick++;
;   }

	; ---- 1. currentTick++ (32-bit 递增) ----
	;     从低字节开始逐字节递增。
	;     inc _currentTick 可替代 mov+add+mov 但 inc 不设进位标志，
	;     这里用 addc 链保证进位正确传播
	mov a,_currentTick                ; 加载低字节
	add a,#1                          ; 低字节 + 1
	mov _currentTick,a                ; 写回

	mov a,(_currentTick+1)            ; 字节 1
	addc a,#0                         ; + 0 + 进位
	mov (_currentTick+1),a

	mov a,(_currentTick+2)            ; 字节 2
	addc a,#0
	mov (_currentTick+2),a

	mov a,(_currentTick+3)            ; 字节 3 (最高字节)
	addc a,#0
	mov (_currentTick+3),a

	; ---- 2. 衰减滴答计数器 ----
	;     if (decayGenTick < 100) decayGenTick++;
	mov a,#DECAY_TIME_FACTOR          ; a = 100
	clr cy
	subb a,_decayGenTick              ; a = 100 - decayGenTick, C = (100 < decayGenTick)
	; subb 结果: 如果 decayGenTick < 100,
	;            则 100 ≥ decayGenTick+1 → 进位被消耗 → C=0
	;            但 subb 语义是 (a - mem - C), C=0 初始
	;            so a - decayGenTick: 够减则 C=0, 不够减则 C=1
	;            结论: C=1 → 100 < decayGenTick → 跳过递增
	jc updateDecayGenTickEnd$         ; 已达到 100, 跳过
	inc _decayGenTick                 ; 未达到 100, decayGenTick++

updateDecayGenTickEnd$:
	; 注意: 此处无 ret — .include 到 PeriodTimer.s 中，
	; 后续代码会流到 timer_isr 的 epilogue
