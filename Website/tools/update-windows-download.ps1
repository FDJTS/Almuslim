param(
  [Parameter(Mandatory=$true)][string]$FilePath,
  [string]$TargetName,
  [string]$WebsiteDir = (Join-Path $PSScriptRoot '..'),
  [string]$ConfigFile = (Join-Path $PSScriptRoot '..' 'config.json')
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

if(-not (Test-Path -LiteralPath $FilePath)){
  throw "Input file not found: $FilePath"
}

$downloadsDir = Join-Path $WebsiteDir 'downloads'
if(-not (Test-Path -LiteralPath $downloadsDir)){
  New-Item -ItemType Directory -Path $downloadsDir | Out-Null
}

$src = (Resolve-Path -LiteralPath $FilePath).Path
$name = if([string]::IsNullOrWhiteSpace($TargetName)) { [IO.Path]::GetFileName($src) } else { $TargetName }
$dest = Join-Path $downloadsDir $name
Copy-Item -LiteralPath $src -Destination $dest -Force

# Compute SHA256
$hash = (Get-FileHash -LiteralPath $dest -Algorithm SHA256).Hash.ToLower()

# Update config.json
if(-not (Test-Path -LiteralPath $ConfigFile)){
  throw "Config file not found: $ConfigFile"
}

$cfgRaw = Get-Content -LiteralPath $ConfigFile -Raw -Encoding UTF8
$cfg = $cfgRaw | ConvertFrom-Json -Depth 100
if(-not $cfg.downloads){ $cfg | Add-Member -Name downloads -MemberType NoteProperty -Value (@{}) }
if(-not $cfg.downloads.windows){ $cfg.downloads | Add-Member -Name windows -MemberType NoteProperty -Value (@{}) }

$rel = "./downloads/$name"
$cfg.downloads.windows.url = $rel
$cfg.downloads.windows.label = $name
$cfg.downloads.windows.checksum = $hash

$cfg | ConvertTo-Json -Depth 100 | Out-File -FilePath $ConfigFile -Encoding UTF8

Write-Host "Copied to: $rel"
Write-Host "SHA256:  $hash"
Write-Host "config.json updated."
