#include "MemoryFootprintProfiler.h"
#ifdef BUILD_MALI_BACKEND
#include "backends/mali.h"
#endif
#ifdef BUILD_ADRENO_BACKEND
#include "backends/adreno.h"
#endif
#include <chrono>

namespace GPUMemoryFootprintProfiler {

// Constants for memory footprint calculation
namespace {
constexpr double BYTES_PER_MB = 1024.0 * 1024.0;
constexpr uint32_t DEFAULT_SAMPLING_INTERVAL_MS = 10;  // 10ms for accurate accumulation
}  // namespace

MemoryFootprintProfiler::MemoryFootprintProfiler()
    : should_sample_(false),
      is_profiling_(false),
      accumulated_read_bytes_(0),
      accumulated_write_bytes_(0),
      sampling_interval_ms_(DEFAULT_SAMPLING_INTERVAL_MS) {
  // Don't auto-initialize - user must call Initialize()
}

MemoryFootprintProfiler::~MemoryFootprintProfiler() { Stop(); }

bool MemoryFootprintProfiler::Initialize() {
  std::lock_guard<std::mutex> lock(mutex_);

  if (is_profiling_) {
    return false;  // Cannot initialize while profiling
  }

  if (backend_) {
    return true;  // Already initialized
  }

  return autoInitialize();
}

bool MemoryFootprintProfiler::autoInitialize() {
  // Try to initialize with available backends
  // Try Mali first, then Adreno

#ifdef BUILD_MALI_BACKEND
  auto mali_backend = std::make_unique<MaliBackend>();
  if (initialize(std::move(mali_backend))) {
    return true;  // Successfully initialized with Mali
  }
#endif

#ifdef BUILD_ADRENO_BACKEND
  auto adreno_backend = std::make_unique<AdrenoBackend>();
  if (initialize(std::move(adreno_backend))) {
    return true;  // Successfully initialized with Adreno
  }
#endif

  // No backend available - backend_ remains nullptr
  return false;
}

bool MemoryFootprintProfiler::initialize(std::unique_ptr<Backend> backend) {
  if (!backend) {
    return false;
  }

  if (!backend->initialize()) {
    return false;
  }

  backend_ = std::move(backend);
  return true;
}

bool MemoryFootprintProfiler::Start() {
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

  // Start sampling thread to accumulate memory footprint
  sampling_thread_ = std::thread(&MemoryFootprintProfiler::samplingThread, this);

  return true;
}

bool MemoryFootprintProfiler::Stop() {
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

uint64_t MemoryFootprintProfiler::GetReadMemoryFootprint() const {
  return accumulated_read_bytes_.load();
}

uint64_t MemoryFootprintProfiler::GetWriteMemoryFootprint() const {
  return accumulated_write_bytes_.load();
}

uint64_t MemoryFootprintProfiler::GetTotalMemoryFootprint() const {
  return accumulated_read_bytes_.load() + accumulated_write_bytes_.load();
}

bool MemoryFootprintProfiler::IsProfiling() const { return is_profiling_.load(); }

const char* MemoryFootprintProfiler::GetBackendName() const {
  std::lock_guard<std::mutex> lock(mutex_);
  if (backend_) {
    return backend_->get_name();
  }
  return nullptr;
}

void MemoryFootprintProfiler::samplingThread() {
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
            // Accumulate memory footprint: bandwidth (MB/s) * time (s) * bytes/MB
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

}  // namespace GPUMemoryFootprintProfiler
