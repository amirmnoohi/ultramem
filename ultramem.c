/*
 * ============================================================================
 *  UltraMem - High Performance Memory Bandwidth Benchmark
 * ============================================================================
 *  
 *  Copyright (c) 2024 - MIT License
 *  https://github.com/amirmnoohi/ultramem
 *
 *  A multi-threaded memory bandwidth benchmark that outperforms STREAM.
 *  Features:
 *    - OpenMP SIMD vectorization
 *    - Automatic cache hierarchy detection (L1/L2/L3)
 *    - Cross-platform: Linux, macOS, Windows
 *    - Auto-sizing arrays to bypass cache
 *
 * ============================================================================
 */

#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#ifdef _WIN32
    #include <windows.h>
    #include <intrin.h>
    #define aligned_alloc(align, size) _aligned_malloc(size, align)
    #define aligned_free(ptr) _aligned_free(ptr)
#else
    #include <sys/time.h>
    #include <unistd.h>
    #define aligned_free(ptr) free(ptr)
    #ifdef __APPLE__
        #include <sys/sysctl.h>
    #endif
#endif

#include <omp.h>

#ifndef NTIMES
#define NTIMES 20
#endif

#define ALIGN 64

// Cache info structure
typedef struct {
    size_t l1d_size;    // L1 data cache (per core)
    size_t l1i_size;    // L1 instruction cache (per core)
    size_t l2_size;     // L2 cache (per core or shared)
    size_t l3_size;     // L3 cache (usually shared)
    size_t line_size;   // Cache line size
    int num_cores;      // Number of physical cores
} cache_info_t;

static double *restrict a = NULL;
static double *restrict b = NULL;
static double *restrict c = NULL;

// ============================================================================
// Cross-platform timing
// ============================================================================

static inline double get_time_sec(void) {
#ifdef _WIN32
    LARGE_INTEGER freq, counter;
    QueryPerformanceFrequency(&freq);
    QueryPerformanceCounter(&counter);
    return (double)counter.QuadPart / (double)freq.QuadPart;
#else
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return tv.tv_sec + tv.tv_usec * 1e-6;
#endif
}

// ============================================================================
// Cross-platform cache detection
// ============================================================================

#ifdef _WIN32
// Windows: Use GetLogicalProcessorInformation
static void detect_cache_windows(cache_info_t *info) {
    DWORD buffer_size = 0;
    GetLogicalProcessorInformation(NULL, &buffer_size);
    
    SYSTEM_LOGICAL_PROCESSOR_INFORMATION *buffer = 
        (SYSTEM_LOGICAL_PROCESSOR_INFORMATION *)malloc(buffer_size);
    
    if (GetLogicalProcessorInformation(buffer, &buffer_size)) {
        DWORD count = buffer_size / sizeof(SYSTEM_LOGICAL_PROCESSOR_INFORMATION);
        for (DWORD i = 0; i < count; i++) {
            if (buffer[i].Relationship == RelationCache) {
                CACHE_DESCRIPTOR *cache = &buffer[i].Cache;
                switch (cache->Level) {
                    case 1:
                        if (cache->Type == CacheData)
                            info->l1d_size = cache->Size;
                        else if (cache->Type == CacheInstruction)
                            info->l1i_size = cache->Size;
                        info->line_size = cache->LineSize;
                        break;
                    case 2:
                        info->l2_size = cache->Size;
                        break;
                    case 3:
                        info->l3_size = cache->Size;
                        break;
                }
            } else if (buffer[i].Relationship == RelationProcessorCore) {
                info->num_cores++;
            }
        }
    }
    free(buffer);
}
#endif

#ifdef __APPLE__
// macOS: Use sysctl
static void detect_cache_macos(cache_info_t *info) {
    size_t size;
    size_t len = sizeof(size);
    
    if (sysctlbyname("hw.l1dcachesize", &size, &len, NULL, 0) == 0)
        info->l1d_size = size;
    if (sysctlbyname("hw.l1icachesize", &size, &len, NULL, 0) == 0)
        info->l1i_size = size;
    if (sysctlbyname("hw.l2cachesize", &size, &len, NULL, 0) == 0)
        info->l2_size = size;
    if (sysctlbyname("hw.l3cachesize", &size, &len, NULL, 0) == 0)
        info->l3_size = size;
    
    int cores;
    len = sizeof(cores);
    if (sysctlbyname("hw.physicalcpu", &cores, &len, NULL, 0) == 0)
        info->num_cores = cores;
    
    len = sizeof(size);
    if (sysctlbyname("hw.cachelinesize", &size, &len, NULL, 0) == 0)
        info->line_size = size;
}
#endif

