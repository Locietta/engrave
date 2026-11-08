#!/usr/bin/env python3
"""Generate a compact SoundFont (SF2) with a pure sine wave across banks 0 and 128.

All 128 presets in bank 0 and all 128 presets in bank 128 (including drum kit)
point to a single instrument zone with:
  - 440 Hz pure sine wave (2205 samples @ 44.1 kHz, seamless 22-cycle loop);
  - Flat volume envelope (1 ms attack/hold/decay/release, 0 cB sustain);
  - Open low-pass filter (initialFilterFc = 13500);
  - Zero reverb and chorus effect sends;
  - Zero modulation/vibrato LFOs.

This enables exact, predictable recovery of pitch, onset times, and signal levels.
"""

from __future__ import annotations

import argparse
import math
import struct
from pathlib import Path

from engrave_testing.paths import REPO_ROOT


def make_chunk(tag: str, data: bytes) -> bytes:
    pad = b"\x00" if len(data) % 2 == 1 else b""
    return tag.encode("ascii") + struct.pack("<I", len(data)) + data + pad


def make_list(list_type: str, data: bytes) -> bytes:
    return make_chunk("LIST", list_type.encode("ascii") + data)


def generate_sine_soundfont(output_path: Path | str) -> bytes:
    """Generate SF2 binary content and write to output_path."""
    output = Path(output_path)
    output.parent.mkdir(parents=True, exist_ok=True)

    # 1. INFO list
    info = make_list(
        "INFO",
        make_chunk("ifil", struct.pack("<HH", 2, 4))
        + make_chunk("isng", b"EMU8000\x00")
        + make_chunk("INAM", b"SineSoundFont\x00"),
    )

    # 2. sdta list: 2205 samples of 440 Hz sine @ 44100 Hz (exact 22 cycles) + 46 zero-padding samples
    sr = 44100
    n_samples = 2205
    amplitude = int(32767 * 0.9)  # 0.9 full-scale peak
    pcm_samples = [int(amplitude * math.sin(2.0 * math.pi * 22.0 * i / n_samples)) for i in range(n_samples)]
    pcm_samples.extend([0] * 46)  # SF2 spec requires >= 46 trailing zero samples
    smpl_data = struct.pack(f"<{len(pcm_samples)}h", *pcm_samples)
    sdta = make_list("sdta", make_chunk("smpl", smpl_data))

    # 3. pdta list
    # Instrument 0 generators
    # Notice: gen 53 (sampleID) must be the last generator in the instrument zone.
    igens = [
        (43, struct.pack("<BB", 0, 127)),  # keyRange [0, 127]
        (8, struct.pack("<h", 13500)),  # initialFilterFc open (~20 kHz)
        (34, struct.pack("<h", -12000)),  # attackVolEnv (~1 ms)
        (35, struct.pack("<h", -12000)),  # holdVolEnv (~1 ms)
        (36, struct.pack("<h", -12000)),  # decayVolEnv (~1 ms)
        (37, struct.pack("<h", 0)),  # sustainVolEnv (0 cB = 0 dB attenuation)
        (38, struct.pack("<h", -12000)),  # releaseVolEnv (~1 ms)
        (15, struct.pack("<h", 0)),  # chorusEffectsSend = 0
        (16, struct.pack("<h", 0)),  # reverbEffectsSend = 0
        (54, struct.pack("<h", 1)),  # sampleModes = 1 (continuous loop)
        (53, struct.pack("<H", 0)),  # sampleID = 0 (MUST BE LAST)
    ]
    ibags = [(0, 0)]  # (wGenNdx=0, wModNdx=0)
    insts = [(b"SineInstrument", 0)]  # (name, wInstBagNdx=0)

    # Presets for Bank 0 (programs 0..127) and Bank 128 (programs 0..127)
    presets = []
    pbags = []
    pgens = []
    gen_idx = 0
    bag_idx = 0
    for bank in (0, 128):
        for prog in range(128):
            name = f"Sine {bank}:{prog}".encode("ascii")[:19]
            presets.append((name, prog, bank, bag_idx))
            pbags.append((gen_idx, 0))
            # gen 41: instrument index = 0 (MUST BE LAST in preset zone)
            pgens.append((41, struct.pack("<H", 0)))
            gen_idx += 1
            bag_idx += 1

    # Pack phdr (38 bytes per preset + 1 terminal)
    phdr_data = bytearray()
    for name, prog, bank, b_idx in presets:
        phdr_data.extend(name.ljust(20, b"\x00"))
        phdr_data.extend(struct.pack("<HHHIII", prog, bank, b_idx, 0, 0, 0))
    # Terminal preset
    phdr_data.extend(b"EOT".ljust(20, b"\x00"))
    phdr_data.extend(struct.pack("<HHHIII", 0, 0, len(pbags), 0, 0, 0))

    # Pack pbag (4 bytes per bag + 1 terminal)
    pbag_data = bytearray()
    for g_idx, m_idx in pbags:
        pbag_data.extend(struct.pack("<HH", g_idx, m_idx))
    pbag_data.extend(struct.pack("<HH", len(pgens), 0))

    # Pack pmod (10 bytes terminal)
    pmod_data = b"\x00" * 10

    # Pack pgen (4 bytes per gen + 1 terminal)
    pgen_data = bytearray()
    for oper, val in pgens:
        pgen_data.extend(struct.pack("<H", oper) + val)
    pgen_data.extend(b"\x00" * 4)

    # Pack inst (22 bytes per instrument + 1 terminal)
    inst_data = bytearray()
    for name, b_idx in insts:
        inst_data.extend(name.ljust(20, b"\x00"))
        inst_data.extend(struct.pack("<H", b_idx))
    inst_data.extend(b"EOI".ljust(20, b"\x00"))
    inst_data.extend(struct.pack("<H", len(ibags)))

    # Pack ibag (4 bytes per bag + 1 terminal)
    ibag_data = bytearray()
    for g_idx, m_idx in ibags:
        ibag_data.extend(struct.pack("<HH", g_idx, m_idx))
    ibag_data.extend(struct.pack("<HH", len(igens), 0))

    # Pack imod (10 bytes terminal)
    imod_data = b"\x00" * 10

    # Pack igen (4 bytes per gen + 1 terminal)
    igen_data = bytearray()
    for oper, val in igens:
        igen_data.extend(struct.pack("<H", oper) + val)
    igen_data.extend(b"\x00" * 4)

    # Pack shdr (46 bytes per sample + 1 terminal)
    shdr_data = bytearray()
    shdr_data.extend(b"Sine440".ljust(20, b"\x00"))
    shdr_data.extend(
        struct.pack(
            "<IIIIIBbhH",
            0,  # dwStart
            n_samples,  # dwEnd
            0,  # dwStartloop
            n_samples,  # dwEndloop
            sr,  # dwSampleRate = 44100
            69,  # byOriginalPitch = A4 (MIDI 69 = 440 Hz)
            0,  # chPitchCorrection = 0
            0,  # wSampleLink = 0
            1,  # sfSampleType = 1 (monoSample)
        )
    )
    shdr_data.extend(b"\x00" * 46)

    pdta = make_list(
        "pdta",
        make_chunk("phdr", bytes(phdr_data))
        + make_chunk("pbag", bytes(pbag_data))
        + make_chunk("pmod", pmod_data)
        + make_chunk("pgen", bytes(pgen_data))
        + make_chunk("inst", bytes(inst_data))
        + make_chunk("ibag", bytes(ibag_data))
        + make_chunk("imod", imod_data)
        + make_chunk("igen", bytes(igen_data))
        + make_chunk("shdr", bytes(shdr_data)),
    )

    riff = make_chunk("RIFF", b"sfbk" + info + sdta + pdta)
    output.write_bytes(riff)
    return riff


def main() -> None:
    parser = argparse.ArgumentParser(description="Generate sine wave SoundFont (SF2)")
    parser.add_argument(
        "output",
        nargs="?",
        type=Path,
        default=REPO_ROOT / "tests/data/engine_audio" / "sine.sf2",
        help="Destination SF2 path (default: tests/data/engine_audio/sine.sf2)",
    )
    args = parser.parse_args()

    content = generate_sine_soundfont(args.output)
    print(f"Generated {args.output} ({len(content)} bytes)")


if __name__ == "__main__":
    main()
