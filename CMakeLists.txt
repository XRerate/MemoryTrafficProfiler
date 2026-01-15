cmake_minimum_required(VERSION 3.13)

project(GPUMemoryFootprintProfiler VERSION 1.0.0 LANGUAGES CXX)

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

# Find QProf (only for Android builds)
if(BUILD_ADRENO_BACKEND)
    # QProf is only available for Android, so skip on desktop builds
    if(CMAKE_SYSTEM_NAME STREQUAL "Android")
        list(APPEND CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/cmake)
        find_package(QProf)
        if(NOT QProf_FOUND)
            message(WARNING "QProf not found. Adreno backend will not be available.")
            set(BUILD_ADRENO_BACKEND OFF)
        endif()
    else()
        message(STATUS "QProf is only available for Android builds. Adreno backend disabled.")
        set(BUILD_ADRENO_BACKEND OFF)
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

if(BUILD_ADRENO_BACKEND)
    list(APPEND BACKEND_SOURCES
        include/backends/adreno.h
        src/backends/adreno.cpp
    )
endif()

# Main library
add_library(GPUMemoryFootprintProfiler
    src/MemoryFootprintProfiler.cpp
    ${BACKEND_SOURCES}
)

target_include_directories(GPUMemoryFootprintProfiler
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
)

# Add compile definitions for backend availability
if(BUILD_MALI_BACKEND)
    target_compile_definitions(GPUMemoryFootprintProfiler PRIVATE BUILD_MALI_BACKEND)
endif()

if(BUILD_ADRENO_BACKEND)
    target_compile_definitions(GPUMemoryFootprintProfiler PRIVATE BUILD_ADRENO_BACKEND)
endif()

# Link libraries
if(BUILD_MALI_BACKEND)
    target_link_libraries(GPUMemoryFootprintProfiler PRIVATE ${MALI_LIBRARIES})
endif()

if(BUILD_ADRENO_BACKEND)
    target_link_libraries(GPUMemoryFootprintProfiler PRIVATE QProf::QProf)
endif()

# Threading library
target_link_libraries(GPUMemoryFootprintProfiler PUBLIC Threads::Threads)

# Build examples
option(BUILD_EXAMPLES "Build example programs" ON)
if(BUILD_EXAMPLES)
    add_subdirectory(examples)
endif()

# Install rules
install(TARGETS GPUMemoryFootprintProfiler
    EXPORT GPUMemoryFootprintProfilerTargets
    LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib
    RUNTIME DESTINATION bin
    INCLUDES DESTINATION include
)

install(FILES include/MemoryFootprintProfiler.h
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

if(BUILD_ADRENO_BACKEND)
    install(FILES include/backends/adreno.h
        DESTINATION include/backends
    )
endif()

