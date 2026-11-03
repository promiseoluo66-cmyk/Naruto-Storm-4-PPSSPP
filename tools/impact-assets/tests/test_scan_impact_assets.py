import mmap
import struct
import sys
import tempfile
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from scan_impact_assets import inspect_gmo, inspect_gim, inspect_riff


class ScannerFormatTests(unittest.TestCase):
    def inspect(self, payload, function):
        with tempfile.TemporaryFile() as handle:
            handle.write(payload)
            handle.flush()
            with mmap.mmap(handle.fileno(), 0, access=mmap.ACCESS_READ) as data:
                return function(data, 0)

    def test_gmo_outer_chunk_bounds(self):
        payload = b"OMG.00.1PSP" + b"\x00" * 5 + struct.pack("<HHI", 2, 8, 16) + b"x" * 16
        status, size, _ = self.inspect(payload, inspect_gmo)
        self.assertEqual(status, "validated-outer-chunk")
        self.assertEqual(size, len(payload))

    def test_gmo_rejects_out_of_bounds_size(self):
        payload = b"OMG.00.1PSP" + b"\x00" * 5 + struct.pack("<HHI", 2, 8, 999) + b"x" * 16
        status, _, _ = self.inspect(payload, inspect_gmo)
        self.assertEqual(status, "header-inconsistent")

    def test_gim_requires_root_block(self):
        payload = b"MIG.00.1PSP" + b"\x00" * 5 + struct.pack("<HHIII", 2, 0, 64, 16, 16) + b"x" * 64
        status, size, _ = self.inspect(payload, inspect_gim)
        self.assertEqual(status, "plausible-root-block")
        self.assertIsNone(size)  # GIM block-size convention is not used for carving.

    def test_riff_declared_length(self):
        payload = b"RIFF" + struct.pack("<I", 8) + b"WAVE" + b"DATA"
        status, size, _ = self.inspect(payload, inspect_riff)
        self.assertEqual(status, "validated-length")
        self.assertEqual(size, len(payload))


if __name__ == "__main__":
    unittest.main()
