#!/usr/bin/env python3
"""Fail-closed checks for the identity comparator and FLOAT32 sample hashing."""

import hashlib
import struct
import tempfile
import unittest
from pathlib import Path

from engrave_testing.playback_harness import compare, pcm_metadata, pinned_cases, write_json

# A minimal capture whose dump A and dump B describe the same track, dynamics, groups and events:
# group 0 holds a text event then a note, group 500 is an empty legacy key.
DUMP_A = (
    b"legacy-mpe-v1\ntrack 1 1:0074\nsetup 1 0 1:0076 0 0 0\ndynamics 1\nlayer 0 1\npoint 0 0 f64:3fd8000000000000\n"
    b"groups 2\ngroup 0 2\nevent 2\ntext 0 0 1:0061\nevent 1\narrangement 0 0 500 450 0 0 f64:4000000000000000\n"
    b"group 500 0\n"
)
PROJECTION = (
    b"projection-v2\ntrack 1 1:0074\nsetup 1 0 1:0076 0 - -\ndynamics 1\nlayer 0 1\npoint 0 0 3fd8000000000000\n"
    b"groups 2\ngroup 0 2\ntext 0 0 1:0061\nnote 0 0 500 450 4000000000000000 6500 3750 - 0 0\narts 1 0:0:500\n"
    b"pitch 2 0:0 5000:-12\nexpression 0\ngroup 500 0\nend-projection 1\n"
)
DEFERRED_STUB = b"DEFERRED: independent flat-record projection extractors arrive in step 4b\n"
B_FILES = ("B.txt", "B_legacy.txt")


class PlaybackHarnessTests(unittest.TestCase):
    def captures(self, temp):
        left, right = Path(temp) / "left", Path(temp) / "right"
        for root, digest in ((left, "0" * 64), (right, "1" * 64)):
            (root / "case/44100").mkdir(parents=True)
            write_json(root / "complete.json", {"renders": 1, "schema": 1, "diagnostic": True, "rates": [44100]})
            write_json(root / "manifest.json", {"case/44100": {"score_sha256": "fixed"}})
            write_json(root / "provenance.json", {"executable_sha256": digest})
            for name, data in {
                "A.txt": DUMP_A,
                "C.txt": b"synth-input-v2\ntrack 1 test\ntotal_frames 1\n",
                "D.json": b"{}\n",
            }.items():
                (root / "case/44100" / name).write_bytes(data)
        # The left (older) executable predates dump B; the right one carries both extractors.
        (left / "case/44100/B.txt").write_bytes(DEFERRED_STUB)
        for name in B_FILES:
            (right / "case/44100" / name).write_bytes(PROJECTION)
        return left, right

    def assert_rejected(self, files, transform, message=""):
        """Applies transform to the named right-hand B files; the comparison must fail."""
        with tempfile.TemporaryDirectory() as temp:
            left, right = self.captures(temp)
            for name in files:
                path = right / "case/44100" / name
                path.write_bytes(transform(path.read_bytes()))
            with self.assertRaisesRegex(AssertionError, message):
                compare(left, right)

    def test_pcm_hash_excludes_container_and_keeps_float_bits(self):
        with tempfile.TemporaryDirectory() as temp:
            path = Path(temp) / "float.wav"
            samples = struct.pack("<II", 0, 0x80000000)
            fmt = struct.pack("<HHIIHH", 3, 2, 48000, 384000, 8, 32)
            body = b"fmt " + struct.pack("<I", len(fmt)) + fmt + b"data" + struct.pack("<I", 8) + samples
            path.write_bytes(b"RIFF" + struct.pack("<I", 4 + len(body)) + b"WAVE" + body)
            metadata = pcm_metadata(path)
            self.assertEqual(metadata["sha256"], hashlib.sha256(samples).hexdigest())
            self.assertEqual((metadata["rate"], metadata["channels"], metadata["frames"]), (48000, 2, 1))
            path.write_bytes(path.read_bytes()[:-1])
            with self.assertRaisesRegex(AssertionError, "truncated"):
                pcm_metadata(path)

    def test_first_difference_and_missing_evidence(self):
        with tempfile.TemporaryDirectory() as temp:
            left, right = self.captures(temp)
            compare(left, right, ("0" * 64, "1" * 64))
            a = right / "case/44100/A.txt"
            original = a.read_bytes()
            a.write_bytes(original + b"corruption")
            with self.assertRaisesRegex(AssertionError, f"byte difference at offset {len(original)}"):
                compare(left, right)
            a.unlink()
            with self.assertRaises(FileNotFoundError):
                compare(left, right)

    def test_projection_is_required_of_the_new_executable(self):
        # B compares the two extractors inside the new executable, not against
        # an older executable that may still have a stub.
        self.assert_rejected(["B.txt"], lambda _: DEFERRED_STUB, "deferred or not a projection")
        self.assert_rejected(B_FILES, lambda _: DEFERRED_STUB, "deferred or not a projection")
        for name in B_FILES:
            with self.subTest(missing=name), tempfile.TemporaryDirectory() as temp:
                left, right = self.captures(temp)
                (right / "case/44100" / name).unlink()
                with self.assertRaisesRegex(AssertionError, "dump B is missing"):
                    compare(left, right)
        self.assert_rejected(["B.txt"], lambda b: b.replace(b"6500", b"6501"), "flat records vs legacy")

    def test_truncated_projection_fails_closed(self):
        # Every cut, at a line boundary or inside a line, in either file or in both identically
        for cut in range(len(PROJECTION)):
            for files in (["B.txt"], ["B_legacy.txt"], B_FILES):
                with self.subTest(cut=cut, files=files):
                    self.assert_rejected(files, lambda b, cut=cut: b[:cut])

    def test_malformed_projection_fails_closed(self):
        cases = {
            "units length": (b"track 1 1:0074", b"track 1 1:007"),
            "non-numeric": (b"note 0 0 500", b"note 0 x 500"),
            "extra field": (b"expression 0\n", b"expression 0 1:2\n"),
            "missing field": (b"text 0 0 1:0061", b"text 0 1:0061"),
            "bits width": (b"4000000000000000", b"400000000000000"),
            "unknown record": (b"text 0 0 1:0061", b"chord 0 0 1:0061"),
            "item count": (b"arts 1 0:0:500", b"arts 2 0:0:500"),
            "item shape": (b"pitch 2 0:0 5000:-12", b"pitch 2 0:0 5000"),
            "group count": (b"groups 2", b"groups 3"),
            "group size": (b"group 0 2", b"group 0 3"),
            "points": (b"layer 0 1", b"layer 0 2"),
            "explicit point": (b"point 0 0 3fd8000000000000", b"point 0 1 3fd8000000000000"),
            "setup subs": (b"setup 1 0 1:0076 0 - -", b"setup 1 0 1:0076 1 - -"),
            "unsorted groups": (b"group 500 0", b"group 0 0"),
            "terminator count": (b"end-projection 1", b"end-projection 2"),
            "no terminator": (b"end-projection 1\n", b""),
            "data after end": (b"end-projection 1\n", b"end-projection 1\ntrack 2 0:\n"),
            "crlf": (b"\n", b"\r\n"),
            "blank line": (b"groups 2\n", b"groups 2\n\n"),
        }
        for label, (old, new) in cases.items():
            self.assertIn(old, PROJECTION, label)
            for files in (["B.txt"], ["B_legacy.txt"], B_FILES):
                with self.subTest(label, files=files):
                    self.assert_rejected(files, lambda b, old=old, new=new: b.replace(old, new))

    def test_projection_must_match_dump_a_inventory(self):
        # Well-formed and identical in both files, but not what dump A recorded
        cases = {
            "lost empty group": (b"groups 2", b"groups 1", b"group 500 0\n", b""),
            "event order": (
                b"group 0 2\ntext 0 0 1:0061\n",
                b"group 0 2\n",
                b"expression 0\n",
                b"expression 0\ntext 0 0 1:0061\n",
            ),
            "group key": (b"group 500 0", b"group 501 0", b"", b""),
            "dynamics": (b"layer 0 1\npoint 0 0 3fd8000000000000\n", b"layer 0 0\n", b"", b""),
            "track": (b"track 1 1:0074", b"track 2 1:0074", b"", b""),
        }
        for label, (a, b, c, d) in cases.items():
            with self.subTest(label):
                self.assert_rejected(
                    B_FILES, lambda p, a=a, b=b, c=c, d=d: p.replace(a, b).replace(c, d), "differs from dump A"
                )

    def test_left_projection_is_a_known_stub_or_complete(self):
        with tempfile.TemporaryDirectory() as temp:
            left, right = self.captures(temp)
            compare(left, right)
            stub = left / "case/44100/B.txt"
            for bad in (b"DEFERRED: step 4b\n", b"", DEFERRED_STUB[:-1], PROJECTION[:40]):
                stub.write_bytes(bad)
                with self.assertRaisesRegex(AssertionError, "dump B"):
                    compare(left, right)
            stub.unlink()
            with self.assertRaisesRegex(AssertionError, "dump B is missing"):
                compare(left, right)
            # A real left projection needs its legacy pair and is checked in full
            stub.write_bytes(PROJECTION)
            with self.assertRaisesRegex(AssertionError, "dump B is missing"):
                compare(left, right)
            (left / "case/44100/B_legacy.txt").write_bytes(PROJECTION)
            compare(left, right)
            stub.write_bytes(PROJECTION.replace(b"6500", b"6501"))  # B_legacy intact
            with self.assertRaisesRegex(AssertionError, "flat records vs legacy"):
                compare(left, right)

    def test_newline_bytes_are_not_normalized(self):
        for dump in ("A.txt", "C.txt", "D.json"):
            with self.subTest(dump=dump), tempfile.TemporaryDirectory() as temp:
                left, right = self.captures(temp)
                path = right / "case/44100" / dump
                original = path.read_bytes()
                path.write_bytes(original.replace(b"\n", b"\r\n"))
                offset = original.index(b"\n")
                with self.assertRaisesRegex(AssertionError, f"byte difference at offset {offset}"):
                    compare(left, right)

    def test_equally_incomplete_records_fail(self):
        for dump, data in (
            ("A.txt", b"legacy-mpe-v1\n"),
            ("C.txt", b"synth-input-v2\ntotal_frames 1\n"),
            ("C.txt", b"synth-input-v2\ntrack 1 test\n"),
        ):
            with self.subTest(dump=dump, data=data), tempfile.TemporaryDirectory() as temp:
                left, right = self.captures(temp)
                for root in (left, right):
                    (root / "case/44100" / dump).write_bytes(data)
                with self.assertRaisesRegex(AssertionError, "structurally incomplete"):
                    compare(left, right)

    def test_full_run_cannot_omit_a_common_case(self):
        with tempfile.TemporaryDirectory() as temp:
            left, right = self.captures(temp)
            manifest = {f"{name}/44100": {} for name in pinned_cases()}
            del manifest[next(iter(manifest))]
            for root in (left, right):
                write_json(root / "manifest.json", manifest)
                write_json(root / "complete.json", {"schema": 1, "renders": len(manifest), "rates": [44100]})
            with self.assertRaisesRegex(AssertionError, "pinned corpus"):
                compare(left, right)

    def test_provenance_is_required_and_expected_hashes_are_independent(self):
        with tempfile.TemporaryDirectory() as temp:
            left, right = self.captures(temp)
            compare(left, right, ("0" * 64, "1" * 64))
            write_json(right / "provenance.json", {"executable_sha256": "f" * 64})
            with self.assertRaisesRegex(AssertionError, "executable SHA-256.*expected"):
                compare(left, right, ("0" * 64, "1" * 64))
            (right / "provenance.json").unlink()
            with self.assertRaises(FileNotFoundError):
                compare(left, right)


if __name__ == "__main__":
    unittest.main()
