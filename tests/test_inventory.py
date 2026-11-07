"""Mutations exercise the actual gate validators and comparator."""

import contextlib
import io
import json
import shutil
import tempfile
import unittest
from pathlib import Path
from unittest.mock import patch

from integration.test_public_output import compare
from support import golden as g
from support.inventory import validate_actual, validate_baseline
from support.paths import REPO_ROOT

TEST_ROOT = REPO_ROOT / "tests"


class InventoryMutations(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        self.golden = self.root / "golden/windows"
        shutil.copytree(TEST_ROOT / "data/golden/windows", self.golden)
        shutil.copyfile(TEST_ROOT / "data/gate-inventory.json", self.root / "gate-inventory.json")
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
                with self.assertRaises(ValueError):
                    compare(
                        self.golden, self.fixture, self.actual, [n for n in self.names if n != name], [], "windows", {}
                    )
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
        allow = [
            {
                "fixture": self.fixture,
                "output": path.name,
                "kind": "different",
                "sha256": {"windows": "0" * 64},
                "step": "mutation",
                "reason": "negative control",
            }
        ]
        for policy in ([], allow):
            counts = dict(identical=0, allowed=0, different=0, **{"missing-golden": 0})
            with contextlib.redirect_stdout(io.StringIO()):
                if policy:
                    with self.assertRaisesRegex(ValueError, "unused"):
                        compare(self.golden, self.fixture, self.actual, self.names, policy, "windows", counts)
                else:
                    compare(self.golden, self.fixture, self.actual, self.names, policy, "windows", counts)
            self.assertEqual(counts["different"], 1)

    def test_comparator_extra(self):
        (self.actual / "unexpected").touch()
        with self.assertRaisesRegex(ValueError, "inventory mismatch"):
            compare(self.golden, self.fixture, self.actual, self.names, [], "windows", {})

    def test_stale_selected_allowance(self):
        allow = [
            {
                "fixture": self.fixture,
                "output": "page-0.svg",
                "kind": "different",
                "sha256": {"windows": "0" * 64},
                "step": "mutation",
                "reason": "control",
            }
        ]
        counts = dict(identical=0, allowed=0, different=0, **{"missing-golden": 0})
        with contextlib.redirect_stdout(io.StringIO()), self.assertRaisesRegex(ValueError, "unused"):
            compare(self.golden, self.fixture, self.actual, self.names, allow, "windows", counts)

    def test_changed_fixture(self):
        inputs = self.root / "inputs"
        shutil.copytree(g.FIXTURE_DIR, inputs)
        with (inputs / self.fixture).open("ab") as stream:
            stream.write(b"\n")
        with patch.object(g, "FIXTURE_DIR", inputs), self.assertRaisesRegex(ValueError, "input fixture changed"):
            compare(self.golden, self.fixture, self.actual, self.names, [], "windows", {})

    def test_manifest_source_commit(self):
        manifest = self.root / "tests/data/golden/inputs.json"
        manifest.parent.mkdir(parents=True)
        data = json.loads((g.REPO_ROOT / "tests/data/golden/inputs.json").read_text())
        data["source_commit"] = "0" * 40
        manifest.write_text(json.dumps(data))
        with patch.object(g, "REPO_ROOT", self.root), self.assertRaisesRegex(ValueError, "manifest hash mismatch"):
            validate_baseline(self.golden)

    def test_full_gate_rejects_allowance_outside_inventory(self):
        from support.inventory import validate_allow_usage

        allowance = [{"fixture": "unselected", "output": "unexpected", "sha256": {"windows": "0" * 64}}]
        with self.assertRaisesRegex(ValueError, "unused"):
            validate_allow_usage(allowance, set(), "windows")
        validate_allow_usage(allowance, set(), "linux")

    def test_dependency_mismatch(self):
        from support.dependencies import validate_dependencies

        with patch("support.dependencies.version", return_value="wrong"):
            with self.assertRaisesRegex(RuntimeError, "PyMuPDF: installed wrong, required 2.0"):
                validate_dependencies()


if __name__ == "__main__":
    unittest.main()
