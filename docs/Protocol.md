# Music Box 8051 通信协议文档

## 物理层

| 参数 | 值 |
|------|-----|
| 接口 | UART1 (P3.0 RX / P3.1 TX) |
| 电平 | 3.3V TTL, 经 CH340E 转 USB-C |
| 波特率 | **115200** bps |
| 数据位 | 8 |
| 停止位 | 1 |
| 校验 | 无 |
| 流控 | 无 |

## 帧格式

所有命令和应答均采用定长帧头 + 变长负载 + 校验的二进制帧结构。

### 请求帧 (Host → MCU)

```
 Offset | Size | 内容
--------|------|-----------
   0    |  1   | SYNC (0x5A)
   1    |  1   | CMD  (命令码, 0x00–0x7F)
   2    |  1   | LEN  (负载字节数, 0–252)
   3    |  LEN | PAYLOAD (负载数据)
 3+LEN  |  1   | CHECKSUM (异或校验)
```

### 应答帧 (MCU → Host)

```
 Offset | Size | 内容
--------|------|-----------
   0    |  1   | SYNC (0x5A)
   1    |  1   | CMD | 0x80   (命令码 + 应答标志位)
   2    |  1   | STATUS (状态码)
   3    |  1   | LEN  (数据字节数)
   4    |  LEN | DATA (应答数据)
 4+LEN  |  1   | CHECKSUM (异或校验)
```

### 校验算法

对帧中 `CMD | STATUS | LEN | [DATA]` 所有字节逐字节异或(XOR)，结果等于 CHECKSUM。

```
checksum = 0
for byte in [CMD|0x80, STATUS, LEN] + DATA:
    checksum ^= byte
```

## 状态码

| 代码 | 宏名 | 含义 |
|------|------|------|
| 0x00 | STATUS_OK | 命令执行成功 |
| 0x01 | STATUS_UNKNOWN_CMD | 未知命令码 |
| 0x02 | STATUS_BAD_CHECKSUM | 校验和不匹配 |
| 0x03 | STATUS_BAD_LEN | 负载长度不合法 |
| 0x04 | STATUS_FLASH_ERR | SPI Flash 读写/擦除错误 |
| 0x05 | STATUS_NOT_SUPPORTED | 当前存储后端不支持该 FLASH 命令 |
| 0x06 | STATUS_INVALID_ADDR | 地址非法 |
| 0x07 | STATUS_INVALID_PARAM | 参数值非法或与固件固定参数不匹配 |

## 命令一览

### 系统命令 (0x00–0x0F)

#### CMD_PING (0x00) — 连接测试

- 负载: 无
- 应答: firmware 版本字符串 "MusicBox-8051 v1.0"

```
>> 5A 00 00 00
<< 5A 80 00 [len] [string...] [cksum]
```

---

#### CMD_GET_INFO (0x01) — 获取系统信息

- 负载: 无
- 应答: 4 字节

```
Offset | Size | 内容
   0   |  1   | FW_Version_Major
   1   |  1   | FW_Version_Minor
   2   |  1   | Storage backend (0=Internal Flash, 1=SPI Flash)
   3   |  1   | 乐曲总数
```

---

#### CMD_RESET (0x02) — 软复位

- 负载: 无
- 行为: 返回 OK 后写入 `IAP_CONTR = 0x60`，复位进入 ISP 引导区

---

#### CMD_UPTIME (0x03) — 系统运行时间

- 负载: 无
- 应答: 4 字节 (uint32 LE)，单位为毫秒

---

#### CMD_MEM_INFO (0x04) — 内存信息

- 负载: 无
- 应答: 4 字节

```
Offset | Size | 内容
   0   |  1   | SP (Stack Pointer, 当前栈顶)
   1   |  1   | Free stack bytes (0xFF - SP)
   2   |  1   | 保留
   3   |  1   | 保留
```

---

