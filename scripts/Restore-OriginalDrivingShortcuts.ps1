param([string]$ConfigPath = (Join-Path $PSScriptRoot 'GTASAConfig.dat'))
$ErrorActionPreference = 'Stop'
$resolvedConfig = (Resolve-Path -LiteralPath $ConfigPath).Path
$shortcutIds = @(
 'B1B9B667-1128-4541-A46A-6778D91F67F1', '6642562D-466F-4E4F-98BB-D3F906AE62CE',
 '26768196-90E3-3B4B-91AB-26602EEF28D0', 'BBFE98CF-8CCD-E042-8E6E-239D3D2CB090',
 'EFC3D760-4F17-7246-9987-21F7E22BCA95', '94217325-2947-C745-B7EA-D89BE18EDA42',
 'E3CDFAB5-FEDE-1B4E-9332-31EE03809FE6', '1F68A800-B956-F147-98FB-EB1266D6E464',
 '6D49DA7F-3569-1C4A-AE68-F78F36E01F84', '9D8BD593-C499-5D43-A4AB-98A61AE8F649',
 'AB540E1C-EF06-5643-8B27-EECC5F66B668', 'D4A60CD3-32E9-054A-8499-0D03470095D8',
 '56492D5A-56A7-E749-8F9F-467E5E3FAB3F', '41E96219-C910-C640-8835-682CADAB0FA1'
)
# Latin-1 is a lossless byte mapping here; do not reinterpret localized text.
$byteEncoding = [Text.Encoding]::GetEncoding(28591)
$originalBytes = [IO.File]::ReadAllBytes($resolvedConfig)
$contents = $byteEncoding.GetString($originalBytes)
$restoredCount = 0
foreach ($shortcutId in $shortcutIds) {
    $pattern = '(?m)^(' + [regex]::Escape($shortcutId) + '\|[^\r\n|]*\|[^\r\n|]*\|)0(?=\|)'
    $matches = [regex]::Matches($contents, $pattern)
    $restoredCount += $matches.Count
    $contents = [regex]::Replace($contents, $pattern, '${1}1')
}
if ($restoredCount -eq 0) {
    Write-Output 'No disabled original driving shortcuts found; file unchanged.'
    exit 0
}
$backupPath = $resolvedConfig + '.before-shortcuts-' + (Get-Date -Format 'yyyyMMdd-HHmmssfff') + '.bak'
$pendingPath = $resolvedConfig + '.restoring-' + [Guid]::NewGuid().ToString('N')
[IO.File]::Copy($resolvedConfig, $backupPath, $false)
try {
    $updatedBytes = $byteEncoding.GetBytes($contents)
    [IO.File]::WriteAllBytes($pendingPath, $updatedBytes)
    [IO.File]::Replace($pendingPath, $resolvedConfig, [NullString]::Value)
    if ($byteEncoding.GetString([IO.File]::ReadAllBytes($resolvedConfig)) -cne $contents) { throw 'Readback verification failed.' }
    Write-Output "Restored $restoredCount shortcuts. Backup: $backupPath"
} finally {
    if (Test-Path -LiteralPath $pendingPath) { Remove-Item -LiteralPath $pendingPath }
}
