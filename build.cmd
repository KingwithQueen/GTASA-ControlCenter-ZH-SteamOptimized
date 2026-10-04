@echo off
setlocal
call "%~dp0scripts\environment.cmd"
if errorlevel 1 exit /b %errorlevel%
python -B "%~dp0scripts\build.py" %*
exit /b %errorlevel%
