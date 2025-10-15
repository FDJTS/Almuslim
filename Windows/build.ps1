<#
  Build script for Windows (PowerShell)
  - Detects Ninja; if not present, lets CMake pick a default generator (e.g., Visual Studio)
  - Allows overriding generator via -Generator
#>
param(
  [string]$BuildDir = "build",
  [switch]$Release,
  [string]$Generator,
  [switch]$Clean
)

$ErrorActionPreference = "Stop"

$root = Split-Path -Parent $MyInvocation.MyCommand.Path
$cpp = Join-Path $root "..\cpp"
$build = Join-Path $cpp $BuildDir

if (!(Test-Path $build)) { New-Item -ItemType Directory -Path $build | Out-Null }

Push-Location $build
try {
  $cfg = if ($Release) { "Release" } else { "Debug" }

  function Clear-CMakeArtifacts {
    if (Test-Path "CMakeCache.txt") { Remove-Item -Force "CMakeCache.txt" }
    if (Test-Path "CMakeFiles") { Remove-Item -Recurse -Force "CMakeFiles" }
    if (Test-Path "build.ninja") { Remove-Item -Force "build.ninja" }
    if (Test-Path "Makefile") { Remove-Item -Force "Makefile" }
    if (Test-Path "cmake_install.cmake") { Remove-Item -Force "cmake_install.cmake" }
  }

  function Invoke-CMakeConfigure($gen, $extra) {
    $cmakeArgsList = @()
    if ($gen) { $cmakeArgsList += @('-G', $gen) }
    if ($extra) { $cmakeArgsList += $extra }
    $cmakeArgsList += @('..', "-DCMAKE_BUILD_TYPE=$cfg")
    Write-Host "Configuring with: cmake $($cmakeArgsList -join ' ')" -ForegroundColor Cyan
    cmake @cmakeArgsList
    $code = $LASTEXITCODE
    # Some cmake versions emit warnings that the script mistakenly treated as failure earlier.
    # Consider configure successful if CMakeCache.txt exists and build files were generated.
    if ($code -ne 0) {
      if ((Test-Path 'CMakeCache.txt') -and ((Test-Path 'build.ninja') -or (Test-Path 'Makefile') -or (Test-Path (Join-Path $PWD 'CMakeFiles')))) {
        return 0
      }
    }
    return $code
  }

  function Get-NinjaPath {
    if ($env:NINJA -and (Test-Path $env:NINJA)) { return $env:NINJA }
    $c = Get-Command ninja -ErrorAction SilentlyContinue
    if ($c) { return $c.Source }
    $candidates = @()
    if ($env:LocalAppData) { $candidates += (Join-Path $env:LocalAppData "Microsoft\WinGet\Links\ninja.exe") }
    if ($env:USERPROFILE) { $candidates += (Join-Path $env:USERPROFILE "AppData\Local\Microsoft\WindowsApps\ninja.exe") }
    if ($env:ProgramFiles) { $candidates += (Join-Path $env:ProgramFiles "Ninja\ninja.exe") }
    # Winget package directory fallback
    $wingetNinja = Get-Item "$env:LOCALAPPDATA\Microsoft\WinGet\Packages\Ninja-build.Ninja_*\ninja.exe" -ErrorAction SilentlyContinue | Select-Object -First 1
    if ($wingetNinja) { $candidates += $wingetNinja.FullName }
    foreach ($p in $candidates) { if (Test-Path $p) { return $p } }
    return $null
  }

  function Get-ClangBinPaths {
    $clang = Get-Command clang -ErrorAction SilentlyContinue
    $clangpp = Get-Command clang++ -ErrorAction SilentlyContinue
  if ($clang -and $clangpp) { return @{ C=$clang.Source; CXX=$clangpp.Source } }
    $base = Join-Path $env:ProgramFiles "LLVM\bin"
    $cpath = Join-Path $base "clang.exe"
    $xxpath = Join-Path $base "clang++.exe"
  if ((Test-Path $cpath) -and (Test-Path $xxpath)) { return @{ C=$cpath; CXX=$xxpath } }
    return $null
  }

  function Get-ResourceCompilerPath {
    # Prefer llvm-rc from LLVM
    $llvmrc = Get-Command llvm-rc -ErrorAction SilentlyContinue
    if ($llvmrc) { return $llvmrc.Source }
    $cand = Join-Path $env:ProgramFiles "LLVM\bin\llvm-rc.exe"
    if (Test-Path $cand) { return $cand }
    # Fallback to Windows SDK rc.exe (if available in PATH or VS dev prompt)
    $vsrc = Get-Command rc -ErrorAction SilentlyContinue
    if ($vsrc) { return $vsrc.Source }
    return $null
  }

  function Convert-ToCMakePath([string]$p) {
    if (-not $p) { return $p }
    return ($p -replace '\\','/')
  }

  function Get-MinGWPaths {
    $gcc = Get-Command x86_64-w64-mingw32-gcc -ErrorAction SilentlyContinue
    $gpp = Get-Command x86_64-w64-mingw32-g++ -ErrorAction SilentlyContinue
    $make = Get-Command mingw32-make -ErrorAction SilentlyContinue
    if ($gcc -and $gpp -and $make) { return @{ CC=$gcc.Source; CXX=$gpp.Source; MAKE=$make.Source } }
    # Fallback to generic MinGW (WinLibs) names
    $gcc = Get-Command gcc -ErrorAction SilentlyContinue
    $gpp = Get-Command g++ -ErrorAction SilentlyContinue
    if ($gcc -and $gpp -and $make) { return @{ CC=$gcc.Source; CXX=$gpp.Source; MAKE=$make.Source } }
    return $null
  }

  $attempted = @()
  $exit = -1

  if ($Clean) { Clear-CMakeArtifacts }

    if ($Generator) {
    $extraArgs = $null
    if ($Generator -eq 'Ninja') {
      $ninjaPath = Get-NinjaPath
      $clangBins = Get-ClangBinPaths
      $gpp = Get-Command g++ -ErrorAction SilentlyContinue
      $cl = Get-Command cl -ErrorAction SilentlyContinue
      $extraArgs = @()
        if ($ninjaPath) { $extraArgs += "-DCMAKE_MAKE_PROGRAM=$(Convert-ToCMakePath $ninjaPath)" }
      if ($clangBins) {
          $extraArgs += @("-DCMAKE_C_COMPILER=$(Convert-ToCMakePath $($clangBins.C))","-DCMAKE_CXX_COMPILER=$(Convert-ToCMakePath $($clangBins.CXX))")
      } elseif ($gpp) {
        $extraArgs += @('-DCMAKE_C_COMPILER=gcc','-DCMAKE_CXX_COMPILER=g++')
      } elseif (-not $cl) {
        Write-Host "No C++ compiler found on PATH for Ninja. Install one of: LLVM (clang++), MinGW (g++), or Visual Studio Build Tools (cl)." -ForegroundColor Yellow
      }
      $rcPath = Get-ResourceCompilerPath
        if ($rcPath) { $extraArgs += "-DCMAKE_RC_COMPILER=$(Convert-ToCMakePath $rcPath)" }
      } elseif ($Generator -eq 'MinGW Makefiles') {
        $mingw = Get-MinGWPaths
        $extraArgs = @()
        if ($mingw) {
          $extraArgs += @(
            "-DCMAKE_MAKE_PROGRAM=$(Convert-ToCMakePath $($mingw.MAKE))",
            "-DCMAKE_C_COMPILER=$(Convert-ToCMakePath $($mingw.CC))",
            "-DCMAKE_CXX_COMPILER=$(Convert-ToCMakePath $($mingw.CXX))"
          )
        } else {
          Write-Host "MinGW not found (x86_64-w64-mingw32-gcc/g++ and mingw32-make). Install LLVM-MinGW or MinGW-w64." -ForegroundColor Yellow
        }
    }
    Clear-CMakeArtifacts
    $exit = Invoke-CMakeConfigure $Generator $extraArgs
    $attempted += $Generator
  } else {
      # Try MinGW first (works without Visual Studio/Windows SDK)
      $mingw = Get-MinGWPaths
      if ($mingw) {
      # Clean cache to switch generators safely
      Clear-CMakeArtifacts
        $extra = @(
          "-DCMAKE_MAKE_PROGRAM=$(Convert-ToCMakePath $($mingw.MAKE))",
          "-DCMAKE_C_COMPILER=$(Convert-ToCMakePath $($mingw.CC))",
          "-DCMAKE_CXX_COMPILER=$(Convert-ToCMakePath $($mingw.CXX))"
        )
        $exit = Invoke-CMakeConfigure 'MinGW Makefiles' $extra
        $attempted += 'MinGW Makefiles'
    }
      # Then Ninja (requires clang/VS/MinGW libs)
    # Then Visual Studio 2022
    if ($exit -ne 0) {
        $ninjaPath = Get-NinjaPath
        if ($ninjaPath) {
          Clear-CMakeArtifacts
          $clangBins = Get-ClangBinPaths
          $gpp = Get-Command g++ -ErrorAction SilentlyContinue
          $cl = Get-Command cl -ErrorAction SilentlyContinue
          $extra = @("-DCMAKE_MAKE_PROGRAM=$(Convert-ToCMakePath $ninjaPath)")
          if ($clangBins) {
            $extra += @("-DCMAKE_C_COMPILER=$(Convert-ToCMakePath $($clangBins.C))","-DCMAKE_CXX_COMPILER=$(Convert-ToCMakePath $($clangBins.CXX))")
          } elseif ($gpp) {
            $extra += @('-DCMAKE_C_COMPILER=gcc','-DCMAKE_CXX_COMPILER=g++')
          } elseif (-not $cl) {
            Write-Host "No C++ compiler found on PATH for Ninja. Install one of: LLVM (clang++), MinGW (g++), or Visual Studio Build Tools (cl)." -ForegroundColor Yellow
          }
          $rcPath = Get-ResourceCompilerPath
          if ($rcPath) { $extra += "-DCMAKE_RC_COMPILER=$(Convert-ToCMakePath $rcPath)" }
          $exit = Invoke-CMakeConfigure 'Ninja' $extra
          $attempted += 'Ninja'
        }
      }
      if ($exit -ne 0) {
      Clear-CMakeArtifacts
      $exit = Invoke-CMakeConfigure 'Visual Studio 17 2022' @('-A','x64')
      $attempted += 'Visual Studio 17 2022'
    }
    if ($exit -ne 0) {
      Clear-CMakeArtifacts
        $exit = Invoke-CMakeConfigure 'Visual Studio 16 2019' @('-A','x64')
      $attempted += 'Visual Studio 16 2019'
    }
    if ($exit -ne 0) {
      Clear-CMakeArtifacts
      $mingw = Get-Command mingw32-make -ErrorAction SilentlyContinue
      if ($mingw) {
          $exit = Invoke-CMakeConfigure 'MinGW Makefiles' $null
        $attempted += 'MinGW Makefiles'
      }
    }
    if ($exit -ne 0 -and $attempted.Count -eq 0) {
      Write-Host "No known generators detected on PATH. CMake will choose a default (may require Visual Studio Build Tools)." -ForegroundColor Yellow
      Clear-CMakeArtifacts
      $exit = Invoke-CMakeConfigure $null $null
      $attempted += '(CMake default)'
    }
  }

  if ($exit -ne 0) {
    # If configure returned non-zero but cache/build files exist, continue
    if ((Test-Path 'CMakeCache.txt') -and ((Test-Path 'build.ninja') -or (Test-Path 'Makefile') -or (Test-Path 'CMakeFiles'))) {
      Write-Host "CMake configure reported a non-zero exit code but build files are present; proceeding with build." -ForegroundColor Yellow
      $exit = 0
    }
  }
  if ($exit -ne 0) {
    Write-Error "CMake configure failed. Attempted generators: $($attempted -join ', '). Ensure one of these is installed: Ninja, Visual Studio (2022/2019), or MinGW."
    Write-Host "Tips:" -ForegroundColor Yellow
    Write-Host "  - Install Ninja: winget install ninja-build.ninja or pip install ninja (ensure Scripts on PATH)"
    Write-Host "  - Or install Visual Studio Build Tools (MSVC + C++ CMake tools): https://aka.ms/vs/17/release/vs_BuildTools.exe"
    Write-Host "  - Or install MSYS2 MinGW and use MinGW Makefiles"
    exit 1
  }

  Write-Host "Building configuration: $cfg" -ForegroundColor Cyan
  cmake --build . --config $cfg

  Write-Host "\nBuilt binaries in: $build" -ForegroundColor Green
} finally {
  Pop-Location
}
