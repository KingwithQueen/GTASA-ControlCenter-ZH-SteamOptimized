@echo off
setlocal
powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%~dp0Restore-OriginalDrivingShortcuts.ps1" -ConfigPath "%~dp0GTASAConfig.dat"
if errorlevel 1 echo Restore failed. The original backup is retained.
pause
