# GPU Bandwidth Profiler

A cross-platform GPU bandwidth profiler for Qualcomm Adreno and ARM Mali GPUs. This profiler provides real-time monitoring of GPU external memory (DRAM) bandwidth with support for custom callbacks and pretty-printing.

## Features

- **Singleton Design**: Easy-to-use singleton interface
- **Dual Backend Support**: 
  - Qualcomm Adreno GPU (using QProf API)
  - ARM Mali GPU (using libGPUCounters)
- **Callback Mechanism**: Register custom callbacks for bandwidth data
- **Default Pretty-Print**: Built-in formatted output for quick monitoring
- **Cross-Platform**: Supports Android devices via NDK cross-compilation
- **Accumulator Buffer**: Collect bandwidth samples into a user-provided buffer
- **Statistics**: Calculate comprehensive statistics (min, max, avg, stddev) from accumulated data

## Build Systems

This project supports both **CMake** and **Bazel** build systems:

- **CMake**: See build instructions below
- **Bazel**: See [README_BAZEL.md](README_BAZEL.md) for Bazel-specific instructions

## Prerequisites

### Common Requirements

- **CMake** (version 3.13 or higher)
- **C++14** compatible compiler
- **Android NDK** (for Android builds)
  - Set `ANDROID_NDK` environment variable to your NDK path
  - Example: `export ANDROID_NDK=/path/to/android-ndk-r25c`

### Qualcomm Adreno Backend Requirements

The Adreno backend requires the Qualcomm Profiler (QProf) API. You need to set the following environment variables:

#### Required Environment Variables

1. **`QPROF_HOME`** (Optional, defaults to `/opt/qcom/Shared/QualcommProfiler/API`)
   - Path to the QProf API directory
   - Example: `export QPROF_HOME=/opt/qcom/Shared/QualcommProfiler/API`
   - This directory should contain:
     - `include/` - Header files (e.g., `QProfilerApi.h`)
     - `target-la/aarch64/libs/` - Library files for Android aarch64

2. **`QPROF_LIBRARY`** (Optional, auto-detected if not set)
   - Full path to the QProf library file
   - Example: `export QPROF_LIBRARY=/opt/qcom/Shared/QualcommProfiler/API/target-la/aarch64/libs/libQualcommProfilerApi.so`
   - If not set, CMake will search in `$QPROF_HOME/target-la/aarch64/libs/`

#### QProf Directory Structure

The QProf installation should have the following structure:
```
/opt/qcom/Shared/QualcommProfiler/
└── API/
    ├── include/
    │   └── QProfilerApi.h
    └── target-la/
        └── aarch64/
            └── libs/
                ├── libQualcommProfilerApi.so
                └── libQualcommProfilerCore.so
```

**Note**: The default path is `/opt/qcom/Shared/QualcommProfiler/API`. If your QProf is installed elsewhere, set `QPROF_HOME` to point to the `API` directory (not the parent directory).

#### Setting Up QProf Environment

Add to your `~/.bashrc` or `~/.zshrc`:
```bash
# Qualcomm QProf Configuration
# Only set if QProf is not installed in the default location
export QPROF_HOME=/opt/qcom/Shared/QualcommProfiler/API

# Optional: specify library path explicitly
# export QPROF_LIBRARY=$QPROF_HOME/target-la/aarch64/libs/libQualcommProfilerApi.so
```

### ARM Mali Backend Requirements

The Mali backend uses `libGPUCounters` which is automatically downloaded from GitHub during the build process. No additional environment variables or manual setup is required.

## Building

### Clone the Repository

```bash
git clone https://github.com/XRerate/GPUBandwidthProfiler.git
cd GPUBandwidthProfiler
```

**Note:** Dependencies (libGPUCounters) are automatically downloaded during the build process, so no submodule initialization is needed.

### Build Options

The project supports the following CMake options:

- `BUILD_ADRENO_BACKEND` (default: ON) - Build Adreno backend (requires QProf)
- `BUILD_MALI_BACKEND` (default: ON) - Build Mali backend (requires libGPUCounters)
- `BUILD_EXAMPLES` (default: ON) - Build example applications

### Building for Android

#### Using Build Scripts

We provide convenience scripts for specific devices:

**For Google Pixel 8 Pro (Mali GPU):**
```bash
./build-pixel8pro.sh
```

**For Samsung Galaxy S25 (Adreno GPU):**
```bash
./build-s25.sh
```

These scripts will:
1. Check for required environment variables
2. Configure CMake for Android cross-compilation
3. Build the project
4. Provide instructions for deployment

#### Manual Android Build

```bash
mkdir build-android
cd build-android

cmake .. \
    -DCMAKE_TOOLCHAIN_FILE=$ANDROID_NDK/build/cmake/android.toolchain.cmake \
    -DANDROID_ABI=arm64-v8a \
    -DANDROID_PLATFORM=android-21 \
    -DBUILD_ADRENO_BACKEND=ON \
    -DBUILD_MALI_BACKEND=ON

make -j$(nproc)
```

### Building for Linux (Native)

