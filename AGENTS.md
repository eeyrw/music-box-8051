# AGENTS.md — Music Box 8051

## Build

```bash
make              # production build (NO_RUN_TEST + STC8 defined)
make clean
make flash        # build + flash via stcgal
```

SDCC toolchain for 8051 (C: `sdcc`, asm: `sdas8051`, hex packer: `packihx`). Outputs `.ihx` → `.hex` (via packihx) and `.bin` (via `sdobjcopy -O binary`).

`F_CPU` defaults to 16000000 in Makefile but `Bsp.c` overrides it to **22118400UL** — do not trust the Makefile value. Timer0 reload is always computed from 22118400.

Memory model is **large** (xdata used). Stack-auto is enabled (`--stack-auto`), required for ISR reentrancy safety.

## Hardware

### MCU: STC8H3K64S2-45I-TSSOP20

64KB Flash, 256B IRAM + 3KB XRAM, 22.1184 MHz internal oscillator.

`EAXFR` (P_SW2.7) 用于访问 XFR 扩展 SFR (0xFA00~0xFFFF)，不影响 XRAM 低 0x0000~0x0BFF 区域。

`EXTRAM` (AUXR.1) 复位默认 0，使能内部 XRAM。**切勿置 1**，否则内部 XRAM 被禁用。

#### 关键寄存器

| 寄存器 | 地址 | 关键位 | 说明 |
|--------|------|--------|------|
| P_SW2 | 0xBA | .7=EAXFR | 1=访问扩展SFR(XFR), 0=访问XRAM(0xFA00+) |
| AUXR | 0x8E | .1=EXTRAM | 0=使能内部XRAM, 1=禁用(访问外部总线) |
| P_SW1 | 0xA2 | .3:.2 | SPI 引脚组选择 (见下文 SPI 组表) |
| SPCTL | 0xCE | .6=SPEN | 1=使能硬件SPI |
| SPSTAT | 0xCD | .7=SPIF | SPI传输完成标志(写1清0) |
| SPDAT | 0xCF | — | SPI数据寄存器 |

#### 内存布局

```
地址范围          大小    用途
0x0000–0x0BFF    3072B   内部 XRAM (MOVX 访问, EXTRAM=0)
0x0C00–0xF9FF    —       保留区
0xFA00–0xFFFF    1536B   扩展 SFR / XFR (EAXFR=1 时 MOVX 访问)
0x0000–0x00FF     256B   内部 RAM (DATA/IDATA, 直接/间接访问)
  0x00–0x07      8B      Register Bank 0–3 (PSW 选择)
  0x08–0x1F      —       Register Bank 1 (ISR 专用)
  0x20–0x2F      16B     位寻址区
  0x21            1B     Synthesizer struct 起始 (绝对地址)
  0x30–0x7F      80B     用户 DATA/堆栈
  0x80–0xFF     128B     IDATA (间接寻址) + SFR (直接寻址)
```

#### IRC 时钟注意事项

- 复位后固定 24MHz；用户代码运行时使用上次烧录时设定的频率
- **32~37MHz 可能为盲区**，强烈建议 ≤30MHz 或 ≥40MHz
- B 版芯片低温温漂较高温大，低频段温漂比高频段大
- 当前项目使用 22.1184MHz (安全)

### SPI 引脚组 (P_SW1 切换)

| 组 | P_SW1[3:2] | SS | MOSI | MISO | SCLK | 说明 |
|----|-----------|------|------|------|------|------|
| 1 | 00 | P1.2 | P1.3 | P1.4 | P1.5 | 与音频PWM (P1.2/P1.3) **冲突** |
| 2 | 01 | P2.2 | P2.3 | P2.4 | P2.5 | TSSOP20 上不可用 (无P2口) |
| 3 | 10 | P5.4 | P4.0 | P4.1 | P4.3 | TSSOP20 上不可用 |
| 4 | 11 | P3.5 | P3.4 | P3.3 | P3.2 | ✓ 当前使用，**数据线交叉** |

