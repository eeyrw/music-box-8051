#!/usr/bin/env python3
"""Play MIDI files through Music Box 8051 fast-note protocol."""

import sys
import time
import struct
import argparse
import mido
import serial

SYNC = 0x5A
CMD_STOP = 0x11
CMD_FAST_NOTE_ON = 0x0B
CMD_FAST_NOTE_OFF = 0x0C


def calc_csum(data):
    c = 0
    for b in data:
        c ^= b
    return c


def frame(cmd, payload=b""):
    h = bytes([SYNC, cmd, len(payload)])
    return h + payload + bytes([calc_csum(struct.pack("BB", cmd, len(payload)) + payload)])


class MidiPlayer:
    def __init__(self, port, baud=115200):
        self.ser = serial.Serial(port, baud, timeout=0.1)
        self.ser.reset_input_buffer()
        self._stop_playback()

    def _stop_playback(self):
        self.ser.write(frame(CMD_STOP))

    def _send_note(self, note, vel):
        if vel > 0:
            self.ser.write(frame(CMD_FAST_NOTE_ON, struct.pack("BB", note, vel & 0x7F)))
        else:
            self.ser.write(frame(CMD_FAST_NOTE_OFF, struct.pack("B", note)))

    def play(self, mid_path, transpose=0, velocity_scale=1.0, channel_filter=None, note_range=None):
        mid = mido.MidiFile(mid_path)
        pending = []
        t = 0.0

        for msg in mid:
            t += msg.time
            if msg.type == "note_on" or msg.type == "note_off":
                if channel_filter is not None and msg.channel not in channel_filter:
                    continue
                note = msg.note + transpose
                if note_range:
                    lo, hi = note_range
                    if note < lo or note > hi:
                        continue
                vel = int(msg.velocity * velocity_scale) if msg.type == "note_on" else 0
                if vel > 127:
                    vel = 127
                pending.append((t, note, vel))

        total = len(pending)
        start = time.time()
        for i, (abs_t, note, vel) in enumerate(pending):
            delay = abs_t - (time.time() - start)
            if delay > 0:
                time.sleep(delay)
            self._send_note(note, vel)
            kind = "on " if vel > 0 else "off"
            pct = (i + 1) * 100 // total
            print(f"\r  {kind} note={note:3d}  vel={vel:3d}  [{pct}%]", end="", flush=True)
        print()

    def close(self):
        self.ser.close()


def main():
    parser = argparse.ArgumentParser(description="Play MIDI file on Music Box 8051")
    parser.add_argument("--port", default="/dev/ttyUSB0", help="Serial port")
    parser.add_argument("--baud", type=int, default=115200, help="Baud rate")
    parser.add_argument("--transpose", type=int, default=0, help="Semitone transpose")
    parser.add_argument("--velocity", type=float, default=1.0, help="Velocity scale factor (0.0-1.0)")
    parser.add_argument("--channel", type=int, nargs="*", default=None, help="MIDI channels to include (default: all)")
    parser.add_argument("--note-min", type=int, default=None, help="Minimum note number")
    parser.add_argument("--note-max", type=int, default=None, help="Maximum note number")
    parser.add_argument("mid", help="MIDI file path")
    args = parser.parse_args()

    note_range = None
    if args.note_min is not None or args.note_max is not None:
        lo = args.note_min if args.note_min is not None else 0
        hi = args.note_max if args.note_max is not None else 127
        note_range = (lo, hi)

    player = MidiPlayer(args.port, args.baud)
    try:
        player.play(args.mid, args.transpose, args.velocity, args.channel, note_range)
    finally:
        player.close()


if __name__ == "__main__":
    main()
