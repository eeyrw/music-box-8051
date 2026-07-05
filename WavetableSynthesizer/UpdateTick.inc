;====================================================================
; UpdateTick.inc — 系统毫秒计数器
;
; 本文件由 PeriodTimer.s 通过 .include 引入到 Timer0 ISR 中。
;
;   sysMs++     — 32-bit 毫秒计数器 (32000/32=1000Hz)
;                 每 32 个 ISR tick 递增一次
;
; 地址: _sysMsPre (1字节) + _sysMs (4字节)
;       均分配在 DATA 段 (内部 RAM, 直接寻址)
;====================================================================

	.globl _sysMsPre
	.globl _sysMs

	.area DSEG    (DATA)
_sysMsPre::
	.ds 1
_sysMs::
	.ds 4

	.area CSEG    (CODE)
_UpdateTick:

	; ---- sysMs 32-bit 毫秒计数器 ----
	;     32000 / 1000 = 32 ticks/ms
	inc _sysMsPre
	mov a,_sysMsPre
	clr c
	subb a,#32
	jc sysMsEnd$
	mov _sysMsPre,#0

	mov a,_sysMs
	add a,#1
	mov _sysMs,a

	mov a,(_sysMs+1)
	addc a,#0
	mov (_sysMs+1),a

	mov a,(_sysMs+2)
	addc a,#0
	mov (_sysMs+2),a

	mov a,(_sysMs+3)
	addc a,#0
	mov (_sysMs+3),a

sysMsEnd$:
