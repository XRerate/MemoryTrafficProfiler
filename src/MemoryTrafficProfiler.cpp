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

#include <cassert>

namespace memory_traffic_profiler {

// Constants for memory traffic calculation
namespace {
constexpr double BYTES_PER_MB = 1024.0 * 1024.0;
constexpr uint32_t DEFAULT_SAMPLING_INTERVAL_MS = 10;  // 10ms for accurate accumulation
}  // namespace

MemoryTrafficProfiler::MemoryTrafficProfiler()
    : should_sample_(false),
      is_profiling_(false),
      sampling_interval_ms_(DEFAULT_SAMPLING_INTERVAL_MS) {
  // Don't auto-initialize - user must call Initialize()
}

MemoryTrafficProfiler::~MemoryTrafficProfiler() { Stop(); }

bool MemoryTrafficProfiler::Initialize() {
  return Initialize(std::vector<BackendCategory>{});
}

bool MemoryTrafficProfiler::Initialize(BackendCategory category) {
  return Initialize(std::vector<BackendCategory>{category});
}

bool MemoryTrafficProfiler::Initialize(
    const std::vector<BackendCategory>& categories) {
  std::lock_guard<std::mutex> lock(mutex_);

  if (is_profiling_) {
    return false;
  }

  if (!backends_.empty()) {
    return true;
  }

  if (categories.empty()) {
    return autoInitialize();
  }

  bool any = false;
  for (BackendCategory c : categories) {
    if (hasBackendCategoryUnlocked(c)) {
      continue;
    }
    if (initializeByCategory(c)) {
      any = true;
    }
  }
  return any;
}

bool MemoryTrafficProfiler::autoInitialize() {
  bool any = false;
  bool gpu_ok = false;

#ifdef BUILD_MALI_BACKEND
  if (initialize(std::make_unique<MaliBackend>())) {
    any = true;
    gpu_ok = true;
  }
#endif
#ifdef BUILD_ADRENO_BACKEND
  if (!gpu_ok && initialize(std::make_unique<AdrenoBackend>())) {
    any = true;
  }
#endif

#ifdef BUILD_CPU_BACKEND
  if (initialize(std::make_unique<CpuBackend>())) {
    any = true;
  }
#endif

#ifdef BUILD_NPU_BACKEND
  if (initialize(std::make_unique<NpuBackend>())) {
    any = true;
  }
#endif

  return any;
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

bool MemoryTrafficProfiler::HasBackendCategory(BackendCategory category) const {
  std::lock_guard<std::mutex> lock(mutex_);
  return hasBackendCategoryUnlocked(category);
}

bool MemoryTrafficProfiler::hasBackendCategoryUnlocked(
    BackendCategory category) const {
  for (const auto& b : backends_) {
    if (b && b->get_category() == category) {
      return true;
    }
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

  backends_.push_back(std::move(backend));
  per_backend_read_bytes_.push_back(0);
  per_backend_write_bytes_.push_back(0);
  return true;
}

bool MemoryTrafficProfiler::Start() {
  std::lock_guard<std::mutex> lock(mutex_);

  if (backends_.empty()) {
    return false;
  }

  if (is_profiling_) {
    return true;
  }

  if (sampling_thread_.joinable()) {
    sampling_thread_.join();
  }

  std::vector<Backend*> started;
  for (auto& b : backends_) {
    if (!b->start()) {
      for (Backend* s : started) {
        s->stop();
      }
      return false;
    }
    started.push_back(b.get());
  }

  for (size_t i = 0; i < per_backend_read_bytes_.size(); ++i) {
    per_backend_read_bytes_[i] = 0;
    per_backend_write_bytes_[i] = 0;
  }

  should_sample_ = true;
  is_profiling_ = true;

  sampling_thread_ = std::thread(&MemoryTrafficProfiler::samplingThread, this);

  return true;
}

bool MemoryTrafficProfiler::Stop() {
  {
    std::lock_guard<std::mutex> lock(mutex_);

    if (!is_profiling_) {
      return true;
    }

    should_sample_ = false;
    is_profiling_ = false;
  }

  if (sampling_thread_.joinable()) {
    sampling_thread_.join();
  }

  for (auto& b : backends_) {
    if (b) {
      b->stop();
    }
  }

  return true;
}

uint64_t MemoryTrafficProfiler::GetReadMemoryTraffic(
    BackendCategory category) const {
  std::lock_guard<std::mutex> lock(mutex_);
  uint64_t sum = 0;
  for (size_t i = 0; i < backends_.size(); ++i) {
    if (backends_[i] && backends_[i]->get_category() == category) {
      sum += per_backend_read_bytes_[i];
    }
  }
  return sum;
}

uint64_t MemoryTrafficProfiler::GetWriteMemoryTraffic(
    BackendCategory category) const {
  std::lock_guard<std::mutex> lock(mutex_);
  uint64_t sum = 0;
  for (size_t i = 0; i < backends_.size(); ++i) {
    if (backends_[i] && backends_[i]->get_category() == category) {
      sum += per_backend_write_bytes_[i];
    }
  }
  return sum;
}

uint64_t MemoryTrafficProfiler::GetTotalMemoryTraffic(
    BackendCategory category) const {
  std::lock_guard<std::mutex> lock(mutex_);
  uint64_t sum = 0;
  for (size_t i = 0; i < backends_.size(); ++i) {
    if (backends_[i] && backends_[i]->get_category() == category) {
      sum += per_backend_read_bytes_[i];
      sum += per_backend_write_bytes_[i];
    }
  }
  return sum;
}

BackendCategory MemoryTrafficProfiler::GetBackendCategory(size_t index) const {
  std::lock_guard<std::mutex> lock(mutex_);
  assert(index < backends_.size() && backends_[index]);
  return backends_[index]->get_category();
}

bool MemoryTrafficProfiler::IsProfiling() const { return is_profiling_.load(); }

size_t MemoryTrafficProfiler::GetBackendCount() const {
  std::lock_guard<std::mutex> lock(mutex_);
  return backends_.size();
}

const char* MemoryTrafficProfiler::GetBackendName(size_t index) const {
  std::lock_guard<std::mutex> lock(mutex_);
  if (index >= backends_.size() || !backends_[index]) {
    return nullptr;
  }
  return backends_[index]->get_name();
}

void MemoryTrafficProfiler::samplingThread() {
  BandwidthData data;
  uint64_t last_timestamp_ns = 0;

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

      auto now = std::chrono::steady_clock::now();
      auto duration = now.time_since_epoch();
      uint64_t current_timestamp_ns =
          std::chrono::duration_cast<std::chrono::nanoseconds>(duration).count();

      uint64_t delta_ns = current_timestamp_ns - last_timestamp_ns;
      double delta_seconds = static_cast<double>(delta_ns) / 1e9;

      bool progressed = false;
      for (size_t i = 0; i < backends_.size(); ++i) {
        Backend* backend = backends_[i].get();
        if (!backend || !backend->is_profiling()) {
          continue;
        }
        if (backend->sample(data) && delta_seconds > 0.0) {
          uint64_t read_bytes_delta = static_cast<uint64_t>(
              data.read_bandwidth_mbps * delta_seconds * BYTES_PER_MB);
          uint64_t write_bytes_delta = static_cast<uint64_t>(
              data.write_bandwidth_mbps * delta_seconds * BYTES_PER_MB);

          per_backend_read_bytes_[i] += read_bytes_delta;
          per_backend_write_bytes_[i] += write_bytes_delta;
          progressed = true;
        }
      }

      if (progressed) {
        last_timestamp_ns = current_timestamp_ns;
      }
    }

    std::this_thread::sleep_for(
        std::chrono::milliseconds(sampling_interval_ms_));
  }
}

}  // namespace memory_traffic_profiler
