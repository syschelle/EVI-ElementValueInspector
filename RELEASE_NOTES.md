# EVI v0.2.15 Release Notes

## Improved

- Replaced the flat per-instance DICOM header list with a hierarchical tree/list view.
- DICOM sequences (`VR=SQ`) are now expandable groups.
- Nested sequence contents remain visually grouped under their parent sequence.
- Added separate top-level groups for the DICOM Dataset and File Meta Information.
- Automatic jump-to-match and header-local search now expand all required parent sequence nodes before selecting a result.
- The original study search remains unchanged when using the Header Inspector.
