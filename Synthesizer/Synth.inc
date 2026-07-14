;====================================================================
; Synth.inc — 8 声道波表合成引擎 (ISR 热路径)
;
; 本文件由 PeriodTimer.s 通过 .include 引入到 Timer0 ISR 中，
; 以 32000 Hz 的频率被调用。定义 _SynthAsm 标签，实现：
;
;   1. 遍历 8 个声道，对每个有 envelopeLevel > 0 的声道：
;      a. 从 WaveTable 以 16.8 定点相位读数
;      b. 线性插值（相邻采样点之间的小数部分插值）
;      c. 将采样值 (signed) 乘包络 (unsigned)
;      d. 完整累加到 24-bit accumulator，循环结束后统一 >> 8 得到 mixOut
;      e. 推进相位 (frac+int)，检查并处理循环点
;   2. 对所有声道处理完后：
;      a. 应用主循环准备好的动态压缩增益
;      b. 最终限幅到 [-128, 127]
;      c. 加 DC 偏移 (+128) 以适配无符号 PWM
;      d. 写入 PWMA_CCR2 PWM 占空比寄存器 (8-bit DAC 输出)
;
; 寄存器约定 (Bank 1, 0x08-0x0F):
;   r0   — 插值暂存；后处理 peak/compressor 乘法暂存
;   r1   — 后处理 peak/compressor 暂存
;   r4   — envelopeLevel (每声道从头持有到尾)
;   r5   — 24-bit 乘积累加器低字节，统一 >> 8 后为丢弃字节
;   r6   — 24-bit 乘积累加器中字节，统一 >> 8 后为 mixOut 低字节
;   r7   — 24-bit 乘积累加器高字节，统一 >> 8 后为 mixOut 高字节
;   r2   — 插值负分支暂存 frac
;   r3   — 未使用
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
;   [1] mixOut/accumulator 保持在寄存器中，避免每声道读写内存累加器。
;   [2] 包络乘法保留完整 16-bit product，8 声道先累加到 24-bit accumulator，
;       最后统一 >> 8；低 8 位能跨声道进位，弱音/尾音不会被每声道提前截断。
;   [3] 使用 xch a,b 搬运 MUL AB 的高/低字节，避开 mov a,b 的 2-byte 指令。
;   [4] 静音声道先 jz 再保存 envelope 到 r4，静音 fast path 少 1 条指令。
;   [5] 限幅利用目标范围 [-128,127] 的高字节模式判断，替代两次 16-bit compare。
;   [6] 插值 signed multiply 负分支用 high(unsigned_delta*frac)-frac，避免取绝对值后再取反。
;   [7] 包络 signed multiply 负分支先累加 unsigned product，再减 env<<8。
;   [8] add a,#128 替代 subb 做 DC 偏移。
;====================================================================

	.area CSEG    (CODE)
_SynthAsm:
	pSynth = SynthAbsAddr

	; ---- 清零 24-bit 乘积累加器 ----
	mov r5,#0                         ; r5 = acc_l = 0
	mov r6,#0                         ; r6 = acc_m = 0
	mov r7,#0                         ; r7 = acc_h = 0

;====================================================================
; 8 声道合成循环 (宏展开)
;====================================================================

.irp  Idx,0,1,2,3,4,5,6,7
	pSndUnit = pSynth+Idx*unitSz

	; 阶段 A: 包络检查
	mov a, (pSndUnit+pEnvelopeLevel)  ; 1
	; Optimization [4]: zero-envelope voices do not need r4. Put the branch before
	; the register copy so silent voices skip one instruction on the hot ISR path.
	jz soundUnitProcessEnd'Idx'$       ; 2 (不跳 = 4 周期)
	mov r4,a                           ; 1 (envelope 暂存)

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
	; Optimization [6]: for negative delta, signed_delta = unsigned_delta - 256,
	; so high(signed_delta * frac) = high(unsigned_delta * frac) - frac.
	; Here A already contains the 8-bit two's-complement delta (0x80..0xFF), which
	; can also be interpreted as unsigned_delta. B holds frac. MUL AB computes:
	;   unsigned_delta * frac = P_hi:P_lo
	; The wanted signed product is:
	;   (unsigned_delta - 256) * frac = P - (frac << 8)
	; After discarding the low byte for interpolation, this becomes P_hi - frac.
	; This avoids abs(delta), 16-bit negation, and the extra carry handling.
	mov r2,b                            ; 1 (r2 = frac, MUL will overwrite B)
	mul ab                             ; 4
	xch a,b                            ; 1 (★ 优化 [2]: 省 1 周期)
	clr c                              ; 1
	subb a,r2                          ; 1 (a = high(unsigned product) - frac)