#if defined(__linux__)
// Linux: Read from sysfs
static size_t read_cache_size(const char *path) {
    FILE *f = fopen(path, "r");
    if (!f) return 0;
    
    char buf[64];
    if (fgets(buf, sizeof(buf), f)) {
        fclose(f);
        size_t val = 0;
        char unit = 'B';
        sscanf(buf, "%zu%c", &val, &unit);
        if (unit == 'K') val *= 1024;
        else if (unit == 'M') val *= 1024 * 1024;
        return val;
    }
    fclose(f);
    return 0;
}

static void detect_cache_linux(cache_info_t *info) {
    // Try sysfs first (more reliable)
    const char *base = "/sys/devices/system/cpu/cpu0/cache";
    char path[256];
    
    for (int i = 0; i < 4; i++) {
        snprintf(path, sizeof(path), "%s/index%d/level", base, i);
        FILE *f = fopen(path, "r");
        if (!f) continue;
        
        int level;
        if (fscanf(f, "%d", &level) != 1) { fclose(f); continue; }
        fclose(f);
        
        snprintf(path, sizeof(path), "%s/index%d/type", base, i);
        f = fopen(path, "r");
        if (!f) continue;
        
        char type[32];
        if (fscanf(f, "%31s", type) != 1) { fclose(f); continue; }
        fclose(f);
        
        snprintf(path, sizeof(path), "%s/index%d/size", base, i);
        size_t size = read_cache_size(path);
        
        snprintf(path, sizeof(path), "%s/index%d/coherency_line_size", base, i);
        f = fopen(path, "r");
        if (f) {
            int line;
            if (fscanf(f, "%d", &line) == 1) info->line_size = line;
            fclose(f);
        }
        
        if (level == 1) {
            if (strcmp(type, "Data") == 0) info->l1d_size = size;
            else if (strcmp(type, "Instruction") == 0) info->l1i_size = size;
        } else if (level == 2) {
            info->l2_size = size;
        } else if (level == 3) {
            // L3 is often shared - try to get total size
            snprintf(path, sizeof(path), "%s/index%d/shared_cpu_list", base, i);
            f = fopen(path, "r");
            if (f) {
                char shared[256];
                if (fgets(shared, sizeof(shared), f)) {
                    // Count how many CPUs share this cache
                    int count = 1;
                    for (char *p = shared; *p; p++) if (*p == ',' || *p == '-') count++;
                    // If it's a range (0-95), calculate properly
                    int start, end;
                    if (sscanf(shared, "%d-%d", &start, &end) == 2) {
                        info->l3_size = size;  // This is the total shared L3
                    } else {
                        info->l3_size = size;
                    }
                }
                fclose(f);
            } else {
                info->l3_size = size;
            }
        }
    }
    
    // Get number of cores
    FILE *f = fopen("/proc/cpuinfo", "r");
    if (f) {
        char line[256];
        int max_core_id = -1;
        while (fgets(line, sizeof(line), f)) {
            int core_id;
            if (sscanf(line, "core id : %d", &core_id) == 1) {
                if (core_id > max_core_id) max_core_id = core_id;
            }
        }
        fclose(f);
        info->num_cores = max_core_id + 1;
    }
    
    // Fallback: use sysconf
    if (info->num_cores == 0) {
        info->num_cores = sysconf(_SC_NPROCESSORS_ONLN);
    }
}
#endif

// CPUID-based cache detection (x86 fallback)
#if defined(__x86_64__) || defined(_M_X64) || defined(__i386__) || defined(_M_IX86)
static void cpuid(int leaf, int subleaf, uint32_t *eax, uint32_t *ebx, uint32_t *ecx, uint32_t *edx) {
#ifdef _WIN32
    int regs[4];
    __cpuidex(regs, leaf, subleaf);
    *eax = regs[0]; *ebx = regs[1]; *ecx = regs[2]; *edx = regs[3];
#else
    __asm__ volatile(
        "cpuid"
        : "=a"(*eax), "=b"(*ebx), "=c"(*ecx), "=d"(*edx)
        : "a"(leaf), "c"(subleaf)
    );
#endif
}

