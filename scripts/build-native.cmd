@echo off
setlocal
if not defined LEGACY_SOURCE_ROOT exit /b 2
if not defined LEGACY_BUILD_OUTPUT exit /b 2
if not defined VCVARS32_PATH (
    if not defined VSWHERE_EXE set "VSWHERE_EXE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
)
if not defined VCVARS32_PATH (
    if not exist "%VSWHERE_EXE%" exit /b 3
    for /f "usebackq tokens=*" %%I in (`"%VSWHERE_EXE%" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do set "VCVARS32_PATH=%%I\VC\Auxiliary\Build\vcvars32.bat"
)
if not defined VCVARS32_PATH exit /b 3
call "%VCVARS32_PATH%" >nul
if errorlevel 1 exit /b 3
if not exist "%LEGACY_BUILD_OUTPUT%\native\" mkdir "%LEGACY_BUILD_OUTPUT%\native"
pushd "%LEGACY_BUILD_OUTPUT%\native"
cl /nologo /W4 /O2 /MT /std:c++17 /EHsc /DUNIT_TEST "%LEGACY_SOURCE_ROOT%\native\SteamMemory.cpp" /Fotests.obj /Febackend-tests.exe /link /INCREMENTAL:NO
if errorlevel 1 goto failed
backend-tests.exe
if errorlevel 1 goto failed
cl /nologo /W4 /O2 /MT /std:c++17 /EHsc /LD "%LEGACY_SOURCE_ROOT%\native\SteamMemory.cpp" /FoSteamMemory.obj /FeGTASASteam.dll /link /DEF:"%LEGACY_SOURCE_ROOT%\native\exports.def" /INCREMENTAL:NO
if errorlevel 1 goto failed
popd
exit /b 0
:failed
popd
exit /b 1
