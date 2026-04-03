#include <cstring>
#include <iomanip>
#include <iostream>
#include <thread>
#include <vector>

#include "MemoryTrafficProfiler.h"

using namespace memory_traffic_profiler;

static const char* categoryLabel(BackendCategory c) {
  switch (c) {
    case BackendCategory::GPU:
      return "GPU";
    case BackendCategory::CPU:
      return "CPU";
    case BackendCategory::NPU:
      return "NPU";
  }
  return "?";
}

static const char* findNameForCategory(const MemoryTrafficProfiler& p,
                                       BackendCategory c) {
  for (size_t i = 0; i < p.GetBackendCount(); ++i) {
    if (p.GetBackendCategory(i) == c) {
      return p.GetBackendName(i);
    }
  }
  return nullptr;
}

void printUsage(const char* program) {
  std::cout << "Usage: " << program << " [backend ...]" << std::endl;
  std::cout << "  backend: gpu, cpu, npu, all" << std::endl;
  std::cout << "  default: auto (one GPU vendor if available, plus CPU and NPU)"
            << std::endl;
  std::cout << "  Multiple backends can be combined, e.g.: cpu gpu" << std::endl;
}

static bool appendBackend(const char* name, std::vector<BackendCategory>* out) {
  if (strcmp(name, "gpu") == 0) {
    out->push_back(BackendCategory::GPU);
    return true;
  }
  if (strcmp(name, "cpu") == 0) {
    out->push_back(BackendCategory::CPU);
    return true;
  }
  if (strcmp(name, "npu") == 0) {
    out->push_back(BackendCategory::NPU);
    return true;
  }
  if (strcmp(name, "all") == 0) {
    out->push_back(BackendCategory::GPU);
    out->push_back(BackendCategory::CPU);
    out->push_back(BackendCategory::NPU);
    return true;
  }
  return false;
}

int main(int argc, char* argv[]) {
  std::cout << "Memory Traffic Profiler Example" << std::endl;
  std::cout << "======================================" << std::endl
            << std::endl;

  MemoryTrafficProfiler p;

  std::vector<BackendCategory> categories;
  for (int i = 1; i < argc; ++i) {
    if (strcmp(argv[i], "--help") == 0 || strcmp(argv[i], "-h") == 0) {
      printUsage(argv[0]);
      return 0;
    }
    if (!appendBackend(argv[i], &categories)) {
      std::cerr << "Unknown backend: " << argv[i] << std::endl;
      printUsage(argv[0]);
      return 1;
    }
  }

  bool initialized = false;
  if (categories.empty()) {
    std::cout << "Auto-detecting backends..." << std::endl;
    initialized = p.Initialize();
  } else {
    std::cout << "Requesting selected backend(s)..." << std::endl;
    initialized = p.Initialize(categories);
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

  std::cout << "Successfully initialized " << p.GetBackendCount()
            << " backend(s):" << std::endl;
  for (size_t i = 0; i < p.GetBackendCount(); ++i) {
    const char* name = p.GetBackendName(i);
    if (name) {
      std::cout << "  [" << i << "] " << name << std::endl;
    }
  }
  std::cout << std::endl;

  std::cout << "Starting profiling..." << std::endl;
  if (!p.Start()) {
    std::cerr << "Error: Failed to start profiling." << std::endl;
    return 1;
  }

  {
    std::cout << "Running target code for 1 second..." << std::endl;
    std::this_thread::sleep_for(std::chrono::seconds(1));
  }

  p.Stop();
  std::cout << "Profiling stopped." << std::endl;
  std::cout << std::endl;

  std::cout << std::fixed << std::setprecision(2);
  const BackendCategory kOrder[] = {
      BackendCategory::GPU,
      BackendCategory::CPU,
      BackendCategory::NPU,
  };
  uint64_t sum_read = 0;
  uint64_t sum_write = 0;
  for (BackendCategory c : kOrder) {
    if (!p.HasBackendCategory(c)) {
      continue;
    }
    uint64_t r = p.GetReadMemoryTraffic(c);
    uint64_t w = p.GetWriteMemoryTraffic(c);
    sum_read += r;
    sum_write += w;
    const char* name = findNameForCategory(p, c);
    std::cout << "=== " << categoryLabel(c) << " / "
              << (name ? name : "?") << " ===" << std::endl;
    std::cout << "Read Memory Traffic:  " << std::setw(15)
              << r / (1024.0 * 1024.0) << " MB (" << r << " bytes)"
              << std::endl;
    std::cout << "Write Memory Traffic: " << std::setw(15)
              << w / (1024.0 * 1024.0) << " MB (" << w << " bytes)"
              << std::endl;
    std::cout << "Total (read+write):   " << std::setw(15)
              << p.GetTotalMemoryTraffic(c) / (1024.0 * 1024.0) << " MB"
              << std::endl
              << std::endl;
  }

  std::cout << "=== Sum across all backends ===" << std::endl;
  std::cout << "Read Memory Traffic:  " << std::setw(15)
            << sum_read / (1024.0 * 1024.0) << " MB (" << sum_read
            << " bytes)" << std::endl;
  std::cout << "Write Memory Traffic: " << std::setw(15)
            << sum_write / (1024.0 * 1024.0) << " MB (" << sum_write
            << " bytes)" << std::endl;
  std::cout << "Total Memory Traffic: " << std::setw(15)
            << (sum_read + sum_write) / (1024.0 * 1024.0) << " MB ("
            << (sum_read + sum_write) << " bytes)" << std::endl;

  std::cout << std::endl;
  std::cout << "Example completed successfully!" << std::endl;
  return 0;
}
