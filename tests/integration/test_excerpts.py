"""Run the native excerpt/metadata probe and independently inspect music exports."""

from __future__ import annotations

import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))


import argparse
import json
import subprocess
import xml.etree.ElementTree as ET
import zipfile
from pathlib import Path

from integration.test_export_options import canonical_notes, count_note_on_events, parse_midi_file


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument("--binary", type=Path, required=True)
    p.add_argument("--assets", type=Path, required=True)
    p.add_argument("--output", type=Path, required=True)
    args = p.parse_args()
    out = args.output.resolve()
    out.mkdir(parents=True, exist_ok=True)
    run = subprocess.run(
        [str(args.binary.resolve()), str(args.assets.resolve()), str(out)],
        capture_output=True,
        text=True,
        encoding="utf-8",
        timeout=60,
    )
    (out / "native.stdout.log").write_text(run.stdout, encoding="utf-8")
    (out / "native.stderr.log").write_text(run.stderr, encoding="utf-8")
    assert run.returncode == 0, run.stderr
    report = {"status": "passed", "parts": []}
    for i, pitch in enumerate((60, 52)):
        xml = ET.parse(out / f"part{i}.musicxml").getroot()
        assert len(xml.findall("part")) == 1
        assert len(xml.findall("./part/measure/note")) == 1
        notes, counts = canonical_notes(out / f"part{i}.musicxml")
        assert counts == [1]
        assert (notes, counts) == canonical_notes(out / f"reopened-part{i}.musicxml")
        midi = parse_midi_file((out / f"part{i}.mid").read_bytes())
        count, events = count_note_on_events(midi)
        assert count == 1 and events[0]["pitch"] == pitch, (i, events)
        svg = ET.parse(out / f"part{i}.svg").getroot()
        assert svg.tag.endswith("svg") and svg.findall(".//{http://www.w3.org/2000/svg}path")
        report["parts"].append({"index": i, "midi_note": pitch, "events": count, "xml_measures": counts})
    with zipfile.ZipFile(out / "with-excerpts.mscz") as archive:
        names = [name for name in archive.namelist() if name.endswith(".mscx")]
        assert len(names) == 3, names
        report["stored_scores"] = names
    metadata = json.loads((out / "metadata.json").read_text("utf-8"))
    assert len(metadata["tracks"]) == 1 and metadata["tracks"][0]["name"] == "Test preset"
    assert metadata["tracks"][0]["soundId"] == "test-resource-id"
    assert "vendor" not in metadata["tracks"][0]
    assert len(metadata["excerpts"]) == 2
    report["metadata_tracks"] = metadata["tracks"]
    (out / "evidence.json").write_text(json.dumps(report, ensure_ascii=False, indent=2), encoding="utf-8")
    print(json.dumps(report, ensure_ascii=False))


if __name__ == "__main__":
    main()
