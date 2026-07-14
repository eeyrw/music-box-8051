;====================================================================
; Synth.inc — 8 声道波表合成引擎 (ISR 热路径)
;
; 本文件由 PeriodTimer.s 通过 .include 引入到 Timer0 ISR 中，
; 以 32000 Hz 的频率被调用。定义 _SynthAsm 标签，实现：
;
;   1. 遍历 8 个声道，对每个有 envelopeLevel > 0 的声道：
;      a. 从 WaveTable 以 16.8 定点相位读数
;      b. 线性插值（相邻采样点之间的小数部分插值）
;      c. 将采样值 (signed) 乘包络 (unsigned) → 除以 256
;      d. 累加到 16-bit mixOut
;      e. 推进相位 (frac+int)，检查并处理循环点
;   2. 对所有声道处理完后：
;      a. 应用主循环准备好的动态压缩增益
;      b. 最终限幅到 [-128, 127]
;      c. 加 DC 偏移 (+128) 以适配无符号 PWM
;      d. 写入 PWMA_CCR2 PWM 占空比寄存器 (8-bit DAC 输出)
;
; 寄存器约定 (Bank 1, 0x08-0x0F):
;   r0   — 插值暂存 (A = WaveTable[pos])
;   r4   — envelopeLevel (每声道从头持有到尾)
;   r5   — mixOut 累加器低字节 (跨声道持有)
;   r6   — mixOut 累加器高字节 (跨声道持有)
;   r1-r3,r7 — 未使用
;
; 算法参数 (WaveTable.inc):
;   波表: Celesta C5, 32000 Hz 采样率, 523 Hz 基频
;   WAVETABLE_LEN        = 21869  (循环段起点)
;   WAVETABLE_ATTACK_LEN = 21260  (起音段长度)
;   WAVETABLE_LOOP_LEN   = 609    (循环段长度)
;   WAVETABLE_ACTUAL_LEN = 21870  (总采样点数，多 1 用于插值)
;
; 宏机制：使用 .irp 将 8 个声道的处理循环展开为 8 段独立代码。
;
; 速度优化:
;   [1] mixOut 累加在 r6:r5 而非内存 — 省 6 周期/声道 (init + post + per-voice)
;   [2] xch a,b 替代 mov a,b — 省 2 周期/乘 × 8×2 = 16 周期

;   [3] add a,#128 替代 subb — 省 2 周期
;   合计: ~42 周期 → 约 7% 加速
;====================================================================

	.area CSEG    (CODE)
_SynthAsm:
	pSynth = SynthAbsAddr

	; ---- 清零 mixOut 累加到寄存器 (2 周期 vs 原本 4 周期内存清零) ----
	mov r5,#0                         ; r5 = mixOut_l = 0
	mov r6,#0                         ; r6 = mixOut_h = 0

;====================================================================
; 8 声道合成循环 (宏展开)
;====================================================================

.irp  Idx,0,1,2,3,4,5,6,7
	pSndUnit = pSynth+Idx*unitSz

	; 阶段 A: 包络检查
	mov a, (pSndUnit+pEnvelopeLevel)  ; 1
	mov r4,a                           ; 1 (envelope 暂存)
	jz soundUnitProcessEnd'Idx'$       ; 2 (不跳 = 4 周期)

	; 阶段 B: 构造 DPTR = WaveTable 基址 + pos_int (5 周期)
	mov dpl, (pSndUnit+pWavetablePos_int_l)  ; 2
	mov a,(pSndUnit+pWavetablePos_int_h)      ; 1
	add a,#(_WaveTable>>8)                     ; 1
	mov dph,a                                   ; 1

	; 读 WaveTable[pos] (3 周期)
	mov a,#(_WaveTable)                ; 1
	movc a,@a+dptr                     ; 2

