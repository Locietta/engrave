"""Runner for upstream MuseScore unit tests built with GoogleTest.

Executes upstream test suites:
  - engraving_playback_tests: tests playback event rendering, playback model,
    playback context, and guitar bends rendering against the pinned fork engine.
  - mpe_tests: tests MPE single-note and multi-note articulation events and patterns.

Parses GoogleTest console output, summarizes pass/fail statistics, and exits
with non-zero if any test suite fails. The pinned inventory is asserted exactly
(229 engraving playback tests in 8 suites, 11 MPE tests in 2 suites); zero tests,
a changed count, an unexpected skip or a disabled test also fail the lane. With
an explicit ``--filter`` the subset size is unknown but zero tests, skips and
disabled tests still fail.
"""

from __future__ import annotations

import argparse
import json
import os
import re
import subprocess
import sys
import tempfile
import time
from pathlib import Path

from engrave_testing.paths import REPO_ROOT

# Exact upstream test inventory built against the pinned fork. The runner fails
# if a suite silently loses tests (e.g. a file stops compiling into the target)
# or gains a skip/disable, so the numbers in docs/dev/validation.md cannot drift
# from what is executed. Update this table together with the pinned engine.
EXPECTED_SUITES = {
    "engraving_playback_tests": {"total": 229, "test_suites": 8},
    "mpe_tests": {"total": 11, "test_suites": 2},
}
# Tests that may legitimately be skipped/disabled without failing the lane. The
# pinned engine currently has none; list a test name here only with a reason.
ALLOWED_SKIPS: set[str] = set()
ALLOWED_DISABLED = 0


def default_bin_dir() -> Path:
    if os.environ.get("ENGRAVE_MCP_BIN_DIR"):
        return Path(os.environ["ENGRAVE_MCP_BIN_DIR"]).resolve()
    candidates = (
        REPO_ROOT / "build" / "xmake-windows",
        REPO_ROOT / "build",
        REPO_ROOT / "build" / "windows" / "x64" / "release",
        REPO_ROOT / "build" / "windows" / "x64" / "debug",
        REPO_ROOT / "build" / "linux" / "x86_64" / "release",
        REPO_ROOT / "build" / "linux" / "x86_64" / "debug",
    )
    for c in candidates:
        if c.is_dir():
            return c.resolve()
    return (REPO_ROOT / "build" / "xmake-windows").resolve()


def default_assets_dir() -> Path:
    if os.environ.get("ENGRAVE_MCP_RESOURCES"):
        return Path(os.environ["ENGRAVE_MCP_RESOURCES"]).resolve()
    candidates = (
        REPO_ROOT / ".runtime" / "assets",
        REPO_ROOT / ".." / ".runtime" / "assets",
    )
    for c in candidates:
        if (c / "engraving" / "instruments" / "instruments.xml").is_file():
            return c.resolve()
    return (REPO_ROOT / ".runtime" / "assets").resolve()


def default_engraving_data_dir() -> Path:
    if os.environ.get("ENGRAVING_TESTS_DATA_ROOT"):
        return Path(os.environ["ENGRAVING_TESTS_DATA_ROOT"]).resolve()
    candidates = (
        REPO_ROOT / "src" / "engraving" / "tests",
        REPO_ROOT / ".." / "vendor" / "engrave" / "src" / "engraving" / "tests",
    )
    for c in candidates:
        if (c / "playback" / "playbackeventsrenderer_data").is_dir():
            return c.resolve()
    return (REPO_ROOT / "src" / "engraving" / "tests").resolve()


