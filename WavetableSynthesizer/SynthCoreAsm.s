;====================================================================
; SynthCoreAsm.s — 音色引擎汇编接口
;
; 提供两个对外函数：
;   _NoteOnAsm        — 在指定声道上触发一个新音符
;   _GenDecayEnvlopeAsm — 对所有正在发声的声道推进一格衰减包络
;
; 所有操作都直接读写 DATA 内存中 Synthesizer 结构体的绝对地址
; (SynthAbsAddr = 0x21)，与 C 代码共享同一块内存。
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
	.include "Player.inc"
	.globl _GenDecayEnvlopeAsm
	.globl _NoteOnAsm
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

		; ---- 条件 1: wavetablePos_int >= WAVETABLE_ATTACK_LEN ----
		; 16 位比较: 先比较低字节，再比较高字节
		mov a, (pSndUnit+pWavetablePos_int_l)
		clr c
		subb a,#WAVETABLE_ATTACK_LEN      ; 低字节减
		mov a, (pSndUnit+pWavetablePos_int_h)
		subb a,#(WAVETABLE_ATTACK_LEN>>8) ; 高字节减（带借位）
		jc 	loopGenDecayEnvlope_end'Idx'$ ; C=1 → 不够减 → 还没到起音段

		; ---- 条件 2: envelopePos < ENVELOP_LEN-1 ----
		mov a,(pSndUnit+pEnvelopePos)
		clr c
		subb a,#(ENVELOP_LEN-1)           ; a = envelopePos - 255
		jnc loopGenDecayEnvlope_end'Idx'$ ; C=0 → envelopePos >= 255 → 已到末尾

		; ---- 查表: envelopeLevel = EnvelopeTable[envelopePos] ----
		mov dptr,#_EnvelopeTable
		mov a,(pSndUnit+pEnvelopePos)
		movc a,@a+dptr                    ; a = EnvelopeTable[envelopePos]

		; ---- (可选) 全局音量缩放 ----
		; 取消下面三行的注释可以把整体音量降到 ~78%
		; mov b,#200                      ; 缩放因子 200/256 ≈ 0.78
		; mul ab                          ; a*b → b:a
		; mov a,b                         ; a = 高字节 = a*200/256

		mov (pSndUnit+pEnvelopeLevel),a   ; 写入新的包络幅度
		inc (pSndUnit+pEnvelopePos)       ; 包络位置 +1

		loopGenDecayEnvlope_end'Idx'$:    ; 本声道跳过
	.endm

	ret

;====================================================================
; _NoteOnAsm — 在新声道上触发一个音符
;
; 参数: dpl = MIDI 音符编号 (0-127)
;
; 流程：
;   1. 从 synth->lastSoundUnit 取得下一个可用声道号 (0-7 round-robin)
;   2. 计算该声道在 soundUnit 数组中的起始地址放入 r0
;   3. 关中断 (clr ea) — 防止 ISR 在设置中途读到半新半旧的状态
;   4. 查 WaveTable_Increment 表获取该音符的频率增量 (16 位)
;   5. 清零该声道所有状态值 (相位 = 0, 包络 = 255 全音量, 包络位置 = 0)
;   6. 开中断
;   7. lastSoundUnit = (lastSoundUnit + 1) % POLY_NUM
;
; 注意：_NoteOnAsm 内部操作在关中断保护下完成，保证 ISR (SynthAsm)
;       不会读到不一致的声道状态。
;====================================================================
_NoteOnAsm:

