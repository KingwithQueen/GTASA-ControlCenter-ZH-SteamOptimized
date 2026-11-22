[CmdletBinding()]
param(
    [string] $OutputDirectory
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

function Get-VisualStudioMSBuild {
    $installer = Join-Path ([Environment]::GetFolderPath('ProgramFilesX86')) 'Microsoft Visual Studio\Installer\vswhere.exe'
    if (-not (Test-Path -LiteralPath $installer -PathType Leaf)) {
        throw '找不到 vswhere.exe。请在安装了 Visual Studio 2026 和 C++ 桌面开发工具的 Windows 上运行。'
    }

    $searchVsArguments = @('-latest', '-products', '*', '-version', '[18.0,19.0)',
        '-requires', 'Microsoft.Component.MSBuild', 'Microsoft.VisualStudio.Component.VC.Tools.x86.x64',
        '-property', 'installationPath')
    $installation = @(& $installer @searchVsArguments)
    if ($LASTEXITCODE -ne 0 -or $installation.Count -ne 1) {
        throw '找不到同时具备 MSBuild 和 Win32 C++ 工具的 Visual Studio 2026。'
    }

    $preferred = Join-Path $installation[0] 'MSBuild\Current\Bin\amd64\MSBuild.exe'
    if (Test-Path -LiteralPath $preferred -PathType Leaf) {
        return $preferred
    }
    $fallback = Join-Path $installation[0] 'MSBuild\Current\Bin\MSBuild.exe'
    if (-not (Test-Path -LiteralPath $fallback -PathType Leaf)) {
        throw "找不到 MSBuild.exe。Visual Studio 安装路径为 $($installation[0])。"
    }
    return $fallback
}

function Confirm-Win32Image([string] $Path, [bool] $IsDll) {
    $file = [IO.File]::OpenRead($Path)
    try {
        $reader = [IO.BinaryReader]::new($file)
        try {
            if ($file.Length -lt 64 -or $reader.ReadUInt16() -ne 0x5A4D) {
                throw "无效的 PE 文件 $Path"
            }
            $file.Position = 0x3C
            $peHeader = $reader.ReadInt32()
            if ($peHeader -lt 64 -or $peHeader -gt ($file.Length - 24)) {
                throw "无效的 PE 文件头 $Path"
            }
            $file.Position = $peHeader
            if ($reader.ReadUInt32() -ne 0x00004550 -or $reader.ReadUInt16() -ne 0x014C) {
                throw "需要 Win32 x86 产物 $Path"
            }
            $file.Position = $peHeader + 22
            $dll = ($reader.ReadUInt16() -band 0x2000) -ne 0
            if ($dll -ne $IsDll) {
                throw "PE 文件类型错误 $Path"
            }
        } finally {
            $reader.Dispose()
        }
    } finally {
        $file.Dispose()
    }
}

$repo = Split-Path -Parent $PSScriptRoot
$solution = Join-Path $PSScriptRoot 'GTASAControlCenterUI.sln'
$release = Join-Path $repo '.work\win32-ui\Release'
$guide = Join-Path $PSScriptRoot 'STEAM-ACCEPTANCE.md'
$version = & (Join-Path $PSScriptRoot 'version.ps1')
if ([string]::IsNullOrWhiteSpace($OutputDirectory)) {
    $stamp = Get-Date -Format 'yyyyMMdd-HHmmss-fff'
    $OutputDirectory = Join-Path $repo ".work\win32-ui\steam-v$version-$stamp"
} elseif (-not [IO.Path]::IsPathRooted($OutputDirectory)) {
    $OutputDirectory = Join-Path (Get-Location).ProviderPath $OutputDirectory
}
$OutputDirectory = [IO.Path]::GetFullPath($OutputDirectory)
$separators = @([IO.Path]::DirectorySeparatorChar, [IO.Path]::AltDirectorySeparatorChar)
$outputRoot = [IO.Path]::GetPathRoot($OutputDirectory).TrimEnd($separators)
$OutputDirectory = $OutputDirectory.TrimEnd($separators)
if ($OutputDirectory -eq $outputRoot) {
    throw '请在磁盘或共享目录下指定一个独立的输出子目录。'
}
$replaceEmpty = $false
if (Test-Path -LiteralPath $OutputDirectory) {
    if (-not (Test-Path -LiteralPath $OutputDirectory -PathType Container) -or
        ((Get-Item -LiteralPath $OutputDirectory -Force).Attributes -band [IO.FileAttributes]::ReparsePoint) -or
        @(Get-ChildItem -LiteralPath $OutputDirectory -Force).Count -ne 0) {
        throw "输出路径必须是新目录或空目录。路径为 $OutputDirectory。"
    }
    $replaceEmpty = $true
}

$msbuild = Get-VisualStudioMSBuild
& $msbuild $solution /m /t:Build /p:Configuration=Release /p:Platform=Win32 /nologo /v:minimal
if ($LASTEXITCODE -ne 0) {
    throw "Win32 Release 构建失败，MSBuild 退出代码为 $LASTEXITCODE。"
}

$exeName = 'GTASAControlCenterUI.exe'
$dllName = 'GTASASteam.dll'
$guideName = Split-Path -Leaf $guide
$exe = Join-Path $release $exeName
$dll = Join-Path $release $dllName
foreach ($path in @($exe, $dll, $guide)) {
    if (-not (Test-Path -LiteralPath $path -PathType Leaf)) {
        throw "打包所需文件不存在。路径为 $path。"
    }
}
Confirm-Win32Image $exe $false
Confirm-Win32Image $dll $true
foreach ($image in @($exe, $dll)) {
    $versionInfo = [Diagnostics.FileVersionInfo]::GetVersionInfo($image)
    if ($versionInfo.FileVersion -cne $version -or $versionInfo.ProductVersion -cne $version) {
        throw "文件版本与源码中的 $version 不符。路径为 $image。"
    }
}

$packageName = "GTASAControlCenter-Steam-v$version-Win32"
$parent = [IO.Path]::GetDirectoryName($OutputDirectory)
$stage = Join-Path $parent ('.gtasa-steam-preview-' + [Guid]::NewGuid().ToString('N'))
$package = Join-Path $stage $packageName
$zip = Join-Path $stage "$packageName.zip"
$checksums = Join-Path $stage 'SHA256SUMS.txt'
$names = @($exeName, $dllName, $guideName)
[void][IO.Directory]::CreateDirectory($package)
try {
    Copy-Item -LiteralPath @($exe, $dll, $guide) -Destination $package
    Compress-Archive -LiteralPath $package -DestinationPath $zip -CompressionLevel Optimal

    Add-Type -AssemblyName System.IO.Compression.FileSystem
    $archive = [IO.Compression.ZipFile]::OpenRead($zip)
    try {
        $files = @($archive.Entries | Where-Object { $_.Name })
        if ($files.Count -ne $names.Count) {
            throw 'ZIP 所含文件数有误。'
        }
        foreach ($name in $names) {
            $matching = @($files | Where-Object { $_.FullName.Replace('\', '/') -ceq "$packageName/$name" })
            if ($matching.Count -ne 1) {
                throw "ZIP 缺少文件或文件重复 $name"
            }
            $stream = $matching[0].Open()
            try {
                $packedHash = (Get-FileHash -InputStream $stream -Algorithm SHA256).Hash
                $looseHash = (Get-FileHash -LiteralPath (Join-Path $package $name) -Algorithm SHA256).Hash
                if ($packedHash -ne $looseHash) {
                    throw "ZIP 内文件校验失败 $name"
                }
            } finally {
                $stream.Dispose()
            }
        }
    } finally {
        $archive.Dispose()
    }

    $results = foreach ($name in @("$packageName.zip", "$packageName/$exeName",
                                 "$packageName/$dllName", "$packageName/$guideName")) {
        $hash = (Get-FileHash -LiteralPath (Join-Path $stage $name) -Algorithm SHA256).Hash.ToLowerInvariant()
        '{0}  {1}' -f $hash, $name
    }
    [IO.File]::WriteAllLines($checksums, [string[]]$results, [Text.UTF8Encoding]::new($false))

    if ($replaceEmpty) {
        $existing = Get-Item -LiteralPath $OutputDirectory -Force
        if (-not $existing.PSIsContainer -or ($existing.Attributes -band [IO.FileAttributes]::ReparsePoint)) {
            throw "输出路径已改变。路径为 $OutputDirectory。"
        }
        [IO.Directory]::Delete($OutputDirectory, $false)
    }
    try {
        [IO.Directory]::Move($stage, $OutputDirectory)
    } catch {
        if ($replaceEmpty -and -not (Test-Path -LiteralPath $OutputDirectory)) {
            [void][IO.Directory]::CreateDirectory($OutputDirectory)
        }
        throw
    }
} finally {
    if ([IO.Directory]::Exists($stage)) {
        try {
            foreach ($name in $names) { [IO.File]::Delete((Join-Path $package $name)) }
            [IO.File]::Delete($zip)
            [IO.File]::Delete($checksums)
            [IO.Directory]::Delete($package, $false)
            [IO.Directory]::Delete($stage, $false)
        } catch {
            Write-Warning "无法清理临时打包目录 $stage"
        }
    }
}

[pscustomobject]@{
    Version = $version
    PackageDirectory = Join-Path $OutputDirectory $packageName
    Zip = Join-Path $OutputDirectory "$packageName.zip"
    SHA256 = Join-Path $OutputDirectory 'SHA256SUMS.txt'
}
