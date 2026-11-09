#!/usr/bin/env python3
"""Fail-closed checks for the identity comparator and FLOAT32 sample hashing."""

import hashlib
import struct
import tempfile
import unittest
from pathlib import Path

from engrave_testing.playback_harness import compare, pcm_metadata, pinned_cases, write_json


PROJECTION = b"projection-v1\ntrack 1 1:0074\ngroups 0\n"


class PlaybackHarnessTests(unittest.TestCase):
    def captures(self, temp):
        left, right = Path(temp) / "left", Path(temp) / "right"
        for root, digest in ((left, "0" * 64), (right, "1" * 64)):
            (root / "case/44100").mkdir(parents=True)
            write_json(root / "complete.json", {"renders": 1, "schema": 1, "diagnostic": True, "rates": [44100]})
            write_json(root / "manifest.json", {"case/44100": {"score_sha256": "fixed"}})
            write_json(root / "provenance.json", {"executable_sha256": digest})
            for name, data in {
                "A.txt": b"legacy-mpe-v1\ntrack 1 test\n",
                "C.txt": b"synth-input-v2\ntrack 1 test\ntotal_frames 1\n",
                "D.json": b"{}\n",
            }.items():
                (root / "case/44100" / name).write_bytes(data)
        # The left (older) executable predates dump B; the right one carries both extractors.
        (left / "case/44100/B.txt").write_bytes(b"DEFERRED: step 4b\n")
        for name in ("B.txt", "B_legacy.txt"):
            (right / "case/44100" / name).write_bytes(PROJECTION)
        return left, right

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
        cases = (
            ("deferred", "B.txt", b"DEFERRED: step 4b\n", "deferred or not a projection"),
            ("missing", "B.txt", None, "dump B is missing"),
            ("missing legacy", "B_legacy.txt", None, "dump B is missing"),
            ("no track", "B.txt", b"projection-v1\n", "structurally incomplete"),
            ("corrupted flat record", "B.txt", PROJECTION.replace(b"groups 0", b"groups 1"), "flat records vs legacy"),
            ("crlf legacy", "B_legacy.txt", PROJECTION.replace(b"\n", b"\r\n"), "not a projection"),
            ("crlf body", "B.txt", PROJECTION[:14] + PROJECTION[14:].replace(b"\n", b"\r\n"), "byte difference at offset 28"),
        )
        for label, dump, data, message in cases:
            with self.subTest(label), tempfile.TemporaryDirectory() as temp:
                left, right = self.captures(temp)
                path = right / "case/44100" / dump
                if data is None:
                    path.unlink()
                else:
                    path.write_bytes(data)
                with self.assertRaisesRegex(AssertionError, message):
                    compare(left, right)

    def test_left_projection_is_checked_when_present(self):
        with tempfile.TemporaryDirectory() as temp:
            left, right = self.captures(temp)
            (left / "case/44100/B.txt").write_bytes(PROJECTION)
            with self.assertRaisesRegex(AssertionError, "dump B is missing"):
                compare(left, right)
            (left / "case/44100/B_legacy.txt").write_bytes(PROJECTION)
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
