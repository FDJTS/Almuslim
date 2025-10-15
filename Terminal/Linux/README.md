# Linux build and run

You can build the terminal app on Linux in two ways.

- CMake + g++ (default):
  - Faster for incremental builds; supports Ninja if installed.
- Direct g++ via Makefile (no CMake):
  - Minimal dependencies, handy for quick builds.

## Prerequisites
- g++ (C++17)
- make (for Makefile route)
- cmake and optionally ninja (for CMake route)

## Build with CMake
```bash
./build.sh
./run.sh --setup
```

## Build directly with g++ (Makefile)
```bash
./build_gpp.sh
./run.sh --setup
```

The binary is placed under:
- CMake: `cpp/build/al-muslim` (or `cpp/build/Release/al-muslim`)
- Makefile: `cpp/build-gpp/al-muslim`

Data files are copied next to the binary so the app can find `data/` at runtime.

## Notes
- Config file path on Linux: `$HOME/.al-muslim/config.toml` (override with `ALMUSLIM_CONFIG` env var).
- Use `--ask` to choose a city at launch; use `--week` to print the next 7 days.