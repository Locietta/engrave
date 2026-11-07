"""Frozen bounded 4.70 writer evidence against the preserved oracle."""
import argparse
import hashlib
import json
import subprocess
import xml.etree.ElementTree as ET
from pathlib import Path


def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def main():
    ap = argparse.ArgumentParser()
    for name in ("probe", "assets", "oracle", "corpus", "out"):
        ap.add_argument("--" + name, type=Path, required=True)
    args = ap.parse_args()
    args.out.mkdir(parents=True, exist_ok=False)
    manifest = json.loads((args.corpus / "manifest.json").read_text())
    assert sha(args.oracle / "bin/semantic_dump") == manifest["oracle_binary_sha256"]
    checks = (args.oracle / "SHA256SUMS").read_text().splitlines()
    assert len(checks) == 87
    for line in checks:
        digest, filename = line.split(maxsplit=1)
        assert sha(args.oracle / filename.lstrip("*")) == digest, filename
    cases = {case["id"]: case for case in manifest["cases"]}
    for case in cases.values():
        for key in ("source", "expected_semantics", "warnings"):
            assert sha(args.corpus / case[key]) == case[key + "_sha256"]
    env = {"PATH": "/usr/bin:/bin", "LD_LIBRARY_PATH": str(args.oracle / "lib")}
    evidence = []
    selected = ("notes", "names", "ids", "brackets", "ties", "ties_paired_edit", "spanners", "excerpts", "shared_parts", "stave_labels", "page_locks", "group_brackets", "instrument_group_names")
    for name in selected:
        case = cases[name]
        for fmt in ("mscx", "mscz"):
            output = args.out / (name + "." + fmt)
            result = subprocess.run([str(args.probe), str(args.assets), str(args.corpus / case["source"]),
                                     str(output), fmt, "4.70"], capture_output=True)
            (args.out / (output.name + ".writer.stderr")).write_bytes(result.stderr)
            assert result.returncode == 0, result.stderr.decode()
            assert json.loads(result.stdout) == json.loads((args.corpus / case["warnings"]).read_text()), result.stdout
            oracle = subprocess.run([str(args.oracle / "bin/semantic_dump"), str(args.oracle / "assets"),
                                     str(output)], env=env, capture_output=True)
            (args.out / (output.name + ".json")).write_bytes(oracle.stdout)
            (args.out / (output.name + ".stderr")).write_bytes(oracle.stderr)
            assert oracle.returncode == 0 and not oracle.stderr, oracle.stderr
            expected = json.loads((args.corpus / case["expected_semantics"]).read_text())
            assert json.loads(oracle.stdout) == expected, name
            if fmt == "mscx":
                xml = ET.parse(output)
                assert xml.getroot().get("version") == "4.70"
                if name == "ids":
                    assert set(case["xml_checks"]["eids"]) <= {e.text for e in xml.iter("eid")}
                if name == "brackets":
                    assert any(b.get("type") == "1" and b.get("span") == "2" and b.get("col") == "0"
                               for b in xml.iter("bracket"))
            evidence.append(dict(case=name, format=fmt, output_sha256=sha(output),
                                 oracle_exit=oracle.returncode, semantics="equal"))
    assert len(evidence) == 26
    paired = args.out / "edited-paired.mscx"
    edit = subprocess.run([str(args.probe), str(args.assets), str(args.corpus / "ties.mscx"),
                           str(paired), "mscx", "4.70", "paired"], capture_output=True)
    assert edit.returncode == 0 and json.loads(edit.stdout) == [], edit.stderr
    oracle = subprocess.run([str(args.oracle / "bin/semantic_dump"), str(args.oracle / "assets"),
                             str(paired)], env=env, capture_output=True)
    assert oracle.returncode == 0 and not oracle.stderr, oracle.stderr
    assert json.loads(oracle.stdout) == json.loads((args.corpus / cases["ties_paired_edit"]["expected_semantics"]).read_text())
    (args.out / "edited-paired.json").write_bytes(oracle.stdout)
    rejected = args.out / "edited-one-end.mscx"
    edit = subprocess.run([str(args.probe), str(args.assets), str(args.corpus / "ties.mscx"),
                           str(rejected), "mscx", "4.70", "one-end"], capture_output=True)
    assert edit.returncode == 2 and not rejected.exists()
    assert json.loads(edit.stdout) == json.loads((args.corpus / cases["ties_mismatched"]["warnings"]).read_text())
    (args.out / "edited-one-end.stderr").write_bytes(edit.stderr)
    warning_evidence = []
    for name in ("ties_mismatched", "unsupported_target"):
        case = cases[name]
        target = args.out / (name + ".refused.mscx")
        result = subprocess.run([str(args.probe), str(args.assets), str(args.corpus / case["source"]),
                                 str(target), "mscx", case["target_version"]], capture_output=True)
        assert result.returncode == 2 and not target.exists(), name
        actual = json.loads(result.stdout)
        expected = json.loads((args.corpus / case["warnings"]).read_text())
        assert actual == expected, (name, actual, expected)
        (args.out / (name + ".warnings.json")).write_text(json.dumps(actual, indent=2) + "\n")
        (args.out / (name + ".refusal.stderr")).write_bytes(result.stderr)
        warning_evidence.append(dict(case=name, exact=actual == expected, no_output=True,
                                     extra_unmapped=sum(w["code"] == "B470_UNSUPPORTED_FEATURE" for w in actual)))
    (args.out / "warning-evidence.json").write_text(json.dumps(warning_evidence, indent=2) + "\n")
    (args.out / "evidence.json").write_text(json.dumps(evidence, indent=2) + "\n")
    print("26 file exports + paired edit: exact frozen oracle semantics; one-end edit refused; archive hashes verified")


if __name__ == "__main__":
    main()