#### CMD_AUDIO_INFO (0x05) — 音频合成器状态

- 负载: 无
- 应答: 4 字节

```
Offset | Size | 内容
   0   |  1   | mixOut (低字节)
   1   |  1   | mixOut (高字节)
   2   |  1   | lastSoundUnit (下一个要分发的 voice 索引)
   3   |  1   | active voice count (0–8)
```

mixOut 为 16-bit 有符号值。

---

#### CMD_ADC_READ (0x06) — 读取 ADC

- 负载: 1 字节 — ADC 通道号 (0–15)
- 应答: 2 字节 (uint16 big-endian)，原始 ADC 读数 (0–4095)

---

#### CMD_VOICE_DUMP (0x07) — 合成器 8 声道完整快照

- 负载: 无
- 应答: 104 字节，每个 voice 13 字节，共计 8 个 voice

```
Per-Voice (13 bytes):
Offset | Size | 字段
   0   |  1   | increment_frac      (相位增量低字节)
   1   |  1   | increment_int       (相位增量高字节)
   2   |  1   | wavetablePos_frac   (波表位置小数部分)
   3   |  1   | wavetablePos_int (L) (波表位置低字节)
   4   |  1   | wavetablePos_int (H) (波表位置高字节)
   5   |  1   | envelopeLevel       (包络电平, 0=静音)
   6   |  1   | val (L)             (输出值低字节)
   7   |  1   | val (H)             (输出值高字节, 有符号16位)
   8   |  1   | sampleVal           (当前采样值)
   9   |  1   | midiNote            (MIDI 音符编号, NoteOff 查找用)
  10   |  1   | velocity            (力度缩放值, MIDI_vel × 2, 0-254)
  11   |  1   | envelopeState       (包络状态: 0=SILENT, 1=ATTACK, 2=DECAY, 3=SUSTAIN, 4=RELEASE)
  12   |  1   | envelopeFrac        (8.8 定点小数累加器)
```

Voice 0 位于 data[0..12]，Voice 1 位于 data[13..25]，以此类推。

---

#### CMD_NOTE_ON (0x09) — 触发音符

- 负载: 1 字节 MIDI 音符编号 (0–127), 可选第 2 字节 velocity (0–127, 默认 127)
- 应答: 状态码
- 行为: 调用 `NoteOnAsm(note, vel)`，分配空闲声道并进入 ADSR ATTACK 阶段 (env 从 0 渐起)。velocity 使用 MIDI 范围 0-127；0 按 NoteOff 处理，>127 饱和为 127。内部使用 `vel_scaled = vel × 2`，再由 `(env × vel_scaled) >> 8` 得到非线性曲线表索引。

---

#### CMD_NOTE_OFF (0x0A) — 释放音符

- 负载: 1 字节 — MIDI 音符编号 (0–127)
- 应答: 状态码
- 行为: 调用 `NoteOffAsm(note)`，扫描所有声道匹配 `midiNote`，将 `envelopeState` 置 RELEASE 触发线性衰减。释放所有匹配声道（处理同音复触）。

---

#### CMD_FAST_NOTE_ON (0x0B) — 快速触发音符（无应答）

- 负载: 1 字节 MIDI 音符编号 (0–127), 可选第 2 字节 velocity (0–127, 默认 127)
- 应答: **无**
- 行为: 同 `CMD_NOTE_ON`，但不返回任何应答帧。适合低延迟连续演奏（如 MIDI 播放器）。
- 注意: payload 中若含 `0x5A` 时帧解析安全——接收端严格按 LEN 读取，不会误判为 SYNC。

---

#### CMD_FAST_NOTE_OFF (0x0C) — 快速释放音符（无应答）

- 负载: 1 字节 — MIDI 音符编号 (0–127)
- 应答: **无**
- 行为: 同 `CMD_NOTE_OFF`，但不返回任何应答帧。适合低延迟连续演奏。

---

