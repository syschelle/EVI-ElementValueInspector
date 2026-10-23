# EVI – ElementValueInspector v0.2.7

## Native Windows DICOM Element Inspector

EVI (ElementValueInspector) is a native Windows application for inspecting complete DICOM studies and locating specific DICOM data elements across studies, series, instances, and nested sequences.

## Highlights

- Native C++20 Windows application
- Single portable `evi.exe`
- No Java or Python runtime required
- DCMTK 3.7.0 based DICOM parsing
- wxWidgets 3.2.12 based Windows user interface
- Recursive scanning of complete DICOM study folders
- Automatic study, series, and instance detection
- Search by manually entered DICOM tag
- Searchable selection from the built-in DICOM dictionary
- Recursive search inside nested DICOM sequences
- File Meta Information search
- Series overview and per-instance match details
- CSV export
- Progress display and scan cancellation
- Built-in DICOM dictionary; no external `dicom.dic` required
- Static Windows runtime and libraries for a portable executable

## New in v0.2.7

- Fixed first-time publishing with `publish_release.ps1`.
- A missing GitHub release is now correctly detected and created instead of terminating with `NativeCommandError`.
- Existing releases can still be updated with the EXE and SHA-256 checksum.
- Windows file/product version updated to `0.2.7.0`.

## Release assets

- `evi.exe` – portable Windows x64 application
- `evi.exe.sha256` – SHA-256 checksum for verifying the executable

## Verify the download

In PowerShell:

```powershell
Get-FileHash .\evi.exe -Algorithm SHA256
Get-Content .\evi.exe.sha256
```

The hash values must match.

## Build environment

The Windows build is intended for:

- Microsoft Visual Studio 2022 Build Tools
- Windows SDK
- CMake
- C++20
- DCMTK 3.7.0
- wxWidgets 3.2.12

## Build

```powershell
cmake -S . -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Release
```

Result:

```text
dist\evi.exe
dist\evi.exe.sha256
```
