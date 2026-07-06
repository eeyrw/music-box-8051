;====================================================================
; SynthCoreAsm.s — 音色引擎汇编接口
;
; 提供三个对外函数：
;   _NoteOnAsm        — 在指定声道上触发一个新音符
;   _NoteOffAsm       — 关闭指定 MIDI 音符对应的声道
;   _GenDecayEnvlopeAsm — 对所有正在发声的声道推进一格衰减包络
;
; 所有操作都直接读写 DATA 内存中 Synthesizer 结构体的绝对地址
; (SynthAbsAddr = 0x21)，与 C 代码共享同一块内存。
;
; 声音数据结构 (SoundUnitSplit)，每个声道 11 字节：
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
;   偏移 10: pMidiNote           — MIDI 音符编号 (uint8_t, NoteOff 查找用)
;
; Synthesizer 结构体 (91 字节)：
;   偏移 0-87:  8 声道 × 11 字节
;   偏移 88-89: mixOut (int16_t) — 混合输出累加器
;   偏移 90:    lastSoundUnit    — 下次分配声道的编号 (0-7)
;====================================================================

	.module SYNTH_CORE_ASM
	.include "SynthCore.inc"
	.globl _GenDecayEnvlopeAsm
	.globl _NoteOnAsm
	.globl _NoteOffAsm
	.globl _SynthReleaseAllAsm
	.globl _synthForAsm

; ---- 在绝对地址声明 Synthesizer 实例 ----
	.area IABS    (ABS,DATA)
	.org SynthAbsAddr               ; SynthAbsAddr = 0x21
_synthForAsm::
	.ds SynthTotalSize              ; 83 字节 = 8×10 + 3

;====================================================================
; _GenDecayEnvlopeAsm — 推进所有声道的衰减包络
;
; 每 DECAY_TIME_FACTOR (100) 个 tick 由 PlayerProcess 调用一次。
; 遍历 8 个声道，对满足条件的声道：
;   1. wavetablePos_int >= WAVETABLE_ATTACK_LEN (21260)
;      → 采样位置已经过了起音段
;   2. envelopePos < ENVELOP_LEN-1 (255)
;      → 包络还没有走到末尾
;   满足则从 EnvelopeTable[] 取 envelopeLevel 值，envelopePos++。
;
; EnvelopeTable 是一张 256 元素衰减表：从 255 逐渐衰减到 0，
; 创造类似乐器的自然的音量衰减曲线。
;====================================================================
	.area CSEG    (CODE)
_GenDecayEnvlopeAsm:

; ---- C 代码等效逻辑 ----
;   SoundUnitUnion* soundUnionList = &(synth->SoundUnitUnionList[0]);
;   for (uint8_t i = 0; i < POLY_NUM; i++) {
;       if (soundUnionList[i].combine.wavetablePos_int >= WAVETABLE_ATTACK_LEN &&
;           soundUnionList[i].combine.envelopePos < sizeof(EnvelopeTable)-1) {
;           soundUnionList[i].combine.envelopeLevel = EnvelopeTable[soundUnionList[i].combine.envelopePos];
;           soundUnionList[i].combine.envelopePos += 1;
;       }
;   }

envelopUpdateEnd$:

	pSynth = SynthAbsAddr

	; ---- 使用宏展开，对 8 个声道逐一处理 ----
	.irp  Idx,0,1,2,3,4,5,6,7
		pSndUnit = pSynth+Idx*unitSz  ; 当前声道的基址

		; ---- 条件: envelopePos < ENVELOP_LEN-1 (255 = sustain, <255 = 衰减中) ----
		mov a,(pSndUnit+pEnvelopePos)
		clr c
		subb a,#(ENVELOP_LEN-1)           ; a = envelopePos - 255
		jnc loopGenDecayEnvlope_end'Idx'$ ; C=0 → envelopePos >= 255 → 不衰减

		; ---- 查表: envelopeLevel = EnvelopeTable[envelopePos] ----
		mov dptr,#_EnvelopeTable
		mov a,(pSndUnit+pEnvelopePos)
		movc a,@a+dptr                    ; a = EnvelopeTable[envelopePos]
		mov r4,a                          ; r4 = 新包络幅度

		; ---- 包络位置 + RELEASE_STEP, 溢出封顶到 255, 同时清零包络 ----
		mov a,(pSndUnit+pEnvelopePos)
		add a,#RELEASE_STEP
		jc releasePosCap'Idx'$
		cjne a,#(ENVELOP_LEN-1),releasePosWrite'Idx'$