.ifne USE_LINEAR_INTEROP
	; 线性插值 sample = A + (B-A)*frac/256  (19 周期, 正分支)
	mov r0,a                            ; 1 (r0 = A)

	mov a,#(_WaveTable+1)              ; 1
	movc a,@a+dptr                     ; 2 (a = B)

	clr c                              ; 1
	subb a,r0                          ; 1 (a = B - A, signed)
	mov b, (pSndUnit+pWavetablePos_frac) ; 2 (b = frac 0-255)

	; 有符号 × 无符号乘法 (B-A)*frac
	jb a.7,signedMulNegBranch0'Idx'$   ; 2 (jump not taken → positive path)

	; -- 正路径 --
	mul ab                             ; 4
	xch a,b                            ; 1 (★ 优化 [2]: 省 1 周期 vs mov a,b)
	sjmp signedMulEnd0'Idx'$           ; 2

signedMulNegBranch0'Idx'$:
	; -- 负路径 --
	cpl a                              ; 1
	inc a                              ; 1 (a = |B-A|)
	mul ab                             ; 4
	cpl a                              ; 1
	add a,#1                           ; 2 (取反乘积低字节, 设 CY)
	xch a,b                            ; 1 (★ 优化 [2]: 省 1 周期)
	cpl a                              ; 1
	addc a,#0                          ; 2 (取反乘积高字节, 带进位)
signedMulEnd0'Idx'$:

	add a,r0                           ; 1 (a = A + 插值项)
.endif ; USE_LINEAR_INTEROP

	; 阶段 C: 包络乘法 — sample(signed) × envelope(unsigned) / 256 (11 周期, 正分支)
	mov b,r4                           ; 2 (b = envelopeLevel)

	jb a.7,signedMulNegBranch'Idx'$    ; 2 (jump not taken → positive path)

	; -- sample 为正路径 --
	mul ab                             ; 4
	xch a,b                            ; 1 (★ 优化 [2]: 省 1 周期)
	mov b,#0                           ; 2 (b = 0x00 符号扩展)
	sjmp signedMulEnd'Idx'$            ; 2

signedMulNegBranch'Idx'$:
	; -- sample 为负路径 --
	cpl a                              ; 1
	inc a                              ; 1 (a = |sample|)
	mul ab                             ; 4
	cpl a                              ; 1
	add a,#1                           ; 2 (取反乘积低字节)
	xch a,b                            ; 1 (★ 优化 [2]: 省 1 周期)
	cpl a                              ; 1
	addc a,#0                          ; 2 (取反乘积高字节)
	mov b,#0xFF                        ; 2 (b = 0xFF 符号扩展)
signedMulEnd'Idx'$:

	; 阶段 D: 累加到 mixOut (5 周期, ★ 优化 [1])
	; 原版: 6 周期 (add+mov 到内存 + mov a,b + addc+mov 到内存)
	add a,r5                           ; 1 (mixOut_l += result)
	mov r5,a                           ; 1
	mov a,r6                           ; 1 (加载 mixOut_h, MOV A,Rn = 1 周期)
	addc a,b                           ; 1 (mixOut_h += sign_ext + carry)
	mov r6,a                           ; 1

	; 阶段 E: 相位推进 (9 周期)
	mov a,(pSndUnit+pIncrement_frac)   ; 1
	add a,(pSndUnit+pWavetablePos_frac) ; 1
	mov (pSndUnit+pWavetablePos_frac),a ; 1

	mov a,(pSndUnit+pIncrement_int)    ; 1
	addc a,(pSndUnit+pWavetablePos_int_l) ; 1
	mov (pSndUnit+pWavetablePos_int_l),a ; 1

	mov a,#0                           ; 1
	addc a,(pSndUnit+pWavetablePos_int_h) ; 1
	mov (pSndUnit+pWavetablePos_int_h),a ; 1

	; 阶段 F: 循环检查 (7 周期, 不卷绕时)
	clr c
	mov a,(pSndUnit+pWavetablePos_int_l) ; 1
	subb a,#(WAVETABLE_LEN)              ; 1
	mov a,(pSndUnit+pWavetablePos_int_h) ; 1
	subb a,#((WAVETABLE_LEN)>>8)         ; 1
	jc waveTablePosAdjustEnd'Idx'$       ; 2 (跳: pos < 21869)

	; -- 卷绕回循环段 (6 周期, 极低频率触发) --
	clr c
	mov a,(pSndUnit+pWavetablePos_int_l) ; 1
	subb a,#WAVETABLE_LOOP_LEN           ; 1
	mov (pSndUnit+pWavetablePos_int_l),a ; 1
	mov a,(pSndUnit+pWavetablePos_int_h) ; 1
	subb a, #(WAVETABLE_LOOP_LEN>>8)     ; 1
	mov (pSndUnit+pWavetablePos_int_h),a ; 1
