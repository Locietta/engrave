"""Validate dynamic native fonts, cache invalidation and immutable retained data."""

import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
import argparse
import json
import os
import subprocess
import zlib
from pathlib import Path

from support.paths import REPO_ROOT

p = argparse.ArgumentParser(description=__doc__)
p.add_argument("--binary", type=Path, required=True)
p.add_argument("--assets", type=Path, required=True)
p.add_argument("--font", type=Path, required=True)
p.add_argument("--output", type=Path, required=True)
p.add_argument("--soundfont", type=Path)
p.add_argument(
    "--family-fallback-font",
    type=Path,
    help="Bold/Medium-only face used for the regular-family fallback case",
)
a = p.parse_args()
out = a.output.resolve()
out.mkdir(parents=True, exist_ok=True)


def fallback_fixture():
    if a.family_fallback_font:
        return a.family_fallback_font
    if os.environ.get("ENGRAVE_MCP_FAMILY_FALLBACK_FONT"):
        return Path(os.environ["ENGRAVE_MCP_FAMILY_FALLBACK_FONT"])
    root = REPO_ROOT
    candidates = (
        root / ".pixi/envs/native/fonts/Ubuntu-M.ttf",
        Path("C:/Windows/Fonts/arialbd.ttf"),
        Path("/usr/share/fonts/truetype/ubuntu/Ubuntu-M.ttf"),
        Path("/usr/share/fonts/TTF/Ubuntu-M.ttf"),
        Path("/usr/share/fonts/truetype/ubuntu/Ubuntu-B.ttf"),
        Path("/usr/share/fonts/truetype/dejavu/DejaVuSans-Bold.ttf"),
        Path("/usr/share/fonts/TTF/DejaVuSans-Bold.ttf"),
        Path("/usr/share/fonts/dejavu/DejaVuSans-Bold.ttf"),
    )
    for candidate in candidates:
        if candidate.is_file():
            return candidate
    raise SystemExit("No bold/medium-only font fixture found; pass --family-fallback-font")


def run_case(binary, assets, font, output, soundfont):
    output = output.resolve()
    output.mkdir(parents=True, exist_ok=True)
    source = font.read_bytes()
    command = [
        str(binary.resolve()),
        str(assets.resolve()),
        str(font.resolve()),
        str(output),
    ]
    sound_source = soundfont.read_bytes() if soundfont else None
    if soundfont:
        command.append(str(soundfont.resolve()))
    r = subprocess.run(command, capture_output=True, text=True, encoding="utf-8", timeout=60)
    (output / "native.stdout.log").write_text(r.stdout, encoding="utf-8")
    (output / "native.stderr.log").write_text(r.stderr, encoding="utf-8")
    assert r.returncode == 0, r.stderr
    report = json.loads((output / "evidence.json").read_text("utf-8"))
    assert report["contentHash"] == f"crc32:{zlib.crc32(source):08x}:{len(source)}"
    assert report["sourceBytes"] == len(source)
    assert font.read_bytes() == source, "Test modified the original host font"
    assert (output / "mutable-input.ttf").read_bytes() == b"invalid-font"
    # The cached path has process lifetime and is expected to be removed on exit.
    assert not Path(report["cachedPath"]).exists(), "Process-owned font cache leaked"
    if sound_source is not None:
        sound = report["soundfont"]
        assert sound["contentHash"] == f"crc32:{zlib.crc32(sound_source):08x}:{len(sound_source)}"
        assert sound["sourceBytes"] == len(sound_source)
        assert soundfont.read_bytes() == sound_source, "Test modified the original SoundFont"
        assert (output / "mutable-input.sf2").read_bytes() == b"invalid-soundfont"
        assert not Path(sound["cachedPath"]).exists(), "Process-owned SoundFont cache leaked"
    return report


report = run_case(a.binary, a.assets, a.font, out, a.soundfont)
# Separate process: the bold/medium face is the family's only registered face,
# so a regular lookup must resolve to it instead of the cached fallback.
fallback_report = run_case(a.binary, a.assets, fallback_fixture(), out / "family-fallback", None)
assert fallback_report["bold"] is True and fallback_report["boldOnlyFamily"] is True, (
    "Family fallback fixture is not a bold/medium-only face"
)
assert abs(fallback_report["after"] - fallback_report["boldAdvance"]) < 1e-4, (
    "Regular lookup of a bold-only family did not use the registered face"
)
print(json.dumps({"primary": report, "familyFallback": fallback_report}, ensure_ascii=False))
