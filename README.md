# ked — Klimadaten EDitor

A lean, modern climate data processing tool written in C11. Designed as a lightweight alternative to [CDO](https://code.mpimet.mpg.de/projects/cdo), `ked` reads netCDF and Zarr (via NCZarr) files with colored terminal output and is built for future multicore support via OpenMP.

## Dependencies

- C11 compiler (GCC or Clang)
- CMake >= 3.14
- libnetcdf >= 4.8 (includes NCZarr for Zarr support)
- pkg-config

### Installing dependencies

**macOS (Homebrew):**

```bash
brew install netcdf cmake pkg-config
```

**Ubuntu / Debian:**

```bash
sudo apt install libnetcdf-dev cmake build-essential pkg-config
```

**Fedora:**

```bash
sudo dnf install netcdf-devel cmake gcc pkg-config
```

## Building

```bash
cd ked
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
make -j$(nproc)
```

On **macOS with Homebrew** (Apple Silicon), you must specify the architecture explicitly:

```bash
cmake .. -DCMAKE_BUILD_TYPE=Release -DCMAKE_OSX_ARCHITECTURES=arm64
make -j$(sysctl -n hw.ncpu)
```

The resulting binary is `build/ked`.

### Running tests

The test suite uses CTest. Test data is generated automatically from a bundled generator (`tools/gen_testdata.c`).

```bash
make testdata   # generate synthetic netCDF file
ctest           # run all tests
```

### Building with AddressSanitizer (development)

```bash
cmake .. -DCMAKE_BUILD_TYPE=Debug \
         -DCMAKE_OSX_ARCHITECTURES=arm64 \
         -DCMAKE_C_FLAGS="-fsanitize=address -fno-omit-frame-pointer" \
         -DCMAKE_EXE_LINKER_FLAGS="-fsanitize=address"
make -j$(sysctl -n hw.ncpu)
```

## Usage

```
ked <operator> [options] <files...>
```

### Operators

| Operator | Description |
|----------|-------------|
| `info`   | Show detailed file information (dimensions, variables, attributes) |
| `sinfo`  | Show short summary — one line per variable |

### Options

| Flag | Description |
|------|-------------|
| `--help` | Show usage information |
| `--version` | Print version |
| `--no-color` | Disable colored output |

Color is also disabled automatically when stdout is not a terminal, when `$TERM` is `dumb`, or when the `NO_COLOR` environment variable is set (see [no-color.org](https://no-color.org/)).

### Examples

```bash
# Detailed file information
ked info climate.nc

# Short summary table
ked sinfo output.nc

# Plain output (no ANSI colors)
ked --no-color info climate.nc

# Works with NCZarr stores too
ked info "file://data.zarr#mode=nczarr,zarr"
```

## Project structure

```
ked/
├── CMakeLists.txt        # Build configuration
├── src/
│   ├── ked.h             # Version and constants
│   ├── main.c            # Entry point and operator dispatch
│   ├── cli.c/h           # Command-line argument parsing
│   ├── dataset.c/h       # Data model (variables, dimensions, attributes)
│   ├── io_netcdf.c/h     # netCDF / NCZarr I/O backend
│   ├── ops_info.c/h      # info and sinfo operators
│   ├── term.c/h          # Terminal output (colors, formatting)
│   └── util.c/h          # Memory, error handling, helpers
└── tools/
    └── gen_testdata.c    # Synthetic test data generator
```

## Roadmap

- **Phase 2** — Data operations: `copy`, `merge`, `cat`, `select`
- **Phase 3** — Statistical operators with OpenMP: `timmean`, `fldmean`, `yearmonmean`
- **Phase 4** — GRIB support via ecCodes

## License

TBD
