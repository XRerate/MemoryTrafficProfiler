#include "backends/adreno.h"
#include "backends/backend.h"
#include <QProfilerApi.h>
#include <QProfilerCommon.h>
#include <cstring>
#include <mutex>
#include <chrono>
#include <cstdio>

namespace GPUBandwidthProfiler {

// QProf metric IDs
namespace QProfMetrics {
    // GPU DDR bandwidth metric ID (total bandwidth in MB/s)
    constexpr uint16_t GPU_DDR_BANDWIDTH_METRIC_ID = 4663;
}

// QProf configuration constants
namespace QProfConfig {
    // Streaming rate: Stream results every 200ms
    constexpr uint32_t STREAMING_RATE_MS = 200;
    // Sampling rate: Sample every 10ms
    constexpr uint32_t SAMPLING_RATE_MS = 10;
}

// Precision constants for bandwidth calculations
namespace BandwidthPrecision {
    // Multiplier to preserve 6 decimal places (MB/s * 1000000)
    constexpr double PRECISION_MULTIPLIER = 1000000.0;
    // Divisor to convert back from precision format
    constexpr double PRECISION_DIVISOR = 1000000.0;
}

// Bandwidth distribution constants
namespace BandwidthDistribution {
    // Split total bandwidth equally between read and write
    constexpr double READ_WRITE_SPLIT_RATIO = 2.0;
}

// Global callback data (simplified - in production, use thread-local or instance-specific)
static struct {
    std::mutex mutex;
    uint64_t read_bytes;
    uint64_t write_bytes;
    uint64_t last_timestamp_ns;
    bool has_data;
} g_callback_data;

// Result callback to receive profiling data
void ResultCallback(LpProfilingResult profilingResult) {
    std::lock_guard<std::mutex> lock(g_callback_data.mutex);
    
    if (profilingResult == nullptr) {
        return;
    }
    
    // Process generic structure format (faster, less detailed)
    if (RESULT_TYPE_GENERIC_STRUCT == profilingResult->resultType) {
        if (profilingResult->profilingResultGeneric != nullptr &&
            profilingResult->profilingResultGeneric->metricResponse != nullptr) {
            
            // Process metric responses
            for (uint16_t index = 0; index < profilingResult->profilingResultGeneric->metricResponseLen; index++) {
                uint16_t metric_id = profilingResult->profilingResultGeneric->metricResponse[index].metricId;
                eDataType data_type = profilingResult->profilingResultGeneric->metricResponse[index].value.dataType;
                uint64_t timestamp = profilingResult->profilingResultGeneric->metricResponse[index].timestamp;
                
                // Check for GPU DDR bandwidth metric
                // Note: This appears to be total bandwidth, not separate read/write
                // If separate read/write metrics exist (e.g., 4661, 4662), they should be added
                if (metric_id == QProfMetrics::GPU_DDR_BANDWIDTH_METRIC_ID) {
                    double bandwidth_value = 0.0;
                    switch (data_type) {
                        case DATA_TYPE_UINT64:
                            bandwidth_value = static_cast<double>(profilingResult->profilingResultGeneric->metricResponse[index].value.uint64Value);
                            break;
                        case DATA_TYPE_DOUBLE:
                            bandwidth_value = profilingResult->profilingResultGeneric->metricResponse[index].value.doubleValue;
                            break;
                        default:
                            break;
                    }
                    // Store the total bandwidth value directly (QProf returns in MB/s)
                    // Store as integer * PRECISION_MULTIPLIER to preserve precision (6 decimal places)
                    g_callback_data.read_bytes = static_cast<uint64_t>(bandwidth_value * BandwidthPrecision::PRECISION_MULTIPLIER);
                    g_callback_data.write_bytes = 0; // Total is stored in read_bytes, write will be calculated
                    g_callback_data.last_timestamp_ns = timestamp;
                    g_callback_data.has_data = true;
                }
            }
        }
    }
    
    // Free the result
    qp_freeProfilingResult(profilingResult);
}

// Message callback for errors/warnings
void MessageCallback(LpProfilingMessage message) {
    if (message != nullptr) {
        // Log messages for debugging
        fprintf(stderr, "QProf Message: %s\n", message->message);
    }
}

class AdrenoBackend::Impl {
public:
    Impl() : context_request_(nullptr), start_config_(nullptr), stop_config_(nullptr),
             is_profiling_(false), last_read_bytes_(0), last_write_bytes_(0),
             last_timestamp_ns_(0) {
        // Initialize callback data
        g_callback_data.read_bytes = 0;
        g_callback_data.write_bytes = 0;
        g_callback_data.last_timestamp_ns = 0;
        g_callback_data.has_data = false;
    }

    ~Impl() {
        if (is_profiling_) {
            stop();
        }
        if (start_config_) {
            delete start_config_;
        }
        if (stop_config_) {
            delete stop_config_;
        }
        if (context_request_) {
            qp_destroy(context_request_);
            delete context_request_;
        }
    }

