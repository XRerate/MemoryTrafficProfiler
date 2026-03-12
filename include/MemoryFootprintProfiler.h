#pragma once

#include <atomic>
#include <cstdint>
#include <memory>
#include <mutex>
#include <thread>

#include "backends/backend.h"

namespace MemoryTrafficProfiler {

/**
 * @brief Memory footprint profiler
 *
 * This class provides a unified interface for profiling GPU memory footprint
 * (total bytes read/written) from external memory (DRAM) on both Qualcomm
 * Adreno and ARM Mali GPUs.
 */
class MemoryFootprintProfiler {
 public:
  /**
   * @brief Constructor
   */
  MemoryFootprintProfiler();

  /**
   * @brief Destructor
   */
  ~MemoryFootprintProfiler();

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
   * @brief Start memory footprint profiling
   * @return true if start successful, false otherwise
   */
  bool Start();

  /**
   * @brief Stop memory footprint profiling
   * @return true if stop successful, false otherwise
   */
  bool Stop();

  /**
   * @brief Get the total read memory footprint in bytes
   * @return Total bytes read during the profiling session
   */
  uint64_t GetReadMemoryFootprint() const;

  /**
   * @brief Get the total write memory footprint in bytes
   * @return Total bytes written during the profiling session
   */
  uint64_t GetWriteMemoryFootprint() const;

  /**
   * @brief Get the total memory footprint in bytes
   * @return Total bytes (read + write) during the profiling session
   */
  uint64_t GetTotalMemoryFootprint() const;

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
  MemoryFootprintProfiler(const MemoryFootprintProfiler&) = delete;
  MemoryFootprintProfiler& operator=(const MemoryFootprintProfiler&) = delete;

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
   * @brief Sampling thread function to accumulate memory footprint
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

}  // namespace MemoryTrafficProfiler
