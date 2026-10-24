# EVI v0.2.16

## Native Windows application icon

EVI now includes its own application icon based on the supplied gold EVI artwork.

### Added

- Embedded multi-resolution Windows icon in `evi.exe`.
- Application icon for the main EVI window.
- Application icon for the per-instance DICOM Header Inspector.
- The same icon is available to Windows Explorer, the taskbar and Alt+Tab through the executable resources and top-level window icons.
- Icon sizes embedded in the ICO resource: 16, 24, 32, 48, 64, 128 and 256 pixels.

The icon is compiled into the executable. No external image or icon file is required next to `evi.exe` at runtime.
