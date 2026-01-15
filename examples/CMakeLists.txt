cmake_minimum_required(VERSION 3.13)

# Example executable
add_executable(gpu_memory_footprint_example
    example.cpp
)

target_link_libraries(gpu_memory_footprint_example
    PRIVATE
        GPUMemoryFootprintProfiler
)

# Set C++ standard
set_target_properties(gpu_memory_footprint_example PROPERTIES
    CXX_STANDARD 14
    CXX_STANDARD_REQUIRED ON
)

# Add compile definition for available backends
if(BUILD_ADRENO_BACKEND)
    target_compile_definitions(gpu_memory_footprint_example PRIVATE BUILD_ADRENO_BACKEND)
endif()

if(BUILD_MALI_BACKEND)
    target_compile_definitions(gpu_memory_footprint_example PRIVATE BUILD_MALI_BACKEND)
endif()

