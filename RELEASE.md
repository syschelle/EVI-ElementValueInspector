# EVI v0.2.23

## Changes

- Added a new main search mode for DICOM element **values/content** in addition to the existing DICOM tag search.
- Value matching uses case-insensitive substring matching across readable DICOM elements.
- Content search traverses the full selected folder tree, nested sequences, and File Meta Information.
- Every content match is reported with its actual tag, tag name, VR, value, series, instance, and sequence path.
- Pixel Data, sequence placeholders, and large binary/deferred values are excluded from content matching.
- Patient and study selectors continue to filter only the already collected results and do not limit the global scan.
- Updated UI wording, CSV naming, documentation, and Windows version metadata.

## Windows binary

The release provides `evi.exe` and its SHA-256 checksum as GitHub Release assets.