waveTablePosAdjustEnd'Idx'$:
soundUnitProcessEnd'Idx'$:

.endm  ; .irp 结束

	; ---- 将原始 mixOut 写回内存供 VisualizeSound() 读取 (4 周期) ----
	;     在 >>1/裁剪/DC偏移之前写回，保持与原版一致: Visualization 用原始值
	mov (pSynth+pMixOut_l),r5         ; 2
	mov (pSynth+pMixOut_h),r6         ; 2

;====================================================================
; 后处理: mixOut 仍在 r6:r5 中 (r5 = 低字节, r6 = 高字节)
;====================================================================

	; ---- 1. 压缩并最终限幅到 [-128, 127] ----
	; 临界值: CLIP_UPPER=127 (0x007F), CLIP_LOWER=-128 (0xFF80)
	; mixOut 在 r6:r5 中 (r6=高, r5=低)

.ifne USE_SEGMENT_COMPRESSOR
	; ---- Dynamic compressor: r6:r5 = r6:r5 * compressorGain / 256 ----
	; compressorGain is prepared by SynthCompressorTick() in C.
	mov	a,(pSynth+pCompressorGain)
	mov	r1,a
	mov	a,r6
	jnb	acc.7,compress_apply_positive$

	; Convert negative sample to magnitude.
	mov	a,r5
	cpl	a
	add	a,#1
	mov	r5,a
	mov	a,r6
	cpl	a
	addc	a,#0
	mov	r6,a

	mov	a,r6
	mov	r0,a
	mov	a,r1
	mov	b,a
	mov	a,r5
	mul	ab
	mov	a,b
	mov	r5,a
	mov	a,r0
	jz	compress_unsigned_high_done_neg$
	mov	a,r1
	mov	b,a
	mov	a,r0
	mul	ab
	add	a,r5
	mov	r5,a
compress_unsigned_high_done_neg$:
	mov	r6,#0

	; Restore sign.
	mov	a,r5
	cpl	a
	add	a,#1
	mov	r5,a
	mov	a,r6
	cpl	a
	addc	a,#0
	mov	r6,a
	sjmp	compress_done$

compress_apply_positive$:
	mov	a,r6
	mov	r0,a
	mov	a,r1
	mov	b,a
	mov	a,r5
	mul	ab
	mov	a,b
	mov	r5,a
	mov	a,r0
	jz	compress_unsigned_high_done_pos$
	mov	a,r1
	mov	b,a
	mov	a,r0
	mul	ab
	add	a,r5
	mov	r5,a
compress_unsigned_high_done_pos$:
	mov	r6,#0
compress_done$:
.endif ; USE_SEGMENT_COMPRESSOR

CLIP_UPPER=127
CLIP_LOWER=-128

	; ---- if (mixOut > 127) ----
	clr	c
	mov	a,#(CLIP_UPPER&0xFF)           ; a = 0x7F
	subb	a,r5                         ; 0x7F - mixOut_l
	mov	a,#((CLIP_UPPER>>8) ^ 0x80)    ; a = 0x80
	mov	b,r6
	xrl	b,#0x80                        ; b = mixOut_h XOR 0x80 (符号反转)
	subb	a,b                          ; 0x80 - (mixOut_h XOR 0x80)
	jnc	00104$                         ; 无借位 → mixOut <= 127

	; ----- mixOut = 127 -----
	mov	r5,#(CLIP_UPPER&0xFF)          ; 0x7F
	mov	r6,#(CLIP_UPPER>>8)            ; 0x00
	sjmp	00107$

