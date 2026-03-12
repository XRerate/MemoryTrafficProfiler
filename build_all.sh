#!/bin/bash
# Build script for Memory Footprint Profiler
# Builds library and examples

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$SCRIPT_DIR"

echo "=========================================="
echo "Memory Footprint Profiler - Build Script"
echo "=========================================="
echo ""

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

# Function to print status
print_status() {
    echo -e "${GREEN}[INFO]${NC} $1"
}

print_error() {
    echo -e "${RED}[ERROR]${NC} $1"
}

print_warning() {
    echo -e "${YELLOW}[WARN]${NC} $1"
}

# Check if Bazel is available
if ! command -v bazel &> /dev/null; then
    print_error "Bazel is not installed or not in PATH"
    exit 1
fi

print_status "Bazel version: $(bazel version | head -1)"
echo ""

# Build library and example
print_status "Building Memory Footprint Profiler library and example..."
if bazel build //:gpu_memory_footprint_profiler //examples:gpu_memory_footprint_example 2>&1 | tee /tmp/build.log | tail -5; then
    if grep -q "Build completed successfully" /tmp/build.log; then
        print_status "✓ Build successful"
        echo "  Library: bazel-bin/libgpu_memory_footprint_profiler.a"
        echo "  Example: bazel-bin/examples/gpu_memory_footprint_example"
    else
        print_error "Build failed"
        exit 1
    fi
else
    print_error "Build failed"
    exit 1
fi
echo ""

print_status "All builds completed successfully!"
echo ""

