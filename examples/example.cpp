#include "GPUBandwidthProfiler.h"
#include <iostream>
#include <thread>
#include <chrono>
#include <iomanip>
#include <vector>
#include <algorithm>

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

    // Example 4: Using accumulator buffer
    std::cout << "Example 4: Using accumulator buffer" << std::endl;
    std::vector<GPUBandwidthProfiler::BandwidthData> data_buffer;
    profiler.registerAccumulatorBuffer(&data_buffer);
    profiler.clearAccumulatorBuffer(); // Clear any previous data
    
    std::cout << "Starting profiling with 100ms sampling interval..." << std::endl;
    if (!profiler.start(100)) {
        std::cerr << "Error: Failed to start profiling." << std::endl;
        return 1;
    }

    // Profile for 2 seconds
    std::this_thread::sleep_for(std::chrono::seconds(2));

    profiler.stop();
    
    // Retrieve accumulated data BEFORE unregistering
    auto accumulated = profiler.getAccumulatedData();
    std::cout << "Accumulated " << accumulated.size() << " samples" << std::endl;
    
    // Get statistics using the new method BEFORE unregistering
    auto stats = profiler.getAccumulatedStatistics();
    
    profiler.unregisterAccumulatorBuffer();
    
    if (stats.sample_count > 0) {
        std::cout << std::fixed << std::setprecision(4);
        std::cout << "\n=== Bandwidth Statistics ===" << std::endl;
        std::cout << "Sample Count: " << stats.sample_count << std::endl;
        std::cout << "Duration: " << std::setprecision(2) << stats.duration_sec << " seconds" << std::endl;
        std::cout << std::endl;
        
        std::cout << "Read Bandwidth:" << std::endl;
        std::cout << "  Average: " << std::setprecision(4) << stats.read_avg_mbps << " MB/s" << std::endl;
        std::cout << "  Min:     " << stats.read_min_mbps << " MB/s" << std::endl;
        std::cout << "  Max:     " << stats.read_max_mbps << " MB/s" << std::endl;
        std::cout << "  StdDev:  " << stats.read_stddev_mbps << " MB/s" << std::endl;
        std::cout << std::endl;
        
        std::cout << "Write Bandwidth:" << std::endl;
        std::cout << "  Average: " << stats.write_avg_mbps << " MB/s" << std::endl;
        std::cout << "  Min:     " << stats.write_min_mbps << " MB/s" << std::endl;
        std::cout << "  Max:     " << stats.write_max_mbps << " MB/s" << std::endl;
        std::cout << "  StdDev:  " << stats.write_stddev_mbps << " MB/s" << std::endl;
        std::cout << std::endl;
        
        std::cout << "Total Bandwidth:" << std::endl;
        std::cout << "  Average: " << stats.total_avg_mbps << " MB/s" << std::endl;
        std::cout << "  Min:     " << stats.total_min_mbps << " MB/s" << std::endl;
        std::cout << "  Max:     " << stats.total_max_mbps << " MB/s" << std::endl;
        std::cout << "  StdDev:  " << stats.total_stddev_mbps << " MB/s" << std::endl;
    }
    
    std::cout << "Profiling stopped." << std::endl;
    std::cout << std::endl;

    // Example 5: Detailed statistics analysis
    std::cout << "Example 5: Detailed Statistics Analysis" << std::endl;
    std::vector<GPUBandwidthProfiler::BandwidthData> stats_buffer;
    profiler.registerAccumulatorBuffer(&stats_buffer);
    profiler.clearAccumulatorBuffer();
    
    std::cout << "Starting profiling with 50ms sampling interval for 5 seconds..." << std::endl;
    if (!profiler.start(50)) {
        std::cerr << "Error: Failed to start profiling." << std::endl;
        return 1;
    }

    // Profile for 5 seconds to collect more data
    std::this_thread::sleep_for(std::chrono::seconds(5));

    profiler.stop();
    
    // Get comprehensive statistics BEFORE unregistering
    auto statistics = profiler.getAccumulatedStatistics();
    
    profiler.unregisterAccumulatorBuffer();
    
    std::cout << "\n=== Comprehensive Statistics Report ===" << std::endl;
    std::cout << "Total Samples Collected: " << statistics.sample_count << std::endl;
    std::cout << "Profiling Duration: " << std::fixed << std::setprecision(2) 
              << statistics.duration_sec << " seconds" << std::endl;
        if (statistics.duration_sec > 0) {
            std::cout << "Sampling Rate: " << std::setprecision(1) 
                      << (statistics.sample_count / statistics.duration_sec) << " samples/second" << std::endl;
        } else {
            std::cout << "Sampling Rate: N/A (no duration)" << std::endl;
        }
    std::cout << std::endl;
    
    if (statistics.sample_count > 0) {
        std::cout << std::fixed << std::setprecision(4);
        
        std::cout << "Read Bandwidth Statistics:" << std::endl;
        std::cout << "  Average:  " << std::setw(10) << statistics.read_avg_mbps << " MB/s" << std::endl;
        std::cout << "  Minimum:  " << std::setw(10) << statistics.read_min_mbps << " MB/s" << std::endl;
        std::cout << "  Maximum:  " << std::setw(10) << statistics.read_max_mbps << " MB/s" << std::endl;
        std::cout << "  Std Dev:  " << std::setw(10) << statistics.read_stddev_mbps << " MB/s" << std::endl;
        if (statistics.read_avg_mbps > 0) {
            double read_cv = (statistics.read_stddev_mbps / statistics.read_avg_mbps) * 100.0;
            std::cout << "  Coeff Var: " << std::setw(8) << std::setprecision(2) << read_cv << "%" << std::endl;
        }
        std::cout << std::endl;
        
        std::cout << "Write Bandwidth Statistics:" << std::endl;
        std::cout << "  Average:  " << std::setw(10) << std::setprecision(4) << statistics.write_avg_mbps << " MB/s" << std::endl;
        std::cout << "  Minimum:  " << std::setw(10) << statistics.write_min_mbps << " MB/s" << std::endl;
        std::cout << "  Maximum:  " << std::setw(10) << statistics.write_max_mbps << " MB/s" << std::endl;
        std::cout << "  Std Dev:  " << std::setw(10) << statistics.write_stddev_mbps << " MB/s" << std::endl;
        if (statistics.write_avg_mbps > 0) {
            double write_cv = (statistics.write_stddev_mbps / statistics.write_avg_mbps) * 100.0;
            std::cout << "  Coeff Var: " << std::setw(8) << std::setprecision(2) << write_cv << "%" << std::endl;
        }
        std::cout << std::endl;
        
        std::cout << "Total Bandwidth Statistics:" << std::endl;
        std::cout << "  Average:  " << std::setw(10) << statistics.total_avg_mbps << " MB/s" << std::endl;
        std::cout << "  Minimum:  " << std::setw(10) << statistics.total_min_mbps << " MB/s" << std::endl;
        std::cout << "  Maximum:  " << std::setw(10) << statistics.total_max_mbps << " MB/s" << std::endl;
        std::cout << "  Std Dev:  " << std::setw(10) << statistics.total_stddev_mbps << " MB/s" << std::endl;
        if (statistics.total_avg_mbps > 0) {
            double total_cv = (statistics.total_stddev_mbps / statistics.total_avg_mbps) * 100.0;
            std::cout << "  Coeff Var: " << std::setw(8) << std::setprecision(2) << total_cv << "%" << std::endl;
        }
        std::cout << std::endl;
        
        // Additional insights
        if (statistics.total_avg_mbps > 0) {
            std::cout << "Bandwidth Distribution:" << std::endl;
            double read_percentage = (statistics.read_avg_mbps / statistics.total_avg_mbps) * 100.0;
            double write_percentage = (statistics.write_avg_mbps / statistics.total_avg_mbps) * 100.0;
            std::cout << "  Read:  " << std::setw(6) << std::setprecision(1) << read_percentage << "%" << std::endl;
            std::cout << "  Write: " << std::setw(6) << write_percentage << "%" << std::endl;
            
            if (statistics.write_avg_mbps > 0) {
                double read_write_ratio = statistics.read_avg_mbps / statistics.write_avg_mbps;
                std::cout << "  Read/Write Ratio: " << std::setprecision(2) << read_write_ratio << std::endl;
            }
        }
        
        // Performance insights
        std::cout << std::endl;
        std::cout << "Performance Insights:" << std::endl;
        if (statistics.total_max_mbps > statistics.total_avg_mbps * 2.0) {
            std::cout << "  - High variability detected (peak " 
                      << std::setprecision(1) << (statistics.total_max_mbps / statistics.total_avg_mbps) 
                      << "x average)" << std::endl;
        }
        if (statistics.read_avg_mbps > statistics.write_avg_mbps * 2.0) {
            std::cout << "  - Read-heavy workload (reads dominate)" << std::endl;
        } else if (statistics.write_avg_mbps > statistics.read_avg_mbps * 2.0) {
            std::cout << "  - Write-heavy workload (writes dominate)" << std::endl;
        } else {
            std::cout << "  - Balanced read/write workload" << std::endl;
        }
    } else {
        std::cout << "No data collected. Statistics unavailable." << std::endl;
    }
    
    std::cout << "\nProfiling stopped." << std::endl;
    std::cout << std::endl;

    std::cout << "Example completed successfully!" << std::endl;
    return 0;
}

