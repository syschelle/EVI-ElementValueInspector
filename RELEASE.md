# EVI – ElementValueInspector v0.2.8

## Native Windows DICOM Element Inspector

EVI (ElementValueInspector) is a portable native Windows application for inspecting complete DICOM studies and locating specific DICOM data elements across studies, series, instances, and nested sequences.

## New in v0.2.8

- Modernized and more compact main window.
- Compact two-row study/patient information panel with more room for result tables.
- Selectable **Deutsch / English** user interface.
- Language switching without restarting the application.
- Localized controls, table headers, scan status, dialogs, and errors.
- DICOM tag selection dialog follows the selected language.
- Long study values are displayed compactly with the complete value available as a tooltip.

## Core features

- Native C++20 Windows application
- Single portable `evi.exe`
- No Java or Python runtime required
- DCMTK 3.7.0 based DICOM parsing
- wxWidgets 3.2.12 based Windows user interface
- Recursive scanning of complete DICOM study folders
- Automatic study, series, and instance detection
- Manual DICOM tag entry
- Searchable selection from the built-in DICOM dictionary
- Recursive search inside nested DICOM sequences
- File Meta Information search
- Series overview and per-instance match details
- CSV export
- Progress display and scan cancellation
- Built-in DICOM dictionary; no external `dicom.dic` required
- Static Windows runtime and libraries for a portable executable

## Release assets

- `evi.exe` – portable Windows x64 application
- `evi.exe.sha256` – SHA-256 checksum

## Verify the download

```powershell
Get-FileHash .\evi.exe -Algorithm SHA256
Get-Content .\evi.exe.sha256
```

The hash values must match.

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
