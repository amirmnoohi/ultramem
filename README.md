# UltraMem

High-performance memory bandwidth benchmark that **outperforms STREAM**.

## Features

- **Faster than STREAM** - Optimized kernels with OpenMP SIMD vectorization
- **Auto cache detection** - Detects L1/L2/L3 cache sizes automatically
- **Smart array sizing** - Auto-sizes arrays to 4× L3 cache to ensure DRAM testing
- **Cross-platform** - Linux, macOS, Windows
- **4 test kernels** - Named as `reads:writes` (1:1, 2:1, 1:0, 0:1)

## Quick Start

```bash
# Build
make

# Run with 8 threads, copy pattern (1:1)
./ultramem 8 1:1

# Run with 32 threads, triad pattern (2:1), 1GB arrays
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
  ./ultramem 8 1:1           # 8 threads, copy pattern
  ./ultramem 32 2:1 256      # 32 threads, triad, 256MB arrays
  ./ultramem 96 0:1 1024     # 96 threads, write-only, 1GB arrays
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

Kernels are named as `reads:writes` to show their memory access pattern:

| Kernel | Operation | Bytes/Element |
|--------|-----------|---------------|
| 1:1 | `c[i] = a[i]` | 16 (1 read + 1 write) |
| 2:1 | `a[i] = b[i] + scalar*c[i]` | 24 (2 reads + 1 write) |
| 1:0 | `sum += a[i]` | 8 (read only) |
| 0:1 | `a[i] = val` | 8 (write only) |

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

