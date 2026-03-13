#include "backends/cpu.h"

#include <chrono>
#include <cstdio>
#include <cstring>
#include <vector>

#ifdef __linux__
#include <linux/perf_event.h>
#include <sys/ioctl.h>
#include <sys/syscall.h>
#include <unistd.h>
#endif

#include "backends/constants.h"

namespace MemoryTrafficProfiler {

// ARMv8 PMU constants
namespace PmuConfig {
// ARMv8 PMU type (confirmed: /sys/bus/event_source/devices/armv8_pmuv3/type)
constexpr uint32_t ARMV8_PMU_TYPE = 10;
// bus_access event code (0x0019): counts 64-byte cache-line transfers
// crossing the L2 output port (reads + writes combined)
constexpr uint64_t BUS_ACCESS_EVENT = 0x0019;
// Cache line size in bytes (Oryon V2: 64 B)
constexpr uint64_t CACHE_LINE_BYTES = 64;
}  // namespace PmuConfig

class CpuBackend::Impl {
 public:
  Impl() : is_profiling_(false), last_total_count_(0),
           last_timestamp_ns_(0) {}

  ~Impl() {
    if (is_profiling_) {
      stop();
    }
#ifdef __linux__
    for (int fd : perf_fds_) {
      if (fd >= 0) close(fd);
    }
#endif
  }

  bool initialize() {
#ifdef __linux__
    // Read CPU count from /sys/devices/system/cpu/present
    // (same approach as bus_monitor.c in android-bwprobe)
    int num_cpus = getNumCpus();

    // Open one perf_event fd per CPU for system-wide monitoring
    // pid=-1, cpu=N: monitor ALL processes on CPU N (requires root or
    // perf_event_paranoid <= 0)
    // Skip CPUs that fail to open (offline, no PMU, etc.)
    for (int cpu = 0; cpu < num_cpus; cpu++) {
      struct perf_event_attr attr;
      memset(&attr, 0, sizeof(attr));
      attr.type = PmuConfig::ARMV8_PMU_TYPE;
      attr.size = sizeof(attr);
      attr.config = PmuConfig::BUS_ACCESS_EVENT;
      attr.disabled = 1;
      // exclude_kernel=0 is MANDATORY on Oryon V2 (Snapdragon 8 Elite):
      // cache refills are attributed to kernel context, causing 23-33x
      // undercounting with exclude_kernel=1
      attr.exclude_kernel = 0;
      attr.exclude_hv = 1;

      int fd = static_cast<int>(
          syscall(__NR_perf_event_open, &attr, -1, cpu, -1, 0));
      if (fd >= 0) {
        perf_fds_.push_back(fd);
      }
      // Skip CPUs that fail (offline, no PMU, etc.)
    }

    if (perf_fds_.empty()) {
      return false;
    }

    return true;
#else
    return false;
#endif
  }

  bool start() {
#ifdef __linux__
    if (perf_fds_.empty()) {
      return false;
    }

    if (is_profiling_) {
      return true;
    }

    // Reset and enable all per-CPU counters
    for (int fd : perf_fds_) {
      ioctl(fd, PERF_EVENT_IOC_RESET, 0);
      ioctl(fd, PERF_EVENT_IOC_ENABLE, 0);
    }

    last_total_count_ = 0;
    last_timestamp_ns_ = get_timestamp_ns();
    is_profiling_ = true;

    return true;
#else
    return false;
#endif
  }

  bool stop() {
#ifdef __linux__
    if (perf_fds_.empty() || !is_profiling_) {
      return false;
    }

    for (int fd : perf_fds_) {
      ioctl(fd, PERF_EVENT_IOC_DISABLE, 0);
    }
    is_profiling_ = false;
    return true;
#else
    return false;
#endif
  }

  bool sample(BandwidthData& data) {
#ifdef __linux__
    if (perf_fds_.empty() || !is_profiling_) {
      return false;
    }

    // Sum counter values across all CPUs
    uint64_t total_count = 0;
    for (int fd : perf_fds_) {
      uint64_t count = 0;
      ssize_t ret = read(fd, &count, sizeof(count));
      if (ret != sizeof(count)) {
        return false;
      }
      total_count += count;
    }

    // Get current timestamp
    uint64_t current_time_ns = get_timestamp_ns();
    uint64_t delta_time_ns = current_time_ns - last_timestamp_ns_;
    if (delta_time_ns == 0) {
      return false;
    }

    double delta_time_sec = static_cast<double>(delta_time_ns) /
                            TimeConversion::NANOSECONDS_TO_SECONDS;

    // Calculate bandwidth from counter delta
    // bus_access counts cache-line (64B) transfers at L2 output port
    // It counts both reads and writes combined (cannot separate)
    uint64_t delta_count = total_count - last_total_count_;
    double bytes_transferred =
        static_cast<double>(delta_count) * PmuConfig::CACHE_LINE_BYTES;
    double bandwidth_mbps = bytes_transferred / delta_time_sec /
                            BandwidthConversion::BYTES_TO_MB;

#ifndef NDEBUG
    fprintf(stderr, "[CPU] delta_count=%lu  delta_ms=%.1f  bw=%.1f MB/s\n",
            (unsigned long)delta_count, delta_time_sec * 1000.0, bandwidth_mbps);
#endif

    // bus_access is a combined read+write counter, report as total
    data.read_bandwidth_mbps = bandwidth_mbps;
    data.write_bandwidth_mbps = 0.0;
    data.total_bandwidth_mbps = bandwidth_mbps;
    data.timestamp_ns = current_time_ns;

    last_total_count_ = total_count;
    last_timestamp_ns_ = current_time_ns;

    return true;
#else
    return false;
#endif
  }

  bool is_profiling() const { return is_profiling_; }

 private:
  // Read CPU count from /sys/devices/system/cpu/present (e.g. "0-7" → 8)
  static int getNumCpus() {
#ifdef __linux__
    FILE* f = fopen("/sys/devices/system/cpu/present", "r");
    if (!f) return static_cast<int>(sysconf(_SC_NPROCESSORS_ONLN));
    int lo = 0, hi = 0;
    if (fscanf(f, "%d-%d", &lo, &hi) == 2) {
      fclose(f);
      return hi + 1;
    }
    fclose(f);
    return static_cast<int>(sysconf(_SC_NPROCESSORS_ONLN));
#else
    return 0;
#endif
  }

  uint64_t get_timestamp_ns() {
    auto now = std::chrono::steady_clock::now();
    auto duration = now.time_since_epoch();
    return std::chrono::duration_cast<std::chrono::nanoseconds>(duration)
        .count();
  }

  std::vector<int> perf_fds_;  // one fd per CPU core
  bool is_profiling_;
  uint64_t last_total_count_;
  uint64_t last_timestamp_ns_;
};

CpuBackend::CpuBackend() : pimpl_(std::make_unique<Impl>()) {}

CpuBackend::~CpuBackend() = default;

bool CpuBackend::initialize() { return pimpl_->initialize(); }

bool CpuBackend::start() { return pimpl_->start(); }

bool CpuBackend::stop() { return pimpl_->stop(); }

bool CpuBackend::sample(BandwidthData& data) { return pimpl_->sample(data); }

bool CpuBackend::is_profiling() const { return pimpl_->is_profiling(); }

const char* CpuBackend::get_name() const { return "CPU"; }

BackendCategory CpuBackend::get_category() const {
  return BackendCategory::CPU;
}

}  // namespace MemoryTrafficProfiler
