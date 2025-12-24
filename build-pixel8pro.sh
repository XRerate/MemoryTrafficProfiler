#!/bin/bash
# Build script for Google Pixel 8 Pro (Mali GPU)

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BUILD_DIR="${SCRIPT_DIR}/build-android-pixel8pro"
DEVICE_SERIAL="38031FDJG005TQ"

echo "=========================================="
echo "Building GPU Bandwidth Profiler"
echo "Target: Google Pixel 8 Pro (Mali GPU)"
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

# Check if device is connected
if ! adb devices | grep -q "$DEVICE_SERIAL"; then
    echo "Warning: Device $DEVICE_SERIAL (Pixel 8 Pro) not found in adb devices"
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
    -DBUILD_ADRENO_BACKEND=OFF \
    -DBUILD_MALI_BACKEND=ON \
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
echo "  adb -s $DEVICE_SERIAL push $BUILD_DIR/examples/gpu_bandwidth_example /data/local/tmp/"
echo "  adb -s $DEVICE_SERIAL shell /data/local/tmp/gpu_bandwidth_example"
echo ""

