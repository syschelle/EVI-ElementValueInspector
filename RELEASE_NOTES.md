# EVI v0.2.14 Release Notes

## Fixed

- Fixed MSVC errors `C2660` and `C2440` in `HeaderFrame.cpp`.
- The internal helper previously named `Lower()` collided with the inherited wxWidgets method `wxWindow::Lower()`. It is now named `LowerText()`.
- The DICOM Header Inspector can now be compiled normally on Windows while retaining its independent header search and automatic selection of the original match.
