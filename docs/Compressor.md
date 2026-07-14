# Dynamic Compressor

This document describes the current audio compressor used after the 8-voice wavetable mixer and before the PWM DAC write.

## Summary

The compressor is a dynamic gain stage, not a per-sample waveshaper.

```text
Timer0 ISR, 32 kHz:
  SynthAsm mixes 8 voices -> raw mixOut
  compressor input = raw mixOut
  apply previously computed compressorGain
  clamp to [-128, 127]
  add PWM DC offset (+128)
  write PWMA_CCR2

Main-loop timing path:
  PlayerProcess()
    -> SSCR_DecodeProcess()
      -> SynthEnvelopeTick()
        -> SynthCompressorTick() every 1 ms
        -> GenDecayEnvlopeAsm() every ADSR_TICK_MS
```

The ISR accumulates a raw peak hold in `synthForAsm.compressorPeak` and applies `synthForAsm.compressorGain`. Envelope detection and gain lookup are done outside the ISR by `SynthCompressorTick()` in `SynthCore.c`. A final limiter remains in the ISR after compression so that transient peaks cannot wrap around the 8-bit PWM range.

## dBFS Definition

The compressor input is the raw post-mix signal before the PWM offset.

```text
raw mixOut theoretical range: 8 voices * [-128, 127] ~= [-1024, 1016]
ISR compressor input:        raw mixOut
configured input range:      [-1024, 1023]
```

Therefore:

```text
0 dBFS = abs(compressor input) == 1024
```

The output range is separate:

```text
output full-scale = 127
output range      = [-127, 127]
```

The generated curve converts input dBFS to output sample magnitude:

```text
in_db = 20 * log10(abs(input) / 1024)

if in_db <= threshold_db:
    out_db = in_db
else:
    out_db = threshold_db + (in_db - threshold_db) / ratio

out_db += makeup_db
output_mag = 127 * 10^(out_db / 20)
```

## Current Parameters

The Makefile default is:

```make
COMPRESSOR_PRESET      ?= loud
COMPRESSOR_INPUT_BITS  ?= 11
COMPRESSOR_OUTPUT_MIN  ?= -127
COMPRESSOR_OUTPUT_MAX  ?= 127
```

`loud` resolves to the same compressor knee as `safe`, but adds makeup gain so that a 0 dBFS input maps close to output full-scale:

```text
threshold_db = -6.0 dBFS
ratio        = 4.0:1
makeup_db    ~= +4.5 dB
```

At full-scale input:

```text
out_db = -6 + (0 - (-6)) / 4 = -4.5 dBFS
output_before_makeup ~= 127 * 10^(-4.5 / 20) ~= 76
output_after_makeup  ~= 127
```

This is louder than the conservative preset, but the final ISR limiter still guarantees that the PWM byte cannot wrap.

The conservative preset is:

```bash
make COMPRESSOR_PRESET=safe
```

`safe` uses the same threshold and ratio with `0 dB` makeup. At full-scale input it maps to about 76 counts before PWM offset, leaving more headroom and hitting the final limiter less often.

## Envelope Follower

`SynthCompressorTick()` runs every `COMPRESSOR_TICK_MS`:

```c
#define COMPRESSOR_TICK_MS 1
```

The ISR updates `synthForAsm.compressorPeak` on every 32 kHz sample:

```text
compressorPeak = max(compressorPeak, abs(raw_mixOut))
```

`SynthCompressorTick()` atomically reads and clears this peak once per compressor tick, then scales it down to a gain-table index with the generated `SYNTH_COMPRESSOR_ENV_SHIFT`:

```c
EA = 0;
mag = synthForAsm.compressorPeak;
synthForAsm.compressorPeak = 0;
EA = 1;
level = mag >> SYNTH_COMPRESSOR_ENV_SHIFT;
```

For the current 11-bit compressor input this resolves to:

```text
SYNTH_COMPRESSOR_ENV_SHIFT = 2
gain_table_index = abs(raw_mixOut) / 4
```

The envelope update is asymmetric:

```c
if (level > env) {
    env += max(1, (level - env) >> 2);   // attack
} else if (env > level) {
    env -= max(1, (env - level) >> 5);   // release
}
```

At 1 ms update rate, this is a fast attack and slower release. It is intended to catch peaks quickly while avoiding audio-rate gain modulation.

## Gain Table

