import struct
import tempfile
import unittest
from pathlib import Path

from dig_probe import BLOCK_SIZE, probe_dig


def make_dig(*, payload_size=16, corrupt_end=False, invalid_compression=False):
    record_count = BLOCK_SIZE // 16
    archive = bytearray(BLOCK_SIZE + payload_size)
    # NNL's first payload starts at LBA 1, so the first table record implies
    # 2048 / 16 = 128 records.
    compressed_size = payload_size + (1 if corrupt_end else 0)
    is_compressed = 2 if invalid_compression else 0
    struct.pack_into(
        "<IIHHI", archive, 0,
        1, compressed_size, 1, is_compressed, compressed_size
    )
    # Remaining records are zero-valued empty placeholders.
    archive[BLOCK_SIZE:] = b"D" * payload_size
    return archive


class DigProbeTests(unittest.TestCase):
    def probe_bytes(self, data):
        with tempfile.TemporaryDirectory() as tmp:
            path = Path(tmp) / "candidate.bin"
            path.write_bytes(data)
            return probe_dig(path)

    def test_accepts_structurally_valid_nnl_dig_archive(self):
        result = self.probe_bytes(make_dig())
        self.assertTrue(result["valid"], result["reason"])
        self.assertEqual(result["record_count"], 128)

    def test_rejects_payload_range_outside_file(self):
        result = self.probe_bytes(make_dig(corrupt_end=True))
        self.assertFalse(result["valid"])
        self.assertIn("extends beyond", result["reason"])

    def test_rejects_invalid_compression_flag(self):
        result = self.probe_bytes(make_dig(invalid_compression=True))
        self.assertFalse(result["valid"])
        self.assertIn("invalid record", result["reason"])

    def test_rejects_small_non_dig_file(self):
        result = self.probe_bytes(b"\x00" * BLOCK_SIZE)
        self.assertFalse(result["valid"])
        self.assertEqual(result["record_count"], 0)


if __name__ == "__main__":
    unittest.main()
