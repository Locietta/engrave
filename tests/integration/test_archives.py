"""Check archive bounds using the native probe."""

import argparse
import json
from pathlib import Path

from engrave_testing.archives import run_probe
from engrave_testing.paths import REPO_ROOT


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--binary", type=Path, required=True, help="archive_test native probe")
    parser.add_argument("--resources", type=Path, default=REPO_ROOT / ".runtime/assets")
    parser.add_argument("--output", type=Path, default=REPO_ROOT / "artifacts/archive-bounds")
    args = parser.parse_args()
    out = args.output.resolve()
    out.mkdir(parents=True, exist_ok=True)
    report = run_probe(args.binary.resolve(), out)
    (out / "evidence.json").write_text(json.dumps(report, indent=2), encoding="utf-8")
    print(json.dumps(report))


if __name__ == "__main__":
    main()
