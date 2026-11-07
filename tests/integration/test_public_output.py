#!/usr/bin/env python3
"""Compare engrave's public output API with the platform goldens.

Runs the fork's ``engrave_output_probe``, which renders through the public API
(``Document::render``/``render_audio``) and writes the produced artifacts, then
compares every artifact with the stored platform golden exactly the way
``tests/engrave_testing/golden.py`` does:

* ``page-0.png.json``: ``describe_png`` (size, PIL mode, sha256 of the RGBA
  bytes) - so the pixels must equal the golden's, not just the file size;
* ``score.pdf.json``: ``describe_pdf`` (PyMuPDF normalization, page count,
  sha256 of the normalized document);
* ``page-N.svg``: the normalized SVG text;
* ``audio.json``: ``describe_audio`` features with the documented tolerances
  (``audio_equal``), including the exact-silence window evidence.

An output that is not identical is accepted only when ``tests/data/golden/allow.json``
pins the actual file's sha256 for this platform, i.e. under the same rule the
golden lane uses.

This module is used by tests/gate.py; partial captures are not an acceptance CLI.
"""

from __future__ import annotations

if __name__ == "__main__":
    raise SystemExit("Partial comparison CLI removed; use tests/gate.py for validated captures")
