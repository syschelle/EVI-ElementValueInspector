# EVI v0.2.15

## Grouped sequences in the DICOM Header Inspector

The per-instance DICOM Header Inspector now presents nested DICOM sequences as real expandable groups instead of displaying all elements in one flat list.

### Improved

- `SQ` elements are shown as expandable sequence nodes.
- Elements contained in a sequence are displayed directly below their parent sequence.
- Nested sequences are grouped recursively below their corresponding parent sequence.
- Dataset and File Meta Information are shown as separate top-level sections.
- Sequence groups remain collapsed by default for a cleaner overview.
- Opening an instance from a study-search result still jumps to the originating DICOM element and now automatically expands all required parent sequences.
- Header-local search continues to be independent of the main study search and automatically expands the hierarchy when a match is selected.

No behavior of the initial study search is changed by this release.
