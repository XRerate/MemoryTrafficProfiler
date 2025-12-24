#pragma once

#include "backends/backend.h"
#include <memory>
#include <mutex>
#include <thread>
#include <atomic>

namespace GPUBandwidthProfiler {

/**
 * @brief Singleton GPU bandwidth profiler
 * 
 * This class provides a unified interface for profiling GPU bandwidth
 * from external memory (DRAM) on both Qualcomm Adreno and ARM Mali GPUs.
 */
class GPUBandwidthProfiler {
public:
    /**
     * @brief Get the singleton instance
     * @return Reference to the singleton instance
     */
    static GPUBandwidthProfiler& getInstance();

    /**
     * @brief Initialize the profiler with a specific backend
     * @param backend Backend to use (AdrenoBackend or MaliBackend)
     * @return true if initialization successful, false otherwise
     */
    bool initialize(std::unique_ptr<Backend> backend);

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
    GPUBandwidthProfiler() = default;
    ~GPUBandwidthProfiler();

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
    std::thread sampling_thread_;
    std::atomic<bool> should_sample_;
    std::atomic<bool> is_profiling_;
    uint32_t sampling_interval_ms_;
    mutable std::mutex mutex_;
};

} // namespace GPUBandwidthProfiler

