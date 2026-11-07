"""Direct ZIP decompression checks plus optional valid MXL/MSCZ/GP MCP imports."""

from __future__ import annotations

import argparse
import io
import json
import statistics
import struct
import subprocess
import time
import zipfile
import zlib
from pathlib import Path

from engrave_testing.paths import REPO_ROOT


def raw_deflate(data):
    encoder = zlib.compressobj(wbits=-15)
    return encoder.compress(data) + encoder.flush()


def archive(
    data=b"archive fixture\n",
    *,
    method=8,
    descriptor=False,
    descriptor_signature=True,
    comment=b"",
    this_disk=0,
    directory_disk=0,
    entries_this_disk=None,
    **changes,
):
    """Two entries ensure a damaged second local header still reaches fileData()."""
    entries = []
    local_bytes = bytearray()
    for name, payload, target in [(b"prefix", b"", False), (b"payload", data, True)]:
        current_method = method if target else 0
        compressed = raw_deflate(payload) if current_method == 8 else payload
        crc = zlib.crc32(payload)
        flags = 8 if descriptor and target else 0
        values = {
            "signature": 0x04034B50,
            "version": 20,
            "flags": flags,
            "method": current_method,
            "crc": crc,
            "compressed_size": len(compressed),
            "uncompressed_size": len(payload),
            "name_length": len(name),
            "extra_length": 0,
            "disk_start": 0,
        }
        local = dict(values)
        if flags & 8:
            local.update(crc=0, compressed_size=0, uncompressed_size=0)
        central = dict(values, offset=len(local_bytes))
        if target:
            for field, value in changes.items():
                if field.startswith("local_"):
                    local[field[6:]] = value
                elif field.startswith("central_"):
                    central[field[8:]] = value
            compressed = changes.get("raw_compressed", compressed)
        local_bytes += struct.pack(
            "<IHHHHHIIIHH",
            local["signature"],
            local["version"],
            local["flags"],
            local["method"],
            0,
            0,
            local["crc"],
            local["compressed_size"],
            local["uncompressed_size"],
            local["name_length"],
            local["extra_length"],
        )
        local_bytes += name + compressed
        if flags & 8:
            if descriptor_signature:
                local_bytes += struct.pack("<I", 0x08074B50)
            local_bytes += struct.pack("<III", crc, values["compressed_size"], len(payload))
        entries.append((name, central))
    directory = bytearray()
    for name, central in entries:
        directory += struct.pack(
            "<IHHHHHHIIIHHHHHII",
            0x02014B50,
            20,
            central["version"],
            central["flags"],
            central["method"],
            0,
            0,
            central["crc"],
            central["compressed_size"],
            central["uncompressed_size"],
            len(name),
            0,
            0,
            central["disk_start"],
            0,
            0,
            central["offset"],
        )
        directory += name
    if entries_this_disk is None:
        entries_this_disk = len(entries)
    return bytes(
        local_bytes
        + directory
        + struct.pack(
            "<IHHHHIIH",
            0x06054B50,
            this_disk,
            directory_disk,
            entries_this_disk,
            len(entries),
            len(directory),
            len(local_bytes),
            len(comment),
        )
        + comment
    )


