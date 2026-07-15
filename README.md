# Music Box 8051

An 8-voice polyphonic wavetable synthesizer running on an STC8H3K64S2 microcontroller (8051-compatible). Plays musical pieces from flash ROM or external SPI NOR flash using the SSPL/SSCR score format, driving audio output through a PWM DAC and a visualization LED/galvo via a second PWM channel.

## Hardware

- **MCU**: STC8H3K64S2 (64 KB flash, 256 B internal RAM, 3 KB XRAM)
- **Clock**: 33.1776 MHz as configured in `Bsp.c` (1T mode)
- **Audio output**: PWM channel 2 on P1.2/P1.3 — 256-period PWM acting as an 8-bit DAC, updated at 32 kHz
- **Visualization**: PWM channel 4 on P1.6/P1.7 — complementary output, driven by `compressorEnv << 1`

## Build

Requires [SDCC](http://sdcc.sourceforge.net/) (Small Device C Compiler) ≥ 4.0:

```bash
sudo apt install sdcc

# Internal flash build (scores embedded in MCU flash)
make

# Clean
make clean

# Force-regenerate compressor table/constants
make compressor
```

Outputs:
- `music-box-8051.ihx` — SDCC intermediate Intel HEX
- `music-box-8051.hex` — Packed Intel HEX
- `music-box-8051.bin` — Raw binary

`make compressor` and `make generate-compressor` explicitly rebuild `Synthesizer/CompressorGenerated.{h,c}` from `tools/gen_compressor.py` and update the compressor stamp file. Use either target after changing compressor Makefile parameters if you want to refresh generated code without relying on timestamp-based rebuild detection.

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

```text
                         ┌─────────────────────────────────────┐
                         │ Timer0 ISR @ 32 kHz, register bank 1 │
                         │                                     │
                         │  SynthAsm                            │
                         │  - read synthForAsm voices           │
                         │  - wavetable + linear interpolation  │
                         │  - envelopeLevel * sample            │
                         │  - 8-voice mix + compressor gain     │
                         │  - clamp, offset, optional dither    │
                         │                                     │
                         │  UpdateTick                          │
                         │  - sysMsPre / sysMs                  │
                         └──────────────┬──────────────┬───────┘
                                        │              │
                                        │              └─ PWMA_CCR2 audio PWM
                                        │
                                        │ shared DATA state
                                        ▼
┌──────────────────────────────────────────────────────────────────────────┐
│ Main loop, register bank 0                                                │
│                                                                          │
│  SynthProcess()                                                           │
│  - phase-locked compressor tick from compressorPeak                       │
│  - phase-locked ADSR step, updates envelopeLevel                          │
│  - writes visualization PWM from compressorEnv ────────────────┐          │
│                                                                 │          │
│  PlayerProcess(&mainPlayer)                                     │          │
│  - SSPL scheduler and SSCR event decoder                         │          │
│  - stream reads through Storage backend                          │          │
│  - dispatches SynthNoteOn / SynthNoteOff / SynthReleaseAll       │          │
│                                                                 │          │
│  Proto_Process()                                                 │          │
│  - UART framed command parser                                    │          │
│  - playback, note, ADSR, ADC, info commands                      │          │
│  - SPI flash read/write/erase commands                           │          │
└─────────────────────────────────────────────────────────────────┼────────┘
                                                                  │
                                                                  └─ PWMA_CCR4 visualization PWM

┌──────────────────────────────┐       ┌────────────────────────────────────┐
│ Storage.c runtime dispatcher  │──────▶│ Internal score backend             │
│ ScoreStream API               │       │ Storage_Internal.c + scoreList.c   │
│ selected by JEDEC auto-detect │       └────────────────────────────────────┘
│ or manual backend override    │       ┌────────────────────────────────────┐
│                              │──────▶│ SPI score/backend flash operations  │
└──────────────────────────────┘       │ Storage_SPI.c + SpiFlash.c         │
                                       └────────────────────────────────────┘
```

The ISR owns the fixed-rate audio sample path. The main loop owns control-rate work: score decoding, ADSR/compressor updates, visualization PWM, serial commands, and storage operations. Short critical sections use `Platform_IrqSave(state)` / `Platform_IrqRestore(state)` when main-loop code reads or clears ISR-shared multi-byte state.

### Signal chain

1. **Wavetable**: Configurable 8-bit signed PCM sample at 32 kHz (current: Square Wave C5, 1,390 samples, 1,328-sample attack + 61-sample loop). `WaveTable.h`/`.inc` define dimensions.
2. **Phase accumulator**: 16.8 fixed-point per voice, indexed by MIDI note number through a precomputed `WaveTable_Increment` table
3. **Linear interpolation**: Between adjacent samples using the 8-bit fractional phase component
4. **Envelope**: Full ADSR model with 8.8 fixed-point fractional rate per tick. Attack (0→128), Decay (128→110), Sustain (hold or decay from 110→0), Release (linear decay from current env to 0). Raw envelope (0-128) × velocity scale (0-254) → index into 128-entry non-linear response curve → final envelopeLevel (0-255). MIDI velocity is clamped to 0-127; 0 is treated as NoteOff. Tick interval `ADSR_TICK_MS=5ms`, phase-locked to system timer. Default durations are configured via `SynthCore.h` macros (`ADSR_ATTACK_MS=20`, `ADSR_DECAY_MS=200`, `ADSR_SUSTAIN_DECAY_MS=0`, `ADSR_RELEASE_MS=200`) and can be adjusted at runtime with the serial `adsr-set` command until reset.
5. **Mixing**: 8 voices summed in a 24-bit accumulator, shifted once to raw 16-bit `mixOut`, dynamically compressed, clamped to [-128, 127], and DC-shifted by +128 for unsigned 8-bit PWM
6. **Optional dithering**: `USE_DITHERING` is currently `0`, so the LFSR field is present but the ISR dither path is compiled out. Setting the macro enables ±1 LSB output dither before the PWM write.

### Timing

- Timer0 ISR runs at 32000 Hz and increments `sysMs` every 32 ticks (1ms).
- `SynthProcess()` uses `GetSysMs()` for phase-locked compressor and ADSR ticks; missed control ticks are caught up with accumulator loops.
- `PlayerProcess()` uses `GetSysMs()` for score scheduling: `nextEventMs += delta × 8` (TickPerSecond=125 → 8ms per tick).
- SPI flash write/erase operations use `GetSysMs()` for busy-wait timeouts.

### Runtime ADSR tuning

The serial protocol exposes ADSR rates as 11 bytes: `ENV_MAX`, `TICK_MS`, four 16-bit big-endian 8.8 rate fields, and `SUSTAIN_THRESHOLD`. `adsr-get` prints both raw rates and effective durations; `adsr-set ATTACK_MS DECAY_MS SUSTAIN_DECAY_MS RELEASE_MS` converts millisecond durations to the same 8.8 format. Attack must be at least `0x0100`, decay and release must be non-zero, every non-zero rate must be `<= 0xFF00`, and `SUSTAIN_DECAY_MS=0` keeps sustain flat. Runtime changes are not persisted and are reset by reboot or reflashing.

For interactive tuning, run `python3 tools/adsr_web.py` and open the Web Serial editor in Chrome/Edge. The editor connects to the CH340 (`1A86:7523`), reads current ADSR parameters automatically after selection, displays the envelope with grid/time axes, and writes the same serial `ADSR_SET` payload as the CLI. Detailed formulas are documented in `docs/Protocol.md`.

### Voice allocation

**Free-voice-first + configurable steal strategy** across 8 voices. NoteOn scans for `voiceState[].envelopeState == SILENT` (XRAM), reuses immediately. If all 8 busy, `stealVoice()` applies the strategy selected by `NOTEON_STEAL_STRATEGY` (default: steal oldest `allocStamp`). Five strategies available: oldest, quietest, newest, highest note, lowest note. `SynthNoteOn()` first clears the selected voice's 8-bit `envelopeLevel`, so the ISR skips that voice while the main loop rewrites multi-byte pitch/phase fields; no broad interrupt-off critical section is needed for the voice update. NoteOff sets `envelopeState = RELEASE` for all matching voices; if `envelopePhase == 0` (before first envelope tick), pre-charges to `ADSR_ENV_MAX/2` to prevent silent staccato notes. Stop/EndOfScore calls `SynthReleaseAll()` which zeroes all envelopes and sets state=SILENT.

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

The core synthesizer has a `RUN_TEST` firmware self-test suite plus host-side serial tests. The self-tests validate the current production behavior: `UpdateTick`, voice allocation, NoteOff release, ADSR transitions, compressor gain table generation, and the `SynthAsm` hot path against an independent C reference.

```bash
make clean DEFS="RUN_TEST STC8"
make DEFS="RUN_TEST STC8"
```

For production and test build verification after shared changes:

```bash
make clean DEFS="RUN_TEST STC8" && make DEFS="RUN_TEST STC8"
make clean && make
```

Full test details are documented in `docs/Testing.md`. Host-side ADSR/protocol tests are available through `tools/adsr_test.py` and `tools/musicbox_proto.py`.

## File Map

```
├── main.c                           Entry point, main loop
├── Bsp.c / Bsp.h                    Hardware init (clock, UART, Timer0, PWM, ADC)
├── Platform.h                       Compiler/platform memory and IRQ abstraction
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
│   ├── Synth.inc                    SynthAsm — 8-voice synthesis core (ISR hot path, optional dithering)
│   ├── SynthCore.inc                Struct offsets + memory layout constants
│   ├── SynthCoreAsm.s               _synthForAsm data segment (0x21)
│   ├── SynthCore.c                  Synthesizer init + process + note/envelope control (C ADSR)
│   ├── SynthCore.h                  SoundUnit/Synthesizer/VoiceState struct definitions
│   ├── NonlinearMapTable.{c,h}      128-entry linear-to-nonlinear response table
│   ├── CompressorGenerated.{c,h}    Generated compressor gain table/constants
│   ├── UpdateTick.inc               sysMs millisecond counter (ISR)
│   ├── PeriodTimer.s                Timer0 ISR entry (bank switch)
│   ├── PeriodTimer.h                sysMs extern declarations
│   ├── WaveTable.{c,h}              Wavetable data + pitch increments
│   ├── WaveTable.inc                Wavetable dimensions + constants (ASM)
│   ├── AlgorithmTest.c              RUN_TEST firmware self-tests
│   ├── Synth_testbench.s            RUN_TEST wrapper for Synth.inc
│   ├── UpdateTick_testbench.s       RUN_TEST wrapper for UpdateTick.inc
│   └── 8051.inc                     SFR address constants
│
├── tools/
│   ├── musicbox_proto.py            Full serial protocol CLI client
│   ├── gen_compressor.py            Generate compressor gain table/constants from dBFS params
│   ├── midi_player.py               Real-time MIDI playback via fast-note protocol
│   ├── adsr_test.py                 ADSR envelope test suite
│   ├── adsr_web.py                  Launch Web Serial ADSR editor
│   ├── adsr_web.html                Web Serial ADSR editor
│   └── boot.py                      Send soft-reset frame for make flash
```

Web ADSR editor:

```bash
python3 tools/adsr_web.py
```

## License

MIT © 2019 Yuan
