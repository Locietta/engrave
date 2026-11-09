"""Negative controls for the mixed-score witness; run this script also with python -O."""

import struct
import tempfile
import unittest
from pathlib import Path
from unittest.mock import patch

from engrave_testing.playback_harness import mixed_score_witness


def wav(rate, value):
    data = struct.pack("<f", value)
    return (
        b"RIFF"
        + struct.pack("<I", 36 + len(data))
        + b"WAVEfmt "
        + struct.pack("<IHHIIHH", 16, 3, 1, rate, rate * 4, 4, 32)
        + b"data"
        + struct.pack("<I", len(data))
        + data
    )


class MixedWitnessControls(unittest.TestCase):
    def test_positive_and_negative_controls(self):
        for fault in (None, "A.txt", "C.txt", "audio.wav", "B.txt", "rate", "identical"):
            with self.subTest(fault=fault), tempfile.TemporaryDirectory() as tmp:
                output_root = Path(tmp) / "witness"

                def fake_probe(exe, assets, font, score, output, options):
                    sequence = [(score, output)]
                    sequence += [
                        (Path(options[i + 1]), Path(options[i + 2])) for i, arg in enumerate(options) if arg == "--next"
                    ]
                    requested = int(options[options.index("--rate") + 1])
                    self.assertEqual(requested, 48000)
                    for path, target in sequence:
                        target.mkdir(parents=True)
                        index = 0 if "mixed_score_a" in path.name else 1
                        value = index if fault != "identical" else 0
                        rate = 44100 if fault == "rate" else requested
                        for dump in ("A.txt", "C.txt", "B.txt", "B_legacy.txt"):
                            content = f"score-{index}".encode()
                            if not target.name.startswith("fresh") and dump == fault:
                                content += b"corrupted"
                            (target / dump).write_bytes(content)
                        if not target.name.startswith("fresh") and fault == "audio.wav":
                            value += 10
                        (target / "audio.wav").write_bytes(wav(rate, value))

                with patch("engrave_testing.playback_harness.run_probe", fake_probe):
                    if fault:
                        with self.assertRaises(ValueError):
                            mixed_score_witness(Path("unused"), Path("."), Path("."), output_root, 48000)
                    else:
                        mixed_score_witness(Path("unused"), Path("."), Path("."), output_root, 48000)
                self.assertEqual((output_root / "result.json").exists(), fault is None)


if __name__ == "__main__":
    unittest.main()
