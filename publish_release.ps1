param(
    [string]$Version = "0.2.8"
)

$ErrorActionPreference = "Stop"
$Tag = "v$Version"
$Root = Split-Path -Parent $MyInvocation.MyCommand.Path
$Exe = Join-Path $Root "dist\evi.exe"
$Sha = Join-Path $Root "dist\evi.exe.sha256"
$Notes = Join-Path $Root "RELEASE.md"

if (-not (Get-Command gh -ErrorAction SilentlyContinue)) {
    throw "GitHub CLI (gh) wurde nicht gefunden. Installation: winget install GitHub.cli"
}

if (-not (Test-Path $Exe)) {
    throw "evi.exe fehlt unter dist. Zuerst bauen: cmake --build build --config Release"
}
if (-not (Test-Path $Sha)) {
    throw "evi.exe.sha256 fehlt unter dist. Bitte EVI v0.2.8 neu bauen."
}
if (-not (Test-Path $Notes)) {
    throw "RELEASE.md fehlt im Projektverzeichnis."
}

Write-Host "Pruefe GitHub-Anmeldung ..."
gh auth status
if ($LASTEXITCODE -ne 0) {
    throw "GitHub CLI ist nicht angemeldet. Bitte zuerst 'gh auth login' ausfuehren."
}

Write-Host "Pruefe Release $Tag ..."
# A missing release is an expected state for the first publication. Run the
# probe through cmd.exe so PowerShell does not turn GitHub CLI stderr into a
# terminating NativeCommandError while $ErrorActionPreference is "Stop".
$ProbeCommand = 'gh release view "' + $Tag + '" >nul 2>&1'
cmd.exe /d /c $ProbeCommand
$ReleaseExists = ($LASTEXITCODE -eq 0)

if ($ReleaseExists) {
    Write-Host "Release $Tag existiert bereits. Assets werden aktualisiert ..."
    gh release upload $Tag $Exe $Sha --clobber
} else {
    Write-Host "Erzeuge Release $Tag mit EXE und SHA-256 ..."
    gh release create $Tag $Exe $Sha --title "EVI v$Version" --notes-file $Notes
}

if ($LASTEXITCODE -ne 0) {
    throw "GitHub Release konnte nicht erstellt/aktualisiert werden."
}

Write-Host ""
Write-Host "Fertig. Release-Assets:"
Write-Host "  evi.exe"
Write-Host "  evi.exe.sha256"
