# Music Box 8051

An 8-voice polyphonic wavetable synthesizer running on an STC8H3K64S2 microcontroller (8051-compatible). Plays musical pieces from flash ROM or external SPI NOR flash using the SSPL/SSCR score format, driving audio output through a PWM DAC and a visualization LED/galvo via a second PWM channel.

## Hardware

- **MCU**: STC8H3K64S2 (64 KB flash, 256 B internal RAM, 3 KB XRAM)
- **Clock**: 22.1184 MHz (1T mode)
- **Audio output**: PWM channel 2 on P1.2/P1.3 — 256-period PWM acting as an 8-bit DAC, updated at 32 kHz
- **Visualization**: PWM channel 4 on P1.6/P1.7 — complementary output, driven by `abs(mixOut) << 1`

## Build

Requires [SDCC](http://sdcc.sourceforge.net/) (Small Device C Compiler) ≥ 4.0:

```bash
sudo apt install sdcc

# Internal flash build (scores embedded in MCU flash)
make

# Clean
make clean
```

Outputs:
- `music-box-8051.ihx` — SDCC intermediate Intel HEX
- `music-box-8051.hex` — Packed Intel HEX
- `music-box-8051.bin` — Raw binary

## Flash

Uses [stcgal](https://github.com/grigorig/stcgal) to program via USB-TTL serial adapter. The MCU is soft-reset into ISP mode automatically via `tools/boot.py`:

```bash
pip3 install stcgal

# Flash (default port /dev/ttyUSB0, protocol stc8d)
make flash

# Custom port/protocol
make flash STCGAL_PORT=/dev/ttyUSB1 STCGAL_PROTO=stc8h
```

## Serial Protocol

Full framed binary protocol at 115200 baud: `SYNC(0x5A) | CMD | LEN | [PAYLOAD] | CHECKSUM`. Response returns `CMD|0x80` with status byte.

```bash
# Python CLI tool
python3 tools/musicbox_proto.py --port /dev/ttyUSB0 ping
python3 tools/musicbox_proto.py --port /dev/ttyUSB0 info
python3 tools/musicbox_proto.py --port /dev/ttyUSB0 sysinfo
python3 tools/musicbox_proto.py --port /dev/ttyUSB0 status
python3 tools/musicbox_proto.py --port /dev/ttyUSB0 voice
python3 tools/musicbox_proto.py --port /dev/ttyUSB0 note-on 60   # trigger note
python3 tools/musicbox_proto.py --port /dev/ttyUSB0 note-off 60  # release note
python3 tools/musicbox_proto.py --port /dev/ttyUSB0 next

# Fast note commands (no response, for low-latency playback)
python3 tools/musicbox_proto.py --port /dev/ttyUSB0 fast-note-on 60 100
python3 tools/musicbox_proto.py --port /dev/ttyUSB0 fast-note-off 60

# Play MIDI files directly via fast-note protocol
python3 tools/midi_player.py --port /dev/ttyUSB0 song.mid

# SPI flash read/write
python3 tools/musicbox_proto.py --port /dev/ttyUSB0 flash-info
python3 tools/musicbox_proto.py --port /dev/ttyUSB0 flash-read 0x0000 256 | xxd
```

See `docs/Protocol.md` for the full protocol specification, or `Protocol.h` for the command table and `tools/musicbox_proto.py --help`.

## Architecture

```
                    ┌─────────────────────────┐
                    │    Timer0 ISR @ 32kHz    │
                    │     (Bank 1 registers)   │
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
                    │  │  · sysMs++         │  │
                    │  └────────────────────┘  │
                    └─────────────────────────┘
                              ↕ mixOut
                    ┌─────────────────────────┐
                    │  Main Loop (Bank 0)      │
                    │  · SSCR_DecodeProcess()  │
                    │    - Score sequencing    │
                    │  · PlaySchedulerProcess  │──→ PWMA_CCR4 (P1.6/P1.7) Visual
                    │    - Auto-advance/stop   │
                    │  · VisualizeSound()      │
                    │  · Proto_Process()       │
                    │    - Serial protocol     │
                    └─────────────────────────┘
```

### Signal chain

1. **Wavetable**: Configurable 8-bit signed PCM sample at 32 kHz (current: Square Wave C4, 5,428 samples, 5,306-sample attack + 121-sample loop). `WaveTable.h`/`.inc` define dimensions.
2. **Phase accumulator**: 16.8 fixed-point per voice, indexed by MIDI note number through a precomputed `WaveTable_Increment` table
3. **Linear interpolation**: Between adjacent samples using the 8-bit fractional phase component
4. **Envelope**: Full ADSR model with 8.8 fixed-point fractional rate per tick. Attack (0→128), Decay (128→100), Sustain (hold or decay from 100→0), Release (linear decay to 0). Raw envelope (0-128) × velocity (0-254) → index into 128-entry non-linear response curve → final envelopeLevel (0-255). Tick interval `ADSR_TICK_MS=5ms`, phase-locked to system timer. Durations configurable via `SynthCore.h` macros (`ADSR_ATTACK_MS`, `ADSR_DECAY_MS`, `ADSR_RELEASE_MS`, `ADSR_SUSTAIN_DECAY_MS`).
5. **Mixing**: 8 voices summed into a 16-bit accumulator, then `>>=1`, clipped to [-128, 127], and DC-shifted by +128 for unsigned 8-bit PWM
6. **Dithering**: Galois 16-bit LFSR adds ±1 LSB triangular dither before PWM output, converting quantization noise to white noise floor. Toggle via `USE_DITHERING` macro in `SynthCore.inc`.

### Timing

- Timer0 ISR at 32000 Hz, `sysMs` incremented every 32 ticks (1ms)
- `GetSysMs()` returns 32-bit millisecond uptime, used for all timing
- Score events: `nextEventMs += delta × 8` (TickPerSecond=125 → 8ms per tick)
- Envelope tick: every `ADSR_TICK_MS` (5ms), phase-locked via `nextTickMs` accumulator (immune to main-loop jitter)
- SPI flash operations use `GetSysMs()` for accurate busy-wait timeouts

### Voice allocation

**Free-voice-first + configurable steal strategy** across 8 voices. NoteOn scans for `voiceState[].envelopeState == SILENT` (XRAM), reuses immediately. If all 8 busy, `stealVoice()` applies the strategy selected by `NOTEON_STEAL_STRATEGY` (default: steal oldest `allocStamp`). Five strategies available: oldest, quietest, newest, highest note, lowest note. Voice writes are inside `EA=0`/`EA=1` critical section to prevent ISR data races. NoteOff sets `envelopeState = RELEASE` for all matching voices; if `envelopePhase == 0` (before first envelope tick), pre-charges to `ADSR_ENV_MAX/2` to prevent silent staccato notes. Stop/EndOfScore calls `SynthReleaseAllAsm` which zeroes all envelopes and sets state=SILENT.

### Score format — SSPL + SSCR

Scores use the **SSPL** container format containing multiple **SSCR** scores. On internal builds, scores are in MCU flash (`scoreList.c`). On SPI builds, scores are pre-programmed on external SPI NOR flash via the serial protocol.

Format details in `Player.h`. Generated by [midi-to-simplescore](https://github.com/eeyrw/midi-to-simplescore):

```bash
pip3 install -e /path/to/midi-to-simplescore

# Generate scoreList.c from MIDI files (internal flash)
python3 SSPL_Packer.py song1.mid song2.mid ... \
  -c scoreList.c \
  --template 8051_sdcc --tickPerSecond 125 --voiceCenterNote 60
```

`--tickPerSecond 125` is required: TIMER0 at 32000 Hz, delta × 8 = ms (1000/125 = 8ms per tick).

### Multi-score scheduler

Three playback modes controlled by serial protocol or `main.c`:

| Mode | Behavior |
|------|----------|
| `MODE_ORDER_PLAY` | Loop all songs forever |
| `MODE_LIST_ONCE` | Play all songs once, then stop |
| `MODE_SINGLE_SONG` | Play one song, stop (serial can override) |

On startup, a random song is selected using ADC noise as entropy.

## Storage Backends

Storage is abstracted behind `Storage.h` with runtime-selectable backends via function pointer dispatcher (`Storage.c`). Both backends are always compiled; the active one is chosen at boot by `storage_auto_detect()`:

| Backend | Files | Description |
|---------|-------|-------------|
| Internal | `Storage_Internal.c` + `scoreList.c` | `__code` flash via `movc a,@a+dptr` |
| SPI | `Storage_SPI.c` + `SpiFlash.c` | External SPI NOR via GPIO bit-bang |

**Auto-detection**: At boot, `storage_auto_detect()` reads the SPI flash JEDEC ID. If a valid chip responds, the SPI backend is selected; otherwise it falls back to internal flash. The backend can also be set manually via `storage_select_backend(type)`.

Player does not know which backend is active — all `stream_*` calls go through the global ops vtable.

`SpiFlash.c` provides generic NOR flash operations (read, write, erase, JEDEC ID) with a 1KB XRAM read cache and ms-based busy-wait timeouts. Used by both the ScoreStream layer and the serial protocol for flash programming.

## Verification Suite

A C-vs-Assembly comparison test framework is included for validating synthesis correctness:

```makefile
# In Makefile: remove NO_RUN_TEST, add RUN_TEST
make clean && make
```

The test feeds 9 notes, runs 10,000 iterations, and compares every voice field between the C and ASM implementations:
- `mixOut` tolerance: ±8
- `val` tolerance: ±2
- All other fields: exact match required

## File Map

```
├── main.c                           Entry point, main loop
├── Bsp.c / Bsp.h                    Hardware init (clock, UART, Timer0, PWM, ADC)
├── UartRedirect.c                   putchar/getchar over UART1
├── Protocol.c / Protocol.h          Serial protocol (frame, commands, flash ops)
├── RegisterDefine.h                 MCU selector (STC8 vs STC15F)
├── Storage.h                        ScoreStream abstraction (opaque buffer)
├── Storage.c                        Runtime backend dispatcher (function pointer vtable)
├── Storage_Internal.c               Internal __code flash backend
├── Storage_SPI.c                    SPI flash ScoreStream wrapper
├── SpiFlash.c / SpiFlash.h          Generic SPI NOR flash driver (bit-bang + cache)
├── scoreList.c                      SSPL container with multiple SSCR scores
├── Makefile                         SDCC build system + stcgal flash target
│
├── Player/                          Score decoder + multi-song scheduler
│   ├── Player.c                     SSCR decoder + SSPL container + scheduler
│   ├── Player.h                     Player struct + scheduler API
│   ├── Player.inc                   Assembly struct layout (documentation only)
│   └── PlayerUtil.s                 Legacy stubs (not compiled)
│
├── Synthesizer/                     Audio synthesis engine
│   ├── Synth.inc                    SynthAsm — 8-voice synthesis core (ISR hot path, inc. dithering)
│   ├── SynthCore.inc                Struct offsets + memory layout constants
│   ├── SynthCoreAsm.s               _synthForAsm data segment (0x21)
│   ├── SynthCore.c                  Synthesizer init + NoteOn/Off/Decay/ReleaseAll (C ADSR)
│   ├── SynthCore.h                  SoundUnit/Synthesizer/VoiceState struct definitions
│   ├── UpdateTick.inc               sysMs millisecond counter (ISR)
│   ├── PeriodTimer.s                Timer0 ISR entry (bank switch)
│   ├── PeriodTimer.h                sysMs extern declarations
│   ├── WaveTable.{c,h}              Wavetable data + pitch increments
│   ├── WaveTable.inc                Wavetable dimensions + constants (ASM)
│   ├── EnvelopTable.{c,h}           256-entry non-linear velocity response curve
│   ├── AlgorithmTest.c              C-vs-ASM verification suite (RUN_TEST only)
│   └── 8051.inc                     SFR address constants
│
├── tools/
│   ├── musicbox_proto.py            Full serial protocol CLI client
│   ├── midi_player.py               Real-time MIDI playback via fast-note protocol
│   ├── adsr_test.py                 ADSR envelope test suite
│   └── boot.py                      Send soft-reset frame for make flash
```

## License

MIT © 2019 Yuan
