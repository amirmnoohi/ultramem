# UltraMem

High-performance memory bandwidth benchmark that **outperforms STREAM**.

## Features

- **Faster than STREAM** - Optimized kernels with OpenMP SIMD vectorization
- **Auto cache detection** - Detects L1/L2/L3 cache sizes automatically
- **Smart array sizing** - Auto-sizes arrays to 4× L3 cache to ensure DRAM testing
- **Cross-platform** - Linux, macOS, Windows
- **7 test kernels** - Copy, Scale, Add, Triad, Read, Write, Memcpy

## Quick Start

```bash
# Build
make

# Run with 8 threads
./ultramem 8

# Run with 32 threads and 1GB arrays
./ultramem 32 1024
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
./ultramem <threads> [array_size_mb]

Arguments:
  threads        Number of OpenMP threads (required)
  array_size_mb  Size of each array in MB (default: 4x L3 cache)

Examples:
  ./ultramem 8           # 8 threads, auto array size
  ./ultramem 32 256      # 32 threads, 256MB arrays
  ./ultramem 96 1024     # 96 threads, 1GB arrays
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
Copy         79302.6     78810.3     0.003385     0.003448
Scale        78422.4     77735.7     0.003423     0.003539
Add          79527.2     79168.3     0.005063     0.005161
Triad        79969.1     79617.3     0.005035     0.005079
Read         87776.3     86033.1     0.001529     0.001584
Write       103330.9    101889.0     0.001299     0.001340
Memcpy       26661.9     26532.7     0.010068     0.010221
────────────────────────────────────────────────────────────

════════════════════════════════════════════════════════════
  PEAK BANDWIDTH: 103330.9 MB/s (103.33 GB/s)
════════════════════════════════════════════════════════════
```

## Kernels

| Kernel | Operation | Bytes/Element |
|--------|-----------|---------------|
| Copy | `c[i] = a[i]` | 16 (1 read + 1 write) |
| Scale | `b[i] = scalar * c[i]` | 16 (1 read + 1 write) |
| Add | `c[i] = a[i] + b[i]` | 24 (2 reads + 1 write) |
| Triad | `a[i] = b[i] + scalar*c[i]` | 24 (2 reads + 1 write) |
| Read | `sum += a[i]` | 8 (1 read) |
| Write | `a[i] = val` | 8 (1 write) |
| Memcpy | `memcpy(c, a, n)` | 16 (baseline) |

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

