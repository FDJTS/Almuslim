param(
  [string]$Svg = "al-muslim-symbol.svg",
  [string]$Out = "al-muslim.ico"
)

$ErrorActionPreference = "Stop"
$here = Split-Path -Parent $MyInvocation.MyCommand.Path
Push-Location $here
try {
  function Get-IM() {
    $m = Get-Command magick -ErrorAction SilentlyContinue
    if ($m) { return @{ Tool="magick"; Path=$m.Source } }
    $c = Get-Command convert -ErrorAction SilentlyContinue
    if ($c) { return @{ Tool="convert"; Path=$c.Source } }
    return $null
  }

  $im = Get-IM
  if (-not $im) {
    Write-Error "ImageMagick not found. Install from https://imagemagick.org (ensure 'magick' in PATH)."
  }

  if (-not (Test-Path $Svg)) { throw "SVG not found: $Svg" }

  $tmp = Join-Path $env:TEMP ("almuslim-ico-" + [guid]::NewGuid().ToString())
  New-Item -ItemType Directory -Path $tmp | Out-Null
  $sizes = @(16,24,32,48,64,128,256)
  $pngs = @()
  foreach ($s in $sizes) {
    $png = Join-Path $tmp ("icon-${s}.png")
    if ($im.Tool -eq 'magick') {
      & $im.Path convert -background none $Svg -resize ${s}x${s} $png
    } else {
      & $im.Path -background none $Svg -resize ${s}x${s} $png
    }
    if ($LASTEXITCODE -ne 0) { throw "Rasterize failed at size $s" }
    $pngs += $png
  }

  if ($im.Tool -eq 'magick') {
    & $im.Path $pngs $Out
  } else {
    & $im.Path $pngs $Out
  }
  if ($LASTEXITCODE -ne 0) { throw "ICO creation failed" }
  Write-Host "Wrote $Out" -ForegroundColor Green
} finally {
  Pop-Location
}
