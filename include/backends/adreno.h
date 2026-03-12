#pragma once

#include <memory>

#include "backends/backend.h"

namespace MemoryTrafficProfiler {

/**
 * @brief Adreno GPU backend using QProf API
 */
class AdrenoBackend : public Backend {
 public:
  AdrenoBackend();
  ~AdrenoBackend() override;

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
