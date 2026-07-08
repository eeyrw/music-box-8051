# Changelog

## 2026-07-08

### Runtime ADSR protocol

- Added runtime ADSR tuning over the serial protocol.
- `CMD_ADSR_GET` now returns an 11-byte payload: `ENV_MAX`, `TICK_MS`, four big-endian 16-bit 8.8 rate fields, and `SUSTAIN_THRESHOLD`.
- `CMD_ADSR_SET` accepts the same 11-byte payload and updates the runtime ADSR rate variables until reset.
- Added `STATUS_INVALID_PARAM` for invalid ADSR fixed fields or zero attack/decay/release rates.
- Updated `tools/musicbox_proto.py` with `adsr-get` 11-byte parsing and `adsr-set ATTACK_MS DECAY_MS SUSTAIN_DECAY_MS RELEASE_MS`.
- Fixed ADSR startup rate calculation to use the same 8.8 fractional formula as the protocol tool.

Validated on hardware with `make flash` and `/dev/ttyUSB0`:

```text
ping -> MusicBox-8051 v1.0

default adsr-get:
ATTACK:         0x4000 -> 10ms
DECAY:          0x04AB -> 30ms
SUSTAIN_DECAY:  0x0040 -> 2000ms
RELEASE:        0x0223 -> 300ms

adsr-set 20 60 0 200:
ATTACK:         0x2000 -> 20ms
DECAY:          0x0256 -> 60ms
SUSTAIN_DECAY:  0x0000 -> flat
RELEASE:        0x0334 -> 200ms
```

Runtime ADSR was restored to the firmware defaults after testing: `10 30 2000 300`.