    bool initialize() {
        if (context_request_) {
            return true; // Already initialized
        }

        // Allocate context request
        context_request_ = new ContextRequest();
        if (!context_request_) {
            return false;
        }

        // Initialize QProf - pass nullptr for server config (from working example)
        eReturnCode ret = qp_initialize(context_request_, nullptr);
        if (ret != RETURN_CODE_SUCCESS) {
            fprintf(stderr, "QProf initialization failed with error code: %d\n", static_cast<int>(ret));
            delete context_request_;
            context_request_ = nullptr;
            return false;
        }

        // Set callbacks
        qp_setResultCallback(context_request_, ResultCallback);
        qp_setMessageCallback(context_request_, MessageCallback);

        // Configure start configuration (from working example)
        start_config_ = new ProfilingEventStartConfiguration();
        start_config_->capabilityName.capabilityNameLen = snprintf(
            (char*)start_config_->capabilityName.capabilityName,
            CAPABILITY_NAME_LENGTH, "profiler:apps-proc-ddr-metrics");
        start_config_->metricIds.metricIdsLen = 1;
        start_config_->metricIds.metricIds[0] = QProfMetrics::GPU_DDR_BANDWIDTH_METRIC_ID;
        start_config_->streamingRate = QProfConfig::STREAMING_RATE_MS;
        start_config_->samplingRate = QProfConfig::SAMPLING_RATE_MS;
        start_config_->resultType = RESULT_TYPE_GENERIC_STRUCT;
        start_config_->profilerConfig = nullptr;

        // Configure stop configuration
        stop_config_ = new ProfilingEventStopConfiguration();
        stop_config_->capabilityName.capabilityNameLen = snprintf(
            (char*)stop_config_->capabilityName.capabilityName,
            CAPABILITY_NAME_LENGTH, "profiler:apps-proc-ddr-metrics");
        stop_config_->metricIds.metricIdsLen = 0;

        return true;
    }

    bool start() {
        if (!context_request_ || !start_config_) {
            return false;
        }

        if (is_profiling_) {
            return true; // Already started
        }

        eReturnCode ret = qp_start(context_request_, start_config_);
        if (ret != RETURN_CODE_SUCCESS) {
            return false;
        }

        is_profiling_ = true;
        last_read_bytes_ = 0;
        last_write_bytes_ = 0;
        last_timestamp_ns_ = get_timestamp_ns();

        return true;
    }

    bool stop() {
        if (!context_request_ || !stop_config_ || !is_profiling_) {
            return false;
        }

        eReturnCode ret = qp_stop(context_request_, stop_config_);
        if (ret != RETURN_CODE_SUCCESS) {
            return false;
        }

        is_profiling_ = false;
        return true;
    }

    bool sample(BandwidthData& data) {
        if (!context_request_ || !is_profiling_) {
            return false;
        }

        // Get current timestamp
        uint64_t current_time_ns = get_timestamp_ns();
        uint64_t delta_time_ns = current_time_ns - last_timestamp_ns_;

        if (delta_time_ns == 0) {
            return false; // No time elapsed
        }

        // Get latest values from callback
        // Note: The QProf GPU DDR bandwidth metric returns total bandwidth in MB/s (MBps) directly
        double current_total_bandwidth_mbps = 0.0;
        
        {
            std::lock_guard<std::mutex> lock(g_callback_data.mutex);
            if (g_callback_data.has_data) {
                // The callback stores total bandwidth in MB/s (stored as integer * PRECISION_MULTIPLIER for precision)
                current_total_bandwidth_mbps = static_cast<double>(g_callback_data.read_bytes) / BandwidthPrecision::PRECISION_DIVISOR;
            }
        }

        // QProf already returns values in MB/s, use directly
        // Split total bandwidth equally between read and write (since we don't have separate metrics)
        data.read_bandwidth_mbps = current_total_bandwidth_mbps / BandwidthDistribution::READ_WRITE_SPLIT_RATIO;
        data.write_bandwidth_mbps = current_total_bandwidth_mbps / BandwidthDistribution::READ_WRITE_SPLIT_RATIO;
        data.total_bandwidth_mbps = current_total_bandwidth_mbps;
        data.timestamp_ns = current_time_ns;
        
        // Update last values for reference (store in MB/s * PRECISION_MULTIPLIER for precision)
        last_read_bytes_ = static_cast<uint64_t>(current_total_bandwidth_mbps * BandwidthPrecision::PRECISION_MULTIPLIER / BandwidthDistribution::READ_WRITE_SPLIT_RATIO);
        last_write_bytes_ = static_cast<uint64_t>(current_total_bandwidth_mbps * BandwidthPrecision::PRECISION_MULTIPLIER / BandwidthDistribution::READ_WRITE_SPLIT_RATIO);
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

    LpContextRequest context_request_;
    LpProfilingEventStartConfiguration start_config_;
    LpProfilingEventStopConfiguration stop_config_;
    bool is_profiling_;
    uint64_t last_read_bytes_;
    uint64_t last_write_bytes_;
    uint64_t last_timestamp_ns_;
};

AdrenoBackend::AdrenoBackend() : pimpl_(std::make_unique<Impl>()) {}

AdrenoBackend::~AdrenoBackend() = default;

bool AdrenoBackend::initialize() {
    return pimpl_->initialize();
}

bool AdrenoBackend::start() {
    return pimpl_->start();
}

bool AdrenoBackend::stop() {
    return pimpl_->stop();
}

bool AdrenoBackend::sample(BandwidthData& data) {
    return pimpl_->sample(data);
}

bool AdrenoBackend::is_profiling() const {
    return pimpl_->is_profiling();
}

const char* AdrenoBackend::get_name() const {
    return "Adreno";
}

} // namespace GPUBandwidthProfiler