### Components

| Ref | Part | Function |
|-----|------|----------|
| U2  | STC8H3K64S2-45I-TSSOP20 | MCU |
| U3  | CH340E | USB-UART (programming + UART commands) |
| U4  | ZD25WQ80BTIGT (SOP-8) | 8Mb SPI NOR Flash (score storage, external) |
| U5  | TPA2010D1YZFR | Audio amplifier (Class-D) |
| U1  | ME2188C50M5G | Boost converter (battery → 5V) |
| POT1 | B503 | 50K potentiometer (volume?) |

### 原理图信号路径

```
电源:
  BT1 (电池) → SW1 → U1 (ME2188C50M5G Boost 5V) → VCC
  USB-C (VUSB) → SW1 也可供电 (D3 隔离)
  U3 (CH340E) 由 USB-C 供电 (VUSB → U3-7)

音频:
  U2-19 (PWM) → R4 → U5-C1 (TPA2010D1 输入)
  U5-A3/C3 → SPK1 (扬声器)
  U2-2 (SHDN) → U5-C2 (功放使能, 高有效)

UART:
  U2-11 (TX/P3.1) → D1 → U3-8 (CH340 RX)
  U2-12 (RX/P3.0) ← D2 ← U3-9 (CH340 TX)
  115200 baud, Timer2 做波特率发生器

ADC:
  U2-20 ← R8 ← 分压 → R6 → GND (电池电压检测?)
```

### Bsp.c HardwareInit 已配置引脚

```c
P3M1 &= ~(1 << 2), P3M0 |= (1 << 2);  // P3.2 推挽输出
P3M1 &= ~(1 << 3), P3M0 |= (1 << 3);  // P3.3 推挽输出
P5M1 &= ~(1 << 5), P5M0 |= (1 << 5);  // P5.5 推挽输出 (P55, 示波器时序测量)
```

### MCU Pin Mapping (TSSOP20)

```
Pin |  Net    | GPIO        | Alt Functions          | Connected to
----|---------|-------------|------------------------|------------------
 1  | RXD2    | P1.0        | RxD2 / ADC0 / PWM1P    | TP_RXD2
 2  | SHDN    | P5.4?       | nRST / MCLKO           | U5-C2 (TPA2010 SHDN)
 3  | INDICATOR|(various)   |                        | LED (R9→VCC)
 4  | GND     | —           |                        | GND
 5  | VCC     | —           |                        | VCC
 6  | VCC     | —           |                        | VCC
 8  | VCC     | —           |                        | VCC
 9  | P55     | P5.5        | TP_P55                 | oscilloscope timing pin
10  | GND     | —           |                        | GND
11  | TX      | P3.1        | TxD (UART1)            | D1→U3-8 (CH340 RX)
12  | RX      | P3.0        | RxD (UART1)            | D2→U3-9 (CH340 TX)
13  | SCK2    | P3.2        | SCLK_4 / INT0          | U4-6 (Flash SCLK)
14  | SDI2    | P3.3        | MISO_4 / INT1          | U4-5 (Flash SI)
15  | SDO2    | P3.4        | MOSI_4 / T0            | U4-2 (Flash SO)
16  | SS2     | P3.5        | SS_4 / T1              | U4-1 (Flash CS#)
17  | TXD2    | P1.1        | TxD2 / ADC1 / PWM1N    | TP_TXD2
19  | PWM     | (PWM output)|                        | R4→U5 (Audio PWM→Amp)
20  | (ADC)   |             |                        | R8 → R6 → GND (ADC?)
```

### SPI Flash Connection (ZD25WQ80B — U4)

