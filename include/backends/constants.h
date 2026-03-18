#pragma once

namespace memory_traffic_profiler {

// Time conversion constants (shared across backends)
namespace TimeConversion {
// Nanoseconds to seconds conversion factor
constexpr double NANOSECONDS_TO_SECONDS = 1e9;
}  // namespace TimeConversion

// Bandwidth conversion constants (shared across backends)
namespace BandwidthConversion {
// Bytes to MB conversion factor (1024 * 1024)
constexpr double BYTES_TO_MB = 1024.0 * 1024.0;
}  // namespace BandwidthConversion

}  // namespace memory_traffic_profiler