#### CMD_SYS_INFO (0x08) — 综合系统状态

- 负载: 无
- 应答: 14 字节

```
Offset | Size | 内容
   0   |  4   | Uptime (uint32 LE, ms)
   4   |  1   | Storage backend (0=Internal, 1=SPI)
   5   |  1   | Audio output on/off (1=on)
   6   |  1   | Free stack bytes
   7   |  1   | Active voice count (0–8)
   8   |  1   | mixOut (低字节)
   9   |  1   | mixOut (高字节)
  10   |  1   | Current song index
  11   |  1   | Total song count
  12   |  1   | Playing flag (1=playing, 0=stopped)
  13   |  1   | Scheduler mode (0=ORDER_PLAY, 1=LIST_ONCE, 2=SINGLE_SONG)
```

---

### 播放控制命令 (0x10–0x1F)

#### CMD_PLAY (0x10) — 开始播放

- 负载: 无
- 应答: 状态码

---

#### CMD_STOP (0x11) — 停止播放

- 负载: 无
- 应答: 状态码

---

#### CMD_PREV (0x12) — 上一曲

- 负载: 无
- 应答: 状态码

---

#### CMD_NEXT (0x13) — 下一曲

- 负载: 无
- 应答: 状态码

---

#### CMD_SET_SONG (0x14) — 切换到指定曲目

- 负载: 1 字节 — 曲目索引 (0-based)
- 应答: 状态码

---

#### CMD_GET_STATUS (0x15) — 播放状态查询

- 负载: 无
- 应答: 3 字节

```
Offset | Size | 内容
   0   |  1   | Current song index
   1   |  1   | Total song count
   2   |  1   | Playing flag (1=playing, 0=stopped)
```

---

---

#### CMD_ADSR_GET (0x30) — 读取 ADSR 包络参数

- 负载: 无
- 应答: 11 字节

```
Offset | Size | 内容
   0   |  1   | ADSR_ENV_MAX (内部 env 范围, 128)
   1   |  1   | ADSR_TICK_MS (包络更新间隔, 5ms)
   2   |  1   | ADSR_ATTACK_RATE_FRAC_H (Attack 步进值 8.8 定点, 高字节)
   3   |  1   | ADSR_ATTACK_RATE_FRAC_L (Attack 步进值 8.8 定点, 低字节)
   4   |  1   | ADSR_DECAY_RATE_FRAC_H (Decay 步进值 8.8 定点, 高字节)
   5   |  1   | ADSR_DECAY_RATE_FRAC_L (Decay 步进值 8.8 定点, 低字节)
   6   |  1   | ADSR_SUSTAIN_THRESHOLD (Sustain 起始阈值)
   7   |  1   | ADSR_SUSTAIN_DECAY_RATE_FRAC_H (Sustain 衰减率 8.8 定点, 高字节)
   8   |  1   | ADSR_SUSTAIN_DECAY_RATE_FRAC_L (Sustain 衰减率 8.8 定点, 低字节)
   9   |  1   | ADSR_RELEASE_RATE_FRAC_H (Release 步进值 8.8 定点, 高字节)
  10   |  1   | ADSR_RELEASE_RATE_FRAC_L (Release 步进值 8.8 定点, 低字节)
```

ADSR rate 使用 unsigned 8.8 定点数，单位是 `envelopePhase / tick`。原始字段 `raw` 与实际步进关系为 `step = raw / 256`，每个 envelope tick 先把低 8 位累加到 `envelopeFrac`，再用高 8 位进位推动 `envelopePhase`。`ADSR_TICK_MS=5` 时，每秒有 200 个 envelope tick。

毫秒时长到 rate 的换算公式：

```
raw = ceil(distance * ADSR_TICK_MS * 256 / duration_ms)
```

