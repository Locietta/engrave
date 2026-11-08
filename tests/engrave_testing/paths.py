"""Shared repository and test data locations."""

from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parents[2]
DATA_ROOT = REPO_ROOT / "tests/data"
INPUT_ROOT = DATA_ROOT / "inputs"
GOLDEN_ROOT = DATA_ROOT / "golden"
WINDOWS_GOLDEN_ROOT = GOLDEN_ROOT / "windows"
LINUX_GOLDEN_ROOT = GOLDEN_ROOT / "linux"


def pixi_env(root: Path) -> Path:
    """Return the default Pixi environment for the owning checkout."""
    return root / ".pixi/envs/default"
