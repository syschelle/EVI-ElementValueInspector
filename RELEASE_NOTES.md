# ElementValueInspector (EVI) v0.2.7

## Changes

- Fixed first-time GitHub release publishing in `publish_release.ps1`.
- A missing release is now treated as the expected state and no longer aborts the script with `NativeCommandError`.
- Release existence is checked via the GitHub CLI exit code without exposing the expected `release not found` stderr message to PowerShell.
- Existing releases are still updated with `--clobber`.
- New releases still upload both `dist/evi.exe` and `dist/evi.exe.sha256`.
- Corrected the Windows executable numeric file/product version to `0.2.7.0`.
- DICOM inspection functionality is unchanged.

## Release assets

```text
dist\evi.exe
dist\evi.exe.sha256
```

## Publish

After building, committing and pushing the source code:

```powershell
powershell.exe -ExecutionPolicy Bypass -File .\publish_release.ps1
```

The script now handles both cases automatically:

- `v0.2.7` does not exist → create the release and upload both assets.
- `v0.2.7` already exists → replace/update both assets.
