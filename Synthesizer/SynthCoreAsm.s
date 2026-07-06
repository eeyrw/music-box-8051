;====================================================================
; SynthCoreAsm.s — 音色引擎汇编接口
;
; 提供四个对外函数：
;   _NoteOnAsm        — 在指定声道上触发一个新音符
;   _NoteOffAsm       — 关闭指定 MIDI 音符对应的声道
;   _GenDecayEnvlopeAsm — 对所有正在衰减的声道推进包络 (含 velocity 缩放)
;   _SynthReleaseAllAsm — 释放所有声道
;
; 所有操作都直接读写 DATA 内存中 Synthesizer 结构体的绝对地址
; (SynthAbsAddr = 0x21)，与 C 代码共享同一块内存。
; VoiceState (midiNote + velocity) 存储在 XRAM (_voiceState)。
;
; 声音数据结构 (SoundUnitSplit)，每个声道 10 字节：
;   偏移 0: pIncrement_frac      — 相位增量小数部分 (uint8_t)
;   偏移 1: pIncrement_int       — 相位增量整数部分 (uint8_t)
;   偏移 2: pWavetablePos_frac   — 当前采样位置小数部分 (uint8_t)
;   偏移 3: pWavetablePos_int_l  — 当前采样位置整数低字节 (uint8_t)
;   偏移 4: pWavetablePos_int_h  — 当前采样位置整数高字节 (uint8_t)
;   偏移 5: pEnvelopeLevel       — 当前包络幅度 0-255 (uint8_t)
;   偏移 6: pEnvelopePos         — 包络表索引 0-255 (uint8_t)
;   偏移 7: pVal_l               — 临时值低字节 (int16_t)
;   偏移 8: pVal_h               — 临时值高字节 (int16_t)
;   偏移 9: pSampleVal           — 波形采样值 (int8_t)
;
; Synthesizer 结构体 (83 字节)：
;   偏移 0-79:  8 声道 × 10 字节
;   偏移 80-81: mixOut (int16_t) — 混合输出累加器
;   偏移 82:    lastSoundUnit    — 下次分配声道的编号 (0-7)
;====================================================================

	.module SYNTH_CORE_ASM
	.include "SynthCore.inc"
	.globl _GenDecayEnvlopeAsm
	.globl _NoteOnAsm
	.globl _NoteOffAsm
	.globl _SynthReleaseAllAsm
	.globl _synthForAsm
	.globl _voiceState

; ---- 在绝对地址声明 Synthesizer 实例 ----
	.area IABS    (ABS,DATA)
	.org SynthAbsAddr               ; SynthAbsAddr = 0x21
_synthForAsm::
	.ds SynthTotalSize              ; 83 字节 = 8×10 + 3

;====================================================================
; _GenDecayEnvlopeAsm — 推进所有声道的衰减包络 (含 velocity 缩放)
;
; 每 3ms 由 PlayerProcess 调用一次。
; 遍历 8 个声道，对 envelopePos < 255 的声道:
;   1. 读 EnvelopeTable[envelopePos]
;   2. 从 XRAM voiceState[voice].velocity 读缩放因子
;   3. envelopeLevel = (EnvelopeTable[pos] * velocity) / 256
;   4. envelopePos += RELEASE_STEP, 封顶 255 时强制 level=0
;====================================================================
	.area CSEG    (CODE)
_GenDecayEnvlopeAsm:

envelopUpdateEnd$:

	pSynth = SynthAbsAddr

	; ---- 使用宏展开，对 8 个声道逐一处理 ----
	.irp  Idx,0,1,2,3,4,5,6,7
		pSndUnit = pSynth+Idx*unitSz  ; 当前声道的基址

		; ---- 条件: envelopePos < ENVELOP_LEN-1 (255 = sustain) ----
		mov a,(pSndUnit+pEnvelopePos)
		clr c
		subb a,#(ENVELOP_LEN-1)
		jnc loopGenDecayEnvlope_end'Idx'$

		; ---- 查表: a = EnvelopeTable[envelopePos] ----
		mov dptr,#_EnvelopeTable
		mov a,(pSndUnit+pEnvelopePos)
		movc a,@a+dptr

		; ---- 读取 XRAM voiceState[Idx].velocity → b ----
		mov dptr,#(_voiceState + Idx*2 + 1)
		movx a,@dptr
		mov b,a

		; ---- 重新读 EnvelopeTable[pos] → a ----
		mov dptr,#_EnvelopeTable
		mov a,(pSndUnit+pEnvelopePos)
		movc a,@a+dptr

		; ---- 乘法: (EnvelopeTable[pos] * velocity) / 256 ----
		mul ab                          ; b:a = product, b = high byte = /256
		mov r4,b                        ; r4 = 缩放后的包络幅度

		; ---- 包络位置 + RELEASE_STEP, 溢出封顶到 255 ----
		mov a,(pSndUnit+pEnvelopePos)
		add a,#RELEASE_STEP
		jc releasePosCap'Idx'$
		cjne a,#(ENVELOP_LEN-1),releasePosWrite'Idx'$
