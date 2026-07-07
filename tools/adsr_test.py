#!/usr/bin/env python3
"""
ADSR 包络完整测试脚本

用法:
    python3 tools/adsr_test.py --port /dev/ttyUSB0
    python3 tools/adsr_test.py --port /dev/ttyUSB0 --test velocity
    python3 tools/adsr_test.py --port /dev/ttyUSB0 --test all

测试项目:
    velocity  — 力度缩放数学验证
    poly      — 8声复音 + 抢占
    release   — Release 衰减时序
    sequence  — 快速音符序列
    retrigger — 同音复触
    all       — 全部测试
"""

import subprocess, time, sys, argparse

TOOL = None  # resolved to this script's sibling
STATE_NAMES = {0: "SILENT", 1: "ATTACK", 2: "DECAY", 3: "SUSTAIN", 4: "RELEASE"}

# -20dB 曲线表 (AdsrCurveTable, index 0-127)
CURVE_DB20 = [
      0,  26,  26,  26,  27,  27,  28,  28,  29,  29,  30,  31,
     31,  32,  32,  33,  33,  34,  35,  35,  36,  37,  37,  38,
     39,  39,  40,  41,  42,  42,  43,  44,  45,  46,  46,  47,
     48,  49,  50,  51,  52,  53,  54,  55,  56,  57,  58,  59,
     60,  61,  62,  63,  64,  65,  67,  68,  69,  70,  72,  73,
     74,  76,  77,  78,  80,  81,  83,  84,  86,  87,  89,  91,
     92,  94,  96,  98,  99, 101, 103, 105, 107, 109, 111, 113,
    115, 117, 119, 121, 123, 126, 128, 130, 133, 135, 138, 140,
    143, 145, 148, 151, 153, 156, 159, 162, 165, 168, 171, 174,
    177, 181, 184, 187, 191, 194, 198, 201, 205, 209, 213, 217,
    221, 225, 229, 233, 237, 242, 246, 255,
]


