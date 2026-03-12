# Memory Footprint Profiler

A cross-platform memory footprint profiler for Android devices. Measures total bytes read/written from external memory (DRAM) during a profiling session. Currently supports GPU backends (Qualcomm Adreno, ARM Mali), with CPU and NPU backends planned.

## Features

- **Dual Backend Support**:
  - Qualcomm Adreno GPU (using QProf API)
  - ARM Mali GPU (using libGPUCounters)
- **Auto-Detection**: Automatically detects and initializes the appropriate backend (tries Mali first, then Adreno)
- **Memory Footprint Accumulation**: Tracks cumulative read/write bytes via background sampling thread
- **Cross-Platform**: Supports Android devices via NDK cross-compilation
- **Dual Build System**: Both CMake and Bazel supported

## Prerequisites

### Common Requirements

- **C++14** compatible compiler
- **Android NDK** (for Android builds)
  - Set `ANDROID_NDK` environment variable to your NDK path
- **CMake** (version 3.13 or higher) for CMake builds
- **Bazel** for Bazel builds

### Qualcomm Adreno Backend Requirements

The Adreno backend requires the Qualcomm Profiler (QProf) API.

#### Environment Variables

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

### ARM Mali Backend Requirements

The Mali backend uses `libGPUCounters` which is automatically downloaded during the build process. No additional setup is required.

## Building

### Build Options

**CMake options:**

- `BUILD_ADRENO_BACKEND` (default: ON) - Build Adreno backend (requires QProf)
- `BUILD_MALI_BACKEND` (default: ON) - Build Mali backend (requires libGPUCounters)
- `BUILD_EXAMPLES` (default: ON) - Build example applications

**Bazel configs (defined in `.bazelrc`):**

- `--config=android_arm64` - Android ARM64 platform
- `--config=android_mali` - Mali backend on Android
- `--config=android_adreno` - Adreno backend on Android

### Build with CMake (Android)

```bash
mkdir build-android && cd build-android

cmake .. \
    -DCMAKE_TOOLCHAIN_FILE=$ANDROID_NDK/build/cmake/android.toolchain.cmake \
    -DANDROID_ABI=arm64-v8a \
    -DANDROID_PLATFORM=android-21 \
    -DBUILD_ADRENO_BACKEND=ON \
    -DBUILD_MALI_BACKEND=ON

make -j$(nproc)
```

### Build with CMake (Linux Native)

```bash
mkdir build && cd build
cmake .. -DBUILD_ADRENO_BACKEND=ON -DBUILD_MALI_BACKEND=ON
make -j$(nproc)
```

**Note**: Native Linux builds are primarily for development. The profiler is designed for Android devices.

### Build with Bazel

```bash
# Build library only
bazel build --config=android_mali //:gpu_memory_footprint_profiler

# Build library and example
bazel build --config=android_mali //:gpu_memory_footprint_profiler //examples:gpu_memory_footprint_example
```

Or use the convenience script:
```bash
./build_all.sh
```

## Deployment to Android Device

### Deploy Executable

```bash
adb push build-android/examples/gpu_memory_footprint_example /data/local/tmp/
adb shell chmod +x /data/local/tmp/gpu_memory_footprint_example
```

### Deploy QProf Libraries (Adreno Backend Only)

```bash
QPROF_HOME=${QPROF_HOME:-/opt/qcom/Shared/QualcommProfiler/API}
adb push $QPROF_HOME/target-la/aarch64/libs/libQualcommProfilerApi.so /data/local/tmp/
adb push $QPROF_HOME/target-la/aarch64/libs/libQualcommProfilerCore.so /data/local/tmp/
```

### Run on Device

```bash
adb shell "cd /data/local/tmp && LD_LIBRARY_PATH=/data/local/tmp ./gpu_memory_footprint_example"
```

## Usage

### Basic Example

