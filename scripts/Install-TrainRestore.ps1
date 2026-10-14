[CmdletBinding()]
param([string]$GameDirectory)
$ErrorActionPreference = 'Stop'
if (-not $GameDirectory) { $GameDirectory = Read-Host '请输入含 gta-sa.exe 的游戏目录' }
$gameRoot = (Resolve-Path -LiteralPath $GameDirectory).Path
$exe = Join-Path $gameRoot 'gta-sa.exe'
if (-not (Test-Path -LiteralPath $exe -PathType Leaf)) { throw '所选目录中找不到 gta-sa.exe。' }
if ((Get-FileHash -LiteralPath $exe -Algorithm SHA256).Hash.ToLowerInvariant() -ne '15e3cfedba9a841df67d8194e7249afb493b0e10d6138fb8ebab2c136e543efb') {
    throw '游戏版本不匹配：此组件仅支持已核对的经典 Steam 版本。'
}
if (Get-Process -Name 'gta-sa','gtasa' -ErrorAction SilentlyContinue) { throw '请完全退出游戏后再安装。' }
$name = 'GTASA_TrainRestore_Steam.asi'
$source = Join-Path $PSScriptRoot $name
$manifest = Join-Path $PSScriptRoot 'FILES.sha256'
$record = @(Get-Content -LiteralPath $manifest -Encoding UTF8 | Where-Object { $_ -match ('^[0-9a-f]{64}  ' + [regex]::Escape($name) + '$') })
if ($record.Count -ne 1 -or (Get-FileHash -LiteralPath $source -Algorithm SHA256).Hash.ToLowerInvariant() -ne $record[0].Substring(0,64)) {
    throw '组件文件校验失败，请重新解压候选包。'
}
$target = Join-Path $gameRoot $name
if (Test-Path -LiteralPath $target) {
    if ((Get-FileHash -LiteralPath $target -Algorithm SHA256).Hash -ne (Get-FileHash -LiteralPath $source -Algorithm SHA256).Hash) {
        throw '游戏目录已有不同版本的同名组件，已保留原文件；请先将它另存备份。'
    }
} else { Copy-Item -LiteralPath $source -Destination $target }
if ((Get-FileHash -LiteralPath $target -Algorithm SHA256).Hash -ne (Get-FileHash -LiteralPath $source -Algorithm SHA256).Hash) { throw '安装后校验失败。' }
Write-Host '组件已安装。游戏需要已有 ASI 加载器；重启游戏后，修改器会确认组件是否已加载。'
Write-Host '移除组件前，必须在组件仍启用时清空所有车库的火车记录并重新保存游戏。'
