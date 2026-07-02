# Music Box 8051

An 8-voice polyphonic wavetable synthesizer running on an STC8H3K64S2 microcontroller (8051-compatible). Plays "La Valse d'Amélie" from flash ROM, driving audio output through a PWM DAC and a visualization LED/galvo via a second PWM channel.

## Hardware

- **MCU**: STC8H3K64S2 (64 KB flash, 256 B internal RAM, 2 KB XRAM)
- **Clock**: 22.1184 MHz (1T mode)
- **Audio output**: PWM channel 2 on P1.2/P1.3 — 256-period PWM acting as an 8-bit DAC, updated at 32 kHz
- **Visualization**: PWM channel 4 on P1.6/P1.7 — complementary output, driven by `abs(mixOut) << 1`

## Build

Requires [SDCC](http://sdcc.sourceforge.net/) (Small Device C Compiler) ≥ 4.0:

```bash
# Install on Ubuntu/Debian
sudo apt install sdcc

# Build
make

# Clean
make clean
```

Outputs:
- `music-box-8051.ihx` — SDCC intermediate Intel HEX
- `music-box-8051.hex` — Packed Intel HEX
- `music-box-8051.bin` — Raw binary

## Flash

Uses [stcgal](https://github.com/grigorig/stcgal) to program via USB-TTL serial adapter:

```bash
# Install stcgal
pip3 install stcgal

# Flash (default port /dev/ttyUSB0, protocol stc8d)
make flash

# Custom port/protocol
make flash STCGAL_PORT=/dev/ttyUSB1 STCGAL_PROTO=stc8h
```

## Architecture

```
                    ┌─────────────────────────┐
                    │     Timer0 ISR @ 32kHz   │
                    │      (Bank 1 registers)  │
                    │                          │
                    │  ┌────────────────────┐  │
                    │  │  SynthAsm          │  │
                    │  │  · Wavetable read  │  │
                    │  │  · Linear interp.  │  │
                    │  │  · Envelope ×8     │  │
                    │  │  · Mix & clip      │──┼──→ PWMA_CCR2 (P1.2/P1.3) Audio
                    │  │  · Phase advance   │  │
                    │  └────────────────────┘  │
                    │  ┌────────────────────┐  │
                    │  │  UpdateTick        │  │
                    │  │  · currentTick++   │  │
                    │  └────────────────────┘  │
                    └─────────────────────────┘
                              ↕ mixOut
                    ┌─────────────────────────┐
                    │  Main Loop (Bank 0)     │
                    │  · PlayerProcess()      │
                    │    - Score sequencing   │
                    │    - NoteOn/deacy       │──→ PWMA_CCR4 (P1.6/P1.7) Visual
                    │  · VisualizeSound()     │
                    └─────────────────────────┘
```

### Signal chain

1. **Wavetable**: 21,870-sample 8-bit signed Celesta C5 recording at 32 kHz, with 21,260-sample attack section and 609-sample sustain loop
2. **Phase accumulator**: 16.8 fixed-point per voice, indexed by MIDI note number through a precomputed `WaveTable_Increment` table
3. **Linear interpolation**: Between adjacent samples using the 8-bit fractional phase component
4. **Envelope**: 256-entry decay table applied after the phase passes the attack section; advances once every 100 ticks (~320 Hz)
5. **Mixing**: 8 voices summed into a 16-bit accumulator, then `>>=1`, clipped to [-128, 127], and DC-shifted by +128 for unsigned 8-bit PWM

### Voice allocation

Round-robin across 8 voices with interrupt-safe state updates. Note-on resets phase and envelope to full volume (255). Envelope decay starts after the attack section is traversed.

### Score format

`__code` byte array in flash. Alternating note and delta-time bytes:
- Byte with bit 7 set (`≥ 0x80`) — note-on event (MIDI number = `byte & 0x7F`)
- Byte `< 0x80` — delta-time offset, accumulated into `lastScoreTick`
- `0xFF` — chains with the previous delta for extended durations
- Two consecutive `0xFF` → end of score

## Verification Suite

A C-vs-Assembly comparison test framework is included for validating synthesis correctness:

```makefile
# In Makefile, change DEFS:
# DEFS = NO_RUN_TEST   → remove
# DEFS += RUN_TEST     → add
# Then rebuild
make clean && make
```

The test feeds 9 notes, runs 10,000 iterations, and compares every voice field between the C and ASM implementations:
- `mixOut` tolerance: ±8
- `val` tolerance: ±2
- All other fields: exact match required

## File Map

```
├── main.c                           Entry point, main loop
├── Bsp.c                            Hardware init (clock, UART, Timer0, PWM, ADC)
├── UartRedirect.c                   putchar/getchar over UART1
├── RegisterDefine.h                 MCU selector (STC8 vs STC15F)
├── Makefile                         SDCC build system + stcgal flash target
│
├── WavetableSynthesizer/
│   ├── Synth.inc                    SynthAsm — 8-voice synthesis core (ISR hot path)
│   ├── SynthCore.inc                Struct offsets + memory layout constants
│   ├── SynthCoreAsm.s               NoteOnAsm + GenDecayEnvlopeAsm
│   ├── SynthCore.c                  C synthesis reference (test only)
│   ├── SynthCore.h                  SoundUnit/Synthesizer struct definitions
│   ├── UpdateTick.inc               32-bit tick counter (ISR)
│   ├── PeriodTimer.s                Timer0 ISR entry (bank switch, include Synth+UpdateTick)
│   ├── Player.c                     Score sequencer
│   ├── Player.h / Player.inc        Player struct + absolute addresses
│   ├── PlayerUtil.s                 mainPlayer struct reservation + stub functions
│   ├── WaveTable.c                  Celesta C5 wavetable data (21,870 samples)
│   ├── WaveTable.h / WaveTable.inc  Wavetable dimensions + constants
│   ├── EnvelopTable.c               Decay envelope lookup table (256 entries)
│   ├── EnvelopeTable.h              Envelope table declaration
│   ├── AlgorithmTest.c              C-vs-ASM verification suite (RUN_TEST only)
│   └── score.c                      "La Valse d'Amélie" score data (1,452 bytes)
```

## Demo

The included demo track is "La Valse d'Amélie" (Yann Tiersen), transcribed to MIDI and encoded for the score sequencer. Notes span MIDI 50–95.

UART1 at 115200 baud also accepts commands:
- Send `0xFF` to restart playback
- Send `0xDD` to trigger a software reset

## License

MIT © 2019 Yuan
