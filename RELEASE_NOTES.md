# EVI v0.2.11 Release Notes

## Fixed

- Fixed the Windows/MSVC build failure caused by duplicate embedded manifest resources (`CVTRES fatal error CVT1100` / `LNK1123`).
- Removed direct manifest embedding from `evi.rc`.
- The application manifest is now merged and embedded once by the MSVC linker via `/MANIFESTINPUT`.

## Unchanged

- Modern Light/Dark interface
- German/English runtime language switching
- Compact study/patient information strip
- Unicode/UTF-8 Windows UI handling
- Recursive DICOM tag search and nested sequence support
- Built-in DICOM dictionary selection and manual tag entry
- CSV export
- Portable native Windows build
