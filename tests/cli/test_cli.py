"""End-to-end tests: the real botbase binary against fake_server.py.

Run through ctest, or directly with BOTBASE_BIN pointing at the binary:
    BOTBASE_BIN=build/botbase python3 -m unittest -v test_cli
"""
import json
import os
import signal
import subprocess
import tempfile
import time
import unittest

from fake_server import HEAP, MAIN, FakeBotbase

BINARY = os.environ.get("BOTBASE_BIN", os.path.join(os.path.dirname(__file__), "..", "..", "build", "botbase"))


class CliTest(unittest.TestCase):
    server_options = {}

    def setUp(self):
        self.server = FakeBotbase(**self.server_options)
        self.cache = tempfile.TemporaryDirectory()
        self.env = {**os.environ, "XDG_CACHE_HOME": self.cache.name}
        for name in ("BOTBASE_HOST", "BOTBASE_PORT", "BOTBASE_INTERFACE", "BOTBASE_SUBNET"):
            self.env.pop(name, None)

    def tearDown(self):
        self.server.close()
        self.cache.cleanup()

    def argv(self, *args, host=True):
        target = ["--host", "127.0.0.1", "--port", str(self.server.port)] if host else ["--port", str(self.server.port)]
        return [BINARY, "--timeout", "3", *target, *args]

    def run_cli(self, *args, stdin=None, host=True):
        return subprocess.run(self.argv(*args, host=host), input=stdin, capture_output=True, env=self.env, timeout=30)

    def ok(self, *args, **kwargs):
        result = self.run_cli(*args, **kwargs)
        self.assertEqual(result.returncode, 0, result.stderr.decode())
        return result.stdout.decode()

    def sent(self):
        return self.server.lines


