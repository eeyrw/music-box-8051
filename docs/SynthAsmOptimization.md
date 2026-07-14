# SynthAsm ISR Optimizations

This document records the current speed-oriented changes in `Synthesizer/Synth.inc` and the cycle estimates used to evaluate them.

## Counting Method

Cycle counts use the same unit shown by SDCC/SDAS in `Synthesizer/PeriodTimer.lst`:

```text
[12] = 1 instruction-cycle unit
[24] = 2 instruction-cycle units
[48] = 4 instruction-cycle units, for example MUL AB
```

The estimates below are path-local. They do not include Timer0 ISR entry/exit overhead, `UpdateTick`, or hardware wait states. They are intended for comparing two versions of the same `Synth.inc` path.

## Summary

| Area | Runtime effect |
| --- | --- |
| 24-bit mixer accumulator | Improves low-level precision. The common positive product path is cycle-neutral versus the old per-voice `>> 8` path, with a small fixed ISR cost for the third accumulator byte and final shift. |
| Silent voice skip | Saves 1 unit per silent voice. |
| Linear interpolation negative slope | Saves about 3 units when `B - A` is negative. |
| Envelope negative sample multiply | Cycle-neutral in the current 24-bit accumulator path; it makes the math explicit and reduces expansion size, but is not counted as a runtime win. |
| Wavetable wrap correction | Saves 1 unit only on the rare wrap path. |
| Compressor negative sign restore | Saves about 4 units on the common non-zero negative compressor output path. |
| Final limiter | Saves about 13 units for common in-range positive samples and about 11 units for common in-range negative samples. |

The largest stable speed win is the final limiter. The per-voice wins are data-dependent: they depend on how many voices are active, how often the interpolation slope is negative, and how often the sample itself is negative.

## Per-Voice Mixer Path

### Silent Voice Fast Path

Current code checks `envelopeLevel` before copying it to `r4`:

```asm
mov a,(pSndUnit+pEnvelopeLevel)
jz  soundUnitProcessEnd...
mov r4,a
```

Old silent path:

```text
mov a,envelopeLevel  1
mov r4,a             1
jz taken             2
total                4
```

Current silent path:

```text
mov a,envelopeLevel  1
jz taken             2
total                3
```

Speed gain: `1` unit per silent voice. Active voices are cycle-neutral because they still execute the same three instructions.

### Linear Interpolation Negative Slope

The interpolation term is:

```text
sample = A + high((int8_t)(B - A) * frac)
```

For a negative delta, the old path computed `abs(delta) * frac`, then two's-complement negated the 16-bit product before taking the high byte.

Current code uses this identity:

```text
signed_delta = unsigned_delta - 256
signed_delta * frac = unsigned_delta * frac - (frac << 8)
high(signed_delta * frac) = high(unsigned_delta * frac) - frac
```

Old negative-slope body after the sign branch:

```text
cpl a        1
inc a        1
mul ab       4
cpl a        1
add a,#1     1
xch a,b      1
cpl a        1
addc a,#0    1
total       11
```

Current negative-slope body after the sign branch:

```text
mov r2,b     1
mul ab       4
xch a,b      1
clr c        1
subb a,r2    1
total        8
```

Speed gain: `3` units on the negative-slope interpolation path.

The positive-slope interpolation path is unchanged.

### 24-bit Envelope Accumulator

The old envelope path effectively did this per voice:

```text
mix += high(sample * envelope)
```

That discarded the low byte of each voice's product before other voices could contribute to it. The current path accumulates the full product first:

```text
acc24 += sign_extend(sample * envelope)
mix = acc24 >> 8
```

For the common positive sample path, the current path is cycle-neutral against the old per-voice high-byte path:

```text
old positive envelope path     ~= 18 units
current positive envelope path ~= 18 units
```

There is a fixed ISR cost for the wider accumulator:

```text
extra accumulator clear: 1 unit  ; r7
final r7:r6:r5 >> 8:  4 units
fixed total:           5 units per ISR sample
```

This fixed cost buys precision rather than speed. It prevents low-level products from being lost independently in each voice.

### Envelope Negative Sample Multiply

For a negative sample, the old path used the same shape as the interpolation negate path: take magnitude, multiply, then two's-complement negate the product.

