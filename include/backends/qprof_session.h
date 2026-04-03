#pragma once

#include <QProfilerApi.h>

#include "backends/backend.h"

namespace memory_traffic_profiler {

/**
 * Single QProf context shared by all backends that use QualcommProfilerApi
 * (e.g. Adreno + NPU). qp_initialize / qp_destroy are reference-counted.
 */
class QProfSession {
 public:
  static bool registerUser();
  static void unregisterUser();
  static LpContextRequest context();
};

#if defined(BUILD_ADRENO_BACKEND)
bool QProfAdrenoConsumeSample(BandwidthData& data, uint64_t* last_timestamp_ns);
void QProfAdrenoResetCallbackState();
#endif

#if defined(BUILD_NPU_BACKEND)
bool QProfNpuConsumeSample(BandwidthData& data, uint64_t* last_timestamp_ns);
void QProfNpuResetCallbackState();
#endif

}  // namespace memory_traffic_profiler
