#pragma once

#include <cstdint>
#include <functional>
#include <memory>

namespace MemoryTrafficProfiler {

/**
 * @brief Backend category for device type identification
 */
enum class BackendCategory { GPU, CPU, NPU };

/**
 * @brief Structure to hold bandwidth data
 */
struct BandwidthData {
  double read_bandwidth_mbps;   // Read bandwidth in MB/s
  double write_bandwidth_mbps;  // Write bandwidth in MB/s
  double total_bandwidth_mbps;  // Total bandwidth in MB/s
  uint64_t timestamp_ns;        // Timestamp in nanoseconds
};

/**
 * @brief Callback function type for bandwidth data
 */
using BandwidthCallback = std::function<void(const BandwidthData&)>;

/**
 * @brief Abstract base class for memory bandwidth backends
 */
class Backend {
 public:
  virtual ~Backend() = default;

  /**
   * @brief Initialize the backend
   * @return true if initialization successful, false otherwise
   */
  virtual bool initialize() = 0;

  /**
   * @brief Start bandwidth profiling
   * @return true if start successful, false otherwise
   */
  virtual bool start() = 0;

  /**
   * @brief Stop bandwidth profiling
   * @return true if stop successful, false otherwise
   */
  virtual bool stop() = 0;

  /**
   * @brief Sample current bandwidth data
   * @param data Output parameter to store bandwidth data
   * @return true if sampling successful, false otherwise
   */
  virtual bool sample(BandwidthData& data) = 0;

  /**
   * @brief Check if backend is currently profiling
   * @return true if profiling is active, false otherwise
   */
  virtual bool is_profiling() const = 0;

  /**
   * @brief Get backend name
   * @return Name of the backend
   */
  virtual const char* get_name() const = 0;

  /**
   * @brief Get backend category
   * @return Category of the backend (GPU, CPU, NPU)
   */
  virtual BackendCategory get_category() const = 0;
};

}  // namespace MemoryTrafficProfiler