class ProtocolTests(CliTest):
    def test_silent_commands_are_fenced(self):
        self.ok("click", "a")
        self.assertEqual(self.sent()[-2:], ["click A", "getVersion"])

    def test_unknown_button_sends_nothing(self):
        result = self.run_cli("click", "A", "PLSU")
        self.assertEqual(result.returncode, 2)
        self.assertIn(b"would press A", result.stderr)
        self.assertEqual(self.sent(), [])

    def test_info(self):
        out = self.ok("info")
        self.assertIn("title_id       0100000000010000", out)
        self.assertIn("heap_base      0x1000000000", out)
        self.assertIn("memory_access  yes", out)
        data = json.loads(self.ok("info", "--json"))
        self.assertEqual(data["charge"], 87)
        self.assertEqual(data["title_version"], 31)
        self.assertEqual(data["heap_base"], HEAP)
        self.assertEqual(data["name"], "Test Game")
        self.assertIs(data["memory_access"], True)

    def test_peek_and_poke(self):
        self.ok("poke", "0x40", "01020304")
        self.assertEqual(self.server.read(HEAP + 0x40, 4), b"\x01\x02\x03\x04")
        self.assertEqual(self.ok("peek", "0x40", "4", "--as", "hex"), "01020304\n")
        self.ok("poke", "0x8", "0x4D4F4430", "--as", "u32", "--region", "main")
        self.assertEqual(self.ok("peek", "0x8", "4", "--as", "u32", "--region", "main"), f"{0x4D4F4430}\n")
        self.assertEqual(self.ok("peek", hex(MAIN + 0x8), "4", "--region", "absolute", "--as", "hex"), "30444F4D\n")
        self.ok("poke", "0x50", "1.5", "--as", "f32")
        self.assertEqual(self.ok("peek", "0x50", "4", "--as", "f32"), "1.5\n")
        self.assertEqual(self.ok("poke", "0x40", "aa", "--verify"), "verified\n")

    def test_peek_dump(self):
        self.server.write(HEAP + 0x10, b"Hi!\x00")
        self.assertEqual(self.ok("peek", "0x10", "4"), "000000000010  48 69 21 00" + " " * 36 + "  Hi!.\n")

    def test_peek_multi(self):
        self.server.write(HEAP, bytes(range(16)))
        self.assertEqual(self.ok("peek-multi", "0:2", "8:3"), "0001\n08090A\n")

    def test_failed_read(self):
        result = self.run_cli("peek", "0x999999", "4")
        self.assertEqual(result.returncode, 7)
        self.assertIn(b"MemoryAccessError", result.stderr)

    def test_pointers(self):
        self.server.write(HEAP + 0x110, b"\xaa\xbb")
        self.assertEqual(self.ok("pointer", "0x10", "-0x20", "--final", "0x10"), f"{HEAP + 0x110:#x}\n")
        self.assertIn("pointerAll 0x10 -0x20 0x10", self.sent())
        self.assertEqual(self.ok("pointer", "0x10", "--final", "0x10", "--peek", "2"), "AABB\n")
        self.assertEqual(self.ok("pointer", "0x10", "--final", "0x10", "--relative"), "0x110\n")
        self.assertEqual(self.ok("pointer", "0x10"), f"{HEAP + 0x200:#x}\n")

    def test_freezes(self):
        self.ok("freeze", "0x20", "63")
        self.assertEqual(self.ok("freezes", "count"), "1\n")
        self.ok("freezes", "clear")
        self.assertEqual(self.ok("freezes", "count"), "0\n")

    def test_click_seq_waits_for_done(self):
        self.ok("seq", "DR", "W300", "+L", "%-36864,100", "-L", "--server")
        self.assertIn("clickSeq DR,W300,+L,%-32768,100,-L", self.sent())

    def test_client_sequence(self):
        self.ok("seq", "A,W10,+L,%0,32767,-L")
        self.assertEqual([line for line in self.sent() if line != "getVersion"],
                         ["click A", "press L", "setStick LEFT 0x0 0x7fff", "release L"])

    def test_typing_uses_shift_chords(self):
        self.ok("type", "aB!")
        self.assertIn("key 4", self.sent())
        self.assertIn("keyMulti 225 5", self.sent())
        self.assertIn("keyMulti 225 30", self.sent())

    def test_screenshot(self):
        with tempfile.TemporaryDirectory() as folder:
            path = os.path.join(folder, "shot.jpg")
            self.assertEqual(self.ok("screenshot", "-o", path), path + "\n")
            with open(path, "rb") as f:
                self.assertTrue(f.read().startswith(b"\xff\xd8\xff"))
        piped = self.run_cli("screenshot", "-o", "-")
        self.assertTrue(piped.stdout.startswith(b"\xff\xd8\xff"))

    def test_touch_bounds(self):
        result = self.run_cli("touch", "1300", "10")
        self.assertEqual(result.returncode, 2)
        self.ok("touch", "100", "200", "300", "400")
        self.assertIn("touch 100 200 300 400", self.sent())

    def test_running_exit_code(self):
        self.assertEqual(self.run_cli("running", "0x0100000000010000").returncode, 0)
        self.assertEqual(self.run_cli("running", "0x0100000000099999").returncode, 1)

    def test_framing_breakers_are_refused(self):
        result = self.run_cli("config", "echoCommands", "1")
        self.assertEqual(result.returncode, 2)
        self.assertNotIn("configure echoCommands 1", self.sent())

    def test_negative_stick_values(self):
        self.ok("stick", "left", "-32768", "0")
        self.assertIn("setStick LEFT -0x8000 0x0", self.sent())