各阶段的 `distance`：Attack 为 `ADSR_ENV_MAX`，Decay 为 `ADSR_ENV_MAX - ADSR_SUSTAIN_THRESHOLD`，Sustain decay 为 `ADSR_SUSTAIN_THRESHOLD`，Release 为 `ADSR_ENV_MAX`。当前默认值中 Sustain decay 为 0，表示 sustain 阶段保持不衰减；例如默认 Attack 20ms：`ceil(128 * 5 * 256 / 20) = 8192`，即 `32 envelope/tick`。

实际阶段时长近似为：

```
duration_ms = ceil(distance / (raw / 256)) * ADSR_TICK_MS
```

由于状态切换时会把 `envelopeFrac` 清零，下一阶段不会继承上一阶段的小数残留。Release 在 NoteOff 时也清零 `envelopeFrac`，从当前 `envelopePhase` 开始线性下降。

所有 `*_RATE_FRAC` 值为 `__xdata uint16_t` 运行时变量，由 `AdsrInit()` 在 `main()` 启动时根据 MS 时长宏初始化，也可由 `CMD_ADSR_SET` 临时覆盖。

---

#### CMD_ADSR_SET (0x31) — 写入 ADSR 速率参数

- 负载: 11 字节 (格式同 ADSR_GET)
- 应答: STATUS_OK / STATUS_BAD_LEN / STATUS_INVALID_PARAM

`ADSR_ENV_MAX`、`ADSR_TICK_MS`、`ADSR_SUSTAIN_THRESHOLD` 是固件固定参数，SET 时必须与当前固件返回值一致；四个 `*_RATE_FRAC` 字段会写入运行时 `__xdata uint16_t` 变量，立即影响后续包络 tick。参数不会持久化，复位后由 `AdsrInit()` 按 `SynthCore.h` 的毫秒宏重新计算。

取值约束：Attack 必须 `>= 0x0100`，确保首个 envelope tick 至少推进 1 个 env 单位；Decay 和 Release 必须非 0；Sustain decay 可为 0，表示 sustain 阶段不自动衰减。所有非零 rate 必须 `<= 0xFF00`，保证 `envelopeFrac + rate` 不发生 16-bit 溢出，且每 tick 的进位不超过 255。

---

### SPI Flash 命令 (0x20–0x2F)

> 仅当存储后端为 SPI Flash 时可用。Internal Flash 模式下返回 STATUS_NOT_SUPPORTED。

#### CMD_FLASH_INFO (0x20) — Flash 信息

- 负载: 无
- 应答: 9 字节

```
Offset | Size | 内容
   0   |  3   | JEDEC ID (Manufacturer + Type + Capacity)
   3   |  4   | Flash 总容量 (uint32 big-endian, 字节)
   7   |  2   | 扇区大小 (uint16 big-endian, 字节)
```

---

#### CMD_FLASH_READ_ID (0x25) — 读 JEDEC ID

- 负载: 无
- 应答: 3 字节 JEDEC ID

---

#### CMD_FLASH_ERASE (0x21) — 擦除扇区

- 负载: 4 字节 (uint32 LE) — 扇区起始地址
- 应答: 状态码
- 超时: 建议 ≥ 2 秒 (扇区擦除最大 1500ms)

---

#### CMD_FLASH_ERASE_ALL (0x22) — 整片擦除

- 负载: 无
- 应答: 状态码
- 超时: 建议 ≥ 35 秒 (整片擦除最大 30s)

---

#### CMD_FLASH_READ (0x23) — 读取 Flash 数据

- 负载: 6 字节 — `[addr:4 LE] [len:2 LE]`
- 参数 `len` 最大 252
- 应答: `len` 字节数据
- 注意: 此命令会驱动内部读取缓存，读取完成后自动 invalidate 缓存以保证数据一致性

---

#### CMD_FLASH_WRITE (0x24) — 写入 Flash