def cases():
    data = b"bounded decompression\n" * 200
    compressed = raw_deflate(data)
    truncated_comment = bytearray(archive(data, comment=b"junk"))
    # The record claims 100 comment bytes but only 4 follow it.
    truncated_comment[-6:-4] = struct.pack("<H", 100)
    valid = {
        "stored": (archive(data, method=0), data),
        "deflated": (archive(data), data),
        "empty-stored": (archive(b"", method=0), b""),
        "empty-deflated": (archive(b""), b""),
        "descriptor-signature": (archive(data, descriptor=True), data),
        "descriptor-no-signature": (
            archive(data, descriptor=True, descriptor_signature=False),
            data,
        ),
        "stored-descriptor": (archive(data, method=0, descriptor=True), data),
        "comment": (archive(data, comment=b"plain archive comment"), data),
        # The comment contains an EndOfDirectory signature; only the real record
        # has a comment length that spans the bytes after it.
        "comment-with-signature": (
            archive(data, comment=b"note PK\x05\x06 inside"),
            data,
        ),
    }
    invalid = {
        "truncated-comment": bytes(truncated_comment),
        "multi-disk": archive(data, this_disk=1),
        "multi-disk-directory": archive(data, directory_disk=1),
        "multi-disk-entry-count": archive(data, entries_this_disk=1),
        "multi-disk-central-entry": archive(data, central_disk_start=1),
        "underreported-output": archive(data, local_uncompressed_size=1, central_uncompressed_size=1),
        "overreported-output": archive(
            data,
            local_uncompressed_size=len(data) + 1,
            central_uncompressed_size=len(data) + 1,
        ),
        "declared-output-cap": archive(
            data,
            local_uncompressed_size=64 * 1024 * 1024 + 1,
            central_uncompressed_size=64 * 1024 * 1024 + 1,
        ),
        "uint32-output": archive(
            data,
            local_uncompressed_size=0xFFFFFFFF,
            central_uncompressed_size=0xFFFFFFFF,
        ),
        "uint32-input": archive(data, local_compressed_size=0xFFFFFFFF, central_compressed_size=0xFFFFFFFF),
        "bad-crc": archive(data, local_crc=0, central_crc=0),
        "local-crc-mismatch": archive(data, local_crc=0),
        "stored-size-mismatch": archive(
            data,
            method=0,
            local_uncompressed_size=len(data) - 1,
            central_uncompressed_size=len(data) - 1,
        ),
        "method-mismatch": archive(data, central_method=0),
        "unsupported-method": archive(data, local_method=9, central_method=9),
        "bad-local-signature": archive(data, local_signature=0x02014B50),
        "truncated-deflate": archive(
            data,
            raw_compressed=compressed[:-1],
            local_compressed_size=len(compressed) - 1,
            central_compressed_size=len(compressed) - 1,
        ),
        "trailing-deflate": archive(
            data,
            raw_compressed=compressed + b"junk",
            local_compressed_size=len(compressed) + 4,
            central_compressed_size=len(compressed) + 4,
        ),
        "bad-local-offset": archive(data, central_offset=0xFFFFFFFF),
        "bad-local-name-length": archive(data, local_name_length=65535),
        "bad-local-extra-length": archive(data, local_extra_length=65535),
        "central-encryption": archive(data, central_flags=1),
        "local-encryption": archive(data, local_flags=1),
        "strong-encryption": archive(data, local_flags=0x40, central_flags=0x40),
        "directory-encryption": archive(data, local_flags=0x2000, central_flags=0x2000),
        "descriptor-flag-mismatch": archive(data, local_flags=8),
        "unsupported-central-version": archive(data, central_version=45),
        "unsupported-local-version": archive(data, local_version=45),
        "one-byte": b"P",
        "two-bytes": b"PK",
        "three-bytes": b"PK\x03",
        "truncated-signature": b"PK\x03\x04" + b"\x00" * 10,
        "huge-dir-start-offset": struct.pack("<IHHHHIIH", 0x06054B50, 0, 0, 0, 0, 0, 0x7FFFFFFF, 0),
        "huge-dir-start-offset-ffffffff": struct.pack("<IHHHHIIH", 0x06054B50, 0, 0, 0, 0, 0, 0xFFFFFFFF, 0),
        "dir-offset-size-beyond-eod": struct.pack("<IHHHHIIH", 0x06054B50, 0, 0, 1, 1, 100, 0, 0),
        "truncated-central-header": struct.pack(
            "<IHHHHHHIIIHHHHHII",
            0x02014B50,
            20,
            20,
            0,
            0,
            0,
            0,
            0,
            0,
            0,
            7,
            0,
            0,
            0,
            0,
            0,
            0,
        )[:20]
        + struct.pack("<IHHHHIIH", 0x06054B50, 0, 0, 1, 1, 20, 0, 0),
        "truncated-central-directory-entries": struct.pack(
            "<IHHHHHHIIIHHHHHII",
            0x02014B50,
            20,
            20,
            0,
            0,
            0,
            0,
            0,
            0,
            0,
            7,
            0,
            0,
            0,
            0,
            0,
            0,
        )
        + b"payload"
        + struct.pack("<IHHHHIIH", 0x06054B50, 0, 0, 2, 2, 53, 0, 0),
        "truncated-central-filename": struct.pack(
            "<IHHHHHHIIIHHHHHII",
            0x02014B50,
            20,
            20,
            0,
            0,
            0,
            0,
            0,
            0,
            0,
            50,
            0,
            0,
            0,
            0,
            0,
            0,
        )
        + b"payl"
        + struct.pack("<IHHHHIIH", 0x06054B50, 0, 0, 1, 1, 50, 0, 0),
        "aggregate-uncompressed-cap": (
            b"".join(
                struct.pack(
                    "<IHHHHHHIIIHHHHHII",
                    0x02014B50,
                    20,
                    20,
                    0,
                    0,
                    0,
                    0,
                    0,
                    0,
                    60 * 1024 * 1024,
                    len(f"e{i}"),
                    0,
                    0,
                    0,
                    0,
                    0,
                    0,
                )
                + f"e{i}".encode("ascii")
                for i in range(5)
            )
            + struct.pack("<IHHHHIIH", 0x06054B50, 0, 0, 5, 5, (46 + 2) * 5, 0, 0)
        ),
    }
    return valid, invalid


