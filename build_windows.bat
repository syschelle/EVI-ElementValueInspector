@echo off
setlocal EnableExtensions EnableDelayedExpansion
cd /d "%~dp0"

set "APP_VERSION=0.2.18"
set "ROOT=%CD%"
set "LOCAL=%ROOT%\.build"
set "CMAKE_HOME=%LOCAL%\cmake"
set "CMAKE_EXE=%CMAKE_HOME%\bin\cmake.exe"
set "BUILD_DIR=%LOCAL%\build"
set "DIST_DIR=%ROOT%\dist"
set "CMAKE_VERSION=4.2.0"
set "CMAKE_ZIP=cmake-%CMAKE_VERSION%-windows-x86_64.zip"
set "CMAKE_URL=https://github.com/Kitware/CMake/releases/download/v%CMAKE_VERSION%/%CMAKE_ZIP%"

echo === ElementValueInspector (EVI) v%APP_VERSION% - Native Windows Build ===
echo.

if not exist "%LOCAL%" mkdir "%LOCAL%"
if not exist "%DIST_DIR%" mkdir "%DIST_DIR%"

rem ----------------------------------------------------------------------
rem Find Visual Studio 2022 / Build Tools. This is the only system-level
rem prerequisite because MSVC and the Windows SDK cannot be redistributed
rem project-locally in a sensible/supportable way.
rem ----------------------------------------------------------------------
set "VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
if not exist "%VSWHERE%" set "VSWHERE=%ProgramFiles%\Microsoft Visual Studio\Installer\vswhere.exe"

if not exist "%VSWHERE%" (
    echo FEHLER: Microsoft Visual Studio 2022 Build Tools wurden nicht gefunden.
    echo.
    echo Benoetigt wird der Workload "Desktop development with C++" inklusive Windows SDK.
    echo Installation per winget, falls gewuenscht:
    echo   winget install Microsoft.VisualStudio.2022.BuildTools --override "--wait --passive --add Microsoft.VisualStudio.Workload.VCTools --includeRecommended"
    echo.
    pause
    exit /b 1
)

rem Do not call vswhere through FOR /F command substitution here.  Paths such as
rem "C:\Program Files (x86)\..." contain parentheses and can be reparsed by
rem cmd.exe inside a parenthesized FOR command, resulting in errors such as
rem "\Microsoft kann syntaktisch an dieser Stelle nicht verarbeitet werden."
set "VSROOT_FILE=%LOCAL%\vs-installation-path.txt"
if exist "%VSROOT_FILE%" del /q "%VSROOT_FILE%" >nul 2>&1

"%VSWHERE%" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath > "%VSROOT_FILE%"
if errorlevel 1 (
    echo FEHLER: vswhere konnte die Visual C++ Build Tools nicht ermitteln.
    if exist "%VSROOT_FILE%" del /q "%VSROOT_FILE%" >nul 2>&1
    pause
    exit /b 1
)

set "VSROOT="
set /p "VSROOT=" < "%VSROOT_FILE%"
del /q "%VSROOT_FILE%" >nul 2>&1

if not defined VSROOT (
    echo FEHLER: Visual C++ Build Tools wurden nicht gefunden.
    echo Bitte den Workload "Desktop development with C++" installieren.
    pause
    exit /b 1
)

set "VSDEVCMD=%VSROOT%\Common7\Tools\VsDevCmd.bat"
if not exist "%VSDEVCMD%" (
    echo FEHLER: VsDevCmd.bat wurde nicht gefunden: %VSDEVCMD%
    pause
    exit /b 1
)

call "%VSDEVCMD%" -no_logo -arch=x64 -host_arch=x64
if errorlevel 1 exit /b 1

rem ----------------------------------------------------------------------
rem Portable CMake: download only into .build, never install globally.
rem ----------------------------------------------------------------------
if not exist "%CMAKE_EXE%" (
    echo [1/4] Lade portables CMake %CMAKE_VERSION% in .build herunter ...
    powershell -NoProfile -ExecutionPolicy Bypass -Command ^
      "$ErrorActionPreference='Stop';" ^
      "$zip='%LOCAL%\%CMAKE_ZIP%';" ^
      "Invoke-WebRequest -UseBasicParsing -Uri '%CMAKE_URL%' -OutFile $zip;" ^
      "$tmp='%LOCAL%\cmake-extract'; Remove-Item $tmp -Recurse -Force -ErrorAction SilentlyContinue;" ^
      "Expand-Archive -Path $zip -DestinationPath $tmp -Force;" ^
      "$dir=Get-ChildItem $tmp -Directory | Select-Object -First 1;" ^
      "Remove-Item '%CMAKE_HOME%' -Recurse -Force -ErrorAction SilentlyContinue;" ^
      "Move-Item $dir.FullName '%CMAKE_HOME%';" ^
      "Remove-Item $tmp -Recurse -Force; Remove-Item $zip -Force"
    if errorlevel 1 (
        echo FEHLER beim Download/Entpacken von CMake.
        pause
        exit /b 1
    )
) else (
    echo [1/4] Lokales CMake bereits vorhanden.
)

rem ----------------------------------------------------------------------
rem Configure. FetchContent downloads wxWidgets + DCMTK into .build only.
rem ----------------------------------------------------------------------
echo [2/4] Konfiguriere C++ Projekt und lokale Abhaengigkeiten ...
"%CMAKE_EXE%" -S "%ROOT%" -B "%BUILD_DIR%" -G "Visual Studio 17 2022" -A x64
if errorlevel 1 (
    echo.
    echo FEHLER bei der CMake-Konfiguration.
    pause
    exit /b 1
)

rem ----------------------------------------------------------------------
rem Build static release.
rem ----------------------------------------------------------------------
echo [3/4] Baue native Release-EXE ...
"%CMAKE_EXE%" --build "%BUILD_DIR%" --config Release --target evi --parallel
if errorlevel 1 (
    echo.
    echo FEHLER beim C++ Build.
    pause
    exit /b 1
)

set "BUILT_EXE=%BUILD_DIR%\bin\Release\evi.exe"
if not exist "%BUILT_EXE%" set "BUILT_EXE=%BUILD_DIR%\bin\evi.exe"
if not exist "%BUILT_EXE%" (
    echo FEHLER: Build war erfolgreich, aber die EXE wurde nicht gefunden.
    pause
    exit /b 1
)

rem CMake POST_BUILD already copies the EXE to dist and creates the SHA-256 file.

echo [4/4] Fertig.
echo.
echo Portable EXE:
echo   %DIST_DIR%\evi.exe
echo   %DIST_DIR%\evi.exe.sha256
echo.
echo Auf dem Ziel-PC sind weder Java noch DCMTK/wxWidgets-DLLs erforderlich.
echo Die Projekt-Abhaengigkeiten liegen nur unter .build und koennen dort komplett geloescht werden.
echo.
pause
