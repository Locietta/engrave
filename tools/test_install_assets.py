import json
import tempfile
import unittest
from pathlib import Path
from unittest.mock import patch

import install_assets as assets


class InstallTests(unittest.TestCase):
    def test_manifest_is_portable_across_checkout_newlines(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            manifest = root / "manifest.json"
            manifest.write_bytes(b'{\r\n  "version": 2,\r\n  "files": {}\r\n}\r\n')
            with patch.object(assets, "MANIFEST", manifest):
                assets.install(root / "installed")
                expected = manifest.read_bytes().replace(b"\r\n", b"\n")
                self.assertEqual((root / "installed/assets.json").read_bytes(), expected)
                manifest.write_bytes(expected)
                assets.install(root / "installed", verify=True)

    def test_install_verify_and_failures(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            manifest = root / "manifest.json"
            data = b"unchanged font/profile/instrument bytes\x00\xff"
            manifest.write_text(json.dumps({"version": 2, "files": {"fonts/test": "src/fonts/test"}}))
            destination = root / "installed"
            with (
                patch.object(assets, "MANIFEST", manifest),
                patch.object(assets, "read_blobs", return_value={"src/fonts/test": data}),
            ):
                assets.install(destination)
                assets.install(destination, verify=True)
                self.assertEqual((destination / "fonts/test").read_bytes(), data)
                with self.assertRaises(FileExistsError):
                    assets.install(destination)
                (destination / "fonts/test").write_bytes(b"corrupt")
                with self.assertRaises(ValueError):
                    assets.install(destination, verify=True)
                (destination / "fonts/test").unlink()
                with self.assertRaises(FileNotFoundError):
                    assets.install(destination, verify=True)
            with (
                patch.object(assets, "MANIFEST", manifest),
                patch.object(
                    assets, "read_blobs", side_effect=ValueError("Missing committed asset blob: src/fonts/test")
                ),
            ):
                with self.assertRaises(ValueError):
                    assets.install(root / "failed")
                self.assertFalse((root / "failed").exists())
                self.assertFalse(list(root.glob(".engrave-assets-*")))

    def test_path_validation(self):
        for name in ("", "../escape", "/absolute", "C:/drive", "x\\y", "a/../../b"):
            with self.subTest(name=name), self.assertRaises(ValueError):
                assets.relative(name)


if __name__ == "__main__":
    unittest.main()
