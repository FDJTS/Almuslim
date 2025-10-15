param(
  [string]$BuildDir = "build",
  [string]$Arch = "x64"
)
$ErrorActionPreference = 'Stop'

$Root = Split-Path -Parent $PSScriptRoot
$Cpp = Join-Path $Root 'cpp'
$Build = Join-Path $Cpp $BuildDir
$Dist = Join-Path $Root 'dist'
$Stage = Join-Path $Build 'almuslim-windows'

New-Item -ItemType Directory -Force -Path $Dist | Out-Null
if (Test-Path $Stage) { Remove-Item -Recurse -Force $Stage }
New-Item -ItemType Directory -Path $Stage | Out-Null

# Copy binary and data
Copy-Item -Force (Join-Path $Build 'al-muslim.exe') $Stage
if (Test-Path (Join-Path $Build 'data')) {
  Copy-Item -Recurse -Force (Join-Path $Build 'data') (Join-Path $Stage 'data')
}

# README
@"
Almuslim (Windows)
------------------

Run:
  .\al-muslim.exe

Optional flags:
  --setup             First-time setup wizard
  --ask               Choose a city at launch
  --week              Show next 7 days

Notes:
- Keep the 'data' folder next to the executable.
"@ | Set-Content -Path (Join-Path $Stage 'README.txt') -Encoding utf8

$Out = Join-Path $Dist ("almuslim-windows-$Arch.tar.gz")
Push-Location $Build
if (Test-Path $Out) { Remove-Item $Out -Force }
# Use built-in tar (BSY) available on Windows 10+
& tar -czf $Out 'almuslim-windows'
Pop-Location

Write-Host "Created: $Out"