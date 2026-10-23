# ElementValueInspector (EVI) v0.2.7

Native Windows DICOM study and tag inspection application written in C++.

## Ziel

Eine komplette DICOM-Studie lokal nach einem bestimmten DICOM-Tag durchsuchen und die Treffer zusammen mit Studien-, Serien- und Instanzdaten anzeigen.

Die fertige Anwendung ist eine einzelne portable Windows-Datei:

```text
dist\evi.exe
```

Auf dem Zielrechner werden weder Java noch Python noch separate DCMTK-/wxWidgets-DLLs benoetigt.

## Technik

- C++20
- wxWidgets 3.2.12, statisch gelinkt
- DCMTK 3.7.0, statisch gelinkt
- eingebautes DCMTK-DICOM-Dictionary
- Microsoft Visual C++ Runtime statisch (`/MT`)
- CMake/Visual Studio 2022 Build Tools

## Funktionen

- kompletter Studienordner inklusive Unterordner
- Anzeige der Studieninformationen
- Anzeige aller gefundenen Serien, auch ohne Treffer
- Serieninformationen:
  - Series Number
  - Modality
  - Series Description
  - Protocol Name
  - Body Part Examined
  - Anzahl Instanzen
  - Trefferzahl
  - Series Instance UID
- DICOM-Tag manuell eingeben:
  - `(0018,1030)`
  - `00181030`
  - `ProtocolName`
- private/unbekannte Tags koennen als numerischer Tag eingegeben werden
- Dialog **Tag auswaehlen ...** mit dem vollstaendigen eingebetteten DCMTK-Dictionary
- Tag-Liste nach Tagnummer, Name/Keyword, VR und VM durchsuchen
- rekursive Suche innerhalb verschachtelter DICOM-Sequences
- Suche auch in File Meta Information
- Trefferanzeige mit:
  - Serie
  - Instance Number
  - Modality
  - Tag
  - Tagname
  - VR
  - Wert
  - Sequence-Pfad
  - Datei
  - SOP Instance UID
- Treffer einer einzelnen Serie filtern
- CSV-Export
- Fortschrittsanzeige
- laufenden Scan abbrechen
- PixelData und sehr grosse Binary-Werte werden nicht fuer die Anzeige geladen
- Unicode-Dateipfade unter Windows



### v0.2.5 compiler fixes

- fixed `wxListEvent` declarations for the series and tag-selection list controls
- fixed wxWidgets event-handler binding compilation
- fixed const-safe filtering in the DICOM tag dialog
- fixed the study-label `wxString` conditional expression
- fixed DCMTK tag-name lookup for const `DcmTag` references
- added the missing wxWidgets message-dialog include
- suppresses MSVC CRT deprecation warnings originating from wxWidgets headers while compiling EVI

### Updating from v0.2.3

If you already ran CMake with v0.2.3, remove the old build cache once before configuring:

```powershell
Remove-Item -Recurse -Force build
cmake -S . -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Release
```

v0.2.4 uses the official wxWidgets 3.2.12 Windows source archive, which includes the required third-party/submodule sources.

## Build unter Windows

### Einzige Systemvoraussetzung

Microsoft Visual Studio 2022 Build Tools mit dem Workload **Desktop development with C++** und Windows SDK.

Falls noch nicht vorhanden:

```bat
winget install Microsoft.VisualStudio.2022.BuildTools --override "--wait --passive --add Microsoft.VisualStudio.Workload.VCTools --includeRecommended"
```

Danach reicht:

```bat
build_windows.bat
```

Das Buildskript:

1. erkennt die installierten Visual C++ Build Tools,
2. laedt CMake 4.2.0 portabel nur nach `.build`,
3. laedt wxWidgets und DCMTK per CMake FetchContent nur nach `.build`,
4. baut alle Bibliotheken statisch,
5. erzeugt `dist\evi.exe`.

Es wird weder DCMTK noch wxWidgets noch CMake systemweit installiert.

Der komplette lokale Buildcache kann jederzeit durch Loeschen von `.build` entfernt werden.

### Visual-Studio-Erkennung in v0.2.3

Die Erkennung von `vswhere.exe` wurde fuer Windows-Pfade wie `C:\Program Files (x86)\...` korrigiert. Das Buildskript verwendet fuer die Rueckgabe des Visual-Studio-Installationspfads keine `for /f`-Command-Substitution mehr.

## VS Code

Den Projektordner in VS Code oeffnen. Empfohlen werden:

- Microsoft C/C++
- CMake Tools

`Ctrl+Shift+B` startet den Task:

**ElementValueInspector (EVI): Build Native EXE**

Weitere Task:

**ElementValueInspector (EVI): Run**

## Datenschutz

Die Anwendung verarbeitet DICOM-Dateien ausschliesslich lokal. Es findet kein Upload und keine Netzwerkuebertragung der DICOM-Daten statt.

Ein CSV-Export kann Patientendaten enthalten und muss entsprechend geschuetzt behandelt werden.

## Release-Artefakte ab v0.2.6

Jeder erfolgreiche Release-Build erzeugt automatisch im Projektordner:

```text
dist\evi.exe
dist\evi.exe.sha256
```

Die Datei `evi.exe.sha256` enthaelt die SHA-256-Pruefsumme der exakt gebauten EXE und kann zusammen mit der EXE als GitHub-Release-Asset veroeffentlicht werden.

Direkter CMake-Build:

```powershell
cmake -S . -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Release
```

Danach kann der GitHub Release nach dem Commit/Push mit folgendem Skript erstellt werden:

```powershell
.\publish_release.ps1
```

Das Skript erstellt den Release `v0.2.7` mit:

- `evi.exe`
- `evi.exe.sha256`
- Release-Text aus `RELEASE.md`

Existiert der Release bereits, werden die beiden Assets mit `--clobber` aktualisiert. Existiert er noch nicht, wird er automatisch neu angelegt. v0.2.7 behebt dabei den Abbruch durch die erwartete GitHub-CLI-Meldung `release not found`.
