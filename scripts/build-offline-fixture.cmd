@echo off
setlocal
if not defined LEGACY_SOURCE_ROOT exit /b 2
if not defined LEGACY_BUILD_OUTPUT exit /b 2
set "VSWHERE_EXE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
for /f "usebackq tokens=*" %%I in (`"%VSWHERE_EXE%" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do set "VCVARS32_PATH=%%I\VC\Auxiliary\Build\vcvars32.bat"
if not defined VCVARS32_PATH exit /b 3
call "%VCVARS32_PATH%" >nul
if errorlevel 1 exit /b 3
pushd "%LEGACY_BUILD_OUTPUT%"
cl /nologo /W4 /O2 /MT /std:c++17 /EHsc /LD /DOFFLINE_UI_FIXTURE "%LEGACY_SOURCE_ROOT%\native\SteamMemory.cpp" /FoOfflineUiFixture.obj /FeOfflineUiFixture.dll /link /DEF:offline-fixture.def /INCREMENTAL:NO
set "FIXTURE_RESULT=%ERRORLEVEL%"
popd
exit /b %FIXTURE_RESULT%