```
Flash Pin | Signal | MCU Pin | MCU GPIO  | Notes
----------|--------|---------|-----------|---------------------------
 1 (CS#)  | SS2    | 16      | P3.5/SS_4 | ✓ Chip Select
 2 (SO)   | SDO2   | 15      | P3.4      | ★ Cross-wired (see below)
 5 (SI)   | SDI2   | 14      | P3.3      | ★ Cross-wired (see below)
 6 (SCLK) | SCK2   | 13      | P3.2      | ✓ Clock
 3 (WP#)  | —      | —       | VCC       | Write protect disabled
 7 (HOLD#)| —      | —       | VCC       | Hold disabled
 4 (GND)  | —      | —       | GND       |
 8 (VCC)  | —      | —       | VCC       |
```

### ★ SPI Data-Line Cross-Wire

MCU 的 SPI 组 4 将 P3.3 定义为 MISO (输入)、P3.4 定义为 MOSI (输出)。
但 PCB 实际走线是：

```
MCU P3.3 (MISO_4)  →  Flash Pin 5 (SI)    ← 应为 MOSI→SI
MCU P3.4 (MOSI_4)  →  Flash Pin 2 (SO)    ← 应为 MISO←SO
```

SCLK (P3.2) 和 CS# (P3.5) 接线正确。

**后果**：STC8 硬件 SPI 模块无法直接使用（MOSI/MISO 角色对调）。

**解决方案** (按推荐排序):
1. **飞线交换** Flash 的 Pin2/Pin5 (或 MCU 的 Pin14/Pin15)，然后可用硬件 SPI 加速
2. **当前软件方案** (`Storage_SPI.c`)：GPIO 模拟 SPI，手动将 P3.3 当 MOSI、P3.4 当 MISO

### Storage Abstraction Layer

乐谱存储通过 `Storage.h` 定义的 `ScoreStream` (12-byte 不透明缓冲) 抽象访问。后端实现在 `Storage_Internal.c` 或 `Storage_SPI.c`。

```
内置 FLASH (Storage_Internal.c):     SPI FLASH (Storage_SPI.c):
ScoreStream {                        ScoreStream {
    uint8_t _[12];  (不透明)              uint8_t _[12];  (不透明)
    实际: __code ptr(2B)+pos(4B)+size(4B)   实际: base_addr(4B)+pos(4B)+size(4B)
}                                   }
```

Player 不感知后端差异，统一调用:
```c
storage_init();                      // 初始化 (内部版本 no-op, SPI 版本调用 SpiFlash_Init)
stream_init(&stream, storage_get_base_addr(), size);  // 打开乐谱流
stream_read / stream_u8 / stream_u16 / stream_u32     // 数据读取
```

### PWM Output Pins

- **PWMA_CCR2** (P1.2/P1.3): audio PWM DAC, 256-count period, 8-bit resolution
- **PWMA_CCR4** (P1.6/P1.7): visualization (mirror galvo/LED), complementary output mode, driven by `abs(mixOut) << 1`

### UART / USB

- UART1 (P3.0/P3.1) → CH340E → USB-C: programming + command interface at **115200 baud**
- UART2 (P1.0/P1.1) available at TP_RXD2/TP_TXD2 test points
- Serial protocol: framed binary `SYNC(0x5A) | CMD | LEN | PAYLOAD | CHECKSUM`, see `Protocol.h`
- Protocol tool: `tools/musicbox_proto.py` (full CLI), `tools/boot.py` (reset-only for make flash)

## Storage Backend Switching

Both backends are always compiled. A global function-pointer vtable (`Storage.c`) dispatches at runtime. The active backend is chosen at boot by `storage_auto_detect()`, which reads the SPI flash JEDEC ID:

- **Valid ID** → SPI backend (`Storage_SPI.c` + `SpiFlash.c`)
- **No response** (0xFF or 0x00) → Internal backend (`Storage_Internal.c` + `scoreList.c`)

Manual override: `storage_select_backend(STORAGE_BACKEND_INTERNAL)` or `STORAGE_BACKEND_SPI`. Use `storage_get_backend()` for runtime checks (e.g., Protocol.c flash commands).

