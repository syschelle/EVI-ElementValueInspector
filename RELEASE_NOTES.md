# EVI v0.2.12 Release Notes

## Fixed

- Match rows are now sorted numerically by Series Number and Instance Number.
- Instance Number ordering now follows the expected DICOM numeric sequence (`1, 2, 3, …, 10, 11 …`) instead of scan/file order.
- Missing or non-numeric instance values remain supported and are placed after numeric values in a stable order.
