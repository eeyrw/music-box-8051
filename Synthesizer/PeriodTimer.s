;====================================================================
; PeriodTimer.s — Timer0 中断服务程序 (ISR) 入口
;
; 系统核心定时源: 以 32000 Hz (约 31.25 μs 间隔) 触发 Timer0 中断。
; 在此 ISR 中调用 SynthAsm (合成) 和 UpdateTick (节拍)。
;
; 寄存器保护策略:
;   本 ISR 使用寄存器 Bank 1 (psw = 0x08, 对应地址 0x08-0x0F)。
;   因为 C 代码使用 Bank 0 (地址 0x00-0x07)，切换 bank 可以避免
;   每次进入 ISR 都保存/恢复 R0-R7 的 8×2=16 周期开销。
;
;   仍需保存的寄存器:
;     acc, b, dpl, dph, psw — 这些是共用的特殊功能寄存器
;
; 时序分析引脚:
;   P55 在 Synth+UpdateTick 代码前后翻转，用于示波器测量 ISR 执行时间
;====================================================================

	.include "SynthCore.inc"
	.include "8051.inc"

	.module PERIOD_TIMER

	; ---- 全局函数声明 ----
	.globl _timer_isr

	; ---- 保留 Bank 1 的 8 字节寄存器空间 ----
	.area REG_BANK_1	(REL,OVR,DATA)
	.ds 8                             ; 保留 R0-R7 (0x08-0x0F)

	.area CSEG    (CODE)
_timer_isr:

	; ---- 给 SDCC 汇编器声明 Bank 1 的寄存器别名 ----
	ar7 = 0x0f
	ar6 = 0x0e
	ar5 = 0x0d
	ar4 = 0x0c
	ar3 = 0x0b
	ar2 = 0x0a
	ar1 = 0x09
	ar0 = 0x08

	; ---- 保存 C 代码上下文 (Bank 0) ----
	;     acc, b, dpl, dph, psw 是 Bank 无关的公共寄存器
	push	acc
	push	b
	push	dpl
	push	dph
	push	psw                        ; 保存当前 psw (包含 Bank 选择和标志)

	; ---- 切换到 Bank 1 ----
	;     此后 R0-R7 使用 0x08-0x0F，不会破坏 C 代码的 R0-R7 (0x00-0x07)
	mov	psw,#0x08

	; ---- 时序测量引脚: 开始 ----
	;     在示波器上观察: 上升沿 = ISR 开始
	setb P55

	; ---- 调用音频合成 ----
	;     .include 将 Synth.inc 的代码直接内联到这里
	.include "Synth.inc"

	; ---- 调用节拍计数器 ----
	;     .include 将 UpdateTick.inc 的代码直接内联到这里
	.include "UpdateTick.inc"

	; ---- 时序测量引脚: 结束 ----
	;     下降沿 = ISR 结束
	clr P55

	; ---- 恢复 C 代码上下文 ----
	pop	psw                            ; 恢复 Bank 选择和标志
	pop	dph
	pop	dpl
	pop	b
	pop	acc

	; ---- 从中断返回 ----
reti
