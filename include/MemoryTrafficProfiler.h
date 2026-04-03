#pragma once

#include <atomic>
#include <cstdint>
#include <memory>
#include <mutex>
#include <thread>
#include <vector>

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
   * @brief Initialize the profiler with all available backends
   * @return true if at least one backend initialized successfully
   * @note Tries one GPU vendor (Mali, then Adreno), then CPU, then NPU,
   * adding each that succeeds so traffic is aggregated across devices.
   */
  bool Initialize();

  /**
   * @brief Initialize the profiler with a specific backend category
   * @param category The backend category to use (GPU, CPU, NPU)
   * @return true if initialization successful, false otherwise
   */
  bool Initialize(BackendCategory category);

  /**
   * @brief Initialize with multiple backend categories (one instance per
   * category; duplicate categories are ignored)
   * @param categories Empty list behaves like Initialize()
   * @return true if at least one backend initialized successfully
   */
  bool Initialize(const std::vector<BackendCategory>& categories);

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
   * @brief Read traffic for all backends with this category
   * @note If multiple backends share the category, their counters are summed
   */
  uint64_t GetReadMemoryTraffic(BackendCategory category) const;

  /**
   * @brief Write traffic for all backends with this category
   */
  uint64_t GetWriteMemoryTraffic(BackendCategory category) const;

  /**
   * @brief Read + write for all backends with this category
   */
  uint64_t GetTotalMemoryTraffic(BackendCategory category) const;

  /**
   * @return Whether a backend for this category was successfully initialized
   */
  bool HasBackendCategory(BackendCategory category) const;

  /**
   * @brief Category of the backend at this index (for pairing with GetBackendName)
   * @pre index < GetBackendCount()
   */
  BackendCategory GetBackendCategory(size_t index) const;

  /**
   * @brief Check if profiling is currently active
   * @return true if profiling is active, false otherwise
   */
  bool IsProfiling() const;

  /**
   * @brief Number of active backends
   */
  size_t GetBackendCount() const;

  /**
   * @param index Backend index (0 .. GetBackendCount()-1)
   * @return Backend name, or nullptr if not initialized / index out of range
   */
  const char* GetBackendName(size_t index = 0) const;

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

  /** @pre mutex_ is held by caller */
  bool hasBackendCategoryUnlocked(BackendCategory category) const;

  /**
   * @brief Sampling thread function to accumulate memory traffic
   */
  void samplingThread();

  std::vector<std::unique_ptr<Backend>> backends_;
  std::vector<uint64_t> per_backend_read_bytes_;
  std::vector<uint64_t> per_backend_write_bytes_;
  std::thread sampling_thread_;
  std::atomic<bool> should_sample_;
  std::atomic<bool> is_profiling_;
  uint32_t sampling_interval_ms_;
  mutable std::mutex mutex_;
};

}  // namespace memory_traffic_profiler
