"""Unit tests for oracle harness improvements (oracle review findings).

Tests:
  1. Semantic checker mutations:
     - Removes all spanners of a type -> detected
     - Shifts a spanner endpoint -> detected
     - Drops a note tie -> detected
     - Changes a note pitch -> detected
  2. Spectral centroid silence rule (exact PCM evidence, no rounded-RMS shortcut):
     - Windows with zero nonzero samples on both sides -> centroid ignored
     - Rounded-zero RMS with nonzero samples on either side -> centroid compared
     - Quiet tone (e.g. RMS 0.00010) within / beyond 2.0 Hz -> passed / detected
     - Asymmetric silence vs quiet tone -> detected/failed
     - Baseline without the evidence field -> rejected, not compared weakly
  3. EID token normalization:
     - All 9 EID-valued XML tags covered
     - Identity renaming preservation
     - Deliberate target modification detected
  4. Rendered-page SVG geometry/visibility and PNG ink controls (test_rendered_page.py).
"""

from __future__ import annotations

import contextlib
import copy
import io
import json
import tempfile
import unittest
import zipfile
from pathlib import Path
from types import SimpleNamespace
from unittest import mock

from engrave_testing import check_semantic_dump as csd
from engrave_testing import golden as golden
from engrave_testing.mscx import PAGE_STYLE_DEFAULTS, compare_mscz_xml, parse_mscx
from engrave_testing.paths import REPO_ROOT


class TestRoundTripXmlControls(unittest.TestCase):
    def test_only_the_exact_default_page_block_is_normalized(self):
        defaults = "".join(f"<{name}>{value}</{name}>" for name, value in PAGE_STYLE_DEFAULTS)
        before = '<museScore><Score><Style><spatium>1.75</spatium></Style><Note pitch="60"/></Score></museScore>'
        after = before.replace("<spatium>", defaults + "<spatium>")
        self.assertEqual(compare_mscz_xml(before, after), [])
        self.assertTrue(compare_mscz_xml(before, after.replace("8.26772", "8.26773")))
        self.assertTrue(compare_mscz_xml(before, after.replace('pitch="60"', 'pitch="61"')))
        self.assertTrue(compare_mscz_xml(before, after.replace("</Score>", "<unknown>1</unknown></Score>")))
        self.assertTrue(compare_mscz_xml(before, after.replace("</pageWidth>", "</pageWidth>content")))
        explicit = before.replace("<spatium>", "<pageWidth>9</pageWidth><spatium>")
        self.assertTrue(compare_mscz_xml(explicit, after))

    def test_excerpt_name_loss_remains_visible(self):
        before = "<museScore><Score><name>Flute</name></Score></museScore>"
        after = "<museScore><Score/></museScore>"
        differences = compare_mscz_xml(before, after)
        self.assertEqual(len(differences), 1)
        self.assertIn("Flute", repr(differences))

    def test_whitespace_in_score_text_is_not_discarded(self):
        before = "<museScore><Score><text> </text></Score></museScore>"
        after = "<museScore><Score><text/></Score></museScore>"
        self.assertTrue(compare_mscz_xml(before, after))
        self.assertTrue(compare_mscz_xml(before.replace("> </text>", ">\n</text>"), after))
        mixed = "<museScore><Score><text> <font/></text></Score></museScore>"
        self.assertTrue(compare_mscz_xml(mixed, mixed.replace("> <font", "><font")))
        self.assertTrue(compare_mscz_xml(mixed.replace("> <font", ">\n<font"), mixed.replace("> <font", "><font")))


class TestGoldenInventory(unittest.TestCase):
    def test_previous_report_differences_cannot_disappear(self):
        with tempfile.TemporaryDirectory() as temporary:
            output = Path(temporary)
            report = {
                "fixtures": {
                    "score": {
                        "page-0.svg": {
                            "status": "different",
                            "actualSha256": "a" * 64,
                        }
                    }
                }
            }
            golden.write_json(output / "report.json", report)
            stdout = io.StringIO()
            with contextlib.redirect_stdout(stdout):
                self.assertEqual(golden.command_pin(SimpleNamespace(output=output, platform="windows")), 1)
            entries = json.loads(stdout.getvalue())
            self.assertEqual(len(entries), 1)
            self.assertEqual(entries[0]["fixture"], "score")
            self.assertEqual(entries[0]["sha256"], {"windows": "a" * 64})
            golden.write_json(output / "report.json", {"unknown": {}})
            with self.assertRaisesRegex(SystemExit, "expected fixtures"):
                golden.command_pin(SimpleNamespace(output=output, platform="windows"))


