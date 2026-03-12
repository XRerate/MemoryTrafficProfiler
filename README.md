# Memory Traffic Profiler

A cross-platform memory traffic profiler for Android devices. Measures total bytes read/written from external memory (DRAM) during a profiling session. Supports GPU (Qualcomm Adreno, ARM Mali), CPU, and NPU (Qualcomm HTP) backends.

## Features

- **Multi-Backend Support**:
  - Qualcomm Adreno GPU (using QProf API)
  - ARM Mali GPU (using libGPUCounters)
  - CPU (using ARMv8 PMU `bus_access` counter via `perf_event_open`)
  - Qualcomm NPU/HTP (using QProf AXI bandwidth metrics)
- **Auto-Detection**: Automatically detects and initializes the appropriate backend (tries Mali → Adreno → CPU → NPU)
- **Backend Selection**: Optionally specify a backend category (GPU, CPU, NPU)
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

### Qualcomm Adreno / NPU Backend Requirements

Both the Adreno GPU and NPU backends require the Qualcomm Profiler (QProf) API.

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

### CPU Backend Requirements

The CPU backend uses the ARMv8 PMU `bus_access` event counter via `perf_event_open`. Requirements:

- **Linux or Android** (uses Linux kernel syscall)
- **Root access** or `perf_event_paranoid` set to allow access
- **ARMv8 PMU** support on the target device

No external library dependencies.

## Building

### Build Options

**CMake options:**

- `BUILD_ADRENO_BACKEND` (default: ON) - Build Adreno backend (requires QProf, Android only)
- `BUILD_MALI_BACKEND` (default: ON) - Build Mali backend (requires libGPUCounters)
- `BUILD_CPU_BACKEND` (default: ON) - Build CPU backend (Linux/Android only)
- `BUILD_NPU_BACKEND` (default: ON) - Build NPU backend (requires QProf, Android only)
- `BUILD_EXAMPLES` (default: ON) - Build example applications

**Bazel configs (defined in `.bazelrc`):**

- `--config=android_arm64` - Android ARM64 platform
- `--config=android_mali` - Mali backend on Android
- `--config=android_adreno` - Adreno backend on Android
- `--config=android_cpu` - CPU backend on Android
- `--config=android_npu` - NPU backend on Android

### Build with CMake (Android)

```bash
mkdir build-android && cd build-android

cmake .. \
    -DCMAKE_TOOLCHAIN_FILE=$ANDROID_NDK/build/cmake/android.toolchain.cmake \
    -DANDROID_ABI=arm64-v8a \
    -DANDROID_PLATFORM=android-21 \
    -DBUILD_ADRENO_BACKEND=ON \
    -DBUILD_MALI_BACKEND=ON \
    -DBUILD_CPU_BACKEND=ON \
    -DBUILD_NPU_BACKEND=ON

make -j$(nproc)
```

### Build with CMake (Linux Native)

```bash
mkdir build && cd build
cmake .. -DBUILD_CPU_BACKEND=ON
make -j$(nproc)
```

**Note**: Native Linux builds are primarily for development. GPU and NPU backends require Android.

### Build with Bazel

```bash
# Build library only
bazel build --config=android_adreno //:memory_traffic_profiler

# Build with multiple backends
bazel build --config=android_adreno --config=build_cpu_backend //:memory_traffic_profiler

# Build library and example
bazel build --config=android_adreno //:memory_traffic_profiler //examples:memory_traffic_example
```

Or use the convenience script:
```bash
./build_all.sh
```

## Deployment to Android Device

### Deploy Executable

```bash
adb push build-android/examples/memory_traffic_example /data/local/tmp/
adb shell chmod +x /data/local/tmp/memory_traffic_example
```

### Deploy QProf Libraries (Adreno / NPU Backend Only)

```bash
QPROF_HOME=${QPROF_HOME:-/opt/qcom/Shared/QualcommProfiler/API}
adb push $QPROF_HOME/target-la/aarch64/libs/libQualcommProfilerApi.so /data/local/tmp/
adb push $QPROF_HOME/target-la/aarch64/libs/libQualcommProfilerCore.so /data/local/tmp/
```

### Run on Device

```bash
adb shell "cd /data/local/tmp && LD_LIBRARY_PATH=/data/local/tmp ./memory_traffic_example"
```

## Usage

### Basic Example (Auto-Detection)

```cpp
#include "MemoryFootprintProfiler.h"

int main() {
    // Create profiler instance
    MemoryTrafficProfiler::MemoryFootprintProfiler p;

    // Initialize (auto-detects backend: tries Mali → Adreno → CPU → NPU)
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

### Backend Selection Example

```cpp
#include "MemoryFootprintProfiler.h"

using namespace MemoryTrafficProfiler;

int main() {
    MemoryFootprintProfiler p;

    // Initialize with a specific backend category
    if (!p.Initialize(BackendCategory::CPU)) {
        std::cerr << "Failed to initialize CPU backend" << std::endl;
        return 1;
    }

    // ... same profiling API as above ...
}
```

### API Reference

| Method | Description |
|--------|-------------|
| `Initialize()` | Auto-detect and initialize the appropriate backend (Mali → Adreno → CPU → NPU) |
| `Initialize(BackendCategory)` | Initialize with a specific backend category (`GPU`, `CPU`, `NPU`) |
| `Start()` | Start memory footprint profiling (background sampling thread) |
| `Stop()` | Stop profiling |
| `GetReadMemoryFootprint()` | Get total bytes read from DRAM |
| `GetWriteMemoryFootprint()` | Get total bytes written to DRAM |
| `GetTotalMemoryFootprint()` | Get total bytes (read + write) |
| `IsProfiling()` | Check if profiling is currently active |
| `GetBackendName()` | Get the name of the active backend |

### Backend Details

| Backend | Device | Category | Metrics | Notes |
|---------|--------|----------|---------|-------|
| Mali | ARM Mali GPU | GPU | Read/Write BW via libGPUCounters | |
| Adreno | Qualcomm Adreno GPU | GPU | Read/Write BW via QProf | Requires QProf libraries on device |
| CPU | ARMv8 CPU | CPU | Combined R+W via `bus_access` PMU | Reports total in read field; write = 0 (not separable) |
| NPU | Qualcomm HTP/Hexagon | NPU | Read/Write BW via QProf AXI counters | Requires QProf libraries on device |

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

### CPU Backend Fails to Initialize

- Ensure running on Linux or Android with ARMv8 processor
- Check `perf_event_paranoid` setting: `cat /proc/sys/kernel/perf_event_paranoid`
  - Value must be ≤ 3, or run as root
- On some devices, root access is required for PMU counters

### NPU Backend Shows 0 Bytes

- Ensure the device has a Qualcomm HTP/Hexagon DSP
- Check that QProf libraries are pushed to the device
- Verify `LD_LIBRARY_PATH` includes the directory containing QProf libraries
- Ensure a workload is actually running on the NPU during profiling

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
│       ├── adreno.h                    # Adreno GPU backend header
│       ├── mali.h                      # Mali GPU backend header
│       ├── cpu.h                       # CPU backend header
│       └── npu.h                       # NPU backend header
├── src/
│   ├── MemoryFootprintProfiler.cpp     # Main profiler implementation
│   └── backends/
│       ├── adreno.cpp                  # Adreno GPU backend (QProf)
│       ├── mali.cpp                    # Mali GPU backend (libGPUCounters)
│       ├── cpu.cpp                     # CPU backend (perf_event_open)
│       └── npu.cpp                     # NPU backend (QProf AXI)
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
- [android-bwprobe](https://github.com/seonjunn/android-bwprobe) for CPU bus_access measurement approach
