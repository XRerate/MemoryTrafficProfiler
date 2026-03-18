#include "backends/adreno.h"

#include <QProfilerApi.h>
#include <QProfilerCommon.h>

#include <chrono>
#include <cstdio>
#include <cstring>
#include <mutex>

#include "backends/backend.h"

namespace memory_traffic_profiler {

// QProf metric IDs
namespace QProfMetrics {
// GPU DDR bandwidth metric ID (total bandwidth in MB/s)
constexpr uint16_t GPU_DDR_BANDWIDTH_METRIC_ID = 4663;
}  // namespace QProfMetrics

// QProf configuration constants
namespace QProfConfig {
constexpr const char* CAPABILITY_NAME = "profiler:apps-proc-ddr-metrics";
// Streaming rate: Stream results every 200ms
constexpr uint32_t STREAMING_RATE_MS = 200;
// Sampling rate: Sample every 10ms
constexpr uint32_t SAMPLING_RATE_MS = 10;
}  // namespace QProfConfig

// Bandwidth distribution constants
namespace BandwidthDistribution {
// Split total bandwidth equally between read and write
constexpr double READ_WRITE_SPLIT_RATIO = 2.0;
}  // namespace BandwidthDistribution

// Global callback data: accumulate-consume pattern with fallback
static struct {
  std::mutex mutex;
  // Accumulator (written by callback, consumed by sample)
  double bandwidth_sum;         // sum of all bandwidth MB/s samples
  uint32_t sample_count;        // number of samples accumulated
  // Fallback (written by sample, read by sample when no new data)
  double last_avg_bandwidth;    // last computed average MB/s
  bool has_ever_received_data;  // one-way latch: true once first data arrives
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
      for (uint16_t index = 0;
           index < profilingResult->profilingResultGeneric->metricResponseLen;
           index++) {
        uint16_t metric_id =
            profilingResult->profilingResultGeneric->metricResponse[index]
                .metricId;
        eDataType data_type =
            profilingResult->profilingResultGeneric->metricResponse[index]
                .value.dataType;
        uint64_t timestamp =
            profilingResult->profilingResultGeneric->metricResponse[index]
                .timestamp;

        double raw_value = 0.0;
        switch (data_type) {
          case DATA_TYPE_UINT64:
            raw_value = static_cast<double>(
                profilingResult->profilingResultGeneric->metricResponse[index]
                    .value.uint64Value);
            break;
          case DATA_TYPE_DOUBLE:
            raw_value =
                profilingResult->profilingResultGeneric->metricResponse[index]
                    .value.doubleValue;
            break;
          default:
            break;
        }

        // Accumulate GPU DDR bandwidth metric
        if (metric_id == QProfMetrics::GPU_DDR_BANDWIDTH_METRIC_ID) {
          g_callback_data.bandwidth_sum += raw_value;
          g_callback_data.sample_count++;
          g_callback_data.has_ever_received_data = true;
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
  Impl()
      : context_request_(nullptr),
        start_config_(nullptr),
        stop_config_(nullptr),
        is_profiling_(false),
        last_timestamp_ns_(0) {
    g_callback_data.bandwidth_sum = 0.0;
    g_callback_data.sample_count = 0;
    g_callback_data.last_avg_bandwidth = 0.0;
    g_callback_data.has_ever_received_data = false;
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
      return true;  // Already initialized
    }

    // Allocate context request
    context_request_ = new ContextRequest();
    if (!context_request_) {
      return false;
    }

    // Initialize QProf - pass nullptr for server config (from working example)
    eReturnCode ret = qp_initialize(context_request_, nullptr);
    if (ret != RETURN_CODE_SUCCESS) {
      fprintf(stderr, "QProf initialization failed with error code: %d\n",
              static_cast<int>(ret));
      delete context_request_;
      context_request_ = nullptr;
      return false;
    }

    // Set callbacks
    qp_setResultCallback(context_request_, ResultCallback);
    qp_setMessageCallback(context_request_, MessageCallback);

    // Validate that the capability is available on this device
    if (!validateCapability()) {
      fprintf(stderr,
              "QProf Adreno: capability '%s' not found on this device\n",
              QProfConfig::CAPABILITY_NAME);
      qp_destroy(context_request_);
      delete context_request_;
      context_request_ = nullptr;
      return false;
    }

    // Configure start configuration
    start_config_ = new ProfilingEventStartConfiguration();
    start_config_->capabilityName.capabilityNameLen =
        snprintf((char*)start_config_->capabilityName.capabilityName,
                 CAPABILITY_NAME_LENGTH, "%s", QProfConfig::CAPABILITY_NAME);
    start_config_->metricIds.metricIdsLen = 1;
    start_config_->metricIds.metricIds[0] =
        QProfMetrics::GPU_DDR_BANDWIDTH_METRIC_ID;
    start_config_->streamingRate = QProfConfig::STREAMING_RATE_MS;
    start_config_->samplingRate = QProfConfig::SAMPLING_RATE_MS;
    start_config_->profilerConfig = nullptr;
    start_config_->profilerConfigLen = 0;
    start_config_->resultType = RESULT_TYPE_GENERIC_STRUCT;

    // Configure stop configuration
    stop_config_ = new ProfilingEventStopConfiguration();
    stop_config_->capabilityName.capabilityNameLen =
        snprintf((char*)stop_config_->capabilityName.capabilityName,
                 CAPABILITY_NAME_LENGTH, "%s", QProfConfig::CAPABILITY_NAME);
    stop_config_->metricIds.metricIdsLen = 0;

    return true;
  }

  bool start() {
    if (!context_request_ || !start_config_) {
      return false;
    }

    if (is_profiling_) {
      return true;  // Already started
    }

    eReturnCode ret = qp_start(context_request_, start_config_);
    if (ret != RETURN_CODE_SUCCESS) {
      return false;
    }

    is_profiling_ = true;
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

    double total_bandwidth_mbps = 0.0;

    {
      std::lock_guard<std::mutex> lock(g_callback_data.mutex);

      if (!g_callback_data.has_ever_received_data) {
        return false;  // No data has ever arrived from QProf yet
      }

      if (g_callback_data.sample_count > 0) {
        // New data: compute average and consume
        total_bandwidth_mbps = g_callback_data.bandwidth_sum /
                               static_cast<double>(g_callback_data.sample_count);

#ifndef NDEBUG
        fprintf(stderr, "[GPU] avg=%.2f MB/s  samples=%u  delta_ms=%.1f\n",
                total_bandwidth_mbps, g_callback_data.sample_count,
                static_cast<double>(get_timestamp_ns() - last_timestamp_ns_) / 1e6);
#endif

        // Reset accumulators
        g_callback_data.bandwidth_sum = 0.0;
        g_callback_data.sample_count = 0;
        // Store as fallback
        g_callback_data.last_avg_bandwidth = total_bandwidth_mbps;
      } else {
        // No new data since last consume: use fallback
        total_bandwidth_mbps = g_callback_data.last_avg_bandwidth;
      }
    }

    uint64_t current_time_ns = get_timestamp_ns();

    data.read_bandwidth_mbps = total_bandwidth_mbps /
                               BandwidthDistribution::READ_WRITE_SPLIT_RATIO;
    data.write_bandwidth_mbps = total_bandwidth_mbps /
                                BandwidthDistribution::READ_WRITE_SPLIT_RATIO;
    data.total_bandwidth_mbps = total_bandwidth_mbps;
    data.timestamp_ns = current_time_ns;

    last_timestamp_ns_ = current_time_ns;

    return true;
  }

  bool is_profiling() const { return is_profiling_; }

 private:
  bool validateCapability() {
    CapabilitiesResponse response = {};
    eReturnCode ret = qp_getCapabilities(context_request_, &response);
    if (ret != RETURN_CODE_SUCCESS) {
      return false;
    }

    for (uint8_t i = 0; i < response.capabilitiesLen; i++) {
      if (strcmp((const char*)response.capabilities[i]
                    .capabilityName.capabilityName,
                QProfConfig::CAPABILITY_NAME) == 0) {
        return true;
      }
    }
    return false;
  }

  uint64_t get_timestamp_ns() {
    auto now = std::chrono::steady_clock::now();
    auto duration = now.time_since_epoch();
    return std::chrono::duration_cast<std::chrono::nanoseconds>(duration)
        .count();
  }

  LpContextRequest context_request_;
  LpProfilingEventStartConfiguration start_config_;
  LpProfilingEventStopConfiguration stop_config_;
  bool is_profiling_;
  uint64_t last_timestamp_ns_;
};

AdrenoBackend::AdrenoBackend() : pimpl_(std::make_unique<Impl>()) {}

AdrenoBackend::~AdrenoBackend() = default;

bool AdrenoBackend::initialize() { return pimpl_->initialize(); }

bool AdrenoBackend::start() { return pimpl_->start(); }

bool AdrenoBackend::stop() { return pimpl_->stop(); }

bool AdrenoBackend::sample(BandwidthData& data) { return pimpl_->sample(data); }

bool AdrenoBackend::is_profiling() const { return pimpl_->is_profiling(); }

const char* AdrenoBackend::get_name() const { return "Adreno"; }

BackendCategory AdrenoBackend::get_category() const {
  return BackendCategory::GPU;
}

}  // namespace memory_traffic_profiler
