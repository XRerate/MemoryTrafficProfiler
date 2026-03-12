#pragma once

#include <memory>

#include "backends/backend.h"

namespace MemoryTrafficProfiler {

/**
 * @brief CPU backend using perf_event_open bus_access PMU counter
 *
 * Measures CPU memory bandwidth by counting L2 cache-line transfers
 * via the ARMv8 bus_access PMU event (0x0019). Each event represents
 * a 64-byte cache-line crossing the L2 output port.
 */
class CpuBackend : public Backend {
 public:
  CpuBackend();
  ~CpuBackend() override;

  bool initialize() override;
  bool start() override;
  bool stop() override;
  bool sample(BandwidthData& data) override;
  bool is_profiling() const override;
  const char* get_name() const override;
  BackendCategory get_category() const override;

 private:
  class Impl;
  std::unique_ptr<Impl> pimpl_;
};

}  // namespace MemoryTrafficProfiler
