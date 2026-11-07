"""Validate exported MNX files against the MNX JSON schema (draft 2020-12).

mnxdom's C++ validator implements JSON Schema draft 7 and silently skips
2020-12 keywords, so the engine gate checks the files that
`engrave_mnx_import_test --mnx-out=DIR` writes with a full 2020-12 validator.
Negative controls prove the validator rejects what the C++ one accepts.
"""

import argparse
import copy
import json
import sys
from pathlib import Path

from jsonschema import Draft202012Validator


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument("exports", type=Path, help="directory of exported .mnx files")
    p.add_argument("schema", type=Path, help="mnx-schema.json")
    p.add_argument("--min-files", type=int, default=8)
    a = p.parse_args()

    schema = json.loads(a.schema.read_text(encoding="utf-8"))
    Draft202012Validator.check_schema(schema)
    validator = Draft202012Validator(schema)

    files = sorted(a.exports.glob("*.mnx"))
    if len(files) < a.min_files:
        print(f"expected at least {a.min_files} exported files, found {len(files)}")
        return 1
    failed = 0
    for path in files:
        errors = sorted(validator.iter_errors(json.loads(path.read_text(encoding="utf-8"))), key=str)
        for error in errors[:5]:
            print(f"{path.name}: {'/'.join(map(str, error.absolute_path))}: {error.message}")
        failed += bool(errors)
        print(f"{path.name}: {'FAIL' if errors else 'ok'}")

    # Controls: an unknown root property (draft-7 validation accepts it) and a
    # wrong type must both be rejected.
    base = json.loads(files[0].read_text(encoding="utf-8"))
    extra = copy.deepcopy(base)
    extra["engrave-unknown-property"] = True
    wrong_type = copy.deepcopy(base)
    wrong_type["mnx"]["version"] = "1"
    for name, document in (("unknown root property", extra), ("wrong version type", wrong_type)):
        rejected = not validator.is_valid(document)
        print(f"control {name}: {'rejected' if rejected else 'ACCEPTED'}")
        failed += not rejected

    print(f"{len(files)} files, {failed} failure(s)")
    return 1 if failed else 0


if __name__ == "__main__":
    sys.exit(main())
