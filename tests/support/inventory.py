"""Fail-closed indexed public-output inventory."""

import json

from support import golden as g


def validate_baseline(root):
    index = json.loads((root / "index.json").read_text(encoding="utf8"))
    issues = g.verify_golden_index(root, index) + g.verify_input_provenance()
    inventory = json.loads((root.parents[1] / "gate-inventory.json").read_text(encoding="utf8"))
    for fixture, names in inventory.items():
        if fixture not in index or not set(names) <= set(index[fixture]):
            issues.append("gate inventory differs from index: " + fixture)
    if issues:
        raise ValueError("invalid baseline: " + repr(issues))
    return inventory


def validate_actual(fixture, out, names, inventory):
    expected = set(inventory[fixture])
    present = {p.relative_to(out).as_posix() for p in out.rglob("*") if p.is_file()}
    if set(names) != expected or present != expected or len(names) != len(expected):
        raise ValueError(
            f"{fixture}: output inventory mismatch: missing={expected - present}, extra={present - expected}"
        )


def validate_allow_usage(allow, matched, platform, selected=None):
    stale = [
        i
        for i, e in enumerate(allow)
        if platform in e["sha256"]
        and (selected is None or (e["fixture"], e["output"]) in selected)
        and i not in matched
    ]
    if stale:
        raise ValueError(f"unused selected-platform allow entries: {stale}")
