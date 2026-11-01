# EVI v0.2.27

This release adds SOP Class identification to the main match and instance view.

## Changes

- Added **SOP Class UID** for each matching DICOM instance.
- Added a readable **SOP Class** column resolved through DCMTK's built-in DICOM UID registry.
- Known standard classes can therefore be shown by name, for example `CT Image Storage`, while the original UID remains visible.
- Unknown or private SOP Class UIDs remain visible and are marked as unknown/private in the UI.
- Added SOP Class UID and resolved name to CSV exports.
- Existing tag/value search, patient/study filtering, DICOM Header Inspector and clipboard functions remain unchanged.

## Windows executable

Download `evi.exe` from the release assets. The matching `evi.exe.sha256` file can be used to verify the download.
