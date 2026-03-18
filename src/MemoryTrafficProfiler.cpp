#include "MemoryTrafficProfiler.h"
#ifdef BUILD_MALI_BACKEND
#include "backends/mali.h"
#endif
#ifdef BUILD_ADRENO_BACKEND
#include "backends/adreno.h"
#endif
#ifdef BUILD_CPU_BACKEND
#include "backends/cpu.h"
#endif
#ifdef BUILD_NPU_BACKEND
#include "backends/npu.h"
#endif
#include <chrono>

namespace memory_traffic_profiler {

// Constants for memory traffic calculation
namespace {
constexpr double BYTES_PER_MB = 1024.0 * 1024.0;
constexpr uint32_t DEFAULT_SAMPLING_INTERVAL_MS = 10;  // 10ms for accurate accumulation
}  // namespace

MemoryTrafficProfiler::MemoryTrafficProfiler()
    : should_sample_(false),
      is_profiling_(false),
      accumulated_read_bytes_(0),
      accumulated_write_bytes_(0),
      sampling_interval_ms_(DEFAULT_SAMPLING_INTERVAL_MS) {
  // Don't auto-initialize - user must call Initialize()
}

MemoryTrafficProfiler::~MemoryTrafficProfiler() { Stop(); }

bool MemoryTrafficProfiler::Initialize() {
  std::lock_guard<std::mutex> lock(mutex_);

  if (is_profiling_) {
    return false;  // Cannot initialize while profiling
  }

  if (backend_) {
    return true;  // Already initialized
  }

  return autoInitialize();
}

bool MemoryTrafficProfiler::Initialize(BackendCategory category) {
  std::lock_guard<std::mutex> lock(mutex_);

  if (is_profiling_) {
    return false;  // Cannot initialize while profiling
  }

  if (backend_) {
    return true;  // Already initialized
  }

  return initializeByCategory(category);
}

bool MemoryTrafficProfiler::autoInitialize() {
  // Try to initialize with available backends
  // Try Mali first, then Adreno, then CPU, then NPU

#ifdef BUILD_MALI_BACKEND
  auto mali_backend = std::make_unique<MaliBackend>();
  if (initialize(std::move(mali_backend))) {
    return true;
  }
#endif

#ifdef BUILD_ADRENO_BACKEND
  auto adreno_backend = std::make_unique<AdrenoBackend>();
  if (initialize(std::move(adreno_backend))) {
    return true;
  }
#endif

#ifdef BUILD_CPU_BACKEND
  auto cpu_backend = std::make_unique<CpuBackend>();
  if (initialize(std::move(cpu_backend))) {
    return true;
  }
#endif

#ifdef BUILD_NPU_BACKEND
  auto npu_backend = std::make_unique<NpuBackend>();
  if (initialize(std::move(npu_backend))) {
    return true;
  }
#endif

  return false;
}

bool MemoryTrafficProfiler::initializeByCategory(BackendCategory category) {
  switch (category) {
    case BackendCategory::GPU:
#ifdef BUILD_MALI_BACKEND
      if (initialize(std::make_unique<MaliBackend>())) return true;
#endif
#ifdef BUILD_ADRENO_BACKEND
      if (initialize(std::make_unique<AdrenoBackend>())) return true;
#endif
      break;
    case BackendCategory::CPU:
#ifdef BUILD_CPU_BACKEND
      if (initialize(std::make_unique<CpuBackend>())) return true;
#endif
      break;
    case BackendCategory::NPU:
#ifdef BUILD_NPU_BACKEND
      if (initialize(std::make_unique<NpuBackend>())) return true;
#endif
      break;
  }
  return false;
}

bool MemoryTrafficProfiler::initialize(std::unique_ptr<Backend> backend) {
  if (!backend) {
    return false;
  }

  if (!backend->initialize()) {
    return false;
  }

  backend_ = std::move(backend);
  return true;
}

bool MemoryTrafficProfiler::Start() {
  std::lock_guard<std::mutex> lock(mutex_);

  if (!backend_) {
    return false;  // Not initialized
  }

  if (is_profiling_) {
    return true;  // Already started
  }

  // Make sure any previous thread is joined before starting a new one
  if (sampling_thread_.joinable()) {
    sampling_thread_.join();
  }

  if (!backend_->start()) {
    return false;
  }

  // Reset accumulated bytes
  accumulated_read_bytes_ = 0;
  accumulated_write_bytes_ = 0;

  should_sample_ = true;
  is_profiling_ = true;

  // Start sampling thread to accumulate memory traffic
  sampling_thread_ = std::thread(&MemoryTrafficProfiler::samplingThread, this);

  return true;
}

bool MemoryTrafficProfiler::Stop() {
  {
    std::lock_guard<std::mutex> lock(mutex_);

    if (!is_profiling_) {
      return true;  // Already stopped
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

uint64_t MemoryTrafficProfiler::GetReadMemoryTraffic() const {
  return accumulated_read_bytes_.load();
}

uint64_t MemoryTrafficProfiler::GetWriteMemoryTraffic() const {
  return accumulated_write_bytes_.load();
}

uint64_t MemoryTrafficProfiler::GetTotalMemoryTraffic() const {
  return accumulated_read_bytes_.load() + accumulated_write_bytes_.load();
}

bool MemoryTrafficProfiler::IsProfiling() const { return is_profiling_.load(); }

const char* MemoryTrafficProfiler::GetBackendName() const {
  std::lock_guard<std::mutex> lock(mutex_);
  if (backend_) {
    return backend_->get_name();
  }
  return nullptr;
}

void MemoryTrafficProfiler::samplingThread() {
  BandwidthData data;
  uint64_t last_timestamp_ns = 0;

  // Get initial timestamp
  {
    auto now = std::chrono::steady_clock::now();
    auto duration = now.time_since_epoch();
    last_timestamp_ns = std::chrono::duration_cast<std::chrono::nanoseconds>(
                            duration)
                            .count();
  }

  while (should_sample_.load()) {
    {
      std::lock_guard<std::mutex> lock(mutex_);

      if (backend_ && backend_->is_profiling()) {
        if (backend_->sample(data)) {
          // Get current timestamp
          auto now = std::chrono::steady_clock::now();
          auto duration = now.time_since_epoch();
          uint64_t current_timestamp_ns =
              std::chrono::duration_cast<std::chrono::nanoseconds>(duration)
                  .count();

          // Calculate time delta in seconds
          uint64_t delta_ns = current_timestamp_ns - last_timestamp_ns;
          double delta_seconds = static_cast<double>(delta_ns) / 1e9;

          if (delta_seconds > 0.0) {
            // Accumulate memory traffic: bandwidth (MB/s) * time (s) * bytes/MB
            // = bytes
            uint64_t read_bytes_delta = static_cast<uint64_t>(
                data.read_bandwidth_mbps * delta_seconds * BYTES_PER_MB);
            uint64_t write_bytes_delta = static_cast<uint64_t>(
                data.write_bandwidth_mbps * delta_seconds * BYTES_PER_MB);

            accumulated_read_bytes_ += read_bytes_delta;
            accumulated_write_bytes_ += write_bytes_delta;
          }

          last_timestamp_ns = current_timestamp_ns;
        }
      }
    }

    // Sleep for the sampling interval
    std::this_thread::sleep_for(
        std::chrono::milliseconds(sampling_interval_ms_));
  }
}

}  // namespace memory_traffic_profiler
