import tempfile
import unittest
from pathlib import Path

from accel3_probe import detect_magic, parse_record, inventory_iso, SECTOR

def iso_record(name: bytes, extent: int, size: int, is_dir: bool = False) -> bytes:
    padding = 1 if len(name) % 2 == 0 else 0
    length = 33 + len(name) + padding
    rec = bytearray(length)
    rec[0] = length
    rec[2:6] = extent.to_bytes(4, "little")
    rec[10:14] = size.to_bytes(4, "little")
    rec[25] = 0x02 if is_dir else 0
    rec[32] = len(name)
    rec[33:33 + len(name)] = name
    return bytes(rec)

class ProbeTests(unittest.TestCase):
    def test_magic(self):
        self.assertEqual(detect_magic(b"CPK " + bytes(8)), "CRI CPK")
        self.assertEqual(detect_magic(b"\x7fELF" + bytes(8)), "ELF executable")
        self.assertEqual(detect_magic(b"\x00" * 8), "unknown / game-specific")

    def test_parse_record(self):
        rec = iso_record(b"FILE.BIN;1", 123, 456)
        parsed = parse_record(rec)
        self.assertEqual(parsed["name"], "FILE.BIN")
        self.assertEqual(parsed["extent"], 123)
        self.assertEqual(parsed["size"], 456)

    def test_inventory_minimal_psp_iso_and_game_id(self):
        image = bytearray(21 * SECTOR)

        # Primary Volume Descriptor at sector 16.
        pvd = memoryview(image)[16 * SECTOR:17 * SECTOR]
        pvd[0] = 1
        pvd[1:6] = b"CD001"
        pvd[6] = 1
        pvd[128:130] = SECTOR.to_bytes(2, "little")
        root = iso_record(b"\x00", 18, SECTOR, True)
        pvd[156:156 + len(root)] = root

        # Volume descriptor terminator at sector 17.
        term = memoryview(image)[17 * SECTOR:18 * SECTOR]
        term[0] = 255
        term[1:6] = b"CD001"
        term[6] = 1

        # Root directory at sector 18.
        root_dir = (
            iso_record(b"\x00", 18, SECTOR, True)
            + iso_record(b"\x01", 18, SECTOR, True)
            + iso_record(b"PSP_GAME", 19, SECTOR, True)
        )
        image[18 * SECTOR:18 * SECTOR + len(root_dir)] = root_dir

        # PSP_GAME directory at sector 19.
        game_dir = (
            iso_record(b"\x00", 19, SECTOR, True)
            + iso_record(b"\x01", 18, SECTOR, True)
            + iso_record(b"PARAM.SFO;1", 20, 9, False)
        )
        image[19 * SECTOR:19 * SECTOR + len(game_dir)] = game_dir
        image[20 * SECTOR:20 * SECTOR + 9] = b"ULJS00236"

        with tempfile.TemporaryDirectory() as td:
            p = Path(td) / "fixture.iso"
            p.write_bytes(image)
            report = inventory_iso(p)

        self.assertEqual(report["entry_count"], 2)
        self.assertEqual(report["param_sfo_game_id_matches"], ["ULJS00236"])
        self.assertEqual(report["entries"][0]["path"], "PSP_GAME")
        self.assertEqual(report["entries"][1]["path"], "PSP_GAME/PARAM.SFO")

    def test_reject_non_iso(self):
        with tempfile.TemporaryDirectory() as td:
            p = Path(td) / "bad.iso"
            p.write_bytes(b"not an iso")
            with self.assertRaises(ValueError):
                inventory_iso(p)

if __name__ == "__main__":
    unittest.main()
