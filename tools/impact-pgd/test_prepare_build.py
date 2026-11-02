import subprocess
import sys
import tempfile
import unittest
from pathlib import Path


SCRIPT = Path(__file__).with_name("prepare_build.py")


class PrepareBuildTests(unittest.TestCase):
    def test_copies_libkirk_and_hardens_descriptor_math(self):
        old = """\tpgd->align_size = (pgd->data_size+15)&~15;
\tpgd->table_offset = pgd->data_offset+pgd->align_size;
\tpgd->block_nr = (pgd->align_size+pgd->block_size-1)&~(pgd->block_size-1);
\tpgd->block_nr = pgd->block_nr/pgd->block_size;
"""
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            source = root / "ppsspp"
            libkirk = source / "ext" / "libkirk"
            libkirk.mkdir(parents=True)
            (libkirk / "CMakeLists.txt").write_text("add_library(kirk STATIC amctrl.c)\n")
            amctrl = libkirk / "amctrl.c"
            amctrl.write_text("#include <string.h>\n" + old)
            out = root / "prepared"

            result = subprocess.run(
                [
                    sys.executable,
                    str(SCRIPT),
                    "--ppsspp-src",
                    str(source),
                    "--output-dir",
                    str(out),
                ],
                check=False,
                capture_output=True,
                text=True,
            )
            self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
            patched = (out / "amctrl.c").read_text()
            self.assertIn("#include <stdint.h>", patched)
            self.assertIn("const uint64_t block_span", patched)
            self.assertIn("block_span > UINT32_MAX", patched)
            self.assertIn("pgd->block_nr = (u32)block_count;", patched)
            self.assertNotIn("pgd->align_size = (pgd->data_size+15)&~15;", patched)
            self.assertEqual(amctrl.read_text(), "#include <string.h>\n" + old)

    def test_refuses_unrecognised_upstream_layout(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            libkirk = root / "ppsspp" / "ext" / "libkirk"
            libkirk.mkdir(parents=True)
            (libkirk / "amctrl.c").write_text("#include <string.h>\n")
            out = root / "prepared"
            result = subprocess.run(
                [
                    sys.executable,
                    str(SCRIPT),
                    "--ppsspp-src",
                    str(root / "ppsspp"),
                    "--output-dir",
                    str(out),
                ],
                check=False,
                capture_output=True,
                text=True,
            )
            self.assertNotEqual(result.returncode, 0)
            self.assertIn("refusing an unsafe build", result.stdout + result.stderr)


if __name__ == "__main__":
    unittest.main()
