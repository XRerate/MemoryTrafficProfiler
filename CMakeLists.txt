cmake_minimum_required(VERSION 3.13)

project(MemoryTrafficProfiler VERSION 1.0.0 LANGUAGES CXX)

# Generate compile_commands.json for IDE support
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Find required packages
find_package(Threads REQUIRED)

# Options
option(BUILD_CPU_BACKEND "Build CPU backend (perf_event_open, Linux only)" ON)
option(BUILD_ADRENO_BACKEND "Build Adreno backend (requires QProf, Android only)" OFF)
option(BUILD_MALI_BACKEND "Build Mali backend (requires libGPUCounters)" OFF)
option(BUILD_NPU_BACKEND "Build NPU backend (requires QProf, Android only)" OFF)

# Include directories
include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

# Add libGPUCounters using FetchContent (downloads from GitHub)
if(BUILD_MALI_BACKEND)
    include(FetchContent)
    FetchContent_Declare(
        libGPUCounters
        GIT_REPOSITORY https://github.com/ARM-software/libGPUCounters.git
        GIT_TAG aa7751b8854a8093b2efdacabec480ca4d3f29b3
    )
    FetchContent_MakeAvailable(libGPUCounters)
    set(MALI_LIBRARIES device hwcpipe)
endif()

# Find QProf (only for Android builds, shared by Adreno and NPU backends)
if(BUILD_ADRENO_BACKEND OR BUILD_NPU_BACKEND)
    if(CMAKE_SYSTEM_NAME STREQUAL "Android")
        list(APPEND CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/cmake)
        find_package(QProf)
        if(NOT QProf_FOUND)
            message(WARNING "QProf not found. Adreno and NPU backends will not be available.")
            set(BUILD_ADRENO_BACKEND OFF)
            set(BUILD_NPU_BACKEND OFF)
        endif()
    else()
        message(STATUS "QProf is only available for Android builds. Adreno and NPU backends disabled.")
        set(BUILD_ADRENO_BACKEND OFF)
        set(BUILD_NPU_BACKEND OFF)
    endif()
endif()

# CPU backend only available on Linux
if(BUILD_CPU_BACKEND)
    if(NOT CMAKE_SYSTEM_NAME STREQUAL "Linux" AND NOT CMAKE_SYSTEM_NAME STREQUAL "Android")
        message(STATUS "CPU backend requires Linux. Disabled.")
        set(BUILD_CPU_BACKEND OFF)
    endif()
endif()

# Note: For desktop builds, backends may not be available
# The server can still be built but will fail to initialize if no backend is available

# Backend sources
set(BACKEND_SOURCES
    include/backends/backend.h
)

if(BUILD_MALI_BACKEND)
    list(APPEND BACKEND_SOURCES
        include/backends/mali.h
        src/backends/mali.cpp
    )
endif()

if(BUILD_ADRENO_BACKEND OR BUILD_NPU_BACKEND)
    list(APPEND BACKEND_SOURCES
        include/backends/qprof_session.h
        src/backends/qprof_session.cpp
    )
endif()

if(BUILD_ADRENO_BACKEND)
    list(APPEND BACKEND_SOURCES
        include/backends/adreno.h
        src/backends/adreno.cpp
    )
endif()

if(BUILD_CPU_BACKEND)
    list(APPEND BACKEND_SOURCES
        include/backends/cpu.h
        src/backends/cpu.cpp
    )
endif()

if(BUILD_NPU_BACKEND)
    list(APPEND BACKEND_SOURCES
        include/backends/npu.h
        src/backends/npu.cpp
    )
endif()

# Main library
add_library(MemoryTrafficProfiler
    src/MemoryTrafficProfiler.cpp
    ${BACKEND_SOURCES}
)

target_include_directories(MemoryTrafficProfiler
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
)

# Add compile definitions for backend availability
if(BUILD_MALI_BACKEND)
    target_compile_definitions(MemoryTrafficProfiler PRIVATE BUILD_MALI_BACKEND)
endif()

if(BUILD_ADRENO_BACKEND)
    target_compile_definitions(MemoryTrafficProfiler PRIVATE BUILD_ADRENO_BACKEND)
endif()

if(BUILD_CPU_BACKEND)
    target_compile_definitions(MemoryTrafficProfiler PRIVATE BUILD_CPU_BACKEND)
endif()

if(BUILD_NPU_BACKEND)
    target_compile_definitions(MemoryTrafficProfiler PRIVATE BUILD_NPU_BACKEND)
endif()

# Link libraries
if(BUILD_MALI_BACKEND)
    target_link_libraries(MemoryTrafficProfiler PRIVATE ${MALI_LIBRARIES})
endif()

if(BUILD_ADRENO_BACKEND)
    target_link_libraries(MemoryTrafficProfiler PRIVATE QProf::QProf)
endif()

if(BUILD_NPU_BACKEND)
    if(NOT BUILD_ADRENO_BACKEND)
        # Only link QProf if not already linked by Adreno
        target_link_libraries(MemoryTrafficProfiler PRIVATE QProf::QProf)
    endif()
endif()

# Threading library
target_link_libraries(MemoryTrafficProfiler PUBLIC Threads::Threads)

# Build examples
option(BUILD_EXAMPLES "Build example programs" ON)
if(BUILD_EXAMPLES)
    add_subdirectory(examples)
endif()

# Install rules
install(TARGETS MemoryTrafficProfiler
    EXPORT MemoryTrafficProfilerTargets
    LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib
    RUNTIME DESTINATION bin
    INCLUDES DESTINATION include
)

install(FILES include/MemoryTrafficProfiler.h
    DESTINATION include
)

install(FILES include/backends/backend.h
        include/backends/constants.h
    DESTINATION include/backends
)

if(BUILD_MALI_BACKEND)
    install(FILES include/backends/mali.h
        DESTINATION include/backends
    )
endif()

if(BUILD_ADRENO_BACKEND OR BUILD_NPU_BACKEND)
    install(FILES include/backends/qprof_session.h
        DESTINATION include/backends
    )
endif()

if(BUILD_ADRENO_BACKEND)
    install(FILES include/backends/adreno.h
        DESTINATION include/backends
    )
endif()

if(BUILD_CPU_BACKEND)
    install(FILES include/backends/cpu.h
        DESTINATION include/backends
    )
endif()

if(BUILD_NPU_BACKEND)
    install(FILES include/backends/npu.h
        DESTINATION include/backends
    )
endif()