releasePosCap'Idx'$:
		mov a,#(ENVELOP_LEN-1)
		mov r4,#0                         ; 衰减结束, 强制静音
releasePosWrite'Idx'$:
		mov (pSndUnit+pEnvelopePos),a
		mov (pSndUnit+pEnvelopeLevel),r4

		loopGenDecayEnvlope_end'Idx'$:    ; 本声道跳过
	.endm

	ret

;====================================================================
; _NoteOffAsm — 关闭指定 MIDI 音符的声道（触发衰减）
;
; 参数: dpl = MIDI 音符编号 (0-127)
;
; 遍历 8 个声道，找到 envelopeLevel > 0 且 midiNote 匹配的声道，
; 将 envelopePos 置 0，触发 GenDecayEnvlopeAsm 开始自然衰减。
; 不修改 wavetablePos，避免波形跳变。
;====================================================================
_NoteOffAsm:
	pSynth = SynthAbsAddr

	mov r2, #0
	mov r3, #pSynth

NoteOffScanLoop:
	; ---- 检查 envelopeLevel > 0 (声道活跃) ----
	mov a, r3
	add a, #pEnvelopeLevel
	mov r1, a
	mov a, @r1
	jz NoteOffNextVoice

	; ---- 检查 midiNote 匹配 ----
	mov a, r3
	add a, #pMidiNote
	mov r1, a
	mov a, @r1
	cjne a, dpl, NoteOffNextVoice

	; ---- 匹配: envelopePos = 0 (触发衰减) ----
	mov a, r3
	add a, #pEnvelopePos
	mov r1, a
	mov @r1, #0
	; 继续扫描，同一音符可能多次触发，全部释放

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
; 在 Stop/Next/Prev 时调用，立即静音并阻止衰减循环重启。
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
; 参数: dpl = MIDI 音符编号 (0-127)
;
; 声道分配策略 (空闲优先 + Round-Robin 回退):
;   阶段 A: 遍历 8 声道，找 envelopeLevel==0 (完全静音) 的空闲声道
;   阶段 B: 若无空闲声道，使用 lastSoundUnit (FIFO round-robin)
;
;   注意: 不使用"最静偷取"是因为高音 phase increment 更大，
;   wavetablePos 更快达到 WAVETABLE_ATTACK_LEN，包络衰减更早启动，
;   导致 envelopeLevel 比较时系统性歧视高音。
;   Round-robin (FIFO) 偷最早分配的声道，与音高无关，更公平。
;
;   时序约束: NoteOnAsm 运行在主循环，无严格实时要求。
;====================================================================
_NoteOnAsm:

; ---- C 代码等效逻辑 ----
;   void NoteOn(Synthesizer* synth, uint8_t note) {
;       uint8_t idx;
;       // 阶段 A: 找空闲声道
;       for (uint8_t i = 0; i < POLY_NUM; i++) {
;           if (synth->SoundUnitUnionList[i].split.envelopeLevel == 0) {
;               idx = i; goto voiceFound;
;           }
;       }
;       // 阶段 B: 无空闲 → round-robin
;       idx = synth->lastSoundUnit;
;   voiceFound:
;       disable_interrupts();
;       synth->SoundUnitUnionList[idx].combine.increment =
;           PitchIncrementTable[note & 0x7F];
;       synth->SoundUnitUnionList[idx].combine.wavetablePos_frac = 0;
;       synth->SoundUnitUnionList[idx].combine.wavetablePos_int = 0;
;       synth->SoundUnitUnionList[idx].combine.envelopePos = 0;
;       synth->SoundUnitUnionList[idx].combine.envelopeLevel = 255;
;       enable_interrupts();
;       synth->lastSoundUnit = (idx + 1) % POLY_NUM;
;   }

	pSynth = SynthAbsAddr

	; ================================================================
	; 阶段 A: 扫描空闲声道 (envelopeLevel == 0)
	; 寄存器: r2=声道索引, r3=声道基址
	; ================================================================
	mov r2,#0                        ; r2 = 声道索引 0..7
	mov r3,#pSynth                   ; r3 = 声道 0 基址
