import unittest
from naruto_asset_tool import read_psf_fields

class UnifiedToolTests(unittest.TestCase):
    def test_invalid_sfo(self):
        self.assertEqual(read_psf_fields(b"not a PSF"), {})
    def test_truncated_sfo(self):
        self.assertEqual(read_psf_fields(b"\x00PSF"+bytes(20)), {})

if __name__ == "__main__": unittest.main()