- 负载: `[addr:4 LE] [data:N]`，共 4+N 字节
- 单次写入数据量不超过 248 字节 (单页 256 − 4 字节地址头)
- 应答: 状态码
- 注意: 不自动执行扇区擦除，需先调用 CMD_FLASH_ERASE
- 注意: 此驱动仅支持页编程，不支持跨页写入

---

## 时序约束

1. **背靠背命令**：MCU 在覆盖 TX 缓冲区前会等待前次发送完成，无需延迟。但建议 host 端等待上一条应答完整接收后再发下一条。
2. **应答超时**：大多数命令 < 100ms; FLASH_ERASE < 5s; FLASH_ERASE_ALL < 35s。
3. **帧间无空闲要求**：帧同步依赖 SYNC (0x5A) 字节。若 payload 中恰好出现 0x5A，不影响帧解析——接收端按 LEN 精确读取，不重同步。

## Python 命令行工具

`tools/musicbox_proto.py` 实现了完整的协议客户端。

```bash
python3 tools/musicbox_proto.py --port /dev/ttyUSB0 <command> [args...]
```

### 支持的命令

| 命令 | 参数 | 说明 |
|------|------|------|
| `ping` | — | 连接测试 |
| `info` | — | 固件版本/存储类型/曲目数 |
| `reset` | — | 软复位进入 ISP |
| `uptime` | — | 运行时间 (时:分:秒) |
| `mem` | — | 栈指针和剩余栈空间 |
| `audio` | — | mixOut/活跃 voice 数 |
| `adc` | `<CH>` | 读 ADC 通道 CH (0–15) |
| `voice` | — | 8 声道合成器状态 dump (含 midiNote/velocity/state) |
| `sysinfo` | — | 综合系统状态 (含 uptime/audio/song) |
| `note-on` | `<NOTE> [VEL]` | 触发 NoteOn ATTACK (0–127, 可选力度 0–127) |
| `note-off` | `<NOTE>` | 触发 NoteOff RELEASE (0–127) |
| `fast-note-on` | `<NOTE> [VEL]` | 快速 NoteOn，无应答 (0–127, 可选力度 0–127) |
| `fast-note-off` | `<NOTE>` | 快速 NoteOff，无应答 (0–127) |
| `adsr-get` | — | ADSR 包络参数 (8.8 速率 + 范围) |
| `adsr-set` | `<A_MS> <D_MS> <S_MS> <R_MS>` | 写入运行时 ADSR 时长；S_MS=0 表示 sustain 不衰减 |
| `play` | — | 开始播放 |
| `stop` | — | 停止播放 |
| `prev` | — | 上一曲 |
| `next` | — | 下一曲 |
| `song` | `<N>` | 切换到第 N 首 (0-based) |
| `status` | — | 播放状态 |
| `flash-info` | — | Flash JEDEC ID/容量/扇区大小 |
| `flash-id` | — | Flash JEDEC ID |
| `flash-erase` | `<ADDR>` | 擦除 ADDR 所在扇区 (hex) |
| `flash-erase-all` | — | 整片擦除 |
| `flash-read` | `<ADDR> <LEN> [FILE]` | 读取 LEN 字节到 stdout 或文件 |
| `flash-write` | `<ADDR> <INFILE>` | 将文件内容写入 ADDR |

### MIDI 播放器

`tools/midi_player.py` 通过 fast-note 协议实时播放标准 MIDI 文件：

```bash
# 播放 MIDI 文件（自动停止当前乐曲）
python3 tools/midi_player.py --port /dev/ttyUSB0 song.mid

# 移调（-12 = 低八度），力度缩放，只保留钢琴通道
python3 tools/midi_player.py --port /dev/ttyUSB0 song.mid --transpose -12 --velocity 0.8 --channel 0

# 限制音域（只播放 36-84 之间的音符）
python3 tools/midi_player.py --port /dev/ttyUSB0 song.mid --note-min 36 --note-max 84
```

### Web ADSR 编辑器

