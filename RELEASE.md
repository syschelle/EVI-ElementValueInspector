# EVI v0.2.25

This release improves the per-instance DICOM Header Inspector for multi-line metadata and fast value extraction.

## Changes

- Header rows now grow automatically when a DICOM value contains multiple lines.
- Multi-line values are shown completely instead of being clipped by a fixed row height.
- Added cell-aware clipboard copy in the DICOM Header Inspector.
- Right-clicking a Tag, Name, Value or Sequence Path cell offers a matching copy command.
- `Ctrl+C` copies the currently focused header cell.
- VR cells intentionally do not provide a copy action.
- Sequence grouping, alternating row backgrounds, local header search and automatic navigation to the original study-search hit remain unchanged.

## Windows executable

Download `evi.exe` from the release assets. The matching `evi.exe.sha256` file can be used to verify the download.
