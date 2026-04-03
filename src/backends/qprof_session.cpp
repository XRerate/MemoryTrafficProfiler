#include "backends/qprof_session.h"

#include <QProfilerApi.h>
#include <QProfilerCommon.h>

#include <chrono>
#include <cstdio>
#include <cstring>
#include <mutex>

namespace memory_traffic_profiler {

namespace {

std::mutex g_session_mutex;
LpContextRequest g_context = nullptr;
int g_refcount = 0;

#if defined(BUILD_ADRENO_BACKEND)
namespace GpuMetrics {
constexpr uint16_t GPU_DDR_BANDWIDTH_METRIC_ID = 4663;
}  // namespace GpuMetrics

namespace BandwidthDistribution {
constexpr double READ_WRITE_SPLIT_RATIO = 2.0;
}  // namespace BandwidthDistribution

struct {
  std::mutex mutex;
  double bandwidth_sum;
  uint32_t sample_count;
  double last_avg_bandwidth;
  bool has_ever_received_data;
} g_gpu_callback_data;
#endif

#if defined(BUILD_NPU_BACKEND)
namespace NpuMetrics {
constexpr uint16_t AXI_128B_READ_REQUEST = 4141;
constexpr uint16_t AXI_128B_WRITE_REQUEST = 4142;
constexpr uint16_t AXI_256B_WRITE_REQUEST = 4143;
constexpr uint16_t AXI_256B_READ_REQUEST = 4144;
}  // namespace NpuMetrics

struct {
  std::mutex mutex;
  double axi_128b_read_sum;
  uint32_t axi_128b_read_count;
  double axi_128b_write_sum;
  uint32_t axi_128b_write_count;
  double axi_256b_write_sum;
  uint32_t axi_256b_write_count;
  double axi_256b_read_sum;
  uint32_t axi_256b_read_count;
  double last_avg_read_mbps;
  double last_avg_write_mbps;
  bool has_ever_received_data;
} g_npu_callback_data;
#endif

void UnifiedMessageCallback(LpProfilingMessage message) {
  if (message != nullptr) {
    fprintf(stderr, "QProf: %s\n", message->message);
  }
}

void UnifiedResultCallback(LpProfilingResult profilingResult) {
  if (profilingResult == nullptr) {
    return;
  }

  if (RESULT_TYPE_GENERIC_STRUCT != profilingResult->resultType) {
    qp_freeProfilingResult(profilingResult);
    return;
  }

  if (profilingResult->profilingResultGeneric == nullptr ||
      profilingResult->profilingResultGeneric->metricResponse == nullptr) {
    qp_freeProfilingResult(profilingResult);
    return;
  }

  for (uint16_t index = 0;
       index < profilingResult->profilingResultGeneric->metricResponseLen;
       index++) {
    uint16_t metric_id =
        profilingResult->profilingResultGeneric->metricResponse[index].metricId;
    eDataType data_type =
        profilingResult->profilingResultGeneric->metricResponse[index]
            .value.dataType;

    double raw_value = 0.0;
    switch (data_type) {
      case DATA_TYPE_UINT64:
        raw_value = static_cast<double>(
            profilingResult->profilingResultGeneric->metricResponse[index]
                .value.uint64Value);
        break;
      case DATA_TYPE_DOUBLE:
        raw_value = profilingResult->profilingResultGeneric->metricResponse[index]
                        .value.doubleValue;
        break;
      default:
        continue;
    }

#if defined(BUILD_ADRENO_BACKEND)
    if (metric_id == GpuMetrics::GPU_DDR_BANDWIDTH_METRIC_ID) {
      std::lock_guard<std::mutex> lock(g_gpu_callback_data.mutex);
      g_gpu_callback_data.bandwidth_sum += raw_value;
      g_gpu_callback_data.sample_count++;
      g_gpu_callback_data.has_ever_received_data = true;
      continue;
    }
#endif
#if defined(BUILD_NPU_BACKEND)
    {
      bool is_npu_metric = false;
      switch (metric_id) {
        case NpuMetrics::AXI_128B_READ_REQUEST:
        case NpuMetrics::AXI_128B_WRITE_REQUEST:
        case NpuMetrics::AXI_256B_WRITE_REQUEST:
        case NpuMetrics::AXI_256B_READ_REQUEST:
          is_npu_metric = true;
          break;
        default:
          break;
      }
      if (is_npu_metric) {
        std::lock_guard<std::mutex> lock(g_npu_callback_data.mutex);
        switch (metric_id) {
          case NpuMetrics::AXI_128B_READ_REQUEST:
            g_npu_callback_data.axi_128b_read_sum += raw_value;
            g_npu_callback_data.axi_128b_read_count++;
            break;
          case NpuMetrics::AXI_128B_WRITE_REQUEST:
            g_npu_callback_data.axi_128b_write_sum += raw_value;
            g_npu_callback_data.axi_128b_write_count++;
            break;
          case NpuMetrics::AXI_256B_WRITE_REQUEST:
            g_npu_callback_data.axi_256b_write_sum += raw_value;
            g_npu_callback_data.axi_256b_write_count++;
            break;
          case NpuMetrics::AXI_256B_READ_REQUEST:
            g_npu_callback_data.axi_256b_read_sum += raw_value;
            g_npu_callback_data.axi_256b_read_count++;
            break;
          default:
            break;
        }
        g_npu_callback_data.has_ever_received_data = true;
      }
    }
#endif
  }

  qp_freeProfilingResult(profilingResult);
}

uint64_t get_timestamp_ns() {
  auto now = std::chrono::steady_clock::now();
  auto duration = now.time_since_epoch();
  return std::chrono::duration_cast<std::chrono::nanoseconds>(duration).count();
}

}  // namespace

bool QProfSession::registerUser() {
  std::lock_guard<std::mutex> lock(g_session_mutex);
  if (!g_context) {
    g_context = new ContextRequest();
    eReturnCode ret = qp_initialize(g_context, nullptr);
    if (ret != RETURN_CODE_SUCCESS) {
      fprintf(stderr, "QProf initialization failed with error code: %d\n",
              static_cast<int>(ret));
      delete g_context;
      g_context = nullptr;
      return false;
    }
    qp_setResultCallback(g_context, UnifiedResultCallback);
    qp_setMessageCallback(g_context, UnifiedMessageCallback);
  }
  g_refcount++;
  return true;
}

void QProfSession::unregisterUser() {
  std::lock_guard<std::mutex> lock(g_session_mutex);
  if (g_refcount <= 0) {
    return;
  }
  g_refcount--;
  if (g_refcount == 0 && g_context != nullptr) {
    qp_destroy(g_context);
    delete g_context;
    g_context = nullptr;
  }
}

LpContextRequest QProfSession::context() {
  std::lock_guard<std::mutex> lock(g_session_mutex);
  return g_context;
}

#if defined(BUILD_ADRENO_BACKEND)
void QProfAdrenoResetCallbackState() {
  std::lock_guard<std::mutex> lock(g_gpu_callback_data.mutex);
  g_gpu_callback_data.bandwidth_sum = 0.0;
  g_gpu_callback_data.sample_count = 0;
  g_gpu_callback_data.last_avg_bandwidth = 0.0;
  g_gpu_callback_data.has_ever_received_data = false;
}

bool QProfAdrenoConsumeSample(BandwidthData& data, uint64_t* last_timestamp_ns) {
  double total_bandwidth_mbps = 0.0;

  {
    std::lock_guard<std::mutex> lock(g_gpu_callback_data.mutex);

    if (!g_gpu_callback_data.has_ever_received_data) {
      return false;
    }

    if (g_gpu_callback_data.sample_count > 0) {
      total_bandwidth_mbps = g_gpu_callback_data.bandwidth_sum /
                             static_cast<double>(g_gpu_callback_data.sample_count);

#ifndef NDEBUG
      fprintf(stderr, "[GPU] avg=%.2f MB/s  samples=%u  delta_ms=%.1f\n",
              total_bandwidth_mbps, g_gpu_callback_data.sample_count,
              static_cast<double>(get_timestamp_ns() - *last_timestamp_ns) /
                  1e6);
#endif

      g_gpu_callback_data.bandwidth_sum = 0.0;
      g_gpu_callback_data.sample_count = 0;
      g_gpu_callback_data.last_avg_bandwidth = total_bandwidth_mbps;
    } else {
      total_bandwidth_mbps = g_gpu_callback_data.last_avg_bandwidth;
    }
  }

  uint64_t current_time_ns = get_timestamp_ns();

  data.read_bandwidth_mbps =
      total_bandwidth_mbps / BandwidthDistribution::READ_WRITE_SPLIT_RATIO;
  data.write_bandwidth_mbps =
      total_bandwidth_mbps / BandwidthDistribution::READ_WRITE_SPLIT_RATIO;
  data.total_bandwidth_mbps = total_bandwidth_mbps;
  data.timestamp_ns = current_time_ns;

  *last_timestamp_ns = current_time_ns;
  return true;
}
#endif

#if defined(BUILD_NPU_BACKEND)
void QProfNpuResetCallbackState() {
  std::lock_guard<std::mutex> lock(g_npu_callback_data.mutex);
  g_npu_callback_data.axi_128b_read_sum = 0.0;
  g_npu_callback_data.axi_128b_read_count = 0;
  g_npu_callback_data.axi_128b_write_sum = 0.0;
  g_npu_callback_data.axi_128b_write_count = 0;
  g_npu_callback_data.axi_256b_write_sum = 0.0;
  g_npu_callback_data.axi_256b_write_count = 0;
  g_npu_callback_data.axi_256b_read_sum = 0.0;
  g_npu_callback_data.axi_256b_read_count = 0;
  g_npu_callback_data.last_avg_read_mbps = 0.0;
  g_npu_callback_data.last_avg_write_mbps = 0.0;
  g_npu_callback_data.has_ever_received_data = false;
}

bool QProfNpuConsumeSample(BandwidthData& data, uint64_t* last_timestamp_ns) {
  double read_mbps = 0.0;
  double write_mbps = 0.0;

  {
    std::lock_guard<std::mutex> lock(g_npu_callback_data.mutex);

    if (!g_npu_callback_data.has_ever_received_data) {
      return false;
    }

    bool has_new_data = (g_npu_callback_data.axi_128b_read_count > 0 ||
                         g_npu_callback_data.axi_128b_write_count > 0 ||
                         g_npu_callback_data.axi_256b_read_count > 0 ||
                         g_npu_callback_data.axi_256b_write_count > 0);

    if (has_new_data) {
      double avg_128b_read =
          g_npu_callback_data.axi_128b_read_count > 0
              ? g_npu_callback_data.axi_128b_read_sum /
                    static_cast<double>(g_npu_callback_data.axi_128b_read_count)
              : 0.0;
      double avg_128b_write =
          g_npu_callback_data.axi_128b_write_count > 0
              ? g_npu_callback_data.axi_128b_write_sum /
                    static_cast<double>(g_npu_callback_data.axi_128b_write_count)
              : 0.0;
      double avg_256b_read =
          g_npu_callback_data.axi_256b_read_count > 0
              ? g_npu_callback_data.axi_256b_read_sum /
                    static_cast<double>(g_npu_callback_data.axi_256b_read_count)
              : 0.0;
      double avg_256b_write =
          g_npu_callback_data.axi_256b_write_count > 0
              ? g_npu_callback_data.axi_256b_write_sum /
                    static_cast<double>(g_npu_callback_data.axi_256b_write_count)
              : 0.0;

      read_mbps = avg_128b_read + avg_256b_read;
      write_mbps = avg_128b_write + avg_256b_write;

#ifndef NDEBUG
      fprintf(stderr,
              "[NPU] avg_rd=%.2f  avg_wr=%.2f MB/s  samples=%u  delta_ms=%.1f\n",
              read_mbps, write_mbps, g_npu_callback_data.axi_128b_read_count,
              static_cast<double>(get_timestamp_ns() - *last_timestamp_ns) /
                  1e6);
#endif

      g_npu_callback_data.axi_128b_read_sum = 0.0;
      g_npu_callback_data.axi_128b_read_count = 0;
      g_npu_callback_data.axi_128b_write_sum = 0.0;
      g_npu_callback_data.axi_128b_write_count = 0;
      g_npu_callback_data.axi_256b_read_sum = 0.0;
      g_npu_callback_data.axi_256b_read_count = 0;
      g_npu_callback_data.axi_256b_write_sum = 0.0;
      g_npu_callback_data.axi_256b_write_count = 0;

      g_npu_callback_data.last_avg_read_mbps = read_mbps;
      g_npu_callback_data.last_avg_write_mbps = write_mbps;
    } else {
      read_mbps = g_npu_callback_data.last_avg_read_mbps;
      write_mbps = g_npu_callback_data.last_avg_write_mbps;
    }
  }

  uint64_t current_time_ns = get_timestamp_ns();

  data.read_bandwidth_mbps = read_mbps;
  data.write_bandwidth_mbps = write_mbps;
  data.total_bandwidth_mbps = read_mbps + write_mbps;
  data.timestamp_ns = current_time_ns;

  *last_timestamp_ns = current_time_ns;
  return true;
}
#endif

}  // namespace memory_traffic_profiler
