# Installed Runtime Assets

`engrave` loads its runtime assets (SMuFL music fonts, text fonts, instrument
definitions, score templates, engraving styles, MPE profiles, and license
notices) from a directory passed via `EngineOptions::resources` (or `--assets`
for test runners).

## Installing and Verifying

From a Git checkout of `engrave`, install assets into a new destination
directory:

```bash
python tools/install_assets.py /absolute/install/share/engrave
```

Verify an installed directory against the committed Git blobs:

```bash
python tools/install_assets.py /absolute/install/share/engrave --verify
```

Python and Git are packaging tools only, not runtime dependencies.

## Manifest and Byte Stability

- `engrave/assets.json` is versioned with the engine and maps each installed
  resource path to its source path in the repository.
- `tools/install_assets.py` reads committed Git blobs via
  `git cat-file --batch`, so local checkout line-ending conversions cannot alter
  installed bytes.
- The manifest is copied into the destination directory; no secondary provenance
  file or QRC regeneration is needed.
- Internal readers continue to address bundled resources via `:/` relative
  identifiers. External font and SoundFont overrides remain separate and keep
  their own lifetime and cache behavior.
