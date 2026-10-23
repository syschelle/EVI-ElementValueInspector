# EVI – ElementValueInspector v0.2.5

## Initial Native Windows Release

EVI (ElementValueInspector) is a native Windows application for inspecting DICOM studies and searching for specific DICOM data elements.

The application scans complete DICOM study folders, identifies series and instances, and searches for a selected DICOM tag across the entire study, including nested sequences.

## Features

- Native Windows application written in C++20
- Single portable `evi.exe`
- No Java or Python runtime required
- DCMTK-based DICOM parsing
- wxWidgets-based Windows user interface
- Recursive scan of complete DICOM study folders
- Automatic detection of DICOM series and instances
- Search for DICOM tags throughout the complete study
- Recursive search inside nested DICOM sequences
- Search of File Meta Information elements
- Manual DICOM tag entry, for example:
  - `(0018,1030)`
  - `00181030`
- DICOM tag selection from the built-in DCMTK dictionary
- Searchable tag list
- Series overview including:
  - Series Number
  - Modality
  - Series Description
  - Protocol Name
  - Body Part Examined
  - Instance count
  - Series Instance UID
- Match details including:
  - DICOM tag
  - VR
  - value
  - instance number
  - SOP Instance UID
  - sequence path
  - source file
- CSV export of search results
- Progress display during study scanning
- Scan cancellation
- Built-in DICOM standard dictionary
- Unicode and long-file support

## Build Environment

EVI v0.2.5 was successfully built and tested with:

- Microsoft Visual Studio 2022 Build Tools
- MSVC 19.44
- Windows SDK 10.0.26100
- CMake 4.4.4
- DCMTK 3.7.0
- wxWidgets 3.2.12
- C++20

## Build

Configure:

```powershell
cmake -S . -B build -G "Visual Studio 17 2022" -A x64
```

Build the Release version:

```powershell
cmake --build build --config Release
```

The resulting executable is located at:

```text
build\bin\Release\evi.exe
```

## Notes

The application uses DCMTK's built-in DICOM dictionary. No external `dicom.dic` file is required.

wxWidgets and DCMTK are statically linked for the Windows build.

Large DICOM binary values such as Pixel Data are intentionally not processed as normal metadata values during study inspection in order to keep scanning efficient.

## Version

**EVI – ElementValueInspector v0.2.5**
