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
- 行为: 调用 `NoteOnAsm(note, vel)`，分配空闲声道并进入 ADSR ATTACK 阶段 (env 从 0 渐起)。velocity 用于 `env × vel << 1` 缩放后查非线表。

---

#### CMD_NOTE_OFF (0x0A) — 释放音符

- 负载: 1 字节 — MIDI 音符编号 (0–127)
- 应答: 状态码
- 行为: 调用 `NoteOffAsm(note)`，扫描所有声道匹配 `midiNote`，将 `envelopeState` 置 RELEASE 触发线性衰减。释放所有匹配声道（处理同音复触）。

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
- 应答: 7 字节

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

实际阶段时长可由 `ceil(幅度/步进) × TICK_MS` 计算。Sustain 衰减率使用 8.8 定点小数：`rate = raw / 256`（单位 envelope/tick），通过 `SynthCore.h` 的 `ADSR_SUSTAIN_DECAY_MS` 宏配置（如 2000ms → FRAC=64 → rate=0.25/tick）。所有 `*_RATE_FRAC` 值为 `__xdata uint16_t` 运行时变量，由 `AdsrInit()` 在 `main()` 启动时根据 MS 时长宏初始化。

---

#### CMD_ADSR_SET (0x31) — 写入 ADSR 参数 (保留)

- 负载: 7 字节 (格式同 ADSR_GET)
- 应答: STATUS_NOT_SUPPORTED (当前 ADSR 参数为编译时常量，暂不支持运行时修改)

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
| `adsr-get` | — | ADSR 包络参数 (步进值 + 范围) |
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

# SPI Flash 操作
python3 tools/musicbox_proto.py --port /dev/ttyUSB0 flash-info
python3 tools/musicbox_proto.py --port /dev/ttyUSB0 flash-erase 0
python3 tools/musicbox_proto.py --port /dev/ttyUSB0 flash-write 0 score.bin
```

## 注意事项

1. payload 中 LEN 硬上限为 252 (受限于 TX 缓冲区 264 字节: 1 SYNC + 1 应答CMD + 1 STATUS + 1 LEN + 252 DATA + 1 CSUM = 257 字节)。
2. 应答帧首字节 (bit7=1) 将命令码和应答区分开。若收到 bit7=0 的帧，说明 MCU 仍在处理中或出现协议错位。
3. FLASH_WRITE 前必须先 FLASH_ERASE 目标扇区，驱动不自动完成此操作。
4. Python 客户端 `_recv_frame` 对 payload 中的 0x5A 是安全的——按 LEN 严格读取，不会截断有效帧。
