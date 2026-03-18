#pragma once

#include <atomic>
#include <cstdint>
#include <memory>
#include <mutex>
#include <thread>

#include "backends/backend.h"

namespace memory_traffic_profiler {

/**
 * @brief Memory traffic profiler
 *
 * This class provides a unified interface for profiling GPU memory traffic
 * (total bytes read/written) from external memory (DRAM) on both Qualcomm
 * Adreno and ARM Mali GPUs.
 */
class MemoryTrafficProfiler {
 public:
  /**
   * @brief Constructor
   */
  MemoryTrafficProfiler();

  /**
   * @brief Destructor
   */
  ~MemoryTrafficProfiler();

  /**
   * @brief Initialize the profiler with available backend
   * @return true if initialization successful, false otherwise
   * @note This function automatically detects and initializes the appropriate
   * backend (tries Mali first, then Adreno). If no backend is available,
   * Initialize() will return false.
   */
  bool Initialize();

  /**
   * @brief Initialize the profiler with a specific backend category
   * @param category The backend category to use (GPU, CPU, NPU)
   * @return true if initialization successful, false otherwise
   */
  bool Initialize(BackendCategory category);

  /**
   * @brief Start memory traffic profiling
   * @return true if start successful, false otherwise
   */
  bool Start();

  /**
   * @brief Stop memory traffic profiling
   * @return true if stop successful, false otherwise
   */
  bool Stop();

  /**
   * @brief Get the total read memory traffic in bytes
   * @return Total bytes read during the profiling session
   */
  uint64_t GetReadMemoryTraffic() const;

  /**
   * @brief Get the total write memory traffic in bytes
   * @return Total bytes written during the profiling session
   */
  uint64_t GetWriteMemoryTraffic() const;

  /**
   * @brief Get the total memory traffic in bytes
   * @return Total bytes (read + write) during the profiling session
   */
  uint64_t GetTotalMemoryTraffic() const;

  /**
   * @brief Check if profiling is currently active
   * @return true if profiling is active, false otherwise
   */
  bool IsProfiling() const;

  /**
   * @brief Get the current backend name
   * @return Name of the current backend, or nullptr if not initialized
   */
  const char* GetBackendName() const;

  // Delete copy constructor and assignment operator
  MemoryTrafficProfiler(const MemoryTrafficProfiler&) = delete;
  MemoryTrafficProfiler& operator=(const MemoryTrafficProfiler&) = delete;

 private:
  /**
   * @brief Auto-initialize with available backend
   * Tries Mali backend first, then Adreno backend
   * @return true if initialization successful, false otherwise
   */
  bool autoInitialize();

  /**
   * @brief Initialize with a specific backend category
   * @param category The backend category to use
   * @return true if initialization successful, false otherwise
   */
  bool initializeByCategory(BackendCategory category);

  /**
   * @brief Initialize the profiler with a specific backend (internal)
   * @param backend Backend to use (AdrenoBackend or MaliBackend)
   * @return true if initialization successful, false otherwise
   */
  bool initialize(std::unique_ptr<Backend> backend);

  /**
   * @brief Sampling thread function to accumulate memory traffic
   */
  void samplingThread();

  std::unique_ptr<Backend> backend_;
  std::thread sampling_thread_;
  std::atomic<bool> should_sample_;
  std::atomic<bool> is_profiling_;
  std::atomic<uint64_t> accumulated_read_bytes_;
  std::atomic<uint64_t> accumulated_write_bytes_;
  uint32_t sampling_interval_ms_;
  mutable std::mutex mutex_;
};

}  // namespace memory_traffic_profiler
