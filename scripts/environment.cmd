@echo off
rem This environment affects only the caller and its children.
set "LEGACY_SOURCE_ROOT=%~dp0.."
set "TEMP=%~dp0..\.work\temp"
set "TMP=%TEMP%"
set "PYTHONDONTWRITEBYTECODE=1"
set "PIP_CACHE_DIR=%~dp0..\.work\pip"
set "npm_config_cache=%~dp0..\.work\npm"
set "WEBVIEW2_USER_DATA_FOLDER=%~dp0..\.work\webview"
if not exist "%TEMP%\" mkdir "%TEMP%"
if not exist "%TEMP%\" exit /b 1
exit /b 0