```bash
mkdir build
cd build

cmake .. \
    -DBUILD_ADRENO_BACKEND=ON \
    -DBUILD_MALI_BACKEND=ON

make -j$(nproc)
```

**Note**: Native Linux builds are primarily for development. The profiler is designed for Android devices.

## Deployment to Android Device

### Deploy Executable

```bash
adb push build-android/examples/gpu_bandwidth_example /data/local/tmp/
adb shell chmod +x /data/local/tmp/gpu_bandwidth_example
```

### Deploy QProf Libraries (Adreno Backend Only)

For Adreno devices, you also need to push the QProf libraries:

```bash
# Set QPROF_HOME if not already set (or use default path)
QPROF_HOME=${QPROF_HOME:-/opt/qcom/Shared/QualcommProfiler/API}

# Push QProf API library
adb push $QPROF_HOME/target-la/aarch64/libs/libQualcommProfilerApi.so /data/local/tmp/

# Push QProf Core library (dependency)
adb push $QPROF_HOME/target-la/aarch64/libs/libQualcommProfilerCore.so /data/local/tmp/
```

**Note**: The build scripts (e.g., `build-s25.sh`) will automatically provide the correct push commands based on your `QPROF_HOME` setting.

### Run on Device

```bash
adb shell /data/local/tmp/gpu_bandwidth_example
```

## Usage

### Basic Example

```cpp
#include "GPUBandwidthProfiler.h"
#include "backends/mali.h"  // or "backends/adreno.h"

using namespace GPUBandwidthProfiler;

int main() {
    auto& profiler = GPUBandwidthProfiler::getInstance();
    
    // Initialize with Mali backend
    if (!profiler.initialize(std::make_unique<MaliBackend>())) {
        std::cerr << "Failed to initialize profiler" << std::endl;
        return -1;
    }
    
    // Start profiling with 200ms sampling interval
    profiler.start(200);
    
    // ... your code here ...
    
    // Stop profiling
    profiler.stop();
    
    return 0;
}
```

### Custom Callback

```cpp
void myCallback(const BandwidthData& data) {
    std::cout << "Read: " << data.read_bandwidth_mbps 
              << " MB/s, Write: " << data.write_bandwidth_mbps 
              << " MB/s" << std::endl;
}

// Register custom callback
profiler.registerCallback(myCallback);
profiler.start(100);
```

## Troubleshooting

### QProf Not Found

If CMake reports that QProf is not found:

1. Verify `QPROF_HOME` is set (or check default location):
   ```bash
   echo $QPROF_HOME
   # Default is: /opt/qcom/Shared/QualcommProfiler/API
   ```

2. Check that the directory exists and contains the required structure:
   ```bash
   ls -la $QPROF_HOME/include/QProfilerApi.h
   ls -la $QPROF_HOME/target-la/aarch64/libs/libQualcommProfilerApi.so
   ```

3. If using a custom library path, set `QPROF_LIBRARY`:
   ```bash
   export QPROF_LIBRARY=/path/to/libQualcommProfilerApi.so
   ```

4. Ensure you're building for Android aarch64 (arm64-v8a), as QProf only supports Android ARM64 targets.

### Adreno Backend Shows 0.00 MB/s

- Ensure the device has an Adreno GPU (not Mali)
- Check that QProf libraries are pushed to the device
- Verify the device has the required permissions to access GPU profiling

### Mali Backend Shows 0.00 MB/s

- Ensure the device has a Mali GPU (not Adreno)
- Check that the device has `/dev/mali0` or similar GPU device node
- Verify the device has the required permissions to access GPU counters

### Dependency Issues

If you encounter issues with `libGPUCounters`:

**For CMake builds:**
- libGPUCounters is automatically downloaded via FetchContent
- If download fails, check your internet connection
- You can clear the cache: `rm -rf build-*/_deps/libgpucounters-*`

**For Bazel builds:**
- libGPUCounters is fetched from the local registry
- Ensure the registry is properly configured in `.bazelrc`

## Project Structure

```
GPUBandwidthProfiler/
├── include/
│   ├── GPUBandwidthProfiler.h      # Main profiler interface
│   └── backends/
│       ├── backend.h               # Abstract backend interface
│       ├── adreno.h                # Adreno backend header
│       └── mali.h                  # Mali backend header
├── src/
│   ├── GPUBandwidthProfiler.cpp   # Main profiler implementation
│   └── backends/
│       ├── adreno.cpp              # Adreno backend implementation
│       └── mali.cpp                # Mali backend implementation
├── examples/
│   └── example.cpp                 # Example usage
├── third_party/
│   └── modules/                    # Bazel registry modules (libgpu_counters, etc.)
├── cmake/
│   └── FindQProf.cmake            # CMake module for finding QProf
├── build-pixel8pro.sh              # Build script for Pixel 8 Pro
├── build-s25.sh                    # Build script for Galaxy S25
└── CMakeLists.txt                 # Main CMake configuration
```

## License

[Add your license information here]

## Contributing

[Add contribution guidelines here]

## Acknowledgments

- ARM Software for libGPUCounters
- Qualcomm for QProf API

