# EVI – ElementValueInspector v0.2.11

## Windows build fix

This maintenance release fixes a compile-time name collision introduced by the modern Light/Dark theme implementation.

### Fixed

- Renamed the internal theme palette helper so it no longer collides with `wxWindowBase::GetPalette()`.
- Restores successful MSVC compilation of the Light/Dark theme code.
- Keeps the modernized interface, compact study information strip, German/English runtime switching, Unicode/UTF-8 handling, and native Windows dark title bar introduced in the previous UI refresh.

## Core capabilities

- Recursive scanning of complete DICOM study folders
- Search by manual DICOM tag or dictionary selection
- Search inside nested sequences
- Search inside File Meta Information
- Complete series overview, including series without matches
- Per-instance match details with VR, value, sequence path, file, and SOP Instance UID
- CSV export
- Progress display and scan cancellation
- Embedded DCMTK DICOM dictionary
- Local-only DICOM processing
- German and English user interface
- Light and Dark appearance modes

## Release assets

- `evi.exe` — portable Windows x64 application
- `evi.exe.sha256` — SHA-256 checksum

## Verify the executable

```powershell
Get-FileHash .\evi.exe -Algorithm SHA256
Get-Content .\evi.exe.sha256
```

The hashes must match.

## Build fix

- Fixed duplicate Windows manifest embedding that caused `CVTRES fatal error CVT1100` and `LNK1123` during MSVC builds.
- The resource script now contains version metadata only; the application manifest is merged by the linker via `/MANIFESTINPUT`.