`tools/adsr_web.py` 启动一个本地 HTTP 服务并打开 `tools/adsr_web.html`。页面使用 Chrome/Edge 的 Web Serial API 访问 CH340 USB-UART (`VID:PID = 1A86:7523`)，选择串口后会自动执行 `CMD_ADSR_GET` 读取当前参数。

```bash
python3 tools/adsr_web.py
```

编辑器提供四个可写运行时参数：Attack、Decay、Sustain Decay、Release，单位为 ms。页面会把 ms 时长转换为固件协议使用的 8.8 rate，并通过 `CMD_ADSR_SET` 写入；`Sustain Decay = 0 ms` 表示 sustain 阶段保持平坦。右侧显示当前 11 字节 payload，曲线图显示 envelope level 与 time(ms) 网格。`SUSTAIN_THRESHOLD` 当前是固件固定字段，只展示为 Sustain Level，不可由 Web UI 写入。

Web Serial 需要从 `localhost` 或 HTTPS 页面打开，不能直接双击 HTML 文件。Linux 上推荐使用 deb 版 Chrome/Edge；Snap/Flatpak 浏览器可能因为沙盒权限无法稳定访问 `/dev/ttyUSB0`。

### 示例

```bash
# 连接测试
python3 tools/musicbox_proto.py --port /dev/ttyUSB0 ping

# 查看系统状态
python3 tools/musicbox_proto.py --port /dev/ttyUSB0 sysinfo

# 播放控制
python3 tools/musicbox_proto.py --port /dev/ttyUSB0 play
python3 tools/musicbox_proto.py --port /dev/ttyUSB0 next
python3 tools/musicbox_proto.py --port /dev/ttyUSB0 song 3

# 合成器测试
python3 tools/musicbox_proto.py --port /dev/ttyUSB0 note-on 60       # 触发 C5 (默认 vel=127)
python3 tools/musicbox_proto.py --port /dev/ttyUSB0 note-on 60 80    # 触发 C5 (vel=80)
python3 tools/musicbox_proto.py --port /dev/ttyUSB0 voice            # 查看声道
python3 tools/musicbox_proto.py --port /dev/ttyUSB0 note-off 60      # 释放 C5
python3 tools/musicbox_proto.py --port /dev/ttyUSB0 adsr-get         # ADSR 参数
python3 tools/musicbox_proto.py --port /dev/ttyUSB0 adsr-set 10 30 2000 300  # 运行时设置 ADSR(ms)

# 快速音符（无应答，适合低延迟演奏）
python3 tools/musicbox_proto.py --port /dev/ttyUSB0 fast-note-on 60 100
python3 tools/musicbox_proto.py --port /dev/ttyUSB0 fast-note-off 60

# SPI Flash 操作
python3 tools/musicbox_proto.py --port /dev/ttyUSB0 flash-info
python3 tools/musicbox_proto.py --port /dev/ttyUSB0 flash-erase 0
python3 tools/musicbox_proto.py --port /dev/ttyUSB0 flash-write 0 score.bin
```

这些命令测试的是已刷入固件的串口交互行为。确定性的固件内核心测试使用 `RUN_TEST` 构建，覆盖 `UpdateTick`、声道分配、ADSR 状态机和 `SynthAsm` 热路径；详见 `docs/Testing.md`。

## 注意事项

1. payload 中 LEN 硬上限为 252 (受限于 TX 缓冲区 264 字节: 1 SYNC + 1 应答CMD + 1 STATUS + 1 LEN + 252 DATA + 1 CSUM = 257 字节)。
2. 应答帧首字节 (bit7=1) 将命令码和应答区分开。若收到 bit7=0 的帧，说明 MCU 仍在处理中或出现协议错位。
3. FLASH_WRITE 前必须先 FLASH_ERASE 目标扇区，驱动不自动完成此操作。
4. Python 客户端 `_recv_frame` 对 payload 中的 0x5A 是安全的——按 LEN 严格读取，不会截断有效帧。
