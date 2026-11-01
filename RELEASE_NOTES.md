# EVI v0.2.27 Release Notes

## SOP Class information

- The match/instance table now includes the SOP Class UID for every DICOM instance.
- EVI resolves known SOP Class UIDs to readable names using DCMTK's built-in UID registry.
- Unknown/private SOP Class UIDs remain visible and are clearly identified.
- CSV export now includes `SOPClassUID` and `SOPClassName`.
