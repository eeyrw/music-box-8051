# AGENTS.md — Music Box 8051

## Build

```bash
make              # production build (NO_RUN_TEST + STC8 defined)
make clean
```

SDCC toolchain for 8051 (C: `sdcc`, asm: `sdas8051`, hex packer: `packihx`). Outputs `.ihx` → `.hex` (via packihx) and `.bin` (via `sdobjcopy -O binary`).

`F_CPU` defaults to 16000000 in Makefile but `Bsp.c` overrides it to **22118400UL** — do not trust the Makefile value. Timer0 reload is always computed from 22118400.

Memory model is **large** (xdata used). Stack-auto is enabled (`--stack-auto`), required for ISR reentrancy safety.

## Test / Verification

To build the C-vs-ASM verification suite, edit Makefile `DEFS`: remove `NO_RUN_TEST`, add `RUN_TEST`. Then `main()` calls `TestProcess()` (see `AlgorithmTest.c`).

- `TestUpdateTickFunc()` — verifies 32-bit `currentTick` increment over 65535 calls
- `TestSynth()` — feeds 9 notes, runs 10000 iterations comparing C and ASM synth paths
- Tolerance: `mixOut` diff > 8 is an error; `val` diff > 2 is an error; all other fields must match exactly

Test bench wrappers (`Synth_testbench.s`, `UpdateTick_testbench.s`) are **commented out** in Makefile — only needed if you re-enable the test suite.

## Architecture

### Signal chain (all at 32 kHz from Timer0 ISR)

```
Timer0 ISR (PeriodTimer.s, bank 1)
  └─ SynthAsm (Synth.inc) — 8-voice polyphonic wavetable synthesis
  └─ UpdateTick (UpdateTick.inc) — 32-bit tick counter
```

Main loop polls `PlayerProcess()` which feeds notes via `NoteOnAsm` and triggers envelope decay via `GenDecayEnvlopeAsm`.

### Fixed memory layout (critical — do not change blindly)

- **Player struct**: absolute DATA `0x10`, 11+3 bytes (`Player.inc`)
- **Synthesizer struct**: absolute DATA `0x21` (= 0x10 + 11 + 6), 83 bytes (`SynthCore.inc`)
- **8 voices × 10 bytes** each: `increment_frac(0)`, `increment_int(1)`, `wavetablePos_frac(2)`, `wavetablePos_int_l(3)`, `wavetablePos_int_h(4)`, `envelopeLevel(5)`, `envelopePos(6)`, `val_l(7)`, `val_h(8)`, `sampleVal(9)`
- ISR uses **register bank 1** (`psw = 0x08`) so R0-R7 do not conflict with C code's bank 0

### Wavetable synthesis algorithm

8-bit signed Celesta C5 sample (21870 samples). Base frequency ~523 Hz, sample rate 32000 Hz.

Per-voice per-tick:
1. Read `WaveTable[pos]` + `WaveTable[pos+1]`, linear-interpolate with fractional phase
2. Multiply interpolated sample (signed) × envelope level (0-255 unsigned), divide by 256
3. Accumulate into 16-bit `mixOut`
4. Phase advance: `pos_frac += inc_frac; pos_int += inc_int + carry`
5. Loop point: when `pos_int >= 21869`, subtract 609

After all voices: `mixOut >>= 1`, clamp to [-128,127], add DC offset (+128), write to `PWMA_CCR2` (PWM DAC output).

### Score format

`__code` data (flash). Alternating bytes:
- byte with bit 7 set (`>= 0x80`) = note-on (MIDI number = `byte & 0x7F`)
- byte `< 0x80` = delta-time (accumulated into `lastScoreTick`)
- `0xFF` = extended duration (chains with previous delta)
- Two consecutive `0xFF` = end of score

### Note assignment

Round-robin across 8 voices. `NoteOnAsm` runs inside `clr ea`/`setb ea` critical section.

### PWM Output

