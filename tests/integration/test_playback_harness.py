#!/usr/bin/env python3
"""Fail-closed checks for the identity comparator and FLOAT32 sample hashing."""

import hashlib
import struct
import tempfile
import unittest
from pathlib import Path

from engrave_testing.playback_harness import compare, pcm_metadata, write_json


class PlaybackHarnessTests(unittest.TestCase):
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
            left, right = Path(temp) / "left", Path(temp) / "right"
            for root in (left, right):
                (root / "case").mkdir(parents=True)
                write_json(root / "complete.json", {"renders": 1, "schema": 1})
                write_json(root / "manifest.json", {"case": {"score_sha256": "fixed"}})
                for name in ("A.txt", "B.txt", "C.txt", "D.json"):
                    (root / "case" / name).write_text("same\nsecond\n")
            compare(left, right)
            (right / "case/A.txt").write_text("same\nchanged\n")
            with self.assertRaisesRegex(AssertionError, "case/A.txt:2"):
                compare(left, right)
            (right / "case/A.txt").unlink()
            with self.assertRaises(FileNotFoundError):
                compare(left, right)
            write_json(right / "manifest.json", {})
            with self.assertRaisesRegex(AssertionError, "manifest differs"):
                compare(left, right)


if __name__ == "__main__":
    unittest.main()
