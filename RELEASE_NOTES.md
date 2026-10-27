# EVI v0.2.23 Release Notes

This release extends the main DICOM scan beyond tag-only searches.

## New

- **DICOM tag** search remains available exactly as before.
- **Value** search finds text contained in any readable DICOM element value.
- Value matching is case-insensitive and uses substring matching.
- Matches inside nested sequences and File Meta Information are included.
- Results identify the concrete tag and instance that contains the matching value.

## Search safety and performance

- Pixel Data is never searched as text.
- Sequence container placeholders are not treated as content.
- Large binary/deferred values are skipped during value matching.
- All processing remains local.
