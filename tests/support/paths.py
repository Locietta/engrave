"""Shared repository and test data locations."""

from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parents[2]
DATA_ROOT = Path(__file__).resolve().parents[2] / "tests/data"
INPUT_ROOT = DATA_ROOT / "inputs"
GOLDEN_ROOT = DATA_ROOT / "golden"
WINDOWS_GOLDEN_ROOT = GOLDEN_ROOT / "windows"
LINUX_GOLDEN_ROOT = GOLDEN_ROOT / "linux"
