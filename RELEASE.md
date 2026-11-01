# EVI v0.2.28

This release improves column sizing in the match and instance view.

## Changes

- Match/instance columns now size themselves to the actual displayed content.
- Every column also respects the width of its complete header label, so headers such as `Modalität` / `Modality` are never clipped just because the cell values are shorter.
- Column widths are recalculated after scan results are populated and after the UI language changes.
- Long paths and UIDs can still expand the table naturally; horizontal scrolling remains available when required.
- Existing SOP Class information, DICOM Header Inspector, search modes and clipboard functions remain unchanged.

## Windows executable

Download `evi.exe` from the release assets. The matching `evi.exe.sha256` file can be used to verify the download.
