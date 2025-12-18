# UltraMem - High Performance Memory Bandwidth Benchmark
# https://github.com/amirmnoohi/ultramem

CC = gcc
CFLAGS = -O3 -march=native -fopenmp -ffast-math -funroll-loops -ftree-vectorize
LDFLAGS = -fopenmp
TARGET = ultramem
SRC = ultramem.c

# Detect OS
UNAME_S := $(shell uname -s)
ifeq ($(UNAME_S),Darwin)
    # macOS - use Homebrew's gcc for OpenMP support
    CC = gcc-13
    ifeq ($(shell which gcc-13 2>/dev/null),)
        CC = gcc-12
    endif
    ifeq ($(shell which gcc-12 2>/dev/null),)
        $(warning No Homebrew GCC found, trying clang with libomp)
        CC = clang
        CFLAGS = -O3 -Xpreprocessor -fopenmp -ffast-math
        LDFLAGS = -lomp
    endif
endif

.PHONY: all clean install uninstall help test

all: $(TARGET)

$(TARGET): $(SRC)
	$(CC) $(CFLAGS) -o $@ $< $(LDFLAGS)

# Debug build with symbols
debug: CFLAGS = -O0 -g -fopenmp -Wall -Wextra
debug: $(TARGET)

# Static build for portability
static: LDFLAGS += -static
static: $(TARGET)

clean:
	rm -f $(TARGET) *.o

install: $(TARGET)
	install -m 755 $(TARGET) /usr/local/bin/

uninstall:
	rm -f /usr/local/bin/$(TARGET)

# Run quick test
test: $(TARGET)
	@echo "Running quick test with 4 threads..."
	./$(TARGET) 4

# Run full benchmark
bench: $(TARGET)
	@echo "Running full benchmark..."
	@for t in 1 2 4 8 16 32; do \
		echo "\n=== $$t threads ==="; \
		./$(TARGET) $$t 256 2>&1 | grep -E "(PEAK|Copy|Triad)"; \
	done

help:
	@echo "UltraMem - Memory Bandwidth Benchmark"
	@echo ""
	@echo "Targets:"
	@echo "  all      - Build ultramem (default)"
	@echo "  debug    - Build with debug symbols"
	@echo "  static   - Build static binary"
	@echo "  clean    - Remove built files"
	@echo "  install  - Install to /usr/local/bin"
	@echo "  uninstall- Remove from /usr/local/bin"
	@echo "  test     - Run quick test"
	@echo "  bench    - Run scaling benchmark"
	@echo "  help     - Show this help"
	@echo ""
	@echo "Usage after build:"
	@echo "  ./ultramem <threads> [array_size_mb]"
	@echo ""
	@echo "Examples:"
	@echo "  ./ultramem 8          # 8 threads, auto array size"
	@echo "  ./ultramem 32 1024    # 32 threads, 1GB arrays"

