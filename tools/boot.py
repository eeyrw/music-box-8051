#!/usr/bin/env python3
"""Send 0xDD boot command to trigger STC MCU soft-reset into ISP bootloader."""
import sys
import time
import serial

def main():
    port = sys.argv[1] if len(sys.argv) > 1 else '/dev/ttyUSB0'
    baud = int(sys.argv[2]) if len(sys.argv) > 2 else 115200

    try:
        s = serial.Serial(port, baud, timeout=0.5)
        s.write(b'\xdd')
        s.flush()
        s.close()
    except Exception as e:
        print(f"boot.py: failed to send 0xDD: {e}", file=sys.stderr)

if __name__ == '__main__':
    main()
