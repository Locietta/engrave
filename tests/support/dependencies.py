"""Check the pinned normalization/validation environment before capturing outputs."""

from importlib.metadata import PackageNotFoundError, version
from pathlib import Path


def validate_dependencies():
    requirements = Path(__file__).resolve().parents[1] / "requirements.txt"
    issues = []
    for line in requirements.read_text(encoding="utf8").splitlines():
        if not line or line.startswith("#"):
            continue
        name, expected = line.split("==")
        try:
            actual = version(name)
        except PackageNotFoundError:
            actual = "missing"
        if actual != expected:
            issues.append(f"{name}: installed {actual}, required {expected}")
    if issues:
        raise RuntimeError(
            "Pinned gate dependencies differ: "
            + "; ".join(issues)
            + ". Install with python -m pip install -r tests/requirements.txt"
        )
