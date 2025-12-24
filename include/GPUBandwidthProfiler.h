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
    std::thread sampling_thread_;
    std::atomic<bool> should_sample_;
    std::atomic<bool> is_profiling_;
    uint32_t sampling_interval_ms_;
    mutable std::mutex mutex_;
};

} // namespace GPUBandwidthProfiler

