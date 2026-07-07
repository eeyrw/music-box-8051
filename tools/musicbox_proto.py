#!/usr/bin/env python3
"""
Music Box 8051 Serial Protocol Client

Usage:
  musicbox_proto.py [--port PORT] [--baud BAUD] <command> [args...]

Commands:
  ping                    Test connection
  info                    Get system info (version, storage type, song count)
  reset                   Soft reset into ISP bootloader
  uptime                  Get system uptime
  mem                     Get memory info (SP, free stack)
  audio                   Get audio info (mixOut, active voices)
  adc <N>                 Read ADC channel N (0-15)
  voice                   Full 8-voice synthesizer state dump
  sysinfo                 Comprehensive system status (one-shot)
  note-on <NOTE> [VEL]    Trigger NoteOn (0-127), optional velocity 0-127
  note-off <NOTE>         Trigger NoteOff (MIDI note 0-127)
  play                    Start playback
  stop                    Stop playback
  prev                    Previous song
  next                    Next song
  song <N>                Switch to song index N (0-based)
  status                  Get playback status
  flash-info              Get SPI flash info (JEDEC ID, size, sector size)
  flash-id                Read JEDEC ID
  flash-erase <ADDR>      Erase 4KB sector at ADDR (hex)
  flash-erase-all         Erase entire SPI flash chip
  flash-read <ADDR> <LEN> [OUTFILE]  Read LEN bytes from ADDR, stdout or file
  flash-write <ADDR> <INFILE>        Write file to ADDR
"""

import sys
import time
import struct
import argparse
import serial

SYNC = 0x5A
RSP_FLAG = 0x80

CMD_PING           = 0x00
CMD_GET_INFO       = 0x01
CMD_RESET          = 0x02
CMD_UPTIME         = 0x03
CMD_MEM_INFO       = 0x04
CMD_AUDIO_INFO     = 0x05
CMD_ADC_READ       = 0x06
CMD_VOICE_DUMP     = 0x07
CMD_SYS_INFO       = 0x08
CMD_NOTE_ON        = 0x09
CMD_NOTE_OFF       = 0x0A
CMD_PLAY           = 0x10
CMD_STOP           = 0x11
CMD_PREV           = 0x12
CMD_NEXT           = 0x13
CMD_SET_SONG       = 0x14
CMD_GET_STATUS     = 0x15
CMD_FLASH_INFO     = 0x20
CMD_FLASH_ERASE    = 0x21
CMD_FLASH_ERASE_ALL= 0x22
CMD_FLASH_READ     = 0x23
CMD_FLASH_WRITE    = 0x24
CMD_FLASH_READ_ID  = 0x25
CMD_ADSR_GET       = 0x30
CMD_ADSR_SET       = 0x31

STATUS_OK            = 0x00
STATUS_UNKNOWN_CMD   = 0x01
STATUS_BAD_CHECKSUM  = 0x02
STATUS_BAD_LEN       = 0x03
STATUS_FLASH_ERR     = 0x04
STATUS_NOT_SUPPORTED = 0x05
STATUS_INVALID_ADDR  = 0x06
STATUS_BUSY          = 0x07

STATUS_NAMES = {
    0x00: "OK",
    0x01: "UNKNOWN_CMD",
    0x02: "BAD_CHECKSUM",
    0x03: "BAD_LEN",
    0x04: "FLASH_ERR",
    0x05: "NOT_SUPPORTED",
    0x06: "INVALID_ADDR",
    0x07: "BUSY",
}


def calc_csum(data):
    csum = 0
    for b in data:
        csum ^= b
    return csum


def build_frame(cmd, payload=b""):
    assert 0 <= cmd <= 0x7F
    assert len(payload) <= 252
    header = bytes([SYNC, cmd, len(payload)])
    csum = calc_csum(struct.pack("BB", cmd, len(payload)) + payload)
    return header + payload + bytes([csum])


def parse_response(data):
    if len(data) < 5:
        return None, None, None, None
    if data[0] != SYNC:
        return None, None, None, None
    rsp_cmd = data[1]
    if not (rsp_cmd & RSP_FLAG):
        return None, None, None, None
    cmd = rsp_cmd & ~RSP_FLAG
    status = data[2]
    dlen = data[3]
    if len(data) < 5 + dlen:
        return None, None, None, None
    payload = data[4:4 + dlen]
    csum_rcv = data[4 + dlen]
    header = struct.pack("BBB", rsp_cmd, status, dlen) + payload
    csum_calc = calc_csum(header)
    if csum_rcv != csum_calc:
        return None, None, None, None
    return cmd, status, payload, dlen


