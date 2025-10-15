param(
  [string]$BuildDir = "build",
  [string]$Config = "Release"
)

$ErrorActionPreference = "Stop"

$root = Split-Path -Parent $MyInvocation.MyCommand.Path
$cpp = Join-Path $root "..\cpp"
$build = Join-Path $cpp $BuildDir

$paths = @(
  (Join-Path $build "al-muslim.exe"),
  (Join-Path (Join-Path $build $Config) "al-muslim.exe"),
  (Join-Path (Join-Path $build "bin") "al-muslim.exe")
)
$exe = $paths | Where-Object { Test-Path $_ } | Select-Object -First 1

if (-not $exe) {
  Write-Error "Could not find al-muslim.exe. Build first with .\\build.ps1 -Release (or -Generator Ninja), then run again."
  exit 1
}

& $exe @args