def parse_gtest_output(text: str) -> dict:
    """Extract test counts and failed test names from GoogleTest standard output."""
    stats = {
        "total": 0,
        "passed": 0,
        "failed": 0,
        "skipped": 0,
        "disabled": 0,
        "failed_tests": [],
        "skipped_tests": [],
        "suites": 0,
    }

    # Matches: [==========] Running X tests from Y test suites.
    m_run = re.search(r"Running\s+(\d+)\s+tests?\s+from\s+(\d+)\s+test\s+suite", text)
    if m_run:
        stats["total"] = int(m_run.group(1))
        stats["suites"] = int(m_run.group(2))

    # Matches: [  PASSED  ] X tests.
    m_pass = re.search(r"\[\s*PASSED\s*\]\s+(\d+)\s+tests?", text)
    if m_pass:
        stats["passed"] = int(m_pass.group(1))

    # Matches: [  FAILED  ] X tests, listed below:
    m_fail = re.search(r"\[\s*FAILED\s*\]\s+(\d+)\s+tests?, listed below:", text)
    if m_fail:
        stats["failed"] = int(m_fail.group(1))

    # Matches individual failure lines: [  FAILED  ] Suite.Test
    for m in re.finditer(r"\[\s*FAILED\s*\]\s+([A-Za-z0-9_]+\.[A-Za-z0-9_]+)", text):
        name = m.group(1)
        if name not in stats["failed_tests"]:
            stats["failed_tests"].append(name)

    # Matches: [  SKIPPED ] X tests, listed below:
    m_skip = re.search(r"\[\s*SKIPPED\s*\]\s+(\d+)\s+tests?, listed below:", text)
    if m_skip:
        stats["skipped"] = int(m_skip.group(1))

    # Matches individual skip lines: [  SKIPPED ] Suite.Test
    for m in re.finditer(r"\[\s*SKIPPED\s*\]\s+([A-Za-z0-9_]+\.[A-Za-z0-9_]+)", text):
        name = m.group(1)
        if name not in stats["skipped_tests"]:
            stats["skipped_tests"].append(name)

    # Matches disabled tests note if present
    m_dis = re.search(r"YOU HAVE\s+(\d+)\s+DISABLED TESTS?", text)
    if m_dis:
        stats["disabled"] = int(m_dis.group(1))

    return stats


def check_expected_counts(suite_name: str, stats: dict, filtered: bool) -> list[str]:
    """Return human-readable violations of the pinned test inventory."""
    problems: list[str] = []
    expected = EXPECTED_SUITES.get(suite_name)
    total = stats.get("total", 0)

    if total <= 0:
        problems.append(f"{suite_name}: ran 0 tests")
    if stats.get("failed", 0):
        problems.append(f"{suite_name}: {stats['failed']} failed test(s)")
    unexpected_skips = sorted(set(stats.get("skipped_tests", [])) - ALLOWED_SKIPS)
    if unexpected_skips:
        problems.append(f"{suite_name}: unexpected skipped tests {unexpected_skips}")
    if stats.get("disabled", 0) > ALLOWED_DISABLED:
        problems.append(f"{suite_name}: {stats['disabled']} disabled test(s)")

    if expected and not filtered:
        if total != expected["total"]:
            problems.append(f"{suite_name}: ran {total} tests, expected exactly {expected['total']}")
        if stats.get("suites", 0) != expected["test_suites"]:
            problems.append(
                f"{suite_name}: ran {stats.get('suites', 0)} test suites, expected exactly {expected['test_suites']}"
            )
        if stats.get("passed", 0) != expected["total"]:
            problems.append(f"{suite_name}: passed {stats.get('passed', 0)} tests, expected {expected['total']}")
        if stats.get("skipped", 0):
            problems.append(f"{suite_name}: {stats['skipped']} skipped test(s); the pinned inventory expects none")
    return problems


