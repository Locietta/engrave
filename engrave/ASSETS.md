# Installed runtime assets

From a Git checkout, run `python tools/install_assets.py /absolute/install/share/engrave`.
The destination must be new. Pass it as `EngineOptions::resources` (or the gate's
`--assets`). Run the same command with `--verify` to check installed bytes against
this engine's committed Git blobs. Python and Git are packaging tools, not runtime
dependencies.

`engrave/assets.json` is versioned with the engine and maps each installed resource
path to its source path in the repository (templates, instruments, profiles, fonts,
and licenses). Installation reads committed blobs via `git cat-file --batch` so
checkout newline conversion cannot change resource bytes. The manifest is installed
inside that directory; no second provenance file or QRC regeneration is needed.
Existing `:/` names remain relative resource identifiers for internal readers.
External font and SoundFont overrides remain separate and retain their existing
lifetime/cache behavior.
