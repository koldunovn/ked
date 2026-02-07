# ked — Klimadaten EDitor

A lean, modern climate data processing tool written in C11. Designed as a lightweight alternative to [CDO](https://code.mpimet.mpg.de/projects/cdo), `ked` reads netCDF, Zarr (via NCZarr), and GRIB files with colored terminal output and is built for future multicore support via OpenMP.

## Dependencies

### Required

- C11 compiler (GCC or Clang)
- CMake >= 3.14
- libnetcdf >= 4.8 (includes NCZarr for Zarr support)
- pkg-config

### Optional

- ecCodes >= 2.20 (enables GRIB1/GRIB2 support)

### Installing dependencies

**macOS (Homebrew):**

```bash
brew install netcdf cmake pkg-config eccodes
```

**Ubuntu / Debian:**

```bash
sudo apt install libnetcdf-dev cmake build-essential pkg-config libeccodes-dev
```

**Fedora:**

```bash
sudo dnf install netcdf-devel cmake gcc pkg-config eccodes-devel
```

ecCodes is optional — the build will succeed without it, but GRIB support will be disabled.

**DKRZ Levante:**

Load netCDF via the module system and point pkg-config at the spack-installed ecCodes:

```bash
module load netcdf-c
export PKG_CONFIG_PATH="/sw/spack-levante/eccodes-2.44.0-hsksp4/lib64/pkgconfig:$PKG_CONFIG_PATH"
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

CMake will report whether GRIB support is enabled:

```
-- ecCodes found (2.45.0) - GRIB support enabled
```

The resulting binary is `build/ked`.

### Running tests

The test suite uses CTest. Test data is generated automatically from a bundled generator (`tools/gen_testdata.c`).

```bash
make testdata   # generate synthetic netCDF and GRIB files
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

### Supported formats

| Format | Detection | Notes |
|--------|-----------|-------|
| netCDF-3 | `CDF\x01` / `CDF\x02` magic bytes | Classic and 64-bit offset |
| netCDF-4 | `\x89HDF` magic bytes | HDF5-based |
| Zarr v2 | Directory with `.zgroup` / `.zmetadata` | Opened via NCZarr |
| GRIB1/GRIB2 | `GRIB` magic bytes | Requires ecCodes |

File format is auto-detected — no flags needed.

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

# GRIB files work the same way
ked info era5_temperature.grib2

# Plain output (no ANSI colors)
ked --no-color info climate.nc

# Works with Zarr stores (auto-detected)
ked info data.zarr
```

## Project structure

```
ked/
├── CMakeLists.txt        # Build configuration
├── src/
│   ├── ked.h             # Version, constants, type/format enums
│   ├── main.c            # Entry point and operator dispatch
│   ├── cli.c/h           # Command-line argument parsing
│   ├── dataset.c/h       # Data model (variables, dimensions, attributes)
│   ├── io.c              # Format auto-detection and backend dispatch
│   ├── io_netcdf.c/h     # netCDF / NCZarr I/O backend
│   ├── io_grib.c/h       # GRIB I/O backend (optional, requires ecCodes)
│   ├── ops_info.c/h      # info and sinfo operators
│   ├── term.c/h          # Terminal output (colors, formatting)
│   └── util.c/h          # Memory, error handling, helpers
└── tools/
    └── gen_testdata.c    # Synthetic test data generator (netCDF + GRIB)
```

## Roadmap

- **Phase 1** — Foundation + info/sinfo operators ✅
- **Phase 2** — GRIB support via ecCodes ✅
- **Phase 3** — Data operations: `copy`, `merge`, `cat`, `select`
- **Phase 4** — Statistical operators with OpenMP: `timmean`, `fldmean`, `yearmonmean`

See [PLAN.md](PLAN.md) for detailed implementation notes.

## License

TBD