def run_suite(
    suite_name: str,
    binary_path: Path,
    assets_dir: Path,
    engraving_data_dir: Path,
    gtest_filter: str | None,
    output_dir: Path | None,
    timeout: int,
    verbose: bool,
) -> dict:
    if not binary_path.is_file():
        return {
            "name": suite_name,
            "status": "missing_binary",
            "binary": str(binary_path),
            "returncode": -1,
            "error": f"Binary not found: {binary_path}",
            "elapsed_seconds": 0.0,
            "stats": {},
        }

    cmd = [
        str(binary_path),
        f"--assets={assets_dir}",
        f"--engraving-data={engraving_data_dir}",
    ]
    if gtest_filter:
        cmd.append(f"--gtest_filter={gtest_filter}")

    start_time = time.perf_counter()
    with tempfile.TemporaryDirectory(prefix="gtest_") as tmp_cwd:
        try:
            proc = subprocess.run(
                cmd,
                capture_output=True,
                text=True,
                encoding="utf-8",
                errors="replace",
                timeout=timeout,
                cwd=tmp_cwd,
            )
            elapsed = time.perf_counter() - start_time
            stdout = proc.stdout
            stderr = proc.stderr
            rc = proc.returncode
        except subprocess.TimeoutExpired as exc:
            elapsed = time.perf_counter() - start_time
            stdout = exc.stdout or ""
            stderr = exc.stderr or ""
            rc = -1

    stats = parse_gtest_output(stdout)

    if output_dir:
        output_dir.mkdir(parents=True, exist_ok=True)
        (output_dir / f"{suite_name}.stdout.log").write_text(stdout, encoding="utf-8")
        (output_dir / f"{suite_name}.stderr.log").write_text(stderr, encoding="utf-8")

    problems = check_expected_counts(suite_name, stats, filtered=bool(gtest_filter))
    status = "passed" if rc == 0 and not problems else "failed"

    if verbose or status == "failed":
        if stdout:
            print(stdout)
        if stderr:
            print(stderr, file=sys.stderr)
        for problem in problems:
            print(f"  count check: {problem}", file=sys.stderr)

    return {
        "name": suite_name,
        "status": status,
        "binary": str(binary_path),
        "returncode": rc,
        "elapsed_seconds": round(elapsed, 2),
        "stats": stats,
        "expected": EXPECTED_SUITES.get(suite_name),
        "count_problems": problems,
    }


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--bin-dir",
        type=Path,
        default=default_bin_dir(),
        help="Directory containing compiled test binaries (default: build/xmake-windows)",
    )
    parser.add_argument(
        "--assets",
        type=Path,
        default=default_assets_dir(),
        help="Path to runtime assets (.runtime/assets)",
    )
    parser.add_argument(
        "--engraving-data",
        type=Path,
        default=default_engraving_data_dir(),
        help="Path to upstream engraving test data (vendor/engrave/src/engraving/tests)",
    )
    parser.add_argument(
        "--suite",
        choices=["all", "engraving_playback", "mpe"],
        default="all",
        help="Which test suite to run (default: all)",
    )
    parser.add_argument(
        "--filter",
        "--gtest_filter",
        dest="filter",
        type=str,
        default=None,
        help="GTest filter expression (e.g. Engraving_PlaybackModelTests.*)",
    )
    parser.add_argument(
        "--output",
        type=Path,
        default=None,
        help="Directory to save log files and summary JSON",
    )
    parser.add_argument(
        "--timeout",
        type=int,
        default=180,
        help="Timeout in seconds per test binary (default: 180)",
    )
    parser.add_argument(
        "--verbose",
        "-v",
        action="store_true",
        help="Print verbose test stdout/stderr",
    )
    args = parser.parse_args()

    bin_dir = args.bin_dir.resolve()
    assets_dir = args.assets.resolve()
    engraving_data_dir = args.engraving_data.resolve()
    ext = ".exe" if sys.platform == "win32" else ""

    suites_to_run = []
    if args.suite in ("all", "engraving_playback"):
        suites_to_run.append(("engraving_playback_tests", bin_dir / f"engraving_playback_tests{ext}"))
    if args.suite in ("all", "mpe"):
        suites_to_run.append(("mpe_tests", bin_dir / f"mpe_tests{ext}"))

    print("=" * 70)
    print("MuseScore Upstream Unit Tests (GTest Runner)")
    print("=" * 70)
    print(f"Binary directory:       {bin_dir}")
    print(f"Runtime assets:         {assets_dir}")
    print(f"Engraving test fixtures: {engraving_data_dir}")
    if args.filter:
        print(f"Filter:                 {args.filter}")
    print("-" * 70)

    results = []
    all_passed = True

    for suite_name, binary_path in suites_to_run:
        print(f"Running suite '{suite_name}'...")
        res = run_suite(
            suite_name=suite_name,
            binary_path=binary_path,
            assets_dir=assets_dir,
            engraving_data_dir=engraving_data_dir,
            gtest_filter=args.filter,
            output_dir=args.output,
            timeout=args.timeout,
            verbose=args.verbose,
        )
        results.append(res)

        st = res["status"]
        stats = res.get("stats", {})
        sec = res["elapsed_seconds"]
        if st == "passed":
            passed = stats.get("passed", 0)
            total = stats.get("total", passed)
            expected = res.get("expected")
            suffix = f" (exact inventory {expected['total']})" if expected else ""
            print(f"  -> PASSED: {passed}/{total} tests in {sec}s{suffix}")
        elif st == "missing_binary":
            print(f"  -> MISSING: {res['error']}")
            all_passed = False
        else:
            failed = stats.get("failed", 0)
            total = stats.get("total", 0)
            print(f"  -> FAILED: exit code {res['returncode']}, {failed} failed (out of {total}) in {sec}s")
            for fn in stats.get("failed_tests", []):
                print(f"       * {fn}")
            for problem in res.get("count_problems", []):
                print(f"       ! {problem}")
            all_passed = False

    print("=" * 70)
    summary_data = {
        "timestamp": time.time(),
        "status": "passed" if all_passed else "failed",
        "suites": results,
    }

    if args.output:
        args.output.mkdir(parents=True, exist_ok=True)
        summary_path = args.output / "gtest_summary.json"
        summary_path.write_text(json.dumps(summary_data, indent=2), encoding="utf-8")
        print(f"Summary JSON saved to {summary_path}")

    print(f"Result: {'ALL SUITES PASSED' if all_passed else 'SOME SUITES FAILED'}")
    print("=" * 70)

    if not all_passed:
        sys.exit(1)


if __name__ == "__main__":
    main()
