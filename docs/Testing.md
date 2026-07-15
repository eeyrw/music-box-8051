# Testing

This project has two different test paths:

1. **Firmware self-tests** compiled with `RUN_TEST`. These validate deterministic core logic on the target firmware image.
2. **Serial hardware tests** run from a host PC against a flashed board. These validate protocol, runtime ADSR behavior, and board-level interaction.

## Firmware Self-Tests

Build the self-test firmware with:

```bash
make clean DEFS="RUN_TEST STC8"
make DEFS="RUN_TEST STC8"
```

`RUN_TEST` changes the build graph in `Makefile`:

- `Synthesizer/AlgorithmTest.c` is compiled only for test builds.
- `Synthesizer/Synth_testbench.s` wraps `Synth.inc` so C can call `_SynthAsm` directly.
- `Synthesizer/UpdateTick_testbench.s` wraps `UpdateTick.inc` so C can call `_UpdateTick` directly and provides a stub `_timer_isr` for the vector reference.
- `Synthesizer/PeriodTimer.s` is not compiled in this mode, because it normally inlines both `Synth.inc` and `UpdateTick.inc` into the real Timer0 ISR.

Flash the generated test firmware as usual if you want to run it on the board:

```bash
make flash DEFS="RUN_TEST STC8"
```

When `RUN_TEST` is defined, `main()` calls `TestProcess()` after hardware and protocol initialization. Test output is printed through UART via the existing `printf` redirection. A successful run ends with:

```text
ALL TESTS PASSED
```

Failures print `FAIL <name>` lines and end with `TESTS FAILED: <count>`.

## Firmware Coverage

The self-test suite is intentionally focused on current production behavior, not the old duplicate C synthesizer implementation.

`TestUpdateTickFunc()` verifies:

- `sysMsPre` divides the 32 kHz ISR tick by 32.
- `sysMs` increments once per 32 `UpdateTick()` calls.
- 32-bit `sysMs` wraparound from `0xffffffff` to `0`.

`TestNoteOnAllocation()` verifies:

- `SynthNoteOn(note, velocity)` initializes note, velocity scale, state, phase, level, and pitch increment.
- Newly allocated ATTACK voices are not reused just because `envelopeLevel == 0`.
- All 8 voices fill before stealing.
- The default `VOICE_STEAL_OLDEST` strategy steals voice 0 for the 9th note after a clean reset.

`TestNoteOffRelease()` verifies:

- `SynthNoteOff(note)` releases every active matching voice, including same-note retriggers.
- A note released before its first envelope tick is precharged to `ADSR_ENV_MAX / 2`.
- `SynthNoteOn(note, 0)` is treated as NoteOff.

`TestAdsrStateMachine()` verifies:

- ADSR state transitions: `ATTACK -> DECAY -> SUSTAIN -> RELEASE -> SILENT`.
- Runtime rates set by `AdsrSetRates()` drive deterministic 8.8 fixed-point phase movement.
- `envelopeLevel` follows `NonlinearMapTable[(env * velocity_scaled) >> 8]`.

`TestSynthAsmStep()` verifies the assembly hot path in `Synth.inc` against a small independent C reference:

- signed 8-bit wavetable sample handling
- linear interpolation
- signed sample times unsigned envelope scaling
- multi-voice 24-bit pre-shift accumulation into raw `mixOut`
- raw `compressorPeak = max(compressorPeak, abs(mixOut))` capture
- 16.8 phase advance
- wavetable loop wrap using `WAVETABLE_LOOP_LEN`

The test compares only fields that `SynthAsm` is responsible for maintaining: raw `mixOut`, `compressorPeak`, `wavetablePos_frac`, and `wavetablePos_int`. It does not compare legacy debug fields such as `val` or `sampleVal`, because the optimized assembly path does not update them.

`TestCompressorGainTable()` verifies:

- `SynthCompressorGainTable[0]` matches `SYNTH_COMPRESSOR_FAST_GAIN`.
- all generated gain values are non-zero.
- the generated table is monotonic non-increasing to avoid gain jitter.

Instruction-count notes for the optimized Timer0 ISR hot path are documented in `docs/SynthAsmOptimization.md`.

## Production Build Check

After changing shared code or Makefile logic, verify both modes:

```bash
make clean DEFS="RUN_TEST STC8" && make DEFS="RUN_TEST STC8"
make clean && make
```

Do not run these two builds concurrently in the same worktree. They share intermediate object paths, so parallel production and test builds can overwrite each other's `.rel`, `.asm`, `.lst`, `.rst`, and `.sym` files.

## Serial Hardware Tests

The serial protocol CLI can exercise the running production firmware:

```bash
python3 tools/musicbox_proto.py --port /dev/ttyUSB0 ping
python3 tools/musicbox_proto.py --port /dev/ttyUSB0 stop
python3 tools/musicbox_proto.py --port /dev/ttyUSB0 note-on 60 100
python3 tools/musicbox_proto.py --port /dev/ttyUSB0 voice
python3 tools/musicbox_proto.py --port /dev/ttyUSB0 note-off 60
```

For ADSR-focused board tests, use:

```bash
python3 tools/adsr_test.py --port /dev/ttyUSB0 --test all
```

For manual ADSR tuning on hardware, use the Web Serial editor:

```bash
python3 tools/adsr_web.py
```

The editor requires Chrome/Edge Web Serial support and writes the same runtime `ADSR_SET` parameters as `musicbox_proto.py`. Changes are not persisted across reset.

The serial tests depend on hardware timing, UART availability, and the currently flashed firmware. They complement the firmware self-tests but are not a replacement for the deterministic `RUN_TEST` suite.
