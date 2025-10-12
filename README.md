# Al‑Muslim — Terminal Prayer & Hijri App

A beautiful, cross‑platform terminal app for prayer times and the Hijri calendar — fast, offline‑capable, and deployable on Windows/macOS/Linux terminals.

- Clean, modern TUI that works in PowerShell, Windows Terminal, Terminal.app, iTerm2, bash, zsh — even over SSH.
- Shows today’s Gregorian date, Hijri (Umm al‑Qura) date, sunrise/sunset, and the five daily prayers (Fajr, Dhuhr, Asr, Maghrib, Isha) with a next‑prayer countdown.
- Configurable calculation method (MWL, ISNA, Umm al‑Qura, etc.), madhab (Shafi/Hanafi), high‑latitude rules, language, and timezone.
- Fully offline after first install; optional lightweight update checker.
- Cross‑platform packaging with PyInstaller for one‑file binaries (Windows .exe, macOS .app/.dmg, Linux AppImage).

> Note: This repository’s Terminal app lives under `Terminal/` (with OS subfolders). The GUI shells under `App/` are independent.


## Quick start

### End users (pip install from GitHub)

Install directly from your GitHub fork (replace <you> with your username or org):

```powershell
# PowerShell (Windows)
python -m pip install --upgrade pip
pip install "git+https://github.com/<you>/al-muslim.git"
```

```bash
# macOS / Linux (bash/zsh)
python3 -m pip install --upgrade pip
pip3 install "git+https://github.com/<you>/al-muslim.git"
```

Run the app:

```powershell
al-muslim
```

- The command is typically `al-muslim` on all platforms. If your environment renames entry points, you may see `al-muslim.exe` on Windows.

Optional: install via pipx to keep it isolated from your global Python:

```powershell
pip install pipx
pipx install "git+https://github.com/<you>/al-muslim.git"
```

### End users (download a binary)

Download the latest release for your OS from GitHub Releases:

- Windows: `Al-Muslim-x.y.z-windows-amd64.exe`
- macOS: `Al-Muslim-x.y.z-macos-universal.app.zip` or `.dmg`
- Linux: `Al-Muslim-x.y.z-linux-amd64.AppImage`

Then run it from your terminal. No Python required.


## Features

- TUI built with Textual/Rich for a crisp, accessible terminal UI with colors, layout, and keyboard hints.
- Prayer times: Fajr, Sunrise, Dhuhr, Asr, Maghrib, Isha.
- Hijri date (Umm al‑Qura).
- Next‑prayer countdown and highlights.
- Configurable calculation method (MWL, ISNA, Umm al‑Qura, etc.), madhab (Shafi/Hanafi), high‑latitude rules.
- Timezone/user locale handling.
- Offline use after install; the app calculates prayer times locally and uses system time.
- Optional update check (non‑blocking, can be disabled in config).


## Controls and usage

- Start: `al-muslim`
- Keyboard shortcuts:
  - q: Quit
  - r: Refresh
  - c: Open config
  - n: Show next 7 days
  - h: Help overlay

Note: Keybindings can vary depending on the final TUI design; see in‑app Help (h) for the authoritative list.


## Configuration

Default config path:

- Linux/macOS: `~/.al-muslim/config.toml`
- Windows: `%USERPROFILE%\.al-muslim\config.toml`

You can copy and edit the sample to get started:

```powershell
# From repo root
Copy-Item -Path .\config.sample.toml -Destination "$HOME/.al-muslim/config.toml" -Force
```

Abridged example (see `config.sample.toml` for all options):

```toml
[location]
city = "Riyadh"
latitude = 24.7136
longitude = 46.6753
timezone = "Asia/Riyadh"  # Optional; app can infer from system

[calculation]
method = "umm_al_qura"    # mwl|isna|umm_al_qura|egypt|karachi|makkah|tehran|kuwait|qatar|singapore|turkey|france|russia|gulf|diyanet
madhab = "shafi"          # shafi|hanafi
high_latitude_rule = "middle_of_the_night"  # middle_of_the_night|seventh_of_the_night|twilight_angle

[ui]
language = "en"           # en|ar (more may be added)
24h = true                 # use 24-hour clock
colors = "auto"            # auto|light|dark

[updates]
auto_check = true          # non-blocking, silent if offline
channel = "stable"         # stable|beta
```

Environment variables

- `ALMUSLIM_CONFIG`: Override the config file path.
- `TZ`: Some shells honor TZ; otherwise set `timezone` in config.


## Calculation methods and rules

- Methods: MWL, ISNA, Umm al‑Qura, Makkah, Egypt, Karachi, Tehran, Kuwait, Qatar, Singapore, Turkey (Diyanet), France, Russia, Gulf, etc.
- Madhab: Shafi or Hanafi (affects Asr).
- High‑latitude rules: angle‑based or night‑fraction (e.g., middle of the night) to handle regions with short nights.

