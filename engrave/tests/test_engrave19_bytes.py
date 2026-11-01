"""Check default and explicit 5.00 exports against frozen engrave-19 captures.

The ZIP writer records the wall-clock time. The same-revision raw archive check
zeroes only its DOS timestamps; later revisions also change programRevision,
so those builds compare every archive entry after normalizing that one field.
"""
from __future__ import annotations

import argparse
import hashlib
import os
from pathlib import Path
import re
import struct
import subprocess
import tempfile
import zipfile

DATA = Path(__file__).parent / "data" / "write470"
SOURCE = DATA / "engrave19.mscz"
BASE_XML = DATA / "engrave19.mscx"
BASE_ARCHIVE_SHA = "dc7e9769b44dfc19c914e44bf2a94cb7bde08d48eb3080ce827219bf608c06f0"
BASE_XML_SHA = "39dbf7e736af44e9acf8cef74963901bd135e934c1a847b01ffc7517d3b01b73"
BASE_ARCHIVE_TIMELESS_SHA = "4d9fed2a99b4512cb8612b87f50e9ad09ebfad73c592b01277fb5af36003250f"
REVISION = re.compile(rb"<programRevision(?:\s*/>|>[^<]*</programRevision>)")


def digest(value: bytes) -> str:
    return hashlib.sha256(value).hexdigest()


def normalized_revision(data: bytes) -> bytes:
    result, count = REVISION.subn(b"<programRevision>45ea71c</programRevision>", data)
    if count != 1:
        raise AssertionError(f"expected one programRevision, got {count}")
    return result


def without_zip_times(path: Path) -> bytes:
    raw = bytearray(path.read_bytes())
    with zipfile.ZipFile(path) as archive:
        central = archive.start_dir
        for item in archive.infolist():
            local = item.header_offset
            assert raw[local:local + 4] == b"PK\x03\x04"
            assert raw[central:central + 4] == b"PK\x01\x02"
            raw[local + 10:local + 14] = b"\0" * 4
            raw[central + 12:central + 16] = b"\0" * 4
            name, extra, comment = struct.unpack_from("<HHH", raw, central + 28)
            central += 46 + name + extra + comment
    return bytes(raw)


def run(probe: Path, assets: Path, output: Path, fmt: str, version: str) -> None:
    command = [str(probe), str(assets), str(SOURCE), str(output), fmt, version]
    result = subprocess.run(command, capture_output=True, text=True, check=False)
    if result.returncode:
        raise AssertionError(f"{command}: exit {result.returncode}: {result.stderr}")
    if result.stdout:
        raise AssertionError(f"unexpected 5.00 warnings: {result.stdout}")


def check_archive(actual: Path) -> None:
    with zipfile.ZipFile(SOURCE) as expected, zipfile.ZipFile(actual) as got:
        expected_infos, actual_infos = expected.infolist(), got.infolist()
        assert [x.filename for x in actual_infos] == [x.filename for x in expected_infos]
        same_revision = True
        for want, have in zip(expected_infos, actual_infos):
            assert (want.compress_type, want.external_attr, want.flag_bits) == (
                have.compress_type, have.external_attr, have.flag_bits)
            original, current = expected.read(want), got.read(have)
            if want.filename == "score.mscx":
                same_revision = original == current
                current = normalized_revision(current)
            assert digest(current) == digest(original), want.filename
        if same_revision and os.name == "nt":
            assert digest(without_zip_times(actual)) == BASE_ARCHIVE_TIMELESS_SHA


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--probe", type=Path, required=True)
    parser.add_argument("--assets", type=Path, required=True)
    args = parser.parse_args()
    assert digest(SOURCE.read_bytes()) == BASE_ARCHIVE_SHA
    assert digest(BASE_XML.read_bytes()) == BASE_XML_SHA
    assert digest(without_zip_times(SOURCE)) == BASE_ARCHIVE_TIMELESS_SHA
    with tempfile.TemporaryDirectory(prefix="engrave19-bytes-") as tmp:
        for version in ("default", "5.00"):
            xml = Path(tmp) / (version + ".mscx")
            archive = Path(tmp) / (version + ".mscz")
            run(args.probe.resolve(), args.assets.resolve(), xml, "mscx", version)
            run(args.probe.resolve(), args.assets.resolve(), archive, "mscz", version)
            assert digest(normalized_revision(xml.read_bytes())) == BASE_XML_SHA
            check_archive(archive)
    print("engrave-19 default and explicit 5.00 captures match")


if __name__ == "__main__":
    main()