### File Layout

```
./Storage.h              ← 抽象接口: ScoreStream (12-byte 不透明缓冲) + backend enum/select API
./Storage.c              ← 运行时调度器: 全局 ops vtable + 9 个 wrapper 函数 + auto_detect
./Storage_Internal.c     ← 内置 __code FLASH 后端
./Storage_SPI.c          ← SPI FLASH 后端，委托 SpiFlash 驱动
./SpiFlash.h             ← 通用 SPI NOR FLASH 驱动接口
./SpiFlash.c             ← GPIO 位模拟 + 1KB XRAM 读缓存
```

Player 不感知后端，统一调用 `storage_init()` / `storage_get_base_addr()` / `stream_*`。

### SPI Flash Driver (`SpiFlash.c`)

- GPIO 位模拟 P3.2(SCLK) / P3.3(MOSI) / P3.4(MISO) / P3.5(CS#)
- 1KB XRAM 读缓存，流式顺序读命中率 ≈ 511/512
- 同步写入/擦除接口，内部 `spi_wait_busy(ms)` 使用 `GetSysMs()` 精确超时
- FLASH 参数: `SPI_FLASH_BASE_ADDR = 0x00000000`, `SPI_FLASH_SIZE = 1MB`
- 如需硬件 SPI: 飞线交换 P3.3/P3.4 后修改 `SpiFlash.c`

## Score Generation

Scores use the [midi-to-simplescore](https://github.com/eeyrw/midi-to-simplescore) toolchain:

```bash
pip3 install -e /path/to/midi-to-simplescore

# Single SSCR score
midi-to-simplescore --midi song.mid -o ./output --template 8051_sdcc --tickPerSecond 125

# Multi-score SSPL container (generates scoreList.c directly)
python3 SSPL_Packer.py song1.mid song2.mid ... \
  -c WavetableSynthesizer/scoreList.c \
  --template 8051_sdcc --tickPerSecond 125 --voiceCenterNote 60
```

`--tickPerSecond 125` is required: Timer0 runs at 32000 Hz, score timing uses `GetSysMs()` with `delta × 8` ms conversion (since 1000/125 = 8ms per tick in the data stream).

## Test / Verification

To build the C-vs-ASM verification suite, edit Makefile `DEFS`: remove `NO_RUN_TEST`, add `RUN_TEST`. Then `main()` calls `TestProcess()` (see `AlgorithmTest.c`).

- `TestUpdateTickFunc()` — verifies 32-bit `sysMs` increment over 65535 calls
- `TestSynth()` — feeds 9 notes, runs 10000 iterations comparing C and ASM synth paths
- Tolerance: `mixOut` diff > 8 is an error; `val` diff > 2 is an error; all other fields must match exactly

Test bench wrappers (`Synth_testbench.s`, `UpdateTick_testbench.s`) are **commented out** in Makefile — only needed if you re-enable the test suite.

## Architecture

### Signal chain (all at 32 kHz from Timer0 ISR)

```
Timer0 ISR (PeriodTimer.s, bank 1)
  └─ SynthAsm (Synth.inc) — 8-voice polyphonic wavetable synthesis
  └─ UpdateTick (UpdateTick.inc) — 32-bit 毫秒计数器 (sysMs)
```

Main loop:
```c
while (1) {
    PlayerProcess(&mainPlayer);   // SSCR 解码 + 调度器
    VisualizeSound();             // Abs(mixOut) << 1 → PWM
    Proto_Process();              // 串口协议处理
}
```

### Timing

- Timer0 ISR: 32000 Hz, 每 32 个 tick (1ms) 递增 `sysMs` (32-bit)
- Player 使用 `GetSysMs()` 驱动乐谱事件调度和包络衰减
- 乐谱 delta (raw tick) × 8 = 毫秒 (TickPerSecond=125, 1000/125=8ms/tick)
- 包络衰减: 每 3ms 推进一格

### Fixed memory layout (critical — do not change blindly)

- **Player struct**: SDCC auto-allocated in XRAM (~39 bytes). `SSCR_Player` (19B) + `PlayScheduler` (24B).
- **Synthesizer struct**: absolute DATA `0x21`, 83 bytes (`SynthCore.inc`, declared in `SynthCoreAsm.s` via `.org`)
- **sysMsPre + sysMs**: DSEG allocated by `UpdateTick.inc` (5 bytes DATA at compiler-assigned address, 0x10-0x1A)
- **SPI 缓存**: 1024 字节 XRAM (`SpiFlash.c`)
- **协议缓冲**: RX ring 256B + pkt_data 260B + tx_buf 264B ≈ 780B XRAM (`Protocol.c`)
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

### Score format — SSPL + SSCR

Scores are stored as a single `__code unsigned char Score[]` in flash (internal build) or pre-programmed on SPI FLASH (SPI build) using the **SSPL** container format containing multiple **SSCR** scores.

#### SSPL container (12-byte header)

```
Offset | Size | Field
-------|------|-------
0      | 4    | Magic "SSPL" (0x53 0x53 0x50 0x4C)
4      | 1    | Version = 0x01
5      | 1    | Flags
6      | 2    | Count (uint16 LE) — number of scores
8      | 4    | Reserved
12     | 8×N  | Entry table: offset(4LE) + size(4LE) per entry
```

#### SSCR format (13-byte header per score)

```
Offset | Size | Field
-------|------|-------
0      | 4    | Magic "SSCR" (0x53 0x53 0x43 0x52)
4      | 1    | Version = 0x03
5      | 1    | Flags (bit0=NoteOn velocity, bit1=NoteOff velocity)
6      | 2    | TickPerSecond (uint16 LE)
8      | 4    | DataLength (uint32 LE)
12     | 1    | TotalTranspose (int8) — inverse to restore original pitch
13     | var  | Event data
```

#### Event data encoding

- **Delta bytes** (bit7=0): 6-bit little-endian chunks, bit6=1 means continuation
  - 0–63: 1 byte; 64–4095: 2 bytes; up to 24-bit total
- **Event bytes** (bit7=1):
  - `0xC0–0xFD`: NoteOn direct (note = byte & 0x3F, range 0–61)
  - `0xFF` + next_byte: NoteOn extended (full note 0–127)
  - `0x80–0xBD`: NoteOff direct (skipped — not used by current player)
  - `0xBF` + next_byte: NoteOff extended (skipped)
  - `0xBE`: EndOfScore
  - `0xFE`: Reserved → stop
- Pitch restoration: `playNote = encodedNote - TotalTranspose`

#### NoteOn flow

```
SSCR event byte → sscr_dispatch_event()
  → decode note (direct or extended)
  → apply totalTranspose inverse
  → NoteOnAsm(MIDI_note)
```

Velocity and NoteOff are parsed but not used by the current synth.

### Multi-score scheduler

The player supports multiple songs via `PlaySchedulerProcess` state machine in `Player.c`. SSPL header is parsed from `Score[]`:

```c
void StartPlayScheduler(Player *player, uint8_t mode);
// mode: MODE_ORDER_PLAY (loop) / MODE_LIST_ONCE / MODE_SINGLE_SONG
```

Serial protocol (115200 baud, framed binary, see `Protocol.h`):
- `0x12` (CMD_PREV): previous song
- `0x13` (CMD_NEXT): next song
- `0x02` (CMD_RESET): software reset (`IAP_CONTR = 0x60`)

State machine: `READY_TO_SWITCH` → `SWITCHING` → `SCORE_PREV/NEXT` → `READY_TO_SWITCH` (or `STOP` → IDLE power-down).

### Note assignment

**Free-voice-first + Round-robin fallback** across 8 voices. `NoteOnAsm` (`SynthCoreAsm.s:122`):

1. Phase A: scan all 8 voices for `envelopeLevel == 0` (fully silent) — reuse immediately
2. Phase B: if none free, use `lastSoundUnit` cursor (FIFO round-robin, steals oldest note)
3. `lastSoundUnit = (selected_index + 1) % 8`

The voice write (zeroing phase, setting envelope=255) is inside `clr ea`/`setb ea` critical section. The free-voice scan runs outside the critical section since `envelopeLevel` is only modified in main-loop context (`GenDecayEnvlopeAsm`), not in ISR.

**Why not "steal the quietest"?** High notes have larger `phase increment`, so `wavetablePos` reaches `WAVETABLE_ATTACK_LEN` (21260) faster, causing their envelope decay to start earlier. Comparing `envelopeLevel` systematically discriminates against high notes. FIFO round-robin is pitch-independent and fair.

## Serial Protocol

Full framed binary protocol documented in `Protocol.h`. Summary:

| Command | Code | Description |
|---------|------|-------------|
| PING | 0x00 | Return firmware version string |
| GET_INFO | 0x01 | Version, storage type, song count |
| RESET | 0x02 | Soft reset to ISP bootloader |
| UPTIME | 0x03 | System uptime in ms (uint32_t) |
| MEM_INFO | 0x04 | Stack pointer, free stack bytes |
| AUDIO_INFO | 0x05 | mixOut, active voice count, lastSoundUnit |
| ADC_READ | 0x06 | Read ADC channel (uint16_t) |
| VOICE_DUMP | 0x07 | Full 8-voice state snapshot (80 bytes) |
| SYS_INFO | 0x08 | Comprehensive system status (uptime, backend, audio, voices, song, mode — 14 bytes) |
| PLAY/STOP/PREV/NEXT | 0x10-13 | Playback control |
| SET_SONG | 0x14 | Switch to song index |
| GET_STATUS | 0x15 | Current song, playing state |
| FLASH_INFO/ID | 0x20/25 | SPI flash parameters / JEDEC ID |
| FLASH_ERASE/ERASE_ALL | 0x21/22 | Sector/chip erase |
| FLASH_READ | 0x23 | Read bytes from SPI flash |
| FLASH_WRITE | 0x24 | Write bytes to SPI flash |

Frame format: `SYNC(0x5A) | CMD | LEN | [PAYLOAD] | CHECKSUM`
Response: `SYNC(0x5A) | CMD\|0x80 | STATUS | LEN | [DATA] | CHECKSUM`

Python CLI: `tools/musicbox_proto.py --port /dev/ttyUSB0 <command> [args...]`

### Protocol reliability fixes (2026-07)

**1. SYNC-byte-in-payload corruption (host side)**

`_recv_frame` 旧版在读到任意 `0x5A` 时重置缓冲区。若 payload 中恰好含 `0x5A`（如 VOICE_DUMP 80 字节合成器状态数据），会截断有效帧，拼接残帧导致 `invalid response frame`。

修复：改为三阶段严格按 dlen 读帧——找 SYNC → 读 4 字节头部获取 dlen → 读满 `5+dlen` 字节返回，中途绝不重同步 `0x5A`。另加 `reset_input_buffer()` 清除串口残留。

**2. TX buffer race (MCU side)**

`send_simple_response` / `send_data_response` / `cmd_flash_read` 在覆盖 `tx_buf` 前不检查前次 TX 是否完成。当两条命令背靠背到达时，新响应会覆盖正在 ISR 逐字节发送的 `tx_buf`，线上出现混合帧乱码。

修复：三个函数在修改 `tx_buf` 前均插入 `while (tx_state != TX_IDLE)` 等待前次发送完毕；`start_tx()` 内用 `ES=0`/`ES=1` 保护 SBUF 写入原子性。

## System Timer API

```c
uint32_t GetSysMs(void);  // 32-bit millisecond counter, driven by Timer0 ISR
```

SPI flash driver (`SpiFlash.c`) uses `GetSysMs()` for accurate busy-wait timeouts:
- Sector erase: 1500ms, Chip erase: 30000ms, Page program: 1000ms

Player (`Player.c`) uses `GetSysMs()` for:
- Score event scheduling: `nextEventMs += delta * 8` (125 ticks/sec → 8ms/tick)
- Envelope decay: every 3ms

## Assembly dependency tracking

SDCC's assembler cannot auto-generate dependency files. Changes to `.inc` files will **not** trigger rebuilds unless the Makefile manual deps (lines 96-101) are updated to cover the changed `.inc` file. Always `make clean` after editing `.inc` files.

## Files with duplicates / overlaps

- `score.c` existed both at repo root and in `WavetableSynthesizer/`. The Makefile now compiles `WavetableSynthesizer/scoreList.c`. Both `score.c` copies are removed.
- `PlayerUtil.s` is removed from build — Player is auto-allocated by SDCC.
- `Player.inc` is documentation only, not compiled.

## Assembly optimization opportunities

### Synth.inc (hot path, runs 8 voices × 32 kHz)

- **B register pressure**: `mov b,r4` (line 48) loads envelope before the signed-mul branch. If interpolation is disabled (`.ifne USE_LINEAR_INTEROP`), `r4` is unused between lines 13-48.
- **Signed multiply branching**: For each voice, two separate signed-mul code paths (interpolation multiply + envelope multiply) generate unique labels per `.irp` iteration. The `jb a.7` / `cpl / inc / mul / cpl / addc` pattern is correct but verbose. Could unify with a subroutine if code size matters more than cycles.
- **mixOut >>= 1 with sign extension**: uses `r4,r5` as temporaries, shifting bit 7 of the high byte into carry. Could be in-place at `(pSynth+pMixOut_x)` instead of copying to registers.
- **Clipping**: the 16-bit signed `if (x > 127) x=127; else if (x < -128) x=-128` uses signed 16-bit compare with XOR-0x80 sign-flip trick. Two conditional branches. Could be replaced with saturation arithmetic using carry flags.
- **DC offset removal**: `a - (-128)` = `a + 128`. A direct `add a,#128` with carry propagation is 1-2 cycles faster than `subb a,#-128`.
- **Phase increment**: reads `pIncrement_frac/int` fresh from DATA each iteration even when increment is unchanged. Loading once per voice is already minimal since registers are scarce.

### SynthCoreAsm.s (NoteOnAsm)

- **FIXED**: `pWavetablePos_frac` was never zeroed on note-on due to a copy-paste error (the `mov a,#pEnvelopePos` appeared twice, missing the `#pWavetablePos_frac` field). This left stale fractional phase from the previous voice on slot reuse, causing pitch drift. Corrected 2026-07.
- **Lines 108-143**: repeated `mov a,#field; add a,r0; mov r1,a; mov @r1,value` pattern. Fields are sequential (offsets 0,1,2,3,4,5,6), so incrementing `r0` linearly and using `mov @r0,val` is shorter (but `r0` is needed for base address). Alternative: use a single DPTR copy loop with INC.

### UpdateTick.inc

- **sysMs 32-bit increment**: 预分频器每 32 个 ISR tick 触发一次 ms 递增。4 条 `addc` 指令链。
- **Prescaler compare**: `inc _sysMsPre; mov a,_sysMsPre; clr c; subb a,#32; jc` 实现 32 分频。

### PeriodTimer.s

- **ISR register save** (lines 18-22): pushes `acc, b, dpl, dph, psw`. Does not push R0-R7 because bank-switch (psw=0x08) isolates them. This is already well-optimized.
- The `setb P55` / `clr P55` pair (lines 24,27) is a **timing measurement pin** for oscilloscope profiling. Safe to remove in production.
