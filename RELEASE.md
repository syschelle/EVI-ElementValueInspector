# EVI – ElementValueInspector v0.2.12

## DICOM instance sorting fix

EVI v0.2.12 corrects the ordering of matches in the **Matches / Instances** table.

### Fixed

- **Instance Number** is now sorted numerically rather than by scan/file order.
- When all series are displayed, results are ordered by **Series Number** and then by **Instance Number**.
- Numeric values are ordered naturally, for example: `1, 2, 3, 4, 10, 11, 12`.
- Missing or non-numeric values remain supported and are placed after numeric values in a stable order.

### Windows binary

The GitHub release provides:

- `evi.exe`
- `evi.exe.sha256`
