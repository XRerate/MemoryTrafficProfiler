#include <cstring>
#include <iomanip>
#include <iostream>
#include <thread>

#include "MemoryFootprintProfiler.h"

using namespace MemoryTrafficProfiler;

void printUsage(const char* program) {
  std::cout << "Usage: " << program << " [backend]" << std::endl;
  std::cout << "  backend: gpu, cpu, npu (default: auto-detect)" << std::endl;
}

int main(int argc, char* argv[]) {
  std::cout << "Memory Traffic Profiler Example" << std::endl;
  std::cout << "======================================" << std::endl
            << std::endl;

  // Create profiler instance
  MemoryFootprintProfiler p;

  // Initialize profiler
  bool initialized = false;
  if (argc > 1) {
    if (strcmp(argv[1], "gpu") == 0) {
      std::cout << "Requesting GPU backend..." << std::endl;
      initialized = p.Initialize(BackendCategory::GPU);
    } else if (strcmp(argv[1], "cpu") == 0) {
      std::cout << "Requesting CPU backend..." << std::endl;
      initialized = p.Initialize(BackendCategory::CPU);
    } else if (strcmp(argv[1], "npu") == 0) {
      std::cout << "Requesting NPU backend..." << std::endl;
      initialized = p.Initialize(BackendCategory::NPU);
    } else if (strcmp(argv[1], "--help") == 0 || strcmp(argv[1], "-h") == 0) {
      printUsage(argv[0]);
      return 0;
    } else {
      std::cerr << "Unknown backend: " << argv[1] << std::endl;
      printUsage(argv[0]);
      return 1;
    }
  } else {
    std::cout << "Auto-detecting backend..." << std::endl;
    initialized = p.Initialize();
  }

  if (!initialized) {
    std::cerr << "Error: Failed to initialize profiler." << std::endl;
    std::cerr << "This may be because:" << std::endl;
    std::cerr << "  - No supported device is available" << std::endl;
    std::cerr
        << "  - Required libraries (libGPUCounters or QProf) are not available"
        << std::endl;
    std::cerr << "  - Insufficient permissions to access device counters"
              << std::endl;
    return 1;
  }

  std::cout << "Successfully initialized " << p.GetBackendName()
            << " backend!" << std::endl;
  std::cout << std::endl;

  // Example: Profile target code
  std::cout << "Starting profiling..." << std::endl;
  if (!p.Start()) {
    std::cerr << "Error: Failed to start profiling." << std::endl;
    return 1;
  }

  {
    // Target code to profile
    std::cout << "Running target code for 1 second..." << std::endl;
    std::this_thread::sleep_for(std::chrono::seconds(1));
  }

  p.Stop();
  std::cout << "Profiling stopped." << std::endl;
  std::cout << std::endl;

  // Get memory footprint
  uint64_t read_footprint = p.GetReadMemoryFootprint();
  uint64_t write_footprint = p.GetWriteMemoryFootprint();
  uint64_t total_footprint = p.GetTotalMemoryFootprint();

  std::cout << "=== Memory Footprint ===" << std::endl;
  std::cout << std::fixed << std::setprecision(2);
  std::cout << "Read Memory Footprint:  " << std::setw(15)
            << read_footprint / (1024.0 * 1024.0) << " MB ("
            << read_footprint << " bytes)" << std::endl;
  std::cout << "Write Memory Footprint: " << std::setw(15)
            << write_footprint / (1024.0 * 1024.0) << " MB ("
            << write_footprint << " bytes)" << std::endl;
  std::cout << "Total Memory Footprint: " << std::setw(15)
            << total_footprint / (1024.0 * 1024.0) << " MB ("
            << total_footprint << " bytes)" << std::endl;

  std::cout << std::endl;
  std::cout << "Example completed successfully!" << std::endl;
  return 0;
}