freeScanLoop:
	mov a,r3
	add a,#pEnvelopeLevel
	mov r1,a
	mov a,@r1                        ; a = envelopeLevel
	jz useFreeVoice                  ; 找到空闲声道

	mov a,r3
	add a,#unitSz
	mov r3,a
	inc r2
	cjne r2,#POLY_NUM,freeScanLoop

	; ================================================================
	; 阶段 B: 无空闲声道，使用 round-robin (lastSoundUnit)
	; ================================================================
	mov a,(pSynth+pLastSoundUnit)    ; a = lastSoundUnit (0-7)
	sjmp voiceSelected

useFreeVoice:
	mov a,r2                         ; a = 空闲声道索引

voiceSelected:
	mov r7,a                         ; r7 = 选中的声道索引 (保存用于后续更新 lastSoundUnit)

	; ---- 计算声道基址: r0 = pSynth + 选中索引 * unitSz ----
	mov b,#unitSz
	mul ab                           ; b:a = 索引 * 10
	add a,#pSynth
	mov r0,a                         ; r0 = 目标声道基址 (DATA 地址)

	mov r2,dpl                       ; r2 = 原始 MIDI 音符编号 (在 dptr 被覆盖前保存)

	; ---- 关中断，进入临界区 ----
	clr ea

	; ---- 查 PitchIncrementTable[note & 0x7F] ----
	mov a,#0x7F
	anl a,dpl                        ; a = note & 0x7F (去掉 bit 7)
	rl a                             ; a = (note & 0x7F) * 2 (16-bit 偏移)
	mov r6,a                         ; r6 = 偏移量备份
	mov dptr,#_WaveTable_Increment
	movc a,@a+dptr                   ; a = 低字节 (increment_frac)
	mov r4,a                         ; r4 = increment_frac
	inc dptr
	mov a,r6                         ; 重新取偏移量 (movc 改了 a)
	movc a,@a+dptr                   ; a = 高字节 (increment_int)
	mov r5,a                         ; r5 = increment_int

	; ---- 写入声道状态 ----

	; -- increment_frac (偏移 0) --
	mov a,#pIncrement_frac
	add a,r0
	mov r1,a
	mov a,r4
	mov @r1,a

	; -- increment_int (偏移 1) --
	mov a,#pIncrement_int
	add a,r0
	mov r1,a
	mov a,r5
	mov @r1,a

	; -- wavetablePos_frac = 0 (偏移 2) --
	mov a,#pWavetablePos_frac
	add a,r0
	mov r1,a
	mov @r1,#0

	; -- wavetablePos_int_l = 0 (偏移 3) --
	mov a,#pWavetablePos_int_l
	add a,r0
	mov r1,a
	mov @r1,#0

	; -- wavetablePos_int_h = 0 (偏移 4) --
	mov a,#pWavetablePos_int_h
	add a,r0
	mov r1,a
	mov @r1,#0

	; -- envelopeLevel = 255 (偏移 5) --
	mov a,#pEnvelopeLevel
	add a,r0
	mov r1,a
	mov @r1,#255

	; -- envelopePos = 255 (sustain, 等待 NoteOff 触发衰减) (偏移 6) --
	mov a,#pEnvelopePos
	add a,r0
	mov r1,a
	mov @r1,#255

	; -- midiNote = r2 (原始 MIDI 音符) (偏移 10) --
	mov a,#pMidiNote
	add a,r0
	mov r1,a
	mov a,r2
	mov @r1,a

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
