# ElementValueInspector (EVI)

ElementValueInspector (EVI) is a native Windows application for inspecting complete DICOM studies and locating specific DICOM data elements or matching text inside DICOM element values across studies, series, instances, file meta information, and nested sequences.

EVI is designed as a lightweight local inspection tool. It does not require a PACS connection and does not upload DICOM data to any external service.

## What EVI does

EVI scans a selected study folder recursively, identifies DICOM files, builds a patient/study/series overview, and can search every readable DICOM instance either for a selected data element or for text contained in DICOM element values.

The main search offers two independent modes:

- **DICOM tag** searches for one specific element by tag number or DICOM name.
- **Value** searches the values of all readable DICOM elements for a text fragment. Value matching is case-insensitive and uses substring matching, so `lowdose` also matches values such as `Thorax_LowDose`.

A tag search can be started with a manually entered tag or with a tag selected from the built-in DICOM dictionary.

Supported manual tag formats include:

```text
(0018,1030)
00181030
ProtocolName
```

Numerical tag entry also allows private or vendor-specific tags to be inspected even when they are not part of the standard dictionary.

### Value/content search

In **Value** mode, EVI traverses the readable elements of the complete dataset, nested sequences, and File Meta Information and returns the concrete DICOM elements whose displayed value contains the search text. Matching is case-insensitive. The result list still shows the actual tag number, tag name, VR, value, sequence path, series, and instance that produced each match.

Pixel Data, sequence container placeholders, and large binary/deferred values are deliberately excluded from content matching to keep scans responsive and to avoid treating binary data as text.

## DICOM tag browser

The **Select tag** dialog provides a searchable view of the DICOM dictionary embedded in DCMTK.

Tags can be filtered by:

- tag number
- tag name or keyword
- VR
- VM
- standard/version information provided by the dictionary

The selected tag is transferred directly into the main search field.

## Patient, study, and series overview

EVI scans the complete selected folder tree in a single pass, even when the directory contains DICOM data from multiple patients. After scanning, the result view can be switched between the detected patients without running the main search again.

For a patient with more than one study, a separate study selector keeps the series and instance view unambiguous. The patient and study selectors only filter the already collected results; they never change the original main search or limit which folders are scanned.

The compact study strip displays:

- patient name selection
- Patient ID as a separate field
- study selection
- study date
- accession number
- Study Instance UID
- total DICOM size of the selected study

Long values remain available through tooltips without consuming unnecessary screen space.

The series overview includes:

- Series Number
- Modality
- Series Description
- Protocol Name
- Body Part Examined
- number of instances
- number of matches
- total DICOM size of the series
- Series Instance UID

All detected series of the selected patient and study are shown, including series that do not contain a match for the current search.

## Match details

Each matching DICOM element is shown together with its instance and series context:

- Series Number
- Instance Number
- Modality
- tag number
- tag name
- VR
- element value
- nested sequence path
- relative source file
- instance file size
- SOP Instance UID

File sizes are displayed automatically in a human-readable unit (KB, MB, or GB depending on size). Selecting a series filters the match list to that series. **Show all** restores all matches for the currently selected patient and study.

## Per-instance DICOM header inspector

Double-click any row in **Matches and instances** to open the complete metadata header of that exact DICOM instance in a separate window.

The header inspector:

- loads the selected instance independently of the study search
- displays File Meta Information and the recursively traversed dataset
- presents DICOM sequences as expandable groups instead of mixing all nested elements into one flat list
- nests sequence contents below the corresponding `SQ` element while preserving tag, name, VR, value, and sequence-path columns
- automatically expands the required sequence groups, selects, and scrolls to the element that opened the window
- keeps the original study search and result set unchanged

The header window has its own search field. It can locate elements by:

- exact tag number, for example `(0008,1030)`
- DICOM keyword/name, for example `StudyDescription`
- VR
- displayed value
- sequence path

Repeated **Find next** actions cycle through all matches inside the current instance. This secondary search is local to the header window and never replaces or modifies the search used for the main folder scan.

Large binary values and Pixel Data remain omitted from textual display so that the inspector stays responsive on large imaging objects.