static void detect_cache_cpuid(cache_info_t *info) {
    uint32_t eax, ebx, ecx, edx;
    
    // Check if CPUID leaf 4 is supported
    cpuid(0, 0, &eax, &ebx, &ecx, &edx);
    if (eax < 4) return;
    
    for (int i = 0; i < 32; i++) {
        cpuid(4, i, &eax, &ebx, &ecx, &edx);
        
        int cache_type = eax & 0x1F;
        if (cache_type == 0) break;  // No more caches
        
        int cache_level = (eax >> 5) & 0x7;
        int line_size = (ebx & 0xFFF) + 1;
        int partitions = ((ebx >> 12) & 0x3FF) + 1;
        int ways = ((ebx >> 22) & 0x3FF) + 1;
        int sets = ecx + 1;
        
        size_t size = (size_t)line_size * partitions * ways * sets;
        
        info->line_size = line_size;
        
        if (cache_level == 1 && cache_type == 1) info->l1d_size = size;
        else if (cache_level == 1 && cache_type == 2) info->l1i_size = size;
        else if (cache_level == 2) info->l2_size = size;
        else if (cache_level == 3) info->l3_size = size;
    }
}
#endif

// Main cache detection function
static cache_info_t detect_cache_info(void) {
    cache_info_t info = {0};
    
    // Set defaults
    info.line_size = 64;
    info.l1d_size = 32 * 1024;      // 32 KB default
    info.l1i_size = 32 * 1024;
    info.l2_size = 256 * 1024;       // 256 KB default
    info.l3_size = 8 * 1024 * 1024;  // 8 MB default
    
#ifdef _WIN32
    detect_cache_windows(&info);
#elif defined(__APPLE__)
    detect_cache_macos(&info);
#elif defined(__linux__)
    detect_cache_linux(&info);
#endif
    
    // x86 fallback using CPUID
#if defined(__x86_64__) || defined(_M_X64) || defined(__i386__) || defined(_M_IX86)
    if (info.l3_size == 8 * 1024 * 1024) {  // Still default, try CPUID
        detect_cache_cpuid(&info);
    }
#endif
    
    return info;
}

static void print_cache_info(cache_info_t *info) {
    printf("════════════════════════════════════════════════════════════\n");
    printf("  Cache Hierarchy Detected\n");
    printf("════════════════════════════════════════════════════════════\n");
    printf("  L1 Data:      %7zu KB (per core)\n", info->l1d_size / 1024);
    printf("  L1 Instr:     %7zu KB (per core)\n", info->l1i_size / 1024);
    printf("  L2 Cache:     %7zu KB\n", info->l2_size / 1024);
    if (info->l3_size >= 1024 * 1024) {
        printf("  L3 Cache:     %7zu MB (shared)\n", info->l3_size / (1024 * 1024));
    } else {
        printf("  L3 Cache:     %7zu KB (shared)\n", info->l3_size / 1024);
    }
    printf("  Cache Line:   %7zu bytes\n", info->line_size);
    printf("  Physical Cores: %5d\n", info->num_cores);
    printf("════════════════════════════════════════════════════════════\n\n");
}

// ============================================================================
// Memory allocation (cross-platform)
// ============================================================================

static void *alloc_aligned(size_t alignment, size_t size) {
#ifdef _WIN32
    return _aligned_malloc(size, alignment);
#else
    void *ptr = NULL;
    if (posix_memalign(&ptr, alignment, size) != 0) return NULL;
    return ptr;
#endif
}

// ============================================================================
// Benchmark kernels
// ============================================================================

static void kernel_copy(size_t n) {
    #pragma omp parallel for simd aligned(a, c: ALIGN) schedule(static)
    for (size_t i = 0; i < n; i++) {
        c[i] = a[i];
    }
}

static void kernel_scale(size_t n, double scalar) {
    #pragma omp parallel for simd aligned(b, c: ALIGN) schedule(static)
    for (size_t i = 0; i < n; i++) {
        b[i] = scalar * c[i];
    }
}

static void kernel_add(size_t n) {
    #pragma omp parallel for simd aligned(a, b, c: ALIGN) schedule(static)
    for (size_t i = 0; i < n; i++) {
        c[i] = a[i] + b[i];
    }
}

static void kernel_triad(size_t n, double scalar) {
    #pragma omp parallel for simd aligned(a, b, c: ALIGN) schedule(static)
    for (size_t i = 0; i < n; i++) {
        a[i] = b[i] + scalar * c[i];
    }
}

