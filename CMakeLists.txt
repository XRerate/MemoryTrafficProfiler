cmake_minimum_required(VERSION 3.13)

project(GPUBandwidthProfiler VERSION 1.0.0 LANGUAGES CXX)

# Generate compile_commands.json for IDE support
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Find required packages
find_package(Threads REQUIRED)

# Options
option(BUILD_ADRENO_BACKEND "Build Adreno backend (requires QProf)" ON)
option(BUILD_MALI_BACKEND "Build Mali backend (requires libGPUCounters)" ON)

# Include directories
include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

# Add libGPUCounters subdirectory
if(BUILD_MALI_BACKEND)
    add_subdirectory(third_party/libGPUCounters)
    set(MALI_LIBRARIES device hwcpipe)
endif()

# Find QProf
if(BUILD_ADRENO_BACKEND)
    list(APPEND CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/cmake)
    find_package(QProf)
    if(NOT QProf_FOUND)
        message(WARNING "QProf not found. Adreno backend will not be available.")
        set(BUILD_ADRENO_BACKEND OFF)
    endif()
endif()

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

if(BUILD_ADRENO_BACKEND)
    list(APPEND BACKEND_SOURCES
        include/backends/adreno.h
        src/backends/adreno.cpp
    )
endif()

# Main library
add_library(GPUBandwidthProfiler
    src/GPUBandwidthProfiler.cpp
    ${BACKEND_SOURCES}
)

target_include_directories(GPUBandwidthProfiler
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
)

# Add compile definitions for backend availability
if(BUILD_MALI_BACKEND)
    target_compile_definitions(GPUBandwidthProfiler PRIVATE BUILD_MALI_BACKEND)
endif()

if(BUILD_ADRENO_BACKEND)
    target_compile_definitions(GPUBandwidthProfiler PRIVATE BUILD_ADRENO_BACKEND)
endif()

# Link libraries
if(BUILD_MALI_BACKEND)
    target_link_libraries(GPUBandwidthProfiler PRIVATE ${MALI_LIBRARIES})
endif()

if(BUILD_ADRENO_BACKEND)
    target_link_libraries(GPUBandwidthProfiler PRIVATE QProf::QProf)
endif()

# Threading library
target_link_libraries(GPUBandwidthProfiler PUBLIC Threads::Threads)

# Build examples
option(BUILD_EXAMPLES "Build example programs" ON)
if(BUILD_EXAMPLES)
    add_subdirectory(examples)
endif()

# Install rules
install(TARGETS GPUBandwidthProfiler
    EXPORT GPUBandwidthProfilerTargets
    LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib
    RUNTIME DESTINATION bin
    INCLUDES DESTINATION include
)

install(FILES include/GPUBandwidthProfiler.h
    DESTINATION include
)

install(FILES include/backends/backend.h
    DESTINATION include/backends
)

if(BUILD_MALI_BACKEND)
    install(FILES include/backends/mali.h
        DESTINATION include/backends
    )
endif()

if(BUILD_ADRENO_BACKEND)
    install(FILES include/backends/adreno.h
        DESTINATION include/backends
    )
endif()

