#include "backends/npu.h"

#include <QProfilerApi.h>
#include <QProfilerCommon.h>

#include <chrono>
#include <cstdio>
#include <cstring>
#include <mutex>

#include "backends/backend.h"

namespace MemoryTrafficProfiler {

// NPU AXI metric IDs (values are reported in MB/s by QProf)
namespace NpuMetrics {
constexpr uint16_t AXI_128B_READ_REQUEST = 4141;
constexpr uint16_t AXI_128B_WRITE_REQUEST = 4142;
constexpr uint16_t AXI_256B_WRITE_REQUEST = 4143;
constexpr uint16_t AXI_256B_READ_REQUEST = 4144;
constexpr uint16_t NUM_METRICS = 4;
}  // namespace NpuMetrics

// QProf configuration constants for NPU AXI metrics (nsp-dsp-metrics capability)
namespace NpuQProfConfig {
constexpr const char* CAPABILITY_NAME = "profiler:nsp-dsp-metrics";
constexpr uint32_t STREAMING_RATE_MS = 200;
// nsp-dsp-metrics supports 1ms and 10ms sampling rates
constexpr uint32_t SAMPLING_RATE_MS = 1;
}  // namespace NpuQProfConfig

// Global callback data for NPU metrics (values in MB/s from QProf)
static struct {
  std::mutex mutex;
  double axi_128b_read;   // AXI 128B read bandwidth (MB/s)
  double axi_128b_write;  // AXI 128B write bandwidth (MB/s)
  double axi_256b_write;  // AXI 256B write bandwidth (MB/s)
  double axi_256b_read;   // AXI 256B read bandwidth (MB/s)
  uint64_t last_timestamp_ns;
  bool has_data;
} g_npu_callback_data;

// Result callback to receive NPU profiling data
void NpuResultCallback(LpProfilingResult profilingResult) {
  std::lock_guard<std::mutex> lock(g_npu_callback_data.mutex);

  if (profilingResult == nullptr) {
    return;
  }

  if (RESULT_TYPE_GENERIC_STRUCT == profilingResult->resultType) {
    if (profilingResult->profilingResultGeneric != nullptr &&
        profilingResult->profilingResultGeneric->metricResponse != nullptr) {
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

        double value = 0.0;
        switch (data_type) {
          case DATA_TYPE_UINT64:
            value = static_cast<double>(
                profilingResult->profilingResultGeneric->metricResponse[index]
                    .value.uint64Value);
            break;
          case DATA_TYPE_DOUBLE:
            value =
                profilingResult->profilingResultGeneric->metricResponse[index]
                    .value.doubleValue;
            break;
          default:
            continue;
        }

        switch (metric_id) {
          case NpuMetrics::AXI_128B_READ_REQUEST:
            g_npu_callback_data.axi_128b_read = value;
            break;
          case NpuMetrics::AXI_128B_WRITE_REQUEST:
            g_npu_callback_data.axi_128b_write = value;
            break;
          case NpuMetrics::AXI_256B_WRITE_REQUEST:
            g_npu_callback_data.axi_256b_write = value;
            break;
          case NpuMetrics::AXI_256B_READ_REQUEST:
            g_npu_callback_data.axi_256b_read = value;
            break;
          default:
            continue;
        }

        g_npu_callback_data.last_timestamp_ns = timestamp;
        g_npu_callback_data.has_data = true;
      }
    }
  }

  qp_freeProfilingResult(profilingResult);
}

// Message callback for errors/warnings
void NpuMessageCallback(LpProfilingMessage message) {
  if (message != nullptr) {
    fprintf(stderr, "QProf NPU Message: %s\n", message->message);
  }
}

class NpuBackend::Impl {
 public:
  Impl()
      : context_request_(nullptr),
        start_config_(nullptr),
        stop_config_(nullptr),
        is_profiling_(false),
        last_timestamp_ns_(0) {
    g_npu_callback_data.axi_128b_read = 0;
    g_npu_callback_data.axi_128b_write = 0;
    g_npu_callback_data.axi_256b_write = 0;
    g_npu_callback_data.axi_256b_read = 0;
    g_npu_callback_data.last_timestamp_ns = 0;
    g_npu_callback_data.has_data = false;
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
      return true;
    }

    context_request_ = new ContextRequest();
    if (!context_request_) {
      return false;
    }