00104$:
	; ---- else if (mixOut < -128) ----
	clr	c
	mov	a,r5
	subb	a,#(CLIP_LOWER&0xFF)          ; mixOut_l - 0x80
	mov	a,r6
	xrl	a,#0x80                        ; mixOut_h XOR 0x80 (符号反转)
	subb	a,#((CLIP_LOWER>>8)&0x7F)    ; (mixOut_h XOR 0x80) - 0x7F
	jnc	00107$                         ; 无借位 → mixOut >= -128

	; ----- mixOut = -128 -----
	mov	r5,#(CLIP_LOWER&0xFF)          ; 0x80
	mov	r6,#(CLIP_LOWER>>8)            ; 0xFF

00107$:
	; ---- 3. DC 偏移: mixOut + 128 → [0, 255] (4 周期, ★ 优化 [3]) ----
	mov a,r5
	add a,#128                         ; 1 (a = mixOut_l + 128; 绝不溢出)
	mov r5,a                           ; 1
	mov a,r6
	addc a,#0                          ; 1 (传播进位)
	mov r6,a                           ; 1

.ifne USE_DITHERING
;====================================================================
; 3.5. LFSR 抖动 — 16-bit Galois LFSR + ±1 triangular dither
;
; 在 8-bit PWM 输出前添加 ±1 LSB 随机抖动，将量化失真转为白噪声。
; Galois 多项式: x^16 + x^14 + x^13 + x^11 + 1
; 可用 USE_DITHERING=0 关闭此功能。
;====================================================================

	; ---- Step 1: 16-bit 算术右移，C = 移位前的 bit 0 ----
	mov a,(pSynth+pLFSR_h)             ; 1
	rrc a                              ; 1
	mov (pSynth+pLFSR_h),a             ; 1
	mov a,(pSynth+pLFSR_l)             ; 1
	rrc a                              ; 1
	mov (pSynth+pLFSR_l),a             ; 1 (6 周期)

	; ---- Step 2: 若旧 LSB=1，XOR 多项式掩码 (平均 50% 触发) ----
	;             掩码 0x6801 = x^14+x^13+x^11+x^0
	jnc	dither_no_xor$                 ; 2 (不触发时跳转)
	mov	a,(pSynth+pLFSR_h)             ; 1
	xrl	a,#0x68                        ; 1
	mov	(pSynth+pLFSR_h),a             ; 1
	mov	a,(pSynth+pLFSR_l)             ; 1
	xrl	a,#0x01                        ; 1
	mov	(pSynth+pLFSR_l),a             ; 1 (6 周期)
	sjmp	dither_apply$                 ; 2
dither_no_xor$:
	; a 仍持有移位后的 pLFSR_l

dither_apply$:
	; ---- Step 3: 取 LFSR LSB 作为抖动位，±1 应用到 r5 ----
	;             r5 = [0..255] (PWM 8-bit 输出值)
	mov	c,a.0                          ; 2 (C = 随机位)
	jnc	dither_sub$                    ; 2
	; +1 带饱和
	cjne	r5,#255,dither_add$           ; 2
	sjmp	dither_done$                  ; 2 (饱和在 255)
dither_add$:
	inc	r5                              ; 1
	sjmp	dither_done$                  ; 2
dither_sub$:
	; -1 带饱和
	cjne	r5,#0,dither_sub_ok$           ; 2
	sjmp	dither_done$                  ; 2 (饱和在 0)
dither_sub_ok$:
	dec	r5                              ; 1
dither_done$:
.endif ; USE_DITHERING

	; ---- 4. 输出到 PWMA_CCR2 (PWM DAC) (10 周期) ----
	mov	dptr,#REG_PWMA_CCR2H
	mov a,r6
	movx	@dptr,a                      ; PWMA_CCR2H = mixOut_h
	mov	dptr,#REG_PWMA_CCR2L
	mov a,r5
	movx	@dptr,a                      ; PWMA_CCR2L = mixOut_l
