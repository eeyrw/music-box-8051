;====================================================================
; Player.inc — Player 结构体字段偏移与地址定义
;
; Player 是乐谱播放器的运行时状态，位于 DATA 内存的绝对地址。
;
; 内存位置: PlayrAbsAddr = 0x10
;           紧挨在寄存器 Bank 0 (0x00-0x07) 和 Bank 1 (0x08-0x0F) 之后
;
; 字段:
;    偏移 0:  status             — 播放状态 (STATUS_STOP/READY/PLAYING)
;    偏移 1-4: lastScoreTick     — 下一个音符的触发时刻 (uint32_t)
;    偏移 5-7: scorePointer      — 乐谱数组的当前读取指针 (__code 3-byte指针)
;    偏移 8-10:synthesizerPointer— 指向 Synthesizer 的指针 (__data 3-byte 指针)
;====================================================================

; ---- C 等效结构体 ----
;   typedef struct _Player {
;       uint8_t status;                  // 偏移 0
;       uint32_t lastScoreTick;          // 偏移 1-4
;       uint8_t *scorePointer;           // 偏移 5-7 (__code 指针 → 3 字节)
;       Synthesizer *synthesizerPointer;  // 偏移 8-10 (__data 指针 → 3 字节)
;   } Player;

; ---- 字段偏移 ----
	pStatus=0                          ; 播放状态
	pLastScoreTick_b3=1                ; lastScoreTick 字节 3 (最高字节)
	pLastScoreTick_b2=2                ; lastScoreTick 字节 2
	pLastScoreTick_b1=3                ; lastScoreTick 字节 1
	pLastScoreTick_b0=4                ; lastScoreTick 字节 0 (最低字节)
	pScorePointer=5                    ; 乐谱指针 (3 字节)
	pMainSynthesizer=8                 ; Synthesizer 指针 (3 字节)

; ---- Player 在 DATA 内存的绝对地址和大小 ----
	PlayrAbsAddr = 0x0010              ; 绝对地址 0x10
	PlayerTotalSize = pMainSynthesizer+3 ; 总大小 = 11 字节
