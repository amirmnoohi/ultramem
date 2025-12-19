# UltraMem

High-performance memory bandwidth benchmark.

## Features

- **Optimized kernels** - OpenMP SIMD vectorization for maximum bandwidth
- **Auto cache detection** - Detects L1/L2/L3 cache sizes automatically
- **Smart array sizing** - Auto-sizes arrays to 4× L3 cache to ensure DRAM testing
- **Cross-platform** - Linux, macOS, Windows
- **Flexible patterns** - Any `reads:writes` combination (e.g., 1:1, 2:1, 5:5, 10:10)

## Quick Start

```bash
# Build
make

# Run with 8 threads, copy pattern (1:1)
./ultramem 8 1:1

# Run with 32 threads, 2:1 pattern, 1GB arrays
./ultramem 32 2:1 1024
```

## Build

### Linux
```bash
make
```

### macOS (requires Homebrew GCC for OpenMP)
```bash
brew install gcc
make
```

### Windows (MSVC)
```bash
cl /O2 /openmp ultramem.c
```

### Manual Build
```bash
gcc -O3 -march=native -fopenmp -ffast-math -funroll-loops -ftree-vectorize -o ultramem ultramem.c
```

## Usage

```
./ultramem <threads> <reads:writes> [array_size_mb]

Arguments:
  threads        Number of OpenMP threads (required)
  reads:writes   Memory access pattern (required)
  array_size_mb  Size of each array in MB (default: 4x L3 cache)

Examples:
  ./ultramem 8 1:1           # 8 threads, 1 read + 1 write
  ./ultramem 32 2:1 256      # 32 threads, 2 reads + 1 write, 256MB arrays
  ./ultramem 96 0:1 1024     # 96 threads, write-only, 1GB arrays
  ./ultramem 16 5:5          # 16 threads, 5 reads + 5 writes
```

## Sample Output

```
════════════════════════════════════════════════════════════
  Cache Hierarchy Detected
════════════════════════════════════════════════════════════
  L1 Data:           64 KB (per core)
  L1 Instr:          64 KB (per core)
  L2 Cache:        1024 KB
  L3 Cache:           8 MB (shared)
  Cache Line:        64 bytes
  Physical Cores:    96
════════════════════════════════════════════════════════════

════════════════════════════════════════════════════════════
  UltraMem - Memory Bandwidth Benchmark
════════════════════════════════════════════════════════════
  Kernel pattern:    2:1 (2 reads + 1 writes)
  Threads:           32
  Array elements:    16777216
  Memory per array:  128.0 MB
  Total memory:      384.0 MB
  L3 Cache:          8.0 MB
  Arrays vs L3:      48.0x (DRAM test ✓)
════════════════════════════════════════════════════════════

────────────────────────────────────────────────────────────
Kernel      Best MB/s    Avg MB/s     Min Time     Max Time
────────────────────────────────────────────────────────────
2:1          79969.1     79617.3     0.005035     0.005079
────────────────────────────────────────────────────────────

════════════════════════════════════════════════════════════
  PEAK BANDWIDTH: 79969.1 MB/s (79.97 GB/s)
════════════════════════════════════════════════════════════
```

## Kernels

Pattern format: `reads:writes` - any values from 0 to 100.

**Bytes transferred per element = (reads + writes) × 8 bytes**

| Pattern | Bytes/Element | Description |
|---------|---------------|-------------|
| 0:1 | 8 | Write only |
| 1:0 | 8 | Read only |
| 1:1 | 16 | Copy |
| 2:1 | 24 | 2 reads + 1 write |
| 3:3 | 48 | Heavy load |
| 10:10 | 160 | Extreme load |

## Make Targets

```bash
make          # Build ultramem
make debug    # Build with debug symbols
make static   # Build static binary
make clean    # Remove built files
make install  # Install to /usr/local/bin
make test     # Run quick test
make bench    # Run scaling benchmark
make help     # Show help
```

## Requirements

- GCC 4.9+ or Clang 3.7+ with OpenMP support
- POSIX system (Linux, macOS) or Windows

## License

MIT License