The generated 256-entry `SynthCompressorGainTable[]` maps envelope level to Q8 gain.

```text
gain_q8 = round(target_output_mag * 256 / compressor_input_mag)
```

For the current 11-bit input, each table index represents four raw mixer counts:

```text
mag = env << SYNTH_COMPRESSOR_ENV_SHIFT
SYNTH_COMPRESSOR_ENV_SHIFT = 2
```

Generation for each entry is:

```text
in_db = 20 * log10(mag / 1024)

if in_db <= threshold_db:
    out_db = in_db
else:
    out_db = threshold_db + (in_db - threshold_db) / ratio

out_db += makeup_db
target_output_mag = 127 * 10^(out_db / 20)
gain_q8 = round(target_output_mag * 256 / mag)
```

For `env == 0`, the generator uses the nominal pass-through gain instead of dividing by zero.

For low levels under the threshold, the nominal gain is the pass-through scale from compressor input full-scale to output full-scale, with optional makeup gain:

```text
nominal_gain = 127 * 256 * makeup_gain / 1024
```

With the `safe` preset this is about `32`. With the current `loud` preset the generated nominal gain is about `53`.

The table is forced non-increasing after generation. This avoids small rounding reversals such as `63,64,63`, which can cause gain jitter.

The table is emitted in generated C source so it has a single CODE-memory definition:

```c
const __code uint8_t SynthCompressorGainTable[256] = { ... };
```

The generated header contains the compressor constants, the `extern` declaration, and the formula used to compute the table. `SynthCompressorTick()` uses this table from C and writes the selected value to `synthForAsm.compressorGain`. The ISR never looks up the table.

## ISR Path

`Synth.inc` contains the compressor gain application inline.

At macro entry:

```text
r6:r5 = signed raw mixOut
```

The compressor block:

1. Reads `synthForAsm.compressorGain` once.
2. Applies signed `16x8` gain scaling inline, with positive and negative paths expanded separately.

The compressor block does not update the envelope and does not look up the gain table. It also does not use `acall` or `ret`, so it does not add stack traffic inside the Timer0 ISR.

After the macro, `Synth.inc` runs the same signed clamp used by the non-compressor path:

```text
if sample > 127:
    sample = 127
else if sample < -128:
    sample = -128
```

This final clamp is the hard anti-pop safety boundary. The dynamic compressor reduces how often the limiter is reached, but the limiter is what guarantees that `sample + 128` always lands in the valid PWM byte range.

## State Storage

The compressor state is stored directly in `Synthesizer`:

```c
uint8_t compressorEnv;
uint8_t compressorGain;
uint8_t compressorTick;
uint16_t compressorPeak;
```

Assembly offsets are defined in `SynthCore.inc`:

```asm
pCompressorEnv  = unitSz*POLY_NUM+5  ; 77
pCompressorGain = unitSz*POLY_NUM+6  ; 78
pCompressorTick = unitSz*POLY_NUM+7  ; 79
pCompressorPeak = unitSz*POLY_NUM+8  ; 80-81
SynthTotalSize  = unitSz*POLY_NUM+10 ; 82
```

This makes the ownership explicit. These bytes are not shared with dithering or visualization state.

## Generated Files

The generator is:

```text
tools/gen_compressor.py
```

It writes:

```text
Synthesizer/CompressorGenerated.h         C constants, extern table declaration, formula notes
Synthesizer/CompressorGenerated.c         CODE-memory gain table definition
```

The Makefile target is automatic; changing generator parameters regenerates both files before compiling dependent objects.

## Verification

After changing compressor parameters or generator code, build both variants:

```bash
make clean DEFS="RUN_TEST STC8" && make DEFS="RUN_TEST STC8"
make clean && make
```

Current self-tests verify:

- generated gain table monotonicity
- the existing `SynthAsm` mixer behavior

## Known Tradeoffs

- `safe` is not a loudness maximizer. Full-scale input maps to about 76 counts before PWM offset.
- The envelope is based on raw `mixOut`, not a true RMS detector.
- The detector updates at 1 ms, not every audio sample.
- The final limiter can still hard-clip extreme transients. That is intentional for anti-pop safety; the compressor exists to make this rare.
- `Synthesizer` is now 82 bytes in internal DATA. This is explicit but reduces C stack headroom by 5 bytes compared with the original 77-byte layout.
- `COMPRESSOR_PRESET=loud` may sound louder but can pump more aggressively.
