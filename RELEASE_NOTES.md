# ElementValueInspector (EVI) v0.2.5

## Fixed

- Fixed MSVC `C2061` errors for `wxListEvent` in `MainFrame.h` and `TagDialog.h`.
- Fixed the resulting wxWidgets `Bind()` signature errors for list-selection and list-activation handlers.
- Fixed `wxString::Trim()` being called on a const string in the tag search dialog.
- Added the missing `<wx/msgdlg.h>` include for `wxMessageBox`.
- Fixed the ambiguous `const char*` / `wxString` conditional expression in study labels.
- Fixed DCMTK `DcmTag::getTagName()` constness by routing tag-name lookup through the existing `DicomUtils::TagName()` helper.
- Suppressed MSVC CRT deprecation warning C4996 for wxWidgets headers while compiling the EVI target.
- Corrected Windows EXE numeric version metadata to 0.2.5.0.
- Renamed the internal wxApp class from the legacy `EvaApp` name to `EviApp`.

## Build

The wxWidgets release-archive fix from v0.2.4 is retained. If the v0.2.4 CMake configure step already completed successfully, you can usually rebuild without deleting downloaded dependencies. For the cleanest upgrade, reconfigure the existing build directory before building:

```powershell
cmake -S . -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Release
```
