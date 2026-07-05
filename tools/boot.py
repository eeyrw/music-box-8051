#!/usr/bin/env python3
"""Send framed RESET command (0x5A 0x02 0x00 0x02) to trigger STC MCU soft-reset into ISP bootloader."""
import sys
import time
import struct
import serial

SYNC = 0x5A
CMD_RESET = 0x02


def calc_csum(data):
    csum = 0
    for b in data:
        csum ^= b
    return csum & 0xFF


def build_frame(cmd, payload=b""):
    csum = calc_csum(struct.pack("BB", cmd, len(payload)) + payload)
    return bytes([SYNC, cmd, len(payload)]) + payload + bytes([csum])


def main():
    port = sys.argv[1] if len(sys.argv) > 1 else "/dev/ttyUSB0"
    baud = int(sys.argv[2]) if len(sys.argv) > 2 else 115200

    frame = build_frame(CMD_RESET)
    try:
        s = serial.Serial(port, baud, timeout=0.5)
        s.write(frame)
        s.flush()
        s.close()
    except Exception as e:
        print(f"boot.py: failed to send RESET frame: {e}", file=sys.stderr)
        sys.exit(1)


if __name__ == "__main__":
    main()
