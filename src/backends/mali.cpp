#include "backends/mali.h"
#include "backends/backend.h"
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
             last_read_bytes_(0), last_write_bytes_(0),
             last_timestamp_ns_(0) {}

    ~Impl() {
        if (sampler_ && is_profiling_) {
            (void)sampler_->stop_sampling(); // Ignore error in destructor
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
        // Use MaliExtBusRdBy and MaliExtBusWrBy (cumulative byte counters)
        // Bandwidth will be calculated as delta_bytes / delta_time
        std::error_code ec;
        
        // Add read bytes counter
        ec = config.add_counter(MaliExtBusRdBy);
        if (ec) {
            fprintf(stderr, "DEBUG: Failed to add MaliExtBusRdBy counter: %s\n", ec.message().c_str());
            return false;
        }

        // Add write bytes counter
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
            (void)sampler_->stop_sampling(); // Clean up on error
            return false;
        }

        // Get initial counter values
        hwcpipe::counter_sample sample;
        ec = sampler_->get_counter_value(MaliExtBusRdBy, sample);
        if (!ec) {
            if (sample.type == hwcpipe::counter_sample::type::uint64) {
                last_read_bytes_ = sample.value.uint64;
            } else if (sample.type == hwcpipe::counter_sample::type::float64) {
                last_read_bytes_ = static_cast<uint64_t>(sample.value.float64);
            }
        }

        ec = sampler_->get_counter_value(MaliExtBusWrBy, sample);
        if (!ec) {
            if (sample.type == hwcpipe::counter_sample::type::uint64) {
                last_write_bytes_ = sample.value.uint64;
            } else if (sample.type == hwcpipe::counter_sample::type::float64) {
                last_write_bytes_ = static_cast<uint64_t>(sample.value.float64);
            }
        }

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
        uint64_t delta_time_ns = current_time_ns - last_timestamp_ns_;

        if (delta_time_ns == 0) {
            return false; // No time elapsed
        }

        // Get current counter values
        hwcpipe::counter_sample read_sample;
        ec = sampler_->get_counter_value(MaliExtBusRdBy, read_sample);
        if (ec) {
            return false;
        }

        hwcpipe::counter_sample write_sample;
        ec = sampler_->get_counter_value(MaliExtBusWrBy, write_sample);
        // Write counter is optional, continue even if it fails

        uint64_t current_read_bytes = 0;
        uint64_t current_write_bytes = 0;

        if (read_sample.type == hwcpipe::counter_sample::type::uint64) {
            current_read_bytes = read_sample.value.uint64;
        } else if (read_sample.type == hwcpipe::counter_sample::type::float64) {
            current_read_bytes = static_cast<uint64_t>(read_sample.value.float64);
        }

        if (write_sample.type == hwcpipe::counter_sample::type::uint64) {
            current_write_bytes = write_sample.value.uint64;
        } else if (write_sample.type == hwcpipe::counter_sample::type::float64) {
            current_write_bytes = static_cast<uint64_t>(write_sample.value.float64);
        }

        // Calculate bandwidth (bytes per second) from delta
        double delta_time_sec = static_cast<double>(delta_time_ns) / 1e9;
        
        // Handle potential counter wraparound
        int64_t read_delta = static_cast<int64_t>(current_read_bytes) - static_cast<int64_t>(last_read_bytes_);
        int64_t write_delta = static_cast<int64_t>(current_write_bytes) - static_cast<int64_t>(last_write_bytes_);
        
        // If delta is negative, it might be wraparound or counter reset - treat as 0
        if (read_delta < 0) read_delta = 0;
        if (write_delta < 0) write_delta = 0;

        // Convert bytes/sec to MB/s
        data.read_bandwidth_mbps = (static_cast<double>(read_delta) / delta_time_sec) / (1024.0 * 1024.0);
        data.write_bandwidth_mbps = (static_cast<double>(write_delta) / delta_time_sec) / (1024.0 * 1024.0);
        data.total_bandwidth_mbps = data.read_bandwidth_mbps + data.write_bandwidth_mbps;
        data.timestamp_ns = current_time_ns;

        // Update last values for next sample
        last_read_bytes_ = current_read_bytes;
        last_write_bytes_ = current_write_bytes;
        last_timestamp_ns_ = current_time_ns;

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
    uint64_t last_read_bytes_;
    uint64_t last_write_bytes_;
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

