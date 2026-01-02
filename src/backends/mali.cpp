#include "backends/mali.h"
#include "backends/backend.h"
#include "backends/constants.h"
#include <hwcpipe/gpu.hpp>
#include <hwcpipe/sampler.hpp>
#include <hwcpipe/hwcpipe_counter.h>
#include <chrono>
#include <cstring>
#include <cstdio>

namespace GPUBandwidthProfiler {

class MaliBackend::Impl {
public:
    Impl() : gpu_(0), is_profiling_(false),
             last_timestamp_ns_(0) {}

    ~Impl() {
        if (sampler_ && is_profiling_) {
            // Best-effort cleanup in destructor: we can't throw exceptions or return errors
            // The error code is captured but not used because:
            // 1. Destructors should be noexcept (can't throw)
            // 2. Object is being destroyed anyway, so error handling is not meaningful
            // 3. This is a cleanup attempt - if it fails, there's nothing we can do
            std::error_code ec = sampler_->stop_sampling();
            (void)ec; // Explicitly acknowledge we're ignoring the error
        }
    }

    bool initialize() {
        // Check if GPU is available
        if (!gpu_) {
            fprintf(stderr, "DEBUG: GPU device not available\n");
            return false;
        }

        // Create sampler configuration
        hwcpipe::sampler_config config(gpu_);

        // Add external memory bandwidth counters
        // According to libGPUCounters documentation:
        // - MaliExtBusRdBy = MaliExtBusRdBt * MALI_CONFIG_EXT_BUS_BYTE_SIZE (read bytes, absolute)
        // - MaliExtBusWrBy = MaliExtBusWrBt * MALI_CONFIG_EXT_BUS_BYTE_SIZE (write bytes, absolute)
        // These counters return instantaneous bandwidth in bytes/second, not cumulative bytes
        // We use them directly and convert to MB/s
        std::error_code ec;
        
        // Add read bytes counter (derived: beats * byte_size)
        ec = config.add_counter(MaliExtBusRdBy);
        if (ec) {
            fprintf(stderr, "DEBUG: Failed to add MaliExtBusRdBy counter: %s\n", ec.message().c_str());
            return false;
        }

        // Add write bytes counter (derived: beats * byte_size)
        ec = config.add_counter(MaliExtBusWrBy);
        if (ec) {
            fprintf(stderr, "DEBUG: Write counter MaliExtBusWrBy not available, continuing without it\n");
            // Write counter is optional, continue without it
        }

        // Create sampler
        sampler_ = std::make_unique<hwcpipe::sampler<>>(config);
        if (!*sampler_) {
            fprintf(stderr, "DEBUG: Failed to create sampler\n");
            return false;
        }

        fprintf(stderr, "DEBUG: Mali backend initialized successfully\n");
        return true;
    }

    bool start() {
        if (!sampler_) {
            return false;
        }

        if (is_profiling_) {
            return true; // Already started
        }

        std::error_code ec = sampler_->start_sampling();
        if (ec) {
            return false;
        }

        // Take initial sample to establish baseline
        ec = sampler_->sample_now();
        if (ec) {
            // Clean up on error: stop sampling since we started it but failed to take initial sample
            // The error code from stop_sampling() is captured but not used because:
            // 1. We're already returning false to indicate start() failed
            // 2. The stop_sampling() error doesn't change the fact that start() failed
            // 3. This is a cleanup attempt - if it fails, there's nothing more we can do
            std::error_code stop_ec = sampler_->stop_sampling();
            (void)stop_ec; // Explicitly acknowledge we're ignoring the cleanup error
            return false;
        }

        // No need to establish baseline - counters are instantaneous, not cumulative
        last_timestamp_ns_ = get_timestamp_ns();
        is_profiling_ = true;

        return true;
    }

    bool stop() {
        if (!sampler_ || !is_profiling_) {
            return false;
        }

        std::error_code ec = sampler_->stop_sampling();
        if (ec) {
            return false;
        }

        is_profiling_ = false;
        return true;
    }

    bool sample(BandwidthData& data) {
        if (!sampler_ || !is_profiling_) {
            return false;
        }

        // Request a new sample
        std::error_code ec = sampler_->sample_now();
        if (ec) {
            return false;
        }

        // Get current timestamp
        uint64_t current_time_ns = get_timestamp_ns();

        // Get current counter values (these are instantaneous bandwidth in bytes/second)
        hwcpipe::counter_sample read_sample;
        ec = sampler_->get_counter_value(MaliExtBusRdBy, read_sample);
        if (ec) {
            return false;
        }

        hwcpipe::counter_sample write_sample;
        ec = sampler_->get_counter_value(MaliExtBusWrBy, write_sample);
        // Write counter is optional, continue even if it fails

        double current_read_bytes_per_sec = 0.0;
        double current_write_bytes_per_sec = 0.0;

        if (read_sample.type == hwcpipe::counter_sample::type::uint64) {
            current_read_bytes_per_sec = static_cast<double>(read_sample.value.uint64);
        } else if (read_sample.type == hwcpipe::counter_sample::type::float64) {
            current_read_bytes_per_sec = read_sample.value.float64;
        }

        if (write_sample.type == hwcpipe::counter_sample::type::uint64) {
            current_write_bytes_per_sec = static_cast<double>(write_sample.value.uint64);
        } else if (write_sample.type == hwcpipe::counter_sample::type::float64) {
            current_write_bytes_per_sec = write_sample.value.float64;
        }

        // Convert from bytes/second to MB/s
        // The counters already return instantaneous bandwidth in bytes/second
        auto delta_time_ns = current_time_ns - last_timestamp_ns_;
        if (delta_time_ns == 0) {
            return false; // No time elapsed
        }

        double delta_time_sec = static_cast<double>(delta_time_ns) / TimeConversion::NANOSECONDS_TO_SECONDS;

        data.read_bandwidth_mbps = (current_read_bytes_per_sec / delta_time_sec) / BandwidthConversion::BYTES_TO_MB;
        data.write_bandwidth_mbps = (current_write_bytes_per_sec / delta_time_sec) / BandwidthConversion::BYTES_TO_MB;
        data.total_bandwidth_mbps = data.read_bandwidth_mbps + data.write_bandwidth_mbps;
        data.timestamp_ns = current_time_ns;

        return true;
    }

    bool is_profiling() const {
        return is_profiling_;
    }

private:
    uint64_t get_timestamp_ns() {
        auto now = std::chrono::steady_clock::now();
        auto duration = now.time_since_epoch();
        return std::chrono::duration_cast<std::chrono::nanoseconds>(duration).count();
    }

    hwcpipe::gpu gpu_;
    std::unique_ptr<hwcpipe::sampler<>> sampler_;
    bool is_profiling_;
    uint64_t last_timestamp_ns_;
};

MaliBackend::MaliBackend() : pimpl_(std::make_unique<Impl>()) {}

MaliBackend::~MaliBackend() = default;

bool MaliBackend::initialize() {
    return pimpl_->initialize();
}

bool MaliBackend::start() {
    return pimpl_->start();
}

bool MaliBackend::stop() {
    return pimpl_->stop();
}

bool MaliBackend::sample(BandwidthData& data) {
    return pimpl_->sample(data);
}

bool MaliBackend::is_profiling() const {
    return pimpl_->is_profiling();
}

const char* MaliBackend::get_name() const {
    return "Mali";
}

} // namespace GPUBandwidthProfiler