This app is designed to interoperate with common Python prayer time libraries. Under the hood you can expect:

- Astral for sunrise/sunset and solar calculations.
- HijriDate (Umm al‑Qura) for Hijri calendar.
- Textual + Rich for the TUI.


## Offline behavior

- Once installed, the app computes times locally using your config and system clock.
- Network access is only used for an optional update check (if enabled) or for any user‑requested geocoding features (if implemented). The app runs fine without internet.


## Developer setup

> These are suggested steps for contributors; they may be adapted once the codebase is fully wired.

Prereqs:

- Python 3.10+
- Git

Create a virtual environment and install dev dependencies:

```bash
python -m venv .venv
# PowerShell
. .venv/Scripts/Activate.ps1
# bash/zsh
source .venv/bin/activate

pip install -U pip wheel
# Core runtime
pip install textual rich astral hijri-date pytz
# CLI helpers (if used)
pip install typer[all] click
# Packaging
pip install pyinstaller
# Lint/format (optional)
pip install ruff black
```

Project layout (relevant to Terminal app):

```
Terminal/
  Windows/
  MacOS/
  Linux/
# …other app shells under App/
```

Running from source (example):

```bash
python -m al_muslim
# or
python Terminal/main.py  # if entry point is a script
```

Note: The exact module/entry point names depend on how the code is structured; check the `Terminal/` folder.


## Building binaries with PyInstaller

Create single‑file executables:

```bash
# Windows
pyinstaller -F -n al-muslim --console Terminal/main.py

# macOS (universal binaries require additional flags and a universal Python)
pyinstaller -F -n Al-Muslim --windowed Terminal/main.py

# Linux
pyinstaller -F -n al-muslim Terminal/main.py
```

Tips:

- Add a spec file (`al-muslim.spec`) for shared assets, icons, and data files.
- For Textual apps, ensure the terminal is attached (`--console`) unless you build a GUI wrapper.
- Sign and notarize macOS builds if you distribute widely.


## Troubleshooting

- Timezone looks wrong: Set `timezone` explicitly in config, or check your OS time settings.
- Wrong location: Provide `latitude`/`longitude` and `city` in config. If auto‑location is added later, ensure permissions.
- High latitude (extreme north/south): Try another `high_latitude_rule`.
- Fonts/boxes look broken: Switch your terminal to a font that supports box‑drawing and Arabic; try light/dark mode.
- Windows colors: Use Windows Terminal or PowerShell 7+ for best ANSI support.


## Roadmap

- Location auto‑detection (with offline cache).
- Localization (Arabic UI and more languages).
- Export next‑7‑days/CSV.
- Notifications before prayer time.
- Minimal web API bridge.


## Contributing

Issues and PRs are welcome. Please:

- Keep features focused and small.
- Add tests for new logic where feasible.
- Update documentation for user‑visible changes.


## License

Choose and add a license (e.g., MIT/Apache‑2.0). Include `LICENSE` in the repository root.


## Credits

- Textual and Rich for TUI/formatting
- Astral for solar events
- HijriDate (Umm al‑Qura)
- PyInstaller for packaging

Links:

- Textual docs: https://textual.textualize.io/
- Rich docs: https://rich.readthedocs.io/
- Astral: https://astral.readthedocs.io/
- HijriDate: https://pypi.org/project/hijri-date/
- PyInstaller: https://www.pyinstaller.org/


## C++ native build (fast CLI)

This repo also includes a portable C++ CLI implementation (work‑in‑progress) that starts instantly and can be extended with native libraries. It uses CMake and requires a C++17 compiler.

Prereqs:

- CMake 3.16+
- Ninja (recommended) or your default generator
- A C++17 compiler (MSVC, Clang, or GCC)

Build and run:

Windows (PowerShell):

```powershell
# From repo root
Set-Location "Terminal/Windows"
./build.ps1 -Release
# Run
& ../cpp/build/al-muslim.exe
```

macOS:

```bash
cd Terminal/MacOS
chmod +x build.sh
./build.sh
# Run
../cpp/build/al-muslim
```

Linux:

```bash
cd Terminal/Linux
chmod +x build.sh
./build.sh
# Run
../cpp/build/al-muslim
```

Notes:

- Config path resolution mirrors the Python version: `~/.al-muslim/config.toml` on all OSes (Windows uses `%USERPROFILE%\\.al-muslim\\config.toml`). You can override via `ALMUSLIM_CONFIG`.
- The current C++ app prints placeholders for prayer times; integrate a prayer time library (e.g., Adhan algorithms) and a Hijri date library for full parity.
- For a rich TUI in C++, consider FTXUI. For TOML parsing, consider `toml++`.