Current code uses this identity:

```text
signed_sample = unsigned_sample - 256
signed_sample * env = unsigned_sample * env - (env << 8)
```

The ISR therefore adds `unsigned_sample * env` to `r7:r6:r5`, then subtracts `env` from the middle byte and propagates borrow into the high byte.

Old 24-bit negative envelope path after the sign branch was about:

```text
cpl/inc sample magnitude             2
mul ab                               4
two's-complement product             6
accumulate sign-extended product     8
total                               20
```

Current negative envelope path after the sign branch:

```text
mul ab                                4
add product low/mid/high              8
subtract env << 8                     8
total                                20
```

Runtime effect: cycle-neutral versus the previous 24-bit negative-product path. This transformation is kept because it expresses the signed multiply as an unsigned multiply plus a correction term, avoids the 16-bit product-negation sequence, and reduces expansion size. It should not be included in runtime speedup totals.

## Wavetable Wrap Check

The no-wrap path is intentionally left as a normal 16-bit compare:

```asm
clr c
mov a,pos_l
subb a,#WAVETABLE_LEN
mov a,pos_h
subb a,#(WAVETABLE_LEN >> 8)
jc  no_wrap
```

With current wavetable constants:

```text
WAVETABLE_LEN      = 1389 = 0x056D
WAVETABLE_LOOP_LEN =   61 = 0x003D
```

The steady loop region is near `0x05xx`, so a high-byte-first check would often still need a low-byte compare and branch. It is not a clear runtime win for the common no-wrap path.

The only safe speed change is in the rare wrap correction path. After the compare falls through to wrap, carry is already clear because `pos >= WAVETABLE_LEN`. The old correction cleared carry again before subtracting `WAVETABLE_LOOP_LEN`; the current code does not.

Speed gain: `1` unit only when a voice wraps.

## Compressor Path

### Gain Multiply Moves

The compressor still applies signed `16x8` gain inline, with separate positive and negative paths. Some moves were shortened:

```asm
mov b,r1     ; instead of mov a,r1 / mov b,a
xch a,b      ; instead of mov a,b where A no longer matters
```

On this 8051 instruction model, these mostly reduce code bytes rather than instruction-cycle units because `mov b,r1` is a 2-byte direct move and `mov a,r1; mov b,a` is two 1-byte moves. They are kept because they do not slow the path and reduce expansion size. They are not counted as runtime speed gains.

### Negative Output Sign Restore

After compression, the negative path has an 8-bit scaled magnitude in `r5`. The old restore path pre-cleared `r6` and then computed the high byte through complement/addcarry.

Current non-zero restore path:

```text
mov a,r5       1
cpl a          1
add a,#1       1
mov r5,a       1
mov r6,#0xFF   1
jnz taken      2
total          7
```

Old restore path:

```text
mov r6,#0      1
mov a,r5       1
cpl a          1
add a,#1       1
mov r5,a       1
mov a,r6       1
cpl a          1
addc a,#0      1
mov r6,a       1
sjmp done      2
total         11
```

Speed gain: about `4` units for non-zero negative compressor output. If the scaled magnitude is zero, the current path still saves about `1` unit.

## Final Limiter

The old limiter performed two signed 16-bit comparisons:

```text
if mixOut > 127: clamp high
else if mixOut < -128: clamp low
```

The current limiter exploits the fact that the target range is exactly one signed byte:

```text
0x00:00..0x00:7F = in-range positive
0xFF:80..0xFF:FF = in-range negative
anything else     = clamp by high-byte sign
```

Approximate common-path counts:

```text
old no-clip positive path:  ~19 units
new no-clip positive path:    6 units
gain:                       ~13 units

old no-clip negative path:  ~19 units
new no-clip negative path:    8 units
gain:                       ~11 units
```

This is the most consistent speed win because it runs once per ISR sample regardless of voice count.

## Verification

After these changes, both builds should pass:

```bash
make clean DEFS="RUN_TEST STC8" && make DEFS="RUN_TEST STC8"
make clean && make
```

`TestSynthAsmStep()` compares the optimized assembly hot path against an independent C reference, including 24-bit pre-shift accumulation and wavetable wrap behavior.
