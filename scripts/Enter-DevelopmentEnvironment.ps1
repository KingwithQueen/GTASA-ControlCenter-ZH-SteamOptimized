$legacyDevelopmentRoot = Split-Path -Parent $PSScriptRoot
$legacyDevelopmentTemp = Join-Path $legacyDevelopmentRoot '.work/temp'
New-Item -ItemType Directory -Force -Path $legacyDevelopmentTemp | Out-Null
$env:TEMP = $legacyDevelopmentTemp
$env:TMP = $legacyDevelopmentTemp
$env:PYTHONDONTWRITEBYTECODE = '1'
$env:PIP_CACHE_DIR = Join-Path $legacyDevelopmentRoot '.work/pip'
$env:npm_config_cache = Join-Path $legacyDevelopmentRoot '.work/npm'
