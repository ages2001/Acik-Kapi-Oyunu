"""Package boundary checks; the validation archive also compares frozen media."""
import tempfile
import unittest
from pathlib import Path
from package_legacy import assemble, render_inf


class PackageTests(unittest.TestCase):
    def test_self_size_decimal_boundary_and_crlf(self):
        template = b"1,X.SYS , SIZE=1\n1,OEMSETUP.INF , SIZE=1, RENAME=RLTKNT31.INF\n"
        for padding in (0, 30, 930, 9930):
            result = render_inf(template + b";" * padding, {"X.SYS": b"x" * 12345}, "RLTKNT31.INF")
            self.assertIn(b"SIZE=12345\r\n", result)
            self.assertIn(b"SIZE=" + str(len(result)).encode() + b", RENAME=", result)
            self.assertNotIn(b"\n", result.replace(b"\r\n", b""))

    def test_duplicate_missing_extra_and_wrong_destination_rejected(self):
        good = b"1,X.SYS , SIZE=1\n1,OEMSETUP.INF , SIZE=1, RENAME=RLTKNT31.INF\n"
        for bad in (good + b"1,X.SYS , SIZE=2\n", good.replace(b"X.SYS", b"Y.SYS"),
                    good + b"1,Y.SYS , SIZE=1\n", good.replace(b"RLTKNT31", b"RLTKNT35")):
            with self.subTest(template=bad), self.assertRaises(ValueError):
                render_inf(bad, {"X.SYS": b"x"}, "RLTKNT31.INF")

    def test_wrong_helper_and_existing_output_preserved(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            names = ("RLTKLANT.SYS", "RLTUSB.EXE", "RLTSETUP.DLL")
            sources = {name: root / name for name in names}
            for path in sources.values():
                path.write_bytes(b"MZfixture")
            sources["RLTSETUP.DLL"].write_bytes(b"MZRLTKNT31.INF\0")
            template = root / "template.inf"
            template.write_text("".join("1," + n + " , SIZE=1\n" for n in names) +
                                "1,OEMSETUP.INF , SIZE=1, RENAME=RLTKNT35.INF\n")
            output = root / "out"
            with self.assertRaises(ValueError):
                assemble("nt35", template, sources, output)
            self.assertFalse(output.exists())
            sources["RLTSETUP.DLL"].write_bytes(b"MZRLTKNT35.INF\0")
            assemble("nt35", template, sources, output)
            before = {p.name: p.read_bytes() for p in output.iterdir()}
            with self.assertRaises(FileExistsError):
                assemble("nt35", template, sources, output)
            self.assertEqual(before, {p.name: p.read_bytes() for p in output.iterdir()})


if __name__ == "__main__":
    unittest.main()
