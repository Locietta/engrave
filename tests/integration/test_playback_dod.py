#!/usr/bin/env python3
"""Step-0 playback harness. Subcommands: capture, compare, executables, measure, generate."""

import argparse
from pathlib import Path

from engrave_testing.playback_corpus import generate_focused
from engrave_testing.playback_harness import capture, compare, measure


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument("command", choices=("capture", "compare", "executables", "measure", "generate"))
    p.add_argument("--exe", type=Path)
    p.add_argument("--other-exe", type=Path)
    p.add_argument("--assets", type=Path)
    p.add_argument("--font", type=Path)
    p.add_argument("--output", type=Path)
    p.add_argument("--left", type=Path)
    p.add_argument("--right", type=Path)
    p.add_argument("--match")
    p.add_argument("--rate", type=int, choices=(44100, 48000), help="capture one sample rate")
    p.add_argument("--expect-exe-sha256", nargs=2, metavar=("OLD", "NEW"))
    p.add_argument("--phases", default="0,1,2,3,4", help="measurement phase subset")
    p.add_argument("--trials", type=int, default=7)
    p.add_argument("--cancel-trials", type=int, default=20)
    a = p.parse_args()
    if a.command == "generate":
        generate_focused()
    elif a.command == "compare":
        compare(a.left, a.right, a.expect_exe_sha256)
    elif a.command == "measure":
        measure(a.exe, a.assets, a.font, a.output, a.trials, a.cancel_trials, [int(v) for v in a.phases.split(",")])
    elif a.command == "executables":
        rates = (a.rate,) if a.rate else (44100, 48000)
        capture(a.exe, a.assets, a.font, a.output / "left", a.match, rates)
        capture(a.other_exe, a.assets, a.font, a.output / "right", a.match, rates)
        compare(a.output / "left", a.output / "right", a.expect_exe_sha256)
    else:
        capture(a.exe, a.assets, a.font, a.output, a.match, (a.rate,) if a.rate else (44100, 48000))


if __name__ == "__main__":
    main()
