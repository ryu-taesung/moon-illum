# moon-illum

Small C++ program that fetches current moon phase and illumination
from the USNO Astronomical Applications API.

## Build

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
```

## Run

```bash
export USNO_LAT=40.7128
export USNO_LON=-74.0060
export USNO_TZ=-5
./build/moon
```

### Notes
- Uses Boost.Asio / Beast for HTTPS
- Uses std::promise
