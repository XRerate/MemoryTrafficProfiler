#include "GPUBandwidthProfiler.h"
#include <iostream>
#include <iomanip>
#include <chrono>

namespace GPUBandwidthProfiler {

GPUBandwidthProfiler& GPUBandwidthProfiler::getInstance() {
    static GPUBandwidthProfiler instance;
    return instance;
}

GPUBandwidthProfiler::~GPUBandwidthProfiler() {
    stop();
}

bool GPUBandwidthProfiler::initialize(std::unique_ptr<Backend> backend) {
    std::lock_guard<std::mutex> lock(mutex_);

    if (is_profiling_) {
        return false; // Cannot initialize while profiling
    }

    if (!backend) {
        return false;
    }

    if (!backend->initialize()) {
        return false;
    }

    backend_ = std::move(backend);
    callback_ = defaultCallback; // Set default callback

    return true;
}

bool GPUBandwidthProfiler::start(uint32_t sampling_interval_ms) {
    std::lock_guard<std::mutex> lock(mutex_);

    if (!backend_) {
        return false; // Not initialized
    }

    if (is_profiling_) {
        return true; // Already started
    }

    if (!backend_->start()) {
        return false;
    }

    sampling_interval_ms_ = sampling_interval_ms;
    should_sample_ = true;
    is_profiling_ = true;

    // Start sampling thread
    sampling_thread_ = std::thread(&GPUBandwidthProfiler::samplingThread, this);

    return true;
}

bool GPUBandwidthProfiler::stop() {
    {
        std::lock_guard<std::mutex> lock(mutex_);

        if (!is_profiling_) {
            return true; // Already stopped
        }

        should_sample_ = false;
        is_profiling_ = false;
    }

    // Wait for sampling thread to finish
    if (sampling_thread_.joinable()) {
        sampling_thread_.join();
    }

    // Stop backend
    if (backend_) {
        backend_->stop();
    }

    return true;
}

void GPUBandwidthProfiler::registerCallback(BandwidthCallback callback) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (callback) {
        callback_ = callback;
    } else {
        callback_ = defaultCallback;
    }
}

void GPUBandwidthProfiler::unregisterCallback() {
    std::lock_guard<std::mutex> lock(mutex_);
    callback_ = defaultCallback;
}

bool GPUBandwidthProfiler::isProfiling() const {
    return is_profiling_.load();
}

const char* GPUBandwidthProfiler::getBackendName() const {
    std::lock_guard<std::mutex> lock(mutex_);
    if (backend_) {
        return backend_->get_name();
    }
    return nullptr;
}

void GPUBandwidthProfiler::defaultCallback(const BandwidthData& data) {
    // Pretty-print the bandwidth data
    auto timestamp_sec = static_cast<double>(data.timestamp_ns) / 1e9;
    
    // Use higher precision for small values to avoid rounding to 0.00
    std::cout << std::fixed << std::setprecision(4);
    std::cout << "[GPU Bandwidth] "
              << "Time: " << std::setw(10) << std::setprecision(2) << timestamp_sec << "s | "
              << "Read: " << std::setw(10) << std::setprecision(4) << data.read_bandwidth_mbps << " MB/s | "
              << "Write: " << std::setw(10) << std::setprecision(4) << data.write_bandwidth_mbps << " MB/s | "
              << "Total: " << std::setw(10) << std::setprecision(4) << data.total_bandwidth_mbps << " MB/s"
              << std::endl;
}

void GPUBandwidthProfiler::samplingThread() {
    BandwidthData data;

    while (should_sample_.load()) {
        {
            std::lock_guard<std::mutex> lock(mutex_);
            
            if (backend_ && backend_->is_profiling()) {
                if (backend_->sample(data)) {
                    // Call the registered callback
                    if (callback_) {
                        callback_(data);
                    }
                }
            }
        }

        // Sleep for the sampling interval
        std::this_thread::sleep_for(std::chrono::milliseconds(sampling_interval_ms_));
    }
}

} // namespace GPUBandwidthProfiler