    eReturnCode ret = qp_initialize(context_request_, nullptr);
    if (ret != RETURN_CODE_SUCCESS) {
      fprintf(stderr, "QProf NPU initialization failed with error code: %d\n",
              static_cast<int>(ret));
      delete context_request_;
      context_request_ = nullptr;
      return false;
    }

    qp_setResultCallback(context_request_, NpuResultCallback);
    qp_setMessageCallback(context_request_, NpuMessageCallback);

    // Validate that the nsp-dsp-metrics capability is available
    if (!validateCapability()) {
      fprintf(stderr,
              "QProf NPU: capability '%s' not found on this device\n",
              NpuQProfConfig::CAPABILITY_NAME);
      qp_destroy(context_request_);
      delete context_request_;
      context_request_ = nullptr;
      return false;
    }

    // Configure start configuration with NPU AXI metrics
    start_config_ = new ProfilingEventStartConfiguration();
    start_config_->capabilityName.capabilityNameLen =
        snprintf((char*)start_config_->capabilityName.capabilityName,
                 CAPABILITY_NAME_LENGTH, "%s", NpuQProfConfig::CAPABILITY_NAME);
    start_config_->metricIds.metricIdsLen = NpuMetrics::NUM_METRICS;
    start_config_->metricIds.metricIds[0] =
        NpuMetrics::AXI_128B_READ_REQUEST;
    start_config_->metricIds.metricIds[1] =
        NpuMetrics::AXI_128B_WRITE_REQUEST;
    start_config_->metricIds.metricIds[2] =
        NpuMetrics::AXI_256B_WRITE_REQUEST;
    start_config_->metricIds.metricIds[3] =
        NpuMetrics::AXI_256B_READ_REQUEST;
    start_config_->streamingRate = NpuQProfConfig::STREAMING_RATE_MS;
    start_config_->samplingRate = NpuQProfConfig::SAMPLING_RATE_MS;
    start_config_->profilerConfig = nullptr;
    start_config_->profilerConfigLen = 0;
    start_config_->resultType = RESULT_TYPE_GENERIC_STRUCT;

    // Configure stop configuration
    stop_config_ = new ProfilingEventStopConfiguration();
    stop_config_->capabilityName.capabilityNameLen =
        snprintf((char*)stop_config_->capabilityName.capabilityName,
                 CAPABILITY_NAME_LENGTH, "%s", NpuQProfConfig::CAPABILITY_NAME);
    stop_config_->metricIds.metricIdsLen = 0;

    return true;
  }

  bool start() {
    if (!context_request_ || !start_config_) {
      return false;
    }

    if (is_profiling_) {
      return true;
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

    uint64_t current_time_ns = get_timestamp_ns();
    uint64_t delta_time_ns = current_time_ns - last_timestamp_ns_;
    if (delta_time_ns == 0) {
      return false;
    }

    double read_mbps = 0.0;
    double write_mbps = 0.0;

    {
      std::lock_guard<std::mutex> lock(g_npu_callback_data.mutex);
      if (g_npu_callback_data.has_data) {
        // QProf AXI metrics are already in MB/s, just sum by direction
        read_mbps = g_npu_callback_data.axi_128b_read +
                    g_npu_callback_data.axi_256b_read;
        write_mbps = g_npu_callback_data.axi_128b_write +
                     g_npu_callback_data.axi_256b_write;
      }
    }

    data.read_bandwidth_mbps = read_mbps;
    data.write_bandwidth_mbps = write_mbps;
    data.total_bandwidth_mbps = read_mbps + write_mbps;
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
                NpuQProfConfig::CAPABILITY_NAME) == 0) {
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

NpuBackend::NpuBackend() : pimpl_(std::make_unique<Impl>()) {}

NpuBackend::~NpuBackend() = default;

bool NpuBackend::initialize() { return pimpl_->initialize(); }

bool NpuBackend::start() { return pimpl_->start(); }

bool NpuBackend::stop() { return pimpl_->stop(); }

bool NpuBackend::sample(BandwidthData& data) { return pimpl_->sample(data); }

bool NpuBackend::is_profiling() const { return pimpl_->is_profiling(); }

const char* NpuBackend::get_name() const { return "NPU"; }

BackendCategory NpuBackend::get_category() const {
  return BackendCategory::NPU;
}

}  // namespace MemoryTrafficProfiler