static void kernel_read(size_t n, double *restrict sum_out) {
    double sum = 0.0;
    #pragma omp parallel for simd reduction(+:sum) aligned(a: ALIGN) schedule(static)
    for (size_t i = 0; i < n; i++) {
        sum += a[i];
    }
    *sum_out = sum;
}

static void kernel_write(size_t n, double val) {
    #pragma omp parallel for simd aligned(a: ALIGN) schedule(static)
    for (size_t i = 0; i < n; i++) {
        a[i] = val;
    }
}

static void kernel_memcpy(size_t n) {
    memcpy(c, a, n * sizeof(double));
}

#define MIN(x,y) ((x)<(y)?(x):(y))
#define MAX(x,y) ((x)>(y)?(x):(y))

// ============================================================================
// Main benchmark
// ============================================================================

void run_benchmark(int num_threads, size_t array_size, cache_info_t *cache) {
    omp_set_num_threads(num_threads);
    
    // Allocate aligned memory
    a = (double *)alloc_aligned(ALIGN, array_size * sizeof(double));
    b = (double *)alloc_aligned(ALIGN, array_size * sizeof(double));
    c = (double *)alloc_aligned(ALIGN, array_size * sizeof(double));
    
    if (!a || !b || !c) {
        fprintf(stderr, "Memory allocation failed\n");
        exit(1);
    }
    
    double mem_per_array = (double)(array_size * sizeof(double)) / (1024.0 * 1024.0);
    double total_mem = mem_per_array * 3;
    double l3_mb = (double)cache->l3_size / (1024.0 * 1024.0);
    
    printf("════════════════════════════════════════════════════════════\n");
    printf("  UltraMem - Memory Bandwidth Benchmark\n");
    printf("════════════════════════════════════════════════════════════\n");
    printf("  Threads:           %d\n", num_threads);
    printf("  Array elements:    %zu\n", array_size);
    printf("  Memory per array:  %.1f MB\n", mem_per_array);
    printf("  Total memory:      %.1f MB\n", total_mem);
    printf("  L3 Cache:          %.1f MB\n", l3_mb);
    printf("  Arrays vs L3:      %.1fx (", total_mem / l3_mb);
    if (total_mem > l3_mb * 4) {
        printf("DRAM test ✓)\n");
    } else if (total_mem > l3_mb) {
        printf("mostly DRAM)\n");
    } else {
        printf("⚠ fits in L3 cache!)\n");
    }
    printf("  Iterations:        %d\n", NTIMES);
    printf("════════════════════════════════════════════════════════════\n\n");

    // Initialize arrays in parallel (first touch policy)
    #pragma omp parallel for simd schedule(static)
    for (size_t i = 0; i < array_size; i++) {
        a[i] = 1.0;
        b[i] = 2.0;
        c[i] = 0.0;
    }
    
    int actual_threads = 0;
    #pragma omp parallel
    {
        #pragma omp single
        actual_threads = omp_get_num_threads();
    }
    printf("  Actual threads:    %d\n\n", actual_threads);
    
    const char *labels[] = {"Copy", "Scale", "Add", "Triad", "Read", "Write", "Memcpy"};
    double bytes[] = {
        2.0 * sizeof(double) * array_size,
        2.0 * sizeof(double) * array_size,
        3.0 * sizeof(double) * array_size,
        3.0 * sizeof(double) * array_size,
        1.0 * sizeof(double) * array_size,
        1.0 * sizeof(double) * array_size,
        2.0 * sizeof(double) * array_size,
    };
    
    double times[7][NTIMES];
    double scalar = 3.0;
    double dummy_sum = 0.0;
    
    printf("Running benchmarks...\n\n");
    
    for (int k = 0; k < NTIMES; k++) {
        times[0][k] = get_time_sec();
        kernel_copy(array_size);
        times[0][k] = get_time_sec() - times[0][k];
        
        times[1][k] = get_time_sec();
        kernel_scale(array_size, scalar);
        times[1][k] = get_time_sec() - times[1][k];
        
        times[2][k] = get_time_sec();
        kernel_add(array_size);
        times[2][k] = get_time_sec() - times[2][k];
        
        times[3][k] = get_time_sec();
        kernel_triad(array_size, scalar);
        times[3][k] = get_time_sec() - times[3][k];
        
        times[4][k] = get_time_sec();
        kernel_read(array_size, &dummy_sum);
        times[4][k] = get_time_sec() - times[4][k];
        
        times[5][k] = get_time_sec();
        kernel_write(array_size, 1.0);
        times[5][k] = get_time_sec() - times[5][k];
        
        times[6][k] = get_time_sec();
        kernel_memcpy(array_size);
        times[6][k] = get_time_sec() - times[6][k];
    }
    
    printf("────────────────────────────────────────────────────────────\n");
    printf("Kernel      Best MB/s    Avg MB/s     Min Time     Max Time\n");
    printf("────────────────────────────────────────────────────────────\n");
    
    double best_bandwidth = 0.0;
    
    for (int j = 0; j < 7; j++) {
        double avgtime = 0.0;
        double mintime = times[j][1];
        double maxtime = times[j][1];
        
        for (int k = 1; k < NTIMES; k++) {
            avgtime += times[j][k];
            mintime = MIN(mintime, times[j][k]);
            maxtime = MAX(maxtime, times[j][k]);
        }
        avgtime /= (NTIMES - 1);
        
        double best_bw = bytes[j] / mintime / 1e6;
        double avg_bw = bytes[j] / avgtime / 1e6;
        
        if (best_bw > best_bandwidth) best_bandwidth = best_bw;
        
        printf("%-8s  %10.1f  %10.1f   %10.6f   %10.6f\n",
               labels[j], best_bw, avg_bw, mintime, maxtime);
    }
    
    printf("────────────────────────────────────────────────────────────\n");
    printf("\n");
    printf("════════════════════════════════════════════════════════════\n");
    printf("  PEAK BANDWIDTH: %.1f MB/s (%.2f GB/s)\n", best_bandwidth, best_bandwidth / 1000.0);
    printf("════════════════════════════════════════════════════════════\n\n");
    
    if (dummy_sum < 0) printf("%f", dummy_sum);
    
    aligned_free(a);
    aligned_free(b);
    aligned_free(c);
}