; ---- C 代码等效逻辑 ----
;   void NoteOn(Synthesizer* synth, uint8_t note) {
;       uint8_t lastSoundUnit = synth->lastSoundUnit;
;       disable_interrupts();
;       synth->SoundUnitUnionList[lastSoundUnit].combine.increment =
;           PitchIncrementTable[note & 0x7F];
;       synth->SoundUnitUnionList[lastSoundUnit].combine.wavetablePos_frac = 0;
;       synth->SoundUnitUnionList[lastSoundUnit].combine.wavetablePos_int = 0;
;       synth->SoundUnitUnionList[lastSoundUnit].combine.envelopePos = 0;
;       synth->SoundUnitUnionList[lastSoundUnit].combine.envelopeLevel = 255;
;       enable_interrupts();
;       lastSoundUnit = (lastSoundUnit + 1) % POLY_NUM;
;       synth->lastSoundUnit = lastSoundUnit;
;   }

	pSynth = SynthAbsAddr

	; ---- 1. 获取当前声道编号，计算该声道在数组中的基址 ----
	mov a,(pSynth+pLastSoundUnit)   ; a = lastSoundUnit (0-7)
	mov b,#unitSz                   ; b = 10 (每个声道的字节数)
	mul ab                          ; b:a = lastSoundUnit * 10
	add a,#pSynth                   ; a += Synthesizer 起始地址
	mov r0,a                        ; r0 = 目标声道的基址 (DATA 地址)

	; ---- 2. 关中断，进入临界区 ----
	clr ea

	; ---- 3. 查 PitchIncrementTable[note & 0x7F] ----
	; WaveTable_Increment 是一个 uint16_t 数组，每 2 字节一个值
	; 需要用 (note & 0x7F) * 2 作为偏移
	mov a,#0x7F
	anl a,dpl                       ; a = note & 0x7F (去掉 bit 7)
	rl a                            ; a = (note & 0x7F) * 2 (16-bit 偏移)
	mov r6,a                        ; r6 = 偏移量备份
	mov dptr,#_WaveTable_Increment
	movc a,@a+dptr                  ; a = 低字节 (increment_frac)
	mov r4,a                        ; r4 = increment_frac
	inc dptr
	mov a,r6                        ; 重新取偏移量 (movc 改了 a)
	movc a,@a+dptr                  ; a = 高字节 (increment_int)
	mov r5,a                        ; r5 = increment_int

	; ---- 4. 写入声道状态 ----
	;    通过间接寻址 r1 = r0 + offset, 写入 @r1

	; -- 写入 increment_frac (偏移 0) --
	mov a,#pIncrement_frac
	add a,r0
	mov r1,a
	mov a,r4
	mov @r1,a

	; -- 写入 increment_int (偏移 1) --
	mov a,#pIncrement_int
	add a,r0
	mov r1,a
	mov a,r5
	mov @r1,a

	; -- 写入 wavetablePos_frac = 0 (偏移 2) --
	;    这是之前 BUG 的位置：原来写成了 pEnvelopePos
	mov a,#pWavetablePos_frac
	add a,r0
	mov r1,a
	mov @r1,#0

	; -- 写入 wavetablePos_int_l = 0 (偏移 3) --
	mov a,#pWavetablePos_int_l
	add a,r0
	mov r1,a
	mov @r1,#0

	; -- 写入 wavetablePos_int_h = 0 (偏移 4) --
	mov a,#pWavetablePos_int_h
	add a,r0
	mov r1,a
	mov @r1,#0

	; -- 写入 envelopeLevel = 255 (偏移 5) --
	;    新音符从最大音量开始
	mov a,#pEnvelopeLevel
	add a,r0
	mov r1,a
	mov @r1,#255

	; -- 写入 envelopePos = 0 (偏移 6) --
	;    包络从第 0 步开始（对应最大音量）
	mov a,#pEnvelopePos
	add a,r0
	mov r1,a
	mov @r1,#0

	; ---- 5. 开中断，退出临界区 ----
	setb ea

	; ---- 6. lastSoundUnit = (lastSoundUnit + 1) % POLY_NUM ----
	mov r4,(pSynth+pLastSoundUnit)  ; 重新加载当前声道号
	inc r4                          ; r4 = lastSoundUnit + 1
	mov a,r4
	clr c
	subb a,#POLY_NUM               ; 比较 r4 和 8
	jnz lastSoundUnitUpdateEndNotEq$ ; r4 != 8 → 未溢出
	mov r4,#0                       ; r4 == 8 → 归零
lastSoundUnitUpdateEndNotEq$:
	mov (pSynth+pLastSoundUnit),r4  ; 写回 Synthesizer

	ret