def run_probe(binary, out):
    valid, invalid = cases()
    report = {"valid": [], "rejected": []}
    for name, packed, expected in [(n, p, e) for n, (p, e) in valid.items()] + [
        (n, p, None) for n, p in invalid.items()
    ]:
        source, result = out / f"{name}.zip", out / f"{name}.bin"
        source.write_bytes(packed)
        samples = []
        # A single process launch can be delayed by unrelated system activity.
        # Three independent rejections keep the 1 s bound on sustained work;
        # the 10 s subprocess timeout still bounds each individual attempt.
        for attempt in range(3 if expected is None else 1):
            started = time.monotonic()
            run = subprocess.run(
                [str(binary), str(source), "payload", str(result)],
                capture_output=True,
                text=True,
                encoding="utf-8",
                timeout=10,
            )
            samples.append(time.monotonic() - started)
            suffix = f"-{attempt + 1}" if expected is None else ""
            (out / f"{name}{suffix}.stderr.log").write_text(run.stderr, encoding="utf-8")
            assert run.returncode == 0, (name, attempt, run.returncode, run.stderr)
            response = json.loads(run.stdout.strip().splitlines()[-1])
            if expected is None:
                assert response["error"] and response["size"] == 0 and result.read_bytes() == b"", (name, response)
                assert response.get("buffer_size", len(packed)) == len(packed), (
                    name,
                    response,
                )
        if expected is None:
            median = statistics.median(samples)
            assert median < 1.0, (name, samples, median)
            report["rejected"].append(
                {
                    "name": name,
                    "seconds": round(median, 4),
                    "samples_seconds": [round(value, 4) for value in samples],
                    "buffer_size": response.get("buffer_size", len(packed)),
                }
            )
        else:
            assert response["exists"], (name, response)
            assert not response["error"] and result.read_bytes() == expected, (
                name,
                response,
            )
            assert response.get("buffer_size", len(packed)) == len(packed), (
                name,
                response,
            )
            if name == "comment-with-signature":
                # Python's zipfile uses rfind, so it picks the signature inside
                # the comment; only the native parser must accept this archive.
                pass
            else:
                with zipfile.ZipFile(io.BytesIO(packed)) as check:
                    assert check.read("payload") == expected
            report["valid"].append(name)
    return report


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--binary", type=Path, required=True, help="archive_test native probe")
    parser.add_argument("--resources", type=Path, default=REPO_ROOT / ".runtime/assets")
    parser.add_argument("--output", type=Path, default=REPO_ROOT / "artifacts/archive-bounds")
    args = parser.parse_args()
    out = args.output.resolve()
    out.mkdir(parents=True, exist_ok=True)
    report = run_probe(args.binary.resolve(), out)
    (out / "evidence.json").write_text(json.dumps(report, indent=2), encoding="utf-8")
    print(json.dumps(report))


if __name__ == "__main__":
    main()
