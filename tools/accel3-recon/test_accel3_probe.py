import tempfile
import unittest
from pathlib import Path

from accel3_probe import detect_magic, parse_record, inventory_iso

class ProbeTests(unittest.TestCase):
    def test_magic(self):
        self.assertEqual(detect_magic(b"CPK " + bytes(8)), "CRI CPK")
        self.assertEqual(detect_magic(b"\x7fELF" + bytes(8)), "ELF executable")
        self.assertEqual(detect_magic(b"\x00" * 8), "unknown / game-specific")

    def test_parse_record(self):
        rec = bytearray(42)
        rec[0] = len(rec)
        rec[2:6] = (123).to_bytes(4, "little")
        rec[10:14] = (456).to_bytes(4, "little")
        rec[25] = 0
        rec[32] = 9
        rec[33:42] = b"FILE.BIN;"
        parsed = parse_record(bytes(rec))
        self.assertEqual(parsed["name"], "FILE.BIN")
        self.assertEqual(parsed["extent"], 123)
        self.assertEqual(parsed["size"], 456)

    def test_reject_non_iso(self):
        with tempfile.TemporaryDirectory() as td:
            p = Path(td) / "bad.iso"
            p.write_bytes(b"not an iso")
            with self.assertRaises(ValueError):
                inventory_iso(p)

if __name__ == "__main__":
    unittest.main()
