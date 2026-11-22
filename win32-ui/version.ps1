[CmdletBinding()]
param([switch] $Bump)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

Add-Type -AssemblyName System.Numerics
$path = Join-Path $PSScriptRoot 'version.h'
$source = [IO.File]::ReadAllText($path)
$pattern = '(?m)^(?<prefix>#define[ \t]+GTASA_VERSION_(?<part>MAJOR|MINOR|PATCH|WIN32_MAJOR)[ \t]+)(?<number>0|[1-9][0-9]*)(?=\r?$)'
$parts = @{}
foreach ($match in [regex]::Matches($source, $pattern)) {
    $part = $match.Groups['part'].Value
    if ($parts.ContainsKey($part)) { throw "Duplicate version component $part in $path" }
    $parts[$part] = $match.Groups['number'].Value
}
if ($parts.Count -ne 4 -or $parts.MINOR.Length -ne 1 -or $parts.PATCH.Length -ne 1) {
    throw "Expected three version components and a Win32 major component in $path"
}

$culture = [Globalization.CultureInfo]::InvariantCulture
$major = [Numerics.BigInteger]::Parse($parts.MAJOR, $culture)
$minor = [int]$parts.MINOR
$patch = [int]$parts.PATCH
if ($Bump) {
    $patch = ($patch + 1) % 10
    if ($patch -eq 0) {
        $minor = ($minor + 1) % 10
        if ($minor -eq 0) { $major += [Numerics.BigInteger]::One }
    }
}

$fixedMajor = if ($major -le [uint16]::MaxValue) { $major.ToString($culture) } else { '0' }
if ($Bump) {
    $parts.MAJOR = $major.ToString($culture)
    $parts.MINOR = $minor.ToString($culture)
    $parts.PATCH = $patch.ToString($culture)
    $parts.WIN32_MAJOR = $fixedMajor
    $source = [regex]::Replace($source, $pattern, {
        param($match)
        $match.Groups['prefix'].Value + $parts[$match.Groups['part'].Value]
    })
    [IO.File]::WriteAllText($path, $source, [Text.UTF8Encoding]::new($false))
} elseif ($fixedMajor -cne $parts.WIN32_MAJOR) {
    throw "The Win32 major component in $path does not match the product version."
}

'{0}.{1}.{2}' -f $parts.MAJOR, $parts.MINOR, $parts.PATCH