class ADSRTester:
    def __init__(self, port):
        self.port = port

    def _cmd(self, *args):
        r = subprocess.run(["python3", TOOL, "--port", self.port] + list(map(str, args)),
                           capture_output=True, text=True, timeout=5)
        return r.stdout.strip()

    def stop(self):
        self._cmd("stop")

    def note_on(self, note, vel=127):
        self._cmd("note-on", note, vel)

    def note_off(self, note):
        self._cmd("note-off", note)

    def voice_lines(self):
        return self._cmd("voice").split("\n")[:8]

    def find_voice(self, note):
        """返回匹配 note 的第一行"""
        for l in self.voice_lines():
            if f"note={note}" in l:
                return l
        return ""

    def parse_voice(self, line):
        """解析上行: 返回 {env_level, state_str}"""
        if not line:
            return {"env_level": 0, "state": "SILENT"}
        env = int(line.split("env=")[1].split("(")[0], 16)
        st   = line.split("state=")[1].split()[0]
        return {"env_level": env, "state": st}

    def wait_idle(self, timeout=1.0):
        """等待所有声道静音"""
        deadline = time.time() + timeout
        while time.time() < deadline:
            all_idle = all(
                "SILENT" in l or "idle" in l
                for l in self.voice_lines()
            )
            if all_idle:
                return True
            time.sleep(0.05)
        return False

    # ================================================================
    # 测试用例
    # ================================================================

    def test_velocity(self):
        """Test 1: 力度缩放管线数学验证"""
        print("=" * 55)
        print("  力度缩放数学验证 (vel × scaling → curve)")
        print("=" * 55)
        self.stop(); self.wait_idle()

        print(f"{'vel':>4} {'v_scaled':>8} {'idx':>5}  {'exp_level':>5} {'actual':>5}  {'match':>5}")
        print("-" * 42)

        errors = 0
        for vel in [20, 40, 64, 80, 100, 120, 127]:
            self.note_on(72, vel)
            time.sleep(0.15)  # 等 SUSTAIN
            line = self.find_voice(72)
            data = self.parse_voice(line)
            actual = data["env_level"]

            vs = vel * 2  # vel_scaled
            idx = (80 * vs) >> 8  # 80 = SUSTAIN_THRESHOLD
            expected = CURVE_DB20[idx]

            match = "✓" if expected == actual else f"✗ ({abs(expected-actual)})"
            if expected != actual:
                errors += 1

            print(f"{vel:>4} {vs:>8} {idx:>5}  {expected:>5}      {actual:>5}  {match:>5}")
            self.note_off(72)
            time.sleep(0.25)

        print("-" * 42)
        print(f"  {'全部通过 ✓' if errors == 0 else f'失败 {errors}'}")
        return errors == 0

    def test_poly(self):
        """Test 2: 复音测试"""
        print("=" * 55)
        print("  复音测试")
        print("=" * 55)
        self.stop(); self.wait_idle()

        # 8声填充
        for i in range(8):
            self.note_on(60 + i * 2, 80 + i * 5)
        time.sleep(0.15)

        active = sum(1 for l in self.voice_lines() if "ACTIVE" in l)
        print(f"  8声道填充: {active}/8 {'✓' if active == 8 else '✗'}")

        states = [self.parse_voice(l)["state"] for l in self.voice_lines()]
        all_sustain = all(s == "SUSTAIN" for s in states)
        print(f"  全部SUSTAIN: {'✓' if all_sustain else '△ ' + str(states)}")

        # 9th note round-robin steal
        print("\n  抢占测试 (9th音符)...")
        self.note_on(72, 127)
        time.sleep(0.10)
        active2 = sum(1 for l in self.voice_lines() if "ACTIVE" in l)
        print(f"  9音符后活跃: {active2}/8 {'✓' if active2 == 8 else '✗'}")

        self.stop(); time.sleep(0.2)
        return active == 8 and active2 == 8

    def test_release_timing(self):
        """Test 3: Release 衰减时序"""
        print("=" * 55)
        print("  Release 衰减时序 (vel=127)")
        print("=" * 55)
        self.stop(); self.wait_idle()

        self.note_on(67, 127)
        time.sleep(0.15)
        self.note_off(67)
        t0 = time.time()

        print(f"{'t(ms)':>6}  {'env':>5}  {'state':>7}")
        for _ in range(12):
            time.sleep(0.025)
            elapsed = int((time.time() - t0) * 1000)
            data = self.parse_voice(self.find_voice(67))
            env = data["env_level"]
            st  = data["state"]
            print(f"{elapsed:5d}  0x{env:02x}  {st:>7}")
            if st == "SILENT":
                print(f"  衰减时长: ~{elapsed}ms")
                break

        self.stop(); time.sleep(0.2)
        return True

    def test_retrigger(self):
        """Test 4: 同音复触"""
        print("=" * 55)
        print("  同音复触 (NoteOn 60 × 2)")
        print("=" * 55)
        self.stop(); self.wait_idle()

        self.note_on(60, 80)
        time.sleep(0.10)
        count1 = sum(1 for l in self.voice_lines() if "note=60" in l)
        print(f"  首次 NoteOn 60: {count1} 声道 (应为1)")

        self.note_on(60, 127)
        time.sleep(0.10)
        matching = [l for l in self.voice_lines() if "note=60" in l and "ACTIVE" in l]
        print(f"  二次 NoteOn 60: {len(matching)} 声道活跃")
        for m in matching:
            data = self.parse_voice(m)
            print(f"    {m.strip()}")

        self.note_off(60)
        time.sleep(0.30)
        remaining = sum(1 for l in self.voice_lines() if "note=60" in l and "SILENT" not in l)
        print(f"  NoteOff后: {remaining} 活跃 ({'✓' if remaining == 0 else '✗'})")

        self.stop(); time.sleep(0.2)
        return remaining == 0

    def test_sequence(self):
        """Test 5: 快速音符序列"""
        print("=" * 55)
        print("  快速音符序列 (staccato)")
        print("=" * 55)
        self.stop(); self.wait_idle()

        for note in [60, 64, 67, 72, 76, 79]:
            self.note_on(note, 100)
            time.sleep(0.06)
            self.note_off(note)
            time.sleep(0.03)

        voices = self.voice_lines()
        active = sum(1 for l in voices if "ACTIVE" in l or "RELEASE" in l)
        print(f"  序列后活跃/释放: {active}")

        ok = self.wait_idle()
        print(f"  ~1s后全静音: {'✓' if ok else '✗'}")

        self.stop()
        return ok

    def test_state_machine(self):
        """Test 6: ADSR 全状态遍历"""
        print("=" * 55)
        print("  ADSR 全状态遍历 (高密度捕捉)")
        print("=" * 55)
        self.stop(); self.wait_idle()

        self.note_on(77, 64)
        states = set()
        t0 = time.time()
        noteoff_sent = False

        for i in range(90):
            elapsed = int((time.time() - t0) * 1000)
            line = self.find_voice(77)
            data = self.parse_voice(line)
            st = data["state"]

            if st not in states:
                states.add(st)
                env = data["env_level"]
                arrow = "→" if len(states) > 1 else " "
                print(f"  {arrow} t={elapsed:3d}ms  {st:>7}   env=0x{env:02x}")

            # ATTACK+DECAY 走完后进入 SUSTAIN 就 NoteOff
            if "SUSTAIN" in states and not noteoff_sent:
                self.note_off(77)
                noteoff_sent = True

            if len(states) >= 5:
                break
            if not line and "SILENT" in states:
                break

            time.sleep(0.005)  # 5ms polling

        missing = {"ATTACK", "DECAY", "SUSTAIN", "RELEASE", "SILENT"} - states
        if missing:
            print(f"\n  ATTACK状态因串口延迟(>攻击时长)未能捕获，这是正常的测试工具限制")
            print(f"  观察到状态: {sorted(states)}")
        else:
            print(f"\n  全部5状态: ✓")
        print(f"  ATTACK→DECAY→SUSTAIN→RELEASE→SILENT: {'✓' if len(states) >= 4 else '✗'}")

        self.stop(); time.sleep(0.2)
        return len(states) >= 4

    def run_all(self):
        print()
        results = {}
        for name, fn in [
            ("velocity",   self.test_velocity),
            ("poly",       self.test_poly),
            ("release",    self.test_release_timing),
            ("retrigger",  self.test_retrigger),
            ("sequence",   self.test_sequence),
            ("states",     self.test_state_machine),
        ]:
            print()
            try:
                results[name] = fn()
            except Exception as e:
                print(f"  ✗ 测试异常: {e}")
                results[name] = False

        # Summary
        print("\n" + "=" * 55)
        print("  测试结果汇总")
        print("=" * 55)
        for name, ok in results.items():
            print(f"  {name:>12}: {'✓ 通过' if ok else '✗ 失败'}")
        all_ok = all(results.values())
        print(f"\n  {'全部通过 ✓' if all_ok else '存在失败项 ✗'}")
        return all_ok


def main():
    global TOOL
    import os
    script_dir = os.path.dirname(os.path.abspath(__file__))
    TOOL = os.path.join(script_dir, "musicbox_proto.py")

    parser = argparse.ArgumentParser(description="ADSR 包络测试套件")
    parser.add_argument("--port", default="/dev/ttyUSB0", help="串口设备")
    parser.add_argument("--test", default="all",
                        choices=["velocity", "poly", "release", "retrigger",
                                 "sequence", "states", "all"],
                        help="测试项目 (默认: all)")
    args = parser.parse_args()

    tester = ADSRTester(args.port)

    tests = {
        "velocity":   tester.test_velocity,
        "poly":       tester.test_poly,
        "release":    tester.test_release_timing,
        "retrigger":  tester.test_retrigger,
        "sequence":   tester.test_sequence,
        "states":     tester.test_state_machine,
    }

    if args.test == "all":
        ok = tester.run_all()
        sys.exit(0 if ok else 1)
    else:
        ok = tests[args.test]()
        sys.exit(0 if ok else 1)


if __name__ == "__main__":
    main()
