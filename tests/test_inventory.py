"""Mutations exercise the actual gate validators and comparator."""

import contextlib
import io
import json
import runpy
import shutil
import subprocess
import sys
import tempfile
import threading
import unittest
from pathlib import Path
from unittest.mock import patch

from engrave_testing import golden as g
from engrave_testing.dependencies import validate_dependencies
from engrave_testing.inventory import golden_inventory, validate_actual, validate_allow_usage, validate_baseline
from engrave_testing.paths import REPO_ROOT
from engrave_testing.public_output import compare, describe_render, describe_render_pdf, run_capture_pipeline

TEST_ROOT = REPO_ROOT / "tests"
PLATFORM = "windows"
FIXTURE = "bend.gp"


def temporary_directory() -> tempfile.TemporaryDirectory:
    """Keep copies on the checkout volume: hashing a freshly copied baseline in
    the OS temp directory dominates the suite runtime on this platform."""
    build = REPO_ROOT / "build"
    try:
        build.mkdir(parents=True, exist_ok=True)
    except OSError:
        build = None
    return tempfile.TemporaryDirectory(dir=build)


class InventoryMutations(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        # The shared baseline is read-only: only the two golden-mutating tests
        # allocate an isolated copy.
        cls.golden = TEST_ROOT / "data/golden" / PLATFORM
        cls.inventory = validate_baseline(cls.golden)
        cls.names = cls.inventory[FIXTURE]

    def setUp(self):
        self.temp = temporary_directory()
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        self.fixture = FIXTURE
        self.actual = self.root / "actual"
        self.actual.mkdir()
        for name in self.names:
            shutil.copyfile(self.golden / self.fixture / name, self.actual / name)

    def isolated_golden(self) -> Path:
        root = self.root / "isolated"
        golden = root / "golden" / PLATFORM
        shutil.copytree(type(self).golden, golden)
        shutil.copyfile(TEST_ROOT / "data/gate-inventory.json", root / "gate-inventory.json")
        self.golden = golden
        return golden

    def test_valid(self):
        validate_actual(self.fixture, self.actual, self.names, self.inventory)

    def test_soundfont_free_inventory(self):
        inventory = golden_inventory(self.golden, no_audio=True)
        self.assertEqual(len(inventory), 25)
        self.assertEqual(sum(map(len, inventory.values())), 226)
        self.assertTrue(all("audio.json" not in outputs for outputs in inventory.values()))
        with self.assertRaisesRegex(ValueError, "extra=.*audio.json"):
            validate_actual(self.fixture, self.actual, inventory[self.fixture], inventory)
        (self.actual / "audio.json").unlink()
        validate_actual(self.fixture, self.actual, inventory[self.fixture], inventory)
        (self.actual / "midi.json").unlink()
        with self.assertRaisesRegex(ValueError, "missing=.*midi.json"):
            validate_actual(self.fixture, self.actual, inventory[self.fixture], inventory)

    def test_missing_png_and_pdf(self):
        for name in ("page-0.png.json", "score.pdf.json"):
            with self.subTest(name=name):
                data = (self.actual / name).read_bytes()
                (self.actual / name).unlink()
                with self.assertRaises(ValueError):
                    validate_actual(self.fixture, self.actual, [n for n in self.names if n != name], self.inventory)
                with self.assertRaises(ValueError):
                    compare(
                        self.golden,
                        self.fixture,
                        self.actual,
                        [n for n in self.names if n != name],
                        [],
                        PLATFORM,
                        {},
                        inventory=self.inventory,
                    )
                (self.actual / name).write_bytes(data)

    def test_extra(self):
        (self.actual / "unexpected").touch()
        with self.assertRaises(ValueError):
            validate_actual(self.fixture, self.actual, self.names, self.inventory)

    def test_wrong_index(self):
        golden = self.isolated_golden()
        index = json.loads((golden / "index.json").read_text())
        index[self.fixture]["score.mscx"] = "0" * 64
        (golden / "index.json").write_text(json.dumps(index))
        with self.assertRaises(ValueError):
            validate_baseline(golden)
        with self.assertRaises(ValueError):
            compare(golden, self.fixture, self.actual, self.names, [], PLATFORM, {})

    def test_unindexed_and_missing_baseline(self):
        golden = self.isolated_golden()
        (golden / self.fixture / "unexpected").touch()
        with self.assertRaises(ValueError):
            validate_baseline(golden)
        (golden / self.fixture / "unexpected").unlink()
        (golden / self.fixture / "score.pdf.json").unlink()
        with self.assertRaises(ValueError):
            validate_baseline(golden)

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
                        compare(
                            self.golden,
                            self.fixture,
                            self.actual,
                            self.names,
                            policy,
                            PLATFORM,
                            counts,
                            inventory=self.inventory,
                        )
                else:
                    compare(
                        self.golden,
                        self.fixture,
                        self.actual,
                        self.names,
                        policy,
                        PLATFORM,
                        counts,
                        inventory=self.inventory,
                    )
            self.assertEqual(counts["different"], 1)

    def test_comparator_extra(self):
        (self.actual / "unexpected").touch()
        with self.assertRaisesRegex(ValueError, "inventory mismatch"):
            compare(
                self.golden,
                self.fixture,
                self.actual,
                self.names,
                [],
                PLATFORM,
                {},
                inventory=self.inventory,
            )

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
            compare(
                self.golden,
                self.fixture,
                self.actual,
                self.names,
                allow,
                PLATFORM,
                counts,
                inventory=self.inventory,
            )

    def test_changed_fixture(self):
        inputs = self.root / "inputs"
        shutil.copytree(g.FIXTURE_DIR, inputs)
        with (inputs / self.fixture).open("ab") as stream:
            stream.write(b"\n")
        with patch.object(g, "FIXTURE_DIR", inputs), self.assertRaisesRegex(ValueError, "input fixture changed"):
            # No cached inventory here: the default compare path must revalidate
            # the baseline before it compares any artifact.
            compare(self.golden, self.fixture, self.actual, self.names, [], PLATFORM, {})

    def test_manifest_source_commit(self):
        manifest = self.root / "tests/data/golden/inputs.json"
        manifest.parent.mkdir(parents=True)
        data = json.loads((g.REPO_ROOT / "tests/data/golden/inputs.json").read_text())
        data["source_commit"] = "0" * 40
        manifest.write_text(json.dumps(data))
        with patch.object(g, "REPO_ROOT", self.root), self.assertRaisesRegex(ValueError, "manifest hash mismatch"):
            validate_baseline(self.golden)

    def test_full_gate_rejects_allowance_outside_inventory(self):
        allowance = [{"fixture": "unselected", "output": "unexpected", "sha256": {"windows": "0" * 64}}]
        with self.assertRaisesRegex(ValueError, "unused"):
            validate_allow_usage(allowance, set(), "windows")
        validate_allow_usage(allowance, set(), "linux")

    def test_dependency_mismatch(self):
        with patch("engrave_testing.dependencies.version", return_value="wrong"):
            with self.assertRaisesRegex(RuntimeError, "PyMuPDF: installed wrong, required 2.0"):
                validate_dependencies()

    def test_render_pdf_deferred_to_caller_thread(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            raw_split = root / "raw-split"
            raw_full = root / "raw-full"
            out_split = root / "out-split"
            out_full = root / "out-full"
            for folder in (raw_split, raw_full, out_split, out_full):
                folder.mkdir()
            for raw in (raw_split, raw_full):
                (raw / "page-0.svg").write_text("<svg/>", encoding="utf-8")
                (raw / "score.pdf").write_bytes(b"%PDF-mock")

            seen_threads = []

            def fake_describe_pdf(path: Path) -> dict:
                seen_threads.append(threading.current_thread())
                return {"pages": 1, "bytes": path.stat().st_size, "sha256": g.sha256(path.read_bytes())}

            with patch("engrave_testing.public_output.describe_pdf", side_effect=fake_describe_pdf):
                worker_threads = []

                def worker(tag: str):
                    worker_threads.append(threading.current_thread())
                    if tag == "split":
                        return describe_render(raw_split, out_split, include_pdf=False)
                    return []

                def finalize(names):
                    return [*names, *describe_render_pdf(raw_split, out_split)]

                results = run_capture_pipeline(["split", "other"], worker, finalize, max_workers=2)
                full_names = describe_render(raw_full, out_full)

            self.assertEqual(results[0], full_names)
            self.assertEqual(seen_threads, [threading.main_thread(), threading.main_thread(), threading.main_thread()])
            self.assertTrue(any(thread is not threading.main_thread() for thread in worker_threads))
            self.assertEqual(
                (out_split / "score.pdf.json").read_text(encoding="utf-8"),
                (out_full / "score.pdf.json").read_text(encoding="utf-8"),
            )

    def test_upstream_gtest_defaults_to_serial_execution(self):
        ext = ".exe" if sys.platform == "win32" else ""
        script = TEST_ROOT / "integration/test_upstream_gtest.py"

        with temporary_directory() as directory:
            root = Path(directory)
            bin_dir = root / "bin"
            assets_dir = root / "assets"
            data_dir = root / "data"
            for folder in (bin_dir, assets_dir, data_dir):
                folder.mkdir()
            for suite in ("engraving_playback_tests", "mpe_tests"):
                (bin_dir / f"{suite}{ext}").write_bytes(b"")

            def invoke(name: str, extra_args: list[str], barrier: threading.Barrier | None = None):
                out_dir = root / name
                lock = threading.Lock()
                active = 0
                max_active = 0
                seen = []

                def fake_run(cmd, **_kwargs):
                    nonlocal active, max_active
                    with lock:
                        active += 1
                        max_active = max(max_active, active)
                    try:
                        if barrier is not None:
                            barrier.wait()
                        binary_name = Path(cmd[0]).stem
                        seen.append(binary_name)
                        if binary_name == "engraving_playback_tests":
                            stdout = "[==========] Running 220 tests from 10 test suites.\n[  PASSED  ] 220 tests.\n"
                        else:
                            stdout = "[==========] Running 24 tests from 3 test suites.\n[  PASSED  ] 24 tests.\n"
                        return subprocess.CompletedProcess(cmd, 0, stdout=stdout, stderr="")
                    finally:
                        with lock:
                            active -= 1

                argv = [
                    str(script),
                    "--bin-dir",
                    str(bin_dir),
                    "--assets",
                    str(assets_dir),
                    "--engraving-data",
                    str(data_dir),
                    "--output",
                    str(out_dir),
                    *extra_args,
                ]
                with (
                    patch.object(sys, "argv", argv),
                    patch("subprocess.run", side_effect=fake_run),
                    contextlib.redirect_stdout(io.StringIO()),
                ):
                    runpy.run_path(str(script), run_name="__main__")
                summary = json.loads((out_dir / "gtest_summary.json").read_text(encoding="utf-8"))
                return max_active, seen, summary

            default_max, default_seen, default_summary = invoke("serial", [])
            self.assertEqual(default_max, 1)
            self.assertEqual(len(default_seen), 2)
            self.assertEqual(default_summary["status"], "passed")
            self.assertEqual(default_summary["jobs"], 1)
            self.assertEqual([s["name"] for s in default_summary["suites"]], ["engraving_playback_tests", "mpe_tests"])

            parallel_max, parallel_seen, parallel_summary = invoke(
                "parallel", ["--jobs", "2"], barrier=threading.Barrier(2, timeout=2.0)
            )
            self.assertEqual(parallel_max, 2)
            self.assertEqual(len(parallel_seen), 2)
            self.assertEqual(parallel_summary["jobs"], 2)


if __name__ == "__main__":
    unittest.main()
