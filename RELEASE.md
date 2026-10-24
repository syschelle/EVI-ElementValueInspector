# EVI v0.2.14

## Header Inspector build fix

This release fixes a Windows/MSVC compile error introduced with the per-instance DICOM Header Inspector.

### Fixed

- Renamed the internal Header Inspector string-lowercasing helper to avoid a name collision with `wxWindow::Lower()`.
- Restored successful compilation of `HeaderFrame.cpp` with wxWidgets on Windows.
- The per-instance Header Inspector, automatic jump to the originating DICOM element, and its independent local tag search remain unchanged.

No DICOM search behavior or study results are changed by this release.
