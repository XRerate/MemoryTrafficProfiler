cmake_minimum_required(VERSION 3.13)

# Example executable
add_executable(memory_traffic_example
    example.cpp
)

target_link_libraries(memory_traffic_example
    PRIVATE
        MemoryTrafficProfiler
)

# Set C++ standard
set_target_properties(memory_traffic_example PROPERTIES
    CXX_STANDARD 14
    CXX_STANDARD_REQUIRED ON
)

# Add compile definition for available backends
if(BUILD_ADRENO_BACKEND)
    target_compile_definitions(memory_traffic_example PRIVATE BUILD_ADRENO_BACKEND)
endif()

if(BUILD_MALI_BACKEND)
    target_compile_definitions(memory_traffic_example PRIVATE BUILD_MALI_BACKEND)
endif()

if(BUILD_CPU_BACKEND)
    target_compile_definitions(memory_traffic_example PRIVATE BUILD_CPU_BACKEND)
endif()

if(BUILD_NPU_BACKEND)
    target_compile_definitions(memory_traffic_example PRIVATE BUILD_NPU_BACKEND)
endif()

# CPU memory bandwidth stress test
add_executable(cpu_memstress
    cpu_memstress.cpp
)

set_target_properties(cpu_memstress PROPERTIES
    CXX_STANDARD 14
    CXX_STANDARD_REQUIRED ON
)
