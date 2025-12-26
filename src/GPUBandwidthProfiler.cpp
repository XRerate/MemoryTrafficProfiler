#include "GPUBandwidthProfiler.h"
#ifdef BUILD_MALI_BACKEND
#include "backends/mali.h"
#endif
#ifdef BUILD_ADRENO_BACKEND
#include "backends/adreno.h"
#endif
#include <iostream>
#include <iomanip>
#include <chrono>
#include <cmath>
#include <algorithm>

namespace GPUBandwidthProfiler {

GPUBandwidthProfiler::GPUBandwidthProfiler() 
    : callback_(defaultCallback),
      accumulator_buffer_(nullptr),
      should_sample_(false), 
      is_profiling_(false),
      sampling_interval_ms_(100) {
    autoInitialize();
}

GPUBandwidthProfiler& GPUBandwidthProfiler::getInstance() {
    static GPUBandwidthProfiler instance;
    return instance;
}

void GPUBandwidthProfiler::autoInitialize() {
    // Try to initialize with available backends
    // Try Mali first, then Adreno
    
#ifdef BUILD_MALI_BACKEND
    auto mali_backend = std::make_unique<MaliBackend>();
    if (initialize(std::move(mali_backend))) {
        return; // Successfully initialized with Mali
    }
#endif

#ifdef BUILD_ADRENO_BACKEND
    auto adreno_backend = std::make_unique<AdrenoBackend>();
    if (initialize(std::move(adreno_backend))) {
        return; // Successfully initialized with Adreno
    }
#endif

    // No backend available - backend_ remains nullptr
    // start() will return false if backend_ is nullptr
}

GPUBandwidthProfiler::~GPUBandwidthProfiler() {
    stop();
}

bool GPUBandwidthProfiler::initialize(std::unique_ptr<Backend> backend) {
    // This is now a private method - only called internally
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

    // Make sure any previous thread is joined before starting a new one
    if (sampling_thread_.joinable()) {
        sampling_thread_.join();
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

void GPUBandwidthProfiler::registerAccumulatorBuffer(std::vector<BandwidthData>* buffer) {
    std::lock_guard<std::mutex> lock(accumulator_mutex_);
    accumulator_buffer_ = buffer;
}

void GPUBandwidthProfiler::unregisterAccumulatorBuffer() {
    std::lock_guard<std::mutex> lock(accumulator_mutex_);
    accumulator_buffer_ = nullptr;
}

void GPUBandwidthProfiler::clearAccumulatorBuffer() {
    std::lock_guard<std::mutex> lock(accumulator_mutex_);
    if (accumulator_buffer_) {
        accumulator_buffer_->clear();
    }
}

std::vector<BandwidthData> GPUBandwidthProfiler::getAccumulatedData() const {
    std::lock_guard<std::mutex> lock(accumulator_mutex_);
    if (accumulator_buffer_) {
        return *accumulator_buffer_; // Return a copy
    }
    return std::vector<BandwidthData>(); // Return empty vector if no buffer registered
}

BandwidthStatistics GPUBandwidthProfiler::getAccumulatedStatistics() const {
    BandwidthStatistics stats = {};
    
    std::lock_guard<std::mutex> lock(accumulator_mutex_);
    
    if (!accumulator_buffer_) {
        return stats; // Return empty statistics if buffer not registered
    }
    
    const auto& data = *accumulator_buffer_;
    stats.sample_count = data.size();
    
    if (stats.sample_count == 0) {
        return stats; // Return empty statistics if no samples
    }
    
    // Initialize min/max with first sample
    stats.read_min_mbps = data[0].read_bandwidth_mbps;
    stats.read_max_mbps = data[0].read_bandwidth_mbps;
    stats.write_min_mbps = data[0].write_bandwidth_mbps;
    stats.write_max_mbps = data[0].write_bandwidth_mbps;
    stats.total_min_mbps = data[0].total_bandwidth_mbps;
    stats.total_max_mbps = data[0].total_bandwidth_mbps;
    
    // Calculate sums and find min/max
    double read_sum = 0.0;
    double write_sum = 0.0;
    double total_sum = 0.0;
    
    stats.first_timestamp_ns = data[0].timestamp_ns;
    stats.last_timestamp_ns = data[0].timestamp_ns;
    
    for (const auto& sample : data) {
        read_sum += sample.read_bandwidth_mbps;
        write_sum += sample.write_bandwidth_mbps;
        total_sum += sample.total_bandwidth_mbps;
        
        stats.read_min_mbps = std::min(stats.read_min_mbps, sample.read_bandwidth_mbps);
        stats.read_max_mbps = std::max(stats.read_max_mbps, sample.read_bandwidth_mbps);
        stats.write_min_mbps = std::min(stats.write_min_mbps, sample.write_bandwidth_mbps);
        stats.write_max_mbps = std::max(stats.write_max_mbps, sample.write_bandwidth_mbps);
        stats.total_min_mbps = std::min(stats.total_min_mbps, sample.total_bandwidth_mbps);
        stats.total_max_mbps = std::max(stats.total_max_mbps, sample.total_bandwidth_mbps);
        
        if (sample.timestamp_ns < stats.first_timestamp_ns) {
            stats.first_timestamp_ns = sample.timestamp_ns;
        }
        if (sample.timestamp_ns > stats.last_timestamp_ns) {
            stats.last_timestamp_ns = sample.timestamp_ns;
        }
    }
    
    // Calculate averages
    stats.read_avg_mbps = read_sum / stats.sample_count;
    stats.write_avg_mbps = write_sum / stats.sample_count;
    stats.total_avg_mbps = total_sum / stats.sample_count;
    
    // Calculate standard deviations
    if (stats.sample_count > 1) {
        double read_variance = 0.0;
        double write_variance = 0.0;
        double total_variance = 0.0;
        
        for (const auto& sample : data) {
            double read_diff = sample.read_bandwidth_mbps - stats.read_avg_mbps;
            double write_diff = sample.write_bandwidth_mbps - stats.write_avg_mbps;
            double total_diff = sample.total_bandwidth_mbps - stats.total_avg_mbps;
            
            read_variance += read_diff * read_diff;
            write_variance += write_diff * write_diff;
            total_variance += total_diff * total_diff;
        }
        
        stats.read_stddev_mbps = std::sqrt(read_variance / (stats.sample_count - 1));
        stats.write_stddev_mbps = std::sqrt(write_variance / (stats.sample_count - 1));
        stats.total_stddev_mbps = std::sqrt(total_variance / (stats.sample_count - 1));
    } else {
        stats.read_stddev_mbps = 0.0;
        stats.write_stddev_mbps = 0.0;
        stats.total_stddev_mbps = 0.0;
    }
    
    // Calculate duration
    if (stats.last_timestamp_ns > stats.first_timestamp_ns) {
        stats.duration_sec = static_cast<double>(stats.last_timestamp_ns - stats.first_timestamp_ns) / 1e9;
    } else {
        stats.duration_sec = 0.0;
    }
    
    return stats;
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
                    
                    // Accumulate data to buffer if registered
                    {
                        std::lock_guard<std::mutex> acc_lock(accumulator_mutex_);
                        if (accumulator_buffer_) {
                            accumulator_buffer_->push_back(data);
                        }
                    }
                }
            }
        }

        // Sleep for the sampling interval
        std::this_thread::sleep_for(std::chrono::milliseconds(sampling_interval_ms_));
    }
}

} // namespace GPUBandwidthProfiler