class SafetyTests(CliTest):
    server_options = {"delays": {"pixelPeek": 1.5}}

    def interrupt_after(self, seconds, *args):
        process = subprocess.Popen(self.argv(*args), stdout=subprocess.PIPE, stderr=subprocess.PIPE, env=self.env)
        time.sleep(seconds)
        process.send_signal(signal.SIGINT)
        out, err = process.communicate(timeout=15)
        return process.returncode, err.decode()

    def test_interrupted_hold_releases_the_button(self):
        code, err = self.interrupt_after(0.8, "hold", "A", "5000")
        self.assertEqual(code, 130, err)
        self.assertIn("release A", self.sent())

    def test_interrupted_tilt_centres_the_stick(self):
        code, err = self.interrupt_after(0.8, "stick", "right", "0", "32767", "--ms", "5000")
        self.assertEqual(code, 130, err)
        self.assertEqual(self.sent()[-2:], ["setStick RIGHT 0x0 0x0", "getVersion"])

    def test_interrupt_mid_reply_resyncs_before_cleanup(self):
        code, err = self.interrupt_after(0.8, "--fast", "screenshot", "-o", os.devnull)
        self.assertEqual(code, 130, err)
        self.assertNotIn("ProtocolError", err)
        self.assertEqual(self.sent()[-2:], ["configure mainLoopSleepTime 50", "getVersion"])

    def test_fast_is_undone_at_exit(self):
        self.ok("--fast", "click", "A")
        self.assertEqual(self.sent()[0], "configure mainLoopSleepTime 0")
        self.assertIn("configure mainLoopSleepTime 50", self.sent())

    def test_detach_at_exit(self):
        self.ok("--detach", "click", "A")
        self.assertIn("detachController", self.sent())

    def test_failed_script_lets_go(self):
        result = self.run_cli("batch", "-e", "press ZR; peek 0x999999 4; click A")
        self.assertEqual(result.returncode, 7)
        self.assertIn("release ZR", self.sent())
        self.assertNotIn("click A", self.sent())


class ScriptTests(CliTest):
    def test_batch_uses_one_connection(self):
        self.ok("batch", "-e", "click A; wait 10", "-e", "click B")
        self.assertIn("click A", self.sent())
        self.assertIn("click B", self.sent())

    def test_batch_from_stdin(self):
        out = self.ok("batch", stdin=b"# comment\ninfo --json\nrunning 0x0100000000010000\n")
        self.assertIn('"charge": 87', out)
        self.assertTrue(out.endswith("yes\n"))

    def test_keep_going(self):
        result = self.run_cli("batch", "-k", "-e", "click PLSU; click B")
        self.assertEqual(result.returncode, 2)
        self.assertIn("click B", self.sent())

    def test_shell_reads_stdin(self):
        out = self.ok("shell", stdin=b"freezes count\nraw getVersion\nquit\nclick A\n")
        self.assertEqual(out, "0\n2.6\n")
        self.assertNotIn("click A", self.sent())

    def test_scripts_cannot_nest(self):
        result = self.run_cli("batch", "-e", "shell")
        self.assertEqual(result.returncode, 2)


class DiscoveryTests(CliTest):
    def test_discover_finds_and_remembers(self):
        out = self.ok("discover", "--subnet", "127.0.0.1/32", host=False)
        self.assertEqual(out, "127.0.0.1\tsys-botbase 2.6\n")
        with open(os.path.join(self.cache.name, "botbase", "host")) as f:
            self.assertEqual(f.read(), "127.0.0.1\n")
        self.assertIn("2.6", self.ok("raw", "getVersion", host=False))

    def test_no_console_found(self):
        self.env["BOTBASE_SUBNET"] = "127.0.0.2/32"
        result = self.run_cli("info", host=False)
        self.assertEqual(result.returncode, 3)
        self.assertIn(b"no sys-botbase on 127.0.0.2/32", result.stderr)


class BusyAndIdleTests(CliTest):
    def test_debugger_busy(self):
        self.server.debugger_busy = True
        self.assertIn("memory_access  no", self.ok("info"))
        result = self.run_cli("peek", "0x40", "4", "--as", "hex")
        self.assertEqual(result.returncode, 7)

    def test_no_game(self):
        self.server.game_running = False
        self.assertIn("title_id\n", self.ok("info"))
        result = self.run_cli("game", "name")
        self.assertEqual(result.returncode, 5)


if __name__ == "__main__":
    unittest.main()
