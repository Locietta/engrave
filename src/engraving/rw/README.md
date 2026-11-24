# Reading and writing MuseScore files

## Public / internal

The classes in the root of the `rw` folder and in the `mu::engraving::rw` namespace (`Reader`, `MscLoader`, `MscSaver`, `RWRegister`, etc.) are intended for use outside the read-write subsystem. Classes in the subfolders (`read/`, `write/`, `compat/`) are internal implementation details of the read-write subsystem.

## Layout and supported versions

- `read/` (`mu::engraving::read`): Unified XML reader (`TRead`, `MeasureRead`, `StaffRead`, `ReadContext`, `ConnectorInfoReader`).
- `write/` (`mu::engraving::write`): Native score writer (`Writer`, `TWrite`, `MeasureWrite`, `StaffWrite`, `WriteContext`, `ConnectorInfoWriter`).
- `compat/` (`mu::engraving::compat`): Format compatibility helpers (`CompatUtils`, `ReadStyle`, `ReadChordListHook`, `MMRestSources`, `tremolocompat.h`).

Only MuseScore format versions 4.60 through 5.00 (`460 <= mscVersion <= Constants::msc_version`) are supported; `RWRegister::reader` and `MscLoader` reject versions below `460` (`Err::FILE_TOO_OLD`) and versions above `Constants::msc_version` (`Err::FILE_TOO_NEW`). Version-specific differences within 4.60–5.00 are handled conditionally via `ReadContext::msc_version()` and `compat/`.
