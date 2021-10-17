import os, sys, unittest
sys.path.insert(0, os.path.join(os.path.dirname(__file__), ".."))

from botbase import BB
from botbase.client import norm


class ButtonTests(unittest.TestCase):
    def test_norm_aliases(self):
        self.assertEqual(norm("a"), "A")
        self.assertEqual(norm("up"), "DUP")

    def test_norm_rejects_typo(self):
        with self.assertRaises(ValueError):
            norm("PLSU")


if __name__ == "__main__":
    unittest.main()