releasePosCap'Idx'$:
		mov a,#(ENVELOP_LEN-1)
		mov r4,#0                       ; 衰减结束, 强制静音
releasePosWrite'Idx'$:
		mov (pSndUnit+pEnvelopePos),a
		mov (pSndUnit+pEnvelopeLevel),r4

		loopGenDecayEnvlope_end'Idx'$:
	.endm

	ret

;====================================================================
; _NoteOffAsm — 关闭指定 MIDI 音符的声道（触发衰减）
;
; 参数: dpl = MIDI 音符编号 (0-127)
;
; 遍历 8 个声道的 XRAM voiceState，找到 midiNote 匹配且
; envelopeLevel > 0 的声道，将 envelopePos 置为合适的衰减起点。
;====================================================================
_NoteOffAsm:
	pSynth = SynthAbsAddr

	mov r7, dpl                      ; r7 = 目标 MIDI 音符 (保存, dptr 操作会覆盖 dpl)
	mov r2, #0                       ; 声道索引
	mov r3, #pSynth                  ; DATA 声道基址

NoteOffScanLoop:
	; ---- 检查 envelopeLevel > 0 ----
	mov a, r3
	add a, #pEnvelopeLevel
	mov r1, a
	mov a, @r1
	jz NoteOffNextVoice

	; ---- 读取 XRAM voiceState[r2].midiNote ----
	mov dptr, #(_voiceState)
	mov a, r2
	rl a                             ; a = r2 * 2
	add a, dpl
	mov dpl, a
	clr a
	addc a, dph
	mov dph, a
	movx a, @dptr                    ; a = voiceState[r2].midiNote
	mov r4, a                        ; r4 = XRAM midiNote
	mov a, r7                        ; a = 原始保存的 dpl (目标音符)
	cjne a, 0x04, NoteOffNextVoice   ; 与 r4 (DATA 0x04) 比较

	; ---- 匹配: 查表找衰减起点 ----
	; 读 XRAM voiceState[r2].velocity (用于参考)
	inc dptr                         ; dptr → voiceState[r2].velocity
	movx a, @dptr
	mov r4, a                        ; r4 = velocity scale (备用)

	; 读当前 envelopeLevel
	mov a, r3
	add a, #pEnvelopeLevel
	mov r1, a
	mov a, @r1
	mov r5, a                        ; r5 = current level

	; 扫描 EnvelopeTable 找 table[pos] >= level 的最小 pos (步进 RELEASE_STEP)
	mov r6, #0                       ; r6 = scan pos
	mov dptr, #_EnvelopeTable
NoteOffScanLevel:
	mov a, r6
	movc a, @a+dptr                  ; a = EnvelopeTable[r6]
	clr c
	subb a, r5                       ; table[r6] - level
	jnc NoteOffFoundPos              ; table[r6] >= level → 找到

	mov a, r6
	add a, #RELEASE_STEP
	jc NoteOffCap255
	mov r6, a
	cjne a, #(ENVELOP_LEN-1), NoteOffScanLevel
NoteOffCap255:
	mov r6, #(ENVELOP_LEN-1)
NoteOffFoundPos:

	; 写 envelopePos = r6
	mov a, r3
	add a, #pEnvelopePos
	mov r1, a
	mov a, r6
	mov @r1, a

NoteOffNextVoice:
	mov a, r3
	add a, #unitSz
	mov r3, a
	inc r2
	cjne r2, #POLY_NUM, NoteOffScanLoop
	ret

;====================================================================
; _SynthReleaseAllAsm — 释放所有声道 (envelopeLevel = 0, envelopePos = 255)
;
; 在 Stop/Next/Prev/EndOfScore 时调用。
;====================================================================
_SynthReleaseAllAsm:
	pSynth = SynthAbsAddr

	mov r2, #0
	mov r3, #pSynth

