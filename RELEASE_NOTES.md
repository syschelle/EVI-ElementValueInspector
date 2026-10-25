# EVI v0.2.19 Release Notes

## Added

- Multi-patient result navigation after a single global folder scan.
- Study selection when the selected patient contains multiple studies.
- Study size, series size, and instance/file size information.
- Automatic human-readable size units (KB, MB, GB).
- Exact byte sizes in CSV exports.

## Behavior

- The DICOM tag search still runs over the entire selected directory tree.
- Changing patient or study only filters the already scanned result set.
- The original search tag remains unchanged when switching patients or studies.
