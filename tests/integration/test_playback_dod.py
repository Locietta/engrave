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
    p.add_argument("--trials", type=int, default=7)
    p.add_argument("--cancel-trials", type=int, default=20)
    a = p.parse_args()
    if a.command == "generate":
        generate_focused()
    elif a.command == "compare":
        compare(a.left, a.right)
    elif a.command == "measure":
        measure(a.exe, a.assets, a.font, a.output, a.trials, a.cancel_trials)
    elif a.command == "executables":
        capture(a.exe, a.assets, a.font, a.output / "left", a.match)
        capture(a.other_exe, a.assets, a.font, a.output / "right", a.match)
        compare(a.output / "left", a.output / "right")
    else:
        capture(a.exe, a.assets, a.font, a.output, a.match)


if __name__ == "__main__":
    main()
