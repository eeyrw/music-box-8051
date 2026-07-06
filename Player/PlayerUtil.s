;====================================================================
; PlayerUtil.s — Player 结构体内存分配和 ASM 桩函数
;
; 本文件有两个功能：
;
; 1. 在绝对地址声明 _mainPlayer 结构体
;    占用 DATA 内存从 PlayrAbsAddr (0x10) 开始的 PlayerTotalSize 字节。
;    这是全局唯一的 Player 实例，C 代码通过 extern __data Player mainPlayer
;    访问，汇编代码通过 PlayrAbsAddr 常量访问。
;
; 2. 提供两个 ASM 桩函数:
;    _PlayNoteTimingCheck 和 _PlayUpdateNextScoreTick
;    这两函数编译入最终二进制但实际不执行——C 代码在 Player.c 中有
;    同名逻辑 (直接内联的逻辑，但函数名不同)。
;    桩函数只包含 ret，汇编逻辑已注释。
;
; 注意: 本文件不能移除——移除会导致 _mainPlayer 结构体缺失，
;  C 代码将无法找到 Player 实例。
;====================================================================

	.module PLAYER_UTILS

	; ---- 全局符号 ----
	.globl _PlayNoteTimingCheck
	.globl _PlayUpdateNextScoreTick

	.include "SynthCore.inc"
	.include "Player.inc"

; =====================================================================
; 绝对地址段: 声明 Player 实例 _mainPlayer
; =====================================================================
	.area IABS    (ABS,DATA)
	.org PlayrAbsAddr                  ; 0x10
_mainPlayer::
	.ds PlayerTotalSize               ; 14 字节 (11 + 3 对齐)

	.area CSEG    (CODE)

; =====================================================================
; _PlayNoteTimingCheck — 乐谱节拍检查 (桩函数)
;
; 检查当前节拍是否已达到下一个音符的触发时刻:
;   if ((currentTick >> 8) >= player->lastScoreTick) return 0xFF;
;   else return 0;
;
; C 代码 Player.c 中直接用内联逻辑替代，此桩只返回。
; =====================================================================
_PlayNoteTimingCheck:
	; ---- 已注释的 ASM 实现 (ST 指令集, 非 8051) ----
	;   if((currentTick>>8)>=player->lastScoreTick) {
	;       ldw y,(0x03, sp)       ; Load player pointer from stack
	;       ld a,(pLastScoreTick_b2,y)
	;       cp a,_currentTick+0    ; Compare byte 2 of lastScoreTick vs currentTick+0
	;       jrc playNoteTimingCheckEndReturnTrue$
	;       ldw x,y
	;       ldw x,(pLastScoreTick_b1,x)
	;       cpw x,_currentTick+1   ; Compare bytes 0-1 of lastScoreTick vs currentTick+1
	;       jrc playNoteTimingCheckEndReturnTrue$
	;       clr a                  ; return 0 (not time yet)
	;       ret
	;   playNoteTimingCheckEndReturnTrue$:
	;       ld a,#0xff             ; return 0xFF (time to play)
	;   }
	ret

; =====================================================================
; _PlayUpdateNextScoreTick — 推进乐谱指针到下一个音符 (桩函数)
;
; 从乐谱读取 delta-time 并累加到 lastScoreTick:
;   tempU32 = player->lastScoreTick;
;   do {
;       temp = *(player->scorePointer);
;       player->scorePointer++;
;       tempU32 += temp;
;   } while (temp == 0xFF);
;   player->lastScoreTick = tempU32;
;
; C 代码 Player.c 中的 PlayUpdateNextScoreTickP() 替代此逻辑。
; =====================================================================
_PlayUpdateNextScoreTick:
	; ---- 已注释的 ASM 实现 (ST 指令集, 非 8051) ----
	;   push a                         ; 保留 a 在栈上
	;   ldw y,(0x03+1, sp)            ; Load player pointer
	; varTickAccumulateLoop$:
	;   ldw x,y
	;   ldw x,(pScorePointer,x)       ; x = player->scorePointer
	;   ld a,(x)
	;   incw x
	;   ldw (pScorePointer,y),x       ; player->scorePointer++
	;   ld (0x01,sp),a                ; temp = *scorePointer
	;
	;   ; lastScoreTick += temp (4 byte add)
	;   ld a,(pLastScoreTick_b0,y)
	;   add a,(0x01,sp)
	;   ld (pLastScoreTick_b0,y),a
	;   ld a,(pLastScoreTick_b1,y)
	;   adc a,#0
	;   ld (pLastScoreTick_b1,y),a
	;   ld a,(pLastScoreTick_b2,y)
	;   adc a,#0
	;   ld (pLastScoreTick_b2,y),a
	;   ld a,(pLastScoreTick_b3,y)
	;   adc a,#0
	;   ld (pLastScoreTick_b3,y),a
	;
	;   ld a,(0x01,sp)
	;   cp a,#0xFF                     ; if temp == 0xFF goto loop
	;   jreq varTickAccumulateLoop$
	;
	;   pop a
	ret