class TestCurrentGoldenInventory(unittest.TestCase):
    def test_unapproved_changes_missing_and_extra_artifacts_fail(self):
        for mutation in ("changed", "missing", "extra", "fixture", "baseline", "index"):
            with self.subTest(mutation=mutation), tempfile.TemporaryDirectory() as temporary:
                root = Path(temporary)
                baseline, actual = root / "golden", root / "actual"
                for tree in (baseline, actual):
                    golden.write_text(tree / "score" / "page-0.svg", "<svg>reviewed</svg>")
                    golden.build_index(tree)
                if mutation == "changed":
                    golden.write_text(actual / "score/page-0.svg", "<svg>changed</svg>")
                elif mutation == "missing":
                    (actual / "score/page-0.svg").unlink()
                elif mutation == "extra":
                    golden.write_text(actual / "score/extra.svg", "<svg/>")
                elif mutation == "fixture":
                    golden.write_text(actual / "extra/page-0.svg", "<svg/>")
                elif mutation == "baseline":
                    golden.write_text(baseline / "score/page-0.svg", "<svg>changed</svg>")
                    golden.write_text(actual / "score/page-0.svg", "<svg>changed</svg>")
                else:
                    golden.write_json(baseline / "index.json", {"score": {"page-0.svg": "0" * 64}})
                with contextlib.redirect_stdout(io.StringIO()):
                    self.assertEqual(golden.compare_trees(baseline, actual, [], root / "report", "windows"), 1)

    def test_allowance_accepts_only_the_reviewed_hash(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            baseline, actual = root / "golden", root / "actual"
            golden.write_text(baseline / "score/page-0.svg", "<svg>old</svg>")
            golden.build_index(baseline)
            golden.write_text(actual / "score/page-0.svg", "<svg>reviewed</svg>")
            entry = dict(
                fixture="score",
                output="page-0.svg",
                kind="different",
                step="current",
                reason="reviewed output",
                sha256={"windows": golden.sha256((actual / "score/page-0.svg").read_bytes())},
            )
            with contextlib.redirect_stdout(io.StringIO()):
                self.assertEqual(golden.compare_trees(baseline, actual, [entry], root / "report", "windows"), 0)
                golden.write_text(actual / "score/page-0.svg", "<svg>unapproved</svg>")
                self.assertEqual(golden.compare_trees(baseline, actual, [entry], root / "report", "windows"), 1)

    def test_input_and_scenario_changes_fail_comparison(self):
        self.assertEqual(golden.verify_input_provenance(), [])
        for mutation in ("fixture-whitespace", "scenario-query"):
            with self.subTest(mutation=mutation), tempfile.TemporaryDirectory() as temporary:
                root = Path(temporary)
                for tree in (root / "golden", root / "actual"):
                    golden.write_text(tree / "score/page-0.svg", "<svg/>")
                    golden.build_index(tree)
                with contextlib.ExitStack() as stack:
                    if mutation == "fixture-whitespace":
                        source = next(p for p in golden.fixture_files() if p.name == "evening.musicxml")
                        changed = root / source.name
                        changed.write_bytes(source.read_bytes() + b"\n")
                        stack.enter_context(mock.patch.object(golden, "EXTRA_FIXTURES", (changed,)))
                    else:
                        scenarios = copy.deepcopy(golden.EDIT_SCENARIOS)
                        scenario = scenarios["edit-set-pitches-coverage"]
                        # Mutate the actual query definition without changing its source input.
                        scenario["measures"] = 3
                        stack.enter_context(mock.patch.object(golden, "EDIT_SCENARIOS", scenarios))
                    with contextlib.redirect_stdout(io.StringIO()):
                        self.assertEqual(
                            golden.compare_trees(root / "golden", root / "actual", [], root / "report", "windows"), 1
                        )
                    report = json.loads((root / "report/report.json").read_text())
                    self.assertTrue(any("changed:" in issue for issue in report["indexIssues"]))

    def test_unused_and_altered_unused_allowance_fail(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            for tree in (root / "golden", root / "actual"):
                golden.write_text(tree / "score/page-0.svg", "<svg/>")
                golden.build_index(tree)
            for digest in ("a" * 64, "b" * 64):
                entry = dict(
                    fixture="score",
                    output="page-0.svg",
                    kind="different",
                    step="test",
                    reason="unused",
                    sha256={"windows": digest},
                )
                with contextlib.redirect_stdout(io.StringIO()):
                    self.assertEqual(
                        golden.compare_trees(root / "golden", root / "actual", [entry], root / "report", "windows"), 1
                    )

    def test_missing_and_wildcard_policy_rejected(self):
        with tempfile.TemporaryDirectory() as temporary:
            path = Path(temporary) / "allow.json"
            control = dict(
                fixture="score",
                output="page-0.svg",
                kind="different",
                step="test",
                reason="control",
                context="host",
                sha256={"windows": "a" * 64},
            )
            golden.write_json(path, {"entries": [control]})
            self.assertEqual(len(golden.load_allow(path)), 1)
            for mutation in ("missing", "wildcard", "hashless", "contextless"):
                entry = dict(control)
                if mutation == "missing":
                    entry["kind"] = "missing"
                    del entry["sha256"]
                elif mutation == "wildcard":
                    entry["fixture"] = "*"
                elif mutation == "hashless":
                    del entry["sha256"]
                else:
                    del entry["context"]
                golden.write_json(path, {"entries": [entry]})
                with self.subTest(mutation=mutation), self.assertRaises(ValueError):
                    golden.load_allow(path)

    def test_allowance_pinned_for_other_platform_is_not_stale(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            for tree in (root / "golden", root / "actual"):
                golden.write_text(tree / "score/page-0.svg", "<svg/>")
                golden.build_index(tree)
            entry = dict(
                fixture="score",
                output="page-0.svg",
                kind="different",
                step="test",
                reason="windows-only",
                context="host",
                sha256={"windows": "a" * 64},
            )
            with contextlib.redirect_stdout(io.StringIO()):
                self.assertEqual(
                    golden.compare_trees(root / "golden", root / "actual", [entry], root / "report", "linux"), 0
                )
                self.assertEqual(
                    golden.compare_trees(root / "golden", root / "actual", [entry], root / "report", "windows"), 1
                )

    def test_host_comparator_rejects_reviewed_archive_bytes(self):
        fixture = "slurs.mscz"
        with zipfile.ZipFile(golden.FIXTURE_DIR / fixture) as archive:
            member = next(n for n in archive.namelist() if n.endswith(".mscx") and "/" not in n)
            archive_xml = golden.normalize_mscx(archive.read(member).decode("utf-8"))
        policy = REPO_ROOT / "tests/data/golden/allow.json"
        archive_entries = golden.load_allow(policy, context="engine-gate-archive")
        entry = next(e for e in archive_entries if e["fixture"] == fixture)
        self.assertEqual(golden.sha256(archive_xml.encode("utf-8")), entry["sha256"]["windows"])
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            golden.write_text(
                root / "golden" / fixture / "score.mscx",
                golden.read_text(REPO_ROOT / "tests/data/golden/windows" / fixture / "score.mscx"),
            )
            golden.build_index(root / "golden")
            golden.write_text(root / "actual" / fixture / "score.mscx", archive_xml)
            with contextlib.redirect_stdout(io.StringIO()):
                self.assertEqual(
                    golden.compare_trees(
                        root / "golden", root / "actual", golden.load_allow(policy), root / "report", "windows"
                    ),
                    1,
                )
            report = json.loads((root / "report/report.json").read_text())
            self.assertEqual(report["counts"]["different"], 1)
            self.assertEqual(report["counts"]["allowed"], 0)

    def test_archive_form_is_not_accepted_for_host(self):
        path = REPO_ROOT / "tests/data/golden/allow.json"
        self.assertEqual(golden.load_allow(path), [])
        entries = golden.load_allow(path, context="engine-gate-archive")
        self.assertEqual(len(entries), 25)
        entry = entries[0]
        digest = entry["sha256"]["linux"]
        self.assertIsNone(
            golden.allow_accept(
                golden.load_allow(path), entry["fixture"], entry["output"], "different", digest, "linux"
            )[1]
        )
        self.assertIsNotNone(
            golden.allow_accept(entries, entry["fixture"], entry["output"], "different", digest, "linux")[1]
        )


class TestSemanticCheckerMutations(unittest.TestCase):
    def setUp(self):
        self.fixtures_dir = REPO_ROOT / "tests/data/golden/windows"
        self.hairpins_fixture = self.fixtures_dir / "hairpins_and_repeats.mscx"
        self.linked_fixture = self.fixtures_dir / "linked_parts.mscx"

        self.hairpins_dump = json.loads((self.hairpins_fixture / "semantic.json").read_text(encoding="utf-8"))
        self.hairpins_parsed = parse_mscx((self.hairpins_fixture / "score.mscx").read_text(encoding="utf-8"))

        self.linked_dump = json.loads((self.linked_fixture / "semantic.json").read_text(encoding="utf-8"))
        self.linked_parsed = parse_mscx((self.linked_fixture / "score.mscx").read_text(encoding="utf-8"))

    def test_baseline_matches(self):
        results = []
        for chk in csd.CHECKS:
            results.extend(chk(self.hairpins_dump, self.hairpins_parsed, limit=3))
        self.assertTrue(all(r.matched for r in results), "Base hairpins fixture should match")

    def test_mutation_remove_all_spanners_of_type(self):
        """Verify that check_spanners fails when all spanners of a supported type vanish."""
        mutated_dump = copy.deepcopy(self.hairpins_dump)
        # Verify fixture has HairPin spanners initially
        hp_count = sum(1 for s in mutated_dump.get("spanners", []) if s["type"] == "HairPin")
        self.assertGreater(hp_count, 0, "Fixture must contain HairPin spanners")

        # Mutate: remove all HairPins from dump
        mutated_dump["spanners"] = [s for s in mutated_dump["spanners"] if s["type"] != "HairPin"]

        results = csd.check_spanners(mutated_dump, self.hairpins_parsed, limit=3)
        failed = [r for r in results if not r.matched]
        self.assertTrue(len(failed) > 0, "Removing all HairPin spanners must be detected")
        # Ensure details mention the missing HairPin spanner
        all_details = " ".join(d for r in failed for d in r.details)
        self.assertIn("HairPin", all_details)

    def test_mutation_shift_spanner_endpoint(self):
        """Verify that shifting a spanner tick2 endpoint is detected."""
        mutated_dump = copy.deepcopy(self.hairpins_dump)
        shifted = False
        for s in mutated_dump["spanners"]:
            if s["type"] == "HairPin":
                s["tick2"] += 480
                shifted = True
                break
        self.assertTrue(shifted, "Must have shifted a spanner endpoint")

        results = csd.check_spanners(mutated_dump, self.hairpins_parsed, limit=3)
        failed = [r for r in results if not r.matched]
        self.assertTrue(len(failed) > 0, "Shifting a spanner endpoint must be detected")

    def test_mutation_drop_tie(self):
        """Verify that dropping a tie on a note is detected by check_events."""
        mutated_dump = copy.deepcopy(self.linked_dump)
        dropped_tie = False
        for staff in mutated_dump["events"]:
            for voice in staff:
                for event in voice:
                    for note in event.get("notes", []):
                        if "tieTo" in note:
                            del note["tieTo"]
                            dropped_tie = True
                            break
                    if dropped_tie:
                        break
                if dropped_tie:
                    break
        self.assertTrue(dropped_tie, "Must have found a note tie in linked_parts")

        results = csd.check_events(mutated_dump, self.linked_parsed, limit=3)
        failed = [r for r in results if not r.matched]
        self.assertTrue(len(failed) > 0, "Dropping a note tie must be detected")

    def test_mutation_change_pitch(self):
        """Verify that changing a note pitch is detected by check_events."""
        mutated_dump = copy.deepcopy(self.hairpins_dump)
        changed_pitch = False
        for staff in mutated_dump["events"]:
            for voice in staff:
                for event in voice:
                    notes = event.get("notes", [])
                    if notes:
                        notes[0]["pitch"] += 2
                        changed_pitch = True
                        break
                if changed_pitch:
                    break
        self.assertTrue(changed_pitch, "Must have changed a note pitch")

        results = csd.check_events(mutated_dump, self.hairpins_parsed, limit=3)
        failed = [r for r in results if not r.matched]
        self.assertTrue(len(failed) > 0, "Changing note pitch must be detected")


class TestCentroidSilenceEvidence(unittest.TestCase):
    def audio(self, centroid, nonzero, rms=None):
        """A minimal audio.json with exact PCM silence evidence for one window."""
        return {
            "sampleRate": 44100,
            "channels": 2,
            "frames": 22050,
            "peak": [0.0, 0.0],
            "rms": [0.0, 0.0],
            "rmsEnvelope": [rms if rms is not None else [0.0, 0.0]],
            "centroidEnvelope": [centroid],
            "nonzeroSamples": [nonzero],
            "spectralCentroidHz": 0.0,
        }

    def test_exactly_silent_windows_ignore_centroid(self):
        """Both windows have no nonzero PCM sample, so their centroids are meaningless."""
        golden_audio = self.audio(0.0, [0, 0])
        actual_audio = self.audio(1500.0, [0, 0])
        self.assertTrue(golden.audio_equal(golden_audio, actual_audio))

    def test_rounded_zero_rms_with_samples_still_compared(self):
        """A rounded-zero RMS is not silence: one or two nonzero samples keep the 2 Hz rule.

        This is the silence regression case: a single 1-LSB sample stores RMS 0.0 in the 5-decimal
        envelope but the centroid moved from 11024.0 to 8011.4 Hz.
        """
        golden_audio = self.audio(11024.0, [1, 1])
        actual_audio = self.audio(8011.4, [2, 2])
        self.assertFalse(golden.audio_equal(golden_audio, actual_audio))

    def test_quiet_tone_within_tolerance_passes(self):
        """Quiet nonzero signals compare centroids against the 2.0 Hz tolerance."""
        golden_audio = self.audio(440.0, [22050, 22050], rms=[0.00010, 0.00010])
        actual_audio = self.audio(441.5, [22050, 22050], rms=[0.00010, 0.00010])
        self.assertTrue(golden.audio_equal(golden_audio, actual_audio))

    def test_quiet_tone_diverging_centroid_fails(self):
        """Quiet nonzero signals (e.g. RMS 0.00010) must still fail a > 2.0 Hz centroid move."""
        golden_audio = self.audio(100.0, [22050, 22050], rms=[0.00010, 0.00010])
        actual_audio = self.audio(10000.0, [22050, 22050], rms=[0.00010, 0.00010])
        self.assertFalse(golden.audio_equal(golden_audio, actual_audio))

    def test_asymmetric_silence_fails(self):
        """One all-zero window against a window with samples must not be skipped."""
        golden_audio = self.audio(0.0, [0, 0])
        actual_audio = self.audio(440.0, [2, 2], rms=[0.0002, 0.0002])
        self.assertFalse(golden.audio_equal(golden_audio, actual_audio))

    def test_missing_silence_evidence_is_rejected(self):
        """A baseline captured before the evidence field fails clearly instead of skipping."""
        golden_audio = self.audio(0.0, [0, 0])
        legacy = {key: value for key, value in golden_audio.items() if key != golden.SILENCE_EVIDENCE}
        with tempfile.TemporaryDirectory(prefix="golden-audio-") as temporary:
            directory = Path(temporary)
            (directory / "golden").write_text(json.dumps(legacy), encoding="utf-8")
            (directory / "actual").write_text(json.dumps(golden_audio), encoding="utf-8")
            with self.assertRaises(golden.GoldenFormatError) as raised:
                golden.files_equal(directory / "golden", directory / "actual", "audio.json")
        message = str(raised.exception)
        self.assertIn(golden.SILENCE_EVIDENCE, message)
        self.assertIn("recapture", message)

    def test_silence_evidence_length_must_match_envelope(self):
        """Evidence for a different window count cannot gate the centroid comparison."""
        golden_audio = self.audio(0.0, [0, 0])
        actual_audio = self.audio(0.0, [0, 0])
        actual_audio["nonzeroSamples"] = []
        self.assertFalse(golden.audio_equal(golden_audio, actual_audio))


class TestEidTokenNormalization(unittest.TestCase):
    def test_all_eid_tags_normalized_and_identity_preserved(self):
        """Verify all 9 EID tags (including PageLocks and SharedPart) map consistently."""
        mscx_input = """<museScore version="5.00">
  <Score>
    <eid>98765432101</eid>
    <Staff id="1">
      <Measure>
        <eid>10001</eid>
        <voice>
          <Chord>
            <eid>20001</eid>
            <Note>
              <eid>30001</eid>
            </Note>
          </Chord>
        </voice>
      </Measure>
      <Measure>
        <eid>10002</eid>
        <mmRestLast>10001</mmRestLast>
      </Measure>
    </Staff>
    <PageLocks>
      <pageLock>
        <startMeasure>10001</startMeasure>
        <endMeasure>10002</endMeasure>
      </pageLock>
    </PageLocks>
    <SystemLocks>
      <systemLock>
        <startMeasure>10001</startMeasure>
        <endMeasure>10002</endMeasure>
      </systemLock>
    </SystemLocks>
    <Part>
      <eid>40001</eid>
      <sharedPart>50001</sharedPart>
    </Part>
    <SharedPart>
      <eid>50001</eid>
    </SharedPart>
    <Spanner type="Slur">
      <eid>60001</eid>
      <startElement>30001</startElement>
      <endElement>30001</endElement>
    </Spanner>
  </Score>
</museScore>"""
        normalized = golden.normalize_mscx(mscx_input)

        # Confirm all 9 tags are normalized to eN tokens
        self.assertIn("<eid>e1</eid>", normalized)
        self.assertIn("<eid>e2</eid>", normalized)
        # mmRestLast refers to 10001 (which was e2)
        self.assertIn("<mmRestLast>e2</mmRestLast>", normalized)
        # startMeasure and endMeasure refer to 10001 (e2) and 10002 (e5)
        self.assertIn("<startMeasure>e2</startMeasure>", normalized)
        self.assertIn("<endMeasure>e5</endMeasure>", normalized)
        # sharedPart in Part refers to 50001
        self.assertIn("<sharedPart>e7</sharedPart>", normalized)
        self.assertIn("<SharedPart>\n      <eid>e7</eid>", normalized)
        # startElement and endElement refer to Note 30001 (e4)
        self.assertIn("<startElement>e4</startElement>", normalized)
        self.assertIn("<endElement>e4</endElement>", normalized)

    def test_deliberate_target_change_detected(self):
        """Verify that changing the lock target changes the normalized representation."""
        score_a = """<Score>
  <Measure><eid>101</eid></Measure>
  <Measure><eid>102</eid></Measure>
  <PageLocks><pageLock><startMeasure>101</startMeasure><endMeasure>102</endMeasure></pageLock></PageLocks>
</Score>"""
        score_b = """<Score>
  <Measure><eid>101</eid></Measure>
  <Measure><eid>102</eid></Measure>
  <PageLocks><pageLock><startMeasure>102</startMeasure><endMeasure>102</endMeasure></pageLock></PageLocks>
</Score>"""
        norm_a = golden.normalize_mscx(score_a)
        norm_b = golden.normalize_mscx(score_b)
        self.assertIn("<startMeasure>e1</startMeasure>", norm_a)
        self.assertIn("<startMeasure>e2</startMeasure>", norm_b)
        self.assertNotEqual(
            norm_a,
            norm_b,
            "Scores targeting different measures must produce different normalized tokens",
        )


if __name__ == "__main__":
    unittest.main()