- **PWMA_CCR2** (P1.2/P1.3): audio PWM DAC, 256-count period, 8-bit resolution
- **PWMA_CCR4** (P1.6/P1.7): visualization (mirror galvo/LED), complementary output mode, driven by `abs(mixOut) << 1`

## Assembly dependency tracking

SDCC's assembler cannot auto-generate dependency files. Changes to `.inc` files will **not** trigger rebuilds unless the Makefile manual deps (lines 96-101) are updated to cover the changed `.inc` file. Always `make clean` after editing `.inc` files.

## Files with duplicates / overlaps

- `score.c` exists both at repo root and in `WavetableSynthesizer/`. The Makefile compiles the `WavetableSynthesizer/` copy. The root copy is stale.
- `PlayerUtil.s` declares the `_mainPlayer` struct at `PlayrAbsAddr` (0x10) and has **stub** implementations of `PlayNoteTimingCheck` and `PlayUpdateNextScoreTick` — the actual logic runs in `Player.c`. Do not remove `PlayerUtil.s` as it reserves the Player struct memory.

## Assembly optimization opportunities

### Synth.inc (hot path, runs 8 voices × 32 kHz)

- **B register pressure**: `mov b,r4` (line 48) loads envelope before the signed-mul branch. If interpolation is disabled (`.ifne USE_LINEAR_INTEROP`), `r4` is unused between lines 13-48.
- **Signed multiply branching**: For each voice, two separate signed-mul code paths (interpolation multiply + envelope multiply) generate unique labels per `.irp` iteration. The `jb a.7` / `cpl / inc / mul / cpl / addc` pattern is correct but verbose. Could unify with a subroutine if code size matters more than cycles.
- **mixOut >>= 1 with sign extension** (lines 120-130): uses `r4,r5` as temporaries, shifting bit 7 of the high byte into carry. Could be in-place at `(pSynth+pMixOut_x)` instead of copying to registers.
- **Clipping** (lines 137-161): the 16-bit signed `if (x > 127) x=127; else if (x < -128) x=-128` uses signed 16-bit compare with XOR-0x80 sign-flip trick. Two conditional branches. Could be replaced with saturation arithmetic using carry flags.
- **DC offset removal** (lines 164-170): `a - (-128)` = `a + 128`. A direct `add a,#128` with carry propagation is 1-2 cycles faster than `subb a,#-128`.
- **Phase increment** (lines 87-97): reads `pIncrement_frac/int` fresh from DATA each iteration even when increment is unchanged. Loading once per voice is already minimal since registers are scarce.

### SynthCoreAsm.s (NoteOnAsm)

- **FIXED**: `pWavetablePos_frac` was never zeroed on note-on due to a copy-paste error (the `mov a,#pEnvelopePos` appeared twice, missing the `#pWavetablePos_frac` field). This left stale fractional phase from the previous voice on slot reuse, causing pitch drift. Corrected 2026-07.
- **Lines 108-143**: repeated `mov a,#field; add a,r0; mov r1,a; mov @r1,value` pattern. Fields are sequential (offsets 0,1,2,3,4,5,6), so incrementing `r0` linearly and using `mov @r0,val` is shorter (but `r0` is needed for base address). Alternative: use a single DPTR copy loop with INC.

### UpdateTick.inc

- **32-bit increment** (lines 18-29): 4 separate `addc` instructions. The low byte could use `inc _currentTick` instead of `mov a,_currentTick; add a,#1; mov _currentTick,a`, saving 1 instruction and 1 byte.
- **decayGenTick compare** (lines 31-34): `mov a,#DECAY_TIME_FACTOR; clr cy; subb a,_decayGenTick; jc` works but the `subb` destroys A. Equivalent to `cjne a,_decayGenTick,$+3; jnc updateDecayGenTickEnd$`.

### PeriodTimer.s

- **ISR register save** (lines 18-22): pushes `acc, b, dpl, dph, psw`. Does not push R0-R7 because bank-switch (psw=0x08) isolates them. This is already well-optimized.
- The `setb P55` / `clr P55` pair (lines 24,27) is a **timing measurement pin** for oscilloscope profiling. Safe to remove in production.
