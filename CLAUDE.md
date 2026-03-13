# CLAUDE.md

## Project Overview

Memory Traffic Profiler: C++ library measuring DRAM read/write bytes on Android devices.
Backends: CPU (perf_event_open), Adreno GPU (QProf), Mali GPU (libGPUCounters), NPU (QProf).

## Build & Test

- Build systems: CMake (primary), Bazel
- Android build: `mkdir -p build-android && cd build-android && cmake .. -DCMAKE_TOOLCHAIN_FILE=$ANDROID_NDK/build/cmake/android.toolchain.cmake -DANDROID_ABI=arm64-v8a -DANDROID_PLATFORM=android-21 -DBUILD_CPU_BACKEND=ON -DBUILD_ADRENO_BACKEND=ON -DBUILD_MALI_BACKEND=ON -DBUILD_NPU_BACKEND=ON -DCMAKE_BUILD_TYPE=Debug && make -j$(nproc) && cd ..`
- CMake defaults: CPU=ON, Adreno/Mali/NPU=OFF
- Test on Android device using `examples/memory_traffic_example` after code changes

## Code Conventions

- C++14 standard
- Keep it simple: no unnecessary abstractions

## After Any Code Change

- Update all affected files: README.md, CMakeLists.txt, BUILD.bazel, MODULE.bazel, examples/, etc.
- If build options change, update both CMake and Bazel configs
