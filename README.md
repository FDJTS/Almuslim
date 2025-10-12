# Al‑Muslim — Fast C++ Terminal Prayer Times

Al‑Muslim is a fast, offline, cross‑platform terminal app that shows today’s prayer times (Fajr, Sunrise, Dhuhr, Asr, Maghrib, Isha) and a next‑prayer countdown. It’s implemented in portable C++17 and ships with per‑OS scripts plus a Windows installer.

- Accurate solar math (NOAA‑style) with common methods (MWL, ISNA, Umm al‑Qura, Egypt, Karachi, Tehran), Shafi/Hanafi Asr, and high‑latitude rules.
- Works on Windows, macOS, and Linux terminals; no runtime dependencies after build/install.
- Simple interactive setup to pick your city from a built‑in database.

Folders of interest:

- Terminal/cpp — C++ source, CMake, data (cities.csv)
- Terminal/Windows — build/run scripts, Inno Setup installer
- Terminal/MacOS and Terminal/Linux — build/run scripts
- App/ — other shells (independent of the CLI)


## Quick start (Windows)

Option A — Installer (recommended for users):

1) Download Almuslim‑Setup‑X.Y.Z.exe from GitHub Releases (or from your CI if configured).
2) Double‑click to install. The installer creates Start Menu entries:
   - Almuslim — runs the app
   - Almuslim (Setup City) — opens the city selector once and saves your config
3) After install, press the Windows key and type “Almuslim”.

Option B — Build and run from source:

1) Prereqs: CMake 3.16+, and one of: Visual Studio (MSVC), LLVM/Clang + Ninja, or MinGW‑w64 (g++ + mingw32‑make).
2) From the repo root in PowerShell:
   - cd Terminal/Windows
   - ./build.ps1 -Release
   - .\run.cmd  --setup

The .\run.cmd script finds and runs the built al-muslim.exe without needing PowerShell.


## Quick start (macOS / Linux)

1) Prereqs: CMake 3.16+, Clang/GCC.
2) From the repo root:
   - macOS: cd Terminal/MacOS && chmod +x build.sh run.sh && ./build.sh && ./run.sh --setup
   - Linux: cd Terminal/Linux && chmod +x build.sh run.sh && ./build.sh && ./run.sh --setup


## Usage

- First run: al-muslim --setup to select your city and save config.
- Normal run: al-muslim
- Output shows today’s times and the next‑prayer countdown.

Config file location:
- Windows: %USERPROFILE%\.al-muslim\config.toml
- macOS/Linux: ~/.al-muslim/config.toml

Environment override:
- ALMUSLIM_CONFIG — set a custom config file path.


## Configuration

You can copy the sample config and edit it:

PowerShell (from repo root):
- Copy-Item -Path .\config.sample.toml -Destination "$HOME/.al-muslim/config.toml" -Force

Example (abridged):

```toml
[location]
city = "Riyadh"
latitude = 24.7136
longitude = 46.6753
# Timezone: prefer numeric offsets for the C++ CLI, e.g. "+03:00".
# Some IANA names like "Asia/Riyadh" are recognized; otherwise the app uses your system timezone.
timezone = "+03:00"

[calculation]
method = "umm_al_qura"       # mwl|isna|umm_al_qura|egypt|karachi|makkah|tehran
madhab = "shafi"             # shafi|hanafi
high_latitude_rule = "middle_of_the_night"   # middle_of_the_night|seventh_of_the_night|twilight_angle

[ui]
24h = true
language = "en"
```

Keys are read as flat key=value pairs, so section headers [location], [calculation] are optional for the CLI.


## Calculation details

- Solar base: NOAA equation of time and declination; sunrise/sunset at −0.833° (refraction + solar radius).
- Fajr/Isha: angle‑based by method presets; Umm al‑Qura uses a fixed Isha offset of 90 minutes after Maghrib.
- Asr: Shafi (factor 1) or Hanafi (factor 2).
- High‑latitude: night‑fraction (middle or seventh) or basic twilight‑angle rule fallback.
- Timezone: numeric offsets like +03:00 are fully supported; a few common IANA names are mapped; otherwise system timezone is used.


## Windows installer (Inno Setup)

- Script: Terminal/Windows/installer.iss
- Packager: Terminal/Windows/make_installer.ps1
- The installer bundles al-muslim.exe and the data directory (cities.csv). It creates Start Menu shortcuts (normal and “Setup City”).

CI: .github/workflows/release-windows-installer.yml builds the Release binary with Visual Studio and then compiles the installer. It triggers on tags starting with v (e.g., v0.1.0) or manual dispatch.


## Build from source (details)

Windows (PowerShell):
- cd Terminal/Windows
- ./build.ps1 -Release
- .\run.cmd  [--setup]

macOS:
- cd Terminal/MacOS && chmod +x build.sh run.sh && ./build.sh
- ./run.sh [--setup]

Linux:
- cd Terminal/Linux && chmod +x build.sh run.sh && ./build.sh
- ./run.sh [--setup]

Binaries are placed under Terminal/cpp/build (and possibly build/Release when using Visual Studio). Data is copied next to the executable for runtime access.


## Troubleshooting

- Build configure fails on Windows:
  - Ensure one toolchain is installed: Visual Studio (C++ Desktop), LLVM + Ninja, or MinGW‑w64 (g++, mingw32‑make).
  - Delete Terminal/cpp/build* and re‑run Terminal/Windows/build.ps1.
- Inno Setup not found: install from https://jrsoftware.org/isdl.php and make sure ISCC.exe is on PATH, or let CI build it.
- Timezone looks off: set timezone to a numeric offset like "+03:00" in config, or ensure system timezone matches your city.
- High latitudes: try high_latitude_rule = "seventh_of_the_night" or "twilight_angle".


## Contributing

Small, focused PRs are welcome. Please include a brief description and update docs if behavior changes.


## License

MIT — see LICENSE.

