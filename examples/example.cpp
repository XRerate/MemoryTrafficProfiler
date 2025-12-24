#include "GPUBandwidthProfiler.h"
#include <iostream>
#include <thread>
#include <chrono>
#include <iomanip>

// Custom callback example
void customCallback(const GPUBandwidthProfiler::BandwidthData& data) {
    std::cout << "Custom Callback: "
              << "Read=" << std::fixed << std::setprecision(4) << data.read_bandwidth_mbps << " MB/s, "
              << "Write=" << data.write_bandwidth_mbps << " MB/s, "
              << "Total=" << data.total_bandwidth_mbps << " MB/s"
              << std::endl;
}

int main(int argc, char* argv[]) {
    std::cout << "GPU Bandwidth Profiler Example" << std::endl;
    std::cout << "===============================" << std::endl << std::endl;

    // getInstance() automatically initializes with available backend
    auto& profiler = GPUBandwidthProfiler::GPUBandwidthProfiler::getInstance();

    // Check if initialization was successful
    if (!profiler.getBackendName()) {
        std::cerr << "Error: Failed to initialize any backend." << std::endl;
        std::cerr << "This may be because:" << std::endl;
        std::cerr << "  - No GPU device is available" << std::endl;
        std::cerr << "  - Required libraries (libGPUCounters or QProf) are not available" << std::endl;
        std::cerr << "  - Insufficient permissions to access GPU device" << std::endl;
        return 1;
    }

    std::cout << "Successfully initialized " << profiler.getBackendName() << " backend!" << std::endl;
    std::cout << std::endl;

    // Example 1: Use default callback (pretty-print)
    std::cout << "Example 1: Using default callback (pretty-print)" << std::endl;
    std::cout << "Starting profiling with 200ms sampling interval..." << std::endl;
    
    if (!profiler.start(200)) {
        std::cerr << "Error: Failed to start profiling." << std::endl;
        return 1;
    }

    // Profile for 3 seconds
    std::this_thread::sleep_for(std::chrono::seconds(3));

    profiler.stop();
    std::cout << "Profiling stopped." << std::endl;
    std::cout << std::endl;

    // Example 2: Use custom callback
    std::cout << "Example 2: Using custom callback" << std::endl;
    profiler.registerCallback(customCallback);
    std::cout << "Starting profiling with 150ms sampling interval..." << std::endl;

    if (!profiler.start(150)) {
        std::cerr << "Error: Failed to start profiling." << std::endl;
        return 1;
    }

    // Profile for 3 seconds
    std::this_thread::sleep_for(std::chrono::seconds(3));

    profiler.stop();
    std::cout << "Profiling stopped." << std::endl;
    std::cout << std::endl;

    // Example 3: Revert to default callback
    std::cout << "Example 3: Reverting to default callback" << std::endl;
    profiler.unregisterCallback();
    std::cout << "Starting profiling with 100ms sampling interval..." << std::endl;

    if (!profiler.start(100)) {
        std::cerr << "Error: Failed to start profiling." << std::endl;
        return 1;
    }

    // Profile for 2 seconds
    std::this_thread::sleep_for(std::chrono::seconds(2));

    profiler.stop();
    std::cout << "Profiling stopped." << std::endl;
    std::cout << std::endl;

    std::cout << "Example completed successfully!" << std::endl;
    return 0;
}

