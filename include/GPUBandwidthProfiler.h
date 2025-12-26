#pragma once

#include "backends/backend.h"
#include <memory>
#include <mutex>
#include <thread>
#include <atomic>
#include <vector>

namespace GPUBandwidthProfiler {

/**
 * @brief Statistics structure for accumulated bandwidth data
 */
struct BandwidthStatistics {
    size_t sample_count;              // Number of samples
    
    // Read bandwidth statistics
    double read_avg_mbps;            // Average read bandwidth
    double read_min_mbps;             // Minimum read bandwidth
    double read_max_mbps;             // Maximum read bandwidth
    double read_stddev_mbps;          // Standard deviation of read bandwidth
    
    // Write bandwidth statistics
    double write_avg_mbps;            // Average write bandwidth
    double write_min_mbps;             // Minimum write bandwidth
    double write_max_mbps;             // Maximum write bandwidth
    double write_stddev_mbps;          // Standard deviation of write bandwidth
    
    // Total bandwidth statistics
    double total_avg_mbps;            // Average total bandwidth
    double total_min_mbps;             // Minimum total bandwidth
    double total_max_mbps;             // Maximum total bandwidth
    double total_stddev_mbps;          // Standard deviation of total bandwidth
    
    // Time range
    uint64_t first_timestamp_ns;      // First sample timestamp
    uint64_t last_timestamp_ns;       // Last sample timestamp
    double duration_sec;              // Duration in seconds
};

/**
 * @brief Singleton GPU bandwidth profiler
 * 
 * This class provides a unified interface for profiling GPU bandwidth
 * from external memory (DRAM) on both Qualcomm Adreno and ARM Mali GPUs.
 */
class GPUBandwidthProfiler {
public:
    /**
     * @brief Get the singleton instance and auto-initialize with available backend
     * @return Reference to the singleton instance
     * @note This function automatically detects and initializes the appropriate backend
     *       (tries Mali first, then Adreno). If no backend is available, the profiler
     *       will not be initialized and start() will return false.
     */
    static GPUBandwidthProfiler& getInstance();

    /**
     * @brief Start bandwidth profiling
     * @param sampling_interval_ms Sampling interval in milliseconds (default: 100ms)
     * @return true if start successful, false otherwise
     */
    bool start(uint32_t sampling_interval_ms = 100);

    /**
     * @brief Stop bandwidth profiling
     * @return true if stop successful, false otherwise
     */
    bool stop();

    /**
     * @brief Register a callback function to be called when bandwidth data is retrieved
     * @param callback Callback function to register
     */
    void registerCallback(BandwidthCallback callback);

    /**
     * @brief Unregister the current callback (revert to default)
     */
    void unregisterCallback();

    /**
     * @brief Register a buffer to accumulate bandwidth data
     * @param buffer Pointer to vector that will store accumulated BandwidthData
     * @note The buffer must remain valid while profiling is active.
     *       Data is appended to the buffer in a thread-safe manner.
     */
    void registerAccumulatorBuffer(std::vector<BandwidthData>* buffer);

    /**
     * @brief Unregister the accumulator buffer
     */
    void unregisterAccumulatorBuffer();

    /**
     * @brief Clear the accumulator buffer
     * @note This requires the accumulator buffer to be registered
     */
    void clearAccumulatorBuffer();

    /**
     * @brief Get a copy of accumulated data from the buffer
     * @return Vector containing accumulated BandwidthData
     * @note Returns empty vector if no accumulator buffer is registered
     */
    std::vector<BandwidthData> getAccumulatedData() const;

    /**
     * @brief Calculate statistics from the accumulated buffer
     * @return BandwidthStatistics structure with calculated statistics
     * @note Returns statistics with sample_count=0 if buffer is empty or not registered
     */
    BandwidthStatistics getAccumulatedStatistics() const;

    /**
     * @brief Check if profiling is currently active
     * @return true if profiling is active, false otherwise
     */
    bool isProfiling() const;

    /**
     * @brief Get the current backend name
     * @return Name of the current backend, or nullptr if not initialized
     */
    const char* getBackendName() const;

    // Delete copy constructor and assignment operator
    GPUBandwidthProfiler(const GPUBandwidthProfiler&) = delete;
    GPUBandwidthProfiler& operator=(const GPUBandwidthProfiler&) = delete;

private:
    GPUBandwidthProfiler();
    ~GPUBandwidthProfiler();

    /**
     * @brief Auto-initialize with available backend
     * Tries Mali backend first, then Adreno backend
     */
    void autoInitialize();

    /**
     * @brief Initialize the profiler with a specific backend (internal)
     * @param backend Backend to use (AdrenoBackend or MaliBackend)
     * @return true if initialization successful, false otherwise
     */
    bool initialize(std::unique_ptr<Backend> backend);

    /**
     * @brief Default callback that pretty-prints bandwidth data
     */
    static void defaultCallback(const BandwidthData& data);

    /**
     * @brief Sampling thread function
     */
    void samplingThread();

    std::unique_ptr<Backend> backend_;
    BandwidthCallback callback_;
    std::vector<BandwidthData>* accumulator_buffer_;
    std::thread sampling_thread_;
    std::atomic<bool> should_sample_;
    std::atomic<bool> is_profiling_;
    uint32_t sampling_interval_ms_;
    mutable std::mutex mutex_;
    mutable std::mutex accumulator_mutex_;
};

} // namespace GPUBandwidthProfiler

