@echo off
setlocal
cd /d "%~dp0"
if not exist "dist\evi.exe" (
    echo Die native EXE wurde noch nicht gebaut.
    echo Starte zuerst build_windows.bat.
    pause
    exit /b 1
)
start "" "dist\evi.exe"
