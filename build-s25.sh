#!/bin/bash
# Build script for Samsung Galaxy S25 (Adreno GPU)

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BUILD_DIR="${SCRIPT_DIR}/build-android-s25"
DEVICE_SERIAL="R3CX80PSH7N"

echo "=========================================="
echo "Building GPU Bandwidth Profiler"
echo "Target: Samsung Galaxy S25 (Adreno GPU)"
echo "=========================================="
echo ""

# Check for Android NDK
if [ -z "$ANDROID_NDK" ]; then
    echo "Error: ANDROID_NDK environment variable is not set"
    echo "Please set it to your Android NDK path, e.g.:"
    echo "  export ANDROID_NDK=/path/to/android-ndk-r25c"
    exit 1
fi

if [ ! -f "$ANDROID_NDK/build/cmake/android.toolchain.cmake" ]; then
    echo "Error: Android NDK toolchain not found at $ANDROID_NDK"
    exit 1
fi

# Check for QProf
if [ -z "$QPROF_HOME" ]; then
    QPROF_HOME="/opt/qcom/Shared/QualcommProfiler/API"
fi

if [ ! -d "$QPROF_HOME" ]; then
    echo "Error: QProf not found at $QPROF_HOME"
    echo "Please set QPROF_HOME environment variable or install QProf"
    exit 1
fi

# Check if device is connected
if ! adb devices | grep -q "$DEVICE_SERIAL"; then
    echo "Warning: Device $DEVICE_SERIAL (Samsung S25) not found in adb devices"
    echo "Continuing with build anyway..."
fi

# Create build directory
mkdir -p "$BUILD_DIR"
cd "$BUILD_DIR"

# Configure CMake
echo "Configuring CMake..."
cmake "$SCRIPT_DIR" \
    -DCMAKE_TOOLCHAIN_FILE="$ANDROID_NDK/build/cmake/android.toolchain.cmake" \
    -DANDROID_ABI=arm64-v8a \
    -DANDROID_PLATFORM=android-21 \
    -DBUILD_ADRENO_BACKEND=ON \
    -DBUILD_MALI_BACKEND=OFF \
    -DBUILD_EXAMPLES=ON

# Build
echo ""
echo "Building..."
make -j$(nproc)

echo ""
echo "=========================================="
echo "Build completed successfully!"
echo "=========================================="
echo ""
echo "To deploy and run on device:"
echo "  # Push executable"
echo "  adb -s $DEVICE_SERIAL push $BUILD_DIR/examples/gpu_bandwidth_example /data/local/tmp/"
echo ""
echo "  # Push QProf libraries"
echo "  adb -s $DEVICE_SERIAL push $QPROF_HOME/target-la/aarch64/libs/libQualcommProfilerApi.so /data/local/tmp/"
echo "  adb -s $DEVICE_SERIAL push $QPROF_HOME/target-la/aarch64/libs/libQualcommProfilerCore.so /data/local/tmp/"
echo ""
echo "  # Run with library path"
echo "  adb -s $DEVICE_SERIAL shell 'export LD_LIBRARY_PATH=/data/local/tmp:\$LD_LIBRARY_PATH && /data/local/tmp/gpu_bandwidth_example'"
echo ""