void print_usage(const char *prog) {
    printf("Usage: %s <num_threads> [array_size_mb]\n", prog);
    printf("\nArguments:\n");
    printf("  num_threads    Number of OpenMP threads\n");
    printf("  array_size_mb  Size of each array in MB (default: 4x L3 cache)\n");
    printf("\nExamples:\n");
    printf("  %s 8           # 8 threads, auto array size (4x L3)\n", prog);
    printf("  %s 32 256      # 32 threads, 256MB arrays\n", prog);
    printf("  %s 96 1024     # 96 threads, 1GB arrays\n", prog);
}

int main(int argc, char *argv[]) {
    if (argc < 2) {
        print_usage(argv[0]);
        return 1;
    }
    
    int num_threads = atoi(argv[1]);
    if (num_threads <= 0 || num_threads > 1024) {
        fprintf(stderr, "Error: num_threads must be between 1 and 1024\n");
        return 1;
    }
    
    // Detect cache info
    cache_info_t cache = detect_cache_info();
    
    printf("\n");
    printf("System info:\n");
#ifdef _WIN32
    SYSTEM_INFO sysinfo;
    GetSystemInfo(&sysinfo);
    printf("  Platform:       Windows\n");
    printf("  Available CPUs: %d\n", sysinfo.dwNumberOfProcessors);
    printf("  Page size:      %lu bytes\n", sysinfo.dwPageSize);
#else
    printf("  Platform:       %s\n", 
#ifdef __APPLE__
           "macOS"
#else
           "Linux"
#endif
    );
    printf("  Available CPUs: %ld\n", sysconf(_SC_NPROCESSORS_ONLN));
    printf("  Page size:      %ld bytes\n", sysconf(_SC_PAGESIZE));
#endif
    printf("\n");
    
    // Print detected cache info
    print_cache_info(&cache);
    
    // Calculate array size
    size_t array_mb;
    if (argc >= 3) {
        array_mb = atol(argv[2]);
        if (array_mb < 1 || array_mb > 65536) {
            fprintf(stderr, "Error: array_size_mb must be between 1 and 65536\n");
            return 1;
        }
    } else {
        // Default: 4x L3 size to ensure we're testing DRAM, not cache
        // Minimum 128 MB per array
        size_t l3_mb = cache.l3_size / (1024 * 1024);
        array_mb = MAX(l3_mb * 4 / 3, 128);  // Divide by 3 because we have 3 arrays
        printf("  Auto array size: %zu MB (4x L3 / 3 arrays)\n\n", array_mb);
    }
    
    size_t array_size = (array_mb * 1024 * 1024) / sizeof(double);
    
    run_benchmark(num_threads, array_size, &cache);
    
    return 0;
}
