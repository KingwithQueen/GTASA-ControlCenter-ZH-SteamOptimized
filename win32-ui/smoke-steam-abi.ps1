param(
    [string] $ProductionDll
)

$ErrorActionPreference = 'Stop'
$root = (Resolve-Path -LiteralPath (Join-Path $PSScriptRoot '..')).Path
$output = Join-Path $root ('.work\win32-ui\steam-abi-' + [Guid]::NewGuid().ToString('N'))
[void](New-Item -ItemType Directory -Path $output)

$vswhere = Join-Path ([Environment]::GetEnvironmentVariable('ProgramFiles(x86)')) 'Microsoft Visual Studio\Installer\vswhere.exe'
$install = & $vswhere -latest -products '*' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
if ($LASTEXITCODE -ne 0 -or -not $install) { throw 'The MSVC x86 toolset was not found.' }
$vcvars = Join-Path $install 'VC\Auxiliary\Build\vcvars32.bat'

$releaseDef = Join-Path $root 'native\win32-ui-exports.def'
$fixtureDef = Join-Path $output 'offline-fixture.def'
$def = (Get-Content -LiteralPath $releaseDef -Raw).Replace('LIBRARY GTASASteam', 'LIBRARY OfflineUiFixture')
$def += @"

 SteamFixtureBegin=_SteamFixtureBegin@0
 SteamFixtureRun=_SteamFixtureRun@0
 SteamFixtureVehicle=_SteamFixtureVehicle@4
 SteamFixtureCoffeeSupported=_SteamFixtureCoffeeSupported@4
 SteamFixtureDisturbGarage=_SteamFixtureDisturbGarage@0
 SteamFixtureTrain=_SteamFixtureTrain@0
"@
[IO.File]::WriteAllText($fixtureDef, $def, [Text.Encoding]::ASCII)

function Build-Backend([string] $Name, [string] $ExportFile, [string] $Defines) {
    $source = Join-Path $root 'native\SteamMemory.cpp'
    $compile = 'call "{0}" >nul && pushd "{1}" && cl /nologo /W4 /O2 /MT /std:c++17 /EHsc /LD {2} "{3}" /Fo"{4}.obj" /Fe"{4}.dll" /link /DEF:"{5}" /INCREMENTAL:NO' -f $vcvars,$output,$Defines,$source,$Name,$ExportFile
    & cmd.exe /d /c $compile
    if ($LASTEXITCODE -ne 0) { throw "Building $Name exited with code $LASTEXITCODE." }
}

$previousRoot = $env:LEGACY_SOURCE_ROOT
$previousOutput = $env:LEGACY_BUILD_OUTPUT
$previousTools = $env:VCVARS32_PATH
try {
    $env:LEGACY_SOURCE_ROOT = $root
    $env:LEGACY_BUILD_OUTPUT = $output
    $env:VCVARS32_PATH = $vcvars
    if (-not $ProductionDll) {
        & (Join-Path $root 'scripts\build-native.cmd')
        if ($LASTEXITCODE -ne 0) { throw "The existing native build exited with code $LASTEXITCODE." }
    }
} finally {
    $env:LEGACY_SOURCE_ROOT = $previousRoot
    $env:LEGACY_BUILD_OUTPUT = $previousOutput
    $env:VCVARS32_PATH = $previousTools
}

if ($ProductionDll) {
    $release = (Resolve-Path -LiteralPath $ProductionDll).Path
} else {
    Build-Backend 'GTASASteam' $releaseDef '/DWIN32_UI_BACKEND'
    $release = Join-Path $output 'GTASASteam.dll'
}
Build-Backend 'OfflineUiFixture' $fixtureDef '/DWIN32_UI_BACKEND /DOFFLINE_UI_FIXTURE'

$source = Join-Path $PSScriptRoot 'steam_abi_test.c'
$object = Join-Path $output 'steam_abi_test.obj'
$test = Join-Path $output 'steam_abi_test.exe'
$compile = 'call "{0}" >nul && cl /nologo /TC /W4 /WX /O2 /MT /DUNICODE /D_UNICODE /Fo"{1}" /Fe"{2}" "{3}" /link /INCREMENTAL:NO' -f $vcvars,$object,$test,$source
& cmd.exe /d /c $compile
if ($LASTEXITCODE -ne 0) { throw "Building the x86 ABI test exited with code $LASTEXITCODE." }

& $test $release (Join-Path $output 'OfflineUiFixture.dll')
if ($LASTEXITCODE -ne 0) { throw "The x86 ABI test exited with code $LASTEXITCODE. Outputs: $output" }

$adapterOutput = Join-Path $output 'adapter'
$targetOutput = Join-Path $output 'wrong-target'
$missingOutput = Join-Path $output 'without-sidecar'
[void](New-Item -ItemType Directory -Path $adapterOutput,$targetOutput,$missingOutput)
$adapterSource = Join-Path $PSScriptRoot 'trainer_session_test.c'
$trainerSource = Join-Path $PSScriptRoot 'trainer.c'
$adapterExe = Join-Path $adapterOutput 'trainer_session_test.exe'
$targetExe = Join-Path $targetOutput 'gta_sa.exe'
$missingExe = Join-Path $missingOutput 'trainer_session_test.exe'
$compile = 'call "{0}" >nul && pushd "{1}" && cl /nologo /TC /W4 /WX /utf-8 /O2 /MT /DUNICODE /D_UNICODE /DWINVER=0x0501 /D_WIN32_WINNT=0x0501 /Fe"{2}" "{3}" "{4}" /link user32.lib /INCREMENTAL:NO' -f $vcvars,$adapterOutput,$adapterExe,$adapterSource,$trainerSource
& cmd.exe /d /c $compile
if ($LASTEXITCODE -ne 0) { throw "Building the adapter session test exited with code $LASTEXITCODE." }

Copy-Item -LiteralPath $release -Destination (Join-Path $adapterOutput 'GTASASteam.dll')
Copy-Item -LiteralPath $adapterExe -Destination $targetExe
Copy-Item -LiteralPath $adapterExe -Destination $missingExe
& $adapterExe $targetExe
if ($LASTEXITCODE -ne 0) { throw "The adapter session test exited with code $LASTEXITCODE. Outputs: $output" }
& $missingExe --without-backend
if ($LASTEXITCODE -ne 0) { throw "The missing-backend session test exited with code $LASTEXITCODE. Outputs: $output" }

"Test outputs: $output"
