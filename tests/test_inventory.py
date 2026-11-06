"""Mutations exercise the actual gate validators and comparator."""
import sys
from pathlib import Path
sys.path.insert(0, str(Path(__file__).resolve().parent))
import contextlib
import io
import json
import shutil
import tempfile
import unittest
from support.inventory import validate_baseline, validate_actual
from support import golden as g
from integration.test_public_output import compare

ROOT = Path(__file__).resolve().parent

class InventoryMutations(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        self.golden = self.root / "golden/windows"
        shutil.copytree(ROOT / "data/golden/windows", self.golden)
        shutil.copyfile(ROOT / "data/gate-inventory.json", self.root / "gate-inventory.json")
        self.inventory = validate_baseline(self.golden)
        self.fixture = "bend.gp"
        self.names = self.inventory[self.fixture]
        self.actual = self.root / "actual"
        self.actual.mkdir()
        for name in self.names:
            shutil.copyfile(self.golden / self.fixture / name, self.actual / name)

    def test_valid(self):
        validate_actual(self.fixture, self.actual, self.names, self.inventory)

    def test_missing_png_and_pdf(self):
        for name in ("page-0.png.json", "score.pdf.json"):
            with self.subTest(name=name):
                data = (self.actual / name).read_bytes()
                (self.actual / name).unlink()
                with self.assertRaises(ValueError):
                    validate_actual(self.fixture, self.actual, [n for n in self.names if n != name], self.inventory)
                (self.actual / name).write_bytes(data)

    def test_extra(self):
        (self.actual / "unexpected").touch()
        with self.assertRaises(ValueError):
            validate_actual(self.fixture, self.actual, self.names, self.inventory)

    def test_wrong_index(self):
        index = json.loads((self.golden / "index.json").read_text())
        index[self.fixture]["score.mscx"] = "0" * 64
        (self.golden / "index.json").write_text(json.dumps(index))
        with self.assertRaises(ValueError):
            validate_baseline(self.golden)
        with self.assertRaises(ValueError):
            compare(self.golden, self.fixture, self.actual, self.names, [], "windows", {})

    def test_unindexed_and_missing_baseline(self):
        (self.golden / self.fixture / "unexpected").touch()
        with self.assertRaises(ValueError):
            validate_baseline(self.golden)
        (self.golden / self.fixture / "unexpected").unlink()
        (self.golden / self.fixture / "score.pdf.json").unlink()
        with self.assertRaises(ValueError):
            validate_baseline(self.golden)

    def test_changed_and_hash_mismatched_allow(self):
        path = self.actual / "score.musicxml"
        path.write_text("<mutated/>")
        allow = [{"fixture": self.fixture, "output": path.name, "kind": "different", "sha256": {"windows": "0"*64}, "step": "mutation", "reason": "negative control"}]
        for policy in ([], allow):
            counts = dict(identical=0, allowed=0, different=0, **{"missing-golden": 0})
            with contextlib.redirect_stdout(io.StringIO()):
                compare(self.golden, self.fixture, self.actual, self.names, policy, "windows", counts)
            self.assertEqual(counts["different"], 1)

if __name__ == "__main__":
    unittest.main()