signedMulEnd0'Idx'$:

	add a,r0                           ; 1 (a = A + 插值项)
.endif ; USE_LINEAR_INTEROP

	; 阶段 C: 包络乘法 — sample(signed) × envelope(unsigned)，保留完整 16-bit 乘积
	; Optimization [2]: the old path used product_h only, equivalent to per-voice
	; (sample * envelope) >> 8. That discarded product_l before other voices could
	; add into it. The new path accumulates sign_extend(product16) into r7:r6:r5
	; and shifts once after all voices, preserving sub-LSB energy across voices.
	mov b,r4                           ; 2 (b = envelopeLevel)

	jb a.7,signedMulNegBranch'Idx'$    ; 2 (jump not taken → positive path)

	; -- sample 为正路径 --
	mul ab                             ; 4
	add a,r5                           ; 1 (acc_l += product_l)
	mov r5,a                           ; 1
	; Optimization [3]: MUL AB leaves product_h in B. XCH A,B is a 1-byte/1-cycle
	; way to load product_h into A while product_l is no longer needed in A.
	xch a,b                            ; 1 (a = product_h)
	addc a,r6                          ; 1 (acc_m += product_h + carry)
	mov r6,a                           ; 1
	; Positive product has a 0x00 sign extension. CLR A is the shortest way to
	; form that byte while preserving carry into the accumulator high byte.
	clr a                              ; 1
	addc a,r7                          ; 1 (acc_h += carry)
	mov r7,a                           ; 1
	sjmp signedMulAccEnd'Idx'$         ; 2

signedMulNegBranch'Idx'$:
	; -- sample 为负路径 --
	; Optimization [7]: for negative sample, signed_sample = unsigned_sample - 256,
	; so sample*env = unsigned_sample*env - (env << 8). Accumulate the unsigned
	; product first, then subtract env from the accumulator middle byte.
	; Register state on entry:
	;   A  = sample as raw two's-complement byte (0x80..0xFF)
	;   B  = envelopeLevel (copied from r4 before the sign branch)
	;   r4 = envelopeLevel, preserved because MUL overwrites B
	;   r7:r6:r5 = 24-bit accumulator
	; Treating A as unsigned lets MUL AB produce unsigned_sample*env in B:A. The
	; correction term env<<8 is then subtracted from r7:r6:r5 after the unsigned
	; product has been added. That is faster than taking abs(sample), multiplying,
	; then two's-complement negating the 16-bit product.
	mul ab                             ; 4
	; Add P_lo into acc_l. Any carry must propagate through P_hi into acc_m.
	add a,r5                           ; 1 (acc_l += product_l)
	mov r5,a                           ; 1
	; Move P_hi from B to A without a two-byte MOV A,B. B is no longer needed for
	; the product after this point; r4 still has envelopeLevel for the correction.
	xch a,b                            ; 1 (a = product_h)
	addc a,r6                          ; 1 (acc_m += product_h + carry)
	mov r6,a                           ; 1
	; Add the carry from acc_m into acc_h. This is still the unsigned product add;
	; the signed correction is performed by the following subtract block.
	clr a                              ; 1
	addc a,r7                          ; 1 (acc_h += carry from unsigned product)
	mov r7,a                           ; 1
	; Subtract env<<8 from the 24-bit accumulator. Low byte is unaffected because
	; the correction term has a zero low byte. Borrow from acc_m propagates into
	; acc_h via SUBB A,#0.
	clr c                              ; 1
	mov a,r6                           ; 1
	subb a,r4                          ; 1 (acc_m -= env)
	mov r6,a                           ; 1
	mov a,r7                           ; 1
	subb a,#0                          ; 2 (borrow from env<<8 subtract)
	mov r7,a                           ; 1
signedMulAccEnd'Idx'$:

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
	; The preceding compare reaches this path only when pos >= WAVETABLE_LEN, so
	; carry is already clear. Do not clear it again before subtracting LOOP_LEN.
	mov a,(pSndUnit+pWavetablePos_int_l) ; 1
	subb a,#WAVETABLE_LOOP_LEN           ; 1
	mov (pSndUnit+pWavetablePos_int_l),a ; 1
	mov a,(pSndUnit+pWavetablePos_int_h) ; 1
	subb a, #(WAVETABLE_LOOP_LEN>>8)     ; 1
	mov (pSndUnit+pWavetablePos_int_h),a ; 1
