# EVI v0.2.25 Release Notes

## DICOM Header Inspector

- Added dynamic row heights for multi-line DICOM values.
- Values containing embedded line breaks are now fully visible without overlapping or clipping adjacent rows.
- Added context-sensitive clipboard copy for Tag, Name, Value and Sequence Path cells.
- Added `Ctrl+C` support for the currently focused header cell.
- VR remains display-only and is intentionally excluded from clipboard copy.

## Existing behaviour retained

- Hierarchical sequence grouping and expand/collapse behaviour are unchanged.
- Alternating row backgrounds remain enabled.
- Header-local search still does not modify the main study search.
- Opening a header from a study-search result still navigates directly to the originating DICOM element.

## Application

EVI remains a native C++20 Windows application using DCMTK and wxWidgets. The release build produces the portable `evi.exe` and its SHA-256 checksum.