## Nested sequences and file meta information

The dataset traversal is recursive. EVI can therefore locate requested tags and content matches inside nested DICOM sequences instead of searching only the dataset root.

File Meta Information is searched as well, allowing `(0002,xxxx)` elements to be inspected.

## User interface

EVI provides a compact native Windows interface with:

- German and English user interface languages
- runtime language switching without restarting the application
- Light and Dark appearance modes
- native Windows title-bar dark mode support
- high-DPI awareness
- modern Windows common controls
- integrated EVI magnifying-glass application icon with transparent background for Explorer, the taskbar, Alt+Tab, and application windows
- compact study information strip
- resizable 50/50 Series / Matches workspace with a manually draggable splitter
- alternating table rows for easier reading
- progress feedback during scanning
- scan cancellation

The interface uses Unicode throughout and explicitly enables UTF-8 at the Windows process level to preserve German umlauts and other non-ASCII characters.

## CSV export

Main-search results can be exported as a UTF-8 CSV file. The export contains patient, study, series, instance, tag, value, sequence-path, source-file, and exact byte-size information for studies, series, and instances.

Because exported files can contain patient information, exported CSV files should be handled according to the same privacy and security requirements as the source DICOM data.

## Performance and large data elements

EVI is intended for metadata inspection, not image rendering.

DICOM files are read with a limited maximum element read length, and large binary values are not loaded into the result view. Pixel Data is explicitly represented as omitted rather than being displayed as metadata text. This keeps study scans responsive and avoids unnecessary memory use on large CT, MR, and similar studies.

## Local processing and privacy

All DICOM processing is performed locally on the Windows computer running EVI.

EVI does not:

- upload DICOM files
- contact a PACS automatically
- send patient information to a cloud service
- require an online account

Network access is only relevant during source builds when CMake downloads the declared third-party source dependencies.

## Technology

EVI is implemented as a native C++20 Windows application using:

- DCMTK 3.7.0 for DICOM parsing and the embedded DICOM dictionary
- wxWidgets 3.2.12 for the native Windows user interface
- CMake for project configuration and builds
- Microsoft Visual C++ for Windows x64 builds

DCMTK, wxWidgets, and the Microsoft C/C++ runtime are linked so that the release output is a portable executable without separate application DLLs.

## Build requirements

The build machine requires:

- Windows 10 or Windows 11 x64
- Visual Studio 2022 Build Tools
- **Desktop development with C++** workload
- Windows SDK
- CMake

CMake can be installed with Windows Package Manager if required:

```powershell
winget install Kitware.CMake
```

## Configure and build

From the repository root:

```powershell
cmake -S . -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Release
```

The compiled application is created under:

```text
build\bin\Release\evi.exe
```

The build also creates ready-to-publish release artifacts:

```text
dist\evi.exe
dist\evi.exe.sha256
```

The SHA-256 file contains the checksum of the exact executable produced by the build.

## VS Code

The repository contains VS Code task definitions for building and running EVI.

Recommended extensions:

- Microsoft C/C++
- CMake Tools

The default build task can be started with:

```text
Ctrl+Shift+B
```

## Repository layout

```text
src/                 C++ application sources
resources/           Embedded Windows application icon
cmake/               CMake helper scripts
.vscode/             VS Code tasks and extension recommendations
CMakeLists.txt        CMake project definition
build_windows.bat     automated Windows build helper
run_windows.bat       starts the built portable executable
RELEASE.md            current GitHub release text
RELEASE_NOTES.md      current release notes
THIRD_PARTY.md        third-party component information
```

Build output directories such as `build/`, `.build/`, and executable release artifacts under `dist/` are excluded from normal source control.

## Windows application identity

EVI is distributed as a native Windows executable with an embedded multi-resolution application icon using the modern navy EVI monogram on a transparent background. Alpha transparency is preserved in the Windows icon resource. The same icon is used by the executable, the main application window, the DICOM Header Inspector, the Windows taskbar, and Alt+Tab. No external icon file is required at runtime. The transparent source artwork is retained under `resources/evi-logo.png` for maintainability.
