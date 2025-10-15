param(
  [string]$BuildDir = "..\cpp\build-mingw",
  [string]$Version = "0.1.0",
  [string]$IsccPath = ""
)

$ErrorActionPreference = "Stop"

$scriptRoot = Split-Path -Parent $MyInvocation.MyCommand.Path
$installer = Join-Path $scriptRoot "installer.iss"

if (-not (Test-Path $installer)) { throw "installer.iss not found at $installer" }

# Resolve ISCC.exe (Inno Setup)
function Get-ISCC {
  param([string]$Hint)
  if ($Hint -and (Test-Path $Hint)) { return $Hint }
  $c = Get-Command iscc.exe -ErrorAction SilentlyContinue
  if ($c) { return $c.Source }
  $paths = @(
    "$env:ProgramFiles(x86)\Inno Setup 6\ISCC.exe",
    "$env:ProgramFiles\Inno Setup 6\ISCC.exe"
  )
  foreach($p in $paths){ if (Test-Path $p){ return $p } }
  return $null
}

$iscc = Get-ISCC -Hint $IsccPath
if (-not $iscc) {
  Write-Error "Inno Setup (ISCC.exe) not found. Install from https://jrsoftware.org/isdl.php and ensure ISCC.exe is in PATH."
  exit 1
}

# Stage app folder
$buildAbs = Resolve-Path (Join-Path $scriptRoot $BuildDir) | Select-Object -ExpandProperty Path
if (-not (Test-Path (Join-Path $buildAbs "al-muslim.exe"))) { throw "al-muslim.exe not found in $buildAbs. Build the app first." }

$stage = Join-Path $scriptRoot "_stage"
if (Test-Path $stage) { Remove-Item -Recurse -Force $stage }
New-Item -ItemType Directory -Path $stage | Out-Null

# Copy exe and data
Copy-Item (Join-Path $buildAbs "al-muslim.exe") (Join-Path $stage "al-muslim.exe") -Force
if (Test-Path (Join-Path $buildAbs "data")) {
  Copy-Item (Join-Path $buildAbs "data") (Join-Path $stage "data") -Recurse -Force
}

# Copy app icon (.ico) so shortcuts can use it
$icoSource = Join-Path $scriptRoot "..\..\App\Assets\logo\al-muslim.ico"
if (Test-Path $icoSource) {
  Copy-Item $icoSource (Join-Path $stage "al-muslim.ico") -Force
}

# Compile installer
Push-Location $scriptRoot
try {
  & "$iscc" "/DAppSource=$stage" "/DAppVersion=$Version" "installer.iss"
  if ($LASTEXITCODE -ne 0) { throw "ISCC failed with exit code $LASTEXITCODE" }
  Write-Host "Installer created in: $scriptRoot" -ForegroundColor Green
} finally {
  Pop-Location
}