ReleaseAllLoop:
	mov a, r3
	add a, #pEnvelopeLevel
	mov r1, a
	mov @r1, #0

	mov a, r3
	add a, #pEnvelopePos
	mov r1, a
	mov @r1, #255

	mov a, r3
	add a, #unitSz
	mov r3, a
	inc r2
	cjne r2, #POLY_NUM, ReleaseAllLoop
	ret

;====================================================================
; _NoteOnAsm — 在新声道上触发一个音符
;
; 参数 (SDCC non-reentrant): dpl = MIDI 音符, _NoteOnAsm_PARM_2 = velocity (0-255)
;
; 声道分配策略 (空闲优先 + Round-Robin 回退).
; 同时写入 voiceState[].midiNote 和 velocity (XRAM).
;====================================================================
_NoteOnAsm:

	pSynth = SynthAbsAddr

	; ---- 从栈上读取 velocity (8051 栈向上增长, velocity 在 SP-2) ----
	mov a, SP
	add a, #(256 - 2)                ; SP - 2 = velocity 地址 (8051 add 实现减法)
	mov r1, a
	mov a, @r1                       ; a = velocity (第二参数)
	mov r6, a                        ; r6 = velocity

	mov r5, dpl                      ; r5 = 原始 MIDI 音符 (保存, dptr 操作会覆盖 dpl)

	; ================================================================
	; 阶段 A: 扫描空闲声道 (envelopeLevel == 0)
	; 寄存器: r2=声道索引, r3=声道基址
	; ================================================================
	mov r2,#0
	mov r3,#pSynth
freeScanLoop:
	mov a,r3
	add a,#pEnvelopeLevel
	mov r1,a
	mov a,@r1
	jz useFreeVoice

	mov a,r3
	add a,#unitSz
	mov r3,a
	inc r2
	cjne r2,#POLY_NUM,freeScanLoop

	; ================================================================
	; 阶段 B: 无空闲声道，使用 round-robin (lastSoundUnit)
	; ================================================================
	mov a,(pSynth+pLastSoundUnit)
	sjmp voiceSelected

useFreeVoice:
	mov a,r2

voiceSelected:
	mov r7,a                         ; r7 = 选中的声道索引

	; ---- 计算声道基址: r0 = pSynth + 选中索引 * unitSz ----
	mov b,#unitSz
	mul ab
	add a,#pSynth
	mov r0,a                         ; r0 = 目标声道基址 (DATA)

	; ---- 关中断，进入临界区 ----
	clr ea

	; ---- 查 PitchIncrementTable[note & 0x7F] ----
	mov a,#0x7F
	anl a,r5                         ; r5 = 保存的原始音符
	rl a
	mov r4,a
	mov dptr,#_WaveTable_Increment
	movc a,@a+dptr
	mov r3,a                         ; increment_frac
	inc dptr
	mov a,r4
	movc a,@a+dptr
	mov r2,a                         ; increment_int

	; ---- 写入声道状态 ----

	mov a,#pIncrement_frac
	add a,r0
	mov r1,a
	mov a,r3
	mov @r1,a

	mov a,#pIncrement_int
	add a,r0
	mov r1,a
	mov a,r2
	mov @r1,a

	mov a,#pWavetablePos_frac
	add a,r0
	mov r1,a
	mov @r1,#0

	mov a,#pWavetablePos_int_l
	add a,r0
	mov r1,a
	mov @r1,#0

	mov a,#pWavetablePos_int_h
	add a,r0
	mov r1,a
	mov @r1,#0

	mov a,#pEnvelopeLevel
	add a,r0
	mov r1,a
	mov a,r6                         ; velocity
	mov @r1,a

	mov a,#pEnvelopePos
	add a,r0
	mov r1,a
	mov @r1,#255

	; ---- 写 XRAM voiceState[r7] = {midiNote, velocity} ----
	mov dptr,#_voiceState
	mov a,r7
	rl a                             ; a = r7 * 2
	add a,dpl
	mov dpl,a
	clr a
	addc a,dph
	mov dph,a
	mov a,r5                         ; 原始 MIDI 音符
	movx @dptr,a                     ; voiceState[r7].midiNote = note

	inc dptr                         ; dptr → voiceState[r7].velocity
	mov a,r6                         ; velocity
	movx @dptr,a                     ; voiceState[r7].velocity = velocity

	; ---- 开中断，退出临界区 ----
	setb ea

	; ---- lastSoundUnit = (选中索引 + 1) % POLY_NUM ----
	mov a,r7
	inc a
	cjne a,#POLY_NUM,noWrap
	clr a
noWrap:
	mov (pSynth+pLastSoundUnit),a

	ret