```cpp
#include "MemoryFootprintProfiler.h"

int main() {
    // Create profiler instance
    GPUMemoryFootprintProfiler::MemoryFootprintProfiler p;

    // Initialize (auto-detects backend: tries Mali first, then Adreno)
    if (!p.Initialize()) {
        std::cerr << "Failed to initialize profiler" << std::endl;
        return 1;
    }

    std::cout << "Using backend: " << p.GetBackendName() << std::endl;

    // Start profiling
    p.Start();

    // ... your code here ...
    std::this_thread::sleep_for(std::chrono::seconds(3));

    // Stop profiling
    p.Stop();

    // Get accumulated memory footprint
    uint64_t read_bytes = p.GetReadMemoryFootprint();
    uint64_t write_bytes = p.GetWriteMemoryFootprint();
    uint64_t total_bytes = p.GetTotalMemoryFootprint();

    std::cout << "Read:  " << read_bytes / (1024.0 * 1024.0) << " MB" << std::endl;
    std::cout << "Write: " << write_bytes / (1024.0 * 1024.0) << " MB" << std::endl;
    std::cout << "Total: " << total_bytes / (1024.0 * 1024.0) << " MB" << std::endl;

    return 0;
}
```

### API Reference

| Method | Description |
|--------|-------------|
| `Initialize()` | Auto-detect and initialize the appropriate GPU backend |
| `Start()` | Start memory footprint profiling (background sampling thread) |
| `Stop()` | Stop profiling |
| `GetReadMemoryFootprint()` | Get total bytes read from DRAM |
| `GetWriteMemoryFootprint()` | Get total bytes written to DRAM |
| `GetTotalMemoryFootprint()` | Get total bytes (read + write) |
| `IsProfiling()` | Check if profiling is currently active |
| `GetBackendName()` | Get the name of the active backend |

## Troubleshooting

### QProf Not Found

1. Verify `QPROF_HOME` is set (or check default location):
   ```bash
   echo $QPROF_HOME
   # Default: /opt/qcom/Shared/QualcommProfiler/API
   ```

2. Check the directory structure:
   ```bash
   ls -la $QPROF_HOME/include/QProfilerApi.h
   ls -la $QPROF_HOME/target-la/aarch64/libs/libQualcommProfilerApi.so
   ```

3. Ensure you're building for Android aarch64 (arm64-v8a), as QProf only supports Android ARM64.

### Adreno Backend Shows 0 Bytes

- Ensure the device has an Adreno GPU (not Mali)
- Check that QProf libraries are pushed to the device
- Verify `LD_LIBRARY_PATH` includes the directory containing QProf libraries
- Verify the device has the required permissions to access GPU profiling

### Mali Backend Shows 0 Bytes

- Ensure the device has a Mali GPU (not Adreno)
- Check that the device has `/dev/mali0` or similar GPU device node
- Verify the device has the required permissions to access GPU counters

### Dependency Issues

**CMake:** libGPUCounters is auto-downloaded via FetchContent. If download fails, check your internet connection or clear cache: `rm -rf build-*/_deps/libgpucounters-*`

**Bazel:** libGPUCounters is fetched from the local registry configured in `.bazelrc`.

## Project Structure

```
MemoryTrafficProfiler/
├── include/
│   ├── MemoryFootprintProfiler.h      # Main profiler interface
│   └── backends/
│       ├── backend.h                   # Abstract backend interface
│       ├── constants.h                 # Unit conversion constants
│       ├── adreno.h                    # Adreno backend header
│       └── mali.h                      # Mali backend header
├── src/
│   ├── MemoryFootprintProfiler.cpp     # Main profiler implementation
│   └── backends/
│       ├── adreno.cpp                  # Adreno backend implementation
│       └── mali.cpp                    # Mali backend implementation
├── examples/
│   ├── example.cpp                     # Example usage
│   ├── CMakeLists.txt                  # Example CMake config
│   └── BUILD.bazel                     # Example Bazel config
├── python/                             # Python client package
├── cmake/
│   └── FindQProf.cmake                 # CMake module for finding QProf
├── bazel/                              # Bazel dependency configs
├── third_party/                        # Local Bazel registry modules
├── CMakeLists.txt                      # Main CMake configuration
├── BUILD.bazel                         # Main Bazel configuration
├── MODULE.bazel                        # Bazel module definition
├── .bazelrc                            # Bazel settings and configs
└── build_all.sh                        # Convenience build script
```

## Acknowledgments

- ARM Software for libGPUCounters
- Qualcomm for QProf API
