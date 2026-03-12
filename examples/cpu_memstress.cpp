/*
 * cpu_memstress — CPU memory bandwidth stress test
 *
 * Generates sustained CPU DRAM traffic by streaming through a large array
 * with NEON loads (AArch64) or scalar loads (other platforms).
 * Reports algorithmic bandwidth (bytes touched / elapsed).
 *
 * Use alongside memory_traffic_example to validate the CPU backend
 * (bus_access PMU counter). Expected relationship:
 *   bus_access ≈ 1.9x bw_alg  (HW prefetcher amplification on LPDDR5X)
 *
 * Usage: cpu_memstress [duration_sec] [array_size_mb]
 *   duration_sec  : how long to run (default: 5)
 *   array_size_mb : working set size in MB (default: 256, should be > L2)
 */
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <chrono>

#ifdef __aarch64__
#include <arm_neon.h>
#endif

static volatile uint64_t sink = 0;  // prevent dead-code elimination

// Streaming read: touch every cache line (64B stride)
static void stream_read(const uint8_t* buf, size_t size) {
#ifdef __aarch64__
  uint64x2_t acc = vdupq_n_u64(0);
  for (size_t i = 0; i < size; i += 64) {
    uint64x2_t v = vld1q_u64(reinterpret_cast<const uint64_t*>(buf + i));
    acc = vaddq_u64(acc, v);
  }
  sink += vgetq_lane_u64(acc, 0) + vgetq_lane_u64(acc, 1);
#else
  uint64_t acc = 0;
  for (size_t i = 0; i < size; i += 64) {
    acc += *reinterpret_cast<const uint64_t*>(buf + i);
  }
  sink += acc;
#endif
}

int main(int argc, char* argv[]) {
  int duration_sec = 5;
  int array_mb = 256;

  if (argc > 1) duration_sec = atoi(argv[1]);
  if (argc > 2) array_mb = atoi(argv[2]);
  if (duration_sec <= 0) duration_sec = 5;
  if (array_mb <= 0) array_mb = 256;

  size_t array_bytes = static_cast<size_t>(array_mb) * 1024 * 1024;

  fprintf(stderr, "cpu_memstress: %d MB array, %d sec\n", array_mb, duration_sec);

  uint8_t* buf = static_cast<uint8_t*>(malloc(array_bytes));
  if (!buf) {
    fprintf(stderr, "malloc failed\n");
    return 1;
  }
  // Touch all pages (prefault)
  memset(buf, 0xAB, array_bytes);

  auto t_start = std::chrono::steady_clock::now();
  auto t_end = t_start + std::chrono::seconds(duration_sec);
  uint64_t passes = 0;

  while (std::chrono::steady_clock::now() < t_end) {
    stream_read(buf, array_bytes);
    passes++;
  }

  auto t_done = std::chrono::steady_clock::now();
  double elapsed = std::chrono::duration<double>(t_done - t_start).count();
  double total_bytes = static_cast<double>(passes) * array_bytes;
  double bw_alg_mbs = total_bytes / elapsed / 1e6;

  fprintf(stderr, "cpu_memstress: %lu passes, %.2f sec, bw_alg = %.0f MB/s\n",
          (unsigned long)passes, elapsed, bw_alg_mbs);
  fprintf(stderr, "cpu_memstress: expected bus_access ~ %.0f MB/s (1.9x bw_alg)\n",
          bw_alg_mbs * 1.9);

  free(buf);
  return 0;
}
