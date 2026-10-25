# EVI v0.2.19

## Multi-patient navigation and DICOM size information

EVI can now scan folders containing DICOM data from multiple patients while keeping the original tag search global across the entire selected directory tree.

### Added

- Patient selector for switching between all patients detected during a scan.
- Study selector for patients that contain more than one study.
- Patient/study switching filters the already collected results and does not start a new scan or change the initial DICOM tag search.
- Study size in the compact study information strip.
- Series size column in the Series list.
- Instance/file size column in Matches and instances.
- Human-readable automatic size formatting in KB, MB, and GB.
- CSV export now includes exact study, series, and instance sizes in bytes.

The scan still recursively covers the complete selected folder tree before the patient and study views are filtered.
