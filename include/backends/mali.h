#pragma once

#include <memory>

#include "backends/backend.h"

namespace memory_traffic_profiler {

/**
 * @brief Mali GPU backend using libGPUCounters
 */
class MaliBackend : public Backend {
 public:
  MaliBackend();
  ~MaliBackend() override;

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
