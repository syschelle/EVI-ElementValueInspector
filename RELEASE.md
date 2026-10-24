# EVI v0.2.17

## Transparent Windows application icon

EVI now uses the transparent version of the supplied gold EVI artwork as its native Windows application icon.

### Changed

- Replaced the previous application icon with the transparent EVI logo.
- Preserved alpha transparency in the Windows ICO resource.
- Embedded multiple Windows icon sizes: 16, 24, 32, 48, 64, 128 and 256 pixels.
- The same transparent icon is used by `evi.exe`, the main EVI window, the DICOM Header Inspector, the Windows taskbar and Alt+Tab.
- No external image or icon file is required at runtime.

The portable release remains a single native `evi.exe` plus the separately published SHA-256 checksum file.