class MusicBoxClient:
    def __init__(self, port, baud=115200):
        self.ser = serial.Serial(port, baud, timeout=1.0)

    def _send_cmd(self, cmd, payload=b""):
        self.ser.reset_input_buffer()
        frame = build_frame(cmd, payload)
        self.ser.write(frame)
        self.ser.flush()

    def _recv_frame(self, timeout=3.0):
        buf = bytearray()
        deadline = time.time() + timeout

        while time.time() < deadline:
            c = self.ser.read(1)
            if not c:
                continue
            if c[0] == SYNC:
                buf = bytearray([c[0]])
                break

        if len(buf) == 0:
            return None

        while len(buf) < 4 and time.time() < deadline:
            c = self.ser.read(1)
            if c:
                buf.append(c[0])

        if len(buf) < 4:
            return None

        dlen = buf[3]
        total = 5 + dlen

        while len(buf) < total and time.time() < deadline:
            c = self.ser.read(1)
            if c:
                buf.append(c[0])

        if len(buf) >= total:
            return bytes(buf[:total])
        return None

    def _do_cmd(self, cmd, payload=b"", eat_extra=True, timeout=3.0):
        self._send_cmd(cmd, payload)
        raw = self._recv_frame(timeout)
        if raw is None:
            sys.exit("Error: no response or timeout")
        rsp_cmd, status, data, dlen = parse_response(raw)
        if rsp_cmd is None:
            sys.exit(f"Error: invalid response frame: {raw.hex()}")
        if rsp_cmd != cmd:
            sys.exit(f"Error: response cmd {rsp_cmd:#04x} != request {cmd:#04x}")
        if status != STATUS_OK:
            sys.exit(
                f"Error: cmd {cmd:#04x} returned status {status:#04x} ({STATUS_NAMES.get(status, 'UNKNOWN')})"
            )
        if eat_extra:
            self.ser.read(self.ser.in_waiting)
        return data

    def ping(self):
        data = self._do_cmd(CMD_PING)
        print(data.decode("ascii", errors="replace"))

    def info(self):
        data = self._do_cmd(CMD_GET_INFO)
        major, minor, storage, songs = struct.unpack("BBBB", data)
        stype = "SPI Flash" if storage else "Internal Flash"
        print(f"Firmware:    v{major}.{minor}")
        print(f"Storage:     {stype}")
        print(f"Songs:       {songs}")

    def reset(self):
        self._send_cmd(CMD_RESET)
        self.ser.close()
        print("Reset command sent.")

    def play(self):
        self._do_cmd(CMD_PLAY)
        print("Playing.")

    def stop(self):
        self._do_cmd(CMD_STOP)
        print("Stopped.")

    def prev(self):
        self._do_cmd(CMD_PREV)
        print("Previous song.")

    def next(self):
        self._do_cmd(CMD_NEXT)
        print("Next song.")

    def song(self, index):
        self._do_cmd(CMD_SET_SONG, struct.pack("B", index))
        print(f"Set song index to {index}.")

    def status(self):
        data = self._do_cmd(CMD_GET_STATUS)
        current, total, playing = struct.unpack("BBB", data)
        print(f"Song:        {current}/{total}")
        print(f"Playing:     {'yes' if playing else 'no'}")

    def flash_info(self):
        data = self._do_cmd(CMD_FLASH_INFO)
        jedec = data[0:3]
        size, = struct.unpack(">I", data[3:7])
        ssize, = struct.unpack(">H", data[7:9])
        print(f"JEDEC ID:    {jedec.hex().upper()}")
        print(f"Flash size:  {size} bytes ({size // 1024} KB)")
        print(f"Sector size: {ssize} bytes ({ssize // 1024} KB)")

    def flash_id(self):
        data = self._do_cmd(CMD_FLASH_READ_ID)
        print(f"JEDEC ID: {data.hex().upper()}")

    def flash_erase(self, addr):
        payload = struct.pack("<I", addr)
        data = self._do_cmd(CMD_FLASH_ERASE, payload, timeout=5.0)
        print(f"Sector at {addr:#010x} erased.")

    def flash_erase_all(self):
        data = self._do_cmd(CMD_FLASH_ERASE_ALL, timeout=35.0)
        print("Chip erased.")

    def flash_read(self, addr, length, outfile=None):
        payload = struct.pack("<IH", addr, length)
        data = self._do_cmd(CMD_FLASH_READ, payload)
        if outfile:
            with open(outfile, "wb") as f:
                f.write(data)
            print(f"Read {len(data)} bytes from {addr:#010x} to {outfile}")
        else:
            sys.stdout.buffer.write(data)

    def flash_write(self, addr, infile):
        with open(infile, "rb") as f:
            file_data = f.read()
        total = len(file_data)
        offset = 0
        while offset < total:
            chunk = file_data[offset : offset + 248]
            payload = struct.pack("<I", addr + offset) + chunk
            self._do_cmd(CMD_FLASH_WRITE, payload, timeout=3.0)
            offset += len(chunk)
            pct = offset * 100 // total if total else 100
            print(f"\rWriting... {offset}/{total} ({pct}%)", end="", flush=True)
        print()
        print(f"Wrote {total} bytes to {addr:#010x}")

    def uptime(self):
        data = self._do_cmd(CMD_UPTIME)
        ms = struct.unpack("<I", data)[0]
        sec = ms // 1000
        h = sec // 3600
        m = (sec % 3600) // 60
        s = sec % 60
        print(f"Uptime:  {h}h {m}m {s}s  ({ms} ms)")

    def mem(self):
        data = self._do_cmd(CMD_MEM_INFO)
        sp, free = struct.unpack("BB", data[:2])
        print(f"SP:         {sp:#04x} ({sp})")
        print(f"Free stack: {free} bytes")

    def audio(self):
        data = self._do_cmd(CMD_AUDIO_INFO)
        mix_l, mix_h, last_voice, active = struct.unpack("BBBB", data)
        mix_out = (mix_h << 8) | mix_l
        if mix_out & 0x8000:
            mix_out -= 0x10000
        print(f"Mix out:     {mix_out:+d}")
        print(f"Last voice:  {last_voice}")
        print(f"Active:      {active}/8")

    def adc(self, channel):
        data = self._do_cmd(CMD_ADC_READ, struct.pack("B", channel))
        val = struct.unpack(">H", data)[0]
        mv = val * 5000 // 4096
        print(f"ADC{channel}:   {val}  ({mv} mV)")

    def voice(self):
        data = self._do_cmd(CMD_VOICE_DUMP)
        state_names = {0: "SILENT", 1: "ATTACK", 2: "DECAY", 3: "SUSTAIN", 4: "RELEASE"}
        for v in range(8):
            off = v * 12
            fields = data[off:off + 12]
            inc_frac  = fields[0]
            inc_int   = fields[1]
            pos_frac  = fields[2]
            pos_int   = fields[3] | (fields[4] << 8)
            env_level = fields[5]
            val       = fields[6] | (fields[7] << 8)
            if val & 0x8000:
                val -= 0x10000
            sample    = fields[8] if fields[8] < 128 else fields[8] - 256
            midi_note = fields[9]
            velocity  = fields[10]
            env_state = fields[11]
            active = "ACTIVE" if env_level > 0 else "idle  "
            print(
                f"  V{v} [{active}] inc={inc_frac:#04x},{inc_int:#04x}  "
                f"pos={pos_frac:#04x},{pos_int:#06x}  "
                f"env={env_level:#04x}(lvl)  "
                f"val={val:+d}  sample={sample:+d}  note={midi_note}"
                f"  vel={velocity}  state={state_names.get(env_state, str(env_state))}"
            )

    def sysinfo(self):
        data = self._do_cmd(CMD_SYS_INFO)
        (uptime_ms, backend, audio_on, stack_free,
         active, mix_l, mix_h, cur_song, max_songs,
         playing, mode) = struct.unpack("<IBBBBBBBBBB", data)

        sec = uptime_ms // 1000
        h = sec // 3600
        m = (sec % 3600) // 60
        s = sec % 60

        mix_out = mix_l | (mix_h << 8)
        if mix_out & 0x8000:
            mix_out -= 0x10000

        stype = "SPI Flash" if backend else "Internal Flash"
        mode_names = {0: "ORDER_PLAY", 1: "LIST_ONCE", 2: "SINGLE_SONG"}

        print(f"Uptime:       {h}h {m}m {s}s  ({uptime_ms} ms)")
        print(f"Storage:      {stype}")
        print(f"Audio out:    {'on' if audio_on else 'off'}")
        print(f"Stack free:   {stack_free} B")
        print(f"Voices:       {active}/8 active")
        print(f"Mix out:      {mix_out:+d}")
        print(f"Song:         {cur_song}/{max_songs}")
        print(f"Playing:      {'yes' if playing else 'no'}")
        print(f"Mode:         {mode} ({mode_names.get(mode, '?')})")

    def note_on(self, note, velocity=None):
        payload = struct.pack("B", note)
        if velocity is not None:
            payload += struct.pack("B", velocity)
        self._do_cmd(CMD_NOTE_ON, payload)
        vel_str = f" vel={velocity}" if velocity is not None else ""
        print(f"NoteOn  note={note}{vel_str}")

    def note_off(self, note):
        self._do_cmd(CMD_NOTE_OFF, struct.pack("B", note))
        print(f"NoteOff note={note}")

    def adsr_get(self):
        data = self._do_cmd(CMD_ADSR_GET)
        (env_max, tick_ms, attack_step, decay_step, sustain_thr,
         sustain_decay, release_step) = struct.unpack("BBBBBBB", data)
        att_dur = ((env_max + attack_step - 1) // attack_step) * tick_ms if attack_step else 0
        dec_delta = env_max - sustain_thr
        dec_dur = ((dec_delta + decay_step - 1) // decay_step) * tick_ms if decay_step else 0
        rel_dur = ((sustain_thr + release_step - 1) // release_step) * tick_ms if release_step else 0
        print(f"ADSR Parameters (tick={tick_ms}ms):")
        print(f"  ENV_MAX:        {env_max}")
        print(f"  ATTACK:         step={attack_step:2d}  → {att_dur:3d}ms  (target ~{att_dur}ms)")
        print(f"  DECAY:          step={decay_step:2d}  → {dec_dur:3d}ms  (delta={dec_delta}, to SUSTAIN_THR={sustain_thr})")
        print(f"  SUSTAIN_THR:    {sustain_thr}")
        print(f"  SUSTAIN_DECAY:  {sustain_decay}  (0=flat)")
        print(f"  RELEASE:        step={release_step:2d}  → {rel_dur:3d}ms  (linear, from SUSTAIN_THR)")

    def close(self):
        self.ser.close()


def main():
    parser = argparse.ArgumentParser(
        description="Music Box 8051 Serial Protocol Client",
        prog="musicbox_proto.py",
    )
    parser.add_argument("--port", default="/dev/ttyUSB0", help="Serial port")
    parser.add_argument("--baud", type=int, default=115200, help="Baud rate")
    parser.add_argument("command", help="Command to execute")
    parser.add_argument("args", nargs="*", help="Command arguments")
    args = parser.parse_args()

    client = MusicBoxClient(args.port, args.baud)
    try:
        cmd = args.command
        if cmd == "ping":
            client.ping()
        elif cmd == "info":
            client.info()
        elif cmd == "reset":
            client.reset()
        elif cmd == "uptime":
            client.uptime()
        elif cmd == "mem":
            client.mem()
        elif cmd == "audio":
            client.audio()
        elif cmd == "adc":
            if not args.args:
                sys.exit("adc requires a channel argument (0-15)")
            client.adc(int(args.args[0]))
        elif cmd == "voice":
            client.voice()
        elif cmd == "sysinfo":
            client.sysinfo()
        elif cmd == "note-on":
            if not args.args:
                sys.exit("note-on requires a MIDI note argument (0-127)")
            vel = int(args.args[1]) if len(args.args) > 1 else None
            client.note_on(int(args.args[0]), vel)
        elif cmd == "note-off":
            if not args.args:
                sys.exit("note-off requires a MIDI note argument (0-127)")
            client.note_off(int(args.args[0]))
        elif cmd == "play":
            client.play()
        elif cmd == "stop":
            client.stop()
        elif cmd == "prev":
            client.prev()
        elif cmd == "next":
            client.next()
        elif cmd == "song":
            if not args.args:
                sys.exit("song requires an index argument")
            client.song(int(args.args[0]))
        elif cmd == "status":
            client.status()
        elif cmd == "flash-info":
            client.flash_info()
        elif cmd == "flash-id":
            client.flash_id()
        elif cmd == "flash-erase":
            if not args.args:
                sys.exit("flash-erase requires an address argument")
            client.flash_erase(int(args.args[0], 0))
        elif cmd == "flash-erase-all":
            client.flash_erase_all()
        elif cmd == "flash-read":
            if len(args.args) < 2:
                sys.exit("flash-read requires ADDR and LEN arguments")
            addr = int(args.args[0], 0)
            length = int(args.args[1], 0)
            outfile = args.args[2] if len(args.args) > 2 else None
            client.flash_read(addr, length, outfile)
        elif cmd == "flash-write":
            if len(args.args) < 2:
                sys.exit("flash-write requires ADDR and INFILE arguments")
            addr = int(args.args[0], 0)
            client.flash_write(addr, args.args[1])
        elif cmd == "adsr-get":
            client.adsr_get()
        else:
            sys.exit(f"Unknown command: {cmd}")
    finally:
        client.close()


if __name__ == "__main__":
    main()
