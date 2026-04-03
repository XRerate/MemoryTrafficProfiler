#include "backends/npu.h"

#include "backends/qprof_session.h"

#include <QProfilerApi.h>
#include <QProfilerCommon.h>

#include <chrono>
#include <cstdio>
#include <cstring>

#include "backends/backend.h"

namespace memory_traffic_profiler {

namespace NpuQProfConfig {
constexpr const char* CAPABILITY_NAME = "profiler:nsp-dsp-metrics";
constexpr uint32_t STREAMING_RATE_MS = 200;
constexpr uint32_t SAMPLING_RATE_MS = 1;
}  // namespace NpuQProfConfig

namespace NpuMetrics {
constexpr uint16_t AXI_128B_READ_REQUEST = 4141;
constexpr uint16_t AXI_128B_WRITE_REQUEST = 4142;
constexpr uint16_t AXI_256B_WRITE_REQUEST = 4143;
constexpr uint16_t AXI_256B_READ_REQUEST = 4144;
constexpr uint16_t NUM_METRICS = 4;
}  // namespace NpuMetrics

class NpuBackend::Impl {
 public:
  Impl()
      : start_config_(nullptr),
        stop_config_(nullptr),
        is_profiling_(false),
        last_timestamp_ns_(0),
        session_registered_(false) {
    QProfNpuResetCallbackState();
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
    if (session_registered_) {
      QProfSession::unregisterUser();
    }
  }

  bool initialize() {
    if (session_registered_) {
      return true;
    }

    if (!QProfSession::registerUser()) {
      return false;
    }
    session_registered_ = true;

    if (!validateCapability()) {
      fprintf(stderr,
              "QProf NPU: capability '%s' not found on this device\n",
              NpuQProfConfig::CAPABILITY_NAME);
      QProfSession::unregisterUser();
      session_registered_ = false;
      return false;
    }

    start_config_ = new ProfilingEventStartConfiguration();
    start_config_->capabilityName.capabilityNameLen =
        snprintf((char*)start_config_->capabilityName.capabilityName,
                 CAPABILITY_NAME_LENGTH, "%s", NpuQProfConfig::CAPABILITY_NAME);
    start_config_->metricIds.metricIdsLen = NpuMetrics::NUM_METRICS;
    start_config_->metricIds.metricIds[0] = NpuMetrics::AXI_128B_READ_REQUEST;
    start_config_->metricIds.metricIds[1] = NpuMetrics::AXI_128B_WRITE_REQUEST;
    start_config_->metricIds.metricIds[2] = NpuMetrics::AXI_256B_WRITE_REQUEST;
    start_config_->metricIds.metricIds[3] = NpuMetrics::AXI_256B_READ_REQUEST;
    start_config_->streamingRate = NpuQProfConfig::STREAMING_RATE_MS;
    start_config_->samplingRate = NpuQProfConfig::SAMPLING_RATE_MS;
    start_config_->profilerConfig = nullptr;
    start_config_->profilerConfigLen = 0;
    start_config_->resultType = RESULT_TYPE_GENERIC_STRUCT;

    stop_config_ = new ProfilingEventStopConfiguration();
    stop_config_->capabilityName.capabilityNameLen =
        snprintf((char*)stop_config_->capabilityName.capabilityName,
                 CAPABILITY_NAME_LENGTH, "%s", NpuQProfConfig::CAPABILITY_NAME);
    stop_config_->metricIds.metricIdsLen = 0;

    return true;
  }

  bool start() {
    LpContextRequest ctx = QProfSession::context();
    if (!ctx || !start_config_) {
      return false;
    }

    if (is_profiling_) {
      return true;
    }

    eReturnCode ret = qp_start(ctx, start_config_);
    if (ret != RETURN_CODE_SUCCESS) {
      return false;
    }

    is_profiling_ = true;
    last_timestamp_ns_ = get_timestamp_ns();

    return true;
  }

  bool stop() {
    LpContextRequest ctx = QProfSession::context();
    if (!ctx || !stop_config_ || !is_profiling_) {
      return false;
    }

    eReturnCode ret = qp_stop(ctx, stop_config_);
    if (ret != RETURN_CODE_SUCCESS) {
      return false;
    }

    is_profiling_ = false;
    return true;
  }

  bool sample(BandwidthData& data) {
    if (!is_profiling_) {
      return false;
    }
    return QProfNpuConsumeSample(data, &last_timestamp_ns_);
  }

  bool is_profiling() const { return is_profiling_; }

 private:
  bool validateCapability() {
    LpContextRequest ctx = QProfSession::context();
    if (!ctx) {
      return false;
    }
    CapabilitiesResponse response = {};
    eReturnCode ret = qp_getCapabilities(ctx, &response);
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

  LpProfilingEventStartConfiguration start_config_;
  LpProfilingEventStopConfiguration stop_config_;
  bool is_profiling_;
  uint64_t last_timestamp_ns_;
  bool session_registered_;
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

}  // namespace memory_traffic_profiler
