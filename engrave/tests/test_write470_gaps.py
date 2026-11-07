"""Focused must-fix captures; optionally check them with the preserved oracle."""

import argparse
import hashlib
import json
import subprocess
from pathlib import Path


def main():
    ap = argparse.ArgumentParser()
    for name in ("probe", "assets", "out"):
        ap.add_argument("--" + name, type=Path, required=True)
    ap.add_argument("--oracle", type=Path)
    args = ap.parse_args()
    args.out.mkdir(parents=True, exist_ok=False)
    data = Path(__file__).parent / "data/write470"
    no_id = args.out / "no-tie-id.mscx"
    no_id.write_text((data / "ties_mismatched.mscx").read_text().replace("<eid>H_H</eid>", ""))
    rows = []
    for case, source in [
        ("hidden", data / "ties_hidden.mscx"),
        ("no-id", no_id),
        ("four-flutes", data / "four_flutes.mscx"),
        ("shared-parts", data / "shared_parts.source.mscx"),
    ]:
        for fmt in ("mscx", "mscz"):
            output = args.out / (case + "." + fmt)
            result = subprocess.run(
                [str(args.probe), str(args.assets), str(source), str(output), fmt, "4.70"], capture_output=True
            )
            output.with_suffix("." + fmt + ".stderr").write_bytes(result.stderr)
            warnings = json.loads(result.stdout)
            output.with_suffix("." + fmt + ".warnings.json").write_text(json.dumps(warnings, indent=2))
            rejected = case in ("hidden", "no-id")
            assert result.returncode == (2 if rejected else 0), result.stderr
            assert output.exists() != rejected
            if rejected:
                assert len(warnings) == 1
                assert warnings[0]["code"] == "B470_INVALID_TIE"
                assert warnings[0]["object"] == (
                    {"kind": "tie", "id": "H_H"} if case == "hidden" else {"kind": "note", "id": "G_G"}
                )
            elif case == "four-flutes":
                assert len(warnings) == 4
                assert {w["object"]["id"] for w in warnings} == {"P_P", "Q_Q", "R_R", "S_S"}
                assert all(w["code"] == "B470_INSTRUMENT_GROUP_NAME_DROPPED" for w in warnings)
            else:
                assert len(warnings) == 2
                assert {w["object"]["id"] for w in warnings} == {"P_P", "R_R"}
            rows.append(
                dict(
                    case=case,
                    format=fmt,
                    exit=result.returncode,
                    no_output=rejected,
                    source_sha256=hashlib.sha256(source.read_bytes()).hexdigest(),
                    warnings=warnings,
                )
            )
    (args.out / "evidence.json").write_text(json.dumps(rows, indent=2))
    if args.oracle:
        oracle = args.oracle.resolve()
        checks = (oracle / "SHA256SUMS").read_text().splitlines()
        assert len(checks) == 87
        for line in checks:
            digest, filename = line.split(maxsplit=1)
            assert hashlib.sha256((oracle / filename.lstrip("*")).read_bytes()).hexdigest() == digest
        env = {"PATH": "/usr/bin:/bin", "LD_LIBRARY_PATH": str(oracle / "lib")}
        # Refused inputs have no writer artifact. Observe the hidden inconsistent
        # tie with the old reader to prove the fixture exercises the escape.
        paths = [data / "ties_hidden.mscx", no_id]
        paths += [args.out / (c + "." + f) for c in ("four-flutes", "shared-parts") for f in ("mscx", "mscz")]
        for path in paths:
            result = subprocess.run(
                [str(oracle / "bin/semantic_dump"), str(oracle / "assets"), str(path)], env=env, capture_output=True
            )
            (args.out / (path.name + ".oracle.json")).write_bytes(result.stdout)
            (args.out / (path.name + ".oracle.stderr")).write_bytes(result.stderr)
            assert result.returncode == 0 and not result.stderr, result.stderr
            dump = json.loads(result.stdout)
            if path.name in ("ties_hidden.mscx", "no-tie-id.mscx"):
                assert dump["events"][0][0][0]["notes"][0]["tieTo"] == [960, 0, 62]
            else:
                assert [part["id"] for part in dump["parts"]] == ["1", "2", "3", "4"]
                assert len(dump["events"]) == 4
                for staff in dump["events"]:
                    assert staff[0][0]["notes"][0]["pitch"] == 60
                    assert staff[0][0]["ticks"] == 1920
                if path.name.startswith("four-flutes"):
                    assert all(
                        part["show"] and part["instrumentId"] == "flute" and part["longName"] == "Flute"
                        for part in dump["parts"]
                    )
        print("Oracle reopened both refused inputs and all four accepted artifacts; SHA256SUMS 87/87; isolated lib/")
    print("Eight focused MSCX/MSCZ captures passed")


if __name__ == "__main__":
    main()
