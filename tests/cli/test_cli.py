# this runs the cpp binary against the python fake_server from the old
# tests folder. same protocol, same behaviour. kept here so we dont have
# two servers to maintain
import os
import subprocess
import sys
import unittest

sys.path.insert(0, os.path.join(os.path.dirname(__file__), "..", ".."))
from tests.fake_server import FakeServer

BINARY = os.environ.get("BOTBASE_BIN", "./build/botbase")


class CliTest(unittest.TestCase):
    def setUp(self):
        self.server = FakeServer()

    def test_version(self):
        r = subprocess.run([BINARY, f"127.0.0.1:{self.server.port}", "version"],
                           capture_output=True, text=True)
        self.assertEqual(r.returncode, 0)
        self.assertIn("2.0", r.stdout)


if __name__ == "__main__":
    unittest.main()