waveTablePosAdjustEnd'Idx'$:
soundUnitProcessEnd'Idx'$:

.endm  ; .irp 结束

	; ---- 24-bit accumulator 统一 >> 8，低 8 位跨声道累加后再丢弃 ----
	; Optimization [2]: only now discard acc_l. This turns r7:r6:r5 >> 8 into the
	; signed 16-bit mixOut expected by the compressor/clip path: r6:r5 = r7:r6.
	mov a,r6                          ; r5 = mixOut_l
	mov r5,a
	mov a,r7                          ; r6 = mixOut_h
	mov r6,a

	; ---- 将原始 mixOut 写回内存供 VisualizeSound() 读取 (4 周期) ----
	;     在 >>1/裁剪/DC偏移之前写回，保持与原版一致: Visualization 用原始值
	mov (pSynth+pMixOut_l),r5         ; 2
	mov (pSynth+pMixOut_h),r6         ; 2

	; ---- 更新压缩器 peak hold: compressorPeak = max(compressorPeak, abs(raw mixOut)) ----
	; r0:r1 = abs(raw mixOut), 保持 r6:r5 不变供后处理继续使用。
	mov	a,r5
	mov	r0,a
	mov	a,r6
	mov	r1,a
	jnb	acc.7,compress_peak_abs_ready$
	mov	a,r0
	cpl	a
	add	a,#1
	mov	r0,a
	mov	a,r1
	cpl	a
	addc	a,#0
	mov	r1,a
compress_peak_abs_ready$:
	clr	c
	mov	a,(pSynth+pCompressorPeak_l)
	subb	a,r0
	mov	a,(pSynth+pCompressorPeak_h)
	subb	a,r1
	jnc	compress_peak_keep$
	mov	a,r0
	mov	(pSynth+pCompressorPeak_l),a
	mov	a,r1
	mov	(pSynth+pCompressorPeak_h),a
compress_peak_keep$:

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
	mov	b,r1
	mov	a,r5
	mul	ab
	xch	a,b
	mov	r5,a
	mov	a,r0
	jz	compress_unsigned_high_done_neg$
	mov	b,r1
	mov	a,r0
	mul	ab
	add	a,r5
	mov	r5,a
compress_unsigned_high_done_neg$:
	; Restore sign. The scaled magnitude is 8-bit in r5. Avoid pre-clearing r6:
	; the common non-zero path sets r6 to 0xFF directly and skips the zero fixup.
	mov	a,r5
	cpl	a
	add	a,#1
	mov	r5,a
	mov	r6,#0xFF
	jnz	compress_done$
	mov	r6,#0
	sjmp	compress_done$

compress_apply_positive$:
	mov	a,r6
	mov	r0,a
	mov	b,r1
	mov	a,r5
	mul	ab
	xch	a,b
	mov	r5,a
	mov	a,r0
	jz	compress_unsigned_high_done_pos$
	mov	b,r1
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

	; ---- signed 16-bit saturate to [-128,127] ----
	; Optimization [5]: because the output range is exactly one signed byte, a full
	; signed 16-bit compare is unnecessary. Values already in range have only two
	; possible byte patterns:
	;   0x00:00..0x00:7F  => 0..127
	;   0xFF:80..0xFF:FF  => -128..-1
	; Every other high-byte pattern is out of range; the high-byte sign bit tells
	; whether to clamp to +127 or -128. This shortens the common no-clip path.
	mov	a,r6
	jz	clip_high_zero$
	cjne	a,#0xFF,clip_high_other$
	mov	a,r5
	jb	acc.7,clip_done$                ; -128..-1
	sjmp	clip_lower$                   ; -256..-129

clip_high_zero$:
	mov	a,r5
	jnb	acc.7,clip_done$               ; 0..127

clip_upper$:
	mov	r5,#(CLIP_UPPER&0xFF)          ; 0x7F
	mov	r6,#(CLIP_UPPER>>8)            ; 0x00
	sjmp	clip_done$

clip_high_other$:
	jb	acc.7,clip_lower$               ; high byte is negative and not 0xFF
	sjmp	clip_upper$

clip_lower$:
	mov	r5,#(CLIP_LOWER&0xFF)          ; 0x80
	mov	r6,#(CLIP_LOWER>>8)            ; 0xFF

clip_done$:
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
