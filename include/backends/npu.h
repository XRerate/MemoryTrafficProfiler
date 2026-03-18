#pragma once

#include <memory>

#include "backends/backend.h"

namespace memory_traffic_profiler {

/**
 * @brief NPU backend using QProf API with AXI bandwidth metrics
 *
 * Measures NPU (Qualcomm HTP/Hexagon) memory bandwidth using QProf
 * AXI request counters:
 *   - 4141: AXI 128Byte read request
 *   - 4142: AXI 128Byte write request
 *   - 4143: AXI 256Byte write request
 *   - 4144: AXI 256Byte read request
 */
class NpuBackend : public Backend {
 public:
  NpuBackend();
  ~NpuBackend() override;

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

}  // namespace memory_traffic_profiler
